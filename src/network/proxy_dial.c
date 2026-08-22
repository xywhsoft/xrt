#include "../internal/xrt_proxy.h"



#if defined(XRT_FEATURE_NET_PROXY_DIAL)

#define XRT_NET_PROXY_DIAL_TIMEOUT_DEFAULT 30000000u



/* Proxy Dial 由调用方、运行阶段和可选 Timer 分别持有引用。 */
struct xnetproxydial {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 TimedOut;
	xatomic32 TimerDone;
	xatomic64 Timer;
	xatomicptr TransportDial;
	xatomicptr Stream;
	xnetengine* Engine;
	xnetworker* Worker;
	__xrt_net_engine_internal CancelCommand;
	xnetproxy* Proxy;
	xnetproxyhandshake* Handshake;
	xnetstreamevents StreamEvents;
	ptr StreamData;
	xnetproxydialproc Done;
	ptr DoneData;
	xerror* Error;
	size_t TargetSize;
	size_t ReceiveLimit;
	size_t WriteLimit;
	uint16 TargetPort;
	bool RuntimeHeld;
	bool Driving;
	bool DriveAgain;
	char TargetHost[];
};



/* 从网络错误域创建一份带原因链的 Proxy Dial 错误。 */
static xerror* __xrtNetProxyDialErrorCreate(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.net";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 设置创建阶段的当前线程错误。 */
static void __xrtNetProxyDialSetError(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtNetSetError(Kind, Code, sOperation, sMessage, 0);
}



/* 用底层 Engine 原因链设置代理拨号装配错误。 */
static void __xrtNetProxyDialSetCause(
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtNetProxyDialErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
}



/* 增加 Proxy Dial 引用。 */
XRT_API xnetproxydial* xrtNetProxyDialRef(xnetproxydial* pDial)
{
	if ( (pDial == NULL) ||
		(xrtRefRetain(&pDial->References) < 0) ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT, XNET_ERROR_PROXY_CREATE,
			"retain-proxy-dial", "proxy dial is null or already released"
		);
		return NULL;
	}
	return pDial;
}



/* 释放最后一个引用及尚未转移的网络和协议对象。 */
XRT_API void xrtNetProxyDialDestroy(xnetproxydial* pDial)
{
	xnetdial* pTransportDial;
	xnetstream* pStream;

	if ( (pDial == NULL) ||
		(xrtRefRelease(&pDial->References) != 0) ) {
		return;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);
	xrtNetDialDestroy(pTransportDial);
	xrtNetStreamDestroy(pStream);
	xrtNetProxyHandshakeDestroy(pDial->Handshake);
	xrtNetProxyRelease(pDial->Proxy);
	xrtErrorFree(pDial->Error);
	xrtSecureZero(
		pDial,
		sizeof(*pDial) + pDial->TargetSize + 1u
	);
	xrtFree(pDial);
}



/* Timer 数据引用始终由 Timer 终态回调释放。 */
static void __xrtNetProxyDialCancelTimer(xnetproxydial* pDial)
{
	uint64 Id = xrtAtomic64Exchange(
		&pDial->Timer,
		0,
		XMEMORY_ACQ_REL
	);

	if ( (Id != 0) && !xrtAtomic32Load(
		&pDial->TimerDone,
		XMEMORY_ACQUIRE
	) && !__xrtNetEngineTimerCancelLifecycle(
		pDial->Engine,
		Id
	) ) {
		xrtClearError();
	}
}



/* 发布一次不可变终态，并转移或释放隧道 Stream 调用方引用。 */
static void __xrtNetProxyDialFinish(
	xnetproxydial* pDial,
	xnetresult Result,
	const xerror* pError
)
{
	uint32 iExpected = 0;
	xnetstream* pStream;
	bool bTimedOut;

	if ( !xrtAtomic32CompareExchange(
		&pDial->FinishGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	__xrtNetProxyDialCancelTimer(pDial);
	bTimedOut = xrtAtomic32Load(
		&pDial->TimedOut,
		XMEMORY_ACQUIRE
	) != 0;
	pStream = (xnetstream*)xrtAtomicPtrExchange(
		&pDial->Stream,
		NULL,
		XMEMORY_ACQ_REL
	);
	if ( Result == XNET_RESULT_OK ) {
		xrtAtomic32Store(
			&pDial->State,
			XNET_PROXY_DIAL_CONNECTED,
			XMEMORY_RELEASE
		);
	} else {
		if ( bTimedOut ) {
			xerror* pTimeoutError = __xrtNetProxyDialErrorCreate(
				XERR_TIMEOUT,
				XNET_ERROR_PROXY_CONNECT,
				"dial-proxy",
				"proxy dial timed out before the tunnel opened",
				pDial->Error != NULL ? pDial->Error : pError
			);

			Result = XNET_RESULT_TIMEOUT;
			xrtErrorFree(pDial->Error);
			pDial->Error = pTimeoutError;
		} else if ( pDial->Error == NULL ) {
			pDial->Error = __xrtNetProxyDialErrorCreate(
				Result == XNET_RESULT_CANCELLED ?
					XERR_CANCELLED : XERR_IO,
				XNET_ERROR_PROXY_CONNECT,
				"dial-proxy",
				Result == XNET_RESULT_CANCELLED ?
					"proxy dial was cancelled" :
					"proxy dial failed before the tunnel opened",
				pError
			);
		}
		xrtAtomic32Store(
			&pDial->State,
			Result == XNET_RESULT_CANCELLED ?
				XNET_PROXY_DIAL_CANCELLED : XNET_PROXY_DIAL_FAILED,
			XMEMORY_RELEASE
		);
	}
	if ( pDial->Done != NULL ) {
		pDial->Done(
			pDial,
			Result,
			Result == XNET_RESULT_OK ? pStream : NULL,
			pDial->Error,
			pDial->DoneData
		);
	}
	if ( Result != XNET_RESULT_OK ) {
		xrtNetStreamDestroy(pStream);
	}
	if ( pDial->RuntimeHeld ) {
		pDial->RuntimeHeld = false;
		xrtNetProxyDialDestroy(pDial);
	}
}



/* TCP 零复制发送释放时先清零代理认证报文。 */
static void __xrtNetProxyDialReleaseOutput(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	xrtSecureZero((ptr)pData, iSize);
	xrtFree((ptr)pData);
}



/* 记录协议失败根因并让 TCP Close 统一发布组合终态。 */
static void __xrtNetProxyDialFailStream(
	xnetproxydial* pDial,
	xnetstream* pStream,
	const xerror* pError
)
{
	if ( pDial->Error == NULL ) {
		pDial->Error = xrtErrorRef(pError);
		if ( pDial->Error == NULL ) {
			pDial->Error = __xrtNetProxyDialErrorCreate(
				XERR_PROTOCOL,
				XNET_ERROR_PROXY_PROTOCOL,
				"handshake-proxy",
				"proxy handshake failed",
				NULL
			);
		}
	}
	__xrtNetStreamFailCurrent(
		pStream,
		xrtAtomic32Load(
			&pDial->CancelGate,
			XMEMORY_ACQUIRE
		) ? XNET_RESULT_CANCELLED : XNET_RESULT_ERROR
	);
}



/* 完成事件切换，按 Open、预读 Read、Done 的稳定顺序发布隧道。 */
static void __xrtNetProxyDialReady(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	xnetbuf* pInput = &pStream->ReadBuffer;

	if ( !xrtNetStreamSetEvents(
		pStream,
		&pDial->StreamEvents,
		pDial->StreamData
	) ) {
		__xrtNetProxyDialFailStream(
			pDial, pStream, xrtGetError()
		);
		return;
	}
	xrtNetProxyHandshakeDestroy(pDial->Handshake);
	pDial->Handshake = NULL;
	xrtAtomic32Store(
		&pDial->State,
		XNET_PROXY_DIAL_CONNECTED,
		XMEMORY_RELEASE
	);
	if ( pDial->StreamEvents.Open != NULL ) {
		pDial->StreamEvents.Open(pStream, pDial->StreamData);
	}
	if ( !xrtNetBufEmpty(pInput) &&
		(pDial->StreamEvents.Read != NULL) ) {
		pDial->StreamEvents.Read(
			pStream,
			pInput,
			pDial->StreamData
		);
	}
	__xrtNetProxyDialFinish(pDial, XNET_RESULT_OK, NULL);
}



/* 把握手输出按 TCP 剩余硬预算分段排队，并继续解析已有输入。 */
static void __xrtNetProxyDialDriveUnsafe(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	for ( ;; ) {
		xnetproxyhandshakestate State =
			xrtNetProxyHandshakeState(pDial->Handshake);

		if ( State == XNET_PROXY_HANDSHAKE_WRITE ) {
			xnetspan Output;
			size_t iPending = xrtNetStreamPending(pStream);
			size_t iAvailable = iPending < pDial->WriteLimit ?
				pDial->WriteLimit - iPending : 0;
			size_t iChunk;
			bytes pCopy;
			xnetresult Result;

			if ( iAvailable == 0 ) {
				return;
			}
			if ( !xrtNetProxyHandshakeOutput(
				pDial->Handshake, &Output
			) || (Output.Size == 0) ) {
				__xrtNetProxyDialSetError(
					XERR_INTERNAL,
					XNET_ERROR_PROXY_PROTOCOL,
					"send-proxy-handshake",
					"proxy handshake entered WRITE without output"
				);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			iChunk = Output.Size < iAvailable ?
				Output.Size : iAvailable;
			pCopy = (bytes)xrtMemDup(Output.Data, iChunk);
			if ( pCopy == NULL ) {
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			Result = xrtNetStreamSendRef(
				pStream,
				pCopy,
				iChunk,
				__xrtNetProxyDialReleaseOutput,
				NULL
			);
			if ( Result == XNET_RESULT_AGAIN ) {
				xrtSecureZero(pCopy, iChunk);
				xrtFree(pCopy);
				return;
			}
			if ( Result != XNET_RESULT_OK ) {
				xrtSecureZero(pCopy, iChunk);
				xrtFree(pCopy);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			if ( xrtNetProxyHandshakeSent(
				pDial->Handshake, iChunk
			) != iChunk ) {
				__xrtNetProxyDialSetError(
					XERR_INTERNAL,
					XNET_ERROR_PROXY_PROTOCOL,
					"send-proxy-handshake",
					"proxy handshake rejected acknowledged output"
				);
				__xrtNetProxyDialFailStream(
					pDial, pStream, xrtGetError()
				);
				return;
			}
			continue;
		}
		if ( State == XNET_PROXY_HANDSHAKE_READ ) {
			State = xrtNetProxyHandshakeStep(
				pDial->Handshake,
				&pStream->ReadBuffer
			);
			if ( State == XNET_PROXY_HANDSHAKE_READ ) {
				return;
			}
			continue;
		}
		if ( State == XNET_PROXY_HANDSHAKE_READY ) {
			__xrtNetProxyDialReady(pDial, pStream);
			return;
		}
		__xrtNetProxyDialFailStream(
			pDial,
			pStream,
			xrtNetProxyHandshakeError(pDial->Handshake)
		);
		return;
	}
}



/* 串行推进握手，并把同步水位回调折叠为下一轮驱动。 */
static void __xrtNetProxyDialDrive(
	xnetproxydial* pDial,
	xnetstream* pStream
)
{
	if ( pDial->Driving ) {
		pDial->DriveAgain = true;
		return;
	}
	if ( xrtRefRetain(&pDial->References) < 0 ) {
		return;
	}
	pDial->Driving = true;
	do {
		pDial->DriveAgain = false;
		__xrtNetProxyDialDriveUnsafe(pDial, pStream);
	} while ( pDial->DriveAgain && !xrtAtomic32Load(
		&pDial->FinishGate,
		XMEMORY_ACQUIRE
	) );
	pDial->Driving = false;
	xrtNetProxyDialDestroy(pDial);
}



/* TCP Open 后才从所属 Worker 缓冲池创建短生命周期握手。 */
static void __xrtNetProxyDialStreamOpen(
	xnetstream* pStream,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;
	xnetproxyhandshakeconfig Config;

	xrtAtomicPtrStore(
		&pDial->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(&pDial->CancelGate, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamFailCurrent(
			pStream,
			XNET_RESULT_CANCELLED
		);
		return;
	}
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pDial->Proxy;
	Config.TargetHost.Data = pDial->TargetHost;
	Config.TargetHost.Size = pDial->TargetSize;
	Config.TargetPort = pDial->TargetPort;
	Config.ReceiveLimit = pDial->ReceiveLimit;
	Config.Pool = xrtNetWorkerBufPool(pStream->Worker);
	pDial->Handshake = xrtNetProxyHandshakeCreate(&Config);
	if ( pDial->Handshake == NULL ) {
		__xrtNetProxyDialFailStream(
			pDial, pStream, xrtGetError()
		);
		return;
	}
	xrtAtomic32Store(
		&pDial->State,
		XNET_PROXY_DIAL_HANDSHAKE,
		XMEMORY_RELEASE
	);
	__xrtNetProxyDialDrive(pDial, pStream);
}



/* TCP Read 只推进协议状态机，成功时剩余输入交给最终用户事件。 */
static void __xrtNetProxyDialStreamRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	(void)pBuffer;
	__xrtNetProxyDialDrive(pDial, pStream);
}



/* 代理回复完成前的 EOF 是协议截断。 */
static void __xrtNetProxyDialStreamEnd(
	xnetstream* pStream,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	__xrtNetProxyDialSetError(
		XERR_PROTOCOL,
		XNET_ERROR_PROXY_PROTOCOL,
		"handshake-proxy",
		"proxy closed before completing the handshake"
	);
	__xrtNetProxyDialFailStream(pDial, pStream, xrtGetError());
}



/* 写队列回到低水位后继续提交尚未排队的协议输出。 */
static void __xrtNetProxyDialStreamLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)iQueued;
	__xrtNetProxyDialDrive((xnetproxydial*)pData, pStream);
}



/* 写队列完全排空也必须唤醒未跨越高水位的小上限配置。 */
static void __xrtNetProxyDialStreamDrain(
	xnetstream* pStream,
	ptr pData
)
{
	__xrtNetProxyDialDrive((xnetproxydial*)pData, pStream);
}



/* 握手前 TCP 关闭只通过 Proxy Dial 完成回调发布。 */
static void __xrtNetProxyDialStreamClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	(void)pStream;
	__xrtNetProxyDialFinish(
		pDial,
		Result,
		pDial->Error != NULL ? pDial->Error : pError
	);
}



/* 代理握手阶段独占的 TCP 事件表。 */
static const xnetstreamevents __xrtNetProxyDialStreamEvents = {
	__xrtNetProxyDialStreamOpen,
	__xrtNetProxyDialStreamRead,
	__xrtNetProxyDialStreamEnd,
	NULL,
	__xrtNetProxyDialStreamLowWater,
	__xrtNetProxyDialStreamDrain,
	__xrtNetProxyDialStreamClose
};



/* TCP Dial 失败直接完成，成功则确认获胜 Stream 所有权已经转移。 */
static void __xrtNetProxyDialTransportDone(
	xnetdial* pTransportDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;
	xnetstream* pExpected = (xnetstream*)xrtAtomicPtrLoad(
		&pDial->Stream,
		XMEMORY_ACQUIRE
	);

	if ( Result != XNET_RESULT_OK ) {
		__xrtNetProxyDialFinish(
			pDial,
			Result,
			pError != NULL ? pError :
				xrtNetDialError(pTransportDial)
		);
		return;
	}
	if ( pExpected != pStream ) {
		__xrtNetProxyDialSetError(
			XERR_INTERNAL,
			XNET_ERROR_PROXY_CONNECT,
			"dial-proxy",
			"TCP dial transferred an unexpected proxy transport"
		);
		if ( pExpected == NULL ) {
			xrtAtomicPtrStore(
				&pDial->Stream,
				pStream,
				XMEMORY_RELEASE
			);
		}
		__xrtNetProxyDialFailStream(
			pDial, pStream, xrtGetError()
		);
		return;
	}
	if ( xrtAtomic32Load(&pDial->CancelGate, XMEMORY_ACQUIRE) ) {
		__xrtNetStreamFailCurrent(
			pStream,
			XNET_RESULT_CANCELLED
		);
	}
}



/* 在唯一所属 Worker 上取消已经公开的代理传输。 */
static void __xrtNetProxyDialCancelTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;
	xnetstream* pStream;

	(void)pWorker;
	if ( !xrtAtomic32Load(&pDial->FinishGate, XMEMORY_ACQUIRE) ) {
		pStream = (xnetstream*)xrtAtomicPtrLoad(
			&pDial->Stream,
			XMEMORY_ACQUIRE
		);
		if ( pStream != NULL ) {
			__xrtNetStreamFailCurrent(
				pStream,
				XNET_RESULT_CANCELLED
			);
		}
	}
	xrtNetProxyDialDestroy(pDial);
}



/* 全过程 Timer 通过正常取消和关闭路径发布超时根因。 */
static void __xrtNetProxyDialTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xnetproxydial* pDial = (xnetproxydial*)pData;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(&pDial->TimerDone, 1, XMEMORY_RELEASE);
	(void)xrtAtomic64Exchange(
		&pDial->Timer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) && !xrtAtomic32Load(
		&pDial->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic32Store(&pDial->TimedOut, 1, XMEMORY_RELEASE);
		(void)xrtNetProxyDialCancel(pDial);
	}
	xrtNetProxyDialDestroy(pDial);
}



/* 初始化 TCP Dial、输入硬上限和全过程超时。 */
XRT_API void xrtNetProxyDialConfigInit(xnetproxydialconfig* pConfig)
{
	if ( pConfig == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CONFIG,
			"init-proxy-dial-config",
			"proxy dial config is null"
		);
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtNetDialConfigInit(&pConfig->Transport);
	pConfig->Timeout = XRT_NET_PROXY_DIAL_TIMEOUT_DEFAULT;
	pConfig->ReceiveLimit = XRT_NET_PROXY_RECEIVE_LIMIT;
}



/* 验证构建后端、TCP 缓冲边界和代理输入硬上限。 */
static bool __xrtNetProxyDialConfigValid(
	const xnetproxy* pProxy,
	const xnetproxydialconfig* pConfig,
	xstrview Target
)
{
	xnetproxyinfo Info;

	(void)Target;

	if ( !__xrtNetStreamConfigValid(&pConfig->Transport.Stream) ) {
		return false;
	}
	if ( (pConfig->ReceiveLimit < 4u) ||
		(pConfig->ReceiveLimit > pConfig->Transport.Stream.ReadLimit) ) {
		__xrtNetProxyDialSetError(
			XERR_RANGE,
			XNET_ERROR_PROXY_LIMIT,
			"configure-proxy-dial",
			"proxy receive limit must fit the TCP read limit"
		);
		return false;
	}
	if ( !xrtNetProxyInfo(pProxy, &Info) ) {
		return false;
	}
	if ( Info.Type == XNET_PROXY_SOCKS5 ) {
		#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
			xnetaddr Address;

			if ( !__xrtNetAddrTryParse(
				&Address,
				Target,
				0
			) && (Target.Size > UINT8_MAX) ) {
				__xrtNetProxyDialSetError(
					XERR_RANGE,
					XNET_ERROR_PROXY_LIMIT,
					"configure-proxy-dial",
					"SOCKS5 target host exceeds the protocol limit"
				);
				return false;
			}
			return true;
		#else
			__xrtNetProxyDialSetError(
				XERR_UNSUPPORTED,
				XNET_ERROR_PROXY_UNSUPPORTED,
				"configure-proxy-dial",
				"SOCKS5 support is not compiled"
			);
			return false;
		#endif
	}
	if ( Info.Type == XNET_PROXY_HTTP_CONNECT ) {
		#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
			return true;
		#else
			__xrtNetProxyDialSetError(
				XERR_UNSUPPORTED,
				XNET_ERROR_PROXY_UNSUPPORTED,
				"configure-proxy-dial",
				"HTTP CONNECT support is not compiled"
			);
			return false;
		#endif
	}
	__xrtNetProxyDialSetError(
		XERR_VALUE,
		XNET_ERROR_PROXY_CONFIG,
		"configure-proxy-dial",
		"proxy type is invalid"
	);
	return false;
}



/* 复用 TCP Dial 连接代理端点，TCP Open 后才分配协议握手状态。 */
XRT_API xnetproxydial* xrtNetProxyDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xnetproxy* pProxy,
	cstr sTargetHost,
	uint16 iTargetPort,
	const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetproxydialproc pDone,
	ptr pDoneData
)
{
	xnetproxydialconfig Config;
	xnetproxyinfo Info;
	xnetproxydial* pDial;
	xnetdial* pTransportDial;
	xstrview Target;
	xerror* pError;
	uint64 Id;
	uint32 iWorkerCount;

	if ( (pEngine == NULL) || (pResolver == NULL) ||
		(pProxy == NULL) || (sTargetHost == NULL) ||
		(sTargetHost[0] == 0) || (iTargetPort == 0) ||
		(pDone == NULL) ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CONFIG,
			"dial-proxy",
			"invalid proxy dial arguments"
		);
		return NULL;
	}
	xrtNetProxyDialConfigInit(&Config);
	if ( pConfig != NULL ) {
		Config = *pConfig;
	}
	Target.Data = sTargetHost;
	Target.Size = strlen(sTargetHost);
	if ( !__xrtNetProxyHostValid(
		Target,
		"dial-proxy",
		"proxy target host is empty"
	) || !__xrtNetProxyDialConfigValid(pProxy, &Config, Target) ) {
		return NULL;
	}
	if ( !xrtNetProxyInfo(pProxy, &Info) ) {
		return NULL;
	}
	if ( !__xrtNetEngineObjectHold(pEngine) ) {
		pError = xrtTakeError();
		__xrtNetProxyDialSetCause(
			XERR_CLOSED,
			XNET_ERROR_PROXY_CREATE,
			"dial-proxy",
			"proxy dial could not hold the network engine",
			pError
		);
		xrtErrorFree(pError);
		return NULL;
	}
	iWorkerCount = xrtNetEngineWorkerCount(pEngine);
	if ( (xrtNetEngineState(pEngine) != XNET_ENGINE_RUNNING) ||
		 (iWorkerCount == 0) ) {
		__xrtNetEngineObjectRelease(pEngine);
		__xrtNetProxyDialSetError(
			XERR_STATE,
			XNET_ERROR_PROXY_CREATE,
			"dial-proxy",
			"proxy dial requires a running network engine"
		);
		return NULL;
	}
	pDial = (xnetproxydial*)xrtCalloc(
		1,
		sizeof(*pDial) + Target.Size + 1u
	);
	if ( pDial == NULL ) {
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	memcpy(pDial->TargetHost, sTargetHost, Target.Size + 1u);
	pDial->TargetSize = Target.Size;
	pDial->Proxy = xrtNetProxyRetain(pProxy);
	if ( pDial->Proxy == NULL ) {
		xrtSecureZero(
			pDial,
			sizeof(*pDial) + pDial->TargetSize + 1u
		);
		xrtFree(pDial);
		__xrtNetEngineObjectRelease(pEngine);
		return NULL;
	}
	pDial->References = 2;
	xrtAtomic32Init(&pDial->State, XNET_PROXY_DIAL_RESOLVING);
	xrtAtomic32Init(&pDial->CancelGate, 0);
	xrtAtomic32Init(&pDial->FinishGate, 0);
	xrtAtomic32Init(&pDial->TimedOut, 0);
	xrtAtomic32Init(&pDial->TimerDone, 0);
	xrtAtomic64Init(&pDial->Timer, 0);
	xrtAtomicPtrInit(&pDial->TransportDial, NULL);
	xrtAtomicPtrInit(&pDial->Stream, NULL);
	pDial->Engine = pEngine;
	pDial->Worker = xrtNetEngineWorker(
		pEngine,
		(uint32)(Config.Transport.Affinity % iWorkerCount)
	);
	pDial->StreamData = pStreamData;
	pDial->Done = pDone;
	pDial->DoneData = pDoneData;
	pDial->TargetPort = iTargetPort;
	pDial->ReceiveLimit = Config.ReceiveLimit;
	pDial->WriteLimit = Config.Transport.Stream.WriteLimit;
	pDial->RuntimeHeld = true;
	if ( pStreamEvents != NULL ) {
		pDial->StreamEvents = *pStreamEvents;
	}
	if ( Config.Timeout != 0 ) {
		xrtNetProxyDialRef(pDial);
		Id = xrtNetEngineAfter(
			pEngine,
			Config.Transport.Affinity,
			Config.Timeout,
			__xrtNetProxyDialTimer,
			pDial
		);
		if ( Id == 0 ) {
			pError = xrtTakeError();
			xrtNetProxyDialDestroy(pDial);
			pDial->RuntimeHeld = false;
			xrtNetProxyDialDestroy(pDial);
			xrtNetProxyDialDestroy(pDial);
			__xrtNetEngineObjectRelease(pEngine);
			xrtSetError(pError);
			xrtErrorFree(pError);
			return NULL;
		}
		xrtAtomic64Store(&pDial->Timer, Id, XMEMORY_RELEASE);
		if ( xrtAtomic32Load(&pDial->TimerDone, XMEMORY_ACQUIRE) ) {
			(void)xrtAtomic64Exchange(
				&pDial->Timer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
	pTransportDial = xrtNetDial(
		pEngine,
		pResolver,
		Info.Host.Data,
		Info.Port,
		&Config.Transport,
		&__xrtNetProxyDialStreamEvents,
		pDial,
		__xrtNetProxyDialTransportDone,
		pDial
	);
	if ( pTransportDial == NULL ) {
		pError = xrtTakeError();
		__xrtNetProxyDialCancelTimer(pDial);
		pDial->RuntimeHeld = false;
		xrtNetProxyDialDestroy(pDial);
		xrtNetProxyDialDestroy(pDial);
		__xrtNetEngineObjectRelease(pEngine);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pDial->TransportDial,
		pTransportDial,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(&pDial->CancelGate, XMEMORY_ACQUIRE) ) {
		(void)xrtNetDialCancel(pTransportDial);
	}
	__xrtNetEngineObjectRelease(pEngine);
	return pDial;
}



/* 从任意线程取消当前名称解析、TCP 或代理握手阶段。 */
XRT_API bool xrtNetProxyDialCancel(xnetproxydial* pDial)
{
	uint32 iExpected = 0;
	xnetdial* pTransportDial;
	xnetproxydialstate State;

	if ( pDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"cancel-proxy-dial",
			"proxy dial is null"
		);
		return false;
	}
	State = (xnetproxydialstate)xrtAtomic32Load(
		&pDial->State,
		XMEMORY_ACQUIRE
	);
	if ( xrtAtomic32Load(&pDial->FinishGate, XMEMORY_ACQUIRE) ||
		 (State == XNET_PROXY_DIAL_CONNECTED) ||
		 (State == XNET_PROXY_DIAL_FAILED) ||
		 (State == XNET_PROXY_DIAL_CANCELLED) ) {
		return false;
	}
	if ( !xrtAtomic32CompareExchange(
		&pDial->CancelGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial != NULL ) {
		(void)xrtNetDialCancel(pTransportDial);
	}
	if ( xrtNetProxyDialRef(pDial) == NULL ) {
		return false;
	}
	__xrtNetEnginePostInternal(
		pDial->Worker,
		&pDial->CancelCommand,
		__xrtNetProxyDialCancelTask,
		pDial
	);
	return true;
}



/* 解析和 TCP 连接阶段直接映射底层 TCP Dial 快照。 */
XRT_API xnetproxydialstate xrtNetProxyDialState(
	const xnetproxydial* pDial
)
{
	xnetproxydialstate State;
	xnetdial* pTransportDial;
	xnetdialstate TransportState;

	if ( pDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"state-proxy-dial",
			"proxy dial is null"
		);
		return XNET_PROXY_DIAL_FAILED;
	}
	State = (xnetproxydialstate)xrtAtomic32Load(
		&pDial->State,
		XMEMORY_ACQUIRE
	);
	if ( (State != XNET_PROXY_DIAL_RESOLVING) &&
		(State != XNET_PROXY_DIAL_CONNECTING) ) {
		return State;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		return State;
	}
	TransportState = xrtNetDialState(pTransportDial);
	if ( TransportState == XNET_DIAL_CONNECTING ) {
		return XNET_PROXY_DIAL_CONNECTING;
	}
	if ( TransportState == XNET_DIAL_CONNECTED ) {
		return XNET_PROXY_DIAL_HANDSHAKE;
	}
	return State;
}



/* 终态后返回 DNS、TCP、代理协议或全过程超时错误。 */
XRT_API const xerror* xrtNetProxyDialError(
	const xnetproxydial* pDial
)
{
	xnetproxydialstate State;

	if ( pDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"error-proxy-dial",
			"proxy dial is null"
		);
		return NULL;
	}
	State = xrtNetProxyDialState(pDial);
	return ((State == XNET_PROXY_DIAL_FAILED) ||
		(State == XNET_PROXY_DIAL_CANCELLED)) ?
		pDial->Error : NULL;
}



/* 复制组合阶段和底层地址竞速统计。 */
XRT_API bool xrtNetProxyDialStats(
	const xnetproxydial* pDial,
	xnetproxydialstats* pStats
)
{
	xnetdial* pTransportDial;

	if ( (pDial == NULL) || (pStats == NULL) ) {
		__xrtNetProxyDialSetError(
			XERR_ARGUMENT,
			XNET_ERROR_PROXY_CREATE,
			"stats-proxy-dial",
			"proxy dial or statistics output is null"
		);
		return false;
	}
	pTransportDial = (xnetdial*)xrtAtomicPtrLoad(
		&pDial->TransportDial,
		XMEMORY_ACQUIRE
	);
	if ( pTransportDial == NULL ) {
		__xrtNetProxyDialSetError(
			XERR_STATE,
			XNET_ERROR_PROXY_CONNECT,
			"stats-proxy-dial",
			"proxy TCP dial is not available"
		);
		return false;
	}
	memset(pStats, 0, sizeof(*pStats));
	pStats->State = xrtNetProxyDialState(pDial);
	return xrtNetDialStats(pTransportDial, &pStats->Transport);
}

#endif
