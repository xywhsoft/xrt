#include <xredis.h>
#include "../tests/support/cluster_nodes_peer.h"
#include <inttypes.h>
static bool nbAuthorize(const xredisclusteraddress* A,ptr Data)
{nnfixture* F=Data;return !A->Tls && A->Host.Size==9 && !memcmp(A->Host.Data,"127.0.0.1",9) && A->Port==F->Peers[0].Address.Port;}
static int nbCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
typedef struct nbwork {
    nnfixture* Fixture;xredispool* Pool;xredisclusteraddress Address;double* Samples;
    size_t Begin,End;unsigned Mode;xatomic32 *Go,*Ready;uint64 Sum;double Finished;
} nbwork;
static int32 nbRun(ptr Data)
{
    nbwork* W=Data;xredispool* Warm=NULL;redisPeerRequire(xredisClusterNodesGetPool(&W->Address,5000,W->Fixture->Cache,&Warm)==XREDIS_CLUSTER_POOL_READY && Warm==W->Pool,"per-thread warm initialization");xredisPoolRelease(Warm);
    xrtAtomic32FetchAdd(W->Ready,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(W->Go,XMEMORY_ACQUIRE))xrtSleep(1);
    for(size_t N=W->Begin;N<W->End;++N){double T=xrtTimer();xredispool* P=NULL;
        if(W->Mode)redisPeerRequire(xredisClusterNodesGetPool(&W->Address,5000,W->Fixture->Cache,&P)==XREDIS_CLUSTER_POOL_READY,"nodes benchmark hit");
        else{redisPeerRequire(nbAuthorize(&W->Address,W->Fixture),"same authority baseline");xredisPoolRetain(W->Pool);P=W->Pool;}
        redisPeerRequire(P==W->Pool,"same cached node");xredisPoolRelease(P);W->Samples[N]=(xrtTimer()-T)*1e9;++W->Sum;}
    W->Finished=xrtTimer();xredisClusterNodesRelease(W->Fixture->Cache);return 0;
}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):200000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10)!=0;unsigned Threads=Argc>4?(unsigned)strtoul(Argv[4],NULL,10):1;
    if(!Count || Count>10000000 || Mode>1 || !Threads || Threads>8 || Count<Threads || Argc>5)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"nodes benchmark caller storage");nnfixture* F=nnStart();xredisclusternodesconfig C;nnConfig(F,&C);C.Authorize=nbAuthorize;nnCreate(F,&C);xredisclusteraddress A=nnAddress(F,0);xredispool* Warm=NULL;
    redisPeerRequire(xredisClusterNodesGetPool(&A,5000,F->Cache,&Warm)==XREDIS_CLUSTER_POOL_READY,"nodes benchmark warm pool");nnReady(Warm,1);
    nbwork Work[8];xthread* Handles[8];xatomic32 Go={0},Ready={0};
    for(unsigned I=0;I<Threads;++I){Work[I]=(nbwork){F,Warm,A,Samples,Count*I/Threads,Count*(I+1)/Threads,Mode,&Go,&Ready,0,0};xredisClusterNodesRetain(F->Cache);Handles[I]=xrtThreadCreate(nbRun,&Work[I],0);redisPeerRequire(Handles[I]!=NULL,"nodes benchmark worker");}
    redisPeerWait(&Ready,Threads);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 2;
#endif
    double Began=xrtTimer();xrtAtomic32Store(&Go,1,XMEMORY_RELEASE);uint64 Sum=0;double End=Began;
    for(unsigned I=0;I<Threads;++I){redisPeerRequire(xrtThreadWaitFor(Handles[I],120000)==XWAIT_OK && !xrtThreadExitCode(Handles[I]),"nodes benchmark join");if(Work[I].Finished>End)End=Work[I].Finished;Sum+=Work[I].Sum;xrtThreadDestroy(Handles[I]);}
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count,"nodes benchmark checksum");qsort(Samples,Count,sizeof(*Samples),nbCompare);printf("lookups_per_sec: %.6f\nlatency_p50_ns: %.6f\nlatency_p99_ns: %.6f\nchecksum: %" PRIu64 "\n",Count/(End-Began),Samples[(Count*50+99)/100-1],Samples[(Count*99+99)/100-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_lookup: %.6f\nheap_bytes_per_lookup: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xredisPoolRelease(Warm);nnClose(F);xredisClusterNodesRelease(F->Cache);nnStop(F);free(Samples);return 0;
}
