#include "../internal/xrt_websocket_stream.h"



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)

/* 明文服务端仅为帧头分配一个最小所有权节点。 */
typedef struct __xrt_ws_ref_head {
	xnetworker* Worker;
	uint8 Data[XWS_FRAME_HEAD_MAX];
} __xrt_ws_ref_head;



/* TCP 完成帧头发送后释放独立头部节点。 */
static void __xrtWsStreamRefHeadRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	{
		__xrt_ws_ref_head* pHead = (__xrt_ws_ref_head*)pContext;

		__xrtNetWorkerNodeRecycle(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
	}
}



/* Take 路径沿用 XRT 分配器释放调用方交出的负载。 */
void __xrtWsStreamTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 查询或构建一个未掩码的完整消息帧头，不复制负载。 */
static bool __xrtWsStreamRefFrame(
	xwsopcode Opcode,
	size_t iPayload,
	bool bFinal,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xwsframe Frame;
	xwsframeconfig Config;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	if ( bFinal ) {
		Frame.Flags = XWS_FRAME_FIN;
	}
	Frame.PayloadSize = iPayload;
	xrtWsFrameConfigInit(&Config);
	if ( !xrtWsFrameWrite(
		&Frame,
		&Config,
		pOutput,
		iCapacity,
		pSize
	) ) {
		(void)__xrtWsStreamReject(
			XERR_INTERNAL,
			XWS_STREAM_ERROR_FRAME,
			"write-websocket-ref",
			"WebSocket reference frame header construction failed",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 把未掩码帧头和负载作为一个不可分割的 TCP 引用批次提交。 */
static xnetresult __xrtWsStreamRefSubmit(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xnetref Payload,
	bool bFinal
)
{
	__xrt_ws_ref_head* pHead;
	xnetstream* pStream;
	xnetref Refs[2];
	size_t iHeadSize = 0;
	size_t iWireSize;
	xnetresult Result;

	Result = __xrtWsStreamFrameBudget(
		pConnection,
		Payload.Size,
		__XRT_WS_SEND_DATA,
		&iWireSize
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( !__xrtWsStreamRefFrame(
		Opcode,
		Payload.Size,
		bFinal,
		NULL,
		0,
		&iHeadSize
	) ) {
		return XNET_RESULT_ERROR;
	}
	pHead = (__xrt_ws_ref_head*)__xrtNetWorkerNodeAlloc(
		pConnection->Worker,
		sizeof(*pHead)
	);
	if ( pHead == NULL ) {
		(void)__xrtWsStreamReject(
			XERR_MEMORY,
			XWS_STREAM_ERROR_MEMORY,
			"write-websocket-ref",
			"WebSocket reference frame header allocation failed",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pHead->Worker = pConnection->Worker;
	if ( !__xrtWsStreamRefFrame(
		Opcode,
		Payload.Size,
		bFinal,
		pHead->Data,
		sizeof(pHead->Data),
		&iHeadSize
	) ) {
		__xrtNetWorkerNodeRecycle(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
		return XNET_RESULT_ERROR;
	}
	pStream = xrtWsStreamTcp(pConnection);
	if ( pStream == NULL ) {
		__xrtNetWorkerNodeRecycle(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
		return XNET_RESULT_CLOSED;
	}
	Refs[0] = (xnetref) {
		pHead->Data,
		iHeadSize,
		__xrtWsStreamRefHeadRelease,
		pHead
	};
	Refs[1] = Payload;
	Result = xrtNetStreamSendRefs(
		pStream,
		Refs,
		sizeof(Refs) / sizeof(Refs[0])
	);
	if ( Result != XNET_RESULT_OK ) {
		__xrtNetWorkerNodeRecycle(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
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
				pCause != NULL ?
					xrtErrorKind(pCause) : XERR_IO,
				"TCP rejected a WebSocket reference frame",
				pCause
			);
		}
		return Result;
	}
	pConnection->DrainPending = true;
	return XNET_RESULT_OK;
}



/* 在必要复制和服务端明文零复制路径之间统一提交一个所有权帧。 */
xnetresult __xrtWsStreamSendRefFrame(
	xwsstream* pConnection,
	xwsopcode Opcode,
	xnetref Ref,
	bool bFinal
)
{
	xbytesview Payload = {
		Ref.Data,
		Ref.Size
	};
	xnetresult Result;

	/*
		客户端必须掩码，TLS 必须先复制进记录层；二者在复制受理后
		立即完成来源所有权。只有未掩码明文服务端能够保留用户引用。
	*/
	if ( (pConnection->Config.Role == XWS_ROLE_CLIENT) ||
		(pConnection->TransportKind != __XRT_WS_TRANSPORT_TCP) ) {
		Result = __xrtWsStreamSendFrame(
			pConnection,
			Opcode,
			Payload,
			bFinal,
			__XRT_WS_SEND_DATA,
			false
		);
		if ( Result == XNET_RESULT_OK ) {
			Ref.Release(
				Ref.Context,
				Ref.Data,
				Ref.Size
			);
		}
		return Result;
	}
	return __xrtWsStreamRefSubmit(
		pConnection,
		Opcode,
		Ref,
		bFinal
	);
}



/* 验证所有权契约并选择零复制或必要复制路径。 */
XRT_API xnetresult xrtWsStreamSendRef(
	xwsstream* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	xnetref Ref;
	xbytesview Payload;
	size_t iConnectionSize;
	xnetresult Result;

	if ( !__xrtWsStreamStorageRange(
		pConnection,
		&iConnectionSize
	) || !__xrtRangeValid(pRef, sizeof(Ref)) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket connection or reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( __xrtRangesOverlap(
			pRef,
			sizeof(Ref),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket connection or reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( !__xrtRangeValid(Ref.Data, Ref.Size) ||
		(Ref.Size == 0) || (Ref.Release == NULL) ||
		__xrtRangesOverlap(
			Ref.Data,
			Ref.Size,
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsStreamReject(
			XERR_ARGUMENT,
			XWS_STREAM_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket reference is incomplete, empty, "
			"or overlaps its connection",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsStreamWorker(
		pConnection,
		"send-websocket-ref"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsStreamState(pConnection) != XWS_STREAM_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	if ( !__xrtWsStreamMessageCheck(
		pConnection,
		Opcode,
		Payload,
		false
	) ) {
		return XNET_RESULT_ERROR;
	}
	Result = __xrtWsStreamSendRefFrame(
		pConnection,
		Opcode,
		Ref,
		true
	);
	return Result;
}



/* 发送一条所有权 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsStreamTextRef(
	xwsstream* pConnection,
	const xnetref* pRef
)
{
	return xrtWsStreamSendRef(
		pConnection,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 发送一条所有权 Binary 消息。 */
XRT_API xnetresult xrtWsStreamBinaryRef(
	xwsstream* pConnection,
	const xnetref* pRef
)
{
	return xrtWsStreamSendRef(
		pConnection,
		XWS_OPCODE_BINARY,
		pRef
	);
}



/* 把一段 XRT 内存包装成一次性释放引用。 */
XRT_API xnetresult xrtWsStreamSendTake(
	xwsstream* pConnection,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsStreamTakeRelease,
		NULL
	};

	return xrtWsStreamSendRef(
		pConnection,
		Opcode,
		&Ref
	);
}



/* 发送并接管一段 XRT UTF-8 Text。 */
XRT_API xnetresult xrtWsStreamTextTake(
	xwsstream* pConnection,
	str sText,
	size_t iSize
)
{
	return xrtWsStreamSendTake(
		pConnection,
		XWS_OPCODE_TEXT,
		sText,
		iSize
	);
}



/* 发送并接管一段 XRT Binary。 */
XRT_API xnetresult xrtWsStreamBinaryTake(
	xwsstream* pConnection,
	bytes pData,
	size_t iSize
)
{
	return xrtWsStreamSendTake(
		pConnection,
		XWS_OPCODE_BINARY,
		pData,
		iSize
	);
}

#endif
