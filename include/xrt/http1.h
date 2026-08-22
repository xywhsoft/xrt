#ifndef XRT_HTTP1_H
#define XRT_HTTP1_H

#include <xrt/error.h>
#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP1_HEAD) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_UPGRADE))
	#error "XRT HTTP/1 head support requires HTTP and HTTP Upgrade"
#endif

#if defined(XRT_FEATURE_HTTP1_BODY) && \
	(!defined(XRT_FEATURE_HTTP1_HEAD) || \
	 !defined(XRT_FEATURE_HTTP_TRAILER))
	#error "XRT HTTP/1 body support requires HTTP/1 head and HTTP Trailer support"
#endif

#if defined(XRT_FEATURE_HTTP1_MESSAGE) && !defined(XRT_FEATURE_HTTP1_BODY)
	#error "XRT HTTP/1 message support requires XRT_FEATURE_HTTP1_BODY"
#endif



#if defined(XRT_FEATURE_HTTP1_HEAD)

/* HTTP/1 解析返回值区分数据不足、字段描述符不足和真正的协议错误。 */
typedef enum xhttp1status {
	XHTTP1_ERROR = -1,
	XHTTP1_MORE = 0,
	XHTTP1_READY = 1,
	XHTTP1_FIELDS = 2
} xhttp1status;



/* 起始行决定消息方向，调用方不需要依赖启发式自动识别。 */
typedef enum xhttpkind {
	XHTTP_REQUEST = 1,
	XHTTP_RESPONSE
} xhttpkind;



/* Header 只描述线上的显式语义，完整消息体计划由上层结合请求方法计算。 */
typedef enum xhttp1flag {
	XHTTP1_KEEP_ALIVE = UINT32_C(0x00000001),
	XHTTP1_CONNECTION_CLOSE = UINT32_C(0x00000002),
	XHTTP1_UPGRADE = UINT32_C(0x00000004),
	XHTTP1_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP1_CHUNKED = UINT32_C(0x00000010),
	XHTTP1_TRANSFER_ENCODING = UINT32_C(0x00000020),
	XHTTP1_TRANSFER_OTHER = UINT32_C(0x00000040)
} xhttp1flag;



/* HTTP/1 解析与封包使用稳定错误码，Offset 和 Line 提供精确协议位置。 */
typedef enum xhttp1error {
	XHTTP1_ERROR_ARGUMENT = 1,
	XHTTP1_ERROR_HEAD_INCOMPLETE,
	XHTTP1_ERROR_HEAD_TOO_LARGE,
	XHTTP1_ERROR_START_LINE_TOO_LARGE,
	XHTTP1_ERROR_FIELD_LINE_TOO_LARGE,
	XHTTP1_ERROR_TOO_MANY_FIELDS,
	XHTTP1_ERROR_LINE_END,
	XHTTP1_ERROR_START_LINE,
	XHTTP1_ERROR_METHOD,
	XHTTP1_ERROR_TARGET,
	XHTTP1_ERROR_VERSION,
	XHTTP1_ERROR_STATUS,
	XHTTP1_ERROR_REASON,
	XHTTP1_ERROR_FIELD_NAME,
	XHTTP1_ERROR_FIELD_VALUE,
	XHTTP1_ERROR_CONTENT_LENGTH,
	XHTTP1_ERROR_CONFLICTING_CONTENT_LENGTH,
	XHTTP1_ERROR_TRANSFER_LENGTH,
	XHTTP1_ERROR_TRANSFER_ENCODING,
	XHTTP1_ERROR_UNSUPPORTED_TRANSFER_ENCODING,
	XHTTP1_ERROR_CONNECTION,
	XHTTP1_ERROR_OUTPUT_SIZE,
	XHTTP1_ERROR_REQUEST_TRANSFER_ENCODING,
	XHTTP1_ERROR_BODY_TOO_LARGE,
	XHTTP1_ERROR_BODY_INCOMPLETE,
	XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE,
	XHTTP1_ERROR_CHUNK_SIZE,
	XHTTP1_ERROR_CHUNK_EXTENSION,
	XHTTP1_ERROR_CHUNK_TERMINATOR,
	XHTTP1_ERROR_TRAILER_TOO_LARGE,
	XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE,
	XHTTP1_ERROR_TOO_MANY_TRAILERS,
	XHTTP1_ERROR_FORBIDDEN_TRAILER,
	XHTTP1_ERROR_UPGRADE
} xhttp1error;



/* 默认限额面向公网协议输入，调用方可以按服务端路由或客户端策略收紧。 */
typedef struct xhttp1limits {
	size_t MaxHead;
	size_t MaxStartLine;
	size_t MaxFieldLine;
	size_t MaxFields;
} xhttp1limits;



/* 解析错误位置从消息首字节开始计数，Line 从一开始计数。 */
typedef struct xhttp1errorinfo {
	xhttp1error Code;
	size_t Offset;
	size_t Line;
} xhttp1errorinfo;



/* Transfer Coding 名称和原样参数都借用字段值，Parameters 不含首个分号。 */
typedef struct xhttp1transfercoding {
	xstrview Name;
	xstrview Parameters;
} xhttp1transfercoding;



/*
	Head 只借用输入和字段数组；输入与数组必须覆盖 Head 的使用期。
	FIELDS 状态下 FieldCount 是需要的描述符数量，其余字段已经可读取。
*/
typedef struct xhttp1head {
	xhttpkind Kind;
	xhttpversion Version;
	uint32 Flags;
	uint16 Status;
	uint64 ContentLength;
	size_t Bytes;
	xstrview Method;
	xstrview Target;
	xstrview Reason;
	xhttpfield* Fields;
	size_t FieldCount;
	size_t FieldCapacity;
} xhttp1head;

#endif



#if defined(XRT_FEATURE_HTTP1_BODY)

/* Body Plan 明确区分无正文、定长、分块、关闭定界和升级后的非 HTTP 字节。 */
typedef enum xhttp1bodymode {
	XHTTP1_BODY_NONE = 0,
	XHTTP1_BODY_FIXED,
	XHTTP1_BODY_CHUNKED,
	XHTTP1_BODY_CLOSE,
	XHTTP1_BODY_TUNNEL
} xhttp1bodymode;



/* Body Reader 每次只发布一个借用数据片段或一个终态。 */
typedef enum xhttp1bodystatus {
	XHTTP1_BODY_ERROR = -1,
	XHTTP1_BODY_MORE = 0,
	XHTTP1_BODY_DATA = 1,
	XHTTP1_BODY_DONE = 2,
	XHTTP1_BODY_FIELDS = 3
} xhttp1bodystatus;



/* Body Plan 是 Header 事实结合请求方法和响应状态后的唯一分帧结论。 */
typedef struct xhttp1bodyplan {
	xhttp1bodymode Mode;
	uint64 Length;
} xhttp1bodyplan;



/* 流式正文不预分配内存；限额约束累计正文、chunk 行和 trailer 区。 */
typedef struct xhttp1bodylimits {
	uint64 MaxBody;
	size_t MaxChunkLine;
	size_t MaxTrailer;
	size_t MaxTrailerLine;
	size_t MaxTrailers;
} xhttp1bodylimits;



/*
	Body Reader 由调用方持有且不分配内存；Trailers 借用完成调用中的输入。
	公开计数可用于进度与诊断，其余状态只能由本模块推进。
*/
typedef struct xhttp1body {
	xhttp1bodymode Mode;
	uint64 Remaining;
	uint64 Received;
	uint64 WireBytes;
	xhttpfield* Trailers;
	size_t TrailerCount;
	size_t TrailerCapacity;
	xhttp1bodylimits Limits;
	uint64 ChunkSize;
	size_t ChunkLineBytes;
	uint32 State;
} xhttp1body;

#endif



#if defined(XRT_FEATURE_HTTP1_MESSAGE)

/*
	完整消息便利层借用连续输入、Header 和 trailer 描述符，不持有堆内存。
	Wire 只覆盖第一条完整消息，BodyBytes 是移除 chunked 分帧后的正文长度。
*/
typedef struct xhttp1message {
	xhttp1head Head;
	xhttp1bodyplan Plan;
	xhttp1bodylimits Limits;
	xbytesview Wire;
	xhttpfield* Trailers;
	size_t TrailerCount;
	size_t TrailerCapacity;
	size_t BodyBytes;
} xhttp1message;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP1_HEAD)

/* 验证非空 request-target 不含空白、控制字符或 fragment。 */
XRT_API bool xrtHttp1TargetValid(xstrview Target);



/*
	初始化适合公网输入的 8 KiB 起始行、64 KiB Header 和 100 字段限额。
	限额描述符允许位于未对齐存储，解析器会先复制其值再开始修改 Head。
*/
XRT_API void xrtHttp1LimitsInit(xhttp1limits* pLimits);



/* 初始化借用调用方字段数组的空 Head。 */
XRT_API void xrtHttp1HeadInit(
	xhttp1head* pHead,
	xhttpfield* pFields,
	size_t iCapacity
);



/* 严格增量解析 HTTP/1.0 或 HTTP/1.1 请求 Header。 */
XRT_API xhttp1status xrtHttp1RequestParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);



/* 严格增量解析 HTTP/1.0 或 HTTP/1.1 响应 Header。 */
XRT_API xhttp1status xrtHttp1ResponseParse(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);



/* 严格迭代一个 Transfer-Encoding 字段值，Offset 初始为零。 */
XRT_API xhttpnext xrtHttp1TransferCodingNext(
	xstrview Value,
	size_t* pOffset,
	xhttp1transfercoding* pCoding
);



/* 返回第一个同名 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttp1Field(
	const xhttp1head* pHead,
	xstrview Name
);



/*
	校验并写入完整请求 Header，不自动添加 Host 或其他策略字段。
	输出为空且容量为零时只查询所需字节数；容量不足不会写入半个报文。
	字段描述符和 Size 输出允许未对齐存储；全部输入与输出范围必须有效且互不重叠。
*/
XRT_API bool xrtHttp1RequestWrite(
	xstrview Method,
	xstrview Target,
	xhttpversion Version,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	校验并写入完整响应 Header，不自动添加 Content-Length 或连接策略。
	Reason 允许为空；输出查询与失败原子性和请求封包一致。
	字段描述符和 Size 输出允许未对齐存储；全部输入与输出范围必须有效且互不重叠。
*/
XRT_API bool xrtHttp1ResponseWrite(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP1_BODY)

/*
	初始化无正文预设上限的流式 Body 限额；上层服务可以按路由继续收紧。
	限额描述符允许位于未对齐存储，Body Reader 初始化时会复制其值。
*/
XRT_API void xrtHttp1BodyLimitsInit(xhttp1bodylimits* pLimits);



/* 按 RFC 9112 请求分帧优先级生成 Body Plan。 */
XRT_API bool xrtHttp1RequestBodyPlan(
	const xhttp1head* pHead,
	xhttp1bodyplan* pPlan
);



/* 按请求方法与响应状态生成 Body Plan；HEAD 和 CONNECT 必须传入原请求方法。 */
XRT_API bool xrtHttp1ResponseBodyPlan(
	const xhttp1head* pHead,
	xstrview RequestMethod,
	xhttp1bodyplan* pPlan
);



/* 初始化无分配 Body Reader；trailer 描述符可以为空并在 FIELDS 后重新绑定。 */
XRT_API bool xrtHttp1BodyInit(
	xhttp1body* pBody,
	const xhttp1bodyplan* pPlan,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity,
	const xhttp1bodylimits* pLimits
);



/* 在 FIELDS 状态后替换 trailer 描述符存储，不重置正文解码进度。 */
XRT_API bool xrtHttp1BodyTrailers(
	xhttp1body* pBody,
	xhttpfield* pTrailers,
	size_t iCapacity
);



/* 严格解析从第一行开始并由空行结束的 trailer 区，所有字段均借用 Input。 */
XRT_API xhttp1status xrtHttp1TrailersParse(
	xbytesview Input,
	xhttpfield* pFields,
	size_t iCapacity,
	const xhttp1bodylimits* pLimits,
	size_t* pBytes,
	size_t* pCount,
	xhttp1errorinfo* pError
);



/*
	写入十六进制 chunk-size 行；Extensions 为空或为以分号起始的完整扩展后缀。
	该函数只写 size 行，调用方可用向量发送依次发送 size 行、原正文和 CRLF，正文不会复制。
*/
XRT_API bool xrtHttp1ChunkLineWrite(
	uint64 iSize,
	xstrview Extensions,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 把一段非空正文封装为完整 chunk；空正文是成功的空操作，不会结束消息。 */
XRT_API bool xrtHttp1ChunkWrite(
	xbytesview Data,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写入 last-chunk、可选 trailer 和最终空行；调用方必须确认字段定义明确允许 trailer。 */
XRT_API bool xrtHttp1ChunkEndWrite(
	const xhttpfield* pTrailers,
	size_t iTrailerCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	推进正文状态机。Consumed 是本次可从输入移除的线缆字节，Data 仅在 DATA 时有效。
	bEnd 表示可靠传输已正常结束；定长或分块正文过早结束会返回协议错误。
*/
XRT_API xhttp1bodystatus xrtHttp1BodyRead(
	xhttp1body* pBody,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xbytesview* pData,
	xhttp1errorinfo* pError
);



/* 判断 Reader 是否已经完整消费 HTTP 正文与 trailer。 */
XRT_API bool xrtHttp1BodyDone(const xhttp1body* pBody);

#endif



#if defined(XRT_FEATURE_HTTP1_MESSAGE)

/* 初始化借用调用方 Header 与 trailer 描述符数组的空完整消息。 */
XRT_API void xrtHttp1MessageInit(
	xhttp1message* pMessage,
	xhttpfield* pFields,
	size_t iFieldCapacity,
	xhttpfield* pTrailers,
	size_t iTrailerCapacity
);



/* 扫描第一条完整请求；bEnd 表示可靠 EOF，并拒绝被截断的 Header 或正文。 */
XRT_API xhttp1status xrtHttp1RequestMessageParse(
	xbytesview Input,
	bool bEnd,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
);



/* 扫描第一条完整响应；RequestMethod 用于 HEAD、CONNECT 和普通响应分帧。 */
XRT_API xhttp1status xrtHttp1ResponseMessageParse(
	xbytesview Input,
	bool bEnd,
	xstrview RequestMethod,
	xhttp1message* pMessage,
	const xhttp1limits* pHeadLimits,
	const xhttp1bodylimits* pBodyLimits,
	xhttp1errorinfo* pError
);



/* 返回无需移除 chunked 分帧时的借用正文；chunked 或空正文返回空视图。 */
XRT_API xbytesview xrtHttp1MessageBodyView(const xhttp1message* pMessage);



/* 把正文复制到连续输出并移除 chunked 分帧；空输出可精确查询所需长度。 */
XRT_API bool xrtHttp1MessageBodyCopy(
	const xhttp1message* pMessage,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
