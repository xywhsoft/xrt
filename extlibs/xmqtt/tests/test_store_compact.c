#include "test.h"
#include "support/legacy_store_ops.h"
#include "../examples/common/store_compact.h"
#include <xmqtt/store_write.h>
#include "support/journal_peer.h"
#include "support/store_write_peer.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
static const char* Names[]={"journal.slot","journal.0","journal.1"};
static void expect(xmqttjournalstatus Actual,xmqttjournalstatus Expected,cstr Step)
{
    if(Actual!=Expected)
        fprintf(stderr,"%s: status %d expected %d, %s\n",Step,(int)Actual,(int)Expected,
                xrtGetError()?xrtErrorMessage(xrtGetError()):"no diagnostic");
    testRequire(Actual==Expected,Step);
}

static void append(xmqttjournal* J,const xmqttstoredeltaconfig* C,const xmqttstoredelta* Ops,size_t Count)
{
    unsigned char Buffer[1536];
    size_t Bytes=0;
    uint64 Sequence=0;
    testRequire(xmqttStoreDeltaWrite(C,Ops,Count,NULL,0,Buffer,sizeof(Buffer),&Bytes)==XMQTT_STORE_DELTA_DONE,
                "encode complete delta batch");
    expect(xmqttJournalAppend(J,(xbytesview){Buffer,Bytes},&Sequence),XMQTT_JOURNAL_DONE,"commit batch");
}
static xbytesview wire(xmqttversion V,unsigned char Data[9],uint8 Qos,uint8 Identifier)
{
    Data[0]=(unsigned char)(0x30u|(Qos<<1u));
    Data[2]=0;Data[3]=1;Data[4]='t';
    size_t Bytes=5;
    if(Qos){Data[Bytes++]=0;Data[Bytes++]=Identifier;}
    if(V==XMQTT_VERSION_5)Data[Bytes++]=0;
    Data[Bytes++]=(unsigned char)('A'+Identifier);
    Data[1]=(unsigned char)(Bytes-2);
    return (xbytesview){Data,Bytes};
}
static void seed(xmqttjournal* J,const xmqttstorestateconfig* C,bool Live)
{
    unsigned char Raw[5][9];
    uint32 Expiry=C->Delta.Version==XMQTT_VERSION_5?60:UINT32_MAX;
    uint8 Reason=C->Delta.Version==XMQTT_VERSION_5?0x10:0;
    const xmqttstoredelta Ops[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=4,.WallMilliseconds=1000,.Data=XRT_BYTES_LITERAL("checkpoint")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=4,.WallMilliseconds=1001,.Expiry=Expiry},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=4,.Key=1,.Identifier=7,.Data=wire(C->Delta.Version,Raw[0],2,7)},
        {.Type=XMQTT_STORE_IN_PUBREL,.Epoch=4,.Key=1,.Identifier=7},
        {.Type=XMQTT_STORE_END,.Epoch=4,.WallMilliseconds=1002,.Expiry=Expiry},
        {.Type=XMQTT_STORE_CLEAR,.Epoch=4,.WallMilliseconds=1003},
        {.Type=XMQTT_STORE_OPEN,.Epoch=5,.WallMilliseconds=1004,.Expiry=Expiry},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=5,.Key=2,.Identifier=7,.Data=wire(C->Delta.Version,Raw[1],2,7)},
        {.Type=XMQTT_STORE_IN_PUBREL,.Epoch=5,.Key=2,.Identifier=7},
        {.Type=XMQTT_STORE_INBOX_ACK,.Epoch=5,.Key=2},
        {.Type=XMQTT_STORE_OUT_PUT,.Epoch=5,.Key=3,.Identifier=7,.Flags=1,.Data=wire(C->Delta.Version,Raw[2],2,7)},
        {.Type=XMQTT_STORE_OUT_PUBREL,.Epoch=5,.Key=3,.Identifier=7,.Reason=Reason},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=5,.Key=4,.Data=wire(C->Delta.Version,Raw[3],0,0)},
        {.Type=XMQTT_STORE_OUT_PUT,.Epoch=5,.Key=50,.Identifier=9,.Flags=3,.Data=wire(C->Delta.Version,Raw[4],1,9)},
        {.Type=XMQTT_STORE_OUT_COMPLETE,.Epoch=5,.Key=50,.Identifier=9,.Flags=XMQTT_PACKET_PUBACK},
        {.Type=XMQTT_STORE_END,.Epoch=5,.WallMilliseconds=2000,.Expiry=Expiry},
    };
    size_t Count=sizeof(Ops)/sizeof(Ops[0])-(Live?1u:0u);
    for(size_t I=0;I<Count;++I)append(J,&C->Delta,Ops+I,1);
}
static xmqttstorestate* recover(mqttExampleJournalSlot* Slot,const xmqttstorestateconfig* C,xmqttjournal** J)
{
    expect(mqttExampleJournalSlotOpenCurrent(Slot,J),XMQTT_JOURNAL_DONE,"open exact current file");
    xmqttstorestate* State=NULL;
    testRequire(xmqttStoreStateRecover(*J,C,&State)==XMQTT_STORE_DONE,"recover exact typed history");
    return State;
}
static xmqttstorestateinfo info(const xmqttstorestate* S)
{
    xmqttstorestateinfo Info;
    testRequire(xmqttStoreStateInfo(S,&Info),"read recovered info");
    return Info;
}
static void same(const xmqttstorestate* A,const xmqttstorestate* B)
{
    xmqttstorestateinfo IA=info(A),IB=info(B);
    testRequire(IA.Version==IB.Version&&IA.Epoch==IB.Epoch&&IA.LastKey==IB.LastKey&&
        IA.LastWallMilliseconds==IB.LastWallMilliseconds&&IA.RemainingMilliseconds==IB.RemainingMilliseconds&&
        IA.HasSession==IB.HasSession&&IA.LiveAtCut==IB.LiveAtCut&&IA.Outgoing==IB.Outgoing&&
        IA.Incoming==IB.Incoming&&IA.Inbox==IB.Inbox&&IA.ClientId.Size==IB.ClientId.Size&&
        !memcmp(IA.ClientId.Data,IB.ClientId.Data,IA.ClientId.Size),"same logical metadata after storage rotation");
    size_t Counts[]={IA.Outgoing,IA.Incoming,IA.Inbox};
    for(size_t K=0;K<3;++K)for(size_t I=0;I<Counts[K];++I){
        xmqttstoreview L,R;
        testRequire(xmqttStoreStateView(A,(xmqttstoreviewkind)K,I,&L)==XMQTT_STORE_DONE&&
            xmqttStoreStateView(B,(xmqttstoreviewkind)K,I,&R)==XMQTT_STORE_DONE,"both ordered record views exist");
        testRequire(L.Epoch==R.Epoch&&L.Key==R.Key&&L.Identifier==R.Identifier&&L.Qos==R.Qos&&
            L.PubrecReason==R.PubrecReason&&L.Attempted==R.Attempted&&L.Sent==R.Sent&&
            L.Pubrel==R.Pubrel&&L.InboxPending==R.InboxPending&&L.Wire.Size==R.Wire.Size&&
            !memcmp(L.Wire.Data,R.Wire.Data,L.Wire.Size),"same protocol stage, delivery and exact canonical wire");
    }
}
static void writer(mqttExampleJournalSlot* Slot,xmqttjournal* J,xmqttstorestate* State,const xmqttstorestateconfig* C)
{
    xtaskpoolconfig PoolConfig={.Threads=1,.QueueLimit=8};
    xtaskpool* Pool=xrtTaskPoolCreate(&PoolConfig);
    xmqttstorewriteconfig Config;
    xmqttStoreWriteConfigInit(&Config);
    xmqttstorewrite* W=NULL;
    testRequire(Pool&&xmqttStoreWriteCreate(Pool,J,State,&Config,&W)&&mqttExampleJournalSlotDetach(Slot,J),
        "writer adopts exact new generation");
    unsigned char Buffer[128];
    size_t Bytes;

    xmqttstoreview View;
    testRequire(xmqttStoreWriteRead(W,1,Buffer,sizeof(Buffer),&View,&Bytes)==XMQTT_STORE_DONE&&
        View.InboxPending&&View.Key==1&&View.Epoch==4&&View.Pubrel,"old epoch Inbox delivery retains its key and retired PUBREL stage");
    xfuture* F=NULL;
    testRequire(swLegacyAck(W,&C->Delta,View.Epoch,View.Key,5000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,5000)==XWAIT_OK,"post-compaction durable Inbox ACK");
    const xmqttcommitresult* Result=xrtFutureValue(F);
    testRequire(Result&&Result->Effect==XMQTT_JOURNAL_COMMITTED,"Inbox acknowledgement committed");
    xrtFutureDestroy(F);

    unsigned char Raw[9];
    uint32 Expiry=C->Delta.Version==XMQTT_VERSION_5?60:UINT32_MAX;
    xmqttstoredelta Opens[]={
        {.Type=XMQTT_STORE_OPEN,.Flags=XMQTT_STORE_DELTA_SESSION_PRESENT,.Expiry=Expiry,.Epoch=5,.WallMilliseconds=3001},
        {.Type=XMQTT_STORE_IN_COMPLETE,.Epoch=5,.Key=2,.Identifier=7},
        {.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=5,.Key=50,.Data=wire(C->Delta.Version,Raw,0,0)}
    };
    unsigned char Work[512];
    testRequire(xmqttStoreDeltaWrite(&C->Delta,Opens,3,NULL,0,Work,sizeof(Work),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttStoreWriteAppendAsync(W,(xbytesview){Work,Bytes},5000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,5000)==XWAIT_OK,"submit stale key after compacted retired suffix");
    Result=xrtFutureValue(F);
    testRequire(Result&&Result->Status==XMQTT_COMMIT_INVALID&&!Result->AppendCalled,"retired key still fenced");
    xrtFutureDestroy(F);
    Opens[2].Key=51;
    testRequire(xmqttStoreDeltaWrite(&C->Delta,Opens,3,NULL,0,Work,sizeof(Work),&Bytes)==XMQTT_STORE_DELTA_DONE&&
        xmqttStoreWriteAppendAsync(W,(xbytesview){Work,Bytes},5000,&F)==XMQTT_JWORK_ACCEPTED&&
        xrtFutureWaitFor(F,5000)==XWAIT_OK,"persist new data after the checkpoint");
    Result=xrtFutureValue(F);
    testRequire(Result&&Result->Effect==XMQTT_JOURNAL_COMMITTED,"new valid delta committed");
    xrtFutureDestroy(F);
    testRequire(xmqttStoreWriteClose(W)&&xmqttStoreWriteWaitFor(W,5000)==XWAIT_OK&&xmqttStoreWriteDestroy(W)&&
        xrtTaskPoolClose(Pool)&&xrtTaskPoolWaitFor(Pool,5000)==XWAIT_OK&&xrtTaskPoolDestroy(Pool),"retire writer then pool");
}
#if defined(TEST_COMPACT_FAULTS)
static xmqttjournalstatus injected(mqttExampleJournalSlot* Slot,const xmqttstorestateconfig* C,unsigned Point)
{
    compactHit=compactSelectorWrites=0;
    compactFault=Point;
    xmqttjournalstatus Status=mqttExampleCompactStore(Slot,C,84);
    compactFault=0;
    testRequire(compactHit==1,"reached requested allocation or I/O fault exactly once");
    return Status;
}
#endif

static void local(xmqttversion V,bool Live)
{
    str Dir=xrtDirTemp(NULL,"xmqtt-compact-",NULL);
    xroot Root=Dir?xrtRootOpen(Dir):NULL;
    testRequire(Root!=NULL,"native anchored fixture root");
    xfileoptions Open;
    xrtFileOptionsInit(&Open);
    Open.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE;
    for(size_t I=0;I<3;++I){
        xfile F=xrtRootFileOpen(Root,Names[I],&Open);
        testRequire(F&&xrtClose(F),"create empty native fixture file");
    }
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Version=V;JC.Binding=XRT_BYTES_LITERAL("test/checkpoint");JC.Generation=11;
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;JC.MaxTransactionBytes=2048;
    mqttExampleJournalSlotFiles Files={0};
    mqttExampleJournalSlot* Slot=NULL;
    expect(mqttExampleOpenNativeSlot(Root,&JC,true,&Files,&Slot),XMQTT_JOURNAL_DONE,"create typed slot");
    xmqttstorestateconfig C;
    xmqttStoreStateConfigInit(&C);
    C.Binding=JC.Binding;C.Generation=JC.Generation;C.Delta.Version=V;
    C.ClientId=XRT_STR_LITERAL("checkpoint");C.WallMilliseconds=3000;
    C.MaxOutgoing=8;C.MaxIncoming=8;C.MaxInbox=8;
    expect(mqttExampleCompactStore(Slot,&C,84),XMQTT_JOURNAL_EMPTY,"empty slot has no history to rewrite");
    xmqttjournal* J=NULL;
    expect(mqttExampleJournalSlotOpenCurrent(Slot,&J),XMQTT_JOURNAL_DONE,"open initial journal");
    xmqttjournalrecord Record;
    expect(xmqttJournalNext(J,&Record),XMQTT_JOURNAL_EMPTY,"validate the empty starting point");
    seed(J,&C,Live);
    expect(mqttExampleCompactStore(Slot,&C,84),XMQTT_JOURNAL_STATE,"retire physical Journal before compaction");
    testRequire(mqttExampleJournalSlotClose(Slot,J),"release source writer");

    xmqttstorestate* Old=recover(Slot,&C,&J);
    testRequire(mqttExampleJournalSlotClose(Slot,J),"keep immutable old State independent of old file");
    testRequire(info(Old).Outgoing==1&&info(Old).Incoming==1&&info(Old).Inbox==2&&info(Old).LastKey==50,
                "oracle covers all three view kinds and a retired highest key");
    expect(mqttExampleCompactStore(Slot,&C,70),XMQTT_JOURNAL_LIMIT,"decline short scratch quota");
    mqttExampleJournalSlotInfo SlotInfo={0};
    testRequire(mqttExampleJournalSlotGetInfo(Slot,&SlotInfo)&&SlotInfo.Generation==C.Generation&&!SlotInfo.JournalOpen,
                "scratch refusal retains selector and releases handles");

#if defined(TEST_COMPACT_FAULTS)
    if(!Live){
        expect(injected(Slot,&C,1),XMQTT_JOURNAL_ERROR,"scratch allocation failure");
        expect(injected(Slot,&C,2),XMQTT_JOURNAL_IO,"inactive data failure is a definite selector refusal");
        expect(injected(Slot,&C,3),XMQTT_JOURNAL_UNCERTAIN,"first selector write could have had effects");
        testRequire(mqttExampleJournalSlotGetInfo(Slot,&SlotInfo)&&SlotInfo.Poisoned&&!SlotInfo.JournalOpen,
                    "close failed preparation and fence its selector");
        testRequire(mqttExampleCloseNativeSlot(Slot,&Files),"retire uncertain owner");
        expect(mqttExampleOpenNativeSlot(Root,&JC,false,&Files,&Slot),XMQTT_JOURNAL_DONE,"recover old generation before selector output");
        xmqttstorestate* Current=recover(Slot,&C,&J);
        same(Old,Current);
        xmqttStoreStateDestroy(Current);
        testRequire(mqttExampleJournalSlotClose(Slot,J),"release old recovery owner");
    }
    if(Live){
        expect(injected(Slot,&C,4),XMQTT_JOURNAL_UNCERTAIN,"second selector write fails after first is synced");
        testRequire(mqttExampleJournalSlotGetInfo(Slot,&SlotInfo)&&SlotInfo.Poisoned&&!SlotInfo.JournalOpen,
                    "retire ambiguous publish before fresh selection");
        testRequire(mqttExampleCloseNativeSlot(Slot,&Files),"retire ambiguous selector owner");
        expect(mqttExampleOpenNativeSlot(Root,&JC,false,&Files,&Slot),XMQTT_JOURNAL_DONE,"recover the first synchronized choice");
        testRequire(mqttExampleJournalSlotGetInfo(Slot,&SlotInfo)&&SlotInfo.Generation==C.Generation+1&&
            SlotInfo.PublishedSequence==3,"first valid choice selects the complete typed checkpoint");
    }else
#endif
    {
        xrtSetErrorKind(XERR_AGAIN);
        const xerror* Ambient=xrtGetError();
        /* The checkpoint preserves the physical cut even when expiry has
         * already elapsed at the compactor's observation time. */
        if(!Live&&V==XMQTT_VERSION_5)C.WallMilliseconds=62000;
        expect(mqttExampleCompactStore(Slot,&C,84),XMQTT_JOURNAL_DONE,"publish a bounded multi-block checkpoint");
        C.WallMilliseconds=3000;
        testRequire(xrtGetError()==Ambient,"successful rotation preserves ambient diagnostics");
        xrtClearError();
        testRequire(mqttExampleJournalSlotGetInfo(Slot,&SlotInfo)&&SlotInfo.Generation==C.Generation+1&&
            SlotInfo.PublishedSequence==3&&SlotInfo.Effect==XMQTT_JOURNAL_COMMITTED,"one descriptor and two row blocks");
    }
    ++C.Generation;
    testRequire(mqttExampleCloseNativeSlot(Slot,&Files),"drop original selector owner");
    expect(mqttExampleOpenNativeSlot(Root,&JC,false,&Files,&Slot),XMQTT_JOURNAL_DONE,"fresh native selector owner");
    xmqttstorestate* New=recover(Slot,&C,&J);
    same(Old,New);
    testRequire(info(New).Transactions==3&&info(New).ReplayBytes<info(Old).ReplayBytes/2,"physical replay reduction");

    if(!Live&&V==XMQTT_VERSION_5){
        testRequire(mqttExampleJournalSlotClose(Slot,J),"retire before expiry read");
        xmqttStoreStateDestroy(New);
        C.WallMilliseconds=61999;
        New=recover(Slot,&C,&J);
        testRequire(info(New).HasSession&&info(New).RemainingMilliseconds==1&&info(New).Outgoing==1,
                    "compactor's later wall did not consume earlier observation's final millisecond");
        testRequire(mqttExampleJournalSlotClose(Slot,J),"release before exact boundary read");
        xmqttStoreStateDestroy(New);
        C.WallMilliseconds=62000;
        New=recover(Slot,&C,&J);
        testRequire(!info(New).HasSession&&info(New).Inbox==2&&!info(New).Outgoing&&!info(New).Incoming,
                    "original absolute expiry consumed despite generation change");
        testRequire(mqttExampleJournalSlotClose(Slot,J),"retire expired reader");
        xmqttStoreStateDestroy(New);
        C.WallMilliseconds=3000;
        New=recover(Slot,&C,&J);
        same(Old,New);
    }
    writer(Slot,J,New,&C);
    C.WallMilliseconds=3100;
    expect(mqttExampleCompactStore(Slot,&C,84),XMQTT_JOURNAL_DONE,"compact a checkpoint followed by ordinary deltas");
    ++C.Generation;
    New=recover(Slot,&C,&J);
    xmqttstorestateinfo Current=info(New);
    testRequire(Current.LastKey==51&&Current.Epoch==5&&Current.LiveAtCut&&Current.Outgoing==1&&
        !Current.Incoming&&Current.Inbox==2&&Current.Transactions==3,"new committed state survives subsequent rotation");
    testRequire(info(Old).LastKey==50&&info(Old).Inbox==2,"earlier State remains immutable across physical reuse");
    testRequire(mqttExampleJournalSlotClose(Slot,J)&&mqttExampleCloseNativeSlot(Slot,&Files),"release all native owners");
    xmqttStoreStateDestroy(New);
    xmqttStoreStateDestroy(Old);
    for(size_t I=0;I<3;++I)testRequire(xrtRootRemove(Root,Names[I]),"unlink exact fixture file");
    testRequire(xrtRootClose(Root)&&xrtDirRemove(Dir),"retire fixture directory");
    xrtFree(Dir);
}

static void portableCheckpoint(xmqttversion V)
{
    static jpdrive SourceDrive,TargetDrive;
    memset(&SourceDrive,0,sizeof(SourceDrive));
    memset(&TargetDrive,0,sizeof(TargetDrive));
    jpfile Source=jpFile(&SourceDrive),Target=jpFile(&TargetDrive);
    xmqttjournalconfig JC=jpConfig();
    JC.Version=V;
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttstorestateconfig C;
    xmqttStoreStateConfigInit(&C);
    C.Binding=JC.Binding;C.Generation=JC.Generation;C.Delta.Version=V;
    C.ClientId=XRT_STR_LITERAL("checkpoint");C.WallMilliseconds=3000;

    xmqttjournal* J=jpCreate(Source,&JC);
    xmqttstorecheckpoint* Snapshot=(xmqttstorecheckpoint*)(uintptr_t)1;
    testRequire(xmqttStoreCheckpointRecover(J,&C,&Snapshot)==XMQTT_STORE_EMPTY&&
        Snapshot==(xmqttstorecheckpoint*)(uintptr_t)1,"empty cut leaves checkpoint output untouched");
    seed(J,&C,false);
    testRequire(xmqttJournalDestroy(J),"reopen source at its original cursor");
    expect(xmqttJournalOpen(Source.File,&JC,&J),XMQTT_JOURNAL_DONE,"open source for snapshot");
    testRequire(xmqttStoreCheckpointRecover(J,&C,&Snapshot)==XMQTT_STORE_DONE&&xmqttJournalDestroy(J),
                "checkpoint owns recovered protocol state");
    jpFileClose(Source);

    xmqttjournalconfig Wrong=JC;
    Wrong.Binding=XRT_BYTES_LITERAL("different store");
    J=jpCreate(Target,&Wrong);
    expect(xmqttStoreCheckpointWrite(Snapshot,J,84),XMQTT_JOURNAL_STATE,"refuse different binding");
    testRequire(xmqttJournalDestroy(J)&&xrtFileResize(Target.File,0),"clear unrelated destination");
    xrtClearError();
    JC.Generation+=5;
    J=jpCreate(Target,&JC);
    expect(xmqttStoreCheckpointWrite(Snapshot,J,70),XMQTT_JOURNAL_LIMIT,"bounded scratch before any append");
    /* Failing the first commit witness makes the first append ambiguous. */
    TargetDrive.Fail=TargetDrive.Calls+5;
    expect(xmqttStoreCheckpointWrite(Snapshot,J,84),XMQTT_JOURNAL_UNCERTAIN,"report uncertainty to the APP");
    TargetDrive.Fail=0;
    testRequire(xmqttJournalDestroy(J)&&xrtFileResize(Target.File,0),"APP discards failed destination");
    xrtClearError();

    J=jpCreate(Target,&JC);
    expect(xmqttStoreCheckpointWrite(Snapshot,J,84),XMQTT_JOURNAL_DONE,"reuse snapshot in a different generation");
    expect(xmqttStoreCheckpointWrite(Snapshot,J,84),XMQTT_JOURNAL_STATE,"checkpoint must start at sequence one");
    testRequire(xmqttJournalDestroy(J),"cold-open portable checkpoint");
    xrtClearError();
    xmqttStoreCheckpointDestroy(Snapshot);

    expect(xmqttJournalOpen(Target.File,&JC,&J),XMQTT_JOURNAL_DONE,"open written snapshot");
    xmqttstorestate* S=NULL;
    C.Generation=JC.Generation;
    testRequire(xmqttStoreStateRecover(J,&C,&S)==XMQTT_STORE_DONE&&info(S).LastKey==50&&
        info(S).Transactions==3&&info(S).Outgoing==1&&info(S).Incoming==1&&info(S).Inbox==2,
        "raw model survives independent source and destination lifetimes");
    xmqttStoreStateDestroy(S);
    testRequire(xmqttJournalDestroy(J),"retire portable target");
    jpFileClose(Target);
}

static void protocolCheckpoint(xmqttversion V)
{
    jpdrive SourceDrive={0},TargetDrive={0};
    jpfile Source=jpFile(&SourceDrive),Target=jpFile(&TargetDrive);
    xmqttjournalconfig JC=stJournalConfig(V);
    xmqttstorestateconfig C=stConfig(V);
    C.MaxInbox=0;
    C.MaxIncoming=2;
    xmqttjournal* J=jpCreate(Source,&JC);
    xmqttstorestate* State=NULL;
    stStatus(xmqttStoreStateRecover(J,&C,&State),XMQTT_STORE_DONE,"empty protocol store");
    xmqttstorewriteconfig WConfig;
    testRequire(xmqttStoreWriteConfigInit(&WConfig),"writer config");
    xtaskpool* Pool=jwPool(1);
    xmqttstorewrite* W=NULL;
    testRequire(xmqttStoreWriteCreate(Pool,J,State,&WConfig,&W),"protocol writer");
    xmqttstoredelta Ops[]={
        {.Type=XMQTT_STORE_BIND,.Epoch=1,.WallMilliseconds=C.WallMilliseconds,
         .Data=XRT_BYTES_LITERAL("delta-client")},
        {.Type=XMQTT_STORE_OPEN,.Epoch=1,.WallMilliseconds=C.WallMilliseconds,
         .Expiry=V==XMQTT_VERSION_5?60:UINT32_MAX},
        {.Type=XMQTT_STORE_IN_TRACK,.Epoch=1,.Key=1,.Identifier=7},
        {.Type=XMQTT_STORE_IN_TRACK,.Epoch=1,.Key=2,.Identifier=8},
        {.Type=XMQTT_STORE_IN_PUBREL,.Epoch=1,.Key=2,.Identifier=8}
    };
    xfuture* Future=swOps(W,&C.Delta,Ops,sizeof(Ops)/sizeof(Ops[0]));
    testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"commit two payload-free receive identities");
    xrtFutureDestroy(Future);
    xmqttstoreview View={0};
    size_t Required=1;
    stStatus(xmqttStoreWriteRead(W,1,NULL,0,&View,&Required),XMQTT_STORE_DONE,"read metadata without buffer");
    testRequire(View.Qos==2&&View.Identifier==7&&!Required&&!View.Wire.Data&&!View.InboxPending&&
        !View.Pubrel&&!swInfo(W).State.Inbox,"receive tracking retains no business payload");
    xmqttstoredelta Duplicate={.Type=XMQTT_STORE_IN_TRACK,.Epoch=1,.Key=3,.Identifier=8};
    Future=swOps(W,&C.Delta,&Duplicate,1);
    const xmqttcommitresult* Result=jwOutcome(Future);
    testRequire(Result->Status==XMQTT_COMMIT_LIMIT&&!Result->AppendCalled&&swInfo(W).State.LastKey==2,
        "receive quota applies without inbox entries");
    xrtFutureDestroy(Future);
    Duplicate.Type=XMQTT_STORE_INBOX_ACK;
    Duplicate.Identifier=0;
    Duplicate.Key=2;
    Future=swOps(W,&C.Delta,&Duplicate,1);
    Result=jwOutcome(Future);
    testRequire(Result->Status==XMQTT_COMMIT_INVALID&&!Result->AppendCalled,
        "protocol identity cannot be retired by application acknowledgement");
    xrtFutureDestroy(Future);
    testRequire(xmqttStoreWriteDestroy(W),"retire protocol writer");
    jwPoolDone(Pool);

    J=jpReopen(Source,&JC);
    xmqttstorecheckpoint* Snapshot=NULL;
    stStatus(xmqttStoreCheckpointRecover(J,&C,&Snapshot),XMQTT_STORE_DONE,"replay metadata-only journal");
    testRequire(xmqttJournalDestroy(J),"retire source journal");
    jpFileClose(Source);
    JC.Generation=++C.Generation;
    J=jpCreate(Target,&JC);
    expect(xmqttStoreCheckpointWrite(Snapshot,J,84),XMQTT_JOURNAL_DONE,"checkpoint metadata-only rows");
    xmqttStoreCheckpointDestroy(Snapshot);
    testRequire(xmqttJournalDestroy(J),"retire checkpoint writer");
    J=jpReopen(Target,&JC);
    stStatus(xmqttStoreStateRecover(J,&C,&State),XMQTT_STORE_DONE,"cold restore checkpoint");
    testRequire(info(State).Incoming==2&&!info(State).Inbox&&info(State).LastKey==2,
        "restore tracks only protocol state");
    for(size_t I=0;I<2;++I){
        stStatus(xmqttStoreStateView(State,XMQTT_STORE_INCOMING,I,&View),XMQTT_STORE_DONE,"restore receive identity");
        testRequire(View.Key==I+1&&View.Identifier==7+I&&View.Qos==2&&View.Pubrel==(I==1)&&
            !View.Wire.Data&&!View.Wire.Size&&!View.InboxPending,"restore exact QoS2 stage without payload");
    }
    Pool=jwPool(1);
    testRequire(xmqttStoreWriteCreate(Pool,J,State,&WConfig,&W),"resume tracker from checkpoint");
    xmqttstoredelta Resume[]={
        {.Type=XMQTT_STORE_OPEN,.Flags=XMQTT_STORE_DELTA_SESSION_PRESENT,.Epoch=1,
         .WallMilliseconds=C.WallMilliseconds,.Expiry=Ops[1].Expiry},
        {.Type=XMQTT_STORE_IN_COMPLETE,.Epoch=1,.Key=2,.Identifier=8},
        {.Type=XMQTT_STORE_IN_TRACK,.Epoch=1,.Key=3,.Identifier=8}
    };
    Future=swOps(W,&C.Delta,Resume,sizeof(Resume)/sizeof(Resume[0]));
    testRequire(jwOutcome(Future)->Status==XMQTT_COMMIT_OK,"complete and reuse restored identifier atomically");
    xrtFutureDestroy(Future);
    stStatus(xmqttStoreWriteRead(W,2,NULL,0,&View,&Required),XMQTT_STORE_EMPTY,"completed identity retired");
    stStatus(xmqttStoreWriteRead(W,3,NULL,0,&View,&Required),XMQTT_STORE_DONE,"new identity committed");
    testRequire(View.Identifier==8&&!View.Pubrel&&!View.Wire.Size&&swInfo(W).State.Incoming==2,
        "reused identifier starts at PUBLISH");
    testRequire(xmqttStoreWriteDestroy(W),"retire restored writer");
    jwPoolDone(Pool);
    jpFileClose(Target);
}

enum {COMPACT_CORRUPT_CASES=12};
static void corrupt(xmqttversion V,unsigned Case)
{
    str Path=NULL;
    xfile F=xrtFileTemp(NULL,"xmqtt-checkpoint-bytes-",NULL,&Path);
    testRequire(F&&Path,"native fixture file");
    xmqttjournalconfig JC;
    xmqttJournalConfigInit(&JC);
    JC.Generation=99;JC.Version=V;JC.Binding=XRT_BYTES_LITERAL("test/checkpoint/bytes");
    JC.PayloadSchema=XMQTT_STORE_DELTA_SCHEMA;
    xmqttjournal* J=NULL;
    expect(xmqttJournalCreate(F,&JC,&J),XMQTT_JOURNAL_DONE,"native raw checkpoint source");
    unsigned char Header[65]={'X','M','Q','C','K','P','T',0,1,(unsigned char)V,7};
    Header[15]=1;Header[23]=2;Header[31]=2;Header[39]=10;Header[55]=2;
    memset(Header+56,255,4);
    Header[64]='c';
    if(Case==1)Header[8]=2;
    if(Case==2)Header[31]=1;
    uint64 Seq;
    if(Case==4){
        xmqttstoredeltaconfig D;
        xmqttStoreDeltaConfigInit(&D);
        D.Version=V;
        append(J,&D,NULL,0);
    }
    expect(xmqttJournalAppend(J,(xbytesview){Header,sizeof(Header)},&Seq),XMQTT_JOURNAL_DONE,
           "commit independent checkpoint descriptor");

    if(Case!=3){
        unsigned char Block[88]={'X','M','Q','C','R','O','W',0};
        Block[11]=2;
        size_t At=16;
        for(uint8 I=1;I<=2;++I){
            unsigned char Raw[9];
            xbytesview Packet=wire(V,Raw,1,I);
            unsigned char* H=Block+At;
            H[7]=I;H[15]=2;H[19]=(unsigned char)Packet.Size;H[21]=I;H[22]=1;
            memcpy(H+24,Packet.Data,Packet.Size);
            if(I==2){
                switch(Case){
                case 5:H[7]=1;break;
                case 6:H[21]=1;H[24+6]=1;break;
                case 7:H[21]=1;break;
                case 8:H[22]=0;break;
                case 9:H[22]=15;break;
                case 10:Block[15]=1;break;
                default:break;
                }
            }
            At+=24+Packet.Size;
        }
        expect(xmqttJournalAppend(J,(xbytesview){Block,At},&Seq),XMQTT_JOURNAL_DONE,
               "commit independently encoded checkpoint packet rows");
    }
    testRequire(xmqttJournalDestroy(J),"retire fixture writer");
    expect(xmqttJournalOpen(F,&JC,&J),XMQTT_JOURNAL_DONE,"independent journal is structurally valid");
    xmqttstorestateconfig C;
    xmqttStoreStateConfigInit(&C);
    C.Binding=JC.Binding;C.Generation=JC.Generation;C.Delta.Version=V;
    C.ClientId=Case==11?XRT_STR_LITERAL("different"):XRT_STR_LITERAL("c");
    C.WallMilliseconds=1000;
    if(Case==12)C.MaxOperations=2;
    xmqttstorestate* S=NULL;
    xmqttstorestatus Expected=!Case?XMQTT_STORE_DONE:Case==11?XMQTT_STORE_STATE_ERROR:
        Case==12?XMQTT_STORE_LIMIT:XMQTT_STORE_CORRUPT;
    xmqttstorestatus Status=xmqttStoreStateRecover(J,&C,&S);
    if(Status!=Expected)fprintf(stderr,"checkpoint oracle case %u status %d wanted %d\n",Case,(int)Status,(int)Expected);
    testRequire(Status==Expected,"classify independently constructed or mutated checkpoint");
    if(Case)testRequire(S==NULL,"invalid checkpoint leaves output unchanged");
    else testRequire(info(S).Outgoing==2&&info(S).LastKey==2,"valid independent checkpoint");
    xmqttStoreStateDestroy(S);
    xrtClearError();
    testRequire(xmqttJournalDestroy(J)&&xrtClose(F)&&xrtFileDelete(Path),"retire exact raw checkpoint fixture");
    xrtFree(Path);
}
#endif

int main(void)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__linux__)
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        local(V,false);
        local(V,true);
        portableCheckpoint(V);
        protocolCheckpoint(V);
        for(unsigned Case=0;Case<=COMPACT_CORRUPT_CASES;++Case)corrupt(V,Case);
    }
    puts("[PASS] bounded typed rotation, state equivalence, expiry and subsequent native Writer");
    puts("[PASS] portable checkpoint ownership, descriptor refusals and uncertain writes");
    puts("[PASS] metadata-only receive identities in MQTT 5 and 3.1.1 journals and checkpoints");
    puts("[PASS] 26 independent checkpoint cases, exact format and failure classifications");
#if defined(TEST_COMPACT_FAULTS)
    puts("[PASS] eight injected checkpoint allocation, data and selector write failures");
#endif
#else
    puts("[SKIP] native store compaction requires Windows or Linux");
#endif
    return 0;
}
