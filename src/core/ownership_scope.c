#include "../internal/xrt_internal.h"

/* One collective freeze domain per linked XRT instance; ordinary admission is
 * sharded by native thread, without TLS, allocation or a thread registry.
 * Locks/counts in different lanes occupy separate cache lines. The coordinator
 * is cold: only freeze begin/end touch it. No allocation, trace, user callback,
 * inner-state lock or reference operation runs under an admission lock.
 *
 * Frozen is read under any one lane lock and written under ALL lane locks.
 * Non-queuing freeze admission rolls back every acquired lock on contention
 * or an active mutation; an admitted reader can always enter nested work. */
enum { XRT_OWNERSHIP_LANES = 64 };
typedef union __xrtownershiplane {
	struct { xrt_spinlock Lock; size_t Mutators; } State;
	unsigned char Padding[128];
} __xrtownershiplane;

#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static xrt_spinlock __xrtOwnershipCoordinator = { PTHREAD_MUTEX_INITIALIZER };
#define __XRT_OWNERSHIP_LANE_INIT { { { PTHREAD_MUTEX_INITIALIZER }, 0 } }
#define __XRT_OWNERSHIP_LANE_4 __XRT_OWNERSHIP_LANE_INIT, __XRT_OWNERSHIP_LANE_INIT, __XRT_OWNERSHIP_LANE_INIT, __XRT_OWNERSHIP_LANE_INIT
#define __XRT_OWNERSHIP_LANE_16 __XRT_OWNERSHIP_LANE_4, __XRT_OWNERSHIP_LANE_4, __XRT_OWNERSHIP_LANE_4, __XRT_OWNERSHIP_LANE_4
static __xrtownershiplane __xrtOwnershipLanes[XRT_OWNERSHIP_LANES] = {
	__XRT_OWNERSHIP_LANE_16, __XRT_OWNERSHIP_LANE_16, __XRT_OWNERSHIP_LANE_16, __XRT_OWNERSHIP_LANE_16
};
#undef __XRT_OWNERSHIP_LANE_16
#undef __XRT_OWNERSHIP_LANE_4
#undef __XRT_OWNERSHIP_LANE_INIT
#else
static xrt_spinlock __xrtOwnershipCoordinator = {0};
static __xrtownershiplane __xrtOwnershipLanes[XRT_OWNERSHIP_LANES];
#endif
static xrtownershipscope* __xrtOwnershipFrozen;

enum {
	XRT_OWNERSHIP_MUTATION = 1,
	XRT_OWNERSHIP_FROZEN = 2,
	XRT_OWNERSHIP_NESTED = 3
};

static __xrtownershiplane* __xrtOwnershipLane(uint64 iThread)
{
	return &__xrtOwnershipLanes[(iThread * UINT64_C(0x9E3779B97F4A7C15)) >> 58];
}

static bool __xrtOwnershipTryLock(xrt_spinlock* pLock)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		return pthread_mutex_trylock(&pLock->Mutex) == 0;
	#else
		return __xrtAtomicRefCompareExchange(&pLock->Value, 1, 0) == 0;
	#endif
}

static void __xrtOwnershipUnlockLanes(size_t iAcquired)
{
	while (iAcquired != 0) __xrtSpinUnlock(&__xrtOwnershipLanes[--iAcquired].State.Lock);
}

static bool __xrtOwnershipScopeEmpty(const xrtownershipscope* pScope)
{
	return pScope != NULL && pScope->Self == NULL && pScope->Parent == NULL &&
		pScope->Thread == 0 && pScope->Children == 0 && pScope->Mode == 0;
}

static bool __xrtOwnershipScopeInvalid(const xrtownershipscope* pScope)
{
	if (pScope == NULL) __xrtErrorSetInvalidArgument();
	else __xrtErrorSetInvalidState();
	return false;
}

static void __xrtOwnershipAdmissionYield(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)SwitchToThread();
	#else
		(void)sched_yield();
	#endif
}

XRT_API bool xrtOwnershipMutationBegin(xrtownershipscope* pScope)
{
	uint64 iThread; __xrtownershiplane* pLane;
	if (!__xrtOwnershipScopeEmpty(pScope)) return __xrtOwnershipScopeInvalid(pScope);
	iThread = __xrtCurrentThreadId();
	pLane = __xrtOwnershipLane(iThread);
	for (;;) {
		__xrtSpinLock(&pLane->State.Lock);
		if (__xrtOwnershipFrozen == NULL) {
			if (pLane->State.Mutators == SIZE_MAX) {
				__xrtSpinUnlock(&pLane->State.Lock);
				__xrtErrorSetSizeOverflow(); return false;
			}
			++pLane->State.Mutators;
			pScope->Self = pScope; pScope->Thread = iThread;
			pScope->Mode = XRT_OWNERSHIP_MUTATION;
			__xrtSpinUnlock(&pLane->State.Lock); return true;
		}
		if (__xrtOwnershipFrozen->Thread == iThread) {
			if (__xrtOwnershipFrozen->Children == SIZE_MAX) {
				__xrtSpinUnlock(&pLane->State.Lock);
				__xrtErrorSetSizeOverflow(); return false;
			}
			++__xrtOwnershipFrozen->Children;
			pScope->Self = pScope; pScope->Parent = __xrtOwnershipFrozen;
			pScope->Thread = iThread; pScope->Mode = XRT_OWNERSHIP_NESTED;
			__xrtSpinUnlock(&pLane->State.Lock); return true;
		}
		__xrtSpinUnlock(&pLane->State.Lock);
		__xrtOwnershipAdmissionYield();
	}
}

XRT_API bool xrtOwnershipFreezeTryBegin(xrtownershipscope* pScope)
{
	size_t iAcquired = 0;
	if (!__xrtOwnershipScopeEmpty(pScope)) return __xrtOwnershipScopeInvalid(pScope);
	if (!__xrtOwnershipTryLock(&__xrtOwnershipCoordinator)) return false;
	while (iAcquired < XRT_OWNERSHIP_LANES) {
		__xrtownershiplane* pLane = &__xrtOwnershipLanes[iAcquired];
		if (!__xrtOwnershipTryLock(&pLane->State.Lock)) break;
		++iAcquired;
		if (__xrtOwnershipFrozen != NULL || pLane->State.Mutators != 0) break;
	}
	/* The last lane may have been acquired but refused for an active reader. */
	if (iAcquired != XRT_OWNERSHIP_LANES || __xrtOwnershipFrozen != NULL ||
		__xrtOwnershipLanes[XRT_OWNERSHIP_LANES - 1].State.Mutators != 0) {
		__xrtOwnershipUnlockLanes(iAcquired);
		__xrtSpinUnlock(&__xrtOwnershipCoordinator); return false;
	}
	pScope->Self = pScope; pScope->Thread = __xrtCurrentThreadId();
	pScope->Mode = XRT_OWNERSHIP_FROZEN; __xrtOwnershipFrozen = pScope;
	__xrtOwnershipUnlockLanes(iAcquired);
	__xrtSpinUnlock(&__xrtOwnershipCoordinator); return true;
}

XRT_API bool xrtOwnershipScopeEnd(xrtownershipscope* pScope)
{
	bool bValid = false; __xrtownershiplane* pLane;
	if (pScope == NULL || pScope->Self != pScope || pScope->Thread != __xrtCurrentThreadId())
		return __xrtOwnershipScopeInvalid(pScope);
	if (pScope->Mode == XRT_OWNERSHIP_FROZEN) {
		size_t i;
		__xrtSpinLock(&__xrtOwnershipCoordinator);
		for (i = 0; i < XRT_OWNERSHIP_LANES; ++i) __xrtSpinLock(&__xrtOwnershipLanes[i].State.Lock);
		if (pScope->Parent == NULL && pScope->Children == 0 && __xrtOwnershipFrozen == pScope) {
			__xrtOwnershipFrozen = NULL; *pScope = (xrtownershipscope){0}; bValid = true;
		}
		__xrtOwnershipUnlockLanes(XRT_OWNERSHIP_LANES);
		__xrtSpinUnlock(&__xrtOwnershipCoordinator);
		return bValid ? true : __xrtOwnershipScopeInvalid(pScope);
	}
	pLane = __xrtOwnershipLane(pScope->Thread);
	__xrtSpinLock(&pLane->State.Lock);
	if (pScope->Mode == XRT_OWNERSHIP_MUTATION && pScope->Parent == NULL &&
		pScope->Children == 0 && __xrtOwnershipFrozen == NULL && pLane->State.Mutators != 0) {
		--pLane->State.Mutators; bValid = true;
	} else if (pScope->Mode == XRT_OWNERSHIP_NESTED && pScope->Children == 0 &&
		pScope->Parent == __xrtOwnershipFrozen && __xrtOwnershipFrozen != NULL &&
		__xrtOwnershipFrozen->Children != 0) {
		--__xrtOwnershipFrozen->Children; bValid = true;
	}
	if (bValid) *pScope = (xrtownershipscope){0};
	__xrtSpinUnlock(&pLane->State.Lock);
	return bValid ? true : __xrtOwnershipScopeInvalid(pScope);
}
