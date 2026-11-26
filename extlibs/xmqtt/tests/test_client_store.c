#include "support/store_write_peer.h"
#include <xmqtt/store_session.h>
#include <xmqtt/store_write.h>
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
    mqttExamplePeer Peer;mqttExampleStart(&Peer,V);xmqttclientconfig Cfg;xmqttClientConfigInit(&Cfg);Cfg.Version=V;Cfg.MaxObjects=2;Cfg.Notify=cgDiagnostic;if(Mode==2)Cfg.SetupTimeout=500;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_CONNECT,&Q);Q.Body.Connect.ClientId=SK.ClientId;Q.Body.Connect.CleanStart=false;xmqttproperty Expiry={0};Expiry.Id=XMQTT_PROPERTY_SESSION_EXPIRY;Expiry.Type=XMQTT_PROPERTY_UINT32;Expiry.Value.Number=60;if(V==XMQTT_VERSION_5)Q.Properties=(xmqttpropertylist){&Expiry,1};
    cgclock Clock={0};xrtAtomic64Init(&Clock.Value,200000);if(Mode==7){testRequire(xrtAllocationBudgetCreate(1,&Clock.Budget),"real tiny native allocation budget");Clock.Arm=true;}
    if(Mode==6)testRequire(xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK,"stopped host pool before connection");
    Cfg.User=&Clock;xmqttclientstoreconfig Plan={sizeof(Plan),W,cgWall,&Clock};xmqttclient* C=NULL;
    testRequire(xmqttClientConnectStore(Peer.Engine,&Peer.Address,1,&Cfg,Session,&Plan,&Q,&C),"actual private numeric Client factory");xrtAtomicPtrStore(&D.Client,C,XMEMORY_RELEASE);xfuture* Ready=xmqttClientReady(C);
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
        xmqttpacket P;xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&P);xfuture* F=(xfuture*)(uintptr_t)1;
        testRequire(xmqttClientSubscribeAsync(C,&P,1000,&F)==XMQTT_ADMISSION_ERROR&&F==(xfuture*)(uintptr_t)1,
                    "normal codec rejects an empty subscription");xrtClearError();
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
    xmqttStoreStateDestroy(S);testRequire(xmqttJournalDestroy(J)&&xmqttSessionDestroy(Session),"release independent native owners");jpFileClose(File);xrtFutureDestroy(Ready);
    if(Disk){testRequire(xrtClose(D.IO.Base.Native)&&xrtFileDelete(NativePath),"close/delete own native fixture");xrtFree(NativePath);}
    if(Clock.Budget){xallocationbudgetinfo B={.Size=sizeof(B)};testRequire(xrtAllocationBudgetInfo(Clock.Budget,&B)&&B.Rejections&&B.LiveBytes==0,"actual allocation rejection and full requested-byte refund");xrtAllocationBudgetFree(Clock.Budget);}
}
typedef struct cg_receive_peer {
    mqttExamplePeer Net;
    xmqttstorewrite* Writer;
    bool Resume,CapturePublish,EarlyPuback,Alias,Control,ZeroExpiry;
    unsigned char Ack[8][32];
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
            unsigned char Ack[18]={0x20,Net->Version==XMQTT_VERSION_5?3:2,Peer->Resume?1:0,0,0};
            if(Peer->Alias){
                Ack[1]=6;Ack[4]=3;Ack[5]=0x22;Ack[6]=0;Ack[7]=1;
            }
            if(Peer->ZeroExpiry){
                Ack[1]=8;Ack[4]=5;Ack[5]=0x11;
            }
            size_t Size=Ack[1]+2u;
            if(Peer->EarlyPuback){
                const unsigned char P[]={0x40,2,0,9};
                memcpy(Ack+Size,P,sizeof(P));Size+=sizeof(P);
            }
            testRequire(xrtNetStreamSend(S,Ack,Size)==XNET_RESULT_OK,"literal CONNACK and optional coalesced PUBACK");
        }else{
            bool Expected=Peer->CapturePublish
                ? F.Header.Type==XMQTT_PACKET_PUBLISH||F.Header.Type==XMQTT_PACKET_PUBREL
                : F.Header.Type==XMQTT_PACKET_PUBACK||F.Header.Type==XMQTT_PACKET_PUBREC||F.Header.Type==XMQTT_PACKET_PUBCOMP;
            bool Control=Peer->Control&&(F.Header.Type==XMQTT_PACKET_SUBSCRIBE||
                F.Header.Type==XMQTT_PACKET_UNSUBSCRIBE||F.Header.Type==XMQTT_PACKET_DISCONNECT);
            testRequire(Peer->Count<8&&F.Wire.Size<=sizeof(Peer->Ack[0])&&(Expected||Control),
                        "only expected frames leave the private Client");
            unsigned I=Peer->Count++;
            memcpy(Peer->Ack[I],F.Wire.Data,F.Wire.Size);
            Peer->AckSize[I]=F.Wire.Size;
            Peer->AckTransaction[I]=swInfo(Peer->Writer).State.Transactions;
            xrtAtomic32Store(&Net->Acks,Peer->Count,XMEMORY_RELEASE);
            if(Control && F.Header.Type!=XMQTT_PACKET_DISCONNECT){
                bool Remove=F.Header.Type==XMQTT_PACKET_UNSUBSCRIBE;
                unsigned char Ack[]={Remove?0xb0:0x90,4,F.Wire.Data[2],F.Wire.Data[3],0,0};
                if(Net->Version==XMQTT_VERSION_311)Ack[1]=Remove?2:3;
                testRequire(xrtNetStreamSend(S,Ack,(size_t)Ack[1]+2)==XNET_RESULT_OK,"literal subscription result");
            }
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
enum {CG_RESUME=1,CG_CAPTURE_PUBLISH=2,CG_EARLY_PUBACK=4,CG_ALIAS=8,CG_CONTROL=16,CG_ZERO_EXPIRY=32};
static void cgPeerStart(cg_receive_peer* Peer,xmqttversion V,xmqttstorewrite* W,unsigned Flags)
{
    memset(Peer,0,sizeof(*Peer));
    Peer->Writer=W;
    Peer->Resume=(Flags&CG_RESUME)!=0;
    Peer->CapturePublish=(Flags&CG_CAPTURE_PUBLISH)!=0;
    Peer->EarlyPuback=(Flags&CG_EARLY_PUBACK)!=0;
    Peer->Alias=(Flags&CG_ALIAS)!=0;
    Peer->Control=(Flags&CG_CONTROL)!=0;
    Peer->ZeroExpiry=(Flags&CG_ZERO_EXPIRY)!=0;
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
static void cgReceiveStart(cg_receive_peer* Peer,xmqttversion V,xmqttstorewrite* W,bool Resume)
{cgPeerStart(Peer,V,W,Resume?CG_RESUME:0);}
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

typedef enum cg_receive_mode {
    CG_FLOW,CG_ACCEPT_CLOSE,CG_RELEASE_CLOSE,CG_WRITE_FAIL,CG_INBOX_LIMIT
    ,CG_LIVE_LEASE
} cg_receive_mode;

static void cgTakeProtocolMessage(xmqttclient* C,unsigned Qos)
{
    xmqttmessage* Message=NULL;
    double Deadline=xrtTimer()+10;
    while(!xmqttClientTakeMessage(C,&Message)){
        testRequire(xrtTimer()<Deadline,"bounded APP delivery");
        xrtSleep(1);
    }
    const xmqttpacket* P=xmqttMessagePacket(Message);
    testRequire(P&&P->Type==XMQTT_PACKET_PUBLISH&&((P->Flags>>1)&3u)==Qos&&
        P->Identifier==(Qos?9:0)&&P->Body.Publish.Topic.Size==1&&P->Body.Publish.Topic.Data[0]=='t'&&
        P->Body.Publish.Payload.Size==1&&P->Body.Publish.Payload.Data[0]=='x',"APP receives the full resolved message");
    xmqttMessageRelease(Message);
}

static void cgReceiveCase(xmqttversion V,cg_receive_mode Mode,bool ProtocolOnly)
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
    if(Mode==CG_INBOX_LIMIT||ProtocolOnly)SC.MaxInbox=0;
    xmqttstorestate* Store=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"empty receive store");
    xtaskpool* Pool=jwPool(1);
    D.IO.Pool=Pool;
    xmqttstorewriteconfig WC;
    testRequire(xmqttStoreWriteConfigInit(&WC)&&WC.ProtocolOnly,"protocol-only Writer default");
    if(!ProtocolOnly)WC.ProtocolOnly=false;
    xmqttstorewrite* W=NULL;
        testRequire(xmqttStoreWriteCreate(Pool,J,Store,&WC,&W),"receive store writer");
    D.IO.Base.Calls=0;
    if(Mode==CG_FLOW||Mode==CG_ACCEPT_CLOSE)D.IO.Hold=14;
    if(Mode==CG_RELEASE_CLOSE)D.IO.Hold=21;
    if(Mode==CG_LIVE_LEASE)D.IO.Hold=21;
    if(Mode==CG_WRITE_FAIL)D.IO.Base.Fail=8;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=V;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"receive mirror Session");
    xmqttclientconfig Cfg;
    xmqttClientConfigInit(&Cfg);
    Cfg.Version=V;
    if(ProtocolOnly&&Mode==CG_INBOX_LIMIT)Cfg.MaxMessageBytes=1;
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
    xmqttclientstoreconfig Plan={sizeof(Plan),W,cgWall,&Clock};
    cg_receive_peer Peer;
    cgReceiveStart(&Peer,V,W,false);
    xmqttclient* C=NULL;
    testRequire(xmqttClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,&Plan,&Connect,&C),"live receive Client");
    xrtAtomicPtrStore(&D.Client,C,XMEMORY_RELEASE);
    xfuture* Ready=xmqttClientReady(C);
    testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"receive Client ready after OPEN");

    const unsigned char Pubrel[]={0x62,2,0,9};
    bool Alias=V==XMQTT_VERSION_5&&Mode==CG_FLOW;
    cgReceivePublish(&Peer,Mode==CG_FLOW?1:2,false,Alias,false);
    if(ProtocolOnly&&Mode==CG_FLOW){
        cgReceiveAck(&Peer,1,0x40,9,false,1);
        cgTakeProtocolMessage(C,1);
        cgReceivePublish(&Peer,2,false,Alias,Alias);
    }
    uint64 FinalTx=0;
    size_t FinalInbox=0,FinalIncoming=0;
    unsigned FinalAcks=0;
    if(Mode==CG_FLOW||Mode==CG_ACCEPT_CLOSE){
        jwAwait(&D.IO.Entered);
        xmqttsessioninfo Info;
        testRequire(xmqttSessionInfo(Session,&Info)&&Info.Incoming==0&&
                     Info.Prepared==(Mode==CG_ACCEPT_CLOSE||ProtocolOnly?1u:0u)&&
                    swInfo(W).State.Inbox==0,"while acceptance sync is held only provisional receive credit exists");
        cgReceiveNoAck(&Peer,ProtocolOnly&&Mode==CG_FLOW?1:0);
        xmqttmessage* Early=NULL;
        testRequire(!xmqttClientTakeMessage(C,&Early),"no delivery before durable acceptance");
        if(Mode==CG_ACCEPT_CLOSE){
            testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains in-flight acceptance");
        }
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
    }
    if(Mode==CG_FLOW){
        if(!ProtocolOnly){
            cgReceiveAck(&Peer,1,0x40,9,false,2);
            cgReceivePublish(&Peer,2,false,Alias,Alias);
        }
        uint64 Tx=ProtocolOnly?2:3;
        size_t Inbox=ProtocolOnly?0:2;
        cgReceiveAck(&Peer,2,0x50,9,false,Tx);
        if(ProtocolOnly)cgTakeProtocolMessage(C,2);
        cgReceivePublish(&Peer,2,true,Alias,Alias);
        cgReceiveAck(&Peer,3,0x50,9,false,Tx);
        testRequire(swInfo(W).State.Inbox==Inbox&&swInfo(W).State.Incoming==1,"duplicate QoS2 creates no new durable delivery");
        xmqttmessage* Duplicate=NULL;
        testRequire(!xmqttClientTakeMessage(C,&Duplicate),"duplicate suppressed in APP queue");
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        cgReceiveAck(&Peer,4,0x70,9,false,++Tx);
        testRequire(swInfo(W).State.Incoming==0&&swInfo(W).State.Inbox==Inbox,"retired id frees protocol tracking");
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        cgReceiveAck(&Peer,5,0x70,9,true,Tx);
        cgReceivePublish(&Peer,2,false,Alias,Alias);
        cgReceiveAck(&Peer,6,0x50,9,false,++Tx);
        if(ProtocolOnly)cgTakeProtocolMessage(C,2);
        cgReceivePublish(&Peer,0,false,Alias,Alias);
        if(ProtocolOnly)cgTakeProtocolMessage(C,0);
        else {
            double Deadline=xrtTimer()+10;
            ++Tx;
            while(swInfo(W).State.Transactions!=Tx){testRequire(xrtTimer()<Deadline,"QoS0 durable acceptance");xrtSleep(1);}
        }
        FinalTx=Tx+1;FinalInbox=ProtocolOnly?0:4;FinalIncoming=1;FinalAcks=6;
        testRequire(xmqttClientClose(C),"close after entire receive sequence");
    }else if(Mode==CG_LIVE_LEASE){
        cgReceiveAck(&Peer,1,0x50,9,false,2);

        xmqttstoreview View;
        unsigned char Buffer[64];
        size_t Bytes=0;
        stStatus(xmqttStoreWriteReadPending(W,Buffer,sizeof(Buffer),&View,&Bytes),XMQTT_STORE_DONE,
                 "lease acquired while actual TCP Client owns Writer");
        testRequire(View.Key==1&&View.Qos==2&&View.Wire.Data==Buffer&&Bytes==View.Wire.Size,"live delivery snapshot");
        xfuture* Ack=NULL;
        testRequire(xmqttStoreWriteRetireAsync(W,View.Epoch,View.Key,10000,&Ack)==XMQTT_JWORK_ACCEPTED,"application ACK admitted alongside Client");
        jwAwait(&D.IO.Entered);
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        double Deadline=xrtTimer()+10;
        while(!swInfo(W).Worker.Queued){
            testRequire(xrtTimer()<Deadline,"PUBREL transaction queues concurrently with in-flight lease ACK");
            xrtSleep(1);
        }
        testRequire(swInfo(W).State.Inbox==1&&swInfo(W).State.Incoming==1,"staged ACK has no visible effect");
        cgReceiveNoAck(&Peer,1);
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
        const xmqttcommitresult* R=jwOutcome(Ack);
        testRequire(R->Status==XMQTT_COMMIT_OK&&R->Sequence==3&&R->Effect==XMQTT_JOURNAL_COMMITTED,"application ACK commits first");
        cgReceiveAck(&Peer,2,0x70,9,false,4);
        testRequire(!swInfo(W).State.Inbox&&!swInfo(W).State.Incoming,"protocol retirement leaves no delivery or receive identity");
        xrtFutureDestroy(Ack);
        testRequire(xmqttClientClose(C),"close after independent app and protocol ACKs");
        FinalTx=5;FinalAcks=2;
    }else if(Mode==CG_RELEASE_CLOSE){
        cgReceiveAck(&Peer,1,0x50,9,false,2);
        cgReceiveSend(&Peer,Pubrel,sizeof(Pubrel));
        jwAwait(&D.IO.Entered);
        testRequire(swInfo(W).State.Incoming==1,"receive identity survives until retirement commit");
        cgReceiveNoAck(&Peer,1);
        testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains in-flight PUBREL retirement");
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
        FinalTx=4;FinalInbox=ProtocolOnly?0:1;FinalAcks=1;
    }else if(Mode==CG_ACCEPT_CLOSE){
        FinalTx=3;FinalInbox=ProtocolOnly?0:1;FinalIncoming=1;
    }else FinalTx=Mode==CG_WRITE_FAIL?1:2;

    testRequire(xmqttClientWaitFor(C,15000)==XWAIT_OK,"retire actual Client and all receive writes");
    xmqttclientinfo Closing;
    testRequire(xmqttClientInfo(C,&Closing),"receive close status");
    if(Mode==CG_WRITE_FAIL||Mode==CG_INBOX_LIMIT)
        testRequire(Closing.CloseStatus==(Mode==CG_WRITE_FAIL?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_LIMIT),"typed failed acceptance");
    xrtAtomicPtrStore(&D.Client,NULL,XMEMORY_RELEASE);
    if(ProtocolOnly&&(Mode==CG_ACCEPT_CLOSE||Mode==CG_RELEASE_CLOSE))cgTakeProtocolMessage(C,2);
    xmqttmessage* Unchanged=(xmqttmessage*)(uintptr_t)1;
    testRequire(!xmqttClientTakeMessage(C,&Unchanged)&&Unchanged==(xmqttmessage*)(uintptr_t)1,"durable delivery stays out of connection-local queue");
    testRequire(xmqttClientDestroy(C),"destroy receive Client");
    mqttExampleStop(&Peer.Net);
    testRequire(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)==FinalAcks,"no uncommitted or post-close ACK");
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
    if(ProtocolOnly&&FinalIncoming){
        xmqttstoreview Record=stView(Store,XMQTT_STORE_INCOMING,0);
        testRequire(Record.Identifier==9&&Record.Qos==2&&!Record.Wire.Size&&!Record.InboxPending,
            "cold replay recovers only the receive identity");
    }
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
        testRequire(xmqttClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,&Plan,&Connect,&C),"resume actual stored session");
        Ready=xmqttClientReady(C);
        testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"actual resumed CONNACK");
        if(Mode==CG_ACCEPT_CLOSE){
            cgReceivePublish(&Peer,2,true,false,false);
            cgReceiveAck(&Peer,1,0x50,9,false,FinalTx+1);
            xmqttmessage* Duplicate=NULL;
            testRequire(swInfo(W).State.Inbox==(ProtocolOnly?0u:1u)&&!xmqttClientTakeMessage(C,&Duplicate),
                "replayed duplicate delivers no additional payload");
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
    jwPoolDone(Pool);
    jpFileClose(File);
    if(NativePath){
        testRequire(xrtClose(D.IO.Base.Native)&&xrtFileDelete(NativePath),"retire only the test's native receive journal");
        xrtFree(NativePath);
    }
}


typedef enum cg_send_mode {
    CG_SEND_FIRST, CG_SEND_PUT_CLOSE, CG_SEND_PUT_TIMEOUT, CG_SEND_ACK_CLOSE, CG_SEND_EARLY_ACK,
    CG_SEND_PUBREC_CLOSE, CG_SEND_TWO, CG_SEND_ACK_LIMIT, CG_SEND_UNATTEMPTED,
    CG_SEND_PUT_REFUSAL, CG_SEND_ACK_UNCERTAIN, CG_SEND_REJECTED
} cg_send_mode;

static void cgSentFrame(cg_receive_peer* Peer,unsigned Count,unsigned Qos,bool Duplicate,bool Alias,uint64 Tx)
{
    if(!Qos){cgReceiveAck(Peer,Count,0x62,9,false,Tx);return;}
    double End=xrtTimer()+10;
    while(xrtAtomic32Load(&Peer->Net.Acks,XMEMORY_ACQUIRE)<Count){
        testRequire(xrtTimer()<End,"durable PUBLISH reaches actual TCP peer");
        xrtSleep(1);
    }
    unsigned char Wire[16];
    size_t N=2;
    Wire[0]=(unsigned char)(0x30|(Qos<<1)|(Duplicate?8u:0u));
    Wire[N++]=0;Wire[N++]=1;Wire[N++]='t';
    Wire[N++]=0;Wire[N++]=9;
    if(Peer->Net.Version==XMQTT_VERSION_5){
        Wire[N++]=Alias?3:0;
        if(Alias){Wire[N++]=0x23;Wire[N++]=0;Wire[N++]=1;}
    }
    Wire[N++]='x';
    Wire[1]=(unsigned char)(N-2u);
    testRequire(Peer->AckSize[Count-1]==N&&!memcmp(Peer->Ack[Count-1],Wire,N)&&
                Peer->AckTransaction[Count-1]==Tx,"literal PUBLISH follows expected attempt commit");
}
static const xmqttwriteresult* cgWriteResult(xfuture* F,xmqttclientstatus Status,xmqttdelivery Delivery)
{
    testRequire(xrtFutureWaitFor(F,15000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"typed original publication outcome");
    const xmqttwriteresult* V=xrtFutureValue(F);
    if(V->Status!=Status||V->Delivery!=Delivery)
        fprintf(stderr,"write outcome status %d expected %d, delivery %d expected %d\n",V->Status,Status,V->Delivery,Delivery);
    testRequire(V->Status==Status&&V->Delivery==Delivery&&V->Identifier==9,"original Future reports its exact connection and peer outcome");
    return V;
}
static void cgTransactions(xmqttstorewrite* W,uint64 Tx,size_t Outgoing)
{
    double End=xrtTimer()+10;
    xmqttstorestateinfo Info;
    do{
        Info=swInfo(W).State;
        testRequire(Info.Transactions<=Tx&&xrtTimer()<End,"bounded expected journal progress");
        if(Info.Transactions!=Tx)xrtSleep(1);
    }while(Info.Transactions!=Tx);
    testRequire(Info.Outgoing==Outgoing,"correct outstanding durable identifiers");
}
static xmqttsession* cgImportSend(xmqttstorestate* Store,const xmqttsessionconfig* SK,const xmqttstorestateconfig* SC)
{
    xmqttstoresessionconfig Import;
    xmqttStoreSessionConfigInit(&Import);
    Import.Binding=SC->Binding;
    Import.Generation=SC->Generation;
    Import.WallMilliseconds=SC->WallMilliseconds;
    Import.MonotonicTime=xrtTimer();
    xmqttsession* Session=NULL;
    testRequire(xmqttStoreSessionRestore(Store,SK,&Import,&Session)==XMQTT_SESSION_DONE,"restore exact committed outgoing Session");
    return Session;
}
static void cgSendCase(xmqttversion V,cg_send_mode Mode)
{
    cgdrive D={0};
    str NativePath=NULL;
    if(Mode==CG_SEND_TWO){
        D.IO.Base.Native=xrtFileTemp(NULL,"xmqtt-client-outgoing-",NULL,&NativePath);
        testRequire(D.IO.Base.Native!=NULL,"true native outgoing journal");
    }
    bool Seeded=Mode==CG_SEND_UNATTEMPTED;
    bool Qos2=Mode==CG_SEND_TWO||Mode==CG_SEND_PUBREC_CLOSE||Mode==CG_SEND_REJECTED;
    bool Alias=V==XMQTT_VERSION_5&&Mode==CG_SEND_TWO;
    unsigned Qos=Qos2?2:1;
    bool PutHeld=Seeded||Mode<=CG_SEND_PUT_TIMEOUT;
    bool AckHeld=Mode==CG_SEND_ACK_CLOSE||Mode==CG_SEND_PUBREC_CLOSE;
    bool Resume=Mode==CG_SEND_PUT_CLOSE||Mode==CG_SEND_PUT_TIMEOUT||Mode==CG_SEND_EARLY_ACK||Mode==CG_SEND_PUBREC_CLOSE;
    const unsigned char Puback[]={0x40,2,0,9},Pubrec[]={0x50,2,0,9},Pubcomp[]={0x70,2,0,9};
    const unsigned char Reject[]={0x50,3,0,9,0x80};
    jpfile File=cgFile(&D);
    xmqttjournalconfig JC=stJournalConfig(V);
    xmqttstorestateconfig SC=stConfig(V);
    SC.WallMilliseconds=Seeded?200000:0;
    xmqttjournal* J=NULL;
    if(Seeded){
        unsigned char Wire[]={0x32,7,0,1,'t',0,9,0,'x'};
        size_t Bytes=sizeof(Wire);
        if(V==XMQTT_VERSION_311){--Bytes;Wire[1]=6;Wire[7]='x';}
        uint32 Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX;
        const xmqttstoredelta Ops[]={
            {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=199900,.Data=XRT_BYTES_LITERAL("delta-client")},
            {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=199901,.Expiry=Expiry},
            {.Type=XMQTT_STORE_OUT_PUT,.Epoch=1,.Key=1,.Identifier=9,.Data={Wire,Bytes}},
            {.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=199902,.Expiry=Expiry}
        };
        J=stWrite(File,V,Ops,4,false);
    }else J=jpCreate(File,&JC);
    xmqttstorestate* Store=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"outgoing commit prefix");
    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=V;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    if(Seeded)Session=cgImportSend(Store,&SK,&SC);
    else testRequire(xmqttSessionCreate(&SK,&Session),"empty outgoing Session");

    xtaskpool* Pool=jwPool(1);
    D.IO.Pool=Pool;
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* W=NULL;
    testRequire(xmqttStoreWriteCreate(Pool,J,Store,&WC,&W),"outgoing writer consumes journal");
    D.IO.Base.Calls=0;
    if(PutHeld)D.IO.Hold=14;
    if(AckHeld)D.IO.Hold=21;
    if(Mode==CG_SEND_PUT_REFUSAL)D.IO.Base.Fail=8;
    if(Mode==CG_SEND_ACK_UNCERTAIN){D.IO.Base.Fail=21;D.IO.Base.FlushOnFailure=true;}

    xmqttclientconfig Cfg;
    xmqttClientConfigInit(&Cfg);
    Cfg.Version=V;
    if(Mode==CG_SEND_ACK_LIMIT)Cfg.MaxObjects=3; /* Ready, CONNACK and write result. */
    Cfg.WriteChunkBytes=1;
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SK.ClientId;
    xmqttproperty Expiry={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
    if(V==XMQTT_VERSION_5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    cgclock Clock={0};
    xrtAtomic64Init(&Clock.Value,200010);
    xmqttclientstoreconfig Plan={sizeof(Plan),W,cgWall,&Clock};
    cg_receive_peer Peer;
    cgPeerStart(&Peer,V,W,CG_CAPTURE_PUBLISH|(Seeded?CG_RESUME:0)|(Alias?CG_ALIAS:0));
    xmqttclient* C=NULL;
    testRequire(xmqttClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,&Plan,&Connect,&C),"actual persistent publishing Client");
    xrtAtomicPtrStore(&D.Client,C,XMEMORY_RELEASE);
    xfuture* Ready=xmqttClientReady(C);
    testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"persistent sending after OPEN");
    xfuture* Written=NULL;
    if(!Seeded){
        xmqttpacket Pub;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);
        Pub.Flags=(uint8)(Qos<<1);
        Pub.Identifier=9;
        Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");
        Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("x");
        xmqttproperty TopicAlias={.Id=XMQTT_PROPERTY_TOPIC_ALIAS,.Type=XMQTT_PROPERTY_UINT16,.Value.Number=1};
        if(Alias)Pub.Properties=(xmqttpropertylist){&TopicAlias,1};
        testRequire(xmqttClientPublishAsync(C,&Pub,Mode==CG_SEND_PUT_TIMEOUT?700:10000,&Written)==XMQTT_ADMISSION_ACCEPTED,"normal publish API admits durable send");
    }

    bool BeforeWire=Mode==CG_SEND_PUT_CLOSE||Mode==CG_SEND_PUT_TIMEOUT;
    uint64 SendTx=Seeded?3:2;
    uint64 FinalTx=0;
    unsigned FinalFrames=0;
    if(PutHeld){
        jwAwait(&D.IO.Entered);
        testRequire(swInfo(W).State.Transactions==SendTx-1,"held attempt has no committed effect");
        cgReceiveNoAck(&Peer,0);
        if(Written)testRequire(xrtFutureState(Written)==XFUTURE_PENDING,"application cannot complete during first checkpoint");
        if(Mode==CG_SEND_PUT_CLOSE)
            testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains attempt IO");
        if(Mode==CG_SEND_PUT_TIMEOUT){
            double End=xrtTimer()+5;
            xmqttclientinfo Info;
            do{
                testRequire(xmqttClientInfo(C,&Info)&&xrtTimer()<End,"original send deadline expires during real sync");
                if(Info.State<XMQTT_CLIENT_CLOSING)xrtSleep(5);
            }while(Info.State<XMQTT_CLIENT_CLOSING);
            testRequire(Info.CloseStatus==XMQTT_STATUS_TIMEOUT&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"timeout closes without dropping store duty");
        }
        xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
    }
    if(Mode==CG_SEND_PUT_REFUSAL){
        cgWriteResult(Written,XMQTT_STATUS_INITIALIZATION,XMQTT_NOT_SENT);
        FinalTx=1;
    }else if(BeforeWire){
        cgWriteResult(Written,Mode==CG_SEND_PUT_TIMEOUT?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_CLOSED,XMQTT_NOT_SENT);
        FinalTx=3;
    }else{
        cgSentFrame(&Peer,1,Qos,false,Alias,SendTx);
        testRequire(swInfo(W).State.Outgoing==1,"wire has a committed durable obligation");
        FinalFrames=1;
        if(Mode==CG_SEND_EARLY_ACK){
            testRequire(xmqttClientClose(C),"disconnect after PUBLISH before broker ACK");
            cgWriteResult(Written,XMQTT_STATUS_CLOSED,XMQTT_MAY_HAVE_REACHED_PEER);
            FinalTx=3;
        }else if(Mode==CG_SEND_REJECTED){
            cgReceiveSend(&Peer,Reject,sizeof(Reject));
            const xmqttwriteresult* R=cgWriteResult(Written,XMQTT_STATUS_REJECTED,XMQTT_PEER_REJECTED);
            testRequire(R->PubrecReceived&&R->PubrecReason==0x80&&!R->PubcompReceived,"negative PUBREC retires flow without PUBREL");
            cgTransactions(W,3,0);
            FinalTx=4;
        }else{
            cgReceiveSend(&Peer,Qos2?Pubrec:Puback,4);
            if(Mode==CG_SEND_ACK_UNCERTAIN){
                const xmqttwriteresult* R=cgWriteResult(Written,XMQTT_STATUS_INITIALIZATION,XMQTT_MAY_HAVE_REACHED_PEER);
                testRequire(!R->Puback&&swInfo(W).Worker.Failed&&swInfo(W).State.Outgoing==1,
                            "uncertain ACK effect advertises no completed model or peer result");
                FinalTx=3; /* Fresh recovery sees the fault fixture's written witness. */
            }else if(AckHeld){
                jwAwait(&D.IO.Entered);
                testRequire(xrtFutureState(Written)==XFUTURE_PENDING&&swInfo(W).State.Outgoing==1&&
                            swInfo(W).State.Transactions==2,"held broker ACK does not complete local result");
                cgReceiveNoAck(&Peer,1);
                xmqttpacket Same;
                xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Same);
                Same.Flags=(uint8)(Qos<<1);
                Same.Identifier=9;
                Same.Body.Publish.Topic=XRT_STR_LITERAL("t");
                xfuture* Unchanged=(xfuture*)(uintptr_t)1;
                testRequire(xmqttClientPublishAsync(C,&Same,1000,&Unchanged)==XMQTT_ADMISSION_OVERLOADED&&
                            Unchanged==(xfuture*)(uintptr_t)1,"identifier stays reserved until durable broker acknowledgement");
                testRequire(xmqttClientClose(C)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,"close retains observed ACK IO");
                xrtAtomic32Store(&D.IO.Release,1,XMEMORY_RELEASE);
                const xmqttwriteresult* R=cgWriteResult(Written,XMQTT_STATUS_CLOSED,Qos2?XMQTT_MAY_HAVE_REACHED_PEER:XMQTT_PEER_ACCEPTED);
                testRequire(Qos2?R->PubrecReceived&&!R->PubcompReceived:R->Puback!=NULL,
                            "committed ACK facts survive late logical close");
                FinalTx=4;
            }else if(Qos2){
                cgSentFrame(&Peer,2,0,false,false,3);
                cgReceiveSend(&Peer,Pubrec,sizeof(Pubrec));
                cgSentFrame(&Peer,3,0,false,false,3);
                testRequire(xrtFutureState(Written)==XFUTURE_PENDING,"duplicate PUBREC is a protocol retry, not completion");
                cgReceiveSend(&Peer,Pubcomp,sizeof(Pubcomp));
                const xmqttwriteresult* R=cgWriteResult(Written,XMQTT_STATUS_OK,XMQTT_PEER_ACCEPTED);
                testRequire(R->PubrecReceived&&R->PubcompReceived&&R->Pubrec&&R->Pubcomp,"typed QoS2 ACK metadata");
                cgTransactions(W,4,0);
                FinalFrames=3;
                FinalTx=5;
            }else{
                if(Written){
                    const xmqttwriteresult* R=cgWriteResult(Written,Mode==CG_SEND_ACK_LIMIT?XMQTT_STATUS_LIMIT:XMQTT_STATUS_OK,XMQTT_PEER_ACCEPTED);
                    testRequire((R->Puback!=NULL)==(Mode!=CG_SEND_ACK_LIMIT),"metadata limit retains durable scalar peer decision");
                }
                if(Mode!=CG_SEND_ACK_LIMIT)cgTransactions(W,SendTx+1,0);
                FinalTx=SendTx+2;
            }
        }
    }

    if(Mode==CG_SEND_FIRST){
        xmqttpacket Pub;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);
        Pub.Body.Publish.Topic=XRT_STR_LITERAL("t");
        Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("x");
        xfuture* F=NULL;
        testRequire(xmqttClientPublishAsync(C,&Pub,10000,&F)==XMQTT_ADMISSION_ACCEPTED&&
                    xrtFutureWaitFor(F,10000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,
                    "QoS0 uses the native write completion");
        const xmqttwriteresult* Value=xrtFutureValue(F);
        testRequire(Value->Status==XMQTT_STATUS_OK&&!Value->Identifier,"QoS0 has no identifier obligation");
        xmqttclientstoreoutcome Unchanged={.Key=UINT64_MAX};
        testRequire(!xmqttClientStoreOutcome(Value,&Unchanged)&&Unchanged.Key==UINT64_MAX,
                    "QoS0 preserves the original result allocation and leaves optional output untouched");
        double Wait=xrtTimer()+10;
        while(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)<2){
            testRequire(xrtTimer()<Wait,"QoS0 native peer bytes");
            xrtSleep(1);
        }
        const unsigned char Wire5[]={0x30,5,0,1,'t',0,'x'},Wire311[]={0x30,4,0,1,'t','x'};
        const unsigned char* Wire=V==XMQTT_VERSION_5?Wire5:Wire311;
        size_t N=V==XMQTT_VERSION_5?sizeof(Wire5):sizeof(Wire311);
        testRequire(Peer.AckSize[1]==N&&!memcmp(Peer.Ack[1],Wire,N)&&Peer.AckTransaction[1]==3,
                    "literal QoS0 without a new journal checkpoint");
        xrtFutureDestroy(F);
        F=NULL;

        Pub.Identifier=9;
        Pub.Flags=2;
        Wait=xrtTimer()+10;
        xmqttclientadmission Admission;
        do{
            Admission=xmqttClientPublishAsync(C,&Pub,10000,&F);
            if(Admission==XMQTT_ADMISSION_OVERLOADED)xrtSleep(1);
        }while(Admission==XMQTT_ADMISSION_OVERLOADED&&xrtTimer()<Wait);
        testRequire(Admission==XMQTT_ADMISSION_ACCEPTED,"same identifier reusable after committed ACK and native retirement");
        cgSentFrame(&Peer,3,1,false,false,4);
        xmqttstorewriteinfo Info=swInfo(W);
        testRequire(Info.State.LastKey==2&&Info.State.Outgoing==1,"identifier reuse gets a new persistent key");
        cgReceiveSend(&Peer,Puback,sizeof(Puback));
        (void)cgWriteResult(F,XMQTT_STATUS_OK,XMQTT_PEER_ACCEPTED);
        xrtFutureDestroy(F);
        FinalFrames=3;
        FinalTx=6;
    }

    testRequire(xmqttClientClose(C)&&xmqttClientWaitFor(C,15000)==XWAIT_OK,"retire outgoing Client, request native tails and end checkpoint");
    xrtAtomicPtrStore(&D.Client,NULL,XMEMORY_RELEASE);
    testRequire(xmqttClientDestroy(C),"destroy original publishing Client");
    mqttExampleStop(&Peer.Net);
    testRequire(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)==FinalFrames,"no unauthorized PUBLISH or PUBREL left the Client");
    testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(W),"physical owners retired before recovery");
    xrtFutureDestroy(Ready);

    D.IO.Base.Fail=D.IO.Hold=0;
    J=jpReopen(File,&JC);
    SC.WallMilliseconds=201000;
    Store=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"recover real durable outgoing state");
    xmqttstorestateinfo S=stInfo(Store);
    testRequire(S.Transactions==FinalTx&&S.Outgoing==(Resume?1u:0u),"fresh store reproduces outgoing ACK cut");
    if(Resume){
        xmqttstoreview Record=stView(Store,XMQTT_STORE_OUTGOING,0);
        testRequire(Record.Key==1&&Record.Identifier==9&&Record.Attempted&&Record.Pubrel==Qos2&&
                    Record.Sent==Qos2&&Record.Wire.Data[0]==(0x30u|(Qos<<1)),"canonical persistent attempt and PUBREL state");
        Session=cgImportSend(Store,&SK,&SC);
        W=NULL;
        testRequire(xmqttStoreWriteCreate(Pool,J,Store,&WC,&W),"reopened outgoing writer");
        Plan.Writer=W;
        xrtAtomic64Store(&Clock.Value,201001,XMEMORY_RELEASE);
        bool Early=Mode==CG_SEND_EARLY_ACK;
        cgPeerStart(&Peer,V,W,CG_RESUME|CG_CAPTURE_PUBLISH|(Early?CG_EARLY_PUBACK:0));
        C=NULL;
        testRequire(xmqttClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&Cfg,Session,&Plan,&Connect,&C),"actual resumed publishing Client");
        Ready=xmqttClientReady(C);
        testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"resume surviving broker epoch");
        if(!Early){
            cgSentFrame(&Peer,1,Qos2?0:1,true,false,FinalTx+1);
            cgReceiveSend(&Peer,Qos2?Pubcomp:Puback,4);
        }
        cgTransactions(W,FinalTx+2,0);
        testRequire(xmqttClientDestroy(C),"retire resumed publishing connection");
        mqttExampleStop(&Peer.Net);
        testRequire(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)==(Early?0u:1u),"replay sends only the outstanding protocol stage");
        testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(W),"release resumed owners");
        xrtFutureDestroy(Ready);
        J=jpReopen(File,&JC);
        Store=NULL;
        SC.WallMilliseconds=202000;
        stStatus(xmqttStoreStateRecover(J,&SC,&Store),XMQTT_STORE_DONE,"replay final resumed acknowledgement");
        testRequire(!stInfo(Store).Outgoing&&stInfo(Store).Transactions==FinalTx+3,"replayed completion and end persist");
    }
    xmqttStoreStateDestroy(Store);
    testRequire(xmqttJournalDestroy(J),"release recovered state, file and independent Inbox");
    jwPoolDone(Pool);
    jpFileClose(File);
    if(NativePath){
        testRequire(xrtClose(D.IO.Base.Native)&&xrtFileDelete(NativePath),"release only the native temporary journal");
        xrtFree(NativePath);
    }
    if(Written){
        const xmqttwriteresult* V=xrtFutureValue(Written);
        testRequire(xrtFutureState(Written)==XFUTURE_RESOLVED&&V->Identifier==9,
                    "original result remains valid without any old or resumed Client owner");
        xmqttclientstoreoutcome Out;
        testRequire(xmqttClientStoreOutcome(V,&Out),"copy physical checkpoint outcome after every live owner retired");
        if(Mode==CG_SEND_PUT_REFUSAL){
            testRequire(!Out.Attempt.Effect&&!Out.Attempt.Sequence&&!Out.Key,"definite first attempt refusal has no durable key");
        }else{
            testRequire(Out.Epoch==1&&Out.Key==1&&Out.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&Out.Attempt.Sequence==2,
                        "actual attempted obligation separate from network delivery");
            bool Pubrel=Mode==CG_SEND_TWO||Mode==CG_SEND_PUBREC_CLOSE;
            testRequire(Out.Pubrel.Effect==(Pubrel?XMQTT_JOURNAL_COMMITTED:XMQTT_JOURNAL_NOT_COMMITTED)&&
                        Out.Pubrel.Sequence==(Pubrel?3u:0u),"exact successful PUBREC checkpoint");
            bool Complete=Mode==CG_SEND_FIRST||Mode==CG_SEND_ACK_CLOSE||Mode==CG_SEND_TWO||
                          Mode==CG_SEND_ACK_LIMIT||Mode==CG_SEND_REJECTED;
            xmqttjournaleffect Effect=Mode==CG_SEND_ACK_UNCERTAIN?XMQTT_JOURNAL_MAY_BE_COMMITTED:
                                      Complete?XMQTT_JOURNAL_COMMITTED:XMQTT_JOURNAL_NOT_COMMITTED;
            testRequire(Out.Completion.Effect==Effect&&Out.Completion.Sequence==
                        (Effect?(Mode==CG_SEND_TWO?4u:3u):0u),"exact ACK checkpoint despite late status, metadata loss or uncertainty");
        }
        xrtFutureDestroy(Written);
    }
}


typedef enum cg_end_mode {
    CG_END_NORMAL,CG_END_CANCEL,CG_END_TIMEOUT,CG_END_UNCERTAIN,CG_END_PENDING_TX,CG_END_ZERO_OVERRIDE
} cg_end_mode;

static void cgCloseReason(xmqttclient* C,xmqttclientstatus Status)
{
    double Deadline=xrtTimer()+10;
    for(;;){
        xmqttclientinfo Info;
        testRequire(xmqttClientInfo(C,&Info)&&xrtTimer()<Deadline,"bounded local close notification");
        if(Info.CloseStatus==Status)return;
        testRequire(Info.CloseStatus==XMQTT_STATUS_OK,"expected first local close error");
        xrtSleep(1);
    }
}
static void cgControls(xmqttclient* C,xmqttstorewrite* W)
{
    xmqttsubscription Topic={.Filter=XRT_STR_LITERAL("t"),.Options=0};
    for(unsigned I=0;I<2;++I){
        xmqttpacket Q;
        xmqttPacketInit(I?XMQTT_PACKET_UNSUBSCRIBE:XMQTT_PACKET_SUBSCRIBE,&Q);
        Q.Identifier=(uint16)(1+I);
        Q.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
        xfuture* F=NULL;
        xmqttclientadmission Admission=I?xmqttClientUnsubscribeAsync(C,&Q,10000,&F):xmqttClientSubscribeAsync(C,&Q,10000,&F);
        testRequire(Admission==XMQTT_ADMISSION_ACCEPTED&&xrtFutureWaitFor(F,10000)==XWAIT_OK&&
                    xrtFutureState(F)==XFUTURE_RESOLVED,"ordinary subscription Future on durable Client");
        const xmqttsubscriptionresult* V=xrtFutureValue(F);
        testRequire(V->Status==XMQTT_STATUS_OK&&V->Acknowledged&&V->Granted==1&&
                    swInfo(W).State.Transactions==1,"broker control has no false durable-intent record");
        xrtFutureDestroy(F);
    }
}
typedef enum cg_transport {CG_NUMERIC,CG_HOSTNAME,CG_PREPARED} cg_transport;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
typedef struct cg_dns {
    xnetaddr Address;
    bool Missing;
    xatomic32 Entered,Release,Exited;
} cg_dns;

static xnetaddrlist* cgLookup(cstr Host,xnetfamily Family,ptr User)
{
    cg_dns* DNS=User;
    xrtAtomic32Store(&DNS->Entered,1,XMEMORY_RELEASE);
    jwAwait(&DNS->Release);
    testRequire(!strcmp(Host,"mqtt.store.fixture")&&Family==XNET_FAMILY_IPV4,"preserve copied hostname and selected family");
    xrtAtomic32Store(&DNS->Exited,1,XMEMORY_RELEASE);
    if(DNS->Missing){
        xrtSetErrorInfo(XERR_NOT_FOUND,"fixture.mqtt.store.dns",71,"unknown durable broker");
        return NULL;
    }
    return xrtNetAddrListCreate(&DNS->Address,1);
}

static xnetresolver* cgResolver(cg_dns* DNS)
{
    xrtAtomic32Init(&DNS->Entered,0);
    xrtAtomic32Init(&DNS->Release,0);
    xrtAtomic32Init(&DNS->Exited,0);
    xnetresolverconfig Config;
    xrtNetResolverConfigInit(&Config);
    Config.Workers=1;
    Config.Lookup=cgLookup;
    Config.LookupData=DNS;
    xnetresolver* Resolver=xrtNetResolverCreate(&Config);
    testRequire(Resolver!=NULL,"create independent durable DNS resolver");
    return Resolver;
}
#endif

static void cgEndCase(xmqttversion Version,cg_end_mode Mode,cg_transport Transport)
{
    cgdrive Drive={0};
    str NativePath=NULL;
    if(Mode==CG_END_NORMAL){
        Drive.IO.Base.Native=xrtFileTemp(NULL,"xmqtt-graceful-",NULL,&NativePath);
        testRequire(Drive.IO.Base.Native!=NULL,"real graceful native journal");
    }
    jpfile File=cgFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(Version);
    xmqttjournal* J=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);
    SC.WallMilliseconds=0;
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"new public Client store");
    xtaskpool* Pool=jwPool(1);
    Drive.IO.Pool=Pool;
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreWriteCreate(Pool,J,State,&WC,&Writer),"explicit durable Inbox");
    Drive.IO.Base.Calls=0;
    Drive.IO.Hold=14;
    if(Mode==CG_END_UNCERTAIN){Drive.IO.Base.Fail=14;Drive.IO.Base.FlushOnFailure=true;}

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"fresh public Session");
    cg_receive_peer Peer;
    cgPeerStart(&Peer,Version,Writer,CG_CAPTURE_PUBLISH|CG_CONTROL|(Mode==CG_END_ZERO_OVERRIDE?CG_ZERO_EXPIRY:0));
    xmqttclientconfig CC;
    xmqttClientConfigInit(&CC);
    CC.Version=Version;
    CC.WriteChunkBytes=1;
    xmqttproperty Expiry={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SK.ClientId;
    bool V5=Version==XMQTT_VERSION_5;
    if(V5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    cgclock Clock={0};
    xrtAtomic64Init(&Clock.Value,200000);
    xmqttclientstoreconfig Store={sizeof(Store),Writer,cgWall,&Clock};
    xmqttclient* C=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetresolver* Resolver=NULL;
    xnetsocket Refused=NULL;
    cg_dns DNS={.Address=Peer.Net.Address};
    DNS.Address.Port=0;
    xmqttclientdialconfig Dial;
    xmqttClientDialConfigInit(&Dial);
    Dial.Family=XNET_FAMILY_IPV4;
    if(Transport==CG_HOSTNAME){
        Resolver=cgResolver(&DNS);
        char Host[]="mqtt.store.fixture";
        testRequire(xmqttClientDialStore(Peer.Net.Engine,Resolver,(xstrview){Host,sizeof(Host)-1},
            Peer.Net.Address.Port,1,&CC,&Dial,Session,&Store,&Connect,&C),"public Store hostname factory");
        memset(Host,'x',sizeof(Host)-1);
        xrtAtomic32Store(&DNS.Release,1,XMEMORY_RELEASE);
    }else if(Transport==CG_PREPARED){
        xnetaddr Addresses[2]={Peer.Net.Address,Peer.Net.Address};
        Addresses[0].Port=0;
        Refused=xrtNetSocketOpen(XNET_FAMILY_IPV4,XNET_SOCKET_STREAM,XNET_SOCKET_NONBLOCK);
        testRequire(Refused&&xrtNetSocketBind(Refused,&Addresses[0])&&xrtNetSocketLocal(Refused,&Addresses[0]),
            "bound non-listening first endpoint");
        xnetaddrlist* List=xrtNetAddrListCreate(Addresses,2);
        Dial.FallbackDelay=1;
        Dial.MaxAttempts=2;
        testRequire(List&&xmqttClientDialAddressesStore(Peer.Net.Engine,List,1,&CC,&Dial,Session,&Store,&Connect,&C),
            "public Store prepared-address fallback");
        xrtNetAddrListDestroy(List);
    }else
#else
    (void)Transport;
#endif
    testRequire(xmqttClientConnectStore(Peer.Net.Engine,&Peer.Net.Address,1,&CC,Session,&Store,&Connect,&C),"public Store factory connects without volatile Inbox");
    xrtAtomicPtrStore(&Drive.Client,C,XMEMORY_RELEASE);
    xfuture* Ready=xmqttClientReady(C);
    testRequire(cgReady(Ready)->Status==XMQTT_STATUS_OK,"public Ready follows OPEN");
    bool Controls=Mode==CG_END_NORMAL;
    if(Controls)cgControls(C,Writer);
    unsigned Frames=Controls?2:0;
    xfuture* Published=NULL;
    bool PendingTx=Mode==CG_END_PENDING_TX;
    if(PendingTx){
        xmqttpacket Publish;
        xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);
        Publish.Flags=2;Publish.Identifier=9;
        Publish.Body.Publish.Topic=XRT_STR_LITERAL("t");
        Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("x");
        testRequire(xmqttClientPublishAsync(C,&Publish,10000,&Published)==XMQTT_ADMISSION_ACCEPTED,"earlier PUBLISH before graceful drain");
        jwAwait(&Drive.IO.Entered);
    }
    xmqttpacket Disconnect;
    xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Disconnect);
    uint32 EndExpiry=Version==XMQTT_VERSION_311?UINT32_MAX:Mode==CG_END_NORMAL?0:17;
    xmqttproperty Override={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=EndExpiry};
    if(V5){Disconnect.Properties=(xmqttpropertylist){&Override,1};Disconnect.Reason=4;}
    xrtAtomic64Store(&Clock.Value,200050,XMEMORY_RELEASE);
    xfuture* Closing=NULL;
    testRequire(xmqttClientDisconnectAsync(C,&Disconnect,Mode==CG_END_TIMEOUT?650:10000,&Closing)==XMQTT_ADMISSION_ACCEPTED,"admit graceful final packet");
    xmqttclientstatus Expected=XMQTT_STATUS_OK;
    if(Mode==CG_END_CANCEL||PendingTx){
        if(!PendingTx)jwAwait(&Drive.IO.Entered);
        testRequire(xrtFutureCancel(Closing),"cancel original graceful Future with live IO");
        Expected=XMQTT_STATUS_CANCELLED;
        cgCloseReason(C,Expected);
    }else{
        jwAwait(&Drive.IO.Entered);
        if(Mode==CG_END_TIMEOUT){
            Expected=XMQTT_STATUS_TIMEOUT;
            cgCloseReason(C,Expected);
        }else if(Mode==CG_END_UNCERTAIN)Expected=XMQTT_STATUS_INITIALIZATION;
    }
    if(!PendingTx)++Frames;
    if(Frames) {
        double Deadline=xrtTimer()+10;
        while(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)<Frames){
            testRequire(xrtTimer()<Deadline,"peer observed complete final DISCONNECT");
            xrtSleep(1);
        }
        unsigned char Wire5[]={0xe0,7,4,5,0x11,0,0,0,(unsigned char)EndExpiry};
        const unsigned char Wire311[]={0xe0,0};
        size_t Bytes=V5?sizeof(Wire5):sizeof(Wire311);
        testRequire(Peer.AckSize[Frames-1]==Bytes&&!memcmp(Peer.Ack[Frames-1],V5?Wire5:Wire311,Bytes)&&
                    Peer.AckTransaction[Frames-1]==1,"complete literal DISCONNECT predates END publication");
    }
    testRequire(xrtFutureState(Closing)==XFUTURE_PENDING&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT&&
                swInfo(Writer).State.Transactions==1,"held physical storage keeps graceful result and retirement pending");
    xrtAtomic32Store(&Drive.IO.Release,1,XMEMORY_RELEASE);

    testRequire(xrtFutureWaitFor(Closing,15000)==XWAIT_OK&&xrtFutureState(Closing)==XFUTURE_RESOLVED,"typed graceful outcome");
    const xmqttwriteresult* V=xrtFutureValue(Closing);
    testRequire(V->Status==Expected&&V->AcceptedBytes==(PendingTx?0:V5?9:2)&&
                V->Delivery==(PendingTx?XMQTT_NOT_SENT:XMQTT_MAY_HAVE_REACHED_PEER),"graceful status independent of complete local wire");
    if(Published){
        const xmqttwriteresult* Pub=cgWriteResult(Published,XMQTT_STATUS_CANCELLED,XMQTT_NOT_SENT);
        xmqttclientstoreoutcome Saved;
        testRequire(xmqttClientStoreOutcome(Pub,&Saved)&&Saved.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&Saved.Key==1,
                    "failed graceful drain pins earlier in-flight publishing request");
    }
    testRequire(xmqttClientWaitFor(C,15000)==XWAIT_OK,"physical public Client retirement");
    xrtAtomicPtrStore(&Drive.Client,NULL,XMEMORY_RELEASE);
    testRequire(xmqttClientDestroy(C),"public Client destroy");
    mqttExampleStop(&Peer.Net);
    testRequire(xrtAtomic32Load(&Peer.Net.Acks,XMEMORY_ACQUIRE)==Frames,"no trailing native frame after disconnect");
    testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(Writer),"retire borrowed owners");
    jwPoolDone(Pool);
    xrtFutureDestroy(Ready);
    xrtFutureDestroy(Published);
    xmqttclientstoreoutcome Result;
    testRequire(xmqttClientStoreOutcome(V,&Result)&&!Result.Key&&Result.Epoch==1&&
                Result.End.Effect==(Mode==CG_END_UNCERTAIN?XMQTT_JOURNAL_MAY_BE_COMMITTED:XMQTT_JOURNAL_COMMITTED)&&
                Result.End.Sequence==(PendingTx?3u:2u),"retained application result records actual END effect");
    xrtFutureDestroy(Closing);

    Drive.IO.Base.Fail=Drive.IO.Hold=0;
    J=jpReopen(File,&JC);
    uint32 Effective=PendingTx?(V5?60:UINT32_MAX):EndExpiry;
    xmqttjournalrecord Record;
    unsigned Count=0;
    xmqttjournalstatus St;
    while((St=xmqttJournalNext(J,&Record))==XMQTT_JOURNAL_DONE){
        ++Count;
        if(Record.Sequence==Result.End.Sequence){
            xmqttstoredelta Ops[4];
            xmqttstoredeltadocument Doc;
            testRequire(xmqttStoreDeltaRead(&SC.Delta,Record.Data,Ops,4,NULL,0,&Doc)==XMQTT_STORE_DELTA_DONE&&
                        Doc.Count==1&&Ops[0].Type==XMQTT_STORE_END&&Ops[0].Flags==(PendingTx?0u:1u)&&
                        Ops[0].Reason==(V5&&!PendingTx?4u:0u)&&Ops[0].Expiry==Effective&&
                        Ops[0].WallMilliseconds==200050,"raw replay checks exact graceful reason, expiry and native-close clock");
        }
    }
    testRequire(St==XMQTT_JOURNAL_EMPTY&&Count==Result.End.Sequence&&xmqttJournalDestroy(J),"full raw end history");
    J=jpReopen(File,&JC);
    State=NULL;
    SC.WallMilliseconds=201050;
    stStatus(xmqttStoreStateRecover(J,&SC,&State),XMQTT_STORE_DONE,"full semantic replay of graceful end");
    xmqttstorestateinfo Recovered=stInfo(State);
    bool Retained=Effective!=0;
    testRequire(Recovered.Transactions==Result.End.Sequence&&!Recovered.LiveAtCut&&Recovered.HasSession==Retained&&
                Recovered.Outgoing==(PendingTx?1u:0u)&&Recovered.RemainingMilliseconds==
                (Effective==UINT32_MAX?UINT64_MAX:Effective?(uint64)Effective*1000u-1000u:0),"correct recovered expiry and obligation");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Resolver)testRequire(xrtNetResolverDestroy(Resolver),"retire hostname resolver after Client");
    if(Refused)testRequire(xrtNetSocketClose(Refused),"retire owned refused endpoint");
#endif
    xmqttStoreStateDestroy(State);
    testRequire(xmqttJournalDestroy(J),"retire sole native reader");
    jpFileClose(File);
    if(NativePath){
        testRequire(xrtClose(Drive.IO.Base.Native)&&xrtFileDelete(NativePath),"retire owned graceful journal");
        xrtFree(NativePath);
    }
}


#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
static void cgDialFailure(xmqttclientstatus Expected)
{
    jwdrive Drive={0};
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);
    xmqttstorestateconfig SC=stConfig(XMQTT_VERSION_5);
    xmqttjournal* Journal=jpCreate(File,&JC);
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"empty dial journal");
    xtaskpool* Pool=jwPool(1);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer),"dial failure Writer");
    Drive.Base.Calls=0;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"dial failure Session");
    mqttExamplePeer Peer;
    mqttExampleStart(&Peer,XMQTT_VERSION_5);
    cg_dns DNS={.Address=Peer.Address,.Missing=Expected==XMQTT_STATUS_TRANSPORT};
    xnetresolver* Resolver=cgResolver(&DNS);
    if(DNS.Missing)xrtAtomic32Store(&DNS.Release,1,XMEMORY_RELEASE);
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SK.ClientId;
    cgclock Clock={0};
    xrtAtomic64Init(&Clock.Value,200000);
    xmqttclientstoreconfig Store={sizeof(Store),Writer,cgWall,&Clock};
    xmqttclientdialconfig Dial;
    xmqttClientDialConfigInit(&Dial);
    Dial.Family=XNET_FAMILY_IPV4;
    xmqttclientconfig Config;
    xmqttClientConfigInit(&Config);
    Config.SetupTimeout=Expected==XMQTT_STATUS_TIMEOUT?500:10000;
    const xstrview Host=XRT_STR_LITERAL("mqtt.store.fixture");
    xmqttclient* C=(xmqttclient*)(uintptr_t)1;
    if(DNS.Missing){
        xallocationbudget* Budget=NULL;
        xallocationscope Scope={0};
        testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"native dial allocation budget");
        testRequire(!xmqttClientDialStore(Peer.Engine,Resolver,Host,Peer.Address.Port,1,&Config,&Dial,
            Session,&Store,&Connect,&C)&&C==(xmqttclient*)(uintptr_t)1,"OOM returns without publishing or claiming");
        testRequire(xrtAllocationBudgetLeave(&Scope),"leave native dial budget");
        xallocationbudgetinfo Info={.Size=sizeof(Info)};
        testRequire(xrtAllocationBudgetInfo(Budget,&Info)&&Info.Rejections&&!Info.LiveBytes,"dial allocation rejection refunds all storage");
        xrtAllocationBudgetFree(Budget);
        xrtClearError();
    }
    testRequire(xmqttClientDialStore(Peer.Engine,Resolver,Host,Peer.Address.Port,1,&Config,&Dial,
        Session,&Store,&Connect,&C),"accept asynchronous dial");
    xfuture* Ready=xmqttClientReady(C);
    jwAwait(&DNS.Entered);
    if(Expected==XMQTT_STATUS_CANCELLED)testRequire(xrtFutureCancel(Ready),"cancel Ready during actual DNS");
    if(Expected==XMQTT_STATUS_CLOSED)testRequire(xmqttClientClose(C),"close during actual DNS");
    testRequire(cgReady(Ready)->Status==Expected,"typed dial failure before handshake");
    testRequire(xmqttClientWaitFor(C,10000)==XWAIT_OK&&xmqttClientDestroy(C),"retire Client independently of shared DNS");
    if(!DNS.Missing)testRequire(!xrtAtomic32Load(&DNS.Exited,XMEMORY_ACQUIRE)&&
        xrtNetResolverTryDestroy(Resolver)==XNET_RETIRE_BUSY,"detached DNS is still owned by Resolver");
    testRequire(!Drive.Base.Calls&&!swInfo(Writer).State.Transactions&&
        xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(Writer),"dial failure leaves journal untouched and releases both claims");
    jwPoolDone(Pool);
    xrtAtomic32Store(&DNS.Release,1,XMEMORY_RELEASE);
    testRequire(xrtNetResolverDestroy(Resolver),"join real DNS worker");
    testRequire(xrtNetListenerClose(Peer.Listener),"close untouched listener");
    mqttExampleWait(&Peer.ListenerClosed);
    testRequire(!xrtAtomicPtrLoad(&Peer.Stream,XMEMORY_ACQUIRE),"no TCP acceptance after failed dial");
    xrtNetListenerDestroy(Peer.Listener);
    testRequire(xrtNetEngineStop(Peer.Engine)&&xrtNetEngineDestroy(Peer.Engine),"retire unused broker Engine");
    xrtFutureDestroy(Ready);
    Journal=jpReopen(File,&JC);
    xmqttjournalrecord Record;
    testRequire(xmqttJournalNext(Journal,&Record)==XMQTT_JOURNAL_EMPTY&&xmqttJournalDestroy(Journal),"cold reopen verifies no OPEN or END");
    jpFileClose(File);
    xrtClearError();
}
#endif

int main(void)
{
    for(unsigned I=0;I<2;++I)
        for(cg_end_mode Mode=CG_END_NORMAL;Mode<=CG_END_PENDING_TX;++Mode)
            cgEndCase(I?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode,CG_NUMERIC);
    cgEndCase(XMQTT_VERSION_5,CG_END_ZERO_OVERRIDE,CG_NUMERIC);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    for(unsigned I=0;I<2;++I){
        xmqttversion Version=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        cgEndCase(Version,CG_END_NORMAL,CG_HOSTNAME);
        cgEndCase(Version,CG_END_NORMAL,CG_PREPARED);
    }
    cgDialFailure(XMQTT_STATUS_TRANSPORT);
    cgDialFailure(XMQTT_STATUS_TIMEOUT);
    cgDialFailure(XMQTT_STATUS_CANCELLED);
    cgDialFailure(XMQTT_STATUS_CLOSED);
    puts("[PASS] durable TCP hostname and prepared fallback on both levels, copied host/list, native OOM and DNS failure/cancel/close/deadline");
#endif
    puts("[PASS] public durable TCP factory, broker controls and physical graceful END effects across timeout, cancel, uncertain sync and pending PUBLISH");
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        for(cg_send_mode Mode=CG_SEND_FIRST;Mode<=CG_SEND_ACK_UNCERTAIN;++Mode)cgSendCase(V,Mode);
    }
    cgSendCase(XMQTT_VERSION_5,CG_SEND_REJECTED);
    puts("[PASS] outgoing attempt-before-PUBLISH, ACK-before-result, PUBREC-before-PUBREL and exact durable replay for both protocol levels");
    for(unsigned V=0;V<2;++V)for(unsigned Mode=0;Mode<8;++Mode)cgCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode);
    for(unsigned V=0;V<2;++V)
        for(cg_receive_mode Mode=CG_FLOW;Mode<=CG_INBOX_LIMIT;++Mode){
            cgReceiveCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode,false);
            cgReceiveCase(V?XMQTT_VERSION_5:XMQTT_VERSION_311,Mode,true);
        }
    cgReceiveCase(XMQTT_VERSION_311,CG_LIVE_LEASE,false);
    cgReceiveCase(XMQTT_VERSION_5,CG_LIVE_LEASE,false);
    puts("[PASS] both protocol levels route protocol-only deliveries to the APP with durable QoS2 and no duplicate after cold recovery");
    puts("[PASS] live QoS2 protocol retirement queues behind an application ACK while file sync is held");
    puts("[PASS] live receive persists before ACK, deduplicates across actual reconnect, retires QoS2 before PUBCOMP and recovers canonical delivery");
    puts("[PASS] actual TCP CONNACK waits for durable BIND/OPEN; late Close/deadline retain IO and END duties,both MQTT levels; storage/clock refusal and exclusive Writer ownership");return 0;
}
