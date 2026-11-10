#include "../tests/support/client_peer.h"
#include <xredis/tracking.h>
#include <inttypes.h>
static bool benchTrackingPeer(redispeer* P,xredisreply* R,ptr User)
{
    (void)R;(void)User;static const char Wire[]=">2\r\n+invalidate\r\n*1\r\n$3\r\nkey\r\n+value\r\n";redisPeerSend(P,Wire,sizeof(Wire)-1);return true;
}
static int benchTrackingCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y;}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):4096,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;
    bool Observe=Argc>3 && strtoul(Argv[3],NULL,10)!=0,Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>128 || Argc>5)return 1;
#if defined(XREDIS_TRACKING_BENCH_LEGACY)
    if(Observe)return 1;
#endif
    redispeer P;redisPeerStart(&P);redispeerhook Hook={benchTrackingPeer,NULL};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);if(Depth>K.MaxInFlight)K.MaxInFlight=Depth;
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnect(P.Engine,&P.Address,0,&K,&C),"benchmark Client");
    xfuture* Ready=xredisClientReady(C);redisPeerRequire(Ready && xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"benchmark transport readiness");xrtFutureDestroy(Ready);
#if !defined(XREDIS_TRACKING_BENCH_LEGACY)
    if(Observe)redisPeerRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"benchmark observations");
#endif
    double* Latency=malloc(Count*sizeof(*Latency));redisPeerRequire(Latency!=NULL,"benchmark caller timings");xfuture* Futures[128];double Offered[128];
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    xbytesview A=XRT_BYTES_LITERAL("READ");uint64 Sum=0;double Start=xrtTimer();
    for(size_t N=0;N<Count;){size_t Width=Count-N<Depth?Count-N:Depth;
        for(size_t I=0;I<Width;++I){Offered[I]=xrtTimer();xredisadmission Admission;
#if !defined(XREDIS_TRACKING_BENCH_LEGACY)
            if(Observe)Admission=xredisClientTrackingCallAsync(C,&A,1,5000,Futures+I);else
#endif
            Admission=xredisClientCallAsync(C,&A,1,5000,Futures+I);
            redisPeerRequire(Admission==XREDIS_ADMISSION_ACCEPTED,"benchmark admission");
        }
        for(size_t I=0;I<Width;++I){redisPeerRequire(xrtFutureWaitFor(Futures[I],5000)==XWAIT_OK,"benchmark reply deadline");
            const xredisrequestresult* R=xrtFutureValue(Futures[I]);redisPeerRequire(R && R->Status==XREDIS_REQUEST_OK,"benchmark successful reply");
            xredisreply* Push=NULL;
#if !defined(XREDIS_TRACKING_BENCH_LEGACY)
            if(Observe){xredistrackingpush Pushed;redisPeerRequire(xredisClientTrackingTake(C,&Pushed) && Pushed.Sequence==2*(N+I)+1 && ((const xredistrackingresult*)R)->Sequence==2*(N+I)+2,"benchmark exact receive ordering");Push=Pushed.Reply;}else
#endif
            redisPeerRequire(xredisClientTakePush(C,&Push),"benchmark queued push");
            xredistrackingevent Event;redisPeerRequire(xredisTrackingEventRead(Push,&Event)==XREDIS_TRACKING_EVENT && Event.Keys==1,"benchmark typed invalidation");
            xredisreplyinfo Info={0};Info.Size=sizeof(Info);xredisreplyvalue Value;xredisReplyInfo(R->Reply,&Info);xredisReplyGet(R->Reply,Info.Root,&Value);Sum+=Value.Data.Size+Event.Keys;
            Latency[N+I]=(xrtTimer()-Offered[I])*1000000;xredisReplyRelease(Push);xrtFutureDestroy(Futures[I]);
        }N+=Width;
    }
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count*6,"benchmark checksum");size_t Bytes=0;
#if !defined(XREDIS_TRACKING_BENCH_LEGACY)
    xredistrackinginfo O={0};O.Size=sizeof(O);redisPeerRequire(xredisClientTrackingInfo(C,&O) && (!Observe || O.Sequence==Count*2),"benchmark observer accounting");Bytes=O.Bytes;
#endif
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark physical close");xredisClientRelease(C);xrtFutureDestroy(Closed);redisPeerStop(&P);
    qsort(Latency,Count,sizeof(*Latency),benchTrackingCompare);
    printf("queries_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nobservation_bytes: %zu\nchecksum: %" PRIu64 "\n",(double)Count/Seconds,Latency[Count/2],Latency[(Count-1)*99/100],Latency[Count-1],Bytes,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_query: %.6f\nheap_bytes_per_query: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latency);return 0;
}
