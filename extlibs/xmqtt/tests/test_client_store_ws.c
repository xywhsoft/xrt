#include <xmqtt/client_store_ws.h>
#include "support/store_write_peer.h"
#include "support/ws_connect_peer.h"

typedef enum dwmode {DW_NORMAL,DW_HTTP_REJECT,DW_TRUNCATE,DW_AUTH} dwmode;
typedef struct dwpeer {mqttWsConnectPeer Net;xmqttstorewrite* Writer;dwmode Mode;} dwpeer;

static bool dwWall(ptr User,uint64* Value)
{*Value=*(const uint64*)User;return true;}

static bool dwFrame(xwsstream* Stream,mqttWsPeer* Peer,unsigned char Kind,const unsigned char* Body,size_t Bytes)
{
    dwpeer* Case=(dwpeer*)Peer;
    if(Kind==0x10)
        testRequire(!swInfo(Case->Writer).State.Transactions,"MQTT CONNECT after Upgrade precedes OPEN");
    else if(Kind==0x32)
        testRequire(swInfo(Case->Writer).State.Transactions==2,"outgoing attempt is durable at the peer");
    else if(Kind==0xe0){
        const unsigned char Close5[]={4,5,0x11,0,0,0,17};
        bool Expected=Peer->Version==XMQTT_VERSION_5?Bytes==sizeof(Close5)&&!memcmp(Body,Close5,Bytes):Bytes==0;
        testRequire(Expected&&swInfo(Case->Writer).State.Transactions==3,"complete DISCONNECT before durable END");
        if(Case->Mode==DW_TRUNCATE){
            xrtAtomic32FetchAdd(&Peer->Disconnects,1,XMEMORY_RELEASE);
            testRequire(xrtWsStreamAbort(Stream),"drop transport before WebSocket close handshake");
            return true;
        }
    }
    return false;
}

static xmqttclient* dwConnect(dwpeer* Case,xmqttsession* Session,const xmqttclientstoreconfig* Store)
{
    mqttWsConnectPeer* P=&Case->Net;
    char URI[256];
    const char* Host=P->Base.TLS?"example.com":"127.0.0.1";
    int A=snprintf(P->UriAuthority,sizeof(P->UriAuthority),"%s:%u",Host,(unsigned)P->Base.Address.Port);
    testRequire(A>0&&(size_t)A<sizeof(P->UriAuthority),"bounded exact URI authority");
    P->ExpectedHost=(xstrview){P->UriAuthority,(size_t)A};
    int N=snprintf(URI,sizeof(URI),"%s://%s/mqtt?x=%%00",P->Base.TLS?"wss":"ws",P->UriAuthority);
    testRequire(N>0&&(size_t)N<sizeof(URI),"exact percent-encoded URI target");
    xstrview U={URI,(size_t)N};
    xmqttclientwsconnectconfig W;
    xmqttClientWsConnectConfigInit(&W);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttauthconfig Auth;
    if(Case->Mode==DW_AUTH){xmqttAuthConfigInit(&Auth);W.WebSocket.Auth=&Auth;}
#endif
    xmqttclientconfig K=P->Base.ClientConfig;
    K.WriteChunkBytes=7;
    xmqttclient* Client=(xmqttclient*)(uintptr_t)1;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    if(P->Base.TLS){
        xmqttclienttlsconfig T;
        xmqttClientTlsConfigInit(&T);
        T.Session.Context=P->Base.Context;
        T.Session.Verifier=P->Base.Verifier;
        T.Session.ResumeLimit=0;
        testRequire(xmqttClientWssConnectUriStore(P->Base.Engine,P->Resolver,U,1,&K,&W,&T,Session,Store,&P->Base.Connect,&Client),
                    "public WSS URI durable factory");
    }else
#endif
    {
        W.WebSocket.Session=Session;
        testRequire(!xmqttClientWsConnectUriStore(P->Base.Engine,NULL,U,1,&K,&W,Session,Store,&P->Base.Connect,&Client)&&
                     Client==(xmqttclient*)(uintptr_t)1,"volatile ownership rejected with unchanged output");
        W.WebSocket.Session=NULL;
        xrtClearError();
        testRequire(xmqttClientWsConnectUriStore(P->Base.Engine,NULL,U,1,&K,&W,Session,Store,&P->Base.Connect,&Client),
                    "public WS URI durable factory");
    }
    memset(URI,'!',sizeof(URI));
    return Client;
}

#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static xmqttauthticket* dwTicket(xmqttclient* Client,xmqttauthphase Phase)
{
    double End=xrtTimer()+10;
    xmqttauthticket* Ticket=NULL;
    while(!xmqttClientTakeAuth(Client,&Ticket)){
        testRequire(xrtTimer()<End,"finite durable WS auth ticket wait");
        xrtSleep(1);
    }
    testRequire(xmqttAuthPhase(Ticket)==Phase,"expected enhanced-auth phase");
    return Ticket;
}
static void dwProof(xmqttclient* Client,xmqttstorewrite* Writer)
{
    xmqttauthticket* Ticket=dwTicket(Client,XMQTT_AUTH_CHALLENGE);
    xmqttpacket Reply;
    xmqttPacketInit(XMQTT_PACKET_AUTH,&Reply);
    Reply.Reason=0x18;
    xmqttproperty Props[]={
        {.Id=XMQTT_PROPERTY_AUTH_METHOD,.Type=XMQTT_PROPERTY_STRING,.Value.Text=XRT_STR_LITERAL("m")},
        {.Id=XMQTT_PROPERTY_AUTH_DATA,.Type=XMQTT_PROPERTY_BINARY,.Value.Binary=XRT_BYTES_LITERAL("D")}
    };
    Reply.Properties=(xmqttpropertylist){Props,2};
    testRequire(xmqttAuthRespond(Ticket,&Reply)==XMQTT_ADMISSION_ACCEPTED,"durable WSS challenge reply");
    xmqttAuthRelease(Ticket);
    Ticket=dwTicket(Client,XMQTT_AUTH_VERIFY_CONNECT);
    testRequire(!swInfo(Writer).State.Transactions&&!swInfo(Writer).State.Inbox,"WSS proof precedes any journal mutation");
    testRequire(xmqttAuthComplete(Ticket,true)==XMQTT_ADMISSION_ACCEPTED,"accept WSS fixture proof");
    xmqttAuthRelease(Ticket);
}
#endif

static void dwDelivery(xmqttclient* Client,xmqttstorewrite* Writer,xmqttversion Version)
{
    double Deadline=xrtTimer()+10;
    xmqttmessage* Message=NULL;
    while(!xmqttClientTakeMessage(Client,&Message)){
        testRequire(xrtTimer()<Deadline,"coalesced publication eventually visible");
        xrtSleep(1);
    }
    xbytesview Wire=xmqttMessagePacket(Message)->Wire;
    const unsigned char V5[]={0x30,8,0,2,'i','n',0,'x',0,'z'};
    const unsigned char V311[]={0x30,7,0,2,'i','n','x',0,'z'};
    testRequire(Version==XMQTT_VERSION_5?Wire.Size==sizeof(V5)&&!memcmp(Wire.Data,V5,Wire.Size):
                Wire.Size==sizeof(V311)&&!memcmp(Wire.Data,V311,Wire.Size),"exact WS/MQTT binary payload");
    xmqttMessageRelease(Message);
    Message=NULL;
    xmqttstorewriteinfo Info=swInfo(Writer);
    testRequire(!xmqttClientTakeMessage(Client,&Message)&&Info.State.Transactions==1&&!Info.State.Inbox,
                "one APP delivery and only OPEN journaled");
}

static void dwPublish(xmqttclient* Client,uint64 Attempt)
{
    xmqttpacket P;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    P.Flags=2;
    P.Identifier=9;
    P.Body.Publish.Topic=XRT_STR_LITERAL("ws");
    P.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");
    xfuture* Future=NULL;
    testRequire(xmqttClientPublishAsync(Client,&P,10000,&Future)==XMQTT_ADMISSION_ACCEPTED,"store WS QoS1 admitted");
    const xmqttwriteresult* R=mqttWsFuture(Future);
    xmqttclientstoreoutcome Effect;
    testRequire(R->Status==XMQTT_STATUS_OK&&R->Delivery==XMQTT_PEER_ACCEPTED&&xmqttClientStoreOutcome(R,&Effect)&&
                Effect.Attempt.Sequence==Attempt&&Effect.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&
                Effect.Completion.Sequence==Attempt+1&&Effect.Completion.Effect==XMQTT_JOURNAL_COMMITTED,
                "peer ACK and both WS publication checkpoints");
    xrtFutureDestroy(Future);
}

static void dwReplay(jpfile File,const xmqttjournalconfig* JC,xmqttstorestateconfig* SC,
    uint64 Count,bool Live,bool Graceful)
{
    xmqttjournal* Journal=jpReopen(File,JC);
    xmqttjournalrecord Record;
    for(uint64 I=1;I<=Count;++I)
        jpStatus(xmqttJournalNext(Journal,&Record),XMQTT_JOURNAL_DONE,"physical WebSocket journal transaction");
    if(Count&&!Live){
        xmqttstoredelta End;
        xmqttstoredeltadocument Doc;
        sdStatus(xmqttStoreDeltaRead(&SC->Delta,Record.Data,&End,1,NULL,0,&Doc),
                 XMQTT_STORE_DELTA_DONE,"physical WebSocket END");
        bool V5=SC->Delta.Version==XMQTT_VERSION_5;
        testRequire(Doc.Count==1&&End.Type==XMQTT_STORE_END&&End.Flags==(Graceful?1u:0u)&&
                    End.Reason==(V5&&Graceful?4u:0u)&&End.Expiry==(V5?17u:UINT32_MAX)&&End.WallMilliseconds==200000,
                    "END records actual native closure and broker expiry facts");
    }
    jpStatus(xmqttJournalNext(Journal,&Record),XMQTT_JOURNAL_EMPTY,"exact durable WS prefix");
    testRequire(xmqttJournalDestroy(Journal),"physical journal cursor released");
    Journal=jpReopen(File,JC);
    xmqttstorestate* Recovered=NULL;
    SC->WallMilliseconds=200001;
    stStatus(xmqttStoreStateRecover(Journal,SC,&Recovered),XMQTT_STORE_DONE,"recover WebSocket ledger");
    xmqttstorestateinfo Info=stInfo(Recovered);
    testRequire(Info.Transactions==Count&&Info.LiveAtCut==Live&&!Info.Outgoing&&!Info.Inbox,"exact recovered WS transport cut");
    xmqttStoreStateDestroy(Recovered);
    testRequire(xmqttJournalDestroy(Journal),"recovered journal cursor released");
}

static void dwCase(xmqttversion Version,bool TLS,dwmode Mode)
{
    bool V5=Version==XMQTT_VERSION_5;
    bool Coalesced=Mode==DW_NORMAL||Mode==DW_AUTH;
    jwdrive Drive={0};
    str Path=NULL;
    if(Mode==DW_NORMAL){
        Drive.Base.Native=xrtFileTemp(NULL,"xmqtt-durable-ws-",NULL,&Path);
        testRequire(Drive.Base.Native!=NULL,"real native WS journal file");
    }
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(Version);
    xmqttjournal* Journal=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"empty store behind HTTP and MQTT");
    xtaskpool* Pool=jwPool(1);
    xmqttstorewriteconfig WC;
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreWriteConfigInit(&WC)&&WC.ProtocolOnly&&
        xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer),"protocol-only WS Writer");
    Drive.Base.Calls=0;
    if(Coalesced)Drive.Hold=7;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"store WS Session");
    dwpeer Case={0};
    Case.Writer=Writer;
    Case.Mode=Mode;
    mqttWsConnectPeer* Net=&Case.Net;
    mqttWsConnectInit(Net,TLS,Version,XNET_PORT_AUTO);
    if(Mode==DW_HTTP_REJECT)Net->Mode=3;
    Net->Base.Control=dwFrame;
    Net->Base.Coalesced=Coalesced;
    Net->Base.Auth=Mode==DW_AUTH;
    Net->Base.Connect.Body.Connect.ClientId=SK.ClientId;
    Net->Base.Connect.Body.Connect.CleanStart=false;
    xmqttproperty Props[]={
        {.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60},
        {.Id=XMQTT_PROPERTY_AUTH_METHOD,.Type=XMQTT_PROPERTY_STRING,.Value.Text=XRT_STR_LITERAL("m")}
    };
    if(V5)Net->Base.Connect.Properties=(xmqttpropertylist){Props,Mode==DW_AUTH?2u:1u};
    uint64 Wall=200000;
    xmqttclientstoreconfig Store={sizeof(Store),Writer,dwWall,&Wall};
    xmqttclient* Client=dwConnect(&Case,Session,&Store);
    xfuture* Ready=xmqttClientReady(Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(Mode==DW_AUTH)dwProof(Client,Writer);
#endif
    if(Coalesced){
        jwAwait(&Drive.Entered);
        xmqttsessioninfo Info;
        xmqttmessage* Early=NULL;
        testRequire(xmqttSessionInfo(Session,&Info)&&!Info.Connected&&xrtFutureState(Ready)==XFUTURE_PENDING&&
                    !swInfo(Writer).State.Transactions&&!xmqttClientTakeMessage(Client,&Early),
                    "coalesced MQTT input remains ordered behind physical OPEN");
        xrtAtomic32Store(&Drive.Release,1,XMEMORY_RELEASE);
    }
    const xmqttreadyresult* Result=mqttWsFuture(Ready);
    uint64 Records=0;
    bool Graceful=false,Live=false;
    xfuture* Closing=NULL;
    if(Mode==DW_HTTP_REJECT){
        xmqttclientwsconnectinfo Http={.Size=sizeof(Http)};
        testRequire(Result->Status==XMQTT_STATUS_REJECTED&&xmqttClientWsConnectInfo(Client,&Http)&&
                    Http.Status==401&&!Http.Upgraded&&!Drive.Base.Calls,"HTTP rejection produces no journal operation");
    }else{
        testRequire(Result->Status==XMQTT_STATUS_OK,"durable WS is Ready");
        if(Coalesced)dwDelivery(Client,Writer,Version);
        dwPublish(Client,2);
        xmqttpacket Disconnect;
        xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Disconnect);
        if(V5){
            Props[0].Value.Number=17;
            Disconnect.Reason=4;
            Disconnect.Properties=(xmqttpropertylist){Props,1};
        }
        testRequire(xmqttClientDisconnectAsync(Client,&Disconnect,10000,&Closing)==XMQTT_ADMISSION_ACCEPTED,
                    "WS application DISCONNECT admitted");
        const xmqttwriteresult* End=mqttWsFuture(Closing);
        Graceful=Mode!=DW_TRUNCATE;
        Live=!Graceful&&V5;
        Records=Live?3:4;
        testRequire(End->Status==(Graceful?XMQTT_STATUS_OK:XMQTT_STATUS_TRANSPORT)&&
                    End->AcceptedBytes==(V5?9u:2u)&&End->Delivery==XMQTT_MAY_HAVE_REACHED_PEER,
                    "native WS close determines local status after complete MQTT DISCONNECT");
    }
    testRequire(xmqttClientWaitFor(Client,15000)==XWAIT_OK&&xmqttClientDestroy(Client),"physical WS and store retirement");
    mqttWsConnectStop(Net);
    if(Closing){
        xmqttclientstoreoutcome Effect;
        testRequire(xrtAtomic32Load(&Net->Base.Disconnects,XMEMORY_ACQUIRE)==1&&
                    xmqttClientStoreOutcome(xrtFutureValue(Closing),&Effect)&&
                    Effect.End.Effect==(Live?XMQTT_JOURNAL_NOT_COMMITTED:XMQTT_JOURNAL_COMMITTED)&&
                    Effect.End.Sequence==(Live?0u:Records),"retained close report describes actual END effect");
    }
    xrtFutureDestroy(Closing);
    xrtFutureDestroy(Ready);
    testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(Writer),"exclusive owners released after Client");
    jwPoolDone(Pool);
    dwReplay(File,&JC,&SC,Records,Live,Graceful);
    jpFileClose(File);
    if(Path){
        testRequire(xrtClose(Drive.Base.Native)&&xrtFileDelete(Path),"native WS fixture file retired");
        xrtFree(Path);
    }
}

int main(void)
{
    const xmqttversion Versions[]={XMQTT_VERSION_5,XMQTT_VERSION_311};
    for(size_t I=0;I<sizeof(Versions)/sizeof(*Versions);++I){
        for(unsigned M=DW_NORMAL;M<=DW_TRUNCATE;++M)dwCase(Versions[I],false,(dwmode)M);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        for(unsigned M=DW_NORMAL;M<=DW_TRUNCATE;++M)dwCase(Versions[I],true,(dwmode)M);
#endif
    }
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS) && defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    dwCase(XMQTT_VERSION_5,true,DW_AUTH);
#endif
    puts("[PASS] WebSocket storage gates Upgrade, coalesced input, outgoing PUBACK and exact graceful or truncated END");
    return 0;
}
