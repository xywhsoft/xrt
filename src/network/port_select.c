#include "../internal/xrt_net_port.h"

#include <errno.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/select.h>
#endif



#if defined(XRT_FEATURE_NET_PORT_SELECT)

#if defined(_WIN32) || defined(_WIN64)
	typedef SOCKET __xrt_net_select_native;
#else
	typedef int __xrt_net_select_native;
#endif

/* 一个观察节点只借用 Socket，并保存下一次 one-shot 事件身份。 */
typedef struct __xrt_net_select_watch {
	struct __xrt_net_select_watch* Next;
	xnetsocket Socket;
	intptr_t Native;
	uint64 Id;
	uint32 Events;
	ptr User;
} __xrt_net_select_watch;



/* select 后端使用一对连接式 UDP Socket 实现全平台唤醒。 */
typedef struct __xrt_net_select_context {
	xnetsocket WakeRead;
	xnetsocket WakeWrite;
	__xrt_net_select_watch* Watches;
	size_t WatchCount;
} __xrt_net_select_context;



/* 返回当前平台的 select 系统错误。 */
static int __xrtNetSelectLastError(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return WSAGetLastError();
	#else
		return errno;
	#endif
}



/* 把内部 Socket 失败提升为调用者可理解的端口操作错误。 */
static void __xrtNetSelectWrapError(const xerror* pErrorBefore,
	xerrkind Fallback, xneterror Code, cstr sOperation, cstr sMessage)
{
	const xerror* pError = xrtGetError();
	xerrkind Kind = Fallback;
	int iSystemCode = 0;

	if ( (pError != NULL) && (pError != pErrorBefore) ) {
		Kind = xrtErrorKind(pError);
		iSystemCode = xrtErrorSystemCode(pError);
	}
	__xrtNetSetError(Kind, Code, sOperation, sMessage, iSystemCode);
}



/* 在观察链中查找指定 Socket。 */
static __xrt_net_select_watch** __xrtNetSelectFind(
	__xrt_net_select_context* pContext, xnetsocket Socket)
{
	__xrt_net_select_watch** ppWatch = &pContext->Watches;

	while ( *ppWatch != NULL ) {
		if ( (*ppWatch)->Socket == Socket ) {
			break;
		}
		ppWatch = &(*ppWatch)->Next;
	}
	return ppWatch;
}



/* 关闭内部唤醒 Socket，两个关闭都必须得到执行。 */
static bool __xrtNetSelectCloseWake(__xrt_net_select_context* pContext)
{
	bool bResult = true;

	if ( pContext->WakeWrite != NULL ) {
		if ( !xrtNetSocketClose(pContext->WakeWrite) ) {
			bResult = false;
		}
		pContext->WakeWrite = NULL;
	}
	if ( pContext->WakeRead != NULL ) {
		if ( !xrtNetSocketClose(pContext->WakeRead) ) {
			bResult = false;
		}
		pContext->WakeRead = NULL;
	}
	return bResult;
}



/* 创建非阻塞 UDP 唤醒通道，避免 Windows select 依赖额外事件 API。 */
static bool __xrtNetSelectOpenWake(__xrt_net_select_context* pContext)
{
	const xerror* pErrorBefore = xrtGetError();
	xnetaddr Address;

	pContext->WakeRead = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	pContext->WakeWrite = xrtNetSocketOpen(XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM, XNET_SOCKET_NONBLOCK);
	if ( (pContext->WakeRead == NULL) || (pContext->WakeWrite == NULL) ||
		 !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetSocketBind(pContext->WakeRead, &Address) ||
		 !xrtNetSocketLocal(pContext->WakeRead, &Address) ||
		 (xrtNetSocketConnect(pContext->WakeWrite,
			&Address) != XNET_RESULT_OK) ) {
		(void)__xrtNetSelectCloseWake(pContext);
		__xrtNetSelectWrapError(pErrorBefore, XERR_IO,
			XNET_ERROR_PORT_CREATE, "create-port",
			"creating select wake channel failed");
		return false;
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		if ( xrtNetSocketNative(pContext->WakeRead) >= FD_SETSIZE ) {
			(void)__xrtNetSelectCloseWake(pContext);
			__xrtNetSetError(
				XERR_RANGE,
				XNET_ERROR_PORT_CREATE,
				"create-port",
				"select wake descriptor exceeds FD_SETSIZE",
				0
			);
			return false;
		}
	#endif
	return true;
}



/* 初始化 select 后端上下文。 */
static bool __xrtNetSelectInit(xnetport* pPort)
{
	__xrt_net_select_context* pContext;

	pContext = (__xrt_net_select_context*)xrtMalloc(sizeof(*pContext));
	if ( pContext == NULL ) {
		return false;
	}
	memset(pContext, 0, sizeof(*pContext));
	if ( !__xrtNetSelectOpenWake(pContext) ) {
		xrtFree(pContext);
		return false;
	}
	pPort->Context = pContext;
	return true;
}



/* 释放观察节点和唤醒通道。 */
static bool __xrtNetSelectUnit(xnetport* pPort)
{
	const xerror* pErrorBefore = xrtGetError();
	__xrt_net_select_context* pContext =
		(__xrt_net_select_context*)pPort->Context;
	bool bResult;

	if ( pContext == NULL ) {
		return true;
	}
	while ( pContext->Watches != NULL ) {
		__xrt_net_select_watch* pNext = pContext->Watches->Next;

		xrtFree(pContext->Watches);
		pContext->Watches = pNext;
	}
	bResult = __xrtNetSelectCloseWake(pContext);
	if ( !bResult ) {
		__xrtNetSelectWrapError(pErrorBefore, XERR_IO,
			XNET_ERROR_PORT_CLOSE, "destroy-port",
			"closing select wake channel failed");
	}
	xrtFree(pContext);
	pPort->Context = NULL;
	return bResult;
}



/* 验证原生句柄可以进入当前平台 fd_set。 */
static bool __xrtNetSelectNativeAllowed(
	__xrt_net_select_context* pContext, intptr_t iNative, bool bNew)
{
	#if defined(_WIN32) || defined(_WIN64)
		if ( bNew && ((pContext->WatchCount + 1) >= FD_SETSIZE) ) {
			return false;
		}
		return iNative != (intptr_t)INVALID_SOCKET;
	#else
		(void)pContext;
		(void)bNew;
		return (iNative >= 0) && (iNative < FD_SETSIZE);
	#endif
}



/* 新增或替换一个 one-shot readiness 观察。 */
static bool __xrtNetSelectWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser)
{
	__xrt_net_select_context* pContext =
		(__xrt_net_select_context*)pPort->Context;
	__xrt_net_select_watch** ppWatch;
	__xrt_net_select_watch* pWatch;
	intptr_t iNative;
	bool bNew;

	ppWatch = __xrtNetSelectFind(pContext, Socket);
	bNew = *ppWatch == NULL;
	iNative = xrtNetSocketNative(Socket);
	if ( !__xrtNetSelectNativeAllowed(pContext, iNative, bNew) ||
		 (bNew && (pContext->WatchCount >= pPort->Config.WatchLimit)) ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_PORT_WATCH,
			"watch", "socket exceeds select watch capacity", 0);
		return false;
	}
	if ( bNew ) {
		pWatch = (__xrt_net_select_watch*)xrtMalloc(sizeof(*pWatch));
		if ( pWatch == NULL ) {
			return false;
		}
		memset(pWatch, 0, sizeof(*pWatch));
		pWatch->Next = pContext->Watches;
		pContext->Watches = pWatch;
		pContext->WatchCount++;
	} else {
		pWatch = *ppWatch;
	}
	pWatch->Socket = Socket;
	pWatch->Native = iNative;
	pWatch->Id = Id;
	pWatch->Events = iEvents;
	pWatch->User = pUser;
	return true;
}



/* 幂等移除一个 readiness 观察。 */
static bool __xrtNetSelectUnwatch(xnetport* pPort, xnetsocket Socket)
{
	__xrt_net_select_context* pContext =
		(__xrt_net_select_context*)pPort->Context;
	__xrt_net_select_watch** ppWatch =
		__xrtNetSelectFind(pContext, Socket);

	if ( *ppWatch != NULL ) {
		__xrt_net_select_watch* pWatch = *ppWatch;

		*ppWatch = pWatch->Next;
		xrtFree(pWatch);
		pContext->WatchCount--;
	}
	return true;
}



/* 把微秒等待转换为 select timeval，NEVER 由空指针表达。 */
static struct timeval* __xrtNetSelectTimeout(uint64 iTimeout,
	struct timeval* pTimeout)
{
	uint64 iSeconds;

	if ( iTimeout == UINT64_MAX ) {
		return NULL;
	}
	iSeconds = iTimeout / 1000000u;
	if ( iSeconds > (uint64)LONG_MAX ) {
		iSeconds = (uint64)LONG_MAX;
		iTimeout = iSeconds * 1000000u;
	}
	pTimeout->tv_sec = (long)iSeconds;
	pTimeout->tv_usec = (long)(iTimeout % 1000000u);
	return pTimeout;
}



/* 清空内部 UDP 唤醒数据，不把平台通知本身暴露成重复事件。 */
static bool __xrtNetSelectDrainWake(__xrt_net_select_context* pContext)
{
	unsigned char Data[64];

	for ( ;; ) {
		const xerror* pErrorBefore = xrtGetError();
		size_t iReceived = 0;
		xnetresult Result = xrtNetSocketRecvFrom(pContext->WakeRead,
			Data, sizeof(Data), &iReceived, NULL);

		if ( Result == XNET_RESULT_AGAIN ) {
			return true;
		}
		if ( (Result != XNET_RESULT_OK) &&
			 (Result != XNET_RESULT_TRUNCATED) ) {
			__xrtNetSelectWrapError(pErrorBefore, XERR_IO,
				XNET_ERROR_PORT_WAIT, "wait",
				"draining select wake channel failed");
			return false;
		}
	}
}



/* 等待 readiness；每个已报告方向自动清除，调用方显式重新观察。 */
static xnetresult __xrtNetSelectWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	uint64 iTimeout, size_t* pCount)
{
	__xrt_net_select_context* pContext =
		(__xrt_net_select_context*)pPort->Context;
	__xrt_net_select_watch* pWatch;
	fd_set ReadSet;
	fd_set WriteSet;
	fd_set ErrorSet;
	struct timeval Timeout;
	intptr_t iWake;
	int iResult;
	size_t iCount = 0;

	#if !defined(_WIN32) && !defined(_WIN64)
		int iMaximum = 0;
	#endif

	*pCount = 0;
	FD_ZERO(&ReadSet);
	FD_ZERO(&WriteSet);
	FD_ZERO(&ErrorSet);
	iWake = xrtNetSocketNative(pContext->WakeRead);
	FD_SET((__xrt_net_select_native)iWake, &ReadSet);
	#if !defined(_WIN32) && !defined(_WIN64)
		iMaximum = (int)iWake;
	#endif

	for ( pWatch = pContext->Watches;
		pWatch != NULL; pWatch = pWatch->Next ) {
		if ( (pWatch->Events & XNET_POLL_READ) != 0 ) {
			FD_SET((__xrt_net_select_native)pWatch->Native, &ReadSet);
		}
		if ( (pWatch->Events & XNET_POLL_WRITE) != 0 ) {
			FD_SET((__xrt_net_select_native)pWatch->Native, &WriteSet);
		}
		FD_SET((__xrt_net_select_native)pWatch->Native, &ErrorSet);
		#if !defined(_WIN32) && !defined(_WIN64)
			if ( pWatch->Native > iMaximum ) {
				iMaximum = (int)pWatch->Native;
			}
		#endif
	}

	#if defined(_WIN32) || defined(_WIN64)
		iResult = select(0, &ReadSet, &WriteSet, &ErrorSet,
			__xrtNetSelectTimeout(iTimeout, &Timeout));
	#else
		iResult = select(iMaximum + 1, &ReadSet, &WriteSet, &ErrorSet,
			__xrtNetSelectTimeout(iTimeout, &Timeout));
	#endif
	if ( iResult == 0 ) {
		return iTimeout == 0 ?
			XNET_RESULT_OK : XNET_RESULT_TIMEOUT;
	}
	if ( iResult < 0 ) {
		int iCode = __xrtNetSelectLastError();

		#if !defined(_WIN32) && !defined(_WIN64)
			if ( iCode == EINTR ) {
				return XNET_RESULT_OK;
			}
		#endif
		__xrtNetSetError(XERR_IO, XNET_ERROR_PORT_WAIT,
			"wait", "select network port wait failed", iCode);
		return XNET_RESULT_ERROR;
	}
	if ( FD_ISSET((__xrt_net_select_native)iWake, &ReadSet) ) {
		if ( !__xrtNetSelectDrainWake(pContext) ) {
			return XNET_RESULT_ERROR;
		}
	}

	{
		__xrt_net_select_watch** ppWatch = &pContext->Watches;

		while ( (*ppWatch != NULL) && (iCount < iCapacity) ) {
			__xrt_net_select_watch* pCurrent = *ppWatch;
			uint32 iFlags = 0;

			if ( ((pCurrent->Events & XNET_POLL_READ) != 0) &&
				 FD_ISSET((__xrt_net_select_native)pCurrent->Native, &ReadSet) ) {
				iFlags |= XNET_PORT_EVENT_READ;
			}
			if ( ((pCurrent->Events & XNET_POLL_WRITE) != 0) &&
				 FD_ISSET((__xrt_net_select_native)pCurrent->Native, &WriteSet) ) {
				iFlags |= XNET_PORT_EVENT_WRITE;
			}
			if ( FD_ISSET((__xrt_net_select_native)pCurrent->Native, &ErrorSet) ) {
				iFlags |= XNET_PORT_EVENT_ERROR;
			}
			if ( iFlags == 0 ) {
				ppWatch = &pCurrent->Next;
				continue;
			}

			memset(&pEvents[iCount], 0, sizeof(pEvents[iCount]));
			pEvents[iCount].Type = XNET_PORT_EVENT_READY;
			pEvents[iCount].Flags = iFlags;
			pEvents[iCount].Result = XNET_RESULT_OK;
			pEvents[iCount].Id = pCurrent->Id;
			pEvents[iCount].Socket = pCurrent->Socket;
			pEvents[iCount].User = pCurrent->User;
			iCount++;

			if ( (iFlags & XNET_PORT_EVENT_ERROR) != 0 ) {
				pCurrent->Events = 0;
			} else {
				if ( (iFlags & XNET_PORT_EVENT_READ) != 0 ) {
					pCurrent->Events &= ~((uint32)XNET_POLL_READ);
				}
				if ( (iFlags & XNET_PORT_EVENT_WRITE) != 0 ) {
					pCurrent->Events &= ~((uint32)XNET_POLL_WRITE);
				}
			}
			if ( pCurrent->Events == 0 ) {
				*ppWatch = pCurrent->Next;
				xrtFree(pCurrent);
				pContext->WatchCount--;
			} else {
				ppWatch = &pCurrent->Next;
			}
		}
	}

	*pCount = iCount;
	return XNET_RESULT_OK;
}



/* 写入一个唤醒报文；发送队列已满表示已有通知在途，也视为成功。 */
static bool __xrtNetSelectWake(xnetport* pPort)
{
	const xerror* pErrorBefore = xrtGetError();
	__xrt_net_select_context* pContext =
		(__xrt_net_select_context*)pPort->Context;
	unsigned char iByte = 1;
	size_t iSent = 0;
	xnetresult Result;

	Result = xrtNetSocketSend(pContext->WakeWrite,
		&iByte, 1, &iSent);
	if ( Result == XNET_RESULT_AGAIN ) {
		return true;
	}
	if ( (Result != XNET_RESULT_OK) || (iSent != 1) ) {
		__xrtNetSelectWrapError(pErrorBefore, XERR_IO,
			XNET_ERROR_PORT_POST, "wake",
			"waking select network port failed");
		return false;
	}
	return true;
}



/* select 是全平台 Tier C fallback，只提供诚实的 readiness 语义。 */
static const __xrt_net_port_driver __xrtNetSelectDriver = {
	"select",
	XNET_PORT_SELECT,
	XNET_PORT_CAP_READINESS |
		XNET_PORT_CAP_ONESHOT |
		XNET_PORT_CAP_BATCH |
		XNET_PORT_CAP_WAKE |
		XNET_PORT_CAP_POST,
	__xrtNetSelectInit,
	__xrtNetSelectUnit,
	__xrtNetSelectWatch,
	__xrtNetSelectUnwatch,
	NULL,
	NULL,
	__xrtNetSelectWait,
	__xrtNetSelectWake
};



/* 返回 select 后端驱动。 */
const __xrt_net_port_driver* __xrtNetPortSelectDriver(void)
{
	return &__xrtNetSelectDriver;
}

#endif
