#ifndef XRT_MODULE_NET_ENGINE
#define XRT_MODULE_NET_ENGINE
#endif
#ifndef XRT_MODULE_NET_RESOLVER
#define XRT_MODULE_NET_RESOLVER
#endif
#ifndef XRT_MODULE_THREAD_KEY
#define XRT_MODULE_THREAD_KEY
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifdef NET_SERVICE_RETIREMENT_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#ifndef NET_SERVICE_RETIREMENT_SINGLE
#include "../../src/internal/xrt_net_engine.h"
#endif

typedef struct retirement {
	xnetengine* Engine;
	xnetresolver* Resolver;
	xthreadkey* Key;
	xnetbuf Buffer;
	xnetpost Post;
	xatomic32 Entered, Release, Done, Posts, Timers;
	xatomic32 TailEntered, TailRelease, TailDone;
	bool Self, BlockLookup, HoldBuffer;
} retirement;

static void wait_for(xatomic32* value, uint32 count)
{
	xdeadline deadline = xrtDeadlineAfter(5000000u);
	while ( xrtAtomic32Load(value, XMEMORY_ACQUIRE) < count ) {
		testRequire(!xrtDeadlineExpired(deadline), "retirement test deadline");
		xrtThreadYield();
	}
}

/* A real thread-key destructor proves join includes the post-callback TLS tail. */
static void thread_tail(ptr data)
{
	retirement* state = (retirement*)data;
	xrtAtomic32Store(&state->TailEntered, 1, XMEMORY_RELEASE);
	wait_for(&state->TailRelease, 1);
	xrtAtomic32Store(&state->TailDone, 1, XMEMORY_RELEASE);
}

static void finish_engine(xnetengine* engine)
{
	xdeadline deadline = xrtDeadlineAfter(5000000u);
	for ( ;; ) {
		xnetretireresult result = xrtNetEngineTryDestroy(engine);
		testRequire(result != XNET_RETIRE_ERROR, "engine retirement error");
		if ( result == XNET_RETIRE_READY ) return;
		testRequire(!xrtDeadlineExpired(deadline), "engine retirement deadline");
		xrtThreadYield();
	}
}

static void finish_resolver(xnetresolver* resolver)
{
	xdeadline deadline = xrtDeadlineAfter(5000000u);
	for ( ;; ) {
		xnetretireresult result = xrtNetResolverTryDestroy(resolver);
		testRequire(result != XNET_RETIRE_ERROR, "resolver retirement error");
		if ( result == XNET_RETIRE_READY ) return;
		testRequire(!xrtDeadlineExpired(deadline), "resolver retirement deadline");
		xrtThreadYield();
	}
}

static xnetengine* create_engine(bool start)
{
	xnetengineconfig config;
	xnetengine* engine;
	xrtNetEngineConfigInit(&config);
	config.Workers = 1;
	config.CommandCapacity = 32;
	engine = xrtNetEngineCreate(&config);
	testRequire(engine != NULL, "engine create");
	if ( start ) testRequire(xrtNetEngineStart(engine), "engine start");
	return engine;
}

static void engine_post(xnetworker* worker, ptr data)
{
	retirement* state = (retirement*)data;
	testRequire(xrtNetWorkerIsCurrent(worker), "post on actual worker");
	(void)xrtAtomic32FetchAdd(&state->Posts, 1, XMEMORY_RELEASE);
}

static void engine_timer(xnetworker* worker, uint64 id, xnetresult result, ptr data)
{
	retirement* state = (retirement*)data;
	testRequire(xrtNetWorkerIsCurrent(worker) && id != 0 && result == XNET_RESULT_CLOSED,
		"retirement closes accepted timer on its worker");
	(void)xrtAtomic32FetchAdd(&state->Timers, 1, XMEMORY_RELEASE);
}

static void engine_body(xnetworker* worker, ptr data)
{
	retirement* state = (retirement*)data;
	if ( state->Key ) testRequire(xrtThreadKeySet(state->Key, state), "install engine TLS tail");
	if ( state->HoldBuffer ) {
		testRequire(xrtNetBufInit(&state->Buffer, xrtNetWorkerBufPool(worker)), "buffer init");
		testRequire(xrtNetBufAppend(&state->Buffer, "held", 4), "real borrowed pool block");
	}
	if ( state->Self ) {
		testRequire(xrtNetEngineTryDestroy(state->Engine) == XNET_RETIRE_BUSY,
			"own worker retirement must not self-join");
		testRequire(xrtGetError() == NULL, "own worker BUSY is not an error");
	}
	xrtAtomic32Store(&state->Entered, 1, XMEMORY_RELEASE);
	wait_for(&state->Release, 1);
	xrtAtomic32Store(&state->Done, 1, XMEMORY_RELEASE);
}

static xerror* diagnostic(void)
{
	xerror* error = xrtErrorCreate(XERR_STATE, "test.retirement", 42, "existing caller error");
	testRequire(error != NULL, "diagnostic create");
	xrtSetError(error);
	return error;
}

static void engine_pinned(void)
{
	retirement state = {0};
	xerror* error;
	state.Engine = create_engine(true);
	testRequire(xrtNetEnginePin(state.Engine), "real engine pin");
	error = diagnostic();
	for ( unsigned i = 0; i < 64; ++i ) {
		testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "pin delays retirement");
		testRequire(xrtNetEngineState(state.Engine) == XNET_ENGINE_RUNNING, "pin preserves progress");
		testRequire(xrtGetError() == error, "BUSY preserves original diagnostic identity");
	}
	testRequire(xrtNetEnginePost(state.Engine, 0, engine_post, &state), "pinned service still accepts work");
	testRequire(xrtNetEngineUnpin(state.Engine), "unpin retained service");
	finish_engine(state.Engine);
	testRequire(xrtGetError() == error, "READY preserves original diagnostic identity");
	testRequire(xrtAtomic32Load(&state.Posts, XMEMORY_ACQUIRE) == 1, "accepted work drained");
	xrtClearError(); xrtErrorFree(error);
}

static void engine_active(bool self, bool synchronous_finish)
{
	retirement state = {0};
	state.Self = self;
	state.Key = xrtThreadKeyCreate(thread_tail);
	testRequire(state.Key != NULL, "engine tail key");
	state.Engine = create_engine(true);
	testRequire(xrtNetEnginePost(state.Engine, 0, engine_body, &state), "post blocking worker");
	wait_for(&state.Entered, 1);
	if ( !self ) {
		for ( unsigned i = 0; i < 16; ++i )
			testRequire(xrtNetEnginePost(state.Engine, 0, engine_post, &state), "accepted queued post");
		testRequire(xrtNetEngineAfter(state.Engine, 0, 60000000u, engine_timer, &state) != 0,
			"accepted future timer");
	}
	for ( unsigned i = 0; i < 64; ++i )
		testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "active callback is BUSY");
	testRequire(xrtGetError() == NULL, "callback BUSY has no diagnostic");
	testRequire(!xrtNetEnginePost(state.Engine, 0, engine_post, &state), "closing rejects fresh work");
	testRequire(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_CLOSED, "closing diagnostic");
	xrtClearError();
	xrtAtomic32Store(&state.Release, 1, XMEMORY_RELEASE);
	wait_for(&state.TailEntered, 1);
	for ( unsigned i = 0; i < 64; ++i )
		testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "TLS tail prevents early READY");
	xrtAtomic32Store(&state.TailRelease, 1, XMEMORY_RELEASE);
	if ( synchronous_finish ) testRequire(xrtNetEngineDestroy(state.Engine), "blocking finish after TryDestroy");
	else finish_engine(state.Engine);
	testRequire(xrtAtomic32Load(&state.TailDone, XMEMORY_ACQUIRE) == 1, "engine TLS tail joined");
	testRequire(xrtAtomic32Load(&state.Posts, XMEMORY_ACQUIRE) == (self ? 0u : 16u), "queued posts drained once");
	testRequire(xrtAtomic32Load(&state.Timers, XMEMORY_ACQUIRE) == (self ? 0u : 1u), "accepted timer closed once");
	testRequire(xrtThreadKeyDestroy(state.Key), "engine tail key close");
}

static void engine_pool(void)
{
	retirement state = {0};
	xthread* worker_thread;
	state.HoldBuffer = true;
	state.Engine = create_engine(true);
	worker_thread = xrtThreadRef(state.Engine->Workers[0].Thread);
	testRequire(worker_thread != NULL, "observe actual worker join");
	testRequire(xrtNetEnginePost(state.Engine, 0, engine_body, &state), "borrow pool block on worker");
	wait_for(&state.Entered, 1);
	xrtAtomic32Store(&state.Release, 1, XMEMORY_RELEASE);
	testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "retire with borrowed block");
	testRequire(xrtThreadWait(worker_thread) == XWAIT_OK, "worker actually joined before pool check");
	for ( unsigned i = 0; i < 64; ++i )
		testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "joined worker still has borrowed pool block");
	testRequire(xrtGetError() == NULL, "pool BUSY does not create error");
	xrtNetBufClear(&state.Buffer);
	finish_engine(state.Engine);
	xrtThreadDestroy(worker_thread);
}

/* Exercise a real bounded-shutdown failure, not a fabricated state or handle. */
static void engine_nonconvergent(xnetworker* worker, ptr data)
{
	retirement* state = (retirement*)data;
	if ( xrtAtomic32FetchAdd(&state->Posts, 1, XMEMORY_ACQ_REL) == 0 ) {
		xrtAtomic32Store(&state->Entered, 1, XMEMORY_RELEASE);
		wait_for(&state->Release, 1);
	}
	if ( !xrtNetPost(worker, &state->Post, engine_nonconvergent, state) ) {
		testRequire(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_CLOSED,
			"nonconvergent post chain is explicitly sealed");
		xrtClearError();
		xrtAtomic32Store(&state->Done, 1, XMEMORY_RELEASE);
	}
}

static void engine_error_retry(void)
{
	retirement state = {0};
	xnetenginestats stats;
	xnetretireresult result;
	xdeadline deadline = xrtDeadlineAfter(5000000u);
	const xerror* error;
	state.Engine = create_engine(true);
	testRequire(xrtNetPostInit(&state.Post), "nonconvergent post init");
	testRequire(xrtNetPost(xrtNetEngineWorker(state.Engine, 0), &state.Post, engine_nonconvergent, &state),
		"start actual nonconvergent chain");
	wait_for(&state.Entered, 1);
	testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_BUSY, "blocked chain BUSY");
	xrtAtomic32Store(&state.Release, 1, XMEMORY_RELEASE);
	do {
		result = xrtNetEngineTryDestroy(state.Engine);
		testRequire(!xrtDeadlineExpired(deadline), "nonconvergent retirement deadline");
		if ( result == XNET_RETIRE_BUSY ) xrtThreadYield();
	} while ( result == XNET_RETIRE_BUSY );
	error = xrtGetError();
	testRequire(result == XNET_RETIRE_ERROR && error != NULL &&
		xrtErrorCode(error) == XNET_ERROR_ENGINE_STOP, "actual shutdown failure stays ERROR");
	testRequire(xrtNetEngineState(state.Engine) == XNET_ENGINE_DESTROYING &&
		xrtNetEngineStats(state.Engine, &stats) && stats.ShutdownStalls == 1 && stats.PendingCommands == 0,
		"ERROR retains actual engine and drained statistics");
	testRequire(!xrtNetPostPending(&state.Post) && xrtAtomic32Load(&state.Done, XMEMORY_ACQUIRE) == 1,
		"shutdown ERROR has sealed and drained accepted work");
	testRequire(xrtNetEngineTryDestroy(state.Engine) == XNET_RETIRE_READY, "retry consumes retained owner exactly once");
	testRequire(xrtGetError() == error, "successful physical retry does not erase semantic failure");
	xrtClearError();
}

static xnetaddrlist* resolver_lookup(cstr host, xnetfamily family, ptr data)
{
	retirement* state = (retirement*)data;
	xnetaddr address;
	(void)host; (void)family;
	if ( state->BlockLookup ) {
		xrtAtomic32Store(&state->Entered, 1, XMEMORY_RELEASE);
		wait_for(&state->Release, 1);
	}
	testRequire(xrtNetAddrLoopback(&address, XNET_FAMILY_IPV4, 0), "lookup loopback address");
	return xrtNetAddrListCreate(&address, 1);
}

static void resolver_done(xnetresolveop* operation, ptr data)
{
	retirement* state = (retirement*)data;
	testRequire(xrtNetResolveOpState(operation) == XNET_RESOLVE_RESOLVED, "accepted query resolved");
	if ( state->Key ) testRequire(xrtThreadKeySet(state->Key, state), "install resolver TLS tail");
	if ( state->Self ) {
		testRequire(xrtNetResolverTryDestroy(state->Resolver) == XNET_RETIRE_BUSY,
			"own resolver worker must not self-join or lose owner");
		testRequire(xrtGetError() == NULL, "own resolver BUSY is not an error");
		xrtAtomic32Store(&state->Entered, 1, XMEMORY_RELEASE);
		wait_for(&state->Release, 1);
	}
	(void)xrtAtomic32FetchAdd(&state->Done, 1, XMEMORY_RELEASE);
}

static void resolver_active(bool self, bool synchronous_finish)
{
	retirement state = {0};
	xnetresolverconfig config;
	xnetresolveop* operations[2];
	unsigned count = self ? 1 : 2;
	state.Self = self; state.BlockLookup = !self;
	/* For two callbacks, use no TLS replacement (which would itself run the destructor). */
	if ( self ) { state.Key = xrtThreadKeyCreate(thread_tail); testRequire(state.Key != NULL, "resolver tail key"); }
	xrtNetResolverConfigInit(&config);
	config.Workers = 1; config.Lookup = resolver_lookup; config.LookupData = &state;
	state.Resolver = xrtNetResolverCreate(&config);
	testRequire(state.Resolver != NULL, "resolver create");
	operations[0] = xrtNetResolverResolve(state.Resolver, "first.test", XNET_FAMILY_IPV4, resolver_done, &state);
	testRequire(operations[0] != NULL, "first accepted query");
	wait_for(&state.Entered, 1);
	if ( !self ) {
		operations[1] = xrtNetResolverResolve(state.Resolver, "second.test", XNET_FAMILY_IPV4, resolver_done, &state);
		testRequire(operations[1] != NULL, "second queued query");
	}
	testRequire(xrtMemDebugFailAfter(0), "BUSY allocation budget");
	for ( unsigned i = 0; i < 64; ++i )
		testRequire(xrtNetResolverTryDestroy(state.Resolver) == XNET_RETIRE_BUSY, "lookup or callback BUSY");
	testRequire(!xrtMemDebugFailTriggered(), "BUSY requires no allocation");
	xrtMemDebugFailClear();
	testRequire(xrtGetError() == NULL, "resolver BUSY has no diagnostic");
	testRequire(xrtNetResolverResolve(state.Resolver, "rejected.test", XNET_FAMILY_IPV4, resolver_done, &state) == NULL,
		"closing resolver rejects new query");
	testRequire(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_CLOSED, "resolver closed diagnostic");
	xrtClearError();
	xrtAtomic32Store(&state.Release, 1, XMEMORY_RELEASE);
	if ( self ) {
		wait_for(&state.TailEntered, 1);
		for ( unsigned i = 0; i < 64; ++i )
			testRequire(xrtNetResolverTryDestroy(state.Resolver) == XNET_RETIRE_BUSY, "resolver TLS tail is BUSY");
		xrtAtomic32Store(&state.TailRelease, 1, XMEMORY_RELEASE);
	}
	if ( synchronous_finish ) testRequire(xrtNetResolverDestroy(state.Resolver), "blocking resolver completes started retirement");
	else finish_resolver(state.Resolver);
	testRequire(xrtAtomic32Load(&state.Done, XMEMORY_ACQUIRE) == count, "all accepted queries drain exactly once");
	for ( unsigned i = 0; i < count; ++i ) {
		xnetaddrlist* result = xrtNetResolveOpResult(operations[i]);
		testRequire(result != NULL && xrtNetAddrListCount(result) == 1, "operation and result outlive service owner");
		xrtNetAddrListDestroy(result); xrtNetResolveOpDestroy(operations[i]);
	}
	if ( self ) {
		testRequire(xrtAtomic32Load(&state.TailDone, XMEMORY_ACQUIRE) == 1, "resolver TLS tail joined");
		testRequire(xrtThreadKeyDestroy(state.Key), "resolver tail key close");
	}
}

static void idle(void)
{
	xnetengine* engine = create_engine(false);
	xnetresolverconfig config;
	xnetresolver* resolver;
	xerror* error = diagnostic();
	testRequire(xrtNetEngineTryDestroy(NULL) == XNET_RETIRE_READY, "null engine is READY");
	testRequire(xrtNetResolverTryDestroy(NULL) == XNET_RETIRE_READY, "null resolver is READY");
	testRequire(xrtNetEngineTryDestroy(engine) == XNET_RETIRE_READY, "stopped engine ready immediately");
	testRequire(xrtGetError() == error, "stopped READY preserves diagnostic");
	xrtClearError(); xrtErrorFree(error);
	engine = create_engine(true);
	xrtNetResolverConfigInit(&config); config.Workers = 1;
	resolver = xrtNetResolverCreate(&config);
	testRequire(resolver != NULL, "idle resolver create");
	error = diagnostic();
	testRequire(xrtMemDebugFailAfter(0), "retirement allocation budget");
	finish_engine(engine); finish_resolver(resolver);
	testRequire(!xrtMemDebugFailTriggered(), "idle retirement requires no allocation");
	xrtMemDebugFailClear();
	testRequire(xrtGetError() == error, "idle READY preserves diagnostic");
	xrtClearError(); xrtErrorFree(error);
}

static void balanced(const xmemdebugsnapshot* before)
{
	xmemdebugsnapshot after;
	xrtClearError();
	xrtMemDebugSnapshot(&after);
	testRequire(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes &&
		after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
		after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount &&
		after.UseAfterFreeCount == before->UseAfterFreeCount && after.OverflowCount == before->OverflowCount &&
		after.UnderflowCount == before->UnderflowCount, "exact service memory and invalid access balance");
}

int main(void)
{
	xmemdebugsnapshot before;
	testRequire(xrtMemDebugEnable(true), "enable exact memory instrumentation on every compiler");
	xrtMemDebugSnapshot(&before);
	for ( unsigned round = 0; round < 24; ++round ) {
		idle();
		engine_pinned();
		engine_pool();
		engine_error_retry();
		for ( unsigned self = 0; self < 2; ++self ) {
			engine_active(self != 0, (round & 1) != 0);
			resolver_active(self != 0, (round & 1) != 0);
		}
	}
	xrtClearError();
	testRequire(xrtRuntimeRetireThreadStorage(), "terminal thread storage retirement after all services");
	balanced(&before);
	printf("[PASS] service retirement: 240 service owners; 96 callback drains; 72 real TLS tails; "
		"24 external pins; 24 borrowed pools; blocked lookup, accepted queues, diagnostic identity, "
		"24 actual shutdown failures and retained-owner retries, allocation-free retries, "
		"blocking completion, independent operation results, exact memory\n");
	testMemoryDebugDrain("service retirement memory drain");
	return 0;
}
