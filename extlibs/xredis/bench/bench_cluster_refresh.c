#include <xredis.h>
#include "../tests/support/cluster_refresh_peer.h"
#include <inttypes.h>
static int fbCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return(X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):200000;unsigned Mode=Argc>2?(unsigned)strtoul(Argv[2],NULL,10):1;bool Heap=Argc>3 && strtoul(Argv[3],NULL,10)!=0;
    if(!Count || Count>10000000 || Mode>1 || Argc>4)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"refresh benchmark samples");rffixture* F=rfStart(3);rfCreate(F,1,NULL);xfuture* Initial=rfBegin(F,false);redisPeerRequire(rfDrive(F,Initial)->Status==XREDIS_REFRESH_OK,"refresh benchmark bootstrap");xredisclustermap* Warm=NULL;bool Stale=true;
    redisPeerRequire(xredisClusterRefreshSnapshot(F->Refresh,&Warm,&Stale)==XREDIS_CLUSTER_VALUE && !Stale,"refresh benchmark warm snapshot");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Heap);xrtMemStatsGet(&Before);
#else
    if(Heap)return 2;
#endif
    double Began=xrtTimer();uint64 Sum=0;
    for(size_t N=0;N<Count;++N){double T=xrtTimer();xredisclustermap* M=NULL;
        if(Mode)redisPeerRequire(xredisClusterRefreshSnapshot(F->Refresh,&M,&Stale)==XREDIS_CLUSTER_VALUE && !Stale,"refresh benchmark snapshot");
        else{xredisClusterMapRetain(Warm);M=Warm;}
        redisPeerRequire(M==Warm,"same immutable view");xredisClusterMapRelease(M);Samples[N]=(xrtTimer()-T)*1e9;++Sum;}
    double Seconds=xrtTimer()-Began;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Samples,Count,sizeof(*Samples),fbCompare);printf("snapshots_per_sec: %.6f\nlatency_p50_ns: %.6f\nlatency_p99_ns: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*99+99)/100-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Heap)printf("heap_blocks_per_snapshot: %.6f\nheap_bytes_per_snapshot: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    xredisClusterMapRelease(Warm);rfStop(F);xrtFutureDestroy(Initial);free(Samples);return 0;
}
