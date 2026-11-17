#include "support/store_write_peer.h"
#include "../src/internal/xmqtt_client_store.h"
#include "../examples/common/client_peer.h"
typedef struct cgdrive {jwdrive IO;xatomicptr Client;xatomic32 Checked;} cgdrive;
static bool cgFlush(void* State)
{
    cgdrive* D=State;xmqttclient* C=xrtAtomicPtrLoad(&D->Client,XMEMORY_ACQUIRE);
    if(C){testRequire(xrtTaskPoolIsCurrentWorkerV1(D->IO.Pool),"actual journal sync remains on storage host pool");
        testRequire(xmqttClientWaitFor(C,0)==XWAIT_ERROR&&!xmqttClientDestroy(C),"same-pool Client joins refused before blocking");xrtClearError();xrtAtomic32FetchAdd(&D->Checked,1,XMEMORY_RELEASE);}
    return jwFlush(State);
}
static const xvfsfileops_v1 cgOps={sizeof(xvfsfileops_v1),XRT_VFS_FILE_OPS_VERSION,XVFS_FILE_READ_AT|XVFS_FILE_WRITE_AT|XVFS_FILE_STAT|XVFS_FILE_RESIZE|XVFS_FILE_FLUSH,NULL,NULL,jpRead,jwWrite,NULL,jpStat,jpResize,cgFlush,jpClose};
static xvfslookup cgOpen(void* C,xvfscase Case,xstrview Path,const xfileoptions* O,xvfsfile_v1* F)
{(void)Case;(void)Path;*F=(xvfsfile_v1){&cgOps,C,O->Flags,0};return XVFS_LOOKUP_OPENED;}
static const xvfsprovider_v1 cgProvider={sizeof(xvfsprovider_v1),XRT_VFS_PROVIDER_VERSION,0,jpRetain,jpRelease,cgOpen,NULL,NULL,NULL};
static jpfile cgFile(cgdrive* D)
{
    jpfile F={0};F.Vfs=xrtVfsCreate();testRequire(F.Vfs!=NULL,"real Core VFS");F.Mount=xrtVfsMount(F.Vfs,"/",0,XVFS_CASE_SENSITIVE,&cgProvider,D,0);testRequire(F.Mount!=NULL,"storage provider");
    xfileoptions O;xrtFileOptionsInit(&O);O.Flags=XFILE_READ|XFILE_WRITE;F.File=xrtVfsOpen(F.Vfs,"/journal",&O);testRequire(F.File!=NULL,"owned file");return F;
}
typedef struct cgclock {xatomic64 Value;xallocationbudget* Budget;xallocationscope Scope;bool Arm,Scoped;} cgclock;
static void cgDiagnostic(xmqttclient* C,ptr User)
{
    cgclock* Clock=User;if(Clock&&Clock->Scoped){testRequire(xrtAllocationBudgetLeave(&Clock->Scope),"leave actual IO-worker allocation budget");Clock->Scoped=false;}
    xmqttclientinfo I;xmqttClientInfo(C,&I);if(I.CloseStatus==XMQTT_STATUS_INITIALIZATION)fprintf(stderr,"bootstrap diagnostic: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
}
static bool cgWall(ptr P,uint64* Out)
{
    cgclock* Clock=P;if(Clock->Arm){Clock->Arm=false;testRequire(xrtAllocationBudgetEnter(Clock->Budget,&Clock->Scope),"real IO-worker allocation scope before snapshot");Clock->Scoped=true;}
    *Out=xrtAtomic64Load(&Clock->Value,XMEMORY_ACQUIRE);return true;
}
typedef struct cgwatch {xfuturewatch Watch;xmqttstorewrite* Writer;xatomic32 Entered,Released;} cgwatch;
static void cgObserve(ptr Data)
{
    cgwatch* O=Data;testRequire(swInfo(O->Writer).State.Transactions==1,"synchronous Ready observer sees complete committed OPEN");
    xrtAtomic32Store(&O->Entered,1,XMEMORY_RELEASE);jwAwait(&O->Released);
}
static void cgObserved(ptr Data){(void)Data;}
static const xmqttreadyresult* cgReady(xfuture* F)
{testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"typed connection outcome");return xrtFutureValue(F);}
static void cgCase(xmqttversion V,unsigned Mode)
{
    bool Disk=Mode==5;if(Disk)Mode=0;bool Observe=Mode==0;
    cgdrive D={0};str NativePath=NULL;if(Disk){D.IO.Base.Native=xrtFileTemp(NULL,"xmqtt-client-store-",NULL,&NativePath);testRequire(D.IO.Base.Native!=NULL,"actual native journal file");}
    jpfile File=cgFile(&D);xmqttjournalconfig JC=stJournalConfig(V);xmqttjournal* J=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(V);SC.WallMilliseconds=Mode==4?300000:0;xmqttstorestate* S=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&S),XMQTT_STORE_DONE,"empty prefix");
    xtaskpool* Pool=jwPool(1);D.IO.Pool=Pool;xmqttstorewriteconfig WC;xmqttStoreWriteConfigInit(&WC);xmqttstorewrite* W=NULL;testRequire(xmqttStoreWriteCreate(Pool,J,S,&WC,&W),"exclusive storage writer");
    D.IO.Base.Calls=0;if(Mode<3)D.IO.Hold=7;else if(Mode==3)D.IO.Base.Fail=1;
    xmqttsessionconfig SK;xmqttSessionConfigInit(&SK);SK.Version=V;SK.ClientId=XRT_STR_LITERAL("delta-client");xmqttsession* Session=NULL;testRequire(xmqttSessionCreate(&SK,&Session),"passive native Session");
    xmqttclientinboxconfig IK;xmqttClientInboxConfigInit(&IK);IK.Version=V;xmqttclientinbox* Inbox=NULL;testRequire(xmqttClientInboxCreate(&IK,&Inbox),"independent Inbox");
    mqttExamplePeer Peer;mqttExampleStart(&Peer,V);xmqttclientconfig Cfg;xmqttClientConfigInit(&Cfg);Cfg.Version=V;Cfg.MaxObjects=2;Cfg.Notify=cgDiagnostic;if(Mode==2)Cfg.SetupTimeout=500;
    /* Same object caps are required by the ordinary Client Inbox factory. */
    IK.MaxObjects=2;testRequire(xmqttClientInboxDestroy(Inbox)&&xmqttClientInboxCreate(&IK,&Inbox),"minimal ready+CONNACK object budget");
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=SK.ClientId;Q.Body.Connect.CleanStart=false;xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Expiry,1};
    cgclock Clock={0};xrtAtomic64Init(&Clock.Value,200000);if(Mode==7){testRequire(xrtAllocationBudgetCreate(1,&Clock.Budget),"real tiny native allocation budget");Clock.Arm=true;}
    if(Mode==6)testRequire(xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK,"stopped host pool before connection");
    Cfg.User=&Clock;xmqttclientstoreplan Plan={W,cgWall,&Clock};xmqttclient* C=NULL;
    testRequire(xmqttInternalClientConnectStore(Peer.Engine,&Peer.Address,1,&Cfg,Session,Inbox,&Q,&Plan,&C),"actual private numeric Client factory");xrtAtomicPtrStore(&D.Client,C,XMEMORY_RELEASE);xfuture* Ready=xmqttClientReady(C);
    cgwatch Observer={0};Observer.Writer=W;
    if(Observe){testRequire(xrtFutureWatchInit(&Observer.Watch,cgObserve,cgObserved,&Observer)&&xrtFutureWatchAdd(Ready,&Observer.Watch)==XFUTURE_WATCH_PENDING,"ready observer registered before held final sync");}
    if(Mode<3){
        double Await=xrtTimer()+8;
        while(!xrtAtomic32Load(&D.IO.Entered,XMEMORY_ACQUIRE)){
            if(xrtFutureState(Ready)!=XFUTURE_PENDING||xrtTimer()>=Await){const xmqttreadyresult* E=xrtFutureValue(Ready);xmqttclientinfo X;xmqttClientInfo(C,&X);fprintf(stderr,"case version%d mode%u before barrier: status%d calls%u state%d ready%d cause%s\n",V,Mode,E?(int)E->Status:-1,D.IO.Base.Calls,X.State,X.Ready,E&&E->Cause?xrtErrorMessage(E->Cause):"none");testRequire(false,"bootstrap reaches actual storage barrier");}xrtSleep(1);
        }
        xmqttclientinfo Info;xmqttClientInfo(C,&Info);xmqttsessioninfo SI;xmqttSessionInfo(Session,&SI);
        testRequire(!Info.Ready&&!SI.Connected&&swInfo(W).State.Transactions==0&&xrtFutureState(Ready)==XFUTURE_PENDING,"Ready and mutable Session remain behind real final sync");
        xfuture* Unchanged=(xfuture*)(uintptr_t)1;
        testRequire(xmqttStoreWriteAppendAsync(W,XRT_BYTES_LITERAL("x"),0,&Unchanged)==XMQTT_JWORK_ERROR&&Unchanged==(xfuture*)(uintptr_t)1&&!xmqttStoreWriteClose(W)&&!xmqttStoreWriteDestroy(W),"live Client exclusively fences all Writer mutations/destruction");xrtClearError();
        if(Mode==1){testRequire(xmqttClientClose(C),"close during actual commit");testRequire(cgReady(Ready)->Status==XMQTT_STATUS_CLOSED,"logical Close wins readiness");}
        if(Mode==2)testRequire(cgReady(Ready)->Status==XMQTT_STATUS_TIMEOUT,"original setup deadline includes held storage");
        if(Mode) testRequire(xmqttClientPollRetired(C)==XWAIT_TIMEOUT&&xmqttClientWaitFor(C,0)==XWAIT_TIMEOUT,"logical close cannot retire physical storage duty");
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
    }
    if(!Mode){
        testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK&&swInfo(W).State.Transactions==1&&swInfo(W).State.LiveAtCut,"successful Ready observes committed broker OPEN");
        xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);P.Body.Publish.Topic=XRT_STR_LITERAL("unexposed");xfuture* F=(xfuture*)(uintptr_t)1;
        testRequire(xmqttClientPublishAsync(C,&P,1000,&F)==XMQTT_ADMISSION_ERROR&&F==(xfuture*)(uintptr_t)1,"unimplemented durable flow is never exposed as success");xrtClearError();
        if(Observe)jwAwait(&Observer.Entered);
        xrtAtomic64Store(&Clock.Value,200100,XMEMORY_RELEASE);testRequire(xmqttClientClose(C),"close live native connection");
        if(Observe){testRequire(xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"blocked synchronous Ready observer retains actual Client duty");xrtAtomic32Store(&Observer.Released,1,XMEMORY_RELEASE);}
    }else if(Mode==3)testRequire(cgReady(Ready)->Status==XMQTT_STATUS_INITIALIZATION,"native storage refusal prevents readiness");
    else if(Mode==4)testRequire(cgReady(Ready)->Status==XMQTT_STATUS_SESSION,"trusted-clock rollback rejected before storage");
    else if(Mode==6)testRequire(cgReady(Ready)->Status==XMQTT_STATUS_INITIALIZATION,"real rejected host submission rolls back Future duty");
    else if(Mode==7)testRequire(cgReady(Ready)->Status==XMQTT_STATUS_MEMORY,"actual snapshot allocation failure before commit");
    testRequire(xmqttClientWaitFor(C,10000)==XWAIT_OK,"physical Client,Watch and END retirement");xrtAtomicPtrStore(&D.Client,NULL,XMEMORY_RELEASE);
    testRequire(xmqttClientDestroy(C),"unique Client destruction releases Writer claim");mqttExampleStop(&Peer);
    xmqttstorewriteinfo State=swInfo(W);testRequire(State.State.Transactions==(Mode<3?2:0),"late actual OPEN commits are followed by END; refused OPEN changes no model");
    if(Mode<3)testRequire(!State.State.LiveAtCut&&xrtAtomic32Load(&D.Checked,XMEMORY_ACQUIRE)>=2,"actual native close persisted and same-pool joins checked");
    unsigned Calls=D.IO.Base.Calls;testRequire(xmqttStoreWriteDestroy(W),"caller retains Writer lifetime after Client");jwPoolDone(Pool);
    D.IO.Base.Fail=0;J=jpReopen(File,&JC);SC.WallMilliseconds=300000;S=NULL;stStatus(xmqttStoreStateRecover(J,&SC,&S),XMQTT_STORE_DONE,"fresh typed full-prefix replay");testRequire(stInfo(S).Transactions==(Mode<3?2:0),"physical history agrees with online committed state");
    if(Mode>=4)testRequire(Calls==0,"preflight/host/OOM refusal performs no file IO");
    xmqttStoreStateDestroy(S);testRequire(xmqttJournalDestroy(J)&&xmqttClientInboxDestroy(Inbox)&&xmqttSessionDestroy(Session),"release independent native owners");jpFileClose(File);xrtFutureDestroy(Ready);
    if(Disk){testRequire(xrtClose(D.IO.Base.Native)&&xrtFileDelete(NativePath),"close/delete own native fixture");xrtFree(NativePath);}
    if(Clock.Budget){xallocationbudgetinfo B={.Size=sizeof(B)};testRequire(xrtAllocationBudgetInfo(Clock.Budget,&B)&&B.Rejections&&B.LiveBytes==0,"actual allocation rejection and full requested-byte refund");xrtAllocationBudgetFree(Clock.Budget);}
}
int main(void)
{
    for(unsigned V=0;V<2;++V)for(unsigned Mode=0;Mode<8;++Mode)cgCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode);
    puts("[PASS] actual TCP CONNACK waits for durable BIND/OPEN; late Close/deadline retain IO and END duties,both MQTT levels; storage/clock refusal and exclusive Writer ownership");return 0;
}
