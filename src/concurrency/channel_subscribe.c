#include "../internal/xrt_channel.h"
#include <xrt/channel_subscribe.h>

#if defined(XRT_FEATURE_CHANNEL_SUBSCRIBE)

#define XRT_CHANNEL_SUB_DRIVER 1u
#define XRT_CHANNEL_SUB_DIRTY 2u

struct xchannelsubscription {
	/* External refs and admitted public calls leave two credits for the one
	 * registration and the one coalesced dispatcher. Never wrap a counter. */
	xatomic32 References;
	xatomic32 External;
	xatomic32 Started;
	xatomic32 Ready;
	xatomic32 Stop;
	xatomic32 Dispatch;
	xrt_channel_deferred Work;
	xrt_channel_select_state Select;
	size_t Count;
	size_t Start;
	ptr Received;
	ptr Data;
	const xchannelsubscribepolicyv1* Policy;
	xchannelselectresult Result;
	xrt_channel_select_waiter Waiters[];
};

static void __xrtChannelSubscriptionDrive(xchannelsubscription* pSubscription);

static bool __xrtChannelSubscriptionRetain(xchannelsubscription* pSubscription,
	uint32 iReserve)
{
	uint32 iRefs=xrtAtomic32Load(&pSubscription->References,XMEMORY_RELAXED);
	while(iRefs && iRefs<UINT32_MAX-iReserve) {
		if(xrtAtomic32CompareExchange(&pSubscription->References,&iRefs,iRefs+1u,
			XMEMORY_RELAXED,XMEMORY_RELAXED)) return true;
	}
	__xrtErrorSetSizeOverflow();
	return false;
}

static void __xrtChannelSubscriptionRelease(xchannelsubscription* pSubscription)
{
	if(xrtAtomic32FetchSub(&pSubscription->References,1u,XMEMORY_ACQ_REL)==1u) {
		/* No mutex or public mutation scope is held across owner code. */
		pSubscription->Policy->Drop(pSubscription->Data);
		xrtFree(pSubscription);
	}
}

/* Claim a single dispatcher before publishing its intrusive node. All wakes
 * during registration/dispatch only set DIRTY; the final CAS closes the lost-
 * wake window. The caller already owns a registration or external credit. */
static bool __xrtChannelSubscriptionSchedule(xchannelsubscription* pSubscription)
{
	uint32 iFlags=xrtAtomic32FetchOr(&pSubscription->Dispatch,
		XRT_CHANNEL_SUB_DIRTY,XMEMORY_ACQ_REL)|XRT_CHANNEL_SUB_DIRTY;
	while(!(iFlags&XRT_CHANNEL_SUB_DRIVER)) {
		if(xrtAtomic32CompareExchange(&pSubscription->Dispatch,&iFlags,
			iFlags|XRT_CHANNEL_SUB_DRIVER,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
			/* Reserved internal credit: at most one dispatcher per subscription. */
			(void)__xrtChannelSubscriptionRetain(pSubscription,0u);
			return true;
		}
	}
	return false;
}

static xchannelsubscription* __xrtChannelSubscriptionFromWork(xrt_channel_deferred* pWork)
{
	return (xchannelsubscription*)((uint8*)pWork-
		offsetof(xchannelsubscription,Work));
}

static void __xrtChannelSubscriptionDispatch(xrt_channel_deferred* pWork)
{
	xchannelsubscription* pSubscription=__xrtChannelSubscriptionFromWork(pWork);
	__xrtChannelSubscriptionDrive(pSubscription);
	__xrtChannelSubscriptionRelease(pSubscription);
}

void __xrtChannelSubscriptionWakeLocked(xrt_channel_impl* pImpl,
	xrt_channel_deferred* pWork)
{
	xchannelsubscription* pSubscription=__xrtChannelSubscriptionFromWork(pWork);
	if(!__xrtChannelSubscriptionSchedule(pSubscription)) return;
	pWork->Next=NULL;
	if(pImpl->DeferredTail) pImpl->DeferredTail->Next=pWork;
	else pImpl->DeferredHead=pWork;
	pImpl->DeferredTail=pWork;
}

static void __xrtChannelSubscriptionWake(ptr pData)
{
	xchannelsubscription* pSubscription=(xchannelsubscription*)pData;
	if(__xrtChannelSubscriptionSchedule(pSubscription))
		__xrtChannelSubscriptionDispatch(&pSubscription->Work);
}

static void __xrtChannelSubscriptionComplete(xchannelsubscription* pSubscription,
	uint32 iWinner)
{
	__xrtChannelSelectUnregisterAll(pSubscription->Waiters,pSubscription->Count);
	pSubscription->Result.Index=XCHANNEL_SELECT_NONE;
	pSubscription->Result.Result=XCHANNEL_ERROR;
	if(iWinner<=XRT_CHANNEL_SELECT_MAX_INDEX) {
		pSubscription->Result.Index=iWinner;
		pSubscription->Result.Result=pSubscription->Waiters[iWinner].Result;
		pSubscription->Result.Wait=pSubscription->Result.Result==XCHANNEL_ERROR ?
			XWAIT_ERROR : XWAIT_OK;
	} else pSubscription->Result.Wait=pSubscription->Select.StopResult;
	xrtAtomic32Store(&pSubscription->Ready,1u,XMEMORY_RELEASE);
	/* Return the physical registration credit before Notify; the dispatcher
	 * still protects the complete object, including reentrant last Destroy. */
	__xrtChannelSubscriptionRelease(pSubscription);
	pSubscription->Policy->Notify(pSubscription,pSubscription->Data);
}

static void __xrtChannelSubscriptionDrive(xchannelsubscription* pSubscription)
{
	for(;;) {
		uint32 iExpected;
		xrtAtomic32FetchAnd(&pSubscription->Dispatch,~XRT_CHANNEL_SUB_DIRTY,
			XMEMORY_ACQ_REL);
		if(!xrtAtomic32Load(&pSubscription->Ready,XMEMORY_ACQUIRE)) {
			uint32 iWinner=xrtAtomic32Load(&pSubscription->Select.Winner,XMEMORY_ACQUIRE);
			if(iWinner==XRT_CHANNEL_SELECT_NONE) {
				uint32 iStop;
				/* A currently ready/closed case gets the same first chance as in
				 * synchronous Select, before a cancellation/deadline request. */
				__xrtChannelSelectTryWaiters(pSubscription->Waiters,pSubscription->Count,
					pSubscription->Start);
				iStop=xrtAtomic32Load(&pSubscription->Stop,XMEMORY_ACQUIRE);
				if(iStop) (void)__xrtChannelSelectStop(&pSubscription->Select,
					(xwaitresult)(int32)iStop);
				iWinner=xrtAtomic32Load(&pSubscription->Select.Winner,XMEMORY_ACQUIRE);
			}
			/* CLAIMING is owned by a live committer. Its release publication
			 * always wakes us; never spin or overwrite an in-flight transfer. */
			if(iWinner!=XRT_CHANNEL_SELECT_NONE && iWinner!=XRT_CHANNEL_SELECT_CLAIMING)
				__xrtChannelSubscriptionComplete(pSubscription,iWinner);
		}
		iExpected=XRT_CHANNEL_SUB_DRIVER;
		if(xrtAtomic32CompareExchange(&pSubscription->Dispatch,&iExpected,0u,
			XMEMORY_RELEASE,XMEMORY_RELAXED)) return;
	}
}

XRT_API xchannelsubscription* xrtChannelSubscribeCreateV1(
	const xchannelsubscribecase* pCases,size_t iCount,
	ptr pData,const xchannelsubscribepolicyv1* pPolicy)
{
	xchannelsubscription* pSubscription;
	xchannelcase* pValidation;
	size_t iBytes;
	if(!pPolicy || pPolicy->size!=sizeof(*pPolicy) || !pPolicy->Notify ||
		!pPolicy->Drop || !pCases || !iCount ||
		((uintptr_t)pCases&(sizeof(ptr)-1u)) ||
		iCount>((size_t)XRT_CHANNEL_SELECT_MAX_INDEX+1u) ||
		iCount>(SIZE_MAX-sizeof(*pSubscription))/sizeof(xrt_channel_select_waiter) ||
		iCount>SIZE_MAX/sizeof(xchannelcase) ||
		(uintptr_t)pCases>UINTPTR_MAX-iCount*sizeof(*pCases)) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iBytes=sizeof(*pSubscription)+iCount*sizeof(xrt_channel_select_waiter);
	pSubscription=(xchannelsubscription*)xrtMalloc(iBytes);
	if(!pSubscription) return NULL;
	memset(pSubscription,0,iBytes);
	pValidation=(xchannelcase*)xrtMalloc(iCount*sizeof(xchannelcase));
	if(!pValidation) { xrtFree(pSubscription); return NULL; }
	for(size_t i=0;i<iCount;i++) {
		pValidation[i].Channel=pCases[i].Channel;
		pValidation[i].Operation=pCases[i].Operation;
		pValidation[i].Value=pCases[i].Value;
		pValidation[i].Output=&pSubscription->Received;
	}
	if(!__xrtChannelSelectCasesValid(pValidation,iCount)) {
		xrtFree(pValidation);
		xrtFree(pSubscription);
		return NULL;
	}
	for(size_t i=0;i<iCount;i++) {
		xrt_channel_select_waiter* pWaiter=&pSubscription->Waiters[i];
		pWaiter->Select=&pSubscription->Select;
		pWaiter->Channel=pValidation[i].Channel;
		pWaiter->Value=pValidation[i].Value;
		pWaiter->Output=&pSubscription->Received;
		pWaiter->Index=(uint32)i;
		pWaiter->Operation=pValidation[i].Operation;
		pWaiter->Result=XCHANNEL_ERROR;
	}
	xrtFree(pValidation);
	xrtAtomic32Init(&pSubscription->References,1u);
	xrtAtomic32Init(&pSubscription->External,1u);
	xrtAtomic32Init(&pSubscription->Started,0u);
	xrtAtomic32Init(&pSubscription->Ready,0u);
	xrtAtomic32Init(&pSubscription->Stop,0u);
	/* Start owns the dormant dispatcher; no partially registered set can run. */
	xrtAtomic32Init(&pSubscription->Dispatch,XRT_CHANNEL_SUB_DRIVER);
	xrtAtomic32Init(&pSubscription->Select.Winner,XRT_CHANNEL_SELECT_NONE);
	pSubscription->Select.Wake=__xrtChannelSubscriptionWake;
	pSubscription->Select.WakeData=pSubscription;
	pSubscription->Select.Deferred=&pSubscription->Work;
	pSubscription->Select.StopResult=XWAIT_ERROR;
	pSubscription->Work.Dispatch=__xrtChannelSubscriptionDispatch;
	pSubscription->Count=iCount;
	pSubscription->Start=__xrtChannelSelectStartIndex(iCount);
	pSubscription->Data=pData;
	pSubscription->Policy=pPolicy;
	return pSubscription;
}

XRT_API bool xrtChannelSubscribeStart(xchannelsubscription* pSubscription)
{
	uint32 iExpected=0u;
	if(!pSubscription) { __xrtErrorSetInvalidArgument(); return false; }
	if(!__xrtChannelSubscriptionRetain(pSubscription,2u)) return false;
	if(!xrtAtomic32CompareExchange(&pSubscription->Started,&iExpected,1u,
		XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)) {
		__xrtChannelSubscriptionRelease(pSubscription);
		__xrtErrorSetInvalidState();
		return false;
	}
	(void)__xrtChannelSubscriptionRetain(pSubscription,1u); /* registration */
	for(size_t i=0;i<pSubscription->Count;i++) {
		uint32 iWinner;
		if(!__xrtChannelSelectRegister(&pSubscription->Waiters[i])) {
			(void)__xrtChannelSelectStop(&pSubscription->Select,XWAIT_ERROR);
			break;
		}
		iWinner=xrtAtomic32Load(&pSubscription->Select.Winner,XMEMORY_ACQUIRE);
		if(iWinner!=XRT_CHANNEL_SELECT_NONE) break;
	}
	/* The admitted Start call's credit serves as the initial dispatch credit. */
	__xrtChannelSubscriptionDispatch(&pSubscription->Work);
	return true;
}

XRT_API xchannelsubscription* xrtChannelSubscribeRef(xchannelsubscription* pSubscription)
{
	if(!pSubscription) { __xrtErrorSetInvalidArgument(); return NULL; }
	if(!__xrtChannelSubscriptionRetain(pSubscription,4u)) return NULL;
	xrtAtomic32FetchAdd(&pSubscription->External,1u,XMEMORY_RELAXED);
	return pSubscription;
}

XRT_API void xrtChannelSubscribeDestroy(xchannelsubscription* pSubscription)
{
	if(!pSubscription) return;
	if(xrtAtomic32FetchSub(&pSubscription->External,1u,XMEMORY_ACQ_REL)==1u) {
		uint32 iExpected=0u;
		(void)xrtAtomic32CompareExchange(&pSubscription->Stop,&iExpected,
			(uint32)XWAIT_CANCELLED,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE);
		if(xrtAtomic32Load(&pSubscription->Started,XMEMORY_ACQUIRE))
			__xrtChannelSubscriptionWake(pSubscription);
	}
	__xrtChannelSubscriptionRelease(pSubscription);
}

XRT_API bool xrtChannelSubscribeStop(xchannelsubscription* pSubscription,xwaitresult iReason)
{
	uint32 iExpected=0u;
	bool bRecorded;
	if(!pSubscription || (iReason!=XWAIT_CANCELLED && iReason!=XWAIT_TIMEOUT &&
		iReason!=XWAIT_ERROR)) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if(!__xrtChannelSubscriptionRetain(pSubscription,2u)) return false;
	bRecorded=xrtAtomic32CompareExchange(&pSubscription->Stop,&iExpected,
		(uint32)iReason,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE);
	if(xrtAtomic32Load(&pSubscription->Started,XMEMORY_ACQUIRE))
		__xrtChannelSubscriptionWake(pSubscription);
	__xrtChannelSubscriptionRelease(pSubscription);
	return bRecorded;
}

XRT_API bool xrtChannelSubscribeResult(const xchannelsubscription* pSubscription,
	xchannelselectresult* pResult,ptr* pValue)
{
	size_t iBytes;
	if(!pSubscription || !pResult || !pValue ||
		((uintptr_t)pResult&(sizeof(size_t)-1u)) ||
		((uintptr_t)pValue&(sizeof(ptr)-1u))) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iBytes=sizeof(*pSubscription)+pSubscription->Count*sizeof(xrt_channel_select_waiter);
	if((uintptr_t)pResult>UINTPTR_MAX-sizeof(*pResult) ||
		(uintptr_t)pValue>UINTPTR_MAX-sizeof(*pValue) ||
		__xrtRangesOverlap(pResult,sizeof(*pResult),pValue,sizeof(*pValue)) ||
		__xrtRangesOverlap(pSubscription,iBytes,pResult,sizeof(*pResult)) ||
		__xrtRangesOverlap(pSubscription,iBytes,pValue,sizeof(*pValue))) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if(!xrtAtomic32Load(&pSubscription->Ready,XMEMORY_ACQUIRE)) return false;
	*pResult=pSubscription->Result;
	*pValue=pSubscription->Received;
	return true;
}

#endif
