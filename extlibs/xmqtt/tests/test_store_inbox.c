#include "support/store_write_peer.h"
#include <xmqtt/store_inbox.h>
#include <xrt/thread.h>

typedef struct inbox_fixture {
    jwdrive Drive;
    jpfile File;
    str NativePath;
    xmqttjournalconfig JournalConfig;
    xmqttstorestateconfig StateConfig;
    xmqttstorewriteconfig WriterConfig;
    xtaskpool* Pool;
    xmqttstorewrite* Writer;
} inbox_fixture;

static void inboxStart(inbox_fixture* X,xmqttversion V,unsigned Count,unsigned Hold,unsigned Fail,bool Native)
{
    memset(X,0,sizeof(*X));
    X->JournalConfig=stJournalConfig(V);
    X->StateConfig=stConfig(V);
    X->StateConfig.MaxInbox=64;
    X->StateConfig.WallMilliseconds=0;
    testRequire(xmqttStoreWriteConfigInit(&X->WriterConfig),"Inbox writer config");
    X->File=jwFile(&X->Drive);
    if(Native){
        X->Drive.Base.Native=xrtFileTemp(NULL,"xmqtt-lease-",NULL,&X->NativePath);
        testRequire(X->Drive.Base.Native!=NULL,"native temporary journal");
    }
    xmqttjournal* J=jpCreate(X->File,&X->JournalConfig);
    xmqttstorestate* S=NULL;
    stStatus(xmqttStoreStateRecover(J,&X->StateConfig,&S),XMQTT_STORE_DONE,"empty committed prefix");
    X->Pool=jwPool(1);
    testRequire(xmqttStoreInboxCreate(X->Pool,J,S,&X->WriterConfig,&X->Writer),"explicit lease mode");
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
        testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"real publication enters ready Inbox");
        xrtFutureDestroy(F);
    }
}
static void inboxFinish(inbox_fixture* X,size_t Remaining)
{
    testRequire(xmqttStoreWriteDestroy(X->Writer),"physical lease writer retirement");
    X->Writer=NULL;
    jwPoolDone(X->Pool);
    X->Drive.Base.Fail=0;
    X->Drive.Hold=0;
    xmqttjournal* J=jpReopen(X->File,&X->JournalConfig);
    xmqttstorestate* S=NULL;
    X->StateConfig.WallMilliseconds=200000;
    stStatus(xmqttStoreStateRecover(J,&X->StateConfig,&S),XMQTT_STORE_DONE,"fresh lease recovery");
    testRequire(stInfo(S).Inbox==Remaining,"fresh recovery agrees with confirmed effects");
    xmqttStoreStateDestroy(S);
    testRequire(xmqttJournalDestroy(J),"close recovered journal");
    jpFileClose(X->File);
    if(X->Drive.Base.Native){
        testRequire(xrtClose(X->Drive.Base.Native)&&xrtFileDelete(X->NativePath),"release own native test journal");
        xrtFree(X->NativePath);
    }
}
static xmqttstorelease inboxTake(xmqttstorewrite* W,uint64 Key)
{
    unsigned char Buffer[64];
    xmqttstorelease Lease;
    xmqttstoreview View;
    size_t Required=0;
    stStatus(xmqttStoreInboxTake(W,Buffer,sizeof(Buffer),&Lease,&View,&Required),XMQTT_STORE_DONE,"take exclusive committed delivery");
    testRequire(View.Key==Key&&View.Wire.Data==Buffer&&View.Wire.Size==Required&&Buffer[Required-1]=='x',"canonical packet copied into caller buffer");
    return Lease;
}
static void inboxEmpty(xmqttstorewrite* W)
{
    unsigned char Buffer[64];
    xmqttstorelease Lease={{99}};
    xmqttstoreview View={.Key=99};
    size_t Required=99;
    stStatus(xmqttStoreInboxTake(W,Buffer,sizeof(Buffer),&Lease,&View,&Required),XMQTT_STORE_EMPTY,"no unleased delivery");
    testRequire(Lease.Opaque[0]==99&&View.Key==99&&Required==99,"empty publishes no outputs");
}
static xfuture* inboxAck(xmqttstorewrite* W,xmqttstorelease Lease,int64 Timeout)
{
    xfuture* F=NULL;
    double Deadline=xrtTimer()+10;
    for(;;){
        xmqttjournaladmission A=xmqttStoreInboxAckAsync(W,Lease,Timeout,&F);
        if(A==XMQTT_JWORK_ACCEPTED)return F;
        testRequire(A==XMQTT_JWORK_OVERLOADED&&!F&&xrtTimer()<Deadline,"bounded native scheduling retry");
        xrtSleep(1);
    }
}
static void inboxCheck(xmqttversion V)
{
    inbox_fixture X;
    inboxStart(&X,V,3,0,0,false);
    xmqttstorelease Lease={{99}};
    xmqttstoreview View={.Key=99};
    size_t Need=0;
    stStatus(xmqttStoreInboxTake(X.Writer,NULL,0,&Lease,&View,&Need),XMQTT_STORE_LIMIT,"short copy reserves nothing");
    testRequire(Lease.Opaque[0]==99&&View.Key==99&&Need>0,"size refusal publishes only required bytes");
    unsigned char Buffer[64];
    stStatus(xmqttStoreInboxTake(X.Writer,Buffer,sizeof(Buffer),&Lease,&View,(size_t*)&View),XMQTT_STORE_ERROR,"overlapping outputs refused");
    xrtClearError();
    xmqttstorelease First=inboxTake(X.Writer,1);
    testRequire(xmqttStoreInboxRelease(X.Writer,First),"return head to ready tail");
    testRequire(!xmqttStoreInboxRelease(X.Writer,First),"copied old claim cannot release new owner");
    xrtClearError();
    xmqttstorelease Second=inboxTake(X.Writer,2),Third=inboxTake(X.Writer,3);
    Lease=inboxTake(X.Writer,1);
    inboxEmpty(X.Writer);
    xfuture* Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(xmqttStoreInboxAckAsync(X.Writer,First,1000,&Unchanged)==XMQTT_JWORK_ERROR&&
                Unchanged==(xfuture*)(uintptr_t)1,"stale claim cannot acknowledge leased delivery");
    xrtClearError();

    xmqttstoredelta Raw={.Type=XMQTT_STORE_INBOX_ACK,.Epoch=1,.Key=2};
    xfuture* F=swOps(X.Writer,&X.StateConfig.Delta,&Raw,1);
    testRequire(jwOutcome(F)->Status==XMQTT_COMMIT_INVALID&&!jwOutcome(F)->AppendCalled,"raw acknowledgement cannot bypass a lease");
    xrtFutureDestroy(F);
    F=inboxAck(X.Writer,Second,0);
    testRequire(jwOutcome(F)->Status==XMQTT_COMMIT_TIMEOUT&&!jwOutcome(F)->AppendCalled,"expired queued ACK requeues before publication");
    xrtFutureDestroy(F);
    Second=inboxTake(X.Writer,2);

    xallocationbudget* Budget=NULL;
    xallocationscope Scope={0};
    testRequire(xrtAllocationBudgetCreate(1,&Budget)&&xrtAllocationBudgetEnter(Budget,&Scope),"actual one-byte admission cap");
    Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(xmqttStoreInboxAckAsync(X.Writer,Third,10000,&Unchanged)==XMQTT_JWORK_ERROR&&
                Unchanged==(xfuture*)(uintptr_t)1&&xrtAllocationBudgetLeave(&Scope),"OOM leaves the original claim live and output unchanged");
    xallocationbudgetinfo Info={.Size=sizeof(Info)};
    testRequire(xrtAllocationBudgetInfo(Budget,&Info)&&Info.Rejections&&!Info.LiveBytes,"real allocation refused and refunded");
    xrtAllocationBudgetFree(Budget);
    xrtClearError();
    testRequire(xmqttStoreInboxRelease(X.Writer,Third),"release same lease after an admission refusal");
    Third=inboxTake(X.Writer,3);

    F=inboxAck(X.Writer,Second,10000);
    const xmqttcommitresult* Result=jwOutcome(F);
    testRequire(Result->Status==XMQTT_COMMIT_OK&&Result->Effect==XMQTT_JOURNAL_COMMITTED&&
                swInfo(X.Writer).State.Inbox==2,"ACK commits, removes delivery and resolves typed outcome");
    testRequire(xmqttStoreInboxRelease(X.Writer,Third)&&xmqttStoreInboxRelease(X.Writer,Lease),"leave other deliveries durably pending");
    inboxFinish(&X,2);
    testRequire(jwOutcome(F)==Result&&Result->Sequence==5,"ACK result survives all storage owners and file");
    xrtFutureDestroy(F);
}
static void inboxLate(xmqttversion V,bool Close)
{
    inbox_fixture X;
    inboxStart(&X,V,1,21,0,false);
    xmqttstorelease Lease=inboxTake(X.Writer,1);
    xfuture* F=inboxAck(X.Writer,Lease,10000);
    jwAwait(&X.Drive.Entered);
    inboxEmpty(X.Writer);
    testRequire(!xmqttStoreInboxRelease(X.Writer,Lease),"active IO owns the claim");
    xrtClearError();
    xfuture* NoDuplicate=NULL;
    testRequire(xmqttStoreInboxAckAsync(X.Writer,Lease,0,&NoDuplicate)==XMQTT_JWORK_ERROR&&!NoDuplicate,"no second ACK while first is in native IO");
    xrtClearError();
    if(Close)testRequire(xmqttStoreWriteClose(X.Writer),"close while ACK file sync is held");
    else testRequire(xrtFutureCancel(F),"cancel while ACK file sync is held");
    testRequire(xrtFutureWaitFor(F,1)==XWAIT_TIMEOUT,"late stop cannot retire pending physical sync");
    xrtAtomic32Store(&X.Drive.Release,1,XMEMORY_RELEASE);
    const xmqttcommitresult* R=jwOutcome(F);
    testRequire(R->Status==(Close?XMQTT_COMMIT_CLOSED:XMQTT_COMMIT_CANCELLED)&&
                R->Effect==XMQTT_JOURNAL_COMMITTED&&swInfo(X.Writer).State.Inbox==0,"late stop publishes actual durable acknowledgement");
    inboxFinish(&X,0);
    xrtFutureDestroy(F);
}
static void inboxFailure(xmqttversion V,unsigned Fail)
{
    inbox_fixture X;
    inboxStart(&X,V,1,0,Fail,false);
    xmqttstorelease Lease=inboxTake(X.Writer,1);
    xfuture* F=inboxAck(X.Writer,Lease,10000);
    const xmqttcommitresult* R=jwOutcome(F);
    bool May=Fail==21;
    testRequire(R->Status==XMQTT_COMMIT_STORAGE&&
                R->Effect==(May?XMQTT_JOURNAL_MAY_BE_COMMITTED:XMQTT_JOURNAL_NOT_COMMITTED),"typed physical ACK refusal/uncertainty");
    unsigned char Buffer[64];
    xmqttstoreview View;
    size_t Bytes=0;
    stStatus(xmqttStoreInboxTake(X.Writer,Buffer,sizeof(Buffer),&Lease,&View,&Bytes),XMQTT_STORE_STATE_ERROR,"failed writer issues no new claims");
    inboxFinish(&X,May?0:1);
    xrtFutureDestroy(F);
}
static void inboxIncarnation(xmqttversion V)
{
    inbox_fixture X;
    inboxStart(&X,V,1,0,0,false);
    xmqttstorelease Old=inboxTake(X.Writer,1);
    testRequire(xmqttStoreWriteDestroy(X.Writer),"unreleased copied token does not pin storage owner");
    xmqttjournal* J=jpReopen(X.File,&X.JournalConfig);
    xmqttstorestate* State=NULL;
    X.StateConfig.WallMilliseconds=200000;
    stStatus(xmqttStoreStateRecover(J,&X.StateConfig,&State),XMQTT_STORE_DONE,"recovered unacknowledged delivery");
    X.Writer=NULL;
    testRequire(xmqttStoreInboxCreate(X.Pool,J,State,&X.WriterConfig,&X.Writer),"fresh Inbox incarnation");
    xmqttstorelease Fresh=inboxTake(X.Writer,1);
    xfuture* Unchanged=(xfuture*)(uintptr_t)1;
    testRequire(!xmqttStoreInboxRelease(X.Writer,Old)&&
                xmqttStoreInboxAckAsync(X.Writer,Old,10000,&Unchanged)==XMQTT_JWORK_ERROR&&
                Unchanged==(xfuture*)(uintptr_t)1,"previous Writer's token cannot mutate recovered state");
    xrtClearError();
    xfuture* F=inboxAck(X.Writer,Fresh,10000);
    testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"fresh claim works while offline awaiting broker decision");
    xrtFutureDestroy(F);
    inboxFinish(&X,0);
}
static void inboxCancelledPool(xmqttversion V)
{
    inbox_fixture X;
    inboxStart(&X,V,1,0,0,false);
    jwblock Block={0};
    xfuture* Holding=xrtTaskSubmit(X.Pool,jwBlock,&Block,NULL);
    testRequire(Holding!=NULL,"native blocker");
    jwAwait(&Block.Started);
    xmqttstorelease Lease=inboxTake(X.Writer,1);
    xfuture* F=inboxAck(X.Writer,Lease,10000);
    testRequire(xrtTaskPoolCancel(X.Pool),"cancel actual queued native pump");
    const xmqttcommitresult* R=jwOutcome(F);
    testRequire(R->Status==XMQTT_COMMIT_INITIALIZATION&&!R->AppendCalled&&
                R->Effect==XMQTT_JOURNAL_NOT_COMMITTED,"native drop settles accepted ACK without IO");
    unsigned char Buffer[64];
    xmqttstoreview View;
    size_t Need=0;
    stStatus(xmqttStoreInboxTake(X.Writer,Buffer,sizeof(Buffer),&Lease,&View,&Need),XMQTT_STORE_STATE_ERROR,
             "cancelled host pool fences new delivery claims");
    inboxFinish(&X,1);
    xrtFutureDestroy(F);
    xrtFutureDestroy(Holding);
}
typedef struct inbox_parallel {xmqttstorewrite* Writer;xatomic32 Start,Count; xatomic32 Seen[32];} inbox_parallel;
static int32 inboxConsume(ptr Data)
{
    inbox_parallel* P=Data;
    jwAwait(&P->Start);
    for(unsigned I=0;I<8;++I){
        unsigned char Buffer[64];
        xmqttstorelease Lease;
        xmqttstoreview View;
        size_t Bytes;
        stStatus(xmqttStoreInboxTake(P->Writer,Buffer,sizeof(Buffer),&Lease,&View,&Bytes),XMQTT_STORE_DONE,"parallel Take");
        testRequire(View.Key>=1&&View.Key<=32&&xrtAtomic32FetchAdd(&P->Seen[View.Key-1],1,XMEMORY_ACQ_REL)==0,"no duplicate concurrent selection");
        xfuture* F=inboxAck(P->Writer,Lease,10000);
        testRequire(jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"parallel ACK retains exclusive claim");
        xrtFutureDestroy(F);
        xrtAtomic32FetchAdd(&P->Count,1,XMEMORY_RELEASE);
    }
    return 0;
}
static void inboxConcurrent(xmqttversion V)
{
    inbox_fixture X;
    inboxStart(&X,V,32,0,0,true);
    inbox_parallel P={.Writer=X.Writer};
    xthread* Threads[4];
    for(size_t I=0;I<4;++I){
        Threads[I]=xrtThreadCreate(inboxConsume,&P,0);
        testRequire(Threads[I]!=NULL,"parallel application consumer");
    }
    xrtAtomic32Store(&P.Start,1,XMEMORY_RELEASE);
    for(size_t I=0;I<4;++I){
        testRequire(xrtThreadWaitFor(Threads[I],20000)==XWAIT_OK&&xrtThreadExitCode(Threads[I])==0,"actual consumer retirement");
        xrtThreadDestroy(Threads[I]);
    }
    testRequire(xrtAtomic32Load(&P.Count,XMEMORY_ACQUIRE)==32&&swInfo(X.Writer).State.Inbox==0,"all 32 deliveries acknowledged on native journal");
    inboxFinish(&X,0);
}
int main(void)
{
    for(unsigned I=0;I<2;++I){
        xmqttversion V=I?XMQTT_VERSION_5:XMQTT_VERSION_311;
        inboxCheck(V);
        inboxIncarnation(V);
        inboxLate(V,false);
        inboxLate(V,true);
        inboxFailure(V,15);
        inboxFailure(V,21);
        inboxConcurrent(V);
        inboxCancelledPool(V);
    }
    puts("[PASS] durable Inbox leases: exclusive FIFO, stale token fencing, no-IO refusal, real OOM, late commit, failed/uncertain sync, fresh recovery and concurrent consumers");
    return 0;
}
