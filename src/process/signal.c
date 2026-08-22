#include "../internal/xrt_signal.h"

#include <signal.h>



#if defined(XRT_FEATURE_SIGNAL)

xsignalstate __xrtSignalState;

static xonce __xrtSignalOnce = XRT_ONCE_INIT;



/* 返回跨平台信号表中的槽下标。 */
static int __xrtSignalSlotIndex(xsignal Code)
{
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		if ( __xrtSignalState.Slots[i].Code == Code ) {
			return (int)i;
		}
	}
	return -1;
}



/* 填充固定信号描述，槽顺序同时是平台后端的稳定内部协议。 */
static void __xrtSignalInitSlots(void)
{
	xsignalslot* pSlots = __xrtSignalState.Slots;

	pSlots[0].Code = XSIGNAL_INT;
	pSlots[0].SystemCode = SIGINT;
	pSlots[0].Name = "INT";
	pSlots[0].Supported = true;

	pSlots[1].Code = XSIGNAL_TERM;
	pSlots[1].SystemCode = SIGTERM;
	pSlots[1].Name = "TERM";
	pSlots[1].Supported = true;

	pSlots[2].Code = XSIGNAL_HUP;
	pSlots[2].Name = "HUP";
	#if defined(SIGHUP)
		pSlots[2].SystemCode = SIGHUP;
		pSlots[2].Supported = true;
	#endif

	pSlots[3].Code = XSIGNAL_BREAK;
	pSlots[3].Name = "BREAK";
	#if defined(_WIN32) || defined(_WIN64)
		#ifdef SIGBREAK
			pSlots[3].SystemCode = SIGBREAK;
		#endif
		pSlots[3].Supported = true;
	#endif

	pSlots[4].Code = XSIGNAL_CLOSE;
	pSlots[4].Name = "CLOSE";
	pSlots[5].Code = XSIGNAL_LOGOFF;
	pSlots[5].Name = "LOGOFF";
	pSlots[6].Code = XSIGNAL_SHUTDOWN;
	pSlots[6].Name = "SHUTDOWN";
	#if defined(_WIN32) || defined(_WIN64)
		pSlots[4].Supported = true;
		pSlots[5].Supported = true;
		pSlots[6].Supported = true;
	#endif

	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		xrtAtomic32Init(&pSlots[i].Mode, XRT_SIGNAL_SLOT_DEFAULT);
		xrtAtomic32Init(&pSlots[i].Pending, 0u);
		xrtAtomic32Init(&pSlots[i].LastSystemCode, 0u);
	}
}



/* 一次性初始化永不销毁的进程级同步对象。 */
static bool __xrtSignalStateInit(ptr pData)
{
	(void)pData;
	memset(&__xrtSignalState, 0, sizeof(__xrtSignalState));
	if ( !xrtMutexInit(&__xrtSignalState.Lock) ) {
		return false;
	}
	if ( !xrtCondInit(&__xrtSignalState.Idle) ) {
		(void)xrtMutexUnit(&__xrtSignalState.Lock);
		return false;
	}
	__xrtSignalInitSlots();
	xrtAtomic32Init(&__xrtSignalState.HandlerActive, 0u);
	return true;
}



/* 保证进程级同步对象已经可用。 */
static bool __xrtSignalEnsure(void)
{
	if ( xrtOnce(&__xrtSignalOnce, __xrtSignalStateInit, NULL) ) {
		return true;
	}
	__xrtErrorWrapDetail(
		XERR_STATE,
		"xrt.signal",
		XSIGNAL_ERROR_STATE,
		"initialize",
		"signal state initialization failed"
	);
	return false;
}



/* 设置带稳定信号错误域的非系统错误。 */
static void __xrtSignalSetError(
	xerrkind Kind,
	xsignalerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtErrorSetDetail(
		Kind,
		"xrt.signal",
		Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 设置保留原生错误代码的平台失败。 */
static void __xrtSignalSetSystemError(
	cstr sOperation,
	int iSystemCode,
	cstr sMessage
)
{
	__xrtErrorSetSystem(
		"xrt.signal",
		XSIGNAL_ERROR_SYSTEM,
		sOperation,
		iSystemCode,
		sMessage
	);
}



/* 验证信号代码并按需拒绝当前平台不支持的代码。 */
static int __xrtSignalRequire(xsignal Code, bool bRequireSupported)
{
	int iSlot;

	if ( !__xrtSignalEnsure() ) {
		return -1;
	}
	iSlot = __xrtSignalSlotIndex(Code);
	if ( iSlot < 0 ) {
		__xrtSignalSetError(
			XERR_ARGUMENT,
			XSIGNAL_ERROR_CODE,
			"code",
			"signal code is invalid"
		);
		return -1;
	}
	if ( bRequireSupported && !__xrtSignalState.Slots[iSlot].Supported ) {
		__xrtSignalSetError(
			XERR_UNSUPPORTED,
			XSIGNAL_ERROR_UNSUPPORTED,
			"code",
			"signal is not supported on this platform"
		);
		return -1;
	}
	return iSlot;
}



/* 最后一个引用离开时析构用户数据和监听句柄。 */
static void __xrtSignalWatchRelease(xsignalwatch* pWatch)
{
	if ( (pWatch == NULL) || (xrtRefRelease(&pWatch->RefCount) != 0) ) {
		return;
	}
	if ( pWatch->Free != NULL ) {
		pWatch->Free(pWatch->Data);
	}
	xrtFree(pWatch);
}



/* 从全局双向链表中摘除一个监听，但不释放其注册表引用。 */
static bool __xrtSignalUnlinkLocked(xsignalwatch* pWatch)
{
	if ( !pWatch->Linked ) {
		return false;
	}
	if ( pWatch->Previous != NULL ) {
		pWatch->Previous->Next = pWatch->Next;
	} else {
		__xrtSignalState.Head = pWatch->Next;
	}
	if ( pWatch->Next != NULL ) {
		pWatch->Next->Previous = pWatch->Previous;
	}
	pWatch->Previous = NULL;
	pWatch->Next = NULL;
	pWatch->Linked = false;
	return true;
}



/* 判断某个信号是否仍有活动监听。 */
static bool __xrtSignalHasWatchLocked(xsignal Code)
{
	for ( xsignalwatch* pWatch = __xrtSignalState.Head;
		pWatch != NULL;
		pWatch = pWatch->Next ) {
		if ( pWatch->Active && (pWatch->Code == Code) ) {
			return true;
		}
	}
	return false;
}



/* 判断某个信号是否仍有正在执行的回调。 */
static bool __xrtSignalHasCallbackLocked(xsignal Code)
{
	return (__xrtSignalState.Callback != NULL) &&
		((Code == XSIGNAL_NONE) ||
		 (__xrtSignalState.Callback->Code == Code));
}



/* 把匹配监听从注册表摘除并串到待释放链表。 */
static xsignalwatch* __xrtSignalDetachLocked(xsignal Code)
{
	xsignalwatch* pRelease = NULL;
	xsignalwatch* pWatch = __xrtSignalState.Head;

	while ( pWatch != NULL ) {
		xsignalwatch* pNext = pWatch->Next;

		if ( (Code == XSIGNAL_NONE) || (pWatch->Code == Code) ) {
			pWatch->Active = false;
			if ( __xrtSignalUnlinkLocked(pWatch) ) {
				pWatch->Next = pRelease;
				pRelease = pWatch;
			}
		}
		pWatch = pNext;
	}
	return pRelease;
}



/* 释放一组已经摘除的注册表引用。 */
static void __xrtSignalReleaseList(xsignalwatch* pWatch)
{
	while ( pWatch != NULL ) {
		xsignalwatch* pNext = pWatch->Next;

		pWatch->Next = NULL;
		__xrtSignalWatchRelease(pWatch);
		pWatch = pNext;
	}
}



/* 调度线程入口在后文定义，启动过程需要提前声明。 */
static int32 __xrtSignalDispatchThread(ptr pData);



/* 最后监听离开时恢复原生处理方式，调度过程需要提前声明。 */
static bool __xrtSignalRestoreUnusedLocked(int iSlot);



/* 在持锁状态下惰性创建平台唤醒资源与唯一调度线程。 */
static bool __xrtSignalStartLocked(void)
{
	if ( __xrtSignalState.Running &&
		!__xrtSignalState.DispatchFailed ) {
		return true;
	}
	if ( __xrtSignalState.Stopping ) {
		__xrtSignalSetError(
			XERR_STATE,
			XSIGNAL_ERROR_STATE,
			"start",
			"signal dispatcher is stopping"
		);
		return false;
	}
	if ( __xrtSignalState.DispatchFailed ) {
		__xrtSignalSetSystemError(
			"start",
			__xrtSignalState.DispatchSystemCode,
			"signal dispatcher failed; call xrtSignalShutdown before restart"
		);
		return false;
	}
	if ( __xrtSignalState.Dispatcher != NULL ) {
		(void)xrtThreadWait(__xrtSignalState.Dispatcher);
		xrtThreadDestroy(__xrtSignalState.Dispatcher);
		__xrtSignalState.Dispatcher = NULL;
	}
	if ( !xrtAtomicIsLockFree(sizeof(uint32)) ) {
		__xrtSignalSetError(
			XERR_UNSUPPORTED,
			XSIGNAL_ERROR_UNSUPPORTED,
			"start",
			"signal delivery requires lock-free 32-bit atomics"
		);
		return false;
	}
	if ( !__xrtSignalState.PlatformReady ) {
		if ( !__xrtSignalPlatformInit() ) {
			return false;
		}
		__xrtSignalState.PlatformReady = true;
	}
	__xrtSignalState.DispatchFailed = false;
	__xrtSignalState.DispatchSystemCode = 0;
	__xrtSignalState.Running = true;
	__xrtSignalState.Dispatcher = xrtThreadCreate(
		__xrtSignalDispatchThread,
		NULL,
		0u
	);
	if ( __xrtSignalState.Dispatcher != NULL ) {
		return true;
	}
	__xrtSignalState.Running = false;
	__xrtSignalPlatformUnit();
	__xrtSignalState.PlatformReady = false;
	__xrtErrorWrapDetail(
		XERR_STATE,
		"xrt.signal",
		XSIGNAL_ERROR_STATE,
		"start",
		"signal dispatcher thread creation failed"
	);
	return false;
}



/* 以饱和方式增加处理器可写的 32 位待处理计数。 */
static uint32 __xrtSignalPendingAdd(xatomic32* pPending)
{
	uint32 iCurrent = xrtAtomic32Load(pPending, XMEMORY_RELAXED);

	for ( ;; ) {
		uint32 iExpected;

		if ( iCurrent == UINT32_MAX ) {
			return iCurrent;
		}
		iExpected = iCurrent;
		if ( xrtAtomic32CompareExchange(
			pPending,
			&iExpected,
			iCurrent + 1u,
			XMEMORY_RELEASE,
			XMEMORY_RELAXED
		) ) {
			return iCurrent;
		}
		iCurrent = iExpected;
	}
}



/* 原生回调只更新锁自由字段，并在首次挂起时写入一次唤醒。 */
void __xrtSignalNotify(uint32 iSlot, int32 iSystemCode)
{
	xsignalslot* pSlot;
	uint32 iOld;

	if ( iSlot >= XRT_SIGNAL_SLOT_COUNT ) {
		return;
	}
	(void)xrtAtomic32FetchAdd(
		&__xrtSignalState.HandlerActive,
		1u,
		XMEMORY_ACQ_REL
	);
	pSlot = &__xrtSignalState.Slots[iSlot];
	if ( xrtAtomic32Load(&pSlot->Mode, XMEMORY_ACQUIRE) ==
		XRT_SIGNAL_SLOT_WATCH ) {
		xrtAtomic32Store(
			&pSlot->LastSystemCode,
			(uint32)iSystemCode,
			XMEMORY_RELAXED
		);
		iOld = __xrtSignalPendingAdd(&pSlot->Pending);
		if ( iOld == 0u ) {
			__xrtSignalPlatformWake();
		}
	}
	(void)xrtAtomic32FetchSub(
		&__xrtSignalState.HandlerActive,
		1u,
		XMEMORY_RELEASE
	);
}



/* 在一次事件快照中逐个选择监听，不分配临时数组且不限制监听数量。 */
static void __xrtSignalDispatchEvent(
	const xsignalevent* pEvent,
	uint64 iMaxId
)
{
	uint64 iLastId = 0u;

	for ( ;; ) {
		xsignalwatch* pSelected = NULL;
		bool bReleaseRegistry = false;

		(void)xrtMutexLock(&__xrtSignalState.Lock);
		if ( __xrtSignalState.Stopping ) {
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			return;
		}
		for ( xsignalwatch* pWatch = __xrtSignalState.Head;
			pWatch != NULL;
			pWatch = pWatch->Next ) {
			if ( !pWatch->Active || (pWatch->Code != pEvent->Code) ||
				(pWatch->Id <= iLastId) || (pWatch->Id > iMaxId) ) {
				continue;
			}
			if ( (pSelected == NULL) || (pWatch->Id < pSelected->Id) ) {
				pSelected = pWatch;
			}
		}
		if ( pSelected != NULL ) {
			iLastId = pSelected->Id;
			(void)xrtRefRetain(&pSelected->RefCount);
			pSelected->CallbackActive = true;
			__xrtSignalState.Callback = pSelected;
			if ( pSelected->Once ) {
				pSelected->Active = false;
				bReleaseRegistry = __xrtSignalUnlinkLocked(pSelected);
				if ( bReleaseRegistry ) {
					int iSlot = __xrtSignalSlotIndex(pSelected->Code);

					if ( iSlot >= 0 ) {
						(void)__xrtSignalRestoreUnusedLocked(iSlot);
					}
				}
			}
		}
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		if ( pSelected == NULL ) {
			return;
		}

		pSelected->Proc(pSelected, pEvent, pSelected->Data);

		(void)xrtMutexLock(&__xrtSignalState.Lock);
		pSelected->CallbackActive = false;
		__xrtSignalState.Callback = NULL;
		(void)xrtCondBroadcast(&__xrtSignalState.Idle);
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		if ( bReleaseRegistry ) {
			__xrtSignalWatchRelease(pSelected);
		}
		__xrtSignalWatchRelease(pSelected);
	}
}



/* 取出每个槽的原子待处理数量，并在调度线程中构造稳定事件。 */
static void __xrtSignalDispatchPending(void)
{
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		xsignalevent Event;
		uint64 iMaxId;

		memset(&Event, 0, sizeof(Event));
		(void)xrtMutexLock(&__xrtSignalState.Lock);
		if ( __xrtSignalState.Stopping ) {
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			return;
		}
		Event.Count = xrtAtomic32Exchange(
			&__xrtSignalState.Slots[i].Pending,
			0u,
			XMEMORY_ACQ_REL
		);
		if ( Event.Count == 0u ) {
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			continue;
		}
		if ( __xrtSignalState.Slots[i].Total >
			(UINT64_MAX - (uint64)Event.Count) ) {
			__xrtSignalState.Slots[i].Total = UINT64_MAX;
		} else {
			__xrtSignalState.Slots[i].Total += (uint64)Event.Count;
		}
		Event.Code = __xrtSignalState.Slots[i].Code;
		Event.SystemCode = (int32)xrtAtomic32Load(
			&__xrtSignalState.Slots[i].LastSystemCode,
			XMEMORY_RELAXED
		);
		Event.Total = __xrtSignalState.Slots[i].Total;
		Event.Name = __xrtSignalState.Slots[i].Name;
		iMaxId = __xrtSignalState.NextId;
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		Event.Time = xrtNow();
		__xrtSignalDispatchEvent(&Event, iMaxId);
	}
}



/* 唯一调度线程把原生通知转换为普通线程上下文中的用户回调。 */
static int32 __xrtSignalDispatchThread(ptr pData)
{
	(void)pData;
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	__xrtSignalState.DispatcherId = xrtThreadCurrentId();
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);

	for ( ;; ) {
		int iSystemCode = 0;

		if ( !__xrtSignalPlatformWait(&iSystemCode) ) {
			xsignalwatch* pRelease = NULL;
			bool bReport = false;

			(void)xrtMutexLock(&__xrtSignalState.Lock);
			if ( !__xrtSignalState.Stopping ) {
				bReport = true;
				__xrtSignalState.DispatchFailed = true;
				__xrtSignalState.DispatchSystemCode = iSystemCode;
				pRelease = __xrtSignalDetachLocked(XSIGNAL_NONE);
				for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
					int iRestoreCode = 0;
					uint32 iMode = xrtAtomic32Load(
						&__xrtSignalState.Slots[i].Mode,
						XMEMORY_ACQUIRE
					);

					if ( (iMode != XRT_SIGNAL_SLOT_DEFAULT) &&
						__xrtSignalPlatformRestore(i, &iRestoreCode) ) {
						xrtAtomic32Store(
							&__xrtSignalState.Slots[i].Mode,
							XRT_SIGNAL_SLOT_DEFAULT,
							XMEMORY_RELEASE
						);
					}
				}
			}
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			__xrtSignalReleaseList(pRelease);
			if ( bReport ) {
				__xrtSignalSetSystemError(
					"dispatch",
					iSystemCode,
					"signal dispatcher wait failed"
				);
			}
			break;
		}
		(void)xrtMutexLock(&__xrtSignalState.Lock);
		if ( __xrtSignalState.Stopping ) {
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			break;
		}
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		__xrtSignalDispatchPending();
	}

	(void)xrtMutexLock(&__xrtSignalState.Lock);
	__xrtSignalState.DispatcherId = 0u;
	__xrtSignalState.Running = false;
	(void)xrtCondBroadcast(&__xrtSignalState.Idle);
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	return 0;
}



/* 创建普通或一次性监听并在成功后接管 Owned 数据。 */
static xsignalwatch* __xrtSignalWatchCreate(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree,
	bool bOnce
)
{
	xsignalwatch* pWatch;
	xsignalslot* pSlot;
	int iSlot;
	int iSystemCode = 0;

	if ( pProc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iSlot = __xrtSignalRequire(Code, true);
	if ( iSlot < 0 ) {
		return NULL;
	}
	pWatch = (xsignalwatch*)xrtCalloc(1u, sizeof(*pWatch));
	if ( pWatch == NULL ) {
		return NULL;
	}
	pWatch->RefCount = 1;
	pWatch->Code = Code;
	pWatch->Proc = pProc;
	pWatch->Data = pData;
	pWatch->Free = pFree;
	pWatch->Once = bOnce;

	(void)xrtMutexLock(&__xrtSignalState.Lock);
	if ( !__xrtSignalStartLocked() ) {
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		xrtFree(pWatch);
		return NULL;
	}
	pSlot = &__xrtSignalState.Slots[iSlot];
	if ( xrtAtomic32Load(&pSlot->Mode, XMEMORY_ACQUIRE) !=
		XRT_SIGNAL_SLOT_WATCH ) {
		if ( !__xrtSignalPlatformMode(
			(uint32)iSlot,
			XRT_SIGNAL_SLOT_WATCH,
			&iSystemCode
		) ) {
			(void)xrtMutexUnlock(&__xrtSignalState.Lock);
			xrtFree(pWatch);
			__xrtSignalSetSystemError(
				"watch",
				iSystemCode,
				"native signal handler installation failed"
			);
			return NULL;
		}
		xrtAtomic32Store(
			&pSlot->Mode,
			XRT_SIGNAL_SLOT_WATCH,
			XMEMORY_RELEASE
		);
	}
	if ( __xrtSignalState.NextId == UINT64_MAX ) {
		(void)__xrtSignalRestoreUnusedLocked(iSlot);
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		xrtFree(pWatch);
		__xrtSignalSetError(
			XERR_RANGE,
			XSIGNAL_ERROR_STATE,
			"watch",
			"signal watch identifier space is exhausted"
		);
		return NULL;
	}
	pWatch->Id = ++__xrtSignalState.NextId;
	pWatch->Active = true;
	pWatch->Linked = true;
	pWatch->RefCount = 2;
	pWatch->Next = __xrtSignalState.Head;
	if ( __xrtSignalState.Head != NULL ) {
		__xrtSignalState.Head->Previous = pWatch;
	}
	__xrtSignalState.Head = pWatch;
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	return pWatch;
}



/* 释放最后监听时恢复该信号接管前的原生处理方式。 */
static bool __xrtSignalRestoreUnusedLocked(int iSlot)
{
	xsignalslot* pSlot = &__xrtSignalState.Slots[iSlot];
	int iSystemCode = 0;

	if ( __xrtSignalHasWatchLocked(pSlot->Code) ||
		(xrtAtomic32Load(&pSlot->Mode, XMEMORY_ACQUIRE) !=
		 XRT_SIGNAL_SLOT_WATCH) ) {
		return true;
	}
	if ( !__xrtSignalPlatformRestore((uint32)iSlot, &iSystemCode) ) {
		__xrtSignalSetSystemError(
			"restore",
			iSystemCode,
			"native signal disposition restore failed"
		);
		return false;
	}
	xrtAtomic32Store(
		&pSlot->Mode,
		XRT_SIGNAL_SLOT_DEFAULT,
		XMEMORY_RELEASE
	);
	return true;
}



/* 判断当前线程是不是唯一信号调度线程。 */
static bool __xrtSignalIsDispatcherLocked(void)
{
	return (__xrtSignalState.DispatcherId != 0u) &&
		(__xrtSignalState.DispatcherId == xrtThreadCurrentId());
}



/* 判断当前平台是否支持指定信号代码。 */
XRT_API bool xrtSignalSupported(xsignal Code)
{
	int iSlot;

	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	iSlot = __xrtSignalSlotIndex(Code);
	return (iSlot >= 0) && __xrtSignalState.Slots[iSlot].Supported;
}



/* 返回稳定信号名称。 */
XRT_API cstr xrtSignalName(xsignal Code)
{
	if ( !__xrtSignalEnsure() ) {
		return "UNKNOWN";
	}
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		if ( __xrtSignalState.Slots[i].Code == Code ) {
			return __xrtSignalState.Slots[i].Name;
		}
	}
	return "UNKNOWN";
}



/* 创建重复监听。 */
XRT_API xsignalwatch* xrtSignalOn(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
)
{
	return __xrtSignalWatchCreate(Code, pProc, pData, NULL, false);
}



/* 创建接管用户数据的重复监听。 */
XRT_API xsignalwatch* xrtSignalOnOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
)
{
	return __xrtSignalWatchCreate(Code, pProc, pData, pFree, false);
}



/* 创建一次性监听。 */
XRT_API xsignalwatch* xrtSignalOnce(
	xsignal Code,
	xsignalproc pProc,
	ptr pData
)
{
	return __xrtSignalWatchCreate(Code, pProc, pData, NULL, true);
}



/* 创建接管用户数据的一次性监听。 */
XRT_API xsignalwatch* xrtSignalOnceOwned(
	xsignal Code,
	xsignalproc pProc,
	ptr pData,
	xsignalfreeproc pFree
)
{
	return __xrtSignalWatchCreate(Code, pProc, pData, pFree, true);
}



/* 增加监听句柄引用。 */
XRT_API xsignalwatch* xrtSignalRef(xsignalwatch* pWatch)
{
	if ( (pWatch == NULL) || (xrtRefRetain(&pWatch->RefCount) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pWatch;
}



/* 注销监听，并等待其他线程上已经开始的回调。 */
XRT_API bool xrtSignalOff(xsignalwatch* pWatch)
{
	bool bReleaseRegistry = false;
	bool bResult = true;
	bool bSelf;
	int iSlot;

	if ( pWatch == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtRefRetain(&pWatch->RefCount) < 0 ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtSignalEnsure() ) {
		__xrtSignalWatchRelease(pWatch);
		return false;
	}
	iSlot = __xrtSignalSlotIndex(pWatch->Code);
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	pWatch->Active = false;
	bReleaseRegistry = __xrtSignalUnlinkLocked(pWatch);
	bSelf = __xrtSignalIsDispatcherLocked();
	while ( pWatch->CallbackActive && !bSelf ) {
		(void)xrtCondWait(&__xrtSignalState.Idle, &__xrtSignalState.Lock);
	}
	if ( iSlot >= 0 ) {
		bResult = __xrtSignalRestoreUnusedLocked(iSlot);
	}
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	if ( bReleaseRegistry ) {
		__xrtSignalWatchRelease(pWatch);
	}
	__xrtSignalWatchRelease(pWatch);
	return bResult;
}



/* 注销监听并释放一个调用方引用。 */
XRT_API void xrtSignalFree(xsignalwatch* pWatch)
{
	if ( pWatch == NULL ) {
		return;
	}
	(void)xrtSignalOff(pWatch);
	__xrtSignalWatchRelease(pWatch);
}



/* 查询监听是否仍然活动。 */
XRT_API bool xrtSignalActive(const xsignalwatch* pWatch)
{
	bool bActive;

	if ( pWatch == NULL ) {
		return false;
	}
	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	bActive = pWatch->Active;
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	return bActive;
}



/* 返回调度后端状态，并在故障时重建结构化系统错误。 */
XRT_API bool xrtSignalHealthy(void)
{
	bool bHealthy;
	int iSystemCode;

	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	bHealthy = !__xrtSignalState.DispatchFailed;
	iSystemCode = __xrtSignalState.DispatchSystemCode;
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	if ( !bHealthy ) {
		__xrtSignalSetSystemError(
			"dispatch",
			iSystemCode,
			"signal dispatcher wait failed"
		);
	}
	return bHealthy;
}



/* 返回监听的稳定信号代码。 */
XRT_API xsignal xrtSignalCode(const xsignalwatch* pWatch)
{
	return pWatch != NULL ? pWatch->Code : XSIGNAL_NONE;
}



/* 统一执行忽略或恢复，并释放匹配监听的注册表引用。 */
static bool __xrtSignalSetMode(xsignal Code, xsignalslotmode Mode)
{
	xsignalwatch* pRelease;
	xsignalslot* pSlot;
	bool bSelf;
	bool bResult;
	int iSystemCode = 0;
	int iSlot = __xrtSignalRequire(Code, true);

	if ( iSlot < 0 ) {
		return false;
	}
	pSlot = &__xrtSignalState.Slots[iSlot];
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	pRelease = __xrtSignalDetachLocked(Code);
	bSelf = __xrtSignalIsDispatcherLocked();
	while ( __xrtSignalHasCallbackLocked(Code) && !bSelf ) {
		(void)xrtCondWait(&__xrtSignalState.Idle, &__xrtSignalState.Lock);
	}
	if ( Mode == XRT_SIGNAL_SLOT_DEFAULT ) {
		bResult = __xrtSignalPlatformRestore((uint32)iSlot, &iSystemCode);
	} else {
		bResult = __xrtSignalPlatformMode((uint32)iSlot, Mode, &iSystemCode);
	}
	if ( bResult ) {
		xrtAtomic32Store(&pSlot->Mode, (uint32)Mode, XMEMORY_RELEASE);
	}
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	__xrtSignalReleaseList(pRelease);
	if ( !bResult ) {
		__xrtSignalSetSystemError(
			Mode == XRT_SIGNAL_SLOT_IGNORE ? "ignore" : "restore",
			iSystemCode,
			Mode == XRT_SIGNAL_SLOT_IGNORE ?
				"native signal ignore failed" :
				"native signal disposition restore failed"
		);
	}
	return bResult;
}



/* 忽略指定信号。 */
XRT_API bool xrtSignalIgnore(xsignal Code)
{
	return __xrtSignalSetMode(Code, XRT_SIGNAL_SLOT_IGNORE);
}



/* 恢复指定信号在 XRT 接管前的处理方式。 */
XRT_API bool xrtSignalRestore(xsignal Code)
{
	return __xrtSignalSetMode(Code, XRT_SIGNAL_SLOT_DEFAULT);
}



/* 恢复全部受支持信号，并保留第一个失败错误。 */
XRT_API bool xrtSignalRestoreAll(void)
{
	bool bResult = true;
	xerror* pFirst = NULL;

	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		if ( !__xrtSignalState.Slots[i].Supported ) {
			continue;
		}
		if ( !xrtSignalRestore(__xrtSignalState.Slots[i].Code) ) {
			if ( pFirst == NULL ) {
				pFirst = xrtTakeError();
			}
			bResult = false;
		}
	}
	if ( pFirst != NULL ) {
		xrtSetError(pFirst);
		xrtErrorFree(pFirst);
	}
	return bResult;
}



/* 向当前进程发送指定原生信号。 */
XRT_API bool xrtSignalRaise(xsignal Code)
{
	int iSystemCode = 0;
	int iSlot = __xrtSignalRequire(Code, true);

	if ( iSlot < 0 ) {
		return false;
	}
	if ( __xrtSignalPlatformRaise((uint32)iSlot, &iSystemCode) ) {
		return true;
	}
	if ( iSystemCode == 0 ) {
		__xrtSignalSetError(
			XERR_UNSUPPORTED,
			XSIGNAL_ERROR_UNSUPPORTED,
			"raise",
			"signal cannot be raised programmatically on this platform"
		);
	} else {
		__xrtSignalSetSystemError(
			"raise",
			iSystemCode,
			"native signal raise failed"
		);
	}
	return false;
}



/* 返回指定信号或全部信号的累计数。 */
XRT_API uint64 xrtSignalCount(xsignal Code)
{
	uint64 iTotal = 0u;
	int iSlot;

	if ( !__xrtSignalEnsure() ) {
		return 0u;
	}
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	if ( Code == XSIGNAL_NONE ) {
		for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
			if ( iTotal > (UINT64_MAX - __xrtSignalState.Slots[i].Total) ) {
				iTotal = UINT64_MAX;
				break;
			}
			iTotal += __xrtSignalState.Slots[i].Total;
		}
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		return iTotal;
	}
	iSlot = __xrtSignalSlotIndex(Code);
	if ( iSlot >= 0 ) {
		iTotal = __xrtSignalState.Slots[iSlot].Total;
	}
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	if ( iSlot < 0 ) {
		__xrtSignalSetError(
			XERR_ARGUMENT,
			XSIGNAL_ERROR_CODE,
			"count",
			"signal code is invalid"
		);
	}
	return iTotal;
}



/* 判断指定信号是否已经接收。 */
XRT_API bool xrtSignalReceived(xsignal Code)
{
	return xrtSignalCount(Code) != 0u;
}



/* 原子清除一个或全部信号的累计数和待处理数量。 */
XRT_API bool xrtSignalClear(xsignal Code)
{
	int iSlot = -1;

	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	if ( Code != XSIGNAL_NONE ) {
		iSlot = __xrtSignalSlotIndex(Code);
		if ( iSlot < 0 ) {
			__xrtSignalSetError(
				XERR_ARGUMENT,
				XSIGNAL_ERROR_CODE,
				"clear",
				"signal code is invalid"
			);
			return false;
		}
	}
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		if ( (Code == XSIGNAL_NONE) || ((int)i == iSlot) ) {
			__xrtSignalState.Slots[i].Total = 0u;
			(void)xrtAtomic32Exchange(
				&__xrtSignalState.Slots[i].Pending,
				0u,
				XMEMORY_ACQ_REL
			);
		}
	}
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	return true;
}



/* 恢复全部原生处理方式后停止调度线程并释放平台唤醒资源。 */
XRT_API bool xrtSignalShutdown(void)
{
	xsignalwatch* pRelease;
	xthread* pDispatcher;
	bool bRestore = true;
	xerror* pFirst = NULL;

	if ( !__xrtSignalEnsure() ) {
		return false;
	}
	(void)xrtMutexLock(&__xrtSignalState.Lock);
	if ( __xrtSignalIsDispatcherLocked() ) {
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		__xrtSignalSetError(
			XERR_STATE,
			XSIGNAL_ERROR_STATE,
			"shutdown",
			"signal dispatcher cannot shut itself down"
		);
		return false;
	}
	pRelease = __xrtSignalDetachLocked(XSIGNAL_NONE);
	while ( __xrtSignalHasCallbackLocked(XSIGNAL_NONE) ) {
		(void)xrtCondWait(&__xrtSignalState.Idle, &__xrtSignalState.Lock);
	}
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		int iSystemCode = 0;
		uint32 iMode = xrtAtomic32Load(
			&__xrtSignalState.Slots[i].Mode,
			XMEMORY_ACQUIRE
		);

		if ( (iMode != XRT_SIGNAL_SLOT_DEFAULT) &&
			!__xrtSignalPlatformRestore(i, &iSystemCode) ) {
			if ( pFirst == NULL ) {
				__xrtSignalSetSystemError(
					"shutdown",
					iSystemCode,
					"native signal disposition restore failed"
				);
				pFirst = xrtTakeError();
			}
			bRestore = false;
		} else {
			xrtAtomic32Store(
				&__xrtSignalState.Slots[i].Mode,
				XRT_SIGNAL_SLOT_DEFAULT,
				XMEMORY_RELEASE
			);
		}
	}
	if ( !bRestore ) {
		(void)xrtMutexUnlock(&__xrtSignalState.Lock);
		__xrtSignalReleaseList(pRelease);
		if ( pFirst != NULL ) {
			xrtSetError(pFirst);
			xrtErrorFree(pFirst);
		}
		return false;
	}
	__xrtSignalState.Stopping = true;
	pDispatcher = __xrtSignalState.Dispatcher;
	if ( __xrtSignalState.PlatformReady ) {
		__xrtSignalPlatformWake();
	}
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	__xrtSignalReleaseList(pRelease);

	if ( pDispatcher != NULL ) {
		(void)xrtThreadWait(pDispatcher);
		xrtThreadDestroy(pDispatcher);
	}
	while ( xrtAtomic32Load(
		&__xrtSignalState.HandlerActive,
		XMEMORY_ACQUIRE
	) != 0u ) {
		xrtThreadYield();
	}
	if ( __xrtSignalState.PlatformReady ) {
		__xrtSignalPlatformUnit();
	}

	(void)xrtMutexLock(&__xrtSignalState.Lock);
	__xrtSignalState.Dispatcher = NULL;
	__xrtSignalState.DispatcherId = 0u;
	__xrtSignalState.PlatformReady = false;
	__xrtSignalState.Running = false;
	__xrtSignalState.Stopping = false;
	__xrtSignalState.DispatchFailed = false;
	__xrtSignalState.DispatchSystemCode = 0;
	for ( uint32 i = 0; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		__xrtSignalState.Slots[i].Total = 0u;
		(void)xrtAtomic32Exchange(
			&__xrtSignalState.Slots[i].Pending,
			0u,
			XMEMORY_ACQ_REL
		);
	}
	(void)xrtCondBroadcast(&__xrtSignalState.Idle);
	(void)xrtMutexUnlock(&__xrtSignalState.Lock);
	return true;
}

#endif
