#include "test.h"
#include "support/sentinel_peer.h"
static void sentinelSuccess(void)
{
    for(unsigned Secure=0;Secure<(ROLE_PEER_TLS?2u:1u);++Secure)for(unsigned Factory=0;Factory<
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    3
#else
    1
#endif
    ;++Factory)for(unsigned Mode=0;Mode<3;++Mode){
        sentinelfixture F;sentinelFixtureStart(&F,Factory,Secure!=0,Mode==0?XREDIS_INIT_RESP2:Mode==1?XREDIS_INIT_RESP3:XREDIS_INIT_AUTO);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelSeed(&F,"_\r\n",false,false);sentinelAddressSeed(&F,0);
        xredissentinelconfig K;sentinelConfig(&F,&K);K.SeedCount=3;K.PreferredSeed=2;unsigned char Service[]="cache";K.Service=(xbytesview){Service,5};xredissentineldiscovery* D=sentinelCreate(&F,&K);memset(Service,'x',5);
        xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==XREDIS_SENTINEL_OK && R.SeedAttempts==2 && R.UnknownReplies==1 && R.MasterAttempts==1 && R.PreferredSeed==0 && F.Order[0]==2 && F.Order[1]==0,"seed rotation, copied binary service and preferred seed");
        xredisclient* C=NULL;testRequire(xredisSentinelDiscoveryTakeClient(D,&C),"once-only native transfer");xredisclient* Bad=(xredisclient*)(uintptr_t)1;testRequire(!xredisSentinelDiscoveryTakeClient(D,&Bad) && Bad==(xredisclient*)(uintptr_t)1,"second transfer preserves output");xrtClearError();xredisSentinelDiscoveryDestroy(D);xfuture* Ping=roleClientPing(C,1000);testRequire(((const xredisrequestresult*)roleClientWait(Ping))->Status==XREDIS_REQUEST_OK,"direct business client");xrtFutureDestroy(Ping);xfuture* Closed=xredisClientClosed(C);xredisClientClose(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"transferred physical close");xrtFutureDestroy(Closed);xredisClientRelease(C);sentinelFixtureStop(&F);
    }
}
static void sentinelStale(void)
{
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_REPLICA_REPLY,false,false);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelAddressSeed(&F,0);sentinelAddressSeed(&F,1);
    xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==XREDIS_SENTINEL_OK && R.Rounds==2 && R.StaleReplies==1 && R.MasterAttempts==2 && R.SeedAttempts==2,"stale ROLE retires and re-resolves before accepting new endpoint");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
}
static void sentinelErrors(void)
{
    const cstr Replies[]={"*2\r\n+127.0.0.1\r\n+0\r\n","*2\r\n+127.0.0.1\r\n+65536\r\n","*2\r\n+127.0.0.1\r\n:-1\r\n","*2\r\n$3\r\na\0b\r\n+1\r\n","*1\r\n+x\r\n","+OK\r\n","-NOPERM synthetic secret must not escape\r\n","*2\r\n+1234567890\r\n+1\r\n","*2\r\n+127.0.0.1\r\n+12x\r\n"};
    for(size_t I=0;I<sizeof(Replies)/sizeof(*Replies);++I){
        if(I==3)continue; /* Binary host is exercised separately below. */
        sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,Replies[I],false,false);xredissentinelconfig K;sentinelConfig(&F,&K);K.MaxHostBytes=9;xredissentineldiscovery* D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==(I==6?XREDIS_SENTINEL_SERVER_ERROR:I==7?XREDIS_SENTINEL_LIMIT:XREDIS_SENTINEL_PROTOCOL) && !R.MasterAttempts,"typed endpoint/server/limit failure");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
    }
    for(unsigned Kind=0;Kind<3;++Kind){sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP2);sentinelSeed(&F,Kind==0?"*-1\r\n":"_\r\n",false,false);xredissentinelconfig K;sentinelConfig(&F,&K);K.SeedCount=2;K.MaxRounds=1;
        if(Kind==0)sentinelSeed(&F,"$-1\r\n",false,false);else F.SeedFailure=Kind==1?XREDIS_SENTINEL_UNREACHABLE:XREDIS_SENTINEL_MEMORY;
        xredissentineldiscovery* D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==(Kind==0?XREDIS_SENTINEL_UNKNOWN_SERVICE:Kind==1?XREDIS_SENTINEL_UNREACHABLE:XREDIS_SENTINEL_MEMORY),"unknown versus unreachable versus typed factory OOM");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);}
}
static void sentinelStops(void)
{
    for(unsigned Kind=0;Kind<4;++Kind){sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"",true,false);xredissentinelconfig K;sentinelConfig(&F,&K);K.Timeout=60;K.SeedTimeout=40;K.MaxRounds=1;
        xredissentineldiscovery* D=sentinelCreate(&F,&K);
        if(Kind==0){testRequire(xredisSentinelDiscoveryCancel(D),"prestart cancel");}
        else if(Kind==1){xrtSleep(80);}
        else {double End=xrtTimer()+3;while(xrtAtomic32Load(&roleClientPeerInit(&F.Seeds[0]->Peer)->StepsSeen,XMEMORY_ACQUIRE)<2){testRequire(xrtTimer()<End,"query started");xredisSentinelDiscoveryStep(D);xrtSleep(1);}if(Kind==2)xredisSentinelDiscoveryCancel(D);}
        xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==(Kind==0 || Kind==2?XREDIS_SENTINEL_CANCELLED:Kind==1?XREDIS_SENTINEL_TIMEOUT:XREDIS_SENTINEL_UNREACHABLE),"cancel/scheduling/global/seed timeout");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);}
}
static void sentinelAdditional(void)
{
    /* RESP3 attributes before the endpoint must not be mistaken for root0. */
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelAddressSeed(&F,0);
    sentinelpeer* P=F.Seeds[0];char Wire[240];snprintf(Wire,sizeof(Wire),"|1\r\n+meta\r\n+x\r\n%s",P->Response);memcpy(P->Response,Wire,strlen(Wire)+1);P->Steps[1].Response=(xbytesview){(cbytes)P->Response,strlen(P->Response)};
    xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=sentinelCreate(&F,&K);testRequire(sentinelDrive(D).Status==XREDIS_SENTINEL_OK,"attributed endpoint root");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
    /* Binary/control host, EOF, malformed ROLE and stale-round exhaustion. */
    for(unsigned Kind=0;Kind<5;++Kind){sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);
        if(Kind<2){sentinelSeed(&F,"+unused\r\n",false,Kind==1);if(Kind==0)F.Seeds[0]->Steps[1].Response=(xbytesview)XRT_BYTES_LITERAL("*2\r\n$3\r\na\0b\r\n+1\r\n");}
        else {sentinelMaster(&F,Kind==2?"+OK\r\n":Kind==3?ROLE_REPLICA_REPLY:"-ERR unknown command 'ROLE'\r\n",false,false);sentinelAddressSeed(&F,0);}
        sentinelConfig(&F,&K);K.MaxRounds=1;D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==(Kind==1?XREDIS_SENTINEL_UNREACHABLE:Kind==3?XREDIS_SENTINEL_STALE_ROLE:Kind==4?XREDIS_SENTINEL_SERVER_ERROR:XREDIS_SENTINEL_PROTOCOL),"binary host/EOF/malformed-role/stale bound/unsupported ROLE");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);}
    /* Destroy during a held query: retained native outcome has no helper access. */
    sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"",true,false);sentinelConfig(&F,&K);D=sentinelCreate(&F,&K);xfuture* Pending=NULL;double End=xrtTimer()+3;
    while(xrtAtomic32Load(&roleClientPeerInit(&F.Seeds[0]->Peer)->StepsSeen,XMEMORY_ACQUIRE)<2){testRequire(xrtTimer()<End,"held query admitted");xredisSentinelDiscoveryStep(D);xrtSleep(1);}
    Pending=xredisSentinelDiscoveryPending(D);testRequire(Pending!=NULL,"retained pending command");xredisSentinelDiscoveryDestroy(D);testRequire(xrtFutureWaitFor(Pending,10000)==XWAIT_OK,"native outcome after helper destruction");const xredisrequestresult* R=roleClientWait(Pending);testRequire(R->Status!=XREDIS_REQUEST_OK,"destroy closes candidate");sentinelFixtureStop(&F);testRequire(((const xredisrequestresult*)roleClientWait(Pending))->Status==R->Status,"retained immutable native outcome after engines");xrtFutureDestroy(Pending);
    /* Backoff does not renew the original deadline. */
    sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_REPLICA_REPLY,false,false);sentinelAddressSeed(&F,0);sentinelConfig(&F,&K);K.Timeout=150;K.RetryDelay=300;D=sentinelCreate(&F,&K);xredissentinelresult Q=sentinelDrive(D);testRequire(Q.Status==XREDIS_SENTINEL_TIMEOUT && Q.Rounds==1 && Q.SeedAttempts==1,"retry shares total deadline");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
}
static void sentinelRoleCaps(void)
{
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelAddressSeed(&F,0);xredissentinelconfig K;sentinelConfig(&F,&K);K.Role.MaxPayloadBytes=0;xredissentineldiscovery* D=sentinelCreate(&F,&K);testRequire(sentinelDrive(D).Status==XREDIS_SENTINEL_LIMIT,"explicit ROLE cap");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
    /* Cancellation at seed retirement must prevent target construction. */
    sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);sentinelAddressSeed(&F,0);sentinelConfig(&F,&K);D=sentinelCreate(&F,&K);
    double End=xrtTimer()+3;for(;;){if(F.ClientCount){xredisclientinfo I={0};I.Size=sizeof(I);xredisClientInfo(F.Clients[0],&I);if(I.State==XREDIS_CLIENT_CLOSED)break;}testRequire(xrtTimer()<End,"seed retirement boundary");xredisSentinelDiscoveryStep(D);xrtSleep(1);}
    xredisSentinelDiscoveryCancel(D);testRequire(sentinelDrive(D).Status==XREDIS_SENTINEL_CANCELLED,"cancel between query and target factory");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);
}
static unsigned sentinelBadFactoryKind;
static xredissentinelstatus sentinelBadFactory(xnetengine* E,size_t Seed,int64 Timeout,ptr Data,xredisclient** C)
{
    (void)Seed;sentinelfixture* F=Data;xredissentinelstatus S;
    if(!sentinelBadFactoryKind)S=xredisClientConnect(E,&roleClientPeerInit(&F->Seeds[0]->Peer)->Peer.Address,0,NULL,C)?XREDIS_SENTINEL_OK:XREDIS_SENTINEL_MEMORY;
    else S=sentinelConnect(F,F->Seeds[0],roleClientPeerInit(&F->Masters[0]->Peer)->Peer.Engine,Timeout,C);
    return sentinelRemember(F,S,C);
}
static void sentinelNativeFailures(void)
{
    const cstr Errors[]={"-WRONGPASS synthetic\r\n","-NOPROTO unsupported\r\n","+OK\r\n"};
    const xredisreadystatus Native[]={XREDIS_READY_AUTH_FAILED,XREDIS_READY_UNSUPPORTED,XREDIS_READY_PROTOCOL};
    for(unsigned I=0;I<3;++I){sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"_\r\n",false,false);F.Seeds[0]->Steps[0].Response=(xbytesview){(cbytes)Errors[I],strlen(Errors[I])};
        xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=sentinelCreate(&F,&K);xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==(I?XREDIS_SENTINEL_PROTOCOL:XREDIS_SENTINEL_AUTH_FAILED) && R.ReadyObserved && R.ReadyStatus==Native[I] && !R.RequestObserved && !R.MasterAttempts,"actual native auth/unsupported/protocol readiness mapping");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);}
    for(sentinelBadFactoryKind=0;sentinelBadFactoryKind<2;++sentinelBadFactoryKind){sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"_\r\n",false,false);sentinelMaster(&F,ROLE_MASTER_REPLY,false,false);xredissentinelconfig K;sentinelConfig(&F,&K);K.ConnectSeed=sentinelBadFactory;xredissentineldiscovery* D=sentinelCreate(&F,&K);testRequire(sentinelDrive(D).Status==XREDIS_SENTINEL_FACTORY,"reject raw/unmanaged or foreign-engine factory profile");xredisSentinelDiscoveryDestroy(D);sentinelFixtureStop(&F);}
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"_\r\n",false,false);xredissentinelconfig K;sentinelConfig(&F,&K);xredissentineldiscovery* D=sentinelCreate(&F,&K);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"actual native constructor OOM scope");testRequire(xredisSentinelDiscoveryStep(D)!=XREDIS_SENTINEL_STEP_ERROR,"typed OOM Step");testRequire(xrtAllocationBudgetLeave(&Scope),"leave native OOM scope");xrtClearError();xredissentinelresult R=sentinelDrive(D);testRequire(R.Status==XREDIS_SENTINEL_MEMORY && !R.ReadyObserved && !R.RequestObserved,"native admission OOM classified without ambient error");xredisSentinelDiscoveryDestroy(D);xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"no native OOM retained bytes");xrtAllocationBudgetFree(B);sentinelFixtureStop(&F);
}
static void sentinelBounds(void)
{
    sentinelfixture F;sentinelFixtureStart(&F,0,false,XREDIS_INIT_RESP3);sentinelSeed(&F,"_\r\n",false,false);xredissentinelconfig K;sentinelConfig(&F,&K);xnetengine* E=roleClientPeerInit(&F.Seeds[0]->Peer)->Peer.Engine;
    for(unsigned I=0;I<8;++I){xredissentinelconfig C=K;switch(I){case 0:C.Size=0;break;case 1:C.SeedCount=0;break;case 2:C.PreferredSeed=1;break;case 3:C.Timeout=0;break;case 4:C.MaxRounds=0;break;case 5:C.Service.Size=65537;break;case 6:C.ConnectMaster=NULL;break;default:C.MaxHostBytes=65537;break;}
        xredissentineldiscovery* D=(xredissentineldiscovery*)(uintptr_t)1;testRequire(xredisSentinelDiscoveryCreate(E,&C,&D)==XREDIS_ADMISSION_ERROR && D==(xredissentineldiscovery*)(uintptr_t)1,"invalid arguments preserve output");xrtClearError();}
    bool Rejected=false,Accepted=false;for(size_t Limit=1;Limit<6000;Limit+=131){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"constructor quota");xredissentineldiscovery* D=NULL;xredisadmission A=xredisSentinelDiscoveryCreate(E,&K,&D);if(A==XREDIS_ADMISSION_ACCEPTED){Accepted=true;xredisSentinelDiscoveryDestroy(D);}else{Rejected=true;testRequire(!D,"OOM preserves output");xrtClearError();}testRequire(xrtAllocationBudgetLeave(&Scope),"leave quota");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveCount && !Info.LiveBytes,"constructor/destroy no outstanding memory");xrtAllocationBudgetFree(B);}testRequire(Rejected && Accepted,"quotas straddle allocation");sentinelFixtureStop(&F);
}
int main(void)
{sentinelSuccess();sentinelStale();sentinelErrors();sentinelStops();sentinelAdditional();sentinelRoleCaps();sentinelNativeFailures();sentinelBounds();puts("Sentinel discovery tests passed");return 0;}
