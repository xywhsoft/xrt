#include "support/journal_worker_peer.h"
#if defined(XMQTT_TEST_LINK_ALLOCATION_WRAP)
static _Thread_local bool waArmed;
static _Thread_local unsigned waCount,waTarget;
static _Thread_local xallocationbudget* waBudget;
extern ptr __real_xrtMalloc(size_t N);
extern ptr __real_xrtCalloc(size_t N,size_t Size);
static void waBefore(size_t N)
{if(waArmed&&++waCount==waTarget){waArmed=false;xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(N&&xrtAllocationBudgetInfo(waBudget,&I)&&xrtAllocationBudgetSetLimit(waBudget,I.LiveBytes+N-1u),"real allocator capacity at observed linked allocation edge");}}
ptr __wrap_xrtMalloc(size_t N){waBefore(N);return __real_xrtMalloc(N);}
ptr __wrap_xrtCalloc(size_t N,size_t Size){waBefore(N*Size);return __real_xrtCalloc(N,Size);}
static void waPrefixes(void)
{
    xtaskpool* Pool=jwPool(2);unsigned Total=0;
    for(unsigned Target=0;Target<=Total;++Target){jwdrive D={0};jpfile File=jwFile(&D);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);xmqttjournalworker* W=jwCreate(File,&D,Pool,&C);xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&waBudget)&&xrtAllocationBudgetEnter(waBudget,&Scope),"admission actual allocation ledger");waCount=0;waTarget=Target;waArmed=true;xfuture* F=(xfuture*)(uintptr_t)1;xmqttjournaladmission A=xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("linked OOM prefix"),10000,&F);waArmed=false;
        if(!Target){testRequire(A==XMQTT_JWORK_ACCEPTED&&jwOutcome(F)->Effect==XMQTT_JOURNAL_COMMITTED,"allocation prefix control");Total=waCount;testRequire(Total>=5&&Total<64,"all extension and Core Promise/token/task allocations observed");xrtFutureDestroy(F);}
        else testRequire(A==XMQTT_JWORK_ERROR&&F==(xfuture*)(uintptr_t)1&&waCount==Target,"each linked real allocation rejected without fabricated NULL or output publication");
        testRequire(xmqttJournalWorkerDestroy(W),"OOM still physically retires");xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"prefix budget leave");xallocationbudgetinfo I={.Size=sizeof(I)};testRequire(xrtAllocationBudgetInfo(waBudget,&I)&&!I.LiveBytes&&!I.LiveCount&&(!Target||I.Rejections),"all caller/queued/native/result prefix allocations physically refunded");xrtAllocationBudgetFree(waBudget);waBudget=NULL;jpFileClose(File);
    }
    jwPoolDone(Pool);printf("[PASS] %u linked extension+Core actual admission allocator-prefix rejections\n",Total);
}
#endif
static void waRefusals(void)
{
    jwdrive D={0};jpfile File=jwFile(&D);xtaskpool* Pool=jwPool(1);xmqttjournalconfig JC=jpConfig();xmqttjournal* J=jpCreate(File,&JC);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);xmqttjournalworker* W=(xmqttjournalworker*)(uintptr_t)1;
    testRequire(!xmqttJournalWorkerCreate(NULL,J,&C,&W)&&W==(xmqttjournalworker*)(uintptr_t)1,"missing host rejected with unchanged output");C.Size=0;testRequire(!xmqttJournalWorkerCreate(Pool,J,&C,&W),"config size rejected");C.Size=sizeof(C);C.MaxBytes=0;testRequire(!xmqttJournalWorkerCreate(Pool,J,&C,&W),"fixed owner bytes bounded");C.MaxBytes=4194304;
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B)&&xrtAllocationBudgetEnter(B,&Scope),"real constructor OOM budget");testRequire(!xmqttJournalWorkerCreate(Pool,J,&C,&W)&&W==(xmqttjournalworker*)(uintptr_t)1,"constructor true OOM leaves Journal caller-owned");xrtClearError();testRequire(xrtAllocationBudgetLeave(&Scope),"constructor scope leave");xallocationbudgetinfo BI={.Size=sizeof(BI)};testRequire(xrtAllocationBudgetInfo(B,&BI)&&!BI.LiveBytes&&!BI.LiveCount&&BI.Rejections,"constructor rejection no leaked owner");xrtAllocationBudgetFree(B);testRequire(jpInfo(J).State==XMQTT_JOURNAL_READY,"constructor never consumed on failure");
    C.MaxObjects=1;C.MaxTransactionBytes=4;testRequire(xmqttJournalWorkerCreate(Pool,J,&C,&W),"fault worker creation");testRequire(xmqttJournalWorkerWaitFor(W,0)==XWAIT_ERROR,"Close required before wait");xrtClearError();xfuture* F=(xfuture*)(uintptr_t)1;
    testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("oversize"),1000,&F)==XMQTT_JWORK_OVERLOADED&&F==(xfuture*)(uintptr_t)1,"payload cap normal refusal");testRequire(xmqttJournalWorkerAppendAsync(W,(xbytesview){NULL,1},1000,&F)==XMQTT_JWORK_ERROR&&xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("x"),-2,&F)==XMQTT_JWORK_ERROR&&F==(xfuture*)(uintptr_t)1,"invalid spans/deadline cannot publish");
    xrtClearError();F=NULL;testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("one"),10000,&F)==XMQTT_JWORK_ACCEPTED,"one object accepted");const xmqttcommitresult* R=jwOutcome(F);xfuture* Refused=(xfuture*)(uintptr_t)1;
    testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("two"),10000,&Refused)==XMQTT_JWORK_OVERLOADED&&Refused==(xfuture*)(uintptr_t)1,"retained completed result cannot hide object capacity");
    testRequire(!xmqttJournalWorkerInfo(W,(xmqttjournalworkerinfo*)(uintptr_t)R)&&xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("x"),1000,(xfuture**)(uintptr_t)R->Data.Data)==XMQTT_JWORK_ERROR&&R->Sequence==1&&!memcmp(R->Data.Data,"one",3),"owned result/wire outputs refused without mutation");xrtClearError();xrtFutureDestroy(F);testRequire(!jwSnapshot(W).Objects,"physical Future destruction returns credits");
    testRequire(xrtTaskPoolClose(Pool),"host independently closes");F=(xfuture*)(uintptr_t)1;testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("x"),10000,&F)==XMQTT_JWORK_ERROR&&F==(xfuture*)(uintptr_t)1,"native scheduling rejection consumes no input/output");testRequire(!jwSnapshot(W).Objects&&!jwSnapshot(W).Requests&&!jwSnapshot(W).Tasks,"native rejection refunds reservations and pump tail");xrtClearError();testRequire(xmqttJournalWorkerClose(W),"explicit close");testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("x"),1000,&F)==XMQTT_JWORK_CLOSED&&F==(xfuture*)(uintptr_t)1&&xmqttJournalWorkerDestroy(W),"closed admission unchanged and actual retirement");jwPoolDone(Pool);jpFileClose(File);
}
static void waLimit(void)
{
    jwdrive D={0};jpfile F=jwFile(&D);xtaskpool* P=jwPool(1);xmqttjournalconfig JC=jpConfig();JC.MaxFileBytes=12288+64+1;xmqttjournal* J=jpCreate(F,&JC);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);xmqttjournalworker* W=NULL;testRequire(xmqttJournalWorkerCreate(P,J,&C,&W),"one-transaction storage cap");xfuture *A=NULL,*B=NULL;testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("a"),10000,&A)==XMQTT_JWORK_ACCEPTED&&jwOutcome(A)->Effect==XMQTT_JOURNAL_COMMITTED,"first actual commit");testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("b"),10000,&B)==XMQTT_JWORK_ACCEPTED,"second valid admission before storage cap");const xmqttcommitresult* R=jwOutcome(B);testRequire(R->Status==XMQTT_COMMIT_LIMIT&&R->AppendStatus==XMQTT_JOURNAL_LIMIT&&R->Effect==XMQTT_JOURNAL_NOT_COMMITTED&&!R->Sequence&&!R->AttemptSequence&&!R->Cause&&!jwSnapshot(W).Failed,"local LIMIT cannot inherit previous COMMITTED effect or stale cause");testRequire(xmqttJournalWorkerDestroy(W),"limited owner retirement");jwPoolDone(P);jpFileClose(F);xrtFutureDestroy(A);xrtFutureDestroy(B);
}
typedef struct wanested {xmqttjournalworker *First,*Second;xfuture* Submitted;bool Inside;unsigned Calls,Notices;} wanested;
static void waNestedDrop(ptr Value,ptr Data)
{
    (void)Data;wanested* X=Value;testRequire(X->Inside,"cancelled neighbour Drop actually executes inside nested admission");
    testRequire(xmqttJournalWorkerWaitFor(X->First,0)==XWAIT_ERROR&&xmqttJournalWorkerWaitFor(X->Second,0)==XWAIT_ERROR&&!xmqttJournalWorkerDestroy(X->First),"allocation-free nested scope stack protects every enclosing storage owner");++X->Calls;
}
static xtaskoutcome waNever(xcancel* C,ptr Data,xtaskvalue* V){(void)C;(void)Data;(void)V;testRequire(false,"raw-cancelled queued neighbour never runs");return XTASK_FAILED;}
static void waNestedNotice(const xerror* E,ptr Data)
{
    (void)E;wanested* X=Data;++X->Notices;testRequire(xmqttJournalWorkerClose(X->First),"Close is reentrant during native schedule rejection");X->Inside=true;
    testRequire(xmqttJournalWorkerAppendAsync(X->Second,XRT_BYTES_LITERAL("next"),10000,&X->Submitted)==XMQTT_JWORK_ACCEPTED,"second admission prunes the raw-cancelled native queue neighbour");X->Inside=false;
}
static void waNested(void)
{
    jwdrive A={0},B={0};jpfile FA=jwFile(&A),FB=jwFile(&B);xtaskpool* P=jwPool(1);xtaskpoolconfig PC={.Threads=1,.QueueLimit=1};xtaskpool* Q=xrtTaskPoolCreate(&PC);testRequire(Q!=NULL,"bounded nested host pool");xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);wanested X={0};X.First=jwCreate(FA,&A,P,&C);X.Second=jwCreate(FB,&B,Q,&C);
    jwblock Block={0};xfuture* Holding=xrtTaskSubmit(Q,jwBlock,&Block,NULL);jwAwait(&Block.Started);xtaskargs Args={.Destroy=waNestedDrop};xfuture* Cancelled=xrtTaskSubmit(Q,waNever,&X,&Args);testRequire(Cancelled!=NULL,"cancelled neighbour occupies one native queue slot");xcancel* Token=xrtFutureCancelToken(Cancelled);testRequire(Token&&xrtCancelRequest(Token),"raw token cancellation defers queue pruning");xrtCancelDestroy(Token);
    testRequire(xrtTaskPoolClose(P),"first pool rejects its new pump");xrtSetErrorHandler(waNestedNotice,&X);xfuture* F=(xfuture*)(uintptr_t)1;xmqttjournaladmission Admission=xmqttJournalWorkerAppendAsync(X.First,XRT_BYTES_LITERAL("first"),10000,&F);xrtSetErrorHandler(NULL,NULL);testRequire(Admission==XMQTT_JWORK_ERROR&&F==(xfuture*)(uintptr_t)1&&X.Notices==1&&X.Calls==1,"nested native diagnostic/pruning callbacks preserve original rejected admission");xrtClearError();xrtAtomic32Store(&Block.Released,1,XMEMORY_RELEASE);testRequire(jwOutcome(X.Submitted)->Effect==XMQTT_JOURNAL_COMMITTED&&xmqttJournalWorkerDestroy(X.First)&&xmqttJournalWorkerDestroy(X.Second),"both enclosing owners retire after nested scopes unwind");jwPoolDone(P);jwPoolDone(Q);jpFileClose(FA);jpFileClose(FB);xrtFutureDestroy(Holding);xrtFutureDestroy(Cancelled);xrtFutureDestroy(X.Submitted);
}
static unsigned waAmbientNotices;
static void waNotice(const xerror* E,ptr Data){if(E==Data)++waAmbientNotices;}
static void waZeroCaps(void)
{
    xtaskpool* Pool=jwPool(1);size_t Base=0;
    for(unsigned Kind=0;Kind<7;++Kind){jwdrive D={0};jpfile F=jwFile(&D);xmqttjournalworkerconfig C;xmqttJournalWorkerConfigInit(&C);
        if(Kind==1)C.MaxRequests=0;else if(Kind==2)C.MaxObjects=0;else if(Kind==3)C.MaxTasks=0;else if(Kind==4)C.MaxTransactionBytes=0;else if(Kind==5)C.WorkTransactions=0;else if(Kind==6)C.MaxBytes=Base;
        xmqttjournalworker* W=jwCreate(F,&D,Pool,&C);if(!Kind)Base=jwSnapshot(W).BaseBytes;
        else {xrtSetErrorInfo(XERR_AGAIN,"worker.ambient",1,"caller owner");const xerror* Old=xrtGetError();waAmbientNotices=0;xrtSetErrorHandler(waNotice,(ptr)Old);xfuture* Future=(xfuture*)(uintptr_t)1;testRequire(xmqttJournalWorkerAppendAsync(W,XRT_BYTES_LITERAL("x"),1000,&Future)==XMQTT_JWORK_OVERLOADED&&Future==(xfuture*)(uintptr_t)1&&xrtGetError()==Old&&!waAmbientNotices,"every zero/fixed-byte cap refuses quietly with exact ambient owner");xrtSetErrorHandler(NULL,NULL);xrtClearError();xmqttjournalworkerinfo I=jwSnapshot(W);testRequire(!I.Objects&&!I.Requests&&!I.Tasks&&!D.Base.Calls,"normal resource refusal refunds all partial producer state without IO");}
        testRequire(xmqttJournalWorkerDestroy(W),"zero-cap owner retirement");jpFileClose(F);}
    jwPoolDone(Pool);
}
int main(void){waRefusals();waLimit();waNested();waZeroCaps();
#if defined(XMQTT_TEST_LINK_ALLOCATION_WRAP)
    waPrefixes();
#endif
    puts("[PASS] actual constructor OOM,limits,aliases,retained credits,host rejection and previous-effect isolation");return 0;}
