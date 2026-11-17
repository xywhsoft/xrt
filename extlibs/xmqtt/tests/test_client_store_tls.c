#include <xmqtt/client_store_tls.h>
#include "support/store_write_peer.h"
#include "support/tls_peer.h"

typedef enum tlsStoreMode {TLS_STORE_NORMAL,TLS_STORE_CERT_REJECT,TLS_STORE_TRUNCATE,TLS_STORE_HOLD_OPEN} tlsStoreMode;
typedef struct tlsStorePeer {xmqttstorewrite* Writer;tlsStoreMode Mode;} tlsStorePeer;

static void tsFrame(xtlsstream* Stream,mqttTlsPeer* Peer,unsigned char Flags,const unsigned char* Data,size_t Size)
{
    const tlsStorePeer* Case=Peer->FrameData;
    if(Flags==0x10){
        testRequire(Case->Mode!=TLS_STORE_CERT_REJECT,"rejected TLS identity never sees MQTT CONNECT");
        testRequire(swInfo(Case->Writer).State.Transactions==0,"TLS establishment precedes durable OPEN");
    }else if(Flags==0x32){
        testRequire(swInfo(Case->Writer).State.Transactions==2,"durable attempt precedes TLS plaintext PUBLISH");
        const unsigned char Expected[]={0,3,'t','l','s',0,9};
        testRequire(Size>=sizeof(Expected)&&!memcmp(Data,Expected,sizeof(Expected)),"literal TLS PUBLISH topic and id");
    }else if(Flags==0xe0){
        testRequire(swInfo(Case->Writer).State.Transactions==3,"MQTT DISCONNECT precedes END on TLS");
        const unsigned char End5[]={4,5,0x11,0,0,0,17};
        testRequire(Peer->Version==XMQTT_VERSION_5?Size==sizeof(End5)&&!memcmp(Data,End5,Size):Size==0,
                    "literal complete TLS DISCONNECT");
        if(Case->Mode==TLS_STORE_TRUNCATE){
            xrtAtomic32FetchAdd(&Peer->Disconnects,1,XMEMORY_RELEASE);
            testRequire(xrtTlsStreamAbort(Stream),"peer truncates TLS without close_notify");
            return;
        }
    }
    mqttTlsFrame(Stream,Peer,Flags,Data,Size);
}

static bool tsWall(ptr State,uint64* Wall)
{
    *Wall=xrtAtomic64Load(State,XMEMORY_ACQUIRE);
    return true;
}

static const void* tsResult(xfuture* Future)
{
    testRequire(Future&&xrtFutureWaitFor(Future,15000)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED,
                "resolved result through actual native retirement");
    return xrtFutureValue(Future);
}

static xmqttclient* tsConnect(mqttTlsPeer* Peer,xmqttsession* Session,const xmqttsessionconfig* SessionConfig,
    const xmqttclientstoreconfig* Store,unsigned Factory)
{
    xmqttclientconfig Config;
    xmqttClientConfigInit(&Config);
    Config.Version=Peer->Version;
    Config.WriteChunkBytes=7;
    xmqttclienttlsconfig TLS;
    char Name[12],Alpn[4];
    xstrview Protocol;
    xtlsverifier* Verifier=mqttTlsConfigure(Peer,&TLS,Name,&Protocol,Alpn);
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SessionConfig->ClientId;
    xmqttproperty Expiry={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
    if(Peer->Version==XMQTT_VERSION_5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    xmqttclient* C=(xmqttclient*)(uintptr_t)1;

    if(Factory==0){
        xmqttclienttlsconfig NoIdentity=TLS;
        NoIdentity.Session.ServerName=(xstrview){0};
        testRequire(!xmqttClientTlsConnectStore(Peer->Engine,&Peer->Address,1,&Config,&NoIdentity,Session,Store,&Connect,&C)&&
                    C==(xmqttclient*)(uintptr_t)1,"numeric factory requires identity and preserves output");
        xrtClearError();
        testRequire(xmqttClientTlsConnectStore(Peer->Engine,&Peer->Address,1,&Config,&TLS,Session,Store,&Connect,&C),
                    "verified durable numeric factory");
    }else if(Factory==1){
        char Host[]="example.com";
        TLS.Session.ServerName=(xstrview){0};
        testRequire(xmqttClientTlsDialStore(Peer->Engine,Peer->Resolver,(xstrview){Host,11},Peer->Address.Port,
                    1,&Config,&TLS,Session,Store,&Connect,&C),"verified durable hostname factory");
        memset(Host,0,sizeof(Host));
    }else{
        xnetaddrlist* List=xrtNetAddrListCreate(&Peer->Address,1);
        testRequire(List&&xmqttClientTlsDialAddressesStore(Peer->Engine,List,1,&Config,&TLS,Session,Store,&Connect,&C),
                    "verified durable prepared address factory");
        xrtNetAddrListDestroy(List);
    }
    xrtTlsVerifierRelease(Verifier);
    memset(Name,0,sizeof(Name));memset(Alpn,0,sizeof(Alpn));
    return C;
}

static void tsPublish(xmqttclient* Client)
{
    xmqttpacket Pub;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Pub);
    Pub.Flags=2;
    Pub.Identifier=9;
    Pub.Body.Publish.Topic=XRT_STR_LITERAL("tls");
    Pub.Body.Publish.Payload=XRT_BYTES_LITERAL("x\0z");
    xfuture* Future=NULL;
    testRequire(xmqttClientPublishAsync(Client,&Pub,10000,&Future)==XMQTT_ADMISSION_ACCEPTED,
                "bounded TLS publication accepted");
    const xmqttwriteresult* Value=tsResult(Future);
    xmqttclientstoreoutcome Outcome;
    testRequire(Value->Status==XMQTT_STATUS_OK&&Value->Delivery==XMQTT_PEER_ACCEPTED&&
                xmqttClientStoreOutcome(Value,&Outcome)&&Outcome.Epoch==1&&Outcome.Key==1&&
                Outcome.Attempt.Sequence==2&&Outcome.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&
                Outcome.Completion.Sequence==3&&Outcome.Completion.Effect==XMQTT_JOURNAL_COMMITTED,
                "verified TLS PUBLISH and broker PUBACK bracket native commits");
    xrtFutureDestroy(Future);
}

static void tsEnd(const xmqttjournalconfig* JC,xmqttstorestateconfig* SC,jpfile File,
    uint64 Records,bool Live,bool Graceful,uint32 Expiry)
{
    xmqttjournal* Journal=jpReopen(File,JC);
    xmqttjournalrecord Record;
    for(uint64 I=1;I<=Records;++I)
        jpStatus(xmqttJournalNext(Journal,&Record),XMQTT_JOURNAL_DONE,"TLS committed record");
    if(Records&&!Live){
        xmqttstoredelta End;
        xmqttstoredeltadocument Doc;
        sdStatus(xmqttStoreDeltaRead(&SC->Delta,Record.Data,&End,1,NULL,0,&Doc),
                 XMQTT_STORE_DELTA_DONE,"read TLS END");
        testRequire(Doc.Count==1&&End.Type==XMQTT_STORE_END&&End.Flags==(Graceful?1u:0u)&&
                    End.WallMilliseconds==200050&&End.Expiry==Expiry&&
                    End.Reason==(Graceful&&SC->Delta.Version==XMQTT_VERSION_5?4:0),
                    "TLS END records authenticated classification, effective expiry and native close wall");
    }
    testRequire(xmqttJournalDestroy(Journal),"TLS scan cursor retirement");
    Journal=jpReopen(File,JC);
    xmqttstorestate* State=NULL;
    SC->WallMilliseconds=200051;
    stStatus(xmqttStoreStateRecover(Journal,SC,&State),XMQTT_STORE_DONE,"recover exact TLS history");
    xmqttstorestateinfo Info=stInfo(State);
    testRequire(Info.Transactions==Records&&Info.LiveAtCut==Live&&Info.Outgoing==0,"TLS replay classifies actual cut");
    xmqttStoreStateDestroy(State);
    testRequire(xmqttJournalDestroy(Journal),"TLS recovered cursor retirement");
}

static void tsCase(xmqttversion Version,unsigned Factory,tlsStoreMode Mode)
{
    bool V5=Version==XMQTT_VERSION_5;
    jwdrive Drive={0};
    str Path=NULL;
    if(Mode==TLS_STORE_NORMAL){
        Drive.Base.Native=xrtFileTemp(NULL,"xmqtt-durable-tls-",NULL,&Path);
        testRequire(Drive.Base.Native!=NULL,"real durable TLS journal file");
    }
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(Version);
    xmqttjournal* Journal=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"empty durable TLS prefix");
    xtaskpool* Pool=jwPool(1);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer),"durable TLS Inbox Writer");
    Drive.Base.Calls=0;
    if(Mode==TLS_STORE_NORMAL)Drive.Hold=28;
    if(Mode==TLS_STORE_HOLD_OPEN)Drive.Hold=7;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"fresh durable TLS Session");
    mqttTlsPeer Peer;
    tlsStorePeer Case={Writer,Mode};
    unsigned Flags=Mode==TLS_STORE_CERT_REJECT?MQTT_TLS_REJECT:Mode==TLS_STORE_HOLD_OPEN?MQTT_TLS_BLOCK_DNS:0;
    mqttTlsStartWith(&Peer,Version,Flags,tsFrame,&Case);
    xatomic64 Wall;
    xrtAtomic64Init(&Wall,200000);
    xmqttclientstoreconfig Store={sizeof(Store),Writer,tsWall,&Wall};

    xmqttclient* Client=tsConnect(&Peer,Session,&SK,&Store,Factory);
    xfuture* Ready=xmqttClientReady(Client);
    if(Mode==TLS_STORE_HOLD_OPEN){
        mqttTlsWait(&Peer.DnsEntered,1,"hostname copied before independent lookup");
        testRequire(!swInfo(Writer).State.Transactions&&xrtFutureState(Ready)==XFUTURE_PENDING,"no OPEN during lookup");
        xrtAtomic32Store(&Peer.DnsGate,1,XMEMORY_RELEASE);
        jwAwait(&Drive.Entered);
        xmqttsessioninfo Info;
        testRequire(xmqttSessionInfo(Session,&Info)&&!Info.Connected&&xrtFutureState(Ready)==XFUTURE_PENDING&&
                    !swInfo(Writer).State.Transactions,"verified CONNACK still waits for OPEN sync");
        xrtAtomic32Store(&Drive.Release,1,XMEMORY_RELEASE);
    }
    const xmqttreadyresult* R=tsResult(Ready);
    uint64 Records=0;
    bool Live=false,Graceful=false;
    xfuture* Closing=NULL;
    const xmqttwriteresult* Closed=NULL;
    if(Mode==TLS_STORE_CERT_REJECT){
        testRequire(R->Status==XMQTT_STATUS_TRANSPORT&&!swInfo(Writer).State.Transactions&&!Drive.Base.Calls,
                    "certificate rejection leaves the journal untouched");
    }else{
        testRequire(R->Status==XMQTT_STATUS_OK&&swInfo(Writer).State.LiveAtCut&&swInfo(Writer).State.Transactions==1,
                    "verified TLS Ready follows committed OPEN");
        tsPublish(Client);
        xmqttpacket Q;
        xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Q);
        xmqttproperty Expiry={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=17};
        if(V5){Q.Reason=4;Q.Properties=(xmqttpropertylist){&Expiry,1};}
        xrtAtomic64Store(&Wall,200050,XMEMORY_RELEASE);
        testRequire(xmqttClientDisconnectAsync(Client,&Q,10000,&Closing)==XMQTT_ADMISSION_ACCEPTED,"durable TLS disconnect admission");
        if(Mode==TLS_STORE_NORMAL){
            jwAwait(&Drive.Entered);
            testRequire(xrtAtomic32Load(&Peer.Disconnects,XMEMORY_ACQUIRE)==1&&swInfo(Writer).State.Transactions==3&&
                        xrtFutureState(Closing)==XFUTURE_PENDING&&xmqttClientPollRetired(Client)==XWAIT_TIMEOUT,
                        "real TLS closure and pending durable END remain distinct");
            xrtAtomic32Store(&Drive.Release,1,XMEMORY_RELEASE);
        }
        Closed=tsResult(Closing);
        Graceful=Mode!=TLS_STORE_TRUNCATE;
        Live=!Graceful&&V5;
        Records=Live?3:4;
        testRequire(Closed->Status==(Graceful?XMQTT_STATUS_OK:XMQTT_STATUS_TRANSPORT)&&
                    Closed->AcceptedBytes==(V5?9u:2u)&&Closed->Delivery==XMQTT_MAY_HAVE_REACHED_PEER&&
                    xrtAtomic32Load(&Peer.Disconnects,XMEMORY_ACQUIRE)==1,
                    "TLS close outcome distinguishes complete plaintext from authenticated closure");
    }
    testRequire(xmqttClientWaitFor(Client,15000)==XWAIT_OK&&xmqttClientDestroy(Client),"TLS and store fully retired");
    testRequire(xrtAtomic32Load(&Peer.VerifierDrops,XMEMORY_ACQUIRE)==1,"TLS verifier physically released");
    testRequire(xrtAtomic32Load(&Peer.Lookups,XMEMORY_ACQUIRE)==(Factory==1?1u:0u),"only hostname dials use resolver");
    mqttTlsStop(&Peer);
    testRequire(xmqttSessionDestroy(Session)&&xmqttStoreWriteDestroy(Writer),"caller retains Session and Writer");
    jwPoolDone(Pool);
    if(Closed){
        xmqttclientstoreoutcome Outcome;
        testRequire(xmqttClientStoreOutcome(Closed,&Outcome)&&Outcome.Key==0&&Outcome.Epoch==(Live?0u:1u)&&
                    Outcome.End.Sequence==(Live?0u:4u)&&Outcome.End.Effect==(Live?XMQTT_JOURNAL_NOT_COMMITTED:XMQTT_JOURNAL_COMMITTED),
                    "TLS End physical outcome survives Client, TLS and Writer retirement");
    }
    xrtFutureDestroy(Ready);xrtFutureDestroy(Closing);
    tsEnd(&JC,&SC,File,Records,Live,Graceful,V5?17u:UINT32_MAX);
    jpFileClose(File);
    if(Path){
        testRequire(xrtClose(Drive.Base.Native)&&xrtFileDelete(Path),"remove owned TLS test file");
        xrtFree(Path);
    }
}

int main(void)
{
    const xmqttversion Versions[]={XMQTT_VERSION_5,XMQTT_VERSION_311};
    for(size_t I=0;I<sizeof(Versions)/sizeof(*Versions);++I){
        for(unsigned Factory=0;Factory<3;++Factory)tsCase(Versions[I],Factory,TLS_STORE_NORMAL);
        tsCase(Versions[I],1,TLS_STORE_CERT_REJECT);
        tsCase(Versions[I],2,TLS_STORE_TRUNCATE);
        tsCase(Versions[I],1,TLS_STORE_HOLD_OPEN);
    }
    puts("[PASS] durable numeric, hostname and prepared TLS; verified OPEN, PUBLISH/PUBACK, graceful END and truncated close recovery");
    return 0;
}
