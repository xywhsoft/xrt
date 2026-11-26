#include <xmqtt/client_store.h>
#include <xmqtt/store_session.h>
#include "support/store_write_peer.h"
#include "../examples/common/auth_peer.h"
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
#include <xmqtt/client_store_tls.h>
#include "support/tls_peer.h"
#endif

typedef enum saMode {SA_EARLY,SA_REJECT,SA_CANCEL,SA_CANCEL_OPEN,SA_TIMEOUT,SA_HEARTBEAT} saMode;
typedef enum sa_dial {SA_NUMERIC,SA_HOSTNAME,SA_PREPARED} sa_dial;
typedef struct saPeer {
    mqttAuthPeer TCP;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetresolver* Resolver;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    mqttTlsPeer TLS;
    bool Challenged,Open,Early;
#endif
} saPeer;

static bool saWall(ptr User,uint64* Time)
{*Time=*(const uint64*)User;return true;}

#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
static void saTlsFrame(xtlsstream* S,mqttTlsPeer* Net,unsigned char Flags,const unsigned char* B,size_t N)
{
    saPeer* P=Net->FrameData;
    if(Flags==0x10){
        static const unsigned char Challenge[]={0xf0,10,0x18,8,0x15,0,1,'m',0x16,0,1,'B'};
        testRequire(!P->Challenged&&N>=10,"TLS CONNECT starts authentication");
        P->Challenged=true;
        mqttTlsSend(S,Challenge,sizeof(Challenge));
    }else if(Flags==0xf0){
        static const unsigned char Answer[]={0x18,8,0x15,0,1,'m',0x16,0,1,'D'};
        static const unsigned char Start[]={0x19,8,0x15,0,1,'m',0x16,0,1,'D'};
        static const unsigned char Final[]={0xf0,10,0,8,0x15,0,1,'m',0x16,0,1,'C'};
        static const unsigned char Ready[]={0x20,11,0,0,8,0x15,0,1,'m',0x16,0,1,'C',
            0x32,13,0,5,'e','a','r','l','y',0,77,0,'y','e','s'};
        if(!P->Open){
            testRequire(N==sizeof(Answer)&&!memcmp(B,Answer,N),"literal TLS challenge reply");
            P->Open=true;
            mqttTlsSend(S,Ready,P->Early?sizeof(Ready):13);
        }else{
            testRequire(N==sizeof(Start)&&!memcmp(B,Start,N),"literal TLS reauthentication");
            mqttTlsSend(S,Final,sizeof(Final));
        }
    }else mqttTlsFrame(S,Net,Flags,B,N);
}
#endif

static xmqttclient* saConnect(saPeer* P,unsigned Transport,saMode Mode,sa_dial DialMode,xmqttsession* Session,
    const xmqttclientstoreconfig* Store,const xmqttpacket* Q)
{
    xmqttauthconfig Auth;
    xmqttAuthConfigInit(&Auth);
    Auth.ResponseTimeout=Mode==SA_TIMEOUT?400:8000;
    xmqttclientconfig K;
    xmqttClientConfigInit(&K);
    K.SetupTimeout=12000;
    K.PingTimeout=300;
    xmqttclient* Client=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    if(Transport){
        P->Early=Mode==SA_EARLY;
        mqttTlsStartWith(&P->TLS,XMQTT_VERSION_5,0,saTlsFrame,P);
        xmqttclienttlsconfig TLS;
        xtlsverifier* V;
        char Name[12],Alpn[4];
        xstrview Protocol;
        V=mqttTlsConfigure(&P->TLS,&TLS,Name,&Protocol,Alpn);
        bool OK;
        if(Transport==1)OK=xmqttClientTlsConnectStoreAuth(P->TLS.Engine,&P->TLS.Address,1,&K,&TLS,&Auth,Session,Store,Q,&Client);
        else if(Transport==2){
            TLS.Session.ServerName=(xstrview){0};
            OK=xmqttClientTlsDialStoreAuth(P->TLS.Engine,P->TLS.Resolver,XRT_STR_LITERAL("example.com"),
                                           P->TLS.Address.Port,1,&K,&TLS,&Auth,Session,Store,Q,&Client);
        }else{
            xnetaddrlist* L=xrtNetAddrListCreate(&P->TLS.Address,1);
            testRequire(L!=NULL,"prepared TLS endpoints created");
            OK=xmqttClientTlsDialAddressesStoreAuth(P->TLS.Engine,L,1,&K,&TLS,&Auth,Session,Store,Q,&Client);
            xrtNetAddrListDestroy(L);
        }
        xrtTlsVerifierRelease(V);
        testRequire(OK,"durable TLS Auth factory");
        return Client;
    }
#else
    (void)Transport;
#endif
    mqttAuthStart(&P->TCP,Mode==SA_EARLY?MQTT_AUTH_EARLY:0);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(DialMode==SA_HOSTNAME){
        P->Resolver=xrtNetResolverCreate(NULL);
        xmqttclientdialconfig Dial;
        xmqttClientDialConfigInit(&Dial);
        Dial.Family=XNET_FAMILY_IPV4;
        testRequire(P->Resolver&&xmqttClientDialStoreAuth(P->TCP.Engine,P->Resolver,XRT_STR_LITERAL("localhost"),
            P->TCP.Address.Port,1,&K,&Dial,&Auth,Session,Store,Q,&Client),"durable hostname Auth factory");
    }else if(DialMode==SA_PREPARED){
        xnetaddrlist* List=xrtNetAddrListCreate(&P->TCP.Address,1);
        testRequire(List&&xmqttClientDialAddressesStoreAuth(P->TCP.Engine,List,1,&K,NULL,&Auth,Session,Store,Q,&Client),
            "durable prepared-address Auth factory");
        xrtNetAddrListDestroy(List);
    }else
#else
    (void)DialMode;
#endif
    testRequire(xmqttClientConnectStoreAuth(P->TCP.Engine,&P->TCP.Address,1,&K,&Auth,Session,Store,Q,&Client),
                "durable TCP Auth factory");
    return Client;
}

static xmqttpacket saAuthPacket(xmqttpackettype Type,uint8 Reason,xmqttproperty Props[2],unsigned Transport)
{
    xmqttpacket Q=mqttAuthPacket(Type,Reason,Props,false);
    if(Transport){
        Props[0].Value.Text=XRT_STR_LITERAL("m");
        Props[1].Value.Binary=XRT_BYTES_LITERAL("D");
    }
    return Q;
}

static bool saProof(const xmqttauthticket* T,unsigned Transport,bool Challenge)
{
    const xmqttpacket* P=xmqttMessagePacket(xmqttAuthMessage(T));
    const unsigned char Plain[]={'p',0,255,'f'};
    const unsigned char TLS[]={Challenge?'B':'C'};
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_DATA){
        xbytesview Data=P->Properties.Data[I].Value.Binary;
        return Transport?Data.Size==sizeof(TLS)&&!memcmp(Data.Data,TLS,Data.Size):
                         Data.Size==sizeof(Plain)&&!memcmp(Data.Data,Plain,Data.Size);
    }
    return false;
}

static xmqttauthticket* saFinal(xmqttclient* C,unsigned Transport,xmqttauthphase Phase)
{
    for(unsigned Round=0;Round<3;++Round){
        xmqttauthticket* T=mqttAuthTicket(C);
        bool Challenge=xmqttAuthPhase(T)==XMQTT_AUTH_CHALLENGE;
        testRequire(saProof(T,Transport,Challenge),"actual binary auth data before application decision");
        if(!Challenge){
            testRequire(xmqttAuthPhase(T)==Phase,"expected final proof phase");
            return T;
        }
        xmqttproperty Props[2];
        xmqttpacket Reply=saAuthPacket(XMQTT_PACKET_AUTH,0x18,Props,Transport);
        testRequire(xmqttAuthRespond(T,&Reply)==XMQTT_ADMISSION_ACCEPTED,"bounded challenge response");
        xmqttAuthRelease(T);
    }
    testRequire(false,"finite fixture auth rounds");
    return NULL;
}

static const void* saResult(xfuture* F)
{
    testRequire(F&&xrtFutureWaitFor(F,12000)==XWAIT_OK&&xrtFutureState(F)==XFUTURE_RESOLVED,"typed settled outcome");
    return xrtFutureValue(F);
}

static void saDelivery(xmqttclient* Client,xmqttstorewrite* Writer,saPeer* Peer,unsigned Transport)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    if(Transport)mqttTlsWait(&Peer->TLS.ReceivedAcks,1,"TLS coalesced publication acknowledged");
    else
#else
    (void)Transport;
#endif
    mqttAuthWait(&Peer->TCP.Acks,1);
    xmqttmessage* Message=NULL;
    testRequire(xmqttClientTakeMessage(Client,&Message),"post-proof APP delivery");
    const xmqttpacket* P=xmqttMessagePacket(Message);
    xbytesview Payload=Transport?XRT_BYTES_LITERAL("yes"):XRT_BYTES_LITERAL("quarantine");
    testRequire(P&&P->Type==XMQTT_PACKET_PUBLISH&&P->Identifier==77&&P->Body.Publish.Payload.Size==Payload.Size&&
                !memcmp(P->Body.Publish.Payload.Data,Payload.Data,Payload.Size),"exact early delivery after OPEN");
    xmqttMessageRelease(Message);
    Message=NULL;
    xmqttstorewriteinfo Info=swInfo(Writer);
    testRequire(!xmqttClientTakeMessage(Client,&Message)&&Info.State.Transactions==1&&!Info.State.Inbox,
                "one APP delivery and only OPEN journaled");
}

static void saReauth(xmqttclient* C,xmqttstorewrite* W,unsigned Transport)
{
    uint64 Before=swInfo(W).State.Transactions;
    xmqttproperty Props[2];
    xmqttpacket Q=saAuthPacket(XMQTT_PACKET_AUTH,0x19,Props,Transport);
    xfuture* Work=NULL;
    testRequire(xmqttClientReauthenticateAsync(C,&Q,5000,&Work)==XMQTT_ADMISSION_ACCEPTED,"reauth after committed readiness");
    xmqttauthticket* T=saFinal(C,Transport,XMQTT_AUTH_VERIFY_REAUTH);
    testRequire(xmqttAuthComplete(T,true)==XMQTT_ADMISSION_ACCEPTED,"reauth proof accepted");
    xmqttAuthRelease(T);
    const xmqttauthresult* Result=saResult(Work);
    testRequire(Result->Status==XMQTT_STATUS_OK&&Result->AcceptedBytes&&
                swInfo(W).State.Transactions==Before,"reauth keeps its original durable epoch");
    xrtFutureDestroy(Work);
}

static void saReplay(xmqttjournal* Journal,uint64 Count,bool Fresh)
{
    xmqttjournalrecord R;
    const unsigned char Token[]={'a',0,255,'z'},Proof[]={'p',0,255,'f'};
    for(uint64 I=1;I<=Count;++I){
        jpStatus(xmqttJournalNext(Journal,&R),XMQTT_JOURNAL_DONE,"read physical auth fixture transaction");
        testRequire(R.Sequence==I,"contiguous durable history");
        const unsigned char* Bytes=R.Data.Data;
        for(size_t J=0;J+sizeof(Token)<=R.Data.Size;++J)
            testRequire(memcmp(Bytes+J,Token,sizeof(Token))&&memcmp(Bytes+J,Proof,sizeof(Proof)),
                        "CONNECT token and final proof are absent from journal payload");
        if(Fresh&&I==1){
            xmqttstoredeltaconfig C=sdConfig(XMQTT_VERSION_5);
            xmqttstoredelta Ops[2];
            xmqttstoredeltadocument Doc;
            sdStatus(xmqttStoreDeltaRead(&C,R.Data,Ops,2,NULL,0,&Doc),XMQTT_STORE_DELTA_DONE,"decode first durable transaction");
            testRequire(Doc.Count==2&&Ops[0].Type==XMQTT_STORE_BIND&&Ops[1].Type==XMQTT_STORE_OPEN&&
                        !Ops[1].Data.Size,"OPEN includes protocol facts only");
        }
    }
    jpStatus(xmqttJournalNext(Journal,&R),XMQTT_JOURNAL_EMPTY,"no concealed authentication record");
}

static void saCase(saMode Mode,unsigned Transport,sa_dial DialMode)
{
    const bool Success=Mode==SA_EARLY||Mode==SA_HEARTBEAT;
    const bool Previous=Mode==SA_REJECT;
    const bool LateCancel=Mode==SA_CANCEL_OPEN;
    const bool Accept=Success||LateCancel;
    jwdrive Drive={0};
    str Path=NULL;
    if(Mode==SA_EARLY&&!Transport){
        Drive.Base.Native=xrtFileTemp(NULL,"xmqtt-auth-store-",NULL,&Path);
        testRequire(Drive.Base.Native!=NULL,"real file for auth-gated OPEN/END");
    }
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(XMQTT_VERSION_5);
    xmqttstorestateconfig SC=stConfig(XMQTT_VERSION_5);
    SC.WallMilliseconds=1250;
    xmqttjournal* Journal;
    if(Previous){
        sdsource Source;
        sdSource(XMQTT_VERSION_5,&Source);
        Source.Ops[3]=(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.Expiry=60,.WallMilliseconds=1002};
        Journal=stWrite(File,XMQTT_VERSION_5,Source.Ops,4,true);
    }else Journal=jpCreate(File,&JC);
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"known auth fixture prefix");
    uint64 Before=stInfo(State).Transactions;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    if(Previous){
        xmqttstoresessionconfig Import;
        xmqttStoreSessionConfigInit(&Import);
        Import.Binding=SC.Binding;
        Import.Generation=SC.Generation;
        Import.MonotonicTime=xrtTimer();
        Import.WallMilliseconds=SC.WallMilliseconds;
        testRequire(xmqttStoreSessionRestore(State,&SK,&Import,&Session)==XMQTT_SESSION_DONE,
                    "restore previous sent obligation before auth rejection");
    }else testRequire(xmqttSessionCreate(&SK,&Session),"fresh auth fixture Session");
    xtaskpool* Pool=jwPool(1);
    xmqttstorewriteconfig WC;
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreWriteConfigInit(&WC)&&
        xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer),"protocol-only auth Writer");
    Drive.Base.Calls=0;
    if(Accept)Drive.Hold=7;

    uint64 Wall=1500;
    xmqttclientstoreconfig Store={sizeof(Store),Writer,saWall,&Wall};
    xmqttproperty P[3];
    xmqttpacket Connect=saAuthPacket(XMQTT_PACKET_CONNECT,0,P,Transport);
    Connect.Body.Connect.ClientId=SC.ClientId;
    Connect.Body.Connect.CleanStart=false;
    if(Mode==SA_HEARTBEAT)Connect.Body.Connect.KeepAlive=1;
    P[2]=(xmqttproperty){.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
    Connect.Properties.Count=3;
    saPeer Peer={0};
    xmqttclient* C=saConnect(&Peer,Transport,Mode,DialMode,Session,&Store,&Connect);
    xfuture* Ready=xmqttClientReady(C);
    xmqttauthticket* T=saFinal(C,Transport,XMQTT_AUTH_VERIFY_CONNECT);
    if(Mode==SA_HEARTBEAT)mqttAuthWait(&Peer.TCP.Pings,2);
    xmqttsessioninfo SessionInfo;
    testRequire(xmqttSessionInfo(Session,&SessionInfo)&&!SessionInfo.Connected&&
                SessionInfo.ActiveOutgoing==(Previous?1u:0u)&&xrtFutureState(Ready)==XFUTURE_PENDING&&
                swInfo(Writer).State.Transactions==Before&&swInfo(Writer).State.Inbox==0,
                "unaccepted CONNACK leaves offline protocol and Inbox intact");
    if(Mode==SA_CANCEL)testRequire(xrtFutureCancel(Ready),"Ready cancellation before proof");
    else if(Mode!=SA_TIMEOUT)
        testRequire(xmqttAuthComplete(T,Accept)==XMQTT_ADMISSION_ACCEPTED,"publish explicit initial proof decision");
    xmqttAuthRelease(T);
    if(Accept){
        jwAwait(&Drive.Entered);
        if(Mode==SA_HEARTBEAT)mqttAuthWait(&Peer.TCP.Pings,4);
        xmqttmessage* Early=NULL;
        testRequire(xmqttSessionInfo(Session,&SessionInfo)&&!SessionInfo.Connected&&
                    xrtFutureState(Ready)==XFUTURE_PENDING&&!swInfo(Writer).State.Transactions&&
                    !xmqttClientTakeMessage(C,&Early),"accepted proof waits behind actual OPEN and quarantines coalesced frames");
        if(LateCancel)testRequire(xrtFutureCancel(Ready)&&xmqttClientPollRetired(C)==XWAIT_TIMEOUT,
                                  "cancel accepted proof while physical OPEN sync is held");
        xrtAtomic32Store(&Drive.Release,1,XMEMORY_RELEASE);
    }
    const xmqttreadyresult* R=saResult(Ready);
    xmqttclientstatus Expected=Success?XMQTT_STATUS_OK:(Mode==SA_CANCEL||LateCancel)?XMQTT_STATUS_CANCELLED:
        Mode==SA_TIMEOUT?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_AUTHENTICATION;
    testRequire(R->Status==Expected,"Ready distinguishes proof, cancel and timeout");
    if(Success){
        if(Mode==SA_EARLY)saDelivery(C,Writer,&Peer,Transport);
        saReauth(C,Writer,Transport);
        xfuture* Closing=NULL;
        testRequire(xmqttClientDisconnectAsync(C,NULL,10000,&Closing)==XMQTT_ADMISSION_ACCEPTED,"auth-gated session graceful close");
        const xmqttwriteresult* End=saResult(Closing);
        xmqttclientstoreoutcome Effect;
        testRequire(End->Status==XMQTT_STATUS_OK&&xmqttClientStoreOutcome(End,&Effect)&&
                    Effect.End.Effect==XMQTT_JOURNAL_COMMITTED,"auth-gated session END");
        xrtFutureDestroy(Closing);
    }
    testRequire(xmqttClientWaitFor(C,12000)==XWAIT_OK&&xmqttClientDestroy(C),"auth tickets and native storage fully retired");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    if(Transport)mqttTlsStop(&Peer.TLS);
    else
#endif
    mqttAuthStop(&Peer.TCP);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Peer.Resolver)testRequire(xrtNetResolverDestroy(Peer.Resolver),"retire Auth hostname resolver");
#endif
    uint64 After=Accept?2u:Before;
    testRequire(swInfo(Writer).State.Transactions==After&&(Accept||!Drive.Base.Calls),"failed proof never appends");
    testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(Writer),"auth fixture owners released");
    jwPoolDone(Pool);
    if(Success)testRequire(R->Negotiated.AuthMethod.Size==(Transport?1u:7u),"retained final proof metadata");
    xrtFutureDestroy(Ready);
    Journal=jpReopen(File,&JC);
    saReplay(Journal,After,Accept);
    testRequire(xmqttJournalDestroy(Journal),"physical journal reader retired");
    if(Previous){
        Journal=jpReopen(File,&JC);
        SC.WallMilliseconds=Wall;State=NULL;
        stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"reauth-free prior state recovery");
        testRequire(stInfo(State).Epoch==1&&stInfo(State).Outgoing==1,"refused proof preserves prior epoch and sent obligation");
        xmqttStoreStateDestroy(State);
        testRequire(xmqttJournalDestroy(Journal),"recovered rejected-proof journal retired");
    }
    jpFileClose(File);
    if(Path){
        testRequire(xrtClose(Drive.Base.Native)&&xrtFileDelete(Path),"actual auth fixture file retired");
        xrtFree(Path);
    }
}

int main(void)
{
    saCase(SA_EARLY,0,SA_NUMERIC);
    saCase(SA_REJECT,0,SA_NUMERIC);
    saCase(SA_CANCEL,0,SA_NUMERIC);
    saCase(SA_CANCEL_OPEN,0,SA_NUMERIC);
    saCase(SA_HEARTBEAT,0,SA_NUMERIC);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    saCase(SA_EARLY,0,SA_HOSTNAME);
    saCase(SA_REJECT,0,SA_PREPARED);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    saCase(SA_EARLY,1,SA_NUMERIC);
    saCase(SA_TIMEOUT,2,SA_NUMERIC);
    saCase(SA_EARLY,3,SA_NUMERIC);
#endif
    puts("[PASS] durable enhanced auth gates OPEN, coalesced PUBLISH, reauthentication, heartbeat, refusal/cancellation and recovered state");
    return 0;
}
