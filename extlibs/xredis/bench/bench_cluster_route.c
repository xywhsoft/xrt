#include <xredis.h>
#include "../tests/support/cluster_route_peer.h"
#include <inttypes.h>
typedef struct routebenchpool {xredispool* Pool;uint16 Port;} routebenchpool;
#if !defined(CR_BENCH_BASELINE)
static xredisclusterpoolstatus routeBenchPool(const xredisclusteraddress* A,int64 Timeout,ptr Data,xredispool** Out)
{routebenchpool* P=Data;redisPeerRequire(Timeout>0 && !A->Tls && A->Port==P->Port,"benchmark exact route endpoint");xredisPoolRetain(P->Pool);*Out=P->Pool;return XREDIS_CLUSTER_POOL_READY;}
#endif
static int routeBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):20000,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):0;bool Heap=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>64 || Mode>1 || Argc>5)return 2;
#if defined(CR_BENCH_BASELINE)
    if(Mode)return 2;
#endif
    double* Samples=malloc(Count*sizeof(double));double* Starts=malloc(Depth*sizeof(double));xfuture** Calls=malloc(Depth*sizeof(xfuture*));xredisclusterroute** Routes=calloc(Depth,sizeof(*Routes));redispeer* Peer=xrtCalloc(1,sizeof(*Peer));
    redisPeerRequire(Samples && Starts && Calls && Routes && Peer,"route benchmark caller storage");redisPeerStart(Peer);redispeerhook Hook={redisPoolPeerCommand,NULL};xrtAtomicPtrStore(&Peer->Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* Client=NULL;redisPeerRequire(xredisClientConnectInit(Peer->Engine,&Peer->Address,0,NULL,NULL,&Client),"route benchmark client");xfuture* Ready=xredisClientReady(Client);
    redisPeerRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"route benchmark ready");xrtFutureDestroy(Ready);
    xredispool* Pool=NULL;redisPeerRequire(xredisSharedPoolCreate(Peer->Engine,0,&Client,1,NULL,&Pool)==XREDIS_ADMISSION_ACCEPTED,"route benchmark shared pool");
#if !defined(CR_BENCH_BASELINE)
    routebenchpool Context={Pool,Peer->Address.Port};xredisclusterrouteconfig C;xredisClusterRouteConfigInit(&C);C.GetPool=routeBenchPool;C.Data=&Context;C.MaxRedirects=C.MaxWaits=0;
#endif
    xredisclustertopology* T=crTopology(Peer->Address.Port,"127.0.0.1",0,16383);xbytesview A[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("bytes\0x")};xfuture* Warm=NULL;
    redisPeerRequire(xredisSharedPoolCall(Pool,A,2,5000,&Warm)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(Warm))->Status==XREDIS_REQUEST_OK,"route benchmark native warm");xrtFutureDestroy(Warm);
#if !defined(CR_BENCH_BASELINE)
    if(Mode){xredisclusterroute* R=NULL;redisPeerRequire(xredisClusterRouteCreate(T,(xbytesview){0},1,A,2,&C,&R)==XREDIS_ADMISSION_ACCEPTED && crDrive(R).Query.Status==XREDIS_REQUEST_OK,"route benchmark controller warm");xredisClusterRouteDestroy(R);}
#endif
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 2;
#endif
    uint64 Sum=0;double Began=xrtTimer();
    for(size_t Base=0;Base<Count;Base+=Depth){size_t N=Count-Base;if(N>Depth)N=Depth;
        for(size_t I=0;I<N;++I){Starts[I]=xrtTimer();Calls[I]=NULL;
#if !defined(CR_BENCH_BASELINE)
            if(Mode){redisPeerRequire(xredisClusterRouteCreate(T,(xbytesview){0},1,A,2,&C,&Routes[I])==XREDIS_ADMISSION_ACCEPTED,"route benchmark constructor");redisPeerRequire(xredisClusterRouteStep(Routes[I])!=XREDIS_CLUSTER_ROUTE_STEP_ERROR,"route benchmark first step");}else
#endif
            redisPeerRequire(xredisSharedPoolCallAsync(Pool,A,2,5000,&Calls[I])==XREDIS_ADMISSION_ACCEPTED,"route benchmark direct admission");}
        for(size_t I=0;I<N;++I){
#if !defined(CR_BENCH_BASELINE)
            if(Mode){xredisclusterrouteresult R=crDrive(Routes[I]);redisPeerRequire(R.Status==XREDIS_CLUSTER_ROUTE_COMPLETE && R.Query.Status==XREDIS_REQUEST_OK && R.Attempts==1,"route benchmark one native attempt");Calls[I]=xredisClusterRouteResultFuture(Routes[I]);}else
#endif
            redisPeerRequire(xrtFutureWaitFor(Calls[I],10000)==XWAIT_OK,"route benchmark native wait");
            Samples[Base+I]=(xrtTimer()-Starts[I])*1e6;const xredisrequestresult* R=xrtFutureValue(Calls[I]);xredisreplyvalue V;
            redisPeerRequire(R && R->Status==XREDIS_REQUEST_OK && xredisReplyGet(R->Reply,0,&V) && V.Data.Size==7 && !memcmp(V.Data.Data,"bytes\0x",7),"route benchmark same binary result");Sum+=V.Data.Size;
#if !defined(CR_BENCH_BASELINE)
            if(Mode){xredisClusterRouteDestroy(Routes[I]);Routes[I]=NULL;}
#endif
            xrtFutureDestroy(Calls[I]);}}
    double Seconds=xrtTimer()-Began;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count*7,"route benchmark checksum");qsort(Samples,Count,sizeof(*Samples),routeBenchCompare);
    printf("queries_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p99_us: %.6f\nlatency_max_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*99+99)/100-1],Samples[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_query: %.6f\nheap_bytes_per_query: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xredisClusterTopologyRelease(T);xfuture* Closed=xredisPoolClosed(Pool);xredisPoolClose(Pool);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"route benchmark physical close");xrtFutureDestroy(Closed);xredisPoolRelease(Pool);xredisClientRelease(Client);redisPeerStop(Peer);xrtFree(Peer);free(Routes);free(Calls);free(Starts);free(Samples);return 0;
}
