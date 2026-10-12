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
#error "xhttp_client requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XHTTP_CLIENT_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XHTTP_CLIENT_IMPLEMENTATION) && \
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
#ifndef XHTTP_CLIENT_SINGLE_HEADER_H
#define XHTTP_CLIENT_SINGLE_HEADER_H
#define XHTTP_CLIENT_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xhttp-client/include/xhttp-client/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XHTTP_CLIENT_FEATURES_H
#define XHTTP_CLIENT_FEATURES_H

/* http_client_stream_async 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM_ASYNC)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC
#define XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#endif
#ifndef XHTTP_MODULE_HTTP_EXCHANGE_ASYNC
#define XHTTP_MODULE_HTTP_EXCHANGE_ASYNC
#endif
#endif

/* http_sse_client 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_SSE_CLIENT)
#ifndef XHTTP_FEATURE_HTTP_SSE_CLIENT
#define XHTTP_FEATURE_HTTP_SSE_CLIENT
#endif
#ifndef XHTTP_MODULE_HTTP_SSE_HTTP
#define XHTTP_MODULE_HTTP_SSE_HTTP
#endif
#ifndef XHTTP_MODULE_HTTP_SSE_PARSER
#define XHTTP_MODULE_HTTP_SSE_PARSER
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_REDIRECT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_REDIRECT
#endif
#endif

/* http_client_cache 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_CACHE)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_CACHE
#define XHTTP_FEATURE_HTTP_CLIENT_CACHE
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_CACHE_STORE
#define XHTTP_POLICY_MODULE_HTTP_CACHE_STORE
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_CACHE_RANGE
#define XHTTP_POLICY_MODULE_HTTP_CACHE_RANGE
#endif
#ifndef XHTTP_MODULE_HTTP_RANGE_MULTIPART
#define XHTTP_MODULE_HTTP_RANGE_MULTIPART
#endif
#ifndef XHTTP_MODULE_MULTIPART
#define XHTTP_MODULE_MULTIPART
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* http_client_decompress 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_DECOMPRESS)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS
#define XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XRT_MODULE_HTTP_ENCODING
#define XRT_MODULE_HTTP_ENCODING
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#endif

/* http_client_cookies 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_COOKIES)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_COOKIES
#define XHTTP_FEATURE_HTTP_CLIENT_COOKIES
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XHTTP_POLICY_MODULE_COOKIE_JAR_HEADERS
#define XHTTP_POLICY_MODULE_COOKIE_JAR_HEADERS
#endif
#endif

/* http_client_pool 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_POOL)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_POOL
#define XHTTP_FEATURE_HTTP_CLIENT_POOL
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_client_proxy 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_PROXY)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_PROXY
#define XHTTP_FEATURE_HTTP_CLIENT_PROXY
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XRT_MODULE_NET_PROXY_DIAL
#define XRT_MODULE_NET_PROXY_DIAL
#endif
#endif

/* http_client_easy_future 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY_FUTURE)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE
#define XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_FUTURE
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_FUTURE
#endif
#endif

/* http_client_future 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_FUTURE)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_FUTURE
#define XHTTP_FEATURE_HTTP_CLIENT_FUTURE
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XRT_MODULE_FUTURE_BRIDGE
#define XRT_MODULE_FUTURE_BRIDGE
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

/* http_client_easy 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_EASY)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_EASY
#define XHTTP_FEATURE_HTTP_CLIENT_EASY
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#endif

/* http_client_resume 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_RESUME)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_RESUME
#define XHTTP_FEATURE_HTTP_CLIENT_RESUME
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_HTTPS
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_HTTPS
#endif
#ifndef XRT_MODULE_TLS_CLIENT_RESUME
#define XRT_MODULE_TLS_CLIENT_RESUME
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* http_client_https 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_HTTPS)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_HTTPS
#define XHTTP_FEATURE_HTTP_CLIENT_HTTPS
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_TLS
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_TLS
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL
#define XRT_MODULE_TLS_STREAM_DIAL
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_X509_STORE_SYSTEM
#define XRT_MODULE_X509_STORE_SYSTEM
#endif
#endif

/* http_client_tls 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_TLS)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_TLS
#define XHTTP_FEATURE_HTTP_CLIENT_TLS
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#endif

/* http_client_retry 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_RETRY)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_RETRY
#define XHTTP_FEATURE_HTTP_CLIENT_RETRY
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XHTTP_MODULE_HTTP_RETRY
#define XHTTP_MODULE_HTTP_RETRY
#endif
#ifndef XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY
#define XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY
#endif
#endif

/* http_client_redirect 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_REDIRECT)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_REDIRECT
#define XHTTP_FEATURE_HTTP_CLIENT_REDIRECT
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#endif
#ifndef XHTTP_MODULE_URL
#define XHTTP_MODULE_URL
#endif
#ifndef XHTTP_POLICY_MODULE_HTTP_REDIRECT_POLICY
#define XHTTP_POLICY_MODULE_HTTP_REDIRECT_POLICY
#endif
#endif

/* http_client 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT)
#ifndef XHTTP_FEATURE_HTTP_CLIENT
#define XHTTP_FEATURE_HTTP_CLIENT
#endif
#ifndef XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#define XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#ifndef XRT_MODULE_CANCEL
#define XRT_MODULE_CANCEL
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

/* http_client_stream 及其直接依赖。 */
#if defined(XHTTP_CLIENT_MODULE_ALL) || defined(XHTTP_CLIENT_MODULE_HTTP_CLIENT_STREAM)
#ifndef XHTTP_FEATURE_HTTP_CLIENT_STREAM
#define XHTTP_FEATURE_HTTP_CLIENT_STREAM
#endif
#ifndef XHTTP_MODULE_HTTP_EXCHANGE
#define XHTTP_MODULE_HTTP_EXCHANGE
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

#endif /* XHTTP_CLIENT_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xhttp-client/export.h */
/* ========================================================================== */

#ifndef XHTTP_CLIENT_EXPORT_H
#define XHTTP_CLIENT_EXPORT_H


/* 独立动态库仅导出本库实现，依赖库由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XHTTP_CLIENT_BUILD_SHARED)
  #define XHTTP_CLIENT_API __declspec(dllexport)
 #elif defined(XHTTP_CLIENT_USE_SHARED)
  #define XHTTP_CLIENT_API __declspec(dllimport)
 #else
  #define XHTTP_CLIENT_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XHTTP_CLIENT_BUILD_SHARED)
  #define XHTTP_CLIENT_API __attribute__((visibility("default")))
 #else
  #define XHTTP_CLIENT_API
 #endif
#else
 #define XHTTP_CLIENT_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xrt/http_client_stream.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CLIENT_STREAM_H
#define XRT_HTTP_CLIENT_STREAM_H





#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) && \
	(!defined(XHTTP_FEATURE_HTTP_EXCHANGE) || \
	 !defined(XRT_FEATURE_NET_TCP) || \
	 !defined(XRT_FEATURE_SPIN))
	#error "XRT HTTP client stream support requires HTTP exchange, TCP and spin support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT HTTPS client stream support requires HTTP client stream and TLS stream support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XHTTP_FEATURE_HTTP_EXCHANGE_ASYNC))
	#error "XRT async HTTP client stream support requires stream and async exchange support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

/* 单次 HTTP/1 调用在已打开的 TCP 或 TLS Stream 上驱动一个 Exchange。 */
typedef struct xhttp1call xhttp1call;



/* 调用状态只从 RUNNING 进入一个不可变终态。 */
typedef enum xhttp1callstate {
	XHTTP1_CALL_RUNNING = 0,
	XHTTP1_CALL_SUCCEEDED,
	XHTTP1_CALL_FAILED,
	XHTTP1_CALL_CANCELLED
} xhttp1callstate;



/* 调用错误位于 xrt.http.call 域，区分状态、传输和 Exchange 失败。 */
typedef enum xhttp1callerror {
	XHTTP1_CALL_ERROR_ARGUMENT = 1,
	XHTTP1_CALL_ERROR_STATE,
	XHTTP1_CALL_ERROR_TRANSPORT,
	XHTTP1_CALL_ERROR_EXCHANGE,
	XHTTP1_CALL_ERROR_CANCELLED
} xhttp1callerror;



/* Progress 只在真实接受输出、消费输入或完整发送请求时产生。 */
typedef enum xhttp1progress {
	XHTTP1_PROGRESS_WRITE = 1,
	XHTTP1_PROGRESS_READ,
	XHTTP1_PROGRESS_REQUEST_DONE
} xhttp1progress;



/* WriteSize 只限制单次借出和发送，不建立固定缓冲。 */
typedef struct xhttp1callconfig {
	size_t WriteSize;
} xhttp1callconfig;



/*
	完成结果只在回调期间借用。成功时 Response 非空；失败时正文和传输均为空。
	Reusable 与 Upgraded 互斥，任一为真时 Tcp/Tls 中恰有一个调用方引用转移给回调。
	Reusable 时 Buffered 为零；Buffered 只表示 Upgrade 后仍留在传输中的协议外字节。
*/
typedef struct xhttp1callresult {
	xnetresult Result;
	xhttpresponse* Response;
	xnetstream* Tcp;
	xtlsstream* Tls;
	const xerror* Error;
	size_t Buffered;
	bool Reusable;
	bool Upgraded;
} xhttp1callresult;



/*
	完成回调在底层 Stream 所属 Worker 上同步执行且至多一次。
	构造函数成功返回之前不会执行完成回调。
*/
typedef void (*xhttp1callproc)(
	xhttp1call* pCall,
	const xhttp1callresult* pResult,
	ptr pData
);



/*
	进度回调在 Stream 所属 Worker 上同步执行。
	Bytes 只对 WRITE 和 READ 有效，REQUEST_DONE 的 Bytes 为零且至多出现一次。
*/
typedef void (*xhttp1progressproc)(
	xhttp1call* pCall,
	xhttp1progress Progress,
	size_t iBytes,
	ptr pData
);



/* Done 必须非空；Progress 为空时调用热路径没有额外回调。 */
typedef struct xhttp1callevents {
	xhttp1callproc Done;
	xhttp1progressproc Progress;
	ptr Data;
} xhttp1callevents;



XRT_EXTERN_C_BEGIN



/*
	初始化 16 KiB 单次输出上限；不会预分配对应大小的缓冲。
	配置允许位于未对齐存储，Call 创建时会先复制完整快照。
*/
XHTTP_CLIENT_API void xrtHttp1CallConfigInit(xhttp1callconfig* pConfig);



/*
	初始化空进度观察器；调用方必须随后设置 Done。
	事件表允许位于未对齐存储，Call 创建时会先复制完整快照。
*/
XHTTP_CLIENT_API void xrtHttp1CallEventsInit(xhttp1callevents* pEvents);



/*
	接管一个已打开 TCP Stream 调用方引用和一个 Exchange。
	必须在 Stream 所属 Worker 上调用；失败时两个输入仍归调用方。
*/
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallTcp(
	xnetstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/*
	接管一个已完成握手的 TLS Stream 调用方引用和一个 Exchange。
	必须在 Stream 所属 Worker 上调用；失败时两个输入仍归调用方。
*/
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallTls(
	xtlsstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
);

#endif



/* 增加调用引用并返回原指针。 */
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallRef(xhttp1call* pCall);



/* 释放调用引用；空指针视为空操作。 */
XHTTP_CLIENT_API void xrtHttp1CallDestroy(xhttp1call* pCall);



/*
	从任意线程协作取消调用并异常关闭尚未交出的传输。
	返回 true 表示取消已被接纳，最终状态必为 CANCELLED；
	返回 false 表示已有取消或终态已经提交。
*/
XHTTP_CLIENT_API bool xrtHttp1CallCancel(xhttp1call* pCall);



/* 在所属 Worker 上立即暂停响应输入和底层传输读取；重复调用安全。 */
XHTTP_CLIENT_API bool xrtHttp1CallPause(xhttp1call* pCall);



/* 从任意线程提交恢复；返回 false 表示未暂停、已提交恢复或已有终态。 */
XHTTP_CLIENT_API bool xrtHttp1CallResume(xhttp1call* pCall);



/* 返回响应输入暂停门的并发快照；调用进入任意终态后始终返回 false。 */
XHTTP_CLIENT_API bool xrtHttp1CallPaused(const xhttp1call* pCall);



/* 返回调用状态的并发快照。 */
XHTTP_CLIENT_API xhttp1callstate xrtHttp1CallState(const xhttp1call* pCall);



/* 返回失败或取消终态的借用错误；其他状态返回空。 */
XHTTP_CLIENT_API const xerror* xrtHttp1CallError(const xhttp1call* pCall);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xrt/http_client_runtime.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CLIENT_RUNTIME_H
#define XRT_HTTP_CLIENT_RUNTIME_H



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL) || \
	 !defined(XRT_FEATURE_CANCEL) || \
	 !defined(XRT_FEATURE_SPIN))
	#error "XRT HTTP client support requires HTTP stream, TCP Dial, cancellation and spin support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) && \
	(!defined(XHTTP_FEATURE_HTTP_CACHE_STORE) || \
	 !defined(XHTTP_FEATURE_HTTP_CACHE_RANGE) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XHTTP_FEATURE_MULTIPART) || \
	 !defined(XRT_FEATURE_CODEC_HEX) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP client cache requires cache storage, ranges, range multipart, generic multipart, hexadecimal codec and secure random"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	 !defined(XRT_FEATURE_TLS_STREAM_DIAL) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_X509_STORE_SYSTEM))
	#error "XRT HTTPS client support requires HTTP client, TLS Dial, verification and system trust"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_RESUME) || \
	 !defined(XRT_FEATURE_MUTEX))
	#error "XRT HTTP client resumption requires HTTPS, TLS client resumption and mutex"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) && \
	!defined(XHTTP_FEATURE_HTTP_CLIENT)
	#error "XRT HTTP client pool support requires the high-level HTTP client"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_REDIRECT_POLICY))
	#error "HTTP client redirect requires the client and redirect policy"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_RETRY) || \
	 !defined(XHTTP_FEATURE_HTTP_RETRY_POLICY) || \
	 !defined(XRT_FEATURE_RANDOM))
	#error "XRT HTTP client retry requires the HTTP client, HTTP retry protocol and random support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_NET_PROXY_DIAL))
	#error "XRT HTTP client proxy support requires the HTTP client and proxy Dial"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_COOKIE_JAR_HEADERS))
	#error "XRT HTTP client cookies require the HTTP client and CookieJar Header integration"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_HTTP_ENCODING) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XRT HTTP client decompression requires the HTTP client, HTTP Encoding and Inflate"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_CACHE_STORE))
	#error "XRT HTTP client cache requires the HTTP client and cache store"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

#define XHTTP_CLIENT_TIMEOUT_DEFAULT INT64_C(30000)
#define XHTTP_CLIENT_IDLE_TIMEOUT_DEFAULT INT64_C(30000)
#define XHTTP_CLIENT_TIMEOUT_NONE XRT_WAIT_FOREVER



/* 可选实现只裁剪代码，公开配置中的借用句柄声明始终可见。 */
typedef struct xhttpcache xhttpcache;
typedef struct xnetproxy xnetproxy;
typedef struct xcookiejar xcookiejar;
typedef struct xtlscontext xtlscontext;
typedef struct xtlsverifier xtlsverifier;



/* Client 保存共享网络策略、解析缓存和可选 TLS 信任快照。 */
typedef struct xhttpclient xhttpclient;



/* Call 表示从请求快照到最终响应或升级传输的一次完整执行。 */
typedef struct xhttpcall xhttpcall;



/*
	高层 Call 事件始终携带当前 Call，避免异步提交返回值与首个回调之间的发布竞态。
	Response 只在回调期间借用；返回 false 会终止当前 Call，并保留当前线程错误作为原因。
*/
typedef bool (*xhttpcallinformationalproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpcallheadersproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpcallbodyproc)(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
);



/*
	Body 为空时缓冲正文；非空时流式交付，Data 必须覆盖 Call 生命周期。
	Body 回调内 WireBodyBytes 已包含当前块，BodyBytes 在回调成功返回后累计。
*/
typedef struct xhttpcallevents {
	xhttpcallinformationalproc Informational;
	xhttpcallheadersproc Headers;
	xhttpcallbodyproc Body;
	ptr Data;
} xhttpcallevents;



#define XHTTP_CLIENT_CACHE_BODY_DEFAULT \
	(UINT64_C(8) * 1024u * 1024u)
#define XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT \
	(UINT64_C(24) * 60u * 60u * 1000000u)
#define XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT \
	UINT32_C(10)
#define XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT 16u



/* 每次调用可以继承、禁用、强制验证或禁止回源。 */
typedef enum xhttpclientcachemode {
	XHTTP_CLIENT_CACHE_DEFAULT = 0,
	XHTTP_CLIENT_CACHE_DISABLED,
	XHTTP_CLIENT_CACHE_RELOAD,
	XHTTP_CLIENT_CACHE_ONLY
} xhttpclientcachemode;



/* Cache 结果描述最终可见响应的来源，不混入具体存储后端状态。 */
typedef enum xhttpclientcacheoutcome {
	XHTTP_CLIENT_CACHE_NONE = 0,
	XHTTP_CLIENT_CACHE_MISS,
	XHTTP_CLIENT_CACHE_HIT,
	XHTTP_CLIENT_CACHE_STALE,
	XHTTP_CLIENT_CACHE_REVALIDATED,
	XHTTP_CLIENT_CACHE_UPDATED,
	XHTTP_CLIENT_CACHE_ONLY_MISS,
	XHTTP_CLIENT_CACHE_BYPASS
} xhttpclientcacheoutcome;



/*
	Store 为空时禁用 Client 自动缓存；创建 Client 时增加一个引用。
	MaxBody 是单次回源允许暂存的编码正文上限，避免无界中间内存。
	MaxRanges 限制一次本地 Range 解析的输入项数，防止多范围放大。
	Heuristic 使用 Last-Modified 的相对年龄并受百分比和最大寿命共同限制。
	Strict 让存储后端错误终止调用；默认采用不中断网络响应的 fail-open。
*/
typedef struct xhttpclientcacheconfig {
	xhttpcache* Store;
	uint64 MaxBody;
	uint64 HeuristicMax;
	size_t MaxRanges;
	uint32 HeuristicPercent;
	bool Shared;
	bool Heuristic;
	bool Strict;
} xhttpclientcacheconfig;



/* PartitionKey 隔离用户、站点或租户；空视图表示未分区缓存。 */
typedef struct xhttpclientcacheoptions {
	xhttpclientcachemode Mode;
	xstrview PartitionKey;
} xhttpclientcacheoptions;

/* Client 状态单向进入排空、终止和关闭终态。 */
typedef enum xhttpclientstate {
	XHTTP_CLIENT_RUNNING = 0,
	XHTTP_CLIENT_DRAINING,
	XHTTP_CLIENT_ABORTING,
	XHTTP_CLIENT_CLOSED
} xhttpclientstate;



/* Call 状态只向前进入一个不可变终态。 */
typedef enum xhttpcallstate {
	XHTTP_CALL_QUEUED = 0,
	XHTTP_CALL_DIALING,
	XHTTP_CALL_HANDSHAKING,
	XHTTP_CALL_EXCHANGING,
	XHTTP_CALL_SUCCEEDED,
	XHTTP_CALL_FAILED,
	XHTTP_CALL_CANCELLED
} xhttpcallstate;



/* Phase 表示最近开始的实际阶段，终态后保留成功或失败发生的位置。 */
typedef enum xhttpcallphase {
	XHTTP_CALL_PHASE_QUEUED = 0,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		XHTTP_CALL_PHASE_CACHE = 1,
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		XHTTP_CALL_PHASE_RETRY = 2,
	#endif
	XHTTP_CALL_PHASE_POOL = 3,
	XHTTP_CALL_PHASE_CONNECT = 4,
	XHTTP_CALL_PHASE_PROXY = 5,
	XHTTP_CALL_PHASE_TLS = 6,
	XHTTP_CALL_PHASE_REQUEST = 7,
	XHTTP_CALL_PHASE_RESPONSE_HEADERS = 8,
	XHTTP_CALL_PHASE_RESPONSE_BODY = 9
} xhttpcallphase;



/* 客户端错误按请求、传输、响应处理、协议和策略阶段稳定分类。 */
typedef enum xhttpclienterror {
	XHTTP_CLIENT_ERROR_NONE = 0,
	XHTTP_CLIENT_ERROR_ARGUMENT = 1,
	XHTTP_CLIENT_ERROR_STATE = 2,
	XHTTP_CLIENT_ERROR_CONFIG = 3,
	XHTTP_CLIENT_ERROR_REQUEST = 4,
	XHTTP_CLIENT_ERROR_RESPONSE = 5,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		XHTTP_CLIENT_ERROR_CACHE = 6,
	#endif
	XHTTP_CLIENT_ERROR_POOL = 7,
	XHTTP_CLIENT_ERROR_DIAL = 8,
	XHTTP_CLIENT_ERROR_PROXY = 9,
	XHTTP_CLIENT_ERROR_TLS = 10,
	XHTTP_CLIENT_ERROR_TRANSPORT = 11,
	XHTTP_CLIENT_ERROR_PROTOCOL = 12,
	XHTTP_CLIENT_ERROR_CALLBACK = 13,
	XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL = 14,
	XHTTP_CLIENT_ERROR_TIMEOUT_IDLE = 15,
	XHTTP_CLIENT_ERROR_CANCELLED = 16,
	XHTTP_CLIENT_ERROR_REDIRECT = 17,
	XHTTP_CLIENT_ERROR_REDIRECT_LIMIT = 18,
	XHTTP_CLIENT_ERROR_REDIRECT_REPLAY = 19,
	XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE = 20,
	XHTTP_CLIENT_ERROR_RETRY = 21,
	XHTTP_CLIENT_ERROR_COOKIE = 22,
	XHTTP_CLIENT_ERROR_DECOMPRESSION = 23,
	XHTTP_CLIENT_ERROR_INTERNAL = 24
} xhttpclienterror;



/*
	Info 是可并发读取的单调时钟快照，时间单位均为毫秒。
	未到达的时间点为零；Result 在运行期间为 AGAIN，终态后不再变化。
*/
typedef struct xhttpcallinfo {
	/* 当前生命周期、实际阶段和稳定客户端错误。 */
	xhttpcallstate State;
	xhttpcallphase Phase;
	xnetresult Result;
	xhttpclienterror Error;

	/* 完整 Call 的公元 UTC 毫秒事件时间。 */
	xtime Submitted;
	xtime Started;
	xtime TransportReady;
	xtime RequestSent;
	xtime FirstByte;
	xtime Headers;
	xtime LastProgress;
	xtime Completed;

	/* 线路累计量与最终可见响应正文量。 */
	uint64 RequestWireBytes;
	uint64 ResponseWireBytes;
	uint64 ResponseBodyBytes;

	/* 完整调用的重定向、复用和最终传输事实。 */
	size_t Redirects;
	size_t Retries;
	bool ReusedConnection;
	bool Secure;
	xhttpclientcacheoutcome Cache;
} xhttpcallinfo;



/*
	DEFAULT 继承 Client 默认代理，DIRECT 显式绕过默认代理，
	EXPLICIT 使用本次 Call 提供的不可变代理对象。
*/
typedef enum xhttpproxymode {
	XHTTP_PROXY_DEFAULT = 0,
	XHTTP_PROXY_DIRECT,
	XHTTP_PROXY_EXPLICIT
} xhttpproxymode;



/* EXPLICIT 要求 Proxy 非空；其他模式要求 Proxy 为空，避免忽略配置。 */
typedef struct xhttpproxyoptions {
	xhttpproxymode Mode;
	const xnetproxy* Proxy;
} xhttpproxyoptions;



#define XHTTP_RETRY_MAX_DEFAULT UINT32_C(2)
#define XHTTP_RETRY_BASE_DEFAULT INT64_C(250)
#define XHTTP_RETRY_DELAY_MAX_DEFAULT INT64_C(30000)

#define XHTTP_RETRY_STATUS UINT32_C(0x00000001)
#define XHTTP_RETRY_TRANSPORT UINT32_C(0x00000002)
#define XHTTP_RETRY_RESPECT_AFTER UINT32_C(0x00000004)
#define XHTTP_RETRY_JITTER UINT32_C(0x00000008)

#define XHTTP_RETRY_UNSAFE UINT32_C(0x00000001)



/*
	MaxRetries 为零时关闭 Client 默认重试。
	BaseDelay 与 MaxDelay 使用毫秒；Flags 分别控制状态、传输、服务端建议和抖动。
*/
typedef struct xhttpretryconfig {
	uint64 BaseDelay;
	uint64 MaxDelay;
	uint32 Flags;
	uint32 MaxRetries;
} xhttpretryconfig;



/* 单次调用可以继承、显式启用或显式关闭 Client 重试策略。 */
typedef enum xhttpretrymode {
	XHTTP_RETRY_DEFAULT = 0,
	XHTTP_RETRY_ENABLED,
	XHTTP_RETRY_DISABLED
} xhttpretrymode;



/* UNSAFE 只对本次调用显式允许非幂等方法重放。 */
typedef struct xhttpretryoptions {
	xhttpretrymode Mode;
	uint32 Flags;
} xhttpretryoptions;



#define XHTTP_COOKIE_DISABLED	UINT32_C(0x00000001)
#define XHTTP_COOKIE_SAME_SITE	UINT32_C(0x00000002)
#define XHTTP_COOKIE_TOP_LEVEL	UINT32_C(0x00000004)



/*
	默认按普通同站 HTTP API 请求选择 Cookie。
	PartitionKey 由 Call 复制，空视图表示未分区请求。
*/
typedef struct xhttpcookieoptions {
	uint32 Flags;
	xstrview PartitionKey;
} xhttpcookieoptions;



/*
	连接上限的零值表示不限制，等待上限的零值也表示不限制。
	任一 Idle 上限为零都会关闭复用；IdleTimeout 为零只关闭过期清扫。
*/
typedef struct xhttpclientpoolconfig {
	/* 全 Client 正在拨号、使用或保留配额的连接上限；零表示不限制。 */
	size_t MaxConnections;
	/* 单 Origin 连接上限；零表示不限制。 */
	size_t MaxConnectionsPerOrigin;
	/* 全 Client 等待 Call 上限；零表示不限制。 */
	size_t MaxWaiting;
	/* 单 Origin 等待 Call 上限；零表示不限制。 */
	size_t MaxWaitingPerOrigin;
	/* 全 Client 可复用空闲连接上限；零表示不保留。 */
	size_t MaxIdle;
	/* 单 Origin 可复用空闲连接上限；零表示不保留。 */
	size_t MaxIdlePerOrigin;
	/* 空闲连接保留时间，单位为毫秒；零表示不按时间清扫。 */
	int64 IdleTimeout;
} xhttpclientpoolconfig;



/*
	当前数量和累计计数都可并发读取，但不承诺来自同一个全局时刻。
	累计计数从 Client 创建起单调递增。
*/
typedef struct xhttpclientstats {
	/* 正在拨号、使用或已保留配额，但尚未进入空闲 LRU 的连接。 */
	size_t ActiveConnections;
	/* 当前位于空闲 LRU 的可复用连接。 */
	size_t IdleConnections;
	/* 已释放池配额、正在异步关闭的空闲连接。 */
	size_t ClosingConnections;
	/* 尚未取得连接或连接配额的 Call。 */
	size_t WaitingCalls;
	/* 已经被 Client 接受的 Call 总数。 */
	uint64 RequestsStarted;
	/* 已经进入唯一终态的 Call 总数。 */
	uint64 RequestsCompleted;
	/* 已经完成 TCP 或 TLS 建立的物理连接总数。 */
	uint64 ConnectionsOpened;
	/* 从空闲池或同源直交取得传输的次数。 */
	uint64 ConnectionsReused;
	/* 由池确认关闭或淘汰的物理连接总数。 */
	uint64 ConnectionsClosed;
	/* 曾经进入连接等待队列的 Call 总数。 */
	uint64 PoolWaits;
	/* 因全局或单 Origin 等待上限被拒绝的 Call 总数。 */
	uint64 PoolRejected;
	/* 全部 Call 实际已经跟随的重定向跳数。 */
	uint64 RedirectsFollowed;
} xhttpclientstats;

#define XHTTP_RESUME_ENTRIES_DEFAULT ((size_t)64u)
#define XHTTP_RESUME_ORIGIN_DEFAULT ((size_t)4u)



/*
	票据按验证 host、目标端口和代理对象身份隔离。
	任一上限为零都会关闭缓存，但不关闭基础 HTTPS。
*/
typedef struct xhttpresumeconfig {
	size_t MaxEntries;
	size_t MaxEntriesPerOrigin;
} xhttpresumeconfig;



/* 所有字段在同一缓存锁内读取，构成一致快照。 */
typedef struct xhttpresumestats {
	size_t Entries;
	uint64 Hits;
	uint64 Misses;
	uint64 Stored;
	uint64 Evicted;
	uint64 Expired;
	uint64 Dropped;
} xhttpresumestats;

#define XHTTP_DECOMPRESS_BODY_DEFAULT UINT64_C(67108864)
#define XHTTP_DECOMPRESS_CODINGS_DEFAULT UINT32_C(4)
#define XHTTP_DECOMPRESS_CODINGS_MAX UINT32_C(16)



/* 单次调用可继承 Client、强制自动解码或保留线路正文。 */
typedef enum xhttpdecompressmode {
	XHTTP_DECOMPRESS_DEFAULT = 0,
	XHTTP_DECOMPRESS_AUTO,
	XHTTP_DECOMPRESS_RAW
} xhttpdecompressmode;



/*
	Enabled 控制 Client 默认行为；MaxBody 约束最终和中间解码层的输出。
	MaxCodings 限制可自动处理的 Content-Encoding 叠加层数。
*/
typedef struct xhttpdecompressconfig {
	uint64 MaxBody;
	uint32 MaxCodings;
	bool Enabled;
} xhttpdecompressconfig;



/*
	Timeout 覆盖排队、DNS、TCP、代理、TLS 和 HTTP I/O 的总时长。
	IdleTimeout 限制没有传输进度的连续时长；两者单位均为毫秒。
	Resolver 只在 xrtHttpClientCreate 创建私有解析器时使用。
*/
typedef struct xhttpclientconfig {
	xnetresolverconfig Resolver;
	xnetdialconfig Dial;
	xhttp1callconfig Call;
	xhttp1exchangeconfig Exchange;
	int64 Timeout;
	int64 IdleTimeout;
	/* Client 创建时保留默认代理；空指针表示默认直连。 */
	const xnetproxy* Proxy;
	xhttpredirectconfig Redirect;
	xhttpretryconfig Retry;
	/* 可选共享 Jar 在 Client 创建时增加引用。 */
	xcookiejar* Cookies;
	xhttpdecompressconfig Decompress;
	xhttpclientcacheconfig Cache;
	xhttpclientpoolconfig Pool;
	xtlsstreamconfig TlsStream;
	const xtlscontext* TlsContext;
	const xtlsverifier* TlsVerifier;
	bool SystemTrust;
	xhttpresumeconfig Resume;
} xhttpclientconfig;



/*
	Timeout 和 IdleTimeout 为零时分别继承 Client，NONE 显式关闭对应限制。
	Cancel 在构造期间增加引用；Events 回调数据必须覆盖 Call 生命周期。
*/
typedef struct xhttpcalloptions {
	xhttp1requestoptions Request;
	xhttpcallevents Events;
	xcancel* Cancel;
	int64 Timeout;
	int64 IdleTimeout;
	/* 零值继承 Client；限制解码前表示正文，UINT64_MAX 允许无界流。 */
	uint64 ResponseBodyLimit;
	xhttpproxyoptions Proxy;
	xhttpredirectmode Redirect;
	xhttpretryoptions Retry;
	xhttpcookieoptions Cookies;
	xhttpdecompressmode Decompress;
	xhttpclientcacheoptions Cache;
} xhttpcalloptions;



/*
	结果只在完成回调期间借用。
	Response 及升级后的 Tcp/Tls 调用方引用在回调入口转移给调用方。
*/
typedef struct xhttpcallresult {
	xnetresult Result;
	xhttpresponse* Response;
	xnetstream* Tcp;
	xtlsstream* Tls;
	const xerror* Error;
	xhttpcallinfo Info;
	size_t Buffered;
	bool Upgraded;
} xhttpcallresult;



/*
	完成回调在本次 Call 选择的网络 Worker 上执行且至多一次。
	回调可能与提交线程并发执行，必须使用参数中的 pCall 识别本次调用。
*/
typedef void (*xhttpcallproc)(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
);



XRT_EXTERN_C_BEGIN



/* 初始化安全公网限额、两类 30 秒超时、Happy Eyeballs 和可选系统信任；允许未对齐存储。 */
XHTTP_CLIENT_API void xrtHttpClientConfigInit(xhttpclientconfig* pConfig);



/* 初始化 origin-form、无回调、无取消并继承 Client 的两类超时；允许未对齐存储。 */
XHTTP_CLIENT_API void xrtHttpCallOptionsInit(xhttpcalloptions* pOptions);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 初始化为继承 Client 默认代理；允许未对齐完整存储并拒绝地址回绕。 */
XHTTP_CLIENT_API void xrtHttpProxyOptionsInit(
	xhttpproxyoptions* pOptions
);

#endif







#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

/* 初始化为零次自动重试及完整安全策略；允许未对齐完整存储并拒绝地址回绕。 */
XHTTP_CLIENT_API void xrtHttpRetryConfigInit(
	xhttpretryconfig* pConfig
);



/* 初始化为继承 Client 且不允许非幂等重放；允许未对齐完整存储并拒绝地址回绕。 */
XHTTP_CLIENT_API void xrtHttpRetryOptionsInit(
	xhttpretryoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 初始化默认 Cookie 策略；接受未对齐的完整存储并拒绝回绕地址。 */
XHTTP_CLIENT_API void xrtHttpCookieOptionsInit(
	xhttpcookieoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 初始化自动解码、64 MiB 正文上限和四层编码上限；允许未对齐存储。 */
XHTTP_CLIENT_API void xrtHttpDecompressConfigInit(
	xhttpdecompressconfig* pConfig
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/* 初始化默认缓存策略；接受未对齐的完整存储并拒绝回绕地址。 */
XHTTP_CLIENT_API void xrtHttpClientCacheConfigInit(
	xhttpclientcacheconfig* pConfig
);



/* 初始化继承且未分区的缓存选项；存储可以未对齐但必须完整。 */
XHTTP_CLIENT_API void xrtHttpClientCacheOptionsInit(
	xhttpclientcacheoptions* pOptions
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/*
	初始化无连接硬上限、128/8 空闲上限和 90 秒空闲过期。
	输出允许未对齐，但必须是完整且不发生地址环绕的结构存储。
*/
XHTTP_CLIENT_API void xrtHttpClientPoolConfigInit(
	xhttpclientpoolconfig* pConfig
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 初始化默认恢复缓存；接受未对齐的完整存储并拒绝回绕地址。 */
XHTTP_CLIENT_API void xrtHttpResumeConfigInit(
	xhttpresumeconfig* pConfig
);

#endif



/*
	创建拥有私有异步 Resolver 的 Client。
	Client 占用运行中的 Engine 生命周期，直到最后一个 Client/Call 引用释放。
*/
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreate(
	xnetengine* pEngine,
	const xhttpclientconfig* pConfig
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/*
	使用默认 HTTP 策略和显式 TLS 对象创建 Client。
	Context 为空时创建默认上下文；Verifier 为空时使用系统信任。
*/
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreateTls(
	xnetengine* pEngine,
	const xtlscontext* pContext,
	const xtlsverifier* pVerifier
);

#endif



/*
	创建借用共享 Resolver 的 Client。
	Resolver 必须保持有效到最后一个 Client/Call 引用释放。
*/
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreateWithResolver(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig
);



/* 增加 Client 公开所有者引用并返回原指针。 */
XHTTP_CLIENT_API xhttpclient* xrtHttpClientRef(xhttpclient* pClient);



/*
	释放 Client 公开所有者引用。
	最后一个所有者会隐式开始平滑排空，但不会等待排空完成。
*/
XHTTP_CLIENT_API void xrtHttpClientDestroy(xhttpclient* pClient);



/* 停止接收新 Call，关闭空闲连接并让全部已提交 Call 自然完成。 */
XHTTP_CLIENT_API bool xrtHttpClientDrain(xhttpclient* pClient);



/* 停止接收新 Call，关闭空闲连接并协作取消全部已提交 Call。 */
XHTTP_CLIENT_API bool xrtHttpClientAbort(xhttpclient* pClient);



/* 返回 Client 当前生命周期状态。 */
XHTTP_CLIENT_API xhttpclientstate xrtHttpClientState(
	const xhttpclient* pClient
);



/* 返回 Client 生命周期绑定的借用网络 Engine；空 Client 返回空指针。 */
XHTTP_CLIENT_API xnetengine* xrtHttpClientEngine(
	const xhttpclient* pClient
);



/*
	冻结 Request 并异步执行一次 HTTP/1 请求。
	成功后 Call 独立于 Request；同步失败不接管 Request，也不会调用 Done。
*/
XHTTP_CLIENT_API xhttpcall* xrtHttpClientDo(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 增加 Call 引用并返回原指针。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpCallRef(xhttpcall* pCall);



/* 释放 Call 引用；空指针视为空操作。 */
XHTTP_CLIENT_API void xrtHttpCallDestroy(xhttpcall* pCall);



/*
	从任意线程协作取消 DNS、TCP、TLS 或 HTTP/1 阶段。
	返回 true 表示取消已被接纳，最终结果必为 CANCELLED；
	返回 false 表示已经接纳过取消或不可变终态已经提交。
*/
XHTTP_CLIENT_API bool xrtHttpCallCancel(xhttpcall* pCall);



/* 在当前网络 Worker 上暂停正在交付的响应正文和传输读取。 */
XHTTP_CLIENT_API bool xrtHttpCallPause(xhttpcall* pCall);



/* 从任意线程恢复当前 HTTP/1 响应输入。 */
XHTTP_CLIENT_API bool xrtHttpCallResume(xhttpcall* pCall);



/* 返回当前 HTTP/1 响应输入暂停门的并发快照。 */
XHTTP_CLIENT_API bool xrtHttpCallPaused(const xhttpcall* pCall);



/* 返回 Call 从提交到终态始终所属的借用网络 Worker。 */
XHTTP_CLIENT_API xnetworker* xrtHttpCallWorker(const xhttpcall* pCall);



/* 从任意线程克隆 Call 当前有效请求；调用方负责销毁返回的拥有型快照。 */
XHTTP_CLIENT_API xhttprequest* xrtHttpCallRequestClone(
	const xhttpcall* pCall
);



/* 返回 Call 当前阶段或不可变终态的并发快照。 */
XHTTP_CLIENT_API xhttpcallstate xrtHttpCallState(const xhttpcall* pCall);



/* 失败、超时或取消后返回借用的稳定错误原因链。 */
XHTTP_CLIENT_API const xerror* xrtHttpCallError(const xhttpcall* pCall);



/* 复制 Call 快照；输出存储可以未对齐但必须完整且地址不回绕。 */
XHTTP_CLIENT_API bool xrtHttpCallInfo(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/*
	返回 Client 借用的默认代理。
	未配置默认代理时返回空指针且不设置错误。
*/
XHTTP_CLIENT_API const xnetproxy* xrtHttpClientProxy(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/*
	返回 Client 借用的共享 CookieJar。
	未配置自动 Cookie 时返回空指针且不设置错误。
*/
XHTTP_CLIENT_API xcookiejar* xrtHttpClientCookieJar(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/*
	返回 Client 借用的统一 Cache 句柄。
	未配置自动缓存时返回空指针且不设置错误。
*/
XHTTP_CLIENT_API xhttpcache* xrtHttpClientCache(
	const xhttpclient* pClient
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 关闭当前全部空闲连接并返回从池中摘除的数量，不影响活动 Call。 */
XHTTP_CLIENT_API size_t xrtHttpClientCloseIdle(xhttpclient* pClient);



/* 读取并发快照；输出存储可以未对齐但必须完整且地址不回绕。 */
XHTTP_CLIENT_API bool xrtHttpClientStats(
	const xhttpclient* pClient,
	xhttpclientstats* pStats
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 清空尚未被连接取用的 TLS 恢复票据，并返回释放数量。 */
XHTTP_CLIENT_API size_t xrtHttpClientResumeClear(xhttpclient* pClient);



/* 读取恢复缓存一致快照；输出存储可以未对齐但必须完整。 */
XHTTP_CLIENT_API bool xrtHttpClientResumeStats(
	const xhttpclient* pClient,
	xhttpresumestats* pStats
);

#endif



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xrt/http_client_future.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CLIENT_FUTURE_H
#define XRT_HTTP_CLIENT_FUTURE_H





#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XRT_FEATURE_FUTURE_BRIDGE) || \
	 !defined(XRT_FEATURE_SPIN))
	#error "XRT HTTP client Future support requires HTTP client, Future bridge and spin support"
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/*
	Future 成功值是拥有型 HTTP 结果。
	结果同时覆盖普通响应与 Upgrade，避免异步便利层形成能力死角。
*/
typedef struct xhttpresult xhttpresult;



XRT_EXTERN_C_BEGIN



/*
	等待 Client 进入 CLOSED；Future 取消只放弃当前等待，不改变 Client 生命周期。
	Future 在活动 Call 回调、池传输和池 Timer 全部退出后才成功完成。
*/
XHTTP_CLIENT_API xfuture* xrtHttpClientWaitAsync(xhttpclient* pClient);



/* 增加有效结果引用并返回原指针；空指针或引用计数耗尽时失败。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpResultRef(xhttpresult* pResult);



/*
	释放结果引用。
	最后一个引用会销毁尚未取走的响应，并中止、释放尚未取走的 Upgrade 传输。
*/
XHTTP_CLIENT_API void xrtHttpResultDestroy(xhttpresult* pResult);



/* 返回结果借用的响应；结果保持有效且响应未被取走期间可用。 */
XHTTP_CLIENT_API const xhttpresponse* xrtHttpResultResponse(
	const xhttpresult* pResult
);



/*
	取走响应所有权并清空结果中的响应。
	同一结果的 Take 与借用访问不可并发。
*/
XHTTP_CLIENT_API xhttpresponse* xrtHttpResultTakeResponse(
	xhttpresult* pResult
);



/* 返回结果借用的明文 Upgrade Stream，普通响应或 HTTPS Upgrade 返回空。 */
XHTTP_CLIENT_API xnetstream* xrtHttpResultTcp(
	const xhttpresult* pResult
);



/* 取走明文 Upgrade Stream 所有权；同一传输至多成功取走一次。 */
XHTTP_CLIENT_API xnetstream* xrtHttpResultTakeTcp(
	xhttpresult* pResult
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 返回结果借用的 TLS Upgrade Stream，普通响应或明文 Upgrade 返回空。 */
XHTTP_CLIENT_API xtlsstream* xrtHttpResultTls(
	const xhttpresult* pResult
);



/* 取走 TLS Upgrade Stream 所有权；同一传输至多成功取走一次。 */
XHTTP_CLIENT_API xtlsstream* xrtHttpResultTakeTls(
	xhttpresult* pResult
);

#endif



/* 返回 Upgrade 后已经留在传输接收缓冲中的协议外字节数。 */
XHTTP_CLIENT_API size_t xrtHttpResultBuffered(
	const xhttpresult* pResult
);



/* 返回本次调用已经跟随的重定向次数。 */
XHTTP_CLIENT_API size_t xrtHttpResultRedirects(
	const xhttpresult* pResult
);



/* 判断结果是否已经把传输从 HTTP/1 交付给调用方。 */
XHTTP_CLIENT_API bool xrtHttpResultUpgraded(
	const xhttpresult* pResult
);



/* 复制不可变 Call 快照；输出存储可以未对齐但必须完整。 */
XHTTP_CLIENT_API bool xrtHttpResultInfo(
	const xhttpresult* pResult,
	xhttpcallinfo* pInfo
);



/*
	以 Future 执行一次高层 HTTP 调用。
	Future 成功值是其拥有的 xhttpresult；失败错误由 Future 持有。
	取消 Future 会协作取消底层 Call，外部 Call 取消也会反映为 Future 取消终态。
*/
XHTTP_CLIENT_API xfuture* xrtHttpClientDoAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
);



/*
	阻塞执行一次高层 HTTP 调用并返回拥有型结果。
	网络 Worker 不允许调用此函数；协程应使用 DoAsync 与 xrtFutureAwait。
*/
XHTTP_CLIENT_API xhttpresult* xrtHttpClientDoSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xrt/http_client_easy.h */
/* ========================================================================== */

#ifndef XRT_HTTP_CLIENT_EASY_H
#define XRT_HTTP_CLIENT_EASY_H



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) && \
	!defined(XHTTP_FEATURE_HTTP_CLIENT)
	#error "XRT HTTP client convenience support requires HTTP client support"
#endif

#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE) && \
	(!defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE))
	#error "XRT HTTP client Future convenience support requires convenience and Future support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY)

/* 构造并提交一条无正文 GET；返回的 Call 所有权与 xrtHttpClientDo 相同。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientGet(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 复制正文并构造一条 POST；ContentType 为空时不生成 Content-Type。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientPost(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);



/* 复制正文并以任意合法 HTTP 方法提交，适合 PUT、PATCH 等固定正文请求。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientSendBytes(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE)

/* 构造并异步提交一条无正文 GET，返回拥有型 Future。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientGetAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
);



/* 复制正文并异步提交一条 POST，返回拥有型 Future。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientPostAsync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 复制正文并以任意合法 HTTP 方法异步提交，返回拥有型 Future。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientSendBytesAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 构造并阻塞执行一条无正文 GET，返回拥有型结果。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientGetSync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
);



/* 复制正文并阻塞执行一条 POST，返回拥有型结果。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientPostSync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);



/* 复制正文并以任意合法 HTTP 方法阻塞执行，返回拥有型结果。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientSendBytesSync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xrt/http_sse_client.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SSE_CLIENT_H
#define XRT_HTTP_SSE_CLIENT_H





#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT) && \
	(!defined(XHTTP_FEATURE_HTTP_SSE_HTTP) || \
	 !defined(XHTTP_FEATURE_HTTP_SSE_PARSER) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	 !defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT))
	#error "XRT HTTP SSE client requires SSE HTTP, Parser, Client and Redirect support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)

/* 默认无限重连，并把服务端毫秒延迟约束在 100 ms 到五分钟。 */
#define XHTTP_SSE_RECONNECT_MAX_DEFAULT SIZE_MAX
#define XHTTP_SSE_RETRY_MIN_DEFAULT UINT64_C(100)
#define XHTTP_SSE_RETRY_MAX_DEFAULT UINT64_C(300000)



/* SSE Client 与 WHATWG EventSource 保持 CONNECTING、OPEN、CLOSED 三态。 */
typedef enum xhttpsseclientstate {
	XHTTP_SSE_CLIENT_CONNECTING = 0,
	XHTTP_SSE_CLIENT_OPEN = 1,
	XHTTP_SSE_CLIENT_CLOSED = 2
} xhttpsseclientstate;



/* 终态原因区分主动关闭、协议停止、永久失败和本地资源失败。 */
typedef enum xhttpsseclosereason {
	XHTTP_SSE_CLOSE_USER = 0,
	XHTTP_SSE_CLOSE_CANCELLED,
	XHTTP_SSE_CLOSE_STOP,
	XHTTP_SSE_CLOSE_REJECTED,
	XHTTP_SSE_CLOSE_PARSE,
	XHTTP_SSE_CLOSE_CALLBACK,
	XHTTP_SSE_CLOSE_HTTP,
	XHTTP_SSE_CLOSE_RECONNECT_LIMIT,
	XHTTP_SSE_CLOSE_INTERNAL
} xhttpsseclosereason;



/* 稳定错误码为错误链提供 SSE 会话层上下文。 */
typedef enum xhttpsseclienterror {
	XHTTP_SSE_CLIENT_ERROR_ARGUMENT = 1,
	XHTTP_SSE_CLIENT_ERROR_STATE,
	XHTTP_SSE_CLIENT_ERROR_CONFIG,
	XHTTP_SSE_CLIENT_ERROR_REQUEST,
	XHTTP_SSE_CLIENT_ERROR_RESPONSE,
	XHTTP_SSE_CLIENT_ERROR_PARSE,
	XHTTP_SSE_CLIENT_ERROR_CALLBACK,
	XHTTP_SSE_CLIENT_ERROR_CANCELLED,
	XHTTP_SSE_CLIENT_ERROR_HTTP,
	XHTTP_SSE_CLIENT_ERROR_RECONNECT,
	XHTTP_SSE_CLIENT_ERROR_INTERNAL
} xhttpsseclienterror;



/* Client 是一个拥有 Parser、Last-Event-ID 和重连状态的订阅会话。 */
typedef struct xhttpsseclient xhttpsseclient;



/* 配置内嵌一次 HTTP Call 策略；动态借用字段会在 Connect 时深持有。 */
typedef struct xhttpsseclientconfig {
	xhttpsseparserconfig Parser;
	xhttpcalloptions Http;
	size_t MaxReconnects;
	uint64 RetryMin;
	uint64 RetryMax;
} xhttpsseclientconfig;



/* 并发快照不暴露 Parser 内部借用视图。 */
typedef struct xhttpsseclientinfo {
	xhttpsseclientstate State;
	uint16 Status;
	uint64 Retry;
	uint64 Messages;
	uint64 Comments;
	uint64 RetryUpdates;
	size_t Reconnects;
	bool Paused;
} xhttpsseclientinfo;



/* Open 与流项目在 HTTP Call 的网络 Worker 上同步发布。 */
typedef bool (*xhttpsseopenproc)(
	xhttpsseclient* pClient,
	const xhttpresponse* pResponse,
	ptr pData
);

typedef bool (*xhttpssemessageproc)(
	xhttpsseclient* pClient,
	const xhttpssemessage* pMessage,
	ptr pData
);

typedef bool (*xhttpssecommentproc)(
	xhttpsseclient* pClient,
	xstrview Comment,
	ptr pData
);

typedef bool (*xhttpsseretryproc)(
	xhttpsseclient* pClient,
	uint64 iRetry,
	ptr pData
);



/* Retrying 报告一次暂态断开；Error 只在本次回调期间借用。 */
typedef void (*xhttpsseretryingproc)(
	xhttpsseclient* pClient,
	size_t iReconnect,
	uint64 iDelay,
	const xerror* pError,
	ptr pData
);



/* Close 是唯一终态回调；Error 在回调后仍可由 ClientError 借用。 */
typedef void (*xhttpssecloseproc)(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError,
	ptr pData
);



/* Message 必须存在，其余事件按需启用。 */
typedef struct xhttpsseclientevents {
	xhttpsseopenproc Open;
	xhttpssemessageproc Message;
	xhttpssecommentproc Comment;
	xhttpsseretryproc Retry;
	xhttpsseretryingproc Retrying;
	xhttpssecloseproc Close;
	ptr Data;
} xhttpsseclientevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)

/* 初始化无总超时、无空闲超时、自动重连和有界重连延迟；允许未对齐存储。 */
XHTTP_CLIENT_API void xrtHttpSseClientConfigInit(
	xhttpsseclientconfig* pConfig
);



/* 使用 GET URL 建立会话；配置与事件表在返回前完整快照并允许未对齐存储。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseConnect(
	xhttpclient* pHttp,
	xstrview Url,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
);



/* 克隆 GET 请求；配置与事件表在返回前完整快照并允许未对齐存储。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseConnectRequest(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
);



/* 增加会话引用并返回原指针。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseClientRef(
	xhttpsseclient* pClient
);



/* 释放会话引用；空指针视为空操作，不隐式关闭仍活动的订阅。 */
XHTTP_CLIENT_API void xrtHttpSseClientDestroy(
	xhttpsseclient* pClient
);



/* 从任意线程永久关闭当前 HTTP Call 或重连 Timer。 */
XHTTP_CLIENT_API bool xrtHttpSseClientClose(
	xhttpsseclient* pClient
);



/* 在当前回调 Worker 上暂停项目交付和底层传输读取。 */
XHTTP_CLIENT_API bool xrtHttpSseClientPause(
	xhttpsseclient* pClient
);



/* 从任意线程恢复 Parser 尾段，再开放底层传输读取。 */
XHTTP_CLIENT_API bool xrtHttpSseClientResume(
	xhttpsseclient* pClient
);



/* 返回当前 EventSource 三态的并发快照。 */
XHTTP_CLIENT_API xhttpsseclientstate xrtHttpSseClientState(
	const xhttpsseclient* pClient
);



/* 返回当前暂停门快照。 */
XHTTP_CLIENT_API bool xrtHttpSseClientPaused(
	const xhttpsseclient* pClient
);



/* 原子复制状态、状态码、重连时间和累计项目数量；输出允许未对齐存储。 */
XHTTP_CLIENT_API bool xrtHttpSseClientInfo(
	const xhttpsseclient* pClient,
	xhttpsseclientinfo* pInfo
);



/* 唯一 Close 已发布后返回稳定错误；正常主动或 204 停止可以为空。 */
XHTTP_CLIENT_API const xerror* xrtHttpSseClientError(
	const xhttpsseclient* pClient
);

#endif



XRT_EXTERN_C_END
#endif


/* ========================================================================== */
/* public: extlibs/xhttp-client/include/xhttp-client.h */
/* ========================================================================== */

#ifndef XHTTP_CLIENT_H
#define XHTTP_CLIENT_H



#endif

#endif

#if defined(XHTTP_CLIENT_IMPLEMENTATION) && !defined(XHTTP_CLIENT_IMPLEMENTATION_ONCE)
#define XHTTP_CLIENT_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xhttp_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
	defined(XHTTP_FEATURE_HTTP_SSE_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#ifndef XHTTP_CLIENT_SUPPORT_BRIDGE_H
#define XHTTP_CLIENT_SUPPORT_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
	defined(XHTTP_FEATURE_HTTP_SSE_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#ifndef XHTTP_INTERNAL_RUNTIME_H
#define XHTTP_INTERNAL_RUNTIME_H

#if defined(XRT_FEATURE_SPIN)
#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_exchange.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
	defined(XHTTP_FEATURE_HTTP_SSE_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#ifndef XHTTP_CLIENT_XRT_HTTP_EXCHANGE_H
#define XHTTP_CLIENT_XRT_HTTP_EXCHANGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_client_stream.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_TLS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
	defined(XHTTP_FEATURE_HTTP_SSE_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#ifndef XRT_INTERNAL_HTTP_CLIENT_STREAM_H
#define XRT_INTERNAL_HTTP_CLIENT_STREAM_H

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
#endif


#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

/* 传输种类只用于选择类型安全的公开 Stream 操作。 */
typedef enum xrt_http1_call_transport {
	XRT_HTTP1_CALL_TCP = 0,
	XRT_HTTP1_CALL_TLS
} xrt_http1_call_transport;



/*
	调用由调用方、运行阶段和临时驱动过程分别持有引用。
	Lock 只线性化终态、取消和传输所有权，不进入协议与 I/O 热循环。
*/
struct xhttp1call {
	volatile int32 References;
	xspinlock Lock;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 PauseGate;
	xatomic32 ResumeGate;
	xatomicptr Stream;
	xnetworker* Worker;
	xhttp1exchange* Exchange;
	xhttp1callconfig Config;
	xhttp1callevents Events;
	xerror* Error;
	xrt_http1_call_transport Transport;
	xnetpost StartPost;
	xnetpost ResumePost;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		xfuturewatch OutputWatch;
		xnetpost OutputPost;
		xfuture* OutputFuture;
		bool OutputNotified;
	#endif
	bool RuntimeHeld;
	bool Driving;
	bool DriveAgain;
	bool OutputWaiting;
	bool OutputQueued;
	size_t OutputBytes;
	bool InputEnded;
	bool RequestDoneObserved;
};



/* 验证调用配置不会造成零进展输出。 */
bool __xrtHttp1CallConfigValid(
	const xhttp1callconfig* pConfig
);




/* 判断普通 TCP 传输是否仍满足 HTTP/1 连接复用边界。 */
bool __xrtHttp1TcpReusable(const xnetstream* pStream);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 在所属 Worker 上判断 TLS 传输是否仍满足 HTTP/1 连接复用边界。 */
bool __xrtHttp1TlsReusable(xtlsstream* pStream);

#endif



/* 串行推进一次调用的收发状态机。 */
void __xrtHttp1CallDrive(xhttp1call* pCall);



/* 摘除传输并以结构化原因结束调用。 */
void __xrtHttp1CallFail(
	xhttp1call* pCall,
	xhttp1callerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)

/* 订阅 Exchange 当前正文背压 Future。 */
bool __xrtHttp1CallAsyncWait(xhttp1call* pCall);



/* 取消并摘除尚未完成的正文背压等待。 */
void __xrtHttp1CallAsyncStop(xhttp1call* pCall);

#endif

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_client_runtime.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_POOL) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE) || \
	defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)
#ifndef XRT_INTERNAL_HTTP_CLIENT_RUNTIME_H
#define XRT_INTERNAL_HTTP_CLIENT_RUNTIME_H


#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
typedef struct __xrt_http_client_origin
	__xrt_http_client_origin;
typedef struct __xrt_http_client_idle
	__xrt_http_client_idle;
typedef struct __xrt_http_client_pool_shard
	__xrt_http_client_pool_shard;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
typedef struct __xrt_http_client_wait
	__xrt_http_client_wait;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
typedef struct __xrt_http_client_resume
	__xrt_http_client_resume;



/* TLS ticket 缓存只需要稳定的 HTTP 路由身份，不依赖完整 Call。 */
typedef struct __xrt_http_resume_route {
	xhttpclient* Client;
	xstrview Host;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
} __xrt_http_resume_route;
#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX "xrt-cache-"
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE 10u
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE 16u
#define XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE \
	(XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE + \
	 (XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE * 2u))



/* 缓存重放范围状态只描述已经完成协议选择的交付方式。 */
typedef enum __xrt_http_client_cache_range {
	__XRT_HTTP_CLIENT_CACHE_RANGE_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL,
	__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART,
	__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE
} __xrt_http_client_cache_range;



/* 源站 multipart/byteranges 规划结果区分非目标类型、保守跳过和可存储。 */
typedef enum __xrt_http_client_cache_multipart_decision {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR = -1,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE
} __xrt_http_client_cache_multipart_decision;



/* 跳过原因保留协议边界，便于测试和后续诊断扩展。 */
typedef enum __xrt_http_client_cache_multipart_reason {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BOUNDARY =
		UINT32_C(0x00000001),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BODY =
		UINT32_C(0x00000002),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS =
		UINT32_C(0x00000004),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_HEADERS =
		UINT32_C(0x00000008),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_RANGE =
		UINT32_C(0x00000010),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH =
		UINT32_C(0x00000020),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_CONTENT_TYPE =
		UINT32_C(0x00000040),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_ENCODING =
		UINT32_C(0x00000080),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_OVERLAP =
		UINT32_C(0x00000100)
} __xrt_http_client_cache_multipart_reason;



typedef enum __xrt_http_client_cache_multipart_flag {
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_FLAG_NONE = 0,
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE =
		UINT32_C(0x00000001),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH =
		UINT32_C(0x00000002),
	__XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE =
		UINT32_C(0x00000004)
} __xrt_http_client_cache_multipart_flag;



/*
	Parts 数组由规划结果拥有，正文视图借用输入 multipart 正文。
	ContentType 借用首个有效 Part Header，不能超过输入正文寿命。
*/
typedef struct __xrt_http_client_cache_multipart {
	xhttpcachepart* Parts;
	xstrview ContentType;
	uint64 Length;
	size_t PartCount;
	__xrt_http_client_cache_multipart_decision Decision;
	uint32 Flags;
	uint32 Reasons;
} __xrt_http_client_cache_multipart;

#endif



/* 高频诊断字段使用原子快照，避免为每次收发进度获取 Call 锁。 */
typedef struct __xrt_http_call_info {
	xatomic32 Phase;
	xatomic32 Result;
	xatomic32 Error;
	xatomic32 Reused;
	xatomic32 Secure;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xatomic32 Cache;
	#endif
	xatomic64 Submitted;
	xatomic64 Started;
	xatomic64 TransportReady;
	xatomic64 RequestSent;
	xatomic64 FirstByte;
	xatomic64 Headers;
	xatomic64 LastProgress;
	xatomic64 Completed;
	xatomic64 RequestWireBytes;
	xatomic64 ResponseWireBytes;
	xatomic64 ResponseBodyBytes;
	xatomic64 Redirects;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xatomic64 Retries;
	#endif
} __xrt_http_call_info;



/* Client 深复制或保留全部跨调用共享配置。 */
struct xhttpclient {
	volatile int32 References;
	volatile int32 Owners;
	xatomic64 NextAffinity;
	xatomic32 State;
	xspinlock LifecycleLock;
	xhttpcall* CallHead;
	xhttpcall* CallTail;
	size_t ActiveCalls;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
		__xrt_http_client_wait* CloseWaitHead;
		__xrt_http_client_wait* CloseWaitTail;
	#endif
	xnetengine* Engine;
	xnetresolver* Resolver;
	xhttpclientconfig Config;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrt_http_client_pool_shard* PoolShards;
		uint32 PoolShardCount;
		xatomic64 PoolConnections;
		xatomic64 PoolIdle;
		xatomic64 PoolClosing;
		xatomic64 PoolWaiting;
		xatomic64 PoolTimersPending;
		xatomic64 PoolLive;
		xatomic64 RequestsStarted;
		xatomic64 RequestsCompleted;
		xatomic64 ConnectionsOpened;
		xatomic64 ConnectionsReused;
		xatomic64 ConnectionsClosed;
		xatomic64 PoolWaits;
		xatomic64 PoolRejected;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
			xatomic64 RedirectsFollowed;
		#endif
		bool PoolReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlscontext* TlsContext;
		xtlsverifier* TlsVerifier;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xmutex ResumeLock;
		__xrt_http_client_resume* ResumeHead;
		__xrt_http_client_resume* ResumeTail;
		size_t ResumeCount;
		uint64 ResumeHits;
		uint64 ResumeMisses;
		uint64 ResumeStored;
		uint64 ResumeEvicted;
		uint64 ResumeExpired;
		uint64 ResumeDropped;
		bool ResumeReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xcookiejar* Cookies;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xhttpcache* Cache;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	bool OwnResolver;
	bool EngineHeld;
};



/* Call 的终态门、可取消对象和短生命周期标志都由 Lock 保护。 */
struct xhttpcall {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CancelGate;
	xatomic32 FinishGate;
	xatomic32 TimeoutCause;
	xatomic32 TotalTimerDone;
	xatomic32 IdleTimerDone;
	xatomic64 TotalTimer;
	xatomic64 IdleTimer;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xatomic32 RetryTimerDone;
		xatomic64 RetryTimer;
	#endif
	double TotalDeadline;
	xatomic64 IdleDeadline;
	__xrt_http_call_info Info;
	xspinlock Lock;
	xhttpclient* Client;
	xhttpcall* ClientPrevious;
	xhttpcall* ClientNext;
	xhttpcall* AbortNext;
	xnetworker* Worker;
	xhttp1exchange* Exchange;
	xnetdial* TcpDial;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxydial* ProxyDial;
		xnetproxy* Proxy;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsdial* TlsDial;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xtlsstream* ProxyTls;
			bool ProxyTlsStarting;
			bool ProxyTlsCallback;
		#endif
	#endif
	xhttp1call* StreamCall;
	xcancel* Cancel;
	xcancelwatch* CancelWatch;
	xhttpcallproc Done;
	ptr Data;
	xerror* Error;
	str Host;
	uint16 Port;
	uint64 Affinity;
	int64 Timeout;
	int64 IdleTimeout;
	uint64 ResponseBodyLimit;
	xhttprequest* Request;
	xhttp1requestoptions RequestOptions;
	xhttpcallevents Events;
	xhttp1exchangeevents InfoEvents;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xhttp1exchangeevents DecompressEvents;
		xhttp1exchangeevents DecompressNext;
		xinflate** Inflaters;
		xhttpresponse* DecompressResponse;
		size_t InflaterCount;
		uint64 DecompressLimit;
		uint64 DecompressBodyBytes;
		uint32 DecompressMaxCodings;
		bool DecompressEnabled;
		bool DecompressActive;
		bool DecompressFailed;
		bool DecompressForwardFailed;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xhttprequest* RedirectRequest;
		xhttp1exchangeevents RedirectEvents;
		xhttp1exchangeevents RedirectNext;
		xhttpredirectconfig RedirectConfig;
		xhttpredirectmode RedirectMode;
		xhttpclienterror RedirectError;
		size_t Redirects;
		bool RedirectPending;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xhttp1exchangeevents RetryEvents;
		xhttp1exchangeevents RetryNext;
		xhttpretryconfig RetryConfig;
		xrng RetryRng;
		uint64 RetryDelay;
		uint64 RetryResponseWireStart;
		uint32 RetryMax;
		size_t Retries;
		bool RetryEnabled;
		bool RetryUnsafe;
		bool RetryPending;
		bool RetryRngReady;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xhttp1exchangeevents CookieEvents;
		xhttp1exchangeevents CookieNext;
		str CookiePartitionKey;
		size_t CookiePartitionSize;
		uint32 CookieFlags;
		xhttpclienterror CookieError;
		bool CookiesEnabled;
		bool CookieAutomatic;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xhttp1exchangeevents CacheEvents;
		xhttp1exchangeevents CacheNext;
		xhttpcacherecord* CacheCandidate;
		xhttpheaders* CacheRequestFields;
		xhttpheaders* CacheResponseFields;
		bytes CacheBody;
		size_t CacheBodySize;
		size_t CacheBodyCapacity;
		str CachePartitionKey;
		size_t CachePartitionSize;
		xhttpbyterange* CacheRanges;
		size_t CacheRangeCount;
		double CacheRequestClock;
		double CacheResponseClock;
		uint64 CacheRangeBodyLength;
		xtime CacheResponseTime;
		xhttpbyterange CacheRange;
		char CacheBoundary[
			XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE + 1u
		];
		xhttpclientcachemode CacheMode;
		__xrt_http_client_cache_range CacheRangeState;
		bool CacheEnabled;
		bool CacheReady;
		bool CacheCapture;
		bool CacheRangeRequest;
		bool CacheRangeCovered;
		bool CacheRangeFill;
		bool CacheValidating;
		bool CacheNotModified;
		bool CacheIfNoneMatch;
		bool CacheIfModifiedSince;
		bool CacheIfRange;
		bool CacheFailed;
	#endif
	xnetpost StartPost;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xnetpost PoolPost;
		__xrt_http_client_origin* PoolOrigin;
		__xrt_http_client_idle* PoolIdle;
		xhttpcall* PoolPrevious;
		xhttpcall* PoolNext;
		xnetstream* PooledTcp;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			xtlsstream* PooledTls;
		#endif
		bool PoolWaiting;
		bool PoolReserved;
		bool PoolOpened;
		uint32 PoolShardIndex;
	#endif
	bool Secure;
	bool ClientLinked;
	bool RuntimeHeld;
};



/* 建立客户端域错误并保留明确的下层原因。 */
xerror* __xrtHttpClientErrorCreate(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 建立稳定的取消或超时终态错误并接管旧错误。 */
xerror* __xrtHttpClientTerminalError(
	xerror* pCause,
	xhttpclienterror Code
);



/* 设置创建或同步准备阶段的当前线程客户端错误。 */
void __xrtHttpClientSetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 沿原因链返回最内层有效错误类别。 */
xerrkind __xrtHttpClientCauseKind(
	const xerror* pError,
	xerrkind Fallback
);



/* 发布高层阶段；提交后仅由所属 Worker 调用。 */
void __xrtHttpCallSetPhase(
	xhttpcall* pCall,
	xhttpcallphase Phase
);



/* 标记当前 Hop 的 TCP、代理或 TLS 传输已经可以承载 HTTP。 */
void __xrtHttpCallTransportReady(xhttpcall* pCall);



/* 标记当前 Hop 使用了连接池中的既有传输。 */
void __xrtHttpCallReused(xhttpcall* pCall);



/* 建立低级 HTTP/1 Call 使用的完成与真实 I/O 进度事件。 */
void __xrtHttpCallStreamEvents(
	xhttpcall* pCall,
	xhttp1callevents* pEvents
);



/* 增加只供 Call、Idle 和 Timer 使用的内部 Client 引用。 */
xhttpclient* __xrtHttpClientHold(xhttpclient* pClient);



/* 释放内部 Client 引用，不改变公开 Owner 数量。 */
void __xrtHttpClientRelease(xhttpclient* pClient);



/* 在 Client 仍运行时原子登记一个已经准备完成的 Call。 */
bool __xrtHttpClientCallAttach(xhttpcall* pCall);



/* 用户完成回调返回后摘除 Call，并尝试发布 Client 关闭终态。 */
void __xrtHttpClientCallDetach(xhttpcall* pCall);



/* 在全部 Call、池传输和池 Timer 退出后发布唯一关闭终态。 */
void __xrtHttpClientTryFinish(xhttpclient* pClient);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/* 摘除并完成 Client 的全部关闭等待。 */
void __xrtHttpClientFutureFinish(xhttpclient* pClient);

#endif



/* 发布一个没有响应或升级传输的失败终态。 */
void __xrtHttpCallFail(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 发布带响应和可选升级传输的成功终态。 */
void __xrtHttpCallSucceed(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded
);



/* 把低级 HTTP/1 Call 的唯一终态提升为完整客户端结果。 */
void __xrtHttpClientStreamDone(
	xhttp1call* pStreamCall,
	const xhttp1callresult* pResult,
	ptr pData
);



/* 为当前请求快照建立下一跳 Plan、Exchange 和传输端点。 */
bool __xrtHttpCallPrepareHop(
	xhttpcall* pCall
);



/* 在不重复安装总 Timer 和取消观察器的前提下获取并启动一跳传输。 */
void __xrtHttpCallStartHop(xhttpcall* pCall);



/* 在目标 Worker 上开始明文 TCP Dial。 */
bool __xrtHttpCallStartTcp(xhttpcall* pCall);



/* 接管一条已经建立的普通 TCP 传输并进入 HTTP/1。 */
void __xrtHttpCallTcpConnected(
	xhttpcall* pCall,
	xnetstream* pStream,
	cstr sOperation
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/* 验证配置并让 Client 保留统一缓存句柄。 */
bool __xrtHttpClientCacheOpen(xhttpclient* pClient);



/* 释放 Client 持有的统一缓存句柄。 */
void __xrtHttpClientCacheClose(xhttpclient* pClient);



/* 释放源站 multipart 范围计划拥有的片段数组。 */
void __xrtHttpClientCacheMultipartUnit(
	__xrt_http_client_cache_multipart* pPlan
);



/* 把完整源站 multipart/byteranges 正文规划为排序、互不重叠的缓存片段。 */
__xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartPlan(
	const xhttpcachefragmentinput* pBase,
	xstrview ContentType,
	xbytesview Body,
	size_t iMaxParts,
	__xrt_http_client_cache_multipart* pPlan
);



/* 冻结调用级缓存模式与分区，并验证 Client 缓存配置。 */
bool __xrtHttpClientCacheInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 持有的候选、字段快照、正文和分区文本。 */
void __xrtHttpClientCacheUnit(xhttpcall* pCall);



/* 在有效请求字段就绪后选择缓存条目或准备条件请求。 */
bool __xrtHttpClientCachePrepare(xhttpcall* pCall);



/* 把缓存观察器放在重定向外层，并保留每一跳原始表示。 */
const xhttp1exchangeevents* __xrtHttpClientCacheEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 在获取连接池配额前交付命中或 only-if-cached 响应。 */
bool __xrtHttpClientCacheStart(
	xhttpcall* pCall,
	bool* pHandled
);



/* 在重定向和解压收尾前提交网络响应或 304 更新。 */
bool __xrtHttpClientCacheDone(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	bool* pReplayed
);



/* 把严格模式下的缓存回调错误提升为高层终态。 */
bool __xrtHttpClientCacheFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 冻结本次调用的默认、直连或显式代理选择。 */
bool __xrtHttpProxyInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 经选定代理建立目标隧道，HTTPS 在隧道内继续 TLS 握手。 */
bool __xrtHttpCallStartProxy(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

/* 建立调用拥有的初始请求快照和重定向事件包装器。 */
bool __xrtHttpRedirectInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 尚未提交的重定向请求。 */
void __xrtHttpRedirectUnit(xhttpcall* pCall);



/* 返回仅向最终响应转发 Header 和正文的 Exchange 事件。 */
const xhttp1exchangeevents* __xrtHttpRedirectEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断低层失败是否由重定向策略产生，并发布对应高层错误。 */
bool __xrtHttpRedirectFail(
	xhttpcall* pCall,
	const xerror* pCause
);



/* 提交已经排空的中间响应并准备下一跳 Exchange。 */
bool __xrtHttpRedirectAdvance(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

/* 冻结单次调用的重试模式、限额和非幂等授权。 */
bool __xrtHttpRetryInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 返回先隐藏待重试响应、再向缓存及用户事件转发的包装器。 */
const xhttp1exchangeevents* __xrtHttpRetryEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断当前成功响应是否已经被重试策略接管。 */
bool __xrtHttpRetryPending(const xhttpcall* pCall);



/* 排空当前响应或结束失败尝试后，安排下一次重放。 */
bool __xrtHttpRetrySchedule(xhttpcall* pCall);



/* 判断可重放的临时传输失败并在命中策略时安排下一次尝试。 */
bool __xrtHttpRetryFailure(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
);



/* 取消仍在退避等待中的 Timer；完成回调负责发布取消终态。 */
bool __xrtHttpRetryCancel(xhttpcall* pCall);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

/* 冻结单次调用的自动解码策略并按需注入 Accept-Encoding。 */
bool __xrtHttpDecompressInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放当前响应的全部解码器并清除每跳状态。 */
void __xrtHttpDecompressReset(xhttpcall* pCall);



/* 返回只向最终响应应用内容解码的事件包装器。 */
const xhttp1exchangeevents* __xrtHttpDecompressEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 完成全部叠加编码并发布准确的解码正文长度。 */
bool __xrtHttpDecompressFinish(
	xhttpcall* pCall,
	xhttpresponse* pResponse
);



/* 把解码器失败映射为稳定的高层客户端错误。 */
bool __xrtHttpDecompressFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 复制每次调用的 Cookie 上下文并建立启用状态。 */
bool __xrtHttpCookieInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
);



/* 释放 Call 拥有的 Cookie 上下文。 */
void __xrtHttpCookieUnit(xhttpcall* pCall);



/* 在当前请求快照上移除旧自动字段并重新选择 Cookie。 */
bool __xrtHttpCookiePrepare(xhttpcall* pCall);



/* 把同步准备阶段的 Cookie 根因包装为当前线程客户端错误。 */
void __xrtHttpCookieSetSubmitError(xhttpcall* pCall);



/* 返回先存储 Set-Cookie、再转发到下层事件的包装器。 */
const xhttp1exchangeevents* __xrtHttpCookieEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
);



/* 判断低层失败是否由 Cookie 策略产生，并发布高层错误。 */
bool __xrtHttpCookieFail(
	xhttpcall* pCall,
	const xerror* pCause
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 建立 Client 连接池状态。 */
bool __xrtHttpPoolInit(xhttpclient* pClient);



/* 释放已经排空的 Client 连接池状态。 */
void __xrtHttpPoolUnit(xhttpclient* pClient);



/* 安排 Call 复用、创建新连接或进入有界等待队列。 */
bool __xrtHttpPoolAcquire(xhttpcall* pCall, bool* pReady);



/* 取消仍在连接池等待队列中的 Call。 */
bool __xrtHttpPoolCancel(xhttpcall* pCall);



/* 标记新拨号已经建立一条实际连接。 */
void __xrtHttpPoolOpened(xhttpcall* pCall);



/* 把可复用传输直接交给等待者或放入空闲 LRU。 */
bool __xrtHttpPoolPut(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
);



/* 已取出的空闲传输成功交给低级 HTTP/1 Call。 */
void __xrtHttpPoolPooledUsed(xhttpcall* pCall);



/* 已取出的空闲传输失效，保留连接配额并准备重新拨号。 */
void __xrtHttpPoolPooledStale(xhttpcall* pCall);



/* 升级或隧道传输离开池管理，但不计为物理关闭。 */
void __xrtHttpPoolTransferred(xhttpcall* pCall);



/* 唯一终态清理等待、待接管空闲传输和连接配额。 */
void __xrtHttpPoolFinish(xhttpcall* pCall);



/* 在空闲 TCP Stream 所属 Worker 上重新附加 HTTP/1 Call。 */
bool __xrtHttpCallStartPooledTcp(xhttpcall* pCall);



/* 连接池唤醒后在选定 Worker 上继续复用或拨号。 */
void __xrtHttpCallPoolReady(
	xnetworker* pWorker,
	ptr pData
);

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 为 Client 建立或保留 TLS Context 与验证器。 */
bool __xrtHttpClientTlsInit(xhttpclient* pClient);



/* 为目标 Host 建立一致的 Context、SNI、ALPN 与验证器配置。 */
void __xrtHttpCallTlsConfig(
	xhttpcall* pCall,
	xtlsclientconfig* pConfig
);



/* 释放 Client 持有的 TLS 共享对象。 */
void __xrtHttpClientTlsUnit(xhttpclient* pClient);



/* 在目标 Worker 上开始带验证和 ALPN 的 TLS Dial。 */
bool __xrtHttpCallStartTls(xhttpcall* pCall);



/* 接管一条已经完成验证的 TLS 传输并进入 HTTP/1。 */
void __xrtHttpCallTlsConnected(
	xhttpcall* pCall,
	xtlsstream* pStream,
	cstr sOperation
);



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TLS Stream 所属 Worker 上重新附加 HTTP/1 Call。 */
bool __xrtHttpCallStartPooledTls(xhttpcall* pCall);

#endif

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 建立 Client 独享的有界 TLS 恢复票据缓存。 */
bool __xrtHttpResumeInit(xhttpclient* pClient);



/* 释放 Client 票据缓存及其代理和恢复对象引用。 */
void __xrtHttpResumeUnit(xhttpclient* pClient);



/* 为本次 TLS 握手取出一张同路由、仍有效的单次票据。 */
xtlsresume* __xrtHttpResumeTake(xhttpcall* pCall);



/*
	把一张票据按 HTTP 路由加入缓存。
	无论是否缓存成功，函数都会接管 pResume 的调用方引用。
*/
void __xrtHttpResumeStore(
	const __xrt_http_resume_route* pRoute,
	xtlsresume* pResume
);



/* 从完成 HTTP 事务的 TLS 会话接管全部新票据。 */
void __xrtHttpResumeCollect(
	xhttpcall* pCall,
	xtlsstream* pStream
);



/* 从活动 Call 之外的同路由 TLS Stream 接管新票据。 */
void __xrtHttpResumeCollectRoute(
	const __xrt_http_resume_route* pRoute,
	xtlsstream* pStream
);

#endif



#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_url.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
#ifndef XHTTP_CLIENT_XRT_URL_H
#define XHTTP_CLIENT_XRT_URL_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_client_easy.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE)
#ifndef XRT_INTERNAL_HTTP_CLIENT_EASY_H
#define XRT_INTERNAL_HTTP_CLIENT_EASY_H




#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY)

/* 校验便利入口共有的 Client 与可选完成过程参数。 */
bool __xrtHttpClientEasyCheck(
	xhttpclient* pClient,
	bool bCompletion,
	cstr sOperation
);



/* 创建临时请求；Body 为空指针表示没有正文，非空则复制固定字节正文。 */
xhttprequest* __xrtHttpClientEasyRequest(
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_cache_range.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
#ifndef XHTTP_CLIENT_XRT_HTTP_CACHE_RANGE_H
#define XHTTP_CLIENT_XRT_HTTP_CACHE_RANGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_cache_store.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
#ifndef XHTTP_CLIENT_XRT_HTTP_CACHE_STORE_H
#define XHTTP_CLIENT_XRT_HTTP_CACHE_STORE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-client/src/internal/xrt_http_sse_client.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)
#ifndef XRT_INTERNAL_HTTP_SSE_CLIENT_H
#define XRT_INTERNAL_HTTP_SSE_CLIENT_H




#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)

/* 当前响应分类只由所属 HTTP Worker 推进。 */
typedef enum xrt_http_sse_attempt {
	XRT_HTTP_SSE_ATTEMPT_WAITING = 0,
	XRT_HTTP_SSE_ATTEMPT_OPEN,
	XRT_HTTP_SSE_ATTEMPT_STOP,
	XRT_HTTP_SSE_ATTEMPT_REJECTED,
	XRT_HTTP_SSE_ATTEMPT_FAILED
} xrt_http_sse_attempt;



/* 会话不预分配正文区；Pending 只保存一次暂停点后的当前传输尾段。 */
struct xhttpsseclient {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CloseGate;
	xatomic32 FinishGate;
	xatomic32 PauseGate;
	xatomic32 ResumeGate;
	xatomic32 RequestedReason;
	xatomic32 Status;
	xatomic64 Retry;
	xatomic64 Messages;
	xatomic64 Comments;
	xatomic64 RetryUpdates;
	xatomic64 Reconnects;
	xspinlock Lock;
	xhttpclient* Http;
	xhttprequest* Request;
	xhttpcall* Call;
	xcancel* Cancel;
	xcancelwatch* CancelWatch;
	xnetworker* Worker;
	xnetengine* Engine;
	uint64 Timer;
	xnetpost ClosePost;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseparser Parser;
	xbuffer Pending;
	xerror* Error;
	xerror* AttemptError;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		str CookiePartition;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		str CachePartition;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	xrt_http_sse_attempt Attempt;
	xhttpsseclosereason AttemptReason;
	bool RuntimeHeld;
	bool Constructing;
};

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_stream.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM)

#define XRT_HTTP1_CALL_WRITE_DEFAULT 16384u



/* 在当前驱动退出后处理可能同步到达的传输输入终点。 */
static void __xrtHttp1CallEnd(xhttp1call* pCall);



/* 建立调用层结构化错误，并保留明确的下层原因。 */
static xerror* __xrtHttp1CallErrorCreate(
	xerrkind Kind,
	xhttp1callerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.call";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 建立稳定的取消错误；分配失败时退回核心静态取消错误。 */
static xerror* __xrtHttp1CallCancelledError(
	const xerror* pCause
)
{
	xerror* pError = __xrtHttp1CallErrorCreate(
		XERR_CANCELLED,
		XHTTP1_CALL_ERROR_CANCELLED,
		"cancel-http1-call",
		"HTTP/1 call was cancelled",
		pCause
	);

	if ( (pError != NULL) &&
		(xrtErrorKind(pError) == XERR_CANCELLED) ) {
		return pError;
	}
	xrtErrorFree(pError);
	xrtSetErrorKind(XERR_CANCELLED);
	return xrtErrorRef(xrtGetError());
}



/* 设置创建阶段当前线程错误。 */
static void __xrtHttp1CallSetError(
	xerrkind Kind,
	xhttp1callerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttp1CallErrorCreate(
		Kind, Code, sOperation, sMessage, pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 验证调用配置不会造成零进展输出。 */
bool __xrtHttp1CallConfigValid(
	const xhttp1callconfig* pConfig
)
{
	if ( (pConfig == NULL) || (pConfig->WriteSize == 0) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"configure-http1-call",
			"HTTP/1 call write size must be non-zero",
			NULL
		);
		return false;
	}
	return true;
}



/* 增加调用引用。 */
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallRef(xhttp1call* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRetain(&pCall->References) < 0) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"retain-http1-call",
			"HTTP/1 call is null or already released",
			NULL
		);
		return NULL;
	}
	return pCall;
}



/* 释放调用尚未转移的传输引用。 */
static void __xrtHttp1CallStreamDestroy(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		xrtNetStreamDestroy((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		xrtTlsStreamDestroy((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 在所有权锁内取得传输独立引用，供跨线程路径在锁外使用。 */
static ptr __xrtHttp1CallStreamRef(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return NULL;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamRef((xnetstream*)pStream);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamRef((xtlsstream*)pStream);
	#else
		(void)pCall;
		return NULL;
	#endif
}



/* 在所有权锁内摘除调用持有的传输引用。 */
static ptr __xrtHttp1CallStreamTake(xhttp1call* pCall)
{
	ptr pStream;

	(void)xrtSpinLock(&pCall->Lock);
	pStream = xrtAtomicPtrExchange(
		&pCall->Stream,
		NULL,
		XMEMORY_ACQ_REL
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	return pStream;
}



/* 异常关闭调用尚未转移的传输。 */
static void __xrtHttp1CallStreamAbort(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		(void)xrtNetStreamAbort((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		(void)xrtTlsStreamAbort((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 正常关闭不再复用的传输。 */
static void __xrtHttp1CallStreamClose(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		(void)xrtNetStreamClose((xnetstream*)pStream);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		(void)xrtTlsStreamClose((xtlsstream*)pStream);
	#else
		(void)pCall;
	#endif
}



/* 返回 Call 当前传输实际使用的 TCP 读门。 */
static xnetstream* __xrtHttp1CallTcpTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return NULL;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return (xnetstream*)pStream;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamTransport((xtlsstream*)pStream);
	#else
		(void)pCall;
		return NULL;
	#endif
}



/* 立即关闭底层 TCP 读门；TLS 已解密明文继续由 TLS 自身限额约束。 */
static void __xrtHttp1CallPauseTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	xnetstream* pTransport = __xrtHttp1CallTcpTransport(
		pCall, pStream
	);

	if ( pTransport != NULL ) {
		xrtNetStreamPause(pTransport);
	}
}



/* 恢复实际 TCP 读门；已有缓冲总是先由 Call 驱动消费。 */
static bool __xrtHttp1CallResumeTransport(
	const xhttp1call* pCall,
	ptr pStream
)
{
	xnetstream* pTransport = __xrtHttp1CallTcpTransport(
		pCall, pStream
	);

	return (pTransport != NULL) && xrtNetStreamResume(pTransport);
}



/* 释放最后一个调用引用和全部尚未交出的状态。 */
XHTTP_CLIENT_API void xrtHttp1CallDestroy(xhttp1call* pCall)
{
	ptr pStream;

	if ( (pCall == NULL) ||
		(xrtRefRelease(&pCall->References) != 0) ) {
		return;
	}
	pStream = __xrtHttp1CallStreamTake(pCall);
	__xrtHttp1CallStreamDestroy(pCall, pStream);
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtErrorFree(pCall->Error);
	(void)xrtSpinUnit(&pCall->Lock);
	memset(pCall, 0, sizeof(*pCall));
	xrtFree(pCall);
}



/* 终态若没有 TCP 输出租约在途，立即释放 Exchange。 */
static void __xrtHttp1CallExchangeUnit(xhttp1call* pCall)
{
	if ( !pCall->OutputQueued && (pCall->Exchange != NULL) ) {
		xrtHttp1ExchangeDestroy(pCall->Exchange);
		pCall->Exchange = NULL;
	}
}



/* 返回传输当前仍未交给 Exchange 的字节数。 */
static size_t __xrtHttp1CallBuffered(
	const xhttp1call* pCall,
	ptr pStream
)
{
	if ( pStream == NULL ) {
		return 0;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamAvailable(
			(const xnetstream*)pStream
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamAvailable(
			(const xtlsstream*)pStream
		);
	#else
		return 0;
	#endif
}



/* 判断普通 TCP 两个方向和接收缓冲都仍可承载下一次 HTTP/1 事务。 */
bool __xrtHttp1TcpReusable(const xnetstream* pStream)
{
	xnetstreamstats Stats;

	if ( (pStream == NULL) ||
		!xrtNetStreamStats(pStream, &Stats) ) {
		return false;
	}
	return (Stats.State == XNET_STREAM_OPEN) &&
		!Stats.ReadEnded &&
		!Stats.WriteEnded &&
		(Stats.BufferedBytes == 0);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 判断 TLS 会话、明文缓冲和底层 TCP 都仍可承载下一次 HTTP/1 事务。 */
bool __xrtHttp1TlsReusable(xtlsstream* pStream)
{
	xtlssession* pSession;
	xnetstream* pTransport;

	if ( (pStream == NULL) ||
		(xrtTlsStreamState(pStream) != XTLS_STREAM_OPEN) ||
		(xrtTlsStreamAvailable(pStream) != 0) ) {
		return false;
	}
	pSession = xrtTlsStreamSession(pStream);
	pTransport = xrtTlsStreamTransport(pStream);
	return (pSession != NULL) &&
		(xrtTlsSessionState(pSession) == XTLS_STATE_READY) &&
		__xrtHttp1TcpReusable(pTransport);
}

#endif



/* 在所属 Worker 上摘除调用事件，后续传输不再引用调用对象。 */
static bool __xrtHttp1CallDetach(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamSetEvents(
			(xnetstream*)pStream,
			NULL,
			NULL
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamSetEvents(
			(xtlsstream*)pStream,
			NULL,
			NULL
		);
	#else
		return false;
	#endif
}



/* 在所有权锁内封闭终态，并清除已经不可能恢复的输入门。 */
static void __xrtHttp1CallSeal(xhttp1call* pCall)
{
	xrtAtomic32Store(
		&pCall->FinishGate,
		1,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
}



/* 发布失败或取消终态；调用方保证当前 Stream 不会再回调本对象。 */
static void __xrtHttp1CallFinishFailure(
	xhttp1call* pCall,
	xnetresult Result,
	const xerror* pCause
)
{
	xhttp1callresult CallResult;
	xerror* pCancelledError;
	xerror* pPreviousError;
	ptr pStream;
	bool bCancelled;

	/*
		取消接纳与终态提交共用一条线性化边界。
		提交后 Cancel 必须返回 false，提交前接纳的取消必须决定最终结果。
	*/
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return;
	}
	bCancelled = xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) != 0;
	__xrtHttp1CallSeal(pCall);
	(void)xrtSpinUnlock(&pCall->Lock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		__xrtHttp1CallAsyncStop(pCall);
	#endif
	if ( !bCancelled && (Result == XNET_RESULT_OK) ) {
		Result = XNET_RESULT_ERROR;
	}
	if ( bCancelled &&
		((pCall->Error == NULL) ||
		 (xrtErrorKind(pCall->Error) != XERR_CANCELLED)) ) {
		pPreviousError = pCall->Error;
		pCancelledError = __xrtHttp1CallCancelledError(
			pPreviousError != NULL ?
				pPreviousError :
				pCause
		);
		if ( pCancelledError != NULL ) {
			pCall->Error = pCancelledError;
			xrtErrorFree(pPreviousError);
		}
	} else if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			XERR_IO,
			XHTTP1_CALL_ERROR_TRANSPORT,
			"run-http1-call",
			"HTTP/1 call transport failed",
			pCause
		);
	}
	Result = bCancelled ? XNET_RESULT_CANCELLED : Result;
	xrtAtomic32Store(
		&pCall->State,
		bCancelled ?
			XHTTP1_CALL_CANCELLED :
			XHTTP1_CALL_FAILED,
		XMEMORY_RELEASE
	);
	pStream = __xrtHttp1CallStreamTake(pCall);
	__xrtHttp1CallExchangeUnit(pCall);
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = Result;
	CallResult.Error = pCall->Error;
	pCall->Events.Done(
		pCall,
		&CallResult,
		pCall->Events.Data
	);
	__xrtHttp1CallStreamDestroy(pCall, pStream);
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
	}
}



/* 摘除事件、异常关闭传输并发布调用层原因。 */
void __xrtHttp1CallFail(
	xhttp1call* pCall,
	xhttp1callerror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	ptr pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);

	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			Kind,
			Code,
			sOperation,
			sMessage,
			pCause
		);
	}
	if ( (pStream != NULL) &&
		__xrtHttp1CallDetach(pCall, pStream) ) {
		pStream = __xrtHttp1CallStreamTake(pCall);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		__xrtHttp1CallFinishFailure(
			pCall,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XNET_RESULT_CANCELLED : XNET_RESULT_ERROR,
			pCall->Error
		);
		return;
	}
	__xrtHttp1CallStreamAbort(pCall, pStream);
}



/* 发布成功响应，并归还可复用或已升级的传输。 */
static void __xrtHttp1CallFinishSuccess(xhttp1call* pCall)
{
	xhttp1callresult CallResult;
	xhttpresponse* pResponse;
	ptr pStream;
	size_t iBuffered;
	bool bUpgraded;
	bool bReusable;
	bool bTransfer;

	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);
	pResponse = xrtHttp1ExchangeTakeResponse(
		pCall->Exchange
	);
	if ( pResponse == NULL ) {
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			XERR_INTERNAL,
			"complete-http1-call",
			"HTTP/1 Exchange completed without a response",
			xrtGetError()
		);
		return;
	}
	bUpgraded = xrtHttp1ExchangeUpgraded(pCall->Exchange);
	iBuffered = __xrtHttp1CallBuffered(pCall, pStream);
	bReusable = !bUpgraded &&
		(iBuffered == 0) &&
		xrtHttp1ExchangeReusable(pCall->Exchange) &&
		(pCall->Transport == XRT_HTTP1_CALL_TCP ?
			__xrtHttp1TcpReusable(
				(const xnetstream*)pStream
			) :
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
				__xrtHttp1TlsReusable(
					(xtlsstream*)pStream
				)
			#else
				false
			#endif
		);
	if ( (pStream == NULL) ||
		!__xrtHttp1CallDetach(pCall, pStream) ) {
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_STATE,
			XERR_STATE,
			"detach-http1-call",
			"HTTP/1 call could not detach its completed transport",
			xrtGetError()
		);
		return;
	}
	pStream = __xrtHttp1CallStreamTake(pCall);
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		__xrtHttp1CallFinishFailure(
			pCall,
			XNET_RESULT_CANCELLED,
			NULL
		);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpResponseDestroy(pResponse);
		__xrtHttp1CallStreamAbort(pCall, pStream);
		__xrtHttp1CallStreamDestroy(pCall, pStream);
		return;
	}
	__xrtHttp1CallSeal(pCall);
	(void)xrtSpinUnlock(&pCall->Lock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
		__xrtHttp1CallAsyncStop(pCall);
	#endif
	xrtAtomic32Store(
		&pCall->State,
		XHTTP1_CALL_SUCCEEDED,
		XMEMORY_RELEASE
	);
	bTransfer = bReusable || bUpgraded;
	if ( !bTransfer ) {
		__xrtHttp1CallStreamClose(pCall, pStream);
	}
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = XNET_RESULT_OK;
	CallResult.Response = pResponse;
	CallResult.Buffered = iBuffered;
	CallResult.Reusable = bReusable;
	CallResult.Upgraded = bUpgraded;
	if ( bTransfer && (pCall->Transport == XRT_HTTP1_CALL_TCP) ) {
		CallResult.Tcp = (xnetstream*)pStream;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		if ( bTransfer &&
			(pCall->Transport == XRT_HTTP1_CALL_TLS) ) {
			CallResult.Tls = (xtlsstream*)pStream;
		}
	#endif
	__xrtHttp1CallExchangeUnit(pCall);
	pCall->Events.Done(
		pCall,
		&CallResult,
		pCall->Events.Data
	);
	if ( !bTransfer ) {
		__xrtHttp1CallStreamDestroy(pCall, pStream);
	}
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
	}
}



/* 精确消费已经被 Exchange 接受的传输输入。 */
static bool __xrtHttp1CallConsume(
	xhttp1call* pCall,
	ptr pStream,
	size_t iSize
)
{
	if ( iSize == 0 ) {
		return true;
	}
	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		return xrtNetStreamConsume(
			(xnetstream*)pStream,
			iSize
		) == iSize;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
		return xrtTlsStreamConsume(
			(xtlsstream*)pStream,
			iSize
		);
	#else
		return false;
	#endif
}



/* 向只读观察器报告已经确认的 I/O 进度。 */
static void __xrtHttp1CallProgress(
	xhttp1call* pCall,
	xhttp1progress Progress,
	size_t iBytes
)
{
	if ( pCall->Events.Progress != NULL ) {
		pCall->Events.Progress(
			pCall,
			Progress,
			iBytes,
			pCall->Events.Data
		);
	}
}



/* 在 Exchange 首次确认请求完整发送时发布唯一完成进度。 */
static void __xrtHttp1CallRequestDone(xhttp1call* pCall)
{
	if ( pCall->RequestDoneObserved ||
		!xrtHttp1ExchangeRequestComplete(
			pCall->Exchange
		) ) {
		return;
	}
	pCall->RequestDoneObserved = true;
	__xrtHttp1CallProgress(
		pCall,
		XHTTP1_PROGRESS_REQUEST_DONE,
		0
	);
}



/* 借用传输当前的第一个输入片段。 */
static bool __xrtHttp1CallInputFront(
	xhttp1call* pCall,
	ptr pStream,
	xnetspan* pSpan
)
{
	const xnetbuf* pBuffer;

	if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
		pBuffer = xrtNetStreamBuffer(
			(xnetstream*)pStream
		);
	} else {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
			pBuffer = xrtTlsStreamBuffer(
				(xtlsstream*)pStream
			);
		#else
			pBuffer = NULL;
		#endif
	}
	return (pBuffer != NULL) &&
		!xrtNetBufEmpty(pBuffer) &&
		xrtNetBufFront(pBuffer, pSpan);
}



/* 消费现有输入，终态后保留底层尚未接受的协议余量。 */
static bool __xrtHttp1CallDriveInput(
	xhttp1call* pCall,
	ptr pStream
)
{
	for ( ;; ) {
		xnetspan Span;
		xhttp1feedstatus Status;
		size_t iAccepted = 0;

		if ( !__xrtHttp1CallInputFront(
			pCall, pStream, &Span
		) ) {
			return true;
		}
		Status = xrtHttp1ExchangeFeed(
			pCall->Exchange,
			(xbytesview){ Span.Data, Span.Size },
			false,
			&iAccepted
		);
		if ( !__xrtHttp1CallConsume(
			pCall, pStream, iAccepted
		) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_TRANSPORT,
				XERR_INTERNAL,
				"consume-http1-response",
				"HTTP/1 call could not consume accepted transport input",
				xrtGetError()
			);
			return false;
		}
		if ( iAccepted != 0 ) {
			__xrtHttp1CallProgress(
				pCall,
				XHTTP1_PROGRESS_READ,
				iAccepted
			);
		}
		if ( Status == XHTTP1_FEED_PAUSED ) {
			xrtAtomic32Store(
				&pCall->PauseGate,
				1,
				XMEMORY_RELEASE
			);
			__xrtHttp1CallPauseTransport(pCall, pStream);
			return true;
		}
		if ( Status == XHTTP1_FEED_ERROR ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_PROTOCOL,
				"parse-http1-response",
				"HTTP/1 response parsing failed",
				xrtHttp1ExchangeError(pCall->Exchange)
			);
			return false;
		}
		if ( (Status == XHTTP1_FEED_DONE) ||
			(Status == XHTTP1_FEED_UPGRADED) ) {
			__xrtHttp1CallFinishSuccess(pCall);
			return false;
		}
		if ( iAccepted == 0 ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_INTERNAL,
				"parse-http1-response",
				"HTTP/1 Exchange made no input progress",
				NULL
			);
			return false;
		}
	}
}



/* TCP 租约离队后推进 Exchange，异常终态只负责延迟回收。 */
static void __xrtHttp1CallTcpOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xhttp1call* pCall = (xhttp1call*)pContext;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);

	if ( pCall->OutputQueued &&
		(pCall->OutputBytes == iSize) ) {
		pCall->OutputQueued = false;
		pCall->OutputBytes = 0;
		if ( !xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) && (pCall->Exchange != NULL) &&
			(pStream != NULL) &&
			(xrtNetStreamState(pStream) ==
			 XNET_STREAM_OPEN) ) {
			if ( !xrtHttp1ExchangeOutputConsume(
				pCall->Exchange,
				iSize
			) ) {
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_INTERNAL,
					"consume-http1-request",
					"HTTP/1 Exchange rejected a completed TCP output lease",
					xrtGetError()
				);
			} else {
				__xrtHttp1CallProgress(
					pCall,
					XHTTP1_PROGRESS_WRITE,
					iSize
				);
				__xrtHttp1CallRequestDone(pCall);
				__xrtHttp1CallDrive(pCall);
			}
		} else if ( xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttp1CallExchangeUnit(pCall);
		}
	}
	(void)pData;
	xrtHttp1CallDestroy(pCall);
}



/* 把一段 Exchange 输出以稳定引用交给 TCP 发送队列。 */
static xnetresult __xrtHttp1CallSendTcp(
	xhttp1call* pCall,
	xnetstream* pStream,
	xbytesview Data
)
{
	xnetresult Result;

	if ( xrtRefRetain(&pCall->References) < 0 ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"send-http1-request",
			"HTTP/1 call reference is no longer valid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pCall->OutputQueued = true;
	pCall->OutputBytes = Data.Size;
	Result = xrtNetStreamSendRef(
		pStream,
		Data.Data,
		Data.Size,
		__xrtHttp1CallTcpOutputRelease,
		pCall
	);
	if ( Result != XNET_RESULT_OK ) {
		pCall->OutputQueued = false;
		pCall->OutputBytes = 0;
		xrtHttp1CallDestroy(pCall);
	}
	return Result;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 把一段 Exchange 输出同步编码到 TLS，并保留成功短写语义。 */
static xnetresult __xrtHttp1CallSendTls(
	xhttp1call* pCall,
	xtlsstream* pStream,
	xbytesview Data,
	size_t* pWritten
)
{
	xtlsresult Result = xrtTlsStreamSend(
		pStream,
		Data.Data,
		Data.Size,
		pWritten
	);

	(void)pCall;
	if ( Result == XTLS_OK ) {
		return XNET_RESULT_OK;
	}
	if ( Result == XTLS_AGAIN ) {
		return XNET_RESULT_AGAIN;
	}
	return XNET_RESULT_ERROR;
}

#endif



/* 在当前硬预算内持续发送请求，直到正文或传输产生背压。 */
static bool __xrtHttp1CallDriveOutput(
	xhttp1call* pCall,
	ptr pStream
)
{
	if ( pCall->OutputWaiting || pCall->OutputQueued ) {
		return true;
	}
	for ( ;; ) {
		xbytesview Data;
		xhttp1outputstatus Status;
		xnetresult Result;
		size_t iMaximum = pCall->Config.WriteSize;
		size_t iWritten = 0;

		if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
			size_t iWritable = xrtNetStreamWritable(
				(const xnetstream*)pStream
			);

			if ( iWritable == 0 ) {
				return true;
			}
			if ( iMaximum > iWritable ) {
				iMaximum = iWritable;
			}
		}
		Status = xrtHttp1ExchangeOutput(
			pCall->Exchange,
			iMaximum,
			&Data
		);
		if ( Status == XHTTP1_OUTPUT_ERROR ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_IO,
				"produce-http1-request",
				"HTTP/1 request output failed",
				xrtHttp1ExchangeError(pCall->Exchange)
			);
			return false;
		}
		if ( Status == XHTTP1_OUTPUT_AGAIN ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
				if ( !__xrtHttp1CallAsyncWait(pCall) ) {
					__xrtHttp1CallFail(
						pCall,
						XHTTP1_CALL_ERROR_EXCHANGE,
						XERR_IO,
						"wait-http-request-body",
						"HTTP request body readiness wait failed",
						xrtGetError()
					);
					return false;
				}
				return true;
			#else
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_UNSUPPORTED,
					"wait-http-request-body",
					"asynchronous HTTP request body support is disabled",
					xrtHttp1ExchangeError(pCall->Exchange)
				);
				return false;
			#endif
		}
		if ( (Status == XHTTP1_OUTPUT_CONTINUE) ||
			(Status == XHTTP1_OUTPUT_DONE) ) {
			__xrtHttp1CallRequestDone(pCall);
			return true;
		}
		if ( (Status != XHTTP1_OUTPUT_DATA) ||
			(Data.Data == NULL) || (Data.Size == 0) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_EXCHANGE,
				XERR_INTERNAL,
				"produce-http1-request",
				"HTTP/1 Exchange returned invalid output",
				NULL
			);
			return false;
		}
		if ( pCall->Transport == XRT_HTTP1_CALL_TCP ) {
			Result = __xrtHttp1CallSendTcp(
				pCall,
				(xnetstream*)pStream,
				Data
			);
			if ( Result == XNET_RESULT_OK ) {
				return true;
			}
		} else {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)
				Result = __xrtHttp1CallSendTls(
					pCall,
					(xtlsstream*)pStream,
					Data,
					&iWritten
				);
			#else
				Result = XNET_RESULT_ERROR;
			#endif
		}
		if ( iWritten != 0 ) {
			if ( !xrtHttp1ExchangeOutputConsume(
				pCall->Exchange,
				iWritten
			) ) {
				__xrtHttp1CallFail(
					pCall,
					XHTTP1_CALL_ERROR_EXCHANGE,
					XERR_INTERNAL,
					"consume-http1-request",
					"HTTP/1 Exchange rejected acknowledged output",
					xrtGetError()
				);
				return false;
			}
			__xrtHttp1CallProgress(
				pCall,
				XHTTP1_PROGRESS_WRITE,
				iWritten
			);
			__xrtHttp1CallRequestDone(pCall);
			continue;
		}
		if ( Result == XNET_RESULT_AGAIN ) {
			return true;
		}
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_TRANSPORT,
			XERR_IO,
			"send-http1-request",
			"HTTP/1 request transport send failed",
			xrtGetError()
		);
		return false;
	}
}



/* 串行折叠发送、接收和同步水位回调。 */
void __xrtHttp1CallDrive(xhttp1call* pCall)
{
	ptr pStream;

	if ( pCall->Driving ) {
		pCall->DriveAgain = true;
		return;
	}
	/* 传输事件已经由运行时引用保证对象有效，只增加当前驱动引用。 */
	if ( xrtRefRetain(&pCall->References) < 0 ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"drive-http1-call",
			"HTTP/1 call reference is no longer valid",
			NULL
		);
		return;
	}
	pCall->Driving = true;
	do {
		pCall->DriveAgain = false;
		if ( xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_CANCELLED,
				XERR_CANCELLED,
				"cancel-http1-call",
				"HTTP/1 call was cancelled",
				NULL
			);
			break;
		}
		pStream = xrtAtomicPtrLoad(
			&pCall->Stream,
			XMEMORY_ACQUIRE
		);
		if ( pStream == NULL ) {
			break;
		}
		if ( !__xrtHttp1CallDriveInput(
			pCall, pStream
		) || xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		if ( !__xrtHttp1CallDriveOutput(
			pCall, pStream
		) || xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
		(void)__xrtHttp1CallDriveInput(
			pCall, pStream
		);
	} while ( pCall->DriveAgain && !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) );
	pCall->Driving = false;
	if ( pCall->InputEnded &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttp1CallEnd(pCall);
	}
	xrtHttp1CallDestroy(pCall);
}



/* TCP 输入到达后推进完整双向状态机。 */
static void __xrtHttp1CallTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TCP 写队列重新具备容量后继续发送请求。 */
static void __xrtHttp1CallTcpWritable(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TCP 写队列完全排空时继续同步正文源。 */
static void __xrtHttp1CallTcpDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* 把 TCP FIN 作为 HTTP/1 close-delimited 输入终点。 */
static void __xrtHttp1CallEnd(xhttp1call* pCall)
{
	size_t iAccepted = 0;
	xhttp1feedstatus Status;

	pCall->InputEnded = true;
	if ( pCall->Driving ||
		xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		return;
	}
	Status = xrtHttp1ExchangeFeed(
		pCall->Exchange,
		(xbytesview){ NULL, 0 },
		true,
		&iAccepted
	);

	if ( Status == XHTTP1_FEED_PAUSED ) {
		return;
	}
	if ( (Status == XHTTP1_FEED_DONE) ||
		(Status == XHTTP1_FEED_UPGRADED) ) {
		__xrtHttp1CallFinishSuccess(pCall);
		return;
	}
	__xrtHttp1CallFail(
		pCall,
		XHTTP1_CALL_ERROR_EXCHANGE,
		XERR_PROTOCOL,
		"finish-http1-response",
		"HTTP/1 response ended before Exchange completion",
		xrtHttp1ExchangeError(pCall->Exchange)
	);
}



/* TCP 对端结束读方向时完成 close-delimited 响应。 */
static void __xrtHttp1CallTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallEnd((xhttp1call*)pData);
}



/* TCP 在 HTTP 终态前关闭时发布传输失败。 */
static void __xrtHttp1CallTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pStream;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XERR_CANCELLED : XERR_IO,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ?
				XHTTP1_CALL_ERROR_CANCELLED :
				XHTTP1_CALL_ERROR_TRANSPORT,
			"close-http1-transport",
			"HTTP/1 TCP transport closed before completion",
			pError
		);
	}
	__xrtHttp1CallFinishFailure(pCall, Result, pError);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* TLS 明文到达后推进完整双向状态机。 */
static void __xrtHttp1CallTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TLS 记录和底层 TCP 重新具备容量后继续发送。 */
static void __xrtHttp1CallTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* TLS 密文与底层 TCP 都排空时继续同步正文源。 */
static void __xrtHttp1CallTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallDrive((xhttp1call*)pData);
}



/* 对端认证关闭读方向时完成 close-delimited 响应。 */
static void __xrtHttp1CallTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttp1CallEnd((xhttp1call*)pData);
}



/* TLS 在 HTTP 终态前关闭时发布传输失败。 */
static void __xrtHttp1CallTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pStream;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( pCall->Error == NULL ) {
		pCall->Error = __xrtHttp1CallErrorCreate(
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ? XERR_CANCELLED : XERR_IO,
			xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_ACQUIRE
			) ?
				XHTTP1_CALL_ERROR_CANCELLED :
				XHTTP1_CALL_ERROR_TRANSPORT,
			"close-http1-transport",
			"HTTP/1 TLS transport closed before completion",
			pError
		);
	}
	__xrtHttp1CallFinishFailure(pCall, Result, pError);
}

#endif



/* 初始化不常驻缓冲的单次输出上限。 */
XHTTP_CLIENT_API void xrtHttp1CallConfigInit(xhttp1callconfig* pConfig)
{
	const xhttp1callconfig Config = {
		XRT_HTTP1_CALL_WRITE_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"init-http1-call-config",
			"HTTP/1 call config range is invalid",
			NULL
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化调用事件表。 */
XHTTP_CLIENT_API void xrtHttp1CallEventsInit(xhttp1callevents* pEvents)
{
	const xhttp1callevents Events = { 0 };

	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"init-http1-call-events",
			"HTTP/1 call events range is invalid",
			NULL
		);
		return;
	}
	memcpy(pEvents, &Events, sizeof(Events));
}



/* 建立调用公共状态；成功后调用对象接管 Exchange。 */
static xhttp1call* __xrtHttp1CallCreate(
	xnetworker* pWorker,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents,
	xrt_http1_call_transport Transport
)
{
	xhttp1callconfig Config = {
		XRT_HTTP1_CALL_WRITE_DEFAULT
	};
	xhttp1callevents Events;
	xhttp1call* pCall;

	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttp1CallSetError(
				XERR_ARGUMENT,
				XHTTP1_CALL_ERROR_ARGUMENT,
				"create-http1-call",
				"HTTP/1 call config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-call",
			"HTTP/1 call events range is invalid",
			NULL
		);
		return NULL;
	}
	memcpy(&Events, pEvents, sizeof(Events));
	if ( (pWorker == NULL) ||
		!xrtNetWorkerIsCurrent(pWorker) ||
		(pExchange == NULL) || (Events.Done == NULL) ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-call",
			"HTTP/1 call requires an Exchange and its current Worker",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttp1CallConfigValid(&Config) ) {
		return NULL;
	}
	pCall = (xhttp1call*)xrtCalloc(
		1, sizeof(*pCall)
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	if ( !xrtNetPostInit(&pCall->StartPost) ||
		!xrtNetPostInit(&pCall->ResumePost)
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)
			|| !xrtNetPostInit(&pCall->OutputPost)
		#endif
	) {
		xrtFree(pCall);
		return NULL;
	}
	if ( !xrtSpinInit(&pCall->Lock) ) {
		xrtFree(pCall);
		return NULL;
	}
	pCall->References = 2;
	xrtAtomic32Init(
		&pCall->State,
		XHTTP1_CALL_RUNNING
	);
	xrtAtomic32Init(&pCall->CancelGate, 0);
	xrtAtomic32Init(&pCall->FinishGate, 0);
	xrtAtomic32Init(&pCall->PauseGate, 0);
	xrtAtomic32Init(&pCall->ResumeGate, 0);
	xrtAtomicPtrInit(&pCall->Stream, NULL);
	pCall->Worker = pWorker;
	pCall->Exchange = pExchange;
	pCall->Config = Config;
	pCall->Events = Events;
	pCall->Transport = Transport;
	pCall->RuntimeHeld = true;
	return pCall;
}



/* 当前构造回调返回后，在所属 Worker 上启动第一次双向驱动。 */
static void __xrtHttp1CallStart(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pWorker;
	__xrtHttp1CallDrive(pCall);
	xrtHttp1CallDestroy(pCall);
}



/* 接管 TCP 事件，并把第一次驱动延迟到当前 Worker 回调返回之后。 */
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallTcp(
	xnetstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
)
{
	xnetstreamevents Events;
	xhttp1call* pCall;

	if ( pStream == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-tcp-call",
			"HTTP/1 TCP call stream is null",
			NULL
		);
		return NULL;
	}
	pCall = __xrtHttp1CallCreate(
		xrtNetStreamWorker(pStream),
		pExchange,
		pConfig,
		pEvents,
		XRT_HTTP1_CALL_TCP
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = __xrtHttp1CallTcpRead;
	Events.End = __xrtHttp1CallTcpEnd;
	Events.LowWater = __xrtHttp1CallTcpWritable;
	Events.Drain = __xrtHttp1CallTcpDrain;
	Events.Close = __xrtHttp1CallTcpClose;
	if ( !xrtNetStreamSetEvents(
		pStream,
		&Events,
		pCall
	) ) {
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pCall->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	(void)xrtHttp1CallRef(pCall);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttp1CallStart,
		pCall
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		memset(&Events, 0, sizeof(Events));
		(void)xrtNetStreamSetEvents(pStream, &Events, NULL);
		xrtAtomicPtrStore(&pCall->Stream, NULL, XMEMORY_RELEASE);
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		__xrtHttp1CallSetError(
			xrtErrorKind(pCause),
			XHTTP1_CALL_ERROR_STATE,
			"start-http1-tcp-call",
			"HTTP/1 TCP call could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	return pCall;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_TLS)

/* 接管 TLS 明文事件，并把第一次驱动延迟到当前 Worker 回调返回之后。 */
XHTTP_CLIENT_API xhttp1call* xrtHttp1CallTls(
	xtlsstream* pStream,
	xhttp1exchange* pExchange,
	const xhttp1callconfig* pConfig,
	const xhttp1callevents* pEvents
)
{
	xtlsstreamevents Events;
	xhttp1call* pCall;

	if ( pStream == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"create-http1-tls-call",
			"HTTP/1 TLS call stream is null",
			NULL
		);
		return NULL;
	}
	pCall = __xrtHttp1CallCreate(
		xrtNetStreamWorker(
			xrtTlsStreamTransport(pStream)
		),
		pExchange,
		pConfig,
		pEvents,
		XRT_HTTP1_CALL_TLS
	);
	if ( pCall == NULL ) {
		return NULL;
	}
	memset(&Events, 0, sizeof(Events));
	Events.Read = __xrtHttp1CallTlsRead;
	Events.End = __xrtHttp1CallTlsEnd;
	Events.Writable = __xrtHttp1CallTlsWritable;
	Events.Drain = __xrtHttp1CallTlsDrain;
	Events.Close = __xrtHttp1CallTlsClose;
	if ( !xrtTlsStreamSetEvents(
		pStream,
		&Events,
		pCall
	) ) {
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pCall->Stream,
		pStream,
		XMEMORY_RELEASE
	);
	(void)xrtHttp1CallRef(pCall);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttp1CallStart,
		pCall
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		memset(&Events, 0, sizeof(Events));
		(void)xrtTlsStreamSetEvents(pStream, &Events, NULL);
		xrtAtomicPtrStore(&pCall->Stream, NULL, XMEMORY_RELEASE);
		pCall->Exchange = NULL;
		pCall->RuntimeHeld = false;
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		xrtHttp1CallDestroy(pCall);
		__xrtHttp1CallSetError(
			xrtErrorKind(pCause),
			XHTTP1_CALL_ERROR_STATE,
			"start-http1-tls-call",
			"HTTP/1 TLS call could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	return pCall;
}

#endif



/* 从任意线程请求取消，最终完成由所属 Worker 串行发布。 */
XHTTP_CLIENT_API bool xrtHttp1CallCancel(xhttp1call* pCall)
{
	ptr pOwned;
	ptr pStream;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"cancel-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_RELAXED
	);
	pOwned = __xrtHttp1CallStreamRef(pCall, pStream);
	if ( (pStream != NULL) && (pOwned == NULL) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttp1CallStreamAbort(pCall, pOwned);
	__xrtHttp1CallStreamDestroy(pCall, pOwned);
	return true;
}



/* 在所属 Worker 上立即暂停 Exchange 与实际 TCP 读门。 */
XHTTP_CLIENT_API bool xrtHttp1CallPause(xhttp1call* pCall)
{
	ptr pStream;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"pause-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pCall->Worker) ) {
		__xrtHttp1CallSetError(
			XERR_STATE,
			XHTTP1_CALL_ERROR_STATE,
			"pause-http1-call",
			"HTTP/1 call pause must run on its Worker",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	if ( xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( !xrtHttp1ExchangePause(pCall->Exchange) ) {
		return false;
	}
	xrtAtomic32Store(
		&pCall->PauseGate,
		1,
		XMEMORY_RELEASE
	);
	pStream = xrtAtomicPtrLoad(
		&pCall->Stream,
		XMEMORY_ACQUIRE
	);
	__xrtHttp1CallPauseTransport(pCall, pStream);
	return true;
}



/* 在 Worker 上恢复协议输入，先消费已缓冲数据，再重新开放 TCP 读取。 */
static void __xrtHttp1CallResumeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;
	ptr pStream;

	(void)pWorker;
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) || (pCall->Exchange == NULL) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttp1CallDestroy(pCall);
		return;
	}
	if ( !xrtHttp1ExchangeResume(pCall->Exchange) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			XERR_STATE,
			"resume-http1-response",
			"HTTP/1 Exchange could not resume response input",
			xrtGetError()
		);
		xrtHttp1CallDestroy(pCall);
		return;
	}
	xrtAtomic32Store(
		&pCall->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	__xrtHttp1CallDrive(pCall);
	if ( !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) && !xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		pStream = xrtAtomicPtrLoad(
			&pCall->Stream,
			XMEMORY_ACQUIRE
		);
		if ( (pStream != NULL) &&
			!__xrtHttp1CallResumeTransport(pCall, pStream) ) {
			__xrtHttp1CallFail(
				pCall,
				XHTTP1_CALL_ERROR_TRANSPORT,
				XERR_IO,
				"resume-http1-response",
				"HTTP/1 call transport could not resume input",
				xrtGetError()
			);
		}
	}
	xrtHttp1CallDestroy(pCall);
}



/* 从任意线程合并提交一次恢复命令。 */
XHTTP_CLIENT_API bool xrtHttp1CallResume(xhttp1call* pCall)
{
	uint32 iExpected = 0;

	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"resume-http1-call",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || !xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_RELAXED
	) || !xrtAtomic32CompareExchange(
		&pCall->ResumeGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_RELAXED
	) || (xrtRefRetain(&pCall->References) < 0) ) {
		if ( iExpected == 0 ) {
			xrtAtomic32Store(
				&pCall->ResumeGate,
				0,
				XMEMORY_RELEASE
			);
		}
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->ResumePost,
		__xrtHttp1CallResumeTask,
		pCall
	) ) {
		xrtAtomic32Store(
			&pCall->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttp1CallDestroy(pCall);
		return false;
	}
	return true;
}



/* 返回暂停门状态；恢复命令真正执行前仍报告暂停。 */
XHTTP_CLIENT_API bool xrtHttp1CallPaused(const xhttp1call* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-pause",
			"HTTP/1 call is null",
			NULL
		);
		return false;
	}
	return xrtAtomic32Load(
		&pCall->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
}



/* 返回调用状态快照。 */
XHTTP_CLIENT_API xhttp1callstate xrtHttp1CallState(
	const xhttp1call* pCall
)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-state",
			"HTTP/1 call is null",
			NULL
		);
		return XHTTP1_CALL_FAILED;
	}
	return (xhttp1callstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回终态错误借用。 */
XHTTP_CLIENT_API const xerror* xrtHttp1CallError(
	const xhttp1call* pCall
)
{
	if ( pCall == NULL ) {
		__xrtHttp1CallSetError(
			XERR_ARGUMENT,
			XHTTP1_CALL_ERROR_ARGUMENT,
			"query-http1-call-error",
			"HTTP/1 call is null",
			NULL
		);
		return NULL;
	}
	return xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	) == XHTTP1_CALL_RUNNING ? NULL : pCall->Error;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_runtime.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT)



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

/* HTTP Client 默认把一次完整请求限制在三十秒内。 */
#define XRT_HTTP_CLIENT_TIMEOUT_DEFAULT_VALUE INT64_C(30000)
#define XRT_HTTP_CLIENT_IDLE_TIMEOUT_DEFAULT_VALUE INT64_C(30000)

#define XRT_HTTP_TIMEOUT_NONE	UINT32_C(0)
#define XRT_HTTP_TIMEOUT_TOTAL	UINT32_C(1)
#define XRT_HTTP_TIMEOUT_IDLE	UINT32_C(2)



/* 建立客户端域错误并保留明确的下层原因。 */
xerror* __xrtHttpClientErrorCreate(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.http.client";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 判断错误是否已经是所需的高层 HTTP Client 终态错误。 */
static bool __xrtHttpCallErrorIs(
	const xerror* pError,
	xerrkind Kind,
	xhttpclienterror Code
)
{
	return (pError != NULL) &&
		(xrtErrorKind(pError) == Kind) &&
		(xrtErrorCode(pError) == (int32)Code) &&
		(strcmp(
			xrtErrorDomain(pError),
			"xrt.http.client"
		) == 0);
}



/*
	建立取消或超时终态错误并接管旧错误。
	分配失败时退回核心静态错误，保证终态类别仍然可靠。
*/
xerror* __xrtHttpClientTerminalError(
	xerror* pCause,
	xhttpclienterror Code
)
{
	xerrkind Kind;
	cstr sOperation;
	cstr sMessage;
	xerror* pError;

	if ( Code == XHTTP_CLIENT_ERROR_TIMEOUT_IDLE ) {
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage =
			"HTTP call made no progress before its idle deadline";
	} else if ( Code == XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL ) {
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage = "HTTP call exceeded its total deadline";
	} else {
		Code = XHTTP_CLIENT_ERROR_CANCELLED;
		Kind = XERR_CANCELLED;
		sOperation = "cancel-http-call";
		sMessage = "HTTP call was cancelled";
	}
	if ( __xrtHttpCallErrorIs(pCause, Kind, Code) ) {
		return pCause;
	}
	pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	if ( __xrtHttpCallErrorIs(pError, Kind, Code) ) {
		xrtErrorFree(pCause);
		return pError;
	}
	xrtErrorFree(pError);
	xrtErrorFree(pCause);
	xrtSetErrorKind(Kind);
	return xrtErrorRef(xrtGetError());
}



/* 设置创建阶段当前线程错误。 */
void __xrtHttpClientSetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 沿原因链返回最内层有效错误类别。 */
xerrkind __xrtHttpClientCauseKind(
	const xerror* pError,
	xerrkind Fallback
)
{
	xerrkind Kind = Fallback;

	while ( pError != NULL ) {
		if ( xrtErrorKind(pError) != XERR_NONE ) {
			Kind = xrtErrorKind(pError);
		}
		pError = xrtErrorCause(pError);
	}
	return Kind;
}



/*
	接管当前错误；已经属于 HTTP Client 域时原样保留，
	否则建立稳定高层分类并把原错误挂入 Cause。
*/
static xerror* __xrtHttpClientPromoteOwned(
	xerror* pCause,
	xhttpclienterror Code,
	xerrkind Fallback,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError;
	cstr sDomain = pCause != NULL ?
		xrtErrorDomain(pCause) : NULL;

	if ( (sDomain != NULL) &&
		(strcmp(sDomain, "xrt.http.client") == 0) ) {
		return pCause;
	}
	pError = __xrtHttpClientErrorCreate(
		__xrtHttpClientCauseKind(pCause, Fallback),
		Code,
		sOperation,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return pError;
}



/* 把下层配置错误提升为 Client 创建阶段的统一错误。 */
static bool __xrtHttpClientConfigFailure(cstr sMessage)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		XHTTP_CLIENT_ERROR_CONFIG,
		XERR_ARGUMENT,
		"configure-http-client",
		sMessage
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 只写入尚未到达的第一个时间点。 */
static bool __xrtHttpCallFirstTime(
	xatomic64* pTime,
	xtime iNow
)
{
	uint64 iExpected = 0;

	return xrtAtomic64CompareExchange(
		pTime,
		&iExpected,
		(uint64)iNow,
		XMEMORY_RELEASE,
		XMEMORY_RELAXED
	);
}



/* 把有限相对时长转换为 Engine 可调度的最大有限 deadline。 */
static double __xrtHttpCallDeadline(double iNow, int64 iTimeout)
{
    if (iTimeout == XRT_WAIT_FOREVER) return INFINITY;
    if (iTimeout < 0 || !isfinite(iNow)) { __xrtWaitInvalid(); return NAN; }
    if (iTimeout == 0) return iNow;
    return nextafter(iNow + (double)(iTimeout / 1000) + (double)(iTimeout % 1000) * 0.001, INFINITY);
}



/* 按最后一次真实进度刷新 idle deadline，不创建新 Timer。 */
static void __xrtHttpCallProgressAt(
	xhttpcall* pCall,
	double iNow
)
{
	double iDeadline;

	xrtAtomic64Store(
		&pCall->Info.LastProgress,
		(uint64)xrtNow(),
		XMEMORY_RELEASE
	);
	if ( pCall->IdleTimeout == XHTTP_CLIENT_TIMEOUT_NONE ) {
		return;
	}
	iDeadline = __xrtHttpCallDeadline(
		iNow,
		pCall->IdleTimeout
	);
	xrtAtomic64Store(
		&pCall->IdleDeadline,
		__xrtWaitTimerBits(iDeadline),
		XMEMORY_RELEASE
	);
}



/* 发布高层阶段。 */
void __xrtHttpCallSetPhase(
	xhttpcall* pCall,
	xhttpcallphase Phase
)
{
	if ( pCall != NULL ) {
		xrtAtomic32Store(
			&pCall->Info.Phase,
			(uint32)Phase,
			XMEMORY_RELEASE
		);
	}
}



/* 标记当前 Hop 的传输已经可以承载 HTTP。 */
void __xrtHttpCallTransportReady(xhttpcall* pCall)
{
	double iNow;

	if ( pCall == NULL ) {
		return;
	}
	iNow = xrtTimer();
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.TransportReady,
		(uint64)xrtNow()
	);
	__xrtHttpCallProgressAt(pCall, iNow);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_REQUEST
	);
}



/* 标记当前 Hop 复用了既有连接。 */
void __xrtHttpCallReused(xhttpcall* pCall)
{
	if ( pCall != NULL ) {
		xrtAtomic32Store(
			&pCall->Info.Reused,
			1,
			XMEMORY_RELEASE
		);
	}
}



/* 把低级 HTTP/1 的真实 I/O 进度累计到高层 Call 快照。 */
static void __xrtHttpCallStreamProgress(
	xhttp1call* pStreamCall,
	xhttp1progress Progress,
	size_t iBytes,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	double iNow = xrtTimer();

	(void)pStreamCall;
	__xrtHttpCallProgressAt(pCall, iNow);
	if ( Progress == XHTTP1_PROGRESS_WRITE ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.RequestWireBytes,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
	} else if ( Progress == XHTTP1_PROGRESS_READ ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.ResponseWireBytes,
			(uint64)iBytes,
			XMEMORY_RELAXED
		);
		if ( __xrtHttpCallFirstTime(
			&pCall->Info.FirstByte,
			(uint64)xrtNow()
		) ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_RESPONSE_HEADERS
			);
		}
	} else if ( Progress ==
		XHTTP1_PROGRESS_REQUEST_DONE ) {
		(void)__xrtHttpCallFirstTime(
			&pCall->Info.RequestSent,
			(uint64)xrtNow()
		);
		if ( xrtAtomic64Load(
			&pCall->Info.FirstByte,
			XMEMORY_ACQUIRE
		) == 0 ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_RESPONSE_HEADERS
			);
		}
	}
}



/* 建立高层 Client 复用的低级 HTTP/1 Call 事件表。 */
void __xrtHttpCallStreamEvents(
	xhttpcall* pCall,
	xhttp1callevents* pEvents
)
{
	xrtHttp1CallEventsInit(pEvents);
	pEvents->Done = __xrtHttpClientStreamDone;
	pEvents->Progress = __xrtHttpCallStreamProgress;
	pEvents->Data = pCall;
}



/* 初始化 Client 的完整默认策略。 */
XHTTP_CLIENT_API void xrtHttpClientConfigInit(xhttpclientconfig* pConfig)
{
	xhttpclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-config",
			"HTTP client config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtNetResolverConfigInit(&Config.Resolver);
	xrtNetDialConfigInit(&Config.Dial);
	xrtHttp1CallConfigInit(&Config.Call);
	xrtHttp1ExchangeConfigInit(&Config.Exchange);
	Config.Timeout = XRT_HTTP_CLIENT_TIMEOUT_DEFAULT_VALUE;
	Config.IdleTimeout =
		XRT_HTTP_CLIENT_IDLE_TIMEOUT_DEFAULT_VALUE;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Config.Proxy = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtHttpRedirectConfigInit(&Config.Redirect);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtHttpRetryConfigInit(&Config.Retry);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xrtHttpClientPoolConfigInit(&Config.Pool);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xrtHttpDecompressConfigInit(
			&Config.Decompress
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtHttpClientCacheConfigInit(
			&Config.Cache
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtTlsStreamConfigInit(&Config.TlsStream);
		Config.SystemTrust = true;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtHttpResumeConfigInit(&Config.Resume);
	#endif
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化单次调用选项。 */
XHTTP_CLIENT_API void xrtHttpCallOptionsInit(xhttpcalloptions* pOptions)
{
	xhttpcalloptions Options;

	if ( !xrtMemRangeValid(pOptions, sizeof(*pOptions)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-call-options",
			"HTTP call options range is invalid",
			NULL
		);
		return;
	}
	memset(&Options, 0, sizeof(Options));
	xrtHttp1RequestOptionsInit(&Options.Request);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtHttpProxyOptionsInit(&Options.Proxy);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xrtHttpCookieOptionsInit(&Options.Cookies);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtHttpRetryOptionsInit(&Options.Retry);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtHttpClientCacheOptionsInit(
			&Options.Cache
		);
	#endif
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 检查 Client 层必须立即确定的配置边界。 */
static bool __xrtHttpClientConfigValid(
	const xhttpclientconfig* pConfig
)
{
	if ( (pConfig == NULL) ||
		(pConfig->Timeout == 0) ||
		(pConfig->IdleTimeout == 0) ||
        (pConfig->Timeout < XRT_WAIT_FOREVER) ||
        (pConfig->IdleTimeout < XRT_WAIT_FOREVER) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client",
			"HTTP client write size and timeout policies must be non-zero",
			NULL
		);
		return false;
	}
	if ( !xrtNetDialConfigValid(&pConfig->Dial) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client Dial policy is invalid"
		);
	}
	if ( !__xrtHttp1CallConfigValid(&pConfig->Call) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client transport policy is invalid"
		);
	}
	if ( !xrtHttpBridge1ExchangeConfigValid(
		&pConfig->Exchange
	) ) {
		return __xrtHttpClientConfigFailure(
			"HTTP client response policy is invalid"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( (pConfig->Redirect.Flags &
			~(XHTTP_REDIRECT_POST_TO_GET |
			  XHTTP_REDIRECT_FORWARD_CREDENTIALS |
			  XHTTP_REDIRECT_ALLOW_DOWNGRADE)) != 0 ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP redirect flags are invalid",
				NULL
			);
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( ((pConfig->Retry.Flags &
			~(XHTTP_RETRY_STATUS |
			  XHTTP_RETRY_TRANSPORT |
			  XHTTP_RETRY_RESPECT_AFTER |
			  XHTTP_RETRY_JITTER)) != 0) ||
			(pConfig->Retry.MaxDelay == 0) ||
			(pConfig->Retry.BaseDelay >
			 pConfig->Retry.MaxDelay) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP retry policy is invalid",
				NULL
			);
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( (pConfig->Decompress.MaxBody == 0) ||
			(pConfig->Decompress.MaxCodings == 0) ||
			(pConfig->Decompress.MaxCodings >
			 XHTTP_DECOMPRESS_CODINGS_MAX) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"configure-http-client",
				"HTTP decompression limits are invalid",
				NULL
			);
			return false;
		}
	#endif
	return true;
}



/* 增加不改变公开 Owner 数量的内部引用。 */
xhttpclient* __xrtHttpClientHold(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(xrtRefRetain(&pClient->References) < 0) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client is null or already released",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放最后一个内部引用和 Client 拥有的共享资源。 */
void __xrtHttpClientRelease(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(xrtRefRelease(&pClient->References) != 0) ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		__xrtHttpResumeUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpClientTlsUnit(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xrtCookieJarRelease(pClient->Cookies);
		pClient->Cookies = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		__xrtHttpClientCacheClose(pClient);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pClient->Proxy);
		pClient->Proxy = NULL;
		pClient->Config.Proxy = NULL;
	#endif
	if ( pClient->OwnResolver && (pClient->Resolver != NULL) ) {
		(void)xrtNetResolverDestroy(pClient->Resolver);
		pClient->Resolver = NULL;
	}
	if ( pClient->EngineHeld ) {
		pClient->EngineHeld = false;
		(void)xrtNetEngineUnpin(pClient->Engine);
	}
	(void)xrtSpinUnit(&pClient->LifecycleLock);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}



/* 判断连接池拥有的异步传输和 Timer 是否已经完全退出。 */
static bool __xrtHttpClientPoolStopped(xhttpclient* pClient)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !pClient->PoolReady ) {
			return true;
		}
		return xrtAtomic64Load(
			&pClient->PoolLive,
			XMEMORY_ACQUIRE
		) == 0;
	#else
		(void)pClient;
		return true;
	#endif
}



/* 在全部活动对象退出后发布唯一 Client 关闭终态。 */
void __xrtHttpClientTryFinish(xhttpclient* pClient)
{
	xhttpclientstate State;
	bool bPoolStopped;
	bool bFinished = false;

	if ( pClient == NULL ) {
		return;
	}
	/* 池互斥锁可能阻塞，不能在生命周期自旋锁内等待。 */
	bPoolStopped = __xrtHttpClientPoolStopped(pClient);
	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	if ( ((State == XHTTP_CLIENT_DRAINING) ||
		  (State == XHTTP_CLIENT_ABORTING)) &&
		(pClient->ActiveCalls == 0) &&
		bPoolStopped ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_CLOSED,
			XMEMORY_RELEASE
		);
		bFinished = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)
		if ( bFinished ) {
			__xrtHttpClientFutureFinish(pClient);
		}
	#else
		(void)bFinished;
	#endif
}



/* 在仍运行的 Client 中登记一个已经准备完成的 Call。 */
bool __xrtHttpClientCallAttach(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	bool bAttached = false;

	(void)xrtSpinLock(&pClient->LifecycleLock);
	if ( xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) == XHTTP_CLIENT_RUNNING ) {
		pCall->ClientPrevious = pClient->CallTail;
		pCall->ClientNext = NULL;
		if ( pClient->CallTail != NULL ) {
			pClient->CallTail->ClientNext = pCall;
		} else {
			pClient->CallHead = pCall;
		}
		pClient->CallTail = pCall;
		pClient->ActiveCalls++;
		pCall->ClientLinked = true;
		bAttached = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( !bAttached ) {
		__xrtHttpClientSetError(
			XERR_CLOSED,
			XHTTP_CLIENT_ERROR_STATE,
			"run-http-client",
			"HTTP client is not accepting new calls",
			NULL
		);
	}
	return bAttached;
}



/* 用户完成回调返回后从 Client 活动表摘除 Call。 */
void __xrtHttpClientCallDetach(xhttpcall* pCall)
{
	xhttpclient* pClient;
	bool bDetached = false;

	if ( (pCall == NULL) || !pCall->ClientLinked ) {
		return;
	}
	pClient = pCall->Client;
	(void)xrtSpinLock(&pClient->LifecycleLock);
	if ( pCall->ClientLinked ) {
		if ( pCall->ClientPrevious != NULL ) {
			pCall->ClientPrevious->ClientNext =
				pCall->ClientNext;
		} else {
			pClient->CallHead = pCall->ClientNext;
		}
		if ( pCall->ClientNext != NULL ) {
			pCall->ClientNext->ClientPrevious =
				pCall->ClientPrevious;
		} else {
			pClient->CallTail = pCall->ClientPrevious;
		}
		pCall->ClientPrevious = NULL;
		pCall->ClientNext = NULL;
		pCall->ClientLinked = false;
		pClient->ActiveCalls--;
		bDetached = true;
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bDetached ) {
		__xrtHttpClientTryFinish(pClient);
	}
}



/* 增加一个可以独立调用 Destroy 的公开 Owner 引用。 */
XHTTP_CLIENT_API xhttpclient* xrtHttpClientRef(xhttpclient* pClient)
{
	if ( (pClient == NULL) ||
		(__xrtHttpClientHold(pClient) == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client is null or already released",
			NULL
		);
		return NULL;
	}
	if ( xrtRefRetain(&pClient->Owners) < 0 ) {
		__xrtHttpClientRelease(pClient);
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-client",
			"HTTP client has no remaining public owner",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放公开 Owner；最后一个 Owner 会隐式开始平滑排空。 */
XHTTP_CLIENT_API void xrtHttpClientDestroy(xhttpclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pClient->Owners) == 0 ) {
		(void)xrtHttpClientDrain(pClient);
	}
	__xrtHttpClientRelease(pClient);
}



/* 返回 Client 当前生命周期状态。 */
XHTTP_CLIENT_API xhttpclientstate xrtHttpClientState(
	const xhttpclient* pClient
)
{
	return pClient != NULL ?
		(xhttpclientstate)xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) : XHTTP_CLIENT_CLOSED;
}



/* 返回 Client 创建时绑定且由 Client 生命周期保留的网络 Engine。 */
XHTTP_CLIENT_API xnetengine* xrtHttpClientEngine(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Engine : NULL;
}



/* 原子开始平滑排空并关闭全部空闲连接。 */
XHTTP_CLIENT_API bool xrtHttpClientDrain(xhttpclient* pClient)
{
	xhttpclientstate State;
	bool bAccepted;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"drain-http-client",
			"HTTP client is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	bAccepted = (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ||
		(State == XHTTP_CLIENT_CLOSED);
	if ( State == XHTTP_CLIENT_RUNNING ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_DRAINING,
			XMEMORY_RELEASE
		);
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	if ( !bAccepted ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtHttpClientCloseIdle(pClient);
	#endif
	__xrtHttpClientTryFinish(pClient);
	return true;
}



/* 构造失败时保留原始错误并回收已经建立的 Client 资源。 */
static xhttpclient* __xrtHttpClientCreateFail(xhttpclient* pClient)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		XHTTP_CLIENT_ERROR_CONFIG,
		XERR_STATE,
		"create-http-client",
		"HTTP client shared resources could not be initialized"
	);

	xrtHttpClientDestroy(pClient);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return NULL;
}



/* 统一创建拥有或借用 Resolver 的 Client。 */
static xhttpclient* __xrtHttpClientCreate(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig,
	bool bOwnResolver
)
{
	xhttpclientconfig Config;
	xhttpclient* pClient;

	xrtHttpClientConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"create-http-client",
				"HTTP client config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, pConfig, sizeof(Config));
	}
	if ( (pEngine == NULL) ||
		(!bOwnResolver && (pResolver == NULL)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"create-http-client",
			"HTTP client requires an engine and a valid resolver policy",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpClientConfigValid(&Config) ) {
		return NULL;
	}
	pClient = (xhttpclient*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->References = 1;
	pClient->Owners = 1;
	xrtAtomic64Init(
		&pClient->NextAffinity,
		Config.Dial.Affinity
	);
	xrtAtomic32Init(&pClient->State, XHTTP_CLIENT_RUNNING);
	if ( !xrtSpinInit(&pClient->LifecycleLock) ) {
		xrtFree(pClient);
		return NULL;
	}
	pClient->Engine = pEngine;
	pClient->Config = Config;
	pClient->OwnResolver = bOwnResolver;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheOpen(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( Config.Proxy != NULL ) {
			pClient->Proxy = xrtNetProxyRetain(
				Config.Proxy
			);
			if ( pClient->Proxy == NULL ) {
				return __xrtHttpClientCreateFail(pClient);
			}
			pClient->Config.Proxy = pClient->Proxy;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( Config.Cookies != NULL ) {
			pClient->Cookies = xrtCookieJarRetain(
				Config.Cookies
			);
			if ( pClient->Cookies == NULL ) {
				return __xrtHttpClientCreateFail(pClient);
			}
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !__xrtHttpPoolInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		if ( !__xrtHttpResumeInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	if ( !xrtNetEnginePin(pEngine) ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"create-http-client",
			"HTTP client requires a running network engine",
			NULL
		);
		return __xrtHttpClientCreateFail(pClient);
	}
	pClient->EngineHeld = true;
	if ( bOwnResolver ) {
		pClient->Resolver = xrtNetResolverCreate(
			&Config.Resolver
		);
		if ( pClient->Resolver == NULL ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	} else {
		pClient->Resolver = pResolver;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( !__xrtHttpClientTlsInit(pClient) ) {
			return __xrtHttpClientCreateFail(pClient);
		}
	#endif
	return pClient;
}



/* 创建拥有私有 Resolver 的 Client。 */
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreate(
	xnetengine* pEngine,
	const xhttpclientconfig* pConfig
)
{
	return __xrtHttpClientCreate(
		pEngine,
		NULL,
		pConfig,
		true
	);
}



/* 创建借用共享 Resolver 的 Client。 */
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreateWithResolver(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xhttpclientconfig* pConfig
)
{
	return __xrtHttpClientCreate(
		pEngine,
		pResolver,
		pConfig,
		false
	);
}



/* 增加 Call 引用。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpCallRef(xhttpcall* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRetain(&pCall->References) < 0) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"retain-http-call",
			"HTTP call is null or already released",
			NULL
		);
		return NULL;
	}
	return pCall;
}



/* 释放 Call 最后一个引用及尚未转移的资源。 */
XHTTP_CLIENT_API void xrtHttpCallDestroy(xhttpcall* pCall)
{
	if ( (pCall == NULL) ||
		(xrtRefRelease(&pCall->References) != 0) ) {
		return;
	}
	xrtNetDialDestroy(pCall->TcpDial);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyDialDestroy(pCall->ProxyDial);
		xrtNetProxyRelease(pCall->Proxy);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtTlsDialDestroy(pCall->TlsDial);
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xrtTlsStreamDestroy(pCall->ProxyTls);
		#endif
	#endif
	xrtHttp1CallDestroy(pCall->StreamCall);
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtCancelUnwatch(pCall->CancelWatch);
	xrtCancelDestroy(pCall->Cancel);
	xrtErrorFree(pCall->Error);
	xrtFree(pCall->Host);
	xrtHttpRequestDestroy(pCall->Request);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		__xrtHttpRedirectUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		__xrtHttpCookieUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		__xrtHttpDecompressReset(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		__xrtHttpClientCacheUnit(pCall);
	#endif
	(void)xrtSpinUnit(&pCall->Lock);
	__xrtHttpClientRelease(pCall->Client);
	memset(pCall, 0, sizeof(*pCall));
	xrtFree(pCall);
}



/* 回收尚未提交的 Call，并把下层同步错误提升为稳定 Client 错误。 */
static xhttpcall* __xrtHttpCallSubmitFail(
	xhttpcall* pCall,
	xhttpclienterror Code,
	xerrkind Fallback,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientPromoteOwned(
		xrtTakeError(),
		Code,
		Fallback,
		sOperation,
		sMessage
	);

	xrtHttpCallDestroy(pCall);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return NULL;
}



/* 取消仍在队列中的一个 Call Timer；回调负责释放其 Call 引用。 */
static void __xrtHttpCallCancelTimer(
	xhttpcall* pCall,
	xatomic64* pTimer,
	const xatomic32* pDone
)
{
	uint64 Id = xrtAtomic64Exchange(
		pTimer,
		0,
		XMEMORY_ACQ_REL
	);

	if ( (Id != 0) && !xrtAtomic32Load(
		pDone,
		XMEMORY_ACQUIRE
	) ) {
		if ( !xrtNetEngineTimerCancelCurrent(
			pCall->Client->Engine,
			Id
		) && !xrtNetEngineTimerCancel(
			pCall->Client->Engine,
			Id
		) ) {
			xrtClearError();
		}
	}
}



/* 同时撤销总截止时间和 idle deadline。 */
static void __xrtHttpCallCancelTimers(xhttpcall* pCall)
{
	__xrtHttpCallCancelTimer(
		pCall,
		&pCall->TotalTimer,
		&pCall->TotalTimerDone
	);
	__xrtHttpCallCancelTimer(
		pCall,
		&pCall->IdleTimer,
		&pCall->IdleTimerDone
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		__xrtHttpCallCancelTimer(
			pCall,
			&pCall->RetryTimer,
			&pCall->RetryTimerDone
		);
	#endif
}



/* 释放一个未被用户接管的成功结果。 */
static void __xrtHttpCallResultDestroy(
	xhttpresponse* pResponse,
	xnetstream* pTcp
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		, xtlsstream* pTls
	#endif
)
{
	xrtHttpResponseDestroy(pResponse);
	if ( pTcp != NULL ) {
		(void)xrtNetStreamAbort(pTcp);
		xrtNetStreamDestroy(pTcp);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			(void)xrtTlsStreamAbort(pTls);
			xrtTlsStreamDestroy(pTls);
		}
	#endif
}



/* 初始化 Call 的无锁诊断快照和绝对截止时间。 */
static void __xrtHttpCallInfoInit(
	xhttpcall* pCall,
	double iNow
)
{
	double iTotalDeadline = 0;
	double iIdleDeadline = 0;
	double iCandidate;

	if ( pCall->Timeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		iTotalDeadline = __xrtHttpCallDeadline(
			iNow,
			pCall->Timeout
		);
	}
	if ( pCall->IdleTimeout !=
		XHTTP_CLIENT_TIMEOUT_NONE ) {
		iCandidate = __xrtHttpCallDeadline(
			iNow,
			pCall->IdleTimeout
		);
		if ( (iTotalDeadline == 0) ||
			(iCandidate < iTotalDeadline) ) {
			iIdleDeadline = iCandidate;
		}
	}
	pCall->TotalDeadline = iTotalDeadline;
	xrtAtomic32Init(
		&pCall->Info.Phase,
		XHTTP_CALL_PHASE_QUEUED
	);
	xrtAtomic32Init(
		&pCall->Info.Result,
		(uint32)(int32)XNET_RESULT_AGAIN
	);
	xrtAtomic32Init(&pCall->Info.Error, 0);
	xrtAtomic32Init(&pCall->Info.Reused, 0);
	xrtAtomic32Init(&pCall->Info.Secure, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtAtomic32Init(
			&pCall->Info.Cache,
			XHTTP_CLIENT_CACHE_NONE
		);
	#endif
	xrtAtomic64Init(&pCall->Info.Submitted, (uint64)xrtNow());
	xrtAtomic64Init(&pCall->Info.Started, 0);
	xrtAtomic64Init(&pCall->Info.TransportReady, 0);
	xrtAtomic64Init(&pCall->Info.RequestSent, 0);
	xrtAtomic64Init(&pCall->Info.FirstByte, 0);
	xrtAtomic64Init(&pCall->Info.Headers, 0);
	xrtAtomic64Init(&pCall->Info.LastProgress, (uint64)xrtNow());
	xrtAtomic64Init(&pCall->Info.Completed, 0);
	xrtAtomic64Init(&pCall->Info.RequestWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseWireBytes, 0);
	xrtAtomic64Init(&pCall->Info.ResponseBodyBytes, 0);
	xrtAtomic64Init(&pCall->Info.Redirects, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic64Init(&pCall->Info.Retries, 0);
	#endif
	xrtAtomic64Init(&pCall->IdleDeadline, __xrtWaitTimerBits(iIdleDeadline));
}



/* 把内部原子计数复制为公开值快照。 */
static void __xrtHttpCallInfoCopy(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
)
{
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	pInfo->Phase = (xhttpcallphase)xrtAtomic32Load(
		&pCall->Info.Phase,
		XMEMORY_ACQUIRE
	);
	pInfo->Result = (xnetresult)(int32)xrtAtomic32Load(
		&pCall->Info.Result,
		XMEMORY_ACQUIRE
	);
	pInfo->Error = (xhttpclienterror)xrtAtomic32Load(
		&pCall->Info.Error,
		XMEMORY_ACQUIRE
	);
	pInfo->Submitted = xrtAtomic64Load(
		&pCall->Info.Submitted,
		XMEMORY_ACQUIRE
	);
	pInfo->Started = xrtAtomic64Load(
		&pCall->Info.Started,
		XMEMORY_ACQUIRE
	);
	pInfo->TransportReady = xrtAtomic64Load(
		&pCall->Info.TransportReady,
		XMEMORY_ACQUIRE
	);
	pInfo->RequestSent = xrtAtomic64Load(
		&pCall->Info.RequestSent,
		XMEMORY_ACQUIRE
	);
	pInfo->FirstByte = xrtAtomic64Load(
		&pCall->Info.FirstByte,
		XMEMORY_ACQUIRE
	);
	pInfo->Headers = xrtAtomic64Load(
		&pCall->Info.Headers,
		XMEMORY_ACQUIRE
	);
	pInfo->LastProgress = xrtAtomic64Load(
		&pCall->Info.LastProgress,
		XMEMORY_ACQUIRE
	);
	pInfo->Completed = xrtAtomic64Load(
		&pCall->Info.Completed,
		XMEMORY_ACQUIRE
	);
	pInfo->RequestWireBytes = xrtAtomic64Load(
		&pCall->Info.RequestWireBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->ResponseWireBytes = xrtAtomic64Load(
		&pCall->Info.ResponseWireBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->ResponseBodyBytes = xrtAtomic64Load(
		&pCall->Info.ResponseBodyBytes,
		XMEMORY_ACQUIRE
	);
	pInfo->Redirects = (size_t)xrtAtomic64Load(
		&pCall->Info.Redirects,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		pInfo->Retries = (size_t)xrtAtomic64Load(
			&pCall->Info.Retries,
			XMEMORY_ACQUIRE
		);
	#endif
	pInfo->ReusedConnection = xrtAtomic32Load(
		&pCall->Info.Reused,
		XMEMORY_ACQUIRE
	) != 0;
	pInfo->Secure = xrtAtomic32Load(
		&pCall->Info.Secure,
		XMEMORY_ACQUIRE
	) != 0;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pInfo->Cache =
			(xhttpclientcacheoutcome)xrtAtomic32Load(
				&pCall->Info.Cache,
				XMEMORY_ACQUIRE
			);
	#endif
}



/* 发布一次不可变终态并转移响应或升级传输。 */
static void __xrtHttpCallFinish(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded,
	xerror* pError
)
{
	xhttpcallresult CallResult;
	xhttp1exchange* pExchange;
	xcancelwatch* pWatch;
	uint32 iTimeout;
	xhttpcallstate State;
	xhttpclienterror TerminalCode;
	uint64 iDeliveredBody;
	uint64 iStoredBody;
	bool bCancelled;

	/*
		完成、显式取消和超时共用这条线性化边界。
		锁内只提交门状态和摘取所有权，不运行回调或释放资源。
	*/
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		__xrtHttpCallResultDestroy(
			pResponse,
			pTcp
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				, pTls
			#endif
		);
		xrtErrorFree(pError);
		return;
	}
	bCancelled = xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) != 0;
	iTimeout = xrtAtomic32Load(
		&pCall->TimeoutCause,
		XMEMORY_RELAXED
	);
	xrtAtomic32Store(
		&pCall->FinishGate,
		1,
		XMEMORY_RELEASE
	);
	pExchange = pCall->Exchange;
	pCall->Exchange = NULL;
	pWatch = pCall->CancelWatch;
	pCall->CancelWatch = NULL;
	(void)xrtSpinUnlock(&pCall->Lock);

	if ( bCancelled ) {
		__xrtHttpCallResultDestroy(
			pResponse,
			pTcp
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				, pTls
			#endif
		);
		pResponse = NULL;
		pTcp = NULL;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls = NULL;
		#endif
		iBuffered = 0;
		bUpgraded = false;
		if ( iTimeout == XRT_HTTP_TIMEOUT_IDLE ) {
			TerminalCode =
				XHTTP_CLIENT_ERROR_TIMEOUT_IDLE;
		} else if ( iTimeout ==
			XRT_HTTP_TIMEOUT_TOTAL ) {
			TerminalCode =
				XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL;
		} else {
			TerminalCode =
				XHTTP_CLIENT_ERROR_CANCELLED;
		}
		pError = __xrtHttpClientTerminalError(
			pError,
			TerminalCode
		);
		if ( iTimeout != XRT_HTTP_TIMEOUT_NONE ) {
			Result = XNET_RESULT_TIMEOUT;
		} else {
			Result = XNET_RESULT_CANCELLED;
		}
		Code = TerminalCode;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolFinish(pCall);
		(void)xrtAtomic64FetchAdd(
			&pCall->Client->RequestsCompleted,
			1,
			XMEMORY_RELAXED
		);
	#endif
	__xrtHttpCallCancelTimers(pCall);
	pCall->Error = pError;
	xrtCancelUnwatch(pWatch);
	xrtHttp1ExchangeDestroy(pExchange);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		/*
			重定向策略在终态前已经不再需要下一跳草案，及时释放它，
			避免用户长期保留已完成 Call 时继续占用请求快照。
		*/
		__xrtHttpRedirectUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		/*
			Cookie 已完成最后一次逐跳选择和响应存储，终态不再需要
			调用级分区键，避免保留 Call 时继续持有它的副本。
		*/
		__xrtHttpCookieUnit(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		/*
			缓存写入和重放在终态前已经完成，及时释放正文与字段快照，
			避免用户长期保留已完成 Call 时继续占用临时内存。
		*/
		__xrtHttpClientCacheUnit(pCall);
	#endif

	if ( Result == XNET_RESULT_OK ) {
		State = XHTTP_CALL_SUCCEEDED;
	} else if ( Result == XNET_RESULT_CANCELLED ) {
		State = XHTTP_CALL_CANCELLED;
	} else {
		State = XHTTP_CALL_FAILED;
	}
	xrtAtomic32Store(
		&pCall->Info.Result,
		(uint32)(int32)Result,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->Info.Error,
		(uint32)Code,
		XMEMORY_RELEASE
	);
	xrtAtomic64Store(
		&pCall->Info.Completed,
		(uint64)xrtNow(),
		XMEMORY_RELEASE
	);
	iDeliveredBody = xrtAtomic64Load(
		&pCall->Info.ResponseBodyBytes,
		XMEMORY_ACQUIRE
	);
	iStoredBody = pResponse != NULL ?
		xrtHttpResponseBodyBytes(pResponse) : 0;
	if ( iStoredBody > iDeliveredBody ) {
		xrtAtomic64Store(
			&pCall->Info.ResponseBodyBytes,
			iStoredBody,
			XMEMORY_RELEASE
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtAtomic64Store(
			&pCall->Info.Redirects,
			(uint64)pCall->Redirects,
			XMEMORY_RELEASE
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic64Store(
			&pCall->Info.Retries,
			(uint64)pCall->Retries,
			XMEMORY_RELEASE
		);
	#endif
	xrtAtomic32Store(
		&pCall->State,
		(uint32)State,
		XMEMORY_RELEASE
	);
	memset(&CallResult, 0, sizeof(CallResult));
	CallResult.Result = Result;
	CallResult.Response = pResponse;
	CallResult.Tcp = pTcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		CallResult.Tls = pTls;
	#endif
	CallResult.Error = pCall->Error;
	__xrtHttpCallInfoCopy(
		pCall,
		&CallResult.Info
	);
	CallResult.Buffered = iBuffered;
	CallResult.Upgraded = bUpgraded;
	pCall->Done(pCall, &CallResult, pCall->Data);
	__xrtHttpClientCallDetach(pCall);
	if ( pCall->RuntimeHeld ) {
		pCall->RuntimeHeld = false;
		xrtHttpCallDestroy(pCall);
	}
}



/* 发布带响应和可选升级传输的成功终态。 */
void __xrtHttpCallSucceed(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xnetstream* pTcp,
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls,
	#endif
	size_t iBuffered,
	bool bUpgraded
)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtHttpBridgeResponseSetRedirects(
			pResponse,
			pCall->Redirects
		);
	#endif
	if ( pCall->Events.Body == NULL ) {
		xrtHttpBridgeResponseSetBuffered(pResponse);
	}
	__xrtHttpCallFinish(
		pCall,
		XNET_RESULT_OK,
		XHTTP_CLIENT_ERROR_NONE,
		pResponse,
		pTcp,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls,
		#endif
		iBuffered,
		bUpgraded,
		NULL
	);
}



/* 发布一个没有响应或升级传输的失败终态。 */
void __xrtHttpCallFail(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError;
	uint32 iTimeout = xrtAtomic32Load(
		&pCall->TimeoutCause,
		XMEMORY_ACQUIRE
	);

	if ( iTimeout != XRT_HTTP_TIMEOUT_NONE ) {
		Result = XNET_RESULT_TIMEOUT;
		Code = iTimeout == XRT_HTTP_TIMEOUT_IDLE ?
			XHTTP_CLIENT_ERROR_TIMEOUT_IDLE :
			XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL;
		Kind = XERR_TIMEOUT;
		sOperation = "run-http-call";
		sMessage = iTimeout == XRT_HTTP_TIMEOUT_IDLE ?
			"HTTP call made no progress before its idle deadline" :
			"HTTP call exceeded its total deadline";
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( (iTimeout == XRT_HTTP_TIMEOUT_NONE) &&
			__xrtHttpRetryFailure(
				pCall,
				Result,
				Code,
				Kind,
				pCause
			) ) {
			return;
		}
	#endif
	pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	__xrtHttpCallFinish(
		pCall,
		Result,
		Code,
		NULL,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false,
		pError
	);
}



/* 任一截止时间到达后复用统一取消路径。 */
static bool __xrtHttpCallTimeout(
	xhttpcall* pCall,
	uint32 iCause
);



/* 总截止时间到达后复用统一取消路径。 */
static void __xrtHttpCallTotalTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->TotalTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->TotalTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		(void)__xrtHttpCallTimeout(
			pCall,
			XRT_HTTP_TIMEOUT_TOTAL
		);
	} else if ( (Result == XNET_RESULT_ERROR) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"run-http-call-timeout",
			"HTTP total timeout timer failed",
			NULL
		);
	}
	xrtHttpCallDestroy(pCall);
}



/* 为现有 idle Timer 引用重新安排下一次 deadline。 */
static bool __xrtHttpCallIdleTimerAgain(
	xhttpcall* pCall,
	double iDeadline
);



/*
	Idle Timer 只在 deadline 到达时检查一次。
	期间任意真实 I/O 仅更新原子 deadline，不产生 Timer 或命令。
*/
static void __xrtHttpCallIdleTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	double iDeadline;
	double iNow;
	uint32 iCause;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->IdleTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->IdleTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Result == XNET_RESULT_OK) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		iDeadline = __xrtWaitTimerFromBits(xrtAtomic64Load(
			&pCall->IdleDeadline,
			XMEMORY_ACQUIRE
		));
		iNow = xrtTimer();
		if ( iDeadline > iNow ) {
			if ( __xrtHttpCallIdleTimerAgain(
				pCall,
				iDeadline
			) ) {
				return;
			}
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"schedule-http-idle-timeout",
				"HTTP idle timeout timer could not be rescheduled",
				xrtGetError()
			);
		} else {
			iCause =
				(pCall->Timeout !=
				 XHTTP_CLIENT_TIMEOUT_NONE) &&
				(pCall->TotalDeadline <= iNow) ?
				XRT_HTTP_TIMEOUT_TOTAL :
				XRT_HTTP_TIMEOUT_IDLE;
			(void)__xrtHttpCallTimeout(pCall, iCause);
		}
	} else if ( (Result == XNET_RESULT_ERROR) &&
		!xrtAtomic32Load(
			&pCall->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"run-http-idle-timeout",
			"HTTP idle timeout timer failed",
			NULL
		);
	}
	xrtHttpCallDestroy(pCall);
}



/* 把当前 Timer 持有的 Call 引用转移给下一次 idle deadline。 */
static bool __xrtHttpCallIdleTimerAgain(
	xhttpcall* pCall,
	double iDeadline
)
{
	uint64 Id;

	xrtAtomic32Store(
		&pCall->IdleTimerDone,
		0,
		XMEMORY_RELEASE
	);
	Id = __xrtNetEngineSchedule(
		pCall->Client->Engine,
		pCall->Affinity,
		iDeadline,
		__xrtHttpCallIdleTimer,
		pCall
	);
	if ( Id == 0 ) {
		xrtAtomic32Store(
			&pCall->IdleTimerDone,
			1,
			XMEMORY_RELEASE
		);
		return false;
	}
	xrtAtomic64Store(
		&pCall->IdleTimer,
		Id,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->IdleTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64Exchange(
			&pCall->IdleTimer,
			0,
			XMEMORY_ACQ_REL
		);
	}
	return true;
}



/* 外部取消令牌只请求 Call 取消，不直接接触阶段对象。 */
static void __xrtHttpCallCancelled(ptr pData)
{
	(void)xrtHttpCallCancel((xhttpcall*)pData);
}



/* 在连接池选定的 Worker 上复用传输或启动新拨号。 */
static void __xrtHttpCallStartTransport(xhttpcall* pCall)
{
	xerror* pCause;
	bool bStarted;

	/*
		Dial 返回值和完成回调的交接依赖同 Worker 不重入契约。
		禁止未来从提交线程或错误的 Worker 直接进入本函数。
	*/
	if ( (pCall->Worker == NULL) ||
		!xrtNetWorkerIsCurrent(pCall->Worker) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_STATE,
			"start-http-transport",
			"HTTP transport must start on the call worker",
			NULL
		);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"start-http-call",
			"HTTP call was cancelled before dialing",
			NULL
		);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( pCall->PooledTcp != NULL ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_REQUEST
			);
			bStarted =
				__xrtHttpCallStartPooledTcp(pCall);
		} else if (
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				pCall->PooledTls != NULL
			#else
				false
			#endif
		) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				__xrtHttpCallSetPhase(
					pCall,
					XHTTP_CALL_PHASE_REQUEST
				);
				bStarted =
					__xrtHttpCallStartPooledTls(
						pCall
					);
			#else
				bStarted = false;
			#endif
		} else
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->Proxy != NULL ) {
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_PROXY
			);
			bStarted = __xrtHttpCallStartProxy(pCall);
		} else
	#endif
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			__xrtHttpCallSetPhase(
				pCall,
				XHTTP_CALL_PHASE_TLS
			);
			bStarted = __xrtHttpCallStartTls(pCall);
		#else
			bStarted = false;
			__xrtHttpClientSetError(
				XERR_UNSUPPORTED,
				XHTTP_CLIENT_ERROR_TLS,
				"start-http-call",
				"HTTPS support is not present in this build",
				NULL
			);
		#endif
	} else {
		__xrtHttpCallSetPhase(
			pCall,
			XHTTP_CALL_PHASE_CONNECT
		);
		bStarted = __xrtHttpCallStartTcp(pCall);
	}
	if ( !bStarted ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					XHTTP_CLIENT_ERROR_PROXY :
			#endif
			(pCall->Secure ?
				XHTTP_CLIENT_ERROR_TLS :
				XHTTP_CLIENT_ERROR_DIAL),
			__xrtHttpClientCauseKind(
				pCause,
				XERR_IO
			),
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					"dial-http-proxy" :
			#endif
			(pCall->Secure ?
				"dial-https" :
				"dial-http"),
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				pCall->Proxy != NULL ?
					"HTTP proxy transport could not start" :
			#endif
			(pCall->Secure ?
				"HTTPS transport could not start" :
				"HTTP transport could not start"),
			pCause
		);
		xrtErrorFree(pCause);
	}
}



/* 获取当前 origin 的连接配额，然后启动或等待这一跳传输。 */
void __xrtHttpCallStartHop(xhttpcall* pCall)
{
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xerror* pCacheCause;
		bool bCacheHandled;

		if ( !__xrtHttpClientCacheStart(
			pCall,
			&bCacheHandled
		) ) {
			pCacheCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_CACHE,
				__xrtHttpClientCauseKind(
					pCacheCause,
					XERR_MEMORY
				),
				"deliver-http-cache-response",
				"HTTP cached response could not be rebuilt",
				pCacheCause
			);
			xrtErrorFree(pCacheCause);
			return;
		}
		if ( bCacheHandled ) {
			return;
		}
	#endif
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_POOL
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		xerror* pCause;
		bool bReady;

		if ( !__xrtHttpPoolAcquire(
			pCall,
			&bReady
		) ) {
			pCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_POOL,
				(pCause != NULL) &&
				(xrtErrorIs(
					pCause,
					XERR_MEMORY
				) != NULL) ?
					XERR_MEMORY : XERR_AGAIN,
				"acquire-http-connection",
				"HTTP connection pool could not schedule the call",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		if ( !bReady ) {
			return;
		}
	#endif
	__xrtHttpCallStartTransport(pCall);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 连接池唤醒后在新的 Stream Worker 上继续执行 Call。 */
void __xrtHttpCallPoolReady(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)pWorker;
	__xrtHttpCallStartTransport(pCall);
}

#endif



/* 在目标 Worker 上装配取消、总 Timer 并进入池或实际拨号。 */
static void __xrtHttpCallStart(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xerror* pCause;
	double iNow;
	uint64 Id;

	(void)pWorker;
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"start-http-call",
			"HTTP call was cancelled before dialing",
			NULL
		);
		return;
	}
	if ( pCall->Cancel != NULL ) {
		pCall->CancelWatch = xrtCancelWatch(
			pCall->Cancel,
			__xrtHttpCallCancelled,
			pCall
		);
		if ( pCall->CancelWatch == NULL ) {
			pCause = xrtTakeError();
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"watch-http-call-cancel",
				"HTTP call could not observe its cancellation token",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
	}
	iNow = xrtTimer();
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.Started,
		(uint64)xrtNow()
	);
	__xrtHttpCallProgressAt(pCall, iNow);
	if ( pCall->Timeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		if ( xrtHttpCallRef(pCall) == NULL ) {
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_STATE,
				"schedule-http-call-timeout",
				"HTTP call timer could not retain the call",
				xrtGetError()
			);
			return;
		}
		xrtAtomic32Store(
			&pCall->TotalTimerDone,
			0,
			XMEMORY_RELEASE
		);
		Id = __xrtNetEngineSchedule(
			pCall->Client->Engine,
			pCall->Affinity,
			pCall->TotalDeadline,
			__xrtHttpCallTotalTimer,
			pCall
		);
		if ( Id == 0 ) {
			pCause = xrtTakeError();
			xrtHttpCallDestroy(pCall);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"schedule-http-call-timeout",
				"HTTP call total deadline could not be scheduled",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		xrtAtomic64Store(
			&pCall->TotalTimer,
			Id,
			XMEMORY_RELEASE
		);
		if ( xrtAtomic32Load(
			&pCall->TotalTimerDone,
			XMEMORY_ACQUIRE
		) ) {
			(void)xrtAtomic64Exchange(
				&pCall->TotalTimer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
	if ( (pCall->IdleTimeout !=
		XHTTP_CLIENT_TIMEOUT_NONE) &&
		((pCall->Timeout ==
		  XHTTP_CLIENT_TIMEOUT_NONE) ||
		 (__xrtWaitTimerFromBits(xrtAtomic64Load(
			&pCall->IdleDeadline,
			XMEMORY_ACQUIRE
		  )) < pCall->TotalDeadline)) ) {
		if ( xrtHttpCallRef(pCall) == NULL ) {
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_STATE,
				"schedule-http-idle-timeout",
				"HTTP idle timer could not retain the call",
				xrtGetError()
			);
			return;
		}
		xrtAtomic32Store(
			&pCall->IdleTimerDone,
			0,
			XMEMORY_RELEASE
		);
		Id = __xrtNetEngineSchedule(
			pCall->Client->Engine,
			pCall->Affinity,
			__xrtWaitTimerFromBits(xrtAtomic64Load(
				&pCall->IdleDeadline,
				XMEMORY_ACQUIRE
			)),
			__xrtHttpCallIdleTimer,
			pCall
		);
		if ( Id == 0 ) {
			pCause = xrtTakeError();
			xrtHttpCallDestroy(pCall);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"schedule-http-idle-timeout",
				"HTTP call idle deadline could not be scheduled",
				pCause
			);
			xrtErrorFree(pCause);
			return;
		}
		xrtAtomic64Store(
			&pCall->IdleTimer,
			Id,
			XMEMORY_RELEASE
		);
		if ( xrtAtomic32Load(
			&pCall->IdleTimerDone,
			XMEMORY_ACQUIRE
		) ) {
			(void)xrtAtomic64Exchange(
				&pCall->IdleTimer,
				0,
				XMEMORY_ACQ_REL
			);
		}
	}
	__xrtHttpCallStartHop(pCall);
}



/* 复制一个计划拥有的 Host 视图并追加 C 字符串终止符。 */
static str __xrtHttpCallHost(xstrview Host)
{
	str sHost;

	if ( (Host.Data == NULL) || (Host.Size == 0) ||
		(Host.Size == SIZE_MAX) ) {
		__xrtHttpClientSetError(
			XERR_VALUE,
			XHTTP_CLIENT_ERROR_REQUEST,
			"prepare-http-call",
			"HTTP request plan has no usable host",
			NULL
		);
		return NULL;
	}
	sHost = (str)xrtMalloc(Host.Size + 1u);
	if ( sHost == NULL ) {
		return NULL;
	}
	memcpy(sHost, Host.Data, Host.Size);
	sHost[Host.Size] = 0;
	return sHost;
}



/* 记录信息响应到达，并保持原事件链的返回语义。 */
static bool __xrtHttpCallInfoInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)__xrtHttpCallFirstTime(
		&pCall->Info.FirstByte,
		xrtTimer()
	);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_HEADERS
	);
	if ( pCall->Events.Informational == NULL ) {
		return true;
	}
	return pCall->Events.Informational(
		pCall,
		pResponse,
		pCall->Events.Data
	);
}



/* 记录首个最终响应 Header，并保持原事件链的返回语义。 */
static bool __xrtHttpCallInfoHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	(void)__xrtHttpCallFirstTime(
		&pCall->Info.FirstByte,
		(uint64)xrtNow()
	);
	(void)__xrtHttpCallFirstTime(
		&pCall->Info.Headers,
		(uint64)xrtNow()
	);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_HEADERS
	);
	if ( pCall->Events.Headers == NULL ) {
		return true;
	}
	return pCall->Events.Headers(
		pCall,
		pResponse,
		pCall->Events.Data
	);
}



/* 记录已经接受的响应正文，并保留缓冲或流式交付策略。 */
static bool __xrtHttpCallInfoBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	bool bAccepted;

	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_RESPONSE_BODY
	);
	if ( pCall->Events.Body == NULL ) {
		bAccepted = xrtHttpBridgeResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	} else {
		bAccepted = pCall->Events.Body(
			pCall,
			pResponse,
			Data,
			pCall->Events.Data
		);
	}
	if ( bAccepted ) {
		(void)xrtAtomic64FetchAdd(
			&pCall->Info.ResponseBodyBytes,
			(uint64)Data.Size,
			XMEMORY_RELAXED
		);
	}
	return bAccepted;
}



/* 把诊断观察器放在用户事件外层，统计用户最终看到的响应字节。 */
static const xhttp1exchangeevents* __xrtHttpCallInfoEvents(
	xhttpcall* pCall
)
{
	memset(&pCall->InfoEvents, 0, sizeof(pCall->InfoEvents));
	pCall->InfoEvents.Informational =
		__xrtHttpCallInfoInformational;
	pCall->InfoEvents.Headers = __xrtHttpCallInfoHeaders;
	pCall->InfoEvents.Body = __xrtHttpCallInfoBody;
	pCall->InfoEvents.Data = pCall;
	return &pCall->InfoEvents;
}



/* 失败原子地建立一跳请求所需的 Plan、Exchange 和传输端点。 */
bool __xrtHttpCallPrepareHop(xhttpcall* pCall)
{
	xhttp1requestplan* pPlan;
	xhttp1exchange* pExchange;
	xhttp1exchangeconfig ExchangeConfig;
	const xhttp1exchangeevents* pEvents;
	xstrview Host;
	str sHost;
	uint16 iPort;
	bool bSecure;

	if ( (pCall == NULL) || (pCall->Request == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"prepare-http-call",
			"HTTP call and request are required",
			NULL
		);
		return false;
	}
	xrtAtomic64Store(
		&pCall->Info.ResponseBodyBytes,
		0,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		__xrtHttpDecompressReset(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( !__xrtHttpCookiePrepare(pCall) ) {
			return false;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCachePrepare(pCall) ) {
			return false;
		}
	#endif
	pEvents = __xrtHttpCallInfoEvents(pCall);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		pEvents = __xrtHttpDecompressEvents(
			pCall,
			pEvents
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		pEvents = __xrtHttpRedirectEvents(
			pCall,
			pEvents
		);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pEvents = __xrtHttpClientCacheEvents(
			pCall,
			pEvents
		);
		if ( pCall->CacheReady ) {
			xrtHttp1ExchangeDestroy(pCall->Exchange);
			xrtFree(pCall->Host);
			pCall->Exchange = NULL;
			pCall->Host = NULL;
			pCall->Port = 0;
			return true;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		pEvents = __xrtHttpRetryEvents(pCall, pEvents);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		pEvents = __xrtHttpCookieEvents(pCall, pEvents);
	#endif
	pPlan = xrtHttp1RequestPrepare(
		pCall->Request,
		&pCall->RequestOptions
	);
	if ( pPlan == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"prepare-http-request",
			"HTTP request could not be prepared for HTTP/1"
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	Host = xrtHttp1RequestPlanHost(pPlan);
	sHost = __xrtHttpCallHost(Host);
	if ( sHost == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"prepare-http-request",
			"HTTP request host could not be retained"
		);

		xrtHttp1RequestPlanDestroy(pPlan);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	iPort = xrtHttp1RequestPlanPort(pPlan);
	bSecure = xrtHttp1RequestPlanSecure(pPlan);
	#if !defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( bSecure ) {
			xrtFree(sHost);
			xrtHttp1RequestPlanDestroy(pPlan);
			__xrtHttpClientSetError(
				XERR_UNSUPPORTED,
				XHTTP_CLIENT_ERROR_TLS,
				"prepare-http-call",
				"HTTPS support is not present in this build",
				NULL
			);
			return false;
		}
	#endif
	ExchangeConfig = pCall->Client->Config.Exchange;
	ExchangeConfig.Body.MaxBody = pCall->ResponseBodyLimit;
	pExchange = xrtHttp1ExchangeCreate(
		pPlan,
		&ExchangeConfig,
		pEvents
	);
	if ( pExchange == NULL ) {
		xerror* pError = __xrtHttpClientPromoteOwned(
			xrtTakeError(),
			XHTTP_CLIENT_ERROR_RESPONSE,
			XERR_MEMORY,
			"prepare-http-response",
			"HTTP response exchange could not be initialized"
		);

		xrtFree(sHost);
		xrtHttp1RequestPlanDestroy(pPlan);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return false;
	}
	xrtHttp1ExchangeDestroy(pCall->Exchange);
	xrtFree(pCall->Host);
	pCall->Exchange = pExchange;
	pCall->Host = sHost;
	pCall->Port = iPort;
	pCall->Secure = bSecure;
	xrtAtomic32Store(
		&pCall->Info.Secure,
		bSecure ? 1u : 0u,
		XMEMORY_RELEASE
	);
	return true;
}



XHTTP_CLIENT_API xhttpcall* xrtHttpClientDo(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	xhttpcalloptions Options;
	xhttpclient* pClientRef;
	xhttpcall* pCall;
	uint64 iAffinity;
	double iNow;
	uint32 iWorkers;

	if ( (pClient == NULL) || (pRequest == NULL) ||
		(pDone == NULL) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"run-http-client",
			"HTTP client, request and completion callback are required",
			NULL
		);
		return NULL;
	}
	if ( xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) != XHTTP_CLIENT_RUNNING ) {
		__xrtHttpClientSetError(
			XERR_CLOSED,
			XHTTP_CLIENT_ERROR_STATE,
			"run-http-client",
			"HTTP client is not accepting new calls",
			NULL
		);
		return NULL;
	}
	xrtHttpCallOptionsInit(&Options);
	if ( pOptions != NULL ) {
		if ( !xrtMemRangeValid(pOptions, sizeof(*pOptions)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"run-http-client",
				"HTTP call options range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Options, pOptions, sizeof(Options));
	}
    if (Options.Timeout < XRT_WAIT_FOREVER || Options.IdleTimeout < XRT_WAIT_FOREVER) {
        __xrtHttpClientSetError(XERR_ARGUMENT, XHTTP_CLIENT_ERROR_ARGUMENT,
            "run-http-client", "invalid millisecond timeout", NULL);
        return NULL;
    }
	pClientRef = __xrtHttpClientHold(pClient);
	if ( pClientRef == NULL ) {
		return NULL;
	}
	pCall = (xhttpcall*)xrtCalloc(1, sizeof(*pCall));
	if ( pCall == NULL ) {
		__xrtHttpClientRelease(pClientRef);
		return NULL;
	}
	pCall->References = 1;
	pCall->Client = pClientRef;
	pCall->Timeout = Options.Timeout == 0 ?
		pClient->Config.Timeout : Options.Timeout;
	pCall->IdleTimeout = Options.IdleTimeout == 0 ?
		pClient->Config.IdleTimeout : Options.IdleTimeout;
	pCall->ResponseBodyLimit = Options.ResponseBodyLimit == 0 ?
		pClient->Config.Exchange.Body.MaxBody :
		Options.ResponseBodyLimit;
	xrtAtomic32Init(&pCall->State, XHTTP_CALL_QUEUED);
	xrtAtomic32Init(&pCall->CancelGate, 0);
	xrtAtomic32Init(&pCall->FinishGate, 0);
	xrtAtomic32Init(
		&pCall->TimeoutCause,
		XRT_HTTP_TIMEOUT_NONE
	);
	xrtAtomic32Init(&pCall->TotalTimerDone, 0);
	xrtAtomic32Init(&pCall->IdleTimerDone, 0);
	xrtAtomic64Init(&pCall->TotalTimer, 0);
	xrtAtomic64Init(&pCall->IdleTimer, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		xrtAtomic32Init(&pCall->RetryTimerDone, 0);
		xrtAtomic64Init(&pCall->RetryTimer, 0);
	#endif
	iNow = xrtTimer();
	__xrtHttpCallInfoInit(pCall, iNow);
	if ( !xrtSpinInit(&pCall->Lock) ) {
		__xrtHttpClientRelease(pCall->Client);
		xrtFree(pCall);
		return NULL;
	}
	if ( !xrtNetPostInit(&pCall->StartPost) ) {
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"initialize-http-call-post",
			"HTTP call could not initialize its Worker post"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		if ( !xrtNetPostInit(&pCall->PoolPost) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"initialize-http-pool-post",
				"HTTP call could not initialize its pool post"
			);
		}
	#endif
	pCall->Done = pDone;
	pCall->Data = pData;
	pCall->Request = xrtHttpRequestClone(pRequest);
	if ( pCall->Request == NULL ) {
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"freeze-http-request",
			"HTTP request could not be frozen for asynchronous execution"
		);
	}
	pCall->RequestOptions = Options.Request;
	pCall->Events = Options.Events;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( !__xrtHttpRetryInit(pCall, &Options) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_RETRY,
				XERR_ARGUMENT,
				"configure-http-retry",
				"HTTP retry policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_DECOMPRESSION,
				XERR_ARGUMENT,
				"configure-http-decompression",
				"HTTP response decompression policy is invalid"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_CACHE,
				XERR_ARGUMENT,
				"configure-http-cache",
				"HTTP cache policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( !__xrtHttpProxyInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_PROXY,
				XERR_ARGUMENT,
				"configure-http-proxy",
				"HTTP proxy policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( !__xrtHttpRedirectInit(
			pCall,
			&Options
		) ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_REDIRECT,
				XERR_ARGUMENT,
				"configure-http-redirect",
				"HTTP redirect policy could not be frozen"
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		if ( !__xrtHttpCookieInit(pCall, &Options) ) {
			__xrtHttpCookieSetSubmitError(pCall);
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_COOKIE,
				XERR_ARGUMENT,
				"configure-http-cookie",
				"HTTP Cookie policy could not be frozen"
			);
		}
	#endif
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			__xrtHttpCookieSetSubmitError(pCall);
		#endif
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"prepare-http-call",
			"HTTP call could not prepare its initial request"
		);
	}
	if ( Options.Cancel != NULL ) {
		pCall->Cancel = xrtCancelRef(Options.Cancel);
		if ( pCall->Cancel == NULL ) {
			return __xrtHttpCallSubmitFail(
				pCall,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				XERR_ARGUMENT,
				"retain-http-cancel",
				"HTTP call cancellation token could not be retained"
			);
		}
		if ( xrtCancelRequested(pCall->Cancel) ) {
			xrtAtomic32Store(
				&pCall->CancelGate,
				1,
				XMEMORY_RELEASE
			);
		}
	}
	iWorkers = xrtNetEngineWorkerCount(pClient->Engine);
	if ( iWorkers == 0 ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"run-http-client",
			"HTTP client engine has no active worker",
			NULL
		);
		xrtHttpCallDestroy(pCall);
		return NULL;
	}
	iAffinity = xrtAtomic64FetchAdd(
		&pClient->NextAffinity,
		1,
		XMEMORY_ACQ_REL
	);
	pCall->Affinity = iAffinity;
	pCall->Worker = xrtNetEngineWorker(
		pClient->Engine,
		(uint32)(iAffinity % iWorkers)
	);
	pCall->References = 2;
	pCall->RuntimeHeld = true;
	if ( !__xrtHttpClientCallAttach(pCall) ) {
		pCall->RuntimeHeld = false;
		pCall->References = 1;
		xrtHttpCallDestroy(pCall);
		return NULL;
	}
	if ( !xrtNetPost(
		pCall->Worker,
		&pCall->StartPost,
		__xrtHttpCallStart,
		pCall
	) ) {
		__xrtHttpClientCallDetach(pCall);
		pCall->RuntimeHeld = false;
		xrtHttpCallDestroy(pCall);
		return __xrtHttpCallSubmitFail(
			pCall,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_CLOSED,
			"post-http-call",
			"HTTP call could not enter its network Worker"
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtAtomic64FetchAdd(
			&pClient->RequestsStarted,
			1,
			XMEMORY_RELAXED
		);
	#endif
	return pCall;
}



/* 把已经取得 CancelGate 的取消请求转发给当前有效阶段。 */
static void __xrtHttpCallCancelActive(xhttpcall* pCall)
{
	xnetdial* pTcpDial = NULL;
	xhttp1call* pStreamCall = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxydial* pProxyDial = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsdial* pTlsDial = NULL;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xtlsstream* pProxyTls = NULL;
		#endif
	#endif

	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TcpDial != NULL ) {
		pTcpDial = xrtNetDialRef(pCall->TcpDial);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->ProxyDial != NULL ) {
			pProxyDial = xrtNetProxyDialRef(
				pCall->ProxyDial
			);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pCall->TlsDial != NULL ) {
			pTlsDial = xrtTlsDialRef(pCall->TlsDial);
		}
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			if ( pCall->ProxyTls != NULL ) {
				pProxyTls = xrtTlsStreamRef(
					pCall->ProxyTls
				);
			}
		#endif
	#endif
	if ( pCall->StreamCall != NULL ) {
		pStreamCall = xrtHttp1CallRef(pCall->StreamCall);
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( pTcpDial != NULL ) {
		(void)xrtNetDialCancel(pTcpDial);
		xrtNetDialDestroy(pTcpDial);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pProxyDial != NULL ) {
			(void)xrtNetProxyDialCancel(pProxyDial);
			xrtNetProxyDialDestroy(pProxyDial);
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTlsDial != NULL ) {
			(void)xrtTlsDialCancel(pTlsDial);
			xrtTlsDialDestroy(pTlsDial);
		}
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			if ( pProxyTls != NULL ) {
				(void)xrtTlsStreamAbort(pProxyTls);
				xrtTlsStreamDestroy(pProxyTls);
			}
		#endif
	#endif
	if ( pStreamCall != NULL ) {
		(void)xrtHttp1CallCancel(pStreamCall);
		xrtHttp1CallDestroy(pStreamCall);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		(void)__xrtHttpRetryCancel(pCall);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)__xrtHttpPoolCancel(pCall);
	#endif
}



/*
	原子进入异常终止状态并保留全部需要取消的活动 Call。
	Call 引用组成锁外临时链，避免取消回调在 Client 锁内重入。
*/
static xhttpcall* __xrtHttpClientAbortCalls(
	xhttpclient* pClient,
	bool* pAccepted
)
{
	xhttpclientstate State;
	xhttpcall* pAbort = NULL;

	(void)xrtSpinLock(&pClient->LifecycleLock);
	State = (xhttpclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	*pAccepted = (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ||
		(State == XHTTP_CLIENT_ABORTING) ||
		(State == XHTTP_CLIENT_CLOSED);
	if ( (State == XHTTP_CLIENT_RUNNING) ||
		(State == XHTTP_CLIENT_DRAINING) ) {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_CLIENT_ABORTING,
			XMEMORY_RELEASE
		);
		for ( xhttpcall* pCall = pClient->CallHead;
			pCall != NULL;
			pCall = pCall->ClientNext ) {
			bool bCancel = false;

			(void)xrtSpinLock(&pCall->Lock);
			if ( !xrtAtomic32Load(
				&pCall->FinishGate,
				XMEMORY_RELAXED
			) && !xrtAtomic32Load(
				&pCall->CancelGate,
				XMEMORY_RELAXED
			) ) {
				xrtAtomic32Store(
					&pCall->CancelGate,
					1,
					XMEMORY_RELEASE
				);
				bCancel = true;
			}
			(void)xrtSpinUnlock(&pCall->Lock);
			if ( bCancel ) {
				(void)xrtRefRetain(&pCall->References);
				pCall->AbortNext = pAbort;
				pAbort = pCall;
			}
		}
	}
	(void)xrtSpinUnlock(&pClient->LifecycleLock);
	return pAbort;
}



/* 停止 Client 并协作取消全部已提交 Call。 */
XHTTP_CLIENT_API bool xrtHttpClientAbort(xhttpclient* pClient)
{
	xhttpcall* pAbort;
	bool bAccepted;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"abort-http-client",
			"HTTP client is null",
			NULL
		);
		return false;
	}
	pAbort = __xrtHttpClientAbortCalls(
		pClient,
		&bAccepted
	);
	if ( !bAccepted ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtHttpClientCloseIdle(pClient);
	#endif
	while ( pAbort != NULL ) {
		xhttpcall* pNext = pAbort->AbortNext;

		pAbort->AbortNext = NULL;
		__xrtHttpCallCancelActive(pAbort);
		xrtHttpCallDestroy(pAbort);
		pAbort = pNext;
	}
	__xrtHttpClientTryFinish(pClient);
	return true;
}



/* 原子决定超时与显式取消的胜者，再执行统一取消分发。 */
static bool __xrtHttpCallTimeout(
	xhttpcall* pCall,
	uint32 iCause
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	xrtAtomic32Store(
		&pCall->TimeoutCause,
		iCause,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttpCallCancelActive(pCall);
	return true;
}



/* 从任意线程把显式取消请求转发给当前有效阶段。 */
XHTTP_CLIENT_API bool xrtHttpCallCancel(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"cancel-http-call",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_RELAXED
	) ) {
		(void)xrtSpinUnlock(&pCall->Lock);
		return false;
	}
	xrtAtomic32Store(
		&pCall->CancelGate,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtSpinUnlock(&pCall->Lock);
	__xrtHttpCallCancelActive(pCall);
	return true;
}



/* 在线性化锁内取得当前低级 Call 的独立引用。 */
static xhttp1call* __xrtHttpCallStreamRef(xhttpcall* pCall)
{
	xhttp1call* pStreamCall = NULL;

	(void)xrtSpinLock(&pCall->Lock);
	if ( !xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_RELAXED
	) && (pCall->StreamCall != NULL) ) {
		pStreamCall = xrtHttp1CallRef(pCall->StreamCall);
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	return pStreamCall;
}



/* 把 Worker 内暂停请求转发给当前 HTTP/1 调用。 */
XHTTP_CLIENT_API bool xrtHttpCallPause(xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bResult;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"pause-http-call",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef(pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bResult = xrtHttp1CallPause(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bResult;
}



/* 把跨线程恢复请求转发给当前 HTTP/1 调用。 */
XHTTP_CLIENT_API bool xrtHttpCallResume(xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bResult;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"resume-http-call",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef(pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bResult = xrtHttp1CallResume(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bResult;
}



/* 查询当前低级 Call 的暂停快照。 */
XHTTP_CLIENT_API bool xrtHttpCallPaused(const xhttpcall* pCall)
{
	xhttp1call* pStreamCall;
	bool bPaused;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-pause",
			"HTTP call is null",
			NULL
		);
		return false;
	}
	pStreamCall = __xrtHttpCallStreamRef((xhttpcall*)pCall);
	if ( pStreamCall == NULL ) {
		return false;
	}
	bPaused = xrtHttp1CallPaused(pStreamCall);
	xrtHttp1CallDestroy(pStreamCall);
	return bPaused;
}



/* 返回创建 Call 时选择且不再变化的网络 Worker。 */
XHTTP_CLIENT_API xnetworker* xrtHttpCallWorker(const xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-worker",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	return pCall->Worker;
}



/* 在 Call 锁内克隆当前有效请求，避免与重定向替换并发。 */
XHTTP_CLIENT_API xhttprequest* xrtHttpCallRequestClone(
	const xhttpcall* pCall
)
{
	xhttpcall* pMutable = (xhttpcall*)pCall;
	xhttprequest* pRequest;
	xerror* pCause;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"clone-http-call-request",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	(void)xrtSpinLock(&pMutable->Lock);
	pRequest = xrtHttpRequestClone(pCall->Request);
	(void)xrtSpinUnlock(&pMutable->Lock);
	if ( pRequest == NULL ) {
		pCause = xrtTakeError();
		__xrtHttpClientSetError(
			XERR_MEMORY,
			XHTTP_CLIENT_ERROR_REQUEST,
			"clone-http-call-request",
			"HTTP call request could not be cloned",
			pCause
		);
		xrtErrorFree(pCause);
	}
	return pRequest;
}



/* 返回 Call 状态快照。 */
XHTTP_CLIENT_API xhttpcallstate xrtHttpCallState(const xhttpcall* pCall)
{
	xhttpcallstate State;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-state",
			"HTTP call is null",
			NULL
		);
		return XHTTP_CALL_FAILED;
	}
	State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( (State == XHTTP_CALL_DIALING) &&
			(xrtAtomic32Load(
				&pCall->Info.Secure,
				XMEMORY_ACQUIRE
			) != 0) ) {
			xhttpcall* pMutable = (xhttpcall*)pCall;
			xtlsdial* pDial = NULL;

			(void)xrtSpinLock(&pMutable->Lock);
			if ( pMutable->TlsDial != NULL ) {
				pDial = xrtTlsDialRef(
					pMutable->TlsDial
				);
			}
			(void)xrtSpinUnlock(&pMutable->Lock);
			if ( pDial != NULL ) {
				if ( xrtTlsDialState(pDial) ==
					XTLS_DIAL_HANDSHAKE ) {
					State =
						XHTTP_CALL_HANDSHAKING;
				}
				xrtTlsDialDestroy(pDial);
			}
		}
	#endif
	return State;
}



/* 返回 Call 保存的终态错误。 */
XHTTP_CLIENT_API const xerror* xrtHttpCallError(const xhttpcall* pCall)
{
	xhttpcallstate State;

	if ( pCall == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-error",
			"HTTP call is null",
			NULL
		);
		return NULL;
	}
	State = (xhttpcallstate)xrtAtomic32Load(
		&pCall->State,
		XMEMORY_ACQUIRE
	);
	if ( State < XHTTP_CALL_SUCCEEDED ) {
		return NULL;
	}
	return pCall->Error;
}



/* 复制 Call 当前的一致终态或近似并发运行快照。 */
XHTTP_CLIENT_API bool xrtHttpCallInfo(
	const xhttpcall* pCall,
	xhttpcallinfo* pInfo
)
{
	xhttpcallinfo Info;

	if ( (pCall == NULL) ||
		!xrtMemRangeValid(pInfo, sizeof(Info)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-call-info",
			"HTTP call and complete info storage are required",
			NULL
		);
		return false;
	}
	__xrtHttpCallInfoCopy(pCall, &Info);
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 返回 Client 保留的默认代理借用引用。 */
XHTTP_CLIENT_API const xnetproxy* xrtHttpClientProxy(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Proxy : NULL;
}

#endif



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

/* 返回 Client 保留的共享 CookieJar。 */
XHTTP_CLIENT_API xcookiejar* xrtHttpClientCookieJar(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Cookies : NULL;
}

#endif



#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_tcp.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT)



#if defined(XHTTP_FEATURE_HTTP_CLIENT)

/* 保存一次低层失败提升后的稳定客户端错误。 */
typedef struct xrt_http_client_failure {
	xhttpclienterror Error;
	xerrkind Kind;
	cstr Operation;
	cstr Message;
} xrt_http_client_failure;



/* 沿原因链查找指定错误域中的第一层错误。 */
static const xerror* __xrtHttpClientErrorDomain(
	const xerror* pError,
	cstr sDomain
)
{
	while ( pError != NULL ) {
		cstr sCurrent = xrtErrorDomain(pError);

		if ( (sCurrent != NULL) &&
			(strcmp(sCurrent, sDomain) == 0) ) {
			return pError;
		}
		pError = xrtErrorCause(pError);
	}
	return NULL;
}



/* 判断 Exchange 回调失败是否来自调用方公开事件。 */
static bool __xrtHttpClientUserCallback(
	const xhttpcall* pCall,
	const xerror* pExchangeError
)
{
	int32 iCode = xrtErrorCode(pExchangeError);
	cstr sOperation = xrtErrorOperation(pExchangeError);

	if ( iCode == XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK ) {
		return pCall->Events.Body != NULL;
	}
	if ( iCode != XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK ) {
		return false;
	}
	if ( (sOperation != NULL) &&
		(strcmp(
			sOperation,
			"deliver-http1-informational"
		) == 0) ) {
		return pCall->Events.Informational != NULL;
	}
	if ( (sOperation != NULL) &&
		(strcmp(
			sOperation,
			"deliver-http1-response-head"
		) == 0) ) {
		return pCall->Events.Headers != NULL;
	}
	return (pCall->Events.Informational != NULL) ||
		(pCall->Events.Headers != NULL);
}



/* 把 Exchange 错误提升为请求、响应、协议或用户回调错误。 */
static void __xrtHttpClientClassifyExchange(
	const xhttpcall* pCall,
	const xerror* pExchangeError,
	xrt_http_client_failure* pFailure
)
{
	int32 iCode = xrtErrorCode(pExchangeError);
	xerrkind Kind = __xrtHttpClientCauseKind(
		pExchangeError,
		XERR_PROTOCOL
	);

	pFailure->Kind = Kind;
	switch ( iCode ) {
		case XHTTP1_EXCHANGE_ERROR_REQUEST_BODY:
		case XHTTP1_EXCHANGE_ERROR_REQUEST_LENGTH:
			pFailure->Error = XHTTP_CLIENT_ERROR_REQUEST;
			pFailure->Operation = "produce-http-request";
			pFailure->Message =
				"HTTP request body production failed";
			return;

		case XHTTP1_EXCHANGE_ERROR_HEADER_CALLBACK:
		case XHTTP1_EXCHANGE_ERROR_BODY_CALLBACK:
			if ( __xrtHttpClientUserCallback(
				pCall,
				pExchangeError
			) ) {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_CALLBACK;
				pFailure->Operation =
					"handle-http-response-callback";
				pFailure->Message =
					"HTTP response callback stopped the call";
			} else {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_RESPONSE;
				pFailure->Operation =
					"store-http-response";
				pFailure->Message =
					"HTTP response processing failed";
			}
			return;

		case XHTTP1_EXCHANGE_ERROR_INPUT_LIMIT:
		case XHTTP1_EXCHANGE_ERROR_INFORMATIONAL_LIMIT:
			pFailure->Error = XHTTP_CLIENT_ERROR_RESPONSE;
			pFailure->Operation = "receive-http-response";
			pFailure->Message =
				"HTTP response exceeds its configured limits";
			return;

		case XHTTP1_EXCHANGE_ERROR_RESPONSE_HEAD:
		case XHTTP1_EXCHANGE_ERROR_RESPONSE_FRAMING:
			if ( (Kind == XERR_MEMORY) ||
				(Kind == XERR_RANGE) ) {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_RESPONSE;
				pFailure->Operation =
					"store-http-response";
				pFailure->Message =
					"HTTP response could not be represented";
			} else {
				pFailure->Error =
					XHTTP_CLIENT_ERROR_PROTOCOL;
				pFailure->Operation =
					"parse-http-response";
				pFailure->Message =
					"HTTP response protocol is invalid";
			}
			return;

		case XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF:
			pFailure->Error = XHTTP_CLIENT_ERROR_PROTOCOL;
			pFailure->Operation = "parse-http-response";
			pFailure->Message =
				"HTTP response ended before completion";
			return;

		case XHTTP1_EXCHANGE_ERROR_ARGUMENT:
		case XHTTP1_EXCHANGE_ERROR_STATE:
		default:
			pFailure->Error = XHTTP_CLIENT_ERROR_INTERNAL;
			pFailure->Operation = "run-http-exchange";
			pFailure->Message =
				"HTTP Exchange entered an invalid state";
			return;
	}
}



/* 把低层 HTTP/1 Call 原因链提升为唯一的高层错误契约。 */
static xrt_http_client_failure __xrtHttpClientClassifyFailure(
	const xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	xrt_http_client_failure Failure = {
		XHTTP_CLIENT_ERROR_INTERNAL,
		XERR_INTERNAL,
		"run-http1-call",
		"HTTP/1 call failed without a classified cause"
	};
	const xerror* pCallError;
	const xerror* pExchangeError;
	int32 iCode;

	if ( pResult == NULL ) {
		return Failure;
	}
	if ( pResult->Result == XNET_RESULT_CANCELLED ) {
		Failure.Error = XHTTP_CLIENT_ERROR_CANCELLED;
		Failure.Kind = XERR_CANCELLED;
		Failure.Operation = "cancel-http-call";
		Failure.Message = "HTTP call was cancelled";
		return Failure;
	}
	pCallError = __xrtHttpClientErrorDomain(
		pResult->Error,
		"xrt.http.call"
	);
	if ( pCallError == NULL ) {
		Failure.Kind = __xrtHttpClientCauseKind(
			pResult->Error,
			XERR_INTERNAL
		);
		return Failure;
	}
	iCode = xrtErrorCode(pCallError);
	if ( iCode == XHTTP1_CALL_ERROR_TRANSPORT ) {
		Failure.Error = XHTTP_CLIENT_ERROR_TRANSPORT;
		Failure.Kind = __xrtHttpClientCauseKind(
			pCallError,
			XERR_IO
		);
		Failure.Operation = "transport-http1";
		Failure.Message = "HTTP/1 transport failed";
		return Failure;
	}
	if ( iCode == XHTTP1_CALL_ERROR_CANCELLED ) {
		Failure.Error = XHTTP_CLIENT_ERROR_CANCELLED;
		Failure.Kind = XERR_CANCELLED;
		Failure.Operation = "cancel-http-call";
		Failure.Message = "HTTP call was cancelled";
		return Failure;
	}
	if ( iCode != XHTTP1_CALL_ERROR_EXCHANGE ) {
		Failure.Kind = __xrtHttpClientCauseKind(
			pCallError,
			XERR_INTERNAL
		);
		return Failure;
	}
	pExchangeError = __xrtHttpClientErrorDomain(
		pCallError,
		"xrt.http.exchange"
	);
	if ( pExchangeError != NULL ) {
		__xrtHttpClientClassifyExchange(
			pCall,
			pExchangeError,
			&Failure
		);
		return Failure;
	}
	Failure.Kind = __xrtHttpClientCauseKind(
		pCallError,
		XERR_INTERNAL
	);
	if ( xrtAtomic32Load(
		&pCall->Info.Phase,
		XMEMORY_ACQUIRE
	) == XHTTP_CALL_PHASE_REQUEST ) {
		Failure.Error = XHTTP_CLIENT_ERROR_REQUEST;
		Failure.Operation = "produce-http-request";
		Failure.Message = "HTTP request production failed";
	}
	return Failure;
}



/* 从 Call 摘除低级 HTTP/1 调用的调用方引用。 */
static void __xrtHttpClientStreamDetach(
	xhttpcall* pCall,
	xhttp1call* pStreamCall
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->StreamCall == pStreamCall ) {
		pCall->StreamCall = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 正常关闭一个不再暴露给调用方的可复用传输。 */
static void __xrtHttpClientStreamClose(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pResult->Tls != NULL ) {
				(void)xrtTlsStreamClose(pResult->Tls);
				xrtTlsStreamDestroy(pResult->Tls);
			}
		#else
			(void)pResult;
		#endif
		return;
	}
	if ( pResult->Tcp != NULL ) {
		(void)xrtNetStreamClose(pResult->Tcp);
		xrtNetStreamDestroy(pResult->Tcp);
	}
}



/* 异常回收一个不满足高层结果不变量的传输。 */
static void __xrtHttpClientStreamAbort(
	const xhttp1callresult* pResult
)
{
	if ( pResult->Tcp != NULL ) {
		(void)xrtNetStreamAbort(pResult->Tcp);
		xrtNetStreamDestroy(pResult->Tcp);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pResult->Tls != NULL ) {
			(void)xrtTlsStreamAbort(pResult->Tls);
			xrtTlsStreamDestroy(pResult->Tls);
		}
	#endif
}



/* 验证成功结果中的传输所有权、复用和协议升级关系。 */
static bool __xrtHttpClientStreamSuccessValid(
	const xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	bool bTransferred;

	if ( (pResult->Response == NULL) ||
		(pResult->Reusable && pResult->Upgraded) ||
		(pResult->Reusable && (pResult->Buffered != 0)) ) {
		return false;
	}
	bTransferred = pResult->Reusable || pResult->Upgraded;
	if ( !bTransferred ) {
		return (pResult->Tcp == NULL)
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				&& (pResult->Tls == NULL)
			#endif
			;
	}
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			return (pResult->Tcp == NULL) &&
				(pResult->Tls != NULL);
		#else
			return false;
		#endif
	}
	return (pResult->Tcp != NULL)
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			&& (pResult->Tls == NULL)
		#endif
		;
}



/* 把低级 HTTP/1 Call 的唯一终态提升为完整客户端结果。 */
void __xrtHttpClientStreamDone(
	xhttp1call* pStreamCall,
	const xhttp1callresult* pResult,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xrt_http_client_failure Failure;
	xhttpresponse* pResponse;
	xnetstream* pTcp = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		bool bPooled = false;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xerror* pPolicyCause;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		xerror* pDecompressCause;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xerror* pCacheCause;
		bool bCacheReplayed;
	#endif

	__xrtHttpClientStreamDetach(pCall, pStreamCall);
	if ( (pResult == NULL) ||
		(pResult->Result != XNET_RESULT_OK) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			if ( __xrtHttpCookieFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
			if ( __xrtHttpClientCacheFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
			if ( __xrtHttpRedirectFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
			if ( __xrtHttpDecompressFail(
				pCall,
				pResult != NULL ?
					pResult->Error : NULL
			) ) {
				xrtHttp1CallDestroy(pStreamCall);
				return;
			}
		#endif
		Failure = __xrtHttpClientClassifyFailure(
			pCall,
			pResult
		);
		__xrtHttpCallFail(
			pCall,
			pResult != NULL ?
				pResult->Result :
				XNET_RESULT_ERROR,
			Failure.Error,
			Failure.Kind,
			Failure.Operation,
			Failure.Message,
			pResult != NULL ? pResult->Error : NULL
		);
		xrtHttp1CallDestroy(pStreamCall);
		return;
	}
	pResponse = pResult->Response;
	if ( !__xrtHttpClientStreamSuccessValid(
		pCall,
		pResult
	) ) {
		xrtHttpResponseDestroy(pResponse);
		__xrtHttpClientStreamAbort(pResult);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"complete-http-call",
			"HTTP/1 call returned an inconsistent success result",
			NULL
		);
		xrtHttp1CallDestroy(pStreamCall);
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		__xrtHttpResumeCollect(pCall, pResult->Tls);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		if ( __xrtHttpRetryPending(pCall) ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			(void)__xrtHttpRetrySchedule(pCall);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		if ( !__xrtHttpClientCacheDone(
			pCall,
			pResponse,
			&bCacheReplayed
		) ) {
			pCacheCause = xrtTakeError();
			__xrtHttpClientStreamClose(
				pCall,
				pResult
			);
			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpClientCacheFail(
				pCall,
				pCacheCause
			) ) {
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_CACHE,
					__xrtHttpClientCauseKind(
						pCacheCause,
						XERR_IO
					),
					"process-http-cache-response",
					"HTTP response cache processing failed",
					pCacheCause
				);
			}
			xrtErrorFree(pCacheCause);
			xrtHttp1CallDestroy(pStreamCall);
			return;
		}
		if ( bCacheReplayed ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			if ( !__xrtHttpClientCacheStart(
				pCall,
				&bCacheReplayed
			) ) {
				pCacheCause = xrtTakeError();
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_CACHE,
					__xrtHttpClientCauseKind(
						pCacheCause,
						XERR_MEMORY
					),
					"deliver-http-cache-validation",
					"validated HTTP cache response could not be rebuilt",
					pCacheCause
				);
				xrtErrorFree(pCacheCause);
			}
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pCall->RedirectPending ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				if ( pResult->Reusable ) {
					bPooled = __xrtHttpPoolPut(
						pCall,
						pResult
					);
					if ( !bPooled ) {
						xrtClearError();
					}
				}
				if ( !bPooled )
			#endif
			{
				__xrtHttpClientStreamClose(
					pCall,
					pResult
				);
			}
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
				__xrtHttpPoolFinish(pCall);
			#endif
			xrtHttpResponseDestroy(pResponse);
			xrtHttp1CallDestroy(pStreamCall);
			if ( !__xrtHttpRedirectAdvance(pCall) ) {
				pPolicyCause = xrtTakeError();
				#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
					if ( __xrtHttpCookieFail(
						pCall,
						pPolicyCause
					) ) {
						xrtErrorFree(pPolicyCause);
						return;
					}
				#endif
				(void)__xrtHttpRedirectFail(
					pCall,
					pPolicyCause
				);
				xrtErrorFree(pPolicyCause);
				return;
			}
			__xrtHttpCallStartHop(pCall);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressFinish(
			pCall,
			pResponse
		) ) {
			pDecompressCause = xrtTakeError();
			__xrtHttpClientStreamClose(
				pCall,
				pResult
			);
			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpDecompressFail(
				pCall,
				pDecompressCause
			) ) {
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_CALLBACK,
					__xrtHttpClientCauseKind(
						pDecompressCause,
						XERR_CANCELLED
					),
					"handle-http-response-callback",
					"HTTP response body callback stopped final decoding",
					pDecompressCause
				);
			}
			xrtErrorFree(pDecompressCause);
			xrtHttp1CallDestroy(pStreamCall);
			return;
		}
	#endif
	if ( pResult->Upgraded ) {
		pTcp = pResult->Tcp;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls = pResult->Tls;
		#endif
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
			__xrtHttpPoolTransferred(pCall);
		#endif
	} else {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
			if ( pResult->Reusable ) {
				bPooled = __xrtHttpPoolPut(
					pCall,
					pResult
				);
				if ( !bPooled ) {
					xrtClearError();
				}
			}
			if ( !bPooled )
		#endif
		{
			__xrtHttpClientStreamClose(pCall, pResult);
		}
	}
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		pTcp,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			pTls,
		#endif
		pResult->Upgraded ? pResult->Buffered : 0,
		pResult->Upgraded
	);
	xrtHttp1CallDestroy(pStreamCall);
}



/* 从 Call 摘除 TCP Dial 的调用方引用。 */
static void __xrtHttpCallTcpDialDetach(
	xhttpcall* pCall,
	xnetdial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TcpDial == pDial ) {
		pCall->TcpDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 在已打开 TCP Stream 上接管 Exchange 并发布活动低级 Call。 */
static bool __xrtHttpCallAttachTcp(
	xhttpcall* pCall,
	xnetstream* pStream
)
{
	xhttp1callevents Events;
	xhttp1call* pStreamCall;

	__xrtHttpCallTransportReady(pCall);
	__xrtHttpCallStreamEvents(pCall, &Events);
	pStreamCall = xrtHttp1CallTcp(
		pStream,
		pCall->Exchange,
		&pCall->Client->Config.Call,
		&Events
	);
	if ( pStreamCall == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->Exchange = NULL;
	pCall->StreamCall = pStreamCall;
	(void)xrtSpinUnlock(&pCall->Lock);
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_EXCHANGING,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtHttp1CallCancel(pStreamCall);
	}
	return true;
}



/* TCP Dial 成功后在获胜 Stream 所属 Worker 上建立 HTTP/1 Call。 */
static void __xrtHttpCallTcpDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpCallTcpDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_DIAL,
			Result == XNET_RESULT_CANCELLED ?
				XERR_CANCELLED :
				__xrtHttpClientCauseKind(
					pError,
					XERR_IO
				),
			"dial-http",
			Result == XNET_RESULT_CANCELLED ?
				"HTTP TCP dial was cancelled" :
				"HTTP TCP dial failed",
			pError
		);
		xrtNetDialDestroy(pDial);
		return;
	}
	__xrtHttpCallTcpConnected(
		pCall,
		pStream,
		"dial-http"
	);
	xrtNetDialDestroy(pDial);
}



/* 接管一条已经建立的 TCP 传输并统一处理取消、配额与 HTTP/1 附加。 */
void __xrtHttpCallTcpConnected(
	xhttpcall* pCall,
	xnetstream* pStream,
	cstr sOperation
)
{
	xerror* pCause;

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolOpened(pCall);
	#endif
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			sOperation,
			"HTTP call was cancelled after TCP connected",
			NULL
		);
		return;
	}
	if ( !__xrtHttpCallAttachTcp(pCall, pStream) ) {
		pCause = xrtTakeError();
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			__xrtHttpClientCauseKind(
				pCause,
				XERR_INTERNAL
			),
			"start-http1",
			"HTTP/1 call could not attach to the connected TCP stream",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TCP Stream Worker 上重新附加 Exchange，失效连接原位重拨。 */
bool __xrtHttpCallStartPooledTcp(xhttpcall* pCall)
{
	xnetstream* pStream = pCall->PooledTcp;

	if ( !__xrtHttp1TcpReusable(pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return __xrtHttpCallStartTcp(pCall);
	}
	if ( !__xrtHttpCallAttachTcp(pCall, pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return false;
	}
	__xrtHttpPoolPooledUsed(pCall);
	return true;
}

#endif



/* 在目标 Worker 上创建解析和地址竞速操作。 */
bool __xrtHttpCallStartTcp(xhttpcall* pCall)
{
	xnetdialconfig Config = pCall->Client->Config.Dial;
	xnetstreamevents Events;
	xnetdial* pDial;

	memset(&Events, 0, sizeof(Events));
	Config.Affinity = pCall->Affinity;
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtNetDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Host,
		pCall->Port,
		&Config,
		&Events,
		pCall,
		__xrtHttpCallTcpDone,
		pCall
	);
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->TcpDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetDialCancel(pDial);
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_redirect.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)

/* 计算本次调用实际使用的重定向模式和上限。 */
static xhttpredirectmode __xrtHttpRedirectMode(
	const xhttpcall* pCall,
	uint32* pMaxHops
)
{
	xhttpredirectmode Mode = pCall->RedirectMode;

	*pMaxHops = pCall->RedirectConfig.MaxHops;
	if ( Mode == XHTTP_REDIRECT_DEFAULT ) {
		Mode = *pMaxHops == 0 ?
			XHTTP_REDIRECT_MANUAL :
			XHTTP_REDIRECT_FOLLOW;
	} else if ( (Mode == XHTTP_REDIRECT_FOLLOW) &&
		(*pMaxHops == 0) ) {
		*pMaxHops = XHTTP_REDIRECT_MAX_DEFAULT;
	}
	return Mode;
}



/* 保存第一个重定向策略错误，供低层 Call 完成入口映射。 */
static bool __xrtHttpRedirectReject(
	xhttpcall* pCall,
	xhttpclienterror Error
)
{
	pCall->RedirectError = Error;
	return false;
}



/* 调用无 IO 的重定向策略，并保留待排空响应的 Call 状态。 */
static bool __xrtHttpRedirectBuild(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	const xhttpfield* pLocation
)
{
	xhttpredirecterror Error;
	xhttprequest* pNext = xrtHttpRedirectRequest(
		pCall->Request, xrtHttpResponseStatus(pResponse),
		pLocation->Value, pCall->RedirectConfig.Flags, &Error
	);
	if ( pNext == NULL ) {
		return __xrtHttpRedirectReject(pCall,
			Error == XHTTP_REDIRECT_DOWNGRADE ? XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE :
			Error == XHTTP_REDIRECT_REPLAY ? XHTTP_CLIENT_ERROR_REDIRECT_REPLAY :
			XHTTP_CLIENT_ERROR_REDIRECT);
	}
	xrtHttpRequestDestroy(pCall->RedirectRequest);
	pCall->RedirectRequest = pNext;
	pCall->RedirectPending = true;
	return true;
}

/* 释放一次 Call 尚未提交的重定向请求。 */
void __xrtHttpRedirectUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	xrtHttpRequestDestroy(pCall->RedirectRequest);
	pCall->RedirectRequest = NULL;
	pCall->RedirectPending = false;
}



/* 信息响应保持低层调用时序，不参与最终响应隐藏规则。 */
static bool __xrtHttpRedirectInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RedirectNext.Informational == NULL ) {
		return true;
	}
	return pCall->RedirectNext.Informational(
		pResponse,
		pCall->RedirectNext.Data
	);
}



/* 在正文到达前决定当前响应是最终结果还是待排空中间跳。 */
static bool __xrtHttpRedirectHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	const xhttpheaders* pHeaders;
	const xhttpfield* pLocation;
	xhttpredirectmode Mode;
	xhttpnext Next;
	uint32 iMaxHops;

	__xrtHttpRedirectUnit(pCall);
	pCall->RedirectError = 0;
	if ( !xrtHttpRedirectStatus(
		xrtHttpResponseStatus(pResponse)
	) ) {
		goto deliver;
	}
	pLocation = xrtHttpResponseHeader(
		pResponse,
		XRT_STR_LITERAL("Location")
	);
	if ( pLocation == NULL ) {
		goto deliver;
	}
	Mode = __xrtHttpRedirectMode(
		pCall,
		&iMaxHops
	);
	if ( Mode == XHTTP_REDIRECT_MANUAL ) {
		goto deliver;
	}
	if ( Mode == XHTTP_REDIRECT_ERROR ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	Next = xrtHttpHeadersGetUnique(
		pHeaders,
		XRT_STR_LITERAL("Location"),
		&pLocation
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT
		);
	}
	if ( pCall->Redirects >= iMaxHops ) {
		return __xrtHttpRedirectReject(
			pCall,
			XHTTP_CLIENT_ERROR_REDIRECT_LIMIT
		);
	}
	return __xrtHttpRedirectBuild(
		pCall,
		pResponse,
		pLocation
	);

deliver:
	if ( pCall->RedirectNext.Headers == NULL ) {
		return true;
	}
	return pCall->RedirectNext.Headers(
		pResponse,
		pCall->RedirectNext.Data
	);
}



/* 中间跳正文只为连接复用而排空，最终响应仍按调用方选择流式交付。 */
static bool __xrtHttpRedirectBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RedirectPending ) {
		return true;
	}
	if ( pCall->RedirectNext.Body == NULL ) {
		return xrtHttpBridgeResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->RedirectNext.Body(
		pResponse,
		Data,
		pCall->RedirectNext.Data
	);
}



/* 建立调用拥有的请求快照和稳定策略副本。 */
bool __xrtHttpRedirectInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	if ( (pCall == NULL) || (pCall->Request == NULL) ||
		(pOptions == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pCall->Client->Config.Redirect.Flags &
		~(XHTTP_REDIRECT_POST_TO_GET | XHTTP_REDIRECT_FORWARD_CREDENTIALS | XHTTP_REDIRECT_ALLOW_DOWNGRADE)) != 0 ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (pOptions->Redirect < XHTTP_REDIRECT_DEFAULT) ||
		(pOptions->Redirect > XHTTP_REDIRECT_ERROR) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pCall->RedirectConfig =
		pCall->Client->Config.Redirect;
	pCall->RedirectMode = pOptions->Redirect;
	return true;
}



/* 返回带当前 Call 数据的事件包装器。 */
const xhttp1exchangeevents* __xrtHttpRedirectEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	pCall->RedirectNext = *pNext;
	memset(
		&pCall->RedirectEvents,
		0,
		sizeof(pCall->RedirectEvents)
	);
	pCall->RedirectEvents.Informational =
		__xrtHttpRedirectInformational;
	pCall->RedirectEvents.Headers =
		__xrtHttpRedirectHeaders;
	pCall->RedirectEvents.Body =
		__xrtHttpRedirectBody;
	pCall->RedirectEvents.Data = pCall;
	return &pCall->RedirectEvents;
}



/* 把包装器拒绝映射成稳定的高层重定向错误。 */
bool __xrtHttpRedirectFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xhttpclienterror Error = pCall->RedirectError;
	xerrkind Kind;
	cstr sOperation;
	cstr sMessage;

	if ( Error == 0 ) {
		return false;
	}
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY : XERR_PROTOCOL;
	sOperation = "follow-http-redirect";
	sMessage = "HTTP redirect is invalid";
	if ( Error == XHTTP_CLIENT_ERROR_REDIRECT_LIMIT ) {
		Kind = XERR_RANGE;
		sMessage = "HTTP redirect limit was reached";
	} else if (
		Error == XHTTP_CLIENT_ERROR_REDIRECT_REPLAY
	) {
		Kind = XERR_STATE;
		sMessage =
			"HTTP redirect requires a replayable request body";
	} else if (
		Error == XHTTP_CLIENT_ERROR_REDIRECT_DOWNGRADE
	) {
		Kind = XERR_PERMISSION;
		sMessage =
			"HTTPS redirect to HTTP is disabled";
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		Error,
		Kind,
		sOperation,
		sMessage,
		pCause
	);
	return true;
}



/* 提交下一跳请求并建立新的低层 Exchange。 */
bool __xrtHttpRedirectAdvance(xhttpcall* pCall)
{
	xhttprequest* pOld;

	if ( (pCall == NULL) ||
		!pCall->RedirectPending ||
		(pCall->RedirectRequest == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pOld = pCall->Request;
	pCall->Request = pCall->RedirectRequest;
	pCall->RedirectRequest = NULL;
	(void)xrtSpinUnlock(&pCall->Lock);
	pCall->RedirectPending = false;
	pCall->Redirects++;
	xrtAtomic64Store(
		&pCall->Info.Redirects,
		(uint64)pCall->Redirects,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		(void)xrtAtomic64FetchAdd(
			&pCall->Client->RedirectsFollowed,
			1,
			XMEMORY_RELAXED
		);
	#endif
	xrtHttpRequestDestroy(pOld);
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
			if ( pCall->CookieError != 0 ) {
				return false;
			}
		#endif
		pCall->RedirectError =
			XHTTP_CLIENT_ERROR_REDIRECT;
		return false;
	}
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_QUEUED,
		XMEMORY_RELEASE
	);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_retry.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)

#define XHTTP_RETRY_FLAGS_MASK \
	(XHTTP_RETRY_STATUS | \
	 XHTTP_RETRY_TRANSPORT | \
	 XHTTP_RETRY_RESPECT_AFTER | \
	 XHTTP_RETRY_JITTER)



/* 初始化默认关闭、启用后采用保守完整能力的重试策略。 */
XHTTP_CLIENT_API void xrtHttpRetryConfigInit(
	xhttpretryconfig* pConfig
)
{
	const xhttpretryconfig Config = {
		XHTTP_RETRY_BASE_DEFAULT,
		XHTTP_RETRY_DELAY_MAX_DEFAULT,
		XHTTP_RETRY_FLAGS_MASK,
		0
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化为继承 Client 且不授权非幂等重放。 */
XHTTP_CLIENT_API void xrtHttpRetryOptionsInit(
	xhttpretryoptions* pOptions
)
{
	const xhttpretryoptions Options = {
		XHTTP_RETRY_DEFAULT,
		0
	};

	if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 判断当前请求是否满足方法与正文重放边界。 */
static bool __xrtHttpRetryReplayable(const xhttpcall* pCall)
{
	xhttpbody* pBody;

	if ( !pCall->RetryUnsafe &&
		!xrtHttpMethodIdempotent(
			xrtHttpRequestMethod(pCall->Request)
		) ) {
		return false;
	}
	pBody = xrtHttpRequestBody(pCall->Request);
	return (pBody == NULL) || xrtHttpBodyReplayable(pBody);
}



/* 判断当前调用是否还可以消费一个新的重试名额。 */
static bool __xrtHttpRetryAvailable(const xhttpcall* pCall)
{
	return pCall->RetryEnabled &&
		(pCall->Retries < (size_t)pCall->RetryMax) &&
		__xrtHttpRetryReplayable(pCall);
}



/* 初始化当前 Call 独占的非安全随机状态。 */
static void __xrtHttpRetryRng(xhttpcall* pCall)
{
	if ( pCall->RetryRngReady ) {
		return;
	}
	xrtRngSeed(
		&pCall->RetryRng,
		(uint64)xrtNow() ^ (uint64)(uintptr_t)pCall,
		(uint64)(uintptr_t)pCall->Request
	);
	pCall->RetryRngReady = true;
}



/* 计算下一次本地退避，并按需应用包含零和上界的 full jitter。 */
static bool __xrtHttpRetryBackoffDelay(
	xhttpcall* pCall,
	uint64* pDelay
)
{
	uint64 iDelay;

	if ( !xrtHttpRetryBackoff(
		pCall->RetryConfig.BaseDelay,
		pCall->RetryConfig.MaxDelay,
		(uint32)pCall->Retries,
		&iDelay
	) ) {
		return false;
	}
	if ( ((pCall->RetryConfig.Flags &
		  XHTTP_RETRY_JITTER) != 0) && (iDelay != 0) ) {
		__xrtHttpRetryRng(pCall);
		iDelay = iDelay == UINT64_MAX ?
			xrtRng64(&pCall->RetryRng) :
			xrtRngBelow64(&pCall->RetryRng, iDelay + 1u);
	}
	*pDelay = iDelay;
	return true;
}



/* 读取有效的服务端等待建议，非法或重复值回退到本地策略。 */
static bool __xrtHttpRetryDelay(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	uint64* pDelay
)
{
	const xhttpheaders* pHeaders;
	xhttpnext Next;
	uint64 iDelay;

	if ( (pCall->RetryConfig.Flags &
		 XHTTP_RETRY_RESPECT_AFTER) != 0 ) {
		pHeaders = xrtHttpResponseHeaders(pResponse);
		Next = xrtHttpRetryAfterFields(
			xrtHttpHeadersData(pHeaders),
			xrtHttpHeadersCount(pHeaders),
			xrtNow(),
			&iDelay
		);
		if ( Next == XHTTP_NEXT_ITEM ) {
			*pDelay = iDelay > pCall->RetryConfig.MaxDelay ?
				pCall->RetryConfig.MaxDelay : iDelay;
			return true;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			xrtClearError();
		}
	}
	return __xrtHttpRetryBackoffDelay(pCall, pDelay);
}



/* 信息响应不参与重试决策，保持原有回调次序。 */
static bool __xrtHttpRetryInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RetryNext.Informational == NULL ) {
		return true;
	}
	return pCall->RetryNext.Informational(
		pResponse,
		pCall->RetryNext.Data
	);
}



/* 在正文前决定是否隐藏并排空一个临时失败响应。 */
static bool __xrtHttpRetryHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	pCall->RetryPending = false;
	if ( ((pCall->RetryConfig.Flags & XHTTP_RETRY_STATUS) != 0) &&
		xrtHttpRetryStatusDefault(
			xrtHttpResponseStatus(pResponse)
		) && __xrtHttpRetryAvailable(pCall) ) {
		if ( !__xrtHttpRetryDelay(
			pCall,
			pResponse,
			&pCall->RetryDelay
		) ) {
			return false;
		}
		pCall->RetryPending = true;
		return true;
	}
	if ( pCall->RetryNext.Headers == NULL ) {
		return true;
	}
	return pCall->RetryNext.Headers(
		pResponse,
		pCall->RetryNext.Data
	);
}



/* 中间响应正文只用于完成消息边界和连接复用。 */
static bool __xrtHttpRetryBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->RetryPending ) {
		return true;
	}
	if ( pCall->RetryNext.Body == NULL ) {
		return xrtHttpBridgeResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->RetryNext.Body(
		pResponse,
		Data,
		pCall->RetryNext.Data
	);
}



/* 冻结客户端重试配置和调用级显式授权。 */
bool __xrtHttpRetryInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xhttpretrymode Mode;

	if ( (pCall == NULL) || (pOptions == NULL) ||
		((pCall->Client->Config.Retry.Flags &
		  ~XHTTP_RETRY_FLAGS_MASK) != 0) ||
		(pCall->Client->Config.Retry.MaxDelay == 0) ||
		(pCall->Client->Config.Retry.BaseDelay >
		 pCall->Client->Config.Retry.MaxDelay) ||
		((pOptions->Retry.Flags & ~XHTTP_RETRY_UNSAFE) != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Mode = pOptions->Retry.Mode;
	if ( (Mode < XHTTP_RETRY_DEFAULT) ||
		(Mode > XHTTP_RETRY_DISABLED) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pCall->RetryConfig = pCall->Client->Config.Retry;
	pCall->RetryMax = pCall->RetryConfig.MaxRetries;
	if ( Mode == XHTTP_RETRY_ENABLED ) {
		if ( pCall->RetryMax == 0 ) {
			pCall->RetryMax = XHTTP_RETRY_MAX_DEFAULT;
		}
		pCall->RetryEnabled = true;
	} else if ( Mode == XHTTP_RETRY_DISABLED ) {
		pCall->RetryEnabled = false;
	} else {
		pCall->RetryEnabled = pCall->RetryMax != 0;
	}
	pCall->RetryUnsafe =
		(pOptions->Retry.Flags & XHTTP_RETRY_UNSAFE) != 0;
	return true;
}



/* 建立位于 Cookie 内侧、Cache 外侧的响应隐藏包装器。 */
const xhttp1exchangeevents* __xrtHttpRetryEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	pCall->RetryNext = *pNext;
	memset(&pCall->RetryEvents, 0, sizeof(pCall->RetryEvents));
	pCall->RetryEvents.Informational =
		__xrtHttpRetryInformational;
	pCall->RetryEvents.Headers = __xrtHttpRetryHeaders;
	pCall->RetryEvents.Body = __xrtHttpRetryBody;
	pCall->RetryEvents.Data = pCall;
	return &pCall->RetryEvents;
}



/* 返回当前响应是否已经被重试包装器隐藏。 */
bool __xrtHttpRetryPending(const xhttpcall* pCall)
{
	return (pCall != NULL) && pCall->RetryPending;
}



/* 计算不回绕的单调截止时间。 */
static double __xrtHttpRetryDeadline(double iNow, uint64 iDelay)
{
    uint64 Milliseconds = iDelay > (uint64)INT64_MAX ? (uint64)INT64_MAX : iDelay;
    return nextafter(iNow + (double)(Milliseconds / 1000) + (double)(Milliseconds % 1000) * 0.001, INFINITY);
}



/* Timer 到期后创建全新的逐跳 Exchange 并重新进入池与传输。 */
static void __xrtHttpRetryTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xerror* pCause;

	(void)pWorker;
	(void)Id;
	xrtAtomic32Store(
		&pCall->RetryTimerDone,
		1,
		XMEMORY_RELEASE
	);
	(void)xrtAtomic64Exchange(
		&pCall->RetryTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( xrtAtomic32Load(
		&pCall->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		goto done;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"wait-http-retry",
			"HTTP retry wait was cancelled",
			NULL
		);
		goto done;
	}
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			XERR_INTERNAL,
			"wait-http-retry",
			"HTTP retry timer did not reach its deadline",
			NULL
		);
		goto done;
	}
	pCall->Retries++;
	xrtAtomic64Store(
		&pCall->Info.Retries,
		(uint64)pCall->Retries,
		XMEMORY_RELEASE
	);
	/* 记录新尝试的响应线字节起点，避免前一次响应污染 EOF 判定。 */
	pCall->RetryResponseWireStart = xrtAtomic64Load(
		&pCall->Info.ResponseWireBytes,
		XMEMORY_ACQUIRE
	);
	if ( !__xrtHttpCallPrepareHop(pCall) ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			__xrtHttpClientCauseKind(pCause, XERR_VALUE),
			"prepare-http-retry",
			"HTTP request could not be prepared for retry",
			pCause
		);
		xrtErrorFree(pCause);
		goto done;
	}
	__xrtHttpCallStartHop(pCall);

done:
	xrtHttpCallDestroy(pCall);
}



/* 安排一个不占用连接池配额且不消耗 idle 超时的退避 Timer。 */
bool __xrtHttpRetrySchedule(xhttpcall* pCall)
{
	xerror* pCause;
	double iNow;
	double iIdle;
	uint64 Id;

	if ( (pCall == NULL) || !__xrtHttpRetryAvailable(pCall) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pCall->RetryPending = false;
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_QUEUED,
		XMEMORY_RELEASE
	);
	__xrtHttpCallSetPhase(pCall, XHTTP_CALL_PHASE_RETRY);
	iNow = xrtTimer();
	if ( pCall->IdleTimeout != XHTTP_CLIENT_TIMEOUT_NONE ) {
		iIdle = __xrtHttpRetryDeadline(
			__xrtHttpRetryDeadline(iNow, pCall->RetryDelay),
			pCall->IdleTimeout
		);
		xrtAtomic64Store(
			&pCall->IdleDeadline,
			__xrtWaitTimerBits(iIdle),
			XMEMORY_RELEASE
		);
	}
	if ( xrtHttpCallRef(pCall) == NULL ) {
		pCause = xrtTakeError();
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			XERR_STATE,
			"schedule-http-retry",
			"HTTP retry timer could not retain the call",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	xrtAtomic32Store(
		&pCall->RetryTimerDone,
		0,
		XMEMORY_RELEASE
	);
	Id = xrtNetEngineAfter(
		pCall->Client->Engine,
		pCall->Affinity,
		pCall->RetryDelay,
		__xrtHttpRetryTimer,
		pCall
	);
	if ( Id == 0 ) {
		pCause = xrtTakeError();
		xrtAtomic32Store(
			&pCall->RetryTimerDone,
			1,
			XMEMORY_RELEASE
		);
		xrtHttpCallDestroy(pCall);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_RETRY,
			__xrtHttpClientCauseKind(pCause, XERR_MEMORY),
			"schedule-http-retry",
			"HTTP retry deadline could not be scheduled",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	xrtAtomic64Store(
		&pCall->RetryTimer,
		Id,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->RetryTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64Exchange(
			&pCall->RetryTimer,
			0,
			XMEMORY_ACQ_REL
		);
	} else if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)__xrtHttpRetryCancel(pCall);
	}
	return true;
}



/* 判断原因链是否表示收到任何响应字节前的意外 EOF。 */
static bool __xrtHttpRetryUnexpectedEof(const xerror* pError)
{
	while ( pError != NULL ) {
		cstr sDomain = xrtErrorDomain(pError);

		if ( (sDomain != NULL) &&
			(strcmp(sDomain, "xrt.http.exchange") == 0) &&
			(xrtErrorCode(pError) ==
			 XHTTP1_EXCHANGE_ERROR_UNEXPECTED_EOF) ) {
			return true;
		}
		pError = xrtErrorCause(pError);
	}
	return false;
}



/* 筛选可重试的临时传输错误，排除配置、协议、内存和取消失败。 */
static bool __xrtHttpRetryTransient(
	const xhttpcall* pCall,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
)
{
	if ( Code == XHTTP_CLIENT_ERROR_PROTOCOL ) {
		return (xrtAtomic64Load(
			&pCall->Info.ResponseWireBytes,
			XMEMORY_ACQUIRE
		) == pCall->RetryResponseWireStart) &&
			__xrtHttpRetryUnexpectedEof(pCause);
	}
	if ( (Code != XHTTP_CLIENT_ERROR_DIAL) &&
		(Code != XHTTP_CLIENT_ERROR_PROXY) &&
		(Code != XHTTP_CLIENT_ERROR_TLS) &&
		(Code != XHTTP_CLIENT_ERROR_TRANSPORT) ) {
		return false;
	}
	return (Kind == XERR_IO) || (Kind == XERR_AGAIN) ||
		(Kind == XERR_TIMEOUT) || (Kind == XERR_CLOSED);
}



/* 接管满足策略的失败尝试，并在退避前释放连接池配额。 */
bool __xrtHttpRetryFailure(
	xhttpcall* pCall,
	xnetresult Result,
	xhttpclienterror Code,
	xerrkind Kind,
	const xerror* pCause
)
{
	if ( (Result == XNET_RESULT_CANCELLED) ||
		(Result == XNET_RESULT_TIMEOUT) ||
		((pCall->RetryConfig.Flags & XHTTP_RETRY_TRANSPORT) == 0) ||
		!__xrtHttpRetryAvailable(pCall) ||
		!__xrtHttpRetryTransient(pCall, Code, Kind, pCause) ) {
		return false;
	}
	if ( !__xrtHttpRetryBackoffDelay(
		pCall,
		&pCall->RetryDelay
	) ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolFinish(pCall);
	#endif
	(void)__xrtHttpRetrySchedule(pCall);
	return true;
}



/* 从任意线程请求取消当前退避 Timer。 */
bool __xrtHttpRetryCancel(xhttpcall* pCall)
{
	uint64 Id;

	if ( pCall == NULL ) {
		return false;
	}
	Id = xrtAtomic64Exchange(
		&pCall->RetryTimer,
		0,
		XMEMORY_ACQ_REL
	);
	if ( (Id == 0) || xrtAtomic32Load(
		&pCall->RetryTimerDone,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	if ( !xrtNetEngineTimerCancelCurrent(
		pCall->Client->Engine,
		Id
	) && !xrtNetEngineTimerCancel(
		pCall->Client->Engine,
		Id
	) ) {
		xrtClearError();
		return false;
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_https.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 高层 HTTP/1 客户端只发布自己能够实际处理的 ALPN。 */
static const xstrview __xrtHttpClientProtocol = {
	"http/1.1",
	8
};



/* 使用默认 HTTP 策略和调用方提供的 TLS 对象创建 Client。 */
XHTTP_CLIENT_API xhttpclient* xrtHttpClientCreateTls(
	xnetengine* pEngine,
	const xtlscontext* pContext,
	const xtlsverifier* pVerifier
)
{
	xhttpclientconfig Config;

	xrtHttpClientConfigInit(&Config);
	Config.TlsContext = pContext;
	Config.TlsVerifier = pVerifier;
	Config.SystemTrust = (pVerifier == NULL);
	return xrtHttpClientCreate(pEngine, &Config);
}



/* 创建默认系统信任验证器。 */
static xtlsverifier* __xrtHttpClientSystemVerifier(void)
{
	xtlsverifierconfig Config;
	xx509store* pStore;
	xtlsverifier* pVerifier;

	pStore = xrtX509StoreSystem();
	if ( pStore == NULL ) {
		return NULL;
	}
	xrtTlsVerifierConfigInit(&Config);
	Config.Store = pStore;
	pVerifier = xrtTlsVerifierCreate(&Config);
	xrtX509StoreFree(pStore);
	return pVerifier;
}



/* 为 Client 建立或保留 TLS Context 与验证器。 */
bool __xrtHttpClientTlsInit(xhttpclient* pClient)
{
	if ( pClient->Config.TlsContext != NULL ) {
		pClient->TlsContext = xrtTlsContextRetain(
			pClient->Config.TlsContext
		);
	} else {
		pClient->TlsContext = xrtTlsContextCreate(NULL);
	}
	if ( pClient->TlsContext == NULL ) {
		return false;
	}
	if ( pClient->Config.TlsVerifier != NULL ) {
		pClient->TlsVerifier = xrtTlsVerifierRetain(
			pClient->Config.TlsVerifier
		);
	} else if ( pClient->Config.SystemTrust ) {
		pClient->TlsVerifier = __xrtHttpClientSystemVerifier();
	} else {
		xerror* pError = __xrtHttpClientErrorCreate(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-https-client",
			"HTTPS client requires system trust or an explicit TLS verifier",
			NULL
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	if ( pClient->TlsVerifier == NULL ) {
		xrtTlsContextRelease(pClient->TlsContext);
		pClient->TlsContext = NULL;
		return false;
	}
	pClient->Config.TlsContext = pClient->TlsContext;
	pClient->Config.TlsVerifier = pClient->TlsVerifier;
	return true;
}



/* 释放 Client 持有的 TLS 共享对象。 */
void __xrtHttpClientTlsUnit(xhttpclient* pClient)
{
	xrtTlsVerifierRelease(pClient->TlsVerifier);
	xrtTlsContextRelease(pClient->TlsContext);
	pClient->TlsVerifier = NULL;
	pClient->TlsContext = NULL;
	pClient->Config.TlsVerifier = NULL;
	pClient->Config.TlsContext = NULL;
}



/* 为直连和代理隧道生成完全一致的目标 TLS 身份配置。 */
void __xrtHttpCallTlsConfig(
	xhttpcall* pCall,
	xtlsclientconfig* pConfig
)
{
	xnetaddr Address;
	xstrview Host;

	xrtTlsClientConfigInit(pConfig);
	pConfig->Context = pCall->Client->TlsContext;
	Host.Data = pCall->Host;
	Host.Size = strlen(pCall->Host);
	pConfig->VerifyName = Host;
	if ( !xrtNetAddrParse(&Address, pCall->Host, 0) ) {
		xrtClearError();
		pConfig->ServerName = Host;
	}
	pConfig->Protocols = &__xrtHttpClientProtocol;
	pConfig->ProtocolCount = 1;
	pConfig->Verifier = pCall->Client->TlsVerifier;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		pConfig->Resume = __xrtHttpResumeTake(pCall);
	#endif
}



/* 从 Call 摘除 TLS Dial 的调用方引用。 */
static void __xrtHttpCallTlsDialDetach(
	xhttpcall* pCall,
	xtlsdial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->TlsDial == pDial ) {
		pCall->TlsDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* 验证服务端没有把 HTTP/1 客户端切换到不支持的 ALPN。 */
static bool __xrtHttpCallTlsProtocol(xtlsstream* pStream)
{
	xbytesview Protocol;

	if ( !xrtTlsSessionProtocol(
		xrtTlsStreamSession(pStream),
		&Protocol
	) ) {
		return true;
	}
	if ( (Protocol.Size == __xrtHttpClientProtocol.Size) &&
		(memcmp(
			Protocol.Data,
			__xrtHttpClientProtocol.Data,
			Protocol.Size
		) == 0) ) {
		return true;
	}
	{
		xerror* pError = __xrtHttpClientErrorCreate(
			XERR_PROTOCOL,
			XHTTP_CLIENT_ERROR_TLS,
			"negotiate-https",
			"HTTPS peer selected an unsupported ALPN protocol",
			NULL
		);

		xrtSetError(pError);
		xrtErrorFree(pError);
	}
	return false;
}



/* 在已验证 TLS Stream 上接管 Exchange 并发布活动低级 Call。 */
static bool __xrtHttpCallAttachTls(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	xhttp1callevents Events;
	xhttp1call* pStreamCall;

	__xrtHttpCallTransportReady(pCall);
	__xrtHttpCallStreamEvents(pCall, &Events);
	pStreamCall = xrtHttp1CallTls(
		pStream,
		pCall->Exchange,
		&pCall->Client->Config.Call,
		&Events
	);

	if ( pStreamCall == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->Exchange = NULL;
	pCall->StreamCall = pStreamCall;
	(void)xrtSpinUnlock(&pCall->Lock);
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_EXCHANGING,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtHttp1CallCancel(pStreamCall);
	}
	return true;
}



/* TLS Dial 成功后在安全 Stream 所属 Worker 上建立 HTTP/1 Call。 */
static void __xrtHttpCallTlsDone(
	xtlsdial* pDial,
	xnetresult Result,
	xtlsstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpCallTlsDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_TLS,
			Result == XNET_RESULT_CANCELLED ?
				XERR_CANCELLED :
				__xrtHttpClientCauseKind(
					pError,
					XERR_IO
				),
			"dial-https",
			Result == XNET_RESULT_CANCELLED ?
				"HTTPS dial was cancelled" :
				"HTTPS dial or TLS handshake failed",
			pError
		);
		xrtTlsDialDestroy(pDial);
		return;
	}
	__xrtHttpCallTlsConnected(
		pCall,
		pStream,
		"dial-https"
	);
	xrtTlsDialDestroy(pDial);
}



/* 接管一条已验证 TLS 传输并统一处理取消、ALPN、配额与 HTTP/1 附加。 */
void __xrtHttpCallTlsConnected(
	xhttpcall* pCall,
	xtlsstream* pStream,
	cstr sOperation
)
{
	xerror* pCause;

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)
		__xrtHttpPoolOpened(pCall);
	#endif
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			sOperation,
			"HTTP call was cancelled after TLS connected",
			NULL
		);
		return;
	}
	if ( !__xrtHttpCallTlsProtocol(pStream) ) {
		pCause = xrtTakeError();
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_TLS,
			XERR_PROTOCOL,
			"negotiate-https",
			"HTTPS ALPN negotiation is incompatible with HTTP/1",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
	if ( !__xrtHttpCallAttachTls(pCall, pStream) ) {
		pCause = xrtTakeError();
		(void)xrtTlsStreamAbort(pStream);
		xrtTlsStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_ERROR,
			XHTTP_CLIENT_ERROR_INTERNAL,
			__xrtHttpClientCauseKind(
				pCause,
				XERR_INTERNAL
			),
			"start-https-http1",
			"HTTP/1 call could not attach to the connected TLS stream",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

/* 在空闲 TLS Stream Worker 上重新附加 Exchange，失效连接原位重拨。 */
bool __xrtHttpCallStartPooledTls(xhttpcall* pCall)
{
	xtlsstream* pStream = pCall->PooledTls;

	if ( !__xrtHttp1TlsReusable(pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return __xrtHttpCallStartTls(pCall);
	}
	if ( !__xrtHttpCallAttachTls(pCall, pStream) ) {
		__xrtHttpPoolPooledStale(pCall);
		return false;
	}
	__xrtHttpPoolPooledUsed(pCall);
	return true;
}

#endif



/* 在目标 Worker 上创建 DNS、TCP 和 TLS 组合拨号。 */
bool __xrtHttpCallStartTls(xhttpcall* pCall)
{
	xtlsclientconfig Tls;
	xtlsdialconfig Config;
	xtlsstreamevents Events;
	xtlsdial* pDial;

	__xrtHttpCallTlsConfig(pCall, &Tls);
	xrtTlsDialConfigInit(&Config);
	Config.Transport = pCall->Client->Config.Dial;
	Config.Transport.Affinity = pCall->Affinity;
	Config.Stream = pCall->Client->Config.TlsStream;
	Config.Timeout = pCall->Timeout ==
		XHTTP_CLIENT_TIMEOUT_NONE ? 0 : pCall->Timeout;
	Config.ServerNameFromHost = false;
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtTlsDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Host,
		pCall->Port,
		&Tls,
		&Config,
		&Events,
		pCall,
		__xrtHttpCallTlsDone,
		pCall
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtTlsResumeRelease((xtlsresume*)Tls.Resume);
	#endif
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->TlsDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsDialCancel(pDial);
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_resume.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 每张票据用同一块内存保存恢复对象、HTTP 路由和 host 深拷贝。 */
struct __xrt_http_client_resume {
	__xrt_http_client_resume* Previous;
	__xrt_http_client_resume* Next;
	xtlsresume* Resume;
	xstrview Host;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
};



/* 设置恢复缓存公共 API 错误。 */
static void __xrtHttpResumeError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpClientSetError(
		Kind,
		Code,
		sOperation,
		sMessage,
		NULL
	);
}



/* 判断缓存项与当前 TLS 路由完全一致。 */
static bool __xrtHttpResumeMatch(
	const __xrt_http_client_resume* pEntry,
	const __xrt_http_resume_route* pRoute
)
{
	return (pEntry->Port == pRoute->Port) &&
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			(pEntry->Proxy == pRoute->Proxy) &&
		#endif
		xrtHttpHostEqual(
			pEntry->Host,
			pRoute->Host
		);
}



/* 从双向 LRU 摘除一张票据。 */
static void __xrtHttpResumeDetach(
	xhttpclient* pClient,
	__xrt_http_client_resume* pEntry
)
{
	if ( pEntry->Previous != NULL ) {
		pEntry->Previous->Next = pEntry->Next;
	} else {
		pClient->ResumeHead = pEntry->Next;
	}
	if ( pEntry->Next != NULL ) {
		pEntry->Next->Previous = pEntry->Previous;
	} else {
		pClient->ResumeTail = pEntry->Previous;
	}
	pEntry->Previous = NULL;
	pEntry->Next = NULL;
	pClient->ResumeCount--;
}



/* 释放缓存项持有的代理与票据引用。 */
static void __xrtHttpResumeFree(
	__xrt_http_client_resume* pEntry
)
{
	if ( pEntry == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pEntry->Proxy);
	#endif
	xrtTlsResumeRelease(pEntry->Resume);
	xrtFree(pEntry);
}



/* 淘汰指定项并累计原因。 */
static void __xrtHttpResumeRemove(
	xhttpclient* pClient,
	__xrt_http_client_resume* pEntry,
	bool bExpired
)
{
	__xrtHttpResumeDetach(pClient, pEntry);
	if ( bExpired ) {
		pClient->ResumeExpired++;
	} else {
		pClient->ResumeEvicted++;
	}
	__xrtHttpResumeFree(pEntry);
}



/* 在持锁状态清理已经失效的墙钟票据。 */
static void __xrtHttpResumePrune(
	xhttpclient* pClient,
	xtime iNow
)
{
	__xrt_http_client_resume* pEntry =
		pClient->ResumeHead;

	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		if ( !xrtTlsResumeValidAt(
			pEntry->Resume,
			iNow
		) ) {
			__xrtHttpResumeRemove(
				pClient,
				pEntry,
				true
			);
		}
		pEntry = pNext;
	}
}



/*
	把一张新票据加入有界 LRU。
	缓存键必须使用 HTTP 实际验证 host；IP 地址不会出现在 TLS SNI 中。
*/
void __xrtHttpResumeStore(
	const __xrt_http_resume_route* pRoute,
	xtlsresume* pResume
)
{
	xhttpclient* pClient = pRoute->Client;
	__xrt_http_client_resume* pEntry;
	__xrt_http_client_resume* pOldest = NULL;
	bytes pHost;
	size_t iOrigin = 0;

	if ( (pClient->Config.Resume.MaxEntries == 0) ||
		(pClient->Config.Resume.MaxEntriesPerOrigin == 0) ) {
		xrtTlsResumeRelease(pResume);
		return;
	}
	if ( !xrtTlsResumeValidAt(pResume, xrtNow()) ) {
		xrtClearError();
		xrtTlsResumeRelease(pResume);
		return;
	}
	if ( (pRoute->Host.Data == NULL) ||
		(pRoute->Host.Size == 0) ||
		(pRoute->Host.Size >
		 (SIZE_MAX - sizeof(*pEntry))) ) {
		xrtTlsResumeRelease(pResume);
		return;
	}
	pEntry = (__xrt_http_client_resume*)xrtCalloc(
		1,
		sizeof(*pEntry) + pRoute->Host.Size
	);
	if ( pEntry == NULL ) {
		xrtClearError();
		(void)xrtMutexLock(&pClient->ResumeLock);
		pClient->ResumeDropped++;
		(void)xrtMutexUnlock(&pClient->ResumeLock);
		xrtTlsResumeRelease(pResume);
		return;
	}
	pEntry->Resume = pResume;
	pHost = (bytes)(pEntry + 1);
	memcpy(pHost, pRoute->Host.Data, pRoute->Host.Size);
	pEntry->Host.Data = (cstr)pHost;
	pEntry->Host.Size = pRoute->Host.Size;
	pEntry->Port = pRoute->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pRoute->Proxy != NULL ) {
			pEntry->Proxy = xrtNetProxyRetain(
				pRoute->Proxy
			);
			if ( pEntry->Proxy == NULL ) {
				xrtClearError();
				(void)xrtMutexLock(
					&pClient->ResumeLock
				);
				pClient->ResumeDropped++;
				(void)xrtMutexUnlock(
					&pClient->ResumeLock
				);
				__xrtHttpResumeFree(pEntry);
				return;
			}
		}
	#endif

	(void)xrtMutexLock(&pClient->ResumeLock);
	__xrtHttpResumePrune(pClient, xrtNow());
	for (
		__xrt_http_client_resume* pCurrent =
			pClient->ResumeHead;
		pCurrent != NULL;
		pCurrent = pCurrent->Next
	) {
		if ( __xrtHttpResumeMatch(pCurrent, pRoute) ) {
			if ( pOldest == NULL ) {
				pOldest = pCurrent;
			}
			iOrigin++;
		}
	}
	if ( (iOrigin >=
		pClient->Config.Resume.MaxEntriesPerOrigin) &&
		(pOldest != NULL) ) {
		__xrtHttpResumeRemove(
			pClient,
			pOldest,
			false
		);
	}
	while ( pClient->ResumeCount >=
		pClient->Config.Resume.MaxEntries ) {
		__xrtHttpResumeRemove(
			pClient,
			pClient->ResumeHead,
			false
		);
	}
	pEntry->Previous = pClient->ResumeTail;
	if ( pClient->ResumeTail != NULL ) {
		pClient->ResumeTail->Next = pEntry;
	} else {
		pClient->ResumeHead = pEntry;
	}
	pClient->ResumeTail = pEntry;
	pClient->ResumeCount++;
	pClient->ResumeStored++;
	(void)xrtMutexUnlock(&pClient->ResumeLock);
}



/* 初始化 HTTP Client 的恢复缓存策略。 */
XHTTP_CLIENT_API void xrtHttpResumeConfigInit(
	xhttpresumeconfig* pConfig
)
{
	const xhttpresumeconfig Config = {
		XHTTP_RESUME_ENTRIES_DEFAULT,
		XHTTP_RESUME_ORIGIN_DEFAULT
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-resume-config",
			"HTTP resume config storage is invalid"
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 建立 Client 独享的恢复缓存锁。 */
bool __xrtHttpResumeInit(xhttpclient* pClient)
{
	if ( !xrtMutexInit(&pClient->ResumeLock) ) {
		return false;
	}
	pClient->ResumeReady = true;
	return true;
}



/* 最后一个 Client 引用释放全部未使用票据。 */
void __xrtHttpResumeUnit(xhttpclient* pClient)
{
	__xrt_http_client_resume* pEntry;

	if ( !pClient->ResumeReady ) {
		return;
	}
	pClient->ResumeReady = false;
	pEntry = pClient->ResumeHead;
	pClient->ResumeHead = NULL;
	pClient->ResumeTail = NULL;
	pClient->ResumeCount = 0;
	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		__xrtHttpResumeFree(pEntry);
		pEntry = pNext;
	}
	(void)xrtMutexUnit(&pClient->ResumeLock);
}



/* 取出最新的同路由有效票据，票据不会再次进入缓存。 */
xtlsresume* __xrtHttpResumeTake(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_resume_route Route;
	__xrt_http_client_resume* pEntry;
	xtlsresume* pResume = NULL;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pClient;
	Route.Host.Data = pCall->Host;
	Route.Host.Size = strlen(pCall->Host);
	Route.Port = pCall->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pCall->Proxy;
	#endif
	if ( (pClient->Config.Resume.MaxEntries == 0) ||
		(pClient->Config.Resume.MaxEntriesPerOrigin == 0) ) {
		return NULL;
	}
	(void)xrtMutexLock(&pClient->ResumeLock);
	__xrtHttpResumePrune(pClient, xrtNow());
	pEntry = pClient->ResumeTail;
	while ( pEntry != NULL ) {
		if ( __xrtHttpResumeMatch(pEntry, &Route) ) {
			__xrtHttpResumeDetach(
				pClient,
				pEntry
			);
			pResume = pEntry->Resume;
			pEntry->Resume = NULL;
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				xrtNetProxyRelease(pEntry->Proxy);
			#endif
			xrtFree(pEntry);
			pClient->ResumeHits++;
			break;
		}
		pEntry = pEntry->Previous;
	}
	if ( pResume == NULL ) {
		pClient->ResumeMisses++;
	}
	(void)xrtMutexUnlock(&pClient->ResumeLock);
	return pResume;
}



/* 把会话在 HTTP 终态前收到的新票据移入 Client 缓存。 */
void __xrtHttpResumeCollect(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	__xrt_http_resume_route Route;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pCall->Client;
	Route.Host.Data = pCall->Host;
	Route.Host.Size = strlen(pCall->Host);
	Route.Port = pCall->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pCall->Proxy;
	#endif
	__xrtHttpResumeCollectRoute(&Route, pStream);
}



/* 从给定 HTTP 路由的 TLS 会话接管全部待取票据。 */
void __xrtHttpResumeCollectRoute(
	const __xrt_http_resume_route* pRoute,
	xtlsstream* pStream
)
{
	xtlssession* pSession;

	if ( (pRoute == NULL) || (pStream == NULL) ) {
		return;
	}
	pSession = xrtTlsStreamSession(pStream);
	if ( pSession == NULL ) {
		xrtClearError();
		return;
	}
	while ( xrtTlsClientResumeCount(pSession) != 0 ) {
		xtlsresume* pResume =
			xrtTlsClientTakeResume(pSession);

		if ( pResume == NULL ) {
			xrtClearError();
			break;
		}
		__xrtHttpResumeStore(pRoute, pResume);
	}
}



/* 清空所有尚未使用的恢复票据。 */
XHTTP_CLIENT_API size_t xrtHttpClientResumeClear(
	xhttpclient* pClient
)
{
	__xrt_http_client_resume* pEntry;
	size_t iCleared;

	if ( pClient == NULL ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"clear-http-client-resume",
			"HTTP client is null"
		);
		return 0;
	}
	(void)xrtMutexLock(&pClient->ResumeLock);
	pEntry = pClient->ResumeHead;
	iCleared = pClient->ResumeCount;
	pClient->ResumeHead = NULL;
	pClient->ResumeTail = NULL;
	pClient->ResumeCount = 0;
	(void)xrtMutexUnlock(&pClient->ResumeLock);
	while ( pEntry != NULL ) {
		__xrt_http_client_resume* pNext =
			pEntry->Next;

		__xrtHttpResumeFree(pEntry);
		pEntry = pNext;
	}
	return iCleared;
}



/* 读取恢复缓存和累计计数的一致快照。 */
XHTTP_CLIENT_API bool xrtHttpClientResumeStats(
	const xhttpclient* pClient,
	xhttpresumestats* pStats
)
{
	xhttpclient* pMutable = (xhttpclient*)pClient;
	xhttpresumestats Stats;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pStats, sizeof(Stats)) ) {
		__xrtHttpResumeError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"query-http-client-resume",
			"HTTP client and complete resume stats storage are required"
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	(void)xrtMutexLock(&pMutable->ResumeLock);
	__xrtHttpResumePrune(pMutable, xrtNow());
	Stats.Entries = pClient->ResumeCount;
	Stats.Hits = pClient->ResumeHits;
	Stats.Misses = pClient->ResumeMisses;
	Stats.Stored = pClient->ResumeStored;
	Stats.Evicted = pClient->ResumeEvicted;
	Stats.Expired = pClient->ResumeExpired;
	Stats.Dropped = pClient->ResumeDropped;
	(void)xrtMutexUnlock(&pMutable->ResumeLock);
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_easy.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY)

/* 发布便利入口的统一参数错误，保持高层 Client 错误域不变。 */
bool __xrtHttpClientEasyCheck(
	xhttpclient* pClient,
	bool bCompletion,
	cstr sOperation
)
{
	if ( (pClient != NULL) && bCompletion ) {
		return true;
	}
	__xrtHttpClientSetError(
		XERR_ARGUMENT,
		XHTTP_CLIENT_ERROR_ARGUMENT,
		sOperation,
		"HTTP client and required completion callback must not be null",
		NULL
	);
	return false;
}



/* 复用公开请求构建器创建一次提交快照，不引入第二套正文表示。 */
xhttprequest* __xrtHttpClientEasyRequest(
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(Method, Url);

	if ( pRequest == NULL ) {
		return NULL;
	}
	if ( (pBody != NULL) && !xrtHttpRequestSetBytes(
		pRequest,
		*pBody,
		ContentType
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* 创建临时请求并把其不可变快照交给现有 callback 执行层。 */
static xhttpcall* __xrtHttpClientEasyDo(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	xhttprequest* pRequest;
	xhttpcall* pCall;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		pDone != NULL,
		"submit-http-easy"
	) ) {
		return NULL;
	}
	pRequest = __xrtHttpClientEasyRequest(
		Method,
		Url,
		pBody,
		ContentType
	);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pCall = xrtHttpClientDo(
		pClient,
		pRequest,
		pOptions,
		pDone,
		pData
	);
	xrtHttpRequestDestroy(pRequest);
	return pCall;
}



/* 提交无正文 GET。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientGet(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions,
		pDone,
		pData
	);
}



/* 提交拥有固定字节正文的 POST。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientPost(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions,
		pDone,
		pData
	);
}



/* 提交拥有固定字节正文的任意方法请求。 */
XHTTP_CLIENT_API xhttpcall* xrtHttpClientSendBytes(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions,
	xhttpcallproc pDone,
	ptr pData
)
{
	return __xrtHttpClientEasyDo(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions,
		pDone,
		pData
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_future.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_FUTURE)

/* Client 关闭等待节点独立持有 Client 和 Promise。 */
struct __xrt_http_client_wait {
	__xrt_http_client_wait* Next;
	xhttpclient* Client;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* 从 Client 等待链表中移除指定节点；调用方持有生命周期锁。 */
static bool __xrtHttpClientWaitRemove(
	xhttpclient* pClient,
	__xrt_http_client_wait* pWaiter
)
{
	__xrt_http_client_wait** ppCurrent =
		&pClient->CloseWaitHead;
	__xrt_http_client_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) &&
		(*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pClient->CloseWaitTail == pWaiter ) {
		pClient->CloseWaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	return true;
}



/* 完成一个已摘除的 Client 关闭等待并释放全部持有资源。 */
static void __xrtHttpClientWaitFinish(
	__xrt_http_client_wait* pWaiter,
	bool bCancelled
)
{
	xpromise* pPromise = pWaiter->Promise;
	xhttpclient* pClient = pWaiter->Client;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	__xrtHttpClientRelease(pClient);
	xrtFree(pWaiter);
	if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseResolve(pPromise, NULL);
	}
	xrtPromiseDestroy(pPromise);
}



/* 取消只摘除当前关闭等待，不排空或中止 Client。 */
static void __xrtHttpClientWaitCancel(ptr pData)
{
	__xrt_http_client_wait* pWaiter =
		(__xrt_http_client_wait*)pData;
	xhttpclient* pClient = pWaiter->Client;
	bool bRemoved;

	xrtSpinLock(&pClient->LifecycleLock);
	bRemoved = pWaiter->Linked &&
		__xrtHttpClientWaitRemove(pClient, pWaiter);
	xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bRemoved ) {
		__xrtHttpClientWaitFinish(pWaiter, true);
	}
}



/* 摘除并成功完成 Client 的全部关闭等待。 */
void __xrtHttpClientFutureFinish(xhttpclient* pClient)
{
	__xrt_http_client_wait* pWaiter;

	if ( pClient == NULL ) {
		return;
	}
	xrtSpinLock(&pClient->LifecycleLock);
	pWaiter = pClient->CloseWaitHead;
	pClient->CloseWaitHead = NULL;
	pClient->CloseWaitTail = NULL;
	for ( __xrt_http_client_wait* pCurrent = pWaiter;
		pCurrent != NULL;
		pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
	}
	xrtSpinUnlock(&pClient->LifecycleLock);
	while ( pWaiter != NULL ) {
		__xrt_http_client_wait* pNext = pWaiter->Next;

		pWaiter->Next = NULL;
		__xrtHttpClientWaitFinish(pWaiter, false);
		pWaiter = pNext;
	}
}



/* 建立一个可独立取消的 Client 关闭 Future。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientWaitAsync(xhttpclient* pClient)
{
	__xrt_http_client_wait* pWaiter;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	bool bClosed;

	if ( pClient == NULL ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"wait-http-client",
			"HTTP client is null",
			NULL
		);
		return NULL;
	}
	pWaiter = (__xrt_http_client_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Client = __xrtHttpClientHold(pClient);
	if ( pWaiter->Client == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtHttpClientRelease(pWaiter->Client);
		xrtFree(pWaiter);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtHttpClientWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		__xrtHttpClientRelease(pWaiter->Client);
		xrtFree(pWaiter);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	xrtSpinLock(&pClient->LifecycleLock);
	bClosed = xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	) == XHTTP_CLIENT_CLOSED;
	if ( !bClosed ) {
		pWaiter->Linked = true;
		if ( pClient->CloseWaitTail != NULL ) {
			pClient->CloseWaitTail->Next = pWaiter;
		} else {
			pClient->CloseWaitHead = pWaiter;
		}
		pClient->CloseWaitTail = pWaiter;
	}
	xrtSpinUnlock(&pClient->LifecycleLock);
	if ( bClosed ) {
		__xrtHttpClientWaitFinish(pWaiter, false);
	}
	return pFuture;
}



/* Future 成功值独立拥有响应、Upgrade 传输和只读元数据。 */
struct xhttpresult {
	volatile int32 References;
	xatomicptr Response;
	xatomicptr Tcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xatomicptr Tls;
	#endif
	xhttpcallinfo Info;
	size_t Buffered;
	bool Upgraded;
};



/*
	桥接上下文先服务于 Call，成功完成后原位转为 Future 拥有的结果。
	Result 必须保持首成员，结果析构才能直接释放整块内存。
*/
typedef struct xrt_http_client_future {
	xhttpresult Result;
	xfuturebridge Bridge;
	xhttpcall* Call;
} xrt_http_client_future;



/* 中止并释放一个未被调用方取走的明文 Upgrade 传输。 */
static void __xrtHttpResultTcpDestroy(xnetstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	(void)xrtNetStreamAbort(pStream);
	xrtNetStreamDestroy(pStream);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 中止并释放一个未被调用方取走的 TLS Upgrade 传输。 */
static void __xrtHttpResultTlsDestroy(xtlsstream* pStream)
{
	if ( pStream == NULL ) {
		return;
	}
	(void)xrtTlsStreamAbort(pStream);
	xrtTlsStreamDestroy(pStream);
}

#endif



/* 增加结果引用并返回原指针。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpResultRef(xhttpresult* pResult)
{
	if ( (pResult == NULL) ||
		(xrtRefRetain(&pResult->References) < 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return pResult;
}



/* 释放最后一个结果引用及其尚未取走的所有权。 */
XHTTP_CLIENT_API void xrtHttpResultDestroy(xhttpresult* pResult)
{
	xhttpresponse* pResponse;
	xnetstream* pTcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls;
	#endif

	if ( (pResult == NULL) ||
		(xrtRefRelease(&pResult->References) != 0) ) {
		return;
	}
	pResponse = (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
	pTcp = (xnetstream*)xrtAtomicPtrExchange(
		&pResult->Tcp,
		NULL,
		XMEMORY_ACQ_REL
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pTls = (xtlsstream*)xrtAtomicPtrExchange(
			&pResult->Tls,
			NULL,
			XMEMORY_ACQ_REL
		);
	#endif
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpResultTcpDestroy(pTcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(pTls);
	#endif
	memset(pResult, 0, sizeof(*pResult));
	xrtFree(pResult);
}



/* 返回结果借用的响应。 */
XHTTP_CLIENT_API const xhttpresponse* xrtHttpResultResponse(
	const xhttpresult* pResult
)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (const xhttpresponse*)xrtAtomicPtrLoad(
		&pResult->Response,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的响应。 */
XHTTP_CLIENT_API xhttpresponse* xrtHttpResultTakeResponse(
	xhttpresult* pResult
)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xhttpresponse*)xrtAtomicPtrExchange(
		&pResult->Response,
		NULL,
		XMEMORY_ACQ_REL
	);
}



/* 返回结果借用的明文 Upgrade Stream。 */
XHTTP_CLIENT_API xnetstream* xrtHttpResultTcp(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrLoad(
		&pResult->Tcp,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的明文 Upgrade Stream。 */
XHTTP_CLIENT_API xnetstream* xrtHttpResultTakeTcp(xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xnetstream*)xrtAtomicPtrExchange(
		&pResult->Tcp,
		NULL,
		XMEMORY_ACQ_REL
	);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 返回结果借用的 TLS Upgrade Stream。 */
XHTTP_CLIENT_API xtlsstream* xrtHttpResultTls(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pResult->Tls,
		XMEMORY_ACQUIRE
	);
}



/* 原子取走结果拥有的 TLS Upgrade Stream。 */
XHTTP_CLIENT_API xtlsstream* xrtHttpResultTakeTls(xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrExchange(
		&pResult->Tls,
		NULL,
		XMEMORY_ACQ_REL
	);
}

#endif



/* 返回 Upgrade 后已经缓冲的协议外字节数。 */
XHTTP_CLIENT_API size_t xrtHttpResultBuffered(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	return pResult->Buffered;
}



/* 返回已经跟随的重定向次数。 */
XHTTP_CLIENT_API size_t xrtHttpResultRedirects(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	return pResult->Info.Redirects;
}



/* 判断结果是否已经交付 Upgrade 传输。 */
XHTTP_CLIENT_API bool xrtHttpResultUpgraded(const xhttpresult* pResult)
{
	if ( pResult == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return pResult->Upgraded;
}



/* 复制成功结果携带的不可变 Call 诊断快照。 */
XHTTP_CLIENT_API bool xrtHttpResultInfo(
	const xhttpresult* pResult,
	xhttpcallinfo* pInfo
)
{
	if ( (pResult == NULL) ||
		!xrtMemRangeValid(pInfo, sizeof(pResult->Info)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pInfo, &pResult->Info, sizeof(pResult->Info));
	return true;
}



/* Future 拥有值的析构过程只释放其一个结果引用。 */
static void __xrtHttpFutureResultFree(ptr pValue, ptr pData)
{
	(void)pData;
	xrtHttpResultDestroy((xhttpresult*)pValue);
}



/* 把 Future 的协作取消请求转发给完整 HTTP Call。 */
static void __xrtHttpFutureCancel(ptr pData)
{
	xrt_http_client_future* pContext =
		(xrt_http_client_future*)pData;

	(void)xrtHttpCallCancel(pContext->Call);
}



/*
	等待提交线程安装取消观察器。
	这只覆盖回调与非 Worker 提交线程并发完成的极短窗口。
*/
/* 把成功回调转为 Future 拥有的完整结果。 */
static void __xrtHttpFutureResolve(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	bool bResolved;

	xrtAtomicPtrStore(
		&pContext->Result.Response,
		pResult->Response,
		XMEMORY_RELEASE
	);
	xrtAtomicPtrStore(
		&pContext->Result.Tcp,
		pResult->Tcp,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtAtomicPtrStore(
			&pContext->Result.Tls,
			pResult->Tls,
			XMEMORY_RELEASE
		);
	#endif
	pContext->Result.Info = pResult->Info;
	pContext->Result.Buffered = pResult->Buffered;
	pContext->Result.Upgraded = pResult->Upgraded;
	xrtHttpCallDestroy(pCall);
	bResolved = xrtPromiseResolveOwned(
		pPromise,
		&pContext->Result,
		__xrtHttpFutureResultFree,
		NULL
	);
	xrtPromiseDestroy(pPromise);
	if ( !bResolved ) {
		xrtHttpResultDestroy(&pContext->Result);
	}
}



/* 把失败或取消回调转为对应 Future 终态并释放桥接上下文。 */
static void __xrtHttpFutureReject(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xpromise* pPromise = xrtFutureBridgePromise(&pContext->Bridge);
	xhttpcall* pCall = pContext->Call;
	xerror* pError = xrtErrorRef(pResult->Error);
	xnetresult Result = pResult->Result;

	xrtHttpResponseDestroy(pResult->Response);
	__xrtHttpResultTcpDestroy(pResult->Tcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(pResult->Tls);
	#endif
	xrtHttpCallDestroy(pCall);
	xrtFree(pContext);

	if ( Result == XNET_RESULT_CANCELLED ) {
		(void)xrtPromiseCancel(pPromise);
	} else if ( pError != NULL ) {
		(void)xrtPromiseReject(pPromise, pError);
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	xrtErrorFree(pError);
	xrtPromiseDestroy(pPromise);
}



/* 回收畸形成功结果中的全部所有权，并把契约破坏转为结构化内部错误。 */
static void __xrtHttpFutureRejectInvalidSuccess(
	xrt_http_client_future* pContext,
	const xhttpcallresult* pResult
)
{
	xhttpcallresult Reject = *pResult;
	xerror* pError;

	xrtHttpResponseDestroy(Reject.Response);
	__xrtHttpResultTcpDestroy(Reject.Tcp);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		__xrtHttpResultTlsDestroy(Reject.Tls);
		Reject.Tls = NULL;
	#endif
	Reject.Result = XNET_RESULT_ERROR;
	Reject.Response = NULL;
	Reject.Tcp = NULL;
	pError = __xrtHttpClientErrorCreate(
		XERR_INTERNAL,
		XHTTP_CLIENT_ERROR_INTERNAL,
		"complete-http-future",
		"HTTP call reported success without a response",
		NULL
	);
	Reject.Error = pError != NULL ? pError : xrtGetError();
	__xrtHttpFutureReject(pContext, &Reject);
	if ( pError != NULL ) {
		xrtErrorFree(pError);
	}
}



/* 接收 Call 唯一终态并完成 Future。 */
static void __xrtHttpFutureDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	xrt_http_client_future* pContext =
		(xrt_http_client_future*)pData;
	bool bReady;

	(void)pCall;
	bReady = xrtFutureBridgeWait(&pContext->Bridge);
	xrtFutureBridgeUnwatch(&pContext->Bridge);
	if ( !bReady ) {
		xrtHttpResponseDestroy(pResult->Response);
		__xrtHttpResultTcpDestroy(pResult->Tcp);
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			__xrtHttpResultTlsDestroy(pResult->Tls);
		#endif
		xrtHttpCallDestroy(pContext->Call);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return;
	}
	if ( (pResult->Result == XNET_RESULT_OK) &&
		(pResult->Response != NULL) ) {
		__xrtHttpFutureResolve(pContext, pResult);
	} else if ( pResult->Result == XNET_RESULT_OK ) {
		__xrtHttpFutureRejectInvalidSuccess(pContext, pResult);
	} else {
		__xrtHttpFutureReject(pContext, pResult);
	}
}



/* 创建 Promise、Call 和双向取消桥。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientDoAsync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
)
{
	xhttpcalloptions Options;
	xrt_http_client_future* pContext;
	const xhttpcalloptions* pCallOptions = NULL;
	xcancel* pParent = NULL;
	xfuture* pFuture;
	xerror* pError;

	if ( pOptions != NULL ) {
		if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
			__xrtHttpClientSetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_ARGUMENT,
				"run-http-future",
				"HTTP call options range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Options, pOptions, sizeof(Options));
		pCallOptions = &Options;
		pParent = Options.Cancel;
	}
	pContext = (xrt_http_client_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Result.References = 1;
	xrtAtomicPtrInit(&pContext->Result.Response, NULL);
	xrtAtomicPtrInit(&pContext->Result.Tcp, NULL);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xrtAtomicPtrInit(&pContext->Result.Tls, NULL);
	#endif
	pFuture = xrtFutureBridgeCreate(
		&pContext->Bridge,
		pParent
	);
	if ( pFuture == NULL ) {
		xrtFree(pContext);
		return NULL;
	}
	pContext->Call = xrtHttpClientDo(
		pClient,
		pRequest,
		pCallOptions,
		__xrtHttpFutureDone,
		pContext
	);
	if ( pContext->Call == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(xrtFutureBridgePromise(&pContext->Bridge));
		xrtFree(pContext);
		return NULL;
	}
	if ( !xrtFutureBridgeWatch(
		&pContext->Bridge,
		__xrtHttpFutureCancel,
		pContext
	) ) {
		pError = xrtTakeError();
		(void)xrtFutureBridgeFail(&pContext->Bridge);
		(void)xrtHttpCallCancel(pContext->Call);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	(void)xrtFutureBridgeReady(&pContext->Bridge);
	return pFuture;
}



/* 设置同步等待层自己的稳定 HTTP Client 错误。 */
static void __xrtHttpSyncError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		"wait-http-call",
		sMessage,
		NULL
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 在宿主线程阻塞等待同一 Future 契约，并保留一个结果引用。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientDoSync(
	xhttpclient* pClient,
	const xhttprequest* pRequest,
	const xhttpcalloptions* pOptions
)
{
	xfuturestate State;
	xfuture* pFuture;
	xhttpresult* pResult = NULL;

	if ( pClient == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( xrtNetEngineCurrent(pClient->Engine) != NULL ) {
		__xrtHttpSyncError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_INTERNAL,
			"network Worker cannot block on its own HTTP call"
		);
		return NULL;
	}
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		pOptions
	);
	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
		xerror* pError = xrtTakeError();

		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	State = xrtFutureState(pFuture);
	if ( State == XFUTURE_RESOLVED ) {
		pResult = xrtHttpResultRef(
			(xhttpresult*)xrtFutureValue(pFuture)
		);
	} else if ( State == XFUTURE_FAILED ) {
		(void)xrtFutureValue(pFuture);
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtHttpSyncError(
			XERR_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			"HTTP call was cancelled while waiting"
		);
	} else {
		__xrtHttpSyncError(
			XERR_INTERNAL,
			XHTTP_CLIENT_ERROR_INTERNAL,
			"HTTP Future completed without a usable result"
		);
	}
	xrtFutureDestroy(pFuture);
	return pResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_easy_future.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_EASY_FUTURE)

/* 创建临时请求并把其不可变快照交给现有 Future 执行层。 */
static xfuture* __xrtHttpClientEasyAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	xhttprequest* pRequest;
	xfuture* pFuture;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		true,
		"submit-http-easy-future"
	) ) {
		return NULL;
	}
	pRequest = __xrtHttpClientEasyRequest(
		Method,
		Url,
		pBody,
		ContentType
	);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pFuture = xrtHttpClientDoAsync(
		pClient,
		pRequest,
		pOptions
	);
	xrtHttpRequestDestroy(pRequest);
	return pFuture;
}



/* 创建临时请求并在宿主线程复用现有同步等待层。 */
static xhttpresult* __xrtHttpClientEasySync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	const xbytesview* pBody,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	xhttprequest* pRequest;
	xhttpresult* pResult;

	if ( !__xrtHttpClientEasyCheck(
		pClient,
		true,
		"run-http-easy-sync"
	) ) {
		return NULL;
	}
	pRequest = __xrtHttpClientEasyRequest(
		Method,
		Url,
		pBody,
		ContentType
	);
	if ( pRequest == NULL ) {
		return NULL;
	}
	pResult = xrtHttpClientDoSync(
		pClient,
		pRequest,
		pOptions
	);
	xrtHttpRequestDestroy(pRequest);
	return pResult;
}



/* 异步提交无正文 GET。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientGetAsync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions
	);
}



/* 异步提交固定正文 POST。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientPostAsync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 异步提交任意方法的固定正文请求。 */
XHTTP_CLIENT_API xfuture* xrtHttpClientSendBytesAsync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasyAsync(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 同步执行无正文 GET。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientGetSync(
	xhttpclient* pClient,
	xstrview Url,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		XRT_STR_LITERAL("GET"),
		Url,
		NULL,
		(xstrview){ NULL, 0 },
		pOptions
	);
}



/* 同步执行固定正文 POST。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientPostSync(
	xhttpclient* pClient,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		XRT_STR_LITERAL("POST"),
		Url,
		&Body,
		ContentType,
		pOptions
	);
}



/* 同步执行任意方法的固定正文请求。 */
XHTTP_CLIENT_API xhttpresult* xrtHttpClientSendBytesSync(
	xhttpclient* pClient,
	xstrview Method,
	xstrview Url,
	xbytesview Body,
	xstrview ContentType,
	const xhttpcalloptions* pOptions
)
{
	return __xrtHttpClientEasySync(
		pClient,
		Method,
		Url,
		&Body,
		ContentType,
		pOptions
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_proxy.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)

/* 设置代理策略入口的客户端域错误，并保留底层代理或内存根因。 */
static void __xrtHttpProxySetError(
	xerrkind Kind,
	xhttpclienterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 从完整原因链中提取对调用方最有价值的代理失败类别。 */
static xerrkind __xrtHttpProxyCauseKind(
	const xerror* pCause,
	xnetresult Result
)
{
	if ( Result == XNET_RESULT_CANCELLED ) {
		return XERR_CANCELLED;
	}
	if ( xrtErrorIs(pCause, XERR_MEMORY) != NULL ) {
		return XERR_MEMORY;
	}
	if ( xrtErrorIs(pCause, XERR_TIMEOUT) != NULL ) {
		return XERR_TIMEOUT;
	}
	if ( xrtErrorIs(pCause, XERR_PERMISSION) != NULL ) {
		return XERR_PERMISSION;
	}
	if ( xrtErrorIs(pCause, XERR_PROTOCOL) != NULL ) {
		return XERR_PROTOCOL;
	}
	if ( xrtErrorIs(pCause, XERR_UNSUPPORTED) != NULL ) {
		return XERR_UNSUPPORTED;
	}
	return XERR_IO;
}



/* 初始化为继承 Client 默认代理。 */
XHTTP_CLIENT_API void xrtHttpProxyOptionsInit(
	xhttpproxyoptions* pOptions
)
{
	const xhttpproxyoptions Options = {
		XHTTP_PROXY_DEFAULT,
		NULL
	};

	if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
		__xrtHttpProxySetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-proxy-options",
			"HTTP proxy options storage is invalid",
			NULL
		);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 冻结本次调用的代理选择并取得独立引用。 */
bool __xrtHttpProxyInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	const xnetproxy* pProxy = NULL;

	if ( (pCall == NULL) || (pOptions == NULL) ) {
		__xrtHttpProxySetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"prepare-http-proxy",
			"HTTP call or proxy options are null",
			NULL
		);
		return false;
	}
	switch ( pOptions->Proxy.Mode ) {
		case XHTTP_PROXY_DEFAULT:
			if ( pOptions->Proxy.Proxy != NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"DEFAULT proxy mode does not accept an explicit proxy",
					NULL
				);
				return false;
			}
			pProxy = pCall->Client->Proxy;
			break;
		case XHTTP_PROXY_DIRECT:
			if ( pOptions->Proxy.Proxy != NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"DIRECT proxy mode requires a null proxy",
					NULL
				);
				return false;
			}
			break;
		case XHTTP_PROXY_EXPLICIT:
			if ( pOptions->Proxy.Proxy == NULL ) {
				__xrtHttpProxySetError(
					XERR_ARGUMENT,
					XHTTP_CLIENT_ERROR_CONFIG,
					"prepare-http-proxy",
					"EXPLICIT proxy mode requires a proxy",
					NULL
				);
				return false;
			}
			pProxy = pOptions->Proxy.Proxy;
			break;
		default:
			__xrtHttpProxySetError(
				XERR_ARGUMENT,
				XHTTP_CLIENT_ERROR_CONFIG,
				"prepare-http-proxy",
				"HTTP proxy mode is invalid",
				NULL
			);
			return false;
	}
	if ( pProxy != NULL ) {
		pCall->Proxy = xrtNetProxyRetain(pProxy);
		if ( pCall->Proxy == NULL ) {
			xerror* pCause = xrtTakeError();

			__xrtHttpProxySetError(
				xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
					XERR_MEMORY : XERR_STATE,
				XHTTP_CLIENT_ERROR_PROXY,
				"prepare-http-proxy",
				"HTTP call could not retain its proxy",
				pCause
			);
			xrtErrorFree(pCause);
			return false;
		}
	}
	return true;
}



/* 从 Call 摘除已经完成的 Proxy Dial 调用方引用。 */
static void __xrtHttpProxyDialDetach(
	xhttpcall* pCall,
	xnetproxydial* pDial
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->ProxyDial == pDial ) {
		pCall->ProxyDial = NULL;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

/* 从 Call 摘除代理后的 TLS 握手引用，并记录同步回调窗口。 */
static void __xrtHttpProxyTlsDetach(
	xhttpcall* pCall,
	xtlsstream* pStream
)
{
	(void)xrtSpinLock(&pCall->Lock);
	if ( pCall->ProxyTls == pStream ) {
		pCall->ProxyTls = NULL;
	}
	if ( pCall->ProxyTlsStarting ) {
		pCall->ProxyTlsCallback = true;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
}



/* TLS 握手完成后沿用直连 HTTPS 的 ALPN、取消和池接管路径。 */
static void __xrtHttpProxyTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	__xrtHttpProxyTlsDetach(pCall, pStream);
	__xrtHttpCallTlsConnected(
		pCall,
		pStream,
		"dial-https-proxy"
	);
}



/* 代理隧道中的 TLS 在 Open 前关闭时发布唯一失败终态。 */
static void __xrtHttpProxyTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	bool bCancelled = (Result == XNET_RESULT_CANCELLED) ||
		(xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) != 0);
	xnetresult FailureResult = bCancelled ?
		XNET_RESULT_CANCELLED :
		(Result == XNET_RESULT_OK ?
			XNET_RESULT_ERROR : Result);

	__xrtHttpProxyTlsDetach(pCall, pStream);
	__xrtHttpCallFail(
		pCall,
		FailureResult,
		bCancelled ?
			XHTTP_CLIENT_ERROR_CANCELLED :
			XHTTP_CLIENT_ERROR_TLS,
		__xrtHttpProxyCauseKind(
			pError,
			FailureResult
		),
		"dial-https-proxy",
		bCancelled ?
			"HTTPS proxy call was cancelled during TLS handshake" :
			"TLS handshake inside the HTTP proxy tunnel failed",
		pError
	);
	xrtTlsStreamDestroy(pStream);
}



/* 代理后的 TLS Stream 只在握手阶段使用这组事件。 */
static const xtlsstreamevents __xrtHttpProxyTlsEvents = {
	.Open = __xrtHttpProxyTlsOpen,
	.Read = NULL,
	.End = NULL,
	.Writable = NULL,
	.Drain = NULL,
	.Close = __xrtHttpProxyTlsClose,
	.Ticket = NULL
};



/* 在已经建立的代理隧道内启动目标 TLS 客户端。 */
static bool __xrtHttpProxyStartTls(
	xhttpcall* pCall,
	xnetstream* pTransport
)
{
	xtlsclientconfig Tls;
	xtlsstream* pTls = NULL;
	bool bCallback;
	bool bStarted;

	/* TLS Attach 可以同步发布终态，短暂引用保护返回后的状态收口。 */
	if ( xrtHttpCallRef(pCall) == NULL ) {
		return false;
	}
	__xrtHttpCallTlsConfig(pCall, &Tls);
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_TLS
	);
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_HANDSHAKING,
		XMEMORY_RELEASE
	);
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyTlsStarting = true;
	pCall->ProxyTlsCallback = false;
	(void)xrtSpinUnlock(&pCall->Lock);
	bStarted = xrtTlsStreamClient(
		pTransport,
		&Tls,
		&pCall->Client->Config.TlsStream,
		&__xrtHttpProxyTlsEvents,
		pCall,
		&pTls
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
		xrtTlsResumeRelease((xtlsresume*)Tls.Resume);
	#endif
	if ( !bStarted ) {
		(void)xrtSpinLock(&pCall->Lock);
		pCall->ProxyTlsStarting = false;
		pCall->ProxyTlsCallback = false;
		(void)xrtSpinUnlock(&pCall->Lock);
		xrtHttpCallDestroy(pCall);
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyTlsStarting = false;
	bCallback = pCall->ProxyTlsCallback;
	pCall->ProxyTlsCallback = false;
	if ( !bCallback ) {
		pCall->ProxyTls = pTls;
	}
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( !bCallback && xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtTlsStreamAbort(pTls);
	}
	xrtHttpCallDestroy(pCall);
	return true;
}

#endif



/* Proxy Dial 完成后进入普通 HTTP 或代理隧道内的 TLS。 */
static void __xrtHttpProxyDialDone(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xerror* pCause;
	#endif

	__xrtHttpProxyDialDetach(pCall, pDial);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpCallFail(
			pCall,
			Result,
			Result == XNET_RESULT_CANCELLED ?
				XHTTP_CLIENT_ERROR_CANCELLED :
				XHTTP_CLIENT_ERROR_PROXY,
			__xrtHttpProxyCauseKind(pError, Result),
			"dial-http-proxy",
			Result == XNET_RESULT_CANCELLED ?
				"HTTP proxy dial was cancelled" :
				"HTTP proxy tunnel could not be established",
			pError
		);
		xrtNetProxyDialDestroy(pDial);
		return;
	}
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetStreamAbort(pStream);
		xrtNetStreamDestroy(pStream);
		__xrtHttpCallFail(
			pCall,
			XNET_RESULT_CANCELLED,
			XHTTP_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"dial-http-proxy",
			"HTTP call was cancelled after the proxy tunnel opened",
			NULL
		);
		xrtNetProxyDialDestroy(pDial);
		return;
	}
	if ( pCall->Secure ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( !__xrtHttpProxyStartTls(
				pCall,
				pStream
			) ) {
				pCause = xrtTakeError();
				(void)xrtNetStreamAbort(pStream);
				xrtNetStreamDestroy(pStream);
				__xrtHttpCallFail(
					pCall,
					XNET_RESULT_ERROR,
					XHTTP_CLIENT_ERROR_TLS,
					xrtErrorIs(
						pCause,
						XERR_MEMORY
					) != NULL ?
						XERR_MEMORY : XERR_IO,
					"dial-https-proxy",
					"TLS could not start inside the HTTP proxy tunnel",
					pCause
				);
				xrtErrorFree(pCause);
			}
		#else
			(void)xrtNetStreamAbort(pStream);
			xrtNetStreamDestroy(pStream);
			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_TLS,
				XERR_UNSUPPORTED,
				"dial-https-proxy",
				"HTTPS support is not present in this build",
				NULL
			);
		#endif
	} else {
		__xrtHttpCallTcpConnected(
			pCall,
			pStream,
			"dial-http-proxy"
		);
	}
	xrtNetProxyDialDestroy(pDial);
}



/* 连接代理端点并为当前目标建立 CONNECT 或 SOCKS5 隧道。 */
bool __xrtHttpCallStartProxy(xhttpcall* pCall)
{
	xnetproxydialconfig Config;
	xnetstreamevents Events;
	xnetproxydial* pDial;

	xrtNetProxyDialConfigInit(&Config);
	Config.Transport = pCall->Client->Config.Dial;
	Config.Transport.Affinity = pCall->Affinity;
	Config.Timeout = 0;
	if ( Config.ReceiveLimit >
		Config.Transport.Stream.ReadLimit ) {
		Config.ReceiveLimit =
			Config.Transport.Stream.ReadLimit;
	}
	memset(&Events, 0, sizeof(Events));
	xrtAtomic32Store(
		&pCall->State,
		XHTTP_CALL_DIALING,
		XMEMORY_RELEASE
	);
	pDial = xrtNetProxyDial(
		pCall->Client->Engine,
		pCall->Client->Resolver,
		pCall->Proxy,
		pCall->Host,
		pCall->Port,
		&Config,
		&Events,
		pCall,
		__xrtHttpProxyDialDone,
		pCall
	);
	if ( pDial == NULL ) {
		return false;
	}
	(void)xrtSpinLock(&pCall->Lock);
	pCall->ProxyDial = pDial;
	(void)xrtSpinUnlock(&pCall->Lock);
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtNetProxyDialCancel(pDial);
	}
	return true;
}



#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_pool.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_POOL)

#define XHTTP_POOL_IDLE_DEFAULT 128u
#define XHTTP_POOL_IDLE_ORIGIN_DEFAULT 8u
#define XHTTP_POOL_IDLE_TIMEOUT_DEFAULT INT64_C(90000)
#define XHTTP_POOL_SHARDS_MAX 32u



/* 空闲项离开 LRU 后仍可等待所属 Worker 上已经排队的关闭事件。 */
typedef enum __xrt_http_client_idle_state {
	XHTTP_POOL_IDLE_LISTED = 0,
	XHTTP_POOL_IDLE_TAKEN,
	XHTTP_POOL_IDLE_CLOSE_QUEUED,
	XHTTP_POOL_IDLE_CLOSING,
	XHTTP_POOL_IDLE_CLOSED,
	XHTTP_POOL_IDLE_USED
} __xrt_http_client_idle_state;



/* Origin 描述目标与代理路径；TLS 策略由所属 Client 固定。 */
struct __xrt_http_client_origin {
	__xrt_http_client_origin* Next;
	__xrt_http_client_pool_shard* Shard;
	str Host;
	size_t HostSize;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	uint16 Port;
	size_t Connections;
	size_t Idle;
	size_t Waiting;
	bool Secure;
};



/* 每条空闲连接持有一个 Client 引用和一个传输调用方引用。 */
struct __xrt_http_client_idle {
	__xrt_http_client_idle* Previous;
	__xrt_http_client_idle* Next;
	xhttpclient* Client;
	__xrt_http_client_origin* Origin;
	__xrt_http_client_pool_shard* Shard;
	xnetworker* Worker;
	xnetstream* Tcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* Tls;
	#endif
	double Deadline;
	__xrt_http_client_idle_state State;
	bool Released;
};



/* 每个分片独立拥有 Origin 索引、等待 FIFO、空闲 LRU 和清扫 Timer。 */
struct __xrt_http_client_pool_shard {
	xmutex Lock;
	xhttpclient* Client;
	__xrt_http_client_origin* Origins;
	__xrt_http_client_idle* IdleHead;
	__xrt_http_client_idle* IdleTail;
	xhttpcall* WaitHead;
	xhttpcall* WaitTail;
	size_t Connections;
	size_t Idle;
	size_t Closing;
	size_t Waiting;
	uint64 Timer;
	uint32 Index;
	bool Ready;
};



/* 一次加锁阶段收集待投递 Call 和待关闭连接，锁外执行回调网络操作。 */
typedef struct __xrt_http_pool_batch {
	xhttpcall* CallHead;
	xhttpcall* CallTail;
	__xrt_http_client_idle* CloseHead;
	__xrt_http_client_idle* CloseTail;
	xhttpclient* DispatchClient;
	uint32 DispatchStart;
} __xrt_http_pool_batch;



/* 唯一空闲清扫 Timer 的完成入口。 */
static void __xrtHttpPoolTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);



/* Origin 路由使用稳定的 ASCII 小写哈希，同一 Origin 始终进入同一分片。 */
static uint64 __xrtHttpPoolRouteHash(const xhttpcall* pCall)
{
	const unsigned char* pHost =
		(const unsigned char*)pCall->Host;
	uint64 iHash = UINT64_C(1469598103934665603);
	uintptr_t iProxy = 0;

	while ( *pHost != 0 ) {
		unsigned char iValue = *pHost++;

		if ( (iValue >= 'A') && (iValue <= 'Z') ) {
			iValue = (unsigned char)(iValue + ('a' - 'A'));
		}
		iHash = (iHash ^ iValue) * UINT64_C(1099511628211);
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		iProxy = (uintptr_t)pCall->Proxy;
	#endif
	iHash = (iHash ^ pCall->Port) * UINT64_C(1099511628211);
	iHash = (iHash ^ (pCall->Secure ? 1u : 0u)) *
		UINT64_C(1099511628211);
	iHash = (iHash ^ (uint64)iProxy) * UINT64_C(1099511628211);
	return iHash;
}



/* 返回 Call 路由所属分片，并保存索引供取消早期快速定位。 */
static __xrt_http_client_pool_shard* __xrtHttpPoolShardCall(
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pCall->Client;
	uint32 iIndex = (uint32)(
		__xrtHttpPoolRouteHash(pCall) %
		pClient->PoolShardCount
	);

	pCall->PoolShardIndex = iIndex;
	return &pClient->PoolShards[iIndex];
}



/* 在有限全局配额中原子保留一项；零限制表示不设上限。 */
static bool __xrtHttpPoolCountReserve(
	xatomic64* pCount,
	size_t iLimit
)
{
	uint64 iCurrent = xrtAtomic64Load(
		pCount,
		XMEMORY_RELAXED
	);

	for ( ;; ) {
		uint64 iExpected;

		if ( (iLimit != 0) &&
			(iCurrent >= (uint64)iLimit) ) {
			return false;
		}
		if ( iCurrent == UINT64_MAX ) {
			return false;
		}
		iExpected = iCurrent;
		if ( xrtAtomic64CompareExchange(
			pCount,
			&iExpected,
			iCurrent + 1u,
			XMEMORY_ACQ_REL,
			XMEMORY_RELAXED
		) ) {
			return true;
		}
		iCurrent = iExpected;
	}
}



/* 释放一项已经成功取得的全局配额。 */
static void __xrtHttpPoolCountRelease(xatomic64* pCount)
{
	(void)xrtAtomic64FetchSub(
		pCount,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 设置连接池公开操作的结构化错误。 */
static void __xrtHttpPoolSetError(
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = __xrtHttpClientErrorCreate(
		Kind,
		XHTTP_CLIENT_ERROR_POOL,
		sOperation,
		sMessage,
		NULL
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 查找与 Call 路由完全一致的 Origin。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginFind(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall,
	xstrview Host
)
{
	__xrt_http_client_origin** ppOrigin = &pShard->Origins;

	while ( *ppOrigin != NULL ) {
		__xrt_http_client_origin* pOrigin = *ppOrigin;

		if ( (pOrigin->Port == pCall->Port) &&
			(pOrigin->Secure == pCall->Secure) &&
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				(pOrigin->Proxy == pCall->Proxy) &&
			#endif
			xrtHttpHostEqual(
				(xstrview) {
					pOrigin->Host,
					pOrigin->HostSize
				},
				Host
			) ) {
			if ( ppOrigin != &pShard->Origins ) {
				*ppOrigin = pOrigin->Next;
				pOrigin->Next = pShard->Origins;
				pShard->Origins = pOrigin;
			}
			return pOrigin;
		}
		ppOrigin = &pOrigin->Next;
	}
	return NULL;
}



/* 建立由 Client 拥有的 Origin 键。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginCreate(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall,
	xstrview Host
)
{
	__xrt_http_client_origin* pOrigin;

	if ( Host.Size == SIZE_MAX ) {
		__xrtHttpPoolSetError(
			XERR_RANGE,
			"create-http-origin",
			"HTTP origin host is too large"
		);
		return NULL;
	}
	pOrigin = (__xrt_http_client_origin*)xrtCalloc(
		1,
		sizeof(*pOrigin)
	);
	if ( pOrigin == NULL ) {
		return NULL;
	}
	pOrigin->Host = (str)xrtMalloc(Host.Size + 1u);
	if ( pOrigin->Host == NULL ) {
		xrtFree(pOrigin);
		return NULL;
	}
	memcpy(pOrigin->Host, Host.Data, Host.Size);
	pOrigin->Host[Host.Size] = '\0';
	pOrigin->HostSize = Host.Size;
	pOrigin->Shard = pShard;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pCall->Proxy != NULL ) {
			pOrigin->Proxy = xrtNetProxyRetain(
				pCall->Proxy
			);
			if ( pOrigin->Proxy == NULL ) {
				xrtFree(pOrigin->Host);
				xrtFree(pOrigin);
				return NULL;
			}
		}
	#endif
	pOrigin->Port = pCall->Port;
	pOrigin->Secure = pCall->Secure;
	pOrigin->Next = pShard->Origins;
	pShard->Origins = pOrigin;
	return pOrigin;
}



/* 返回已有 Origin，缺失时按需建立。 */
static __xrt_http_client_origin* __xrtHttpPoolOriginGet(
	__xrt_http_client_pool_shard* pShard,
	const xhttpcall* pCall
)
{
	xstrview Host = {
		pCall->Host,
		strlen(pCall->Host)
	};
	__xrt_http_client_origin* pOrigin =
		__xrtHttpPoolOriginFind(pShard, pCall, Host);

	if ( pOrigin == NULL ) {
		pOrigin = __xrtHttpPoolOriginCreate(
			pShard,
			pCall,
			Host
		);
	}
	return pOrigin;
}



/* 删除已经没有连接和等待者的 Origin。 */
static void __xrtHttpPoolOriginPrune(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_origin** ppOrigin;

	if ( (pOrigin == NULL) ||
		(pOrigin->Connections != 0) ||
		(pOrigin->Waiting != 0) ) {
		return;
	}
	ppOrigin = &pShard->Origins;
	while ( (*ppOrigin != NULL) &&
		(*ppOrigin != pOrigin) ) {
		ppOrigin = &(*ppOrigin)->Next;
	}
	if ( *ppOrigin == pOrigin ) {
		*ppOrigin = pOrigin->Next;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
			xrtNetProxyRelease(pOrigin->Proxy);
		#endif
		xrtFree(pOrigin->Host);
		xrtFree(pOrigin);
	}
}



/* 判断 Client 与 Origin 的连接硬上限是否都仍有容量。 */
static bool __xrtHttpPoolCanOpen(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;

	return ((pConfig->MaxConnectionsPerOrigin == 0) ||
			(pOrigin->Connections <
			 pConfig->MaxConnectionsPerOrigin));
}



/* 判断 Client 与 Origin 的等待硬上限是否都仍有容量。 */
static bool __xrtHttpPoolCanWait(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;

	return ((pConfig->MaxWaitingPerOrigin == 0) ||
			(pOrigin->Waiting <
			 pConfig->MaxWaitingPerOrigin));
}



/*
	判断释放当前连接后指定 Origin 能否取得它留下的配额。
	只计算假设状态，不提前修改任何连接或 Origin 计数。
*/
static bool __xrtHttpPoolCanOpenAfterRelease(
	const __xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin,
	const __xrt_http_client_origin* pReleased
)
{
	const xhttpclient* pClient = pShard->Client;
	const xhttpclientpoolconfig* pConfig =
		&pClient->Config.Pool;
	size_t iOriginConnections = pOrigin->Connections;

	if ( (pOrigin == pReleased) &&
		(iOriginConnections != 0) ) {
		iOriginConnections--;
	}
	return ((pConfig->MaxConnectionsPerOrigin == 0) ||
			(iOriginConnections <
			 pConfig->MaxConnectionsPerOrigin));
}



/* 把 Call 追加到锁外投递批次。 */
static void __xrtHttpPoolBatchCall(
	__xrt_http_pool_batch* pBatch,
	xhttpcall* pCall
)
{
	pCall->PoolPrevious = NULL;
	pCall->PoolNext = NULL;
	if ( pBatch->CallTail == NULL ) {
		pBatch->CallHead = pCall;
	} else {
		pBatch->CallTail->PoolNext = pCall;
	}
	pBatch->CallTail = pCall;
}



/* 把已经摘除的空闲项追加到锁外关闭批次。 */
static void __xrtHttpPoolBatchClose(
	__xrt_http_pool_batch* pBatch,
	__xrt_http_client_idle* pIdle
)
{
	pIdle->Previous = NULL;
	pIdle->Next = NULL;
	if ( pBatch->CloseTail == NULL ) {
		pBatch->CloseHead = pIdle;
	} else {
		pBatch->CloseTail->Next = pIdle;
	}
	pBatch->CloseTail = pIdle;
}



/* 记录本批次释放了全局连接配额，锁外从相邻分片开始重新分发。 */
static void __xrtHttpPoolBatchReleased(
	__xrt_http_pool_batch* pBatch,
	__xrt_http_client_pool_shard* pShard
)
{
	pBatch->DispatchClient = pShard->Client;
	pBatch->DispatchStart = (
		pShard->Index + 1u
	) % pShard->Client->PoolShardCount;
}



/* 从全局等待队列摘除一个 Call。 */
static void __xrtHttpPoolWaitRemove(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	if ( !pCall->PoolWaiting ) {
		return;
	}
	if ( pCall->PoolPrevious == NULL ) {
		pShard->WaitHead = pCall->PoolNext;
	} else {
		pCall->PoolPrevious->PoolNext =
			pCall->PoolNext;
	}
	if ( pCall->PoolNext == NULL ) {
		pShard->WaitTail = pCall->PoolPrevious;
	} else {
		pCall->PoolNext->PoolPrevious =
			pCall->PoolPrevious;
	}
	pCall->PoolPrevious = NULL;
	pCall->PoolNext = NULL;
	pCall->PoolWaiting = false;
	pShard->Waiting--;
	__xrtHttpPoolCountRelease(&pShard->Client->PoolWaiting);
	__xrtHttpPoolCountRelease(&pShard->Client->PoolLive);
	pCall->PoolOrigin->Waiting--;
}



/* 把 Call 追加到全局有界等待队列。 */
static void __xrtHttpPoolWaitAppend(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pShard->Client;

	pCall->PoolPrevious = pShard->WaitTail;
	pCall->PoolNext = NULL;
	if ( pShard->WaitTail == NULL ) {
		pShard->WaitHead = pCall;
	} else {
		pShard->WaitTail->PoolNext = pCall;
	}
	pShard->WaitTail = pCall;
	pCall->PoolWaiting = true;
	pShard->Waiting++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	pCall->PoolOrigin->Waiting++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolWaits,
		1,
		XMEMORY_RELAXED
	);
}



/* 从空闲 LRU 摘除一项，但不改变物理连接数量。 */
static void __xrtHttpPoolIdleRemove(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle
)
{
	if ( pIdle->Previous == NULL ) {
		pShard->IdleHead = pIdle->Next;
	} else {
		pIdle->Previous->Next = pIdle->Next;
	}
	if ( pIdle->Next == NULL ) {
		pShard->IdleTail = pIdle->Previous;
	} else {
		pIdle->Next->Previous = pIdle->Previous;
	}
	pIdle->Previous = NULL;
	pIdle->Next = NULL;
	pShard->Idle--;
	__xrtHttpPoolCountRelease(&pShard->Client->PoolIdle);
	pIdle->Origin->Idle--;
}



/* 把新空闲项放在全局 LRU 的最新端。 */
static void __xrtHttpPoolIdleInsert(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle
)
{
	pIdle->Previous = NULL;
	pIdle->Next = pShard->IdleHead;
	if ( pShard->IdleHead == NULL ) {
		pShard->IdleTail = pIdle;
	} else {
		pShard->IdleHead->Previous = pIdle;
	}
	pShard->IdleHead = pIdle;
	pShard->Idle++;
	pIdle->Origin->Idle++;
}



/* 查找指定 Origin 最近归还的一条空闲连接。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleFind(
	__xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_idle* pIdle = pShard->IdleHead;

	while ( (pIdle != NULL) &&
		(pIdle->Origin != pOrigin) ) {
		pIdle = pIdle->Next;
	}
	return pIdle;
}



/* 查找指定 Origin 最久未使用的一条空闲连接。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleFindOldest(
	__xrt_http_client_pool_shard* pShard,
	const __xrt_http_client_origin* pOrigin
)
{
	__xrt_http_client_idle* pIdle = pShard->IdleTail;

	while ( (pIdle != NULL) &&
		(pIdle->Origin != pOrigin) ) {
		pIdle = pIdle->Previous;
	}
	return pIdle;
}



/* 为新拨号保留连接配额。 */
static bool __xrtHttpPoolReserve(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall
)
{
	xhttpclient* pClient = pShard->Client;

	if ( !__xrtHttpPoolCountReserve(
		&pClient->PoolConnections,
		pClient->Config.Pool.MaxConnections
	) ) {
		return false;
	}
	pShard->Connections++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	pCall->PoolOrigin->Connections++;
	pCall->PoolReserved = true;
	pCall->PoolOpened = false;
	return true;
}



/* 释放 Call 的连接配额，并按需记录物理关闭。 */
static void __xrtHttpPoolRelease(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pShard->Client;
	__xrt_http_client_origin* pOrigin =
		pCall->PoolOrigin;

	if ( !pCall->PoolReserved ) {
		return;
	}
	pShard->Connections--;
	__xrtHttpPoolCountRelease(&pClient->PoolConnections);
	__xrtHttpPoolCountRelease(&pClient->PoolLive);
	pOrigin->Connections--;
	__xrtHttpPoolBatchReleased(pBatch, pShard);
	if ( pCall->PoolOpened ) {
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
	}
	pCall->PoolReserved = false;
	pCall->PoolOpened = false;
	pCall->PoolOrigin = NULL;
	__xrtHttpPoolOriginPrune(pShard, pOrigin);
}



/* 摘除并关闭一条空闲连接，同时立即释放其池配额。 */
static void __xrtHttpPoolEvict(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_idle* pIdle,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pShard->Client;
	__xrt_http_client_origin* pOrigin =
		pIdle->Origin;

	__xrtHttpPoolIdleRemove(pShard, pIdle);
	pShard->Connections--;
	__xrtHttpPoolCountRelease(&pClient->PoolConnections);
	pOrigin->Connections--;
	__xrtHttpPoolBatchReleased(pBatch, pShard);
	pIdle->Origin = NULL;
	pIdle->State = XHTTP_POOL_IDLE_CLOSE_QUEUED;
	pIdle->Released = true;
	pShard->Closing++;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolClosing,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsClosed,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpPoolBatchClose(pBatch, pIdle);
	__xrtHttpPoolOriginPrune(pShard, pOrigin);
}



/* 把空闲项的传输所有权和原配额交给等待 Call。 */
static void __xrtHttpPoolTakeIdle(
	__xrt_http_client_pool_shard* pShard,
	xhttpcall* pCall,
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pShard->Client;

	__xrtHttpPoolIdleRemove(pShard, pIdle);
	pIdle->State = XHTTP_POOL_IDLE_TAKEN;
	pCall->PoolIdle = pIdle;
	pCall->PoolReserved = true;
	pCall->PoolOpened = true;
	pCall->Worker = pIdle->Worker;
	pCall->Affinity = xrtNetWorkerIndex(
		pIdle->Worker
	);
	pCall->PooledTcp = pIdle->Tcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = pIdle->Tls;
	#endif
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsReused,
		1,
		XMEMORY_RELAXED
	);
}



/* 为等待队列分配当前可用的空闲连接或新连接配额。 */
static void __xrtHttpPoolDispatch(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpcall* pCall = pShard->WaitHead;

	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;
		__xrt_http_client_origin* pOrigin =
			pCall->PoolOrigin;
		__xrt_http_client_idle* pIdle;

		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			pCall->PoolOrigin = NULL;
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
			__xrtHttpPoolOriginPrune(
				pShard,
				pOrigin
			);
			pCall = pNext;
			continue;
		}
		pIdle = __xrtHttpPoolIdleFind(
			pShard,
			pOrigin
		);
		if ( pIdle != NULL ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			__xrtHttpPoolTakeIdle(
				pShard,
				pCall,
				pIdle
			);
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
		} else if ( __xrtHttpPoolCanOpen(
			pShard,
			pOrigin
		) && __xrtHttpPoolReserve(pShard, pCall) ) {
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
		}
		pCall = pNext;
	}
}



/* 配额释放后逐分片分发，避免有容量时另一分片的等待者长期休眠。 */
static void __xrtHttpPoolDispatchAll(
	xhttpclient* pClient,
	uint32 iStart,
	__xrt_http_pool_batch* pBatch
)
{
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard;
		uint32 iIndex = (iStart + i) %
			pClient->PoolShardCount;

		pShard = &pClient->PoolShards[iIndex];
		(void)xrtMutexLock(&pShard->Lock);
		__xrtHttpPoolDispatch(pShard, pBatch);
		(void)xrtMutexUnlock(&pShard->Lock);
	}
}



/* 在锁外把已分配 Call 唤醒到其目标 Worker。 */
static void __xrtHttpPoolPost(__xrt_http_pool_batch* pBatch)
{
	xhttpcall* pCall = pBatch->CallHead;

	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;

		pCall->PoolNext = NULL;
		if ( !xrtNetPost(
			pCall->Worker,
			&pCall->PoolPost,
			__xrtHttpCallPoolReady,
			pCall
		) ) {
			xerror* pCause = xrtTakeError();

			__xrtHttpCallFail(
				pCall,
				XNET_RESULT_ERROR,
				XHTTP_CLIENT_ERROR_INTERNAL,
				__xrtHttpClientCauseKind(
					pCause,
					XERR_CLOSED
				),
				"post-http-pool-call",
				"HTTP pool could not resume the call on its target Worker",
				pCause
			);
			xrtErrorFree(pCause);
		}
		pCall = pNext;
	}
	pBatch->CallHead = NULL;
	pBatch->CallTail = NULL;
}



/* 关闭批次中的传输并释放池持有的调用方引用。 */
static void __xrtHttpPoolClose(__xrt_http_pool_batch* pBatch)
{
	__xrt_http_client_idle* pIdle =
		pBatch->CloseHead;

	while ( pIdle != NULL ) {
		__xrt_http_client_idle* pNext =
			pIdle->Next;
		xhttpclient* pClient = pIdle->Client;
		xnetstream* pTcp = pIdle->Tcp;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			xtlsstream* pTls = pIdle->Tls;
		#endif
		bool bClosed = false;
		bool bFinished = false;

		pIdle->Next = NULL;
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pTls != NULL ) {
				bClosed = xrtTlsStreamClose(
					pTls
				);
				if ( !bClosed ) {
					xrtClearError();
					bClosed = xrtTlsStreamAbort(
						pTls
					);
				}
			} else
		#endif
		{
			bClosed = xrtNetStreamClose(pTcp);
			if ( !bClosed ) {
				xrtClearError();
				bClosed = xrtNetStreamAbort(
					pTcp
				);
			}
		}

		/*
			批次持有 CLOSE_QUEUED 项，Close 回调只能把它标为 CLOSED。
			异步关闭成功后再交给回调；终态已经先到达时由当前批次收尾。
		*/
		(void)xrtMutexLock(&pIdle->Shard->Lock);
		if ( pIdle->State == XHTTP_POOL_IDLE_CLOSE_QUEUED ) {
			pIdle->State = XHTTP_POOL_IDLE_CLOSING;
			if ( !bClosed ) {
				pIdle->Released = false;
			}
		} else if ( pIdle->State == XHTTP_POOL_IDLE_CLOSED ) {
			bFinished = true;
		}
		(void)xrtMutexUnlock(&pIdle->Shard->Lock);

		if ( bClosed || bFinished ) {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				if ( pTls != NULL ) {
					xrtTlsStreamDestroy(pTls);
				} else
			#endif
			{
				xrtNetStreamDestroy(pTcp);
			}
		}
		if ( bFinished ) {
			xrtFree(pIdle);
			__xrtHttpPoolCountRelease(&pClient->PoolLive);
			__xrtHttpClientTryFinish(pClient);
			__xrtHttpClientRelease(pClient);
		}
		if ( !bClosed ) {
			xrtClearError();
		}
		pIdle = pNext;
	}
	pBatch->CloseHead = NULL;
	pBatch->CloseTail = NULL;
}



/* 锁外执行批次，关闭请求先于新拨号，尽快释放系统资源。 */
static void __xrtHttpPoolRun(__xrt_http_pool_batch* pBatch)
{
	if ( pBatch->DispatchClient != NULL ) {
		xhttpclient* pClient = pBatch->DispatchClient;
		uint32 iStart = pBatch->DispatchStart;

		pBatch->DispatchClient = NULL;
		__xrtHttpPoolDispatchAll(
			pClient,
			iStart,
			pBatch
		);
	}
	__xrtHttpPoolClose(pBatch);
	__xrtHttpPoolPost(pBatch);
}



/* 从 LRU 摘除出现额外输入或读端终止的空闲传输。 */
static void __xrtHttpPoolIdleRetire(
	__xrt_http_client_idle* pIdle
)
{
	__xrt_http_client_pool_shard* pShard = pIdle->Shard;
	__xrt_http_pool_batch Batch;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pIdle->State == XHTTP_POOL_IDLE_LISTED ) {
		__xrtHttpPoolEvict(pShard, pIdle, &Batch);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
}



/* 在首条可过期空闲连接出现时建立唯一清扫 Timer。 */
static bool __xrtHttpPoolTimerStart(
	__xrt_http_client_pool_shard* pShard
)
{
	xhttpclient* pClient = pShard->Client;
	uint64 Id;

	if ( (pShard->Timer != 0) ||
		(pShard->IdleTail == NULL) ||
		(pClient->Config.Pool.IdleTimeout == 0) ) {
		return true;
	}
	if ( __xrtHttpClientHold(pClient) == NULL ) {
		return false;
	}
	Id = __xrtNetEngineSchedule(
		pClient->Engine,
		pShard->Index % xrtNetEngineWorkerCount(pClient->Engine),
		pShard->IdleTail->Deadline,
		__xrtHttpPoolTimer,
		pShard
	);
	if ( Id == 0 ) {
		__xrtHttpClientRelease(pClient);
		return false;
	}
	pShard->Timer = Id;
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolTimersPending,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pClient->PoolLive,
		1,
		XMEMORY_RELEASE
	);
	return true;
}



/* 空闲传输关闭时按其状态回收配额、调用方引用和项内存。 */
static void __xrtHttpPoolIdleClosed(
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pIdle->Client;
	__xrt_http_client_pool_shard* pShard = pIdle->Shard;
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin = NULL;
	bool bDestroy = false;
	bool bFree = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pIdle->State == XHTTP_POOL_IDLE_LISTED ) {
		pOrigin = pIdle->Origin;
		__xrtHttpPoolIdleRemove(pShard, pIdle);
		pShard->Connections--;
		__xrtHttpPoolCountRelease(&pClient->PoolConnections);
		pOrigin->Connections--;
		__xrtHttpPoolBatchReleased(&Batch, pShard);
		pIdle->Origin = NULL;
		pIdle->State = XHTTP_POOL_IDLE_CLOSED;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		bDestroy = !pIdle->Released;
		bFree = true;
	} else if (
		pIdle->State == XHTTP_POOL_IDLE_CLOSE_QUEUED
	) {
		pIdle->State = XHTTP_POOL_IDLE_CLOSED;
		pShard->Closing--;
		__xrtHttpPoolCountRelease(&pClient->PoolClosing);
	} else if (
		pIdle->State == XHTTP_POOL_IDLE_CLOSING
	) {
		pIdle->State = XHTTP_POOL_IDLE_CLOSED;
		pShard->Closing--;
		__xrtHttpPoolCountRelease(&pClient->PoolClosing);
		bDestroy = !pIdle->Released;
		bFree = true;
	} else if (
		pIdle->State == XHTTP_POOL_IDLE_TAKEN
	) {
		pIdle->State = XHTTP_POOL_IDLE_CLOSED;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	if ( bDestroy ) {
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			if ( pIdle->Tls != NULL ) {
				xrtTlsStreamDestroy(pIdle->Tls);
			} else
		#endif
		{
			xrtNetStreamDestroy(pIdle->Tcp);
		}
	}
	if ( bFree ) {
		xrtFree(pIdle);
	}
	if ( bFree ) {
		__xrtHttpPoolCountRelease(&pClient->PoolLive);
	}
	__xrtHttpClientTryFinish(pClient);
	if ( bFree ) {
		__xrtHttpClientRelease(pClient);
	}
}



/* 空闲 TCP 收到任何字节都说明它不能再匹配下一次请求。 */
static void __xrtHttpPoolTcpRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* 空闲 TCP 对端半关闭后立即释放池配额。 */
static void __xrtHttpPoolTcpEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* TCP 空闲连接关闭后完成项和传输引用回收。 */
static void __xrtHttpPoolTcpClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	__xrtHttpPoolIdleClosed(
		(__xrt_http_client_idle*)pData
	);
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)

#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)

/* 空闲 TLS 收到晚到 ticket 时立即转移到所属 Client 的有界缓存。 */
static void __xrtHttpPoolTlsTicket(
	xtlsstream* pStream,
	ptr pData
)
{
	__xrt_http_client_idle* pIdle =
		(__xrt_http_client_idle*)pData;
	__xrt_http_resume_route Route;

	memset(&Route, 0, sizeof(Route));
	Route.Client = pIdle->Client;
	Route.Host.Data = pIdle->Origin->Host;
	Route.Host.Size = pIdle->Origin->HostSize;
	Route.Port = pIdle->Origin->Port;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		Route.Proxy = pIdle->Origin->Proxy;
	#endif
	__xrtHttpResumeCollectRoute(&Route, pStream);
}

#endif



/* 空闲 TLS 收到未被上一事务消费的明文后立即退出连接池。 */
static void __xrtHttpPoolTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* 空闲 TLS 收到 close_notify 后立即释放池配额。 */
static void __xrtHttpPoolTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpPoolIdleRetire(
		(__xrt_http_client_idle*)pData
	);
}



/* TLS 空闲连接关闭后完成项和传输引用回收。 */
static void __xrtHttpPoolTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	(void)pStream;
	(void)Result;
	(void)pError;
	__xrtHttpPoolIdleClosed(
		(__xrt_http_client_idle*)pData
	);
}

#endif



/* 把完成传输切换为空闲 Close 观察器。 */
static __xrt_http_client_idle* __xrtHttpPoolIdleCreate(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	__xrt_http_client_idle* pIdle;
	xhttpclient* pClient = pCall->Client;

	pIdle = (__xrt_http_client_idle*)xrtCalloc(
		1,
		sizeof(*pIdle)
	);
	if ( pIdle == NULL ) {
		return NULL;
	}
	if ( __xrtHttpClientHold(pClient) == NULL ) {
		xrtFree(pIdle);
		return NULL;
	}
	pIdle->Client = pClient;
	pIdle->Origin = pCall->PoolOrigin;
	pIdle->Shard = pCall->PoolOrigin->Shard;
	pIdle->State = XHTTP_POOL_IDLE_LISTED;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pResult->Tls != NULL ) {
			xtlsstreamevents Events;
			xnetstream* pTransport =
				xrtTlsStreamTransport(pResult->Tls);

			memset(&Events, 0, sizeof(Events));
			Events.Read = __xrtHttpPoolTlsRead;
			Events.End = __xrtHttpPoolTlsEnd;
			Events.Close = __xrtHttpPoolTlsClose;
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESUME)
				Events.Ticket = __xrtHttpPoolTlsTicket;
			#endif
			pIdle->Tls = pResult->Tls;
			pIdle->Worker = xrtNetStreamWorker(
				pTransport
			);
			if ( !xrtTlsStreamSetEvents(
				pResult->Tls,
				&Events,
				pIdle
			) ) {
				__xrtHttpClientRelease(pClient);
				xrtFree(pIdle);
				return NULL;
			}
			return pIdle;
		}
	#endif
	{
		xnetstreamevents Events;

		memset(&Events, 0, sizeof(Events));
		Events.Read = __xrtHttpPoolTcpRead;
		Events.End = __xrtHttpPoolTcpEnd;
		Events.Close = __xrtHttpPoolTcpClose;
		pIdle->Tcp = pResult->Tcp;
		pIdle->Worker = xrtNetStreamWorker(
			pResult->Tcp
		);
		if ( !xrtNetStreamSetEvents(
			pResult->Tcp,
			&Events,
			pIdle
		) ) {
			__xrtHttpClientRelease(pClient);
			xrtFree(pIdle);
			return NULL;
		}
	}
	return pIdle;
}



/*
	归还失败时在当前 Worker 摘除临时观察器。
	极端关闭竞态下保留项到 Close 回调，传输仍由调用方随后释放。
*/
static void __xrtHttpPoolIdleDiscard(
	__xrt_http_client_idle* pIdle
)
{
	xhttpclient* pClient = pIdle->Client;
	bool bDetached;

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pIdle->Tls != NULL ) {
			bDetached = xrtTlsStreamSetEvents(
				pIdle->Tls,
				NULL,
				NULL
			);
		} else
	#endif
	{
		bDetached = xrtNetStreamSetEvents(
			pIdle->Tcp,
			NULL,
			NULL
		);
	}
	if ( bDetached ) {
		pIdle->State = XHTTP_POOL_IDLE_USED;
		xrtFree(pIdle);
		__xrtHttpClientRelease(pClient);
	} else {
		xrtClearError();
		(void)xrtMutexLock(&pIdle->Shard->Lock);
		pIdle->State = XHTTP_POOL_IDLE_CLOSING;
		pIdle->Released = true;
		pIdle->Shard->Closing++;
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolClosing,
			1,
			XMEMORY_RELAXED
		);
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolLive,
			1,
			XMEMORY_RELEASE
		);
		(void)xrtMutexUnlock(&pIdle->Shard->Lock);
	}
}



/* 清扫到期空闲项，并为下一条最早截止项重建唯一 Timer。 */
static void __xrtHttpPoolTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	__xrt_http_client_pool_shard* pShard =
		(__xrt_http_client_pool_shard*)pData;
	xhttpclient* pClient = pShard->Client;
	__xrt_http_pool_batch Batch;
	double iNow = xrtTimer();
	bool bMatched;

	(void)pWorker;
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bMatched = pShard->Timer == Id;
	if ( bMatched ) {
		pShard->Timer = 0;
	}
	__xrtHttpPoolCountRelease(&pClient->PoolTimersPending);
	__xrtHttpPoolCountRelease(&pClient->PoolLive);
	if ( bMatched && (Result == XNET_RESULT_OK) ) {
		while ( (pShard->IdleTail != NULL) &&
			(pShard->IdleTail->Deadline <= iNow) ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
		}
		if ( !__xrtHttpPoolTimerStart(pShard) ) {
			xrtClearError();
			while ( pShard->IdleTail != NULL ) {
				__xrtHttpPoolEvict(
					pShard,
					pShard->IdleTail,
					&Batch
				);
			}
		}
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	__xrtHttpClientTryFinish(pClient);
	__xrtHttpClientRelease(pClient);
}



/* 初始化公开连接池默认值。 */
XHTTP_CLIENT_API void xrtHttpClientPoolConfigInit(
	xhttpclientpoolconfig* pConfig
)
{
	const xhttpclientpoolconfig Config = {
		0,
		0,
		0,
		0,
		XHTTP_POOL_IDLE_DEFAULT,
		XHTTP_POOL_IDLE_ORIGIN_DEFAULT,
		XHTTP_POOL_IDLE_TIMEOUT_DEFAULT
	};

	if ( pConfig == NULL ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"init-http-client-pool-config",
			"HTTP client pool config is null"
		);
		return;
	}
	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"init-http-client-pool-config",
			"HTTP client pool config range is invalid"
		);
		return;
	}
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 按 Engine 规模建立固定分片，避免运行期扩容和 Origin 间锁争用。 */
bool __xrtHttpPoolInit(xhttpclient* pClient)
{
	uint32 iWorkers = pClient->Engine == NULL ?
		1u : xrtNetEngineWorkerCount(pClient->Engine);
	uint32 iShards = 1;

	while ( (iShards < iWorkers) &&
		(iShards < XHTTP_POOL_SHARDS_MAX) ) {
		iShards <<= 1u;
	}
	pClient->PoolShards = (__xrt_http_client_pool_shard*)
		xrtCalloc(iShards, sizeof(*pClient->PoolShards));
	if ( pClient->PoolShards == NULL ) {
		return false;
	}
	for ( uint32 i = 0; i < iShards; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];

		if ( !xrtMutexInit(&pShard->Lock) ) {
			for ( uint32 j = 0; j < i; j++ ) {
				(void)xrtMutexUnit(
					&pClient->PoolShards[j].Lock
				);
			}
			xrtFree(pClient->PoolShards);
			pClient->PoolShards = NULL;
			return false;
		}
		pShard->Client = pClient;
		pShard->Index = i;
		pShard->Ready = true;
	}
	pClient->PoolShardCount = iShards;
	xrtAtomic64Init(&pClient->PoolConnections, 0);
	xrtAtomic64Init(&pClient->PoolIdle, 0);
	xrtAtomic64Init(&pClient->PoolClosing, 0);
	xrtAtomic64Init(&pClient->PoolWaiting, 0);
	xrtAtomic64Init(&pClient->PoolTimersPending, 0);
	xrtAtomic64Init(&pClient->PoolLive, 0);
	xrtAtomic64Init(&pClient->RequestsStarted, 0);
	xrtAtomic64Init(&pClient->RequestsCompleted, 0);
	xrtAtomic64Init(&pClient->ConnectionsOpened, 0);
	xrtAtomic64Init(&pClient->ConnectionsReused, 0);
	xrtAtomic64Init(&pClient->ConnectionsClosed, 0);
	xrtAtomic64Init(&pClient->PoolWaits, 0);
	xrtAtomic64Init(&pClient->PoolRejected, 0);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		xrtAtomic64Init(&pClient->RedirectsFollowed, 0);
	#endif
	pClient->PoolReady = true;
	return true;
}



/* 最后一个内部引用只能在池内对象和 Timer 已经排空后释放锁。 */
void __xrtHttpPoolUnit(xhttpclient* pClient)
{
	if ( !pClient->PoolReady ) {
		return;
	}
	pClient->PoolReady = false;
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];
		__xrt_http_client_origin* pOrigin = pShard->Origins;

		while ( pOrigin != NULL ) {
			__xrt_http_client_origin* pNext =
				pOrigin->Next;

			#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
				xrtNetProxyRelease(pOrigin->Proxy);
			#endif
			xrtFree(pOrigin->Host);
			xrtFree(pOrigin);
			pOrigin = pNext;
		}
		pShard->Origins = NULL;
		pShard->Ready = false;
		(void)xrtMutexUnit(&pShard->Lock);
	}
	xrtFree(pClient->PoolShards);
	pClient->PoolShards = NULL;
	pClient->PoolShardCount = 0;
}



/* 申请空闲传输、新连接配额或一个有界等待位置。 */
bool __xrtHttpPoolAcquire(xhttpcall* pCall, bool* pReady)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		__xrtHttpPoolShardCall(pCall);
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;
	__xrt_http_client_idle* pIdle;
	double iNow;
	bool bResult = true;

	memset(&Batch, 0, sizeof(Batch));
	*pReady = false;
	(void)xrtMutexLock(&pShard->Lock);
	/*
		取消分发可能在 Call 尚未进入池之前完成。
		必须在池锁内重新观察取消门，避免随后把无人再摘除的 Call 放入等待队列。
	*/
	if ( xrtAtomic32Load(
		&pCall->CancelGate,
		XMEMORY_ACQUIRE
	) ) {
		*pReady = true;
		goto finish;
	}
	pOrigin = __xrtHttpPoolOriginGet(
		pShard,
		pCall
	);
	if ( pOrigin == NULL ) {
		bResult = false;
		goto finish;
	}
	pCall->PoolOrigin = pOrigin;
	if ( pClient->Config.Pool.IdleTimeout != 0 ) {
		iNow = xrtTimer();
		while ( (pShard->IdleTail != NULL) &&
			(pShard->IdleTail->Deadline <= iNow) ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
		}
	}
	__xrtHttpPoolDispatch(pShard, &Batch);
	pIdle = __xrtHttpPoolIdleFind(
		pShard,
		pOrigin
	);
	if ( pIdle != NULL ) {
		__xrtHttpPoolTakeIdle(
			pShard,
			pCall,
			pIdle
		);
		__xrtHttpPoolBatchCall(
			&Batch,
			pCall
		);
	} else if ( __xrtHttpPoolCanOpen(
		pShard,
		pOrigin
	) && __xrtHttpPoolReserve(pShard, pCall) ) {
		*pReady = true;
	} else if ( __xrtHttpPoolCanWait(
		pShard,
		pOrigin
	) && __xrtHttpPoolCountReserve(
		&pClient->PoolWaiting,
		pClient->Config.Pool.MaxWaiting
	) ) {
		__xrtHttpPoolWaitAppend(
			pShard,
			pCall
		);
	} else {
		(void)xrtAtomic64FetchAdd(
			&pClient->PoolRejected,
			1,
			XMEMORY_RELAXED
		);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		__xrtHttpPoolSetError(
			XERR_AGAIN,
			"acquire-http-connection",
			"HTTP connection pool waiting limit reached"
		);
		bResult = false;
	}

finish:
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	return bResult;
}



/* 把仍在等待队列中的取消 Call 摘除并唤醒到其原 Worker。 */
bool __xrtHttpPoolCancel(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;
	bool bCancelled = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolWaiting ) {
		pOrigin = pCall->PoolOrigin;
		__xrtHttpPoolWaitRemove(pShard, pCall);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
		__xrtHttpPoolBatchCall(
			&Batch,
			pCall
		);
		bCancelled = true;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	return bCancelled;
}



/* 新拨号成功后把保留配额标记为一条实际连接。 */
void __xrtHttpPoolOpened(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];

	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolReserved &&
		!pCall->PoolOpened ) {
		pCall->PoolOpened = true;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsOpened,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
}



/*
	选择释放当前连接后最早可运行的等待者。
	同 Origin 可以直接接管传输；异 Origin 要求归还方释放配额，避免后到
	同源请求持续越过全局队列中的较早请求。
*/
static xhttpcall* __xrtHttpPoolTakeHandoff(
	__xrt_http_client_pool_shard* pShard,
	__xrt_http_client_origin* pOrigin,
	__xrt_http_pool_batch* pBatch,
	bool* pYield
)
{
	xhttpcall* pCall = pShard->WaitHead;

	*pYield = false;
	while ( pCall != NULL ) {
		xhttpcall* pNext = pCall->PoolNext;

		if ( xrtAtomic32Load(
			&pCall->CancelGate,
			XMEMORY_ACQUIRE
		) ) {
			__xrt_http_client_origin* pCancelled =
				pCall->PoolOrigin;

			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			pCall->PoolOrigin = NULL;
			__xrtHttpPoolBatchCall(
				pBatch,
				pCall
			);
			__xrtHttpPoolOriginPrune(
				pShard,
				pCancelled
			);
		} else if ( __xrtHttpPoolCanOpenAfterRelease(
			pShard,
			pCall->PoolOrigin,
			pOrigin
		) ) {
			if ( pCall->PoolOrigin != pOrigin ) {
				*pYield = true;
				return NULL;
			}
			__xrtHttpPoolWaitRemove(
				pShard,
				pCall
			);
			return pCall;
		}
		pCall = pNext;
	}
	return NULL;
}



/* 把刚完成的传输和原配额直接交给同 Origin 等待者。 */
static void __xrtHttpPoolHandoff(
	xhttpcall* pFrom,
	xhttpcall* pTo,
	const xhttp1callresult* pResult,
	__xrt_http_client_idle* pIdle,
	__xrt_http_pool_batch* pBatch
)
{
	xhttpclient* pClient = pFrom->Client;
	xnetstream* pTransport;

	pFrom->PoolReserved = false;
	pFrom->PoolOpened = false;
	pFrom->PoolOrigin = NULL;
	pTo->PoolReserved = true;
	pTo->PoolOpened = true;
	pTo->PoolIdle = pIdle;
	pTo->PooledTcp = pResult->Tcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pTo->PooledTls = pResult->Tls;
		if ( pResult->Tls != NULL ) {
			pTransport = xrtTlsStreamTransport(
				pResult->Tls
			);
		} else
	#endif
	{
		pTransport = pResult->Tcp;
	}
	pTo->Worker = xrtNetStreamWorker(pTransport);
	pTo->Affinity = xrtNetWorkerIndex(pTo->Worker);
	if ( pIdle != NULL ) {
		pIdle->State = XHTTP_POOL_IDLE_TAKEN;
	}
	(void)xrtAtomic64FetchAdd(
		&pClient->ConnectionsReused,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpPoolBatchCall(pBatch, pTo);
}



/* 归还可复用传输：优先同源直接交接，否则进入有界 LRU。 */
bool __xrtHttpPoolPut(
	xhttpcall* pCall,
	const xhttp1callresult* pResult
)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_idle* pIdle = NULL;
	xhttpcall* pWait;
	bool bKeep;
	bool bYield = false;

	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bKeep = pCall->PoolReserved &&
		pCall->PoolOpened &&
		(xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) == XHTTP_CLIENT_RUNNING);
	if ( bKeep ) {
		pWait = __xrtHttpPoolTakeHandoff(
			pShard,
			pCall->PoolOrigin,
			&Batch,
			&bYield
		);
		if ( pWait != NULL ) {
			__xrtHttpPoolHandoff(
				pCall,
				pWait,
				pResult,
				NULL,
				&Batch
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
			__xrtHttpPoolRun(&Batch);
			return true;
		}
		if ( !bYield &&
			(xrtAtomic64Load(
				&pClient->PoolWaiting,
				XMEMORY_ACQUIRE
			) > pShard->Waiting) ) {
			bYield = true;
		}
	}
	bKeep = bKeep &&
		!bYield &&
		(pClient->Config.Pool.MaxIdle != 0) &&
		(pClient->Config.Pool.MaxIdlePerOrigin != 0);
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
	if ( !bKeep ) {
		return false;
	}

	pIdle = __xrtHttpPoolIdleCreate(
		pCall,
		pResult
	);
	if ( pIdle == NULL ) {
		return false;
	}
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	bKeep = pCall->PoolReserved &&
		pCall->PoolOpened &&
		(xrtAtomic32Load(
			&pClient->State,
			XMEMORY_ACQUIRE
		) == XHTTP_CLIENT_RUNNING);
	if ( bKeep ) {
		bYield = false;
		pWait = __xrtHttpPoolTakeHandoff(
			pShard,
			pCall->PoolOrigin,
			&Batch,
			&bYield
		);
		if ( pWait != NULL ) {
			__xrtHttpPoolHandoff(
				pCall,
				pWait,
				pResult,
				pIdle,
				&Batch
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
			__xrtHttpPoolRun(&Batch);
			return true;
		}
		if ( !bYield &&
			(xrtAtomic64Load(
				&pClient->PoolWaiting,
				XMEMORY_ACQUIRE
			) > pShard->Waiting) ) {
			bYield = true;
		}
	}
	bKeep = bKeep &&
		!bYield &&
		(pClient->Config.Pool.MaxIdle != 0) &&
		(pClient->Config.Pool.MaxIdlePerOrigin != 0);
	if ( bKeep ) {
		while ( pCall->PoolOrigin->Idle >=
			pClient->Config.Pool.MaxIdlePerOrigin ) {
			__xrt_http_client_idle* pOld =
				__xrtHttpPoolIdleFindOldest(
					pShard,
					pCall->PoolOrigin
				);

			if ( pOld == NULL ) {
				break;
			}
			__xrtHttpPoolEvict(
				pShard,
				pOld,
				&Batch
			);
		}
		bKeep = __xrtHttpPoolCountReserve(
			&pClient->PoolIdle,
			pClient->Config.Pool.MaxIdle
		);
		if ( bKeep ) {
			pIdle->Deadline =
				pClient->Config.Pool.IdleTimeout == 0 ?
					INFINITY :
					__xrtWaitAfter(
						pClient->Config.Pool.IdleTimeout
					);
			__xrtHttpPoolIdleInsert(pShard, pIdle);
		}
		if ( bKeep ) {
			if ( !__xrtHttpPoolTimerStart(pShard) ) {
				__xrtHttpPoolIdleRemove(
					pShard,
					pIdle
				);
				bKeep = false;
			} else {
				pCall->PoolReserved = false;
				pCall->PoolOpened = false;
				pCall->PoolOrigin = NULL;
			}
		}
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	if ( !bKeep ) {
		__xrtHttpPoolIdleDiscard(pIdle);
	}
	__xrtHttpPoolRun(&Batch);
	return bKeep;
}



/* 成功附加后释放临时空闲项，传输引用已经交给低级 HTTP/1 Call。 */
void __xrtHttpPoolPooledUsed(xhttpcall* pCall)
{
	__xrt_http_client_idle* pIdle =
		pCall->PoolIdle;

	__xrtHttpCallReused(pCall);
	pCall->PoolIdle = NULL;
	pCall->PooledTcp = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = NULL;
	#endif
	if ( pIdle != NULL ) {
		pIdle->State = XHTTP_POOL_IDLE_USED;
		xrtFree(pIdle);
		__xrtHttpClientRelease(pCall->Client);
	}
}



/* 在当前 Worker 上摘除失效空闲项并关闭其传输。 */
void __xrtHttpPoolPooledStale(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_client_idle* pIdle =
		pCall->PoolIdle;
	xnetstream* pTcp = pCall->PooledTcp;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		xtlsstream* pTls = pCall->PooledTls;
	#endif
	xerror* pSaved = xrtTakeError();
	bool bDetached = true;
	bool bClosed;

	pCall->PoolIdle = NULL;
	pCall->PooledTcp = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		pCall->PooledTls = NULL;
	#endif
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolOpened ) {
		pCall->PoolOpened = false;
		(void)xrtAtomic64FetchAdd(
			&pClient->ConnectionsClosed,
			1,
			XMEMORY_RELAXED
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	if ( pIdle != NULL ) {
		if ( pIdle->State == XHTTP_POOL_IDLE_CLOSED ) {
			bDetached = true;
		} else {
			#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
				if ( pTls != NULL ) {
					bDetached =
						xrtTlsStreamSetEvents(
							pTls,
							NULL,
							NULL
						);
				} else
			#endif
			{
				bDetached = xrtNetStreamSetEvents(
					pTcp,
					NULL,
					NULL
				);
			}
		}
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
		if ( pTls != NULL ) {
			bClosed = xrtTlsStreamAbort(pTls);
			if ( bClosed || bDetached ) {
				xrtTlsStreamDestroy(pTls);
			}
		} else
	#endif
	{
		bClosed = xrtNetStreamAbort(pTcp);
		if ( bClosed || bDetached ) {
			xrtNetStreamDestroy(pTcp);
		}
	}
	if ( pIdle != NULL ) {
		if ( bDetached ) {
			pIdle->State = XHTTP_POOL_IDLE_USED;
			xrtFree(pIdle);
			__xrtHttpClientRelease(pClient);
		} else {
			(void)xrtMutexLock(&pShard->Lock);
			pIdle->State = XHTTP_POOL_IDLE_CLOSING;
			pIdle->Released = bClosed;
			pShard->Closing++;
			(void)xrtAtomic64FetchAdd(
				&pClient->PoolClosing,
				1,
				XMEMORY_RELAXED
			);
			(void)xrtAtomic64FetchAdd(
				&pClient->PoolLive,
				1,
				XMEMORY_RELEASE
			);
			(void)xrtMutexUnlock(
				&pShard->Lock
			);
		}
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 标记升级传输已经转交调用方，随后只释放池配额。 */
void __xrtHttpPoolTransferred(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];

	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolReserved ) {
		pCall->PoolOpened = false;
	}
	(void)xrtMutexUnlock(&pShard->Lock);
}



/* 唯一终态释放等待位置、连接配额，并唤醒当前可运行的等待者。 */
void __xrtHttpPoolFinish(xhttpcall* pCall)
{
	xhttpclient* pClient = pCall->Client;
	__xrt_http_client_pool_shard* pShard =
		&pClient->PoolShards[pCall->PoolShardIndex];
	__xrt_http_pool_batch Batch;
	__xrt_http_client_origin* pOrigin;

	if ( (pCall->PooledTcp != NULL)
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			|| (pCall->PooledTls != NULL)
		#endif
	) {
		__xrtHttpPoolPooledStale(pCall);
	}
	memset(&Batch, 0, sizeof(Batch));
	(void)xrtMutexLock(&pShard->Lock);
	if ( pCall->PoolWaiting ) {
		pOrigin = pCall->PoolOrigin;
		__xrtHttpPoolWaitRemove(pShard, pCall);
		pCall->PoolOrigin = NULL;
		__xrtHttpPoolOriginPrune(
			pShard,
			pOrigin
		);
	} else {
		/*
			等待位置与连接配额互斥，只有已经取得配额的
			Call 才能递减 Origin 的连接计数。
		*/
		__xrtHttpPoolRelease(
			pShard,
			pCall,
			&Batch
		);
	}
	(void)xrtMutexUnlock(&pShard->Lock);
	__xrtHttpPoolRun(&Batch);
}



/* 关闭当前全部空闲连接，不影响活动 Call 或等待者。 */
XHTTP_CLIENT_API size_t xrtHttpClientCloseIdle(xhttpclient* pClient)
{
	__xrt_http_pool_batch Batch;
	size_t iClosed = 0;

	if ( pClient == NULL ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"close-http-client-idle",
			"HTTP client is null"
		);
		return 0;
	}
	/* 构造失败回滚可能在 PoolLock 建立前隐式进入 Drain。 */
	if ( !pClient->PoolReady ) {
		return 0;
	}
	memset(&Batch, 0, sizeof(Batch));
	for ( uint32 i = 0; i < pClient->PoolShardCount; i++ ) {
		__xrt_http_client_pool_shard* pShard =
			&pClient->PoolShards[i];
		uint64 Id;

		(void)xrtMutexLock(&pShard->Lock);
		while ( pShard->IdleTail != NULL ) {
			__xrtHttpPoolEvict(
				pShard,
				pShard->IdleTail,
				&Batch
			);
			iClosed++;
		}
		Id = pShard->Timer;
		pShard->Timer = 0;
		__xrtHttpPoolDispatch(pShard, &Batch);
		(void)xrtMutexUnlock(&pShard->Lock);
		if ( (Id != 0) &&
			!xrtNetEngineTimerCancelCurrent(
				pClient->Engine,
				Id
			) &&
			!xrtNetEngineTimerCancel(
				pClient->Engine,
				Id
			) ) {
			xrtClearError();
		}
	}
	__xrtHttpPoolRun(&Batch);
	__xrtHttpClientTryFinish(pClient);
	return iClosed;
}



/* 读取连接池和生命周期计数的一致池快照。 */
XHTTP_CLIENT_API bool xrtHttpClientStats(
	const xhttpclient* pClient,
	xhttpclientstats* pStats
)
{
	xhttpclientstats Stats;
	uint64 iConnections;
	uint64 iIdle;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pStats, sizeof(Stats)) ) {
		__xrtHttpPoolSetError(
			XERR_ARGUMENT,
			"query-http-client-stats",
			"HTTP client and complete stats storage are required"
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	iConnections = xrtAtomic64Load(
		&pClient->PoolConnections,
		XMEMORY_ACQUIRE
	);
	iIdle = xrtAtomic64Load(
		&pClient->PoolIdle,
		XMEMORY_ACQUIRE
	);
	Stats.ActiveConnections = (size_t)(
		iConnections >= iIdle ?
			iConnections - iIdle : 0
	);
	Stats.IdleConnections = (size_t)iIdle;
	Stats.ClosingConnections = (size_t)xrtAtomic64Load(
		&pClient->PoolClosing,
		XMEMORY_ACQUIRE
	);
	Stats.WaitingCalls = (size_t)xrtAtomic64Load(
		&pClient->PoolWaiting,
		XMEMORY_ACQUIRE
	);
	Stats.RequestsStarted = xrtAtomic64Load(
		&pClient->RequestsStarted,
		XMEMORY_RELAXED
	);
	Stats.RequestsCompleted = xrtAtomic64Load(
		&pClient->RequestsCompleted,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsOpened = xrtAtomic64Load(
		&pClient->ConnectionsOpened,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsReused = xrtAtomic64Load(
		&pClient->ConnectionsReused,
		XMEMORY_RELAXED
	);
	Stats.ConnectionsClosed = xrtAtomic64Load(
		&pClient->ConnectionsClosed,
		XMEMORY_RELAXED
	);
	Stats.PoolWaits = xrtAtomic64Load(
		&pClient->PoolWaits,
		XMEMORY_RELAXED
	);
	Stats.PoolRejected = xrtAtomic64Load(
		&pClient->PoolRejected,
		XMEMORY_RELAXED
	);
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		Stats.RedirectsFollowed = xrtAtomic64Load(
			&pClient->RedirectsFollowed,
			XMEMORY_RELAXED
		);
	#endif
	memcpy(pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_cookie.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)

#define XRT_HTTP_COOKIE_FLAGS \
	(XHTTP_COOKIE_DISABLED | XHTTP_COOKIE_SAME_SITE | \
	 XHTTP_COOKIE_TOP_LEVEL)



/* 初始化普通同站 HTTP API 调用的 Cookie 策略。 */
XHTTP_CLIENT_API void xrtHttpCookieOptionsInit(
	xhttpcookieoptions* pOptions
)
{
	const xhttpcookieoptions Options = {
		XHTTP_COOKIE_SAME_SITE,
		{ NULL, 0 }
	};

	if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 复制可跨异步和重定向阶段使用的分区键。 */
static bool __xrtHttpCookiePartitionCopy(
	xhttpcall* pCall,
	xstrview PartitionKey
)
{
	if ( (PartitionKey.Data == NULL) &&
		(PartitionKey.Size != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( PartitionKey.Size == 0 ) {
		return true;
	}
	if ( PartitionKey.Size == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	pCall->CookiePartitionKey =
		(str)xrtMalloc(PartitionKey.Size + 1);
	if ( pCall->CookiePartitionKey == NULL ) {
		return false;
	}
	memcpy(
		pCall->CookiePartitionKey,
		PartitionKey.Data,
		PartitionKey.Size
	);
	pCall->CookiePartitionKey[PartitionKey.Size] = '\0';
	pCall->CookiePartitionSize = PartitionKey.Size;
	return true;
}



/* 复制 Call 策略；没有配置 Jar 时保持零成本禁用状态。 */
bool __xrtHttpCookieInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	if ( (pCall == NULL) || (pOptions == NULL) ||
		((pOptions->Cookies.Flags &
		  ~XRT_HTTP_COOKIE_FLAGS) != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		if ( pCall != NULL ) {
			pCall->CookieError =
				XHTTP_CLIENT_ERROR_COOKIE;
		}
		return false;
	}
	pCall->CookieFlags = pOptions->Cookies.Flags;
	if ( (pCall->Client->Cookies == NULL) ||
		((pCall->CookieFlags &
		  XHTTP_COOKIE_DISABLED) != 0) ) {
		return true;
	}
	if ( !__xrtHttpCookiePartitionCopy(
		pCall,
		pOptions->Cookies.PartitionKey
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	pCall->CookiesEnabled = true;
	return true;
}



/* 释放 Call 拥有的 Cookie 上下文。 */
void __xrtHttpCookieUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	xrtFree(pCall->CookiePartitionKey);
	pCall->CookiePartitionKey = NULL;
	pCall->CookiePartitionSize = 0;
	pCall->CookieAutomatic = false;
}



/* 为当前跳构造精确的 Cookie 请求上下文。 */
static void __xrtHttpCookieRequestContext(
	const xhttpcall* pCall,
	xcookierequestcontext* pContext
)
{
	memset(pContext, 0, sizeof(*pContext));
	pContext->Flags = XCOOKIE_REQUEST_HTTP_API;
	if ( (pCall->CookieFlags &
		XHTTP_COOKIE_SAME_SITE) != 0 ) {
		pContext->Flags |= XCOOKIE_REQUEST_SAME_SITE;
	}
	if ( (pCall->CookieFlags &
		XHTTP_COOKIE_TOP_LEVEL) != 0 ) {
		pContext->Flags |= XCOOKIE_REQUEST_TOP_LEVEL;
	}
	if ( xrtHttpMethodSafe(
		xrtHttpRequestMethod(pCall->Request)
	) ) {
		pContext->Flags |= XCOOKIE_REQUEST_SAFE_METHOD;
	}
	pContext->URL = xrtHttpRequestUrlText(
		pCall->Request
	);
	pContext->PartitionKey = (xstrview){
		pCall->CookiePartitionKey,
		pCall->CookiePartitionSize
	};
}



/*
	显式 Cookie 由调用方完全控制。
	自动字段在每一跳先删除再按新 URL、方法和 Jar 状态重新生成。
*/
bool __xrtHttpCookiePrepare(xhttpcall* pCall)
{
	xcookierequestcontext Context;
	xhttpheaders* pHeaders;

	if ( pCall == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pCall->CookieError = 0;
	if ( !pCall->CookiesEnabled ) {
		return true;
	}
	pHeaders = xrtHttpRequestHeaders(pCall->Request);
	if ( pHeaders == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	if ( pCall->CookieAutomatic ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Cookie")
		);
		pCall->CookieAutomatic = false;
	}
	if ( xrtHttpHeadersHas(
		pHeaders,
		XRT_STR_LITERAL("Cookie")
	) ) {
		return true;
	}
	__xrtHttpCookieRequestContext(pCall, &Context);
	if ( !xrtCookieJarApply(
		pCall->Client->Cookies,
		&Context,
		pHeaders
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	pCall->CookieAutomatic = true;
	return true;
}



/* 把当前 Jar 错误包装成同步提交阶段的客户端错误。 */
void __xrtHttpCookieSetSubmitError(xhttpcall* pCall)
{
	xerror* pCause;
	xerror* pError;
	xerrkind Kind;

	if ( (pCall == NULL) ||
		(pCall->CookieError == 0) ) {
		return;
	}
	pCause = xrtTakeError();
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY :
			(pCause != NULL ?
				xrtErrorKind(pCause) : XERR_PROTOCOL);
	pError = __xrtHttpClientErrorCreate(
		Kind,
		XHTTP_CLIENT_ERROR_COOKIE,
		"prepare-http-cookies",
		"HTTP Cookie selection failed",
		pCause
	);
	xrtSetError(pError);
	xrtErrorFree(pError);
	xrtErrorFree(pCause);
}



/* 信息响应不修改 Jar，只保持下层回调时序。 */
static bool __xrtHttpCookieInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CookieNext.Informational == NULL ) {
		return true;
	}
	return pCall->CookieNext.Informational(
		pResponse,
		pCall->CookieNext.Data
	);
}



/* 在任何最终响应交给重定向或调用方前接收独立 Set-Cookie 字段。 */
static bool __xrtHttpCookieHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xcookiestorecontext Context;

	pCall->CookieError = 0;
	memset(&Context, 0, sizeof(Context));
	Context.Flags = XCOOKIE_STORE_HTTP_API;
	if ( (pCall->CookieFlags & XHTTP_COOKIE_SAME_SITE) != 0 ) {
		Context.Flags |= XCOOKIE_STORE_SAME_SITE;
	}
	if ( (pCall->CookieFlags & XHTTP_COOKIE_TOP_LEVEL) != 0 ) {
		Context.Flags |= XCOOKIE_STORE_TOP_LEVEL;
	}
	Context.URL = xrtHttpRequestUrlText(
		pCall->Request
	);
	Context.PartitionKey = (xstrview){
		pCall->CookiePartitionKey,
		pCall->CookiePartitionSize
	};
	if ( !xrtCookieJarStoreHeaders(
		pCall->Client->Cookies,
		&Context,
		xrtHttpResponseHeaders(pResponse),
		NULL
	) ) {
		pCall->CookieError = XHTTP_CLIENT_ERROR_COOKIE;
		return false;
	}
	if ( pCall->CookieNext.Headers == NULL ) {
		return true;
	}
	return pCall->CookieNext.Headers(
		pResponse,
		pCall->CookieNext.Data
	);
}



/* 保持原始缓冲或流式正文语义。 */
static bool __xrtHttpCookieBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CookieNext.Body == NULL ) {
		return xrtHttpBridgeResponseBufferDeliveredBody(
			(xhttpresponse*)pResponse,
			Data
		);
	}
	return pCall->CookieNext.Body(
		pResponse,
		Data,
		pCall->CookieNext.Data
	);
}



/* 构造一层只拥有事件副本、不拥有用户回调数据的 Cookie 包装器。 */
const xhttp1exchangeevents* __xrtHttpCookieEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->CookiesEnabled ) {
		return pNext;
	}
	pCall->CookieNext = *pNext;
	memset(
		&pCall->CookieEvents,
		0,
		sizeof(pCall->CookieEvents)
	);
	pCall->CookieEvents.Informational =
		__xrtHttpCookieInformational;
	pCall->CookieEvents.Headers =
		__xrtHttpCookieHeaders;
	pCall->CookieEvents.Body =
		__xrtHttpCookieBody;
	pCall->CookieEvents.Data = pCall;
	return &pCall->CookieEvents;
}



/* 把 Cookie 包装器拒绝映射成稳定的高层错误。 */
bool __xrtHttpCookieFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xerrkind Kind;

	if ( (pCall == NULL) ||
		(pCall->CookieError == 0) ) {
		return false;
	}
	Kind = (pCause != NULL) &&
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ?
			XERR_MEMORY :
			(pCause != NULL ?
				xrtErrorKind(pCause) : XERR_PROTOCOL);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_COOKIE,
		Kind,
		"process-http-cookies",
		"HTTP Cookie processing failed",
		pCause
	);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_decompress.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)

typedef struct xrt_http_decompress_output {
	xhttpcall* Call;
	size_t Inflater;
} xrt_http_decompress_output;



/* 初始化高层客户端自动内容解码策略。 */
XHTTP_CLIENT_API void xrtHttpDecompressConfigInit(
	xhttpdecompressconfig* pConfig
)
{
	xhttpdecompressconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-decompress-config",
			"HTTP decompression config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.MaxBody = XHTTP_DECOMPRESS_BODY_DEFAULT;
	Config.MaxCodings =
		XHTTP_DECOMPRESS_CODINGS_DEFAULT;
	Config.Enabled = true;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 设置一次表示解码失败并保留当前错误作为后续原因。 */
static bool __xrtHttpDecompressError(
	xhttpcall* pCall,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	pCall->DecompressFailed = true;
	__xrtHttpClientSetError(
		Kind,
		XHTTP_CLIENT_ERROR_DECOMPRESSION,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 把协议层内置编码映射为客户端 Inflate 格式。 */
static xinflateformat __xrtHttpDecompressFormat(
	xhttpcoding Coding
)
{
	return Coding == XHTTP_CODING_GZIP ?
		XINFLATE_GZIP : XINFLATE_DEFLATE;
}



/* 释放一个尚未发布到 Call 的局部解码器数组。 */
static void __xrtHttpDecompressArrayFree(
	xinflate** pInflaters,
	size_t iCount
)
{
	size_t i;

	if ( pInflaters == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		xrtInflateDestroy(pInflaters[i]);
	}
	xrtFree(pInflaters);
}



/* 复制全部同名 Content-Encoding 字段并按标准字段合并顺序连接。 */
static str __xrtHttpDecompressEncodingCopy(
	const xhttpresponse* pResponse,
	size_t iSize
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	str sEncoding;
	size_t iWritten;

	if ( iSize == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	sEncoding = (str)xrtMalloc(iSize + 1u);
	if ( sEncoding == NULL ) {
		return NULL;
	}
	if ( !xrtHttpContentEncodingWrite(
		pFields,
		iFieldCount,
		sEncoding,
		iSize,
		&iWritten
	) || (iWritten != iSize) ) {
		xrtFree(sEncoding);
		return NULL;
	}
	sEncoding[iWritten] = '\0';
	return sEncoding;
}



/*
	解析完整 Content-Encoding 列表。
	零表示没有可解码表示，一表示全部受支持，负值表示语法或限额失败。
*/
static int __xrtHttpDecompressPlan(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xinflateformat Formats[XHTTP_DECOMPRESS_CODINGS_MAX],
	size_t* pInflaterCount,
	size_t* pEncodingSize
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	xhttpcontentencodingplan Plan;
	xhttpcontentencodingcursor Cursor;
	xhttpcontentencodingitem Item;
	xhttpnext Next;
	size_t iInflaterCount = 0;

	if ( !xrtHttpContentEncodingPlan(
		pFields, iFieldCount, &Plan
	) ) {
		xerror* pCause = xrtErrorRef(xrtGetError());
		xerrkind Kind = xrtErrorIs(
			pCause, XERR_RANGE
		) != NULL ? XERR_RANGE : XERR_PROTOCOL;

		(void)__xrtHttpDecompressError(
			pCall,
			Kind,
			"parse-http-content-encoding",
			"HTTP Content-Encoding list is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		return -1;
	}
	if ( (Plan.FieldCount == 0) ||
		(Plan.UnknownCount != 0) ) {
		return 0;
	}
	if ( Plan.CodingCount >
		pCall->DecompressMaxCodings ) {
		(void)__xrtHttpDecompressError(
			pCall,
			XERR_RANGE,
			"parse-http-content-encoding",
			"HTTP Content-Encoding nesting exceeds its limit",
			NULL
		);
		return -1;
	}
	if ( Plan.DecoderCount == 0 ) {
		return 0;
	}
	xrtHttpContentEncodingCursorInit(&Cursor);
	while ( (Next = xrtHttpContentEncodingNext(
		pFields,
		iFieldCount,
		&Cursor,
		&Item
	)) == XHTTP_NEXT_ITEM ) {
		if ( (Item.Coding == XHTTP_CODING_GZIP) ||
			(Item.Coding == XHTTP_CODING_DEFLATE) ) {
			Formats[iInflaterCount] =
				__xrtHttpDecompressFormat(
					Item.Coding
				);
			iInflaterCount++;
		}
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		(void)__xrtHttpDecompressError(
			pCall,
			XERR_INTERNAL,
			"parse-http-content-encoding",
			"HTTP Content-Encoding plan changed while iterating",
			xrtGetError()
		);
		return -1;
	}
	*pInflaterCount = iInflaterCount;
	*pEncodingSize = Plan.JoinedSize;
	return 1;
}



/* 为最终响应创建逆序执行的表示解码链。 */
static bool __xrtHttpDecompressSetup(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	xinflateformat Formats[
		XHTTP_DECOMPRESS_CODINGS_MAX
	];
	xinflateconfig Config;
	xinflate** pInflaters;
	str sOriginal;
	size_t iCodingCount = 0;
	size_t iEncodingSize = 0;
	size_t i;
	int iPlan;

	if ( !xrtHttpResponseContentAllowed(
		xrtHttpRequestMethod(pCall->Request),
		xrtHttpResponseStatus(pResponse)
	) ) {
		return true;
	}
	iPlan = __xrtHttpDecompressPlan(
		pCall,
		pResponse,
		Formats,
		&iCodingCount,
		&iEncodingSize
	);
	if ( iPlan <= 0 ) {
		return iPlan == 0;
	}
	pInflaters = (xinflate**)xrtCalloc(
		iCodingCount,
		sizeof(*pInflaters)
	);
	if ( pInflaters == NULL ) {
		return __xrtHttpDecompressError(
			pCall,
			XERR_MEMORY,
			"create-http-content-decoders",
			"HTTP content decoder array allocation failed",
			xrtGetError()
		);
	}
	xrtInflateConfigInit(&Config);
	Config.OutputLimit = pCall->DecompressLimit;
	for ( i = 0; i < iCodingCount; i++ ) {
		Config.Format =
			Formats[iCodingCount - i - 1u];
		pInflaters[i] = xrtInflateCreate(&Config);
		if ( pInflaters[i] == NULL ) {
			xerror* pCause = xrtErrorRef(xrtGetError());

			__xrtHttpDecompressArrayFree(
				pInflaters,
				iCodingCount
			);
			(void)__xrtHttpDecompressError(
				pCall,
				xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
					XERR_MEMORY : XERR_INTERNAL,
				"create-http-content-decoders",
				"HTTP content decoder creation failed",
				pCause
			);
			xrtErrorFree(pCause);
			return false;
		}
	}
	sOriginal = __xrtHttpDecompressEncodingCopy(
		pResponse,
		iEncodingSize
	);
	if ( sOriginal == NULL ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		__xrtHttpDecompressArrayFree(
			pInflaters,
			iCodingCount
		);
		(void)__xrtHttpDecompressError(
			pCall,
			xrtErrorIs(pCause, XERR_RANGE) != NULL ?
				XERR_RANGE : XERR_MEMORY,
			"copy-http-content-encoding",
			"HTTP Content-Encoding copy failed",
			pCause
		);
		xrtErrorFree(pCause);
		return false;
	}
	pCall->Inflaters = pInflaters;
	pCall->InflaterCount = iCodingCount;
	pCall->DecompressResponse = pResponse;
	pCall->DecompressActive = true;
	xrtHttpBridgeResponseSetDecoded(
		pResponse,
		sOriginal,
		iEncodingSize
	);
	return true;
}



/* 转发信息响应，不为其建立内容解码状态。 */
static bool __xrtHttpDecompressInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->DecompressNext.Informational == NULL ) {
		return true;
	}
	return pCall->DecompressNext.Informational(
		pResponse,
		pCall->DecompressNext.Data
	);
}



/* 在用户看到最终 Header 前完成编码计划和元数据改写。 */
static bool __xrtHttpDecompressHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( !__xrtHttpDecompressSetup(
		pCall,
		(xhttpresponse*)pResponse
	) ) {
		return false;
	}
	if ( pCall->DecompressNext.Headers == NULL ) {
		return true;
	}
	return pCall->DecompressNext.Headers(
		pResponse,
		pCall->DecompressNext.Data
	);
}



/* 把最内层解码输出交给下一个事件消费者。 */
static bool __xrtHttpDecompressDeliver(
	xhttpcall* pCall,
	xbytesview Data
)
{
	bool bAccepted;

	/*
		Exchange 会在最外层回调返回后累计线路片段。
		发布明文前恢复已接受的解码长度，使流式回调看到一致的响应语义。
	*/
	xrtHttpBridgeResponseSetBodyBytes(
		pCall->DecompressResponse,
		pCall->DecompressBodyBytes
	);
	if ( pCall->DecompressNext.Body == NULL ) {
		bAccepted = xrtHttpBridgeResponseBufferDeliveredBody(
			pCall->DecompressResponse,
			Data
		);
	} else {
		bAccepted = pCall->DecompressNext.Body(
			pCall->DecompressResponse,
			Data,
			pCall->DecompressNext.Data
		);
	}
	if ( !bAccepted ) {
		if ( pCall->DecompressNext.Body == NULL ) {
			pCall->DecompressFailed = true;
		} else {
			pCall->DecompressForwardFailed = true;
		}
		return false;
	}
	if ( pCall->DecompressBodyBytes >
		(UINT64_MAX - (uint64)Data.Size) ) {
		xrtSetErrorKind(XERR_RANGE);
		pCall->DecompressFailed = true;
		return false;
	}
	pCall->DecompressBodyBytes += (uint64)Data.Size;
	return true;
}



/* 把一层输出递归送入下一层，最后交给 HTTP 事件链。 */
static bool __xrtHttpDecompressOutput(
	xbytesview Data,
	ptr pData
)
{
	xrt_http_decompress_output* pOutput =
		(xrt_http_decompress_output*)pData;
	xhttpcall* pCall = pOutput->Call;
	size_t iNext = pOutput->Inflater + 1u;

	if ( iNext < pCall->InflaterCount ) {
		xrt_http_decompress_output Next;

		Next.Call = pCall;
		Next.Inflater = iNext;
		return xrtInflateWrite(
			pCall->Inflaters[iNext],
			Data,
			false,
			__xrtHttpDecompressOutput,
			&Next
		);
	}
	return __xrtHttpDecompressDeliver(pCall, Data);
}



/* 解码一段线路表示正文，未知编码路径直接转发原始字节。 */
static bool __xrtHttpDecompressBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	xrt_http_decompress_output Output;

	if ( !pCall->DecompressActive ) {
		if ( pCall->DecompressNext.Body == NULL ) {
			return xrtHttpBridgeResponseBufferDeliveredBody(
				(xhttpresponse*)pResponse,
				Data
			);
		}
		return pCall->DecompressNext.Body(
			pResponse,
			Data,
			pCall->DecompressNext.Data
		);
	}
	Output.Call = pCall;
	Output.Inflater = 0;
	if ( !xrtInflateWrite(
		pCall->Inflaters[0],
		Data,
		false,
		__xrtHttpDecompressOutput,
		&Output
	) ) {
		pCall->DecompressFailed = true;
		return false;
	}
	return true;
}



/* 冻结本次调用的启用模式、限额和自动协商 Header。 */
bool __xrtHttpDecompressInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xhttpdecompressmode Mode = pOptions->Decompress;

	if ( (Mode < XHTTP_DECOMPRESS_DEFAULT) ||
		(Mode > XHTTP_DECOMPRESS_RAW) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_DECOMPRESSION,
			"configure-http-decompression",
			"HTTP call decompression mode is invalid",
			NULL
		);
		return false;
	}
	pCall->DecompressEnabled =
		Mode == XHTTP_DECOMPRESS_AUTO ||
		((Mode == XHTTP_DECOMPRESS_DEFAULT) &&
		 pCall->Client->Config.Decompress.Enabled);
	pCall->DecompressLimit =
		pCall->Client->Config.Decompress.MaxBody;
	pCall->DecompressMaxCodings =
		pCall->Client->Config.Decompress.MaxCodings;
	if ( !pCall->DecompressEnabled ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Range")
		) != NULL) ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Accept-Encoding")
		) != NULL) ) {
		return true;
	}
	if ( !xrtHttpRequestAddHeader(
		pCall->Request,
		XRT_STR_LITERAL("Accept-Encoding"),
		XRT_STR_LITERAL("gzip, deflate")
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpClientSetError(
			xrtErrorIs(pCause, XERR_MEMORY) != NULL ?
				XERR_MEMORY : XERR_VALUE,
			XHTTP_CLIENT_ERROR_DECOMPRESSION,
			"prepare-http-accept-encoding",
			"HTTP automatic Accept-Encoding could not be added",
			pCause
		);
		return false;
	}
	return true;
}



/* 清除当前一跳的解码对象，不改变冻结的调用策略。 */
void __xrtHttpDecompressReset(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	__xrtHttpDecompressArrayFree(
		pCall->Inflaters,
		pCall->InflaterCount
	);
	pCall->Inflaters = NULL;
	pCall->InflaterCount = 0;
	pCall->DecompressResponse = NULL;
	pCall->DecompressBodyBytes = 0;
	pCall->DecompressActive = false;
	pCall->DecompressFailed = false;
	pCall->DecompressForwardFailed = false;
}



/* 构造只在启用时存在的内容解码事件装饰器。 */
const xhttp1exchangeevents* __xrtHttpDecompressEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->DecompressEnabled ) {
		return pNext;
	}
	pCall->DecompressNext = *pNext;
	memset(
		&pCall->DecompressEvents,
		0,
		sizeof(pCall->DecompressEvents)
	);
	pCall->DecompressEvents.Informational =
		__xrtHttpDecompressInformational;
	pCall->DecompressEvents.Headers =
		__xrtHttpDecompressHeaders;
	pCall->DecompressEvents.Body =
		__xrtHttpDecompressBody;
	pCall->DecompressEvents.Data = pCall;
	return &pCall->DecompressEvents;
}



/* 依次结束外层到内层解码器并发布最终正文长度。 */
bool __xrtHttpDecompressFinish(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	size_t i;

	if ( !pCall->DecompressActive ) {
		return true;
	}
	if ( (pResponse == NULL) ||
		(pCall->DecompressResponse != pResponse) ||
		(pCall->InflaterCount == 0) ) {
		return __xrtHttpDecompressError(
			pCall,
			XERR_INTERNAL,
			"finish-http-content-decoders",
			"HTTP content decoder state is inconsistent",
			NULL
		);
	}
	for ( i = 0; i < pCall->InflaterCount; i++ ) {
		xrt_http_decompress_output Output;

		if ( xrtInflateDone(pCall->Inflaters[i]) ) {
			continue;
		}
		Output.Call = pCall;
		Output.Inflater = i;
		if ( !xrtInflateWrite(
			pCall->Inflaters[i],
			(xbytesview){ NULL, 0 },
			true,
			__xrtHttpDecompressOutput,
			&Output
		) ) {
			pCall->DecompressFailed = true;
			return false;
		}
	}
	xrtHttpBridgeResponseSetBodyBytes(
		pResponse,
		pCall->DecompressBodyBytes
	);
	pCall->DecompressActive = false;
	return true;
}



/* 把真正的解码失败提升为高层客户端终态。 */
bool __xrtHttpDecompressFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	xerrkind Kind = XERR_PROTOCOL;

	if ( (pCall == NULL) ||
		!pCall->DecompressFailed ||
		pCall->DecompressForwardFailed ) {
		return false;
	}
	if ( xrtErrorIs(pCause, XERR_MEMORY) != NULL ) {
		Kind = XERR_MEMORY;
	} else if ( xrtErrorIs(pCause, XERR_RANGE) != NULL ) {
		Kind = XERR_RANGE;
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_DECOMPRESSION,
		Kind,
		"decode-http-response",
		"HTTP response content decoding failed",
		pCause
	);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_cache.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#include <stdio.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_COMMIT_RETRIES 8u
#define XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY 256u



/* 把最终缓存来源发布到并发诊断快照。 */
static void __xrtHttpClientCacheOutcome(
	xhttpcall* pCall,
	xhttpclientcacheoutcome Outcome
)
{
	xrtAtomic32Store(
		&pCall->Info.Cache,
		(uint32)Outcome,
		XMEMORY_RELEASE
	);
}



/* 建立稳定的缓存域错误并标记严格模式回调失败。 */
static bool __xrtHttpClientCacheError(
	xhttpcall* pCall,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	pCall->CacheFailed = true;
	__xrtHttpClientSetError(
		Kind,
		XHTTP_CLIENT_ERROR_CACHE,
		sOperation,
		sMessage,
		pCause
	);
	return false;
}



/* 宽松模式清除存储后端错误，严格模式把错误保留给终态。 */
static bool __xrtHttpClientCacheRecover(
	xhttpcall* pCall,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	if ( !pCall->Client->Config.Cache.Strict ) {
		xrtErrorFree(pCause);
		xrtClearError();
		return true;
	}
	(void)__xrtHttpClientCacheError(
		pCall,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_IO
		),
		sOperation,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return false;
}



/* 复制允许为空的分区文本并追加调试用零字符。 */
static str __xrtHttpClientCacheText(
	xstrview Text
)
{
	str sCopy;

	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( Text.Size == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	sCopy = (str)xrtMalloc(Text.Size + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	if ( Text.Size != 0 ) {
		memcpy(sCopy, Text.Data, Text.Size);
	}
	sCopy[Text.Size] = '\0';
	return sCopy;
}



/* 返回不含 fragment 的请求目标 URI，保持其他词法形式不变。 */
static xstrview __xrtHttpClientCacheURI(
	const xhttprequest* pRequest
)
{
	xstrview URI = xrtHttpRequestUrlText(pRequest);
	cstr sFragment;

	if ( (URI.Data == NULL) || (URI.Size == 0) ) {
		return (xstrview){ NULL, 0 };
	}
	sFragment = (cstr)memchr(URI.Data, '#', URI.Size);
	if ( sFragment != NULL ) {
		URI.Size = (size_t)(sFragment - URI.Data);
	}
	return URI;
}



/* 释放一跳缓存状态但保留调用级模式和分区。 */
static void __xrtHttpClientCacheReset(
	xhttpcall* pCall
)
{
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	xrtHttpHeadersDestroy(pCall->CacheRequestFields);
	xrtHttpHeadersDestroy(pCall->CacheResponseFields);
	xrtFree(pCall->CacheBody);
	xrtFree(pCall->CacheRanges);
	pCall->CacheCandidate = NULL;
	pCall->CacheRequestFields = NULL;
	pCall->CacheResponseFields = NULL;
	pCall->CacheBody = NULL;
	pCall->CacheRanges = NULL;
	pCall->CacheBodySize = 0;
	pCall->CacheBodyCapacity = 0;
	pCall->CacheRangeCount = 0;
	pCall->CacheRequestClock = 0;
	pCall->CacheResponseClock = 0;
	pCall->CacheRangeBodyLength = 0;
	pCall->CacheResponseTime = 0;
	pCall->CacheRange = (xhttpbyterange){ 0, 0 };
	memset(
		pCall->CacheBoundary,
		0,
		sizeof(pCall->CacheBoundary)
	);
	pCall->CacheRangeState =
		__XRT_HTTP_CLIENT_CACHE_RANGE_NONE;
	pCall->CacheReady = false;
	pCall->CacheCapture = false;
	pCall->CacheRangeRequest = false;
	pCall->CacheRangeCovered = false;
	pCall->CacheRangeFill = false;
	pCall->CacheValidating = false;
	pCall->CacheNotModified = false;
	pCall->CacheIfNoneMatch = false;
	pCall->CacheIfModifiedSince = false;
	pCall->CacheIfRange = false;
	pCall->CacheFailed = false;
}



/* 用指定方法构造借用冻结请求字段的完整缓存主键。 */
static bool __xrtHttpClientCacheKeyMethod(
	xhttpcall* pCall,
	xstrview Method,
	xhttpcachekey* pKey
)
{
	xstrview URI = __xrtHttpClientCacheURI(pCall->Request);

	if ( !xrtHttpCacheKeyInit(
		pKey,
		Method,
		URI
	) ) {
		return false;
	}
	pKey->Partition = (xstrview){
		pCall->CachePartitionKey,
		pCall->CachePartitionSize
	};
	pKey->Fields = xrtHttpHeadersData(
		pCall->CacheRequestFields
	);
	pKey->FieldCount = xrtHttpHeadersCount(
		pCall->CacheRequestFields
	);
	return true;
}



/* 用当前请求方法构造完整缓存主键。 */
static bool __xrtHttpClientCacheKey(
	xhttpcall* pCall,
	xhttpcachekey* pKey
)
{
	return __xrtHttpClientCacheKeyMethod(
		pCall,
		xrtHttpRequestMethod(pCall->Request),
		pKey
	);
}



/* 重新读取指定方法的当前匹配记录并替换调用持有的旧快照。 */
static xhttpcachelookup __xrtHttpClientCacheReload(
	xhttpcall* pCall,
	xstrview Method
)
{
	xhttpcacherecord* pRecord = NULL;
	xhttpcachekey Key;
	xhttpcachelookup Lookup;

	if ( !__xrtHttpClientCacheKeyMethod(
		pCall,
		Method,
		&Key
	) ) {
		return XHTTP_CACHE_LOOKUP_ERROR;
	}
	Lookup = xrtHttpCacheGet(
		pCall->Client->Cache,
		&Key,
		&pRecord
	);
	if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
		return Lookup;
	}
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	pCall->CacheCandidate = pRecord;
	return Lookup;
}



/* 判断记录是否已经无空洞覆盖完整表示。 */
static bool __xrtHttpClientCacheRecordComplete(
	const xhttpcacherecord* pRecord
)
{
	return (xrtHttpCacheRecordFlags(pRecord) &
		XHTTP_CACHE_RECORD_COMPLETE) != 0;
}



/* 判断当前请求是否为可以由 GET 元数据服务的 HEAD。 */
static bool __xrtHttpClientCacheHeadRequest(
	const xhttpcall* pCall
)
{
	return xrtHttpMethodEqual(
		xrtHttpRequestMethod(pCall->Request),
		XRT_STR_LITERAL("HEAD")
	);
}



/* 判断 HEAD 当前选择的是已有 GET 表示，而不是独立 HEAD 记录。 */
static bool __xrtHttpClientCacheHeadFromGet(
	const xhttpcall* pCall
)
{
	const xhttpcachekey* pKey;

	if ( !__xrtHttpClientCacheHeadRequest(pCall) ||
		(pCall->CacheCandidate == NULL) ) {
		return false;
	}
	pKey = xrtHttpCacheRecordKey(pCall->CacheCandidate);
	return (pKey != NULL) &&
		xrtHttpMethodEqual(
			pKey->Method,
			XRT_STR_LITERAL("GET")
		);
}



/* 判断请求 If-Range 是否与缓存记录的强验证器匹配。 */
static bool __xrtHttpClientCacheIfRangeMatch(
	const xhttpcacherecord* pRecord,
	const xhttpfield* pField,
	bool* pMatch
)
{
	xhttpcacheentry Entry;
	xhttpcacheifrange Plan;
	xhttpcacheifrangekind Kind;
	xhttprepresentation Current;

	*pMatch = pField == NULL;
	if ( pField == NULL ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pRecord,
		&Entry
	) ) {
		return false;
	}
	Kind = xrtHttpCacheIfRangePlan(&Entry, &Plan);
	if ( Kind == XHTTP_CACHE_IF_RANGE_ERROR ) {
		return false;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_NONE ) {
		return true;
	}

	memset(&Current, 0, sizeof(Current));
	Current.Exists = true;
	if ( Kind == XHTTP_CACHE_IF_RANGE_ETAG ) {
		Current.HasETag = true;
		Current.ETag = Plan.ETag;
	} else {
		Current.HasLastModified = true;
		Current.LastModifiedStrong = true;
		Current.LastModified = Plan.Date;
	}
	*pMatch = xrtHttpIfRangeMatch(
		pField->Value,
		&Current
	);
	return true;
}



/* 判断有序正文片段是否连续覆盖请求闭区间。 */
static bool __xrtHttpClientCacheRangeCovered(
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRange
)
{
	uint64 iNext = pRange->First;
	size_t i;

	for ( i = 0;
		i < xrtHttpCacheRecordPartCount(pRecord);
		i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		uint64 iLast;

		if ( pPart == NULL ) {
			return false;
		}
		iLast = pPart->Offset +
			(uint64)pPart->Data.Size -
			UINT64_C(1);
		if ( iLast < iNext ) {
			continue;
		}
		if ( pPart->Offset > iNext ) {
			return false;
		}
		if ( iLast >= pRange->Last ) {
			return true;
		}
		iNext = iLast + UINT64_C(1);
	}
	return false;
}



/* 返回调用当前规范化范围数组；单范围直接使用内联存储。 */
static const xhttpbyterange* __xrtHttpClientCacheRanges(
	const xhttpcall* pCall
)
{
	if ( pCall->CacheRangeCount == 1u ) {
		return &pCall->CacheRange;
	}
	return pCall->CacheRanges;
}



/* 判断缓存记录是否完整覆盖全部规范化请求范围。 */
static bool __xrtHttpClientCacheRangesCovered(
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRanges,
	size_t iRangeCount
)
{
	size_t i;

	for ( i = 0; i < iRangeCount; i++ ) {
		if ( !__xrtHttpClientCacheRangeCovered(
			pRecord,
			&pRanges[i]
		) ) {
			return false;
		}
	}
	return true;
}



/* 返回缓存表示的原始媒体类型，缺失时由 multipart 编码器采用默认值。 */
static xstrview __xrtHttpClientCacheContentType(
	const xhttpcacherecord* pRecord
)
{
	const xhttpfield* pField = xrtHttpCacheRecordField(
		pRecord,
		XRT_STR_LITERAL("Content-Type")
	);

	return pField != NULL ?
		pField->Value :
		(xstrview){ NULL, 0 };
}



/* 为一次本地 multipart/byteranges 重放生成 128 位随机 token boundary。 */
static bool __xrtHttpClientCacheBoundary(
	xhttpcall* pCall
)
{
	uint8 Random[XRT_HTTP_CLIENT_CACHE_BOUNDARY_RANDOM_SIZE];
	size_t iEncoded = 0;

	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		memset(Random, 0, sizeof(Random));
		return false;
	}
	memcpy(
		pCall->CacheBoundary,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE
	);
	if ( !xrtHexEncode(
			Random,
			sizeof(Random),
			pCall->CacheBoundary +
				XRT_HTTP_CLIENT_CACHE_BOUNDARY_PREFIX_SIZE,
			(sizeof(Random) * 2u) + 1u,
			&iEncoded,
			0
		) || (iEncoded != (sizeof(Random) * 2u)) ) {
		memset(Random, 0, sizeof(Random));
		memset(
			pCall->CacheBoundary,
			0,
			sizeof(pCall->CacheBoundary)
		);
		return false;
	}
	memset(Random, 0, sizeof(Random));
	pCall->CacheBoundary[
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	] = '\0';
	return true;
}



/*
	把 bytes Range 集合解析为缓存交付状态。
	非法、重复、超限和未知完整长度均保守回源，不改变请求语义。
*/
static bool __xrtHttpClientCacheRangePrepare(
	xhttpcall* pCall,
	bool* pUsable
)
{
	const xhttpfield* pFields = xrtHttpHeadersData(
		pCall->CacheRequestFields
	);
	size_t iCount = xrtHttpHeadersCount(
		pCall->CacheRequestFields
	);
	const xhttpfield* pRange;
	const xhttpfield* pIfRange;
	xstrview Unit;
	xstrview Set;
	xstrview ContentType;
	xhttpbyterange* pResolved;
	xhttprangeresult Result;
	xhttpnext RangeNext;
	xhttpnext IfRangeNext;
	size_t iInputCount = 0;
	size_t iResolved = 0;
	uint32 iFlags = xrtHttpCacheRecordFlags(
		pCall->CacheCandidate
	);
	bool bComplete =
		__xrtHttpClientCacheRecordComplete(
			pCall->CacheCandidate
		);
	bool bIfRange;

	*pUsable = bComplete;
	if ( !xrtHttpMethodEqual(
			xrtHttpRequestMethod(pCall->Request),
			XRT_STR_LITERAL("GET")
		) || (xrtHttpCacheRecordStatus(
			pCall->CacheCandidate
		 ) != XHTTP_STATUS_OK) ) {
		return true;
	}

	RangeNext = xrtHttpFieldGetUnique(
		pFields,
		iCount,
		XRT_STR_LITERAL("Range"),
		&pRange
	);
	if ( RangeNext != XHTTP_NEXT_ITEM ) {
		if ( RangeNext == XHTTP_NEXT_ERROR ) {
			xrtClearError();
			pCall->CacheRangeRequest = true;
			*pUsable = false;
		}
		return true;
	}
	pCall->CacheRangeRequest = true;

	if ( !xrtHttpRangeParse(
			pRange->Value,
			&Unit,
			&Set
		) || !xrtHttpTokenEqual(
			Unit,
			XRT_STR_LITERAL("bytes")
		) || !xrtHttpByteRangeCount(
			Set,
			&iInputCount
		) ) {
		xrtClearError();
		*pUsable = false;
		return true;
	}
	if ( iInputCount >
		pCall->Client->Config.Cache.MaxRanges ) {
		*pUsable = false;
		return true;
	}
	if ( (iFlags &
		  XHTTP_CACHE_RECORD_HAS_LENGTH) == 0 ) {
		*pUsable = false;
		return true;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( pCall->DecompressEnabled ) {
			xhttpcacheentry Entry;
			xhttpcontentencodingplan Encoding;

			if ( !xrtHttpCacheRecordEntry(
				pCall->CacheCandidate,
				&Entry
			) || !xrtHttpContentEncodingPlan(
				Entry.Fields,
				Entry.FieldCount,
				&Encoding
			) ) {
				return false;
			}
			if ( (Encoding.DecoderCount != 0) ||
				(Encoding.UnknownCount != 0) ) {
				*pUsable = false;
				return true;
			}
		}
	#endif

	IfRangeNext = xrtHttpFieldGetUnique(
		pFields,
		iCount,
		XRT_STR_LITERAL("If-Range"),
		&pIfRange
	);
	if ( IfRangeNext == XHTTP_NEXT_ERROR ) {
		xrtClearError();
		bIfRange = false;
	} else if ( !__xrtHttpClientCacheIfRangeMatch(
		pCall->CacheCandidate,
		pIfRange,
		&bIfRange
	) ) {
		return false;
	}
	if ( !bIfRange ) {
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}

	pResolved = iInputCount == 1u ?
		&pCall->CacheRange :
		(xhttpbyterange*)xrtMalloc(
			iInputCount * sizeof(*pResolved)
		);
	if ( pResolved == NULL ) {
		return false;
	}
	Result = xrtHttpByteRangesResolve(
		Set,
		xrtHttpCacheRecordLength(
			pCall->CacheCandidate
		),
		pResolved,
		iInputCount,
		0,
		&iResolved,
		&pCall->CacheRangeBodyLength
	);
	if ( Result == XHTTP_RANGE_ERROR ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		return false;
	}
	if ( Result == XHTTP_RANGE_EMPTY ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}
	if ( Result == XHTTP_RANGE_UNSATISFIED ) {
		if ( pResolved != &pCall->CacheRange ) {
			xrtFree(pResolved);
		}
		pCall->CacheRangeState =
			__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE;
		pCall->CacheRangeCovered = bComplete;
		*pUsable = bComplete;
		return true;
	}

	if ( iResolved == 1u ) {
		if ( pResolved != &pCall->CacheRange ) {
			pCall->CacheRange = pResolved[0];
			xrtFree(pResolved);
		}
	} else {
		pCall->CacheRanges = pResolved;
	}
	pCall->CacheRangeCount = iResolved;
	pCall->CacheRangeCovered =
		__xrtHttpClientCacheRangesCovered(
			pCall->CacheCandidate,
			__xrtHttpClientCacheRanges(pCall),
			pCall->CacheRangeCount
		);
	pCall->CacheRangeFill =
		(pCall->CacheRangeCount == 1u) &&
		!pCall->CacheRangeCovered;
	if ( pCall->CacheRangeCovered ) {
		if ( pCall->CacheRangeCount == 1u ) {
			pCall->CacheRangeState =
				__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL;
		} else {
			ContentType =
				__xrtHttpClientCacheContentType(
					pCall->CacheCandidate
				);
			if ( !__xrtHttpClientCacheBoundary(pCall) ||
				!xrtHttpRangeMultipartLength(
					__xrtHttpClientCacheRanges(pCall),
					pCall->CacheRangeCount,
					xrtHttpCacheRecordLength(
						pCall->CacheCandidate
					),
					ContentType,
					(xstrview){
						pCall->CacheBoundary,
						XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
					},
					&pCall->CacheRangeBodyLength
				) ) {
				return false;
			}
			pCall->CacheRangeState =
				__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART;
		}
	}
	*pUsable = pCall->CacheRangeCovered;
	return true;
}



/* 解析一组字段中的 Cache-Control，非法值保守地禁用本次缓存路径。 */
static bool __xrtHttpClientCacheControl(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcachecontrol* pControl
)
{
	xrtHttpCacheControlInit(pControl);
	return xrtHttpCacheControlParse(
		pFields,
		iCount,
		pControl
	);
}



/* 从唯一有效 Last-Modified 计算受限启发式新鲜寿命。 */
static bool __xrtHttpClientCacheHeuristic(
	xhttpcall* pCall,
	const xhttpcacheentry* pEntry,
	const xhttpcachetime* pTime,
	uint16 iStatus,
	xhttpcachefreshness* pFreshness
)
{
	const xhttpfield* pModified = NULL;
	xtime iModified;
	xtime iReference;
	uint64 iDifference;
	uint64 iLifetime;
	size_t i;

	if ( !pCall->Client->Config.Cache.Heuristic ||
		!xrtHttpCacheStatusHeuristic(iStatus) ) {
		return false;
	}
	for ( i = 0; i < pEntry->FieldCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pEntry->Fields[i].Name,
			XRT_STR_LITERAL("Last-Modified")
		) ) {
			if ( pModified != NULL ) {
				return false;
			}
			pModified = &pEntry->Fields[i];
		}
	}
	if ( (pModified == NULL) ||
		!xrtTimeParseHTTPDate(
			pModified->Value,
			&iModified
		) ) {
		xrtClearError();
		return false;
	}
	iReference =
		((pTime->Flags & XHTTP_CACHE_TIME_DATE) != 0) ?
			pTime->Date :
			xrtHttpCacheRecordResponseTime(
				pCall->CacheCandidate
			);
	if ( iModified >= iReference ) {
		return false;
	}
	iDifference = (uint64)(iReference - iModified);
	iLifetime =
		(iDifference / 100u) *
			pCall->Client->Config.Cache.HeuristicPercent;
	iLifetime +=
		((iDifference % 100u) *
		 pCall->Client->Config.Cache.HeuristicPercent) / 100u;
	if ( iLifetime >
		pCall->Client->Config.Cache.HeuristicMax ) {
		iLifetime =
			pCall->Client->Config.Cache.HeuristicMax;
	}
	pFreshness->Lifetime = iLifetime;
	pFreshness->Source = XHTTP_CACHE_FRESHNESS_HEURISTIC;
	return true;
}



/* 计算一个候选在当前时刻的年龄、寿命和复用决定。 */
static xhttpcacheusedecision __xrtHttpClientCacheUse(
	xhttpcall* pCall,
	const xhttpcachecontrol* pRequest,
	xhttpcacheuseplan* pPlan
)
{
	xhttpcacheentry Entry;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xhttpcachefreshness Freshness;
	xhttpcacheuseinput Input;
	xhttpcachecalc AgeResult;
	xhttpcachecalc FreshnessResult;
	uint16 iStatus = xrtHttpCacheRecordStatus(
		pCall->CacheCandidate
	);

	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) || !__xrtHttpClientCacheControl(
		Entry.Fields,
		Entry.FieldCount,
		&Response
	) || !xrtHttpCacheTimeParse(
		Entry.Fields,
		Entry.FieldCount,
		&Time
	) ) {
		return XHTTP_CACHE_USE_ERROR;
	}
	AgeResult = xrtHttpCacheCurrentAge(
		&Time,
		xrtHttpCacheRecordResponseTime(
			pCall->CacheCandidate
		),
		xrtHttpCacheRecordRequestClock(
			pCall->CacheCandidate
		),
		xrtHttpCacheRecordResponseClock(
			pCall->CacheCandidate
		),
		xrtTimer(),
		&Age
	);
	FreshnessResult = xrtHttpCacheFreshness(
		&Response,
		&Time,
		xrtHttpCacheRecordResponseTime(
			pCall->CacheCandidate
		),
		pCall->Client->Config.Cache.Shared,
		&Freshness
	);
	if ( (FreshnessResult == XHTTP_CACHE_CALC_NONE) &&
		__xrtHttpClientCacheHeuristic(
			pCall,
			&Entry,
			&Time,
			iStatus,
			&Freshness
		) ) {
		FreshnessResult = XHTTP_CACHE_CALC_READY;
	}
	if ( (AgeResult != XHTTP_CACHE_CALC_READY) ||
		(FreshnessResult != XHTTP_CACHE_CALC_READY) ||
		!xrtHttpCacheUseInputInit(
			&Input,
			iStatus,
			pCall->Client->Config.Cache.Shared
		) ) {
		memset(pPlan, 0, sizeof(*pPlan));
		pPlan->Decision = XHTTP_CACHE_USE_VALIDATE;
		return pPlan->Decision;
	}
	if ( (xrtHttpCacheRecordFlags(
			pCall->CacheCandidate
		 ) & XHTTP_CACHE_RECORD_COMPLETE) == 0 ) {
		if ( !pCall->CacheRangeCovered ) {
			Input.Flags &=
				~(uint32)XHTTP_CACHE_USE_REPRESENTATION;
		}
	}
	if ( pCall->CacheMode == XHTTP_CLIENT_CACHE_ONLY ) {
		Input.Flags |= XHTTP_CACHE_USE_DISCONNECTED;
	}
	if ( xrtHttpRequestHeader(
		pCall->Request,
		XRT_STR_LITERAL("Authorization")
	) != NULL ) {
		Input.Flags |= XHTTP_CACHE_USE_AUTHORIZATION;
	}
	return xrtHttpCacheUsePlan(
		pRequest,
		&Response,
		&Age,
		&Freshness,
		&Input,
		pPlan
	);
}



/* 移除仅由缓存验证路径添加的条件字段，避免跨重定向泄漏。 */
static void __xrtHttpClientCacheValidatorsRemove(
	xhttpcall* pCall
)
{
	if ( pCall->CacheIfNoneMatch ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match")
		);
		pCall->CacheIfNoneMatch = false;
	}
	if ( pCall->CacheIfModifiedSince ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since")
		);
		pCall->CacheIfModifiedSince = false;
	}
	if ( pCall->CacheIfRange ) {
		(void)xrtHttpRequestRemoveHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Range")
		);
		pCall->CacheIfRange = false;
	}
}



/* 为单个候选添加最强可用的条件验证字段。 */
static bool __xrtHttpClientCacheValidate(
	xhttpcall* pCall
)
{
	xhttpcacheentry Entry;
	xhttpcachevalidateplan Plan;
	xhttpcachevalidatedecision Decision;
	char sDate[64];
	size_t iDate;
	str sTags = NULL;
	size_t iTags = 0;

	if ( (xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match")
		 ) != NULL) ||
		(xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since")
		 ) != NULL) ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return false;
	}
	if ( pCall->CacheRangeCovered ) {
		Entry.Flags |=
			XHTTP_CACHE_ENTRY_RANGE_COVERED;
	}
	Decision = xrtHttpCacheValidatePlan(
		&Entry,
		1,
		pCall->CacheRangeRequest,
		&Plan
	);
	if ( Decision == XHTTP_CACHE_VALIDATE_ERROR ) {
		return false;
	}
	if ( Decision != XHTTP_CACHE_VALIDATE_CONDITIONAL ) {
		return true;
	}
	if ( (Plan.Actions &
		XHTTP_CACHE_VALIDATE_IF_NONE_MATCH) != 0 ) {
		if ( Plan.ETagSize == SIZE_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		sTags = (str)xrtMalloc(Plan.ETagSize + 1u);
		if ( sTags == NULL ) {
			return false;
		}
		if ( !xrtHttpCacheValidateETagsWrite(
			&Entry,
			1,
			pCall->CacheRangeRequest,
			sTags,
			Plan.ETagSize,
			&iTags
		) || (iTags != Plan.ETagSize) ) {
			xrtFree(sTags);
			return false;
		}
		sTags[iTags] = '\0';
		if ( !xrtHttpRequestSetHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-None-Match"),
			(xstrview){ sTags, iTags }
		) ) {
			xrtFree(sTags);
			return false;
		}
		xrtFree(sTags);
		pCall->CacheIfNoneMatch = true;
	}
	if ( (Plan.Actions &
		XHTTP_CACHE_VALIDATE_IF_MODIFIED_SINCE) != 0 ) {
		iDate = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			Plan.LastModified
		);
		if ( iDate == XRT_NPOS ) {
			return false;
		}
		if ( !xrtHttpRequestSetHeader(
			pCall->Request,
			XRT_STR_LITERAL("If-Modified-Since"),
			(xstrview){ sDate, iDate }
		) ) {
			return false;
		}
		pCall->CacheIfModifiedSince = true;
	}
	pCall->CacheValidating =
		pCall->CacheIfNoneMatch ||
		pCall->CacheIfModifiedSince;
	return true;
}



/* 为缺失范围回源添加仅属于缓存实现的强 If-Range。 */
static bool __xrtHttpClientCacheIfRange(
	xhttpcall* pCall
)
{
	xhttpcacheentry Entry;
	xhttpcacheifrange Plan;
	xhttpcacheifrangekind Kind;
	char sDate[64];
	str sTag = NULL;
	xstrview Value;
	size_t iSize;

	if ( xrtHttpRequestHeader(
		pCall->Request,
		XRT_STR_LITERAL("If-Range")
	) != NULL ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return false;
	}
	Kind = xrtHttpCacheIfRangePlan(&Entry, &Plan);
	if ( Kind == XHTTP_CACHE_IF_RANGE_ERROR ) {
		return false;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_NONE ) {
		return true;
	}
	if ( Kind == XHTTP_CACHE_IF_RANGE_ETAG ) {
		sTag = xrtHttpETagBuild(
			&Plan.ETag,
			&iSize
		);
		if ( sTag == NULL ) {
			return false;
		}
		Value = (xstrview){ sTag, iSize };
	} else {
		iSize = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			Plan.Date
		);
		if ( iSize == XRT_NPOS ) {
			return false;
		}
		Value = (xstrview){ sDate, iSize };
	}
	if ( !xrtHttpRequestSetHeader(
		pCall->Request,
		XRT_STR_LITERAL("If-Range"),
		Value
	) ) {
		xrtFree(sTag);
		return false;
	}
	xrtFree(sTag);
	pCall->CacheIfRange = true;
	return true;
}



/* 判断请求控制是否明确禁止任何缓存存取。 */
static bool __xrtHttpClientCacheNoStore(
	const xhttpcachecontrol* pControl
)
{
	return (pControl->Flags & XHTTP_CACHE_NO_STORE) != 0;
}



/* 判断请求控制是否禁止回源。 */
static bool __xrtHttpClientCacheOnly(
	const xhttpcachecontrol* pControl
)
{
	return (pControl->Flags &
		XHTTP_CACHE_ONLY_IF_CACHED) != 0;
}



/* 初始化禁用存储的安全客户端缓存默认值。 */
XHTTP_CLIENT_API void xrtHttpClientCacheConfigInit(
	xhttpclientcacheconfig* pConfig
)
{
	xhttpclientcacheconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-cache-config",
			"HTTP client cache config storage is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.MaxBody = XHTTP_CLIENT_CACHE_BODY_DEFAULT;
	Config.HeuristicMax =
		XHTTP_CLIENT_CACHE_HEURISTIC_MAX_DEFAULT;
	Config.HeuristicPercent =
		XHTTP_CLIENT_CACHE_HEURISTIC_PERCENT_DEFAULT;
	Config.MaxRanges =
		XHTTP_CLIENT_CACHE_MAX_RANGES_DEFAULT;
	Config.Heuristic = true;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 初始化继承 Client 且未分区的调用级缓存选项。 */
XHTTP_CLIENT_API void xrtHttpClientCacheOptionsInit(
	xhttpclientcacheoptions* pOptions
)
{
	const xhttpclientcacheoptions Options = {
		XHTTP_CLIENT_CACHE_DEFAULT,
		{ NULL, 0 }
	};

	if ( !xrtMemRangeValid(pOptions, sizeof(Options)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_ARGUMENT,
			"init-http-client-cache-options",
			"HTTP client cache options storage is invalid",
			NULL
		);
		return;
	}
	memcpy(pOptions, &Options, sizeof(Options));
}



/* 验证配置并让 Client 保留统一 Cache 句柄。 */
bool __xrtHttpClientCacheOpen(xhttpclient* pClient)
{
	const xhttpclientcacheconfig* pConfig =
		&pClient->Config.Cache;

	if ( (pConfig->MaxBody == 0) ||
		(pConfig->MaxRanges == 0) ||
		(pConfig->MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(pConfig->HeuristicPercent > 100u) ||
		(pConfig->Heuristic &&
		 (pConfig->HeuristicMax == 0)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client-cache",
			"HTTP client cache limits are invalid",
			NULL
		);
		return false;
	}
	if ( pConfig->Store == NULL ) {
		return true;
	}
	pClient->Cache = xrtHttpCacheRetain(pConfig->Store);
	if ( pClient->Cache == NULL ) {
		__xrtHttpClientSetError(
			XERR_STATE,
			XHTTP_CLIENT_ERROR_CONFIG,
			"configure-http-client-cache",
			"HTTP client cache could not be retained",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 释放 Client 持有的统一 Cache 句柄。 */
void __xrtHttpClientCacheClose(xhttpclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	xrtHttpCacheRelease(pClient->Cache);
	pClient->Cache = NULL;
}



/* 返回 Client 借用的统一缓存句柄。 */
XHTTP_CLIENT_API xhttpcache* xrtHttpClientCache(
	const xhttpclient* pClient
)
{
	return pClient != NULL ? pClient->Cache : NULL;
}



/* 冻结调用级缓存模式和分区文本。 */
bool __xrtHttpClientCacheInit(
	xhttpcall* pCall,
	const xhttpcalloptions* pOptions
)
{
	xstrview Partition = pOptions->Cache.PartitionKey;

	if ( (pOptions->Cache.Mode <
		  XHTTP_CLIENT_CACHE_DEFAULT) ||
		(pOptions->Cache.Mode >
		  XHTTP_CLIENT_CACHE_ONLY) ||
		((Partition.Data == NULL) &&
		 (Partition.Size != 0)) ) {
		__xrtHttpClientSetError(
			XERR_ARGUMENT,
			XHTTP_CLIENT_ERROR_CACHE,
			"configure-http-call-cache",
			"HTTP call cache options are invalid",
			NULL
		);
		return false;
	}
	pCall->CachePartitionKey =
		__xrtHttpClientCacheText(Partition);
	if ( pCall->CachePartitionKey == NULL ) {
		return false;
	}
	pCall->CachePartitionSize = Partition.Size;
	pCall->CacheMode = pOptions->Cache.Mode;
	pCall->CacheEnabled =
		(pCall->Client->Cache != NULL) &&
		(pCall->CacheMode !=
		 XHTTP_CLIENT_CACHE_DISABLED);
	if ( !pCall->CacheEnabled ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
	}
	return true;
}



/* 释放 Call 的全部缓存状态。 */
void __xrtHttpClientCacheUnit(xhttpcall* pCall)
{
	if ( pCall == NULL ) {
		return;
	}
	__xrtHttpClientCacheReset(pCall);
	xrtFree(pCall->CachePartitionKey);
	pCall->CachePartitionKey = NULL;
	pCall->CachePartitionSize = 0;
}



/* 冻结有效请求字段并选择命中、验证或回源路径。 */
bool __xrtHttpClientCachePrepare(xhttpcall* pCall)
{
	xhttpcachecontrol Request;
	xhttpcacheuseplan Plan;
	xhttpcacheusedecision Decision;
	xhttpcachelookup Lookup;
	const xhttpheaders* pHeaders;
	bool bHead;
	bool bOnly;
	bool bUsable;

	/* 重试可能发生在响应 Header 到达前，先撤销上一跳自动条件字段。 */
	__xrtHttpClientCacheValidatorsRemove(pCall);
	__xrtHttpClientCacheReset(pCall);
	if ( !pCall->CacheEnabled ) {
		return true;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_TRAILERS)
		if ( xrtHttpRequestTrailerCount(
			pCall->Request
		) != 0 ) {
			pCall->CacheCapture = false;
			if ( pCall->CacheMode ==
				XHTTP_CLIENT_CACHE_ONLY ) {
				pCall->CacheReady = true;
				__xrtHttpClientCacheOutcome(
					pCall,
					XHTTP_CLIENT_CACHE_ONLY_MISS
				);
			} else {
				__xrtHttpClientCacheOutcome(
					pCall,
					XHTTP_CLIENT_CACHE_BYPASS
				);
			}
			return true;
		}
	#endif
	pHeaders = xrtHttpRequestHeaders(pCall->Request);
	pCall->CacheRequestFields = xrtHttpHeadersClone(pHeaders);
	if ( pCall->CacheRequestFields == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"snapshot-http-cache-request",
			"HTTP cache request fields could not be copied"
		);
	}
	if ( !__xrtHttpClientCacheControl(
		xrtHttpHeadersData(pCall->CacheRequestFields),
		xrtHttpHeadersCount(pCall->CacheRequestFields),
		&Request
	) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"parse-http-cache-request",
			"HTTP request Cache-Control is invalid"
		) ) {
			return false;
		}
		pCall->CacheCapture = false;
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
		return true;
	}
	if ( __xrtHttpClientCacheNoStore(&Request) ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_BYPASS
		);
		return true;
	}
	bOnly =
		(pCall->CacheMode == XHTTP_CLIENT_CACHE_ONLY) ||
		__xrtHttpClientCacheOnly(&Request);
	pCall->CacheCapture =
		pCall->CacheMode != XHTTP_CLIENT_CACHE_ONLY;
	bHead = __xrtHttpClientCacheHeadRequest(pCall);
	Lookup = __xrtHttpClientCacheReload(
		pCall,
		bHead ?
			XRT_STR_LITERAL("GET") :
			xrtHttpRequestMethod(pCall->Request)
	);
	if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"lookup-http-cache",
			"HTTP cache lookup failed"
		) ) {
			return false;
		}
		Lookup = XHTTP_CACHE_LOOKUP_MISS;
	}
	if ( bHead &&
		(Lookup == XHTTP_CACHE_LOOKUP_MISS) ) {
		Lookup = __xrtHttpClientCacheReload(
			pCall,
			XRT_STR_LITERAL("HEAD")
		);
		if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"lookup-http-head-cache",
				"HTTP HEAD cache lookup failed"
			) ) {
				return false;
			}
			Lookup = XHTTP_CACHE_LOOKUP_MISS;
		}
	}
	if ( Lookup == XHTTP_CACHE_LOOKUP_MISS ) {
		if ( bOnly ) {
			pCall->CacheReady = true;
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_ONLY_MISS
			);
		} else {
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_MISS
			);
		}
		return true;
	}
	if ( !__xrtHttpClientCacheRangePrepare(
		pCall,
		&bUsable
	) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-range",
			"HTTP cache range selection failed"
		) ) {
			return false;
		}
		bUsable = false;
	}
	if ( !bUsable ) {
		if ( bOnly ) {
			xrtHttpCacheRecordRelease(
				pCall->CacheCandidate
			);
			pCall->CacheCandidate = NULL;
			pCall->CacheReady = true;
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_ONLY_MISS
			);
			return true;
		}
		if ( pCall->CacheRangeFill &&
			!__xrtHttpClientCacheIfRange(pCall) ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"prepare-http-cache-if-range",
				"HTTP cache If-Range could not be prepared"
			) ) {
				return false;
			}
		}
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_MISS
		);
		return true;
	}
	Decision = __xrtHttpClientCacheUse(
		pCall,
		&Request,
		&Plan
	);
	if ( Decision == XHTTP_CACHE_USE_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-use",
			"HTTP cache reuse plan failed"
		) ) {
			return false;
		}
		Decision = XHTTP_CACHE_USE_FORWARD;
	}
	if ( (Decision == XHTTP_CACHE_USE_STORED) &&
		(pCall->CacheMode !=
		 XHTTP_CLIENT_CACHE_RELOAD) ) {
		pCall->CacheReady = true;
		__xrtHttpClientCacheOutcome(
			pCall,
			((Plan.Actions &
			  XHTTP_CACHE_USE_STALE) != 0) ?
				XHTTP_CLIENT_CACHE_STALE :
				XHTTP_CLIENT_CACHE_HIT
		);
		return true;
	}
	if ( (Decision ==
		  XHTTP_CACHE_USE_GATEWAY_TIMEOUT) ||
		(bOnly &&
		 (Decision != XHTTP_CACHE_USE_STORED)) ) {
		xrtHttpCacheRecordRelease(
			pCall->CacheCandidate
		);
		pCall->CacheCandidate = NULL;
		pCall->CacheReady = true;
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_ONLY_MISS
		);
		return true;
	}
	if ( (Decision == XHTTP_CACHE_USE_VALIDATE) ||
		(pCall->CacheMode ==
		 XHTTP_CLIENT_CACHE_RELOAD) ) {
		if ( !__xrtHttpClientCacheValidate(pCall) ) {
			/* SetHeader 可能已提交第一个验证器，宽松回源前必须回滚。 */
			__xrtHttpClientCacheValidatorsRemove(pCall);
			pCall->CacheValidating = false;
			return __xrtHttpClientCacheRecover(
				pCall,
				"prepare-http-cache-validation",
				"HTTP cache validators could not be prepared"
			);
		}
	}
	return true;
}



/* 为未知长度正文增长自适应暂存，不使用固定对象缓冲。 */
static bool __xrtHttpClientCacheBodyAppend(
	xhttpcall* pCall,
	xbytesview Data
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pBody;

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( (pCall->CacheBodySize >
		  (SIZE_MAX - Data.Size)) ||
		((uint64)Data.Size >
		 (pCall->Client->Config.Cache.MaxBody -
		  (uint64)pCall->CacheBodySize)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iRequired = pCall->CacheBodySize + Data.Size;
	iCapacity = pCall->CacheBodyCapacity;
	if ( iRequired > iCapacity ) {
		if ( iCapacity == 0 ) {
			iCapacity =
				iRequired >
				XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY ?
					iRequired :
					XRT_HTTP_CLIENT_CACHE_BODY_MIN_CAPACITY;
		}
		while ( iCapacity < iRequired ) {
			size_t iNext = iCapacity +
				(iCapacity >> 1u);

			if ( iNext <= iCapacity ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity = iNext;
		}
		if ( (uint64)iCapacity >
			pCall->Client->Config.Cache.MaxBody ) {
			iCapacity = (size_t)
				pCall->Client->Config.Cache.MaxBody;
		}
		pBody = (bytes)xrtRealloc(
			pCall->CacheBody,
			iCapacity
		);
		if ( pBody == NULL ) {
			return false;
		}
		pCall->CacheBody = pBody;
		pCall->CacheBodyCapacity = iCapacity;
	}
	memcpy(
		pCall->CacheBody + pCall->CacheBodySize,
		Data.Data,
		Data.Size
	);
	pCall->CacheBodySize = iRequired;
	return true;
}



/* 缓存观察器透传信息响应，不把它们保存为表示。 */
static bool __xrtHttpClientCacheInformational(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CacheNext.Informational == NULL ) {
		return true;
	}
	return pCall->CacheNext.Informational(
		pResponse,
		pCall->CacheNext.Data
	);
}



/* 在下游改写前快照原始响应 Header，并吞掉自有验证产生的 304。 */
static bool __xrtHttpClientCacheHeaders(
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);

	pCall->CacheResponseClock = xrtTimer();
	pCall->CacheResponseTime = xrtNow();
	if ( pCall->CacheValidating &&
		(xrtHttpResponseStatus(pResponse) ==
		 XHTTP_STATUS_NOT_MODIFIED) ) {
		pCall->CacheResponseFields =
			xrtHttpHeadersClone(pHeaders);
		if ( pCall->CacheResponseFields == NULL ) {
			return __xrtHttpClientCacheError(
				pCall,
				XERR_MEMORY,
				"copy-http-cache-validation",
				"HTTP 304 fields could not be copied",
				xrtGetError()
			);
		}
		pCall->CacheNotModified = true;
		pCall->CacheCapture = false;
		__xrtHttpClientCacheValidatorsRemove(pCall);
		return true;
	}
	__xrtHttpClientCacheValidatorsRemove(pCall);
	if ( pCall->CacheCapture ) {
		pCall->CacheResponseFields =
			xrtHttpHeadersClone(pHeaders);
		if ( pCall->CacheResponseFields == NULL ) {
			if ( !__xrtHttpClientCacheRecover(
				pCall,
				"copy-http-cache-response",
				"HTTP cache response fields could not be copied"
			) ) {
				return false;
			}
			pCall->CacheCapture = false;
		}
	}
	if ( pCall->CacheNext.Headers == NULL ) {
		return true;
	}
	return pCall->CacheNext.Headers(
		pResponse,
		pCall->CacheNext.Data
	);
}



/* 暂存原始编码正文后按原顺序转发给重定向和解压链。 */
static bool __xrtHttpClientCacheBody(
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpcall* pCall = (xhttpcall*)pData;

	if ( pCall->CacheCapture &&
		!__xrtHttpClientCacheBodyAppend(
			pCall,
			Data
		) ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"buffer-http-cache-response",
			"HTTP cache response body exceeded its limit or allocation failed"
		) ) {
			return false;
		}
		pCall->CacheCapture = false;
		xrtHttpHeadersDestroy(
			pCall->CacheResponseFields
		);
		pCall->CacheResponseFields = NULL;
		xrtFree(pCall->CacheBody);
		pCall->CacheBody = NULL;
		pCall->CacheBodySize = 0;
		pCall->CacheBodyCapacity = 0;
	}
	if ( pCall->CacheNext.Body == NULL ) {
		return true;
	}
	return pCall->CacheNext.Body(
		pResponse,
		Data,
		pCall->CacheNext.Data
	);
}



/* 构造保存原始表示的逐跳事件观察器。 */
const xhttp1exchangeevents* __xrtHttpClientCacheEvents(
	xhttpcall* pCall,
	const xhttp1exchangeevents* pNext
)
{
	if ( !pCall->CacheEnabled ) {
		return pNext;
	}
	pCall->CacheNext = *pNext;
	memset(
		&pCall->CacheEvents,
		0,
		sizeof(pCall->CacheEvents)
	);
	pCall->CacheEvents.Informational =
		__xrtHttpClientCacheInformational;
	pCall->CacheEvents.Headers =
		__xrtHttpClientCacheHeaders;
	pCall->CacheEvents.Body =
		__xrtHttpClientCacheBody;
	pCall->CacheEvents.Data = pCall;
	return &pCall->CacheEvents;
}



/* 构造只包含 StorePlan 允许字段的借用数组。 */
static xhttpfield* __xrtHttpClientCacheStoredFields(
	const xhttpheaders* pHeaders,
	bool Shared,
	uint32 iActions,
	size_t* pCount
)
{
	const xhttpfield* pFields = xrtHttpHeadersData(pHeaders);
	size_t iCount = xrtHttpHeadersCount(pHeaders);
	xhttpfield* pStored;
	size_t iStored = 0;
	size_t i;

	*pCount = 0;
	if ( iCount == 0 ) {
		return NULL;
	}
	if ( iCount > (SIZE_MAX / sizeof(*pStored)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	pStored = (xhttpfield*)xrtMalloc(
		iCount * sizeof(*pStored)
	);
	if ( pStored == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		xhttpcachefieldstore Result =
			xrtHttpCacheFieldStore(
				pFields,
				iCount,
				i,
				Shared,
				iActions
			);

		if ( Result == XHTTP_CACHE_FIELD_STORE_ERROR ) {
			xrtFree(pStored);
			return NULL;
		}
		if ( Result == XHTTP_CACHE_FIELD_STORE_KEEP ) {
			pStored[iStored] = pFields[i];
			iStored++;
		}
	}
	*pCount = iStored;
	return pStored;
}



/* 把借用字段数组复制到可规范化的 Header 容器。 */
static xhttpheaders* __xrtHttpClientCacheHeadersCreate(
	const xhttpfield* pFields,
	size_t iCount
)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	size_t i;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !xrtHttpHeadersAdd(
			pHeaders,
			pFields[i].Name,
			pFields[i].Value
		) ) {
			xrtHttpHeadersDestroy(pHeaders);
			return NULL;
		}
	}
	return pHeaders;
}



/* 用十进制完整长度设置规范化 Content-Length。 */
static bool __xrtHttpClientCacheContentLength(
	xhttpheaders* pHeaders,
	uint64 iLength
)
{
	char sLength[32];
	int iSize = snprintf(
		sLength,
		sizeof(sLength),
		"%llu",
		(unsigned long long)iLength
	);

	if ( (iSize < 0) ||
		((size_t)iSize >= sizeof(sLength)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	return xrtHttpHeadersSet(
		pHeaders,
		XRT_STR_LITERAL("Content-Length"),
		(xstrview){ sLength, (size_t)iSize }
	);
}



/* 按已经解码的计划事实规范化表示 Header。 */
static bool __xrtHttpClientCacheNormalizeHeaders(
	xhttpheaders* pHeaders,
	bool bRemoveRange,
	bool bRemoveLength,
	bool bSetLength,
	uint64 iLength
)
{
	if ( bRemoveRange ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Range")
		);
	}
	if ( bRemoveLength ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Length")
		);
	}
	if ( bSetLength ) {
		return __xrtHttpClientCacheContentLength(
			pHeaders,
			iLength
		);
	}
	return true;
}



/* 把外层 multipart framing Header 还原为被分片表示的 Header。 */
static bool __xrtHttpClientCacheMultipartHeaders(
	xhttpheaders* pHeaders,
	const __xrt_http_client_cache_multipart* pPlan
)
{
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Range")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Length")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Type")
	);
	if ( ((pPlan->Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE) != 0) &&
		!xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Type"),
			pPlan->ContentType
		) ) {
		return false;
	}
	if ( (pPlan->Flags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) != 0 ) {
		return __xrtHttpClientCacheContentLength(
			pHeaders,
			pPlan->Length
		);
	}
	return true;
}



/* 删除 unsafe 成功响应影响的目标与同源位置条目。 */
static bool __xrtHttpClientCacheInvalidate(
	xhttpcall* pCall,
	const xhttpresponse* pResponse
)
{
	const xhttpheaders* pHeaders =
		xrtHttpResponseHeaders(pResponse);
	const xhttpfield* pFields =
		xrtHttpHeadersData(pHeaders);
	size_t iFieldCount =
		xrtHttpHeadersCount(pHeaders);
	xhttpcacheinvalidatecursor Cursor;
	xhttpcacheinvalidateitem Item;
	xhttpnext Next;
	xstrview Target =
		__xrtHttpClientCacheURI(pCall->Request);
	xstrview Partition = {
		pCall->CachePartitionKey,
		pCall->CachePartitionSize
	};

	xrtHttpCacheInvalidationCursorInit(&Cursor);
	for ( ;; ) {
		char sLocal[256];
		str sURI = sLocal;
		size_t iSize = 0;

		Next = xrtHttpCacheInvalidationNext(
			xrtHttpRequestMethod(pCall->Request),
			xrtHttpResponseStatus(pResponse),
			Target,
			pFields,
			iFieldCount,
			&Cursor,
			&Item
		);
		if ( Next == XHTTP_NEXT_END ) {
			return true;
		}
		if ( Next == XHTTP_NEXT_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"plan-http-cache-invalidation",
				"HTTP cache invalidation candidates are invalid"
			);
		}
		if ( !xrtHttpCacheInvalidationWrite(
			Target,
			&Item,
			NULL,
			0,
			&iSize
		) ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"resolve-http-cache-invalidation",
				"HTTP cache invalidation URI could not be resolved"
			);
		}
		if ( iSize > sizeof(sLocal) ) {
			sURI = (str)xrtMalloc(iSize);
			if ( sURI == NULL ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"allocate-http-cache-invalidation",
					"HTTP cache invalidation URI could not be allocated"
				);
			}
		}
		if ( !xrtHttpCacheInvalidationWrite(
			Target,
			&Item,
			sURI,
			iSize,
			&iSize
		) ) {
			if ( sURI != sLocal ) {
				xrtFree(sURI);
			}
			return __xrtHttpClientCacheRecover(
				pCall,
				"write-http-cache-invalidation",
				"HTTP cache invalidation URI could not be written"
			);
		}
		xrtClearError();
		if ( !xrtHttpCacheRemoveURI(
			pCall->Client->Cache,
			(xstrview){ sURI, iSize },
			Partition,
			NULL
		) ) {
			if ( sURI != sLocal ) {
				xrtFree(sURI);
			}
			return __xrtHttpClientCacheRecover(
				pCall,
				"remove-http-cache-invalidation",
				"HTTP cache backend could not invalidate the response URI"
			);
		}
		if ( sURI != sLocal ) {
			xrtFree(sURI);
		}
		if ( xrtGetError() != NULL ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"remove-http-cache-invalidation",
				"HTTP cache backend could not invalidate a URI"
			);
		}
	}
}



/* 创建保存字段的可修改副本。 */
static xhttpheaders* __xrtHttpClientCacheRecordHeaders(
	const xhttpcacherecord* pRecord
);



/* 把记录的每个正文片段投影为范围组合协议视图。 */
static xhttpcachefragment* __xrtHttpClientCacheFragments(
	const xhttpcacherecord* pRecord,
	size_t* pCount
)
{
	xhttpcacheentry Entry;
	xhttpcachefragment* pFragments;
	size_t iPartCount =
		xrtHttpCacheRecordPartCount(pRecord);
	size_t iCount = 0;
	uint32 iRecordFlags = xrtHttpCacheRecordFlags(pRecord);
	size_t i;

	*pCount = 0;
	if ( iPartCount == 0 ) {
		return NULL;
	}
	if ( iPartCount >
		(SIZE_MAX / sizeof(*pFragments)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	if ( !xrtHttpCacheRecordEntry(pRecord, &Entry) ) {
		return NULL;
	}
	pFragments = (xhttpcachefragment*)xrtMalloc(
		iPartCount * sizeof(*pFragments)
	);
	if ( pFragments == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iPartCount; i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		xhttpcachefragment* pFragment;
		uint64 iLast;

		if ( (pPart == NULL) ||
			(pPart->Data.Size == 0) ||
			(((uint64)pPart->Data.Size - UINT64_C(1)) >
			 (UINT64_MAX - pPart->Offset)) ) {
			xrtFree(pFragments);
			xrtSetErrorKind(XERR_INTERNAL);
			return NULL;
		}
		iLast = pPart->Offset +
			(uint64)pPart->Data.Size -
			UINT64_C(1);
		if ( (iCount != 0) &&
			(pFragments[iCount - 1u].Range.Last !=
			 UINT64_MAX) &&
			(pPart->Offset ==
			 (pFragments[iCount - 1u].Range.Last +
			  UINT64_C(1))) ) {
			pFragments[iCount - 1u].Range.Last =
				iLast;
			continue;
		}
		pFragment = &pFragments[iCount++];
		memset(pFragment, 0, sizeof(*pFragment));
		pFragment->Entry = Entry;
		pFragment->Entry.Flags &=
			~(uint32)XHTTP_CACHE_ENTRY_RANGE_COVERED;
		pFragment->Range.First = pPart->Offset;
		pFragment->Range.Last = iLast;
		pFragment->Flags =
			XHTTP_CACHE_FRAGMENT_HAS_RANGE;
		if ( (iRecordFlags &
			  XHTTP_CACHE_RECORD_HAS_LENGTH) != 0 ) {
			pFragment->Flags |=
				XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
			pFragment->Length =
				xrtHttpCacheRecordLength(pRecord);
		}
	}
	for ( i = 0; i < iCount; i++ ) {
		bool bComplete =
			(iCount == 1) &&
			((iRecordFlags &
			  XHTTP_CACHE_RECORD_COMPLETE) != 0);

		if ( bComplete ) {
			pFragments[i].Entry.Flags &=
				~(uint32)XHTTP_CACHE_ENTRY_PARTIAL;
			pFragments[i].SourceStatus =
				XHTTP_STATUS_OK;
		} else {
			pFragments[i].Entry.Flags |=
				XHTTP_CACHE_ENTRY_PARTIAL;
			pFragments[i].SourceStatus =
				XHTTP_STATUS_PARTIAL_CONTENT;
		}
	}
	*pCount = iCount;
	return pFragments;
}



/* 按字段名用新记录字段覆盖已有 Header，保持同名字段的线路顺序。 */
static bool __xrtHttpClientCacheHeadersOverlay(
	xhttpheaders* pHeaders,
	const xhttpcacherecord* pIncoming,
	bool bShared
)
{
	xhttpcacheentry Entry;
	const xhttpfield* pFields;
	size_t iCount;
	size_t i;

	if ( !xrtHttpCacheRecordEntry(
		pIncoming,
		&Entry
	) ) {
		return false;
	}
	pFields = Entry.Fields;
	iCount = Entry.FieldCount;
	for ( i = 0; i < iCount; i++ ) {
		const xhttpfield* pField = &pFields[i];
		xhttpcachefieldupdate Update;
		size_t j;

		for ( j = 0; j < i; j++ ) {
			if ( xrtHttpFieldNameEqual(
					pFields[j].Name,
					pField->Name
				) ) {
				break;
			}
		}
		if ( j != i ) {
			continue;
		}
		Update = xrtHttpCacheFieldUpdate(
			pFields,
			iCount,
			i,
			bShared,
			XHTTP_CACHE_UPDATE_FIELD_NONE
		);
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			return false;
		}
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			continue;
		}
		(void)xrtHttpHeadersRemove(
			pHeaders,
			pField->Name
		);
		for ( j = i; j < iCount; j++ ) {
			const xhttpfield* pValue = &pFields[j];

			if ( xrtHttpFieldNameEqual(
					pValue->Name,
					pField->Name
				) && !xrtHttpHeadersAdd(
					pHeaders,
					pValue->Name,
					pValue->Value
				) ) {
				return false;
			}
		}
	}
	return true;
}



/* 在规范覆盖数组上原地应用一个片段组合计划。 */
static bool __xrtHttpClientCacheCombineApply(
	xhttpcachefragment* pFragments,
	size_t* pCount,
	size_t iCapacity,
	const xhttpcachefragment* pIncoming,
	const xhttpcachecombineplan* pPlan
)
{
	xhttpcachefragment Fragment = *pIncoming;
	size_t iTail;
	size_t iTailCount;
	size_t iResult;

	if ( (pPlan->Index > *pCount) ||
		(pPlan->RemoveCount >
		 (*pCount - pPlan->Index)) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	iResult =
		*pCount - pPlan->RemoveCount + 1u;
	if ( (iResult != pPlan->ResultCount) ||
		(iResult > iCapacity) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	iTail = pPlan->Index +
		pPlan->RemoveCount;
	iTailCount = *pCount - iTail;
	memmove(
		&pFragments[pPlan->Index + 1u],
		&pFragments[iTail],
		iTailCount * sizeof(*pFragments)
	);

	Fragment.Range = pPlan->Range;
	Fragment.Length = pPlan->Length;
	Fragment.Flags =
		XHTTP_CACHE_FRAGMENT_HAS_RANGE;
	if ( pPlan->HasLength ) {
		Fragment.Flags |=
			XHTTP_CACHE_FRAGMENT_HAS_LENGTH;
	}
	Fragment.Entry.Flags &=
		~(uint32)XHTTP_CACHE_ENTRY_RANGE_COVERED;
	if ( pPlan->Complete ) {
		Fragment.Entry.Flags &=
			~(uint32)XHTTP_CACHE_ENTRY_PARTIAL;
	} else {
		Fragment.Entry.Flags |=
			XHTTP_CACHE_ENTRY_PARTIAL;
	}
	pFragments[pPlan->Index] = Fragment;
	*pCount = iResult;
	return true;
}



/* 为最终覆盖区间计算一次性正文布局。 */
static bool __xrtHttpClientCacheCombineLayout(
	const xhttpcachefragment* pFragments,
	size_t iCount,
	xhttpcachepart* pParts,
	uint64 iMaxBody,
	size_t* pBodySize
)
{
	size_t iTotal = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		uint64 iSpan64 =
			pFragments[i].Range.Last -
			pFragments[i].Range.First;
		size_t iSpan;

		if ( iSpan64 == UINT64_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iSpan64++;
		if ( (uint64)(size_t)iSpan64 != iSpan64 ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		iSpan = (size_t)iSpan64;
		if ( ((uint64)iTotal > iMaxBody) ||
			(iSpan64 >
			 (iMaxBody - (uint64)iTotal)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		pParts[i].Offset =
			pFragments[i].Range.First;
		pParts[i].Data.Data = NULL;
		pParts[i].Data.Size = iSpan;
		iTotal += iSpan;
	}
	*pBodySize = iTotal;
	return true;
}



/* 把一个不可变记录的相交字节复制到最终覆盖布局。 */
static bool __xrtHttpClientCacheCombineCopy(
	const xhttpcacherecord* pRecord,
	xhttpcachepart* pParts,
	size_t iPartCount
)
{
	size_t iSourceCount =
		xrtHttpCacheRecordPartCount(pRecord);
	size_t iSource = 0;
	size_t iTarget = 0;

	while ( (iSource < iSourceCount) &&
		(iTarget < iPartCount) ) {
		const xhttpcachepart* pSource =
			xrtHttpCacheRecordPartAt(
				pRecord,
				iSource
			);
		xhttpcachepart* pTarget =
			&pParts[iTarget];
		uint64 iSourceLast;
		uint64 iTargetLast;
		uint64 iFirst;
		uint64 iLast;
		size_t iSize;

		if ( (pSource == NULL) ||
			(pSource->Data.Size == 0) ||
			(pTarget->Data.Size == 0) ||
			(((uint64)pSource->Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - pSource->Offset)) ||
			(((uint64)pTarget->Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - pTarget->Offset)) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return false;
		}
		iSourceLast =
			pSource->Offset +
			(uint64)pSource->Data.Size -
			UINT64_C(1);
		iTargetLast =
			pTarget->Offset +
			(uint64)pTarget->Data.Size -
			UINT64_C(1);
		if ( iSourceLast < pTarget->Offset ) {
			iSource++;
			continue;
		}
		if ( iTargetLast < pSource->Offset ) {
			iTarget++;
			continue;
		}
		iFirst = pSource->Offset >
			pTarget->Offset ?
				pSource->Offset :
				pTarget->Offset;
		iLast = iSourceLast < iTargetLast ?
			iSourceLast :
			iTargetLast;
		iSize = (size_t)(
			(iLast - iFirst) +
			UINT64_C(1)
		);
		memcpy(
			(bytes)pTarget->Data.Data +
				(size_t)(iFirst -
				 pTarget->Offset),
			pSource->Data.Data +
				(size_t)(iFirst -
				 pSource->Offset),
			iSize
		);
		if ( iSourceLast <= iTargetLast ) {
			iSource++;
		}
		if ( iTargetLast <= iSourceLast ) {
			iTarget++;
		}
	}
	return true;
}



/* 依据全部已应用计划一次创建最终组合记录。 */
static xhttpcacherecord* __xrtHttpClientCacheCombineRecord(
	xhttpcall* pCall,
	const xhttpcacherecord* pIncoming,
	const xhttpcachefragment* pFragments,
	size_t iCount,
	uint64 iLength,
	bool bHasLength,
	bool bComplete
)
{
	const xhttpcacherecord* pStored =
		pCall->CacheCandidate;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcachepart* pParts = NULL;
	xhttpheaders* pHeaders = NULL;
	xhttpcacherecord* pRecord = NULL;
	bytes pBody = NULL;
	size_t iBodySize = 0;
	size_t iOffset = 0;
	size_t i;

	if ( iCount >
		(SIZE_MAX / sizeof(*pParts)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	pParts = (xhttpcachepart*)xrtMalloc(
		iCount * sizeof(*pParts)
	);
	if ( pParts == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpClientCacheCombineLayout(
		pFragments,
		iCount,
		pParts,
		pCall->Client->Config.Cache.MaxBody,
		&iBodySize
	) ) {
		goto cleanup;
	}
	pBody = (bytes)xrtMalloc(iBodySize);
	if ( pBody == NULL ) {
		goto cleanup;
	}
	for ( i = 0; i < iCount; i++ ) {
		pParts[i].Data.Data =
			pBody + iOffset;
		iOffset += pParts[i].Data.Size;
	}
	if ( (iOffset != iBodySize) ||
		!__xrtHttpClientCacheCombineCopy(
			pStored,
			pParts,
			iCount
		) || !__xrtHttpClientCacheCombineCopy(
			pIncoming,
			pParts,
			iCount
		) ) {
		goto cleanup;
	}

	pHeaders = __xrtHttpClientCacheRecordHeaders(
		pStored
	);
	if ( (pHeaders == NULL) ||
		!__xrtHttpClientCacheHeadersOverlay(
			pHeaders,
			pIncoming,
			pCall->Client->Config.Cache.Shared
		) || !__xrtHttpClientCacheNormalizeHeaders(
			pHeaders,
			true,
			true,
			bComplete,
			iLength
		) || !__xrtHttpClientCacheKey(
			pCall,
			&Key
		) || !xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			XHTTP_STATUS_OK
		) ) {
		goto cleanup;
	}
	Input.Version =
		xrtHttpCacheRecordVersion(pIncoming);
	Input.Reason = XRT_STR_LITERAL("OK");
	Input.Fields = xrtHttpHeadersData(pHeaders);
	Input.FieldCount =
		xrtHttpHeadersCount(pHeaders);
	Input.Parts = pParts;
	Input.PartCount = iCount;
	Input.Length = iLength;
	if ( bHasLength ) {
		Input.Flags |=
			XHTTP_CACHE_RECORD_HAS_LENGTH;
	}
	if ( bComplete ) {
		Input.Flags |=
			XHTTP_CACHE_RECORD_COMPLETE;
	}
	Input.ResponseTime =
		pCall->CacheResponseTime;
	Input.RequestClock =
		pCall->CacheRequestClock;
	Input.ResponseClock =
		pCall->CacheResponseClock;
	pRecord = xrtHttpCacheRecordCreate(&Input);

cleanup:
	xrtHttpHeadersDestroy(pHeaders);
	xrtFree(pBody);
	xrtFree(pParts);
	return pRecord;
}



/* 尝试把一个或多个新 206 片段原子并入同一 Vary 变体。 */
static xhttpcacherecord* __xrtHttpClientCacheCombine(
	xhttpcall* pCall,
	xhttpcacherecord* pIncoming
)
{
	xhttpcachefragment* pStored = NULL;
	xhttpcachefragment* pNew = NULL;
	xhttpcachefragment* pWorking = NULL;
	xhttpcachecombineplan Plan;
	xhttpcachecombinedecision Decision;
	xhttpcacherecord* pRecord = NULL;
	size_t iStoredCount = 0;
	size_t iNewCount = 0;
	size_t iCapacity;
	size_t iCount;
	size_t i;

	if ( (pCall->CacheCandidate == NULL) ||
		(xrtHttpCacheRecordStatus(
			pCall->CacheCandidate
		 ) != XHTTP_STATUS_OK) ||
		((xrtHttpCacheRecordFlags(pIncoming) &
		  XHTTP_CACHE_RECORD_COMPLETE) != 0) ) {
		return xrtHttpCacheRecordRetain(pIncoming);
	}
	if ( xrtHttpCacheRecordPartCount(
		pCall->CacheCandidate
	) == 0 ) {
		return xrtHttpCacheRecordRetain(pIncoming);
	}
	pStored = __xrtHttpClientCacheFragments(
		pCall->CacheCandidate,
		&iStoredCount
	);
	pNew = __xrtHttpClientCacheFragments(
		pIncoming,
		&iNewCount
	);
	if ( (pStored == NULL) || (pNew == NULL) ||
		(iStoredCount == 0) || (iNewCount == 0) ) {
		goto cleanup;
	}
	if ( iNewCount >
		(SIZE_MAX - iStoredCount) ) {
		xrtSetErrorKind(XERR_RANGE);
		goto cleanup;
	}
	iCapacity = iStoredCount + iNewCount;
	if ( iCapacity >
		(SIZE_MAX / sizeof(*pWorking)) ) {
		xrtSetErrorKind(XERR_RANGE);
		goto cleanup;
	}
	pWorking = (xhttpcachefragment*)xrtMalloc(
		iCapacity * sizeof(*pWorking)
	);
	if ( pWorking == NULL ) {
		goto cleanup;
	}
	memcpy(
		pWorking,
		pStored,
		iStoredCount * sizeof(*pWorking)
	);
	iCount = iStoredCount;

	for ( i = 0; i < iNewCount; i++ ) {
		Decision = xrtHttpCacheCombinePlan(
			pWorking,
			iCount,
			&pNew[i],
			&Plan
		);
		if ( Decision == XHTTP_CACHE_COMBINE_ERROR ) {
			goto cleanup;
		}
		if ( Decision != XHTTP_CACHE_COMBINE_APPLY ) {
			pRecord =
				xrtHttpCacheRecordRetain(pIncoming);
			goto cleanup;
		}
		if ( !__xrtHttpClientCacheCombineApply(
			pWorking,
			&iCount,
			iCapacity,
			&pNew[i],
			&Plan
		) ) {
			goto cleanup;
		}
	}
	if ( iCount >
		pCall->Client->Config.Cache.MaxRanges ) {
		pRecord =
			xrtHttpCacheRecordRetain(pIncoming);
		goto cleanup;
	}
	pRecord = __xrtHttpClientCacheCombineRecord(
		pCall,
		pIncoming,
		pWorking,
		iCount,
		Plan.Length,
		Plan.HasLength,
		Plan.Complete
	);

cleanup:
	xrtFree(pWorking);
	xrtFree(pNew);
	xrtFree(pStored);
	return pRecord;
}



/*
	以条件插入或条件替换提交不完整表示。
	并发冲突后重新读取当前记录并重新执行片段规划，避免丢失覆盖。
*/
static bool __xrtHttpClientCacheFragmentCommit(
	xhttpcall* pCall,
	xhttpcacherecord* pIncoming
)
{
	xstrview Method = xrtHttpRequestMethod(pCall->Request);
	xhttpcacherecord* pCombined;
	xhttpcachelookup Lookup;
	xhttpcacheput Put;
	size_t iAttempt;

	for ( iAttempt = 0;
		iAttempt < XRT_HTTP_CLIENT_CACHE_COMMIT_RETRIES;
		iAttempt++ ) {
		if ( pCall->CacheCandidate == NULL ) {
			Put = xrtHttpCacheInsert(
				pCall->Client->Cache,
				pIncoming
			);
		} else {
			pCombined = __xrtHttpClientCacheCombine(
				pCall,
				pIncoming
			);
			if ( pCombined == NULL ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"combine-http-cache-fragment",
					"HTTP cache response fragment could not be combined"
				);
			}
			Put = xrtHttpCacheReplace(
				pCall->Client->Cache,
				pCall->CacheCandidate,
				pCombined
			);
			xrtHttpCacheRecordRelease(pCombined);
		}
		if ( Put == XHTTP_CACHE_PUT_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"store-http-cache-fragment",
				"HTTP cache backend could not commit the response fragment"
			);
		}
		if ( Put == XHTTP_CACHE_PUT_REJECTED ) {
			return true;
		}
		if ( Put != XHTTP_CACHE_PUT_CONFLICT ) {
			__xrtHttpClientCacheOutcome(
				pCall,
				XHTTP_CLIENT_CACHE_UPDATED
			);
			return true;
		}
		Lookup = __xrtHttpClientCacheReload(
			pCall,
			Method
		);
		if ( Lookup == XHTTP_CACHE_LOOKUP_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"reload-http-cache-fragment",
				"HTTP cache fragment conflict could not be reloaded"
			);
		}
	}

	xrtSetErrorKind(XERR_AGAIN);
	return __xrtHttpClientCacheRecover(
		pCall,
		"retry-http-cache-fragment",
		"HTTP cache fragment commit remained contended"
	);
}



/* 把成功网络响应规范化为完整或部分不可变记录。 */
static bool __xrtHttpClientCacheStore(
	xhttpcall* pCall,
	const xhttpresponse* pResponse
)
{
	const xhttpfield* pRequestFields =
		xrtHttpHeadersData(pCall->CacheRequestFields);
	size_t iRequestCount =
		xrtHttpHeadersCount(pCall->CacheRequestFields);
	const xhttpfield* pResponseFields =
		xrtHttpHeadersData(pCall->CacheResponseFields);
	size_t iResponseCount =
		xrtHttpHeadersCount(pCall->CacheResponseFields);
	xhttpcachecontrol Request;
	xhttpcachecontrol Response;
	xhttpcachetime Time;
	xhttpcachestoreinput StoreInput;
	xhttpcachestoreplan StorePlan;
	xhttpcachefragmentinput FragmentInput;
	xhttpcachefragmentplan FragmentPlan;
	xhttpcachefragmentdecision FragmentDecision;
	__xrt_http_client_cache_multipart Multipart;
	__xrt_http_client_cache_multipart_decision
		MultipartDecision =
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE;
	xhttpcacherecordinput Input;
	xhttpcachekey Key;
	xhttpcachepart Part;
	const xhttpcachepart* pRecordParts;
	xhttpcacherecord* pRecord;
	xhttpheaders* pNormalized;
	xhttpfield* pStored;
	const xhttpfield* pContentType;
	size_t iStored;
	size_t iRecordPartCount;
	xhttpcacheput Put;
	xhttpnext ContentTypeNext;
	uint16 iSourceStatus =
		xrtHttpResponseStatus(pResponse);
	uint16 iStoredStatus = iSourceStatus;
	bool bFragment = false;
	bool bMultipart = false;
	bool bComplete = false;

	memset(&Multipart, 0, sizeof(Multipart));
	if ( !pCall->CacheCapture ||
		(pCall->CacheResponseFields == NULL) ) {
		return true;
	}
	if ( (xrtHttpRequestHeader(
			pCall->Request,
			XRT_STR_LITERAL("Range")
		 ) != NULL) &&
		(iSourceStatus != XHTTP_STATUS_OK) &&
		(iSourceStatus !=
		 XHTTP_STATUS_PARTIAL_CONTENT) ) {
		return true;
	}
	if ( !__xrtHttpClientCacheControl(
		pRequestFields,
		iRequestCount,
		&Request
	) || !__xrtHttpClientCacheControl(
		pResponseFields,
		iResponseCount,
		&Response
	) || !xrtHttpCacheTimeParse(
		pResponseFields,
		iResponseCount,
		&Time
	) || !xrtHttpCacheStoreInputInit(
		&StoreInput,
		xrtHttpRequestMethod(pCall->Request),
		xrtHttpResponseStatus(pResponse),
		pCall->Client->Config.Cache.Shared
	) ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-store",
			"HTTP cache response metadata is invalid"
		);
	}
	StoreInput.Flags |=
		XHTTP_CACHE_STORE_RANGE_SUPPORTED;
	if ( xrtHttpCacheStorePlan(
		&Request,
		&Response,
		&Time,
		&StoreInput,
		&StorePlan
	) != XHTTP_CACHE_STORE_KEEP ) {
		if ( StorePlan.Decision == XHTTP_CACHE_STORE_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"plan-http-cache-store",
				"HTTP cache store plan failed"
			);
		}
		return true;
	}
	if ( xrtHttpMethodEqual(
			xrtHttpRequestMethod(pCall->Request),
			XRT_STR_LITERAL("GET")
		) && ((iSourceStatus == XHTTP_STATUS_OK) ||
		 (iSourceStatus ==
		  XHTTP_STATUS_PARTIAL_CONTENT)) ) {
		xrtHttpCacheFragmentInputInit(&FragmentInput);
		FragmentInput.Method =
			xrtHttpRequestMethod(pCall->Request);
		FragmentInput.Fields = pResponseFields;
		FragmentInput.FieldCount = iResponseCount;
		FragmentInput.ResponseTime =
			pCall->CacheResponseTime;
		FragmentInput.BodySize =
			(uint64)pCall->CacheBodySize;
		FragmentInput.Status = iSourceStatus;
		FragmentInput.Flags =
			XHTTP_CACHE_FRAGMENT_HEADERS_COMPLETE |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
		if ( iSourceStatus ==
			XHTTP_STATUS_PARTIAL_CONTENT ) {
			ContentTypeNext = xrtHttpFieldGetUnique(
				pResponseFields,
				iResponseCount,
				XRT_STR_LITERAL("Content-Type"),
				&pContentType
			);
			if ( ContentTypeNext == XHTTP_NEXT_ITEM ) {
				MultipartDecision =
					__xrtHttpClientCacheMultipartPlan(
						&FragmentInput,
						pContentType->Value,
						(xbytesview){
							pCall->CacheBody,
							pCall->CacheBodySize
						},
						pCall->Client->Config.Cache.MaxRanges,
						&Multipart
					);
			} else if ( ContentTypeNext == XHTTP_NEXT_ERROR ) {
				xrtClearError();
			}
			if ( (MultipartDecision ==
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR) ||
				(MultipartDecision ==
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP) ) {
				bool bResult =
					__xrtHttpClientCacheRecover(
						pCall,
						"plan-http-cache-multipart",
						"HTTP multipart range response could not be cached"
					);

				__xrtHttpClientCacheMultipartUnit(
					&Multipart
				);
				return bResult;
			}
			if ( MultipartDecision ==
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE ) {
				bMultipart = true;
				bFragment = true;
				iStoredStatus = XHTTP_STATUS_OK;
			}
		}
		if ( !bMultipart ) {
			FragmentDecision =
				xrtHttpCacheFragmentPlan(
					&FragmentInput,
					&FragmentPlan
				);
			if ( FragmentDecision ==
				XHTTP_CACHE_FRAGMENT_ERROR ) {
				return __xrtHttpClientCacheRecover(
					pCall,
					"plan-http-cache-fragment",
					"HTTP cache response fragment is invalid"
				);
			}
			if ( FragmentDecision !=
				XHTTP_CACHE_FRAGMENT_STORE ) {
				return true;
			}
			bFragment = true;
			if ( (FragmentPlan.Actions &
				  XHTTP_CACHE_FRAGMENT_AS_200) != 0 ) {
				iStoredStatus = XHTTP_STATUS_OK;
			}
		}
	}
	pStored = __xrtHttpClientCacheStoredFields(
		pCall->CacheResponseFields,
		pCall->Client->Config.Cache.Shared,
		StorePlan.Actions,
		&iStored
	);
	if ( (pStored == NULL) && (iResponseCount != 0) ) {
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"filter-http-cache-fields",
			"HTTP cache response fields could not be filtered"
		);
	}
	pNormalized = __xrtHttpClientCacheHeadersCreate(
		pStored,
		iStored
	);
	xrtFree(pStored);
	if ( pNormalized == NULL ) {
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"copy-http-cache-fields",
			"HTTP cache response fields could not be copied"
		);
	}
	if ( bMultipart &&
		!__xrtHttpClientCacheMultipartHeaders(
			pNormalized,
			&Multipart
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"normalize-http-cache-multipart",
			"HTTP multipart range fields could not be normalized"
		);
	}
	if ( !bMultipart && bFragment &&
		!__xrtHttpClientCacheNormalizeHeaders(
			pNormalized,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_RANGE) != 0,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_REMOVE_CONTENT_LENGTH) != 0,
			(FragmentPlan.Actions &
			 XHTTP_CACHE_FRAGMENT_SET_CONTENT_LENGTH) != 0,
			FragmentPlan.Fragment.Length
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return __xrtHttpClientCacheRecover(
			pCall,
			"normalize-http-cache-fields",
			"HTTP cache response fields could not be normalized"
		);
	}
	if ( !__xrtHttpClientCacheKey(pCall, &Key) ||
		!xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			iStoredStatus
		) ) {
		xrtHttpHeadersDestroy(pNormalized);
		__xrtHttpClientCacheMultipartUnit(
			&Multipart
		);
		return false;
	}
	Part.Offset = !bMultipart && bFragment &&
		((FragmentPlan.Fragment.Flags &
		  XHTTP_CACHE_FRAGMENT_HAS_RANGE) != 0) ?
			FragmentPlan.Fragment.Range.First : 0;
	Part.Data = (xbytesview){
		pCall->CacheBody,
		pCall->CacheBodySize
	};
	pRecordParts = bMultipart ?
		Multipart.Parts :
		(pCall->CacheBodySize != 0 ? &Part : NULL);
	iRecordPartCount = bMultipart ?
		Multipart.PartCount :
		(pCall->CacheBodySize != 0 ? 1u : 0u);
	Input.Version = xrtHttpResponseVersion(pResponse);
	Input.Reason = iStoredStatus == iSourceStatus ?
		xrtHttpResponseReason(pResponse) :
		XRT_STR_LITERAL("OK");
	Input.Fields = xrtHttpHeadersData(pNormalized);
	Input.FieldCount = xrtHttpHeadersCount(pNormalized);
	if ( iSourceStatus !=
		XHTTP_STATUS_PARTIAL_CONTENT ) {
		Input.Trailers = xrtHttpResponseTrailerData(pResponse);
		Input.TrailerCount =
			xrtHttpResponseTrailerCount(pResponse);
	}
	Input.Parts = pRecordParts;
	Input.PartCount = iRecordPartCount;
	if ( bMultipart ) {
		if ( (Multipart.Flags &
			  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_HAS_LENGTH;
			Input.Length = Multipart.Length;
		}
		if ( (Multipart.Flags &
			  __XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_COMPLETE;
			bComplete = true;
		}
	} else if ( bFragment ) {
		if ( (FragmentPlan.Fragment.Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0 ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_HAS_LENGTH;
			Input.Length =
				FragmentPlan.Fragment.Length;
		}
		if ( xrtHttpCacheFragmentComplete(
			&FragmentPlan.Fragment
		) ) {
			Input.Flags |=
				XHTTP_CACHE_RECORD_COMPLETE;
			bComplete = true;
		}
	} else {
		Input.Length =
			(uint64)pCall->CacheBodySize;
		Input.Flags =
			XHTTP_CACHE_RECORD_HAS_LENGTH |
			XHTTP_CACHE_RECORD_COMPLETE;
		bComplete = true;
	}
	Input.ResponseTime = pCall->CacheResponseTime;
	Input.RequestClock = pCall->CacheRequestClock;
	Input.ResponseClock = pCall->CacheResponseClock;
	pRecord = xrtHttpCacheRecordCreate(&Input);
	xrtHttpHeadersDestroy(pNormalized);
	__xrtHttpClientCacheMultipartUnit(&Multipart);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"create-http-cache-record",
			"HTTP cache record could not be created"
		);
	}
	if ( bFragment &&
		(iSourceStatus ==
		 XHTTP_STATUS_PARTIAL_CONTENT) &&
		!bComplete ) {
		bool bResult =
			__xrtHttpClientCacheFragmentCommit(
				pCall,
				pRecord
			);

		xrtHttpCacheRecordRelease(pRecord);
		return bResult;
	}
	Put = xrtHttpCachePut(
		pCall->Client->Cache,
		pRecord
	);
	xrtHttpCacheRecordRelease(pRecord);
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-record",
			"HTTP cache backend could not store the response"
		);
	}
	if ( Put > XHTTP_CACHE_PUT_REJECTED ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_UPDATED
		);
	}
	return true;
}



/* 创建保存字段的可修改副本。 */
static xhttpheaders* __xrtHttpClientCacheRecordHeaders(
	const xhttpcacherecord* pRecord
)
{
	xhttpheaders* pHeaders = xrtHttpHeadersCreate(NULL);
	size_t i;

	if ( pHeaders == NULL ) {
		return NULL;
	}
	for ( i = 0;
		i < xrtHttpCacheRecordFieldCount(pRecord);
		i++ ) {
		const xhttpfield* pField =
			xrtHttpCacheRecordFieldAt(pRecord, i);

		if ( (pField == NULL) ||
			!xrtHttpHeadersAdd(
				pHeaders,
				pField->Name,
				pField->Value
			) ) {
			xrtHttpHeadersDestroy(pHeaders);
			return NULL;
		}
	}
	return pHeaders;
}



/* 判断字段名是否已经在当前输入前缀出现。 */
static bool __xrtHttpClientCacheFieldEarlier(
	const xhttpfield* pFields,
	size_t iIndex
)
{
	size_t i;

	for ( i = 0; i < iIndex; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			pFields[iIndex].Name
		) ) {
			return true;
		}
	}
	return false;
}



/* 按验证或 HEAD 元数据建立保留原正文的不可变记录。 */
static xhttpcacherecord* __xrtHttpClientCacheUpdateRecord(
	xhttpcall* pCall,
	bool bSelect304
)
{
	const xhttpcachekey* pCandidateKey =
		xrtHttpCacheRecordKey(pCall->CacheCandidate);
	const xhttpfield* pFields = xrtHttpHeadersData(
		pCall->CacheResponseFields
	);
	size_t iCount = xrtHttpHeadersCount(
		pCall->CacheResponseFields
	);
	xhttpcacheentry Entry;
	xhttpcacheupdatematch Match;
	xhttpheaders* pMerged;
	xhttpcachekey Key;
	xhttpcacherecordinput Input;
	xhttpcachepart* pParts = NULL;
	xhttpfield* pTrailers = NULL;
	xhttpcacherecord* pRecord = NULL;
	size_t iPartCount;
	size_t iTrailerCount;
	size_t iSelected;
	size_t i;

	if ( (pCandidateKey == NULL) ||
		!xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return NULL;
	}
	if ( bSelect304 ) {
		Match = xrtHttpCache304Select(
			pFields,
			iCount,
			&Entry,
			1,
			NULL,
			0,
			&iSelected
		);
		if ( (Match == XHTTP_CACHE_UPDATE_MATCH_ERROR) ||
			(Match == XHTTP_CACHE_UPDATE_MATCH_NONE) ) {
			if ( Match ==
				XHTTP_CACHE_UPDATE_MATCH_NONE ) {
				xrtSetErrorKind(XERR_STATE);
			}
			return NULL;
		}
	}
	pMerged = __xrtHttpClientCacheRecordHeaders(
		pCall->CacheCandidate
	);
	if ( pMerged == NULL ) {
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		xhttpcachefieldupdate Update;
		size_t j;

		if ( __xrtHttpClientCacheFieldEarlier(
			pFields,
			i
		) ) {
			continue;
		}
		Update = xrtHttpCacheFieldUpdate(
			pFields,
			iCount,
			i,
			pCall->Client->Config.Cache.Shared,
			xrtHttpFieldNameEqual(
				pFields[i].Name,
				XRT_STR_LITERAL("Content-Range")
			) ?
				XHTTP_CACHE_UPDATE_FIELD_PROCESSED :
				XHTTP_CACHE_UPDATE_FIELD_NONE
		);
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_ERROR ) {
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		if ( Update == XHTTP_CACHE_FIELD_UPDATE_SKIP ) {
			continue;
		}
		(void)xrtHttpHeadersRemove(
			pMerged,
			pFields[i].Name
		);
		for ( j = i; j < iCount; j++ ) {
			if ( xrtHttpFieldNameEqual(
				pFields[j].Name,
				pFields[i].Name
			) && !xrtHttpHeadersAdd(
				pMerged,
				pFields[j].Name,
				pFields[j].Value
			) ) {
				xrtHttpHeadersDestroy(pMerged);
				return NULL;
			}
		}
	}
	iPartCount = xrtHttpCacheRecordPartCount(
		pCall->CacheCandidate
	);
	iTrailerCount = xrtHttpCacheRecordTrailerCount(
		pCall->CacheCandidate
	);
	if ( iPartCount != 0 ) {
		if ( iPartCount >
			(SIZE_MAX / sizeof(*pParts)) ) {
			xrtHttpHeadersDestroy(pMerged);
			xrtSetErrorKind(XERR_RANGE);
			return NULL;
		}
		pParts = (xhttpcachepart*)xrtMalloc(
			iPartCount * sizeof(*pParts)
		);
		if ( pParts == NULL ) {
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		for ( i = 0; i < iPartCount; i++ ) {
			const xhttpcachepart* pPart =
				xrtHttpCacheRecordPartAt(
					pCall->CacheCandidate,
					i
				);

			if ( pPart == NULL ) {
				xrtFree(pParts);
				xrtHttpHeadersDestroy(pMerged);
				xrtSetErrorKind(XERR_INTERNAL);
				return NULL;
			}
			pParts[i] = *pPart;
		}
	}
	if ( iTrailerCount != 0 ) {
		if ( iTrailerCount >
			(SIZE_MAX / sizeof(*pTrailers)) ) {
			xrtFree(pParts);
			xrtHttpHeadersDestroy(pMerged);
			xrtSetErrorKind(XERR_RANGE);
			return NULL;
		}
		pTrailers = (xhttpfield*)xrtMalloc(
			iTrailerCount * sizeof(*pTrailers)
		);
		if ( pTrailers == NULL ) {
			xrtFree(pParts);
			xrtHttpHeadersDestroy(pMerged);
			return NULL;
		}
		for ( i = 0; i < iTrailerCount; i++ ) {
			const xhttpfield* pTrailer =
				xrtHttpCacheRecordTrailerAt(
					pCall->CacheCandidate,
					i
				);

			if ( pTrailer == NULL ) {
				xrtFree(pTrailers);
				xrtFree(pParts);
				xrtHttpHeadersDestroy(pMerged);
				xrtSetErrorKind(XERR_INTERNAL);
				return NULL;
			}
			pTrailers[i] = *pTrailer;
		}
	}
	if ( __xrtHttpClientCacheKeyMethod(
			pCall,
			pCandidateKey->Method,
			&Key
		) &&
		xrtHttpCacheRecordInputInit(
			&Input,
			&Key,
			xrtHttpCacheRecordStatus(
				pCall->CacheCandidate
			)
		) ) {
		Input.Version = xrtHttpCacheRecordVersion(
			pCall->CacheCandidate
		);
		Input.Flags = xrtHttpCacheRecordFlags(
			pCall->CacheCandidate
		);
		Input.Reason = xrtHttpCacheRecordReason(
			pCall->CacheCandidate
		);
		Input.Fields = xrtHttpHeadersData(pMerged);
		Input.FieldCount = xrtHttpHeadersCount(pMerged);
		Input.Trailers = pTrailers;
		Input.TrailerCount = iTrailerCount;
		Input.Parts = pParts;
		Input.PartCount = iPartCount;
		Input.Length = xrtHttpCacheRecordLength(
			pCall->CacheCandidate
		);
		Input.ResponseTime = pCall->CacheResponseTime;
		Input.RequestClock = pCall->CacheRequestClock;
		Input.ResponseClock = pCall->CacheResponseClock;
		pRecord = xrtHttpCacheRecordCreate(&Input);
	}
	xrtFree(pTrailers);
	xrtFree(pParts);
	xrtHttpHeadersDestroy(pMerged);
	return pRecord;
}



/*
	用 200 HEAD 更新或条件删除当前选择的 GET 表示。
	更新冲突表示已有更新结果胜出，不再额外保存重复 HEAD 记录。
*/
static bool __xrtHttpClientCacheHeadFreshen(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	bool* pHandled
)
{
	xhttpcacheentry Entry;
	xhttpcacheheaddecision Decision;
	xhttpcacherecord* pRecord;
	xhttpcacheput Put;
	xhttpcachechange Change;

	*pHandled = false;
	if ( !__xrtHttpClientCacheHeadFromGet(pCall) ||
		(xrtHttpResponseStatus(pResponse) !=
		 XHTTP_STATUS_OK) ||
		(pCall->CacheResponseFields == NULL) ) {
		return true;
	}
	if ( !xrtHttpCacheRecordEntry(
		pCall->CacheCandidate,
		&Entry
	) ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"prepare-http-cache-head",
			"HTTP HEAD cache candidate is invalid"
		);
	}
	Decision = xrtHttpCacheHeadPlan(
		xrtHttpResponseStatus(pResponse),
		&Entry,
		xrtHttpHeadersData(pCall->CacheResponseFields),
		xrtHttpHeadersCount(pCall->CacheResponseFields)
	);
	if ( Decision == XHTTP_CACHE_HEAD_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"plan-http-cache-head",
			"HTTP HEAD cache metadata could not be compared"
		);
	}
	if ( Decision == XHTTP_CACHE_HEAD_IGNORE ) {
		return true;
	}
	if ( Decision == XHTTP_CACHE_HEAD_STALE ) {
		Change = xrtHttpCacheRemoveRecord(
			pCall->Client->Cache,
			pCall->CacheCandidate
		);
		if ( Change == XHTTP_CACHE_CHANGE_ERROR ) {
			return __xrtHttpClientCacheRecover(
				pCall,
				"invalidate-http-cache-head",
				"HTTP HEAD could not invalidate the stale GET response"
			);
		}
		if ( Change == XHTTP_CACHE_CHANGE_CONFLICT ) {
			*pHandled = true;
		}
		return true;
	}

	pRecord = __xrtHttpClientCacheUpdateRecord(
		pCall,
		false
	);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"update-http-cache-head",
			"HTTP HEAD metadata could not update the GET response"
		);
	}
	Put = xrtHttpCacheReplace(
		pCall->Client->Cache,
		pCall->CacheCandidate,
		pRecord
	);
	xrtHttpCacheRecordRelease(pRecord);
	*pHandled = true;
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		return __xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-head",
			"HTTP cache backend could not store HEAD metadata"
		);
	}
	if ( Put == XHTTP_CACHE_PUT_REPLACED ) {
		__xrtHttpClientCacheOutcome(
			pCall,
			XHTTP_CLIENT_CACHE_UPDATED
		);
	}
	return true;
}



/* 按缓存范围交付状态重写 Content-Range 与 Content-Length。 */
static bool __xrtHttpClientCacheResponseRange(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord
)
{
	static const char sMultipartPrefix[] =
		"multipart/byteranges; boundary=";
	xhttpheaders* pHeaders = (xhttpheaders*)
		xrtHttpResponseHeaders(pResponse);
	xhttpcontentrange ContentRange;
	char sLength[32];
	char sType[
		(sizeof(sMultipartPrefix) - 1u) +
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	];
	str sRange;
	size_t iRangeSize;
	uint64 iLength;
	int iWritten;

	/*
		HEAD 使用 GET 记录的原始表示元数据，但不执行 Range，
		也不能把无正文误写成长度为零的范围响应。
	*/
	if ( __xrtHttpClientCacheHeadRequest(pCall) ||
		(pCall->CacheRangeState ==
		 __XRT_HTTP_CLIENT_CACHE_RANGE_NONE) ) {
		return true;
	}
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Range")
	);
	(void)xrtHttpHeadersRemove(
		pHeaders,
		XRT_STR_LITERAL("Content-Length")
	);

	if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART ) {
		(void)xrtHttpHeadersRemove(
			pHeaders,
			XRT_STR_LITERAL("Content-Type")
		);
		memcpy(
			sType,
			sMultipartPrefix,
			sizeof(sMultipartPrefix) - 1u
		);
		memcpy(
			sType + sizeof(sMultipartPrefix) - 1u,
			pCall->CacheBoundary,
			XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
		);
		iWritten = snprintf(
			sLength,
			sizeof(sLength),
			"%llu",
			(unsigned long long)
				pCall->CacheRangeBodyLength
		);
		return (iWritten >= 0) &&
			((size_t)iWritten < sizeof(sLength)) &&
			xrtHttpHeadersSet(
				pHeaders,
				XRT_STR_LITERAL("Content-Type"),
				(xstrview){ sType, sizeof(sType) }
			) &&
			xrtHttpHeadersSet(
				pHeaders,
				XRT_STR_LITERAL("Content-Length"),
				(xstrview){
					sLength,
					(size_t)iWritten
				}
			);
	}

	memset(&ContentRange, 0, sizeof(ContentRange));
	ContentRange.HasLength = true;
	ContentRange.Length =
		xrtHttpCacheRecordLength(pRecord);
	if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL ) {
		ContentRange.Satisfied = true;
		ContentRange.First =
			pCall->CacheRange.First;
		ContentRange.Last =
			pCall->CacheRange.Last;
		iLength =
			(pCall->CacheRange.Last -
			 pCall->CacheRange.First) +
			UINT64_C(1);
	} else {
		iLength = 0;
	}

	sRange = xrtHttpContentRangeBuild(
		&ContentRange,
		&iRangeSize
	);
	if ( sRange == NULL ) {
		return false;
	}
	iWritten = snprintf(
		sLength,
		sizeof(sLength),
		"%llu",
		(unsigned long long)iLength
	);
	if ( (iWritten < 0) ||
		((size_t)iWritten >= sizeof(sLength)) ||
		!xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Range"),
			(xstrview){ sRange, iRangeSize }
		) || !xrtHttpHeadersSet(
			pHeaders,
			XRT_STR_LITERAL("Content-Length"),
			(xstrview){
				sLength,
				(size_t)iWritten
			}
		) ) {
		xrtFree(sRange);
		return false;
	}
	xrtFree(sRange);
	return true;
}



/* 重建一个拥有型响应并为复用路径写入当前 Age。 */
static xhttpresponse* __xrtHttpClientCacheResponse(
	xhttpcall* pCall,
	const xhttpcacherecord* pRecord
)
{
	xhttpresponse* pResponse;
	xhttpcachetime Time;
	xhttpcacheage Age;
	xstrview Reason = xrtHttpCacheRecordReason(pRecord);
	char sAge[32];
	int iAge;
	uint16 iStatus = xrtHttpCacheRecordStatus(pRecord);
	size_t i;

	if ( (pCall->CacheRangeState ==
		  __XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL) ||
		(pCall->CacheRangeState ==
		 __XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART) ) {
		iStatus = XHTTP_STATUS_PARTIAL_CONTENT;
		Reason = XRT_STR_LITERAL("Partial Content");
	} else if ( pCall->CacheRangeState ==
		__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE ) {
		iStatus = XHTTP_STATUS_RANGE_NOT_SATISFIABLE;
		Reason = XRT_STR_LITERAL("Range Not Satisfiable");
	}
	pResponse = xrtHttpBridgeResponseCreate(
		xrtHttpCacheRecordVersion(pRecord),
		iStatus,
		Reason,
		&pCall->Client->Config.Exchange.Headers
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	for ( i = 0;
		i < xrtHttpCacheRecordFieldCount(pRecord);
		i++ ) {
		const xhttpfield* pField =
			xrtHttpCacheRecordFieldAt(pRecord, i);

		if ( (pField == NULL) ||
			!xrtHttpBridgeResponseAddHeader(
				pResponse,
				pField->Name,
				pField->Value
			) ) {
			xrtHttpResponseDestroy(pResponse);
			return NULL;
		}
	}
	if ( (pCall->CacheRangeState ==
		  __XRT_HTTP_CLIENT_CACHE_RANGE_NONE) &&
		!__xrtHttpClientCacheHeadRequest(pCall) ) {
		for ( i = 0;
			i < xrtHttpCacheRecordTrailerCount(pRecord);
			i++ ) {
			const xhttpfield* pField =
				xrtHttpCacheRecordTrailerAt(pRecord, i);

			if ( (pField == NULL) ||
				!xrtHttpBridgeResponseAddTrailer(
					pResponse,
					&pCall->Client->Config.Exchange.Trailers,
					pField->Name,
					pField->Value
				) ) {
				xrtHttpResponseDestroy(pResponse);
				return NULL;
			}
		}
	}
	if ( !xrtHttpBridgeResponseSetUrl(
		pResponse,
		xrtHttpRequestUrlText(pCall->Request)
	) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	xrtHttpBridgeResponseSetFlags(
		pResponse,
		XHTTP_RESPONSE_STREAMED
	);
	if ( xrtHttpCacheTimeParse(
		xrtHttpHeadersData(
			xrtHttpResponseHeaders(pResponse)
		),
		xrtHttpHeadersCount(
			xrtHttpResponseHeaders(pResponse)
		),
		&Time
	) && (xrtHttpCacheCurrentAge(
		&Time,
		xrtHttpCacheRecordResponseTime(pRecord),
		xrtHttpCacheRecordRequestClock(pRecord),
		xrtHttpCacheRecordResponseClock(pRecord),
		xrtTimer(),
		&Age
	) == XHTTP_CACHE_CALC_READY) ) {
		iAge = snprintf(
			sAge,
			sizeof(sAge),
			"%llu",
			(unsigned long long)Age.CurrentAgeSeconds
		);
		if ( (iAge < 0) ||
			((size_t)iAge >= sizeof(sAge)) ||
			!xrtHttpHeadersSet(
				(xhttpheaders*)xrtHttpResponseHeaders(
					pResponse
				),
				XRT_STR_LITERAL("Age"),
				(xstrview){ sAge, (size_t)iAge }
			) ) {
			xrtHttpResponseDestroy(pResponse);
			return NULL;
		}
	} else {
		xrtClearError();
	}
	if ( !__xrtHttpClientCacheResponseRange(
		pCall,
		pResponse,
		pRecord
	) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	return pResponse;
}



/* 把缓存重建过程中的回调失败提升为唯一高层终态。 */
static void __xrtHttpClientCacheCallbackFail(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( __xrtHttpRedirectFail(pCall, pCause) ) {
			xrtHttpResponseDestroy(pResponse);
			xrtErrorFree(pCause);
			return;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( __xrtHttpDecompressFail(pCall, pCause) ) {
			xrtHttpResponseDestroy(pResponse);
			xrtErrorFree(pCause);
			return;
		}
	#endif
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_CALLBACK,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_CANCELLED
		),
		"deliver-http-cache-response",
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
}



/* 缓存重放交付区分继续、回调已终止和内部错误。 */
typedef enum __xrt_http_client_cache_delivery {
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR = -1,
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP = 0,
	__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE = 1
} __xrt_http_client_cache_delivery;



/* 以响应限额终止缓存重放，保持与网络 Exchange 相同的高层错误分类。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheBodyLimitFail(
	xhttpcall* pCall,
	xhttpresponse* pResponse
)
{
	xerror* pCause;

	xrtSetErrorKind(XERR_RANGE);
	pCause = xrtTakeError();
	xrtHttpResponseDestroy(pResponse);
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_RESPONSE,
		XERR_RANGE,
		"receive-http-response",
		"HTTP response exceeds its configured body limit",
		pCause
	);
	xrtErrorFree(pCause);
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
}



/* 截取一个缓存片段与指定范围的交集；空范围表示完整片段。 */
static xbytesview __xrtHttpClientCachePartView(
	const xhttpcachepart* pPart,
	const xhttpbyterange* pRange
)
{
	xbytesview Data = pPart->Data;
	uint64 iPartLast;
	uint64 iFirst;
	uint64 iLast;
	size_t iOffset;
	size_t iSize;

	if ( pRange == NULL ) {
		return Data;
	}
	if ( Data.Size == 0 ) {
		return (xbytesview){ NULL, 0 };
	}

	iPartLast = pPart->Offset +
		(uint64)pPart->Data.Size -
		UINT64_C(1);
	if ( (iPartLast < pRange->First) ||
		(pPart->Offset > pRange->Last) ) {
		return (xbytesview){ NULL, 0 };
	}
	iFirst = pPart->Offset >
		pRange->First ?
			pPart->Offset :
			pRange->First;
	iLast = iPartLast <
		pRange->Last ?
			iPartLast :
			pRange->Last;
	iOffset = (size_t)(iFirst - pPart->Offset);
	iSize = (size_t)(
		(iLast - iFirst) + UINT64_C(1)
	);
	return (xbytesview){
		pPart->Data.Data + iOffset,
		iSize
	};
}



/* 把一段缓存或 framing 字节交给缓冲响应或用户 Body 回调。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliver(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	xbytesview Data
)
{
	uint64 iDelivered;
	bool bAccepted;

	if ( Data.Size == 0 ) {
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
	}
	iDelivered = xrtHttpResponseWireBodyBytes(pResponse);
	if ( (iDelivered > pCall->ResponseBodyLimit) ||
		((uint64)Data.Size >
		 (pCall->ResponseBodyLimit - iDelivered)) ) {
		return __xrtHttpClientCacheBodyLimitFail(
			pCall,
			pResponse
		);
	}
	/* 与网络 Exchange 一致，回调可见计数必须包含当前线路块。 */
	if ( !xrtHttpBridgeResponseAddWireBody(
		pResponse,
		(uint64)Data.Size
	) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response body byte count overflowed"
		);
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
	}
	/* Header 回调留下的非失败错误不能污染下一次 Body 回调。 */
	xrtClearError();
	if ( pCall->CacheNext.Body == NULL ) {
		bAccepted =
			xrtHttpBridgeResponseBufferDeliveredBody(
				pResponse,
				Data
			);
	} else {
		bAccepted = pCall->CacheNext.Body(
			pResponse,
			Data,
			pCall->CacheNext.Data
		);
	}
	if ( !bAccepted ||
		!xrtHttpBridgeResponseDeliverBody(
			pResponse,
			(uint64)Data.Size
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response body callback stopped delivery"
		);
		return __XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP;
	}
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
}



/* 从有序缓存片段流式交付一个完整或选定范围。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliverRange(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord,
	const xhttpbyterange* pRange,
	uint64* pDelivered
)
{
	uint64 iDelivered = 0;
	size_t i;

	for ( i = 0;
		i < xrtHttpCacheRecordPartCount(pRecord);
		i++ ) {
		const xhttpcachepart* pPart =
			xrtHttpCacheRecordPartAt(pRecord, i);
		xbytesview Data;
		__xrt_http_client_cache_delivery Delivery;

		if ( pPart == NULL ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
		}
		Data = __xrtHttpClientCachePartView(
			pPart,
			pRange
		);
		if ( Data.Size == 0 ) {
			continue;
		}
		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			Data
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			return Delivery;
		}
		if ( iDelivered >
			(UINT64_MAX - (uint64)Data.Size) ) {
			xrtSetErrorKind(XERR_RANGE);
			return __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
		}
		iDelivered += (uint64)Data.Size;
	}
	if ( pDelivered != NULL ) {
		*pDelivered = iDelivered;
	}
	return __XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
}



/* 流式交付 multipart/byteranges framing 与缓存正文切片。 */
static __xrt_http_client_cache_delivery
__xrtHttpClientCacheDeliverMultipart(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	const xhttpcacherecord* pRecord
)
{
	const xhttpbyterange* pRanges =
		__xrtHttpClientCacheRanges(pCall);
	xstrview ContentType =
		__xrtHttpClientCacheContentType(pRecord);
	xstrview Boundary = {
		pCall->CacheBoundary,
		XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE
	};
	bytes pHead = NULL;
	size_t iHeadCapacity = 0;
	uint8 sEnd[2];
	uint8 sClose[XRT_HTTP_CLIENT_CACHE_BOUNDARY_SIZE + 6u];
	uint64 iCompleteLength =
		xrtHttpCacheRecordLength(pRecord);
	uint64 iDelivered = 0;
	size_t iEndSize;
	size_t iCloseSize;
	size_t i;
	__xrt_http_client_cache_delivery Delivery =
		__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;

	if ( (pRanges == NULL) ||
		(pCall->CacheRangeCount < 2u) ) {
		xrtSetErrorKind(XERR_STATE);
		return Delivery;
	}
	if ( !xrtHttpRangeMultipartEndWrite(
			sEnd,
			sizeof(sEnd),
			&iEndSize
		) || !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			sClose,
			sizeof(sClose),
			&iCloseSize
		) ) {
		return Delivery;
	}

	for ( i = 0; i < pCall->CacheRangeCount; i++ ) {
		bytes pNext;
		size_t iHeadSize;
		size_t iWritten;
		uint64 iPayload;

		if ( !xrtHttpRangeMultipartHeadWrite(
			&pRanges[i],
			iCompleteLength,
			ContentType,
			Boundary,
			NULL,
			0,
			&iHeadSize
		) ) {
			goto cleanup;
		}
		if ( iHeadSize > iHeadCapacity ) {
			pNext = (bytes)xrtRealloc(
				pHead,
				iHeadSize
			);
			if ( pNext == NULL ) {
				goto cleanup;
			}
			pHead = pNext;
			iHeadCapacity = iHeadSize;
		}
		if ( !xrtHttpRangeMultipartHeadWrite(
			&pRanges[i],
			iCompleteLength,
			ContentType,
			Boundary,
			pHead,
			iHeadCapacity,
			&iWritten
		) || (iWritten != iHeadSize) ) {
			goto cleanup;
		}
		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			(xbytesview){ pHead, iHeadSize }
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		iDelivered += (uint64)iHeadSize;

		Delivery = __xrtHttpClientCacheDeliverRange(
			pCall,
			pResponse,
			pRecord,
			&pRanges[i],
			&iPayload
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		if ( iPayload !=
			((pRanges[i].Last - pRanges[i].First) +
			 UINT64_C(1)) ) {
			xrtSetErrorKind(XERR_INTERNAL);
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
			goto cleanup;
		}
		iDelivered += iPayload;

		Delivery = __xrtHttpClientCacheDeliver(
			pCall,
			pResponse,
			(xbytesview){ sEnd, iEndSize }
		);
		if ( Delivery !=
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
			goto cleanup;
		}
		iDelivered += (uint64)iEndSize;
	}

	Delivery = __xrtHttpClientCacheDeliver(
		pCall,
		pResponse,
		(xbytesview){ sClose, iCloseSize }
	);
	if ( Delivery !=
		__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE ) {
		goto cleanup;
	}
	iDelivered += (uint64)iCloseSize;
	if ( iDelivered != pCall->CacheRangeBodyLength ) {
		xrtSetErrorKind(XERR_INTERNAL);
		Delivery = __XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR;
	}

cleanup:
	xrtFree(pHead);
	return Delivery;
}



/* 交付完整缓存记录并复用重定向、解压和用户事件链。 */
static bool __xrtHttpClientCacheReplay(
	xhttpcall* pCall,
	xhttpcacherecord* pRecord
)
{
	xhttpresponse* pResponse =
		__xrtHttpClientCacheResponse(
			pCall,
			pRecord
		);

	if ( pResponse == NULL ) {
		return false;
	}
	if ( (pCall->CacheNext.Headers != NULL) &&
		!pCall->CacheNext.Headers(
			pResponse,
			pCall->CacheNext.Data
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache response Header callback stopped delivery"
		);
		return true;
	}
	{
		__xrt_http_client_cache_delivery Delivery =
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		const xhttpbyterange* pRange = NULL;

		if ( __xrtHttpClientCacheHeadRequest(pCall) ) {
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		} else if ( pCall->CacheRangeState ==
			__XRT_HTTP_CLIENT_CACHE_RANGE_MULTIPART ) {
			Delivery =
				__xrtHttpClientCacheDeliverMultipart(
					pCall,
					pResponse,
					pRecord
				);
		} else if ( pCall->CacheRangeState ==
			__XRT_HTTP_CLIENT_CACHE_RANGE_UNSATISFIABLE ) {
			Delivery =
				__XRT_HTTP_CLIENT_CACHE_DELIVERY_CONTINUE;
		} else {
			if ( pCall->CacheRangeState ==
				__XRT_HTTP_CLIENT_CACHE_RANGE_PARTIAL ) {
				pRange = &pCall->CacheRange;
			}
			Delivery = __xrtHttpClientCacheDeliverRange(
				pCall,
				pResponse,
				pRecord,
				pRange,
				NULL
			);
		}
		if ( Delivery ==
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_STOP ) {
			return true;
		}
		if ( Delivery ==
			__XRT_HTTP_CLIENT_CACHE_DELIVERY_ERROR ) {
			xrtHttpResponseDestroy(pResponse);
			return false;
		}
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		if ( pCall->RedirectPending ) {
			xerror* pCause;

			xrtHttpResponseDestroy(pResponse);
			if ( !__xrtHttpRedirectAdvance(pCall) ) {
				pCause = xrtTakeError();
				(void)__xrtHttpRedirectFail(
					pCall,
					pCause
				);
				xrtErrorFree(pCause);
				return true;
			}
			__xrtHttpCallStartHop(pCall);
			return true;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		if ( !__xrtHttpDecompressFinish(
			pCall,
			pResponse
		) ) {
			__xrtHttpClientCacheCallbackFail(
				pCall,
				pResponse,
				"HTTP cache response final decoding failed"
			);
			return true;
		}
	#endif
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false
	);
	return true;
}



/* 构造 only-if-cached 未命中的 504 空响应。 */
static xhttpresponse* __xrtHttpClientCacheGatewayTimeout(
	xhttpcall* pCall
)
{
	xhttpresponse* pResponse = xrtHttpBridgeResponseCreate(
		XHTTP_VERSION_1_1,
		XHTTP_STATUS_GATEWAY_TIMEOUT,
		XRT_STR_LITERAL("Gateway Timeout"),
		&pCall->Client->Config.Exchange.Headers
	);

	if ( (pResponse == NULL) ||
		!xrtHttpBridgeResponseAddHeader(
			pResponse,
			XRT_STR_LITERAL("Content-Length"),
			XRT_STR_LITERAL("0")
		) || !xrtHttpBridgeResponseSetUrl(
			pResponse,
			xrtHttpRequestUrlText(pCall->Request)
		) ) {
		xrtHttpResponseDestroy(pResponse);
		return NULL;
	}
	xrtHttpBridgeResponseSetFlags(
		pResponse,
		XHTTP_RESPONSE_STREAMED
	);
	return pResponse;
}



/* 在连接池前交付 ready 命中或合成 504。 */
bool __xrtHttpClientCacheStart(
	xhttpcall* pCall,
	bool* pHandled
)
{
	xhttpresponse* pResponse;

	if ( (pCall == NULL) || (pHandled == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pHandled = false;
	if ( !pCall->CacheEnabled || !pCall->CacheReady ) {
		pCall->CacheRequestClock = xrtTimer();
		return true;
	}
	*pHandled = true;
	__xrtHttpCallSetPhase(
		pCall,
		XHTTP_CALL_PHASE_CACHE
	);
	if ( pCall->CacheCandidate != NULL ) {
		return __xrtHttpClientCacheReplay(
			pCall,
			pCall->CacheCandidate
		);
	}
	pResponse = __xrtHttpClientCacheGatewayTimeout(pCall);
	if ( pResponse == NULL ) {
		return false;
	}
	if ( (pCall->CacheNext.Headers != NULL) &&
		!pCall->CacheNext.Headers(
			pResponse,
			pCall->CacheNext.Data
		) ) {
		__xrtHttpClientCacheCallbackFail(
			pCall,
			pResponse,
			"HTTP cache 504 Header callback stopped delivery"
		);
		return true;
	}
	__xrtHttpCallSucceed(
		pCall,
		pResponse,
		NULL,
		#if defined(XHTTP_FEATURE_HTTP_CLIENT_HTTPS)
			NULL,
		#endif
		0,
		false
	);
	return true;
}



/* 保存完整响应，或把自有 304 合并为待重放记录。 */
bool __xrtHttpClientCacheDone(
	xhttpcall* pCall,
	xhttpresponse* pResponse,
	bool* pReplayed
)
{
	xhttpcacherecord* pRecord;
	xhttpcacheput Put;
	bool bHeadHandled;

	if ( (pCall == NULL) ||
		(pResponse == NULL) ||
		(pReplayed == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pReplayed = false;
	if ( !pCall->CacheEnabled ) {
		return true;
	}
	if ( !__xrtHttpClientCacheInvalidate(
		pCall,
		pResponse
	) ) {
		return false;
	}
	if ( !pCall->CacheNotModified ) {
		if ( !__xrtHttpClientCacheHeadFreshen(
			pCall,
			pResponse,
			&bHeadHandled
		) ) {
			return false;
		}
		if ( bHeadHandled ) {
			return true;
		}
		return __xrtHttpClientCacheStore(
			pCall,
			pResponse
		);
	}
	pRecord = __xrtHttpClientCacheUpdateRecord(
		pCall,
		true
	);
	if ( pRecord == NULL ) {
		return __xrtHttpClientCacheError(
			pCall,
			__xrtHttpClientCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			"update-http-cache-validation",
			"HTTP 304 could not update the selected cache record",
			xrtGetError()
		);
	}
	Put = xrtHttpCacheReplace(
		pCall->Client->Cache,
		pCall->CacheCandidate,
		pRecord
	);
	if ( Put == XHTTP_CACHE_PUT_ERROR ) {
		if ( !__xrtHttpClientCacheRecover(
			pCall,
			"store-http-cache-validation",
			"HTTP cache backend could not store the validated response"
		) ) {
			xrtHttpCacheRecordRelease(pRecord);
			return false;
		}
	}
	xrtHttpCacheRecordRelease(pCall->CacheCandidate);
	pCall->CacheCandidate = pRecord;
	pCall->CacheReady = true;
	pCall->CacheNotModified = false;
	__xrtHttpClientCacheOutcome(
		pCall,
		XHTTP_CLIENT_CACHE_REVALIDATED
	);
	*pReplayed = true;
	return true;
}



/* 把严格模式缓存回调错误提升为稳定客户端错误。 */
bool __xrtHttpClientCacheFail(
	xhttpcall* pCall,
	const xerror* pCause
)
{
	if ( (pCall == NULL) || !pCall->CacheFailed ) {
		return false;
	}
	__xrtHttpCallFail(
		pCall,
		XNET_RESULT_ERROR,
		XHTTP_CLIENT_ERROR_CACHE,
		__xrtHttpClientCauseKind(
			pCause,
			XERR_IO
		),
		"process-http-cache",
		"HTTP client cache processing failed",
		pCause
	);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_cache_multipart.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)




#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

#define XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX 64u
#define XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADER_BYTES_MAX (64u * 1024u)



/* 判断两个借用文本是否逐字节相同。 */
static bool __xrtHttpClientCacheMultipartViewEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 保守比较两个 Part Content-Type，类型标记忽略大小写，参数保持原值。 */
static bool __xrtHttpClientCacheMultipartTypeEqual(
	const xmediatype* pLeft,
	const xmediatype* pRight
)
{
	xstrview LeftParameters =
		xrtHttpOwsTrim(pLeft->Parameters);
	xstrview RightParameters =
		xrtHttpOwsTrim(pRight->Parameters);

	return xrtHttpTokenEqual(
			pLeft->Type,
			pRight->Type
		) && xrtHttpTokenEqual(
			pLeft->Subtype,
			pRight->Subtype
		) && __xrtHttpClientCacheMultipartViewEqual(
			LeftParameters,
			RightParameters
		);
}



/* 发布不属于 multipart/byteranges 的无错误结果。 */
static __xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartNone(
	__xrt_http_client_cache_multipart* pPlan
)
{
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_NONE;
	return pPlan->Decision;
}



/* 释放局部片段并发布带稳定原因的保守跳过结果。 */
static __xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartSkip(
	xhttpcachepart* pParts,
	uint32 iReasons,
	xerrkind Kind,
	__xrt_http_client_cache_multipart* pPlan
)
{
	xrtFree(pParts);
	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_SKIP;
	pPlan->Reasons = iReasons;
	if ( Kind == XERR_RANGE ) {
		xrtSetErrorKind(XERR_RANGE);
	} else {
		xrtSetErrorKind(XERR_VALUE);
	}
	return pPlan->Decision;
}



/* 严格读取 Part 字段块到固定上限的借用数组。 */
static bool __xrtHttpClientCacheMultipartFields(
	const xmultipartpart* pPart,
	xhttpfield Fields[
		XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX
	],
	size_t* pCount
)
{
	xhttpfield Field;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iCount = 0;

	*pCount = 0;
	if ( (pPart->HeaderCount >
		  XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX) ||
		(pPart->Headers.Size >
		 XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADER_BYTES_MAX) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	while ( (Next = xrtHttpFieldNext(
		pPart->Headers,
		&iOffset,
		&Field
	)) == XHTTP_NEXT_ITEM ) {
		if ( iCount >=
			XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		Fields[iCount++] = Field;
	}
	if ( (Next != XHTTP_NEXT_END) ||
		(iCount != pPart->HeaderCount) ) {
		if ( Next != XHTTP_NEXT_ERROR ) {
			xrtSetErrorKind(XERR_VALUE);
		}
		return false;
	}
	*pCount = iCount;
	return true;
}



/* 拒绝 Part 级传输编码，避免把编码载荷误当表示字节。 */
static bool __xrtHttpClientCacheMultipartEncoding(
	const xmultipartpart* pPart,
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	if ( (pPart->Flags &
		  XMULTIPART_PART_TRANSFER_ENCODING) != 0 ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Transfer-Encoding")
		) ) {
			return false;
		}
	}
	return true;
}



/* 验证可选 Part Content-Length 的全部值相同且匹配实际正文。 */
static bool __xrtHttpClientCacheMultipartLength(
	const xhttpfield* pFields,
	size_t iCount,
	size_t iBodySize
)
{
	uint64 iLength = 0;
	bool bPresent = false;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		uint64 iCurrent;

		if ( !xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Length")
		) ) {
			continue;
		}
		if ( !xrtHttpContentLengthParse(
			pFields[i].Value,
			&iCurrent
		) || (bPresent && (iCurrent != iLength)) ) {
			return false;
		}
		iLength = iCurrent;
		bPresent = true;
	}
	return !bPresent || (iLength == (uint64)iBodySize);
}



/* 返回 Part 原始 Content-Type 字段，通用解析器已经拒绝重复值。 */
static xstrview __xrtHttpClientCacheMultipartContentType(
	const xhttpfield* pFields,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( xrtHttpFieldNameEqual(
			pFields[i].Name,
			XRT_STR_LITERAL("Content-Type")
		) ) {
			return xrtHttpOwsTrim(pFields[i].Value);
		}
	}
	return (xstrview){ NULL, 0 };
}



/* 按表示偏移对借用片段执行稳定插入排序。 */
static void __xrtHttpClientCacheMultipartSort(
	xhttpcachepart* pParts,
	size_t iCount
)
{
	size_t i;

	for ( i = 1; i < iCount; i++ ) {
		xhttpcachepart Part = pParts[i];
		size_t j = i;

		while ( (j != 0) &&
			(pParts[j - 1u].Offset > Part.Offset) ) {
			pParts[j] = pParts[j - 1u];
			j--;
		}
		pParts[j] = Part;
	}
}



/* 合并重复或相交 Part；重叠字节不一致时拒绝整组表示。 */
static bool __xrtHttpClientCacheMultipartNormalize(
	xhttpcachepart* pParts,
	size_t iCount,
	size_t* pNormalized
)
{
	size_t iWrite = 0;
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		xhttpcachepart Part = pParts[i];
		uint64 iPartLast;

		if ( (Part.Data.Size == 0) ||
			(((uint64)Part.Data.Size -
			  UINT64_C(1)) >
			 (UINT64_MAX - Part.Offset)) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		iPartLast =
			Part.Offset +
			(uint64)Part.Data.Size -
			UINT64_C(1);
		if ( iWrite != 0 ) {
			xhttpcachepart* pPrevious =
				&pParts[iWrite - 1u];
			uint64 iPreviousLast =
				pPrevious->Offset +
				(uint64)pPrevious->Data.Size -
				UINT64_C(1);

			if ( Part.Offset <= iPreviousLast ) {
				uint64 iOverlapLast =
					iPartLast < iPreviousLast ?
						iPartLast :
						iPreviousLast;
				size_t iOverlap = (size_t)(
					(iOverlapLast -
					 Part.Offset) +
					UINT64_C(1)
				);
				size_t iPreviousOffset =
					(size_t)(
						Part.Offset -
						pPrevious->Offset
					);

				if ( memcmp(
					pPrevious->Data.Data +
						iPreviousOffset,
					Part.Data.Data,
					iOverlap
				) != 0 ) {
					xrtSetErrorKind(XERR_VALUE);
					return false;
				}
				if ( iPartLast <= iPreviousLast ) {
					continue;
				}
				Part.Offset =
					iPreviousLast +
					UINT64_C(1);
				Part.Data.Data += iOverlap;
				Part.Data.Size -= iOverlap;
			}
		}
		pParts[iWrite++] = Part;
	}
	*pNormalized = iWrite;
	return true;
}



/* 验证所有未知长度 Part 也落在后来确认的完整长度内。 */
static bool __xrtHttpClientCacheMultipartWithinLength(
	const xhttpcachepart* pParts,
	size_t iCount,
	uint64 iLength
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( (pParts[i].Offset >= iLength) ||
			((uint64)pParts[i].Data.Size >
			 (iLength - pParts[i].Offset)) ) {
			return false;
		}
	}
	return true;
}



/* 判断规范片段是否无空洞覆盖已知完整表示。 */
static bool __xrtHttpClientCacheMultipartComplete(
	const xhttpcachepart* pParts,
	size_t iCount,
	uint64 iLength
)
{
	uint64 iNext = 0;
	size_t i;

	if ( (iLength == 0) || (iCount == 0) ) {
		return false;
	}
	for ( i = 0; i < iCount; i++ ) {
		uint64 iSize =
			(uint64)pParts[i].Data.Size;

		if ( pParts[i].Offset != iNext ) {
			return false;
		}
		if ( iSize > (UINT64_MAX - iNext) ) {
			return false;
		}
		iNext += iSize;
	}
	return iNext == iLength;
}



/* 按实际 Part 数小步扩展描述数组，不按用户上限预留内存。 */
static bool __xrtHttpClientCacheMultipartReserve(
	xhttpcachepart** ppParts,
	size_t* pCapacity,
	size_t iRequired,
	size_t iMaxParts
)
{
	xhttpcachepart* pParts;
	size_t iCapacity;
	size_t iBytes;

	if ( iRequired <= *pCapacity ) {
		return true;
	}
	iCapacity = *pCapacity != 0 ?
		*pCapacity :
		(iMaxParts < 4u ? iMaxParts : 4u);
	while ( iCapacity < iRequired ) {
		if ( iCapacity > (iMaxParts / 2u) ) {
			iCapacity = iMaxParts;
		} else {
			iCapacity *= 2u;
		}
	}
	if ( (iCapacity < iRequired) ||
		(iCapacity >
		 (SIZE_MAX / sizeof(**ppParts))) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	iBytes = iCapacity * sizeof(**ppParts);
	pParts = *ppParts == NULL ?
		(xhttpcachepart*)xrtMalloc(iBytes) :
		(xhttpcachepart*)xrtRealloc(*ppParts, iBytes);
	if ( pParts == NULL ) {
		return false;
	}
	*ppParts = pParts;
	*pCapacity = iCapacity;
	return true;
}



/* 释放源站 multipart 范围计划。 */
void __xrtHttpClientCacheMultipartUnit(
	__xrt_http_client_cache_multipart* pPlan
)
{
	if ( pPlan == NULL ) {
		return;
	}
	xrtFree(pPlan->Parts);
	memset(pPlan, 0, sizeof(*pPlan));
}



/* 把源站 multipart/byteranges 正文转换为规范缓存片段。 */
__xrt_http_client_cache_multipart_decision
__xrtHttpClientCacheMultipartPlan(
	const xhttpcachefragmentinput* pBase,
	xstrview ContentType,
	xbytesview Body,
	size_t iMaxParts,
	__xrt_http_client_cache_multipart* pPlan
)
{
	xhttpfield Fields[
		XRT_HTTP_CLIENT_CACHE_MULTIPART_HEADERS_MAX
	];
	xmultipartboundary Boundary;
	xmultiparterrorinfo MultipartError;
	xmultipartpart MultipartPart;
	xhttpcachefragmentinput Input;
	xhttpcachefragmentplan FragmentPlan;
	xhttpcachefragmentdecision FragmentDecision;
	xhttpcachepart* pParts = NULL;
	xmediatype OuterType;
	xmediatype SelectedType;
	xstrview SelectedText = { NULL, 0 };
	uint64 iLength = 0;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t iCapacity = 0;
	size_t iNormalized = 0;
	uint32 iFlags = 0;
	xhttpnext Next;

	if ( (pPlan == NULL) || (pBase == NULL) ||
		!__xhttpViewValid(ContentType) ||
		((Body.Data == NULL) && (Body.Size != 0)) ||
		(iMaxParts == 0) ||
		(pBase->Status != XHTTP_STATUS_PARTIAL_CONTENT) ||
		(pBase->RangeFields != NULL) ||
		(pBase->RangeFieldCount != 0) ||
		((pBase->Flags &
		  XHTTP_CACHE_FRAGMENT_MULTIPART_PART) != 0) ||
		xrtMemRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			pBase,
			sizeof(*pBase)
		) || xrtMemRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			ContentType.Data,
			ContentType.Size
		) || xrtMemRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			Body.Data,
			Body.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
	}
	if ( !xrtHttpMediaTypeParse(
		ContentType,
		&OuterType
	) ) {
		xrtClearError();
		return __xrtHttpClientCacheMultipartNone(pPlan);
	}
	if ( !xrtHttpTokenEqual(
			OuterType.Type,
			XRT_STR_LITERAL("multipart")
		) || !xrtHttpTokenEqual(
			OuterType.Subtype,
			XRT_STR_LITERAL("byteranges")
		) ) {
		return __xrtHttpClientCacheMultipartNone(pPlan);
	}
	if ( !xrtMultipartBoundaryFromContentType(
		ContentType,
		&Boundary
	) ) {
		return __xrtHttpClientCacheMultipartSkip(
			NULL,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BOUNDARY,
			XERR_VALUE,
			pPlan
		);
	}
	memset(&SelectedType, 0, sizeof(SelectedType));

	for ( ;; ) {
		size_t iFieldCount;
		xstrview PartType;

		Next = xrtMultipartNext(
			Body,
			&Boundary,
			&iOffset,
			&MultipartPart,
			&MultipartError
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_BODY,
				XERR_VALUE,
				pPlan
			);
		}
		if ( Next == XHTTP_NEXT_END ) {
			break;
		}
		if ( iCount >= iMaxParts ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS,
				XERR_RANGE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartReserve(
			&pParts,
			&iCapacity,
			iCount + 1u,
			iMaxParts
		) ) {
			xrtFree(pParts);
			return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
		}
		if ( !__xrtHttpClientCacheMultipartFields(
			&MultipartPart,
			Fields,
			&iFieldCount
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_HEADERS,
				xrtErrorIs(xrtGetError(), XERR_RANGE) != NULL ?
					XERR_RANGE :
					XERR_VALUE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartEncoding(
			&MultipartPart,
			Fields,
			iFieldCount
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_ENCODING,
				XERR_VALUE,
				pPlan
			);
		}
		if ( !__xrtHttpClientCacheMultipartLength(
			Fields,
			iFieldCount,
			MultipartPart.Body.Size
		) ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
				XERR_VALUE,
				pPlan
			);
		}

		Input = *pBase;
		Input.RangeFields = Fields;
		Input.RangeFieldCount = iFieldCount;
		Input.BodySize =
			(uint64)MultipartPart.Body.Size;
		Input.Flags |=
			XHTTP_CACHE_FRAGMENT_MULTIPART_PART |
			XHTTP_CACHE_FRAGMENT_BODY_COMPLETE;
		FragmentDecision = xrtHttpCacheFragmentPlan(
			&Input,
			&FragmentPlan
		);
		if ( FragmentDecision ==
			XHTTP_CACHE_FRAGMENT_ERROR ) {
			xrtFree(pParts);
			return __XRT_HTTP_CLIENT_CACHE_MULTIPART_ERROR;
		}
		if ( FragmentDecision !=
			XHTTP_CACHE_FRAGMENT_STORE ) {
			return __xrtHttpClientCacheMultipartSkip(
				pParts,
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_RANGE,
				XERR_VALUE,
				pPlan
			);
		}
		if ( (FragmentPlan.Fragment.Flags &
			  XHTTP_CACHE_FRAGMENT_HAS_LENGTH) != 0 ) {
			if ( ((iFlags &
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
				(iLength !=
				 FragmentPlan.Fragment.Length) ) {
				return __xrtHttpClientCacheMultipartSkip(
					pParts,
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
					XERR_VALUE,
					pPlan
				);
			}
			iLength = FragmentPlan.Fragment.Length;
			iFlags |=
				__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH;
		}

		PartType = __xrtHttpClientCacheMultipartContentType(
			Fields,
			iFieldCount
		);
		if ( PartType.Data != NULL ) {
			if ( (iFlags &
				  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE) == 0 ) {
				SelectedType = MultipartPart.ContentType;
				SelectedText = PartType;
				iFlags |=
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_CONTENT_TYPE;
			} else if (
				!__xrtHttpClientCacheMultipartTypeEqual(
					&SelectedType,
					&MultipartPart.ContentType
				)
			) {
				return __xrtHttpClientCacheMultipartSkip(
					pParts,
					__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_CONTENT_TYPE,
					XERR_VALUE,
					pPlan
				);
			}
		}

		pParts[iCount].Offset =
			FragmentPlan.Fragment.Range.First;
		pParts[iCount].Data =
			MultipartPart.Body;
		iCount++;
	}
	if ( iCount == 0 ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_PARTS,
			XERR_VALUE,
			pPlan
		);
	}
	__xrtHttpClientCacheMultipartSort(
		pParts,
		iCount
	);
	if ( !__xrtHttpClientCacheMultipartNormalize(
		pParts,
		iCount,
		&iNormalized
	) ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_OVERLAP,
			XERR_VALUE,
			pPlan
		);
	}
	if ( ((iFlags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
		!__xrtHttpClientCacheMultipartWithinLength(
			pParts,
			iNormalized,
			iLength
		) ) {
		return __xrtHttpClientCacheMultipartSkip(
			pParts,
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_REASON_LENGTH,
			XERR_VALUE,
			pPlan
		);
	}
	if ( ((iFlags &
		  __XRT_HTTP_CLIENT_CACHE_MULTIPART_HAS_LENGTH) != 0) &&
		__xrtHttpClientCacheMultipartComplete(
			pParts,
			iNormalized,
			iLength
		) ) {
		iFlags |=
			__XRT_HTTP_CLIENT_CACHE_MULTIPART_COMPLETE;
	}

	memset(pPlan, 0, sizeof(*pPlan));
	pPlan->Parts = pParts;
	pPlan->ContentType = SelectedText;
	pPlan->Length = iLength;
	pPlan->PartCount = iNormalized;
	pPlan->Decision =
		__XRT_HTTP_CLIENT_CACHE_MULTIPART_STORE;
	pPlan->Flags = iFlags;
	return pPlan->Decision;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_sse_client.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)



#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)

/* 创建 SSE Client 层错误，并保留明确的下层原因。 */
static xerror* __xrtHttpSseClientErrorCreate(
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	return pCause != NULL ?
		xrtErrorWrap(
			pCause,
			Kind,
			"xrt.http.sse.client",
			(int32)Code,
			sMessage
		) :
		xrtErrorCreate(
			Kind,
			"xrt.http.sse.client",
			(int32)Code,
			sMessage
		);
}



/* 向当前线程发布一个 SSE Client 参数或同步构造错误。 */
static void __xrtHttpSseClientSetError(
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpSseClientErrorCreate(
		Code, Kind, sMessage, pCause
	);

	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES) || \
	defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)

/* 深复制一个可空字符串视图，并保持空视图规范化。 */
static str __xrtHttpSseClientString(xstrview Value)
{
	str sValue;

	if ( !xrtMemRangeValid(Value.Data, Value.Size) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( Value.Size == 0 ) {
		return NULL;
	}
	if ( Value.Size == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	sValue = (str)xrtMalloc(Value.Size + 1u);
	if ( sValue == NULL ) {
		return NULL;
	}
	memcpy(sValue, Value.Data, Value.Size);
	sValue[Value.Size] = '\0';
	return sValue;
}

#endif



/* 初始化 EventSource 兼容 Parser、无调用超时和自动重连策略。 */
XHTTP_CLIENT_API void xrtHttpSseClientConfigInit(
	xhttpsseclientconfig* pConfig
)
{
	xhttpsseclientconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpSseParserConfigInit(&Config.Parser);
	xrtHttpCallOptionsInit(&Config.Http);
	Config.Http.Timeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Config.Http.IdleTimeout = XHTTP_CLIENT_TIMEOUT_NONE;
	Config.Http.ResponseBodyLimit = UINT64_MAX;
	Config.Http.Request.TargetForm = XHTTP1_TARGET_ORIGIN;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		Config.Http.Redirect = XHTTP_REDIRECT_FOLLOW;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		Config.Http.Retry.Mode = XHTTP_RETRY_DISABLED;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		Config.Http.Cache.Mode = XHTTP_CLIENT_CACHE_DISABLED;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		Config.Http.Decompress = XHTTP_DECOMPRESS_AUTO;
	#endif
	Config.MaxReconnects = XHTTP_SSE_RECONNECT_MAX_DEFAULT;
	Config.RetryMin = XHTTP_SSE_RETRY_MIN_DEFAULT;
	Config.RetryMax = XHTTP_SSE_RETRY_MAX_DEFAULT;
	memcpy(pConfig, &Config, sizeof(Config));
}



/* 验证只由 SSE Client 管理的回调、target 和重连范围。 */
static bool __xrtHttpSseClientConfigValid(
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	if ( (pConfig == NULL) || (pEvents == NULL) ||
		(pEvents->Message == NULL) ||
		(pConfig->RetryMin > pConfig->RetryMax) ||
		(pConfig->Http.Request.TargetForm !=
		 XHTTP1_TARGET_ORIGIN) ||
		(pConfig->Http.Request.CustomTarget.Size != 0) ||
		(pConfig->Http.Events.Informational != NULL) ||
		(pConfig->Http.Events.Headers != NULL) ||
		(pConfig->Http.Events.Body != NULL) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_VALUE,
			"HTTP SSE client config or callbacks are invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpSseParserConfigValid(&pConfig->Parser) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_VALUE,
			"HTTP SSE parser config is invalid",
			NULL
		);
		return false;
	}
	return true;
}



/* 解析可选配置和必需事件表，允许固定公开值位于未对齐存储。 */
static bool __xrtHttpSseClientInputsResolve(
	const xhttpsseclientconfig* pInput,
	const xhttpsseclientevents* pEventInput,
	xhttpsseclientconfig* pConfig,
	xhttpsseclientevents* pEvents
)
{
	xrtHttpSseClientConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
			__xrtHttpSseClientSetError(
				XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
				XERR_ARGUMENT,
				"HTTP SSE client config range is invalid",
				NULL
			);
			return false;
		}
		memcpy(pConfig, pInput, sizeof(*pConfig));
	}
	if ( !xrtMemRangeValid(pEventInput, sizeof(*pEventInput)) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client event range is invalid",
			NULL
		);
		return false;
	}
	memcpy(pEvents, pEventInput, sizeof(*pEvents));
	return __xrtHttpSseClientConfigValid(pConfig, pEvents);
}



/* 释放会话深持有的 HTTP 选项对象与文本。 */
static void __xrtHttpSseClientOptionsUnit(
	xhttpsseclient* pClient
)
{
	xrtCancelDestroy(pClient->Cancel);
	pClient->Cancel = NULL;
	pClient->Config.Http.Cancel = NULL;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xrtNetProxyRelease(pClient->Proxy);
		pClient->Proxy = NULL;
		pClient->Config.Http.Proxy.Proxy = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		xrtFree(pClient->CookiePartition);
		pClient->CookiePartition = NULL;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		xrtFree(pClient->CachePartition);
		pClient->CachePartition = NULL;
	#endif
}



/* 深持有会跨越多次 HTTP Call 使用的取消、代理和分区字段。 */
static bool __xrtHttpSseClientOptionsInit(
	xhttpsseclient* pClient,
	const xhttpsseclientconfig* pConfig
)
{
	pClient->Config = *pConfig;
	pClient->Config.Http.Events.Informational = NULL;
	pClient->Config.Http.Events.Headers = NULL;
	pClient->Config.Http.Events.Body = NULL;
	pClient->Config.Http.Events.Data = NULL;
	pClient->Config.Http.ResponseBodyLimit = UINT64_MAX;
	pClient->Config.Http.Request.TargetForm =
		XHTTP1_TARGET_ORIGIN;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_REDIRECT)
		pClient->Config.Http.Redirect = XHTTP_REDIRECT_FOLLOW;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_RETRY)
		pClient->Config.Http.Retry.Mode = XHTTP_RETRY_DISABLED;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pClient->Config.Http.Cache.Mode =
			XHTTP_CLIENT_CACHE_DISABLED;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_DECOMPRESS)
		pClient->Config.Http.Decompress = XHTTP_DECOMPRESS_AUTO;
	#endif
	if ( pConfig->Http.Cancel != NULL ) {
		pClient->Cancel = xrtCancelRef(
			pConfig->Http.Cancel
		);
		if ( pClient->Cancel == NULL ) {
			return false;
		}
		pClient->Config.Http.Cancel = pClient->Cancel;
	}
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		if ( pConfig->Http.Proxy.Proxy != NULL ) {
			pClient->Proxy = xrtNetProxyRetain(
				pConfig->Http.Proxy.Proxy
			);
			if ( pClient->Proxy == NULL ) {
				return false;
			}
			pClient->Config.Http.Proxy.Proxy =
				pClient->Proxy;
		}
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		pClient->CookiePartition =
			__xrtHttpSseClientString(
				pConfig->Http.Cookies.PartitionKey
			);
		if ( (pConfig->Http.Cookies.PartitionKey.Size != 0) &&
			(pClient->CookiePartition == NULL) ) {
			return false;
		}
		pClient->Config.Http.Cookies.PartitionKey =
			(xstrview){
				pClient->CookiePartition,
				pConfig->Http.Cookies.PartitionKey.Size
			};
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		pClient->CachePartition =
			__xrtHttpSseClientString(
				pConfig->Http.Cache.PartitionKey
			);
		if ( (pConfig->Http.Cache.PartitionKey.Size != 0) &&
			(pClient->CachePartition == NULL) ) {
			return false;
		}
		pClient->Config.Http.Cache.PartitionKey =
			(xstrview){
				pClient->CachePartition,
				pConfig->Http.Cache.PartitionKey.Size
			};
	#endif
	return true;
}



/* 验证自定义 EventSource 请求的 GET、无正文和无 fragment 契约。 */
static bool __xrtHttpSseClientRequestValid(
	const xhttprequest* pRequest
)
{
	const xurl* pUrl;

	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE request is null",
			NULL
		);
		return false;
	}
	pUrl = xrtHttpRequestUrl(pRequest);
	if ( !xrtHttpMethodEqual(
		xrtHttpRequestMethod(pRequest),
		XRT_STR_LITERAL("GET")
	) || (xrtHttpRequestBody(pRequest) != NULL) ||
		(pUrl == NULL) ||
		((pUrl->Flags & XURL_HAS_FRAGMENT) != 0) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			XERR_VALUE,
			"HTTP SSE request must be a fragment-free GET without a body",
			NULL
		);
		return false;
	}
	return true;
}



/* 增加 SSE 会话引用并拒绝已经释放的对象。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseClientRef(
	xhttpsseclient* pClient
)
{
	if ( (pClient == NULL) ||
		(xrtRefRetain(&pClient->References) < 0) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			pClient == NULL ? XERR_ARGUMENT : XERR_STATE,
			"HTTP SSE client is null or already released",
			NULL
		);
		return NULL;
	}
	return pClient;
}



/* 释放最后一个会话引用和全部已经停止的拥有型资源。 */
XHTTP_CLIENT_API void xrtHttpSseClientDestroy(
	xhttpsseclient* pClient
)
{
	if ( (pClient == NULL) ||
		(xrtRefRelease(&pClient->References) != 0) ) {
		return;
	}
	xrtCancelUnwatch(pClient->CancelWatch);
	__xrtHttpSseClientOptionsUnit(pClient);
	xrtErrorFree(pClient->AttemptError);
	xrtErrorFree(pClient->Error);
	xrtBufferUnit(&pClient->Pending);
	xrtHttpSseParserUnit(&pClient->Parser);
	xrtHttpCallDestroy(pClient->Call);
	xrtHttpRequestDestroy(pClient->Request);
	xrtHttpClientDestroy(pClient->Http);
	xrtSpinUnit(&pClient->Lock);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}



/* 记录一次 Parser、用户回调或响应分类失败。 */
static bool __xrtHttpSseClientAttemptFail(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage,
	const xerror* pCause
)
{
	if ( pClient->AttemptError == NULL ) {
		pClient->AttemptError =
			__xrtHttpSseClientErrorCreate(
				Code, Kind, sMessage, pCause
			);
		if ( pClient->AttemptError == NULL ) {
			pClient->AttemptError = xrtErrorRef(
				xrtGetError()
			);
		}
	}
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_FAILED;
	pClient->AttemptReason = Reason;
	if ( pClient->AttemptError != NULL ) {
		xrtSetError(pClient->AttemptError);
	}
	return false;
}



/* 发布一个借用 Parser 缓冲的项目并更新无锁统计。 */
static bool __xrtHttpSseClientItem(
	xhttpsseclient* pClient,
	const xhttpsseitem* pItem
)
{
	bool bAccepted = true;

	xrtClearError();
	if ( pItem->Kind == XHTTP_SSE_ITEM_EVENT ) {
		bAccepted = pClient->Events.Message(
			pClient,
			&pItem->Message,
			pClient->Events.Data
		);
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->Messages,
				1,
				XMEMORY_RELAXED
			);
		}
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_COMMENT ) {
		if ( pClient->Events.Comment != NULL ) {
			bAccepted = pClient->Events.Comment(
				pClient,
				pItem->Comment,
				pClient->Events.Data
			);
		}
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->Comments,
				1,
				XMEMORY_RELAXED
			);
		}
	} else if ( pItem->Kind == XHTTP_SSE_ITEM_RETRY ) {
		if ( pClient->Events.Retry != NULL ) {
			bAccepted = pClient->Events.Retry(
				pClient,
				pItem->Retry,
				pClient->Events.Data
			);
		}
		if ( bAccepted ) {
			(void)xrtAtomic64FetchAdd(
				&pClient->RetryUpdates,
				1,
				XMEMORY_RELAXED
			);
		}
	} else {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_INTERNAL,
			"HTTP SSE parser returned an unknown item",
			NULL
		);
	}
	if ( !bAccepted ) {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_CALLBACK,
			XHTTP_SSE_CLIENT_ERROR_CALLBACK,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_CANCELLED,
			"HTTP SSE item callback stopped the stream",
			xrtGetError()
		);
	}
	xrtAtomic64Store(
		&pClient->Retry,
		xrtHttpSseParserRetry(&pClient->Parser),
		XMEMORY_RELEASE
	);
	return true;
}



/* 增量解析一个连续输入，并在暂停点复制尚未解析的当前尾段。 */
static bool __xrtHttpSseClientRead(
	xhttpsseclient* pClient,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed
)
{
	size_t iOffset = 0;

	*pConsumed = 0;
	for ( ;; ) {
		xhttpsseerrorinfo Error;
		xhttpsseitem Item;
		xhttpsseparsestatus Status;
		size_t iRead = 0;
		xbytesview Remaining = iOffset == Input.Size ?
			(xbytesview){ NULL, 0 } :
			(xbytesview){
				Input.Data + iOffset,
				Input.Size - iOffset
			};

		Status = xrtHttpSseParserRead(
			&pClient->Parser,
			Remaining,
			bEnd,
			&iRead,
			&Item,
			&Error
		);
		iOffset += iRead;
		if ( Status == XHTTP_SSE_PARSE_ERROR ) {
			*pConsumed = iOffset;
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_PARSE,
				XHTTP_SSE_CLIENT_ERROR_PARSE,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_PROTOCOL,
				"HTTP SSE event stream parsing failed",
				xrtGetError()
			);
		}
		if ( Status == XHTTP_SSE_PARSE_ITEM ) {
			if ( !__xrtHttpSseClientItem(
				pClient, &Item
			) ) {
				*pConsumed = iOffset;
				return false;
			}
			if ( xrtAtomic32Load(
				&pClient->CloseGate,
				XMEMORY_ACQUIRE
			) || xrtAtomic32Load(
				&pClient->PauseGate,
				XMEMORY_ACQUIRE
			) ) {
				*pConsumed = iOffset;
				return true;
			}
			continue;
		}
		if ( (iRead == 0) &&
			(iOffset < Input.Size) ) {
			*pConsumed = iOffset;
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE parser made no input progress",
				NULL
			);
		}
		*pConsumed = iOffset;
		return true;
	}
}



/* HTTP 正文到达时直接解析，暂停只复制本次输入尚未解析的后缀。 */
static bool __xrtHttpSseClientBody(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	size_t iConsumed = 0;

	(void)pCall;
	(void)pResponse;
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( (pClient->Attempt != XRT_HTTP_SSE_ATTEMPT_OPEN) ||
		!__xrtHttpSseClientRead(
			pClient, Data, false, &iConsumed
		) ) {
		return false;
	}
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) && (iConsumed < Data.Size) ) {
		if ( !xrtBufferAssign(
			&pClient->Pending,
			(xbytesview){
				Data.Data + iConsumed,
				Data.Size - iConsumed
			}
		) ) {
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_MEMORY,
				"HTTP SSE paused tail could not be retained",
				xrtGetError()
			);
		}
	}
	return true;
}



/* 最终响应 Header 决定本次尝试是打开、停止还是永久拒绝。 */
static bool __xrtHttpSseClientHeaders(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseresponse Result = xrtHttpSseResponseCheck(
		xrtHttpResponseStatus(pResponse),
		xrtHttpResponseHeaders(pResponse)
	);

	(void)pCall;
	xrtAtomic32Store(
		&pClient->Status,
		xrtHttpResponseStatus(pResponse),
		XMEMORY_RELEASE
	);
	if ( Result == XHTTP_SSE_RESPONSE_STOP ) {
		pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_STOP;
		return true;
	}
	if ( Result != XHTTP_SSE_RESPONSE_OPEN ) {
		pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_REJECTED;
		pClient->AttemptReason = XHTTP_SSE_CLOSE_REJECTED;
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_REJECTED,
			XHTTP_SSE_CLIENT_ERROR_RESPONSE,
			XERR_PROTOCOL,
			"HTTP response is not an EventSource stream",
			Result == XHTTP_SSE_RESPONSE_ERROR ?
				xrtGetError() : NULL
		);
	}
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_OPEN;
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_OPEN,
		XMEMORY_RELEASE
	);
	if ( pClient->Events.Open != NULL ) {
		bool bAccepted;

		xrtClearError();
		bAccepted = pClient->Events.Open(
			pClient,
			pResponse,
			pClient->Events.Data
		);
		if ( !bAccepted ) {
			return __xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_CALLBACK,
				XHTTP_SSE_CLIENT_ERROR_CALLBACK,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_CANCELLED,
				"HTTP SSE open callback stopped the stream",
				xrtGetError()
			);
		}
	}
	return true;
}



/* 原子发布唯一 CLOSED 状态、稳定错误和终态回调。 */
static void __xrtHttpSseClientFinish(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason,
	const xerror* pError
)
{
	uint32 iExpected = 0;
	bool bRelease = false;

	if ( !xrtAtomic32CompareExchange(
		&pClient->FinishGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	if ( Reason == XHTTP_SSE_CLOSE_CANCELLED ) {
		pClient->Error = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"HTTP SSE session was cancelled",
			pError
		);
	} else if ( pError != NULL ) {
		pClient->Error = xrtErrorRef(pError);
	}
	if ( (pClient->Error == NULL) &&
		(Reason != XHTTP_SSE_CLOSE_USER) &&
		(Reason != XHTTP_SSE_CLOSE_STOP) ) {
		if ( xrtGetError() == NULL ) {
			if ( Reason == XHTTP_SSE_CLOSE_CANCELLED ) {
				xrtSetErrorKind(XERR_CANCELLED);
			} else {
				xrtSetErrorKind(XERR_INTERNAL);
			}
		}
		pClient->Error = xrtErrorRef(xrtGetError());
	}
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CLOSED,
		XMEMORY_RELEASE
	);
	if ( pClient->Events.Close != NULL ) {
		pClient->Events.Close(
			pClient,
			Reason,
			pClient->Error,
			pClient->Events.Data
		);
	}
	xrtSpinLock(&pClient->Lock);
	if ( pClient->RuntimeHeld ) {
		pClient->RuntimeHeld = false;
		bRelease = true;
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( bRelease ) {
		xrtHttpSseClientDestroy(pClient);
	}
}



/* 在 Worker 上排空暂停尾段，并仅在仍活动时恢复 HTTP 读取。 */
static void __xrtHttpSseClientResumeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpcall* pCall = NULL;

	(void)pWorker;
	xrtAtomic32Store(
		&pClient->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) || xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		xrtHttpSseClientDestroy(pClient);
		return;
	}
	xrtSpinLock(&pClient->Lock);
	if ( pClient->Call != NULL ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( pCall == NULL ) {
		xrtHttpSseClientDestroy(pClient);
		return;
	}
	xrtAtomic32Store(
		&pClient->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	while ( pClient->Pending.Size != 0 ) {
		size_t iConsumed = 0;

		if ( !__xrtHttpSseClientRead(
			pClient,
			xrtBufferView(&pClient->Pending),
			false,
			&iConsumed
		) ) {
			(void)xrtHttpCallCancel(pCall);
			break;
		}
		if ( (iConsumed == 0) ||
			!xrtBufferRemove(
				&pClient->Pending,
				0,
				iConsumed
			) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE paused tail made no progress",
				xrtGetError()
			);
			(void)xrtHttpCallCancel(pCall);
			break;
		}
		if ( xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	if ( pClient->Pending.Size == 0 ) {
		(void)xrtBufferTrim(&pClient->Pending);
	}
	if ( (pClient->Attempt != XRT_HTTP_SSE_ATTEMPT_FAILED) &&
		!xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) && !xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) ) {
		if ( !xrtHttpCallResume(pCall) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				xrtGetError() != NULL ?
					xrtErrorKind(xrtGetError()) :
					XERR_STATE,
				"HTTP SSE transport could not resume",
				xrtGetError()
			);
			(void)xrtHttpCallCancel(pCall);
		}
	}
	xrtHttpCallDestroy(pCall);
	xrtHttpSseClientDestroy(pClient);
}



/* 在当前 HTTP Worker 上关闭 Parser 与传输输入门。 */
XHTTP_CLIENT_API bool xrtHttpSseClientPause(
	xhttpsseclient* pClient
)
{
	xhttpcall* pCall = NULL;
	bool bResult;

	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	xrtSpinLock(&pClient->Lock);
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) && (pClient->Call != NULL) ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( pCall == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client has no active call to pause",
			NULL
		);
		return false;
	}
	bResult = xrtHttpCallPause(pCall);
	if ( bResult ) {
		xrtAtomic32Store(
			&pClient->PauseGate,
			1,
			XMEMORY_RELEASE
		);
	}
	xrtHttpCallDestroy(pCall);
	return bResult;
}



/* 合并一次跨线程恢复，并投递到当前 Call 的稳定 Worker。 */
XHTTP_CLIENT_API bool xrtHttpSseClientResume(
	xhttpsseclient* pClient
)
{
	xnetworker* pWorker = NULL;
	uint32 iExpected = 0;
	bool bPosted;

	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	xrtSpinLock(&pClient->Lock);
	if ( !xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) && xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_RELAXED
	) && (pClient->Call != NULL) &&
		xrtAtomic32CompareExchange(
		&pClient->ResumeGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_RELAXED
	) ) {
		pWorker = xrtHttpCallWorker(pClient->Call);
		if ( (pWorker == NULL) ||
			(xrtHttpSseClientRef(pClient) == NULL) ) {
			pWorker = NULL;
			xrtAtomic32Store(
				&pClient->ResumeGate,
				0,
				XMEMORY_RELEASE
			);
		}
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( pWorker == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client is not paused or resume is already pending",
			NULL
		);
		return false;
	}
	bPosted = xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		__xrtHttpSseClientResumeTask,
		pClient
	);
	if ( !bPosted ) {
		xrtAtomic32Store(
			&pClient->ResumeGate,
			0,
			XMEMORY_RELEASE
		);
		xrtHttpSseClientDestroy(pClient);
	}
	return bPosted;
}



/* 删除 GET EventSource 请求不应携带的旧正文分帧字段。 */
static void __xrtHttpSseClientRequestClean(
	xhttprequest* pRequest
)
{
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Content-Length")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Transfer-Encoding")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Trailer")
	);
	(void)xrtHttpRequestRemoveHeader(
		pRequest, XRT_STR_LITERAL("Expect")
	);
}



/* 为一次连接尝试克隆请求并应用持久 Last-Event-ID。 */
static xhttprequest* __xrtHttpSseClientAttemptRequest(
	xhttpsseclient* pClient
)
{
	xhttprequest* pRequest = xrtHttpRequestClone(
		pClient->Request
	);

	if ( pRequest == NULL ) {
		return NULL;
	}
	__xrtHttpSseClientRequestClean(pRequest);
	if ( !xrtHttpSseRequestHeaders(
		xrtHttpRequestHeaders(pRequest),
		xrtHttpSseParserLastEventId(&pClient->Parser)
	) ) {
		xrtHttpRequestDestroy(pRequest);
		return NULL;
	}
	return pRequest;
}



/* HTTP Call 和重连 Timer 回调在后续生命周期实现中定义。 */
static void __xrtHttpSseClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
);

static void __xrtHttpSseClientTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
);



/* 启动一条新的 HTTP Call；同步失败不留下半活动句柄。 */
static bool __xrtHttpSseClientStart(
	xhttpsseclient* pClient
)
{
	xhttprequest* pRequest;
	xhttpcalloptions Options;
	xhttpcall* pCall;

	xrtHttpSseParserReconnect(&pClient->Parser);
	xrtBufferClear(&pClient->Pending);
	(void)xrtBufferTrim(&pClient->Pending);
	xrtErrorFree(pClient->AttemptError);
	pClient->AttemptError = NULL;
	pClient->Attempt = XRT_HTTP_SSE_ATTEMPT_WAITING;
	pClient->AttemptReason = XHTTP_SSE_CLOSE_HTTP;
	xrtAtomic32Store(
		&pClient->PauseGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pClient->ResumeGate,
		0,
		XMEMORY_RELEASE
	);
	pRequest = __xrtHttpSseClientAttemptRequest(pClient);
	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE reconnect request could not be prepared",
			xrtGetError()
		);
		return false;
	}
	Options = pClient->Config.Http;
	Options.Events.Headers = __xrtHttpSseClientHeaders;
	Options.Events.Body = __xrtHttpSseClientBody;
	Options.Events.Data = pClient;
	xrtSpinLock(&pClient->Lock);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) || (pClient->Call != NULL) ) {
		xrtSpinUnlock(&pClient->Lock);
		xrtHttpRequestDestroy(pRequest);
		return false;
	}
	pCall = xrtHttpClientDo(
		pClient->Http,
		pRequest,
		&Options,
		__xrtHttpSseClientDone,
		pClient
	);
	if ( pCall != NULL ) {
		pClient->Call = pCall;
		pClient->Worker = xrtHttpCallWorker(pCall);
		pClient->Engine = xrtNetWorkerEngine(
			pClient->Worker
		);
	}
	xrtSpinUnlock(&pClient->Lock);
	xrtHttpRequestDestroy(pRequest);
	return pCall != NULL;
}



/* 判断 HTTP 失败是否属于 EventSource 可以重连的暂态网络失败。 */
static bool __xrtHttpSseClientRetryable(
	const xhttpcallresult* pResult
)
{
	if ( (pResult->Result == XNET_RESULT_TIMEOUT) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_DIAL) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_POOL) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_PROXY) ||
		(pResult->Info.Error == XHTTP_CLIENT_ERROR_TRANSPORT) ||
		(pResult->Info.Error ==
		 XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL) ||
		(pResult->Info.Error ==
		 XHTTP_CLIENT_ERROR_TIMEOUT_IDLE) ) {
		return true;
	}
	return false;
}



/* 重连前保留 HTTP 重定向后的最终请求，避免反复访问旧入口。 */
static bool __xrtHttpSseClientRememberRequest(
	xhttpsseclient* pClient,
	xhttpcall* pCall,
	size_t iRedirects
)
{
	xhttprequest* pRequest;

	if ( iRedirects == 0 ) {
		return true;
	}
	pRequest = xrtHttpCallRequestClone(pCall);
	if ( pRequest == NULL ) {
		return __xrtHttpSseClientAttemptFail(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE effective request could not be retained",
			xrtGetError()
		);
	}
	xrtHttpRequestDestroy(pClient->Request);
	pClient->Request = pRequest;
	return true;
}



/* 把服务端毫秒重连值裁剪到本地策略并安全转换为毫秒。 */
static uint64 __xrtHttpSseClientDelay(
	const xhttpsseclient* pClient
)
{
	uint64 iDelay = xrtHttpSseParserRetry(
		&pClient->Parser
	);

	if ( iDelay < pClient->Config.RetryMin ) {
		iDelay = pClient->Config.RetryMin;
	}
	if ( iDelay > pClient->Config.RetryMax ) {
		iDelay = pClient->Config.RetryMax;
	}
	return iDelay > (uint64)INT64_MAX ? (uint64)INT64_MAX : iDelay;
}



/* 在当前 Call Worker 上安排唯一一次重连 Timer。 */
static bool __xrtHttpSseClientReconnect(
	xhttpsseclient* pClient,
	const xerror* pError
)
{
	xhttpsseclient* pTimerHold;
	uint64 iReconnect = xrtAtomic64Load(
		&pClient->Reconnects,
		XMEMORY_ACQUIRE
	);
	uint64 iDelay;
	uint64 Id;

	if ( iReconnect >= (uint64)pClient->Config.MaxReconnects ) {
		xerror* pLimit = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_RANGE,
			"HTTP SSE reconnect limit was reached",
			pError
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_RECONNECT_LIMIT,
			pLimit
		);
		xrtErrorFree(pLimit);
		return false;
	}
	iDelay = __xrtHttpSseClientDelay(pClient);
	pTimerHold = xrtHttpSseClientRef(pClient);
	if ( pTimerHold == NULL ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			xrtGetError()
		);
		return false;
	}
	xrtSpinLock(&pClient->Lock);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_RELAXED
	) || xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_RELAXED
	) || (pClient->Engine == NULL) ||
		(pClient->Worker == NULL) ||
		(pClient->Timer != 0) ) {
		Id = 0;
	} else {
		xrtAtomic32Store(
			&pClient->State,
			XHTTP_SSE_CLIENT_CONNECTING,
			XMEMORY_RELEASE
		);
		Id = xrtNetEngineAfter(
			pClient->Engine,
			xrtNetWorkerIndex(pClient->Worker),
			iDelay,
			__xrtHttpSseClientTimer,
			pTimerHold
		);
	}
	if ( Id != 0 ) {
		pClient->Timer = Id;
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( Id == 0 ) {
		xrtHttpSseClientDestroy(pTimerHold);
		if ( xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) || xrtAtomic32Load(
			&pClient->FinishGate,
			XMEMORY_ACQUIRE
		) ) {
			return false;
		}
		xerror* pTimer = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_IO,
			"HTTP SSE reconnect timer could not be scheduled",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pTimer
		);
		xrtErrorFree(pTimer);
		return false;
	}
	iReconnect = xrtAtomic64FetchAdd(
		&pClient->Reconnects,
		1,
		XMEMORY_ACQ_REL
	) + 1u;
	if ( pClient->Events.Retrying != NULL ) {
		pClient->Events.Retrying(
			pClient,
			(size_t)iReconnect,
			iDelay,
			pError,
			pClient->Events.Data
		);
	}
	return true;
}



/* HTTP Call 终态决定永久关闭、正常重连或暂态网络重连。 */
static void __xrtHttpSseClientDone(
	xhttpcall* pCall,
	const xhttpcallresult* pResult,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseclient* pHold = xrtHttpSseClientRef(pClient);
	xhttpsseclosereason Requested;
	bool bClosed;

	if ( pHold == NULL ) {
		return;
	}
	xrtSpinLock(&pClient->Lock);
	if ( pClient->Call == pCall ) {
		pClient->Call = NULL;
	}
	xrtSpinUnlock(&pClient->Lock);
	bClosed = xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) != 0;
	Requested = (xhttpsseclosereason)xrtAtomic32Load(
		&pClient->RequestedReason,
		XMEMORY_ACQUIRE
	);
	if ( bClosed ) {
		__xrtHttpSseClientFinish(
			pClient,
			Requested,
			Requested == XHTTP_SSE_CLOSE_CANCELLED ?
				pResult->Error : NULL
		);
	} else if ( pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_FAILED ) {
		__xrtHttpSseClientFinish(
			pClient,
			pClient->AttemptReason,
			pClient->AttemptError
		);
	} else if ( pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_STOP ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_STOP,
			NULL
		);
	} else if ( (pClient->Attempt ==
		XRT_HTTP_SSE_ATTEMPT_OPEN) &&
		(pResult->Result == XNET_RESULT_OK) ) {
		size_t iConsumed = 0;

		if ( xrtAtomic32Load(
			&pClient->PauseGate,
			XMEMORY_ACQUIRE
		) || (pClient->Pending.Size != 0) ) {
			(void)__xrtHttpSseClientAttemptFail(
				pClient,
				XHTTP_SSE_CLOSE_INTERNAL,
				XHTTP_SSE_CLIENT_ERROR_INTERNAL,
				XERR_INTERNAL,
				"HTTP SSE call completed with a paused input tail",
				NULL
			);
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		} else if ( !__xrtHttpSseClientRead(
			pClient,
			(xbytesview){ NULL, 0 },
			true,
			&iConsumed
		) ) {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		} else if ( __xrtHttpSseClientRememberRequest(
			pClient, pCall, pResult->Info.Redirects
		) ) {
			(void)__xrtHttpSseClientReconnect(
				pClient, NULL
			);
		} else {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		}
	} else if ( __xrtHttpSseClientRetryable(pResult) ) {
		if ( __xrtHttpSseClientRememberRequest(
			pClient, pCall, pResult->Info.Redirects
		) ) {
			(void)__xrtHttpSseClientReconnect(
				pClient, pResult->Error
			);
		} else {
			__xrtHttpSseClientFinish(
				pClient,
				pClient->AttemptReason,
				pClient->AttemptError
			);
		}
	} else if ( pResult->Result == XNET_RESULT_CANCELLED ) {
		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_CANCELLED,
			pResult->Error
		);
	} else {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_HTTP,
			pResult->Error != NULL ?
				xrtErrorKind(pResult->Error) : XERR_IO,
			"HTTP SSE connection failed permanently",
			pResult->Error
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_HTTP,
			pError
		);
		xrtErrorFree(pError);
	}
	xrtHttpResponseDestroy(pResult->Response);
	xrtHttpCallDestroy(pCall);
	xrtHttpSseClientDestroy(pHold);
}



/* Timer 到期后在同一 Engine 上开始下一次独立 HTTP Call。 */
static void __xrtHttpSseClientTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xhttpsseclosereason Requested;

	(void)pWorker;
	xrtSpinLock(&pClient->Lock);
	if ( pClient->Timer == Id ) {
		pClient->Timer = 0;
	}
	xrtSpinUnlock(&pClient->Lock);
	Requested = (xhttpsseclosereason)xrtAtomic32Load(
		&pClient->RequestedReason,
		XMEMORY_ACQUIRE
	);
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) || (Result == XNET_RESULT_CANCELLED) ) {
		__xrtHttpSseClientFinish(
			pClient,
			Requested,
			NULL
		);
		goto Complete;
	}
	if ( Result != XNET_RESULT_OK ) {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			XERR_IO,
			"HTTP SSE reconnect timer failed",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pError
		);
		xrtErrorFree(pError);
		goto Complete;
	}
	if ( !__xrtHttpSseClientStart(pClient) ) {
		xerror* pError = __xrtHttpSseClientErrorCreate(
			XHTTP_SSE_CLIENT_ERROR_RECONNECT,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_MEMORY,
			"HTTP SSE reconnect call could not be submitted",
			xrtGetError()
		);

		__xrtHttpSseClientFinish(
			pClient,
			XHTTP_SSE_CLOSE_INTERNAL,
			pError
		);
		xrtErrorFree(pError);
	}

Complete:
	xrtHttpSseClientDestroy(pClient);
}



/* 在 Timer 所属 Worker 上无分配地撤销重连并释放命令持有的引用。 */
static void __xrtHttpSseClientCloseTimerTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpsseclient* pClient = (xhttpsseclient*)pData;
	xnetengine* pEngine = NULL;
	uint64 Id = 0;

	xrtSpinLock(&pClient->Lock);
	if ( pClient->Worker == pWorker ) {
		pEngine = pClient->Engine;
		Id = pClient->Timer;
	}
	xrtSpinUnlock(&pClient->Lock);
	if ( (Id != 0) && (pEngine != NULL) ) {
		(void)xrtNetEngineTimerCancelCurrent(
			pEngine, Id
		);
	}
	if ( !xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpSseClientFinish(
			pClient,
			(xhttpsseclosereason)xrtAtomic32Load(
				&pClient->RequestedReason,
				XMEMORY_ACQUIRE
			),
			NULL
		);
	}
	xrtHttpSseClientDestroy(pClient);
}



/* 关闭当前 Call 或 Timer，并让其所属 Worker 发布终态。 */
static bool __xrtHttpSseClientCloseReason(
	xhttpsseclient* pClient,
	xhttpsseclosereason Reason
)
{
	xhttpcall* pCall = NULL;
	xnetworker* pWorker = NULL;
	uint64 Id = 0;
	uint32 iExpected = 0;
	bool bConstructing;

	if ( !xrtAtomic32CompareExchange(
		&pClient->CloseGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pClient->RequestedReason,
		(uint32)Reason,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CLOSED,
		XMEMORY_RELEASE
	);
	xrtSpinLock(&pClient->Lock);
	if ( pClient->Call != NULL ) {
		pCall = xrtHttpCallRef(pClient->Call);
	}
	Id = pClient->Timer;
	pWorker = pClient->Worker;
	bConstructing = pClient->Constructing;
	xrtSpinUnlock(&pClient->Lock);
	if ( pCall != NULL ) {
		(void)xrtHttpCallCancel(pCall);
		xrtHttpCallDestroy(pCall);
	} else if ( (Id != 0) && (pWorker != NULL) ) {
		xhttpsseclient* pPostHold =
			xrtHttpSseClientRef(pClient);

		if ( pPostHold != NULL ) {
			if ( !xrtNetPost(
				pWorker,
				&pClient->ClosePost,
				__xrtHttpSseClientCloseTimerTask,
				pClient
			) ) {
				xrtHttpSseClientDestroy(pPostHold);
				xrtClearError();
				if ( !xrtNetEngineTimerCancel(
					xrtNetWorkerEngine(pWorker), Id
				) ) {
					xrtClearError();
				}
			}
		} else if ( !xrtNetEngineTimerCancel(
			xrtNetWorkerEngine(pWorker), Id
		) ) {
			xrtClearError();
		}
	} else if ( !bConstructing ) {
		__xrtHttpSseClientFinish(pClient, Reason, NULL);
	}
	return true;
}



/* 外部取消令牌把整个 EventSource 会话永久关闭。 */
static void __xrtHttpSseClientCancel(ptr pData)
{
	(void)__xrtHttpSseClientCloseReason(
		(xhttpsseclient*)pData,
		XHTTP_SSE_CLOSE_CANCELLED
	);
}



/* 从任意线程主动关闭整个订阅会话。 */
XHTTP_CLIENT_API bool xrtHttpSseClientClose(
	xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpSseClientCloseReason(
		pClient,
		XHTTP_SSE_CLOSE_USER
	) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client is already closing or closed",
			NULL
		);
		return false;
	}
	return true;
}



/* 构造失败时保留当前线程错误，并回收用户与运行时两份初始引用。 */
static xhttpsseclient* __xrtHttpSseClientCreateFail(
	xhttpsseclient* pClient,
	xhttpsseclienterror Code,
	xerrkind Kind,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();

	if ( pClient != NULL ) {
		xrtSpinLock(&pClient->Lock);
		pClient->Constructing = false;
		pClient->RuntimeHeld = false;
		xrtSpinUnlock(&pClient->Lock);
		xrtHttpSseClientDestroy(pClient);
		xrtHttpSseClientDestroy(pClient);
	}
	__xrtHttpSseClientSetError(
		Code,
		Kind,
		sMessage,
		pCause
	);
	xrtErrorFree(pCause);
	return NULL;
}



/* 创建拥有请求模板、Parser 和自动重连运行时引用的 SSE 会话。 */
static xhttpsseclient* __xrtHttpSseClientCreate(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseclient* pClient;

	if ( pHttp == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP client is null",
			NULL
		);
		return NULL;
	}
	if ( !__xrtHttpSseClientInputsResolve(
		pConfig, pEvents, &Config, &Events
	) || !__xrtHttpSseClientRequestValid(pRequest) ) {
		return NULL;
	}
	pClient = (xhttpsseclient*)xrtMalloc(
		sizeof(*pClient)
	);
	if ( pClient == NULL ) {
		return __xrtHttpSseClientCreateFail(
			NULL,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_MEMORY,
			"HTTP SSE client could not be allocated"
		);
	}
	memset(pClient, 0, sizeof(*pClient));
	pClient->References = 2;
	pClient->RuntimeHeld = true;
	pClient->Constructing = true;
	pClient->AttemptReason = XHTTP_SSE_CLOSE_HTTP;
	pClient->Events = Events;
	xrtSpinInit(&pClient->Lock);
	if ( !xrtNetPostInit(&pClient->ClosePost) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_STATE,
			"HTTP SSE close post could not be initialized"
		);
	}
	xrtAtomic32Store(
		&pClient->State,
		XHTTP_SSE_CLIENT_CONNECTING,
		XMEMORY_RELAXED
	);
	if ( !xrtBufferInit(&pClient->Pending) ||
		!xrtHttpSseParserInit(
			&pClient->Parser,
			&Config.Parser
		) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_INTERNAL,
			XERR_MEMORY,
			"HTTP SSE client state could not be initialized"
		);
	}
	xrtAtomic64Store(
		&pClient->Retry,
		xrtHttpSseParserRetry(&pClient->Parser),
		XMEMORY_RELAXED
	);
	pClient->Http = xrtHttpClientRef(pHttp);
	if ( pClient->Http == NULL ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_STATE,
			"HTTP client could not be retained"
		);
	}
	pClient->Request = xrtHttpRequestClone(pRequest);
	if ( pClient->Request == NULL ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			XERR_MEMORY,
			"HTTP SSE request could not be retained"
		);
	}
	if ( !__xrtHttpSseClientOptionsInit(
		pClient, &Config
	) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_CONFIG,
			XERR_MEMORY,
			"HTTP SSE call options could not be retained"
		);
	}
	if ( pClient->Cancel != NULL ) {
		pClient->CancelWatch = xrtCancelWatch(
			pClient->Cancel,
			__xrtHttpSseClientCancel,
			pClient
		);
		if ( pClient->CancelWatch == NULL ) {
			return __xrtHttpSseClientCreateFail(
				pClient,
				XHTTP_SSE_CLIENT_ERROR_CONFIG,
				XERR_MEMORY,
				"HTTP SSE cancellation watch could not be installed"
			);
		}
	}
	if ( xrtAtomic32Load(
		&pClient->CloseGate,
		XMEMORY_ACQUIRE
	) ) {
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_CANCELLED,
			XERR_CANCELLED,
			"HTTP SSE connection was cancelled before submission"
		);
	}
	if ( !__xrtHttpSseClientStart(pClient) ) {
		if ( xrtAtomic32Load(
			&pClient->CloseGate,
			XMEMORY_ACQUIRE
		) ) {
			return __xrtHttpSseClientCreateFail(
				pClient,
				XHTTP_SSE_CLIENT_ERROR_CANCELLED,
				XERR_CANCELLED,
				"HTTP SSE connection was cancelled during submission"
			);
		}
		return __xrtHttpSseClientCreateFail(
			pClient,
			XHTTP_SSE_CLIENT_ERROR_HTTP,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_IO,
			"HTTP SSE connection could not be submitted"
		);
	}
	xrtSpinLock(&pClient->Lock);
	pClient->Constructing = false;
	xrtSpinUnlock(&pClient->Lock);
	return pClient;
}



/* 使用 GET URL 建立自动重连的 EventSource 会话。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseConnect(
	xhttpclient* pHttp,
	xstrview Url,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	xhttprequest* pRequest = xrtHttpRequestCreate(
		XRT_STR_LITERAL("GET"),
		Url
	);
	xhttpsseclient* pClient;

	if ( pRequest == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_REQUEST,
			xrtGetError() != NULL ?
				xrtErrorKind(xrtGetError()) : XERR_VALUE,
			"HTTP SSE URL is invalid",
			xrtGetError()
		);
		return NULL;
	}
	pClient = __xrtHttpSseClientCreate(
		pHttp,
		pRequest,
		pConfig,
		pEvents
	);
	xrtHttpRequestDestroy(pRequest);
	return pClient;
}



/* 克隆 GET 请求模板，并由会话管理 SSE 专用 Header 与重连状态。 */
XHTTP_CLIENT_API xhttpsseclient* xrtHttpSseConnectRequest(
	xhttpclient* pHttp,
	const xhttprequest* pRequest,
	const xhttpsseclientconfig* pConfig,
	const xhttpsseclientevents* pEvents
)
{
	return __xrtHttpSseClientCreate(
		pHttp,
		pRequest,
		pConfig,
		pEvents
	);
}



/* 返回 EventSource 三态的并发快照。 */
XHTTP_CLIENT_API xhttpsseclientstate xrtHttpSseClientState(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return XHTTP_SSE_CLIENT_CLOSED;
	}
	return (xhttpsseclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
}



/* 返回当前 Parser 与底层 HTTP 输入是否处于暂停状态。 */
XHTTP_CLIENT_API bool xrtHttpSseClientPaused(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return false;
	}
	return xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
}



/* 复制不暴露 Parser 借用内存的会话统计快照。 */
XHTTP_CLIENT_API bool xrtHttpSseClientInfo(
	const xhttpsseclient* pClient,
	xhttpsseclientinfo* pInfo
)
{
	xhttpsseclientinfo Info;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pInfo, sizeof(*pInfo)) ||
		xrtMemRangesOverlap(
			pClient, sizeof(*pClient), pInfo, sizeof(*pInfo)
		) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client or info output range is invalid",
			NULL
		);
		return false;
	}
	memset(&Info, 0, sizeof(Info));
	Info.State = (xhttpsseclientstate)xrtAtomic32Load(
		&pClient->State,
		XMEMORY_ACQUIRE
	);
	Info.Status = (uint16)xrtAtomic32Load(
		&pClient->Status,
		XMEMORY_ACQUIRE
	);
	Info.Retry = xrtAtomic64Load(
		&pClient->Retry,
		XMEMORY_ACQUIRE
	);
	Info.Messages = xrtAtomic64Load(
		&pClient->Messages,
		XMEMORY_ACQUIRE
	);
	Info.Comments = xrtAtomic64Load(
		&pClient->Comments,
		XMEMORY_ACQUIRE
	);
	Info.RetryUpdates = xrtAtomic64Load(
		&pClient->RetryUpdates,
		XMEMORY_ACQUIRE
	);
	Info.Reconnects = (size_t)xrtAtomic64Load(
		&pClient->Reconnects,
		XMEMORY_ACQUIRE
	);
	Info.Paused = xrtAtomic32Load(
		&pClient->PauseGate,
		XMEMORY_ACQUIRE
	) != 0;
	memcpy(pInfo, &Info, sizeof(Info));
	return true;
}



/* CLOSED 后返回由会话拥有的稳定终态错误。 */
XHTTP_CLIENT_API const xerror* xrtHttpSseClientError(
	const xhttpsseclient* pClient
)
{
	if ( pClient == NULL ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_ARGUMENT,
			XERR_ARGUMENT,
			"HTTP SSE client is null",
			NULL
		);
		return NULL;
	}
	if ( !xrtAtomic32Load(
		&pClient->FinishGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpSseClientSetError(
			XHTTP_SSE_CLIENT_ERROR_STATE,
			XERR_STATE,
			"HTTP SSE client terminal state is not published",
			NULL
		);
		return NULL;
	}
	return pClient->Error;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-client/src/http/http_client_stream_async.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)



#if defined(XHTTP_FEATURE_HTTP_CLIENT_STREAM_ASYNC)

/* Future 通知释放后在所属 Worker 上消费终态。 */
static void __xrtHttp1CallAsyncReadyTask(
	xnetworker* pWorker,
	ptr pData
);



/* 用调用对象内嵌 Post 无分配地提交 Future 完成命令。 */
static bool __xrtHttp1CallAsyncPost(xhttp1call* pCall)
{
	if ( xrtNetPost(
		pCall->Worker,
		&pCall->OutputPost,
		__xrtHttp1CallAsyncReadyTask,
		pCall
	) ) {
		return true;
	}
	xrtHttp1CallDestroy(pCall);
	return false;
}



/* 释放 waiter 引用，或在完成通知退出后把它转移给 Worker 命令。 */
static void __xrtHttp1CallAsyncRelease(ptr pData)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	if ( pCall->OutputNotified ) {
		pCall->OutputNotified = false;
		(void)__xrtHttp1CallAsyncPost(pCall);
		return;
	}
	xrtHttp1CallDestroy(pCall);
}



/* 把非成功 Future 终态转换为可保留的结构化原因。 */
static xerror* __xrtHttp1CallAsyncError(
	const xfutureresult* pResult
)
{
	if ( pResult->State == XFUTURE_FAILED ) {
		return xrtErrorRef(pResult->Error);
	}
	if ( pResult->State == XFUTURE_CANCELLED ) {
		return xrtErrorCreate(
			XERR_CANCELLED,
			"xrt.future",
			(int32)XFUTURE_CANCELLED,
			"HTTP request body readiness wait was cancelled"
		);
	}
	return xrtErrorCreate(
		XERR_CLOSED,
		"xrt.future",
		(int32)pResult->State,
		"HTTP request body readiness Future closed without a value"
	);
}



/* 在所属 Worker 上消费 Future 终态并恢复或结束调用。 */
static void __xrtHttp1CallAsyncReady(xhttp1call* pCall)
{
	xfutureresult Result;
	xfuture* pFuture = pCall->OutputFuture;
	xerror* pCause = NULL;

	if ( (pFuture == NULL) || !pCall->OutputWaiting ) {
		return;
	}
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	if ( !xrtFutureResult(pFuture, &Result) ) {
		pCause = xrtErrorRef(xrtGetError());
	} else if ( Result.State != XFUTURE_RESOLVED ) {
		pCause = __xrtHttp1CallAsyncError(&Result);
	}
	xrtFutureDestroy(pFuture);
	if ( pCause != NULL ) {
		__xrtHttp1CallFail(
			pCall,
			XHTTP1_CALL_ERROR_EXCHANGE,
			xrtErrorKind(pCause),
			"wait-http-request-body",
			"HTTP request body readiness wait did not succeed",
			pCause
		);
		xrtErrorFree(pCause);
		return;
	}
	__xrtHttp1CallDrive(pCall);
}



/* Engine 投递回到所属 Worker 后消费 Future 终态。 */
static void __xrtHttp1CallAsyncReadyTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	(void)pWorker;
	__xrtHttp1CallAsyncReady(pCall);
	xrtHttp1CallDestroy(pCall);
}



/* Future 完成回调只登记通知；Release 在 Calling 清除后提交 Worker 命令。 */
static void __xrtHttp1CallAsyncNotify(ptr pData)
{
	xhttp1call* pCall = (xhttp1call*)pData;

	pCall->OutputNotified = true;
}



/* 订阅一次正文可读性 Future，并覆盖立即完成竞态。 */
bool __xrtHttp1CallAsyncWait(xhttp1call* pCall)
{
	xfuture* pFuture;
	xfuturewatchresult WatchResult;

	if ( (pCall == NULL) || pCall->OutputWaiting ||
		(pCall->OutputFuture != NULL) ) {
		xrtSetErrorInfo(
			XERR_STATE,
			"xrt.http.call",
			XHTTP1_CALL_ERROR_STATE,
			"HTTP/1 call already has a pending output wait"
		);
		return false;
	}
	pFuture = xrtHttp1ExchangeOutputWait(pCall->Exchange);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !xrtFutureWatchInit(
		&pCall->OutputWatch,
		__xrtHttp1CallAsyncNotify,
		__xrtHttp1CallAsyncRelease,
		pCall
	) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pCall->OutputNotified = false;
	pCall->OutputFuture = pFuture;
	pCall->OutputWaiting = true;
	if ( xrtHttp1CallRef(pCall) == NULL ) {
		pCall->OutputFuture = NULL;
		pCall->OutputWaiting = false;
		xrtFutureDestroy(pFuture);
		return false;
	}
	WatchResult = xrtFutureWatchAdd(
		pFuture,
		&pCall->OutputWatch
	);
	if ( WatchResult == XFUTURE_WATCH_PENDING ) {
		return true;
	}
	if ( WatchResult == XFUTURE_WATCH_READY ) {
		if ( __xrtHttp1CallAsyncPost(pCall) ) {
			return true;
		}
		pCall->OutputFuture = NULL;
		pCall->OutputWaiting = false;
		xrtFutureDestroy(pFuture);
		return false;
	}
	xrtHttp1CallDestroy(pCall);
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	xrtFutureDestroy(pFuture);
	return false;
}



/* 停止调用时请求取消来源，并等待并发通知离开内嵌节点。 */
void __xrtHttp1CallAsyncStop(xhttp1call* pCall)
{
	xfuture* pFuture;

	if ( pCall == NULL ) {
		return;
	}
	pFuture = pCall->OutputFuture;
	if ( pFuture == NULL ) {
		pCall->OutputWaiting = false;
		return;
	}
	pCall->OutputFuture = NULL;
	pCall->OutputWaiting = false;
	(void)xrtFutureCancel(pFuture);
	xrtFutureWatchRemove(
		pFuture,
		&pCall->OutputWatch
	);
	xrtFutureDestroy(pFuture);
}

#endif
#endif

#endif
