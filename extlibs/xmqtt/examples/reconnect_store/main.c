#include <xmqtt/reconnect_store.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_COMPACT_ONLINE) && defined(XMQTT_FEATURE_XMQTT_STORE_SESSION)
#define RECONNECT_EXAMPLE_COMPACT
#include <xmqtt/store_compact_online.h>
#include <xmqtt/store_session.h>
#include <errno.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
#include <xmqtt/client_store_tls.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
#include <xmqtt/client_store_ws.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS) || \
    (defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) && defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS))
#define RECONNECT_EXAMPLE_TLS
#include "../tls/material.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool wall(ptr User,uint64* Milliseconds)
{
    uint64* Last=User;
    int64 Current;
    if(!xrtTimeToUnixMs(xrtNow(),&Current)||Current<0)return false;
    if((uint64)Current>*Last)*Last=(uint64)Current;
    *Milliseconds=*Last;
    return true;
}

typedef enum channel {CHANNEL_TCP,CHANNEL_TLS,CHANNEL_WS,CHANNEL_WSS} channel;
typedef struct endpoint {
    channel Channel;
    xnetaddr Address;
    xstrview Name;
    uint16 Port;
    const char* CaPath;
#if defined(RECONNECT_EXAMPLE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    xnetresolver* Resolver;
#endif
#if defined(RECONNECT_EXAMPLE_TLS)
    xx509store* Trust;
    xtlsverifier* Verifier;
    xmqttclienttlsconfig TLS;
#endif
} endpoint;

static bool endpointInit(endpoint* Remote)
{
#if defined(RECONNECT_EXAMPLE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    if(Remote->Channel!=CHANNEL_TCP){
        Remote->Resolver=xrtNetResolverCreate(NULL);
        if(!Remote->Resolver)return false;
    }
#endif
#if defined(RECONNECT_EXAMPLE_TLS)
    if(Remote->CaPath){
        size_t Size=0;
        bytes Data=xmqttExampleTlsRead(Remote->CaPath,65536,&Size);
        if(!Data)return false;
        Remote->Trust=xrtX509StoreCreate();
        bool Added=Remote->Trust&&xrtX509StoreAddData(Remote->Trust,(xbytesview){Data,Size},NULL);
        xrtFree(Data);
        if(!Added)return false;
        xtlsverifierconfig VC;
        xrtTlsVerifierConfigInit(&VC);
        VC.Store=Remote->Trust;
        xtlsverifylimits Limits;
        xrtTlsVerifyLimitsInit(&Limits);
        Remote->Verifier=xrtTlsVerifierCreateLimited(&VC,&Limits);
        if(!Remote->Verifier||!xmqttClientTlsConfigInit(&Remote->TLS))return false;
        Remote->TLS.Session.Verifier=Remote->Verifier;
        Remote->TLS.Session.ResumeLimit=0;
        if(Remote->Channel==CHANNEL_TLS){
            static const xstrview MQTT=XRT_STR_LITERAL("mqtt");
            Remote->TLS.Session.Protocols=&MQTT;
            Remote->TLS.Session.ProtocolCount=1;
        }
    }
#endif
    (void)Remote;
    return true;
}

static bool endpointDestroy(endpoint* Remote)
{
#if defined(RECONNECT_EXAMPLE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    if(Remote->Resolver&&!xrtNetResolverDestroy(Remote->Resolver))return false;
#endif
#if defined(RECONNECT_EXAMPLE_TLS)
    xrtTlsVerifierRelease(Remote->Verifier);
    xrtX509StoreFree(Remote->Trust);
#endif
    (void)Remote;
    return true;
}

static xmqttclientstatus connectAttempt(xnetengine* Engine,uint64 Affinity,const xmqttclientconfig* Config,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,ptr User,xmqttclient** Client)
{
    endpoint* Remote=User;
    bool OK=false;
    switch(Remote->Channel){
    case CHANNEL_TCP:
        OK=xmqttClientConnectStore(Engine,&Remote->Address,Affinity,Config,Session,Store,Connect,Client);
        break;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    case CHANNEL_TLS:
        OK=xmqttClientTlsDialStore(Engine,Remote->Resolver,Remote->Name,Remote->Port,Affinity,Config,
                                  &Remote->TLS,Session,Store,Connect,Client);
        break;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    case CHANNEL_WS:
        OK=xmqttClientWsConnectUriStore(Engine,Remote->Resolver,Remote->Name,Affinity,Config,NULL,Session,Store,Connect,Client);
        break;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    case CHANNEL_WSS:
        OK=xmqttClientWssConnectUriStore(Engine,Remote->Resolver,Remote->Name,Affinity,Config,NULL,&Remote->TLS,Session,Store,Connect,Client);
        break;
#endif
#endif
    default:
        break;
    }
    return OK?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
}

static bool online(xmqttreconnect* Controller,uint64 Generation,xmqttclientlease* Lease)
{
    double Deadline=xrtTimer()+10;
    do{
        xmqttreconnectinfo Info;
        if(!xmqttReconnectInfo(Controller,&Info)||Info.State>=XMQTT_RECONNECT_STOPPED)return false;
        /* A request result can be visible before the controller retires
         * its Client. Require the next generation before taking a lease. */
        if(Info.Generation>=Generation&&xmqttReconnectAcquire(Controller,Lease))return true;
        xrtSleep(1);
    }while(xrtTimer()<Deadline);
    return false;
}

static const xmqttwriteresult* result(xfuture* Future)
{
    return xrtFutureWaitFor(Future,10000)==XWAIT_OK&&xrtFutureState(Future)==XFUTURE_RESOLVED?xrtFutureValue(Future):NULL;
}

static bool replayed(xmqttstorewrite* Writer)
{
    double Deadline=xrtTimer()+10;
    do{
        xmqttstorewriteinfo Info;
        if(!xmqttStoreWriteInfo(Writer,&Info)||Info.Worker.Failed)return false;
        if(Info.State.LiveAtCut&&!Info.State.Outgoing)return true;
        xrtSleep(1);
    }while(xrtTimer()<Deadline);
    return false;
}

static bool destroyPool(xtaskpool* Pool)
{
    return !Pool||(xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool));
}

#if defined(RECONNECT_EXAMPLE_COMPACT)
enum {COMPACT_SEED_TRANSACTIONS=24,COMPACT_HISTORY_PUBLISHES=3};

static bool compactSeed(xmqttjournal* Journal,const xmqttstoredeltaconfig* Config)
{
    unsigned char Bytes[64];
    size_t Size=0;
    uint64 Seq=0;
    xmqttjournalrecord Record;
    if(xmqttJournalNext(Journal,&Record)!=XMQTT_JOURNAL_EMPTY||
       xmqttStoreDeltaWrite(Config,NULL,0,NULL,0,Bytes,sizeof(Bytes),&Size)!=XMQTT_STORE_DELTA_DONE)return false;
    for(unsigned I=0;I<COMPACT_SEED_TRANSACTIONS;++I)
        if(xmqttJournalAppend(Journal,(xbytesview){Bytes,Size},&Seq)!=XMQTT_JOURNAL_DONE)return false;
    return true;
}

static const xstrview compactTriggerTopic=XRT_STR_LITERAL("xrt/example/compact-trigger");
static const xbytesview compactTriggerBody=XRT_BYTES_LITERAL("accepted before automatic compaction");

static bool compactSubscribe(xmqttclient* Client)
{
    xmqttsubscription Filter={compactTriggerTopic,1};
    xmqttpacket Packet;
    xmqttPacketInit(XMQTT_PACKET_SUBSCRIBE,&Packet);
    Packet.Body.Topics=(xmqttsubscriptionlist){&Filter,1};
    xfuture* F=NULL;
    if(xmqttClientSubscribeAsync(Client,&Packet,10000,&F)!=XMQTT_ADMISSION_ACCEPTED)return false;
    const xmqttsubscriptionresult* Result=xrtFutureWaitFor(F,10000)==XWAIT_OK?xrtFutureValue(F):NULL;
    bool Subscribed=Result&&Result->Status==XMQTT_STATUS_OK&&Result->Acknowledged&&Result->Granted==1&&Result->Ack;
    if(Subscribed){
        const xmqttpacket* Ack=xmqttMessagePacket(Result->Ack);
        Subscribed=Ack&&Ack->Body.Reasons.Size==1&&Ack->Body.Reasons.Data[0]==1;
    }
    xrtFutureDestroy(F);
    return Subscribed;
}

static bool compactHistory(xmqttclient* Client)
{
    xmqttpacket P;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    P.Flags=2;
    P.Body.Publish.Topic=XRT_STR_LITERAL("xrt/example/compact-history");
    P.Body.Publish.Payload=XRT_BYTES_LITERAL("retired QoS 1");
    for(unsigned I=0;I<COMPACT_HISTORY_PUBLISHES;++I){
        xfuture* F=NULL;
        if(xmqttClientPublishAsync(Client,&P,10000,&F)!=XMQTT_ADMISSION_ACCEPTED)return false;
        const xmqttwriteresult* R=result(F);
        xmqttclientstoreoutcome O;
        bool Settled=R&&R->Status==XMQTT_STATUS_OK&&xmqttClientStoreOutcome(R,&O)&&
            O.Key==I+1u&&O.Completion.Effect==XMQTT_JOURNAL_COMMITTED;
        xrtFutureDestroy(F);
        if(!Settled)return false;
    }
    return true;
}

static bool compactManual(xmqttstorewrite* Writer,const xmqttstorecompactconfig* Compact,
    xmqttstorecompactresult* Selection)
{
    xfuture* Future=NULL;
    if(xmqttStoreWriteCompactAsync(Writer,Compact,10000,&Future)!=XMQTT_JWORK_ACCEPTED)return false;
    const xmqttstorecompactresult* Result=xrtFutureWaitFor(Future,10000)==XWAIT_OK?xrtFutureValue(Future):NULL;
    if(Result){
        *Selection=*Result;
        Selection->Cause=NULL;
    }
    bool Done=Result!=NULL;
    xrtFutureDestroy(Future);
    return Done;
}

static bool compactAutomatic(xmqttstorewrite* Writer,const xmqttstorecompactconfig* Compact,
    xfuture* Publish,bool Pressure,uint64 InboxKey,xmqttstorecompactresult* Selection)
{
    /* In the capacity scenario the APP opens room before releasing its
     * next input. Its store configuration supplies the fixture's limit. */
    if(Pressure&&(!compactManual(Writer,Compact,Selection)||
       Selection->Effect!=XMQTT_JOURNAL_COMMITTED))return false;

    puts("AUTO_ARMED");
    if(fflush(stdout)!=0)return false;
    double Deadline=xrtTimer()+5;
    for(;;){
        xmqttstorewriteinfo Info;
        if(!xmqttStoreWriteInfo(Writer,&Info)||Info.Worker.Closed||
           xrtFutureState(Publish)!=XFUTURE_PENDING||xrtTimer()>=Deadline)return false;
        if(Info.State.Inbox==1&&Info.State.LastKey==InboxKey&&
           (!Pressure||Info.Worker.Journal.Sequence==3))break;
        xrtSleep(1);
    }
    if(!Pressure&&(!compactManual(Writer,Compact,Selection)||
       Selection->Effect!=XMQTT_JOURNAL_COMMITTED))return false;
    puts("AUTO_ROTATED 1 1");
    return true;
}

static bool compactInboxAck(xmqttstorewrite* Writer,xmqttversion Version,uint64 InboxKey)
{
    unsigned char Wire[1024];
    size_t Bytes=0;

    xmqttstoreview View;
    if(xmqttStoreWriteReadPending(Writer,Wire,sizeof(Wire),&View,&Bytes)!=XMQTT_STORE_DONE||
       View.Key!=InboxKey||View.Epoch!=1||View.Qos!=1||!View.InboxPending)return false;
    xmqttpacket Packet;
    xbytesview Input={Wire,Bytes};
    xmqttcodecstatus Decoded=Version==XMQTT_VERSION_5
        ?xmqttPacketRead5(Input,XMQTT_SERVER_TO_CLIENT,NULL,NULL,&Packet,NULL)
        :xmqttPacketRead311(Input,XMQTT_SERVER_TO_CLIENT,NULL,NULL,&Packet,NULL);
    if(Decoded!=XMQTT_CODEC_DONE||Packet.Type!=XMQTT_PACKET_PUBLISH||Packet.Identifier!=View.Identifier||
       Packet.Body.Publish.Topic.Size!=compactTriggerTopic.Size||
       memcmp(Packet.Body.Publish.Topic.Data,compactTriggerTopic.Data,compactTriggerTopic.Size)||
       Packet.Body.Publish.Payload.Size!=compactTriggerBody.Size||
       memcmp(Packet.Body.Publish.Payload.Data,compactTriggerBody.Data,compactTriggerBody.Size))return false;
    xfuture* F=NULL;
    if(xmqttStoreWriteRetireAsync(Writer,View.Epoch,View.Key,10000,&F)!=XMQTT_JWORK_ACCEPTED)return false;
    const xmqttcommitresult* Result=xrtFutureWaitFor(F,10000)==XWAIT_OK?xrtFutureValue(F):NULL;
    bool Done=Result&&Result->Status==XMQTT_COMMIT_OK&&Result->Effect==XMQTT_JOURNAL_COMMITTED;
    if(Done)printf("INBOX_ACKED %llu %llu %u\n",(unsigned long long)View.Key,
        (unsigned long long)Result->Sequence,(unsigned)View.Identifier);
    xrtFutureDestroy(F);
    return Done&&xmqttStoreWriteReadPending(Writer,Wire,sizeof(Wire),&View,&Bytes)==XMQTT_STORE_EMPTY;
}

static void compactCut(xmqttstorewrite* Writer,xfuture* Publish,bool Auto,bool Pressure,uint64 InboxKey)
{
    /* The fixture signals only after independently observing the dropped ACK. */
    if(fgetc(stdin)!='c'||xrtFutureState(Publish)!=XFUTURE_PENDING)return;
    xmqttstorewriteinfo Info;
    xmqttstoreview Outgoing;
    unsigned char Packet[1024];
    size_t Bytes=0;
    if(!xmqttStoreWriteInfo(Writer,&Info)||!Info.State.LiveAtCut||Info.State.Outgoing!=1||
       xmqttStoreWriteRead(Writer,Info.State.LastKey,Packet,sizeof(Packet),&Outgoing,&Bytes)!=XMQTT_STORE_DONE||
       Outgoing.Qos!=2||!Outgoing.Attempted)return;
    xmqttstorecompactconfig Config;
    xmqttStoreCompactConfigInit(&Config);
    Config.MinReclaimBytes=1;
    Config.MaxEntries=4;
    Config.MaxScratchBytes=1024;
    xmqttstorecompactresult Result={0};
    bool Selected=Auto?compactAutomatic(Writer,&Config,Publish,Pressure,InboxKey,&Result):compactManual(Writer,&Config,&Result);
    bool Held=Selected&&Result.Status==XMQTT_COMMIT_OK&&Result.Effect==XMQTT_JOURNAL_COMMITTED&&
       Result.PreviousGeneration==1&&Result.Generation==2&&Result.Sequence==2&&Result.Bytes<Result.PreviousBytes&&
       xrtFutureState(Publish)==XFUTURE_PENDING;
    if(Held){
        printf("COMPACTED %llu %llu %u %s %llu %llu %llu\n",(unsigned long long)Result.Generation,
            (unsigned long long)Result.Sequence,(unsigned)Outgoing.Identifier,Outgoing.Pubrel?"PUBREL":"PUBLISH",
            (unsigned long long)Outgoing.Key,(unsigned long long)Result.PreviousBytes,(unsigned long long)Result.Bytes);
        Held=fflush(stdout)==0;
    }
    /* The qualification runner terminates this native process at the marker. */
    if(Held)(void)fgetc(stdin);
}

static bool compactRestore(const xmqttstorestate* State,const xmqttsessionconfig* SessionConfig,
    const xmqttstorestateconfig* Config,bool Auto,bool Pressure,uint64 InboxKey,xmqttsession** Session)
{
    xmqttstorestateinfo Info;
    xmqttstoreview Outgoing;
    if(!xmqttStoreStateInfo(State,&Info)||!Info.LiveAtCut||!Info.HasSession||Info.Outgoing!=1||
       Info.Transactions!=(Pressure?3u:2u)||Info.Inbox!=(Auto?1u:0u)||Info.LastKey!=(Auto?InboxKey:1u)||
       xmqttStoreStateView(State,XMQTT_STORE_OUTGOING,0,&Outgoing)!=XMQTT_STORE_DONE||
       Outgoing.Qos!=2||!Outgoing.Attempted)return false;
    xmqttstoresessionconfig Restore;
    if(!xmqttStoreSessionConfigInit(&Restore))return false;
    Restore.Binding=Config->Binding;
    Restore.Generation=Config->Generation;
    Restore.WallMilliseconds=Config->WallMilliseconds;
    Restore.MonotonicTime=xrtTimer();
    if(xmqttStoreSessionRestore(State,SessionConfig,&Restore,Session)!=XMQTT_SESSION_DONE)return false;
    printf("RECOVERED %llu %llu %u %s %llu\n",(unsigned long long)Config->Generation,
        (unsigned long long)Info.Transactions,(unsigned)Outgoing.Identifier,Outgoing.Pubrel?"PUBREL":"PUBLISH",
        (unsigned long long)Outgoing.Key);
    return true;
}

static bool compactVerify(xmqttjournalslot* Slot,xmqttstorestateconfig* C,uint64* Last,uint64 End)
{
    if(!wall(Last,&C->WallMilliseconds))return false;
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorestateinfo Info;
    bool OK=xmqttJournalSlotOpenCurrent(Slot,&Journal)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(Journal,C,&State)==XMQTT_STORE_DONE&&xmqttStoreStateInfo(State,&Info)&&
        !Info.LiveAtCut&&!Info.Outgoing&&!Info.Inbox&&Info.Transactions==End;
    xmqttStoreStateDestroy(State);
    if(!xmqttJournalDestroy(Journal)){(void)xmqttJournalDestroy(Journal);return false;}
    if(OK)printf("VERIFIED %llu %llu\n",(unsigned long long)C->Generation,(unsigned long long)End);
    return OK;
}
#endif

int main(int Argc,char** Argv)
{
    if(Argc==1){
        puts("Usage: examples_reconnect_store_main ADDRESS PORT 5|311 [expect-replay|replay-qos2]");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
        puts("       examples_reconnect_store_main tls HOST PORT 5|311 CA.pem [expect-replay|replay-qos2]");
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
        puts("       examples_reconnect_store_main ws://HOST:PORT/mqtt 5|311 [expect-replay|replay-qos2]");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        puts("       examples_reconnect_store_main wss://HOST:PORT/mqtt 5|311 CA.pem [expect-replay|replay-qos2]");
#endif
#endif
#if defined(RECONNECT_EXAMPLE_COMPACT)
        puts("       The interop runner accepts compact, compact-auto, compact-pressure and compact-replay cut/resume modes.");
#endif
        return 0;
    }
    endpoint Remote={0};
    int Target=1,Level=3;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    if(!strcmp(Argv[1],"tls")){
        Remote.Channel=CHANNEL_TLS;
        Target=2;
        Level=4;
    }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    if(!strncmp(Argv[1],"ws://",5)){
        Remote.Channel=CHANNEL_WS;
        Level=2;
    }
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
    if(!strncmp(Argv[1],"wss://",6)){
        Remote.Channel=CHANNEL_WSS;
        Level=2;
    }
#endif
#endif
    bool Secure=Remote.Channel==CHANNEL_TLS||Remote.Channel==CHANNEL_WSS;
    int Mode=Level+(Secure?2:1);
    bool Cut=false,Resume=false;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    const char* Directory=NULL;
    bool Auto=false,Pressure=false;
    const char* ReplayQuota=NULL;
    uint64 ReplayLimit=0;
    if(Argc==Mode+4){
        Cut=!strcmp(Argv[Mode],"compact-replay-cut");
        Resume=!strcmp(Argv[Mode],"compact-replay-resume");
        if(!Cut&&!Resume)return 2;
        Directory=Argv[Mode+1];
        ReplayQuota=Argv[Mode+2];
        const char* Limit=Argv[Mode+3];
        char* End;
        errno=0;
        unsigned long long Parsed=strtoull(Limit,&End,10);
        if(Limit[0]<'1'||Limit[0]>'9'||*End||errno==ERANGE||Parsed>UINT64_MAX)return 2;
        ReplayLimit=(uint64)Parsed;
        Auto=Pressure=true;
    }else if(Argc==Mode+2){
        bool PressureCut=!strcmp(Argv[Mode],"compact-pressure-cut");
        bool PressureResume=!strcmp(Argv[Mode],"compact-pressure-resume");
        bool AutoCut=PressureCut||!strcmp(Argv[Mode],"compact-auto-cut");
        bool AutoResume=PressureResume||!strcmp(Argv[Mode],"compact-auto-resume");
        Pressure=PressureCut||PressureResume;
        Auto=AutoCut||AutoResume;
        Cut=AutoCut||!strcmp(Argv[Mode],"compact-cut");
        Resume=AutoResume||!strcmp(Argv[Mode],"compact-resume");
        if(!Cut&&!Resume)return 2;
        Directory=Argv[Mode+1];
    }else
#endif
    if(Argc<Mode||Argc>Mode+1)return 2;
    bool Replay=Argc==Mode+1;
    bool Qos2=Cut||Resume||(Replay&&!strcmp(Argv[Mode],"replay-qos2"));
    if(Replay&&!Qos2&&strcmp(Argv[Mode],"expect-replay"))return 2;
    bool V5=!strcmp(Argv[Level],"5");
    if(!V5&&strcmp(Argv[Level],"311"))return 2;
    const char* Name=Argv[Target];
    Remote.Name=(xstrview){Name,strlen(Name)};
    if(Level!=2){
        char* End;
        unsigned long Port=strtoul(Argv[Target+1],&End,10);
        if(End==Argv[Target+1]||*End||!Port||Port>65535)return 2;
        Remote.Port=(uint16)Port;
        if(Remote.Channel==CHANNEL_TCP&&!xrtNetAddrParse(&Remote.Address,Name,Remote.Port))return 2;
    }
    if(Secure)Remote.CaPath=Argv[Level+1];

    xmqttversion Version=V5?XMQTT_VERSION_5:XMQTT_VERSION_311;
    xstrview Id=XRT_STR_LITERAL("xrt-reconnect-store-example");
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Version=Version;
    JC.Generation=1;
    JC.Binding=XRT_BYTES_LITERAL("example/reconnect-store");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    /* Both original QoS 2 cuts fit. The independently encoded trigger
     * transaction crosses this cap for either MQTT level and cut. */
    if(ReplayQuota)JC.MaxFileBytes=1024u*1024u;
    else if(Pressure)JC.MaxFileBytes=15200;
#endif
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Delta.Version=Version;
    SC.Generation=JC.Generation;
    SC.Binding=JC.Binding;
    SC.ClientId=Id;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    uint64 InboxKey=ReplayQuota?COMPACT_HISTORY_PUBLISHES+2u:2u;
    if(ReplayQuota){
        if(!strcmp(ReplayQuota,"transactions"))SC.MaxTransactions=ReplayLimit;
        else if(!strcmp(ReplayQuota,"operations"))SC.MaxOperations=ReplayLimit;
        else if(!strcmp(ReplayQuota,"bytes"))SC.MaxReplayBytes=ReplayLimit;
        else return 2;
    }
#endif

    bool OK=false,Started=false;
    uint64 Wall=0;
    str Path=NULL;
    xfile File=NULL;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    xroot Root=NULL;
    xmqttjournalslot* Slot=NULL;
#endif
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorewrite* Writer=NULL;
    xmqttsession* Session=NULL;
    xmqttreconnect* Controller=NULL;
    xmqttclientlease Lease={0};
    xtaskpool *Storage=NULL,*Cleanup=NULL;
    xnetengine* Engine=NULL;
    xfuture *Publish=NULL,*Disconnect=NULL;
    if(!wall(&Wall,&SC.WallMilliseconds))goto Done;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(Directory){
        Root=xrtRootOpen(Directory);
        if(!Root)goto Done;
        xmqttjournalstatus Open=Cut?xmqttJournalSlotCreate(Root,&JC,&Slot):xmqttJournalSlotOpen(Root,&JC,&Slot);
        xmqttjournalslotinfo Selected;
        if(Open!=XMQTT_JOURNAL_DONE||!xmqttJournalSlotInfo(Slot,&Selected)||
           Selected.Generation!=(Cut?1u:2u))goto Done;
        SC.Generation=Selected.Generation;
        if(xmqttJournalSlotOpenCurrent(Slot,&Journal)!=XMQTT_JOURNAL_DONE)goto Done;
        if(Cut){
            if(!compactSeed(Journal,&SC.Delta)||!xmqttJournalDestroy(Journal))goto Done;
            Journal=NULL;
            if(xmqttJournalSlotOpenCurrent(Slot,&Journal)!=XMQTT_JOURNAL_DONE)goto Done;
        }
    }else
#endif
    {
        File=xrtFileTemp(NULL,"xmqtt-reconnect-store-",NULL,&Path);
        if(!File||xmqttJournalCreate(File,&JC,&Journal)!=XMQTT_JOURNAL_DONE)goto Done;
    }
    if(xmqttStoreStateRecover(Journal,&SC,&State)!=XMQTT_STORE_DONE)goto Done;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=Id;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(Resume){
        if(!compactRestore(State,&SK,&SC,Auto,Pressure,InboxKey,&Session))goto Done;
    }else
#endif
    if(!xmqttSessionCreate(&SK,&Session))goto Done;
    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    Storage=xrtTaskPoolCreate(&PC);
    Cleanup=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    if(!Storage||!Cleanup||!xmqttStoreWriteCreate(Storage,Journal,State,&WC,&Writer))goto Done;
    Journal=NULL;
    State=NULL;

    xnetengineconfig EC;
    xrtNetEngineConfigInit(&EC);
    EC.Workers=1;
    Engine=xrtNetEngineCreate(&EC);
    if(!Engine||!xrtNetEngineStart(Engine))goto Done;
    Started=true;
    xmqttreconnectconfig RC;
    xmqttReconnectConfigInit(&RC);
    RC.Client.Version=Version;
    RC.FactoryData=&Remote;
    xmqttclientstoreconfig Store={.Size=sizeof(Store),.Writer=Writer,.Wall=wall,.User=&Wall};
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=Id;
    Connect.Body.Connect.CleanStart=!Resume;
    xmqttproperty Expiry={XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{.Number=300}};
    if(V5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    /* MQTT 3.1.1 needs a persistent first connection to exercise replay.
     * Use a fresh local broker session for this example's fixed client ID. */
    if(!V5)Connect.Body.Connect.CleanStart=false;
    const xmqttreconnectstoreplan Plan={.Session=Session,.Config=&Store,.Factory=connectAttempt};
    RC.Store=&Plan;
    if(!endpointInit(&Remote)||!xmqttReconnectCreate(Engine,Cleanup,0,&RC,&Connect,&Controller))goto Done;
    Session=NULL;
    if(!online(Controller,1,&Lease)||Lease.Generation!=1)goto Done;

    xmqttclientstoreoutcome Outcome;
    if(Resume){
        if(!replayed(Writer))goto Done;
#if defined(RECONNECT_EXAMPLE_COMPACT)
        if(Auto&&!compactInboxAck(Writer,Version,InboxKey))goto Done;
#endif
        puts("Fresh process settled the compacted QoS 2 obligation.");
        goto Disconnect;
    }
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(Auto&&!compactSubscribe(Lease.Client))goto Done;
    if(Cut&&ReplayQuota&&!compactHistory(Lease.Client))goto Done;
#endif
    xmqttpacket Packet;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
    Packet.Flags=Qos2?4:2;
    Packet.Body.Publish.Topic=XRT_STR_LITERAL("xrt/example/reconnect-store");
    Packet.Body.Publish.Payload=XRT_BYTES_LITERAL("durable automatic reconnect");
    if(xmqttClientPublishAsync(Lease.Client,&Packet,10000,&Publish)!=XMQTT_ADMISSION_ACCEPTED)goto Done;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(Cut){
        compactCut(Writer,Publish,Auto,Pressure,InboxKey);
        goto Done;
    }
#endif
    const xmqttwriteresult* First=result(Publish);
    if(!First||!xmqttClientStoreOutcome(First,&Outcome)||Outcome.Attempt.Effect!=XMQTT_JOURNAL_COMMITTED)goto Done;
    if(Replay){
        if(First->Status!=XMQTT_STATUS_TRANSPORT||Outcome.Completion.Sequence||
           !xmqttReconnectRelease(&Lease)||!online(Controller,2,&Lease)||Lease.Generation!=2||
           !replayed(Writer)||xrtFutureValue(Publish)!=First)goto Done;
        if(Qos2)printf("Original PUBREL checkpoint %llu.\n",(unsigned long long)Outcome.Pubrel.Sequence);
    }else if(First->Status!=XMQTT_STATUS_OK||Outcome.Completion.Effect!=XMQTT_JOURNAL_COMMITTED)goto Done;
    printf("Generation %llu settled durable QoS %u. Original status %d, attempt %llu.\n",
        (unsigned long long)Lease.Generation,Qos2?2u:1u,(int)First->Status,
        (unsigned long long)Outcome.Attempt.Sequence);

Disconnect:
    if(xmqttClientDisconnectAsync(Lease.Client,NULL,10000,&Disconnect)!=XMQTT_ADMISSION_ACCEPTED)goto Done;
    const xmqttwriteresult* Last=result(Disconnect);
    if(!Last||Last->Status!=XMQTT_STATUS_OK||!xmqttClientStoreOutcome(Last,&Outcome)||
       Outcome.End.Effect!=XMQTT_JOURNAL_COMMITTED)goto Done;
    printf("Committed graceful END at %llu.\n",(unsigned long long)Outcome.End.Sequence);
    OK=true;

Done:
    if(Lease.Owner&&!xmqttReconnectRelease(&Lease))return 1;
    if(Controller&&(!xmqttReconnectClose(Controller)||xmqttReconnectWaitFor(Controller,10000)!=XWAIT_OK||
       !xmqttReconnectDestroy(Controller)))return 1;
    if(!endpointDestroy(&Remote))return 1;
    xrtFutureDestroy(Disconnect);
    xrtFutureDestroy(Publish);
    if(Session&&!xmqttSessionDestroy(Session))return 1;
    if(Writer&&!xmqttStoreWriteDestroy(Writer))return 1;
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(OK&&Resume&&!compactVerify(Slot,&SC,&Wall,Outcome.End.Sequence))OK=false;
#endif
    if(Started&&!xrtNetEngineStop(Engine))return 1;
    if(Engine&&!xrtNetEngineDestroy(Engine))return 1;
    if(!destroyPool(Cleanup)||!destroyPool(Storage))return 1;
    xmqttStoreStateDestroy(State);
    if(!xmqttJournalDestroy(Journal)){
        (void)xmqttJournalDestroy(Journal);
        OK=false;
    }
#if defined(RECONNECT_EXAMPLE_COMPACT)
    if(!xmqttJournalSlotDestroy(Slot)){
        (void)xmqttJournalSlotDestroy(Slot);
        OK=false;
    }
    if(Root&&!xrtRootClose(Root))OK=false;
#endif
    if(File&&(!xrtClose(File)||!xrtFileDelete(Path)))OK=false;
    xrtFree(Path);
    if(!OK)fputs("Durable MQTT reconnect example failed.\n",stderr);
    return OK?0:1;
}
