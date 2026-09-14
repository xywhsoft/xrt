#ifndef XACME_HTTP_H
#define XACME_HTTP_H

#include <xrt/core.h>
#include <xrt/error.h>

struct xnetengine;
struct xnetresolver;
struct xtlsverifier;

#if defined(XACME_FEATURE_ACME_HTTP)

/* http 模块稳定错误码（错误域 "xrt.acme.http"）。 */
typedef enum xacmehttperror {
	XACME_HTTP_ERROR_ARGUMENT = 1,
	XACME_HTTP_ERROR_URL,
	XACME_HTTP_ERROR_CONNECT,
	XACME_HTTP_ERROR_SEND,
	XACME_HTTP_ERROR_PROTOCOL,
	XACME_HTTP_ERROR_TIMEOUT,
	XACME_HTTP_ERROR_TLS
} xacmehttperror;

/*
	同步 HTTP(S) 客户端：一次请求一条连接，引擎与信任库可借用。
	sCaPem 为空时使用系统信任库验证对端证书。
*/
typedef struct xacmehttp {
	struct xnetengine* pEngine;
	bool bEngineOwned;
	struct xnetresolver* pResolver;
	struct xtlsverifier* pVerifier;
	uint64 uTimeoutUs;
} xacmehttp;

/* 响应字段均为 xrtMalloc 零结尾文本，缺失头为 NULL。 */
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
	uint64 uTimeoutUs
);

void xacmeHttpUnit(xacmehttp* pHttp);

void xacmeHttpResponseUnit(xacmehttpresponse* pResponse);

/*
	执行一次 HTTP 交换（http/https 均可）。
	sMethod 为 "GET"/"POST"；POST 空 body 即 POST-as-GET。
	失败返回 false 并设置线程错误。
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

/* 同上，支持附加请求头。 */
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

#endif

XRT_EXTERN_C_END

#endif
