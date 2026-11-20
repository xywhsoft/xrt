#include <xmqtt/client_store.h>
#include <xrt/memory.h>
#include <xrt/time.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
#include <xmqtt/client_store_ws.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
#include <xmqtt/client_store_tls.h>
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS) || (defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) && defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS))
#define XMQTT_EXAMPLE_STORE_TLS
#include "../tls/material.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool wall(ptr User,uint64* Time)
{
    uint64* Last=User;
    int64 Current;
    if(!xrtTimeToUnixMs(xrtNow(),&Current)||Current<0)return false;
    if((uint64)Current>*Last)*Last=(uint64)Current;
    *Time=*Last;
    return true;
}

static const void* result(xfuture* Future)
{
    if(xrtFutureWaitFor(Future,10000)!=XWAIT_OK||xrtFutureState(Future)!=XFUTURE_RESOLVED)return NULL;
    return xrtFutureValue(Future);
}

int main(int Argc,char** Argv)
{
    if(Argc==1){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        puts("Usage: examples_client_store_main HOST_OR_NUMERIC_ADDRESS PORT 5|311");
#else
        puts("Usage: examples_client_store_main NUMERIC_ADDRESS PORT 5|311");
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
        puts("       examples_client_store_main HOST PORT 5|311 SERVER_CA.pem");
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
        puts("       examples_client_store_main ws://HOST:PORT/mqtt 5|311");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        puts("       examples_client_store_main wss://HOST:PORT/mqtt 5|311 SERVER_CA.pem");
#endif
#endif
        return 0;
    }

    bool UseWS=false,UseTLS=false,UseHost=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
    UseWS=!strncmp(Argv[1],"ws://",5)||!strncmp(Argv[1],"wss://",6);
    UseTLS=!strncmp(Argv[1],"wss://",6);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
    if(!UseWS)UseTLS=Argc==5;
#endif
    int Level=UseWS?2:3;
    if(Argc!=Level+(UseTLS?2:1))return 2;
#if !defined(XMQTT_EXAMPLE_STORE_TLS)
    if(UseTLS)return 2;
#endif
    bool V5=!strcmp(Argv[Level],"5");
    if(!V5&&strcmp(Argv[Level],"311"))return 2;
    uint16 Port=0;
    xnetaddr Remote;
    if(!UseWS){
        char* End;
        unsigned long Number=strtoul(Argv[2],&End,10);
        if(End==Argv[2]||*End||!Number||Number>65535)return 2;
        Port=(uint16)Number;
        if(!UseTLS)UseHost=!xrtNetAddrParse(&Remote,Argv[1],Port);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        if(UseHost)xrtClearError();
#else
        if(UseHost)return 2;
#endif
    }

    xmqttversion Version=V5?XMQTT_VERSION_5:XMQTT_VERSION_311;
    const xstrview Id=XRT_STR_LITERAL("xrt-durable-demo");
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Version=Version;
    JC.Generation=1;
    JC.Binding=XRT_BYTES_LITERAL("demo/broker/durable-client");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig SC;
    xmqttStoreStateConfigInit(&SC);
    SC.Delta.Version=Version;
    SC.Generation=JC.Generation;
    SC.Binding=JC.Binding;
    SC.ClientId=Id;

    uint64 Wall=0;
    bool OK=false;
    str Path=NULL;
    xfile File=xrtFileTemp(NULL,"xmqtt-durable-client-",NULL,&Path);
    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorewrite* Writer=NULL;
    xmqttsession* Session=NULL;
    xmqttclient* Client=NULL;
    xtaskpool* Pool=NULL;
    xnetengine* Engine=NULL;
    bool EngineStarted=false;
#if defined(XMQTT_EXAMPLE_STORE_TLS)
    xx509store* Trust=NULL;
    xtlsverifier* Verifier=NULL;
#endif
#if defined(XMQTT_EXAMPLE_STORE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetresolver* Resolver=NULL;
#endif
    xfuture* Future=NULL;
    if(!File||!wall(&Wall,&SC.WallMilliseconds)||
       xmqttJournalCreate(File,&JC,&Journal)!=XMQTT_JOURNAL_DONE||
       xmqttStoreStateRecover(Journal,&SC,&State)!=XMQTT_STORE_DONE)goto Cleanup;

    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    Pool=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    if(!Pool||!xmqttStoreWriteCreate(Pool,Journal,State,&WC,&Writer))goto Cleanup;
    Journal=NULL;
    State=NULL;

    xmqttsessionconfig SK;
    xmqttSessionConfigInit(&SK);
    SK.Version=Version;
    SK.ClientId=Id;
    if(!xmqttSessionCreate(&SK,&Session))goto Cleanup;
    xnetengineconfig EC;
    xrtNetEngineConfigInit(&EC);
    EC.Workers=1;
    Engine=xrtNetEngineCreate(&EC);
    if(!Engine||!xrtNetEngineStart(Engine))goto Cleanup;
    EngineStarted=true;

    xmqttclientconfig CK;
    xmqttClientConfigInit(&CK);
    CK.Version=Version;
    xmqttclientstoreconfig DK={.Size=sizeof(DK),.Writer=Writer,.Wall=wall,.User=&Wall};
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=Id;
    Connect.Body.Connect.CleanStart=true;
    bool Connected=false;
#if defined(XMQTT_EXAMPLE_STORE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(UseWS||UseTLS||UseHost){
        Resolver=xrtNetResolverCreate(NULL);
        if(!Resolver)goto Cleanup;
    }
    xstrview Endpoint={Argv[1],strlen(Argv[1])};
#endif
#if defined(XMQTT_EXAMPLE_STORE_TLS)
    if(UseTLS){
        size_t Size=0;
        bytes CA=xmqttExampleTlsRead(Argv[Level+1],65536,&Size);
        if(!CA)goto Cleanup;
        Trust=xrtX509StoreCreate();
        bool Added=Trust&&xrtX509StoreAddData(Trust,(xbytesview){CA,Size},NULL);
        xrtFree(CA);
        if(!Added)goto Cleanup;
        xtlsverifierconfig VC;
        xrtTlsVerifierConfigInit(&VC);
        VC.Store=Trust;
        xtlsverifylimits VL;
        xrtTlsVerifyLimitsInit(&VL);
        Verifier=xrtTlsVerifierCreateLimited(&VC,&VL);
        if(!Verifier)goto Cleanup;
        xmqttclienttlsconfig TLS;
        xmqttClientTlsConfigInit(&TLS);
        TLS.Session.Verifier=Verifier;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) && defined(XMQTT_FEATURE_XMQTT_CLIENT_WS_URI_TLS)
        if(UseWS){
            TLS.Session.ResumeLimit=0;
            Connected=xmqttClientWssConnectUriStore(Engine,Resolver,Endpoint,0,&CK,NULL,&TLS,Session,&DK,&Connect,&Client);
        }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_TLS)
        if(!UseWS)Connected=xmqttClientTlsDialStore(Engine,Resolver,Endpoint,Port,0,&CK,&TLS,Session,&DK,&Connect,&Client);
#endif
    }
#endif
    if(!UseTLS){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS)
        if(UseWS)Connected=xmqttClientWsConnectUriStore(Engine,Resolver,Endpoint,0,&CK,NULL,Session,&DK,&Connect,&Client);
        else
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        if(UseHost)Connected=xmqttClientDialStore(Engine,Resolver,Endpoint,Port,0,&CK,NULL,Session,&DK,&Connect,&Client);
        else
#endif
        Connected=xmqttClientConnectStore(Engine,&Remote,0,&CK,Session,&DK,&Connect,&Client);
    }
    if(!Connected)goto Cleanup;
    Future=xmqttClientReady(Client);
    const xmqttreadyresult* Ready=result(Future);
    if(!Ready||Ready->Status!=XMQTT_STATUS_OK){
        if(Ready&&Ready->Cause)fprintf(stderr,"Connection failed: %s\n",xrtErrorMessage(Ready->Cause));
        goto Cleanup;
    }
    xrtFutureDestroy(Future);
    Future=NULL;

    xmqttpacket Publish;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Publish);
    Publish.Flags=2;
    Publish.Body.Publish.Topic=XRT_STR_LITERAL("xrt/example/durable");
    Publish.Body.Publish.Payload=XRT_BYTES_LITERAL("committed QoS 1 exchange");
    if(xmqttClientPublishAsync(Client,&Publish,10000,&Future)!=XMQTT_ADMISSION_ACCEPTED)goto Cleanup;
    const xmqttwriteresult* Published=result(Future);
    xmqttclientstoreoutcome Outcome;
    if(!Published||Published->Status!=XMQTT_STATUS_OK||Published->Delivery!=XMQTT_PEER_ACCEPTED||
       !xmqttClientStoreOutcome(Published,&Outcome)||Outcome.Attempt.Effect!=XMQTT_JOURNAL_COMMITTED||
       Outcome.Completion.Effect!=XMQTT_JOURNAL_COMMITTED)goto Cleanup;
    printf("MQTT %s committed PUBLISH at %llu and PUBACK at %llu.\n",
           Argv[Level],(unsigned long long)Outcome.Attempt.Sequence,
           (unsigned long long)Outcome.Completion.Sequence);
    xrtFutureDestroy(Future);
    Future=NULL;

    if(xmqttClientDisconnectAsync(Client,NULL,10000,&Future)!=XMQTT_ADMISSION_ACCEPTED)goto Cleanup;
    const xmqttwriteresult* Closed=result(Future);
    if(!Closed||!xmqttClientStoreOutcome(Closed,&Outcome))goto Cleanup;
    if(Closed->Status!=XMQTT_STATUS_OK||Outcome.End.Effect!=XMQTT_JOURNAL_COMMITTED){
        fprintf(stderr,"Disconnect status %d, journal END effect %d.\n",(int)Closed->Status,(int)Outcome.End.Effect);
        goto Cleanup;
    }
    printf("Committed graceful END at %llu.\n",(unsigned long long)Outcome.End.Sequence);
    OK=true;

Cleanup:
    xrtFutureDestroy(Future);
    if(Client&&!xmqttClientDestroy(Client))return 1;
#if defined(XMQTT_EXAMPLE_STORE_TLS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_STORE_WS) || defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(Resolver&&!xrtNetResolverDestroy(Resolver))return 1;
#endif
#if defined(XMQTT_EXAMPLE_STORE_TLS)
    xrtTlsVerifierRelease(Verifier);
    xrtX509StoreFree(Trust);
#endif
    if(Session&&!xmqttSessionDestroy(Session))return 1;
    if(Writer&&!xmqttStoreWriteDestroy(Writer))return 1;
    if(EngineStarted&&!xrtNetEngineStop(Engine))return 1;
    if(Engine&&!xrtNetEngineDestroy(Engine))return 1;
    if(Pool&&(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,10000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool)))return 1;
    xmqttStoreStateDestroy(State);
    if(Journal&&!xmqttJournalDestroy(Journal))return 1;
    if(File&&(!xrtClose(File)||!xrtFileDelete(Path)))OK=false;
    xrtFree(Path);
    if(!OK)fputs("Durable MQTT exchange failed.\n",stderr);
    return OK?0:1;
}
