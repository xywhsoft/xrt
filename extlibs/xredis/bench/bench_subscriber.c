#include <xredis/subscriber.h>
#include "../tests/support/client_peer.h"
#include <inttypes.h>
static xatomic32 SubscriberBenchEmit;
static bool subscriberBenchPeer(redispeer* P,xredisreply* R,ptr Data)
{
    (void)Data;(void)R;static const char Ack[]="*3\r\n+subscribe\r\n$1\r\nc\r\n:1\r\n";
    static const char Message[]="*3\r\n+message\r\n$1\r\nc\r\n$4\r\nv\0\r\n\r\n";
    redisPeerSend(P,Ack,sizeof(Ack)-1);if(xrtAtomic32Load(&SubscriberBenchEmit,XMEMORY_ACQUIRE))redisPeerSend(P,Message,sizeof(Message)-1);return true;
}
static int subscriberBenchCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):4096,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):32;
    bool Instrument=Argc>3 && strtoul(Argv[3],NULL,10)!=0;if(!Count || Count>1000000 || !Depth || Depth>256 || Argc>4)return 2;
    double* Latencies=malloc(Count*sizeof(*Latencies));redisPeerRequire(Latencies!=NULL,"latency storage");
    redispeer P;redisPeerStart(&P);redispeerhook Hook={subscriberBenchPeer,NULL};xrtAtomic32Init(&SubscriberBenchEmit,0);xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig Config;xredisClientConfigInit(&Config);Config.MaxPushes=Depth;
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,&Config,&C),"benchmark connect");xfuture* Ready=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK,"ready");xrtFutureDestroy(Ready);
    redisPeerRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"benchmark adoption");xbytesview Name=XRT_BYTES_LITERAL("c");xfuture* Prime=NULL;
    redisPeerRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,5000,&Prime)==XREDIS_ADMISSION_ACCEPTED,"warm persistent name");xrtFutureDestroy(Prime);xrtAtomic32Store(&SubscriberBenchEmit,1,XMEMORY_RELEASE);
    xfuture* Futures[256]={0};double Starts[256]={0};size_t Sent=0,Done=0;uint64 Sum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    double Begin=xrtTimer();
    while(Done<Count){
        while(Sent<Count && Sent-Done<Depth){size_t Slot=Sent%Depth;Starts[Slot]=xrtTimer();redisPeerRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,10000,&Futures[Slot])==XREDIS_ADMISSION_ACCEPTED,"pipeline admission");++Sent;}
        size_t Slot=Done%Depth;redisPeerRequire(xrtFutureWaitFor(Futures[Slot],10000)==XWAIT_OK,"control completion");xfutureresult V;xrtFutureResult(Futures[Slot],&V);const xredissubscriberresult* Result=V.Value;
        redisPeerRequire(Result && Result->Request.Status==XREDIS_REQUEST_OK && Result->Acknowledged==1,"ACK validation");xrtFutureDestroy(Futures[Slot]);Futures[Slot]=NULL;
        xredisreply* Message=NULL;double End=xrtTimer()+10;while(!xredisClientTakePush(C,&Message)){redisPeerRequire(xrtTimer()<End,"message completion");xrtThreadYield();}
        xredispubsubevent E;redisPeerRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && E.Kind==XREDIS_PUBSUB_MESSAGE && E.Channel.Size==1 && E.Payload.Size==4 && !memcmp(E.Payload.Data,"v\0\r\n",4),"full binary delivery");
        Sum+=E.Channel.Size+E.Payload.Size;xredisReplyRelease(Message);Latencies[Done++]=xrtTimer()-Starts[Slot];
    }
    double Seconds=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count*5,"benchmark checksum");xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark close");xrtFutureDestroy(Closed);xredisClientRelease(C);redisPeerStop(&P);
    qsort(Latencies,Count,sizeof(*Latencies),subscriberBenchCompare);
    printf("operations_per_sec: %.3f\np50_us: %.3f\np99_us: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Latencies[(Count-1)/2]*1000000,Latencies[((Count-1)*99)/100]*1000000,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latencies);return 0;
}
