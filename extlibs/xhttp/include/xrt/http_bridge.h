#ifndef XHTTP_HTTP_BRIDGE_H
#define XHTTP_HTTP_BRIDGE_H

#include <xhttp/export.h>

#include <xrt/http_client.h>
#include <xrt/http_exchange.h>
#include <xrt/http_server_exchange.h>
#include <xrt/http_body_stream.h>
#include <xrt/http_cache_time.h>
#include <xrt/http_origin.h>
#include <xrt/http_semantics.h>
#include <xrt/http_auth.h>

/*
	消息与传输适配器之间的公开契约。此头不公开对象布局、不持有 I/O 状态。
	对象修改只允许发生在适配器独占构建期间；交付后由消息读取 API 访问。
	带 Value 后缀的探测函数保留现有错误，其他函数沿用消息层错误契约。
*/
#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)
/* 同步准备期间根据最终线路 target 生成一个字段；返回前由计划复制。 */
typedef bool (*xhttprequestfieldproc)(xstrview Method, xstrview Target, ptr pContext, xhttpfield* pField);
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)
/* 回调仅在提交一个定长正文块期间使用调用方数据。 */
typedef bool (*xhttpbodyfillproc)(void* pOutput, size_t iSize, ptr pData);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)
/* 准备调用借用元数据；成功计划独立拥有线路状态并保留 Body。 */
typedef struct xhttpresponsesource {
	uint16 Status;
	xstrview Reason;
	const xhttpfield* Headers;
	size_t HeaderCount;
	const xhttpfield* Trailers;
	size_t TrailerCount;
	xhttpbody* Body;
	uint64 BodyLength;
} xhttpresponsesource;
#endif

XRT_EXTERN_C_BEGIN

#if defined(XHTTP_FEATURE_HTTP_SERVER_REQUEST)
/* 无错误副作用地验证输出不会覆盖请求拥有的行、字段、正文与 Trailer。 */
XHTTP_API bool xrtHttpBridgeServerRequestOutputValid(
	const xhttpserverrequest* pRequest, const void* pOutput, size_t iSize
);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_REPLY)
/* 无错误副作用地验证输出不会覆盖 Reply 及其拥有的文本和字段。 */
XHTTP_API bool xrtHttpBridgeReplyOutputValid(
	const xhttpreply* pReply, const void* pOutput, size_t iSize
);
#endif

#if defined(XHTTP_FEATURE_HTTP_EXCHANGE)
/* 验证 Exchange 解析、存储和信息响应限额。 */
XHTTP_API bool xrtHttpBridge1ExchangeConfigValid(
	const xhttp1exchangeconfig* pConfig
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE)
/* 冻结请求并用回调生成的单个字段替换全部同名用户字段。 */
XHTTP_API xhttp1requestplan* xrtHttpBridge1RequestPrepareField(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttprequestfieldproc pField,
	ptr pContext
);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_EXCHANGE)
/* 无分配验证 Server Exchange 的静态限额。 */
XHTTP_API bool xrtHttpBridge1ServerConfigValid(
	const xhttp1serverconfig* pConfig
);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)
/* 最终响应被连接成功受理后，接管其全部 Wire 引用。 */
XHTTP_API void xrtHttpBridge1ServerResponseOwnRefs(
	xhttp1serverresponse* pResponse
);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)
/* 从借用型响应来源冻结唯一 HTTP/1 输出计划。 */
XHTTP_API xhttp1serverresponse* xrtHttpBridge1ServerResponsePrepareSource(
	xhttpversion Version,
	xstrview Method,
	uint32 iRequestFlags,
	const xhttpresponsesource* pSource
);
#endif

#if defined(XHTTP_FEATURE_HTTP_BODY_STREAM)
/* 计量完成后以一次载荷分配构建并提交 Chunk。 */
XHTTP_API xhttpbodystreamresult xrtHttpBridgeBodyStreamBuild(
	xhttpbodystream* pStream,
	size_t iSize,
	xhttpbodyfillproc pFill,
	ptr pData
);
#endif

#if defined(XHTTP_FEATURE_HTTP_RANGE)
/* 无错误副作用地迭代已经验证视图的 byte-range-set。 */
XHTTP_API xhttpnext xrtHttpBridgeByteRangeNextValue(
	xstrview Set,
	size_t* pOffset,
	xhttprangespec* pSpec
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME)
/* 饱和累加两个缓存时间，保证溢出不会使条目变年轻。 */
XHTTP_API uint64 xrtHttpBridgeCacheTimeAdd(
	uint64 iLeft,
	uint64 iRight
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CACHE_TIME)
/* 把线路秒数转换为内部毫秒，溢出时保持饱和。 */
XHTTP_API uint64 xrtHttpBridgeCacheTimeSeconds(uint64 iSeconds);
#endif

#if defined(XHTTP_FEATURE_HTTP_RANGE)
/* 无错误副作用地解析一个完整 bytes Content-Range 字段值。 */
XHTTP_API bool xrtHttpBridgeContentRangeParseValue(
	xstrview Value,
	xhttpcontentrange* pRange
);
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)
/* 把单个十六进制字节转换为数值，无效字节返回负数。 */
XHTTP_API int xrtHttpBridgeDigestHexValue(uint8 iByte);
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)
/* 验证借用视图是非空十六进制文本。 */
XHTTP_API bool xrtHttpBridgeDigestHexViewValid(xstrview Text);
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST)
/* 把 nonce count 规范写为八位小写十六进制。 */
XHTTP_API void xrtHttpBridgeDigestNonceCountWrite(
	uint32 iCount,
	char sOutput[8]
);
#endif

#if defined(XHTTP_FEATURE_HTTP_ETAG)
/* 无错误副作用地解析一个完整实体标签。 */
XHTTP_API bool xrtHttpBridgeETagParseValue(
	xstrview Text,
	xhttpetag* pTag
);
#endif

#if defined(XHTTP_FEATURE_HTTP_ETAG)
/* 无错误副作用地验证实体标签正文。 */
XHTTP_API bool xrtHttpBridgeETagValid(const xhttpetag* pTag);
#endif

#if defined(XHTTP_FEATURE_HTTP_ORIGIN)
/* 比较两个已经验证的非 null Origin 三元组。 */
XHTTP_API bool xrtHttpBridgeOriginTupleSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
);
#endif

#if defined(XHTTP_FEATURE_HTTP_ORIGIN)
/* 无错误副作用地验证 Origin 描述符。 */
XHTTP_API bool xrtHttpBridgeOriginValueValid(
	const xhttporigin* pOrigin
);
#endif

#if defined(XHTTP_FEATURE_HTTP_PRECONDITION)
/* 验证条件评估输入并快照当前表示，兼容未对齐的固定结构与字段数组。 */
XHTTP_API bool xrtHttpBridgePreconditionInputRead(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pInput,
	xhttprepresentation* pCurrent
);
#endif

#if defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART)
/* 验证并计算 canonical multipart/byteranges 正文长度。 */
XHTTP_API bool xrtHttpBridgeRangeMultipartLength(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	uint64* pLength
);
#endif

#if defined(XHTTP_FEATURE_HTTP_RANGE)
/* 无错误副作用地拆分一个完整 Range 字段值。 */
XHTTP_API bool xrtHttpBridgeRangeParseValue(
	xstrview Value,
	xstrview* pUnit,
	xstrview* pSet
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
/* 清空已经存在的请求 Trailer；未创建容器时保持无分配。 */
XHTTP_API void xrtHttpBridgeRequestClearTrailers(xhttprequest* pRequest);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST)
/* 校验公开输出不会覆盖请求对象及其借用可见存储。 */
XHTTP_API bool xrtHttpBridgeRequestOutputValid(
	const xhttprequest* pRequest,
	const void* pOutput,
	size_t iSize
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 追加一个拥有型响应 Header。 */
XHTTP_API bool xrtHttpBridgeResponseAddHeader(
	xhttpresponse* pResponse,
	xstrview Name,
	xstrview Value
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 按需创建 trailer 容器并追加一个拥有型字段。 */
XHTTP_API bool xrtHttpBridgeResponseAddTrailer(
	xhttpresponse* pResponse,
	const xhttpheadersconfig* pConfig,
	xstrview Name,
	xstrview Value
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 累计线上编码正文载荷字节数。 */
XHTTP_API bool xrtHttpBridgeResponseAddWireBody(
	xhttpresponse* pResponse,
	uint64 iBytes
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/*
	复制已经通过内部流式包装器交付的正文。
	Exchange 随后统一累计 BodyBytes，因此这里仅增长缓冲内容。
*/
XHTTP_API bool xrtHttpBridgeResponseBufferDeliveredBody(
	xhttpresponse* pResponse,
	xbytesview Data
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 创建客户端事务内部可填充的响应对象。 */
XHTTP_API xhttpresponse* xrtHttpBridgeResponseCreate(
	xhttpversion Version,
	uint16 iStatus,
	xstrview Reason,
	const xhttpheadersconfig* pHeaders
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 在流式模式累计已交付正文，不分配正文缓冲。 */
XHTTP_API bool xrtHttpBridgeResponseDeliverBody(
	xhttpresponse* pResponse,
	uint64 iBytes
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 校验公开输出不会覆盖响应对象及其借用可见存储。 */
XHTTP_API bool xrtHttpBridgeResponseOutputValid(
	const xhttpresponse* pResponse,
	const void* pOutput,
	size_t iSize
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 在全部解码层结束后发布调用方可见的准确正文长度。 */
XHTTP_API void xrtHttpBridgeResponseSetBodyBytes(
	xhttpresponse* pResponse,
	uint64 iBodyBytes
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 清除内部包装器产生的流式标志，恢复调用方选择的缓冲语义。 */
XHTTP_API void xrtHttpBridgeResponseSetBuffered(xhttpresponse* pResponse);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 接管原始编码文本，删除失效的表示元数据并标记响应已经解码。 */
XHTTP_API void xrtHttpBridgeResponseSetDecoded(
	xhttpresponse* pResponse,
	str sOriginalEncoding,
	size_t iOriginalEncodingSize
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 设置不占用客户端 Call 终态槽的响应读取错误。 */
XHTTP_API void xrtHttpBridgeResponseSetError(
	xerrkind Kind,
	xhttpresponseerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 增加由客户端事务确认的响应标志。 */
XHTTP_API void xrtHttpBridgeResponseSetFlags(
	xhttpresponse* pResponse,
	uint32 iFlags
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 保存高层客户端已经完成的重定向跳数。 */
XHTTP_API void xrtHttpBridgeResponseSetRedirects(
	xhttpresponse* pResponse,
	size_t iRedirects
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 失败原子地替换最终有效 URL。 */
XHTTP_API bool xrtHttpBridgeResponseSetUrl(
	xhttpresponse* pResponse,
	xstrview Url
);
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE)
/* 提取并包装当前线程错误，保留底层类别和完整原因链。 */
XHTTP_API void xrtHttpBridgeResponseWrapError(
	xerrkind DefaultKind,
	xhttpresponseerror Code,
	cstr sOperation,
	cstr sMessage
);
#endif

#if defined(XHTTP_FEATURE_URL)
/* 无错误副作用地严格解析 URI-reference，结果借用输入文本。 */
XHTTP_API bool xrtHttpBridgeUrlParseValue(
	xstrview Text,
	xurl* pUrl
);
#endif

#if defined(XHTTP_FEATURE_URL)
/* 无错误副作用地取得显式端口或已知 scheme 的默认端口。 */
XHTTP_API bool xrtHttpBridgeUrlPortValue(const xurl* pUrl, uint16* pPort);
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)
/* 创建已组装线路字节的输出计划，成功保留 Body 引用。 */
XHTTP_API xhttp1serverresponse* xrtHttpBridgeServerWireBody(
	xhttpbody* pBody, uint64 iLength, bool bClose, bool bTunnel
);
/* 复制 Chunk 描述符；接管租约前必须由适配器确认响应已被受理。 */
XHTTP_API xhttp1serverresponse* xrtHttpBridgeServerWireChunks(
	const xhttpbodychunk* pChunks, size_t iCount, uint64 iLength, bool bClose, bool bTunnel
);
#endif

XRT_EXTERN_C_END
#endif
