#if !defined(TEST_TLS_DIAL_PUBLICATION)
#include "net_dial_future_publish.h"
#else
#include "../test.h"
#include <xrt/detail/wait.h>
#include <xrt/future_bridge.h>

/* Run the actual completion adapter before its caller publishes the Dial.
 * Only the Dial destructor is replaced, so no network timing is involved. */
static xatomic32 Entered;
static unsigned Released;
static uint8 DialMarker;

static bool testDialBridgeWait(const xfuturebridge* pBridge)
{
	xrtAtomic32Store(&Entered, 1u, XMEMORY_RELEASE);
	return xrtFutureBridgeWait(pBridge);
}

#if defined(TEST_TLS_DIAL_PUBLICATION)
static void testDialRelease(xtlsdial* pDial)
#else
static void testDialRelease(xnetdial* pDial)
#endif
{
	if ( pDial != NULL ) {
		testRequire((ptr)pDial == (ptr)&DialMarker,
			"completion released an unpublished Dial");
		Released++;
	}
}

#define xrtFutureBridgeWait testDialBridgeWait
#if defined(TEST_TLS_DIAL_PUBLICATION)
	#define xrtTlsDialDestroy testDialRelease
	#define xrtTlsDialAsync testPublishedTlsDialAsync
	#include "../../src/tls/stream_dial_future.c"
	#undef xrtTlsDialAsync
	#undef xrtTlsDialDestroy
	typedef xrt_tls_dial_future test_dial_context;
#else
	#define xrtNetDialDestroy testDialRelease
	#define xrtNetDialAsync testPublishedNetDialAsync
	#define xrtNetDialAddressesAsync testPublishedNetDialAddressesAsync
	#define xrtNetDialFutureProducerPolicyV1Get testPublishedDialProducer
	#define xrtNetDialFutureCancelPolicyV1Get testPublishedDialCancel
	#define xrtNetDialFutureDialPolicyV1Get testPublishedDialPolicy
	#define xrtNetDialFuturePayloadPolicyV1Get testPublishedDialPayload
	#define xrtNetDialFutureOwnershipQuiescenceV1Get testPublishedDialQuiescence
	#define xrtNetDialFutureOwnershipAdapterV1 testPublishedDialAdapter
	#include "../../src/network/tcp_dial_future.c"
	#undef xrtNetDialFutureOwnershipAdapterV1
	#undef xrtNetDialFutureOwnershipQuiescenceV1Get
	#undef xrtNetDialFuturePayloadPolicyV1Get
	#undef xrtNetDialFutureDialPolicyV1Get
	#undef xrtNetDialFutureCancelPolicyV1Get
	#undef xrtNetDialFutureProducerPolicyV1Get
	#undef xrtNetDialAddressesAsync
	#undef xrtNetDialAsync
	#undef xrtNetDialDestroy
	typedef xrt_net_dial_future test_dial_context;
#endif
#undef xrtFutureBridgeWait

static int32 testDialCompleteBeforePublish(ptr pData)
{
#if defined(TEST_TLS_DIAL_PUBLICATION)
	__xrtTlsDialFutureDone(NULL, XNET_RESULT_CLOSED, NULL, NULL, pData);
#else
	__xrtNetDialFutureDone(NULL, XNET_RESULT_CLOSED, NULL, NULL, pData);
	__xrtNetDialFutureDrop(pData); /* real owned Done-context reference, after its callback tail */
#endif
	return 0;
}

int main(void)
{
	test_dial_context* pContext = xrtCalloc(1u, sizeof(*pContext));
	xfuture* pFuture;
	xthread* pThread;
	double Limit = __xrtWaitAfter(INT64_C(5000));
	testRequire(pContext != NULL, "publication context allocation failed");
	pFuture = xrtFutureBridgeCreate(&pContext->Bridge, NULL);
	testRequire(pFuture != NULL, "publication bridge creation failed");
	pContext->References = 2;
	#if !defined(TEST_TLS_DIAL_PUBLICATION)
		testRequire(xrtMutexInit(&pContext->Lock), "publication metadata lock init failed");
		pContext->Promise=xrtFutureBridgePromise(&pContext->Bridge);
		pContext->Publishing=true;
	#endif
	xrtAtomic32Init(&Entered, 0u);
	pThread = xrtThreadCreate(testDialCompleteBeforePublish, pContext, 0u);
	testRequire(pThread != NULL, "publication callback thread creation failed");
	while ( !xrtAtomic32Load(&Entered, XMEMORY_ACQUIRE) ) {
		testRequire(!__xrtWaitExpired(Limit), "callback did not reach setup gate");
		xrtThreadYield();
	}
	/* The callback is already blocked at the setup gate with Dial still null. */
#if defined(TEST_TLS_DIAL_PUBLICATION)
	pContext->Dial = (xtlsdial*)&DialMarker;
#else
	xrtownershipscope Scope={0};
	__xrtNetDialFutureBegin(pContext,&Scope);
	pContext->Dial = (xnetdial*)&DialMarker;
	pContext->Publishing=false;
	__xrtNetDialFutureEnd(pContext,&Scope);
#endif
	testRequire(xrtFutureBridgeReady(&pContext->Bridge), "publication failed");
	#if defined(TEST_TLS_DIAL_PUBLICATION)
		if ( xrtRefRelease(&pContext->References) == 0 ) xrtFree(pContext);
	#else
		__xrtNetDialFutureDrop(pContext);
	#endif
	testRequire(xrtThreadWaitFor(pThread, INT64_C(5000)) == XWAIT_OK,
		"publication callback did not finish");
	xrtThreadDestroy(pThread);
	testRequire(Released == 1u, "completion leaked the published Dial reference");
	testRequire(xrtFutureState(pFuture) == XFUTURE_CLOSED,
		"publication callback lost the terminal result");
	xrtFutureDestroy(pFuture);
	xrtClearError();
	puts("[PASS] Dial completion before setup publication releases its reference");
	return 0;
}
#endif
