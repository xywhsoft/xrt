#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

typedef struct endpoint_fault {
	xnetengine* Engine;
	xatomic32 Done, Failure;
	unsigned int Budget, Mode, Opens, Closes, PeerCloses;
	unsigned int ExpectedPeers;
	bool DialFault;
	xrudpendpoint* Endpoint;
} endpoint_fault;

static void expect(endpoint_fault* c, bool condition, const char* message)
{
	if (!condition) { fprintf(stderr, "endpoint fault: %s (budget=%u mode=%u)\n", message, c->Budget, c->Mode); xrtAtomic32Store(&c->Failure, 1, XMEMORY_RELEASE); }
}
static void peer_closed(xrudpendpoint* e, xrudppeer* p, xrudpresult result, void* context)
{
	endpoint_fault* c = (endpoint_fault*)context; (void)e; (void)p;
	expect(c, (result == XRUDP_CANCELLED || (c->Mode == 2 && result == XRUDP_NO_MEMORY)) && ++c->PeerCloses == 1, "peer terminal result mismatch");
}
static void closed(xrudpendpoint* e, xrudpresult result, const xerror* error, void* context)
{
	endpoint_fault* c = (endpoint_fault*)context; (void)error;
	expect(c, (result == XRUDP_CANCELLED || (c->Mode == 2 && c->DialFault && result == XRUDP_NO_MEMORY && error != NULL)) &&
		++c->Closes == 1, "endpoint terminal result mismatch");
	expect(c, !xrtMemDebugFailTriggered(), "close/cancel allocated while heap failure was armed");
	xrtMemDebugFailClear(); xrtRudpEndpointDestroy(e); c->Endpoint = NULL;
	xrtClearError(); xrtAtomic32Store(&c->Done, 1, XMEMORY_RELEASE);
}
static void opened(xrudpendpoint* e, void* context)
{
	endpoint_fault* c = (endpoint_fault*)context; ++c->Opens;
	if (c->Mode != 0) {
		xrudpendpointinfo info; xrudppeer* p = NULL;
		expect(c, xrtRudpEndpointInfo(e, &info) == XRUDP_OK, "identity query failed");
		xrudpconnectconfig dial = {info.Public, info.Scope, NULL};
		/* Dial this non-listening socket, keeping all test traffic self-contained. */
		if (c->Mode == 2) expect(c, xrtMemDebugFailAfter(c->Budget), "arm dial failure failed");
		xrudpresult result = xrtRudpEndpointConnect(e, &info.Local, &dial, &p);
		if (c->Mode == 2) { c->DialFault = xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); }
		expect(c, result == XRUDP_OK || (c->Mode == 2 && c->DialFault && result == XRUDP_NO_MEMORY && p == NULL), "pre-close negotiation failed");
		c->ExpectedPeers = result == XRUDP_OK ? 1u : 0u;
		xrtRudpPeerDestroy(p);
	}
	expect(c, xrtMemDebugFailAfter(0), "arm close failure failed");
	expect(c, xrtRudpEndpointClose(e) == XRUDP_OK, "allocation-free close rejected");
}
static void create_endpoint(xnetworker* worker, ptr context)
{
	endpoint_fault* c = (endpoint_fault*)context;
	unsigned char privateKey[32] = {7}; xrudpendpointconfig cfg;
	xrtRudpEndpointConfigInit(&cfg); cfg.StaticPrivate = privateKey; cfg.MaxPeers = cfg.MaxHalfOpen = 2;
	xnetaddr local; expect(c, xrtNetAddrLoopback(&local, XNET_FAMILY_IPV4, 0), "loopback failed");
	xrudpendpointevents events = {opened, NULL, peer_closed, closed};
	expect(c, xrtMemDebugFailAfter(c->Mode == 2 ? 1000 : c->Budget), "arm constructor failure failed");
	xrudpresult result = xrtRudpEndpointBind(worker, &local, &cfg, &events, c, &c->Endpoint);
	bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
	if (result != XRUDP_OK) {
		expect(c, result == XRUDP_NO_MEMORY && triggered && c->Endpoint == NULL, "constructor failure misclassified or leaked handle");
		xrtClearError(); xrtAtomic32Store(&c->Done, 1, XMEMORY_RELEASE);
	}
}

static int one_case(unsigned int budget, unsigned int mode)
{
	endpoint_fault c; memset(&c, 0, sizeof(c)); c.Budget = budget; c.Mode = mode;
	xrtAtomic32Init(&c.Done, 0); xrtAtomic32Init(&c.Failure, 0);
	xnetengineconfig cfg; xrtNetEngineConfigInit(&cfg); cfg.Backend = XNET_PORT_SELECT; cfg.Workers = 1;
	c.Engine = xrtNetEngineCreate(&cfg); CHECK(c.Engine != NULL && xrtNetEngineStart(c.Engine));
	CHECK(xrtNetEnginePost(c.Engine, 0, create_endpoint, &c));
	double deadline = xrtTimer() + 5;
	while (!xrtAtomic32Load(&c.Done, XMEMORY_ACQUIRE)) {
		CHECK(!xrtAtomic32Load(&c.Failure, XMEMORY_ACQUIRE) && xrtTimer() < deadline); xrtThreadYield();
	}
	CHECK(xrtNetEngineStop(c.Engine) && xrtNetEngineDestroy(c.Engine));
	CHECK(!xrtAtomic32Load(&c.Failure, XMEMORY_ACQUIRE));
	CHECK(c.Opens == c.Closes && c.Endpoint == NULL);
	if (c.Opens) CHECK(c.PeerCloses == c.ExpectedPeers);
	xrtClearError(); return 0;
}
int main(void)
{
	CHECK(one_case(1000, 0) == 0); /* Warm process-global network/TLS facilities. */
	xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before); CHECK(before.Enabled);
	for (unsigned int budget = 0; budget < 20; ++budget) {
		CHECK(one_case(budget, 0) == 0); CHECK(one_case(budget, 1) == 0);
		CHECK(one_case(budget, 2) == 0);
		xrtMemDebugSnapshot(&after);
		CHECK(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
		CHECK(after.DoubleFreeCount == before.DoubleFreeCount && after.UseAfterFreeCount == before.UseAfterFreeCount);
	}
	puts("xrudp native endpoint faults: 60 constructor/dial/close schedules, allocation-free cancellation and balanced object lifetimes passed");
	return 0;
}
