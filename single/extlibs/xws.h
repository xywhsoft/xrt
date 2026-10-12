/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xws requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XWS_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XWS_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XWS_SINGLE_HEADER_H
#define XWS_SINGLE_HEADER_H
#define XWS_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xws/include/xws/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XWS_FEATURES_H
#define XWS_FEATURES_H

/* websocket_connection_tls 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION_TLS)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION_TLS
#define XWS_FEATURE_WEBSOCKET_CONNECTION_TLS
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#endif

/* websocket_connection_ref_future 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION_REF_FUTURE)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE
#define XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_REF
#define XWS_MODULE_WEBSOCKET_CONNECTION_REF
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_FUTURE
#define XWS_MODULE_WEBSOCKET_CONNECTION_FUTURE
#endif
#endif

/* websocket_connection_future 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION_FUTURE)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE
#define XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* websocket_writer_ref 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_WRITER_REF)
#ifndef XWS_FEATURE_WEBSOCKET_WRITER_REF
#define XWS_FEATURE_WEBSOCKET_WRITER_REF
#endif
#ifndef XWS_MODULE_WEBSOCKET_WRITER
#define XWS_MODULE_WEBSOCKET_WRITER
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_REF
#define XWS_MODULE_WEBSOCKET_CONNECTION_REF
#endif
#endif

/* websocket_writer_deflate 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_WRITER_DEFLATE)
#ifndef XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE
#define XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE
#endif
#ifndef XWS_MODULE_WEBSOCKET_WRITER
#define XWS_MODULE_WEBSOCKET_WRITER
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#define XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE
#endif
#endif

/* websocket_connection_deflate 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION_DEFLATE)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE
#define XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_WEBSOCKET_INFLATER
#define XRT_MODULE_WEBSOCKET_INFLATER
#endif
#ifndef XRT_MODULE_WEBSOCKET_DEFLATER
#define XRT_MODULE_WEBSOCKET_DEFLATER
#endif
#endif

/* websocket_writer 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_WRITER)
#ifndef XWS_FEATURE_WEBSOCKET_WRITER
#define XWS_FEATURE_WEBSOCKET_WRITER
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#endif

/* websocket_connection_ref 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION_REF)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION_REF
#define XWS_FEATURE_WEBSOCKET_CONNECTION_REF
#endif
#ifndef XWS_MODULE_WEBSOCKET_CONNECTION
#define XWS_MODULE_WEBSOCKET_CONNECTION
#endif
#endif

/* websocket_connection 及其直接依赖。 */
#if defined(XWS_MODULE_ALL) || defined(XWS_MODULE_WEBSOCKET_CONNECTION)
#ifndef XWS_FEATURE_WEBSOCKET_CONNECTION
#define XWS_FEATURE_WEBSOCKET_CONNECTION
#endif
#ifndef XRT_MODULE_WEBSOCKET_MESSAGE
#define XRT_MODULE_WEBSOCKET_MESSAGE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

#endif /* XWS_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xws/include/xws/export.h */
/* ========================================================================== */

#ifndef XWS_EXPORT_H
#define XWS_EXPORT_H


/* 独立动态库只导出本库接口，依赖由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XWS_BUILD_SHARED)
  #define XWS_API __declspec(dllexport)
 #elif defined(XWS_USE_SHARED)
  #define XWS_API __declspec(dllimport)
 #else
  #define XWS_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XWS_BUILD_SHARED)
  #define XWS_API __attribute__((visibility("default")))
 #else
  #define XWS_API
 #endif
#else
 #define XWS_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xws/include/xrt/websocket_runtime.h */
/* ========================================================================== */

#ifndef XWS_WEBSOCKET_RUNTIME_H
#define XWS_WEBSOCKET_RUNTIME_H


#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION) && \
	(!defined(XRT_FEATURE_SPIN) || \
	 !defined(XRT_FEATURE_WEBSOCKET_MESSAGE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "xws connections require spin, messages, secure random and TCP"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF) && \
	!defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
	#error "xws connection references require connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER) && \
	!defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
	#error "xws writers require connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF) && \
	(!defined(XWS_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF))
	#error "xws writer references require writer and connection reference support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "xws TLS connections require connections and TLS streams"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATER))
	#error "xws compressed connections require connection, Inflater and Deflater"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE))
	#error "xws compressed writers require writer and compressed connection support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "xws connection Futures require connection and Future support"
#endif

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE) && \
	(!defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF) || \
	 !defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE))
	#error "xws reference Futures require reference and Future connection support"
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

#define XWS_CONN_MESSAGE_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_FRAME_LIMIT_DEFAULT UINT64_C(1048576)
#define XWS_CONN_SEND_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_CONTROL_RESERVE_DEFAULT ((size_t)512u)
#define XWS_CONN_CLOSE_TIMEOUT_DEFAULT INT64_C(5000)
#define XWS_CONN_ASYNC_BYTES_DEFAULT ((size_t)1048576u)
#define XWS_CONN_ASYNC_COUNT_DEFAULT UINT32_C(1024)
#define XWS_CONN_ASYNC_BATCH_DEFAULT UINT32_C(64)



/* 已建立会话只有开放、关闭握手和传输终态三个阶段。 */
typedef enum xwsconnstate {
	XWS_CONN_OPEN = 0,
	XWS_CONN_CLOSING,
	XWS_CONN_CLOSED
} xwsconnstate;



/* Connection 错误码区分协议、资源、发送和底层传输边界。 */
typedef enum xwsconnerror {
	XWS_CONN_ERROR_ARGUMENT = 1,
	XWS_CONN_ERROR_CONFIG,
	XWS_CONN_ERROR_MEMORY,
	XWS_CONN_ERROR_STATE,
	XWS_CONN_ERROR_FRAME,
	XWS_CONN_ERROR_MESSAGE,
	XWS_CONN_ERROR_RANDOM,
	XWS_CONN_ERROR_SEND,
	XWS_CONN_ERROR_LIMIT,
	XWS_CONN_ERROR_TRANSPORT,
	XWS_CONN_ERROR_TIMEOUT
} xwsconnerror;



/* Close 标志同时描述本地、远端和 RFC 6455 完整握手结果。 */
typedef enum xwsconncloseflag {
	XWS_CONN_CLOSE_SENT = UINT32_C(0x00000001),
	XWS_CONN_CLOSE_RECEIVED = UINT32_C(0x00000002),
	XWS_CONN_CLOSE_CLEAN = UINT32_C(0x00000004),
	XWS_CONN_CLOSE_REMOTE = UINT32_C(0x00000008)
} xwsconncloseflag;



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 条件 Future 不复制消息，只观察统一 Connection 状态。 */
typedef enum xwsconnwait {
	XWS_CONN_WAIT_WRITE = 1,
	XWS_CONN_WAIT_DRAIN,
	XWS_CONN_WAIT_CLOSE
} xwsconnwait;
#endif



/*
	MessageLimit 作用于扩展解码后的逻辑消息，FrameLimit 作用于线路帧。
	SendLimit 是 WebSocket 与底层传输待发字节的硬预算；TLS 会按记录密文
	线路长度计账。ControlReserve 从普通数据预算中保留三个最大控制帧槽：
	手动 Ping/Pong、自动 Pong、Close 逐层使用，低优先级发送不能占用
	更高优先级槽。启用 Future 层时，
	AsyncBytesLimit 和 AsyncCountLimit 独立限制尚未受理的异步操作，
	AsyncBatch 限制一次 Worker 轮转完成的操作数。
*/
typedef struct xwsconnconfig {
	xwsrole Role;
	xstrview Protocol;
	size_t MessageLimit;
	uint64 FrameLimit;
	size_t SendLimit;
	size_t ControlReserve;
	int64 CloseTimeout;
	bool AutoPong;
	size_t AsyncBytesLimit;
	uint32 AsyncCountLimit;
	uint32 AsyncBatch;
	xwsdeflate Deflate;
	xwsinflaterconfig Inflater;
	xwsdeflaterconfig Deflater;
	bool DeflateEnabled;
} xwsconnconfig;



/* Reason 借用 Connection 内部不可变副本，至少保持到 Connection 销毁。 */
typedef struct xwsconnclose {
	uint32 Flags;
	xnetresult Transport;
	uint16 LocalCode;
	uint16 RemoteCode;
	xstrview Reason;
} xwsconnclose;



typedef struct xwsconn xwsconn;

#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* Writer 独占一条尚未结束的出站数据消息。 */
typedef struct xwswriter xwswriter;
#endif



/*
	消息事件按 Begin、零个或多个 Data、End 发布，支持空消息和流式消费。
	MessageBegin 的 Info 以及 Data、Ping、Pong 的视图仅在对应同步回调期间有效。
	Info 描述首个数据帧；COMPRESSED 表示 Connection 已经按协商扩展解码消息。
	全部事件都在传输所属 Worker 上串行执行。
*/
typedef struct xwsconnevents {
	void (*MessageBegin)(
		xwsconn* pConnection,
		const xwsmessageinfo* pInfo,
		ptr pData
	);
	void (*MessageData)(
		xwsconn* pConnection,
		xbytesview Data,
		ptr pData
	);
	void (*MessageEnd)(
		xwsconn* pConnection,
		ptr pData
	);
	void (*Ping)(
		xwsconn* pConnection,
		xbytesview Payload,
		ptr pData
	);
	void (*Pong)(
		xwsconn* pConnection,
		xbytesview Payload,
		ptr pData
	);
	void (*Backpressure)(
		xwsconn* pConnection,
		size_t iPending,
		ptr pData
	);
	void (*Writable)(
		xwsconn* pConnection,
		size_t iPending,
		ptr pData
	);
	void (*Drain)(
		xwsconn* pConnection,
		ptr pData
	);
	void (*Error)(
		xwsconn* pConnection,
		const xerror* pError,
		ptr pData
	);
	void (*Close)(
		xwsconn* pConnection,
		const xwsconnclose* pClose,
		ptr pData
	);
} xwsconnevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

/*
	初始化服务端角色、1 MiB 收发上限、512 字节控制预算和五秒关闭超时。
	pConfig 可以指向完整但未对齐的可写结构存储；失败时不修改该存储。
*/
XWS_API void xrtWsConnConfigInit(xwsconnconfig* pConfig);



/*
	无分配验证完整 Connection 配置；不会修改传入结构。
	pConfig 及其 Protocol 视图都必须是完整有效范围，结构本身可以未对齐。
*/
XWS_API bool xrtWsConnConfigValid(
	const xwsconnconfig* pConfig
);

/* 验证不透明连接的完整地址范围；不修改连接或线程错误。 */
XWS_API bool xrtWsConnValid(const xwsconn* pConnection);



/*
	在 TCP Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 Stream 引用，并在下一次 Worker 循环处理
	已经缓冲的早到帧；失败时 Stream 所有权保持不变。
	TCP WriteLimit 必须同时容纳 ControlReserve 和至少一个空数据帧。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XWS_API xwsconn* xrtWsConnAttach(
	xnetstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/*
	在 TLS Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 TLS Stream 引用，明文余量不会丢失。
	ControlReserve 和 SendLimit 必须容纳协商后 TLS 记录开销。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XWS_API xwsconn* xrtWsConnAttachTls(
	xtlsstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);
#endif



/* 增加 Connection 引用并返回原指针。 */
XWS_API xwsconn* xrtWsConnRef(xwsconn* pConnection);



/* 释放 Connection 引用；关闭或中止传输必须另行请求。 */
XWS_API void xrtWsConnDestroy(xwsconn* pConnection);



/* 返回并发可读的会话状态。 */
XWS_API xwsconnstate xrtWsConnState(const xwsconn* pConnection);



/* 返回建立连接时固定的本端角色。 */
XWS_API xwsrole xrtWsConnRole(const xwsconn* pConnection);



/* 返回 Connection 拥有的已协商子协议快照。 */
XWS_API xstrview xrtWsConnProtocol(
	const xwsconn* pConnection
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	复制协商后的 permessage-deflate 响应。
	返回 false 表示连接未启用压缩或参数无效。
	pDeflate 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出。
*/
XWS_API bool xrtWsConnDeflate(
	const xwsconn* pConnection,
	xwsdeflate* pDeflate
);
#endif



/* 返回传输所属的借用 Worker。 */
XWS_API xnetworker* xrtWsConnWorker(const xwsconn* pConnection);



/* 在所属 Worker 上借用 TCP Stream；TLS 会话返回空指针。 */
XWS_API xnetstream* xrtWsConnTcp(const xwsconn* pConnection);



/* 从任意线程取得 TCP Stream 强引用；调用方最终 Destroy。 */
XWS_API xnetstream* xrtWsConnTcpRef(const xwsconn* pConnection);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 在所属 Worker 上借用 TLS Stream；TCP 会话返回空指针。 */
XWS_API xtlsstream* xrtWsConnTls(const xwsconn* pConnection);



/* 从任意线程取得 TLS Stream 强引用；调用方最终 Destroy。 */
XWS_API xtlsstream* xrtWsConnTlsRef(const xwsconn* pConnection);
#endif



/* 返回 WebSocket 与底层传输当前待发字节的并发快照。 */
XWS_API size_t xrtWsConnPending(const xwsconn* pConnection);



/* 返回普通数据当前仍可受理的硬预算快照。 */
XWS_API size_t xrtWsConnWritable(const xwsconn* pConnection);



/*
	暂停应用消息读取。
	可以从任意线程调用；当前回调分块完成后不再发布后续消息事件。
*/
XWS_API void xrtWsConnPause(xwsconn* pConnection);



/*
	恢复应用消息读取并投递一次所属 Worker 驱动。
	已关闭连接返回 false；关闭握手中的连接允许恢复协议读取。
*/
XWS_API bool xrtWsConnResume(xwsconn* pConnection);



/* 返回接收侧当前是否被应用暂停的并发快照。 */
XWS_API bool xrtWsConnPaused(const xwsconn* pConnection);



/*
	在所属 Worker 上复制发送一条完整 Text 或 Binary 消息。
	Text 在受理前完成 UTF-8 校验；当前预算不足返回 AGAIN，
	单帧永久超过 WebSocket 或 TCP 硬容量返回 ERROR 和 XERR_RANGE。
	Payload 必须是完整且不发生地址回绕的只读范围，返回前已经完成复制。
*/
XWS_API xnetresult xrtWsConnSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 发送一条完整 UTF-8 Text 消息。 */
XWS_API xnetresult xrtWsConnText(
	xwsconn* pConnection,
	xstrview Text
);



/* 发送一条完整 Binary 消息。 */
XWS_API xnetresult xrtWsConnBinary(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/*
	在所属 Worker 上发送一条所有权消息。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但结构和数据必须是完整范围，且都不能覆盖 Connection。
	空负载不接管所有权，应改用普通 Send；失败不会调用 Release。
*/
XWS_API xnetresult xrtWsConnSendRef(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 发送一条所有权 UTF-8 Text 消息。 */
XWS_API xnetresult xrtWsConnTextRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 发送一条所有权 Binary 消息。 */
XWS_API xnetresult xrtWsConnBinaryRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/*
	在所属 Worker 上发送并接管一段 xrtMalloc 内存。
	仅返回 OK 时由 Connection 最终 xrtFree；空负载应改用普通 Send。
*/
XWS_API xnetresult xrtWsConnSendTake(
	xwsconn* pConnection,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc UTF-8 Text。 */
XWS_API xnetresult xrtWsConnTextTake(
	xwsconn* pConnection,
	str sText,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc Binary。 */
XWS_API xnetresult xrtWsConnBinaryTake(
	xwsconn* pConnection,
	bytes pData,
	size_t iSize
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/*
	在所属 Worker 上开始一条未压缩的分片 Text 或 Binary 消息。
	同一 Connection 同时只能存在一个 Writer；失败返回空指针并设置错误。
*/
XWS_API xwswriter* xrtWsConnBegin(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条未压缩的分片 UTF-8 Text 消息。 */
XWS_API xwswriter* xrtWsConnBeginText(
	xwsconn* pConnection
);



/* 开始一条未压缩的分片 Binary 消息。 */
XWS_API xwswriter* xrtWsConnBeginBinary(
	xwsconn* pConnection
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/*
	在所属 Worker 上开始一条压缩分片 Text 或 Binary 消息。
	每次 Write 都独立建立同步边界，因此成功返回后对应线路帧已被传输层受理。
	AGAIN、永久容量和 OOM 不推进 Writer；半条消息后的编码或传输故障会终止会话。
*/
XWS_API xwswriter* xrtWsConnBeginCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条压缩分片 UTF-8 Text 消息。 */
XWS_API xwswriter* xrtWsConnBeginTextCompressed(
	xwsconn* pConnection
);



/* 开始一条压缩分片 Binary 消息。 */
XWS_API xwswriter* xrtWsConnBeginBinaryCompressed(
	xwsconn* pConnection
);
#endif



/*
	复制并提交一个非最终分片。
	仅返回 OK 时推进消息大小、UTF-8 和首帧状态；AGAIN 可原样重试。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XWS_API xnetresult xrtWsWriterWrite(
	xwswriter* pWriter,
	xbytesview Data
);



/*
	复制并提交最终分片；空分片合法。
	成功后释放 Connection 的 Writer 独占权，但对象仍需 Destroy。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XWS_API xnetresult xrtWsWriterFinish(
	xwswriter* pWriter,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
/*
	提交一个非空所有权分片。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XWS_API xnetresult xrtWsWriterWriteRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/*
	提交最终所有权分片；失败时 Ref 所有权保持不变。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XWS_API xnetresult xrtWsWriterFinishRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/* 提交并接管一个非空 xrtMalloc 分片。 */
XWS_API xnetresult xrtWsWriterWriteTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);



/* 提交并接管一个非空 xrtMalloc 最终分片。 */
XWS_API xnetresult xrtWsWriterFinishTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);
#endif



/* 返回 Writer 是否已经成功提交最终分片；空指针返回 false，回绕范围设置参数错误。 */
XWS_API bool xrtWsWriterIsFinished(
	const xwswriter* pWriter
);



/*
	在所属 Worker 上销毁 Writer。
	放弃已经开始但未结束的消息会尝试发送 1011 Close，失败则中止传输。
	空指针无操作；回绕对象范围只设置参数错误。
*/
XWS_API void xrtWsWriterDestroy(
	xwswriter* pWriter
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	压缩并发送一条完整 Text 或 Binary 消息。
	失败不会让未发送的压缩上下文影响下一条消息。
*/
XWS_API xnetresult xrtWsConnSendCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 压缩并发送一条完整 UTF-8 Text 消息。 */
XWS_API xnetresult xrtWsConnTextCompressed(
	xwsconn* pConnection,
	xstrview Text
);



/* 压缩并发送一条完整 Binary 消息。 */
XWS_API xnetresult xrtWsConnBinaryCompressed(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/*
	发送 Ping 控制帧；Payload 最多 125 字节。
	控制帧可以使用普通数据不可占用的预留预算。
*/
XWS_API xnetresult xrtWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Pong 控制帧；通常在 AutoPong 关闭后的 Ping 回调中使用。
	Payload 最多 125 字节，并使用控制帧预留预算。
*/
XWS_API xnetresult xrtWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Close 并进入关闭握手；Code 为零且 Reason 为空时发送空负载。
	重复调用返回 CLOSED，不会发送第二个 Close。
*/
XWS_API xnetresult xrtWsConnClose(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);



/*
	从任意线程请求立即异常关闭底层传输。
	OPEN 只会前进到 CLOSING；已经 CLOSED 时返回 false 且终态保持不变。
*/
XWS_API bool xrtWsConnAbort(xwsconn* pConnection);



/*
	一次性复制当前 Close 快照；Reason 仍借用 Connection 内部存储。
	pClose 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出，成功后除 Reason 视图外不再借用 Connection 状态。
*/
XWS_API bool xrtWsConnCloseInfo(
	const xwsconn* pConnection,
	xwsconnclose* pClose
);



/* 返回 Connection 第一个结构化错误；结果借用到 Connection 销毁。 */
XWS_API const xerror* xrtWsConnError(const xwsconn* pConnection);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 返回尚未由 Connection Worker 终结的异步发送负载字节数。 */
XWS_API size_t xrtWsConnAsyncBytes(const xwsconn* pConnection);



/* 返回异步发送和条件等待的合计操作数。 */
XWS_API uint32 xrtWsConnAsyncCount(const xwsconn* pConnection);



/*
	建立 WRITE、DRAIN 或 CLOSE 条件 Future。
	WRITE 与 DRAIN 是发送 FIFO 屏障，只等待调用前已经排队的发送；
	WRITE 随后要求当前可写，DRAIN 随后要求传输待发归零。
	取消只移除本次等待，不会关闭 Connection。
*/
XWS_API xfuture* xrtWsConnWaitAsync(
	xwsconn* pConnection,
	xwsconnwait Wait
);



/*
	从任意线程复制并按 FIFO 提交一条完整消息。
	Future 在帧被传输层受理时完成；排空必须另行等待 DRAIN。
	取消只在 Worker 受理前有效，不会撤回已经提交的线路帧。
*/
XWS_API xfuture* xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 UTF-8 Text。 */
XWS_API xfuture* xrtWsConnTextAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制并异步提交 Binary。 */
XWS_API xfuture* xrtWsConnBinaryAsync(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
/*
	从任意线程异步提交一条所有权消息。
	成功返回 Future 时立即接管 Ref；终态、取消和关闭都恰好调用一次 Release。
	空负载不接管所有权，应改用普通 Async 发送。
	Ref 可以未对齐，但结构与数据都必须是完整、不回绕且与 Connection 分离的范围；
	任何预检失败都不接管所有权，也不会调用 Release。
*/
XWS_API xfuture* xrtWsConnSendRefAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 UTF-8 Text。 */
XWS_API xfuture* xrtWsConnTextRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 Binary。 */
XWS_API xfuture* xrtWsConnBinaryRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 从任意线程复制、压缩并异步提交一条完整消息。 */
XWS_API xfuture* xrtWsConnSendCompressedAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制、压缩并异步提交 UTF-8 Text。 */
XWS_API xfuture* xrtWsConnTextCompressedAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制、压缩并异步提交 Binary。 */
XWS_API xfuture* xrtWsConnBinaryCompressedAsync(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/* 从任意线程复制并异步提交 Ping。 */
XWS_API xfuture* xrtWsConnPingAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 Pong。 */
XWS_API xfuture* xrtWsConnPongAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程异步提交唯一 Close。 */
XWS_API xfuture* xrtWsConnCloseAsync(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);
#endif

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xws/include/xws.h */
/* ========================================================================== */

#ifndef XWS_H
#define XWS_H


#endif

#endif

#if defined(XWS_IMPLEMENTATION) && !defined(XWS_IMPLEMENTATION_ONCE)
#define XWS_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xws/src/internal/xrt_websocket.h */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)
#ifndef XWS_INTERNAL_WEBSOCKET_H
#define XWS_INTERNAL_WEBSOCKET_H

#include <string.h>




/* 设置扩展库公共参数错误。 */
static inline void __xwsErrorSetInvalidArgument(void)
{
	xrtSetErrorInfo(
		XERR_ARGUMENT,
		"xws",
		0,
		"invalid argument"
	);
}



/* 设置扩展库公共大小溢出错误。 */
static inline void __xwsErrorSetSizeOverflow(void)
{
	xrtSetErrorInfo(
		XERR_RANGE,
		"xws",
		0,
		"size overflow"
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

typedef struct __xrt_ws_async __xrt_ws_async;



/*
	发送类别按协议存活优先级逐层使用预算：数据、手动控制、自动 Pong、Close。
	较低优先级永远不能占用较高优先级的固定帧槽。
*/
typedef enum __xws_send_class {
	__XWS_SEND_DATA = 0,
	__XWS_SEND_CONTROL,
	__XWS_SEND_AUTO_PONG,
	__XWS_SEND_CLOSE
} __xws_send_class;



/* 验证 Connection 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsConnRangeValid(const xwsconn* pConnection);



/* 复用同步发送的完整消息参数检查，不触碰 Worker 状态。 */
bool __xrtWsConnMessageCheck(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
);



/* 验证活动操作位于 Connection 所属 Worker。 */
bool __xrtWsConnWorker(
	xwsconn* pConnection,
	cstr sOperation
);



/* 验证 Connection 及其协商协议副本占用的完整连续存储。 */
bool __xrtWsConnStorageRange(
	const xwsconn* pConnection,
	size_t* pSize
);



/* 记录普通数据第一次耗尽硬预算的背压边沿。 */
void __xrtWsConnBackpressure(xwsconn* pConnection);



/* 归一化底层发送失败，并按传输状态决定是否占用终态错误槽。 */
void __xrtWsConnSendFailure(
	xwsconn* pConnection,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
);



/* 检查一帧的协议上限、永久容量和当前发送预算。 */
xnetresult __xrtWsConnFrameBudget(
	xwsconn* pConnection,
	size_t iPayload,
	__xws_send_class Class,
	size_t* pWireSize
);



/* 复制并提交一帧，供完整消息、控制帧和 Writer 共享。 */
xnetresult __xrtWsConnSendFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xws_send_class Class,
	bool bCompressed
);



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/* 预留最终线路帧后推进并提交一个压缩 Writer 分片。 */
xnetresult __xrtWsConnSendDeflatePart(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Input,
	bool bFirst,
	bool bFinal
);
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)
/* 提交一个所有权帧；仅 OK 接管 Ref。 */
xnetresult __xrtWsConnSendRefFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xnetref Ref,
	bool bFinal
);



/* 释放由 Take 系列 API 接管的 XRT 内存。 */
void __xrtWsConnTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
);
#endif



/* 创建 Connection 域错误，供基础层和可裁剪适配层共享。 */
xerror* __xrtWsConnErrorCreate(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 设置不占用 Connection 终态错误槽的一次调用错误。 */
const xerror* __xrtWsConnReject(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 在所属 Worker 上推进异步发送和条件 Future。 */
void __xrtWsConnFutureNotify(xwsconn* pConnection);
#endif



typedef enum __xws_transport {
	__XWS_TRANSPORT_TCP = 1,
	__XWS_TRANSPORT_TLS
} __xws_transport;



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
/* Writer 只保存一条出站消息的事务状态，由 Connection Worker 串行访问。 */
struct xwswriter {
	xwsconn* Connection;
	xutf8state Utf8;
	size_t Size;
	xwsopcode Opcode;
	bool Started;
	bool Finished;
	bool Calling;
	bool DestroyRequested;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		bool Compressed;
	#endif
};



/* 创建并独占一条普通或压缩数据消息。 */
xwswriter* __xrtWsWriterCreate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	bool bCompressed
);
#endif



/* TLS 短写后只保留尚未进入 TLS 会话的精确帧余量。 */
typedef struct __xws_output {
	struct __xws_output* Next;
	size_t Size;
	size_t Offset;
	size_t Pending;
	uint8 Data[];
} __xws_output;



/*
	客户端与服务端共享同一个已建立会话状态。
	对象不持有固定收发缓冲；帧头和掩码使用栈空间，TLS 只在短写后保留
	按实际帧大小分配的余量节点。
*/
struct xwsconn {
	volatile int32 References;
	xatomic32 State;
	xatomic32 TransportResult;
	xatomicptr Transport;
	/* 同步传输强引用与跨线程 Close 快照。 */
	xspinlock TransportLock;
	xnetworker* Worker;
	__xws_transport TransportKind;
	xwsconnconfig Config;
	size_t SendOverhead;
	size_t ControlSlot;
	xstrview Protocol;
	xwsconnevents Events;
	ptr Data;
	xwsmessagestate Message;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		xwswriter WriterStorage;
		xwswriter* Writer;
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
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
	__xws_output* OutputHead;
	__xws_output* OutputTail;
	xatomic64 OutputBytes;
	xatomic32 ReadPaused;
	xatomic32 DrivePosted;
	uint64 CloseTimer;
	xnetpost DriveCommand;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xspinlock AsyncLock;
		__xrt_ws_async* AsyncSendHead;
		__xrt_ws_async* AsyncSendTail;
		__xrt_ws_async* AsyncWaitHead;
		__xrt_ws_async* AsyncWaitTail;
		xatomic64 AsyncBytes;
		xatomic32 AsyncCount;
		xnetpost AsyncCommand;
		bool AsyncPosted;
		bool AsyncDriving;
		bool AsyncNotified;
	#endif
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
#endif


/* ========================================================================== */
/* source: extlibs/xws/src/websocket/connection.c */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION)

#define __XRT_WS_MASK_CHUNK 1024u



/* 创建 Connection 域错误，并保留完整底层原因链。 */
xerror* __xrtWsConnErrorCreate(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "xrt.websocket.connection";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	return xrtErrorBuild(&Desc);
}



/* 保存第一个不可恢复错误，并同步设置当前线程错误。 */
static const xerror* __xrtWsConnRemember(
	xwsconn* pConnection,
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtWsConnErrorCreate(
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
const xerror* __xrtWsConnReject(
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	return __xrtWsConnRemember(
		NULL,
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 第一次错误发布与第一个保存错误保持一一对应。 */
static void __xrtWsConnEmitError(xwsconn* pConnection)
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
static size_t __xrtWsConnControlSlot(xwsrole Role)
{
	return 2u + XWS_CLOSE_PAYLOAD_MAX +
		(Role == XWS_ROLE_CLIENT ? XWS_MASK_SIZE : 0u);
}



/* 验证基础边界，并把协商参数映射到本地压缩方向。 */
static bool __xrtWsConnConfigPrepare(xwsconnconfig* pConfig)
{
	size_t iControlSlot;
	size_t iControlMinimum;
	size_t iDataMinimum;

	if ( (pConfig == NULL) ||
		((pConfig->Role != XWS_ROLE_CLIENT) &&
		 (pConfig->Role != XWS_ROLE_SERVER)) ||
		!xrtMemRangeValid(
			pConfig->Protocol.Data,
			pConfig->Protocol.Size
		) ||
		(pConfig->Protocol.Size >
		 (SIZE_MAX - sizeof(xwsconn) - 1u)) ||
		(pConfig->MessageLimit == 0) ||
		(pConfig->FrameLimit == 0) ||
		(pConfig->FrameLimit > XWS_FRAME_PAYLOAD_MAX) ||
		(pConfig->FrameLimit > (uint64)SIZE_MAX) ) {
		return false;
	}
	iControlSlot = __xrtWsConnControlSlot(pConfig->Role);
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
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		if ( (pConfig->AsyncBytesLimit == 0) ||
			(pConfig->AsyncCountLimit == 0) ||
			(pConfig->AsyncBatch == 0) ) {
			return false;
		}
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
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
	#else
		if (pConfig->DeflateEnabled) return false;
	#endif
	return true;
}



/* 对外部适配层执行无分配且不修改输入的 Connection 配置预检。 */
XWS_API bool xrtWsConnConfigValid(
	const xwsconnconfig* pConfig
)
{
	xwsconnconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		return false;
	}
	memcpy(&Config, pConfig, sizeof(Config));
	return __xrtWsConnConfigPrepare(&Config);
}



/* 验证 Connection 固定结构位于完整、非回绕的地址区间。 */
bool __xrtWsConnRangeValid(const xwsconn* pConnection)
{
	return xrtMemRangeValid(
		pConnection,
		sizeof(*pConnection)
	);
}

/* 扩展库通过公开入口验证连接，不共享私有结构布局。 */
XWS_API bool xrtWsConnValid(const xwsconn* pConnection)
{
	return __xrtWsConnRangeValid(pConnection);
}



/* 验证公开入口收到的 Connection，并设置稳定的结构化错误。 */
static bool __xrtWsConnCheck(
	const xwsconn* pConnection,
	cstr sOperation
)
{
	if ( __xrtWsConnRangeValid(pConnection) ) {
		return true;
	}
	(void)__xrtWsConnReject(
		XERR_ARGUMENT,
		XWS_CONN_ERROR_ARGUMENT,
		sOperation,
		"WebSocket connection range is invalid",
		NULL
	);
	return false;
}



/* 已关闭对象可从任意线程查询，活动操作必须留在传输 Worker。 */
bool __xrtWsConnWorker(
	xwsconn* pConnection,
	cstr sOperation
)
{
	if ( !__xrtWsConnCheck(pConnection, sOperation) ) {
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			sOperation,
			"WebSocket connection operation requires its network worker",
			NULL
		);
		return false;
	}
	return true;
}



/* 可裁剪地推进 Connection Future 适配层。 */
static void __xrtWsConnNotifyFutures(xwsconn* pConnection)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		__xrtWsConnFutureNotify(pConnection);
	#else
		(void)pConnection;
	#endif
}



/* 短暂增加底层传输引用，避免并发 Abort 和终态回调发生悬空访问。 */
static ptr __xrtWsConnTransportRef(
	const xwsconn* pConnection
)
{
	xwsconn* pMutable = (xwsconn*)pConnection;
	ptr pTransport;

	if ( pMutable == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pMutable->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pMutable->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pMutable->TransportKind ==
				__XWS_TRANSPORT_TLS ) {
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
	xrtSpinUnlock(&pMutable->TransportLock);
	return pTransport;
}



/* 释放由传输快照取得的临时引用。 */
static void __xrtWsConnTransportRelease(
	const xwsconn* pConnection,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	(void)pConnection;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
			return;
		}
	#endif
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 返回当前传输已经受理但尚未排空的字节数。 */
static size_t __xrtWsConnTransportPending(
	const xwsconn* pConnection
)
{
	ptr pTransport = __xrtWsConnTransportRef(pConnection);
	size_t iPending = 0;

	if ( pTransport != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pConnection->TransportKind ==
				__XWS_TRANSPORT_TLS ) {
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
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iPending;
}



/* 释放 TLS 尚未受理的全部精确帧余量。 */
static void __xrtWsConnOutputClear(xwsconn* pConnection)
{
	__xws_output* pOutput = pConnection->OutputHead;

	pConnection->OutputHead = NULL;
	pConnection->OutputTail = NULL;
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	while ( pOutput != NULL ) {
		__xws_output* pNext = pOutput->Next;

		xrtFree(pOutput);
		pOutput = pNext;
	}
}



/* 最后一个 Connection 引用释放所有非传输资源。 */
static void __xrtWsConnFree(xwsconn* pConnection)
{
	xerror* pError;

	if ( pConnection == NULL ) {
		return;
	}
	__xrtWsConnOutputClear(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xrtWsInflaterDestroy(pConnection->Inflater);
		xrtWsDeflaterDestroy(pConnection->Deflater);
	#endif
	pError = (xerror*)xrtAtomicPtrExchange(
		&pConnection->Error,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtErrorFree(pError);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xrtSpinUnit(&pConnection->AsyncLock);
	#endif
	xrtSpinUnit(&pConnection->TransportLock);
	xrtFree(pConnection);
}



/* TCP 零复制发送完成后释放包含帧数据的单一分配。 */
static void __xrtWsConnOutputRelease(
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
XWS_API size_t xrtWsConnPending(const xwsconn* pConnection)
{
	uint64 iOutput;
	size_t iTransport;

	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-pending"
	) ) {
		return 0;
	}
	iOutput = xrtAtomic64Load(
		&pConnection->OutputBytes,
		XMEMORY_ACQUIRE
	);
	iTransport = __xrtWsConnTransportPending(pConnection);
	if ( iOutput > (uint64)(SIZE_MAX - iTransport) ) {
		return SIZE_MAX;
	}
	return iTransport + (size_t)iOutput;
}



/* 把 WebSocket 线路字节换算为当前传输实际占用的发送预算。 */
static bool __xrtWsConnTransportSize(
	const xwsconn* pConnection,
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
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
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
static size_t __xrtWsConnReserve(
	const xwsconn* pConnection,
	__xws_send_class Class
)
{
	switch ( Class ) {
		case __XWS_SEND_DATA:
			return pConnection->Config.ControlReserve;
		case __XWS_SEND_CONTROL:
			return pConnection->ControlSlot * 2u;
		case __XWS_SEND_AUTO_PONG:
			return pConnection->ControlSlot;
		case __XWS_SEND_CLOSE:
		default:
			return 0;
	}
}



/* 在一个总容量中扣除当前类别不能占用的协议预留。 */
static size_t __xrtWsConnClassCapacity(
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
static size_t __xrtWsConnCapacity(
	const xwsconn* pConnection,
	__xws_send_class Class
)
{
	size_t iReserve = __xrtWsConnReserve(pConnection, Class);
	size_t iCapacity = __xrtWsConnClassCapacity(
		pConnection->Config.SendLimit,
		iReserve
	);
	ptr pTransport;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			return iCapacity;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsConnClassCapacity(
			xrtNetStreamWriteLimit((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iCapacity ) {
			iCapacity = iTcp;
		}
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iCapacity;
}



/* 前置声明供公开普通数据可写查询复用同一容量口径。 */
static size_t __xrtWsConnAvailable(
	const xwsconn* pConnection,
	__xws_send_class Class
);



/* 计算控制预留之后普通数据仍可使用的发送预算。 */
XWS_API size_t xrtWsConnWritable(const xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-writable"
	) || (xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return 0;
	}
	return __xrtWsConnAvailable(
		pConnection,
		__XWS_SEND_DATA
	);
}



/* 按发送类别扣除更高优先级协议帧的固定预留。 */
static size_t __xrtWsConnAvailable(
	const xwsconn* pConnection,
	__xws_send_class Class
)
{
	size_t iPending = xrtWsConnPending(pConnection);
	size_t iReserve = __xrtWsConnReserve(pConnection, Class);
	size_t iAvailable;
	ptr pTransport;

	if ( iPending >= pConnection->Config.SendLimit ) {
		return 0;
	}
	iAvailable = __xrtWsConnClassCapacity(
		pConnection->Config.SendLimit - iPending,
		iReserve
	);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			return iAvailable;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		size_t iTcp = __xrtWsConnClassCapacity(
			xrtNetStreamWritable((xnetstream*)pTransport),
			iReserve
		);

		if ( iTcp < iAvailable ) {
			iAvailable = iTcp;
		}
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
	return iAvailable;
}



/* 普通发送第一次遇到硬预算时发布一次背压边沿。 */
void __xrtWsConnBackpressure(xwsconn* pConnection)
{
	if ( pConnection->Backpressured ) {
		return;
	}
	pConnection->Backpressured = true;
	if ( pConnection->Events.Backpressure != NULL ) {
		pConnection->Events.Backpressure(
			pConnection,
			xrtWsConnPending(pConnection),
			pConnection->Data
		);
	}
}



/* 发送预算恢复后发布一次可写边沿。 */
static void __xrtWsConnWritableEvent(xwsconn* pConnection)
{
	if ( !pConnection->Backpressured ||
		(xrtWsConnWritable(pConnection) == 0) ) {
		return;
	}
	pConnection->Backpressured = false;
	if ( pConnection->Events.Writable != NULL ) {
		pConnection->Events.Writable(
			pConnection,
			xrtWsConnPending(pConnection),
			pConnection->Data
		);
	}
}



/* 创建一个头部与负载同分配的完整线路帧。 */
static __xws_output* __xrtWsConnFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	bool bCompressed
)
{
	xwsframe Frame;
	xwsframeconfig Config;
	__xws_output* pOutput;
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
			(void)__xrtWsConnReject(
				XERR_IO,
				XWS_CONN_ERROR_RANDOM,
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
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_FRAME,
			"write-websocket-frame",
			"WebSocket frame size is not representable",
			xrtGetError()
		);
		return NULL;
	}
	iTotal = iHead + Payload.Size;
	if ( iTotal > (SIZE_MAX - sizeof(*pOutput)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-websocket-frame",
			"WebSocket frame allocation size overflowed",
			NULL
		);
		return NULL;
	}
	pOutput = (struct __xws_output*)xrtMalloc(
		sizeof(*pOutput) + iTotal
	);
	if ( pOutput == NULL ) {
		(void)__xrtWsConnReject(
			XERR_MEMORY,
			XWS_CONN_ERROR_MEMORY,
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
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_FRAME,
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
static bool __xrtWsConnFrameSize(
	const xwsconn* pConnection,
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
xnetresult __xrtWsConnFrameBudget(
	xwsconn* pConnection,
	size_t iPayload,
	__xws_send_class Class,
	size_t* pWireSize
)
{
	size_t iWireSize;
	size_t iBudget;
	bool bControl = Class != __XWS_SEND_DATA;

	if ( (pConnection == NULL) || (pWireSize == NULL) ||
		(Class < __XWS_SEND_DATA) ||
		(Class > __XWS_SEND_CLOSE) ) {
		__xwsErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	if ( (bControl && (iPayload > XWS_CLOSE_PAYLOAD_MAX)) ||
		(!bControl &&
		 ((uint64)iPayload > pConnection->Config.FrameLimit)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			bControl ?
				"WebSocket control payload exceeds 125 bytes" :
				"WebSocket data payload exceeds its frame limit",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnFrameSize(
		pConnection,
		iPayload,
		&iWireSize
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnTransportSize(
		pConnection,
		iWireSize,
		&iBudget
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnCapacity(
		pConnection,
		Class
	) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-frame",
			"WebSocket frame exceeds its permanent send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnAvailable(
		pConnection,
		Class
	) ) {
		if ( Class == __XWS_SEND_DATA ) {
			__xrtWsConnBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	*pWireSize = iWireSize;
	return XNET_RESULT_OK;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 把 TLS 尚未受理的帧尾追加到精确余量队列。 */
static void __xrtWsConnOutputAppend(
	xwsconn* pConnection,
	__xws_output* pOutput
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
void __xrtWsConnSendFailure(
	xwsconn* pConnection,
	bool bFatal,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( bFatal ) {
		(void)__xrtWsConnRemember(
			pConnection,
			Kind,
			XWS_CONN_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	} else {
		(void)__xrtWsConnReject(
			Kind,
			XWS_CONN_ERROR_SEND,
			"send-websocket-frame",
			sMessage,
			pCause
		);
	}
}



/* 向 TCP 或 TLS 提交一整个已计入预算的帧。 */
static xnetresult __xrtWsConnSubmit(
	xwsconn* pConnection,
	__xws_output* pOutput,
	__xws_send_class Class
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
	if ( !__xrtWsConnTransportSize(
		pConnection,
		pOutput->Size,
		&iBudget
	) ) {
		xrtFree(pOutput);
		__xrtWsConnSendFailure(
			pConnection,
			false,
			XERR_RANGE,
			"WebSocket transport size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iBudget > __xrtWsConnAvailable(
		pConnection,
		Class
	) ) {
		xrtFree(pOutput);
		if ( Class == __XWS_SEND_DATA ) {
			__xrtWsConnBackpressure(pConnection);
		}
		return XNET_RESULT_AGAIN;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			size_t iWritten = 0;
			xtlsresult TlsResult;

			if ( pConnection->OutputHead != NULL ) {
				pOutput->Pending = iBudget;
				__xrtWsConnOutputAppend(
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
				__xrtWsConnSendFailure(
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
				if ( !__xrtWsConnTransportSize(
					pConnection,
					pOutput->Size - pOutput->Offset,
					&pOutput->Pending
				) ) {
					xrtFree(pOutput);
					__xrtWsConnSendFailure(
						pConnection,
						true,
						XERR_INTERNAL,
						"TLS short-write budget became invalid",
						NULL
					);
					return XNET_RESULT_ERROR;
				}
				__xrtWsConnOutputAppend(
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
		__xrtWsConnOutputRelease,
		pOutput
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtFree(pOutput);
		if ( (Result == XNET_RESULT_AGAIN) &&
			(Class == __XWS_SEND_DATA) ) {
			__xrtWsConnBackpressure(pConnection);
		} else if ( Result == XNET_RESULT_ERROR ) {
			const xerror* pCause = xrtNetStreamError(
				(xnetstream*)pTransport
			);

			if ( pCause == NULL ) {
				pCause = xrtGetError();
			}
			__xrtWsConnSendFailure(
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
xnetresult __xrtWsConnSendFrame(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bFinal,
	__xws_send_class Class,
	bool bCompressed
)
{
	__xws_output* pOutput;
	size_t iWireSize;
	xnetresult Budget;

	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-frame",
			"WebSocket payload range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	Budget = __xrtWsConnFrameBudget(
		pConnection,
		Payload.Size,
		Class,
		&iWireSize
	);
	if ( Budget != XNET_RESULT_OK ) {
		return Budget;
	}
	pOutput = __xrtWsConnFrame(
		pConnection,
		Opcode,
		Payload,
		bFinal,
		bCompressed
	);
	if ( pOutput == NULL ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtWsConnSubmit(
		pConnection,
		pOutput,
		Class
	);
}



/* 取消关闭计时器；终态回调仍负责释放其 Connection 引用。 */
static void __xrtWsConnCancelCloseTimer(xwsconn* pConnection)
{
	xnetengine* pEngine;

	if ( pConnection->CloseTimer == 0 ) {
		return;
	}
	pEngine = xrtNetWorkerEngine(pConnection->Worker);
	if ( !xrtNetEngineTimerCancelCurrent(
		pEngine,
		pConnection->CloseTimer
	) && !xrtNetEngineTimerCancel(
		pEngine,
		pConnection->CloseTimer
	) ) {
		xrtClearError();
	}
}



/* 排空条件满足后开始唯一的底层传输关闭。 */
static void __xrtWsConnCloseTransportStart(xwsconn* pConnection)
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
	__xrtWsConnCancelCloseTimer(pConnection);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
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
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 远端 Close 或协议失败后的新输入到达后，排空输出并关闭底层传输。 */
static void __xrtWsConnCloseTransport(xwsconn* pConnection)
{
	if ( (!pConnection->CloseReceived &&
		 !(pConnection->ProtocolFailed &&
		   pConnection->ProtocolPeerActivity)) ||
		(pConnection->OutputHead != NULL) ) {
		return;
	}
	__xrtWsConnCloseTransportStart(pConnection);
}



/* 关闭计时器只在远端没有回应时把握手变为明确超时。 */
static void __xrtWsConnCloseTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pWorker;
	if ( pConnection->CloseTimer == Id ) {
		pConnection->CloseTimer = 0;
		if ( (Result == XNET_RESULT_OK) &&
			!pConnection->CloseReceived &&
			(xrtWsConnState(pConnection) !=
			 XWS_CONN_CLOSED) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_TIMEOUT,
				XWS_CONN_ERROR_TIMEOUT,
				"close-websocket",
				"WebSocket close handshake timed out",
				NULL
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
		}
	}
	xrtWsConnDestroy(pConnection);
}



/* 首个本地 Close 建立独立握手超时。 */
static bool __xrtWsConnStartCloseTimer(xwsconn* pConnection)
{
	if ( (pConnection->Config.CloseTimeout == 0) ||
		(pConnection->CloseTimer != 0) ||
		pConnection->CloseReceived ) {
		return true;
	}
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return false;
	}
	pConnection->CloseTimer = xrtNetEngineAfter(
		xrtNetWorkerEngine(pConnection->Worker),
		xrtNetWorkerIndex(pConnection->Worker),
		pConnection->Config.CloseTimeout,
		__xrtWsConnCloseTimer,
		pConnection
	);
	if ( pConnection->CloseTimer == 0 ) {
		(void)__xrtWsConnRemember(
			pConnection,
			XERR_AGAIN,
			XWS_CONN_ERROR_TIMEOUT,
			"close-websocket",
			"WebSocket close timer could not be scheduled",
			xrtGetError()
		);
		xrtWsConnDestroy(pConnection);
		return false;
	}
	return true;
}



/* 在同步终态重入期间保护会话，并提交唯一的本地 Close。 */
static xnetresult __xrtWsConnClosePayload(
	xwsconn* pConnection,
	xbytesview Payload,
	uint16 iCode,
	bool bRemote
)
{
	uint32 iState;
	xnetresult Result;

	if ( xrtWsConnRef(pConnection) == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	xrtSpinLock(&pConnection->TransportLock);
	if ( pConnection->CloseSent ||
		((iState = xrtAtomic32Load(
			&pConnection->State,
			XMEMORY_ACQUIRE
		 )) != XWS_CONN_OPEN) ) {
		xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsConnDestroy(pConnection);
		return XNET_RESULT_CLOSED;
	}
	/*
		先发布唯一 Close 意图。底层发送可同步触发终态回调，
		终态快照必须在该重入窗口内看到完整的本地关闭信息。
	*/
	pConnection->CloseSent = true;
	pConnection->LocalCode = iCode;
	pConnection->RemoteInitiated = bRemote;
	xrtSpinUnlock(&pConnection->TransportLock);
	Result = __xrtWsConnSendFrame(
		pConnection,
		XWS_OPCODE_CLOSE,
		Payload,
		true,
		__XWS_SEND_CLOSE,
		false
	);
	if ( Result != XNET_RESULT_OK ) {
		/* 未受理的 Close 不占用唯一发送槽，调用方可以重试。 */
		xrtSpinLock(&pConnection->TransportLock);
		if ( xrtWsConnState(pConnection) != XWS_CONN_CLOSED ) {
			pConnection->CloseSent = false;
			pConnection->LocalCode = 0;
			pConnection->RemoteInitiated = false;
		}
		xrtSpinUnlock(&pConnection->TransportLock);
		xrtWsConnDestroy(pConnection);
		return Result;
	}
	/* 同步终态可能已经写入 CLOSED，只允许从 OPEN 单向推进。 */
	iState = XWS_CONN_OPEN;
	(void)xrtAtomic32CompareExchange(
		&pConnection->State,
		&iState,
		XWS_CONN_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	);
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	if ( (iState != XWS_CONN_CLOSED) &&
		xrtWsConnPaused(pConnection) ) {
		(void)xrtWsConnResume(pConnection);
	}
	if ( (iState != XWS_CONN_CLOSED) &&
		!__xrtWsConnStartCloseTimer(pConnection) ) {
		__xrtWsConnEmitError(pConnection);
		(void)xrtWsConnAbort(pConnection);
		xrtWsConnDestroy(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( iState != XWS_CONN_CLOSED ) {
		__xrtWsConnCloseTransport(pConnection);
	}
	xrtWsConnDestroy(pConnection);
	return XNET_RESULT_OK;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS 可写边沿继续提交之前发生短写的精确帧余量。 */
static bool __xrtWsConnOutputDrive(xwsconn* pConnection)
{
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	while ( (pStream != NULL) &&
		(pConnection->OutputHead != NULL) ) {
		__xws_output* pOutput =
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
			if ( !__xrtWsConnTransportSize(
				pConnection,
				pOutput->Size - pOutput->Offset,
				&pOutput->Pending
			) || (pOutput->Pending > iPending) ) {
				(void)__xrtWsConnRemember(
					pConnection,
					XERR_INTERNAL,
					XWS_CONN_ERROR_SEND,
					"drain-websocket-output",
					"TLS short-write accounting became invalid",
					NULL
				);
				__xrtWsConnEmitError(pConnection);
				(void)xrtWsConnAbort(pConnection);
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
			(void)__xrtWsConnRemember(
				pConnection,
				Result == XTLS_CLOSED ?
					XERR_CLOSED : XERR_IO,
				XWS_CONN_ERROR_SEND,
				"drain-websocket-output",
				"TLS could not continue a WebSocket frame",
				xrtTlsStreamError(pStream)
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
			return false;
		}
		if ( (Result == XTLS_AGAIN) ||
			(iWritten == 0) ) {
			break;
		}
	}
	if ( pConnection->OutputHead == NULL ) {
		__xrtWsConnWritableEvent(pConnection);
		__xrtWsConnCloseTransport(pConnection);
	}
	return true;
}
#endif



/* 返回当前 TCP 或 TLS 明文接收链。 */
static const xnetbuf* __xrtWsConnBuffer(xwsconn* pConnection)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			return xrtTlsStreamBuffer(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return xrtNetStreamBuffer((xnetstream*)pTransport);
}



/* 精确消费 TCP 或 TLS 明文，并保持各自恢复读取的内部契约。 */
static bool __xrtWsConnConsume(
	xwsconn* pConnection,
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
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
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



/* 丢弃协议失败时已经进入明文缓冲的不可恢复输入。 */
static bool __xrtWsConnDiscardInput(xwsconn* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsConnBuffer(pConnection);
	size_t iSize = pBuffer != NULL ?
		xrtNetBufSize(pBuffer) : 0;

	return (iSize == 0) ||
		__xrtWsConnConsume(pConnection, iSize);
}



/* 校验一段扩展解码后的语义负载并发布消息数据。 */
static bool __xrtWsConnPayloadSemantic(
	xwsconn* pConnection,
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
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
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
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_CONN_ERROR_MESSAGE,
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



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 把 Inflater 的同步输出接入消息校验与应用事件。 */
static bool __xrtWsConnInflateOutput(
	xbytesview Data,
	ptr pData
)
{
	return __xrtWsConnPayloadSemantic(
		(xwsconn*)pData,
		Data
	);
}



/* 把压缩层错误固定映射为消息过大或非法扩展数据。 */
static bool __xrtWsConnInflateFailure(
	xwsconn* pConnection,
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
	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ?
			xrtErrorKind(pCause) : XERR_PROTOCOL,
		XWS_CONN_ERROR_MESSAGE,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}
#endif



/* 解码一段线路负载；未协商压缩时保持零复制直通。 */
static bool __xrtWsConnPayloadWire(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_CONTROL) == 0) ) {
			if ( !xrtWsInflaterWrite(
				pConnection->Inflater,
				Payload,
				__xrtWsConnInflateOutput,
				pConnection
			) ) {
				return __xrtWsConnInflateFailure(
					pConnection,
					"inflate-websocket-message",
					"WebSocket compressed payload is invalid"
				);
			}
			return true;
		}
	#endif
	return __xrtWsConnPayloadSemantic(
		pConnection,
		Payload
	);
}



/* 把本地控制帧提交失败映射为 1011，而不是归咎于对端协议。 */
static bool __xrtWsConnControlFailure(
	xwsconn* pConnection,
	xnetresult Result,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	pConnection->FailureCloseCode = XWS_CLOSE_INTERNAL;
	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ? xrtErrorKind(pCause) :
			(Result == XNET_RESULT_AGAIN ? XERR_AGAIN : XERR_IO),
		XWS_CONN_ERROR_SEND,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 处理已经完整校验的 Ping、Pong 或 Close 控制帧。 */
static bool __xrtWsConnControl(xwsconn* pConnection)
{
	xbytesview Payload = {
		pConnection->Control,
		pConnection->ControlSize
	};

	if ( pConnection->Frame.Opcode ==
		(uint8)XWS_OPCODE_PING ) {
		if ( pConnection->Config.AutoPong &&
			(xrtWsConnState(pConnection) == XWS_CONN_OPEN) ) {
			xnetresult Result = __xrtWsConnSendFrame(
				pConnection,
				XWS_OPCODE_PONG,
				Payload,
				true,
				__XWS_SEND_AUTO_PONG,
				false
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsConnControlFailure(
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
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_PROTOCOL,
				XWS_CONN_ERROR_MESSAGE,
				"read-websocket-close",
				"WebSocket Close payload is invalid",
				xrtGetError()
			);
			return false;
		}
		xrtSpinLock(&pConnection->TransportLock);
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
		xrtSpinUnlock(&pConnection->TransportLock);
		if ( !pConnection->CloseSent ) {
			xnetresult Result = __xrtWsConnClosePayload(
				pConnection,
				Payload,
				Close.Code,
				true
			);

			if ( Result != XNET_RESULT_OK ) {
				return __xrtWsConnControlFailure(
					pConnection,
					Result,
					"reply-websocket-close",
					"WebSocket Close reply could not be submitted"
				);
			}
		} else {
			__xrtWsConnCloseTransport(pConnection);
		}
		return true;
	}
	return false;
}



/* 完成当前帧，并在协议校验成功后发布消息 End 或控制事件。 */
static bool __xrtWsConnFrameEnd(xwsconn* pConnection)
{
	xwsmessageerrorinfo Error;
	bool bControl =
		(pConnection->MessageInfo.Flags &
		 XWS_MESSAGE_CONTROL) != 0;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pConnection->Config.DeflateEnabled &&
			!bControl &&
			((pConnection->MessageInfo.Flags &
			  XWS_MESSAGE_END) != 0) &&
			!xrtWsInflaterEnd(
				pConnection->Inflater,
				__xrtWsConnInflateOutput,
				pConnection
			) ) {
			return __xrtWsConnInflateFailure(
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
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"finish-websocket-frame",
			"WebSocket message frame is incomplete or invalid",
			xrtGetError()
		);
		return false;
	}
	if ( bControl ) {
		if ( !__xrtWsConnControl(pConnection) ) {
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
static int __xrtWsConnFrameBegin(xwsconn* pConnection)
{
	const xnetbuf* pBuffer = __xrtWsConnBuffer(pConnection);
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
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
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
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_FRAME,
			"read-websocket-frame",
			"WebSocket frame header is invalid",
			xrtGetError()
		);
		return -1;
	}
	if ( !__xrtWsConnConsume(
		pConnection,
		pConnection->Frame.HeadSize
	) ) {
		(void)__xrtWsConnRemember(
			pConnection,
			XERR_INTERNAL,
			XWS_CONN_ERROR_TRANSPORT,
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
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
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
			#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
				if ( pConnection->Config.DeflateEnabled &&
					!xrtWsInflaterBegin(
						pConnection->Inflater,
						(pConnection->MessageInfo.Flags &
						 XWS_MESSAGE_EXTENDED) != 0
					) ) {
					(void)__xrtWsConnInflateFailure(
						pConnection,
						"begin-websocket-inflate",
						"WebSocket compressed message could not begin"
					);
					return -1;
				}
			#endif
			pConnection->MessageOpen = true;
			if ( pConnection->Events.MessageBegin != NULL ) {
				#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
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
static void __xrtWsConnProtocolFail(
	xwsconn* pConnection,
	uint16 iCloseCode
)
{
	uint8 Payload[2];
	xbytesview View;

	__xrtWsConnEmitError(pConnection);
	if ( pConnection->CloseSent ||
		(xrtWsConnState(pConnection) == XWS_CONN_CLOSED) ) {
		(void)xrtWsConnAbort(pConnection);
		return;
	}
	pConnection->ProtocolFailed = true;
	Payload[0] = (uint8)(iCloseCode >> 8u);
	Payload[1] = (uint8)iCloseCode;
	View.Data = Payload;
	View.Size = sizeof(Payload);
	if ( __xrtWsConnClosePayload(
		pConnection,
		View,
		iCloseCode,
		false
	) != XNET_RESULT_OK ) {
		(void)xrtWsConnAbort(pConnection);
	} else if ( !__xrtWsConnDiscardInput(pConnection) ) {
		(void)xrtWsConnAbort(pConnection);
	}
}



/* 增量消费任意网络分块，不为消息或帧建立固定连接缓冲。 */
static void __xrtWsConnRead(xwsconn* pConnection)
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
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return;
	}
	pConnection->Reading = true;
	while ( (xrtWsConnState(pConnection) !=
		 XWS_CONN_CLOSED) &&
		!pConnection->CloseReceived &&
		!xrtAtomic32Load(
			&pConnection->ReadPaused,
			XMEMORY_ACQUIRE
		) ) {
		const xnetbuf* pBuffer;
		xnetspan Span;
		size_t iChunk;

		if ( !pConnection->FrameActive ) {
			int iBegin = __xrtWsConnFrameBegin(pConnection);

			if ( iBegin == 0 ) {
				break;
			}
			if ( iBegin < 0 ) {
				if ( iBegin == -2 ) {
					__xrtWsConnEmitError(pConnection);
					(void)xrtWsConnAbort(pConnection);
				} else {
					__xrtWsConnProtocolFail(
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
			if ( !__xrtWsConnFrameEnd(pConnection) ) {
				__xrtWsConnProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
			continue;
		}
		pBuffer = __xrtWsConnBuffer(pConnection);
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
			) || !__xrtWsConnPayloadWire(
				pConnection,
				(xbytesview) { Scratch, iChunk }
			) ) {
				__xrtWsConnProtocolFail(
					pConnection,
					pConnection->FailureCloseCode != 0 ?
						pConnection->FailureCloseCode :
						XWS_CLOSE_PROTOCOL
				);
				break;
			}
		} else if ( !__xrtWsConnPayloadWire(
			pConnection,
			(xbytesview) { Span.Data, iChunk }
		) ) {
			__xrtWsConnProtocolFail(
				pConnection,
				pConnection->FailureCloseCode != 0 ?
					pConnection->FailureCloseCode :
					XWS_CLOSE_PROTOCOL
			);
			break;
		}
		if ( !__xrtWsConnConsume(pConnection, iChunk) ) {
			(void)__xrtWsConnRemember(
				pConnection,
				XERR_INTERNAL,
				XWS_CONN_ERROR_TRANSPORT,
				"consume-websocket-input",
				"WebSocket transport did not consume its payload",
				xrtGetError()
			);
			__xrtWsConnEmitError(pConnection);
			(void)xrtWsConnAbort(pConnection);
			break;
		}
		pConnection->FrameRemaining -= iChunk;
		pConnection->FrameOffset += iChunk;
	}
	/*
		Close 是接收方向的协议终点。当前缓冲中的后续字节不再属于
		应用消息，必须在关闭传输前统一丢弃。
	*/
	if ( pConnection->CloseReceived &&
		(xrtWsConnState(pConnection) != XWS_CONN_CLOSED) &&
		!__xrtWsConnDiscardInput(pConnection) ) {
		(void)__xrtWsConnRemember(
			pConnection,
			XERR_INTERNAL,
			XWS_CONN_ERROR_TRANSPORT,
			"discard-websocket-input",
			"WebSocket transport did not discard data after Close",
			xrtGetError()
		);
		__xrtWsConnEmitError(pConnection);
		(void)xrtWsConnAbort(pConnection);
	}
	pConnection->Reading = false;
	xrtWsConnDestroy(pConnection);
}



/* 在所属 Worker 上处理早到数据，并按应用暂停状态恢复 TCP 读取。 */
static void __xrtWsConnDrive(
	xnetworker* pWorker,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

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
		__xrtWsConnRead(pConnection);
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			xrtWsConnDestroy(pConnection);
			return;
		}
	#endif
	if ( (xrtWsConnState(pConnection) !=
		 XWS_CONN_CLOSED) &&
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
	xrtWsConnDestroy(pConnection);
}



/* 合并任意线程的恢复请求，并为嵌入命令持有一份 Connection 引用。 */
static void __xrtWsConnDriveSchedule(xwsconn* pConnection)
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
	if ( xrtWsConnRef(pConnection) == NULL ) {
		xrtAtomic32Store(
			&pConnection->DrivePosted,
			0,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->DriveCommand,
		__xrtWsConnDrive,
		pConnection
	) ) {
		xrtAtomic32Store(
			&pConnection->DrivePosted,
			0,
			XMEMORY_RELEASE
		);
		xrtWsConnDestroy(pConnection);
	}
}



/* 暂停后续应用消息分块；TCP 立即停止新接收，TLS 由未消费明文施加背压。 */
XWS_API void xrtWsConnPause(xwsconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"pause-websocket-read"
	) || (xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return;
	}
	xrtAtomic32Store(
		&pConnection->ReadPaused,
		1,
		XMEMORY_RELEASE
	);
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		xrtAtomic32Store(
			&pConnection->ReadPaused,
			0,
			XMEMORY_RELEASE
		);
		__xrtWsConnDriveSchedule(pConnection);
		return;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
			return;
		}
	#endif
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		xrtNetStreamPause((xnetstream*)pTransport);
	}
	__xrtWsConnTransportRelease(pConnection, pTransport);
}



/* 恢复读取，并让所属 Worker 继续消费已缓冲的 TCP 或 TLS 明文。 */
XWS_API bool xrtWsConnResume(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"resume-websocket-read",
			"WebSocket connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"resume-websocket-read"
	) ) {
		return false;
	}
	if ( xrtWsConnState(pConnection) == XWS_CONN_CLOSED ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
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
	__xrtWsConnDriveSchedule(pConnection);
	return true;
}



/* 返回应用读取暂停状态的并发快照。 */
XWS_API bool xrtWsConnPaused(const xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsConnCheck(
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
static void __xrtWsConnTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

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
static void __xrtWsConnTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsConnDiscardInput(pConnection) ) {
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsConnCloseTransport(pConnection);
		return;
	}
	if ( xrtWsConnPaused(pConnection) ) {
		xrtNetStreamPause(pStream);
		return;
	}
	__xrtWsConnRead(pConnection);
}



/* TCP 高水位和 Connection 自身预算共同折叠为一个背压状态。 */
static void __xrtWsConnTcpHigh(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsConnBackpressure((xwsconn*)pData);
}



/* TCP 回落到低水位后重新计算真正可写预算。 */
static void __xrtWsConnTcpLow(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtWsConnWritableEvent((xwsconn*)pData);
	__xrtWsConnNotifyFutures((xwsconn*)pData);
}



/* 两级队列排空后发布唯一 Drain 边沿。 */
static void __xrtWsConnDrain(xwsconn* pConnection)
{
	if ( !pConnection->DrainPending ||
		(xrtWsConnPending(pConnection) != 0) ) {
		return;
	}
	pConnection->DrainPending = false;
	__xrtWsConnWritableEvent(pConnection);
	if ( pConnection->Events.Drain != NULL ) {
		pConnection->Events.Drain(
			pConnection,
			pConnection->Data
		);
	}
}



/* TCP 排空回调完成 WebSocket Drain 边沿。 */
static void __xrtWsConnTcpDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtWsConnDrain((xwsconn*)pData);
	__xrtWsConnNotifyFutures((xwsconn*)pData);
}



/* 复制当前 Close 终态供同步回调和后续查询共用。 */
static void __xrtWsConnCloseSnapshot(
	const xwsconn* pConnection,
	xwsconnclose* pClose
)
{
	xwsconn* pMutable = (xwsconn*)pConnection;
	xwsconnclose Close;

	memset(&Close, 0, sizeof(Close));
	xrtSpinLock(&pMutable->TransportLock);
	if ( pConnection->CloseSent ) {
		Close.Flags |= XWS_CONN_CLOSE_SENT;
	}
	if ( pConnection->CloseReceived ) {
		Close.Flags |= XWS_CONN_CLOSE_RECEIVED;
	}
	if ( pConnection->CloseSent &&
		pConnection->CloseReceived &&
		((xnetresult)xrtAtomic32Load(
			&pConnection->TransportResult,
			XMEMORY_ACQUIRE
		 ) == XNET_RESULT_OK) ) {
		Close.Flags |= XWS_CONN_CLOSE_CLEAN;
	}
	if ( pConnection->RemoteInitiated ) {
		Close.Flags |= XWS_CONN_CLOSE_REMOTE;
	}
	Close.Transport = (xnetresult)xrtAtomic32Load(
		&pConnection->TransportResult,
		XMEMORY_ACQUIRE
	);
	Close.LocalCode = pConnection->LocalCode;
	Close.RemoteCode = pConnection->RemoteCode;
	Close.Reason.Data = pConnection->RemoteReason;
	Close.Reason.Size = pConnection->RemoteReasonSize;
	xrtSpinUnlock(&pMutable->TransportLock);
	memcpy(pClose, &Close, sizeof(Close));
}



/* 验证 Connection 及其协商协议副本占用的完整连续存储。 */
bool __xrtWsConnStorageRange(
	const xwsconn* pConnection,
	size_t* pSize
)
{
	size_t iSize;

	if ( !__xrtWsConnRangeValid(pConnection) ||
		(pConnection->Protocol.Size >
		 (SIZE_MAX - sizeof(*pConnection) - 1u)) ) {
		return false;
	}
	iSize = sizeof(*pConnection) +
		pConnection->Protocol.Size + 1u;
	if ( !xrtMemRangeValid(pConnection, iSize) ) {
		return false;
	}
	if ( pSize != NULL ) {
		*pSize = iSize;
	}
	return true;
}



/* 统一处理 TCP/TLS 传输终态并释放 Connection 的传输引用。 */
static void __xrtWsConnTransportClose(
	xwsconn* pConnection,
	ptr pTransport,
	xnetresult Result,
	const xerror* pError
)
{
	xwsconnclose Close;
	ptr pOwned;

	xrtAtomic32Store(
		&pConnection->TransportResult,
		(uint32)Result,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XWS_CONN_CLOSED,
		XMEMORY_RELEASE
	);
	__xrtWsConnCancelCloseTimer(pConnection);
	if ( (Result != XNET_RESULT_OK) &&
		(pError != NULL) &&
		(xrtWsConnError(pConnection) == NULL) ) {
		(void)__xrtWsConnRemember(
			pConnection,
			xrtErrorKind(pError),
			XWS_CONN_ERROR_TRANSPORT,
			"close-websocket-transport",
			"WebSocket transport closed with an error",
			pError
		);
		__xrtWsConnEmitError(pConnection);
	}
	__xrtWsConnOutputClear(pConnection);
	if ( !pConnection->CloseEmitted ) {
		pConnection->CloseEmitted = true;
		__xrtWsConnCloseSnapshot(pConnection, &Close);
		if ( pConnection->Events.Close != NULL ) {
			pConnection->Events.Close(
				pConnection,
				&Close,
				pConnection->Data
			);
		}
	}
	__xrtWsConnNotifyFutures(pConnection);
	xrtSpinLock(&pConnection->TransportLock);
	pOwned = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtSpinUnlock(&pConnection->TransportLock);
	if ( pOwned != NULL ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
			if ( pConnection->TransportKind ==
				__XWS_TRANSPORT_TLS ) {
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
	xrtWsConnDestroy(pConnection);
}



/* TCP Close 转入统一 WebSocket 终态。 */
static void __xrtWsConnTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsConnTransportClose(
		(xwsconn*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TCP 事件表。 */
static const xnetstreamevents* __xrtWsConnTcpEvents(void)
{
	static const xnetstreamevents Events = {
		NULL,
		__xrtWsConnTcpRead,
		__xrtWsConnTcpEnd,
		__xrtWsConnTcpHigh,
		__xrtWsConnTcpLow,
		__xrtWsConnTcpDrain,
		__xrtWsConnTcpClose
	};

	return &Events;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS Read 驱动同一消息状态机。 */
static void __xrtWsConnTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	(void)pBuffer;
	if ( pConnection->ProtocolFailed ) {
		if ( !__xrtWsConnDiscardInput(pConnection) ) {
			(void)xrtWsConnAbort(pConnection);
			return;
		}
		pConnection->ProtocolPeerActivity = true;
		__xrtWsConnCloseTransport(pConnection);
		return;
	}
	__xrtWsConnRead(pConnection);
}



/* TLS 明文写空间恢复后先排空 Connection 精确余量。 */
static void __xrtWsConnTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	if ( __xrtWsConnOutputDrive(pConnection) ) {
		__xrtWsConnWritableEvent(pConnection);
		__xrtWsConnNotifyFutures(pConnection);
	}
}



/* TLS 两级发送队列排空后发布 Connection Drain。 */
static void __xrtWsConnTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pStream;
	if ( __xrtWsConnOutputDrive(pConnection) ) {
		__xrtWsConnDrain(pConnection);
		__xrtWsConnNotifyFutures(pConnection);
	}
}



/* TLS 对端完成 close_notify 时继续认证关闭。 */
static void __xrtWsConnTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	xwsconn* pConnection = (xwsconn*)pData;

	if ( !pConnection->TransportClosing ) {
		pConnection->TransportClosing = true;
		if ( !xrtTlsStreamClose(pStream) ) {
			xrtClearError();
			(void)xrtTlsStreamAbort(pStream);
		}
	}
}



/* TLS Close 转入统一 WebSocket 终态。 */
static void __xrtWsConnTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	__xrtWsConnTransportClose(
		(xwsconn*)pData,
		pStream,
		Result,
		pError
	);
}



/* 返回唯一 TLS 事件表。 */
static const xtlsstreamevents* __xrtWsConnTlsEvents(void)
{
	static const xtlsstreamevents Events = {
		.Open = NULL,
		.Read = __xrtWsConnTlsRead,
		.End = __xrtWsConnTlsEnd,
		.Writable = __xrtWsConnTlsWritable,
		.Drain = __xrtWsConnTlsDrain,
		.Close = __xrtWsConnTlsClose,
		.Ticket = NULL
	};

	return &Events;
}
#endif



/* 创建共享 Connection 状态，但不提前改变传输事件所有权。 */
static xwsconn* __xrtWsConnCreate(
	xnetworker* pWorker,
	__xws_transport TransportKind,
	ptr pTransport,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xwsconnconfig Config;
	xwsconnevents Events;
	xwsmessageconfig MessageConfig;
	xwsconn* pConnection;
	size_t iAllocation;

	xrtWsConnConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			(void)__xrtWsConnRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_CONN_ERROR_ARGUMENT,
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
		if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
			(void)__xrtWsConnRemember(
				NULL,
				XERR_ARGUMENT,
				XWS_CONN_ERROR_ARGUMENT,
				"attach-websocket",
				"WebSocket connection event range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Events, pEvents, sizeof(Events));
	}
	if ( !__xrtWsConnConfigPrepare(&Config) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_VALUE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket connection configuration is invalid",
			NULL
		);
		return NULL;
	}
	iAllocation = sizeof(*pConnection) +
		Config.Protocol.Size + 1u;
	pConnection = (xwsconn*)xrtCalloc(1, iAllocation);
	if ( pConnection == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_MEMORY,
			XWS_CONN_ERROR_MEMORY,
			"attach-websocket",
			"WebSocket connection allocation failed",
			NULL
		);
		return NULL;
	}
	pConnection->References = 2;
	xrtAtomic32Init(&pConnection->State, XWS_CONN_OPEN);
	xrtAtomic32Init(
		&pConnection->TransportResult,
		XNET_RESULT_OK
	);
	xrtAtomicPtrInit(&pConnection->Transport, pTransport);
	xrtAtomicPtrInit(&pConnection->Error, NULL);
	xrtAtomic64Init(&pConnection->OutputBytes, 0);
	xrtAtomic32Init(&pConnection->ReadPaused, 0);
	xrtAtomic32Init(&pConnection->DrivePosted, 0);
	if ( !xrtNetPostInit(&pConnection->DriveCommand) ) {
		xrtFree(pConnection);
		return NULL;
	}
	xrtSpinInit(&pConnection->TransportLock);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		xrtAtomic64Init(&pConnection->AsyncBytes, 0);
		xrtAtomic32Init(&pConnection->AsyncCount, 0);
		if ( !xrtNetPostInit(&pConnection->AsyncCommand) ) {
			xrtSpinUnit(&pConnection->TransportLock);
			xrtFree(pConnection);
			return NULL;
		}
		xrtSpinInit(&pConnection->AsyncLock);
	#endif
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
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( Config.DeflateEnabled ) {
			MessageConfig.FirstRsv = XWS_FRAME_RSV1;
		}
	#endif
	if ( !xrtWsMessageInit(
		&pConnection->Message,
		&MessageConfig
	) ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
			xrtSpinUnit(&pConnection->AsyncLock);
		#endif
		xrtSpinUnit(&pConnection->TransportLock);
		xrtFree(pConnection);
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
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
				xrtSpinUnit(
					&pConnection->TransportLock
				);
				#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
					xrtSpinUnit(
						&pConnection->AsyncLock
					);
				#endif
				xrtFree(pConnection);
				(void)__xrtWsConnRemember(
					NULL,
					XERR_MEMORY,
					XWS_CONN_ERROR_MEMORY,
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
XWS_API void xrtWsConnConfigInit(xwsconnconfig* pConfig)
{
	xwsconnconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"config-init-websocket",
			"WebSocket connection configuration range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Role = XWS_ROLE_SERVER;
	Config.MessageLimit =
		XWS_CONN_MESSAGE_LIMIT_DEFAULT;
	Config.FrameLimit =
		XWS_CONN_FRAME_LIMIT_DEFAULT;
	Config.SendLimit = XWS_CONN_SEND_LIMIT_DEFAULT;
	Config.ControlReserve =
		XWS_CONN_CONTROL_RESERVE_DEFAULT;
	Config.CloseTimeout =
		XWS_CONN_CLOSE_TIMEOUT_DEFAULT;
	Config.AutoPong = true;
	Config.AsyncBytesLimit =
		XWS_CONN_ASYNC_BYTES_DEFAULT;
	Config.AsyncCountLimit =
		XWS_CONN_ASYNC_COUNT_DEFAULT;
	Config.AsyncBatch =
		XWS_CONN_ASYNC_BATCH_DEFAULT;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xrtWsDeflateInit(&Config.Deflate);
		xrtWsInflaterConfigInit(&Config.Inflater);
		xrtWsDeflaterConfigInit(&Config.Deflater);
	#else
		Config.Deflate.ServerMaxWindowBits = XWS_DEFLATE_WINDOW_MAX;
		Config.Deflate.ClientMaxWindowBits = XWS_DEFLATE_WINDOW_MAX;
		Config.Inflater.OutputLimit = XWS_INFLATE_OUTPUT_DEFAULT;
		Config.Inflater.WindowBits = XWS_DEFLATE_WINDOW_MAX;
		Config.Deflater.OutputLimit = UINT64_MAX;
		Config.Deflater.Level = 6;
		Config.Deflater.Strategy = XDEFLATE_STRATEGY_DEFAULT;
		Config.Deflater.WindowBits = XWS_DEFLATE_WINDOW_MAX;
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* Attach 失败时只销毁 Connection 草稿，不接管或关闭输入传输。 */
static void __xrtWsConnAttachDiscard(xwsconn* pConnection)
{
	pConnection->References = 1;
	xrtAtomicPtrStore(
		&pConnection->Transport,
		NULL,
		XMEMORY_RELEASE
	);
	xrtWsConnDestroy(pConnection);
}



/*
	把控制槽换算到实际传输成本，并验证普通数据至少能容纳一个空帧。
	TLS 记录开销在握手完成后固定，Connection 后续可以无查询地精确计账。
*/
static bool __xrtWsConnAttachBudget(
	xwsconn* pConnection,
	ptr pTransport
)
{
	size_t iControl = __xrtWsConnControlSlot(
		pConnection->Config.Role
	);
	size_t iDataFrame = 2u + (
		pConnection->Config.Role == XWS_ROLE_CLIENT ?
			XWS_MASK_SIZE : 0u
	);
	size_t iDataBudget;
	size_t iMinimum;

	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
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
	if ( !__xrtWsConnTransportSize(
		pConnection,
		iControl,
		&pConnection->ControlSlot
	) || !__xrtWsConnTransportSize(
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
	if ( pConnection->TransportKind == __XWS_TRANSPORT_TCP ) {
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
XWS_API xwsconn* xrtWsConnAttach(
	xnetstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xnetworker* pWorker;
	xwsconn* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"attach-websocket",
			"WebSocket TCP Stream is null",
			NULL
		);
		return NULL;
	}
	pWorker = xrtNetStreamWorker(pStream);
	if ( (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(xrtNetStreamState(pStream) != XNET_STREAM_OPEN) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"attach-websocket",
			"WebSocket TCP Stream is not open on its worker",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsConnCreate(
		pWorker,
		__XWS_TRANSPORT_TCP,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnAttachBudget(pConnection, pStream) ) {
		__xrtWsConnAttachDiscard(pConnection);
		(void)__xrtWsConnRemember(
			NULL,
			XERR_RANGE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket",
			"WebSocket send budget is incompatible with its TCP Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtNetStreamSetEvents(
		pStream,
		__xrtWsConnTcpEvents(),
		pConnection
	) ) {
		__xrtWsConnAttachDiscard(pConnection);
		return NULL;
	}
	__xrtWsConnDriveSchedule(pConnection);
	return pConnection;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 接管开放 TLS Stream，并延迟处理已经解密的早到明文。 */
XWS_API xwsconn* xrtWsConnAttachTls(
	xtlsstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
)
{
	xnetstream* pTransport;
	xnetworker* pWorker;
	xwsconn* pConnection;

	if ( pStream == NULL ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
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
		 XTLS_STREAM_OPEN) ) {
		(void)__xrtWsConnRemember(
			NULL,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"attach-websocket-tls",
			"WebSocket TLS Stream is not open on its worker",
			NULL
		);
		return NULL;
	}
	pConnection = __xrtWsConnCreate(
		pWorker,
		__XWS_TRANSPORT_TLS,
		pStream,
		pConfig,
		pEvents,
		pData
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnAttachBudget(pConnection, pStream) ) {
		__xrtWsConnAttachDiscard(pConnection);
		(void)__xrtWsConnRemember(
			NULL,
			XERR_RANGE,
			XWS_CONN_ERROR_CONFIG,
			"attach-websocket-tls",
			"WebSocket send budget is incompatible with its TLS Stream",
			NULL
		);
		return NULL;
	}
	if ( !xrtTlsStreamSetEvents(
		pStream,
		__xrtWsConnTlsEvents(),
		pConnection
	) ) {
		__xrtWsConnAttachDiscard(pConnection);
		return NULL;
	}
	__xrtWsConnDriveSchedule(pConnection);
	return pConnection;
}
#endif



/* 增加 Connection 引用。 */
XWS_API xwsconn* xrtWsConnRef(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket"
	) || (xrtRefRetain(&pConnection->References) < 0) ) {
		return NULL;
	}
	return pConnection;
}



/* 释放 Connection 引用。 */
XWS_API void xrtWsConnDestroy(xwsconn* pConnection)
{
	if ( pConnection == NULL ) {
		return;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"release-websocket"
	) ) {
		return;
	}
	if ( xrtRefRelease(&pConnection->References) == 0 ) {
		__xrtWsConnFree(pConnection);
	}
}



/* 返回并发可读状态。 */
XWS_API xwsconnstate xrtWsConnState(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_CONN_CLOSED;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-state"
	) ) {
		return XWS_CONN_CLOSED;
	}
	return (xwsconnstate)xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回固定本端角色。 */
XWS_API xwsrole xrtWsConnRole(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return XWS_ROLE_SERVER;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-role"
	) ) {
		return XWS_ROLE_SERVER;
	}
	return pConnection->Config.Role;
}



/* 返回拥有型已协商子协议的借用视图。 */
XWS_API xstrview xrtWsConnProtocol(
	const xwsconn* pConnection
)
{
	xstrview Empty = { 0 };

	if ( pConnection == NULL ) {
		return Empty;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-protocol"
	) ) {
		return Empty;
	}
	return pConnection->Protocol;
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 复制固定的 permessage-deflate 协商结果。 */
XWS_API bool xrtWsConnDeflate(
	const xwsconn* pConnection,
	xwsdeflate* pDeflate
)
{
	xwsdeflate Deflate;
	size_t iConnectionSize;

	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-deflate"
	) ) {
		return false;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnectionSize
	) || !xrtMemRangeValid(pDeflate, sizeof(Deflate)) ||
		xrtMemRangesOverlap(
			pDeflate,
			sizeof(Deflate),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
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
XWS_API xnetworker* xrtWsConnWorker(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-worker"
	) ) {
		return NULL;
	}
	return pConnection->Worker;
}



/* TCP 会话返回借用 Stream。 */
XWS_API xnetstream* xrtWsConnTcp(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XWS_TRANSPORT_TCP) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
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
XWS_API xnetstream* xrtWsConnTcpRef(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket-tcp"
	) || (pConnection->TransportKind !=
		 __XWS_TRANSPORT_TCP) ) {
		return NULL;
	}
	return (xnetstream*)__xrtWsConnTransportRef(
		pConnection
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* TLS 会话返回借用 Stream。 */
XWS_API xtlsstream* xrtWsConnTls(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XWS_TRANSPORT_TLS) ) {
		return NULL;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
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
XWS_API xtlsstream* xrtWsConnTlsRef(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"retain-websocket-tls"
	) || (pConnection->TransportKind !=
		 __XWS_TRANSPORT_TLS) ) {
		return NULL;
	}
	return (xtlsstream*)__xrtWsConnTransportRef(
		pConnection
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 单条压缩消息只按实际输出增长，不在连接对象中保留固定缓冲。 */
typedef struct __xws_compressed {
	bytes Data;
	size_t Size;
	size_t Capacity;
	size_t Limit;
	xnetbuf* Buffer;
} __xws_compressed;



/* 在当前线路预算内收集 Deflater 的同步输出。 */
static bool __xrtWsConnDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xws_compressed* pOutput =
		(__xws_compressed*)pData;
	size_t iRequired;
	size_t iCapacity;
	bytes pBytes;

	if ( (pOutput->Size > pOutput->Limit) ||
		(Data.Size >
		 (pOutput->Limit - pOutput->Size)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
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
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
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
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
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
static xnetresult __xrtWsConnDeflateBufferSubmit(
	xwsconn* pConnection,
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

	Result = __xrtWsConnFrameBudget(
		pConnection,
		iPayload,
		__XWS_SEND_DATA,
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
		(void)__xrtWsConnReject(
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_INTERNAL,
			XWS_CONN_ERROR_FRAME,
			"compress-websocket-message",
			"compressed WebSocket frame construction failed",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pStream = xrtWsConnTcp(pConnection);
	if ( pStream == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	Result = xrtNetStreamSendBuffer(pStream, pBuffer);
	if ( Result == XNET_RESULT_OK ) {
		pConnection->DrainPending = true;
		return Result;
	}
	if ( Result == XNET_RESULT_AGAIN ) {
		__xrtWsConnBackpressure(pConnection);
	} else if ( Result == XNET_RESULT_ERROR ) {
		const xerror* pCause = xrtNetStreamError(pStream);

		if ( pCause == NULL ) {
			pCause = xrtGetError();
		}
		__xrtWsConnSendFailure(
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
static void __xrtWsConnDeflateRollback(
	xwsconn* pConnection
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



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/* 流式压缩帧把最终线路节点和最大负载放在同一分配中。 */
typedef struct __xrt_ws_deflate_frame {
	__xws_output* Output;
	size_t Size;
	size_t Limit;
	xnetbuf* Buffer;
} __xrt_ws_deflate_frame;



/* 把 Deflater 的同步输出直接追加到已经预留的最终帧负载区。 */
static bool __xrtWsConnDeflateFrameOutput(
	xbytesview Data,
	ptr pData
)
{
	__xrt_ws_deflate_frame* pFrame =
		(__xrt_ws_deflate_frame*)pData;

	if ( (pFrame->Size > pFrame->Limit) ||
		(Data.Size > (pFrame->Limit - pFrame->Size)) ) {
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"WebSocket Deflater exceeded its advertised output bound",
			NULL
		);
		return false;
	}
	if ( pFrame->Buffer != NULL ) {
		if ( !xrtNetBufAppend(
			pFrame->Buffer,
			Data.Data,
			Data.Size
		) ) {
			(void)__xrtWsConnReject(
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) : XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"write-compressed-websocket-fragment",
				"compressed WebSocket fragment buffer allocation failed",
				xrtGetError()
			);
			return false;
		}
		pFrame->Size += Data.Size;
		return true;
	}
	if ( Data.Size != 0 ) {
		memcpy(
			pFrame->Output->Data +
				XWS_FRAME_HEAD_MAX + pFrame->Size,
			Data.Data,
			Data.Size
		);
	}
	pFrame->Size += Data.Size;
	return true;
}



/* 半条压缩消息已经在线路上时，任何后续编码故障都必须终止会话。 */
static void __xrtWsConnDeflatePartFatal(
	xwsconn* pConnection,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	(void)__xrtWsConnRemember(
		pConnection,
		pCause != NULL ?
			xrtErrorKind(pCause) : XERR_INTERNAL,
		XWS_CONN_ERROR_SEND,
		"write-compressed-websocket-fragment",
		sMessage,
		pCause
	);
	__xrtWsConnEmitError(pConnection);
	(void)xrtWsConnAbort(pConnection);
}



/*
	预留最大编码负载，推进一个同步压缩边界，再把实际负载原地封成最终帧。
	预算、掩码和 OOM 都在 Deflater 状态改变前完成。
*/
xnetresult __xrtWsConnSendDeflatePart(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Input,
	bool bFirst,
	bool bFinal
)
{
	__xrt_ws_deflate_frame Output;
	xnetbuf Buffer;
	xwsframe Frame;
	xwsframeconfig FrameConfig;
	size_t iBound;
	size_t iAllocation;
	size_t iHead = 0;
	size_t iWireSize;
	xnetresult Budget;
	xnetresult Result;
	bool bEncoded;
	bool bBuffer;

	if ( !xrtWsDeflaterBound(Input.Size, &iBound) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"compressed WebSocket fragment bound is not representable",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	Budget = __xrtWsConnFrameBudget(
		pConnection,
		iBound,
		__XWS_SEND_DATA,
		&iWireSize
	);
	if ( Budget != XNET_RESULT_OK ) {
		return Budget;
	}
	if ( iBound >
		(SIZE_MAX - XWS_FRAME_HEAD_MAX -
		 sizeof(*Output.Output)) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-compressed-websocket-fragment",
			"compressed WebSocket frame allocation size overflowed",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memset(&Output, 0, sizeof(Output));
	memset(&Buffer, 0, sizeof(Buffer));
	bBuffer = (pConnection->Config.Role == XWS_ROLE_SERVER) &&
		(pConnection->TransportKind == __XWS_TRANSPORT_TCP);
	if ( bBuffer ) {
		if ( !xrtNetBufInit(
			&Buffer,
			xrtNetWorkerBufPool(pConnection->Worker)
		) ) {
			return XNET_RESULT_ERROR;
		}
		Output.Buffer = &Buffer;
	} else {
		iAllocation = sizeof(*Output.Output) +
			XWS_FRAME_HEAD_MAX + iBound;
		Output.Output = (__xws_output*)xrtMalloc(iAllocation);
		if ( Output.Output == NULL ) {
			(void)__xrtWsConnReject(
				XERR_MEMORY,
				XWS_CONN_ERROR_MEMORY,
				"write-compressed-websocket-fragment",
				"compressed WebSocket frame allocation failed",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
		memset(Output.Output, 0, sizeof(*Output.Output));
	}
	Output.Size = 0;
	Output.Limit = iBound;

	xrtWsFrameInit(&Frame);
	Frame.Opcode = (uint8)Opcode;
	if ( bFinal ) {
		Frame.Flags |= XWS_FRAME_FIN;
	}
	if ( bFirst ) {
		Frame.Flags |= XWS_FRAME_RSV1;
	}
	if ( pConnection->Config.Role == XWS_ROLE_CLIENT ) {
		Frame.Flags |= XWS_FRAME_MASKED;
		if ( !xrtSecureRandom(Frame.Mask, sizeof(Frame.Mask)) ) {
			xrtFree(Output.Output);
			(void)__xrtWsConnReject(
				XERR_IO,
				XWS_CONN_ERROR_RANDOM,
				"write-compressed-websocket-fragment",
				"WebSocket client could not generate a frame mask",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
	}

	bEncoded = (!bFirst || xrtWsDeflaterBegin(
		pConnection->Deflater,
		true
	)) && xrtWsDeflaterWrite(
		pConnection->Deflater,
		Input,
		__xrtWsConnDeflateFrameOutput,
		&Output
	) && (bFinal ? xrtWsDeflaterEnd(
		pConnection->Deflater,
		__xrtWsConnDeflateFrameOutput,
		&Output
	) : xrtWsDeflaterFlush(
		pConnection->Deflater,
		__xrtWsConnDeflateFrameOutput,
		&Output
	));
	if ( !bEncoded ) {
		xrtFree(Output.Output);
		xrtNetBufClear(&Buffer);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compression failed after a fragmented message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( Output.Buffer != NULL ) {
		Result = __xrtWsConnDeflateBufferSubmit(
			pConnection,
			Opcode,
			&Buffer,
			Output.Size,
			bFirst,
			bFinal
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtNetBufClear(&Buffer);
			if ( bFirst ) {
				__xrtWsConnDeflateRollback(pConnection);
			} else {
				__xrtWsConnDeflatePartFatal(
					pConnection,
					"WebSocket transport rejected a compressed fragmented message"
				);
				Result = XNET_RESULT_ERROR;
			}
		}
		return Result;
	}

	Frame.PayloadSize = Output.Size;
	xrtWsFrameConfigInit(&FrameConfig);
	FrameConfig.AllowedRsv = XWS_FRAME_RSV1;
	if ( !xrtWsFrameWrite(
		&Frame,
		&FrameConfig,
		NULL,
		0,
		&iHead
	) || (iHead > XWS_FRAME_HEAD_MAX) ) {
		xrtFree(Output.Output);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compressed frame header failed after a message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( Output.Size != 0 ) {
		memmove(
			Output.Output->Data + iHead,
			Output.Output->Data + XWS_FRAME_HEAD_MAX,
			Output.Size
		);
	}
	if ( !xrtWsFrameWrite(
		&Frame,
		&FrameConfig,
		Output.Output->Data,
		iHead,
		&iHead
	) ) {
		xrtFree(Output.Output);
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket compressed frame construction failed after a message started"
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( ((Frame.Flags & XWS_FRAME_MASKED) != 0) &&
		(Output.Size != 0) ) {
		(void)xrtWsMask(
			Output.Output->Data + iHead,
			Output.Size,
			Frame.Mask,
			0
		);
	}
	Output.Output->Size = iHead + Output.Size;
	Result = __xrtWsConnSubmit(
		pConnection,
		Output.Output,
		__XWS_SEND_DATA
	);
	if ( Result != XNET_RESULT_OK ) {
		if ( bFirst ) {
			__xrtWsConnDeflateRollback(pConnection);
		} else {
			__xrtWsConnDeflatePartFatal(
				pConnection,
				"WebSocket transport rejected a compressed fragmented message"
			);
			Result = XNET_RESULT_ERROR;
		}
	}
	return Result;
}
#endif



/* 压缩、封成单帧并仅在传输受理后提交上下文。 */
static xnetresult __xrtWsConnSendDeflate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	__xws_compressed Output;
	xnetbuf Buffer;
	xnetresult Result;
	size_t iMinimum;
	size_t iAvailable = __xrtWsConnAvailable(
		pConnection,
		__XWS_SEND_DATA
	);
	size_t iCapacity = __xrtWsConnCapacity(
		pConnection,
		__XWS_SEND_DATA
	);

	if ( iCapacity == 0 ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket connection has no permanent data send capacity",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnFrameSize(
		pConnection,
		0,
		&iMinimum
	) || !__xrtWsConnTransportSize(
		pConnection,
		iMinimum,
		&iMinimum
	) ) {
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_LIMIT,
			"compress-websocket-message",
			"WebSocket minimum compressed frame size is not representable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( iAvailable < iMinimum ) {
		__xrtWsConnBackpressure(pConnection);
		return XNET_RESULT_AGAIN;
	}
	memset(&Output, 0, sizeof(Output));
	(void)xrtNetBufInit(&Buffer, NULL);
	if ( (pConnection->Config.Role == XWS_ROLE_SERVER) &&
		(pConnection->TransportKind == __XWS_TRANSPORT_TCP) ) {
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
		__xrtWsConnDeflateOutput,
		&Output
	) || !xrtWsDeflaterEnd(
		pConnection->Deflater,
		__xrtWsConnDeflateOutput,
		&Output
	) ) {
		const xerror* pCause = xrtGetError();

		if ( (pCause == NULL) ||
			(xrtErrorDomain(pCause) == NULL) ||
			(strcmp(
				xrtErrorDomain(pCause),
				"xrt.websocket.connection"
			 ) != 0) ) {
			(void)__xrtWsConnReject(
				pCause != NULL ?
					xrtErrorKind(pCause) :
					XERR_INTERNAL,
				XWS_CONN_ERROR_MESSAGE,
				"compress-websocket-message",
				"WebSocket message compression failed",
				pCause
			);
		}
		xrtFree(Output.Data);
		xrtNetBufClear(&Buffer);
		__xrtWsConnDeflateRollback(pConnection);
		return XNET_RESULT_ERROR;
	}
	if ( Output.Buffer != NULL ) {
		Result = __xrtWsConnDeflateBufferSubmit(
			pConnection,
			Opcode,
			&Buffer,
			Output.Size,
			true,
			true
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtNetBufClear(&Buffer);
			__xrtWsConnDeflateRollback(pConnection);
		}
		return Result;
	}
	Result = __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		(xbytesview) {
			Output.Data,
			Output.Size
		},
		true,
		__XWS_SEND_DATA,
		true
	);
	xrtFree(Output.Data);
	if ( Result != XNET_RESULT_OK ) {
		__xrtWsConnDeflateRollback(pConnection);
	}
	return Result;
}
#endif



/* 验证一条完整 Text 或 Binary 消息的不可变参数。 */
bool __xrtWsConnMessageCheck(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	size_t iWireSize;

	if ( (Opcode != XWS_OPCODE_TEXT) &&
		(Opcode != XWS_OPCODE_BINARY) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-message",
			"WebSocket message opcode must be Text or Binary",
			NULL
		);
		return false;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
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
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
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
			(void)__xrtWsConnReject(
				XERR_VALUE,
				XWS_CONN_ERROR_MESSAGE,
				"send-websocket-message",
				"WebSocket text message is not valid UTF-8",
				xrtGetError()
			);
			return false;
		}
	}
	if ( !bCompressed &&
		(!__xrtWsConnFrameSize(
			pConnection,
			Payload.Size,
			&iWireSize
		 ) || (iWireSize >
			__xrtWsConnCapacity(
				pConnection,
				__XWS_SEND_DATA
			))) ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"send-websocket-message",
			"WebSocket message exceeds its permanent send capacity",
			NULL
		);
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( bCompressed ) {
			if ( !pConnection->Config.DeflateEnabled ) {
				(void)__xrtWsConnReject(
					XERR_STATE,
					XWS_CONN_ERROR_CONFIG,
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
static xnetresult __xrtWsConnSendMessage(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	if ( !__xrtWsConnWorker(
		pConnection,
		"send-websocket-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		if ( pConnection->Writer != NULL ) {
			return XNET_RESULT_AGAIN;
		}
	#endif
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		bCompressed
	) ) {
		return XNET_RESULT_ERROR;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( bCompressed ) {
			return __xrtWsConnSendDeflate(
				pConnection,
				Opcode,
				Payload
			);
		}
	#endif
	return __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XWS_SEND_DATA,
		false
	);
}



/* 发送一条完整 Text 或 Binary 消息。 */
XWS_API xnetresult xrtWsConnSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendMessage(
		pConnection,
		Opcode,
		Payload,
		false
	);
}



/* 发送完整 Text。 */
XWS_API xnetresult xrtWsConnText(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSend(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 发送完整 Binary。 */
XWS_API xnetresult xrtWsConnBinary(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSend(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 压缩并发送完整 Text 或 Binary。 */
XWS_API xnetresult xrtWsConnSendCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendMessage(
		pConnection,
		Opcode,
		Payload,
		true
	);
}



/* 压缩并发送完整 Text。 */
XWS_API xnetresult xrtWsConnTextCompressed(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendCompressed(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 压缩并发送完整 Binary。 */
XWS_API xnetresult xrtWsConnBinaryCompressed(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendCompressed(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}
#endif



/* 在所属 Worker 上发送一条 Ping 或 Pong 控制帧。 */
static xnetresult __xrtWsConnControlSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	cstr sOperation = Opcode == XWS_OPCODE_PING ?
		"send-websocket-ping" :
		"send-websocket-pong";

	if ( !__xrtWsConnWorker(
		pConnection,
		sOperation
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	return __xrtWsConnSendFrame(
		pConnection,
		Opcode,
		Payload,
		true,
		__XWS_SEND_CONTROL,
		false
	);
}



/* 发送 Ping 控制帧。 */
XWS_API xnetresult xrtWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlSend(
		pConnection,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 发送 Pong 控制帧。 */
XWS_API xnetresult xrtWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlSend(
		pConnection,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 发送唯一 Close 并等待远端回应。 */
XWS_API xnetresult xrtWsConnClose(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iSize = 0;

	if ( !__xrtWsConnWorker(
		pConnection,
		"close-websocket"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pConnection->CloseSent ||
		(xrtWsConnState(pConnection) != XWS_CONN_OPEN) ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		Payload,
		sizeof(Payload),
		&iSize
	) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"close-websocket",
			"WebSocket Close code or reason is invalid",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtWsConnClosePayload(
		pConnection,
		(xbytesview) { Payload, iSize },
		iCode,
		false
	);
}



/* 从任意线程安全取得临时传输引用并请求异常关闭。 */
XWS_API bool xrtWsConnAbort(xwsconn* pConnection)
{
	ptr pTransport;
	bool bAccepted;
	uint32 iState;

	if ( pConnection == NULL ) {
		return false;
	}
	if ( !__xrtWsConnCheck(
		pConnection,
		"abort-websocket"
	) ) {
		return false;
	}
	iState = xrtAtomic32Load(
		&pConnection->State,
		XMEMORY_ACQUIRE
	);
	while ( iState == XWS_CONN_OPEN ) {
		if ( xrtAtomic32CompareExchange(
			&pConnection->State,
			&iState,
			XWS_CONN_CLOSING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	if ( iState == XWS_CONN_CLOSED ) {
		return false;
	}
	pTransport = __xrtWsConnTransportRef(pConnection);
	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_TLS)
		if ( pConnection->TransportKind ==
			__XWS_TRANSPORT_TLS ) {
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
	__xrtWsConnTransportRelease(
		pConnection,
		pTransport
	);
	return bAccepted;
}



/* 复制 Close 快照。 */
XWS_API bool xrtWsConnCloseInfo(
	const xwsconn* pConnection,
	xwsconnclose* pClose
)
{
	size_t iConnectionSize;

	if ( !__xrtWsConnCheck(
		pConnection,
		"query-websocket-close"
	) ) {
		return false;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnectionSize
	) || !xrtMemRangeValid(pClose, sizeof(*pClose)) ||
		xrtMemRangesOverlap(
			pClose,
			sizeof(*pClose),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"query-websocket-close",
			"WebSocket connection or disjoint Close output is invalid",
			NULL
		);
		return false;
	}
	__xrtWsConnCloseSnapshot(pConnection, pClose);
	return true;
}



/* 返回第一个结构化错误。 */
XWS_API const xerror* xrtWsConnError(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !__xrtWsConnCheck(
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
#endif


/* ========================================================================== */
/* source: extlibs/xws/src/websocket/connection_ref.c */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF)

/* 明文服务端仅为帧头分配一个最小所有权节点。 */
typedef struct __xws_ref_head {
	xnetworker* Worker;
	uint8 Data[XWS_FRAME_HEAD_MAX];
} __xws_ref_head;



/* TCP 完成帧头发送后释放独立头部节点。 */
static void __xrtWsConnRefHeadRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	{
		__xws_ref_head* pHead = (__xws_ref_head*)pContext;

		xrtNetWorkerFree(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
	}
}



/* Take 路径沿用 XRT 分配器释放调用方交出的负载。 */
void __xrtWsConnTakeRelease(
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
static bool __xrtWsConnRefFrame(
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
		(void)__xrtWsConnReject(
			XERR_INTERNAL,
			XWS_CONN_ERROR_FRAME,
			"write-websocket-ref",
			"WebSocket reference frame header construction failed",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 把未掩码帧头和负载作为一个不可分割的 TCP 引用批次提交。 */
static xnetresult __xrtWsConnRefSubmit(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xnetref Payload,
	bool bFinal
)
{
	__xws_ref_head* pHead;
	xnetstream* pStream;
	xnetref Refs[2];
	size_t iHeadSize = 0;
	size_t iWireSize;
	xnetresult Result;

	Result = __xrtWsConnFrameBudget(
		pConnection,
		Payload.Size,
		__XWS_SEND_DATA,
		&iWireSize
	);
	if ( Result != XNET_RESULT_OK ) {
		return Result;
	}
	if ( !__xrtWsConnRefFrame(
		Opcode,
		Payload.Size,
		bFinal,
		NULL,
		0,
		&iHeadSize
	) ) {
		return XNET_RESULT_ERROR;
	}
	pHead = (__xws_ref_head*)xrtNetWorkerAlloc(
		pConnection->Worker,
		sizeof(*pHead)
	);
	if ( pHead == NULL ) {
		(void)__xrtWsConnReject(
			XERR_MEMORY,
			XWS_CONN_ERROR_MEMORY,
			"write-websocket-ref",
			"WebSocket reference frame header allocation failed",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pHead->Worker = pConnection->Worker;
	if ( !__xrtWsConnRefFrame(
		Opcode,
		Payload.Size,
		bFinal,
		pHead->Data,
		sizeof(pHead->Data),
		&iHeadSize
	) ) {
		xrtNetWorkerFree(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
		return XNET_RESULT_ERROR;
	}
	pStream = xrtWsConnTcp(pConnection);
	if ( pStream == NULL ) {
		xrtNetWorkerFree(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
		return XNET_RESULT_CLOSED;
	}
	Refs[0] = (xnetref) {
		pHead->Data,
		iHeadSize,
		__xrtWsConnRefHeadRelease,
		pHead
	};
	Refs[1] = Payload;
	Result = xrtNetStreamSendRefs(
		pStream,
		Refs,
		sizeof(Refs) / sizeof(Refs[0])
	);
	if ( Result != XNET_RESULT_OK ) {
		xrtNetWorkerFree(
			pHead->Worker,
			pHead,
			sizeof(*pHead)
		);
		if ( Result == XNET_RESULT_AGAIN ) {
			__xrtWsConnBackpressure(pConnection);
		} else if ( Result == XNET_RESULT_ERROR ) {
			const xerror* pCause = xrtNetStreamError(pStream);

			if ( pCause == NULL ) {
				pCause = xrtGetError();
			}
			__xrtWsConnSendFailure(
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
xnetresult __xrtWsConnSendRefFrame(
	xwsconn* pConnection,
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
		(pConnection->TransportKind != __XWS_TRANSPORT_TCP) ) {
		Result = __xrtWsConnSendFrame(
			pConnection,
			Opcode,
			Payload,
			bFinal,
			__XWS_SEND_DATA,
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
	return __xrtWsConnRefSubmit(
		pConnection,
		Opcode,
		Ref,
		bFinal
	);
}



/* 验证所有权契约并选择零复制或必要复制路径。 */
XWS_API xnetresult xrtWsConnSendRef(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	xnetref Ref;
	xbytesview Payload;
	size_t iConnectionSize;
	xnetresult Result;

	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnectionSize
	) || !xrtMemRangeValid(pRef, sizeof(Ref)) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket connection or reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( xrtMemRangesOverlap(
			pRef,
			sizeof(Ref),
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket connection or reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( !xrtMemRangeValid(Ref.Data, Ref.Size) ||
		(Ref.Size == 0) || (Ref.Release == NULL) ||
		xrtMemRangesOverlap(
			Ref.Data,
			Ref.Size,
			pConnection,
			iConnectionSize
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref",
			"WebSocket reference is incomplete, empty, "
			"or overlaps its connection",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtWsConnWorker(
		pConnection,
		"send-websocket-ref"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER)
		if ( pConnection->Writer != NULL ) {
			return XNET_RESULT_AGAIN;
		}
	#endif
	Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		false
	) ) {
		return XNET_RESULT_ERROR;
	}
	Result = __xrtWsConnSendRefFrame(
		pConnection,
		Opcode,
		Ref,
		true
	);
	return Result;
}



/* 发送一条所有权 UTF-8 Text 消息。 */
XWS_API xnetresult xrtWsConnTextRef(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRef(
		pConnection,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 发送一条所有权 Binary 消息。 */
XWS_API xnetresult xrtWsConnBinaryRef(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRef(
		pConnection,
		XWS_OPCODE_BINARY,
		pRef
	);
}



/* 把一段 XRT 内存包装成一次性释放引用。 */
XWS_API xnetresult xrtWsConnSendTake(
	xwsconn* pConnection,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsConnTakeRelease,
		NULL
	};

	return xrtWsConnSendRef(
		pConnection,
		Opcode,
		&Ref
	);
}



/* 发送并接管一段 XRT UTF-8 Text。 */
XWS_API xnetresult xrtWsConnTextTake(
	xwsconn* pConnection,
	str sText,
	size_t iSize
)
{
	return xrtWsConnSendTake(
		pConnection,
		XWS_OPCODE_TEXT,
		sText,
		iSize
	);
}



/* 发送并接管一段 XRT Binary。 */
XWS_API xnetresult xrtWsConnBinaryTake(
	xwsconn* pConnection,
	bytes pData,
	size_t iSize
)
{
	return xrtWsConnSendTake(
		pConnection,
		XWS_OPCODE_BINARY,
		pData,
		iSize
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws/src/websocket/writer.c */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_WRITER)



#if defined(XWS_FEATURE_WEBSOCKET_WRITER)

/* 设置 Writer 的可恢复 Connection 域错误。 */
static void __xrtWsWriterReject(
	xwswriter* pWriter,
	xerrkind Kind,
	xwsconnerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		(pWriter->Connection == NULL) ) {
		__xwsErrorSetInvalidArgument();
		return;
	}
	(void)__xrtWsConnReject(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* Writer 释放独占权后唤醒排队的异步完整消息。 */
static void __xrtWsWriterNotify(xwsconn* pConnection)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		__xrtWsConnFutureNotify(pConnection);
	#else
		(void)pConnection;
	#endif
}



/* 清除当前 Writer；重复清理不会影响后来创建的 Writer。 */
static void __xrtWsWriterUnlock(xwswriter* pWriter)
{
	xwsconn* pConnection = pWriter->Connection;

	if ( (pConnection != NULL) &&
		(pConnection->Writer == pWriter) ) {
		pConnection->Writer = NULL;
		__xrtWsWriterNotify(pConnection);
	}
}



/* 释放嵌入式 Writer；已经开始的半条线路消息必须终止会话。 */
static void __xrtWsWriterDispose(xwswriter* pWriter)
{
	xwsconn* pConnection = pWriter->Connection;

	pWriter->DestroyRequested = true;
	if ( !pWriter->Finished && pWriter->Started &&
		(pConnection != NULL) &&
		(xrtWsConnState(pConnection) == XWS_CONN_OPEN) ) {
		xnetresult Result = xrtWsConnClose(
			pConnection,
			XWS_CLOSE_INTERNAL,
			XRT_STR_LITERAL("message writer abandoned")
		);

		if ( Result != XNET_RESULT_OK ) {
			(void)xrtWsConnAbort(pConnection);
		}
	}
	__xrtWsWriterUnlock(pWriter);
	pWriter->Connection = NULL;
	xrtWsConnDestroy(pConnection);
}



/*
	在不修改原状态的副本上检查累计上限和 UTF-8。
	调用方只有在线路帧受理后才提交副本。
*/
static bool __xrtWsWriterPrepare(
	xwswriter* pWriter,
	xbytesview Data,
	bool bFinal,
	xutf8state* pUtf8,
	size_t* pSize
)
{
	xwsconn* pConnection = pWriter->Connection;

	if ( !xrtMemRangeValid(Data.Data, Data.Size) ||
		xrtMemRangesOverlap(
			Data.Data, Data.Size, pWriter, sizeof(*pWriter)
		) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message",
			"WebSocket Writer data view is invalid",
			NULL
		);
		return false;
	}
	if ( (pWriter->Size > pConnection->Config.MessageLimit) ||
		(Data.Size >
		 (pConnection->Config.MessageLimit - pWriter->Size)) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"write-websocket-message",
			"WebSocket Writer exceeded the message limit",
			NULL
		);
		return false;
	}
	*pSize = pWriter->Size + Data.Size;
	*pUtf8 = pWriter->Utf8;
	if ( pWriter->Opcode == XWS_OPCODE_TEXT ) {
		xutfstatus Status = xrtUtf8StateFeed(
			pUtf8,
			(xstrview) {
				(const char*)Data.Data,
				Data.Size
			},
			bFinal
		);

		if ( (Status == XUTF_INVALID) ||
			(Status == XUTF_OVERFLOW) ||
			(bFinal && (Status != XUTF_OK)) ) {
			__xrtWsWriterReject(
				pWriter,
				Status == XUTF_OVERFLOW ?
					XERR_RANGE : XERR_VALUE,
				Status == XUTF_OVERFLOW ?
					XWS_CONN_ERROR_LIMIT :
					XWS_CONN_ERROR_MESSAGE,
				"write-websocket-message",
				Status == XUTF_OVERFLOW ?
					"WebSocket Writer UTF-8 size overflowed" :
					"WebSocket Writer text is not valid UTF-8",
				Status == XUTF_OVERFLOW ?
					xrtGetError() : NULL
			);
			return false;
		}
	}
	return true;
}



/* 受理成功后一次提交 Writer 的逻辑消息状态。 */
static void __xrtWsWriterCommit(
	xwswriter* pWriter,
	const xutf8state* pUtf8,
	size_t iSize,
	bool bFinal
)
{
	pWriter->Utf8 = *pUtf8;
	pWriter->Size = iSize;
	pWriter->Started = true;
	if ( bFinal ) {
		pWriter->Finished = true;
		__xrtWsWriterUnlock(pWriter);
	}
}



/* 统一复制与所有权分片的事务发送路径。 */
static xnetresult __xrtWsWriterSend(
	xwswriter* pWriter,
	xbytesview Data,
	const xnetref* pRef,
	bool bFinal
)
{
	xwsconn* pConnection;
	xutf8state Utf8;
	size_t iSize;
	xnetresult Result;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return XNET_RESULT_ERROR;
	}
	pConnection = pWriter->Connection;
	if ( !__xrtWsConnWorker(
		pConnection,
		"write-websocket-message"
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( pWriter->Finished || pWriter->DestroyRequested ||
		pWriter->Calling ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"write-websocket-message",
			"WebSocket Writer is not writable",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		return XNET_RESULT_CLOSED;
	}
	if ( !__xrtWsWriterPrepare(
		pWriter,
		Data,
		bFinal,
		&Utf8,
		&iSize
	) ) {
		return XNET_RESULT_ERROR;
	}

	pWriter->Calling = true;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		if ( pWriter->Compressed ) {
			Result = __xrtWsConnSendDeflatePart(
				pConnection,
				pWriter->Started ?
					XWS_OPCODE_CONTINUATION :
					pWriter->Opcode,
				Data,
				!pWriter->Started,
				bFinal
			);
		} else
	#endif
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
		if ( pRef != NULL ) {
			Result = __xrtWsConnSendRefFrame(
				pConnection,
				pWriter->Started ?
					XWS_OPCODE_CONTINUATION :
					pWriter->Opcode,
				*pRef,
				bFinal
			);
		} else
	#endif
	{
		(void)pRef;
		Result = __xrtWsConnSendFrame(
			pConnection,
			pWriter->Started ?
				XWS_OPCODE_CONTINUATION :
				pWriter->Opcode,
			Data,
			bFinal,
			__XWS_SEND_DATA,
			false
		);
	}
	if ( Result == XNET_RESULT_OK ) {
		#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE) && \
			defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
			if ( pWriter->Compressed && (pRef != NULL) ) {
				pRef->Release(
					pRef->Context,
					pRef->Data,
					pRef->Size
				);
			}
		#endif
		__xrtWsWriterCommit(
			pWriter,
			&Utf8,
			iSize,
			bFinal
		);
	}
	pWriter->Calling = false;
	if ( pWriter->DestroyRequested ) {
		__xrtWsWriterDispose(pWriter);
	}
	return Result;
}



/* 复用 Connection 内嵌状态并独占一条普通或压缩出站数据消息。 */
xwswriter* __xrtWsWriterCreate(
	xwsconn* pConnection,
	xwsopcode Opcode,
	bool bCompressed
)
{
	xwswriter* pWriter;

	if ( !__xrtWsConnWorker(
		pConnection,
		"begin-websocket-message"
	) ) {
		return NULL;
	}
	if ( (Opcode != XWS_OPCODE_TEXT) &&
		(Opcode != XWS_OPCODE_BINARY) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"begin-websocket-message",
			"WebSocket Writer opcode must be Text or Binary",
			NULL
		);
		return NULL;
	}
	if ( xrtWsConnState(pConnection) != XWS_CONN_OPEN ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection is not open",
			NULL
		);
		return NULL;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		if ( bCompressed &&
			!pConnection->Config.DeflateEnabled ) {
			(void)__xrtWsConnReject(
				XERR_STATE,
				XWS_CONN_ERROR_CONFIG,
				"begin-compressed-websocket-message",
				"WebSocket connection did not negotiate compression",
				NULL
			);
			return NULL;
		}
	#else
		(void)bCompressed;
	#endif
	if ( pConnection->Writer != NULL ) {
		(void)__xrtWsConnReject(
			XERR_AGAIN,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection already has an active Writer",
			NULL
		);
		return NULL;
	}
	pWriter = &pConnection->WriterStorage;
	memset(pWriter, 0, sizeof(*pWriter));
	pWriter->Connection = xrtWsConnRef(pConnection);
	if ( pWriter->Connection == NULL ) {
		(void)__xrtWsConnReject(
			XERR_CLOSED,
			XWS_CONN_ERROR_STATE,
			"begin-websocket-message",
			"WebSocket connection reference is closed",
			NULL
		);
		return NULL;
	}
	pWriter->Opcode = Opcode;
	#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)
		pWriter->Compressed = bCompressed;
	#endif
	xrtUtf8StateInit(&pWriter->Utf8);
	pConnection->Writer = pWriter;
	return pWriter;
}



/* 创建并独占 Connection 的一条未压缩出站数据消息。 */
XWS_API xwswriter* xrtWsConnBegin(
	xwsconn* pConnection,
	xwsopcode Opcode
)
{
	return __xrtWsWriterCreate(
		pConnection,
		Opcode,
		false
	);
}



/* 开始一条分片 Text 消息。 */
XWS_API xwswriter* xrtWsConnBeginText(
	xwsconn* pConnection
)
{
	return xrtWsConnBegin(
		pConnection,
		XWS_OPCODE_TEXT
	);
}



/* 开始一条分片 Binary 消息。 */
XWS_API xwswriter* xrtWsConnBeginBinary(
	xwsconn* pConnection
)
{
	return xrtWsConnBegin(
		pConnection,
		XWS_OPCODE_BINARY
	);
}



/* 复制并提交一个非最终分片。 */
XWS_API xnetresult xrtWsWriterWrite(
	xwswriter* pWriter,
	xbytesview Data
)
{
	return __xrtWsWriterSend(
		pWriter,
		Data,
		NULL,
		false
	);
}



/* 复制并提交最终分片。 */
XWS_API xnetresult xrtWsWriterFinish(
	xwswriter* pWriter,
	xbytesview Data
)
{
	return __xrtWsWriterSend(
		pWriter,
		Data,
		NULL,
		true
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_REF)
/* 验证 Ref 后提交一个所有权分片。 */
static xnetresult __xrtWsWriterSendRef(
	xwswriter* pWriter,
	const xnetref* pRef,
	bool bFinal
)
{
	xnetref Ref;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		!xrtMemRangeValid(pRef, sizeof(Ref)) ||
		xrtMemRangesOverlap(
			pRef, sizeof(Ref), pWriter, sizeof(*pWriter)
		) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message-ref",
			"WebSocket Writer reference range is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( !xrtMemRangeValid(Ref.Data, Ref.Size) ||
		(Ref.Size == 0) || (Ref.Release == NULL) ) {
		__xrtWsWriterReject(
			pWriter,
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"write-websocket-message-ref",
			"WebSocket Writer reference is incomplete or empty",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtWsWriterSend(
		pWriter,
		(xbytesview) {
			Ref.Data,
			Ref.Size
		},
		&Ref,
		bFinal
	);
}



/* 提交一个非最终所有权分片。 */
XWS_API xnetresult xrtWsWriterWriteRef(
	xwswriter* pWriter,
	const xnetref* pRef
)
{
	return __xrtWsWriterSendRef(
		pWriter,
		pRef,
		false
	);
}



/* 提交最终所有权分片。 */
XWS_API xnetresult xrtWsWriterFinishRef(
	xwswriter* pWriter,
	const xnetref* pRef
)
{
	return __xrtWsWriterSendRef(
		pWriter,
		pRef,
		true
	);
}



/* 提交并接管一个非空 XRT 内存分片。 */
XWS_API xnetresult xrtWsWriterWriteTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsConnTakeRelease,
		NULL
	};

	return xrtWsWriterWriteRef(pWriter, &Ref);
}



/* 提交并接管一个非空 XRT 内存最终分片。 */
XWS_API xnetresult xrtWsWriterFinishTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
)
{
	xnetref Ref = {
		(cbytes)pData,
		iSize,
		__xrtWsConnTakeRelease,
		NULL
	};

	return xrtWsWriterFinishRef(pWriter, &Ref);
}
#endif



/* 返回最终分片是否已经受理。 */
XWS_API bool xrtWsWriterIsFinished(
	const xwswriter* pWriter
)
{
	if ( pWriter == NULL ) {
		return false;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return false;
	}
	return pWriter->Finished;
}



/* 销毁 Writer；发送回调重入销毁时延迟到当前事务结束。 */
XWS_API void xrtWsWriterDestroy(
	xwswriter* pWriter
)
{
	if ( pWriter == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xwsErrorSetInvalidArgument();
		return;
	}
	if ( !__xrtWsConnWorker(
		pWriter->Connection,
		"destroy-websocket-writer"
	) ) {
		return;
	}
	if ( pWriter->DestroyRequested ) {
		return;
	}
	pWriter->DestroyRequested = true;
	if ( !pWriter->Calling ) {
		__xrtWsWriterDispose(pWriter);
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws/src/websocket/writer_deflate.c */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)



#if defined(XWS_FEATURE_WEBSOCKET_WRITER_DEFLATE)

/* 开始一条压缩分片 Text 或 Binary 消息。 */
XWS_API xwswriter* xrtWsConnBeginCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode
)
{
	return __xrtWsWriterCreate(
		pConnection,
		Opcode,
		true
	);
}



/* 开始一条压缩分片 Text 消息。 */
XWS_API xwswriter* xrtWsConnBeginTextCompressed(
	xwsconn* pConnection
)
{
	return xrtWsConnBeginCompressed(
		pConnection,
		XWS_OPCODE_TEXT
	);
}



/* 开始一条压缩分片 Binary 消息。 */
XWS_API xwswriter* xrtWsConnBeginBinaryCompressed(
	xwsconn* pConnection
)
{
	return xrtWsConnBeginCompressed(
		pConnection,
		XWS_OPCODE_BINARY
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xws/src/websocket/connection_future.c */
/* ========================================================================== */

#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE) || \
	defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_FUTURE)

/* 每个异步节点只表达一种发送或观察操作。 */
typedef enum __xrt_ws_async_kind {
	__XRT_WS_ASYNC_MESSAGE = 1,
	__XRT_WS_ASYNC_MESSAGE_COMPRESSED,
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		__XRT_WS_ASYNC_MESSAGE_REF,
	#endif
	__XRT_WS_ASYNC_CONTROL,
	__XRT_WS_ASYNC_CLOSE,
	__XRT_WS_ASYNC_WAIT
} __xrt_ws_async_kind;



/* Worker 从待定状态线性化到唯一 Future 终态。 */
typedef enum __xrt_ws_async_result {
	__XRT_WS_ASYNC_PENDING = 0,
	__XRT_WS_ASYNC_READY,
	__XRT_WS_ASYNC_FAILED,
	__XRT_WS_ASYNC_CANCELLED,
	__XRT_WS_ASYNC_CLOSED
} __xrt_ws_async_result;



/* 标记节点当前所属队列，支持取消回调常数时间迁移。 */
typedef enum __xrt_ws_async_queue {
	__XRT_WS_ASYNC_QUEUE_NONE = 0,
	__XRT_WS_ASYNC_QUEUE_SEND,
	__XRT_WS_ASYNC_QUEUE_WAIT
} __xrt_ws_async_queue;



/* 受阻后依次寻找控制帧和最终 Close，保持协议关闭始终可推进。 */
typedef enum __xrt_ws_async_take {
	__XRT_WS_ASYNC_TAKE_FIFO = 0,
	__XRT_WS_ASYNC_TAKE_CONTROL,
	__XRT_WS_ASYNC_TAKE_CLOSE
} __xrt_ws_async_take;



/*
	节点、Promise 和复制负载采用单一分配。
	节点持有 Connection，直到 Worker 或同步终态路径完成 Promise。
*/
struct __xrt_ws_async {
	__xrt_ws_async* Next;
	__xrt_ws_async* Previous;
	xwsconn* Connection;
	xpromise* Promise;
	xcancelwatch* Watch;
	size_t Size;
	uint16 CloseCode;
	uint8 Kind;
	uint8 Opcode;
	uint8 Wait;
	uint8 Queue;
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		xnetref Ref;
		bool RefOwned;
	#endif
	uint8 Data[];
};



/* 验证公开异步入口收到的完整 Connection 固定结构范围。 */
static bool __xrtWsAsyncConnectionCheck(
	const xwsconn* pConnection,
	cstr sOperation
)
{
	if ( __xrtWsConnRangeValid(pConnection) ) {
		return true;
	}
	(void)__xrtWsConnReject(
		XERR_ARGUMENT,
		XWS_CONN_ERROR_ARGUMENT,
		sOperation,
		"WebSocket connection range is invalid",
		NULL
	);
	return false;
}



/* 返回节点是否可以越过暂时受阻的数据消息使用控制预算。 */
static bool __xrtWsAsyncIsControl(
	const __xrt_ws_async* pAsync
)
{
	return (pAsync->Kind == __XRT_WS_ASYNC_CONTROL) ||
		(pAsync->Kind == __XRT_WS_ASYNC_CLOSE);
}



/* 释放一次尚未挂入节点的原子预算预留。 */
static void __xrtWsAsyncBudgetRelease(
	xwsconn* pConnection,
	size_t iBytes
)
{
	if ( iBytes != 0 ) {
		(void)xrtAtomic64FetchSub(
			&pConnection->AsyncBytes,
			(uint64)iBytes,
			XMEMORY_ACQ_REL
		);
	}
	(void)xrtAtomic32FetchSub(
		&pConnection->AsyncCount,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 为一个待定操作原子保留操作数和负载字节硬预算。 */
static bool __xrtWsAsyncReserve(
	xwsconn* pConnection,
	size_t iBytes
)
{
	uint32 iCount = xrtAtomic32Load(
		&pConnection->AsyncCount,
		XMEMORY_ACQUIRE
	);
	uint64 iQueued;

	if ( iBytes > pConnection->Config.AsyncBytesLimit ) {
		(void)__xrtWsConnReject(
			XERR_RANGE,
			XWS_CONN_ERROR_LIMIT,
			"submit-websocket-async",
			"WebSocket asynchronous payload exceeds its hard byte limit",
			NULL
		);
		return false;
	}
	for ( ;; ) {
		uint32 iExpected = iCount;

		if ( iCount >= pConnection->Config.AsyncCountLimit ) {
			(void)__xrtWsConnReject(
				XERR_AGAIN,
				XWS_CONN_ERROR_LIMIT,
				"submit-websocket-async",
				"WebSocket asynchronous operation queue is full",
				NULL
			);
			return false;
		}
		if ( xrtAtomic32CompareExchange(
			&pConnection->AsyncCount,
			&iExpected,
			iCount + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		iCount = iExpected;
	}
	iQueued = xrtAtomic64Load(
		&pConnection->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	for ( ;; ) {
		uint64 iExpected = iQueued;
		uint64 iLimit =
			(uint64)pConnection->Config.AsyncBytesLimit;

		if ( (iQueued > iLimit) ||
			((uint64)iBytes > (iLimit - iQueued)) ) {
			__xrtWsAsyncBudgetRelease(
				pConnection,
				0
			);
			(void)__xrtWsConnReject(
				XERR_AGAIN,
				XWS_CONN_ERROR_LIMIT,
				"submit-websocket-async",
				"WebSocket asynchronous payload queue is full",
				NULL
			);
			return false;
		}
		if ( xrtAtomic64CompareExchange(
			&pConnection->AsyncBytes,
			&iExpected,
			iQueued + (uint64)iBytes,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			return true;
		}
		iQueued = iExpected;
	}
}



/* 在节点离开全部内部队列后释放其异步硬预算。 */
static void __xrtWsAsyncRelease(__xrt_ws_async* pAsync)
{
	__xrtWsAsyncBudgetRelease(
		pAsync->Connection,
		pAsync->Size
	);
}



/* 为缺少底层原因的异步故障建立稳定 Connection 错误。 */
static xerror* __xrtWsAsyncError(
	const __xrt_ws_async* pAsync,
	cstr sOperation,
	cstr sMessage
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pAsync->Connection->TransportResult,
		XMEMORY_ACQUIRE
	);

	return __xrtWsConnErrorCreate(
		Result == XNET_RESULT_TIMEOUT ?
			XERR_TIMEOUT : XERR_IO,
		Result == XNET_RESULT_TIMEOUT ?
			XWS_CONN_ERROR_TIMEOUT : XWS_CONN_ERROR_SEND,
		sOperation,
		sMessage,
		NULL
	);
}



/* 完成唯一 Future 终态，并在唤醒观察者前释放队列预算。 */
static void __xrtWsAsyncFinish(
	__xrt_ws_async* pAsync,
	__xrt_ws_async_result Result,
	xerror* pError
)
{
	const xerror* pFailure = pError;
	xerror* pCreated = NULL;
	xpromise* pPromise = pAsync->Promise;
	xwsconn* pConnection = pAsync->Connection;

	xrtCancelUnwatch(pAsync->Watch);
	pAsync->Watch = NULL;
	__xrtWsAsyncRelease(pAsync);
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		if ( pAsync->RefOwned ) {
			pAsync->Ref.Release(
				pAsync->Ref.Context,
				pAsync->Ref.Data,
				pAsync->Ref.Size
			);
			pAsync->RefOwned = false;
		}
	#endif
	if ( (Result == __XRT_WS_ASYNC_FAILED) &&
		(pFailure == NULL) ) {
		pCreated = xrtErrorRef(xrtWsConnError(pConnection));
		pFailure = pCreated;
		if ( pFailure == NULL ) {
			pCreated = __xrtWsAsyncError(
				pAsync,
				"complete-websocket-async",
				"WebSocket asynchronous operation failed"
			);
			pFailure = pCreated;
		}
	}
	xrtWsConnDestroy(pConnection);
	xrtFree(pAsync);

	if ( Result == __XRT_WS_ASYNC_READY ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( Result == __XRT_WS_ASYNC_FAILED ) {
		if ( pFailure != NULL ) {
			(void)xrtPromiseReject(
				pPromise,
				pFailure
			);
		} else {
			(void)xrtPromiseClose(pPromise);
		}
	} else if ( Result == __XRT_WS_ASYNC_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseClose(pPromise);
	}

	xrtErrorFree(pCreated);
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 内嵌命令进入 Worker 后允许后续取消再次投递。 */
static void __xrtWsAsyncTask(xnetworker* pWorker, ptr pData)
{
	xwsconn* pConnection = (xwsconn*)pData;

	(void)pWorker;
	xrtSpinLock(&pConnection->AsyncLock);
	pConnection->AsyncPosted = false;
	xrtSpinUnlock(&pConnection->AsyncLock);
	__xrtWsConnFutureNotify(pConnection);
	xrtWsConnDestroy(pConnection);
}



/* 调用方持有 AsyncLock 时无分配地唤醒所属 Worker。 */
static void __xrtWsAsyncSchedule(xwsconn* pConnection)
{
	if ( pConnection->AsyncPosted ) {
		return;
	}
	if ( xrtWsConnRef(pConnection) == NULL ) {
		return;
	}
	pConnection->AsyncPosted = true;
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->AsyncCommand,
		__xrtWsAsyncTask,
		pConnection
	) ) {
		pConnection->AsyncPosted = false;
		xrtWsConnDestroy(pConnection);
	}
}



static void __xrtWsAsyncDetach(
	__xrt_ws_async** ppHead,
	__xrt_ws_async** ppTail,
	__xrt_ws_async* pAsync
);



static void __xrtWsAsyncSendPrepend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
);



/* 取消回调只唤醒 Worker，不从请求取消的线程完成 Promise。 */
static void __xrtWsAsyncCancel(ptr pData)
{
	__xrt_ws_async* pAsync = (__xrt_ws_async*)pData;
	xwsconn* pConnection = pAsync->Connection;

	xrtSpinLock(&pConnection->AsyncLock);
	if ( pAsync->Queue == __XRT_WS_ASYNC_QUEUE_SEND ) {
		__xrtWsAsyncDetach(
			&pConnection->AsyncSendHead,
			&pConnection->AsyncSendTail,
			pAsync
		);
		__xrtWsAsyncSendPrepend(pConnection, pAsync);
		__xrtWsAsyncSchedule(pConnection);
	} else if ( pAsync->Queue == __XRT_WS_ASYNC_QUEUE_WAIT ) {
		__xrtWsAsyncDetach(
			&pConnection->AsyncWaitHead,
			&pConnection->AsyncWaitTail,
			pAsync
		);
		__xrtWsAsyncSendPrepend(pConnection, pAsync);
		__xrtWsAsyncSchedule(pConnection);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
}



/* 从指定双向 FIFO 常数时间摘除节点；调用方持有 AsyncLock。 */
static void __xrtWsAsyncDetach(
	__xrt_ws_async** ppHead,
	__xrt_ws_async** ppTail,
	__xrt_ws_async* pAsync
)
{
	if ( pAsync->Previous != NULL ) {
		pAsync->Previous->Next = pAsync->Next;
	} else {
		*ppHead = pAsync->Next;
	}
	if ( pAsync->Next != NULL ) {
		pAsync->Next->Previous = pAsync->Previous;
	} else {
		*ppTail = pAsync->Previous;
	}
	pAsync->Next = NULL;
	pAsync->Previous = NULL;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_NONE;
}



/* 把发送节点追加到严格 FIFO 队尾；调用方持有 AsyncLock。 */
static void __xrtWsAsyncSendAppend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Previous = pConnection->AsyncSendTail;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_SEND;
	if ( pConnection->AsyncSendTail != NULL ) {
		pConnection->AsyncSendTail->Next = pAsync;
	} else {
		pConnection->AsyncSendHead = pAsync;
	}
	pConnection->AsyncSendTail = pAsync;
}



/* 重试发送必须回到队头，保持已受理异步操作的顺序。 */
static void __xrtWsAsyncSendPrepend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = pConnection->AsyncSendHead;
	pAsync->Previous = NULL;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_SEND;
	if ( pConnection->AsyncSendHead != NULL ) {
		pConnection->AsyncSendHead->Previous = pAsync;
	}
	pConnection->AsyncSendHead = pAsync;
	if ( pConnection->AsyncSendTail == NULL ) {
		pConnection->AsyncSendTail = pAsync;
	}
}



/* 把关闭条件等待追加到独立观察队列。 */
static void __xrtWsAsyncWaitAppend(
	xwsconn* pConnection,
	__xrt_ws_async* pAsync
)
{
	pAsync->Next = NULL;
	pAsync->Previous = pConnection->AsyncWaitTail;
	pAsync->Queue = __XRT_WS_ASYNC_QUEUE_WAIT;
	if ( pConnection->AsyncWaitTail != NULL ) {
		pConnection->AsyncWaitTail->Next = pAsync;
	} else {
		pConnection->AsyncWaitHead = pAsync;
	}
	pConnection->AsyncWaitTail = pAsync;
}



/* 创建 Promise、取消监听和单一分配节点。 */
static __xrt_ws_async* __xrtWsAsyncCreate(
	xwsconn* pConnection,
	__xrt_ws_async_kind Kind,
	size_t iBytes,
	size_t iStorage,
	xfuture** ppFuture
)
{
	__xrt_ws_async* pAsync;
	xwsconn* pOwned;
	xcancel* pCancel;
	xerror* pError;
	size_t iAllocation;

	*ppFuture = NULL;
	if ( iStorage > (SIZE_MAX - sizeof(*pAsync)) ) {
		__xwsErrorSetSizeOverflow();
		return NULL;
	}
	pOwned = xrtWsConnRef(pConnection);
	if ( pOwned == NULL ) {
		(void)__xrtWsConnReject(
			XERR_STATE,
			XWS_CONN_ERROR_STATE,
			"submit-websocket-async",
			"WebSocket connection reference is no longer valid",
			NULL
		);
		return NULL;
	}
	if ( !__xrtWsAsyncReserve(
		pOwned,
		iBytes
	) ) {
		xrtWsConnDestroy(pOwned);
		return NULL;
	}
	iAllocation = sizeof(*pAsync) + iStorage;
	pAsync = (__xrt_ws_async*)xrtCalloc(1, iAllocation);
	if ( pAsync == NULL ) {
		__xrtWsAsyncBudgetRelease(
			pOwned,
			iBytes
		);
		xrtWsConnDestroy(pOwned);
		return NULL;
	}
	pAsync->Connection = pOwned;
	pAsync->Kind = (uint8)Kind;
	pAsync->Size = iBytes;
	pAsync->Promise = xrtPromiseCreate(ppFuture, NULL);
	if ( pAsync->Promise == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(*ppFuture);
		xrtPromiseDestroy(pAsync->Promise);
		__xrtWsAsyncRelease(pAsync);
		xrtWsConnDestroy(pAsync->Connection);
		xrtFree(pAsync);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		*ppFuture = NULL;
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pAsync->Promise);
	if ( pCancel != NULL ) {
		pAsync->Watch = xrtCancelWatch(
			pCancel,
			__xrtWsAsyncCancel,
			pAsync
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pAsync->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(*ppFuture);
		xrtPromiseDestroy(pAsync->Promise);
		__xrtWsAsyncRelease(pAsync);
		xrtWsConnDestroy(pAsync->Connection);
		xrtFree(pAsync);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		*ppFuture = NULL;
		return NULL;
	}
	return pAsync;
}



/* 关闭后的新操作不依赖已经停止的 Worker，直接进入稳定终态。 */
static __xrt_ws_async_result __xrtWsAsyncClosed(
	const xwsconn* pConnection,
	bool bCloseWait
)
{
	xnetresult Result = (xnetresult)xrtAtomic32Load(
		&pConnection->TransportResult,
		XMEMORY_ACQUIRE
	);

	if ( Result == XNET_RESULT_CANCELLED ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( (xrtWsConnError(pConnection) != NULL) ||
		(Result == XNET_RESULT_ERROR) ||
		(Result == XNET_RESULT_TIMEOUT) ) {
		return __XRT_WS_ASYNC_FAILED;
	}
	return bCloseWait ?
		__XRT_WS_ASYNC_READY : __XRT_WS_ASYNC_CLOSED;
}



/* 把完整节点发布给 Worker，关闭竞争由 AsyncLock 内的状态快照裁决。 */
static void __xrtWsAsyncSubmit(__xrt_ws_async* pAsync)
{
	xwsconn* pConnection = pAsync->Connection;
	bool bClosed;

	xrtSpinLock(&pConnection->AsyncLock);
	bClosed = xrtWsConnState(pConnection) == XWS_CONN_CLOSED;
	if ( !bClosed ) {
		if ( (pAsync->Kind == __XRT_WS_ASYNC_WAIT) &&
			(pAsync->Wait == XWS_CONN_WAIT_CLOSE) ) {
			__xrtWsAsyncWaitAppend(pConnection, pAsync);
		} else {
			/* WRITE 与 DRAIN 留在 FIFO 中，形成只覆盖先前发送的屏障。 */
			__xrtWsAsyncSendAppend(pConnection, pAsync);
		}
		__xrtWsAsyncSchedule(pConnection);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	if ( bClosed ) {
		__xrtWsAsyncFinish(
			pAsync,
			__xrtWsAsyncClosed(
				pConnection,
				pAsync->Kind == __XRT_WS_ASYNC_WAIT &&
				pAsync->Wait == XWS_CONN_WAIT_CLOSE
			),
			NULL
		);
	}
}



/* 普通模式取 FIFO 队首；控制模式取第一个可穿透的控制帧。 */
static __xrt_ws_async* __xrtWsAsyncSendTake(
	xwsconn* pConnection,
	__xrt_ws_async_take Take,
	bool* pbCancelled
)
{
	__xrt_ws_async* pAsync;

	*pbCancelled = false;
	xrtSpinLock(&pConnection->AsyncLock);
	pAsync = pConnection->AsyncSendHead;
	if ( (pAsync != NULL) &&
		xrtCancelTriggered(pAsync->Watch) ) {
		*pbCancelled = true;
	} else if ( Take != __XRT_WS_ASYNC_TAKE_FIFO ) {
		for ( pAsync = pConnection->AsyncSendHead;
			pAsync != NULL; pAsync = pAsync->Next ) {
			if ( (Take == __XRT_WS_ASYNC_TAKE_CONTROL) ?
				__xrtWsAsyncIsControl(pAsync) :
				(pAsync->Kind == __XRT_WS_ASYNC_CLOSE) ) {
				break;
			}
		}
	}
	if ( pAsync != NULL ) {
		*pbCancelled = xrtCancelTriggered(pAsync->Watch);
		__xrtWsAsyncDetach(
			&pConnection->AsyncSendHead,
			&pConnection->AsyncSendTail,
			pAsync
		);
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	return pAsync;
}



/* 在 Connection Worker 上执行一次发送尝试。 */
static xnetresult __xrtWsAsyncSend(
	__xrt_ws_async* pAsync
)
{
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
		xnetresult Result;

		if ( pAsync->Kind == __XRT_WS_ASYNC_MESSAGE_REF ) {
			Result = xrtWsConnSendRef(
				pAsync->Connection,
				(xwsopcode)pAsync->Opcode,
				&pAsync->Ref
			);
			if ( Result == XNET_RESULT_OK ) {
				pAsync->RefOwned = false;
			}
			return Result;
		}
	#endif
	xbytesview Payload = {
		pAsync->Data,
		pAsync->Size
	};

	if ( pAsync->Kind == __XRT_WS_ASYNC_MESSAGE ) {
		return xrtWsConnSend(
			pAsync->Connection,
			(xwsopcode)pAsync->Opcode,
			Payload
		);
	}
	#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		if ( pAsync->Kind ==
			__XRT_WS_ASYNC_MESSAGE_COMPRESSED ) {
			return xrtWsConnSendCompressed(
				pAsync->Connection,
				(xwsopcode)pAsync->Opcode,
				Payload
			);
		}
	#endif
	if ( pAsync->Kind == __XRT_WS_ASYNC_CONTROL ) {
		if ( pAsync->Opcode == XWS_OPCODE_PING ) {
			return xrtWsConnPing(
				pAsync->Connection,
				Payload
			);
		}
		return xrtWsConnPong(
			pAsync->Connection,
			Payload
		);
	}
	return xrtWsConnClose(
		pAsync->Connection,
		pAsync->CloseCode,
		(xstrview) {
			(const char*)pAsync->Data,
			pAsync->Size
		}
	);
}



/* 把一次发送返回值转换成 Promise 终态。 */
static __xrt_ws_async_result __xrtWsAsyncSendResult(
	const __xrt_ws_async* pAsync,
	xnetresult Result
)
{
	if ( Result == XNET_RESULT_OK ) {
		return __XRT_WS_ASYNC_READY;
	}
	if ( Result == XNET_RESULT_CANCELLED ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( Result == XNET_RESULT_CLOSED ) {
		return __xrtWsAsyncClosed(
			pAsync->Connection,
			false
		);
	}
	return __XRT_WS_ASYNC_FAILED;
}



/* 评估一个条件等待；全部可变状态只由当前 Worker 推进。 */
static __xrt_ws_async_result __xrtWsAsyncWaitResult(
	const __xrt_ws_async* pAsync
)
{
	const xwsconn* pConnection = pAsync->Connection;
	xwsconnstate State = xrtWsConnState(pConnection);

	if ( xrtCancelTriggered(pAsync->Watch) ) {
		return __XRT_WS_ASYNC_CANCELLED;
	}
	if ( pAsync->Wait == XWS_CONN_WAIT_CLOSE ) {
		return State == XWS_CONN_CLOSED ?
			__xrtWsAsyncClosed(pConnection, true) :
			__XRT_WS_ASYNC_PENDING;
	}
	if ( State == XWS_CONN_CLOSED ) {
		if ( (pAsync->Wait == XWS_CONN_WAIT_DRAIN) &&
			(xrtWsConnError(pConnection) == NULL) &&
			(xrtWsConnPending(pConnection) == 0) ) {
			return __XRT_WS_ASYNC_READY;
		}
		return __xrtWsAsyncClosed(pConnection, false);
	}
	if ( State != XWS_CONN_OPEN ) {
		return __XRT_WS_ASYNC_CLOSED;
	}
	if ( pAsync->Wait == XWS_CONN_WAIT_DRAIN ) {
		return xrtWsConnPending(pConnection) == 0 ?
			__XRT_WS_ASYNC_READY :
			__XRT_WS_ASYNC_PENDING;
	}
	return xrtWsConnWritable(pConnection) != 0 ?
		__XRT_WS_ASYNC_READY :
		__XRT_WS_ASYNC_PENDING;
}



/* 摘除第一个已经满足、取消或终止的条件等待。 */
static __xrt_ws_async* __xrtWsAsyncWaitTake(
	xwsconn* pConnection,
	__xrt_ws_async_result* pResult
)
{
	__xrt_ws_async* pAsync;

	xrtSpinLock(&pConnection->AsyncLock);
	for ( pAsync = pConnection->AsyncWaitHead;
		pAsync != NULL; pAsync = pAsync->Next ) {
		*pResult = __xrtWsAsyncWaitResult(pAsync);
		if ( *pResult != __XRT_WS_ASYNC_PENDING ) {
			__xrtWsAsyncDetach(
				&pConnection->AsyncWaitHead,
				&pConnection->AsyncWaitTail,
				pAsync
			);
			break;
		}
	}
	xrtSpinUnlock(&pConnection->AsyncLock);
	return pAsync;
}



/* 依次推进发送 FIFO，再完成观察 Future；同步传输通知折叠到下一批。 */
void __xrtWsConnFutureNotify(xwsconn* pConnection)
{
	uint32 iCompleted = 0;
	uint32 iBatch = pConnection->Config.AsyncBatch;
	__xrt_ws_async_take Take = __XRT_WS_ASYNC_TAKE_FIFO;

	if ( pConnection->AsyncDriving ) {
		pConnection->AsyncNotified = true;
		return;
	}
	pConnection->AsyncDriving = true;
	while ( iCompleted < iBatch ) {
		__xrt_ws_async* pAsync;
		bool bCancelled;
		xnetresult SendResult;
		__xrt_ws_async_result Result;
		xerror* pError;

		pAsync = __xrtWsAsyncSendTake(
			pConnection,
			Take,
			&bCancelled
		);
		if ( pAsync == NULL ) {
			break;
		}
		if ( bCancelled ) {
			__xrtWsAsyncFinish(
				pAsync,
				__XRT_WS_ASYNC_CANCELLED,
				NULL
			);
			iCompleted++;
			continue;
		}
		if ( pAsync->Kind == __XRT_WS_ASYNC_WAIT ) {
			Result = __xrtWsAsyncWaitResult(pAsync);
			if ( Result == __XRT_WS_ASYNC_PENDING ) {
				xrtSpinLock(&pConnection->AsyncLock);
				if ( xrtCancelTriggered(pAsync->Watch) ) {
					Result = __XRT_WS_ASYNC_CANCELLED;
				} else {
					__xrtWsAsyncSendPrepend(
						pConnection,
						pAsync
					);
				}
				xrtSpinUnlock(&pConnection->AsyncLock);
				if ( Result == __XRT_WS_ASYNC_PENDING ) {
					/* 协议控制帧仍可越过暂时受阻的发送屏障。 */
					Take = __XRT_WS_ASYNC_TAKE_CONTROL;
					continue;
				}
			}
			__xrtWsAsyncFinish(pAsync, Result, NULL);
			iCompleted++;
			Take = __XRT_WS_ASYNC_TAKE_FIFO;
			continue;
		}

		xrtClearError();
		SendResult = __xrtWsAsyncSend(pAsync);
		pError = xrtTakeError();
		if ( SendResult == XNET_RESULT_AGAIN ) {
			xrtErrorFree(pError);
			pError = NULL;
			xrtSpinLock(&pConnection->AsyncLock);
			if ( xrtCancelTriggered(pAsync->Watch) ) {
				Result = __XRT_WS_ASYNC_CANCELLED;
			} else if ( xrtWsConnState(pConnection) !=
				XWS_CONN_OPEN ) {
				Result = __xrtWsAsyncClosed(
					pConnection,
					false
				);
			} else {
				__xrtWsAsyncSendPrepend(
					pConnection,
					pAsync
				);
				Result = __XRT_WS_ASYNC_PENDING;
			}
			xrtSpinUnlock(&pConnection->AsyncLock);
			if ( Result == __XRT_WS_ASYNC_PENDING ) {
				if ( pAsync->Kind != __XRT_WS_ASYNC_CLOSE ) {
					Take = __xrtWsAsyncIsControl(pAsync) ?
						__XRT_WS_ASYNC_TAKE_CLOSE :
						__XRT_WS_ASYNC_TAKE_CONTROL;
					continue;
				}
				break;
			}
			__xrtWsAsyncFinish(pAsync, Result, NULL);
			iCompleted++;
			continue;
		}
		Result = __xrtWsAsyncSendResult(
			pAsync,
			SendResult
		);
		__xrtWsAsyncFinish(pAsync, Result, pError);
		iCompleted++;
		Take = __XRT_WS_ASYNC_TAKE_FIFO;
	}

	while ( iCompleted < iBatch ) {
		__xrt_ws_async_result Result =
			__XRT_WS_ASYNC_PENDING;
		__xrt_ws_async* pAsync =
			__xrtWsAsyncWaitTake(
				pConnection,
				&Result
			);

		if ( pAsync == NULL ) {
			break;
		}
		__xrtWsAsyncFinish(pAsync, Result, NULL);
		iCompleted++;
	}
	pConnection->AsyncDriving = false;
	if ( (iCompleted == iBatch) ||
		pConnection->AsyncNotified ) {
		pConnection->AsyncNotified = false;
		xrtSpinLock(&pConnection->AsyncLock);
		if ( (pConnection->AsyncSendHead != NULL) ||
			(pConnection->AsyncWaitHead != NULL) ) {
			__xrtWsAsyncSchedule(pConnection);
		}
		xrtSpinUnlock(&pConnection->AsyncLock);
	}
}



/* 返回当前异步发送负载字节数。 */
XWS_API size_t xrtWsConnAsyncBytes(
	const xwsconn* pConnection
)
{
	uint64 iBytes;

	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"query-websocket-async-bytes"
	) ) {
		return 0;
	}
	iBytes = xrtAtomic64Load(
		&pConnection->AsyncBytes,
		XMEMORY_ACQUIRE
	);
	return iBytes > (uint64)SIZE_MAX ?
		SIZE_MAX : (size_t)iBytes;
}



/* 返回当前异步发送和等待操作总数。 */
XWS_API uint32 xrtWsConnAsyncCount(
	const xwsconn* pConnection
)
{
	if ( pConnection == NULL ) {
		return 0;
	}
	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"query-websocket-async-count"
	) ) {
		return 0;
	}
	return xrtAtomic32Load(
		&pConnection->AsyncCount,
		XMEMORY_ACQUIRE
	);
}



/* 创建并发布一个条件 Future。 */
XWS_API xfuture* xrtWsConnWaitAsync(
	xwsconn* pConnection,
	xwsconnwait Wait
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"wait-websocket"
	) ) {
		return NULL;
	}
	if ( (Wait < XWS_CONN_WAIT_WRITE) ||
		(Wait > XWS_CONN_WAIT_CLOSE) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"wait-websocket",
			"WebSocket asynchronous wait argument is invalid",
			NULL
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_WAIT,
		0,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Wait = (uint8)Wait;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 预检、复制并发布一个 Text 或 Binary 异步发送。 */
static xfuture* __xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload,
	bool bCompressed
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"send-websocket-async"
	) ) {
		return NULL;
	}
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		bCompressed
	) ) {
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		bCompressed ?
			__XRT_WS_ASYNC_MESSAGE_COMPRESSED :
			__XRT_WS_ASYNC_MESSAGE,
		Payload.Size,
		Payload.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Opcode = (uint8)Opcode;
	if ( Payload.Size != 0 ) {
		memcpy(pAsync->Data, Payload.Data, Payload.Size);
	}
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程复制并异步提交完整消息。 */
XWS_API xfuture* xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendAsync(
		pConnection,
		Opcode,
		Payload,
		false
	);
}



/* 从任意线程复制并异步提交 Text。 */
XWS_API xfuture* xrtWsConnTextAsync(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 从任意线程复制并异步提交 Binary。 */
XWS_API xfuture* xrtWsConnBinaryAsync(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
/* 预检并发布一条所有权异步消息；仅成功返回 Future 时转移调用方引用。 */
XWS_API xfuture* xrtWsConnSendRefAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
)
{
	__xrt_ws_async* pAsync;
	xnetref Ref;
	xbytesview Payload;
	xfuture* pFuture;
	size_t iConnection;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"send-websocket-ref-async"
	) ) {
		return NULL;
	}
	if ( !__xrtWsConnStorageRange(
		pConnection,
		&iConnection
	) || !xrtMemRangeValid(pRef, sizeof(Ref)) ||
		xrtMemRangesOverlap(
			pRef,
			sizeof(Ref),
			pConnection,
			iConnection
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref-async",
			"WebSocket asynchronous reference range is invalid",
			NULL
		);
		return NULL;
	}
	memcpy(&Ref, pRef, sizeof(Ref));
	if ( (Ref.Data == NULL) ||
		(Ref.Size == 0) ||
		(Ref.Release == NULL) ||
		!xrtMemRangeValid(Ref.Data, Ref.Size) ||
		xrtMemRangesOverlap(
			Ref.Data,
			Ref.Size,
			pConnection,
			iConnection
		) ) {
		(void)__xrtWsConnReject(
			XERR_ARGUMENT,
			XWS_CONN_ERROR_ARGUMENT,
			"send-websocket-ref-async",
			"WebSocket asynchronous reference is incomplete or invalid",
			NULL
		);
		return NULL;
	}
	Payload = (xbytesview) {
		Ref.Data,
		Ref.Size
	};
	if ( !__xrtWsConnMessageCheck(
		pConnection,
		Opcode,
		Payload,
		false
	) ) {
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_MESSAGE_REF,
		Ref.Size,
		0,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->Opcode = (uint8)Opcode;
	pAsync->Ref = Ref;
	pAsync->RefOwned = true;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程异步提交所有权 Text。 */
XWS_API xfuture* xrtWsConnTextRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRefAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		pRef
	);
}



/* 从任意线程异步提交所有权 Binary。 */
XWS_API xfuture* xrtWsConnBinaryRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
)
{
	return xrtWsConnSendRefAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		pRef
	);
}
#endif



#if defined(XWS_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 从任意线程复制、压缩并异步提交完整消息。 */
XWS_API xfuture* xrtWsConnSendCompressedAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	return __xrtWsConnSendAsync(
		pConnection,
		Opcode,
		Payload,
		true
	);
}



/* 从任意线程复制、压缩并异步提交 Text。 */
XWS_API xfuture* xrtWsConnTextCompressedAsync(
	xwsconn* pConnection,
	xstrview Text
)
{
	return xrtWsConnSendCompressedAsync(
		pConnection,
		XWS_OPCODE_TEXT,
		(xbytesview) {
			(cbytes)Text.Data,
			Text.Size
		}
	);
}



/* 从任意线程复制、压缩并异步提交 Binary。 */
XWS_API xfuture* xrtWsConnBinaryCompressedAsync(
	xwsconn* pConnection,
	xbytesview Data
)
{
	return xrtWsConnSendCompressedAsync(
		pConnection,
		XWS_OPCODE_BINARY,
		Data
	);
}
#endif



/* 从任意线程复制并异步提交一条 Ping 或 Pong。 */
static xfuture* __xrtWsConnControlAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
)
{
	__xrt_ws_async* pAsync;
	xfuture* pFuture;
	cstr sOperation = Opcode == XWS_OPCODE_PING ?
		"ping-websocket-async" :
		"pong-websocket-async";
	cstr sMessage = Opcode == XWS_OPCODE_PING ?
		"WebSocket asynchronous Ping payload is invalid" :
		"WebSocket asynchronous Pong payload is invalid";

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		sOperation
	) ) {
		return NULL;
	}
	if ( !xrtMemRangeValid(Payload.Data, Payload.Size) ||
		(Payload.Size > XWS_CLOSE_PAYLOAD_MAX) ) {
		(void)__xrtWsConnReject(
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XERR_ARGUMENT : XERR_RANGE,
			!xrtMemRangeValid(Payload.Data, Payload.Size) ?
				XWS_CONN_ERROR_ARGUMENT :
				XWS_CONN_ERROR_LIMIT,
			sOperation,
			sMessage,
			NULL
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_CONTROL,
		Payload.Size,
		Payload.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	if ( Payload.Size != 0 ) {
		memcpy(pAsync->Data, Payload.Data, Payload.Size);
	}
	pAsync->Opcode = (uint8)Opcode;
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}



/* 从任意线程复制并异步提交 Ping。 */
XWS_API xfuture* xrtWsConnPingAsync(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlAsync(
		pConnection,
		XWS_OPCODE_PING,
		Payload
	);
}



/* 从任意线程复制并异步提交 Pong。 */
XWS_API xfuture* xrtWsConnPongAsync(
	xwsconn* pConnection,
	xbytesview Payload
)
{
	return __xrtWsConnControlAsync(
		pConnection,
		XWS_OPCODE_PONG,
		Payload
	);
}



/* 从任意线程预检并异步提交唯一 Close。 */
XWS_API xfuture* xrtWsConnCloseAsync(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
)
{
	uint8 Payload[XWS_CLOSE_PAYLOAD_MAX];
	size_t iPayload = 0;
	__xrt_ws_async* pAsync;
	xfuture* pFuture;

	if ( !__xrtWsAsyncConnectionCheck(
		pConnection,
		"close-websocket-async"
	) ) {
		return NULL;
	}
	if ( !xrtWsCloseWrite(
		iCode,
		Reason,
		Payload,
		sizeof(Payload),
		&iPayload
	) ) {
		(void)__xrtWsConnReject(
			xrtErrorKind(xrtGetError()),
			XWS_CONN_ERROR_MESSAGE,
			"close-websocket-async",
			"WebSocket Close code or reason is invalid",
			xrtGetError()
		);
		return NULL;
	}
	pAsync = __xrtWsAsyncCreate(
		pConnection,
		__XRT_WS_ASYNC_CLOSE,
		Reason.Size,
		Reason.Size,
		&pFuture
	);
	if ( pAsync == NULL ) {
		return NULL;
	}
	pAsync->CloseCode = iCode;
	if ( Reason.Size != 0 ) {
		memcpy(pAsync->Data, Reason.Data, Reason.Size);
	}
	__xrtWsAsyncSubmit(pAsync);
	return pFuture;
}

#endif
#endif

#endif
