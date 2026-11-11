#include "test.h"
#include "support/role_client_peer.h"
static const uint32 roleFlags[]={XREDIS_INIT_REQUIRE_MASTER,XREDIS_INIT_REQUIRE_REPLICA,XREDIS_INIT_REQUIRE_SENTINEL};
static const cstr roleReplies[]={ROLE_MASTER_REPLY,ROLE_REPLICA_REPLY,ROLE_SENTINEL_REPLY};
static void roleReady(const xredisreadyresult* R,xredisprotocol Protocol,bool Downgraded)
{testRequire(R->Status==XREDIS_READY_OK && R->Step==XREDIS_INIT_DONE && R->Protocol==Protocol && R->Downgraded==Downgraded && R->Negotiated==(Protocol==XREDIS_RESP3) && (R->Hello!=NULL)==(Protocol==XREDIS_RESP3),"role readiness and original HELLO metadata");}
static void roleSuccess(void)
{
    for(unsigned Secure=0;Secure<(ROLE_PEER_TLS?2u:1u);++Secure)for(unsigned Mode=0;Mode<4;++Mode)for(unsigned Kind=0;Kind<3;++Kind){
        unsigned Factory=0;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
        Factory=Kind;
#endif
        unsigned char Pass[]={'p',0,'\r','\n'},Name[]={'n'};xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=Mode==0?XREDIS_INIT_RESP2:Mode==1?XREDIS_INIT_RESP3:XREDIS_INIT_AUTO;
        Init.Flags=roleFlags[Kind]|XREDIS_INIT_AUTH|XREDIS_INIT_USERNAME|XREDIS_INIT_SETNAME|XREDIS_INIT_SELECT;Init.Username=(xbytesview)XRT_BYTES_LITERAL("u");Init.Password=(xbytesview){Pass,sizeof(Pass)};Init.Name=(xbytesview){Name,sizeof(Name)};Init.Database=1;
        redisinitpeerstep Steps[6]={0};size_t Count=0;bool Legacy=Mode==0 || Mode==3;
        if(Mode!=0)Steps[Count++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3"),XRT_BYTES_LITERAL("AUTH"),XRT_BYTES_LITERAL("u"),XRT_BYTES_LITERAL("p\0\r\n"),XRT_BYTES_LITERAL("SETNAME"),XRT_BYTES_LITERAL("n")},7,Mode==3?(xbytesview)XRT_BYTES_LITERAL("-NOPROTO unsupported version\r\n"):(xbytesview)XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),false,false};
        if(Legacy){Steps[Count++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("AUTH"),XRT_BYTES_LITERAL("u"),XRT_BYTES_LITERAL("p\0\r\n")},3,XRT_BYTES_LITERAL("+OK\r\n"),false,false};Steps[Count++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("CLIENT"),XRT_BYTES_LITERAL("SETNAME"),XRT_BYTES_LITERAL("n")},3,XRT_BYTES_LITERAL("+OK\r\n"),false,false};}
        Steps[Count++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("SELECT"),XRT_BYTES_LITERAL("1")},2,XRT_BYTES_LITERAL("+OK\r\n"),false,false};Steps[Count++]=(redisinitpeerstep){{XRT_BYTES_LITERAL("ROLE")},1,{(cbytes)roleReplies[Kind],strlen(roleReplies[Kind])},false,false};
        roleclientpeer P;roleClientPeerStart(&P,Secure!=0,Factory!=0,Steps,Count);xredisclientconfig Config;xredisClientConfigInit(&Config);Config.ReadBytes=1;Config.ReadFrames=1;Config.WorkRequests=1;
        xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,Factory,&Config,&Init,&C),"role factory admission");memset(Pass,0x55,sizeof(Pass));memset(Name,0x55,sizeof(Name));xfuture* Ready=xredisClientReady(C);xfuture* Ping=roleClientPing(C,5000);
        roleReady(roleClientWait(Ready),Legacy?XREDIS_RESP2:XREDIS_RESP3,Mode==3);const xredisrequestresult* R=roleClientWait(Ping);testRequire(R->Status==XREDIS_REQUEST_OK && R->Delivery==XREDIS_REPLIED,"queued business follows role");redisinitpeer* I=roleClientPeerInit(&P);redisPeerWait(&I->Peer.Commands,(uint32)Count+1);testRequire(xrtAtomic32Load(&I->Peer.Commands,XMEMORY_ACQUIRE)==Count+1,"role sent exactly once before business");
        xrtFutureDestroy(Ping);roleClientClose(&P,C);roleReady(roleClientWait(Ready),Legacy?XREDIS_RESP2:XREDIS_RESP3,Mode==3);xrtFutureDestroy(Ready);
    }
}
static void roleBarrier(void)
{
    for(unsigned Secure=0;Secure<(ROLE_PEER_TLS?2u:1u);++Secure){const redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),true,false};roleclientpeer P;roleClientPeerStart(&P,Secure!=0,false,&Step,1);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=XREDIS_INIT_RESP2;Init.Flags=XREDIS_INIT_REQUIRE_MASTER;
        xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,0,NULL,&Init,&C),"role-only connect");xfuture* Ready=xredisClientReady(C);redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,1);testRequire(xrtFutureWaitFor(Ready,0)==XWAIT_TIMEOUT,"ROLE is a readiness barrier");xredisclientinfo Info={0};Info.Size=sizeof(Info);testRequire(xredisClientInfo(C,&Info) && !Info.Ready && Info.InitStep==XREDIS_INIT_ROLE && Info.InitBytes==14,"role-only wire accounting");
        xfuture* Expired=roleClientPing(C,20);xfuture* Cancelled=roleClientPing(C,5000);xfuture* Live=roleClientPing(C,5000);testRequire(xrtFutureCancel(Ready) && xrtFutureCancel(Cancelled),"independent observer/request cancellation");const xredisrequestresult* R=roleClientWait(Expired);testRequire(R->Status==XREDIS_REQUEST_TIMEOUT && R->Delivery==XREDIS_NOT_SENT,"queued timeout includes role wait");R=roleClientWait(Cancelled);testRequire(R->Status==XREDIS_REQUEST_CANCELLED && R->Delivery==XREDIS_NOT_SENT,"queued cancel stays unsent");testRequire(xrtAtomic32Load(&I->Peer.Commands,XMEMORY_ACQUIRE)==1 && xrtFutureWaitFor(Ready,0)==XWAIT_TIMEOUT,"business cannot cross role barrier");
        redisPeerReleaseBlocked(&I->Peer);roleReady(roleClientWait(Ready),XREDIS_RESP2,false);R=roleClientWait(Live);testRequire(R->Status==XREDIS_REQUEST_OK,"live request after role");xrtFutureDestroy(Expired);xrtFutureDestroy(Cancelled);xrtFutureDestroy(Live);xrtFutureDestroy(Ready);roleClientClose(&P,C);
    }
}
static void roleFailures(void)
{
    const struct {cstr Response;xredisreadystatus Status;} Cases[]={
        {ROLE_REPLICA_REPLY,XREDIS_READY_ROLE_MISMATCH},{ROLE_SENTINEL_REPLY,XREDIS_READY_ROLE_MISMATCH},
        {"*3\r\n+master\r\n:-1\r\n*0\r\n",XREDIS_READY_PROTOCOL},{"*3\r\n+master\r\n:0\r\n*1\r\n*2\r\n+h\r\n+1\r\n",XREDIS_READY_PROTOCOL},{"+OK\r\n",XREDIS_READY_PROTOCOL},{"*0\r\n",XREDIS_READY_PROTOCOL},
        {"-NOPERM leaked-password\r\n",XREDIS_READY_SERVER_ERROR},{"-ERR unknown command 'role', with args beginning with: leaked-password\r\n",XREDIS_READY_UNSUPPORTED},
        {"-ERR unknown command 'ROLEX'\r\n",XREDIS_READY_SERVER_ERROR},{"-NOPROTO unsupported version\r\n",XREDIS_READY_SERVER_ERROR},{"-WRONGPASS leaked-password\r\n",XREDIS_READY_AUTH_FAILED}};
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){redisinitpeerstep Steps[]={{{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3")},2,XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),false,false},{{XRT_BYTES_LITERAL("ROLE")},1,{(cbytes)Cases[N].Response,strlen(Cases[N].Response)},false,false}};
        roleclientpeer P;roleClientPeerStart(&P,false,false,Steps,2);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=XREDIS_INIT_AUTO;Init.Flags=XREDIS_INIT_REQUIRE_MASTER;xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,0,NULL,&Init,&C),"role failure admission");xfuture* Ready=xredisClientReady(C);xfuture* Ping=roleClientPing(C,5000);const xredisreadyresult* R=roleClientWait(Ready);testRequire(R->Status==Cases[N].Status && R->Step==XREDIS_INIT_ROLE && !R->Hello && !R->Downgraded,"role typed failure without AUTO downgrade/HELLO leakage");
        if(R->Cause){testRequire(strstr(xrtErrorMessage(R->Cause),"leaked-password")==NULL,"redacted role diagnostics");}
        const xredisrequestresult* B=roleClientWait(Ping);testRequire(B->Status==XREDIS_REQUEST_INITIALIZATION && B->Delivery==XREDIS_NOT_SENT,"failed role blocks all queued business");xrtFutureDestroy(Ping);roleClientClose(&P,C);R=roleClientWait(Ready);testRequire(R->Status==Cases[N].Status && R->Step==XREDIS_INIT_ROLE,"retained role terminal status");xrtFutureDestroy(Ready);
    }
}
static void roleLimitsAndClose(void)
{
    redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,{NULL,0},true,false};
    for(unsigned Trial=0;Trial<4;++Trial){Step.Abort=Trial==3;Step.Response=Trial==2?(xbytesview)XRT_BYTES_LITERAL(ROLE_MASTER_REPLY):(xbytesview){NULL,0};roleclientpeer P;roleClientPeerStart(&P,false,false,&Step,1);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=XREDIS_INIT_RESP2;Init.Flags=XREDIS_INIT_REQUIRE_MASTER;Init.Timeout=Trial==0?200:5000;xredisclientconfig K;xredisClientConfigInit(&K);if(Trial==2)K.Replies.Reader.MaxValues=3;
        xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,0,&K,&Init,&C),"role interrupt admission");xfuture* Ready=xredisClientReady(C);xfuture* Ping=roleClientPing(C,5000);redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,1);
        if(Trial==1)testRequire(xredisClientClose(C),"close held role");
        if(Trial==2)redisPeerReleaseBlocked(&I->Peer);

        const xredisreadyresult* R=roleClientWait(Ready);testRequire(R->Status==(Trial==0?XREDIS_READY_TIMEOUT:Trial==1?XREDIS_READY_CLOSED:Trial==2?XREDIS_READY_LIMIT:XREDIS_READY_DISCONNECTED) && R->Step==XREDIS_INIT_ROLE,"role timeout/close/limit/EOF");testRequire(((const xredisrequestresult*)roleClientWait(Ping))->Delivery==XREDIS_NOT_SENT,"interrupted role business never sent");xrtFutureDestroy(Ping);xrtFutureDestroy(Ready);roleClientClose(&P,C);
    }
    roleclientpeer P;roleClientPeerStart(&P,false,false,NULL,0);xredisclientinitconfig Init;xredisClientInitConfigInit(&Init);Init.Mode=XREDIS_INIT_RESP2;
    const uint32 Bad[]={XREDIS_INIT_REQUIRE_MASTER|XREDIS_INIT_REQUIRE_REPLICA,XREDIS_INIT_REQUIRE_REPLICA|XREDIS_INIT_REQUIRE_SENTINEL,0x80000000u};for(unsigned N=0;N<3;++N){Init.Flags=Bad[N];xredisclient* Out=(xredisclient*)(uintptr_t)1;testRequire(!roleClientPeerConnect(&P,0,NULL,&Init,&Out) && Out==(xredisclient*)(uintptr_t)1,"exclusive/unknown role flags reject atomically");xrtClearError();}
    Init.Flags=XREDIS_INIT_REQUIRE_MASTER;Init.MaxWireBytes=13;xredisclient* Out=(xredisclient*)(uintptr_t)1;testRequire(!roleClientPeerConnect(&P,0,NULL,&Init,&Out) && Out==(xredisclient*)(uintptr_t)1,"14-byte ROLE participates in aggregate cap");xrtClearError();Init.MaxWireBytes=14;xredisclientconfig K;xredisClientConfigInit(&K);K.Stream.WriteLimit=13;testRequire(!roleClientPeerConnect(&P,0,&K,&Init,&Out),"ROLE obeys stream write cap");xrtClearError();roleClientPeerStop(&P);
}
static void roleOriginalDeadline(void)
{
    const redisinitpeerstep Steps[]={{{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3")},2,XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),true,false},{{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),true,false}};
    roleclientpeer P;roleClientPeerStart(&P,false,false,Steps,2);xredisclientinitconfig K;xredisClientInitConfigInit(&K);K.Flags=XREDIS_INIT_REQUIRE_MASTER;K.Timeout=1000;xredisclient* C=NULL;double Start=xrtTimer();testRequire(roleClientPeerConnect(&P,0,NULL,&K,&C),"original role deadline admission");xfuture* Ready=xredisClientReady(C);redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,1);
    while(xrtTimer()<Start+.65)xrtThreadYield();
    redisPeerReleaseBlocked(&I->Peer);redisInitPeerStepWait(I,2);const xredisreadyresult* R=roleClientWait(Ready);testRequire(R->Status==XREDIS_READY_TIMEOUT && R->Step==XREDIS_INIT_ROLE && !R->Hello && xrtTimer()-Start<1.4,"ROLE did not restart the original setup deadline");roleClientClose(&P,C);xrtFutureDestroy(Ready);
}
static void roleExactWire(void)
{
    const redisinitpeerstep Steps[]={{{XRT_BYTES_LITERAL("HELLO"),XRT_BYTES_LITERAL("3"),XRT_BYTES_LITERAL("AUTH"),XRT_BYTES_LITERAL("default"),XRT_BYTES_LITERAL("p")},5,XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY),false,false},{{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),true,false}};xbytesview Auth[]={XRT_BYTES_LITERAL("AUTH"),XRT_BYTES_LITERAL("p")};size_t HelloBytes=0,AuthBytes=0;testRequire(xredisCommandSize(Steps[0].Arguments,5,&HelloBytes) && xredisCommandSize(Auth,2,&AuthBytes),"role aggregate measurement");
    roleclientpeer P;roleClientPeerStart(&P,false,false,Steps,2);xredisclientinitconfig K;xredisClientInitConfigInit(&K);K.Mode=XREDIS_INIT_AUTO;K.Flags=XREDIS_INIT_AUTH|XREDIS_INIT_REQUIRE_MASTER;K.Password=(xbytesview)XRT_BYTES_LITERAL("p");K.MaxWireBytes=HelloBytes+AuthBytes+14-1;xredisclient* C=(xredisclient*)(uintptr_t)1;testRequire(!roleClientPeerConnect(&P,0,NULL,&K,&C) && C==(xredisclient*)(uintptr_t)1,"ROLE plus unused AUTO fallback are bounded together");xrtClearError();++K.MaxWireBytes;testRequire(roleClientPeerConnect(&P,0,NULL,&K,&C),"exact aggregate role cap");xfuture* Ready=xredisClientReady(C);redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,2);xredisclientinfo Info={0};Info.Size=sizeof(Info);testRequire(xredisClientInfo(C,&Info) && Info.InitBytes==K.MaxWireBytes,"all encoded role/fallback bytes are accounted while held");redisPeerReleaseBlocked(&I->Peer);roleReady(roleClientWait(Ready),XREDIS_RESP3,false);roleClientClose(&P,C);xrtFutureDestroy(Ready);
}
static void roleCreationQuotas(void)
{
    size_t Success=0,Failure=0;const redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),false,false};
    for(size_t Limit=1;Limit<25000;Limit+=521){roleclientpeer P;roleClientPeerStart(&P,false,false,&Step,1);redisinitpeer* I=roleClientPeerInit(&P);xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"role constructor quota");xredisclientinitconfig K;xredisClientInitConfigInit(&K);K.Mode=XREDIS_INIT_RESP2;K.Flags=XREDIS_INIT_REQUIRE_MASTER;xredisclient* C=(xredisclient*)(uintptr_t)1;bool OK=roleClientPeerConnect(&P,0,NULL,&K,&C);if(OK)++Success;else{++Failure;testRequire(C==(xredisclient*)(uintptr_t)1,"role constructor failure preserves output");}xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"role constructor quota leave");
        if(OK){xfuture* F=xredisClientReady(C);roleReady(roleClientWait(F),XREDIS_RESP2,false);roleClientClose(&P,C);xrtFutureDestroy(F);}else roleClientPeerStop(&P);
        (void)I;xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount,"role creation quota physically drained");xrtAllocationBudgetFree(B);
    }
    testRequire(Success && Failure,"role creation quota crosses boundary");
}
typedef struct rolequota {xallocationbudget* Budget;xallocationscope Scope;xatomic32 Stage;} rolequota;
static void roleQuotaEnter(xnetworker* W,ptr Data){(void)W;rolequota* Q=Data;testRequire(xrtAllocationBudgetEnter(Q->Budget,&Q->Scope),"role owner quota enter");xrtAtomic32Store(&Q->Stage,1,XMEMORY_RELEASE);}
static void roleQuotaLeave(xnetworker* W,ptr Data){(void)W;rolequota* Q=Data;xrtClearError();testRequire(xrtAllocationBudgetLeave(&Q->Scope),"role owner quota leave");xrtAtomic32Store(&Q->Stage,2,XMEMORY_RELEASE);}
static void roleWorkerOOM(void)
{
    const redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),true,false};roleclientpeer P;roleClientPeerStart(&P,false,false,&Step,1);xredisclientinitconfig K;xredisClientInitConfigInit(&K);K.Mode=XREDIS_INIT_RESP2;K.Flags=XREDIS_INIT_REQUIRE_MASTER;xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,0,NULL,&K,&C),"owner OOM connect");redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,1);rolequota Q={0};testRequire(xrtAllocationBudgetCreate(1,&Q.Budget) && xrtNetEnginePost(I->Peer.Engine,0,roleQuotaEnter,&Q),"owner OOM install");redisPeerWait(&Q.Stage,1);xfuture* F=xredisClientReady(C);redisPeerReleaseBlocked(&I->Peer);const xredisreadyresult* R=roleClientWait(F);testRequire(R->Status==XREDIS_READY_MEMORY && R->Step==XREDIS_INIT_ROLE && !R->Hello,"role reply/diagnostic OOM keeps typed phase/outcome");testRequire(xrtNetEnginePost(I->Peer.Engine,0,roleQuotaLeave,&Q),"owner quota leave post");redisPeerWait(&Q.Stage,2);roleClientClose(&P,C);xrtFutureDestroy(F);xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(Q.Budget,&Info) && !Info.LiveBytes && !Info.LiveCount && Info.Rejections,"owner quota covered and physically drained");xrtAllocationBudgetFree(Q.Budget);
}
typedef struct rolenote {xredisclient* Client;bool Success;xfuture* Child;xatomic32 Done;} rolenote;
static void roleNotified(ptr Data)
{
    rolenote* N=Data;if(N->Success)N->Child=roleClientPing(N->Client,5000);else{xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* F=(xfuture*)(uintptr_t)1;testRequire(xredisClientCallAsync(N->Client,&Ping,1,5000,&F)==XREDIS_ADMISSION_CLOSED && F==(xfuture*)(uintptr_t)1,"role failure closes admission before notification");}testRequire(xredisClientClose(N->Client),"role ready callback close");xrtAtomic32Store(&N->Done,1,XMEMORY_RELEASE);
}
static void roleReentry(void)
{
    for(unsigned N=0;N<2;++N){redisinitpeerstep Step={{XRT_BYTES_LITERAL("ROLE")},1,N?(xbytesview)XRT_BYTES_LITERAL(ROLE_REPLICA_REPLY):(xbytesview)XRT_BYTES_LITERAL(ROLE_MASTER_REPLY),true,false};roleclientpeer P;roleClientPeerStart(&P,false,false,&Step,1);xredisclientinitconfig K;xredisClientInitConfigInit(&K);K.Mode=XREDIS_INIT_RESP2;K.Flags=XREDIS_INIT_REQUIRE_MASTER;xredisclient* C=NULL;testRequire(roleClientPeerConnect(&P,0,NULL,&K,&C),"role reentry admission");xfuture* Ready=xredisClientReady(C);redisinitpeer* I=roleClientPeerInit(&P);redisInitPeerStepWait(I,1);rolenote Note={C,N==0,NULL,{0}};xfuturewatch Watch;testRequire(xrtFutureWatchInit(&Watch,roleNotified,NULL,&Note) && xrtFutureWatchAdd(Ready,&Watch)==XFUTURE_WATCH_PENDING,"role watcher registration");redisPeerReleaseBlocked(&I->Peer);redisPeerWait(&Note.Done,1);if(Note.Success){const xredisrequestresult* R=roleClientWait(Note.Child);testRequire(R->Status==XREDIS_REQUEST_CLOSED && R->Delivery==XREDIS_NOT_SENT,"reentrant close precedes business dispatch");xrtFutureDestroy(Note.Child);}roleClientClose(&P,C);xrtFutureDestroy(Ready);
    }
}
int main(void)
{roleSuccess();roleBarrier();roleFailures();roleLimitsAndClose();roleOriginalDeadline();roleExactWire();roleCreationQuotas();roleWorkerOOM();roleReentry();puts("[PASS] role-client: all identities, setup modes/factories, barriers, partial TLS, typed/redacted failures and physical cleanup");return 0;}
