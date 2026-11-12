#include <xredis.h>
#include "../tests/support/cluster_command_peer.h"
#include <inttypes.h>
static int cbCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
static xredisclusterclient* cbCreate(rffixture* F,unsigned Affinity)
{
    xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);K.RefreshMin=1;K.RefreshRetryMax=5;K.Refresh.RetryDelay=1;K.Route.RetryDelay=1;
    xredisclusteraddress A=nnAddress(F->Base,0);A.Slot=0;xredisclusterclient* C=NULL;
    redisPeerRequire(xredisClusterClientCreate(F->Base->Peers[0].Engine,Affinity,F->Base->Cache,&A,1,&K,&C)==XREDIS_ADMISSION_ACCEPTED,"benchmark client affinity");return C;
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):20000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10)!=0;size_t Pipeline=Argc>4?(size_t)strtoull(Argv[4],NULL,10):32;
    unsigned Affinity=Argc>5?(unsigned)strtoul(Argv[5],NULL,10):1;
    if(!Count || Count>1000000 || Mode>2 || !Pipeline || Pipeline>32 || Affinity>1 || Argc>6)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"managed benchmark samples");rffixture* F=qpStart();xredisclusterclient* C=cbCreate(F,Affinity);cfReady(C);xrediscommandcatalog* Catalogue=qpCatalogue(1,NULL);
    xredisclusteraddress Address=nnAddress(F->Base,0);xredispool* Pool=NULL;redisPeerRequire(xredisClusterNodesGetPool(&Address,5000,F->Base->Cache,&Pool)==XREDIS_CLUSTER_POOL_READY,"managed benchmark shared baseline");nnReady(Pool,1);
    xfuture* Futures[32]={0};double Submitted[32]={0};const xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};uint64 Sum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 2;
#endif
    double Began=xrtTimer();
    for(size_t N=0;N<Count+Pipeline;++N){size_t I=N%Pipeline;
        if(N>=Pipeline && Futures[I]){redisPeerRequire(xrtFutureWaitFor(Futures[I],15000)==XWAIT_OK,"managed benchmark bounded wait");const xredisrequestresult* Q;
            if(Mode<=2){const xredisclusterclientresult* V=xrtFutureValue(Futures[I]);redisPeerRequire(V && V->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && V->RouteObserved && V->Route.Status==XREDIS_CLUSTER_ROUTE_COMPLETE,"managed benchmark typed result");Q=&V->Route.Query;}
            else Q=xrtFutureValue(Futures[I]);
            redisPeerRequire(Q && Q->Status==XREDIS_REQUEST_OK,"benchmark native status");xredisreplyvalue V;redisPeerRequire(xredisReplyGet(Q->Reply,0,&V) && V.Data.Size==13 && !memcmp(V.Data.Data,"owned\0{22179}",13),"benchmark binary result");Samples[N-Pipeline]=(xrtTimer()-Submitted[I])*1e6;++Sum;xrtFutureDestroy(Futures[I]);Futures[I]=NULL;}
        if(N<Count){Submitted[I]=xrtTimer();
            if(Mode==2)redisPeerRequire(xredisClusterClientCallCommand(C,Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&Futures[I])==XREDIS_CLUSTER_COMMAND_ACCEPTED,"integrated command benchmark admission");
            else {uint16 Slot=1;if(Mode==1){xrediscommandkeyresult Keys={0};Keys.Size=sizeof(Keys);redisPeerRequire(xredisCommandCatalogEvaluate(Catalogue,A,2,NULL,0,&Keys)==XREDIS_KEYS_OK,"manual metadata benchmark");Slot=Keys.Slot;}
                redisPeerRequire(xredisClusterClientCall(C,Slot,A,2,10000,&Futures[I])==XREDIS_ADMISSION_ACCEPTED,"explicit command benchmark admission");}}
    }
    double Seconds=xrtTimer()-Began;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Samples,Count,sizeof(*Samples),cbCompare);printf("requests_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p95_us: %.6f\nlatency_p99_us: %.6f\nlatency_p999_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*95+99)/100-1],Samples[(Count*99+99)/100-1],Samples[(Count*999+999)/1000-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_request: %.6f\nheap_bytes_per_request: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
    if(Heap)printf("heap_backing_allocs_per_request: %.6f\nheap_direct_blocks_per_request: %.6f\nheap_pooled_blocks_per_request: %.6f\n",(double)(After.BackingAllocCalls-Before.BackingAllocCalls)/Count,(double)(After.DirectAllocCalls-Before.DirectAllocCalls)/Count,(double)(After.PooledAllocCalls-Before.PooledAllocCalls)/Count);
#endif
    xredisPoolRelease(Pool);xredisCommandCatalogRelease(Catalogue);cfStop(F,C);free(Samples);return 0;
}
