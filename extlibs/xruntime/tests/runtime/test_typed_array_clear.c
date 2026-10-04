#include "../test.h"

static xtypedarray* ClearArray;
static xerror* ClearErrors[3];
static unsigned int ClearMask, ClearDrops, ClearOrder[3];
static bool ClearProbe;

static void testClearDrop(ptr pValue, const xrttype* pType)
{
	(void)pType;
	unsigned int iValue = (unsigned int)*(uint64*)pValue;
	testRequire(iValue >= 1u && iValue <= 3u && ClearDrops < 3u,
		"clear dropped an invalid or already released element");
	ClearOrder[ClearDrops++] = iValue;
	*(uint64*)pValue = 0u;
	/* Every callback has an empty local error context even after an earlier
	 * Drop failed. Reentry remains rejected for the complete cleanup range. */
	testRequire(xrtGetError() == NULL, "Drop inherited another callback's error");
	if ( ClearProbe ) {
		testRequire(xrtTypedArrayCount(ClearArray) == 0u &&
			xrtErrorKind(xrtGetError()) == XERR_STATE, "clear allowed API reentry");
		xrtClearError();
	}
	if ( (ClearMask & (1u << (iValue - 1u))) != 0u ) {
		xrtSetErrorTake(xrtErrorRef(ClearErrors[iValue - 1u]));
	}
}

int main(void)
{
	testRequire(xrtMemDebugEnable(true), "clear debug enable failed");
	xmemdebugsnapshot Before, After;
	xrtMemDebugSnapshot(&Before);
	for ( unsigned int i = 0u; i < 3u; i++ ) {
		ClearErrors[i] = xrtErrorCreate(XERR_VALUE, "test.clear.drop", (int64)i + 1, "drop failed");
		testRequire(ClearErrors[i] != NULL, "clear error fixture failed");
	}
	xerror* pPending = xrtErrorCreate(XERR_RANGE, "test.clear.pending", 9, "pending failure");
	testRequire(pPending != NULL, "clear pending fixture failed");
	xrttypeops Ops = { .Drop = testClearDrop };
	xrttype Type = *xrtTypeUInt64();
	Type.Id = xrtTypeId((xstrview)XRT_STR_INIT("test.clear.u64"));
	Type.AbiName = (xstrview)XRT_STR_INIT("test.clear.u64");
	Type.Flags &= ~XRT_TYPE_FLAG_TRIVIAL_DROP;
	Type.Ops = &Ops;
	const unsigned int Masks[] = {0u, 1u, 2u, 4u, 7u};
	unsigned int iChecks = 0u;
	for ( unsigned int iOperation = 0u; iOperation < 4u; iOperation++ ) {
		for ( size_t iMask = 0u; iMask < sizeof(Masks) / sizeof(*Masks); iMask++ ) {
			for ( unsigned int iPending = 0u; iPending < 2u; iPending++ ) {
			for ( unsigned int iFault = 0u; iFault < 2u; iFault++ ) {
				xtypedarray Array;
				ClearArray = &Array; ClearMask = 0u; ClearDrops = 0u;
				testRequire(xrtTypedArrayInit(&Array, &Type) && xrtTypedArrayResize(&Array, 3u),
					"clear array fixture failed");
				for ( size_t i = 0u; i < 3u; i++ ) *(uint64*)xrtTypedArrayGet(&Array, i) = i + 1u;
				xarray Storage = Array.Storage;
				ClearMask = Masks[iMask];
				/* Reentry's rich diagnostic can allocate. Do not mistake the
				 * deliberate callback diagnostic for cleanup allocation. */
				ClearProbe = iFault == 0u;
				if ( iPending != 0u ) xrtSetErrorTake(xrtErrorRef(pPending));
				if ( iFault ) testRequire(xrtMemDebugFailAfter(0u), "clear fault arming failed");
				if ( iOperation == 0u ) xrtTypedArrayClear(&Array);
				else if ( iOperation == 1u ) testRequire(xrtTypedArrayResize(&Array, 1u), "shrink did not commit");
				else if ( iOperation == 2u ) testRequire(xrtTypedArrayRemove(&Array, 0u, 2u), "remove did not commit");
				else xrtTypedArrayUnit(&Array);
				testRequire(!xrtMemDebugFailTriggered(), "cleanup unexpectedly allocated");
				xrtMemDebugFailClear();
				unsigned int iFirst = iOperation == 2u ? 2u : 3u;
				unsigned int iExpectedDrops = (iOperation == 1u || iOperation == 2u) ? 2u : 3u;
				const xerror* pExpected = iPending ? pPending : NULL;
				for ( unsigned int i = 0u; i < iExpectedDrops; i++ ) {
					testRequire(ClearOrder[i] == iFirst - i, "cleanup order changed");
					if ( !pExpected && (ClearMask & (1u << (iFirst - i - 1u))) )
						pExpected = ClearErrors[iFirst - i - 1u];
				}
				testRequire(ClearDrops == iExpectedDrops && xrtGetError() == pExpected,
					"cleanup lost the first failure or abandoned a later Drop");
				xrtClearError();
				if ( iOperation != 3u ) {
					testRequire(Array.Storage.Data == Storage.Data && Array.Storage.Allocation == Storage.Allocation &&
						Array.Storage.Capacity == Storage.Capacity && Array.Storage.Count == (iExpectedDrops == 3u ? 0u : 1u),
						"cleanup changed capacity/address or did not commit its count");
					ClearMask = 0u;
					/* A residual item is still valid, and clear is reusable after
					 * its first reported error has been handled. */
					ClearDrops = 0u; xrtTypedArrayClear(&Array); xrtTypedArrayClear(&Array);
					testRequire(!xrtGetError(), "repeated clear left a stale error");
					xrtTypedArrayUnit(&Array);
				}
				iChecks++;
			}
			}
		}
	}
	xrtErrorFree(pPending);
	for ( unsigned int i = 0u; i < 3u; i++ ) xrtErrorFree(ClearErrors[i]);
	xrtMemDebugSnapshot(&After);
	testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
		After.AllocCount - Before.AllocCount == After.FreeCount - Before.FreeCount &&
		Before.InvalidFreeCount == After.InvalidFreeCount && Before.DoubleFreeCount == After.DoubleFreeCount &&
		Before.UseAfterFreeCount == After.UseAfterFreeCount && !xrtGetError(), "cleanup ledger did not balance");
	printf("[PASS] typed array cleanup first-error contract: %u cases\n", iChecks);
	return 0;
}
