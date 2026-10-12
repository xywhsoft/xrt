/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xhttp_policy requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XHTTP_POLICY_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XHTTP_POLICY_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XHTTP_POLICY_SINGLE_HEADER_H
#define XHTTP_POLICY_SINGLE_HEADER_H
#define XHTTP_POLICY_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xhttp-policy/include/xhttp-policy/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XHTTP_POLICY_FEATURES_H
#define XHTTP_POLICY_FEATURES_H

/* http_redirect_policy 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_REDIRECT_POLICY)
#ifndef XHTTP_FEATURE_HTTP_REDIRECT_POLICY
#define XHTTP_FEATURE_HTTP_REDIRECT_POLICY
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_REQUEST
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_REQUEST_TRAILERS
#define XHTTP_MODULE_HTTP_CLIENT_REQUEST_TRAILERS
#endif
#ifndef XHTTP_MODULE_URL
#define XHTTP_MODULE_URL
#endif
#endif

/* http_retry_policy 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY)
#ifndef XHTTP_FEATURE_HTTP_RETRY_POLICY
#define XHTTP_FEATURE_HTTP_RETRY_POLICY
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* cookie_jar_headers 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_COOKIE_JAR_HEADERS)
#ifndef XHTTP_FEATURE_COOKIE_JAR_HEADERS
#define XHTTP_FEATURE_COOKIE_JAR_HEADERS
#endif
#ifndef XHTTP_POLICY_MODULE_COOKIE_JAR
#define XHTTP_POLICY_MODULE_COOKIE_JAR
#endif
#ifndef XHTTP_MODULE_HTTP_HEADERS
#define XHTTP_MODULE_HTTP_HEADERS
#endif
#endif

/* cookie_jar 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_COOKIE_JAR)
#ifndef XHTTP_FEATURE_COOKIE_JAR
#define XHTTP_FEATURE_COOKIE_JAR
#endif
#ifndef XHTTP_MODULE_SET_COOKIE
#define XHTTP_MODULE_SET_COOKIE
#endif
#ifndef XHTTP_MODULE_URL
#define XHTTP_MODULE_URL
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_client_response_auth_digest_session 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION
#define XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SESSION
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SESSION
#endif
#endif

/* http_client_response_auth_digest_choose 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE
#define XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST
#define XHTTP_MODULE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_CLIENT
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_CLIENT
#endif
#endif

/* http_client_prepare_auth_digest_session 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION
#define XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION
#endif
#ifndef XHTTP_MODULE_HTTP_CLIENT_PREPARE
#define XHTTP_MODULE_HTTP_CLIENT_PREPARE
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SESSION
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SESSION
#endif
#endif

/* http_auth_digest_replay 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_REPLAY)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST
#define XHTTP_MODULE_HTTP_AUTH_DIGEST
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_auth_digest_session 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SESSION)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_CLIENT
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_CLIENT
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_auth_digest_client 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_CLIENT)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST_CHALLENGE
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_CHALLENGE
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST_CREDENTIALS
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_CREDENTIALS
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST_INFO
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_INFO
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#endif
#endif

/* http_auth_digest_verify 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_VERIFY)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST_CHALLENGE
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_CHALLENGE
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST_CREDENTIALS
#define XHTTP_MODULE_HTTP_AUTH_DIGEST_CREDENTIALS
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE
#endif
#endif

/* http_auth_digest_nonce_random 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE_RANDOM)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* http_auth_digest_nonce 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_NONCE)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST
#define XHTTP_MODULE_HTTP_AUTH_DIGEST
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#ifndef XRT_MODULE_CRYPTO_HMAC_SHA256
#define XRT_MODULE_CRYPTO_HMAC_SHA256
#endif
#endif

/* http_auth_digest_md5 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_MD5)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#define XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2
#endif
#ifndef XRT_MODULE_CRYPTO_MD5
#define XRT_MODULE_CRYPTO_MD5
#endif
#endif

/* http_auth_digest_sha2 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_AUTH_DIGEST_SHA2)
#ifndef XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2
#define XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2
#endif
#ifndef XHTTP_MODULE_HTTP_AUTH_DIGEST
#define XHTTP_MODULE_HTTP_AUTH_DIGEST
#endif
#ifndef XRT_MODULE_CRYPTO_SHA256
#define XRT_MODULE_CRYPTO_SHA256
#endif
#ifndef XRT_MODULE_CRYPTO_SHA512_256
#define XRT_MODULE_CRYPTO_SHA512_256
#endif
#endif

/* http_cache_store 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CACHE_STORE)
#ifndef XHTTP_FEATURE_HTTP_CACHE_STORE
#define XHTTP_FEATURE_HTTP_CACHE_STORE
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_CACHE_VALIDATE
#define XHTTP_POLICY_MODULE_HTTP_CACHE_VALIDATE
#endif
#ifndef XHTTP_MODULE_HTTP_VARY
#define XHTTP_MODULE_HTTP_VARY
#endif
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_cache_range 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CACHE_RANGE)
#ifndef XHTTP_FEATURE_HTTP_CACHE_RANGE
#define XHTTP_FEATURE_HTTP_CACHE_RANGE
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_CACHE_VALIDATE
#define XHTTP_POLICY_MODULE_HTTP_CACHE_VALIDATE
#endif
#ifndef XHTTP_MODULE_HTTP_RANGE
#define XHTTP_MODULE_HTTP_RANGE
#endif
#endif

/* http_cache_validate 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CACHE_VALIDATE)
#ifndef XHTTP_FEATURE_HTTP_CACHE_VALIDATE
#define XHTTP_FEATURE_HTTP_CACHE_VALIDATE
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_CACHE_POLICY
#define XHTTP_POLICY_MODULE_HTTP_CACHE_POLICY
#endif
#ifndef XHTTP_MODULE_HTTP_FORWARD
#define XHTTP_MODULE_HTTP_FORWARD
#endif
#ifndef XHTTP_MODULE_HTTP_ORIGIN
#define XHTTP_MODULE_HTTP_ORIGIN
#endif
#ifndef XHTTP_MODULE_HTTP_PRECONDITION
#define XHTTP_MODULE_HTTP_PRECONDITION
#endif
#ifndef XHTTP_MODULE_URL
#define XHTTP_MODULE_URL
#endif
#endif

/* http_cache_policy 及其直接依赖。 */
#if defined(XHTTP_POLICY_MODULE_ALL) || defined(XHTTP_POLICY_MODULE_HTTP_CACHE_POLICY)
#ifndef XHTTP_FEATURE_HTTP_CACHE_POLICY
#define XHTTP_FEATURE_HTTP_CACHE_POLICY
#endif
#ifndef XHTTP_MODULE_HTTP_CACHE_TIME
#define XHTTP_MODULE_HTTP_CACHE_TIME
#endif
#endif

#endif /* XHTTP_POLICY_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xhttp-policy/export.h */
/* ========================================================================== */

#ifndef XHTTP_POLICY_EXPORT_H
#define XHTTP_POLICY_EXPORT_H


/* 独立动态库仅导出本库实现，依赖库由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XHTTP_POLICY_BUILD_SHARED)
  #define XHTTP_POLICY_API __declspec(dllexport)
 #elif defined(XHTTP_POLICY_USE_SHARED)
  #define XHTTP_POLICY_API __declspec(dllimport)
 #else
  #define XHTTP_POLICY_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XHTTP_POLICY_BUILD_SHARED)
  #define XHTTP_POLICY_API __attribute__((visibility("default")))
 #else
  #define XHTTP_POLICY_API
 #endif
#else
 #define XHTTP_POLICY_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_cache_policy.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CACHE_POLICY_H
#define XRT_HTTP_CACHE_POLICY_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) && \
	!defined(XHTTP_FEATURE_HTTP_CACHE_TIME)
	#error "XRT HTTP cache policy requires HTTP cache time support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

/* 策略原因位用于解释存储、复用、验证和转发决定。 */
typedef enum xhttpcachepolicyreason {
	XHTTP_CACHE_REASON_NONE = 0,
	XHTTP_CACHE_REASON_METHOD_UNKNOWN = UINT32_C(0x00000001),
	XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE = UINT32_C(0x00000002),
	XHTTP_CACHE_REASON_STATUS_NOT_FINAL = UINT32_C(0x00000004),
	XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD = UINT32_C(0x00000008),
	XHTTP_CACHE_REASON_HEADERS_INCOMPLETE = UINT32_C(0x00000010),
	XHTTP_CACHE_REASON_REQUEST_NO_STORE = UINT32_C(0x00000020),
	XHTTP_CACHE_REASON_RESPONSE_NO_STORE = UINT32_C(0x00000040),
	XHTTP_CACHE_REASON_SHARED_PRIVATE = UINT32_C(0x00000080),
	XHTTP_CACHE_REASON_AUTHORIZATION = UINT32_C(0x00000100),
	XHTTP_CACHE_REASON_NO_PERMISSION = UINT32_C(0x00000200),
	XHTTP_CACHE_REASON_POST_REQUIREMENTS = UINT32_C(0x00000400),
	XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED = UINT32_C(0x00000800),
	XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE = UINT32_C(0x00001000),
	XHTTP_CACHE_REASON_CANDIDATE_MISS = UINT32_C(0x00002000),
	XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE = UINT32_C(0x00004000),
	XHTTP_CACHE_REASON_NO_CLOCK = UINT32_C(0x00008000),
	XHTTP_CACHE_REASON_REQUEST_REVALIDATE = UINT32_C(0x00010000),
	XHTTP_CACHE_REASON_RESPONSE_REVALIDATE = UINT32_C(0x00020000),
	XHTTP_CACHE_REASON_STALE = UINT32_C(0x00040000),
	XHTTP_CACHE_REASON_ONLY_IF_CACHED = UINT32_C(0x00080000),
	XHTTP_CACHE_REASON_DISCONNECTED = UINT32_C(0x00100000),
	XHTTP_CACHE_REASON_EXTENSION = UINT32_C(0x00200000)
} xhttpcachepolicyreason;



/* 存储输入把协议无法自行观察的缓存能力和请求事实显式化。 */
typedef enum xhttpcachestoreflag {
	XHTTP_CACHE_STORE_NONE = 0,
	XHTTP_CACHE_STORE_SHARED = UINT32_C(0x00000001),
	XHTTP_CACHE_STORE_AUTHORIZATION = UINT32_C(0x00000002),
	XHTTP_CACHE_STORE_METHOD_UNDERSTOOD = UINT32_C(0x00000004),
	XHTTP_CACHE_STORE_METHOD_CACHEABLE = UINT32_C(0x00000008),
	XHTTP_CACHE_STORE_STATUS_UNDERSTOOD = UINT32_C(0x00000010),
	XHTTP_CACHE_STORE_STATUS_HEURISTIC = UINT32_C(0x00000020),
	XHTTP_CACHE_STORE_HEADERS_COMPLETE = UINT32_C(0x00000040),
	XHTTP_CACHE_STORE_RESPONSE_COMPLETE = UINT32_C(0x00000080),
	XHTTP_CACHE_STORE_RANGE_SUPPORTED = UINT32_C(0x00000100),
	XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH = UINT32_C(0x00000200),
	XHTTP_CACHE_STORE_EXTENSION = UINT32_C(0x00000400),
	XHTTP_CACHE_STORE_EXTENSION_FRESHNESS = UINT32_C(0x00000800),
	XHTTP_CACHE_STORE_EXTENSION_OVERRIDE = UINT32_C(0x00001000)
} xhttpcachestoreflag;



/* 存储动作描述保存响应前必须由存储实现完成的变换。 */
typedef enum xhttpcachestoreaction {
	XHTTP_CACHE_STORE_ACTION_NONE = 0,
	XHTTP_CACHE_STORE_REMOVE_CONNECTION = UINT32_C(0x00000001),
	XHTTP_CACHE_STORE_REMOVE_PROXY = UINT32_C(0x00000002),
	XHTTP_CACHE_STORE_SEPARATE_TRAILERS = UINT32_C(0x00000004),
	XHTTP_CACHE_STORE_REMOVE_NO_CACHE = UINT32_C(0x00000008),
	XHTTP_CACHE_STORE_REMOVE_PRIVATE = UINT32_C(0x00000010),
	XHTTP_CACHE_STORE_MARK_INCOMPLETE = UINT32_C(0x00000020),
	XHTTP_CACHE_STORE_AS_200 = UINT32_C(0x00000040),
	XHTTP_CACHE_STORE_IGNORE_NO_STORE = UINT32_C(0x00000080)
} xhttpcachestoreaction;



/* 存储决定只区分参数错误、不得保存和可以按动作保存。 */
typedef enum xhttpcachestoredecision {
	XHTTP_CACHE_STORE_ERROR = -1,
	XHTTP_CACHE_STORE_SKIP = 0,
	XHTTP_CACHE_STORE_KEEP = 1
} xhttpcachestoredecision;



/* 方法和状态属于线路事实，其余位描述调用方已经实现的缓存能力。 */
typedef struct xhttpcachestoreinput {
	xstrview Method;
	uint16 Status;
	uint32 Flags;
} xhttpcachestoreinput;



/* 存储计划失败原子地返回决定、全部阻断原因和成功时的必要动作。 */
typedef struct xhttpcachestoreplan {
	xhttpcachestoredecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcachestoreplan;



/* 复用输入由键选择、表示范围、时钟、线路状态和扩展策略共同组成。 */
typedef enum xhttpcacheuseflag {
	XHTTP_CACHE_USE_NONE = 0,
	XHTTP_CACHE_USE_SHARED = UINT32_C(0x00000001),
	XHTTP_CACHE_USE_CANDIDATE_MATCH = UINT32_C(0x00000002),
	XHTTP_CACHE_USE_REPRESENTATION = UINT32_C(0x00000004),
	XHTTP_CACHE_USE_CLOCK = UINT32_C(0x00000008),
	XHTTP_CACHE_USE_VALIDATED = UINT32_C(0x00000010),
	XHTTP_CACHE_USE_DISCONNECTED = UINT32_C(0x00000020),
	XHTTP_CACHE_USE_STALE_ALLOWED = UINT32_C(0x00000040),
	XHTTP_CACHE_USE_EXTENSION = UINT32_C(0x00000080),
	XHTTP_CACHE_USE_STATUS_UNDERSTOOD = UINT32_C(0x00000100),
	XHTTP_CACHE_USE_AUTHORIZATION = UINT32_C(0x00000200)
} xhttpcacheuseflag;



/* 复用动作由缓存输出层执行，不与具体 Header 容器绑定。 */
typedef enum xhttpcacheuseaction {
	XHTTP_CACHE_USE_ACTION_NONE = 0,
	XHTTP_CACHE_USE_SET_AGE = UINT32_C(0x00000001),
	XHTTP_CACHE_USE_REMOVE_NO_CACHE = UINT32_C(0x00000002),
	XHTTP_CACHE_USE_STALE = UINT32_C(0x00000004),
	XHTTP_CACHE_USE_EVICT = UINT32_C(0x00000008)
} xhttpcacheuseaction;



/* 复用决定覆盖命中、源站验证、普通转发和 only-if-cached 的 504。 */
typedef enum xhttpcacheusedecision {
	XHTTP_CACHE_USE_ERROR = -1,
	XHTTP_CACHE_USE_FORWARD = 0,
	XHTTP_CACHE_USE_STORED = 1,
	XHTTP_CACHE_USE_VALIDATE = 2,
	XHTTP_CACHE_USE_GATEWAY_TIMEOUT = 3
} xhttpcacheusedecision;



/* 状态用于 must-understand；Flags 保存候选和运行环境事实。 */
typedef struct xhttpcacheuseinput {
	uint16 Status;
	uint32 Flags;
} xhttpcacheuseinput;



/* StaleBy 使用毫秒；新鲜命中和非复用结果均为零。 */
typedef struct xhttpcacheuseplan {
	uint64 StaleBy;
	xhttpcacheusedecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcacheuseplan;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

/* 判断标准方法是否定义了本模块实现的默认缓存语义。 */
XHTTP_POLICY_API bool xrtHttpCacheMethodDefault(xstrview Method);



/* 判断 RFC 9110 状态是否允许在没有显式寿命时使用启发式新鲜度。 */
XHTTP_POLICY_API bool xrtHttpCacheStatusHeuristic(uint16 iStatus);



/*
	建立完整响应的默认存储输入。
	GET、HEAD、POST 和已注册状态自动获得相应能力位，扩展语义可由调用方修改。
*/
XHTTP_POLICY_API bool xrtHttpCacheStoreInputInit(
	xhttpcachestoreinput* pInput,
	xstrview Method,
	uint16 iStatus,
	bool Shared
);



/*
	按 RFC 9111 建立零分配存储计划。
	请求与响应 Cache-Control 必须分别解析；Time 来自响应 Header。
*/
XHTTP_POLICY_API xhttpcachestoredecision xrtHttpCacheStorePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput,
	xhttpcachestoreplan* pPlan
);



/*
	建立已匹配、表示可用、具有时钟的默认复用输入。
	键匹配必须已经覆盖目标 URI、方法规则和 Vary。
*/
XHTTP_POLICY_API bool xrtHttpCacheUseInputInit(
	xhttpcacheuseinput* pInput,
	uint16 iStatus,
	bool Shared
);



/*
	按请求约束、响应约束、年龄和寿命建立零分配复用计划。
	启发式或扩展寿命由上层构造有效 xhttpcachefreshness 后传入。
*/
XHTTP_POLICY_API xhttpcacheusedecision xrtHttpCacheUsePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness,
	const xhttpcacheuseinput* pInput,
	xhttpcacheuseplan* pPlan
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_cache_validate.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CACHE_VALIDATE_H
#define XRT_HTTP_CACHE_VALIDATE_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) || \
	 !defined(XHTTP_FEATURE_HTTP_FORWARD) || \
	 !defined(XHTTP_FEATURE_HTTP_ORIGIN) || \
	 !defined(XHTTP_FEATURE_HTTP_PRECONDITION) || \
	 !defined(XHTTP_FEATURE_URL))
	#error "XRT HTTP cache validation requires cache policy, forwarding, Origin, precondition and URL support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 缓存条目标志只描述正文完整性和当前 Range 是否可由该条目完整满足。 */
typedef enum xhttpcacheentryflag {
	XHTTP_CACHE_ENTRY_NONE = 0,
	XHTTP_CACHE_ENTRY_PARTIAL = UINT32_C(0x00000001),
	XHTTP_CACHE_ENTRY_RANGE_COVERED = UINT32_C(0x00000002)
} xhttpcacheentryflag;



/*
	缓存条目借用已保存的响应 Header。
	ResponseTime 是收到或成功验证响应时的墙钟，用于缺少 Date 时判断最新条目。
*/
typedef struct xhttpcacheentry {
	const xhttpfield* Fields;
	size_t FieldCount;
	xtime ResponseTime;
	uint32 Flags;
} xhttpcacheentry;



/* 验证请求动作直接对应需要设置或替换的条件字段。 */
typedef enum xhttpcachevalidateaction {
	XHTTP_CACHE_VALIDATE_ACTION_NONE = 0,
	XHTTP_CACHE_VALIDATE_IF_NONE_MATCH = UINT32_C(0x00000001),
	XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE = UINT32_C(0x00000002)
} xhttpcachevalidateaction;



/* 验证请求计划区分参数错误、没有可用验证器和可生成条件请求。 */
typedef enum xhttpcachevalidatedecision {
	XHTTP_CACHE_VALIDATE_ERROR = -1,
	XHTTP_CACHE_VALIDATE_NONE = 0,
	XHTTP_CACHE_VALIDATE_CONDITIONAL = 1
} xhttpcachevalidatedecision;



/*
	ETagSize 是不含零结尾的 If-None-Match 值长度。
	LastModified 仅在对应动作存在时有效。
*/
typedef struct xhttpcachevalidateplan {
	xtime LastModified;
	size_t EligibleCount;
	size_t ETagCount;
	size_t ETagSize;
	uint32 Actions;
	xhttpcachevalidatedecision Decision;
} xhttpcachevalidateplan;



/* If-Range 计划优先使用强 ETag，其次使用已经证明为强验证器的日期。 */
typedef enum xhttpcacheifrangekind {
	XHTTP_CACHE_IF_RANGE_ERROR = -1,
	XHTTP_CACHE_IF_RANGE_NONE = 0,
	XHTTP_CACHE_IF_RANGE_ETAG = 1,
	XHTTP_CACHE_IF_RANGE_DATE = 2
} xhttpcacheifrangekind;



/* If-Range 计划中的 ETag 和日期都借用或复制自缓存条目。 */
typedef struct xhttpcacheifrange {
	xhttpetag ETag;
	xtime Date;
	xhttpcacheifrangekind Kind;
} xhttpcacheifrange;



/* 验证响应分类把 304、完整响应和可按失联处理的 5xx 分开。 */
typedef enum xhttpcachevalidateresult {
	XHTTP_CACHE_VALIDATE_RESULT_ERROR = -1,
	XHTTP_CACHE_VALIDATE_RESULT_FULL = 0,
	XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED = 1,
	XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE = 2
} xhttpcachevalidateresult;



/* 304 选择结果记录实际采用的强、弱或无验证器单条规则。 */
typedef enum xhttpcacheupdatematch {
	XHTTP_CACHE_UPDATE_MATCH_ERROR = -1,
	XHTTP_CACHE_UPDATE_MATCH_NONE = 0,
	XHTTP_CACHE_UPDATE_MATCH_STRONG = 1,
	XHTTP_CACHE_UPDATE_MATCH_WEAK = 2,
	XHTTP_CACHE_UPDATE_MATCH_SINGLE = 3
} xhttpcacheupdatematch;



/* HEAD freshening 只对 200 响应比较验证器和 Content-Length。 */
typedef enum xhttpcacheheaddecision {
	XHTTP_CACHE_HEAD_ERROR = -1,
	XHTTP_CACHE_HEAD_IGNORE = 0,
	XHTTP_CACHE_HEAD_UPDATE = 1,
	XHTTP_CACHE_HEAD_STALE = 2
} xhttpcacheheaddecision;



/*
	字段更新标志由保存实现提供。
	DEPENDENT 表示已保存表示依赖该元数据，PROCESSED 表示该字段已经被消费或移除。
*/
typedef enum xhttpcacheupdatefieldflag {
	XHTTP_CACHE_UPDATE_FIELD_NONE = 0,
	XHTTP_CACHE_UPDATE_FIELD_DEPENDENT = UINT32_C(0x00000001),
	XHTTP_CACHE_UPDATE_FIELD_PROCESSED = UINT32_C(0x00000002)
} xhttpcacheupdatefieldflag;



/* 更新字段计划区分非法输入、规范要求保留旧值和应使用新值替换。 */
typedef enum xhttpcachefieldupdate {
	XHTTP_CACHE_FIELD_UPDATE_ERROR = -1,
	XHTTP_CACHE_FIELD_UPDATE_SKIP = 0,
	XHTTP_CACHE_FIELD_UPDATE_REPLACE = 1
} xhttpcachefieldupdate;



/* 初次保存字段时区分协议错误、必须删除和可以保留。 */
typedef enum xhttpcachefieldstore {
	XHTTP_CACHE_FIELD_STORE_ERROR = -1,
	XHTTP_CACHE_FIELD_STORE_SKIP = 0,
	XHTTP_CACHE_FIELD_STORE_KEEP = 1
} xhttpcachefieldstore;



/* 失效候选来源区分请求目标和两个可选响应字段。 */
typedef enum xhttpcacheinvalidatekind {
	XHTTP_CACHE_INVALIDATE_TARGET = 1,
	XHTTP_CACHE_INVALIDATE_LOCATION,
	XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION
} xhttpcacheinvalidatekind;



/* 失效游标先返回目标 URI，再按线路顺序扫描两个位置字段。 */
typedef struct xhttpcacheinvalidatecursor {
	size_t Field;
	bool Target;
} xhttpcacheinvalidatecursor;



/*
	失效候选借用目标 URI或响应字段值。
	Field 对目标 URI 为 XRT_NPOS，对字段候选为原字段下标。
*/
typedef struct xhttpcacheinvalidateitem {
	xstrview Reference;
	size_t Field;
	xhttpcacheinvalidatekind Kind;
} xhttpcacheinvalidateitem;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断缓存条目视图、字段数组和正文范围标志是否自洽。 */
XHTTP_POLICY_API bool xrtHttpCacheEntryValid(
	const xhttpcacheentry* pEntry
);



/*
	按 RFC 9111 为一组已经完成 URI、方法与 Vary 选择的条目生成验证计划。
	Range 为 true 时，部分条目只有声明 RANGE_COVERED 才能贡献 ETag，
	且不会生成 If-Modified-Since。
*/
XHTTP_POLICY_API xhttpcachevalidatedecision xrtHttpCacheValidatePlan(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
);



/*
	写出计划中的去重 If-None-Match 值。
	空输出可查询精确长度；短缓冲和重叠输出保持目标内存不变。
*/
XHTTP_POLICY_API bool xrtHttpCacheValidateETagsWrite(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	为单个缓存条目的 Range 请求选择 If-Range。
	弱 ETag 不可使用，并且存在任何 ETag 时不会退回 Last-Modified。
*/
XHTTP_POLICY_API xhttpcacheifrangekind xrtHttpCacheIfRangePlan(
	const xhttpcacheentry* pEntry,
	xhttpcacheifrange* pPlan
);



/*
	缓存按 RFC 9111 §4.3.2 评估收到的条件请求。
	只处理 GET/HEAD 的 If-None-Match 与 If-Modified-Since；
	源站专用的 If-Match 和 If-Unmodified-Since 保持转发。
*/
XHTTP_POLICY_API xhttpprecondition xrtHttpCachePreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestCount,
	const xhttpcacheentry* pEntry
);



/*
	分类条件请求的最终响应。
	Treat5xxAsFailure 为 true 时，5xx 交给重试或失联复用策略处理。
*/
XHTTP_POLICY_API xhttpcachevalidateresult xrtHttpCacheValidateResult(
	uint16 iStatus,
	bool Treat5xxAsFailure
);



/*
	按 304 中的验证器选择应更新的候选下标。
	强验证器选择全部匹配项；弱验证器只选择 Date 最新项；
	无验证器仅允许唯一且同样无验证器的候选。
	空下标输出可查询数量，短缓冲不修改下标数组。
*/
XHTTP_POLICY_API xhttpcacheupdatematch xrtHttpCache304Select(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iCapacity,
	size_t* pCount
);



/*
	判断 200 HEAD 是否能更新一个已保存 GET 响应。
	HEAD 中出现的 ETag、Last-Modified 和 Content-Length 必须逐项匹配。
*/
XHTTP_POLICY_API xhttpcacheheaddecision xrtHttpCacheHeadPlan(
	uint16 iStatus,
	const xhttpcacheentry* pEntry,
	const xhttpfield* pFields,
	size_t iFieldCount
);



/*
	按 RFC 9111 §3.2 判断新响应中的一个字段是否可替换已保存字段。
	函数自动过滤 Content-Length、Connection 及其提名字段、代理专用字段、
	以及限定 no-cache/private 排除的字段。
*/
XHTTP_POLICY_API xhttpcachefieldupdate xrtHttpCacheFieldUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iFlags
);



/*
	按 StorePlan 的 Actions 判断初次保存时是否保留一个响应字段。
	函数统一过滤固定逐跳字段、Connection 提名字段、代理专用字段，
	以及限定 no-cache/private 明确排除的字段。
*/
XHTTP_POLICY_API xhttpcachefieldstore xrtHttpCacheFieldStore(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iActions
);



/* 初始化可重复使用的缓存失效候选游标。 */
XHTTP_POLICY_API void xrtHttpCacheInvalidationCursorInit(
	xhttpcacheinvalidatecursor* pCursor
);



/*
	迭代 unsafe 方法在 2xx/3xx 后产生的失效候选。
	目标 URI 必定先返回；Location 与 Content-Location 只有语法有效且同源才返回。
*/
XHTTP_POLICY_API xhttpnext xrtHttpCacheInvalidationNext(
	xstrview Method,
	uint16 iStatus,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcacheinvalidatecursor* pCursor,
	xhttpcacheinvalidateitem* pItem
);



/*
	把失效候选解析为不含 fragment 的绝对 URI。
	空输出可查询精确长度，结果不附加零结尾。
	相对引用解析遵循 URL 模块契约，当前可能使用临时内存。
*/
XHTTP_POLICY_API bool xrtHttpCacheInvalidationWrite(
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_cache_range.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CACHE_RANGE_H
#define XRT_HTTP_CACHE_RANGE_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE))
	#error "XRT HTTP cache range support requires cache validation and HTTP ranges"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 片段输入标志描述传输完整性、multipart 层级和表示变换事实。 */
typedef enum xhttpcachefragmentinputflag {
	XHTTP_CACHE_FRAGMENT_INPUT_NONE = 0,
	XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_BODY_COMPLETE = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_MULTIPART_PART = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_TRANSFORMED = UINT32_C(0x00000008)
} xhttpcachefragmentinputflag;



/*
	Fields 是响应 Header，RangeFields 是 multipart part Header。
	普通 206 的 RangeFields 为空时直接从 Fields 读取 Content-Range。
*/
typedef struct xhttpcachefragmentinput {
	xstrview Method;
	const xhttpfield* Fields;
	size_t FieldCount;
	const xhttpfield* RangeFields;
	size_t RangeFieldCount;
	xtime ResponseTime;
	uint64 BodySize;
	uint16 Status;
	uint32 Flags;
} xhttpcachefragmentinput;



/* 片段跳过原因可以组合，便于缓存统计和诊断保守拒绝。 */
typedef enum xhttpcachefragmentreason {
	XHTTP_CACHE_FRAGMENT_REASON_NONE = 0,
	XHTTP_CACHE_FRAGMENT_REASON_METHOD = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_REASON_STATUS = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_REASON_HEADERS = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED = UINT32_C(0x00000008),
	XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE = UINT32_C(0x00000010),
	XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH = UINT32_C(0x00000020),
	XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH = UINT32_C(0x00000040),
	XHTTP_CACHE_FRAGMENT_REASON_EMPTY = UINT32_C(0x00000080)
} xhttpcachefragmentreason;



/* 片段计划区分参数错误、保守跳过和可以保存。 */
typedef enum xhttpcachefragmentdecision {
	XHTTP_CACHE_FRAGMENT_ERROR = -1,
	XHTTP_CACHE_FRAGMENT_SKIP = 0,
	XHTTP_CACHE_FRAGMENT_STORE = 1
} xhttpcachefragmentdecision;



/* 片段标志区分正文区间和已知完整表示长度。 */
typedef enum xhttpcachefragmentflag {
	XHTTP_CACHE_FRAGMENT_NONE = 0,
	XHTTP_CACHE_FRAGMENT_HAS_RANGE = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_HAS_LENGTH = UINT32_C(0x00000002)
} xhttpcachefragmentflag;



/*
	片段借用响应 Header；Range 是已经收到的实际字节闭区间。
	SourceStatus 保留 Header 来源是 200 还是 206，存储状态统一按 200 处理。
*/
typedef struct xhttpcachefragment {
	xhttpcacheentry Entry;
	xhttpbyterange Range;
	uint64 Length;
	uint16 SourceStatus;
	uint32 Flags;
} xhttpcachefragment;



/* 片段规范化动作由 Header 存储层执行，不绑定具体 Header 容器。 */
typedef enum xhttpcachefragmentaction {
	XHTTP_CACHE_FRAGMENT_ACTION_NONE = 0,
	XHTTP_CACHE_FRAGMENT_AS_200 = UINT32_C(0x00000001),
	XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE = UINT32_C(0x00000002),
	XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE = UINT32_C(0x00000004),
	XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH = UINT32_C(0x00000010)
} xhttpcachefragmentaction;



/* 片段计划同时返回协议决定、全部原因和 Header 规范化动作。 */
typedef struct xhttpcachefragmentplan {
	xhttpcachefragment Fragment;
	xhttpcachefragmentdecision Decision;
	uint32 Actions;
	uint32 Reasons;
} xhttpcachefragmentplan;



/*
	覆盖集借用按起点排序、互不重叠且不相邻的闭区间。
	HasLength 为 true 时所有区间都必须位于 Length 内。
*/
typedef struct xhttpcachecoverage {
	const xhttpbyterange* Ranges;
	size_t RangeCount;
	uint64 Length;
	bool HasLength;
} xhttpcachecoverage;



/* 覆盖判断区分参数错误、未覆盖和完整覆盖。 */
typedef enum xhttpcachecoverageresult {
	XHTTP_CACHE_COVERAGE_ERROR = -1,
	XHTTP_CACHE_COVERAGE_MISS = 0,
	XHTTP_CACHE_COVERAGE_HIT = 1
} xhttpcachecoverageresult;



/* 缺口游标允许在规范覆盖集上无分配前向迭代。 */
typedef struct xhttpcachemissingcursor {
	size_t Range;
	uint64 Next;
	bool Started;
	bool Finished;
} xhttpcachemissingcursor;



/* 组合决定区分独立表示、协议冲突、增量应用和完整替换。 */
typedef enum xhttpcachecombinedecision {
	XHTTP_CACHE_COMBINE_ERROR = -1,
	XHTTP_CACHE_COMBINE_SEPARATE = 0,
	XHTTP_CACHE_COMBINE_APPLY = 1,
	XHTTP_CACHE_COMBINE_REPLACE = 2,
	XHTTP_CACHE_COMBINE_CONFLICT = 3
} xhttpcachecombinedecision;



/* 组合动作描述 Header 来源和新响应字段更新要求。 */
typedef enum xhttpcachecombineaction {
	XHTTP_CACHE_COMBINE_ACTION_NONE = 0,
	XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS = UINT32_C(0x00000001),
	XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS = UINT32_C(0x00000002),
	XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE = UINT32_C(0x00000004),
	XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH = UINT32_C(0x00000008),
	XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH = UINT32_C(0x00000010),
	XHTTP_CACHE_COMBINE_MARK_INCOMPLETE = UINT32_C(0x00000020),
	XHTTP_CACHE_COMBINE_AS_200 = UINT32_C(0x00000040)
} xhttpcachecombineaction;



/*
	Index 和 RemoveCount 描述规范区间数组中的替换窗口。
	HeaderIndex 为 XRT_NPOS 时使用 Incoming Header，否则借用对应 Stored Header。
*/
typedef struct xhttpcachecombineplan {
	xhttpbyterange Range;
	uint64 Length;
	size_t Index;
	size_t RemoveCount;
	size_t ResultCount;
	size_t HeaderIndex;
	xhttpcachecombinedecision Decision;
	uint32 Actions;
	bool HasRange;
	bool HasLength;
	bool Complete;
} xhttpcachecombineplan;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 初始化尚未收到 Header 或正文的片段输入。 */
XHTTP_POLICY_API void xrtHttpCacheFragmentInputInit(
	xhttpcachefragmentinput* pInput
);



/*
	把完整或截断 200、单段 206、multipart 中的一个 part 规范化为缓存片段。
	调用方必须先完成缓存键、Vary 和可存储策略判断；STORE 只表示片段结构可保存。
	协议性跳过返回 SKIP 并填写 Reasons；参数错误返回 ERROR 且不修改计划。
*/
XHTTP_POLICY_API xhttpcachefragmentdecision xrtHttpCacheFragmentPlan(
	const xhttpcachefragmentinput* pInput,
	xhttpcachefragmentplan* pPlan
);



/* 判断公开片段的 Header、区间、完整长度和 partial 标志是否自洽。 */
XHTTP_POLICY_API bool xrtHttpCacheFragmentValid(
	const xhttpcachefragment* pFragment
);



/* 判断片段是否已经覆盖完整表示，包括长度为零的空表示。 */
XHTTP_POLICY_API bool xrtHttpCacheFragmentComplete(
	const xhttpcachefragment* pFragment
);



/* 判断规范覆盖集的排序、相邻合并和完整长度约束是否成立。 */
XHTTP_POLICY_API bool xrtHttpCacheCoverageValid(
	const xhttpcachecoverage* pCoverage
);



/* 判断一个闭区间是否完全位于覆盖集中。 */
XHTTP_POLICY_API xhttpcachecoverageresult xrtHttpCacheCoverageCovers(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pRange
);



/* 初始化可以重复使用的缺口游标。 */
XHTTP_POLICY_API void xrtHttpCacheMissingCursorInit(
	xhttpcachemissingcursor* pCursor
);



/*
	按起点顺序返回目标闭区间内尚未保存的连续缺口。
	覆盖集和目标必须保持不变，直到游标返回 END 或重新初始化。
*/
XHTTP_POLICY_API xhttpnext xrtHttpCacheMissingNext(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	xhttpcachemissingcursor* pCursor,
	xhttpbyterange* pMissing
);



/*
	按 RFC 9111 与 RFC 9110 组合一个新片段。
	Stored 和 Incoming 必须属于同一缓存键及 Vary 变体，Stored 按区间规范排序。
	现有片段只有整组共享同一个强验证器才能增量组合。
*/
XHTTP_POLICY_API xhttpcachecombinedecision xrtHttpCacheCombinePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_cache_store.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CACHE_STORE_H
#define XRT_HTTP_CACHE_STORE_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	 !defined(XHTTP_FEATURE_HTTP_VARY) || \
	 !defined(XRT_FEATURE_MAP) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP cache store requires cache validation, Vary, Map and Mutex"
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

#define XHTTP_CACHE_ENTRIES_DEFAULT		1024u
#define XHTTP_CACHE_BYTES_DEFAULT		(UINT64_C(64) * 1024u * 1024u)
#define XHTTP_CACHE_ENTRY_BYTES_DEFAULT	(UINT64_C(8) * 1024u * 1024u)



/* Cache 是引用计数、线程安全且按容量淘汰的内存响应存储。 */
typedef struct xhttpcache xhttpcache;



/* CacheRecord 是可跨线程持有的不可变响应快照。 */
typedef struct xhttpcacherecord xhttpcacherecord;



/*
	主键至少包含方法和目标 URI；Partition 用于站点、用户或租户隔离。
	Fields 是产生响应时的有效请求字段，Record 只保存 Vary 实际引用的字段。
*/
typedef struct xhttpcachekey {
	xstrview Method;
	xstrview URI;
	xstrview Partition;
	const xhttpfield* Fields;
	size_t FieldCount;
} xhttpcachekey;



/* Part 保存表示正文中的一段连续字节，Offset 使用完整表示的零基偏移。 */
typedef struct xhttpcachepart {
	uint64 Offset;
	xbytesview Data;
} xhttpcachepart;



typedef enum xhttpcacherecordflag {
	XHTTP_CACHE_RECORD_NONE = 0,

	/* Length 是已知的完整表示长度。 */
	XHTTP_CACHE_RECORD_HAS_LENGTH = UINT32_C(0x00000001),

	/* Parts 已经无空洞覆盖 [0, Length)，空表示允许 Length 为零。 */
	XHTTP_CACHE_RECORD_COMPLETE = UINT32_C(0x00000002)
} xhttpcacherecordflag;



/*
	RecordInput 全部借用调用方数据，创建成功后得到完全独立的紧凑副本。
	RequestClock 和 ResponseClock 来自同一单调时钟，ResponseTime 是墙钟时间。
*/
typedef struct xhttpcacherecordinput {
	xhttpcachekey Key;
	xhttpversion Version;
	uint16 Status;
	uint32 Flags;
	xstrview Reason;
	const xhttpfield* Fields;
	size_t FieldCount;
	const xhttpfield* Trailers;
	size_t TrailerCount;
	const xhttpcachepart* Parts;
	size_t PartCount;
	uint64 Length;
	xtime ResponseTime;
	double RequestClock;
	double ResponseClock;
} xhttpcacherecordinput;



/*
	所有限额都是硬逻辑限额；MaxBytes 计算不可变 Record 的实际分配大小。
	MaxEntries 独立约束索引和 LRU 元数据，零值配置无效，应先调用 ConfigInit。
*/
typedef struct xhttpcacheconfig {
	size_t InitialEntries;
	size_t MaxEntries;
	size_t MaxBytes;
	size_t MaxEntryBytes;
} xhttpcacheconfig;



/* Lookup 把正常未命中与执行错误分开。 */
typedef enum xhttpcachelookup {
	XHTTP_CACHE_LOOKUP_ERROR = -1,
	XHTTP_CACHE_LOOKUP_MISS = 0,
	XHTTP_CACHE_LOOKUP_HIT = 1
} xhttpcachelookup;



/*
	写入结果把并发冲突、容量拒绝、首次保存和变体替换分开。
	普通 Put 不返回 CONFLICT；条件 Insert 和 Replace 可以返回 CONFLICT。
*/
typedef enum xhttpcacheput {
	XHTTP_CACHE_PUT_ERROR = -1,
	XHTTP_CACHE_PUT_CONFLICT = 0,
	XHTTP_CACHE_PUT_REJECTED = 1,
	XHTTP_CACHE_PUT_STORED = 2,
	XHTTP_CACHE_PUT_REPLACED = 3
} xhttpcacheput;



/* 按不可变 Record 身份删除时区分错误、并发冲突和成功删除。 */
typedef enum xhttpcachechange {
	XHTTP_CACHE_CHANGE_ERROR = -1,
	XHTTP_CACHE_CHANGE_CONFLICT = 0,
	XHTTP_CACHE_CHANGE_APPLIED = 1
} xhttpcachechange;



/* Stats 是锁内取得的一次一致快照，累计计数从 Cache 创建起单调递增。 */
typedef struct xhttpcachestats {
	size_t Entries;
	size_t Bytes;
	uint64 Lookups;
	uint64 Hits;
	uint64 Misses;
	uint64 Stores;
	uint64 Replacements;
	uint64 Rejected;
	uint64 Conflicts;
	uint64 Evictions;
	uint64 Removals;
} xhttpcachestats;



/*
	Ops 为持久化、跨进程或应用自定义存储提供统一后端契约。
	除 Close 外的回调均为必需项，并且必须自行保证并发调用安全。
	Get 命中时转移一个独立 Record 引用；写入回调均不接管调用方引用。
	Insert 必须把“不存在”判断和插入作为一个不可分割的操作。
	Replace 的 Expected 是先前 Get 返回的不可变版本；后端只有确认该版本仍是
	对应变体的当前值时，才能原子提交 Replacement。持久化后端可以把 Expected
	映射为自己的修订号，但不能把比较和写入拆开。
	RemoveRecord 使用相同的当前版本条件并把比较和删除作为一个原子操作。
	条件不成立返回 CONFLICT，不得覆盖更新者，也不应伪装成执行错误。
	Remove、RemoveURI 和 Clear 用 false 报告失败，删除数量输出不能代替状态。
	统一句柄会验证 Get 命中的 Record 与查询 Key 匹配；Stats 失败时输出保持全零。
*/
typedef struct xhttpcacheops {
	xhttpcachelookup (*Get)(
		ptr pContext,
		const xhttpcachekey* pKey,
		xhttpcacherecord** ppRecord
	);
	xhttpcacheput (*Put)(
		ptr pContext,
		xhttpcacherecord* pRecord
	);
	xhttpcacheput (*Insert)(
		ptr pContext,
		xhttpcacherecord* pRecord
	);
	xhttpcacheput (*Replace)(
		ptr pContext,
		const xhttpcacherecord* pExpected,
		xhttpcacherecord* pReplacement
	);
	xhttpcachechange (*RemoveRecord)(
		ptr pContext,
		const xhttpcacherecord* pExpected
	);
	bool (*Remove)(
		ptr pContext,
		const xhttpcachekey* pKey,
		size_t* pRemoved
	);
	bool (*RemoveURI)(
		ptr pContext,
		xstrview URI,
		xstrview Partition,
		size_t* pRemoved
	);
	bool (*Clear)(ptr pContext);
	bool (*Stats)(
		ptr pContext,
		xhttpcachestats* pStats
	);
	void (*Close)(ptr pContext);
} xhttpcacheops;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 初始化默认缓存限额；固定配置描述符支持未对齐存储。 */
XHTTP_POLICY_API void xrtHttpCacheConfigInit(xhttpcacheconfig* pConfig);



/* 初始化没有分区和请求字段的主键；未对齐输出在返回前一次性发布。 */
XHTTP_POLICY_API bool xrtHttpCacheKeyInit(
	xhttpcachekey* pKey,
	xstrview Method,
	xstrview URI
);



/* 初始化 HTTP/1.1 响应输入；Key 与输出描述符都支持未对齐存储。 */
XHTTP_POLICY_API bool xrtHttpCacheRecordInputInit(
	xhttpcacherecordinput* pInput,
	const xhttpcachekey* pKey,
	uint16 iStatus
);



/*
	创建完全拥有输入内容的不可变 Record。
	Vary 星号被正常响应语义允许，但不能形成可复用记录，因此返回不支持错误。
	RecordInput 与正文片段数组可以未对齐，返回前已复制全部借用内容。
*/
XHTTP_POLICY_API xhttpcacherecord* xrtHttpCacheRecordCreate(
	const xhttpcacherecordinput* pInput
);



/* 增加 Record 引用并返回原指针；引用溢出时返回空指针。 */
XHTTP_POLICY_API xhttpcacherecord* xrtHttpCacheRecordRetain(
	const xhttpcacherecord* pRecord
);



/* 释放 Record 引用；空指针是安全的空操作。 */
XHTTP_POLICY_API void xrtHttpCacheRecordRelease(xhttpcacherecord* pRecord);



/* 返回 Record 拥有的主键；Fields 只包含 Vary 引用的原请求字段。 */
XHTTP_POLICY_API const xhttpcachekey* xrtHttpCacheRecordKey(
	const xhttpcacherecord* pRecord
);



/* 返回响应协议版本。 */
XHTTP_POLICY_API xhttpversion xrtHttpCacheRecordVersion(
	const xhttpcacherecord* pRecord
);



/* 返回最终响应状态码。 */
XHTTP_POLICY_API uint16 xrtHttpCacheRecordStatus(
	const xhttpcacherecord* pRecord
);



/* 返回 Record 标志。 */
XHTTP_POLICY_API uint32 xrtHttpCacheRecordFlags(
	const xhttpcacherecord* pRecord
);



/* 返回借用的 reason phrase。 */
XHTTP_POLICY_API xstrview xrtHttpCacheRecordReason(
	const xhttpcacherecord* pRecord
);



/* 返回响应 Header 数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordFieldCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定响应 Header，越界返回空指针且不设置错误。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordFieldAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回首个同名响应 Header，未找到返回空指针。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordField(
	const xhttpcacherecord* pRecord,
	xstrview Name
);



/* 返回响应 Trailer 数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordTrailerCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定响应 Trailer，越界返回空指针且不设置错误。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordTrailerAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回正文片段数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordPartCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定不可变正文片段，越界返回空指针且不设置错误。 */
XHTTP_POLICY_API const xhttpcachepart* xrtHttpCacheRecordPartAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/* 返回全部片段有效载荷字节数。 */
XHTTP_POLICY_API uint64 xrtHttpCacheRecordBodyBytes(
	const xhttpcacherecord* pRecord
);



/* 返回完整表示长度；未知时返回零并由 HAS_LENGTH 标志区分。 */
XHTTP_POLICY_API uint64 xrtHttpCacheRecordLength(
	const xhttpcacherecord* pRecord
);



/* 返回响应接收墙钟时间。 */
XHTTP_POLICY_API xtime xrtHttpCacheRecordResponseTime(
	const xhttpcacherecord* pRecord
);



/* 返回发出请求时的单调时钟。 */
XHTTP_POLICY_API double xrtHttpCacheRecordRequestClock(
	const xhttpcacherecord* pRecord
);



/* 返回收到响应时的单调时钟。 */
XHTTP_POLICY_API double xrtHttpCacheRecordResponseClock(
	const xhttpcacherecord* pRecord
);



/* 返回 Record 单次紧凑分配的实际字节数。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordCharge(
	const xhttpcacherecord* pRecord
);



/* 返回去重后的 Vary 选择字段数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordVaryCount(
	const xhttpcacherecord* pRecord
);



/* 返回指定 Vary 字段名，越界返回空视图且不设置错误。 */
XHTTP_POLICY_API xstrview xrtHttpCacheRecordVaryAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
);



/*
	判断主键和 Vary 原请求字段是否匹配。
	字段值按顺序比较并忽略两端 OWS；缺失字段只匹配缺失字段。
*/
XHTTP_POLICY_API bool xrtHttpCacheRecordMatches(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
);



/* 构造验证协议层可直接使用的借用 Entry 视图；输出支持未对齐存储。 */
XHTTP_POLICY_API bool xrtHttpCacheRecordEntry(
	const xhttpcacherecord* pRecord,
	xhttpcacheentry* pEntry
);



/* 创建线程安全的有界内存缓存；可选配置在返回前从未对齐存储快照。 */
XHTTP_POLICY_API xhttpcache* xrtHttpCacheCreate(
	const xhttpcacheconfig* pConfig
);



/*
	用自定义线程安全后端创建统一 Cache 句柄。
	句柄从可未对齐存储复制 Ops；除 Close 外的全部回调都必须存在。
	最后一个引用释放时调用可选 Close，Context 由后端定义。
*/
XHTTP_POLICY_API xhttpcache* xrtHttpCacheOpen(
	const xhttpcacheops* pOps,
	ptr pContext
);



/* 增加 Cache 引用并返回原指针；引用溢出时返回空指针。 */
XHTTP_POLICY_API xhttpcache* xrtHttpCacheRetain(const xhttpcache* pCache);



/* 释放 Cache 引用；空指针是安全的空操作。 */
XHTTP_POLICY_API void xrtHttpCacheRelease(xhttpcache* pCache);



/*
	查找最新匹配记录并返回独立引用。
	未命中和错误都会把输出清空，命中记录由调用方 Release。
	Key 与记录指针输出支持未对齐存储，但彼此及借用内容不得重叠。
*/
XHTTP_POLICY_API xhttpcachelookup xrtHttpCacheGet(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
);



/*
	保存 Record 的独立引用，函数不接管调用方引用。
	相同主键、Vary 维度和原请求值替换旧记录；容量不足是正常拒绝。
	该无条件接口使用最后写入者获胜，不返回 CONFLICT。
*/
XHTTP_POLICY_API xhttpcacheput xrtHttpCachePut(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
);



/*
	仅在相同主键和 Vary 变体尚不存在时原子插入 Record。
	已有当前记录时返回 CONFLICT，不以旧快照覆盖并发写入。
	函数不接管调用方引用；成功后 Cache 独立持有 Record。
*/
XHTTP_POLICY_API xhttpcacheput xrtHttpCacheInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
);



/*
	仅在 Expected 仍是对应 Vary 变体的当前 Record 时原子提交 Replacement。
	两个 Record 的方法、URI 和 Partition 必须相同；Vary 可以按新元数据变化。
	Expected 应来自先前 Get；函数不接管任何引用，成功后 Cache 独立持有
	Replacement。并发更新使条件失效时返回 CONFLICT。
*/
XHTTP_POLICY_API xhttpcacheput xrtHttpCacheReplace(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
);



/*
	仅在 Expected 仍是当前 Record 时原子删除。
	记录已经被替换、删除或淘汰时返回 CONFLICT，不把正常竞态报告为执行错误。
	函数不接管 Expected 引用。
*/
XHTTP_POLICY_API xhttpcachechange xrtHttpCacheRemoveRecord(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected
);



/*
	删除与完整 Key 和 Vary 选择匹配的全部记录。
	pRemoved 可以为空或未对齐；成功且没有匹配记录与后端失败保持可区分。
*/
XHTTP_POLICY_API bool xrtHttpCacheRemove(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	size_t* pRemoved
);



/*
	删除 URI 和 Partition 下的全部方法与 Vary 变体。
	pRemoved 可以为空或未对齐；失败时输出保持为零。
*/
XHTTP_POLICY_API bool xrtHttpCacheRemoveURI(
	xhttpcache* pCache,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
);



/* 删除全部记录并保留 Map 桶容量供后续复用。 */
XHTTP_POLICY_API bool xrtHttpCacheClear(xhttpcache* pCache);



/* 取得一致统计快照；未对齐输出受支持，失败时输出全零。 */
XHTTP_POLICY_API bool xrtHttpCacheStats(
	xhttpcache* pCache,
	xhttpcachestats* pStats
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_auth_policy.h */
/* ========================================================================== */

#ifndef XHTTP_POLICY_HTTP_AUTH_POLICY_H
#define XHTTP_POLICY_HTTP_AUTH_POLICY_H



XRT_EXTERN_C_BEGIN

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XRT_FEATURE_CRYPTO_MD5))
	#error "XRT HTTP Digest MD5 requires Digest SHA-2 and MD5 support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP Digest random nonce requires nonce and secure random support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest session support requires Digest client and mutex support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)

/*
	Secret 是规范十六进制 H(username:realm:password)，可直接持久化。
	EntityHash 仅供 auth-int 使用，并由流式或一次性摘要入口预先计算。
*/
typedef struct xhttpdigestproof {
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	uint32 NonceCount;
	xstrview Secret;
	xstrview Nonce;
	xstrview Cnonce;
	xstrview Uri;
	xstrview EntityHash;
} xhttpdigestproof;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)

#define XHTTP_DIGEST_NONCE_KEY_MIN 32u
#define XHTTP_DIGEST_NONCE_SALT_SIZE 16u
#define XHTTP_DIGEST_NONCE_TEXT_SIZE 76u



/* 无状态 nonce 验证结果；INVALID 不表示库调用失败。 */
typedef enum xhttpdigestnoncecheck {
	XHTTP_DIGEST_NONCE_ERROR = -1,
	XHTTP_DIGEST_NONCE_INVALID = 0,
	XHTTP_DIGEST_NONCE_VALID,
	XHTTP_DIGEST_NONCE_STALE
} xhttpdigestnoncecheck;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

#define XHTTP_DIGEST_REPLAY_KEY_SIZE XRT_SHA256_SIZE



/* 重放键是带域分隔和长度边界的 SHA-256 摘要，可直接交给外部原子存储。 */
typedef struct xhttpdigestreplaykey {
	uint8 Bytes[XHTTP_DIGEST_REPLAY_KEY_SIZE];
} xhttpdigestreplaykey;



/* 内置重放表按分片设置硬容量；LifetimeSeconds 必须与 nonce 策略一致。 */
typedef struct xhttpdigestreplayconfig {
	size_t Shards;
	size_t EntriesPerShard;
	int64 LifetimeSeconds;
} xhttpdigestreplayconfig;



/* 重放检查的策略结果；只有 ERROR 表示库调用失败。 */
typedef enum xhttpdigestreplaycheck {
	XHTTP_DIGEST_REPLAY_ERROR = -1,
	XHTTP_DIGEST_REPLAY_REPLAY = 0,
	XHTTP_DIGEST_REPLAY_ACCEPTED,
	XHTTP_DIGEST_REPLAY_EXPIRED,
	XHTTP_DIGEST_REPLAY_FULL
} xhttpdigestreplaycheck;



/* 统计按分片锁取得一致快照，计数达到 uint64 上限后保持饱和。 */
typedef struct xhttpdigestreplaystats {
	size_t Entries;
	size_t Capacity;
	uint64 Accepted;
	uint64 Replayed;
	uint64 Expired;
	uint64 Full;
	uint64 Purged;
} xhttpdigestreplaystats;



/* DigestReplay 是线程安全、有硬容量边界的进程内 nonce-count 存储。 */
typedef struct xhttpdigestreplay xhttpdigestreplay;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

#define XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH UINT32_C(0x00000001)



/* 证明验证结果是普通协议决策；只有 ERROR 表示库调用失败。 */
typedef enum xhttpdigestverifycheck {
	XHTTP_DIGEST_VERIFY_ERROR = -1,
	XHTTP_DIGEST_VERIFY_INVALID = 0,
	XHTTP_DIGEST_VERIFY_VALID,
	XHTTP_DIGEST_VERIFY_STALE
} xhttpdigestverifycheck;



/*
	验证描述符把客户端凭据绑定到实际 challenge、请求行和已查得的 Secret。
	EntityHash 只在 qop=auth-int 时提供，所有文本和对象均由调用方借用。
*/
typedef struct xhttpdigestverification {
	uint32 Flags;
	const xhttpdigestauth* Auth;
	const xhttpdigestchallenge* Challenge;
	xstrview Secret;
	xstrview Method;
	xstrview RequestTarget;
	xstrview EntityHash;
} xhttpdigestverification;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

#define XHTTP_DIGEST_ALGORITHMS_MD5 \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_ALGORITHMS_MD5_SESSION \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_ALGORITHMS_SHA256 \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION \
	UINT32_C(0x00000008)
#define XHTTP_DIGEST_ALGORITHMS_SHA512_256 \
	UINT32_C(0x00000010)
#define XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION \
	UINT32_C(0x00000020)
#define XHTTP_DIGEST_ALGORITHMS_SHA2 \
	((uint32)XHTTP_DIGEST_ALGORITHMS_SHA256 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA512_256 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION)
#define XHTTP_DIGEST_ALGORITHMS_ALL \
	((uint32)XHTTP_DIGEST_ALGORITHMS_MD5 | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_MD5_SESSION | \
	 (uint32)XHTTP_DIGEST_ALGORITHMS_SHA2)

#define XHTTP_DIGEST_QOPS_AUTH \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_QOPS_AUTH_INT \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_QOPS_ALL \
	((uint32)XHTTP_DIGEST_QOPS_AUTH | \
	 (uint32)XHTTP_DIGEST_QOPS_AUTH_INT)

#define XHTTP_DIGEST_POLICY_PREFER_AUTH_INT \
	UINT32_C(0x00000001)
#define XHTTP_DIGEST_POLICY_PLAIN_USERNAME \
	UINT32_C(0x00000002)
#define XHTTP_DIGEST_POLICY_REQUIRE_USERHASH \
	UINT32_C(0x00000004)
#define XHTTP_DIGEST_POLICY_REQUIRE_UTF8 \
	UINT32_C(0x00000008)

#define XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED \
	UINT32_C(0x00000001)



/* 客户端协商结果区分普通策略拒绝与库调用错误。 */
typedef enum xhttpdigestchoosecheck {
	XHTTP_DIGEST_CHOOSE_ERROR = -1,
	XHTTP_DIGEST_CHOOSE_REJECTED = 0,
	XHTTP_DIGEST_CHOOSE_ACCEPTED
} xhttpdigestchoosecheck;



/* 响应证明结果区分认证不匹配与库调用错误。 */
typedef enum xhttpdigestinfocheck {
	XHTTP_DIGEST_INFO_ERROR = -1,
	XHTTP_DIGEST_INFO_INVALID = 0,
	XHTTP_DIGEST_INFO_VALID
} xhttpdigestinfocheck;



/* 算法和 qop 是允许集合；Flags 只控制客户端本地偏好。 */
typedef struct xhttpdigestpolicy {
	uint32 Flags;
	uint32 Algorithms;
	uint32 Qops;
} xhttpdigestpolicy;



/* 选择结果固定 challenge 算法、单个 qop 和用户名传输方式。 */
typedef struct xhttpdigestchoice {
	xhttpdigestalgorithm Algorithm;
	xhttpdigestqop Qop;
	bool UserHash;
} xhttpdigestchoice;



/*
	客户端凭据输入只借用调用方数据；Secret 是规范十六进制 H(A1)。
	EntityHash 只在 qop=auth-int 时提供，UsernameLanguage 只供 username* 使用。
*/
typedef struct xhttpdigestclientauth {
	uint32 Flags;
	const xhttpdigestchallenge* Challenge;
	const xhttpdigestchoice* Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Method;
	xstrview RequestTarget;
	xstrview Cnonce;
	xstrview EntityHash;
	uint32 NonceCount;
} xhttpdigestclientauth;



/*
	响应证明输入必须保留实际请求使用的 Proof；ResponseEntityHash 只供 auth-int。
	Info 和 Proof 由调用方借用，允许验证乱序完成的并发请求。
*/
typedef struct xhttpdigestinfoverification {
	const xhttpdigestinfo* Info;
	const xhttpdigestproof* Proof;
	xstrview ResponseEntityHash;
} xhttpdigestinfoverification;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 会话拥有认证状态；Exchange 固定一次请求使用的 nonce-count 与响应证明。 */
typedef struct xhttpdigestsession xhttpdigestsession;
typedef struct xhttpdigestexchange xhttpdigestexchange;



/* VALID 只表示响应证明有效；UPDATED 表示 nextnonce 已原子采用。 */
typedef enum xhttpdigestsessioncheck {
	XHTTP_DIGEST_SESSION_ERROR = -1,
	XHTTP_DIGEST_SESSION_INVALID = 0,
	XHTTP_DIGEST_SESSION_VALID,
	XHTTP_DIGEST_SESSION_UPDATED,
	XHTTP_DIGEST_SESSION_SUPERSEDED
} xhttpdigestsessioncheck;



/* 创建和更新时复制全部字段；Flags 使用 XHTTP_DIGEST_CLIENT_*。 */
typedef struct xhttpdigestsessionconfig {
	uint32 Flags;
	const xhttpdigestchallenge* Challenge;
	const xhttpdigestchoice* Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Cnonce;
} xhttpdigestsessionconfig;

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)

/* 使用所选 Digest 算法散列任意连续数据，并写出规范小写十六进制。 */
XHTTP_POLICY_API bool xrtHttpDigestHash(
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iDataSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算可持久化的 H(username:realm:password)；session 算法仍返回基础值。 */
XHTTP_POLICY_API bool xrtHttpDigestSecret(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 RFC 7616 userhash，即 H(username:realm)。 */
XHTTP_POLICY_API bool xrtHttpDigestUserHash(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authorization 中的 request-digest；Method 使用请求线路中的原始方法。 */
XHTTP_POLICY_API bool xrtHttpDigestRequest(
	const xhttpdigestproof* pProof,
	xstrview Method,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 计算 Authentication-Info 中的 rspauth。 */
XHTTP_POLICY_API bool xrtHttpDigestRspAuth(
	const xhttpdigestproof* pProof,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 不修改错误槽地常量时间比较两个大小写不敏感的十六进制摘要。 */
XHTTP_POLICY_API bool xrtHttpDigestEqual(xstrview Left, xstrview Right);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)

/*
	从 Unix 秒、调用方 salt 和上下文构建 HMAC-SHA256 无状态 nonce。
	Key 至少 32 字节；输出是不补等号且不补零的固定 76 字节 Base64URL。
*/
XHTTP_POLICY_API bool xrtHttpDigestNonceWrite(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	验证无状态 nonce 的签名、上下文和时间窗口。
	无效线路值、签名或超出未来容差返回 INVALID；合法过期值返回 STALE。
*/
XHTTP_POLICY_API xhttpdigestnoncecheck xrtHttpDigestNonceVerify(
	xstrview Nonce,
	xbytesview Key,
	xbytesview Context,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)

/* 使用操作系统安全随机源生成 salt，再构建无状态 nonce。 */
XHTTP_POLICY_API bool xrtHttpDigestNonceCreate(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

/*
	验证 challenge、请求目标和 request-digest，不检查 nonce 签名或时效。
	该入口适合自定义 nonce、轮换 key-ring 或外部认证策略。
*/
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestProofVerify(
	const xhttpdigestverification* pVerification
);



/*
	验证内置无状态 nonce 与 request-digest；过期 nonce 仅在证明有效时返回 STALE。
	IssuedSeconds 只在 VALID 或 STALE 时写入，可以为空。
*/
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestVerify(
	const xhttpdigestverification* pVerification,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

/* 初始化现代默认策略：SHA-2、auth/auth-int、优先 auth、遵循 userhash。 */
XHTTP_POLICY_API void xrtHttpDigestPolicyInit(
	xhttpdigestpolicy* pPolicy
);



/*
	按本地策略判断一份已解析 challenge；空 Policy 使用现代默认策略。
	普通不支持或策略拒绝返回 REJECTED，不修改线程错误槽。
*/
XHTTP_POLICY_API xhttpdigestchoosecheck xrtHttpDigestChallengeChoose(
	const xhttpdigestchallenge* pChallenge,
	const xhttpdigestpolicy* pPolicy,
	xhttpdigestchoice* pChoice
);



/*
	构造完整 Digest 凭据；查询模式返回用户名摘要和 response 的合计字节数。
	实际输出中的所有新视图借用 Output，其余视图借用输入和 challenge。
*/
XHTTP_POLICY_API bool xrtHttpDigestClientAuth(
	const xhttpdigestclientauth* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
);



/*
	校验 Authentication-Info 的 rspauth、qop、cnonce 和 nc。
	缺少 rspauth 或任何上下文不匹配均返回 INVALID，不发布 nextnonce 决策。
*/
XHTTP_POLICY_API xhttpdigestinfocheck xrtHttpDigestInfoVerify(
	const xhttpdigestinfoverification* pVerification
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 创建并复制 Digest 客户端状态；首个请求的 nonce-count 为 1。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionCreate(
	const xhttpdigestsessionconfig* pConfig
);



/* 增加会话引用；Exchange 会自动持有创建它的会话。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionRetain(
	xhttpdigestsession* pSession
);



/* 释放会话引用；空指针是安全空操作。 */
XHTTP_POLICY_API void xrtHttpDigestSessionRelease(
	xhttpdigestsession* pSession
);



/* 原子替换 challenge、选择结果与凭据，并把 nonce-count 重置为 1。 */
XHTTP_POLICY_API bool xrtHttpDigestSessionUpdate(
	xhttpdigestsession* pSession,
	const xhttpdigestsessionconfig* pConfig
);



/* 为一次请求保留唯一 nonce-count；返回对象可跨线程等待乱序响应。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestSessionAuthorize(
	xhttpdigestsession* pSession,
	xstrview Method,
	xstrview RequestTarget,
	xstrview EntityHash
);



/* 增加请求 Exchange 引用。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestExchangeRetain(
	xhttpdigestexchange* pExchange
);



/* 释放 Exchange 及其持有的会话、状态和敏感数据。 */
XHTTP_POLICY_API void xrtHttpDigestExchangeRelease(
	xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的 Authorization 协议对象。 */
XHTTP_POLICY_API const xhttpdigestauth* xrtHttpDigestExchangeAuth(
	const xhttpdigestexchange* pExchange
);



/* 返回 Exchange 生命周期内稳定的响应验证 proof。 */
XHTTP_POLICY_API const xhttpdigestproof* xrtHttpDigestExchangeProof(
	const xhttpdigestexchange* pExchange
);



/*
	验证 Authentication-Info，并在证明有效时原子采用 nextnonce。
	NextCnonce 为空时沿用当前 cnonce；旧 Exchange 的 nextnonce 返回 SUPERSEDED。
*/
XHTTP_POLICY_API xhttpdigestsessioncheck xrtHttpDigestSessionAccept(
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const xhttpdigestinfo* pInfo,
	xstrview ResponseEntityHash,
	xstrview NextCnonce
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

/* 初始化适合普通多线程 HTTP 服务的有界默认配置。 */
XHTTP_POLICY_API void xrtHttpDigestReplayConfigInit(
	xhttpdigestreplayconfig* pConfig
);



/* 从用户名、nonce 和 cnonce 构建无歧义的固定重放键。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayKey(
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	xhttpdigestreplaykey* pKey
);



/* 创建并发安全的内置重放表；配置为空时使用默认值。 */
XHTTP_POLICY_API xhttpdigestreplay* xrtHttpDigestReplayCreate(
	const xhttpdigestreplayconfig* pConfig
);



/* 销毁重放表；空指针是安全空操作，不能与其他操作并发。 */
XHTTP_POLICY_API void xrtHttpDigestReplayDestroy(
	xhttpdigestreplay* pReplay
);



/* 按预先派生的键原子检查并推进最大 nonce-count。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheckKey(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 派生键并原子检查，适合已经验证 Digest 证明的普通服务端路径。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheck(
	xhttpdigestreplay* pReplay,
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
);



/* 删除指定时间已经超出 nonce 有效期的记录并返回删除数量。 */
XHTTP_POLICY_API size_t xrtHttpDigestReplayPurge(
	xhttpdigestreplay* pReplay,
	int64 iNowSeconds
);



/* 清空全部重放记录并保留映射、堆和统计计数。 */
XHTTP_POLICY_API void xrtHttpDigestReplayClear(
	xhttpdigestreplay* pReplay
);



/* 取得当前容量、条目和累计策略结果；输出可位于合法未对齐存储。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayStats(
	xhttpdigestreplay* pReplay,
	xhttpdigestreplaystats* pStats
);

#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA512_256))
	#error "XRT HTTP Digest SHA-2 requires Digest, SHA-256 and SHA-512/256"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_CRYPTO_HMAC_SHA256))
	#error "XRT HTTP Digest nonce requires Digest, Base64 and HMAC-SHA256"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST) || \
	 !defined(XRT_FEATURE_CRYPTO_SHA256) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP Digest replay protection requires Digest, SHA-256 and mutex support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE))
	#error "XRT HTTP Digest verification requires challenge, credentials, SHA-2 and nonce support"
#endif

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) && \
	(!defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CHALLENGE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CREDENTIALS) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_INFO) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2))
	#error "XRT HTTP Digest client support requires challenge, credentials, info and SHA-2 support"
#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_client_policy.h */
/* ========================================================================== */

#ifndef XHTTP_POLICY_HTTP_CLIENT_POLICY_H
#define XHTTP_POLICY_HTTP_CLIENT_POLICY_H



XRT_EXTERN_C_BEGIN


#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP/1 Digest prepare support requires request prepare and Digest session support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT))
	#error "XRT HTTP client Digest selection requires response Digest and client protocol support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO) || \
	 !defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION))
	#error "XRT HTTP client Digest session response support requires Digest info and session support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)

/*
	使用最终 request-target 生成源站 Digest 凭据并冻结请求计划。
	成功时发布由调用方释放的 Exchange；失败时请求对象不变且输出为空。
	ppExchange 允许未对齐，但不得覆盖请求或 Digest session 拥有的存储。
*/
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);



/* 使用同一准备契约生成 Proxy-Authorization Digest 字段。 */
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareProxyDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
);

#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)

/*
	按线路顺序选择首个满足本地策略的源站 Digest challenge。
	查询模式完成全部候选验证并返回所选项的精确解码长度。
	全部写出区允许未对齐，但必须互不覆盖且不得覆盖响应拥有的存储。
*/
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);



/* 选择首个满足本地策略的代理 Digest challenge；契约与源站入口一致。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
);

#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

/*
	解析并验证源站 Digest 回执。
	END 表示字段缺失；ITEM 时 pCheck 区分无效、有效、更新与已取代状态。
	pCheck 允许未对齐，但不得覆盖响应、会话或 Exchange 拥有的存储。
*/
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);



/* 解析并验证代理 Digest 回执；返回与状态契约和源站入口一致。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
);

#endif

XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/cookie_jar.h */
/* ========================================================================== */

#ifndef XRT_COOKIE_JAR_H
#define XRT_COOKIE_JAR_H



#if defined(XHTTP_FEATURE_COOKIE_JAR)
#endif

#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)
#endif



#if defined(XHTTP_FEATURE_COOKIE_JAR) && \
	(!defined(XHTTP_FEATURE_SET_COOKIE) || !defined(XHTTP_FEATURE_URL) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT cookie jar requires set_cookie, url and mutex"
#endif

#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS) && \
	(!defined(XHTTP_FEATURE_COOKIE_JAR) || !defined(XHTTP_FEATURE_HTTP_HEADERS))
	#error "XRT cookie jar headers require cookie_jar and http_headers"
#endif



#if defined(XHTTP_FEATURE_COOKIE_JAR)

/* RFC 6265 要求用户代理把持久 Cookie 的生存期限制在最多 400 天。 */
#define XCOOKIE_JAR_MAX_LIFETIME \
	((xtime)(INT64_C(400) * XRT_TIME_DAY))



/* CookieJar 是引用计数、线程安全的用户代理 Cookie 存储。 */
typedef struct xcookiejar xcookiejar;



/* CookieSnapshot 拥有不可变文本，借用视图在快照销毁前保持有效。 */
typedef struct xcookiesnapshot xcookiesnapshot;



/* 回调接收已经转为小写 ASCII 的域名；返回 true 表示公共后缀。 */
typedef bool (*xcookiepublicsuffixfn)(ptr pContext, xstrview Domain);



/*
 * 高风险显式选项：没有公共后缀回调时仍接受跨子域 Domain Cookie。
 * 这可能把公共后缀误作 Domain 并形成 supercookie；普通客户端不应启用。
 */
#define XCOOKIE_JAR_ALLOW_DOMAIN_WITHOUT_PSL UINT32_C(0x00000001)



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

/* CookieInfo 只借用所属快照，时间使用与 xrt 时间模块一致的毫秒。 */
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



#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)

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



#if defined(XHTTP_FEATURE_COOKIE_JAR)

/* 初始化安全、适合通用 HTTP 客户端的配置；输出可位于合法的未对齐存储。 */
XHTTP_POLICY_API void xrtCookieJarConfigInit(xcookiejarconfig* pConfig);



/* 创建线程安全 CookieJar；配置为空时使用默认值，非空配置会立即复制。 */
XHTTP_POLICY_API xcookiejar* xrtCookieJarCreate(const xcookiejarconfig* pConfig);



/* 增加 CookieJar 引用；失效对象返回空指针。 */
XHTTP_POLICY_API xcookiejar* xrtCookieJarRetain(const xcookiejar* pJar);



/* 释放 CookieJar 引用；空指针是安全的空操作。 */
XHTTP_POLICY_API void xrtCookieJarRelease(xcookiejar* pJar);



/* 删除全部 Cookie，但保留已经分配的数组容量。 */
XHTTP_POLICY_API void xrtCookieJarClear(xcookiejar* pJar);



/* 返回当前条目数；不会隐式清理过期条目。 */
XHTTP_POLICY_API size_t xrtCookieJarCount(const xcookiejar* pJar);



/* 删除在指定时间已经过期的 Cookie，并返回删除数量。 */
XHTTP_POLICY_API size_t xrtCookieJarPurge(xcookiejar* pJar, xtime iNow);



/* 使用已解析 Set-Cookie 更新存储；策略拒绝通过 Reject 返回。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarSet(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	xcookiereject* pReject
);



/* 宽松解析 Set-Cookie 字段值后更新存储。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarStore(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	xstrview SetCookie,
	xcookiereject* pReject
);



/* 使用当前时间和 HTTP API 语义接收一个普通、非分区 Cookie。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarStoreUrl(
	xcookiejar* pJar,
	xstrview URL,
	xstrview SetCookie,
	xcookiereject* pReject
);



/* 按请求上下文写出不含字段名和零结尾的 Cookie 字段值。 */
XHTTP_POLICY_API bool xrtCookieJarWrite(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 按请求上下文分配并构建零结尾 Cookie 字段值。 */
XHTTP_POLICY_API str xrtCookieJarBuild(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	size_t* pSize
);



/* 使用当前时间、同站和 HTTP API 语义写出普通请求 Cookie。 */
XHTTP_POLICY_API bool xrtCookieJarWriteUrl(
	xcookiejar* pJar,
	xstrview URL,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 使用当前时间、同站和 HTTP API 语义构建普通请求 Cookie。 */
XHTTP_POLICY_API str xrtCookieJarBuildUrl(
	xcookiejar* pJar,
	xstrview URL,
	size_t* pSize
);



/* 在锁内复制全部未过期条目，返回并发稳定的不可变快照。 */
XHTTP_POLICY_API xcookiesnapshot* xrtCookieJarSnapshot(
	xcookiejar* pJar,
	xtime iNow
);



/* 销毁快照；空指针是安全的空操作。 */
XHTTP_POLICY_API void xrtCookieSnapshotDestroy(xcookiesnapshot* pSnapshot);



/* 返回快照中的 Cookie 数量。 */
XHTTP_POLICY_API size_t xrtCookieSnapshotCount(const xcookiesnapshot* pSnapshot);



/* 返回稳定借用条目，越界返回空指针且不设置错误。 */
XHTTP_POLICY_API const xcookieinfo* xrtCookieSnapshotAt(
	const xcookiesnapshot* pSnapshot,
	size_t iIndex
);

#endif



#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)

/* 分别处理每个 Set-Cookie 字段，绝不把同名字段合并。 */
XHTTP_POLICY_API bool xrtCookieJarStoreHeaders(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xhttpheaders* pHeaders,
	xcookiestorereport* pReport
);



/* 构建并设置单个 Cookie 字段；没有可发送条目时删除已有字段。 */
XHTTP_POLICY_API bool xrtCookieJarApply(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	xhttpheaders* pHeaders
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_retry_policy.h */
/* ========================================================================== */

#ifndef XHTTP_RETRY_POLICY_H
#define XHTTP_RETRY_POLICY_H


#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY) && !defined(XRT_FEATURE_HTTP)
#error "HTTP retry policy requires the HTTP core"
#endif

XRT_EXTERN_C_BEGIN
#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY)
/* 判断状态码是否属于默认的临时失败重试集合。 */
XHTTP_POLICY_API bool xrtHttpRetryStatusDefault(uint16 iStatus);



/*
	计算以零开始编号的封顶指数退避，单位由调用方决定。
	Maximum 必须非零且 Base 不得大于 Maximum；失败时不修改输出。
*/
XHTTP_POLICY_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
);

#endif
XRT_EXTERN_C_END
#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xrt/http_redirect_policy.h */
/* ========================================================================== */

#ifndef XHTTP_REDIRECT_POLICY_H
#define XHTTP_REDIRECT_POLICY_H


#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS) || \
	 !defined(XHTTP_FEATURE_URL))
#error "HTTP redirect policy requires requests, trailers and URL support"
#endif

#define XHTTP_REDIRECT_MAX_DEFAULT UINT32_C(10)

#define XHTTP_REDIRECT_POST_TO_GET \
	UINT32_C(0x00000001)
#define XHTTP_REDIRECT_FORWARD_CREDENTIALS \
	UINT32_C(0x00000002)
#define XHTTP_REDIRECT_ALLOW_DOWNGRADE \
	UINT32_C(0x00000004)



/* 重定向配置用零跳明确关闭自动跟随，其余字段只调整安全策略。 */
typedef struct xhttpredirectconfig {
	uint32 Flags;
	uint32 MaxHops;
} xhttpredirectconfig;



/* 每次调用可以继承客户端策略、强制跟随、返回原响应或拒绝重定向。 */
typedef enum xhttpredirectmode {
	XHTTP_REDIRECT_DEFAULT = 0,
	XHTTP_REDIRECT_FOLLOW,
	XHTTP_REDIRECT_MANUAL,
	XHTTP_REDIRECT_ERROR
} xhttpredirectmode;



/* 无 IO 的下一跳构建结果；不会改变输入请求。 */
typedef enum xhttpredirecterror {
	XHTTP_REDIRECT_OK = 0,
	XHTTP_REDIRECT_INVALID,
	XHTTP_REDIRECT_REPLAY,
	XHTTP_REDIRECT_DOWNGRADE
} xhttpredirecterror;

XRT_EXTERN_C_BEGIN
#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)
/* 初始化十跳上限和安全重定向策略，允许未对齐完整存储。 */
XHTTP_POLICY_API void xrtHttpRedirectConfigInit(xhttpredirectconfig* pConfig);
/* 判断状态是否为 301、302、303、307 或 308。 */
XHTTP_POLICY_API bool xrtHttpRedirectStatus(uint16 iStatus);
/* 根据 Location 构建独立的下一跳请求，不访问网络。
   继承 fragment，按状态改写方法/正文，拒绝未授权降级和不可重放正文，
   跨 origin 默认去除凭据。成功返回的请求由调用方销毁；失败返回 NULL。
   pError 必须指向可写存储，给出拒绝原因；分配失败仍可读取 XRT 错误链。 */
XHTTP_POLICY_API xhttprequest* xrtHttpRedirectRequest(
	const xhttprequest* pRequest, uint16 iStatus, xstrview Location,
	uint32 iFlags, xhttpredirecterror* pError
);
#endif
XRT_EXTERN_C_END
#endif


/* ========================================================================== */
/* public: extlibs/xhttp-policy/include/xhttp-policy.h */
/* ========================================================================== */

#ifndef XHTTP_POLICY_H
#define XHTTP_POLICY_H



#endif

#endif

#if defined(XHTTP_POLICY_IMPLEMENTATION) && !defined(XHTTP_POLICY_IMPLEMENTATION_ONCE)
#define XHTTP_POLICY_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xhttp_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_COOKIE_JAR) || \
	defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS) || \
	defined(XHTTP_FEATURE_HTTP_RETRY_POLICY) || \
	defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)
#ifndef XHTTP_POLICY_SUPPORT_BRIDGE_H
#define XHTTP_POLICY_SUPPORT_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_COOKIE_JAR) || \
	defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS) || \
	defined(XHTTP_FEATURE_HTTP_RETRY_POLICY) || \
	defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)
#ifndef XHTTP_INTERNAL_RUNTIME_H
#define XHTTP_INTERNAL_RUNTIME_H

#if defined(XRT_FEATURE_SPIN)
#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_cache.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
#ifndef XHTTP_POLICY_XRT_HTTP_CACHE_H
#define XHTTP_POLICY_XRT_HTTP_CACHE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)
#ifndef XHTTP_POLICY_HTTP_INTERNAL_H
#define XHTTP_POLICY_HTTP_INTERNAL_H


#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)
/* 验证 Digest 会话内部存储的输出别名。 */
bool __xrtHttpDigestSessionOutputValid(
 const xhttpdigestsession* pSession,
 const xhttpdigestexchange* pExchange,
 const void* pOutput, size_t iSize
);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_semantics.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
#ifndef XHTTP_POLICY_XRT_HTTP_SEMANTICS_H
#define XHTTP_POLICY_XRT_HTTP_SEMANTICS_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_cache_validate.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
#ifndef XRT_INTERNAL_HTTP_CACHE_VALIDATE_H
#define XRT_INTERNAL_HTTP_CACHE_VALIDATE_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 单个响应的验证元数据全部借用原 Header。 */
typedef struct xrt_http_cache_validator {
	xhttpetag ETag;
	xtime LastModified;
	xtime Date;
	uint64 ContentLength;
	bool ETagPresent;
	bool ETagValid;
	bool LastModifiedPresent;
	bool LastModifiedValid;
	bool LastModifiedStrong;
	bool DateValid;
	bool ContentLengthPresent;
	bool ContentLengthValid;
} xrt_http_cache_validator;



/* 验证借用字段数组的内存与 HTTP 名称和值语法。 */
bool __xrtHttpCacheFieldsValid(
	const xhttpfield* pFields,
	size_t iCount
);



/* 验证缓存条目数组及其中全部借用字段。 */
bool __xrtHttpCacheEntriesValid(
	const xhttpcacheentry* pEntries,
	size_t iCount
);



/* 判断一段内存是否覆盖条目数组或任一借用 Header。 */
bool __xrtHttpCacheEntriesOverlap(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	const void* pMemory,
	size_t iSize
);



/* 从响应 Header 读取 ETag、日期和 Content-Length，不分配内存。 */
bool __xrtHttpCacheValidatorRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
);



/* 判断条目能否满足当前完整或 Range 请求。 */
bool __xrtHttpCacheEntryEligible(
	const xhttpcacheentry* pEntry,
	bool Range
);



/* 精确比较包含强弱标志的两个实体标签。 */
bool __xrtHttpCacheETagExact(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);



/* 按 HTTP 秒精度比较两个日期。 */
bool __xrtHttpCacheDateEqual(
	xtime iLeft,
	xtime iRight
);



/* 返回候选用于“最新响应”排序的 Date 或接收时间。 */
xtime __xrtHttpCacheEntryDate(
	const xhttpcacheentry* pEntry,
	const xrt_http_cache_validator* pValidator
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_origin.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)
#ifndef XHTTP_POLICY_ORIGIN_BRIDGE_H
#define XHTTP_POLICY_ORIGIN_BRIDGE_H
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_url.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE) || \
	defined(XHTTP_FEATURE_COOKIE_JAR) || \
	defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)
#ifndef XHTTP_POLICY_XRT_URL_H
#define XHTTP_POLICY_XRT_URL_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_cache_range.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)
#ifndef XRT_INTERNAL_HTTP_CACHE_RANGE_H
#define XRT_INTERNAL_HTTP_CACHE_RANGE_H





#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 判断闭区间是否合法。 */
bool __xrtHttpCacheRangeValid(
	const xhttpbyterange* pRange
);



/* 判断两个闭区间是否重叠或相邻。 */
bool __xrtHttpCacheRangeJoins(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
);



#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_cache_store.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
#ifndef XRT_INTERNAL_HTTP_CACHE_STORE_H
#define XRT_INTERNAL_HTTP_CACHE_STORE_H


#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)
#endif



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 每个 Vary 维度引用 Record 内连续保存的原请求字段。 */
typedef struct xrt_http_cache_vary {
	xstrview Name;
	size_t Field;
	size_t FieldCount;
} xrt_http_cache_vary;



/* 不可变 Record 的数组、文本和正文全部位于结构后的同一分配中。 */
struct xhttpcacherecord {
	volatile int32 References;
	size_t Charge;
	xhttpcachekey Key;
	xhttpversion Version;
	uint16 Status;
	uint32 Flags;
	xstrview Reason;
	xhttpfield* Fields;
	size_t FieldCount;
	xhttpfield* Trailers;
	size_t TrailerCount;
	xhttpcachepart* Parts;
	size_t PartCount;
	uint64 BodyBytes;
	uint64 Length;
	xtime ResponseTime;
	double RequestClock;
	double ResponseClock;
	xtime SelectionTime;
	xrt_http_cache_vary* Vary;
	size_t VaryCount;
};



/* 验证主键视图、方法、URI 和字段数组，并复制到对齐快照。 */
bool __xrtHttpCacheKeyResolve(
	const xhttpcachekey* pInput,
	xhttpcachekey* pKey
);



/* 判断一段内存是否覆盖已验证主键或任一借用内容。 */
bool __xrtHttpCacheKeyOverlap(
	const xhttpcachekey* pKey,
	const void* pMemory,
	size_t iSize
);



/* 判断已校验主键和全部 Vary 原请求字段是否匹配。 */
bool __xrtHttpCacheRecordMatchesValid(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
);



/* 比较两个 Record 是否描述同一个 Vary 变体。 */
bool __xrtHttpCacheRecordVariantEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
);



/* 返回用于多个匹配响应之间选择最新项的 Date 或接收时间。 */
xtime __xrtHttpCacheRecordSelectionTime(
	const xhttpcacherecord* pRecord
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_http_client.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_cookie.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_COOKIE_JAR) || \
	defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)
#ifndef XHTTP_POLICY_XRT_COOKIE_H
#define XHTTP_POLICY_XRT_COOKIE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-policy/src/internal/xrt_cookie_jar.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_COOKIE_JAR) || \
	defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)
#ifndef XRT_INTERNAL_COOKIE_JAR_H
#define XRT_INTERNAL_COOKIE_JAR_H





#if defined(XHTTP_FEATURE_COOKIE_JAR)

/* CookieJar 条目拥有全部文本，不向可变存储之外暴露借用视图。 */
typedef struct xrt_cookie_entry {
	str Storage;
	str Name;
	str Value;
	str Domain;
	str Path;
	str PartitionKey;
	size_t NameSize;
	size_t ValueSize;
	size_t DomainSize;
	size_t PathSize;
	size_t PartitionKeySize;
	xtime Expires;
	xtime Created;
	xtime Accessed;
	uint64 CreationOrder;
	uint32 Flags;
	xcookiesamesite SameSite;
	xcookiepriority Priority;
} xrt_cookie_entry;



/* 解析后的请求源只在一次公开调用期间借用 URL path。 */
typedef struct xrt_cookie_origin {
	str Host;
	size_t HostSize;
	xstrview Path;
	bool Secure;
	bool IpHost;
} xrt_cookie_origin;



/* CookieJar 使用系统 mutex，不再自行睡眠轮询原子锁。 */
struct xcookiejar {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_cookie_entry* Cookies;
	size_t Count;
	size_t Capacity;
	uint64 Sequence;
	xcookiejarconfig Config;
};



/* 快照把 info 数组和文本放在一个分配块中。 */
struct xcookiesnapshot {
	size_t Count;
	xcookieinfo Cookies[1];
};



/* 验证并规范化 Cookie URL，Host 由调用方释放。 */
bool __xrtCookieOriginParse(xstrview URL, xrt_cookie_origin* pOrigin);



/* 释放规范化 URL 中拥有的 Host。 */
void __xrtCookieOriginClear(xrt_cookie_origin* pOrigin);

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_policy.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)




#if defined(XHTTP_FEATURE_HTTP_CACHE_POLICY)

#define XRT_HTTP_CACHE_STORE_FLAGS \
	((uint32)XHTTP_CACHE_STORE_SHARED | \
	 (uint32)XHTTP_CACHE_STORE_AUTHORIZATION | \
	 (uint32)XHTTP_CACHE_STORE_METHOD_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_STORE_METHOD_CACHEABLE | \
	 (uint32)XHTTP_CACHE_STORE_STATUS_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_STORE_STATUS_HEURISTIC | \
	 (uint32)XHTTP_CACHE_STORE_HEADERS_COMPLETE | \
	 (uint32)XHTTP_CACHE_STORE_RESPONSE_COMPLETE | \
	 (uint32)XHTTP_CACHE_STORE_RANGE_SUPPORTED | \
	 (uint32)XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION_FRESHNESS | \
	 (uint32)XHTTP_CACHE_STORE_EXTENSION_OVERRIDE)

#define XRT_HTTP_CACHE_USE_FLAGS \
	((uint32)XHTTP_CACHE_USE_SHARED | \
	 (uint32)XHTTP_CACHE_USE_CANDIDATE_MATCH | \
	 (uint32)XHTTP_CACHE_USE_REPRESENTATION | \
	 (uint32)XHTTP_CACHE_USE_CLOCK | \
	 (uint32)XHTTP_CACHE_USE_VALIDATED | \
	 (uint32)XHTTP_CACHE_USE_DISCONNECTED | \
	 (uint32)XHTTP_CACHE_USE_STALE_ALLOWED | \
	 (uint32)XHTTP_CACHE_USE_EXTENSION | \
	 (uint32)XHTTP_CACHE_USE_STATUS_UNDERSTOOD | \
	 (uint32)XHTTP_CACHE_USE_AUTHORIZATION)

#define XRT_HTTP_CACHE_STORE_ACTIONS \
	((uint32)XHTTP_CACHE_STORE_REMOVE_CONNECTION | \
	 (uint32)XHTTP_CACHE_STORE_REMOVE_PROXY | \
	 (uint32)XHTTP_CACHE_STORE_SEPARATE_TRAILERS)



/* 判断方法是否等于一个标准大写方法名。 */
static bool __xrtHttpCachePolicyMethod(
	xstrview Method,
	xstrview Expected
)
{
	return xrtHttpMethodEqual(Method, Expected);
}



/* 判断一个已知指令存在且参数有效，并按需拒绝重复值。 */
static bool __xrtHttpCachePolicyDirective(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive,
	bool Unique
)
{
	uint32 iDirective = (uint32)Directive;

	return
		((pControl->Flags & iDirective) != 0) &&
		((pControl->InvalidDirectives & iDirective) == 0) &&
		(!Unique ||
		 ((pControl->DuplicateDirectives &
		   iDirective) == 0));
}



/* 判断单值限定指令是否可靠地携带字段名列表。 */
static bool __xrtHttpCachePolicyFields(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive,
	uint32 iFieldFlag
)
{
	return
		__xrtHttpCachePolicyDirective(
			pControl, Directive, true
		) &&
		((pControl->Flags & iFieldFlag) != 0);
}



/* 判断一个数值请求指令是否存在但不能可靠解释。 */
static bool __xrtHttpCachePolicyUnreliable(
	const xhttpcachecontrol* pControl,
	xhttpcachedirective Directive
)
{
	uint32 iDirective = (uint32)Directive;

	return
		((pControl->Flags & iDirective) != 0) &&
		!__xrtHttpCachePolicyDirective(
			pControl, Directive, true
		);
}



/* 判断状态码是否由当前公共状态表完整识别。 */
static bool __xrtHttpCachePolicyStatusKnown(uint16 iStatus)
{
	return xrtHttpStatusText(iStatus).Size != 0;
}



/* 验证存储输入结构及方法借用边界。 */
static bool __xrtHttpCacheStoreInputValid(
	const xhttpcachestoreinput* pInput
)
{
	if ( (pInput == NULL) ||
		!__xhttpViewValid(pInput->Method) ||
		!xrtHttpTokenValid(pInput->Method) ||
		(pInput->Status < 100) ||
		(pInput->Status > 999) ||
		((pInput->Flags &
		  ~XRT_HTTP_CACHE_STORE_FLAGS) != 0) ||
		xrtMemRangesOverlap(
			pInput, sizeof(*pInput),
			pInput->Method.Data,
			pInput->Method.Size
		) ) {
		return false;
	}
	return
		((pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_CACHEABLE) == 0) ||
		((pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_UNDERSTOOD) != 0);
}



/* 验证复用输入结构。 */
static bool __xrtHttpCacheUseInputValid(
	const xhttpcacheuseinput* pInput
)
{
	return
		(pInput != NULL) &&
		(pInput->Status >= 100) &&
		(pInput->Status <= 999) &&
		((pInput->Flags &
		  ~XRT_HTTP_CACHE_USE_FLAGS) == 0);
}



/* 判断响应是否具有当前缓存可以依赖的显式新鲜寿命。 */
static bool __xrtHttpCachePolicyExplicit(
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	bool Shared,
	bool Extension
)
{
	return
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_MAX_AGE, true
		) ||
		(Shared &&
		 __xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_S_MAXAGE, true
		 )) ||
		(((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES) != 0) &&
		 ((pTime->Flags &
		   XHTTP_CACHE_TIME_EXPIRES_DUPLICATE) == 0)) ||
		Extension;
}



/* 判断响应具有 RFC 9111 存储许可或调用方扩展许可。 */
static bool __xrtHttpCachePolicyStorable(
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput
)
{
	bool bShared =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_SHARED) != 0;

	return
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PUBLIC, false
		) ||
		(!bShared &&
		 __xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PRIVATE, true
		 )) ||
		__xrtHttpCachePolicyExplicit(
			pResponse, pTime, bShared,
			(pInput->Flags &
			 XHTTP_CACHE_STORE_EXTENSION) != 0
		) ||
		((pInput->Flags &
		  XHTTP_CACHE_STORE_STATUS_HEURISTIC) != 0);
}



/* 判断共享缓存可以保存带 Authorization 请求的响应。 */
static bool __xrtHttpCachePolicyAuthorized(
	const xhttpcachecontrol* pResponse
)
{
	return
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_REVALIDATE,
			false
		) ||
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_PUBLIC, false
		) ||
		__xrtHttpCachePolicyDirective(
			pResponse, XHTTP_CACHE_S_MAXAGE, true
		);
}



/* 按 only-if-cached 和线路状态结束一个需要源站的决定。 */
static xhttpcacheusedecision __xrtHttpCachePolicyOrigin(
	xhttpcacheuseplan* pPlan,
	uint32 iReason,
	bool Validate,
	bool OnlyIfCached,
	bool Disconnected
)
{
	pPlan->Reasons |= iReason;
	if ( OnlyIfCached || Disconnected ) {
		if ( OnlyIfCached ) {
			pPlan->Reasons |=
				XHTTP_CACHE_REASON_ONLY_IF_CACHED;
		}
		if ( Disconnected ) {
			pPlan->Reasons |=
				XHTTP_CACHE_REASON_DISCONNECTED;
		}
		pPlan->Decision =
			XHTTP_CACHE_USE_GATEWAY_TIMEOUT;
	} else {
		pPlan->Decision = Validate ?
			XHTTP_CACHE_USE_VALIDATE :
			XHTTP_CACHE_USE_FORWARD;
	}
	return pPlan->Decision;
}



/* 发布一个可直接使用的缓存响应计划。 */
static xhttpcacheusedecision __xrtHttpCachePolicyUse(
	xhttpcacheuseplan* pPlan,
	uint32 iActions,
	uint32 iReasons,
	uint64 iStaleBy
)
{
	pPlan->StaleBy = iStaleBy;
	pPlan->Decision = XHTTP_CACHE_USE_STORED;
	pPlan->Actions =
		XHTTP_CACHE_USE_SET_AGE | iActions;
	pPlan->Reasons = iReasons;
	return pPlan->Decision;
}



/* 判断标准方法是否具有当前模块实现的缓存语义。 */
XHTTP_POLICY_API bool xrtHttpCacheMethodDefault(xstrview Method)
{
	if ( !__xhttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ) {
		return false;
	}
	return
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("GET")
		) ||
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("HEAD")
		) ||
		__xrtHttpCachePolicyMethod(
			Method, XRT_STR_LITERAL("POST")
		);
}



/* 判断标准状态是否允许启发式缓存。 */
XHTTP_POLICY_API bool xrtHttpCacheStatusHeuristic(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_OK:
		case XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION:
		case XHTTP_STATUS_NO_CONTENT:
		case XHTTP_STATUS_PARTIAL_CONTENT:
		case XHTTP_STATUS_MULTIPLE_CHOICES:
		case XHTTP_STATUS_MOVED_PERMANENTLY:
		case XHTTP_STATUS_PERMANENT_REDIRECT:
		case XHTTP_STATUS_NOT_FOUND:
		case XHTTP_STATUS_METHOD_NOT_ALLOWED:
		case XHTTP_STATUS_GONE:
		case XHTTP_STATUS_URI_TOO_LONG:
		case XHTTP_STATUS_NOT_IMPLEMENTED:
			return true;

		default:
			return false;
	}
}



/* 建立标准方法和状态的默认存储能力。 */
XHTTP_POLICY_API bool xrtHttpCacheStoreInputInit(
	xhttpcachestoreinput* pInput,
	xstrview Method,
	uint16 iStatus,
	bool Shared
)
{
	xhttpcachestoreinput Input;

	if ( (pInput == NULL) ||
		!__xhttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		(iStatus < 100) ||
		(iStatus > 999) ||
		xrtMemRangesOverlap(
			pInput, sizeof(*pInput),
			Method.Data, Method.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Method = Method;
	Input.Status = iStatus;
	Input.Flags =
		XHTTP_CACHE_STORE_HEADERS_COMPLETE |
		XHTTP_CACHE_STORE_RESPONSE_COMPLETE;
	if ( Shared ) {
		Input.Flags |= XHTTP_CACHE_STORE_SHARED;
	}
	if ( xrtHttpCacheMethodDefault(Method) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_METHOD_UNDERSTOOD |
			XHTTP_CACHE_STORE_METHOD_CACHEABLE;
	}
	if ( __xrtHttpCachePolicyStatusKnown(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_STATUS_UNDERSTOOD;
	}
	if ( xrtHttpCacheStatusHeuristic(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_STORE_STATUS_HEURISTIC;
	}
	*pInput = Input;
	return true;
}



/* 按协议事实建立存储计划。 */
XHTTP_POLICY_API xhttpcachestoredecision xrtHttpCacheStorePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcachetime* pTime,
	const xhttpcachestoreinput* pInput,
	xhttpcachestoreplan* pPlan
)
{
	xhttpcachestoreplan Plan;
	uint32 iActions = XRT_HTTP_CACHE_STORE_ACTIONS;
	bool bShared;
	bool bGet;
	bool bPost;
	bool bStatusUnderstood;
	bool bMustUnderstand;
	bool bOverride;

	if ( !xrtHttpCacheControlValid(pRequest) ||
		!xrtHttpCacheControlValid(pResponse) ||
		!xrtHttpCacheTimeValid(pTime) ||
		!__xrtHttpCacheStoreInputValid(pInput) ||
		(pPlan == NULL) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pRequest, sizeof(*pRequest)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pResponse, sizeof(*pResponse)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pTime, sizeof(*pTime)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pInput, sizeof(*pInput)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_STORE_ERROR;
	}
	memset(&Plan, 0, sizeof(Plan));
	bShared =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_SHARED) != 0;
	bGet = __xrtHttpCachePolicyMethod(
		pInput->Method, XRT_STR_LITERAL("GET")
	);
	bPost = __xrtHttpCachePolicyMethod(
		pInput->Method, XRT_STR_LITERAL("POST")
	);
	bStatusUnderstood =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_STATUS_UNDERSTOOD) != 0;
	bMustUnderstand =
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_UNDERSTAND,
			false
		);
	bOverride =
		(pInput->Flags &
		 XHTTP_CACHE_STORE_EXTENSION_OVERRIDE) != 0;
	if ( bOverride ) {
		Plan.Decision = XHTTP_CACHE_STORE_KEEP;
		Plan.Actions = iActions;
		Plan.Reasons = XHTTP_CACHE_REASON_EXTENSION;
		*pPlan = Plan;
		return Plan.Decision;
	}

	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_UNDERSTOOD) == 0 ) {
		Plan.Reasons |= XHTTP_CACHE_REASON_METHOD_UNKNOWN;
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_METHOD_CACHEABLE) == 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_METHOD_NOT_CACHEABLE;
	}
	if ( pInput->Status < 200 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_STATUS_NOT_FINAL;
	}
	if ( (((pInput->Status == XHTTP_STATUS_PARTIAL_CONTENT) ||
		   (pInput->Status == XHTTP_STATUS_NOT_MODIFIED) ||
		   bMustUnderstand)) &&
		!bStatusUnderstood ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_STATUS_NOT_UNDERSTOOD;
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_HEADERS_COMPLETE) == 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_HEADERS_INCOMPLETE;
	}
	if ( (pRequest->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_REQUEST_NO_STORE;
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 ) {
		if ( bMustUnderstand && bStatusUnderstood ) {
			iActions |=
				XHTTP_CACHE_STORE_IGNORE_NO_STORE;
		} else {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_RESPONSE_NO_STORE;
		}
	}
	if ( bShared &&
		((pResponse->Flags &
		  XHTTP_CACHE_PRIVATE) != 0) ) {
		if ( __xrtHttpCachePolicyFields(
			pResponse,
			XHTTP_CACHE_PRIVATE,
			XHTTP_CACHE_PRIVATE_FIELDS
		) ) {
			iActions |=
				XHTTP_CACHE_STORE_REMOVE_PRIVATE;
		} else {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_SHARED_PRIVATE;
		}
	}
	if ( __xrtHttpCachePolicyFields(
		pResponse,
		XHTTP_CACHE_NO_CACHE,
		XHTTP_CACHE_NO_CACHE_FIELDS
	) ) {
		iActions |=
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE;
	}
	if ( bShared &&
		((pInput->Flags &
		  XHTTP_CACHE_STORE_AUTHORIZATION) != 0) &&
		!__xrtHttpCachePolicyAuthorized(pResponse) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_AUTHORIZATION;
	}
	if ( !__xrtHttpCachePolicyStorable(
		pResponse, pTime, pInput
	) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_NO_PERMISSION;
	}
	if ( bPost &&
		(!__xrtHttpCachePolicyExplicit(
			pResponse, pTime, bShared,
			(pInput->Flags &
			 XHTTP_CACHE_STORE_EXTENSION_FRESHNESS) != 0
		 ) ||
		 ((pInput->Flags &
		   XHTTP_CACHE_STORE_CONTENT_LOCATION_MATCH) == 0)) ) {
		Plan.Reasons |=
			XHTTP_CACHE_REASON_POST_REQUIREMENTS;
	}
	if ( pInput->Status ==
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		if ( !bGet ||
			((pInput->Flags &
			  XHTTP_CACHE_STORE_RANGE_SUPPORTED) == 0) ) {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_PARTIAL_UNSUPPORTED;
		} else {
			iActions |=
				XHTTP_CACHE_STORE_MARK_INCOMPLETE |
				XHTTP_CACHE_STORE_AS_200;
		}
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_STORE_RESPONSE_COMPLETE) == 0 ) {
		if ( !bGet ||
			((pInput->Status != XHTTP_STATUS_OK) &&
			 (pInput->Status !=
			  XHTTP_STATUS_PARTIAL_CONTENT)) ||
			((pInput->Flags &
			  XHTTP_CACHE_STORE_RANGE_SUPPORTED) == 0) ) {
			Plan.Reasons |=
				XHTTP_CACHE_REASON_RESPONSE_INCOMPLETE;
		} else {
			iActions |=
				XHTTP_CACHE_STORE_MARK_INCOMPLETE;
		}
	}
	if ( Plan.Reasons == XHTTP_CACHE_REASON_NONE ) {
		Plan.Decision = XHTTP_CACHE_STORE_KEEP;
		Plan.Actions = iActions;
	} else {
		Plan.Decision = XHTTP_CACHE_STORE_SKIP;
	}
	*pPlan = Plan;
	return Plan.Decision;
}



/* 建立标准状态的默认复用环境。 */
XHTTP_POLICY_API bool xrtHttpCacheUseInputInit(
	xhttpcacheuseinput* pInput,
	uint16 iStatus,
	bool Shared
)
{
	xhttpcacheuseinput Input;

	if ( (pInput == NULL) ||
		(iStatus < 100) ||
		(iStatus > 999) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Status = iStatus;
	Input.Flags =
		XHTTP_CACHE_USE_CANDIDATE_MATCH |
		XHTTP_CACHE_USE_REPRESENTATION |
		XHTTP_CACHE_USE_CLOCK;
	if ( Shared ) {
		Input.Flags |= XHTTP_CACHE_USE_SHARED;
	}
	if ( __xrtHttpCachePolicyStatusKnown(iStatus) ) {
		Input.Flags |=
			XHTTP_CACHE_USE_STATUS_UNDERSTOOD;
	}
	*pInput = Input;
	return true;
}



/* 按缓存约束建立复用、验证、转发或 504 计划。 */
XHTTP_POLICY_API xhttpcacheusedecision xrtHttpCacheUsePlan(
	const xhttpcachecontrol* pRequest,
	const xhttpcachecontrol* pResponse,
	const xhttpcacheage* pAge,
	const xhttpcachefreshness* pFreshness,
	const xhttpcacheuseinput* pInput,
	xhttpcacheuseplan* pPlan
)
{
	uint32 iUseActions = 0;
	bool bShared;
	bool bOnlyIfCached;
	bool bDisconnected;
	bool bMustUnderstand;
	bool bStatusUnderstood;
	bool bFresh;
	bool bStaleAllowed;
	uint64 iStaleBy;

	if ( !xrtHttpCacheControlValid(pRequest) ||
		!xrtHttpCacheControlValid(pResponse) ||
		!xrtHttpCacheAgeValid(pAge) ||
		!xrtHttpCacheFreshnessValid(pFreshness) ||
		!__xrtHttpCacheUseInputValid(pInput) ||
		(pPlan == NULL) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pRequest, sizeof(*pRequest)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pResponse, sizeof(*pResponse)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pAge, sizeof(*pAge)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pFreshness, sizeof(*pFreshness)
		) ||
		xrtMemRangesOverlap(
			pPlan, sizeof(*pPlan),
			pInput, sizeof(*pInput)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_USE_ERROR;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	bShared =
		(pInput->Flags &
		 XHTTP_CACHE_USE_SHARED) != 0;
	bOnlyIfCached =
		(pRequest->Flags &
		 XHTTP_CACHE_ONLY_IF_CACHED) != 0;
	bDisconnected =
		(pInput->Flags &
		 XHTTP_CACHE_USE_DISCONNECTED) != 0;
	bMustUnderstand =
		__xrtHttpCachePolicyDirective(
			pResponse,
			XHTTP_CACHE_MUST_UNDERSTAND,
			false
		);
	bStatusUnderstood =
		(pInput->Flags &
		 XHTTP_CACHE_USE_STATUS_UNDERSTOOD) != 0;

	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_CANDIDATE_MATCH) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_CANDIDATE_MISS,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_REPRESENTATION) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REPRESENTATION_UNUSABLE,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_EXTENSION) != 0 ) {
		return __xrtHttpCachePolicyUse(
			pPlan, 0,
			XHTTP_CACHE_REASON_EXTENSION, 0
		);
	}
	if ( bShared &&
		((pInput->Flags &
		  XHTTP_CACHE_USE_AUTHORIZATION) != 0) &&
		!__xrtHttpCachePolicyAuthorized(pResponse) ) {
		pPlan->Actions = XHTTP_CACHE_USE_EVICT;
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_AUTHORIZATION,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_STORE) != 0 &&
		!(bMustUnderstand && bStatusUnderstood) ) {
		pPlan->Actions = XHTTP_CACHE_USE_EVICT;
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_RESPONSE_NO_STORE,
			false,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_VALIDATED) != 0 ) {
		return __xrtHttpCachePolicyUse(
			pPlan, 0, XHTTP_CACHE_REASON_NONE, 0
		);
	}
	if ( (pInput->Flags &
		  XHTTP_CACHE_USE_CLOCK) == 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_NO_CLOCK,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pRequest->Flags &
		  XHTTP_CACHE_NO_CACHE) != 0 ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REQUEST_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}
	if ( (pResponse->Flags &
		  XHTTP_CACHE_NO_CACHE) != 0 ) {
		if ( __xrtHttpCachePolicyFields(
			pResponse,
			XHTTP_CACHE_NO_CACHE,
			XHTTP_CACHE_NO_CACHE_FIELDS
		) ) {
			iUseActions |=
				XHTTP_CACHE_USE_REMOVE_NO_CACHE;
		} else {
			return __xrtHttpCachePolicyOrigin(
				pPlan,
				XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
				true,
				bOnlyIfCached,
				bDisconnected
			);
		}
	}
	if ( __xrtHttpCachePolicyUnreliable(
			pRequest, XHTTP_CACHE_MAX_AGE
		) ||
		__xrtHttpCachePolicyUnreliable(
			pRequest, XHTTP_CACHE_MIN_FRESH
		) ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_REQUEST_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	bFresh =
		(pFreshness->Source !=
		 XHTTP_CACHE_FRESHNESS_NONE) &&
		(pFreshness->Lifetime > pAge->CurrentAge);
	if ( bFresh &&
		__xrtHttpCachePolicyDirective(
			pRequest, XHTTP_CACHE_MAX_AGE, true
		) ) {
		bFresh =
			pAge->CurrentAge <=
			xrtHttpBridgeCacheTimeSeconds(
				pRequest->MaxAge
			);
	}
	if ( bFresh &&
		__xrtHttpCachePolicyDirective(
			pRequest, XHTTP_CACHE_MIN_FRESH, true
		) ) {
		bFresh =
			pFreshness->Lifetime >=
			xrtHttpBridgeCacheTimeAdd(
				pAge->CurrentAge,
				xrtHttpBridgeCacheTimeSeconds(
					pRequest->MinFresh
				)
			);
	}
	if ( bFresh ) {
		return __xrtHttpCachePolicyUse(
			pPlan,
			iUseActions,
			XHTTP_CACHE_REASON_NONE,
			0
		);
	}
	if ( pFreshness->Source ==
		XHTTP_CACHE_FRESHNESS_NONE ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	iStaleBy =
		pAge->CurrentAge > pFreshness->Lifetime ?
		pAge->CurrentAge - pFreshness->Lifetime :
		0;
	pPlan->Reasons |= XHTTP_CACHE_REASON_STALE;
	if ( ((pResponse->Flags &
		   XHTTP_CACHE_MUST_REVALIDATE) != 0) ||
		(bShared &&
		 ((pResponse->Flags & (
			XHTTP_CACHE_PROXY_REVALIDATE |
			XHTTP_CACHE_S_MAXAGE
		  )) != 0)) ) {
		return __xrtHttpCachePolicyOrigin(
			pPlan,
			XHTTP_CACHE_REASON_STALE |
			XHTTP_CACHE_REASON_RESPONSE_REVALIDATE,
			true,
			bOnlyIfCached,
			bDisconnected
		);
	}

	bStaleAllowed =
		bDisconnected ||
		((pInput->Flags &
		  XHTTP_CACHE_USE_STALE_ALLOWED) != 0);
	if ( __xrtHttpCachePolicyDirective(
		pRequest, XHTTP_CACHE_MAX_STALE, true
	) ) {
		if ( (pRequest->Flags &
			  XHTTP_CACHE_MAX_STALE_ANY) != 0 ) {
			bStaleAllowed = true;
		} else if ( iStaleBy <=
			xrtHttpBridgeCacheTimeSeconds(
				pRequest->MaxStale
			) ) {
			bStaleAllowed = true;
		}
	}
	if ( bStaleAllowed ) {
		return __xrtHttpCachePolicyUse(
			pPlan,
			iUseActions |
				XHTTP_CACHE_USE_STALE,
			XHTTP_CACHE_REASON_STALE,
			iStaleBy
		);
	}
	return __xrtHttpCachePolicyOrigin(
		pPlan,
		XHTTP_CACHE_REASON_STALE,
		true,
		bOnlyIfCached,
		bDisconnected
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_validate.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断字段名称是否匹配指定 ASCII 字面量。 */
static bool __xrtHttpCacheFieldName(
	const xhttpfield* pField,
	xstrview Name
)
{
	return xrtHttpFieldNameEqual(pField->Name, Name);
}



/* 把 HTTP 日期向负无穷取整为整秒。 */
static int64 __xrtHttpCacheDateSecond(xtime iTime)
{
	return xrtTimeUnix(iTime);
}



/* 扫描不可重复的 ETag 字段；重复或非法值只会使验证器不可用。 */
static void __xrtHttpCacheETagRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i], XRT_STR_LITERAL("ETag")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	pValidator->ETagPresent = iMatches != 0;
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		pValidator->ETagValid = xrtHttpBridgeETagParseValue(
			Value, &pValidator->ETag
		);
	}
}



/* 扫描 Last-Modified，并结合单个有效 Date 判断它能否作为强验证器。 */
static void __xrtHttpCacheLastModifiedRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	xhttpcachetime Time;
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("Last-Modified")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	pValidator->LastModifiedPresent =
		iMatches != 0;
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		pValidator->LastModifiedValid =
			xrtTimeTryParseHTTPDate(
				Value,
				&pValidator->LastModified
			);
	}

	/* Date 已由缓存时间模块按单值规则解析。 */
	if ( !xrtHttpCacheTimeParse(
		pFields, iCount, &Time
	) ) {
		return;
	}
	pValidator->DateValid =
		(Time.DateCount == 1) &&
		((Time.Flags & (
			XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID
		 )) == 0);
	if ( pValidator->DateValid ) {
		pValidator->Date = Time.Date;
	}
	if ( pValidator->LastModifiedValid &&
		pValidator->DateValid ) {
		int64 iDate = __xrtHttpCacheDateSecond(
			pValidator->Date
		);
		int64 iModified = __xrtHttpCacheDateSecond(
			pValidator->LastModified
		);

		pValidator->LastModifiedStrong =
			(iDate > iModified);
	}
}



/* 扫描一个或多个语义相同的 Content-Length 字段。 */
static void __xrtHttpCacheContentLengthRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	uint64 iLength = 0;
	uint64 iValue;
	bool bHaveValue = false;
	size_t i;

	pValidator->ContentLengthValid = true;
	for ( i = 0; i < iCount; i++ ) {
		bool bValid;
		xerror* pPrevious;

		if ( !__xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("Content-Length")
		) ) {
			continue;
		}
		pValidator->ContentLengthPresent = true;
		pPrevious = xrtTakeError();
		bValid = xrtHttpContentLengthParse(pFields[i].Value, &iValue);
		xrtClearError();
		xrtSetErrorTake(pPrevious);
		if ( !bValid ||
			(bHaveValue && (iValue != iLength)) ) {
			pValidator->ContentLengthValid = false;
			continue;
		}
		iLength = iValue;
		bHaveValue = true;
	}
	if ( !pValidator->ContentLengthPresent ||
		!bHaveValue ) {
		pValidator->ContentLengthValid =
			!pValidator->ContentLengthPresent;
	}
	if ( pValidator->ContentLengthValid &&
		pValidator->ContentLengthPresent ) {
		pValidator->ContentLength = iLength;
	}
}



/* 判断缓存条目视图、字段数组和正文范围标志是否自洽。 */
XHTTP_POLICY_API bool xrtHttpCacheEntryValid(
	const xhttpcacheentry* pEntry
)
{
	if ( (pEntry == NULL) ||
		((pEntry->Flags & ~(
			XHTTP_CACHE_ENTRY_PARTIAL |
			XHTTP_CACHE_ENTRY_RANGE_COVERED
		 )) != 0) ||
		(((pEntry->Flags &
		   XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0) &&
		 ((pEntry->Flags &
		   XHTTP_CACHE_ENTRY_PARTIAL) == 0)) ) {
		return false;
	}
	return __xrtHttpCacheFieldsValid(
		pEntry->Fields, pEntry->FieldCount
	);
}



/* 验证借用字段数组的内存与 HTTP 名称和值语法。 */
bool __xrtHttpCacheFieldsValid(
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	if ( !__xhttpFieldArrayValid(
		pFields, iCount
	) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpTokenValid(
			pFields[i].Name
		) || !xrtHttpFieldValueValid(
			pFields[i].Value
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证缓存条目数组及其中全部借用字段。 */
bool __xrtHttpCacheEntriesValid(
	const xhttpcacheentry* pEntries,
	size_t iCount
)
{
	size_t i;

	if ( ((pEntries == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pEntries))) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpCacheEntryValid(&pEntries[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断一段内存是否覆盖条目数组或任一借用 Header。 */
bool __xrtHttpCacheEntriesOverlap(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	const void* pMemory,
	size_t iSize
)
{
	size_t i;

	if ( xrtMemRangesOverlap(
		pEntries,
		iCount * sizeof(*pEntries),
		pMemory,
		iSize
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xhttpFieldArrayOverlap(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			pMemory,
			iSize
		) ) {
			return true;
		}
	}
	return false;
}



/* 从响应 Header 读取 ETag、日期和 Content-Length，不分配内存。 */
bool __xrtHttpCacheValidatorRead(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cache_validator* pValidator
)
{
	if ( (pValidator == NULL) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iCount
		) ) {
		return false;
	}
	memset(pValidator, 0, sizeof(*pValidator));
	__xrtHttpCacheETagRead(
		pFields, iCount, pValidator
	);
	__xrtHttpCacheLastModifiedRead(
		pFields, iCount, pValidator
	);
	__xrtHttpCacheContentLengthRead(
		pFields, iCount, pValidator
	);
	return true;
}



/* 判断条目能否满足当前完整或 Range 请求。 */
bool __xrtHttpCacheEntryEligible(
	const xhttpcacheentry* pEntry,
	bool Range
)
{
	if ( (pEntry->Flags &
		 XHTTP_CACHE_ENTRY_PARTIAL) == 0 ) {
		return true;
	}
	return Range &&
		((pEntry->Flags &
		  XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0);
}



/* 精确比较包含强弱标志的两个实体标签。 */
bool __xrtHttpCacheETagExact(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
)
{
	return (pLeft->Weak == pRight->Weak) &&
		(pLeft->Opaque.Size == pRight->Opaque.Size) &&
		((pLeft->Opaque.Size == 0) ||
		 (memcmp(
			pLeft->Opaque.Data,
			pRight->Opaque.Data,
			pLeft->Opaque.Size
		 ) == 0));
}



/* 按 HTTP 秒精度比较两个日期。 */
bool __xrtHttpCacheDateEqual(
	xtime iLeft,
	xtime iRight
)
{
	return __xrtHttpCacheDateSecond(iLeft) ==
		__xrtHttpCacheDateSecond(iRight);
}



/* 返回候选用于“最新响应”排序的 Date 或接收时间。 */
xtime __xrtHttpCacheEntryDate(
	const xhttpcacheentry* pEntry,
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->DateValid ?
		pValidator->Date :
		pEntry->ResponseTime;
}



/* 判断一个可用 ETag 是否已经由更早的可用条目贡献。 */
static bool __xrtHttpCacheETagEarlier(
	const xhttpcacheentry* pEntries,
	size_t iIndex,
	bool Range,
	const xhttpetag* pTag
)
{
	xrt_http_cache_validator Validator;
	size_t i;

	for ( i = 0; i < iIndex; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( Validator.ETagValid &&
			xrtHttpETagWeakEqual(
				&Validator.ETag, pTag
			) ) {
			return true;
		}
	}
	return false;
}



/* 累加验证计划中的 ETag 条目和线路长度。 */
static bool __xrtHttpCacheValidateTagAdd(
	xhttpcachevalidateplan* pPlan,
	const xhttpetag* pTag
)
{
	size_t iTagSize;
	size_t iWrapper;
	size_t iAdd;

	iWrapper = pTag->Weak ? 4u : 2u;
	if ( pTag->Opaque.Size > (SIZE_MAX - iWrapper) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iTagSize = pTag->Opaque.Size + iWrapper;
	if ( (pPlan->ETagCount != 0) &&
		(iTagSize > (SIZE_MAX - 2u)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iAdd = iTagSize +
		((pPlan->ETagCount == 0) ? 0u : 2u);
	if ( pPlan->ETagSize > (SIZE_MAX - iAdd) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	pPlan->ETagSize += iAdd;
	pPlan->ETagCount++;
	return true;
}



/* 生成验证计划的内部实现同时供长度写出复用。 */
static bool __xrtHttpCacheValidateMeasure(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
)
{
	xrt_http_cache_validator Validator;
	xtime iLastModified = 0;
	bool bLastModified = false;
	size_t i;

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_VALIDATE_NONE;
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		pPlan->EligibleCount++;
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( Validator.ETagValid &&
			!__xrtHttpCacheETagEarlier(
				pEntries, i, Range,
				&Validator.ETag
			) &&
			!__xrtHttpCacheValidateTagAdd(
				pPlan, &Validator.ETag
			) ) {
			return false;
		}
		if ( Validator.LastModifiedValid ) {
			iLastModified =
				Validator.LastModified;
			bLastModified = true;
		}
	}
	if ( pPlan->ETagCount != 0 ) {
		pPlan->Actions |=
			XHTTP_CACHE_VALIDATE_IF_NONE_MATCH;
	}
	if ( !Range &&
		(pPlan->EligibleCount == 1) &&
		bLastModified ) {
		pPlan->LastModified = iLastModified;
		pPlan->Actions |=
			XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE;
	}
	if ( pPlan->Actions != 0 ) {
		pPlan->Decision =
			XHTTP_CACHE_VALIDATE_CONDITIONAL;
	}
	return true;
}



/* 按选定候选生成条件验证请求计划。 */
XHTTP_POLICY_API xhttpcachevalidatedecision xrtHttpCacheValidatePlan(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	xhttpcachevalidateplan* pPlan
)
{
	xhttpcachevalidateplan Plan;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iCount
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pPlan, sizeof(*pPlan)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_VALIDATE_ERROR;
	}
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_VALIDATE_ERROR;
	if ( !__xrtHttpCacheValidateMeasure(
		pEntries, iCount, Range, &Plan
	) ) {
		return XHTTP_CACHE_VALIDATE_ERROR;
	}
	*pPlan = Plan;
	return Plan.Decision;
}



/* 写出计划中的去重 If-None-Match 字段值。 */
XHTTP_POLICY_API bool xrtHttpCacheValidateETagsWrite(
	const xhttpcacheentry* pEntries,
	size_t iCount,
	bool Range,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpcachevalidateplan Plan;
	xrt_http_cache_validator Validator;
	bytes pBytes = (bytes)pOutput;
	size_t iWritten = 0;
	size_t iTagSize;
	size_t i;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iCount
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pSize, sizeof(*pSize)
		) ||
		xrtMemRangesOverlap(
			pOutput, iCapacity,
			pSize, sizeof(*pSize)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pSize = 0;
	if ( !__xrtHttpCacheValidateMeasure(
		pEntries, iCount, Range, &Plan
	) ) {
		return false;
	}
	*pSize = Plan.ETagSize;
	if ( pOutput == NULL ) {
		return true;
	}
	if ( iCapacity < Plan.ETagSize ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( Plan.ETagSize == 0 ) {
		return true;
	}
	if ( __xrtHttpCacheEntriesOverlap(
			pEntries, iCount,
			pOutput, Plan.ETagSize
		) ||
		xrtMemRangesOverlap(
			pOutput, Plan.ETagSize,
			pSize, sizeof(*pSize)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}

	/* 容量与重叠已经验证，实际写出不会留下半成品。 */
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheEntryEligible(
			&pEntries[i], Range
		) ) {
			continue;
		}
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Validator
		);
		if ( !Validator.ETagValid ||
			__xrtHttpCacheETagEarlier(
				pEntries, i, Range,
				&Validator.ETag
			) ) {
			continue;
		}
		if ( iWritten != 0 ) {
			pBytes[iWritten] = (uint8)',';
			pBytes[iWritten + 1u] = (uint8)' ';
			iWritten += 2u;
		}
		iTagSize = 0;
		if ( !xrtHttpETagWrite(
			&Validator.ETag,
			pBytes + iWritten,
			Plan.ETagSize - iWritten,
			&iTagSize
		) ) {
			return false;
		}
		iWritten += iTagSize;
	}
	return iWritten == Plan.ETagSize;
}



/* 为单个缓存条目的 Range 请求选择 If-Range。 */
XHTTP_POLICY_API xhttpcacheifrangekind xrtHttpCacheIfRangePlan(
	const xhttpcacheentry* pEntry,
	xhttpcacheifrange* pPlan
)
{
	xrt_http_cache_validator Validator;
	xhttpcacheifrange Plan;

	if ( !xrtHttpCacheEntryValid(pEntry) ||
		(pPlan == NULL) ||
		__xhttpFieldArrayOverlap(
			pEntry->Fields,
			pEntry->FieldCount,
			pPlan,
			sizeof(*pPlan)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_IF_RANGE_ERROR;
	}
	memset(&Plan, 0, sizeof(Plan));
	Plan.Kind = XHTTP_CACHE_IF_RANGE_NONE;
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Kind = XHTTP_CACHE_IF_RANGE_ERROR;
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Validator
	);
	if ( Validator.ETagPresent ) {
		if ( Validator.ETagValid &&
			!Validator.ETag.Weak ) {
			Plan.ETag = Validator.ETag;
			Plan.Kind = XHTTP_CACHE_IF_RANGE_ETAG;
		}
	} else if ( Validator.LastModifiedValid &&
		Validator.LastModifiedStrong ) {
		Plan.Date = Validator.LastModified;
		Plan.Kind = XHTTP_CACHE_IF_RANGE_DATE;
	}
	*pPlan = Plan;
	return Plan.Kind;
}



/* 扫描缓存可评估的 If-None-Match 条件。 */
static bool __xrtHttpCacheIfNoneMatch(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cache_validator* pValidator,
	bool* pPresent,
	bool* pMatch
)
{
	xhttpetagitem Item;
	xhttpnext Next;
	xstrview Value;
	size_t iOffset;
	size_t iItems = 0;
	size_t iFields = 0;
	bool bAny = false;
	size_t i;

	*pPresent = false;
	*pMatch = false;
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("If-None-Match")
		) ) {
			continue;
		}
		*pPresent = true;
		iFields++;
		Value = xrtHttpOwsTrim(pFields[i].Value);
		iOffset = 0;
		do {
			Next = xrtHttpETagNext(
				Value, &iOffset, &Item
			);
			if ( Next == XHTTP_NEXT_ERROR ) {
				return false;
			}
			if ( Next == XHTTP_NEXT_ITEM ) {
				iItems++;
				if ( Item.Kind ==
					XHTTP_ETAG_ANY ) {
					bAny = true;
					*pMatch = true;
				} else if (
					pValidator->ETagValid &&
					xrtHttpETagWeakEqual(
						&Item.Tag,
						&pValidator->ETag
					) ) {
					*pMatch = true;
				}
			}
		} while ( Next == XHTTP_NEXT_ITEM );
	}
	if ( !*pPresent ) {
		return true;
	}
	if ( (iItems == 0) ||
		(bAny &&
		 ((iItems != 1) || (iFields != 1))) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 读取最多一个 If-Modified-Since；重复或非法日期按规范忽略。 */
static bool __xrtHttpCacheIfModifiedSince(
	const xhttpfield* pFields,
	size_t iCount,
	bool* pValid,
	xtime* pDate
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	*pValid = false;
	*pDate = 0;
	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFieldName(
			&pFields[i],
			XRT_STR_LITERAL("If-Modified-Since")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	if ( iMatches == 1 ) {
		Value = xrtHttpOwsTrim(Value);
		*pValid = xrtTimeTryParseHTTPDate(
			Value, pDate
		);
	}
	return true;
}



/* 缓存只评估适用于已保存 GET/HEAD 表示的条件字段。 */
XHTTP_POLICY_API xhttpprecondition xrtHttpCachePreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pRequestFields,
	size_t iRequestCount,
	const xhttpcacheentry* pEntry
)
{
	xrt_http_cache_validator Validator;
	xtime iCondition;
	xtime iStored;
	bool bPresent;
	bool bMatch;
	bool bValid;
	bool bGet;
	bool bHead;

	if ( !__xhttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		!__xrtHttpCacheFieldsValid(
			pRequestFields, iRequestCount
		) ||
		!xrtHttpCacheEntryValid(pEntry) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_PRECONDITION_ERROR;
	}
	bGet = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("GET")
	);
	bHead = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);
	if ( !bGet && !bHead ) {
		return XHTTP_PRECONDITION_PROCEED;
	}
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Validator
	);

	/* If-None-Match 优先，并且星号命中任何已保存表示。 */
	if ( !__xrtHttpCacheIfNoneMatch(
		pRequestFields,
		iRequestCount,
		&Validator,
		&bPresent,
		&bMatch
	) ) {
		return XHTTP_PRECONDITION_ERROR;
	}
	if ( bPresent ) {
		return bMatch ?
			XHTTP_PRECONDITION_NOT_MODIFIED :
			XHTTP_PRECONDITION_PROCEED;
	}

	/* 缺少 Last-Modified 时按 Date、接收时间顺序回退。 */
	(void)__xrtHttpCacheIfModifiedSince(
		pRequestFields,
		iRequestCount,
		&bValid,
		&iCondition
	);
	if ( !bValid ) {
		return XHTTP_PRECONDITION_PROCEED;
	}
	if ( Validator.LastModifiedValid ) {
		iStored = Validator.LastModified;
	} else if ( Validator.DateValid ) {
		iStored = Validator.Date;
	} else {
		iStored = pEntry->ResponseTime;
	}
	return (__xrtHttpCacheDateSecond(iStored) <=
		__xrtHttpCacheDateSecond(iCondition)) ?
		XHTTP_PRECONDITION_NOT_MODIFIED :
		XHTTP_PRECONDITION_PROCEED;
}



/* 分类条件请求的最终响应。 */
XHTTP_POLICY_API xhttpcachevalidateresult xrtHttpCacheValidateResult(
	uint16 iStatus,
	bool Treat5xxAsFailure
)
{
	if ( (iStatus < 200u) || (iStatus > 599u) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_VALIDATE_RESULT_ERROR;
	}
	if ( iStatus == XHTTP_STATUS_NOT_MODIFIED ) {
		return XHTTP_CACHE_VALIDATE_RESULT_NOT_MODIFIED;
	}
	if ( Treat5xxAsFailure && (iStatus >= 500u) ) {
		return XHTTP_CACHE_VALIDATE_RESULT_SERVER_FAILURE;
	}
	return XHTTP_CACHE_VALIDATE_RESULT_FULL;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_update.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)




#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 判断候选是否与 304 中任一强验证器匹配。 */
static bool __xrtHttpCache304StrongMatch(
	const xrt_http_cache_validator* pResponse,
	const xrt_http_cache_validator* pStored
)
{
	if ( pResponse->ETagValid &&
		!pResponse->ETag.Weak &&
		pStored->ETagValid &&
		xrtHttpETagStrongEqual(
			&pResponse->ETag,
			&pStored->ETag
		) ) {
		return true;
	}
	return pResponse->LastModifiedValid &&
		pResponse->LastModifiedStrong &&
		pStored->LastModifiedValid &&
		pStored->LastModifiedStrong &&
		__xrtHttpCacheDateEqual(
			pResponse->LastModified,
			pStored->LastModified
		);
}



/* 判断候选是否与 304 中任一弱验证器匹配。 */
static bool __xrtHttpCache304WeakMatch(
	const xrt_http_cache_validator* pResponse,
	const xrt_http_cache_validator* pStored
)
{
	if ( pResponse->ETagValid &&
		pStored->ETagValid &&
		xrtHttpETagWeakEqual(
			&pResponse->ETag,
			&pStored->ETag
		) ) {
		return true;
	}
	return pResponse->LastModifiedValid &&
		pStored->LastModifiedValid &&
		__xrtHttpCacheDateEqual(
			pResponse->LastModified,
			pStored->LastModified
		);
}



/* 判断验证元数据是否包含任何有效强验证器。 */
static bool __xrtHttpCacheValidatorStrong(
	const xrt_http_cache_validator* pValidator
)
{
	return (pValidator->ETagValid &&
		!pValidator->ETag.Weak) ||
		(pValidator->LastModifiedValid &&
		 pValidator->LastModifiedStrong);
}



/* 判断验证元数据是否包含任何有效验证器。 */
static bool __xrtHttpCacheValidatorAny(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagValid ||
		pValidator->LastModifiedValid;
}



/* 判断验证元数据是否在线路上出现了验证器字段。 */
static bool __xrtHttpCacheValidatorPresent(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagPresent ||
		pValidator->LastModifiedPresent;
}



/* 统计与 304 强验证器匹配的全部候选。 */
static size_t __xrtHttpCache304StrongCount(
	const xrt_http_cache_validator* pResponse,
	const xhttpcacheentry* pEntries,
	size_t iCount
)
{
	xrt_http_cache_validator Stored;
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Stored
		);
		if ( __xrtHttpCache304StrongMatch(
			pResponse, &Stored
		) ) {
			iMatches++;
		}
	}
	return iMatches;
}



/* 选择与弱验证器匹配且 Date 最新的唯一候选。 */
static bool __xrtHttpCache304WeakLatest(
	const xrt_http_cache_validator* pResponse,
	const xhttpcacheentry* pEntries,
	size_t iCount,
	size_t* pIndex
)
{
	xrt_http_cache_validator Stored;
	xtime iNewest = 0;
	xtime iDate;
	size_t iNewestIndex = 0;
	bool bFound = false;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[i].Fields,
			pEntries[i].FieldCount,
			&Stored
		);
		if ( !__xrtHttpCache304WeakMatch(
			pResponse, &Stored
		) ) {
			continue;
		}
		iDate = __xrtHttpCacheEntryDate(
			&pEntries[i], &Stored
		);
		if ( !bFound || (iDate > iNewest) ||
			((iDate == iNewest) &&
			 (i > iNewestIndex)) ) {
			bFound = true;
			iNewest = iDate;
			iNewestIndex = i;
		}
	}
	if ( bFound ) {
		*pIndex = iNewestIndex;
	}
	return bFound;
}



/* 验证 304 下标输出不会覆盖输入或数量输出。 */
static bool __xrtHttpCache304OutputValid(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iSelected,
	size_t* pCount
)
{
	size_t iBytes;

	if ( iSelected > (SIZE_MAX / sizeof(*pIndices)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iBytes = iSelected * sizeof(*pIndices);
	if ( __xhttpFieldArrayOverlap(
		pFields, iFieldCount,
		pIndices, iBytes
	) || __xrtHttpCacheEntriesOverlap(
		pEntries, iEntryCount,
		pIndices, iBytes
	) || xrtMemRangesOverlap(
		pIndices, iBytes,
		pCount, sizeof(*pCount)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 按 304 中的验证器选择应更新的候选下标。 */
XHTTP_POLICY_API xhttpcacheupdatematch xrtHttpCache304Select(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttpcacheentry* pEntries,
	size_t iEntryCount,
	size_t* pIndices,
	size_t iCapacity,
	size_t* pCount
)
{
	xrt_http_cache_validator Response;
	xrt_http_cache_validator Stored;
	xhttpcacheupdatematch Match =
		XHTTP_CACHE_UPDATE_MATCH_NONE;
	size_t iSelected = 0;
	size_t iWeak = 0;
	size_t iWrite = 0;
	size_t iOutputBytes;
	size_t i;

	if ( (pCount == NULL) ||
		((pIndices == NULL) && (iCapacity != 0)) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iFieldCount
		) ||
		!__xrtHttpCacheEntriesValid(
			pEntries, iEntryCount
		) ||
		__xhttpFieldArrayOverlap(
			pFields, iFieldCount,
			pCount, sizeof(*pCount)
		) ||
		__xrtHttpCacheEntriesOverlap(
			pEntries, iEntryCount,
			pCount, sizeof(*pCount)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	if ( iCapacity > (SIZE_MAX / sizeof(*pIndices)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	iOutputBytes = iCapacity * sizeof(*pIndices);
	if ( xrtMemRangesOverlap(
		pIndices, iOutputBytes,
		pCount, sizeof(*pCount)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	*pCount = 0;
	(void)__xrtHttpCacheValidatorRead(
		pFields, iFieldCount, &Response
	);

	/* 强验证器优先并选择全部匹配候选。 */
	if ( __xrtHttpCacheValidatorStrong(&Response) ) {
		iSelected = __xrtHttpCache304StrongCount(
			&Response, pEntries, iEntryCount
		);
		if ( iSelected != 0 ) {
			Match = XHTTP_CACHE_UPDATE_MATCH_STRONG;
		}
	} else if ( __xrtHttpCacheValidatorAny(&Response) ) {
		if ( __xrtHttpCache304WeakLatest(
			&Response,
			pEntries,
			iEntryCount,
			&iWeak
		) ) {
			iSelected = 1;
			Match = XHTTP_CACHE_UPDATE_MATCH_WEAK;
		}
	} else if (
		!__xrtHttpCacheValidatorPresent(&Response) &&
		(iEntryCount == 1) ) {
		(void)__xrtHttpCacheValidatorRead(
			pEntries[0].Fields,
			pEntries[0].FieldCount,
			&Stored
		);
		if ( !__xrtHttpCacheValidatorPresent(
			&Stored
		) ) {
			iSelected = 1;
			Match = XHTTP_CACHE_UPDATE_MATCH_SINGLE;
		}
	}
	*pCount = iSelected;
	if ( (iSelected == 0) || (pIndices == NULL) ) {
		return Match;
	}
	if ( iCapacity < iSelected ) {
		xrtSetErrorKind(XERR_RANGE);
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}
	if ( !__xrtHttpCache304OutputValid(
		pFields, iFieldCount,
		pEntries, iEntryCount,
		pIndices, iSelected, pCount
	) ) {
		return XHTTP_CACHE_UPDATE_MATCH_ERROR;
	}

	/* 容量已经确认后再写下标，保持短缓冲原子性。 */
	if ( Match == XHTTP_CACHE_UPDATE_MATCH_STRONG ) {
		for ( i = 0; i < iEntryCount; i++ ) {
			(void)__xrtHttpCacheValidatorRead(
				pEntries[i].Fields,
				pEntries[i].FieldCount,
				&Stored
			);
			if ( __xrtHttpCache304StrongMatch(
				&Response, &Stored
			) ) {
				pIndices[iWrite++] = i;
			}
		}
	} else if ( Match == XHTTP_CACHE_UPDATE_MATCH_WEAK ) {
		pIndices[0] = iWeak;
		iWrite = 1;
	} else if ( Match == XHTTP_CACHE_UPDATE_MATCH_SINGLE ) {
		pIndices[0] = 0;
		iWrite = 1;
	}
	return (iWrite == iSelected) ?
		Match :
		XHTTP_CACHE_UPDATE_MATCH_ERROR;
}



/* 判断 HEAD 中出现的 ETag 是否与已保存响应精确相同。 */
static bool __xrtHttpCacheHeadETagMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->ETagPresent ) {
		return true;
	}
	return pHead->ETagValid &&
		pStored->ETagValid &&
		__xrtHttpCacheETagExact(
			&pHead->ETag,
			&pStored->ETag
		);
}



/* 判断 HEAD 中出现的 Last-Modified 是否与已保存响应相同。 */
static bool __xrtHttpCacheHeadDateMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->LastModifiedPresent ) {
		return true;
	}
	return pHead->LastModifiedValid &&
		pStored->LastModifiedValid &&
		__xrtHttpCacheDateEqual(
			pHead->LastModified,
			pStored->LastModified
		);
}



/* 判断 HEAD 中出现的 Content-Length 是否与已保存响应相同。 */
static bool __xrtHttpCacheHeadLengthMatch(
	const xrt_http_cache_validator* pHead,
	const xrt_http_cache_validator* pStored
)
{
	if ( !pHead->ContentLengthPresent ) {
		return true;
	}
	return pHead->ContentLengthValid &&
		pStored->ContentLengthPresent &&
		pStored->ContentLengthValid &&
		(pHead->ContentLength ==
		 pStored->ContentLength);
}



/* 判断 200 HEAD 是否能更新一个已保存 GET 响应。 */
XHTTP_POLICY_API xhttpcacheheaddecision xrtHttpCacheHeadPlan(
	uint16 iStatus,
	const xhttpcacheentry* pEntry,
	const xhttpfield* pFields,
	size_t iFieldCount
)
{
	xrt_http_cache_validator Head;
	xrt_http_cache_validator Stored;

	if ( (iStatus < 200u) || (iStatus > 599u) ||
		!xrtHttpCacheEntryValid(pEntry) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iFieldCount
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_HEAD_ERROR;
	}
	if ( iStatus != XHTTP_STATUS_OK ) {
		return XHTTP_CACHE_HEAD_IGNORE;
	}
	(void)__xrtHttpCacheValidatorRead(
		pEntry->Fields,
		pEntry->FieldCount,
		&Stored
	);
	(void)__xrtHttpCacheValidatorRead(
		pFields, iFieldCount, &Head
	);
	if ( !__xrtHttpCacheHeadETagMatch(
		&Head, &Stored
	) || !__xrtHttpCacheHeadDateMatch(
		&Head, &Stored
	) || !__xrtHttpCacheHeadLengthMatch(
		&Head, &Stored
	) ) {
		return XHTTP_CACHE_HEAD_STALE;
	}
	return XHTTP_CACHE_HEAD_UPDATE;
}



/* 从 quoted-string 正文读取一个已经解码 quoted-pair 的字节。 */
static bool __xrtHttpCacheQuotedByte(
	xstrview Value,
	size_t* pOffset,
	unsigned char* pByte
)
{
	unsigned char iByte;

	if ( *pOffset >= Value.Size ) {
		return false;
	}
	iByte = (unsigned char)Value.Data[*pOffset];
	(*pOffset)++;
	if ( iByte == (unsigned char)'\\' ) {
		if ( *pOffset >= Value.Size ) {
			return false;
		}
		iByte = (unsigned char)Value.Data[*pOffset];
		(*pOffset)++;
	}
	*pByte = iByte;
	return true;
}



/* 判断解码后的字节是否为 OWS。 */
static bool __xrtHttpCacheQualifiedOws(unsigned char iByte)
{
	return (iByte == (unsigned char)' ') ||
		(iByte == (unsigned char)'\t');
}



/*
	在限定字段名列表中查找目标名称。
	函数同时完整验证解码后的 #field-name，避免命中前缀后忽略坏尾部。
*/
static bool __xrtHttpCacheQualifiedHas(
	const xhttpcacheitem* pItem,
	xstrview Name,
	bool* pValid
)
{
	size_t iOffset = 0;
	size_t iName;
	size_t iItems = 0;
	unsigned char iByte;
	bool bHaveByte = false;
	bool bMatch = false;
	bool bItemMatch;

	*pValid = true;
	if ( (pItem->Flags &
		 XHTTP_PARAM_HAS_VALUE) == 0 ) {
		return false;
	}
	if ( (pItem->Flags &
		 XHTTP_PARAM_QUOTED) == 0 ) {
		if ( !xrtHttpTokenValid(pItem->Value) ) {
			*pValid = false;
			return false;
		}
		return xrtHttpFieldNameEqual(
			pItem->Value, Name
		);
	}

	/* quoted-string 先按 quoted-pair 解码，再按逗号拆分 field-name。 */
	while ( iOffset < pItem->Value.Size ) {
		bHaveByte = false;
		while ( iOffset < pItem->Value.Size ) {
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
			if ( !__xrtHttpCacheQualifiedOws(iByte) &&
				(iByte != (unsigned char)',') ) {
				bHaveByte = true;
				break;
			}
		}
		if ( !bHaveByte ) {
			break;
		}
		iName = 0;
		bItemMatch = true;
		do {
			if ( !__xhttpTokenByte(iByte) ) {
				*pValid = false;
				return false;
			}
			if ( (iName >= Name.Size) ||
				(__xhttpAsciiLower(iByte) !=
				 __xhttpAsciiLower(
					(unsigned char)Name.Data[iName]
				 )) ) {
				bItemMatch = false;
			}
			iName++;
			if ( iOffset >= pItem->Value.Size ) {
				bHaveByte = false;
				break;
			}
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
		} while ( !__xrtHttpCacheQualifiedOws(iByte) &&
			(iByte != (unsigned char)',') );
		if ( bItemMatch && (iName == Name.Size) ) {
			bMatch = true;
		}
		iItems++;

		/* token 后只允许 OWS、逗号或输入结束。 */
		while ( bHaveByte &&
			__xrtHttpCacheQualifiedOws(iByte) ) {
			if ( iOffset >= pItem->Value.Size ) {
				bHaveByte = false;
				break;
			}
			if ( !__xrtHttpCacheQuotedByte(
				pItem->Value,
				&iOffset,
				&iByte
			) ) {
				*pValid = false;
				return false;
			}
		}
		if ( bHaveByte &&
			(iByte != (unsigned char)',') ) {
			*pValid = false;
			return false;
		}
	}
	if ( iItems == 0 ) {
		*pValid = false;
		return false;
	}
	return bMatch;
}



/* 判断字段是否被 Cache-Control 的限定 no-cache/private 排除。 */
static xhttpcachefieldupdate __xrtHttpCacheQualifiedUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	bool Shared
)
{
	xhttpcachecursor Cursor;
	xhttpcacheitem Item;
	xhttpnext Next;
	bool bValid;
	bool bFound;

	xrtHttpCacheCursorInit(&Cursor);
	while ( (Next = xrtHttpCacheNext(
		pFields, iCount, &Cursor, &Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Directive != XHTTP_CACHE_NO_CACHE) &&
			(!Shared ||
			 (Item.Directive != XHTTP_CACHE_PRIVATE)) ) {
			continue;
		}
		bFound = __xrtHttpCacheQualifiedHas(
			&Item, Name, &bValid
		);
		if ( !bValid ) {
			xrtSetErrorKind(XERR_VALUE);
			return XHTTP_CACHE_FIELD_UPDATE_ERROR;
		}
		if ( bFound ) {
			return XHTTP_CACHE_FIELD_UPDATE_SKIP;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	return XHTTP_CACHE_FIELD_UPDATE_REPLACE;
}



/* 判断字段是否被 Connection 字段提名为逐跳字段。 */
static xhttpcachefieldupdate __xrtHttpCacheConnectionUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
)
{
	xhttpnext Next = xrtHttpConnectionFind(
		pFields, iCount, Name
	);

	if ( Next == XHTTP_NEXT_ERROR ) {
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	return (Next == XHTTP_NEXT_ITEM) ?
		XHTTP_CACHE_FIELD_UPDATE_SKIP :
		XHTTP_CACHE_FIELD_UPDATE_REPLACE;
}



/* 判断字段是否属于代理连接身份专用元数据。 */
static bool __xrtHttpCacheProxyField(xstrview Name)
{
	return xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Proxy-Authenticate")
	) || xrtHttpFieldNameEqual(
		Name,
		XRT_STR_LITERAL("Proxy-Authentication-Info")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Proxy-Authorization")
	);
}



/* 判断新响应中的一个字段是否可替换已保存字段。 */
XHTTP_POLICY_API xhttpcachefieldupdate xrtHttpCacheFieldUpdate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iFlags
)
{
	xstrview Name;
	xhttpcachefieldupdate Result;

	if ( !__xrtHttpCacheFieldsValid(
		pFields, iCount
	) || (iIndex >= iCount) ||
		((iFlags & ~(
			XHTTP_CACHE_UPDATE_FIELD_DEPENDENT |
			XHTTP_CACHE_UPDATE_FIELD_PROCESSED
		 )) != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_FIELD_UPDATE_ERROR;
	}
	if ( (iFlags & (
		XHTTP_CACHE_UPDATE_FIELD_DEPENDENT |
		XHTTP_CACHE_UPDATE_FIELD_PROCESSED
	 )) != 0 ) {
		return XHTTP_CACHE_FIELD_UPDATE_SKIP;
	}
	Name = pFields[iIndex].Name;
	if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Content-Length")
	) || xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Connection")
	) || __xrtHttpCacheProxyField(Name) ) {
		return XHTTP_CACHE_FIELD_UPDATE_SKIP;
	}
	Result = __xrtHttpCacheConnectionUpdate(
		pFields, iCount, Name
	);
	if ( Result != XHTTP_CACHE_FIELD_UPDATE_REPLACE ) {
		return Result;
	}
	return __xrtHttpCacheQualifiedUpdate(
		pFields, iCount, Name, Shared
	);
}



/* 按存储计划动作过滤初次保存的响应字段。 */
XHTTP_POLICY_API xhttpcachefieldstore xrtHttpCacheFieldStore(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	bool Shared,
	uint32 iActions
)
{
	xstrview Name;
	xhttpcachefieldupdate Result;
	uint32 iKnown =
		XHTTP_CACHE_STORE_REMOVE_CONNECTION |
		XHTTP_CACHE_STORE_REMOVE_PROXY |
		XHTTP_CACHE_STORE_SEPARATE_TRAILERS |
		XHTTP_CACHE_STORE_REMOVE_NO_CACHE |
		XHTTP_CACHE_STORE_REMOVE_PRIVATE |
		XHTTP_CACHE_STORE_MARK_INCOMPLETE |
		XHTTP_CACHE_STORE_AS_200 |
		XHTTP_CACHE_STORE_IGNORE_NO_STORE;

	if ( !__xrtHttpCacheFieldsValid(
		pFields, iCount
	) || (iIndex >= iCount) ||
		((iActions & ~iKnown) != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_FIELD_STORE_ERROR;
	}
	Name = pFields[iIndex].Name;
	if ( (iActions &
		XHTTP_CACHE_STORE_REMOVE_CONNECTION) != 0 ) {
		if ( xrtHttpHopFieldKnown(Name) ||
			xrtHttpFieldNameEqual(
				Name, XRT_STR_LITERAL("Trailer")
			) ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
		Result = __xrtHttpCacheConnectionUpdate(
			pFields, iCount, Name
		);
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return XHTTP_CACHE_FIELD_STORE_ERROR;
		}
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
	}
	if ( ((iActions &
		  XHTTP_CACHE_STORE_REMOVE_PROXY) != 0) &&
		__xrtHttpCacheProxyField(Name) ) {
		return XHTTP_CACHE_FIELD_STORE_SKIP;
	}
	if ( (iActions & (
			XHTTP_CACHE_STORE_REMOVE_NO_CACHE |
			XHTTP_CACHE_STORE_REMOVE_PRIVATE
		 )) != 0 ) {
		Result = __xrtHttpCacheQualifiedUpdate(
			pFields, iCount, Name, Shared
		);
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return XHTTP_CACHE_FIELD_STORE_ERROR;
		}
		if ( Result == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			return XHTTP_CACHE_FIELD_STORE_SKIP;
		}
	}
	return XHTTP_CACHE_FIELD_STORE_KEEP;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_invalidate.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_VALIDATE)

/* 验证 HTTP 缓存目标是可确定 origin 的绝对 HTTP(S) URI。 */
static bool __xrtHttpCacheTargetUrl(
	xstrview Target,
	xurl* pUrl
)
{
	if ( !xrtHttpBridgeUrlParseValue(Target, pUrl) ) {
		return false;
	}
	if ( ((pUrl->Flags & (
			XURL_HAS_SCHEME |
			XURL_HAS_AUTHORITY |
			XURL_HAS_HOST
		 )) != (
			XURL_HAS_SCHEME |
			XURL_HAS_AUTHORITY |
			XURL_HAS_HOST
		 )) ||
		((pUrl->Flags & (
			XURL_HAS_USERINFO |
			XURL_HAS_FRAGMENT
		 )) != 0) ||
		(pUrl->Host.Size == 0) ) {
		return false;
	}
	return xrtUrlSchemeIs(
		pUrl, XRT_STR_LITERAL("http")
	) || xrtUrlSchemeIs(
		pUrl, XRT_STR_LITERAL("https")
	);
}



/* 从完整 URL 投影出不含资源组件的 Origin 三元组。 */
static bool __xrtHttpCacheOriginFromUrl(
	const xurl* pUrl,
	xhttporigin* pOrigin
)
{
	memset(pOrigin, 0, sizeof(*pOrigin));
	pOrigin->Url = *pUrl;
	pOrigin->Url.Flags &= ~(
		XURL_HAS_QUERY | XURL_HAS_FRAGMENT
	);
	pOrigin->Url.Path = (xstrview){ NULL, 0 };
	pOrigin->Url.Query = (xstrview){ NULL, 0 };
	pOrigin->Url.Fragment = (xstrview){ NULL, 0 };
	return xrtHttpBridgeOriginValueValid(pOrigin);
}



/* 判断 URI-reference 解析并解析后仍与目标 URI 同源。 */
static bool __xrtHttpCacheReferenceSameOrigin(
	const xurl* pTarget,
	xstrview Reference,
	xurl* pReference
)
{
	xhttporigin TargetOrigin;
	xhttporigin ReferenceOrigin;

	if ( !xrtHttpBridgeUrlParseValue(
		Reference, pReference
	) ) {
		return false;
	}
	if ( (pReference->Flags &
		 XURL_HAS_SCHEME) != 0 ) {
		if ( (pReference->Flags &
			 XURL_HAS_AUTHORITY) == 0 ) {
			return false;
		}
	}
	if ( (pReference->Flags &
		 XURL_HAS_AUTHORITY) == 0 ) {
		return (pReference->Flags &
			XURL_HAS_SCHEME) == 0;
	}
	if ( ((pReference->Flags & (
			XURL_HAS_HOST |
			XURL_HAS_USERINFO
		 )) != XURL_HAS_HOST) ||
		(pReference->Host.Size == 0) ) {
		return false;
	}
	if ( (pReference->Flags & XURL_HAS_SCHEME) == 0 ) {
		pReference->Scheme = pTarget->Scheme;
		pReference->Flags |= XURL_HAS_SCHEME;
	}
	return __xrtHttpCacheOriginFromUrl(
		pTarget, &TargetOrigin
	) && __xrtHttpCacheOriginFromUrl(
		pReference, &ReferenceOrigin
	) &&
		xrtHttpBridgeOriginTupleSame(
			&TargetOrigin, &ReferenceOrigin
		);
}



/* 判断当前位置字段属于可选的同源失效候选。 */
static bool __xrtHttpCacheLocationItem(
	const xurl* pTarget,
	const xhttpfield* pFields,
	size_t iCount,
	size_t iIndex,
	xhttpcacheinvalidateitem* pItem
)
{
	xstrview Name;
	xstrview Reference;
	xurl Parsed;
	xhttpcacheinvalidatekind Kind;

	Name = pFields[iIndex].Name;
	if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Location")
	) ) {
		if ( xrtHttpFieldCount(
			pFields, iCount,
			XRT_STR_LITERAL("Location")
		) != 1 ) {
			return false;
		}
		Kind = XHTTP_CACHE_INVALIDATE_LOCATION;
	} else if ( xrtHttpFieldNameEqual(
		Name, XRT_STR_LITERAL("Content-Location")
	) ) {
		if ( xrtHttpFieldCount(
			pFields, iCount,
			XRT_STR_LITERAL("Content-Location")
		) != 1 ) {
			return false;
		}
		Kind = XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION;
	} else {
		return false;
	}
	Reference = xrtHttpOwsTrim(
		pFields[iIndex].Value
	);
	if ( !__xrtHttpCacheReferenceSameOrigin(
		pTarget, Reference, &Parsed
	) ) {
		return false;
	}
	pItem->Reference = Reference;
	pItem->Field = iIndex;
	pItem->Kind = Kind;
	return true;
}



/* 判断失效迭代器输入和输出边界不会相互覆盖。 */
static bool __xrtHttpCacheInvalidationInputValid(
	xstrview Method,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpcacheinvalidatecursor* pCursor,
	const xhttpcacheinvalidateitem* pItem
)
{
	if ( !__xhttpViewValid(Method) ||
		!xrtHttpTokenValid(Method) ||
		!__xhttpViewValid(Target) ||
		!__xrtHttpCacheFieldsValid(
			pFields, iCount
		) ||
		(pCursor == NULL) ||
		(pItem == NULL) ||
		(pCursor->Field > iCount) ||
		xrtMemRangesOverlap(
			pCursor, sizeof(*pCursor),
			pItem, sizeof(*pItem)
		) ||
		xrtMemRangesOverlap(
			pCursor, sizeof(*pCursor),
			Method.Data, Method.Size
		) ||
		xrtMemRangesOverlap(
			pCursor, sizeof(*pCursor),
			Target.Data, Target.Size
		) ||
		xrtMemRangesOverlap(
			pItem, sizeof(*pItem),
			Method.Data, Method.Size
		) ||
		xrtMemRangesOverlap(
			pItem, sizeof(*pItem),
			Target.Data, Target.Size
		) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount,
			pCursor, sizeof(*pCursor)
		) ||
		__xhttpFieldArrayOverlap(
			pFields, iCount,
			pItem, sizeof(*pItem)
		) ) {
		return false;
	}
	return true;
}



/* 初始化可重复使用的缓存失效候选游标。 */
XHTTP_POLICY_API void xrtHttpCacheInvalidationCursorInit(
	xhttpcacheinvalidatecursor* pCursor
)
{
	if ( pCursor == NULL ) {
		return;
	}
	pCursor->Field = 0;
	pCursor->Target = true;
}



/* 迭代 unsafe 方法成功响应产生的同源失效候选。 */
XHTTP_POLICY_API xhttpnext xrtHttpCacheInvalidationNext(
	xstrview Method,
	uint16 iStatus,
	xstrview Target,
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcacheinvalidatecursor* pCursor,
	xhttpcacheinvalidateitem* pItem
)
{
	xurl TargetUrl;
	size_t i;

	if ( !__xrtHttpCacheInvalidationInputValid(
		Method,
		Target,
		pFields,
		iCount,
		pCursor,
		pItem
	) || (iStatus < 100u) || (iStatus > 599u) ||
		!__xrtHttpCacheTargetUrl(
			Target, &TargetUrl
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	memset(pItem, 0, sizeof(*pItem));
	pItem->Field = XRT_NPOS;
	if ( xrtHttpMethodSafe(Method) ||
		(iStatus < 200u) || (iStatus >= 400u) ) {
		pCursor->Target = false;
		pCursor->Field = iCount;
		return XHTTP_NEXT_END;
	}
	if ( pCursor->Target ) {
		pCursor->Target = false;
		pItem->Reference = Target;
		pItem->Field = XRT_NPOS;
		pItem->Kind = XHTTP_CACHE_INVALIDATE_TARGET;
		return XHTTP_NEXT_ITEM;
	}
	for ( i = pCursor->Field; i < iCount; i++ ) {
		pCursor->Field = i + 1u;
		if ( __xrtHttpCacheLocationItem(
			&TargetUrl,
			pFields,
			iCount,
			i,
			pItem
		) ) {
			return XHTTP_NEXT_ITEM;
		}
	}
	return XHTTP_NEXT_END;
}



/* 截去 URI-reference 中不会参与 HTTP 缓存键的 fragment。 */
static xstrview __xrtHttpCacheFragmentRemove(
	xstrview Reference
)
{
	size_t i;

	for ( i = 0; i < Reference.Size; i++ ) {
		if ( Reference.Data[i] == '#' ) {
			Reference.Size = i;
			break;
		}
	}
	return Reference;
}



/* 判断两个字符串视图按字节精确相同。 */
static bool __xrtHttpCacheReferenceEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(
			Left.Data, Right.Data, Left.Size
		 ) == 0));
}



/* 验证手工构造的失效候选仍满足来源和同源约束。 */
static bool __xrtHttpCacheInvalidationItemValid(
	const xurl* pTarget,
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem
)
{
	xurl Reference;

	if ( (pItem == NULL) ||
		!__xhttpViewValid(pItem->Reference) ) {
		return false;
	}
	if ( pItem->Kind == XHTTP_CACHE_INVALIDATE_TARGET ) {
		return (pItem->Field == XRT_NPOS) &&
			__xrtHttpCacheReferenceEqual(
				pItem->Reference, Target
			);
	}
	if ( ((pItem->Kind !=
		  XHTTP_CACHE_INVALIDATE_LOCATION) &&
		 (pItem->Kind !=
		  XHTTP_CACHE_INVALIDATE_CONTENT_LOCATION)) ||
		(pItem->Field == XRT_NPOS) ) {
		return false;
	}
	return __xrtHttpCacheReferenceSameOrigin(
		pTarget, pItem->Reference, &Reference
	);
}



/* 把失效候选解析为不含 fragment 的绝对 URI。 */
XHTTP_POLICY_API bool xrtHttpCacheInvalidationWrite(
	xstrview Target,
	const xhttpcacheinvalidateitem* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xurl TargetUrl;
	xstrview Reference;

	if ( (pSize == NULL) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		!__xhttpViewValid(Target) ||
		!__xrtHttpCacheTargetUrl(
			Target, &TargetUrl
		) ||
		!__xrtHttpCacheInvalidationItemValid(
			&TargetUrl, Target, pItem
		) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pItem, sizeof(*pItem)
		) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			Target.Data, Target.Size
		) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pItem->Reference.Data,
			pItem->Reference.Size
		) ||
		xrtMemRangesOverlap(
			pOutput, iCapacity,
			pSize, sizeof(*pSize)
		) ||
		xrtMemRangesOverlap(
			pOutput, iCapacity,
			pItem, sizeof(*pItem)
		) ||
		xrtMemRangesOverlap(
			pOutput, iCapacity,
			Target.Data, Target.Size
		) ||
		xrtMemRangesOverlap(
			pOutput, iCapacity,
			pItem->Reference.Data,
			pItem->Reference.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pItem->Kind == XHTTP_CACHE_INVALIDATE_TARGET ) {
		return xrtUrlWrite(
			&TargetUrl,
			pOutput,
			iCapacity,
			pSize
		);
	}
	Reference = __xrtHttpCacheFragmentRemove(
		pItem->Reference
	);
	return xrtUrlResolve(
		&TargetUrl,
		Reference,
		pOutput,
		iCapacity,
		pSize
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_fragment.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 初始化尚未收到任何协议数据的片段输入。 */
XHTTP_POLICY_API void xrtHttpCacheFragmentInputInit(
	xhttpcachefragmentinput* pInput
)
{
	if ( pInput != NULL ) {
		memset(pInput, 0, sizeof(*pInput));
	}
}



/* 判断闭区间是否合法。 */
bool __xrtHttpCacheRangeValid(
	const xhttpbyterange* pRange
)
{
	return (pRange != NULL) &&
		(pRange->First <= pRange->Last);
}



/* 判断两个闭区间是否重叠或相邻。 */
bool __xrtHttpCacheRangeJoins(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
)
{
	if ( pRight->First <= pLeft->Last ) {
		return true;
	}
	return (pLeft->Last != UINT64_MAX) &&
		(pRight->First == (pLeft->Last + UINT64_C(1)));
}



/* 判断片段是否覆盖完整表示。 */
static bool __xrtHttpCacheFragmentIsComplete(
	const xhttpcachefragment* pFragment
)
{
	if ( (pFragment->Flags &
		 XHTTP_CACHE_FRAGMENT_HAS_LENGTH) == 0 ) {
		return false;
	}
	if ( pFragment->Length == 0 ) {
		return (pFragment->Flags &
			XHTTP_CACHE_FRAGMENT_HAS_RANGE) == 0;
	}
	return ((pFragment->Flags &
			 XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0) &&
		(pFragment->Range.First == 0) &&
		(pFragment->Range.Last ==
		 (pFragment->Length - UINT64_C(1)));
}



/* 判断公开片段的 Header、区间和完整性标志是否自洽。 */
XHTTP_POLICY_API bool xrtHttpCacheFragmentValid(
	const xhttpcachefragment* pFragment
)
{
	bool bComplete;
	bool bHasRange;

	if ( (pFragment == NULL) ||
		((pFragment->SourceStatus != XHTTP_STATUS_OK) &&
		 (pFragment->SourceStatus !=
		  XHTTP_STATUS_PARTIAL_CONTENT)) ||
		((pFragment->Flags & ~(
			XHTTP_CACHE_FRAGMENT_HAS_RANGE |
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH
		 )) != 0) ||
		((pFragment->Entry.Flags &
		  XHTTP_CACHE_ENTRY_RANGE_COVERED) != 0) ||
		!xrtHttpCacheEntryValid(&pFragment->Entry) ) {
		return false;
	}
	bHasRange = (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0;
	if ( bHasRange ) {
		if ( !__xrtHttpCacheRangeValid(
			&pFragment->Range
		) ) {
			return false;
		}
		if ( (pFragment->SourceStatus ==
			 XHTTP_STATUS_OK) &&
			(pFragment->Range.First != 0) ) {
			return false;
		}
		if ( ((pFragment->Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0) &&
			(pFragment->Range.Last >=
			 pFragment->Length) ) {
			return false;
		}
	} else if ( ((pFragment->Flags &
				  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) == 0) ||
		(pFragment->Length != 0) ||
		(pFragment->SourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		return false;
	}

	bComplete = __xrtHttpCacheFragmentIsComplete(
		pFragment
	);
	return bComplete ==
		((pFragment->Entry.Flags &
		  XHTTP_CACHE_ENTRY_PARTIAL) == 0);
}



/* 判断片段是否已经覆盖完整表示。 */
XHTTP_POLICY_API bool xrtHttpCacheFragmentComplete(
	const xhttpcachefragment* pFragment
)
{
	return xrtHttpCacheFragmentValid(pFragment) &&
		__xrtHttpCacheFragmentIsComplete(pFragment);
}



/* 判断输入和输出计划是否发生任何内存重叠。 */
static bool __xrtHttpCacheFragmentPlanOverlap(
	const xhttpcachefragmentinput* pInput,
	const xhttpcachefragmentplan* pPlan
)
{
	if ( xrtMemRangesOverlap(
		pInput, sizeof(*pInput),
		pPlan, sizeof(*pPlan)
	) || xrtMemRangesOverlap(
		pInput->Method.Data,
		pInput->Method.Size,
		pPlan,
		sizeof(*pPlan)
	) || __xhttpFieldArrayOverlap(
		pInput->Fields,
		pInput->FieldCount,
		pPlan,
		sizeof(*pPlan)
	) || __xhttpFieldArrayOverlap(
		pInput->RangeFields,
		pInput->RangeFieldCount,
		pPlan,
		sizeof(*pPlan)
	) ) {
		return true;
	}
	return false;
}



/* 验证片段输入结构及其借用视图。 */
static bool __xrtHttpCacheFragmentInputValid(
	const xhttpcachefragmentinput* pInput
)
{
	if ( (pInput == NULL) ||
		((pInput->Flags & ~(
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE |
			XHTTP_CACHE_FRAGMENT_MULTIPART_PART |
			XHTTP_CACHE_FRAGMENT_TRANSFORMED
		 )) != 0) ||
		!__xhttpViewValid(pInput->Method) ||
		!xrtHttpTokenValid(pInput->Method) ||
		!__xrtHttpCacheFieldsValid(
			pInput->Fields,
			pInput->FieldCount
		) ||
		!__xrtHttpCacheFieldsValid(
			pInput->RangeFields,
			pInput->RangeFieldCount
		) ) {
		return false;
	}
	return true;
}



/* 记录协议性跳过并保持其余计划字段为零。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragmentSkip(
	xhttpcachefragmentplan* pPlan,
	uint32 iReasons
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_SKIP;
	pPlan->Reasons = iReasons;
	return pPlan->Decision;
}



/* 从指定字段组读取唯一、满足形式的 bytes Content-Range。 */
static bool __xrtHttpCacheContentRangeRead(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentrange* pRange
)
{
	xstrview Value = { NULL, 0 };
	size_t iMatches = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Range")
		) ) {
			Value = pFields[i].Value;
			iMatches++;
		}
	}
	return (iMatches == 1) &&
		xrtHttpBridgeContentRangeParseValue(
			xrtHttpOwsTrim(Value), pRange
		) &&
		pRange->Satisfied;
}



/* 判断已接收字节数与声明的完整或截断消息长度是否一致。 */
static bool __xrtHttpCacheBodyLengthValid(
	uint64 iReceived,
	uint64 iDeclared,
	bool bComplete
)
{
	return bComplete ?
		(iReceived == iDeclared) :
		(iReceived < iDeclared);
}



/* 计算从声明范围起点开始实际收到的闭区间。 */
static bool __xrtHttpCacheReceivedRange(
	uint64 iFirst,
	uint64 iSize,
	xhttpbyterange* pRange
)
{
	uint64 iTail;

	if ( iSize == 0 ) {
		return false;
	}
	iTail = iSize - UINT64_C(1);
	if ( iTail > (UINT64_MAX - iFirst) ) {
		return false;
	}
	pRange->First = iFirst;
	pRange->Last = iFirst + iTail;
	return true;
}



/* 规划完整或截断 200 响应片段。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragment200(
	const xhttpcachefragmentinput* pInput,
	const xrt_http_cache_validator* pValidator,
	xhttpcachefragmentplan* pPlan
)
{
	xhttpcachefragment Fragment;
	bool bComplete = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE) != 0;

	if ( (pInput->RangeFields != NULL) ||
		(pInput->RangeFieldCount != 0) ||
		((pInput->Flags &
		  XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!pValidator->ContentLengthValid ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!__xrtHttpCacheBodyLengthValid(
			pInput->BodySize,
			pValidator->ContentLength,
			bComplete
		) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bComplete && (pInput->BodySize == 0) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_EMPTY
		);
	}

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Fields = pInput->Fields;
	Fragment.Entry.FieldCount = pInput->FieldCount;
	Fragment.Entry.ResponseTime = pInput->ResponseTime;
	Fragment.SourceStatus = XHTTP_STATUS_OK;
	if ( pInput->BodySize != 0 ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_RANGE;
		Fragment.Range.First = 0;
		Fragment.Range.Last =
			pInput->BodySize - UINT64_C(1);
	}
	if ( pValidator->ContentLengthPresent ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = pValidator->ContentLength;
	} else if ( bComplete ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = pInput->BodySize;
	}
	if ( !bComplete ) {
		Fragment.Entry.Flags =
			XHTTP_CACHE_ENTRY_PARTIAL;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Fragment = Fragment;
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_STORE;
	if ( !bComplete ) {
		pPlan->Actions =
			XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}



/* 判断一个 Content-Range 声明区间的可表示字节数。 */
static bool __xrtHttpCacheDeclaredRangeSize(
	const xhttpcontentrange* pRange,
	uint64* pSize
)
{
	uint64 iDifference =
		pRange->Last - pRange->First;

	if ( iDifference == UINT64_MAX ) {
		return false;
	}
	*pSize = iDifference + UINT64_C(1);
	return true;
}



/* 规划单段或 multipart part 的 206 响应片段。 */
static xhttpcachefragmentdecision __xrtHttpCacheFragment206(
	const xhttpcachefragmentinput* pInput,
	const xrt_http_cache_validator* pValidator,
	xhttpcachefragmentplan* pPlan
)
{
	const xhttpfield* pRangeFields = pInput->RangeFields;
	size_t iRangeCount = pInput->RangeFieldCount;
	xhttpcachefragment Fragment;
	xhttpcontentrange ContentRange;
	uint64 iDeclared = 0;
	bool bDeclared;
	bool bComplete = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_BODY_COMPLETE) != 0;
	bool bMultipart = (pInput->Flags &
		XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0;

	if ( !bMultipart &&
		(pRangeFields == NULL) &&
		(iRangeCount == 0) ) {
		pRangeFields = pInput->Fields;
		iRangeCount = pInput->FieldCount;
	}
	if ( bMultipart &&
		((pRangeFields == NULL) ||
		 (iRangeCount == 0)) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( !__xrtHttpCacheContentRangeRead(
		pRangeFields,
		iRangeCount,
		&ContentRange
	) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_RANGE
		);
	}
	if ( pValidator->ContentLengthPresent &&
		!pValidator->ContentLengthValid ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_CONTENT_LENGTH
		);
	}

	bDeclared = __xrtHttpCacheDeclaredRangeSize(
		&ContentRange, &iDeclared
	);
	if ( bComplete &&
		(!bDeclared ||
		 (pInput->BodySize != iDeclared)) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bComplete &&
		((pInput->BodySize == 0) ||
		 (bDeclared &&
		  (pInput->BodySize >= iDeclared))) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			(pInput->BodySize == 0) ?
				XHTTP_CACHE_FRAGMENT_REASON_EMPTY :
				XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( !bMultipart &&
		pValidator->ContentLengthPresent &&
		!__xrtHttpCacheBodyLengthValid(
			pInput->BodySize,
			pValidator->ContentLength,
			bComplete
		) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}

	memset(&Fragment, 0, sizeof(Fragment));
	Fragment.Entry.Fields = pInput->Fields;
	Fragment.Entry.FieldCount = pInput->FieldCount;
	Fragment.Entry.ResponseTime = pInput->ResponseTime;
	Fragment.SourceStatus =
		XHTTP_STATUS_PARTIAL_CONTENT;
	Fragment.Flags = XHTTP_CACHE_FRAGMENT_HAS_RANGE;
	if ( !__xrtHttpCacheReceivedRange(
		ContentRange.First,
		pInput->BodySize,
		&Fragment.Range
	) || (Fragment.Range.Last >
		ContentRange.Last) ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan,
			XHTTP_CACHE_FRAGMENT_REASON_BODY_LENGTH
		);
	}
	if ( ContentRange.HasLength ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
		Fragment.Length = ContentRange.Length;
	}
	if ( !__xrtHttpCacheFragmentIsComplete(
		&Fragment
	) ) {
		Fragment.Entry.Flags =
			XHTTP_CACHE_ENTRY_PARTIAL;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Fragment = Fragment;
	pPlan->Decision = XHTTP_CACHE_FRAGMENT_STORE;
	pPlan->Actions =
		XHTTP_CACHE_FRAGMENT_AS_200 |
		XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE |
		XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH;
	if ( __xrtHttpCacheFragmentIsComplete(
		&Fragment
	) ) {
		pPlan->Actions |=
			XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH;
	} else {
		pPlan->Actions |=
			XHTTP_CACHE_FRAGMENT_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}



/* 把收到的 200 或 206 数据规范化为缓存片段。 */
XHTTP_POLICY_API xhttpcachefragmentdecision xrtHttpCacheFragmentPlan(
	const xhttpcachefragmentinput* pInput,
	xhttpcachefragmentplan* pPlan
)
{
	xrt_http_cache_validator Validator;
	uint32 iReasons = 0;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheFragmentInputValid(pInput) ||
		__xrtHttpCacheFragmentPlanOverlap(
			pInput, pPlan
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_FRAGMENT_ERROR;
	}
	if ( !xrtHttpMethodEqual(
		pInput->Method,
		XRT_STR_LITERAL("GET")
	) ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_METHOD;
	}
	if ( (pInput->Status != XHTTP_STATUS_OK) &&
		(pInput->Status !=
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_STATUS;
	}
	if ( (pInput->Flags &
		 XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE) == 0 ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_HEADERS;
	}
	if ( (pInput->Flags &
		 XHTTP_CACHE_FRAGMENT_TRANSFORMED) != 0 ) {
		iReasons |=
			XHTTP_CACHE_FRAGMENT_REASON_TRANSFORMED;
	}
	if ( iReasons != 0 ) {
		return __xrtHttpCacheFragmentSkip(
			pPlan, iReasons
		);
	}
	if ( !__xrtHttpCacheValidatorRead(
		pInput->Fields,
		pInput->FieldCount,
		&Validator
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_FRAGMENT_ERROR;
	}
	if ( pInput->Status == XHTTP_STATUS_OK ) {
		return __xrtHttpCacheFragment200(
			pInput, &Validator, pPlan
		);
	}
	return __xrtHttpCacheFragment206(
		pInput, &Validator, pPlan
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_coverage.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 判断规范覆盖集的排序、间隔和完整长度约束是否成立。 */
XHTTP_POLICY_API bool xrtHttpCacheCoverageValid(
	const xhttpcachecoverage* pCoverage
)
{
	size_t i;

	if ( (pCoverage == NULL) ||
		((pCoverage->Ranges == NULL) &&
		 (pCoverage->RangeCount != 0)) ||
		(pCoverage->RangeCount >
		 (SIZE_MAX / sizeof(*pCoverage->Ranges))) ) {
		return false;
	}
	if ( pCoverage->HasLength &&
		(pCoverage->Length == 0) &&
		(pCoverage->RangeCount != 0) ) {
		return false;
	}
	for ( i = 0; i < pCoverage->RangeCount; i++ ) {
		if ( !__xrtHttpCacheRangeValid(
			&pCoverage->Ranges[i]
		) ) {
			return false;
		}
		if ( pCoverage->HasLength &&
			(pCoverage->Ranges[i].Last >=
			 pCoverage->Length) ) {
			return false;
		}
		if ( (i != 0) &&
			__xrtHttpCacheRangeJoins(
				&pCoverage->Ranges[i - 1u],
				&pCoverage->Ranges[i]
			) ) {
			return false;
		}
	}
	return true;
}



/* 判断两个可写输出是否覆盖覆盖集、目标或彼此。 */
static bool __xrtHttpCacheCoverageOutputOverlap(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	const xhttpcachemissingcursor* pCursor,
	const xhttpbyterange* pMissing
)
{
	return xrtMemRangesOverlap(
		pCursor, sizeof(*pCursor),
		pCoverage, sizeof(*pCoverage)
	) || xrtMemRangesOverlap(
		pCursor, sizeof(*pCursor),
		pCoverage->Ranges,
		pCoverage->RangeCount *
			sizeof(*pCoverage->Ranges)
	) || xrtMemRangesOverlap(
		pCursor, sizeof(*pCursor),
		pTarget, sizeof(*pTarget)
	) || xrtMemRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCoverage, sizeof(*pCoverage)
	) || xrtMemRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCoverage->Ranges,
		pCoverage->RangeCount *
			sizeof(*pCoverage->Ranges)
	) || xrtMemRangesOverlap(
		pMissing, sizeof(*pMissing),
		pTarget, sizeof(*pTarget)
	) || xrtMemRangesOverlap(
		pMissing, sizeof(*pMissing),
		pCursor, sizeof(*pCursor)
	);
}



/* 判断目标闭区间是否与已知完整长度一致。 */
static bool __xrtHttpCacheCoverageTargetValid(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget
)
{
	return __xrtHttpCacheRangeValid(pTarget) &&
		(!pCoverage->HasLength ||
		 (pTarget->Last < pCoverage->Length));
}



/* 判断目标区间是否完全位于规范覆盖集中。 */
XHTTP_POLICY_API xhttpcachecoverageresult xrtHttpCacheCoverageCovers(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pRange
)
{
	size_t i;

	if ( !xrtHttpCacheCoverageValid(pCoverage) ||
		!__xrtHttpCacheCoverageTargetValid(
			pCoverage, pRange
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_COVERAGE_ERROR;
	}
	for ( i = 0; i < pCoverage->RangeCount; i++ ) {
		if ( pCoverage->Ranges[i].Last <
			pRange->First ) {
			continue;
		}
		if ( pCoverage->Ranges[i].First >
			pRange->First ) {
			return XHTTP_CACHE_COVERAGE_MISS;
		}
		return (pCoverage->Ranges[i].Last >=
			pRange->Last) ?
			XHTTP_CACHE_COVERAGE_HIT :
			XHTTP_CACHE_COVERAGE_MISS;
	}
	return XHTTP_CACHE_COVERAGE_MISS;
}



/* 初始化一个尚未开始迭代的缺口游标。 */
XHTTP_POLICY_API void xrtHttpCacheMissingCursorInit(
	xhttpcachemissingcursor* pCursor
)
{
	if ( pCursor != NULL ) {
		memset(pCursor, 0, sizeof(*pCursor));
	}
}



/* 验证调用方可见的缺口游标状态。 */
static bool __xrtHttpCacheMissingCursorValid(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	const xhttpcachemissingcursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Range > pCoverage->RangeCount) ) {
		return false;
	}
	if ( !pCursor->Started ) {
		return (pCursor->Range == 0) &&
			(pCursor->Next == 0) &&
			!pCursor->Finished;
	}
	if ( pCursor->Finished ) {
		return true;
	}
	return (pCursor->Next >= pTarget->First) &&
		(pCursor->Next <= pTarget->Last);
}



/* 在目标区间内迭代尚未保存的连续缺口。 */
XHTTP_POLICY_API xhttpnext xrtHttpCacheMissingNext(
	const xhttpcachecoverage* pCoverage,
	const xhttpbyterange* pTarget,
	xhttpcachemissingcursor* pCursor,
	xhttpbyterange* pMissing
)
{
	xhttpcachemissingcursor Cursor;
	xhttpbyterange Missing;

	if ( (pCursor == NULL) || (pMissing == NULL) ||
		!xrtHttpCacheCoverageValid(pCoverage) ||
		!__xrtHttpCacheCoverageTargetValid(
			pCoverage, pTarget
		) ||
		__xrtHttpCacheCoverageOutputOverlap(
			pCoverage, pTarget, pCursor, pMissing
		) ||
		!__xrtHttpCacheMissingCursorValid(
			pCoverage, pTarget, pCursor
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_NEXT_ERROR;
	}
	Cursor = *pCursor;
	if ( Cursor.Finished ) {
		return XHTTP_NEXT_END;
	}
	if ( !Cursor.Started ) {
		Cursor.Started = true;
		Cursor.Next = pTarget->First;
	}

	while ( Cursor.Range < pCoverage->RangeCount ) {
		const xhttpbyterange* pStored =
			&pCoverage->Ranges[Cursor.Range];

		if ( pStored->Last < Cursor.Next ) {
			Cursor.Range++;
			continue;
		}
		if ( pStored->First > pTarget->Last ) {
			break;
		}
		if ( pStored->First > Cursor.Next ) {
			Missing.First = Cursor.Next;
			Missing.Last = pStored->First -
				UINT64_C(1);
			if ( Missing.Last > pTarget->Last ) {
				Missing.Last = pTarget->Last;
			}
			if ( Missing.Last == UINT64_MAX ) {
				Cursor.Finished = true;
			} else {
				Cursor.Next =
					Missing.Last + UINT64_C(1);
			}
			*pCursor = Cursor;
			*pMissing = Missing;
			return XHTTP_NEXT_ITEM;
		}
		if ( pStored->Last >= pTarget->Last ) {
			Cursor.Finished = true;
			*pCursor = Cursor;
			return XHTTP_NEXT_END;
		}
		Cursor.Next =
			pStored->Last + UINT64_C(1);
		Cursor.Range++;
	}

	Missing.First = Cursor.Next;
	Missing.Last = pTarget->Last;
	Cursor.Finished = true;
	*pCursor = Cursor;
	*pMissing = Missing;
	return XHTTP_NEXT_ITEM;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_combine.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_RANGE)

/* 整组片段可以同时保留强 ETag 和强 Last-Modified 候选。 */
typedef struct xrt_http_cache_strong_set {
	xhttpetag ETag;
	xtime Date;
	bool ETagAny;
	bool ETagCommon;
	bool DateCommon;
} xrt_http_cache_strong_set;



/* 判断验证元数据是否带有强 ETag。 */
static bool __xrtHttpCacheValidatorStrongETag(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->ETagValid &&
		!pValidator->ETag.Weak;
}



/* 判断验证元数据是否带有强 Last-Modified。 */
static bool __xrtHttpCacheValidatorStrongDate(
	const xrt_http_cache_validator* pValidator
)
{
	return pValidator->LastModifiedValid &&
		pValidator->LastModifiedStrong;
}



/*
	计算全部片段共享的强验证器。
	任意两个强 ETag 明确冲突时，即使日期相同也不能组合。
*/
static bool __xrtHttpCacheStrongSetRead(
	const xhttpcachefragment* pStored,
	size_t iCount,
	xrt_http_cache_strong_set* pSet
)
{
	xrt_http_cache_validator Validator;
	xrt_http_cache_strong_set Set;
	bool bETagAll = true;
	bool bDateAll = true;
	bool bETagConflict = false;
	bool bDateAny = false;
	bool bETag;
	bool bDate;
	size_t i;

	memset(&Set, 0, sizeof(Set));
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpCacheValidatorRead(
			pStored[i].Entry.Fields,
			pStored[i].Entry.FieldCount,
			&Validator
		) ) {
			return false;
		}
		bETag = __xrtHttpCacheValidatorStrongETag(
			&Validator
		);
		bDate = __xrtHttpCacheValidatorStrongDate(
			&Validator
		);

		if ( !bETag ) {
			bETagAll = false;
		} else if ( !Set.ETagAny ) {
			Set.ETag = Validator.ETag;
			Set.ETagAny = true;
		} else if ( !xrtHttpETagStrongEqual(
			&Set.ETag, &Validator.ETag
		) ) {
			bETagAll = false;
			bETagConflict = true;
		}

		if ( !bDate ) {
			bDateAll = false;
		} else if ( !bDateAny ) {
			Set.Date = Validator.LastModified;
			bDateAny = true;
		} else if ( !__xrtHttpCacheDateEqual(
			Set.Date, Validator.LastModified
		) ) {
			bDateAll = false;
		}
	}
	Set.ETagCommon =
		bETagAll && Set.ETagAny;
	Set.DateCommon =
		bDateAll && bDateAny;
	*pSet = Set;
	return !bETagConflict &&
		((iCount < 2) ||
		 Set.ETagCommon ||
		 Set.DateCommon);
}



/* 判断新片段与整组现有片段共享一个强验证器。 */
static bool __xrtHttpCacheStrongSetMatch(
	const xrt_http_cache_strong_set* pSet,
	const xhttpcachefragment* pIncoming
)
{
	xrt_http_cache_validator Incoming;
	bool bETag;

	if ( !__xrtHttpCacheValidatorRead(
		pIncoming->Entry.Fields,
		pIncoming->Entry.FieldCount,
		&Incoming
	) ) {
		return false;
	}
	bETag = __xrtHttpCacheValidatorStrongETag(
		&Incoming
	);
	if ( pSet->ETagAny && bETag &&
		!xrtHttpETagStrongEqual(
			&pSet->ETag, &Incoming.ETag
		) ) {
		return false;
	}
	if ( pSet->ETagCommon && bETag ) {
		return true;
	}
	return pSet->DateCommon &&
		__xrtHttpCacheValidatorStrongDate(
			&Incoming
		) &&
		__xrtHttpCacheDateEqual(
			pSet->Date,
			Incoming.LastModified
		);
}



/* 判断片段是否带有正文覆盖区间。 */
static bool __xrtHttpCacheFragmentHasRange(
	const xhttpcachefragment* pFragment
)
{
	return (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0;
}



/* 判断片段是否带有已知完整表示长度。 */
static bool __xrtHttpCacheFragmentHasLength(
	const xhttpcachefragment* pFragment
)
{
	return (pFragment->Flags &
		XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0;
}



/* 判断两个有序片段区间是否仍保留至少一个字节缺口。 */
static bool __xrtHttpCacheRangesSeparate(
	const xhttpbyterange* pLeft,
	const xhttpbyterange* pRight
)
{
	if ( pLeft->Last == UINT64_MAX ) {
		return false;
	}
	return (pLeft->Last + UINT64_C(1)) <
		pRight->First;
}



/* 验证现有片段数组已经表示同一强验证器下的规范覆盖集。 */
static bool __xrtHttpCacheStoredValid(
	const xhttpcachefragment* pStored,
	size_t iCount,
	xrt_http_cache_strong_set* pValidators
)
{
	bool bHaveRange = false;
	bool bHaveLength = false;
	uint64 iLength = 0;
	size_t i;

	if ( ((pStored == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pStored))) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpCacheFragmentValid(
			&pStored[i]
		) ) {
			return false;
		}
		if ( __xrtHttpCacheFragmentHasLength(
			&pStored[i]
		) ) {
			if ( bHaveLength &&
				(iLength != pStored[i].Length) ) {
				return false;
			}
			bHaveLength = true;
			iLength = pStored[i].Length;
		}
		if ( __xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) ) {
			if ( bHaveRange &&
				!__xrtHttpCacheRangesSeparate(
					&pStored[i - 1u].Range,
					&pStored[i].Range
				) ) {
				return false;
			}
			bHaveRange = true;
		} else if ( (iCount != 1) ||
			!xrtHttpCacheFragmentComplete(
				&pStored[i]
			) ) {
			return false;
		}
	}
	return __xrtHttpCacheStrongSetRead(
		pStored, iCount, pValidators
	);
}



/* 判断组合计划输出是否覆盖任一片段结构或借用 Header。 */
static bool __xrtHttpCacheCombineOutputOverlap(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	const xhttpcachecombineplan* pPlan
)
{
	size_t i;

	if ( xrtMemRangesOverlap(
		pPlan, sizeof(*pPlan),
		pStored, iCount * sizeof(*pStored)
	) || xrtMemRangesOverlap(
		pPlan, sizeof(*pPlan),
		pIncoming, sizeof(*pIncoming)
	) ) {
		return true;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( __xhttpFieldArrayOverlap(
			pStored[i].Entry.Fields,
			pStored[i].Entry.FieldCount,
			pPlan,
			sizeof(*pPlan)
		) ) {
			return true;
		}
	}
	return __xhttpFieldArrayOverlap(
		pIncoming->Entry.Fields,
		pIncoming->Entry.FieldCount,
		pPlan,
		sizeof(*pPlan)
	);
}



/* 发布不需要区间计划的协议决定。 */
static xhttpcachecombinedecision __xrtHttpCacheCombineDecision(
	xhttpcachecombineplan* pPlan,
	xhttpcachecombinedecision Decision
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = Decision;
	return Decision;
}



/* 读取现有片段集合已经确认的完整表示长度。 */
static bool __xrtHttpCacheStoredLength(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint64* pLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFragmentHasLength(
			&pStored[i]
		) ) {
			*pLength = pStored[i].Length;
			return true;
		}
	}
	return false;
}



/* 判断现有覆盖区间能否容纳新确认的完整长度。 */
static bool __xrtHttpCacheStoredWithinLength(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint64 iLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) && (pStored[i].Range.Last >=
			iLength) ) {
			return false;
		}
	}
	return true;
}



/* 选择最晚收到的指定来源状态 Header。 */
static size_t __xrtHttpCacheNewestHeader(
	const xhttpcachefragment* pStored,
	size_t iCount,
	uint16 iStatus
)
{
	size_t iSelected = XRT_NPOS;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( (iStatus != 0) &&
			(pStored[i].SourceStatus != iStatus) ) {
			continue;
		}
		if ( (iSelected == XRT_NPOS) ||
			(pStored[i].Entry.ResponseTime >=
			 pStored[iSelected].Entry.ResponseTime) ) {
			iSelected = i;
		}
	}
	return iSelected;
}



/* 根据最新响应状态选择组合后的 Header 来源和更新动作。 */
static void __xrtHttpCacheCombineHeaders(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	size_t iHeader = XRT_NPOS;

	if ( pIncoming->SourceStatus == XHTTP_STATUS_OK ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS;
		return;
	}

	iHeader = __xrtHttpCacheNewestHeader(
		pStored,
		iCount,
		XHTTP_STATUS_OK
	);
	if ( iHeader == XRT_NPOS ) {
		iHeader = __xrtHttpCacheNewestHeader(
			pStored, iCount, 0
		);
	}
	if ( iHeader != XRT_NPOS ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_UPDATE_INCOMING_FIELDS;
	}
	pPlan->HeaderIndex = iHeader;
	if ( iHeader == XRT_NPOS ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS;
	}
	pPlan->Actions |=
		XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE;
	if ( (iHeader == XRT_NPOS) ||
		(pStored[iHeader].SourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH;
	}
}



/* 建立完整片段替换全部现有覆盖的计划。 */
static xhttpcachecombinedecision __xrtHttpCacheReplacePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	(void)pStored;
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = XHTTP_CACHE_COMBINE_REPLACE;
	pPlan->Index = 0;
	pPlan->RemoveCount = iCount;
	pPlan->ResultCount =
		__xrtHttpCacheFragmentHasRange(
			pIncoming
		) ? 1u : 0u;
	pPlan->HasRange =
		__xrtHttpCacheFragmentHasRange(
			pIncoming
		);
	pPlan->HasLength = true;
	pPlan->Length = pIncoming->Length;
	pPlan->Complete = true;
	if ( pPlan->HasRange ) {
		pPlan->Range = pIncoming->Range;
	}
	pPlan->Actions =
		XHTTP_CACHE_COMBINE_USE_INCOMING_FIELDS |
		XHTTP_CACHE_COMBINE_AS_200 |
		XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH;
	if ( pIncoming->SourceStatus ==
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_RANGE |
			XHTTP_CACHE_COMBINE_REMOVE_CONTENT_LENGTH;
	}
	return pPlan->Decision;
}



/* 计算新片段在规范区间数组中需要替换的连续窗口。 */
static void __xrtHttpCacheCombineWindow(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpbyterange* pIncoming,
	size_t* pIndex,
	size_t* pRemoveCount,
	xhttpbyterange* pMerged
)
{
	size_t i = 0;

	*pMerged = *pIncoming;
	while ( (i < iCount) &&
		__xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) &&
		__xrtHttpCacheRangesSeparate(
			&pStored[i].Range, pMerged
		) ) {
		i++;
	}
	*pIndex = i;
	while ( (i < iCount) &&
		__xrtHttpCacheFragmentHasRange(
			&pStored[i]
		) &&
		!__xrtHttpCacheRangesSeparate(
			pMerged, &pStored[i].Range
		) ) {
		if ( pStored[i].Range.First <
			pMerged->First ) {
			pMerged->First =
				pStored[i].Range.First;
		}
		if ( pStored[i].Range.Last >
			pMerged->Last ) {
			pMerged->Last =
				pStored[i].Range.Last;
		}
		i++;
	}
	*pRemoveCount = i - *pIndex;
}



/* 判断区间替换后的规范覆盖是否完整。 */
static bool __xrtHttpCacheCombineComplete(
	const xhttpcachecombineplan* pPlan
)
{
	if ( !pPlan->HasLength ) {
		return false;
	}
	if ( pPlan->Length == 0 ) {
		return pPlan->ResultCount == 0;
	}
	return (pPlan->ResultCount == 1) &&
		pPlan->HasRange &&
		(pPlan->Range.First == 0) &&
		(pPlan->Range.Last ==
		 (pPlan->Length - UINT64_C(1)));
}



/* 规划强验证器一致的部分响应组合。 */
XHTTP_POLICY_API xhttpcachecombinedecision xrtHttpCacheCombinePlan(
	const xhttpcachefragment* pStored,
	size_t iCount,
	const xhttpcachefragment* pIncoming,
	xhttpcachecombineplan* pPlan
)
{
	xrt_http_cache_strong_set StoredValidators;
	bool bStoredLength;
	bool bIncomingLength;
	uint64 iStoredLength = 0;

	if ( (pPlan == NULL) ||
		!__xrtHttpCacheStoredValid(
			pStored, iCount, &StoredValidators
		) ||
		!xrtHttpCacheFragmentValid(pIncoming) ||
		xrtMemRangesOverlap(
			pStored,
			iCount * sizeof(*pStored),
			pIncoming,
			sizeof(*pIncoming)
		) ||
		__xrtHttpCacheCombineOutputOverlap(
			pStored, iCount, pIncoming, pPlan
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_COMBINE_ERROR;
	}
	if ( xrtHttpCacheFragmentComplete(pIncoming) ) {
		return __xrtHttpCacheReplacePlan(
			pStored, iCount, pIncoming, pPlan
		);
	}

	bStoredLength = __xrtHttpCacheStoredLength(
		pStored, iCount, &iStoredLength
	);
	bIncomingLength =
		__xrtHttpCacheFragmentHasLength(pIncoming);
	if ( bStoredLength && bIncomingLength &&
		(iStoredLength != pIncoming->Length) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( bStoredLength &&
		(pIncoming->Range.Last >= iStoredLength) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( !bStoredLength && bIncomingLength &&
		!__xrtHttpCacheStoredWithinLength(
			pStored, iCount, pIncoming->Length
		) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan, XHTTP_CACHE_COMBINE_CONFLICT
		);
	}
	if ( (iCount != 0) &&
		!__xrtHttpCacheStrongSetMatch(
			&StoredValidators, pIncoming
		) ) {
		return __xrtHttpCacheCombineDecision(
			pPlan,
			XHTTP_CACHE_COMBINE_SEPARATE
		);
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->HeaderIndex = XRT_NPOS;
	pPlan->Decision = XHTTP_CACHE_COMBINE_APPLY;
	pPlan->HasRange = true;
	pPlan->HasLength =
		bStoredLength || bIncomingLength;
	pPlan->Length = bStoredLength ?
		iStoredLength : pIncoming->Length;
	__xrtHttpCacheCombineWindow(
		pStored,
		iCount,
		&pIncoming->Range,
		&pPlan->Index,
		&pPlan->RemoveCount,
		&pPlan->Range
	);
	pPlan->ResultCount =
		iCount - pPlan->RemoveCount + 1u;
	pPlan->Actions = XHTTP_CACHE_COMBINE_AS_200;
	__xrtHttpCacheCombineHeaders(
		pStored, iCount, pIncoming, pPlan
	);
	pPlan->Complete =
		__xrtHttpCacheCombineComplete(pPlan);
	if ( pPlan->Complete ) {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_SET_CONTENT_LENGTH;
	} else {
		pPlan->Actions |=
			XHTTP_CACHE_COMBINE_MARK_INCOMPLETE;
	}
	return pPlan->Decision;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_record.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* 安全累加紧凑布局大小。 */
static bool __xrtHttpCacheSizeAdd(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( (pResult == NULL) || (iLeft > (SIZE_MAX - iRight)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 安全累加带零结尾的文本大小。 */
static bool __xrtHttpCacheTextSizeAdd(
	size_t* pTotal,
	size_t iTextSize
)
{
	size_t iStoredSize;

	if ( !__xrtHttpCacheSizeAdd(
		iTextSize, 1u, &iStoredSize
	) ) {
		return false;
	}
	return __xrtHttpCacheSizeAdd(
		*pTotal, iStoredSize, pTotal
	);
}



/* 安全计算数组字节数。 */
static bool __xrtHttpCacheSizeMultiply(
	size_t iCount,
	size_t iSize,
	size_t* pResult
)
{
	if ( (pResult == NULL) ||
		((iSize != 0) && (iCount > (SIZE_MAX / iSize))) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pResult = iCount * iSize;
	return true;
}



/* 在总布局中加入一个满足 ABI 对齐的数组。 */
static bool __xrtHttpCacheLayoutArray(
	size_t* pTotal,
	size_t iCount,
	size_t iSize,
	size_t iAlignment,
	size_t* pOffset
)
{
	size_t iPadding;
	size_t iBytes;

	if ( (pTotal == NULL) || (pOffset == NULL) ||
		(iAlignment == 0) ||
		((iAlignment & (iAlignment - 1u)) != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pOffset = 0;
	if ( iCount == 0 ) {
		return true;
	}
	iPadding = (iAlignment - (*pTotal & (iAlignment - 1u))) &
		(iAlignment - 1u);
	if ( !__xrtHttpCacheSizeAdd(
		*pTotal, iPadding, pTotal
	) || !__xrtHttpCacheSizeMultiply(
		iCount, iSize, &iBytes
	) ) {
		return false;
	}
	*pOffset = *pTotal;
	return __xrtHttpCacheSizeAdd(*pTotal, iBytes, pTotal);
}



/* 判断两个借用视图是否逐字节相同。 */
static bool __xrtHttpCacheViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证目标 URI 可以作为不含控制字符或空格的稳定键。 */
static bool __xrtHttpCacheURIValid(xstrview URI)
{
	size_t i;

	if ( !__xhttpViewValid(URI) || (URI.Size == 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( i = 0; i < URI.Size; i++ ) {
		uint8 iByte = (uint8)URI.Data[i];

		if ( (iByte <= 0x20u) || (iByte == 0x7fu) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
	}
	return true;
}



/* 验证主键视图、方法、URI 和字段数组，并复制到对齐快照。 */
bool __xrtHttpCacheKeyResolve(
	const xhttpcachekey* pInput,
	xhttpcachekey* pKey
)
{
	xhttpcachekey Key;

	if ( !xrtMemRangeValid(pInput, sizeof(Key)) ||
		(pKey == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Key, pInput, sizeof(Key));
	if ( !__xhttpViewValid(Key.Method) ||
		!__xhttpViewValid(Key.Partition) ||
		!__xhttpFieldArrayValid(
			Key.Fields, Key.FieldCount
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpTokenValid(Key.Method) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( !__xrtHttpCacheURIValid(Key.URI) ) {
		return false;
	}
	*pKey = Key;
	return true;
}



/* 判断一段内存是否覆盖已验证主键或任一借用内容。 */
bool __xrtHttpCacheKeyOverlap(
	const xhttpcachekey* pKey,
	const void* pMemory,
	size_t iSize
)
{
	return xrtMemRangesOverlap(
		pKey->Method.Data, pKey->Method.Size,
		pMemory, iSize
	) || xrtMemRangesOverlap(
		pKey->URI.Data, pKey->URI.Size,
		pMemory, iSize
	) || xrtMemRangesOverlap(
		pKey->Partition.Data, pKey->Partition.Size,
		pMemory, iSize
	) || __xhttpFieldArrayOverlap(
		pKey->Fields, pKey->FieldCount,
		pMemory, iSize
	);
}



/* 验证 Record 的响应元数据、时钟和字段数组。 */
static bool __xrtHttpCacheRecordHeadValid(
	const xhttpcacherecordinput* pInput
)
{
	xhttpcachekey Key;
	uint32 iKnownFlags = XHTTP_CACHE_RECORD_HAS_LENGTH |
		XHTTP_CACHE_RECORD_COMPLETE;

	if ( pInput == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&pInput->Key, &Key) ||
		!__xhttpViewValid(pInput->Reason) ||
		!__xhttpFieldArrayValid(
			pInput->Fields, pInput->FieldCount
		) ||
		!__xhttpFieldArrayValid(
			pInput->Trailers, pInput->TrailerCount
		) ) {
		return false;
	}
	if ( ((pInput->Parts == NULL) && (pInput->PartCount != 0)) ||
		(pInput->PartCount >
		 (SIZE_MAX / sizeof(*pInput->Parts))) ||
		!xrtMemRangeValid(
			pInput->Parts,
			pInput->PartCount * sizeof(*pInput->Parts)
		) ||
		((pInput->Version != XHTTP_VERSION_1_0) &&
		 (pInput->Version != XHTTP_VERSION_1_1)) ||
		(pInput->Status < 200) || (pInput->Status > 999) ||
		((pInput->Flags & ~iKnownFlags) != 0) ||
		(((pInput->Flags & XHTTP_CACHE_RECORD_HAS_LENGTH) == 0) &&
		 (pInput->Length != 0)) ||
		(((pInput->Flags & XHTTP_CACHE_RECORD_COMPLETE) != 0) &&
		 ((pInput->Flags & XHTTP_CACHE_RECORD_HAS_LENGTH) == 0)) ||
		(pInput->RequestClock > pInput->ResponseClock) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return xrtHttpFieldValueValid(pInput->Reason);
}



/* 验证片段排序、覆盖范围和完整标志，并累计正文大小。 */
static bool __xrtHttpCachePartsValid(
	const xhttpcacherecordinput* pInput,
	size_t* pBodySize,
	uint64* pBodyBytes
)
{
	xhttpcachepart Part;
	uint64 iPreviousEnd = 0;
	uint64 iBodyBytes = 0;
	size_t iBodySize = 0;
	size_t i;

	if ( (pBodySize == NULL) || (pBodyBytes == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( i = 0; i < pInput->PartCount; i++ ) {
		uint64 iSize;
		uint64 iEnd;

		memcpy(
			&Part,
			((const uint8*)pInput->Parts) +
				(i * sizeof(Part)),
			sizeof(Part)
		);
		if ( !xrtMemRangeValid(Part.Data.Data, Part.Data.Size) ||
			(Part.Data.Size == 0) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		iSize = (uint64)Part.Data.Size;
		if ( (uint64)(size_t)iSize != iSize ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		if ( Part.Offset > (UINT64_MAX - iSize) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iEnd = Part.Offset + iSize;
		if ( (i != 0) && (Part.Offset < iPreviousEnd) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( ((pInput->Flags &
				XHTTP_CACHE_RECORD_HAS_LENGTH) != 0) &&
			(iEnd > pInput->Length) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		if ( iBodyBytes > (UINT64_MAX - iSize) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iBodyBytes += iSize;
		if ( !__xrtHttpCacheSizeAdd(
			iBodySize, Part.Data.Size, &iBodySize
		) ) {
			return false;
		}
		iPreviousEnd = iEnd;
	}
	if ( (pInput->Flags & XHTTP_CACHE_RECORD_COMPLETE) != 0 ) {
		uint64 iExpected = 0;

		for ( i = 0; i < pInput->PartCount; i++ ) {
			memcpy(
				&Part,
				((const uint8*)pInput->Parts) +
					(i * sizeof(Part)),
				sizeof(Part)
			);
			if ( Part.Offset != iExpected ) {
				xrtSetErrorKind(XERR_VALUE);
				return false;
			}
			iExpected += (uint64)Part.Data.Size;
		}
		if ( iExpected != pInput->Length ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
	}
	*pBodySize = iBodySize;
	*pBodyBytes = iBodyBytes;
	return true;
}



/* 判断当前 Vary 成员是否已经在更早成员中出现。 */
static bool __xrtHttpCacheVaryDuplicate(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iBefore,
	xstrview Name
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	size_t iSeen = 0;

	xrtHttpVaryCursorInit(&Cursor);
	while ( iSeen < iBefore ) {
		xhttpnext Next = xrtHttpVaryNext(
			pFields, iCount, &Cursor, &Item
		);

		if ( Next != XHTTP_NEXT_ITEM ) {
			return false;
		}
		if ( !Item.Wildcard &&
			xrtHttpFieldNameEqual(Item.Name, Name) ) {
			return true;
		}
		iSeen++;
	}
	return false;
}



/* 累计去重 Vary 维度、原请求字段和对应文本大小。 */
static bool __xrtHttpCacheVaryMeasure(
	const xhttpcacherecordinput* pInput,
	size_t* pVaryCount,
	size_t* pVaryFieldCount,
	size_t* pTextSize
)
{
	xhttpvaryplan Plan;
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	size_t iItem = 0;
	size_t iVaryCount = 0;
	size_t iFieldCount = 0;
	size_t iTextSize = 0;

	if ( (pVaryCount == NULL) ||
		(pVaryFieldCount == NULL) ||
		(pTextSize == NULL) ||
		!xrtHttpVaryPlan(
			pInput->Fields, pInput->FieldCount, &Plan
		) ) {
		return false;
	}
	if ( (Plan.Flags & XHTTP_VARY_WILDCARD) != 0 ) {
		xrtSetErrorKind(XERR_UNSUPPORTED);
		return false;
	}
	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pInput->Fields,
		pInput->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		size_t i;

		if ( __xrtHttpCacheVaryDuplicate(
			pInput->Fields,
			pInput->FieldCount,
			iItem,
			Item.Name
		) ) {
			iItem++;
			continue;
		}
		if ( (iVaryCount == SIZE_MAX) ||
			!__xrtHttpCacheTextSizeAdd(
				&iTextSize, Item.Name.Size
			) ) {
			return false;
		}
		iVaryCount++;
		for ( i = 0; i < pInput->Key.FieldCount; i++ ) {
			xhttpfield Field;

			__xhttpFieldLoad(
				pInput->Key.Fields, i, &Field
			);
			if ( !xrtHttpFieldNameEqual(
				Field.Name, Item.Name
			) ) {
				continue;
			}
			if ( (iFieldCount == SIZE_MAX) ||
				!__xrtHttpCacheTextSizeAdd(
					&iTextSize, Field.Value.Size
				) ) {
				return false;
			}
			iFieldCount++;
		}
		iItem++;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		return false;
	}
	*pVaryCount = iVaryCount;
	*pVaryFieldCount = iFieldCount;
	*pTextSize = iTextSize;
	return true;
}



/* 累计字段数组的名称、值和零结尾文本大小。 */
static bool __xrtHttpCacheFieldsMeasure(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pTextSize
)
{
	xhttpfield Field;
	size_t iTextSize = *pTextSize;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		size_t iFieldSize;

		__xhttpFieldLoad(pFields, i, &Field);
		if ( !__xrtHttpCacheSizeAdd(
			Field.Name.Size,
			Field.Value.Size,
			&iFieldSize
		) || !__xrtHttpCacheSizeAdd(
			iFieldSize, 2u, &iFieldSize
		) || !__xrtHttpCacheSizeAdd(
			iTextSize, iFieldSize, &iTextSize
		) ) {
			return false;
		}
	}
	*pTextSize = iTextSize;
	return true;
}



/* 复制一个文本视图并追加只用于诊断和 C 互操作的零字符。 */
static xstrview __xrtHttpCacheTextCopy(
	char** ppWrite,
	xstrview Text
)
{
	char* pWrite = *ppWrite;
	xstrview Copy;

	if ( Text.Size != 0 ) {
		memcpy(pWrite, Text.Data, Text.Size);
	}
	pWrite[Text.Size] = '\0';
	Copy.Data = pWrite;
	Copy.Size = Text.Size;
	*ppWrite = pWrite + Text.Size + 1u;
	return Copy;
}



/* 把字段数组复制到 Record 的连续文本区。 */
static void __xrtHttpCacheFieldsCopy(
	xhttpfield* pOutput,
	const xhttpfield* pInput,
	size_t iCount,
	char** ppWrite
)
{
	xhttpfield Field;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		__xhttpFieldLoad(pInput, i, &Field);
		pOutput[i].Name = __xrtHttpCacheTextCopy(
			ppWrite, Field.Name
		);
		pOutput[i].Value = __xrtHttpCacheTextCopy(
			ppWrite, Field.Value
		);
	}
}



/* 复制去重 Vary 名称及其对应的原请求字段值。 */
static bool __xrtHttpCacheVaryCopy(
	xhttpcacherecord* pRecord,
	const xhttpcacherecordinput* pInput,
	char** ppWrite
)
{
	xhttpvarycursor Cursor;
	xhttpvaryitem Item;
	xhttpnext Next;
	size_t iItem = 0;
	size_t iVary = 0;
	size_t iField = 0;

	xrtHttpVaryCursorInit(&Cursor);
	while ( (Next = xrtHttpVaryNext(
		pInput->Fields,
		pInput->FieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		xrt_http_cache_vary* pVary;
		size_t i;

		if ( __xrtHttpCacheVaryDuplicate(
			pInput->Fields,
			pInput->FieldCount,
			iItem,
			Item.Name
		) ) {
			iItem++;
			continue;
		}
		pVary = &pRecord->Vary[iVary++];
		pVary->Name = __xrtHttpCacheTextCopy(
			ppWrite, Item.Name
		);
		pVary->Field = iField;
		for ( i = 0; i < pInput->Key.FieldCount; i++ ) {
			xhttpfield Source;
			xhttpfield* pTarget;

			__xhttpFieldLoad(
				pInput->Key.Fields, i, &Source
			);
			if ( !xrtHttpFieldNameEqual(
				Source.Name, Item.Name
			) ) {
				continue;
			}
			pTarget = (xhttpfield*)&pRecord->Key.Fields[
				iField++
			];
			pTarget->Name = pVary->Name;
			pTarget->Value = __xrtHttpCacheTextCopy(
				ppWrite, Source.Value
			);
			pVary->FieldCount++;
		}
		iItem++;
	}
	return Next != XHTTP_NEXT_ERROR;
}



/* 复制正文片段并把每个公开视图重新指向 Record 内存。 */
static void __xrtHttpCachePartsCopy(
	xhttpcachepart* pOutput,
	const xhttpcachepart* pInput,
	size_t iCount,
	char** ppWrite
)
{
	xhttpcachepart Part;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		memcpy(
			&Part,
			((const uint8*)pInput) + (i * sizeof(Part)),
			sizeof(Part)
		);
		pOutput[i].Offset = Part.Offset;
		pOutput[i].Data.Data = (cbytes)*ppWrite;
		pOutput[i].Data.Size = Part.Data.Size;
		memcpy(
			*ppWrite,
			Part.Data.Data,
			Part.Data.Size
		);
		*ppWrite += Part.Data.Size;
	}
}



/* 从 Date 或接收时间建立多个匹配项的稳定新旧顺序。 */
static bool __xrtHttpCacheSelectionTime(
	xhttpcacherecord* pRecord
)
{
	xhttpcachetime Time;

	if ( !xrtHttpCacheTimeParse(
		pRecord->Fields,
		pRecord->FieldCount,
		&Time
	) ) {
		return false;
	}
	pRecord->SelectionTime = pRecord->ResponseTime;
	if ( (Time.DateCount == 1) &&
		((Time.Flags & XHTTP_CACHE_TIME_DATE) != 0) &&
		((Time.Flags & (XHTTP_CACHE_TIME_DATE_DUPLICATE |
			XHTTP_CACHE_TIME_DATE_INVALID)) == 0) ) {
		pRecord->SelectionTime = Time.Date;
	}
	return true;
}



/* 初始化适合通用私有客户端缓存的条目、总字节和单条目限额。 */
XHTTP_POLICY_API void xrtHttpCacheConfigInit(xhttpcacheconfig* pConfig)
{
	const xhttpcacheconfig Config = {
		64u,
		XHTTP_CACHE_ENTRIES_DEFAULT,
		(size_t)XHTTP_CACHE_BYTES_DEFAULT,
		(size_t)XHTTP_CACHE_ENTRY_BYTES_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化没有分区和请求字段的主键。 */
XHTTP_POLICY_API bool xrtHttpCacheKeyInit(
	xhttpcachekey* pKey,
	xstrview Method,
	xstrview URI
)
{
	xhttpcachekey Key;

	memset(&Key, 0, sizeof(Key));
	Key.Method = Method;
	Key.URI = URI;
	if ( !xrtMemRangeValid(pKey, sizeof(Key)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&Key, &Key) ) {
		return false;
	}
	if ( __xrtHttpCacheKeyOverlap(&Key, pKey, sizeof(Key)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pKey, &Key, sizeof(Key));
	return true;
}



/* 初始化 HTTP/1.1 响应记录输入并借用完整 Key。 */
XHTTP_POLICY_API bool xrtHttpCacheRecordInputInit(
	xhttpcacherecordinput* pInput,
	const xhttpcachekey* pKey,
	uint16 iStatus
)
{
	xhttpcachekey Key;
	xhttpcacherecordinput Input;

	if ( !xrtMemRangeValid(pInput, sizeof(Input)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pKey, sizeof(Key), pInput, sizeof(Input)
	) || __xrtHttpCacheKeyOverlap(
		&Key, pInput, sizeof(Input)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (iStatus < 200) || (iStatus > 999) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	memset(&Input, 0, sizeof(Input));
	Input.Key = Key;
	Input.Version = XHTTP_VERSION_1_1;
	Input.Status = iStatus;
	memcpy(pInput, &Input, sizeof(Input));
	return true;
}



/* 创建完全拥有输入内容的单分配不可变 Record。 */
XHTTP_POLICY_API xhttpcacherecord* xrtHttpCacheRecordCreate(
	const xhttpcacherecordinput* pInput
)
{
	xhttpcacherecordinput Input;
	xhttpcacherecord* pRecord;
	size_t iFieldOffset;
	size_t iTrailerOffset;
	size_t iPartOffset;
	size_t iVaryOffset;
	size_t iVaryFieldOffset;
	size_t iTextOffset;
	size_t iTextSize = 0;
	size_t iBodySize;
	size_t iVaryCount;
	size_t iVaryFieldCount;
	size_t iTotal = sizeof(*pRecord);
	uint64 iBodyBytes;
	char* pWrite;

	if ( !xrtMemRangeValid(pInput, sizeof(Input)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	memcpy(&Input, pInput, sizeof(Input));
	pInput = &Input;
	if ( !__xrtHttpCacheRecordHeadValid(pInput) ||
		!__xrtHttpCachePartsValid(
			pInput, &iBodySize, &iBodyBytes
		) ||
		!__xrtHttpCacheVaryMeasure(
			pInput,
			&iVaryCount,
			&iVaryFieldCount,
			&iTextSize
		) ||
		!__xrtHttpCacheFieldsMeasure(
			pInput->Fields,
			pInput->FieldCount,
			&iTextSize
		) ||
		!__xrtHttpCacheFieldsMeasure(
			pInput->Trailers,
			pInput->TrailerCount,
			&iTextSize
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.Method.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.URI.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Key.Partition.Size
		) ||
		!__xrtHttpCacheTextSizeAdd(
			&iTextSize, pInput->Reason.Size
		) ||
		!__xrtHttpCacheSizeAdd(
			iTextSize, iBodySize, &iTextSize
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->FieldCount,
			sizeof(xhttpfield),
			XHTTP_ALIGNOF(xhttpfield),
			&iFieldOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->TrailerCount,
			sizeof(xhttpfield),
			XHTTP_ALIGNOF(xhttpfield),
			&iTrailerOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			pInput->PartCount,
			sizeof(xhttpcachepart),
			XHTTP_ALIGNOF(xhttpcachepart),
			&iPartOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			iVaryCount,
			sizeof(xrt_http_cache_vary),
			XHTTP_ALIGNOF(xrt_http_cache_vary),
			&iVaryOffset
		) ||
		!__xrtHttpCacheLayoutArray(
			&iTotal,
			iVaryFieldCount,
			sizeof(xhttpfield),
			XHTTP_ALIGNOF(xhttpfield),
			&iVaryFieldOffset
		) ) {
		return NULL;
	}
	iTextOffset = iTotal;
	if ( !__xrtHttpCacheSizeAdd(
		iTotal, iTextSize, &iTotal
	) ) {
		return NULL;
	}
	pRecord = (xhttpcacherecord*)xrtCalloc(1, iTotal);
	if ( pRecord == NULL ) {
		return NULL;
	}
	pRecord->References = 1;
	pRecord->Charge = iTotal;
	pRecord->Version = pInput->Version;
	pRecord->Status = pInput->Status;
	pRecord->Flags = pInput->Flags;
	pRecord->FieldCount = pInput->FieldCount;
	pRecord->TrailerCount = pInput->TrailerCount;
	pRecord->PartCount = pInput->PartCount;
	pRecord->BodyBytes = iBodyBytes;
	pRecord->Length = pInput->Length;
	pRecord->ResponseTime = pInput->ResponseTime;
	pRecord->RequestClock = pInput->RequestClock;
	pRecord->ResponseClock = pInput->ResponseClock;
	pRecord->VaryCount = iVaryCount;
	pRecord->Fields = iFieldOffset != 0 ?
		(xhttpfield*)((uint8*)pRecord + iFieldOffset) : NULL;
	pRecord->Trailers = iTrailerOffset != 0 ?
		(xhttpfield*)((uint8*)pRecord + iTrailerOffset) : NULL;
	pRecord->Parts = iPartOffset != 0 ?
		(xhttpcachepart*)((uint8*)pRecord + iPartOffset) : NULL;
	pRecord->Vary = iVaryOffset != 0 ?
		(xrt_http_cache_vary*)((uint8*)pRecord + iVaryOffset) :
		NULL;
	pRecord->Key.Fields = iVaryFieldOffset != 0 ?
		(const xhttpfield*)((uint8*)pRecord + iVaryFieldOffset) :
		NULL;
	pRecord->Key.FieldCount = iVaryFieldCount;
	pWrite = (char*)pRecord + iTextOffset;
	pRecord->Key.Method = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.Method
	);
	pRecord->Key.URI = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.URI
	);
	pRecord->Key.Partition = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Key.Partition
	);
	pRecord->Reason = __xrtHttpCacheTextCopy(
		&pWrite, pInput->Reason
	);
	__xrtHttpCacheFieldsCopy(
		pRecord->Fields,
		pInput->Fields,
		pInput->FieldCount,
		&pWrite
	);
	__xrtHttpCacheFieldsCopy(
		pRecord->Trailers,
		pInput->Trailers,
		pInput->TrailerCount,
		&pWrite
	);
	if ( !__xrtHttpCacheVaryCopy(
		pRecord, pInput, &pWrite
	) ) {
		xrtFree(pRecord);
		return NULL;
	}
	__xrtHttpCachePartsCopy(
		pRecord->Parts,
		pInput->Parts,
		pInput->PartCount,
		&pWrite
	);
	if ( !__xrtHttpCacheSelectionTime(pRecord) ) {
		xrtFree(pRecord);
		return NULL;
	}
	return pRecord;
}



/* 增加 Record 引用并返回原指针。 */
XHTTP_POLICY_API xhttpcacherecord* xrtHttpCacheRecordRetain(
	const xhttpcacherecord* pRecord
)
{
	if ( (pRecord == NULL) ||
		(xrtRefRetain(
			&((xhttpcacherecord*)pRecord)->References
		) < 0) ) {
		return NULL;
	}
	return (xhttpcacherecord*)pRecord;
}



/* 释放最后一个 Record 引用及其唯一紧凑分配。 */
XHTTP_POLICY_API void xrtHttpCacheRecordRelease(xhttpcacherecord* pRecord)
{
	if ( (pRecord == NULL) ||
		(xrtRefRelease(&pRecord->References) != 0) ) {
		return;
	}
	xrtFree(pRecord);
}



/* 返回 Record 拥有的主键。 */
XHTTP_POLICY_API const xhttpcachekey* xrtHttpCacheRecordKey(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? &pRecord->Key : NULL;
}



/* 返回响应协议版本。 */
XHTTP_POLICY_API xhttpversion xrtHttpCacheRecordVersion(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Version : 0;
}



/* 返回最终响应状态码。 */
XHTTP_POLICY_API uint16 xrtHttpCacheRecordStatus(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Status : 0;
}



/* 返回 Record 标志。 */
XHTTP_POLICY_API uint32 xrtHttpCacheRecordFlags(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Flags : 0;
}



/* 返回借用的 reason phrase。 */
XHTTP_POLICY_API xstrview xrtHttpCacheRecordReason(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ?
		pRecord->Reason : (xstrview){ NULL, 0 };
}



/* 返回响应 Header 数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordFieldCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->FieldCount : 0;
}



/* 返回指定响应 Header。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordFieldAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->FieldCount) ?
		&pRecord->Fields[iIndex] : NULL;
}



/* 返回首个同名响应 Header。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordField(
	const xhttpcacherecord* pRecord,
	xstrview Name
)
{
	size_t i;

	if ( (pRecord == NULL) || !xrtHttpTokenValid(Name) ) {
		return NULL;
	}
	for ( i = 0; i < pRecord->FieldCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pRecord->Fields[i].Name, Name
		) ) {
			return &pRecord->Fields[i];
		}
	}
	return NULL;
}



/* 返回响应 Trailer 数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordTrailerCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->TrailerCount : 0;
}



/* 返回指定响应 Trailer。 */
XHTTP_POLICY_API const xhttpfield* xrtHttpCacheRecordTrailerAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) &&
		(iIndex < pRecord->TrailerCount) ?
			&pRecord->Trailers[iIndex] : NULL;
}



/* 返回正文片段数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordPartCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->PartCount : 0;
}



/* 返回指定不可变正文片段。 */
XHTTP_POLICY_API const xhttpcachepart* xrtHttpCacheRecordPartAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->PartCount) ?
		&pRecord->Parts[iIndex] : NULL;
}



/* 返回全部片段有效载荷字节数。 */
XHTTP_POLICY_API uint64 xrtHttpCacheRecordBodyBytes(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->BodyBytes : 0;
}



/* 返回完整表示长度。 */
XHTTP_POLICY_API uint64 xrtHttpCacheRecordLength(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Length : 0;
}



/* 返回响应接收墙钟时间。 */
XHTTP_POLICY_API xtime xrtHttpCacheRecordResponseTime(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->ResponseTime : 0;
}



/* 返回发出请求时的单调时钟。 */
XHTTP_POLICY_API double xrtHttpCacheRecordRequestClock(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->RequestClock : 0;
}



/* 返回收到响应时的单调时钟。 */
XHTTP_POLICY_API double xrtHttpCacheRecordResponseClock(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->ResponseClock : 0;
}



/* 返回 Record 单次紧凑分配的实际字节数。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordCharge(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->Charge : 0;
}



/* 返回去重后的 Vary 选择字段数量。 */
XHTTP_POLICY_API size_t xrtHttpCacheRecordVaryCount(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->VaryCount : 0;
}



/* 返回指定 Vary 字段名。 */
XHTTP_POLICY_API xstrview xrtHttpCacheRecordVaryAt(
	const xhttpcacherecord* pRecord,
	size_t iIndex
)
{
	return (pRecord != NULL) && (iIndex < pRecord->VaryCount) ?
		pRecord->Vary[iIndex].Name :
		(xstrview){ NULL, 0 };
}



/* 去掉允许的首尾 OWS 后返回字段值。 */
static xstrview __xrtHttpCacheValueTrim(xstrview Value)
{
	size_t iBegin = 0;
	size_t iEnd = Value.Size;

	while ( (iBegin < iEnd) &&
		((Value.Data[iBegin] == ' ') ||
		 (Value.Data[iBegin] == '\t')) ) {
		iBegin++;
	}
	while ( (iEnd > iBegin) &&
		((Value.Data[iEnd - 1u] == ' ') ||
		 (Value.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return (xstrview){
		Value.Data != NULL ? Value.Data + iBegin : NULL,
		iEnd - iBegin
	};
}



/* 比较一个 Vary 维度在保存请求和当前请求中的字段序列。 */
static bool __xrtHttpCacheVaryMatches(
	const xhttpcacherecord* pRecord,
	const xrt_http_cache_vary* pVary,
	const xhttpcachekey* pKey
)
{
	size_t iStored = 0;
	size_t i;

	for ( i = 0; i < pKey->FieldCount; i++ ) {
		xstrview Stored;
		xstrview Current;

		if ( !xrtHttpFieldNameEqual(
			pKey->Fields[i].Name, pVary->Name
		) ) {
			continue;
		}
		if ( iStored >= pVary->FieldCount ) {
			return false;
		}
		Stored = __xrtHttpCacheValueTrim(
			pRecord->Key.Fields[
				pVary->Field + iStored
			].Value
		);
		Current = __xrtHttpCacheValueTrim(
			pKey->Fields[i].Value
		);
		if ( !__xrtHttpCacheViewEqual(Stored, Current) ) {
			return false;
		}
		iStored++;
	}
	return iStored == pVary->FieldCount;
}



/* 判断已校验主键和全部 Vary 原请求字段是否匹配。 */
bool __xrtHttpCacheRecordMatchesValid(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
)
{
	size_t i;

	if ( !__xrtHttpCacheViewEqual(
		pRecord->Key.Method, pKey->Method
	) || !__xrtHttpCacheViewEqual(
		pRecord->Key.URI, pKey->URI
	) || !__xrtHttpCacheViewEqual(
		pRecord->Key.Partition, pKey->Partition
	) ) {
		return false;
	}
	for ( i = 0; i < pRecord->VaryCount; i++ ) {
		if ( !__xrtHttpCacheVaryMatches(
			pRecord, &pRecord->Vary[i], pKey
		) ) {
			return false;
		}
	}
	return true;
}



/* 判断主键和全部 Vary 原请求字段是否匹配。 */
XHTTP_POLICY_API bool xrtHttpCacheRecordMatches(
	const xhttpcacherecord* pRecord,
	const xhttpcachekey* pKey
)
{
	xhttpcachekey Key;

	if ( pRecord == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		return false;
	}
	return __xrtHttpCacheRecordMatchesValid(pRecord, &Key);
}



/* 查找 Record 中指定名称的 Vary 维度。 */
static const xrt_http_cache_vary* __xrtHttpCacheVaryFind(
	const xhttpcacherecord* pRecord,
	xstrview Name
)
{
	size_t i;

	for ( i = 0; i < pRecord->VaryCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pRecord->Vary[i].Name, Name
		) ) {
			return &pRecord->Vary[i];
		}
	}
	return NULL;
}



/* 比较两个 Record 中一个 Vary 维度的保存字段序列。 */
static bool __xrtHttpCacheVaryEqual(
	const xhttpcacherecord* pLeft,
	const xrt_http_cache_vary* pLeftVary,
	const xhttpcacherecord* pRight,
	const xrt_http_cache_vary* pRightVary
)
{
	size_t i;

	if ( pLeftVary->FieldCount != pRightVary->FieldCount ) {
		return false;
	}
	for ( i = 0; i < pLeftVary->FieldCount; i++ ) {
		xstrview Left = __xrtHttpCacheValueTrim(
			pLeft->Key.Fields[pLeftVary->Field + i].Value
		);
		xstrview Right = __xrtHttpCacheValueTrim(
			pRight->Key.Fields[pRightVary->Field + i].Value
		);

		if ( !__xrtHttpCacheViewEqual(Left, Right) ) {
			return false;
		}
	}
	return true;
}



/* 比较两个 Record 是否描述同一个 Vary 变体。 */
bool __xrtHttpCacheRecordVariantEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
)
{
	size_t i;

	if ( (pLeft == NULL) || (pRight == NULL) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.Method, pRight->Key.Method
		) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.URI, pRight->Key.URI
		) ||
		!__xrtHttpCacheViewEqual(
			pLeft->Key.Partition, pRight->Key.Partition
		) ||
		(pLeft->VaryCount != pRight->VaryCount) ) {
		return false;
	}
	for ( i = 0; i < pLeft->VaryCount; i++ ) {
		const xrt_http_cache_vary* pRightVary =
			__xrtHttpCacheVaryFind(
				pRight, pLeft->Vary[i].Name
			);

		if ( (pRightVary == NULL) ||
			!__xrtHttpCacheVaryEqual(
				pLeft,
				&pLeft->Vary[i],
				pRight,
				pRightVary
			) ) {
			return false;
		}
	}
	return true;
}



/* 返回用于多个匹配响应之间选择最新项的时间。 */
xtime __xrtHttpCacheRecordSelectionTime(
	const xhttpcacherecord* pRecord
)
{
	return pRecord != NULL ? pRecord->SelectionTime : 0;
}



/* 构造验证协议层可直接使用的借用 Entry 视图。 */
XHTTP_POLICY_API bool xrtHttpCacheRecordEntry(
	const xhttpcacherecord* pRecord,
	xhttpcacheentry* pEntry
)
{
	xhttpcacheentry Entry = { 0 };

	if ( (pRecord == NULL) ||
		!xrtMemRangeValid(pEntry, sizeof(Entry)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Entry.Fields = pRecord->Fields;
	Entry.FieldCount = pRecord->FieldCount;
	Entry.ResponseTime = pRecord->ResponseTime;
	if ( (pRecord->Flags & XHTTP_CACHE_RECORD_COMPLETE) == 0 ) {
		Entry.Flags |= XHTTP_CACHE_ENTRY_PARTIAL;
	}
	memcpy(pEntry, &Entry, sizeof(Entry));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_cache_store.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)



#if defined(XHTTP_FEATURE_HTTP_CACHE_STORE)

/* Node 只保存索引、LRU 和 Record 引用，不复制响应内容。 */
typedef struct xrt_http_cache_node {
	xhttpcacherecord* Record;
	struct xrt_http_cache_node* BucketNext;
	struct xrt_http_cache_node* LRUPrevious;
	struct xrt_http_cache_node* LRUNext;
	struct xrt_http_cache_node* RetireNext;
	uint64 Sequence;
} xrt_http_cache_node;



/* Cache 的 Map、LRU、限额和统计全部由同一 Mutex 保护。 */
struct xhttpcache {
	volatile int32 References;
	xhttpcacheops Ops;
	ptr Context;
	xmutex Lock;
	xmap Index;
	xhttpcacheconfig Config;
	xrt_http_cache_node* LRUHead;
	xrt_http_cache_node* LRUTail;
	xhttpcachestats Stats;
	uint64 Sequence;
	bool Custom;
	bool LockReady;
	bool IndexReady;
};



/* 内部状态检查区分逻辑损坏、容量拒绝和可提交。 */
typedef enum xrt_http_cache_fit {
	XRT_HTTP_CACHE_FIT_ERROR = -1,
	XRT_HTTP_CACHE_FIT_REJECTED = 0,
	XRT_HTTP_CACHE_FIT_READY = 1
} xrt_http_cache_fit;



/* 把文本 URI 转换为 Map 使用的只读字节键。 */
static xbytesview __xrtHttpCacheMapKey(xstrview URI)
{
	return (xbytesview){ (cbytes)URI.Data, URI.Size };
}



/* 判断两个普通视图是否逐字节相同。 */
static bool __xrtHttpCacheStoreViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 单调统计达到上限后保持饱和。 */
static void __xrtHttpCacheCounterAdd(
	uint64* pCounter,
	uint64 iValue
)
{
	if ( *pCounter > (UINT64_MAX - iValue) ) {
		*pCounter = UINT64_MAX;
	} else {
		*pCounter += iValue;
	}
}



/*
	验证一次原子状态变更可以由 size_t 精确表示。
	容量超限随后由 LRU 处理；这里仅阻止计数回绕后绕过硬限额。
*/
static xrt_http_cache_fit __xrtHttpCacheStateFits(
	const xhttpcache* pCache,
	size_t iRemoveEntries,
	size_t iRemoveBytes,
	size_t iAddEntries,
	size_t iAddBytes
)
{
	size_t iEntries;
	size_t iBytes;

	if ( (iRemoveEntries > pCache->Stats.Entries) ||
		(iRemoveBytes > pCache->Stats.Bytes) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return XRT_HTTP_CACHE_FIT_ERROR;
	}
	iEntries = pCache->Stats.Entries - iRemoveEntries;
	iBytes = pCache->Stats.Bytes - iRemoveBytes;
	if ( (iAddEntries > (SIZE_MAX - iEntries)) ||
		(iAddBytes > (SIZE_MAX - iBytes)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return XRT_HTTP_CACHE_FIT_REJECTED;
	}
	return XRT_HTTP_CACHE_FIT_READY;
}



/* 为新写入分配一个单调序号，极端溢出后保持饱和。 */
static uint64 __xrtHttpCacheSequence(xhttpcache* pCache)
{
	if ( pCache->Sequence != UINT64_MAX ) {
		pCache->Sequence++;
	}
	return pCache->Sequence;
}



/* 解析并验证 Cache 配置。 */
static bool __xrtHttpCacheConfigResolve(
	const xhttpcacheconfig* pInput,
	xhttpcacheconfig* pConfig
)
{
	xrtHttpCacheConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->MaxEntries == 0) ||
		(pConfig->MaxBytes == 0) ||
		(pConfig->MaxEntryBytes == 0) ||
		(pConfig->InitialEntries > pConfig->MaxEntries) ||
		(pConfig->MaxEntryBytes > pConfig->MaxBytes) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 从 LRU 双向链摘除 Node。 */
static void __xrtHttpCacheLRURemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	if ( pNode->LRUPrevious != NULL ) {
		pNode->LRUPrevious->LRUNext = pNode->LRUNext;
	} else {
		pCache->LRUHead = pNode->LRUNext;
	}
	if ( pNode->LRUNext != NULL ) {
		pNode->LRUNext->LRUPrevious =
			pNode->LRUPrevious;
	} else {
		pCache->LRUTail = pNode->LRUPrevious;
	}
	pNode->LRUPrevious = NULL;
	pNode->LRUNext = NULL;
}



/* 把 Node 放到 LRU 最新端。 */
static void __xrtHttpCacheLRUFirst(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	pNode->LRUPrevious = NULL;
	pNode->LRUNext = pCache->LRUHead;
	if ( pCache->LRUHead != NULL ) {
		pCache->LRUHead->LRUPrevious = pNode;
	} else {
		pCache->LRUTail = pNode;
	}
	pCache->LRUHead = pNode;
}



/* 把已存在 Node 移到 LRU 最新端。 */
static void __xrtHttpCacheLRUTouch(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	if ( pCache->LRUHead == pNode ) {
		return;
	}
	__xrtHttpCacheLRURemove(pCache, pNode);
	__xrtHttpCacheLRUFirst(pCache, pNode);
}



/* 把摘除 Node 链接到锁外回收列表。 */
static void __xrtHttpCacheRetire(
	xrt_http_cache_node** ppRetired,
	xrt_http_cache_node* pNode
)
{
	pNode->RetireNext = *ppRetired;
	*ppRetired = pNode;
}



/* 在锁外释放一组已经从索引和 LRU 摘除的 Node。 */
static void __xrtHttpCacheRetiredRelease(
	xrt_http_cache_node* pRetired
)
{
	while ( pRetired != NULL ) {
		xrt_http_cache_node* pNext = pRetired->RetireNext;

		xrtHttpCacheRecordRelease(pRetired->Record);
		memset(pRetired, 0, sizeof(*pRetired));
		xrtFree(pRetired);
		pRetired = pNext;
	}
}



/* 在锁外按原 LRU 链释放全部 Node。 */
static void __xrtHttpCacheLRURelease(
	xrt_http_cache_node* pNode
)
{
	while ( pNode != NULL ) {
		xrt_http_cache_node* pNext = pNode->LRUNext;

		xrtHttpCacheRecordRelease(pNode->Record);
		memset(pNode, 0, sizeof(*pNode));
		xrtFree(pNode);
		pNode = pNext;
	}
}



/* 返回 URI 对应的可写 Bucket 头槽。 */
static xrt_http_cache_node** __xrtHttpCacheBucket(
	xhttpcache* pCache,
	xstrview URI
)
{
	return (xrt_http_cache_node**)xrtMapGet(
		&pCache->Index,
		__xrtHttpCacheMapKey(URI)
	);
}



/* 从 URI Bucket 摘除指定 Node，并在 Bucket 变空时删除 Map 键。 */
static bool __xrtHttpCacheBucketRemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode
)
{
	const xhttpcachekey* pKey =
		xrtHttpCacheRecordKey(pNode->Record);
	xbytesview MapKey = __xrtHttpCacheMapKey(pKey->URI);
	xrt_http_cache_node** ppHead =
		(xrt_http_cache_node**)xrtMapGet(
			&pCache->Index, MapKey
		);
	xrt_http_cache_node** ppNode;

	if ( ppHead == NULL ) {
		return false;
	}
	ppNode = ppHead;
	while ( (*ppNode != NULL) && (*ppNode != pNode) ) {
		ppNode = &(*ppNode)->BucketNext;
	}
	if ( *ppNode == NULL ) {
		return false;
	}
	*ppNode = pNode->BucketNext;
	pNode->BucketNext = NULL;
	if ( *ppHead == NULL ) {
		(void)xrtMapRemove(&pCache->Index, MapKey);
	}
	return true;
}



/* 从 Cache 计数、索引和 LRU 摘除一个 Node。 */
static bool __xrtHttpCacheNodeRemove(
	xhttpcache* pCache,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node** ppRetired
)
{
	size_t iCharge = xrtHttpCacheRecordCharge(pNode->Record);

	if ( !__xrtHttpCacheBucketRemove(pCache, pNode) ) {
		return false;
	}
	__xrtHttpCacheLRURemove(pCache, pNode);
	pCache->Stats.Entries--;
	pCache->Stats.Bytes -= iCharge;
	__xrtHttpCacheRetire(ppRetired, pNode);
	return true;
}



/* 淘汰最旧 Node 直到条目数和 Record 字节数同时满足硬限额。 */
static void __xrtHttpCacheLimit(
	xhttpcache* pCache,
	xrt_http_cache_node** ppRetired
)
{
	while ( (pCache->Stats.Entries >
			pCache->Config.MaxEntries) ||
		(pCache->Stats.Bytes > pCache->Config.MaxBytes) ) {
		xrt_http_cache_node* pNode = pCache->LRUTail;

		if ( (pNode == NULL) ||
			!__xrtHttpCacheNodeRemove(
				pCache, pNode, ppRetired
			) ) {
			break;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Evictions, 1
		);
	}
}



/* 在 URI Bucket 中查找相同主键和 Vary 变体。 */
static xrt_http_cache_node* __xrtHttpCacheVariant(
	xrt_http_cache_node* pNode,
	const xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppPrevious
)
{
	xrt_http_cache_node* pPrevious = NULL;

	while ( pNode != NULL ) {
		if ( __xrtHttpCacheRecordVariantEqual(
			pNode->Record, pRecord
		) ) {
			if ( ppPrevious != NULL ) {
				*ppPrevious = pPrevious;
			}
			return pNode;
		}
		pPrevious = pNode;
		pNode = pNode->BucketNext;
	}
	if ( ppPrevious != NULL ) {
		*ppPrevious = NULL;
	}
	return NULL;
}



/* 把 Bucket 中的已有 Node 移到最新端，保持相同时间下的确定选择。 */
static void __xrtHttpCacheBucketFirst(
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious
)
{
	if ( pPrevious == NULL ) {
		return;
	}
	pPrevious->BucketNext = pNode->BucketNext;
	pNode->BucketNext = *ppHead;
	*ppHead = pNode;
}



/* 判断候选是否比当前选择更新。 */
static bool __xrtHttpCacheNewer(
	const xrt_http_cache_node* pCandidate,
	const xrt_http_cache_node* pSelected
)
{
	xtime iCandidate;
	xtime iSelected;

	if ( pSelected == NULL ) {
		return true;
	}
	iCandidate = __xrtHttpCacheRecordSelectionTime(
		pCandidate->Record
	);
	iSelected = __xrtHttpCacheRecordSelectionTime(
		pSelected->Record
	);
	if ( iCandidate != iSelected ) {
		return iCandidate > iSelected;
	}
	return pCandidate->Sequence > pSelected->Sequence;
}



/* 创建线程安全的有界内存缓存。 */
XHTTP_POLICY_API xhttpcache* xrtHttpCacheCreate(
	const xhttpcacheconfig* pConfig
)
{
	xhttpcacheconfig Config;
	xhttpcache* pCache;

	if ( !__xrtHttpCacheConfigResolve(pConfig, &Config) ) {
		return NULL;
	}
	pCache = (xhttpcache*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		return NULL;
	}
	pCache->References = 1;
	pCache->Config = Config;
	if ( !xrtMutexInit(&pCache->Lock) ) {
		xrtFree(pCache);
		return NULL;
	}
	pCache->LockReady = true;
	if ( !xrtMapInit(&pCache->Index, sizeof(ptr)) ) {
		(void)xrtMutexUnit(&pCache->Lock);
		xrtFree(pCache);
		return NULL;
	}
	pCache->IndexReady = true;
	if ( (Config.InitialEntries != 0) &&
		!xrtMapReserve(
			&pCache->Index, Config.InitialEntries
		) ) {
		xrtMapUnit(&pCache->Index);
		(void)xrtMutexUnit(&pCache->Lock);
		xrtFree(pCache);
		return NULL;
	}
	return pCache;
}



/* 校验自定义后端必须实现的完整操作集合。 */
static bool __xrtHttpCacheOpsValid(const xhttpcacheops* pOps)
{
	if ( (pOps == NULL) ||
		(pOps->Get == NULL) ||
		(pOps->Put == NULL) ||
		(pOps->Insert == NULL) ||
		(pOps->Replace == NULL) ||
		(pOps->RemoveRecord == NULL) ||
		(pOps->Remove == NULL) ||
		(pOps->RemoveURI == NULL) ||
		(pOps->Clear == NULL) ||
		(pOps->Stats == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 创建委托给自定义线程安全后端的统一 Cache 句柄。 */
XHTTP_POLICY_API xhttpcache* xrtHttpCacheOpen(
	const xhttpcacheops* pOps,
	ptr pContext
)
{
	xhttpcacheops Ops;
	xhttpcache* pCache;

	if ( !xrtMemRangeValid(pOps, sizeof(Ops)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	memcpy(&Ops, pOps, sizeof(Ops));
	if ( !__xrtHttpCacheOpsValid(&Ops) ) {
		return NULL;
	}
	pCache = (xhttpcache*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		return NULL;
	}
	pCache->References = 1;
	pCache->Ops = Ops;
	pCache->Context = pContext;
	pCache->Custom = true;
	return pCache;
}



/* 增加 Cache 引用并返回原指针。 */
XHTTP_POLICY_API xhttpcache* xrtHttpCacheRetain(const xhttpcache* pCache)
{
	if ( (pCache == NULL) ||
		(xrtRefRetain(
			&((xhttpcache*)pCache)->References
		) < 0) ) {
		return NULL;
	}
	return (xhttpcache*)pCache;
}



/* 释放 Cache 最后一个引用及其全部 Node。 */
XHTTP_POLICY_API void xrtHttpCacheRelease(xhttpcache* pCache)
{
	xrt_http_cache_node* pNodes;

	if ( (pCache == NULL) ||
		(xrtRefRelease(&pCache->References) != 0) ) {
		return;
	}
	if ( pCache->Custom ) {
		if ( pCache->Ops.Close != NULL ) {
			pCache->Ops.Close(pCache->Context);
		}
		memset(pCache, 0, sizeof(*pCache));
		xrtFree(pCache);
		return;
	}
	pNodes = pCache->LRUHead;
	pCache->LRUHead = NULL;
	pCache->LRUTail = NULL;
	if ( pCache->IndexReady ) {
		xrtMapUnit(&pCache->Index);
		pCache->IndexReady = false;
	}
	if ( pCache->LockReady ) {
		(void)xrtMutexUnit(&pCache->Lock);
		pCache->LockReady = false;
	}
	__xrtHttpCacheLRURelease(pNodes);
	memset(pCache, 0, sizeof(*pCache));
	xrtFree(pCache);
}



/* 查找最新匹配记录并返回独立引用。 */
XHTTP_POLICY_API xhttpcachelookup xrtHttpCacheGet(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	xhttpcacherecord** ppRecord
)
{
	xhttpcachekey Key;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pSelected = NULL;
	xhttpcacherecord* pRecord = NULL;

	if ( (pCache == NULL) ||
		!xrtMemRangeValid(ppRecord, sizeof(pRecord)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( !xrtMemRangeValid(pKey, sizeof(Key)) ) {
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( xrtMemRangesOverlap(
		pKey, sizeof(Key), ppRecord, sizeof(pRecord)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	if ( __xrtHttpCacheKeyOverlap(
		&Key, ppRecord, sizeof(pRecord)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	memcpy(ppRecord, &pRecord, sizeof(pRecord));
	if ( pCache->Custom ) {
		xhttpcachelookup Result = pCache->Ops.Get(
			pCache->Context,
			&Key,
			&pRecord
		);

		if ( (Result < XHTTP_CACHE_LOOKUP_ERROR) ||
			(Result > XHTTP_CACHE_LOOKUP_HIT) ||
			((Result == XHTTP_CACHE_LOOKUP_HIT) &&
			 (pRecord == NULL)) ||
			((Result == XHTTP_CACHE_LOOKUP_HIT) &&
			 !__xrtHttpCacheRecordMatchesValid(
				pRecord,
				&Key
			 )) ||
			((Result != XHTTP_CACHE_LOOKUP_HIT) &&
			 (pRecord != NULL)) ) {
			xrtHttpCacheRecordRelease(pRecord);
			pRecord = NULL;
			memcpy(ppRecord, &pRecord, sizeof(pRecord));
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_CACHE_LOOKUP_ERROR;
		}
		memcpy(ppRecord, &pRecord, sizeof(pRecord));
		return Result;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	__xrtHttpCacheCounterAdd(&pCache->Stats.Lookups, 1);
	{
		xrt_http_cache_node** ppHead =
			__xrtHttpCacheBucket(pCache, Key.URI);

		pNode = ppHead != NULL ? *ppHead : NULL;
	}
	while ( pNode != NULL ) {
		if ( __xrtHttpCacheRecordMatchesValid(
			pNode->Record, &Key
		) && __xrtHttpCacheNewer(
			pNode, pSelected
		) ) {
			pSelected = pNode;
		}
		pNode = pNode->BucketNext;
	}
	if ( pSelected == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Misses, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_LOOKUP_MISS;
	}
	pRecord = xrtHttpCacheRecordRetain(pSelected->Record);
	if ( pRecord == NULL ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	__xrtHttpCacheLRUTouch(pCache, pSelected);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Hits, 1);
	(void)xrtMutexUnlock(&pCache->Lock);
	memcpy(ppRecord, &pRecord, sizeof(pRecord));
	return XHTTP_CACHE_LOOKUP_HIT;
}



/* 保存首次出现的 Vary 变体。 */
static xhttpcacheput __xrtHttpCacheNodeInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppRetired
)
{
	const xhttpcachekey* pKey =
		xrtHttpCacheRecordKey(pRecord);
	xrt_http_cache_node* pNode;
	xrt_http_cache_node** ppHead;
	xhttpcacherecord* pHeld;
	bool bNewKey = false;

	pNode = (xrt_http_cache_node*)xrtCalloc(
		1, sizeof(*pNode)
	);
	if ( pNode == NULL ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	pHeld = xrtHttpCacheRecordRetain(pRecord);
	if ( pHeld == NULL ) {
		xrtFree(pNode);
		return XHTTP_CACHE_PUT_ERROR;
	}
	ppHead = (xrt_http_cache_node**)xrtMapGetOrAdd(
		&pCache->Index,
		__xrtHttpCacheMapKey(pKey->URI),
		&bNewKey
	);
	if ( ppHead == NULL ) {
		xrtHttpCacheRecordRelease(pHeld);
		xrtFree(pNode);
		return XHTTP_CACHE_PUT_ERROR;
	}
	(void)bNewKey;
	pNode->Record = pHeld;
	pNode->Sequence = __xrtHttpCacheSequence(pCache);
	pNode->BucketNext = *ppHead;
	*ppHead = pNode;
	__xrtHttpCacheLRUFirst(pCache, pNode);
	pCache->Stats.Entries++;
	pCache->Stats.Bytes += xrtHttpCacheRecordCharge(pHeld);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Stores, 1);
	__xrtHttpCacheLimit(pCache, ppRetired);
	return XHTTP_CACHE_PUT_STORED;
}



/* 用已经持有的 Record 无失败地替换一个定位完成的 Node。 */
static void __xrtHttpCacheNodeReplaceHeld(
	xhttpcache* pCache,
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious,
	xhttpcacherecord* pHeld,
	xhttpcacherecord** ppOld
)
{
	size_t iOld;
	size_t iNew;

	iOld = xrtHttpCacheRecordCharge(pNode->Record);
	iNew = xrtHttpCacheRecordCharge(pHeld);
	*ppOld = pNode->Record;
	pNode->Record = pHeld;
	pNode->Sequence = __xrtHttpCacheSequence(pCache);
	pCache->Stats.Bytes = pCache->Stats.Bytes - iOld + iNew;
	__xrtHttpCacheBucketFirst(
		ppHead, pNode, pPrevious
	);
	__xrtHttpCacheLRUTouch(pCache, pNode);
	__xrtHttpCacheCounterAdd(&pCache->Stats.Stores, 1);
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Replacements, 1
	);
}



/* 持有新 Record 后替换一个已经定位的 Node。 */
static xhttpcacheput __xrtHttpCacheNodeReplace(
	xhttpcache* pCache,
	xrt_http_cache_node** ppHead,
	xrt_http_cache_node* pNode,
	xrt_http_cache_node* pPrevious,
	xhttpcacherecord* pRecord,
	xhttpcacherecord** ppOld
)
{
	xhttpcacherecord* pHeld =
		xrtHttpCacheRecordRetain(pRecord);

	if ( pHeld == NULL ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	__xrtHttpCacheNodeReplaceHeld(
		pCache,
		ppHead,
		pNode,
		pPrevious,
		pHeld,
		ppOld
	);
	return XHTTP_CACHE_PUT_REPLACED;
}



/* 保存 Record 的独立引用，并在提交后锁外回收旧内容。 */
XHTTP_POLICY_API xhttpcacheput xrtHttpCachePut(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pPrevious;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacherecord* pOld = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;

	if ( (pCache == NULL) || (pRecord == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		xhttpcacheput Result = pCache->Ops.Put(
			pCache->Context,
			pRecord
		);

		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_STORED) &&
			(Result != XHTTP_CACHE_PUT_REPLACED) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pRecord);
	iCharge = xrtHttpCacheRecordCharge(pRecord);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pRecord,
		&pPrevious
	);
	Fit = __xrtHttpCacheStateFits(
		pCache,
		pNode != NULL ? 1u : 0u,
		pNode != NULL ?
			xrtHttpCacheRecordCharge(pNode->Record) :
			0,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	if ( pNode != NULL ) {
		Result = __xrtHttpCacheNodeReplace(
			pCache,
			ppHead,
			pNode,
			pPrevious,
			pRecord,
			&pOld
		);
		if ( Result == XHTTP_CACHE_PUT_REPLACED ) {
			__xrtHttpCacheLimit(
				pCache,
				&pRetired
			);
		}
	} else {
		Result = __xrtHttpCacheNodeInsert(
			pCache, pRecord, &pRetired
		);
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	xrtHttpCacheRecordRelease(pOld);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 仅在相同 Vary 变体不存在时保存 Record。 */
XHTTP_POLICY_API xhttpcacheput xrtHttpCacheInsert(
	xhttpcache* pCache,
	xhttpcacherecord* pRecord
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;

	if ( (pCache == NULL) || (pRecord == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.Insert(
			pCache->Context,
			pRecord
		);
		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_CONFLICT) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_STORED) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pRecord);
	iCharge = xrtHttpCacheRecordCharge(pRecord);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pRecord,
		NULL
	);
	if ( pNode != NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_CONFLICT;
	}
	Fit = __xrtHttpCacheStateFits(
		pCache,
		0,
		0,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	Result = __xrtHttpCacheNodeInsert(
		pCache,
		pRecord,
		&pRetired
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 判断两个 Record 是否属于同一方法、URI 和 Partition。 */
static bool __xrtHttpCacheRecordBaseEqual(
	const xhttpcacherecord* pLeft,
	const xhttpcacherecord* pRight
)
{
	const xhttpcachekey* pLeftKey =
		xrtHttpCacheRecordKey(pLeft);
	const xhttpcachekey* pRightKey =
		xrtHttpCacheRecordKey(pRight);

	return (pLeftKey != NULL) &&
		(pRightKey != NULL) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->Method,
			pRightKey->Method
		) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->URI,
			pRightKey->URI
		) &&
		__xrtHttpCacheStoreViewEqual(
			pLeftKey->Partition,
			pRightKey->Partition
		);
}



/* 在 URI Bucket 中按不可变 Record 身份定位当前 Node。 */
static xrt_http_cache_node* __xrtHttpCacheRecordNode(
	xrt_http_cache_node* pNode,
	const xhttpcacherecord* pRecord,
	xrt_http_cache_node** ppPrevious
)
{
	xrt_http_cache_node* pPrevious = NULL;

	while ( pNode != NULL ) {
		if ( pNode->Record == pRecord ) {
			if ( ppPrevious != NULL ) {
				*ppPrevious = pPrevious;
			}
			return pNode;
		}
		pPrevious = pNode;
		pNode = pNode->BucketNext;
	}
	if ( ppPrevious != NULL ) {
		*ppPrevious = NULL;
	}
	return NULL;
}



/*
	仅在 Expected 仍是当前 Record 时提交 Replacement。
	若 Vary 变化后撞上另一变体，保留新响应并在同一锁内淘汰旧的重复 Node。
*/
XHTTP_POLICY_API xhttpcacheput xrtHttpCacheReplace(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected,
	xhttpcacherecord* pReplacement
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pPrevious;
	xrt_http_cache_node* pDuplicate;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcacherecord* pOld = NULL;
	xhttpcacherecord* pHeld = NULL;
	xhttpcacheput Result;
	xrt_http_cache_fit Fit;
	size_t iCharge;
	size_t iRemoveBytes;
	size_t iRemoveEntries;

	if ( (pCache == NULL) || (pExpected == NULL) ||
		(pReplacement == NULL) ||
		!__xrtHttpCacheRecordBaseEqual(
			pExpected,
			pReplacement
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.Replace(
			pCache->Context,
			pExpected,
			pReplacement
		);
		if ( (Result != XHTTP_CACHE_PUT_ERROR) &&
			(Result != XHTTP_CACHE_PUT_CONFLICT) &&
			(Result != XHTTP_CACHE_PUT_REJECTED) &&
			(Result != XHTTP_CACHE_PUT_REPLACED) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_CACHE_PUT_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pExpected);
	iCharge = xrtHttpCacheRecordCharge(pReplacement);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (iCharge > pCache->Config.MaxEntryBytes) ||
		(iCharge > pCache->Config.MaxBytes) ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheRecordNode(
		ppHead != NULL ? *ppHead : NULL,
		pExpected,
		&pPrevious
	);
	if ( pNode == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_CONFLICT;
	}

	pDuplicate = __xrtHttpCacheVariant(
		ppHead != NULL ? *ppHead : NULL,
		pReplacement,
		NULL
	);
	if ( pDuplicate == pNode ) {
		pDuplicate = NULL;
	}
	iRemoveEntries = 1u;
	iRemoveBytes = xrtHttpCacheRecordCharge(
		pNode->Record
	);
	if ( pDuplicate != NULL ) {
		size_t iDuplicateCharge =
			xrtHttpCacheRecordCharge(
				pDuplicate->Record
			);

		if ( iDuplicateCharge >
			(pCache->Stats.Bytes - iRemoveBytes) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		iRemoveEntries++;
		iRemoveBytes += iDuplicateCharge;
	}
	Fit = __xrtHttpCacheStateFits(
		pCache,
		iRemoveEntries,
		iRemoveBytes,
		1u,
		iCharge
	);
	if ( Fit != XRT_HTTP_CACHE_FIT_READY ) {
		if ( Fit == XRT_HTTP_CACHE_FIT_ERROR ) {
			(void)xrtMutexUnlock(&pCache->Lock);
			return XHTTP_CACHE_PUT_ERROR;
		}
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Rejected, 1
		);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_REJECTED;
	}
	pHeld = xrtHttpCacheRecordRetain(pReplacement);
	if ( pHeld == NULL ) {
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( (pDuplicate != NULL) &&
		!__xrtHttpCacheNodeRemove(
			pCache,
			pDuplicate,
			&pRetired
		) ) {
		xrtHttpCacheRecordRelease(pHeld);
		xrtSetErrorKind(XERR_INTERNAL);
		(void)xrtMutexUnlock(&pCache->Lock);
		return XHTTP_CACHE_PUT_ERROR;
	}
	if ( pDuplicate != NULL ) {
		pNode = __xrtHttpCacheRecordNode(
			ppHead != NULL ? *ppHead : NULL,
			pExpected,
			&pPrevious
		);
		if ( pNode == NULL ) {
			xrtHttpCacheRecordRelease(pHeld);
			xrtSetErrorKind(XERR_INTERNAL);
			Result = XHTTP_CACHE_PUT_ERROR;
			goto finish;
		}
	}
	__xrtHttpCacheNodeReplaceHeld(
		pCache,
		ppHead,
		pNode,
		pPrevious,
		pHeld,
		&pOld
	);
	Result = XHTTP_CACHE_PUT_REPLACED;
	__xrtHttpCacheLimit(
		pCache,
		&pRetired
	);

finish:
	(void)xrtMutexUnlock(&pCache->Lock);
	xrtHttpCacheRecordRelease(pOld);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 仅在 Expected 仍是当前 Record 时删除对应 Node。 */
XHTTP_POLICY_API xhttpcachechange xrtHttpCacheRemoveRecord(
	xhttpcache* pCache,
	const xhttpcacherecord* pExpected
)
{
	const xhttpcachekey* pKey;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node* pNode;
	xrt_http_cache_node* pRetired = NULL;
	xhttpcachechange Result;

	if ( (pCache == NULL) || (pExpected == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_CACHE_CHANGE_ERROR;
	}
	if ( pCache->Custom ) {
		Result = pCache->Ops.RemoveRecord(
			pCache->Context,
			pExpected
		);
		if ( (Result < XHTTP_CACHE_CHANGE_ERROR) ||
			(Result > XHTTP_CACHE_CHANGE_APPLIED) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_CACHE_CHANGE_ERROR;
		}
		return Result;
	}
	pKey = xrtHttpCacheRecordKey(pExpected);
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return XHTTP_CACHE_CHANGE_ERROR;
	}
	ppHead = __xrtHttpCacheBucket(pCache, pKey->URI);
	pNode = __xrtHttpCacheRecordNode(
		ppHead != NULL ? *ppHead : NULL,
		pExpected,
		NULL
	);
	if ( pNode == NULL ) {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Conflicts, 1
		);
		Result = XHTTP_CACHE_CHANGE_CONFLICT;
	} else if ( !__xrtHttpCacheNodeRemove(
		pCache,
		pNode,
		&pRetired
	) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		Result = XHTTP_CACHE_CHANGE_ERROR;
	} else {
		__xrtHttpCacheCounterAdd(
			&pCache->Stats.Removals, 1
		);
		Result = XHTTP_CACHE_CHANGE_APPLIED;
	}
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	return Result;
}



/* 删除与完整 Key 和 Vary 选择匹配的全部记录。 */
XHTTP_POLICY_API bool xrtHttpCacheRemove(
	xhttpcache* pCache,
	const xhttpcachekey* pKey,
	size_t* pRemoved
)
{
	xhttpcachekey Key;
	xrt_http_cache_node* pRetired = NULL;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node** ppNode;
	size_t iRemoved = 0;

	if ( (pRemoved != NULL) &&
		!xrtMemRangeValid(pRemoved, sizeof(iRemoved)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pCache == NULL ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtMemRangeValid(pKey, sizeof(Key)) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pRemoved != NULL) && xrtMemRangesOverlap(
		pKey, sizeof(Key), pRemoved, sizeof(iRemoved)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(pKey, &Key) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return false;
	}
	if ( (pRemoved != NULL) && __xrtHttpCacheKeyOverlap(
		&Key, pRemoved, sizeof(iRemoved)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	if ( pCache->Custom ) {
		if ( !pCache->Ops.Remove(
			pCache->Context,
			&Key,
			&iRemoved
		) ) {
			return false;
		}
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	ppHead = __xrtHttpCacheBucket(pCache, Key.URI);
	ppNode = ppHead;
	while ( (ppNode != NULL) && (*ppNode != NULL) ) {
		xrt_http_cache_node* pNode = *ppNode;

		if ( !__xrtHttpCacheRecordMatchesValid(
			pNode->Record, &Key
		) ) {
			ppNode = &pNode->BucketNext;
			continue;
		}
		*ppNode = pNode->BucketNext;
		pNode->BucketNext = NULL;
		__xrtHttpCacheLRURemove(pCache, pNode);
		pCache->Stats.Entries--;
		pCache->Stats.Bytes -=
			xrtHttpCacheRecordCharge(pNode->Record);
		__xrtHttpCacheRetire(&pRetired, pNode);
		iRemoved++;
	}
	if ( (ppHead != NULL) && (*ppHead == NULL) ) {
		(void)xrtMapRemove(
			&pCache->Index,
			__xrtHttpCacheMapKey(Key.URI)
		);
	}
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 删除 URI 和 Partition 下的全部方法与 Vary 变体。 */
XHTTP_POLICY_API bool xrtHttpCacheRemoveURI(
	xhttpcache* pCache,
	xstrview URI,
	xstrview Partition,
	size_t* pRemoved
)
{
	xrt_http_cache_node* pRetired = NULL;
	xrt_http_cache_node** ppHead;
	xrt_http_cache_node** ppNode;
	size_t iRemoved = 0;
	xhttpcachekey Key;
	xhttpcachekey Resolved;

	if ( (pRemoved != NULL) &&
		!xrtMemRangeValid(pRemoved, sizeof(iRemoved)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(&Key, 0, sizeof(Key));
	Key.Method = XRT_STR_LITERAL("GET");
	Key.URI = URI;
	Key.Partition = Partition;
	if ( pCache == NULL ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpCacheKeyResolve(&Key, &Resolved) ) {
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return false;
	}
	if ( (pRemoved != NULL) && __xrtHttpCacheKeyOverlap(
		&Resolved, pRemoved, sizeof(iRemoved)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	if ( pCache->Custom ) {
		if ( !pCache->Ops.RemoveURI(
			pCache->Context,
			URI,
			Partition,
			&iRemoved
		) ) {
			return false;
		}
		if ( pRemoved != NULL ) {
			memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
		}
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	ppHead = __xrtHttpCacheBucket(pCache, URI);
	ppNode = ppHead;
	while ( (ppNode != NULL) && (*ppNode != NULL) ) {
		xrt_http_cache_node* pNode = *ppNode;
		const xhttpcachekey* pStored =
			xrtHttpCacheRecordKey(pNode->Record);

		if ( !__xrtHttpCacheStoreViewEqual(
			pStored->Partition, Partition
		) ) {
			ppNode = &pNode->BucketNext;
			continue;
		}
		*ppNode = pNode->BucketNext;
		pNode->BucketNext = NULL;
		__xrtHttpCacheLRURemove(pCache, pNode);
		pCache->Stats.Entries--;
		pCache->Stats.Bytes -=
			xrtHttpCacheRecordCharge(pNode->Record);
		__xrtHttpCacheRetire(&pRetired, pNode);
		iRemoved++;
	}
	if ( (ppHead != NULL) && (*ppHead == NULL) ) {
		(void)xrtMapRemove(
			&pCache->Index,
			__xrtHttpCacheMapKey(URI)
		);
	}
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheRetiredRelease(pRetired);
	if ( pRemoved != NULL ) {
		memcpy(pRemoved, &iRemoved, sizeof(iRemoved));
	}
	return true;
}



/* 删除全部记录并保留 Map 桶容量。 */
XHTTP_POLICY_API bool xrtHttpCacheClear(xhttpcache* pCache)
{
	xrt_http_cache_node* pNodes;
	size_t iRemoved;

	if ( pCache == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pCache->Custom ) {
		return pCache->Ops.Clear(pCache->Context);
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	pNodes = pCache->LRUHead;
	iRemoved = pCache->Stats.Entries;
	pCache->LRUHead = NULL;
	pCache->LRUTail = NULL;
	xrtMapClear(&pCache->Index);
	pCache->Stats.Entries = 0;
	pCache->Stats.Bytes = 0;
	__xrtHttpCacheCounterAdd(
		&pCache->Stats.Removals, (uint64)iRemoved
	);
	(void)xrtMutexUnlock(&pCache->Lock);
	__xrtHttpCacheLRURelease(pNodes);
	return true;
}



/* 取得条目、字节和累计行为的一致快照。 */
XHTTP_POLICY_API bool xrtHttpCacheStats(
	xhttpcache* pCache,
	xhttpcachestats* pStats
)
{
	xhttpcachestats Stats = { 0 };

	if ( (pCache == NULL) ||
		!xrtMemRangeValid(pStats, sizeof(Stats)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pStats, &Stats, sizeof(Stats));
	if ( pCache->Custom ) {
		if ( !pCache->Ops.Stats(
			pCache->Context,
			&Stats
		) ) {
			memset(&Stats, 0, sizeof(Stats));
			memcpy(pStats, &Stats, sizeof(Stats));
			return false;
		}
		memcpy(pStats, &Stats, sizeof(Stats));
		return true;
	}
	if ( !xrtMutexLock(&pCache->Lock) ) {
		return false;
	}
	Stats = pCache->Stats;
	(void)xrtMutexUnlock(&pCache->Lock);
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_compute.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SHA2)

#define XRT_HTTP_DIGEST_MAX_MESSAGE (UINT64_MAX >> 3u)



/* 统一承载 SHA-256、SHA-512/256 与可选 MD5 的流状态。 */
typedef union xrt_http_auth_digest_hash_state {
	xsha256 Sha256;
	xsha512_256 Sha512_256;
	#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
	xmd5 Md5;
	#endif
} xrt_http_auth_digest_hash_state;



/* 把 session 算法映射到共享同一摘要函数的基础算法。 */
static xhttpdigestalgorithm __xrtHttpDigestBaseAlgorithm(
	xhttpdigestalgorithm Algorithm
)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return XHTTP_DIGEST_ALGORITHM_MD5;
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
			return XHTTP_DIGEST_ALGORITHM_SHA256;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return XHTTP_DIGEST_ALGORITHM_SHA512_256;
		default:
			return Algorithm;
	}
}



/* 区分无效算法值与当前裁剪闭包未提供的已知算法。 */
static bool __xrtHttpDigestAlgorithmValid(
	xhttpdigestalgorithm Algorithm
)
{
	if ( xrtHttpDigestSize(Algorithm) == 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( !xrtHttpDigestAlgorithmSupported(Algorithm) ) {
		xrtSetErrorKind(XERR_UNSUPPORTED);
		return false;
	}
	return true;
}



/* 初始化所选摘要后端。 */
static void __xrtHttpDigestStateInit(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm
)
{
	memset(pState, 0, sizeof(*pState));
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			xrtSha256Init(&pState->Sha256);
			break;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			xrtSha512_256Init(&pState->Sha512_256);
			break;
		#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			xrtMd5Init(&pState->Md5);
			break;
		#endif
		default:
			break;
	}
}



/* 向所选摘要后端追加一段连续字节。 */
static bool __xrtHttpDigestStateUpdate(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iSize
)
{
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return xrtSha256Update(&pState->Sha256, pData, iSize);
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return xrtSha512_256Update(
				&pState->Sha512_256, pData, iSize
			);
		#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return xrtMd5Update(&pState->Md5, pData, iSize);
		#endif
		default:
			xrtSetErrorKind(XERR_UNSUPPORTED);
			return false;
	}
}



/* 从所选摘要后端的快照读取二进制摘要。 */
static bool __xrtHttpDigestStateFinal(
	const xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	void* pDigest
)
{
	switch ( __xrtHttpDigestBaseAlgorithm(Algorithm) ) {
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return xrtSha256Final(&pState->Sha256, pDigest);
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return xrtSha512_256Final(
				&pState->Sha512_256, pDigest
			);
		#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_MD5)
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return xrtMd5Final(&pState->Md5, pDigest);
		#endif
		default:
			xrtSetErrorKind(XERR_UNSUPPORTED);
			return false;
	}
}



/* 把二进制摘要转换为 RFC 7616 使用的小写十六进制文本。 */
static void __xrtHttpDigestHexWrite(
	const uint8* pDigest,
	size_t iSize,
	char* sOutput
)
{
	static const char Hex[] = "0123456789abcdef";

	for ( size_t i = 0; i < iSize; i++ ) {
		sOutput[i * 2u] = Hex[pDigest[i] >> 4u];
		sOutput[(i * 2u) + 1u] = Hex[pDigest[i] & 0x0Fu];
	}
}



/* 完成当前摘要，规范编码文本，并清理流状态和二进制副本。 */
static bool __xrtHttpDigestStateFinalText(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	char* sOutput
)
{
	uint8 Digest[XHTTP_DIGEST_MAX_SIZE];
	size_t iSize = xrtHttpDigestSize(Algorithm);
	bool bResult;

	bResult = __xrtHttpDigestStateFinal(
		pState, Algorithm, Digest
	);
	if ( bResult ) {
		__xrtHttpDigestHexWrite(Digest, iSize, sOutput);
	}
	xrtSecureZero(Digest, sizeof(Digest));
	xrtSecureZero(pState, sizeof(*pState));
	return bResult;
}



/* 校验输出描述符、固定结构和所有借用输入之间没有危险别名。 */
static bool __xrtHttpDigestOutputValid(
	const void* pOwner,
	size_t iOwnerSize,
	const xstrview* pViews,
	size_t iViewCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !xrtMemRangeValid(pSize, sizeof(size_t)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtMemRangeValid(
			pOutput, iCapacity
		)) || ((pOwner != NULL) && (!xrtMemRangeValid(
			pOwner, iOwnerSize
		) || xrtMemRangesOverlap(
			pOwner, iOwnerSize, pSize, sizeof(size_t)
		) || ((pOutput != NULL) && xrtMemRangesOverlap(
			pOwner, iOwnerSize, pOutput, iCapacity
		)))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( size_t i = 0; i < iViewCount; i++ ) {
		if ( !__xhttpViewValid(pViews[i]) ||
			xrtMemRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pSize, sizeof(size_t)
			) || ((pOwner != NULL) && xrtMemRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pOwner, iOwnerSize
			)) || ((pOutput != NULL) && xrtMemRangesOverlap(
				pViews[i].Data, pViews[i].Size,
				pOutput, iCapacity
			)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
	}
	return true;
}



/* 发布固定长度摘要的查询、短缓冲或实际写入决策。 */
static bool __xrtHttpDigestOutputPrepare(
	size_t iRequired,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pWrite
)
{
	*pWrite = false;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pWrite = true;
	return true;
}



/* 保证拼接后的摘要消息落在所有当前后端都能表示的长度范围内。 */
static bool __xrtHttpDigestMessageSize(
	const size_t* pParts,
	size_t iCount
)
{
	size_t iTotal = 0;

	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xhttpSizeAdd(&iTotal, pParts[i]) ) {
			return false;
		}
	}
	if ( (uint64)iTotal > XRT_HTTP_DIGEST_MAX_MESSAGE ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 向摘要状态追加一个已经验证的文本视图。 */
static bool __xrtHttpDigestStateText(
	xrt_http_auth_digest_hash_state* pState,
	xhttpdigestalgorithm Algorithm,
	xstrview Text
)
{
	return __xrtHttpDigestStateUpdate(
		pState, Algorithm, Text.Data, Text.Size
	);
}



/* 使用所选 Digest 算法散列任意连续数据。 */
XHTTP_POLICY_API bool xrtHttpDigestHash(
	xhttpdigestalgorithm Algorithm,
	const void* pData,
	size_t iDataSize,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Data = { (cstr)pData, iDataSize };
	xrt_http_auth_digest_hash_state State;
	char Text[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iRequired;
	bool bWrite;

	if ( !__xrtHttpDigestOutputValid(
		NULL, 0, &Data, 1u,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(Algorithm) ||
		!__xrtHttpDigestMessageSize(&iDataSize, 1u) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	__xrtHttpDigestStateInit(&State, Algorithm);
	if ( !__xrtHttpDigestStateUpdate(
		&State, Algorithm, pData, iDataSize
	) || !__xrtHttpDigestStateFinalText(
		&State, Algorithm, Text
	) ) {
		xrtSecureZero(&State, sizeof(State));
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算用户身份拼接值，Password 为空指针时生成 userhash。 */
static bool __xrtHttpDigestIdentity(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	const xstrview* pPassword,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xstrview Views[3] = { Username, Realm, { NULL, 0 } };
	xrt_http_auth_digest_hash_state State;
	char Text[XHTTP_DIGEST_MAX_TEXT_SIZE];
	static const char Colon = ':';
	size_t Parts[5];
	size_t iViewCount = 2u;
	size_t iPartCount = 3u;
	size_t iRequired;
	bool bWrite;

	Parts[0] = Username.Size;
	Parts[1] = 1u;
	Parts[2] = Realm.Size;
	if ( pPassword != NULL ) {
		Views[2] = *pPassword;
		iViewCount = 3u;
		Parts[3] = 1u;
		Parts[4] = pPassword->Size;
		iPartCount = 5u;
	}
	if ( !__xrtHttpDigestOutputValid(
		NULL, 0, Views, iViewCount,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(Algorithm) ||
		!__xrtHttpDigestMessageSize(Parts, iPartCount) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	__xrtHttpDigestStateInit(&State, Algorithm);
	if ( !__xrtHttpDigestStateText(
		&State, Algorithm, Username
	) || !__xrtHttpDigestStateUpdate(
		&State, Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, Algorithm, Realm
	) || ((pPassword != NULL) && (!__xrtHttpDigestStateUpdate(
		&State, Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, Algorithm, *pPassword
	))) || !__xrtHttpDigestStateFinalText(
		&State, Algorithm, Text
	) ) {
		xrtSecureZero(&State, sizeof(State));
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算可持久化的基础 H(username:realm:password)。 */
XHTTP_POLICY_API bool xrtHttpDigestSecret(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	xstrview Password,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestIdentity(
		Algorithm, Username, Realm, &Password,
		pOutput, iCapacity, pSize
	);
}



/* 计算 RFC 7616 的 H(username:realm) 用户名摘要。 */
XHTTP_POLICY_API bool xrtHttpDigestUserHash(
	xhttpdigestalgorithm Algorithm,
	xstrview Username,
	xstrview Realm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestIdentity(
		Algorithm, Username, Realm, NULL,
		pOutput, iCapacity, pSize
	);
}



/* 把已经验证的十六进制文本规范化为小写。 */
static void __xrtHttpDigestHexLower(
	xstrview Text,
	char* sOutput
)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		sOutput[i] = (char)__xhttpAsciiLower(
			(uint8)Text.Data[i]
		);
	}
}



/* 校验证明上下文、借用视图、算法和 qop 关系。 */
static bool __xrtHttpDigestProofValid(
	const xhttpdigestproof* pInput,
	xstrview Method,
	bool bResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestproof* pProof
)
{
	xstrview Views[6];
	size_t iViewCount = bResponse ? 5u : 6u;
	size_t iDigest;

	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pProof, pInput, sizeof(*pProof));
	Views[0] = pProof->Secret;
	Views[1] = pProof->Nonce;
	Views[2] = pProof->Cnonce;
	Views[3] = pProof->Uri;
	Views[4] = pProof->EntityHash;
	Views[5] = Method;
	if ( !__xrtHttpDigestOutputValid(
		pInput, sizeof(*pInput), Views, iViewCount,
		pOutput, iCapacity, pSize
	) || !__xrtHttpDigestAlgorithmValid(pProof->Algorithm) ) {
		return false;
	}
	iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	if ( ((pProof->Qop != XHTTP_DIGEST_QOP_AUTH) &&
		 (pProof->Qop != XHTTP_DIGEST_QOP_AUTH_INT)) ||
		(pProof->NonceCount == 0) ||
		(pProof->Secret.Size != iDigest) ||
		!xrtHttpBridgeDigestHexViewValid(pProof->Secret) ||
		(pProof->Nonce.Size == 0) ||
		(pProof->Cnonce.Size == 0) ||
		(pProof->Uri.Size == 0) ||
		(!bResponse && (Method.Size == 0)) ||
		((pProof->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 (pProof->EntityHash.Size != 0)) ||
		((pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pProof->EntityHash.Size != iDigest) ||
		  !xrtHttpBridgeDigestHexViewValid(pProof->EntityHash))) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 预检 session A1、A2 和最终 KD 三段消息的累计长度。 */
static bool __xrtHttpDigestProofSizes(
	const xhttpdigestproof* pProof,
	xstrview Method,
	bool bResponse
)
{
	size_t iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	size_t A1[] = {
		iDigest, 1u, pProof->Nonce.Size, 1u, pProof->Cnonce.Size
	};
	size_t A2[] = {
		bResponse ? 0u : Method.Size,
		1u,
		pProof->Uri.Size,
		(pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) ? 1u : 0u,
		(pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) ? iDigest : 0u
	};
	xstrview Qop = xrtHttpDigestQopName(pProof->Qop);
	size_t Final[] = {
		iDigest, 1u, pProof->Nonce.Size, 1u, 8u, 1u,
		pProof->Cnonce.Size, 1u, Qop.Size, 1u, iDigest
	};

	if ( xrtHttpDigestAlgorithmSession(pProof->Algorithm) &&
		!__xrtHttpDigestMessageSize(
			A1, sizeof(A1) / sizeof(A1[0])
		) ) {
		return false;
	}
	return __xrtHttpDigestMessageSize(
		A2, sizeof(A2) / sizeof(A2[0])
	) && __xrtHttpDigestMessageSize(
		Final, sizeof(Final) / sizeof(Final[0])
	);
}



/* 计算请求 response 或服务器 rspauth 的公共 RFC 7616 公式。 */
static bool __xrtHttpDigestProofCalculate(
	const xhttpdigestproof* pProof,
	xstrview Method,
	bool bResponse,
	char* sOutput
)
{
	xrt_http_auth_digest_hash_state State;
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char A1[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char A2[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char Entity[XHTTP_DIGEST_MAX_TEXT_SIZE];
	char NonceCount[8];
	static const char Colon = ':';
	xstrview Qop = xrtHttpDigestQopName(pProof->Qop);
	size_t iDigest = xrtHttpDigestSize(pProof->Algorithm) * 2u;
	bool bResult = false;

	__xrtHttpDigestHexLower(pProof->Secret, Secret);
	if ( pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		__xrtHttpDigestHexLower(pProof->EntityHash, Entity);
	}
	if ( xrtHttpDigestAlgorithmSession(pProof->Algorithm) ) {
		__xrtHttpDigestStateInit(&State, pProof->Algorithm);
		if ( !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, Secret, iDigest
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateText(
			&State, pProof->Algorithm, pProof->Nonce
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateText(
			&State, pProof->Algorithm, pProof->Cnonce
		) || !__xrtHttpDigestStateFinalText(
			&State, pProof->Algorithm, A1
		) ) {
			goto cleanup;
		}
	} else {
		memcpy(A1, Secret, iDigest);
	}

	__xrtHttpDigestStateInit(&State, pProof->Algorithm);
	if ( (!bResponse && !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, Method
	)) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Uri
	) || ((pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		(!__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, &Colon, 1u
		) || !__xrtHttpDigestStateUpdate(
			&State, pProof->Algorithm, Entity, iDigest
		))) || !__xrtHttpDigestStateFinalText(
		&State, pProof->Algorithm, A2
	) ) {
		goto cleanup;
	}

	xrtHttpBridgeDigestNonceCountWrite(
		pProof->NonceCount, NonceCount
	);
	__xrtHttpDigestStateInit(&State, pProof->Algorithm);
	if ( !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, A1, iDigest
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Nonce
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, NonceCount, sizeof(NonceCount)
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, pProof->Cnonce
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateText(
		&State, pProof->Algorithm, Qop
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, &Colon, 1u
	) || !__xrtHttpDigestStateUpdate(
		&State, pProof->Algorithm, A2, iDigest
	) || !__xrtHttpDigestStateFinalText(
		&State, pProof->Algorithm, sOutput
	) ) {
		goto cleanup;
	}
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(Secret, sizeof(Secret));
	xrtSecureZero(A1, sizeof(A1));
	xrtSecureZero(A2, sizeof(A2));
	xrtSecureZero(Entity, sizeof(Entity));
	xrtSecureZero(NonceCount, sizeof(NonceCount));
	return bResult;
}



/* 执行请求 response 与服务器 rspauth 的公共入口契约。 */
static bool __xrtHttpDigestProof(
	const xhttpdigestproof* pInput,
	xstrview Method,
	bool bResponse,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xhttpdigestproof Proof;
	char Text[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iRequired;
	bool bWrite;

	if ( !__xrtHttpDigestProofValid(
		pInput, Method, bResponse,
		pOutput, iCapacity, pSize, &Proof
	) || !__xrtHttpDigestProofSizes(
		&Proof, Method, bResponse
	) ) {
		return false;
	}
	iRequired = xrtHttpDigestSize(Proof.Algorithm) * 2u;
	if ( !__xrtHttpDigestOutputPrepare(
		iRequired, pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	if ( !__xrtHttpDigestProofCalculate(
		&Proof, Method, bResponse, Text
	) ) {
		xrtSecureZero(Text, sizeof(Text));
		return false;
	}
	memcpy(pOutput, Text, iRequired);
	memcpy(pSize, &iRequired, sizeof(iRequired));
	xrtSecureZero(Text, sizeof(Text));
	return true;
}



/* 计算 Authorization 中的 request-digest。 */
XHTTP_POLICY_API bool xrtHttpDigestRequest(
	const xhttpdigestproof* pProof,
	xstrview Method,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestProof(
		pProof, Method, false,
		pOutput, iCapacity, pSize
	);
}



/* 计算 Authentication-Info 中的 rspauth。 */
XHTTP_POLICY_API bool xrtHttpDigestRspAuth(
	const xhttpdigestproof* pProof,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtHttpDigestProof(
		pProof, (xstrview){ NULL, 0 }, true,
		pOutput, iCapacity, pSize
	);
}



/* 常量时间比较大小写不敏感的十六进制摘要。 */
XHTTP_POLICY_API bool xrtHttpDigestEqual(xstrview Left, xstrview Right)
{
	uint32 iDifference = 0;
	uint32 iInvalid = 0;

	if ( !xrtMemRangeValid(Left.Data, Left.Size) ||
		!xrtMemRangeValid(Right.Data, Right.Size) ||
		(Left.Size == 0) ||
		(Left.Size != Right.Size) ||
		((Left.Size & 1u) != 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		int iLeft = xrtHttpBridgeDigestHexValue((uint8)Left.Data[i]);
		int iRight = xrtHttpBridgeDigestHexValue((uint8)Right.Data[i]);

		if ( (iLeft < 0) || (iRight < 0) ) {
			iInvalid = 1u;
			iLeft = 0;
			iRight = 0;
		}
		iDifference |= (uint32)(iLeft ^ iRight);
	}
	return (iDifference | iInvalid) == 0;
}



#undef XRT_HTTP_DIGEST_MAX_MESSAGE

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_nonce.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE) || \
	defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE)

#define XRT_HTTP_DIGEST_NONCE_VERSION 1u
#define XRT_HTTP_DIGEST_NONCE_BODY_SIZE \
	(1u + 8u + XHTTP_DIGEST_NONCE_SALT_SIZE)
#define XRT_HTTP_DIGEST_NONCE_BINARY_SIZE \
	(XRT_HTTP_DIGEST_NONCE_BODY_SIZE + XRT_SHA256_SIZE)
#define XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE (UINT64_MAX >> 3u)



static const uint8 __xrtHttpDigestNonceDomain[] =
	"xrt-http-digest-nonce-v1";



/* 写出无符号 64 位大端时间戳。 */
static void __xrtHttpDigestNonceStore64(uint8* pOutput, uint64 iValue)
{
	for ( size_t i = 0; i < 8u; i++ ) {
		pOutput[7u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 读取无符号 64 位大端时间戳。 */
static uint64 __xrtHttpDigestNonceLoad64(const uint8* pInput)
{
	uint64 iValue = 0;

	for ( size_t i = 0; i < 8u; i++ ) {
		iValue = (iValue << 8u) | (uint64)pInput[i];
	}
	return iValue;
}



/* 验证密钥、上下文和 HMAC 可表示的累计消息长度。 */
static bool __xrtHttpDigestNonceInputValid(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds
)
{
	uint64 iPrefix = (uint64)(
		sizeof(__xrtHttpDigestNonceDomain) - 1u +
		XRT_HTTP_DIGEST_NONCE_BODY_SIZE
	);

	if ( !xrtMemRangeValid(Key.Data, Key.Size) ||
		!xrtMemRangeValid(Context.Data, Context.Size) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (Key.Size < XHTTP_DIGEST_NONCE_KEY_MIN) ||
		(iIssuedSeconds < 0) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( ((uint64)Key.Size > XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE) ||
		((uint64)Context.Size >
		 (XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE - iPrefix)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 验证写出描述符和所有输入范围互不覆盖。 */
static bool __xrtHttpDigestNonceOutputValid(
	xbytesview Key,
	xbytesview Context,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	if ( !xrtMemRangeValid(pSize, sizeof(size_t)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtMemRangeValid(
			pOutput, iCapacity
		)) || ((pSalt != NULL) && !xrtMemRangeValid(
			pSalt, XHTTP_DIGEST_NONCE_SALT_SIZE
		)) || xrtMemRangesOverlap(
			pSize, sizeof(size_t), Key.Data, Key.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(size_t), Context.Data, Context.Size
		) || ((pSalt != NULL) && xrtMemRangesOverlap(
			pSize, sizeof(size_t),
			pSalt, XHTTP_DIGEST_NONCE_SALT_SIZE
		)) || ((pOutput != NULL) && (
			xrtMemRangesOverlap(
				pOutput, iCapacity, pSize, sizeof(size_t)
			) || xrtMemRangesOverlap(
				pOutput, iCapacity, Key.Data, Key.Size
			) || xrtMemRangesOverlap(
				pOutput, iCapacity, Context.Data, Context.Size
			) || ((pSalt != NULL) && xrtMemRangesOverlap(
				pOutput, iCapacity,
				pSalt, XHTTP_DIGEST_NONCE_SALT_SIZE
			))
		)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 发布固定长度 nonce 的查询、短缓冲或实际写入决策。 */
static bool __xrtHttpDigestNonceOutputPrepare(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pWrite
)
{
	size_t iRequired = XHTTP_DIGEST_NONCE_TEXT_SIZE;

	*pWrite = false;
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pWrite = true;
	return true;
}



/* 计算 HMAC 并把固定二进制 nonce 编码为 Base64URL。 */
static bool __xrtHttpDigestNonceEncode(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const uint8* pSalt,
	char* sOutput
)
{
	static const xbase64config Config = {
		NULL,
		(uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
	};
	xhmacsha256 State;
	uint8 Binary[XRT_HTTP_DIGEST_NONCE_BINARY_SIZE];
	char Encoded[XHTTP_DIGEST_NONCE_TEXT_SIZE + 1u];
	size_t iSize;
	bool bResult = false;

	memset(&State, 0, sizeof(State));
	memset(Binary, 0, sizeof(Binary));
	memset(Encoded, 0, sizeof(Encoded));
	Binary[0] = XRT_HTTP_DIGEST_NONCE_VERSION;
	__xrtHttpDigestNonceStore64(
		Binary + 1u, (uint64)iIssuedSeconds
	);
	memcpy(
		Binary + 9u,
		pSalt,
		XHTTP_DIGEST_NONCE_SALT_SIZE
	);
	if ( !xrtHmacSha256Init(&State, Key.Data, Key.Size) ||
		!xrtHmacSha256Update(
			&State,
			__xrtHttpDigestNonceDomain,
			sizeof(__xrtHttpDigestNonceDomain) - 1u
		) || !xrtHmacSha256Update(
			&State, Binary, XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) || !xrtHmacSha256Update(
			&State, Context.Data, Context.Size
		) || !xrtHmacSha256Final(
			&State, Binary + XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) || !xrtBase64Encode(
			Binary,
			sizeof(Binary),
			Encoded,
			sizeof(Encoded),
			&iSize,
			&Config
		) || (iSize != XHTTP_DIGEST_NONCE_TEXT_SIZE) ) {
		goto cleanup;
	}
	memcpy(sOutput, Encoded, XHTTP_DIGEST_NONCE_TEXT_SIZE);
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(Binary, sizeof(Binary));
	xrtSecureZero(Encoded, sizeof(Encoded));
	return bResult;
}



/* 使用调用方 salt 构建确定性的无状态 nonce。 */
XHTTP_POLICY_API bool xrtHttpDigestNonceWrite(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	const void* pSalt,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
	bool bWrite;

	if ( (pSalt == NULL) || !xrtMemRangeValid(
		pSalt, XHTTP_DIGEST_NONCE_SALT_SIZE
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpDigestNonceInputValid(
		Key, Context, iIssuedSeconds
	) || !__xrtHttpDigestNonceOutputValid(
		Key, Context, pSalt, pOutput, iCapacity, pSize
	) ) {
		return false;
	}
	if ( !__xrtHttpDigestNonceOutputPrepare(
		pOutput, iCapacity, pSize, &bWrite
	) || !bWrite ) {
		return pOutput == NULL;
	}
	if ( !__xrtHttpDigestNonceEncode(
		Key, Context, iIssuedSeconds, (const uint8*)pSalt, Nonce
	) ) {
		xrtSecureZero(Nonce, sizeof(Nonce));
		return false;
	}
	memcpy(pOutput, Nonce, sizeof(Nonce));
	{
		size_t iRequired = sizeof(Nonce);

		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	xrtSecureZero(Nonce, sizeof(Nonce));
	return true;
}



/* 判断固定长度输入是否只包含无填充 Base64URL 字符。 */
static bool __xrtHttpDigestNonceTextValid(xstrview Nonce)
{
	if ( Nonce.Size != XHTTP_DIGEST_NONCE_TEXT_SIZE ) {
		return false;
	}
	for ( size_t i = 0; i < Nonce.Size; i++ ) {
		uint8 c = (uint8)Nonce.Data[i];

		if ( !(((c >= (uint8)'A') && (c <= (uint8)'Z')) ||
			  ((c >= (uint8)'a') && (c <= (uint8)'z')) ||
			  ((c >= (uint8)'0') && (c <= (uint8)'9')) ||
			  (c == (uint8)'-') || (c == (uint8)'_')) ) {
			return false;
		}
	}
	return true;
}



/* 重新计算固定二进制 nonce 的 HMAC。 */
static bool __xrtHttpDigestNonceMac(
	xbytesview Key,
	xbytesview Context,
	const uint8* pBinary,
	uint8* pMac
)
{
	xhmacsha256 State;
	bool bResult;

	memset(&State, 0, sizeof(State));
	bResult = xrtHmacSha256Init(&State, Key.Data, Key.Size) &&
		xrtHmacSha256Update(
			&State,
			__xrtHttpDigestNonceDomain,
			sizeof(__xrtHttpDigestNonceDomain) - 1u
		) && xrtHmacSha256Update(
			&State, pBinary, XRT_HTTP_DIGEST_NONCE_BODY_SIZE
		) && xrtHmacSha256Update(
			&State, Context.Data, Context.Size
		) && xrtHmacSha256Final(&State, pMac);
	xrtSecureZero(&State, sizeof(State));
	return bResult;
}



/* 验证无状态 nonce 的结构、签名和时间窗口。 */
XHTTP_POLICY_API xhttpdigestnoncecheck xrtHttpDigestNonceVerify(
	xstrview Nonce,
	xbytesview Key,
	xbytesview Context,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
)
{
	static const xbase64config Config = {
		NULL,
		(uint32)XBASE64_URL | (uint32)XBASE64_NO_PADDING
	};
	uint8 Binary[XRT_HTTP_DIGEST_NONCE_BINARY_SIZE];
	uint8 Mac[XRT_SHA256_SIZE];
	uint64 iUnsignedIssued;
	int64 iIssued;
	size_t iSize;
	xhttpdigestnoncecheck Check = XHTTP_DIGEST_NONCE_ERROR;

	if ( !__xhttpViewValid(Nonce) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( !__xrtHttpDigestNonceInputValid(Key, Context, 0) ) {
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( (pIssuedSeconds != NULL) && (
			!xrtMemRangeValid(pIssuedSeconds, sizeof(int64)) ||
			xrtMemRangesOverlap(
				pIssuedSeconds, sizeof(int64), Nonce.Data, Nonce.Size
			) || xrtMemRangesOverlap(
				pIssuedSeconds, sizeof(int64), Key.Data, Key.Size
			) || xrtMemRangesOverlap(
				pIssuedSeconds, sizeof(int64),
				Context.Data, Context.Size
			)
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( (iNowSeconds < 0) || (iLifetimeSeconds <= 0) ||
		(iFutureSkewSeconds < 0) ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_DIGEST_NONCE_ERROR;
	}
	if ( !__xrtHttpDigestNonceTextValid(Nonce) ) {
		return XHTTP_DIGEST_NONCE_INVALID;
	}
	memset(Binary, 0, sizeof(Binary));
	memset(Mac, 0, sizeof(Mac));
	if ( !xrtBase64Decode(
		Nonce.Data,
		Nonce.Size,
		Binary,
		sizeof(Binary),
		&iSize,
		&Config
	) || (iSize != sizeof(Binary)) ) {
		goto cleanup;
	}
	if ( Binary[0] != XRT_HTTP_DIGEST_NONCE_VERSION ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	if ( !__xrtHttpDigestNonceMac(Key, Context, Binary, Mac) ) {
		goto cleanup;
	}
	if ( !xrtConstTimeEqual(
		Mac,
		Binary + XRT_HTTP_DIGEST_NONCE_BODY_SIZE,
		sizeof(Mac)
	) ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	iUnsignedIssued = __xrtHttpDigestNonceLoad64(Binary + 1u);
	if ( iUnsignedIssued > (uint64)INT64_MAX ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	iIssued = (int64)iUnsignedIssued;
	if ( (iIssued > iNowSeconds) &&
		((iIssued - iNowSeconds) > iFutureSkewSeconds) ) {
		Check = XHTTP_DIGEST_NONCE_INVALID;
		goto cleanup;
	}
	Check = ((iNowSeconds > iIssued) &&
		((iNowSeconds - iIssued) > iLifetimeSeconds)) ?
		XHTTP_DIGEST_NONCE_STALE : XHTTP_DIGEST_NONCE_VALID;
	if ( pIssuedSeconds != NULL ) {
		memcpy(pIssuedSeconds, &iIssued, sizeof(iIssued));
	}

cleanup:
	xrtSecureZero(Binary, sizeof(Binary));
	xrtSecureZero(Mac, sizeof(Mac));
	return Check;
}



#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_NONCE_RANDOM)

/* 使用系统安全随机 salt 构建无状态 nonce。 */
XHTTP_POLICY_API bool xrtHttpDigestNonceCreate(
	xbytesview Key,
	xbytesview Context,
	int64 iIssuedSeconds,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	uint8 Salt[XHTTP_DIGEST_NONCE_SALT_SIZE];
	char Nonce[XHTTP_DIGEST_NONCE_TEXT_SIZE];
	bool bWrite;
	bool bResult = false;

	if ( !__xrtHttpDigestNonceInputValid(
		Key, Context, iIssuedSeconds
	) || !__xrtHttpDigestNonceOutputValid(
		Key, Context, NULL, pOutput, iCapacity, pSize
	) ) {
		return false;
	}
	if ( !__xrtHttpDigestNonceOutputPrepare(
		pOutput, iCapacity, pSize, &bWrite
	) ) {
		return false;
	}
	if ( !bWrite ) {
		return true;
	}
	if ( !xrtSecureRandom(Salt, sizeof(Salt)) ||
		!__xrtHttpDigestNonceEncode(
			Key, Context, iIssuedSeconds, Salt, Nonce
		) ) {
		goto cleanup;
	}
	memcpy(pOutput, Nonce, sizeof(Nonce));
	{
		size_t iRequired = sizeof(Nonce);

		memcpy(pSize, &iRequired, sizeof(iRequired));
	}
	bResult = true;

cleanup:
	xrtSecureZero(Salt, sizeof(Salt));
	xrtSecureZero(Nonce, sizeof(Nonce));
	return bResult;
}

#endif



#undef XRT_HTTP_DIGEST_NONCE_VERSION
#undef XRT_HTTP_DIGEST_NONCE_BODY_SIZE
#undef XRT_HTTP_DIGEST_NONCE_BINARY_SIZE
#undef XRT_HTTP_DIGEST_NONCE_MAX_MESSAGE

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_verify.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_VERIFY)

#define XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH)



/* 工作副本允许公开描述符和两个协议对象位于未对齐存储。 */
typedef struct xrt_http_digest_verify_prepared {
	xhttpdigestverification Verification;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;
} xrt_http_digest_verify_prepared;



/* 使用协议写出器的查询模式统一验证 challenge 与凭据结构。 */
static bool __xrtHttpDigestVerifyProtocol(
	const xhttpdigestauth* pAuth,
	const xhttpdigestchallenge* pChallenge
)
{
	size_t iSize;

	return xrtHttpDigestAuthWrite(
		pAuth, NULL, 0, &iSize
	) && xrtHttpDigestChallengeWrite(
		pChallenge, NULL, 0, &iSize
	);
}



/* 复制并验证固定描述符，策略错误与普通认证失败严格分离。 */
static bool __xrtHttpDigestVerifyPrepare(
	const xhttpdigestverification* pInput,
	xrt_http_digest_verify_prepared* pPrepared
)
{
	xhttpdigestverification Verification;
	xhttpdigestauth Auth;
	xhttpdigestchallenge Challenge;

	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Verification, pInput, sizeof(Verification));
	if ( !xrtMemRangeValid(Verification.Auth, sizeof(Auth)) ||
		!xrtMemRangeValid(Verification.Challenge, sizeof(Challenge)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Auth, Verification.Auth, sizeof(Auth));
	memcpy(&Challenge, Verification.Challenge, sizeof(Challenge));
	if ( !__xhttpViewValid(Verification.Secret) ||
		!__xhttpViewValid(Verification.Method) ||
		!__xhttpViewValid(Verification.RequestTarget) ||
		!__xhttpViewValid(Verification.EntityHash) ) {
		return false;
	}
	if ( ((Verification.Flags &
		~XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS) != 0) ||
		(Verification.Method.Size == 0) ||
		(Verification.RequestTarget.Size == 0) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( !__xrtHttpDigestVerifyProtocol(&Auth, &Challenge) ) {
		return false;
	}
	if ( !xrtHttpDigestAlgorithmSupported(Challenge.Algorithm) ) {
		xrtSetErrorKind(XERR_UNSUPPORTED);
		return false;
	}
	if ( ((Verification.Flags &
		XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH) != 0) &&
		(((Challenge.Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) == 0) ||
		 ((Challenge.Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) == 0)) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	pPrepared->Verification = Verification;
	pPrepared->Auth = Auth;
	pPrepared->Challenge = Challenge;
	pPrepared->Verification.Auth = &pPrepared->Auth;
	pPrepared->Verification.Challenge = &pPrepared->Challenge;
	return true;
}



/* 判断凭据选择是否与服务器实际发布的 challenge 完全一致。 */
static bool __xrtHttpDigestVerifyChallenge(
	const xrt_http_digest_verify_prepared* pPrepared
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	bool bChallengeOpaque = (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) != 0;
	bool bAuthOpaque = (pAuth->Flags &
		XHTTP_DIGEST_AUTH_HAS_OPAQUE) != 0;
	bool bUserHash = (pAuth->Flags &
		XHTTP_DIGEST_AUTH_USERHASH) != 0;
	bool bUserHashOffered =
		((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) != 0) &&
		((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) != 0);

	if ( (pAuth->Algorithm != pChallenge->Algorithm) ||
		!__xhttpViewEqual(pAuth->Realm, pChallenge->Realm) ||
		!__xhttpViewEqual(pAuth->Nonce, pChallenge->Nonce) ||
		!__xhttpViewEqual(
			pAuth->Uri, pVerification->RequestTarget
		) || (bChallengeOpaque != bAuthOpaque) ||
		(bChallengeOpaque && !__xhttpViewEqual(
			pAuth->Opaque, pChallenge->Opaque
		)) ) {
		return false;
	}
	if ( ((pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) != 0) &&
		((pAuth->Flags &
		 XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT) == 0) ) {
		return false;
	}
	if ( ((pAuth->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_QOP_AUTH) == 0)) ||
		((pAuth->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) == 0)) ) {
		return false;
	}
	if ( (bUserHash && !bUserHashOffered) ||
		(((pVerification->Flags &
		   XHTTP_DIGEST_VERIFY_REQUIRE_USERHASH) != 0) &&
		 !bUserHash) ||
		(((pAuth->Flags &
		   XHTTP_DIGEST_AUTH_USERNAME_EXTENDED) != 0) &&
		 ((pChallenge->Flags &
		   XHTTP_DIGEST_CHALLENGE_UTF8) == 0)) ) {
		return false;
	}
	return true;
}



/* 重新计算客户端 request-digest，并用常量时间比较线路证明。 */
static xhttpdigestverifycheck __xrtHttpDigestProofVerifyPrepared(
	const xrt_http_digest_verify_prepared* pPrepared
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	xhttpdigestproof Proof = {
		pAuth->Algorithm,
		pAuth->Qop,
		pAuth->NonceCount,
		pVerification->Secret,
		pAuth->Nonce,
		pAuth->Cnonce,
		pAuth->Uri,
		pVerification->EntityHash
	};
	char Expected[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestverifycheck Check;

	memset(Expected, 0, sizeof(Expected));
	if ( !__xrtHttpDigestVerifyChallenge(pPrepared) ) {
		return XHTTP_DIGEST_VERIFY_INVALID;
	}
	if ( !xrtHttpDigestRequest(
		&Proof,
		pVerification->Method,
		Expected,
		sizeof(Expected),
		&iSize
	) ) {
		xrtSecureZero(Expected, sizeof(Expected));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	Check = xrtHttpDigestEqual(
		(xstrview){ Expected, iSize }, pAuth->Response
	) ? XHTTP_DIGEST_VERIFY_VALID : XHTTP_DIGEST_VERIFY_INVALID;
	xrtSecureZero(Expected, sizeof(Expected));
	return Check;
}



/* 验证可选签发时间输出不会覆盖任何仍需读取的输入。 */
static bool __xrtHttpDigestVerifyIssuedValid(
	const xhttpdigestverification* pInput,
	const xrt_http_digest_verify_prepared* pPrepared,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64* pIssuedSeconds
)
{
	const xhttpdigestverification* pVerification =
		&pPrepared->Verification;
	const xhttpdigestauth* pAuth = &pPrepared->Auth;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	xstrview Views[18];

	if ( pIssuedSeconds == NULL ) {
		return true;
	}
	Views[0] = pVerification->Secret;
	Views[1] = pVerification->Method;
	Views[2] = pVerification->RequestTarget;
	Views[3] = pVerification->EntityHash;
	Views[4] = pAuth->Username;
	Views[5] = pAuth->UsernameLanguage;
	Views[6] = pAuth->Realm;
	Views[7] = pAuth->Nonce;
	Views[8] = pAuth->Uri;
	Views[9] = pAuth->Cnonce;
	Views[10] = pAuth->Response;
	Views[11] = pAuth->Opaque;
	Views[12] = pAuth->AlgorithmName;
	Views[13] = pChallenge->Realm;
	Views[14] = pChallenge->Domain;
	Views[15] = pChallenge->Nonce;
	Views[16] = pChallenge->Opaque;
	Views[17] = pChallenge->AlgorithmName;
	if ( !xrtMemRangeValid(NonceKey.Data, NonceKey.Size) ||
		!xrtMemRangeValid(NonceContext.Data, NonceContext.Size) ||
		!xrtMemRangeValid(pIssuedSeconds, sizeof(*pIssuedSeconds)) ||
		xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput, sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput->Auth, sizeof(*pInput->Auth)
		) || xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			pInput->Challenge, sizeof(*pInput->Challenge)
		) || xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			NonceKey.Data, NonceKey.Size
		) || xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			NonceContext.Data, NonceContext.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( xrtMemRangesOverlap(
			pIssuedSeconds, sizeof(*pIssuedSeconds),
			Views[i].Data, Views[i].Size
		) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
	}
	return true;
}



/* 验证 request-digest，nonce 生命周期由调用方单独处理。 */
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestProofVerify(
	const xhttpdigestverification* pVerification
)
{
	xrt_http_digest_verify_prepared Prepared;
	xhttpdigestverifycheck Check;

	memset(&Prepared, 0, sizeof(Prepared));
	if ( !__xrtHttpDigestVerifyPrepare(
		pVerification, &Prepared
	) ) {
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	Check = __xrtHttpDigestProofVerifyPrepared(&Prepared);
	xrtSecureZero(&Prepared, sizeof(Prepared));
	return Check;
}



/* 组合验证无状态 nonce 与 request-digest，并保持 stale 结果不可伪造。 */
XHTTP_POLICY_API xhttpdigestverifycheck xrtHttpDigestVerify(
	const xhttpdigestverification* pVerification,
	xbytesview NonceKey,
	xbytesview NonceContext,
	int64 iNowSeconds,
	int64 iLifetimeSeconds,
	int64 iFutureSkewSeconds,
	int64* pIssuedSeconds
)
{
	xrt_http_digest_verify_prepared Prepared;
	xhttpdigestnoncecheck NonceCheck;
	xhttpdigestverifycheck ProofCheck;
	int64 iIssuedSeconds;

	memset(&Prepared, 0, sizeof(Prepared));
	if ( !__xrtHttpDigestVerifyPrepare(
		pVerification, &Prepared
	) || !__xrtHttpDigestVerifyIssuedValid(
		pVerification,
		&Prepared,
		NonceKey,
		NonceContext,
		pIssuedSeconds
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	NonceCheck = xrtHttpDigestNonceVerify(
		Prepared.Auth.Nonce,
		NonceKey,
		NonceContext,
		iNowSeconds,
		iLifetimeSeconds,
		iFutureSkewSeconds,
		&iIssuedSeconds
	);
	if ( NonceCheck == XHTTP_DIGEST_NONCE_ERROR ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_ERROR;
	}
	if ( NonceCheck == XHTTP_DIGEST_NONCE_INVALID ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return XHTTP_DIGEST_VERIFY_INVALID;
	}
	ProofCheck = __xrtHttpDigestProofVerifyPrepared(&Prepared);
	xrtSecureZero(&Prepared, sizeof(Prepared));
	if ( ProofCheck != XHTTP_DIGEST_VERIFY_VALID ) {
		return ProofCheck;
	}
	if ( pIssuedSeconds != NULL ) {
		memcpy(
			pIssuedSeconds,
			&iIssuedSeconds,
			sizeof(iIssuedSeconds)
		);
	}
	return NonceCheck == XHTTP_DIGEST_NONCE_STALE ?
		XHTTP_DIGEST_VERIFY_STALE : XHTTP_DIGEST_VERIFY_VALID;
}



#undef XRT_HTTP_DIGEST_VERIFY_VALID_FLAGS

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_client.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_CLIENT)

#define XRT_HTTP_DIGEST_POLICY_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_POLICY_PREFER_AUTH_INT | \
	 (uint32)XHTTP_DIGEST_POLICY_PLAIN_USERNAME | \
	 (uint32)XHTTP_DIGEST_POLICY_REQUIRE_USERHASH | \
	 (uint32)XHTTP_DIGEST_POLICY_REQUIRE_UTF8)

#define XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS \
	((uint32)XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED)



/* 把算法枚举映射到客户端策略位。 */
static uint32 __xrtHttpDigestAlgorithmMask(
	xhttpdigestalgorithm Algorithm
)
{
	switch ( Algorithm ) {
		case XHTTP_DIGEST_ALGORITHM_MD5:
			return XHTTP_DIGEST_ALGORITHMS_MD5;
		case XHTTP_DIGEST_ALGORITHM_MD5_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_MD5_SESSION;
		case XHTTP_DIGEST_ALGORITHM_SHA256:
			return XHTTP_DIGEST_ALGORITHMS_SHA256;
		case XHTTP_DIGEST_ALGORITHM_SHA256_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_SHA256_SESSION;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256:
			return XHTTP_DIGEST_ALGORITHMS_SHA512_256;
		case XHTTP_DIGEST_ALGORITHM_SHA512_256_SESSION:
			return XHTTP_DIGEST_ALGORITHMS_SHA512_256_SESSION;
		default:
			return 0;
	}
}



/* 把 qop 枚举映射到客户端策略位。 */
static uint32 __xrtHttpDigestQopMask(xhttpdigestqop Qop)
{
	if ( Qop == XHTTP_DIGEST_QOP_AUTH ) {
		return XHTTP_DIGEST_QOPS_AUTH;
	}
	if ( Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		return XHTTP_DIGEST_QOPS_AUTH_INT;
	}
	return 0;
}



/* 使用协议写出器的查询模式验证 challenge 结构。 */
static bool __xrtHttpDigestClientChallengeValid(
	const xhttpdigestchallenge* pChallenge
)
{
	size_t iSize;

	return xrtHttpDigestChallengeWrite(
		pChallenge, NULL, 0, &iSize
	);
}



/* 验证协商输出不会覆盖 challenge、策略或其借用值。 */
static bool __xrtHttpDigestChoiceOutputValid(
	const xhttpdigestchallenge* pInput,
	const xhttpdigestchallenge* pChallenge,
	const xhttpdigestpolicy* pPolicy,
	xhttpdigestchoice* pChoice
)
{
	xstrview Views[5];

	if ( !xrtMemRangeValid(pChoice, sizeof(*pChoice)) ||
		xrtMemRangesOverlap(
			pChoice, sizeof(*pChoice), pInput, sizeof(*pInput)
		) || ((pPolicy != NULL) && (!xrtMemRangeValid(
			pPolicy, sizeof(*pPolicy)
		) || xrtMemRangesOverlap(
			pChoice, sizeof(*pChoice), pPolicy, sizeof(*pPolicy)
		))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Views[0] = pChallenge->Realm;
	Views[1] = pChallenge->Domain;
	Views[2] = pChallenge->Nonce;
	Views[3] = pChallenge->Opaque;
	Views[4] = pChallenge->AlgorithmName;
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( xrtMemRangesOverlap(
			pChoice, sizeof(*pChoice), Views[i].Data, Views[i].Size
		) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
	}
	return true;
}



/* 复制并校验本地协商策略。 */
static bool __xrtHttpDigestPolicyPrepare(
	const xhttpdigestpolicy* pInput,
	xhttpdigestpolicy* pPolicy
)
{
	if ( pInput == NULL ) {
		xrtHttpDigestPolicyInit(pPolicy);
		return true;
	}
	memcpy(pPolicy, pInput, sizeof(*pPolicy));
	if ( ((pPolicy->Flags &
		~XRT_HTTP_DIGEST_POLICY_VALID_FLAGS) != 0) ||
		((pPolicy->Algorithms &
		 ~XHTTP_DIGEST_ALGORITHMS_ALL) != 0) ||
		((pPolicy->Qops & ~XHTTP_DIGEST_QOPS_ALL) != 0) ||
		(pPolicy->Algorithms == 0) ||
		(pPolicy->Qops == 0) ||
		(((pPolicy->Flags &
		  XHTTP_DIGEST_POLICY_PLAIN_USERNAME) != 0) &&
		 ((pPolicy->Flags &
		  XHTTP_DIGEST_POLICY_REQUIRE_USERHASH) != 0)) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 初始化不启用遗留 MD5 的现代客户端策略。 */
XHTTP_POLICY_API void xrtHttpDigestPolicyInit(xhttpdigestpolicy* pPolicy)
{
	xhttpdigestpolicy Policy = {
		0,
		XHTTP_DIGEST_ALGORITHMS_SHA2,
		XHTTP_DIGEST_QOPS_ALL
	};

	if ( !xrtMemRangeValid(pPolicy, sizeof(*pPolicy)) ) {
		return;
	}
	memcpy(pPolicy, &Policy, sizeof(Policy));
}



/* 按服务器顺序之外的单项本地策略判断 challenge。 */
XHTTP_POLICY_API xhttpdigestchoosecheck xrtHttpDigestChallengeChoose(
	const xhttpdigestchallenge* pInput,
	const xhttpdigestpolicy* pPolicyInput,
	xhttpdigestchoice* pChoice
)
{
	xhttpdigestchallenge Challenge;
	xhttpdigestpolicy Policy;
	xhttpdigestchoice Choice = { 0 };
	uint32 iOffered;
	uint32 iAllowed;
	bool bUserHash;

	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	memcpy(&Challenge, pInput, sizeof(Challenge));
	if ( !__xrtHttpDigestChoiceOutputValid(
		pInput, &Challenge, pPolicyInput, pChoice
	) ) {
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	memcpy(pChoice, &Choice, sizeof(Choice));
	if ( !__xrtHttpDigestPolicyPrepare(
		pPolicyInput, &Policy
	) || !__xrtHttpDigestClientChallengeValid(&Challenge) ) {
		return XHTTP_DIGEST_CHOOSE_ERROR;
	}
	if ( ((__xrtHttpDigestAlgorithmMask(Challenge.Algorithm) &
		Policy.Algorithms) == 0) ||
		!xrtHttpDigestAlgorithmSupported(Challenge.Algorithm) ||
		(((Policy.Flags & XHTTP_DIGEST_POLICY_REQUIRE_UTF8) != 0) &&
		 ((Challenge.Flags & XHTTP_DIGEST_CHALLENGE_UTF8) == 0)) ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	iOffered = 0;
	if ( (Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH) != 0 ) {
		iOffered |= XHTTP_DIGEST_QOPS_AUTH;
	}
	if ( (Challenge.Flags & XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) != 0 ) {
		iOffered |= XHTTP_DIGEST_QOPS_AUTH_INT;
	}
	iAllowed = iOffered & Policy.Qops;
	if ( iAllowed == 0 ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	bUserHash = ((Challenge.Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) != 0) &&
		((Challenge.Flags &
		XHTTP_DIGEST_CHALLENGE_USERHASH) != 0);
	if ( ((Policy.Flags &
		XHTTP_DIGEST_POLICY_REQUIRE_USERHASH) != 0) && !bUserHash ) {
		return XHTTP_DIGEST_CHOOSE_REJECTED;
	}
	Choice.Algorithm = Challenge.Algorithm;
	if ( ((Policy.Flags &
		XHTTP_DIGEST_POLICY_PREFER_AUTH_INT) != 0) &&
		((iAllowed & XHTTP_DIGEST_QOPS_AUTH_INT) != 0) ) {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	} else if ( (iAllowed & XHTTP_DIGEST_QOPS_AUTH) != 0 ) {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH;
	} else {
		Choice.Qop = XHTTP_DIGEST_QOP_AUTH_INT;
	}
	Choice.UserHash = bUserHash && ((Policy.Flags &
		XHTTP_DIGEST_POLICY_PLAIN_USERNAME) == 0);
	memcpy(pChoice, &Choice, sizeof(Choice));
	return XHTTP_DIGEST_CHOOSE_ACCEPTED;
}



/* 工作副本允许公开描述符和协议对象位于未对齐存储。 */
typedef struct xrt_http_digest_client_prepared {
	xhttpdigestclientauth Input;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
} xrt_http_digest_client_prepared;



/* 判断用户名是否含有会使 H(username:realm:password) 歧义的冒号。 */
static bool __xrtHttpDigestUsernameValid(xstrview Username)
{
	if ( Username.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Username.Size; i++ ) {
		if ( Username.Data[i] == ':' ) {
			return false;
		}
	}
	return true;
}



/* 校验凭据输出描述符及其与全部输入区间的别名关系。 */
static bool __xrtHttpDigestClientOutputValid(
	const xhttpdigestclientauth* pInput,
	const xrt_http_digest_client_prepared* pPrepared,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pClient = &pPrepared->Input;
	xstrview Views[12];

	if ( !xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		!xrtMemRangeValid(pAuth, sizeof(*pAuth)) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtMemRangeValid(
			pOutput, iCapacity
		)) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pAuth, sizeof(*pAuth)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pInput, sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pAuth, sizeof(*pAuth), pInput, sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || xrtMemRangesOverlap(
			pAuth, sizeof(*pAuth), pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pClient->Choice,
			sizeof(*pClient->Choice)
		) || xrtMemRangesOverlap(
			pAuth, sizeof(*pAuth), pClient->Choice,
			sizeof(*pClient->Choice)
		) || ((pOutput != NULL) && (xrtMemRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pAuth, sizeof(*pAuth)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pInput, sizeof(*pInput)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pClient->Challenge,
			sizeof(*pClient->Challenge)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pClient->Choice,
			sizeof(*pClient->Choice)
		))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Views[0] = pClient->Username;
	Views[1] = pClient->UsernameLanguage;
	Views[2] = pClient->Secret;
	Views[3] = pClient->Method;
	Views[4] = pClient->RequestTarget;
	Views[5] = pClient->Cnonce;
	Views[6] = pClient->EntityHash;
	Views[7] = pPrepared->Challenge.Realm;
	Views[8] = pPrepared->Challenge.Domain;
	Views[9] = pPrepared->Challenge.Nonce;
	Views[10] = pPrepared->Challenge.Opaque;
	Views[11] = pPrepared->Challenge.AlgorithmName;
	for ( size_t i = 0;
		i < (sizeof(Views) / sizeof(Views[0]));
		i++ ) {
		if ( !__xhttpViewValid(Views[i]) ||
			xrtMemRangesOverlap(
				pSize, sizeof(*pSize), Views[i].Data, Views[i].Size
			) || xrtMemRangesOverlap(
				pAuth, sizeof(*pAuth), Views[i].Data, Views[i].Size
			) || ((pOutput != NULL) && xrtMemRangesOverlap(
				pOutput, iCapacity, Views[i].Data, Views[i].Size
			)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
	}
	return true;
}



/* 复制并验证客户端凭据输入的固定结构和语义关系。 */
static bool __xrtHttpDigestClientPrepare(
	const xhttpdigestclientauth* pInput,
	xrt_http_digest_client_prepared* pPrepared
)
{
	const xhttpdigestclientauth* pClient;
	const xhttpdigestchallenge* pChallenge;
	const xhttpdigestchoice* pChoice;
	xstrview Views[7];
	uint32 iChoiceQop;
	size_t iDigest;

	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&pPrepared->Input, pInput, sizeof(pPrepared->Input));
	if ( !xrtMemRangeValid(
		pPrepared->Input.Challenge, sizeof(pPrepared->Challenge)
	) || !xrtMemRangeValid(
		pPrepared->Input.Choice, sizeof(pPrepared->Choice)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(
		&pPrepared->Challenge,
		pPrepared->Input.Challenge,
		sizeof(pPrepared->Challenge)
	);
	memcpy(
		&pPrepared->Choice,
		pPrepared->Input.Choice,
		sizeof(pPrepared->Choice)
	);
	pClient = &pPrepared->Input;
	pChallenge = &pPrepared->Challenge;
	pChoice = &pPrepared->Choice;
	Views[0] = pClient->Username;
	Views[1] = pClient->UsernameLanguage;
	Views[2] = pClient->Secret;
	Views[3] = pClient->Method;
	Views[4] = pClient->RequestTarget;
	Views[5] = pClient->Cnonce;
	Views[6] = pClient->EntityHash;
	for ( size_t i = 0; i < (sizeof(Views) / sizeof(Views[0])); i++ ) {
		if ( !__xhttpViewValid(Views[i]) ) {
			return false;
		}
	}
	if ( !__xrtHttpDigestClientChallengeValid(pChallenge) ||
		!xrtHttpDigestAlgorithmSupported(pChoice->Algorithm) ) {
		if ( xrtHttpDigestSize(pChoice->Algorithm) == 0 ) {
			xrtSetErrorKind(XERR_VALUE);
		} else if ( !xrtHttpDigestAlgorithmSupported(
			pChoice->Algorithm
		) ) {
			xrtSetErrorKind(XERR_UNSUPPORTED);
		}
		return false;
	}
	iChoiceQop = __xrtHttpDigestQopMask(pChoice->Qop);
	iDigest = xrtHttpDigestSize(pChoice->Algorithm) * 2u;
	if ( ((pClient->Flags &
		~XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS) != 0) ||
		(pChoice->Algorithm != pChallenge->Algorithm) ||
		(iChoiceQop == 0) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_QOP_AUTH) == 0)) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_QOP_AUTH_INT) == 0)) ||
		(pChoice->UserHash && ((((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_HAS_USERHASH) == 0) ||
		 ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_USERHASH) == 0)))) ||
		(((pClient->Flags &
		  XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) != 0) &&
		 (pChoice->UserHash || ((pChallenge->Flags &
		  XHTTP_DIGEST_CHALLENGE_UTF8) == 0))) ||
		(((pClient->Flags &
		  XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) == 0) &&
		 (pClient->UsernameLanguage.Size != 0)) ||
		!__xrtHttpDigestUsernameValid(pClient->Username) ||
		(pClient->Secret.Size != iDigest) ||
		!xrtHttpBridgeDigestHexViewValid(pClient->Secret) ||
		(pClient->Method.Size == 0) ||
		(pClient->RequestTarget.Size == 0) ||
		(pClient->Cnonce.Size == 0) ||
		(pClient->NonceCount == 0) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH) &&
		 (pClient->EntityHash.Size != 0)) ||
		((pChoice->Qop == XHTTP_DIGEST_QOP_AUTH_INT) &&
		 ((pClient->EntityHash.Size != iDigest) ||
		  !xrtHttpBridgeDigestHexViewValid(pClient->EntityHash))) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 从 challenge、选择结果和输入视图组装尚未计算摘要的凭据。 */
static void __xrtHttpDigestClientAuthInit(
	const xrt_http_digest_client_prepared* pPrepared,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pInput = &pPrepared->Input;
	const xhttpdigestchallenge* pChallenge = &pPrepared->Challenge;
	const xhttpdigestchoice* pChoice = &pPrepared->Choice;
	xhttpdigestauth Auth = { 0 };

	Auth.Algorithm = pChoice->Algorithm;
	Auth.Qop = pChoice->Qop;
	Auth.NonceCount = pInput->NonceCount;
	Auth.Realm = pChallenge->Realm;
	Auth.Nonce = pChallenge->Nonce;
	Auth.Uri = pInput->RequestTarget;
	Auth.Cnonce = pInput->Cnonce;
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT) != 0 ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_ALGORITHM_EXPLICIT;
	}
	if ( (pChallenge->Flags &
		XHTTP_DIGEST_CHALLENGE_HAS_OPAQUE) != 0 ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_HAS_OPAQUE;
		Auth.Opaque = pChallenge->Opaque;
	}
	if ( pChoice->UserHash ) {
		Auth.Flags |= XHTTP_DIGEST_AUTH_HAS_USERHASH |
			XHTTP_DIGEST_AUTH_USERHASH;
	} else {
		Auth.Username = pInput->Username;
		if ( (pInput->Flags &
			XHTTP_DIGEST_CLIENT_USERNAME_EXTENDED) != 0 ) {
			Auth.Flags |= XHTTP_DIGEST_AUTH_USERNAME_EXTENDED;
			Auth.UsernameLanguage = pInput->UsernameLanguage;
		}
	}
	*pAuth = Auth;
}



/* 计算用户名摘要和 request-digest，并发布最终凭据。 */
static bool __xrtHttpDigestClientAuthWrite(
	const xrt_http_digest_client_prepared* pPrepared,
	void* pOutput,
	size_t iRequired,
	xhttpdigestauth* pAuth
)
{
	const xhttpdigestclientauth* pInput = &pPrepared->Input;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	bytes pNext = (bytes)pOutput;
	size_t iWritten;
	size_t iDigest = xrtHttpDigestSize(
		pPrepared->Choice.Algorithm
	) * 2u;

	__xrtHttpDigestClientAuthInit(pPrepared, &Auth);
	if ( pPrepared->Choice.UserHash ) {
		if ( !xrtHttpDigestUserHash(
			pPrepared->Choice.Algorithm,
			pInput->Username,
			pPrepared->Challenge.Realm,
			pNext,
			iDigest,
			&iWritten
		) ) {
			goto fail;
		}
		Auth.Username = (xstrview){ (cstr)pNext, iWritten };
		pNext += iWritten;
	}
	Proof = (xhttpdigestproof){
		pPrepared->Choice.Algorithm,
		pPrepared->Choice.Qop,
		pInput->NonceCount,
		pInput->Secret,
		pPrepared->Challenge.Nonce,
		pInput->Cnonce,
		pInput->RequestTarget,
		pInput->EntityHash
	};
	if ( !xrtHttpDigestRequest(
		&Proof,
		pInput->Method,
		pNext,
		iDigest,
		&iWritten
	) ) {
		goto fail;
	}
	Auth.Response = (xstrview){ (cstr)pNext, iWritten };
	if ( !xrtHttpDigestAuthWrite(
		&Auth, NULL, 0, &iWritten
	) ) {
		goto fail;
	}
	memcpy(pAuth, &Auth, sizeof(Auth));
	return true;

fail:
	xrtSecureZero(pOutput, iRequired);
	return false;
}



/* 构造一份不保存密码、不分配内存的 Digest 客户端凭据。 */
XHTTP_POLICY_API bool xrtHttpDigestClientAuth(
	const xhttpdigestclientauth* pInput,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pAuth
)
{
	xrt_http_digest_client_prepared Prepared;
	xhttpdigestauth Auth;
	size_t iDigest;
	size_t iRequired;

	memset(&Prepared, 0, sizeof(Prepared));
	memset(&Auth, 0, sizeof(Auth));
	if ( !__xrtHttpDigestClientPrepare(
		pInput, &Prepared
	) || !__xrtHttpDigestClientOutputValid(
		pInput, &Prepared, pOutput, iCapacity, pSize, pAuth
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	iDigest = xrtHttpDigestSize(Prepared.Choice.Algorithm) * 2u;
	iRequired = iDigest + (Prepared.Choice.UserHash ? iDigest : 0u);
	__xrtHttpDigestClientAuthInit(&Prepared, &Auth);
	memcpy(pAuth, &Auth, sizeof(Auth));
	if ( pOutput == NULL ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return true;
	}
	if ( iCapacity < iRequired ) {
		memcpy(pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	if ( !__xrtHttpDigestClientAuthWrite(
		&Prepared, pOutput, iRequired, &Auth
	) ) {
		xrtSecureZero(&Prepared, sizeof(Prepared));
		return false;
	}
	memcpy(pSize, &iRequired, sizeof(iRequired));
	memcpy(pAuth, &Auth, sizeof(Auth));
	xrtSecureZero(&Prepared, sizeof(Prepared));
	return true;
}



/* 复制并验证响应证明描述符和两个协议对象。 */
static bool __xrtHttpDigestInfoVerificationPrepare(
	const xhttpdigestinfoverification* pInput,
	xhttpdigestinfoverification* pVerification,
	xhttpdigestinfo* pInfo,
	xhttpdigestproof* pProof
)
{
	size_t iSize;

	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pVerification, pInput, sizeof(*pVerification));
	if ( !xrtMemRangeValid(
		pVerification->Info, sizeof(*pInfo)
	) || !xrtMemRangeValid(
		pVerification->Proof, sizeof(*pProof)
	) || !__xhttpViewValid(pVerification->ResponseEntityHash) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pInfo, pVerification->Info, sizeof(*pInfo));
	memcpy(pProof, pVerification->Proof, sizeof(*pProof));
	if ( !xrtHttpDigestInfoWrite(
		pInfo, NULL, 0, &iSize
	) ) {
		return false;
	}
	if ( pProof->Qop == XHTTP_DIGEST_QOP_AUTH_INT ) {
		pProof->EntityHash = pVerification->ResponseEntityHash;
	} else if ( pVerification->ResponseEntityHash.Size != 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 验证服务器响应证明，并把 nextnonce 的采用留给更高层状态机。 */
XHTTP_POLICY_API xhttpdigestinfocheck xrtHttpDigestInfoVerify(
	const xhttpdigestinfoverification* pInput
)
{
	xhttpdigestinfoverification Verification;
	xhttpdigestinfo Info;
	xhttpdigestproof Proof;
	char Expected[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSize;
	xhttpdigestinfocheck Check;

	memset(&Verification, 0, sizeof(Verification));
	memset(&Info, 0, sizeof(Info));
	memset(&Proof, 0, sizeof(Proof));
	memset(Expected, 0, sizeof(Expected));
	if ( !__xrtHttpDigestInfoVerificationPrepare(
		pInput, &Verification, &Info, &Proof
	) || !xrtHttpDigestRspAuth(
		&Proof, Expected, sizeof(Expected), &iSize
	) ) {
		xrtSecureZero(&Verification, sizeof(Verification));
		xrtSecureZero(&Info, sizeof(Info));
		xrtSecureZero(&Proof, sizeof(Proof));
		xrtSecureZero(Expected, sizeof(Expected));
		return XHTTP_DIGEST_INFO_ERROR;
	}
	Check = ((Info.Flags & XHTTP_DIGEST_INFO_HAS_RESPONSE) != 0) &&
		(Info.Algorithm == Proof.Algorithm) &&
		(Info.Qop == Proof.Qop) &&
		(Info.NonceCount == Proof.NonceCount) &&
		__xhttpViewEqual(Info.Cnonce, Proof.Cnonce) &&
		xrtHttpDigestEqual(
			Info.Response, (xstrview){ Expected, iSize }
		) ? XHTTP_DIGEST_INFO_VALID : XHTTP_DIGEST_INFO_INVALID;
	xrtSecureZero(&Verification, sizeof(Verification));
	xrtSecureZero(&Info, sizeof(Info));
	xrtSecureZero(&Proof, sizeof(Proof));
	xrtSecureZero(Expected, sizeof(Expected));
	return Check;
}



#undef XRT_HTTP_DIGEST_POLICY_VALID_FLAGS
#undef XRT_HTTP_DIGEST_CLIENT_AUTH_VALID_FLAGS

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_session.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 会话状态只在所属会话锁内推进计数，其余字段创建后保持不变。 */
typedef struct xrt_http_digest_session_state {
	volatile int32 RefCount;
	size_t AllocationSize;
	uint32 Flags;
	uint32 NextCount;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xstrview Username;
	xstrview UsernameLanguage;
	xstrview Secret;
	xstrview Cnonce;
	uint8 Data[];
} xrt_http_digest_session_state;



/* 会话用一把短临界区互斥锁保护当前状态指针和 nonce-count。 */
struct xhttpdigestsession {
	volatile int32 RefCount;
	xmutex Lock;
	xrt_http_digest_session_state* State;
};



/* Exchange 拥有本次请求目标、实体摘要和生成的凭据字节。 */
struct xhttpdigestexchange {
	volatile int32 RefCount;
	size_t AllocationSize;
	xhttpdigestsession* Session;
	xrt_http_digest_session_state* State;
	xhttpdigestauth Auth;
	xhttpdigestproof Proof;
	uint8 Data[];
};



/* 校验写出区不会破坏会话、Exchange 及其保留的认证状态。 */
bool __xrtHttpDigestSessionOutputValid(
	const xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const void* pOutput,
	size_t iSize
)
{
	const xrt_http_digest_session_state* pState;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pOutput, iSize) ||
		xrtMemRangesOverlap(
			pSession, sizeof(*pSession), pOutput, iSize
		) ) {
		return false;
	}
	if ( pExchange == NULL ) {
		return true;
	}
	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ||
		(pExchange->Session != pSession) ||
		(pExchange->AllocationSize < sizeof(*pExchange)) ||
		!xrtMemRangeValid(
			pExchange, pExchange->AllocationSize
		) ) {
		return false;
	}
	pState = pExchange->State;
	if ( !xrtMemRangeValid(pState, sizeof(*pState)) ||
		(pState->AllocationSize < sizeof(*pState)) ||
		!xrtMemRangeValid(pState, pState->AllocationSize) ) {
		return false;
	}
	return !xrtMemRangesOverlap(
		pExchange,
		pExchange->AllocationSize,
		pOutput,
		iSize
	) && !xrtMemRangesOverlap(
		pState,
		pState->AllocationSize,
		pOutput,
		iSize
	);
}



/* 增加内部状态引用，避免会话更新破坏正在处理的请求。 */
static bool __xrtHttpDigestSessionStateRetain(
	xrt_http_digest_session_state* pState
)
{
	if ( xrtRefRetain(&pState->RefCount) < 0 ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 释放状态最后一个引用，并可靠清除其中的长期 Secret。 */
static void __xrtHttpDigestSessionStateRelease(
	xrt_http_digest_session_state* pState
)
{
	size_t iAllocation;

	if ( (pState == NULL) ||
		(xrtRefRelease(&pState->RefCount) != 0) ) {
		return;
	}
	iAllocation = pState->AllocationSize;
	xrtSecureZero(pState, iAllocation);
	xrtFree(pState);
}



/* 把一个可选视图复制到对象尾部，并返回新的自有视图。 */
static xstrview __xrtHttpDigestSessionCopy(
	bytes* ppOutput,
	xstrview Value
)
{
	xstrview Result = { NULL, 0 };

	if ( Value.Size == 0 ) {
		return Result;
	}
	memcpy(*ppOutput, Value.Data, Value.Size);
	Result = (xstrview){ (cstr)*ppOutput, Value.Size };
	*ppOutput += Value.Size;
	return Result;
}



/* 复制未对齐配置和它引用的两个公开协议描述符。 */
static bool __xrtHttpDigestSessionConfigPrepare(
	const xhttpdigestsessionconfig* pInput,
	xhttpdigestsessionconfig* pConfig,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pConfig, pInput, sizeof(*pConfig));
	if ( !xrtMemRangeValid(
		pConfig->Challenge, sizeof(*pChallenge)
	) || !xrtMemRangeValid(
		pConfig->Choice, sizeof(*pChoice)
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pChallenge, pConfig->Challenge, sizeof(*pChallenge));
	memcpy(pChoice, pConfig->Choice, sizeof(*pChoice));
	pConfig->Challenge = pChallenge;
	pConfig->Choice = pChoice;
	return true;
}



/*
	先复用无状态构建器完成全部语义校验，再按实际字段长度创建状态。
	这样状态层不会维护第二套 Digest 协议规则。
*/
static xrt_http_digest_session_state* __xrtHttpDigestSessionStateCreate(
	const xhttpdigestsessionconfig* pInput
)
{
	xhttpdigestsessionconfig Config;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xhttpdigestclientauth Client;
	xhttpdigestauth Auth;
	xrt_http_digest_session_state* pState;
	xstrview Views[9];
	char EntityHash[XHTTP_DIGEST_MAX_TEXT_SIZE];
	bytes pNext;
	size_t iAllocation = sizeof(*pState);
	size_t iDigest;
	size_t iSize;

	memset(&Config, 0, sizeof(Config));
	memset(&Challenge, 0, sizeof(Challenge));
	memset(&Choice, 0, sizeof(Choice));
	memset(&Auth, 0, sizeof(Auth));
	memset(EntityHash, '0', sizeof(EntityHash));
	if ( !__xrtHttpDigestSessionConfigPrepare(
		pInput, &Config, &Challenge, &Choice
	) ) {
		return NULL;
	}
	iDigest = xrtHttpDigestSize(Choice.Algorithm) * 2u;
	Client = (xhttpdigestclientauth){
		Config.Flags,
		&Challenge,
		&Choice,
		Config.Username,
		Config.UsernameLanguage,
		Config.Secret,
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("/"),
		Config.Cnonce,
		Choice.Qop == XHTTP_DIGEST_QOP_AUTH_INT ?
			(xstrview){ EntityHash, iDigest } :
			(xstrview){ NULL, 0 },
		1u
	};
	if ( !xrtHttpDigestClientAuth(
		&Client, NULL, 0, &iSize, &Auth
	) ) {
		xrtSecureZero(EntityHash, sizeof(EntityHash));
		return NULL;
	}
	Views[0] = Challenge.Realm;
	Views[1] = Challenge.Domain;
	Views[2] = Challenge.Nonce;
	Views[3] = Challenge.Opaque;
	Views[4] = Challenge.AlgorithmName;
	Views[5] = Config.Username;
	Views[6] = Config.UsernameLanguage;
	Views[7] = Config.Secret;
	Views[8] = Config.Cnonce;
	for ( size_t i = 0;
		i < (sizeof(Views) / sizeof(Views[0]));
		i++ ) {
		if ( !__xhttpSizeAdd(&iAllocation, Views[i].Size) ) {
			xrtSecureZero(EntityHash, sizeof(EntityHash));
			return NULL;
		}
	}
	pState = (xrt_http_digest_session_state*)xrtMalloc(iAllocation);
	if ( pState == NULL ) {
		xrtSecureZero(EntityHash, sizeof(EntityHash));
		return NULL;
	}
	memset(pState, 0, sizeof(*pState));
	pState->RefCount = 1;
	pState->AllocationSize = iAllocation;
	pState->Flags = Config.Flags;
	pState->NextCount = 1u;
	pState->Challenge = Challenge;
	pState->Choice = Choice;
	pNext = pState->Data;
	pState->Challenge.Realm = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Realm
	);
	pState->Challenge.Domain = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Domain
	);
	pState->Challenge.Nonce = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Nonce
	);
	pState->Challenge.Opaque = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.Opaque
	);
	pState->Challenge.AlgorithmName = __xrtHttpDigestSessionCopy(
		&pNext, Challenge.AlgorithmName
	);
	pState->Username = __xrtHttpDigestSessionCopy(
		&pNext, Config.Username
	);
	pState->UsernameLanguage = __xrtHttpDigestSessionCopy(
		&pNext, Config.UsernameLanguage
	);
	pState->Secret = __xrtHttpDigestSessionCopy(
		&pNext, Config.Secret
	);
	pState->Cnonce = __xrtHttpDigestSessionCopy(
		&pNext, Config.Cnonce
	);
	xrtSecureZero(EntityHash, sizeof(EntityHash));
	return pState;
}



/* 从已验证响应的 nextnonce 派生新状态，并把计数重置为 1。 */
static xrt_http_digest_session_state* __xrtHttpDigestSessionStateNext(
	const xrt_http_digest_session_state* pState,
	xstrview NextNonce,
	xstrview NextCnonce
)
{
	xhttpdigestchallenge Challenge = pState->Challenge;
	xhttpdigestsessionconfig Config;

	Challenge.Nonce = NextNonce;
	Config = (xhttpdigestsessionconfig){
		pState->Flags,
		&Challenge,
		&pState->Choice,
		pState->Username,
		pState->UsernameLanguage,
		pState->Secret,
		NextCnonce.Size != 0 ? NextCnonce : pState->Cnonce
	};
	return __xrtHttpDigestSessionStateCreate(&Config);
}



/* 创建拥有独立状态副本的 Digest 客户端会话。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionCreate(
	const xhttpdigestsessionconfig* pConfig
)
{
	xrt_http_digest_session_state* pState;
	xhttpdigestsession* pSession;

	pState = __xrtHttpDigestSessionStateCreate(pConfig);
	if ( pState == NULL ) {
		return NULL;
	}
	pSession = (xhttpdigestsession*)xrtMalloc(sizeof(*pSession));
	if ( pSession == NULL ) {
		__xrtHttpDigestSessionStateRelease(pState);
		return NULL;
	}
	memset(pSession, 0, sizeof(*pSession));
	pSession->RefCount = 1;
	pSession->State = pState;
	if ( !xrtMutexInit(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pState);
		xrtFree(pSession);
		return NULL;
	}
	return pSession;
}



/* 增加会话引用并返回原对象。 */
XHTTP_POLICY_API xhttpdigestsession* xrtHttpDigestSessionRetain(
	xhttpdigestsession* pSession
)
{
	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( xrtRefRetain(&pSession->RefCount) < 0 ) {
		xrtSetErrorKind(XERR_STATE);
		return NULL;
	}
	return pSession;
}



/* 释放会话最后一个引用和当前状态。 */
XHTTP_POLICY_API void xrtHttpDigestSessionRelease(
	xhttpdigestsession* pSession
)
{
	xrt_http_digest_session_state* pState;

	if ( (pSession == NULL) ||
		(xrtRefRelease(&pSession->RefCount) != 0) ) {
		return;
	}
	pState = pSession->State;
	(void)xrtMutexUnit(&pSession->Lock);
	memset(pSession, 0, sizeof(*pSession));
	xrtFree(pSession);
	__xrtHttpDigestSessionStateRelease(pState);
}



/* 先完整构造新状态，再用短临界区提交，失败不改变原状态。 */
XHTTP_POLICY_API bool xrtHttpDigestSessionUpdate(
	xhttpdigestsession* pSession,
	const xhttpdigestsessionconfig* pConfig
)
{
	xrt_http_digest_session_state* pState;
	xrt_http_digest_session_state* pOld;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pState = __xrtHttpDigestSessionStateCreate(pConfig);
	if ( pState == NULL ) {
		return false;
	}
	if ( !xrtMutexLock(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pState);
		return false;
	}
	pOld = pSession->State;
	pSession->State = pState;
	(void)xrtMutexUnlock(&pSession->Lock);
	__xrtHttpDigestSessionStateRelease(pOld);
	return true;
}



/* 释放尚未发布或已经发布的 Exchange。 */
static void __xrtHttpDigestExchangeFree(
	xhttpdigestexchange* pExchange
)
{
	xhttpdigestsession* pSession = pExchange->Session;
	xrt_http_digest_session_state* pState = pExchange->State;
	size_t iAllocation = pExchange->AllocationSize;

	xrtSecureZero(pExchange, iAllocation);
	xrtFree(pExchange);
	__xrtHttpDigestSessionStateRelease(pState);
	xrtHttpDigestSessionRelease(pSession);
}



/* 为请求分配精确大小的 Exchange，并在线性化点保留唯一 nonce-count。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestSessionAuthorize(
	xhttpdigestsession* pSession,
	xstrview Method,
	xstrview RequestTarget,
	xstrview EntityHash
)
{
	for ( ;; ) {
		xrt_http_digest_session_state* pState;
		xhttpdigestexchange* pExchange;
		xhttpdigestclientauth Client;
		xhttpdigestauth Auth;
		bytes pNext;
		size_t iAllocation = sizeof(*pExchange);
		size_t iOutput;
		size_t iWritten;
		uint32 iNonceCount;

		if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
			!__xhttpViewValid(Method) ||
			!__xhttpViewValid(RequestTarget) ||
			!__xhttpViewValid(EntityHash) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return NULL;
		}
		if ( !xrtMutexLock(&pSession->Lock) ) {
			return NULL;
		}
		pState = pSession->State;
		if ( !__xrtHttpDigestSessionStateRetain(pState) ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			return NULL;
		}
		(void)xrtMutexUnlock(&pSession->Lock);
		Client = (xhttpdigestclientauth){
			pState->Flags,
			&pState->Challenge,
			&pState->Choice,
			pState->Username,
			pState->UsernameLanguage,
			pState->Secret,
			Method,
			RequestTarget,
			pState->Cnonce,
			EntityHash,
			1u
		};
		memset(&Auth, 0, sizeof(Auth));
		if ( !xrtHttpDigestClientAuth(
			&Client, NULL, 0, &iOutput, &Auth
		) || !__xhttpSizeAdd(
			&iAllocation, RequestTarget.Size
		) || !__xhttpSizeAdd(
			&iAllocation, EntityHash.Size
		) || !__xhttpSizeAdd(
			&iAllocation, iOutput
		) ) {
			__xrtHttpDigestSessionStateRelease(pState);
			return NULL;
		}
		pExchange = (xhttpdigestexchange*)xrtMalloc(iAllocation);
		if ( pExchange == NULL ) {
			__xrtHttpDigestSessionStateRelease(pState);
			return NULL;
		}
		memset(pExchange, 0, sizeof(*pExchange));
		pExchange->RefCount = 1;
		pExchange->AllocationSize = iAllocation;
		pExchange->State = pState;
		pNext = pExchange->Data;
		Client.RequestTarget = __xrtHttpDigestSessionCopy(
			&pNext, RequestTarget
		);
		Client.EntityHash = __xrtHttpDigestSessionCopy(
			&pNext, EntityHash
		);
		if ( !xrtMutexLock(&pSession->Lock) ) {
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		if ( pSession->State != pState ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			__xrtHttpDigestExchangeFree(pExchange);
			continue;
		}
		if ( pState->NextCount == 0 ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			xrtSetErrorKind(XERR_RANGE);
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		iNonceCount = pState->NextCount;
		pState->NextCount = (iNonceCount == UINT32_MAX) ?
			0u : (iNonceCount + 1u);
		if ( xrtRefRetain(&pSession->RefCount) < 0 ) {
			(void)xrtMutexUnlock(&pSession->Lock);
			xrtSetErrorKind(XERR_STATE);
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		pExchange->Session = pSession;
		(void)xrtMutexUnlock(&pSession->Lock);
		Client.NonceCount = iNonceCount;
		if ( !xrtHttpDigestClientAuth(
			&Client, pNext, iOutput, &iWritten, &Auth
		) ) {
			__xrtHttpDigestExchangeFree(pExchange);
			return NULL;
		}
		pExchange->Auth = Auth;
		pExchange->Proof = (xhttpdigestproof){
			pState->Choice.Algorithm,
			pState->Choice.Qop,
			iNonceCount,
			pState->Secret,
			pState->Challenge.Nonce,
			pState->Cnonce,
			Client.RequestTarget,
			Client.EntityHash
		};
		return pExchange;
	}
}



/* 增加 Exchange 引用并返回原对象。 */
XHTTP_POLICY_API xhttpdigestexchange* xrtHttpDigestExchangeRetain(
	xhttpdigestexchange* pExchange
)
{
	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( xrtRefRetain(&pExchange->RefCount) < 0 ) {
		xrtSetErrorKind(XERR_STATE);
		return NULL;
	}
	return pExchange;
}



/* 释放 Exchange 最后一个引用和它持有的状态快照。 */
XHTTP_POLICY_API void xrtHttpDigestExchangeRelease(
	xhttpdigestexchange* pExchange
)
{
	if ( (pExchange == NULL) ||
		(xrtRefRelease(&pExchange->RefCount) != 0) ) {
		return;
	}
	__xrtHttpDigestExchangeFree(pExchange);
}



/* 取得不可变 Authorization 协议对象。 */
XHTTP_POLICY_API const xhttpdigestauth* xrtHttpDigestExchangeAuth(
	const xhttpdigestexchange* pExchange
)
{
	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return &pExchange->Auth;
}



/* 取得与本次请求严格绑定的响应验证 proof。 */
XHTTP_POLICY_API const xhttpdigestproof* xrtHttpDigestExchangeProof(
	const xhttpdigestexchange* pExchange
)
{
	if ( !xrtMemRangeValid(pExchange, sizeof(*pExchange)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return &pExchange->Proof;
}



/* 验证服务器证明，并只允许当前状态对应的 Exchange 推进 nextnonce。 */
XHTTP_POLICY_API xhttpdigestsessioncheck xrtHttpDigestSessionAccept(
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const xhttpdigestinfo* pInput,
	xstrview ResponseEntityHash,
	xstrview NextCnonce
)
{
	xhttpdigestinfo Info;
	xhttpdigestinfoverification Verification;
	xrt_http_digest_session_state* pNext;
	xrt_http_digest_session_state* pOld;
	xhttpdigestinfocheck Check;

	if ( !xrtMemRangeValid(pSession, sizeof(*pSession)) ||
		!xrtMemRangeValid(pExchange, sizeof(*pExchange)) ||
		!xrtMemRangeValid(pInput, sizeof(*pInput)) ||
		!__xhttpViewValid(ResponseEntityHash) ||
		!__xhttpViewValid(NextCnonce) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( pExchange->Session != pSession ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	memcpy(&Info, pInput, sizeof(Info));
	Verification = (xhttpdigestinfoverification){
		&Info,
		&pExchange->Proof,
		ResponseEntityHash
	};
	Check = xrtHttpDigestInfoVerify(&Verification);
	if ( Check == XHTTP_DIGEST_INFO_ERROR ) {
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( Check == XHTTP_DIGEST_INFO_INVALID ) {
		return XHTTP_DIGEST_SESSION_INVALID;
	}
	if ( (Info.Flags & XHTTP_DIGEST_INFO_HAS_NEXT_NONCE) == 0 ) {
		if ( NextCnonce.Size != 0 ) {
			xrtSetErrorKind(XERR_VALUE);
			return XHTTP_DIGEST_SESSION_ERROR;
		}
		return XHTTP_DIGEST_SESSION_VALID;
	}
	pNext = __xrtHttpDigestSessionStateNext(
		pExchange->State, Info.NextNonce, NextCnonce
	);
	if ( pNext == NULL ) {
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( !xrtMutexLock(&pSession->Lock) ) {
		__xrtHttpDigestSessionStateRelease(pNext);
		return XHTTP_DIGEST_SESSION_ERROR;
	}
	if ( pSession->State != pExchange->State ) {
		(void)xrtMutexUnlock(&pSession->Lock);
		__xrtHttpDigestSessionStateRelease(pNext);
		return XHTTP_DIGEST_SESSION_SUPERSEDED;
	}
	pOld = pSession->State;
	pSession->State = pNext;
	(void)xrtMutexUnlock(&pSession->Lock);
	__xrtHttpDigestSessionStateRelease(pOld);
	return XHTTP_DIGEST_SESSION_UPDATED;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_auth_digest_replay.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)




#if defined(XHTTP_FEATURE_HTTP_AUTH_DIGEST_REPLAY)

#define XRT_HTTP_DIGEST_REPLAY_GUARD UINT32_C(0x4852504C)
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS 16u
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES 1024u
#define XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME 300
#define XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX 256u
#define XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE (UINT64_MAX >> 3u)



/* 单条记录只保存不可逆键、最大 nc 和固定过期时间。 */
typedef struct xrt_http_digest_replay_entry {
	xhttpdigestreplaykey Key;
	uint32 NonceCount;
	int64 ExpiresSeconds;
	size_t HeapIndex;
	bool Occupied;
} xrt_http_digest_replay_entry;



/* 每个分片独立持有固定哈希槽、过期小根堆和策略统计。 */
typedef struct xrt_http_digest_replay_shard {
	xmutex Lock;
	xrt_http_digest_replay_entry* Slots;
	xrt_http_digest_replay_entry** Heap;
	size_t EntryCount;
	size_t HeapCount;
	size_t Capacity;
	size_t SlotCapacity;
	uint64 Accepted;
	uint64 Replayed;
	uint64 Expired;
	uint64 Full;
	uint64 Purged;
	bool LockReady;
} xrt_http_digest_replay_shard;



/* 所有槽和堆在创建时一次性分配，锁内检查路径不再分配。 */
struct xhttpdigestreplay {
	uint32 Guard;
	int64 LifetimeSeconds;
	size_t ShardCount;
	size_t TotalCapacity;
	size_t TotalSlots;
	xrt_http_digest_replay_entry* SlotStorage;
	xrt_http_digest_replay_entry** HeapStorage;
	xrt_http_digest_replay_shard Shards[];
};



static const uint8 __xrtHttpDigestReplayDomain[] =
	"xrt-http-digest-replay-v1";



/* 写出无符号 64 位大端长度，避免不同字段拼接产生歧义。 */
static void __xrtHttpDigestReplayStore64(uint8* pOutput, uint64 iValue)
{
	for ( size_t i = 0; i < 8u; i++ ) {
		pOutput[7u - i] = (uint8)(iValue >> (i * 8u));
	}
}



/* 验证重放表对象并拒绝明显失效的对象。 */
static bool __xrtHttpDigestReplayValid(const xhttpdigestreplay* pReplay)
{
	if ( (pReplay == NULL) ||
		!xrtMemRangeValid(pReplay, sizeof(*pReplay)) ||
		(pReplay->Guard != XRT_HTTP_DIGEST_REPLAY_GUARD) ||
		(pReplay->ShardCount == 0) ||
		(pReplay->ShardCount > XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 饱和增加统计，避免诊断计数在长期服务中回绕。 */
static void __xrtHttpDigestReplayCounterAdd(
	uint64* pCounter,
	uint64 iValue
)
{
	if ( *pCounter > (UINT64_MAX - iValue) ) {
		*pCounter = UINT64_MAX;
	} else {
		*pCounter += iValue;
	}
}



/* 从固定键前八字节读取均匀分布的表索引哈希。 */
static uint64 __xrtHttpDigestReplayHash(
	const xhttpdigestreplaykey* pKey
)
{
	uint64 iHash;

	memcpy(&iHash, pKey->Bytes, sizeof(iHash));
	return iHash;
}



/* 返回环形开放寻址中的下一个槽。 */
static size_t __xrtHttpDigestReplayNext(size_t iIndex, size_t iCapacity)
{
	iIndex++;
	return iIndex == iCapacity ? 0 : iIndex;
}



/* 计算某槽沿线性探测路径距离理想槽的步数。 */
static size_t __xrtHttpDigestReplayDistance(
	size_t iHome,
	size_t iIndex,
	size_t iCapacity
)
{
	return iIndex >= iHome ?
		(iIndex - iHome) : (iCapacity - iHome + iIndex);
}



/* 查找固定键，并按需返回首次空槽供新记录使用。 */
static xrt_http_digest_replay_entry* __xrtHttpDigestReplayFind(
	xrt_http_digest_replay_shard* pShard,
	const xhttpdigestreplaykey* pKey,
	xrt_http_digest_replay_entry** ppEmpty
)
{
	size_t iIndex = (size_t)(
		__xrtHttpDigestReplayHash(pKey) %
		(uint64)pShard->SlotCapacity
	);

	if ( ppEmpty != NULL ) {
		*ppEmpty = NULL;
	}
	for ( size_t i = 0; i < pShard->SlotCapacity; i++ ) {
		xrt_http_digest_replay_entry* pEntry = &pShard->Slots[iIndex];

		if ( !pEntry->Occupied ) {
			if ( ppEmpty != NULL ) {
				*ppEmpty = pEntry;
			}
			return NULL;
		}
		if ( memcmp(
			pEntry->Key.Bytes,
			pKey->Bytes,
			XHTTP_DIGEST_REPLAY_KEY_SIZE
		) == 0 ) {
			return pEntry;
		}
		iIndex = __xrtHttpDigestReplayNext(
			iIndex, pShard->SlotCapacity
		);
	}
	return NULL;
}



/* 比较过期时间和键，使堆顺序在相同时间下仍完全确定。 */
static bool __xrtHttpDigestReplayEntryLess(
	const xrt_http_digest_replay_entry* pLeft,
	const xrt_http_digest_replay_entry* pRight
)
{
	if ( pLeft->ExpiresSeconds != pRight->ExpiresSeconds ) {
		return pLeft->ExpiresSeconds < pRight->ExpiresSeconds;
	}
	return memcmp(
		pLeft->Key.Bytes,
		pRight->Key.Bytes,
		XHTTP_DIGEST_REPLAY_KEY_SIZE
	) < 0;
}



/* 向容量已经确认的分片过期堆加入一条记录。 */
static void __xrtHttpDigestReplayHeapPush(
	xrt_http_digest_replay_shard* pShard,
	xrt_http_digest_replay_entry* pEntry
)
{
	size_t iIndex = pShard->HeapCount++;

	while ( iIndex != 0 ) {
		size_t iParent = (iIndex - 1u) / 2u;
		xrt_http_digest_replay_entry* pParent = pShard->Heap[iParent];

		if ( !__xrtHttpDigestReplayEntryLess(pEntry, pParent) ) {
			break;
		}
		pShard->Heap[iIndex] = pParent;
		pParent->HeapIndex = iIndex;
		iIndex = iParent;
	}
	pShard->Heap[iIndex] = pEntry;
	pEntry->HeapIndex = iIndex;
}



/* 从过期堆删除根，但暂不修改哈希槽。 */
static xrt_http_digest_replay_entry* __xrtHttpDigestReplayHeapPop(
	xrt_http_digest_replay_shard* pShard
)
{
	xrt_http_digest_replay_entry* pRemoved = pShard->Heap[0];
	xrt_http_digest_replay_entry* pLast =
		pShard->Heap[pShard->HeapCount - 1u];
	size_t iIndex = 0;

	pShard->HeapCount--;
	if ( pShard->HeapCount != 0 ) {
		while ( true ) {
			size_t iLeft = (iIndex * 2u) + 1u;
			size_t iRight = iLeft + 1u;
			size_t iChild;
			xrt_http_digest_replay_entry* pChild;

			if ( iLeft >= pShard->HeapCount ) {
				break;
			}
			iChild = ((iRight < pShard->HeapCount) &&
				__xrtHttpDigestReplayEntryLess(
					pShard->Heap[iRight], pShard->Heap[iLeft]
				)) ? iRight : iLeft;
			pChild = pShard->Heap[iChild];
			if ( !__xrtHttpDigestReplayEntryLess(pChild, pLast) ) {
				break;
			}
			pShard->Heap[iIndex] = pChild;
			pChild->HeapIndex = iIndex;
			iIndex = iChild;
		}
		pShard->Heap[iIndex] = pLast;
		pLast->HeapIndex = iIndex;
	}
	pShard->Heap[pShard->HeapCount] = NULL;
	return pRemoved;
}



/* 后移探测簇填补删除空洞，并同步修正堆中的稳定指针。 */
static bool __xrtHttpDigestReplayRemove(
	xrt_http_digest_replay_shard* pShard,
	xrt_http_digest_replay_entry* pEntry
)
{
	uintptr_t iSlotsAddress = (uintptr_t)pShard->Slots;
	uintptr_t iEntryAddress = (uintptr_t)pEntry;
	size_t iSlotBytes;
	size_t iHole;
	size_t iScan;

	if ( (pShard->SlotCapacity == 0) ||
		(pShard->SlotCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	iSlotBytes = pShard->SlotCapacity *
		sizeof(xrt_http_digest_replay_entry);
	if ( (iSlotsAddress > (UINTPTR_MAX - iSlotBytes)) ||
		(iEntryAddress < iSlotsAddress) ||
		(iEntryAddress >= (iSlotsAddress + iSlotBytes)) ||
		(((iEntryAddress - iSlotsAddress) %
		  sizeof(xrt_http_digest_replay_entry)) != 0) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	iHole = (size_t)(
		(iEntryAddress - iSlotsAddress) /
		sizeof(xrt_http_digest_replay_entry)
	);
	pEntry = &pShard->Slots[iHole];
	if ( !pEntry->Occupied || (pShard->EntryCount == 0) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	pEntry->Occupied = false;
	iScan = __xrtHttpDigestReplayNext(iHole, pShard->SlotCapacity);
	while ( pShard->Slots[iScan].Occupied ) {
		xrt_http_digest_replay_entry* pMove = &pShard->Slots[iScan];
		size_t iHome = (size_t)(
			__xrtHttpDigestReplayHash(&pMove->Key) %
			(uint64)pShard->SlotCapacity
		);

		if ( __xrtHttpDigestReplayDistance(
			iHome, iHole, pShard->SlotCapacity
		) < __xrtHttpDigestReplayDistance(
			iHome, iScan, pShard->SlotCapacity
		) ) {
			pShard->Slots[iHole] = *pMove;
			pShard->Heap[pShard->Slots[iHole].HeapIndex] =
				&pShard->Slots[iHole];
			pMove->Occupied = false;
			iHole = iScan;
		}
		iScan = __xrtHttpDigestReplayNext(
			iScan, pShard->SlotCapacity
		);
	}
	memset(&pShard->Slots[iHole], 0, sizeof(pShard->Slots[iHole]));
	pShard->EntryCount--;
	return true;
}



/* 在线性堆顶路径上删除全部已经失效的记录。 */
static bool __xrtHttpDigestReplayPurgeLocked(
	xrt_http_digest_replay_shard* pShard,
	int64 iNowSeconds,
	size_t* pRemoved
)
{
	size_t iRemoved = 0;

	while ( (pShard->HeapCount != 0) &&
		(pShard->Heap[0]->ExpiresSeconds < iNowSeconds) ) {
		xrt_http_digest_replay_entry* pEntry =
			__xrtHttpDigestReplayHeapPop(pShard);

		if ( !__xrtHttpDigestReplayRemove(pShard, pEntry) ) {
			return false;
		}
		iRemoved++;
	}
	__xrtHttpDigestReplayCounterAdd(&pShard->Purged, iRemoved);
	*pRemoved = iRemoved;
	return true;
}



/* 从固定键选择分片。 */
static size_t __xrtHttpDigestReplayShard(
	const xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey
)
{
	return (size_t)(
		__xrtHttpDigestReplayHash(pKey) %
		(uint64)pReplay->ShardCount
	);
}



/* 解析默认或调用方配置，并验证所有容量乘法。 */
static bool __xrtHttpDigestReplayConfigResolve(
	const xhttpdigestreplayconfig* pInput,
	xhttpdigestreplayconfig* pConfig,
	size_t* pTotalCapacity,
	size_t* pSlotsPerShard,
	size_t* pTotalSlots
)
{
	size_t iExtraSlots;

	xrtHttpDigestReplayConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( (pConfig->Shards == 0) ||
		(pConfig->Shards > XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX) ||
		(pConfig->EntriesPerShard == 0) ||
		(pConfig->LifetimeSeconds <= 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pConfig->Shards >
		(SIZE_MAX / pConfig->EntriesPerShard) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pTotalCapacity = pConfig->Shards * pConfig->EntriesPerShard;
	iExtraSlots = (pConfig->EntriesPerShard / 2u) +
		(pConfig->EntriesPerShard % 2u);
	if ( pConfig->EntriesPerShard > (SIZE_MAX - iExtraSlots) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pSlotsPerShard = pConfig->EntriesPerShard + iExtraSlots;
	if ( pConfig->Shards > (SIZE_MAX / *pSlotsPerShard) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pTotalSlots = pConfig->Shards * *pSlotsPerShard;
	if ( (*pTotalCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ||
		(*pTotalCapacity >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry*))) ||
		(*pTotalSlots >
		 (SIZE_MAX / sizeof(xrt_http_digest_replay_entry))) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return true;
}



/* 释放已初始化的 Mutex 前缀。 */
static void __xrtHttpDigestReplayLocksUnit(
	xhttpdigestreplay* pReplay,
	size_t iShards
)
{
	while ( iShards != 0 ) {
		xrt_http_digest_replay_shard* pShard =
			&pReplay->Shards[--iShards];

		if ( pShard->LockReady ) {
			(void)xrtMutexUnit(&pShard->Lock);
		}
		pShard->LockReady = false;
	}
}



/* 初始化默认容量和五分钟有效期。 */
XHTTP_POLICY_API void xrtHttpDigestReplayConfigInit(
	xhttpdigestreplayconfig* pConfig
)
{
	xhttpdigestreplayconfig Config = {
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS,
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES,
		XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME
	};

	if ( (pConfig == NULL) ||
		!xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 构建可供内置或外部原子存储使用的固定重放键。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayKey(
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	xhttpdigestreplaykey* pKey
)
{
	xsha256 State;
	xhttpdigestreplaykey Key;
	uint8 Length[8];
	uint64 iTotal = sizeof(__xrtHttpDigestReplayDomain) - 1u + 24u;
	bool bResult = false;

	memset(&State, 0, sizeof(State));
	memset(&Key, 0, sizeof(Key));
	memset(Length, 0, sizeof(Length));
	if ( !__xhttpViewValid(Username) ||
		!__xhttpViewValid(Nonce) ||
		!__xhttpViewValid(Cnonce) ||
		(Nonce.Size == 0) || (Cnonce.Size == 0) ||
		(pKey == NULL) || !xrtMemRangeValid(pKey, sizeof(Key)) ||
		xrtMemRangesOverlap(
			pKey, sizeof(Key), Username.Data, Username.Size
		) || xrtMemRangesOverlap(
			pKey, sizeof(Key), Nonce.Data, Nonce.Size
		) || xrtMemRangesOverlap(
			pKey, sizeof(Key), Cnonce.Data, Cnonce.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		goto cleanup;
	}
	if ( ((uint64)Username.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		xrtSetErrorKind(XERR_RANGE);
		goto cleanup;
	}
	iTotal += (uint64)Username.Size;
	if ( ((uint64)Nonce.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		xrtSetErrorKind(XERR_RANGE);
		goto cleanup;
	}
	iTotal += (uint64)Nonce.Size;
	if ( ((uint64)Cnonce.Size >
		 (XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE - iTotal)) ) {
		xrtSetErrorKind(XERR_RANGE);
		goto cleanup;
	}
	xrtSha256Init(&State);
	if ( !xrtSha256Update(
		&State,
		__xrtHttpDigestReplayDomain,
		sizeof(__xrtHttpDigestReplayDomain) - 1u
	) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Username.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Username.Data, Username.Size) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Nonce.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Nonce.Data, Nonce.Size) ) {
		goto cleanup;
	}
	__xrtHttpDigestReplayStore64(Length, (uint64)Cnonce.Size);
	if ( !xrtSha256Update(&State, Length, sizeof(Length)) ||
		!xrtSha256Update(&State, Cnonce.Data, Cnonce.Size) ||
		!xrtSha256Final(&State, Key.Bytes) ) {
		goto cleanup;
	}
	memcpy(pKey, &Key, sizeof(Key));
	bResult = true;

cleanup:
	xrtSecureZero(&State, sizeof(State));
	xrtSecureZero(&Key, sizeof(Key));
	xrtSecureZero(Length, sizeof(Length));
	return bResult;
}



/* 创建全部分片及固定容量槽和过期堆。 */
XHTTP_POLICY_API xhttpdigestreplay* xrtHttpDigestReplayCreate(
	const xhttpdigestreplayconfig* pConfig
)
{
	xhttpdigestreplayconfig Config;
	xhttpdigestreplay* pReplay;
	size_t iTotalCapacity;
	size_t iSlotsPerShard;
	size_t iTotalSlots;
	size_t iObjectSize;
	size_t iInitialized = 0;
	xerror* pError;

	if ( !__xrtHttpDigestReplayConfigResolve(
		pConfig, &Config, &iTotalCapacity,
		&iSlotsPerShard, &iTotalSlots
	) ) {
		return NULL;
	}
	if ( Config.Shards >
		((SIZE_MAX - sizeof(*pReplay)) /
		 sizeof(xrt_http_digest_replay_shard)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iObjectSize = sizeof(*pReplay) +
		(Config.Shards * sizeof(xrt_http_digest_replay_shard));
	pReplay = (xhttpdigestreplay*)xrtMalloc(iObjectSize);
	if ( pReplay == NULL ) {
		return NULL;
	}
	memset(pReplay, 0, iObjectSize);
	pReplay->SlotStorage =
		(xrt_http_digest_replay_entry*)xrtMalloc(
			iTotalSlots *
			sizeof(xrt_http_digest_replay_entry)
		);
	if ( pReplay->SlotStorage == NULL ) {
		goto failure;
	}
	memset(
		pReplay->SlotStorage,
		0,
		iTotalSlots * sizeof(xrt_http_digest_replay_entry)
	);
	pReplay->HeapStorage =
		(xrt_http_digest_replay_entry**)xrtMalloc(
			iTotalCapacity *
			sizeof(xrt_http_digest_replay_entry*)
		);
	if ( pReplay->HeapStorage == NULL ) {
		goto failure;
	}
	memset(
		pReplay->HeapStorage,
		0,
		iTotalCapacity * sizeof(xrt_http_digest_replay_entry*)
	);
	pReplay->LifetimeSeconds = Config.LifetimeSeconds;
	pReplay->ShardCount = Config.Shards;
	pReplay->TotalCapacity = iTotalCapacity;
	pReplay->TotalSlots = iTotalSlots;
	for ( size_t i = 0; i < Config.Shards; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		pShard->Slots = pReplay->SlotStorage +
			(i * iSlotsPerShard);
		pShard->Heap = pReplay->HeapStorage +
			(i * Config.EntriesPerShard);
		pShard->Capacity = Config.EntriesPerShard;
		pShard->SlotCapacity = iSlotsPerShard;
		if ( !xrtMutexInit(&pShard->Lock) ) {
			goto failure;
		}
		pShard->LockReady = true;
		iInitialized = i + 1u;
	}
	pReplay->Guard = XRT_HTTP_DIGEST_REPLAY_GUARD;
	return pReplay;

failure:
	pError = xrtTakeError();
	__xrtHttpDigestReplayLocksUnit(pReplay, iInitialized);
	xrtFree(pReplay->HeapStorage);
	xrtFree(pReplay->SlotStorage);
	xrtSecureZero(pReplay, iObjectSize);
	xrtFree(pReplay);
	xrtClearError();
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	} else {
		xrtSetErrorKind(XERR_INTERNAL);
	}
	return NULL;
}



/* 销毁全部分片，同时保持调用线程原有错误不变。 */
XHTTP_POLICY_API void xrtHttpDigestReplayDestroy(
	xhttpdigestreplay* pReplay
)
{
	xerror* pSaved;
	size_t iObjectSize;
	size_t iShards;

	if ( pReplay == NULL ) {
		return;
	}
	if ( !__xrtHttpDigestReplayValid(pReplay) ) {
		return;
	}
	pSaved = xrtTakeError();
	iShards = pReplay->ShardCount;
	iObjectSize = sizeof(*pReplay) +
		(iShards * sizeof(xrt_http_digest_replay_shard));
	pReplay->Guard = 0;
	__xrtHttpDigestReplayLocksUnit(pReplay, iShards);
	xrtSecureZero(
		pReplay->SlotStorage,
		pReplay->TotalSlots *
		sizeof(xrt_http_digest_replay_entry)
	);
	xrtFree(pReplay->HeapStorage);
	xrtFree(pReplay->SlotStorage);
	xrtSecureZero(pReplay, iObjectSize);
	xrtFree(pReplay);
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetErrorTake(pSaved);
	}
}



/* 在一个分片锁内完成过期清理、重放判断和单调更新。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheckKey(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
)
{
	xhttpdigestreplaykey Key;
	xrt_http_digest_replay_shard* pShard;
	xrt_http_digest_replay_entry* pEntry;
	xrt_http_digest_replay_entry* pEmpty;
	xhttpdigestreplaycheck Check = XHTTP_DIGEST_REPLAY_ERROR;
	int64 iExpiresSeconds;
	size_t iRemoved;

	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(pKey == NULL) || !xrtMemRangeValid(pKey, sizeof(Key)) ) {
		if ( (pKey == NULL) ||
			((pKey != NULL) && !xrtMemRangeValid(pKey, sizeof(Key))) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	if ( (iNonceCount == 0) || (iIssuedSeconds < 0) ||
		(iNowSeconds < 0) ||
		(iIssuedSeconds >
		 (INT64_MAX - pReplay->LifetimeSeconds)) ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	memcpy(&Key, pKey, sizeof(Key));
	iExpiresSeconds = iIssuedSeconds + pReplay->LifetimeSeconds;
	pShard = &pReplay->Shards[
		__xrtHttpDigestReplayShard(pReplay, &Key)
	];
	if ( !xrtMutexLock(&pShard->Lock) ) {
		goto cleanup;
	}
	if ( !__xrtHttpDigestReplayPurgeLocked(
		pShard, iNowSeconds, &iRemoved
	) ) {
		(void)xrtMutexUnlock(&pShard->Lock);
		goto cleanup;
	}
	if ( iNowSeconds > iExpiresSeconds ) {
		__xrtHttpDigestReplayCounterAdd(&pShard->Expired, 1u);
		Check = XHTTP_DIGEST_REPLAY_EXPIRED;
		goto unlock;
	}
	pEntry = __xrtHttpDigestReplayFind(pShard, &Key, &pEmpty);
	if ( pEntry != NULL ) {
		if ( iNonceCount <= pEntry->NonceCount ) {
			__xrtHttpDigestReplayCounterAdd(&pShard->Replayed, 1u);
			Check = XHTTP_DIGEST_REPLAY_REPLAY;
		} else {
			pEntry->NonceCount = iNonceCount;
			__xrtHttpDigestReplayCounterAdd(&pShard->Accepted, 1u);
			Check = XHTTP_DIGEST_REPLAY_ACCEPTED;
		}
		goto unlock;
	}
	if ( (pShard->EntryCount >= pShard->Capacity) ||
		(pEmpty == NULL) ) {
		__xrtHttpDigestReplayCounterAdd(&pShard->Full, 1u);
		Check = XHTTP_DIGEST_REPLAY_FULL;
		goto unlock;
	}
	memcpy(&pEmpty->Key, &Key, sizeof(Key));
	pEmpty->NonceCount = iNonceCount;
	pEmpty->ExpiresSeconds = iExpiresSeconds;
	pEmpty->Occupied = true;
	pShard->EntryCount++;
	__xrtHttpDigestReplayHeapPush(pShard, pEmpty);
	__xrtHttpDigestReplayCounterAdd(&pShard->Accepted, 1u);
	Check = XHTTP_DIGEST_REPLAY_ACCEPTED;

unlock:
	if ( !xrtMutexUnlock(&pShard->Lock) ) {
		Check = XHTTP_DIGEST_REPLAY_ERROR;
	}

cleanup:
	xrtSecureZero(&Key, sizeof(Key));
	return Check;
}



/* 组合公开重放键派生与低层原子检查。 */
XHTTP_POLICY_API xhttpdigestreplaycheck xrtHttpDigestReplayCheck(
	xhttpdigestreplay* pReplay,
	xstrview Username,
	xstrview Nonce,
	xstrview Cnonce,
	uint32 iNonceCount,
	int64 iIssuedSeconds,
	int64 iNowSeconds
)
{
	xhttpdigestreplaykey Key;
	xhttpdigestreplaycheck Check;

	memset(&Key, 0, sizeof(Key));
	if ( !xrtHttpDigestReplayKey(
		Username, Nonce, Cnonce, &Key
	) ) {
		return XHTTP_DIGEST_REPLAY_ERROR;
	}
	Check = xrtHttpDigestReplayCheckKey(
		pReplay,
		&Key,
		iNonceCount,
		iIssuedSeconds,
		iNowSeconds
	);
	xrtSecureZero(&Key, sizeof(Key));
	return Check;
}



/* 逐分片删除全部过期记录。 */
XHTTP_POLICY_API size_t xrtHttpDigestReplayPurge(
	xhttpdigestreplay* pReplay,
	int64 iNowSeconds
)
{
	size_t iTotal = 0;

	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(iNowSeconds < 0) ) {
		if ( iNowSeconds < 0 ) {
			xrtSetErrorKind(XERR_VALUE);
		}
		return 0;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];
		size_t iRemoved;

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return iTotal;
		}
		if ( !__xrtHttpDigestReplayPurgeLocked(
			pShard, iNowSeconds, &iRemoved
		) ) {
			(void)xrtMutexUnlock(&pShard->Lock);
			return iTotal;
		}
		iTotal += iRemoved;
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return iTotal;
		}
	}
	return iTotal;
}



/* 清空所有分片并保留已分配容量。 */
XHTTP_POLICY_API void xrtHttpDigestReplayClear(
	xhttpdigestreplay* pReplay
)
{
	if ( !__xrtHttpDigestReplayValid(pReplay) ) {
		return;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return;
		}
		memset(
			pShard->Slots,
			0,
			pShard->SlotCapacity * sizeof(*pShard->Slots)
		);
		memset(
			pShard->Heap,
			0,
			pShard->Capacity * sizeof(*pShard->Heap)
		);
		pShard->EntryCount = 0;
		pShard->HeapCount = 0;
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return;
		}
	}
}



/* 汇总各分片当前状态和饱和计数。 */
XHTTP_POLICY_API bool xrtHttpDigestReplayStats(
	xhttpdigestreplay* pReplay,
	xhttpdigestreplaystats* pStats
)
{
	xhttpdigestreplaystats Stats;

	memset(&Stats, 0, sizeof(Stats));
	if ( !__xrtHttpDigestReplayValid(pReplay) ||
		(pStats == NULL) || !xrtMemRangeValid(pStats, sizeof(Stats)) ) {
		if ( (pStats == NULL) ||
			((pStats != NULL) &&
			 !xrtMemRangeValid(pStats, sizeof(Stats))) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return false;
	}
	for ( size_t i = 0; i < pReplay->ShardCount; i++ ) {
		xrt_http_digest_replay_shard* pShard = &pReplay->Shards[i];

		if ( !xrtMutexLock(&pShard->Lock) ) {
			return false;
		}
		Stats.Entries += pShard->EntryCount;
		Stats.Capacity += pShard->Capacity;
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Accepted, pShard->Accepted
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Replayed, pShard->Replayed
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Expired, pShard->Expired
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Full, pShard->Full
		);
		__xrtHttpDigestReplayCounterAdd(
			&Stats.Purged, pShard->Purged
		);
		if ( !xrtMutexUnlock(&pShard->Lock) ) {
			return false;
		}
	}
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}



#undef XRT_HTTP_DIGEST_REPLAY_GUARD
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_SHARDS
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_ENTRIES
#undef XRT_HTTP_DIGEST_REPLAY_DEFAULT_LIFETIME
#undef XRT_HTTP_DIGEST_REPLAY_SHARDS_MAX
#undef XRT_HTTP_DIGEST_REPLAY_MAX_MESSAGE

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_client_prepare_auth_digest_session.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_PREPARE_AUTH_DIGEST_SESSION)

/* 回调上下文只在一次同步准备调用内存活。 */
typedef struct xrt_http_digest_prepare {
	xhttpdigestsession* Session;
	xstrview EntityHash;
	xstrview Name;
	xhttpdigestexchange* Exchange;
	str Value;
	size_t ValueSize;
} xrt_http_digest_prepare;



/* 使用准备器已经确定的线路 target 生成 Exchange 和认证字段。 */
static bool __xrtHttp1RequestDigestField(
	xstrview Method,
	xstrview Target,
	ptr pData,
	xhttpfield* pField
)
{
	xrt_http_digest_prepare* pPrepare =
		(xrt_http_digest_prepare*)pData;
	const xhttpdigestauth* pAuth;

	pPrepare->Exchange = xrtHttpDigestSessionAuthorize(
		pPrepare->Session,
		Method,
		Target,
		pPrepare->EntityHash
	);
	if ( pPrepare->Exchange == NULL ) {
		return false;
	}
	pAuth = xrtHttpDigestExchangeAuth(pPrepare->Exchange);
	if ( (pAuth == NULL) || !xrtHttpDigestAuthWrite(
		pAuth, NULL, 0, &pPrepare->ValueSize
	) ) {
		return false;
	}
	pPrepare->Value = (str)xrtMalloc(pPrepare->ValueSize);
	if ( pPrepare->Value == NULL ) {
		return false;
	}
	if ( !xrtHttpDigestAuthWrite(
		pAuth,
		pPrepare->Value,
		pPrepare->ValueSize,
		&pPrepare->ValueSize
	) ) {
		return false;
	}
	*pField = (xhttpfield){
		pPrepare->Name,
		{ pPrepare->Value, pPrepare->ValueSize }
	};
	return true;
}



/* 清除临时线路凭据，并按准备结果决定是否保留 Exchange。 */
static void __xrtHttp1RequestDigestPrepareUnit(
	xrt_http_digest_prepare* pPrepare,
	bool bKeepExchange
)
{
	if ( pPrepare->Value != NULL ) {
		xrtSecureZero(
			pPrepare->Value,
			pPrepare->ValueSize
		);
		xrtFree(pPrepare->Value);
	}
	if ( !bKeepExchange ) {
		xrtHttpDigestExchangeRelease(pPrepare->Exchange);
	}
	pPrepare->Value = NULL;
	pPrepare->Exchange = NULL;
}



/* 校验输出位置，并执行源站或代理 Digest 请求准备。 */
static xhttp1requestplan* __xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xstrview Name,
	xhttpdigestexchange** ppExchange
)
{
	xrt_http_digest_prepare Prepare;
	xhttpdigestexchange* pEmpty = NULL;
	xhttp1requestplan* pPlan;

	if ( !xrtHttpBridgeRequestOutputValid(
		pRequest, ppExchange, sizeof(*ppExchange)
	) || !__xrtHttpDigestSessionOutputValid(
		pSession, NULL, ppExchange, sizeof(*ppExchange)
	) ||
		((pOptions != NULL) && !xrtMemRangeValid(
			pOptions, sizeof(*pOptions)
		)) ||
		!__xhttpViewValid(EntityHash) ||
		((pOptions != NULL) && xrtMemRangesOverlap(
			ppExchange, sizeof(*ppExchange),
			pOptions, sizeof(*pOptions)
		)) || xrtMemRangesOverlap(
			ppExchange, sizeof(*ppExchange),
			EntityHash.Data, EntityHash.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	memcpy(ppExchange, &pEmpty, sizeof(pEmpty));
	memset(&Prepare, 0, sizeof(Prepare));
	Prepare.Session = pSession;
	Prepare.EntityHash = EntityHash;
	Prepare.Name = Name;
	pPlan = xrtHttpBridge1RequestPrepareField(
		pRequest,
		pOptions,
		__xrtHttp1RequestDigestField,
		&Prepare
	);
	if ( pPlan == NULL ) {
		__xrtHttp1RequestDigestPrepareUnit(&Prepare, false);
		return NULL;
	}
	memcpy(ppExchange, &Prepare.Exchange, sizeof(Prepare.Exchange));
	__xrtHttp1RequestDigestPrepareUnit(&Prepare, true);
	return pPlan;
}



/* 使用最终线路 target 准备源站 Digest 请求。 */
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
)
{
	return __xrtHttp1RequestPrepareDigest(
		pRequest,
		pOptions,
		pSession,
		EntityHash,
		XRT_STR_LITERAL("Authorization"),
		ppExchange
	);
}



/* 使用最终线路 target 准备代理 Digest 请求。 */
XHTTP_POLICY_API xhttp1requestplan* xrtHttp1RequestPrepareProxyDigest(
	const xhttprequest* pRequest,
	const xhttp1requestoptions* pOptions,
	xhttpdigestsession* pSession,
	xstrview EntityHash,
	xhttpdigestexchange** ppExchange
)
{
	return __xrtHttp1RequestPrepareDigest(
		pRequest,
		pOptions,
		pSession,
		EntityHash,
		XRT_STR_LITERAL("Proxy-Authorization"),
		ppExchange
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_client_response_auth_digest_choose.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_CHOOSE)

/* 校验选择输出，并建立失败原子的空结果。 */
static bool __xrtHttpResponseDigestChoosePrepare(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	xhttpdigestchallenge EmptyChallenge = { 0 };
	xhttpdigestchoice EmptyChoice = { 0 };
	size_t iZero = 0;

	if ( ((pPolicy != NULL) && !xrtMemRangeValid(
			pPolicy, sizeof(*pPolicy)
		)) || !xrtHttpBridgeResponseOutputValid(
			pResponse, pSize, sizeof(*pSize)
		) || !xrtHttpBridgeResponseOutputValid(
			pResponse, pChallenge, sizeof(*pChallenge)
		) || !xrtHttpBridgeResponseOutputValid(
			pResponse, pChoice, sizeof(*pChoice)
		) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !xrtHttpBridgeResponseOutputValid(
			pResponse, pOutput, iCapacity
		)) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pChallenge, sizeof(*pChallenge)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pChoice, sizeof(*pChoice)
		) || xrtMemRangesOverlap(
			pChallenge, sizeof(*pChallenge),
			pChoice, sizeof(*pChoice)
		) || ((pPolicy != NULL) && (xrtMemRangesOverlap(
			pPolicy, sizeof(*pPolicy), pSize, sizeof(*pSize)
		) || xrtMemRangesOverlap(
			pPolicy, sizeof(*pPolicy),
			pChallenge, sizeof(*pChallenge)
		) || xrtMemRangesOverlap(
			pPolicy, sizeof(*pPolicy), pChoice, sizeof(*pChoice)
		) || ((pOutput != NULL) && xrtMemRangesOverlap(
			pPolicy, sizeof(*pPolicy), pOutput, iCapacity
		)))) || ((pOutput != NULL) && (xrtMemRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity,
			pChallenge, sizeof(*pChallenge)
		) || xrtMemRangesOverlap(
			pOutput, iCapacity, pChoice, sizeof(*pChoice)
		))) ) {
		xrtHttpBridgeResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"choose-http-response-digest-challenge",
			"HTTP response Digest selection output is invalid",
			NULL
		);
		return false;
	}
	memcpy(pSize, &iZero, sizeof(iZero));
	memcpy(pChallenge, &EmptyChallenge, sizeof(EmptyChallenge));
	memcpy(pChoice, &EmptyChoice, sizeof(EmptyChoice));
	return true;
}



/* 恢复通用解析结果覆盖的完整 challenge 连续视图。 */
static bool __xrtHttpResponseDigestChallengeValue(
	const xhttpauth* pAuth,
	xstrview* pValue
)
{
	uintptr_t iStart = (uintptr_t)pAuth->Scheme.Data;
	uintptr_t iEnd;

	if ( pAuth->Kind == XHTTP_AUTH_NONE ) {
		if ( pAuth->Scheme.Size > (UINTPTR_MAX - iStart) ) {
			return false;
		}
		iEnd = iStart + pAuth->Scheme.Size;
	} else {
		iEnd = (uintptr_t)pAuth->Data.Data;
		if ( pAuth->Data.Size > (UINTPTR_MAX - iEnd) ) {
			return false;
		}
		iEnd += pAuth->Data.Size;
	}
	if ( iEnd < iStart ) {
		return false;
	}
	*pValue = (xstrview){
		pAuth->Scheme.Data,
		(size_t)(iEnd - iStart)
	};
	return true;
}



/* 包装协议层解析或策略错误到稳定的客户端响应错误域。 */
static xhttpnext __xrtHttpResponseDigestChooseError(cstr sMessage)
{
	xrtHttpBridgeResponseWrapError(
		XERR_VALUE,
		XHTTP_RESPONSE_ERROR_AUTH,
		"choose-http-response-digest-challenge",
		sMessage
	);
	return XHTTP_NEXT_ERROR;
}



/* 按字段线路顺序选择首个本地支持的 Digest challenge。 */
static xhttpnext __xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	bool bProxy,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	xhttpauthcursor Cursor;
	xhttpauth Auth;
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	xstrview Value;
	size_t iRequired;
	xhttpnext Next;

	if ( !__xrtHttpResponseDigestChoosePrepare(
		pResponse,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	) ) {
		return XHTTP_NEXT_ERROR;
	}
	xrtHttpAuthCursorInit(&Cursor);
	for ( ;; ) {
		xhttpdigestchoosecheck Check;

		Next = bProxy ? xrtHttpResponseProxyChallengeNext(
			pResponse, &Cursor, &Auth
		) : xrtHttpResponseChallengeNext(
			pResponse, &Cursor, &Auth
		);
		if ( Next != XHTTP_NEXT_ITEM ) {
			return Next;
		}
		if ( !xrtHttpTokenEqual(
			Auth.Scheme, XRT_STR_LITERAL("Digest")
		) ) {
			continue;
		}
		if ( !__xrtHttpResponseDigestChallengeValue(
			&Auth, &Value
		) ) {
			xrtHttpBridgeResponseSetError(
				XERR_STATE,
				XHTTP_RESPONSE_ERROR_AUTH,
				"choose-http-response-digest-challenge",
				"HTTP response Digest challenge range is invalid",
				NULL
			);
			return XHTTP_NEXT_ERROR;
		}
		if ( !xrtHttpDigestChallengeRead(
			Value, NULL, 0, &iRequired, &Challenge
		) ) {
			return __xrtHttpResponseDigestChooseError(
				"HTTP response Digest challenge is invalid"
			);
		}
		Check = xrtHttpDigestChallengeChoose(
			&Challenge, pPolicy, &Choice
		);
		if ( Check == XHTTP_DIGEST_CHOOSE_ERROR ) {
			return __xrtHttpResponseDigestChooseError(
				"HTTP response Digest selection policy is invalid"
			);
		}
		if ( Check == XHTTP_DIGEST_CHOOSE_REJECTED ) {
			continue;
		}
		memcpy(pSize, &iRequired, sizeof(iRequired));
		if ( pOutput != NULL ) {
			if ( !xrtHttpDigestChallengeRead(
				Value,
				pOutput,
				iCapacity,
				&iRequired,
				&Challenge
			) ) {
				memcpy(pSize, &iRequired, sizeof(iRequired));
				return __xrtHttpResponseDigestChooseError(
					"HTTP response Digest challenge cannot be decoded"
				);
			}
		}
		memcpy(pChallenge, &Challenge, sizeof(Challenge));
		memcpy(pChoice, &Choice, sizeof(Choice));
		return XHTTP_NEXT_ITEM;
	}
}



/* 选择源站首个可用 Digest challenge。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	return __xrtHttpResponseDigestChallengeChoose(
		pResponse,
		false,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	);
}



/* 选择代理首个可用 Digest challenge。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestChallengeChoose(
	const xhttpresponse* pResponse,
	const xhttpdigestpolicy* pPolicy,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	return __xrtHttpResponseDigestChallengeChoose(
		pResponse,
		true,
		pPolicy,
		pOutput,
		iCapacity,
		pSize,
		pChallenge,
		pChoice
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_client_response_auth_digest_session.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

/* Digest 回执读取函数保持源站与代理路径完全对称。 */
typedef xhttpnext (*xrt_http_digest_session_info_function)(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);



/* 解析唯一回执，并把会话状态转换保持为一次失败原子操作。 */
static xhttpnext __xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck,
	xrt_http_digest_session_info_function pInfo,
	cstr sOperation
)
{
	const xhttpdigestproof* pProof;
	xhttpdigestsessioncheck Check = XHTTP_DIGEST_SESSION_ERROR;
	xhttpdigestinfo Info;
	xhttpnext Next;
	void* pOutput;
	size_t iSize = 0;

	if ( (pSession == NULL) || (pExchange == NULL) ||
		!__xhttpViewValid(ResponseEntityHash) ||
		!__xhttpViewValid(NextCnonce) ||
		!xrtHttpBridgeResponseOutputValid(
			pResponse, pCheck, sizeof(Check)
		) || !__xrtHttpDigestSessionOutputValid(
			pSession, pExchange, pCheck, sizeof(Check)
		) ||
		xrtMemRangesOverlap(
			pCheck, sizeof(Check),
			ResponseEntityHash.Data, ResponseEntityHash.Size
		) || xrtMemRangesOverlap(
			pCheck, sizeof(Check),
			NextCnonce.Data, NextCnonce.Size
		) ) {
		xrtHttpBridgeResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			sOperation,
			"HTTP response Digest session input is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCheck, &Check, sizeof(Check));
	pProof = xrtHttpDigestExchangeProof(pExchange);
	if ( pProof == NULL ) {
		xrtHttpBridgeResponseWrapError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest exchange is invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = pInfo(
		pResponse,
		pProof->Algorithm,
		NULL,
		0,
		&iSize,
		&Info
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	pOutput = xrtMalloc(iSize);
	if ( pOutput == NULL ) {
		xrtHttpBridgeResponseWrapError(
			XERR_MEMORY,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest info allocation failed"
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = pInfo(
		pResponse,
		pProof->Algorithm,
		pOutput,
		iSize,
		&iSize,
		&Info
	);
	if ( Next == XHTTP_NEXT_ITEM ) {
		Check = xrtHttpDigestSessionAccept(
			pSession,
			pExchange,
			&Info,
			ResponseEntityHash,
			NextCnonce
		);
	}
	xrtSecureZero(pOutput, iSize);
	xrtFree(pOutput);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( Check == XHTTP_DIGEST_SESSION_ERROR ) {
		xrtHttpBridgeResponseWrapError(
			XERR_PROTOCOL,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest session verification failed"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCheck, &Check, sizeof(Check));
	return XHTTP_NEXT_ITEM;
}



/* 解析并接受源站 Authentication-Info。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
)
{
	return __xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		ResponseEntityHash,
		NextCnonce,
		pCheck,
		xrtHttpResponseDigestInfo,
		"accept-http-response-digest-session"
	);
}



/* 解析并接受代理 Proxy-Authentication-Info。 */
XHTTP_POLICY_API xhttpnext xrtHttpResponseProxyDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
)
{
	return __xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		ResponseEntityHash,
		NextCnonce,
		pCheck,
		xrtHttpResponseProxyDigestInfo,
		"accept-http-response-proxy-digest-session"
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/cookie_jar.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_COOKIE_JAR)
#include <stdlib.h>



#if defined(XHTTP_FEATURE_COOKIE_JAR)

/* 内部选择数组保存稳定条目指针，避免对 Jar 本体重新排列。 */
typedef struct xrt_cookie_selection {
	xrt_cookie_entry** Items;
	size_t Count;
	size_t Size;
} xrt_cookie_selection;



/* 验证通用借用视图。 */
static bool __xrtCookieJarViewValid(xstrview Text)
{
	return (Text.Data != NULL) || (Text.Size == 0);
}



/* 按字节比较两个视图。 */
static bool __xrtCookieJarTextEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 按 ASCII 大小写不敏感规则比较两个视图。 */
static bool __xrtCookieJarAsciiEqual(xstrview Left, xstrview Right)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		if ( __xhttpAsciiLower((unsigned char)Left.Data[i]) !=
			__xhttpAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断名称是否带有大小写不敏感的安全前缀。 */
static bool __xrtCookieJarPrefix(xstrview Name, cstr sPrefix, size_t iSize)
{
	return (Name.Size >= iSize) && __xrtCookieJarAsciiEqual(
		(xstrview){ Name.Data, iSize }, (xstrview){ sPrefix, iSize }
	);
}



/* 验证不透明分区键不包含控制字节。 */
static bool __xrtCookieJarPartitionValid(xstrview Key)
{
	size_t i;

	if ( !__xrtCookieJarViewValid(Key) ) {
		return false;
	}
	for ( i = 0; i < Key.Size; i++ ) {
		unsigned char iByte = (unsigned char)Key.Data[i];

		if ( (iByte < 0x20u) || (iByte == 0x7Fu) ) {
			return false;
		}
	}
	return true;
}



/* 判断 Set 的 pair 与宽松接收解析器产物一致。 */
static bool __xrtCookieJarPairValid(const xsetcookie* pCookie)
{
	size_t i;

	if ( (pCookie->Name.Size > XSET_COOKIE_MAX_PAIR_BYTES) ||
		(pCookie->Value.Size >
		 (XSET_COOKIE_MAX_PAIR_BYTES - pCookie->Name.Size)) ) {
		return false;
	}
	if ( ((pCookie->Name.Size != 0) &&
		 ((pCookie->Name.Data[0] == ' ') ||
		  (pCookie->Name.Data[0] == '\t') ||
		  (pCookie->Name.Data[pCookie->Name.Size - 1u] == ' ') ||
		  (pCookie->Name.Data[pCookie->Name.Size - 1u] == '\t'))) ||
		((pCookie->Value.Size != 0) &&
		 ((pCookie->Value.Data[0] == ' ') ||
		  (pCookie->Value.Data[0] == '\t') ||
		  (pCookie->Value.Data[pCookie->Value.Size - 1u] == ' ') ||
		  (pCookie->Value.Data[pCookie->Value.Size - 1u] == '\t'))) ) {
		return false;
	}
	for ( i = 0; i < pCookie->Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)pCookie->Name.Data[i];

		if ( (iByte <= 0x08u) ||
			((iByte >= 0x0Au) && (iByte <= 0x1Fu)) ||
			(iByte == 0x7Fu) || (iByte == ';') || (iByte == '=') ) {
			return false;
		}
	}
	for ( i = 0; i < pCookie->Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)pCookie->Value.Data[i];

		if ( (iByte <= 0x08u) ||
			((iByte >= 0x0Au) && (iByte <= 0x1Fu)) ||
			(iByte == 0x7Fu) || (iByte == ';') ) {
			return false;
		}
	}
	return true;
}



/* 验证 DNS 标签形式的 ASCII 主机名或 Domain 属性。 */
static bool __xrtCookieJarDomainValid(xstrview Domain)
{
	size_t i = 0;

	if ( (Domain.Size == 0) || (Domain.Size > 253u) ) {
		return false;
	}
	while ( i < Domain.Size ) {
		size_t iBegin = i;

		while ( (i < Domain.Size) && (Domain.Data[i] != '.') ) {
			unsigned char iByte = (unsigned char)Domain.Data[i];

			if ( !((iByte >= (unsigned char)'A' &&
				iByte <= (unsigned char)'Z') ||
				(iByte >= (unsigned char)'a' &&
				 iByte <= (unsigned char)'z') ||
				(iByte >= (unsigned char)'0' &&
				 iByte <= (unsigned char)'9') ||
				(iByte == (unsigned char)'-')) ) {
				return false;
			}
			i++;
		}
		if ( (i == iBegin) || ((i - iBegin) > 63u) ||
			(Domain.Data[iBegin] == '-') ||
			(Domain.Data[i - 1u] == '-') ) {
			return false;
		}
		if ( i < Domain.Size ) {
			i++;
			if ( i == Domain.Size ) {
				return false;
			}
		}
	}
	return true;
}



/* 严格识别规范十进制 IPv4 地址。 */
static bool __xrtCookieJarIpv4(xstrview Host)
{
	size_t i = 0;
	size_t iPart;

	for ( iPart = 0; iPart < 4; iPart++ ) {
		size_t iBegin = i;
		uint32 iValue = 0;

		while ( (i < Host.Size) &&
			(Host.Data[i] >= '0') && (Host.Data[i] <= '9') ) {
			iValue = (iValue * 10u) +
				(uint32)(Host.Data[i] - '0');
			i++;
		}
		if ( (i == iBegin) || ((i - iBegin) > 3u) ||
			(iValue > 255u) ||
			(((i - iBegin) > 1u) && (Host.Data[iBegin] == '0')) ) {
			return false;
		}
		if ( iPart == 3u ) {
			return i == Host.Size;
		}
		if ( (i >= Host.Size) || (Host.Data[i] != '.') ) {
			return false;
		}
		i++;
	}
	return false;
}



/* 规范化 HTTP Cookie 使用的 URL 主机名。 */
bool __xrtCookieOriginParse(xstrview URL, xrt_cookie_origin* pOrigin)
{
	xurl Parsed;
	xstrview Host;
	size_t i;

	if ( (pOrigin == NULL) || !__xrtCookieJarViewValid(URL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memset(pOrigin, 0, sizeof(*pOrigin));
	if ( !xrtUrlParse(URL, &Parsed) ||
		((Parsed.Flags & (XURL_HAS_SCHEME | XURL_HAS_AUTHORITY |
		 XURL_HAS_HOST)) != (XURL_HAS_SCHEME | XURL_HAS_AUTHORITY |
		 XURL_HAS_HOST)) || (Parsed.Host.Size == 0) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("https")) ||
		xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("wss")) ) {
		pOrigin->Secure = true;
	} else if ( !xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("http")) &&
		!xrtUrlSchemeIs(&Parsed, XRT_STR_LITERAL("ws")) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	Host = Parsed.Host;
	while ( (Host.Size != 0) && (Host.Data[Host.Size - 1u] == '.') ) {
		Host.Size--;
	}
	if ( Host.Size == 0 ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	if ( (Parsed.Flags & XURL_HOST_IP_LITERAL) == 0 ) {
		if ( !__xrtCookieJarDomainValid(Host) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		pOrigin->IpHost = __xrtCookieJarIpv4(Host);
	} else {
		pOrigin->IpHost = true;
	}
	pOrigin->Host = (str)xrtMalloc(Host.Size + 1u);
	if ( pOrigin->Host == NULL ) {
		return false;
	}
	for ( i = 0; i < Host.Size; i++ ) {
		pOrigin->Host[i] = (char)__xhttpAsciiLower(
			(unsigned char)Host.Data[i]
		);
	}
	pOrigin->Host[Host.Size] = '\0';
	pOrigin->HostSize = Host.Size;
	pOrigin->Path = (Parsed.Path.Size == 0) ?
		XRT_STR_LITERAL("/") : Parsed.Path;
	return true;
}



/* 释放规范 URL 拥有的主机副本。 */
void __xrtCookieOriginClear(xrt_cookie_origin* pOrigin)
{
	if ( pOrigin == NULL ) {
		return;
	}
	xrtFree(pOrigin->Host);
	memset(pOrigin, 0, sizeof(*pOrigin));
}



/* 判断规范 Host 是否与规范 Domain 相同或位于其子域。 */
static bool __xrtCookieJarDomainMatch(xstrview Host, xstrview Domain)
{
	if ( __xrtCookieJarAsciiEqual(Host, Domain) ) {
		return true;
	}
	return (Host.Size > Domain.Size) &&
		(Host.Data[Host.Size - Domain.Size - 1u] == '.') &&
		__xrtCookieJarAsciiEqual(
			(xstrview){ Host.Data + Host.Size - Domain.Size,
				Domain.Size }, Domain
		);
}



/* 按 RFC 6265 的目录边界规则匹配请求路径。 */
static bool __xrtCookieJarPathMatch(xstrview Request, xstrview Cookie)
{
	if ( (Request.Size < Cookie.Size) ||
		((Cookie.Size != 0) &&
		 (memcmp(Request.Data, Cookie.Data, Cookie.Size) != 0)) ) {
		return false;
	}
	if ( (Request.Size == Cookie.Size) ||
		(Cookie.Data[Cookie.Size - 1u] == '/') ) {
		return true;
	}
	return Request.Data[Cookie.Size] == '/';
}



/* 从请求路径借用 RFC 默认 Cookie 路径。 */
static xstrview __xrtCookieJarDefaultPath(xstrview Path)
{
	size_t i;
	size_t iLast = 0;

	if ( (Path.Size == 0) || (Path.Data[0] != '/') ) {
		return XRT_STR_LITERAL("/");
	}
	for ( i = 1; i < Path.Size; i++ ) {
		if ( Path.Data[i] == '/' ) {
			iLast = i;
		}
	}
	return (iLast == 0) ? XRT_STR_LITERAL("/") :
		(xstrview){ Path.Data, iLast };
}



/* 释放单分配块 Cookie 条目。 */
static void __xrtCookieJarEntryClear(xrt_cookie_entry* pCookie)
{
	if ( pCookie == NULL ) {
		return;
	}
	xrtFree(pCookie->Storage);
	memset(pCookie, 0, sizeof(*pCookie));
}



/* 删除指定数组条目并保持其余条目相对顺序。 */
static void __xrtCookieJarRemoveAt(xcookiejar* pJar, size_t iIndex)
{
	__xrtCookieJarEntryClear(&pJar->Cookies[iIndex]);
	if ( (iIndex + 1u) < pJar->Count ) {
		memmove(
			&pJar->Cookies[iIndex], &pJar->Cookies[iIndex + 1u],
			(pJar->Count - iIndex - 1u) * sizeof(*pJar->Cookies)
		);
	}
	pJar->Count--;
	memset(&pJar->Cookies[pJar->Count], 0, sizeof(*pJar->Cookies));
}



/* 清理锁内全部过期条目。 */
static size_t __xrtCookieJarPurgeLocked(xcookiejar* pJar, xtime iNow)
{
	size_t i = 0;
	size_t iRemoved = 0;

	while ( i < pJar->Count ) {
		xrt_cookie_entry* pCookie = &pJar->Cookies[i];

		if ( ((pCookie->Flags & XCOOKIE_INFO_PERSISTENT) != 0) &&
			(pCookie->Expires <= iNow) ) {
			__xrtCookieJarRemoveAt(pJar, i);
			iRemoved++;
		} else {
			i++;
		}
	}
	return iRemoved;
}



/* 事务式扩充条目数组。 */
static bool __xrtCookieJarReserve(xcookiejar* pJar, size_t iNeed)
{
	xrt_cookie_entry* pNew;
	size_t iCapacity;

	if ( iNeed <= pJar->Capacity ) {
		return true;
	}
	iCapacity = (pJar->Capacity == 0) ?
		pJar->Config.InitialCookies : pJar->Capacity;
	while ( iCapacity < iNeed ) {
		if ( iCapacity > (SIZE_MAX / 2u) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iCapacity *= 2u;
	}
	if ( iCapacity > pJar->Config.MaxCookies ) {
		iCapacity = pJar->Config.MaxCookies;
	}
	if ( (iCapacity < iNeed) ||
		(iCapacity > (SIZE_MAX / sizeof(*pNew))) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	pNew = (xrt_cookie_entry*)xrtMalloc(iCapacity * sizeof(*pNew));
	if ( pNew == NULL ) {
		return false;
	}
	memset(pNew, 0, iCapacity * sizeof(*pNew));
	if ( pJar->Count != 0 ) {
		memcpy(pNew, pJar->Cookies,
			pJar->Count * sizeof(*pNew));
	}
	xrtFree(pJar->Cookies);
	pJar->Cookies = pNew;
	pJar->Capacity = iCapacity;
	return true;
}



/* 返回条目视图，集中维护内部拥有文本的长度。 */
static xstrview __xrtCookieJarEntryView(cstr sText, size_t iSize)
{
	return (xstrview){ sText, iSize };
}



/* 判断两个条目是否使用相同的存储主键。 */
static bool __xrtCookieJarSameKey(
	const xrt_cookie_entry* pLeft,
	const xrt_cookie_entry* pRight
)
{
	return (((pLeft->Flags ^ pRight->Flags) &
		XCOOKIE_INFO_HOST_ONLY) == 0) &&
		__xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Name, pLeft->NameSize),
		__xrtCookieJarEntryView(pRight->Name, pRight->NameSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Domain, pLeft->DomainSize),
		__xrtCookieJarEntryView(pRight->Domain, pRight->DomainSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(pLeft->Path, pLeft->PathSize),
		__xrtCookieJarEntryView(pRight->Path, pRight->PathSize)
	) && __xrtCookieJarTextEqual(
		__xrtCookieJarEntryView(
			pLeft->PartitionKey, pLeft->PartitionKeySize
		),
		__xrtCookieJarEntryView(
			pRight->PartitionKey, pRight->PartitionKeySize
		)
	);
}



/* 在锁内查找相同名称、域、HostOnly、路径和分区键的条目。 */
static size_t __xrtCookieJarFindLocked(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pNeedle
)
{
	size_t i;

	for ( i = 0; i < pJar->Count; i++ ) {
		if ( __xrtCookieJarSameKey(&pJar->Cookies[i], pNeedle) ) {
			return i;
		}
	}
	return XRT_NPOS;
}



/* Priority 未指定时按 Medium 参与容量淘汰。 */
static int __xrtCookieJarPriority(const xrt_cookie_entry* pCookie)
{
	if ( pCookie->Priority == XCOOKIE_PRIORITY_LOW ) {
		return 1;
	}
	if ( pCookie->Priority == XCOOKIE_PRIORITY_HIGH ) {
		return 3;
	}
	return 2;
}



/* 淘汰最低优先级、最久未访问的一个候选条目。 */
static bool __xrtCookieJarEvictLocked(
	xcookiejar* pJar,
	xstrview Domain,
	bool bFilter
)
{
	size_t i;
	size_t iCandidate = XRT_NPOS;

	for ( i = 0; i < pJar->Count; i++ ) {
		xrt_cookie_entry* pCookie = &pJar->Cookies[i];

		if ( bFilter && !__xrtCookieJarTextEqual(
			__xrtCookieJarEntryView(pCookie->Domain, pCookie->DomainSize),
			Domain
		) ) {
			continue;
		}
		if ( (iCandidate == XRT_NPOS) ||
			(__xrtCookieJarPriority(pCookie) <
			 __xrtCookieJarPriority(&pJar->Cookies[iCandidate])) ||
			((__xrtCookieJarPriority(pCookie) ==
			  __xrtCookieJarPriority(&pJar->Cookies[iCandidate])) &&
			 (pCookie->Accessed < pJar->Cookies[iCandidate].Accessed)) ||
			((pCookie->Accessed == pJar->Cookies[iCandidate].Accessed) &&
			 (pCookie->CreationOrder <
			  pJar->Cookies[iCandidate].CreationOrder)) ) {
			iCandidate = i;
		}
	}
	if ( iCandidate == XRT_NPOS ) {
		return false;
	}
	__xrtCookieJarRemoveAt(pJar, iCandidate);
	return true;
}



/* 复制 Cookie 的全部文本到一个紧凑拥有块。 */
static bool __xrtCookieJarEntryCopy(
	xrt_cookie_entry* pEntry,
	xstrview Name,
	xstrview Value,
	xstrview Domain,
	xstrview Path,
	xstrview PartitionKey
)
{
	xstrview Parts[5];
	str* Outputs[5];
	size_t* Sizes[5];
	size_t i;
	size_t iTotal = 0;
	str sCurrent;

	Parts[0] = Name;
	Parts[1] = Value;
	Parts[2] = Domain;
	Parts[3] = Path;
	Parts[4] = PartitionKey;
	Outputs[0] = &pEntry->Name;
	Outputs[1] = &pEntry->Value;
	Outputs[2] = &pEntry->Domain;
	Outputs[3] = &pEntry->Path;
	Outputs[4] = &pEntry->PartitionKey;
	Sizes[0] = &pEntry->NameSize;
	Sizes[1] = &pEntry->ValueSize;
	Sizes[2] = &pEntry->DomainSize;
	Sizes[3] = &pEntry->PathSize;
	Sizes[4] = &pEntry->PartitionKeySize;
	for ( i = 0; i < 5u; i++ ) {
		if ( (Parts[i].Size == SIZE_MAX) ||
			(iTotal > (SIZE_MAX - Parts[i].Size - 1u)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iTotal += Parts[i].Size + 1u;
	}
	pEntry->Storage = (str)xrtMalloc(iTotal);
	if ( pEntry->Storage == NULL ) {
		return false;
	}
	sCurrent = pEntry->Storage;
	for ( i = 0; i < 5u; i++ ) {
		size_t j;

		*Outputs[i] = sCurrent;
		*Sizes[i] = Parts[i].Size;
		for ( j = 0; j < Parts[i].Size; j++ ) {
			sCurrent[j] = (i == 2u) ?
				(char)__xhttpAsciiLower(
					(unsigned char)Parts[i].Data[j]
				) : Parts[i].Data[j];
		}
		sCurrent[Parts[i].Size] = '\0';
		sCurrent += Parts[i].Size + 1u;
	}
	return true;
}



/* 安全计算当前时间加固定时长，并钳制到 xtime 上界。 */
static xtime __xrtCookieJarTimeAdd(xtime iNow, xtime iDuration)
{
	if ( iNow > (INT64_MAX - iDuration) ) {
		return INT64_MAX;
	}
	return iNow + iDuration;
}



/* 返回配置允许的最晚持久化时间。 */
static xtime __xrtCookieJarLatestExpiry(xtime iNow)
{
	return __xrtCookieJarTimeAdd(iNow, XCOOKIE_JAR_MAX_LIFETIME);
}



/* 把策略拒绝结果统一写入可选输出。 */
static xcookiestorestatus __xrtCookieJarReject(
	xcookiereject Reason,
	xcookiereject* pReject
)
{
	if ( pReject != NULL ) {
		*pReject = Reason;
	}
	return XCOOKIE_STORE_REJECTED;
}



/* 检查配置中的大小关系和未知标志。 */
static bool __xrtCookieJarConfigValid(const xcookiejarconfig* pConfig)
{
	return ((pConfig->Flags & ~XCOOKIE_JAR_ALLOW_DOMAIN_WITHOUT_PSL) == 0) &&
		(pConfig->InitialCookies != 0) &&
		(pConfig->MaxCookies != 0) &&
		(pConfig->InitialCookies <= pConfig->MaxCookies) &&
		(pConfig->MaxCookiesPerDomain != 0) &&
		(pConfig->MaxCookiesPerDomain <= pConfig->MaxCookies) &&
		(pConfig->MaxCookieBytes != 0) &&
		(pConfig->MaxNameBytes != 0) &&
		(pConfig->MaxValueBytes != 0) &&
		(pConfig->MaxDomainBytes != 0) &&
		(pConfig->MaxPathBytes != 0) &&
		(pConfig->MaxPartitionKeyBytes != 0) &&
		(pConfig->LaxUnsafeAge >= 0);
}



/* 初始化通用客户端配置。 */
XHTTP_POLICY_API void xrtCookieJarConfigInit(xcookiejarconfig* pConfig)
{
	xcookiejarconfig Config;

	if ( pConfig == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.InitialCookies = 16;
	Config.MaxCookies = 3000;
	Config.MaxCookiesPerDomain = 180;
	Config.MaxCookieBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxNameBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxValueBytes = XSET_COOKIE_MAX_PAIR_BYTES;
	Config.MaxDomainBytes = 253;
	Config.MaxPathBytes = 1024;
	Config.MaxPartitionKeyBytes = 1024;
	Config.LaxUnsafeAge = 2 * XRT_TIME_MINUTE;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 创建 CookieJar 并初始化系统互斥锁。 */
XHTTP_POLICY_API xcookiejar* xrtCookieJarCreate(const xcookiejarconfig* pConfig)
{
	xcookiejarconfig Config;
	xcookiejar* pJar;

	xrtCookieJarConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !__xrtCookieJarConfigValid(&Config) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pJar = (xcookiejar*)xrtMalloc(sizeof(*pJar));
	if ( pJar == NULL ) {
		return NULL;
	}
	memset(pJar, 0, sizeof(*pJar));
	pJar->RefCount = 1;
	pJar->Config = Config;
	if ( !xrtMutexInit(&pJar->Lock) ) {
		xrtFree(pJar);
		return NULL;
	}
	return pJar;
}



/* 增加 Jar 引用。 */
XHTTP_POLICY_API xcookiejar* xrtCookieJarRetain(const xcookiejar* pJar)
{
	if ( (pJar == NULL) ||
		(xrtRefRetain(&((xcookiejar*)pJar)->RefCount) < 0) ) {
		if ( pJar == NULL ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return NULL;
	}
	return (xcookiejar*)pJar;
}



/* 释放 Jar 最后一个引用及其拥有资源。 */
XHTTP_POLICY_API void xrtCookieJarRelease(xcookiejar* pJar)
{
	size_t i;

	if ( (pJar == NULL) || (xrtRefRelease(&pJar->RefCount) != 0) ) {
		return;
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		__xrtCookieJarEntryClear(&pJar->Cookies[i]);
	}
	xrtFree(pJar->Cookies);
	(void)xrtMutexUnit(&pJar->Lock);
	memset(pJar, 0, sizeof(*pJar));
	xrtFree(pJar);
}



/* 清空 Jar 内容并保留容量。 */
XHTTP_POLICY_API void xrtCookieJarClear(xcookiejar* pJar)
{
	size_t i;

	if ( pJar == NULL ) {
		return;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return;
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		__xrtCookieJarEntryClear(&pJar->Cookies[i]);
	}
	pJar->Count = 0;
	(void)xrtMutexUnlock(&pJar->Lock);
}



/* 读取当前条目数。 */
XHTTP_POLICY_API size_t xrtCookieJarCount(const xcookiejar* pJar)
{
	size_t iCount;

	if ( pJar == NULL ) {
		return 0;
	}
	if ( !xrtMutexLock(&((xcookiejar*)pJar)->Lock) ) {
		return 0;
	}
	iCount = pJar->Count;
	(void)xrtMutexUnlock(&((xcookiejar*)pJar)->Lock);
	return iCount;
}



/* 清理指定时间已经过期的条目。 */
XHTTP_POLICY_API size_t xrtCookieJarPurge(xcookiejar* pJar, xtime iNow)
{
	size_t iRemoved;

	if ( pJar == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return 0;
	}
	iRemoved = __xrtCookieJarPurgeLocked(pJar, iNow);
	(void)xrtMutexUnlock(&pJar->Lock);
	return iRemoved;
}



/* 检查 Set 输入的视图和标志，区分参数错误与策略拒绝。 */
static bool __xrtCookieJarSetInputValid(
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	const xcookiereject* pReject
)
{
	const uint32 iContextFlags = XCOOKIE_STORE_HTTP_API |
		XCOOKIE_STORE_HAS_NOW | XCOOKIE_STORE_SAME_SITE |
		XCOOKIE_STORE_TOP_LEVEL;
	const uint32 iCookieFlags = XSET_COOKIE_HAS_DOMAIN |
		XSET_COOKIE_HAS_PATH | XSET_COOKIE_HAS_EXPIRES |
		XSET_COOKIE_HAS_MAX_AGE | XSET_COOKIE_HAS_SAME_SITE |
		XSET_COOKIE_SECURE | XSET_COOKIE_HTTP_ONLY |
		XSET_COOKIE_PARTITIONED | XSET_COOKIE_HAS_PRIORITY;

	if ( (pContext == NULL) || (pCookie == NULL) ||
		((pContext->Flags & ~iContextFlags) != 0) ||
		((pCookie->Flags & ~iCookieFlags) != 0) ||
		!__xrtCookieJarViewValid(pContext->URL) ||
		!__xrtCookieJarPartitionValid(pContext->PartitionKey) ||
		!__xrtCookieJarViewValid(pCookie->Name) ||
		!__xrtCookieJarViewValid(pCookie->Value) ||
		!__xrtCookieJarViewValid(pCookie->Domain) ||
		!__xrtCookieJarViewValid(pCookie->Path) ||
		((pReject != NULL) && (
		 xrtMemRangesOverlap(
			pReject, sizeof(*pReject), pContext, sizeof(*pContext)
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject), pCookie, sizeof(*pCookie)
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->URL.Data, pContext->URL.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Name.Data, pCookie->Name.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Value.Data, pCookie->Value.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Domain.Data, pCookie->Domain.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pCookie->Path.Data, pCookie->Path.Size
		 ))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 判断非安全来源是否会覆盖已有 Secure Cookie。 */
static bool __xrtCookieJarSecureOverlay(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pNew
)
{
	size_t i;

	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pOld = &pJar->Cookies[i];
		bool bDomainOverlap;
		bool bPathOverlap;

		if ( ((pOld->Flags & XCOOKIE_INFO_SECURE) == 0) ||
			!__xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(pOld->Name, pOld->NameSize),
				__xrtCookieJarEntryView(pNew->Name, pNew->NameSize)
			) || !__xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(
					pOld->PartitionKey, pOld->PartitionKeySize
				),
				__xrtCookieJarEntryView(
					pNew->PartitionKey, pNew->PartitionKeySize
				)
			) ) {
			continue;
		}
		bDomainOverlap = __xrtCookieJarDomainMatch(
			__xrtCookieJarEntryView(pOld->Domain, pOld->DomainSize),
			__xrtCookieJarEntryView(pNew->Domain, pNew->DomainSize)
		) || __xrtCookieJarDomainMatch(
			__xrtCookieJarEntryView(pNew->Domain, pNew->DomainSize),
			__xrtCookieJarEntryView(pOld->Domain, pOld->DomainSize)
		);
		bPathOverlap = __xrtCookieJarPathMatch(
			__xrtCookieJarEntryView(pOld->Path, pOld->PathSize),
			__xrtCookieJarEntryView(pNew->Path, pNew->PathSize)
		) || __xrtCookieJarPathMatch(
			__xrtCookieJarEntryView(pNew->Path, pNew->PathSize),
			__xrtCookieJarEntryView(pOld->Path, pOld->PathSize)
		);
		if ( bDomainOverlap && bPathOverlap ) {
			return true;
		}
	}
	return false;
}



/* 使用已解析 Set-Cookie 执行完整用户代理存储算法。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarSet(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xsetcookie* pCookie,
	xcookiereject* pReject
)
{
	xrt_cookie_origin Origin;
	xrt_cookie_entry Entry;
	xstrview Host;
	xstrview Domain;
	xstrview Path;
	xstrview PartitionKey = { NULL, 0 };
	xstrview PrefixSubject;
	xcookiesamesite SameSite;
	char sDomain[254];
	xtime iNow;
	xtime iLatest;
	bool bHostOnly = true;
	bool bDelete = false;
	size_t iExisting;
	size_t iDomainCount = 0;
	size_t i;

	if ( (pJar == NULL) ||
		!__xrtCookieJarSetInputValid(pContext, pCookie, pReject) ) {
		return XCOOKIE_STORE_ERROR;
	}
	if ( pReject != NULL ) {
		*pReject = XCOOKIE_REJECT_NONE;
	}
	if ( !__xrtCookieOriginParse(pContext->URL, &Origin) ) {
		return XCOOKIE_STORE_ERROR;
	}
	memset(&Entry, 0, sizeof(Entry));
	Host = (xstrview){ Origin.Host, Origin.HostSize };
	iNow = ((pContext->Flags & XCOOKIE_STORE_HAS_NOW) != 0) ?
		pContext->Now : xrtNow();
	iLatest = __xrtCookieJarLatestExpiry(iNow);
	SameSite = ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) ?
		pCookie->SameSite : XCOOKIE_SAME_SITE_DEFAULT;

	if ( !__xrtCookieJarPairValid(pCookie) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		 ((SameSite < XCOOKIE_SAME_SITE_LAX) ||
		  (SameSite > XCOOKIE_SAME_SITE_NONE))) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) == 0) &&
		 (pCookie->SameSite != XCOOKIE_SAME_SITE_DEFAULT)) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) &&
		 ((pCookie->Priority < XCOOKIE_PRIORITY_LOW) ||
		  (pCookie->Priority > XCOOKIE_PRIORITY_HIGH))) ||
		(((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) == 0) &&
		 (pCookie->Priority != XCOOKIE_PRIORITY_UNSPECIFIED)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SYNTAX, pReject);
	}
	if ( ((pCookie->Name.Size == 0) && (pCookie->Value.Size == 0)) ||
		((pCookie->Name.Size == 0) &&
		 (memchr(pCookie->Value.Data, '=', pCookie->Value.Size) != NULL)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_EMPTY, pReject);
	}
	if ( (pCookie->Name.Size > pJar->Config.MaxNameBytes) ||
		(pCookie->Value.Size > pJar->Config.MaxValueBytes) ||
		(pCookie->Name.Size > pJar->Config.MaxCookieBytes) ||
		(pCookie->Value.Size >
		 (pJar->Config.MaxCookieBytes - pCookie->Name.Size)) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_LIMIT, pReject);
	}

	Domain = Host;
	if ( (pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0 ) {
		Domain = pCookie->Domain;
		if ( (Domain.Size > pJar->Config.MaxDomainBytes) ||
			!__xrtCookieJarDomainValid(Domain) || Origin.IpHost ||
			!__xrtCookieJarDomainMatch(Host, Domain) ) {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_DOMAIN, pReject
			);
		}
		for ( i = 0; i < Domain.Size; i++ ) {
			sDomain[i] = (char)__xhttpAsciiLower(
				(unsigned char)Domain.Data[i]
			);
		}
		Domain = (xstrview){ sDomain, Domain.Size };
		if ( pJar->Config.IsPublicSuffix != NULL ) {
			if ( pJar->Config.IsPublicSuffix(
				pJar->Config.PublicSuffixContext, Domain
			) ) {
				if ( !__xrtCookieJarAsciiEqual(Host, Domain) ) {
					__xrtCookieOriginClear(&Origin);
					return __xrtCookieJarReject(
						XCOOKIE_REJECT_PUBLIC_SUFFIX, pReject
					);
				}
				bHostOnly = true;
			} else {
				bHostOnly = false;
			}
		} else if ( __xrtCookieJarAsciiEqual(Host, Domain) ) {
			bHostOnly = true;
		} else if ( (pJar->Config.Flags &
			XCOOKIE_JAR_ALLOW_DOMAIN_WITHOUT_PSL) != 0 ) {
			bHostOnly = false;
		} else {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_PUBLIC_SUFFIX, pReject
			);
		}
	}
	Path = (((pCookie->Flags & XSET_COOKIE_HAS_PATH) != 0) &&
		(pCookie->Path.Size != 0) && (pCookie->Path.Data[0] == '/')) ?
		pCookie->Path : __xrtCookieJarDefaultPath(Origin.Path);
	if ( Path.Size > pJar->Config.MaxPathBytes ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_LIMIT, pReject);
	}

	if ( ((pCookie->Flags & XSET_COOKIE_SECURE) != 0) &&
		!Origin.Secure ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SECURE, pReject);
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_STORE_HTTP_API) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_HTTP_ONLY, pReject);
	}
	if ( ((pCookie->Flags & XSET_COOKIE_HAS_SAME_SITE) != 0) &&
		(pCookie->SameSite == XCOOKIE_SAME_SITE_NONE) &&
		((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SAME_SITE, pReject);
	}
	if ( (SameSite != XCOOKIE_SAME_SITE_NONE) &&
		((pContext->Flags & XCOOKIE_STORE_SAME_SITE) == 0) &&
		((pContext->Flags & XCOOKIE_STORE_TOP_LEVEL) == 0) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_SAME_SITE, pReject);
	}
	if ( (pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0 ) {
		if ( ((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
			(pContext->PartitionKey.Size == 0) ||
			(pContext->PartitionKey.Size >
			 pJar->Config.MaxPartitionKeyBytes) ) {
			__xrtCookieOriginClear(&Origin);
			return __xrtCookieJarReject(
				XCOOKIE_REJECT_PARTITION, pReject
			);
		}
		PartitionKey = pContext->PartitionKey;
	}
	PrefixSubject = (pCookie->Name.Size != 0) ?
		pCookie->Name : pCookie->Value;
	if ( __xrtCookieJarPrefix(
		PrefixSubject, "__Secure-", 9u
	) && (((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
		 !Origin.Secure) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_PREFIX, pReject);
	}
	if ( __xrtCookieJarPrefix(
		PrefixSubject, "__Host-", 7u
	) && (((pCookie->Flags & XSET_COOKIE_SECURE) == 0) ||
		 !Origin.Secure || !bHostOnly ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_DOMAIN) != 0) ||
		 ((pCookie->Flags & XSET_COOKIE_HAS_PATH) == 0) ||
		 (Path.Size != 1u) || (Path.Data[0] != '/')) ) {
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(XCOOKIE_REJECT_PREFIX, pReject);
	}

	if ( !__xrtCookieJarEntryCopy(
		&Entry, pCookie->Name, pCookie->Value, Domain, Path, PartitionKey
	) ) {
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_ERROR;
	}
	if ( bHostOnly ) {
		Entry.Flags |= XCOOKIE_INFO_HOST_ONLY;
	}
	if ( (pCookie->Flags & XSET_COOKIE_SECURE) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_SECURE;
	}
	if ( (pCookie->Flags & XSET_COOKIE_HTTP_ONLY) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_HTTP_ONLY;
	}
	if ( (pCookie->Flags & XSET_COOKIE_PARTITIONED) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PARTITIONED;
	}
	Entry.SameSite = SameSite;
	Entry.Priority = ((pCookie->Flags & XSET_COOKIE_HAS_PRIORITY) != 0) ?
		pCookie->Priority : XCOOKIE_PRIORITY_UNSPECIFIED;
	Entry.Created = iNow;
	Entry.Accessed = iNow;
	if ( (pCookie->Flags & XSET_COOKIE_HAS_MAX_AGE) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PERSISTENT;
		if ( pCookie->MaxAge <= 0 ) {
			Entry.Expires = iNow;
			bDelete = true;
		} else {
			xtime iDuration = (pCookie->MaxAge >
				(INT64_MAX / XRT_TIME_SECOND)) ? INT64_MAX :
				(pCookie->MaxAge * XRT_TIME_SECOND);

			Entry.Expires = __xrtCookieJarTimeAdd(iNow, iDuration);
			if ( Entry.Expires > iLatest ) {
				Entry.Expires = iLatest;
			}
		}
	} else if ( (pCookie->Flags & XSET_COOKIE_HAS_EXPIRES) != 0 ) {
		Entry.Flags |= XCOOKIE_INFO_PERSISTENT;
		Entry.Expires = pCookie->Expires;
		if ( Entry.Expires > iLatest ) {
			Entry.Expires = iLatest;
		}
		bDelete = Entry.Expires <= iNow;
	}

	if ( !xrtMutexLock(&pJar->Lock) ) {
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_ERROR;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( !Origin.Secure &&
		((Entry.Flags & XCOOKIE_INFO_SECURE) == 0) &&
		__xrtCookieJarSecureOverlay(pJar, &Entry) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(
			XCOOKIE_REJECT_SECURE_OVERWRITE, pReject
		);
	}
	iExisting = __xrtCookieJarFindLocked(pJar, &Entry);
	if ( (iExisting != XRT_NPOS) &&
		((pJar->Cookies[iExisting].Flags & XCOOKIE_INFO_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_STORE_HTTP_API) == 0) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return __xrtCookieJarReject(
			XCOOKIE_REJECT_HTTP_ONLY, pReject
		);
	}
	if ( bDelete ) {
		if ( iExisting != XRT_NPOS ) {
			__xrtCookieJarRemoveAt(pJar, iExisting);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_REMOVED;
		}
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieJarEntryClear(&Entry);
		__xrtCookieOriginClear(&Origin);
		return XCOOKIE_STORE_IGNORED;
	}

	if ( iExisting != XRT_NPOS ) {
		Entry.Created = pJar->Cookies[iExisting].Created;
		Entry.CreationOrder =
			pJar->Cookies[iExisting].CreationOrder;
		__xrtCookieJarEntryClear(&pJar->Cookies[iExisting]);
		pJar->Cookies[iExisting] = Entry;
	} else {
		xstrview EntryDomain = __xrtCookieJarEntryView(
			Entry.Domain, Entry.DomainSize
		);

		if ( (pJar->Count < pJar->Config.MaxCookies) &&
			!__xrtCookieJarReserve(pJar, pJar->Count + 1u) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_ERROR;
		}
		for ( i = 0; i < pJar->Count; i++ ) {
			if ( __xrtCookieJarTextEqual(
				__xrtCookieJarEntryView(
					pJar->Cookies[i].Domain,
					pJar->Cookies[i].DomainSize
				), EntryDomain
			) ) {
				iDomainCount++;
			}
		}
		while ( iDomainCount >= pJar->Config.MaxCookiesPerDomain ) {
			if ( !__xrtCookieJarEvictLocked(
				pJar, EntryDomain, true
			) ) {
				break;
			}
			iDomainCount--;
		}
		while ( pJar->Count >= pJar->Config.MaxCookies ) {
			if ( !__xrtCookieJarEvictLocked(
				pJar, (xstrview){ NULL, 0 }, false
			) ) {
				break;
			}
		}
		if ( !__xrtCookieJarReserve(pJar, pJar->Count + 1u) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieJarEntryClear(&Entry);
			__xrtCookieOriginClear(&Origin);
			return XCOOKIE_STORE_ERROR;
		}
		pJar->Sequence++;
		if ( pJar->Sequence == 0 ) {
			pJar->Sequence = 1;
		}
		Entry.CreationOrder = pJar->Sequence;
		pJar->Cookies[pJar->Count++] = Entry;
	}
	(void)xrtMutexUnlock(&pJar->Lock);
	__xrtCookieOriginClear(&Origin);
	return XCOOKIE_STORE_STORED;
}



/* 解析字段值后调用结构化存储路径。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarStore(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	xstrview SetCookie,
	xcookiereject* pReject
)
{
	xsetcookie Cookie;

	if ( (pJar == NULL) || (pContext == NULL) ||
		!__xrtCookieJarViewValid(SetCookie) ||
		((pReject != NULL) && (
		 xrtMemRangesOverlap(
			pReject, sizeof(*pReject), pContext, sizeof(*pContext)
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject), SetCookie.Data, SetCookie.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->URL.Data, pContext->URL.Size
		 ) || xrtMemRangesOverlap(
			pReject, sizeof(*pReject),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XCOOKIE_STORE_ERROR;
	}
	if ( pReject != NULL ) {
		*pReject = XCOOKIE_REJECT_NONE;
	}
	if ( !xrtSetCookieParse(SetCookie, &Cookie) ) {
		return __xrtCookieJarReject(XCOOKIE_REJECT_SYNTAX, pReject);
	}
	return xrtCookieJarSet(pJar, pContext, &Cookie, pReject);
}



/* 提供普通 HTTP 响应的低心智负担存储路径。 */
XHTTP_POLICY_API xcookiestorestatus xrtCookieJarStoreUrl(
	xcookiejar* pJar,
	xstrview URL,
	xstrview SetCookie,
	xcookiereject* pReject
)
{
	xcookiestorecontext Context;

	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API | XCOOKIE_STORE_SAME_SITE;
	Context.URL = URL;
	return xrtCookieJarStore(pJar, &Context, SetCookie, pReject);
}



/* 判断 SameSite 模式是否允许当前请求发送。 */
static bool __xrtCookieJarSameSiteAllowed(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pCookie,
	const xcookierequestcontext* pContext,
	xtime iNow
)
{
	bool bSameSite =
		(pContext->Flags & XCOOKIE_REQUEST_SAME_SITE) != 0;
	bool bTopLevel =
		(pContext->Flags & XCOOKIE_REQUEST_TOP_LEVEL) != 0;
	bool bSafe =
		(pContext->Flags & XCOOKIE_REQUEST_SAFE_METHOD) != 0;

	if ( bSameSite || (pCookie->SameSite == XCOOKIE_SAME_SITE_NONE) ) {
		return true;
	}
	if ( pCookie->SameSite == XCOOKIE_SAME_SITE_STRICT ) {
		return false;
	}
	if ( bTopLevel && bSafe ) {
		return true;
	}
	if ( (pCookie->SameSite == XCOOKIE_SAME_SITE_DEFAULT) &&
		bTopLevel && (pJar->Config.LaxUnsafeAge != 0) &&
		(iNow >= pCookie->Created) &&
		(((uint64)iNow - (uint64)pCookie->Created) <=
		 (uint64)pJar->Config.LaxUnsafeAge) ) {
		return true;
	}
	return false;
}



/* 判断一个条目是否可用于当前请求。 */
static bool __xrtCookieJarEligible(
	const xcookiejar* pJar,
	const xrt_cookie_entry* pCookie,
	const xrt_cookie_origin* pOrigin,
	const xcookierequestcontext* pContext,
	xtime iNow
)
{
	xstrview Host = { pOrigin->Host, pOrigin->HostSize };
	xstrview Domain = { pCookie->Domain, pCookie->DomainSize };

	if ( ((pCookie->Flags & XCOOKIE_INFO_PERSISTENT) != 0) &&
		(pCookie->Expires <= iNow) ) {
		return false;
	}
	if ( ((pCookie->Flags & XCOOKIE_INFO_SECURE) != 0) &&
		!pOrigin->Secure ) {
		return false;
	}
	if ( ((pCookie->Flags & XCOOKIE_INFO_HTTP_ONLY) != 0) &&
		((pContext->Flags & XCOOKIE_REQUEST_HTTP_API) == 0) ) {
		return false;
	}
	if ( (pCookie->Flags & XCOOKIE_INFO_HOST_ONLY) != 0 ) {
		if ( !__xrtCookieJarTextEqual(Host, Domain) ) {
			return false;
		}
	} else if ( !__xrtCookieJarDomainMatch(Host, Domain) ) {
		return false;
	}
	if ( !__xrtCookieJarPathMatch(
		pOrigin->Path,
		__xrtCookieJarEntryView(pCookie->Path, pCookie->PathSize)
	) ) {
		return false;
	}
	if ( (pCookie->Flags & XCOOKIE_INFO_PARTITIONED) != 0 ) {
		if ( !__xrtCookieJarTextEqual(
			pContext->PartitionKey,
			__xrtCookieJarEntryView(
				pCookie->PartitionKey, pCookie->PartitionKeySize
			)
		) ) {
			return false;
		}
	}
	return __xrtCookieJarSameSiteAllowed(
		pJar, pCookie, pContext, iNow
	);
}



/* qsort 比较器实现最长 Path 优先、较早创建优先。 */
static int __xrtCookieJarSelectionCompare(const void* pLeft, const void* pRight)
{
	const xrt_cookie_entry* pA = *(xrt_cookie_entry* const*)pLeft;
	const xrt_cookie_entry* pB = *(xrt_cookie_entry* const*)pRight;

	if ( pA->PathSize != pB->PathSize ) {
		return (pA->PathSize > pB->PathSize) ? -1 : 1;
	}
	if ( pA->CreationOrder == pB->CreationOrder ) {
		return 0;
	}
	return (pA->CreationOrder < pB->CreationOrder) ? -1 : 1;
}



/* 在锁内选择、排序并精确计算一个 Cookie 字段值。 */
static bool __xrtCookieJarSelectLocked(
	xcookiejar* pJar,
	const xrt_cookie_origin* pOrigin,
	const xcookierequestcontext* pContext,
	xtime iNow,
	xrt_cookie_selection* pSelection
)
{
	size_t i;

	memset(pSelection, 0, sizeof(*pSelection));
	if ( pJar->Count != 0 ) {
		if ( pJar->Count >
			(SIZE_MAX / sizeof(*pSelection->Items)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		pSelection->Items = (xrt_cookie_entry**)xrtMalloc(
			pJar->Count * sizeof(*pSelection->Items)
		);
		if ( pSelection->Items == NULL ) {
			return false;
		}
	}
	for ( i = 0; i < pJar->Count; i++ ) {
		if ( __xrtCookieJarEligible(
			pJar, &pJar->Cookies[i], pOrigin, pContext, iNow
		) ) {
			pSelection->Items[pSelection->Count++] = &pJar->Cookies[i];
		}
	}
	if ( pSelection->Count > 1u ) {
		qsort(
			pSelection->Items, pSelection->Count,
			sizeof(*pSelection->Items), __xrtCookieJarSelectionCompare
		);
	}
	for ( i = 0; i < pSelection->Count; i++ ) {
		xrt_cookie_entry* pCookie = pSelection->Items[i];
		size_t iExtra;

		if ( (pCookie->ValueSize == SIZE_MAX) ||
			(pCookie->NameSize >
			 (SIZE_MAX - pCookie->ValueSize - 1u)) ) {
			xrtFree(pSelection->Items);
			memset(pSelection, 0, sizeof(*pSelection));
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iExtra = pCookie->NameSize + pCookie->ValueSize + 1u;
		if ( i != 0 ) {
			if ( iExtra > (SIZE_MAX - 2u) ) {
				xrtFree(pSelection->Items);
				memset(pSelection, 0, sizeof(*pSelection));
				xrtSetErrorKind(XERR_RANGE);
				return false;
			}
			iExtra += 2u;
		}
		if ( pSelection->Size > (SIZE_MAX - iExtra) ) {
			xrtFree(pSelection->Items);
			memset(pSelection, 0, sizeof(*pSelection));
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		pSelection->Size += iExtra;
	}
	return true;
}



/* 写出已经选择的条目并更新实际发送条目的访问时间。 */
static void __xrtCookieJarSelectionWrite(
	const xrt_cookie_selection* pSelection,
	str sOutput,
	xtime iNow
)
{
	size_t i;
	size_t iOffset = 0;

	for ( i = 0; i < pSelection->Count; i++ ) {
		xrt_cookie_entry* pCookie = pSelection->Items[i];

		if ( i != 0 ) {
			memcpy(sOutput + iOffset, "; ", 2u);
			iOffset += 2u;
		}
		memcpy(sOutput + iOffset, pCookie->Name, pCookie->NameSize);
		iOffset += pCookie->NameSize;
		sOutput[iOffset++] = '=';
		memcpy(sOutput + iOffset, pCookie->Value, pCookie->ValueSize);
		iOffset += pCookie->ValueSize;
		pCookie->Accessed = iNow;
	}
}



/* 共用单次加锁的写入和分配构建路径。 */
static bool __xrtCookieJarRender(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	str* psAllocated
)
{
	const uint32 iFlags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_HAS_NOW | XCOOKIE_REQUEST_SAME_SITE |
		XCOOKIE_REQUEST_TOP_LEVEL | XCOOKIE_REQUEST_SAFE_METHOD;
	xrt_cookie_origin Origin;
	xrt_cookie_selection Selection;
	xtime iNow;
	str sOutput = (str)pOutput;

	if ( (pJar == NULL) || (pContext == NULL) || (pSize == NULL) ||
		((pContext->Flags & ~iFlags) != 0) ||
		!__xrtCookieJarViewValid(pContext->URL) ||
		!__xrtCookieJarPartitionValid(pContext->PartitionKey) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((psAllocated != NULL) && (pOutput != NULL)) ||
		xrtMemRangesOverlap(
			pSize, sizeof(*pSize), pContext, sizeof(*pContext)
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pContext->URL.Data, pContext->URL.Size
		) || xrtMemRangesOverlap(
			pSize, sizeof(*pSize),
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		) || ((pOutput != NULL) && (
		 xrtMemRangesOverlap(pOutput, iCapacity, pSize, sizeof(*pSize)) ||
		 xrtMemRangesOverlap(
			pOutput, iCapacity, pContext, sizeof(*pContext)
		 ) || xrtMemRangesOverlap(
			pOutput, iCapacity,
			pContext->URL.Data, pContext->URL.Size
		 ) || xrtMemRangesOverlap(
			pOutput, iCapacity,
			pContext->PartitionKey.Data, pContext->PartitionKey.Size
		 ))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pSize = 0;
	if ( psAllocated != NULL ) {
		*psAllocated = NULL;
	}
	if ( !__xrtCookieOriginParse(pContext->URL, &Origin) ) {
		return false;
	}
	iNow = ((pContext->Flags & XCOOKIE_REQUEST_HAS_NOW) != 0) ?
		pContext->Now : xrtNow();
	if ( !xrtMutexLock(&pJar->Lock) ) {
		__xrtCookieOriginClear(&Origin);
		return false;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( !__xrtCookieJarSelectLocked(
		pJar, &Origin, pContext, iNow, &Selection
	) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieOriginClear(&Origin);
		return false;
	}
	*pSize = Selection.Size;
	if ( psAllocated != NULL ) {
		if ( Selection.Size == SIZE_MAX ) {
			xrtFree(Selection.Items);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieOriginClear(&Origin);
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		sOutput = (str)xrtMalloc(Selection.Size + 1u);
		if ( sOutput == NULL ) {
			xrtFree(Selection.Items);
			(void)xrtMutexUnlock(&pJar->Lock);
			__xrtCookieOriginClear(&Origin);
			return false;
		}
	} else if ( (pOutput == NULL) || (iCapacity < Selection.Size) ) {
		xrtFree(Selection.Items);
		(void)xrtMutexUnlock(&pJar->Lock);
		__xrtCookieOriginClear(&Origin);
		if ( pOutput != NULL ) {
			xrtSetErrorKind(XERR_RANGE);
		}
		return pOutput == NULL;
	}
	__xrtCookieJarSelectionWrite(&Selection, sOutput, iNow);
	if ( psAllocated != NULL ) {
		sOutput[Selection.Size] = '\0';
		*psAllocated = sOutput;
	}
	xrtFree(Selection.Items);
	(void)xrtMutexUnlock(&pJar->Lock);
	__xrtCookieOriginClear(&Origin);
	return true;
}



/* 写出请求 Cookie 字段值。 */
XHTTP_POLICY_API bool xrtCookieJarWrite(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return __xrtCookieJarRender(
		pJar, pContext, pOutput, iCapacity, pSize, NULL
	);
}



/* 分配请求 Cookie 字段值。 */
XHTTP_POLICY_API str xrtCookieJarBuild(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	size_t* pSize
)
{
	str sOutput = NULL;
	size_t iLocalSize;

	if ( pSize == NULL ) {
		pSize = &iLocalSize;
	}
	if ( !__xrtCookieJarRender(
		pJar, pContext, NULL, 0, pSize, &sOutput
	) ) {
		return NULL;
	}
	return sOutput;
}



/* 初始化普通 URL 快捷请求上下文。 */
static void __xrtCookieJarUrlRequest(
	xcookierequestcontext* pContext,
	xstrview URL
)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Flags = XCOOKIE_REQUEST_HTTP_API |
		XCOOKIE_REQUEST_SAME_SITE | XCOOKIE_REQUEST_SAFE_METHOD;
	pContext->URL = URL;
}



/* 写出普通同站 HTTP 请求 Cookie。 */
XHTTP_POLICY_API bool xrtCookieJarWriteUrl(
	xcookiejar* pJar,
	xstrview URL,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	xcookierequestcontext Context;

	__xrtCookieJarUrlRequest(&Context, URL);
	return xrtCookieJarWrite(
		pJar, &Context, pOutput, iCapacity, pSize
	);
}



/* 构建普通同站 HTTP 请求 Cookie。 */
XHTTP_POLICY_API str xrtCookieJarBuildUrl(
	xcookiejar* pJar,
	xstrview URL,
	size_t* pSize
)
{
	xcookierequestcontext Context;

	__xrtCookieJarUrlRequest(&Context, URL);
	return xrtCookieJarBuild(pJar, &Context, pSize);
}



/* 创建拥有全部文本的并发稳定快照。 */
XHTTP_POLICY_API xcookiesnapshot* xrtCookieJarSnapshot(
	xcookiejar* pJar,
	xtime iNow
)
{
	xcookiesnapshot* pSnapshot;
	size_t iBase;
	size_t iText = 0;
	size_t iTotal;
	size_t i;
	str sText;

	if ( pJar == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( !xrtMutexLock(&pJar->Lock) ) {
		return NULL;
	}
	(void)__xrtCookieJarPurgeLocked(pJar, iNow);
	if ( pJar->Count >
		((SIZE_MAX - offsetof(xcookiesnapshot, Cookies)) /
		 sizeof(xcookieinfo)) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iBase = offsetof(xcookiesnapshot, Cookies) +
		(pJar->Count * sizeof(xcookieinfo));
	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pCookie = &pJar->Cookies[i];
		size_t Sizes[5];
		size_t iCookie = 0;
		size_t j;

		Sizes[0] = pCookie->NameSize;
		Sizes[1] = pCookie->ValueSize;
		Sizes[2] = pCookie->DomainSize;
		Sizes[3] = pCookie->PathSize;
		Sizes[4] = pCookie->PartitionKeySize;
		for ( j = 0; j < 5u; j++ ) {
			if ( (Sizes[j] == SIZE_MAX) ||
				(iCookie > (SIZE_MAX - Sizes[j] - 1u)) ) {
				(void)xrtMutexUnlock(&pJar->Lock);
				xrtSetErrorKind(XERR_RANGE);
				return NULL;
			}
			iCookie += Sizes[j] + 1u;
		}

		if ( iCookie > (SIZE_MAX - iText) ) {
			(void)xrtMutexUnlock(&pJar->Lock);
			xrtSetErrorKind(XERR_RANGE);
			return NULL;
		}
		iText += iCookie;
	}
	if ( iText > (SIZE_MAX - iBase) ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iTotal = iBase + iText;
	pSnapshot = (xcookiesnapshot*)xrtMalloc(
		(iTotal == 0) ? 1u : iTotal
	);
	if ( pSnapshot == NULL ) {
		(void)xrtMutexUnlock(&pJar->Lock);
		return NULL;
	}
	pSnapshot->Count = pJar->Count;
	sText = (str)((uint8*)pSnapshot + iBase);
	for ( i = 0; i < pJar->Count; i++ ) {
		const xrt_cookie_entry* pSource = &pJar->Cookies[i];
		xcookieinfo* pTarget = &pSnapshot->Cookies[i];
		xstrview* Views[5];
		cstr Sources[5];
		size_t Sizes[5];
		size_t j;

		memset(pTarget, 0, sizeof(*pTarget));
		pTarget->Flags = pSource->Flags;
		pTarget->SameSite = pSource->SameSite;
		pTarget->Priority = pSource->Priority;
		pTarget->Expires = pSource->Expires;
		pTarget->Created = pSource->Created;
		pTarget->Accessed = pSource->Accessed;
		Views[0] = &pTarget->Name;
		Views[1] = &pTarget->Value;
		Views[2] = &pTarget->Domain;
		Views[3] = &pTarget->Path;
		Views[4] = &pTarget->PartitionKey;
		Sources[0] = pSource->Name;
		Sources[1] = pSource->Value;
		Sources[2] = pSource->Domain;
		Sources[3] = pSource->Path;
		Sources[4] = pSource->PartitionKey;
		Sizes[0] = pSource->NameSize;
		Sizes[1] = pSource->ValueSize;
		Sizes[2] = pSource->DomainSize;
		Sizes[3] = pSource->PathSize;
		Sizes[4] = pSource->PartitionKeySize;
		for ( j = 0; j < 5u; j++ ) {
			Views[j]->Data = sText;
			Views[j]->Size = Sizes[j];
			memcpy(sText, Sources[j], Sizes[j]);
			sText[Sizes[j]] = '\0';
			sText += Sizes[j] + 1u;
		}
	}
	(void)xrtMutexUnlock(&pJar->Lock);
	return pSnapshot;
}



/* 释放快照单分配块。 */
XHTTP_POLICY_API void xrtCookieSnapshotDestroy(xcookiesnapshot* pSnapshot)
{
	xrtFree(pSnapshot);
}



/* 返回快照数量。 */
XHTTP_POLICY_API size_t xrtCookieSnapshotCount(const xcookiesnapshot* pSnapshot)
{
	return (pSnapshot == NULL) ? 0 : pSnapshot->Count;
}



/* 返回快照中的稳定借用条目。 */
XHTTP_POLICY_API const xcookieinfo* xrtCookieSnapshotAt(
	const xcookiesnapshot* pSnapshot,
	size_t iIndex
)
{
	if ( (pSnapshot == NULL) || (iIndex >= pSnapshot->Count) ) {
		return NULL;
	}
	return &pSnapshot->Cookies[iIndex];
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/cookie_jar_headers.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)



#if defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS)

/* 批量接收独立 Set-Cookie 字段并保留已经提交的结果。 */
XHTTP_POLICY_API bool xrtCookieJarStoreHeaders(
	xcookiejar* pJar,
	const xcookiestorecontext* pContext,
	const xhttpheaders* pHeaders,
	xcookiestorereport* pReport
)
{
	xcookiestorereport Report;
	size_t i;

	memset(&Report, 0, sizeof(Report));
	if ( (pJar == NULL) || (pContext == NULL) || (pHeaders == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( i = 0; i < xrtHttpHeadersCount(pHeaders); i++ ) {
		const xhttpfield* pField = xrtHttpHeadersAt(pHeaders, i);
		xcookiestorestatus Status;

		if ( (pField == NULL) || !xrtHttpFieldNameEqual(
			pField->Name, XRT_STR_LITERAL("Set-Cookie")
		) ) {
			continue;
		}
		Report.Fields++;
		Status = xrtCookieJarStore(
			pJar, pContext, pField->Value, NULL
		);
		if ( Status == XCOOKIE_STORE_ERROR ) {
			if ( pReport != NULL ) {
				*pReport = Report;
			}
			return false;
		}
		if ( Status == XCOOKIE_STORE_STORED ) {
			Report.Stored++;
		} else if ( Status == XCOOKIE_STORE_REMOVED ) {
			Report.Removed++;
		} else if ( Status == XCOOKIE_STORE_IGNORED ) {
			Report.Ignored++;
		} else {
			Report.Rejected++;
		}
	}
	if ( pReport != NULL ) {
		*pReport = Report;
	}
	return true;
}



/* 把 Jar 的选择结果设置为单个 Cookie 请求字段。 */
XHTTP_POLICY_API bool xrtCookieJarApply(
	xcookiejar* pJar,
	const xcookierequestcontext* pContext,
	xhttpheaders* pHeaders
)
{
	str sValue;
	size_t iSize;
	bool bResult;

	if ( (pJar == NULL) || (pContext == NULL) || (pHeaders == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	sValue = xrtCookieJarBuild(pJar, pContext, &iSize);
	if ( sValue == NULL ) {
		return false;
	}
	if ( iSize == 0 ) {
		(void)xrtHttpHeadersRemove(
			pHeaders, XRT_STR_LITERAL("Cookie")
		);
		bResult = true;
	} else {
		bResult = xrtHttpHeadersSet(
			pHeaders, XRT_STR_LITERAL("Cookie"),
			(xstrview){ sValue, iSize }
		);
	}
	xrtFree(sValue);
	return bResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_retry_policy.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY)

#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY)

/* 使用保守集合识别常见的临时服务或路由失败。 */
XHTTP_POLICY_API bool xrtHttpRetryStatusDefault(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_REQUEST_TIMEOUT:
		case XHTTP_STATUS_MISDIRECTED_REQUEST:
		case XHTTP_STATUS_TOO_EARLY:
		case XHTTP_STATUS_TOO_MANY_REQUESTS:
		case XHTTP_STATUS_INTERNAL_SERVER_ERROR:
		case XHTTP_STATUS_BAD_GATEWAY:
		case XHTTP_STATUS_SERVICE_UNAVAILABLE:
		case XHTTP_STATUS_GATEWAY_TIMEOUT:
			return true;

		default:
			return false;
	}
}



/* 以最多 64 次有效倍增得到不回绕的封顶指数退避。 */
XHTTP_POLICY_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
)
{
	uint64 iDelay = iBase;
	uint32 i;

	if ( !xrtMemRangeValid(pDelay, sizeof(iDelay)) ||
		(iMaximum == 0) ||
		(iBase > iMaximum) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	/* 零基数的全部指数项都为零，避免按外部序号空转。 */
	if ( iBase == 0 ) {
		memcpy(pDelay, &iDelay, sizeof(iDelay));
		return true;
	}
	for ( i = 0; (i < iRetry) && (iDelay < iMaximum); i++ ) {
		if ( iDelay > (iMaximum / UINT64_C(2)) ) {
			iDelay = iMaximum;
		} else {
			iDelay *= UINT64_C(2);
		}
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-policy/src/http/http_redirect_policy.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)

#if defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY)

#define XHTTP_REDIRECT_FLAGS_MASK \
	(XHTTP_REDIRECT_POST_TO_GET | \
	 XHTTP_REDIRECT_FORWARD_CREDENTIALS | \
	 XHTTP_REDIRECT_ALLOW_DOWNGRADE)



/* 初始化通用客户端使用的安全重定向策略。 */
XHTTP_POLICY_API void xrtHttpRedirectConfigInit(
	xhttpredirectconfig* pConfig
)
{
	const xhttpredirectconfig Config = {
		XHTTP_REDIRECT_POST_TO_GET,
		XHTTP_REDIRECT_MAX_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 按 ASCII 大小写不敏感规则比较两个文本视图。 */
static bool __xrtHttpRedirectTextEqual(
	xstrview Left,
	xstrview Right
)
{
	size_t i;

	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( i = 0; i < Left.Size; i++ ) {
		uint8 iLeft = (uint8)Left.Data[i];
		uint8 iRight = (uint8)Right.Data[i];

		if ( (iLeft >= (uint8)'A') &&
			(iLeft <= (uint8)'Z') ) {
			iLeft = (uint8)(
				iLeft +
				((uint8)'a' - (uint8)'A')
			);
		}
		if ( (iRight >= (uint8)'A') &&
			(iRight <= (uint8)'Z') ) {
			iRight = (uint8)(
				iRight +
				((uint8)'a' - (uint8)'A')
			);
		}
		if ( iLeft != iRight ) {
			return false;
		}
	}
	return true;
}



/* 判断状态码是否具有标准自动重定向语义。 */
XHTTP_POLICY_API bool xrtHttpRedirectStatus(uint16 iStatus)
{
	return (iStatus == 301) ||
		(iStatus == 302) ||
		(iStatus == 303) ||
		(iStatus == 307) ||
		(iStatus == 308);
}



/* 比较会影响凭据转发和连接复用的 HTTP origin。 */
static bool __xrtHttpRedirectSameOrigin(
	const xurl* pLeft,
	const xurl* pRight
)
{
	uint16 iLeftPort;
	uint16 iRightPort;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		return false;
	}
	if ( !xrtHttpBridgeUrlPortValue(pLeft, &iLeftPort) ||
		!xrtHttpBridgeUrlPortValue(pRight, &iRightPort) ) {
		return false;
	}
	return (xrtUrlSecure(pLeft) ==
			xrtUrlSecure(pRight)) &&
		(iLeftPort == iRightPort) &&
		__xrtHttpRedirectTextEqual(
			pLeft->Host,
			pRight->Host
		);
}



/*
	按 HTTP 重定向语义解析 Location。
	通用 URL 解析遵循 RFC 3986；HTTP 额外要求 Location 没有 fragment 时，
	继承当前目标的 fragment，显式空 fragment 则阻止继承。
*/
static str __xrtHttpRedirectResolve(
	const xurl* pBase,
	xstrview Location,
	size_t* pSize
)
{
	xurl Reference;
	str sResolved;
	str sExtended;
	size_t iResolved;
	size_t iExtended;

	if ( !xrtUrlParse(Location, &Reference) ) {
		return NULL;
	}
	sResolved = xrtUrlResolveBuild(
		pBase,
		Location,
		&iResolved
	);
	if ( sResolved == NULL ) {
		return NULL;
	}
	if ( ((Reference.Flags & XURL_HAS_FRAGMENT) != 0) ||
		((pBase->Flags & XURL_HAS_FRAGMENT) == 0) ) {
		*pSize = iResolved;
		return sResolved;
	}
	if ( (iResolved > (SIZE_MAX - 2u)) ||
		(pBase->Fragment.Size >
		 (SIZE_MAX - iResolved - 2u)) ) {
		xrtFree(sResolved);
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iExtended = iResolved +
		1u +
		pBase->Fragment.Size;
	sExtended = (str)xrtRealloc(
		sResolved,
		iExtended + 1u
	);
	if ( sExtended == NULL ) {
		xrtFree(sResolved);
		return NULL;
	}
	sExtended[iResolved] = '#';
	if ( pBase->Fragment.Size != 0 ) {
		memcpy(
			sExtended + iResolved + 1u,
			pBase->Fragment.Data,
			pBase->Fragment.Size
		);
	}
	sExtended[iExtended] = 0;
	*pSize = iExtended;
	return sExtended;
}



/* 删除改写为无正文请求后不再成立的正文和表示元数据。 */
static bool __xrtHttpRedirectDropBody(
	xhttprequest* pRequest
)
{
	static const xstrview Fields[] = {
		XRT_STR_INIT("Content-Length"),
		XRT_STR_INIT("Transfer-Encoding"),
		XRT_STR_INIT("Trailer"),
		XRT_STR_INIT("Expect"),
		XRT_STR_INIT("Content-Type"),
		XRT_STR_INIT("Content-Encoding"),
		XRT_STR_INIT("Content-Language"),
		XRT_STR_INIT("Content-Location"),
		XRT_STR_INIT("Content-Disposition"),
		XRT_STR_INIT("Content-Range"),
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest")
	};
	size_t i;

	if ( !xrtHttpRequestSetBody(pRequest, NULL) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		xrtHttpBridgeRequestClearTrailers(pRequest);
	#endif
	for ( i = 0;
		i < (sizeof(Fields) / sizeof(Fields[0]));
		i++ ) {
		(void)xrtHttpRequestRemoveHeader(
			pRequest,
			Fields[i]
		);
	}
	return true;
}



/* 构造下一跳请求，但直到当前响应排空后才提交。 */
XHTTP_POLICY_API xhttprequest* xrtHttpRedirectRequest(
	const xhttprequest* pRequest,
	uint16 iStatus,
	xstrview Location,
	uint32 iFlags,
	xhttpredirecterror* pError
)
{
	const xurl* pOldUrl;
	const xurl* pNewUrl;
	xhttprequest* pNext;
	xhttpredirecterror Error;
	xhttpbody* pBody;
	xstrview Method;
	str sResolved;
	size_t iResolved;
	bool bDropBody;
	bool bHead;
	bool bPost;
	bool bCrossOrigin;

	if ( !xrtMemRangeValid(pError, sizeof(*pError)) ||
		!__xhttpViewValid(Location) ||
		xrtMemRangesOverlap(pError, sizeof(*pError), Location.Data, Location.Size) ||
		((pRequest != NULL) && !xrtHttpBridgeRequestOutputValid(
			pRequest, pError, sizeof(*pError)
		)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	Error = XHTTP_REDIRECT_INVALID;
	memcpy(pError, &Error, sizeof(Error));
	if ( (pRequest == NULL) || !xrtHttpRedirectStatus(iStatus) ||
		(iFlags & ~XHTTP_REDIRECT_FLAGS_MASK) != 0 ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pOldUrl = xrtHttpRequestUrl(pRequest);
	sResolved = __xrtHttpRedirectResolve(
		pOldUrl,
		Location,
		&iResolved
	);
	if ( sResolved == NULL ) {
		Error = XHTTP_REDIRECT_INVALID;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	pNext = xrtHttpRequestClone(pRequest);
	if ( (pNext == NULL) ||
		!xrtHttpRequestSetUrl(
			pNext,
			(xstrview){ sResolved, iResolved }
		) ) {
		xrtHttpRequestDestroy(pNext);
		xrtFree(sResolved);
		Error = XHTTP_REDIRECT_INVALID;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	xrtFree(sResolved);
	pNewUrl = xrtHttpRequestUrl(pNext);
	if ( xrtUrlSecure(pOldUrl) &&
		!xrtUrlSecure(pNewUrl) &&
		((iFlags &
		  XHTTP_REDIRECT_ALLOW_DOWNGRADE) == 0) ) {
		xrtHttpRequestDestroy(pNext);
		Error = XHTTP_REDIRECT_DOWNGRADE;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}

	Method = xrtHttpRequestMethod(pRequest);
	bHead = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("HEAD")
	);
	bPost = xrtHttpMethodEqual(
		Method,
		XRT_STR_LITERAL("POST")
	);
	bDropBody = (iStatus == 303) ||
		(((iStatus == 301) || (iStatus == 302)) &&
		 bPost &&
		 ((iFlags &
		   XHTTP_REDIRECT_POST_TO_GET) != 0));
	pBody = xrtHttpRequestBody(pNext);
	if ( !bDropBody && (pBody != NULL) &&
		!xrtHttpBodyReplayable(pBody) ) {
		xrtHttpRequestDestroy(pNext);
		Error = XHTTP_REDIRECT_REPLAY;
		memcpy(pError, &Error, sizeof(Error));
		return NULL;
	}
	if ( bDropBody ) {
		if ( !bHead &&
			!xrtHttpRequestSetMethod(
				pNext,
				XRT_STR_LITERAL("GET")
			) ) {
			xrtHttpRequestDestroy(pNext);
			Error = XHTTP_REDIRECT_INVALID;
			memcpy(pError, &Error, sizeof(Error));
			return NULL;
		}
		if ( !__xrtHttpRedirectDropBody(pNext) ) {
			xrtHttpRequestDestroy(pNext);
			Error = XHTTP_REDIRECT_INVALID;
			memcpy(pError, &Error, sizeof(Error));
			return NULL;
		}
	}

	(void)xrtHttpRequestRemoveHeader(
		pNext,
		XRT_STR_LITERAL("Host")
	);
	bCrossOrigin = !__xrtHttpRedirectSameOrigin(
		pOldUrl,
		pNewUrl
	);
	if ( bCrossOrigin &&
		((iFlags &
		  XHTTP_REDIRECT_FORWARD_CREDENTIALS) == 0) ) {
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Authorization")
		);
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Proxy-Authorization")
		);
		(void)xrtHttpRequestRemoveHeader(
			pNext,
			XRT_STR_LITERAL("Cookie")
		);
	}
	Error = XHTTP_REDIRECT_OK;
	memcpy(pError, &Error, sizeof(Error));
	return pNext;
}



#endif
#endif

#endif
