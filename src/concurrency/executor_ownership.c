#include "../internal/xrt_executor.h"

#if defined(XRT_FEATURE_EXECUTOR)

void __xrtExecutorMutationBegin(xrtownershipscope* pScope)
{ if (!xrtOwnershipMutationBegin(pScope)) abort(); }
void __xrtExecutorMutationEnd(xrtownershipscope* pScope)
{ if (!xrtOwnershipScopeEnd(pScope)) abort(); }
void __xrtExecutorOwnershipBegin(xexecutor* pExecutor, xrtownershipscope* pScope)
{
	__xrtExecutorMutationBegin(pScope);
	if (!xrtMutexLock(&pExecutor->OwnershipLock)) abort();
}
void __xrtExecutorOwnershipEnd(xexecutor* pExecutor, xrtownershipscope* pScope)
{
	if (!xrtMutexUnlock(&pExecutor->OwnershipLock)) abort();
	__xrtExecutorMutationEnd(pScope);
}

/* OwnershipLock never encloses a queue lock, wait, or user callback. API
 * entries keep a real reference until their LAST access, including notify. */
bool __xrtExecutorEnter(xexecutor* pExecutor, bool bTerminal)
{
	xrtownershipscope Mutation = {0}; bool bEntered = false;
	if (!pExecutor) { __xrtErrorSetInvalidArgument(); return false; }
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	if (pExecutor->Initialized && pExecutor->References && pExecutor->References < SIZE_MAX &&
		pExecutor->Entries < SIZE_MAX && !pExecutor->Retiring && !pExecutor->Joining &&
		(!pExecutor->Retired || bTerminal) && (!pExecutor->OwnershipCleared || bTerminal)) {
		++pExecutor->References; ++pExecutor->Entries; bEntered = true;
	}
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	if (!bEntered) __xrtErrorSetInvalidState();
	return bEntered;
}
static void __xrtExecutorDropLocked(xexecutor* pExecutor, xrtownershipscope* pScope)
{
	if (!pExecutor->References) abort();
	bool bLast = --pExecutor->References == 0;
	if (bLast && (!pExecutor->Retired || pExecutor->OwnerHeld || pExecutor->Entries ||
		pExecutor->LiveJobs || pExecutor->Callbacks || pExecutor->StartedThreads ||
		pExecutor->ExitData || pExecutor->ExitPolicy)) abort();
	if (!xrtMutexUnlock(&pExecutor->OwnershipLock)) abort();
	if (bLast) {
		for (uint32 i = 0; i < pExecutor->ThreadCount; ++i)
			__xrtSpinUnit(&pExecutor->Workers[i].Lock);
		if (!xrtCondUnit(&pExecutor->Idle) || !xrtCondUnit(&pExecutor->Work) ||
			!xrtMutexUnit(&pExecutor->SleepLock) || !xrtMutexUnit(&pExecutor->OwnershipLock)) abort();
		xrtFree(pExecutor);
	}
	__xrtExecutorMutationEnd(pScope);
}
void __xrtExecutorRelease(const void* pData)
{
	xexecutor* pExecutor = (xexecutor*)pData; xrtownershipscope Mutation = {0};
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	__xrtExecutorDropLocked(pExecutor, &Mutation);
}
void __xrtExecutorLeave(xexecutor* pExecutor)
{
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	if (!pExecutor->Entries) abort();
	--pExecutor->Entries; __xrtExecutorDropLocked(pExecutor, &Mutation);
}
bool __xrtExecutorReserveJobs(xexecutor* pExecutor, size_t iCount)
{
	xrtownershipscope Mutation = {0}; bool bHeld = false;
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	if (iCount <= SIZE_MAX - pExecutor->References && iCount <= SIZE_MAX - pExecutor->LiveJobs) {
		pExecutor->References += iCount; pExecutor->LiveJobs += iCount; bHeld = true;
	}
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	if (!bHeld) __xrtErrorSetSizeOverflow();
	return bHeld;
}
void __xrtExecutorUnreserveJobs(xexecutor* pExecutor, size_t iCount)
{
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	/* The active submit entry still holds the shell; no final disposal here. */
	if (pExecutor->LiveJobs < iCount || pExecutor->References <= iCount) abort();
	pExecutor->LiveJobs -= iCount; pExecutor->References -= iCount;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
}

void __xrtExecutorJobRelease(const void* pData)
{
	xrt_executor_job* pJob = (xrt_executor_job*)pData; xrtownershipscope Mutation = {0};
	__xrtExecutorMutationBegin(&Mutation);
	int32 iRefs = xrtRefRelease(&pJob->RefCount);
	if (iRefs < 0) abort();
	if (iRefs == 0) {
		xrt_executor_worker* pOwner = pJob->Owner; xexecutor* pExecutor = pOwner->Executor;
		if (!pJob->Finished || pJob->Active || pJob->Data || pJob->Destroy || pJob->DestroyContext) abort();
		/* The last pin, not just execution completion, permits slot reuse. Keep
		 * embedded locks alive until this slot's real containing ref is dropped. */
		pJob->Proc = NULL; pJob->DataPolicy = NULL; pJob->OwnershipClaim = NULL;
		pJob->OwnershipCleared = false; pJob->Finished = false; pJob->Previous = NULL;
		__xrtSpinLock(&pOwner->Lock);
		pJob->Next = pOwner->Free; pOwner->Free = pJob; ++pOwner->FreeCount;
		__xrtSpinUnlock(&pOwner->Lock);
		xrtownershipscope Ownership = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Ownership);
		if (!pExecutor->LiveJobs) abort();
		--pExecutor->LiveJobs; __xrtExecutorDropLocked(pExecutor, &Ownership);
	}
	__xrtExecutorMutationEnd(&Mutation);
}

bool __xrtExecutorIdle(const xexecutor* pExecutor)
{ return xrtAtomic32Load(&pExecutor->Outstanding, XMEMORY_ACQUIRE) == 0; }

bool __xrtExecutorRetire(xexecutor* pExecutor, size_t iEntries)
{
	xrtownershipscope Mutation = {0}; __xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	if (pExecutor->Retired) { __xrtExecutorOwnershipEnd(pExecutor, &Mutation); return true; }
	if (!pExecutor->Joined || pExecutor->Joining || pExecutor->Retiring || pExecutor->Entries != iEntries ||
		pExecutor->Callbacks || !__xrtExecutorIdle(pExecutor)) {
		__xrtExecutorOwnershipEnd(pExecutor, &Mutation); return false;
	}
	pExecutor->Retiring = true; uint32 iThreads = pExecutor->StartedThreads;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	for (uint32 i = 0; i < iThreads; ++i) {
		xrt_executor_worker* pWorker = &pExecutor->Workers[i];
		if (!pWorker->Exited || pWorker->Parked) abort();
		xrtThreadDestroy(pWorker->Thread); pWorker->Thread = NULL;
	}
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	ptr pExitData = pExecutor->ExitData;
	const xexecutorworkerexitownershipv1* pExitPolicy = pExecutor->ExitPolicy;
	pExecutor->ExitData = NULL; pExecutor->ExitPolicy = NULL;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	if (pExitPolicy) pExitPolicy->Drop(pExitData, NULL);
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	pExecutor->StartedThreads = 0; pExecutor->Retired = true; pExecutor->Retiring = false;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation); return true;
}

static bool __xrtExecutorOwnershipCount(const void* pData, size_t* pCount)
{
	const xexecutor* pExecutor = pData;
	if (!pExecutor || !pCount || !pExecutor->Initialized || !pExecutor->References ||
		pExecutor->Entries || pExecutor->Callbacks || pExecutor->Destroying || pExecutor->Joining ||
		pExecutor->Retiring || pExecutor->OwnershipCleared || pExecutor->LiveJobs > pExecutor->SlotCount ||
		pExecutor->References < pExecutor->LiveJobs ||
		xrtAtomic32Load(&pExecutor->Running, XMEMORY_ACQUIRE) ||
		xrtAtomic32Load(&pExecutor->Submitters, XMEMORY_ACQUIRE)) return false;
	size_t iQueued = xrtAtomic32Load(&pExecutor->Queued, XMEMORY_ACQUIRE);
	if (iQueued != xrtAtomic32Load(&pExecutor->Outstanding, XMEMORY_ACQUIRE)) return false;
	if (pExecutor->Retired) {
		if (!pExecutor->Joined || pExecutor->StartedThreads || iQueued ||
			!xrtAtomic32Load(&pExecutor->Closed, XMEMORY_ACQUIRE)) return false;
	} else if (pExecutor->StartedThreads != pExecutor->ThreadCount) return false;
	size_t iSeen = 0; uint32 iExited = 0;
	for (uint32 i = 0; i < pExecutor->ThreadCount; ++i) {
		const xrt_executor_worker* pWorker = &pExecutor->Workers[i];
		if (pWorker->Executor != pExecutor || (pExecutor->Retired ? pWorker->Thread != NULL : pWorker->Thread == NULL) ||
			(pWorker->Exited ? pWorker->Parked : pExecutor->Joined)) return false;
		if (pWorker->Exited) ++iExited;
		size_t iJobs = 0; const xrt_executor_job* pTail = NULL;
		for (const xrt_executor_job* pJob = pWorker->Head; pJob; pJob = pJob->Next) {
			if (iJobs++ == pWorker->Queued || pJob->Owner != pWorker || pJob->Active ||
				pJob->Finished || pJob->OwnershipCleared || pJob->Previous != pTail ||
				__xrtAtomicRefLoad(&pJob->RefCount) <= 0) return false;
			pTail = pJob;
		}
		if (iJobs != pWorker->Queued || pTail != pWorker->Tail || iJobs > iQueued - iSeen) return false;
		iSeen += iJobs;
	}
	if (iSeen != iQueued || iExited != xrtAtomic32Load(&pExecutor->ExitedWorkers, XMEMORY_ACQUIRE) ||
		(pExecutor->ExitData == NULL) != (pExecutor->ExitPolicy == NULL)) return false;
	*pCount = pExecutor->References; return true;
}
static bool __xrtExecutorOwnershipTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xexecutor* pExecutor = pData; size_t iCount;
	if (!pVisit || !__xrtExecutorOwnershipCount(pData, &iCount)) return false;
	if (pExecutor->ExitPolicy && !pVisit((xrtownershipref){pExecutor->ExitData, pExecutor->ExitPolicy->Ops}, pContext)) return false;
	for (uint32 i = 0; i < pExecutor->ThreadCount; ++i)
		for (const xrt_executor_job* pJob = pExecutor->Workers[i].Head; pJob; pJob = pJob->Next)
			if (!pVisit(__xrtExecutorJobOwnership(pJob), pContext)) return false;
	return true;
}
static const xrtownershipops __xrtExecutorOwnershipOps = {
	__xrtExecutorOwnershipCount, __xrtExecutorOwnershipTrace
};
XRT_API xrtownershipref xrtExecutorOwnership(const xexecutor* pExecutor)
{ return (xrtownershipref){pExecutor, pExecutor ? &__xrtExecutorOwnershipOps : NULL}; }
static bool __xrtExecutorOwnershipBusy(const void* pData)
{
	const xexecutor* pExecutor = pData;
	return pExecutor && pExecutor->Initialized && pExecutor->References &&
		!pExecutor->OwnershipCleared && !pExecutor->Retired &&
		(pExecutor->Entries || pExecutor->Callbacks || pExecutor->Destroying || pExecutor->Joining ||
		 pExecutor->Retiring || xrtAtomic32Load(&pExecutor->Running, XMEMORY_ACQUIRE));
}
XRT_API const xrtownershipquiescencev1* xrtExecutorOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtExecutorOwnershipOps, __xrtExecutorOwnershipBusy};
	return &Policy;
}
static bool __xrtExecutorHold(const void* pData)
{
	xexecutor* pExecutor = (xexecutor*)pData; xrtownershipscope Mutation = {0}; bool bHeld = false;
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	if (pExecutor->References && pExecutor->References < SIZE_MAX && !pExecutor->OwnershipCleared) {
		++pExecutor->References; bHeld = true;
	}
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation); return bHeld;
}
static bool __xrtExecutorClaim(const void* pData, const void* pToken)
{
	xexecutor* pExecutor = (xexecutor*)pData;
	if (!pToken || pExecutor->OwnershipCleared ||
		(pExecutor->OwnershipClaim && pExecutor->OwnershipClaim != pToken)) return false;
	pExecutor->OwnershipClaim = pToken; return true;
}
static void __xrtExecutorRestore(const void* pData, const void* pToken)
{
	xexecutor* pExecutor = (xexecutor*)pData;
	if (!pToken || pExecutor->OwnershipClaim != pToken || pExecutor->OwnershipCleared) abort();
	pExecutor->OwnershipClaim = NULL;
}
static bool __xrtExecutorPreparationReady(const void* pData)
{
	const xexecutor* pExecutor = pData;
	return pExecutor->Joined && !pExecutor->Joining && !pExecutor->Destroying && !pExecutor->Retiring &&
		!pExecutor->Entries && !pExecutor->Callbacks && __xrtExecutorIdle(pExecutor) &&
		xrtAtomic32Load(&pExecutor->Closed, XMEMORY_ACQUIRE);
}
static xrtownershipprepareresult __xrtExecutorPrepare(const void* pData, const void* pToken)
{
	xexecutor* pExecutor = (xexecutor*)pData; xrtownershipscope Freeze = {0}, Mutation = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pExecutor->OwnershipClaim != pToken || pExecutor->OwnershipCleared) abort();
	bool bReady = __xrtExecutorPreparationReady(pData);
	bool bJoin = !pExecutor->Entries && !pExecutor->Callbacks && !pExecutor->Destroying &&
		!pExecutor->Joining && !pExecutor->Retiring;
	if (!bReady && bJoin) {
		xrtAtomic32Store(&pExecutor->Closed, 1, XMEMORY_SEQ_CST);
		bJoin = __xrtExecutorIdle(pExecutor);
		if (bJoin) xrtAtomic32Store(&pExecutor->Shutdown, 1, XMEMORY_RELEASE);
		__xrtExecutorWake(pExecutor, true);
		for (uint32 i = 0; i < pExecutor->StartedThreads; ++i)
			if (!pExecutor->Workers[i].Exited || pExecutor->Workers[i].Parked) bJoin = false;
		if (bJoin) pExecutor->Joining = true;
	}
	__xrtExecutorMutationEnd(&Freeze);
	if (bReady) return XRT_OWNERSHIP_PREPARE_READY;
	if (!bJoin) return XRT_OWNERSHIP_PREPARE_BUSY;
	bool bJoined = true, bFailed = false;
	for (uint32 i = 0; i < pExecutor->StartedThreads; ++i) {
		xwaitresult Result = xrtThreadWaitFor(pExecutor->Workers[i].Thread, 0);
		if (Result != XWAIT_OK) { bJoined = false; if (Result != XWAIT_TIMEOUT) bFailed = true; }
	}
	__xrtExecutorOwnershipBegin(pExecutor, &Mutation);
	pExecutor->Joining = false; pExecutor->Joined = bJoined;
	__xrtExecutorOwnershipEnd(pExecutor, &Mutation);
	return bFailed ? XRT_OWNERSHIP_PREPARE_FAILED : bJoined ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtExecutorClear(const void* pData, const void* pToken)
{
	xexecutor* pExecutor = (xexecutor*)pData;
	if (!pToken || pExecutor->OwnershipClaim != pToken || pExecutor->OwnershipCleared ||
		!__xrtExecutorPreparationReady(pData)) abort();
	pExecutor->OwnershipCleared = true;
}
static bool __xrtExecutorFinish(const void* pData, const void* pToken)
{
	xexecutor* pExecutor = (xexecutor*)pData;
	if (!pToken || pExecutor->OwnershipClaim != pToken || !pExecutor->OwnershipCleared) abort();
	return __xrtExecutorRetire(pExecutor, 0);
}
XRT_API const xrtownershipadapterv1* xrtExecutorOwnershipAdapterV2(
	xrtownershipref Reference, const xexecutorworkerexitownershipv1* const* pPolicies,
	size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtExecutorHold, __xrtExecutorRelease,
		__xrtExecutorClaim, __xrtExecutorRestore, NULL, __xrtExecutorClear, __xrtExecutorFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtExecutorPreparationReady, __xrtExecutorPrepare};
	size_t iCount;
	if (!ppPreparation || !Reference.Data || Reference.Ops != &__xrtExecutorOwnershipOps ||
		(iPolicyCount && !pPolicies)) return NULL;
	const xexecutor* pExecutor = Reference.Data;
	if (pExecutor->ExitPolicy) {
		bool bKnown = false;
		for (size_t i = 0; i < iPolicyCount; ++i) if (pPolicies[i] == pExecutor->ExitPolicy) { bKnown = true; break; }
		if (!bKnown) return NULL;
	}
	if (!__xrtExecutorOwnershipCount(Reference.Data, &iCount)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}
XRT_API const xrtownershipadapterv1* xrtExecutorOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation)
{ return xrtExecutorOwnershipAdapterV2(Reference, NULL, 0, ppPreparation); }

static bool __xrtExecutorJobCount(const void* pData, size_t* pCount)
{
	const xrt_executor_job* pJob = pData; int32 iRefs;
	if (!pJob || !pCount || !pJob->DataPolicy || pJob->Active || pJob->OwnershipCleared) return false;
	iRefs = __xrtAtomicRefLoad(&pJob->RefCount);
	if (iRefs <= 0) return false;
	*pCount = (size_t)iRefs; return true;
}
static bool __xrtExecutorJobTrace(const void* pData, xrtownershipvisitor pVisit, ptr pContext)
{
	const xrt_executor_job* pJob = pData; size_t iCount;
	return pVisit && __xrtExecutorJobCount(pData, &iCount) &&
		pVisit(xrtExecutorOwnership(pJob->Owner->Executor), pContext) &&
		(!pJob->Data || pVisit((xrtownershipref){pJob->Data, pJob->DataPolicy->Ops}, pContext));
}
static const xrtownershipops __xrtExecutorJobOps = {__xrtExecutorJobCount, __xrtExecutorJobTrace};
xrtownershipref __xrtExecutorJobOwnership(const xrt_executor_job* pJob)
{ return (xrtownershipref){pJob, pJob ? &__xrtExecutorJobOps : NULL}; }
static bool __xrtExecutorJobBusy(const void* pData)
{
	const xrt_executor_job* pJob = pData;
	return pJob && __xrtAtomicRefLoad(&pJob->RefCount) > 0 && !pJob->OwnershipCleared && pJob->Active;
}
XRT_API const xrtownershipquiescencev1* xrtExecutorJobOwnershipQuiescenceV1Get(void)
{
	static const xrtownershipquiescencev1 Policy = {sizeof(Policy), &__xrtExecutorJobOps, __xrtExecutorJobBusy};
	return &Policy;
}
static bool __xrtExecutorJobHold(const void* pData)
{
	xrt_executor_job* pJob = (xrt_executor_job*)pData; xrtownershipscope Mutation = {0};
	__xrtExecutorMutationBegin(&Mutation);
	bool bHeld = !pJob->OwnershipCleared && xrtRefRetain(&pJob->RefCount) >= 0;
	__xrtExecutorMutationEnd(&Mutation); return bHeld;
}
static bool __xrtExecutorJobClaim(const void* pData, const void* pToken)
{
	xrt_executor_job* pJob = (xrt_executor_job*)pData;
	if (!pToken || pJob->OwnershipCleared || (pJob->OwnershipClaim && pJob->OwnershipClaim != pToken)) return false;
	pJob->OwnershipClaim = pToken; return true;
}
static void __xrtExecutorJobRestore(const void* pData, const void* pToken)
{
	xrt_executor_job* pJob = (xrt_executor_job*)pData;
	if (!pToken || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared) abort();
	pJob->OwnershipClaim = NULL;
}
static bool __xrtExecutorJobReady(const void* pData)
{ const xrt_executor_job* pJob = pData; return pJob->Finished && !pJob->Active; }
static xrtownershipprepareresult __xrtExecutorJobPrepare(const void* pData, const void* pToken)
{
	const xrt_executor_job* pJob = pData; xrtownershipscope Freeze = {0};
	if (!xrtOwnershipFreezeTryBegin(&Freeze)) return XRT_OWNERSHIP_PREPARE_BUSY;
	if (!pToken || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared) abort();
	bool bReady = __xrtExecutorJobReady(pData); __xrtExecutorMutationEnd(&Freeze);
	return bReady ? XRT_OWNERSHIP_PREPARE_READY : XRT_OWNERSHIP_PREPARE_BUSY;
}
static void __xrtExecutorJobClear(const void* pData, const void* pToken)
{
	xrt_executor_job* pJob = (xrt_executor_job*)pData;
	if (!pToken || pJob->OwnershipClaim != pToken || pJob->OwnershipCleared || !__xrtExecutorJobReady(pData) ||
		pJob->Data || pJob->Destroy || pJob->DestroyContext) abort();
	pJob->OwnershipCleared = true;
}
static bool __xrtExecutorJobFinish(const void* pData, const void* pToken)
{
	const xrt_executor_job* pJob = pData;
	if (!pToken || pJob->OwnershipClaim != pToken || !pJob->OwnershipCleared) abort();
	return true;
}
XRT_API const xrtownershipadapterv1* xrtExecutorJobOwnershipAdapterV1(
	xrtownershipref Reference, const xexecutordataownershipv1* const* pPolicies,
	size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation)
{
	static const xrtownershipadapterv1 Adapter = {sizeof(Adapter), __xrtExecutorJobHold, __xrtExecutorJobRelease,
		__xrtExecutorJobClaim, __xrtExecutorJobRestore, NULL, __xrtExecutorJobClear, __xrtExecutorJobFinish};
	static const xrtownershippreparationv1 Preparation = {sizeof(Preparation), &Adapter,
		__xrtExecutorJobReady, __xrtExecutorJobPrepare};
	if (!Reference.Data || Reference.Ops != &__xrtExecutorJobOps || !ppPreparation ||
		(iPolicyCount && !pPolicies)) return NULL;
	const xrt_executor_job* pJob = Reference.Data;
	const xexecutordataownershipv1* pPolicy = pJob->DataPolicy; bool bKnown = false;
	for (size_t i = 0; i < iPolicyCount; ++i) if (pPolicies[i] && pPolicies[i] == pPolicy) { bKnown = true; break; }
	if (!bKnown || pPolicy->size != sizeof(*pPolicy) || !pPolicy->Proc || !pPolicy->Drop ||
		!pPolicy->Ops || !pPolicy->Ops->Count || !pPolicy->Ops->Trace || pJob->Proc != pPolicy->Proc ||
		pJob->Active || pJob->OwnershipCleared || __xrtAtomicRefLoad(&pJob->RefCount) <= 0) return NULL;
	if (pJob->Finished ? (pJob->Data || pJob->Destroy || pJob->DestroyContext) :
		(!pJob->Data || pJob->Destroy != pPolicy->Drop || pJob->DestroyContext)) return NULL;
	*ppPreparation = &Preparation; return &Adapter;
}

#endif
