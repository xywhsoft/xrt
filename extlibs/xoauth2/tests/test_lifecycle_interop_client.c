#include <xrt/detail/wait.h>
/* Actual core engines and pins; only startup/retirement faults and the rollback clock are controlled. */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "support/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TestCa;
static _Thread_local unsigned Fault, Starts, Retires;
static _Thread_local uint64_t RollbackBudget;
static _Thread_local bool ForceStartFailure, ConcurrentFactory, WrapOom;
static _Thread_local bool InjectRetirementError, SlowRetirement;
static _Thread_local xnetengine* LastEngine;
static _Thread_local xerror* StartError;
static xatomic32 FactoryBarrier = { 0u }, HoldRetirement = { 0u };

static bool start_engine(xnetengine* engine)
{
	Starts++;
	if (!xrtNetEngineStart(engine)) return false;
	if (!ForceStartFailure) return true;
	LastEngine = engine;
	if (Fault != 0u && !xrtNetEnginePin(engine)) return false;
	if (ConcurrentFactory) {
		double deadline = __xrtWaitAfter(5000);
		xrtAtomic32FetchAdd(&FactoryBarrier, 1u, XMEMORY_ACQ_REL);
		while (xrtAtomic32Load(&FactoryBarrier, XMEMORY_ACQUIRE) < 4u) {
			if (__xrtWaitExpired(deadline)) return false;
			xrtSleep(1u);
		}
	}
	xrtSetErrorInfo(XERR_STATE, "test", 1, "injected partial startup failure");
	if (WrapOom) {
		StartError = xrtErrorRef(xrtGetError());
	}
	return false;
}

static double rollback_deadline(uint64_t timeout)
{
	if (ForceStartFailure && timeout >= 30000000u) {
		RollbackBudget = timeout;
		if (Fault != 0u) timeout = 20000u;
	}
	return __xrtWaitAfter(timeout);
}

static xnetretireresult retire_engine(xnetengine* engine)
{
	uint32 expected = 1u;
	Retires++;
	if (xrtAtomic32CompareExchange(&HoldRetirement, &expected, 2u,
		XMEMORY_ACQ_REL, XMEMORY_RELAXED)) {
		double deadline = __xrtWaitAfter(5000);
		while (xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) == 2u) {
			if (__xrtWaitExpired(deadline)) {
				xrtSetErrorInfo(XERR_STATE, "test", 3, "retirement barrier timed out");
				return XNET_RETIRE_ERROR;
			}
			xrtSleep(1u);
		}
	}
	if (Fault == 1u || InjectRetirementError) {
		InjectRetirementError = false;
		xrtSetErrorInfo(XERR_STATE, "test", 2, "injected retirement failure");
		return XNET_RETIRE_ERROR;
	}
	if (SlowRetirement) xrtSleep(1u);
	return xrtNetEngineTryDestroy(engine);
}

static xerror* wrap_error(const xerror* cause, xerrkind kind, cstr domain, int32 code, cstr message)
{
	if (WrapOom) (void)xrtMemDebugFailAfter(0u);
	return xrtErrorWrap(cause, kind, domain, code, message);
}

#define xrtNetEngineStart start_engine
#define xrtNetEngineTryDestroy retire_engine
#define __xrtWaitAfter rollback_deadline
#define xrtErrorWrap wrap_error
#include "support/implementation.c"
#undef xrtNetEngineStart
#undef xrtNetEngineTryDestroy
#undef __xrtWaitAfter
#undef xrtErrorWrap

static bool memory_empty(void)
{
	xmemdebugsnapshot memory;
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	if (memory.LiveCount || memory.LiveBytes || memory.InvalidFreeCount || memory.DoubleFreeCount) {
		fprintf(stderr, "ownership memory live=%zu bytes=%zu invalid=%llu double=%llu\n",
			memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
			(unsigned long long)memory.DoubleFreeCount);
		return false;
	}
	return true;
}

static bool error_contains(const xerror* error, const char* message)
{
	for (; error != NULL; error = xrtErrorCause(error)) {
		const char* text = xrtErrorMessage(error);
		if (text != NULL && strcmp(text, message) == 0) return true;
	}
	return false;
}

typedef struct allocation_check { ptr address; bool live; } allocation_check;
static bool find_allocation(const xmemdebugallocation* allocation, ptr data)
{
	allocation_check* check = data;
	if (allocation->Address == check->address) check->live = true;
	return !check->live;
}
static bool allocation_live(ptr address)
{
	allocation_check check = {address, false};
	xrtMemDebugVisitLive(find_allocation, &check);
	return check.live;
}

static xnetengine* create_engine(void)
{
	xnetengineconfig config;
	xnetengine* engine;
	xrtNetEngineConfigInit(&config);
	engine = xrtNetEngineCreate(&config);
	if (engine == NULL || !xrtNetEngineStart(engine)) return NULL;
	return engine;
}

static bool factory_failures(void)
{
	for (unsigned stack = 0u; stack < 2u; stack++) {
	for (unsigned fault = 1u; fault <= 2u; fault++) {
		xoauth2httpxrt local, rejected;
		xerror* original;
		size_t pending = SIZE_MAX;
		bool ok;
		Fault = fault; ForceStartFailure = true; LastEngine = NULL; RollbackBudget = 0u;
		if (stack ? xoauth2HttpXrtInit(&local, NULL, TestCa, 1) :
			(xoauth2HttpXrtCreate(NULL, TestCa, 1) != NULL)) return false;
		ForceStartFailure = false;
		original = xrtErrorRef(xrtGetError());
		ok = LastEngine != NULL && original != NULL && RollbackBudget >= 30000000u &&
			xoauth2LastError() == XOAUTH2_ERROR_NETWORK &&
			error_contains(original, "injected partial startup failure") &&
			xrtNetEngineState(LastEngine) == XNET_ENGINE_RUNNING;
		ok = (xoauth2HttpXrtCleanupPending(0, &pending) == (stack != 0u)) && ok;
		ok = pending == (stack ? 0u : 1u) && xrtGetError() == original && ok;
		if (stack) {
			ok = local.pEngine == LastEngine && local.bEngineOwned && local.uTimeoutMs == 1u &&
				local.pResolver == NULL && local.pVerifier == NULL && ok;
			ok = !xoauth2HttpXrtCleanup(&local) && local.pEngine == LastEngine &&
				xrtGetError() == original && ok;
		} else {
			xoauth2httpxrt* owner = __xoauth2PendingHead;
			unsigned starts = Starts;
			xoauth2httpxrt* borrowed_http;
			xnetengine* borrowed;
			ok = owner != NULL && allocation_live(owner) && owner->pEngine == LastEngine &&
				owner->bEngineOwned && owner->pResolver == NULL && owner->pVerifier == NULL && ok;
			xrtClearError();
			ok = !xoauth2HttpXrtInit(&rejected, NULL, TestCa, 1) && ok;
			ok = rejected.pEngine == NULL && rejected.pResolver == NULL && rejected.pVerifier == NULL &&
				Starts == starts && strcmp(xrtErrorMessage(xrtGetError()),
				 "http pending cleanup must finish before creating a private engine") == 0 && ok;
			borrowed = create_engine();
			if (borrowed == NULL) return false;
			borrowed_http = xoauth2HttpXrtCreate(borrowed, TestCa, 1);
			if (borrowed_http == NULL || !xoauth2HttpXrtCleanup(borrowed_http)) return false;
			xoauth2HttpXrtDestroy(borrowed_http);
			ok = xrtNetEngineState(borrowed) == XNET_ENGINE_RUNNING && Starts == starts && ok;
			if (!xrtNetEngineDestroy(borrowed)) return false;
			xrtClearError();
			ok = !xoauth2HttpXrtCleanupPending(fault == 1u ? 5000u : 20u, &pending) &&
				pending == 1u && ok;
			ok = strcmp(xrtErrorMessage(xrtGetError()), fault == 1u ?
				"injected retirement failure" : "http pending cleanup still has live objects") == 0 && ok;
			if (fault == 2u) ok = xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
				xoauth2LastError() == XOAUTH2_ERROR_NETWORK && ok;
		}
		xrtSetError(original); Fault = 0u;
		if (!xrtNetEngineUnpin(LastEngine)) return false;
		if (stack) { local.uTimeoutMs = 5000u; ok = xoauth2HttpXrtCleanup(&local) && ok; }
		ok = xoauth2HttpXrtCleanupPending(5, &pending) && pending == 0u &&
			xrtGetError() == original && ok;
		xrtErrorFree(original);
		if (!memory_empty() || !ok) {
			fprintf(stderr, "failed constructor stack=%u fault=%u ownership/cause failed\n", stack, fault);
			return false;
		}
		printf("  failed constructor stack=%u fault=%u, cause, admission and retry: passed\n", stack, fault);
	}
	}
	return true;
}

static bool startup_ready_and_wrap_oom(void)
{
	size_t pending = SIZE_MAX;
	bool ok;
	ForceStartFailure = true; Fault = 0u;
	ok = xoauth2HttpXrtCreate(NULL, TestCa, 1) == NULL &&
		error_contains(xrtGetError(), "injected partial startup failure") &&
		xoauth2HttpXrtCleanupPending(0, &pending) && pending == 0u;
	ForceStartFailure = false;
	if (!memory_empty() || !ok) return false;
	puts("  partial startup failure with completed retirement: passed");
	ForceStartFailure = true; Fault = 1u; WrapOom = true; StartError = NULL;
	ok = xoauth2HttpXrtCreate(NULL, TestCa, 1) == NULL &&
		xrtMemDebugFailTriggered() && StartError != NULL && xrtGetError() == StartError;
	if (!ok) fprintf(stderr, "wrap OOM triggered=%d start=%p current=%p message=%s\n",
		(int)xrtMemDebugFailTriggered(), (void*)StartError, (void*)xrtGetError(),
		xrtErrorMessage(xrtGetError()));
	xrtMemDebugFailClear(); ForceStartFailure = false; WrapOom = false; Fault = 0u;
	if (LastEngine == NULL || !xrtNetEngineUnpin(LastEngine)) return false;
	ok = xoauth2HttpXrtCleanupPending(5, &pending) && pending == 0u &&
		xrtGetError() == StartError && ok;
	xrtErrorFree(StartError); StartError = NULL;
	if (!memory_empty() || !ok) { fprintf(stderr, "wrap OOM retirement pending=%zu ready=%d\n", pending, (int)ok); return false; }
	puts("  startup cause retained when error wrapping runs out of memory: passed");
	return true;
}

static xoauth2httpxrt* pinned_http(void)
{
	xoauth2httpxrt* http = xoauth2HttpXrtCreate(NULL, TestCa, 1);
	if (http == NULL || !xrtNetEnginePin(http->pEngine)) return NULL;
	return http;
}

static bool published_owner_and_no_allocation(void)
{
	xoauth2httpxrt* http = pinned_http();
	xnetengine* engine;
	xerror* original;
	size_t pending = SIZE_MAX;
	bool ok;
	if (http == NULL) return false;
	engine = http->pEngine;
	xrtSetErrorInfo(XERR_PROTOCOL, "test", 4, "original operation error");
	original = xrtErrorRef(xrtGetError());
	ok = !xoauth2HttpXrtCleanup(http) && xoauth2HttpXrtCleanupPending(0, &pending) && pending == 0u;
	xoauth2HttpXrtDestroy(http);
	ok = allocation_live(http) && xrtGetError() == original && ok;
	if (!xrtNetEngineUnpin(engine)) return false;
	http->uTimeoutMs = 5000u;
	ok = xoauth2HttpXrtCleanup(http) && ok;
	xoauth2HttpXrtDestroy(http);
	xrtErrorFree(original);
	if (!memory_empty() || !ok) return false;
	puts("  published busy handle remains caller-owned, never queued: passed");
	http = pinned_http();
	if (http == NULL) return false;
	engine = http->pEngine;
	xrtSetErrorInfo(XERR_PROTOCOL, "test", 4, "original operation error");
	original = xrtErrorRef(xrtGetError());
	if (xoauth2HttpXrtCleanup(http) || !xrtMemDebugFailAfter(0u)) return false;
	http_defer_owner(http);
	ok = !xoauth2HttpXrtCleanupPending(0, &pending) && pending == 1u &&
		!xrtMemDebugFailTriggered() && xrtGetError() == original;
	xrtMemDebugFailClear();
	if (!xrtNetEngineUnpin(engine)) return false;
	ok = xoauth2HttpXrtCleanupPending(5, &pending) && pending == 0u &&
		xrtGetError() == original && ok;
	xrtErrorFree(original);
	if (!memory_empty() || !ok) return false;
	puts("  unpublished handoff and nonblocking poll allocate nothing: passed");
	return true;
}

typedef struct cleanup_task {
	bool factory, loop, ready;
	size_t pending;
	xnetengine* engine;
} cleanup_task;
static int32 cleanup_thread(ptr data)
{
	cleanup_task* task = data;
	double deadline = __xrtWaitAfter(5000);
	if (task->factory) {
		ForceStartFailure = ConcurrentFactory = true; Fault = 1u;
		task->ready = xoauth2HttpXrtCreate(NULL, TestCa, 1) == NULL &&
			LastEngine != NULL && error_contains(xrtGetError(), "injected partial startup failure");
		task->engine = LastEngine;
		xrtClearError();
		return task->ready ? 0 : 1;
	}
	do {
		task->ready = xoauth2HttpXrtCleanupPending(0, &task->pending);
		if (!task->loop || task->ready) return 0;
		xrtSleep(1u);
	} while (!__xrtWaitExpired(deadline));
	return 1;
}

static bool join_thread(xthread* thread)
{
	bool ok = thread != NULL && xrtThreadWaitFor(thread, 8000) == XWAIT_OK;
	if (ok) xrtThreadDestroy(thread);
	return ok;
}

static bool claimed_owner(void)
{
	xoauth2httpxrt* first = pinned_http();
	xoauth2httpxrt* second = pinned_http();
	xnetengine *a, *b;
	cleanup_task task = {0};
	xthread* thread;
	double deadline;
	size_t pending = SIZE_MAX;
	bool ok;
	if (first == NULL || second == NULL) return false;
	a = first->pEngine; b = second->pEngine;
	if (xoauth2HttpXrtCleanup(first)) return false;
	http_defer_owner(first);
	xrtClearError();
	if (!xrtNetEngineUnpin(a)) return false;
	xrtAtomic32Store(&HoldRetirement, 1u, XMEMORY_RELEASE);
	thread = xrtThreadCreate(cleanup_thread, &task, 0u);
	if (thread == NULL) return false;
	deadline = __xrtWaitAfter(5000);
	while (xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) != 2u &&
		!__xrtWaitExpired(deadline)) xrtSleep(1u);
	ok = xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) == 2u &&
		!xoauth2HttpXrtCleanupPending(0, &pending) && pending == 1u;
	if (xoauth2HttpXrtCleanup(second)) return false;
	http_defer_owner(second);
	ok = !xoauth2HttpXrtCleanupPending(0, &pending) && pending == 2u && ok;
	xrtAtomic32Store(&HoldRetirement, 3u, XMEMORY_RELEASE);
	if (!join_thread(thread)) return false;
	xrtAtomic32Store(&HoldRetirement, 0u, XMEMORY_RELEASE);
	ok = !task.ready && task.pending >= 1u && task.pending <= 2u && ok;
	xrtClearError();
	deadline = __xrtWaitAfter(5000);
	do {
		ok = !xoauth2HttpXrtCleanupPending(0, &pending) && ok;
		if (pending == 1u) break;
		xrtSleep(1u);
	} while (!__xrtWaitExpired(deadline));
	ok = pending == 1u && ok;
	if (!xrtNetEngineUnpin(b)) return false;
	ok = xoauth2HttpXrtCleanupPending(5, &pending) && pending == 0u && ok;
	if (!memory_empty() || !ok) return false;
	puts("  claimed owner remains counted during concurrent enqueue/requeue: passed");
	return true;
}

static bool concurrent_factories(void)
{
	cleanup_task tasks[4] = {{0}};
	xthread* threads[4];
	size_t pending = SIZE_MAX;
	unsigned retires;
	bool ok;
	xrtAtomic32Store(&FactoryBarrier, 0u, XMEMORY_RELEASE);
	for (unsigned i = 0; i < 4u; i++) {
		tasks[i].factory = true;
		threads[i] = xrtThreadCreate(cleanup_thread, &tasks[i], 0u);
		if (threads[i] == NULL) return false;
	}
	for (unsigned i = 0; i < 4u; i++)
		if (!join_thread(threads[i]) || !tasks[i].ready) return false;
	xrtClearError();
	InjectRetirementError = true;
	ok = !xoauth2HttpXrtCleanupPending(0, &pending) && pending == 4u &&
		strcmp(xrtErrorMessage(xrtGetError()), "injected retirement failure") == 0;
	xrtClearError();
	SlowRetirement = true; retires = Retires;
	ok = !xoauth2HttpXrtCleanupPending(1, &pending) && pending == 4u &&
		Retires == retires + 1u && strcmp(xrtErrorMessage(xrtGetError()),
		 "http pending cleanup still has live objects") == 0 && ok;
	ok = xrtErrorKind(xrtGetError()) == XERR_TIMEOUT && xoauth2LastError() == XOAUTH2_ERROR_NETWORK && ok;
	SlowRetirement = false;
	xrtClearError();
	for (unsigned i = 0; i < 4u; i++) {
		if (!xrtNetEngineUnpin(tasks[i].engine)) return false;
		tasks[i] = (cleanup_task){false, true, false, SIZE_MAX, NULL};
		threads[i] = xrtThreadCreate(cleanup_thread, &tasks[i], 0u);
		if (threads[i] == NULL) return false;
	}
	for (unsigned i = 0; i < 4u; i++) {
		if (!join_thread(threads[i])) return false;
		ok = tasks[i].ready && tasks[i].pending == 0u && ok;
	}
	ok = xoauth2HttpXrtCleanupPending(0, &pending) && pending == 0u && ok;
	if (!memory_empty() || !ok) return false;
	puts("  four actual failed factories/drainers preserve ERROR and deadline tails: passed");
	return true;
}

int main(int argc, char** argv)
{
	FILE* input;
	char* ca;
	long size;
	bool ok;
	if (argc != 2 || (input = fopen(argv[1], "rb")) == NULL) return 2;
	if (fseek(input, 0, SEEK_END) != 0 || (size = ftell(input)) <= 0 ||
		fseek(input, 0, SEEK_SET) != 0) return 2;
	ca = malloc((size_t)size + 1u);
	if (ca == NULL || fread(ca, 1u, (size_t)size, input) != (size_t)size) return 2;
	ca[size] = 0; fclose(input); TestCa = ca;
	if (!xrtMemDebugEnable(true)) return 2;
	ok = factory_failures() && startup_ready_and_wrap_oom() &&
		published_owner_and_no_allocation() && claimed_owner() && concurrent_factories();
	if (!ok) fprintf(stderr, "HTTP unpublished ownership regression failed\n");
	free(ca);
	return ok ? 0 : 1;
}
