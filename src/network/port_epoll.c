#include "../internal/xrt_net_port.h"

#if defined(XRT_FEATURE_NET_PORT_EPOLL) && defined(__linux__)
	#include <errno.h>
	#include <fcntl.h>
	#include <limits.h>
	#include <sys/epoll.h>
	#include <sys/eventfd.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_NET_PORT_EPOLL) && defined(__linux__)

#define XRT_NET_EPOLL_READY_MAX 256u
#define XRT_NET_EPOLL_WAKE_TOKEN UINT64_C(0)



typedef struct __xrt_net_epoll_watch __xrt_net_epoll_watch;



/* 一个节点对应一个原生描述符，并保存当前 one-shot 身份和剩余关注位。 */
struct __xrt_net_epoll_watch {
	__xrt_net_epoll_watch* Next;
	xnetsocket Socket;
	int Native;
	uint64 Id;
	uint64 Token;
	uint32 Events;
	ptr User;
};



/* epoll 上下文只保存内核端口、eventfd 和 owner 线程独占的观察表。 */
typedef struct __xrt_net_epoll_context {
	int Port;
	int Wake;
	__xrt_net_epoll_watch** Buckets;
	struct epoll_event* Ready;
	size_t BucketCount;
	size_t BucketLimit;
	size_t WatchCount;
	int ReadyCapacity;
	uint32 Generation;
} __xrt_net_epoll_context;



/* 查找指定原生描述符的观察节点链接位置。 */
static __xrt_net_epoll_watch** __xrtNetEpollFind(
	__xrt_net_epoll_context* pContext,
	int iNative
)
{
	__xrt_net_epoll_watch** ppWatch =
		&pContext->Buckets[__xrtNetPortHash(
			(uintptr_t)iNative,
			pContext->BucketCount
		)];

	while ( *ppWatch != NULL ) {
		if ( (*ppWatch)->Native == iNative ) {
			break;
		}
		ppWatch = &(*ppWatch)->Next;
	}
	return ppWatch;
}



/* 观察数量增长时扩展索引，较大的硬上限不再产生同规模空闲表。 */
static bool __xrtNetEpollIndexGrow(__xrt_net_epoll_context* pContext)
{
	size_t iNext = __xrtNetPortBucketNext(
		pContext->WatchCount + 1u,
		pContext->BucketCount,
		pContext->BucketLimit
	);
	__xrt_net_epoll_watch** pBuckets;

	if ( iNext == pContext->BucketCount ) {
		return true;
	}
	pBuckets = (__xrt_net_epoll_watch**)xrtCalloc(
		iNext,
		sizeof(*pBuckets)
	);
	if ( pBuckets == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < pContext->BucketCount; i++ ) {
		__xrt_net_epoll_watch* pWatch = pContext->Buckets[i];

		while ( pWatch != NULL ) {
			__xrt_net_epoll_watch* pNext = pWatch->Next;
			size_t iBucket = __xrtNetPortHash(
				(uintptr_t)pWatch->Native,
				iNext
			);

			pWatch->Next = pBuckets[iBucket];
			pBuckets[iBucket] = pWatch;
			pWatch = pNext;
		}
	}
	xrtFree(pContext->Buckets);
	pContext->Buckets = pBuckets;
	pContext->BucketCount = iNext;
	return true;
}



/* 为每次观察生成包含描述符和代际的内核身份，拒绝迟到事件错配。 */
static uint64 __xrtNetEpollToken(
	__xrt_net_epoll_context* pContext,
	int iNative
)
{
	pContext->Generation++;
	if ( pContext->Generation == 0 ) {
		pContext->Generation++;
	}
	return ((uint64)pContext->Generation << 32) | (uint32)iNative;
}



/* 从内部事件身份恢复经过范围检查的非负原生描述符。 */
static int __xrtNetEpollTokenNative(uint64 iToken)
{
	return (int)(uint32)iToken;
}



/* 把公开关注位转换为 level-triggered 内核 one-shot 掩码。 */
static uint32 __xrtNetEpollEvents(uint32 iEvents)
{
	uint32 iNative = EPOLLONESHOT;

	if ( (iEvents & XNET_POLL_READ) != 0 ) {
		iNative |= EPOLLIN;
		#if defined(EPOLLRDHUP)
			iNative |= EPOLLRDHUP;
		#endif
	}
	if ( (iEvents & XNET_POLL_WRITE) != 0 ) {
		iNative |= EPOLLOUT;
	}
	return iNative;
}



/* 设置保留 errno 的 epoll 结构化错误。 */
static void __xrtNetEpollError(
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	int iSystemCode
)
{
	__xrtNetSocketSetSystemError(
		Code,
		sOperation,
		sMessage,
		iSystemCode
	);
}



/* 释放初始化过程中已经建立的资源，不覆盖原始失败。 */
static void __xrtNetEpollDiscard(__xrt_net_epoll_context* pContext)
{
	if ( pContext == NULL ) {
		return;
	}
	if ( pContext->Wake >= 0 ) {
		(void)close(pContext->Wake);
	}
	if ( pContext->Port >= 0 ) {
		(void)close(pContext->Port);
	}
	xrtFree(pContext->Ready);
	xrtFree(pContext->Buckets);
	xrtFree(pContext);
}



/* 为低版本内核创建的描述符补上 close-on-exec，失败时保留 errno。 */
static bool __xrtNetEpollCloseOnExec(int iNative)
{
	int iFlags;
	int iResult;

	do {
		iFlags = fcntl(iNative, F_GETFD);
	} while ( (iFlags < 0) && (errno == EINTR) );
	if ( iFlags < 0 ) {
		return false;
	}
	do {
		iResult = fcntl(iNative, F_SETFD, iFlags | FD_CLOEXEC);
	} while ( (iResult < 0) && (errno == EINTR) );
	return iResult == 0;
}



/* 为低版本 eventfd 补上非阻塞属性，避免唤醒通道阻塞 worker。 */
static bool __xrtNetEpollNonblock(int iNative)
{
	int iFlags;
	int iResult;

	do {
		iFlags = fcntl(iNative, F_GETFL);
	} while ( (iFlags < 0) && (errno == EINTR) );
	if ( iFlags < 0 ) {
		return false;
	}
	do {
		iResult = fcntl(iNative, F_SETFL, iFlags | O_NONBLOCK);
	} while ( (iResult < 0) && (errno == EINTR) );
	return iResult == 0;
}



/* 执行不会阻塞的 epoll 控制调用，并重试信号中断。 */
static int __xrtNetEpollControl(
	__xrt_net_epoll_context* pContext,
	int iOperation,
	int iNative,
	struct epoll_event* pEvent
)
{
	int iResult;

	do {
		iResult = epoll_ctl(
			pContext->Port,
			iOperation,
			iNative,
			pEvent
		);
	} while ( (iResult != 0) && (errno == EINTR) );
	return iResult;
}



/* 优先使用原子 close-on-exec 创建，旧内核则回退到 epoll_create。 */
static int __xrtNetEpollCreatePort(void)
{
	int iNative;

	#if defined(EPOLL_CLOEXEC)
		iNative = epoll_create1(EPOLL_CLOEXEC);
		if ( iNative >= 0 ) {
			return iNative;
		}
		if ( (errno != ENOSYS) && (errno != EINVAL) ) {
			return -1;
		}
	#endif

	iNative = epoll_create(1);
	if ( iNative < 0 ) {
		return -1;
	}
	if ( !__xrtNetEpollCloseOnExec(iNative) ) {
		int iSystemCode = errno;

		(void)close(iNative);
		errno = iSystemCode;
		return -1;
	}
	return iNative;
}



/* 优先原子创建唤醒描述符，旧内核则在发布前补齐两个必要标志。 */
static int __xrtNetEpollCreateWake(void)
{
	int iNative;

	#if defined(EFD_CLOEXEC) && defined(EFD_NONBLOCK)
		iNative = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
		if ( iNative >= 0 ) {
			return iNative;
		}
		if ( errno != EINVAL ) {
			return -1;
		}
	#endif

	iNative = eventfd(0, 0);
	if ( iNative < 0 ) {
		return -1;
	}
	if ( !__xrtNetEpollCloseOnExec(iNative) ||
		 !__xrtNetEpollNonblock(iNative) ) {
		int iSystemCode = errno;

		(void)close(iNative);
		errno = iSystemCode;
		return -1;
	}
	return iNative;
}



/* 初始化 Linux epoll 和无阻塞 eventfd 唤醒通道。 */
static bool __xrtNetEpollInit(xnetport* pPort)
{
	__xrt_net_epoll_context* pContext;
	struct epoll_event Event;
	size_t iReadyCapacity = pPort->Config.WatchLimit;
	int iSystemCode;

	pContext = (__xrt_net_epoll_context*)xrtCalloc(1, sizeof(*pContext));
	if ( pContext == NULL ) {
		return false;
	}
	pContext->Port = -1;
	pContext->Wake = -1;
	pContext->BucketCount = XRT_NET_PORT_BUCKET_MIN;
	pContext->BucketLimit = __xrtNetPortBucketCount(
		pPort->Config.WatchLimit
	);
	if ( iReadyCapacity > XRT_NET_EPOLL_READY_MAX ) {
		iReadyCapacity = XRT_NET_EPOLL_READY_MAX;
	}
	pContext->ReadyCapacity = (int)iReadyCapacity;
	pContext->Buckets = (__xrt_net_epoll_watch**)xrtCalloc(
		pContext->BucketCount,
		sizeof(*pContext->Buckets)
	);
	pContext->Ready = (struct epoll_event*)xrtCalloc(
		iReadyCapacity,
		sizeof(*pContext->Ready)
	);
	if ( (pContext->Buckets == NULL) || (pContext->Ready == NULL) ) {
		__xrtNetEpollDiscard(pContext);
		return false;
	}

	pContext->Port = __xrtNetEpollCreatePort();
	if ( pContext->Port < 0 ) {
		iSystemCode = errno;
		__xrtNetEpollDiscard(pContext);
		__xrtNetEpollError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating epoll instance failed",
			iSystemCode
		);
		return false;
	}
	pContext->Wake = __xrtNetEpollCreateWake();
	if ( pContext->Wake < 0 ) {
		iSystemCode = errno;
		__xrtNetEpollDiscard(pContext);
		__xrtNetEpollError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating epoll wake event failed",
			iSystemCode
		);
		return false;
	}

	memset(&Event, 0, sizeof(Event));
	Event.events = EPOLLIN;
	Event.data.u64 = XRT_NET_EPOLL_WAKE_TOKEN;
	if ( __xrtNetEpollControl(
		pContext,
		EPOLL_CTL_ADD,
		pContext->Wake,
		&Event
	) != 0 ) {
		iSystemCode = errno;
		__xrtNetEpollDiscard(pContext);
		__xrtNetEpollError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"registering epoll wake event failed",
			iSystemCode
		);
		return false;
	}
	pPort->Context = pContext;
	return true;
}



/* 释放观察表以及两个内核描述符，关闭失败仍完成全部清理。 */
static bool __xrtNetEpollUnit(xnetport* pPort)
{
	__xrt_net_epoll_context* pContext =
		(__xrt_net_epoll_context*)pPort->Context;
	int iSystemCode = 0;

	if ( pContext == NULL ) {
		return true;
	}
	for ( size_t i = 0; i < pContext->BucketCount; i++ ) {
		__xrt_net_epoll_watch* pWatch = pContext->Buckets[i];

		while ( pWatch != NULL ) {
			__xrt_net_epoll_watch* pNext = pWatch->Next;

			xrtFree(pWatch);
			pWatch = pNext;
		}
	}
	if ( (pContext->Wake >= 0) && (close(pContext->Wake) != 0) ) {
		iSystemCode = errno;
	}
	if ( (pContext->Port >= 0) && (close(pContext->Port) != 0) &&
		 (iSystemCode == 0) ) {
		iSystemCode = errno;
	}
	xrtFree(pContext->Ready);
	xrtFree(pContext->Buckets);
	xrtFree(pContext);
	pPort->Context = NULL;

	if ( iSystemCode != 0 ) {
		__xrtNetEpollError(
			XNET_ERROR_PORT_CLOSE,
			"destroy-port",
			"closing epoll network port failed",
			iSystemCode
		);
		return false;
	}
	return true;
}



/* 新增或原子替换一个 one-shot readiness 观察。 */
static bool __xrtNetEpollWatch(
	xnetport* pPort,
	xnetsocket Socket,
	uint64 Id,
	uint32 iEvents,
	ptr pUser
)
{
	__xrt_net_epoll_context* pContext =
		(__xrt_net_epoll_context*)pPort->Context;
	__xrt_net_epoll_watch** ppWatch;
	__xrt_net_epoll_watch* pWatch;
	struct epoll_event Event;
	intptr_t iNativeValue = xrtNetSocketNative(Socket);
	uint64 iToken;
	int iNative;
	int iOperation;

	if ( (iNativeValue < 0) || (iNativeValue > INT_MAX) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"watch",
			"socket cannot be represented by epoll",
			0
		);
		return false;
	}
	iNative = (int)iNativeValue;
	ppWatch = __xrtNetEpollFind(pContext, iNative);
	pWatch = *ppWatch;
	if ( (pWatch == NULL) &&
		 (pContext->WatchCount >= pPort->Config.WatchLimit) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"watch",
			"epoll watch limit reached",
			0
		);
		return false;
	}
	if ( pWatch == NULL ) {
		if ( !__xrtNetEpollIndexGrow(pContext) ) {
			return false;
		}
		ppWatch = __xrtNetEpollFind(pContext, iNative);
		pWatch = (__xrt_net_epoll_watch*)xrtMalloc(sizeof(*pWatch));
		if ( pWatch == NULL ) {
			return false;
		}
		memset(pWatch, 0, sizeof(*pWatch));
		pWatch->Native = iNative;
		iOperation = EPOLL_CTL_ADD;
	} else {
		iOperation = EPOLL_CTL_MOD;
	}

	iToken = __xrtNetEpollToken(pContext, iNative);
	memset(&Event, 0, sizeof(Event));
	Event.events = __xrtNetEpollEvents(iEvents);
	Event.data.u64 = iToken;
	if ( __xrtNetEpollControl(
		pContext,
		iOperation,
		iNative,
		&Event
	) != 0 ) {
		int iSystemCode = errno;

		if ( iOperation == EPOLL_CTL_ADD ) {
			xrtFree(pWatch);
		}
		__xrtNetEpollError(
			XNET_ERROR_PORT_WATCH,
			"watch",
			"registering epoll socket failed",
			iSystemCode
		);
		return false;
	}

	if ( iOperation == EPOLL_CTL_ADD ) {
		pWatch->Next = *ppWatch;
		*ppWatch = pWatch;
		pContext->WatchCount++;
	}
	pWatch->Socket = Socket;
	pWatch->Id = Id;
	pWatch->Token = iToken;
	pWatch->Events = iEvents;
	pWatch->User = pUser;
	return true;
}



/* 幂等移除一个 readiness 观察。 */
static bool __xrtNetEpollUnwatch(xnetport* pPort, xnetsocket Socket)
{
	__xrt_net_epoll_context* pContext =
		(__xrt_net_epoll_context*)pPort->Context;
	intptr_t iNativeValue = xrtNetSocketNative(Socket);
	__xrt_net_epoll_watch** ppWatch;
	__xrt_net_epoll_watch* pWatch;
	int iNative;

	if ( (iNativeValue < 0) || (iNativeValue > INT_MAX) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"unwatch",
			"socket cannot be represented by epoll",
			0
		);
		return false;
	}
	iNative = (int)iNativeValue;
	ppWatch = __xrtNetEpollFind(pContext, iNative);
	pWatch = *ppWatch;
	if ( pWatch == NULL ) {
		return true;
	}
	if ( (__xrtNetEpollControl(
		pContext,
		EPOLL_CTL_DEL,
		iNative,
		NULL
	) != 0) && (errno != ENOENT) ) {
		int iSystemCode = errno;

		/* 即使内核删除失败，也先退休用户身份以屏蔽迟到事件。 */
		*ppWatch = pWatch->Next;
		pContext->WatchCount--;
		xrtFree(pWatch);
		__xrtNetEpollError(
			XNET_ERROR_PORT_WATCH,
			"unwatch",
			"removing epoll socket failed",
			iSystemCode
		);
		return false;
	}
	*ppWatch = pWatch->Next;
	pContext->WatchCount--;
	xrtFree(pWatch);
	return true;
}



/* 把微秒等待向上转换成 epoll 的毫秒精度。 */
static int __xrtNetEpollTimeout(uint64 iTimeout)
{
	uint64 iMilliseconds;

	if ( iTimeout == UINT64_MAX ) {
		return -1;
	}
	if ( iTimeout == 0 ) {
		return 0;
	}
	iMilliseconds = iTimeout / 1000u;
	if ( (iTimeout % 1000u) != 0 ) {
		iMilliseconds++;
	}
	if ( iMilliseconds > (uint64)INT_MAX ) {
		return INT_MAX;
	}
	return (int)iMilliseconds;
}



/* 排空 eventfd；计数已经饱和也只代表通知在途，不丢失端口队列状态。 */
static bool __xrtNetEpollDrainWake(__xrt_net_epoll_context* pContext)
{
	uint64 iValue;

	for ( ;; ) {
		ssize_t iRead = read(
			pContext->Wake,
			&iValue,
			sizeof(iValue)
		);

		if ( iRead == (ssize_t)sizeof(iValue) ) {
			continue;
		}
		if ( (iRead < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( (iRead < 0) &&
			 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
			return true;
		}
		__xrtNetEpollError(
			XNET_ERROR_PORT_WAIT,
			"wait",
			"draining epoll wake event failed",
			iRead < 0 ? errno : EIO
		);
		return false;
	}
}



/* 从 epoll 掩码生成稳定事件标志，并确保挂断推进受关注的方向。 */
static uint32 __xrtNetEpollEventFlags(
	const __xrt_net_epoll_watch* pWatch,
	uint32 iNative
)
{
	uint32 iFlags = 0;

	if ( ((iNative & EPOLLIN) != 0) &&
		 ((pWatch->Events & XNET_POLL_READ) != 0) ) {
		iFlags |= XNET_PORT_EVENT_READ;
	}
	if ( ((iNative & EPOLLOUT) != 0) &&
		 ((pWatch->Events & XNET_POLL_WRITE) != 0) ) {
		iFlags |= XNET_PORT_EVENT_WRITE;
	}
	if ( (iNative & EPOLLERR) != 0 ) {
		iFlags |= XNET_PORT_EVENT_ERROR;
	}
	if ( (iNative & EPOLLHUP) != 0 ) {
		iFlags |= XNET_PORT_EVENT_HANGUP;
		if ( (pWatch->Events & XNET_POLL_READ) != 0 ) {
			iFlags |= XNET_PORT_EVENT_READ;
		}
		if ( (pWatch->Events & XNET_POLL_WRITE) != 0 ) {
			iFlags |= XNET_PORT_EVENT_WRITE;
		}
	}
	#if defined(EPOLLRDHUP)
		if ( (iNative & EPOLLRDHUP) != 0 ) {
			iFlags |= XNET_PORT_EVENT_HANGUP;
			if ( (pWatch->Events & XNET_POLL_READ) != 0 ) {
				iFlags |= XNET_PORT_EVENT_READ;
			}
		}
	#endif
	return iFlags;
}



/* 提取一个观察事件，并按方向清除或重新武装剩余 one-shot。 */
static void __xrtNetEpollPublish(
	__xrt_net_epoll_context* pContext,
	const struct epoll_event* pReady,
	xnetportevent* pEvent
)
{
	int iNative = __xrtNetEpollTokenNative(pReady->data.u64);
	__xrt_net_epoll_watch** ppWatch =
		__xrtNetEpollFind(pContext, iNative);
	__xrt_net_epoll_watch* pWatch = *ppWatch;
	uint32 iFlags;
	uint32 iRemaining;

	memset(pEvent, 0, sizeof(*pEvent));
	if ( (pWatch == NULL) || (pWatch->Token != pReady->data.u64) ) {
		return;
	}
	iFlags = __xrtNetEpollEventFlags(pWatch, pReady->events);
	iRemaining = pWatch->Events;
	if ( (iFlags & (XNET_PORT_EVENT_ERROR |
		 XNET_PORT_EVENT_HANGUP)) != 0 ) {
		iRemaining = 0;
	} else {
		if ( (iFlags & XNET_PORT_EVENT_READ) != 0 ) {
			iRemaining &= ~((uint32)XNET_POLL_READ);
		}
		if ( (iFlags & XNET_PORT_EVENT_WRITE) != 0 ) {
			iRemaining &= ~((uint32)XNET_POLL_WRITE);
		}
	}

	pEvent->Type = XNET_PORT_EVENT_READY;
	pEvent->Flags = iFlags;
	pEvent->Result = XNET_RESULT_OK;
	pEvent->Id = pWatch->Id;
	pEvent->Socket = pWatch->Socket;
	pEvent->User = pWatch->User;
	/* SO_ERROR 由 FinishConnect 或调用方唯一消费，后端不能提前清除。 */

	if ( iRemaining != 0 ) {
		struct epoll_event Event;

		memset(&Event, 0, sizeof(Event));
		Event.events = __xrtNetEpollEvents(iRemaining);
		Event.data.u64 = pWatch->Token;
		if ( __xrtNetEpollControl(
			pContext,
			EPOLL_CTL_MOD,
			pWatch->Native,
			&Event
		) == 0 ) {
			pWatch->Events = iRemaining;
			return;
		}

		pEvent->Flags |= XNET_PORT_EVENT_ERROR;
		pEvent->Result = XNET_RESULT_ERROR;
		pEvent->SystemCode = errno;
	}

	/*
		事件已经由 EPOLLONESHOT 禁用，即使 DEL 失败也不会再次引用节点。
		Socket 关闭还会让内核自动删除残留注册。
	*/
	(void)__xrtNetEpollControl(
		pContext,
		EPOLL_CTL_DEL,
		pWatch->Native,
		NULL
	);
	*ppWatch = pWatch->Next;
	pContext->WatchCount--;
	xrtFree(pWatch);
}



/* 等待并批量提取 readiness；eventfd 只唤醒公共端口队列，不单独制造事件。 */
static xnetresult __xrtNetEpollWait(
	xnetport* pPort,
	xnetportevent* pEvents,
	size_t iCapacity,
	uint64 iTimeout,
	size_t* pCount
)
{
	__xrt_net_epoll_context* pContext =
		(__xrt_net_epoll_context*)pPort->Context;
	int iMaximum = pContext->ReadyCapacity;
	size_t iCount = 0;

	*pCount = 0;
	if ( iCapacity < (size_t)iMaximum ) {
		iMaximum = (int)iCapacity;
	}
	for ( ;; ) {
		int iReady = epoll_wait(
			pContext->Port,
			pContext->Ready,
			iMaximum,
			__xrtNetEpollTimeout(iTimeout)
		);

		if ( iReady == 0 ) {
			return iTimeout == 0 ? XNET_RESULT_OK : XNET_RESULT_TIMEOUT;
		}
		if ( iReady < 0 ) {
			if ( errno == EINTR ) {
				return XNET_RESULT_OK;
			}
			__xrtNetEpollError(
				XNET_ERROR_PORT_WAIT,
				"wait",
				"epoll wait failed",
				errno
			);
			return XNET_RESULT_ERROR;
		}

		for ( int i = 0; i < iReady; i++ ) {
			if ( pContext->Ready[i].data.u64 ==
				 XRT_NET_EPOLL_WAKE_TOKEN ) {
				if ( !__xrtNetEpollDrainWake(pContext) ) {
					return XNET_RESULT_ERROR;
				}
				continue;
			}
			__xrtNetEpollPublish(
				pContext,
				&pContext->Ready[i],
				&pEvents[iCount]
			);
			if ( pEvents[iCount].Type != 0 ) {
				iCount++;
			}
		}
		if ( iCount != 0 ) {
			*pCount = iCount;
			return XNET_RESULT_OK;
		}

		/* 内部唤醒和迟到身份不能占用调用方的公共事件容量。 */
		iTimeout = 0;
	}
}



/* 累加 eventfd 计数；饱和表示已有通知在途，同样视为成功。 */
static bool __xrtNetEpollWake(xnetport* pPort)
{
	__xrt_net_epoll_context* pContext =
		(__xrt_net_epoll_context*)pPort->Context;
	const uint64 iValue = 1;

	for ( ;; ) {
		ssize_t iWritten = write(
			pContext->Wake,
			&iValue,
			sizeof(iValue)
		);

		if ( iWritten == (ssize_t)sizeof(iValue) ) {
			return true;
		}
		if ( (iWritten < 0) && (errno == EINTR) ) {
			continue;
		}
		if ( (iWritten < 0) &&
			 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
			return true;
		}
		__xrtNetEpollError(
			XNET_ERROR_PORT_POST,
			"wake",
			"waking epoll network port failed",
			iWritten < 0 ? errno : EIO
		);
		return false;
	}
}



/* epoll 是 Linux Tier A readiness 后端，不拥有连接缓冲或执行 Socket IO。 */
static const __xrt_net_port_driver __xrtNetEpollDriver = {
	"epoll",
	XNET_PORT_EPOLL,
	XNET_PORT_CAP_READINESS |
		XNET_PORT_CAP_ONESHOT |
		XNET_PORT_CAP_BATCH |
		XNET_PORT_CAP_WAKE |
		XNET_PORT_CAP_POST,
	__xrtNetEpollInit,
	__xrtNetEpollUnit,
	__xrtNetEpollWatch,
	__xrtNetEpollUnwatch,
	NULL,
	NULL,
	__xrtNetEpollWait,
	__xrtNetEpollWake
};



/* 返回 Linux epoll 后端驱动。 */
const __xrt_net_port_driver* __xrtNetPortEpollDriver(void)
{
	return &__xrtNetEpollDriver;
}

#endif
