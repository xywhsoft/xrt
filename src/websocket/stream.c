#include "../internal/xrt_websocket_stream.h"



#if defined(XRT_FEATURE_WEBSOCKET_STREAM)

#define __XRT_WS_MASK_CHUNK 1024u



/* 创建 Connection 域错误，并保留完整底层原因链。 */
xerror* __xrtWsStreamErrorCreate(
	xerrkind Kind,
	xwsstreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.stream";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	return xrtErrorBuild(&Desc);
}



/* 保存第一个不可恢复错误，并同步设置当前线程错误。 */
static const xerror* __xrtWsStreamRemember(
	xwsstream* pConnection,
	xerrkind Kind,
	xwsstreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtWsStreamErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	ptr pExpected = NULL;
	const xerror* pStored;

	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	if ( (pConnection != NULL) && (pError != NULL) ) {
		if ( !xrtAtomicPtrCompareExchange(
			&pConnection->Error,
			&pExpected,
			pError,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			xrtErrorFree(pError);
		}
		pStored = (const xerror*)xrtAtomicPtrLoad(
			&pConnection->Error,
			XMEMORY_ACQUIRE
		);
	} else {
		pStored = pError;
	}
	if ( pStored != NULL ) {
		xrtSetError(pStored);
	}
	if ( pConnection == NULL ) {
		xrtErrorFree(pError);
	}
	return pStored;
}



/* 设置一次调用错误，不污染 Connection 保存的不可恢复错误。 */
const xerror* __xrtWsStreamReject(
	xerrkind Kind,
	xwsstreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	return __xrtWsStreamRemember(
		NULL,
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 第一次错误发布与第一个保存错误保持一一对应。 */
static void __xrtWsStreamEmitError(xwsstream* pConnection)
{
	const xerror* pError;

	if ( (pConnection == NULL) || pConnection->ErrorEmitted ) {
		return;
	}
	pError = (const xerror*)xrtAtomicPtrLoad(
		&pConnection->Error,
		XMEMORY_ACQUIRE
	);
	if ( pError == NULL ) {
		return;
	}
	pConnection->ErrorEmitted = true;
	if ( pConnection->Events.Error != NULL ) {
		pConnection->Events.Error(
			pConnection,
			pError,
			pConnection->Data
		);
	}
}



/* 返回指定角色最大控制帧在线路上占用的字节数。 */
static size_t __xrtWsStreamControlSlot(xwsrole Role)
{
	return 2u + XWS_CLOSE_PAYLOAD_MAX +
		(Role == XWS_ROLE_CLIENT ? XWS_MASK_SIZE : 0u);
}



/* 验证基础边界，并把协商参数映射到本地压缩方向。 */
static bool __xrtWsStreamConfigPrepare(xwsstreamconfig* pConfig)
{
	size_t iControlSlot;
	size_t iControlMinimum;
	size_t iDataMinimum;

	if ( (pConfig == NULL) ||
		((pConfig->Role != XWS_ROLE_CLIENT) &&
		 (pConfig->Role != XWS_ROLE_SERVER)) ||
		!__xrtRangeValid(
			pConfig->Protocol.Data,
			pConfig->Protocol.Size
		) ||
		(pConfig->Protocol.Size >
		 (SIZE_MAX - sizeof(xwsstream) - 1u)) ||
		(pConfig->MessageLimit == 0) ||
		(pConfig->FrameLimit == 0) ||
		(pConfig->FrameLimit > XWS_FRAME_PAYLOAD_MAX) ||
		(pConfig->FrameLimit > (uint64)SIZE_MAX) ) {
		return false;
	}
	iControlSlot = __xrtWsStreamControlSlot(pConfig->Role);
	iControlMinimum = iControlSlot * 3u;
	if ( (pConfig->ControlReserve < iControlMinimum) ||
		(pConfig->ControlReserve > pConfig->SendLimit) ) {
		return false;
	}
	iDataMinimum = 2u + (
		pConfig->Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u
	);
	if ( (pConfig->SendLimit -
		 pConfig->ControlReserve) <
		iDataMinimum ) {
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( pConfig->DeflateEnabled ) {
			xwsdeflatedirection Receive;
			xwsdeflatedirection Send;
			size_t iSize = 0;

			if ( !xrtWsDeflateResponseWrite(
				&pConfig->Deflate,
				NULL,
				0,
				&iSize
			) || !xrtWsDeflateDirection(
				&pConfig->Deflate,
				pConfig->Role,
				false,
				&Receive
			) || !xrtWsDeflateDirection(
				&pConfig->Deflate,
				pConfig->Role,
				true,
				&Send
			) || !xrtWsInflaterConfigApply(
				&pConfig->Inflater,
				&Receive
			) || !xrtWsDeflaterConfigApply(
				&pConfig->Deflater,
				&Send
			) ) {
				return false;
			}
		}
	#endif
	return true;
}



/* 对外部适配层执行无分配且不修改输入的 Connection 配置预检。 */
XRT_API bool xrtWsStreamConfigValid(
	const xwsstreamconfig* pConfig
)
{
	xwsstreamconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return __xrtWsStreamConfigPrepare(&Config);
}



/* 验证 Connection 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsStreamRangeValid(const xwsstream* pConnection)
{
	return __xrtRangeValid(
		pConnection,
		sizeof(*pConnection)
	);
}



/* 验证公开入口收到的 Connection，并设置稳定的结构化错误。 */
static bool __xrtWsStreamCheck(
	const xwsstream* pConnection,
	cstr sOperation
)
{
	if ( __xrtWsStreamRangeValid(pConnection) ) {
		return true;
	}
	(void)__xrtWsStreamReject(
		XERR_ARGUMENT,
		XWS_STREAM_ERROR_ARGUMENT,
		sOperation,
		"WebSocket connection range is invalid",
		NULL
	);
	return false;
}



/* 已关闭对象可从任意线程查询，活动操作必须留在传输 Worker。 */
bool __xrtWsStreamWorker(
	xwsstream* pConnection,
	cstr sOperation
)
{
	if ( !__xrtWsStreamCheck(pConnection, sOperation) ) {
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsStreamReject(
			XERR_STATE,
			XWS_STREAM_ERROR_STATE,
			sOperation,
			"WebSocket connection operation requires its network worker",
			NULL
		);
		return false;
	}
	return true;
}



/* 短暂增加底层传输引用，避免并发 Abort 和终态回调发生悬空访问。 */
static ptr __xrtWsStreamTransportRef(
	const xwsstream* pConnection
)
{
	xwsstream* pMutable = (xwsstream*)pConnection;
	ptr pTransport;

	if ( pMutable == NULL ) {
		return NULL;
	}
	__xrtSpinLock(&pMutable->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pMutable->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport != NULL ) {
		#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
			if ( pMutable->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				pTransport = xrtTlsStreamRef(
					(xtlsstream*)pTransport
				);
			} else
		#endif
		{
			pTransport = xrtNetStreamRef(
				(xnetstream*)pTransport
			);
		}
	}
	__xrtSpinUnlock(&pMutable->TransportLock);
	return pTransport;
}



/* 释放由传输快照取得的临时引用。 */
static void __xrtWsStreamTransportRelease(
	const xwsstream* pConnection,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	(void)pConnection;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
			return;
		}
	#endif
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 返回当前传输已经受理但尚未排空的字节数。 */
static size_t __xrtWsStreamTransportPending(
	const xwsstream* pConnection
)
{
	ptr pTransport = __xrtWsStreamTransportRef(pConnection);
	size_t iPending = 0;

	if ( pTransport != NULL ) {
		#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
			if ( pConnection->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				iPending = xrtTlsStreamPending(
					(xtlsstream*)pTransport
				);
			} else
		#endif
		{
			iPending = xrtNetStreamPending(
				(xnetstream*)pTransport
			);
		}
	}
	__xrtWsStreamTransportRelease(pConnection, pTransport);
	return iPending;
}



/* 释放 TLS 尚未受理的全部精确帧余量。 */
static void __xrtWsStreamOutputClear(xwsstream* pConnection)
{
	__xrt_ws_output* pOutput = pConnection->OutputHead;

	pConnection->OutputHead = NULL;
	pConnection->OutputTail = NULL;
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	while ( pOutput != NULL ) {
		__xrt_ws_output* pNext = pOutput->Next;

		xrtFree(pOutput);
		pOutput = pNext;
	}
}



/* 最后一个 Connection 引用释放所有非传输资源。 */
static void __xrtWsStreamFree(xwsstream* pConnection)
{
	xerror* pError;

	if ( pConnection == NULL ) {
		return;
	}
	__xrtWsStreamOutputClear(pConnection);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xrtWsInflaterDestroy(pConnection->Inflater);
		xrtWsDeflaterDestroy(pConnection->Deflater);
	#endif
	pError = (xerror*)xrtAtomicPtrExchange(
		&pConnection->Error,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtErrorFree(pError);
	__xrtSpinUnit(&pConnection->TransportLock);
	xrtFree(pConnection);
}



/* TCP 零复制发送完成后释放包含帧数据的单一分配。 */
static void __xrtWsStreamOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	xrtFree(pContext);
}



/* 合并 TLS 余量和传输队列，溢出时返回饱和值。 */
XRT_API size_t xrtWsStreamPending(const xwsstream* pConnection)
{
	uint64 iOutput;
	size_t iTransport;

	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-pending"
	) ) {
		return 0;
	}
	iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	iTransport = __xrtWsStreamTransportPending(pConnection);
	if ( iOutput > (uint64)(SIZE_MAX - iTransport) ) {
		return SIZE_MAX;
	}
	return iTransport + (size_t)iOutput;
}



/* 把 WebSocket 线路字节换算为当前传输实际占用的发送预算。 */
static bool __xrtWsStreamTransportSize(
	const xwsstream* pConnection,
	size_t iSize,
	size_t* pBudget
)
{
	size_t iRecords;

	if ( iSize == 0 ) {
		*pBudget = 0;
		return true;
	}
	if ( pConnection->SendOverhead == 0 ) {
		*pBudget = iSize;
		return true;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		iRecords = (iSize / XTLS_RECORD_PLAINTEXT_MAX) +
			((iSize % XTLS_RECORD_PLAINTEXT_MAX) != 0 ? 1u : 0u);
		if ( iRecords > ((SIZE_MAX - iSize) /
			pConnection->SendOverhead) ) {
			return false;
		}
		*pBudget = iSize +
			(iRecords * pConnection->SendOverhead);
		return true;
	#else
		(void)iRecords;
		return false;
	#endif
}



/* 返回发送类别必须留给更高优先级协议帧的传输预算。 */
static size_t __xrtWsStreamReserve(
	const xwsstream* pConnection,
	__xrt_ws_send_class Class
)
{
	switch ( Class ) {
		case __XRT_WS_SEND_DATA:
			return pConnection->Config.ControlReserve;
		case __XRT_WS_SEND_CONTROL:
			return pConnection->ControlSlot * 2u;
		case __XRT_WS_SEND_AUTO_PONG:
			return pConnection->ControlSlot;
		case __XRT_WS_SEND_CLOSE:
		default:
			return 0;
	}
}



/* 在一个总容量中扣除当前类别不能占用的协议预留。 */
static size_t __xrtWsStreamClassCapacity(
	size_t iCapacity,
	size_t iReserve
)
{
	return iCapacity > iReserve ?
		iCapacity - iReserve : 0;
}



/*
	返回一帧永久能够占用的发送容量。
	TCP 的 WriteLimit 可能小于 WebSocket SendLimit，必须在提交前共同取小值；
	TLS 短写余量由 Connection 自身队列承接，因此只受 SendLimit 约束。
*/
static size_t __xrtWsStreamCapacity(
	const xwsstream* pConnection,
	__xrt_ws_send_class Class
)
{
	size_t iReserve = __xrtWsStreamReserve(pConnection, Class);
	size_t iCapacity = __xrtWsStreamClassCapacity(
		pConnection->Config.SendLimit,
		iReserve
	);
	ptr pTransport;

	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return iCapacity;
		}
	#endif
	pTransport = __xrtWsStreamTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsStreamClassCapacity(
			xrtNetStreamWriteLimit((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iCapacity ) {
			iCapacity = iTcp;
		}
	}
	__xrtWsStreamTransportRelease(pConnection, pTransport);
	return iCapacity;
}



/* 前置声明供公开普通数据可写查询复用同一容量口径。 */
static size_t __xrtWsStreamAvailable(
	const xwsstream* pConnection,
	__xrt_ws_send_class Class
);



/* 计算控制预留之后普通数据仍可使用的发送预算。 */
XRT_API size_t xrtWsStreamWritable(const xwsstream* pConnection)
{
	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-writable"
	) || (xrtWsStreamState(pConnection) != XWS_STREAM_OPEN) ) {
		return 0;
	}
	return __xrtWsStreamAvailable(
		pConnection,
		__XRT_WS_SEND_DATA
	);
}



/* 按发送类别扣除更高优先级协议帧的固定预留。 */
static size_t __xrtWsStreamAvailable(
	const xwsstream* pConnection,
	__xrt_ws_send_class Class
)
{
	size_t iPending = xrtWsStreamPending(pConnection);
	size_t iReserve = __xrtWsStreamReserve(pConnection, Class);
	size_t iAvailable;
	ptr pTransport;

	if ( iPending >= pConnection->Config.SendLimit ) {
		return 0;
	}
	iAvailable = __xrtWsStreamClassCapacity(
		pConnection->Config.SendLimit - iPending,
		iReserve
	);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return iAvailable;
		}
	#endif
	pTransport = __xrtWsStreamTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsStreamClassCapacity(
			xrtNetStreamWritable((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iAvailable ) {
			iAvailable = iTcp;
		}
	}
	__xrtWsStreamTransportRelease(pConnection, pTransport);
	return iAvailable;
}



/* 普通发送第一次遇到硬预算时发布一次背压边沿。 */
void __xrtWsStreamBackpressure(xwsstream* pConnection)
{
	if ( pConnection->Backpressured ) {
		return;
	}
	pConnection->Backpressured = true;
	if ( pConnection->Events.Backpressure != NULL ) {
		pConnection->Events.Backpressure(
			pConnection,
			xrtWsStreamPending(pConnection),
			pConnection->Data
		);
	}
}



/* 发送预算恢复后发布一次可写边沿。 */
static void __xrtWsStreamWritableEvent(xwsstream* pConnection)
{
	if ( !pConnection->Backpressured ||
		(xrtWsStreamWritable(pConnection) == 0) ) {
		return;
	}
	pConnection->Backpressured = false;
	if ( pConnection->Events.Writable != NULL ) {
		pConnection->Events.Writable(
			pConnection,
			xrtWsStreamPending(pConnection),
			pConnection->Data
		);
	}
}



/* 创建一个头部与负载同分配的完整线路帧。 */
static __xrt_ws_output* __xrtWsStreamFrame(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	bool bCompressed
)
{
	xwsframe Frame;
	xwsframeconfig Config;
	__xrt_ws_output* pOutput;
	size_t iHead = 0;
	size_t iTotal;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	Frame.PayloadSize = Payload.Size;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bCompressed ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		Frame.Flags |= XWS_FRAME_MASKED;
		if ( !xrtSecureRandom(Frame.Mask, sizeof(Frame.Mask)) ) {
			(void)__xrtWsStreamReject(
				XERR_IO,
				XWS_STREAM_ERROR_RANDOM,
				"write-websocket-frame",
				"WebSocket client could not generate a frame mask",
				xrtGetError()
			);
			return NULL;
		}
	}
	xrtWsFrameConfigInit(&Config);
	if ( bCompressed ) {
		Config.AllowedRsv = XWS_FRAME_RSV1;
	}
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		NULL,
		0,
		&iHead
	) || (Payload.Size > (SIZE_MAX - iHead)) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_FRAME,
			"write-websocket-frame",
			"WebSocket frame size is not representable",
			xrtGetError()
		);
		return NULL;
	}
	iTotal = iHead + Payload.Size;
	if ( iTotal > (SIZE_MAX - sizeof(*pOutput)) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"write-websocket-frame",
			"WebSocket frame allocation size overflowed",
			NULL
		);
		return NULL;
	}
	pOutput = (struct __xrt_ws_output*)xrtMalloc(
		sizeof(*pOutput) + iTotal
	);
	if ( pOutput == NULL ) {
		(void)__xrtWsStreamReject(
			XERR_MEMORY,
			XWS_STREAM_ERROR_MEMORY,
			"write-websocket-frame",
			"WebSocket frame allocation failed",
			NULL
		);
		return NULL;
	}
	memset(pOutput, 0, sizeof(*pOutput));
	pOutput->Size = iTotal;
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		pOutput->Data,
		iHead,
		&iHead
	) ) {
		xrtFree(pOutput);
		(void)__xrtWsStreamReject(
			XERR_INTERNAL,
			XWS_STREAM_ERROR_FRAME,
			"write-websocket-frame",
			"WebSocket frame header construction failed",
			xrtGetError()
		);
		return NULL;
	}
	if ( Payload.Size != 0 ) {
		memcpy(
			pOutput->Data + iHead,
			Payload.Data,
			Payload.Size
		);
		if ( (Frame.Flags & XWS_FRAME_MASKED) != 0 ) {
			(void)xrtWsMask(
				pOutput->Data + iHead,
				Payload.Size,
				Frame.Mask,
				0
			);
		}
	}
	return pOutput;
}



/* 不分配内存地计算完整线路帧大小，用于先执行硬预算检查。 */
static bool __xrtWsStreamFrameSize(
	const xwsstream* pConnection,
	size_t iPayload,
	size_t* pSize
)
{
	size_t iHead = 2u;

	if ( iPayload > UINT16_MAX ) {
		iHead += 8u;
	} else if ( iPayload > XWS_CLOSE_PAYLOAD_MAX ) {
		iHead += 2u;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		iHead += XWS_MASK_SIZE;
	}
	if ( iPayload > (SIZE_MAX - iHead) ) {
		return false;
	}
	*pSize = iHead + iPayload;
	return true;
}



/* 在分配前统一检查帧上限、永久容量和瞬时预算。 */
xnetresult __xrtWsStreamFrameBudget(
	xwsstream* pConnection,
	size_t iPayload,
	__xrt_ws_send_class Class,
	size_t* pWireSize
)
{
	size_t iWireSize;
	size_t iBudget;
	bool bControl = Class != __XRT_WS_SEND_DATA;

	if ( (pConnection == NULL) || (pWireSize == NULL) ||
		(Class < __XRT_WS_SEND_DATA) ||
		(Class > __XRT_WS_SEND_CLOSE) ) {
		__xrtErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( (bControl && (iPayload > XWS_CLOSE_PAYLOAD_MAX)) ||
		(!bControl &&
		 ((uint64)iPayload > pConnection->Config.FrameLimit)) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-frame",
			bControl ?
				"WebSocket control payload exceeds 125 bytes" :
				"WebSocket data payload exceeds its frame limit",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsStreamFrameSize(
		pConnection,
		iPayload,
		&iWireSize
	) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsStreamTransportSize(
		pConnection,
		iWireSize,
		&iBudget
	) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsStreamCapacity(
		pConnection,
		Class
	) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame exceeds its permanent send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsStreamAvailable(
		pConnection,
		Class
	) ) {
		if ( Class == __XRT_WS_SEND_DATA ) {
			__xrtWsStreamBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	*pWireSize = iWireSize;
	return XNET_RESULT_OK;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* 把 TLS 尚未受理的帧尾追加到精确余量队列。 */
static void __xrtWsStreamOutputAppend(
	xwsstream* pConnection,
	__xrt_ws_output* pOutput
)
{
	pOutput->Next = NULL;
	if ( pConnection->OutputTail != NULL ) {
		pConnection->OutputTail->Next = pOutput;
	} else {
		pConnection->OutputHead = pOutput;
	}
	pConnection->OutputTail = pOutput;
	(void)xrtAtomic64FetchAdd(
		&pConnection->OutputBytes,
		(uint64)pOutput->Pending,
		XMEMORY_RELEASE
	);
}
#endif



/* 区分可重试的同步发送失败与已经破坏传输的会话故障。 */
void __xrtWsStreamSendFailure(
	xwsstream* pConnection,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( bFatal ) {
		(void)__xrtWsStreamRemember(
			pConnection,
			Kind,
			XWS_STREAM_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	} else {
		(void)__xrtWsStreamReject(
			Kind,
			XWS_STREAM_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	}
}



/* 向 TCP 或 TLS 提交一整个已计入预算的帧。 */
static xnetresult __xrtWsStreamSubmit(
	xwsstream* pConnection,
	__xrt_ws_output* pOutput,
	__xrt_ws_send_class Class
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	size_t iBudget;
	xnetresult Result;

	if ( pTransport == NULL ) {
		xrtFree(pOutput);
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtWsStreamTransportSize(
		pConnection,
		pOutput->Size,
		&iBudget
	) ) {
		xrtFree(pOutput);
		__xrtWsStreamSendFailure(
			pConnection,
			false,
			XERR_RANGE,
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsStreamAvailable(
		pConnection,
		Class
	) ) {
		xrtFree(pOutput);
		if ( Class == __XRT_WS_SEND_DATA ) {
			__xrtWsStreamBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			size_t iWritten = 0;
			xtlsresult TlsResult;

			if ( pConnection->OutputHead != NULL ) {
				pOutput->Pending = iBudget;
				__xrtWsStreamOutputAppend(
					pConnection,
					pOutput
				);
				pConnection->DrainPending = true;
				return XNET_RESULT_OK;
			}
			TlsResult = xrtTlsStreamSend(
				(xtlsstream*)pTransport,
				pOutput->Data,
				pOutput->Size,
				&iWritten
			);
			pOutput->Offset = iWritten;
			if ( (TlsResult == XTLS_ERROR) ||
				(TlsResult == XTLS_CLOSED) ) {
				const xerror* pCause =
					xrtTlsStreamError(
						(xtlsstream*)pTransport
					);

				xrtFree(pOutput);
				if ( pCause == NULL ) {
					pCause = xrtGetError();
				}
				__xrtWsStreamSendFailure(
					pConnection,
					xrtTlsStreamState(
						(xtlsstream*)pTransport
					) != XTLS_STREAM_OPEN,
					TlsResult == XTLS_CLOSED ?
						XERR_CLOSED : XERR_IO,
					"TLS rejected a WebSocket frame",
					pCause
				);
				return TlsResult == XTLS_CLOSED ?
					XNET_RESULT_CLOSED :
					XNET_RESULT_ERROR;
			}
			if ( pOutput->Offset < pOutput->Size ) {
				if ( !__xrtWsStreamTransportSize(
					pConnection,
					pOutput->Size - pOutput->Offset,
					&pOutput->Pending
				) ) {
					xrtFree(pOutput);
					__xrtWsStreamSendFailure(
						pConnection,
						true,
						XERR_INTERNAL,
						"TLS short-write budget became invalid",
						NULL
					);
					return XNET_RESULT_ERROR;
				}
				__xrtWsStreamOutputAppend(
					pConnection,
					pOutput
				);
			} else {
				xrtFree(pOutput);
			}
			pConnection->DrainPending = true;
			return XNET_RESULT_OK;
		}
	#endif
	Result = xrtNetStreamSendRef(
		(xnetstream*)pTransport,
		pOutput->Data,
		pOutput->Size,
		__xrtWsStreamOutputRelease,
		pOutput
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pOutput);
		if ( (Result == XNET_RESULT_AGAIN) &&
			(Class == __XRT_WS_SEND_DATA) ) {
			__xrtWsStreamBackpressure(pConnection);
		} else if ( Result == XNET_RESULT_ERROR ) {
			const xerror* pCause = xrtNetStreamError(
				(xnetstream*)pTransport
			);

			if ( pCause == NULL ) {
				pCause = xrtGetError();
			}
			__xrtWsStreamSendFailure(
				pConnection,
				xrtNetStreamState(
					(xnetstream*)pTransport
				) != XNET_STREAM_OPEN,
				pCause != NULL ?
					xrtErrorKind(pCause) : XERR_IO,
				"TCP rejected a WebSocket frame",
				pCause
			);
		}
		return Result;
	}
	pConnection->DrainPending = true;
	return XNET_RESULT_OK;
}



/* 验证并发送一个数据或控制帧。 */
xnetresult __xrtWsStreamSendFrame(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xrt_ws_send_class Class,
	bool bCompressed
)
{
	__xrt_ws_output* pOutput;
	size_t iWireSize;
	xnetresult Budget;

	if ( !__xrtRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-frame",
			"WebSocket payload range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	Budget = __xrtWsStreamFrameBudget(
		pConnection,
		Payload.Size,
		Class,
		&iWireSize
	);
	if ( Budget != XNET_RESULT_OK ) {
		return Budget;
	}
	pOutput = __xrtWsStreamFrame(
		pConnection,
		Opcode,
		Payload,
		bFinal,
		bCompressed
	);
	if ( pOutput == NULL ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtWsStreamSubmit(
		pConnection,
		pOutput,
		Class
	);
}



/* 取消关闭计时器；终态回调仍负责释放其 Connection 引用。 */
static void __xrtWsStreamCancelCloseTimer(xwsstream* pConnection)
{
	xnetengine* pEngine;

	if ( pConnection->CloseTimer == 0 ) {
		return;
	}
	pEngine = xrtNetWorkerEngine(pConnection->Worker);
	if ( !__xrtNetEngineTimerCancelLifecycle(
		pEngine,
		pConnection->CloseTimer
	) ) {
		xrtClearError();
	}
}



/* 排空条件满足后开始唯一的底层传输关闭。 */
static void __xrtWsStreamCloseTransportStart(xwsstream* pConnection)
{
	ptr pTransport;
	bool bAccepted;

	if ( pConnection->TransportClosing ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	pConnection->TransportClosing = true;
	__xrtWsStreamCancelCloseTimer(pConnection);
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			bAccepted = xrtTlsStreamClose(
				(xtlsstream*)pTransport
			);
		} else
	#endif
	{
		bAccepted = xrtNetStreamClose(
			(xnetstream*)pTransport
		);
	}
	if ( !bAccepted ) {
		xrtClearError();
		(void)xrtWsStreamAbort(pConnection);
	}
}



/* 远端 Close 或协议失败后的新输入到达后，排空输出并关闭底层传输。 */
static void __xrtWsStreamCloseTransport(xwsstream* pConnection)
{
	if ( (!pConnection->CloseReceived &&
		 !(pConnection->ProtocolFailed &&
		   pConnection->ProtocolPeerActivity)) ||
		(pConnection->OutputHead != NULL) ) {
		return;
	}
	__xrtWsStreamCloseTransportStart(pConnection);
}



/* 关闭计时器只在远端没有回应时把握手变为明确超时。 */
static void __xrtWsStreamCloseTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pWorker;
	if ( pConnection->CloseTimer == Id ) {
		pConnection->CloseTimer = 0;
		if ( (Result == XNET_RESULT_OK) &&
			!pConnection->CloseReceived &&
			(xrtWsStreamState(pConnection) !=
			 XWS_STREAM_CLOSED) ) {
			(void)__xrtWsStreamRemember(
				pConnection,
				XERR_TIMEOUT,
				XWS_STREAM_ERROR_TIMEOUT,
				"close-websocket",
				"WebSocket close handshake timed out",
				NULL
			);
			__xrtWsStreamEmitError(pConnection);
			(void)xrtWsStreamAbort(pConnection);
		}
	}
	xrtWsStreamDestroy(pConnection);
}



/* 首个本地 Close 建立独立握手超时。 */
static bool __xrtWsStreamStartCloseTimer(xwsstream* pConnection)
{
	if ( (pConnection->Config.CloseTimeout == 0) ||
		(pConnection->CloseTimer != 0) ||
		pConnection->CloseReceived ) {
		return true;
	}
	if ( xrtWsStreamRef(pConnection) == NULL ) {
		return false;
	}
	pConnection->CloseTimer = xrtNetEngineAfter(
		xrtNetWorkerEngine(pConnection->Worker),
		xrtNetWorkerIndex(pConnection->Worker),
		pConnection->Config.CloseTimeout,
		__xrtWsStreamCloseTimer,
		pConnection
	);
	if ( pConnection->CloseTimer == 0 ) {
		xrtWsStreamDestroy(pConnection);
		(void)__xrtWsStreamRemember(
			pConnection,
			XERR_AGAIN,
			XWS_STREAM_ERROR_TIMEOUT,
			"close-websocket",
			"WebSocket close timer could not be scheduled",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 在同步终态重入期间保护会话，并提交唯一的本地 Close。 */
static xnetresult __xrtWsStreamClosePayload(
	xwsstream* pConnection,
	xbytesview Payload,
	uint16 iCode,
	bool bRemote
)
{
	uint32 iState;
	xnetresult Result;

	if ( xrtWsStreamRef(pConnection) == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	__xrtSpinLock(&pConnection->TransportLock);
	if ( pConnection->CloseSent ||
		((iState = xrtAtomic32Load(
			&pConnection->State,
			XMEMORY_ACQUIRE
		 )) != XWS_STREAM_OPEN) ) {
		__xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsStreamDestroy(pConnection);
		return XNET_RESULT_CLOSED;
	}
	/*
		先发布唯一 Close 意图。底层发送可同步触发终态回调，
		终态快照必须在该重入窗口内看到完整的本地关闭信息。
	*/
	pConnection->CloseSent = true;
	pConnection->LocalCode = iCode;
	pConnection->RemoteInitiated = bRemote;
	__xrtSpinUnlock(&pConnection->TransportLock);
	Result = __xrtWsStreamSendFrame(
		pConnection,
		XWS_OPCODE_CLOSE,
		Payload,
		true,
		__XRT_WS_SEND_CLOSE,
		false
	);
	if ( Result != XNET_RESULT_OK ) {
		/* 未受理的 Close 不占用唯一发送槽，调用方可以重试。 */
		__xrtSpinLock(&pConnection->TransportLock);
		if ( xrtWsStreamState(pConnection) != XWS_STREAM_CLOSED ) {
			pConnection->CloseSent = false;
			pConnection->LocalCode = 0;
			pConnection->RemoteInitiated = false;
		}
		__xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsStreamDestroy(pConnection);
		return Result;
	}
	/* 同步终态可能已经写入 CLOSED，只允许从 OPEN 单向推进。 */
	iState = XWS_STREAM_OPEN;
	(void)xrtAtomic32CompareExchange(
		&pConnection->State,
		&iState,
		XWS_STREAM_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	if ( (iState != XWS_STREAM_CLOSED) &&
		xrtWsStreamPaused(pConnection) ) {
		(void)xrtWsStreamResume(pConnection);
	}
	if ( (iState != XWS_STREAM_CLOSED) &&
		!__xrtWsStreamStartCloseTimer(pConnection) ) {
		__xrtWsStreamEmitError(pConnection);
		(void)xrtWsStreamAbort(pConnection);
		xrtWsStreamDestroy(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( iState != XWS_STREAM_CLOSED ) {
		__xrtWsStreamCloseTransport(pConnection);
	}
	xrtWsStreamDestroy(pConnection);
	return XNET_RESULT_OK;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* TLS 可写边沿继续提交之前发生短写的精确帧余量。 */
static bool __xrtWsStreamOutputDrive(xwsstream* pConnection)
{
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	while ( (pStream != NULL) &&
		(pConnection->OutputHead != NULL) ) {
		__xrt_ws_output* pOutput =
			pConnection->OutputHead;
		size_t iRemaining =
			pOutput->Size - pOutput->Offset;
		size_t iPending = pOutput->Pending;
		size_t iWritten = 0;
		xtlsresult Result = xrtTlsStreamSend(
			pStream,
			pOutput->Data + pOutput->Offset,
			iRemaining,
			&iWritten
		);

		if ( iWritten != 0 ) {
			pOutput->Offset += iWritten;
			if ( !__xrtWsStreamTransportSize(
				pConnection,
				pOutput->Size - pOutput->Offset,
				&pOutput->Pending
			) || (pOutput->Pending > iPending) ) {
				(void)__xrtWsStreamRemember(
					pConnection,
					XERR_INTERNAL,
					XWS_STREAM_ERROR_SEND,
					"drain-websocket-output",
					"TLS short-write accounting became invalid",
					NULL
				);
				__xrtWsStreamEmitError(pConnection);
				(void)xrtWsStreamAbort(pConnection);
				return false;
			}
			(void)xrtAtomic64FetchSub(
				&pConnection->OutputBytes,
				(uint64)(iPending - pOutput->Pending),
				XMEMORY_RELEASE
			);
		}
		if ( pOutput->Offset == pOutput->Size ) {
			pConnection->OutputHead = pOutput->Next;
			if ( pConnection->OutputHead == NULL ) {
				pConnection->OutputTail = NULL;
			}
			xrtFree(pOutput);
		}
		if ( (Result == XTLS_ERROR) ||
			(Result == XTLS_CLOSED) ) {
			(void)__xrtWsStreamRemember(
				pConnection,
				Result == XTLS_CLOSED ?
					XERR_CLOSED : XERR_IO,
				XWS_STREAM_ERROR_SEND,
				"drain-websocket-output",
				"TLS could not continue a WebSocket frame",
				xrtTlsStreamError(pStream)
			);
			__xrtWsStreamEmitError(pConnection);
			(void)xrtWsStreamAbort(pConnection);
			return false;
		}
		if ( (Result == XTLS_AGAIN) ||
			(iWritten == 0) ) {
			break;
		}
	}
	if ( pConnection->OutputHead == NULL ) {
		__xrtWsStreamWritableEvent(pConnection);
		__xrtWsStreamCloseTransport(pConnection);
	}
	return true;
}
#endif



/* 返回当前 TCP 或 TLS 明文接收链。 */
static const xnetbuf* __xrtWsStreamBuffer(xwsstream* pConnection)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return xrtTlsStreamBuffer(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return xrtNetStreamBuffer((xnetstream*)pTransport);
}



/* 精确消费 TCP 或 TLS 明文，并保持各自恢复读取的内部契约。 */
static bool __xrtWsStreamConsume(
	xwsstream* pConnection,
	size_t iSize
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return xrtTlsStreamConsume(
				(xtlsstream*)pTransport,
				iSize
			);
		}
	#endif
	return xrtNetStreamConsume(
		(xnetstream*)pTransport,
		iSize
	) == iSize;
}



/* TLS 在保留不完整帧头时显式请求继续累积受限明文。 */
static bool __xrtWsStreamReadMore(xwsstream* pConnection)
{
	(void)pConnection;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
				&pConnection->Transport,
				XMEMORY_ACQUIRE
			);

			return (pStream != NULL) &&
				((xrtTlsStreamAvailable(pStream) == 0) ||
				 xrtTlsStreamReadMore(pStream));
		}
	#endif
	return true;
}



/* 丢弃协议失败时已经进入明文缓冲的不可恢复输入。 */
static bool __xrtWsStreamDiscardInput(xwsstream* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsStreamBuffer(pConnection);
	size_t iSize = pBuffer != NULL ?
		xrtNetBufSize(pBuffer) : 0;

	return (iSize == 0) ||
		__xrtWsStreamConsume(pConnection, iSize);
}



/* 校验一段扩展解码后的语义负载并发布消息数据。 */
static bool __xrtWsStreamPayloadSemantic(
	xwsstream* pConnection,
	xbytesview Payload
)
{
	xwsmessageerrorinfo Error;

	if ( !xrtWsMessagePayload(
		&pConnection->Message,
		Payload,
		&Error
	) ) {
		pConnection->FailureCloseCode =
			Error.CloseCode != 0 ?
			Error.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsStreamRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_STREAM_ERROR_MESSAGE,
			"read-websocket-message",
			"WebSocket message payload is invalid",
			xrtGetError()
		);
		return false;
	}
	if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) != 0 ) {
		if ( Payload.Size >
			(sizeof(pConnection->Control) -
			 pConnection->ControlSize) ) {
			(void)__xrtWsStreamRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_STREAM_ERROR_MESSAGE,
				"read-websocket-control",
				"WebSocket control payload overflowed",
				NULL
			);
			return false;
		}
		if ( Payload.Size != 0 ) {
			memcpy(
				pConnection->Control +
					pConnection->ControlSize,
				Payload.Data,
				Payload.Size
			);
		}
		pConnection->ControlSize += Payload.Size;
	} else if ( pConnection->MessageOpen &&
		(pConnection->Events.MessageData != NULL) ) {
		pConnection->Events.MessageData(
			pConnection,
			Payload,
			pConnection->Data
		);
	}
	return true;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 把 Inflater 的同步输出接入消息校验与应用事件。 */
static bool __xrtWsStreamInflateOutput(
	xbytesview Data,
	ptr pData
)
{
	return __xrtWsStreamPayloadSemantic(
		(xwsstream*)pData,
		Data
	);
}



/* 把压缩层错误固定映射为消息过大或非法扩展数据。 */
static bool __xrtWsStreamInflateFailure(
	xwsstream* pConnection,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	if ( pConnection->FailureCloseCode != 0 ) {
		return false;
	}
	pConnection->FailureCloseCode =
		(pCause != NULL) &&
		(xrtErrorDomain(pCause) != NULL) &&
		(strcmp(
			xrtErrorDomain(pCause),
			"xrt.websocket.deflate"
		 ) == 0) &&
		(xrtErrorCode(pCause) ==
		 XWS_DEFLATE_ERROR_LIMIT) ?
			XWS_CLOSE_TOO_BIG :
			XWS_CLOSE_INVALID_DATA;
	(void)__xrtWsStreamRemember(
		pConnection,
		pCause != NULL ?
			xrtErrorKind(pCause) : XERR_PROTOCOL,
		XWS_STREAM_ERROR_MESSAGE,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}
#endif



/* 解码一段线路负载；未协商压缩时保持零复制直通。 */
static bool __xrtWsStreamPayloadWire(
	xwsstream* pConnection,
	xbytesview Payload
)
{
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_CONTROL) == 0) ) {
			if ( !xrtWsInflaterWrite(
				pConnection->Inflater,
				Payload,
				__xrtWsStreamInflateOutput,
				pConnection
			) ) {
				return __xrtWsStreamInflateFailure(
					pConnection,
					"inflate-websocket-message",
					"WebSocket compressed payload is invalid"
				);
			}
			return true;
		}
	#endif
	return __xrtWsStreamPayloadSemantic(
		pConnection,
		Payload
	);
}



/* 把本地控制帧提交失败映射为 1011，而不是归咎于对端协议。 */
static bool __xrtWsStreamControlFailure(
	xwsstream* pConnection,
	xnetresult Result,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	pConnection->FailureCloseCode = XWS_CLOSE_INTERNAL;
	(void)__xrtWsStreamRemember(
		pConnection,
		pCause != NULL ? xrtErrorKind(pCause) :
			(Result == XNET_RESULT_AGAIN ? XERR_AGAIN : XERR_IO),
		XWS_STREAM_ERROR_SEND,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 处理已经完整校验的 Ping、Pong 或 Close 控制帧。 */
static bool __xrtWsStreamControl(xwsstream* pConnection)
{
	xbytesview Payload = {
		pConnection->Control,
		pConnection->ControlSize
	};

	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_PING ) {
		if ( pConnection->Config.AutoPong &&
			(xrtWsStreamState(pConnection) == XWS_STREAM_OPEN) ) {
			xnetresult Result = __xrtWsStreamSendFrame(
				pConnection,
				XWS_OPCODE_PONG,
				Payload,
				true,
				__XRT_WS_SEND_AUTO_PONG,
				false
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsStreamControlFailure(
					pConnection,
					Result,
					"reply-websocket-ping",
					"WebSocket automatic Pong could not be submitted"
				);
			}
		}
		if ( pConnection->Events.Ping != NULL ) {
			pConnection->Events.Ping(
				pConnection,
				Payload,
				pConnection->Data
			);
		}
		return true;
	}
	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_PONG ) {
		if ( pConnection->Events.Pong != NULL ) {
			pConnection->Events.Pong(
				pConnection,
				Payload,
				pConnection->Data
			);
		}
		return true;
	}
	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_CLOSE ) {
		xwsclose Close;

		if ( !xrtWsCloseParse(Payload, &Close) ) {
			pConnection->FailureCloseCode =
				(xrtErrorCode(xrtGetError()) ==
				 XWS_CLOSE_ERROR_UTF8) ?
				XWS_CLOSE_INVALID_DATA :
				XWS_CLOSE_PROTOCOL;
			(void)__xrtWsStreamRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_STREAM_ERROR_MESSAGE,
				"read-websocket-close",
				"WebSocket Close payload is invalid",
				xrtGetError()
			);
			return false;
		}
		__xrtSpinLock(&pConnection->TransportLock);
		pConnection->CloseReceived = true;
		pConnection->RemoteCode = Close.Code;
		pConnection->RemoteReasonSize =
			(uint16)Close.Reason.Size;
		if ( Close.Reason.Size != 0 ) {
			memcpy(
				pConnection->RemoteReason,
				Close.Reason.Data,
				Close.Reason.Size
			);
		}
		pConnection->RemoteReason[
			pConnection->RemoteReasonSize
		] = '\0';
		__xrtSpinUnlock(&pConnection->TransportLock);
		if ( !pConnection->CloseSent ) {
			xnetresult Result = __xrtWsStreamClosePayload(
				pConnection,
				Payload,
				Close.Code,
				true
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsStreamControlFailure(
					pConnection,
					Result,
					"reply-websocket-close",
					"WebSocket Close reply could not be submitted"
				);
			}
		} else {
			__xrtWsStreamCloseTransport(pConnection);
		}
		return true;
	}
	return false;
}



/* 完成当前帧，并在协议校验成功后发布消息 End 或控制事件。 */
static bool __xrtWsStreamFrameEnd(xwsstream* pConnection)
{
	xwsmessageerrorinfo Error;
	bool bControl =
		(pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) != 0;

	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			!bControl &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_END) != 0) &&
			!xrtWsInflaterEnd(
				pConnection->Inflater,
				__xrtWsStreamInflateOutput,
				pConnection
			) ) {
			return __xrtWsStreamInflateFailure(
				pConnection,
				"finish-websocket-inflate",
				"WebSocket compressed message could not finish"
			);
		}
	#endif
	if ( !xrtWsMessageFrameEnd(
		&pConnection->Message,
		&Error
	) ) {
		pConnection->FailureCloseCode =
			Error.CloseCode != 0 ?
			Error.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsStreamRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_STREAM_ERROR_MESSAGE,
			"finish-websocket-frame",
			"WebSocket message frame is incomplete or invalid",
			xrtGetError()
		);
		return false;
	}
	if ( bControl ) {
		if ( !__xrtWsStreamControl(pConnection) ) {
			return false;
		}
	} else if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_END) != 0 ) {
		if ( pConnection->MessageOpen &&
			(pConnection->Events.MessageEnd != NULL) ) {
			pConnection->Events.MessageEnd(
				pConnection,
				pConnection->Data
			);
		}
		pConnection->MessageOpen = false;
	}
	pConnection->FrameActive = false;
	pConnection->FrameRemaining = 0;
	pConnection->FrameOffset = 0;
	pConnection->ControlSize = 0;
	return true;
}



/* 解析一个完整帧头并建立消息层语义状态。 */
static int __xrtWsStreamFrameBegin(xwsstream* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsStreamBuffer(pConnection);
	xwsframeconfig Config;
	xwsframeerrorinfo FrameError;
	xwsmessageerrorinfo MessageError;
	uint8 Header[XWS_FRAME_HEAD_MAX];
	size_t iAvailable;
	size_t iHeader;
	xwsframestatus Status;

	if ( pBuffer == NULL ) {
		return -1;
	}
	iAvailable = xrtNetBufSize(pBuffer);
	if ( iAvailable == 0 ) {
		return 0;
	}
	iHeader = iAvailable < sizeof(Header) ?
		iAvailable : sizeof(Header);
	if ( xrtNetBufPeek(
		pBuffer,
		0,
		Header,
		iHeader
	) != iHeader ) {
		return -1;
	}
	xrtWsFrameConfigInit(&Config);
	Config.MaxPayload = pConnection->Config.FrameLimit;
	Config.Mask = pConnection->Config.Role ==
		XWS_ROLE_SERVER ?
		XWS_MASK_REQUIRED : XWS_MASK_FORBIDDEN;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( pConnection->Config.DeflateEnabled ) {
			Config.AllowedRsv = XWS_FRAME_RSV1;
		}
	#endif
	Status = xrtWsFrameParse(
		(xbytesview) { Header, iHeader },
		&pConnection->Frame,
		&Config,
		&FrameError
	);
	if ( Status == XWS_FRAME_MORE ) {
		return 0;
	}
	if ( Status == XWS_FRAME_ERROR ) {
		pConnection->FailureCloseCode =
			(FrameError.Code == XWS_FRAME_ERROR_LENGTH) &&
			(xrtErrorKind(xrtGetError()) == XERR_RANGE) ?
				XWS_CLOSE_TOO_BIG : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsStreamRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_STREAM_ERROR_FRAME,
			"read-websocket-frame",
			"WebSocket frame header is invalid",
			xrtGetError()
		);
		return -1;
	}
	if ( !__xrtWsStreamConsume(
		pConnection,
		pConnection->Frame.HeadSize
	) ) {
		(void)__xrtWsStreamRemember(
			pConnection,
			XERR_INTERNAL,
			XWS_STREAM_ERROR_TRANSPORT,
			"consume-websocket-frame",
			"WebSocket transport did not consume its frame header",
			xrtGetError()
		);
		return -2;
	}
	if ( !xrtWsMessageFrameBegin(
		&pConnection->Message,
		&pConnection->Frame,
		&pConnection->MessageInfo,
		&MessageError
	) ) {
		pConnection->FailureCloseCode =
			MessageError.CloseCode != 0 ?
			MessageError.CloseCode : XWS_CLOSE_PROTOCOL;
		(void)__xrtWsStreamRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_STREAM_ERROR_MESSAGE,
			"begin-websocket-message",
			"WebSocket frame violates message sequencing",
			xrtGetError()
		);
		return -1;
	}
	pConnection->FrameActive = true;
	pConnection->FrameRemaining =
		pConnection->Frame.PayloadSize;
	pConnection->FrameOffset = 0;
	pConnection->ControlSize = 0;
	if ( (pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) == 0 ) {
		if ( (pConnection->MessageInfo.Flags &
			 XWS_MESSAGE_BEGIN) != 0 ) {
			#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
				if ( pConnection->Config.DeflateEnabled &&
					!xrtWsInflaterBegin(
						pConnection->Inflater,
						(pConnection->MessageInfo.Flags &
						 XWS_MESSAGE_EXTENDED) != 0
					) ) {
					(void)__xrtWsStreamInflateFailure(
						pConnection,
						"begin-websocket-inflate",
						"WebSocket compressed message could not begin"
					);
					return -1;
				}
			#endif
			pConnection->MessageOpen = true;
			if ( pConnection->Events.MessageBegin != NULL ) {
				#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
					if ( (pConnection->MessageInfo.Rsv &
						  XWS_FRAME_RSV1) != 0 ) {
						pConnection->MessageInfo.Flags |=
							XWS_MESSAGE_COMPRESSED;
					}
				#endif
				pConnection->Events.MessageBegin(
					pConnection,
					&pConnection->MessageInfo,
					pConnection->Data
				);
			}
		}
	}
	return 1;
}



/* 失败后优先发送标准 Close；控制预算也不可用时立即中止。 */
static void __xrtWsStreamProtocolFail(
	xwsstream* pConnection,
	uint16 iCloseCode
)
{
	uint8 Payload[2];
	xbytesview View;

	__xrtWsStreamEmitError(pConnection);
	if ( pConnection->CloseSent ||
		(xrtWsStreamState(pConnection) == XWS_STREAM_CLOSED) ) {
		(void)xrtWsStreamAbort(pConnection);
		return;
	}
	pConnection->ProtocolFailed = true;
	Payload[0] = (uint8)(iCloseCode >> 8u);
	Payload[1] = (uint8)iCloseCode;
	View.Data = Payload;
	View.Size = sizeof(Payload);
	if ( __xrtWsStreamClosePayload(
		pConnection,
		View,
		iCloseCode,
		false
	) != XNET_RESULT_OK ) {
		(void)xrtWsStreamAbort(pConnection);
	} else if ( !__xrtWsStreamDiscardInput(pConnection) ) {
		(void)xrtWsStreamAbort(pConnection);
	}
}



/* 增量消费任意网络分块，不为消息或帧建立固定连接缓冲。 */
static void __xrtWsStreamRead(xwsstream* pConnection)
{
	uint8 Scratch[__XRT_WS_MASK_CHUNK];

	if ( pConnection->Reading ||
		pConnection->ProtocolFailed ||
		xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		return;
	}
	/*
		消息与控制帧回调可以同步关闭底层传输并释放其 Connection 引用。
		读取状态机必须独立持有活动引用，直到不再访问任何会话字段。
	*/
	if ( xrtWsStreamRef(pConnection) == NULL ) {
		return;
	}
	pConnection->Reading = true;
	while ( (xrtWsStreamState(pConnection) !=
		 XWS_STREAM_CLOSED) &&
		!xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		const xnetbuf* pBuffer;
		xnetspan Span;
		size_t iChunk;

		if ( !pConnection->FrameActive ) {
			int iBegin = __xrtWsStreamFrameBegin(pConnection);

			if ( iBegin == 0 ) {
				if ( !__xrtWsStreamReadMore(pConnection) ) {
					(void)__xrtWsStreamRemember(
						pConnection,
						xrtErrorKind(xrtGetError()),
						XWS_STREAM_ERROR_TRANSPORT,
						"read-more-websocket-frame",
						"WebSocket TLS frame prefix could not grow",
						xrtGetError()
					);
					__xrtWsStreamEmitError(pConnection);
					(void)xrtWsStreamAbort(pConnection);
				}
				break;
			}
			if ( iBegin < 0 ) {
				if ( iBegin == -2 ) {
					__xrtWsStreamEmitError(pConnection);
					(void)xrtWsStreamAbort(pConnection);
				} else {
					__xrtWsStreamProtocolFail(
						pConnection,
						pConnection->FailureCloseCode != 0 ?
							pConnection->FailureCloseCode :
							XWS_CLOSE_PROTOCOL
					);
				}
				break;
			}
			if ( xrtAtomic32Load(
				&pConnection->ReadPaused,
				XMEMORY_ACQUIRE
			) ) {
				break;
			}
		}
		if ( pConnection->FrameRemaining == 0 ) {
			if ( !__xrtWsStreamFrameEnd(pConnection) ) {
				__xrtWsStreamProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
			continue;
		}
		pBuffer = __xrtWsStreamBuffer(pConnection);
		if ( (pBuffer == NULL) ||
			!xrtNetBufFront(pBuffer, &Span) ) {
			break;
		}
		iChunk = Span.Size;
		if ( (uint64)iChunk >
			pConnection->FrameRemaining ) {
			iChunk = (size_t)
				pConnection->FrameRemaining;
		}
		if ( (pConnection->Frame.Flags &
			 XWS_FRAME_MASKED) != 0 ) {
			if ( iChunk > sizeof(Scratch) ) {
				iChunk = sizeof(Scratch);
			}
			memcpy(Scratch, Span.Data, iChunk);
			if ( !xrtWsMask(
				Scratch,
				iChunk,
				pConnection->Frame.Mask,
				pConnection->FrameOffset
			) || !__xrtWsStreamPayloadWire(
				pConnection,
				(xbytesview) { Scratch, iChunk }
			) ) {
				__xrtWsStreamProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
		} else if ( !__xrtWsStreamPayloadWire(
			pConnection,
			(xbytesview) { Span.Data, iChunk }
		) ) {
			__xrtWsStreamProtocolFail(
				pConnection,
				pConnection->FailureCloseCode != 0 ?
					pConnection->FailureCloseCode :
					XWS_CLOSE_PROTOCOL
			);
			break;
		}
		if ( !__xrtWsStreamConsume(pConnection, iChunk) ) {
			(void)__xrtWsStreamRemember(
				pConnection,
				XERR_INTERNAL,
				XWS_STREAM_ERROR_TRANSPORT,
				"consume-websocket-input",
				"WebSocket transport did not consume its payload",
				xrtGetError()
			);
			__xrtWsStreamEmitError(pConnection);
			(void)xrtWsStreamAbort(pConnection);
			break;
		}
		pConnection->FrameRemaining -= iChunk;
		pConnection->FrameOffset += iChunk;
	}
	pConnection->Reading = false;
	xrtWsStreamDestroy(pConnection);
}



/* 在所属 Worker 上处理早到数据，并按应用暂停状态恢复 TCP 读取。 */
static void __xrtWsStreamDrive(
	xnetworker* pWorker,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pConnection->DrivePosted,
		0,
		XMEMORY_RELEASE
	);
	if ( !xrtAtomic32Load(
		&pConnection->ReadPaused,
		XMEMORY_ACQUIRE
	) ) {
		__xrtWsStreamRead(pConnection);
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			xrtWsStreamDestroy(pConnection);
			return;
		}
	#endif
	if ( (xrtWsStreamState(pConnection) !=
		 XWS_STREAM_CLOSED) &&
		!xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
			&pConnection->Transport,
			XMEMORY_ACQUIRE
		);

		if ( (pStream != NULL) &&
			!xrtNetStreamResume(pStream) ) {
			xrtClearError();
		}
	}
	xrtWsStreamDestroy(pConnection);
}



/* 合并任意线程的恢复请求，并为嵌入命令持有一份 Connection 引用。 */
static void __xrtWsStreamDriveSchedule(xwsstream* pConnection)
{
	uint32 iExpected = 0;

	if ( !xrtAtomic32CompareExchange(
		&pConnection->DrivePosted,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( xrtWsStreamRef(pConnection) == NULL ) {
		xrtAtomic32Store(
			&pConnection->DrivePosted,
			0,
			XMEMORY_RELEASE
		);
		return;
	}
	__xrtNetEnginePostInternal(
		pConnection->Worker,
		&pConnection->DriveCommand,
		__xrtWsStreamDrive,
		pConnection
	);
}



/* 暂停后续应用消息分块；TCP 立即停止新接收，TLS 由未消费明文施加背压。 */
XRT_API void xrtWsStreamPause(xwsstream* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"pause-websocket-read"
	) || (xrtWsStreamState(pConnection) != XWS_STREAM_OPEN) ) {
		return;
	}
	xrtAtomic32Store(
		&pConnection->ReadPaused,
		1,
		XMEMORY_RELEASE
	);
	if ( xrtWsStreamState(pConnection) != XWS_STREAM_OPEN ) {
		xrtAtomic32Store(
			&pConnection->ReadPaused,
			0,
			XMEMORY_RELEASE
		);
		__xrtWsStreamDriveSchedule(pConnection);
		return;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			return;
		}
	#endif
	pTransport = __xrtWsStreamTransportRef(pConnection);
	if ( pTransport != NULL ) {
		xrtNetStreamPause((xnetstream*)pTransport);
	}
	__xrtWsStreamTransportRelease(pConnection, pTransport);
}



/* 恢复读取，并让所属 Worker 继续消费已缓冲的 TCP 或 TLS 明文。 */
XRT_API bool xrtWsStreamResume(xwsstream* pConnection)
{
	if ( pConnection == NULL ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"resume-websocket-read",
			"WebSocket connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"resume-websocket-read"
	) ) {
		return false;
	}
	if ( xrtWsStreamState(pConnection) == XWS_STREAM_CLOSED ) {
		(void)__xrtWsStreamReject(
			XERR_CLOSED,
			XWS_STREAM_ERROR_STATE,
			"resume-websocket-read",
			"WebSocket connection is closed",
			NULL
		);
		return false;
	}
	xrtAtomic32Store(
		&pConnection->ReadPaused,
		0,
		XMEMORY_RELEASE
	);
	__xrtWsStreamDriveSchedule(pConnection);
	return true;
}



/* 返回应用读取暂停状态的并发快照。 */
XRT_API bool xrtWsStreamPaused(const xwsstream* pConnection)
{
	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-paused"
	) ) {
		return false;
	}
	return xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) != 0;
}



/* 对端 TCP 半关闭且没有 Close 帧时终止为非完整 WebSocket 关闭。 */
static void __xrtWsStreamTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pStream;
	if ( !pConnection->CloseReceived ) {
		pConnection->TransportClosing = true;
		if ( !xrtNetStreamClose(pStream) ) {
			xrtClearError();
			(void)xrtNetStreamAbort(pStream);
		}
	}
}



/* TCP Read 直接驱动统一消息状态机。 */
static void __xrtWsStreamTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsStreamDiscardInput(pConnection) ) {
			(void)xrtWsStreamAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsStreamCloseTransport(pConnection);
		return;
	}
	if ( xrtWsStreamPaused(pConnection) ) {
		xrtNetStreamPause(pStream);
		return;
	}
	__xrtWsStreamRead(pConnection);
}



/* TCP 高水位和 Connection 自身预算共同折叠为一个背压状态。 */
static void __xrtWsStreamTcpHigh(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsStreamBackpressure((xwsstream*)pData);
}



/* TCP 回落到低水位后重新计算真正可写预算。 */
static void __xrtWsStreamTcpLow(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsStreamWritableEvent((xwsstream*)pData);
}



/* 两级队列排空后发布唯一 Drain 边沿。 */
static void __xrtWsStreamDrain(xwsstream* pConnection)
{
	if ( !pConnection->DrainPending ||
		(xrtWsStreamPending(pConnection) != 0) ) {
		return;
	}
	pConnection->DrainPending = false;
	__xrtWsStreamWritableEvent(pConnection);
	if ( pConnection->Events.Drain != NULL ) {
		pConnection->Events.Drain(
			pConnection,
			pConnection->Data
		);
	}
}



/* TCP 排空回调完成 WebSocket Drain 边沿。 */
static void __xrtWsStreamTcpDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtWsStreamDrain((xwsstream*)pData);
}



/* 复制当前 Close 终态供同步回调和后续查询共用。 */
static void __xrtWsStreamCloseSnapshot(
	const xwsstream* pConnection,
	xwsstreamclose* pClose
)
{
	xwsstream* pMutable = (xwsstream*)pConnection;
	xwsstreamclose Close;

	memset(&Close, 0, sizeof(Close));
	__xrtSpinLock(&pMutable->TransportLock);
	if ( pConnection->CloseSent ) {
		Close.Flags |= XWS_STREAM_CLOSE_SENT;
	}
	if ( pConnection->CloseReceived ) {
		Close.Flags |= XWS_STREAM_CLOSE_RECEIVED;
	}
	if ( pConnection->CloseSent &&
		pConnection->CloseReceived &&
		((xnetresult)xrtAtomic32Load(
			&pConnection->TransportResult,
			XMEMORY_ACQUIRE
		 ) == XNET_RESULT_OK) ) {
		Close.Flags |= XWS_STREAM_CLOSE_CLEAN;
	}
	if ( pConnection->RemoteInitiated ) {
		Close.Flags |= XWS_STREAM_CLOSE_REMOTE;
	}
	Close.Transport = (xnetresult)xrtAtomic32Load(
		&pConnection->TransportResult,
		XMEMORY_ACQUIRE
	);
	Close.LocalCode = pConnection->LocalCode;
	Close.RemoteCode = pConnection->RemoteCode;
	Close.Reason.Data = pConnection->RemoteReason;
	Close.Reason.Size = pConnection->RemoteReasonSize;
	__xrtSpinUnlock(&pMutable->TransportLock);
	memcpy(pClose, &Close, sizeof(Close));
}



/* 验证 Connection 及其协商协议副本占用的完整连续存储。 */
bool __xrtWsStreamStorageRange(
	const xwsstream* pConnection,
	size_t* pSize
)
{
	size_t iSize;

	if ( !__xrtWsStreamRangeValid(pConnection) ||
		(pConnection->Protocol.Size >
		 (SIZE_MAX - sizeof(*pConnection) - 1u)) ) {
		return false;
	}
	iSize = sizeof(*pConnection) +
		pConnection->Protocol.Size + 1u;
	if ( !__xrtRangeValid(pConnection, iSize) ) {
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 统一处理 TCP/TLS 传输终态并释放 Connection 的传输引用。 */
static void __xrtWsStreamTransportClose(
	xwsstream* pConnection,
	ptr pTransport,
	xnetresult Result,
	const xerror* pError
)
{
	xwsstreamclose Close;
	ptr pOwned;

	xrtAtomic32Store(
		&pConnection->TransportResult,
		(uint32)Result,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XWS_STREAM_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtWsStreamCancelCloseTimer(pConnection);
	if ( (Result != XNET_RESULT_OK) &&
		(pError != NULL) &&
		(xrtWsStreamError(pConnection) == NULL) ) {
		(void)__xrtWsStreamRemember(
			pConnection,
			xrtErrorKind(pError),
			XWS_STREAM_ERROR_TRANSPORT,
			"close-websocket-transport",
			"WebSocket transport closed with an error",
			pError
		);
		__xrtWsStreamEmitError(pConnection);
	}
	__xrtWsStreamOutputClear(pConnection);
	if ( !pConnection->CloseEmitted ) {
		pConnection->CloseEmitted = true;
		__xrtWsStreamCloseSnapshot(pConnection, &Close);
		if ( pConnection->Events.Close != NULL ) {
			pConnection->Events.Close(
				pConnection,
				&Close,
				pConnection->Data
			);
		}
	}
	__xrtSpinLock(&pConnection->TransportLock);
	pOwned = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtSpinUnlock(&pConnection->TransportLock);
	if ( pOwned != NULL ) {
		#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
			if ( pConnection->TransportKind ==
				__XRT_WS_TRANSPORT_TLS ) {
				xrtTlsStreamDestroy(
					(xtlsstream*)pOwned
				);
			} else
		#endif
		{
			xrtNetStreamDestroy(
				(xnetstream*)pOwned
			);
		}
	}
	(void)pTransport;
	xrtWsStreamDestroy(pConnection);
}



/* TCP Close 转入统一 WebSocket 终态。 */
static void __xrtWsStreamTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsStreamTransportClose(
		(xwsstream*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TCP 事件表。 */
static const xnetstreamevents* __xrtWsStreamTcpEvents(void)
{
	static const xnetstreamevents Events = {
		NULL,
		__xrtWsStreamTcpRead,
		__xrtWsStreamTcpEnd,
		__xrtWsStreamTcpHigh,
		__xrtWsStreamTcpLow,
		__xrtWsStreamTcpDrain,
		__xrtWsStreamTcpClose
	};

	return &Events;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* TLS Read 驱动同一消息状态机。 */
static void __xrtWsStreamTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pStream;
	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsStreamDiscardInput(pConnection) ) {
			(void)xrtWsStreamAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsStreamCloseTransport(pConnection);
		return;
	}
	__xrtWsStreamRead(pConnection);
}



/* TLS 明文写空间恢复后先排空 Connection 精确余量。 */
static void __xrtWsStreamTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pStream;
	if ( __xrtWsStreamOutputDrive(pConnection) ) {
		__xrtWsStreamWritableEvent(pConnection);
	}
}



/* TLS 两级发送队列排空后发布 Connection Drain。 */
static void __xrtWsStreamTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	(void)pStream;
	if ( __xrtWsStreamOutputDrive(pConnection) ) {
		__xrtWsStreamDrain(pConnection);
	}
}



/* TLS 对端完成 close_notify 时继续认证关闭。 */
static void __xrtWsStreamTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsstream* pConnection = (xwsstream*)pData;

	if ( !pConnection->TransportClosing ) {
		pConnection->TransportClosing = true;
		if ( !xrtTlsStreamClose(pStream) ) {
			xrtClearError();
			(void)xrtTlsStreamAbort(pStream);
		}
	}
}



/* TLS Close 转入统一 WebSocket 终态。 */
static void __xrtWsStreamTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsStreamTransportClose(
		(xwsstream*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TLS 事件表。 */
static const xtlsstreamevents* __xrtWsStreamTlsEvents(void)
{
	static const xtlsstreamevents Events = {
		.Open = NULL,
		.Read = __xrtWsStreamTlsRead,
		.End = __xrtWsStreamTlsEnd,
		.Writable = __xrtWsStreamTlsWritable,
		.Drain = __xrtWsStreamTlsDrain,
		.Close = __xrtWsStreamTlsClose,
		.Ticket = NULL
	};

	return &Events;
}
#endif



/* 创建共享 Connection 状态，但不提前改变传输事件所有权。 */
static xwsstream* __xrtWsStreamCreate(
	xnetworker* pWorker,
	__xrt_ws_transport TransportKind,
	ptr pTransport,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
)
{
	xwsstreamconfig Config;
	xwsstreamevents Events;
	xwsmessageconfig MessageConfig;
	xwsstream* pConnection;
	size_t iAllocation;

	xrtWsStreamConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
			(void)__xrtWsStreamRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_STREAM_ERROR_ARGUMENT,
				"attach-websocket",
				"WebSocket connection config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	memset(&Events, 0, sizeof(Events));
	if ( pEvents != NULL ) {
		if ( !__xrtRangeValid(pEvents, sizeof(Events)) ) {
			(void)__xrtWsStreamRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_STREAM_ERROR_ARGUMENT,
				"attach-websocket",
				"WebSocket connection event range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	if ( !__xrtWsStreamConfigPrepare(&Config) ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_VALUE,
			XWS_STREAM_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket connection configuration is invalid",
			NULL
		);
		return NULL;
	}
	iAllocation = sizeof(*pConnection) +
		Config.Protocol.Size + 1u;
	pConnection = (xwsstream*)xrtCalloc(1, iAllocation);
	if ( pConnection == NULL ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_MEMORY,
			XWS_STREAM_ERROR_MEMORY,
			"attach-websocket",
			"WebSocket connection allocation failed",
			NULL
		);
		return NULL;
	}
	pConnection->References = 2;
	xrtAtomic32Init(&pConnection->State, XWS_STREAM_OPEN);
	xrtAtomic32Init(
		&pConnection->TransportResult,
		XNET_RESULT_OK
	);
	xrtAtomicPtrInit(&pConnection->Transport, pTransport);
	xrtAtomicPtrInit(&pConnection->Error, NULL);
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	xrtAtomic32Init(&pConnection->ReadPaused, 0);
	xrtAtomic32Init(&pConnection->DrivePosted, 0);
	__xrtSpinInit(&pConnection->TransportLock);
	pConnection->Worker = pWorker;
	pConnection->TransportKind = TransportKind;
	pConnection->Config = Config;
	if ( Config.Protocol.Size != 0 ) {
		char* sProtocol = (char*)(pConnection + 1);

		memcpy(
			sProtocol,
			Config.Protocol.Data,
			Config.Protocol.Size
		);
		sProtocol[Config.Protocol.Size] = '\0';
		pConnection->Protocol.Data = sProtocol;
		pConnection->Protocol.Size =
			Config.Protocol.Size;
		pConnection->Config.Protocol =
			pConnection->Protocol;
	} else {
		memset(
			&pConnection->Config.Protocol,
			0,
			sizeof(pConnection->Config.Protocol)
		);
	}
	pConnection->Events = Events;
	pConnection->Data = pData;
	xrtWsMessageConfigInit(&MessageConfig);
	MessageConfig.MaxSize = Config.MessageLimit;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( Config.DeflateEnabled ) {
			MessageConfig.FirstRsv = XWS_FRAME_RSV1;
		}
	#endif
	if ( !xrtWsMessageInit(
		&pConnection->Message,
		&MessageConfig
	) ) {
		__xrtSpinUnit(&pConnection->TransportLock);
		xrtFree(pConnection);
		return NULL;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( Config.DeflateEnabled ) {
			pConnection->Inflater = xrtWsInflaterCreate(
				&Config.Inflater
			);
			pConnection->Deflater = xrtWsDeflaterCreate(
				&Config.Deflater
			);
			if ( (pConnection->Inflater == NULL) ||
				(pConnection->Deflater == NULL) ) {
				xrtWsInflaterDestroy(
					pConnection->Inflater
				);
				xrtWsDeflaterDestroy(
					pConnection->Deflater
				);
				__xrtSpinUnit(
					&pConnection->TransportLock
				);
				xrtFree(pConnection);
				(void)__xrtWsStreamRemember(
					NULL,
					XERR_MEMORY,
					XWS_STREAM_ERROR_MEMORY,
					"attach-websocket-deflate",
					"WebSocket compression state allocation failed",
					xrtGetError()
				);
				return NULL;
			}
		}
	#endif
	return pConnection;
}



/* 初始化稳定的已建立会话默认值。 */
XRT_API void xrtWsStreamConfigInit(xwsstreamconfig* pConfig)
{
	xwsstreamconfig Config;

	if ( !__xrtRangeValid(pConfig, sizeof(Config)) ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"config-init-websocket",
			"WebSocket connection configuration range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Role = XWS_ROLE_SERVER;
	Config.MessageLimit =
		XWS_STREAM_MESSAGE_LIMIT_DEFAULT;
	Config.FrameLimit =
		XWS_STREAM_FRAME_LIMIT_DEFAULT;
	Config.SendLimit = XWS_STREAM_SEND_LIMIT_DEFAULT;
	Config.ControlReserve =
		XWS_STREAM_CONTROL_RESERVE_DEFAULT;
	Config.CloseTimeout =
		XWS_STREAM_CLOSE_TIMEOUT_DEFAULT;
	Config.AutoPong = true;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		xrtWsInflaterConfigInit(&Config.Inflater);
		xrtWsDeflaterConfigInit(&Config.Deflater);
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* Attach 失败时只销毁 Connection 草稿，不接管或关闭输入传输。 */
static void __xrtWsStreamAttachDiscard(xwsstream* pConnection)
{
	pConnection->References = 1;
	xrtAtomicPtrStore(
		&pConnection->Transport,
		NULL,
		XMEMORY_RELEASE
	);
	xrtWsStreamDestroy(pConnection);
}



/*
	把控制槽换算到实际传输成本，并验证普通数据至少能容纳一个空帧。
	TLS 记录开销在握手完成后固定，Connection 后续可以无查询地精确计账。
*/
static bool __xrtWsStreamAttachBudget(
	xwsstream* pConnection,
	ptr pTransport
)
{
	size_t iControl = __xrtWsStreamControlSlot(
		pConnection->Config.Role
	);
	size_t iDataFrame = 2u + (
		pConnection->Config.Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u
	);
	size_t iDataBudget;
	size_t iMinimum;

	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			size_t iOne;
			size_t iFull;

			if ( !xrtTlsStreamSendBound(
				(xtlsstream*)pTransport,
				1u,
				&iOne
			) || !xrtTlsStreamSendBound(
				(xtlsstream*)pTransport,
				XTLS_RECORD_PLAINTEXT_MAX,
				&iFull
			) || (iOne <= 1u) ||
				(iFull <= XTLS_RECORD_PLAINTEXT_MAX) ||
				((iOne - 1u) !=
				 (iFull - XTLS_RECORD_PLAINTEXT_MAX)) ) {
				return false;
			}
			pConnection->SendOverhead = iOne - 1u;
		}
	#endif
	if ( !__xrtWsStreamTransportSize(
		pConnection,
		iControl,
		&pConnection->ControlSlot
	) || !__xrtWsStreamTransportSize(
		pConnection,
		iDataFrame,
		&iDataBudget
	) || (pConnection->ControlSlot > (SIZE_MAX / 3u)) ) {
		return false;
	}
	iMinimum = pConnection->ControlSlot * 3u;
	if ( (pConnection->Config.ControlReserve < iMinimum) ||
		(iDataBudget > (pConnection->Config.SendLimit -
		 pConnection->Config.ControlReserve)) ) {
		return false;
	}
	if ( pConnection->TransportKind == __XRT_WS_TRANSPORT_TCP ) {
		size_t iWriteLimit = xrtNetStreamWriteLimit(
			(xnetstream*)pTransport
		);

		if ( (pConnection->Config.ControlReserve > iWriteLimit) ||
			(iDataBudget > (iWriteLimit -
			 pConnection->Config.ControlReserve)) ) {
			return false;
		}
	}
	return true;
}



/* 接管开放 TCP Stream，并延迟处理已经缓冲的早到帧。 */
XRT_API xwsstream* xrtWsStreamAttach(
	xnetstream* pStream,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
)
{
	xnetworker* pWorker;
	xwsstream* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"attach-websocket",
			"WebSocket TCP Stream is null",
			NULL
		);
		return NULL;
	}
	pWorker = xrtNetStreamWorker(pStream);
	if ( (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ||
		(iPrefix > xrtNetStreamAvailable(pStream)) ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_STATE,
			XWS_STREAM_ERROR_STATE,
			"attach-websocket",
			"WebSocket TCP Stream or consumed prefix is invalid",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsStreamCreate(
		pWorker,
		__XRT_WS_TRANSPORT_TCP,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamAttachBudget(pConnection, pStream) ) {
		__xrtWsStreamAttachDiscard(pConnection);
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_RANGE,
			XWS_STREAM_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket send budget is incompatible with its TCP Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtNetStreamSetEvents(
		pStream,
		__xrtWsStreamTcpEvents(),
		pConnection
	) ) {
		__xrtWsStreamAttachDiscard(pConnection);
		return NULL;
	}
	if ( (iPrefix != 0) &&
		(xrtNetStreamConsume(pStream, iPrefix) != iPrefix) ) {
		(void)xrtNetStreamSetEvents(pStream, NULL, NULL);
		__xrtWsStreamAttachDiscard(pConnection);
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_STATE,
			XWS_STREAM_ERROR_TRANSPORT,
			"attach-websocket",
			"WebSocket TCP prefix consumption failed",
			xrtGetError()
		);
		return NULL;
	}
	__xrtWsStreamDriveSchedule(pConnection);
	return pConnection;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* 接管开放 TLS Stream，并延迟处理已经解密的早到明文。 */
XRT_API xwsstream* xrtWsStreamAttachTls(
	xtlsstream* pStream,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
)
{
	xnetstream* pTransport;
	xnetworker* pWorker;
	xwsstream* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"attach-websocket-tls",
			"WebSocket TLS Stream is null",
			NULL
		);
		return NULL;
	}
	pTransport = xrtTlsStreamTransport(pStream);
	pWorker = xrtNetStreamWorker(pTransport);
	if ( (pTransport == NULL) || (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(xrtTlsStreamState(pStream) !=
		 XTLS_STREAM_OPEN) ||
		(iPrefix > xrtTlsStreamAvailable(pStream)) ) {
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_STATE,
			XWS_STREAM_ERROR_STATE,
			"attach-websocket-tls",
			"WebSocket TLS Stream or consumed prefix is invalid",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsStreamCreate(
		pWorker,
		__XRT_WS_TRANSPORT_TLS,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamAttachBudget(pConnection, pStream) ) {
		__xrtWsStreamAttachDiscard(pConnection);
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_RANGE,
			XWS_STREAM_ERROR_CONFIG,
			"attach-websocket-tls",
			"WebSocket send budget is incompatible with its TLS Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtTlsStreamSetEvents(
		pStream,
		__xrtWsStreamTlsEvents(),
		pConnection
	) ) {
		__xrtWsStreamAttachDiscard(pConnection);
		return NULL;
	}
	if ( (iPrefix != 0) &&
		!xrtTlsStreamConsume(pStream, iPrefix) ) {
		(void)xrtTlsStreamSetEvents(pStream, NULL, NULL);
		__xrtWsStreamAttachDiscard(pConnection);
		(void)__xrtWsStreamRemember(
			NULL,
			XERR_STATE,
			XWS_STREAM_ERROR_TRANSPORT,
			"attach-websocket-tls",
			"WebSocket TLS prefix consumption failed",
			xrtGetError()
		);
		return NULL;
	}
	__xrtWsStreamDriveSchedule(pConnection);
	return pConnection;
}
#endif



/* 增加 Connection 引用。 */
XRT_API xwsstream* xrtWsStreamRef(xwsstream* pConnection)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"retain-websocket"
	) || (xrtRefRetain(&pConnection->References) < 0) ) {
		return NULL;
	}
	return pConnection;
}



/* 释放 Connection 引用。 */
XRT_API void xrtWsStreamDestroy(xwsstream* pConnection)
{
	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"release-websocket"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pConnection->References) == 0 ) {
		__xrtWsStreamFree(pConnection);
	}
}



/* 返回并发可读状态。 */
XRT_API xwsstreamstate xrtWsStreamState(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_STREAM_CLOSED;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-state"
	) ) {
		return XWS_STREAM_CLOSED;
	}
	return (xwsstreamstate)xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回固定本端角色。 */
XRT_API xwsrole xrtWsStreamRole(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_ROLE_SERVER;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-role"
	) ) {
		return XWS_ROLE_SERVER;
	}
	return pConnection->Config.Role;
}



/* 返回拥有型已协商子协议的借用视图。 */
XRT_API xstrview xrtWsStreamProtocol(
	const xwsstream* pConnection
)
{
	xstrview Empty = { 0 };

	if ( pConnection == NULL ) {
		return Empty;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-protocol"
	) ) {
		return Empty;
	}
	return pConnection->Protocol;
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 复制固定的 permessage-deflate 协商结果。 */
XRT_API bool xrtWsStreamDeflate(
	const xwsstream* pConnection,
	xwsdeflate* pDeflate
)
{
	xwsdeflate Deflate;
	size_t iConnectionSize;

	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-deflate"
	) ) {
		return false;
	}
	if ( !__xrtWsStreamStorageRange(
		pConnection,
		&iConnectionSize
	) || !__xrtRangeValid(pDeflate, sizeof(Deflate)) ||
		__xrtRangesOverlap(
			pDeflate,
			sizeof(Deflate),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"query-websocket-deflate",
			"WebSocket connection or disjoint Deflate output is invalid",
			NULL
		);
		return false;
	}
	if ( !pConnection->Config.DeflateEnabled ) {
		return false;
	}
	Deflate = pConnection->Config.Deflate;
	memcpy(pDeflate, &Deflate, sizeof(Deflate));
	return true;
}
#endif



/* 返回借用 Worker。 */
XRT_API xnetworker* xrtWsStreamWorker(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-worker"
	) ) {
		return NULL;
	}
	return pConnection->Worker;
}



/* TCP 会话返回借用 Stream。 */
XRT_API xnetstream* xrtWsStreamTcp(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TCP) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsStreamReject(
			XERR_STATE,
			XWS_STREAM_ERROR_STATE,
			"query-websocket-tcp",
			"borrowed WebSocket TCP Stream requires its network worker",
			NULL
		);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}



/* 从任意线程取得 TCP Stream 强引用。 */
XRT_API xnetstream* xrtWsStreamTcpRef(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"retain-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TCP) ) {
		return NULL;
	}
	return (xnetstream*)__xrtWsStreamTransportRef(
		pConnection
	);
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* TLS 会话返回借用 Stream。 */
XRT_API xtlsstream* xrtWsStreamTls(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TLS) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsStreamReject(
			XERR_STATE,
			XWS_STREAM_ERROR_STATE,
			"query-websocket-tls",
			"borrowed WebSocket TLS Stream requires its network worker",
			NULL
		);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}



/* 从任意线程取得 TLS Stream 强引用。 */
XRT_API xtlsstream* xrtWsStreamTlsRef(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"retain-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XRT_WS_TRANSPORT_TLS) ) {
		return NULL;
	}
	return (xtlsstream*)__xrtWsStreamTransportRef(
		pConnection
	);
}
#endif



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 单条压缩消息只按实际输出增长，不在连接对象中保留固定缓冲。 */
typedef struct __xrt_ws_compressed {
	bytes Data;
	size_t Size;
	size_t Capacity;
	size_t Limit;
	xnetbuf* Buffer;
} __xrt_ws_compressed;



/* 在当前线路预算内收集 Deflater 的同步输出。 */
static bool __xrtWsStreamDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xrt_ws_compressed* pOutput =
		(__xrt_ws_compressed*)pData;
	size_t iRequired;
	size_t iCapacity;
	bytes pBytes;

	if ( (pOutput->Size > pOutput->Limit) ||
		(Data.Size >
		 (pOutput->Limit - pOutput->Size)) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"compress-websocket-message",
			"compressed WebSocket message exceeds its permanent send capacity",
			NULL
		);
		return false;
	}
	iRequired = pOutput->Size + Data.Size;
	if ( pOutput->Buffer != NULL ) {
		if ( !xrtNetBufAppend(
			pOutput->Buffer,
			Data.Data,
			Data.Size
		) ) {
			(void)__xrtWsStreamReject(
				XERR_MEMORY,
				XWS_STREAM_ERROR_MEMORY,
				"compress-websocket-message",
				"compressed WebSocket buffer allocation failed",
				xrtGetError()
			);
			return false;
		}
		pOutput->Size = iRequired;
		return true;
	}
	if ( iRequired > pOutput->Capacity ) {
		iCapacity = pOutput->Capacity != 0 ?
			pOutput->Capacity : 256u;
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity <=
				(pOutput->Limit / 2u) ?
					(iCapacity * 2u) :
					pOutput->Limit;

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		pBytes = (bytes)xrtRealloc(
			pOutput->Data,
			iCapacity
		);
		if ( pBytes == NULL ) {
			(void)__xrtWsStreamReject(
				XERR_MEMORY,
				XWS_STREAM_ERROR_MEMORY,
				"compress-websocket-message",
				"compressed WebSocket output allocation failed",
				xrtGetError()
			);
			return false;
		}
		pOutput->Data = pBytes;
		pOutput->Capacity = iCapacity;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pOutput->Data + pOutput->Size,
			Data.Data,
			Data.Size
		);
	}
	pOutput->Size = iRequired;
	return true;
}



/* 为服务端明文压缩结果前置帧头并把 Worker 缓冲链直接转交 TCP。 */
static xnetresult __xrtWsStreamDeflateBufferSubmit(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xnetbuf* pBuffer,
	size_t iPayload,
	bool bFirst,
	bool bFinal
)
{
	xwsframe Frame;
	xwsframeconfig Config;
	xnetstream* pStream;
	uint8 pHead[XWS_FRAME_HEAD_MAX];
	size_t iHead = 0;
	size_t iWireSize;
	xnetresult Result;

	Result = __xrtWsStreamFrameBudget(
		pConnection,
		iPayload,
		__XRT_WS_SEND_DATA,
		&iWireSize
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bFirst ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	Frame.PayloadSize = iPayload;
	xrtWsFrameConfigInit(&Config);
	Config.AllowedRsv = XWS_FRAME_RSV1;
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		pHead,
		sizeof(pHead),
		&iHead
	) || !xrtNetBufPrepend(pBuffer, pHead, iHead) ) {
		(void)__xrtWsStreamReject(
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_INTERNAL,
			XWS_STREAM_ERROR_FRAME,
			"compress-websocket-message",
			"compressed WebSocket frame construction failed",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pStream = xrtWsStreamTcp(pConnection);
	if ( pStream == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	Result = xrtNetStreamSendBuffer(pStream, pBuffer);
	if ( Result == XNET_RESULT_OK ) {
		pConnection->DrainPending = true;
		return Result;
	}
	if ( Result == XNET_RESULT_AGAIN ) {
		__xrtWsStreamBackpressure(pConnection);
	} else if ( Result == XNET_RESULT_ERROR ) {
		const xerror* pCause = xrtNetStreamError(pStream);

		if ( pCause == NULL ) {
			pCause = xrtGetError();
		}
		__xrtWsStreamSendFailure(
			pConnection,
			xrtNetStreamState(pStream) != XNET_STREAM_OPEN,
			pCause != NULL ? xrtErrorKind(pCause) : XERR_IO,
			"TCP rejected a compressed WebSocket buffer",
			pCause
		);
	}
	return Result;
}



/* 回滚未发送消息的发送上下文，同时保留原始调用错误。 */
static void __xrtWsStreamDeflateRollback(
	xwsstream* pConnection
)
{
	xerror* pError = xrtTakeError();

	if ( !xrtWsDeflaterAbort(pConnection->Deflater) ) {
		xrtClearError();
	}
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 压缩、封成单帧并仅在传输受理后提交上下文。 */
static xnetresult __xrtWsStreamSendDeflate(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	__xrt_ws_compressed Output;
	xnetbuf Buffer;
	xnetresult Result;
	size_t iMinimum;
	size_t iAvailable = __xrtWsStreamAvailable(
		pConnection,
		__XRT_WS_SEND_DATA
	);
	size_t iCapacity = __xrtWsStreamCapacity(
		pConnection,
		__XRT_WS_SEND_DATA
	);

	if ( iCapacity == 0 ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket connection has no permanent data send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsStreamFrameSize(
		pConnection,
		0,
		&iMinimum
	) || !__xrtWsStreamTransportSize(
		pConnection,
		iMinimum,
		&iMinimum
	) ) {
		(void)__xrtWsStreamReject(
			XERR_INTERNAL,
			XWS_STREAM_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket minimum compressed frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iAvailable < iMinimum ) {
		__xrtWsStreamBackpressure(pConnection);
		return XNET_RESULT_AGAIN;
	}
	memset(&Output, 0, sizeof(Output));
	(void)xrtNetBufInit(&Buffer, NULL);
	if ( (pConnection->Config.Role == XWS_ROLE_SERVER) &&
		(pConnection->TransportKind == __XRT_WS_TRANSPORT_TCP) ) {
		if ( !xrtNetBufInit(
			&Buffer,
			xrtNetWorkerBufPool(pConnection->Worker)
		) ) {
			return XNET_RESULT_ERROR;
		}
		Output.Buffer = &Buffer;
	}
	Output.Limit = iCapacity;
	if ( (uint64)Output.Limit >
		pConnection->Config.FrameLimit ) {
		Output.Limit = (size_t)
			pConnection->Config.FrameLimit;
	}
	if ( !xrtWsDeflaterBegin(
		pConnection->Deflater,
		true
	) || !xrtWsDeflaterWrite(
		pConnection->Deflater,
		Payload,
		__xrtWsStreamDeflateOutput,
		&Output
	) || !xrtWsDeflaterEnd(
		pConnection->Deflater,
		__xrtWsStreamDeflateOutput,
		&Output
	) ) {
		const xerror* pCause = xrtGetError();

		if ( (pCause == NULL) ||
			(xrtErrorDomain(pCause) == NULL) ||
			(strcmp(
				xrtErrorDomain(pCause),
				"xrt.websocket.stream"
			 ) != 0) ) {
			(void)__xrtWsStreamReject(
				pCause != NULL ?
					xrtErrorKind(pCause) :
					XERR_INTERNAL,
				XWS_STREAM_ERROR_MESSAGE,
				"compress-websocket-message",
				"WebSocket message compression failed",
				pCause
			);
		}
		xrtFree(Output.Data);
		xrtNetBufClear(&Buffer);
		__xrtWsStreamDeflateRollback(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( Output.Buffer != NULL ) {
		Result = __xrtWsStreamDeflateBufferSubmit(
			pConnection,
			Opcode,
			&Buffer,
			Output.Size,
			true,
			true
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtNetBufClear(&Buffer);
			__xrtWsStreamDeflateRollback(pConnection);
		}
		return Result;
	}
	Result = __xrtWsStreamSendFrame(
		pConnection,
		Opcode,
		(xbytesview) {
			Output.Data,
			Output.Size
		},
		true,
		__XRT_WS_SEND_DATA,
		true
	);
	xrtFree(Output.Data);
	if ( Result != XNET_RESULT_OK ) {
		__xrtWsStreamDeflateRollback(pConnection);
	}
	return Result;
}
#endif



/* 验证一条完整 Text 或 Binary 消息的不可变参数。 */
bool __xrtWsStreamMessageCheck(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	size_t iWireSize;

	if ( (Opcode != XWS_OPCODE_TEXT) &&
		(Opcode != XWS_OPCODE_BINARY) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-message",
			"WebSocket message opcode must be Text or Binary",
			NULL
		);
		return false;
	}
	if ( !__xrtRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-message",
			"WebSocket message range is invalid",
			NULL
		);
		return false;
	}
	if ( (Payload.Size >
		 pConnection->Config.MessageLimit) ||
		(!bCompressed &&
		 ((uint64)Payload.Size >
		  pConnection->Config.FrameLimit)) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-message",
			"WebSocket message exceeds its configured limit",
			NULL
		);
		return false;
	}
	if ( Opcode == XWS_OPCODE_TEXT ) {
		xstrview Text = {
			(const char*)Payload.Data,
			Payload.Size
		};

		if ( !xrtUtf8Valid(Text, NULL) ) {
			(void)__xrtWsStreamReject(
				XERR_VALUE,
				XWS_STREAM_ERROR_MESSAGE,
				"send-websocket-message",
				"WebSocket text message is not valid UTF-8",
				xrtGetError()
			);
			return false;
		}
	}
	if ( !bCompressed &&
		(!__xrtWsStreamFrameSize(
			pConnection,
			Payload.Size,
			&iWireSize
		 ) || (iWireSize >
			__xrtWsStreamCapacity(
				pConnection,
				__XRT_WS_SEND_DATA
			))) ) {
		(void)__xrtWsStreamReject(
			XERR_RANGE,
			XWS_STREAM_ERROR_LIMIT,
			"send-websocket-message",
			"WebSocket message exceeds its permanent send capacity",
			NULL
		);
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( bCompressed ) {
			if ( !pConnection->Config.DeflateEnabled ) {
				(void)__xrtWsStreamReject(
					XERR_STATE,
					XWS_STREAM_ERROR_CONFIG,
					"compress-websocket-message",
					"WebSocket connection did not negotiate compression",
					NULL
				);
				return false;
			}
		}
	#else
		(void)bCompressed;
	#endif
	return true;
}



/* 验证并发送一条完整 Text 或 Binary 消息。 */
static xnetresult __xrtWsStreamSendMessage(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	if ( !__xrtWsStreamWorker(
		pConnection,
		"send-websocket-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsStreamState(pConnection) != XWS_STREAM_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtWsStreamMessageCheck(
		pConnection,
		Opcode,
		Payload,
		bCompressed
	) ) {
		return XNET_RESULT_ERROR;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		if ( bCompressed ) {
			return __xrtWsStreamSendDeflate(
				pConnection,
				Opcode,
				Payload
			);
		}
	#endif
	return __xrtWsStreamSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XRT_WS_SEND_DATA,
		false
	);
}



/* 发送一条完整 Text 或 Binary 消息。 */
XRT_API xnetresult xrtWsStreamSend(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsStreamSendMessage(
		pConnection,
		Opcode,
		Payload,
		false
	);
}



/* 发送完整 Text。 */
XRT_API xnetresult xrtWsStreamText(
	xwsstream* pConnection,
	xstrview Text
)
{
	return xrtWsStreamSend(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 发送完整 Binary。 */
XRT_API xnetresult xrtWsStreamBinary(
	xwsstream* pConnection,
	xbytesview Data
)
{
	return xrtWsStreamSend(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 压缩并发送完整 Text 或 Binary。 */
XRT_API xnetresult xrtWsStreamSendCompressed(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsStreamSendMessage(
		pConnection,
		Opcode,
		Payload,
		true
	);
}



/* 压缩并发送完整 Text。 */
XRT_API xnetresult xrtWsStreamTextCompressed(
	xwsstream* pConnection,
	xstrview Text
)
{
	return xrtWsStreamSendCompressed(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 压缩并发送完整 Binary。 */
XRT_API xnetresult xrtWsStreamBinaryCompressed(
	xwsstream* pConnection,
	xbytesview Data
)
{
	return xrtWsStreamSendCompressed(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}
#endif



/* 在所属 Worker 上发送一条 Ping 或 Pong 控制帧。 */
static xnetresult __xrtWsStreamControlSend(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	cstr sOperation = Opcode == XWS_OPCODE_PING ?
		"send-websocket-ping" :
		"send-websocket-pong";

	if ( !__xrtWsStreamWorker(
		pConnection,
		sOperation
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsStreamState(pConnection) != XWS_STREAM_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	return __xrtWsStreamSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XRT_WS_SEND_CONTROL,
		false
	);
}



/* 发送 Ping 控制帧。 */
XRT_API xnetresult xrtWsStreamPing(
	xwsstream* pConnection,
	xbytesview Payload
)
{
	return __xrtWsStreamControlSend(
		pConnection,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 发送 Pong 控制帧。 */
XRT_API xnetresult xrtWsStreamPong(
	xwsstream* pConnection,
	xbytesview Payload
)
{
	return __xrtWsStreamControlSend(
		pConnection,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 发送唯一 Close 并等待远端回应。 */
XRT_API xnetresult xrtWsStreamClose(
	xwsstream* pConnection,
	uint16 iCode,
	xstrview Reason
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iSize = 0;

	if ( !__xrtWsStreamWorker(
		pConnection,
		"close-websocket"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pConnection->CloseSent ||
		(xrtWsStreamState(pConnection) != XWS_STREAM_OPEN) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		(void)__xrtWsStreamReject(
			xrtErrorKind(xrtGetError()),
			XWS_STREAM_ERROR_MESSAGE,
			"close-websocket",
			"WebSocket Close code or reason is invalid",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtWsStreamClosePayload(
		pConnection,
		(xbytesview) { Payload, iSize },
		iCode,
		false
	);
}



/* 从任意线程安全取得临时传输引用并请求异常关闭。 */
XRT_API bool xrtWsStreamAbort(xwsstream* pConnection)
{
	ptr pTransport;
	bool bAccepted;
	uint32 iState;

	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"abort-websocket"
	) ) {
		return false;
	}
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	while ( iState == XWS_STREAM_OPEN ) {
		if ( xrtAtomic32CompareExchange(
			&pConnection->State,
			&iState,
			XWS_STREAM_CLOSING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	if ( iState == XWS_STREAM_CLOSED ) {
		return false;
	}
	pTransport = __xrtWsStreamTransportRef(pConnection);
	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
		if ( pConnection->TransportKind ==
			__XRT_WS_TRANSPORT_TLS ) {
			bAccepted = xrtTlsStreamAbort(
				(xtlsstream*)pTransport
			);
		} else
	#endif
	{
		bAccepted = xrtNetStreamAbort(
			(xnetstream*)pTransport
		);
	}
	__xrtWsStreamTransportRelease(
		pConnection,
		pTransport
	);
	return bAccepted;
}



/* 复制 Close 快照。 */
XRT_API bool xrtWsStreamCloseInfo(
	const xwsstream* pConnection,
	xwsstreamclose* pClose
)
{
	size_t iConnectionSize;

	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-close"
	) ) {
		return false;
	}
	if ( !__xrtWsStreamStorageRange(
		pConnection,
		&iConnectionSize
	) || !__xrtRangeValid(pClose, sizeof(*pClose)) ||
		__xrtRangesOverlap(
			pClose,
			sizeof(*pClose),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"query-websocket-close",
			"WebSocket connection or disjoint Close output is invalid",
			NULL
		);
		return false;
	}
	__xrtWsStreamCloseSnapshot(pConnection, pClose);
	return true;
}



/* 返回第一个结构化错误。 */
XRT_API const xerror* xrtWsStreamError(
	const xwsstream* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsStreamCheck(
		pConnection,
		"query-websocket-error"
	) ) {
		return NULL;
	}
	return (const xerror*)xrtAtomicPtrLoad(
		&pConnection->Error,
		XMEMORY_ACQUIRE
	);
}

#endif
