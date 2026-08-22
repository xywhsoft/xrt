#if !defined(_WIN32) && !defined(_WIN64) && !defined(_POSIX_C_SOURCE)
	#define _POSIX_C_SOURCE 200809L
#endif

#include "../internal/xrt_signal.h"

#if defined(XRT_FEATURE_SIGNAL) && !defined(_WIN32) && !defined(_WIN64)

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>



static int __xrtSignalPipeRead = -1;
static volatile sig_atomic_t __xrtSignalPipeWrite = -1;
static struct sigaction __xrtSignalOldAction[3];
static bool __xrtSignalActionSaved[3];



/* 把 POSIX 原生代码映射到稳定内部槽。 */
static int __xrtSignalPosixSlot(int iSystemCode)
{
	if ( iSystemCode == SIGINT ) {
		return 0;
	}
	if ( iSystemCode == SIGTERM ) {
		return 1;
	}
	#if defined(SIGHUP)
		if ( iSystemCode == SIGHUP ) {
			return 2;
		}
	#endif
	return -1;
}



/* POSIX 处理器只保存 errno、记录原子计数并写自管道。 */
static void __xrtSignalPosixHandler(int iSystemCode)
{
	int iSavedError = errno;
	int iSlot = __xrtSignalPosixSlot(iSystemCode);

	if ( iSlot >= 0 ) {
		__xrtSignalNotify((uint32)iSlot, (int32)iSystemCode);
	}
	errno = iSavedError;
}



/* 为自管道描述符增加关闭继承标记。 */
static bool __xrtSignalCloseOnExec(int iDescriptor)
{
	int iFlags = fcntl(iDescriptor, F_GETFD, 0);

	return (iFlags >= 0) &&
		(fcntl(iDescriptor, F_SETFD, iFlags | FD_CLOEXEC) == 0);
}



/* 创建非阻塞写、阻塞读的自管道。 */
bool __xrtSignalPlatformInit(void)
{
	int arrPipe[2];
	int iFlags;
	int iSystemCode;

	if ( (__xrtSignalPipeRead >= 0) && (__xrtSignalPipeWrite >= 0) ) {
		return true;
	}
	if ( pipe(arrPipe) != 0 ) {
		__xrtErrorSetSystem(
			"xrt.signal",
			XSIGNAL_ERROR_SYSTEM,
			"pipe",
			errno,
			"signal wake pipe creation failed"
		);
		return false;
	}
	iFlags = fcntl(arrPipe[1], F_GETFL, 0);
	if ( (iFlags < 0) ||
		(fcntl(arrPipe[1], F_SETFL, iFlags | O_NONBLOCK) != 0) ||
		!__xrtSignalCloseOnExec(arrPipe[0]) ||
		!__xrtSignalCloseOnExec(arrPipe[1]) ) {
		iSystemCode = errno;
		(void)close(arrPipe[0]);
		(void)close(arrPipe[1]);
		__xrtErrorSetSystem(
			"xrt.signal",
			XSIGNAL_ERROR_SYSTEM,
			"pipe.configure",
			iSystemCode,
			"signal wake pipe configuration failed"
		);
		return false;
	}
	__xrtSignalPipeRead = arrPipe[0];
	__xrtSignalPipeWrite = arrPipe[1];
	return true;
}



/* 关闭已经停止使用的自管道。 */
void __xrtSignalPlatformUnit(void)
{
	int iRead = __xrtSignalPipeRead;
	int iWrite = __xrtSignalPipeWrite;

	__xrtSignalPipeRead = -1;
	__xrtSignalPipeWrite = -1;
	if ( iRead >= 0 ) {
		(void)close(iRead);
	}
	if ( iWrite >= 0 ) {
		(void)close(iWrite);
	}
}



/* 阻塞读取一个或多个唤醒字节。 */
bool __xrtSignalPlatformWait(int* pSystemCode)
{
	unsigned char arrBuffer[64];

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	for ( ;; ) {
		ssize_t iRead = read(
			__xrtSignalPipeRead,
			arrBuffer,
			sizeof(arrBuffer)
		);

		if ( iRead > 0 ) {
			return true;
		}
		if ( (iRead < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( pSystemCode != NULL ) {
			*pSystemCode = iRead == 0 ? EPIPE : errno;
		}
		return false;
	}
}



/* 非阻塞写入一个唤醒字节；待处理原子计数保证管道满时事件不会丢失。 */
void __xrtSignalPlatformWake(void)
{
	unsigned char iWake = 1u;
	int iWrite = (int)__xrtSignalPipeWrite;
	ssize_t iResult;

	if ( iWrite >= 0 ) {
		iResult = write(iWrite, &iWake, 1u);
		(void)iResult;
	}
}



/* 构造会屏蔽全部受管信号的 POSIX 动作。 */
static void __xrtSignalPosixAction(
	struct sigaction* pAction,
	xsignalslotmode Mode
)
{
	memset(pAction, 0, sizeof(*pAction));
	pAction->sa_handler = Mode == XRT_SIGNAL_SLOT_IGNORE ?
		SIG_IGN : __xrtSignalPosixHandler;
	(void)sigemptyset(&pAction->sa_mask);
	(void)sigaddset(&pAction->sa_mask, SIGINT);
	(void)sigaddset(&pAction->sa_mask, SIGTERM);
	#if defined(SIGHUP)
		(void)sigaddset(&pAction->sa_mask, SIGHUP);
	#endif
	#ifdef SA_RESTART
		pAction->sa_flags = SA_RESTART;
	#endif
}



/* 安装监听或忽略动作，并且只在首次接管时保存旧动作。 */
bool __xrtSignalPlatformMode(
	uint32 iSlot,
	xsignalslotmode Mode,
	int* pSystemCode
)
{
	struct sigaction Action;
	struct sigaction* pOld;
	int iNative;

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( (iSlot >= 3u) ||
		(Mode == XRT_SIGNAL_SLOT_DEFAULT) ) {
		return false;
	}
	iNative = __xrtSignalState.Slots[iSlot].SystemCode;
	__xrtSignalPosixAction(&Action, Mode);
	pOld = __xrtSignalActionSaved[iSlot] ?
		NULL : &__xrtSignalOldAction[iSlot];
	if ( sigaction(iNative, &Action, pOld) != 0 ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = errno;
		}
		return false;
	}
	__xrtSignalActionSaved[iSlot] = true;
	return true;
}



/* 恢复首次接管前保存的 POSIX 动作。 */
bool __xrtSignalPlatformRestore(uint32 iSlot, int* pSystemCode)
{
	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( iSlot >= 3u ) {
		return true;
	}
	if ( !__xrtSignalActionSaved[iSlot] ) {
		return true;
	}
	if ( sigaction(
		__xrtSignalState.Slots[iSlot].SystemCode,
		&__xrtSignalOldAction[iSlot],
		NULL
	) != 0 ) {
		if ( pSystemCode != NULL ) {
			*pSystemCode = errno;
		}
		return false;
	}
	memset(&__xrtSignalOldAction[iSlot], 0, sizeof(struct sigaction));
	__xrtSignalActionSaved[iSlot] = false;
	return true;
}



/* 使用标准 raise 向当前进程发送 POSIX 信号。 */
bool __xrtSignalPlatformRaise(uint32 iSlot, int* pSystemCode)
{
	int iResult;

	if ( pSystemCode != NULL ) {
		*pSystemCode = 0;
	}
	if ( iSlot >= 3u ) {
		return false;
	}
	iResult = raise(__xrtSignalState.Slots[iSlot].SystemCode);
	if ( (iResult != 0) && (pSystemCode != NULL) ) {
		*pSystemCode = errno != 0 ? errno : iResult;
	}
	return iResult == 0;
}

#endif
