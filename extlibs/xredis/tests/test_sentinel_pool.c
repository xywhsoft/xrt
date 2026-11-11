#include "test.h"
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/sentinel_pool_peer.h"
#include "../src/internal/sentinel_config.h"
static void spRepairTest(void)
{
    for(unsigned Secure=0;Secure<(SP_TLS?2u:1u);++Secure)for(unsigned Factory=0;Factory<
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    3
#else
    1
#endif
    ;++Factory)for(unsigned Resp2=0;Resp2<2;++Resp2){
        printf("repair factory=%u TLS=%u RESP2=%u\n",Factory,Secure,Resp2);fflush(stdout);spfixture F;spStart(&F,Factory,Secure!=0,Resp2!=0);xredissentinelpoolconfig K;spConfig(&F,&K);xredispoolconfig Options;xredisPoolConfigInit(&Options);Options.WorkItems=1;
        xredispool* P=spCreate(&F,2,&Options,&K);xfuture *A=spAcquire(P,5000),*B=spAcquire(P,5000);xredislease *First=spLease(A),*Second=spLease(B);spPing(First);spPing(Second);spReturn(First,true);
        xfuture* Again=spAcquire(P,5000);spPing(spLease(Again));spPing(Second);xredissentinelpoolinfo I=spInfo(P);testRequire(I.Generation==1 && I.Changes==0 && I.Connect.Connected==3 && xrtAtomic32Load(&F.Queries,XMEMORY_ACQUIRE)>=3,"same endpoint reconnect rediscovers without retiring unaffected member");
        spReturn(spLease(Again),false);spReturn(Second,false);spClose(&F,P);spStop(&F);testRequire(spInfo(P).Generation==1,"closed metadata after engine");xredisPoolRelease(P);xrtFutureDestroy(A);xrtFutureDestroy(B);xrtFutureDestroy(Again);
    }
}
static void spSwitchTest(bool Close)
{
    printf("switch close=%u\n",(unsigned)Close);fflush(stdout);spfixture F;spStart(&F,0,false,false);xredissentinelpoolconfig K;spConfig(&F,&K);xredispoolconfig O;xredisPoolConfigInit(&O);O.WorkItems=1;
    xredispool* P=spCreate(&F,2,&O,&K);xfuture *A=spAcquire(P,5000),*B=spAcquire(P,5000);xredislease *First=spLease(A),*Second=spLease(B);xrtAtomicPtrStore(&F.Watched,Second,XMEMORY_RELEASE);F.CloseFromNotify=Close;
    xbytesview Hold=XRT_BYTES_LITERAL("HOLD");xfuture* Request=NULL;testRequire(xredisLeaseCallAsync(Second,&Hold,1,5000,&Request)==XREDIS_ADMISSION_ACCEPTED,"old sent command");redisPeerWait(&F.A.Base.Commands,1);
    xrtAtomic32Store(&F.Current,1,XMEMORY_RELEASE);spReturn(First,true);
    if(!Close){xfuture* Next=spAcquire(P,5000);xredislease* L=spLease(Next);xredissentinelpoolinfo I=spInfo(P);testRequire(I.Generation==2 && I.Changes==1 && I.Port==F.B.Base.Address.Port && xrtAtomic32Load(&F.FenceRejects,XMEMORY_ACQUIRE)>0,"verified change fences all old lease admissions");
        const xredisrequestresult* Old=spWait(Request);testRequire(Old->Status!=XREDIS_REQUEST_OK && Old->Delivery==XREDIS_MAY_HAVE_EXECUTED && xrtAtomic32Load(&F.B.Base.Commands,XMEMORY_ACQUIRE)==0,"uncertain old command never replayed");spPing(L);spReturn(L,false);xrtFutureDestroy(Next);}
    else {xfuture* Closed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"close reentry during fence");xrtFutureDestroy(Closed);}
    spClose(&F,P);spStop(&F);testRequire(((const xredisrequestresult*)spWait(Request))->Delivery==XREDIS_MAY_HAVE_EXECUTED,"retained uncertain outcome after engine");xredisPoolRelease(P);xrtFutureDestroy(Request);xrtFutureDestroy(A);xrtFutureDestroy(B);
}
static void spFaultTests(void)
{
    puts("fault tests");fflush(stdout);for(unsigned Mode=1;Mode<4;++Mode){spfixture F;spStart(&F,0,false,false);xrtAtomic32Store(&F.QueryMode,Mode,XMEMORY_RELEASE);xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,1,NULL,&K);
        xfuture* Closed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && spInfo(P).LastStatus==(Mode==1?XREDIS_SENTINEL_UNKNOWN_SERVICE:Mode==2?XREDIS_SENTINEL_PROTOCOL:XREDIS_SENTINEL_SERVER_ERROR),"fatal unknown/protocol/ACL status retained through physical close");spClose(&F,P);spStop(&F);xrtFutureDestroy(Closed);xredisPoolRelease(P);}
    spfixture F;spStart(&F,0,false,false);xrtAtomic32Store(&F.Seed.Base.Mode,PC_BAD_HELLO,XMEMORY_RELEASE);xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,1,NULL,&K);xfuture* Closed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && spInfo(P).LastStatus==XREDIS_SENTINEL_AUTH_FAILED,"authentication failure fails closed");spClose(&F,P);spStop(&F);xrtFutureDestroy(Closed);xredisPoolRelease(P);
    for(unsigned Mode=4;Mode<6;++Mode){spStart(&F,0,false,false);xrtAtomic32Store(&F.QueryMode,Mode,XMEMORY_RELEASE);spConfig(&F,&K);K.MaxConnecting=2;K.Discovery.Timeout=200;K.Discovery.SeedTimeout=50;K.Discovery.MaxRounds=1;xredispoolconfig Options;xredisPoolConfigInit(&Options);Options.WorkItems=1;P=spCreate(&F,3,&Options,&K);
        redisPeerWait(&F.Queries,2);xredissentinelpoolinfo I=spInfo(P);testRequire(I.Connect.Connecting<=2 && I.Connect.Waiting>=1 && !I.Connect.Connected,"bounded complete discovery concurrency");
        xfuture *Expired=spAcquire(P,10),*Cancelled=spAcquire(P,XRT_WAIT_FOREVER);testRequire(((const xredisleaseresult*)spWait(Expired))->Status==XREDIS_REQUEST_TIMEOUT,"outage acquisition original deadline");xrtFutureCancel(Cancelled);testRequire(((const xredisleaseresult*)spWait(Cancelled))->Status==XREDIS_REQUEST_CANCELLED,"outage independent cancellation");
        if(Mode==4){xrtAtomic32Store(&F.QueryMode,0,XMEMORY_RELEASE);xfuture* Again=spAcquire(P,5000);spPing(spLease(Again));spReturn(spLease(Again),false);xrtFutureDestroy(Again);}
        spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(Expired);xrtFutureDestroy(Cancelled);}
    spStart(&F,0,false,false);xrtAtomic32Store(&F.Stale,1,XMEMORY_RELEASE);spConfig(&F,&K);K.Discovery.Timeout=500;P=spCreate(&F,1,NULL,&K);redisPeerWait(&F.Roles,1);testRequire(!spInfo(P).Connect.Connected,"stale role never becomes available");xrtAtomic32Store(&F.Stale,0,XMEMORY_RELEASE);xfuture* Again=spAcquire(P,5000);spPing(spLease(Again));spReturn(spLease(Again),false);spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(Again);
}
static void spBoundsTests(void)
{
    spfixture F;spStart(&F,0,false,false);xredissentinelpoolconfig K;spConfig(&F,&K);
    for(unsigned Kind=0;Kind<9;++Kind){xredissentinelpoolconfig C=K;size_t Count=2;switch(Kind){case 0:C.Size=0;break;case 1:C.MaxConnecting=0;break;case 2:C.RetryMin=0;break;case 3:C.RetryMax=60001;break;case 4:C.Discovery.Timeout=60001;break;case 5:C.Discovery.ConnectSeed=NULL;break;case 6:C.Discovery.Service.Size=65537;break;case 7:Count=0;break;default:Count=65537;break;}xredispool* Out=(xredispool*)(uintptr_t)1;testRequire(xredisSentinelPoolCreate(F.Seed.Base.Engine,0,Count,NULL,&C,&Out)==XREDIS_ADMISSION_ERROR && Out==(xredispool*)(uintptr_t)1 && !xrtAtomic32Load(&F.Drops,XMEMORY_ACQUIRE),"invalid config preserves output/context");xrtClearError();}
    unsigned Accepted=0,Rejected=0;
    for(size_t Limit=1;Limit<24000;Limit+=977){xrtAtomic32Store(&F.Drops,0,XMEMORY_RELAXED);xrtAtomicPtrStore(&F.Pool,NULL,XMEMORY_RELEASE);F.ClosedCount=0;xallocationbudget* Budget=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&Budget) && xrtAllocationBudgetEnter(Budget,&Scope),"Sentinel constructor quota");xredispool* P=(xredispool*)(uintptr_t)1;xredisadmission A=xredisSentinelPoolCreate(F.Seed.Base.Engine,0,2,NULL,&K,&P);testRequire(xrtAllocationBudgetLeave(&Scope),"leave Sentinel constructor quota");
        if(A==XREDIS_ADMISSION_ACCEPTED){++Accepted;xrtAtomicPtrStore(&F.Pool,P,XMEMORY_RELEASE);spClose(&F,P);xredisPoolRelease(P);}else{++Rejected;testRequire(P==(xredispool*)(uintptr_t)1 && !xrtAtomic32Load(&F.Drops,XMEMORY_ACQUIRE),"OOM preserves context and output");xrtClearError();}
        double End=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(Budget,&I),"constructor quota accounting");if(!I.LiveCount && !I.LiveBytes)break;testRequire(xrtTimer()<End,"constructor quota physical tail leaked");xrtThreadYield();}xrtAllocationBudgetFree(Budget);
    }testRequire(Accepted && Rejected,"quota boundary crossed");spStop(&F);
}
typedef struct spworkerbudget {xallocationbudget* Budget;xallocationscope Scope;xatomic32 Done;bool Enter;} spworkerbudget;
static void spWorkerBudget(xnetworker* W,ptr Data)
{(void)W;spworkerbudget* B=Data;testRequire(B->Enter?xrtAllocationBudgetEnter(B->Budget,&B->Scope):xrtAllocationBudgetLeave(&B->Scope),"owner budget scope");xrtAtomic32Store(&B->Done,1,XMEMORY_RELEASE);}
static void spOwnerOOM(void)
{
    spfixture F;spStart(&F,0,false,false);spworkerbudget B={0};B.Enter=true;testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetEnginePost(F.Seed.Base.Engine,0,spWorkerBudget,&B),"owner one-byte budget");redisPeerWait(&B.Done,1);xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,2,NULL,&K);xfuture* Closed=xredisPoolClosed(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && spInfo(P).LastStatus==XREDIS_SENTINEL_MEMORY && spInfo(P).Connect.Attempts==1 && !xrtAtomic32Load(&F.Calls,XMEMORY_ACQUIRE),"owner discovery OOM closes without retry/factory spin");spClose(&F,P);xredisPoolRelease(P);xrtFutureDestroy(Closed);B.Enter=false;xrtAtomic32Store(&B.Done,0,XMEMORY_RELEASE);testRequire(xrtNetEnginePost(F.Seed.Base.Engine,0,spWorkerBudget,&B),"owner budget leave");redisPeerWait(&B.Done,1);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveCount && !I.LiveBytes,"owner OOM no retained allocations");xrtAllocationBudgetFree(B.Budget);spStop(&F);
}
static void spOrdinaryPoolInfo(void)
{
    spfixture F;spStart(&F,0,false,false);redisconnectfactory Factory;xredispoolconnectconfig K;redisConnectFactoryConfig(&Factory,&F.A.Base,&K);xredispool* P=NULL;testRequire(xredisPoolConnect(F.Seed.Base.Engine,0,1,NULL,&K,&P)==XREDIS_ADMISSION_ACCEPTED,"ordinary pool on same engine");xfuture* A=spAcquire(P,5000);spPing(spLease(A));spReturn(spLease(A),false);xredissentinelpoolinfo I={0};I.Size=sizeof(I);testRequire(!xredisSentinelPoolInfo(P,&I),"ordinary live pool rejects Sentinel snapshot");xrtClearError();xfuture* Closed=xredisPoolClosed(P);xredisPoolClose(P);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK && !xredisSentinelPoolInfo(P,&I),"ordinary closed connector is never cast as Sentinel state");xrtClearError();xredisPoolRelease(P);xrtFutureDestroy(Closed);xrtFutureDestroy(A);spStop(&F);
}
static void spNestedFence(void)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    spfixture F;spStart(&F,0,false,false);xredissentinelpoolconfig K;spConfig(&F,&K);xredispool* P=spCreate(&F,2,NULL,&K);xfuture *A=spAcquire(P,5000),*B=spAcquire(P,5000);xredislease *L=spLease(A),*Other=spLease(B);
    xbytesview Key=XRT_BYTES_LITERAL("topology-watch");xfuture* Started=NULL;testRequire(xredisLeaseWatch(Other,&Key,1,NULL,&Started)==XREDIS_ADMISSION_ACCEPTED,"nested WATCH begin");const xrediswatchresult* W=spWait(Started);testRequire(W->Request.Status==XREDIS_REQUEST_OK && W->Watch,"nested WATCH active");xfuture* Child=xredisWatchClosed(W->Watch);
    xrtAtomic32Store(&F.Current,1,XMEMORY_RELEASE);spReturn(L,true);xfuture* Next=spAcquire(P,5000);spPing(spLease(Next));testRequire(xrtFutureState(Child)==XFUTURE_RESOLVED && spInfo(P).Generation==2,"old nested owner retires before new generation grant");spReturn(spLease(Next),false);spClose(&F,P);spStop(&F);xredisPoolRelease(P);xrtFutureDestroy(A);xrtFutureDestroy(B);xrtFutureDestroy(Started);xrtFutureDestroy(Child);xrtFutureDestroy(Next);
#endif
}
typedef struct spproducer {spfixture* Fixture;xredispool* Pool;xatomic32 *Ready,*Go;unsigned Role;bool Close;} spproducer;
static int32 spProduce(ptr Data)
{
    spproducer* T=Data;xrtAtomic32FetchAdd(T->Ready,1,XMEMORY_RELEASE);redisPeerWait(T->Go,1);
    for(unsigned I=0;I<8;++I){xfuture* A=NULL;xredisadmission Admission=xredisPoolAcquire(T->Pool,5000,&A);if(Admission==XREDIS_ADMISSION_CLOSED)break;testRequire(Admission==XREDIS_ADMISSION_ACCEPTED,"concurrent Sentinel acquisition");const xredisleaseresult* R=spWait(A);if(R->Status==XREDIS_REQUEST_CLOSED){xrtFutureDestroy(A);break;}testRequire(R->Status==XREDIS_REQUEST_OK,"concurrent recovery grant");
        if(T->Role==0 && I==2)xrtAtomic32Store(&T->Fixture->Current,1,XMEMORY_RELEASE);
        xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* C=(xfuture*)(uintptr_t)1;Admission=xredisLeaseCallAsync(R->Lease,&Ping,1,5000,&C);
        if(Admission==XREDIS_ADMISSION_ACCEPTED){const xredisrequestresult* B=spWait(C);testRequire(B->Status==XREDIS_REQUEST_OK || B->Status==XREDIS_REQUEST_CLOSED || B->Status==XREDIS_REQUEST_DISCONNECTED,"concurrent native outcome");xrtFutureDestroy(C);}else testRequire(Admission==XREDIS_ADMISSION_CLOSED && C==(xfuture*)(uintptr_t)1,"fence preserved rejected output");
        if(T->Close && T->Role==3 && I==4)xredisPoolClose(T->Pool);
        spReturn(R->Lease,(I%3)==2);xrtFutureDestroy(A);
    }return 0;
}
static void spRaceTests(void)
{
    for(unsigned Round=0;Round<4;++Round){spfixture F;spStart(&F,0,false,false);xredissentinelpoolconfig K;spConfig(&F,&K);K.MaxConnecting=2;xredispoolconfig O;xredisPoolConfigInit(&O);O.WorkItems=1;xredispool* P=spCreate(&F,4,&O,&K);xatomic32 Ready,Go;xrtAtomic32Init(&Ready,0);xrtAtomic32Init(&Go,0);spproducer T[4];xthread* Threads[4];
        for(unsigned N=0;N<4;++N){T[N]=(spproducer){&F,P,&Ready,&Go,N,Round%2!=0};Threads[N]=xrtThreadCreate(spProduce,&T[N],0);testRequire(Threads[N]!=NULL,"recovery producer thread");}redisPeerWait(&Ready,4);xrtAtomic32Store(&Go,1,XMEMORY_RELEASE);for(unsigned N=0;N<4;++N){testRequire(xrtThreadWait(Threads[N])==XWAIT_OK && !xrtThreadExitCode(Threads[N]),"recovery producer join");xrtThreadDestroy(Threads[N]);}spClose(&F,P);spStop(&F);xredisPoolRelease(P);
    }
}
static void spInheritedDeadline(void)
{
    spfixture F;spStart(&F,0,false,false);xredissentinelpoolconfig K;spConfig(&F,&K);xredissentineldiscovery* D=NULL;
    testRequire(__xredisSentinelDiscoveryCreateUntil(F.Seed.Base.Engine,&K.Discovery,xrtTimer()-.1,&D)==XREDIS_ADMISSION_ACCEPTED,"inherited absolute deadline construction");
    testRequire(xredisSentinelDiscoveryStep(D)==XREDIS_SENTINEL_DONE,"inherited deadline expires before native factory");xredissentinelresult R={0};R.Size=sizeof(R);testRequire(xredisSentinelDiscoveryResult(D,&R) && R.Status==XREDIS_SENTINEL_TIMEOUT && !xrtAtomic32Load(&F.Calls,XMEMORY_ACQUIRE),"relative defaults cannot reset inherited deadline");xredisSentinelDiscoveryDestroy(D);spStop(&F);
}
int main(void)
{spRepairTest();spSwitchTest(false);spSwitchTest(true);spFaultTests();spBoundsTests();spOwnerOOM();spOrdinaryPoolInfo();spNestedFence();spRaceTests();spInheritedDeadline();puts("Sentinel pool tests passed");return 0;}
