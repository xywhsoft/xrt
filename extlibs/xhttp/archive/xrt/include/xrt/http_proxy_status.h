#ifndef XRT_HTTP_PROXY_STATUS_H
#define XRT_HTTP_PROXY_STATUS_H

#include <xrt/http_structured.h>



#if defined(XRT_FEATURE_HTTP_PROXY_STATUS) && \
	!defined(XRT_FEATURE_HTTP_STRUCTURED)
	#error "XRT HTTP Proxy-Status requires Structured Fields support"
#endif

#if defined(XRT_FEATURE_HTTP_PROXY_STATUS_WRITE) && \
	(!defined(XRT_FEATURE_HTTP_PROXY_STATUS) || \
	 !defined(XRT_FEATURE_HTTP_STRUCTURED_WRITE))
	#error "XRT HTTP Proxy-Status writer requires parser and Structured Fields writer support"
#endif

#if defined(XRT_FEATURE_HTTP_PROXY_ALIAS) && \
	(!defined(XRT_FEATURE_HTTP_PROXY_STATUS) || \
	 !defined(XRT_FEATURE_CODEC_PERCENT))
	#error "XRT HTTP proxy alias parser requires Proxy-Status and percent codec support"
#endif

#if defined(XRT_FEATURE_HTTP_PROXY_ALIAS_WRITE) && \
	!defined(XRT_FEATURE_HTTP_PROXY_ALIAS)
	#error "XRT HTTP proxy alias writer requires proxy alias parser support"
#endif



#if defined(XRT_FEATURE_HTTP_PROXY_STATUS)

/* ALPN 协议标识的最大字节数。 */
#define XHTTP_PROXY_ALPN_MAX 255u



/* Flags 表示类型正确的已知参数，InvalidFlags 使用相同位。 */
typedef enum xhttpproxystatusflag {
	XHTTP_PROXY_STATUS_HAS_ERROR = 0x0001,
	XHTTP_PROXY_STATUS_HAS_NEXT_HOP = 0x0002,
	XHTTP_PROXY_STATUS_HAS_NEXT_PROTOCOL = 0x0004,
	XHTTP_PROXY_STATUS_HAS_RECEIVED_STATUS = 0x0008,
	XHTTP_PROXY_STATUS_HAS_DETAILS = 0x0010,
	XHTTP_PROXY_STATUS_HAS_NEXT_HOP_ALIASES = 0x0020
} xhttpproxystatusflag;



/* 一个 Proxy-Status 成员；所有文本视图借用原字段值。 */
typedef struct xhttpproxystatus {
	xhttpstructuredbare Proxy;
	xhttpstructuredbare Error;
	xhttpstructuredbare NextHop;
	xhttpstructuredbare NextProtocol;
	xhttpstructuredbare Details;
	xhttpstructuredbare NextHopAliases;
	xstrview Parameters;
	int64 ReceivedStatus;
	uint16 Flags;
	uint16 InvalidFlags;
} xhttpproxystatus;



/* 单字段游标绑定首次迭代的不可变字段值，调用方不得直接修改。 */
typedef struct xhttpproxystatuscursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 Validated;
} xhttpproxystatuscursor;



/* 重复字段游标独立持有通用 Structured Fields 状态。 */
typedef struct xhttpproxystatusfieldcursor {
	xhttpstructuredfieldcursor Structured;
	uint8 Validated;
} xhttpproxystatusfieldcursor;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Proxy-Status 字段值游标。 */
XRT_API void xrtHttpProxyStatusCursorInit(
	xhttpproxystatuscursor* pCursor
);



/* 初始化跨重复 Proxy-Status 字段行游标。 */
XRT_API void xrtHttpProxyStatusFieldCursorInit(
	xhttpproxystatusfieldcursor* pCursor
);



/* 严格验证完整 Proxy-Status 字段值和每个代理标识。 */
XRT_API bool xrtHttpProxyStatusValid(xstrview Value);



/* 迭代一个完整 Proxy-Status 字段值。 */
XRT_API xhttpnext xrtHttpProxyStatusNext(
	xstrview Value,
	xhttpproxystatuscursor* pCursor,
	xhttpproxystatus* pStatus
);



/* 按线路顺序跨越全部重复 Proxy-Status 字段行。 */
XRT_API xhttpnext xrtHttpProxyStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpproxystatusfieldcursor* pCursor,
	xhttpproxystatus* pStatus
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_HTTP_PROXY_ALIAS)

/* RFC 9532 别名游标绑定首次迭代的不可变列表。 */
typedef struct xhttpproxyaliascursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 Validated;
} xhttpproxyaliascursor;



XRT_EXTERN_C_BEGIN



/* 初始化 next-hop-aliases 列表游标。 */
XRT_API void xrtHttpProxyAliasCursorInit(
	xhttpproxyaliascursor* pCursor
);



/* 严格验证完整 RFC 9532 别名列表；空文本表示没有别名。 */
XRT_API bool xrtHttpProxyAliasesValid(xstrview Aliases);



/* 零拷贝迭代一个已编码别名，首次调用先验证完整列表。 */
XRT_API xhttpnext xrtHttpProxyAliasNext(
	xstrview Aliases,
	xhttpproxyaliascursor* pCursor,
	xstrview* pAlias
);



/* 百分号解码一个已编码别名；保留 RFC 9532 的反斜杠表示。 */
XRT_API bool xrtHttpProxyAliasRead(
	xstrview Alias,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_HTTP_PROXY_ALIAS_WRITE)

XRT_EXTERN_C_BEGIN



/* 把一个 RFC 9532 展示形式名称写为百分号编码别名。 */
XRT_API bool xrtHttpProxyAliasWrite(
	xstrview Alias,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 把名称数组写为逗号分隔的 next-hop-aliases String 正文。 */
XRT_API bool xrtHttpProxyAliasesWrite(
	const xstrview* pAliases,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾别名列表；返回值由 xrtFree 释放。 */
XRT_API str xrtHttpProxyAliasesBuild(
	const xstrview* pAliases,
	size_t iCount,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_HTTP_PROXY_STATUS_WRITE)

XRT_EXTERN_C_BEGIN



/* 规范写出一个 Proxy-Status 成员，适合追加独立字段行。 */
XRT_API bool xrtHttpProxyStatusWrite(
	const xhttpstructureditemvalue* pStatus,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
