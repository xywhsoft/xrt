#ifdef FUTURE_BRIDGE_OWNERSHIP_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#include <xrt/future_bridge.h>
#include <assert.h>

typedef struct BridgeCase {
    xfuturebridge bridge;
    unsigned refs, notified, dropped;
    bool reenter, inspect_during_drop;
} BridgeCase;
static unsigned synchronous, races, pins;
static void freeze_begin(xrtownershipscope* scope)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static bool data_count(const void* data,size_t* count)
{const BridgeCase* test=data;*count=test->refs;return test->refs!=0;}
static bool data_trace(const void* data,xrtownershipvisitor visit,ptr other)
{(void)data;(void)visit;(void)other;return true;}
static const xrtownershipops data_ops={data_count,data_trace};
static int32 unpublished_probe(ptr data)
{
    BridgeCase* test=data;xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref marker={test,&data_ops},out=marker;
    xerror* error=xrtErrorCreate(XERR_STATE,"bridge.owned",1,"marker");assert(error);xrtSetError(error);
    assert(!xrtFutureBridgeWatchOwnershipV1(&test->bridge,&out));
    assert(out.Data==marker.Data&&out.Ops==marker.Ops&&xrtGetError()==error);
    xrtClearError();xrtErrorFree(error);assert(xrtOwnershipScopeEnd(&freeze));return 0;
}
static void cross_thread_probe(BridgeCase* test)
{
    xthread* thread=xrtThreadCreate(unpublished_probe,test,0);assert(thread);
    assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);
}
static void notify(ptr data)
{
    BridgeCase* test=data;++test->notified;
    if(test->reenter){
        cross_thread_probe(test);
        assert(!xrtFutureBridgeReady(&test->bridge)&&xrtErrorKind(xrtGetError())==XERR_STATE);
        xrtClearError();xrtFutureBridgeUnwatch(&test->bridge);
        assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    }
}
static void drop(const void* data)
{
    BridgeCase* test=(BridgeCase*)data;
    if(test->inspect_during_drop)cross_thread_probe(test);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    assert(test->refs==2);--test->refs;++test->dropped;assert(xrtOwnershipScopeEnd(&mutation));
}
static const xcancelwatchownershipv1 policy={sizeof(policy),notify,drop,&data_ops};
static void finish(BridgeCase* test,xfuture* future)
{
    xrtFutureBridgeUnwatch(&test->bridge);
    xpromise* promise=xrtFutureBridgePromise(&test->bridge);
    (void)xrtPromiseCancel(promise);xrtPromiseDestroy(promise);xrtFutureDestroy(future);
    assert(test->refs==1&&test->dropped==1);test->refs=0;
}
static void synchronous_case(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    BridgeCase test={0};test.refs=1;test.reenter=true;
    xcancel* parent=xrtCancelCreate();assert(parent&&xrtCancelRequest(parent));
    xfuture* future=xrtFutureBridgeCreate(&test.bridge,parent);assert(future);
    ++test.refs;assert(xrtFutureBridgeWatchOwnedV1(&test.bridge,&test,&policy));assert(test.notified==1);
    assert(xrtFutureBridgeReady(&test.bridge));
    xrtownershipscope freeze={0};freeze_begin(&freeze);xrtownershipref ref={0};
    assert(xrtFutureBridgeWatchOwnershipV1(&test.bridge,&ref)&&ref.Data);
    const xcancelwatchownershipv1* known=&policy;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xrtCancelWatchOwnershipAdapterV1(ref,&known,1,&preparation);
    assert(adapter&&preparation);
    /* Unknown-but-byte-identical descriptors are not lifecycle authority. */
    xcancelwatchownershipv1 copy=policy;const xcancelwatchownershipv1* wrong=&copy;
    const xrtownershippreparationv1* marker=preparation;
    assert(!xrtCancelWatchOwnershipAdapterV1(ref,&wrong,1,&preparation)&&preparation==marker);
    assert(xrtOwnershipScopeEnd(&freeze));test.inspect_during_drop=true;
    finish(&test,future);xrtCancelDestroy(parent);balanced(&before);++synchronous;
}
static void pinned_watch(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    BridgeCase test={0};test.refs=1;xfuture* future=xrtFutureBridgeCreate(&test.bridge,NULL);assert(future);
    ++test.refs;assert(xrtFutureBridgeWatchOwnedV1(&test.bridge,&test,&policy));
    assert(xrtFutureBridgeReady(&test.bridge));xrtownershipscope freeze={0};freeze_begin(&freeze);
    xrtownershipref ref={0};assert(xrtFutureBridgeWatchOwnershipV1(&test.bridge,&ref)&&ref.Data);
    const xcancelwatchownershipv1* known=&policy;const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=xrtCancelWatchOwnershipAdapterV1(ref,&known,1,&preparation);
    assert(adapter&&adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&test));
    assert(xrtOwnershipScopeEnd(&freeze));assert(preparation->Prepare(ref.Data,&test)==XRT_OWNERSHIP_PREPARE_BUSY);
    xrtFutureBridgeUnwatch(&test.bridge);assert(!test.dropped&&test.refs==2);
    freeze_begin(&freeze);xrtownershipref empty={&test,&data_ops};
    assert(xrtFutureBridgeWatchOwnershipV1(&test.bridge,&empty)&&!empty.Data&&!empty.Ops);
    assert(preparation->Ready(ref.Data));adapter->Clear(ref.Data,&test);assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(ref.Data,&test)&&adapter->Finish(ref.Data,&test));adapter->Drop(ref.Data);
    finish(&test,future);balanced(&before);++pins;
}
static int32 wait_and_unlink(ptr data)
{
    BridgeCase* test=data;(void)xrtFutureBridgeWait(&test->bridge);
    xrtFutureBridgeUnwatch(&test->bridge);assert(!xrtGetError());
    return 0;
}
static void publish_races(void)
{
    for(unsigned i=0;i<256;++i){
        xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        BridgeCase test={0};test.refs=1;xfuture* future=xrtFutureBridgeCreate(&test.bridge,NULL);assert(future);
        ++test.refs;assert(xrtFutureBridgeWatchOwnedV1(&test.bridge,&test,&policy));
        xthread* thread=xrtThreadCreate(wait_and_unlink,&test,0);assert(thread);
        assert((i&1)?xrtFutureBridgeFail(&test.bridge):xrtFutureBridgeReady(&test.bridge));
        assert(xrtThreadWaitFor(thread,5000000)==XWAIT_OK);xrtThreadDestroy(thread);
        assert(test.dropped==1&&test.refs==1);finish(&test,future);balanced(&before);++races;
    }
}
static void legacy_refusal(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    BridgeCase test={0};test.refs=1;xfuture* future=xrtFutureBridgeCreate(&test.bridge,NULL);assert(future);
    assert(xrtFutureBridgeWatch(&test.bridge,notify,&test)&&xrtFutureBridgeReady(&test.bridge));
    xrtownershipscope freeze={0};freeze_begin(&freeze);xrtownershipref ref={&test,&data_ops};
    assert(!xrtFutureBridgeWatchOwnershipV1(&test.bridge,&ref)&&ref.Data==&test&&ref.Ops==&data_ops);
    assert(xrtOwnershipScopeEnd(&freeze));xrtFutureBridgeUnwatch(&test.bridge);
    xpromise* promise=xrtFutureBridgePromise(&test.bridge);xrtPromiseDestroy(promise);xrtFutureDestroy(future);
    assert(!test.notified&&!test.dropped&&test.refs==1);balanced(&before);
}
int main(int argc,char** argv)
{
    const char* selected=argc>1?argv[1]:NULL;
    for(unsigned i=0;i<8;++i){
        if(!selected||!strcmp(selected,"sync"))synchronous_case();
        if(!selected||!strcmp(selected,"pin"))pinned_watch();
        if(!selected||!strcmp(selected,"legacy"))legacy_refusal();
    }
    if(!selected||!strcmp(selected,"race"))publish_races();
    testMemoryDebugDrain("Future bridge owned balance");
    printf("Future bridge owned: synchronous=%u pinned=%u publish-races=%u\n",synchronous,pins,races);return 0;
}
