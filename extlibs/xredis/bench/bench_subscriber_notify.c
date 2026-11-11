#include <xredis/subscriber_notify.h>
#include "../tests/support/client_peer.h"
#include <inttypes.h>
#include <math.h>
typedef struct receivestream {redispeer* Peer;uint64 First;size_t Count;xatomic32 Done;} receivestream;
static bool receivePeer(redispeer* P,xredisreply* R,ptr Data)
{
    (void)R;(void)Data;static const char Ack[]="*3\r\n+subscribe\r\n$1\r\nc\r\n:1\r\n";redisPeerSend(P,Ack,sizeof(Ack)-1);return true;
}
static void receiveBurst(xnetworker* W,ptr Data)
{
    (void)W;receivestream* B=Data;static const char H[]="*3\r\n+message\r\n$1\r\nc\r\n$16\r\n";
    for(size_t N=0;N<B->Count;++N){unsigned char Payload[16];uint64 Seq=B->First+N;double Start=xrtTimer();
        _Static_assert(sizeof(Start)==8,"benchmark timestamp representation");memcpy(Payload,&Seq,8);memcpy(Payload+8,&Start,8);
        redisPeerSend(B->Peer,H,sizeof(H)-1);redisPeerSend(B->Peer,Payload,sizeof(Payload));redisPeerSend(B->Peer,"\r\n",2);}
    xrtAtomic32Store(&B->Done,1,XMEMORY_RELEASE);
}
static int receiveCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):8192,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):32,Batch=Argc>3?(size_t)strtoull(Argv[3],NULL,10):32;
    bool Wait=Argc>4 && strtoul(Argv[4],NULL,10)!=0,Instrument=Argc>5 && strtoul(Argv[5],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>256 || !Batch || Batch>256 || Argc>6)return 2;
    double* Latencies=malloc(Count*sizeof(*Latencies));redisPeerRequire(Latencies!=NULL,"latency storage");
    redispeer P;redisPeerStart(&P);redispeerhook Hook={receivePeer,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.MaxPushes=Depth;xredisclient* C=NULL;redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,&K,&C),"message stream connect");
    xfuture* F=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"message stream ready");xrtFutureDestroy(F);redisPeerRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"message stream adoption");
    xbytesview Channel=XRT_BYTES_LITERAL("c");F=NULL;redisPeerRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Channel,1,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"untimed subscription");
    xfutureresult V;redisPeerRequire(xrtFutureResult(F,&V) && V.Value && ((xredissubscriberresult*)V.Value)->Request.Status==XREDIS_REQUEST_OK,"untimed ACK validation");xrtFutureDestroy(F);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;size_t Done=0;double Begin=xrtTimer();
    while(Done<Count){
        size_t Group=Count-Done<Depth?Count-Done:Depth;receivestream B={&P,Done,Group,{0}};xrtAtomic32Init(&B.Done,0);
        redisPeerRequire(xrtNetEnginePost(P.Engine,1,receiveBurst,&B),"bounded stream burst");size_t Target=Done+Group;
        while(Done<Target){xredisreply* Replies[256];size_t Taken=0;
            if(!xredisSubscriberTakeBatch(C,Replies,Batch,&Taken)){
                if(Wait){xfuture* Hint=NULL;redisPeerRequire(xredisSubscriberReadable(C,&Hint)==XREDIS_ADMISSION_ACCEPTED,"message stream hint");redisPeerRequire(xrtFutureWaitFor(Hint,10000)==XWAIT_OK,"message stream wait");xrtFutureDestroy(Hint);}
                else {xrtThreadYield();}
                continue;
            }
            for(size_t N=0;N<Taken;++N){xredispubsubevent E;uint64 Seq=0;double Start=0,Now=xrtTimer();
                redisPeerRequire(xredisPubSubEventRead(Replies[N],&E)==XREDIS_PUBSUB_EVENT && E.Kind==XREDIS_PUBSUB_MESSAGE && E.Channel.Size==1 && *(const char*)E.Channel.Data=='c' && E.Payload.Size==16,"full binary message semantics");
                memcpy(&Seq,E.Payload.Data,8);memcpy(&Start,E.Payload.Data+8,8);redisPeerRequire(Seq==Done && isfinite(Start) && Start<=Now && Now-Start<10,"exact FIFO and finite enqueue timestamp");
                Sum+=Seq+1;Latencies[Done++]=Now-Start;xredisReplyRelease(Replies[N]);
            }
        }
        redisPeerWait(&B.Done,1);
    }
    double Seconds=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==(uint64)Count*(Count+1)/2,"message stream checksum");F=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"message stream close");xrtFutureDestroy(F);xredisClientRelease(C);redisPeerStop(&P);
    qsort(Latencies,Count,sizeof(*Latencies),receiveCompare);
    printf("operations_per_sec: %.3f\np50_us: %.3f\np95_us: %.3f\np99_us: %.3f\np999_us: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Latencies[(Count-1)/2]*1e6,Latencies[(Count-1)*95/100]*1e6,Latencies[(Count-1)*99/100]*1e6,Latencies[(Count-1)*999/1000]*1e6,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latencies);return 0;
}
