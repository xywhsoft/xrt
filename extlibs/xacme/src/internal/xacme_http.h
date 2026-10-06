#ifndef XACME_HTTP_H
#define XACME_HTTP_H

#include <xrt/core.h>
#include <xrt/error.h>
#include <xrt/acme_http.h>

struct xnetengine;
struct xnetresolver;
struct xtlsverifier;

#if defined(XACME_FEATURE_ACME_HTTP)

/*
	同步 HTTP(S) 客户端：一次请求一条连接，引擎与信任库可借用。
	sCaPem 为空时使用系统信任库验证对端证书。
*/
typedef struct xacmehttp {
	struct xnetengine* pEngine;
	bool bEngineOwned;
	struct xnetresolver* pResolver;
	struct xtlsverifier* pVerifier;
	int64 uTimeoutMs;
	/* Last exchange failed after a write began; independent of error allocation. */
	bool bWriteUncertain;
	/* 未交付的堆外壳可转移到退休队列；入列后仅由队列访问。 */
	struct xacmehttp* pPendingNext;
	size_t iPendingOwnerSize;
} xacmehttp;

/* 响应字段均为 xrtMalloc 零结尾文本，缺失头为 NULL；正文拒绝原始 NUL。 */
typedef struct xacmehttpresponse {
	uint16 iStatus;
	str sLocation;
	str sReplayNonce;
	str sRetryAfter;
	str sLink;
	str sContentType;
	str sBody;
	size_t iBodySize;
} xacmehttpresponse;

#endif

XRT_EXTERN_C_BEGIN

#if defined(XACME_FEATURE_ACME_HTTP)

/* pBorrowedEngine 为空时自建引擎并在 Unit 中停止销毁。 */
bool xacmeHttpInit(
	xacmehttp* pHttp,
	struct xnetengine* pBorrowedEngine,
	cstr sCaPem,
	int64 uTimeoutMs
);

/* 等待自建引擎的异步关闭退休，保留调用前诊断。
 * true 表示清理完成；false 保留引擎拥有权，不能释放或清零外层对象。
 * 释放阻塞对象后可再次 Unit；无论结果如何，句柄只能继续清理，不能发请求。 */
bool xacmeHttpUnit(xacmehttp* pHttp);

/* 消费一个 HTTP 位于首字段的堆外壳。调用前须已释放其他子对象，
 * 且 Unit 返回 false；复用内嵌链结，不分配内存，立即清除外壳的非 HTTP 数据。 */
void xacmeHttpDeferOwner(xacmehttp* pHttp, size_t iOwnerSize);

void xacmeHttpResponseUnit(xacmehttpresponse* pResponse);

/*
	执行一次 HTTP 交换（http/https 均可）。
	sMethod 为 "GET"/"POST"；POST 空 body 即 POST-as-GET。
	失败返回 false 并设置线程错误；响应不交付部分字段。
	响应头/trailer 各最多 100 字段，正文最多 4 MiB；关闭定界 HTTPS 要求 close_notify。
	URL 支持大小写等价的 http/https 与 IPv6 方括号；无路径查询使用 /?，片段不发送。
	拒绝 userinfo、非法端口、非规范数值地址、未编码控制字符与畸形百分号编码。
	DNS SNI/校验名去末尾根点；IP 字面量只校验证书身份，不发送 SNI。
*/
bool xacmeHttpExchange(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	xacmehttpresponse* pResponse
);

/* 附加请求头（借用视图），用于签名类 API。 */
typedef struct xacmehttpheader {
	cstr sName;
	cstr sValue;
} xacmehttpheader;

/* 同上，支持附加请求头；总数最多 100，包括传输层自动生成的字段。
 * 不可覆盖 Host/User-Agent/Accept/Connection/Content-Type/Content-Length/Transfer-Encoding。 */
bool xacmeHttpExchangeV(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount,
	xacmehttpresponse* pResponse
);

/*
	单次尝试；需要逐次重新签名的 provider 自行管理有限重试。
	失败时响应结构保持全零，调用方无需释放部分响应。
	写请求开始发送后失败会以 XACME_HTTP_ERROR_UNCERTAIN 标记结果未知。
*/
bool xacmeHttpExchangeOnceV(
	xacmehttp* pHttp,
	cstr sMethod,
	cstr sUrl,
	cstr sContentType,
	xstrview sBody,
	const xacmehttpheader* pExtraHeaders,
	size_t iExtraCount,
	xacmehttpresponse* pResponse
);

#endif

XRT_EXTERN_C_END

#endif
