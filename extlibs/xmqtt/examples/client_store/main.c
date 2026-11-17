#include <xmqtt/client_store.h>
#include <xrt/memory.h>
#include <xrt/time.h>
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
    if(Argc!=4){
        puts("Usage: examples_client_store_main NUMERIC_ADDRESS PORT 5|311");
        return Argc==1?0:2;
    }
    const bool V5=strcmp(Argv[3],"5")==0;
    char* End;
    unsigned long Port=strtoul(Argv[2],&End,10);
    xnetaddr Remote;
    if(End==Argv[2]||*End||Port==0||Port>65535||(!V5&&strcmp(Argv[3],"311"))||
       !xrtNetAddrParse(&Remote,Argv[1],(uint16)Port))return 2;

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
    xfuture* Future=NULL;
    if(!File||!wall(&Wall,&SC.WallMilliseconds)||
       xmqttJournalCreate(File,&JC,&Journal)!=XMQTT_JOURNAL_DONE||
       xmqttStoreStateRecover(Journal,&SC,&State)!=XMQTT_STORE_DONE)goto Cleanup;

    xtaskpoolconfig PC={.Threads=1,.QueueLimit=16};
    Pool=xrtTaskPoolCreate(&PC);
    xmqttstorewriteconfig WC;
    xmqttStoreWriteConfigInit(&WC);
    if(!Pool||!xmqttStoreInboxCreate(Pool,Journal,State,&WC,&Writer))goto Cleanup;
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
    if(!xmqttClientConnectStore(Engine,&Remote,0,&CK,Session,&DK,&Connect,&Client))goto Cleanup;
    Future=xmqttClientReady(Client);
    const xmqttreadyresult* Ready=result(Future);
    if(!Ready||Ready->Status!=XMQTT_STATUS_OK)goto Cleanup;
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
           Argv[3],(unsigned long long)Outcome.Attempt.Sequence,
           (unsigned long long)Outcome.Completion.Sequence);
    xrtFutureDestroy(Future);
    Future=NULL;

    if(xmqttClientDisconnectAsync(Client,NULL,10000,&Future)!=XMQTT_ADMISSION_ACCEPTED)goto Cleanup;
    const xmqttwriteresult* Closed=result(Future);
    if(!Closed||Closed->Status!=XMQTT_STATUS_OK||
       !xmqttClientStoreOutcome(Closed,&Outcome)||Outcome.End.Effect!=XMQTT_JOURNAL_COMMITTED)goto Cleanup;
    printf("Committed graceful END at %llu.\n",(unsigned long long)Outcome.End.Sequence);
    OK=true;

Cleanup:
    xrtFutureDestroy(Future);
    if(Client&&!xmqttClientDestroy(Client))return 1;
    if(Session&&!xmqttSessionDestroy(Session))return 1;
    if(Writer&&!xmqttStoreWriteDestroy(Writer))return 1;
    if(EngineStarted&&!xrtNetEngineStop(Engine))return 1;
    if(Engine&&!xrtNetEngineDestroy(Engine))return 1;
    if(Pool&&(!xrtTaskPoolClose(Pool)||xrtTaskPoolWaitFor(Pool,10000)!=XWAIT_OK||!xrtTaskPoolDestroy(Pool)))return 1;
    xmqttStoreStateDestroy(State);
    if(Journal&&!xmqttJournalDestroy(Journal))return 1;
    if(File&&(!xrtClose(File)||!xrtFileDelete(Path)))OK=false;
    xrtFree(Path);
    if(!OK)fputs("Durable TCP exchange failed.\n",stderr);
    return OK?0:1;
}
