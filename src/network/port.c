#include "../internal/xrt_net_port.h"



#if defined(XRT_FEATURE_NET_PORT)

#define XRT_NET_PORT_POST_DEFAULT 4096u
#define XRT_NET_PORT_WATCH_DEFAULT 4096u
#define XRT_NET_PORT_WATCH_SCALABLE_DEFAULT 65536u
#define XRT_NET_PORT_WATCH_SELECT_DEFAULT 1024u
#define XRT_NET_PORT_OPERATION_DEFAULT 4096u
#define XRT_NET_PORT_OPERATION_IOCP_DEFAULT 65536u
#define XRT_NET_PORT_OPERATION_CACHE_DEFAULT 64u



static xatomic64 __xrtNetPortOwnerNext = XRT_ATOMIC64_INIT(0);



/* 分配跨端口地址复用周期的唯一 owner 标识。 */
static uint64 __xrtNetPortOwnerNextId(void)
{
	uint64 Id;

	do {
		Id = xrtAtomic64FetchAdd(
			&__xrtNetPortOwnerNext,
			1,
			XMEMORY_RELAXED
		) + 1u;
	} while ( Id == 0 );
	return Id;
}



/* 检查后端操作是否由端口当前拥有线程执行。 */
static bool __xrtNetPortRequireThread(
	xnetport* pPort,
	uint32 iCode,
	cstr sOperation
)
{
	if ( xrtAtomic64Load(
		&pPort->OwnerThread,
		XMEMORY_ACQUIRE
	) != __xrtCurrentThreadId() ) {
		__xrtNetSetError(
			XERR_STATE,
			iCode,
			sOperation,
			"network port operation must run on its owner thread",
			0
		);
		return false;
	}
	return true;
}

/* 检查端口后端枚举是否属于当前稳定集合。 */
static bool __xrtNetPortBackendValid(xnetportbackend Backend)
{
	return (Backend >= XNET_PORT_AUTO) && (Backend <= XNET_PORT_SELECT);
}



/* 按显式配置或平台优先级选择已经编译的后端。 */
static const __xrt_net_port_driver* __xrtNetPortDriver(
	xnetportbackend Backend)
{
	#if defined(XRT_FEATURE_NET_PORT_IOCP) && \
		(defined(_WIN32) || defined(_WIN64))
		if ( (Backend == XNET_PORT_AUTO) ||
			 (Backend == XNET_PORT_IOCP) ) {
			return __xrtNetPortIOCPDriver();
		}
	#endif
	#if defined(XRT_FEATURE_NET_PORT_EPOLL) && defined(__linux__)
		if ( (Backend == XNET_PORT_AUTO) ||
			 (Backend == XNET_PORT_EPOLL) ) {
			return __xrtNetPortEpollDriver();
		}
	#endif
	#if defined(XRT_FEATURE_NET_PORT_URING) && defined(__linux__)
		if ( Backend == XNET_PORT_URING ) {
			return __xrtNetPortUringDriver();
		}
	#endif
	#if defined(XRT_FEATURE_NET_PORT_KQUEUE) && \
		(defined(__APPLE__) || defined(__FreeBSD__) || \
		 defined(__OpenBSD__) || defined(__NetBSD__) || \
		 defined(__DragonFly__))
		if ( (Backend == XNET_PORT_AUTO) ||
			 (Backend == XNET_PORT_KQUEUE) ) {
			return __xrtNetPortKqueueDriver();
		}
	#endif
	#if defined(XRT_FEATURE_NET_PORT_SELECT)
		if ( (Backend == XNET_PORT_AUTO) ||
			 (Backend == XNET_PORT_SELECT) ) {
			return __xrtNetPortSelectDriver();
		}
	#else
		(void)Backend;
	#endif
	return NULL;
}



/* 把零值容量解析为实际后端适合的默认值，并保留显式硬边界。 */
static void __xrtNetPortConfigResolve(
	xnetportconfig* pConfig,
	xnetportbackend Backend
)
{
	pConfig->Backend = Backend;
	if ( pConfig->WatchLimit == 0 ) {
		if ( (Backend == XNET_PORT_EPOLL) ||
			 (Backend == XNET_PORT_KQUEUE) ) {
			pConfig->WatchLimit = XRT_NET_PORT_WATCH_SCALABLE_DEFAULT;
		} else if ( Backend == XNET_PORT_SELECT ) {
			pConfig->WatchLimit = XRT_NET_PORT_WATCH_SELECT_DEFAULT;
		} else {
			pConfig->WatchLimit = XRT_NET_PORT_WATCH_DEFAULT;
		}
	}
	if ( pConfig->OperationLimit == 0 ) {
		pConfig->OperationLimit = Backend == XNET_PORT_IOCP ?
			XRT_NET_PORT_OPERATION_IOCP_DEFAULT :
			XRT_NET_PORT_OPERATION_DEFAULT;
	}
}



/* 释放尚未提取的用户事件节点。 */
static void __xrtNetPortClearPosts(xnetport* pPort)
{
	__xrt_net_port_post* pPost = pPort->PostHead;

	pPort->PostHead = NULL;
	pPort->PostTail = NULL;
	pPort->PostCount = 0;
	pPort->NotifyPending = false;
	pPort->WakePending = false;

	while ( pPost != NULL ) {
		__xrt_net_port_post* pNext = pPost->Next;

		xrtFree(pPost);
		pPost = pNext;
	}
}



/* 提取用户事件和一个可合并唤醒事件，并报告队列是否仍有积压。 */
static bool __xrtNetPortDrainPosts(
	xnetport* pPort,
	xnetportevent* pEvents,
	size_t iCapacity,
	size_t* pCount,
	bool* pPending
)
{
	size_t iCount = 0;

	*pCount = 0;
	*pPending = false;
	if ( !xrtMutexLock(&pPort->Lock) ) {
		return false;
	}
	while ( (iCount < iCapacity) && (pPort->PostHead != NULL) ) {
		__xrt_net_port_post* pPost = pPort->PostHead;
		xnetportevent* pEvent = &pEvents[iCount++];

		pPort->PostHead = pPost->Next;
		if ( pPort->PostHead == NULL ) {
			pPort->PostTail = NULL;
		}
		pPort->PostCount--;
		memset(pEvent, 0, sizeof(*pEvent));
		pEvent->Type = XNET_PORT_EVENT_USER;
		pEvent->Result = XNET_RESULT_OK;
		pEvent->Id = pPost->Id;
		pEvent->User = pPost->User;
		xrtFree(pPost);
	}
	if ( (iCount < iCapacity) && pPort->WakePending ) {
		xnetportevent* pEvent = &pEvents[iCount++];

		pPort->WakePending = false;
		memset(pEvent, 0, sizeof(*pEvent));
		pEvent->Type = XNET_PORT_EVENT_WAKE;
		pEvent->Result = XNET_RESULT_OK;
	}
	if ( (pPort->PostHead == NULL) && !pPort->WakePending ) {
		pPort->NotifyPending = false;
	}
	*pPending = (pPort->PostHead != NULL) || pPort->WakePending;
	(void)xrtMutexUnlock(&pPort->Lock);
	*pCount = iCount;
	return true;
}



/* 初始化网络端口默认配置。 */
XRT_API void xrtNetPortConfigInit(xnetportconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Backend = XNET_PORT_AUTO;
	pConfig->PostLimit = XRT_NET_PORT_POST_DEFAULT;
	pConfig->WatchLimit = 0;
	pConfig->OperationLimit = 0;
	pConfig->OperationCache = XRT_NET_PORT_OPERATION_CACHE_DEFAULT;
}



/* 创建事件端口；AUTO 在当前已编译后端中选择最高能力实现。 */
XRT_API xnetport* xrtNetPortCreate(const xnetportconfig* pConfig)
{
	xnetportconfig Config;
	const __xrt_net_port_driver* pDriver;
	xnetport* pPort;

	xrtNetPortConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	if ( !__xrtNetPortBackendValid(Config.Backend) ||
		 (Config.Flags != 0) || (Config.PostLimit == 0) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_CREATE,
			"create-port", "invalid network port configuration", 0);
		return NULL;
	}
	pDriver = __xrtNetPortDriver(Config.Backend);
	if ( pDriver == NULL ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_CREATE,
			"create-port", "requested network port backend is unavailable", 0);
		return NULL;
	}
	__xrtNetPortConfigResolve(&Config, pDriver->Backend);

	pPort = (xnetport*)xrtMalloc(sizeof(*pPort));
	if ( pPort == NULL ) {
		return NULL;
	}
	memset(pPort, 0, sizeof(*pPort));
	pPort->Config = Config;
	pPort->Driver = pDriver;
	pPort->Capabilities = pDriver->Capabilities;
	pPort->Owner = __xrtNetPortOwnerNextId();
	xrtAtomic64Init(&pPort->OwnerThread, __xrtCurrentThreadId());
	xrtAtomic32Init(&pPort->Closing, 0u);
	if ( !xrtMutexInit(&pPort->Lock) ) {
		xrtFree(pPort);
		return NULL;
	}
	if ( !pPort->Driver->Init(pPort) ) {
		(void)xrtMutexUnit(&pPort->Lock);
		xrtFree(pPort);
		return NULL;
	}
	return pPort;
}



/* 销毁端口和全部观察、用户事件及唤醒资源。 */
XRT_API bool xrtNetPortDestroy(xnetport* pPort)
{
	xerror* pPrevious;
	xerror* pFailure = NULL;
	xerror* pError;
	uint32 iExpected = 0u;
	bool bResult;

	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_CLOSE,
		"destroy-port"
	) ) {
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pPort->Closing,
		&iExpected,
		1u,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pPrevious = __xrtErrorSwapOwned(NULL);
	bResult = pPort->Driver->Unit(pPort);
	if ( !bResult ) {
		pFailure = __xrtErrorSwapOwned(NULL);
	}
	__xrtNetPortClearPosts(pPort);
	if ( !xrtMutexUnit(&pPort->Lock) ) {
		bResult = false;
		pError = __xrtErrorSwapOwned(NULL);
		if ( pFailure == NULL ) {
			pFailure = pError;
		} else {
			xrtErrorFree(pError);
		}
	}
	xrtFree(pPort);
	if ( bResult ) {
		pError = __xrtErrorSwapOwned(pPrevious);
		xrtErrorFree(pError);
		return true;
	}
	xrtErrorFree(pPrevious);
	if ( pFailure != NULL ) {
		pError = __xrtErrorSwapOwned(pFailure);
		xrtErrorFree(pError);
	} else {
		__xrtNetSetError(
			XERR_IO,
			XNET_ERROR_PORT_CLOSE,
			"destroy-port",
			"closing network port failed",
			0
		);
	}
	return false;
}



/* 返回实际启用的后端。 */
XRT_API xnetportbackend xrtNetPortBackend(const xnetport* pPort)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_PORT_AUTO;
	}
	return pPort->Driver->Backend;
}



/* 返回已经解析 AUTO 容量和实际后端的有效配置。 */
XRT_API bool xrtNetPortGetConfig(
	const xnetport* pPort,
	xnetportconfig* pConfig
)
{
	if ( (pPort == NULL) || (pConfig == NULL) ||
		 (pPort->Driver == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pConfig = pPort->Config;
	return true;
}



/* 返回静态后端名称。 */
XRT_API cstr xrtNetPortName(const xnetport* pPort)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return pPort->Driver->Name;
}



/* 返回实际后端能力位。 */
XRT_API uint32 xrtNetPortCapabilities(const xnetport* pPort)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	return pPort->Capabilities;
}



/* 返回端口不可复用的文件归属标识。 */
uint64 __xrtNetPortOwner(const xnetport* pPort)
{
	return pPort != NULL ? pPort->Owner : 0;
}



/* 当前拥有线程释放端口，供尚未启动的 Engine Worker 认领。 */
bool __xrtNetPortThreadRelease(xnetport* pPort)
{
	uint64 iExpected;

	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iExpected = __xrtCurrentThreadId();
	if ( !xrtAtomic64CompareExchange(
		&pPort->OwnerThread,
		&iExpected,
		0,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_PORT_CREATE,
			"release-port-thread",
			"network port is not owned by the current thread",
			0
		);
		return false;
	}
	return true;
}



/* 当前线程认领一个无归属端口；重复认领自身是幂等操作。 */
bool __xrtNetPortThreadClaim(xnetport* pPort)
{
	uint64 iCurrent;
	uint64 iExpected = 0;

	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iCurrent = __xrtCurrentThreadId();
	if ( xrtAtomic64CompareExchange(
		&pPort->OwnerThread,
		&iExpected,
		iCurrent,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) || (iExpected == iCurrent) ) {
		return true;
	}
	__xrtNetSetError(
		XERR_STATE,
		XNET_ERROR_PORT_CREATE,
		"claim-port-thread",
		"network port is owned by another thread",
		0
	);
	return false;
}



/* 校验完成式端口、Socket 类别和非零操作标识。 */
static bool __xrtNetPortCompletion(xnetport* pPort,
	xnetsocket Socket, xnetsockettype Type, uint64 Id, cstr sOperation)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (Socket == NULL) || (Id == 0) ||
		 ((Type != 0) && (Socket->Type != Type)) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			sOperation, "invalid network completion submission", 0);
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_SUBMIT,
		sOperation
	) ) {
		return false;
	}
	if ( ((pPort->Capabilities & XNET_PORT_CAP_COMPLETION) == 0) ||
		 (pPort->Driver->Submit == NULL) ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_SUBMIT,
			sOperation, "network port backend has no completion capability", 0);
		return false;
	}
	return true;
}



/* 校验不依赖 Socket 的完成式文件操作。 */
static bool __xrtNetPortFileCompletion(
	xnetport* pPort,
	intptr_t iFile,
	uint64 Id,
	cstr sOperation
)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (iFile == (intptr_t)-1) || (Id == 0) ) {
		__xrtNetSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PORT_SUBMIT,
			sOperation,
			"invalid native file completion submission",
			0
		);
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_SUBMIT,
		sOperation
	) ) {
		return false;
	}
	if ( ((pPort->Capabilities & XNET_PORT_CAP_FILE_IO) == 0) ||
		 (pPort->Driver->Submit == NULL) ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			sOperation,
			"network port backend has no native file I/O capability",
			0
		);
		return false;
	}
	return true;
}



/* 校验原生定位文件请求在所有支持平台上的公共有符号范围。 */
static bool __xrtNetPortFileRange(
	uint64 iOffset,
	size_t iSize,
	cstr sOperation
)
{
	if ( (iOffset > (uint64)INT64_MAX) ||
		((iSize != 0u) &&
		 ((uint64)(iSize - 1u) > ((uint64)INT64_MAX - iOffset))) ) {
		__xrtNetSetError(
			XERR_RANGE,
			XNET_ERROR_PORT_SUBMIT,
			sOperation,
			"native file operation range is too large",
			0
		);
		return false;
	}
	return true;
}



/* 校验可写 Span 描述符和单次完成事件可表达的总长度。 */
static bool __xrtNetPortReadSpans(const xnetwspan* pSpans,
	size_t iCount, bool bRequireData, cstr sOperation)
{
	uint64 iTotal = 0;

	if ( (pSpans == NULL) || (iCount == 0) ||
		 (iCount > XRT_NET_SOCKET_VECTOR_LIMIT) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			sOperation, "invalid network receive spans", 0);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( ((pSpans[i].Data == NULL) && (pSpans[i].Size != 0)) ||
		 (pSpans[i].Size > INT_MAX) ) {
			__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
				sOperation, "invalid network receive span", 0);
			return false;
		}
		iTotal += (uint64)pSpans[i].Size;
		if ( iTotal > INT_MAX ) {
			__xrtNetSetError(XERR_RANGE, XNET_ERROR_PORT_SUBMIT,
				sOperation, "network receive span total is too large", 0);
			return false;
		}
	}
	if ( bRequireData && (iTotal == 0) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			sOperation, "stream receive requires a non-empty buffer", 0);
		return false;
	}
	return true;
}



/* 校验只读 Span 描述符和单次完成事件可表达的总长度。 */
static bool __xrtNetPortWriteSpans(
	const xnetspan* pSpans,
	size_t iCount,
	size_t* pTotal,
	cstr sOperation
)
{
	uint64 iTotal = 0;

	if ( pTotal != NULL ) {
		*pTotal = 0;
	}

	if ( (pSpans == NULL) || (iCount == 0) ||
		 (iCount > XRT_NET_SOCKET_VECTOR_LIMIT) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			sOperation, "invalid network send spans", 0);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( ((pSpans[i].Data == NULL) && (pSpans[i].Size != 0)) ||
		 (pSpans[i].Size > INT_MAX) ) {
			__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
				sOperation, "invalid network send span", 0);
			return false;
		}
		iTotal += (uint64)pSpans[i].Size;
		if ( iTotal > INT_MAX ) {
			__xrtNetSetError(XERR_RANGE, XNET_ERROR_PORT_SUBMIT,
				sOperation, "network send span total is too large", 0);
			return false;
		}
	}
	if ( pTotal != NULL ) {
		*pTotal = (size_t)iTotal;
	}
	return true;
}



/* 提交一项已经完成公共校验的异步操作。 */
static bool __xrtNetPortSubmit(xnetport* pPort,
	const __xrt_net_port_submit* pSubmit)
{
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_SUBMIT,
		"submit"
	) ) {
		return false;
	}
	return pPort->Driver->Submit(pPort, pSubmit);
}



/* 替换一个 Socket 的 readiness 关注位；事件为零等价于 Unwatch。 */
XRT_API bool xrtNetPortWatch(xnetport* pPort, xnetsocket Socket,
	uint64 Id, uint32 iEvents, ptr pUser)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (Socket == NULL) ||
		 ((iEvents & ~((uint32)XNET_POLL_READ |
			(uint32)XNET_POLL_WRITE)) != 0) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_WATCH,
			"watch", "invalid network port watch", 0);
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_WATCH,
		"watch"
	) ) {
		return false;
	}
	if ( (pPort->Capabilities & XNET_PORT_CAP_READINESS) == 0 ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_WATCH,
			"watch", "network port backend has no readiness capability", 0);
		return false;
	}
	if ( iEvents == 0 ) {
		return pPort->Driver->Unwatch(pPort, Socket);
	}
	return pPort->Driver->Watch(pPort,
		Socket, Id, iEvents, pUser);
}



/* 幂等移除一个 Socket 的 readiness 观察。 */
XRT_API bool xrtNetPortUnwatch(xnetport* pPort, xnetsocket Socket)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (Socket == NULL) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_WATCH,
			"unwatch", "invalid network port watch", 0);
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_WATCH,
		"unwatch"
	) ) {
		return false;
	}
	if ( (pPort->Capabilities & XNET_PORT_CAP_READINESS) == 0 ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_WATCH,
			"unwatch", "network port backend has no readiness capability", 0);
		return false;
	}
	return pPort->Driver->Unwatch(pPort, Socket);
}



/* 异步接受一个连接。 */
XRT_API bool xrtNetPortAccept(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		XNET_SOCKET_STREAM, Id, "accept") ) {
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_ACCEPT;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步连接远端地址。 */
XRT_API bool xrtNetPortConnect(xnetport* pPort, xnetsocket Socket,
	const xnetaddr* pRemote, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		XNET_SOCKET_STREAM, Id, "connect") ) {
		return false;
	}
	if ( (pRemote == NULL) || (pRemote->Family != Socket->Family) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			"connect", "network connect address family mismatch", 0);
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_CONNECT;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.Address = pRemote;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步等待流 Socket 可读，不占用调用方接收缓冲。 */
XRT_API bool xrtNetPortReadProbe(xnetport* pPort,
	xnetsocket Socket, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(
		pPort,
		Socket,
		XNET_SOCKET_STREAM,
		Id,
		"read-probe"
	) ) {
		return false;
	}
	if ( (pPort->Capabilities &
		 XNET_PORT_CAP_READ_PROBE) == 0 ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			"read-probe",
			"network port backend cannot probe stream readability",
			0
		);
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_READ_PROBE;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步接收到一个调用方缓冲。 */
XRT_API bool xrtNetPortRecv(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser)
{
	xnetwspan Span;

	Span.Data = (bytes)pData;
	Span.Size = iSize;
	return xrtNetPortRecvVec(pPort, Socket, &Span, 1, Id, pUser);
}



/* 异步分散接收到调用方缓冲。 */
XRT_API bool xrtNetPortRecvVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		(xnetsockettype)0, Id, "recv") ||
		 !__xrtNetPortReadSpans(pSpans, iCount,
			Socket->Type == XNET_SOCKET_STREAM, "recv") ) {
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_RECV;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.ReadSpans = pSpans;
	Submit.SpanCount = iCount;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步发送一个调用方缓冲。 */
XRT_API bool xrtNetPortSend(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, uint64 Id, ptr pUser)
{
	xnetspan Span;

	Span.Data = (cbytes)pData;
	Span.Size = iSize;
	return xrtNetPortSendVec(pPort, Socket, &Span, 1, Id, pUser);
}



/* 异步聚集发送调用方缓冲。 */
XRT_API bool xrtNetPortSendVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		(xnetsockettype)0, Id, "send") ||
		 !__xrtNetPortWriteSpans(pSpans, iCount, NULL, "send") ) {
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_SEND;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.WriteSpans = pSpans;
	Submit.SpanCount = iCount;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 从绝对偏移向调用方缓冲提交一次原生异步读取。 */
bool __xrtNetPortFileRead(
	xnetport* pPort,
	intptr_t iFile,
	uint64 iOffset,
	void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser,
	bool* pAssociated
)
{
	__xrt_net_port_submit Submit;
	xnetwspan Span;

	if ( !__xrtNetPortFileCompletion(
		pPort,
		iFile,
		Id,
		"read-file"
	) || !__xrtNetPortReadSpans(
		&(xnetwspan) { (bytes)pData, iSize },
		1,
		true,
		"read-file"
	) || !__xrtNetPortFileRange(
		iOffset,
		iSize,
		"read-file"
	) ) {
		return false;
	}
	Span.Data = (bytes)pData;
	Span.Size = iSize;
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_FILE_READ;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.ReadSpans = &Span;
	Submit.SpanCount = 1;
	Submit.File = iFile;
	Submit.FileOffset = iOffset;
	Submit.FileAssociated = pAssociated;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 从调用方缓冲向绝对偏移提交一次原生异步写入。 */
bool __xrtNetPortFileWrite(
	xnetport* pPort,
	intptr_t iFile,
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser,
	bool* pAssociated
)
{
	__xrt_net_port_submit Submit;
	xnetspan Span;

	if ( !__xrtNetPortFileCompletion(
		pPort,
		iFile,
		Id,
		"write-file"
	) || !__xrtNetPortWriteSpans(
		&(xnetspan) { (cbytes)pData, iSize },
		1,
		NULL,
		"write-file"
	) || !__xrtNetPortFileRange(
		iOffset,
		iSize,
		"write-file"
	) || (iSize == 0) ) {
		if ( iSize == 0 ) {
			__xrtNetSetError(
				XERR_ARGUMENT,
				XNET_ERROR_PORT_SUBMIT,
				"write-file",
				"native file write requires a non-empty buffer",
				0
			);
		}
		return false;
	}
	Span.Data = (cbytes)pData;
	Span.Size = iSize;
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_FILE_WRITE;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.WriteSpans = &Span;
	Submit.SpanCount = 1;
	Submit.File = iFile;
	Submit.FileOffset = iOffset;
	Submit.FileAssociated = pAssociated;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 借用文件句柄直到唯一终态，用完成后端发送一个文件区间。 */
bool __xrtNetPortSendFile(
	xnetport* pPort,
	xnetsocket Socket,
	intptr_t iFile,
	uint64 iOffset,
	size_t iSize,
	uint64 Id,
	ptr pUser
)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(
		pPort,
		Socket,
		XNET_SOCKET_STREAM,
		Id,
		"send-file"
	) ) {
		return false;
	}
	if ( (iFile == (intptr_t)-1) || (iSize == 0) ||
		(iSize > (size_t)INT_MAX) ||
		(iOffset > (UINT64_MAX - (uint64)iSize)) ||
		((pPort->Capabilities & XNET_PORT_CAP_SEND_FILE) == 0) ) {
		__xrtNetSetError(
			((pPort->Capabilities & XNET_PORT_CAP_SEND_FILE) == 0) ?
				XERR_UNSUPPORTED :
			((iSize > (size_t)INT_MAX) ||
			 (iOffset > (UINT64_MAX - (uint64)iSize))) ?
				XERR_RANGE : XERR_ARGUMENT,
			XNET_ERROR_PORT_SUBMIT,
			"send-file",
			((pPort->Capabilities & XNET_PORT_CAP_SEND_FILE) == 0) ?
				"network port backend cannot send file ranges" :
			((iSize > (size_t)INT_MAX) ||
			 (iOffset > (UINT64_MAX - (uint64)iSize))) ?
				"network file send range is too large" :
				"invalid network file send",
			0
		);
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_SEND_FILE;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.File = iFile;
	Submit.FileOffset = iOffset;
	Submit.FileSize = iSize;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步接收一个数据报。 */
XRT_API bool xrtNetPortRecvFrom(xnetport* pPort, xnetsocket Socket,
	void* pData, size_t iSize, uint64 Id, ptr pUser)
{
	xnetwspan Span;

	Span.Data = (bytes)pData;
	Span.Size = iSize;
	return xrtNetPortRecvFromVec(pPort, Socket, &Span, 1, Id, pUser);
}



/* 异步分散接收一个数据报。 */
XRT_API bool xrtNetPortRecvFromVec(xnetport* pPort, xnetsocket Socket,
	const xnetwspan* pSpans, size_t iCount, uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		XNET_SOCKET_DGRAM, Id, "recv-from") ||
		 !__xrtNetPortReadSpans(pSpans, iCount, false, "recv-from") ) {
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_RECV_FROM;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.ReadSpans = pSpans;
	Submit.SpanCount = iCount;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步接收一个数据报及 Socket 已启用的元数据。 */
XRT_API bool xrtNetPortRecvMsg(
	xnetport* pPort,
	xnetsocket Socket,
	void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser
)
{
	xnetwspan Span;

	Span.Data = (bytes)pData;
	Span.Size = iSize;
	return xrtNetPortRecvMsgVec(
		pPort,
		Socket,
		&Span,
		1,
		Id,
		pUser
	);
}



/* 异步分散接收一个数据报及控制消息。 */
XRT_API bool xrtNetPortRecvMsgVec(
	xnetport* pPort,
	xnetsocket Socket,
	const xnetwspan* pSpans,
	size_t iCount,
	uint64 Id,
	ptr pUser
)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(
		pPort,
		Socket,
		XNET_SOCKET_DGRAM,
		Id,
		"recv-message"
	) || !__xrtNetPortReadSpans(
		pSpans,
		iCount,
		false,
		"recv-message"
	) ) {
		return false;
	}
	if ( Socket->DgramMeta == 0 ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_PORT_SUBMIT,
			"recv-message",
			"datagram metadata is not enabled on the socket",
			0
		);
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_RECV_MSG;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.ReadSpans = pSpans;
	Submit.SpanCount = iCount;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步等待并读取一个 Linux 数据报错误队列条目。 */
XRT_API bool xrtNetPortRecvError(
	xnetport* pPort,
	xnetsocket Socket,
	void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser
)
{
	__xrt_net_port_submit Submit;
	xnetwspan Span;
	int64 iEnabled;

	Span.Data = (bytes)pData;
	Span.Size = iSize;
	if ( !__xrtNetPortCompletion(
		pPort,
		Socket,
		XNET_SOCKET_DGRAM,
		Id,
		"receive-datagram-error"
	) || !__xrtNetPortReadSpans(
		&Span,
		1,
		false,
		"receive-datagram-error"
	) ) {
		return false;
	}
	if ( (pPort->Capabilities &
		 XNET_PORT_CAP_DGRAM_ERROR) == 0 ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			"receive-datagram-error",
			"network port backend cannot wait for datagram errors",
			0
		);
		return false;
	}
	if ( (xrtNetSocketDgramCapabilities(Socket) &
		 XNET_DGRAM_CAP_ERROR_QUEUE) == 0 ) {
		__xrtNetSetError(
			XERR_UNSUPPORTED,
			XNET_ERROR_PORT_SUBMIT,
			"receive-datagram-error",
			"datagram error queue is not supported by the socket",
			0
		);
		return false;
	}
	if ( !xrtNetSocketGet(Socket, XNET_OPTION_DGRAM_ERRORS, &iEnabled) ) {
		return false;
	}
	if ( iEnabled == 0 ) {
		__xrtNetSetError(
			XERR_STATE,
			XNET_ERROR_PORT_SUBMIT,
			"receive-datagram-error",
			"datagram error queue is not enabled on the socket",
			0
		);
		return false;
	}

	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_RECV_ERROR;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.ReadSpans = &Span;
	Submit.SpanCount = 1;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步发送一个数据报。 */
XRT_API bool xrtNetPortSendTo(xnetport* pPort, xnetsocket Socket,
	const void* pData, size_t iSize, const xnetaddr* pRemote,
	uint64 Id, ptr pUser)
{
	xnetspan Span;

	Span.Data = (cbytes)pData;
	Span.Size = iSize;
	return xrtNetPortSendToVec(pPort, Socket,
		&Span, 1, pRemote, Id, pUser);
}



/* 异步聚集发送一个数据报。 */
XRT_API bool xrtNetPortSendToVec(xnetport* pPort, xnetsocket Socket,
	const xnetspan* pSpans, size_t iCount, const xnetaddr* pRemote,
	uint64 Id, ptr pUser)
{
	__xrt_net_port_submit Submit;

	if ( !__xrtNetPortCompletion(pPort, Socket,
		XNET_SOCKET_DGRAM, Id, "send-to") ||
		 !__xrtNetPortWriteSpans(pSpans, iCount, NULL, "send-to") ) {
		return false;
	}
	if ( (pRemote == NULL) || (pRemote->Family != Socket->Family) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_SUBMIT,
			"send-to", "network datagram address family mismatch", 0);
		return false;
	}
	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_SEND_TO;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.WriteSpans = pSpans;
	Submit.SpanCount = iCount;
	Submit.Address = pRemote;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 异步发送一个带逐包控制的数据报。 */
XRT_API bool xrtNetPortSendMsg(
	xnetport* pPort,
	xnetsocket Socket,
	const void* pData,
	size_t iSize,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	uint64 Id,
	ptr pUser
)
{
	xnetspan Span;

	Span.Data = (cbytes)pData;
	Span.Size = iSize;
	return xrtNetPortSendMsgVec(
		pPort,
		Socket,
		&Span,
		1,
		pRemote,
		pControl,
		Id,
		pUser
	);
}



/* 异步聚集发送一个带逐包控制的数据报。 */
XRT_API bool xrtNetPortSendMsgVec(
	xnetport* pPort,
	xnetsocket Socket,
	const xnetspan* pSpans,
	size_t iCount,
	const xnetaddr* pRemote,
	const xnetdgramcontrol* pControl,
	uint64 Id,
	ptr pUser
)
{
	__xrt_net_port_submit Submit;
	union {
		uint64 Align;
		unsigned char Data[XRT_NET_SOCKET_DGRAM_CONTROL_SIZE];
	} ControlBuffer;
	size_t iPayload = 0;
	size_t iControlSize;

	if ( (pControl == NULL) || (pControl->Flags == 0) ) {
		return (pRemote != NULL) ? xrtNetPortSendToVec(
			pPort, Socket, pSpans, iCount, pRemote, Id, pUser
		) : xrtNetPortSendVec(
			pPort, Socket, pSpans, iCount, Id, pUser
		);
	}
	if ( !__xrtNetPortCompletion(
		pPort,
		Socket,
		XNET_SOCKET_DGRAM,
		Id,
		"send-message"
	) || !__xrtNetPortWriteSpans(
		pSpans,
		iCount,
		&iPayload,
		"send-message"
	) ) {
		return false;
	}
	if ( (pRemote != NULL) && (pRemote->Family != Socket->Family) ) {
		__xrtNetSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PORT_SUBMIT,
			"send-message",
			"network datagram address family mismatch",
			0
		);
		return false;
	}
	if ( !__xrtNetSocketDgramControlBuild(
		Socket,
		pControl,
		iPayload,
		ControlBuffer.Data,
		sizeof(ControlBuffer.Data),
		&iControlSize,
		XNET_ERROR_PORT_SUBMIT,
		"send-message"
	) ) {
		return false;
	}
	(void)iControlSize;

	memset(&Submit, 0, sizeof(Submit));
	Submit.Type = XNET_PORT_EVENT_SEND_MSG;
	Submit.Socket = Socket;
	Submit.Id = Id;
	Submit.User = pUser;
	Submit.WriteSpans = pSpans;
	Submit.SpanCount = iCount;
	Submit.Address = pRemote;
	Submit.Control = pControl;
	return __xrtNetPortSubmit(pPort, &Submit);
}



/* 请求取消指定 ID 的在途操作。 */
XRT_API bool xrtNetPortCancel(xnetport* pPort, uint64 Id)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (Id == 0) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_CANCEL,
			"cancel", "invalid network completion cancellation", 0);
		return false;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_CANCEL,
		"cancel"
	) ) {
		return false;
	}
	if ( ((pPort->Capabilities & XNET_PORT_CAP_CANCEL) == 0) ||
		 (pPort->Driver->Cancel == NULL) ) {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_PORT_CANCEL,
			"cancel", "network port backend cannot cancel operations", 0);
		return false;
	}
	return pPort->Driver->Cancel(pPort, Id);
}



/* 跨线程投递一个不会合并的用户事件。 */
XRT_API bool xrtNetPortPost(xnetport* pPort, uint64 Id, ptr pUser)
{
	__xrt_net_port_post* pPost;
	bool bNotify;

	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_POST,
			"post", "invalid network port", 0);
		return false;
	}
	if ( !xrtMutexLock(&pPort->Lock) ) {
		return false;
	}
	if ( xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		__xrtNetSetError(XERR_STATE, XNET_ERROR_PORT_POST,
			"post", "network port is closing", 0);
		return false;
	}
	if ( pPort->PostCount >= pPort->Config.PostLimit ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		__xrtNetSetError(XERR_AGAIN, XNET_ERROR_PORT_POST,
			"post", "network port post queue is full", 0);
		return false;
	}
	pPost = (__xrt_net_port_post*)xrtMalloc(sizeof(*pPost));
	if ( pPost == NULL ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		return false;
	}
	memset(pPost, 0, sizeof(*pPost));
	pPost->Id = Id;
	pPost->User = pUser;
	bNotify = !pPort->NotifyPending;
	if ( bNotify && !pPort->Driver->Wake(pPort) ) {
		xrtFree(pPost);
		(void)xrtMutexUnlock(&pPort->Lock);
		return false;
	}
	if ( pPort->PostTail != NULL ) {
		pPort->PostTail->Next = pPost;
	} else {
		pPort->PostHead = pPost;
	}
	pPort->PostTail = pPost;
	pPort->PostCount++;
	pPort->NotifyPending = true;
	(void)xrtMutexUnlock(&pPort->Lock);
	return true;
}



/* 跨线程请求一个可合并的 WAKE 事件。 */
XRT_API bool xrtNetPortWake(xnetport* pPort)
{
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_POST,
			"wake", "invalid network port", 0);
		return false;
	}
	if ( !xrtMutexLock(&pPort->Lock) ) {
		return false;
	}
	if ( xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		__xrtNetSetError(XERR_STATE, XNET_ERROR_PORT_POST,
			"wake", "network port is closing", 0);
		return false;
	}
	if ( pPort->WakePending ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		return true;
	}
	if ( !pPort->NotifyPending && !pPort->Driver->Wake(pPort) ) {
		(void)xrtMutexUnlock(&pPort->Lock);
		return false;
	}
	pPort->NotifyPending = true;
	pPort->WakePending = true;
	(void)xrtMutexUnlock(&pPort->Lock);
	return true;
}



/* 等待到事件、截止时间或错误；成功和超时都会先清零输出数量。 */
XRT_API xnetresult xrtNetPortWait(xnetport* pPort,
	xnetportevent* pEvents, size_t iCapacity,
	xdeadline iDeadline, size_t* pCount)
{
	if ( pCount != NULL ) {
		*pCount = 0;
	}
	if ( (pPort == NULL) || (pPort->Driver == NULL) ||
		(xrtAtomic32Load(&pPort->Closing, XMEMORY_ACQUIRE) != 0u) ||
		 (pEvents == NULL) || (iCapacity == 0) || (pCount == NULL) ) {
		__xrtNetSetError(XERR_ARGUMENT, XNET_ERROR_PORT_WAIT,
			"wait", "invalid network port wait", 0);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtNetPortRequireThread(
		pPort,
		XNET_ERROR_PORT_WAIT,
		"wait"
	) ) {
		return XNET_RESULT_ERROR;
	}

	for ( ;; ) {
		xnetresult Result;
		size_t iCount = 0;
		size_t iPosts = 0;
		size_t iReady = 0;
		size_t iPostBudget;
		uint64 iTimeout;
		bool bBackendFirst = pPort->BackendTurn;
		bool bPending = false;

		pPort->BackendTurn = false;
		if ( !bBackendFirst ) {
			iPostBudget = (iCapacity > 1u) ?
				((iCapacity / 2u) + (iCapacity % 2u)) : iCapacity;
			if ( !__xrtNetPortDrainPosts(
				pPort,
				pEvents,
				iPostBudget,
				&iPosts,
				&bPending
			) ) {
				return XNET_RESULT_ERROR;
			}
			iCount = iPosts;
			pPort->BackendTurn = bPending;
		}
		iTimeout = ((iCount != 0) || bBackendFirst) ?
			0 : xrtDeadlineRemaining(iDeadline);
		if ( iCount < iCapacity ) {
			Result = pPort->Driver->Wait(pPort,
				pEvents + iCount, iCapacity - iCount,
				iTimeout, &iReady);
			if ( Result == XNET_RESULT_ERROR ) {
				/* 已提取的用户事件必须先交付，终止错误会在下一轮再次出现。 */
				if ( iCount != 0 ) {
					*pCount = iCount;
					return XNET_RESULT_OK;
				}
				return Result;
			}
			iCount += iReady;
		} else {
			Result = XNET_RESULT_OK;
		}
		if ( iCount < iCapacity ) {
			if ( !__xrtNetPortDrainPosts(
				pPort,
				pEvents + iCount,
				iCapacity - iCount,
				&iPosts,
				&bPending
			) ) {
				if ( iCount != 0 ) {
					*pCount = iCount;
					return XNET_RESULT_OK;
				}
				return XNET_RESULT_ERROR;
			}
			iCount += iPosts;
			pPort->BackendTurn = bPending;
		}
		if ( iCount != 0 ) {
			*pCount = iCount;
			return XNET_RESULT_OK;
		}
		if ( (Result == XNET_RESULT_TIMEOUT) ||
			xrtDeadlineExpired(iDeadline) ) {
			return XNET_RESULT_TIMEOUT;
		}
	}
}

#endif
