#include "../internal/xrt_net_port.h"

#if defined(XRT_FEATURE_NET_PORT_KQUEUE) && \
	(defined(__APPLE__) || defined(__FreeBSD__) || \
	 defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__))
	#include <errno.h>
	#include <fcntl.h>
	#include <limits.h>
	#include <sys/event.h>
	#include <sys/time.h>
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_NET_PORT_KQUEUE) && \
	(defined(__APPLE__) || defined(__FreeBSD__) || \
	 defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__))

#define XRT_NET_KQUEUE_READY_MAX 256u
#define XRT_NET_KQUEUE_WAKE_IDENT UINTPTR_MAX
#define XRT_NET_KQUEUE_WAKE_TOKEN ((uintptr_t)0)

#if defined(EVFILT_USER) && defined(NOTE_TRIGGER)
	#define XRT_NET_KQUEUE_USER_WAKE 1
#else
	#define XRT_NET_KQUEUE_USER_WAKE 0
#endif



typedef struct __xrt_net_kqueue_watch __xrt_net_kqueue_watch;



/* 一个节点同时进入描述符表与令牌表，保证两种查找都保持平均 O(1)。 */
struct __xrt_net_kqueue_watch {
	__xrt_net_kqueue_watch* NativeNext;
	__xrt_net_kqueue_watch* TokenNext;
	xnetsocket Socket;
	uint64 Id;
	uint64 Batch;
	size_t OutputIndex;
	uintptr_t Token;
	uint32 Events;
	int Native;
	ptr User;
};



/* kqueue 上下文保存内核端口、可选 pipe 唤醒和 owner 线程观察区。 */
typedef struct __xrt_net_kqueue_context {
	int Port;
	int WakeRead;
	int WakeWrite;
	__xrt_net_kqueue_watch** NativeBuckets;
	__xrt_net_kqueue_watch** TokenBuckets;
	struct kevent* Ready;
	size_t BucketCount;
	size_t BucketLimit;
	size_t WatchCount;
	int ReadyCapacity;
	uintptr_t NextToken;
	uint64 Batch;
} __xrt_net_kqueue_context;



/* 设置保留 errno 的 kqueue 结构化错误。 */
static void __xrtNetKqueueError(
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



/* 查找指定原生描述符的观察节点链接位置。 */
static __xrt_net_kqueue_watch** __xrtNetKqueueFindNative(
	__xrt_net_kqueue_context* pContext,
	int iNative
)
{
	__xrt_net_kqueue_watch** ppWatch =
		&pContext->NativeBuckets[__xrtNetPortHash(
			(uintptr_t)iNative,
			pContext->BucketCount
		)];

	while ( *ppWatch != NULL ) {
		if ( (*ppWatch)->Native == iNative ) {
			break;
		}
		ppWatch = &(*ppWatch)->NativeNext;
	}
	return ppWatch;
}



/* 同步扩展描述符和代际令牌索引，保持较大容量配置的空闲成本固定。 */
static bool __xrtNetKqueueIndexGrow(__xrt_net_kqueue_context* pContext)
{
	size_t iNext = __xrtNetPortBucketNext(
		pContext->WatchCount + 1u,
		pContext->BucketCount,
		pContext->BucketLimit
	);
	__xrt_net_kqueue_watch** pNative;
	__xrt_net_kqueue_watch** pToken;

	if ( iNext == pContext->BucketCount ) {
		return true;
	}
	pNative = (__xrt_net_kqueue_watch**)xrtCalloc(
		iNext,
		sizeof(*pNative)
	);
	pToken = (__xrt_net_kqueue_watch**)xrtCalloc(
		iNext,
		sizeof(*pToken)
	);
	if ( (pNative == NULL) || (pToken == NULL) ) {
		xrtFree(pNative);
		xrtFree(pToken);
		return false;
	}
	for ( size_t i = 0; i < pContext->BucketCount; i++ ) {
		__xrt_net_kqueue_watch* pWatch = pContext->NativeBuckets[i];

		while ( pWatch != NULL ) {
			__xrt_net_kqueue_watch* pNext = pWatch->NativeNext;
			size_t iNative = __xrtNetPortHash(
				(uintptr_t)pWatch->Native,
				iNext
			);
			size_t iToken = __xrtNetPortHash(
				pWatch->Token,
				iNext
			);

			pWatch->NativeNext = pNative[iNative];
			pNative[iNative] = pWatch;
			pWatch->TokenNext = pToken[iToken];
			pToken[iToken] = pWatch;
			pWatch = pNext;
		}
	}
	xrtFree(pContext->NativeBuckets);
	xrtFree(pContext->TokenBuckets);
	pContext->NativeBuckets = pNative;
	pContext->TokenBuckets = pToken;
	pContext->BucketCount = iNext;
	return true;
}



/* 查找指定代际令牌的观察节点链接位置。 */
static __xrt_net_kqueue_watch** __xrtNetKqueueFindToken(
	__xrt_net_kqueue_context* pContext,
	uintptr_t iToken
)
{
	__xrt_net_kqueue_watch** ppWatch =
		&pContext->TokenBuckets[__xrtNetPortHash(
			iToken,
			pContext->BucketCount
		)];

	while ( *ppWatch != NULL ) {
		if ( (*ppWatch)->Token == iToken ) {
			break;
		}
		ppWatch = &(*ppWatch)->TokenNext;
	}
	return ppWatch;
}



/* 生成当前活动集合中唯一的非零令牌，拒绝迟到事件错配。 */
static uintptr_t __xrtNetKqueueToken(
	__xrt_net_kqueue_context* pContext
)
{
	for ( ;; ) {
		pContext->NextToken++;
		if ( pContext->NextToken == XRT_NET_KQUEUE_WAKE_TOKEN ) {
			continue;
		}
		if ( *__xrtNetKqueueFindToken(
			pContext,
			pContext->NextToken
		) == NULL ) {
			return pContext->NextToken;
		}
	}
}



/* 把观察节点加入内部令牌索引。 */
static void __xrtNetKqueueInsertToken(
	__xrt_net_kqueue_context* pContext,
	__xrt_net_kqueue_watch* pWatch
)
{
	__xrt_net_kqueue_watch** ppWatch =
		__xrtNetKqueueFindToken(pContext, pWatch->Token);

	pWatch->TokenNext = *ppWatch;
	*ppWatch = pWatch;
}



/* 从内部令牌索引移除观察节点。 */
static void __xrtNetKqueueRemoveToken(
	__xrt_net_kqueue_context* pContext,
	__xrt_net_kqueue_watch* pWatch
)
{
	__xrt_net_kqueue_watch** ppWatch =
		__xrtNetKqueueFindToken(pContext, pWatch->Token);

	if ( *ppWatch == pWatch ) {
		*ppWatch = pWatch->TokenNext;
	}
	pWatch->TokenNext = NULL;
}



/* 从两个索引移除并释放一个已经脱离内核的观察节点。 */
static void __xrtNetKqueueRemoveWatch(
	__xrt_net_kqueue_context* pContext,
	__xrt_net_kqueue_watch* pWatch
)
{
	__xrt_net_kqueue_watch** ppNative =
		__xrtNetKqueueFindNative(pContext, pWatch->Native);

	__xrtNetKqueueRemoveToken(pContext, pWatch);
	if ( *ppNative == pWatch ) {
		*ppNative = pWatch->NativeNext;
	}
	pContext->WatchCount--;
	xrtFree(pWatch);
}



/* 为 kqueue 描述符补上 close-on-exec，并重试被信号中断的 fcntl。 */
static bool __xrtNetKqueueCloseOnExec(int iNative)
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



#if !XRT_NET_KQUEUE_USER_WAKE
	/* 为 pipe 唤醒描述符补上非阻塞属性，写满时可安全合并唤醒。 */
	static bool __xrtNetKqueueNonblock(int iNative)
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
#endif



/* 创建不会泄漏到子进程的 kqueue 描述符。 */
static int __xrtNetKqueueCreatePort(void)
{
	int iNative = kqueue();

	if ( iNative < 0 ) {
		return -1;
	}
	if ( !__xrtNetKqueueCloseOnExec(iNative) ) {
		int iSystemCode = errno;

		(void)close(iNative);
		errno = iSystemCode;
		return -1;
	}
	return iNative;
}



/* 释放初始化过程中已经建立的资源，不覆盖原始失败。 */
static void __xrtNetKqueueDiscard(
	__xrt_net_kqueue_context* pContext
)
{
	if ( pContext == NULL ) {
		return;
	}
	if ( pContext->Port >= 0 ) {
		(void)close(pContext->Port);
	}
	if ( pContext->WakeWrite >= 0 ) {
		(void)close(pContext->WakeWrite);
	}
	if ( pContext->WakeRead >= 0 ) {
		(void)close(pContext->WakeRead);
	}
	xrtFree(pContext->Ready);
	xrtFree(pContext->TokenBuckets);
	xrtFree(pContext->NativeBuckets);
	xrtFree(pContext);
}



/* 建立一个过滤器变更，令牌只进入新增或重新武装的观察。 */
static void __xrtNetKqueueSetChange(
	struct kevent* pChange,
	int iNative,
	int16_t iFilter,
	uint16_t iFlags,
	uintptr_t iToken
)
{
	EV_SET(
		pChange,
		(uintptr_t)iNative,
		iFilter,
		iFlags,
		0,
		0,
		(void*)iToken
	);
}



/* 单独应用一个过滤器变更，删除不存在或已关闭的描述符保持幂等。 */
static bool __xrtNetKqueueChange(
	__xrt_net_kqueue_context* pContext,
	int iNative,
	int16_t iFilter,
	uint16_t iFlags,
	uintptr_t iToken,
	bool bIgnoreMissing
)
{
	struct kevent Change;
	int iResult;

	__xrtNetKqueueSetChange(
		&Change,
		iNative,
		iFilter,
		iFlags,
		iToken
	);
	do {
		iResult = kevent(
			pContext->Port,
			&Change,
			1,
			NULL,
			0,
			NULL
		);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) {
		return true;
	}
	return bIgnoreMissing &&
		((errno == ENOENT) || (errno == EBADF));
}



/* 恢复一次失败替换之前的过滤器集合与身份，并报告是否完整恢复。 */
static bool __xrtNetKqueueRestore(
	__xrt_net_kqueue_context* pContext,
	int iNative,
	uint32 iEvents,
	uintptr_t iToken
)
{
	bool bRead;
	bool bWrite;

	if ( (iEvents & XNET_POLL_READ) != 0 ) {
		bRead = __xrtNetKqueueChange(
			pContext,
			iNative,
			EVFILT_READ,
			EV_ADD | EV_ENABLE | EV_ONESHOT,
			iToken,
			false
		);
	} else {
		bRead = __xrtNetKqueueChange(
			pContext,
			iNative,
			EVFILT_READ,
			EV_DELETE,
			0,
			true
		);
	}
	if ( (iEvents & XNET_POLL_WRITE) != 0 ) {
		bWrite = __xrtNetKqueueChange(
			pContext,
			iNative,
			EVFILT_WRITE,
			EV_ADD | EV_ENABLE | EV_ONESHOT,
			iToken,
			false
		);
	} else {
		bWrite = __xrtNetKqueueChange(
			pContext,
			iNative,
			EVFILT_WRITE,
			EV_DELETE,
			0,
			true
		);
	}
	return bRead && bWrite;
}



/* 尽力删除两个过滤器，用于终态和无法恢复的内核状态。 */
static void __xrtNetKqueueDrop(
	__xrt_net_kqueue_context* pContext,
	int iNative
)
{
	(void)__xrtNetKqueueChange(
		pContext,
		iNative,
		EVFILT_READ,
		EV_DELETE,
		0,
		true
	);
	(void)__xrtNetKqueueChange(
		pContext,
		iNative,
		EVFILT_WRITE,
		EV_DELETE,
		0,
		true
	);
}



/* 原子意图替换读写过滤器，并显式返回旧集合是否成功恢复。 */
static bool __xrtNetKqueueApply(
	__xrt_net_kqueue_context* pContext,
	int iNative,
	uint32 iOldEvents,
	uintptr_t iOldToken,
	uint32 iNewEvents,
	uintptr_t iNewToken,
	bool* pRestored
)
{
	struct kevent Changes[2];
	int iCount = 0;
	int iResult;

	if ( (iNewEvents & XNET_POLL_READ) != 0 ) {
		__xrtNetKqueueSetChange(
			&Changes[iCount++],
			iNative,
			EVFILT_READ,
			EV_ADD | EV_ENABLE | EV_ONESHOT,
			iNewToken
		);
	} else if ( (iOldEvents & XNET_POLL_READ) != 0 ) {
		__xrtNetKqueueSetChange(
			&Changes[iCount++],
			iNative,
			EVFILT_READ,
			EV_DELETE,
			0
		);
	}
	if ( (iNewEvents & XNET_POLL_WRITE) != 0 ) {
		__xrtNetKqueueSetChange(
			&Changes[iCount++],
			iNative,
			EVFILT_WRITE,
			EV_ADD | EV_ENABLE | EV_ONESHOT,
			iNewToken
		);
	} else if ( (iOldEvents & XNET_POLL_WRITE) != 0 ) {
		__xrtNetKqueueSetChange(
			&Changes[iCount++],
			iNative,
			EVFILT_WRITE,
			EV_DELETE,
			0
		);
	}

	do {
		iResult = kevent(
			pContext->Port,
			Changes,
			iCount,
			NULL,
			0,
			NULL
		);
	} while ( (iResult != 0) && (errno == EINTR) );
	if ( iResult == 0 ) {
		return true;
	}
	{
		int iSystemCode = errno;

		*pRestored = __xrtNetKqueueRestore(
			pContext,
			iNative,
			iOldEvents,
			iOldToken
		);
		if ( !*pRestored ) {
			__xrtNetKqueueDrop(pContext, iNative);
		}
		errno = iSystemCode;
	}
	return false;
}



/* 删除过滤器；部分失败时报告旧集合是否恢复，避免用户态分叉。 */
static bool __xrtNetKqueueDelete(
	__xrt_net_kqueue_context* pContext,
	const __xrt_net_kqueue_watch* pWatch,
	bool* pRestored
)
{
	*pRestored = true;
	if ( ((pWatch->Events & XNET_POLL_READ) != 0) &&
		 !__xrtNetKqueueChange(
			pContext,
			pWatch->Native,
			EVFILT_READ,
			EV_DELETE,
			0,
			true
		 ) ) {
		return false;
	}
	if ( ((pWatch->Events & XNET_POLL_WRITE) != 0) &&
		 !__xrtNetKqueueChange(
			pContext,
			pWatch->Native,
			EVFILT_WRITE,
			EV_DELETE,
			0,
			true
		 ) ) {
		int iSystemCode = errno;

		*pRestored = __xrtNetKqueueRestore(
			pContext,
			pWatch->Native,
			pWatch->Events,
			pWatch->Token
		);
		if ( !*pRestored ) {
			__xrtNetKqueueDrop(
				pContext,
				pWatch->Native
			);
		}
		errno = iSystemCode;
		return false;
	}
	return true;
}



/* 建立当前 BSD 可用的可合并唤醒源，并注册到同一个 kqueue。 */
static bool __xrtNetKqueueOpenWake(
	__xrt_net_kqueue_context* pContext
)
{
	struct kevent Wake;

	#if XRT_NET_KQUEUE_USER_WAKE
		int iResult;

		EV_SET(
			&Wake,
			XRT_NET_KQUEUE_WAKE_IDENT,
			EVFILT_USER,
			EV_ADD | EV_CLEAR,
			0,
			0,
			(void*)XRT_NET_KQUEUE_WAKE_TOKEN
		);
		do {
			iResult = kevent(
				pContext->Port,
				&Wake,
				1,
				NULL,
				0,
				NULL
			);
		} while ( (iResult != 0) && (errno == EINTR) );
		return iResult == 0;
	#else
		int Fds[2];
		int iResult;

		do {
			iResult = pipe(Fds);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			return false;
		}
		if ( !__xrtNetKqueueCloseOnExec(Fds[0]) ||
			 !__xrtNetKqueueCloseOnExec(Fds[1]) ||
			 !__xrtNetKqueueNonblock(Fds[0]) ||
			 !__xrtNetKqueueNonblock(Fds[1]) ) {
			int iSystemCode = errno;

			(void)close(Fds[1]);
			(void)close(Fds[0]);
			errno = iSystemCode;
			return false;
		}

		pContext->WakeRead = Fds[0];
		pContext->WakeWrite = Fds[1];
		EV_SET(
			&Wake,
			(uintptr_t)pContext->WakeRead,
			EVFILT_READ,
			EV_ADD | EV_ENABLE | EV_CLEAR,
			0,
			0,
			(void*)XRT_NET_KQUEUE_WAKE_TOKEN
		);
		do {
			iResult = kevent(
				pContext->Port,
				&Wake,
				1,
				NULL,
				0,
				NULL
			);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult != 0 ) {
			int iSystemCode = errno;

			(void)close(pContext->WakeWrite);
			(void)close(pContext->WakeRead);
			pContext->WakeWrite = -1;
			pContext->WakeRead = -1;
			errno = iSystemCode;
			return false;
		}
		return true;
	#endif
}



/* 初始化 kqueue、双索引观察表、批量事件区和平台唤醒源。 */
static bool __xrtNetKqueueInit(xnetport* pPort)
{
	__xrt_net_kqueue_context* pContext;
	size_t iReadyCapacity = pPort->Config.WatchLimit;
	int iSystemCode;

	pContext = (__xrt_net_kqueue_context*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return false;
	}
	pContext->Port = -1;
	pContext->WakeRead = -1;
	pContext->WakeWrite = -1;
	pContext->BucketCount = XRT_NET_PORT_BUCKET_MIN;
	pContext->BucketLimit = __xrtNetPortBucketCount(
		pPort->Config.WatchLimit
	);
	if ( iReadyCapacity > XRT_NET_KQUEUE_READY_MAX ) {
		iReadyCapacity = XRT_NET_KQUEUE_READY_MAX;
	}
	pContext->ReadyCapacity = (int)iReadyCapacity;
	pContext->NativeBuckets = (__xrt_net_kqueue_watch**)xrtCalloc(
		pContext->BucketCount,
		sizeof(*pContext->NativeBuckets)
	);
	pContext->TokenBuckets = (__xrt_net_kqueue_watch**)xrtCalloc(
		pContext->BucketCount,
		sizeof(*pContext->TokenBuckets)
	);
	pContext->Ready = (struct kevent*)xrtCalloc(
		iReadyCapacity,
		sizeof(*pContext->Ready)
	);
	if ( (pContext->NativeBuckets == NULL) ||
		 (pContext->TokenBuckets == NULL) ||
		 (pContext->Ready == NULL) ) {
		__xrtNetKqueueDiscard(pContext);
		return false;
	}

	pContext->Port = __xrtNetKqueueCreatePort();
	if ( pContext->Port < 0 ) {
		iSystemCode = errno;
		__xrtNetKqueueDiscard(pContext);
		__xrtNetKqueueError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating kqueue instance failed",
			iSystemCode
		);
		return false;
	}

	if ( !__xrtNetKqueueOpenWake(pContext) ) {
		iSystemCode = errno;
		__xrtNetKqueueDiscard(pContext);
		__xrtNetKqueueError(
			XNET_ERROR_PORT_CREATE,
			"create-port",
			"creating kqueue wake channel failed",
			iSystemCode
		);
		return false;
	}
	pPort->Context = pContext;
	return true;
}



/* 释放观察表、索引和内核端口，关闭失败仍完成全部内存清理。 */
static bool __xrtNetKqueueUnit(xnetport* pPort)
{
	__xrt_net_kqueue_context* pContext =
		(__xrt_net_kqueue_context*)pPort->Context;
	int iSystemCode = 0;

	if ( pContext == NULL ) {
		return true;
	}
	for ( size_t i = 0; i < pContext->BucketCount; i++ ) {
		__xrt_net_kqueue_watch* pWatch =
			pContext->NativeBuckets[i];

		while ( pWatch != NULL ) {
			__xrt_net_kqueue_watch* pNext =
				pWatch->NativeNext;

			xrtFree(pWatch);
			pWatch = pNext;
		}
	}
	if ( (pContext->Port >= 0) &&
		 (close(pContext->Port) != 0) ) {
		iSystemCode = errno;
	}
	if ( (pContext->WakeWrite >= 0) &&
		 (close(pContext->WakeWrite) != 0) &&
		 (iSystemCode == 0) ) {
		iSystemCode = errno;
	}
	if ( (pContext->WakeRead >= 0) &&
		 (close(pContext->WakeRead) != 0) &&
		 (iSystemCode == 0) ) {
		iSystemCode = errno;
	}
	xrtFree(pContext->Ready);
	xrtFree(pContext->TokenBuckets);
	xrtFree(pContext->NativeBuckets);
	xrtFree(pContext);
	pPort->Context = NULL;

	if ( iSystemCode != 0 ) {
		__xrtNetKqueueError(
			XNET_ERROR_PORT_CLOSE,
			"destroy-port",
			"closing kqueue network port failed",
			iSystemCode
		);
		return false;
	}
	return true;
}



/* 新增或替换一个带独立代际身份的 kqueue one-shot 观察。 */
static bool __xrtNetKqueueWatch(
	xnetport* pPort,
	xnetsocket Socket,
	uint64 Id,
	uint32 iEvents,
	ptr pUser
)
{
	__xrt_net_kqueue_context* pContext =
		(__xrt_net_kqueue_context*)pPort->Context;
	intptr_t iNativeValue = xrtNetSocketNative(Socket);
	__xrt_net_kqueue_watch** ppWatch;
	__xrt_net_kqueue_watch* pWatch;
	uint32 iOldEvents = 0;
	uintptr_t iOldToken = 0;
	uintptr_t iNewToken;
	bool bRestored = true;
	int iNative;

	if ( (iNativeValue < 0) || (iNativeValue > INT_MAX) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"watch",
			"socket cannot be represented by kqueue",
			0
		);
		return false;
	}
	iNative = (int)iNativeValue;
	ppWatch = __xrtNetKqueueFindNative(pContext, iNative);
	pWatch = *ppWatch;
	if ( (pWatch == NULL) &&
		 (pContext->WatchCount >= pPort->Config.WatchLimit) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"watch",
			"kqueue watch limit reached",
			0
		);
		return false;
	}
	if ( pWatch == NULL ) {
		if ( !__xrtNetKqueueIndexGrow(pContext) ) {
			return false;
		}
		ppWatch = __xrtNetKqueueFindNative(pContext, iNative);
		pWatch = (__xrt_net_kqueue_watch*)xrtCalloc(
			1,
			sizeof(*pWatch)
		);
		if ( pWatch == NULL ) {
			return false;
		}
		pWatch->Native = iNative;
	} else {
		iOldEvents = pWatch->Events;
		iOldToken = pWatch->Token;
	}
	iNewToken = __xrtNetKqueueToken(pContext);
	if ( !__xrtNetKqueueApply(
		pContext,
		iNative,
		iOldEvents,
		iOldToken,
		iEvents,
		iNewToken,
		&bRestored
	) ) {
		int iSystemCode = errno;

		if ( iOldEvents == 0 ) {
			xrtFree(pWatch);
		} else if ( !bRestored ) {
			__xrtNetKqueueRemoveWatch(
				pContext,
				pWatch
			);
		}
		__xrtNetKqueueError(
			XNET_ERROR_PORT_WATCH,
			"watch",
			"registering kqueue socket failed",
			iSystemCode
		);
		return false;
	}

	if ( iOldEvents == 0 ) {
		pWatch->NativeNext = *ppWatch;
		*ppWatch = pWatch;
		pContext->WatchCount++;
	} else {
		__xrtNetKqueueRemoveToken(pContext, pWatch);
	}
	pWatch->Socket = Socket;
	pWatch->Id = Id;
	pWatch->Token = iNewToken;
	pWatch->Events = iEvents;
	pWatch->User = pUser;
	__xrtNetKqueueInsertToken(pContext, pWatch);
	return true;
}



/* 幂等移除一个 kqueue readiness 观察。 */
static bool __xrtNetKqueueUnwatch(
	xnetport* pPort,
	xnetsocket Socket
)
{
	__xrt_net_kqueue_context* pContext =
		(__xrt_net_kqueue_context*)pPort->Context;
	intptr_t iNativeValue = xrtNetSocketNative(Socket);
	__xrt_net_kqueue_watch** ppWatch;
	__xrt_net_kqueue_watch* pWatch;
	bool bRestored = true;
	int iNative;

	if ( (iNativeValue < 0) || (iNativeValue > INT_MAX) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_WATCH,
			"unwatch",
			"socket cannot be represented by kqueue",
			0
		);
		return false;
	}
	iNative = (int)iNativeValue;
	ppWatch = __xrtNetKqueueFindNative(pContext, iNative);
	pWatch = *ppWatch;
	if ( pWatch == NULL ) {
		return true;
	}
	if ( !__xrtNetKqueueDelete(
		pContext,
		pWatch,
		&bRestored
	) ) {
		int iSystemCode = errno;

		/* 移除令牌映射，确保恢复的旧过滤器也不能回调用户身份。 */
		__xrtNetKqueueRemoveWatch(
			pContext,
			pWatch
		);
		__xrtNetKqueueError(
			XNET_ERROR_PORT_WATCH,
			"unwatch",
			"removing kqueue socket failed",
			iSystemCode
		);
		return false;
	}
	__xrtNetKqueueRemoveWatch(pContext, pWatch);
	return true;
}



/* 把微秒等待转换为有界 timespec，无限等待返回空指针。 */
static const struct timespec* __xrtNetKqueueTimeout(
	uint64 iTimeout,
	struct timespec* pTimeout
)
{
	uint64 iSeconds;
	uint64 iMaximum;

	if ( iTimeout == UINT64_MAX ) {
		return NULL;
	}
	iSeconds = iTimeout / UINT64_C(1000000);
	iMaximum = (sizeof(time_t) >= sizeof(int64)) ?
		(uint64)INT64_MAX : (uint64)INT32_MAX;
	if ( iSeconds > iMaximum ) {
		pTimeout->tv_sec = (time_t)iMaximum;
		pTimeout->tv_nsec = 999999999L;
	} else {
		pTimeout->tv_sec = (time_t)iSeconds;
		pTimeout->tv_nsec = (long)(
			(iTimeout % UINT64_C(1000000)) * UINT64_C(1000)
		);
	}
	return pTimeout;
}



/* 把单个 kqueue 过滤器事件映射为稳定公共标志与系统错误。 */
static uint32 __xrtNetKqueueEventFlags(
	const __xrt_net_kqueue_watch* pWatch,
	const struct kevent* pReady,
	int* pSystemCode
)
{
	uint32 iFlags = 0;

	*pSystemCode = 0;
	if ( (pReady->filter == EVFILT_READ) &&
		 ((pWatch->Events & XNET_POLL_READ) != 0) ) {
		iFlags |= XNET_PORT_EVENT_READ;
	}
	if ( (pReady->filter == EVFILT_WRITE) &&
		 ((pWatch->Events & XNET_POLL_WRITE) != 0) ) {
		iFlags |= XNET_PORT_EVENT_WRITE;
	}
	if ( (pReady->flags & EV_ERROR) != 0 ) {
		iFlags |= XNET_PORT_EVENT_ERROR;
		*pSystemCode = pReady->data != 0 ?
			(int)pReady->data : EIO;
	}
	if ( (pReady->flags & EV_EOF) != 0 ) {
		iFlags |= XNET_PORT_EVENT_HANGUP;
		if ( pReady->filter == EVFILT_READ ) {
			iFlags |= XNET_PORT_EVENT_READ;
		}
		if ( pReady->filter == EVFILT_WRITE ) {
			iFlags |= XNET_PORT_EVENT_WRITE;
		}
		if ( pReady->fflags != 0 ) {
			iFlags |= XNET_PORT_EVENT_ERROR;
			*pSystemCode = (int)pReady->fflags;
		}
	}
	return iFlags;
}



/* 判断一个内核事件是否来自当前平台选择的内部唤醒源。 */
static bool __xrtNetKqueueWakeReady(
	const __xrt_net_kqueue_context* pContext,
	const struct kevent* pReady
)
{
	#if XRT_NET_KQUEUE_USER_WAKE
		(void)pContext;
		return (pReady->filter == EVFILT_USER) &&
			(pReady->ident == XRT_NET_KQUEUE_WAKE_IDENT);
	#else
		return (pReady->filter == EVFILT_READ) &&
			(pReady->ident == (uintptr_t)pContext->WakeRead) &&
			((uintptr_t)pReady->udata == XRT_NET_KQUEUE_WAKE_TOKEN);
	#endif
}



#if !XRT_NET_KQUEUE_USER_WAKE
	/* 排空 pipe 中全部唤醒字节；EAGAIN 表示本轮已完全消费。 */
	static bool __xrtNetKqueueWakeDrain(
		__xrt_net_kqueue_context* pContext
	)
	{
		uint8 Data[128];

		for ( ;; ) {
			ssize_t iResult = read(
				pContext->WakeRead,
				Data,
				sizeof(Data)
			);

			if ( iResult > 0 ) {
				continue;
			}
			if ( (iResult < 0) && (errno == EINTR) ) {
				continue;
			}
			if ( (iResult < 0) &&
				 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
				return true;
			}
			__xrtNetKqueueError(
				XNET_ERROR_PORT_WAIT,
				"wait",
				"draining kqueue wake channel failed",
				(iResult < 0) ? errno : EIO
			);
			return false;
		}
	}
#endif



/* 合并同批同观察的读写事件，并在 one-shot 终结后释放节点。 */
static void __xrtNetKqueuePublish(
	__xrt_net_kqueue_context* pContext,
	const struct kevent* pReady,
	xnetportevent* pEvents,
	size_t* pCount
)
{
	uintptr_t iToken = (uintptr_t)pReady->udata;
	__xrt_net_kqueue_watch** ppWatch =
		__xrtNetKqueueFindToken(pContext, iToken);
	__xrt_net_kqueue_watch* pWatch = *ppWatch;
	xnetportevent* pEvent;
	uint32 iFlags;
	int iSystemCode;

	if ( pWatch == NULL ) {
		return;
	}
	iFlags = __xrtNetKqueueEventFlags(
		pWatch,
		pReady,
		&iSystemCode
	);
	if ( iFlags == 0 ) {
		return;
	}

	if ( pWatch->Batch == pContext->Batch ) {
		pEvent = &pEvents[pWatch->OutputIndex];
	} else {
		pWatch->Batch = pContext->Batch;
		pWatch->OutputIndex = *pCount;
		pEvent = &pEvents[(*pCount)++];
		memset(pEvent, 0, sizeof(*pEvent));
		pEvent->Type = XNET_PORT_EVENT_READY;
		pEvent->Result = XNET_RESULT_OK;
		pEvent->Id = pWatch->Id;
		pEvent->Socket = pWatch->Socket;
		pEvent->User = pWatch->User;
	}
	pEvent->Flags |= iFlags;
	if ( iSystemCode != 0 ) {
		pEvent->SystemCode = iSystemCode;
	}

	if ( (iFlags & (XNET_PORT_EVENT_ERROR |
		 XNET_PORT_EVENT_HANGUP)) != 0 ) {
		__xrtNetKqueueDrop(
			pContext,
			pWatch->Native
		);
		pWatch->Events = 0;
	} else if ( pReady->filter == EVFILT_READ ) {
		pWatch->Events &= ~((uint32)XNET_POLL_READ);
	} else if ( pReady->filter == EVFILT_WRITE ) {
		pWatch->Events &= ~((uint32)XNET_POLL_WRITE);
	}
	if ( pWatch->Events == 0 ) {
		__xrtNetKqueueRemoveWatch(pContext, pWatch);
	}
}



/* 等待并批量提取 one-shot readiness；内部唤醒只推进公共端口队列。 */
static xnetresult __xrtNetKqueueWait(
	xnetport* pPort,
	xnetportevent* pEvents,
	size_t iCapacity,
	uint64 iTimeout,
	size_t* pCount
)
{
	__xrt_net_kqueue_context* pContext =
		(__xrt_net_kqueue_context*)pPort->Context;
	struct timespec Timeout;
	const struct timespec* pTimeout;
	int iMaximum = pContext->ReadyCapacity;
	int iReady;
	size_t iCount = 0;

	*pCount = 0;
	if ( iCapacity < (size_t)iMaximum ) {
		iMaximum = (int)iCapacity;
	}
	pTimeout = __xrtNetKqueueTimeout(iTimeout, &Timeout);
	iReady = kevent(
		pContext->Port,
		NULL,
		0,
		pContext->Ready,
		iMaximum,
		pTimeout
	);
	if ( iReady == 0 ) {
		return iTimeout == 0 ?
			XNET_RESULT_OK : XNET_RESULT_TIMEOUT;
	}
	if ( iReady < 0 ) {
		if ( errno == EINTR ) {
			return XNET_RESULT_OK;
		}
		__xrtNetKqueueError(
			XNET_ERROR_PORT_WAIT,
			"wait",
			"kqueue wait failed",
			errno
		);
		return XNET_RESULT_ERROR;
	}

	pContext->Batch++;
	if ( pContext->Batch == 0 ) {
		pContext->Batch++;
	}
	for ( int i = 0; i < iReady; i++ ) {
		if ( __xrtNetKqueueWakeReady(
			pContext,
			&pContext->Ready[i]
		) ) {
			#if !XRT_NET_KQUEUE_USER_WAKE
				if ( !__xrtNetKqueueWakeDrain(pContext) ) {
					return XNET_RESULT_ERROR;
				}
			#endif
			continue;
		}
		__xrtNetKqueuePublish(
			pContext,
			&pContext->Ready[i],
			pEvents,
			&iCount
		);
	}
	if ( (iCount == 0) && (iTimeout == 0) ) {
		/* 本轮只消化了内部唤醒；唤醒不得吞掉一次非阻塞轮询，
		   再查一次避免就绪事件被积压的用户事件饿死。 */
		iReady = kevent(
			pContext->Port,
			NULL,
			0,
			pContext->Ready,
			iMaximum,
			pTimeout
		);
		if ( iReady > 0 ) {
			for ( int i = 0; i < iReady; i++ ) {
				if ( __xrtNetKqueueWakeReady(
					pContext,
					&pContext->Ready[i]
				) ) {
					continue;
				}
				__xrtNetKqueuePublish(
					pContext,
					&pContext->Ready[i],
					pEvents,
					&iCount
				);
			}
		}
	}
	*pCount = iCount;
	return XNET_RESULT_OK;
}



/* 触发平台可用的可合并用户过滤器或非阻塞 pipe。 */
static bool __xrtNetKqueueWake(xnetport* pPort)
{
	__xrt_net_kqueue_context* pContext =
		(__xrt_net_kqueue_context*)pPort->Context;

	#if XRT_NET_KQUEUE_USER_WAKE
		struct kevent Trigger;
		int iResult;

		EV_SET(
			&Trigger,
			XRT_NET_KQUEUE_WAKE_IDENT,
			EVFILT_USER,
			0,
			NOTE_TRIGGER,
			0,
			(void*)XRT_NET_KQUEUE_WAKE_TOKEN
		);
		do {
			iResult = kevent(
				pContext->Port,
				&Trigger,
				1,
				NULL,
				0,
				NULL
			);
		} while ( (iResult != 0) && (errno == EINTR) );
		if ( iResult == 0 ) {
			return true;
		}
	#else
		uint8 iByte = 1;

		for ( ;; ) {
			ssize_t iResult = write(
				pContext->WakeWrite,
				&iByte,
				1
			);

			if ( iResult == 1 ) {
				return true;
			}
			if ( (iResult < 0) && (errno == EINTR) ) {
				continue;
			}
			if ( (iResult < 0) &&
				 ((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
				return true;
			}
			errno = (iResult < 0) ? errno : EIO;
			break;
		}
	#endif

	__xrtNetKqueueError(
		XNET_ERROR_PORT_POST,
		"wake",
		"waking kqueue network port failed",
		errno
	);
	return false;
}



/* kqueue 是 Darwin/BSD Tier A readiness 后端，不拥有载荷或执行 Socket IO。 */
static const __xrt_net_port_driver __xrtNetKqueueDriver = {
	"kqueue",
	XNET_PORT_KQUEUE,
	XNET_PORT_CAP_READINESS |
		XNET_PORT_CAP_ONESHOT |
		XNET_PORT_CAP_BATCH |
		XNET_PORT_CAP_WAKE |
		XNET_PORT_CAP_POST,
	__xrtNetKqueueInit,
	__xrtNetKqueueUnit,
	__xrtNetKqueueWatch,
	__xrtNetKqueueUnwatch,
	NULL,
	NULL,
	__xrtNetKqueueWait,
	__xrtNetKqueueWake
};



/* 返回 Darwin/BSD kqueue 后端驱动。 */
const __xrt_net_port_driver* __xrtNetPortKqueueDriver(void)
{
	return &__xrtNetKqueueDriver;
}

#endif
