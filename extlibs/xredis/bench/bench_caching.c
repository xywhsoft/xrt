#include "../tests/support/client_peer.h"
#include <xredis/tracking.h>
#include <inttypes.h>
static bool benchCachingPeer(redispeer* P,xredisreply* R,ptr Data)
{
    bool* Armed=Data;xredisreplyvalue Root,Name;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Name);
    if(redisPeerName(Name,"HELLO")){
        static const char H[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,H,sizeof(H)-1);return true;
    }
    if(redisPeerName(Name,"CLIENT")){redisPeerRequire(!*Armed,"benchmark adjacent prefix");*Armed=true;redisPeerSend(P,"+OK\r\n",5);}
    else {redisPeerRequire(*Armed,"benchmark adjacent query");*Armed=false;redisPeerSend(P,"$5\r\nvalue\r\n",11);}
    return true;
}
static int benchCachingCompare(const void* A,const void* B)
{double X=*(const double*)A,Y=*(const double*)B;return X<Y?-1:X>Y;}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):4096,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;
    bool Group=Argc>3 && strtoul(Argv[3],NULL,10)!=0,Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>64 || Argc>5)return 1;
    redispeer P;redisPeerStart(&P);bool Armed=false;redispeerhook Hook={benchCachingPeer,&Armed};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=NULL;redisPeerRequire(xredisClientConnectInit(P.Engine,&P.Address,0,NULL,NULL,&C),"benchmark managed client");
    xfuture* Ready=xredisClientReady(C);redisPeerRequire(xrtFutureWaitFor(Ready,5000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"benchmark setup");xrtFutureDestroy(Ready);
    redisPeerRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"benchmark observation");
    double* Latency=malloc(Count*sizeof(*Latency));redisPeerRequire(Latency!=NULL,"benchmark caller timings");double Offered[64];xfuture *Control[64],*Query[64];
    xbytesview Prefix[]={XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("CACHING"),XRT_BYTES_LITERAL("YES")};
    xbytesview Args[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("binary\0key")};
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    uint64 Sum=0;double Start=xrtTimer();
    for(size_t N=0;N<Count;){size_t Width=Count-N<Depth?Count-N:Depth;
        /* Manual comparison has one serialized producer, so the two separate
         * async submissions are adjacent here; it does not wait for prefix ACK.
         * It needs producer synchronization in a shared-connection application. */
        for(size_t I=0;I<Width;++I){Offered[I]=xrtTimer();
            if(Group)redisPeerRequire(xredisClientCachingCallAsync(C,true,Args,2,5000,&Query[I])==XREDIS_ADMISSION_ACCEPTED,"group benchmark admission");
            else {redisPeerRequire(xredisClientTrackingCallAsync(C,Prefix,3,5000,&Control[I])==XREDIS_ADMISSION_ACCEPTED && xredisClientTrackingCallAsync(C,Args,2,5000,&Query[I])==XREDIS_ADMISSION_ACCEPTED,"manual benchmark submissions");}
        }
        for(size_t I=0;I<Width;++I){const xredistrackingresult *A,*B;
            redisPeerRequire(xrtFutureWaitFor(Query[I],5000)==XWAIT_OK,"benchmark query result");
            if(Group){const xrediscachingresult* R=xrtFutureValue(Query[I]);redisPeerRequire(R && R->ControlReceived,"benchmark prefix known");A=&R->Control;B=&R->Query;}
            else {redisPeerRequire(xrtFutureWaitFor(Control[I],5000)==XWAIT_OK,"benchmark control result");A=xrtFutureValue(Control[I]);B=xrtFutureValue(Query[I]);}
            redisPeerRequire(A && B && A->Request.Status==XREDIS_REQUEST_OK && B->Request.Status==XREDIS_REQUEST_OK && A->Sequence==2*(N+I)+1 && B->Sequence==A->Sequence+1,"same ordered successful workload");
            xredisreplyvalue V;xredisReplyGet(A->Request.Reply,0,&V);Sum+=V.Data.Size;xredisReplyGet(B->Request.Reply,0,&V);Sum+=V.Data.Size;
            Latency[N+I]=(xrtTimer()-Offered[I])*1000000;xrtFutureDestroy(Query[I]);if(!Group)xrtFutureDestroy(Control[I]);
        }
        N+=Width;
    }
    double Seconds=xrtTimer()-Start;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    redisPeerRequire(Sum==Count*7,"benchmark full control/query checksum");
    xfuture* Closed=xredisClientClosed(C);redisPeerRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark physical close");xredisClientRelease(C);xrtFutureDestroy(Closed);redisPeerStop(&P);
    qsort(Latency,Count,sizeof(*Latency),benchCachingCompare);
    printf("groups_per_sec: %.3f\nlatency_p50_us: %.3f\nlatency_p99_us: %.3f\nlatency_max_us: %.3f\nchecksum: %" PRIu64 "\n",Count/Seconds,Latency[Count/2],Latency[(Count-1)*99/100],Latency[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_group: %.6f\nheap_bytes_per_group: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    free(Latency);return 0;
}
