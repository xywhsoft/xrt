#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifdef INTERNAL_SPIN_SINGLE
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"
#endif
#include "../test.h"
#if defined(_WIN32) || defined(_WIN64)
/* Internal headers select Winsock2 before the native test helper's windows.h. */
#include "../../src/internal/xrt_internal.h"
#endif
#include "../test_thread.h"
#include "../../src/internal/xrt_internal.h"

#define INTERNAL_SPIN_THREADS 4u
#define INTERNAL_SPIN_ITERATIONS 50000u
#define INTERNAL_SPIN_PATTERN UINT64_C(0x3141592653589793)

typedef struct internal_spin_state {
	xrt_spinlock Lock;
	uint64 Counter, Mirror;
} internal_spin_state;

static int writer(ptr data)
{
	internal_spin_state* state = data;
	for (unsigned i = 0; i < INTERNAL_SPIN_ITERATIONS; ++i) {
		xrtownershipscope mutation = {0};
		if (!xrtOwnershipMutationBegin(&mutation)) return 1;
		__xrtSpinLock(&state->Lock);
		if (state->Mirror != (state->Counter ^ INTERNAL_SPIN_PATTERN)) {
			__xrtSpinUnlock(&state->Lock);
			(void)xrtOwnershipScopeEnd(&mutation);
			return 2;
		}
		++state->Counter;
		/* Expose a long inconsistent interval: an admitted freeze must never
		 * observe it, and the next lock holder must see the completed writes. */
		if ((i & 1023u) == 0) xrtThreadYield();
		state->Mirror = state->Counter ^ INTERNAL_SPIN_PATTERN;
		__xrtSpinUnlock(&state->Lock);
		if (!xrtOwnershipScopeEnd(&mutation)) return 3;
	}
	return 0;
}

static unsigned one(internal_spin_state* state)
{
	testthread threads[INTERNAL_SPIN_THREADS] = {0};
	unsigned snapshots = 0;
	state->Counter = 0; state->Mirror = INTERNAL_SPIN_PATTERN;
	for (unsigned i = 0; i < INTERNAL_SPIN_THREADS; ++i) {
		threads[i].Proc = writer; threads[i].Data = state;
	}
	testThreadsStart(threads, INTERNAL_SPIN_THREADS);
	for (unsigned i = 0; i < 10000; ++i) {
		xrtownershipscope freeze = {0};
		if (xrtOwnershipFreezeTryBegin(&freeze)) {
			testRequire(state->Mirror == (state->Counter ^ INTERNAL_SPIN_PATTERN),
				"whole-domain freeze must observe complete protected writes");
			testRequire(xrtOwnershipScopeEnd(&freeze), "internal spin freeze end");
			++snapshots;
		}
		xrtThreadYield();
	}
	testThreadsJoin(threads, INTERNAL_SPIN_THREADS);
	for (unsigned i = 0; i < INTERNAL_SPIN_THREADS; ++i)
		testRequire(threads[i].Result == 0, "internal spin exclusion and visibility");
	testRequire(state->Counter == (uint64)INTERNAL_SPIN_THREADS * INTERNAL_SPIN_ITERATIONS &&
		state->Mirror == (state->Counter ^ INTERNAL_SPIN_PATTERN), "no lost internal spin writes");
	xrtownershipscope freeze = {0};
	testRequire(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze),
		"all mutation admissions released after contention");
	return snapshots + 1;
}

int main(void)
{
	static internal_spin_state static_state = { XRT_INTERNAL_SPIN_INIT, 0, 0 };
	internal_spin_state dynamic_state;
	#if !defined(XRT_INTERNAL_SPIN_PTHREAD)
		testRequire(sizeof(xrt_spinlock) == sizeof(int32), "compact native short lock");
	#endif
	__xrtSpinInit(&dynamic_state.Lock);
	unsigned snapshots = one(&static_state) + one(&dynamic_state);
	__xrtSpinUnit(&dynamic_state.Lock); __xrtSpinUnit(&static_state.Lock);
	printf("Internal spin: protected-writes=400000 snapshots=%u lock-bytes=%zu static-and-dynamic=verified\n",
		snapshots, sizeof(xrt_spinlock));
	return 0;
}
