#ifdef PROCESS_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#ifndef PROCESS_OWNERSHIP_SINGLE
#include "../../src/internal/xrt_process.h"
#endif
#include <assert.h>

static bool admit(xrtownershipref ref,ptr context)
{
    const xfuturepayloadownershipv1* payload=xrtProcessFuturePayloadPolicyV1Get();
    xfutureownershipadmissionv1 policy={0};const xrtownershippreparationv1* preparation=NULL;
    (void)context;policy.size=sizeof(policy);policy.PayloadPolicies=&payload;policy.PayloadPolicyCount=1;
    return xrtProcessOwnershipAdapterV1(ref) || xrtFutureOwnershipAdapterV4(ref,&policy,&preparation) ||
        xrtCancelOwnershipAdapterV1(ref) || xrtErrorOwnershipAdapterV1(ref);
}
static void freeze(xrtownershipscope* scope)
{
    double deadline=xrtTimer()+5;
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(xrtTimer()<deadline);xrtThreadYield();}
}
static xprocess* spawn(cstr program)
{
    const cstr args[]={"--process-ownership-child"};xprocessconfig config;
    assert(xrtProcessConfigInit(&config));config.Program=program;config.Args=args;config.ArgCount=1;
    config.Stdin.Mode=XPROCESS_IO_PIPE;config.Stdout.Mode=XPROCESS_IO_NULL;config.Stderr.Mode=XPROCESS_IO_NULL;
    config.HideWindow=true;return xrtProcessSpawn(&config);
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void lifecycle(cstr program)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xprocess* process=spawn(program);assert(process);xthread* waiter=xrtThreadRef(process->Waiter);assert(waiter);
    xrtownershipref ref=xrtProcessOwnership(process);size_t count=0;
    xrtownershipscope scope={0};freeze(&scope);
    const xrtownershipadapterv1* adapter=xrtProcessOwnershipAdapterV1(ref);assert(adapter);
    assert(ref.Ops->Count(ref.Data,&count) && count==2); /* user + actual waiter */
    assert(adapter->Hold(ref.Data));
    assert(ref.Ops->Count(ref.Data,&count) && count==3 && process->UserRefs==1);
    assert(xrtOwnershipScopeEnd(&scope));
    xfuture* future=xrtProcessWaitAsync(process);assert(future && !xrtFutureDone(future));
    xfuture* same=xrtProcessWaitAsync(process);assert(same==future);xrtFutureDestroy(same);
    memset(&scope,0,sizeof(scope));freeze(&scope);
    xrtownershipsnapshot* snapshot=NULL;xrtownershipref slots[2];slots[0]=ref;slots[1]=ref;
    assert(xrtOwnershipSnapshotCreate(&ref,1,slots,2,admit,NULL,&snapshot));
    assert(xrtOwnershipSnapshotNodeCount(snapshot)==3); /* process, shared state, cancel */
    xrtownershipnode node;assert(xrtOwnershipSnapshotNode(snapshot,0,&node));
    assert(node.Reference.Data==process && node.StrongCount==3 && node.InternalCount==2 && node.Reachable);
    xrtOwnershipSnapshotDestroy(snapshot);assert(xrtOwnershipScopeEnd(&scope));
    /* Collector Hold is not a user endpoint: dropping the last user MUST
     * deliver stdin EOF even while the real process control block stays pinned. */
    xrtProcessDestroy(process);
    assert(xrtFutureWaitFor(future,5000)==XWAIT_OK);
    assert(xrtThreadWaitFor(waiter,5000)==XWAIT_OK && !xrtThreadExitCode(waiter));
    assert(((const xprocessstatus*)xrtFutureValue(future))->Code==7);
    memset(&scope,0,sizeof(scope));freeze(&scope);
    assert(ref.Ops->Count(ref.Data,&count) && count==1 && process->UserRefs==0);
    assert(!xrtFutureOwnershipAdapterV1(xrtFutureOwnership(future),NULL,0)); /* no payload capability */
    snapshot=NULL;assert(xrtOwnershipSnapshotCreate(&ref,1,&ref,1,admit,NULL,&snapshot));
    assert(xrtOwnershipSnapshotNode(snapshot,0,&node) && !node.Reachable && node.StrongCount==1);
    xrtOwnershipSnapshotDestroy(snapshot);
    const int token=1,other=2;
    assert(adapter->Claim(ref.Data,&token));assert(!adapter->Claim(ref.Data,&other));
    adapter->Restore(ref.Data,&token);assert(adapter->Claim(ref.Data,&token));
    adapter->Clear(ref.Data,&token);assert(!ref.Ops->Count(ref.Data,&count));
    assert(process->WaitFuture==future); /* retired slot still physically owns */
    assert(xrtOwnershipScopeEnd(&scope));
    assert(adapter->Finish(ref.Data,&token) && adapter->Finish(ref.Data,&token));
    assert(!process->WaitFuture && !process->WaitPromise && !process->Error);
    assert(((const xprocessstatus*)xrtFutureValue(future))->Code==7);
    adapter->Drop(ref.Data);xrtFutureDestroy(future);xrtThreadDestroy(waiter);balance(&before);
    puts("process owning graph: 3 physical nodes; worker root and non-user pins; phased retirement balanced");
}
static void prefixes(cstr program)
{
    unsigned point;
    for(point=0;point<128;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xprocess* process=spawn(program);assert(process);xthread* waiter=xrtThreadRef(process->Waiter);assert(waiter);
        assert(xrtMemDebugFailAfter(point));xfuture* future=xrtProcessWaitAsync(process);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        assert(hit ? future==NULL : future!=NULL);xrtClearError();
        if(!future)future=xrtProcessWaitAsync(process);
        assert(future);xfuture* same=xrtProcessWaitAsync(process);assert(same==future);xrtFutureDestroy(same);
        xrtProcessDestroy(process);assert(xrtFutureWaitFor(future,5000)==XWAIT_OK);
        assert(xrtThreadWaitFor(waiter,5000)==XWAIT_OK && !xrtThreadExitCode(waiter));
        assert(((const xprocessstatus*)xrtFutureValue(future))->Code==7);
        xrtFutureDestroy(future);xrtThreadDestroy(waiter);balance(&before);
        if(!hit)break;
    }
    assert(point>0 && point<128);printf("process shared Future prefixes=%u first-no-hit=%u balanced\n",point+1,point);
}
int main(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--process-ownership-child")){while(fgetc(stdin)!=EOF){}return ferror(stdin)?90:7;}
    testRequire(xrtMemDebugEnable(true),"memory debug setup failed");xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    lifecycle(argv[0]);prefixes(argv[0]);assert(!xrtGetError());return 0;
}
