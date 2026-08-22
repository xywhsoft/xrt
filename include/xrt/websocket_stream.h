#ifndef XRT_WEBSOCKET_STREAM_H
#define XRT_WEBSOCKET_STREAM_H

#include <xrt/websocket.h>
#include <xrt/tcp.h>

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
	#include <xrt/tls_stream.h>
#endif



#if defined(XRT_FEATURE_WEBSOCKET_STREAM) && \
	(!defined(XRT_FEATURE_WEBSOCKET_MESSAGE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XRT WebSocket Stream requires messages, secure random and TCP"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF) && \
	!defined(XRT_FEATURE_WEBSOCKET_STREAM)
	#error "XRT WebSocket reference send requires WebSocket Stream"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS) && \
	(!defined(XRT_FEATURE_WEBSOCKET_STREAM) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT WebSocket TLS Stream requires WebSocket Stream and TLS Stream"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_STREAM) || \
	 !defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATER))
	#error "XRT WebSocket compressed Stream requires Stream, Inflater and Deflater"
#endif



#if defined(XRT_FEATURE_WEBSOCKET_STREAM)

#define XWS_STREAM_MESSAGE_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_STREAM_FRAME_LIMIT_DEFAULT UINT64_C(1048576)
#define XWS_STREAM_SEND_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_STREAM_CONTROL_RESERVE_DEFAULT ((size_t)512u)
#define XWS_STREAM_CLOSE_TIMEOUT_DEFAULT UINT64_C(5000000)



/* WebSocket Stream 只包含开放、关闭握手和传输终态。 */
typedef enum xwsstreamstate {
	XWS_STREAM_OPEN = 0,
	XWS_STREAM_CLOSING,
	XWS_STREAM_CLOSED
} xwsstreamstate;



/* Stream 错误区分协议、资源、发送和底层传输边界。 */
typedef enum xwsstreamerror {
	XWS_STREAM_ERROR_ARGUMENT = 1,
	XWS_STREAM_ERROR_CONFIG,
	XWS_STREAM_ERROR_MEMORY,
	XWS_STREAM_ERROR_STATE,
	XWS_STREAM_ERROR_FRAME,
	XWS_STREAM_ERROR_MESSAGE,
	XWS_STREAM_ERROR_RANDOM,
	XWS_STREAM_ERROR_SEND,
	XWS_STREAM_ERROR_LIMIT,
	XWS_STREAM_ERROR_TRANSPORT,
	XWS_STREAM_ERROR_TIMEOUT
} xwsstreamerror;



/* Close 标志描述本地、远端和 RFC 6455 完整关闭结果。 */
typedef enum xwsstreamcloseflag {
	XWS_STREAM_CLOSE_SENT = UINT32_C(0x00000001),
	XWS_STREAM_CLOSE_RECEIVED = UINT32_C(0x00000002),
	XWS_STREAM_CLOSE_CLEAN = UINT32_C(0x00000004),
	XWS_STREAM_CLOSE_REMOTE = UINT32_C(0x00000008)
} xwsstreamcloseflag;



/*
	消息和帧上限分别约束解码后语义与线路输入。
	发送上限包含 WebSocket 与底层传输待发字节；控制预算保留 Ping、Pong 和 Close。
	Stream 不分配固定接收缓冲，协议数据直接消费 TCP 或 TLS 的现有缓冲链。
*/
typedef struct xwsstreamconfig {
	xwsrole Role;
	xstrview Protocol;
	size_t MessageLimit;
	uint64 FrameLimit;
	size_t SendLimit;
	size_t ControlReserve;
	uint64 CloseTimeout;
	bool AutoPong;
	xwsdeflate Deflate;
	xwsinflaterconfig Inflater;
	xwsdeflaterconfig Deflater;
	bool DeflateEnabled;
} xwsstreamconfig;



/* Reason 借用 Stream 内部不可变副本，至少保持到 Stream 销毁。 */
typedef struct xwsstreamclose {
	uint32 Flags;
	xnetresult Transport;
	uint16 LocalCode;
	uint16 RemoteCode;
	xstrview Reason;
} xwsstreamclose;



typedef struct xwsstream xwsstream;



/*
	数据消息按 Begin、零个或多个 Data、End 流式发布，不拼接完整消息。
	回调视图只在当前同步调用期间有效，全部事件在传输所属 Worker 上串行执行。
*/
typedef struct xwsstreamevents {
	void (*MessageBegin)(
		xwsstream* pStream,
		const xwsmessageinfo* pInfo,
		ptr pData
	);
	void (*MessageData)(
		xwsstream* pStream,
		xbytesview Data,
		ptr pData
	);
	void (*MessageEnd)(xwsstream* pStream, ptr pData);
	void (*Ping)(
		xwsstream* pStream,
		xbytesview Payload,
		ptr pData
	);
	void (*Pong)(
		xwsstream* pStream,
		xbytesview Payload,
		ptr pData
	);
	void (*Backpressure)(
		xwsstream* pStream,
		size_t iPending,
		ptr pData
	);
	void (*Writable)(
		xwsstream* pStream,
		size_t iPending,
		ptr pData
	);
	void (*Drain)(xwsstream* pStream, ptr pData);
	void (*Error)(
		xwsstream* pStream,
		const xerror* pError,
		ptr pData
	);
	void (*Close)(
		xwsstream* pStream,
		const xwsstreamclose* pClose,
		ptr pData
	);
} xwsstreamevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_WEBSOCKET_STREAM)

/* 初始化服务端角色、1 MiB 限额、控制预算和五秒关闭超时。 */
XRT_API void xrtWsStreamConfigInit(xwsstreamconfig* pConfig);



/* 无分配验证完整配置，不修改输入。 */
XRT_API bool xrtWsStreamConfigValid(const xwsstreamconfig* pConfig);



/*
	在 TCP Stream 所属 Worker 上接管已经开放的 Stream。
	iPrefix 用于原子跳过已经校验但尚未消费的 HTTP Upgrade Header，普通接管传零。
	成功后接管调用方引用，并在下一次 Worker 循环处理已有 WebSocket 余量。
*/
XRT_API xwsstream* xrtWsStreamAttach(
	xnetstream* pTransport,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
);



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/*
	在 TLS Stream 所属 Worker 上接管已经开放的 Stream。
	iPrefix 原子消费已解密 HTTP Header，Upgrade 后缀不会丢失。
	TLS 短写按实际余量继续提交。
*/
XRT_API xwsstream* xrtWsStreamAttachTls(
	xtlsstream* pTransport,
	size_t iPrefix,
	const xwsstreamconfig* pConfig,
	const xwsstreamevents* pEvents,
	ptr pData
);
#endif



/* 增加 Stream 引用并返回原指针。 */
XRT_API xwsstream* xrtWsStreamRef(xwsstream* pStream);



/* 释放 Stream 引用；关闭或中止传输必须另行请求。 */
XRT_API void xrtWsStreamDestroy(xwsstream* pStream);



/* 返回并发可读的会话状态。 */
XRT_API xwsstreamstate xrtWsStreamState(const xwsstream* pStream);



/* 返回建立 Stream 时固定的本端角色。 */
XRT_API xwsrole xrtWsStreamRole(const xwsstream* pStream);



/* 返回拥有的已协商子协议快照。 */
XRT_API xstrview xrtWsStreamProtocol(const xwsstream* pStream);



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 复制协商后的 permessage-deflate 响应。 */
XRT_API bool xrtWsStreamDeflate(
	const xwsstream* pStream,
	xwsdeflate* pDeflate
);
#endif



/* 返回传输所属的借用 Worker。 */
XRT_API xnetworker* xrtWsStreamWorker(const xwsstream* pStream);



/* 在所属 Worker 上借用 TCP Stream；WSS 返回空指针。 */
XRT_API xnetstream* xrtWsStreamTcp(const xwsstream* pStream);



/* 从任意线程取得 TCP Stream 强引用；调用方最终 Destroy。 */
XRT_API xnetstream* xrtWsStreamTcpRef(const xwsstream* pStream);



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS)
/* 在所属 Worker 上借用 TLS Stream；WS 返回空指针。 */
XRT_API xtlsstream* xrtWsStreamTls(const xwsstream* pStream);



/* 从任意线程取得 TLS Stream 强引用；调用方最终 Destroy。 */
XRT_API xtlsstream* xrtWsStreamTlsRef(const xwsstream* pStream);
#endif



/* 返回 WebSocket 与底层传输当前待发字节的并发快照。 */
XRT_API size_t xrtWsStreamPending(const xwsstream* pStream);



/* 返回普通数据当前仍可受理的硬预算快照。 */
XRT_API size_t xrtWsStreamWritable(const xwsstream* pStream);



/* 在当前消息分块结束后暂停应用数据事件。 */
XRT_API void xrtWsStreamPause(xwsstream* pStream);



/* 恢复应用数据事件并投递一次 Worker 驱动。 */
XRT_API bool xrtWsStreamResume(xwsstream* pStream);



/* 返回接收侧当前是否被应用暂停。 */
XRT_API bool xrtWsStreamPaused(const xwsstream* pStream);



/* 在所属 Worker 上复制发送一条完整 Text 或 Binary 消息。 */
XRT_API xnetresult xrtWsStreamSend(
	xwsstream* pStream,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsStreamText(xwsstream* pStream, xstrview Text);



/* 发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsStreamBinary(xwsstream* pStream, xbytesview Data);



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_REF)
/* 发送所有权消息；仅返回 OK 时接管 Ref。 */
XRT_API xnetresult xrtWsStreamSendRef(
	xwsstream* pStream,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 发送所有权 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsStreamTextRef(
	xwsstream* pStream,
	const xnetref* pRef
);



/* 发送所有权 Binary 消息。 */
XRT_API xnetresult xrtWsStreamBinaryRef(
	xwsstream* pStream,
	const xnetref* pRef
);



/* 发送并接管一段 xrtMalloc 内存。 */
XRT_API xnetresult xrtWsStreamSendTake(
	xwsstream* pStream,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc UTF-8 Text。 */
XRT_API xnetresult xrtWsStreamTextTake(
	xwsstream* pStream,
	str sText,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc Binary。 */
XRT_API xnetresult xrtWsStreamBinaryTake(
	xwsstream* pStream,
	bytes pData,
	size_t iSize
);
#endif



#if defined(XRT_FEATURE_WEBSOCKET_STREAM_DEFLATE)
/* 压缩并发送一条完整 Text 或 Binary 消息。 */
XRT_API xnetresult xrtWsStreamSendCompressed(
	xwsstream* pStream,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 压缩并发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsStreamTextCompressed(
	xwsstream* pStream,
	xstrview Text
);



/* 压缩并发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsStreamBinaryCompressed(
	xwsstream* pStream,
	xbytesview Data
);
#endif



/* 发送 Ping 控制帧；Payload 最多 125 字节。 */
XRT_API xnetresult xrtWsStreamPing(
	xwsstream* pStream,
	xbytesview Payload
);



/* 发送 Pong 控制帧；Payload 最多 125 字节。 */
XRT_API xnetresult xrtWsStreamPong(
	xwsstream* pStream,
	xbytesview Payload
);



/* 发送唯一 Close 并进入关闭握手。 */
XRT_API xnetresult xrtWsStreamClose(
	xwsstream* pStream,
	uint16 iCode,
	xstrview Reason
);



/* 从任意线程请求立即异常关闭底层传输。 */
XRT_API bool xrtWsStreamAbort(xwsstream* pStream);



/* 一次性复制当前 Close 快照。 */
XRT_API bool xrtWsStreamCloseInfo(
	const xwsstream* pStream,
	xwsstreamclose* pClose
);



/* 返回 Stream 保存的第一个结构化错误。 */
XRT_API const xerror* xrtWsStreamError(const xwsstream* pStream);

#endif



XRT_EXTERN_C_END

#endif
