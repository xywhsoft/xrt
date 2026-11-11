#include <xredis.h>
#include "../tests/support/pool_peer.h"
#include <inttypes.h>
typedef struct scbenchfactory {redispeer* Peer; xatomic32 Dropped;} scbenchfactory;
#if !defined(SC_BENCH_BASELINE)
static bool scBenchConnect(xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** Out)
{
    (void)Slot;(void)Attempt;scbenchfactory* F=Data;xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Timeout=Timeout;
    return xredisClientConnectInit(E,&F->Peer->Address,0,NULL,&I,Out);
}
static void scBenchDrop(ptr Data){xrtAtomic32FetchAdd(&((scbenchfactory*)Data)->Dropped,1,XMEMORY_RELEASE);}
#endif
static int sharedBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):20000,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):0;bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>64 || Mode>1 || Argc>5)return 2;
#if defined(SC_BENCH_BASELINE)
    if(Mode)return 2;
#endif
    double* Samples=malloc(Count*sizeof(double));double* Starts=malloc(Depth*sizeof(double));xfuture** Calls=malloc(Depth*sizeof(xfuture*));redispeer* Peer=xrtCalloc(1,sizeof(*Peer));redisPeerRequire(Samples && Starts && Calls && Peer,"benchmark caller fixtures");
    redisPeerStart(Peer);redispeerhook Hook={redisPoolPeerCommand,NULL};xrtAtomicPtrStore(&Peer->Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* Client=NULL;xredispool* Pool=NULL;scbenchfactory Factory={Peer,{0}};
#if !defined(SC_BENCH_BASELINE)
    if(Mode){xredispoolconnectconfig C;xredisPoolConnectConfigInit(&C);C.Connect=scBenchConnect;C.Drop=scBenchDrop;C.Data=&Factory;
        redisPeerRequire(xredisSharedPoolConnect(Peer->Engine,0,1,NULL,&C,&Pool)==XREDIS_ADMISSION_ACCEPTED,"benchmark maintained shared pool");
        double Due=xrtTimer()+15;for(;;){xredispoolinfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisPoolInfo(Pool,&I) && xrtTimer()<Due,"benchmark shared ready");if(I.Idle==1)break;xrtThreadYield();}}
    else
#endif
    {redisPeerRequire(xredisClientConnectInit(Peer->Engine,&Peer->Address,0,NULL,NULL,&Client),"benchmark numeric managed client");xfuture* Ready=xredisClientReady(Client);redisPeerRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"benchmark ready");xrtFutureDestroy(Ready);
        redisPeerRequire(xredisSharedPoolCreate(Peer->Engine,0,&Client,1,NULL,&Pool)==XREDIS_ADMISSION_ACCEPTED,"benchmark fixed shared pool");}
    xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("bytes\0x")};uint64 Sum=0;
    /* Warm the same mode outside all measured intervals. */
    xfuture* Warm=NULL;xredisadmission A=xredisSharedPoolCall(Pool,Args,2,5000,&Warm);
    redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(Warm))->Status==XREDIS_REQUEST_OK,"benchmark warm native call");xrtFutureDestroy(Warm);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    double Started=xrtTimer();
    for(size_t Base=0;Base<Count;Base+=Depth){size_t N=Count-Base;if(N>Depth)N=Depth;
        for(size_t I=0;I<N;++I){Starts[I]=xrtTimer();Calls[I]=NULL;
            A=xredisSharedPoolCallAsync(Pool,Args,2,5000,&Calls[I]);
            redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED,"benchmark direct native admission");}
        for(size_t I=0;I<N;++I){redisPeerRequire(xrtFutureWaitFor(Calls[I],10000)==XWAIT_OK,"benchmark call deadline");Samples[Base+I]=(xrtTimer()-Starts[I])*1e6;const xredisrequestresult* R=xrtFutureValue(Calls[I]);xredisreplyvalue V;
            redisPeerRequire(R && R->Status==XREDIS_REQUEST_OK && xredisReplyGet(R->Reply,0,&V) && V.Data.Size==7 && !memcmp(V.Data.Data,"bytes\0x",7),"benchmark same owned binary reply");

            Sum+=V.Data.Size;xrtFutureDestroy(Calls[I]);}
    }
    double Seconds=xrtTimer()-Started;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Samples,Count,sizeof(*Samples),sharedBenchCompare);redisPeerRequire(Sum==Count*7,"benchmark checksum");
    printf("queries_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p99_us: %.6f\nlatency_max_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*99+99)/100-1],Samples[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_query: %.6f\nheap_bytes_per_query: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xfuture* Closed=xredisPoolClosed(Pool);xredisPoolClose(Pool);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark pool close");redisPeerRequire(!Mode || xrtAtomic32Load(&Factory.Dropped,XMEMORY_ACQUIRE)==1,"benchmark maintenance context joined");xredisPoolRelease(Pool);xrtFutureDestroy(Closed);xredisClientRelease(Client);redisPeerStop(Peer);xrtFree(Peer);free(Calls);free(Starts);free(Samples);return 0;
}
