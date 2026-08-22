#ifndef XRT_SIGNAL_H
#define XRT_SIGNAL_H

/*
	进程信号处理基座。
	OS signal handler / Windows console handler 只记录事件并唤醒调度线程，
	所有用户回调都在 XRT 调度线程内执行，避免运行时重入。
*/

#define XRT_SIGNAL_SLOT_COUNT 7u

typedef struct xrtsignal_handle {
	volatile int32 iRefCount;
	int iCode;
	int bOnce;
	volatile int bActive;
	xrtsignal_fn Proc;
	ptr pUserData;
	xrtsignal_free_fn FreeProc;
	struct xrtsignal_handle* pPrev;
	struct xrtsignal_handle* pNext;
} xrtsignal_handle;

typedef struct xrtsignal_slot {
	int iCode;
	int iNativeCode;
	const char* sName;
	volatile int64 iCount;
	volatile int64 iDelivered;
	int bSupported;
	int bInstalled;
	#if !defined(_WIN32) && !defined(_WIN64)
		struct sigaction tOldAction;
	#endif
} xrtsignal_slot;

typedef struct xrtsignal_state {
	int bReady;
	int bStopping;
	int bDispatcherStarted;
	xmutex hLock;
	xcond hCond;
	xthread hThread;
	xrtsignal_handle* pHead;
	xrtsignal_slot Slots[XRT_SIGNAL_SLOT_COUNT];
	#if defined(_WIN32) || defined(_WIN64)
		int bConsoleInstalled;
		void (__cdecl *OldIntHandler)(int);
		void (__cdecl *OldTermHandler)(int);
		#ifdef SIGBREAK
			void (__cdecl *OldBreakHandler)(int);
		#endif
	#else
		int iPipeRead;
		int iPipeWrite;
	#endif
} xrtsignal_state;

static xrtsignal_state __xrtSignalState = { 0 };

static int __xrtSignalSlotIndex(int iCode)
{
	uint32 i;
	for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
		if ( __xrtSignalState.Slots[i].iCode == iCode ) {
			return (int)i;
		}
	}
	return -1;
}

static xrtsignal_slot* __xrtSignalSlot(int iCode)
{
	int iIndex = __xrtSignalSlotIndex(iCode);
	return iIndex >= 0 ? &__xrtSignalState.Slots[iIndex] : NULL;
}

static xrtsignal_handle* __xrtSignalHandleRetain(xrtsignal_handle* pHandle)
{
	if ( pHandle != NULL ) {
		xrtAtomicRefRetain(&pHandle->iRefCount);
	}
	return pHandle;
}

static void __xrtSignalHandleRelease(xrtsignal_handle* pHandle)
{
	if ( pHandle == NULL ) {
		return;
	}
	if ( xrtAtomicRefRelease(&pHandle->iRefCount) == 0 ) {
		if ( pHandle->FreeProc != NULL ) {
			pHandle->FreeProc(pHandle->pUserData);
		}
		xrtFree(pHandle);
	}
}

static void __xrtSignalNotifyCode(int iCode, int iPlatformCode)
{
	xrtsignal_slot* pSlot = __xrtSignalSlot(iCode);
	(void)iPlatformCode;
	if ( pSlot == NULL || !pSlot->bSupported ) {
		return;
	}
	__xrtAtomicAddFetch64(&pSlot->iCount, 1);
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtSignalState.hLock != NULL && __xrtSignalState.hCond != NULL ) {
			xrtMutexLock(__xrtSignalState.hLock);
			xrtCondSignal(__xrtSignalState.hCond);
			xrtMutexUnlock(__xrtSignalState.hLock);
		}
	#else
		if ( __xrtSignalState.iPipeWrite >= 0 ) {
			unsigned char c = (unsigned char)iCode;
			(void)write(__xrtSignalState.iPipeWrite, &c, 1);
		}
	#endif
}

#if defined(_WIN32) || defined(_WIN64)
static BOOL WINAPI __xrtSignalConsoleHandler(DWORD iControlType)
{
	switch ( iControlType ) {
		case CTRL_C_EVENT:
			__xrtSignalNotifyCode(XRT_SIGNAL_INT, (int)iControlType);
			return TRUE;
		case CTRL_BREAK_EVENT:
			__xrtSignalNotifyCode(XRT_SIGNAL_BREAK, (int)iControlType);
			return TRUE;
		case CTRL_CLOSE_EVENT:
			__xrtSignalNotifyCode(XRT_SIGNAL_CLOSE, (int)iControlType);
			return TRUE;
		case CTRL_LOGOFF_EVENT:
			__xrtSignalNotifyCode(XRT_SIGNAL_LOGOFF, (int)iControlType);
			return TRUE;
		case CTRL_SHUTDOWN_EVENT:
			__xrtSignalNotifyCode(XRT_SIGNAL_SHUTDOWN, (int)iControlType);
			return TRUE;
		default:
			return FALSE;
	}
}

static void __cdecl __xrtSignalCHandler(int iNativeCode)
{
	switch ( iNativeCode ) {
		case SIGINT:
			__xrtSignalNotifyCode(XRT_SIGNAL_INT, iNativeCode);
			break;
		case SIGTERM:
			__xrtSignalNotifyCode(XRT_SIGNAL_TERM, iNativeCode);
			break;
		#ifdef SIGBREAK
		case SIGBREAK:
			__xrtSignalNotifyCode(XRT_SIGNAL_BREAK, iNativeCode);
			break;
		#endif
		default:
			break;
	}
}
#else
static void __xrtSignalPosixHandler(int iNativeCode)
{
	switch ( iNativeCode ) {
		case SIGINT:
			__xrtSignalNotifyCode(XRT_SIGNAL_INT, iNativeCode);
			break;
		case SIGTERM:
			__xrtSignalNotifyCode(XRT_SIGNAL_TERM, iNativeCode);
			break;
		#ifdef SIGHUP
		case SIGHUP:
			__xrtSignalNotifyCode(XRT_SIGNAL_HUP, iNativeCode);
			break;
		#endif
		default:
			break;
	}
}
#endif

static void __xrtSignalInitSlots()
{
	memset(__xrtSignalState.Slots, 0, sizeof(__xrtSignalState.Slots));
	__xrtSignalState.Slots[0].iCode = XRT_SIGNAL_INT;
	__xrtSignalState.Slots[0].iNativeCode = SIGINT;
	__xrtSignalState.Slots[0].sName = "INT";
	__xrtSignalState.Slots[0].bSupported = 1;
	__xrtSignalState.Slots[1].iCode = XRT_SIGNAL_TERM;
	__xrtSignalState.Slots[1].iNativeCode = SIGTERM;
	__xrtSignalState.Slots[1].sName = "TERM";
	__xrtSignalState.Slots[1].bSupported = 1;
	__xrtSignalState.Slots[2].iCode = XRT_SIGNAL_HUP;
	#ifdef SIGHUP
		__xrtSignalState.Slots[2].iNativeCode = SIGHUP;
		__xrtSignalState.Slots[2].bSupported = 1;
	#else
		__xrtSignalState.Slots[2].iNativeCode = 0;
		__xrtSignalState.Slots[2].bSupported = 0;
	#endif
	__xrtSignalState.Slots[2].sName = "HUP";
	__xrtSignalState.Slots[3].iCode = XRT_SIGNAL_BREAK;
	#ifdef SIGBREAK
		__xrtSignalState.Slots[3].iNativeCode = SIGBREAK;
	#else
		__xrtSignalState.Slots[3].iNativeCode = 0;
	#endif
	__xrtSignalState.Slots[3].sName = "BREAK";
	__xrtSignalState.Slots[3].bSupported =
	#if defined(_WIN32) || defined(_WIN64)
		1;
	#else
		0;
	#endif
	__xrtSignalState.Slots[4].iCode = XRT_SIGNAL_CLOSE;
	__xrtSignalState.Slots[4].iNativeCode = 0;
	__xrtSignalState.Slots[4].sName = "CLOSE";
	__xrtSignalState.Slots[4].bSupported =
	#if defined(_WIN32) || defined(_WIN64)
		1;
	#else
		0;
	#endif
	__xrtSignalState.Slots[5].iCode = XRT_SIGNAL_LOGOFF;
	__xrtSignalState.Slots[5].iNativeCode = 0;
	__xrtSignalState.Slots[5].sName = "LOGOFF";
	__xrtSignalState.Slots[5].bSupported =
	#if defined(_WIN32) || defined(_WIN64)
		1;
	#else
		0;
	#endif
	__xrtSignalState.Slots[6].iCode = XRT_SIGNAL_SHUTDOWN;
	__xrtSignalState.Slots[6].iNativeCode = 0;
	__xrtSignalState.Slots[6].sName = "SHUTDOWN";
	__xrtSignalState.Slots[6].bSupported =
	#if defined(_WIN32) || defined(_WIN64)
		1;
	#else
		0;
	#endif
}

static void __xrtSignalInstallSlot(xrtsignal_slot* pSlot)
{
	if ( pSlot == NULL || !pSlot->bSupported || pSlot->bInstalled ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pSlot->iCode == XRT_SIGNAL_INT ) {
			__xrtSignalState.OldIntHandler = signal(SIGINT, __xrtSignalCHandler);
			pSlot->bInstalled = 1;
		} else if ( pSlot->iCode == XRT_SIGNAL_TERM ) {
			__xrtSignalState.OldTermHandler = signal(SIGTERM, __xrtSignalCHandler);
			pSlot->bInstalled = 1;
		}
		#ifdef SIGBREAK
		else if ( pSlot->iCode == XRT_SIGNAL_BREAK ) {
			__xrtSignalState.OldBreakHandler = signal(SIGBREAK, __xrtSignalCHandler);
			pSlot->bInstalled = 1;
		}
		#endif
		if ( !__xrtSignalState.bConsoleInstalled ) {
			SetConsoleCtrlHandler(__xrtSignalConsoleHandler, TRUE);
			__xrtSignalState.bConsoleInstalled = 1;
		}
	#else
		struct sigaction tAction;
		memset(&tAction, 0, sizeof(tAction));
		tAction.sa_handler = __xrtSignalPosixHandler;
		sigemptyset(&tAction.sa_mask);
		tAction.sa_flags = 0;
		if ( sigaction(pSlot->iNativeCode, &tAction, &pSlot->tOldAction) == 0 ) {
			pSlot->bInstalled = 1;
		}
	#endif
}

static void __xrtSignalRestoreSlot(xrtsignal_slot* pSlot)
{
	if ( pSlot == NULL || !pSlot->bInstalled ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pSlot->iCode == XRT_SIGNAL_INT ) {
			signal(SIGINT, __xrtSignalState.OldIntHandler ? __xrtSignalState.OldIntHandler : SIG_DFL);
			__xrtSignalState.OldIntHandler = NULL;
		} else if ( pSlot->iCode == XRT_SIGNAL_TERM ) {
			signal(SIGTERM, __xrtSignalState.OldTermHandler ? __xrtSignalState.OldTermHandler : SIG_DFL);
			__xrtSignalState.OldTermHandler = NULL;
		}
		#ifdef SIGBREAK
		else if ( pSlot->iCode == XRT_SIGNAL_BREAK ) {
			signal(SIGBREAK, __xrtSignalState.OldBreakHandler ? __xrtSignalState.OldBreakHandler : SIG_DFL);
			__xrtSignalState.OldBreakHandler = NULL;
		}
		#endif
	#else
		sigaction(pSlot->iNativeCode, &pSlot->tOldAction, NULL);
		memset(&pSlot->tOldAction, 0, sizeof(pSlot->tOldAction));
	#endif
	pSlot->bInstalled = 0;
}

static int __xrtSignalHasActiveForCode(int iCode)
{
	xrtsignal_handle* pNode = __xrtSignalState.pHead;
	while ( pNode != NULL ) {
		if ( pNode->iCode == iCode && pNode->bActive ) {
			return 1;
		}
		pNode = pNode->pNext;
	}
	return 0;
}

static void __xrtSignalRemoveInactiveLocked()
{
	xrtsignal_handle* pNode = __xrtSignalState.pHead;
	while ( pNode != NULL ) {
		xrtsignal_handle* pNext = pNode->pNext;
		if ( !pNode->bActive ) {
			if ( pNode->pPrev != NULL ) {
				pNode->pPrev->pNext = pNode->pNext;
			} else {
				__xrtSignalState.pHead = pNode->pNext;
			}
			if ( pNode->pNext != NULL ) {
				pNode->pNext->pPrev = pNode->pPrev;
			}
			pNode->pPrev = NULL;
			pNode->pNext = NULL;
			__xrtSignalHandleRelease(pNode);
		}
		pNode = pNext;
	}
}

static void __xrtSignalDispatchCode(int iCode, int64 iCount)
{
	xrtsignal_handle* arrHandlers[128];
	uint32 iHandlerCount = 0;
	uint32 i;
	xrtsignal_slot* pSlot;
	xrtsignal_event tEvent;

	if ( iCount <= 0 ) {
		return;
	}
	pSlot = __xrtSignalSlot(iCode);
	if ( pSlot == NULL ) {
		return;
	}
	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iCode = pSlot->iCode;
	tEvent.iPlatformCode = pSlot->iNativeCode;
	tEvent.iCount = (uint64)iCount;
	tEvent.iTime = xrtNow();
	tEvent.sName = pSlot->sName;

	xrtMutexLock(__xrtSignalState.hLock);
	{
		xrtsignal_handle* pNode = __xrtSignalState.pHead;
		while ( pNode != NULL && iHandlerCount < (uint32)(sizeof(arrHandlers) / sizeof(arrHandlers[0])) ) {
			if ( pNode->iCode == iCode && pNode->bActive ) {
				arrHandlers[iHandlerCount++] = __xrtSignalHandleRetain(pNode);
			}
			pNode = pNode->pNext;
		}
	}
	xrtMutexUnlock(__xrtSignalState.hLock);

	for ( i = 0; i < iHandlerCount; ++i ) {
		xrtsignal_handle* pHandle = arrHandlers[i];
		if ( pHandle != NULL && pHandle->bActive && pHandle->Proc != NULL ) {
			pHandle->Proc(&tEvent, pHandle->pUserData);
			if ( pHandle->bOnce ) {
				xrtSignalOff(pHandle);
			}
		}
		__xrtSignalHandleRelease(pHandle);
	}
}

static uint32 __xrtSignalDispatcher(ptr pParam)
{
	(void)pParam;
	while ( !__xrtSignalState.bStopping ) {
		uint32 i;
		#if defined(_WIN32) || defined(_WIN64)
			xrtMutexLock(__xrtSignalState.hLock);
			xrtCondWaitTimeout(__xrtSignalState.hCond, __xrtSignalState.hLock, 100);
			xrtMutexUnlock(__xrtSignalState.hLock);
		#else
			unsigned char arrBuf[64];
			if ( __xrtSignalState.iPipeRead >= 0 ) {
				(void)read(__xrtSignalState.iPipeRead, arrBuf, sizeof(arrBuf));
			}
		#endif
		for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
			xrtsignal_slot* pSlot = &__xrtSignalState.Slots[i];
			int64 iCurrent = __xrtAtomicLoad64(&pSlot->iCount);
			int64 iDelivered = __xrtAtomicLoad64(&pSlot->iDelivered);
			if ( iCurrent > iDelivered ) {
				__xrtAtomicStore64(&pSlot->iDelivered, iCurrent);
				__xrtSignalDispatchCode(pSlot->iCode, iCurrent);
			}
		}
		xrtMutexLock(__xrtSignalState.hLock);
		__xrtSignalRemoveInactiveLocked();
		xrtMutexUnlock(__xrtSignalState.hLock);
	}
	return 0;
}

XXAPI bool xrtSignalInit(void)
{
	if ( __xrtSignalState.bReady ) {
		return TRUE;
	}
	memset(&__xrtSignalState, 0, sizeof(__xrtSignalState));
	#if !defined(_WIN32) && !defined(_WIN64)
		__xrtSignalState.iPipeRead = -1;
		__xrtSignalState.iPipeWrite = -1;
	#endif
	__xrtSignalInitSlots();
	__xrtSignalState.hLock = xrtMutexCreate();
	__xrtSignalState.hCond = xrtCondCreate();
	if ( __xrtSignalState.hLock == NULL || __xrtSignalState.hCond == NULL ) {
		xrtSignalUnit();
		return FALSE;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		{
			int fds[2];
			if ( pipe(fds) != 0 ) {
				xrtSignalUnit();
				return FALSE;
			}
			__xrtSignalState.iPipeRead = fds[0];
			__xrtSignalState.iPipeWrite = fds[1];
		}
	#endif
	__xrtSignalState.bReady = 1;
	__xrtSignalState.hThread = xrtThreadCreate(__xrtSignalDispatcher, NULL, 0);
	if ( __xrtSignalState.hThread == NULL ) {
		xrtSignalUnit();
		return FALSE;
	}
	__xrtSignalState.bDispatcherStarted = 1;
	return TRUE;
}

XXAPI void xrtSignalUnit(void)
{
	uint32 i;
	xrtsignal_handle* pNode;
	if ( !__xrtSignalState.bReady &&
		 __xrtSignalState.hLock == NULL &&
		 __xrtSignalState.hCond == NULL ) {
		return;
	}
	__xrtSignalState.bStopping = 1;
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtSignalState.hLock != NULL && __xrtSignalState.hCond != NULL ) {
			xrtMutexLock(__xrtSignalState.hLock);
			xrtCondBroadcast(__xrtSignalState.hCond);
			xrtMutexUnlock(__xrtSignalState.hLock);
		}
	#else
		if ( __xrtSignalState.iPipeWrite >= 0 ) {
			unsigned char c = 0;
			(void)write(__xrtSignalState.iPipeWrite, &c, 1);
		}
	#endif
	if ( __xrtSignalState.hThread != NULL ) {
		xrtThreadWait(__xrtSignalState.hThread);
		xrtThreadDestroy(__xrtSignalState.hThread);
		__xrtSignalState.hThread = NULL;
	}
	for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
		__xrtSignalRestoreSlot(&__xrtSignalState.Slots[i]);
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( __xrtSignalState.bConsoleInstalled ) {
			SetConsoleCtrlHandler(__xrtSignalConsoleHandler, FALSE);
			__xrtSignalState.bConsoleInstalled = 0;
		}
	#else
		if ( __xrtSignalState.iPipeRead >= 0 ) {
			close(__xrtSignalState.iPipeRead);
			__xrtSignalState.iPipeRead = -1;
		}
		if ( __xrtSignalState.iPipeWrite >= 0 ) {
			close(__xrtSignalState.iPipeWrite);
			__xrtSignalState.iPipeWrite = -1;
		}
	#endif
	pNode = __xrtSignalState.pHead;
	while ( pNode != NULL ) {
		xrtsignal_handle* pNext = pNode->pNext;
		pNode->bActive = 0;
		pNode->pPrev = NULL;
		pNode->pNext = NULL;
		__xrtSignalHandleRelease(pNode);
		pNode = pNext;
	}
	__xrtSignalState.pHead = NULL;
	if ( __xrtSignalState.hCond != NULL ) {
		xrtCondDestroy(__xrtSignalState.hCond);
		__xrtSignalState.hCond = NULL;
	}
	if ( __xrtSignalState.hLock != NULL ) {
		xrtMutexDestroy(__xrtSignalState.hLock);
		__xrtSignalState.hLock = NULL;
	}
	__xrtSignalState.bReady = 0;
	__xrtSignalState.bStopping = 0;
}

XXAPI bool xrtSignalSupported(int iCode)
{
	xrtsignal_slot* pSlot;
	if ( !__xrtSignalState.bReady ) {
		__xrtSignalInitSlots();
	}
	pSlot = __xrtSignalSlot(iCode);
	return pSlot != NULL && pSlot->bSupported;
}

XXAPI const char* xrtSignalName(int iCode)
{
	xrtsignal_slot* pSlot;
	if ( !__xrtSignalState.bReady ) {
		__xrtSignalInitSlots();
	}
	pSlot = __xrtSignalSlot(iCode);
	return pSlot != NULL ? pSlot->sName : "UNKNOWN";
}

XXAPI xrtsignal_handle* xrtSignalOnEx(int iCode, bool bOnce, xrtsignal_fn Proc, ptr pUserData, xrtsignal_free_fn FreeProc)
{
	xrtsignal_handle* pHandle;
	xrtsignal_slot* pSlot;
	if ( Proc == NULL ) {
		xrtSetError("signal handler is null.", FALSE);
		return NULL;
	}
	if ( !xrtSignalInit() ) {
		return NULL;
	}
	pSlot = __xrtSignalSlot(iCode);
	if ( pSlot == NULL ) {
		xrtSetError("invalid signal code.", FALSE);
		return NULL;
	}
	if ( !pSlot->bSupported ) {
		xrtSetError("signal is not supported on this platform.", FALSE);
		return NULL;
	}
	pHandle = (xrtsignal_handle*)xrtCalloc(1u, sizeof(*pHandle));
	if ( pHandle == NULL ) {
		return NULL;
	}
	pHandle->iRefCount = 2;
	pHandle->iCode = iCode;
	pHandle->bOnce = bOnce ? 1 : 0;
	pHandle->bActive = 1;
	pHandle->Proc = Proc;
	pHandle->pUserData = pUserData;
	pHandle->FreeProc = FreeProc;
	xrtMutexLock(__xrtSignalState.hLock);
	__xrtSignalInstallSlot(pSlot);
	pHandle->pNext = __xrtSignalState.pHead;
	if ( __xrtSignalState.pHead != NULL ) {
		__xrtSignalState.pHead->pPrev = pHandle;
	}
	__xrtSignalState.pHead = pHandle;
	xrtMutexUnlock(__xrtSignalState.hLock);
	return pHandle;
}

XXAPI xrtsignal_handle* xrtSignalOn(int iCode, xrtsignal_fn Proc, ptr pUserData, xrtsignal_free_fn FreeProc)
{
	return xrtSignalOnEx(iCode, FALSE, Proc, pUserData, FreeProc);
}

XXAPI xrtsignal_handle* xrtSignalOnce(int iCode, xrtsignal_fn Proc, ptr pUserData, xrtsignal_free_fn FreeProc)
{
	return xrtSignalOnEx(iCode, TRUE, Proc, pUserData, FreeProc);
}

XXAPI bool xrtSignalOff(xrtsignal_handle* pHandle)
{
	if ( pHandle == NULL || !__xrtSignalState.bReady ) {
		return FALSE;
	}
	xrtMutexLock(__xrtSignalState.hLock);
	if ( pHandle->bActive ) {
		xrtsignal_slot* pSlot;
		pHandle->bActive = 0;
		pSlot = __xrtSignalSlot(pHandle->iCode);
		if ( pSlot != NULL && !__xrtSignalHasActiveForCode(pHandle->iCode) ) {
			__xrtSignalRestoreSlot(pSlot);
		}
	}
	__xrtSignalRemoveInactiveLocked();
	xrtMutexUnlock(__xrtSignalState.hLock);
	return TRUE;
}

XXAPI void xrtSignalHandleRelease(xrtsignal_handle* pHandle)
{
	__xrtSignalHandleRelease(pHandle);
}

XXAPI bool xrtSignalHandleActive(xrtsignal_handle* pHandle)
{
	return pHandle != NULL && pHandle->bActive;
}

XXAPI int xrtSignalHandleCode(xrtsignal_handle* pHandle)
{
	return pHandle != NULL ? pHandle->iCode : 0;
}

XXAPI bool xrtSignalRestore(int iCode)
{
	xrtsignal_handle* pNode;
	xrtsignal_slot* pSlot;
	if ( !__xrtSignalState.bReady ) {
		return TRUE;
	}
	pSlot = __xrtSignalSlot(iCode);
	if ( pSlot == NULL ) {
		return FALSE;
	}
	xrtMutexLock(__xrtSignalState.hLock);
	pNode = __xrtSignalState.pHead;
	while ( pNode != NULL ) {
		if ( pNode->iCode == iCode ) {
			pNode->bActive = 0;
		}
		pNode = pNode->pNext;
	}
	__xrtSignalRestoreSlot(pSlot);
	__xrtSignalRemoveInactiveLocked();
	xrtMutexUnlock(__xrtSignalState.hLock);
	return TRUE;
}

XXAPI void xrtSignalRestoreAll(void)
{
	uint32 i;
	if ( !__xrtSignalState.bReady ) {
		return;
	}
	for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
		xrtSignalRestore(__xrtSignalState.Slots[i].iCode);
	}
}

XXAPI bool xrtSignalIgnore(int iCode)
{
	xrtsignal_slot* pSlot;
	if ( !xrtSignalInit() ) {
		return FALSE;
	}
	pSlot = __xrtSignalSlot(iCode);
	if ( pSlot == NULL || !pSlot->bSupported ) {
		return FALSE;
	}
	xrtSignalRestore(iCode);
	#if defined(_WIN32) || defined(_WIN64)
		if ( iCode == XRT_SIGNAL_INT ) {
			signal(SIGINT, SIG_IGN);
		} else if ( iCode == XRT_SIGNAL_TERM ) {
			signal(SIGTERM, SIG_IGN);
		}
		#ifdef SIGBREAK
		else if ( iCode == XRT_SIGNAL_BREAK ) {
			signal(SIGBREAK, SIG_IGN);
		}
		#endif
	#else
		signal(pSlot->iNativeCode, SIG_IGN);
	#endif
	return TRUE;
}

XXAPI bool xrtSignalRaise(int iCode)
{
	xrtsignal_slot* pSlot;
	if ( !__xrtSignalState.bReady ) {
		__xrtSignalInitSlots();
	}
	pSlot = __xrtSignalSlot(iCode);
	if ( pSlot == NULL || !pSlot->bSupported ) {
		return FALSE;
	}
	if ( pSlot->iNativeCode != 0 ) {
		return raise(pSlot->iNativeCode) == 0;
	}
	__xrtSignalNotifyCode(iCode, 0);
	return TRUE;
}

XXAPI uint64 xrtSignalCount(int iCode)
{
	xrtsignal_slot* pSlot;
	if ( !__xrtSignalState.bReady ) {
		__xrtSignalInitSlots();
	}
	if ( iCode == 0 ) {
		uint64 iTotal = 0;
		uint32 i;
		for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
			iTotal += (uint64)__xrtAtomicLoad64(&__xrtSignalState.Slots[i].iCount);
		}
		return iTotal;
	}
	pSlot = __xrtSignalSlot(iCode);
	return pSlot != NULL ? (uint64)__xrtAtomicLoad64(&pSlot->iCount) : 0u;
}

XXAPI bool xrtSignalReceived(int iCode)
{
	return xrtSignalCount(iCode) > 0u;
}

XXAPI void xrtSignalClear(int iCode)
{
	uint32 i;
	if ( !__xrtSignalState.bReady ) {
		__xrtSignalInitSlots();
	}
	for ( i = 0; i < XRT_SIGNAL_SLOT_COUNT; ++i ) {
		xrtsignal_slot* pSlot = &__xrtSignalState.Slots[i];
		if ( iCode == 0 || pSlot->iCode == iCode ) {
			__xrtAtomicStore64(&pSlot->iCount, 0);
			__xrtAtomicStore64(&pSlot->iDelivered, 0);
		}
	}
}

#endif
