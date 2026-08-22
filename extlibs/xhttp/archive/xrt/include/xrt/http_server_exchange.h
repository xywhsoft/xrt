#ifndef XRT_HTTP_SERVER_EXCHANGE_H
#define XRT_HTTP_SERVER_EXCHANGE_H

#include <xrt/http_server.h>



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_HTTP_HOST) || \
	 !defined(XRT_FEATURE_HTTP_TARGET) || \
	 !defined(XRT_FEATURE_HTTP_EXPECT) || \
	 !defined(XRT_FEATURE_HTTP_TE) || \
	 !defined(XRT_FEATURE_HTTP_CONNECTION) || \
	 !defined(XRT_FEATURE_HTTP_TRAILER))
	#error "XRT HTTP server Exchange support requires server request, Host, target, Expect, TE, Connection and Trailer support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_HTTP_SERVER_REPLY) || \
	 !defined(XRT_FEATURE_HTTP1_BODY) || \
	 !defined(XRT_FEATURE_HTTP_TRAILER))
	#error "XRT HTTP server response support requires request, Reply, HTTP/1 body and Trailer support"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

/* Server Exchange 是无 I/O 的单连接串行 HTTP/1 请求状态机。 */
typedef struct xhttp1serverexchange xhttp1serverexchange;



/* Feed 明确区分等待输入、背压暂停、应用拒绝、完整请求和可靠空闲 EOF。 */
typedef enum xhttp1serverfeedstatus {
	XHTTP1_SERVER_FEED_ERROR = -1,
	XHTTP1_SERVER_FEED_MORE = 0,
	XHTTP1_SERVER_FEED_COMPLETE = 1,
	XHTTP1_SERVER_FEED_PAUSED = 2,
	XHTTP1_SERVER_FEED_REJECTED = 3,
	XHTTP1_SERVER_FEED_CLOSED = 4
} xhttp1serverfeedstatus;



/* Header 回调为当前请求选择唯一正文交付策略。 */
typedef enum xhttpserverbodypolicy {
	XHTTP_SERVER_BODY_BUFFER = 0,
	XHTTP_SERVER_BODY_STREAM = 1,
	XHTTP_SERVER_BODY_REJECT = 2,
	XHTTP_SERVER_BODY_DISCARD = 3
} xhttpserverbodypolicy;



/* Exchange 错误码区分线路协议、策略限额、回调和内部状态。 */
typedef enum xhttp1servererror {
	XHTTP1_SERVER_ERROR_ARGUMENT = 1,
	XHTTP1_SERVER_ERROR_STATE,
	XHTTP1_SERVER_ERROR_HEAD,
	XHTTP1_SERVER_ERROR_TARGET,
	XHTTP1_SERVER_ERROR_HOST,
	XHTTP1_SERVER_ERROR_EXPECTATION,
	XHTTP1_SERVER_ERROR_TE,
	XHTTP1_SERVER_ERROR_FRAMING,
	XHTTP1_SERVER_ERROR_BODY_LIMIT,
	XHTTP1_SERVER_ERROR_BODY_STORAGE,
	XHTTP1_SERVER_ERROR_TRAILER,
	XHTTP1_SERVER_ERROR_TRAILER_STORAGE,
	XHTTP1_SERVER_ERROR_HEADERS_CALLBACK,
	XHTTP1_SERVER_ERROR_BODY_CALLBACK,
	XHTTP1_SERVER_ERROR_COMPLETE_CALLBACK,
	XHTTP1_SERVER_ERROR_UNEXPECTED_EOF
} xhttp1servererror;



/*
	Headers 在完整请求头复制完成后同步调用，并选择缓冲、流式或拒绝正文。
	Body 只在流式策略下接收借用片段；返回 false 使 Exchange 失败。
	Complete 在正文与 Trailer 完整后调用；全部回调都由 Feed 串行执行。
*/
typedef xhttpserverbodypolicy (*xhttpserverheadersproc)(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
);

typedef bool (*xhttpserverbodyproc)(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
);

typedef bool (*xhttpservercompleteproc)(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
);



/* 事件表不持有用户数据，生命周期必须覆盖 Exchange。 */
typedef struct xhttp1serverevents {
	xhttpserverheadersproc Headers;
	xhttpserverbodyproc Body;
	xhttpservercompleteproc Complete;
	ptr Data;
} xhttp1serverevents;



/* 默认只接受本库能够完整处理的 chunked；原始传输编码必须显式开启。 */
typedef struct xhttp1serverconfig {
	xhttp1limits Head;
	xhttp1bodylimits Body;
	bool AllowRawTransferCodings;
} xhttp1serverconfig;

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE)

/* Server Response 是不拥有网络连接的单次 HTTP/1 响应线路状态机。 */
typedef struct xhttp1serverresponse xhttp1serverresponse;



/* 输出状态明确区分正文源暂不可读、线路完成与协议升级。 */
typedef enum xhttp1serveroutputstatus {
	XHTTP1_SERVER_OUTPUT_ERROR = -1,
	XHTTP1_SERVER_OUTPUT_AGAIN = 0,
	XHTTP1_SERVER_OUTPUT_DATA = 1,
	XHTTP1_SERVER_OUTPUT_DONE = 2,
	XHTTP1_SERVER_OUTPUT_TUNNEL = 3
} xhttp1serveroutputstatus;



/* 响应错误码区分准备、分帧、正文来源、长度和状态错误。 */
typedef enum xhttp1serverresponseerror {
	XHTTP1_SERVER_RESPONSE_ERROR_ARGUMENT = 1,
	XHTTP1_SERVER_RESPONSE_ERROR_STATUS,
	XHTTP1_SERVER_RESPONSE_ERROR_HEADER,
	XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
	XHTTP1_SERVER_RESPONSE_ERROR_TRAILER,
	XHTTP1_SERVER_RESPONSE_ERROR_BODY,
	XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
	XHTTP1_SERVER_RESPONSE_ERROR_STATE
} xhttp1serverresponseerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

/*
	初始化公网 Header 限额和 4 MiB 默认请求正文上限。
	配置允许位于未对齐存储，Exchange 创建时会先复制配置和事件表。
*/
XRT_API void xrtHttp1ServerConfigInit(
	xhttp1serverconfig* pConfig
);



/* 创建不拥有 Socket、线程或 Timer 的串行 Server Exchange。 */
XRT_API xhttp1serverexchange* xrtHttp1ServerExchangeCreate(
	const xhttp1serverconfig* pConfig,
	const xhttp1serverevents* pEvents
);



/* 销毁当前请求、临时解析存储和终态错误。 */
XRT_API void xrtHttp1ServerExchangeDestroy(
	xhttp1serverexchange* pExchange
);



/*
	同步消费一段连接输入，Accepted 是可以从调用方缓冲移除的精确前缀。
	End 只在该段之后到达可靠 EOF 时为真；流水线后缀不会被提前接收。
	Accepted 允许未对齐存储，但不得覆盖 Input、Exchange 或当前请求快照。
	Input 必须是完整不回绕的只读范围，并且不得借用 Exchange 或当前请求内存。
*/
XRT_API xhttp1serverfeedstatus xrtHttp1ServerExchangeFeed(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
);



/*
	暂停当前请求正文消费；已交付给回调的片段视为已接受。
	最后一个固定长度片段也可以在 Body 回调内暂停，恢复后以空输入 Feed 完成请求。
	可以在 Headers、Body 回调或所属 Worker 的其他串行路径调用。
*/
XRT_API bool xrtHttp1ServerExchangePause(
	xhttp1serverexchange* pExchange
);



/* 恢复正文消费；调用方随后以尚未接受的输入再次 Feed。 */
XRT_API bool xrtHttp1ServerExchangeResume(
	xhttp1serverexchange* pExchange
);



/*
	修改当前请求正文上限，只允许在第一段正文交付前调用。
	Headers 回调可以按路由提高、降低或取消默认 4 MiB 上限。
*/
XRT_API bool xrtHttp1ServerExchangeSetBodyLimit(
	xhttp1serverexchange* pExchange,
	uint64 iMaxBody
);



/*
	完成响应后进入下一条串行 keep-alive 请求。
	当前请求不允许复用连接或尚未完整时调用失败。
*/
XRT_API bool xrtHttp1ServerExchangeNext(
	xhttp1serverexchange* pExchange
);



/* 返回当前拥有型请求的借用指针，尚未完成 Header 时为空。 */
XRT_API const xhttpserverrequest* xrtHttp1ServerExchangeRequest(
	const xhttp1serverexchange* pExchange
);



/* 返回稳定终态错误；非失败状态返回空指针。 */
XRT_API const xerror* xrtHttp1ServerExchangeError(
	const xhttp1serverexchange* pExchange
);



/* 判断当前请求是否已经完整交付 Header、正文和 Trailer。 */
XRT_API bool xrtHttp1ServerExchangeComplete(
	const xhttp1serverexchange* pExchange
);



/* 判断 Exchange 是否因正文消费者背压而暂停。 */
XRT_API bool xrtHttp1ServerExchangePaused(
	const xhttp1serverexchange* pExchange
);



/* 返回当前请求已经接受的 Header 与正文线缆字节总数。 */
XRT_API uint64 xrtHttp1ServerExchangeWireBytes(
	const xhttp1serverexchange* pExchange
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE)

/*
	冻结一条 HTTP/1.1 信息响应。
	只接受 100..199（不含 101），并拒绝正文、Trailer 和分帧字段。
*/
XRT_API xhttp1serverresponse* xrtHttp1ServerResponseInform(
	xhttpversion Version,
	const xhttpreply* pReply
);



/*
	按已经固定的请求事实冻结 Reply，并生成唯一 HTTP/1 响应线路计划。
	Method 区分 HEAD、CONNECT 与普通响应，Flags 提供 keep-alive 和 Upgrade 事实。
*/
XRT_API xhttp1serverresponse* xrtHttp1ServerResponsePrepare(
	xhttpversion Version,
	xstrview Method,
	uint32 iRequestFlags,
	const xhttpreply* pReply
);



/*
	冻结 Reply 并为所属请求生成唯一 HTTP/1 响应线路计划。
	结果复制全部响应元数据并保留正文引用，调用方可以立即修改或销毁 Reply。
*/
XRT_API xhttp1serverresponse* xrtHttp1ServerResponseCreate(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply
);



/* 销毁响应计划、正文 Reader、未消费 Chunk 和稳定错误。 */
XRT_API void xrtHttp1ServerResponseDestroy(
	xhttp1serverresponse* pResponse
);



/*
	借出不超过 MaxBytes 的下一段线路数据。
	DATA 必须通过 OutputConsume 推进，AGAIN 表示正文源当前不可读。
	存在未消费 Offer 时重复调用返回同一租约，此时新的 MaxBytes 不会缩短它。
*/
XRT_API xhttp1serveroutputstatus xrtHttp1ServerResponseOutput(
	xhttp1serverresponse* pResponse,
	size_t iMaxBytes,
	xbytesview* pData
);



/* 消费最近一次 Output 借出片段的前缀，允许网络短写和零长度消费。 */
XRT_API bool xrtHttp1ServerResponseOutputConsume(
	xhttp1serverresponse* pResponse,
	size_t iSize
);



/* 返回响应准备或输出阶段的稳定终态错误。 */
XRT_API const xerror* xrtHttp1ServerResponseError(
	const xhttp1serverresponse* pResponse
);



/* 判断全部 HTTP/1 线路字节是否已经由调用方确认消费。 */
XRT_API bool xrtHttp1ServerResponseComplete(
	const xhttp1serverresponse* pResponse
);



/* 判断响应后是否必须关闭 HTTP 连接。 */
XRT_API bool xrtHttp1ServerResponseClose(
	const xhttp1serverresponse* pResponse
);



/* 判断线路结束后是否应把连接交给升级协议或 CONNECT 隧道。 */
XRT_API bool xrtHttp1ServerResponseTunnel(
	const xhttp1serverresponse* pResponse
);



/* 判断该计划是否为发送后仍须等待最终响应的信息响应。 */
XRT_API bool xrtHttp1ServerResponseInformational(
	const xhttp1serverresponse* pResponse
);



/* 返回已经通过 OutputConsume 确认发送的线路字节数。 */
XRT_API uint64 xrtHttp1ServerResponseWireBytes(
	const xhttp1serverresponse* pResponse
);

#endif



XRT_EXTERN_C_END

#endif
