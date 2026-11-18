#include <xmqtt/reconnect_store.h>
#include <xrt/memory.h>
#include <xrt/time.h>
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
    if(Argc<Mode||Argc>Mode+1)return 2;
    bool Replay=Argc==Mode+1;
    bool Qos2=Replay&&!strcmp(Argv[Mode],"replay-qos2");
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
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Delta.Version=Version;
    SC.Generation=JC.Generation;
    SC.Binding=JC.Binding;
    SC.ClientId=Id;

    bool OK=false,Started=false;
    uint64 Wall=0;
    str Path=NULL;
    xfile File=xrtFileTemp(NULL,"xmqtt-reconnect-store-",NULL,&Path);
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorewrite* Writer=NULL;
    xmqttsession* Session=NULL;
    xmqttreconnect* Controller=NULL;
    xmqttclientlease Lease={0};
    xtaskpool *Storage=NULL,*Cleanup=NULL;
    xnetengine* Engine=NULL;
    xfuture *Publish=NULL,*Disconnect=NULL;
    if(!File||!wall(&Wall,&SC.WallMilliseconds)||
       xmqttJournalCreate(File,&JC,&Journal)!=XMQTT_JOURNAL_DONE||
       xmqttStoreStateRecover(Journal,&SC,&State)!=XMQTT_STORE_DONE)goto Done;
    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    Storage=xrtTaskPoolCreate(&PC);
    Cleanup=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    if(!Storage||!Cleanup||!xmqttStoreInboxCreate(Storage,Journal,State,&WC,&Writer))goto Done;
    Journal=NULL;
    State=NULL;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=Id;
    if(!xmqttSessionCreate(&SK,&Session))goto Done;
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
    Connect.Body.Connect.CleanStart=true;
    xmqttproperty Expiry={XMQTT_PROPERTY_SESSION_EXPIRY,XMQTT_PROPERTY_UINT32,{.Number=300}};
    if(V5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    /* MQTT 3.1.1 needs a persistent first connection to exercise replay.
     * Use a fresh local broker session for this example's fixed client ID. */
    if(!V5)Connect.Body.Connect.CleanStart=false;
    if(!endpointInit(&Remote)||!xmqttReconnectCreateStore(Engine,Cleanup,0,&RC,Session,&Store,connectAttempt,&Connect,&Controller))goto Done;
    Session=NULL;
    if(!online(Controller,1,&Lease)||Lease.Generation!=1)goto Done;

    xmqttpacket Packet;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
    Packet.Flags=Qos2?4:2;
    Packet.Body.Publish.Topic=XRT_STR_LITERAL("xrt/example/reconnect-store");
    Packet.Body.Publish.Payload=XRT_BYTES_LITERAL("durable automatic reconnect");
    if(xmqttClientPublishAsync(Lease.Client,&Packet,10000,&Publish)!=XMQTT_ADMISSION_ACCEPTED)goto Done;
    const xmqttwriteresult* First=result(Publish);
    xmqttclientstoreoutcome Outcome;
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
    if(Started&&!xrtNetEngineStop(Engine))return 1;
    if(Engine&&!xrtNetEngineDestroy(Engine))return 1;
    if(!destroyPool(Cleanup)||!destroyPool(Storage))return 1;
    xmqttStoreStateDestroy(State);
    if(Journal&&!xmqttJournalDestroy(Journal))return 1;
    if(File&&(!xrtClose(File)||!xrtFileDelete(Path)))OK=false;
    xrtFree(Path);
    if(!OK)fputs("Durable MQTT reconnect example failed.\n",stderr);
    return OK?0:1;
}
