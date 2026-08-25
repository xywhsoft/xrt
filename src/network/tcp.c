#include "../internal/xrt_tcp.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <errno.h>
#endif

#if defined(XRT_FEATURE_NET_TCP_FILE)
	#if !defined(_WIN32) && !defined(_WIN64)
		#include <fcntl.h>
		#include <unistd.h>
		#if defined(__linux__)
			#include <sys/sendfile.h>
		#elif defined(__APPLE__) || defined(__FreeBSD__)
			#include <sys/socket.h>
			#include <sys/types.h>
			#include <sys/uio.h>
		#endif
	#endif
#endif



#if defined(XRT_FEATURE_NET_TCP)

#define XRT_NET_STREAM_READ_DEFAULT 2048u
#define XRT_NET_STREAM_READ_LIMIT_DEFAULT (1024u * 1024u)
#define XRT_NET_STREAM_WRITE_HIGH_DEFAULT (256u * 1024u)
#define XRT_NET_STREAM_WRITE_LOW_DEFAULT (64u * 1024u)
#define XRT_NET_STREAM_WRITE_LIMIT_DEFAULT (1024u * 1024u)
#define XRT_NET_STREAM_CONNECT_TIMEOUT_DEFAULT 30000000u
#define XRT_NET_LISTENER_ACCEPT_DEFAULT 16u
#define XRT_NET_LISTENER_ACCEPT_MAX 1024u
#define XRT_NET_LISTENER_QUEUE_DEFAULT 256u
#define XRT_NET_LISTENER_BACKLOG_DEFAULT 256
#define XRT_NET_LISTENER_RETRY_MIN 10000u
#define XRT_NET_LISTENER_RETRY_MAX 1000000u
#define XRT_NET_STREAM_IO_BUDGET 16u
#define XRT_NET_STREAM_CONTROL_RESUME 0x00000001u
#define XRT_NET_STREAM_CONTROL_SHUTDOWN 0x00000002u
#define XRT_NET_STREAM_CONTROL_CLOSE 0x00000004u
#define XRT_NET_STREAM_CONTROL_ABORT 0x00000008u
#define XRT_NET_STREAM_CONTROL_FINISH 0x00000010u
#define XRT_NET_STREAM_CONTROL_POSTED 0x80000000u
#define XRT_NET_STREAM_CONTROL_LIFECYCLE \
	(XRT_NET_STREAM_CONTROL_SHUTDOWN | \
	 XRT_NET_STREAM_CONTROL_CLOSE | \
	 XRT_NET_STREAM_CONTROL_ABORT)



typedef struct __xrt_net_accept_task {
	__xrt_net_engine_internal Internal;
	xnetlistener* Listener;
	xnetstream* Stream;
	xerror* Error;
} __xrt_net_accept_task;



static bool __xrtNetListenerArmAccepts(xnetlistener* pListener);
static bool __xrtNetListenerWatch(xnetlistener* pListener);



/* 从 Listener Worker 的统一小节点缓存取得分发任务。 */
static __xrt_net_accept_task* __xrtNetListenerTaskAlloc(
	xnetlistener* pListener
)
{
	return (__xrt_net_accept_task*)__xrtNetWorkerNodeAlloc(
		pListener->Worker,
		sizeof(__xrt_net_accept_task)
	);
}



/* 把已经完成的分发任务放回 Listener Worker。 */
static void __xrtNetListenerTaskRecycle(
	xnetlistener* pListener,
	__xrt_net_accept_task* pTask
)
{
	__xrtNetWorkerNodeRecycle(
		pListener->Worker,
		pTask,
		sizeof(*pTask)
	);
}



/* 在所属 Worker 上取得当前回调数据，不额外建立跨线程屏障。 */
static ptr __xrtNetStreamDataCurrent(const xnetstream* pStream)
{
	return xrtAtomicPtrLoad(&pStream->Data, XMEMORY_RELAXED);
}



/* 可裁剪地推进统一 Future 适配层。 */
static void __xrtNetStreamNotifyFutures(
	xnetstream* pStream,
	bool bDriveRead
)
{
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtNetStreamFutureNotify(pStream, bDriveRead);
	#else
		(void)pStream;
		(void)bDriveRead;
	#endif
}



/* 可裁剪地推进 Listener Future 适配层。 */
static void __xrtNetListenerNotifyFutures(xnetlistener* pListener)
{
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtNetListenerFutureNotify(pListener);
	#else
		(void)pListener;
	#endif
}



/* 可裁剪地把已接受 Stream 直接交给最早的 Future。 */
static bool __xrtNetListenerAcceptFuture(
	xnetlistener* pListener,
	xnetstream* pStream
)
{
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		return __xrtNetListenerFutureAccept(pListener, pStream);
	#else
		(void)pListener;
		(void)pStream;
		return false;
	#endif
}



/* 设置 TCP 层结构化错误。 */
static void __xrtNetStreamSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 保存第一个导致 Stream 终止的错误。 */
static void __xrtNetStreamRememberError(xnetstream* pStream)
{
	xerror* pError = xrtTakeError();

	if ( pStream->Error == NULL ) {
		pStream->Error = pError;
	} else {
		xrtErrorFree(pError);
	}
}



/* 把端口终态中的系统错误转换成完整 TCP 错误。 */
static void __xrtNetStreamEventError(
	xnetstream* pStream,
	const xnetportevent* pEvent,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	if ( pEvent->SystemCode != 0 ) {
		__xrtNetSocketSetSystemError(
			Code,
			sOperation,
			sMessage,
			pEvent->SystemCode
		);
	} else {
		__xrtNetStreamSetError(XERR_IO, Code, sOperation, sMessage);
	}
	__xrtNetStreamRememberError(pStream);
}



/* 验证 Stream 的读取和写入硬边界。 */
bool __xrtNetStreamConfigValid(const xnetstreamconfig* pConfig)
{
	if ( (pConfig->ReadSize == 0) ||
		 (pConfig->ReadLimit < pConfig->ReadSize) ||
		 (pConfig->ReadMode > XNET_STREAM_READ_PROBE) ||
		 (pConfig->WriteLimit == 0) ||
		 (pConfig->WriteHighWater == 0) ||
		 (pConfig->WriteHighWater > pConfig->WriteLimit) ||
		 (pConfig->WriteLowWater > pConfig->WriteHighWater) ) {
		__xrtNetStreamSetError(
			XERR_ARGUMENT,
			XNET_ERROR_STREAM_CONFIG,
			"configure-stream",
			"invalid TCP stream limits"
		);
		return false;
	}
	return true;
}



/* 验证 Listener 地址、并发数和 backlog。 */
bool __xrtNetListenConfigValid(const xnetlistenconfig* pConfig)
{
	if ( ((pConfig->Address.Family != XNET_FAMILY_IPV4) &&
		  (pConfig->Address.Family != XNET_FAMILY_IPV6)) ||
		 (pConfig->AcceptConcurrency == 0) ||
		 (pConfig->AcceptConcurrency > XRT_NET_LISTENER_ACCEPT_MAX) ||
		 (pConfig->AcceptQueueLimit == 0) ||
		 (pConfig->Backlog <= 0) ||
		 ((pConfig->Distribution != XNET_ACCEPT_ROUND_ROBIN) &&
		  (pConfig->Distribution != XNET_ACCEPT_LOCAL)) ||
		 (pConfig->ExclusiveAddress &&
		  (pConfig->ReuseAddress || pConfig->ReusePort)) ||
		 !__xrtNetStreamConfigValid(&pConfig->Stream) ) {
		if ( xrtGetError() == NULL ) {
			__xrtNetStreamSetError(
				XERR_ARGUMENT,
				XNET_ERROR_LISTENER_CREATE,
				"create-listener",
				"invalid TCP listener configuration"
			);
		}
		return false;
	}
	return true;
}



/* 初始化 Stream 默认配置。 */
XRT_API void xrtNetStreamConfigInit(xnetstreamconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->ReadSize = XRT_NET_STREAM_READ_DEFAULT;
	pConfig->ReadLimit = XRT_NET_STREAM_READ_LIMIT_DEFAULT;
	pConfig->WriteHighWater = XRT_NET_STREAM_WRITE_HIGH_DEFAULT;
	pConfig->WriteLowWater = XRT_NET_STREAM_WRITE_LOW_DEFAULT;
	pConfig->WriteLimit = XRT_NET_STREAM_WRITE_LIMIT_DEFAULT;
	pConfig->ConnectTimeout = XRT_NET_STREAM_CONNECT_TIMEOUT_DEFAULT;
	pConfig->NoDelay = true;
}



/* 初始化 Listener 默认配置。 */
XRT_API void xrtNetListenConfigInit(xnetlistenconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	(void)xrtNetAddrAny(&pConfig->Address, XNET_FAMILY_IPV4, 0);
	xrtNetStreamConfigInit(&pConfig->Stream);
	pConfig->AcceptConcurrency = XRT_NET_LISTENER_ACCEPT_DEFAULT;
	pConfig->AcceptQueueLimit = XRT_NET_LISTENER_QUEUE_DEFAULT;
	pConfig->Backlog = XRT_NET_LISTENER_BACKLOG_DEFAULT;
	#if defined(_WIN32) || defined(_WIN64)
		pConfig->ExclusiveAddress = true;
	#else
		pConfig->ReuseAddress = true;
	#endif
}



/* 增加 Stream 引用。 */
XRT_API xnetstream* xrtNetStreamRef(xnetstream* pStream)
{
	if ( (pStream == NULL) ||
		 (xrtRefRetain(&pStream->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pStream;
}



/* 释放 Stream 尚未进入终态的 Engine 占用。 */
static void __xrtNetStreamReleaseEngine(xnetstream* pStream)
{
	if ( pStream->EngineHeld ) {
		pStream->EngineHeld = false;
		__xrtNetEngineObjectRelease(pStream->Engine);
	}
}



/* 释放 Listener 尚未进入终态的 Engine 占用。 */
static void __xrtNetListenerReleaseEngine(xnetlistener* pListener)
{
	if ( pListener->EngineHeld ) {
		pListener->EngineHeld = false;
		__xrtNetEngineObjectRelease(pListener->Engine);
	}
}



/* 释放最后一个 Stream 引用及其残留资源。 */
XRT_API void xrtNetStreamDestroy(xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pStream->References) != 0 ) {
		return;
	}
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtSpinUnit(&pStream->WaitLock);
	#endif
	xrtErrorFree(pStream->Error);
	__xrtNetStreamReleaseEngine(pStream);
	xrtFree(pStream);
}



/* 增加 Listener 引用。 */
XRT_API xnetlistener* xrtNetListenerRef(xnetlistener* pListener)
{
	if ( (pListener == NULL) ||
		 (xrtRefRetain(&pListener->References) < 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pListener;
}



/* 释放最后一个 Listener 引用及其残留资源。 */
XRT_API void xrtNetListenerDestroy(xnetlistener* pListener)
{
	if ( pListener == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pListener->References) != 0 ) {
		return;
	}
	__xrtSpinUnit(&pListener->AcceptLock);
	xrtFree(pListener->AcceptSlots);
	__xrtNetListenerReleaseEngine(pListener);
	xrtFree(pListener);
}



/* 调用方持有 AcceptLock 时取走一个排队 Stream。 */
xnetstream* __xrtNetListenerTakeQueued(xnetlistener* pListener)
{
	xnetstream* pStream = pListener->AcceptHead;

	if ( pStream == NULL ) {
		return NULL;
	}
	pListener->AcceptHead = pStream->AcceptNext;
	if ( pListener->AcceptHead == NULL ) {
		pListener->AcceptTail = NULL;
	}
	pStream->AcceptNext = NULL;
	(void)xrtAtomic32FetchSub(
		&pListener->QueuedAccepts,
		1,
		XMEMORY_ACQ_REL
	);
	return pStream;
}



/* 把一个已初始化 Stream 放入有界拉取队列。 */
static bool __xrtNetListenerQueue(
	xnetlistener* pListener,
	xnetstream* pStream
)
{
	uint32 iQueued;
	bool bQueued = false;

	__xrtSpinLock(&pListener->AcceptLock);
	iQueued = xrtAtomic32Load(
		&pListener->QueuedAccepts,
		XMEMORY_RELAXED
	);
	if ( (xrtNetListenerState(pListener) == XNET_LISTENER_OPEN) &&
		 (iQueued < pListener->Config.AcceptQueueLimit) ) {
		pStream->AcceptNext = NULL;
		if ( pListener->AcceptTail != NULL ) {
			pListener->AcceptTail->AcceptNext = pStream;
		} else {
			pListener->AcceptHead = pStream;
		}
		pListener->AcceptTail = pStream;
		iQueued++;
		xrtAtomic32Store(
			&pListener->QueuedAccepts,
			iQueued,
			XMEMORY_RELEASE
		);
		__xrtNetStatFullPeak32(&pListener->PeakQueuedAccepts, iQueued);
		bQueued = true;
	}
	__xrtSpinUnlock(&pListener->AcceptLock);
	return bQueued;
}



/* 拉取模式下非阻塞取走一个已接受 Stream。 */
XRT_API xnetstream* xrtNetListenerAccept(xnetlistener* pListener)
{
	xnetstream* pStream = NULL;
	bool bWaiting = false;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pListener->Events.Accept != NULL ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_LISTENER_ACCEPT,
			"accept-listener",
			"TCP listener is configured for push accept callbacks"
		);
		return NULL;
	}
	__xrtSpinLock(&pListener->AcceptLock);
	if ( xrtAtomic32Load(
		&pListener->AcceptWaiters,
		XMEMORY_ACQUIRE
	) != 0 ) {
		bWaiting = true;
	} else if ( xrtNetListenerState(pListener) == XNET_LISTENER_OPEN ) {
		pStream = __xrtNetListenerTakeQueued(pListener);
	}
	__xrtSpinUnlock(&pListener->AcceptLock);
	if ( bWaiting ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_LISTENER_ACCEPT,
			"accept-listener",
			"TCP listener already has an asynchronous accept consumer"
		);
	}
	return pStream;
}



/* 关闭并释放全部尚未由拉取消费者领取的 Stream。 */
static void __xrtNetListenerDiscardQueued(xnetlistener* pListener)
{
	xnetstream* pHead;

	__xrtSpinLock(&pListener->AcceptLock);
	pHead = pListener->AcceptHead;
	pListener->AcceptHead = NULL;
	pListener->AcceptTail = NULL;
	xrtAtomic32Store(
		&pListener->QueuedAccepts,
		0,
		XMEMORY_RELEASE
	);
	__xrtSpinUnlock(&pListener->AcceptLock);
	while ( pHead != NULL ) {
		xnetstream* pNext = pHead->AcceptNext;

		pHead->AcceptNext = NULL;
		(void)xrtNetStreamAbort(pHead);
		xrtNetStreamDestroy(pHead);
		pHead = pNext;
	}
}



/* 原子占用发送预算并更新峰值。 */
static bool __xrtNetStreamReserveSend(xnetstream* pStream, size_t iSize)
{
	uint64 iQueued = xrtAtomic64Load(
		&pStream->QueuedBytes,
		XMEMORY_ACQUIRE
	);
	uint64 iLimit = (uint64)pStream->Config.WriteLimit;

	for ( ;; ) {
		uint64 iExpected = iQueued;
		uint64 iNext;

		if ( (uint64)iSize > (iLimit - iQueued) ) {
			__xrtNetStatBasicAdd(
				&pStream->SendRejected,
				1
			);
			return false;
		}
		iNext = iQueued + (uint64)iSize;
		if ( xrtAtomic64CompareExchange(
			&pStream->QueuedBytes,
			&iExpected,
			iNext,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			__xrtNetStatFullPeak64(&pStream->PeakQueuedBytes, iNext);
			return true;
		}
		iQueued = iExpected;
	}
}



static void __xrtNetStreamControlRequest(
	xnetstream* pStream,
	uint32 iRequest
);
static void __xrtNetStreamEndSend(xnetstream* pStream);



/* 发送门建立后，最后一个提交者或命令负责唤醒挂起的生命周期请求。 */
static void __xrtNetStreamWakeLifecycle(xnetstream* pStream)
{
	if ( xrtAtomic32Load(&pStream->WriteGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamControlRequest(
			pStream,
			XRT_NET_STREAM_CONTROL_FINISH
		);
	}
}



/* 进入跨线程发送提交区，阻止关闭过程越过已开始的提交。 */
static bool __xrtNetStreamBeginSend(xnetstream* pStream)
{
	bool bWorker = xrtNetWorkerIsCurrent(pStream->Worker);
	xnetstreamstate State;

	if ( !bWorker ) {
		(void)xrtAtomic32FetchAdd(
			&pStream->SendSubmitters,
			1,
			XMEMORY_ACQ_REL
		);
	}
	State = xrtNetStreamState(pStream);
	if ( ((State != XNET_STREAM_CONNECTING) &&
		  (State != XNET_STREAM_OPEN)) ||
		 xrtAtomic32Load(&pStream->WriteGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		if ( !bWorker ) {
			__xrtNetStreamEndSend(pStream);
		}
		return false;
	}
	return true;
}



/* 离开跨线程发送提交区。 */
static void __xrtNetStreamEndSend(xnetstream* pStream)
{
	if ( !xrtNetWorkerIsCurrent(pStream->Worker) ) {
		uint32 iPrevious = xrtAtomic32FetchSub(
			&pStream->SendSubmitters,
			1,
			XMEMORY_ACQ_REL
		);

		if ( iPrevious == 1 ) {
			__xrtNetStreamWakeLifecycle(pStream);
		}
	}
}



/* 在节点没有进入发送队列时归还预算。 */
static void __xrtNetStreamUnreserveSend(
	xnetstream* pStream,
	size_t iSize
)
{
	uint64 iPrevious = xrtAtomic64FetchSub(
		&pStream->QueuedBytes,
		(uint64)iSize,
		XMEMORY_ACQ_REL
	);
	uint64 iQueued = iPrevious >= iSize ?
		iPrevious - (uint64)iSize : 0;

	if ( (iQueued == 0) ||
		 (iQueued <= pStream->Config.WriteLowWater) ) {
		__xrtNetStreamNotifyFutures(pStream, true);
	}
}



/* 按真正离开发送缓冲的字节归还预算，事件在缓冲变更完成后发布。 */
static void __xrtNetStreamReleaseSendBudget(
	xnetstream* pStream,
	size_t iSize
)
{
	(void)xrtAtomic64FetchSub(
		&pStream->QueuedBytes,
		(uint64)iSize,
		XMEMORY_ACQ_REL
	);
}



/* 消费已经确认发送的前缀，并拒绝后端报告超过队列的字节数。 */
static bool __xrtNetStreamConsumeWrite(
	xnetstream* pStream,
	size_t iSize
)
{
	size_t iConsumed = xrtNetBufConsume(
		&pStream->WriteBuffer,
		iSize
	);

	if ( iConsumed != iSize ) {
		__xrtNetStreamSetError(
			XERR_INTERNAL,
			XNET_ERROR_STREAM_WRITE,
			"consume-stream-write",
			"network backend reported more bytes than were queued"
		);
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	__xrtNetStreamReleaseSendBudget(pStream, iConsumed);
	return true;
}



#if defined(XRT_FEATURE_NET_TCP_FILE)

/* 关闭文件发送节点持有的独立原生句柄。 */
static void __xrtNetStreamFileRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_net_stream_file* pFile =
		(__xrt_net_stream_file*)pContext;
	xnetstream* pStream = pFile->Stream;

	(void)pData;
	(void)iSize;
	#if defined(_WIN32) || defined(_WIN64)
		(void)CloseHandle((HANDLE)(uintptr_t)pFile->Handle);
	#else
		(void)close((int)pFile->Handle);
	#endif
	__xrtNetWorkerNodeRecycle(
		pStream->Worker,
		pFile,
		sizeof(*pFile)
	);
	xrtNetStreamDestroy(pStream);
}



/* 复制文件句柄，使受理后的操作不再借用公开文件对象。 */
static bool __xrtNetStreamFileDuplicate(
	xfile File,
	intptr_t* pHandle
)
{
	intptr_t iSource = xrtFileNative(File);

	if ( (iSource == (intptr_t)-1) ||
		((xrtFileFlags(File) & XFILE_READ) == 0) ) {
		if ( iSource != (intptr_t)-1 ) {
			__xrtNetStreamSetError(
				XERR_PERMISSION,
				XNET_ERROR_STREAM_WRITE,
				"send-stream-file",
				"file range send requires a readable file"
			);
		}
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		{
			HANDLE hCopy = INVALID_HANDLE_VALUE;

			if ( !DuplicateHandle(
				GetCurrentProcess(),
				(HANDLE)(uintptr_t)iSource,
				GetCurrentProcess(),
				&hCopy,
				0,
				FALSE,
				DUPLICATE_SAME_ACCESS
			) ) {
				__xrtNetSocketSetSystemError(
					XNET_ERROR_STREAM_WRITE,
					"send-stream-file",
					"duplicating file handle failed",
					(int)GetLastError()
				);
				return false;
			}
			*pHandle = (intptr_t)hCopy;
		}
	#else
		{
			int hCopy;

			#if defined(F_DUPFD_CLOEXEC)
				do {
					hCopy = fcntl((int)iSource, F_DUPFD_CLOEXEC, 0);
				} while ( (hCopy < 0) && (errno == EINTR) );
			#else
				do {
					hCopy = dup((int)iSource);
				} while ( (hCopy < 0) && (errno == EINTR) );
				if ( hCopy >= 0 ) {
					(void)fcntl(hCopy, F_SETFD, FD_CLOEXEC);
				}
			#endif
			if ( hCopy < 0 ) {
				__xrtNetSocketSetSystemError(
					XNET_ERROR_STREAM_WRITE,
					"send-stream-file",
					"duplicating file descriptor failed",
					errno
				);
				return false;
			}
			*pHandle = (intptr_t)hCopy;
		}
	#endif
	return true;
}



/* readiness 后端直接推进内核文件发送，绝不阻塞等待 Socket。 */
static xnetresult __xrtNetStreamSendFileReady(
	xnetstream* pStream,
	__xrt_net_stream_file* pFile,
	size_t iRelative,
	size_t iSize,
	size_t* pSent
)
{
	*pSent = 0;
	#if defined(__linux__)
		{
			off_t iOffset = (off_t)(pFile->Offset + (uint64)iRelative);
			ssize_t iResult;

			do {
				iResult = sendfile(
					(int)xrtNetSocketNative(pStream->Socket),
					(int)pFile->Handle,
					&iOffset,
					iSize
				);
			} while ( (iResult < 0) && (errno == EINTR) );
			if ( iResult > 0 ) {
				*pSent = (size_t)iResult;
				return XNET_RESULT_OK;
			}
			if ( (iResult < 0) &&
				((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
				return XNET_RESULT_AGAIN;
			}
			__xrtNetSocketSetSystemError(
				XNET_ERROR_STREAM_WRITE,
				"send-stream-file",
				iResult == 0 ?
					"file ended before the requested range" :
					"sendfile failed",
				iResult == 0 ? EIO : errno
			);
			return XNET_RESULT_ERROR;
		}
	#elif defined(__APPLE__)
		{
			off_t iDone = (off_t)iSize;
			off_t iOffset = (off_t)(pFile->Offset + (uint64)iRelative);
			int iResult;

			do {
				iResult = sendfile(
					(int)pFile->Handle,
					(int)xrtNetSocketNative(pStream->Socket),
					iOffset,
					&iDone,
					NULL,
					0
				);
			} while ( (iResult != 0) && (errno == EINTR) );
			if ( iDone > 0 ) {
				*pSent = (size_t)iDone;
				return XNET_RESULT_OK;
			}
			if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) ) {
				return XNET_RESULT_AGAIN;
			}
			__xrtNetSocketSetSystemError(
				XNET_ERROR_STREAM_WRITE,
				"send-stream-file",
				"sendfile failed",
				errno
			);
			return XNET_RESULT_ERROR;
		}
	#elif defined(__FreeBSD__)
		{
			off_t iDone = 0;
			int iResult = sendfile(
				(int)pFile->Handle,
				(int)xrtNetSocketNative(pStream->Socket),
				(off_t)(pFile->Offset + (uint64)iRelative),
				iSize,
				NULL,
				&iDone,
				0
			);

			if ( iDone > 0 ) {
				*pSent = (size_t)iDone;
				return XNET_RESULT_OK;
			}
			if ( (iResult != 0) &&
				((errno == EAGAIN) || (errno == EWOULDBLOCK)) ) {
				return XNET_RESULT_AGAIN;
			}
			__xrtNetSocketSetSystemError(
				XNET_ERROR_STREAM_WRITE,
				"send-stream-file",
				"sendfile failed",
				iResult == 0 ? EIO : errno
			);
			return XNET_RESULT_ERROR;
		}
	#else
		(void)pStream;
		(void)pFile;
		(void)iRelative;
		(void)iSize;
		__xrtNetStreamSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_STREAM_WRITE,
			"send-stream-file",
			"this platform has no kernel file send path"
		);
		return XNET_RESULT_ERROR;
	#endif
}

#endif



/* 写缓冲完成一次结构变更后发布稳定的低水位和排空事件。 */
static void __xrtNetStreamPublishWriteState(xnetstream* pStream)
{
	uint64 iQueued = xrtAtomic64Load(
		&pStream->QueuedBytes,
		XMEMORY_ACQUIRE
	);
	bool bNotify = false;

	if ( !pStream->AbortRequested && !xrtAtomic32Load(
		&pStream->AbortGate,
		XMEMORY_ACQUIRE
	) ) {
		if ( (iQueued <= pStream->Config.WriteLowWater) &&
			 (xrtAtomic32Exchange(
				&pStream->WriteBackpressured,
				0,
				XMEMORY_ACQ_REL
			 ) != 0) ) {
			bNotify = true;
			if ( pStream->Events.LowWater != NULL ) {
				pStream->Events.LowWater(
					pStream,
					(size_t)iQueued,
					__xrtNetStreamDataCurrent(pStream)
				);
			}
		}
		iQueued = xrtAtomic64Load(
			&pStream->QueuedBytes,
			XMEMORY_ACQUIRE
		);
		if ( !pStream->AbortRequested && !xrtAtomic32Load(
			&pStream->AbortGate,
			XMEMORY_ACQUIRE
		) && (iQueued == 0) && !pStream->WriteDrained ) {
			pStream->WriteDrained = true;
			bNotify = true;
			if ( pStream->Events.Drain != NULL ) {
				pStream->Events.Drain(
					pStream,
					__xrtNetStreamDataCurrent(pStream)
				);
			}
		}
		if ( bNotify ) {
			__xrtNetStreamNotifyFutures(pStream, true);
		}
	}
}



/* 发送节点离开队列时释放外部数据和 Stream 引用。 */
static void __xrtNetStreamSendRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_net_stream_send* pSend =
		(__xrt_net_stream_send*)pContext;
	xnetstream* pStream = pSend->Stream;
	xnetworker* pWorker = pStream->Worker;
	size_t iAllocation = pSend->Data == pSend->Copy ?
		offsetof(__xrt_net_stream_send, Copy) + pSend->Size :
		sizeof(*pSend);

	if ( pSend->OwnsExternal && (pSend->Release != NULL) ) {
		pSend->Release(pSend->ReleaseContext, pData, iSize);
	}
	__xrtNetWorkerNodeRecycle(pWorker, pSend, iAllocation);
	xrtNetStreamDestroy(pStream);
}



/* 一个批量引用块离队后执行原释放过程，并在最后一块回收批次。 */
static void __xrtNetStreamRefsRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_net_stream_ref_item* pItem =
		(__xrt_net_stream_ref_item*)pContext;
	__xrt_net_stream_refs* pBatch = pItem->Batch;
	xnetstream* pStream = pBatch->Stream;

	pItem->Release(pItem->Context, pData, iSize);
	pBatch->Remaining--;
	if ( pBatch->Remaining == 0 ) {
		xnetworker* pWorker = pStream->Worker;
		size_t iAllocation = offsetof(__xrt_net_stream_refs, Items) +
			(pBatch->Count * sizeof(__xrt_net_stream_ref_item));

		__xrtNetWorkerNodeRecycle(pWorker, pBatch, iAllocation);
		xrtNetStreamDestroy(pStream);
	}
}



/* 一个缓冲视图离队后在最后一个视图统一释放原块链。 */
static void __xrtNetStreamBufferRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	__xrt_net_stream_buffer_item* pItem =
		(__xrt_net_stream_buffer_item*)pContext;
	__xrt_net_stream_buffer* pBatch = pItem->Batch;
	xnetstream* pStream = pBatch->Stream;

	(void)pData;
	(void)iSize;
	pBatch->Remaining--;
	if ( pBatch->Remaining == 0 ) {
		xnetworker* pWorker = pStream->Worker;
		size_t iAllocation = offsetof(__xrt_net_stream_buffer, Items) +
			(pBatch->Count * sizeof(__xrt_net_stream_buffer_item));

		xrtNetBufClear(&pBatch->Owned);
		__xrtNetWorkerNodeRecycle(pWorker, pBatch, iAllocation);
		xrtNetStreamDestroy(pStream);
	}
}



/* 释放由 SendTake 接管的数据。 */
static void __xrtNetStreamFreeTaken(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



static void __xrtNetStreamDriveRead(xnetstream* pStream);
static void __xrtNetStreamDriveWrite(xnetstream* pStream);
static void __xrtNetStreamTryFinish(xnetstream* pStream);



/* 判断 Worker 端口是否使用完成式 IO。 */
static bool __xrtNetStreamCompletionPort(const xnetstream* pStream)
{
	return (xrtNetPortCapabilities(
		xrtNetWorkerPort(pStream->Worker)
	) & XNET_PORT_CAP_COMPLETION) != 0;
}



/* 判断完成式后端能否在不占用载荷缓冲的情况下等待可读。 */
static bool __xrtNetStreamReadProbeCapable(const xnetstream* pStream)
{
	return (xrtNetPortCapabilities(
		xrtNetWorkerPort(pStream->Worker)
	) & XNET_PORT_CAP_READ_PROBE) != 0;
}



/* 决定完成式接收后是否仍应携带载荷缓冲连续读取。 */
static bool __xrtNetStreamContinueDirectRead(
	xnetstream* pStream,
	size_t iReceived,
	size_t iCapacity
)
{
	size_t iAvailable;

	if ( pStream->Config.ReadMode == XNET_STREAM_READ_DIRECT ) {
		return true;
	}
	if ( (pStream->Config.ReadMode != XNET_STREAM_READ_ADAPTIVE) ||
		 (iReceived != iCapacity) ) {
		return false;
	}

	/* 查询失败只影响优化策略，保守保持直读且不覆盖公开错误。 */
	if ( !__xrtNetSocketAvailableNative(
		pStream->Socket,
		&iAvailable,
		NULL
	) ) {
		return true;
	}
	return iAvailable != 0;
}



/* 按当前读写需求重置 readiness one-shot 观察。 */
static bool __xrtNetStreamWatch(xnetstream* pStream)
{
	xnetstreamstate State = xrtNetStreamState(pStream);
	uint32 iEvents = 0;
	uint64 Id;

	if ( __xrtNetStreamCompletionPort(pStream) ) {
		return true;
	}
	if ( State == XNET_STREAM_CONNECTING ) {
		iEvents |= XNET_POLL_WRITE;
	} else {
		if ( (State == XNET_STREAM_OPEN) &&
			 !xrtAtomic32Load(
				&pStream->ReadPaused,
				XMEMORY_ACQUIRE
			 ) &&
			 !xrtAtomic32Load(
				&pStream->ReadEnded,
				XMEMORY_ACQUIRE
			 ) ) {
			iEvents |= XNET_POLL_READ;
		}
		if ( !xrtNetBufEmpty(&pStream->WriteBuffer) &&
			 !xrtAtomic32Load(
				&pStream->WriteEnded,
				XMEMORY_ACQUIRE
			 ) ) {
			iEvents |= XNET_POLL_WRITE;
		}
	}
	if ( iEvents == 0 ) {
		if ( pStream->WatchPending ) {
			if ( !xrtNetPortUnwatch(
				xrtNetWorkerPort(pStream->Worker),
				pStream->Socket
			) ) {
				pStream->WatchPending = false;
				pStream->WatchEvents = 0;
				return false;
			}
			pStream->WatchPending = false;
			pStream->WatchEvents = 0;
		}
		return true;
	}
	Id = xrtNetWorkerOperationId(pStream->Worker);
	if ( (Id == 0) || !xrtNetPortWatch(
		xrtNetWorkerPort(pStream->Worker),
		pStream->Socket,
		Id,
		iEvents,
		&pStream->Completion
	) ) {
		return false;
	}
	pStream->WatchPending = true;
	pStream->WatchEvents = iEvents;
	return true;
}



/* 取消 Stream 当前所有可取消的端口操作。 */
static void __xrtNetStreamCancelOperations(xnetstream* pStream)
{
	xnetport* pPort = xrtNetWorkerPort(pStream->Worker);

	if ( pStream->WatchPending ) {
		if ( !xrtNetPortUnwatch(pPort, pStream->Socket) ) {
			xrtClearError();
		}
		pStream->WatchPending = false;
		pStream->WatchEvents = 0;
	}
	if ( pStream->ConnectPending ) {
		if ( !xrtNetPortCancel(pPort, pStream->ConnectId) ) {
			xrtClearError();
		}
	}
	if ( pStream->ReadPending ) {
		if ( !xrtNetPortCancel(pPort, pStream->ReadId) ) {
			xrtClearError();
		}
	}
	if ( pStream->AbortRequested && pStream->WritePending ) {
		if ( !xrtNetPortCancel(pPort, pStream->WriteId) ) {
			xrtClearError();
		}
	}
}



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 摘除候选控制器后再释放其上下文，避免释放过程重入同一控制器。 */
static void __xrtNetStreamControlRelease(xnetstream* pStream)
{
	void (*pRelease)(ptr pData) = pStream->Control.Release;
	ptr pData = pStream->Control.Data;

	memset(&pStream->Control, 0, sizeof(pStream->Control));
	if ( pRelease != NULL ) {
		pRelease(pData);
	}
}
#endif



/* 取消连接阶段 Timer，并忽略不影响 Stream 终态的取消竞争。 */
static void __xrtNetStreamCancelConnectTimer(xnetstream* pStream)
{
	if ( pStream->ConnectTimer == 0 ) {
		return;
	}
	(void)__xrtNetEngineTimerCancelLifecycle(
		pStream->Engine,
		pStream->ConnectTimer
	);
	xrtClearError();
	pStream->ConnectTimer = 0;
}



/* 在没有借用 IO 缓冲后完成唯一 Close 回调。 */
static void __xrtNetStreamTryFinish(xnetstream* pStream)
{
	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		pStream->AbortRequested = true;
		pStream->CloseRequested = true;
		if ( pStream->CloseResult == XNET_RESULT_OK ) {
			pStream->CloseResult = XNET_RESULT_CANCELLED;
		}
	}
	if ( (xrtNetStreamState(pStream) != XNET_STREAM_CLOSING) ||
		 !pStream->CloseRequested ) {
		return;
	}
	if ( xrtAtomic32Load(&pStream->SendSubmitters, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->SendCommands, XMEMORY_ACQUIRE) ) {
		return;
	}
	__xrtNetStreamCancelConnectTimer(pStream);
	if ( pStream->StartPending ||
		 pStream->ConnectPending ||
		 pStream->ReadPending || pStream->WritePending ) {
		return;
	}
	if ( !pStream->AbortRequested &&
		 !xrtNetBufEmpty(&pStream->WriteBuffer) ) {
		__xrtNetStreamDriveWrite(pStream);
		return;
	}
	if ( pStream->Socket != NULL ) {
		if ( pStream->AbortRequested ) {
			(void)xrtNetSocketSet(
				pStream->Socket,
				XNET_OPTION_LINGER,
				0
			);
			xrtClearError();
		}
		(void)xrtNetSocketClose(pStream->Socket);
		pStream->Socket = NULL;
	}
	if ( pStream->BuffersReady ) {
		__xrtNetStreamReleaseSendBudget(
			pStream,
			xrtNetBufSize(&pStream->WriteBuffer)
		);
		xrtNetBufClear(&pStream->ReadBuffer);
		xrtNetBufClear(&pStream->WriteBuffer);
		xrtAtomic64Store(
			&pStream->BufferedBytes,
			0,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&pStream->ReadBlocked,
			0,
			XMEMORY_RELEASE
		);
		pStream->BuffersReady = false;
	}
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtSpinLock(&pStream->WaitLock);
		pStream->WaitClosed = true;
		__xrtSpinUnlock(&pStream->WaitLock);
	#endif
	__xrtNetStreamReleaseEngine(pStream);
	xrtAtomic32Store(
		&pStream->State,
		XNET_STREAM_CLOSED,
		XMEMORY_RELEASE
	);
	if ( pStream->Events.Close != NULL ) {
		pStream->Events.Close(
			pStream,
			pStream->CloseResult,
			pStream->Error,
			__xrtNetStreamDataCurrent(pStream)
		);
	}
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		if ( pStream->Control.Close != NULL ) {
			pStream->Control.Close(
				pStream,
				pStream->CloseResult,
				pStream->Error,
				pStream->Control.Data
			);
		}
		if ( pStream->Control.Release != NULL ) {
			__xrtNetStreamControlRelease(pStream);
		}
	#endif
	__xrtNetStreamNotifyFutures(pStream, false);
	if ( pStream->RuntimeHeld ) {
		pStream->RuntimeHeld = false;
		xrtNetStreamDestroy(pStream);
	}
}



/* 进入错误关闭并取消所有不再需要的 IO。 */
static void __xrtNetStreamFail(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSED ) {
		return;
	}
	if ( pStream->CloseResult == XNET_RESULT_OK ) {
		pStream->CloseResult = XNET_RESULT_ERROR;
	}
	xrtAtomic32Store(&pStream->AbortGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pStream->CloseGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pStream->WriteGate, 1, XMEMORY_RELEASE);
	pStream->CloseRequested = true;
	pStream->AbortRequested = true;
	xrtAtomic32Store(
		&pStream->State,
		XNET_STREAM_CLOSING,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pStream->ReadPaused,
		1,
		XMEMORY_RELEASE
	);
	__xrtNetStreamNotifyFutures(pStream, false);
	__xrtNetStreamCancelOperations(pStream);
	__xrtNetStreamTryFinish(pStream);
}



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 在所属 Worker 上立即终止 Stream，供无分配组合层使用。 */
void __xrtNetStreamFailCurrent(
	xnetstream* pStream,
	xnetresult Result
)
{
	if ( (pStream == NULL) ||
		 !xrtNetWorkerIsCurrent(pStream->Worker) ||
		 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
		return;
	}
	if ( (Result != XNET_RESULT_ERROR) &&
		 (Result != XNET_RESULT_CANCELLED) ) {
		Result = XNET_RESULT_ERROR;
	}
	xrtAtomic32Store(&pStream->AbortGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pStream->CloseGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pStream->WriteGate, 1, XMEMORY_RELEASE);
	pStream->CloseResult = Result;
	__xrtNetStreamFail(pStream);
}



/* 在所属 Worker 上立即拒绝一个尚未公开的托管候选。 */
void __xrtNetStreamReject(xnetstream* pStream)
{
	__xrtNetStreamFailCurrent(pStream, XNET_RESULT_CANCELLED);
}
#endif



/* 发布唯一的读端结束事件。 */
static void __xrtNetStreamEndRead(xnetstream* pStream)
{
	xrtAtomic32Store(
		&pStream->ReadEnded,
		1,
		XMEMORY_RELEASE
	);
	if ( !pStream->EndEmitted && (pStream->Events.End != NULL) ) {
		pStream->EndEmitted = true;
		pStream->Events.End(
			pStream,
			__xrtNetStreamDataCurrent(pStream)
		);
	}
	__xrtNetStreamNotifyFutures(pStream, false);
}



/* 提交接收字节并要求回调在硬边界内消费缓冲。 */
static bool __xrtNetStreamPublishRead(
	xnetstream* pStream,
	size_t iReceived
)
{
	if ( !xrtNetBufCommit(&pStream->ReadBuffer, iReceived) ) {
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
		return false;
	}
	__xrtNetStatFullAdd(
		&pStream->ReceivedBytes,
		(uint64)iReceived
	);
	__xrtNetStatFullAdd(
		&pStream->ReadEvents,
		1
	);
	xrtAtomic64Store(
		&pStream->BufferedBytes,
		(uint64)xrtNetBufSize(&pStream->ReadBuffer),
		XMEMORY_RELEASE
	);
	if ( pStream->Events.Read != NULL ) {
		pStream->Events.Read(
			pStream,
			&pStream->ReadBuffer,
			__xrtNetStreamDataCurrent(pStream)
		);
	}
	__xrtNetStreamReadRefresh(pStream, false);
	if ( pStream->Events.Read == NULL ) {
		__xrtNetStreamNotifyFutures(pStream, false);
		__xrtNetStreamReadRefresh(pStream, false);
	}
	return xrtNetStreamState(pStream) == XNET_STREAM_OPEN;
}



/* 发送队列排空后执行写半关闭或正常关闭。 */
static void __xrtNetStreamAfterDrain(xnetstream* pStream)
{
	if ( pStream->CloseRequested ) {
		__xrtNetStreamTryFinish(pStream);
		return;
	}
	if ( pStream->ShutdownRequested &&
		 !xrtAtomic32Load(
			&pStream->WriteEnded,
			XMEMORY_ACQUIRE
		 ) ) {
		if ( !xrtNetSocketShutdown(
			pStream->Socket,
			XNET_SHUTDOWN_WRITE
		) ) {
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
			return;
		}
		xrtAtomic32Store(
			&pStream->WriteEnded,
			1,
			XMEMORY_RELEASE
		);
		__xrtNetStreamNotifyFutures(pStream, false);
	}
}



/* 向 completion 后端提交一次直接接收。 */
static bool __xrtNetStreamSubmitRead(xnetstream* pStream)
{
	xnetwspan Span;
	size_t iBuffered = xrtNetBufSize(&pStream->ReadBuffer);
	size_t iRemaining = iBuffered < pStream->Config.ReadLimit ?
		pStream->Config.ReadLimit - iBuffered : 0;
	size_t iMinimum = pStream->Config.ReadSize < iRemaining ?
		pStream->Config.ReadSize : iRemaining;

	if ( iMinimum == 0 ) {
		return true;
	}
	if ( !xrtNetBufReserve(&pStream->ReadBuffer, iMinimum, &Span) ) {
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	if ( Span.Size > iRemaining ) {
		Span.Size = iRemaining;
	}
	pStream->ReadId = xrtNetWorkerOperationId(pStream->Worker);
	if ( (pStream->ReadId == 0) || !xrtNetPortRecv(
		xrtNetWorkerPort(pStream->Worker),
		pStream->Socket,
		Span.Data,
		Span.Size,
		pStream->ReadId,
		&pStream->Completion
	) ) {
		(void)xrtNetBufCancel(&pStream->ReadBuffer);
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	pStream->ReadPending = true;
	pStream->ReadCapacity = Span.Size;
	return true;
}



/* 向 completion 后端提交一次无载荷缓冲的可读探测。 */
static bool __xrtNetStreamSubmitReadProbe(xnetstream* pStream)
{
	pStream->ReadId = xrtNetWorkerOperationId(pStream->Worker);
	if ( (pStream->ReadId == 0) || !xrtNetPortReadProbe(
		xrtNetWorkerPort(pStream->Worker),
		pStream->Socket,
		pStream->ReadId,
		&pStream->Completion
	) ) {
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	pStream->ReadPending = true;
	pStream->ReadCapacity = 0;
	return true;
}



/* 在 readiness 后端同步排空可读数据，在 completion 后端预投递一次。 */
static void __xrtNetStreamDriveRead(xnetstream* pStream)
{
	if ( (xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->ReadPaused, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->ReadBlocked, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->ReadEnded, XMEMORY_ACQUIRE) ||
		 pStream->ReadPending ) {
		return;
	}
	if ( __xrtNetStreamCompletionPort(pStream) ) {
		bool bDirect =
			(pStream->Config.ReadMode == XNET_STREAM_READ_DIRECT) ||
			pStream->ReadDirect ||
			!__xrtNetStreamReadProbeCapable(pStream);

		if ( !(bDirect ?
			__xrtNetStreamSubmitRead(pStream) :
			__xrtNetStreamSubmitReadProbe(pStream)) ) {
			__xrtNetStreamFail(pStream);
		}
		return;
	}
	for ( uint32 i = 0; i < XRT_NET_STREAM_IO_BUDGET; i++ ) {
		xnetwspan Span;
		size_t iBuffered = xrtNetBufSize(&pStream->ReadBuffer);
		size_t iRemaining = iBuffered < pStream->Config.ReadLimit ?
			pStream->Config.ReadLimit - iBuffered : 0;
		size_t iMinimum = pStream->Config.ReadSize < iRemaining ?
			pStream->Config.ReadSize : iRemaining;
		size_t iReceived = 0;
		xnetresult Result;

		if ( iMinimum == 0 ) {
			return;
		}
		if ( !xrtNetBufReserve(&pStream->ReadBuffer, iMinimum, &Span) ) {
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
			return;
		}
		if ( Span.Size > iRemaining ) {
			Span.Size = iRemaining;
		}
		Result = xrtNetSocketRecv(
			pStream->Socket,
			Span.Data,
			Span.Size,
			&iReceived
		);
		if ( Result == XNET_RESULT_AGAIN ) {
			(void)xrtNetBufCancel(&pStream->ReadBuffer);
			if ( !__xrtNetStreamWatch(pStream) ) {
				__xrtNetStreamRememberError(pStream);
				__xrtNetStreamFail(pStream);
			}
			return;
		}
		if ( Result == XNET_RESULT_CLOSED ) {
			(void)xrtNetBufCancel(&pStream->ReadBuffer);
			__xrtNetStreamEndRead(pStream);
			return;
		}
		if ( Result != XNET_RESULT_OK ) {
			(void)xrtNetBufCancel(&pStream->ReadBuffer);
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
			return;
		}
		if ( !__xrtNetStreamPublishRead(pStream, iReceived) ) {
			return;
		}
		if ( (xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ||
			 xrtAtomic32Load(
				&pStream->CloseGate,
				XMEMORY_ACQUIRE
			 ) ||
			 xrtAtomic32Load(
				&pStream->ReadPaused,
				XMEMORY_ACQUIRE
			 ) ||
			 xrtAtomic32Load(
				&pStream->ReadBlocked,
				XMEMORY_ACQUIRE
			 ) ) {
			return;
		}
	}
	if ( !__xrtNetStreamWatch(pStream) ) {
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
	}
}



/* 刷新接收缓冲快照，并在低于硬上限后恢复自动读取。 */
void __xrtNetStreamReadRefresh(xnetstream* pStream, bool bDrive)
{
	size_t iBuffered = xrtNetBufSize(&pStream->ReadBuffer);

	xrtAtomic64Store(
		&pStream->BufferedBytes,
		(uint64)iBuffered,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pStream->ReadBlocked,
		iBuffered >= pStream->Config.ReadLimit ? 1 : 0,
		XMEMORY_RELEASE
	);
	if ( bDrive && (iBuffered < pStream->Config.ReadLimit) ) {
		__xrtNetStreamDriveRead(pStream);
	}
}



/* 在发送节点进入队列后更新高水位并启动写入。 */
static void __xrtNetStreamSendQueued(xnetstream* pStream)
{
	uint64 iQueued = xrtAtomic64Load(
		&pStream->QueuedBytes,
		XMEMORY_ACQUIRE
	);

	pStream->WriteDrained = false;
	if ( (iQueued >= pStream->Config.WriteHighWater) &&
		 (xrtAtomic32Exchange(
			&pStream->WriteBackpressured,
			1,
			XMEMORY_ACQ_REL
		 ) == 0) ) {
		if ( pStream->Events.HighWater != NULL ) {
			pStream->Events.HighWater(
				pStream,
				(size_t)iQueued,
				__xrtNetStreamDataCurrent(pStream)
			);
		}
	}
	__xrtNetStreamDriveWrite(pStream);
}



/* Worker 本地复制发送直接进入拥有型缓冲，避免发送节点和引用块。 */
static xnetresult __xrtNetStreamCopyCurrent(
	xnetstream* pStream,
	const xnetspan* pSpans,
	size_t iCount,
	size_t iTotal
)
{
	xnetbuf Temporary;
	bool bSingle = iCount == 1;

	if ( !__xrtNetStreamReserveSend(pStream, iTotal) ) {
		return XNET_RESULT_AGAIN;
	}
	if ( bSingle ) {
		if ( !xrtNetBufAppend(
			&pStream->WriteBuffer,
			pSpans[0].Data,
			pSpans[0].Size
		) ) {
			__xrtNetStreamUnreserveSend(pStream, iTotal);
			return XNET_RESULT_ERROR;
		}
	} else {
		if ( !xrtNetBufInit(
			&Temporary,
			pStream->WriteBuffer.Pool
		) ) {
			__xrtNetStreamUnreserveSend(pStream, iTotal);
			return XNET_RESULT_ERROR;
		}
		for ( size_t i = 0; i < iCount; i++ ) {
			if ( (pSpans[i].Size != 0) && !xrtNetBufAppend(
				&Temporary,
				pSpans[i].Data,
				pSpans[i].Size
			) ) {
				xrtNetBufClear(&Temporary);
				__xrtNetStreamUnreserveSend(pStream, iTotal);
				return XNET_RESULT_ERROR;
			}
		}
		__xrtNetBufMoveTrusted(
			&pStream->WriteBuffer,
			&Temporary
		);
	}
	__xrtNetStreamSendQueued(pStream);
	return XNET_RESULT_OK;
}



/* 在 completion 后端提交一次聚集发送。 */
static bool __xrtNetStreamSubmitWrite(
	xnetstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
)
{
	pStream->WriteId = xrtNetWorkerOperationId(pStream->Worker);
	if ( (pStream->WriteId == 0) || !xrtNetPortSendVec(
		xrtNetWorkerPort(pStream->Worker),
		pStream->Socket,
		pSpans,
		iCount,
		pStream->WriteId,
		&pStream->Completion
	) ) {
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	pStream->WritePending = true;
	return true;
}



#if defined(XRT_FEATURE_NET_TCP_FILE)

/* 在完成式后端提交文件节点尚未发送的前缀。 */
static bool __xrtNetStreamSubmitFile(
	xnetstream* pStream,
	__xrt_net_stream_file* pFile,
	size_t iRelative,
	size_t iSize
)
{
	size_t iChunk = iSize > (size_t)INT_MAX ?
		(size_t)INT_MAX : iSize;

	pStream->WriteId = xrtNetWorkerOperationId(pStream->Worker);
	if ( (pStream->WriteId == 0) || !__xrtNetPortSendFile(
		xrtNetWorkerPort(pStream->Worker),
		pStream->Socket,
		pFile->Handle,
		pFile->Offset + (uint64)iRelative,
		iChunk,
		pStream->WriteId,
		&pStream->Completion
	) ) {
		__xrtNetStreamRememberError(pStream);
		return false;
	}
	pStream->WritePending = true;
	return true;
}

#endif



/* 推进发送队列；每次系统调用只消费实际完成的前缀。 */
static void __xrtNetStreamDriveWrite(xnetstream* pStream)
{
	xnetstreamstate State = xrtNetStreamState(pStream);

	if ( pStream->WritePending ||
		 pStream->AbortRequested ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ||
		 ((State != XNET_STREAM_OPEN) &&
		  (State != XNET_STREAM_CLOSING)) ) {
		return;
	}
	if ( xrtNetBufEmpty(&pStream->WriteBuffer) ) {
		__xrtNetStreamAfterDrain(pStream);
		return;
	}
	if ( xrtAtomic32Load(&pStream->WriteEnded, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamSetError(
			XERR_CLOSED,
			XNET_ERROR_STREAM_WRITE,
			"send-stream",
			"TCP write side is closed"
		);
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
		return;
	}
	for ( uint32 i = 0; i < XRT_NET_STREAM_IO_BUDGET; i++ ) {
		#if defined(XRT_FEATURE_NET_TCP_FILE)
			ptr pDescriptor = NULL;
			size_t iRelative = 0;
			size_t iFileSize = 0;

			if ( __xrtNetBufFileFront(
				&pStream->WriteBuffer,
				&pDescriptor,
				&iRelative,
				&iFileSize
			) ) {
				__xrt_net_stream_file* pFile =
					(__xrt_net_stream_file*)pDescriptor;

				if ( __xrtNetStreamCompletionPort(pStream) ) {
					if ( !__xrtNetStreamSubmitFile(
						pStream,
						pFile,
						iRelative,
						iFileSize
					) ) {
						__xrtNetStreamFail(pStream);
					}
					return;
				} else {
					size_t iSent = 0;
					xnetresult Result = __xrtNetStreamSendFileReady(
						pStream,
						pFile,
						iRelative,
						iFileSize,
						&iSent
					);

					if ( Result == XNET_RESULT_AGAIN ) {
						if ( !__xrtNetStreamWatch(pStream) ) {
							__xrtNetStreamRememberError(pStream);
							__xrtNetStreamFail(pStream);
						}
						return;
					}
					if ( Result != XNET_RESULT_OK ) {
						__xrtNetStreamRememberError(pStream);
						__xrtNetStreamFail(pStream);
						return;
					}
					if ( !__xrtNetStreamConsumeWrite(pStream, iSent) ) {
						__xrtNetStreamFail(pStream);
						return;
					}
					__xrtNetStatFullAdd(
						&pStream->SentBytes,
						(uint64)iSent
					);
					__xrtNetStatFullAdd(
						&pStream->WriteEvents,
						1
					);
					__xrtNetStreamPublishWriteState(pStream);
					continue;
				}
			}
		#endif
		xnetspan Spans[XRT_NET_SOCKET_VECTOR_LIMIT];
		size_t iCount = xrtNetBufSpans(
			&pStream->WriteBuffer,
			Spans,
			XRT_NET_SOCKET_VECTOR_LIMIT
		);

		if ( iCount == 0 ) {
			__xrtNetStreamAfterDrain(pStream);
			return;
		}
		if ( __xrtNetStreamCompletionPort(pStream) ) {
			if ( !__xrtNetStreamSubmitWrite(pStream, Spans, iCount) ) {
				__xrtNetStreamFail(pStream);
			}
			return;
		} else {
			size_t iSent = 0;
			xnetresult Result = xrtNetSocketSendVec(
				pStream->Socket,
				Spans,
				iCount,
				&iSent
			);

			if ( Result == XNET_RESULT_AGAIN ) {
				if ( !__xrtNetStreamWatch(pStream) ) {
					__xrtNetStreamRememberError(pStream);
					__xrtNetStreamFail(pStream);
				}
				return;
			}
			if ( Result != XNET_RESULT_OK ) {
				__xrtNetStreamRememberError(pStream);
				__xrtNetStreamFail(pStream);
				return;
			}
			if ( !__xrtNetStreamConsumeWrite(pStream, iSent) ) {
				__xrtNetStreamFail(pStream);
				return;
			}
			__xrtNetStatFullAdd(
				&pStream->SentBytes,
				(uint64)iSent
			);
			__xrtNetStatFullAdd(
				&pStream->WriteEvents,
				1
			);
			__xrtNetStreamPublishWriteState(pStream);
			if ( pStream->AbortRequested || xrtAtomic32Load(
				&pStream->AbortGate,
				XMEMORY_ACQUIRE
			) ) {
				return;
			}
		}
	}
	if ( pStream->AbortRequested || xrtAtomic32Load(
		&pStream->AbortGate,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( !__xrtNetStreamWatch(pStream) ) {
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
	}
}



/* 完成连接后发布地址和 Open，再启动双向 IO。 */
static void __xrtNetStreamOpened(xnetstream* pStream)
{
	__xrtNetStreamCancelConnectTimer(pStream);
	if ( !xrtNetSocketLocal(pStream->Socket, &pStream->Local) ||
		 !xrtNetSocketRemote(pStream->Socket, &pStream->Remote) ) {
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
		return;
	}
	if ( xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ) {
		return;
	}
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		if ( pStream->Control.Open != NULL ) {
			bool bPublish = pStream->Control.Open(
				pStream,
				pStream->Control.Data
			);

			if ( !bPublish ) {
				__xrtNetStreamReject(pStream);
				return;
			}
			if ( xrtAtomic32Load(
				&pStream->CloseGate,
				XMEMORY_ACQUIRE
			) ) {
				return;
			}
		}
	#endif
	xrtAtomic32Store(
		&pStream->State,
		XNET_STREAM_OPEN,
		XMEMORY_RELEASE
	);
	if ( !pStream->OpenEmitted ) {
		pStream->OpenEmitted = true;
		if ( pStream->Events.Open != NULL ) {
			pStream->Events.Open(
				pStream,
				__xrtNetStreamDataCurrent(pStream)
			);
		}
	}
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		if ( pStream->Control.Published != NULL ) {
			pStream->Control.Published(
				pStream,
				pStream->Control.Data
			);
		}
		if ( pStream->Control.Release != NULL ) {
			__xrtNetStreamControlRelease(pStream);
		}
	#endif
	__xrtNetStreamNotifyFutures(pStream, true);
	if ( xrtNetStreamState(pStream) == XNET_STREAM_OPEN ) {
		/* 写失败可以同步进入终态，短暂引用保护后续状态判断。 */
		(void)xrtNetStreamRef(pStream);
		__xrtNetStreamDriveWrite(pStream);
		if ( xrtNetStreamState(pStream) == XNET_STREAM_OPEN ) {
			__xrtNetStreamDriveRead(pStream);
		}
		xrtNetStreamDestroy(pStream);
	}
}



/* 处理 completion 接收终态。 */
static void __xrtNetStreamReadComplete(
	xnetstream* pStream,
	const xnetportevent* pEvent
)
{
	size_t iCapacity = pStream->ReadCapacity;

	pStream->ReadPending = false;
	pStream->ReadCapacity = 0;

	/*
	 * 完成式后端允许取消与正常完成竞争。关闭已建立后，无论完成包是
	 * CANCELLED、EOF 还是成功字节，都应丢弃借用缓冲并继续收敛终态。
	 */
	if ( pStream->CloseRequested || xrtAtomic32Load(
		&pStream->CloseGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pStream->AbortGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetBufCancel(&pStream->ReadBuffer);
		__xrtNetStreamTryFinish(pStream);
		return;
	}
	if ( (pEvent->Result == XNET_RESULT_CLOSED) ||
		 ((pEvent->Result == XNET_RESULT_OK) && (pEvent->Bytes == 0)) ||
		 ((pEvent->Flags & XNET_PORT_EVENT_EOF) != 0) ) {
		(void)xrtNetBufCancel(&pStream->ReadBuffer);
		__xrtNetStreamEndRead(pStream);
		return;
	}
	if ( pEvent->Result != XNET_RESULT_OK ) {
		(void)xrtNetBufCancel(&pStream->ReadBuffer);
		__xrtNetStreamEventError(
			pStream,
			pEvent,
			XNET_ERROR_STREAM_READ,
			"receive-stream",
			"TCP receive failed"
		);
		__xrtNetStreamFail(pStream);
		return;
	}
	if ( !__xrtNetStreamPublishRead(pStream, pEvent->Bytes) ) {
		return;
	}
	pStream->ReadDirect = __xrtNetStreamContinueDirectRead(
		pStream,
		pEvent->Bytes,
		iCapacity
	);
	__xrtNetStreamDriveRead(pStream);
}



/* 可读探测完成后才分配真实接收块。 */
static void __xrtNetStreamReadProbeComplete(
	xnetstream* pStream,
	const xnetportevent* pEvent
)
{
	pStream->ReadPending = false;
	pStream->ReadCapacity = 0;

	if ( pStream->CloseRequested || xrtAtomic32Load(
		&pStream->CloseGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pStream->AbortGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtNetStreamTryFinish(pStream);
		return;
	}
	if ( pEvent->Result != XNET_RESULT_OK ) {
		__xrtNetStreamEventError(
			pStream,
			pEvent,
			XNET_ERROR_STREAM_READ,
			"probe-stream",
			"TCP readability probe failed"
		);
		__xrtNetStreamFail(pStream);
		return;
	}
	pStream->ReadDirect = true;
	__xrtNetStreamDriveRead(pStream);
}



/* 处理 completion 发送终态。 */
static void __xrtNetStreamWriteComplete(
	xnetstream* pStream,
	const xnetportevent* pEvent
)
{
	pStream->WritePending = false;

	/* 异常关闭不再发送取消竞争中已经完成的剩余队列。 */
	if ( pStream->AbortRequested || xrtAtomic32Load(
		&pStream->AbortGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtNetStreamTryFinish(pStream);
		return;
	}
	if ( pEvent->Result != XNET_RESULT_OK ) {
		__xrtNetStreamEventError(
			pStream,
			pEvent,
			XNET_ERROR_STREAM_WRITE,
			"send-stream",
			"TCP send failed"
		);
		__xrtNetStreamFail(pStream);
		return;
	}
	if ( pEvent->Bytes == 0 ) {
		__xrtNetStreamSetError(
			XERR_CLOSED,
			XNET_ERROR_STREAM_WRITE,
			"send-stream",
			"TCP send completed without progress"
		);
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
		return;
	}
	if ( !__xrtNetStreamConsumeWrite(pStream, pEvent->Bytes) ) {
		__xrtNetStreamFail(pStream);
		return;
	}
	__xrtNetStatFullAdd(
		&pStream->SentBytes,
		(uint64)pEvent->Bytes
	);
	__xrtNetStatFullAdd(
		&pStream->WriteEvents,
		1
	);
	__xrtNetStreamPublishWriteState(pStream);
	__xrtNetStreamDriveWrite(pStream);
}



/* 分发 Stream 的 connect、read、write 和 readiness 事件。 */
static void __xrtNetStreamCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	xnetstream* pStream = (xnetstream*)pData;

	(void)pWorker;
	/* 用户回调可以重入关闭 Stream，完成分发必须独立保活到返回。 */
	if ( xrtNetStreamRef(pStream) == NULL ) {
		xrtClearError();
		return;
	}
	if ( pEvent->Type == XNET_PORT_EVENT_CONNECT ) {
		pStream->ConnectPending = false;

		/* 关闭请求优先于与取消竞争的任意连接终态。 */
		if ( pStream->CloseRequested || xrtAtomic32Load(
			&pStream->CloseGate,
			XMEMORY_ACQUIRE
		) || xrtAtomic32Load(
			&pStream->AbortGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtNetStreamTryFinish(pStream);
		} else if ( pEvent->Result == XNET_RESULT_OK ) {
			__xrtNetStreamOpened(pStream);
		} else {
			__xrtNetStreamEventError(
				pStream,
				pEvent,
				XNET_ERROR_STREAM_CONNECT,
				"connect-stream",
				"TCP connect failed"
			);
			__xrtNetStreamFail(pStream);
		}
	} else if ( pEvent->Type == XNET_PORT_EVENT_READ_PROBE ) {
		__xrtNetStreamReadProbeComplete(pStream, pEvent);
	} else if ( pEvent->Type == XNET_PORT_EVENT_RECV ) {
		__xrtNetStreamReadComplete(pStream, pEvent);
	} else if ( (pEvent->Type == XNET_PORT_EVENT_SEND) ||
		(pEvent->Type == XNET_PORT_EVENT_SEND_FILE) ) {
		__xrtNetStreamWriteComplete(pStream, pEvent);
	} else if ( pEvent->Type == XNET_PORT_EVENT_READY ) {
		bool bDriven = false;

		pStream->WatchPending = false;
		pStream->WatchEvents = 0;
		if ( xrtNetStreamState(pStream) == XNET_STREAM_CONNECTING ) {
			xnetresult Result = xrtNetSocketFinishConnect(
				pStream->Socket
			);

			if ( Result == XNET_RESULT_OK ) {
				__xrtNetStreamOpened(pStream);
			} else if ( Result == XNET_RESULT_AGAIN ) {
				if ( !__xrtNetStreamWatch(pStream) ) {
					__xrtNetStreamRememberError(pStream);
					__xrtNetStreamFail(pStream);
				}
			} else {
				__xrtNetStreamRememberError(pStream);
				__xrtNetStreamFail(pStream);
			}
			goto Finish;
		}
		if ( (pEvent->Flags & XNET_PORT_EVENT_READ) != 0 ) {
			bDriven = true;
			__xrtNetStreamDriveRead(pStream);
		}
		if ( (pEvent->Flags & XNET_PORT_EVENT_WRITE) != 0 ) {
			bDriven = true;
			__xrtNetStreamDriveWrite(pStream);
		}
		/*
			readiness 后端可能把最后一批数据与错误同时上报。先让
			recv/send 取得数据或具体系统错误，只有纯错误事件才兜底终止。
		*/
		if ( !bDriven &&
			 (pEvent->Flags & XNET_PORT_EVENT_ERROR) != 0 &&
			 (xrtNetStreamState(pStream) < XNET_STREAM_CLOSING) ) {
			__xrtNetStreamEventError(
				pStream,
				pEvent,
				XNET_ERROR_STREAM_READ,
				"poll-stream",
				"TCP readiness backend reported a socket error"
			);
			__xrtNetStreamFail(pStream);
		}
	}

Finish:
	xrtNetStreamDestroy(pStream);
}



/* 连接 Timer 只在真正到期且 Stream 仍连接中时终止连接。 */
static void __xrtNetStreamConnectTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xnetstream* pStream = (xnetstream*)pData;

	(void)pWorker;
	if ( pStream->ConnectTimer == Id ) {
		pStream->ConnectTimer = 0;
	}
	if ( (Result == XNET_RESULT_OK) &&
		 (xrtNetStreamState(pStream) == XNET_STREAM_CONNECTING) ) {
		__xrtNetStreamSetError(
			XERR_TIMEOUT,
			XNET_ERROR_STREAM_CONNECT,
			"connect-stream",
			"TCP connect timed out"
		);
		__xrtNetStreamRememberError(pStream);
		pStream->CloseResult = XNET_RESULT_TIMEOUT;
		__xrtNetStreamFail(pStream);
	}
	xrtNetStreamDestroy(pStream);
}



/* 分配一个持有 Engine 生命周期的 Stream。 */
static xnetstream* __xrtNetStreamCreate(
	xnetengine* pEngine,
	xnetworker* pWorker,
	xnetsocket Socket,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pEvents,
	ptr pData
)
{
	xnetstream* pStream;

	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		return NULL;
	}
	pStream = (xnetstream*)xrtCalloc(1, sizeof(*pStream));
	if ( pStream == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	pStream->References = 2;
	xrtAtomic32Init(&pStream->State, XNET_STREAM_CONNECTING);
	xrtAtomic32Init(&pStream->ReadPaused, 0);
	xrtAtomic32Init(&pStream->ControlRequests, 0);
	xrtAtomic32Init(&pStream->ReadEnded, 0);
	xrtAtomic32Init(&pStream->WriteEnded, 0);
	xrtAtomic32Init(&pStream->WriteGate, 0);
	xrtAtomic32Init(&pStream->CloseGate, 0);
	xrtAtomic32Init(&pStream->AbortGate, 0);
	xrtAtomic32Init(&pStream->SendSubmitters, 0);
	xrtAtomic32Init(&pStream->SendCommands, 0);
	xrtAtomic64Init(&pStream->QueuedBytes, 0);
	xrtAtomic64Init(&pStream->PeakQueuedBytes, 0);
	xrtAtomic64Init(&pStream->ReceivedBytes, 0);
	xrtAtomic64Init(&pStream->SentBytes, 0);
	xrtAtomic64Init(&pStream->ReadEvents, 0);
	xrtAtomic64Init(&pStream->WriteEvents, 0);
	xrtAtomic64Init(&pStream->SendRejected, 0);
	xrtAtomic64Init(&pStream->BufferedBytes, 0);
	xrtAtomic32Init(&pStream->ReadBlocked, 0);
	xrtAtomic32Init(&pStream->WriteBackpressured, 0);
	xrtAtomicPtrInit(&pStream->Data, pData);
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtSpinInit(&pStream->WaitLock);
		pStream->ReadPush =
			(pEvents != NULL) && (pEvents->Read != NULL);
	#endif
	pStream->Engine = pEngine;
	pStream->Worker = pWorker;
	pStream->Socket = Socket;
	pStream->Config = *pConfig;
	if ( pEvents != NULL ) {
		pStream->Events = *pEvents;
	}
	pStream->CloseResult = XNET_RESULT_OK;
	pStream->WriteDrained = true;
	pStream->EngineHeld = true;
	pStream->RuntimeHeld = true;
	xrtNetCompletionInit(
		&pStream->Completion,
		__xrtNetStreamCompletion,
		pStream
	);
	return pStream;
}



/* 在所属 Worker 上初始化缓冲并发起连接。 */
static void __xrtNetStreamStartConnect(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetstream* pStream = (xnetstream*)pData;
	xnetport* pPort = xrtNetWorkerPort(pWorker);

	pStream->StartPending = false;
	if ( (xrtNetStreamState(pStream) != XNET_STREAM_CONNECTING) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamTryFinish(pStream);
		return;
	}
	if ( !xrtNetBufInit(
		&pStream->ReadBuffer,
		xrtNetWorkerBufPool(pWorker)
	) || !xrtNetBufInit(
		&pStream->WriteBuffer,
		xrtNetWorkerBufPool(pWorker)
	) ) {
		__xrtNetStreamRememberError(pStream);
		__xrtNetStreamFail(pStream);
		return;
	}
	pStream->BuffersReady = true;
	if ( pStream->Config.ConnectTimeout != 0 ) {
		xrtNetStreamRef(pStream);
		pStream->ConnectTimer = xrtNetEngineAfter(
			pStream->Engine,
			xrtNetWorkerIndex(pWorker),
			pStream->Config.ConnectTimeout,
			__xrtNetStreamConnectTimer,
			pStream
		);
		if ( pStream->ConnectTimer == 0 ) {
			xrtNetStreamDestroy(pStream);
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
			return;
		}
	}
	if ( (xrtNetPortCapabilities(pPort) & XNET_PORT_CAP_COMPLETION) != 0 ) {
		pStream->ConnectId = xrtNetWorkerOperationId(pWorker);
		if ( (pStream->ConnectId == 0) || !xrtNetPortConnect(
			pPort,
			pStream->Socket,
			&pStream->Remote,
			pStream->ConnectId,
			&pStream->Completion
		) ) {
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
			return;
		}
		pStream->ConnectPending = true;
	} else {
		xnetresult Result = xrtNetSocketConnect(
			pStream->Socket,
			&pStream->Remote
		);

		if ( Result == XNET_RESULT_OK ) {
			__xrtNetStreamOpened(pStream);
		} else if ( Result == XNET_RESULT_AGAIN ) {
			if ( !__xrtNetStreamWatch(pStream) ) {
				__xrtNetStreamRememberError(pStream);
				__xrtNetStreamFail(pStream);
			}
		} else {
			__xrtNetStreamRememberError(pStream);
			__xrtNetStreamFail(pStream);
		}
	}
}



/* 应用 TCP Stream 常用 Socket 选项。 */
static bool __xrtNetStreamSocketOptions(
	xnetsocket Socket,
	const xnetstreamconfig* pConfig
)
{
	if ( pConfig->NoDelay && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_NO_DELAY,
		1
	) ) {
		return false;
	}
	if ( pConfig->KeepAlive && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_KEEP_ALIVE,
		1
	) ) {
		return false;
	}
	return true;
}



/* 创建非阻塞 Socket 并把连接命令提交给目标 Worker。 */
static xnetstream* __xrtNetStreamConnectCreate(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pEvents,
	ptr pData,
	const void* pControl,
	bool bDeferred
)
{
	xnetstreamconfig Config;
	xnetworker* pWorker;
	xnetsocket Socket;
	xnetstream* pStream;
	xerror* pError;

	if ( (pEngine == NULL) || (pRemote == NULL) ||
		 ((pRemote->Family != XNET_FAMILY_IPV4) &&
		  (pRemote->Family != XNET_FAMILY_IPV6)) ) {
		__xrtNetStreamSetError(
			XERR_ARGUMENT,
			XNET_ERROR_STREAM_CREATE,
			"connect-stream",
			"invalid TCP connect arguments"
		);
		return NULL;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		__xrtNetStreamSetError(
			XERR_CLOSED,
			XNET_ERROR_STREAM_CREATE,
			"connect-stream",
			"TCP connect requires a running engine"
		);
		return NULL;
	}
	xrtNetStreamConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetStreamConfigValid(&Config) ) {
		return NULL;
	}
	pWorker = xrtNetEngineWorker(
		pEngine,
		(uint32)(iAffinity % xrtNetEngineWorkerCount(pEngine))
	);
	if ( pWorker == NULL ) {
		return NULL;
	}
	Socket = xrtNetSocketOpen(
		(xnetfamily)pRemote->Family,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	if ( Socket == NULL ) {
		return NULL;
	}
	if ( !__xrtNetStreamSocketOptions(Socket, &Config) ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	pStream = __xrtNetStreamCreate(
		pEngine,
		pWorker,
		Socket,
		&Config,
		pEvents,
		pData
	);
	if ( pStream == NULL ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	pStream->Remote = *pRemote;
	#if defined(XRT_FEATURE_NET_TCP_DIAL)
		if ( pControl != NULL ) {
			pStream->Control =
				*(const __xrt_net_stream_control*)pControl;
		}
	#else
		(void)pControl;
	#endif
	if ( bDeferred ) {
		return pStream;
	}
	pStream->StartPending = true;
	if ( !xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(pWorker),
		__xrtNetStreamStartConnect,
		pStream
	) ) {
		pError = xrtTakeError();
		pStream->StartPending = false;
		pStream->Socket = NULL;
		(void)xrtNetSocketClose(Socket);
		xrtAtomic32Store(
			&pStream->State,
			XNET_STREAM_CLOSED,
			XMEMORY_RELEASE
		);
		pStream->RuntimeHeld = false;
		xrtNetStreamDestroy(pStream);
		xrtNetStreamDestroy(pStream);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return pStream;
}



/* 创建普通数字地址 Stream，不附加托管候选控制器。 */
XRT_API xnetstream* xrtNetStreamConnect(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pEvents,
	ptr pData
)
{
	return __xrtNetStreamConnectCreate(
		pEngine,
		pRemote,
		iAffinity,
		pConfig,
		pEvents,
		pData,
		NULL,
		false
	);
}



#if defined(XRT_FEATURE_NET_TCP_DIAL)
/* 创建一个由 Dial 在公开 Open 前选择、尚未启动的数字地址候选。 */
xnetstream* __xrtNetStreamCreateControlled(
	xnetengine* pEngine,
	const xnetaddr* pRemote,
	uint64 iAffinity,
	const xnetstreamconfig* pConfig,
	const __xrt_net_stream_control* pControl
)
{
	if ( (pControl == NULL) || (pControl->Open == NULL) ||
		 (pControl->Published == NULL) ||
		 (pControl->Close == NULL) || (pControl->Release == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return __xrtNetStreamConnectCreate(
		pEngine,
		pRemote,
		iAffinity,
		pConfig,
		NULL,
		NULL,
		pControl,
		true
	);
}



/* 在候选所属 Worker 上直接启动，避免关闭越过尚未执行的启动命令。 */
void __xrtNetStreamStartControlled(xnetstream* pStream)
{
	if ( (pStream == NULL) ||
		 !xrtNetWorkerIsCurrent(pStream->Worker) ||
		 (xrtNetStreamState(pStream) != XNET_STREAM_CONNECTING) ||
		 (pStream->Control.Open == NULL) ) {
		return;
	}
	__xrtNetStreamStartConnect(pStream->Worker, pStream);
}



/* 把获胜候选切换到最终用户事件；只能在控制器 Open 回调内调用。 */
bool __xrtNetStreamAdopt(
	xnetstream* pStream,
	const xnetstreamevents* pEvents,
	ptr pData
)
{
	if ( (pStream == NULL) ||
		 !xrtNetWorkerIsCurrent(pStream->Worker) ||
		 (xrtNetStreamState(pStream) != XNET_STREAM_CONNECTING) ||
		 (pStream->Control.Open == NULL) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	memset(&pStream->Events, 0, sizeof(pStream->Events));
	if ( pEvents != NULL ) {
		pStream->Events = *pEvents;
	}
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		pStream->ReadPush = pStream->Events.Read != NULL;
	#endif
	xrtAtomicPtrStore(&pStream->Data, pData, XMEMORY_RELEASE);
	return true;
}

#endif



/*
	在所属 Worker 上切换 Stream 事件和数据。
	已接受但尚未发布 Open 的 Stream 也允许安装自己的协议处理器。
*/
XRT_API bool xrtNetStreamSetEvents(
	xnetstream* pStream,
	const xnetstreamevents* pEvents,
	ptr pData
)
{
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		bool bReadPush = (pEvents != NULL) &&
			(pEvents->Read != NULL);
	#endif

	if ( (pStream == NULL) ||
		!xrtNetWorkerIsCurrent(pStream->Worker) ||
		(xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ) {
		__xrtNetStreamSetError(
			pStream == NULL ? XERR_ARGUMENT : XERR_STATE,
			XNET_ERROR_STREAM_CONFIG,
			"set-stream-events",
			"stream events require an open stream on its worker"
		);
		return false;
	}
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtSpinLock(&pStream->WaitLock);
		if ( bReadPush && (pStream->ReadWaiters != 0) ) {
			__xrtSpinUnlock(&pStream->WaitLock);
			__xrtNetStreamSetError(
				XERR_STATE,
				XNET_ERROR_STREAM_READ,
				"set-stream-events",
				"TCP read callback conflicts with pending read Futures"
			);
			return false;
		}
	#endif
	memset(&pStream->Events, 0, sizeof(pStream->Events));
	if ( pEvents != NULL ) {
		pStream->Events = *pEvents;
	}
	xrtAtomicPtrStore(&pStream->Data, pData, XMEMORY_RELEASE);
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		pStream->ReadPush = bReadPush;
		__xrtSpinUnlock(&pStream->WaitLock);
	#endif
	return true;
}



/* 丢弃尚未进入缓冲链的发送节点。 */
static void __xrtNetStreamDiscardSend(
	__xrt_net_stream_send* pSend,
	bool bReleaseExternal
)
{
	xnetstream* pStream = pSend->Stream;

	if ( bReleaseExternal && pSend->OwnsExternal &&
		 (pSend->Release != NULL) ) {
		pSend->Release(
			pSend->ReleaseContext,
			pSend->Data,
			pSend->Size
		);
	}
	__xrtNetStreamUnreserveSend(pStream, pSend->Size);
	{
		xnetworker* pWorker = pStream->Worker;
		size_t iAllocation = pSend->Data == pSend->Copy ?
			offsetof(__xrt_net_stream_send, Copy) + pSend->Size :
			sizeof(*pSend);

		__xrtNetWorkerNodeRecycle(pWorker, pSend, iAllocation);
	}
	xrtNetStreamDestroy(pStream);
}



/* 丢弃尚未进入缓冲链的引用批次，并按受理状态处理外部所有权。 */
static void __xrtNetStreamDiscardRefs(
	__xrt_net_stream_refs* pBatch,
	bool bReleaseExternal
)
{
	xnetstream* pStream = pBatch->Stream;

	if ( bReleaseExternal ) {
		for ( size_t i = 0; i < pBatch->Count; i++ ) {
			pBatch->Items[i].Release(
				pBatch->Items[i].Context,
				pBatch->Items[i].Data,
				pBatch->Items[i].Size
			);
		}
	}
	__xrtNetStreamUnreserveSend(pStream, pBatch->Total);
	__xrtNetWorkerNodeRecycle(
		pStream->Worker,
		pBatch,
		offsetof(__xrt_net_stream_refs, Items) +
			(pBatch->Count * sizeof(__xrt_net_stream_ref_item))
	);
	xrtNetStreamDestroy(pStream);
}



/* 把发送节点链接到所属 Worker 的发送缓冲。 */
static bool __xrtNetStreamAttachSend(
	__xrt_net_stream_send* pSend
)
{
	xnetstream* pStream = pSend->Stream;

	if ( pStream->AbortRequested ||
		 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
		return false;
	}
	if ( !xrtNetBufAppendRef(
		&pStream->WriteBuffer,
		pSend->Data,
		pSend->Size,
		__xrtNetStreamSendRelease,
		pSend
	) ) {
		return false;
	}
	__xrtNetStreamSendQueued(pStream);
	return true;
}



/* 以临时链原子建立全部引用块，再一次移动到 Worker 发送缓冲。 */
static bool __xrtNetStreamAttachRefs(__xrt_net_stream_refs* pBatch)
{
	xnetstream* pStream = pBatch->Stream;
	xnetbuf Temporary;
	xnetblock* pBlock;

	if ( pStream->AbortRequested ||
		 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
		return false;
	}
	if ( !xrtNetBufInit(&Temporary, pStream->WriteBuffer.Pool) ) {
		return false;
	}
	for ( size_t i = 0; i < pBatch->Count; i++ ) {
		if ( !xrtNetBufAppendBorrow(
			&Temporary,
			pBatch->Items[i].Data,
			pBatch->Items[i].Size
		) ) {
			xrtNetBufClear(&Temporary);
			return false;
		}
	}
	pBlock = Temporary.Head;
	for ( size_t i = 0; i < pBatch->Count; i++ ) {
		pBlock->Release = __xrtNetStreamRefsRelease;
		pBlock->ReleaseContext = &pBatch->Items[i];
		pBlock = pBlock->Next;
	}
	if ( !xrtNetBufMove(&pStream->WriteBuffer, &Temporary) ) {
		pBlock = Temporary.Head;
		while ( pBlock != NULL ) {
			pBlock->Release = NULL;
			pBlock->ReleaseContext = NULL;
			pBlock = pBlock->Next;
		}
		xrtNetBufClear(&Temporary);
		return false;
	}
	__xrtNetStreamSendQueued(pStream);
	return true;
}



/* 为源缓冲建立只读发送视图，全部成功后一次转移原块链。 */
static bool __xrtNetStreamAttachBuffer(
	__xrt_net_stream_buffer* pBatch,
	xnetbuf* pBuffer
)
{
	xnetstream* pStream = pBatch->Stream;
	xnetbuf Temporary;
	xnetblock* pBlock;
	xnetblock* pView;
	size_t iItem = 0;

	(void)xrtNetBufInit(&Temporary, pStream->WriteBuffer.Pool);
	pBlock = pBuffer->Head;
	while ( pBlock != NULL ) {
		cbytes pData = pBlock->Class == XRT_NET_BLOCK_REF ?
			pBlock->External : pBlock->Data;
		size_t iSize;

		if ( (pBlock->End <= pBlock->Begin) ||
			(iItem >= pBatch->Remaining) ) {
			xrtNetBufClear(&Temporary);
			__xrtNetStreamSetError(
				XERR_INTERNAL,
				XNET_ERROR_BUFFER_STATE,
				"send-stream-buffer",
				"network buffer block metadata is inconsistent"
			);
			return false;
		}
		iSize = pBlock->End - pBlock->Begin;
		if ( !xrtNetBufAppendBorrow(
				&Temporary,
				pData + pBlock->Begin,
				iSize
			) ) {
			xrtNetBufClear(&Temporary);
			return false;
		}
		pBatch->Items[iItem].Batch = pBatch;
		iItem++;
		pBlock = pBlock->Next;
	}
	if ( iItem != pBatch->Remaining ) {
		xrtNetBufClear(&Temporary);
		__xrtNetStreamSetError(
			XERR_INTERNAL,
			XNET_ERROR_BUFFER_STATE,
			"send-stream-buffer",
			"network buffer block count is inconsistent"
		);
		return false;
	}
	pView = Temporary.Head;
	for ( size_t i = 0; i < iItem; i++ ) {
		pView->Release = __xrtNetStreamBufferRelease;
		pView->ReleaseContext = &pBatch->Items[i];
		pView = pView->Next;
	}
	__xrtNetBufMoveTrusted(&pBatch->Owned, pBuffer);
	__xrtNetBufMoveTrusted(&pStream->WriteBuffer, &Temporary);
	__xrtNetStreamSendQueued(pStream);
	return true;
}



/* 在所属 Worker 上接收一个已经转移所有权的发送节点。 */
static void __xrtNetStreamQueueSend(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_stream_send* pSend =
		(__xrt_net_stream_send*)pData;
	xnetstream* pStream = pSend->Stream;
	bool bClosed;
	uint32 iCommands;

	(void)pWorker;
	iCommands = xrtAtomic32FetchSub(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iCommands == 1 ) {
		__xrtNetStreamWakeLifecycle(pStream);
	}
	if ( __xrtNetStreamAttachSend(pSend) ) {
		return;
	}
	bClosed = pStream->AbortRequested ||
		(xrtNetStreamState(pStream) == XNET_STREAM_CLOSED);
	if ( !bClosed ) {
		__xrtNetStreamRememberError(pStream);
	}
	__xrtNetStreamDiscardSend(pSend, true);
	if ( !bClosed ) {
		__xrtNetStreamFail(pStream);
	} else if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSING ) {
		__xrtNetStreamTryFinish(pStream);
	}
}



/* 在所属 Worker 上接收一个已经转移所有权的引用批次。 */
static void __xrtNetStreamQueueRefs(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_stream_refs* pBatch =
		(__xrt_net_stream_refs*)pData;
	xnetstream* pStream = pBatch->Stream;
	bool bClosed;
	uint32 iCommands;

	(void)pWorker;
	iCommands = xrtAtomic32FetchSub(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iCommands == 1 ) {
		__xrtNetStreamWakeLifecycle(pStream);
	}
	if ( __xrtNetStreamAttachRefs(pBatch) ) {
		return;
	}
	bClosed = pStream->AbortRequested ||
		(xrtNetStreamState(pStream) == XNET_STREAM_CLOSED);
	if ( !bClosed ) {
		__xrtNetStreamRememberError(pStream);
	}
	__xrtNetStreamDiscardRefs(pBatch, true);
	if ( !bClosed ) {
		__xrtNetStreamFail(pStream);
	} else if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSING ) {
		__xrtNetStreamTryFinish(pStream);
	}
}



#if defined(XRT_FEATURE_NET_TCP_FILE)

/* 在所属 Worker 上把已复制句柄的文件节点接入统一发送队列。 */
static void __xrtNetStreamQueueFile(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_stream_file* pFile =
		(__xrt_net_stream_file*)pData;
	xnetstream* pStream = pFile->Stream;
	bool bAttached = false;
	bool bClosed;
	uint32 iCommands;

	(void)pWorker;
	iCommands = xrtAtomic32FetchSub(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( iCommands == 1 ) {
		__xrtNetStreamWakeLifecycle(pStream);
	}
	if ( !pStream->AbortRequested &&
		(xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ) {
		bAttached = __xrtNetBufAppendFile(
			&pStream->WriteBuffer,
			pFile,
			pFile->Size,
			__xrtNetStreamFileRelease,
			pFile
		);
	}
	if ( bAttached ) {
		__xrtNetStreamSendQueued(pStream);
		return;
	}
	bClosed = pStream->AbortRequested ||
		(xrtNetStreamState(pStream) == XNET_STREAM_CLOSED);
	if ( !bClosed ) {
		__xrtNetStreamRememberError(pStream);
	}
	__xrtNetStreamUnreserveSend(pStream, pFile->Size);
	__xrtNetStreamFileRelease(pFile, NULL, 0);
	if ( !bClosed ) {
		__xrtNetStreamFail(pStream);
	} else if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSING ) {
		__xrtNetStreamTryFinish(pStream);
	}
}



/* 直接入队或通过跨线程命令转移文件节点。 */
static xnetresult __xrtNetStreamSubmitFileNode(
	__xrt_net_stream_file* pFile
)
{
	xnetstream* pStream = pFile->Stream;

	if ( xrtNetWorkerIsCurrent(pStream->Worker) ) {
		if ( __xrtNetBufAppendFile(
			&pStream->WriteBuffer,
			pFile,
			pFile->Size,
			__xrtNetStreamFileRelease,
			pFile
		) ) {
			__xrtNetStreamSendQueued(pStream);
			return XNET_RESULT_OK;
		}
		return (pStream->AbortRequested ||
			(xrtNetStreamState(pStream) == XNET_STREAM_CLOSED)) ?
			XNET_RESULT_CLOSED : XNET_RESULT_ERROR;
	}
	(void)xrtAtomic32FetchAdd(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetEnginePost(
		pStream->Engine,
		xrtNetWorkerIndex(pStream->Worker),
		__xrtNetStreamQueueFile,
		pFile
	) ) {
		(void)xrtAtomic32FetchSub(
			&pStream->SendCommands,
			1,
			XMEMORY_ACQ_REL
		);
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}

#endif



/* 为一种发送所有权分配节点并原子占用硬预算。 */
static __xrt_net_stream_send* __xrtNetStreamCreateSend(
	xnetstream* pStream,
	size_t iSize,
	bool bCopy,
	xnetresult* pResult
)
{
	__xrt_net_stream_send* pSend;
	size_t iAllocation = sizeof(*pSend);
	xnetstreamstate State = xrtNetStreamState(pStream);

	if ( ((State != XNET_STREAM_CONNECTING) &&
		  (State != XNET_STREAM_OPEN)) ||
		 xrtAtomic32Load(&pStream->WriteGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		*pResult = XNET_RESULT_CLOSED;
		return NULL;
	}
	if ( !__xrtNetStreamReserveSend(pStream, iSize) ) {
		*pResult = XNET_RESULT_AGAIN;
		return NULL;
	}
	if ( bCopy ) {
		if ( iSize > (SIZE_MAX - offsetof(
			__xrt_net_stream_send,
			Copy
		)) ) {
			__xrtNetStreamUnreserveSend(pStream, iSize);
			__xrtErrorSetSizeOverflow();
			*pResult = XNET_RESULT_ERROR;
			return NULL;
		}
		iAllocation = offsetof(__xrt_net_stream_send, Copy) + iSize;
	}
	pSend = (__xrt_net_stream_send*)__xrtNetWorkerNodeAlloc(
		pStream->Worker,
		iAllocation
	);
	if ( pSend == NULL ) {
		__xrtNetStreamUnreserveSend(pStream, iSize);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	pSend->Stream = xrtNetStreamRef(pStream);
	pSend->Size = iSize;
	*pResult = XNET_RESULT_OK;
	return pSend;
}



/* 分配批量引用元数据并原子占用全部发送预算。 */
static __xrt_net_stream_refs* __xrtNetStreamCreateRefs(
	xnetstream* pStream,
	const xnetref* pRefs,
	size_t iCount,
	size_t iUsed,
	size_t iTotal,
	xnetresult* pResult
)
{
	__xrt_net_stream_refs* pBatch;
	size_t iAllocation;
	size_t iItem = 0;
	xnetstreamstate State = xrtNetStreamState(pStream);

	if ( ((State != XNET_STREAM_CONNECTING) &&
		  (State != XNET_STREAM_OPEN)) ||
		 xrtAtomic32Load(&pStream->WriteGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		*pResult = XNET_RESULT_CLOSED;
		return NULL;
	}
	if ( iUsed > ((SIZE_MAX - offsetof(
		__xrt_net_stream_refs,
		Items
	)) / sizeof(__xrt_net_stream_ref_item)) ) {
		__xrtErrorSetSizeOverflow();
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	if ( !__xrtNetStreamReserveSend(pStream, iTotal) ) {
		*pResult = XNET_RESULT_AGAIN;
		return NULL;
	}
	iAllocation = offsetof(__xrt_net_stream_refs, Items) +
		(iUsed * sizeof(__xrt_net_stream_ref_item));
	pBatch = (__xrt_net_stream_refs*)__xrtNetWorkerNodeAlloc(
		pStream->Worker,
		iAllocation
	);
	if ( pBatch == NULL ) {
		__xrtNetStreamUnreserveSend(pStream, iTotal);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	pBatch->Stream = xrtNetStreamRef(pStream);
	pBatch->Total = iTotal;
	pBatch->Count = iUsed;
	pBatch->Remaining = iUsed;
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pRefs[i].Size == 0 ) {
			continue;
		}
		pBatch->Items[iItem].Batch = pBatch;
		pBatch->Items[iItem].Data = pRefs[i].Data;
		pBatch->Items[iItem].Size = pRefs[i].Size;
		pBatch->Items[iItem].Release = pRefs[i].Release;
		pBatch->Items[iItem].Context = pRefs[i].Context;
		iItem++;
	}
	*pResult = XNET_RESULT_OK;
	return pBatch;
}



/* 为 Worker 内缓冲接管分配批次并原子占用完整发送预算。 */
static __xrt_net_stream_buffer* __xrtNetStreamCreateBuffer(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	xnetresult* pResult
)
{
	__xrt_net_stream_buffer* pBatch;
	size_t iCount = pBuffer->Blocks;
	size_t iSize = pBuffer->Size;
	size_t iAllocation;

	if ( (iCount == 0) || (iCount > ((SIZE_MAX - offsetof(
		__xrt_net_stream_buffer,
		Items
	)) / sizeof(__xrt_net_stream_buffer_item))) ) {
		__xrtNetStreamSetError(
			iCount == 0 ? XERR_INTERNAL : XERR_RANGE,
			XNET_ERROR_BUFFER_STATE,
			"send-stream-buffer",
			iCount == 0 ?
				"non-empty network buffer has no blocks" :
				"network buffer block count overflows send metadata"
		);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	if ( !__xrtNetStreamReserveSend(pStream, iSize) ) {
		*pResult = XNET_RESULT_AGAIN;
		return NULL;
	}
	iAllocation = offsetof(__xrt_net_stream_buffer, Items) +
		(iCount * sizeof(__xrt_net_stream_buffer_item));
	pBatch = (__xrt_net_stream_buffer*)__xrtNetWorkerNodeAlloc(
		pStream->Worker,
		iAllocation
	);
	if ( pBatch == NULL ) {
		__xrtNetStreamUnreserveSend(pStream, iSize);
		*pResult = XNET_RESULT_ERROR;
		return NULL;
	}
	pBatch->Stream = xrtNetStreamRef(pStream);
	pBatch->Count = iCount;
	pBatch->Remaining = iCount;
	(void)xrtNetBufInit(&pBatch->Owned, pBuffer->Pool);
	*pResult = XNET_RESULT_OK;
	return pBatch;
}



/* 回收尚未接管源缓冲的发送批次。 */
static void __xrtNetStreamDiscardBuffer(
	__xrt_net_stream_buffer* pBatch,
	size_t iSize
)
{
	xnetstream* pStream = pBatch->Stream;

	__xrtNetStreamUnreserveSend(pStream, iSize);
	__xrtNetWorkerNodeRecycle(
		pStream->Worker,
		pBatch,
		offsetof(__xrt_net_stream_buffer, Items) +
			(pBatch->Count * sizeof(__xrt_net_stream_buffer_item))
	);
	xrtNetStreamDestroy(pStream);
}



/* 直接入队或跨线程提交一个已经准备好的发送节点。 */
static xnetresult __xrtNetStreamSubmitSend(
	__xrt_net_stream_send* pSend
)
{
	xnetstream* pStream = pSend->Stream;

	if ( xrtNetWorkerIsCurrent(pStream->Worker) ) {
		if ( __xrtNetStreamAttachSend(pSend) ) {
			return XNET_RESULT_OK;
		}
		if ( pStream->AbortRequested ||
			 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
			__xrtNetStreamDiscardSend(pSend, false);
			return XNET_RESULT_CLOSED;
		}
		__xrtNetStreamDiscardSend(pSend, false);
		return XNET_RESULT_ERROR;
	}
	(void)xrtAtomic32FetchAdd(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetEnginePost(
		pStream->Engine,
		xrtNetWorkerIndex(pStream->Worker),
		__xrtNetStreamQueueSend,
		pSend
	) ) {
		(void)xrtAtomic32FetchSub(
			&pStream->SendCommands,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetStreamDiscardSend(pSend, false);
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}



/* 直接入队或跨线程提交一个已经准备好的引用批次。 */
static xnetresult __xrtNetStreamSubmitRefs(
	__xrt_net_stream_refs* pBatch
)
{
	xnetstream* pStream = pBatch->Stream;

	if ( xrtNetWorkerIsCurrent(pStream->Worker) ) {
		if ( __xrtNetStreamAttachRefs(pBatch) ) {
			return XNET_RESULT_OK;
		}
		if ( pStream->AbortRequested ||
			 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
			__xrtNetStreamDiscardRefs(pBatch, false);
			return XNET_RESULT_CLOSED;
		}
		__xrtNetStreamDiscardRefs(pBatch, false);
		return XNET_RESULT_ERROR;
	}
	(void)xrtAtomic32FetchAdd(
		&pStream->SendCommands,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetEnginePost(
		pStream->Engine,
		xrtNetWorkerIndex(pStream->Worker),
		__xrtNetStreamQueueRefs,
		pBatch
	) ) {
		(void)xrtAtomic32FetchSub(
			&pStream->SendCommands,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetStreamDiscardRefs(pBatch, false);
		return XNET_RESULT_ERROR;
	}
	return XNET_RESULT_OK;
}



/* 建立一种所有权发送节点并提交给所属 Worker。 */
static xnetresult __xrtNetStreamSend(
	xnetstream* pStream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext,
	bool bCopy,
	bool bOwnsExternal
)
{
	__xrt_net_stream_send* pSend;
	xnetresult Result;

	if ( (pStream == NULL) || ((pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetStreamBeginSend(pStream) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( iSize == 0 ) {
		__xrtNetStreamEndSend(pStream);
		return XNET_RESULT_OK;
	}
	if ( bCopy && pStream->BuffersReady &&
		 xrtNetWorkerIsCurrent(pStream->Worker) ) {
		xnetspan Span = { (cbytes)pData, iSize };

		Result = __xrtNetStreamCopyCurrent(
			pStream,
			&Span,
			1,
			iSize
		);
		__xrtNetStreamEndSend(pStream);
		return Result;
	}
	pSend = __xrtNetStreamCreateSend(pStream, iSize, bCopy, &Result);
	if ( pSend == NULL ) {
		__xrtNetStreamEndSend(pStream);
		return Result;
	}
	pSend->Release = pRelease;
	pSend->ReleaseContext = pContext;
	pSend->OwnsExternal = bOwnsExternal;
	if ( bCopy ) {
		memcpy(pSend->Copy, pData, iSize);
		pSend->Data = pSend->Copy;
	} else {
		pSend->Data = (cbytes)pData;
	}
	Result = __xrtNetStreamSubmitSend(pSend);
	__xrtNetStreamEndSend(pStream);
	return Result;
}



/* 有界复制发送。 */
XRT_API xnetresult xrtNetStreamSend(
	xnetstream* pStream,
	const void* pData,
	size_t iSize
)
{
	return __xrtNetStreamSend(
		pStream,
		pData,
		iSize,
		NULL,
		NULL,
		true,
		false
	);
}



/* 有界聚集复制发送。 */
XRT_API xnetresult xrtNetStreamSendVec(
	xnetstream* pStream,
	const xnetspan* pSpans,
	size_t iCount
)
{
	__xrt_net_stream_send* pSend;
	xnetresult Result;
	size_t iTotal = 0;
	size_t iOffset = 0;

	if ( (pStream == NULL) || ((pSpans == NULL) && (iCount != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (pSpans[i].Data == NULL) && (pSpans[i].Size != 0) ) {
			__xrtErrorSetInvalidArgument();
			return XNET_RESULT_ERROR;
		}
		if ( pSpans[i].Size > (SIZE_MAX - iTotal) ) {
			__xrtErrorSetSizeOverflow();
			return XNET_RESULT_ERROR;
		}
		iTotal += pSpans[i].Size;
	}
	if ( iTotal == 0 ) {
		return __xrtNetStreamSend(
			pStream,
			NULL,
			0,
			NULL,
			NULL,
			true,
			false
		);
	}
	if ( !__xrtNetStreamBeginSend(pStream) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( pStream->BuffersReady &&
		 xrtNetWorkerIsCurrent(pStream->Worker) ) {
		Result = __xrtNetStreamCopyCurrent(
			pStream,
			pSpans,
			iCount,
			iTotal
		);
		__xrtNetStreamEndSend(pStream);
		return Result;
	}
	pSend = __xrtNetStreamCreateSend(pStream, iTotal, true, &Result);
	if ( pSend == NULL ) {
		__xrtNetStreamEndSend(pStream);
		return Result;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pSpans[i].Size != 0 ) {
			memcpy(
				pSend->Copy + iOffset,
				pSpans[i].Data,
				pSpans[i].Size
			);
			iOffset += pSpans[i].Size;
		}
	}
	pSend->Data = pSend->Copy;
	Result = __xrtNetStreamSubmitSend(pSend);
	__xrtNetStreamEndSend(pStream);
	return Result;
}



/* 有界引用发送。 */
XRT_API xnetresult xrtNetStreamSendRef(
	xnetstream* pStream,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
)
{
	if ( (pRelease == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	return __xrtNetStreamSend(
		pStream,
		pData,
		iSize,
		pRelease,
		pContext,
		false,
		true
	);
}



/* 原子提交一组零复制引用。 */
XRT_API xnetresult xrtNetStreamSendRefs(
	xnetstream* pStream,
	const xnetref* pRefs,
	size_t iCount
)
{
	__xrt_net_stream_refs* pBatch;
	xnetresult Result;
	size_t iTotal = 0;
	size_t iUsed = 0;

	if ( (pStream == NULL) || ((pRefs == NULL) && (iCount != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pRefs[i].Size == 0 ) {
			continue;
		}
		if ( (pRefs[i].Data == NULL) || (pRefs[i].Release == NULL) ) {
			__xrtErrorSetInvalidArgument();
			return XNET_RESULT_ERROR;
		}
		if ( pRefs[i].Size > (SIZE_MAX - iTotal) ) {
			__xrtErrorSetSizeOverflow();
			return XNET_RESULT_ERROR;
		}
		iTotal += pRefs[i].Size;
		iUsed++;
	}
	if ( iTotal == 0 ) {
		return __xrtNetStreamSend(
			pStream,
			NULL,
			0,
			NULL,
			NULL,
			false,
			false
		);
	}
	if ( !__xrtNetStreamBeginSend(pStream) ) {
		return XNET_RESULT_CLOSED;
	}
	pBatch = __xrtNetStreamCreateRefs(
		pStream,
		pRefs,
		iCount,
		iUsed,
		iTotal,
		&Result
	);
	if ( pBatch != NULL ) {
		Result = __xrtNetStreamSubmitRefs(pBatch);
	}
	__xrtNetStreamEndSend(pStream);
	return Result;
}



/* 有界接管发送。 */
XRT_API xnetresult xrtNetStreamSendTake(
	xnetstream* pStream,
	ptr pData,
	size_t iSize
)
{
	if ( (pData != NULL) && (iSize == 0) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	return __xrtNetStreamSend(
		pStream,
		pData,
		iSize,
		__xrtNetStreamFreeTaken,
		NULL,
		false,
		true
	);
}



/* 在 Stream Worker 上零复制接管完整缓冲链。 */
XRT_API xnetresult xrtNetStreamSendBuffer(
	xnetstream* pStream,
	xnetbuf* pBuffer
)
{
	__xrt_net_stream_buffer* pBatch;
	xnetresult Result;
	size_t iSize;

	if ( (pStream == NULL) || (pBuffer == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( !xrtNetWorkerIsCurrent(pStream->Worker) ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_WRITE,
			"send-stream-buffer",
			"network buffer send requires the stream worker"
		);
		return XNET_RESULT_ERROR;
	}
	if ( (pBuffer == &pStream->WriteBuffer) ||
		(pBuffer->Reserved != NULL) ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_BUFFER_STATE,
			"send-stream-buffer",
			pBuffer == &pStream->WriteBuffer ?
				"stream write buffer cannot send itself" :
				"network buffer has an active write reservation"
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetStreamBeginSend(pStream) ) {
		return XNET_RESULT_CLOSED;
	}
	iSize = pBuffer->Size;
	if ( iSize == 0 ) {
		__xrtNetStreamEndSend(pStream);
		return XNET_RESULT_OK;
	}
	pBatch = __xrtNetStreamCreateBuffer(pStream, pBuffer, &Result);
	if ( pBatch == NULL ) {
		__xrtNetStreamEndSend(pStream);
		return Result;
	}
	if ( !__xrtNetStreamAttachBuffer(pBatch, pBuffer) ) {
		__xrtNetStreamDiscardBuffer(pBatch, iSize);
		Result = XNET_RESULT_ERROR;
	}
	__xrtNetStreamEndSend(pStream);
	return Result;
}



#if defined(XRT_FEATURE_NET_TCP_FILE)

/* 有界受理文件区间并按现有发送队列顺序执行内核文件发送。 */
XRT_API xnetresult xrtNetStreamSendFile(
	xnetstream* pStream,
	xfile File,
	uint64 iOffset,
	size_t iSize
)
{
	__xrt_net_stream_file* pFile;
	xnetresult Result = XNET_RESULT_ERROR;
	uint64 iFileSize = 0;
	intptr_t iHandle = (intptr_t)-1;

	if ( (pStream == NULL) || (File == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( iSize == 0 ) {
		return XNET_RESULT_OK;
	}
	if ( (iOffset > UINT64_MAX - (uint64)iSize) ||
		!xrtFileSize(File, &iFileSize) ||
		(iOffset > iFileSize) || ((uint64)iSize > (iFileSize - iOffset)) ) {
		if ( xrtGetError() == NULL ) {
			__xrtNetStreamSetError(
				XERR_RANGE,
				XNET_ERROR_STREAM_WRITE,
				"send-stream-file",
				"file send range exceeds the current file size"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetStreamBeginSend(pStream) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtNetStreamReserveSend(pStream, iSize) ) {
		__xrtNetStreamEndSend(pStream);
		return XNET_RESULT_AGAIN;
	}
	if ( !__xrtNetStreamFileDuplicate(File, &iHandle) ) {
		__xrtNetStreamUnreserveSend(pStream, iSize);
		__xrtNetStreamEndSend(pStream);
		return XNET_RESULT_ERROR;
	}
	pFile = (__xrt_net_stream_file*)__xrtNetWorkerNodeAlloc(
		pStream->Worker,
		sizeof(*pFile)
	);
	if ( pFile == NULL ) {
		#if defined(_WIN32) || defined(_WIN64)
			(void)CloseHandle((HANDLE)(uintptr_t)iHandle);
		#else
			(void)close((int)iHandle);
		#endif
		__xrtNetStreamUnreserveSend(pStream, iSize);
		__xrtNetStreamEndSend(pStream);
		return XNET_RESULT_ERROR;
	}
	pFile->Stream = xrtNetStreamRef(pStream);
	pFile->Handle = iHandle;
	pFile->Offset = iOffset;
	pFile->Size = iSize;

	Result = __xrtNetStreamSubmitFileNode(pFile);
	if ( Result != XNET_RESULT_OK ) {
		__xrtNetStreamUnreserveSend(pStream, iSize);
		__xrtNetStreamFileRelease(pFile, NULL, 0);
	}
	__xrtNetStreamEndSend(pStream);
	return Result;
}

#endif



/* 统一生命周期命令只由第一个登记请求持有引用并进入 Worker 队列。 */
static void __xrtNetStreamControlTask(
	xnetworker* pWorker,
	ptr pData
);



/* 原子合并控制请求，避免每个 Stream 为每类请求常驻一个命令节点。 */
static void __xrtNetStreamControlRequest(
	xnetstream* pStream,
	uint32 iRequest
)
{
	uint32 iPrevious = xrtAtomic32FetchOr(
		&pStream->ControlRequests,
		iRequest | XRT_NET_STREAM_CONTROL_POSTED,
		XMEMORY_ACQ_REL
	);

	if ( (iPrevious & XRT_NET_STREAM_CONTROL_POSTED) == 0 ) {
		xrtNetStreamRef(pStream);
		if ( !__xrtNetEnginePostInternal(
			pStream->Worker,
			&pStream->ControlCommand,
			__xrtNetStreamControlTask,
			pStream
		) ) {
			(void)xrtAtomic32FetchAnd(
				&pStream->ControlRequests,
				~XRT_NET_STREAM_CONTROL_POSTED,
				XMEMORY_ACQ_REL
			);
			xrtNetStreamDestroy(pStream);
		}
	}
}



/* 在 Worker 上执行已经合并的读取恢复。 */
static void __xrtNetStreamApplyResume(xnetstream* pStream)
{
	if ( (xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
		!xrtAtomic32Load(
			&pStream->CloseGate,
			XMEMORY_ACQUIRE
		) &&
		!xrtAtomic32Load(
			&pStream->AbortGate,
			XMEMORY_ACQUIRE
		) &&
		!xrtAtomic32Load(
			&pStream->ReadEnded,
			XMEMORY_ACQUIRE
		) &&
		!xrtAtomic32Load(
			&pStream->ReadPaused,
			XMEMORY_ACQUIRE
		 ) ) {
		__xrtNetStreamDriveRead(pStream);
	}
}



/* 暂停新读取。 */
XRT_API void xrtNetStreamPause(xnetstream* pStream)
{
	if ( pStream != NULL ) {
		xrtAtomic32Store(
			&pStream->ReadPaused,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 恢复读取并投递一次推进。 */
XRT_API bool xrtNetStreamResume(xnetstream* pStream)
{
	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ||
		 xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->ReadEnded, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamSetError(
			XERR_CLOSED,
			XNET_ERROR_STREAM_READ,
			"resume-stream",
			"TCP stream is not open"
		);
		return false;
	}
	xrtAtomic32Store(
		&pStream->ReadPaused,
		0,
		XMEMORY_RELEASE
	);
	__xrtNetStreamControlRequest(
		pStream,
		XRT_NET_STREAM_CONTROL_RESUME
	);
	return true;
}



/* 在 Worker 上登记写半关闭。 */
static void __xrtNetStreamApplyShutdown(xnetstream* pStream)
{
	pStream->ShutdownRequested = true;
	__xrtNetStreamNotifyFutures(pStream, false);
	__xrtNetStreamDriveWrite(pStream);
}



/* 请求排空后的写半关闭。 */
XRT_API bool xrtNetStreamShutdownWrite(xnetstream* pStream)
{
	uint32 iExpected = 0;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSED ) {
		return true;
	}
	if ( xrtAtomic32Load(&pStream->CloseGate, XMEMORY_ACQUIRE) ||
		 xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pStream->WriteGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	__xrtNetStreamControlRequest(
		pStream,
		XRT_NET_STREAM_CONTROL_SHUTDOWN
	);
	return true;
}



/* 在 Worker 上开始正常关闭，并吸收已经建立的 Abort 升级。 */
static void __xrtNetStreamApplyClose(xnetstream* pStream)
{
	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		pStream->AbortRequested = true;
		pStream->CloseResult = XNET_RESULT_CANCELLED;
	}
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		if ( xrtNetStreamState(pStream) == XNET_STREAM_CONNECTING ) {
			pStream->AbortRequested = true;
			pStream->CloseResult = XNET_RESULT_CANCELLED;
		}
		pStream->CloseRequested = true;
		if ( !pStream->AbortRequested ) {
			pStream->CloseResult = XNET_RESULT_OK;
		}
		xrtAtomic32Store(
			&pStream->State,
			XNET_STREAM_CLOSING,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&pStream->ReadPaused,
			1,
			XMEMORY_RELEASE
		);
		__xrtNetStreamNotifyFutures(pStream, false);
		__xrtNetStreamCancelOperations(pStream);
		__xrtNetStreamDriveWrite(pStream);
		__xrtNetStreamTryFinish(pStream);
	}
}



/* 请求排空发送队列后正常关闭。 */
XRT_API bool xrtNetStreamClose(xnetstream* pStream)
{
	uint32 iExpected = 0;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSED ) {
		return true;
	}
	if ( xrtAtomic32Load(&pStream->AbortGate, XMEMORY_ACQUIRE) ) {
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pStream->CloseGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	__xrtNetStreamControlRequest(
		pStream,
		XRT_NET_STREAM_CONTROL_CLOSE
	);
	return true;
}



/* 在 Worker 上升级为异常关闭。 */
static void __xrtNetStreamApplyAbort(xnetstream* pStream)
{
	if ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		pStream->AbortRequested = true;
		pStream->CloseRequested = true;
		pStream->CloseResult = XNET_RESULT_CANCELLED;
		xrtAtomic32Store(
			&pStream->State,
			XNET_STREAM_CLOSING,
			XMEMORY_RELEASE
		);
		xrtAtomic32Store(
			&pStream->ReadPaused,
			1,
			XMEMORY_RELEASE
		);
		__xrtNetStreamNotifyFutures(pStream, false);
		__xrtNetStreamCancelOperations(pStream);
		__xrtNetStreamTryFinish(pStream);
	}
}



/*
	批量处理控制请求。
	生命周期请求等待此前受理的发送命令排空；Abort 高于 Close，Close 高于 Shutdown。
*/
static void __xrtNetStreamControlTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetstream* pStream = (xnetstream*)pData;

	(void)pWorker;
	for ( ;; ) {
		uint32 iRequests = xrtAtomic32Exchange(
			&pStream->ControlRequests,
			XRT_NET_STREAM_CONTROL_POSTED,
			XMEMORY_ACQ_REL
		);
		uint32 iLifecycle = iRequests &
			XRT_NET_STREAM_CONTROL_LIFECYCLE;

		if ( (iRequests & XRT_NET_STREAM_CONTROL_RESUME) != 0 ) {
			__xrtNetStreamApplyResume(pStream);
		}
		if ( (iLifecycle != 0) && (xrtAtomic32Load(
			&pStream->SendSubmitters,
			XMEMORY_ACQUIRE
		 ) || xrtAtomic32Load(
			&pStream->SendCommands,
			XMEMORY_ACQUIRE
		 )) ) {
			uint32 iExpected;

			(void)xrtAtomic32FetchOr(
				&pStream->ControlRequests,
				iLifecycle,
				XMEMORY_ACQ_REL
			);
			iExpected = XRT_NET_STREAM_CONTROL_POSTED | iLifecycle;
			if ( xrtAtomic32CompareExchange(
				&pStream->ControlRequests,
				&iExpected,
				iLifecycle,
				XMEMORY_ACQ_REL,
				XMEMORY_ACQUIRE
			) ) {
				if ( !xrtAtomic32Load(
					&pStream->SendSubmitters,
					XMEMORY_ACQUIRE
				) && !xrtAtomic32Load(
					&pStream->SendCommands,
					XMEMORY_ACQUIRE
				) ) {
					__xrtNetStreamWakeLifecycle(pStream);
				}
				xrtNetStreamDestroy(pStream);
				return;
			}
			continue;
		}
		if ( (iLifecycle & XRT_NET_STREAM_CONTROL_ABORT) != 0 ) {
			__xrtNetStreamApplyAbort(pStream);
		} else if ( (iLifecycle & XRT_NET_STREAM_CONTROL_CLOSE) != 0 ) {
			__xrtNetStreamApplyClose(pStream);
		} else if ( (iLifecycle &
			XRT_NET_STREAM_CONTROL_SHUTDOWN) != 0 ) {
			__xrtNetStreamApplyShutdown(pStream);
		}
		if ( (iRequests & XRT_NET_STREAM_CONTROL_FINISH) != 0 ) {
			__xrtNetStreamTryFinish(pStream);
		}
		{
			uint32 iExpected = XRT_NET_STREAM_CONTROL_POSTED;

			if ( xrtAtomic32CompareExchange(
				&pStream->ControlRequests,
				&iExpected,
				0,
				XMEMORY_ACQ_REL,
				XMEMORY_ACQUIRE
			) ) {
				break;
			}
		}
	}
	xrtNetStreamDestroy(pStream);
}



/* 请求立即异常关闭。 */
XRT_API bool xrtNetStreamAbort(xnetstream* pStream)
{
	uint32 iExpected = 0;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( xrtNetStreamState(pStream) == XNET_STREAM_CLOSED ) {
		return true;
	}
	if ( !xrtAtomic32CompareExchange(
		&pStream->AbortGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	__xrtNetStreamControlRequest(
		pStream,
		XRT_NET_STREAM_CONTROL_ABORT
	);
	xrtAtomic32Store(&pStream->CloseGate, 1, XMEMORY_RELEASE);
	xrtAtomic32Store(&pStream->WriteGate, 1, XMEMORY_RELEASE);
	return true;
}



/* 在 Listener Worker 上发布一次可恢复的接受错误。 */
static void __xrtNetListenerReportError(
	xnetlistener* pListener,
	const xerror* pError
)
{
	__xrtNetStatBasicAdd(
		&pListener->Errors,
		1
	);
	if ( pListener->Events.Error != NULL ) {
		pListener->Events.Error(
			pListener,
			pError,
			pListener->Data
		);
	}
}



/* 取走当前错误并在 Listener Worker 上发布。 */
static void __xrtNetListenerError(xnetlistener* pListener)
{
	xerror* pError = xrtTakeError();

	__xrtNetListenerReportError(pListener, pError);
	xrtErrorFree(pError);
}



/* 判断 accept 是否因进程或系统资源暂时耗尽而失败。 */
static bool __xrtNetListenerResourceError(int iSystemCode)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (iSystemCode == WSAEMFILE) ||
			(iSystemCode == WSAENOBUFS) ||
			(iSystemCode == WSA_NOT_ENOUGH_MEMORY);
	#else
		return (iSystemCode == EMFILE) ||
			(iSystemCode == ENFILE) ||
			(iSystemCode == ENOBUFS) ||
			(iSystemCode == ENOMEM);
	#endif
}



/* Retry Timer 到期后重新补足 accept；Timer 引用在唯一终态释放。 */
static void __xrtNetListenerAcceptRetry(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xnetlistener* pListener = (xnetlistener*)pData;
	uint32 iCapabilities;
	bool bResult = true;

	if ( pListener->AcceptRetryTimer == Id ) {
		pListener->AcceptRetryTimer = 0;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(xrtNetListenerState(pListener) == XNET_LISTENER_OPEN) ) {
		iCapabilities = xrtNetPortCapabilities(
			xrtNetWorkerPort(pWorker)
		);
		if ( (iCapabilities & XNET_PORT_CAP_COMPLETION) != 0 ) {
			bResult = __xrtNetListenerArmAccepts(pListener);
		} else {
			bResult = __xrtNetListenerWatch(pListener);
		}
		if ( !bResult ) {
			__xrtNetListenerError(pListener);
			(void)xrtNetListenerClose(pListener);
		}
	}
	xrtNetListenerDestroy(pListener);
}



/* 暂停 accept 并使用 10 ms 到 1 s 的指数退避恢复。 */
static bool __xrtNetListenerPauseAccept(xnetlistener* pListener)
{
	uint64 iDelay = pListener->AcceptRetryDelay;
	uint64 Id;

	if ( pListener->AcceptRetryTimer != 0 ) {
		return true;
	}
	if ( iDelay == 0 ) {
		iDelay = XRT_NET_LISTENER_RETRY_MIN;
	}
	if ( xrtNetListenerRef(pListener) == NULL ) {
		return false;
	}
	Id = xrtNetEngineAfter(
		pListener->Engine,
		pListener->Config.Affinity,
		iDelay,
		__xrtNetListenerAcceptRetry,
		pListener
	);
	if ( Id == 0 ) {
		xrtNetListenerDestroy(pListener);
		return false;
	}
	pListener->AcceptRetryTimer = Id;
	pListener->AcceptRetryDelay = iDelay <
		(XRT_NET_LISTENER_RETRY_MAX / 2u) ?
		iDelay * 2u : XRT_NET_LISTENER_RETRY_MAX;
	return true;
}



/* 以槽身份收回一个完成式 Accept，避免在热路径扫描全部并发槽。 */
static bool __xrtNetListenerAcceptDone(
	xnetlistener* pListener,
	__xrt_net_accept_slot* pSlot,
	uint64 Id
)
{
	if ( (pSlot == NULL) || (pSlot->Listener != pListener) ||
		 (pSlot->Id == 0) || (pSlot->Id != Id) ) {
		__xrtNetStreamSetError(
			XERR_INTERNAL,
			XNET_ERROR_LISTENER_ACCEPT,
			"complete-accept",
			"TCP accept completion identity mismatch"
		);
		return false;
	}
	pSlot->Id = 0;
	(void)xrtAtomic32FetchSub(
		&pListener->ActiveAccepts,
		1,
		XMEMORY_RELAXED
	);
	return true;
}



/* 在所有 Accept 终态到达后关闭 Listener。 */
static void __xrtNetListenerTryFinish(xnetlistener* pListener)
{
	if ( (xrtNetListenerState(pListener) != XNET_LISTENER_CLOSING) ||
		 pListener->StartPending ||
		 (xrtAtomic32Load(
			&pListener->ActiveAccepts,
			XMEMORY_ACQUIRE
		 ) != 0) ||
		 (xrtAtomic32Load(
			&pListener->ActiveDispatches,
			XMEMORY_ACQUIRE
		 ) != 0) ) {
		return;
	}
	if ( pListener->Socket != NULL ) {
		(void)xrtNetSocketClose(pListener->Socket);
		pListener->Socket = NULL;
	}
	#if defined(XRT_FEATURE_NET_TCP_FUTURE)
		__xrtSpinLock(&pListener->AcceptLock);
		pListener->WaitClosed = true;
		__xrtSpinUnlock(&pListener->AcceptLock);
	#endif
	__xrtNetListenerReleaseEngine(pListener);
	xrtAtomic32Store(
		&pListener->State,
		XNET_LISTENER_CLOSED,
		XMEMORY_RELEASE
	);
	if ( pListener->Events.Close != NULL ) {
		pListener->Events.Close(pListener, pListener->Data);
	}
	if ( pListener->RuntimeHeld ) {
		pListener->RuntimeHeld = false;
		xrtNetListenerDestroy(pListener);
	}
}



/* 在 Listener Worker 上收回分发任务并推进关闭。 */
static void __xrtNetListenerAcceptFinished(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_accept_task* pTask =
		(__xrt_net_accept_task*)pData;
	xnetlistener* pListener = pTask->Listener;

	(void)pWorker;
	if ( pTask->Error != NULL ) {
		__xrtNetListenerReportError(pListener, pTask->Error);
		xrtErrorFree(pTask->Error);
		pTask->Error = NULL;
	}
	(void)xrtAtomic32FetchSub(
		&pListener->ActiveDispatches,
		1,
		XMEMORY_ACQ_REL
	);
	if ( xrtNetListenerState(pListener) == XNET_LISTENER_CLOSING ) {
		__xrtNetListenerTryFinish(pListener);
	}
	__xrtNetListenerTaskRecycle(pListener, pTask);
	xrtNetListenerDestroy(pListener);
}



/* 在目标 Worker 上发布一个已接受 Stream。 */
static void __xrtNetListenerAcceptTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_net_accept_task* pTask =
		(__xrt_net_accept_task*)pData;
	xnetlistener* pListener = pTask->Listener;
	xnetstream* pStream = pTask->Stream;
	bool bAccepted = false;
	bool bSetupFailed = false;
	bool bClosing = xrtNetListenerState(pListener) != XNET_LISTENER_OPEN;

	if ( bClosing ) {
		/* Listener 已经关闭入口，尚未公开的连接直接回收。 */
	} else if ( !xrtNetBufInit(
		&pStream->ReadBuffer,
		xrtNetWorkerBufPool(pWorker)
	) ) {
		bSetupFailed = true;
	} else if ( !xrtNetBufInit(
		&pStream->WriteBuffer,
		xrtNetWorkerBufPool(pWorker)
	) ) {
		xrtNetBufClear(&pStream->ReadBuffer);
		bSetupFailed = true;
	} else {
		pStream->BuffersReady = true;
		if ( !xrtNetSocketLocal(pStream->Socket, &pStream->Local) ) {
			bSetupFailed = true;
		} else {
			xrtAtomic32Store(
				&pStream->State,
				XNET_STREAM_OPEN,
				XMEMORY_RELEASE
			);
			if ( pListener->Events.Accept != NULL ) {
				bAccepted = pListener->Events.Accept(
					pListener,
					pStream,
					pListener->Data
				);
			} else {
				bAccepted = __xrtNetListenerAcceptFuture(
					pListener,
					pStream
				);
				if ( !bAccepted ) {
					bAccepted = __xrtNetListenerQueue(
						pListener,
						pStream
					);
				}
			}
		}
	}
	if ( bSetupFailed ) {
		pTask->Error = xrtTakeError();
	}
	if ( bAccepted ) {
		__xrtNetStatFullAdd(
			&pListener->Accepted,
			1
		);
		if ( !xrtAtomic32Load(
			&pStream->CloseGate,
			XMEMORY_ACQUIRE
		) && !xrtAtomic32Load(
			&pStream->AbortGate,
			XMEMORY_ACQUIRE
		) ) {
			pStream->OpenEmitted = true;
			if ( pStream->Events.Open != NULL ) {
				pStream->Events.Open(
					pStream,
					__xrtNetStreamDataCurrent(pStream)
				);
			}
		}
		__xrtNetStreamNotifyFutures(pStream, true);
		if ( pListener->Events.Accept == NULL ) {
			__xrtNetListenerNotifyFutures(pListener);
		}
		if ( (xrtNetStreamState(pStream) == XNET_STREAM_OPEN) &&
			 !xrtAtomic32Load(
				&pStream->CloseGate,
				XMEMORY_ACQUIRE
			 ) ) {
			__xrtNetStreamDriveRead(pStream);
		}
	} else {
		if ( !bSetupFailed && !bClosing ) {
			__xrtNetStatBasicAdd(
				&pListener->Rejected,
				1
			);
		}
		memset(&pStream->Events, 0, sizeof(pStream->Events));
		pStream->CloseResult = XNET_RESULT_CANCELLED;
		__xrtNetStreamFail(pStream);
		xrtNetStreamDestroy(pStream);
	}
	if ( !__xrtNetEnginePostInternal(
		pListener->Worker,
		&pTask->Internal,
		__xrtNetListenerAcceptFinished,
		pTask
	) ) {
		__xrtNetListenerAcceptFinished(pWorker, pTask);
	}
}



/* 把已接受 Socket 配置并分发到轮转 Worker。 */
static void __xrtNetListenerDispatch(
	xnetlistener* pListener,
	xnetsocket Socket,
	const xnetaddr* pRemote
)
{
	xnetworker* pWorker = pListener->Worker;
	xnetstream* pStream;
	__xrt_net_accept_task* pTask;
	xerror* pError;

	if ( pListener->Config.Distribution == XNET_ACCEPT_ROUND_ROBIN ) {
		uint32 iWorkers = xrtNetEngineWorkerCount(pListener->Engine);
		uint64 iAffinity = pListener->NextAffinity++;

		pWorker = xrtNetEngineWorker(
			pListener->Engine,
			(uint32)(iAffinity % iWorkers)
		);
	}

	if ( !__xrtNetStreamSocketOptions(Socket, &pListener->Config.Stream) ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtNetListenerError(pListener);
		return;
	}
	pStream = __xrtNetStreamCreate(
		pListener->Engine,
		pWorker,
		Socket,
		&pListener->Config.Stream,
		&pListener->StreamEvents,
		NULL
	);
	if ( pStream == NULL ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtNetListenerError(pListener);
		return;
	}
	pStream->Remote = *pRemote;
	pTask = __xrtNetListenerTaskAlloc(pListener);
	if ( pTask == NULL ) {
		pError = xrtTakeError();
		pStream->Socket = NULL;
		(void)xrtNetSocketClose(Socket);
		xrtAtomic32Store(
			&pStream->State,
			XNET_STREAM_CLOSED,
			XMEMORY_RELEASE
		);
		pStream->RuntimeHeld = false;
		xrtNetStreamDestroy(pStream);
		xrtNetStreamDestroy(pStream);
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtNetListenerError(pListener);
		return;
	}
	pTask->Listener = xrtNetListenerRef(pListener);
	pTask->Stream = pStream;
	(void)xrtAtomic32FetchAdd(
		&pListener->ActiveDispatches,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetEnginePost(
		pListener->Engine,
		xrtNetWorkerIndex(pWorker),
		__xrtNetListenerAcceptTask,
		pTask
	) ) {
		pError = xrtTakeError();
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveDispatches,
			1,
			XMEMORY_ACQ_REL
		);
		__xrtNetListenerTaskRecycle(pListener, pTask);
		xrtNetListenerDestroy(pListener);
		pStream->Socket = NULL;
		(void)xrtNetSocketClose(Socket);
		xrtAtomic32Store(
			&pStream->State,
			XNET_STREAM_CLOSED,
			XMEMORY_RELEASE
		);
		pStream->RuntimeHeld = false;
		xrtNetStreamDestroy(pStream);
		xrtNetStreamDestroy(pStream);
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtNetListenerError(pListener);
	}
}



/* 向 completion 端口补足预投递 Accept。 */
static bool __xrtNetListenerArmAccepts(xnetlistener* pListener)
{
	xnetport* pPort = xrtNetWorkerPort(pListener->Worker);
	bool bArmed = false;

	for ( uint32 i = 0; i < pListener->Config.AcceptConcurrency; i++ ) {
		__xrt_net_accept_slot* pSlot = &pListener->AcceptSlots[i];
		uint64 Id;

		if ( pSlot->Id != 0 ) {
			continue;
		}
		Id = xrtNetWorkerOperationId(pListener->Worker);
		if ( (Id == 0) || !xrtNetPortAccept(
			pPort,
			pListener->Socket,
			Id,
			&pSlot->Completion
		) ) {
			if ( !bArmed && (xrtAtomic32Load(
				&pListener->ActiveAccepts,
				XMEMORY_ACQUIRE
			) == 0) ) {
				return false;
			}
			xrtClearError();
			break;
		}
		pSlot->Id = Id;
		(void)xrtAtomic32FetchAdd(
			&pListener->ActiveAccepts,
			1,
			XMEMORY_RELAXED
		);
		bArmed = true;
	}
	return true;
}



/* 向 readiness 端口提交一次 Listener 观察。 */
static bool __xrtNetListenerWatch(xnetlistener* pListener)
{
	pListener->WatchId = xrtNetWorkerOperationId(pListener->Worker);
	if ( (pListener->WatchId == 0) || !xrtNetPortWatch(
		xrtNetWorkerPort(pListener->Worker),
		pListener->Socket,
		pListener->WatchId,
		XNET_POLL_READ,
		&pListener->Completion
	) ) {
		return false;
	}
	pListener->WatchPending = true;
	(void)xrtAtomic32FetchAdd(
		&pListener->ActiveAccepts,
		1,
		XMEMORY_RELAXED
	);
	return true;
}



/* 处理 Listener completion 和 readiness 事件。 */
static void __xrtNetListenerCompletion(
	xnetworker* pWorker,
	const xnetportevent* pEvent,
	ptr pData
)
{
	xnetlistener* pListener = (xnetlistener*)pData;

	(void)pWorker;
	if ( pEvent->Type == XNET_PORT_EVENT_ACCEPT ) {
		__xrt_net_accept_slot* pSlot =
			(__xrt_net_accept_slot*)pData;
		bool bPauseAccept = false;

		pListener = pSlot->Listener;
		if ( !__xrtNetListenerAcceptDone(
			pListener,
			pSlot,
			pEvent->Id
		) ) {
			if ( pEvent->Accepted != NULL ) {
				(void)xrtNetSocketClose(pEvent->Accepted);
			}
			__xrtNetListenerError(pListener);
			(void)xrtNetListenerClose(pListener);
			return;
		}
		/*
		 * CancelIoEx 与 AcceptEx 的正常完成可以竞争。Listener 关闭入口
		 * 建立后，迟到的成功 Socket 直接回收，其他终态也不再发布 Error。
		 */
		if ( xrtNetListenerState(pListener) != XNET_LISTENER_OPEN ) {
			if ( pEvent->Accepted != NULL ) {
				(void)xrtNetSocketClose(pEvent->Accepted);
			}
			__xrtNetListenerTryFinish(pListener);
			return;
		}
		if ( pEvent->Result == XNET_RESULT_OK ) {
			pListener->AcceptRetryDelay = XRT_NET_LISTENER_RETRY_MIN;
			__xrtNetListenerDispatch(
				pListener,
				pEvent->Accepted,
				&pEvent->Address
			);
		} else {
			bPauseAccept = __xrtNetListenerResourceError(
				pEvent->SystemCode
			);
			__xrtNetSocketSetSystemError(
				XNET_ERROR_LISTENER_ACCEPT,
				"accept-listener",
				"TCP accept failed",
				pEvent->SystemCode
			);
			__xrtNetListenerError(pListener);
		}
		if ( xrtNetListenerState(pListener) == XNET_LISTENER_OPEN ) {
			bool bResult = bPauseAccept ?
				__xrtNetListenerPauseAccept(pListener) :
				__xrtNetListenerArmAccepts(pListener);

			if ( !bResult ) {
				__xrtNetListenerError(pListener);
				(void)xrtNetListenerClose(pListener);
			}
		} else {
			__xrtNetListenerTryFinish(pListener);
		}
	} else if ( pEvent->Type == XNET_PORT_EVENT_READY ) {
		bool bEventError = (pEvent->Flags & (
			XNET_PORT_EVENT_ERROR |
			XNET_PORT_EVENT_HANGUP
		)) != 0;
		bool bAcceptError = false;
		bool bPauseAccept = false;

		pListener->WatchPending = false;
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveAccepts,
			1,
			XMEMORY_RELAXED
		);
		/* 复合事件必须先耗尽已完成连接，避免 READ 被 ERROR 或 HANGUP 吞掉。 */
		for ( uint32 i = 0;
			i < pListener->Config.AcceptConcurrency;
			i++ ) {
			xnetsocket Socket = NULL;
			xnetaddr Remote;
			xnetresult Result = xrtNetSocketAccept(
				pListener->Socket,
				&Socket,
				&Remote
			);

			if ( Result == XNET_RESULT_AGAIN ) {
				break;
			}
			if ( Result != XNET_RESULT_OK ) {
				const xerror* pError = xrtGetError();

				bPauseAccept = (pError != NULL) &&
					__xrtNetListenerResourceError(
						xrtErrorSystemCode(pError)
					);
				__xrtNetListenerError(pListener);
				bAcceptError = true;
				break;
			}
			pListener->AcceptRetryDelay = XRT_NET_LISTENER_RETRY_MIN;
			__xrtNetListenerDispatch(pListener, Socket, &Remote);
		}
		if ( bEventError ) {
			if ( !bAcceptError ) {
				if ( pEvent->SystemCode != 0 ) {
					__xrtNetSocketSetSystemError(
						XNET_ERROR_LISTENER_ACCEPT,
						"accept-listener",
						"TCP readiness backend reported a listener error",
						pEvent->SystemCode
					);
				} else {
					__xrtNetStreamSetError(
						XERR_IO,
						XNET_ERROR_LISTENER_ACCEPT,
						"accept-listener",
						"TCP readiness backend reported a listener error"
					);
				}
				__xrtNetListenerError(pListener);
			}
			(void)xrtNetListenerClose(pListener);
			return;
		}
		if ( xrtNetListenerState(pListener) == XNET_LISTENER_OPEN ) {
			bool bResult = bPauseAccept ?
				__xrtNetListenerPauseAccept(pListener) :
				__xrtNetListenerWatch(pListener);

			if ( !bResult ) {
				__xrtNetListenerError(pListener);
				(void)xrtNetListenerClose(pListener);
			}
		} else {
			__xrtNetListenerTryFinish(pListener);
		}
	}
}



/* 在 Listener Worker 上启动接受。 */
static void __xrtNetListenerStart(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetlistener* pListener = (xnetlistener*)pData;
	uint32 iCapabilities = xrtNetPortCapabilities(
		xrtNetWorkerPort(pWorker)
	);
	bool bResult;

	pListener->StartPending = false;
	if ( xrtNetListenerState(pListener) != XNET_LISTENER_OPEN ) {
		__xrtNetListenerTryFinish(pListener);
		return;
	}
	if ( (iCapabilities & XNET_PORT_CAP_COMPLETION) != 0 ) {
		bResult = __xrtNetListenerArmAccepts(pListener);
	} else {
		bResult = __xrtNetListenerWatch(pListener);
	}
	if ( !bResult ) {
		__xrtNetListenerError(pListener);
		(void)xrtNetListenerClose(pListener);
	}
}



/* 同步建立监听 Socket 并异步启动 Accept。 */
XRT_API xnetlistener* xrtNetListen(
	xnetengine* pEngine,
	const xnetlistenconfig* pConfig,
	const xnetlistenerevents* pEvents,
	const xnetstreamevents* pStreamEvents,
	ptr pData
)
{
	xnetlistenconfig Config;
	xnetlistener* pListener;
	xnetworker* pWorker;
	xnetsocket Socket;
	xerror* pError;

	if ( pEngine == NULL ) {
		__xrtNetStreamSetError(
			XERR_ARGUMENT,
			XNET_ERROR_LISTENER_CREATE,
			"create-listener",
			"TCP listener requires a running engine"
		);
		return NULL;
	}
	if ( xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING ) {
		__xrtNetStreamSetError(
			XERR_CLOSED,
			XNET_ERROR_LISTENER_CREATE,
			"create-listener",
			"TCP listener requires a running engine"
		);
		return NULL;
	}
	xrtNetListenConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetListenConfigValid(&Config) ) {
		return NULL;
	}
	pWorker = xrtNetEngineWorker(
		pEngine,
		(uint32)(Config.Affinity % xrtNetEngineWorkerCount(pEngine))
	);
	if ( pWorker == NULL ) {
		return NULL;
	}
	Socket = xrtNetSocketOpen(
		(xnetfamily)Config.Address.Family,
		XNET_SOCKET_STREAM,
		XNET_SOCKET_NONBLOCK
	);
	if ( Socket == NULL ) {
		return NULL;
	}
	if ( (Config.ReuseAddress && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_REUSE_ADDRESS,
		1
	)) || (Config.ReusePort && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_REUSE_PORT,
		1
	)) || (Config.ExclusiveAddress && !xrtNetSocketSet(
		Socket,
		XNET_OPTION_EXCLUSIVE_ADDRESS,
		1
	)) || ((Config.Address.Family == XNET_FAMILY_IPV6) &&
		 !xrtNetSocketSet(
			Socket,
			XNET_OPTION_IPV6_ONLY,
			Config.IPv6Only ? 1 : 0
		 )) || !xrtNetSocketBind(Socket, &Config.Address) ||
		 !xrtNetSocketListen(Socket, Config.Backlog) ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		pError = xrtTakeError();
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	pListener = (xnetlistener*)xrtCalloc(1, sizeof(*pListener));
	if ( pListener != NULL ) {
		pListener->AcceptSlots = (__xrt_net_accept_slot*)xrtCalloc(
			Config.AcceptConcurrency,
			sizeof(*pListener->AcceptSlots)
		);
	}
	if ( (pListener == NULL) || (pListener->AcceptSlots == NULL) ) {
		pError = xrtTakeError();
		xrtFree(pListener);
		__xrtNetEngineObjectRelease(pEngine);
		(void)xrtNetSocketClose(Socket);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	pListener->References = 2;
	xrtAtomic32Init(&pListener->State, XNET_LISTENER_OPEN);
	xrtAtomic64Init(&pListener->Accepted, 0);
	xrtAtomic64Init(&pListener->Rejected, 0);
	xrtAtomic64Init(&pListener->Errors, 0);
	xrtAtomic32Init(&pListener->ActiveAccepts, 0);
	xrtAtomic32Init(&pListener->ActiveDispatches, 0);
	xrtAtomic32Init(&pListener->QueuedAccepts, 0);
	xrtAtomic32Init(&pListener->PeakQueuedAccepts, 0);
	xrtAtomic32Init(&pListener->AcceptWaiters, 0);
	__xrtSpinInit(&pListener->AcceptLock);
	pListener->Engine = pEngine;
	pListener->Worker = pWorker;
	pListener->Socket = Socket;
	pListener->Config = Config;
	if ( pEvents != NULL ) {
		pListener->Events = *pEvents;
	}
	if ( pStreamEvents != NULL ) {
		pListener->StreamEvents = *pStreamEvents;
	}
	pListener->Data = pData;
	pListener->EngineHeld = true;
	pListener->RuntimeHeld = true;
	pListener->NextAffinity = Config.Affinity;
	xrtNetCompletionInit(
		&pListener->Completion,
		__xrtNetListenerCompletion,
		pListener
	);
	for ( uint32 i = 0; i < Config.AcceptConcurrency; i++ ) {
		pListener->AcceptSlots[i].Listener = pListener;
		xrtNetCompletionInit(
			&pListener->AcceptSlots[i].Completion,
			__xrtNetListenerCompletion,
			&pListener->AcceptSlots[i]
		);
	}
	if ( !xrtNetSocketLocal(Socket, &pListener->Local) ) {
		pError = xrtTakeError();
		pListener->Socket = NULL;
		(void)xrtNetSocketClose(Socket);
		xrtAtomic32Store(
			&pListener->State,
			XNET_LISTENER_CLOSED,
			XMEMORY_RELEASE
		);
		pListener->RuntimeHeld = false;
		xrtNetListenerDestroy(pListener);
		xrtNetListenerDestroy(pListener);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	pListener->StartPending = true;
	if ( !xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(pWorker),
		__xrtNetListenerStart,
		pListener
	) ) {
		pError = xrtTakeError();
		pListener->StartPending = false;
		pListener->Socket = NULL;
		(void)xrtNetSocketClose(Socket);
		xrtAtomic32Store(
			&pListener->State,
			XNET_LISTENER_CLOSED,
			XMEMORY_RELEASE
		);
		pListener->RuntimeHeld = false;
		xrtNetListenerDestroy(pListener);
		xrtNetListenerDestroy(pListener);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return pListener;
}



/* 在 Listener Worker 上取消观察和全部在途 Accept。 */
static void __xrtNetListenerCloseTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetlistener* pListener = (xnetlistener*)pData;
	xnetport* pPort = xrtNetWorkerPort(pWorker);

	if ( pListener->AcceptRetryTimer != 0 ) {
		uint64 Id = pListener->AcceptRetryTimer;

		pListener->AcceptRetryTimer = 0;
		if ( !__xrtNetEngineTimerCancelLifecycle(
			pListener->Engine,
			Id
		) ) {
			xrtClearError();
		}
	}
	__xrtNetListenerDiscardQueued(pListener);
	__xrtNetListenerNotifyFutures(pListener);
	if ( pListener->WatchPending ) {
		if ( !xrtNetPortUnwatch(pPort, pListener->Socket) ) {
			xrtClearError();
		}
		pListener->WatchPending = false;
		(void)xrtAtomic32FetchSub(
			&pListener->ActiveAccepts,
			1,
			XMEMORY_RELAXED
		);
	}
	for ( uint32 i = 0; i < pListener->Config.AcceptConcurrency; i++ ) {
		if ( pListener->AcceptSlots[i].Id != 0 ) {
			if ( !xrtNetPortCancel(
				pPort,
				pListener->AcceptSlots[i].Id
			) ) {
				xrtClearError();
			}
		}
	}
	__xrtNetListenerTryFinish(pListener);
	xrtNetListenerDestroy(pListener);
}



/* 请求 Listener 唯一关闭过程。 */
XRT_API bool xrtNetListenerClose(xnetlistener* pListener)
{
	uint32 iExpected = XNET_LISTENER_OPEN;

	if ( pListener == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pListener->State,
		&iExpected,
		XNET_LISTENER_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return iExpected != XNET_LISTENER_OPEN;
	}
	xrtNetListenerRef(pListener);
	if ( !__xrtNetEnginePostInternal(
		pListener->Worker,
		&pListener->CloseCommand,
		__xrtNetListenerCloseTask,
		pListener
	) ) {
		xrtAtomic32Store(
			&pListener->State,
			XNET_LISTENER_OPEN,
			XMEMORY_RELEASE
		);
		xrtNetListenerDestroy(pListener);
		__xrtNetSetError(
			XERR_CLOSED,
			XNET_ERROR_LISTENER_CLOSE,
			"close-listener",
			"network worker shutdown is sealed",
			0
		);
		return false;
	}
	return true;
}



/* 返回 Stream 状态。 */
XRT_API xnetstreamstate xrtNetStreamState(const xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return XNET_STREAM_CLOSED;
	}
	return (xnetstreamstate)xrtAtomic32Load(
		&pStream->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回 Listener 状态。 */
XRT_API xnetlistenerstate xrtNetListenerState(
	const xnetlistener* pListener
)
{
	if ( pListener == NULL ) {
		return XNET_LISTENER_CLOSED;
	}
	return (xnetlistenerstate)xrtAtomic32Load(
		&pListener->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回已占用发送预算。 */
XRT_API size_t xrtNetStreamPending(const xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return 0;
	}
	return (size_t)xrtAtomic64Load(
		&pStream->QueuedBytes,
		XMEMORY_ACQUIRE
	);
}



/* 返回不会随队列状态变化的发送硬上限。 */
XRT_API size_t xrtNetStreamWriteLimit(const xnetstream* pStream)
{
	return pStream != NULL ?
		pStream->Config.WriteLimit : 0;
}



/* 返回尚未被并发发送入口占用的发送硬预算。 */
XRT_API size_t xrtNetStreamWritable(const xnetstream* pStream)
{
	size_t iQueued;

	if ( pStream == NULL ) {
		return 0;
	}
	iQueued = xrtNetStreamPending(pStream);
	return iQueued < pStream->Config.WriteLimit ?
		pStream->Config.WriteLimit - iQueued : 0;
}



/* 返回接收缓冲字节数的并发快照。 */
XRT_API size_t xrtNetStreamAvailable(const xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return 0;
	}
	return (size_t)xrtAtomic64Load(
		&pStream->BufferedBytes,
		XMEMORY_ACQUIRE
	);
}



/* 在所属 Worker 上借用只读接收缓冲。 */
XRT_API const xnetbuf* xrtNetStreamBuffer(xnetstream* pStream)
{
	if ( (pStream == NULL) ||
		 !xrtNetWorkerIsCurrent(pStream->Worker) ||
		 !pStream->BuffersReady ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_READ,
			"get-stream-buffer",
			"stream buffer is only available on its worker"
		);
		return NULL;
	}
	return &pStream->ReadBuffer;
}



/* 在所属 Worker 上复制并消费接收字节。 */
XRT_API size_t xrtNetStreamRead(
	xnetstream* pStream,
	void* pOutput,
	size_t iSize
)
{
	size_t iRead;

	if ( (pStream == NULL) || ((pOutput == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( iSize == 0 ) {
		return 0;
	}
	if ( xrtNetStreamBuffer(pStream) == NULL ) {
		return 0;
	}
	iRead = xrtNetBufRead(&pStream->ReadBuffer, pOutput, iSize);
	__xrtNetStreamReadRefresh(pStream, true);
	__xrtNetStreamNotifyFutures(pStream, true);
	return iRead;
}



/* 在所属 Worker 上无复制消费接收字节。 */
XRT_API size_t xrtNetStreamConsume(xnetstream* pStream, size_t iSize)
{
	size_t iConsumed;

	if ( pStream == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( iSize == 0 ) {
		return 0;
	}
	if ( xrtNetStreamBuffer(pStream) == NULL ) {
		return 0;
	}
	iConsumed = xrtNetBufConsume(&pStream->ReadBuffer, iSize);
	__xrtNetStreamReadRefresh(pStream, true);
	__xrtNetStreamNotifyFutures(pStream, true);
	return iConsumed;
}



/* 复制已发布的 Stream 本地地址。 */
XRT_API bool xrtNetStreamLocal(
	const xnetstream* pStream,
	xnetaddr* pAddress
)
{
	if ( (pStream == NULL) || (pAddress == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (xrtNetStreamState(pStream) == XNET_STREAM_CONNECTING) ||
		 ((xrtNetStreamState(pStream) != XNET_STREAM_OPEN) &&
		  !pStream->OpenEmitted) ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_CONFIG,
			"get-stream-local",
			"TCP stream has no published local address"
		);
		return false;
	}
	*pAddress = pStream->Local;
	return true;
}



/* 复制 Stream 远端地址。 */
XRT_API bool xrtNetStreamRemote(
	const xnetstream* pStream,
	xnetaddr* pAddress
)
{
	if ( (pStream == NULL) || (pAddress == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAddress = pStream->Remote;
	return true;
}



/* 复制 Listener 实际绑定地址。 */
XRT_API bool xrtNetListenerLocal(
	const xnetlistener* pListener,
	xnetaddr* pAddress
)
{
	if ( (pListener == NULL) || (pAddress == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pAddress = pListener->Local;
	return true;
}



/* 返回 Stream 所属 Worker。 */
XRT_API xnetworker* xrtNetStreamWorker(const xnetstream* pStream)
{
	return pStream != NULL ? pStream->Worker : NULL;
}



/* 返回 Listener 所属 Worker。 */
XRT_API xnetworker* xrtNetListenerWorker(const xnetlistener* pListener)
{
	return pListener != NULL ? pListener->Worker : NULL;
}



/* 只在所属 Worker 回调中暴露借用 Socket。 */
XRT_API xnetsocket xrtNetStreamSocket(xnetstream* pStream)
{
	if ( (pStream == NULL) ||
		 !xrtNetWorkerIsCurrent(pStream->Worker) ||
		 (pStream->Socket == NULL) ||
		 (xrtNetStreamState(pStream) == XNET_STREAM_CLOSED) ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_CONFIG,
			"get-stream-socket",
			"stream socket is only available on its worker"
		);
		return NULL;
	}
	return pStream->Socket;
}



/* 只允许在所属 Worker 回调中替换 Stream 数据。 */
XRT_API bool xrtNetStreamSetData(xnetstream* pStream, ptr pData)
{
	if ( (pStream == NULL) || !xrtNetWorkerIsCurrent(pStream->Worker) ) {
		__xrtNetStreamSetError(
			XERR_STATE,
			XNET_ERROR_STREAM_CONFIG,
			"set-stream-data",
			"stream data can only be changed on its worker"
		);
		return false;
	}
	xrtAtomicPtrStore(&pStream->Data, pData, XMEMORY_RELEASE);
	return true;
}



/* 返回 Stream 用户数据。 */
XRT_API ptr xrtNetStreamData(const xnetstream* pStream)
{
	return pStream != NULL ? xrtAtomicPtrLoad(
		&pStream->Data,
		XMEMORY_ACQUIRE
	) : NULL;
}



/* 返回 Listener 用户数据。 */
XRT_API ptr xrtNetListenerData(const xnetlistener* pListener)
{
	return pListener != NULL ? pListener->Data : NULL;
}



/* 返回 Stream 终止错误。 */
XRT_API const xerror* xrtNetStreamError(const xnetstream* pStream)
{
	if ( (pStream == NULL) ||
		 (xrtNetStreamState(pStream) != XNET_STREAM_CLOSED) ) {
		return NULL;
	}
	return pStream->Error;
}



/* 复制 Stream 并发统计。 */
XRT_API bool xrtNetStreamStats(
	const xnetstream* pStream,
	xnetstreamstats* pStats
)
{
	if ( (pStream == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetStreamState(pStream);
	pStats->ReceivedBytes = xrtAtomic64Load(
		&pStream->ReceivedBytes,
		XMEMORY_RELAXED
	);
	pStats->SentBytes = xrtAtomic64Load(
		&pStream->SentBytes,
		XMEMORY_RELAXED
	);
	pStats->ReadEvents = xrtAtomic64Load(
		&pStream->ReadEvents,
		XMEMORY_RELAXED
	);
	pStats->WriteEvents = xrtAtomic64Load(
		&pStream->WriteEvents,
		XMEMORY_RELAXED
	);
	pStats->SendRejected = xrtAtomic64Load(
		&pStream->SendRejected,
		XMEMORY_RELAXED
	);
	pStats->BufferedBytes = xrtNetStreamAvailable(pStream);
	pStats->QueuedBytes = xrtNetStreamPending(pStream);
	pStats->PeakQueuedBytes = (size_t)xrtAtomic64Load(
		&pStream->PeakQueuedBytes,
		XMEMORY_RELAXED
	);
	pStats->ReadPaused = xrtAtomic32Load(
		&pStream->ReadPaused,
		XMEMORY_ACQUIRE
	) != 0;
	pStats->ReadBlocked = xrtAtomic32Load(
		&pStream->ReadBlocked,
		XMEMORY_ACQUIRE
	) != 0;
	pStats->ReadEnded = xrtAtomic32Load(
		&pStream->ReadEnded,
		XMEMORY_ACQUIRE
	) != 0;
	pStats->WriteEnded = xrtAtomic32Load(
		&pStream->WriteEnded,
		XMEMORY_ACQUIRE
	) != 0;
	pStats->WriteBackpressured = xrtAtomic32Load(
		&pStream->WriteBackpressured,
		XMEMORY_ACQUIRE
	) != 0;
	return true;
}



/* 复制 Listener 并发统计。 */
XRT_API bool xrtNetListenerStats(
	const xnetlistener* pListener,
	xnetlistenerstats* pStats
)
{
	if ( (pListener == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetListenerState(pListener);
	pStats->Accepted = xrtAtomic64Load(
		&pListener->Accepted,
		XMEMORY_RELAXED
	);
	pStats->Rejected = xrtAtomic64Load(
		&pListener->Rejected,
		XMEMORY_RELAXED
	);
	pStats->Errors = xrtAtomic64Load(
		&pListener->Errors,
		XMEMORY_RELAXED
	);
	pStats->ActiveAccepts = xrtAtomic32Load(
		&pListener->ActiveAccepts,
		XMEMORY_ACQUIRE
	);
	pStats->ActiveDispatches = xrtAtomic32Load(
		&pListener->ActiveDispatches,
		XMEMORY_ACQUIRE
	);
	pStats->QueuedAccepts = xrtAtomic32Load(
		&pListener->QueuedAccepts,
		XMEMORY_ACQUIRE
	);
	pStats->PeakQueuedAccepts = xrtAtomic32Load(
		&pListener->PeakQueuedAccepts,
		XMEMORY_RELAXED
	);
	pStats->AcceptWaiters = xrtAtomic32Load(
		&pListener->AcceptWaiters,
		XMEMORY_ACQUIRE
	);
	return true;
}

#endif
