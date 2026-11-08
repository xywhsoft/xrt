#include "../test.h"

typedef struct testpostcommit {
	xvalue* Parent;
	unsigned References;
	unsigned Drops;
	bool Committed;
	bool OutsideMutation;
	bool Raise;
} testpostcommit;

static bool postCommitTrace(const xvalue* pValue, xrtownershipvisitor pVisit, ptr pContext)
{
	ptr pHandle = NULL;
	if (!xrtValueGetHandle(pValue, &pHandle, NULL, NULL)) return false;
	const testpostcommit* pState = (const testpostcommit*)pHandle;
	for (unsigned i = 0; i < pState->References; ++i)
		if (!pVisit(xrtValueOwnership(pState->Parent), pContext)) return false;
	return true;
}

static void postCommitDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	testpostcommit* pState = (testpostcommit*)pHandle;
	int64 iValue = 0;
	xvalue* pCurrent = xrtValueObjectGet(pState->Parent, XRT_STR_LITERAL("slot"));
	pState->Committed = pCurrent != NULL && xrtValueGetInt(pCurrent, &iValue) && iValue == 99;
	xrtownershipscope Freeze = {0};
	pState->OutsideMutation = xrtOwnershipFreezeTryBegin(&Freeze);
	if (pState->OutsideMutation) testRequire(xrtOwnershipScopeEnd(&Freeze), "end callback freeze");
	++pState->Drops;
	while (pState->References != 0) {
		--pState->References;
		xrtValueRelease(pState->Parent);
	}
	if (pState->Raise) { xrtClearError(); xrtSetErrorKind(XERR_PROTOCOL); }
}

static size_t strongCount(xvalue* pValue)
{
	xrtownershipref Reference = xrtValueOwnership(pValue);
	xrtownershipscope Freeze = {0};
	size_t iCount = 0;
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze), "count under freeze");
	testRequire(Reference.Ops != NULL && Reference.Ops->Count(Reference.Data, &iCount), "actual strong count");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "end count freeze");
	return iCount;
}

static void testBackReferences(void)
{
	static const xvaluehandleops Ops = {NULL, postCommitDrop, NULL, NULL};
	for (unsigned i = 0; i < 100; ++i) {
		xvalue* pParent = xrtValueObject();
		testpostcommit State = {pParent, 2, 0, false, false, false};
		testRequire(pParent && xrtValueRetain(pParent) && xrtValueRetain(pParent), "two actual child back-references");
		ptr pHandle = &State;
		xvalue* pChild = xrtValueHandleTake(&pHandle, &Ops, NULL);
		testRequire(pChild && !pHandle && xrtValueHandleOwnershipBindPhased(pChild, postCommitTrace), "exact phased owner");
		testRequire(pChild && xrtValueObjectSetNew(pParent, XRT_STR_LITERAL("slot"), pChild), "install child");
		testRequire(strongCount(pParent) == 3, "before replace: caller and two child edges");
		testRequire(xrtValueObjectSetNewPostCommit(pParent, XRT_STR_LITERAL("slot"), xrtValueInt(99)), "replace after commit");
		testRequire(State.Drops == 1 && State.Committed && State.OutsideMutation && !xrtGetError(), "actual old drop after publication and mutation");
		testRequire(strongCount(pParent) == 1, "both owned back-references consumed, not refused");
		xrtValueRelease(pParent);
	}
}

static void testCowAndErrors(void)
{
	static const xvaluehandleops Ops = {NULL, postCommitDrop, NULL, NULL};
	xvalue* pObject = xrtValueObject();
	testRequire(pObject && xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("first"), xrtValueInt(1)), "first key");
	testRequire(xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("slot"), xrtValueInt(2)), "second key");
	xvalue* pCopy = xrtValueClone(pObject);
	testRequire(pCopy && xrtValueObjectSetNewPostCommit(pCopy, XRT_STR_LITERAL("slot"), xrtValueInt(99)), "COW replacement");
	int64 iValue = 0;
	testRequire(xrtValueGetInt(xrtValueObjectGet(pObject, XRT_STR_LITERAL("slot")), &iValue) && iValue == 2, "COW source unchanged");
	xstrview Key = {0};
	testRequire(xrtValueObjectAt(pCopy, 1, &Key) && Key.Size == 4 && !memcmp(Key.Data, "slot", 4), "replacement preserves key order");
	xvalue* pSame = xrtValueRetain(xrtValueObjectGet(pCopy, XRT_STR_LITERAL("slot")));
	testRequire(pSame && xrtValueObjectSetNewPostCommit(pCopy, XRT_STR_LITERAL("slot"), pSame), "same value consumes independent owner");
	testRequire(strongCount(pSame) == 1, "same value has only field owner");
	testRequire(xrtValueObjectSetNewPostCommit(pCopy, (xstrview){"a\0b", 3}, xrtValueInt(8)), "exact NUL key");
	testRequire(!xrtValueObjectSetNewPostCommit(pCopy, (xstrview){NULL, 1}, xrtValueInt(7)), "invalid key");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT && xrtValueCount(pCopy) == 3, "invalid key keeps object intact");
	xrtClearError();
	testpostcommit State = {pCopy, 0, 0, false, false, true};
	ptr pHandle = &State;
	xvalue* pChild = xrtValueHandleTake(&pHandle, &Ops, NULL);
	testRequire(pChild && xrtValueHandleOwnershipBindPhased(pChild, postCommitTrace), "raising phased child");
	testRequire(xrtValueObjectSetNew(pCopy, XRT_STR_LITERAL("slot"), pChild), "install raising child");
	testRequire(xrtValueObjectSetNewPostCommit(pCopy, XRT_STR_LITERAL("slot"), xrtValueInt(99)), "publication committed despite retirement diagnostic");
	testRequire(State.Drops == 1 && State.Committed && State.OutsideMutation && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL, "retirement diagnostic visible");
	xrtClearError();
	State = (testpostcommit){pCopy, 0, 0, false, false, true};
	pHandle = &State;
	pChild = xrtValueHandleTake(&pHandle, &Ops, NULL);
	testRequire(pChild && xrtValueHandleOwnershipBindPhased(pChild, postCommitTrace) &&
		xrtValueObjectSetNew(pCopy, XRT_STR_LITERAL("slot"), pChild), "install second raising child");
	xvalue* pInput = xrtValueInt(99);
	xrtSetErrorKind(XERR_RANGE);
	testRequire(xrtValueObjectSetNewPostCommit(pCopy, XRT_STR_LITERAL("slot"), pInput) &&
		State.Drops == 1 && xrtErrorKind(xrtGetError()) == XERR_RANGE, "existing error outranks destructor error");
	xrtClearError();
	xrtValueRelease(pCopy); xrtValueRelease(pObject);
}

static void testReceiverPin(void)
{
	static const xvaluehandleops Ops = {NULL, postCommitDrop, NULL, NULL};
	xvalue* pParent = xrtValueObject();
	testpostcommit State = {pParent, 2, 0, false, false, false};
	testRequire(pParent && xrtValueRetain(pParent) && xrtValueRetain(pParent), "pin case: two child edges");
	xvalue* pWeak = xrtValueWeakRef(pParent);
	ptr pHandle = &State;
	xvalue* pChild = xrtValueHandleTake(&pHandle, &Ops, NULL);
	testRequire(pWeak && pChild && xrtValueHandleOwnershipBindPhased(pChild, postCommitTrace) &&
		xrtValueObjectSetNew(pParent, XRT_STR_LITERAL("slot"), pChild), "pin case: install cycle");
	xrtValueRelease(pParent); /* Child edges alone keep this quiescent borrowed address alive. */
	testRequire(xrtValueObjectSetNewPostCommit(pParent, XRT_STR_LITERAL("slot"), xrtValueInt(99)), "receiver pin spans last back-reference release");
	testRequire(State.Drops == 1 && State.Committed && State.OutsideMutation && !xrtGetError(), "old drop sees intact receiver through API pin");
	xvalue* pPromoted = xrtValueWeakRefLock(pWeak);
	testRequire(pPromoted == xrtValueNull(), "receiver destroyed when API releases last real pin");
	xrtValueRelease(pPromoted); xrtValueRelease(pWeak);
}

#if defined(XRT_FEATURE_MEMORY_DEBUG)
static unsigned testOomPrefixes(void)
{
	unsigned iPrefixes = 0;
	char KeyData[512]; memset(KeyData, 'k', sizeof(KeyData));
	for (unsigned iMode = 0; iMode < 2; ++iMode) {
		bool bFinished = false;
		for (int64 iFail = 0; iFail < 100; ++iFail) {
			xvalue* pObject = xrtValueObject();
			testRequire(pObject && xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("slot"), xrtValueInt(2)), "OOM seed");
			xvalue* pTarget = iMode ? xrtValueClone(pObject) : xrtValueRetain(pObject);
			xvalue* pInput = xrtValueInt(99);
			xvalue* pWeak = xrtValueWeakRef(pInput);
			testRequire(pTarget && pInput && pWeak, "OOM independent owners");
			xstrview Key = iMode ? XRT_STR_LITERAL("slot") : (xstrview){KeyData, sizeof(KeyData)};
			testRequire(xrtMemDebugFailAfter(iFail), "arm actual allocation prefix");
			bool bResult = xrtValueObjectSetNewPostCommit(pTarget, Key, pInput);
			bool bHit = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear(); xrtClearError();
			xvalue* pPromoted = xrtValueWeakRefLock(pWeak);
			bool bAlive = pPromoted != NULL && xrtValueType(pPromoted) != XVALUE_NULL;
			if (bAlive != bResult)
				fprintf(stderr, "mode=%u prefix=%lld hit=%d result=%d promoted=%p error=%d\n", iMode, (long long)iFail,
					(int)bHit, (int)bResult, (void*)pPromoted, (int)xrtErrorKind(xrtGetError()));
			testRequire(pPromoted != NULL && !xrtGetError() && bAlive == bResult, "input consumed on both outcomes");
			xrtValueRelease(pPromoted);
			int64 iValue = 0;
			if (bHit) {
				testRequire(!bResult && xrtValueCount(pTarget) == 1, "failure preserves all fields");
				testRequire(xrtValueGetInt(xrtValueObjectGet(pTarget, XRT_STR_LITERAL("slot")), &iValue) && iValue == 2, "failure preserves original payload");
			} else {
				testRequire(bResult && xrtValueGetInt(xrtValueObjectGet(pTarget, Key), &iValue) && iValue == 99, "first non-hit completes operation");
				bFinished = true;
			}
			if (iMode) testRequire(xrtValueGetInt(xrtValueObjectGet(pObject, XRT_STR_LITERAL("slot")), &iValue) && iValue == 2, "COW source intact after every prefix");
			xrtValueRelease(pTarget); xrtValueRelease(pObject); xrtValueRelease(pWeak);
			testRequire(!xrtGetError(), "OOM cleanup has no error");
			++iPrefixes;
			if (bFinished) break;
		}
		testRequire(bFinished, "all prefixes exhausted for insertion and COW replacement");
	}
	return iPrefixes;
}
#endif

static void postCommitFinalizer(xvalue* pObject, ptr pData)
{
	unsigned* pCalls = pData;
	++*pCalls;
	testRequire(xrtValueRetain(pObject) == NULL, "finalizer receiver cannot be retained");
	testRequire(xrtGetError() != NULL, "resurrection is rejected"); xrtClearError();
	testRequire(xrtValueObjectSetNewPostCommit(pObject, (xstrview){"value", 5}, xrtValueInt(0)),
		"borrowed finalizer may replace a field after commit");
	int64 iValue = -1;
	testRequire(xrtValueGetInt(xrtValueObjectGet(pObject, (xstrview){"value", 5}), &iValue) && iValue == 0,
		"finalizer observes the committed replacement");
	testRequire(xrtValueRetain(pObject) == NULL, "field write does not revive receiver");
	xrtClearError();
}
static void testFinalizerBorrow(void)
{
	unsigned iCalls = 0;
	xvalue* pObject = xrtValueObject();
	testRequire(pObject && xrtValueObjectSetNew(pObject, (xstrview){"value", 5}, xrtValueInt(1)) &&
		xrtValueObjectFinalizerBind(pObject, postCommitFinalizer, &iCalls), "prepare finalizer field write");
	xrtValueRelease(pObject);
	testRequire(iCalls == 1 && !xrtGetError(), "exactly one finalizer, no borrowed shell release");
}

static void testReadScope(void)
{
    xvalue* pArray = xrtValueArray();
    xvalue* pObject = xrtValueObject();
    const xstrview Key = {"n\0k", 3};
    testRequire(pArray && pObject && xrtValueArrayAppendNew(pArray, xrtValueInt(7)) &&
        xrtValueObjectSetNew(pObject, Key, xrtValueInt(9)), "read scope fixture");
    xvalue* pAlias = xrtValueClone(pArray);
    testRequire(pAlias != NULL, "independent COW shell");
    xvalueviewv1 Array = {0}, Object = {0};
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    xmemdebugsnapshot Before = {0}, After = {0};
    xrtMemDebugSnapshot(&Before);
#endif
    testRequire(xrtValueViewBeginV1(pArray, &Array) && xrtValueViewBeginV1(pObject, &Object),
        "borrow wire containers without allocation");
    int64 iValue = 0; xstrview Found = {0};
    testRequire(xrtValueViewCountV1(&Array) == 1 &&
        xrtValueGetInt(xrtValueViewAtV1(&Array, 0, NULL), &iValue) && iValue == 7,
        "read original array slot");
    testRequire(xrtValueGetInt(xrtValueViewFindV1(&Object, Key), &iValue) && iValue == 9,
        "exact embedded NUL lookup");
    testRequire(xrtValueViewAtV1(&Object, 0, &Found) != NULL && Found.Size == 3 &&
        !memcmp(Found.Data, Key.Data, 3), "borrow insertion-ordered key");
    testRequire(!xrtValueViewFindV1(&Object, (xstrview){"missing", 7}) && !xrtGetError(), "missing is not failure");
    xvalueviewv1 Copied = Array;
    testRequire(!xrtValueViewEndV1(&Copied), "copied scope cannot unguard original"); xrtClearError();
    testRequire(xrtValueRetain(pArray) == NULL, "guarded shell retain refused"); xrtClearError();
    xrtValueRelease(pArray); testRequire(xrtGetError() != NULL, "guarded shell release refused"); xrtClearError();
    testRequire(!xrtValueViewAtV1(&Array, 1, NULL), "view checks bounds"); xrtClearError();
    testRequire(xrtValueViewEndV1(&Object) && xrtValueViewEndV1(&Array) &&
        xrtValueViewEndV1(&Array), "read scope ends once and cleared End is harmless");
#if defined(XRT_FEATURE_MEMORY_DEBUG)
    xrtMemDebugSnapshot(&After);
    testRequire(Before.AllocCount == After.AllocCount && Before.FreeCount == After.FreeCount,
        "read, guard, lookup and end allocate and free nothing");
#endif
    testRequire(xrtValueViewBeginV1(pArray, &Array), "scope reusable after End");
    testRequire(xrtValueArrayAppendNew(pAlias, xrtValueInt(11)), "independent alias separates by COW");
    testRequire(xrtValueViewCountV1(&Array) == 1 && xrtValueViewEndV1(&Array), "borrow sees original backing");
    testRequire(!xrtValueViewAtV1(&Array, 0, NULL), "expired scope refused"); xrtClearError();
    xrtValueRelease(pAlias); xrtValueRelease(pArray); xrtValueRelease(pObject);
}

int main(void)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugEnable(true), "enable real allocator ledger");
	testCowAndErrors(); testCowAndErrors();
	xmemdebugsnapshot Before = {0}, After = {0};
	xrtMemDebugSnapshot(&Before);
#endif
	testReadScope();
	testFinalizerBorrow();
	testBackReferences();
	testCowAndErrors();
	testReceiverPin();
	testRequire(!xrtGetError(), "no residual diagnostic");
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	unsigned iPrefixes = testOomPrefixes();
	xrtMemDebugSnapshot(&After);
	testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
		After.AllocCount - Before.AllocCount == After.FreeCount - Before.FreeCount &&
		Before.InvalidFreeCount == After.InvalidFreeCount && Before.DoubleFreeCount == After.DoubleFreeCount &&
		Before.UseAfterFreeCount == After.UseAfterFreeCount, "all actual owners balance, no unsafe frees");
	printf("Value post-commit OOM: %u insertion/COW prefixes and balanced allocator ledger\n", iPrefixes);
#endif
	puts("Value post-commit: 100 actual replacements; back-references, COW, order, NUL keys, consumption and callback error");
	return 0;
}
