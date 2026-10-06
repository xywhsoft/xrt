#ifndef XRT_INTERNAL_WEBSOCKET_STREAM_H
#define XRT_INTERNAL_WEBSOCKET_STREAM_H

#include "xrt_internal.h"
#include "xrt_net_engine.h"
#include "xrt_websocket.h"

#include <xrt/websocket_stream.h>



#if defined(XRT_FEATURE_WEBSOCKET_STREAM)

/* 发送类别按协议存活优先级逐层使用预算。 */
typedef enum __xrt_ws_send_class {
	__XRT_WS_SEND_DATA = 0,
	__XRT_WS_SEND_CONTROL,
	__XRT_WS_SEND_AUTO_PONG,
	__XRT_WS_SEND_CLOSE
} __xrt_ws_send_class;



/* 验证 Stream 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsStreamRangeValid(const xwsstream* pStream);



/* 复用同步发送的完整消息参数检查，不触碰 Worker 状态。 */
bool __xrtWsStreamMessageCheck(
	xwsstream* pStream,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
);



/* 验证活动操作位于 Stream 所属 Worker。 */
bool __xrtWsStreamWorker(xwsstream* pStream, cstr sOperation);



/* 验证 Stream 及其协议副本占用的完整连续存储。 */
bool __xrtWsStreamStorageRange(
	const xwsstream* pStream,
	size_t* pSize
);



/* 记录普通数据第一次耗尽硬预算的背压边沿。 */
void __xrtWsStreamBackpressure(xwsstream* pStream);



/* 归一化底层发送失败，并按传输状态决定是否保存终态错误。 */
void __xrtWsStreamSendFailure(
	xwsstream* pStream,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
);



/* 检查一帧的协议上限、永久容量和当前发送预算。 */
xnetresult __xrtWsStreamFrameBudget(
	xwsstream* pStream,
	size_t iPayload,
	__xrt_ws_send_class Class,
	size_t* pWireSize
);



/* 复制并提交一帧，供完整消息和控制帧共享。 */
xnetresult __xrtWsStreamSendFrame(
	xwsstream* pStream,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xrt_ws_send_class Class,
	bool bCompressed
);



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
/* 提交一个所有权帧；仅 OK 接管 Ref。 */
xnetresult __xrtWsStreamSendRefFrame(
	xwsstream* pStream,
	xwsopcode Opcode,
	xnetref Ref,
	bool bFinal
);



/* 释放由 Take 系列 API 接管的 XRT 内存。 */
void __xrtWsStreamTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
);
#endif



/* 创建 Stream 域错误，供基础层和可裁剪发送层共享。 */
xerror* __xrtWsStreamErrorCreate(
	xerrkind Kind,
	xwsstreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 设置不占用 Stream 终态错误槽的一次调用错误。 */
const xerror* __xrtWsStreamReject(
	xerrkind Kind,
	xwsstreamerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



typedef enum __xrt_ws_transport {
	__XRT_WS_TRANSPORT_TCP = 1,
	__XRT_WS_TRANSPORT_TLS
} __xrt_ws_transport;



/* TLS 短写后只保存尚未进入 TLS 会话的精确帧余量。 */
typedef struct __xrt_ws_output {
	struct __xrt_ws_output* Next;
	size_t Size;
	size_t Offset;
	size_t Pending;
	uint8 Data[];
} __xrt_ws_output;



/*
	对象不持有固定收发缓冲；帧头和掩码使用栈空间。
	TLS 仅在发生短写时按实际帧余量分配节点。
*/
struct xwsstream {
	volatile int32 References;
	xatomic32 State;
	xatomic32 TransportResult;
	xatomicptr Transport;
	xrt_spinlock TransportLock;
	xnetworker* Worker;
	__xrt_ws_transport TransportKind;
	xwsstreamconfig Config;
	size_t SendOverhead;
	size_t ControlSlot;
	xstrview Protocol;
	xwsstreamevents Events;
	ptr Data;
	xwsmessagestate Message;
	#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
		xwsinflater* Inflater;
		xwsdeflater* Deflater;
	#endif
	xwsframe Frame;
	xwsmessageinfo MessageInfo;
	uint64 FrameRemaining;
	uint64 FrameOffset;
	size_t ControlSize;
	uint8 Control[XWS_CLOSE_PAYLOAD_MAX];
	uint16 LocalCode;
	uint16 RemoteCode;
	uint16 FailureCloseCode;
	uint16 RemoteReasonSize;
	char RemoteReason[XWS_CLOSE_REASON_MAX + 1u];
	xatomicptr Error;
	__xrt_ws_output* OutputHead;
	__xrt_ws_output* OutputTail;
	xatomic64 OutputBytes;
	xatomic32 ReadPaused;
	xatomic32 DrivePosted;
	uint64 CloseTimer;
	__xrt_net_engine_internal DriveCommand;
	bool FrameActive;
	bool MessageOpen;
	bool Reading;
	bool CloseSent;
	bool CloseReceived;
	bool RemoteInitiated;
	bool ProtocolFailed;
	bool ProtocolPeerActivity;
	bool TransportClosing;
	bool Backpressured;
	bool DrainPending;
	bool ErrorEmitted;
	bool CloseEmitted;
};

#endif

#endif
