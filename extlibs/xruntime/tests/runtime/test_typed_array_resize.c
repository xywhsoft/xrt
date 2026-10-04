#include "../test.h"

/* One owned resource per default; large slots make allocator probes bypass
 * the heap's small-block cache. Logical OOM uses the public MemDebug injector. */
typedef struct testresizeitem {
	int* Resource;
	unsigned char Padding[8192u - sizeof(int*)];
} testresizeitem;

typedef struct testresizestate {
	xtypedarray* Array;
	size_t LiveValues;
	size_t Initializations;
	size_t FailInitialization;
	size_t Drops;
	int DropOrder[32];
	bool ProbeAlloc;
	bool ProbeFree;
	bool DropError;
	size_t ExpectedFreeCount;
	xerror* Secondary;
} testresizestate;

static testresizestate State;

/* Preserve even a preexisting error while probing callback/allocator access. */
static void testResizeProbe(void)
{
	xerror* pSaved = xrtTakeError();
	testRequire(xrtTypedArrayCount(State.Array) == 0u &&
		xrtErrorKind(xrtGetError()) == XERR_STATE,
		"resize allowed same-array API reentry");
	xrtSetErrorTake(pSaved);
}

static void testResizeAllocationProbe(void)
{
	if ( State.ProbeAlloc ) {
		State.ProbeAlloc = false;
		testResizeProbe();
	}
}

static ptr testResizeAlloc(ptr pContext, size_t iSize)
{
	(void)pContext;
	testResizeAllocationProbe();
	return malloc(iSize);
}

static ptr testResizeRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	(void)pContext;
	testResizeAllocationProbe();
	return realloc(pMemory, iSize);
}

static void testResizeFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	if ( State.ProbeFree ) {
		State.ProbeFree = false;
		testRequire(State.Array->Storage.Count == State.ExpectedFreeCount,
			"resize allocator cleanup ran before ownership publication");
		testResizeProbe();
	}
	free(pMemory);
}

static bool testResizeInit(ptr pValue, const xrttype* pType)
{
	testresizeitem* pItem = pValue;
	(void)pType;
	memset(pItem, 0, sizeof(*pItem));
	size_t iOrdinal = State.Initializations++;
	/* Force a backing allocation for the allocator reentry probes; logical
	 * faults below still use MemDebug and do not mistake heap caches for leaks. */
	pItem->Resource = xrtMalloc(8192u);
	if ( pItem->Resource == NULL ) {
		return false;
	}
	*pItem->Resource = (int)iOrdinal + 1;
	if ( iOrdinal == State.FailInitialization ) {
		/* Failed Init owns and releases its partial value, not the caller. */
		xrtFree(pItem->Resource);
		pItem->Resource = NULL;
		xrtSetErrorInfo(XERR_VALUE, "test.resize.init", 17, "init rejected");
		return false;
	}
	State.LiveValues++;
	return true;
}

static bool testResizeCopy(ptr pTarget, const void* pSource, const xrttype* pType)
{
	(void)pTarget;
	(void)pSource;
	(void)pType;
	testRequire(false, "resize copied an existing owned element");
	return false;
}

static bool testResizeMove(ptr pTarget, ptr pSource, const xrttype* pType)
{
	return testResizeCopy(pTarget, pSource, pType);
}

static void testResizeDrop(ptr pValue, const xrttype* pType)
{
	testresizeitem* pItem = pValue;
	(void)pType;
	testRequire(pItem->Resource != NULL && State.LiveValues != 0u,
		"resize dropped an uninitialized or transferred value twice");
	testRequire(State.Drops < 32u, "resize drop ledger overflow");
	State.DropOrder[State.Drops++] = *pItem->Resource;
	State.LiveValues--;
	xrtFree(pItem->Resource);
	pItem->Resource = NULL;
	if ( State.DropError ) {
		xrtSetError(State.Secondary);
	}
}

static xrttype testResizeType(size_t iAlignment)
{
	static const xrttypeops Ops = {
		.Init = testResizeInit, .Copy = testResizeCopy,
		.Move = testResizeMove, .Drop = testResizeDrop
	};
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("test.resize.Owned")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_RELOCATABLE,
		.Name = XRT_STR_INIT("Owned"), .AbiName = XRT_STR_INIT("test.resize.Owned"),
		.Size = sizeof(testresizeitem), .Align = iAlignment,
		.InstanceSize = sizeof(testresizeitem), .InstanceAlign = iAlignment,
		.Ops = &Ops
	};
	return Type;
}

/* Failed growth must not invalidate even the allocation identity. */
static void testResizeUnchanged(xtypedarray* pArray, const xarray* pBefore)
{
	testRequire(pArray->Storage.Data == pBefore->Data &&
		pArray->Storage.Allocation == pBefore->Allocation &&
		pArray->Storage.Count == pBefore->Count &&
		pArray->Storage.Capacity == pBefore->Capacity,
		"failed resize changed address/count/capacity");
	for ( size_t i = 0u; i < pBefore->Count; i++ ) {
		const testresizeitem* pItem = xrtTypedArrayConstGet(pArray, i);
		testRequire(*pItem->Resource == (int)i + 1,
			"failed resize changed an existing element");
	}
}

static void testResizeBalanced(const xmemdebugsnapshot* pBefore)
{
	if ( !xrtMemDebugEnabled() ) {
		return; /* The allocator-probe phase uses the owned-value ledger. */
	}
	xmemdebugsnapshot After;
	xrtMemDebugSnapshot(&After);
	testRequire(After.LiveCount == pBefore->LiveCount && After.LiveBytes == pBefore->LiveBytes &&
		After.InvalidFreeCount == pBefore->InvalidFreeCount &&
		After.DoubleFreeCount == pBefore->DoubleFreeCount &&
		After.OverflowCount == pBefore->OverflowCount && After.UnderflowCount == pBefore->UnderflowCount &&
		After.UseAfterFreeCount == pBefore->UseAfterFreeCount,
		"resize logical allocation ledger is not balanced");
}

static void testResizeCase(size_t iAlignment, bool bSpare, size_t iFailure)
{
	xmemdebugsnapshot Baseline;
	xrtMemDebugSnapshot(&Baseline);
	xrttype Type = testResizeType(iAlignment);
	xtypedarray Array;
	State.Initializations = 0u;
	State.FailInitialization = SIZE_MAX;
	testRequire(xrtTypedArrayInit(&Array, &Type) && xrtTypedArrayResize(&Array, 3u),
		"resize fixture failed");
	if ( bSpare ) {
		testRequire(xrtTypedArrayReserve(&Array, 40u), "resize spare reserve failed");
	}
	xarray Before = Array.Storage;
	size_t iTarget = bSpare ? 9u : Before.Capacity + 6u;
	State.Array = &Array;
	State.Drops = 0u;
	State.FailInitialization = iFailure == SIZE_MAX ? SIZE_MAX : 3u + iFailure;
	State.DropError = iFailure != SIZE_MAX;
	State.ProbeAlloc = !xrtMemDebugEnabled() && !bSpare;
	State.ProbeFree = !xrtMemDebugEnabled() && (!bSpare || iFailure != SIZE_MAX);
	State.ExpectedFreeCount = iFailure == SIZE_MAX ? iTarget : Before.Count;
	testRequire(xrtTypedArrayResize(&Array, iTarget) == (iFailure == SIZE_MAX),
		"resize returned an incorrect init outcome");
	if ( iFailure != SIZE_MAX ) {
		testRequire(xrtErrorFind(xrtGetError(), "test.resize.init", 17) != NULL &&
			xrtErrorFind(xrtGetError(), "test.resize.cleanup", 18) == NULL,
			"resize replaced the primary init failure during cleanup");
		testRequire(State.Drops == iFailure && State.LiveValues == 3u,
			"resize rollback lost/duplicated a default value");
		for ( size_t i = 0u; i < State.Drops; i++ ) {
			testRequire(State.DropOrder[i] == (int)(3u + iFailure - i),
				"resize rollback is not reverse initialization order");
		}
		testResizeUnchanged(&Array, &Before);
	} else {
		testRequire(xrtGetError() == NULL && State.Drops == 0u &&
			State.LiveValues == iTarget && Array.Storage.Count == iTarget &&
			((uintptr_t)Array.Storage.Data % iAlignment) == 0u,
			"successful resize broke ownership/alignment");
		for ( size_t i = 0u; i < iTarget; i++ ) {
			const testresizeitem* pItem = xrtTypedArrayConstGet(&Array, i);
			testRequire(*pItem->Resource == (int)i + 1, "resize lost a transferred value");
		}
		if ( bSpare ) {
			testRequire(Array.Storage.Data == Before.Data &&
				Array.Storage.Capacity == Before.Capacity,
				"spare-capacity resize unnecessarily allocated a new array");
		}
	}
	testRequire(!State.ProbeAlloc && !State.ProbeFree, "resize did not execute allocator probes");
	State.DropError = false;
	State.Drops = 0u;
	xrtClearError();
	xrtTypedArrayUnit(&Array);
	testRequire(State.LiveValues == 0u, "resize init failure/success leaked an owned value");
	testResizeBalanced(&Baseline);
}

static void testResizeOom(size_t iAlignment, bool bSpare)
{
	for ( size_t iPoint = 0u; iPoint < 128u; iPoint++ ) {
		xmemdebugsnapshot Baseline;
		xrtMemDebugSnapshot(&Baseline);
		xrttype Type = testResizeType(iAlignment);
		xtypedarray Array;
		State.Initializations = 0u;
		State.FailInitialization = SIZE_MAX;
		State.Drops = 0u;
		testRequire(xrtTypedArrayInit(&Array, &Type) && xrtTypedArrayResize(&Array, 3u),
			"resize OOM fixture failed");
		if ( bSpare ) {
			testRequire(xrtTypedArrayReserve(&Array, 40u), "resize OOM spare reserve failed");
		}
		xarray Before = Array.Storage;
		size_t iTarget = bSpare ? 9u : Before.Capacity + 6u;
		testRequire(xrtMemDebugFailAfter(iPoint), "resize logical fault arming failed");
		bool bResult = xrtTypedArrayResize(&Array, iTarget);
		bool bHit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( bHit ) {
			testRequire(!bResult && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"resize swallowed an allocation failure");
			testRequire(State.LiveValues == 3u, "resize OOM default ledger changed");
			testResizeUnchanged(&Array, &Before);
		} else {
			testRequire(bResult && xrtGetError() == NULL, "resize OOM prefix never closed");
		}
		State.Drops = 0u;
		xrtClearError();
		xrtTypedArrayUnit(&Array);
		testRequire(State.LiveValues == 0u, "resize OOM leaked an owned value");
		testResizeBalanced(&Baseline);
		if ( !bHit ) {
			printf("[PASS] resize align=%zu spare=%d closed-prefix=%zu\n",
				iAlignment, bSpare, iPoint);
			return;
		}
	}
	testRequire(false, "resize OOM prefix did not terminate");
}

int main(void)
{
	State.FailInitialization = SIZE_MAX;
	xallocator Allocator = { &State, testResizeAlloc, testResizeRealloc, testResizeFree };
	testRequire(xrtSetAllocator(&Allocator), "resize allocator install failed");
	State.Secondary = xrtErrorCreate(XERR_STATE, "test.resize.cleanup", 18, "drop failed");
	testRequire(State.Secondary != NULL, "resize secondary fixture failed");
	for ( size_t iAlignment = 16u; iAlignment <= 64u; iAlignment *= 4u ) {
		for ( int iSpare = 0; iSpare <= 1; iSpare++ ) {
			/* Initial raw capacity is 8; detached path adds 11, spare adds 6. */
			size_t iAdded = iSpare ? 6u : 11u;
			for ( size_t iFailure = 0u; iFailure < iAdded; iFailure++ ) {
				testResizeCase(iAlignment, iSpare != 0, iFailure);
			}
			testResizeCase(iAlignment, iSpare != 0, SIZE_MAX);
		}
	}
	/* Successful growth and allocator probes must not discard a pending error. */
	xrttype Type = testResizeType(64u);
	xtypedarray Pending;
	State.Initializations = 0u;
	State.FailInitialization = SIZE_MAX;
	testRequire(xrtTypedArrayInit(&Pending, &Type) && xrtTypedArrayResize(&Pending, 3u),
		"resize pending-error fixture failed");
	State.Array = &Pending;
	State.ProbeAlloc = true;
	State.ProbeFree = true;
	State.ExpectedFreeCount = Pending.Storage.Capacity + 6u;
	xrtSetError(State.Secondary);
	testRequire(xrtTypedArrayResize(&Pending, State.ExpectedFreeCount) &&
		xrtGetError() == State.Secondary && !State.ProbeAlloc && !State.ProbeFree,
		"resize success lost a pending error or cleanup reentry protection");
	xrtClearError();
	State.Drops = 0u;
	xrtTypedArrayUnit(&Pending);
	testRequire(State.LiveValues == 0u, "resize pending-error fixture leaked values");
	/* No Init callback: bulk-zero fast path, same transaction and BUSY gate. */
	xtypedarray Array;
	testRequire(xrtTypedArrayInit(&Array, xrtTypeInt64()), "resize scalar init failed");
	State.Array = &Array;
	State.ProbeAlloc = true;
	testRequire(xrtTypedArrayResize(&Array, 8192u) && !State.ProbeAlloc,
		"resize scalar fast path allocator was not protected");
	for ( size_t i = 0u; i < 8192u; i++ ) {
		testRequire(*(const int64*)xrtTypedArrayConstGet(&Array, i) == 0,
			"resize scalar default was not zero");
	}
	xrtTypedArrayUnit(&Array);
	xrtErrorFree(State.Secondary);
	xrtClearError();
	/* Debug quarantine intentionally delays backing frees. Do not pretend
	 * those callbacks run inside Resize; test them separately above without
	 * quarantine, then check complete logical faults/ledgers with it enabled. */
	testRequire(xrtMemDebugEnable(true), "resize debug ledger enable failed");
	xmemdebugsnapshot Baseline;
	xrtMemDebugSnapshot(&Baseline);
	State.Secondary = xrtErrorCreate(XERR_STATE, "test.resize.cleanup", 18, "drop failed");
	testRequire(State.Secondary != NULL, "resize debug secondary fixture failed");
	for ( size_t iAlignment = 16u; iAlignment <= 64u; iAlignment *= 4u ) {
		for ( int iSpare = 0; iSpare <= 1; iSpare++ ) {
			size_t iAdded = iSpare ? 6u : 11u;
			for ( size_t iFailure = 0u; iFailure < iAdded; iFailure++ ) {
				testResizeCase(iAlignment, iSpare != 0, iFailure);
			}
			testResizeCase(iAlignment, iSpare != 0, SIZE_MAX);
			testResizeOom(iAlignment, iSpare != 0);
		}
	}
	testRequire(xrtTypedArrayInit(&Array, xrtTypeInt64()) && xrtTypedArrayResize(&Array, 3u),
		"resize scalar failure fixture failed");
	*(int64*)xrtTypedArrayGet(&Array, 0u) = 37;
	xarray Before = Array.Storage;
	testRequire(xrtMemDebugFailAfter(0u), "resize scalar fault arming failed");
	testRequire(!xrtTypedArrayResize(&Array, Before.Capacity + 6u) &&
		xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"resize scalar allocation failure was lost");
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(!xrtTypedArrayResize(&Array, SIZE_MAX) && xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"resize size overflow was not rejected");
	testRequire(Array.Storage.Data == Before.Data && Array.Storage.Allocation == Before.Allocation &&
		Array.Storage.Count == Before.Count && Array.Storage.Capacity == Before.Capacity &&
		*(const int64*)xrtTypedArrayConstGet(&Array, 0u) == 37,
		"resize scalar failure changed an existing value/address/capacity");
	xrtClearError();
	xrtTypedArrayUnit(&Array);
	xrtErrorFree(State.Secondary);
	xrtClearError();
	testRequire(State.LiveValues == 0u, "resize final owned-value ledger is not balanced");
	testResizeBalanced(&Baseline);
	printf("[PASS] typed array resize transaction\n");
	return 0;
}
