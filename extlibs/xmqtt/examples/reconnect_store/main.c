#include <xmqtt/reconnect_store.h>
#include <xrt/memory.h>
#include <xrt/time.h>
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

static xmqttclientstatus connectAttempt(xnetengine* Engine,uint64 Affinity,const xmqttclientconfig* Config,
    xmqttsession* Session,const xmqttclientstoreconfig* Store,const xmqttpacket* Connect,ptr User,xmqttclient** Client)
{
    return xmqttClientConnectStore(Engine,User,Affinity,Config,Session,Store,Connect,Client)
        ?XMQTT_STATUS_OK:XMQTT_STATUS_INITIALIZATION;
}

static bool online(xmqttreconnect* Controller,xmqttclientlease* Lease)
{
    double Deadline=xrtTimer()+10;
    do{
        xmqttreconnectinfo Info;
        if(xmqttReconnectAcquire(Controller,Lease))return true;
        if(!xmqttReconnectInfo(Controller,&Info)||Info.State>=XMQTT_RECONNECT_STOPPED)return false;
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
        puts("Usage: examples_reconnect_store_main ADDRESS PORT 5|311 [expect-replay]");
        return 0;
    }
    const bool Replay=Argc==5&&!strcmp(Argv[4],"expect-replay");
    if(Argc!=4&&!Replay)return 2;
    bool V5=!strcmp(Argv[3],"5");
    if(!V5&&strcmp(Argv[3],"311"))return 2;
    char* End;
    unsigned long Port=strtoul(Argv[2],&End,10);
    xnetaddr Remote;
    if(End==Argv[2]||*End||!Port||Port>65535||!xrtNetAddrParse(&Remote,Argv[1],(uint16)Port))return 2;

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
    if(!xmqttReconnectCreateStore(Engine,Cleanup,0,&RC,Session,&Store,connectAttempt,&Connect,&Controller))goto Done;
    Session=NULL;
    if(!online(Controller,&Lease)||Lease.Generation!=1)goto Done;

    xmqttpacket Packet;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&Packet);
    Packet.Flags=2;
    Packet.Body.Publish.Topic=XRT_STR_LITERAL("xrt/example/reconnect-store");
    Packet.Body.Publish.Payload=XRT_BYTES_LITERAL("durable automatic reconnect");
    if(xmqttClientPublishAsync(Lease.Client,&Packet,10000,&Publish)!=XMQTT_ADMISSION_ACCEPTED)goto Done;
    const xmqttwriteresult* First=result(Publish);
    xmqttclientstoreoutcome Outcome;
    if(!First||!xmqttClientStoreOutcome(First,&Outcome)||Outcome.Attempt.Effect!=XMQTT_JOURNAL_COMMITTED)goto Done;
    if(Replay){
        if(First->Status!=XMQTT_STATUS_TRANSPORT||Outcome.Completion.Sequence||
           !xmqttReconnectRelease(&Lease)||!online(Controller,&Lease)||Lease.Generation!=2||
           !replayed(Writer)||xrtFutureValue(Publish)!=First)goto Done;
    }else if(First->Status!=XMQTT_STATUS_OK||Outcome.Completion.Effect!=XMQTT_JOURNAL_COMMITTED)goto Done;
    printf("Generation %llu settled durable QoS 1. Original status %d, attempt %llu.\n",
        (unsigned long long)Lease.Generation,(int)First->Status,(unsigned long long)Outcome.Attempt.Sequence);

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
