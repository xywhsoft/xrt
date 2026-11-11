#include <xredis.h>
#include "../tests/support/client_peer.h"
#include <inttypes.h>
typedef struct askingbenchpeer {bool Asking,Armed;} askingbenchpeer;
static bool askingBenchCommand(redispeer* P,xredisreply* R,ptr Data)
{
    askingbenchpeer* F=Data;xredisreplyvalue Root,Name;xredisReplyGet(R,0,&Root);xredisReplyGet(R,Root.FirstChild,&Name);
    if(redisPeerName(Name,"HELLO")){static const char H[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,H,sizeof(H)-1);return true;}
    if(redisPeerName(Name,"ASKING")){redisPeerRequire(F->Asking && !F->Armed,"benchmark prefix adjacency");F->Armed=true;redisPeerSend(P,"+OK\r\n",5);return true;}
    redisPeerRequire(redisPeerName(Name,"ECHO") && F->Armed==F->Asking,"benchmark exact ECHO/ASKING wire grouping");F->Armed=false;return false;
}
static int askingBenchCompare(const void* A,const void* B){double X=*(const double*)A,Y=*(const double*)B;return (X>Y)-(X<Y);}
int main(int Argc,char** Argv)
{
    size_t Count=Argc>1?(size_t)strtoull(Argv[1],NULL,10):20000,Depth=Argc>2?(size_t)strtoull(Argv[2],NULL,10):1;unsigned Mode=Argc>3?(unsigned)strtoul(Argv[3],NULL,10):0;bool Instrument=Argc>4 && strtoul(Argv[4],NULL,10)!=0;
    if(!Count || Count>1000000 || !Depth || Depth>64 || Mode>2 || Argc>5)return 2;
#if defined(SA_BENCH_BASELINE)
    if(Mode)return 2;
#endif
    double* Samples=malloc(Count*sizeof(double));double* Starts=malloc(Depth*sizeof(double));xfuture** Calls=malloc(Depth*sizeof(xfuture*));redispeer* Peer=xrtCalloc(1,sizeof(*Peer));redisPeerRequire(Samples && Starts && Calls && Peer,"benchmark caller fixtures");
    redisPeerStart(Peer);askingbenchpeer State={Mode!=0,false};redispeerhook Hook={askingBenchCommand,&State};xrtAtomicPtrStore(&Peer->Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* Client=NULL;redisPeerRequire(xredisClientConnectInit(Peer->Engine,&Peer->Address,0,NULL,NULL,&Client),"benchmark numeric managed client");xfuture* Ready=xredisClientReady(Client);redisPeerRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK && ((const xredisreadyresult*)xrtFutureValue(Ready))->Status==XREDIS_READY_OK,"benchmark ready");xrtFutureDestroy(Ready);
    xredispoolconfig C;xredisPoolConfigInit(&C);C.LeaseTimeout=86400000;xredispool* Pool=NULL;xfuture* Acquired=NULL;xredislease* Lease=NULL;
    if(Mode==2){redisPeerRequire(xredisPoolCreate(Peer->Engine,0,&Client,1,&C,&Pool)==XREDIS_ADMISSION_ACCEPTED,"benchmark exclusive pool");redisPeerRequire(xredisPoolAcquire(Pool,5000,&Acquired)==XREDIS_ADMISSION_ACCEPTED,"benchmark lease");Lease=((const xredisleaseresult*)xrtFutureValue(Acquired))->Lease;redisPeerRequire(Lease!=NULL,"benchmark grant");}
    else redisPeerRequire(xredisSharedPoolCreate(Peer->Engine,0,&Client,1,&C,&Pool)==XREDIS_ADMISSION_ACCEPTED,"benchmark shared pool");
    xbytesview Args[]={XRT_BYTES_LITERAL("ECHO"),XRT_BYTES_LITERAL("bytes\0x")};uint64 Sum=0;
    /* Warm the same mode outside all measured intervals. */
    xfuture* Warm=NULL;xredisadmission A;
#if !defined(SA_BENCH_BASELINE)
    if(Mode==1)A=xredisSharedPoolAskingCall(Pool,Args,2,5000,&Warm);else if(Mode==2)A=xredisLeaseAskingCall(Lease,Args,2,5000,&Warm);else
#endif
    A=xredisSharedPoolCall(Pool,Args,2,5000,&Warm);
    redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)xrtFutureValue(Warm))->Status==XREDIS_REQUEST_OK,"benchmark warm native call");xrtFutureDestroy(Warm);
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(Instrument);xrtMemStatsGet(&Before);
#else
    if(Instrument)return 2;
#endif
    double Started=xrtTimer();
    for(size_t Base=0;Base<Count;Base+=Depth){size_t N=Count-Base;if(N>Depth)N=Depth;
        for(size_t I=0;I<N;++I){Starts[I]=xrtTimer();Calls[I]=NULL;
#if !defined(SA_BENCH_BASELINE)
            if(Mode==1)A=xredisSharedPoolAskingCallAsync(Pool,Args,2,5000,&Calls[I]);else if(Mode==2)A=xredisLeaseAskingCallAsync(Lease,Args,2,5000,&Calls[I]);else
#endif
            A=xredisSharedPoolCallAsync(Pool,Args,2,5000,&Calls[I]);
            redisPeerRequire(A==XREDIS_ADMISSION_ACCEPTED,"benchmark direct native admission");}
        for(size_t I=0;I<N;++I){redisPeerRequire(xrtFutureWaitFor(Calls[I],10000)==XWAIT_OK,"benchmark call deadline");Samples[Base+I]=(xrtTimer()-Starts[I])*1e6;const xredisrequestresult* R=xrtFutureValue(Calls[I]);xredisreplyvalue V;
            redisPeerRequire(R && R->Status==XREDIS_REQUEST_OK && xredisReplyGet(R->Reply,0,&V) && V.Data.Size==7 && !memcmp(V.Data.Data,"bytes\0x",7),"benchmark same owned binary reply");
#if !defined(SA_BENCH_BASELINE)
            if(Mode){const xredisaskingresult* Pair=(const xredisaskingresult*)R;redisPeerRequire(Pair->ControlReceived && Pair->Control.Status==XREDIS_REQUEST_OK,"benchmark validated ASKING control");}
#endif
            Sum+=V.Data.Size;xrtFutureDestroy(Calls[I]);}
    }
    double Seconds=xrtTimer()-Started;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
#endif
    qsort(Samples,Count,sizeof(*Samples),askingBenchCompare);redisPeerRequire(Sum==Count*7,"benchmark checksum");
    printf("queries_per_sec: %.6f\nlatency_p50_us: %.6f\nlatency_p99_us: %.6f\nlatency_max_us: %.6f\nchecksum: %" PRIu64 "\n",Count/Seconds,Samples[(Count*50+99)/100-1],Samples[(Count*99+99)/100-1],Samples[Count-1],Sum);
#if defined(XRT_FEATURE_MEMORY_STATS)
    if(Instrument)printf("heap_blocks_per_query: %.6f\nheap_bytes_per_query: %.6f\n",(double)(After.BlockAllocCalls-Before.BlockAllocCalls)/Count,(double)(After.BlockAllocBytes-Before.BlockAllocBytes)/Count);
#endif
    if(Lease){xfuture* Returned=xredisLeaseClosed(Lease);redisPeerRequire(xredisLeaseReturn(Lease) && xrtFutureWaitFor(Returned,10000)==XWAIT_OK,"benchmark physical lease return");xrtFutureDestroy(Returned);xrtFutureDestroy(Acquired); }xfuture* Closed=xredisPoolClosed(Pool);xredisPoolClose(Pool);redisPeerRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"benchmark pool close");xredisPoolRelease(Pool);xrtFutureDestroy(Closed);xredisClientRelease(Client);redisPeerStop(Peer);xrtFree(Peer);free(Calls);free(Starts);free(Samples);return 0;
}
