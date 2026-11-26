#include "support/store_write_peer.h"
#include "support/legacy_store_ops.h"
#include <xrt/thread.h>

typedef struct pending_fixture {
    jwdrive Drive;
    jpfile File;
    str NativePath;
    xmqttjournalconfig JournalConfig;
    xmqttstorestateconfig StateConfig;
    xmqttstorewriteconfig WriterConfig;
    xtaskpool* Pool;
    xmqttstorewrite* Writer;
} pending_fixture;

static void pendingStart(pending_fixture* X,xmqttversion V,unsigned Count,unsigned Hold,unsigned Fail,bool Native)
{
    memset(X,0,sizeof(*X));
    X->JournalConfig=stJournalConfig(V);
    X->StateConfig=stConfig(V);
    X->StateConfig.MaxInbox=64;
    X->StateConfig.WallMilliseconds=0;
    testRequire(xmqttStoreWriteConfigInit(&X->WriterConfig),"store writer config");
    X->File=jwFile(&X->Drive);
    if(Native){
        X->Drive.Base.Native=xrtFileTemp(NULL,"xmqtt-pending-",NULL,&X->NativePath);
        testRequire(X->Drive.Base.Native!=NULL,"native temporary journal");
    }
    xmqttjournal* J=jpCreate(X->File,&X->JournalConfig);
    xmqttstorestate* S=NULL;
    stStatus(xmqttStoreStateRecover(J,&X->StateConfig,&S),XMQTT_STORE_DONE,"empty committed prefix");
    X->Pool=jwPool(1);
    testRequire(xmqttStoreWriteCreate(X->Pool,J,S,&X->WriterConfig,&X->Writer),"semantic writer");
    X->Drive.Base.Calls=0;
    X->Drive.Hold=Hold;
    X->Drive.Base.Fail=Fail;
    X->Drive.Base.FlushOnFailure=true;
    sdsource Source;
    sdSource(V,&Source);
    xfuture* F=swOps(X->Writer,&X->StateConfig.Delta,Source.Ops,2);
    testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"initial BIND+OPEN");
    xrtFutureDestroy(F);
    for(unsigned I=0;I<Count;++I){
        unsigned char Wire[]={0x30,5,0,1,'t',0,'x'};
        size_t Bytes=sizeof(Wire);
        if(V==XMQTT_VERSION_311){Wire[1]=4;Wire[5]='x';--Bytes;}
        xmqttstoredelta Op={.Type=XMQTT_STORE_IN_ACCEPT,.Epoch=1,.Key=I+1,.Data={Wire,Bytes}};
        F=swOps(X->Writer,&X->StateConfig.Delta,&Op,1);
        testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"real publication enters pending projection");
        xrtFutureDestroy(F);
    }
}
static void pendingFinish(pending_fixture* X,size_t Remaining)
{
    testRequire(xmqttStoreWriteDestroy(X->Writer),"physical writer retirement");
    X->Writer=NULL;
    jwPoolDone(X->Pool);
    X->Drive.Base.Fail=0;
    X->Drive.Hold=0;
    xmqttjournal* J=jpReopen(X->File,&X->JournalConfig);
    xmqttstorestate* S=NULL;
    X->StateConfig.WallMilliseconds=200000;
    stStatus(xmqttStoreStateRecover(J,&X->StateConfig,&S),XMQTT_STORE_DONE,"fresh journal recovery");
    testRequire(stInfo(S).Inbox==Remaining,"fresh recovery agrees with confirmed effects");
    xmqttStoreStateDestroy(S);
    testRequire(xmqttJournalDestroy(J),"close recovered journal");
    jpFileClose(X->File);
    if(X->Drive.Base.Native){
        testRequire(xrtClose(X->Drive.Base.Native)&&xrtFileDelete(X->NativePath),"release own native test journal");
        xrtFree(X->NativePath);
    }
}
static xmqttstoreview pendingRead(xmqttstorewrite* W,uint64 Key)
{
    unsigned char Buffer[64];
    xmqttstoreview View;
    size_t Required=0;
    stStatus(xmqttStoreWriteRead(W,Key,Buffer,sizeof(Buffer),&View,&Required),XMQTT_STORE_DONE,"read committed pending publication");
    testRequire(View.InboxPending&&View.Key==Key&&View.Wire.Data==Buffer&&View.Wire.Size==Required&&Buffer[Required-1]=='x',
        "copy canonical packet by key");
    View.Wire=(xbytesview){0};
    return View;
}
static void pendingAbsent(xmqttstorewrite* W,uint64 Key)
{
    unsigned char Buffer[64];
    xmqttstoreview View={.Key=99};
    size_t Required=99;
    stStatus(xmqttStoreWriteRead(W,Key,Buffer,sizeof(Buffer),&View,&Required),XMQTT_STORE_EMPTY,"absent committed record");
    testRequire(View.Key==99&&Required==99,"empty publishes no outputs");
}
static xfuture* pendingRetire(xmqttstorewrite* W,const xmqttstoredeltaconfig* Delta,
    uint64 Epoch,uint64 Key,int64 Timeout)
{
    xfuture* F=NULL;
    double Deadline=xrtTimer()+10;
    for(;;){
        xmqttjournaladmission Admission=swLegacyAck(W,Delta,Epoch,Key,Timeout,&F);
        if(Admission==XMQTT_JWORK_ACCEPTED)return F;
        testRequire(Admission==XMQTT_JWORK_OVERLOADED&&!F&&xrtTimer()<Deadline,"bounded application admission retry");
        xrtSleep(1);
    }
}
static void pendingCheck(xmqttversion V)
{
    pending_fixture X;
    pendingStart(&X,V,3,0,0,false);
    xmqttstoreview View={.Key=99};
    size_t Need=0;
    stStatus(xmqttStoreWriteRead(X.Writer,1,NULL,0,&View,&Need),XMQTT_STORE_LIMIT,"short buffer reports size");
    testRequire(View.Key==99&&Need>0,"size refusal publishes only required bytes");
    unsigned char Buffer[64];
    stStatus(xmqttStoreWriteRead(X.Writer,1,Buffer,sizeof(Buffer),&View,(size_t*)&View),
        XMQTT_STORE_ERROR,"overlapping outputs refused");
    xrtClearError();

    const xmqttstoreview First=pendingRead(X.Writer,1);
    testRequire(pendingRead(X.Writer,1).Epoch==First.Epoch,"peeking does not claim or advance");
    xfuture* F=pendingRetire(X.Writer,&X.StateConfig.Delta,First.Epoch+1,First.Key,10000);
    testRequire(jwOutcome(F)->Status==XMQTT_COMMIT_INVALID&&!jwOutcome(F)->AppendCalled,"wrong epoch rejected before I/O");
    xrtFutureDestroy(F);
    F=pendingRetire(X.Writer,&X.StateConfig.Delta,First.Epoch,First.Key,0);
    testRequire(jwOutcome(F)->Status==XMQTT_COMMIT_TIMEOUT&&!jwOutcome(F)->AppendCalled,"expired retirement leaves committed record");
    xrtFutureDestroy(F);
    (void)pendingRead(X.Writer,1);

    xallocationbudget* Budget=NULL;
    xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"actual one-byte admission cap");
    xfuture* Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(swLegacyAck(X.Writer,&X.StateConfig.Delta,First.Epoch,First.Key,10000,&Unchanged)==XMQTT_JWORK_ERROR&&
        Unchanged==(xfuture*)(uintptr_t)1&&xrtAllocationBudgetLeave(&Scope),"OOM preserves output and pending record");
    xallocationbudgetinfo Info={.Size=sizeof(Info)};
    testRequire(xrtAllocationBudgetInfo(Budget,&Info)&&Info.Rejections&&!Info.LiveBytes,"failed allocations refunded");
    xrtAllocationBudgetFree(Budget);xrtClearError();

    F=pendingRetire(X.Writer,&X.StateConfig.Delta,First.Epoch,2,10000);
    const xmqttcommitresult* Result=jwOutcome(F);
    testRequire(Result->Effect==XMQTT_JOURNAL_COMMITTED,"retire the middle legacy record");
    xrtFutureDestroy(F);
    (void)pendingRead(X.Writer,1);
    F=pendingRetire(X.Writer,&X.StateConfig.Delta,First.Epoch,First.Key,10000);
    Result=jwOutcome(F);
    testRequire(Result->Effect==XMQTT_JOURNAL_COMMITTED&&swInfo(X.Writer).State.Inbox==1,"retire oldest pending key");
    xfuture* Duplicate=pendingRetire(X.Writer,&X.StateConfig.Delta,First.Epoch,First.Key,10000);
    testRequire(jwOutcome(Duplicate)->Status==XMQTT_COMMIT_INVALID&&!jwOutcome(Duplicate)->AppendCalled,
        "repeated retirement leaves the next record intact");
    xrtFutureDestroy(Duplicate);
    pendingAbsent(X.Writer,1);
    pendingAbsent(X.Writer,2);
    (void)pendingRead(X.Writer,3);
    pendingFinish(&X,1);
    testRequire(jwOutcome(F)==Result&&Result->Sequence==6,"commit result outlives every storage owner");
    xrtFutureDestroy(F);
}
static void pendingLate(xmqttversion V,bool Close)
{
    pending_fixture X;
    pendingStart(&X,V,1,21,0,false);
    const xmqttstoreview View=pendingRead(X.Writer,1);
    xfuture* F=pendingRetire(X.Writer,&X.StateConfig.Delta,View.Epoch,View.Key,10000);
    jwAwait(&X.Drive.Entered);
    (void)pendingRead(X.Writer,1); /* old committed view before sync finishes */
    if(Close)testRequire(xmqttStoreWriteClose(X.Writer),"close while ACK file sync is held");
    else testRequire(xrtFutureCancel(F),"cancel while ACK file sync is held");
    testRequire(xrtFutureWaitFor(F,1)==XWAIT_TIMEOUT,"late stop cannot retire pending physical sync");
    xrtAtomic32Store(&X.Drive.Release,1,XMEMORY_RELEASE);
    const xmqttcommitresult* R=jwOutcome(F);
    testRequire(R->Status==(Close?XMQTT_COMMIT_CLOSED:XMQTT_COMMIT_CANCELLED)&&
                R->Effect==XMQTT_JOURNAL_COMMITTED&&swInfo(X.Writer).State.Inbox==0,"late stop publishes actual durable acknowledgement");
    pendingFinish(&X,0);
    xrtFutureDestroy(F);
}
static void pendingFailure(xmqttversion V,unsigned Fail)
{
    pending_fixture X;
    pendingStart(&X,V,1,0,Fail,false);
    const xmqttstoreview View=pendingRead(X.Writer,1);
    xfuture* F=pendingRetire(X.Writer,&X.StateConfig.Delta,View.Epoch,View.Key,10000);
    const xmqttcommitresult* R=jwOutcome(F);
    bool May=Fail==21;
    testRequire(R->Status==XMQTT_COMMIT_STORAGE&&
                R->Effect==(May?XMQTT_JOURNAL_MAY_BE_COMMITTED:XMQTT_JOURNAL_NOT_COMMITTED),"typed physical ACK refusal/uncertainty");
    xfuture* Refused=NULL;
    testRequire(swLegacyAck(X.Writer,&X.StateConfig.Delta,View.Epoch,View.Key,10000,&Refused)==XMQTT_JWORK_CLOSED&&!Refused,
        "failed writer rejects new retirement until recovery");
    pendingFinish(&X,May?0:1);
    xrtFutureDestroy(F);
}
static void pendingCancelledPool(xmqttversion V)
{
    pending_fixture X;
    pendingStart(&X,V,1,0,0,false);
    jwblock Block={0};
    xfuture* Holding=xrtTaskSubmit(X.Pool,jwBlock,&Block,NULL);
    testRequire(Holding!=NULL,"native blocker");
    jwAwait(&Block.Started);
    const xmqttstoreview View=pendingRead(X.Writer,1);
    xfuture* F=pendingRetire(X.Writer,&X.StateConfig.Delta,View.Epoch,View.Key,10000);
    testRequire(xrtTaskPoolCancel(X.Pool),"cancel actual queued native pump");
    const xmqttcommitresult* R=jwOutcome(F);
    testRequire(R->Status==XMQTT_COMMIT_INITIALIZATION&&!R->AppendCalled&&
                R->Effect==XMQTT_JOURNAL_NOT_COMMITTED,"native drop settles accepted ACK without IO");
    xfuture* Refused=NULL;
    testRequire(swLegacyAck(X.Writer,&X.StateConfig.Delta,View.Epoch,View.Key,10000,&Refused)==XMQTT_JWORK_CLOSED&&!Refused,
        "cancelled pool rejects new retirement");
    pendingFinish(&X,1);
    xrtFutureDestroy(F);
    xrtFutureDestroy(Holding);
}
typedef struct pending_partition {
    xmqttstorewrite* Writer;
    const xmqttstoredeltaconfig* Delta;
    unsigned Offset;
} pending_partition;

static int32 pendingConsume(ptr Data)
{
    pending_partition* P=Data;
    for(unsigned Key=P->Offset;Key<=32;Key+=4){
        unsigned char Buffer[64];
        xmqttstoreview View;
        size_t Required;
        stStatus(xmqttStoreWriteRead(P->Writer,Key,Buffer,sizeof(Buffer),&View,&Required),
            XMQTT_STORE_DONE,"read assigned APP partition record");
        xfuture* F=pendingRetire(P->Writer,P->Delta,View.Epoch,View.Key,10000);
        testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"commit retirement from APP partition");
        xrtFutureDestroy(F);
    }
    return 0;
}
static void pendingConcurrent(xmqttversion V)
{
    pending_fixture X;
    pendingStart(&X,V,32,0,0,true);
    pending_partition Work[4];
    xthread* Threads[4];
    for(unsigned I=0;I<4;++I){
        Work[I]=(pending_partition){X.Writer,&X.StateConfig.Delta,I+1};
        Threads[I]=xrtThreadCreate(pendingConsume,Work+I,0);
        testRequire(Threads[I]!=NULL,"parallel APP partition");
    }
    for(unsigned I=0;I<4;++I){
        testRequire(xrtThreadWaitFor(Threads[I],20000)==XWAIT_OK&&xrtThreadExitCode(Threads[I])==0,"join APP partition");
        xrtThreadDestroy(Threads[I]);
    }
    pendingAbsent(X.Writer,1);
    testRequire(!swInfo(X.Writer).State.Inbox,"all legacy records retired");
    pendingFinish(&X,0);
}
int main(void)
{
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        pendingCheck(V);
        pendingLate(V,false);
        pendingLate(V,true);
        pendingFailure(V,15);
        pendingFailure(V,21);
        pendingConcurrent(V);
        pendingCancelledPool(V);
    }
    puts("[PASS] durable pending records: committed reads, stale epochs and duplicates, OOM, late commit, uncertain recovery and APP partitions");
    return 0;
}
