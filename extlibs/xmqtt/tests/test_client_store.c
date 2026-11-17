#include "support/store_write_peer.h"
#include <xmqtt/store_session.h>
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
typedef struct cg_receive_peer {
    mqttExamplePeer Net;
    xmqttstorewrite* Writer;
    bool Resume;
    unsigned char Ack[8][5];
    size_t AckSize[8];
    uint64 AckTransaction[8];
    unsigned Count;
} cg_receive_peer;

static void cgReceivePeerRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    mqttExamplePeer* Net=Data;
    cg_receive_peer* Peer=(cg_receive_peer*)Net;
    size_t N=xrtNetBufSize(Buffer);
    testRequire(N<=sizeof(Net->Input)-Net->Used,"receive peer input bound");
    Net->Used+=xrtNetBufRead(Buffer,Net->Input+Net->Used,N);
    while(Net->Used){
        xmqttwireconfig K;
        xmqttWireConfigInit(&K);
        K.Version=Net->Version;
        xmqttframe F;
        xmqttwirestatus R=xmqttFrameRead((xbytesview){Net->Input,Net->Used},&K,&F);
        if(R==XMQTT_WIRE_MORE)break;
        testRequire(R==XMQTT_WIRE_DONE,"literal receive peer framing");
        if(F.Header.Type==XMQTT_PACKET_CONNECT){
            const unsigned char Ack[]={0x20,Net->Version==XMQTT_VERSION_5?3:2,Peer->Resume?1:0,0,0};
            testRequire(xrtNetStreamSend(S,Ack,(size_t)Ack[1]+2)==XNET_RESULT_OK,"literal CONNACK");
        }else{
            testRequire(Peer->Count<8 && F.Wire.Size<=5 &&
                        (F.Header.Type==XMQTT_PACKET_PUBACK||F.Header.Type==XMQTT_PACKET_PUBREC||F.Header.Type==XMQTT_PACKET_PUBCOMP),
                        "only expected acknowledgments leave the private Client");
            unsigned I=Peer->Count++;
            memcpy(Peer->Ack[I],F.Wire.Data,F.Wire.Size);
            Peer->AckSize[I]=F.Wire.Size;
            Peer->AckTransaction[I]=swInfo(Peer->Writer).State.Transactions;
            xrtAtomic32Store(&Net->Acks,Peer->Count,XMEMORY_RELEASE);
        }
        Net->Used-=F.Wire.Size;
        memmove(Net->Input,Net->Input+F.Wire.Size,Net->Used);
    }
}
static bool cgReceiveAccept(xnetlistener* L,xnetstream* S,ptr Data)
{
    (void)L;
    mqttExamplePeer* Net=Data;
    static const xnetstreamevents E={NULL,cgReceivePeerRead,mqttExampleEnd,NULL,NULL,NULL,mqttExampleClosed};
    testRequire(!xrtAtomicPtrLoad(&Net->Stream,XMEMORY_ACQUIRE)&&xrtNetStreamSetEvents(S,&E,Net),"receive peer stream");
    xrtAtomicPtrStore(&Net->Stream,S,XMEMORY_RELEASE);
    return true;
}
static void cgReceiveStart(cg_receive_peer* Peer,xmqttversion V,xmqttstorewrite* W,bool Resume)
{
    memset(Peer,0,sizeof(*Peer));
    Peer->Writer=W;
    Peer->Resume=Resume;
    mqttExamplePeer* Net=&Peer->Net;
    Net->Version=V;
    xnetengineconfig E;
    xrtNetEngineConfigInit(&E);
    E.Workers=2;
    Net->Engine=xrtNetEngineCreate(&E);
    testRequire(Net->Engine&&xrtNetEngineStart(Net->Engine),"receive Engine");
    xnetlistenconfig L;
    xrtNetListenConfigInit(&L);
    xrtNetAddrParse(&L.Address,"127.0.0.1",0);
    L.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents Events={cgReceiveAccept,NULL,mqttExampleListenerClosed};
    Net->Listener=xrtNetListen(Net->Engine,&L,&Events,NULL,Net);
    testRequire(Net->Listener&&xrtNetListenerLocal(Net->Listener,&Net->Address),"receive listener");
}
static void cgReceiveSend(cg_receive_peer* Peer,const unsigned char* Bytes,size_t Size)
{
    xnetstream* S=xrtAtomicPtrLoad(&Peer->Net.Stream,XMEMORY_ACQUIRE);
    testRequire(S&&xrtNetStreamSend(S,Bytes,Size)==XNET_RESULT_OK,"send independent literal packet");
}
static void cgReceiveAck(cg_receive_peer* Peer,unsigned Count,unsigned Type,unsigned Id,bool Unknown,uint64 Tx)
{
    double Deadline=xrtTimer()+10;
    while(xrtAtomic32Load(&Peer->Net.Acks,XMEMORY_ACQUIRE)<Count){
        testRequire(xrtTimer()<Deadline,"durable acknowledgment reached peer");
        xrtSleep(1);
    }
    bool Negative=Unknown&&Peer->Net.Version==XMQTT_VERSION_5;
    unsigned char Expected[]={Type,Negative?3:2,Id>>8,Id&255u,0x92};
    testRequire(Peer->AckSize[Count-1]==(Negative?5u:4u)&&
                !memcmp(Peer->Ack[Count-1],Expected,Peer->AckSize[Count-1])&&
                Peer->AckTransaction[Count-1]==Tx,"literal ACK bytes follow correct committed transaction");
}
static void cgReceiveNoAck(cg_receive_peer* Peer,unsigned Count)
{
    xrtSleep(50);
    testRequire(xrtAtomic32Load(&Peer->Net.Acks,XMEMORY_ACQUIRE)==Count,"held file barrier prevents any new ACK");
}
static void cgReceivePublish(cg_receive_peer* Peer,unsigned Qos,bool Duplicate,bool Alias,bool EmptyTopic)
{
    unsigned char B[16];
    size_t N=2;
    B[0]=(unsigned char)(0x30|(Qos<<1)|(Duplicate?8u:0u));
    B[N++]=0;
    B[N++]=EmptyTopic?0:1;
    if(!EmptyTopic)B[N++]='t';
    if(Qos){B[N++]=0;B[N++]=9;}
    if(Peer->Net.Version==XMQTT_VERSION_5){
        B[N++]=Alias?3:0;
        if(Alias){B[N++]=0x23;B[N++]=0;B[N++]=1;}
    }
    B[N++]='x';
    B[1]=(unsigned char)(N-2);
    cgReceiveSend(Peer,B,N);
}

typedef enum cg_receive_mode {CG_FLOW,CG_ACCEPT_CLOSE,CG_RELEASE_CLOSE,CG_WRITE_FAIL,CG_INBOX_LIMIT} cg_receive_mode;

static void cgReceiveCase(xmqttversion V,cg_receive_mode Mode)
{
    cgdrive D={0};
    str NativePath=NULL;
    if(Mode==CG_FLOW){
        D.IO.Base.Native=xrtFileTemp(NULL,"xmqtt-client-receive-",NULL,&NativePath);
        testRequire(D.IO.Base.Native!=NULL,"actual native receive journal");
    }
    jpfile File=cgFile(&D);
    xmqttjournalconfig JC=stJournalConfig(V);
    xmqttjournal* J=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(V);
    if(Mode==CG_INBOX_LIMIT)SC.MaxInbox=0;
    xmqttstorestate* Store=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"empty receive store");
    xtaskpool* Pool=jwPool(1);
    D.IO.Pool=Pool;
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* W=NULL;
    testRequire(xmqttStoreWriteCreate(Pool,J,Store,&WC,&W),"receive store writer");
    D.IO.Base.Calls=0;
    if(Mode==CG_FLOW||Mode==CG_ACCEPT_CLOSE)D.IO.Hold=14;
    if(Mode==CG_RELEASE_CLOSE)D.IO.Hold=21;
    if(Mode==CG_WRITE_FAIL)D.IO.Base.Fail=8;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=V;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"receive mirror Session");
    xmqttclientinboxconfig IK;
    xmqttClientInboxConfigInit(&IK);
    IK.Version=V;
    xmqttclientinbox* Inbox=NULL;
    testRequire(xmqttClientInboxCreate(&IK,&Inbox),"independent volatile Inbox");
    xmqttclientconfig Cfg;
    xmqttClientConfigInit(&Cfg);
    Cfg.Version=V;
    Cfg.MaxObjects=IK.MaxObjects;
    Cfg.MaxObjectBytes=IK.MaxObjectBytes;
    Cfg.MaxMessages=IK.MaxMessages;
    Cfg.MaxMessageBytes=IK.MaxMessageBytes;
    Cfg.WriteChunkBytes=1; /* Reuses the normal partial ACK send path. */
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SK.ClientId;
    xmqttproperty Props[2]={0};
    if(V==XMQTT_VERSION_5){
        Props[0]=(xmqttproperty){.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
        Props[1]=(xmqttproperty){.Id=XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM,.Type=XMQTT_PROPERTY_UINT16,.Value.Number=1};
        Connect.Properties=(xmqttpropertylist){Props,2};
    }
    cgclock Clock={0};
    xrtAtomic64Init(&Clock.Value,200000);
    xmqttclientstoreplan Plan={W,cgWall,&Clock};
    cg_receive_peer Peer;
    cgReceiveStart(&Peer,V,W,false);
    xmqttclient* C=NULL;
    testRequire(xmqttInternalClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,Inbox,&Connect,&Plan,&C),"live receive Client");
    xrtAtomicPtrStore(&D.Client,C,XMEMORY_RELEASE);
    xfuture* Ready=xmqttClientReady(C);
    testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"receive Client ready after OPEN");

    const unsigned char Pubrel[]={0x62,2,0,9};
    bool Alias=V==XMQTT_VERSION_5&&Mode==CG_FLOW;
    cgReceivePublish(&Peer,Mode==CG_FLOW?1:2,false,Alias,false);
    uint64 FinalTx=0;
    size_t FinalInbox=0,FinalIncoming=0;
    unsigned FinalAcks=0;
    if(Mode==CG_FLOW||Mode==CG_ACCEPT_CLOSE){
        jwAwait(&D.IO.Entered);
        xmqttsessioninfo Info;
        testRequire(xmqttSessionInfo(Session,&Info)&&Info.Incoming==0&&
                    Info.Prepared==(Mode==CG_ACCEPT_CLOSE?1u:0u)&&
                    swInfo(W).State.Inbox==0,"while acceptance sync is held only provisional receive credit exists");
        cgReceiveNoAck(&Peer,0);
        if(Mode==CG_ACCEPT_CLOSE){
            testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains in-flight acceptance");
        }
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
    }
    if(Mode==CG_FLOW){
        cgReceiveAck(&Peer,1,0x40,9,false,2);
        cgReceivePublish(&Peer,2,false,Alias,Alias);
        cgReceiveAck(&Peer,2,0x50,9,false,3);
        cgReceivePublish(&Peer,2,true,Alias,Alias);
        cgReceiveAck(&Peer,3,0x50,9,false,3);
        testRequire(swInfo(W).State.Inbox==2&&swInfo(W).State.Incoming==1,"duplicate QoS2 creates no new durable delivery");
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        cgReceiveAck(&Peer,4,0x70,9,false,4);
        testRequire(swInfo(W).State.Incoming==0&&swInfo(W).State.Inbox==2,"retired id keeps its unacked delivery");
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        cgReceiveAck(&Peer,5,0x70,9,true,4);
        cgReceivePublish(&Peer,2,false,Alias,Alias);
        cgReceiveAck(&Peer,6,0x50,9,false,5);
        cgReceivePublish(&Peer,0,false,Alias,Alias);
        double Deadline=xrtTimer()+10;
        while(swInfo(W).State.Transactions!=6){testRequire(xrtTimer()<Deadline,"QoS0 durable acceptance");xrtSleep(1);}
        FinalTx=7;FinalInbox=4;FinalIncoming=1;FinalAcks=6;
        testRequire(xmqttClientClose(C),"close after entire receive sequence");
    }else if(Mode==CG_RELEASE_CLOSE){
        cgReceiveAck(&Peer,1,0x50,9,false,2);
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        jwAwait(&D.IO.Entered);
        testRequire(swInfo(W).State.Incoming==1,"receive identity survives until retirement commit");
        cgReceiveNoAck(&Peer,1);
        testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains in-flight PUBREL retirement");
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
        FinalTx=4;FinalInbox=1;FinalAcks=1;
    }else if(Mode==CG_ACCEPT_CLOSE){
        FinalTx=3;FinalInbox=1;FinalIncoming=1;
    }else FinalTx=Mode==CG_WRITE_FAIL?1:2;

    testRequire(xmqttClientWaitFor(C,15000)==XWAIT_OK,"retire actual Client and all receive writes");
    xmqttclientinfo Closing;
    testRequire(xmqttClientInfo(C,&Closing),"receive close status");
    if(Mode==CG_WRITE_FAIL||Mode==CG_INBOX_LIMIT)
        testRequire(Closing.CloseStatus==(Mode==CG_WRITE_FAIL?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_LIMIT),"typed failed acceptance");
    xrtAtomicPtrStore(&D.Client,NULL,XMEMORY_RELEASE);
    testRequire(xmqttClientDestroy(C),"destroy receive Client");
    mqttExampleStop(&Peer.Net);
    testRequire(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)==FinalAcks,"no uncommitted or post-close ACK");
    xmqttmessage* Unchanged=(xmqttmessage*)(uintptr_t)1;
    testRequire(!xmqttClientInboxTake(Inbox,&Unchanged)&&Unchanged==(xmqttmessage*)(uintptr_t)1,"durable delivery stays out of volatile Inbox");
    xmqttsessioninfo SI;
    testRequire(xmqttSessionInfo(Session,&SI)&&!SI.Prepared&&SI.Incoming==FinalIncoming,"mirror has no provisional identity after retirement");
    testRequire(xmqttSessionDestroy(Session),"destroy original Session");
    testRequire(xmqttStoreWriteDestroy(W),"retire Writer before reopen");
    xrtFutureDestroy(Ready);

    D.IO.Hold=D.IO.Base.Fail=0;
    J=jpReopen(File,&JC);
    SC.WallMilliseconds=201000;
    Store=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"fresh receive replay");
    xmqttstorestateinfo Model=stInfo(Store);
    testRequire(Model.Transactions==FinalTx&&Model.Inbox==FinalInbox&&Model.Incoming==FinalIncoming,"recovery exactly matches expected ACK cut");
    for(size_t I=0;I<Model.Inbox;++I){
        xmqttstoreview Record=stView(Store,XMQTT_STORE_INBOX,I);
        const unsigned char Body5[]={0,1,'t',0,9,0,'x'};
        const unsigned char Body311[]={0,1,'t',0,9,'x'};
        const unsigned char BodyZero5[]={0,1,'t',0,'x'};
        const unsigned char BodyZero311[]={0,1,'t','x'};
        bool Zero=Mode==CG_FLOW&&I==3;
        const unsigned char* Body=V==XMQTT_VERSION_5?(Zero?BodyZero5:Body5):(Zero?BodyZero311:Body311);
        size_t Bytes=(V==XMQTT_VERSION_5?sizeof(Body5):sizeof(Body311))-(Zero?2u:0u);
        testRequire(Record.Key==I+1&&Record.Wire.Size==Bytes+2&&
                    !memcmp(Record.Wire.Data+2,Body,Bytes),"recovered payload has full topic and no connection-only alias");
    }
    if(Mode==CG_ACCEPT_CLOSE||Mode==CG_RELEASE_CLOSE){
        xmqttstoresessionconfig Import;
        xmqttStoreSessionConfigInit(&Import);
        Import.Binding=SC.Binding;
        Import.Generation=SC.Generation;
        Import.WallMilliseconds=SC.WallMilliseconds;
        Import.MonotonicTime=xrtTimer();
        Session=NULL;
        testRequire(xmqttStoreSessionRestore(Store,&SK,&Import,&Session)==XMQTT_SESSION_DONE,"import true committed receive identity");
        W=NULL;
        testRequire(xmqttStoreWriteCreate(Pool,J,Store,&WC,&W),"reopened Writer");
        Plan.Writer=W;
        xrtAtomic64Store(&Clock.Value,201000,XMEMORY_RELEASE);
        cgReceiveStart(&Peer,V,W,true);
        C=NULL;
        testRequire(xmqttInternalClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,Inbox,&Connect,&Plan,&C),"resume actual stored session");
        Ready=xmqttClientReady(C);
        testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"actual resumed CONNACK");
        if(Mode==CG_ACCEPT_CLOSE){
            cgReceivePublish(&Peer,2,true,false,false);
            cgReceiveAck(&Peer,1,0x50,9,false,FinalTx+1);
            testRequire(swInfo(W).State.Inbox==1,"replayed duplicate delivers no additional payload");
            cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
            cgReceiveAck(&Peer,2,0x70,9,false,FinalTx+2);
        }else{
            cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
            cgReceiveAck(&Peer,1,0x70,9,true,FinalTx+1);
            cgReceivePublish(&Peer,2,false,false,false);
            cgReceiveAck(&Peer,2,0x50,9,false,FinalTx+2);
        }
        testRequire(xmqttClientDestroy(C),"retire resumed Client");
        mqttExampleStop(&Peer.Net);
        xrtFutureDestroy(Ready);
        testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(W),"release resumed owners");
    }else {
        xmqttStoreStateDestroy(Store);
        testRequire(xmqttJournalDestroy(J),"release fresh read-only Journal");
    }
    testRequire(xmqttClientInboxDestroy(Inbox),"release unused volatile Inbox");
    jwPoolDone(Pool);
    jpFileClose(File);
    if(NativePath){
        testRequire(xrtClose(D.IO.Base.Native)&&xrtFileDelete(NativePath),"retire only the test's native receive journal");
        xrtFree(NativePath);
    }
}


int main(void)
{
    for(unsigned V=0;V<2;++V)for(unsigned Mode=0;Mode<8;++Mode)cgCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode);
    for(unsigned V=0;V<2;++V)
        for(cg_receive_mode Mode=CG_FLOW;Mode<=CG_INBOX_LIMIT;++Mode)
            cgReceiveCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode);
    puts("[PASS] live receive persists before ACK, deduplicates across actual reconnect, retires QoS2 before PUBCOMP and recovers canonical delivery");
    puts("[PASS] actual TCP CONNACK waits for durable BIND/OPEN; late Close/deadline retain IO and END duties,both MQTT levels; storage/clock refusal and exclusive Writer ownership");return 0;
}
