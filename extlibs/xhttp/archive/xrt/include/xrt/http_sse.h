#ifndef XRT_HTTP_SSE_H
#define XRT_HTTP_SSE_H

#include <xrt/http.h>

#if defined(XRT_FEATURE_HTTP_SSE)
	#include <xrt/memory.h>
#endif

#if defined(XRT_FEATURE_HTTP_SSE) || \
	defined(XRT_FEATURE_HTTP_SSE_PARSER)
	#include <xrt/charset.h>
#endif

#if defined(XRT_FEATURE_HTTP_SSE_PARSER)
	#include <xrt/buffer.h>
#endif

#if defined(XRT_FEATURE_HTTP_SSE_HTTP)
	#include <xrt/mime.h>
#endif



#if defined(XRT_FEATURE_HTTP_SSE) && \
	(!defined(XRT_FEATURE_HTTP) || !defined(XRT_FEATURE_UNICODE))
	#error "XRT HTTP SSE requires HTTP and Unicode support"
#endif

#if defined(XRT_FEATURE_HTTP_SSE_PARSER) && \
	(!defined(XRT_FEATURE_HTTP_SSE) || !defined(XRT_FEATURE_BUFFER))
	#error "XRT HTTP SSE parser requires HTTP SSE and Buffer support"
#endif

#if defined(XRT_FEATURE_HTTP_SSE_HTTP) && \
	(!defined(XRT_FEATURE_HTTP_SSE) || \
	 !defined(XRT_FEATURE_HTTP_HEADERS) || \
	 !defined(XRT_FEATURE_MIME))
	#error "XRT HTTP SSE adapter requires HTTP SSE, Header and MIME support"
#endif



#if defined(XRT_FEATURE_HTTP_SSE)

/* EventSource 唯一标准媒体类型和浏览器常用初始重连时间。 */
#define XHTTP_SSE_MEDIA_TYPE "text/event-stream"
#define XHTTP_SSE_RETRY_DEFAULT UINT64_C(3000)



/* 字段标志区分省略字段与显式空字段；空 data 仍会发布一条空消息。 */
typedef enum xhttpsseeventflag {
	XHTTP_SSE_EVENT_NONE = 0,
	XHTTP_SSE_EVENT_DATA = UINT32_C(0x00000001),
	XHTTP_SSE_EVENT_TYPE = UINT32_C(0x00000002),
	XHTTP_SSE_EVENT_ID = UINT32_C(0x00000004),
	XHTTP_SSE_EVENT_RETRY = UINT32_C(0x00000008)
} xhttpsseeventflag;



/*
	待封包事件借用 UTF-8 字段；Data 可以包含 LF，但不能包含 CR。
	固定描述符可存放在完整但未对齐的存储中。
*/
typedef struct xhttpsseevent {
	xstrview Type;
	xstrview Data;
	xstrview Id;
	uint64 Retry;
	uint32 Flags;
} xhttpsseevent;

#endif



#if defined(XRT_FEATURE_HTTP_SSE_HTTP)

/* HTTP 响应分类区分可重连拒绝、建立流和服务端要求停止重连。 */
typedef enum xhttpsseresponse {
	XHTTP_SSE_RESPONSE_ERROR = -1,
	XHTTP_SSE_RESPONSE_REJECT = 0,
	XHTTP_SSE_RESPONSE_OPEN = 1,
	XHTTP_SSE_RESPONSE_STOP = 2
} xhttpsseresponse;

#endif



#if defined(XRT_FEATURE_HTTP_SSE_PARSER)

/* 增量解析每次最多发布一个项目，调用方可以自然地施加背压。 */
typedef enum xhttpsseparsestatus {
	XHTTP_SSE_PARSE_ERROR = -1,
	XHTTP_SSE_PARSE_MORE = 0,
	XHTTP_SSE_PARSE_ITEM = 1,
	XHTTP_SSE_PARSE_DONE = 2
} xhttpsseparsestatus;



/* 项目区分应用消息、可选注释心跳和有效 retry 更新。 */
typedef enum xhttpsseitemkind {
	XHTTP_SSE_ITEM_EVENT = 1,
	XHTTP_SSE_ITEM_COMMENT,
	XHTTP_SSE_ITEM_RETRY
} xhttpsseitemkind;



/* 解析错误保留稳定分类；精确字节与行位置由错误详情给出。 */
typedef enum xhttpsseerror {
	XHTTP_SSE_ERROR_ARGUMENT = 1,
	XHTTP_SSE_ERROR_STATE,
	XHTTP_SSE_ERROR_UTF8,
	XHTTP_SSE_ERROR_LINE_TOO_LARGE,
	XHTTP_SSE_ERROR_DATA_TOO_LARGE,
	XHTTP_SSE_ERROR_TYPE_TOO_LARGE,
	XHTTP_SSE_ERROR_ID_TOO_LARGE,
	XHTTP_SSE_ERROR_ALLOCATION
} xhttpsseerror;



/*
	限额全部按解码后的 UTF-8 字节计算，零值表示禁止对应非空内容。
	配置是可快照固定值，可以存放在完整但未对齐的存储中。
*/
typedef struct xhttpsseparserconfig {
	size_t LineLimit;
	size_t DataLimit;
	size_t TypeLimit;
	size_t IdLimit;
	uint64 Retry;
	xutfpolicy Utf8Policy;
	bool EmitComments;
	bool EmitRetry;
} xhttpsseparserconfig;



/* 应用消息借用 Parser 缓冲，Type 为空时已经映射为静态 message。 */
typedef struct xhttpssemessage {
	xstrview Type;
	xstrview Data;
	xstrview LastEventId;
	uint64 Retry;
} xhttpssemessage;



/* ITEM 返回值按 Kind 使用 Message、Comment 或 Retry。 */
typedef struct xhttpsseitem {
	xhttpsseitemkind Kind;
	xhttpssemessage Message;
	xstrview Comment;
	uint64 Retry;
} xhttpsseitem;



/* 错误位置使用整个当前响应正文内的零基字节偏移和一基行号。 */
typedef struct xhttpsseerrorinfo {
	xhttpsseerror Code;
	size_t Offset;
	size_t Line;
} xhttpsseerrorinfo;



/*
	Parser 不预分配固定接收区，五个缓冲只按实际峰值增长。
	公开字段用于调用方栈分配，内部状态不能由调用方修改。
	Parser 持有动态缓冲，结构本体必须自然对齐。
*/
typedef struct xhttpsseparser {
	xhttpsseparserconfig Config;
	xbuffer Line;
	xbuffer Decoded;
	xbuffer Data;
	xbuffer Type;
	xbuffer Id;
	uint64 Retry;
	size_t Offset;
	size_t LineOffset;
	size_t LineNumber;
	uint32 State;
	uint32 Flags;
} xhttpsseparser;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SSE)

/* 判断 Event 的视图、标志、UTF-8 和单行字段约束是否有效。 */
XRT_API bool xrtHttpSseEventValid(const xhttpsseevent* pEvent);



/* 精确计算规范 LF 封包长度；Size 可以未对齐，失败不修改。 */
XRT_API bool xrtHttpSseEventSize(
	const xhttpsseevent* pEvent,
	size_t* pSize
);



/*
	写出一个完整事件块，不附加零字符。
	空输出只查询长度，容量不足或输入重叠时不写入部分结果。
	Size 可以使用完整但未对齐的存储。
*/
XRT_API bool xrtHttpSseEventWrite(
	const xhttpsseevent* pEvent,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾事件块，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpSseEventBuild(
	const xhttpsseevent* pEvent,
	size_t* pSize
);



/* 精确计算规范注释心跳长度；Comment 可以包含 LF，但不能包含 CR。 */
XRT_API bool xrtHttpSseCommentSize(
	xstrview Comment,
	size_t* pSize
);



/* 写出一条或多条注释行，不添加事件分隔空行。 */
XRT_API bool xrtHttpSseCommentWrite(
	xstrview Comment,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾注释心跳，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpSseCommentBuild(
	xstrview Comment,
	size_t* pSize
);



/* 判断 Last-Event-ID 是否是可安全放入请求 Header 的 UTF-8 值。 */
XRT_API bool xrtHttpSseLastEventIdValid(xstrview Id);

#endif



#if defined(XRT_FEATURE_HTTP_SSE_HTTP)

/* 判断 Content-Type 是否是参数合法的 text/event-stream。 */
XRT_API bool xrtHttpSseContentTypeValid(xstrview ContentType);



/* 事务设置 Accept，并按空值删除或唯一设置 Last-Event-ID。 */
XRT_API bool xrtHttpSseRequestHeaders(
	xhttpheaders* pHeaders,
	xstrview LastEventId
);



/* 事务唯一设置响应 Content-Type，不添加应用策略字段。 */
XRT_API bool xrtHttpSseResponseHeaders(xhttpheaders* pHeaders);



/* 按 EventSource 规则分类状态码和唯一 Content-Type；协议拒绝不修改线程错误。 */
XRT_API xhttpsseresponse xrtHttpSseResponseCheck(
	uint16 iStatus,
	const xhttpheaders* pHeaders
);

#endif



#if defined(XRT_FEATURE_HTTP_SSE_PARSER)

/* 初始化现代浏览器兼容的动态限额、替换解码和三秒重连配置。 */
XRT_API void xrtHttpSseParserConfigInit(
	xhttpsseparserconfig* pConfig
);



/* 纯判断解析配置是否可以安全建立状态机。 */
XRT_API bool xrtHttpSseParserConfigValid(
	const xhttpsseparserconfig* pConfig
);



/* 初始化调用方持有且初始零分配的解析器。 */
XRT_API bool xrtHttpSseParserInit(
	xhttpsseparser* pParser,
	const xhttpsseparserconfig* pConfig
);



/* 创建初始零分配的解析器。 */
XRT_API xhttpsseparser* xrtHttpSseParserCreate(
	const xhttpsseparserconfig* pConfig
);



/* 释放解析器持有的动态容量，但不释放解析器结构。 */
XRT_API void xrtHttpSseParserUnit(xhttpsseparser* pParser);



/* 释放解析器持有的动态容量和解析器结构。 */
XRT_API void xrtHttpSseParserDestroy(xhttpsseparser* pParser);



/* 清除全部流状态、Last-Event-ID 和 retry，并恢复配置初值。 */
XRT_API void xrtHttpSseParserReset(xhttpsseparser* pParser);



/* 开始重连后的新响应，保留 Last-Event-ID 和 retry。 */
XRT_API void xrtHttpSseParserReconnect(xhttpsseparser* pParser);



/* 把五个动态缓冲的容量裁剪到当前有效长度。 */
XRT_API bool xrtHttpSseParserTrim(xhttpsseparser* pParser);



/*
	增量读取任意分块输入并最多发布一个借用项目。
	bEnd 只结束当前响应，未由空行完成的事件按规范丢弃。
	Consumed、Item 和可选 Error 可以使用完整但未对齐的存储。
*/
XRT_API xhttpsseparsestatus xrtHttpSseParserRead(
	xhttpsseparser* pParser,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
);



/* 返回当前持久 Last-Event-ID；视图在下一次修改或销毁前有效。 */
XRT_API xstrview xrtHttpSseParserLastEventId(
	const xhttpsseparser* pParser
);



/* 返回当前重连毫秒数。 */
XRT_API uint64 xrtHttpSseParserRetry(
	const xhttpsseparser* pParser
);

#endif



XRT_EXTERN_C_END

#endif
