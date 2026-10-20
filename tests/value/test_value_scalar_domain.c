#include "../test.h"

static void requireFailure(xvalue* pValue, uint64 iIdentity, uint32 iDomain, xerrkind Kind)
{
	uint64 iOldId = xrtValueTypeId(pValue);
	uint32 iOldDomain = xrtValueScalarDomainV1(pValue);
	xrtClearError();
	testRequire(!xrtValueScalarDomainBindV1(pValue, iIdentity, iDomain), "domain refusal missing");
	testRequire(xrtErrorKind(xrtGetError()) == Kind, "domain refusal kind");
	xrtClearError();
	testRequire(xrtValueTypeId(pValue) == iOldId &&
		xrtValueScalarDomainV1(pValue) == iOldDomain, "domain refusal mutated input");
}

static void scalar(xvalue* pValue)
{
	const uint64 iIdentity = UINT64_C(0xA1B2C3D4E5F60001);
	const uint32 iDomain = UINT32_C(0xFEDCBA98);
	xvalue* pShared;
	testRequire(pValue != NULL && xrtValueScalarDomainV1(pValue) == 0, "fresh scalar domain");
	uint64 iHashBefore, iHashAfter;
	bool bHashable = xrtValueHash(pValue, &iHashBefore);
	xrtClearError();
	pShared = xrtValueRetain(pValue);
	requireFailure(pValue, iIdentity, iDomain, XERR_STATE);
	xrtValueRelease(pShared);
	requireFailure(pValue, 0, iDomain, XERR_ARGUMENT);
	requireFailure(pValue, iIdentity, 0, XERR_ARGUMENT);
	testRequire(xrtValueTypeIdBind(pValue, iIdentity), "prior equal identity");
	requireFailure(pValue, iIdentity + 1, iDomain, XERR_STATE);
	xrtSetErrorKind(XERR_RANGE);
	const xerror* pPrior = xrtGetError();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(xrtMemDebugFailAfter(0), "arm allocation-free binding");
	#endif
	testRequire(xrtValueScalarDomainBindV1(pValue, iIdentity, iDomain) &&
		xrtGetError() == pPrior, "domain bind changed prior error");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(!xrtMemDebugFailTriggered(), "binding allocated");
		xrtMemDebugFailClear();
	#endif
	xrtClearError();
	pShared = xrtValueClone(pValue);
	testRequire(pShared == pValue && xrtValueScalarDomainBindV1(pValue, iIdentity, iDomain),
		"shared exact binding not idempotent");
	requireFailure(pValue, iIdentity + 1, iDomain, XERR_STATE);
	requireFailure(pValue, iIdentity, iDomain - 1, XERR_STATE);
	xrtValueRelease(pShared);
	testRequire(!xrtValueTypeIdRebind(pValue, iIdentity + 1) &&
		xrtErrorKind(xrtGetError()) == XERR_STATE, "domain allowed identity replacement");
	xrtClearError();
	testRequire(xrtValueTypeIdRebind(pValue, iIdentity) &&
		xrtValueTypeIdBind(pValue, iIdentity), "domain refused equal identity");
	if (bHashable) testRequire(xrtValueHash(pValue, &iHashAfter) && iHashBefore == iHashAfter,
		"opaque domain changed native hash");
	#if defined(XRT_FEATURE_VALUE_GRAPH)
		pShared = xrtValueDeepClone(pValue);
		testRequire(pShared == pValue && xrtValueTypeId(pShared) == iIdentity &&
			xrtValueScalarDomainV1(pShared) == iDomain, "scalar deep clone lost domain");
		xrtValueRelease(pShared);
		xvalue* pArray = xrtValueArray();
		testRequire(pArray && xrtValueArrayAppend(pArray, pValue), "nested scalar");
		xvalue* pCopy = xrtValueDeepClone(pArray);
		testRequire(pCopy && xrtValueScalarDomainV1(xrtValueArrayGet(pCopy, 0)) == iDomain &&
			xrtValueTypeId(xrtValueArrayGet(pCopy, 0)) == iIdentity, "nested deep clone lost domain");
		xrtValueRelease(pCopy); xrtValueRelease(pArray);
	#endif
	testRequire(xrtValueTypeId(pValue) == iIdentity &&
		xrtValueScalarDomainV1(pValue) == iDomain, "domain metadata changed");
	xrtValueRelease(pValue);
}

int main(void)
{
	xrtClearError();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		testRequire(xrtMemDebugEnable(true), "memory ledger");
		xrtSetErrorKind(XERR_STATE); xrtClearError();
		xmemdebugsnapshot Before;
		xrtMemDebugSnapshot(&Before);
	#endif
	testRequire(xrtValueScalarDomainV1(NULL) == 0 && xrtGetError() == NULL, "null query");
	requireFailure(NULL, 1, 1, XERR_ARGUMENT);
	requireFailure(xrtValueNull(), 1, 1, XERR_STATE);
	requireFailure(xrtValueBool(true), 1, 1, XERR_STATE);
	xvalue* pDirect = xrtValueInt(42);
	testRequire(pDirect && xrtValueScalarDomainBindV1(pDirect, UINT64_MAX, UINT32_MAX) &&
		xrtValueTypeId(pDirect) == UINT64_MAX && xrtValueScalarDomainV1(pDirect) == UINT32_MAX,
		"direct pair/maximal opaque tags");
	xrtValueRelease(pDirect);
	scalar(xrtValueInt(-128)); scalar(xrtValueUInt(UINT64_MAX));
	scalar(xrtValueFloat(1.25)); scalar(xrtValueChar(0x1F600));
	scalar(xrtValueTime(7)); scalar(xrtValuePointer(NULL));
	scalar(xrtValueString(XRT_STR_LITERAL("a\0b")));
	scalar(xrtValueBytes((xbytesview){(cbytes)"a\0b", 3}));
	xvalue* pTarget = xrtValueInt(5);
	xvalue* pHandle = xrtValueWeakRef(pTarget);
	testRequire(pHandle != NULL, "handle fixture");
	requireFailure(pHandle, 1, 1, XERR_TYPE);
	xrtValueRelease(pHandle); xrtValueRelease(pTarget);
	#if defined(XRT_IMPLEMENTATION)
		/* Tests may inspect private layout; applications must not do so. Compare
		 * the actual 64-bit ABI against its pre-tag published build layout. */
		if (sizeof(void*) == 8) {
			testRequire(sizeof(xvalue) == 88 && offsetof(xvalue, TypeId) == 16 &&
				offsetof(xvalue, Data) == 64, "64-bit value shell ABI grew or moved");
		}
		pTarget = xrtValueInt(4);
		const xvalue* pBorrowed[] = {pTarget};
		testRequire(__xrtValueCallbackProtect(pBorrowed, 1), "busy fixture");
		xrtClearError();
		testRequire(xrtValueScalarDomainV1(pTarget) == 0 &&
			xrtErrorKind(xrtGetError()) == XERR_STATE, "busy query not checked");
		xrtClearError();
		testRequire(!xrtValueScalarDomainBindV1(pTarget, 1, 1) &&
			xrtErrorKind(xrtGetError()) == XERR_STATE, "busy bind not checked");
		__xrtValueCallbackUnprotect(pBorrowed, 1);
		xrtClearError();
		testRequire(xrtValueTypeId(pTarget) == 0 && xrtValueScalarDomainV1(pTarget) == 0,
			"busy failure changed metadata");
		xrtValueRelease(pTarget);
	#endif
	#if defined(XRT_FEATURE_VALUE_CONTAINER)
		xvalue* pContainers[] = {xrtValueArray(), xrtValueObject(), xrtValueSet(), xrtValueIntMap()};
		for (size_t i = 0; i < sizeof(pContainers)/sizeof(*pContainers); ++i) {
			testRequire(pContainers[i] != NULL, "container fixture");
			requireFailure(pContainers[i], 1, 1, XERR_TYPE);
			xrtValueRelease(pContainers[i]);
		}
	#endif
	xrtClearError();
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		xmemdebugsnapshot After;
		xrtMemDebugSnapshot(&After);
		testRequire(After.LiveCount == Before.LiveCount && After.LiveBytes == Before.LiveBytes &&
			After.AllocCount - Before.AllocCount == After.FreeCount - Before.FreeCount &&
			After.InvalidFreeCount == Before.InvalidFreeCount &&
			After.DoubleFreeCount == Before.DoubleFreeCount &&
			After.UseAfterFreeCount == Before.UseAfterFreeCount, "scalar domain memory imbalance");
	#endif
	puts("[PASS] scalar domain: immutable identity pair, all scalar carriers, clone and rebind");
	return 0;
}
