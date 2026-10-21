#include "../test.h"
#include <xrt/detail/wait.h>
#include <xrt/future_bridge.h>

/* Exercise the actual shared producer before constructor publication.
 * This is a resident implementation seam, not a fabricated native graph. */
static xatomic32 Entered;
static unsigned Released;
static uint8 DialMarker;
static bool testDialBridgeWait(const xfuturebridge* bridge)
{
    xrtAtomic32Store(&Entered, 1, XMEMORY_RELEASE);
    return xrtFutureBridgeWait(bridge);
}
static void testDialRelease(ptr operation)
{
    if (!operation) return;
    testRequire(operation == &DialMarker, "released an unpublished operation");
    ++Released;
}

#define xrtFutureBridgeWait testDialBridgeWait
#define __xrtNetStreamDialFutureOps testPublishedDialOps
#define __xrtNetStreamDialFutureDrop testPublishedDialDrop
#define __xrtNetStreamDialFutureComplete testPublishedDialComplete
#define __xrtNetStreamDialFutureCreate testPublishedDialCreate
#define __xrtNetStreamDialFutureProducerPolicyV1Get testPublishedDialProducer
#define __xrtNetStreamDialFutureCancelPolicyV1Get testPublishedDialCancel
#define __xrtNetStreamDialFuturePayloadPolicyV1Get testPublishedDialPayload
#define __xrtNetStreamDialFutureOwnershipQuiescenceV1Get testPublishedDialQuiet
#define __xrtNetStreamDialFutureOwnershipAdapterV1 testPublishedDialAdapter
#include "../../src/network/dial_future.c"
#undef __xrtNetStreamDialFutureOwnershipAdapterV1
#undef __xrtNetStreamDialFutureOwnershipQuiescenceV1Get
#undef __xrtNetStreamDialFuturePayloadPolicyV1Get
#undef __xrtNetStreamDialFutureCancelPolicyV1Get
#undef __xrtNetStreamDialFutureProducerPolicyV1Get
#undef __xrtNetStreamDialFutureCreate
#undef __xrtNetStreamDialFutureComplete
#undef __xrtNetStreamDialFutureDrop
#undef __xrtNetStreamDialFutureOps
#undef xrtFutureBridgeWait

static int32 testDialCompleteBeforePublish(ptr data)
{
    testPublishedDialComplete(data, XNET_RESULT_CLOSED, NULL, NULL);
    testPublishedDialDrop(data); /* actual owned Done-context callback tail */
    return 0;
}
int main(void)
{
    static const xrt_net_dial_future_policy policy = {NULL, testDialRelease, NULL, NULL};
    xrt_net_stream_dial_future* context = xrtCalloc(1, sizeof(*context));
    testRequire(context != NULL, "publication context allocation failed");
    xfuture* future = xrtFutureBridgeCreate(&context->Bridge, NULL);
    testRequire(future != NULL && xrtMutexInit(&context->Lock), "publication setup failed");
    context->References = 2; context->Publishing = true; context->Policy = &policy;
    context->Promise = xrtFutureBridgePromise(&context->Bridge);
    xrtAtomic32Init(&Entered, 0);
    xthread* thread = xrtThreadCreate(testDialCompleteBeforePublish, context, 0);
    testRequire(thread != NULL, "publication thread creation failed");
    double due = __xrtWaitAfter(5000);
    while (!xrtAtomic32Load(&Entered, XMEMORY_ACQUIRE)) {
        testRequire(!__xrtWaitExpired(due), "callback did not reach publication gate");
        xrtThreadYield();
    }
    xrtownershipscope scope = {0};
    __xrtNetStreamDialFutureBegin(context, &scope);
    context->Dial = &DialMarker; context->Publishing = false;
    __xrtNetStreamDialFutureEnd(context, &scope);
    testRequire(xrtFutureBridgeReady(&context->Bridge), "publication failed");
    testPublishedDialDrop(context);
    testRequire(xrtThreadWaitFor(thread, 5000) == XWAIT_OK, "publication callback did not finish");
    xrtThreadDestroy(thread);
    testRequire(Released == 1 && xrtFutureState(future) == XFUTURE_CLOSED,
        "completion lost the published operation or terminal result");
    xrtFutureDestroy(future); xrtClearError();
    puts("[PASS] shared Dial completion before setup publication releases its reference");
    return 0;
}
