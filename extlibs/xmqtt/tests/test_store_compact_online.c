#include "test.h"
#include "../examples/common/store_compact_online.h"
#include "../examples/common/journal_files.h"
#include "../examples/common/client_peer.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
typedef enum onlinequota {
    QUOTA_NONE,QUOTA_TRANSACTIONS,QUOTA_OPERATIONS,QUOTA_BYTES
} onlinequota;
typedef struct onlinefixture {
    str Path;
    xroot Root;
    xmqttjournalconfig Journal;
    xmqttstorestateconfig State;
    mqttExampleJournalSlotFiles Files;
    mqttExampleJournalSlot* Slot;
    xtaskpool* Pool;
    xmqttstorewrite* Writer;
    onlinequota Quota;
} onlinefixture;

static void check(bool OK,cstr Message)
{
    if(!OK)fprintf(stderr,"%s: %s\n",Message,xrtGetError()?xrtErrorMessage(xrtGetError()):"no diagnostic");
    testRequire(OK,Message);
}
static const char* const Paths[]={"journal.slot","journal.0","journal.1"};
static void begin(onlinefixture* X,xmqttversion V,bool Expired,bool Pending,uint64 FileBytes)
{
    X->Path=xrtDirTemp(NULL,"xmqtt-online-",NULL);
    X->Root=X->Path?xrtRootOpen(X->Path):NULL;
    check(X->Root!=NULL,"open native fixture");
    xfileoptions O;
    xrtFileOptionsInit(&O);
    O.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    for(size_t I=0;I<3;++I){
        xfile F=xrtRootFileOpen(X->Root,Paths[I],&O);
        check(F&&xrtClose(F),"provision exact slot paths");
    }
    xmqttJournalConfigInit(&X->Journal);
    X->Journal.Binding=XRT_BYTES_LITERAL("test/online");
    X->Journal.Version=V;X->Journal.Generation=1;X->Journal.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    X->Journal.MaxTransactionBytes=8192;
    if(FileBytes)X->Journal.MaxFileBytes=FileBytes;
    check(mqttExampleOpenNativeSlot(X->Root,&X->Journal,true,&X->Files,&X->Slot)==XMQTT_JOURNAL_DONE,"create native selector");
    xmqttStoreStateConfigInit(&X->State);
    X->State.Binding=X->Journal.Binding;X->State.Generation=1;X->State.Delta.Version=V;
    X->State.ClientId=XRT_STR_LITERAL("online");
    X->State.WallMilliseconds=Expired?62000:2000;
    xmqttjournal* J=NULL;
    xmqttjournalrecord Record;
    check(mqttExampleJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE&&
        xmqttJournalNext(J,&Record)==XMQTT_JOURNAL_EMPTY,"open empty typed journal");
    unsigned char Wire[512];
    size_t Bytes;
    uint64 Sequence;
    check(xmqttStoreDeltaWrite(&X->State.Delta,NULL,0,NULL,0,Wire,sizeof(Wire),&Bytes)==XMQTT_STORE_DELTA_DONE,
        "encode ordinary empty delta batch");
    uint64 ReplayBytes=18*Bytes;
    for(unsigned I=0;I<18;++I)
        check(xmqttJournalAppend(J,(xbytesview){Wire,Bytes},&Sequence)==XMQTT_JOURNAL_DONE,"populate native history");
    if(Expired||Pending){
        const unsigned char Five[]={0x30,5,0,1,'t',0,'x'},Three[]={0x30,4,0,1,'t','x'};
        xbytesview Message=V==XMQTT_VERSION_5?(xbytesview){Five,sizeof(Five)}:(xbytesview){Three,sizeof(Three)};
        uint32 Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX;
        const xmqttstoredelta Ops[]={
            {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("online")},
            {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=1001,.Expiry=Expiry},
            Expired?(xmqttstoredelta){.Type=XMQTT_STORE_END,.Epoch=1,.WallMilliseconds=2000,.Expiry=Expiry}:
                (xmqttstoredelta){.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=1,.Data=Message},
        };
        check(xmqttStoreDeltaWrite(&X->State.Delta,Ops,3,NULL,0,Wire,sizeof(Wire),&Bytes)==XMQTT_STORE_DELTA_DONE&&
            xmqttJournalAppend(J,(xbytesview){Wire,Bytes},&Sequence)==XMQTT_JOURNAL_DONE,"native session history");
        ReplayBytes+=Bytes;
    }
    if(X->Quota==QUOTA_TRANSACTIONS)X->State.MaxTransactions=Sequence;
    if(X->Quota==QUOTA_OPERATIONS)X->State.MaxOperations=3;
    if(X->Quota==QUOTA_BYTES)X->State.MaxReplayBytes=ReplayBytes;
    check(mqttExampleJournalSlotClose(X->Slot,J)&&mqttExampleJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE,"fresh replay owner");
    xmqttstorestate* State=NULL;
    check(xmqttStoreStateRecover(J,&X->State,&State)==XMQTT_STORE_DONE,"recover native prefix");
    xtaskpoolconfig T={.Threads=1,.QueueLimit=16};
    X->Pool=xrtTaskPoolCreate(&T);
    xmqttstorewriteconfig C;
    xmqttStoreWriteConfigInit(&C);
    check(X->Pool&&xmqttStoreWriteCreate(X->Pool,J,State,&C,&X->Writer)&&mqttExampleJournalSlotDetach(X->Slot,J),
        "create sole Store Writer");
}
static xmqttstorestateinfo reopen(onlinefixture* X,uint64 Generation)
{
    check(xmqttStoreWriteDestroy(X->Writer),"retire worker before reopening selector");
    X->Writer=NULL;
    check(mqttExampleCloseNativeSlot(X->Slot,&X->Files)&&mqttExampleOpenNativeSlot(X->Root,&X->Journal,false,&X->Files,&X->Slot)==XMQTT_JOURNAL_DONE,
        "fresh control owner");
    mqttExampleJournalSlotInfo S={0};
    check(mqttExampleJournalSlotGetInfo(X->Slot,&S)&&S.Generation==Generation,"precise selected generation on cold reopen");
    X->State.Generation=Generation;
    X->State.WallMilliseconds=70000;
    xmqttjournal* J=NULL;
    xmqttstorestate* State=NULL;
    check(mqttExampleJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(J,&X->State,&State)==XMQTT_STORE_DONE,"typed cold recovery of selected cut");
    xmqttstorestateinfo Info;
    check(xmqttStoreStateInfo(State,&Info),"read typed replay metrics");
    Info.ClientId=(xstrview){0};
    xmqttStoreStateDestroy(State);
    check(mqttExampleJournalSlotClose(X->Slot,J),"retire selected reader");
    return Info;
}
static void end(onlinefixture* X)
{
    check(xmqttStoreWriteDestroy(X->Writer)&&mqttExampleCloseNativeSlot(X->Slot,&X->Files),"retire writer/slot");
    check(xrtTaskPoolClose(X->Pool)&&xrtTaskPoolWaitFor(X->Pool,10000)==XWAIT_OK&&xrtTaskPoolDestroy(X->Pool),"retire storage host");
    for(size_t I=0;I<3;++I)check(xrtRootRemove(X->Root,Paths[I]),"delete exact fixture");
    check(xrtRootClose(X->Root)&&xrtDirRemove(X->Path),"release fixture directory");
    xrtFree(X->Path);
}
static xmqttstorewriteinfo current(xmqttstorewrite* W)
{
    xmqttstorewriteinfo Info;
    check(xmqttStoreWriteInfo(W,&Info),"snapshot active writer");
    return Info;
}
static xfuture* compact(xmqttstorewrite* W,mqttExampleJournalSlot* Slot,const mqttExampleCompactConfig* C)
{
    xfuture* F=NULL;
    double End=xrtTimer()+5;
    xmqttjournaladmission A;
    do{
        check(xrtTimer()<End,"finite storage admission");
        A=mqttExampleCompactAsync(W,Slot,C,10000,&F);
        if(A==XMQTT_JWORK_OVERLOADED)xrtSleep(1);
    }while(A==XMQTT_JWORK_OVERLOADED);
    check(A==XMQTT_JWORK_ACCEPTED&&F,"admit maintenance into existing FIFO");
    return F;
}
static const xmqttstorecheckpointresult* outcome(xfuture* F,xmqttcommitstatus Status,
    xmqttjournalstatus Storage,xmqttjournaleffect Effect)
{
    check(xrtFutureWaitFor(F,10000)==XWAIT_OK,"maintenance completes physically");
    const xmqttstorecheckpointresult* R=xrtFutureValue(F);
    if(R&&(R->Status!=Status||R->StorageStatus!=Storage||R->Effect!=Effect))
        fprintf(stderr,"maintenance result %d/%d/%d, wanted %d/%d/%d\n",
            (int)R->Status,(int)R->StorageStatus,(int)R->Effect,(int)Status,(int)Storage,(int)Effect);
    check(R&&R->Status==Status&&R->StorageStatus==Storage&&R->Effect==Effect,"maintenance typed result");
    return R;
}
static bool wall(ptr C,uint64* Out){(void)C;*Out=3000;return true;}
static xfuture* publish(xmqttclient* Client,uint8 Value)
{
    xmqttpacket P;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    P.Flags=2;
    P.Body.Publish.Topic=XRT_STR_LITERAL("test/online");
    P.Body.Publish.Payload=(xbytesview){&Value,1};
    xfuture* F=NULL;
    check(xmqttClientPublishAsync(Client,&P,10000,&F)==XMQTT_ADMISSION_ACCEPTED,"admit live QoS 1 publish");
    return F;
}
static xmqttclientstoreoutcome published(xfuture* F)
{
    check(xrtFutureWaitFor(F,10000)==XWAIT_OK,"live QoS 1 round trip");
    const xmqttwriteresult* R=xrtFutureValue(F);
    xmqttclientstoreoutcome O;
    check(R&&R->Status==XMQTT_STATUS_OK&&R->Delivery==XMQTT_PEER_ACCEPTED&&xmqttClientStoreOutcome(R,&O)&&
        O.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&O.Completion.Effect==XMQTT_JOURNAL_COMMITTED,
        "durable QoS 1 proof on existing connection");
    xrtFutureDestroy(F);
    return O;
}
static void waitAcks(mqttExamplePeer* Peer,uint32 Count)
{
    double End=xrtTimer()+10;
    while(xrtAtomic32Load(&Peer->Acks,XMEMORY_ACQUIRE)<Count){
        check(xrtTimer()<End,"peer sees inbound PUBACK");
        xrtSleep(1);
    }
}
static void takeEcho(xmqttclient* Client,uint8 Value)
{
    xmqttmessage* Message=NULL;
    check(xmqttClientTakeMessage(Client,&Message),"APP takes accepted echo");
    xbytesview Payload=xmqttMessagePacket(Message)->Body.Publish.Payload;
    check(Payload.Size==1&&Payload.Data[0]==Value,"echo preserves APP payload");
    xmqttMessageRelease(Message);
}
static void connected(xmqttversion V)
{
    onlinefixture X={0};
    begin(&X,V,false,false,0);
    mqttExamplePeer Peer;
    mqttExampleStartMode(&Peer,V,true,false);
    xmqttsessionconfig SC;
    xmqttSessionConfigInit(&SC);
    SC.Version=V;SC.ClientId=X.State.ClientId;
    xmqttsession* Session=NULL;
    check(xmqttSessionCreate(&SC,&Session),"create passive Session");
    xmqttpacket Connect;
    xmqttPacketInit(XMQTT_PACKET_CONNECT,&Connect);
    Connect.Body.Connect.ClientId=SC.ClientId;
    xmqttproperty Expiry={.Id=XMQTT_PROPERTY_SESSION_EXPIRY,.Type=XMQTT_PROPERTY_UINT32,.Value.Number=60};
    if(V==XMQTT_VERSION_5)Connect.Properties=(xmqttpropertylist){&Expiry,1};
    xmqttclientconfig CC;
    xmqttClientConfigInit(&CC);
    CC.Version=V;
    xmqttclientstoreconfig Store={sizeof(Store),X.Writer,wall,NULL};
    xmqttclient* Client=NULL;
    check(xmqttClientConnectStore(Peer.Engine,&Peer.Address,1,&CC,Session,&Store,&Connect,&Client),"connect durable real TCP Client");
    xfuture* R=xmqttClientReady(Client);
    check(xrtFutureWaitFor(R,10000)==XWAIT_OK&&((const xmqttreadyresult*)xrtFutureValue(R))->Status==XMQTT_STATUS_OK,
        "wait for durable broker OPEN");
    xrtFutureDestroy(R);
    xmqttclientstoreoutcome First=published(publish(Client,'a'));
    waitAcks(&Peer,1);
    takeEcho(Client,'a');
    check(!current(X.Writer).State.Inbox,"only the outbound QoS state was journaled");

    mqttExampleCompactConfig C;
    mqttExampleCompactConfigInit(&C);
    C.MinReclaimBytes=1;C.MaxScratchBytes=512;
    C.MaxEntries=4;
    xfuture* F;
#if defined(TEST_ONLINE_FAULTS)
    xrtAtomic32Store(&onlineHook,1,XMEMORY_RELEASE);
    F=compact(X.Writer,X.Slot,&C);
    mqttExampleWait(&onlineEntered);
    check(xrtFutureCancel(F),"request cancellation during inactive native write");
    xrtAtomic32Store(&onlineReleased,1,XMEMORY_RELEASE);
    outcome(F,XMQTT_COMMIT_CANCELLED,XMQTT_JOURNAL_EMPTY,XMQTT_JOURNAL_NOT_COMMITTED);
    xrtFutureDestroy(F);
    xrtAtomic32Store(&onlineHook,2,XMEMORY_RELEASE);
    F=compact(X.Writer,X.Slot,&C);
    outcome(F,XMQTT_COMMIT_STORAGE,XMQTT_JOURNAL_IO,XMQTT_JOURNAL_NOT_COMMITTED);
    check(!current(X.Writer).Worker.Failed,"inactive I/O failure leaves old Writer usable");
    xrtFutureDestroy(F);
    xrtAtomic32Store(&onlineEntered,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineReleased,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineHook,1,XMEMORY_RELEASE);
#endif
    F=compact(X.Writer,X.Slot,&C);
#if defined(TEST_ONLINE_FAULTS)
    mqttExampleWait(&onlineEntered);
    check(current(X.Writer).Worker.Busy,"read model while inactive storage call is held");
#endif
    xfuture* Next=publish(Client,'b');
#if defined(TEST_ONLINE_FAULTS)
    check(xrtFutureState(Next)==XFUTURE_PENDING,"later publish waits for the physical checkpoint");
    xrtAtomic32Store(&onlineReleased,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
#endif
    const xmqttstorecheckpointresult* Cut=outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_DONE,XMQTT_JOURNAL_COMMITTED);
    check(Cut->PreviousGeneration==1&&Cut->Generation==2&&Cut->Sequence==1&&Cut->PreviousBytes>Cut->Bytes,
        "publish exact smaller checkpoint and keep generation-local sequence");
    xmqttclientstoreoutcome Second=published(Next);
    check(Second.Key>First.Key&&Second.Attempt.Sequence==Cut->Sequence+1&&
        Second.Completion.Sequence==Cut->Sequence+2,"live publish continues in the selected journal");
    waitAcks(&Peer,2);
    takeEcho(Client,'b');
    C.MinReclaimBytes=UINT64_MAX;
    xfuture* Skip=compact(X.Writer,X.Slot,&C);
    const xmqttstorecheckpointresult* Empty=outcome(Skip,XMQTT_COMMIT_OK,XMQTT_JOURNAL_EMPTY,XMQTT_JOURNAL_NOT_COMMITTED);
    xmqttstorewriteinfo BeforeNext=current(X.Writer);
    check(Empty->Generation==2&&Empty->Sequence==3&&BeforeNext.Worker.Journal.Sequence==3,
        "insufficient byte reduction preserves the cut");
    xrtFutureDestroy(Skip);

    check(xmqttClientClose(Client)&&xmqttClientWaitFor(Client,10000)==XWAIT_OK&&xmqttClientDestroy(Client),
        "physical client retirement after online rotation");
    mqttExampleStop(&Peer);
    check(xmqttSessionDestroy(Session),"release Session after driver");
    xmqttstorestateinfo Recovered=reopen(&X,2);
    check(!Recovered.Inbox&&!Recovered.Outgoing&&Recovered.LastKey==Second.Key&&!Recovered.LiveAtCut,
        "selected journal replays the protocol state and native END");
    check(Cut->Generation==2&&Cut->Sequence==1,"retained maintenance Future survives writer destruction");
    xrtFutureDestroy(F);
    end(&X);
}
static xmqttjournalstatus stageWithoutPublishing(xmqttstorecheckpointcut* Cut,
    uint64 CurrentBytes,uint64 CurrentDataBytes,ptr Context,uint64 User)
{
    check(CurrentDataBytes>0&&User==17,"callback receives the selected cut and copied APP input");
    check(xmqttStoreCheckpointPublish(Cut,mqttExampleSelectCheckpoint,Context)==XMQTT_JOURNAL_STATE,"require staging before publishing");
    uint64 Projected=0;
    check(xmqttStoreCheckpointMeasure(Cut,&Projected)==XMQTT_JOURNAL_DONE&&Projected<CurrentBytes,
        "measure a smaller equivalent Journal");
    check(xmqttStoreCheckpointStage(Cut,mqttExamplePrepareCheckpoint,Context)==XMQTT_JOURNAL_DONE,"stage without authorizing publication");
    check(xmqttStoreCheckpointStage(Cut,mqttExamplePrepareCheckpoint,Context)==XMQTT_JOURNAL_STATE,"refuse a second staging attempt");
    return XMQTT_JOURNAL_EMPTY;
}

static void abandonStaged(void)
{
    onlinefixture X={0};
    begin(&X,XMQTT_VERSION_5,false,true,0);
    xmqttstorewriteinfo Before=current(X.Writer);
    xmqttstorecheckpointrequest Request={
        sizeof(Request),512,4,stageWithoutPublishing,X.Slot,17
    };
    xfuture* F=NULL;
    check(xmqttStoreWriteCheckpointAsync(X.Writer,&Request,10000,&F)==XMQTT_JWORK_ACCEPTED,
        "queue APP-controlled checkpoint");
    Request.User=18;
    const xmqttstorecheckpointresult* Result=outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_EMPTY,XMQTT_JOURNAL_NOT_COMMITTED);
    check(Result->Generation==1&&Result->Sequence==Before.Worker.Journal.Sequence&&
        Result->Bytes==Before.Worker.Journal.CommittedEnd&&!current(X.Writer).Worker.Failed,
        "return without Publish leaves the original Writer selected");
    xrtFutureDestroy(F);
    check(reopen(&X,1).Transactions==Before.State.Transactions,"reopen the old generation after explicit abandonment");
    end(&X);
}

static void expired(void)
{
    onlinefixture X={0};
    begin(&X,XMQTT_VERSION_5,true,false,0);
    mqttExampleCompactConfig C;
    mqttExampleCompactConfigInit(&C);
    C.MinReclaimBytes=1;
    xfuture* F=compact(X.Writer,X.Slot,&C);
    outcome(F,XMQTT_COMMIT_INVALID,XMQTT_JOURNAL_STATE,XMQTT_JOURNAL_NOT_COMMITTED);
    xrtFutureDestroy(F);
    xmqttstoredelta Reset={.Type=XMQTT_STORE_OPEN,.Epoch=2,.WallMilliseconds=62001,.Expiry=60};
    unsigned char Buffer[128];
    size_t Bytes;
    check(xmqttStoreDeltaWrite(&X.State.Delta,&Reset,1,NULL,0,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttStoreWriteAppendAsync(X.Writer,(xbytesview){Buffer,Bytes},10000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,10000)==XWAIT_OK,"durable reset establishes compaction cut");
    check(((const xmqttcommitresult*)xrtFutureValue(F))->Effect==XMQTT_JOURNAL_COMMITTED,"persist reset");
    xrtFutureDestroy(F);
    F=compact(X.Writer,X.Slot,&C);
    outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_DONE,XMQTT_JOURNAL_COMMITTED);
    xrtFutureDestroy(F);
    check(reopen(&X,2).Epoch==2,"reopen exact reset epoch");
    end(&X);
}
#if defined(TEST_ONLINE_FAULTS)
static void uncertain(xmqttversion V)
{
    onlinefixture X={0};
    begin(&X,V,false,false,0);
    mqttExampleCompactConfig C;
    mqttExampleCompactConfigInit(&C);
    C.MinReclaimBytes=1;
    xrtAtomic32Store(&onlineHook,3,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineWrites,0,XMEMORY_RELEASE);
    xfuture* F=compact(X.Writer,X.Slot,&C);
    outcome(F,XMQTT_COMMIT_STORAGE,XMQTT_JOURNAL_UNCERTAIN,XMQTT_JOURNAL_MAY_BE_COMMITTED);
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
    check(current(X.Writer).Worker.Failed&&current(X.Writer).Worker.Closed,
        "ambiguous selector physically fences future admissions");
    xfuture* Unchanged=NULL;
    check(mqttExampleCompactAsync(X.Writer,X.Slot,&C,1000,&Unchanged)==XMQTT_JWORK_CLOSED&&!Unchanged,
        "fenced owner refuses further replacement");
    check(reopen(&X,2).Transactions==0,"first durable selector recovers new empty cut");
    xrtFutureDestroy(F);

    xmqttjournal* Journal=NULL;
    xmqttstorestate* State=NULL;
    xmqttstorewriteconfig Config;
    xmqttStoreWriteConfigInit(&Config);
    check(mqttExampleJournalSlotOpenCurrent(X.Slot,&Journal)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(Journal,&X.State,&State)==XMQTT_STORE_DONE&&
        xmqttStoreWriteCreate(X.Pool,Journal,State,&Config,&X.Writer)&&mqttExampleJournalSlotDetach(X.Slot,Journal),
        "start Writer on the selected generation with a stale selector witness");
    unsigned char Buffer[128];
    size_t Bytes=0;
    F=NULL;
    check(xmqttStoreDeltaWrite(&X.State.Delta,NULL,0,NULL,0,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttStoreWriteAppendAsync(X.Writer,(xbytesview){Buffer,Bytes},10000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,10000)==XWAIT_OK,"append a batch eligible for another checkpoint");
    check(((const xmqttcommitresult*)xrtFutureValue(F))->Effect==XMQTT_JOURNAL_COMMITTED,"batch committed");
    xrtFutureDestroy(F);

    xrtAtomic32Store(&onlineHook,4,XMEMORY_RELEASE);
    F=compact(X.Writer,X.Slot,&C);
    outcome(F,XMQTT_COMMIT_STORAGE,XMQTT_JOURNAL_IO,XMQTT_JOURNAL_NOT_COMMITTED);
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
    check(current(X.Writer).Worker.Failed&&current(X.Writer).Worker.Closed,
        "failed selector repair fences the current Writer");
    xrtFutureDestroy(F);
    check(reopen(&X,2).Transactions==1,"selected history survives a failed selector repair");
    end(&X);
}
#endif
static void explicitPressure(xmqttversion Version,onlinequota Quota,uint64 FileBytes)
{
    onlinefixture X={.Quota=Quota};
    begin(&X,Version,false,true,FileBytes);
    xmqttstorewriteinfo Before=current(X.Writer);
    check(Before.State.Inbox==1&&Before.State.Transactions==19&&Before.State.Operations==3,
        "recover exactly one pending delivery at the configured limit");

    unsigned char Buffer[128];

    xmqttstoreview View;
    size_t Bytes;
    xfuture* F=NULL;
    check(xmqttStoreWriteReadPending(X.Writer,Buffer,sizeof(Buffer),&View,&Bytes)==XMQTT_STORE_DONE&&View.Key==1,
        "take delivery before the refusal");
    check(xmqttStoreWriteRetireAsync(X.Writer,View.Epoch,View.Key,10000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,10000)==XWAIT_OK,"get terminal ACK refusal");
    const xmqttcommitresult* R=xrtFutureValue(F);
    check(R&&R->Status==XMQTT_COMMIT_LIMIT&&R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&
        !R->AttemptSequence&&R->AppendCalled==(Quota==QUOTA_NONE),
        "quota refusal cannot append or rotate the selected cut");
    xrtFutureDestroy(F);

    xmqttstorewriteinfo Refused=current(X.Writer);
    check(Refused.Worker.Journal.Sequence==Before.Worker.Journal.Sequence&&
        Refused.Worker.Journal.CommittedEnd==Before.Worker.Journal.CommittedEnd&&
        Refused.State.Inbox==1&&!Refused.Worker.Closed,
        "selected history and pending delivery survive the refusal");

    mqttExampleCompactConfig C;
    mqttExampleCompactConfigInit(&C);
    C.MinReclaimBytes=1;
    C.MaxScratchBytes=512;
    C.MaxEntries=4;
    F=compact(X.Writer,X.Slot,&C);
    const xmqttstorecheckpointresult* Rotation=outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_DONE,XMQTT_JOURNAL_COMMITTED);
    check(Rotation->PreviousGeneration==1&&Rotation->Generation==2&&
        Rotation->Sequence==2&&Rotation->Bytes<Rotation->PreviousBytes,
        "the explicit APP operation reclaims capacity");
    xrtFutureDestroy(F);

    F=NULL;
    check(xmqttStoreWriteReadPending(X.Writer,Buffer,sizeof(Buffer),&View,&Bytes)==XMQTT_STORE_DONE&&View.Key==1,
        "a definite refusal releases the exact delivery for APP retry");
    check(xmqttStoreWriteRetireAsync(X.Writer,View.Epoch,View.Key,10000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,10000)==XWAIT_OK,"submit the ACK again after rotation");
    R=xrtFutureValue(F);
    check(R&&R->Status==XMQTT_COMMIT_OK&&R->Effect==XMQTT_JOURNAL_COMMITTED&&
        R->AppendCalled&&R->AttemptSequence==3&&R->Sequence==3,"one ACK on the new generation");
    xrtFutureDestroy(F);
    xmqttstorestateinfo S=reopen(&X,2);
    check(S.Transactions==3&&S.Operations==3&&!S.Inbox&&S.LastKey==1,
        "cold replay under unchanged limits preserves exactly one ACK");
    end(&X);
}
#endif
int main(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    connected(XMQTT_VERSION_5);
    connected(XMQTT_VERSION_311);
    expired();
    abandonStaged();
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_311:XMQTT_VERSION_5;
        explicitPressure(V,QUOTA_NONE,14350);
        explicitPressure(V,QUOTA_TRANSACTIONS,0);
        explicitPressure(V,QUOTA_OPERATIONS,0);
        explicitPressure(V,QUOTA_BYTES,0);
        explicitPressure(V,QUOTA_TRANSACTIONS,14350);
    }
    puts("[PASS] storage quotas refuse without rotation, APP compaction and one ACK retry");
    puts("[PASS] online APP rotation on the same TCP connection, continued protocol writes, FIFO and expiry");
#if defined(TEST_ONLINE_FAULTS)
    uncertain(XMQTT_VERSION_5);
    uncertain(XMQTT_VERSION_311);
    puts("[PASS] online checkpoint cancellation, inactive I/O, selector publication and repair fencing");
#endif
#else
    puts("[SKIP] native store compaction requires Windows or Linux");
#endif
    return 0;
}
