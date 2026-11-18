#include "test.h"
#include "../examples/common/client_peer.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
typedef struct onlinefixture {
    str Path;
    xroot Root;
    xmqttjournalconfig Journal;
    xmqttstorestateconfig State;
    xmqttjournalslot* Slot;
    xtaskpool* Pool;
    xmqttstorewrite* Writer;
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
    check(xmqttJournalSlotCreate(X->Root,&X->Journal,&X->Slot)==XMQTT_JOURNAL_DONE,"create native selector");
    xmqttStoreStateConfigInit(&X->State);
    X->State.Binding=X->Journal.Binding;X->State.Generation=1;X->State.Delta.Version=V;
    X->State.ClientId=XRT_STR_LITERAL("online");
    X->State.WallMilliseconds=Expired?62000:2000;
    xmqttjournal* J=NULL;
    xmqttjournalrecord Record;
    check(xmqttJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE&&
        xmqttJournalNext(J,&Record)==XMQTT_JOURNAL_EMPTY,"open empty typed journal");
    unsigned char Wire[512];
    size_t Bytes;
    uint64 Sequence;
    check(xmqttStoreDeltaWrite(&X->State.Delta,NULL,0,NULL,0,Wire,sizeof(Wire),&Bytes)==XMQTT_STORE_DELTA_DONE,
        "encode ordinary empty delta batch");
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
            xmqttJournalAppend(J,(xbytesview){Wire,Bytes},&Sequence)==XMQTT_JOURNAL_DONE,"native expired history");
    }
    check(xmqttJournalDestroy(J)&&xmqttJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE,"fresh replay owner");
    xmqttstorestate* State=NULL;
    check(xmqttStoreStateRecover(J,&X->State,&State)==XMQTT_STORE_DONE,"recover native prefix");
    xtaskpoolconfig T={.Threads=1,.QueueLimit=16};
    X->Pool=xrtTaskPoolCreate(&T);
    xmqttstorewriteconfig C;
    xmqttStoreWriteConfigInit(&C);
    check(X->Pool&&xmqttStoreInboxCreate(X->Pool,J,State,&C,&X->Writer),"create sole Store Writer");
}
static xmqttstorestateinfo reopen(onlinefixture* X,uint64 Generation)
{
    check(xmqttStoreWriteDestroy(X->Writer),"retire worker before reopening selector");
    X->Writer=NULL;
    check(xmqttJournalSlotDestroy(X->Slot)&&xmqttJournalSlotOpen(X->Root,&X->Journal,&X->Slot)==XMQTT_JOURNAL_DONE,
        "fresh control owner");
    xmqttjournalslotinfo S;
    check(xmqttJournalSlotInfo(X->Slot,&S)&&S.Generation==Generation,"precise selected generation on cold reopen");
    X->State.Generation=Generation;
    X->State.WallMilliseconds=70000;
    xmqttjournal* J=NULL;
    xmqttstorestate* State=NULL;
    check(xmqttJournalSlotOpenCurrent(X->Slot,&J)==XMQTT_JOURNAL_DONE&&
        xmqttStoreStateRecover(J,&X->State,&State)==XMQTT_STORE_DONE,"typed cold recovery of selected cut");
    xmqttstorestateinfo Info;
    check(xmqttStoreStateInfo(State,&Info),"read typed replay metrics");
    Info.ClientId=(xstrview){0};
    xmqttStoreStateDestroy(State);
    check(xmqttJournalDestroy(J),"retire selected reader");
    return Info;
}
static void end(onlinefixture* X)
{
    check(xmqttStoreWriteDestroy(X->Writer)&&xmqttJournalSlotDestroy(X->Slot),"retire writer/slot");
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
static xfuture* compact(xmqttstorewrite* W,const xmqttstorecompactconfig* C)
{
    xfuture* F=NULL;
    double End=xrtTimer()+5;
    xmqttjournaladmission A;
    do{
        check(xrtTimer()<End,"finite storage admission");
        A=xmqttStoreWriteCompactAsync(W,C,10000,&F);
        if(A==XMQTT_JWORK_OVERLOADED)xrtSleep(1);
    }while(A==XMQTT_JWORK_OVERLOADED);
    check(A==XMQTT_JWORK_ACCEPTED&&F,"admit maintenance into existing FIFO");
    return F;
}
static const xmqttstorecompactresult* outcome(xfuture* F,xmqttcommitstatus Status,
    xmqttjournalstatus Storage,xmqttjournaleffect Effect)
{
    check(xrtFutureWaitFor(F,10000)==XWAIT_OK,"maintenance completes physically");
    const xmqttstorecompactresult* R=xrtFutureValue(F);
    if(R&&(R->Status!=Status||R->StorageStatus!=Storage||R->Effect!=Effect))
        fprintf(stderr,"maintenance result %d/%d/%d, wanted %d/%d/%d\n",
            (int)R->Status,(int)R->StorageStatus,(int)R->Effect,(int)Status,(int)Storage,(int)Effect);
    check(R&&R->Status==Status&&R->StorageStatus==Storage&&R->Effect==Effect,"maintenance typed result");
    return R;
}
static bool wall(ptr C,uint64* Out){(void)C;*Out=3000;return true;}
static void publish(xmqttclient* Client,uint8 Value)
{
    xmqttpacket P;
    xmqttPacketInit(XMQTT_PACKET_PUBLISH,&P);
    P.Flags=2;
    P.Body.Publish.Topic=XRT_STR_LITERAL("test/online");
    P.Body.Publish.Payload=(xbytesview){&Value,1};
    xfuture* F=NULL;
    check(xmqttClientPublishAsync(Client,&P,10000,&F)==XMQTT_ADMISSION_ACCEPTED&&
        xrtFutureWaitFor(F,10000)==XWAIT_OK,"live QoS 1 round trip");
    const xmqttwriteresult* R=xrtFutureValue(F);
    xmqttclientstoreoutcome O;
    check(R&&R->Status==XMQTT_STATUS_OK&&R->Delivery==XMQTT_PEER_ACCEPTED&&xmqttClientStoreOutcome(R,&O)&&
        O.Attempt.Effect==XMQTT_JOURNAL_COMMITTED&&O.Completion.Effect==XMQTT_JOURNAL_COMMITTED,
        "durable QoS 1 proof on existing connection");
    xrtFutureDestroy(F);
}
static void waitAcks(mqttExamplePeer* Peer,uint32 Count)
{
    double End=xrtTimer()+10;
    while(xrtAtomic32Load(&Peer->Acks,XMEMORY_ACQUIRE)<Count){
        check(xrtTimer()<End,"peer sees durable inbound PUBACK");
        xrtSleep(1);
    }
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
    publish(Client,'a');
    waitAcks(&Peer,1);
    unsigned char Buffer[128];
    xmqttstorelease Lease;
    xmqttstoreview View;
    size_t Bytes;
    check(xmqttStoreInboxTake(X.Writer,Buffer,sizeof(Buffer),&Lease,&View,&Bytes)==XMQTT_STORE_DONE,
        "hold pending delivery lease across rotation");
    uint64 FirstKey=View.Key;

    xmqttstorecompactconfig C;
    xmqttStoreCompactConfigInit(&C);
    C.MinReclaimBytes=1;C.MaxScratchBytes=512;
    C.MaxEntries=0;
    xfuture* F=compact(X.Writer,&C);
    outcome(F,XMQTT_COMMIT_LIMIT,XMQTT_JOURNAL_LIMIT,XMQTT_JOURNAL_NOT_COMMITTED);
    xrtFutureDestroy(F);
    C.MaxEntries=4;
#if defined(TEST_ONLINE_FAULTS)
    xrtAtomic32Store(&onlineHook,1,XMEMORY_RELEASE);
    F=compact(X.Writer,&C);
    mqttExampleWait(&onlineEntered);
    check(xrtFutureCancel(F),"request cancellation during inactive native write");
    xrtAtomic32Store(&onlineReleased,1,XMEMORY_RELEASE);
    outcome(F,XMQTT_COMMIT_CANCELLED,XMQTT_JOURNAL_EMPTY,XMQTT_JOURNAL_NOT_COMMITTED);
    xrtFutureDestroy(F);
    xrtAtomic32Store(&onlineHook,2,XMEMORY_RELEASE);
    F=compact(X.Writer,&C);
    outcome(F,XMQTT_COMMIT_STORAGE,XMQTT_JOURNAL_IO,XMQTT_JOURNAL_NOT_COMMITTED);
    check(!current(X.Writer).Worker.Failed,"inactive I/O failure leaves old Writer usable");
    xrtFutureDestroy(F);
    xrtAtomic32Store(&onlineEntered,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineReleased,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineHook,1,XMEMORY_RELEASE);
#endif
    F=compact(X.Writer,&C);
#if defined(TEST_ONLINE_FAULTS)
    mqttExampleWait(&onlineEntered);
    check(current(X.Writer).Worker.Busy,"read model while inactive storage call is held");
#endif
    xfuture* Ack=NULL;
    check(xmqttStoreInboxAckAsync(X.Writer,Lease,10000,&Ack)==XMQTT_JWORK_ACCEPTED,
        "same lease can queue an ACK behind the rotation");
#if defined(TEST_ONLINE_FAULTS)
    check(xrtFutureState(Ack)==XFUTURE_PENDING,"later ACK waits for the physical checkpoint");
    xrtAtomic32Store(&onlineReleased,1,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
#endif
    const xmqttstorecompactresult* Cut=outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_DONE,XMQTT_JOURNAL_COMMITTED);
    check(Cut->PreviousGeneration==1&&Cut->Generation==2&&Cut->Sequence==2&&Cut->PreviousBytes>Cut->Bytes,
        "publish exact smaller checkpoint and keep generation-local sequence");
    check(xrtFutureWaitFor(Ack,10000)==XWAIT_OK,"later ACK retires");
    const xmqttcommitresult* A=xrtFutureValue(Ack);
    check(A&&A->Effect==XMQTT_JOURNAL_COMMITTED&&A->Sequence==Cut->Sequence+1,"leased application ACK is the first new delta");
    xrtFutureDestroy(Ack);
    C.MinReclaimBytes=UINT64_MAX;
    xfuture* Skip=compact(X.Writer,&C);
    const xmqttstorecompactresult* Empty=outcome(Skip,XMQTT_COMMIT_OK,XMQTT_JOURNAL_EMPTY,XMQTT_JOURNAL_NOT_COMMITTED);
    xmqttstorewriteinfo BeforeNext=current(X.Writer);
    check(Empty->Generation==2&&Empty->Sequence==3&&BeforeNext.Worker.Journal.Sequence==3,
        "insufficient byte reduction preserves the cut");
    xrtFutureDestroy(Skip);

    publish(Client,'b');
    waitAcks(&Peer,2);
    check(xmqttStoreInboxTake(X.Writer,Buffer,sizeof(Buffer),&Lease,&View,&Bytes)==XMQTT_STORE_DONE&&
        View.Key>FirstKey&&xmqttStoreInboxRelease(X.Writer,Lease),"same TCP connection advances durable delivery keys");
    check(xmqttClientClose(Client)&&xmqttClientWaitFor(Client,10000)==XWAIT_OK&&xmqttClientDestroy(Client),
        "physical client retirement after online rotation");
    mqttExampleStop(&Peer);
    check(xmqttSessionDestroy(Session),"release Session after driver");
    xmqttstorestateinfo Recovered=reopen(&X,2);
    check(Recovered.Inbox==1&&Recovered.LastKey>FirstKey&&!Recovered.LiveAtCut,
        "new generation replays late ACK, second publish, and native END");
    check(Cut->Generation==2&&Cut->Sequence==2,"retained maintenance Future survives writer destruction");
    xrtFutureDestroy(F);
    end(&X);
}
static void expired(void)
{
    onlinefixture X={0};
    begin(&X,XMQTT_VERSION_5,true,false,0);
    xmqttstorecompactconfig C;
    xmqttStoreCompactConfigInit(&C);
    C.MinReclaimBytes=1;
    xfuture* F=compact(X.Writer,&C);
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
    F=compact(X.Writer,&C);
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
    xmqttstorecompactconfig C;
    xmqttStoreCompactConfigInit(&C);
    C.MinReclaimBytes=1;
    xrtAtomic32Store(&onlineHook,3,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineWrites,0,XMEMORY_RELEASE);
    xfuture* F=compact(X.Writer,&C);
    outcome(F,XMQTT_COMMIT_STORAGE,XMQTT_JOURNAL_UNCERTAIN,XMQTT_JOURNAL_MAY_BE_COMMITTED);
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
    check(current(X.Writer).Worker.Failed&&current(X.Writer).Worker.Closed,
        "ambiguous selector physically fences future admissions");
    xfuture* Unchanged=NULL;
    check(xmqttStoreWriteCompactAsync(X.Writer,&C,1000,&Unchanged)==XMQTT_JWORK_CLOSED&&!Unchanged,
        "fenced owner refuses further replacement");
    check(reopen(&X,2).Transactions==0,"first durable selector recovers new empty cut");
    xrtFutureDestroy(F);
    end(&X);
}
#endif
static xmqttstoreautocompactinfo autoInfo(xmqttstorewrite* Writer)
{
    xmqttstoreautocompactinfo Info;
    check(xmqttStoreWriteAutoCompactInfo(Writer,&Info),"read coherent automatic policy snapshot");
    return Info;
}
static xmqttstoreautocompactconfig autoConfig(uint64 Trigger)
{
    xmqttstoreautocompactconfig C;
    check(xmqttStoreAutoCompactConfigInit(&C),"automatic policy defaults");
    C.TriggerBytes=Trigger;
    C.RetryGrowthBytes=250;
    C.Compact.MinReclaimBytes=1;
    C.Compact.MaxScratchBytes=512;
    C.Compact.MaxEntries=4;
    return C;
}
static void autoIdle(xmqttstorewrite* Writer)
{
    double End=xrtTimer()+10;
    for(;;){
        xmqttstorewriteinfo I=current(Writer);
        if(!I.Worker.Tasks&&!I.Worker.Requests&&!I.Worker.Busy)return;
        check(xrtTimer()<End,"storage pump physically retires");
        xrtSleep(1);
    }
}
static xfuture* autoAppend(onlinefixture* X,const xmqttstoredelta* O,size_t N)
{
    unsigned char B[512];
    size_t Bytes;
    xfuture* F=NULL;
    check(xmqttStoreDeltaWrite(&X->State.Delta,O,N,NULL,0,B,sizeof(B),&Bytes)==XMQTT_STORE_DELTA_DONE,
        "encode automatic compaction fixture batch");
    check(xmqttStoreWriteAppendAsync(X->Writer,(xbytesview){B,Bytes},10000,&F)==XMQTT_JWORK_ACCEPTED,
        "queue foreground batch");
    return F;
}
static const xmqttcommitresult* autoCommit(xfuture* F)
{
    check(xrtFutureWaitFor(F,10000)==XWAIT_OK,"foreground physically completes");
    const xmqttcommitresult* R=xrtFutureValue(F);
    check(R!=NULL,"foreground typed result");
    return R;
}
typedef struct onlineblocker {xatomic32 Entered,Released;} onlineblocker;
static xtaskoutcome holdPump(xcancel* Cancel,ptr Context,xtaskvalue* Result)
{
    (void)Cancel;(void)Result;
    onlineblocker* B=Context;
    xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);
    mqttExampleWait(&B->Released);
    return XTASK_SUCCESS;
}
static void automaticIdle(xmqttversion V)
{
    onlinefixture X={0};
    begin(&X,V,false,false,0);
    xmqttstoreautocompactconfig C=autoConfig(1);
    check(xmqttStoreWriteSetAutoCompact(X.Writer,&C)&&autoInfo(X.Writer).Enabled,"equip idle native Writer");
    onlineblocker B;
    xrtAtomic32Init(&B.Entered,0);xrtAtomic32Init(&B.Released,0);
    xfuture* Hold=xrtTaskSubmit(X.Pool,holdPump,&B,NULL);
    check(Hold!=NULL,"hold storage pool ahead of foreground FIFO");
    mqttExampleWait(&B.Entered);
    xfuture* First=autoAppend(&X,NULL,0);
    xfuture* Second=autoAppend(&X,NULL,0);
    check(!xmqttStoreWriteSetAutoCompact(X.Writer,NULL)&&autoInfo(X.Writer).Attempts==0,
        "queued foreground keeps policy stable and runs ahead of idle maintenance");
    xrtClearError();
    xrtAtomic32Store(&B.Released,1,XMEMORY_RELEASE);
    check(autoCommit(First)->Sequence==19&&autoCommit(Second)->Sequence==20,"both foreground batches stay on original cut");
    check(xrtFutureWaitFor(Hold,10000)==XWAIT_OK,"physical blocker retirement");
    autoIdle(X.Writer);
    xmqttstoreautocompactinfo I=autoInfo(X.Writer);
    check(I.Attempts==1&&I.Rotations==1&&I.Generation==2&&I.Bytes<I.PreviousBytes&&
        I.Status==XMQTT_COMMIT_OK&&I.Effect==XMQTT_JOURNAL_COMMITTED,"one idle rotation after FIFO drains");
    check(xmqttStoreWriteSetAutoCompact(X.Writer,NULL)&&!autoInfo(X.Writer).Enabled,"disable at an idle boundary");
    check(reopen(&X,2).Transactions==0&&autoCommit(Second)->Sequence==20,"retained result and exact physical selection");
    xrtFutureDestroy(Second);xrtFutureDestroy(First);xrtFutureDestroy(Hold);
    end(&X);
}
static void automaticBackoff(xmqttversion V)
{
    onlinefixture X={0};
    begin(&X,V,false,false,0);
    xmqttstoreautocompactconfig C=autoConfig(1);
    C.Compact.MinReclaimBytes=UINT64_MAX;
    check(xmqttStoreWriteSetAutoCompact(X.Writer,&C),"install growth-gated policy");
    for(unsigned I=0;I<4;++I){
        xfuture* F=autoAppend(&X,NULL,0);
        check(autoCommit(F)->Effect==XMQTT_JOURNAL_COMMITTED,"ordinary empty batch commits");
        xrtFutureDestroy(F);
        autoIdle(X.Writer);
        xmqttstoreautocompactinfo Info=autoInfo(X.Writer);
        check(Info.Attempts==(I==3?2u:1u)&&!Info.Rotations&&Info.StorageStatus==XMQTT_JOURNAL_EMPTY&&
            Info.NextEligibleBytes==Info.PreviousBytes+C.RetryGrowthBytes,"skip measurement until committed growth reaches its floor");
    }
    check(current(X.Writer).Worker.Journal.Sequence==22,"growth suppression retains native history");
    xmqttstorecompactconfig Manual=C.Compact;
    Manual.MinReclaimBytes=1;
    xfuture* F=compact(X.Writer,&Manual);
    outcome(F,XMQTT_COMMIT_OK,XMQTT_JOURNAL_DONE,XMQTT_JOURNAL_COMMITTED);
    xrtFutureDestroy(F);
    autoIdle(X.Writer);
    xmqttstoreautocompactinfo After=autoInfo(X.Writer);
    check(After.Attempts==2&&!After.NextEligibleBytes&&!After.RetryAfter,"manual rotation clears old generation backoff");
    check(reopen(&X,2).Transactions==0,"manual replacement is selected");
    end(&X);
}
enum {AUTO_PRESSURE_DONE,AUTO_PRESSURE_UNCERTAIN,AUTO_PRESSURE_RETRY};
static void automaticPressure(xmqttversion V,unsigned Mode)
{
    onlinefixture X={0};
    begin(&X,V,false,true,14350);
    xmqttstoreautocompactconfig C=autoConfig(UINT64_MAX);
    if(Mode==AUTO_PRESSURE_RETRY)C.RetryDelayMs=40;
    check(xmqttStoreWriteSetAutoCompact(X.Writer,&C),"enable pressure-only policy for native capacity fixture");
    unsigned char B[128];
    xmqttstorelease Lease;
    xmqttstoreview View;
    size_t Bytes;
    check(xmqttStoreInboxTake(X.Writer,B,sizeof(B),&Lease,&View,&Bytes)==XMQTT_STORE_DONE&&View.Key==1,
        "register exact delivery ACK reservation");
#if defined(TEST_ONLINE_FAULTS)
    if(Mode==AUTO_PRESSURE_UNCERTAIN){
        xrtAtomic32Store(&onlineWrites,0,XMEMORY_RELEASE);
        xrtAtomic32Store(&onlineHook,3,XMEMORY_RELEASE);
    }else if(Mode==AUTO_PRESSURE_RETRY)xrtAtomic32Store(&onlineHook,2,XMEMORY_RELEASE);
#endif
    xfuture* F=NULL;
    check(xmqttStoreInboxAckAsync(X.Writer,Lease,10000,&F)==XMQTT_JWORK_ACCEPTED,"accept one ACK under physical pressure");
    const xmqttcommitresult* R=autoCommit(F);
    autoIdle(X.Writer);
    xmqttstoreautocompactinfo I=autoInfo(X.Writer);
    check(I.Attempts==1,"exactly one pressure rotation attempt");
#if defined(TEST_ONLINE_FAULTS)
    if(Mode==AUTO_PRESSURE_RETRY){
        check(R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&R->Status==XMQTT_COMMIT_LIMIT&&
            I.StorageStatus==XMQTT_JOURNAL_IO&&I.RetryAfter>0&&I.NextEligibleBytes>I.PreviousBytes,
            "inactive write error gates retries without changing selected space");
        xrtFutureDestroy(F);
        xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
        while(xrtTimer()<I.RetryAfter)xrtSleep(1);
        check(current(X.Writer).Worker.Journal.CommittedEnd==I.PreviousBytes&&
            xmqttStoreInboxTake(X.Writer,B,sizeof(B),&Lease,&View,&Bytes)==XMQTT_STORE_DONE,
            "no committed growth and old Inbox ready for another attempt");
        check(xmqttStoreInboxAckAsync(X.Writer,Lease,10000,&F)==XMQTT_JWORK_ACCEPTED,"retry through the elapsed cooldown");
        R=autoCommit(F);
        autoIdle(X.Writer);
        I=autoInfo(X.Writer);
        check(I.Attempts==2&&!I.RetryAfter,"one later foreground attempt clears the cooldown");
    }
#endif
    if(Mode==AUTO_PRESSURE_UNCERTAIN){
        check(R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&R->Status==XMQTT_COMMIT_STORAGE&&
            I.Effect==XMQTT_JOURNAL_MAY_BE_COMMITTED&&!I.Rotations&&current(X.Writer).Worker.Failed,
            "selector uncertainty does not become an uncertain foreground ACK");
        check(reopen(&X,2).Inbox==1,"cold recovery preserves the unacknowledged delivery");
    }else{
        check(R->Effect==XMQTT_JOURNAL_COMMITTED&&R->Status==XMQTT_COMMIT_OK&&R->Sequence==3&&
            R->AttemptSequence==3&&I.Effect==XMQTT_JOURNAL_COMMITTED&&I.Rotations==1&&I.Generation==2,
            "same ACK retries once against the new checkpoint without losing its reservation");
        xmqttstorestateinfo S=reopen(&X,2);
        check(!S.Inbox&&S.Transactions==3&&S.LastKey==1,"cold recovery sees exactly one successful ACK");
    }
#if defined(TEST_ONLINE_FAULTS)
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
#endif
    xrtFutureDestroy(F);
    end(&X);
}
#if defined(TEST_ONLINE_FAULTS)
static void automaticCloseOrError(xmqttversion V,bool Close)
{
    onlinefixture X={0};
    begin(&X,V,false,true,0);
    xmqttstoreautocompactconfig C=autoConfig(1);
    C.RetryDelayMs=0;
    check(xmqttStoreWriteSetAutoCompact(X.Writer,&C),"install automatic failure fixture policy");
    xrtAtomic32Store(&onlineEntered,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineReleased,0,XMEMORY_RELEASE);
    xrtAtomic32Store(&onlineHook,Close?1u:2u,XMEMORY_RELEASE);
    xfuture* F=autoAppend(&X,NULL,0);
    check(autoCommit(F)->Effect==XMQTT_JOURNAL_COMMITTED,"foreground result publishes before idle maintenance");
    if(Close){
        mqttExampleWait(&onlineEntered);
        check(autoInfo(X.Writer).Running&&xmqttStoreWriteClose(X.Writer)&&
            xmqttStoreWriteWaitFor(X.Writer,0)==XWAIT_TIMEOUT,"Close keeps in-flight automatic I/O live");
        xrtAtomic32Store(&onlineReleased,1,XMEMORY_RELEASE);
        check(xmqttStoreWriteWaitFor(X.Writer,10000)==XWAIT_OK,"actual automatic I/O retires");
    }else autoIdle(X.Writer);
    xmqttstoreautocompactinfo I=autoInfo(X.Writer);
    check(I.Attempts==1&&!I.Rotations&&!I.Running&&I.Effect==XMQTT_JOURNAL_NOT_COMMITTED&&
        I.Status==(Close?XMQTT_COMMIT_CLOSED:XMQTT_COMMIT_STORAGE)&&
        I.StorageStatus==(Close?XMQTT_JOURNAL_EMPTY:XMQTT_JOURNAL_IO),
        "automatic close or definite write failure preserves the original cut");
    xrtAtomic32Store(&onlineHook,0,XMEMORY_RELEASE);
    if(!Close){
        check(!current(X.Writer).Worker.Failed,"inactive I/O error leaves automatic Writer usable");
        xfuture* Next=autoAppend(&X,NULL,0);
        check(autoCommit(Next)->Effect==XMQTT_JOURNAL_COMMITTED,"next foreground record commits");
        xrtFutureDestroy(Next);
        autoIdle(X.Writer);
        check(autoInfo(X.Writer).Attempts==1,"growth policy suppresses repeated candidate failure");
    }
    check(reopen(&X,1).Inbox==1,"old selection and retained Inbox survive the failed attempt");
    xrtFutureDestroy(F);
    end(&X);
}
#endif
#endif
int main(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    connected(XMQTT_VERSION_5);
    connected(XMQTT_VERSION_311);
    expired();
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_311:XMQTT_VERSION_5;
        automaticIdle(V);
        automaticBackoff(V);
        automaticPressure(V,AUTO_PRESSURE_DONE);
#if defined(TEST_ONLINE_FAULTS)
        automaticPressure(V,AUTO_PRESSURE_UNCERTAIN);
        automaticPressure(V,AUTO_PRESSURE_RETRY);
        automaticCloseOrError(V,false);
        automaticCloseOrError(V,true);
#endif
    }
    puts("[PASS] automatic idle ordering, committed-growth backoff and exact pressure ACK retry");
    puts("[PASS] online native rotation on the same TCP connection, Inbox lease, FIFO and expiry");
#if defined(TEST_ONLINE_FAULTS)
    uncertain(XMQTT_VERSION_5);
    uncertain(XMQTT_VERSION_311);
    puts("[PASS] online checkpoint cancellation, inactive I/O and uncertain selector fencing");
    puts("[PASS] automatic pressure uncertainty and cooldown, idle inactive I/O and physical Close retirement");
#endif
#else
    puts("[SKIP] native store compaction requires Windows or Linux");
#endif
    return 0;
}
