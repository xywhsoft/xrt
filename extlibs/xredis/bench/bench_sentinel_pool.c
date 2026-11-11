#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "../tests/support/sentinel_pool_peer.h"
#include <inttypes.h>
static int spBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
/* Match the data connection affinity in every variant. Seed lookup remains on
 * its own existing control path; steady requests all use native worker1. */
static xredissentinelstatus spBenchNative(xnetengine* E,const xnetaddr* A,int64 Timeout,spfixture* F,xredisclient** C)
{
    xredisclientinitconfig I;xredisClientInitConfigInit(&I);I.Timeout=Timeout;
    bool OK=xredisClientConnectInit(E,A,1,NULL,&I,C);
    if(OK){redisPeerRequire(F->ClosedCount<512,"benchmark native observers");F->Closed[F->ClosedCount++]=xredisClientClosed(*C);}
    return OK?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
}
static bool spBenchConnect(xnetengine* E,size_t Slot,uint64 Attempt,int64 Timeout,ptr Data,xredisclient** C)
{(void)Slot;(void)Attempt;spfixture* F=Data;return spBenchNative(E,&F->A.Base.Address,Timeout,F,C)==XREDIS_SENTINEL_OK;}
#if !defined(SP_BENCH_BASELINE)
static xredissentinelstatus spBenchMaster(xnetengine* E,const xredissentinelendpoint* A,int64 Timeout,ptr Data,xredisclient** C)
{spfixture* F=Data;xnetaddr Address;redisPeerRequire(A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9) && xrtNetAddrParse(&Address,"127.0.0.1",A->Port),"benchmark announced endpoint");return spBenchNative(E,&Address,Timeout,F,C);}
#endif
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):10000,Depth=Argc>2?(size_t)strtoul(Argv[2],NULL,10):32;unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):1;bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>32 || Mode>1 || Argc>5)return 2;
#if defined(SP_BENCH_BASELINE)
    if(Mode)return 2;
#endif
    spfixture F;spStart(&F,0,false,false);xredispoolconfig O;xredisPoolConfigInit(&O);O.LeaseTimeout=60000;xredispool* P=NULL;
#if !defined(SP_BENCH_BASELINE)
    if(Mode){xredissentinelpoolconfig K;spConfig(&F,&K);K.Discovery.ConnectMaster=spBenchMaster;K.Notify=NULL;P=spCreate(&F,1,&O,&K);}else
#endif
    {xredispoolconnectconfig K;xredisPoolConnectConfigInit(&K);K.Connect=spBenchConnect;K.Drop=spDrop;K.Data=&F;redisPeerRequire(xredisPoolConnect(F.Seed.Base.Engine,0,1,&O,&K,&P)==XREDIS_ADMISSION_ACCEPTED,"ordinary benchmark pool");xrtAtomicPtrStore(&F.Pool,P,XMEMORY_RELEASE);}
    xfuture* A=spAcquire(P,5000);xredislease* L=spLease(A);spPing(L);double* Latency=malloc(Count*sizeof(*Latency));redisPeerRequire(Latency!=NULL,"caller latency storage");
    for(;;){xredispoolinfo I={0};I.Size=sizeof(I);xredisPoolInfo(P,&I);if(!I.Requests)break;xrtSleep(1);}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("ha\0echo")};uint64 Sum=0;double Start=xrtTimer();
    for(size_t Done=0;Done<Count;){size_t Batch=Count-Done;if(Batch>Depth)Batch=Depth;xfuture* Calls[32];double Entered[32];
        for(size_t N=0;N<Batch;++N){Entered[N]=xrtTimer();redisPeerRequire(xredisLeaseCallAsync(L,Args,2,5000,&Calls[N])==XREDIS_ADMISSION_ACCEPTED,"steady native admission");}
        for(size_t N=0;N<Batch;++N){const xredisrequestresult* R=spWait(Calls[N]);Latency[Done+N]=(xrtTimer()-Entered[N])*1000000;redisPeerRequire(R->Status==XREDIS_REQUEST_OK,"steady native success");xredisreplyvalue V;xredisReplyGet(R->Reply,0,&V);redisPeerRequire(V.Data.Size==Args[1].Size && !memcmp(V.Data.Data,Args[1].Data,V.Data.Size),"steady semantic binary value");Sum+=V.Data.Size;xrtFutureDestroy(Calls[N]);}Done+=Batch;}
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count*7,"steady checksum");qsort(Latency,Count,sizeof(*Latency),spBenchCompare);
    printf("queries_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Latency[(Count-1)/2],Latency[(Count-1)*99/100],Latency[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_query: %.6f\nheap_bytes_per_query: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latency);spReturn(L,false);xfuture* Closed=xredisPoolClosed(P);redisPeerRequire(xredisPoolClose(P) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark physical pool close");xrtFutureDestroy(Closed);xredisPoolRelease(P);xrtFutureDestroy(A);spStop(&F);return 0;
}
