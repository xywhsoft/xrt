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

int main(void)
{
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testRequire(xrtMemDebugEnable(true), "enable real allocator ledger");
	testCowAndErrors(); testCowAndErrors();
	xmemdebugsnapshot Before = {0}, After = {0};
	xrtMemDebugSnapshot(&Before);
#endif
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
