#ifndef XRT_INTERNAL_HTTP_SERVER_H
#define XRT_INTERNAL_HTTP_SERVER_H

#include "xrt_internal.h"
#include "xrt_http.h"
#include <xrt/http_server.h>

#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)
	#include <xrt/http_server_exchange.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE) && \
	!defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)
	#include <xrt/http_server_exchange.h>
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST)

/* 请求头使用单块紧凑存储，正文和 Trailer 只在实际出现时分配。 */
struct xhttpserverrequest {
	volatile int32 RefCount;
	xhttpversion Version;
	uint32 Flags;
	xhttp1bodyplan Plan;
	xstrview Method;
	xstrview Target;
	xhttpfield* Fields;
	size_t FieldCount;
	uint8* Body;
	size_t BodySize;
	size_t BodyCapacity;
	uint64 BodyBytes;
	ptr TrailerBlock;
	xhttpfield* Trailers;
	size_t TrailerCount;
};



/* 从已完成解析的请求 Header 创建紧凑拥有型快照。 */
xhttpserverrequest* __xrtHttpServerRequestCreate(
	const xhttp1head* pHead,
	const xhttp1bodyplan* pPlan,
	uint32 iFlags
);



/* 把一段正文复制进按需缓冲并累计接收字节。 */
bool __xrtHttpServerRequestAppendBody(
	xhttpserverrequest* pRequest,
	xbytesview Data
);



/* 仅累计流式交付正文，不分配正文缓冲。 */
bool __xrtHttpServerRequestDeliverBody(
	xhttpserverrequest* pRequest,
	uint64 iBytes
);



/* 复制完整 Trailer 集合并失败原子地发布。 */
bool __xrtHttpServerRequestSetTrailers(
	xhttpserverrequest* pRequest,
	const xhttpfield* pTrailers,
	size_t iCount
);



/* 增加请求稳定事实标志。 */
void __xrtHttpServerRequestSetFlags(
	xhttpserverrequest* pRequest,
	uint32 iFlags
);



/* 设置不占用网络 Connection 终态槽的请求辅助层错误。 */
void __xrtHttpServerRequestSetError(
	xerrkind Kind,
	xhttpserverrequesterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 提取并包装当前线程错误，保留最内层错误类别和完整原因链。 */
void __xrtHttpServerRequestWrapError(
	xerrkind DefaultKind,
	xhttpserverrequesterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 验证输出范围完整且不会覆盖请求行、字段、正文或 Trailer 快照。 */
bool __xrtHttpServerRequestOutputValid(
	const xhttpserverrequest* pRequest,
	const void* pOutput,
	size_t iSize
);



/* 只向高层表单解析器公开完整的拥有型缓冲正文。 */
bool __xrtHttpServerRequestBufferedBody(
	const xhttpserverrequest* pRequest,
	xbytesview* pBody,
	cstr sOperation
);



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH)

/* 返回唯一认证字段，并统一处理缺失、重复和参数错误。 */
xhttpnext __xrtHttpServerRequestAuthField(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	const xhttpfield** ppField
);

#endif

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_EXCHANGE)

#define XRT_HTTP_SERVER_STATE_HEAD UINT32_C(1)
#define XRT_HTTP_SERVER_STATE_BODY UINT32_C(2)
#define XRT_HTTP_SERVER_STATE_COMPLETE UINT32_C(3)
#define XRT_HTTP_SERVER_STATE_REJECTED UINT32_C(4)
#define XRT_HTTP_SERVER_STATE_CLOSED UINT32_C(5)
#define XRT_HTTP_SERVER_STATE_FAILED UINT32_C(6)



/* 临时 Header 缓冲只在请求头跨 Feed 边界时存在。 */
typedef struct xrt_http_server_buffer {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xrt_http_server_buffer;



/* Exchange 只保存一个当前请求，并让调用方保留未接受的流水线后缀。 */
struct xhttp1serverexchange {
	xhttp1serverconfig Config;
	xhttp1serverevents Events;
	xrt_http_server_buffer HeadBuffer;
	xhttpfield* ParseFields;
	size_t ParseFieldCapacity;
	xhttpfield* ParseTrailers;
	size_t ParseTrailerCapacity;
	xhttpserverrequest* Request;
	xhttp1body Body;
	xerror* Error;
	uint64 WireBytes;
	uint64 BodyLimit;
	uint32 State;
	uint32 Delimiter;
	bool Paused;
	bool BodyStarted;
	bool InCallback;
};



/* 无分配验证 Server Exchange 的静态限额。 */
bool __xrtHttp1ServerConfigValid(
	const xhttp1serverconfig* pConfig
);



/* 建立并保存唯一 Server Exchange 终态错误。 */
bool __xrtHttp1ServerExchangeFailCause(
	xhttp1serverexchange* pExchange,
	xhttp1servererror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 建立没有额外原因链的 Server Exchange 终态错误。 */
bool __xrtHttp1ServerExchangeFail(
	xhttp1serverexchange* pExchange,
	xhttp1servererror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 累计当前请求已经接受的线缆字节，并拒绝计数溢出。 */
bool __xrtHttp1ServerExchangeWireAdd(
	xhttp1serverexchange* pExchange,
	size_t iBytes
);



/* 完成当前请求并调用唯一 Complete 事件。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFinish(
	xhttp1serverexchange* pExchange
);



/* 推进请求头，Accepted 只覆盖已复制或已形成快照的输入。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFeedHead(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
);



/* 推进正文与 Trailer，并在流式策略下同步交付正文片段。 */
xhttp1serverfeedstatus __xrtHttp1ServerExchangeFeedBody(
	xhttp1serverexchange* pExchange,
	xbytesview Input,
	bool bEnd,
	size_t* pAccepted
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE)

#define XRT_HTTP_SERVER_RESPONSE_HEAD UINT32_C(1)
#define XRT_HTTP_SERVER_RESPONSE_BODY UINT32_C(2)
#define XRT_HTTP_SERVER_RESPONSE_CHUNK_LINE UINT32_C(3)
#define XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA UINT32_C(4)
#define XRT_HTTP_SERVER_RESPONSE_CHUNK_END UINT32_C(5)
#define XRT_HTTP_SERVER_RESPONSE_DONE UINT32_C(6)
#define XRT_HTTP_SERVER_RESPONSE_TUNNEL UINT32_C(7)
#define XRT_HTTP_SERVER_RESPONSE_FAILED UINT32_C(8)



/* 响应来源借用调用期元数据，冻结结果独立拥有全部线缆状态。 */
typedef struct xrt_http1_server_response_source {
	uint16 Status;
	xstrview Reason;
	const xhttpfield* Headers;
	size_t HeaderCount;
	const xhttpfield* Trailers;
	size_t TrailerCount;
	xhttpbody* Body;
	uint64 BodyLength;
} xrt_http1_server_response_source;



/* Response 使用一块尾随存储持有完整 Header 和终止 chunk。 */
struct xhttp1serverresponse {
	bytes Head;
	size_t HeadSize;
	bytes End;
	size_t EndSize;
	xhttpbody* Body;
	xhttpbodyreader* Reader;
	xhttpbodychunk Chunk;
	xhttpbodychunk* WireRefs;
	size_t WireRefCount;
	size_t WireRefIndex;
	xerror* Error;
	uint64 BodyLength;
	uint64 BodyRemaining;
	uint64 WireBytes;
	size_t HeadOffset;
	size_t PartOffset;
	size_t Offered;
	cbytes OfferData;
	char ChunkLine[32];
	size_t ChunkLineSize;
	xhttp1bodymode Mode;
	uint32 State;
	bool ReaderOpened;
	bool ChunkTerminal;
	bool OutputAgain;
	bool Close;
	bool Tunnel;
	bool Informational;
	bool WireRefsOwned;
};



/* 从借用型响应来源冻结唯一 HTTP/1 输出计划。 */
xhttp1serverresponse* __xrtHttp1ServerResponsePrepareSource(
	xhttpversion Version,
	xstrview Method,
	uint32 iRequestFlags,
	const xrt_http1_server_response_source* pSource
);



/* 建立并保存唯一 Server Response 终态错误。 */
bool __xrtHttp1ServerResponseFailCause(
	xhttp1serverresponse* pResponse,
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 建立没有额外原因链的 Server Response 终态错误。 */
bool __xrtHttp1ServerResponseFail(
	xhttp1serverresponse* pResponse,
	xhttp1serverresponseerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 最终响应被连接成功受理后，接管其全部 Wire 引用。 */
void __xrtHttp1ServerResponseOwnRefs(
	xhttp1serverresponse* pResponse
);



/* 释放并推进当前已经完整输出的 Wire 引用。 */
void __xrtHttp1ServerResponseReleaseCurrentRef(
	xhttp1serverresponse* pResponse
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY)

/* Reply 只在使用相应内容时创建字段容器、原因存储和正文对象。 */
struct xhttpreply {
	xhttpreplyconfig Config;
	uint16 Status;
	bool CustomReason;
	xstrview Reason;
	str ReasonStorage;
	xhttpheaders* Headers;
	xhttpheaders* Trailers;
	xhttpbody* Body;
};



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH)

/* 写出并追加认证 challenge，临时字段值在消费后立即清零。 */
bool __xrtHttpReplyAddWrittenAuth(
	xhttpreply* pReply,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
);



/* 写出并设置唯一认证信息字段，临时字段值在消费后立即清零。 */
bool __xrtHttpReplySetWrittenAuth(
	xhttpreply* pReply,
	xstrview Name,
	__xrtHttpAuthWriteFunction pWrite,
	const void* pContext
);

#endif

#endif



#if defined(XRT_FEATURE_HTTP_REPLY_COMPRESS)

/* 建立供协议压缩层与 Server 便利层共用的结构化错误。 */
void __xrtHttpReplyCompressError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
);

#endif

#endif
