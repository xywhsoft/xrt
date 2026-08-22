#ifndef XRT_HTTP_SERVER_H
#define XRT_HTTP_SERVER_H

#include <xrt/http.h>
#include <xrt/http_body.h>

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST)
	#include <xrt/http1.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_QUERY) || \
	defined(XRT_FEATURE_HTTP_SERVER_FORM)
	#include <xrt/query_params.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_COOKIE)
	#include <xrt/cookie.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH) || \
	defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BASIC) || \
	defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BEARER) || \
	defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_DIGEST) || \
	defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) || \
	defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BASIC) || \
	defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BEARER) || \
	defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST) || \
	defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST_INFO)
	#include <xrt/http_auth.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE) || \
	defined(XRT_FEATURE_HTTP_SERVER_FORM) || \
	defined(XRT_FEATURE_HTTP_SERVER_FORM_DATA)
	#include <xrt/mime.h>
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_FORM_DATA)
	#include <xrt/form_data.h>
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY) && \
	(!defined(XRT_FEATURE_HTTP_HEADERS) || \
	 !defined(XRT_FEATURE_HTTP_BODY))
	#error "XRT HTTP server Reply support requires HTTP Headers and body support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST) && \
	(!defined(XRT_FEATURE_HTTP1_BODY) || \
	 !defined(XRT_FEATURE_HTTP_TARGET))
	#error "XRT HTTP server request support requires HTTP/1 body and target support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_HTTP_AUTH))
	#error "XRT HTTP server request authentication requires request and authentication support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BASIC) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_BASIC))
	#error "XRT HTTP server Basic request authentication requires request authentication and Basic support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BEARER) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_BEARER))
	#error "XRT HTTP server Bearer request authentication requires request authentication and Bearer support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_DIGEST) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS))
	#error "XRT HTTP server Digest request authentication requires request authentication and Digest credentials"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REPLY) || \
	 !defined(XRT_FEATURE_HTTP_AUTH))
	#error "XRT HTTP server reply authentication requires Reply and authentication support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BASIC) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_BASIC))
	#error "XRT HTTP server Basic reply authentication requires Reply authentication and Basic support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BEARER) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_BEARER_CHALLENGE))
	#error "XRT HTTP server Bearer reply authentication requires Reply authentication and Bearer challenge support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE))
	#error "XRT HTTP server Digest reply authentication requires Reply authentication and Digest challenge support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST_INFO) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH) || \
	 !defined(XRT_FEATURE_HTTP_AUTH_DIGEST_INFO))
	#error "XRT HTTP server Digest info requires Reply authentication and Digest info support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_QUERY) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_QUERY_PARAMS))
	#error "XRT HTTP server query support requires request and QueryParams support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_COOKIE) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_COOKIE))
	#error "XRT HTTP server cookie support requires request and Cookie support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_REQUEST) || \
	 !defined(XRT_FEATURE_MIME))
	#error "XRT HTTP server Content-Type support requires request and MIME support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_FORM) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE) || \
	 !defined(XRT_FEATURE_QUERY_PARAMS))
	#error "XRT HTTP server form support requires Content-Type and QueryParams support"
#endif

#if defined(XRT_FEATURE_HTTP_SERVER_FORM_DATA) && \
	(!defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE) || \
	 !defined(XRT_FEATURE_FORM_DATA_PARSE))
	#error "XRT HTTP server FormData support requires Content-Type and FormData parsing"
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST)

/* 服务端请求拥有解析后的请求行与字段，可安全跨越一次 Feed 调用。 */
typedef struct xhttpserverrequest xhttpserverrequest;



/* 请求标志描述连接、升级、Expect、正文交付和完整性事实。 */
typedef enum xhttpserverrequestflag {
	XHTTP_SERVER_REQUEST_NONE = 0,
	XHTTP_SERVER_REQUEST_KEEP_ALIVE = UINT32_C(0x00000001),
	XHTTP_SERVER_REQUEST_UPGRADE = UINT32_C(0x00000002),
	XHTTP_SERVER_REQUEST_EXPECT_CONTINUE = UINT32_C(0x00000004),
	XHTTP_SERVER_REQUEST_STREAMED = UINT32_C(0x00000008),
	XHTTP_SERVER_REQUEST_COMPLETE = UINT32_C(0x00000010),
	XHTTP_SERVER_REQUEST_DISCARDED = UINT32_C(0x00000020),
	XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS = UINT32_C(0x00000040)
} xhttpserverrequestflag;



/* 请求辅助层稳定区分参数、状态、字段、媒体类型、正文和具体解析阶段。 */
typedef enum xhttpserverrequesterror {
	XHTTP_SERVER_REQUEST_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_REQUEST_ERROR_STATE,
	XHTTP_SERVER_REQUEST_ERROR_TARGET,
	XHTTP_SERVER_REQUEST_ERROR_HEADER,
	XHTTP_SERVER_REQUEST_ERROR_CONTENT_TYPE,
	XHTTP_SERVER_REQUEST_ERROR_BODY,
	XHTTP_SERVER_REQUEST_ERROR_QUERY,
	XHTTP_SERVER_REQUEST_ERROR_FORM,
	XHTTP_SERVER_REQUEST_ERROR_AUTH
} xhttpserverrequesterror;

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY)

/* Reply 是无网络依赖的拥有型服务端响应构建器。 */
typedef struct xhttpreply xhttpreply;



/*
	Header 与 Trailer 分别保存容量和安全限额。
	默认初始容量为零，空 Reply 除自身外不预先分配其他对象。
*/
typedef struct xhttpreplyconfig {
	xhttpheadersconfig Headers;
	xhttpheadersconfig Trailers;
} xhttpreplyconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST)

/* 增加请求引用并返回原指针；请求正文仍由所属 Worker 串行推进。 */
XRT_API xhttpserverrequest* xrtHttpServerRequestRef(
	xhttpserverrequest* pRequest
);



/* 释放请求引用及其实际使用的字段、Trailer 和缓冲正文。 */
XRT_API void xrtHttpServerRequestDestroy(
	xhttpserverrequest* pRequest
);



/* 返回请求 HTTP 版本；空请求返回零。 */
XRT_API xhttpversion xrtHttpServerRequestVersion(
	const xhttpserverrequest* pRequest
);



/* 返回借用的大小写敏感请求方法。 */
XRT_API xstrview xrtHttpServerRequestMethod(
	const xhttpserverrequest* pRequest
);



/* 返回借用的原始 request-target。 */
XRT_API xstrview xrtHttpServerRequestTarget(
	const xhttpserverrequest* pRequest
);



/* 解析拥有 target 的四种形式，结果借用请求快照。 */
XRT_API bool xrtHttpServerRequestParseTarget(
	const xhttpserverrequest* pRequest,
	xhttptarget* pTarget
);



/*
	解析请求的有效 authority。
	absolute 和 CONNECT 使用 target，其他形式使用 Host 字段。
*/
XRT_API bool xrtHttpServerRequestAuthority(
	const xhttpserverrequest* pRequest,
	xurl* pAuthority
);



/* 返回连接、Expect、正文交付和完整性标志。 */
XRT_API uint32 xrtHttpServerRequestFlags(
	const xhttpserverrequest* pRequest
);



/* 判断 HTTP/1.1 客户端是否正确声明不会丢弃响应 Trailer。 */
XRT_API bool xrtHttpServerRequestAcceptsTrailers(
	const xhttpserverrequest* pRequest
);



/* 返回请求正文的唯一 HTTP/1 分帧模式。 */
XRT_API xhttp1bodymode xrtHttpServerRequestBodyMode(
	const xhttpserverrequest* pRequest
);



/* 返回定长正文声明长度；其他分帧模式返回零。 */
XRT_API uint64 xrtHttpServerRequestContentLength(
	const xhttpserverrequest* pRequest
);



/* 返回首个同名借用 Header；未找到返回空指针，字段名必须是有效 token。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeader(
	const xhttpserverrequest* pRequest,
	xstrview Name
);



/* 返回请求 Header 数量。 */
XRT_API size_t xrtHttpServerRequestHeaderCount(
	const xhttpserverrequest* pRequest
);



/* 返回连续只读 Header 数组；空请求或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeaderData(
	const xhttpserverrequest* pRequest
);



/* 返回指定位置的借用 Header，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpServerRequestHeaderAt(
	const xhttpserverrequest* pRequest,
	size_t iIndex
);



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH)

/* 解析唯一 Authorization；缺失、有效和错误分别返回 END、ITEM 和 ERROR。 */
XRT_API xhttpnext xrtHttpServerRequestAuth(
	const xhttpserverrequest* pRequest,
	xhttpauth* pAuth
);



/* 解析唯一 Proxy-Authorization；结果视图借用请求快照。 */
XRT_API xhttpnext xrtHttpServerRequestProxyAuth(
	const xhttpserverrequest* pRequest,
	xhttpauth* pAuth
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BASIC)

/*
	解码唯一 Basic Authorization；缺失返回 END 并发布空结果。
	OOM 或格式错误清空 Basic，但保持长度和正文输出不变；短缓冲发布所需长度。
*/
XRT_API xhttpnext xrtHttpServerRequestBasicAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
);



/*
	解码唯一 Basic Proxy-Authorization；缺失返回 END 并发布空结果。
	输出原子性与源站 Basic 入口一致。
*/
XRT_API xhttpnext xrtHttpServerRequestProxyBasicAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_BEARER)

/* 解析唯一 Bearer Authorization，Token 借用请求快照。 */
XRT_API xhttpnext xrtHttpServerRequestBearerAuth(
	const xhttpserverrequest* pRequest,
	xstrview* pToken
);



/* 解析唯一 Bearer Proxy-Authorization，Token 借用请求快照。 */
XRT_API xhttpnext xrtHttpServerRequestProxyBearerAuth(
	const xhttpserverrequest* pRequest,
	xstrview* pToken
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_DIGEST)

/* 解码唯一 Digest Authorization；标准值借用 Output。 */
XRT_API xhttpnext xrtHttpServerRequestDigestAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
);



/* 解码唯一 Digest Proxy-Authorization；标准值借用 Output。 */
XRT_API xhttpnext xrtHttpServerRequestProxyDigestAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
);

#endif



/*
	返回缓冲模式下已经拥有的连续正文。
	流式与丢弃模式始终返回空视图；流式片段只在回调期间借用。
*/
XRT_API xbytesview xrtHttpServerRequestBody(
	const xhttpserverrequest* pRequest
);



/* 返回已经缓冲、流式交付或丢弃的正文总字节数。 */
XRT_API uint64 xrtHttpServerRequestBodyBytes(
	const xhttpserverrequest* pRequest
);



/* 返回首个同名借用 Trailer；未找到返回空指针，字段名必须是有效 token。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailer(
	const xhttpserverrequest* pRequest,
	xstrview Name
);



/* 返回请求 Trailer 数量。 */
XRT_API size_t xrtHttpServerRequestTrailerCount(
	const xhttpserverrequest* pRequest
);



/* 返回连续只读 Trailer 数组；空请求或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailerData(
	const xhttpserverrequest* pRequest
);



/* 返回指定位置的借用 Trailer，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpServerRequestTrailerAt(
	const xhttpserverrequest* pRequest,
	size_t iIndex
);



#if defined(XRT_FEATURE_HTTP_SERVER_QUERY)
/* 解析 request-target 的查询组件并创建拥有型 QueryParams。 */
XRT_API xqueryparams* xrtHttpServerRequestQueryParams(
	const xhttpserverrequest* pRequest,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
);
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_COOKIE)
/* 严格校验全部 Cookie 字段并一次写出借用项；Cookies 为空时查询总数。 */
XRT_API bool xrtHttpServerRequestCookies(
	const xhttpserverrequest* pRequest,
	xcookiepair* pCookies,
	size_t iCapacity,
	size_t* pCount
);



/* 严格校验全部 Cookie 字段并返回首个同名借用项；END 或 ERROR 发布空结果。 */
XRT_API xcookienext xrtHttpServerRequestCookie(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	xcookiepair* pCookie
);
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_CONTENT_TYPE)
/* 解析唯一 Content-Type；字段缺失返回 END，重复或无效返回 ERROR。 */
XRT_API xhttpnext xrtHttpServerRequestContentType(
	const xhttpserverrequest* pRequest,
	xmediatype* pType
);
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_FORM)
/* 校验媒体类型并把完整缓冲的 urlencoded 正文解析为 QueryParams。 */
XRT_API xqueryparams* xrtHttpServerRequestForm(
	const xhttpserverrequest* pRequest,
	const xqueryparamsconfig* pConfig,
	size_t* pErrorOffset
);
#endif



#if defined(XRT_FEATURE_HTTP_SERVER_FORM_DATA)
/* 校验媒体类型并把完整缓冲的 multipart 正文解析为拥有型 FormData。 */
XRT_API xformdata* xrtHttpServerRequestFormData(
	const xhttpserverrequest* pRequest,
	const xformdataconfig* pConfig,
	const xmultipartlimits* pLimits,
	xmultiparterrorinfo* pError
);
#endif

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY)

/*
	初始化按需分配 Header 与 Trailer 的默认 Reply 配置。
	配置对象只要求位于完整可访问的存储范围，不要求自然对齐。
*/
XRT_API void xrtHttpReplyConfigInit(xhttpreplyconfig* pConfig);



/* 创建使用标准原因短语且正文为空的 Reply。 */
XRT_API xhttpreply* xrtHttpReplyCreate(uint16 iStatus);



/*
	使用独立 Header 与 Trailer 限额创建 Reply。
	函数立即快照配置；调用返回后配置存储可修改或释放。
*/
XRT_API xhttpreply* xrtHttpReplyCreateWithConfig(
	uint16 iStatus,
	const xhttpreplyconfig* pConfig
);



/*
	克隆状态、原因短语和字段存储，并增加正文引用。
	克隆结果与源对象可以独立修改和销毁。
*/
XRT_API xhttpreply* xrtHttpReplyClone(const xhttpreply* pReply);



/* 销毁 Reply 及其字段、原因短语和正文引用；空指针是安全的空操作。 */
XRT_API void xrtHttpReplyDestroy(xhttpreply* pReply);



/* 设置三位状态码，并恢复该状态的标准原因短语。 */
XRT_API bool xrtHttpReplySetStatus(
	xhttpreply* pReply,
	uint16 iStatus
);



/* 返回 Reply 状态码；空 Reply 返回零。 */
XRT_API uint16 xrtHttpReplyStatus(const xhttpreply* pReply);



/* 失败原子地设置自定义原因短语；空文本会显式写出空原因短语。 */
XRT_API bool xrtHttpReplySetReason(
	xhttpreply* pReply,
	xstrview Reason
);



/* 返回借用的有效原因短语。 */
XRT_API xstrview xrtHttpReplyReason(const xhttpreply* pReply);



/* 追加一个拥有型 Header，并保留同名字段。 */
XRT_API bool xrtHttpReplyAddHeader(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
);



/* 设置首个同名 Header，并删除其余同名字段。 */
XRT_API bool xrtHttpReplySetHeader(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
);



/* 删除全部同名 Header 并返回删除数量。 */
XRT_API size_t xrtHttpReplyRemoveHeader(
	xhttpreply* pReply,
	xstrview Name
);



/* 返回首个同名借用 Header，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyHeader(
	const xhttpreply* pReply,
	xstrview Name
);



/* 返回 Header 数量。 */
XRT_API size_t xrtHttpReplyHeaderCount(const xhttpreply* pReply);



/* 返回连续只读 Header 数组；空 Reply 或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyHeaderData(
	const xhttpreply* pReply
);



/* 返回指定位置的借用 Header，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyHeaderAt(
	const xhttpreply* pReply,
	size_t iIndex
);



/*
	返回借用的只读 Header 容器。
	从未添加字段时返回空指针，查询不会触发分配。
*/
XRT_API const xhttpheaders* xrtHttpReplyHeaders(
	const xhttpreply* pReply
);



/*
	返回可修改 Header 容器并在首次调用时创建它。
	容器由 Reply 拥有，不能单独销毁。
*/
XRT_API xhttpheaders* xrtHttpReplyEditHeaders(xhttpreply* pReply);



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH)

/* 校验并追加一条 WWW-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddChallenge(
	xhttpreply* pReply,
	xstrview Scheme,
	xstrview Data
);



/* 校验并追加一条 Proxy-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddProxyChallenge(
	xhttpreply* pReply,
	xstrview Scheme,
	xstrview Data
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BASIC)

/* 追加带安全转义 realm 的 Basic WWW-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddBasicChallenge(
	xhttpreply* pReply,
	xstrview Realm,
	bool bUtf8
);



/* 追加带安全转义 realm 的 Basic Proxy-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddProxyBasicChallenge(
	xhttpreply* pReply,
	xstrview Realm,
	bool bUtf8
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_BEARER)

/* 校验并追加一条 Bearer WWW-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddBearerChallenge(
	xhttpreply* pReply,
	const xhttpbearerchallenge* pChallenge
);



/* 校验并追加一条 Bearer Proxy-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddProxyBearerChallenge(
	xhttpreply* pReply,
	const xhttpbearerchallenge* pChallenge
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST)

/* 追加一条规范 Digest WWW-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddDigestChallenge(
	xhttpreply* pReply,
	const xhttpdigestchallenge* pChallenge
);



/* 追加一条规范 Digest Proxy-Authenticate challenge。 */
XRT_API bool xrtHttpReplyAddProxyDigestChallenge(
	xhttpreply* pReply,
	const xhttpdigestchallenge* pChallenge
);

#endif



#if defined(XRT_FEATURE_HTTP_SERVER_REPLY_AUTH_DIGEST_INFO)

/* 设置唯一 Authentication-Info 字段。 */
XRT_API bool xrtHttpReplySetDigestInfo(
	xhttpreply* pReply,
	const xhttpdigestinfo* pInfo
);



/* 设置唯一 Proxy-Authentication-Info 字段。 */
XRT_API bool xrtHttpReplySetProxyDigestInfo(
	xhttpreply* pReply,
	const xhttpdigestinfo* pInfo
);

#endif



/* 追加一个拥有型 Trailer，并保留同名字段。 */
XRT_API bool xrtHttpReplyAddTrailer(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
);



/* 设置首个同名 Trailer，并删除其余同名字段。 */
XRT_API bool xrtHttpReplySetTrailer(
	xhttpreply* pReply,
	xstrview Name,
	xstrview Value
);



/* 删除全部同名 Trailer 并返回删除数量。 */
XRT_API size_t xrtHttpReplyRemoveTrailer(
	xhttpreply* pReply,
	xstrview Name
);



/* 返回首个同名借用 Trailer，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyTrailer(
	const xhttpreply* pReply,
	xstrview Name
);



/* 返回 Trailer 数量。 */
XRT_API size_t xrtHttpReplyTrailerCount(const xhttpreply* pReply);



/* 返回连续只读 Trailer 数组；空 Reply 或无字段返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyTrailerData(
	const xhttpreply* pReply
);



/* 返回指定位置的借用 Trailer，越界返回空指针。 */
XRT_API const xhttpfield* xrtHttpReplyTrailerAt(
	const xhttpreply* pReply,
	size_t iIndex
);



/*
	返回借用的只读 Trailer 容器。
	从未添加字段时返回空指针，查询不会触发分配。
*/
XRT_API const xhttpheaders* xrtHttpReplyTrailers(
	const xhttpreply* pReply
);



/*
	返回可修改 Trailer 容器并在首次调用时创建它。
	协议准备层仍会拒绝禁止作为 Trailer 的字段。
*/
XRT_API xhttpheaders* xrtHttpReplyEditTrailers(xhttpreply* pReply);



/*
	替换正文并保留一个独立引用；空 Body 清除正文。
	该函数不自动修改 Content-Type、Content-Length 或 Transfer-Encoding。
*/
XRT_API bool xrtHttpReplySetBody(
	xhttpreply* pReply,
	xhttpbody* pBody
);



/*
	复制字节并设置正文；ContentType 非空时同时失败原子地设置 Content-Type。
	正文长度和线路分帧仍由后续协议准备层生成。
*/
XRT_API bool xrtHttpReplySetBytes(
	xhttpreply* pReply,
	xbytesview Data,
	xstrview ContentType
);



/* 返回借用的正文对象，可能为空。 */
XRT_API xhttpbody* xrtHttpReplyBody(const xhttpreply* pReply);

#endif



XRT_EXTERN_C_END

#endif
