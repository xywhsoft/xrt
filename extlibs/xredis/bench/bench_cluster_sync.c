#include <xredis.h>
#include "../tests/support/cluster_command_peer.h"
#include <inttypes.h>
static int swbCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):10000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10);unsigned Affinity=Argc>4?(unsigned)strtoul(Argv[4],NULL,10):1;
    if(!Count || Count>1000000 || Mode>3 || Affinity>1 || Argc>5)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"sync benchmark samples");rffixture* F=qpStart();xredisclusterclientconfig K;xredisClusterClientConfigInit(&K);xredisclusteraddress Seed=nnAddress(F->Base,0);Seed.Slot=0;xredisclusterclient* C=NULL;
    redisPeerRequire(xredisClusterClientCreate(F->Base->Peers[0].Engine,Affinity,F->Base->Cache,&Seed,1,&K,&C)==XREDIS_ADMISSION_ACCEPTED,"sync benchmark affinity");cfReady(C);xrediscommandcatalog* Catalogue=qpCatalogue(1,NULL);xredisclusteraddress Address=nnAddress(F->Base,0);xredispool* Pool=NULL;
    redisPeerRequire(xredisClusterNodesGetPool(&Address,5000,F->Base->Cache,&Pool)==XREDIS_CLUSTER_POOL_READY,"sync benchmark pool");nnReady(Pool,1);const xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};uint64 Sum=0;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 2;
#endif
    double Began=xrtTimer();for(size_t N=0;N<Count;++N){double Start=xrtTimer();xredisclusterwait W={XWAIT_ERROR,NULL};
        switch(Mode){
        case 0:redisPeerRequire(xredisClusterClientCall(C,1,A,2,10000,&W.Future)==XREDIS_ADMISSION_ACCEPTED,"explicit async admission");W.Wait=xrtFutureWait(W.Future);break;
        case 1:redisPeerRequire(xredisClusterClientCallSync(C,1,A,2,10000,&W)==XREDIS_ADMISSION_ACCEPTED,"explicit sync admission");break;
        case 2:redisPeerRequire(xredisClusterClientCallCommand(C,Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&W.Future)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"metadata async admission");W.Wait=xrtFutureWait(W.Future);break;
        default:redisPeerRequire(xredisClusterClientCallCommandSync(C,Catalogue,A,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,10000,&W)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"metadata sync admission");break;}
        redisPeerRequire(W.Wait==XWAIT_OK,"sync benchmark wait");const xredisclusterclientresult* R=xrtFutureValue(W.Future);xredisreplyvalue V;redisPeerRequire(R && R->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && R->RouteObserved && R->Route.Query.Status==XREDIS_REQUEST_OK && xredisReplyGet(R->Route.Query.Reply,0,&V) && V.Data.Size==13 && !memcmp(V.Data.Data,"owned\0{22179}",13),"sync benchmark result");Samples[N]=(xrtTimer()-Start)*1e6;++Sum;xrtFutureDestroy(W.Future);}
    double Seconds=xrtTimer()-Began;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Samples,Count,sizeof(*Samples),swbCompare);printf("requests_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p95_us: %.6f\nlatency_p99_us: %.6f\nlatency_p999_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*95+99)/100-1],Samples[(Count*99+99)/100-1],Samples[(Count*999+999)/1000-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_request: %.6f\nheap_bytes_per_request: %.6f\nheap_backing_allocs_per_request: %.6f\nheap_direct_blocks_per_request: %.6f\nheap_pooled_blocks_per_request: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count,(double)(After.BackingAllocCalls-Before.BackingAllocCalls)/Count,(double)(After.DirectAllocCalls-Before.DirectAllocCalls)/Count,(double)(After.PooledAllocCalls-Before.PooledAllocCalls)/Count);
#endif
    xredisPoolRelease(Pool);xredisCommandCatalogRelease(Catalogue);cfStop(F,C);free(Samples);return 0;
}
