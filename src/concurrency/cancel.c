#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_CANCEL)

/* 一个父链节点只挂入对应令牌的监听链表。 */
typedef struct xcancelnode {
	struct xcancelnode* Next;
	struct xcancelwatch* Watch;
	struct xcancel* Cancel;
	bool Linked;
} xcancelnode;



/* 取消令牌保留父令牌，并用互斥锁保护监听链表。 */
struct xcancel {
	volatile int32 RefCount;
	volatile int32 Requested;
	xmutex Lock;
	struct xcancel* Parent;
	xcancelnode* WatchHead;
};



/* 监听对象集中保存回调状态和全部父链节点，避免逐节点分配。 */
struct xcancelwatch {
	volatile int32 RefCount;
	volatile int32 Triggered;
	xmutex Lock;
	xcond Idle;
	xcancel* Cancel;
	xcancelproc Proc;
	ptr Data;
	uint32 NodeCount;
	bool Armed;
	bool CallbackStarted;
	bool CallbackActive;
	bool CallbackThreadValid;
	bool Destroying;
	bool DeferredRelease;
	#if defined(_WIN32) || defined(_WIN64)
		DWORD CallbackThread;
	#else
		pthread_t CallbackThread;
	#endif
	xcancelnode Nodes[1];
};



/* 释放监听对象的一个内部引用。 */
static void __xrtCancelWatchRelease(xcancelwatch* pWatch)
{
	xcancel* pCancel;

	if ( (pWatch == NULL) || (xrtRefRelease(&pWatch->RefCount) != 0) ) {
		return;
	}
	pCancel = pWatch->Cancel;
	(void)xrtCondUnit(&pWatch->Idle);
	(void)xrtMutexUnit(&pWatch->Lock);
	xrtFree(pWatch);
	xrtCancelDestroy(pCancel);
}



/* 判断当前线程是否正在执行指定监听的回调。 */
static bool __xrtCancelWatchIsCallbackThread(const xcancelwatch* pWatch)
{
	if ( !pWatch->CallbackThreadValid ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return pWatch->CallbackThread == GetCurrentThreadId();
	#else
		return pthread_equal(pWatch->CallbackThread, pthread_self()) != 0;
	#endif
}



/* 在持有监听锁时记录回调启动，并返回待执行过程。 */
static xcancelproc __xrtCancelWatchStart(xcancelwatch* pWatch, ptr* ppData)
{
	if (
		pWatch->Destroying || !pWatch->Armed ||
		(__xrtAtomicRefLoad(&pWatch->Triggered) == 0) ||
		pWatch->CallbackStarted
	) {
		return NULL;
	}
	pWatch->CallbackStarted = true;
	pWatch->CallbackActive = true;
	pWatch->CallbackThreadValid = true;
	#if defined(_WIN32) || defined(_WIN64)
		pWatch->CallbackThread = GetCurrentThreadId();
	#else
		pWatch->CallbackThread = pthread_self();
	#endif
	*ppData = pWatch->Data;
	return pWatch->Proc;
}



/* 执行回调并在返回后唤醒注销方或完成回调内延迟回收。 */
static void __xrtCancelWatchRun(
	xcancelwatch* pWatch,
	xcancelproc pProc,
	ptr pData
)
{
	bool bRelease;

	pProc(pData);
	(void)xrtMutexLock(&pWatch->Lock);
	pWatch->CallbackActive = false;
	pWatch->CallbackThreadValid = false;
	bRelease = pWatch->DeferredRelease;
	(void)xrtCondBroadcast(&pWatch->Idle);
	(void)xrtMutexUnlock(&pWatch->Lock);
	if ( bRelease ) {
		__xrtCancelWatchRelease(pWatch);
	}
}



/* 标记监听已触发，并在监听完成装配后同步执行一次回调。 */
static void __xrtCancelWatchNotify(xcancelwatch* pWatch)
{
	xcancelproc pProc = NULL;
	ptr pData = NULL;

	(void)xrtMutexLock(&pWatch->Lock);
	if ( !pWatch->Destroying && (__xrtAtomicRefLoad(&pWatch->Triggered) == 0) ) {
		(void)__xrtAtomicRefCompareExchange(&pWatch->Triggered, 1, 0);
		pProc = __xrtCancelWatchStart(pWatch, &pData);
	}
	(void)xrtMutexUnlock(&pWatch->Lock);
	if ( pProc != NULL ) {
		__xrtCancelWatchRun(pWatch, pProc, pData);
	}
}



/* 创建一个独立的取消令牌。 */
XRT_API xcancel* xrtCancelCreate(void)
{
	xcancel* pCancel = (xcancel*)xrtMalloc(sizeof(xcancel));

	if ( pCancel == NULL ) {
		return NULL;
	}
	memset(pCancel, 0, sizeof(xcancel));
	pCancel->RefCount = 1;
	if ( !xrtMutexInit(&pCancel->Lock) ) {
		xrtFree(pCancel);
		return NULL;
	}
	return pCancel;
}



/* 创建一个继承不可变父链的子取消令牌。 */
XRT_API xcancel* xrtCancelChild(xcancel* pParent)
{
	xcancel* pCancel = xrtCancelCreate();

	if ( pCancel == NULL ) {
		return NULL;
	}
	if ( pParent != NULL ) {
		pCancel->Parent = xrtCancelRef(pParent);
		if ( pCancel->Parent == NULL ) {
			xrtCancelDestroy(pCancel);
			return NULL;
		}
	}
	return pCancel;
}



/* 增加取消令牌引用。 */
XRT_API xcancel* xrtCancelRef(xcancel* pCancel)
{
	if ( (pCancel == NULL) || (xrtRefRetain(&pCancel->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pCancel;
}



/* 释放取消令牌引用，并顺着唯一父引用迭代回收。 */
XRT_API void xrtCancelDestroy(xcancel* pCancel)
{
	while ( (pCancel != NULL) && (xrtRefRelease(&pCancel->RefCount) == 0) ) {
		xcancel* pParent = pCancel->Parent;

		(void)xrtMutexUnit(&pCancel->Lock);
		xrtFree(pCancel);
		pCancel = pParent;
	}
}



/* 首次请求取消并在令牌锁外通知全部监听。 */
XRT_API bool xrtCancelRequest(xcancel* pCancel)
{
	xcancelnode* pList;
	xcancelnode* pNode;

	if ( pCancel == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)xrtMutexLock(&pCancel->Lock);
	if ( __xrtAtomicRefLoad(&pCancel->Requested) != 0 ) {
		(void)xrtMutexUnlock(&pCancel->Lock);
		return false;
	}
	(void)__xrtAtomicRefCompareExchange(&pCancel->Requested, 1, 0);
	pList = pCancel->WatchHead;
	pCancel->WatchHead = NULL;
	for ( pNode = pList; pNode != NULL; pNode = pNode->Next ) {
		pNode->Linked = false;
		(void)xrtRefRetain(&pNode->Watch->RefCount);
	}
	(void)xrtMutexUnlock(&pCancel->Lock);

	while ( pList != NULL ) {
		pNode = pList;
		pList = pNode->Next;
		pNode->Next = NULL;
		__xrtCancelWatchNotify(pNode->Watch);
		__xrtCancelWatchRelease(pNode->Watch);
	}
	return true;
}



/* 查询令牌及其不可变父链是否已取消。 */
XRT_API bool xrtCancelRequested(const xcancel* pCancel)
{
	while ( pCancel != NULL ) {
		if ( __xrtAtomicRefLoad(&pCancel->Requested) != 0 ) {
			return true;
		}
		pCancel = pCancel->Parent;
	}
	return false;
}



/* 为令牌及其全部祖先一次性装配监听节点。 */
XRT_API xcancelwatch* xrtCancelWatch(
	xcancel* pCancel,
	xcancelproc pProc,
	ptr pData
)
{
	xcancelwatch* pWatch;
	xcancel* pCurrent;
	xcancelproc pStart = NULL;
	ptr pStartData = NULL;
	uint32 iCount = 0;
	size_t iBytes;

	if ( (pCancel == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pCancel = xrtCancelRef(pCancel);
	if ( pCancel == NULL ) {
		return NULL;
	}
	for ( pCurrent = pCancel; pCurrent != NULL; pCurrent = pCurrent->Parent ) {
		if ( iCount == UINT32_MAX ) {
			xrtCancelDestroy(pCancel);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iCount++;
	}
	#if SIZE_MAX <= UINT32_MAX
		if (
			(size_t)iCount >
			((SIZE_MAX - offsetof(xcancelwatch, Nodes)) / sizeof(xcancelnode))
		) {
			xrtCancelDestroy(pCancel);
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
	#endif
	iBytes = offsetof(xcancelwatch, Nodes) + ((size_t)iCount * sizeof(xcancelnode));
	pWatch = (xcancelwatch*)xrtMalloc(iBytes);
	if ( pWatch == NULL ) {
		xrtCancelDestroy(pCancel);
		return NULL;
	}
	memset(pWatch, 0, iBytes);
	pWatch->RefCount = 1;
	pWatch->Cancel = pCancel;
	pWatch->Proc = pProc;
	pWatch->Data = pData;
	pWatch->NodeCount = iCount;
	if ( !xrtMutexInit(&pWatch->Lock) ) {
		xrtFree(pWatch);
		xrtCancelDestroy(pCancel);
		return NULL;
	}
	if ( !xrtCondInit(&pWatch->Idle) ) {
		(void)xrtMutexUnit(&pWatch->Lock);
		xrtFree(pWatch);
		xrtCancelDestroy(pCancel);
		return NULL;
	}

	pCurrent = pCancel;
	for ( uint32 i = 0; i < iCount; i++, pCurrent = pCurrent->Parent ) {
		xcancelnode* pNode = &pWatch->Nodes[i];

		pNode->Watch = pWatch;
		pNode->Cancel = pCurrent;
		(void)xrtMutexLock(&pCurrent->Lock);
		if ( __xrtAtomicRefLoad(&pCurrent->Requested) != 0 ) {
			(void)__xrtAtomicRefCompareExchange(&pWatch->Triggered, 1, 0);
		} else {
			pNode->Next = pCurrent->WatchHead;
			pCurrent->WatchHead = pNode;
			pNode->Linked = true;
		}
		(void)xrtMutexUnlock(&pCurrent->Lock);
	}

	(void)xrtMutexLock(&pWatch->Lock);
	pWatch->Armed = true;
	pStart = __xrtCancelWatchStart(pWatch, &pStartData);
	(void)xrtMutexUnlock(&pWatch->Lock);
	if ( pStart != NULL ) {
		__xrtCancelWatchRun(pWatch, pStart, pStartData);
	}
	return pWatch;
}



/* 查询监听是否已经命中取消。 */
XRT_API bool xrtCancelTriggered(const xcancelwatch* pWatch)
{
	if ( pWatch == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtAtomicRefLoad(&pWatch->Triggered) != 0;
}



/* 从一个令牌链表中移除仍然挂接的监听节点。 */
static void __xrtCancelUnlinkNode(xcancelnode* pNode)
{
	xcancelnode** ppNode;
	xcancel* pCancel = pNode->Cancel;

	(void)xrtMutexLock(&pCancel->Lock);
	if ( pNode->Linked ) {
		ppNode = &pCancel->WatchHead;
		while ( (*ppNode != NULL) && (*ppNode != pNode) ) {
			ppNode = &(*ppNode)->Next;
		}
		if ( *ppNode == pNode ) {
			*ppNode = pNode->Next;
		}
		pNode->Next = NULL;
		pNode->Linked = false;
	}
	(void)xrtMutexUnlock(&pCancel->Lock);
}



/* 注销监听，并针对回调自身注销采用返回后延迟回收。 */
XRT_API void xrtCancelUnwatch(xcancelwatch* pWatch)
{
	bool bSelf;

	if ( pWatch == NULL ) {
		return;
	}
	if ( xrtRefRetain(&pWatch->RefCount) < 0 ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	(void)xrtMutexLock(&pWatch->Lock);
	if ( pWatch->Destroying ) {
		bSelf = pWatch->CallbackActive && __xrtCancelWatchIsCallbackThread(pWatch);
		while ( pWatch->CallbackActive && !bSelf ) {
			(void)xrtCondWait(&pWatch->Idle, &pWatch->Lock);
		}
		(void)xrtMutexUnlock(&pWatch->Lock);
		__xrtCancelWatchRelease(pWatch);
		return;
	}
	pWatch->Destroying = true;
	(void)xrtMutexUnlock(&pWatch->Lock);

	for ( uint32 i = 0; i < pWatch->NodeCount; i++ ) {
		__xrtCancelUnlinkNode(&pWatch->Nodes[i]);
	}

	(void)xrtMutexLock(&pWatch->Lock);
	bSelf = pWatch->CallbackActive && __xrtCancelWatchIsCallbackThread(pWatch);
	if ( bSelf ) {
		pWatch->DeferredRelease = true;
		(void)xrtMutexUnlock(&pWatch->Lock);
		__xrtCancelWatchRelease(pWatch);
		return;
	}
	while ( pWatch->CallbackActive ) {
		(void)xrtCondWait(&pWatch->Idle, &pWatch->Lock);
	}
	(void)xrtMutexUnlock(&pWatch->Lock);
	__xrtCancelWatchRelease(pWatch);
	__xrtCancelWatchRelease(pWatch);
}

#endif
