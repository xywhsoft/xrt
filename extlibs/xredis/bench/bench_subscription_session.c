#include <xredis/subscription_session.h>
#include "../tests/support/subscription_session_peer.h"
#include <inttypes.h>
static xredissubscriptionsessioncurrent sessionBenchWait(xredissubscriptionsession* S,uint64 Generation,uint64 Revision)
{
    double End=xrtTimer()+10;for(;;){xredissubscriptionsessioncurrent C;xredisadmission A=xredisSubscriptionSessionCurrent(S,&C);if(A==XREDIS_ADMISSION_ACCEPTED){if(C.Generation>=Generation && C.Revision>=Revision)return C;xredisClientRelease(C.Client);}else redisPeerRequire(A==XREDIS_ADMISSION_OVERLOADED,"bench pending");redisPeerRequire(xrtTimer()<End,"bench completion deadline");xrtThreadYield();}
}
static void sessionBenchAbort(xnetworker* W,ptr Data)
{(void)W;sessionpeer* P=Data;uint32 N=xrtAtomic32Load(&P->Accepted,XMEMORY_ACQUIRE);xrtNetStreamAbort(P->Peers[(N-1)%16].Stream);}
static int sessionBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):128,Names=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;bool Recovery=Argc>3 && atoi(Argv[3])!=0,Instrument=Argc>4 && atoi(Argv[4])!=0;
    if(!Count || Count>10000 || !Names || Names>32 || Argc>5)return 2;
    double* Samples=malloc(Count*sizeof(*Samples));redisPeerRequire(Samples!=NULL,"bench timings");sessionpeer P;sessionPeerStart(&P);sessionfactory F;xredissubscriptionsessionconfig K;sessionFactoryConfig(&F,&P,&K);K.RetryMin=K.RetryMax=1;
    xredissubscriptions* Store=NULL;redisPeerRequire(xredisSubscriptionsCreate(NULL,&Store),"bench store");unsigned char Data[32][16];xbytesview Views[32];for(size_t N=0;N<Names;++N){int L=snprintf((char*)Data[N],16,"n%zu",N);redisPeerRequire(L>0 && L<16,"bench name");Views[N]=(xbytesview){Data[N],(size_t)L};}
    uint64 Revision=0;redisPeerRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,Views,Names,&Revision)==XREDIS_ADMISSION_ACCEPTED,"bench seed");xredissubscriptionsession* S=NULL;redisPeerRequire(xredisSubscriptionSessionCreate(P.Engine,0,Store,&K,&S)==XREDIS_ADMISSION_ACCEPTED,"bench autonomous session");xredissubscriptionsessioncurrent C=sessionBenchWait(S,1,Revision);xredisClientRelease(C.Client);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Begin=xrtTimer();
    for(size_t N=0;N<Count;++N){double Start=xrtTimer();if(Recovery)redisPeerRequire(xrtNetEnginePost(P.Engine,1,sessionBenchAbort,&P),"bench actual EOF");else{xbytesview Name=XRT_BYTES_LITERAL("extra");redisPeerRequire(xredisSubscriptionSessionApply(S,(N&1u)?XREDIS_PUBSUB_CMD_UNSUBSCRIBE:XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Revision)==XREDIS_ADMISSION_ACCEPTED,"bench live edit");}
        C=sessionBenchWait(S,Recovery?N+2:1,Revision);Sum+=Recovery?C.Generation:C.Revision;xredisClientRelease(C.Client);Samples[N]=xrtTimer()-Start;}
    double Seconds=xrtTimer()-Begin;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==(uint64)Count*(Count+3)/2,"generation/revision checksum");xredissubscriptionsessioninfo I={0};I.Size=sizeof(I);redisPeerRequire(xredisSubscriptionSessionInfo(S,&I) && I.Generations==(Recovery?Count+1:1),"same-generation live edits versus actual replacements");xfuture* Closed=xredisSubscriptionSessionClosed(S);redisPeerRequire(xredisSubscriptionSessionClose(S) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"bench close");xredisSubscriptionSessionRelease(S);xredisSubscriptionsRelease(Store);sessionPeerStop(&P);xrtFutureDestroy(Closed);
    qsort(Samples,Count,sizeof(*Samples),sessionBenchCompare);printf("operations_per_sec: %.3f\np50_us: %.6f\np95_us: %.6f\np99_us: %.6f\np999_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count-1)/2]*1e6,Samples[(Count-1)*95/100]*1e6,Samples[(Count-1)*99/100]*1e6,Samples[(Count-1)*999/1000]*1e6,Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_operation: %.6f\nheap_bytes_per_operation: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Samples);return 0;
}
