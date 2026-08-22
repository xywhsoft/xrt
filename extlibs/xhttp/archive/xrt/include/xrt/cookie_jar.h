#ifndef XRT_COOKIE_JAR_H
#define XRT_COOKIE_JAR_H

#include <xrt/core.h>
#include <xrt/memory.h>

#if defined(XRT_FEATURE_COOKIE_JAR)
	#include <xrt/cookie.h>
	#include <xrt/sync.h>
	#include <xrt/time.h>
	#include <xrt/url.h>
#endif

#if defined(XRT_FEATURE_COOKIE_JAR_HEADERS)
	#include <xrt/http.h>
#endif



#if defined(XRT_FEATURE_COOKIE_JAR) && \
	(!defined(XRT_FEATURE_SET_COOKIE) || !defined(XRT_FEATURE_URL) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT cookie jar requires set_cookie, url and mutex"
#endif

#if defined(XRT_FEATURE_COOKIE_JAR_HEADERS) && \
	(!defined(XRT_FEATURE_COOKIE_JAR) || !defined(XRT_FEATURE_HTTP_HEADERS))
	#error "XRT cookie jar headers require cookie_jar and http_headers"
#endif



#if defined(XRT_FEATURE_COOKIE_JAR)

/* RFC 10025 要求用户代理把持久 Cookie 的生存期限制在最多 400 天。 */
#define XCOOKIE_JAR_MAX_LIFETIME \
	((xtime)(INT64_C(400) * XRT_TIME_DAY))



/* CookieJar 是引用计数、线程安全的用户代理 Cookie 存储。 */
typedef struct xcookiejar xcookiejar;



/* CookieSnapshot 拥有不可变文本，借用视图在快照销毁前保持有效。 */
typedef struct xcookiesnapshot xcookiesnapshot;



/* 回调接收已经转为小写 ASCII 的域名；返回 true 表示公共后缀。 */
typedef bool (*xcookiepublicsuffixfn)(ptr pContext, xstrview Domain);



/* 没有公共后缀回调时，显式允许可跨子域的 Domain Cookie。 */
#define XCOOKIE_JAR_ALLOW_UNVERIFIED_DOMAIN UINT32_C(0x00000001)



/* CookieJar 限额全部约束有效数据；零值配置无效，应先调用 ConfigInit。 */
typedef struct xcookiejarconfig {
	uint32 Flags;
	size_t InitialCookies;
	size_t MaxCookies;
	size_t MaxCookiesPerDomain;
	size_t MaxCookieBytes;
	size_t MaxNameBytes;
	size_t MaxValueBytes;
	size_t MaxDomainBytes;
	size_t MaxPathBytes;
	size_t MaxPartitionKeyBytes;

	/* Default SameSite 的跨站顶层非安全请求兼容窗口；零表示禁用。 */
	xtime LaxUnsafeAge;

	/* 公共后缀策略是数据驱动的真实扩展点，回调上下文由调用方管理。 */
	xcookiepublicsuffixfn IsPublicSuffix;
	ptr PublicSuffixContext;
} xcookiejarconfig;



#define XCOOKIE_STORE_HTTP_API	UINT32_C(0x00000001)
#define XCOOKIE_STORE_HAS_NOW	UINT32_C(0x00000002)
#define XCOOKIE_STORE_SAME_SITE	UINT32_C(0x00000004)
#define XCOOKIE_STORE_TOP_LEVEL	UINT32_C(0x00000008)

/* 接收上下文显式表达来源、站点关系和分区键，URL 必须是 HTTP(S) 或 WS(S) 绝对 URL。 */
typedef struct xcookiestorecontext {
	uint32 Flags;
	xstrview URL;
	xstrview PartitionKey;
	xtime Now;
} xcookiestorecontext;



#define XCOOKIE_REQUEST_HTTP_API		UINT32_C(0x00000001)
#define XCOOKIE_REQUEST_HAS_NOW		UINT32_C(0x00000002)
#define XCOOKIE_REQUEST_SAME_SITE	UINT32_C(0x00000004)
#define XCOOKIE_REQUEST_TOP_LEVEL	UINT32_C(0x00000008)
#define XCOOKIE_REQUEST_SAFE_METHOD	UINT32_C(0x00000010)

/* 请求上下文由上层导航或调用模型给出，CookieJar 不猜测跨站关系。 */
typedef struct xcookierequestcontext {
	uint32 Flags;
	xstrview URL;
	xstrview PartitionKey;
	xtime Now;
} xcookierequestcontext;



/* 策略拒绝不是运行时错误，调用方可稳定区分具体原因。 */
typedef enum xcookiereject {
	XCOOKIE_REJECT_NONE = 0,
	XCOOKIE_REJECT_SYNTAX,
	XCOOKIE_REJECT_LIMIT,
	XCOOKIE_REJECT_EMPTY,
	XCOOKIE_REJECT_DOMAIN,
	XCOOKIE_REJECT_PUBLIC_SUFFIX,
	XCOOKIE_REJECT_SECURE,
	XCOOKIE_REJECT_HTTP_ONLY,
	XCOOKIE_REJECT_SAME_SITE,
	XCOOKIE_REJECT_PREFIX,
	XCOOKIE_REJECT_PARTITION,
	XCOOKIE_REJECT_SECURE_OVERWRITE
} xcookiereject;



/* Store 结果把策略拒绝、有效更新、删除和无变化分开。 */
typedef enum xcookiestorestatus {
	XCOOKIE_STORE_ERROR = -1,
	XCOOKIE_STORE_REJECTED = 0,
	XCOOKIE_STORE_STORED = 1,
	XCOOKIE_STORE_REMOVED = 2,
	XCOOKIE_STORE_IGNORED = 3
} xcookiestorestatus;



#define XCOOKIE_INFO_HOST_ONLY		UINT32_C(0x00000001)
#define XCOOKIE_INFO_SECURE			UINT32_C(0x00000002)
#define XCOOKIE_INFO_HTTP_ONLY		UINT32_C(0x00000004)
#define XCOOKIE_INFO_PERSISTENT		UINT32_C(0x00000008)
#define XCOOKIE_INFO_PARTITIONED	UINT32_C(0x00000010)

/* CookieInfo 只借用所属快照，时间使用与 xrt 时间模块一致的微秒。 */
typedef struct xcookieinfo {
	uint32 Flags;
	xcookiesamesite SameSite;
	xcookiepriority Priority;
	xtime Expires;
	xtime Created;
	xtime Accessed;
	xstrview Name;
	xstrview Value;
	xstrview Domain;
	xstrview Path;
	xstrview PartitionKey;
} xcookieinfo;

#endif



#if defined(XRT_FEATURE_COOKIE_JAR_HEADERS)

/* 批量接收 Set-Cookie 字段时返回逐类计数，错误前已经提交的字段不会回滚。 */
typedef struct xcookiestorereport {
	size_t Fields;
	size_t Stored;
	size_t Removed;
	size_t Ignored;
	size_t Rejected;
} xcookiestorereport;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_COOKIE_JAR)

/* 初始化安全、适合通用 HTTP 客户端的配置；输出可位于合法的未对齐存储。 */
XRT_API void xrtCookieJarConfigInit(xcookiejarconfig* pConfig);



/* 创建线程安全 CookieJar；配置为空时使用默认值，非空配置会立即复制。 */
XRT_API xcookiejar* xrtCookieJarCreate(const xcookiejarconfig* pConfig);



/* 增加 CookieJar 引用；失效对象返回空指针。 */
XRT_API xcookiejar* xrtCookieJarRetain(const xcookiejar* pJar);



/* 释放 CookieJar 引用；空指针是安全的空操作。 */
XRT_API void xrtCookieJarRelease(xcookiejar* pJar);



/* 删除全部 Cookie，但保留已经分配的数组容量。 */
XRT_API void xrtCookieJarClear(xcookiejar* pJar);



/* 返回当前条目数；不会隐式清理过期条目。 */
XRT_API size_t xrtCookieJarCount(const xcookiejar* pJar);



/* 删除在指定时间已经过期的 Cookie，并返回删除数量。 */
XRT_API size_t xrtCookieJarPurge(xcookiejar* pJar, xtime iNow);



/* 使用已解析 Set-Cookie 更新存储；策略拒绝通过 Reject 返回。 */
XRT_API xcookiestorestatus xrtCookieJarSet(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	xcookiereject* pReject
);



/* 宽松解析 Set-Cookie 字段值后更新存储。 */
XRT_API xcookiestorestatus xrtCookieJarStore(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	xstrview SetCookie,
	xcookiereject* pReject
);



/* 使用当前时间和 HTTP API 语义接收一个普通、非分区 Cookie。 */
XRT_API xcookiestorestatus xrtCookieJarStoreUrl(
	xcookiejar* pJar,
	xstrview URL,
	xstrview SetCookie,
	xcookiereject* pReject
);



/* 按请求上下文写出不含字段名和零结尾的 Cookie 字段值。 */
XRT_API bool xrtCookieJarWrite(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 按请求上下文分配并构建零结尾 Cookie 字段值。 */
XRT_API str xrtCookieJarBuild(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	size_t* pSize
);



/* 使用当前时间、同站和 HTTP API 语义写出普通请求 Cookie。 */
XRT_API bool xrtCookieJarWriteUrl(
	xcookiejar* pJar,
	xstrview URL,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 使用当前时间、同站和 HTTP API 语义构建普通请求 Cookie。 */
XRT_API str xrtCookieJarBuildUrl(
	xcookiejar* pJar,
	xstrview URL,
	size_t* pSize
);



/* 在锁内复制全部未过期条目，返回并发稳定的不可变快照。 */
XRT_API xcookiesnapshot* xrtCookieJarSnapshot(
	xcookiejar* pJar,
	xtime iNow
);



/* 销毁快照；空指针是安全的空操作。 */
XRT_API void xrtCookieSnapshotDestroy(xcookiesnapshot* pSnapshot);



/* 返回快照中的 Cookie 数量。 */
XRT_API size_t xrtCookieSnapshotCount(const xcookiesnapshot* pSnapshot);



/* 返回稳定借用条目，越界返回空指针且不设置错误。 */
XRT_API const xcookieinfo* xrtCookieSnapshotAt(
	const xcookiesnapshot* pSnapshot,
	size_t iIndex
);

#endif



#if defined(XRT_FEATURE_COOKIE_JAR_HEADERS)

/* 分别处理每个 Set-Cookie 字段，绝不把同名字段合并。 */
XRT_API bool xrtCookieJarStoreHeaders(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xhttpheaders* pHeaders,
	xcookiestorereport* pReport
);



/* 构建并设置单个 Cookie 字段；没有可发送条目时删除已有字段。 */
XRT_API bool xrtCookieJarApply(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	xhttpheaders* pHeaders
);

#endif



XRT_EXTERN_C_END

#endif
