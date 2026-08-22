#ifndef XRT_WEBSOCKET_H
#define XRT_WEBSOCKET_H

#include <xrt/error.h>

#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)
	#include <xrt/charset.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)
	#include <xrt/codec.h>
	#include <xrt/crypto.h>
	#include <xrt/http.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_KEYGEN)
	#include <xrt/random.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION)
	#include <xrt/random.h>
	#include <xrt/tcp.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
	#include <xrt/future.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS)
	#include <xrt/tls_stream.h>
#endif

#if defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	defined(XRT_FEATURE_WEBSOCKET_DEFLATER)
	#include <xrt/compress.h>
#endif



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA1))
	#error "XRT WebSocket handshake requires HTTP, Base64 and SHA-1"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_KEYGEN) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT WebSocket key generation requires handshake and secure random"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION) && \
	(!defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE) || \
	 !defined(XRT_FEATURE_HTTP_PARAM))
	#error "XRT WebSocket extensions require handshake and HTTP parameters"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE) && \
	!defined(XRT_FEATURE_WEBSOCKET_EXTENSION)
	#error "XRT WebSocket permessage-deflate requires extensions"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_INFLATER) && \
	(!defined(XRT_FEATURE_WEBSOCKET_DEFLATE) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XRT WebSocket Inflater requires permessage-deflate and Inflate"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER) && \
	(!defined(XRT_FEATURE_WEBSOCKET_DEFLATE) || \
	 !defined(XRT_FEATURE_DEFLATE))
	#error "XRT WebSocket Deflater requires permessage-deflate and Deflate"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CLOSE) && \
	!defined(XRT_FEATURE_UNICODE)
	#error "XRT WebSocket close payloads require Unicode"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_FRAME) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CLOSE))
	#error "XRT WebSocket messages require frames and close payloads"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION) && \
	(!defined(XRT_FEATURE_WEBSOCKET_MESSAGE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_NET_TCP))
	#error "XRT WebSocket connections require messages, secure random and TCP"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF) && \
	!defined(XRT_FEATURE_WEBSOCKET_CONNECTION)
	#error "XRT WebSocket connection references require connection support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_WRITER) && \
	!defined(XRT_FEATURE_WEBSOCKET_CONNECTION)
	#error "XRT WebSocket writers require connection support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_WRITER_REF) && \
	(!defined(XRT_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF))
	#error "XRT WebSocket writer references require writer and connection reference support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT WebSocket TLS connections require WebSocket connections and TLS streams"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_DEFLATER))
	#error "XRT WebSocket compressed connections require connection, Inflater and Deflater"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_WRITER_DEFLATE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_WRITER) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE))
	#error "XRT WebSocket compressed writers require writer and compressed connection support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CONNECTION) || \
	 !defined(XRT_FEATURE_FUTURE))
	#error "XRT WebSocket connection Futures require connection and Future support"
#endif

#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE) && \
	(!defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF) || \
	 !defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE))
	#error "XRT WebSocket reference Futures require reference and Future connection support"
#endif



#if defined(XRT_FEATURE_WEBSOCKET_FRAME)

/* WebSocket 标准数据帧和控制帧操作码。 */
typedef enum xwsopcode {
	XWS_OPCODE_CONTINUATION = 0x0,
	XWS_OPCODE_TEXT = 0x1,
	XWS_OPCODE_BINARY = 0x2,
	XWS_OPCODE_CLOSE = 0x8,
	XWS_OPCODE_PING = 0x9,
	XWS_OPCODE_PONG = 0xA
} xwsopcode;



/* 帧标志使用逻辑位，调用方不需要了解线路字节布局。 */
typedef enum xwsframeflag {
	XWS_FRAME_FIN = UINT32_C(0x00000001),
	XWS_FRAME_MASKED = UINT32_C(0x00000002),
	XWS_FRAME_RSV1 = UINT32_C(0x00000004),
	XWS_FRAME_RSV2 = UINT32_C(0x00000008),
	XWS_FRAME_RSV3 = UINT32_C(0x00000010)
} xwsframeflag;



/* 接收方向使用角色对应的掩码策略，ANY 仅适合协议工具和中间层。 */
typedef enum xwsmaskpolicy {
	XWS_MASK_ANY = 0,
	XWS_MASK_REQUIRED,
	XWS_MASK_FORBIDDEN
} xwsmaskpolicy;



/* 帧头解析只区分协议错误、数据不足和头部就绪。 */
typedef enum xwsframestatus {
	XWS_FRAME_ERROR = -1,
	XWS_FRAME_MORE = 0,
	XWS_FRAME_READY = 1
} xwsframestatus;



/* 帧层错误码覆盖参数、扩展策略和 RFC 6455 线路约束。 */
typedef enum xwsframeerror {
	XWS_FRAME_ERROR_ARGUMENT = 1,
	XWS_FRAME_ERROR_CONFIG,
	XWS_FRAME_ERROR_RSV,
	XWS_FRAME_ERROR_OPCODE,
	XWS_FRAME_ERROR_MASK,
	XWS_FRAME_ERROR_LENGTH,
	XWS_FRAME_ERROR_CONTROL,
	XWS_FRAME_ERROR_CLOSE,
	XWS_FRAME_ERROR_OUTPUT
} xwsframeerror;



/* 标准操作码集合按操作码数值映射到十六位位图。 */
#define XWS_OPCODES_STANDARD UINT16_C(0x0707)



/* WebSocket 固定线路边界。 */
#define XWS_FRAME_HEAD_MAX 14u
#define XWS_MASK_SIZE 4u
#define XWS_FRAME_PAYLOAD_MAX UINT64_C(0x7FFFFFFFFFFFFFFF)



/*
	帧配置不持有资源；AllowedRsv 使用 XWS_FRAME_RSV* 位。
	AllowedOpcodes 的第 n 位表示是否允许操作码 n。
*/
typedef struct xwsframeconfig {
	uint64 MaxPayload;
	uint16 AllowedOpcodes;
	uint16 AllowedRsv;
	xwsmaskpolicy Mask;
} xwsframeconfig;



/* 错误位置从帧头首字节开始计数。 */
typedef struct xwsframeerrorinfo {
	xwsframeerror Code;
	size_t Offset;
} xwsframeerrorinfo;



/*
	帧只描述头部和负载长度，不借用负载，也不要求负载已经到达。
	HeadSize 在解析成功后有效，封包时由模块重新计算。
*/
typedef struct xwsframe {
	uint32 Flags;
	uint8 Opcode;
	uint8 Mask[XWS_MASK_SIZE];
	uint64 PayloadSize;
	size_t HeadSize;
} xwsframe;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)

/* Close 控制帧负载最多包含两字节状态码和 123 字节 UTF-8 原因。 */
#define XWS_CLOSE_PAYLOAD_MAX 125u
#define XWS_CLOSE_REASON_MAX 123u



/*
	1005、1006 和 1015 只表示本地观察结果，不允许写入 Close 帧。
	Code 为零由 xwsclose 专门表示线上负载没有携带状态码。
*/
typedef enum xwsclosecode {
	XWS_CLOSE_NORMAL = 1000,
	XWS_CLOSE_GOING_AWAY = 1001,
	XWS_CLOSE_PROTOCOL = 1002,
	XWS_CLOSE_UNSUPPORTED = 1003,
	XWS_CLOSE_NO_STATUS = 1005,
	XWS_CLOSE_ABNORMAL = 1006,
	XWS_CLOSE_INVALID_DATA = 1007,
	XWS_CLOSE_POLICY = 1008,
	XWS_CLOSE_TOO_BIG = 1009,
	XWS_CLOSE_EXTENSION_REQUIRED = 1010,
	XWS_CLOSE_INTERNAL = 1011,
	XWS_CLOSE_RESTART = 1012,
	XWS_CLOSE_TRY_AGAIN = 1013,
	XWS_CLOSE_BAD_GATEWAY = 1014,
	XWS_CLOSE_TLS = 1015
} xwsclosecode;



/* Close 负载错误区分参数、协议状态码、UTF-8、长度和输出容量。 */
typedef enum xwscloseerror {
	XWS_CLOSE_ERROR_ARGUMENT = 1,
	XWS_CLOSE_ERROR_SIZE,
	XWS_CLOSE_ERROR_CODE,
	XWS_CLOSE_ERROR_UTF8,
	XWS_CLOSE_ERROR_OUTPUT
} xwscloseerror;



/*
	关闭原因直接借用原始负载；Code 为零表示负载为空。
	结构不拥有内存，也不会把本地合成的 1005 写回线路。
*/
typedef struct xwsclose {
	uint16 Code;
	xstrview Reason;
} xwsclose;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE)

/* 消息事件标志同时描述逻辑消息边界、控制帧和扩展变换。 */
typedef enum xwsmessageflag {
	XWS_MESSAGE_BEGIN = UINT32_C(0x00000001),
	XWS_MESSAGE_END = UINT32_C(0x00000002),
	XWS_MESSAGE_CONTROL = UINT32_C(0x00000004),
	XWS_MESSAGE_EXTENDED = UINT32_C(0x00000008),
	XWS_MESSAGE_COMPRESSED = UINT32_C(0x00000010)
} xwsmessageflag;



/* 消息层错误可稳定映射到协议错误、非法数据或消息过大关闭码。 */
typedef enum xwsmessageerror {
	XWS_MESSAGE_ERROR_ARGUMENT = 1,
	XWS_MESSAGE_ERROR_CONFIG,
	XWS_MESSAGE_ERROR_STATE,
	XWS_MESSAGE_ERROR_OPCODE,
	XWS_MESSAGE_ERROR_FRAGMENT,
	XWS_MESSAGE_ERROR_RSV,
	XWS_MESSAGE_ERROR_PAYLOAD,
	XWS_MESSAGE_ERROR_SIZE,
	XWS_MESSAGE_ERROR_UTF8,
	XWS_MESSAGE_ERROR_CLOSE
} xwsmessageerror;



/*
	MaxSize 限制扩展解码后的单条消息字节数；零表示只允许空消息。
	三个 RSV 位图分别描述扩展允许在哪类帧上出现，默认全部禁止。
*/
typedef struct xwsmessageconfig {
	size_t MaxSize;
	uint16 FirstRsv;
	uint16 ContinuationRsv;
	uint16 ControlRsv;
	bool ValidateText;
} xwsmessageconfig;



/* 帧开始时发布的只读语义，不借用帧对象，也不持有负载。 */
typedef struct xwsmessageinfo {
	uint32 Flags;
	uint16 Rsv;
	uint8 Opcode;
	uint8 FrameOpcode;
	uint64 PayloadSize;
	size_t Offset;
} xwsmessageinfo;



/* 可选错误详情给出消息内偏移和应该发送给对端的 Close 状态码。 */
typedef struct xwsmessageerrorinfo {
	xwsmessageerror Code;
	uint16 CloseCode;
	size_t Offset;
} xwsmessageerrorinfo;



/*
	消息状态可放在连接对象内；它只保存有限状态、两个 UTF-8 校验器和
	Close 状态码前缀，不缓存帧负载或完整消息。
*/
typedef struct xwsmessagestate {
	xwsmessageconfig Config;
	xutf8state Utf8;
	xutf8state CloseUtf8;
	size_t Size;
	size_t FrameSize;
	uint64 FramePayloadSize;
	uint32 MessageRsv;
	uint32 FrameRsv;
	uint8 Opcode;
	uint8 FrameOpcode;
	uint8 CloseHead[2];
	uint8 CloseHeadSize;
	bool Fragmented;
	bool FrameActive;
	bool FrameFinal;
	bool Initialized;
	bool Failed;
	bool Closed;
} xwsmessagestate;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* RFC 6455 握手使用版本 13、十六字节随机 nonce 和两个固定 Base64 长度。 */
#define XWS_VERSION 13u
#define XWS_KEY_BYTES 16u
#define XWS_KEY_SIZE 24u
#define XWS_KEY_CAPACITY 25u
#define XWS_ACCEPT_SIZE 28u
#define XWS_ACCEPT_CAPACITY 29u



/* 握手错误码覆盖纯协议工具和后续 HTTP/1.1 Upgrade 层。 */
typedef enum xwshandshakeerror {
	XWS_HANDSHAKE_ERROR_ARGUMENT = 1,
	XWS_HANDSHAKE_ERROR_KEY,
	XWS_HANDSHAKE_ERROR_ACCEPT,
	XWS_HANDSHAKE_ERROR_PROTOCOL,
	XWS_HANDSHAKE_ERROR_EXTENSION,
	XWS_HANDSHAKE_ERROR_METHOD,
	XWS_HANDSHAKE_ERROR_VERSION,
	XWS_HANDSHAKE_ERROR_HOST,
	XWS_HANDSHAKE_ERROR_UPGRADE,
	XWS_HANDSHAKE_ERROR_CONNECTION,
	XWS_HANDSHAKE_ERROR_BODY,
	XWS_HANDSHAKE_ERROR_STATUS,
	XWS_HANDSHAKE_ERROR_FIELD,
	XWS_HANDSHAKE_ERROR_OUTPUT,
	XWS_HANDSHAKE_ERROR_RANDOM
} xwshandshakeerror;

#endif



/* 本地端点角色同时用于协议方向、掩码规则和扩展协商。 */
typedef enum xwsrole {
	XWS_ROLE_CLIENT = 0,
	XWS_ROLE_SERVER
} xwsrole;



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

/*
	扩展名称和参数段都借用 Sec-WebSocket-Extensions 原字段值。
	Parameters 不包含名称后的第一个分号，空视图表示没有参数。
*/
typedef struct xwsextension {
	xstrview Name;
	xstrview Parameters;
} xwsextension;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE)

/* permessage-deflate 的固定名称、窗口范围和最长规范字段项。 */
#define XWS_DEFLATE_NAME "permessage-deflate"
#define XWS_DEFLATE_WINDOW_MIN 8u
#define XWS_DEFLATE_WINDOW_MAX 15u
#define XWS_DEFLATE_MAX_SIZE 128u



/* 标志同时表达参数是否出现，以及 offer 中 client 窗口是否省略值。 */
typedef enum xwsdeflateflag {
	XWS_DEFLATE_SERVER_NO_CONTEXT = UINT32_C(0x00000001),
	XWS_DEFLATE_CLIENT_NO_CONTEXT = UINT32_C(0x00000002),
	XWS_DEFLATE_SERVER_MAX_WINDOW = UINT32_C(0x00000004),
	XWS_DEFLATE_CLIENT_MAX_WINDOW = UINT32_C(0x00000008),
	XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY = UINT32_C(0x00000010)
} xwsdeflateflag;



/* permessage-deflate 错误码区分通用参数、重复项、窗口和协商响应。 */
typedef enum xwsdeflateerror {
	XWS_DEFLATE_ERROR_ARGUMENT = 1,
	XWS_DEFLATE_ERROR_EXTENSION,
	XWS_DEFLATE_ERROR_PARAMETER,
	XWS_DEFLATE_ERROR_DUPLICATE,
	XWS_DEFLATE_ERROR_WINDOW,
	XWS_DEFLATE_ERROR_RESPONSE,
	XWS_DEFLATE_ERROR_OUTPUT,
	XWS_DEFLATE_ERROR_CONFIG,
	XWS_DEFLATE_ERROR_STATE,
	XWS_DEFLATE_ERROR_DATA,
	XWS_DEFLATE_ERROR_LIMIT,
	XWS_DEFLATE_ERROR_CODEC
} xwsdeflateerror;



/*
	配置不持有资源；Flags 表达参数是否存在。
	窗口参数未出现，或 offer 的 client 窗口省略值时，对应字段保持 15。
*/
typedef struct xwsdeflate {
	uint32 Flags;
	uint8 ServerMaxWindowBits;
	uint8 ClientMaxWindowBits;
} xwsdeflate;



/* 单向运行参数不持有资源，也不混淆客户端与服务端参数名。 */
typedef struct xwsdeflatedirection {
	uint8 WindowBits;
	bool NoContextTakeover;
} xwsdeflatedirection;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER) || \
	defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

/* WebSocket 压缩变换的输出视图只在同步回调期间有效。 */
typedef bool (*xwsoutputproc)(xbytesview Data, ptr pData);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER)

#define XWS_INFLATE_OUTPUT_DEFAULT UINT64_C(67108864)



/*
	OutputLimit 是每条逻辑消息的解码后上限。
	Retain 只在禁用上下文接管时决定是否保留已复位的算法对象。
*/
typedef struct xwsinflaterconfig {
	uint64 OutputLimit;
	uint8 WindowBits;
	bool NoContextTakeover;
	bool Retain;
} xwsinflaterconfig;



/* 接收变换对象按需创建底层 Inflate，不缓存线路或解码后消息。 */
typedef struct xwsinflater xwsinflater;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

/*
	OutputLimit 是每条逻辑消息实际交付的线路负载上限；中间 Flush 尾部计入，
	最终 End 尾部会被剥离。
	Retain 只在禁用上下文接管时决定是否保留已复位的算法对象。
*/
typedef struct xwsdeflaterconfig {
	uint64 OutputLimit;
	int32 Level;
	xdeflatestrategy Strategy;
	uint8 WindowBits;
	bool NoContextTakeover;
	bool Retain;
} xwsdeflaterconfig;



/* 发送变换对象按需创建底层 Deflate，只额外暂存四字节同步尾部。 */
typedef struct xwsdeflater xwsdeflater;

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION)

#define XWS_CONN_MESSAGE_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_FRAME_LIMIT_DEFAULT UINT64_C(1048576)
#define XWS_CONN_SEND_LIMIT_DEFAULT ((size_t)1048576u)
#define XWS_CONN_CONTROL_RESERVE_DEFAULT ((size_t)512u)
#define XWS_CONN_CLOSE_TIMEOUT_DEFAULT UINT64_C(5000000)
#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
	#define XWS_CONN_ASYNC_BYTES_DEFAULT ((size_t)1048576u)
	#define XWS_CONN_ASYNC_COUNT_DEFAULT UINT32_C(1024)
	#define XWS_CONN_ASYNC_BATCH_DEFAULT UINT32_C(64)
#endif



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



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
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
	uint64 CloseTimeout;
	bool AutoPong;
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
		size_t AsyncBytesLimit;
		uint32 AsyncCountLimit;
		uint32 AsyncBatch;
	#endif
	#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
		xwsdeflate Deflate;
		xwsinflaterconfig Inflater;
		xwsdeflaterconfig Deflater;
		bool DeflateEnabled;
	#endif
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

#if defined(XRT_FEATURE_WEBSOCKET_WRITER)
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



#if defined(XRT_FEATURE_WEBSOCKET_FRAME)

/*
	初始化严格标准操作码、无扩展、任意掩码方向和协议最大负载配置。
	Config 必须是完整可写范围，可以未对齐；无效范围只设置线程错误。
*/
XRT_API void xrtWsFrameConfigInit(xwsframeconfig* pConfig);



/*
	初始化一个空的 continuation 帧描述。
	Frame 必须是完整可写范围，可以未对齐；无效范围只设置线程错误。
*/
XRT_API void xrtWsFrameInit(xwsframe* pFrame);



/*
	增量解析最多十四字节帧头；READY 不表示负载已经到达。
	Config 为空时使用默认配置，MORE 不设置线程错误。
	输入、Frame、可选 Config 和 Error 都必须是完整范围；结构可以未对齐。
	两个输出必须彼此分离，且不能覆盖输入或配置；参数范围错误不修改 Frame。
*/
XRT_API xwsframestatus xrtWsFrameParse(
	xbytesview Input,
	xwsframe* pFrame,
	const xwsframeconfig* pConfig,
	xwsframeerrorinfo* pError
);



/*
	规范封包帧头；输出为空且容量为零时只查询长度。
	容量不足不会写入半个帧头，并通过 Size 返回所需容量。
	结构和 Size 可以未对齐；输出范围及 Size 必须彼此分离且不能覆盖 Frame 或配置。
	除容量不足以外的失败不修改输出或 Size。
*/
XRT_API bool xrtWsFrameWrite(
	const xwsframe* pFrame,
	const xwsframeconfig* pConfig,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	从消息内绝对偏移开始原地应用或移除掩码。
	分片调用只要连续传入正确偏移，结果就与一次处理完全相同。
	数据和四字节 Mask 必须是完整范围；Mask 可以位于数据内，函数会先快照密钥。
*/
XRT_API bool xrtWsMask(
	void* pData,
	size_t iSize,
	const uint8 pMask[XWS_MASK_SIZE],
	uint64 iOffset
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CLOSE)

/* 纯判断状态码当前是否允许出现在 RFC 6455 Close 控制帧中。 */
XRT_API bool xrtWsCloseCodeValid(uint16 iCode);



/*
	解析完整 Close 负载并借用其中的原因文本；空负载返回 Code == 0。
	一字节负载、禁用状态码、超长负载和非法 UTF-8 都会失败且不修改输出。
	负载和 Close 必须是完整且分离的范围；Close 可以未对齐。
*/
XRT_API bool xrtWsCloseParse(
	xbytesview Payload,
	xwsclose* pClose
);



/*
	写出完整 Close 负载；Code 和 Reason 同时为空时写出空负载。
	空输出可查询长度，容量不足或任何失败都不会修改输出。
	Reason、整个输出容量和 Size 必须是完整范围，Size 可以未对齐且不能覆盖其它范围。
	输出可以覆盖 Reason；除容量不足发布所需长度外，其它失败不修改 Size。
*/
XRT_API bool xrtWsCloseWrite(
	uint16 iCode,
	xstrview Reason,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_MESSAGE)

/*
	初始化默认消息上限、严格文本校验和不允许任何 RSV 位的配置。
	Config 必须是完整可写范围，可以未对齐；无效范围只设置线程错误。
*/
XRT_API void xrtWsMessageConfigInit(xwsmessageconfig* pConfig);



/*
	绑定配置并初始化一个可复用、无堆资源的消息状态。
	State 和可选 Config 必须是完整且分离的范围，可以未对齐；失败不修改 State。
*/
XRT_API bool xrtWsMessageInit(
	xwsmessagestate* pState,
	const xwsmessageconfig* pConfig
);



/*
	保留配置并清除当前连接的分片、UTF-8、Close 和失败状态。
	State 必须是完整范围，可以未对齐；无效或未初始化状态保持不变。
*/
XRT_API void xrtWsMessageReset(xwsmessagestate* pState);



/*
	开始处理一个已经通过帧层校验的帧，并返回它所属的逻辑消息语义。
	控制帧可以穿插在分片消息中；错误不会发布部分 Info。
	State、Frame、Info 和可选 Error 必须是完整且彼此分离的范围，结构可以未对齐。
	协议错误只把 State 标记为 Failed；参数或调用状态错误不改变 State。
*/
XRT_API bool xrtWsMessageFrameBegin(
	xwsmessagestate* pState,
	const xwsframe* pFrame,
	xwsmessageinfo* pInfo,
	xwsmessageerrorinfo* pError
);



/*
	提交扩展解码后的语义负载分块；未使用扩展时就是原始解掩码负载。
	函数增量执行消息上限、文本 UTF-8 和 Close 原因校验。
	State、负载和可选 Error 必须是完整且彼此分离的范围；Error 可以未对齐。
	成功一次提交状态，协议数据错误只提交 Failed，参数和调用状态错误保持 State。
*/
XRT_API bool xrtWsMessagePayload(
	xwsmessagestate* pState,
	xbytesview Payload,
	xwsmessageerrorinfo* pError
);



/*
	结束当前帧；无扩展时会核对负载字节数，消息末尾会完成 UTF-8 校验。
	成功处理 Close 帧后状态拒绝继续接收其它帧，直到 Reset。
	State 和可选 Error 必须是完整且分离的范围，可以未对齐。
	成功一次提交状态，协议数据错误只提交 Failed，调用状态错误保持 State。
*/
XRT_API bool xrtWsMessageFrameEnd(
	xwsmessagestate* pState,
	xwsmessageerrorinfo* pError
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_HANDSHAKE)

/* 验证去除两端 OWS 后的值是规范编码的十六字节 WebSocket nonce。 */
XRT_API bool xrtWsKeyValid(xstrview Key);



/*
	计算末尾补零的 Sec-WebSocket-Accept；输出至少需要 XWS_ACCEPT_CAPACITY 字节。
	Key 和输出必须是完整且不发生地址回绕的范围；输出可以和 Key 重叠，
	任何失败都不会修改输出。
*/
XRT_API bool xrtWsAccept(
	xstrview Key,
	char* sAccept,
	size_t iCapacity
);



/* 以固定工作量比较预期值和去除两端 OWS 后的 Sec-WebSocket-Accept。 */
XRT_API bool xrtWsAcceptValid(
	xstrview Key,
	xstrview Accept
);



/*
	迭代逗号分隔的子协议 token；Offset 初值为零。
	返回 ITEM 时 Protocol 借用原文本，END 表示列表结束，ERROR 表示语法错误。
	两个输出都可以未对齐，但必须是彼此分离且不覆盖输入的完整可写范围；
	失败时不修改任一输出。
*/
XRT_API xhttpnext xrtWsProtocolNext(
	xstrview Protocols,
	size_t* pOffset,
	xstrview* pProtocol
);



/* 验证完整子协议列表的语法与名称唯一性；空列表表示没有提供子协议。 */
XRT_API bool xrtWsProtocolsValid(xstrview Protocols);



/* 验证完整列表后，按大小写敏感规则判断其中是否包含指定子协议。 */
XRT_API bool xrtWsProtocolsHas(
	xstrview Protocols,
	xstrview Protocol
);



/*
	在完整验证两份列表后按客户端偏好顺序选择首个服务端支持项。
	没有交集仍返回 true，并把 Selected 设置为空视图。
	Selected 可以未对齐，但必须是与两份输入分离的完整可写范围；失败时不修改。
*/
XRT_API bool xrtWsProtocolSelect(
	xstrview ClientProtocols,
	xstrview ServerProtocols,
	xstrview* pSelected
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_EXTENSION)

/*
	迭代 Sec-WebSocket-Extensions 的下一项；Offset 初值为零。
	逗号产生的空成员按 HTTP #rule 忽略，但非空字段必须至少含一个扩展。
	借用字段和两个输出都必须是完整地址范围；输出允许未对齐但不得相互重叠，
	也不得覆盖借用字段。ERROR 不推进 Offset，也不修改 Extension。
*/
XRT_API xhttpnext xrtWsExtensionNext(
	xstrview Extensions,
	size_t* pOffset,
	xwsextension* pExtension
);



/*
	严格统计完整扩展列表；空视图表示字段未出现并成功返回零。
	Count 允许未对齐但不得覆盖字段值；失败不修改 Count。
*/
XRT_API bool xrtWsExtensionCount(
	xstrview Extensions,
	size_t* pCount
);



/*
	迭代扩展的参数段；Offset 初值为零。
	quoted-string 参数会额外验证解转义后的值仍然是 token。
	输入结构和借用字段必须完整；两个输出允许未对齐，但不得覆盖输入或彼此。
	ERROR 不推进 Offset，也不修改 Param。
*/
XRT_API xhttpnext xrtWsExtensionParamNext(
	const xwsextension* pExtension,
	size_t* pOffset,
	xhttpparam* pParam
);



/*
	写出一个扩展项；Parameters 是不含首个分号的已序列化参数段。
	借用字段、输出容量和 Size 都必须是完整地址范围，Size 允许未对齐。
	空输出可查询精确长度；容量不足只更新所需长度，任何失败都不写部分结果。
*/
XRT_API bool xrtWsExtensionWrite(
	xstrview Name,
	xstrview Parameters,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATE)

/* 初始化为不带参数、两个方向都使用默认十五位窗口的配置；输出允许未对齐。 */
XRT_API void xrtWsDeflateInit(xwsdeflate* pConfig);



/* 纯判断完整扩展描述符的名称是否是大小写不敏感的 permessage-deflate。 */
XRT_API bool xrtWsDeflateIs(const xwsextension* pExtension);



/*
	严格解析一个 permessage-deflate offer；输入输出允许未对齐且不得重叠，
	借用字段必须是完整地址范围，失败不修改 Offer。
*/
XRT_API bool xrtWsDeflateOfferParse(
	const xwsextension* pExtension,
	xwsdeflate* pOffer
);



/*
	严格解析一个 permessage-deflate response；输入输出允许未对齐且不得重叠，
	借用字段必须是完整地址范围，失败不修改 Response。
*/
XRT_API bool xrtWsDeflateResponseParse(
	const xwsextension* pExtension,
	xwsdeflate* pResponse
);



/*
	从 offer 构造最小合规响应，只确认客户端对服务端方向提出的强制约束。
	函数不判断具体压缩后端是否支持该窗口，调用方可继续调整并执行 Check。
	固定结构允许未对齐和精确原地转换，其他重叠会被拒绝；失败不修改 Response。
*/
XRT_API bool xrtWsDeflateAccept(
	const xwsdeflate* pOffer,
	xwsdeflate* pResponse
);



/* 检查完整且可未对齐的响应是否能作为给定 offer 的 RFC 7692 协商结果。 */
XRT_API bool xrtWsDeflateResponseCheck(
	const xwsdeflate* pOffer,
	const xwsdeflate* pResponse
);



/*
	规范写出一个完整 permessage-deflate offer，不附加零字符。
	固定输入和 Size 允许未对齐；容量不足只更新所需长度，不写部分结果。
*/
XRT_API bool xrtWsDeflateOfferWrite(
	const xwsdeflate* pOffer,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	规范写出一个完整 permessage-deflate response，不附加零字符。
	固定输入和 Size 允许未对齐；容量不足只更新所需长度，不写部分结果。
*/
XRT_API bool xrtWsDeflateResponseWrite(
	const xwsdeflate* pResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	把已经协商并校验的 response 映射为本地发送或接收方向。
	固定输入输出允许未对齐但不得重叠；失败不修改 Direction。
	bSend 为 true 表示本地发送方向。
*/
XRT_API bool xrtWsDeflateDirection(
	const xwsdeflate* pResponse,
	xwsrole Role,
	bool bSend,
	xwsdeflatedirection* pDirection
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_INFLATER)

/*
	初始化 RFC 默认窗口、上下文接管、64 MiB 消息上限和不保留空闲算法对象。
	输出结构允许未对齐，但必须是完整地址范围。
*/
XRT_API void xrtWsInflaterConfigInit(
	xwsinflaterconfig* pConfig
);



/*
	把协商方向应用到现有配置，并保留输出上限与 Retain 策略。
	固定结构允许未对齐但不得重叠；失败不修改配置。
*/
XRT_API bool xrtWsInflaterConfigApply(
	xwsinflaterconfig* pConfig,
	const xwsdeflatedirection* pDirection
);



/* 创建惰性接收变换；配置为空时使用默认值，非空配置会在分配前完整快照。 */
XRT_API xwsinflater* xrtWsInflaterCreate(
	const xwsinflaterconfig* pConfig
);



/*
	复位到新连接并保留已经分配的 Inflate 存储；活动且未失败的消息会被拒绝。
	非空配置允许未对齐并在修改对象前完成快照；失败保持既有配置和状态。
*/
XRT_API bool xrtWsInflaterReset(
	xwsinflater* pInflater,
	const xwsinflaterconfig* pConfig
);



/* 开始一条压缩或直通消息；控制帧不应进入该状态机。 */
XRT_API bool xrtWsInflaterBegin(
	xwsinflater* pInflater,
	bool bCompressed
);



/*
	同步提交任意完整线路分块，并发布当前能够产生的语义负载。
	输入不得覆盖 Inflater；参数错误不终止活动消息，数据或回调错误进入失败态。
*/
XRT_API bool xrtWsInflaterWrite(
	xwsinflater* pInflater,
	xbytesview Input,
	xwsoutputproc pOutput,
	ptr pData
);



/* 结束消息；压缩消息会补入 RFC 7692 同步尾部并按策略复位上下文。 */
XRT_API bool xrtWsInflaterEnd(
	xwsinflater* pInflater,
	xwsoutputproc pOutput,
	ptr pData
);



/* 返回当前或上一条消息已经成功交付的语义字节数；无效对象范围返回零。 */
XRT_API uint64 xrtWsInflaterSize(
	const xwsinflater* pInflater
);



/* 销毁接收变换；空指针为空操作，无效范围和输出回调内销毁会被拒绝。 */
XRT_API void xrtWsInflaterDestroy(
	xwsinflater* pInflater
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_DEFLATER)

/*
	初始化 RFC 默认窗口、上下文接管、级别 6 和不保留空闲算法对象。
	输出结构允许未对齐，但必须是完整地址范围。
*/
XRT_API void xrtWsDeflaterConfigInit(
	xwsdeflaterconfig* pConfig
);



/*
	把协商方向应用到现有配置，并保留级别、策略、上限与 Retain。
	固定结构允许未对齐但不得重叠；失败不修改配置。
*/
XRT_API bool xrtWsDeflaterConfigApply(
	xwsdeflaterconfig* pConfig,
	const xwsdeflatedirection* pDirection
);



/* 创建惰性发送变换；配置为空时使用默认值，非空配置会在分配前完整快照。 */
XRT_API xwsdeflater* xrtWsDeflaterCreate(
	const xwsdeflaterconfig* pConfig
);



/*
	复位到新连接并保留已经分配的 Deflate 存储；活动且未失败的消息会被拒绝。
	非空配置允许未对齐并在修改对象前完成快照；失败保持既有配置和状态。
*/
XRT_API bool xrtWsDeflaterReset(
	xwsdeflater* pDeflater,
	const xwsdeflaterconfig* pConfig
);



/* 开始一条压缩或直通消息；调用方据此决定首帧是否设置 RSV1。 */
XRT_API bool xrtWsDeflaterBegin(
	xwsdeflater* pDeflater,
	bool bCompressed
);



/*
	同步提交任意完整语义分块，并发布当前能够产生的线路负载。
	输入不得覆盖 Deflater；参数错误不终止活动消息，编码或回调错误进入失败态。
*/
XRT_API bool xrtWsDeflaterWrite(
	xwsdeflater* pDeflater,
	xbytesview Input,
	xwsoutputproc pOutput,
	ptr pData
);



/*
	建立可继续写入的同步边界，并发布包含四字节同步尾部的全部线路负载。
	该接口用于把一条压缩消息安全地切分为多个 WebSocket 线路帧。
*/
XRT_API bool xrtWsDeflaterFlush(
	xwsdeflater* pDeflater,
	xwsoutputproc pOutput,
	ptr pData
);



/*
	放弃当前消息或刚结束但尚未被外部受理的发送事务，并复位编码器。
	没有活动消息时也可以用它主动丢弃上下文历史。
	该操作可能丢弃可选的发送上下文历史，但保证下一条消息仍可独立解码。
*/
XRT_API bool xrtWsDeflaterAbort(
	xwsdeflater* pDeflater
);



/* 结束消息，验证并去除四字节同步尾部，再按策略复位上下文。 */
XRT_API bool xrtWsDeflaterEnd(
	xwsdeflater* pDeflater,
	xwsoutputproc pOutput,
	ptr pData
);



/*
	返回一次 Write 后紧接 Flush 或 End 可能产生的线路负载硬上界。
	结果适合在推进压缩状态前执行内存与背压预算；输出允许未对齐，
	溢出和其他失败不修改 OutputSize。
*/
XRT_API bool xrtWsDeflaterBound(
	size_t iInputSize,
	size_t* pOutputSize
);



/* 返回当前或上一条消息已经成功交付的线路字节数；无效对象范围返回零。 */
XRT_API uint64 xrtWsDeflaterSize(
	const xwsdeflater* pDeflater
);



/* 销毁发送变换；空指针为空操作，无效范围和输出回调内销毁会被拒绝。 */
XRT_API void xrtWsDeflaterDestroy(
	xwsdeflater* pDeflater
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_KEYGEN)

/*
	使用操作系统安全随机源生成末尾补零的 Sec-WebSocket-Key。
	输出至少需要 XWS_KEY_CAPACITY 字节，任何失败都不会暴露部分密钥。
*/
XRT_API bool xrtWsKeyGenerate(
	char* sKey,
	size_t iCapacity
);

#endif



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION)

/*
	初始化服务端角色、1 MiB 收发上限、512 字节控制预算和五秒关闭超时。
	pConfig 可以指向完整但未对齐的可写结构存储；失败时不修改该存储。
*/
XRT_API void xrtWsConnConfigInit(xwsconnconfig* pConfig);



/*
	无分配验证完整 Connection 配置；不会修改传入结构。
	pConfig 及其 Protocol 视图都必须是完整有效范围，结构本身可以未对齐。
*/
XRT_API bool xrtWsConnConfigValid(
	const xwsconnconfig* pConfig
);



/*
	在 TCP Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 Stream 引用，并在下一次 Worker 循环处理
	已经缓冲的早到帧；失败时 Stream 所有权保持不变。
	TCP WriteLimit 必须同时容纳 ControlReserve 和至少一个空数据帧。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XRT_API xwsconn* xrtWsConnAttach(
	xnetstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS)
/*
	在 TLS Stream 所属 Worker 上接管已经开放的 Stream。
	成功后 Connection 接管调用方 TLS Stream 引用，明文余量不会丢失。
	ControlReserve 和 SendLimit 必须容纳协商后 TLS 记录开销。
	Config、Events 和 Protocol 内容在调用期间复制，调用返回后不再借用。
	两个结构都可以位于完整但未对齐的只读存储。
*/
XRT_API xwsconn* xrtWsConnAttachTls(
	xtlsstream* pStream,
	const xwsconnconfig* pConfig,
	const xwsconnevents* pEvents,
	ptr pData
);
#endif



/* 增加 Connection 引用并返回原指针。 */
XRT_API xwsconn* xrtWsConnRef(xwsconn* pConnection);



/* 释放 Connection 引用；关闭或中止传输必须另行请求。 */
XRT_API void xrtWsConnDestroy(xwsconn* pConnection);



/* 返回并发可读的会话状态。 */
XRT_API xwsconnstate xrtWsConnState(const xwsconn* pConnection);



/* 返回建立连接时固定的本端角色。 */
XRT_API xwsrole xrtWsConnRole(const xwsconn* pConnection);



/* 返回 Connection 拥有的已协商子协议快照。 */
XRT_API xstrview xrtWsConnProtocol(
	const xwsconn* pConnection
);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	复制协商后的 permessage-deflate 响应。
	返回 false 表示连接未启用压缩或参数无效。
	pDeflate 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出。
*/
XRT_API bool xrtWsConnDeflate(
	const xwsconn* pConnection,
	xwsdeflate* pDeflate
);
#endif



/* 返回传输所属的借用 Worker。 */
XRT_API xnetworker* xrtWsConnWorker(const xwsconn* pConnection);



/* 在所属 Worker 上借用 TCP Stream；TLS 会话返回空指针。 */
XRT_API xnetstream* xrtWsConnTcp(const xwsconn* pConnection);



/* 从任意线程取得 TCP Stream 强引用；调用方最终 Destroy。 */
XRT_API xnetstream* xrtWsConnTcpRef(const xwsconn* pConnection);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_TLS)
/* 在所属 Worker 上借用 TLS Stream；TCP 会话返回空指针。 */
XRT_API xtlsstream* xrtWsConnTls(const xwsconn* pConnection);



/* 从任意线程取得 TLS Stream 强引用；调用方最终 Destroy。 */
XRT_API xtlsstream* xrtWsConnTlsRef(const xwsconn* pConnection);
#endif



/* 返回 WebSocket 与底层传输当前待发字节的并发快照。 */
XRT_API size_t xrtWsConnPending(const xwsconn* pConnection);



/* 返回普通数据当前仍可受理的硬预算快照。 */
XRT_API size_t xrtWsConnWritable(const xwsconn* pConnection);



/*
	暂停应用消息读取。
	可以从任意线程调用；当前回调分块完成后不再发布后续消息事件。
*/
XRT_API void xrtWsConnPause(xwsconn* pConnection);



/*
	恢复应用消息读取并投递一次所属 Worker 驱动。
	已关闭连接返回 false；关闭握手中的连接允许恢复协议读取。
*/
XRT_API bool xrtWsConnResume(xwsconn* pConnection);



/* 返回接收侧当前是否被应用暂停的并发快照。 */
XRT_API bool xrtWsConnPaused(const xwsconn* pConnection);



/*
	在所属 Worker 上复制发送一条完整 Text 或 Binary 消息。
	Text 在受理前完成 UTF-8 校验；当前预算不足返回 AGAIN，
	单帧永久超过 WebSocket 或 TCP 硬容量返回 ERROR 和 XERR_RANGE。
	Payload 必须是完整且不发生地址回绕的只读范围，返回前已经完成复制。
*/
XRT_API xnetresult xrtWsConnSend(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnText(
	xwsconn* pConnection,
	xstrview Text
);



/* 发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinary(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF)
/*
	在所属 Worker 上发送一条所有权消息。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但结构和数据必须是完整范围，且都不能覆盖 Connection。
	空负载不接管所有权，应改用普通 Send；失败不会调用 Release。
*/
XRT_API xnetresult xrtWsConnSendRef(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 发送一条所有权 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnTextRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 发送一条所有权 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinaryRef(
	xwsconn* pConnection,
	const xnetref* pRef
);



/*
	在所属 Worker 上发送并接管一段 xrtMalloc 内存。
	仅返回 OK 时由 Connection 最终 xrtFree；空负载应改用普通 Send。
*/
XRT_API xnetresult xrtWsConnSendTake(
	xwsconn* pConnection,
	xwsopcode Opcode,
	ptr pData,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc UTF-8 Text。 */
XRT_API xnetresult xrtWsConnTextTake(
	xwsconn* pConnection,
	str sText,
	size_t iSize
);



/* 发送并接管一段 xrtMalloc Binary。 */
XRT_API xnetresult xrtWsConnBinaryTake(
	xwsconn* pConnection,
	bytes pData,
	size_t iSize
);
#endif



#if defined(XRT_FEATURE_WEBSOCKET_WRITER)
/*
	在所属 Worker 上开始一条未压缩的分片 Text 或 Binary 消息。
	同一 Connection 同时只能存在一个 Writer；失败返回空指针并设置错误。
*/
XRT_API xwswriter* xrtWsConnBegin(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条未压缩的分片 UTF-8 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginText(
	xwsconn* pConnection
);



/* 开始一条未压缩的分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinary(
	xwsconn* pConnection
);



#if defined(XRT_FEATURE_WEBSOCKET_WRITER_DEFLATE)
/*
	在所属 Worker 上开始一条压缩分片 Text 或 Binary 消息。
	每次 Write 都独立建立同步边界，因此成功返回后对应线路帧已被传输层受理。
	AGAIN、永久容量和 OOM 不推进 Writer；半条消息后的编码或传输故障会终止会话。
*/
XRT_API xwswriter* xrtWsConnBeginCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode
);



/* 开始一条压缩分片 UTF-8 Text 消息。 */
XRT_API xwswriter* xrtWsConnBeginTextCompressed(
	xwsconn* pConnection
);



/* 开始一条压缩分片 Binary 消息。 */
XRT_API xwswriter* xrtWsConnBeginBinaryCompressed(
	xwsconn* pConnection
);
#endif



/*
	复制并提交一个非最终分片。
	仅返回 OK 时推进消息大小、UTF-8 和首帧状态；AGAIN 可原样重试。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XRT_API xnetresult xrtWsWriterWrite(
	xwswriter* pWriter,
	xbytesview Data
);



/*
	复制并提交最终分片；空分片合法。
	成功后释放 Connection 的 Writer 独占权，但对象仍需 Destroy。
	Writer 和 Data 必须是完整且分离的范围；参数错误不推进 Writer。
*/
XRT_API xnetresult xrtWsWriterFinish(
	xwswriter* pWriter,
	xbytesview Data
);



#if defined(XRT_FEATURE_WEBSOCKET_WRITER_REF)
/*
	提交一个非空所有权分片。
	仅返回 OK 时接管 Ref；Release 可能在返回前执行，也可能在线路发送后执行。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XRT_API xnetresult xrtWsWriterWriteRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/*
	提交最终所有权分片；失败时 Ref 所有权保持不变。
	Ref 可以未对齐，但其结构和数据都必须是完整范围，并且 Ref 不能覆盖 Writer。
*/
XRT_API xnetresult xrtWsWriterFinishRef(
	xwswriter* pWriter,
	const xnetref* pRef
);



/* 提交并接管一个非空 xrtMalloc 分片。 */
XRT_API xnetresult xrtWsWriterWriteTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);



/* 提交并接管一个非空 xrtMalloc 最终分片。 */
XRT_API xnetresult xrtWsWriterFinishTake(
	xwswriter* pWriter,
	ptr pData,
	size_t iSize
);
#endif



/* 返回 Writer 是否已经成功提交最终分片；空指针返回 false，回绕范围设置参数错误。 */
XRT_API bool xrtWsWriterIsFinished(
	const xwswriter* pWriter
);



/*
	在所属 Worker 上销毁 Writer。
	放弃已经开始但未结束的消息会尝试发送 1011 Close，失败则中止传输。
	空指针无操作；回绕对象范围只设置参数错误。
*/
XRT_API void xrtWsWriterDestroy(
	xwswriter* pWriter
);
#endif



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/*
	压缩并发送一条完整 Text 或 Binary 消息。
	失败不会让未发送的压缩上下文影响下一条消息。
*/
XRT_API xnetresult xrtWsConnSendCompressed(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 压缩并发送一条完整 UTF-8 Text 消息。 */
XRT_API xnetresult xrtWsConnTextCompressed(
	xwsconn* pConnection,
	xstrview Text
);



/* 压缩并发送一条完整 Binary 消息。 */
XRT_API xnetresult xrtWsConnBinaryCompressed(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/*
	发送 Ping 控制帧；Payload 最多 125 字节。
	控制帧可以使用普通数据不可占用的预留预算。
*/
XRT_API xnetresult xrtWsConnPing(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Pong 控制帧；通常在 AutoPong 关闭后的 Ping 回调中使用。
	Payload 最多 125 字节，并使用控制帧预留预算。
*/
XRT_API xnetresult xrtWsConnPong(
	xwsconn* pConnection,
	xbytesview Payload
);



/*
	发送 Close 并进入关闭握手；Code 为零且 Reason 为空时发送空负载。
	重复调用返回 CLOSED，不会发送第二个 Close。
*/
XRT_API xnetresult xrtWsConnClose(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);



/*
	从任意线程请求立即异常关闭底层传输。
	OPEN 只会前进到 CLOSING；已经 CLOSED 时返回 false 且终态保持不变。
*/
XRT_API bool xrtWsConnAbort(xwsconn* pConnection);



/*
	一次性复制当前 Close 快照；Reason 仍借用 Connection 内部存储。
	pClose 可以未对齐，但必须是与 Connection 所有存储分离的完整可写范围；
	失败时不修改输出，成功后除 Reason 视图外不再借用 Connection 状态。
*/
XRT_API bool xrtWsConnCloseInfo(
	const xwsconn* pConnection,
	xwsconnclose* pClose
);



/* 返回 Connection 第一个结构化错误；结果借用到 Connection 销毁。 */
XRT_API const xerror* xrtWsConnError(const xwsconn* pConnection);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_FUTURE)
/* 返回尚未由 Connection Worker 终结的异步发送负载字节数。 */
XRT_API size_t xrtWsConnAsyncBytes(const xwsconn* pConnection);



/* 返回异步发送和条件等待的合计操作数。 */
XRT_API uint32 xrtWsConnAsyncCount(const xwsconn* pConnection);



/*
	建立 WRITE、DRAIN 或 CLOSE 条件 Future。
	WRITE 与 DRAIN 是发送 FIFO 屏障，只等待调用前已经排队的发送；
	WRITE 随后要求当前可写，DRAIN 随后要求传输待发归零。
	取消只移除本次等待，不会关闭 Connection。
*/
XRT_API xfuture* xrtWsConnWaitAsync(
	xwsconn* pConnection,
	xwsconnwait Wait
);



/*
	从任意线程复制并按 FIFO 提交一条完整消息。
	Future 在帧被传输层受理时完成；排空必须另行等待 DRAIN。
	取消只在 Worker 受理前有效，不会撤回已经提交的线路帧。
*/
XRT_API xfuture* xrtWsConnSendAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryAsync(
	xwsconn* pConnection,
	xbytesview Data
);



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_REF_FUTURE)
/*
	从任意线程异步提交一条所有权消息。
	成功返回 Future 时立即接管 Ref；终态、取消和关闭都恰好调用一次 Release。
	空负载不接管所有权，应改用普通 Async 发送。
	Ref 可以未对齐，但结构与数据都必须是完整、不回绕且与 Connection 分离的范围；
	任何预检失败都不接管所有权，也不会调用 Release。
*/
XRT_API xfuture* xrtWsConnSendRefAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);



/* 从任意线程异步提交一条所有权 Binary。 */
XRT_API xfuture* xrtWsConnBinaryRefAsync(
	xwsconn* pConnection,
	const xnetref* pRef
);
#endif



#if defined(XRT_FEATURE_WEBSOCKET_CONNECTION_DEFLATE)
/* 从任意线程复制、压缩并异步提交一条完整消息。 */
XRT_API xfuture* xrtWsConnSendCompressedAsync(
	xwsconn* pConnection,
	xwsopcode Opcode,
	xbytesview Payload
);



/* 从任意线程复制、压缩并异步提交 UTF-8 Text。 */
XRT_API xfuture* xrtWsConnTextCompressedAsync(
	xwsconn* pConnection,
	xstrview Text
);



/* 从任意线程复制、压缩并异步提交 Binary。 */
XRT_API xfuture* xrtWsConnBinaryCompressedAsync(
	xwsconn* pConnection,
	xbytesview Data
);
#endif



/* 从任意线程复制并异步提交 Ping。 */
XRT_API xfuture* xrtWsConnPingAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程复制并异步提交 Pong。 */
XRT_API xfuture* xrtWsConnPongAsync(
	xwsconn* pConnection,
	xbytesview Payload
);



/* 从任意线程异步提交唯一 Close。 */
XRT_API xfuture* xrtWsConnCloseAsync(
	xwsconn* pConnection,
	uint16 iCode,
	xstrview Reason
);
#endif

#endif



XRT_EXTERN_C_END

#endif
