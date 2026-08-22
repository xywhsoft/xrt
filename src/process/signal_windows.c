#include "../internal/xrt_signal.h"

#if defined(XRT_FEATURE_SIGNAL) && (defined(_WIN32) || defined(_WIN64))

#include <errno.h>
#include <signal.h>



typedef void (__cdecl *xrtsignalnativeproc)(int iSystemCode);

static HANDLE __xrtSignalEvent;
static xrtsignalnativeproc __xrtSignalOldProc[4];
static bool __xrtSignalProcSaved[4];
static bool __xrtSignalConsoleInstalled;
static xatomic32 __xrtSignalCRTMode = XRT_ATOMIC32_INIT(
	XRT_SIGNAL_SLOT_DEFAULT
);
static xatomic32 __xrtSignalCRTActive = XRT_ATOMIC32_INIT(0u);



/* 把 CRT 信号映射到稳定内部槽。 */
static int __xrtSignalCRTSlot(int iSystemCode)
{
	if ( iSystemCode == SIGTERM ) {
		return 1;
	}
	return -1;
}



/* CRT 信号处理器只记录事件并唤醒普通调度线程。 */
static void __cdecl __xrtSignalCRTHandler(int iSystemCode)
{
	int iSavedError = errno;
	int iSlot = __xrtSignalCRTSlot(iSystemCode);
	uint32 iMode;

	(void)xrtAtomic32FetchAdd(
		&__xrtSignalCRTActive,
		1u,
		XMEMORY_ACQ_REL
	);
	iMode = xrtAtomic32Load(&__xrtSignalCRTMode, XMEMORY_ACQUIRE);
	if ( (iSlot >= 0) && (iMode == XRT_SIGNAL_SLOT_WATCH) ) {
		__xrtSignalNotify((uint32)iSlot, (int32)iSystemCode);
	}
	if ( xrtAtomic32Load(&__xrtSignalCRTMode, XMEMORY_ACQUIRE) ==
		XRT_SIGNAL_SLOT_WATCH ) {
		(void)signal(SIGTERM, __xrtSignalCRTHandler);
	}
	(void)xrtAtomic32FetchSub(
		&__xrtSignalCRTActive,
		1u,
		XMEMORY_RELEASE
	);
	errno = iSavedError;
}



/* 把 Windows 控制台事件映射到内部槽。 */
static int __xrtSignalConsoleSlot(DWORD iControlCode)
{
	switch ( iControlCode ) {
		case CTRL_C_EVENT:
			return 0;
		case CTRL_BREAK_EVENT:
			return 3;
		case CTRL_CLOSE_EVENT:
			return 4;
		case CTRL_LOGOFF_EVENT:
			return 5;
		case CTRL_SHUTDOWN_EVENT:
			return 6;
		default:
			return -1;
	}
}



/* 控制台处理器不加锁；默认模式返回 FALSE，让后续处理器继续接管。 */
static BOOL WINAPI __xrtSignalConsoleHandler(DWORD iControlCode)
{
	int iSlot = __xrtSignalConsoleSlot(iControlCode);
	uint32 iMode;

	if ( iSlot < 0 ) {
		return FALSE;
	}
	iMode = xrtAtomic32Load(
		&__xrtSignalState.Slots[iSlot].Mode,
		XMEMORY_ACQUIRE
	);
	if ( iMode == XRT_SIGNAL_SLOT_WATCH ) {
		__xrtSignalNotify((uint32)iSlot, (int32)iControlCode);
		return TRUE;
	}
	return iMode == XRT_SIGNAL_SLOT_IGNORE ? TRUE : FALSE;
}



/* 创建自动复位事件，避免每个信号都进入内核等待队列。 */
bool __xrtSignalPlatformInit(void)
{
	if ( __xrtSignalEvent != NULL ) {
		return true;
	}
	__xrtSignalEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	if ( __xrtSignalEvent != NULL ) {
		return true;
	}
	__xrtErrorSetSystem(
		"xrt.signal",
		XSIGNAL_ERROR_SYSTEM,
		"event.create",
		(int)GetLastError(),
		"signal wake event creation failed"
	);
	return false;
}



/* 关闭调度线程已经停止使用的事件。 */
void __xrtSignalPlatformUnit(void)
{
	HANDLE hEvent = __xrtSignalEvent;

	__xrtSignalEvent = NULL;
	if ( hEvent != NULL ) {
		(void)CloseHandle(hEvent);
	}
}



/* 等待自动复位事件。 */
bool __xrtSignalPlatformWait(int* pSystemCode)
{
	DWORD iResult = WaitForSingleObject(__xrtSignalEvent, INFINITE);

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( iResult == WAIT_OBJECT_0 ) {
		return true;
	}
	if ( pSystemCode != NULL ) {
		*pSystemCode = (int)GetLastError();
	}
	return false;
}



/* SetEvent 可由 CRT 与控制台回调线程直接调用。 */
void __xrtSignalPlatformWake(void)
{
	if ( __xrtSignalEvent != NULL ) {
		(void)SetEvent(__xrtSignalEvent);
	}
}



/* 判断除指定槽外是否仍需要控制台处理器。 */
static bool __xrtSignalConsoleNeeded(uint32 iExcept)
{
	for ( uint32 i = 0u; i < XRT_SIGNAL_SLOT_COUNT; i++ ) {
		if ( (i == 1u) || (i == 2u) ) {
			continue;
		}
		if ( (i != iExcept) &&
			(xrtAtomic32Load(
				&__xrtSignalState.Slots[i].Mode,
				XMEMORY_ACQUIRE
			) != XRT_SIGNAL_SLOT_DEFAULT) ) {
			return true;
		}
	}
	return false;
}



/* 设置 CRT 或控制台信号处理方式。 */
bool __xrtSignalPlatformMode(
	uint32 iSlot,
	xsignalslotmode Mode,
	int* pSystemCode
)
{
	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( Mode == XRT_SIGNAL_SLOT_DEFAULT ) {
		return false;
	}
	if ( iSlot == 1u ) {
		uint32 iOldMode = xrtAtomic32Exchange(
			&__xrtSignalCRTMode,
			(uint32)Mode,
			XMEMORY_ACQ_REL
		);

		while ( xrtAtomic32Load(
			&__xrtSignalCRTActive,
			XMEMORY_ACQUIRE
		) != 0u ) {
			xrtThreadYield();
		}
		xrtsignalnativeproc pOld = signal(
			__xrtSignalState.Slots[iSlot].SystemCode,
			Mode == XRT_SIGNAL_SLOT_IGNORE ?
				SIG_IGN : __xrtSignalCRTHandler
		);

		if ( pOld == SIG_ERR ) {
			xrtAtomic32Store(
				&__xrtSignalCRTMode,
				iOldMode,
				XMEMORY_RELEASE
			);
			if ( pSystemCode != NULL ) {
				*pSystemCode = errno;
			}
			return false;
		}
		if ( !__xrtSignalProcSaved[iSlot] ) {
			__xrtSignalOldProc[iSlot] = pOld;
			__xrtSignalProcSaved[iSlot] = true;
		}
		return true;
	}
	if ( (iSlot == 2u) || (iSlot >= XRT_SIGNAL_SLOT_COUNT) ) {
		return false;
	}
	if ( !__xrtSignalConsoleInstalled ) {
		if ( !SetConsoleCtrlHandler(__xrtSignalConsoleHandler, TRUE) ) {
			if ( pSystemCode != NULL ) {
				*pSystemCode = (int)GetLastError();
			}
			return false;
		}
		__xrtSignalConsoleInstalled = true;
	}
	return true;
}



/* 恢复 CRT 旧处理器，或移除不再需要的控制台处理器。 */
bool __xrtSignalPlatformRestore(uint32 iSlot, int* pSystemCode)
{
	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( iSlot == 1u ) {
		uint32 iOldMode;

		if ( !__xrtSignalProcSaved[iSlot] ) {
			return true;
		}
		iOldMode = xrtAtomic32Exchange(
			&__xrtSignalCRTMode,
			XRT_SIGNAL_SLOT_DEFAULT,
			XMEMORY_ACQ_REL
		);
		while ( xrtAtomic32Load(
			&__xrtSignalCRTActive,
			XMEMORY_ACQUIRE
		) != 0u ) {
			xrtThreadYield();
		}
		if ( signal(
			__xrtSignalState.Slots[iSlot].SystemCode,
			__xrtSignalOldProc[iSlot]
		) == SIG_ERR ) {
			xrtAtomic32Store(
				&__xrtSignalCRTMode,
				iOldMode,
				XMEMORY_RELEASE
			);
			if ( pSystemCode != NULL ) {
				*pSystemCode = errno;
			}
			return false;
		}
		__xrtSignalOldProc[iSlot] = NULL;
		__xrtSignalProcSaved[iSlot] = false;
		return true;
	}
	if ( (iSlot == 2u) || (iSlot >= XRT_SIGNAL_SLOT_COUNT) ||
		!__xrtSignalConsoleInstalled ||
		__xrtSignalConsoleNeeded(iSlot) ) {
		return true;
	}
	if ( !SetConsoleCtrlHandler(__xrtSignalConsoleHandler, FALSE) ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = (int)GetLastError();
		}
		return false;
	}
	__xrtSignalConsoleInstalled = false;
	return true;
}



/* INT 在监听状态直接入队，避免 GenerateConsoleCtrlEvent 广播到整个进程组。 */
bool __xrtSignalPlatformRaise(uint32 iSlot, int* pSystemCode)
{
	int iResult;
	uint32 iMode;

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( iSlot == 0u ) {
		iMode = xrtAtomic32Load(
			&__xrtSignalState.Slots[0].Mode,
			XMEMORY_ACQUIRE
		);
		if ( iMode == XRT_SIGNAL_SLOT_WATCH ) {
			__xrtSignalNotify(0u, SIGINT);
			return true;
		}
		if ( iMode == XRT_SIGNAL_SLOT_IGNORE ) {
			return true;
		}
	}
	if ( iSlot > 1u ) {
		return false;
	}
	iResult = raise(__xrtSignalState.Slots[iSlot].SystemCode);
	if ( (iResult != 0) && (pSystemCode != NULL) ) {
		*pSystemCode = errno != 0 ? errno : iResult;
	}
	return iResult == 0;
}

#endif
