#include "../test.h"
#include <stdatomic.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

static size_t Drops;
static bool DropFailed;
static void fieldsDrop(ptr pData, ptr pContext)
{
	(void)pContext;
	xrtownershipscope Freeze = {0};
	/* Physical callbacks must not inherit the collector's freeze/mutation. */
	testRequire(xrtOwnershipFreezeTryBegin(&Freeze), "drop inherited ownership scope");
	testRequire(xrtOwnershipScopeEnd(&Freeze), "drop freeze end failed");
	Drops++;
	xrtDynamicFieldsUnref(pData);
	if (DropFailed) xrtSetErrorKind(XERR_IO);
}
static const xvaluehandleops FieldsOps = {.Drop = fieldsDrop};
static bool fieldsTrace(const xvalue* pValue, xrtownershipvisitor pVisit, ptr pContext)
{
	ptr pFields = NULL;
	const xvaluehandleops* pOps = NULL;
	if (!xrtValueGetHandle(pValue, &pFields, &pOps, NULL) || pOps != &FieldsOps || !pFields) return false;
	return pVisit(xrtDynamicFieldsOwnership(pFields), pContext);
}
static xvalue* fieldsValue(xrtdynamicfields* pFields)
{
	ptr pOwned = xrtDynamicFieldsRef(pFields);
	xvalue* pValue = xrtValueHandleTake(&pOwned, &FieldsOps, NULL);
	testRequire(pValue && !pOwned && xrtValueHandleOwnershipBindPhased(pValue, fieldsTrace), "fields handle setup");
	return pValue;
}
static bool visitReject(xrtownershipref Ref, ptr pData)
{ (void)Ref; (void)pData; return false; }
static bool visitCount(xrtownershipref Ref, ptr pData)
{ testRequire(Ref.Data != NULL, "null native owning edge"); (*(size_t*)pData)++; return true; }
static void foreignStorageDrop(xbytesview Key, ptr pValue, ptr pData)
{ (void)Key; (void)pValue; (void)pData; testRequire(false, "foreign storage callback invoked during admission"); }
static void beginFreeze(xrtownershipscope* pFreeze)
{ testRequire(xrtOwnershipFreezeTryBegin(pFreeze), "ownership freeze failed"); }
static void endFreeze(xrtownershipscope* pFreeze)
{ testRequire(xrtOwnershipScopeEnd(pFreeze), "ownership freeze end failed"); }

static void testPhysicalSlots(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xvalue* pValue = xrtValueArray();
	testRequire(pFields && pValue && xrtValueArrayAppendNew(pValue, xrtValueInt(42)) &&
		xrtDynamicFieldsSetRef(pFields, XRT_STR_LITERAL("a"), pValue) &&
		xrtDynamicFieldsSetRef(pFields, XRT_STR_LITERAL("b\0c"), pValue), "shared slot setup");
	xrtownershipref Ref = xrtDynamicFieldsOwnership(pFields);
	xrttype Lookalike = *xrtDynamicFieldsType();
	testRequire(xrtDynamicFieldsType()->InstanceSize == sizeof(xtypeddict) &&
		xrtTypedDictTypeValidate(xrtDynamicFieldsType()) && !xrtTypedDictTypeValidate(&Lookalike),
		"payload grew or structural type impersonation admitted");
	xrtClearError();
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
	const xrtownershipadapterv1* pAdapter = xrtDynamicFieldsOwnershipAdapterV1(Ref);
	size_t iEdges = 0u, iCount = 0u;
	testRequire(pAdapter && Ref.Ops->Count(Ref.Data, &iCount) && iCount == 1u &&
		Ref.Ops->Trace(Ref.Data, visitCount, &iEdges) && iEdges == 2u &&
		!Ref.Ops->Trace(Ref.Data, visitReject, NULL), "physical slot multiplicity or rejected visitor");
	/* Identity-before-layout: the native admission never reads this address. */
	testRequire(!xrtDynamicFieldsOwnershipAdapterV1((xrtownershipref){(void*)1, xrtValueOwnership(pValue).Ops}) &&
		!xrtDynamicFieldsOwnershipAdapterV1(xrtObjectOwnership(pFields)), "foreign ownership admission");
	xtypeddict* pRaw = xrtObjectData(pFields);
	xmapdrop pDrop = pRaw->Storage.Drop;
	ptr pDropData = pRaw->Storage.DropUserData;
	/* Raw payload corruption is deliberate and enclosed by the same freeze.
	 * The public SetDrop correctly refuses changing a nonempty map's policy. */
	pRaw->Storage.Drop = foreignStorageDrop; pRaw->Storage.DropUserData = (ptr)1;
	testRequire(!xrtDynamicFieldsOwnershipAdapterV1(Ref) && !Ref.Ops->Count(Ref.Data, &iCount) &&
		!Ref.Ops->Trace(Ref.Data, visitCount, &iEdges), "foreign storage policy admitted");
	pRaw->Storage.Drop = pDrop; pRaw->Storage.DropUserData = pDropData;
	testRequire(xrtDynamicFieldsOwnershipAdapterV1(Ref) != NULL, "resident storage policy not restored");
	xrtownershipresult Result = {0};
	testRequire(xrtOwnershipInspect(&Ref, 1u, &Ref, 1u, &Result) && Result.ExternalRootCount == 1u,
		"external Value owner was lost or array backing flattened");
	endFreeze(&Freeze);
	xrtValueRelease(pValue);
	memset(&Freeze, 0, sizeof(Freeze)); beginFreeze(&Freeze);
	testRequire(xrtOwnershipInspect(&Ref, 1u, &Ref, 1u, &Result) && Result.ExternalRootCount == 0u &&
		Result.NodeCount == 4u && Result.EdgeCount == 5u, "native fields/Value/backing physical graph");
	endFreeze(&Freeze);
	xrtDynamicFieldsUnref(pFields);
}

static void testCycleAndClaims(void)
{
	size_t iPrior = Drops;
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtweak Weak = {0};
	xvalue* pValue = fieldsValue(pFields);
	testRequire(xrtWeakInit(&Weak, pFields) &&
		xrtDynamicFieldsSetRef(pFields, XRT_STR_LITERAL("self"), pValue), "cycle setup");
	xrtValueRelease(pValue); xrtDynamicFieldsUnref(pFields);
	xrtownershipref Refs[] = {xrtDynamicFieldsOwnership(pFields), xrtValueOwnership(pValue)};
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
	xrtownershipresult Result = {0};
	testRequire(xrtOwnershipInspect(Refs, 2u, NULL, 0u, &Result) &&
		Result.NodeCount == 2u && Result.EdgeCount == 2u && !Result.ExternalRootCount, "cycle roots or edges");
	const xrtownershipadapterv1* Adapters[] = {xrtDynamicFieldsOwnershipAdapterV1(Refs[0]),
		xrtValueHandleOwnershipAdapterV1(Refs[1], &FieldsOps, fieldsTrace)};
	testRequire(Adapters[0] && Adapters[1] && Adapters[0]->Hold(pFields) && Adapters[1]->Hold(pValue), "cycle pins");
	int Token = 0, Other = 0;
	testRequire(Adapters[0]->Claim(pFields, &Token) && Adapters[1]->Claim(pValue, &Token) &&
		!Adapters[0]->Claim(pFields, &Other) && !xrtWeakLock(&Weak), "claim quarantine");
	endFreeze(&Freeze);
	testRequire(!xrtWeakLock(&Weak), "weak promotion escaped claim");
	/* A real existing strong owner may resurrect before commit. */
	xrtobject* pResurrected = xrtObjectRef(pFields);
	testRequire(pResurrected != NULL, "claim wrongly blocked strong owner");
	xrtObjectUnref(pResurrected);
	memset(&Freeze, 0, sizeof(Freeze)); beginFreeze(&Freeze);
	Adapters[0]->Restore(pFields, &Token); Adapters[1]->Restore(pValue, &Token);
	xrtobject* pLocked = xrtWeakLock(&Weak);
	testRequire(pLocked != NULL, "abort failed to restore weak promotion");
	xrtObjectUnref(pLocked);
	testRequire(xrtDynamicFieldsGet(pFields, XRT_STR_LITERAL("self")) == pValue &&
		xrtOwnershipInspect(Refs, 2u, Refs, 2u, &Result) && !Result.ExternalRootCount, "abort fields/pin counts");
	testRequire(Adapters[0]->Claim(pFields, &Token) && Adapters[1]->Claim(pValue, &Token), "second claim");
	Adapters[0]->Clear(pFields, &Token); Adapters[1]->Clear(pValue, &Token);
	testRequire(!xrtDynamicFieldsOwnershipAdapterV1(Refs[0]) && !xrtWeakLock(&Weak) &&
		!xrtObjectRef(pFields), "cleared admission still live");
	xrtClearError(); endFreeze(&Freeze);
	testRequire(Drops == iPrior && Adapters[0]->Finish(pFields, &Token) &&
		Adapters[0]->Finish(pFields, &Token) && Adapters[1]->Finish(pValue, &Token) &&
		Adapters[1]->Finish(pValue, &Token) && Drops == iPrior + 1u, "phased repeat or early physical drop");
	Adapters[1]->Drop(pValue); Adapters[0]->Drop(pFields);
	testRequire(xrtWeakExpired(&Weak), "cycle native control stayed live");
	xrtWeakUnit(&Weak);
}

static void failDrop(ptr pData, ptr pContext)
{
	(void)pData; (void)pContext;
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze); endFreeze(&Freeze);
	Drops++; xrtSetErrorKind(XERR_IO);
}
static const xvaluehandleops FailureOps = {.Drop = failDrop};
static bool leafTrace(const xvalue* pValue, xrtownershipvisitor pVisit, ptr pData)
{ (void)pValue; (void)pData; return pVisit != NULL; }
static void testFinishFailure(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	ptr pOwned = (ptr)1;
	xvalue* pValue = xrtValueHandleTake(&pOwned, &FailureOps, NULL);
	testRequire(pFields && pValue && xrtValueHandleOwnershipBindPhased(pValue, leafTrace) &&
		xrtDynamicFieldsSetRef(pFields, XRT_STR_LITERAL("drop"), pValue), "failure setup");
	xrtValueRelease(pValue);
	xrtownershipref Ref = xrtDynamicFieldsOwnership(pFields);
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
	const xrtownershipadapterv1* pAdapter = xrtDynamicFieldsOwnershipAdapterV1(Ref);
	int Token = 0;
	testRequire(pAdapter && pAdapter->Claim(pFields, &Token), "failure claim");
	pAdapter->Clear(pFields, &Token); endFreeze(&Freeze);
	size_t iPrior = Drops;
	xrtSetErrorKind(XERR_RANGE);
	testRequire(!pAdapter->Finish(pFields, &Token) && Drops == iPrior + 1u &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE, "first cleanup failure lost");
	xrtClearError();
	testRequire(pAdapter->Finish(pFields, &Token) && Drops == iPrior + 1u && !xrtGetError(), "cleanup retry repeated drop");
	xrtDynamicFieldsUnref(pFields);
}

static void testLegacyGraphRefusal(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	xrtobjectgraph* pGraph = xrtObjectGraphCreate();
	testRequire(pFields && pGraph && xrtObjectGraphTrack(pGraph, pFields), "legacy graph setup");
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
	testRequire(!xrtDynamicFieldsOwnershipAdapterV1(xrtDynamicFieldsOwnership(pFields)), "legacy collector admitted into phased graph");
	endFreeze(&Freeze);
	xrtDynamicFieldsUnref(pFields); xrtObjectGraphDestroy(pGraph);
}

#if defined(XRT_FEATURE_MEMORY_DEBUG)
static void testDiscoveryOom(void)
{
	xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
	testRequire(pFields && xrtDynamicFieldsSetRefNew(pFields, XRT_STR_LITERAL("a"), xrtValueInt(7)), "OOM graph setup");
	xrtownershipref Ref = xrtDynamicFieldsOwnership(pFields);
	bool bFailed = false, bSucceeded = false;
	for (uint64 i = 0; i < 256u; i++) {
		xrtClearError();
		xmemdebugsnapshot Before, After;
		xrtMemDebugSnapshot(&Before);
		xrtownershipresult Result = {71u, 72u, 73u, 74u};
		xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
		testRequire(xrtMemDebugFailAfter(i), "OOM arm");
		bool bOk = xrtOwnershipInspect(&Ref, 1u, &Ref, 1u, &Result);
		bool bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear(); endFreeze(&Freeze);
		if (!bOk) {
			bFailed = true;
			testRequire(bTriggered && Result.NodeCount == 71u && Result.EdgeCount == 72u &&
				Result.ExternalRootCount == 73u && Result.ReachableAnchorCount == 74u, "OOM changed output");
		} else {
			bSucceeded = true;
			testRequire(!bTriggered && Result.NodeCount == 2u && Result.EdgeCount == 2u && !Result.ExternalRootCount, "OOM success graph");
		}
		xrtClearError(); xrtMemDebugSnapshot(&After);
		testRequire(Before.LiveCount == After.LiveCount && Before.LiveBytes == After.LiveBytes &&
			xrtDynamicFieldsCount(pFields) == 1u, "OOM leaked or mutated native fields");
		if (bOk) break;
	}
	testRequire(bFailed && bSucceeded, "incomplete graph OOM prefix");
	xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
	const xrtownershipadapterv1* pAdapter = xrtDynamicFieldsOwnershipAdapterV1(Ref);
	int Token = 0;
	testRequire(xrtMemDebugFailAfter(0u) && pAdapter && pAdapter->Hold(pFields) &&
		pAdapter->Claim(pFields, &Token), "no-allocation claim setup");
	pAdapter->Restore(pFields, &Token);
	testRequire(pAdapter->Claim(pFields, &Token), "no-allocation second claim");
	pAdapter->Clear(pFields, &Token);
	testRequire(!xrtMemDebugFailTriggered(), "hold/claim/restore/clear allocated");
	endFreeze(&Freeze);
	testRequire(pAdapter->Finish(pFields, &Token) && !xrtMemDebugFailTriggered(), "physical finish allocated");
	xrtMemDebugFailClear(); pAdapter->Drop(pFields); xrtDynamicFieldsUnref(pFields);
}
#endif

typedef struct Worker {
	xrtdynamicfields* Fields;
	xrtweak* Weak;
	xvalue* Value;
	int Mode;
	atomic_int State;
} Worker;
static void delay(void)
{
#if defined(_WIN32)
	Sleep(1);
#else
	struct timespec t = {0, 1000000}; nanosleep(&t, NULL);
#endif
}
#if defined(_WIN32)
static DWORD WINAPI runWorker(LPVOID pData)
#else
static void* runWorker(void* pData)
#endif
{
	Worker* pWorker = pData;
	atomic_store(&pWorker->State, 1);
	if (pWorker->Mode == 1) testRequire(xrtDynamicFieldsSetRef(pWorker->Fields, XRT_STR_LITERAL("worker"), pWorker->Value), "worker set");
	else if (pWorker->Mode == 2) {
		xrtobject* pRef = xrtObjectRef(pWorker->Fields);
		testRequire(pRef != NULL, "worker ref"); xrtObjectUnref(pRef);
	} else if (pWorker->Mode == 3) {
		xrtobject* pRef = xrtWeakLock(pWorker->Weak);
		testRequire(pRef != NULL, "worker weak lock"); xrtObjectUnref(pRef);
	} else xrtObjectUnref(pWorker->Fields);
	atomic_store(&pWorker->State, 2);
	return 0;
}
static void testFreezeExclusion(void)
{
	for (int iMode = 1; iMode <= 4; iMode++) {
		xrtdynamicfields* pFields = xrtDynamicFieldsCreate();
		xvalue* pValue = xrtValueInt(42);
		xrtweak Weak = {0};
		testRequire(pFields && pValue && xrtWeakInit(&Weak, pFields), "freeze worker setup");
		if (iMode == 4) testRequire(xrtObjectRef(pFields) != NULL, "unref worker extra owner");
		Worker Task = {pFields, &Weak, pValue, iMode, ATOMIC_VAR_INIT(0)};
		xrtownershipscope Freeze = {0}; beginFreeze(&Freeze);
#if defined(_WIN32)
		HANDLE Thread = CreateThread(NULL, 0, runWorker, &Task, 0, NULL);
		testRequire(Thread != NULL, "native thread create");
#else
		pthread_t Thread;
		testRequire(pthread_create(&Thread, NULL, runWorker, &Task) == 0, "native thread create");
#endif
		for (int i = 0; i < 5000 && !atomic_load(&Task.State); i++) delay();
		for (int i = 0; i < 20; i++) delay();
		testRequire(atomic_load(&Task.State) == 1 && xrtObjectRefCount(pFields) == (iMode == 4 ? 2u : 1u), "native mutation crossed freeze");
		endFreeze(&Freeze);
#if defined(_WIN32)
		testRequire(WaitForSingleObject(Thread, 5000) == WAIT_OBJECT_0, "native thread finish"); CloseHandle(Thread);
#else
		testRequire(pthread_join(Thread, NULL) == 0, "native thread finish");
#endif
		testRequire(atomic_load(&Task.State) == 2 && xrtObjectRefCount(pFields) == 1u, "native mutation failed after freeze");
		xrtValueRelease(pValue); xrtDynamicFieldsUnref(pFields); xrtWeakUnit(&Weak);
	}
}

int main(void)
{
	testPhysicalSlots(); testCycleAndClaims(); testFinishFailure(); testLegacyGraphRefusal(); testFreezeExclusion();
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	testDiscoveryOom();
#endif
	xrtClearError();
#if defined(XRT_FEATURE_MEMORY_DEBUG)
	xmemdebugsnapshot Final; xrtMemDebugSnapshot(&Final);
	testRequire(Final.LiveCount == 0u && Final.LiveBytes == 0u, "final physical ownership allocation ledger");
	testMemoryDebugDrain("final debug storage drain");
#endif
	printf("[PASS] runtime dynamic field physical ownership\n"); return 0;
}
