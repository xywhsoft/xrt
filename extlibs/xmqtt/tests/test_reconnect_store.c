#include "support/store_write_peer.h"
#include "../examples/reconnect_options.h"
#include <xmqtt/reconnect_store.h>
#include <xmqtt/store_session.h>
struct rcConnection;
static bool drControl(struct rcConnection* Connection,const xmqttpacket* Packet);
#define RC_CONTROL drControl
#define main xmqttReconnectFixtureTests
#include "test_reconnect.c"
#undef main
#undef RC_CONTROL

typedef struct dr_combined {
    rcPeer Peer;
    xatomic32 Subscribes;
} dr_combined;

static bool drControl(rcConnection* Connection,const xmqttpacket* Packet)
{
    if(Packet->Type!=XMQTT_PACKET_SUBSCRIBE)return false;
    dr_combined* Peer=(dr_combined*)Connection->Peer;
    testRequire(Packet->Body.Topics.Count==1 &&
        Packet->Body.Topics.Data[0].Filter.Size==8 &&
        !memcmp(Packet->Body.Topics.Data[0].Filter.Data,"restored",8),
        "combined reconnect sends the requested filter");
    const unsigned char Reason=0;
    xmqttpacket Reply;
    xmqttPacketInit(XMQTT_PACKET_SUBACK,&Reply);
    Reply.Identifier=Packet->Identifier;
    Reply.Body.Reasons=(xbytesview){&Reason,1};
    xrtAtomic32FetchAdd(&Peer->Subscribes,1,XMEMORY_RELEASE);
    rcSend(Connection,&Reply);
    return true;
}

typedef enum drmode {DR_REPLAY,DR_LATE_OPEN,DR_UNCERTAIN_OPEN,DR_COLD} drmode;

typedef struct dr_factory {
    rcPeer* Peer;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetresolver* Resolver;
#endif
} dr_factory;

static xmqttclientstatus drFactory(xnetengine* Engine,uint64 Affinity,const xmqttclientconfig* Config,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,ptr User,xmqttclient** Client)
{
    dr_factory* Factory=User;
    rcPeer* Peer=Factory->Peer;
    xrtAtomic32FetchAdd(&Peer->FactoryCalls,1,XMEMORY_RELEASE);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Factory->Resolver)return xmqttClientDialStore(Engine,Factory->Resolver,XRT_STR_LITERAL("localhost"),
        Peer->Address.Port,Affinity,Config,NULL,Session,Store,Connect,Client)?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
#endif
    return xmqttClientConnectStore(Engine,&Peer->Address,Affinity,Config,Session,Store,Connect,Client)
        ?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
}

static bool drWall(ptr User,uint64* Milliseconds)
{
    (void)User;
    *Milliseconds=200000;
    return true;
}

static xmqttstorestateinfo drWaitState(xmqttstorewrite* Writer,uint64 Sequence)
{
    double Deadline=xrtTimer()+5;
    for(;;){
        xmqttstorestateinfo Info=swInfo(Writer).State;
        if(Info.Transactions>=Sequence)return Info;
        testRequire(xrtTimer()<Deadline,"committed store sequence deadline");
        xrtSleep(1);
    }
}

static void drWriterReserved(xmqttstorewrite* Writer)
{
    xfuture* Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(!xmqttStoreWriteClose(Writer)&&xmqttStoreWriteWaitFor(Writer,0)==XWAIT_ERROR&&
        !xmqttStoreWriteDestroy(Writer)&&
        xmqttStoreWriteAppendAsync(Writer,XRT_BYTES_LITERAL("x"),0,&Unchanged)==XMQTT_JWORK_ERROR&&
        Unchanged==(xfuture*)(uintptr_t)1,"controller reserves raw mutation and lifecycle");
    xrtClearError();
}

static void drReplay(rcPeer* Peer,xmqttreconnect* R,xmqttstorewrite* W,jwdrive* Drive)
{
    (void)rcOnline(R,1);
    testRequire(drWaitState(W,2).Inbox==1,"first Client committed its Inbox delivery");
    xmqttclientlease L={0};
    testRequire(xmqttReconnectAcquire(R,&L)&&L.Generation==1,"lease original durable Client");
    xmqttpacket P;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    P.Flags=2;
    P.Body.Publish.Topic=XRT_STR_LITERAL("work");
    P.Body.Publish.Payload=XRT_BYTES_LITERAL("replay");
    xfuture* Original=NULL;
    testRequire(xmqttClientPublishAsync(L.Client,&P,5000,&Original)==XMQTT_ADMISSION_ACCEPTED,"original QoS1 admission");
    rcWait(&Peer->Publishes,1);
    testRequire(xmqttClientWaitFor(L.Client,5000)==XWAIT_OK,"old transport and durable END retire");
    xmqttstorestateinfo State=drWaitState(W,4);
    testRequire(!State.LiveAtCut&&State.Outgoing==1,"old END retains durable outgoing obligation");
    xmqttreconnectinfo Info;
    testRequire(xmqttReconnectInfo(R,&Info)&&Info.State==XMQTT_RECONNECT_RETIRING&&
        xrtAtomic32Load(&Peer->FactoryCalls,XMEMORY_ACQUIRE)==1,"old lease fences the next factory");
    const xmqttwriteresult* Result=xrtFutureValue(Original);
    xmqttclientstoreoutcome Outcome;
    testRequire(Result&&Result->Status!=XMQTT_STATUS_OK&&xmqttClientStoreOutcome(Result,&Outcome)&&
        Outcome.Attempt.Sequence==3&&Outcome.Attempt.Effect==XMQTT_JOURNAL_COMMITTED,
        "old Future retains original local failure and committed attempt");
    drWriterReserved(W);
    if(!Drive){
        testRequire(xmqttReconnectClose(R)&&xmqttReconnectRelease(&L),"stop retries before cold recovery");
        rcClose(R);
        xrtFutureDestroy(Original);
        return;
    }

    unsigned char Wire[128];
    xmqttstorelease Delivery={0};
    xmqttstoreview View;
    size_t Required=0;
    xmqttmessage* Message=NULL;
    testRequire(!xmqttClientTakeMessage(L.Client,&Message)&&
        xmqttStoreInboxTake(W,Wire,sizeof(Wire),&Delivery,&View,&Required)==XMQTT_STORE_DONE,
        "durable-only Inbox remains readable offline");
    xfuture* Ack=NULL;
    testRequire(xmqttStoreInboxAckAsync(W,Delivery,5000,&Ack)==XMQTT_JWORK_ACCEPTED,"offline application ACK");
    jwAwait(&Drive->Entered);
    testRequire(xmqttReconnectRelease(&L),"release original Client lease");
    rcWait(&Peer->Connects,2);
    testRequire(!xmqttReconnectAcquire(R,&L)&&xrtFutureState(Ack)==XFUTURE_PENDING&&swInfo(W).State.Transactions==4,
        "replacement attaches while application ACK is pending; Ready waits for durable OPEN");
    xrtAtomic32Store(&Drive->Release,1,XMEMORY_RELEASE);
    testRequire(jwOutcome(Ack)->Effect==XMQTT_JOURNAL_COMMITTED,"old application ACK commits");
    Info=rcOnline(R,2);
    rcWait(&Peer->Replays,1);
    State=drWaitState(W,7);
    testRequire(Info.Generation==2&&Info.Attempts==2&&State.Outgoing==0&&State.Inbox==0&&
        xrtFutureValue(Original)==Result&&Result->Status!=XMQTT_STATUS_OK,
        "DUP replay retires obligation without resolving the original Future again");
    xrtFutureDestroy(Ack);
    rcClose(R);
    testRequire(xrtFutureValue(Original)==Result,"old Future outlives controller");
    xrtFutureDestroy(Original);
    State=swInfo(W).State;
    testRequire(State.Transactions==8&&!State.LiveAtCut,"second physical END is durable");
}

static void drCase(xmqttversion Version,drmode Mode,bool Hostname)
{
    rcPeer Peer;
    rcStart(&Peer,Version,RC_PERSIST|((Mode==DR_REPLAY||Mode==DR_COLD)?RC_CUT_PUBLISH:0));
    jwdrive Drive={0};
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(Version);
    xmqttjournal* Journal=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);
    SC.ClientId=XRT_STR_LITERAL("managed");
    SC.WallMilliseconds=0;
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"initial durable projection");
    xtaskpool* Pool=jwPool(2);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    xmqttstorewrite* Writer=NULL;
    testRequire(xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer),"controller Inbox Writer");
    Drive.Base.Calls=0;
    if(Mode==DR_REPLAY)Drive.Hold=35;
    else if(Mode==DR_LATE_OPEN)Drive.Hold=7;
    else if(Mode==DR_UNCERTAIN_OPEN){Drive.Base.Fail=7;Drive.Base.FlushOnFailure=true;}

    xmqttreconnectconfig K;
    xmqttpacket Connect;
    xmqttproperty Expiry;
    rcConfig(&Peer,&K,&Connect,&Expiry);
    K.Factory=NULL;
    dr_factory Factory={.Peer=&Peer};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Hostname){
        Factory.Resolver=xrtNetResolverCreate(NULL);
        testRequire(Factory.Resolver!=NULL,"create durable reconnect hostname Resolver");
    }
#else
    (void)Hostname;
#endif
    K.FactoryData=&Factory;
    K.InitialDelay=K.MaxDelay=5;
    if(Mode==DR_LATE_OPEN)K.Client.SetupTimeout=500;
    K.RetryStatuses|=UINT32_C(1)<<XMQTT_STATUS_INITIALIZATION;
    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"passive application Session");
    xmqttclientstoreconfig Store={sizeof(Store),Writer,drWall,NULL};
    xmqttreconnect* R=(xmqttreconnect*)(uintptr_t)1;
    testRequire(!mqttAppReconnectStored(Peer.Engine,Pool,1,&K,Session,&Store,drFactory,&Connect,&R)&&
        R==(xmqttreconnect*)(uintptr_t)1,"same-pool constructor failure preserves Session and output");
    xrtClearError();
    testRequire(mqttAppReconnectStored(Peer.Engine,Peer.Pool,1,&K,Session,&Store,drFactory,&Connect,&R),
        "create real durable reconnect controller");

    if(Mode==DR_REPLAY)drReplay(&Peer,R,Writer,&Drive);
    else if(Mode==DR_COLD){
        drReplay(&Peer,R,Writer,NULL);
        testRequire(xmqttStoreWriteDestroy(Writer),"completely retire original Writer and live Session");
        Journal=jpReopen(File,&JC);
        SC.WallMilliseconds=200000;
        stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"reopen exact offline prefix");
        xmqttstoresessionconfig Restore;
        xmqttStoreSessionConfigInit(&Restore);
        Restore.Binding=SC.Binding;Restore.Generation=SC.Generation;
        Restore.WallMilliseconds=SC.WallMilliseconds;Restore.MonotonicTime=xrtTimer();
        testRequire(xmqttStoreSessionRestore(State,&SK,&Restore,&Session)==XMQTT_SESSION_DONE&&
            xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer),"restore new passive Session and Writer");
        Store.Writer=Writer;
        testRequire(mqttAppReconnectStored(Peer.Engine,Peer.Pool,1,&K,Session,&Store,drFactory,&Connect,&R),
            "start new controller from recovered obligations");
        testRequire(rcOnline(R,1).Generation==1,"recovered state starts a new controller generation");
        rcWait(&Peer.Replays,1);
        xmqttstorestateinfo Replayed=drWaitState(Writer,6);
        testRequire(Replayed.Outgoing==0&&Replayed.Inbox==1,"cold DUP retires only protocol obligation");
        unsigned char Wire[128];
        size_t Required=0;
        xmqttstorelease Delivery={0};
        xmqttstoreview View;
        xfuture* Ack=NULL;
        testRequire(xmqttStoreInboxTake(Writer,Wire,sizeof(Wire),&Delivery,&View,&Required)==XMQTT_STORE_DONE&&
            xmqttStoreInboxAckAsync(Writer,Delivery,5000,&Ack)==XMQTT_JWORK_ACCEPTED&&
            jwOutcome(Ack)->Effect==XMQTT_JOURNAL_COMMITTED,"restored Inbox separately acknowledged");
        xrtFutureDestroy(Ack);
        rcClose(R);
    }else if(Mode==DR_LATE_OPEN){
        jwAwait(&Drive.Entered);
        xmqttreconnectinfo Info;
        double Deadline=xrtTimer()+5;
        do{
            testRequire(xmqttReconnectInfo(R,&Info)&&xrtTimer()<Deadline,"late OPEN timeout");
            if(Info.State==XMQTT_RECONNECT_RETIRING)break;
            xrtSleep(1);
        }while(true);
        testRequire(Info.LastStatus==XMQTT_STATUS_TIMEOUT&&Info.Attempts==1&&
            xrtAtomic32Load(&Peer.Accepted,XMEMORY_ACQUIRE)==1&&swInfo(Writer).State.Transactions==0,
            "setup deadline closes logically while late OPEN still owns physical storage");
        drWriterReserved(Writer);
        xrtAtomic32Store(&Drive.Release,1,XMEMORY_RELEASE);
        Info=rcOnline(R,1);
        testRequire(Info.Generation==2&&Info.Attempts==2&&drWaitState(Writer,3).LiveAtCut,
            "late committed OPEN and END reconcile before Session Present on replacement");
        rcClose(R);
        testRequire(swInfo(Writer).State.Transactions==4,"late and resumed connection both record END");
    }else{
        testRequire(xmqttReconnectWaitFor(R,5000)==XWAIT_OK,"uncertain OPEN reaches terminal controller state");
        xmqttreconnectinfo Info;
        testRequire(xmqttReconnectInfo(R,&Info)&&Info.State==XMQTT_RECONNECT_FAILED&&
            Info.LastStatus==XMQTT_STATUS_SESSION&&Info.Attempts==1&&swInfo(Writer).Worker.Failed&&
            xrtAtomic32Load(&Peer.Accepted,XMEMORY_ACQUIRE)==1,"unusable Writer stops retry before another socket");
        drWriterReserved(Writer);
        testRequire(xmqttReconnectDestroy(R),"destroy terminal controller releases Writer");
    }
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Factory.Resolver)testRequire(xrtNetResolverDestroy(Factory.Resolver),"retire Resolver after all reconnect generations");
#endif
    rcStop(&Peer);
    testRequire(xmqttStoreWriteDestroy(Writer),"caller destroys Writer after controller");
    jwPoolDone(Pool);
    Drive.Base.Fail=0;
    Journal=jpReopen(File,&JC);
    SC.WallMilliseconds=200000;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"ordinary cold journal recovery");
    xmqttstorestateinfo Recovery=stInfo(State);
    uint64 Sequences=(Mode==DR_REPLAY||Mode==DR_COLD)?8:Mode==DR_LATE_OPEN?4:1;
    testRequire(Recovery.Transactions==Sequences&&Recovery.Outgoing==0&&Recovery.Inbox==0,
        "reopened file reports authoritative durable effects");
    xmqttStoreStateDestroy(State);
    testRequire(xmqttJournalDestroy(Journal),"destroy recovered journal");
    jpFileClose(File);
}

static void drCombined(xmqttversion Version,bool Retained)
{
    dr_combined Peer;
    rcStart(&Peer.Peer,Version,Retained?RC_PERSIST:0);
    xrtAtomic32Init(&Peer.Subscribes,0);
    jwdrive Drive={0};
    jpfile File=jwFile(&Drive);
    xmqttjournalconfig JC=stJournalConfig(Version);
    xmqttjournal* Journal=jpCreate(File,&JC);
    xmqttstorestateconfig SC=stConfig(Version);
    SC.ClientId=XRT_STR_LITERAL("managed");
    SC.WallMilliseconds=0;
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(Journal,&SC,&State),XMQTT_STORE_DONE,"combined durable projection");
    xtaskpool* Pool=jwPool(2);
    xmqttstorewrite* Writer=NULL;
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    testRequire(xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer),"combined Writer");

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=SC.ClientId;
    xmqttsession* Session=NULL;
    testRequire(xmqttSessionCreate(&SK,&Session),"combined Session");
    xmqttclientstoreconfig Store={sizeof(Store),Writer,drWall,NULL};
    xmqttreconnectstoreplan StorePlan={Session,&Store,drFactory};
    dr_factory Factory={.Peer=&Peer.Peer};

    xmqttreconnectconfig Config;
    xmqttpacket Connect;
    xmqttproperty Expiry;
    rcConfig(&Peer.Peer,&Config,&Connect,&Expiry);
    Config.Factory=NULL;
    Config.FactoryData=&Factory;
    Config.InitialDelay=Config.MaxDelay=5;
    Config.Store=&StorePlan;
    xmqttsubscriptionsconfig Subscriptions;
    xmqttSubscriptionsConfigInit(&Subscriptions);
    xmqttsubscription Topic={XRT_STR_LITERAL("restored"),0};
    xmqttpacket Initial;
    xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Initial);
    Initial.Body.Topics=(xmqttsubscriptionlist){&Topic,1};
    xmqttreconnectsubscriptionplan Intent={&Subscriptions,&Initial,1};
    Config.Subscriptions=&Intent;

    xmqttreconnect* Controller=NULL;
    testRequire(xmqttReconnectCreate(Peer.Peer.Engine,Peer.Peer.Pool,1,&Config,
        &Connect,&Controller),"one constructor combines durable session and subscriptions");
    testRequire(rcOnline(Controller,1).Generation==1,"first subscription gate");
    xmqttsubscriptionsinfo Info={.Size=sizeof(Info)};
    testRequire(xmqttReconnectSubscriptionsInfo(Controller,&Info) && Info.Synchronized &&
        Info.Generation==1 && xrtAtomic32Load(&Peer.Subscribes,XMEMORY_ACQUIRE)==1 &&
        drWaitState(Writer,1).LiveAtCut,"first OPEN and SUBACK precede online");

    testRequire(xrtNetStreamAbort(Peer.Peer.Connections[0].Stream),"force transport reconnect");
    testRequire(rcOnline(Controller,2).Generation==2,"restored generation comes online");
    testRequire(xmqttReconnectSubscriptionsInfo(Controller,&Info) && Info.Synchronized &&
        Info.Generation==2 && xrtAtomic32Load(&Peer.Subscribes,XMEMORY_ACQUIRE)==(Retained?1u:2u) &&
        swInfo(Writer).State.LiveAtCut,"retained session preserves subscriptions and reset restores them");
    rcClose(Controller);
    rcStop(&Peer.Peer);
    testRequire(xmqttStoreWriteDestroy(Writer),"retire combined Writer");
    jwPoolDone(Pool);
    jpFileClose(File);
}

int main(void)
{
    for(unsigned I=0;I<2;++I){
        xmqttversion Version=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        drCombined(Version,true);
        drCombined(Version,false);
        for(drmode Mode=DR_REPLAY;Mode<=DR_COLD;++Mode)drCase(Version,Mode,false);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        drCase(Version,DR_REPLAY,true);
        drCase(Version,DR_COLD,true);
#endif
    }
    puts("[PASS] combined subscription recovery and durable automatic TCP reconnect, cold recovery, lease/ACK ordering, late OPEN reconciliation and uncertain storage fail-stop");
    return 0;
}
