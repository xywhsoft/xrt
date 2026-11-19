#include "test.h"
#define REDIS_PEER_NODE_CACHE_BYTES 0
#include "support/sentinel_events_peer.h"

static void seWaitState(xredispool* P,uint64 Generation,xredissubscriptionsessionstate State)
{
    double End=xrtTimer()+10;
    for(;;){
        xredissentineleventinfo I=seInfo(P);
        if(I.Session.State==State && I.Session.Generations>=Generation)return;
        testRequire(xrtTimer()<End,"listener state deadline");
        xrtSleep(1);
    }
}
static void seBehavior(bool Secure,bool Resp2)
{
    printf("listener TLS=%u RESP2=%u\n",(unsigned)Secure,(unsigned)Resp2);
    sefixture F;seStart(&F,Secure,Resp2,true);
    xredispool* P=seCreate(&F,2);
    xfuture *First=spAcquire(P,5000), *Second=spAcquire(P,5000);
    xredislease *L=spLease(First), *Other=spLease(Second);
    xredissentineleventinfo Unconfigured={.Size=sizeof(Unconfigured)};
    testRequire(!xredisSentinelPoolEventInfo(P,&Unconfigured),"disabled listener route");
    xrtClearError();
    xredissentineleventconfig K;xredisSentinelEventConfigInit(&K);K.WorkMessages=65;
    testRequire(xredisSentinelPoolListen(P,&K)==XREDIS_ADMISSION_ERROR,"message budget bound");
    xrtClearError();
    uint32 WarmRoles=xrtAtomic32Load(&F.Pool.Roles,XMEMORY_ACQUIRE);
    seListen(P,1500);
    xredisSentinelEventConfigInit(&K);
    testRequire(xredisSentinelPoolListen(P,&K)==XREDIS_ADMISSION_OVERLOADED,"one listener per pool");
    seWaitState(P,1,XREDIS_SESSION_ONLINE);
    double End=xrtTimer()+10;
    while(!seInfo(P).Hints){testRequire(xrtTimer()<End,"first subscribe catch-up");xrtSleep(1);}
    /* The startup hint has queried the old address before changing topology. */
    redisPeerWait(&F.Pool.Roles,WarmRoles+1);
    uint64 Hints=seInfo(P).Hints;
    seSend(&F,0,"cache-other 127.0.0.1 1234 127.0.0.1 5678",3);
    seSend(&F,0,"cache 127.0.0.1 0 127.0.0.1 70000",3);
    End=xrtTimer()+10;
    while(seInfo(P).Ignored<6){testRequire(xrtTimer()<End,"ignored event deadline");xrtSleep(1);}
    testRequire(seInfo(P).Hints==Hints && !seInfo(P).Matched && spInfo(P).Generation==1,"filter service and malformed payload");
    spPing(L);spPing(Other);

    xbytesview Hold=XRT_BYTES_LITERAL("HOLD");xfuture* Sent=NULL;
    uint32 Commands=xrtAtomic32Load(&F.Pool.A.Base.Commands,XMEMORY_ACQUIRE);
    testRequire(xredisLeaseCallAsync(L,&Hold,1,10000,&Sent)==XREDIS_ADMISSION_ACCEPTED,"old request");
    redisPeerWait(&F.Pool.A.Base.Commands,Commands+1);
    xrtAtomicPtrStore(&F.Pool.Watched,Other,XMEMORY_RELEASE);
    xfuture* Waiting=spAcquire(P,10000);
    xrtAtomic32Store(&F.Pool.Current,1,XMEMORY_RELEASE);
    seSend(&F,0,"cache old.example 1000 203.0.113.22 2000",12);
    xredislease* New=spLease(Waiting);
    const xredisrequestresult* Old=spWait(Sent);
    testRequire(Old->Status!=XREDIS_REQUEST_OK && Old->Delivery==XREDIS_MAY_HAVE_EXECUTED &&
        !xrtAtomic32Load(&F.Pool.B.Base.Commands,XMEMORY_ACQUIRE),"event switch preserves uncertainty without replay");
    testRequire(spInfo(P).Generation==2 && spInfo(P).Port==F.Pool.B.Base.Address.Port &&
        xrtAtomic32Load(&F.Pool.FenceRejects,XMEMORY_ACQUIRE)!=0,"event address ignored, verified topology used");
    End=xrtTimer()+10;
    while(seInfo(P).Matched<12){testRequire(xrtTimer()<End,"bounded batches drain all events");xrtSleep(1);}
    testRequire(seInfo(P).Hints==Hints+1,"event burst coalesces to one discovery hint");
    xrtAtomicPtrStore(&F.Pool.Watched,NULL,XMEMORY_RELEASE);
    spPing(New);

    /* Isolate subscriber recovery from the previous switch's unfinished refill. */
    End=xrtTimer()+10;
    for(;;){
        xredispoolinfo Members={.Size=sizeof(Members)};
        testRequire(xredisPoolInfo(P,&Members),"pool member snapshot");
        if(Members.Live==2 && !spInfo(P).Connect.Connecting)break;
        testRequire(xrtTimer()<End,"second verified slot ready before listener loss");
        xrtSleep(1);
    }
    xrtAtomic32Store(&F.Pool.SeedOffline,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&F.Pool.Current,0,XMEMORY_RELEASE);
    seSend(&F,0,NULL,0);
    seWaitState(P,2,XREDIS_SESSION_ONLINE);
    End=xrtTimer()+10;
    while(spInfo(P).Generation!=3){testRequire(xrtTimer()<End,"missed event catch-up via configured backup seed");xrtSleep(1);}
    xredissentineleventinfo Events=seInfo(P);
    xredissentinelpoolinfo Topology=spInfo(P);
    if(Events.Seed!=1 || Topology.PreferredSeed!=1 || Topology.Port!=F.Pool.A.Base.Address.Port)
        fprintf(stderr,"rotation event_seed=%zu topology_seed=%zu actual=%u expected=%u\n",
            Events.Seed,Topology.PreferredSeed,(unsigned)Topology.Port,(unsigned)F.Pool.A.Base.Address.Port);
    testRequire(Events.Seed==1 && Topology.PreferredSeed==1 && Topology.Port==F.Pool.A.Base.Address.Port,
        "subscriber and discovery rotated to configured backup seed");
    xfuture* Recovered=spAcquire(P,5000);spPing(spLease(Recovered));
    seStop(&F,P);
    xrtFutureDestroy(First);xrtFutureDestroy(Second);xrtFutureDestroy(Waiting);xrtFutureDestroy(Sent);xrtFutureDestroy(Recovered);
}
static void seFailure(uint32 Mode)
{
    sefixture F;seStart(&F,false,false,false);
    xredispool* P=seCreate(&F,1);
    xfuture* A=spAcquire(P,5000);xredislease* L=spLease(A);
    xrtAtomic32Store(&F.Mode,Mode,XMEMORY_RELEASE);
    seListen(P,1);
    redisPeerWait(&F.Subscribes,1);
    if(Mode==1){
        seWaitState(P,0,XREDIS_SESSION_CLOSED);
        testRequire(seInfo(P).Session.LastFailure==XREDIS_REQUEST_SERVER_ERROR,"subscription ACL failure is observable");
        spPing(L);
    }
    seStop(&F,P);
    xrtFutureDestroy(A);
}
typedef struct sebudget {
    xallocationbudget* Budget;
    xallocationscope Scope;
    xatomic32 Entered, Left;
} sebudget;
static void seBudgetEnter(xnetworker* Worker,ptr Data)
{
    (void)Worker;
    sebudget* B=Data;
    testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"listener allocation scope");
    xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);
}
static void seBudgetLeave(xnetworker* Worker,ptr Data)
{
    (void)Worker;
    sebudget* B=Data;
    testRequire(xrtAllocationBudgetLeave(&B->Scope),"leave listener allocation scope");
    xrtAtomic32Store(&B->Left,1,XMEMORY_RELEASE);
}
static void seOOM(void)
{
    sefixture F;seStart(&F,false,false,false);
    xredispool* P=seCreate(&F,1);
    xfuture* A=spAcquire(P,5000);xredislease* L=spLease(A);
    sebudget B={0};
    xnetengine* Engine=F.Pool.Seed.Base.Engine;
    testRequire(xrtAllocationBudgetCreate(1,&B.Budget) &&
        xrtNetEnginePost(Engine,0,seBudgetEnter,&B),"limit listener worker");
    redisPeerWait(&B.Entered,1);
    seListen(P,1);
    seWaitState(P,0,XREDIS_SESSION_CLOSED);
    testRequire(seInfo(P).Session.LastFailure==XREDIS_REQUEST_MEMORY &&
        !xrtAtomic32Load(&F.Subscribes,XMEMORY_ACQUIRE),"listener construction OOM stays isolated");
    testRequire(xrtNetEnginePost(Engine,0,seBudgetLeave,&B),"release worker limit");
    redisPeerWait(&B.Left,1);
    xrtAllocationBudgetFree(B.Budget);
    spPing(L);
    seStop(&F,P);
    xrtFutureDestroy(A);
}
int main(void)
{
    seBehavior(false,true);seBehavior(false,false);
#if SP_TLS
    seBehavior(true,false);seBehavior(true,true);
#endif
    seFailure(1);seFailure(2);seOOM();
    puts("Sentinel event listener tests passed");
    return 0;
}
