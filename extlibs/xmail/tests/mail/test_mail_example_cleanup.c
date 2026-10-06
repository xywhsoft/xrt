#include <xrt/detail/wait.h>
#include "../test.h"
#include "../../examples/mail_client_setup.h"

static void requireClean(void)
{
	xmemdebugsnapshot memory;
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	testRequire(memory.LiveCount == 0u && memory.LiveBytes == 0u &&
		memory.InvalidFreeCount == 0u && memory.DoubleFreeCount == 0u,
		"mail example cleanup left live allocations or invalid frees");
}

static void busyCleanup(bool primary)
{
	mail_example_net net = {0};
	xnetengineconfig config;
	const xerror* cause = NULL;
	xrtNetEngineConfigInit(&config);
	config.Workers = 1u;
	net.Engine = xrtNetEngineCreate(&config);
	testRequire(net.Engine != NULL && xrtNetEngineStart(net.Engine) &&
		xrtNetEnginePin(net.Engine), "mail example test engine setup failed");
	net.Tls = xrtTlsContextCreate(NULL);
	testRequire(net.Tls != NULL, "mail example test TLS setup failed");
	xnetengine* engine = net.Engine;
	xtlscontext* tls = net.Tls;
	if ( primary ) {
		xerror* error = xrtErrorCreate(XERR_PROTOCOL, "mail-example-test", 700, "original protocol diagnostic");
		testRequire(error != NULL, "mail example test error setup failed");
		xrtSetErrorTake(error);
		cause = xrtGetError();
	}
	testRequire(!mailExampleNetCleanup(&net, __xrtWaitAfter(0)),
		"mail example cleanup reported success with a live engine pin");
	testRequire(net.Engine == engine && net.Tls == tls,
		"mail example cleanup discarded retained resource owners");
	if ( primary ) testRequire(xrtGetError() == cause,
		"mail example cleanup replaced the primary protocol diagnostic");
	else {
		testRequire(xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
			"mail example cleanup timeout has no diagnostic");
		cause = xrtGetError();
	}
	testRequire(xrtNetEngineUnpin(engine), "mail example test unpin failed");
	testRequire(mailExampleNetUnit(&net), "mail example retained cleanup retry failed");
	testRequire(net.Engine == NULL && net.Resolver == NULL && net.Tls == NULL && net.Verifier == NULL,
		"mail example cleanup retry retained released resource pointers");
	testRequire(xrtGetError() == cause, "mail example successful retry replaced original diagnostic");
	testRequire(mailExampleNetUnit(&net) && xrtGetError() == cause,
		"mail example repeated cleanup changed the primary diagnostic");
	requireClean();
}

typedef struct resolverGate {
	xatomic32 Entered;
	xatomic32 Release;
} resolverGate;

static xnetaddrlist* blockedLookup(cstr host, xnetfamily family, ptr data)
{
	resolverGate* gate = data;
	double limit = __xrtWaitAfter(INT64_C(3000));
	(void)host;
	(void)family;
	xrtAtomic32Store(&gate->Entered, 1u, XMEMORY_RELEASE);
	while ( !xrtAtomic32Load(&gate->Release, XMEMORY_ACQUIRE) && !__xrtWaitExpired(limit) ) xrtSleep(1u);
	xerror* error = xrtErrorCreate(XERR_CANCELLED, "mail-example-test", 701, "controlled lookup completion");
	if ( error != NULL ) xrtSetErrorTake(error);
	return NULL;
}

static void busyResolverCleanup(void)
{
	mail_example_net net = {0};
	resolverGate gate = {0};
	xnetresolverconfig config;
	xrtNetResolverConfigInit(&config);
	config.Workers = 1u;
	config.Lookup = blockedLookup;
	config.LookupData = &gate;
	net.Engine = xrtNetEngineCreate(NULL);
	testRequire(net.Engine != NULL && xrtNetEngineStart(net.Engine), "mail example resolver engine setup failed");
	net.Resolver = xrtNetResolverCreate(&config);
	testRequire(net.Resolver != NULL, "mail example blocked resolver setup failed");
	xnetresolver* resolver = net.Resolver;
	xnetengine* engine = net.Engine;
	xnetresolveop* operation = xrtNetResolverResolve(resolver, "blocked.test", XNET_FAMILY_IPV4, NULL, NULL);
	testRequire(operation != NULL, "mail example blocked lookup submit failed");
	double limit = __xrtWaitAfter(INT64_C(3000));
	while ( !xrtAtomic32Load(&gate.Entered, XMEMORY_ACQUIRE) ) {
		testRequire(!__xrtWaitExpired(limit), "mail example lookup never entered");
		xrtSleep(1u);
	}
	testRequire(!mailExampleNetCleanup(&net, __xrtWaitAfter(0)),
		"mail example cleanup waited through an active resolver query");
	testRequire(net.Resolver == resolver && net.Engine == engine &&
		xrtNetEngineState(engine) == XNET_ENGINE_RUNNING && xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
		"mail example busy resolver lost owners, stopped engine or omitted timeout");
	xrtAtomic32Store(&gate.Release, 1u, XMEMORY_RELEASE);
	testRequire(mailExampleNetUnit(&net), "mail example blocked resolver retry failed");
	testRequire(net.Resolver == NULL && net.Engine == NULL, "mail example resolver retry did not consume owners");
	xrtNetResolveOpDestroy(operation);
	requireClean();
}

int main(void)
{
	mail_example_net net = {0};
	uint16 port = 0u;
	(void)mailExampleNetInit;
	(void)mailExampleDiagnostic;
	testRequire(mailExamplePort("993", &port) && port == 993u, "mail example port setup failed");
	testRequire(xrtMemDebugEnable(true), "mail example memory debug setup failed");
	busyCleanup(true);
	busyCleanup(false);
	busyResolverCleanup();
	net.Engine = xrtNetEngineCreate(NULL);
	net.Resolver = xrtNetResolverCreate(NULL);
	testRequire(net.Engine != NULL && net.Resolver != NULL, "mail example idle owners setup failed");
	testRequire(mailExampleNetUnit(&net), "mail example idle owner cleanup failed");
	testRequire(net.Engine == NULL && net.Resolver == NULL, "mail example idle owners not consumed");
	testRequire(mailExampleNetCleanup(NULL, __xrtWaitAfter(0)), "mail example null cleanup failed");
	requireClean();
	puts("mail example busy retirement, retained owners, retry and diagnostics passed");
	return 0;
}
