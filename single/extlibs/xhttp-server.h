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
#error "xhttp_server requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XHTTP_SERVER_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XHTTP_SERVER_IMPLEMENTATION) && \
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
#ifndef XHTTP_SERVER_SINGLE_HEADER_H
#define XHTTP_SERVER_SINGLE_HEADER_H
#define XHTTP_SERVER_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xhttp-server/include/xhttp-server/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XHTTP_SERVER_FEATURES_H
#define XHTTP_SERVER_FEATURES_H

/* http_server_router_tls 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER_TLS)
#ifndef XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS
#define XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#define XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#define XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#endif
#endif

/* http_server_mux_tls 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_MUX_TLS)
#ifndef XHTTP_FEATURE_HTTP_SERVER_MUX_TLS
#define XHTTP_FEATURE_HTTP_SERVER_MUX_TLS
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_MUX
#define XHTTP_SERVER_MODULE_HTTP_SERVER_MUX
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#define XHTTP_SERVER_MODULE_HTTP_SERVER_TLS
#endif
#endif

/* http_server_mux 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_MUX)
#ifndef XHTTP_FEATURE_HTTP_SERVER_MUX
#define XHTTP_FEATURE_HTTP_SERVER_MUX
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#define XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#endif
#ifndef XRT_MODULE_RWLOCK
#define XRT_MODULE_RWLOCK
#endif
#endif

/* http_server_middleware 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_MIDDLEWARE)
#ifndef XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE
#define XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#define XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER
#endif
#endif

/* http_server_router 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_ROUTER)
#ifndef XHTTP_FEATURE_HTTP_SERVER_ROUTER
#define XHTTP_FEATURE_HTTP_SERVER_ROUTER
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_ROUTER
#define XHTTP_SERVER_MODULE_HTTP_ROUTER
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#endif

/* http_server_compress 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_COMPRESS)
#ifndef XHTTP_FEATURE_HTTP_SERVER_COMPRESS
#define XHTTP_FEATURE_HTTP_SERVER_COMPRESS
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_REPLY_COMPRESS
#define XHTTP_SERVER_MODULE_HTTP_REPLY_COMPRESS
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_REQUEST
#define XHTTP_MODULE_HTTP_SERVER_REQUEST
#endif
#endif

/* http_reply_compress 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_REPLY_COMPRESS)
#ifndef XHTTP_FEATURE_HTTP_REPLY_COMPRESS
#define XHTTP_FEATURE_HTTP_REPLY_COMPRESS
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_REPLY
#define XHTTP_MODULE_HTTP_SERVER_REPLY
#endif
#ifndef XRT_MODULE_HTTP_ENCODING
#define XRT_MODULE_HTTP_ENCODING
#endif
#ifndef XHTTP_MODULE_HTTP_VARY
#define XHTTP_MODULE_HTTP_VARY
#endif
#ifndef XHTTP_MODULE_HTTP_CACHE
#define XHTTP_MODULE_HTTP_CACHE
#endif
#ifndef XHTTP_MODULE_HTTP_BODY_DEFLATE
#define XHTTP_MODULE_HTTP_BODY_DEFLATE
#endif
#ifndef XHTTP_MODULE_MIME
#define XHTTP_MODULE_MIME
#endif
#ifndef XHTTP_MODULE_HTTP_ETAG
#define XHTTP_MODULE_HTTP_ETAG
#endif
#endif

/* http_router 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_ROUTER)
#ifndef XHTTP_FEATURE_HTTP_ROUTER
#define XHTTP_FEATURE_HTTP_ROUTER
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_ROUTE
#define XHTTP_SERVER_MODULE_HTTP_ROUTE
#endif
#endif

/* http_route 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_ROUTE)
#ifndef XHTTP_FEATURE_HTTP_ROUTE
#define XHTTP_FEATURE_HTTP_ROUTE
#endif
#ifndef XRT_MODULE_HTTP
#define XRT_MODULE_HTTP
#endif
#endif

/* http_server_static 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_STATIC)
#ifndef XHTTP_FEATURE_HTTP_SERVER_STATIC
#define XHTTP_FEATURE_HTTP_SERVER_STATIC
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_FUTURE
#define XHTTP_SERVER_MODULE_HTTP_SERVER_FUTURE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_BODY_ASYNC
#define XHTTP_SERVER_MODULE_HTTP_SERVER_BODY_ASYNC
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_PATH
#define XHTTP_SERVER_MODULE_HTTP_STATIC_PATH
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_FILE
#define XHTTP_SERVER_MODULE_HTTP_STATIC_FILE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_PLAN
#define XHTTP_SERVER_MODULE_HTTP_STATIC_PLAN
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_RESPONSE
#define XHTTP_SERVER_MODULE_HTTP_STATIC_RESPONSE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_MULTIPART_BODY
#define XHTTP_SERVER_MODULE_HTTP_STATIC_MULTIPART_BODY
#endif
#ifndef XHTTP_MODULE_MIME_TYPES
#define XHTTP_MODULE_MIME_TYPES
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#endif

/* http_server_file 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_FILE)
#ifndef XHTTP_FEATURE_HTTP_SERVER_FILE
#define XHTTP_FEATURE_HTTP_SERVER_FILE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_FUTURE
#define XHTTP_SERVER_MODULE_HTTP_SERVER_FUTURE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#define XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER_BODY_ASYNC
#define XHTTP_SERVER_MODULE_HTTP_SERVER_BODY_ASYNC
#endif
#endif

/* http_server_future 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_FUTURE)
#ifndef XHTTP_FEATURE_HTTP_SERVER_FUTURE
#define XHTTP_FEATURE_HTTP_SERVER_FUTURE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#ifndef XRT_MODULE_FUTURE_CONTINUE
#define XRT_MODULE_FUTURE_CONTINUE
#endif
#endif

/* http_server_upgrade 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_UPGRADE)
#ifndef XHTTP_FEATURE_HTTP_SERVER_UPGRADE
#define XHTTP_FEATURE_HTTP_SERVER_UPGRADE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#endif

/* http_server_raw 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_RAW)
#ifndef XHTTP_FEATURE_HTTP_SERVER_RAW
#define XHTTP_FEATURE_HTTP_SERVER_RAW
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#endif

/* http_server_body_async 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_BODY_ASYNC)
#ifndef XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC
#define XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_RESPONSE_ASYNC
#define XHTTP_MODULE_HTTP_SERVER_RESPONSE_ASYNC
#endif
#endif

/* http_server_tls 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER_TLS)
#ifndef XHTTP_FEATURE_HTTP_SERVER_TLS
#define XHTTP_FEATURE_HTTP_SERVER_TLS
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_SERVER
#define XHTTP_SERVER_MODULE_HTTP_SERVER
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
#endif
#endif

/* http_server 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SERVER)
#ifndef XHTTP_FEATURE_HTTP_SERVER
#define XHTTP_FEATURE_HTTP_SERVER
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_EXCHANGE
#define XHTTP_MODULE_HTTP_SERVER_EXCHANGE
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_RESPONSE
#define XHTTP_MODULE_HTTP_SERVER_RESPONSE
#endif
#ifndef XRT_MODULE_NET_TCP_SERVER
#define XRT_MODULE_NET_TCP_SERVER
#endif
#ifndef XRT_MODULE_SPIN
#define XRT_MODULE_SPIN
#endif
#endif

/* http_sse_server 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_SSE_SERVER)
#ifndef XHTTP_FEATURE_HTTP_SSE_SERVER
#define XHTTP_FEATURE_HTTP_SSE_SERVER
#endif
#ifndef XHTTP_MODULE_HTTP_SSE_HTTP
#define XHTTP_MODULE_HTTP_SSE_HTTP
#endif
#ifndef XHTTP_MODULE_HTTP_BODY_STREAM
#define XHTTP_MODULE_HTTP_BODY_STREAM
#endif
#ifndef XHTTP_MODULE_HTTP_SERVER_REPLY
#define XHTTP_MODULE_HTTP_SERVER_REPLY
#endif
#endif

/* http_static_multipart_body 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_STATIC_MULTIPART_BODY)
#ifndef XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY
#define XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY
#endif
#ifndef XHTTP_MODULE_HTTP_RANGE_MULTIPART
#define XHTTP_MODULE_HTTP_RANGE_MULTIPART
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_FILE
#define XHTTP_SERVER_MODULE_HTTP_STATIC_FILE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#define XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#endif
#endif

/* http_static_file 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_STATIC_FILE)
#ifndef XHTTP_FEATURE_HTTP_STATIC_FILE
#define XHTTP_FEATURE_HTTP_STATIC_FILE
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_ROOT
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#define XHTTP_SERVER_MODULE_HTTP_BODY_FILE
#endif
#ifndef XHTTP_MODULE_HTTP_PRECONDITION
#define XHTTP_MODULE_HTTP_PRECONDITION
#endif
#endif

/* http_body_file 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_BODY_FILE)
#ifndef XHTTP_FEATURE_HTTP_BODY_FILE
#define XHTTP_FEATURE_HTTP_BODY_FILE
#endif
#ifndef XHTTP_MODULE_HTTP_BODY_ASYNC
#define XHTTP_MODULE_HTTP_BODY_ASYNC
#endif
#ifndef XRT_MODULE_FILE_ASYNC
#define XRT_MODULE_FILE_ASYNC
#endif
#endif

/* http_static_response 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_STATIC_RESPONSE)
#ifndef XHTTP_FEATURE_HTTP_STATIC_RESPONSE
#define XHTTP_FEATURE_HTTP_STATIC_RESPONSE
#endif
#ifndef XHTTP_SERVER_MODULE_HTTP_STATIC_PLAN
#define XHTTP_SERVER_MODULE_HTTP_STATIC_PLAN
#endif
#ifndef XHTTP_MODULE_HTTP_RANGE_MULTIPART
#define XHTTP_MODULE_HTTP_RANGE_MULTIPART
#endif
#endif

/* http_static_plan 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_STATIC_PLAN)
#ifndef XHTTP_FEATURE_HTTP_STATIC_PLAN
#define XHTTP_FEATURE_HTTP_STATIC_PLAN
#endif
#ifndef XHTTP_MODULE_HTTP_PRECONDITION
#define XHTTP_MODULE_HTTP_PRECONDITION
#endif
#ifndef XHTTP_MODULE_HTTP_RANGE
#define XHTTP_MODULE_HTTP_RANGE
#endif
#endif

/* http_static_path 及其直接依赖。 */
#if defined(XHTTP_SERVER_MODULE_ALL) || defined(XHTTP_SERVER_MODULE_HTTP_STATIC_PATH)
#ifndef XHTTP_FEATURE_HTTP_STATIC_PATH
#define XHTTP_FEATURE_HTTP_STATIC_PATH
#endif
#ifndef XRT_MODULE_CODEC_PERCENT
#define XRT_MODULE_CODEC_PERCENT
#endif
#ifndef XRT_MODULE_PATH_SAFE
#define XRT_MODULE_PATH_SAFE
#endif
#endif

#endif /* XHTTP_SERVER_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xhttp-server/export.h */
/* ========================================================================== */

#ifndef XHTTP_SERVER_EXPORT_H
#define XHTTP_SERVER_EXPORT_H


/* 独立动态库仅导出本库实现，依赖库由宿主共同链接。 */
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XHTTP_SERVER_BUILD_SHARED)
  #define XHTTP_SERVER_API __declspec(dllexport)
 #elif defined(XHTTP_SERVER_USE_SHARED)
  #define XHTTP_SERVER_API __declspec(dllimport)
 #else
  #define XHTTP_SERVER_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XHTTP_SERVER_BUILD_SHARED)
  #define XHTTP_SERVER_API __attribute__((visibility("default")))
 #else
  #define XHTTP_SERVER_API
 #endif
#else
 #define XHTTP_SERVER_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_body_file.h */
/* ========================================================================== */

#ifndef XRT_HTTP_BODY_FILE_H
#define XRT_HTTP_BODY_FILE_H





#if defined(XHTTP_FEATURE_HTTP_BODY_FILE) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_ASYNC) || \
	 !defined(XRT_FEATURE_FILE_ASYNC))
	#error "XRT HTTP file body support requires async HTTP body and async file support"
#endif



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 默认单次异步读取 64 KiB；这是按请求分配上限，不是每对象固定缓冲。 */
#define XHTTP_BODY_FILE_READ_DEFAULT 65536u



/* ReadSize 限制每次异步文件读取的字节数，必须大于零。 */
typedef struct xhttpbodyfileconfig {
	size_t ReadSize;
} xhttpbodyfileconfig;



/* 文件正文错误保留准备阶段或读取阶段的底层 cause。 */
typedef enum xhttpbodyfileerror {
	XHTTP_BODY_FILE_ERROR_SUBMIT = 1,
	XHTTP_BODY_FILE_ERROR_OPEN,
	XHTTP_BODY_FILE_ERROR_SIZE,
	XHTTP_BODY_FILE_ERROR_RANGE,
	XHTTP_BODY_FILE_ERROR_ADOPT,
	XHTTP_BODY_FILE_ERROR_CREATE,
	XHTTP_BODY_FILE_ERROR_READ
} xhttpbodyfileerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 初始化默认文件正文配置；输出结构可以未对齐。 */
XHTTP_SERVER_API void xrtHttpBodyFileConfigInit(
	xhttpbodyfileconfig* pConfig
);



/*
	创建采用异步文件的不可重放正文。
	成功后正文独占关闭 pFile；失败时调用方仍然拥有 pFile。
	配置在调用期间复制，可以未对齐；NULL 使用默认配置。
*/
XHTTP_SERVER_API xhttpbody* xrtHttpBodyFileAdopt(
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
);



/*
	在任务池中打开文件并创建完整文件正文。
	成功 Future 的值为借用 xhttpbody，Future 释放时销毁其正文引用。
*/
XHTTP_SERVER_API xfuture* xrtHttpBodyFileFuture(
	xtaskpool* pPool,
	cstr sPath,
	const xhttpbodyfileconfig* pConfig
);



/*
	在任务池中打开文件并创建严格字节区间正文。
	区间超出同一已打开文件的大小时，Future 以 RANGE 错误失败。
*/
XHTTP_SERVER_API xfuture* xrtHttpBodyFileRangeFuture(
	xtaskpool* pPool,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_static.h */
/* ========================================================================== */

#ifndef XRT_HTTP_STATIC_H
#define XRT_HTTP_STATIC_H



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE)
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) && \
	(!defined(XHTTP_FEATURE_HTTP_PRECONDITION) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE))
	#error "XRT HTTP static planning requires precondition and range support"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) && \
	(!defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART))
	#error "XRT HTTP static response support requires static planning and multipart"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) && \
	(!defined(XRT_FEATURE_CODEC_PERCENT) || \
	 !defined(XRT_FEATURE_PATH_SAFE))
	#error "XRT HTTP static path mapping requires percent codec and safe path support"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE) && \
	(!defined(XRT_FEATURE_ATOMIC) || \
	 !defined(XRT_FEATURE_FILE_ROOT) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_PRECONDITION))
	#error "XRT HTTP static files require atomic, file roots, file bodies and HTTP preconditions"
#endif

#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) && \
	(!defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_FILE))
	#error "XRT HTTP static multipart bodies require multipart, static files and file bodies"
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)

/* 默认使用可移植文件名；隐藏段必须由调用方明确放行。 */
typedef enum xhttpstaticpathflag {
	XHTTP_STATIC_PATH_PORTABLE = UINT32_C(0x00000001),
	XHTTP_STATIC_PATH_ALLOW_HIDDEN = UINT32_C(0x00000002)
} xhttpstaticpathflag;



/* 路径写入区分协议错误、正常路由未命中和成功映射。 */
typedef enum xhttpstaticpathstatus {
	XHTTP_STATIC_PATH_ERROR = -1,
	XHTTP_STATIC_PATH_NO_MATCH = 0,
	XHTTP_STATIC_PATH_MATCH = 1
} xhttpstaticpathstatus;



/*
	Mount 是已解码且规范化的 origin 路径，根挂载为斜杠。
	非根挂载不得以斜杠结尾，比较区分大小写并要求完整段边界。
*/
typedef struct xhttpstaticpathconfig {
	xstrview Mount;
	uint32 Flags;
} xhttpstaticpathconfig;



/* 高层映射结果拥有 Path；未命中时其余字段保持空值。 */
typedef struct xhttpstaticpath {
	str Path;
	size_t Size;
	bool Matched;
	bool TrailingSlash;
} xhttpstaticpath;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)

typedef struct xhttpstaticfile xhttpstaticfile;



/* 静态文件错误保留受限文件、异步文件和正文层的完整原因链。 */
typedef enum xhttpstaticfileerror {
	XHTTP_STATIC_FILE_ERROR_SUBMIT = 1,
	XHTTP_STATIC_FILE_ERROR_OPEN,
	XHTTP_STATIC_FILE_ERROR_STAT,
	XHTTP_STATIC_FILE_ERROR_TYPE,
	XHTTP_STATIC_FILE_ERROR_SIZE,
	XHTTP_STATIC_FILE_ERROR_RANGE,
	XHTTP_STATIC_FILE_ERROR_ADOPT,
	XHTTP_STATIC_FILE_ERROR_BODY,
	XHTTP_STATIC_FILE_ERROR_REFERENCE,
	XHTTP_STATIC_FILE_ERROR_CONSUMED
} xhttpstaticfileerror;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)

/*
	静态响应计划的 MaxRanges 同时限制解析工作和调用方输出容量。
	MergeGap 表示两个闭区间之间最多允许多少未选择字节仍合并。
*/
typedef struct xhttpstaticplanconfig {
	size_t MaxRanges;
	uint64 MergeGap;
} xhttpstaticplanconfig;



/*
	计划只描述协议结果，不创建 Reply 或 Body。
	Ranges 的前 RangeCount 项按起点排序并且已经合并。
*/
typedef struct xhttpstaticplan {
	uint16 Status;
	bool SendBody;
	bool AcceptRanges;
	uint64 CompleteLength;
	uint64 SelectedLength;
	size_t RangeCount;
} xhttpstaticplan;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 静态响应最多生成七个协议字段，业务字段仍由调用方自由追加。 */
#define XHTTP_STATIC_RESPONSE_MAX_FIELDS 7u



/*
	ContentType 为空时使用 application/octet-stream，CacheControl 为空时省略。
	多范围响应必须提供 1 到 70 字节的 token Boundary，正文长度由范围自动计算。
*/
typedef struct xhttpstaticresponseconfig {
	xstrview ContentType;
	xstrview CacheControl;
	xstrview Boundary;
} xhttpstaticresponseconfig;



/*
	响应字段借用配置文本、静态文本和调用方工作区。
	结构本身不拥有内存，可以直接交给 HTTP/1、HTTP/2 或自定义响应层。
*/
typedef struct xhttpstaticresponse {
	uint16 Status;
	bool SendBody;
	bool Multipart;
	uint64 BodyLength;
	xstrview ContentType;
	xstrview Boundary;
	xhttpfield Fields[XHTTP_STATIC_RESPONSE_MAX_FIELDS];
	size_t FieldCount;
} xhttpstaticresponse;

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

/* 静态多范围正文只补充采用阶段错误，读取错误保留文件正文的完整原因链。 */
typedef enum xhttpstaticmultipartbodyerror {
	XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT = 1
} xhttpstaticmultipartbodyerror;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)

/* 初始化根挂载、可移植文件名和默认拒绝隐藏段的安全配置；输出结构可以未对齐。 */
XHTTP_SERVER_API void xrtHttpStaticPathConfigInit(
	xhttpstaticpathconfig* pConfig
);



/*
	把原始 URL path 映射为零结尾的相对文件路径。
	空输出只查询长度；容量不足不修改输出，并通过 pSize 返回所需正文长度。
	配置、pSize 与 pTrailingSlash 可以未对齐，但都必须是完整且不回绕的内存区间。
*/
XHTTP_SERVER_API xhttpstaticpathstatus xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
);



/* 初始化一个可交给映射与释放函数使用的空拥有型结果；结果结构可以未对齐。 */
XHTTP_SERVER_API void xrtHttpStaticPathInit(
	xhttpstaticpath* pPath
);



/*
	分配并映射相对路径；正常未命中返回 true 且 Matched 为 false。
	pPath 必须是已经初始化或释放后的空结果。
*/
XHTTP_SERVER_API bool xrtHttpStaticPathMap(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	xhttpstaticpath* pPath
);



/* 释放拥有型路径并恢复为空结果；空指针和空结果均可安全传入。 */
XHTTP_SERVER_API void xrtHttpStaticPathFree(
	xhttpstaticpath* pPath
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)

/*
	同步打开文件根内的普通文件，并采用同一文件句柄作为异步资源。
	该函数可能执行阻塞文件系统操作；调用方拥有返回引用。
*/
XHTTP_SERVER_API xhttpstaticfile* xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
);



/*
	在任务池中打开文件根内的普通文件。
	成功 Future 的值为借用 xhttpstaticfile；Root 必须存活到 Future 终态。
	路径文本在函数返回前复制；任务池必须存活到资源及其正文全部释放。
*/
XHTTP_SERVER_API xfuture* xrtHttpStaticFileFuture(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
);



/* 增加静态文件资源引用并返回原指针。 */
XHTTP_SERVER_API xhttpstaticfile* xrtHttpStaticFileRef(
	xhttpstaticfile* pFile
);



/* 释放资源引用；尚未取走的异步文件会进入异步关闭流程。 */
XHTTP_SERVER_API void xrtHttpStaticFileDestroy(
	xhttpstaticfile* pFile
);



/* 返回从最终打开文件句柄取得的借用只读元数据。 */
XHTTP_SERVER_API const xfileinfo* xrtHttpStaticFileInfo(
	const xhttpstaticfile* pFile
);



/*
	返回借用的当前表示验证器。
	弱 ETag 和 Last-Modified 直接来自同一打开文件的稳定元数据。
*/
XHTTP_SERVER_API const xhttprepresentation* xrtHttpStaticFileRepresentation(
	const xhttpstaticfile* pFile
);



/* 返回静态文件的完整字节长度；参数无效时返回零并设置错误。 */
XHTTP_SERVER_API uint64 xrtHttpStaticFileSize(
	const xhttpstaticfile* pFile
);



/*
	一次性取走底层异步文件，供 multipart 或自定义正文实现使用。
	成功后静态文件对象仍保留元数据，但不能再次创建正文。
*/
XHTTP_SERVER_API xasyncfile* xrtHttpStaticFileTakeFile(
	xhttpstaticfile* pFile
);



/*
	一次性创建并取走严格文件区间正文。
	失败时资源仍归静态文件对象，调用方可以修正参数或内存条件后重试。
*/
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeBody(
	xhttpstaticfile* pFile,
	uint64 iOffset,
	uint64 iLength
);



/* 一次性创建并取走完整文件正文。 */
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeBodyAll(
	xhttpstaticfile* pFile
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)

/* 初始化静态响应计划默认值；输出可以未对齐，但必须覆盖完整结构范围。 */
XHTTP_SERVER_API void xrtHttpStaticPlanConfigInit(
	xhttpstaticplanconfig* pConfig
);



/*
	评估 GET/HEAD 静态表示的条件请求和 Range。
	Ranges 容量必须至少为 Config.MaxRanges，且不得覆盖任何输入或 Plan。
	配置、字段数组、Current、Ranges 和 Plan 均可未对齐，但范围必须完整且不回绕。
*/
XHTTP_SERVER_API bool xrtHttpStaticPlanBuild(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iRangeCapacity,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 初始化默认响应配置；输出可以未对齐，但必须覆盖完整且不回绕的结构范围。 */
XHTTP_SERVER_API void xrtHttpStaticResponseConfigInit(
	xhttpstaticresponseconfig* pConfig
);



/*
	把静态计划转换为状态、正文长度和协议字段。
	pResponse 为空时执行精确工作区长度查询；实际构建不分配内存。
	固定结构、范围数组和标量输出均可未对齐；函数只发布完整成功结果。
*/
XHTTP_SERVER_API bool xrtHttpStaticResponseBuild(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	void* pWorkspace,
	size_t iCapacity,
	size_t* pSize,
	xhttpstaticresponse* pResponse
);

#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

/*
	采用一个可读异步文件并创建不可重放的多范围正文。
	成功后正文独占关闭文件；失败时文件所有权仍归调用方。
	范围数组可以未对齐，但必须覆盖完整且不回绕的结构范围；函数返回前会复制范围。
*/
XHTTP_SERVER_API xhttpbody* xrtHttpStaticMultipartBodyAdopt(
	xasyncfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
);



/*
	从静态文件资源一次性取走底层文件并创建多范围正文。
	任何构造失败都不会消费静态文件，调用方可以修正后重试。
	范围数组可以未对齐，但必须覆盖完整且不回绕的结构范围；函数返回前会复制范围。
*/
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeMultipartBody(
	xhttpstaticfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	xstrview ContentType,
	xstrview Boundary
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_sse_server.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SSE_SERVER_H
#define XRT_HTTP_SSE_SERVER_H





#if defined(XHTTP_FEATURE_HTTP_SSE_SERVER) && \
	(!defined(XHTTP_FEATURE_HTTP_SSE_HTTP) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_STREAM) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_REPLY))
	#error "XRT HTTP SSE server requires SSE HTTP, body stream and server Reply support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SSE_SERVER)

/*
	创建 200 SSE Reply 和独立生产端；成功后可继续修改 Reply 应用字段。
	配置会立即快照，输出句柄可以未对齐，但不得与非空配置重叠。
*/
XHTTP_SERVER_API xhttpreply* xrtHttpSseReplyCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
);



/* 把 UTF-8 数据封装为一条默认 message 事件并原子提交。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSend(
	xhttpbodystream* pStream,
	xstrview Data
);



/* 单分配封装并提交完整事件；AGAIN 时不保留任何输入引用。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSendEvent(
	xhttpbodystream* pStream,
	const xhttpsseevent* pEvent
);



/* 单分配封装并提交一条或多条注释心跳。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSendComment(
	xhttpbodystream* pStream,
	xstrview Comment
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_runtime.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_RUNTIME_H
#define XRT_HTTP_SERVER_RUNTIME_H





#if defined(XHTTP_FEATURE_HTTP_SERVER) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_EXCHANGE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE) || \
	 !defined(XRT_FEATURE_SPIN) || \
	 !defined(XRT_FEATURE_NET_TCP_SERVER))
	#error "XRT HTTP server requires Exchange, response and TCP Server support"
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC))
	#error "XRT async HTTP server body support requires server and async response support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER)

typedef struct xhttpserver xhttpserver;
typedef struct xhttpconn xhttpconn;



/* Server 状态单向进入排空、终止和关闭终态。 */
typedef enum xhttpserverstate {
	XHTTP_SERVER_RUNNING = 0,
	XHTTP_SERVER_DRAINING,
	XHTTP_SERVER_ABORTING,
	XHTTP_SERVER_CLOSED
} xhttpserverstate;



/* Connection 状态描述当前可观察阶段，keep-alive 时可以重新进入 REQUEST。 */
typedef enum xhttpconnstate {
	XHTTP_CONN_REQUEST = 0,
	XHTTP_CONN_BODY,
	XHTTP_CONN_WAITING,
	XHTTP_CONN_INFORMATION,
	XHTTP_CONN_RESPONSE,
	XHTTP_CONN_UPGRADED,
	XHTTP_CONN_CLOSING,
	XHTTP_CONN_CLOSED
} xhttpconnstate;



/* 服务端错误按配置、监听、TLS、协议、响应、回调和超时来源稳定分类。 */
typedef enum xhttpservererror {
	XHTTP_SERVER_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_ERROR_CONFIG,
	XHTTP_SERVER_ERROR_LISTEN,
	XHTTP_SERVER_ERROR_TLS,
	XHTTP_SERVER_ERROR_CONNECTION,
	XHTTP_SERVER_ERROR_PROTOCOL,
	XHTTP_SERVER_ERROR_RESPONSE,
	XHTTP_SERVER_ERROR_CALLBACK,
	XHTTP_SERVER_ERROR_TIMEOUT_HEADER,
	XHTTP_SERVER_ERROR_TIMEOUT_BODY,
	XHTTP_SERVER_ERROR_TIMEOUT_REQUEST,
	XHTTP_SERVER_ERROR_TIMEOUT_IDLE,
	XHTTP_SERVER_ERROR_TIMEOUT_WRITE,
	XHTTP_SERVER_ERROR_UPGRADE,
	XHTTP_SERVER_ERROR_STATE,
	XHTTP_SERVER_ERROR_INTERNAL
} xhttpservererror;



/*
	全部超时使用毫秒，零表示关闭对应保护。
	WriteSize 只限制单次零复制发送租约，不建立每连接固定缓冲。
	MaxConnections 为零时不限制，仍受系统和 Engine 硬边界约束。
*/
typedef struct xhttpserverconfig {
	xnetserverconfig Network;
	xhttp1serverconfig Http1;
	size_t WriteSize;
	int64 HeaderTimeout;
	int64 BodyTimeout;
	int64 RequestTimeout;
	int64 IdleTimeout;
	int64 WriteTimeout;
	size_t MaxConnections;
	size_t MaxInformations;
} xhttpserverconfig;



/*
	Headers、Body 和 Request 在连接 Worker 上串行执行。
	Headers 为空时默认缓冲正文；Body 只在 STREAM 策略下调用。
	Body 可以暂停当前请求正文，异步消费者完成后从任意线程请求恢复。
	Headers 可直接提交最终响应：无正文请求正常完成并可继续 keep-alive，有正文请求
	停在 Header 边界并在响应排空后关闭。Body 提交最终响应后不再交付剩余正文，
	回调返回值不再替换已经固定的最终响应。
	Request 收到完整拥有型请求，响应可以在回调内同步提交或稍后提交。
*/
typedef struct xhttpserverevents {
	void (*Open)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		ptr pData
	);
	xhttpserverbodypolicy (*Headers)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		ptr pData
	);
	bool (*Body)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		xbytesview Data,
		ptr pData
	);
	void (*Request)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xhttpserverrequest* pRequest,
		ptr pData
	);
	void (*Close)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		xnetresult Result,
		const xerror* pError,
		ptr pData
	);
	void (*Error)(
		xhttpserver* pServer,
		xhttpconn* pConnection,
		const xerror* pError,
		ptr pData
	);
	/* 全部 HTTP Connection 运行时（含剩余适配器）和受理的 Upgrade 回调退出后发布。
	 * 调用方仍可持有已关闭/升级的 Connection 引用，不延迟内部运行时退休。 */
	void (*Shutdown)(xhttpserver* pServer, ptr pData);
	ptr Data;
} xhttpserverevents;



/* Server 统计全部是无锁累计快照；Close 回调前活跃连接数已经退账。 */
typedef struct xhttpserverstats {
	xhttpserverstate State;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Requests;
	uint64 Responses;
	uint64 Informations;
	uint64 Upgraded;
	uint64 ProtocolErrors;
	int64 Timeouts;
	size_t Connections;
	size_t PeakConnections;
	size_t Endpoints;
	size_t Listeners;
	bool Secure;
} xhttpserverstats;



/* Connection 统计组合 HTTP 和 TCP/TLS 传输事实，不借出内部缓冲。 */
typedef struct xhttpconnstats {
	xhttpconnstate State;
	uint64 Requests;
	uint64 RequestWireBytes;
	uint64 ResponseWireBytes;
	size_t BufferedBytes;
	size_t QueuedBytes;
	bool RequestBodyPaused;
	bool RequestActive;
	bool FinalCommitted;
	bool Secure;
} xhttpconnstats;



XRT_EXTERN_C_BEGIN



/*
	初始化安全公网限额、TCP 背压和分阶段超时默认值。
	配置允许位于未对齐存储，Server 启动时会先复制完整快照。
*/
XHTTP_SERVER_API void xrtHttpServerConfigInit(xhttpserverconfig* pConfig);



/*
	初始化全部为空的服务端事件表。
	事件表允许位于未对齐存储，Server 启动时会先复制完整快照。
*/
XHTTP_SERVER_API void xrtHttpServerEventsInit(xhttpserverevents* pEvents);



/* 创建并立即启动一个明文 HTTP/1 TCP Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
);



/* 增加 Server 引用并返回原指针。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerRef(xhttpserver* pServer);



/* 释放 Server 引用；不会隐式停止仍在运行的 Server。 */
XHTTP_SERVER_API void xrtHttpServerDestroy(xhttpserver* pServer);



/* 停止接受新连接，关闭空闲连接并让活动请求发送最后一个响应。 */
XHTTP_SERVER_API bool xrtHttpServerDrain(xhttpserver* pServer);



/* 停止接受并异常关闭全部 HTTP 连接。 */
XHTTP_SERVER_API bool xrtHttpServerAbort(xhttpserver* pServer);



/* 返回 Server 当前生命周期状态。 */
XHTTP_SERVER_API xhttpserverstate xrtHttpServerState(const xhttpserver* pServer);



/* 返回 Server 是否使用 TLS 传输。 */
XHTTP_SERVER_API bool xrtHttpServerSecure(const xhttpserver* pServer);



/* 返回 HTTP Server 的逻辑监听端点数量。 */
XHTTP_SERVER_API size_t xrtHttpServerEndpointCount(
	const xhttpserver* pServer
);



/* 复制指定逻辑端点的实际监听地址；输出可未对齐，但不得覆盖 Server。 */
XHTTP_SERVER_API bool xrtHttpServerLocal(
	const xhttpserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
);



/* 返回 HTTP Server 当前拥有的实际底层 Listener 数量。 */
XHTTP_SERVER_API size_t xrtHttpServerListenerCount(
	const xhttpserver* pServer
);



/* 增加并返回底层 TCP Server 引用，供特殊场景继续使用网络层能力。 */
XHTTP_SERVER_API xnetserver* xrtHttpServerNetwork(xhttpserver* pServer);



/* 返回导致网络 Server 终止的借用结构化错误。 */
XHTTP_SERVER_API const xerror* xrtHttpServerError(
	const xhttpserver* pServer
);



/* 复制 Server 并发统计；输出可未对齐，但不得覆盖 Server。 */
XHTTP_SERVER_API bool xrtHttpServerStats(
	const xhttpserver* pServer,
	xhttpserverstats* pStats
);



/* 增加 Connection 引用并返回原指针。 */
XHTTP_SERVER_API xhttpconn* xrtHttpConnRef(xhttpconn* pConnection);



/* 释放 Connection 引用；空指针视为空操作。 */
XHTTP_SERVER_API void xrtHttpConnDestroy(xhttpconn* pConnection);



/* 返回 Connection 当前协议阶段。 */
XHTTP_SERVER_API xhttpconnstate xrtHttpConnState(
	const xhttpconn* pConnection
);



/* 返回借用的所属 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpConnServer(
	const xhttpconn* pConnection
);



/* 返回当前请求的借用指针；没有活动请求时为空。 */
XHTTP_SERVER_API const xhttpserverrequest* xrtHttpConnRequest(
	const xhttpconn* pConnection
);



/* 返回 Connection 所属的借用 Worker。 */
XHTTP_SERVER_API xnetworker* xrtHttpConnWorker(
	const xhttpconn* pConnection
);



/* 只在 Connection Worker 回调内返回底层借用 TCP Stream，包括 TLS 的传输。 */
XHTTP_SERVER_API xnetstream* xrtHttpConnTcp(xhttpconn* pConnection);



/* 返回 Connection 是否使用 TLS 传输。 */
XHTTP_SERVER_API bool xrtHttpConnSecure(const xhttpconn* pConnection);



/* 返回接受当前 Connection 的逻辑 Server 端点索引。 */
XHTTP_SERVER_API size_t xrtHttpConnEndpoint(const xhttpconn* pConnection);



/* 复制连接本地地址；输出可未对齐、成功才修改，且不得覆盖 Connection。 */
XHTTP_SERVER_API bool xrtHttpConnLocal(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
);



/* 复制连接远端地址；输出可未对齐、成功才修改，且不得覆盖 Connection。 */
XHTTP_SERVER_API bool xrtHttpConnRemote(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
);



/* 返回导致连接关闭或协议终止的借用结构化错误。 */
XHTTP_SERVER_API const xerror* xrtHttpConnError(
	const xhttpconn* pConnection
);



/* 复制 Connection 协议和传输统计；输出可未对齐，但不得覆盖 Connection。 */
XHTTP_SERVER_API bool xrtHttpConnStats(
	const xhttpconn* pConnection,
	xhttpconnstats* pStats
);



/*
	在 Connection Worker 的 Headers 回调中替换当前请求正文硬上限。
	零表示不接受非空正文，UINT64_MAX 表示不增加应用层总量上限。
*/
XHTTP_SERVER_API bool xrtHttpConnSetRequestBodyLimit(
	xhttpconn* pConnection,
	uint64 iMaxBody
);



/*
	在 Connection Worker 的流式 Body 回调中暂停后续请求正文交付。
	当前回调片段视为已经接受；正文空闲超时在暂停期间继续生效。
*/
XHTTP_SERVER_API bool xrtHttpConnPauseRequestBody(
	xhttpconn* pConnection
);



/*
	从任意线程恢复请求正文交付，并让所属 Worker 处理已经缓冲的输入。
	未暂停时视为成功的空操作；关闭或升级后的连接返回 false。
*/
XHTTP_SERVER_API bool xrtHttpConnResumeRequestBody(
	xhttpconn* pConnection
);



/* 返回请求正文是否被应用暂停的并发快照。 */
XHTTP_SERVER_API bool xrtHttpConnRequestBodyPaused(
	const xhttpconn* pConnection
);



/*
	在 Connection Worker 上提交一条信息响应。
	发送完成后继续当前请求，不能代替最终响应。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnInform(
	xhttpconn* pConnection,
	const xhttpreply* pReply
);



/* 在 Connection Worker 上提交唯一最终 Reply。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespond(
	xhttpconn* pConnection,
	const xhttpreply* pReply
);



/* 一次调用复制常用固定正文并提交最终响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnReply(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body
);



/*
	一次调用保留可选 Body 引用并提交最终响应。
	调用返回后可以立即销毁调用方引用；未知长度正文按请求版本选择 chunked 或关闭分帧。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnReplyBody(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody
);



/* 从任意线程停止读取并在已经受理的响应字节排空后关闭。 */
XHTTP_SERVER_API bool xrtHttpConnClose(xhttpconn* pConnection);



/* 从任意线程丢弃发送队列并异常关闭。 */
XHTTP_SERVER_API bool xrtHttpConnAbort(xhttpconn* pConnection);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_tls.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_TLS_H
#define XRT_HTTP_SERVER_TLS_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XRT_FEATURE_TLS_STREAM))
	#error "XRT HTTP server TLS requires HTTP server and TLS Stream support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/*
	Handshake 的 Context、Identity 和 Protocols 在启动时保存独立所有权。
	SelectContext 和 ResumeContext 由调用方保持到 Server 完全关闭。
*/
typedef struct xhttpservertlsconfig {
	xtlsserverconfig Handshake;
	xtlsstreamconfig Stream;
} xhttpservertlsconfig;



XRT_EXTERN_C_BEGIN



/*
	初始化 HTTP/1.1 ALPN、TLS 服务端和组合 Stream 默认策略。
	输出结构可以未对齐，但必须是完整且不回绕的可写范围。
*/
XHTTP_SERVER_API void xrtHttpServerTlsConfigInit(
	xhttpservertlsconfig* pConfig
);



/*
	创建并立即启动 TLS HTTP/1 Server。
	启动时只读取一次 TLS 配置；固定结构与 ALPN 描述符均允许未对齐，
	但必须覆盖完整且不回绕的范围。ALPN 字节会被深复制，TLS 对象会被保留；
	选择器与恢复回调的上下文仍由调用方保持到 Server 完全关闭。
*/
XHTTP_SERVER_API xhttpserver* xrtHttpServerStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	const xhttpserverevents* pEvents
);



/* 只在 Connection Worker 回调内返回借用 TLS Stream。 */
XHTTP_SERVER_API xtlsstream* xrtHttpConnTls(xhttpconn* pConnection);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_raw.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_RAW_H
#define XRT_HTTP_SERVER_RAW_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW) && \
	!defined(XHTTP_FEATURE_HTTP_SERVER)
	#error "XRT raw HTTP server response support requires HTTP server support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/*
	原始响应默认在完整报文排空后关闭连接。
	KEEP_ALIVE 只是一项请求；运行时仍会服从请求版本、Connection、半关闭和排空事实。
*/
typedef enum xhttpserverrawflag {
	XHTTP_SERVER_RAW_NONE = 0,
	XHTTP_SERVER_RAW_KEEP_ALIVE = UINT32_C(0x00000001)
} xhttpserverrawflag;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/*
	在 Connection Worker 上复制一条已经完整封包的 HTTP/1 响应并提交。

	运行时不会解析、改写或补全报文；调用方负责状态行、Header、分帧与请求方法语义。
	默认关闭连接，显式请求 KEEP_ALIVE 时也只有请求本身允许复用才会进入下一条请求。
	Response 必须覆盖完整且不回绕的非空字节范围。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	uint32 iFlags
);



/*
	零复制提交一条完整 HTTP/1 线缆响应。

	成功后转移 Ref 的释放责任，数据离开发送队列时执行一次 Release；失败时全部责任
	仍归调用方。空引用不转移责任，非空引用必须覆盖完整字节并提供 Release。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawRef(
	xhttpconn* pConnection,
	const xnetref* pResponse,
	uint32 iFlags
);



/*
	原子提交一组顺序组成完整 HTTP/1 线缆响应的零复制引用。

	成功后转移全部非空引用，失败时一个也不转移。总长度必须已知、非零且不溢出，
	描述符数组在函数返回后不再使用。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawRefs(
	xhttpconn* pConnection,
	const xnetref* pResponses,
	size_t iCount,
	uint32 iFlags
);



/*
	成功时接管由 xrtMalloc 分配的完整 HTTP/1 线缆响应，失败时所有权仍归调用方。
	非空数据必须具有非零长度。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawTake(
	xhttpconn* pConnection,
	ptr pResponse,
	size_t iSize,
	uint32 iFlags
);



/*
	在 Connection Worker 上以已知非零长度 Body 作为完整 HTTP/1 线缆响应。

	提交过程保留 Body 引用，调用方可以在函数返回后立即销毁自己的引用。Body 的
	Length 必须已知，实际输出必须严格等于该长度。它可以使用 Borrow、Take 或
	Reference 建立零复制和自定义所有权；异步 AGAIN 需要服务端异步正文组合层。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pResponse,
	uint32 iFlags
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_upgrade.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_UPGRADE_H
#define XRT_HTTP_SERVER_UPGRADE_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE))
	#error "XRT HTTP server Upgrade requires HTTP server response support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

typedef struct xtlsstream xtlsstream;



/*
	Upgrade 成功时恰好拥有一种传输。
	Buffered 是 HTTP 停止解释后已经留在传输明文缓冲中的字节数。
*/
typedef struct xhttpupgrade {
	xnetstream* Tcp;
	xtlsstream* Tls;
	size_t Buffered;
} xhttpupgrade;



/*
	受理成功后恰好调用一次，并且不会从提交函数调用栈重入。
	成功时 Upgrade 的传输所有权交给回调；失败时两种传输都为空。
	Error 只在回调期间借用，Connection 也只保证在回调期间有效。
	Server 的 Shutdown 与关闭 Future 等待该回调返回，但不等待新协议会话关闭。
*/
typedef void (*xhttpupgradeproc)(
	xhttpconn* pConnection,
	xnetresult Result,
	xhttpupgrade Upgrade,
	const xerror* pError,
	ptr pData
);



XRT_EXTERN_C_BEGIN



/*
	异常关闭并释放尚未交给新协议对象的 Upgrade 传输。
	结构可以未对齐，但必须覆盖完整且不回绕的范围；有效清理会先清空结构，且不改变已有错误。
*/
XHTTP_SERVER_API void xrtHttpUpgradeAbort(xhttpupgrade* pUpgrade);



/*
	以下三个提交入口只接受已经完整解析的当前请求，通常应在 Request 回调中调用。
	Headers 或 Body 回调中的请求仍不完整，提交会以 XERR_STATE 同步失败，也不会
	安排完成回调。这样传输余量只包含 HTTP 请求之后的新协议字节。
*/



/*
	提交已经拥有的 HTTP/1 Tunnel 响应计划。
	非空 Response 无论成功失败都由函数接管；只接受 101 Upgrade 或 CONNECT
	Tunnel 终态。受理后 HTTP 停止拥有该连接，并通过 Proc 交付结果。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnUpgradeResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	xhttpupgradeproc pProc,
	ptr pData
);



/*
	冻结 Reply 并提交 HTTP Upgrade。
	Reply 只在调用期间借用，通常是带 Connection、Upgrade 等字段的 101 响应。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnUpgrade(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	xhttpupgradeproc pProc,
	ptr pData
);



/*
	复制并原样发送完整 HTTP/1 切换响应，然后转移传输。
	该入口不解析 Response，调用方负责状态行、Header 和终止空行完全正确。
	Response 必须覆盖完整且不回绕的非空字节范围。
*/
XHTTP_SERVER_API xnetresult xrtHttpConnUpgradeRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	xhttpupgradeproc pProc,
	ptr pData
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_future.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_FUTURE_H
#define XRT_HTTP_SERVER_FUTURE_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER) || \
	 !defined(XRT_FEATURE_FUTURE_CONTINUE))
	#error "XRT HTTP server Future support requires HTTP server and Future continuation support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

XRT_EXTERN_C_BEGIN



/*
	等待 Server 进入 CLOSED 终态。
	全部 HTTP Connection 的协议、传输所有权和内部运行时均已退出，Shutdown 与已经受理的
	Upgrade 交接回调也已返回；已经转交的新协议会话可以仍然存活。
	每次调用建立独立等待；取消 Future 只摘除本次等待，不会排空或中止 Server。
	Server 已经关闭时返回立即成功的 Future。
*/
XHTTP_SERVER_API xfuture* xrtHttpServerWaitAsync(xhttpserver* pServer);



/*
	在 Connection Worker 上把当前请求的唯一最终响应绑定到 Future。

	Future 成功值必须是非空的借用 xhttpreply*。桥接层持有源 Future，并在所属
	Worker 上冻结 Reply；调用方或 Future 的值析构过程仍拥有 Reply。源 Future
	可以来自 Promise、任务池或协程，不需要 HTTP 专用任务类型。

	连接关闭、请求超时或其他最终响应先提交时会请求取消源 Future；取消是协作式
	通知，不要求生产者立即终止。已经投递的完成与手工响应由唯一最终响应门收敛。
	失败、超时和取消分别生成 500、504 和 503 兜底响应，并保留结构化原因。
*/
XHTTP_SERVER_API bool xrtHttpConnRespondFuture(
	xhttpconn* pConnection,
	xfuture* pFuture
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_file.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_FILE_H
#define XRT_HTTP_SERVER_FILE_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE) && \
	(!defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC))
	#error "XRT HTTP server file support requires file body, server Future and async body support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE)

XRT_EXTERN_C_BEGIN



/*
	异步准备完整文件正文，并返回拥有 xhttpreply* 的 Future。
	ContentType 为空时不添加 Content-Type，线路层自动生成 Content-Length。
*/
XHTTP_SERVER_API xfuture* xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
);



/*
	异步准备严格文件区间，并返回拥有 xhttpreply* 的 Future。
	区间越过准备时的文件大小会失败，不会静默截断。
*/
XHTTP_SERVER_API xfuture* xrtHttpReplyFileRangeFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
);



/*
	在 Connection Worker 上异步准备完整文件并绑定唯一最终响应。
	成功表示响应 Future 已受理，不表示文件已经打开或响应已经发送。
*/
XHTTP_SERVER_API bool xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
);



/*
	在 Connection Worker 上异步准备严格文件区间并绑定唯一最终响应。
	该函数不推断 Range、Content-Range、ETag 或缓存语义。
*/
XHTTP_SERVER_API bool xrtHttpConnFileRange(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_static.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_STATIC_H
#define XRT_HTTP_SERVER_STATIC_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) && \
	(!defined(XRT_FEATURE_CODEC_HEX) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	 !defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	 !defined(XHTTP_FEATURE_HTTP_RANGE_MULTIPART) || \
	 !defined(XHTTP_FEATURE_MIME_TYPES) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE))
	#error "XRT HTTP static server support requires codec, server Future, static protocol, MIME and secure random support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)

/* 静态 Reply 组合层的稳定错误代码保留失败发生的准确阶段。 */
typedef enum xhttpserverstaticerror {
	XHTTP_SERVER_STATIC_ERROR_CONFIG = 1,
	XHTTP_SERVER_STATIC_ERROR_TARGET,
	XHTTP_SERVER_STATIC_ERROR_PATH,
	XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
	XHTTP_SERVER_STATIC_ERROR_PLAN,
	XHTTP_SERVER_STATIC_ERROR_RESPONSE,
	XHTTP_SERVER_STATIC_ERROR_BODY,
	XHTTP_SERVER_STATIC_ERROR_REPLY
} xhttpserverstaticerror;



/*
	Reply 配置同时服务已打开文件和一站式异步服务。
	ContentType 为空时按资源路径推断；Boundary 为空时仅在多范围响应中自动生成。
*/
typedef struct xhttpstaticreplyconfig {
	xhttpstaticplanconfig Plan;
	xstrview ContentType;
	xstrview CacheControl;
	xstrview Boundary;
} xhttpstaticreplyconfig;



/*
	服务配置把 URL 挂载、有序目录索引和静态 Reply 策略组成一个完整但可替换的高层路径。
	Indexes 按顺序借用 IndexCount 个安全文件名；计数为零时不为尾随斜杠查找索引文件。
*/
typedef struct xhttpstaticserveconfig {
	xhttpstaticpathconfig Path;
	xhttpstaticreplyconfig Reply;
	const xstrview* Indexes;
	size_t IndexCount;
} xhttpstaticserveconfig;



XRT_EXTERN_C_BEGIN



/* 初始化默认 Reply 配置；输出可以未对齐，但必须覆盖完整结构范围。 */
XHTTP_SERVER_API void xrtHttpStaticReplyConfigInit(
	xhttpstaticreplyconfig* pConfig
);



/* 初始化默认服务配置；输出可以未对齐，但必须覆盖完整结构范围。 */
XHTTP_SERVER_API void xrtHttpStaticServeConfigInit(
	xhttpstaticserveconfig* pConfig
);



/*
	把纯静态响应描述转换为拥有型 Reply，并保留可选正文的独立引用。
	SendBody 为 true 时必须提供声明了精确长度的正文，且长度必须与响应一致；无正文响应不得附带正文。
	响应描述符可以未对齐；函数先验证并快照全部字段，再创建拥有型 Reply。
*/
XHTTP_SERVER_API xhttpreply* xrtHttpReplyFromStatic(
	const xhttpstaticresponse* pResponse,
	xhttpbody* pBody
);



/*
	根据请求、已打开的安全静态文件和资源路径构建完整 Reply。
	成功时从 pFile 一次性取走正文文件；失败时尽量保持文件可重试，调用方始终拥有 pFile。
	可选配置可以未对齐，并在任何异步或文件操作前完成一次快照。
*/
XHTTP_SERVER_API xhttpreply* xrtHttpReplyStatic(
	const xhttpserverrequest* pRequest,
	xhttpstaticfile* pFile,
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pConfig
);



/*
	映射请求路径并在任务池中打开根内文件，返回拥有 xhttpreply* 的 Future。
	路径未命中、被安全策略拒绝或资源不存在时成功生成 404；格式错误的 target 生成 400。
	Root 必须存活到 Future 终态；pPool 必须继续覆盖 Reply 正文和异步文件关闭。
	请求引用与配置文本由函数独立保留；取消 Future 会跳过尚未开始的 Reply 组合。
	可选配置可以未对齐；函数返回后调用方可立即修改或释放配置及其借用文本。
*/
XHTTP_SERVER_API xfuture* xrtHttpReplyStaticFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpserverrequest* pRequest,
	const xhttpstaticserveconfig* pConfig
);



/*
	为 Connection 当前请求启动静态文件 Future 并绑定唯一最终响应。
	成功只表示 Future 已被连接接管，不表示文件已经打开或响应已经发送。
*/
XHTTP_SERVER_API bool xrtHttpConnStatic(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticserveconfig* pConfig
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_route.h */
/* ========================================================================== */

#ifndef XRT_HTTP_ROUTE_H
#define XRT_HTTP_ROUTE_H





#if defined(XHTTP_FEATURE_HTTP_ROUTE) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP route support requires HTTP support"
#endif



#if defined(XHTTP_FEATURE_HTTP_ROUTE)

/* 路由匹配结果区分调用错误、正常未命中、完整命中和捕获存储不足。 */
typedef enum xhttproutestatus {
	XHTTP_ROUTE_ERROR = -1,
	XHTTP_ROUTE_MISS = 0,
	XHTTP_ROUTE_MATCH = 1,
	XHTTP_ROUTE_MORE = 2
} xhttproutestatus;



/* 路由参数名称借用 Pattern，原始值借用 Path，均不保证以零结尾。 */
typedef struct xhttprouteparam {
	xstrview Name;
	xstrview Value;
} xhttprouteparam;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_ROUTE)

/*
	验证绝对路径模板并返回捕获数量。
	{name} 匹配一个非空路径段，{name...} 只能位于末尾并匹配剩余原始路径。
*/
XHTTP_SERVER_API bool xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
);



/*
	按字节和路径段严格匹配模板；重复斜杠与尾斜杠均有意义。
	空 Params 可查询捕获数量；容量不足返回 MORE，且不写入任何 Param。
*/
XHTTP_SERVER_API xhttproutestatus xrtHttpRouteMatch(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount
);



/*
	按区分大小写的名称查找首个借用参数；未找到返回空指针且不设置错误。
	若输入描述符数组未对齐，返回地址也可能未对齐；调用方应显式转成
	const void*，再以 memcpy 读取到对齐的局部值。
*/
XHTTP_SERVER_API const xhttprouteparam* xrtHttpRouteParam(
	const xhttprouteparam* pParams,
	size_t iCount,
	xstrview Name
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_router.h */
/* ========================================================================== */

#ifndef XRT_HTTP_ROUTER_H
#define XRT_HTTP_ROUTER_H





#if defined(XHTTP_FEATURE_HTTP_ROUTER) && \
	!defined(XHTTP_FEATURE_HTTP_ROUTE)
	#error "XRT HTTP router support requires HTTP route support"
#endif



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* Router 是注册阶段可变、冻结后只读并发的拥有型路由索引。 */
typedef struct xhttprouter xhttprouter;



/* Router 结果明确区分调用错误、404、405、完整命中和捕获容量不足。 */
typedef enum xhttprouterstatus {
	XHTTP_ROUTER_ERROR = -1,
	XHTTP_ROUTER_NOT_FOUND = 0,
	XHTTP_ROUTER_MATCH = 1,
	XHTTP_ROUTER_MORE = 2,
	XHTTP_ROUTER_METHOD_NOT_ALLOWED = 3
} xhttprouterstatus;



/* HEAD 使用同一路径的 GET 路由时记录显式回退事实。 */
#define XHTTP_ROUTER_HEAD_FALLBACK UINT32_C(0x00000001)

/* 命中注册方法为星号的任意方法路由。 */
#define XHTTP_ROUTER_ANY_METHOD UINT32_C(0x00000002)



/* 显式限额约束注册规模；全部字节包含模板、方法和静态边副本。 */
typedef struct xhttprouterconfig {
	size_t MaxRoutes;
	size_t MaxNodes;
	size_t MaxBytes;
} xhttprouterconfig;



/* 命中结果借用冻结 Router 中的方法和模板，Value 保留注册时的借用指针。 */
typedef struct xhttproutermatch {
	uint32 Flags;
	xstrview Method;
	xstrview Pattern;
	ptr Value;
} xhttproutermatch;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* 初始化最多 4096 条路由、16384 个节点和 4 MiB 文本的默认配置；存储可以未对齐。 */
XHTTP_SERVER_API void xrtHttpRouterConfigInit(xhttprouterconfig* pConfig);



/* 创建空 Router；配置可以未对齐，并在返回前被按值持有。 */
XHTTP_SERVER_API xhttprouter* xrtHttpRouterCreate(
	const xhttprouterconfig* pConfig
);



/* 销毁 Router 自身资源；注册 Value 始终由调用方持有。 */
XHTTP_SERVER_API void xrtHttpRouterDestroy(xhttprouter* pRouter);



/*
	复制并注册一个方法与模板；Method 为 "*" 时匹配任意合法请求方法。
	同一结构路径不能重复注册同一方法，参数名可以按方法分别定义。
*/
XHTTP_SERVER_API bool xrtHttpRouterAdd(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	ptr pValue
);



/*
	冻结静态边并允许无锁并发匹配；重复调用是安全的。
	冻结失败时 Router 仍保持可注册状态，成功后不能继续 Add。
*/
XHTTP_SERVER_API bool xrtHttpRouterFreeze(xhttprouter* pRouter);



/* 判断 Router 是否已经进入不可变匹配阶段。 */
XHTTP_SERVER_API bool xrtHttpRouterFrozen(const xhttprouter* pRouter);



/* 返回当前注册路由数量。 */
XHTTP_SERVER_API size_t xrtHttpRouterCount(const xhttprouter* pRouter);



/* 返回当前预编译节点数量，空 Router 仍包含根节点。 */
XHTTP_SERVER_API size_t xrtHttpRouterNodes(const xhttprouter* pRouter);



/* 返回当前实际持有的方法、模板和静态边文本字节数。 */
XHTTP_SERVER_API size_t xrtHttpRouterBytes(const xhttprouter* pRouter);



/*
	在冻结 Router 中按静态、参数、尾参数优先级匹配方法与原始路径。
	容量不足返回 MORE，Match 和 Count 有效，但不会写入任何 Param。
	固定输出和捕获数组支持未对齐存储，参数错误不修改任何输出。
	所有输出及 Method、Path 不得相互重叠。
*/
XHTTP_SERVER_API xhttprouterstatus xrtHttpRouterMatch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount,
	xhttproutermatch* pMatch
);



/*
	列出至少一条匹配结构路径允许的唯一注册方法，并为 GET 合成 HEAD。
	容量不足返回 MORE 和精确需求，且不写入任何 Method；结果均借用 Router。
	方法数组和 Count 支持未对齐存储，参数错误不修改任何输出。
*/
XHTTP_SERVER_API xhttprouterstatus xrtHttpRouterMethods(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t iCapacity,
	size_t* pCount
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_compress.h */
/* ========================================================================== */

#ifndef XRT_HTTP_COMPRESS_H
#define XRT_HTTP_COMPRESS_H





#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_REPLY) || \
	 !defined(XRT_FEATURE_HTTP_ENCODING) || \
	 !defined(XHTTP_FEATURE_HTTP_VARY) || \
	 !defined(XHTTP_FEATURE_HTTP_BODY_DEFLATE) || \
	 !defined(XHTTP_FEATURE_HTTP_CACHE) || \
	 !defined(XHTTP_FEATURE_MIME) || \
	 !defined(XHTTP_FEATURE_HTTP_ETAG))
	#error "XRT HTTP Reply compression requires Reply, encoding, Vary, cache, body Deflate, MIME and ETag support"
#endif

#if defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS) && \
	(!defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_REQUEST))
	#error "XRT HTTP server compression requires Reply compression and server request support"
#endif



#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS)

#define XHTTP_REPLY_COMPRESS_MIN_DEFAULT UINT64_C(1024)
#define XHTTP_REPLY_COMPRESS_EAGER_DEFAULT 65536u



/* 压缩结果区分不适用、identity 克隆、已编码克隆和没有可接受表示。 */
typedef enum xhttpreplycompressstatus {
	XHTTP_REPLY_COMPRESS_ERROR = -1,
	XHTTP_REPLY_COMPRESS_SKIP = 0,
	XHTTP_REPLY_COMPRESS_IDENTITY = 1,
	XHTTP_REPLY_COMPRESS_APPLIED = 2,
	XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE = 3
} xhttpreplycompressstatus;



/* 压缩标志只打开默认策略有意关闭的路径。 */
typedef enum xhttpreplycompressflag {
	XHTTP_REPLY_COMPRESS_NONE = 0,
	XHTTP_REPLY_COMPRESS_ALLOW_ABSENT = UINT32_C(0x00000001),
	XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH = UINT32_C(0x00000002),
	XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE = UINT32_C(0x00000004),
	XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM = UINT32_C(0x00000008),
	XHTTP_REPLY_COMPRESS_KEEP_LARGER = UINT32_C(0x00000010)
} xhttpreplycompressflag;



/* Reply 压缩域错误用于识别配置、响应元数据和协商状态问题。 */
typedef enum xhttpreplycompresserror {
	XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT = 1,
	XHTTP_REPLY_COMPRESS_ERROR_CONFIG,
	XHTTP_REPLY_COMPRESS_ERROR_RESPONSE,
	XHTTP_REPLY_COMPRESS_ERROR_HEADER
} xhttpreplycompresserror;



/*
	Codings 只接受 gzip 与 deflate，identity 始终作为回退表示参与协商。
	EagerLimit 为零时全部正文都使用流式变换。
	ReadSize 控制来源推进粒度；QueueLimit 限制尚未交付的压缩载荷，零表示不限制。
*/
typedef struct xhttpreplycompressconfig {
	uint32 Codings;
	xhttpcoding Preferred;
	uint64 MinimumSize;
	uint64 MaximumSize;
	size_t EagerLimit;
	size_t ReadSize;
	size_t QueueLimit;
	int32 Level;
	xdeflatestrategy Strategy;
	uint64 OutputLimit;
	uint32 Flags;
} xhttpreplycompressconfig;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS)

/* 初始化 gzip/deflate、1 KiB 下限、64 KiB eager、1 MiB 队列和级别 6。 */
XHTTP_SERVER_API void xrtHttpReplyCompressConfigInit(
	xhttpreplycompressconfig* pConfig
);



/*
	按协商状态生成独立 Reply，绝不修改输入 Reply。
	IDENTITY 与 APPLIED 返回拥有型 Output；其他结果把有效且不重叠的 Output 置为空。
	Output 槽允许未对齐存储，但完整可写区间不得回绕或覆盖输入。
*/
XHTTP_SERVER_API xhttpreplycompressstatus xrtHttpReplyCompress(
	const xhttpacceptencoding* pAccept,
	xstrview Method,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
);

#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS)

/*
	合并请求中的全部 Accept-Encoding 字段后调用 Reply 压缩层。
	该函数不要求请求正文已经接收完成。
	失败时把有效且不与输入重叠的 Output 置空，并发布 compress-server-reply 错误。
	Output 槽允许未对齐存储，但完整可写区间不得回绕或覆盖输入。
*/
XHTTP_SERVER_API xhttpreplycompressstatus xrtHttpServerReplyCompress(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_router.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_ROUTER_H
#define XRT_HTTP_SERVER_ROUTER_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) && \
	(!defined(XHTTP_FEATURE_HTTP_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER))
	#error "XRT HTTP server router requires HTTP router and server support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

typedef struct xhttpserverrouter xhttpserverrouter;



/* 服务端路由错误区分参数、生命周期、目标、分配、响应和内部索引故障。 */
typedef enum xhttpserverroutererror {
	XHTTP_SERVER_ROUTER_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_ROUTER_ERROR_STATE,
	XHTTP_SERVER_ROUTER_ERROR_TARGET,
	XHTTP_SERVER_ROUTER_ERROR_LIMIT,
	XHTTP_SERVER_ROUTER_ERROR_MEMORY,
	XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
	XHTTP_SERVER_ROUTER_ERROR_START,
	XHTTP_SERVER_ROUTER_ERROR_INTERNAL
} xhttpserverroutererror;



/* Header 路由回调可按请求选择正文策略，并可在连接上设置细化正文限额。 */
typedef xhttpserverbodypolicy (*xhttpserverrouteheadersproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
);



/* Body 路由回调只借用当前片段和参数描述符，返回 false 会终止请求。 */
typedef bool (*xhttpserverroutebodyproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xbytesview Data,
	ptr pData
);



/* Request 路由回调接收完整请求；允许同步响应或保留 Connection 后异步响应。 */
typedef void (*xhttpserverrouteproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
);



/* Router 最后释放成功注册的路由时调用一次 Data 清理器。 */
typedef void (*xhttpserverrouterreleaseproc)(ptr pData);



/* 每条路由复制自己的分阶段回调、用户数据和可选清理器。 */
typedef struct xhttpserverrouteevents {
	xhttpserverrouteheadersproc Headers;
	xhttpserverroutebodyproc Body;
	xhttpserverrouteproc Request;
	xhttpserverrouterreleaseproc Release;
	ptr Data;
} xhttpserverrouteevents;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/*
	初始化空路由事件；完整请求回调应在注册前设置。
	事件描述符支持未对齐存储，输出范围无效时设置结构化参数错误。
*/
XHTTP_SERVER_API void xrtHttpServerRouteEventsInit(
	xhttpserverrouteevents* pEvents
);



/*
	创建拥有回调记录和通用结构索引的服务端 Router。
	可选配置在返回前完整复制，支持未对齐的固定配置描述符。
*/
XHTTP_SERVER_API xhttpserverrouter* xrtHttpServerRouterCreate(
	const xhttprouterconfig* pConfig
);



/* 增加 Router 引用；冻结后的 Router 可由多个运行中 Server 共享。 */
XHTTP_SERVER_API xhttpserverrouter* xrtHttpServerRouterRef(
	xhttpserverrouter* pRouter
);



/* 释放 Router 引用；空指针是安全的空操作。 */
XHTTP_SERVER_API void xrtHttpServerRouterDestroy(
	xhttpserverrouter* pRouter
);



/*
	复制并注册一组分阶段路由回调；Request 不能为空。
	成功后 Router 接管一次 Release(Data) 责任，失败时所有权仍属于调用方。
	事件表在返回前完整复制，支持未对齐的固定描述符。
*/
XHTTP_SERVER_API bool xrtHttpServerRouteEvents(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	const xhttpserverrouteevents* pEvents
);



/* 注册只处理完整请求的常用路由。 */
XHTTP_SERVER_API bool xrtHttpServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 GET 路由；HEAD 自动回退到同一路径的 GET。 */
XHTTP_SERVER_API bool xrtHttpServerGet(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 POST 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPost(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 PUT 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPut(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 PATCH 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPatch(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册常用 DELETE 路由。 */
XHTTP_SERVER_API bool xrtHttpServerDelete(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 注册任意合法 HTTP 方法路由。 */
XHTTP_SERVER_API bool xrtHttpServerAny(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
);



/* 冻结 Router；成功后可无锁并发分发且不能继续注册。 */
XHTTP_SERVER_API bool xrtHttpServerRouterFreeze(
	xhttpserverrouter* pRouter
);



/* 返回 Router 是否已经冻结。 */
XHTTP_SERVER_API bool xrtHttpServerRouterFrozen(
	const xhttpserverrouter* pRouter
);



/* 返回已注册路由数量。 */
XHTTP_SERVER_API size_t xrtHttpServerRouterCount(
	const xhttpserverrouter* pRouter
);



/* 对完整请求执行一次分发；未命中时自动响应 404、405 或 OPTIONS。 */
XHTTP_SERVER_API xhttprouterstatus xrtHttpServerRouterDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest
);



/*
	启动由 Router 分发的明文 HTTP/1 Server。
	Events.Request 在此入口中是未命中回调；其他事件保持原始含义。
*/
XHTTP_SERVER_API xhttpserver* xrtHttpServerRouterStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_middleware.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_MIDDLEWARE_H
#define XRT_HTTP_SERVER_MIDDLEWARE_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE) && \
	!defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)
	#error "XRT HTTP server middleware requires server router support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

typedef struct xhttpservernext xhttpservernext;



/* 中间件错误稳定区分参数、冻结状态、容量、分配、Next 和回调失败。 */
typedef enum xhttpservermiddlewareerror {
	XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_MIDDLEWARE_ERROR_STATE,
	XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT,
	XHTTP_SERVER_MIDDLEWARE_ERROR_MEMORY,
	XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT,
	XHTTP_SERVER_MIDDLEWARE_ERROR_CALLBACK
} xhttpservermiddlewareerror;



/*
	同步中间件可在调用 Next 前后执行逻辑，或不调用 Next 而直接响应或异步接管。
	返回 false 表示不可恢复的应用错误；Next 只在当前回调栈内有效且最多调用一次。
*/
typedef bool (*xhttpservermiddlewareproc)(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	xhttpservernext* pNext,
	ptr pData
);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/*
	把一层同步中间件追加到 Router；调用顺序与注册顺序相同。
	成功后 Data 仍由调用方管理，且必须覆盖 Router 的完整生命周期。
*/
XHTTP_SERVER_API bool xrtHttpServerUse(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData
);



/*
	追加一层由 Router 管理 Data 生命周期的同步中间件。
	成功后 Router 在最终销毁时调用一次 Release(Data)，失败时所有权不转移。
*/
XHTTP_SERVER_API bool xrtHttpServerUseOwned(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData,
	xhttpserverrouterreleaseproc pRelease
);



/* 返回 Router 已注册的中间件数量；空 Router 返回零。 */
XHTTP_SERVER_API size_t xrtHttpServerMiddlewareCount(
	const xhttpserverrouter* pRouter
);



/*
	继续执行下一层中间件或最终路由；同一个 Next 最多成功调用一次。
	返回 false 时当前请求进入失败路径，调用方应立即返回 false。
*/
XHTTP_SERVER_API bool xrtHttpServerNext(xhttpservernext* pNext);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_mux.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_MUX_H
#define XRT_HTTP_SERVER_MUX_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XRT_FEATURE_RWLOCK))
	#error "XRT HTTP server mux requires server router and rwlock support"
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)

typedef struct xhttpservermux xhttpservermux;



/* Mux 查找明确区分错误、未配置、Host 命中和默认站点命中。 */
typedef enum xhttpservermuxstatus {
	XHTTP_SERVER_MUX_ERROR = -1,
	XHTTP_SERVER_MUX_NOT_FOUND = 0,
	XHTTP_SERVER_MUX_HOST = 1,
	XHTTP_SERVER_MUX_DEFAULT = 2
} xhttpservermuxstatus;



/* Mux 错误稳定区分参数、Host、状态、限额、内存、锁和启动阶段。 */
typedef enum xhttpservermuxerror {
	XHTTP_SERVER_MUX_ERROR_ARGUMENT = 1,
	XHTTP_SERVER_MUX_ERROR_HOST,
	XHTTP_SERVER_MUX_ERROR_STATE,
	XHTTP_SERVER_MUX_ERROR_LIMIT,
	XHTTP_SERVER_MUX_ERROR_MEMORY,
	XHTTP_SERVER_MUX_ERROR_LOCK,
	XHTTP_SERVER_MUX_ERROR_CONTEXT,
	XHTTP_SERVER_MUX_ERROR_START,
	XHTTP_SERVER_MUX_ERROR_INTERNAL
} xhttpservermuxerror;



/* 显式限制虚拟主机数量与复制 Host 文本总量。 */
typedef struct xhttpservermuxconfig {
	size_t MaxHosts;
	size_t MaxHostBytes;
} xhttpservermuxconfig;



/* Mux 统计是加读锁取得的一致配置快照。 */
typedef struct xhttpservermuxstats {
	size_t Hosts;
	size_t HostBytes;
	bool HasDefault;
} xhttpservermuxstats;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)

/* 初始化最多 256 个 Host 和 64 KiB Host 文本的默认配置；存储可以未对齐。 */
XHTTP_SERVER_API void xrtHttpServerMuxConfigInit(
	xhttpservermuxconfig* pConfig
);



/* 创建可并发查询并允许运行时替换 Router 的空 Mux；配置被立即快照。 */
XHTTP_SERVER_API xhttpservermux* xrtHttpServerMuxCreate(
	const xhttpservermuxconfig* pConfig
);



/* 增加 Mux 引用；运行中的 Server 会独立持有一份引用。 */
XHTTP_SERVER_API xhttpservermux* xrtHttpServerMuxRef(
	xhttpservermux* pMux
);



/* 释放 Mux 引用；最后一个引用释放全部 Router 与 Host 副本。 */
XHTTP_SERVER_API void xrtHttpServerMuxDestroy(
	xhttpservermux* pMux
);



/* 设置或热替换默认站点；Router 必须已经冻结。 */
XHTTP_SERVER_API bool xrtHttpServerMuxDefault(
	xhttpservermux* pMux,
	xhttpserverrouter* pRouter
);



/*
	设置或热替换一个精确 Host；比较忽略 ASCII 大小写和请求端口。
	Host 使用 HTTP Host 语法但不得包含端口，IPv6 字面量必须使用方括号。
*/
XHTTP_SERVER_API bool xrtHttpServerMuxHost(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter* pRouter
);



/* 移除精确 Host；不存在是正常 NOT_FOUND，不设置错误。 */
XHTTP_SERVER_API xhttpservermuxstatus xrtHttpServerMuxRemove(
	xhttpservermux* pMux,
	xstrview Host
);



/*
	按 Host 查询精确或默认 Router，并通过输出返回一份拥有型引用。
	Host 使用与注册相同的语法；NOT_FOUND 时输出保持为空。
	Router 输出可以位于完整但未对齐的存储中，但不得回绕或覆盖 Mux 与 Host。
*/
XHTTP_SERVER_API xhttpservermuxstatus xrtHttpServerMuxMatch(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
);



/*
	复制 Host 数量、文本字节数和默认站点状态的一致快照。
	输出可以未对齐，但不得回绕或覆盖 Mux 对象。
*/
XHTTP_SERVER_API bool xrtHttpServerMuxStats(
	xhttpservermux* pMux,
	xhttpservermuxstats* pStats
);



/*
	启动按有效请求 Host 选择 Router 的明文 HTTP/1 Server。
	每个请求固定一次 Router 引用，热替换只影响之后开始的请求。
*/
XHTTP_SERVER_API xhttpserver* xrtHttpServerMuxStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_mux_tls.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_MUX_TLS_H
#define XRT_HTTP_SERVER_MUX_TLS_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT HTTP server mux TLS requires server mux and TLS support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)

/* 启动按有效请求 Host 选择 Router 的 TLS HTTP/1 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerMuxStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xrt/http_server_router_tls.h */
/* ========================================================================== */

#ifndef XRT_HTTP_SERVER_ROUTER_TLS_H
#define XRT_HTTP_SERVER_ROUTER_TLS_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS) && \
	(!defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	 !defined(XHTTP_FEATURE_HTTP_SERVER_TLS))
	#error "XRT HTTP server router TLS requires server router and TLS support"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)

/* 启动由冻结 Router 分发的 TLS HTTP/1 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerRouterStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
);

#endif



XRT_EXTERN_C_END

#endif


/* ========================================================================== */
/* public: extlibs/xhttp-server/include/xhttp-server.h */
/* ========================================================================== */

#ifndef XHTTP_SERVER_H
#define XHTTP_SERVER_H



#endif

#endif

#if defined(XHTTP_SERVER_IMPLEMENTATION) && !defined(XHTTP_SERVER_IMPLEMENTATION_ONCE)
#define XHTTP_SERVER_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xhttp_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SSE_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_RAW) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FILE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) || \
	defined(XHTTP_FEATURE_HTTP_ROUTE) || \
	defined(XHTTP_FEATURE_HTTP_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)
#ifndef XHTTP_SERVER_SUPPORT_BRIDGE_H
#define XHTTP_SERVER_SUPPORT_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_internal.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SSE_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_RAW) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FILE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) || \
	defined(XHTTP_FEATURE_HTTP_ROUTE) || \
	defined(XHTTP_FEATURE_HTTP_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)
#ifndef XHTTP_INTERNAL_RUNTIME_H
#define XHTTP_INTERNAL_RUNTIME_H

#if defined(XRT_FEATURE_SPIN)
#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) || \
	defined(XHTTP_FEATURE_HTTP_ROUTE) || \
	defined(XHTTP_FEATURE_HTTP_ROUTER)
#ifndef XHTTP_SERVER_XRT_HTTP_H
#define XHTTP_SERVER_XRT_HTTP_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_body.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)
#ifndef XHTTP_SERVER_XRT_HTTP_BODY_H
#define XHTTP_SERVER_XRT_HTTP_BODY_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_body_file.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_BODY_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)
#ifndef XRT_INTERNAL_HTTP_BODY_FILE_H
#define XRT_INTERNAL_HTTP_BODY_FILE_H




#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 异步文件区间 cursor 只管理一次连续区间，不拥有底层文件。 */
typedef struct xrt_http_body_file_cursor {
	xasyncfile* File;
	xfuture* Pending;
	uint64 Offset;
	uint64 Remaining;
	size_t ReadSize;
	size_t Requested;
	size_t ReadyOffset;
	bool Ready;
} xrt_http_body_file_cursor;



/* 静默关闭同步文件，并保留当前执行上下文正在处理的错误。 */
void __xrtHttpBodyFileCloseSync(xfile File);



/* 请求关闭异步文件，并保留当前执行上下文正在处理的错误。 */
void __xrtHttpBodyFileCloseAsync(xasyncfile* pFile);



/* 初始化一个尚未提交读取的连续文件区间。 */
void __xrtHttpBodyFileCursorInit(
	xrt_http_body_file_cursor* pCursor,
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	size_t iReadSize
);



/* 取消并释放 cursor 尚未消费完的读取 Future。 */
void __xrtHttpBodyFileCursorCancel(
	xrt_http_body_file_cursor* pCursor
);



/* 在 AGAIN 后返回当前读取 Future 的独立消费端引用。 */
xfuture* __xrtHttpBodyFileCursorWait(
	xrt_http_body_file_cursor* pCursor
);



/* 推进连续文件区间并发布拥有独立 Future 租约的 Chunk。 */
xhttpbodystatus __xrtHttpBodyFileCursorNext(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
);



/* 校验文件区间能由跨平台异步文件偏移表达。 */
bool __xrtHttpBodyFileRangeValid(
	uint64 iOffset,
	uint64 iLength
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_semantics.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)
#ifndef XHTTP_SERVER_XRT_HTTP_SEMANTICS_H
#define XHTTP_SERVER_XRT_HTTP_SEMANTICS_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_static.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_PLAN) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_FILE) || \
	defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)
#ifndef XRT_INTERNAL_HTTP_STATIC_H
#define XRT_INTERNAL_HTTP_STATIC_H





#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)
#endif



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)


	#define XRT_HTTP_STATIC_ETAG_CAPACITY 72u



	/* 静态文件对象把可消费文件资源和不可变表示元数据放在同一生命周期内。 */
	struct xhttpstaticfile {
		volatile int32 RefCount;
		xatomicptr File;
		xfileinfo Info;
		xhttprepresentation Representation;
		char ETag[XRT_HTTP_STATIC_ETAG_CAPACITY];
	};
#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_body_stream.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SSE_SERVER)

#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_RAW) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FILE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) || \
	defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)
#ifndef XHTTP_SERVER_XRT_HTTP_SERVER_H
#define XHTTP_SERVER_XRT_HTTP_SERVER_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server_runtime.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_RAW) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_FILE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_STATIC) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)
#ifndef XRT_INTERNAL_HTTP_SERVER_RUNTIME_H
#define XRT_INTERNAL_HTTP_SERVER_RUNTIME_H

#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
#endif
#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
#endif
#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER)

#define XRT_HTTP_SERVER_TIMER_NONE UINT32_C(0)
#define XRT_HTTP_SERVER_TIMER_HEADER UINT32_C(1)
#define XRT_HTTP_SERVER_TIMER_BODY UINT32_C(2)
#define XRT_HTTP_SERVER_TIMER_REQUEST UINT32_C(3)
#define XRT_HTTP_SERVER_TIMER_IDLE UINT32_C(4)
#define XRT_HTTP_SERVER_TIMER_WRITE UINT32_C(5)

#define XRT_HTTP_CONN_TRANSPORT_TCP UINT32_C(0)
#define XRT_HTTP_CONN_TRANSPORT_TLS UINT32_C(1)

#define XRT_HTTP_CONN_GATE_OPEN UINT32_C(0)
#define XRT_HTTP_CONN_GATE_CLOSING UINT32_C(1)
#define XRT_HTTP_CONN_GATE_UPGRADED UINT32_C(2)



typedef struct __xrt_http_response_queue {
	xhttp1serverresponse* Response;
	struct __xrt_http_response_queue* Next;
	bool Information;
} __xrt_http_response_queue;



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
typedef struct __xrt_http_conn_future __xrt_http_conn_future;
typedef struct __xrt_http_server_wait __xrt_http_server_wait;
#endif



/* Server 持有聚合 TCP Server 和全部尚未退出 HTTP 所有权的连接。 */
struct xhttpserver {
	volatile int32 References;
	xatomic32 State;
	xatomic64 Accepted;
	xatomic64 Rejected;
	xatomic64 Requests;
	xatomic64 Responses;
	xatomic64 Informations;
	xatomic64 Upgraded;
	xatomic64 ProtocolErrors;
	xatomic64 Timeouts;
	/* 公开活跃连接数与尚未退出的回调生命周期分别计数。 */
	xatomic64 Connections;
	xatomic64 RuntimeConnections;
	xatomic64 PeakConnections;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		xatomic64 UpgradeCallbacks;
	#endif
	xatomic32 NetworkClosed;
	xatomic32 NetworkPublished;
	xatomicptr NetworkError;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		xatomic32 ShutdownPublished;
	#endif
	xspinlock Lock;
	xnetengine* Engine;
	xnetserver* Network;
	xnetaddr* Locals;
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpconn* Head;
	xhttpconn* Tail;
	size_t EndpointCount;
	size_t ListenerCount;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrt_http_server_wait* WaitHead;
		__xrt_http_server_wait* WaitTail;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		xtlsserverconfig Tls;
		xtlsstreamconfig TlsStream;
		xtlscontext* TlsContext;
		xtlsidentity* TlsIdentity;
		xstrview* TlsProtocols;
	#endif
	bool RuntimeHeld;
	bool Secure;
};



/* Connection 的协议与 I/O 可变状态只在所属 Worker 上推进。 */
struct xhttpconn {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CloseGate;
	xatomic32 FinalGate;
	xatomic32 RequestActive;
	xatomic32 RequestBodyPaused;
	xatomic32 RequestBodyResumePosted;
	xatomic64 Requests;
	xatomic64 RequestWireBytes;
	xatomic64 ResponseWireBytes;
	xatomicptr Error;
	xspinlock TransportLock;
	ptr AdapterData;
	void (*AdapterRelease)(ptr pData);
	xhttpserver* Server;
	xnetworker* Worker;
	xatomicptr Transport;
	xhttp1serverexchange* Exchange;
	xnetpost RequestBodyResumePost;
	xhttp1serverresponse* Response;
	__xrt_http_response_queue* ResponseHead;
	__xrt_http_response_queue* ResponseTail;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrt_http_conn_future* Future;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
		xfuturewatch BodyWatch;
		xfuture* BodyFuture;
		bool BodyWaiting;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		xnetpost UpgradePost;
		xhttpupgradeproc Upgrade;
		ptr UpgradeData;
		ptr UpgradeTransport;
		xerror* UpgradeError;
		xnetresult UpgradeResult;
		size_t UpgradeBuffered;
		bool UpgradePosted;
	#endif
	xnetaddr Local;
	xnetaddr Remote;
	size_t Endpoint;
	xhttpconn* Previous;
	xhttpconn* Next;
	uint64 Timer;
	uint64 WriteDeadline;
	uint32 TimerKind;
	uint32 TransportKind;
	size_t Offered;
	size_t WritePending;
	size_t InformationCount;
	bool OutputQueued;
	bool OutputDriving;
	bool OutputPending;
	bool OutputPosted;
	bool OutputDraining;
	bool ResponseInformation;
	bool AutoContinue;
	bool ForceClose;
	bool InputEnded;
	bool Listed;
	bool Counted;
	bool RuntimeHeld;
	bool CloseNotified;
	bool ErrorNotified;
	bool ServerCloseQueued;
	bool ServerAbortQueued;
};



/* 建立 Server 域错误并保留底层原因。 */
xerror* __xrtHttpServerErrorCreate(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 沿原因链选取最内层有效错误类别。 */
xerrkind __xrtHttpServerCauseKind(
	const xerror* pError,
	xerrkind Fallback
);



/* 设置当前线程的 Server 创建或同步调用错误。 */
void __xrtHttpServerSetError(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 保存 Connection 的第一个稳定错误并发布给当前线程。 */
void __xrtHttpConnRememberError(
	xhttpconn* pConnection,
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 把 Connection 加入 Server 生命周期列表。 */
void __xrtHttpServerAddConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
);



/* 从 Server 的可遍历列表移除 Connection，但暂不归还运行时计数。 */
void __xrtHttpServerRemoveConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
);



/* 在 Connection 的 HTTP 资源和运行时引用退出后归还运行时计数。 */
void __xrtHttpServerConnectionFinished(xhttpserver* pServer);



/* 在发布 Connection Close 前归还公开活跃连接计数。 */
void __xrtHttpServerConnectionClosed(xhttpserver* pServer);



/* 连接和聚合 TCP Server 都退出后发布唯一 Server 关闭终态。 */
void __xrtHttpServerTryFinish(xhttpserver* pServer);



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

/* 摘除并完成 Server 的全部关闭等待。 */
void __xrtHttpServerFutureFinish(xhttpserver* pServer);

#endif



/* 验证配置并创建尚未开始监听的 Server。 */
xhttpserver* __xrtHttpServerCreate(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
);



/* 启动 Server 的 TCP Listener；失败时销毁传入的调用方引用。 */
xhttpserver* __xrtHttpServerListen(xhttpserver* pServer);



/* 检查每个网络端点的 TCP 写入上限都不小于指定值。 */
bool __xrtHttpServerNetworkWriteLimit(
	const xhttpserverconfig* pConfig,
	size_t iMinimum
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 校验并保存 HTTPS Server 使用的 TLS 配置与引用。 */
bool __xrtHttpServerTlsSetup(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
);



/* 释放 HTTPS Server 保存的 TLS 配置与引用。 */
void __xrtHttpServerTlsCleanup(xhttpserver* pServer);

#endif



/* 在所属 Worker 上创建并接管一个已接受的明文 TCP Stream。 */
xhttpconn* __xrtHttpConnCreateTcp(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 在所属 Worker 上创建并接管一个已接受的 TLS Stream。 */
xhttpconn* __xrtHttpConnCreateTls(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
);

#endif



/* 取得 Connection 底层 TCP Stream 的稳定引用。 */
xnetstream* __xrtHttpConnTcpRef(xhttpconn* pConnection);



/* 在所属 Worker 上安装唯一顶层适配器上下文。 */
bool __xrtHttpConnAdapterSet(
	xhttpconn* pConnection,
	ptr pData,
	void (*pRelease)(ptr pData)
);



/* 在 Worker 上、HTTP 运行时退出前归还剩余适配器；Take 后或重复调用为空操作。
 * 最终析构仍作为未启动/失败实例的兜底，不延迟正常运行时清理到公开引用归零。 */
void __xrtHttpConnAdapterRelease(xhttpconn* pConnection);



/* 在所属 Worker 上借用当前顶层适配器上下文。 */
ptr __xrtHttpConnAdapterData(xhttpconn* pConnection);



/* 在所属 Worker 上取走顶层适配器上下文并取消析构责任。 */
ptr __xrtHttpConnAdapterTake(xhttpconn* pConnection);



/* 借用 Connection 当前可读的明文块链。 */
const xnetbuf* __xrtHttpConnBuffer(xhttpconn* pConnection);



/* 消费 Connection 当前明文输入。 */
bool __xrtHttpConnConsume(
	xhttpconn* pConnection,
	size_t iSize
);



/* 暂停明文 TCP 输入；TLS 由明文所有权自动施加背压。 */
void __xrtHttpConnPauseInput(xhttpconn* pConnection);



/* 恢复明文 TCP 输入；TLS 在消费明文后自动恢复。 */
void __xrtHttpConnResumeInput(xhttpconn* pConnection);



/* 恢复协议 Exchange，并用空输入发布已经到达的正文终态。 */
bool __xrtHttpConnResumeExchange(xhttpconn* pConnection);



/* 串行消费 Stream 已经缓冲的输入。 */
void __xrtHttpConnDriveInput(xhttpconn* pConnection);



/* 串行输出当前信息或最终响应。 */
void __xrtHttpConnDriveOutput(xhttpconn* pConnection);



/*
	建立以 Body 作为完整线缆字节的响应计划。
	普通 Raw 与 Upgrade Raw 共用该唯一构造路径。
*/
xhttp1serverresponse* __xrtHttpConnWireResponse(
	xhttpbody* pBody,
	uint64 iLength,
	bool bClose,
	bool bTunnel
);



/* 建立顺序输出一组外部引用的完整线缆响应计划。 */
xhttp1serverresponse* __xrtHttpConnWireRefsResponse(
	const xnetref* pRefs,
	size_t iCount,
	uint64 iLength,
	bool bClose,
	bool bTunnel
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

/* 完成 Tunnel 输出并把传输异步转交给 Upgrade 回调。 */
void __xrtHttpConnUpgradeFinish(xhttpconn* pConnection);



/* 为尚未完成的 Upgrade 安排唯一失败回调。 */
bool __xrtHttpConnUpgradeFail(
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError
);

#endif



/* 检查当前 Worker、连接终态和活动请求是否允许提交响应。 */
bool __xrtHttpConnCanRespond(
	xhttpconn* pConnection,
	cstr sOperation,
	bool bRequireRequest
);



/*
	把已经拥有的响应计划提交给唯一最终响应门。
	无论成功失败，该函数都接管 Response；Upgrade 层可以显式允许 Tunnel 终态。
*/
xnetresult __xrtHttpConnCommitResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	cstr sOperation,
	bool bAllowTunnel
);



/* 发布 Connection 已经保存的稳定错误。 */
void __xrtHttpConnEmitError(xhttpconn* pConnection);



/* 发送自动 100 Continue 并在完成后恢复请求正文。 */
bool __xrtHttpConnContinue(xhttpconn* pConnection);



/* 发送协议错误并强制在响应后关闭。 */
void __xrtHttpConnProtocolFail(
	xhttpconn* pConnection,
	uint16 iStatus,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 为当前协议阶段安装唯一 Timer。 */
bool __xrtHttpConnArmTimer(
	xhttpconn* pConnection,
	uint32 iKind,
	int64 iTimeout
);



/* 取消当前阶段 Timer；引用由最终 Timer 回调释放。 */
void __xrtHttpConnCancelTimer(xhttpconn* pConnection);



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

/* 解除当前响应 Future；关闭、超时和手动响应可以同时请求协作取消。 */
void __xrtHttpConnFutureDetach(
	xhttpconn* pConnection,
	bool bCancel
);

#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)

/* 订阅当前响应正文的可读性 Future。 */
bool __xrtHttpConnBodyWait(xhttpconn* pConnection);



/* 取消并摘除尚未完成的响应正文等待。 */
void __xrtHttpConnBodyStop(
	xhttpconn* pConnection,
	bool bCancel
);

#endif



/* 返回 HTTP Server 使用的 TCP Stream 事件表。 */
const xnetstreamevents* __xrtHttpConnStreamEvents(void);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server_file.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE)
#ifndef XRT_INTERNAL_HTTP_SERVER_FILE_H
#define XRT_INTERNAL_HTTP_SERVER_FILE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server_static.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)
#ifndef XRT_INTERNAL_HTTP_SERVER_STATIC_H
#define XRT_INTERNAL_HTTP_SERVER_STATIC_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_route.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTE) || \
	defined(XHTTP_FEATURE_HTTP_ROUTER)
#ifndef XRT_INTERNAL_HTTP_ROUTE_H
#define XRT_INTERNAL_HTTP_ROUTE_H





#if defined(XHTTP_FEATURE_HTTP_ROUTE)

/* 路由段类别由纯匹配器与预编译 Router 共享。 */
typedef enum xrt_http_route_segment_kind {
	XRT_HTTP_ROUTE_STATIC = 0,
	XRT_HTTP_ROUTE_PARAMETER,
	XRT_HTTP_ROUTE_TAIL
} xrt_http_route_segment_kind;



/* 路由段保留原始文本及去除花括号和尾标记后的参数名。 */
typedef struct xrt_http_route_segment {
	xrt_http_route_segment_kind Kind;
	xstrview Text;
	xstrview Name;
} xrt_http_route_segment;



/* Cursor 使用 Size + 1 表示结束，从而保留根路径之外的尾空段。 */
typedef struct xrt_http_route_cursor {
	xstrview Text;
	size_t Position;
} xrt_http_route_cursor;



/* 初始化严格路径段游标。 */
void __xrtHttpRouteCursorInit(
	xrt_http_route_cursor* pCursor,
	xstrview Text
);



/* 读取下一个严格路径段。 */
bool __xrtHttpRouteCursorNext(
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
);



/* 解析一个已经切分的模板段。 */
bool __xrtHttpRouteSegmentParse(
	xstrview Text,
	bool bLast,
	xrt_http_route_segment* pSegment
);



/* 验证不含 query 或 fragment 的绝对 RFC 3986 路径。 */
bool __xrtHttpRoutePathValid(xstrview Path);



/* 匹配已经验证的模板和路径；调用方保证捕获容量充足。 */
xhttproutestatus __xrtHttpRouteMatchValidated(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_router.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTER)
#ifndef XRT_INTERNAL_HTTP_ROUTER_H
#define XRT_INTERNAL_HTTP_ROUTER_H





#if defined(XHTTP_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_NONE SIZE_MAX

#define XRT_HTTP_ROUTER_PARENT_STATIC ((uint8)1u)
#define XRT_HTTP_ROUTER_PARENT_PARAMETER ((uint8)2u)



/* 注册期静态边使用父节点链表，冻结后转换为节点连续排序区间。 */
typedef struct xrt_http_router_build_edge {
	size_t Parent;
	size_t Child;
	size_t Text;
	size_t Size;
	size_t Next;
} xrt_http_router_build_edge;



/* 冻结静态边只保留二分查找需要的文本和子节点。 */
typedef struct xrt_http_router_edge {
	size_t Child;
	size_t Text;
	size_t Size;
} xrt_http_router_edge;



/* 节点同时保留注册图、冻结区间和无栈回溯需要的父关系。 */
typedef struct xrt_http_router_node {
	size_t Parent;
	size_t Depth;
	size_t BuildEdge;
	size_t Parameter;
	size_t Tail;
	size_t Route;
	size_t EdgeStart;
	size_t EdgeCount;
	size_t EdgeWrite;
	uint8 ParentKind;
} xrt_http_router_node;



/* 路由记录用 Arena 偏移持有稳定方法和模板，并借用用户 Value。 */
typedef struct xrt_http_router_route {
	size_t Next;
	size_t Method;
	size_t MethodSize;
	size_t Pattern;
	size_t PatternSize;
	size_t Parameters;
	ptr Value;
} xrt_http_router_route;



/* Router 在冻结前持有 BuildEdges，冻结后持有排序 Edges。 */
struct xhttprouter {
	xhttprouterconfig Config;
	xrt_http_router_node* Nodes;
	size_t NodeCount;
	size_t NodeCapacity;
	xrt_http_router_build_edge* BuildEdges;
	size_t BuildEdgeCount;
	size_t BuildEdgeCapacity;
	xrt_http_router_edge* Edges;
	xrt_http_router_route* Routes;
	size_t RouteCount;
	size_t RouteCapacity;
	char* Arena;
	size_t ArenaSize;
	size_t ArenaCapacity;
	bool Frozen;
};



/* 返回 Arena 中的借用文本。 */
static inline xstrview __xrtHttpRouterView(
	const xhttprouter* pRouter,
	size_t iOffset,
	size_t iSize
)
{
	return (xstrview){ pRouter->Arena + iOffset, iSize };
}

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server_router.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)
#ifndef XRT_INTERNAL_HTTP_SERVER_ROUTER_H
#define XRT_INTERNAL_HTTP_SERVER_ROUTER_H


#include <string.h>


#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
#endif



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

#define XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS ((size_t)8u)



/* 路由记录与通用 Router 的 Value 索引一一对应。 */
typedef struct xrt_http_server_route_entry {
	xhttpserverrouteevents Events;
} xrt_http_server_route_entry;



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 中间件记录复制回调和用户数据，并可在 Router 终态释放拥有型数据。 */
typedef struct xrt_http_server_middleware_entry {
	xhttpservermiddlewareproc Handle;
	xhttpserverrouterreleaseproc Release;
	ptr Data;
} xrt_http_server_middleware_entry;

#endif



/* 运行时适配器持有 Router 引用，并保存调用方原始事件和 Data。 */
typedef struct xrt_http_server_router_runtime {
	xhttpserverrouter* Router;
	xhttpserverevents Events;
} xrt_http_server_router_runtime;



/* 服务端 Router 在冻结前拥有可增长回调表，冻结后只读共享。 */
struct xhttpserverrouter {
	volatile int32 References;
	xhttprouter* Index;
	xrt_http_server_route_entry* Entries;
	size_t Count;
	size_t Capacity;
	size_t MaxRoutes;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		xrt_http_server_middleware_entry* Middleware;
		size_t MiddlewareCount;
		size_t MiddlewareCapacity;
	#endif
};



/* 一次分发结果使用八个栈上参数，极端模板才按精确数量分配。 */
typedef struct xrt_http_server_route_match {
	xhttprouterstatus Status;
	const xrt_http_server_route_entry* Entry;
	xhttprouteparam* Params;
	size_t Count;
	xstrview Method;
	xstrview Path;
	xhttprouteparam Local[XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS];
} xrt_http_server_route_match;



/* 设置带操作名和可选原因链的稳定服务端路由错误。 */
void __xrtHttpServerRouterSetError(
	xerrkind Kind,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 原因链缺失或未分类时返回调用点给出的保守错误种类。 */
xerrkind __xrtHttpServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
);



/* 把当前线程错误保留为原因，并发布服务端 Router 顶层错误。 */
void __xrtHttpServerRouterWrapError(
	xerrkind Default,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage
);



/* 建立持有 Router 引用的完整 Server 事件适配器。 */
xrt_http_server_router_runtime* __xrtHttpServerRouterRuntimeCreate(
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
);



/* 释放尚未交给运行中 Server 或已经完成 Shutdown 的适配器。 */
void __xrtHttpServerRouterRuntimeDestroy(
	xrt_http_server_router_runtime* pRuntime
);



/* 使用指定 Router 和回退事件处理 Header 阶段，供固定路由与 Host Mux 共用。 */
xhttpserverbodypolicy __xrtHttpServerRouterDispatchHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match** ppMatch
);



/* 使用指定 Router 和回退事件处理一个流式正文片段。 */
bool __xrtHttpServerRouterDispatchBody(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pMatch
);



/* 使用指定 Router 和回退事件处理完整请求。 */
void __xrtHttpServerRouterDispatchRequest(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pMatch
);



/* 报告 Router 适配失败并尽力提交固定 500 响应。 */
void __xrtHttpServerRouterDispatchFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverevents* pEvents
);



/* 执行已经匹配的最终路由、用户回退或标准响应，不再次进入中间件。 */
bool __xrtHttpServerRouterDispatchTerminal(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
);



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 用冻结的中间件链包裹一次已经匹配的完整请求终端分派。 */
bool __xrtHttpServerMiddlewareDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
);



/* 按洋葱逆序释放 Router 拥有的中间件数据和记录数组。 */
void __xrtHttpServerMiddlewareClear(xhttpserverrouter* pRouter);

#endif



/* 解析请求目标并执行容量完整的 Router 匹配。 */
bool __xrtHttpServerRouterMatch(
	const xhttpserverrouter* pRouter,
	const xhttpserverrequest* pRequest,
	xrt_http_server_route_match* pMatch
);



/* 释放极端参数模板使用的临时描述符数组。 */
void __xrtHttpServerRouterMatchClear(
	xrt_http_server_route_match* pMatch
);



/* 把栈上匹配结果转移为流式请求期拥有对象。 */
xrt_http_server_route_match* __xrtHttpServerRouterMatchTake(
	xrt_http_server_route_match* pMatch
);



/* 释放流式请求期匹配结果。 */
void __xrtHttpServerRouterMatchDestroy(ptr pData);



/* 为 404、405 和自动 OPTIONS 提交默认响应。 */
bool __xrtHttpServerRouterDefault(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	const xrt_http_server_route_match* pMatch
);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xhttp-server/src/internal/xrt_http_server_mux.h */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX) || \
	defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)
#ifndef XRT_INTERNAL_HTTP_SERVER_MUX_H
#define XRT_INTERNAL_HTTP_SERVER_MUX_H





#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)

/* Host 表按 ASCII 不区分大小写排序，Entry 独立持有文本和 Router 引用。 */
typedef struct xrt_http_server_mux_entry {
	str Host;
	size_t Size;
	xhttpserverrouter* Router;
} xrt_http_server_mux_entry;



/* Mux 用读写锁保护热替换表，普通 Router 本身继续保持冻结只读。 */
struct xhttpservermux {
	volatile int32 References;
	xrwlock Lock;
	xhttpservermuxconfig Config;
	xrt_http_server_mux_entry* Entries;
	size_t Count;
	size_t Capacity;
	size_t HostBytes;
	xhttpserverrouter* Default;
};



/* 每个 Mux Connection 固定当前请求选择的 Router，直到下一请求或关闭。 */
typedef struct xrt_http_server_mux_connection {
	xhttpserverrouter* Router;
	xrt_http_server_route_match* Match;
} xrt_http_server_mux_connection;



/* Server 适配器独立持有 Mux，并保存调用方原始事件。 */
typedef struct xrt_http_server_mux_runtime {
	xhttpservermux* Mux;
	xhttpserverevents Events;
} xrt_http_server_mux_runtime;



/* 设置稳定 Mux 错误及可选原因链。 */
void __xrtHttpServerMuxSetError(
	xerrkind Kind,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 包装当前线程错误并保留原始原因链。 */
void __xrtHttpServerMuxWrapError(
	xerrkind Default,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 使用已解析的非空 Host 选择并保留精确或默认 Router。 */
xhttpservermuxstatus __xrtHttpServerMuxSelect(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
);



/* 建立供明文和 TLS Start 共用的 Server 事件适配器。 */
xrt_http_server_mux_runtime* __xrtHttpServerMuxRuntimeCreate(
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
);



/* 释放尚未交给 Server 或已经完成 Shutdown 的适配器。 */
void __xrtHttpServerMuxRuntimeDestroy(
	xrt_http_server_mux_runtime* pRuntime
);

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_static_path.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)



#if defined(XHTTP_FEATURE_HTTP_STATIC_PATH)

/* 第一次扫描保留路由匹配与相对路径起点，不持有任何输入。 */
typedef struct xrt_http_static_path_info {
	size_t RelativeOffset;
	bool Matched;
	bool TrailingSlash;
} xrt_http_static_path_info;



/* 内部写入器同时服务精确长度查询和已经验证容量的实际写出。 */
typedef struct xrt_http_static_path_writer {
	char* Data;
	size_t Size;
} xrt_http_static_path_writer;



/* 判断一个完整路径段是否是点或双点。 */
static bool __xrtHttpStaticPathDot(
	size_t iSize,
	uint8 iFirst,
	uint8 iLast
)
{
	return ((iSize == 1u) && (iFirst == (uint8)'.')) ||
		((iSize == 2u) && (iFirst == (uint8)'.') &&
		 (iLast == (uint8)'.'));
}



/* 验证挂载点是已解码、无空段且不含查询或片段的规范 origin 路径。 */
static bool __xrtHttpStaticPathMountValid(
	xstrview Mount
)
{
	size_t iSegmentSize = 0;
	uint8 iFirst = 0;
	uint8 iLast = 0;
	size_t i;

	if ( !xrtMemRangeValid(Mount.Data, Mount.Size) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( (Mount.Data == NULL) || (Mount.Size == 0) ||
		(Mount.Data[0] != '/') ||
		((Mount.Size != 1u) &&
		 (Mount.Data[Mount.Size - 1u] == '/')) ||
		!xrtUtf8Valid(Mount, NULL) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	for ( i = 1; i < Mount.Size; i++ ) {
		uint8 iValue = (uint8)Mount.Data[i];

		if ( iValue == (uint8)'/' ) {
			if ( (iSegmentSize == 0) ||
				__xrtHttpStaticPathDot(
					iSegmentSize,
					iFirst,
					iLast
				) ) {
				xrtSetErrorKind(XERR_VALUE);
				return false;
			}
			iSegmentSize = 0;
			continue;
		}
		if ( (iValue < 0x20u) || (iValue == 0x7Fu) ||
			(iValue == (uint8)'\\') ||
			(iValue == (uint8)'?') ||
			(iValue == (uint8)'#') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( iSegmentSize == 0 ) {
			iFirst = iValue;
		}
		iLast = iValue;
		iSegmentSize++;
	}
	if ( (Mount.Size != 1u) &&
		((iSegmentSize == 0) ||
		 __xrtHttpStaticPathDot(
			iSegmentSize,
			iFirst,
			iLast
		 )) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/* 解析默认配置并拒绝未知策略位。 */
static bool __xrtHttpStaticPathConfigResolve(
	const xhttpstaticpathconfig* pInput,
	xhttpstaticpathconfig* pConfig
)
{
	*pConfig = (xhttpstaticpathconfig){
		XRT_STR_LITERAL("/"),
		XHTTP_STATIC_PATH_PORTABLE
	};
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	}
	if ( (pConfig->Flags &
		~((uint32)XHTTP_STATIC_PATH_PORTABLE |
		  (uint32)XHTTP_STATIC_PATH_ALLOW_HIDDEN)) != 0u ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return __xrtHttpStaticPathMountValid(pConfig->Mount);
}



/* 完成一个 origin 路径段的空段与点段检查。 */
static bool __xrtHttpStaticPathSegmentFinish(
	size_t iSize,
	uint8 iFirst,
	uint8 iLast
)
{
	if ( (iSize == 0) ||
		__xrtHttpStaticPathDot(iSize, iFirst, iLast) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/*
	严格扫描原始 URL path，验证 percent、UTF-8 和结构，
	同时按解码后的字节匹配挂载点。
*/
static bool __xrtHttpStaticPathInspect(
	xstrview RawPath,
	xstrview Mount,
	xrt_http_static_path_info* pInfo
)
{
	xutf8state Utf8;
	size_t iRaw = 0;
	size_t iDecoded = 0;
	size_t iSegmentSize = 0;
	size_t iRootRelative = 0;
	size_t iBoundaryRelative = 0;
	uint8 iFirst = 0;
	uint8 iLast = 0;
	bool bPrefix = true;
	bool bBoundary = false;

	memset(pInfo, 0, sizeof(*pInfo));
	if ( (RawPath.Data == NULL) || (RawPath.Size == 0) ||
		(RawPath.Data[0] != '/') ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	xrtUtf8StateInit(&Utf8);
	while ( iRaw < RawPath.Size ) {
		size_t iBefore = iRaw;
		uint8 iValue;
		char iText;
		xutfstatus UtfStatus;
		xpercentnext Next;

		if ( (RawPath.Data[iBefore] == '?') ||
			(RawPath.Data[iBefore] == '#') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		Next = xrtPercentNext(
			RawPath,
			false,
			&iRaw,
			&iValue
		);
		if ( Next == XPERCENT_NEXT_ERROR ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( Next == XPERCENT_NEXT_END ) {
			break;
		}

		/* percent 编码不得改变路径分段结构。 */
		if ( ((RawPath.Data[iBefore] == '%') &&
			 (iValue == (uint8)'/')) ||
			(iValue == 0) || (iValue < 0x20u) ||
			(iValue == 0x7Fu) || (iValue == (uint8)'\\') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		iText = (char)iValue;
		UtfStatus = xrtUtf8StateFeed(
			&Utf8,
			(xstrview){ &iText, 1u },
			false
		);
		if ( (UtfStatus != XUTF_OK) &&
			(UtfStatus != XUTF_MORE) ) {
			if ( UtfStatus != XUTF_OVERFLOW ) {
				xrtSetErrorKind(XERR_VALUE);
			}
			return false;
		}

		/* 挂载点按解码后的 origin 路径逐字节区分大小写匹配。 */
		if ( iDecoded < Mount.Size ) {
			if ( iValue != (uint8)Mount.Data[iDecoded] ) {
				bPrefix = false;
			}
		} else if ( iDecoded == Mount.Size ) {
			bBoundary = iValue == (uint8)'/';
			if ( bBoundary ) {
				iBoundaryRelative = iRaw;
			}
		}
		if ( iDecoded == 0 ) {
			if ( iValue != (uint8)'/' ) {
				xrtSetErrorKind(XERR_VALUE);
				return false;
			}
			iRootRelative = iRaw;
		} else if ( iValue == (uint8)'/' ) {
			if ( !__xrtHttpStaticPathSegmentFinish(
				iSegmentSize,
				iFirst,
				iLast
			) ) {
				return false;
			}
			iSegmentSize = 0;
		} else {
			if ( iSegmentSize == SIZE_MAX ) {
				xrtSetErrorKind(XERR_RANGE);
				return false;
			}
			if ( iSegmentSize == 0 ) {
				iFirst = iValue;
			}
			iLast = iValue;
			iSegmentSize++;
		}
		iDecoded++;
	}

	if ( (iSegmentSize != 0) &&
		!__xrtHttpStaticPathSegmentFinish(
			iSegmentSize,
			iFirst,
			iLast
		) ) {
		return false;
	}
	if ( xrtUtf8StateFeed(
		&Utf8,
		(xstrview){ NULL, 0 },
		true
	) != XUTF_OK ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	pInfo->TrailingSlash = iSegmentSize == 0;

	/* 根挂载省略唯一的前导斜杠，其余挂载要求精确命中或段边界。 */
	if ( Mount.Size == 1u ) {
		pInfo->Matched = bPrefix;
		pInfo->RelativeOffset = iRootRelative;
	} else if ( bPrefix && (iDecoded == Mount.Size) ) {
		pInfo->Matched = true;
		pInfo->RelativeOffset = RawPath.Size;
	} else if ( bPrefix && (iDecoded > Mount.Size) && bBoundary ) {
		pInfo->Matched = true;
		pInfo->RelativeOffset = iBoundaryRelative;
	}
	return true;
}



/* 向可空的内部写入器追加一个字节并检查长度溢出。 */
static bool __xrtHttpStaticPathAppend(
	xrt_http_static_path_writer* pWriter,
	uint8 iValue
)
{
	if ( pWriter->Size == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( pWriter->Data != NULL ) {
		pWriter->Data[pWriter->Size] = (char)iValue;
	}
	pWriter->Size++;
	return true;
}



/* 完成映射段的隐藏策略和可移植路径规则。 */
static bool __xrtHttpStaticPathMappedSegmentFinish(
	const xpathsafesegment* pPortable,
	bool bPortable
)
{
	if ( bPortable &&
		!xrtPathSafeSegmentFinish(pPortable) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



/*
	把已经完成协议扫描的相对后缀解码为文件路径。
	该过程既可只计算长度，也可在第二遍直接写出。
*/
static bool __xrtHttpStaticPathMapBody(
	xstrview Relative,
	uint32 iFlags,
	xrt_http_static_path_writer* pWriter
)
{
	xpathsafesegment Portable;
	size_t iRaw = 0;
	size_t iSegmentSize = 0;
	bool bPortable =
		(iFlags & XHTTP_STATIC_PATH_PORTABLE) != 0u;
	bool bAllowHidden =
		(iFlags & XHTTP_STATIC_PATH_ALLOW_HIDDEN) != 0u;
	xpercentnext Next;

	if ( Relative.Size == 0 ) {
		return __xrtHttpStaticPathAppend(
			pWriter,
			(uint8)'.'
		);
	}
	xrtPathSafeSegmentInit(&Portable);
	do {
		uint8 iValue;

		Next = xrtPercentNext(
			Relative,
			false,
			&iRaw,
			&iValue
		);
		if ( Next == XPERCENT_NEXT_ERROR ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( Next == XPERCENT_NEXT_END ) {
			break;
		}
		if ( iValue == (uint8)'/' ) {
			if ( !__xrtHttpStaticPathMappedSegmentFinish(
				&Portable,
				bPortable
			) ) {
				return false;
			}
			if ( iRaw == Relative.Size ) {
				iSegmentSize = 0;
				break;
			}
			if ( !__xrtHttpStaticPathAppend(
				pWriter,
				(uint8)'/'
			) ) {
				return false;
			}
			iSegmentSize = 0;
			xrtPathSafeSegmentInit(&Portable);
			continue;
		}
		if ( (iSegmentSize == 0) &&
			!bAllowHidden &&
			(iValue == (uint8)'.') ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( bPortable &&
			!xrtPathSafeSegmentFeed(
				&Portable,
				iValue
			) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
		if ( !__xrtHttpStaticPathAppend(
			pWriter,
			iValue
		) ) {
			return false;
		}
		iSegmentSize++;
	} while ( Next == XPERCENT_NEXT_BYTE );

	if ( (iSegmentSize != 0) &&
		!__xrtHttpStaticPathMappedSegmentFinish(
			&Portable,
			bPortable
		) ) {
		return false;
	}
	return true;
}



/* 检查两个标量输出不会覆盖请求路径、配置或彼此。 */
static bool __xrtHttpStaticPathMetadataAliases(
	size_t* pSize,
	bool* pTrailingSlash,
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	xstrview Mount
)
{
	if ( xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			RawPath.Data,
			RawPath.Size
		) || xrtMemRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			RawPath.Data,
			RawPath.Size
		) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			Mount.Data,
			Mount.Size
		) || xrtMemRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			Mount.Data,
			Mount.Size
		) || ((pInput != NULL) &&
		 (xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pInput,
			sizeof(*pInput)
		 ) || xrtMemRangesOverlap(
			pTrailingSlash,
			sizeof(*pTrailingSlash),
			pInput,
			sizeof(*pInput)
		 ))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 检查实际文本输出不会覆盖任何借用输入或标量输出。 */
static bool __xrtHttpStaticPathOutputAliases(
	char* sOutput,
	size_t iWriteSize,
	size_t* pSize,
	bool* pTrailingSlash,
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	xstrview Mount
)
{
	if ( xrtMemRangesOverlap(
			sOutput,
			iWriteSize,
			pSize,
			sizeof(*pSize)
		) || xrtMemRangesOverlap(
			sOutput,
			iWriteSize,
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || xrtMemRangesOverlap(
			sOutput,
			iWriteSize,
			RawPath.Data,
			RawPath.Size
		) || xrtMemRangesOverlap(
			sOutput,
			iWriteSize,
			Mount.Data,
			Mount.Size
		) || ((pInput != NULL) && xrtMemRangesOverlap(
			sOutput,
			iWriteSize,
			pInput,
			sizeof(*pInput)
		)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 初始化静态路径映射的安全默认策略。 */
XHTTP_SERVER_API void xrtHttpStaticPathConfigInit(
	xhttpstaticpathconfig* pConfig
)
{
	xhttpstaticpathconfig Config = {
		XRT_STR_LITERAL("/"),
		XHTTP_STATIC_PATH_PORTABLE
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 无分配地查询或写出静态资源相对路径。 */
static xhttpstaticpathstatus __xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pInput,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
)
{
	xrt_http_static_path_info Info;
	xrt_http_static_path_writer Writer;
	xstrview Relative;
	size_t iWriteSize;
	size_t iResultSize;
	bool bResultTrailing;

	if ( !__xrtHttpStaticPathMetadataAliases(
		pSize,
		pTrailingSlash,
		RawPath,
		pInput,
		pConfig->Mount
	) || !__xrtHttpStaticPathInspect(
		RawPath,
		pConfig->Mount,
		&Info
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	if ( !Info.Matched ) {
		iResultSize = 0;
		bResultTrailing = false;
		memcpy((void*)pSize, &iResultSize, sizeof(iResultSize));
		memcpy(
			pTrailingSlash,
			&bResultTrailing,
			sizeof(bResultTrailing)
		);
		return XHTTP_STATIC_PATH_NO_MATCH;
	}
	Relative = (xstrview){
		RawPath.Data + Info.RelativeOffset,
		RawPath.Size - Info.RelativeOffset
	};
	Writer = (xrt_http_static_path_writer){ NULL, 0 };
	if ( !__xrtHttpStaticPathMapBody(
		Relative,
		pConfig->Flags,
		&Writer
	) || (Writer.Size == SIZE_MAX) ) {
		if ( Writer.Size == SIZE_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
		}
		return XHTTP_STATIC_PATH_ERROR;
	}
	iWriteSize = Writer.Size + 1u;
	if ( (sOutput != NULL) &&
		!__xrtHttpStaticPathOutputAliases(
			sOutput,
			iWriteSize,
			pSize,
			pTrailingSlash,
			RawPath,
			pInput,
			pConfig->Mount
		) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	iResultSize = Writer.Size;
	bResultTrailing = Info.TrailingSlash;
	memcpy((void*)pSize, &iResultSize, sizeof(iResultSize));
	memcpy(
		pTrailingSlash,
		&bResultTrailing,
		sizeof(bResultTrailing)
	);
	if ( sOutput == NULL ) {
		return XHTTP_STATIC_PATH_MATCH;
	}
	if ( iCapacity < iWriteSize ) {
		xrtSetErrorKind(XERR_RANGE);
		return XHTTP_STATIC_PATH_ERROR;
	}
	Writer.Data = sOutput;
	Writer.Size = 0;
	if ( !__xrtHttpStaticPathMapBody(
		Relative,
		pConfig->Flags,
		&Writer
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	sOutput[Writer.Size] = 0;
	return XHTTP_STATIC_PATH_MATCH;
}



/* 无分配地查询或写出静态资源相对路径。 */
XHTTP_SERVER_API xhttpstaticpathstatus xrtHttpStaticPathWrite(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize,
	bool* pTrailingSlash
)
{
	xhttpstaticpathconfig Config;

	if ( !xrtMemRangeValid(RawPath.Data, RawPath.Size) ||
		!xrtMemRangeValid(pSize, sizeof(*pSize)) ||
		!xrtMemRangeValid(
			pTrailingSlash,
			sizeof(*pTrailingSlash)
		) || !xrtMemRangeValid(sOutput, iCapacity) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_STATIC_PATH_ERROR;
	}
	if ( !__xrtHttpStaticPathConfigResolve(
		pConfig,
		&Config
	) ) {
		return XHTTP_STATIC_PATH_ERROR;
	}
	return __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		sOutput,
		iCapacity,
		pSize,
		pTrailingSlash
	);
}



/* 初始化拥有型静态路径结果。 */
XHTTP_SERVER_API void xrtHttpStaticPathInit(
	xhttpstaticpath* pPath
)
{
	xhttpstaticpath Path = { 0 };

	if ( !xrtMemRangeValid(pPath, sizeof(Path)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy((void*)pPath, &Path, sizeof(Path));
}



/* 分配并构建静态资源相对路径。 */
XHTTP_SERVER_API bool xrtHttpStaticPathMap(
	xstrview RawPath,
	const xhttpstaticpathconfig* pConfig,
	xhttpstaticpath* pPath
)
{
	xhttpstaticpathconfig Config;
	xhttpstaticpath Current;
	xhttpstaticpath Result = { 0 };
	xhttpstaticpathstatus Status;
	size_t iSize;
	bool bTrailingSlash;
	str sPath;

	if ( !xrtMemRangeValid(RawPath.Data, RawPath.Size) ||
		!xrtMemRangeValid(pPath, sizeof(Current)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpStaticPathConfigResolve(
		pConfig,
		&Config
	) ) {
		return false;
	}
	memcpy(&Current, (const void*)pPath, sizeof(Current));
	if ( (Current.Path != NULL) ||
		(Current.Size != 0) ||
		Current.Matched ||
		Current.TrailingSlash ||
		xrtMemRangesOverlap(
			pPath,
			sizeof(Current),
			RawPath.Data,
			RawPath.Size
		) || ((pConfig != NULL) &&
		 (xrtMemRangesOverlap(
			pPath,
			sizeof(Current),
			pConfig,
			sizeof(*pConfig)
		 ) || xrtMemRangesOverlap(
			pPath,
			sizeof(Current),
			Config.Mount.Data,
			Config.Mount.Size
		 ))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Status = __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		NULL,
		0,
		&iSize,
		&bTrailingSlash
	);
	if ( Status == XHTTP_STATIC_PATH_ERROR ) {
		return false;
	}
	if ( Status == XHTTP_STATIC_PATH_NO_MATCH ) {
		return true;
	}
	if ( iSize == SIZE_MAX ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	sPath = (str)xrtMalloc(iSize + 1u);
	if ( sPath == NULL ) {
		return false;
	}
	Status = __xrtHttpStaticPathWrite(
		RawPath,
		pConfig,
		&Config,
		sPath,
		iSize + 1u,
		&iSize,
		&bTrailingSlash
	);
	if ( Status != XHTTP_STATIC_PATH_MATCH ) {
		xrtFree(sPath);
		return false;
	}
	Result.Path = sPath;
	Result.Size = iSize;
	Result.Matched = true;
	Result.TrailingSlash = bTrailingSlash;
	memcpy((void*)pPath, &Result, sizeof(Result));
	return true;
}



/* 释放并清空拥有型静态路径。 */
XHTTP_SERVER_API void xrtHttpStaticPathFree(
	xhttpstaticpath* pPath
)
{
	xhttpstaticpath Path;
	xhttpstaticpath Empty = { 0 };

	if ( pPath == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pPath, sizeof(Path)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(&Path, (const void*)pPath, sizeof(Path));
	xrtFree(Path.Path);
	memcpy((void*)pPath, &Empty, sizeof(Empty));
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_static_plan.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)



#if defined(XHTTP_FEATURE_HTTP_STATIC_PLAN)

/* Range 扫描区分缺失、可处理和应被忽略三种结果。 */
typedef enum xrt_http_static_range_state {
	XRT_HTTP_STATIC_RANGE_NONE = 0,
	XRT_HTTP_STATIC_RANGE_READY,
	XRT_HTTP_STATIC_RANGE_IGNORE
} xrt_http_static_range_state;



/* 初始化静态响应计划的有界默认策略。 */
XHTTP_SERVER_API void xrtHttpStaticPlanConfigInit(
	xhttpstaticplanconfig* pConfig
)
{
	xhttpstaticplanconfig Config = { 16u, 0 };

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 解析调用方配置并验证范围数组大小计算不会溢出。 */
static bool __xrtHttpStaticPlanConfigResolve(
	const xhttpstaticplanconfig* pInput,
	size_t iRangeCapacity,
	xhttpstaticplanconfig* pConfig,
	size_t* pRangeBytes
)
{
	*pConfig = (xhttpstaticplanconfig){ 16u, 0 };
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return false;
		}
		memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	}
	if ( (pConfig->MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(iRangeCapacity >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ||
		(iRangeCapacity < pConfig->MaxRanges) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	*pRangeBytes = iRangeCapacity * sizeof(xhttpbyterange);
	return true;
}



/* 判断一个输出区间是否覆盖任一借用请求输入。 */
static bool __xrtHttpStaticPlanOutputAliases(
	const void* pOutput,
	size_t iOutputSize,
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrentInput,
	const xhttprepresentation* pCurrent,
	const xhttpstaticplanconfig* pConfig
)
{
	xhttpfield Field;
	size_t iFieldBytes = iFieldCount * sizeof(xhttpfield);
	size_t i;

	if ( xrtMemRangesOverlap(
			pOutput, iOutputSize, Method.Data, Method.Size
		) || xrtMemRangesOverlap(
			pOutput, iOutputSize, pFields, iFieldBytes
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrentInput,
			sizeof(*pCurrentInput)
		) || ((pConfig != NULL) && xrtMemRangesOverlap(
			pOutput, iOutputSize, pConfig, sizeof(*pConfig)
		)) || (pCurrent->HasETag && xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrent->ETag.Opaque.Data,
			pCurrent->ETag.Opaque.Size
		)) ) {
		return true;
	}
	for ( i = 0; i < iFieldCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( xrtMemRangesOverlap(
				pOutput,
				iOutputSize,
				Field.Name.Data,
				Field.Name.Size
			) || xrtMemRangesOverlap(
				pOutput,
				iOutputSize,
				Field.Value.Data,
				Field.Value.Size
			) ) {
			return true;
		}
	}
	return false;
}



/* 验证两个输出与全部输入相互独立。 */
static bool __xrtHttpStaticPlanAliasesValid(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrentInput,
	const xhttprepresentation* pCurrent,
	xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
)
{
	if ( __xrtHttpStaticPlanOutputAliases(
			pPlan,
			sizeof(*pPlan),
			Method,
			pFields,
			iFieldCount,
			pCurrentInput,
			pCurrent,
			pConfig
		) || __xrtHttpStaticPlanOutputAliases(
			pRanges,
			iRangeBytes,
			Method,
			pFields,
			iFieldCount,
			pCurrentInput,
			pCurrent,
			pConfig
		) || xrtMemRangesOverlap(
			pPlan,
			sizeof(*pPlan),
			pRanges,
			iRangeBytes
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 查找唯一 Range 字段并完整验证它的单位、语法和工作上限。 */
static xrt_http_static_range_state __xrtHttpStaticRangeRead(
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t iMaximum,
	xstrview* pSet
)
{
	xhttpfield Field;
	xhttpfield Range;
	xhttprangespec Spec;
	xhttpnext Next;
	xstrview Unit;
	xstrview Set;
	size_t iOffset = 0;
	size_t iCount = 0;
	size_t i;
	bool bRange = false;

	*pSet = (xstrview){ NULL, 0 };
	for ( i = 0; i < iFieldCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("Range")
		) ) {
			continue;
		}
		if ( bRange ) {
			return XRT_HTTP_STATIC_RANGE_IGNORE;
		}
		Range = Field;
		bRange = true;
	}
	if ( !bRange ) {
		return XRT_HTTP_STATIC_RANGE_NONE;
	}
	if ( (iMaximum == 0) ||
		!xrtHttpBridgeRangeParseValue(
			Range.Value, &Unit, &Set
		) || !xrtHttpTokenEqual(
			Unit, XRT_STR_LITERAL("bytes")
		) ) {
		return XRT_HTTP_STATIC_RANGE_IGNORE;
	}
	do {
		Next = xrtHttpBridgeByteRangeNextValue(
			Set, &iOffset, &Spec
		);
		if ( Next == XHTTP_NEXT_ERROR ) {
			return XRT_HTTP_STATIC_RANGE_IGNORE;
		}
		if ( Next == XHTTP_NEXT_ITEM ) {
			iCount++;
			if ( iCount > iMaximum ) {
				return XRT_HTTP_STATIC_RANGE_IGNORE;
			}
		}
	} while ( Next == XHTTP_NEXT_ITEM );
	if ( iCount == 0 ) {
		return XRT_HTTP_STATIC_RANGE_IGNORE;
	}
	*pSet = Set;
	return XRT_HTTP_STATIC_RANGE_READY;
}



/* 只有唯一且匹配的 If-Range 才允许继续处理 Range。 */
static bool __xrtHttpStaticIfRangeAllows(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent
)
{
	xhttpfield Field;
	xstrview Value = { NULL, 0 };
	size_t i;
	bool bIfRange = false;

	for ( i = 0; i < iFieldCount; i++ ) {
		__xhttpFieldLoad(pFields, i, &Field);
		if ( !xrtHttpFieldNameEqual(
			Field.Name,
			XRT_STR_LITERAL("If-Range")
		) ) {
			continue;
		}
		if ( bIfRange ) {
			return false;
		}
		Value = Field.Value;
		bIfRange = true;
	}
	return !bIfRange || xrtHttpIfRangeMatch(Value, pCurrent);
}



/* 原子发布一个已经完整构建的静态响应计划。 */
static void __xrtHttpStaticPlanPublish(
	xhttpstaticplan* pOutput,
	const xhttpstaticplan* pPlan
)
{
	memcpy((void*)pOutput, pPlan, sizeof(*pPlan));
}



/* 评估静态表示的条件请求、方法和字节范围。 */
XHTTP_SERVER_API bool xrtHttpStaticPlanBuild(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iRangeCapacity,
	const xhttpstaticplanconfig* pConfig,
	xhttpstaticplan* pPlan
)
{
	xhttpstaticplanconfig Config;
	xhttpstaticplan Plan;
	xhttprepresentation Current;
	xhttpprecondition Condition;
	xrt_http_static_range_state RangeState;
	xhttprangeresult RangeResult;
	xstrview Set;
	size_t iRangeBytes;
	size_t iResolved = 0;
	bool bGet;
	bool bHead;

	if ( !xrtMemRangeValid(pPlan, sizeof(Plan)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpStaticPlanConfigResolve(
		pConfig,
		iRangeCapacity,
		&Config,
		&iRangeBytes
	) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pRanges, iRangeBytes) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpBridgePreconditionInputRead(
		Method,
		pFields,
		iFieldCount,
		pCurrent,
		&Current
	) ) {
		return false;
	}
	if ( !Current.Exists ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !__xrtHttpStaticPlanAliasesValid(
		Method,
		pFields,
		iFieldCount,
		pCurrent,
		&Current,
		pRanges,
		iRangeBytes,
		pConfig,
		pPlan
	) ) {
		return false;
	}

	memset(&Plan, 0, sizeof(Plan));
	Plan.Status = XHTTP_STATUS_OK;
	Plan.CompleteLength = iLength;
	Plan.SelectedLength = iLength;
	bGet = __xhttpViewEqual(
		Method, XRT_STR_LITERAL("GET")
	);
	bHead = __xhttpViewEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);
	if ( !bGet && !bHead ) {
		Plan.Status = XHTTP_STATUS_METHOD_NOT_ALLOWED;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	Plan.SendBody = bGet;
	Plan.AcceptRanges = Config.MaxRanges != 0;

	Condition = xrtHttpPreconditionsEvaluate(
		Method,
		pFields,
		iFieldCount,
		&Current
	);
	if ( Condition == XHTTP_PRECONDITION_ERROR ) {
		return false;
	}
	if ( Condition == XHTTP_PRECONDITION_NOT_MODIFIED ) {
		Plan.Status = XHTTP_STATUS_NOT_MODIFIED;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	if ( Condition == XHTTP_PRECONDITION_FAILED ) {
		Plan.Status = XHTTP_STATUS_PRECONDITION_FAILED;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}

	/* Range 只用于 GET；HEAD 按完整表示生成元数据但不发送正文。 */
	if ( !bGet ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	RangeState = __xrtHttpStaticRangeRead(
		pFields,
		iFieldCount,
		Config.MaxRanges,
		&Set
	);
	if ( (RangeState != XRT_HTTP_STATIC_RANGE_READY) ||
		!__xrtHttpStaticIfRangeAllows(
			pFields, iFieldCount, &Current
		) ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	RangeResult = xrtHttpByteRangesResolve(
		Set,
		iLength,
		pRanges,
		iRangeCapacity,
		Config.MergeGap,
		&iResolved,
		&Plan.SelectedLength
	);
	if ( RangeResult == XHTTP_RANGE_ERROR ) {
		return false;
	}
	if ( RangeResult == XHTTP_RANGE_EMPTY ) {
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	if ( RangeResult == XHTTP_RANGE_UNSATISFIED ) {
		Plan.Status = XHTTP_STATUS_RANGE_NOT_SATISFIABLE;
		Plan.SendBody = false;
		Plan.SelectedLength = 0;
		__xrtHttpStaticPlanPublish(pPlan, &Plan);
		return true;
	}
	Plan.Status = XHTTP_STATUS_PARTIAL_CONTENT;
	Plan.RangeCount = iResolved;
	__xrtHttpStaticPlanPublish(pPlan, &Plan);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_static_response.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)



#if defined(XHTTP_FEATURE_HTTP_STATIC_RESPONSE)

/* 构建前测量保存全部派生状态，实际写出不再执行可变决策。 */
typedef struct xrt_http_static_response_measure {
	xhttpstaticresponseconfig Config;
	xstrview ContentType;
	xhttpcontentrange ContentRange;
	uint64 BodyLength;
	size_t Required;
	size_t ETagSize;
	size_t DateSize;
	size_t LengthSize;
	size_t RangeSize;
	size_t MultipartTypeSize;
	size_t FieldCount;
	bool Multipart;
	bool IncludeType;
	bool IncludeLength;
	bool IncludeRange;
	bool IncludeAcceptRanges;
	bool IncludeETag;
	bool IncludeLastModified;
	bool IncludeCacheControl;
	bool IncludeAllow;
} xrt_http_static_response_measure;



/* 安全累加工作区长度。 */
static bool __xrtHttpStaticResponseSizeAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 初始化静态响应字段策略。 */
XHTTP_SERVER_API void xrtHttpStaticResponseConfigInit(
	xhttpstaticresponseconfig* pConfig
)
{
	xhttpstaticresponseconfig Config = {
		XRT_STR_LITERAL("application/octet-stream"),
		{ NULL, 0 },
		{ NULL, 0 }
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 验证计划中的已解析范围并重新核对有效负载总长度。 */
/* 快照可选的静态响应配置。 */
static bool __xrtHttpStaticResponseConfigRead(
	const xhttpstaticresponseconfig* pInput,
	xhttpstaticresponseconfig* pConfig
)
{
	*pConfig = (xhttpstaticresponseconfig){
		XRT_STR_LITERAL("application/octet-stream"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	if ( pInput == NULL ) {
		return true;
	}
	if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	return true;
}



static bool __xrtHttpStaticResponseRangesValid(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges
)
{
	xhttpbyterange Current;
	xhttpbyterange Previous;
	uint64 iSelected = 0;
	size_t i;

	if ( pPlan->RangeCount == 0 ) {
		return true;
	}
	if ( (pRanges == NULL) ||
		(pPlan->CompleteLength == 0) ) {
		return false;
	}
	for ( i = 0; i < pPlan->RangeCount; i++ ) {
		uint64 iLength;

		memcpy(
			&Current,
			((const uint8*)pRanges) +
				(i * sizeof(Current)),
			sizeof(Current)
		);
		if ( (Current.First > Current.Last) ||
			(Current.Last >= pPlan->CompleteLength) ||
			((i != 0) &&
			 (Previous.Last >= Current.First)) ) {
			return false;
		}
		iLength = (Current.Last - Current.First) +
			UINT64_C(1);
		if ( iSelected > (UINT64_MAX - iLength) ) {
			return false;
		}
		iSelected += iLength;
		Previous = Current;
	}
	return iSelected == pPlan->SelectedLength;
}



/* 验证静态计划自身可以安全生成响应。 */
static bool __xrtHttpStaticResponsePlanValid(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges
)
{
	if ( (pPlan == NULL) ||
		(pPlan->RangeCount >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ) {
		return false;
	}
	switch ( pPlan->Status ) {
		case XHTTP_STATUS_OK:
			return (pPlan->RangeCount == 0) &&
				(pPlan->SelectedLength ==
				 pPlan->CompleteLength);

		case XHTTP_STATUS_PARTIAL_CONTENT:
			return pPlan->SendBody &&
				(pPlan->RangeCount != 0) &&
				(pPlan->SelectedLength != 0) &&
				__xrtHttpStaticResponseRangesValid(
					pPlan,
					pRanges
				);

		case XHTTP_STATUS_NOT_MODIFIED:
		case XHTTP_STATUS_METHOD_NOT_ALLOWED:
		case XHTTP_STATUS_PRECONDITION_FAILED:
		case XHTTP_STATUS_RANGE_NOT_SATISFIABLE:
			return !pPlan->SendBody &&
				(pPlan->RangeCount == 0) &&
				(pPlan->SelectedLength == 0);

		default:
			return false;
	}
}



/* 解析配置、状态和字段集合并计算精确工作区长度。 */
static bool __xrtHttpStaticResponseMeasure(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	xrt_http_static_response_measure* pMeasure
)
{
	static const size_t iMultipartPrefix =
		sizeof("multipart/byteranges; boundary=") - 1u;
	xrt_http_static_response_measure Measure;
	xhttpbyterange Range;
	bool bMetadata;

	memset(&Measure, 0, sizeof(Measure));
	Measure.Config = *pConfig;
	if ( !pCurrent->Exists ||
		!__xhttpViewValid(Measure.Config.ContentType) ||
		!__xhttpViewValid(Measure.Config.CacheControl) ||
		!__xhttpViewValid(Measure.Config.Boundary) ||
		!__xrtHttpStaticResponsePlanValid(
			pPlan,
			pRanges
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	Measure.ContentType =
		Measure.Config.ContentType.Size != 0 ?
		Measure.Config.ContentType :
		XRT_STR_LITERAL("application/octet-stream");
	if ( !xrtHttpFieldValueValid(Measure.ContentType) ||
		((Measure.Config.CacheControl.Size != 0) &&
		 !xrtHttpFieldValueValid(
			Measure.Config.CacheControl
		 )) ||
		(pCurrent->HasETag &&
		 !xrtHttpBridgeETagValid(&pCurrent->ETag)) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}

	Measure.Multipart =
		(pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		(pPlan->RangeCount > 1u);
	if ( Measure.Multipart ) {
		if ( !xrtHttpBridgeRangeMultipartLength(
			pRanges,
			pPlan->RangeCount,
			pPlan->CompleteLength,
			Measure.ContentType,
			Measure.Config.Boundary,
			&Measure.BodyLength
		) ) {
			return false;
		}
		if ( !__xrtHttpStaticResponseSizeAdd(
			&Measure.MultipartTypeSize,
			iMultipartPrefix
		) || !__xrtHttpStaticResponseSizeAdd(
			&Measure.MultipartTypeSize,
			Measure.Config.Boundary.Size
		) ) {
			return false;
		}
	}

	Measure.IncludeType =
		(pPlan->Status == XHTTP_STATUS_OK) ||
		(pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT);
	Measure.IncludeLength =
		pPlan->Status != XHTTP_STATUS_NOT_MODIFIED;
	Measure.IncludeRange =
		((pPlan->Status == XHTTP_STATUS_PARTIAL_CONTENT) &&
		 (pPlan->RangeCount == 1u)) ||
		(pPlan->Status ==
		 XHTTP_STATUS_RANGE_NOT_SATISFIABLE);
	Measure.IncludeAcceptRanges =
		pPlan->AcceptRanges &&
		(pPlan->Status != XHTTP_STATUS_METHOD_NOT_ALLOWED);
	Measure.IncludeAllow =
		pPlan->Status == XHTTP_STATUS_METHOD_NOT_ALLOWED;
	bMetadata =
		pPlan->Status != XHTTP_STATUS_METHOD_NOT_ALLOWED;
	Measure.IncludeETag =
		bMetadata && pCurrent->HasETag;
	Measure.IncludeLastModified =
		bMetadata && pCurrent->HasLastModified;
	Measure.IncludeCacheControl =
		bMetadata &&
		(Measure.Config.CacheControl.Size != 0);

	if ( pPlan->Status == XHTTP_STATUS_OK ) {
		Measure.BodyLength = pPlan->CompleteLength;
	} else if ( pPlan->Status ==
		XHTTP_STATUS_PARTIAL_CONTENT &&
		!Measure.Multipart ) {
		Measure.BodyLength = pPlan->SelectedLength;
	}
	if ( Measure.IncludeRange ) {
		memset(
			&Measure.ContentRange,
			0,
			sizeof(Measure.ContentRange)
		);
		Measure.ContentRange.HasLength = true;
		Measure.ContentRange.Length =
			pPlan->CompleteLength;
		if ( pPlan->Status ==
			XHTTP_STATUS_PARTIAL_CONTENT ) {
			memcpy(&Range, (const void*)pRanges, sizeof(Range));
			Measure.ContentRange.Satisfied = true;
			Measure.ContentRange.First = Range.First;
			Measure.ContentRange.Last = Range.Last;
		}
		if ( !xrtHttpContentRangeWrite(
			&Measure.ContentRange,
			NULL,
			0,
			&Measure.RangeSize
		) ) {
			return false;
		}
	}
	if ( Measure.IncludeETag &&
		!xrtHttpETagWrite(
			&pCurrent->ETag,
			NULL,
			0,
			&Measure.ETagSize
		) ) {
		return false;
	}
	if ( Measure.IncludeLastModified ) {
		Measure.DateSize = xrtTimeWriteHTTPDate(
			NULL,
			0,
			pCurrent->LastModified
		);
		if ( Measure.DateSize == XRT_NPOS ) {
			return false;
		}
	}
	if ( Measure.IncludeLength ) {
		Measure.LengthSize = __xhttpUInt64Size(
			Measure.BodyLength
		);
	}

	Measure.FieldCount =
		(size_t)Measure.IncludeType +
		(size_t)Measure.IncludeLength +
		(size_t)Measure.IncludeRange +
		(size_t)Measure.IncludeAcceptRanges +
		(size_t)Measure.IncludeETag +
		(size_t)Measure.IncludeLastModified +
		(size_t)Measure.IncludeCacheControl +
		(size_t)Measure.IncludeAllow;
	if ( Measure.FieldCount >
		XHTTP_STATIC_RESPONSE_MAX_FIELDS ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	if ( (Measure.Multipart &&
		 !__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.MultipartTypeSize
		 )) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.LengthSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.RangeSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.ETagSize
		) ||
		!__xrtHttpStaticResponseSizeAdd(
			&Measure.Required,
			Measure.DateSize
		) ) {
		return false;
	}
	*pMeasure = Measure;
	return true;
}



/* 判断一个输出区域是否覆盖静态响应的任一输入。 */
static bool __xrtHttpStaticResponseInputOverlap(
	const void* pOutput,
	size_t iOutputSize,
	const xhttpstaticplan* pPlanInput,
	const xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttprepresentation* pCurrentInput,
	const xhttpstaticresponseconfig* pConfig,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure
)
{
	return xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pPlanInput,
			sizeof(*pPlanInput)
		) || xrtMemRangesOverlap(
			pOutput, iOutputSize, pRanges, iRangeBytes
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrentInput,
			sizeof(*pCurrentInput)
		) || ((pConfig != NULL) && xrtMemRangesOverlap(
			pOutput, iOutputSize, pConfig, sizeof(*pConfig)
		)) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.ContentType.Data,
			pMeasure->Config.ContentType.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.CacheControl.Data,
			pMeasure->Config.CacheControl.Size
		) || xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pMeasure->Config.Boundary.Data,
			pMeasure->Config.Boundary.Size
		) || (pCurrent->HasETag && xrtMemRangesOverlap(
			pOutput,
			iOutputSize,
			pCurrent->ETag.Opaque.Data,
			pCurrent->ETag.Opaque.Size
		));
}



/* 验证长度、结构和工作区输出互不覆盖。 */
static bool __xrtHttpStaticResponseAliasesValid(
	const xhttpstaticplan* pPlanInput,
	const xhttpbyterange* pRanges,
	size_t iRangeBytes,
	const xhttprepresentation* pCurrentInput,
	const xhttpstaticresponseconfig* pConfig,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure,
	void* pWorkspace,
	size_t* pSize,
	xhttpstaticresponse* pResponse
)
{
	if ( __xrtHttpStaticResponseInputOverlap(
			pSize,
			sizeof(*pSize),
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		) || ((pResponse != NULL) &&
		 (__xrtHttpStaticResponseInputOverlap(
			pResponse,
			sizeof(*pResponse),
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		  ) || xrtMemRangesOverlap(
			pResponse,
			sizeof(*pResponse),
			pSize,
			sizeof(*pSize)
		  ))) || ((pMeasure->Required != 0) &&
		 (__xrtHttpStaticResponseInputOverlap(
			pWorkspace,
			pMeasure->Required,
			pPlanInput,
			pRanges,
			iRangeBytes,
			pCurrentInput,
			pConfig,
			pCurrent,
			pMeasure
		  ) || xrtMemRangesOverlap(
			pWorkspace,
			pMeasure->Required,
			pSize,
			sizeof(*pSize)
		  ) || ((pResponse != NULL) &&
		   xrtMemRangesOverlap(
			pWorkspace,
			pMeasure->Required,
			pResponse,
			sizeof(*pResponse)
		   )))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	return true;
}



/* 向响应追加一个借用字段。 */
static void __xrtHttpStaticResponseField(
	xhttpstaticresponse* pResponse,
	xstrview Name,
	xstrview Value
)
{
	pResponse->Fields[pResponse->FieldCount].Name = Name;
	pResponse->Fields[pResponse->FieldCount].Value = Value;
	pResponse->FieldCount++;
}



/* 在工作区中保留一个字段值视图。 */
static xstrview __xrtHttpStaticResponseReserve(
	char* sWorkspace,
	size_t* pOffset,
	size_t iSize
)
{
	xstrview Value = {
		sWorkspace + *pOffset,
		iSize
	};

	*pOffset += iSize;
	return Value;
}



/* 把测量结果写入工作区和独立字段描述符。 */
static bool __xrtHttpStaticResponseWrite(
	const xhttpstaticplan* pPlan,
	const xhttprepresentation* pCurrent,
	const xrt_http_static_response_measure* pMeasure,
	char* sWorkspace,
	xhttpstaticresponse* pResponse
)
{
	static const char sMultipartPrefix[] =
		"multipart/byteranges; boundary=";
	xhttpstaticresponse Response;
	xstrview Value;
	char sDate[30];
	size_t iValue;
	size_t iOffset = 0;

	memset(&Response, 0, sizeof(Response));
	Response.Status = pPlan->Status;
	Response.SendBody = pPlan->SendBody;
	Response.Multipart = pMeasure->Multipart;
	Response.BodyLength = pMeasure->BodyLength;
	Response.ContentType = pMeasure->ContentType;
	Response.Boundary = pMeasure->Config.Boundary;

	if ( pMeasure->IncludeType ) {
		if ( pMeasure->Multipart ) {
			Value = __xrtHttpStaticResponseReserve(
				sWorkspace,
				&iOffset,
				pMeasure->MultipartTypeSize
			);
			memcpy(
				(char*)Value.Data,
				sMultipartPrefix,
				sizeof(sMultipartPrefix) - 1u
			);
			memcpy(
				(char*)Value.Data +
					sizeof(sMultipartPrefix) - 1u,
				pMeasure->Config.Boundary.Data,
				pMeasure->Config.Boundary.Size
			);
		} else {
			Value = pMeasure->ContentType;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Type"),
			Value
		);
	}
	if ( pMeasure->IncludeLength ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->LengthSize
		);
		iValue = __xhttpUInt64Write(
			(char*)Value.Data,
			pMeasure->BodyLength
		);
		if ( iValue != Value.Size ) {
			xrtSetErrorKind(XERR_INTERNAL);
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Length"),
			Value
		);
	}
	if ( pMeasure->IncludeRange ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->RangeSize
		);
		if ( !xrtHttpContentRangeWrite(
			&pMeasure->ContentRange,
			(char*)Value.Data,
			Value.Size,
			&iValue
		) || (iValue != Value.Size) ) {
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Content-Range"),
			Value
		);
	}
	if ( pMeasure->IncludeAcceptRanges ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Accept-Ranges"),
			XRT_STR_LITERAL("bytes")
		);
	}
	if ( pMeasure->IncludeETag ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->ETagSize
		);
		if ( !xrtHttpETagWrite(
			&pCurrent->ETag,
			(char*)Value.Data,
			Value.Size,
			&iValue
		) || (iValue != Value.Size) ) {
			return false;
		}
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("ETag"),
			Value
		);
	}
	if ( pMeasure->IncludeLastModified ) {
		Value = __xrtHttpStaticResponseReserve(
			sWorkspace,
			&iOffset,
			pMeasure->DateSize
		);
		iValue = xrtTimeWriteHTTPDate(
			sDate,
			sizeof(sDate),
			pCurrent->LastModified
		);
		if ( iValue != Value.Size ) {
			return false;
		}
		memcpy((char*)Value.Data, sDate, iValue);
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Last-Modified"),
			Value
		);
	}
	if ( pMeasure->IncludeCacheControl ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Cache-Control"),
			pMeasure->Config.CacheControl
		);
	}
	if ( pMeasure->IncludeAllow ) {
		__xrtHttpStaticResponseField(
			&Response,
			XRT_STR_LITERAL("Allow"),
			XRT_STR_LITERAL("GET, HEAD")
		);
	}
	if ( (Response.FieldCount != pMeasure->FieldCount) ||
		(iOffset != pMeasure->Required) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	memcpy((void*)pResponse, &Response, sizeof(Response));
	return true;
}



/* 构建静态响应协议字段。 */
XHTTP_SERVER_API bool xrtHttpStaticResponseBuild(
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	const xhttprepresentation* pCurrent,
	const xhttpstaticresponseconfig* pConfig,
	void* pWorkspace,
	size_t iCapacity,
	size_t* pSize,
	xhttpstaticresponse* pResponse
)
{
	xhttpstaticresponseconfig Config;
	xhttpstaticplan Plan;
	xhttprepresentation Current;
	xrt_http_static_response_measure Measure;
	size_t iRangeBytes;
	size_t iRequired;

	if ( !xrtMemRangeValid(pSize, sizeof(iRequired)) ||
		!xrtMemRangeValid(pWorkspace, iCapacity) ||
		((pResponse == NULL) &&
		 ((pWorkspace != NULL) || (iCapacity != 0))) ||
		((pResponse != NULL) &&
		 !xrtMemRangeValid(pResponse, sizeof(*pResponse))) ||
		!xrtMemRangeValid(pPlan, sizeof(Plan)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(&Plan, (const void*)pPlan, sizeof(Plan));
	if ( Plan.RangeCount >
		(SIZE_MAX / sizeof(xhttpbyterange)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	iRangeBytes = Plan.RangeCount * sizeof(xhttpbyterange);
	if ( !xrtMemRangeValid(pRanges, iRangeBytes) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpBridgePreconditionInputRead(
			XRT_STR_LITERAL("GET"),
			NULL,
			0,
			pCurrent,
			&Current
		) || !__xrtHttpStaticResponseConfigRead(
			pConfig,
			&Config
		) ) {
		return false;
	}
	if ( !__xrtHttpStaticResponseMeasure(
		&Plan,
		pRanges,
		&Current,
		&Config,
		&Measure
	) || !__xrtHttpStaticResponseAliasesValid(
		pPlan,
		pRanges,
		iRangeBytes,
		pCurrent,
		pConfig,
		&Current,
		&Measure,
		pWorkspace,
		pSize,
		pResponse
	) ) {
		return false;
	}
	if ( pResponse == NULL ) {
		iRequired = Measure.Required;
		memcpy((void*)pSize, &iRequired, sizeof(iRequired));
		return true;
	}
	if ( iCapacity < Measure.Required ) {
		iRequired = Measure.Required;
		memcpy((void*)pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( (Measure.Required != 0) &&
		(pWorkspace == NULL) ) {
		iRequired = Measure.Required;
		memcpy((void*)pSize, &iRequired, sizeof(iRequired));
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( !__xrtHttpStaticResponseWrite(
		&Plan,
		&Current,
		&Measure,
		(char*)pWorkspace,
		pResponse
	) ) {
		return false;
	}
	iRequired = Measure.Required;
	memcpy((void*)pSize, &iRequired, sizeof(iRequired));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_body_file.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)



#if defined(XHTTP_FEATURE_HTTP_BODY_FILE)

/* 文件正文工厂独占一个异步文件和不可变区间。 */
typedef struct xrt_http_body_file_factory {
	xasyncfile* File;
	uint64 Offset;
	uint64 Length;
	size_t ReadSize;
} xrt_http_body_file_factory;



/* 每个文件正文只有一个 Reader，并且最多保留一次异步读取。 */
typedef struct xrt_http_body_file_reader {
	xrt_http_body_file_factory* Factory;
	xrt_http_body_file_cursor Cursor;
} xrt_http_body_file_reader;



/* 文件准备任务复制路径，并保留完整文件或严格区间参数。 */
typedef struct xrt_http_body_file_prepare {
	xtaskpool* Pool;
	uint64 Offset;
	uint64 Length;
	xhttpbodyfileconfig Config;
	bool Range;
	char Path[];
} xrt_http_body_file_prepare;



/* 设置文件正文域错误，并接管可选 cause 引用。 */
static void __xrtHttpBodyFileError(
	xerrkind Kind,
	xhttpbodyfileerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.body.file";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 把当前执行上下文错误包装为文件正文错误。 */
static void __xrtHttpBodyFileWrap(
	xerrkind Kind,
	xhttpbodyfileerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpBodyFileError(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);
}



/* 静默关闭同步文件，并保留调用方正在处理的错误。 */
void __xrtHttpBodyFileCloseSync(xfile File)
{
	xerror* pSaved = xrtTakeError();

	if ( File != NULL ) {
		(void)xrtClose(File);
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 请求关闭异步文件，并保留调用方正在处理的错误。 */
void __xrtHttpBodyFileCloseAsync(xasyncfile* pFile)
{
	xerror* pSaved = xrtTakeError();
	xfuture* pFuture;

	if ( pFile != NULL ) {
		pFuture = xrtAsyncFileClose(pFile);
		xrtFutureDestroy(pFuture);
	}
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	}
}



/* 文件 Chunk 通过保留读取 Future 延长连续结果缓冲的生命周期。 */
static void __xrtHttpBodyFileChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	xrtFutureDestroy((xfuture*)pContext);
}



/* 释放 Reader 的待处理读取，并触发工厂文件关闭。 */
static void __xrtHttpBodyFileReaderClose(ptr pContext)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;
	xasyncfile* pFile;

	if ( pReader == NULL ) {
		return;
	}
	__xrtHttpBodyFileCursorCancel(&pReader->Cursor);
	pFile = pReader->Factory->File;
	pReader->Factory->File = NULL;
	__xrtHttpBodyFileCloseAsync(pFile);
	xrtFree(pReader);
}



/* 在 AGAIN 后返回当前读取 Future 的独立消费端引用。 */
static xfuture* __xrtHttpBodyFileReaderWait(ptr pContext)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;

	if ( pReader == NULL ) {
		__xhttpErrorSetInvalidState();
		return NULL;
	}
	return __xrtHttpBodyFileCursorWait(
		&pReader->Cursor
	);
}



/* 初始化一个连续异步文件区间。 */
void __xrtHttpBodyFileCursorInit(
	xrt_http_body_file_cursor* pCursor,
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	size_t iReadSize
)
{
	memset(pCursor, 0, sizeof(*pCursor));
	pCursor->File = pFile;
	pCursor->Offset = iOffset;
	pCursor->Remaining = iLength;
	pCursor->ReadSize = iReadSize;
}



/* 取消并释放尚未消费完的异步读取。 */
void __xrtHttpBodyFileCursorCancel(
	xrt_http_body_file_cursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Pending == NULL) ) {
		return;
	}
	(void)xrtFutureCancel(pCursor->Pending);
	xrtFutureDestroy(pCursor->Pending);
	pCursor->Pending = NULL;
	pCursor->Requested = 0;
	pCursor->ReadyOffset = 0;
	pCursor->Ready = false;
}



/* 在 AGAIN 后返回 cursor 当前读取的独立引用。 */
xfuture* __xrtHttpBodyFileCursorWait(
	xrt_http_body_file_cursor* pCursor
)
{
	if ( (pCursor == NULL) ||
		(pCursor->Pending == NULL) ) {
		__xhttpErrorSetInvalidState();
		return NULL;
	}
	return xrtFutureRef(pCursor->Pending);
}



/* 把已经完成的读取结果切分成不超过当前上限的独立 Chunk。 */
static xhttpbodystatus __xrtHttpBodyFileCursorReady(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xfiledata* pData = (xfiledata*)xrtFutureValue(
		pCursor->Pending
	);
	xfuture* pLease;
	size_t iAvailable;
	size_t iSize;

	if ( pData == NULL ) {
		__xrtHttpBodyFileError(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_READ,
			"read",
			"the asynchronous file read returned no data result",
			NULL
		);
		return XHTTP_BODY_ERROR;
	}
	if ( !pCursor->Ready ) {
		if ( pData->Size != pCursor->Requested ) {
			__xrtHttpBodyFileError(
				XERR_IO,
				XHTTP_BODY_FILE_ERROR_READ,
				"read",
				"the file ended before the declared body range",
				NULL
			);
			return XHTTP_BODY_ERROR;
		}
		pCursor->Ready = true;
		pCursor->ReadyOffset = 0;
	}
	iAvailable = pData->Size - pCursor->ReadyOffset;
	iSize = iAvailable < iMaxBytes ? iAvailable : iMaxBytes;
	pLease = xrtFutureRef(pCursor->Pending);
	if ( pLease == NULL ) {
		return XHTTP_BODY_ERROR;
	}
	pChunk->Data = pData->Data + pCursor->ReadyOffset;
	pChunk->Size = iSize;
	pChunk->Release = __xrtHttpBodyFileChunkRelease;
	pChunk->Context = pLease;
	pCursor->ReadyOffset += iSize;
	pCursor->Offset += (uint64)iSize;
	pCursor->Remaining -= (uint64)iSize;
	if ( pCursor->ReadyOffset == pData->Size ) {
		xrtFutureDestroy(pCursor->Pending);
		pCursor->Pending = NULL;
		pCursor->Requested = 0;
		pCursor->ReadyOffset = 0;
		pCursor->Ready = false;
	}
	return XHTTP_BODY_DATA;
}



/* 推进文件读取状态机；磁盘读取只通过异步文件任务发生。 */
xhttpbodystatus __xrtHttpBodyFileCursorNext(
	xrt_http_body_file_cursor* pCursor,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xfuturestate State;
	size_t iRequest;

	if ( (pCursor == NULL) || (pCursor->File == NULL) ||
		(pCursor->ReadSize == 0) ||
		(pChunk == NULL) || (iMaxBytes == 0) ) {
		__xhttpErrorSetInvalidArgument();
		return XHTTP_BODY_ERROR;
	}
	if ( pCursor->Pending != NULL ) {
		State = xrtFutureState(pCursor->Pending);
		if ( State == XFUTURE_PENDING ) {
			return XHTTP_BODY_AGAIN;
		}
		if ( State != XFUTURE_RESOLVED ) {
			(void)xrtFutureValue(pCursor->Pending);
			__xrtHttpBodyFileWrap(
				XERR_IO,
				XHTTP_BODY_FILE_ERROR_READ,
				"read",
				"the asynchronous file read failed"
			);
			return XHTTP_BODY_ERROR;
		}
		return __xrtHttpBodyFileCursorReady(
			pCursor,
			iMaxBytes,
			pChunk
		);
	}
	if ( pCursor->Remaining == 0 ) {
		return XHTTP_BODY_EOF;
	}
	iRequest = pCursor->Remaining < (uint64)iMaxBytes ?
		(size_t)pCursor->Remaining : iMaxBytes;
	if ( iRequest > pCursor->ReadSize ) {
		iRequest = pCursor->ReadSize;
	}
	pCursor->Pending = xrtAsyncFileReadAt(
		pCursor->File,
		pCursor->Offset,
		iRequest
	);
	if ( pCursor->Pending == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_READ,
			"submit-read",
			"failed to submit the next file body read"
		);
		return XHTTP_BODY_ERROR;
	}
	pCursor->Requested = iRequest;
	return XHTTP_BODY_AGAIN;
}



/* 通过共享 cursor 推进单区间文件正文。 */
static xhttpbodystatus __xrtHttpBodyFileReaderNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_body_file_reader* pReader =
		(xrt_http_body_file_reader*)pContext;

	if ( pReader == NULL ) {
		__xhttpErrorSetInvalidState();
		return XHTTP_BODY_ERROR;
	}
	return __xrtHttpBodyFileCursorNext(
		&pReader->Cursor,
		iMaxBytes,
		pChunk
	);
}



/* 快速分配单消费 Reader，不执行任何文件系统调用。 */
static bool __xrtHttpBodyFileOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xrt_http_body_file_factory* pFile =
		(xrt_http_body_file_factory*)pFactory;
	xrt_http_body_file_reader* pReader;

	if ( (pFile == NULL) || (pFile->File == NULL) ||
		(pOps == NULL) || (ppReader == NULL) ) {
		__xhttpErrorSetInvalidState();
		return false;
	}
	pReader = (xrt_http_body_file_reader*)xrtCalloc(
		1,
		sizeof(xrt_http_body_file_reader)
	);
	if ( pReader == NULL ) {
		return false;
	}
	pReader->Factory = pFile;
	__xrtHttpBodyFileCursorInit(
		&pReader->Cursor,
		pFile->File,
		pFile->Offset,
		pFile->Length,
		pFile->ReadSize
	);
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = __xrtHttpBodyFileReaderNext;
	pOps->Close = __xrtHttpBodyFileReaderClose;
	pOps->Wait = __xrtHttpBodyFileReaderWait;
	*ppReader = pReader;
	return true;
}



/* 销毁未打开的工厂，或回收 Reader 已经关闭后的空工厂。 */
static void __xrtHttpBodyFileDestroy(ptr pFactory)
{
	xrt_http_body_file_factory* pFile =
		(xrt_http_body_file_factory*)pFactory;

	if ( pFile == NULL ) {
		return;
	}
	__xrtHttpBodyFileCloseAsync(pFile->File);
	pFile->File = NULL;
	xrtFree(pFile);
}



/* 校验文件区间能由跨平台异步文件层表达。 */
bool __xrtHttpBodyFileRangeValid(
	uint64 iOffset,
	uint64 iLength
)
{
	if ( (iOffset > (uint64)INT64_MAX) ||
		(iLength > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtHttpBodyFileError(
			XERR_RANGE,
			XHTTP_BODY_FILE_ERROR_RANGE,
			"range",
			"the file body range is outside the supported file offset space",
			NULL
		);
		return false;
	}
	return true;
}



/* 初始化默认的按需读取粒度，不为正文预分配固定缓冲。 */
XHTTP_SERVER_API void xrtHttpBodyFileConfigInit(
	xhttpbodyfileconfig* pConfig
)
{
	xhttpbodyfileconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xhttpErrorSetInvalidArgument();
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.ReadSize = XHTTP_BODY_FILE_READ_DEFAULT;
	/* Caller snapshots may be unaligned: copy an explicit byte view. */
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 复制并校验调用方配置；NULL 使用稳定默认值。 */
static bool __xrtHttpBodyFileConfigCopy(
	xhttpbodyfileconfig* pOutput,
	const xhttpbodyfileconfig* pConfig
)
{
	memset(pOutput, 0, sizeof(*pOutput));
	pOutput->ReadSize = XHTTP_BODY_FILE_READ_DEFAULT;
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
			__xhttpErrorSetInvalidArgument();
			return false;
		}
		memcpy(pOutput, (const void*)pConfig, sizeof(*pOutput));
	}
	if ( pOutput->ReadSize == 0 ) {
		__xhttpErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建采用异步文件的不可重放正文。 */
XHTTP_SERVER_API xhttpbody* xrtHttpBodyFileAdopt(
	xasyncfile* pFile,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpBodyFileOpen,
		__xrtHttpBodyFileDestroy
	};
	xrt_http_body_file_factory* pFactory;
	xhttpbody* pBody;
	xhttpbodyfileconfig Config;

	if ( (pFile == NULL) ||
		((xrtAsyncFileFlags(pFile) & XFILE_READ) == 0u) ) {
		if ( pFile == NULL ) {
			__xhttpErrorSetInvalidArgument();
		} else {
			__xrtHttpBodyFileError(
				XERR_ARGUMENT,
				XHTTP_BODY_FILE_ERROR_ADOPT,
				"adopt",
				"the adopted asynchronous file is not readable",
				NULL
			);
		}
		return NULL;
	}
	if ( !__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		return NULL;
	}
	if ( !__xrtHttpBodyFileConfigCopy(&Config, pConfig) ) {
		return NULL;
	}
	pFactory = (xrt_http_body_file_factory*)xrtCalloc(
		1,
		sizeof(xrt_http_body_file_factory)
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->File = pFile;
	pFactory->Offset = iOffset;
	pFactory->Length = iLength;
	pFactory->ReadSize = Config.ReadSize;
	pBody = xrtHttpBodyCreate(
		&Ops,
		pFactory,
		iLength,
		XHTTP_BODY_NONE
	);
	if ( pBody == NULL ) {
		pFactory->File = NULL;
		xrtFree(pFactory);
	}
	return pBody;
}



/* 释放 Future 拥有的文件正文结果。 */
static void __xrtHttpBodyFileValueFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpBodyDestroy((xhttpbody*)pValue);
}



/* 在任务池工作线程内打开、定长并采用同一个文件句柄。 */
static xtaskoutcome __xrtHttpBodyFilePrepareTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_body_file_prepare* pPrepare =
		(xrt_http_body_file_prepare*)pData;
	xfile File;
	xasyncfile* pAsync;
	xhttpbody* pBody;
	uint64 iFileSize;
	uint64 iOffset;
	uint64 iLength;

	File = xrtOpen(pPrepare->Path, XFILE_READ);
	if ( File == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_OPEN,
			"open",
			"failed to open the HTTP body file"
		);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_CANCELLED;
	}
	if ( !xrtFileSize(File, &iFileSize) ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_SIZE,
			"size",
			"failed to query the HTTP body file size"
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	iOffset = pPrepare->Range ? pPrepare->Offset : 0;
	iLength = pPrepare->Range ? pPrepare->Length : iFileSize;
	if ( (iOffset > iFileSize) ||
		(iLength > (iFileSize - iOffset)) ) {
		__xrtHttpBodyFileError(
			XERR_RANGE,
			XHTTP_BODY_FILE_ERROR_RANGE,
			"range",
			"the requested HTTP body range exceeds the opened file",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( !__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_CANCELLED;
	}
	pAsync = xrtAsyncFileAdopt(pPrepare->Pool, File);
	if ( pAsync == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_ADOPT,
			"adopt",
			"failed to adopt the opened HTTP body file"
		);
		__xrtHttpBodyFileCloseSync(File);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseAsync(pAsync);
		return XTASK_CANCELLED;
	}
	pBody = xrtHttpBodyFileAdopt(
		pAsync,
		iOffset,
		iLength,
		&pPrepare->Config
	);
	if ( pBody == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_CREATE,
			"create",
			"failed to create the HTTP file body"
		);
		__xrtHttpBodyFileCloseAsync(pAsync);
		return XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtHttpBodyDestroy(pBody);
		return XTASK_CANCELLED;
	}
	pResult->Value = pBody;
	pResult->Destroy = __xrtHttpBodyFileValueFree;
	return XTASK_SUCCESS;
}



/* 释放任务池已经受理的文件准备参数。 */
static void __xrtHttpBodyFilePrepareFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtFree(pValue);
}



/* 复制路径并提交完整文件或严格区间准备任务。 */
static xfuture* __xrtHttpBodyFilePrepare(
	xtaskpool* pPool,
	cstr sPath,
	bool bRange,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	xrt_http_body_file_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;
	size_t iPathLength;
	xhttpbodyfileconfig Config;

	if ( (pPool == NULL) || (sPath == NULL) ||
		(sPath[0] == '\0') ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	if ( bRange &&
		!__xrtHttpBodyFileRangeValid(iOffset, iLength) ) {
		return NULL;
	}
	if ( !__xrtHttpBodyFileConfigCopy(&Config, pConfig) ) {
		return NULL;
	}
	iPathLength = strlen(sPath);
	if ( iPathLength > (SIZE_MAX -
		sizeof(xrt_http_body_file_prepare) - 1) ) {
		__xhttpErrorSetSizeOverflow();
		return NULL;
	}
	pPrepare = (xrt_http_body_file_prepare*)xrtMalloc(
		sizeof(xrt_http_body_file_prepare) +
		iPathLength + 1
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Offset = iOffset;
	pPrepare->Length = iLength;
	pPrepare->Config = Config;
	pPrepare->Range = bRange;
	memcpy(pPrepare->Path, sPath, iPathLength + 1);
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpBodyFilePrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpBodyFilePrepareTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpBodyFileWrap(
			XERR_IO,
			XHTTP_BODY_FILE_ERROR_SUBMIT,
			"submit",
			"failed to submit the HTTP file body preparation"
		);
		xrtFree(pPrepare);
	}
	return pFuture;
}



/* 在任务池中准备完整文件正文。 */
XHTTP_SERVER_API xfuture* xrtHttpBodyFileFuture(
	xtaskpool* pPool,
	cstr sPath,
	const xhttpbodyfileconfig* pConfig
)
{
	return __xrtHttpBodyFilePrepare(
		pPool,
		sPath,
		false,
		0,
		0,
		pConfig
	);
}



/* 在任务池中准备严格文件区间正文。 */
XHTTP_SERVER_API xfuture* xrtHttpBodyFileRangeFuture(
	xtaskpool* pPool,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	const xhttpbodyfileconfig* pConfig
)
{
	return __xrtHttpBodyFilePrepare(
		pPool,
		sPath,
		true,
		iOffset,
		iLength,
		pConfig
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_static_file.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)



#if defined(XHTTP_FEATURE_HTTP_STATIC_FILE)

/* 文件准备任务复制根内路径，并借用根和任务池到任务完成。 */
typedef struct xrt_http_static_file_prepare {
	xtaskpool* Pool;
	xroot Root;
	char Path[];
} xrt_http_static_file_prepare;



/* 设置静态文件域错误，并接管可选原因错误引用。 */
static void __xrtHttpStaticFileError(
	xerrkind Kind,
	xhttpstaticfileerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ? xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.static.file";
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* 把当前执行上下文错误包装为静态文件域错误。 */
static void __xrtHttpStaticFileWrap(
	xerrkind Kind,
	xhttpstaticfileerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtHttpStaticFileError(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);
}



/* 以无前导零的小写十六进制写入一个无符号元数据值。 */
static size_t __xrtHttpStaticFileHex(
	char* sOutput,
	uint64 iValue
)
{
	static const char sHex[] = "0123456789abcdef";
	char Reverse[16];
	size_t iSize = 0;
	size_t i;

	do {
		Reverse[iSize++] = sHex[iValue & UINT64_C(0x0f)];
		iValue >>= 4;
	} while ( iValue != 0 );
	for ( i = 0; i < iSize; i++ ) {
		sOutput[i] = Reverse[iSize - i - 1];
	}
	return iSize;
}



/* 把一个带类型前缀的元数据值追加到弱 ETag opaque-tag。 */
static void __xrtHttpStaticFileTagPart(
	xhttpstaticfile* pFile,
	size_t* pOffset,
	char iPrefix,
	uint64 iValue
)
{
	size_t iOffset = *pOffset;

	if ( iOffset != 0 ) {
		pFile->ETag[iOffset++] = '-';
	}
	pFile->ETag[iOffset++] = iPrefix;
	iOffset += __xrtHttpStaticFileHex(
		pFile->ETag + iOffset,
		iValue
	);
	*pOffset = iOffset;
}



/* 从同一文件句柄的元数据构造默认弱验证器。 */
static void __xrtHttpStaticFileValidators(
	xhttpstaticfile* pFile
)
{
	const xfileinfo* pInfo = &pFile->Info;
	xhttprepresentation* pCurrent =
		&pFile->Representation;
	size_t iTagSize = 0;

	memset(pCurrent, 0, sizeof(*pCurrent));
	pCurrent->Exists = true;
	if ( (pInfo->Available &
		XFILE_INFO_MODIFY_TIME) != 0u ) {
		pCurrent->HasLastModified = true;
		pCurrent->LastModified = pInfo->Modified;
		pCurrent->LastModifiedStrong = false;
		__xrtHttpStaticFileTagPart(
			pFile,
			&iTagSize,
			's',
			pInfo->Size
		);
		__xrtHttpStaticFileTagPart(
			pFile,
			&iTagSize,
			'm',
			(uint64)pInfo->Modified
		);
		if ( (pInfo->Available &
			XFILE_INFO_IDENTITY) != 0u ) {
			__xrtHttpStaticFileTagPart(
				pFile,
				&iTagSize,
				'd',
				pInfo->Device
			);
			__xrtHttpStaticFileTagPart(
				pFile,
				&iTagSize,
				'i',
				pInfo->Identity
			);
		}
		pFile->ETag[iTagSize] = '\0';
		pCurrent->HasETag = true;
		pCurrent->ETag.Opaque.Data = pFile->ETag;
		pCurrent->ETag.Opaque.Size = iTagSize;
		pCurrent->ETag.Weak = true;
	}
}



/* 打开、检查并采用文件根内的同一普通文件句柄。 */
static xhttpstaticfile* __xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath,
	xcancel* pCancel
)
{
	xfileoptions Options;
	xhttpstaticfile* pStatic;
	xasyncfile* pAsync;
	xfile File;
	xfileinfo Info;

	if ( (pPool == NULL) || (Root == NULL) ||
		(sPath == NULL) || (sPath[0] == '\0') ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		return NULL;
	}
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ;
	File = xrtRootFileOpen(Root, sPath, &Options);
	if ( File == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_OPEN,
			"open",
			"failed to open the root-relative static file"
		);
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( !xrtFileStat(File, &Info) ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_STAT,
			"stat",
			"failed to inspect the opened static file"
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( Info.Type != XFILE_TYPE_FILE ) {
		__xrtHttpStaticFileError(
			XERR_TYPE,
			XHTTP_STATIC_FILE_ERROR_TYPE,
			"stat",
			"the opened static resource is not a regular file",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( (Info.Available & XFILE_INFO_SIZE) == 0u ) {
		__xrtHttpStaticFileError(
			XERR_UNSUPPORTED,
			XHTTP_STATIC_FILE_ERROR_SIZE,
			"stat",
			"the opened static file does not provide a stable size",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( Info.Size > (uint64)INT64_MAX ) {
		__xrtHttpStaticFileError(
			XERR_RANGE,
			XHTTP_STATIC_FILE_ERROR_SIZE,
			"stat",
			"the static file exceeds the supported HTTP body offset space",
			NULL
		);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	pStatic = (xhttpstaticfile*)xrtCalloc(
		1,
		sizeof(xhttpstaticfile)
	);
	if ( pStatic == NULL ) {
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	pAsync = xrtAsyncFileAdopt(pPool, File);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_ADOPT,
			"adopt",
			"failed to adopt the opened static file"
		);
		xrtFree(pStatic);
		__xrtHttpBodyFileCloseSync(File);
		return NULL;
	}
	if ( (pCancel != NULL) &&
		xrtCancelRequested(pCancel) ) {
		__xrtHttpBodyFileCloseAsync(pAsync);
		xrtFree(pStatic);
		return NULL;
	}
	pStatic->RefCount = 1;
	pStatic->Info = Info;
	xrtAtomicPtrInit(&pStatic->File, pAsync);
	__xrtHttpStaticFileValidators(pStatic);
	return pStatic;
}



/* Future 成功值释放时归还静态文件资源引用。 */
static void __xrtHttpStaticFileValueFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpStaticFileDestroy(
		(xhttpstaticfile*)pValue
	);
}



/* 在任务池工作线程内执行受根约束的文件准备。 */
static xtaskoutcome __xrtHttpStaticFileTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_static_file_prepare* pPrepare =
		(xrt_http_static_file_prepare*)pData;
	xhttpstaticfile* pFile;

	pFile = __xrtHttpStaticFileOpen(
		pPrepare->Pool,
		pPrepare->Root,
		pPrepare->Path,
		pCancel
	);
	if ( pFile == NULL ) {
		return xrtCancelRequested(pCancel) ?
			XTASK_CANCELLED : XTASK_FAILED;
	}
	if ( xrtCancelRequested(pCancel) ) {
		xrtHttpStaticFileDestroy(pFile);
		return XTASK_CANCELLED;
	}
	pResult->Value = pFile;
	pResult->Destroy = __xrtHttpStaticFileValueFree;
	return XTASK_SUCCESS;
}



/* 释放任务池已经受理的静态文件准备参数。 */
static void __xrtHttpStaticFilePrepareFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtFree(pValue);
}



/* 同步打开根内静态文件资源。 */
XHTTP_SERVER_API xhttpstaticfile* xrtHttpStaticFileOpen(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
)
{
	return __xrtHttpStaticFileOpen(
		pPool,
		Root,
		sPath,
		NULL
	);
}



/* 复制根内路径并提交静态文件准备任务。 */
XHTTP_SERVER_API xfuture* xrtHttpStaticFileFuture(
	xtaskpool* pPool,
	xroot Root,
	cstr sPath
)
{
	xrt_http_static_file_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;
	size_t iPathLength;

	if ( (pPool == NULL) || (Root == NULL) ||
		(sPath == NULL) || (sPath[0] == '\0') ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	iPathLength = strlen(sPath);
	if ( iPathLength > (SIZE_MAX -
		sizeof(xrt_http_static_file_prepare) - 1u) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	pPrepare = (xrt_http_static_file_prepare*)xrtMalloc(
		sizeof(xrt_http_static_file_prepare) +
		iPathLength + 1u
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Root = Root;
	memcpy(
		pPrepare->Path,
		sPath,
		iPathLength + 1u
	);
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpStaticFilePrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpStaticFileTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpStaticFileWrap(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_SUBMIT,
			"submit",
			"failed to submit static file preparation"
		);
		xrtFree(pPrepare);
	}
	return pFuture;
}



/* 增加静态文件资源引用。 */
XHTTP_SERVER_API xhttpstaticfile* xrtHttpStaticFileRef(
	xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( xrtRefRetain(&pFile->RefCount) < 0 ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_REFERENCE,
			"retain",
			"the static file reference count cannot be increased",
			NULL
		);
		return NULL;
	}
	return pFile;
}



/* 释放资源引用，并异步关闭尚未取走的文件。 */
XHTTP_SERVER_API void xrtHttpStaticFileDestroy(
	xhttpstaticfile* pFile
)
{
	xasyncfile* pAsync;

	if ( (pFile == NULL) ||
		(xrtRefRelease(&pFile->RefCount) != 0) ) {
		return;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	__xrtHttpBodyFileCloseAsync(pAsync);
	xrtFree(pFile);
}



/* 返回最终文件句柄的借用元数据。 */
XHTTP_SERVER_API const xfileinfo* xrtHttpStaticFileInfo(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return &pFile->Info;
}



/* 返回与文件资源同生命周期的借用表示验证器。 */
XHTTP_SERVER_API const xhttprepresentation* xrtHttpStaticFileRepresentation(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return &pFile->Representation;
}



/* 返回完整静态文件长度。 */
XHTTP_SERVER_API uint64 xrtHttpStaticFileSize(
	const xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return 0;
	}
	return pFile->Info.Size;
}



/* 一次性取走静态文件持有的异步文件资源。 */
XHTTP_SERVER_API xasyncfile* xrtHttpStaticFileTakeFile(
	xhttpstaticfile* pFile
)
{
	xasyncfile* pAsync;

	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_CONSUMED,
			"take-file",
			"the static file resource has already been consumed",
			NULL
		);
	}
	return pAsync;
}



/* 一次性采用底层异步文件并创建严格区间正文。 */
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeBody(
	xhttpstaticfile* pFile,
	uint64 iOffset,
	uint64 iLength
)
{
	xasyncfile* pAsync;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( (iOffset > pFile->Info.Size) ||
		(iLength > (pFile->Info.Size - iOffset)) ||
		(iOffset > (uint64)INT64_MAX) ||
		(iLength > ((uint64)INT64_MAX - iOffset)) ) {
		__xrtHttpStaticFileError(
			XERR_RANGE,
			XHTTP_STATIC_FILE_ERROR_RANGE,
			"take-body",
			"the requested body range exceeds the opened static file",
			NULL
		);
		return NULL;
	}
	pAsync = (xasyncfile*)xrtAtomicPtrExchange(
		&pFile->File,
		NULL,
		XMEMORY_ACQ_REL
	);
	if ( pAsync == NULL ) {
		__xrtHttpStaticFileError(
			XERR_STATE,
			XHTTP_STATIC_FILE_ERROR_CONSUMED,
			"take-body",
			"the static file resource has already been consumed",
			NULL
		);
		return NULL;
	}
	pBody = xrtHttpBodyFileAdopt(
		pAsync,
		iOffset,
		iLength,
		NULL
	);
	if ( pBody == NULL ) {
		xerror* pCause = xrtTakeError();
		ptr pExpected = NULL;

		if ( !xrtAtomicPtrCompareExchange(
			&pFile->File,
			&pExpected,
			pAsync,
			XMEMORY_RELEASE,
			XMEMORY_ACQUIRE
		) ) {
			__xrtHttpBodyFileCloseAsync(pAsync);
		}
		__xrtHttpStaticFileError(
			XERR_IO,
			XHTTP_STATIC_FILE_ERROR_BODY,
			"take-body",
			"failed to create a body from the opened static file",
			pCause
		);
	}
	return pBody;
}



/* 一次性创建完整静态文件正文。 */
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeBodyAll(
	xhttpstaticfile* pFile
)
{
	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	return xrtHttpStaticFileTakeBody(
		pFile,
		0,
		pFile->Info.Size
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_static_multipart_body.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)



#if defined(XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY)

#define XRT_HTTP_STATIC_MULTIPART_HEAD   1u
#define XRT_HTTP_STATIC_MULTIPART_FILE   2u
#define XRT_HTTP_STATIC_MULTIPART_END    3u
#define XRT_HTTP_STATIC_MULTIPART_CLOSE  4u
#define XRT_HTTP_STATIC_MULTIPART_DONE   5u



/* 每个 Part 保存自己的文件范围和预生成头部位置。 */
typedef struct xrt_http_static_multipart_part {
	xhttpbyterange Range;
	size_t HeadOffset;
	size_t HeadSize;
} xrt_http_static_multipart_part;



/* 工厂在一个分配块中拥有范围表和全部固定线缆片段。 */
typedef struct xrt_http_static_multipart_factory {
	volatile int32 RefCount;
	xasyncfile* File;
	uint64 Length;
	size_t PartCount;
	size_t EndOffset;
	size_t EndSize;
	size_t CloseOffset;
	size_t CloseSize;
	xrt_http_static_multipart_part Parts[];
} xrt_http_static_multipart_factory;



/* Reader 在固定片段和共享异步文件 cursor 之间切换。 */
typedef struct xrt_http_static_multipart_reader {
	xrt_http_static_multipart_factory* Factory;
	xrt_http_body_file_cursor Cursor;
	size_t Part;
	size_t MetaOffset;
	size_t MetaRemaining;
	uint32 State;
} xrt_http_static_multipart_reader;



/* 设置静态多范围正文采用错误。 */
static void __xrtHttpStaticMultipartBodyError(
	xerrkind Kind,
	xhttpstaticmultipartbodyerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Domain = "http.static.multipart.body";
	Desc.Code = (int32)Code;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 返回工厂分配块中紧随 Part 表之后的线缆数据。 */
static bytes __xrtHttpStaticMultipartMetadata(
	xrt_http_static_multipart_factory* pFactory
)
{
	return (bytes)(
		pFactory->Parts + pFactory->PartCount
	);
}



/* 增加工厂引用，保护可能晚于 Reader 释放的元数据 Chunk。 */
static bool __xrtHttpStaticMultipartFactoryRef(
	xrt_http_static_multipart_factory* pFactory
)
{
	if ( (pFactory == NULL) ||
		(xrtRefRetain(&pFactory->RefCount) < 0) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	return true;
}



/* 释放工厂最后一个引用并关闭尚未打开 Reader 的文件。 */
static void __xrtHttpStaticMultipartFactoryRelease(
	xrt_http_static_multipart_factory* pFactory
)
{
	if ( (pFactory == NULL) ||
		(xrtRefRelease(&pFactory->RefCount) != 0) ) {
		return;
	}
	__xrtHttpBodyFileCloseAsync(pFactory->File);
	pFactory->File = NULL;
	xrtFree(pFactory);
}



/* 元数据 Chunk 释放时归还独立工厂引用。 */
static void __xrtHttpStaticMultipartChunkRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pData;
	(void)iSize;
	__xrtHttpStaticMultipartFactoryRelease(
		(xrt_http_static_multipart_factory*)pContext
	);
}



/* 安全累加单个工厂分配块的大小。 */
static bool __xrtHttpStaticMultipartAllocAdd(
	size_t* pSize,
	size_t iAdd
)
{
	if ( *pSize > (SIZE_MAX - iAdd) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 测量并分配一个尚未采用文件的多范围工厂。 */
static xrt_http_static_multipart_factory*
__xrtHttpStaticMultipartFactoryCreate(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	bytes pMetadata;
	uint64 iLength;
	size_t iAllocate = sizeof(*pFactory);
	size_t iBaseSize;
	size_t iMetadata = 0;
	size_t iMetadataCapacity;
	size_t iEndSize;
	size_t iCloseSize;
	size_t iWritten;
	xhttpbyterange Range;
	size_t i;

	if ( (iCompleteLength > (uint64)INT64_MAX) ||
		!xrtHttpBridgeRangeMultipartLength(
			pRanges,
			iRangeCount,
			iCompleteLength,
			ContentType,
			Boundary,
			&iLength
		) ) {
		if ( iCompleteLength > (uint64)INT64_MAX ) {
			xrtSetErrorKind(XERR_RANGE);
		}
		return NULL;
	}
	if ( iRangeCount >
		((SIZE_MAX - iAllocate) /
		 sizeof(xrt_http_static_multipart_part)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iAllocate += iRangeCount *
		sizeof(xrt_http_static_multipart_part);
	iBaseSize = iAllocate;
	for ( i = 0; i < iRangeCount; i++ ) {
		size_t iHeadSize;
		uint64 iRangeLength;

		memcpy(
			&Range,
			(const uint8*)pRanges + (i * sizeof(Range)),
			sizeof(Range)
		);
		iRangeLength =
			(Range.Last - Range.First) +
			UINT64_C(1);

		if ( !__xrtHttpBodyFileRangeValid(
				Range.First,
				iRangeLength
			) || !xrtHttpRangeMultipartHeadWrite(
				&Range,
				iCompleteLength,
				ContentType,
				Boundary,
				NULL,
				0,
				&iHeadSize
			) || !__xrtHttpStaticMultipartAllocAdd(
				&iMetadata,
				iHeadSize
			) ) {
			return NULL;
		}
	}
	if ( !xrtHttpRangeMultipartEndWrite(
			NULL,
			0,
			&iEndSize
		) || !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			NULL,
			0,
			&iCloseSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iMetadata,
			iEndSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iMetadata,
			iCloseSize
		) || !__xrtHttpStaticMultipartAllocAdd(
			&iAllocate,
			iMetadata
		) ) {
		return NULL;
	}
	pFactory = (xrt_http_static_multipart_factory*)
		xrtCalloc(1, iAllocate);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pFactory->RefCount = 1;
	pFactory->Length = iLength;
	pFactory->PartCount = iRangeCount;
	pMetadata = __xrtHttpStaticMultipartMetadata(
		pFactory
	);
	iMetadataCapacity = iAllocate - iBaseSize;
	iMetadata = 0;
	for ( i = 0; i < iRangeCount; i++ ) {
		memcpy(
			&Range,
			(const uint8*)pRanges + (i * sizeof(Range)),
			sizeof(Range)
		);
		pFactory->Parts[i].Range = Range;
		pFactory->Parts[i].HeadOffset = iMetadata;
		if ( !xrtHttpRangeMultipartHeadWrite(
				&pFactory->Parts[i].Range,
				iCompleteLength,
				ContentType,
				Boundary,
				pMetadata + iMetadata,
				iMetadataCapacity - iMetadata,
				&iWritten
			) ) {
			__xrtHttpStaticMultipartFactoryRelease(
				pFactory
			);
			return NULL;
		}
		pFactory->Parts[i].HeadSize = iWritten;
		iMetadata += iWritten;
	}
	pFactory->EndOffset = iMetadata;
	if ( !xrtHttpRangeMultipartEndWrite(
			pMetadata + iMetadata,
			iEndSize,
			&iWritten
		) || (iWritten != iEndSize) ) {
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	pFactory->EndSize = iWritten;
	iMetadata += iWritten;
	pFactory->CloseOffset = iMetadata;
	if ( !xrtHttpRangeMultipartCloseWrite(
			Boundary,
			pMetadata + iMetadata,
			iCloseSize,
			&iWritten
		) || (iWritten != iCloseSize) ) {
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	pFactory->CloseSize = iWritten;
	iMetadata += iWritten;
	if ( iMetadata != iMetadataCapacity ) {
		xrtSetErrorKind(XERR_INTERNAL);
		__xrtHttpStaticMultipartFactoryRelease(pFactory);
		return NULL;
	}
	return pFactory;
}



/* 开始发布一个工厂内固定线缆片段。 */
static void __xrtHttpStaticMultipartMetaBegin(
	xrt_http_static_multipart_reader* pReader,
	size_t iOffset,
	size_t iSize
)
{
	pReader->MetaOffset = iOffset;
	pReader->MetaRemaining = iSize;
}



/* 发布不超过调用方上限的固定线缆 Chunk。 */
static xhttpbodystatus __xrtHttpStaticMultipartMetaNext(
	xrt_http_static_multipart_reader* pReader,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	size_t iSize;

	if ( pReader->MetaRemaining == 0 ) {
		return XHTTP_BODY_EOF;
	}
	if ( !__xrtHttpStaticMultipartFactoryRef(
		pReader->Factory
	) ) {
		return XHTTP_BODY_ERROR;
	}
	iSize = pReader->MetaRemaining < iMaxBytes ?
		pReader->MetaRemaining : iMaxBytes;
	pChunk->Data = __xrtHttpStaticMultipartMetadata(
		pReader->Factory
	) + pReader->MetaOffset;
	pChunk->Size = iSize;
	pChunk->Release =
		__xrtHttpStaticMultipartChunkRelease;
	pChunk->Context = pReader->Factory;
	pReader->MetaOffset += iSize;
	pReader->MetaRemaining -= iSize;
	return XHTTP_BODY_DATA;
}



/* 推进 Part 头、文件数据、段尾和关闭边界状态机。 */
static xhttpbodystatus __xrtHttpStaticMultipartReaderNext(
	ptr pContext,
	size_t iMaxBytes,
	xhttpbodychunk* pChunk
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;

	if ( pReader == NULL ) {
		xrtSetErrorKind(XERR_STATE);
		return XHTTP_BODY_ERROR;
	}
	for ( ;; ) {
		xhttpbodystatus Status;

		switch ( pReader->State ) {
			case XRT_HTTP_STATIC_MULTIPART_HEAD:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				__xrtHttpBodyFileCursorInit(
					&pReader->Cursor,
					pReader->Factory->File,
					pReader->Factory
						->Parts[pReader->Part]
						.Range.First,
					(pReader->Factory
						->Parts[pReader->Part]
						.Range.Last -
					 pReader->Factory
						->Parts[pReader->Part]
						.Range.First) +
						UINT64_C(1),
					XHTTP_BODY_FILE_READ_DEFAULT
				);
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_FILE;
				break;

			case XRT_HTTP_STATIC_MULTIPART_FILE:
				Status = __xrtHttpBodyFileCursorNext(
					&pReader->Cursor,
					iMaxBytes,
					pChunk
				);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				__xrtHttpStaticMultipartMetaBegin(
					pReader,
					pReader->Factory->EndOffset,
					pReader->Factory->EndSize
				);
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_END;
				break;

			case XRT_HTTP_STATIC_MULTIPART_END:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				pReader->Part++;
				if ( pReader->Part <
					pReader->Factory->PartCount ) {
					__xrtHttpStaticMultipartMetaBegin(
						pReader,
						pReader->Factory
							->Parts[pReader->Part]
							.HeadOffset,
						pReader->Factory
							->Parts[pReader->Part]
							.HeadSize
					);
					pReader->State =
						XRT_HTTP_STATIC_MULTIPART_HEAD;
				} else {
					__xrtHttpStaticMultipartMetaBegin(
						pReader,
						pReader->Factory->CloseOffset,
						pReader->Factory->CloseSize
					);
					pReader->State =
						XRT_HTTP_STATIC_MULTIPART_CLOSE;
				}
				break;

			case XRT_HTTP_STATIC_MULTIPART_CLOSE:
				Status =
					__xrtHttpStaticMultipartMetaNext(
						pReader,
						iMaxBytes,
						pChunk
					);
				if ( Status != XHTTP_BODY_EOF ) {
					return Status;
				}
				pReader->State =
					XRT_HTTP_STATIC_MULTIPART_DONE;
				break;

			case XRT_HTTP_STATIC_MULTIPART_DONE:
				return XHTTP_BODY_EOF;

			default:
				xrtSetErrorKind(XERR_STATE);
				return XHTTP_BODY_ERROR;
		}
	}
}



/* 在文件 cursor 返回 AGAIN 后取得当前读取 Future。 */
static xfuture* __xrtHttpStaticMultipartReaderWait(
	ptr pContext
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;

	if ( (pReader == NULL) ||
		(pReader->State !=
		 XRT_HTTP_STATIC_MULTIPART_FILE) ) {
		xrtSetErrorKind(XERR_STATE);
		return NULL;
	}
	return __xrtHttpBodyFileCursorWait(
		&pReader->Cursor
	);
}



/* 取消待处理读取并异步关闭工厂采用的文件。 */
static void __xrtHttpStaticMultipartReaderClose(
	ptr pContext
)
{
	xrt_http_static_multipart_reader* pReader =
		(xrt_http_static_multipart_reader*)pContext;
	xasyncfile* pFile;

	if ( pReader == NULL ) {
		return;
	}
	__xrtHttpBodyFileCursorCancel(&pReader->Cursor);
	pFile = pReader->Factory->File;
	pReader->Factory->File = NULL;
	__xrtHttpBodyFileCloseAsync(pFile);
	xrtFree(pReader);
}



/* 快速创建一个从首个 Part 头开始的单消费 Reader。 */
static bool __xrtHttpStaticMultipartBodyOpen(
	ptr pFactory,
	xhttpbodyreaderops* pOps,
	ptr* ppReader
)
{
	xrt_http_static_multipart_factory* pMultipart =
		(xrt_http_static_multipart_factory*)pFactory;
	xrt_http_static_multipart_reader* pReader;

	if ( (pMultipart == NULL) ||
		(pMultipart->File == NULL) ||
		(pMultipart->PartCount == 0) ||
		(pOps == NULL) || (ppReader == NULL) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	pReader = (xrt_http_static_multipart_reader*)
		xrtCalloc(1, sizeof(*pReader));
	if ( pReader == NULL ) {
		return false;
	}
	pReader->Factory = pMultipart;
	pReader->State = XRT_HTTP_STATIC_MULTIPART_HEAD;
	__xrtHttpStaticMultipartMetaBegin(
		pReader,
		pMultipart->Parts[0].HeadOffset,
		pMultipart->Parts[0].HeadSize
	);
	memset(pOps, 0, sizeof(*pOps));
	pOps->Next = __xrtHttpStaticMultipartReaderNext;
	pOps->Close = __xrtHttpStaticMultipartReaderClose;
	pOps->Wait = __xrtHttpStaticMultipartReaderWait;
	*ppReader = pReader;
	return true;
}



/* 正文释放时归还工厂自身的初始引用。 */
static void __xrtHttpStaticMultipartBodyDestroy(
	ptr pFactory
)
{
	__xrtHttpStaticMultipartFactoryRelease(
		(xrt_http_static_multipart_factory*)pFactory
	);
}



/* 为已完成的工厂创建不可重放正文对象。 */
static xhttpbody* __xrtHttpStaticMultipartBodyCreate(
	xrt_http_static_multipart_factory* pFactory
)
{
	static const xhttpbodyops Ops = {
		__xrtHttpStaticMultipartBodyOpen,
		__xrtHttpStaticMultipartBodyDestroy
	};

	return xrtHttpBodyCreate(
		&Ops,
		pFactory,
		pFactory->Length,
		XHTTP_BODY_NONE
	);
}



/* 采用可读异步文件并创建多范围正文。 */
XHTTP_SERVER_API xhttpbody* xrtHttpStaticMultipartBodyAdopt(
	xasyncfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( (xrtAsyncFileFlags(pFile) & XFILE_READ) == 0u ) {
		__xrtHttpStaticMultipartBodyError(
			XERR_ARGUMENT,
			XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT,
			"adopt",
			"the adopted multipart file is not readable"
		);
		return NULL;
	}
	pFactory = __xrtHttpStaticMultipartFactoryCreate(
		pRanges,
		iRangeCount,
		iCompleteLength,
		ContentType,
		Boundary
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pBody = __xrtHttpStaticMultipartBodyCreate(
		pFactory
	);
	if ( pBody == NULL ) {
		__xrtHttpStaticMultipartFactoryRelease(
			pFactory
		);
		return NULL;
	}
	pFactory->File = pFile;
	return pBody;
}



/* 构造成功后才从静态文件资源取走底层异步文件。 */
XHTTP_SERVER_API xhttpbody* xrtHttpStaticFileTakeMultipartBody(
	xhttpstaticfile* pFile,
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	xstrview ContentType,
	xstrview Boundary
)
{
	xrt_http_static_multipart_factory* pFactory;
	xasyncfile* pAsync;
	xhttpbody* pBody;

	if ( pFile == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pFactory = __xrtHttpStaticMultipartFactoryCreate(
		pRanges,
		iRangeCount,
		xrtHttpStaticFileSize(pFile),
		ContentType,
		Boundary
	);
	if ( pFactory == NULL ) {
		return NULL;
	}
	pBody = __xrtHttpStaticMultipartBodyCreate(
		pFactory
	);
	if ( pBody == NULL ) {
		__xrtHttpStaticMultipartFactoryRelease(
			pFactory
		);
		return NULL;
	}
	pAsync = xrtHttpStaticFileTakeFile(pFile);
	if ( pAsync == NULL ) {
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pFactory->File = pAsync;
	return pBody;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_sse_server.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SSE_SERVER)




#if defined(XHTTP_FEATURE_HTTP_SSE_SERVER)

/* 直接把事件写入 Body Stream 已分配的节点载荷。 */
static bool __xrtHttpSseServerEventFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	size_t iWritten;

	return xrtHttpSseEventWrite(
		(const xhttpsseevent*)pData,
		pOutput,
		iSize,
		&iWritten
	) && (iWritten == iSize);
}



/* 直接把注释写入 Body Stream 已分配的节点载荷。 */
static bool __xrtHttpSseServerCommentFill(
	void* pOutput,
	size_t iSize,
	ptr pData
)
{
	xstrview* pComment = (xstrview*)pData;
	size_t iWritten;

	return xrtHttpSseCommentWrite(
		*pComment,
		pOutput,
		iSize,
		&iWritten
	) && (iWritten == iSize);
}



/* 组合 Reply、规范 SSE Content-Type 与有界异步正文。 */
XHTTP_SERVER_API xhttpreply* xrtHttpSseReplyCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
)
{
	xhttpbodystream* pStream = NULL;
	xhttpbodystream* pOutput = NULL;
	xhttpbody* pBody;
	xhttpreply* pReply;
	xhttpheaders* pHeaders;

	if ( !xrtMemRangeValid(ppStream, sizeof(pOutput)) ||
		((pConfig != NULL) && !xrtMemRangeValid(
			pConfig, sizeof(*pConfig)
		)) || ((pConfig != NULL) && xrtMemRangesOverlap(
			ppStream,
			sizeof(pOutput),
			pConfig,
			sizeof(*pConfig)
		)) ) {
		__xhttpErrorSetInvalidArgument();
		return NULL;
	}
	memcpy((void*)ppStream, &pOutput, sizeof(pOutput));
	pBody = xrtHttpBodyStreamCreate(pConfig, &pStream);
	if ( pBody == NULL ) {
		return NULL;
	}
	pReply = xrtHttpReplyCreate(200);
	if ( pReply == NULL ) {
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	pHeaders = xrtHttpReplyEditHeaders(pReply);
	if ( (pHeaders == NULL) ||
		!xrtHttpSseResponseHeaders(pHeaders) ||
		!xrtHttpReplySetBody(pReply, pBody) ) {
		xrtHttpReplyDestroy(pReply);
		xrtHttpBodyStreamDestroy(pStream);
		xrtHttpBodyDestroy(pBody);
		return NULL;
	}
	xrtHttpBodyDestroy(pBody);
	memcpy((void*)ppStream, &pStream, sizeof(pStream));
	return pReply;
}



/* 封装最常用的只有 data 字段的默认事件。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSend(
	xhttpbodystream* pStream,
	xstrview Data
)
{
	xhttpsseevent Event;

	memset(&Event, 0, sizeof(Event));
	Event.Data = Data;
	Event.Flags = XHTTP_SSE_EVENT_DATA;
	return xrtHttpSseSendEvent(pStream, &Event);
}



/* 计量后在一个队列节点中完成事件编码和提交。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSendEvent(
	xhttpbodystream* pStream,
	const xhttpsseevent* pEvent
)
{
	size_t iSize;

	if ( !xrtHttpSseEventSize(pEvent, &iSize) ) {
		return XHTTP_BODY_STREAM_ERROR;
	}
	return xrtHttpBridgeBodyStreamBuild(
		pStream,
		iSize,
		__xrtHttpSseServerEventFill,
		(ptr)pEvent
	);
}



/* 计量后在一个队列节点中完成注释编码和提交。 */
XHTTP_SERVER_API xhttpbodystreamresult xrtHttpSseSendComment(
	xhttpbodystream* pStream,
	xstrview Comment
)
{
	size_t iSize;

	if ( !xrtHttpSseCommentSize(Comment, &iSize) ) {
		return XHTTP_BODY_STREAM_ERROR;
	}
	return xrtHttpBridgeBodyStreamBuild(
		pStream,
		iSize,
		__xrtHttpSseServerCommentFill,
		&Comment
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_runtime.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER)



#if defined(XHTTP_FEATURE_HTTP_SERVER)

#define XRT_HTTP_SERVER_WRITE_DEFAULT ((size_t)16384)
#define XRT_HTTP_SERVER_HEADER_TIMEOUT_DEFAULT INT64_C(10000)
#define XRT_HTTP_SERVER_BODY_TIMEOUT_DEFAULT INT64_C(30000)
#define XRT_HTTP_SERVER_REQUEST_TIMEOUT_DEFAULT INT64_C(30000)
#define XRT_HTTP_SERVER_IDLE_TIMEOUT_DEFAULT INT64_C(60000)
#define XRT_HTTP_SERVER_WRITE_TIMEOUT_DEFAULT INT64_C(30000)
#define XRT_HTTP_SERVER_INFORMATION_DEFAULT ((size_t)16)



/* 建立 Server 域错误并保留底层原因。 */
xerror* __xrtHttpServerErrorCreate(
	xerrkind Kind,
	xhttpservererror Code,
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
	Desc.Domain = "xrt.http.server";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 设置当前线程的 Server 同步错误。 */
void __xrtHttpServerSetError(
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError = __xrtHttpServerErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		pCause
	);

	xrtSetError(pError);
	xrtErrorFree(pError);
}



/* 沿原因链选取最内层有效错误类别。 */
xerrkind __xrtHttpServerCauseKind(
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



/* 保存连接第一个稳定错误。 */
void __xrtHttpConnRememberError(
	xhttpconn* pConnection,
	xerrkind Kind,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerror* pError;
	ptr pExpected = NULL;

	if ( pConnection == NULL ) {
		return;
	}
	pError = __xrtHttpServerErrorCreate(
		__xrtHttpServerCauseKind(pCause, Kind),
		Code,
		sOperation,
		sMessage,
		pCause
	);
	if ( pError == NULL ) {
		return;
	}
	if ( !xrtAtomicPtrCompareExchange(
		&pConnection->Error,
		&pExpected,
		pError,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtErrorFree(pError);
		pError = (xerror*)pExpected;
	}
	xrtSetError(pError);
}



/* 原子提高连接峰值。 */
static void __xrtHttpServerPeak(
	xhttpserver* pServer,
	uint64 iValue
)
{
	uint64 iPeak = xrtAtomic64Load(
		&pServer->PeakConnections,
		XMEMORY_RELAXED
	);

	while ( (iValue > iPeak) &&
		!xrtAtomic64CompareExchange(
			&pServer->PeakConnections,
			&iPeak,
			iValue,
			XMEMORY_RELAXED,
			XMEMORY_RELAXED
		) ) {
	}
}



/* 把已保留计数的 Connection 加入 Server 列表。 */
void __xrtHttpServerAddConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
)
{
	uint64 iConnections = xrtAtomic64Load(
		&pServer->Connections,
		XMEMORY_RELAXED
	);

	xrtSpinLock(&pServer->Lock);
	pConnection->Previous = pServer->Tail;
	pConnection->Next = NULL;
	if ( pServer->Tail != NULL ) {
		pServer->Tail->Next = pConnection;
	} else {
		pServer->Head = pConnection;
	}
	pServer->Tail = pConnection;
	pConnection->Listed = true;
	pConnection->Counted = true;
	xrtSpinUnlock(&pServer->Lock);
	__xrtHttpServerPeak(pServer, iConnections);
}



/* 从 Server 的可遍历列表摘除 Connection，但保留尚未退出的运行时计数。 */
void __xrtHttpServerRemoveConnection(
	xhttpserver* pServer,
	xhttpconn* pConnection
)
{
	xrtSpinLock(&pServer->Lock);
	if ( pConnection->Listed ) {
		if ( pConnection->Previous != NULL ) {
			pConnection->Previous->Next = pConnection->Next;
		} else {
			pServer->Head = pConnection->Next;
		}
		if ( pConnection->Next != NULL ) {
			pConnection->Next->Previous =
				pConnection->Previous;
		} else {
			pServer->Tail = pConnection->Previous;
		}
		pConnection->Previous = NULL;
		pConnection->Next = NULL;
		pConnection->Listed = false;
	}
	xrtSpinUnlock(&pServer->Lock);

}



/* 在发布 Connection Close 前归还公开活跃连接计数。 */
void __xrtHttpServerConnectionClosed(xhttpserver* pServer)
{
	(void)xrtAtomic64FetchSub(
		&pServer->Connections,
		1,
		XMEMORY_ACQ_REL
	);
}



/* 最后归还 Connection 生命周期计数，再尝试发布 Server 关闭终态。 */
void __xrtHttpServerConnectionFinished(xhttpserver* pServer)
{
	(void)xrtAtomic64FetchSub(
		&pServer->RuntimeConnections,
		1,
		XMEMORY_ACQ_REL
	);
	__xrtHttpServerTryFinish(pServer);
}



/* 增加 Server 引用。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerRef(xhttpserver* pServer)
{
	if ( (pServer == NULL) ||
		(xrtRefRetain(&pServer->References) < 0) ) {
		__xrtHttpServerSetError(
			pServer == NULL ? XERR_ARGUMENT : XERR_STATE,
			pServer == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			"retain-http-server",
			"HTTP server is null or no longer retainable",
			NULL
		);
		return NULL;
	}
	return pServer;
}



/* 释放 Server 最终存储。 */
XHTTP_SERVER_API void xrtHttpServerDestroy(xhttpserver* pServer)
{
	if ( (pServer == NULL) ||
		(xrtRefRelease(&pServer->References) != 0) ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		__xrtHttpServerTlsCleanup(pServer);
	#endif
	xrtErrorFree((xerror*)xrtAtomicPtrLoad(
		&pServer->NetworkError,
		XMEMORY_ACQUIRE
	));
	xrtNetServerDestroy(pServer->Network);
	xrtSpinUnit(&pServer->Lock);
	memset(pServer, 0, sizeof(*pServer));
	xrtFree(pServer);
}



/* 连接和聚合 TCP Server 全部退出后发布唯一关闭事件。 */
void __xrtHttpServerTryFinish(xhttpserver* pServer)
{
	uint32 iState;
	uint32 iExpected;
	bool bPublished;

	if ( (pServer == NULL) ||
		!xrtAtomic32Load(
			&pServer->NetworkClosed,
			XMEMORY_ACQUIRE
		) ||
		(xrtAtomic64Load(
			&pServer->RuntimeConnections,
			XMEMORY_ACQUIRE
		) != 0) ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		if ( xrtAtomic64Load(
			&pServer->UpgradeCallbacks,
			XMEMORY_ACQUIRE
		) != 0 ) {
			return;
		}
	#endif
	iState = xrtAtomic32Load(
		&pServer->State,
		XMEMORY_ACQUIRE
	);
	if ( (iState != XHTTP_SERVER_DRAINING) &&
		(iState != XHTTP_SERVER_ABORTING) ) {
		return;
	}
	iExpected = iState;
	if ( !xrtAtomic32CompareExchange(
		&pServer->State,
		&iExpected,
		XHTTP_SERVER_CLOSED,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	bPublished = xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) != 0;
	if ( bPublished && (pServer->Events.Shutdown != NULL) ) {
		pServer->Events.Shutdown(
			pServer, pServer->Events.Data
		);
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		if ( bPublished ) {
			xrtAtomic32Store(
				&pServer->ShutdownPublished,
				1,
				XMEMORY_RELEASE
			);
			__xrtHttpServerFutureFinish(pServer);
		}
	#endif
	if ( pServer->RuntimeHeld ) {
		pServer->RuntimeHeld = false;
		xrtHttpServerDestroy(pServer);
	}
}



/* 保存并提升聚合 TCP Server 错误。 */
static void __xrtHttpServerNetworkError(
	xnetserver* pNetwork,
	size_t iEndpoint,
	const xerror* pCause,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xerror* pError;
	ptr pExpected = NULL;
	bool bStored = false;

	(void)pNetwork;
	(void)iEndpoint;
	pError = __xrtHttpServerErrorCreate(
		__xrtHttpServerCauseKind(pCause, XERR_IO),
		XHTTP_SERVER_ERROR_LISTEN,
		"listen-http-server",
		"HTTP server network failed",
		pCause
	);
	if ( pError != NULL ) {
		bStored = xrtAtomicPtrCompareExchange(
			&pServer->NetworkError,
			&pExpected,
			pError,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		);
	}
	if ( (pError != NULL) &&
		 xrtAtomic32Load(
			&pServer->NetworkPublished,
			XMEMORY_ACQUIRE
		 ) && (pServer->Events.Error != NULL) ) {
		pServer->Events.Error(
			pServer,
			NULL,
			pError,
			pServer->Events.Data
		);
	}
	if ( !bStored ) {
		xrtErrorFree(pError);
	}
}



/* 聚合 TCP Server 关闭后封闭 HTTP 接入，并终止意外中断的连接。 */
static void __xrtHttpServerNetworkClose(
	xnetserver* pNetwork,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xhttpserver* pOwned = xrtHttpServerRef(pServer);
	bool bPublished;

	(void)pNetwork;
	if ( pOwned == NULL ) {
		return;
	}
	xrtSpinLock(&pServer->Lock);
	xrtAtomic32Store(
		&pServer->NetworkClosed,
		1,
		XMEMORY_RELEASE
	);
	bPublished = xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) != 0;
	xrtSpinUnlock(&pServer->Lock);
	if ( bPublished &&
		 (xrtHttpServerState(pServer) == XHTTP_SERVER_RUNNING) ) {
		(void)xrtHttpServerAbort(pServer);
	}
	__xrtHttpServerTryFinish(pServer);
	xrtHttpServerDestroy(pOwned);
}



/* 为一个新连接原子保留 MaxConnections 预算。 */
static bool __xrtHttpServerReserveConnection(
	xhttpserver* pServer
)
{
	uint64 iPrevious;

	if ( xrtHttpServerState(pServer) != XHTTP_SERVER_RUNNING ) {
		return false;
	}
	iPrevious = xrtAtomic64FetchAdd(
		&pServer->Connections,
		1,
		XMEMORY_ACQ_REL
	);
	(void)xrtAtomic64FetchAdd(
		&pServer->RuntimeConnections,
		1,
		XMEMORY_ACQ_REL
	);
	if ( (pServer->Config.MaxConnections != 0) &&
		(iPrevious >=
		 (uint64)pServer->Config.MaxConnections) ) {
		(void)xrtAtomic64FetchSub(
			&pServer->Connections,
			1,
			XMEMORY_ACQ_REL
		);
		(void)xrtAtomic64FetchSub(
			&pServer->RuntimeConnections,
			1,
			XMEMORY_ACQ_REL
		);
		return false;
	}
	return true;
}



/* 接管聚合 TCP Server 转移的 Stream 引用并建立 HTTP Connection。 */
static bool __xrtHttpServerNetworkAccept(
	xnetserver* pNetwork,
	size_t iEndpoint,
	xnetstream* pStream,
	ptr pData
)
{
	xhttpserver* pServer = (xhttpserver*)pData;
	xhttpconn* pConnection;
	xerror* pError;

	(void)pNetwork;
	if ( !xrtAtomic32Load(
		&pServer->NetworkPublished,
		XMEMORY_ACQUIRE
	) ) {
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	if ( !__xrtHttpServerReserveConnection(pServer) ) {
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pServer->Secure ) {
			pConnection = __xrtHttpConnCreateTls(
				pServer, iEndpoint, pStream
			);
		} else {
			pConnection = __xrtHttpConnCreateTcp(
				pServer, iEndpoint, pStream
			);
		}
	#else
		pConnection = __xrtHttpConnCreateTcp(
			pServer, iEndpoint, pStream
		);
	#endif
	if ( pConnection == NULL ) {
		pError = xrtTakeError();
		(void)xrtAtomic64FetchAdd(
			&pServer->Rejected,
			1,
			XMEMORY_RELAXED
		);
		if ( (pError != NULL) &&
			(pServer->Events.Error != NULL) ) {
			pServer->Events.Error(
				pServer,
				NULL,
				pError,
				pServer->Events.Data
			);
		}
		xrtSetError(pError);
		xrtErrorFree(pError);
		__xrtHttpServerConnectionClosed(pServer);
		__xrtHttpServerConnectionFinished(pServer);
		return false;
	}
	__xrtHttpServerAddConnection(pServer, pConnection);
	(void)xrtAtomic64FetchAdd(
		&pServer->Accepted,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtHttpServerState(pServer) != XHTTP_SERVER_RUNNING ) {
		(void)xrtHttpConnClose(pConnection);
	}
	return true;
}



/* 返回静态聚合 TCP Server 事件表。 */
static const xnetserverevents* __xrtHttpServerNetworkEvents(void)
{
	static const xnetserverevents Events = {
		__xrtHttpServerNetworkAccept,
		__xrtHttpServerNetworkError,
		__xrtHttpServerNetworkClose
	};

	return &Events;
}



/* 验证公开输出不会覆盖 Server 状态或紧随其后的端点地址。 */
static bool __xrtHttpServerOutputValid(
	const xhttpserver* pServer,
	const void* pOutput,
	size_t iSize
)
{
	if ( !xrtMemRangeValid(pOutput, iSize) ) {
		return false;
	}
	if ( pServer == NULL ) {
		return true;
	}
	return !xrtMemRangesOverlap(
		pOutput, iSize, pServer, sizeof(*pServer)
	) && !xrtMemRangesOverlap(
		pOutput,
		iSize,
		pServer->Locals,
		pServer->EndpointCount * sizeof(xnetaddr)
	);
}



/* 初始化 Server 默认策略。 */
XHTTP_SERVER_API void xrtHttpServerConfigInit(xhttpserverconfig* pConfig)
{
	xhttpserverconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-http-server-config",
			"HTTP server config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtNetServerConfigInit(&Config.Network);
	xrtHttp1ServerConfigInit(&Config.Http1);
	Config.WriteSize = XRT_HTTP_SERVER_WRITE_DEFAULT;
	Config.HeaderTimeout =
		XRT_HTTP_SERVER_HEADER_TIMEOUT_DEFAULT;
	Config.BodyTimeout =
		XRT_HTTP_SERVER_BODY_TIMEOUT_DEFAULT;
	Config.RequestTimeout =
		XRT_HTTP_SERVER_REQUEST_TIMEOUT_DEFAULT;
	Config.IdleTimeout =
		XRT_HTTP_SERVER_IDLE_TIMEOUT_DEFAULT;
	Config.WriteTimeout =
		XRT_HTTP_SERVER_WRITE_TIMEOUT_DEFAULT;
	Config.MaxInformations =
		XRT_HTTP_SERVER_INFORMATION_DEFAULT;
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 初始化空 Server 事件表。 */
XHTTP_SERVER_API void xrtHttpServerEventsInit(xhttpserverevents* pEvents)
{
	const xhttpserverevents Events = { 0 };

	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-http-server-events",
			"HTTP server events range is invalid",
			NULL
		);
		return;
	}
	memcpy((void*)pEvents, &Events, sizeof(Events));
}



/* 检查全部逻辑端点的 TCP 写入硬上限。 */
bool __xrtHttpServerNetworkWriteLimit(
	const xhttpserverconfig* pConfig,
	size_t iMinimum
)
{
	size_t iEndpoints = pConfig->Network.AdditionalCount + 1u;

	for ( size_t i = 0; i < iEndpoints; i++ ) {
		xnetlistenconfig Listen;

		if ( i == 0 ) {
			memcpy(
				&Listen,
				&pConfig->Network.Listen,
				sizeof(Listen)
			);
		} else {
			memcpy(
				&Listen,
				(const void*)(pConfig->Network.Additional + (i - 1u)),
				sizeof(Listen)
			);
		}
		if ( Listen.Stream.WriteLimit < iMinimum ) {
			return false;
		}
	}
	return true;
}



/* 检查必须在监听前确定的 Server 组合边界。 */
static bool __xrtHttpServerConfigValid(
	const xhttpserverconfig* pConfig
)
{
	size_t iAdditionalBytes;

	if ( (pConfig == NULL) ||
		(pConfig->WriteSize == 0) ||
		(pConfig->MaxInformations == 0) ||
		(pConfig->Network.AdditionalCount == SIZE_MAX) ||
		(pConfig->Network.AdditionalCount >
		 (SIZE_MAX / sizeof(xnetlistenconfig))) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP server configuration is invalid",
			NULL
		);
		return false;
	}
	iAdditionalBytes = pConfig->Network.AdditionalCount *
		sizeof(xnetlistenconfig);
	if ( !xrtMemRangeValid(
		pConfig->Network.Additional,
		iAdditionalBytes
	) || !__xrtHttpServerNetworkWriteLimit(
		pConfig,
		pConfig->WriteSize
	) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP write size must fit every TCP endpoint write limit",
			NULL
		);
		return false;
	}
	if ( !xrtHttpBridge1ServerConfigValid(&pConfig->Http1) ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(), XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-http-server",
			"HTTP server Exchange configuration is invalid",
			xrtGetError()
		);
		return false;
	}
	return true;
}



/* 验证配置并创建尚未开始监听的 Server。 */
xhttpserver* __xrtHttpServerCreate(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
)
{
	xhttpserverconfig Config;
	xhttpserverevents Events;
	xhttpserver* pServer;
	size_t iEndpointCount;
	size_t iTotal;

	xrtHttpServerConfigInit(&Config);
	xrtHttpServerEventsInit(&Events);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, (const void*)pConfig, sizeof(Config));
	}
	if ( pEvents != NULL ) {
		if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server events range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Events, (const void*)pEvents, sizeof(Events));
	}
	if ( (pEngine == NULL) ||
		!__xrtHttpServerConfigValid(&Config) ) {
		if ( pEngine == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"start-http-server",
				"HTTP server requires a network engine",
				NULL
			);
		}
		return NULL;
	}
	iEndpointCount = Config.Network.AdditionalCount + 1u;
	if ( iEndpointCount >
		 ((SIZE_MAX - sizeof(*pServer)) / sizeof(xnetaddr)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return NULL;
	}
	iTotal = sizeof(*pServer) +
		(iEndpointCount * sizeof(xnetaddr));
	pServer = (xhttpserver*)xrtCalloc(1, iTotal);
	if ( pServer == NULL ) {
		return NULL;
	}
	pServer->References = 1;
	xrtAtomic32Init(&pServer->State, XHTTP_SERVER_RUNNING);
	xrtAtomic64Init(&pServer->Accepted, 0);
	xrtAtomic64Init(&pServer->Rejected, 0);
	xrtAtomic64Init(&pServer->Requests, 0);
	xrtAtomic64Init(&pServer->Responses, 0);
	xrtAtomic64Init(&pServer->Informations, 0);
	xrtAtomic64Init(&pServer->Upgraded, 0);
	xrtAtomic64Init(&pServer->ProtocolErrors, 0);
	xrtAtomic64Init(&pServer->Timeouts, 0);
	xrtAtomic64Init(&pServer->Connections, 0);
	xrtAtomic64Init(&pServer->RuntimeConnections, 0);
	xrtAtomic64Init(&pServer->PeakConnections, 0);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		xrtAtomic64Init(&pServer->UpgradeCallbacks, 0);
	#endif
	xrtAtomic32Init(&pServer->NetworkClosed, 0);
	xrtAtomic32Init(&pServer->NetworkPublished, 0);
	xrtAtomicPtrInit(&pServer->NetworkError, NULL);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		xrtAtomic32Init(&pServer->ShutdownPublished, 0);
	#endif
	xrtSpinInit(&pServer->Lock);
	pServer->Engine = pEngine;
	pServer->Locals = (xnetaddr*)(pServer + 1u);
	pServer->Config = Config;
	pServer->Events = Events;
	pServer->EndpointCount = iEndpointCount;
	return pServer;
}



/* 启动聚合 TCP Server，并建立独立 HTTP 运行时引用。 */
xhttpserver* __xrtHttpServerListen(xhttpserver* pServer)
{
	const xnetstreamevents* pStreamEvents;
	xnetserver* pNetwork;
	xerror* pCause;
	bool bPublished = false;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"listen-http-server",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	if ( xrtHttpServerRef(pServer) == NULL ) {
		xrtHttpServerDestroy(pServer);
		return NULL;
	}
	pServer->RuntimeHeld = true;
	pStreamEvents = pServer->Secure ?
		NULL : __xrtHttpConnStreamEvents();
	pNetwork = xrtNetServerStart(
		pServer->Engine,
		&pServer->Config.Network,
		__xrtHttpServerNetworkEvents(),
		pStreamEvents,
		pServer
	);
	if ( pNetwork == NULL ) {
		pCause = xrtErrorRef(xrtGetError());

		pServer->RuntimeHeld = false;
		xrtHttpServerDestroy(pServer);
		xrtHttpServerDestroy(pServer);
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(pCause, XERR_IO),
			XHTTP_SERVER_ERROR_LISTEN,
			"start-http-server",
			"HTTP server could not start its TCP listener",
			pCause
		);
		xrtErrorFree(pCause);
		return NULL;
	}
	pServer->Network = pNetwork;
	pServer->ListenerCount = xrtNetServerListenerCount(pNetwork);
	pServer->Config.Network.Additional = NULL;
	pServer->Config.Network.AdditionalCount = 0;
	for ( size_t i = 0; i < pServer->EndpointCount; i++ ) {
		if ( !xrtNetServerLocal(pNetwork, i, &pServer->Locals[i]) ) {
			pCause = xrtErrorRef(xrtGetError());
			goto Failed;
		}
	}
	xrtSpinLock(&pServer->Lock);
	if ( !xrtAtomic32Load(
		&pServer->NetworkClosed,
		XMEMORY_ACQUIRE
	) ) {
		xrtAtomic32Store(
			&pServer->NetworkPublished,
			1,
			XMEMORY_RELEASE
		);
		bPublished = true;
	}
	xrtSpinUnlock(&pServer->Lock);
	if ( !bPublished ) {
		pCause = xrtErrorRef((const xerror*)xrtAtomicPtrLoad(
			&pServer->NetworkError,
			XMEMORY_ACQUIRE
		));
		goto Failed;
	}
	return pServer;

Failed:
	xrtAtomic32Store(
		&pServer->State,
		XHTTP_SERVER_ABORTING,
		XMEMORY_RELEASE
	);
	(void)xrtNetServerClose(pNetwork);
	__xrtHttpServerTryFinish(pServer);
	__xrtHttpServerSetError(
		__xrtHttpServerCauseKind(pCause, XERR_CLOSED),
		XHTTP_SERVER_ERROR_LISTEN,
		"start-http-server",
		"HTTP server network closed during startup",
		pCause
	);
	xrtErrorFree(pCause);
	xrtHttpServerDestroy(pServer);
	return NULL;
}



/* 创建并启动明文 HTTP/1 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpserverevents* pEvents
)
{
	xhttpserver* pServer = __xrtHttpServerCreate(
		pEngine, pConfig, pEvents
	);

	return pServer != NULL ?
		__xrtHttpServerListen(pServer) : NULL;
}



/* 返回 Server 状态。 */
XHTTP_SERVER_API xhttpserverstate xrtHttpServerState(
	const xhttpserver* pServer
)
{
	return pServer != NULL ?
		(xhttpserverstate)xrtAtomic32Load(
			&pServer->State,
			XMEMORY_ACQUIRE
		) : XHTTP_SERVER_CLOSED;
}



/* 返回 Server 是否使用 TLS 传输。 */
XHTTP_SERVER_API bool xrtHttpServerSecure(const xhttpserver* pServer)
{
	return (pServer != NULL) && pServer->Secure;
}



/* 返回 Server 逻辑端点数量。 */
XHTTP_SERVER_API size_t xrtHttpServerEndpointCount(
	const xhttpserver* pServer
)
{
	return pServer != NULL ? pServer->EndpointCount : 0;
}



/* 复制 Server 指定逻辑端点的实际绑定地址。 */
XHTTP_SERVER_API bool xrtHttpServerLocal(
	const xhttpserver* pServer,
	size_t iEndpoint,
	xnetaddr* pAddress
)
{
	if ( (pServer == NULL) ||
		!__xrtHttpServerOutputValid(
			pServer, pAddress, sizeof(*pAddress)
		) ||
		 (iEndpoint >= pServer->EndpointCount) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-local",
			"HTTP server, endpoint or address output is invalid",
			NULL
		);
		return false;
	}
	memcpy(
		(void*)pAddress,
		&pServer->Locals[iEndpoint],
		sizeof(*pAddress)
	);
	return true;
}



/* 返回 Server 实际底层 Listener 数量。 */
XHTTP_SERVER_API size_t xrtHttpServerListenerCount(
	const xhttpserver* pServer
)
{
	return pServer != NULL ? pServer->ListenerCount : 0;
}



/* 增加并返回底层聚合 TCP Server 引用。 */
XHTTP_SERVER_API xnetserver* xrtHttpServerNetwork(xhttpserver* pServer)
{
	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-network",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	return xrtNetServerRef(pServer->Network);
}



/* 返回聚合 TCP Server 的第一个稳定终止错误。 */
XHTTP_SERVER_API const xerror* xrtHttpServerError(
	const xhttpserver* pServer
)
{
	return pServer != NULL ?
		(const xerror*)xrtAtomicPtrLoad(
			&pServer->NetworkError,
			XMEMORY_ACQUIRE
		) : NULL;
}



/* 复制 Server 统计快照。 */
XHTTP_SERVER_API bool xrtHttpServerStats(
	const xhttpserver* pServer,
	xhttpserverstats* pStats
)
{
	xhttpserverstats Stats;
	uint64 iConnections;
	uint64 iPeak;

	if ( (pServer == NULL) ||
		!__xrtHttpServerOutputValid(
			pServer, pStats, sizeof(Stats)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-server-stats",
			"HTTP server or stats output is null",
			NULL
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	Stats.State = xrtHttpServerState(pServer);
	Stats.Accepted = xrtAtomic64Load(
		&pServer->Accepted, XMEMORY_RELAXED
	);
	Stats.Rejected = xrtAtomic64Load(
		&pServer->Rejected, XMEMORY_RELAXED
	);
	Stats.Requests = xrtAtomic64Load(
		&pServer->Requests, XMEMORY_RELAXED
	);
	Stats.Responses = xrtAtomic64Load(
		&pServer->Responses, XMEMORY_RELAXED
	);
	Stats.Informations = xrtAtomic64Load(
		&pServer->Informations, XMEMORY_RELAXED
	);
	Stats.Upgraded = xrtAtomic64Load(
		&pServer->Upgraded, XMEMORY_RELAXED
	);
	Stats.ProtocolErrors = xrtAtomic64Load(
		&pServer->ProtocolErrors, XMEMORY_RELAXED
	);
	Stats.Timeouts = xrtAtomic64Load(
		&pServer->Timeouts, XMEMORY_RELAXED
	);
	iConnections = xrtAtomic64Load(
		&pServer->Connections, XMEMORY_RELAXED
	);
	iPeak = xrtAtomic64Load(
		&pServer->PeakConnections, XMEMORY_RELAXED
	);
	Stats.Connections = iConnections > SIZE_MAX ?
		SIZE_MAX : (size_t)iConnections;
	Stats.PeakConnections = iPeak > SIZE_MAX ?
		SIZE_MAX : (size_t)iPeak;
	Stats.Endpoints = pServer->EndpointCount;
	Stats.Listeners = pServer->ListenerCount;
	Stats.Secure = pServer->Secure;
	memcpy((void*)pStats, &Stats, sizeof(Stats));
	return true;
}



/* 请求关闭聚合 TCP Server。 */
static void __xrtHttpServerCloseNetwork(xhttpserver* pServer)
{
	if ( pServer->Network != NULL ) {
		(void)xrtNetServerClose(pServer->Network);
	}
}



/* 每批保留少量待关闭连接，避免持有 Server 锁进入传输层。 */
static size_t __xrtHttpServerSelectConnections(
	xhttpserver* pServer,
	xhttpconn** pConnections,
	size_t iCapacity,
	bool bAbort
)
{
	xhttpconn* pConnection;
	size_t iCount = 0;

	xrtSpinLock(&pServer->Lock);
	pConnection = pServer->Head;
	while ( (pConnection != NULL) &&
		(iCount < iCapacity) ) {
		bool bSelect;

		if ( bAbort ) {
			bSelect = !pConnection->ServerAbortQueued;
		} else {
			bSelect =
				!pConnection->ServerCloseQueued &&
				!xrtAtomic32Load(
					&pConnection->RequestActive,
					XMEMORY_ACQUIRE
				);
		}
		if ( bSelect ) {
			if ( bAbort ) {
				pConnection->ServerAbortQueued = true;
			} else {
				pConnection->ServerCloseQueued = true;
			}
			pConnections[iCount] =
				xrtHttpConnRef(pConnection);
			if ( pConnections[iCount] != NULL ) {
				iCount++;
			}
		}
		pConnection = pConnection->Next;
	}
	xrtSpinUnlock(&pServer->Lock);
	return iCount;
}



/* 不持有 Server 锁批量关闭 Connection。 */
static void __xrtHttpServerCloseConnections(
	xhttpserver* pServer,
	bool bAbort
)
{
	xhttpconn* Connections[64];
	size_t iCount;
	size_t i;

	do {
		iCount = __xrtHttpServerSelectConnections(
			pServer,
			Connections,
			sizeof(Connections) / sizeof(Connections[0]),
			bAbort
		);
		for ( i = 0; i < iCount; i++ ) {
			if ( bAbort ) {
				(void)xrtHttpConnAbort(Connections[i]);
			} else {
				(void)xrtHttpConnClose(Connections[i]);
			}
			xrtHttpConnDestroy(Connections[i]);
		}
	} while ( iCount ==
		(sizeof(Connections) / sizeof(Connections[0])) );
}



/* 原子从 RUNNING 进入指定关闭状态。 */
static bool __xrtHttpServerBeginClose(
	xhttpserver* pServer,
	xhttpserverstate State
)
{
	uint32 iExpected = XHTTP_SERVER_RUNNING;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"close-http-server",
			"HTTP server is null",
			NULL
		);
		return false;
	}
	if ( xrtAtomic32CompareExchange(
		&pServer->State,
		&iExpected,
		State,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	return (iExpected == (uint32)State) ||
		(iExpected == XHTTP_SERVER_CLOSED);
}



/* 开始优雅排空。 */
XHTTP_SERVER_API bool xrtHttpServerDrain(xhttpserver* pServer)
{
	if ( !__xrtHttpServerBeginClose(
		pServer, XHTTP_SERVER_DRAINING
	) ) {
		return false;
	}
	__xrtHttpServerCloseNetwork(pServer);
	__xrtHttpServerCloseConnections(pServer, false);
	__xrtHttpServerTryFinish(pServer);
	return true;
}



/* 开始异常终止。 */
XHTTP_SERVER_API bool xrtHttpServerAbort(xhttpserver* pServer)
{
	uint32 iState;

	if ( pServer == NULL ) {
		return __xrtHttpServerBeginClose(
			pServer, XHTTP_SERVER_ABORTING
		);
	}
	for ( ;; ) {
		uint32 iExpected;

		iState = xrtAtomic32Load(
			&pServer->State,
			XMEMORY_ACQUIRE
		);
		if ( (iState == XHTTP_SERVER_ABORTING) ||
			(iState == XHTTP_SERVER_CLOSED) ) {
			break;
		}
		if ( (iState != XHTTP_SERVER_RUNNING) &&
			(iState != XHTTP_SERVER_DRAINING) ) {
			return false;
		}
		iExpected = iState;
		if ( xrtAtomic32CompareExchange(
			&pServer->State,
			&iExpected,
			XHTTP_SERVER_ABORTING,
			XMEMORY_ACQ_REL,
			XMEMORY_ACQUIRE
		) ) {
			break;
		}
	}
	__xrtHttpServerCloseNetwork(pServer);
	__xrtHttpServerCloseConnections(pServer, true);
	__xrtHttpServerTryFinish(pServer);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_connection.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER)



#if defined(XHTTP_FEATURE_HTTP_SERVER)

/* 验证公开输出不会覆盖 Connection 的引用、锁和协议状态。 */
static bool __xrtHttpConnOutputValid(
	const xhttpconn* pConnection,
	const void* pOutput,
	size_t iSize
)
{
	return xrtMemRangeValid(pOutput, iSize) &&
		((pConnection == NULL) || !xrtMemRangesOverlap(
			pOutput, iSize, pConnection, sizeof(*pConnection)
		));
}



/* 增加 Connection 引用。 */
XHTTP_SERVER_API xhttpconn* xrtHttpConnRef(xhttpconn* pConnection)
{
	if ( (pConnection == NULL) ||
		(xrtRefRetain(&pConnection->References) < 0) ) {
		__xrtHttpServerSetError(
			pConnection == NULL ? XERR_ARGUMENT : XERR_STATE,
			pConnection == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			"retain-http-connection",
			"HTTP connection is null or no longer retainable",
			NULL
		);
		return NULL;
	}
	return pConnection;
}



/* 取得当前 TCP 或 TLS 传输的稳定引用。 */
static ptr __xrtHttpConnTransportRef(xhttpconn* pConnection)
{
	ptr pTransport = NULL;

	if ( pConnection == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport != NULL ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				pTransport = xrtTlsStreamRef(
					(xtlsstream*)pTransport
				);
			} else {
				pTransport = xrtNetStreamRef(
					(xnetstream*)pTransport
				);
			}
		#else
			pTransport = xrtNetStreamRef(
				(xnetstream*)pTransport
			);
		#endif
	}
	xrtSpinUnlock(&pConnection->TransportLock);
	return pTransport;
}



/* 释放与 Connection 类型匹配的稳定传输引用。 */
static void __xrtHttpConnTransportDestroy(
	const xhttpconn* pConnection,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
		} else {
			xrtNetStreamDestroy((xnetstream*)pTransport);
		}
	#else
		(void)pConnection;
		xrtNetStreamDestroy((xnetstream*)pTransport);
	#endif
}



/* Retire the remaining runtime adapter before publishing HTTP completion.
 * Clear first: a Close/Upgrade adapter may already have taken its own data. */
void __xrtHttpConnAdapterRelease(xhttpconn* pConnection)
{
	void (*pRelease)(ptr pData) = pConnection->AdapterRelease;
	ptr pData = pConnection->AdapterData;

	pConnection->AdapterData = NULL;
	pConnection->AdapterRelease = NULL;
	if ( pRelease != NULL ) {
		pRelease(pData);
	}
}



/* 释放 Connection 最终存储。 */
XHTTP_SERVER_API void xrtHttpConnDestroy(xhttpconn* pConnection)
{
	ptr pTransport;
	xerror* pError;
	__xrt_http_response_queue* pQueued;

	if ( (pConnection == NULL) ||
		(xrtRefRelease(&pConnection->References) != 0) ) {
		return;
	}
	xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtSpinUnlock(&pConnection->TransportLock);
	pError = (xerror*)xrtAtomicPtrExchange(
		&pConnection->Error,
		NULL,
		XMEMORY_ACQ_REL
	);
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	__xrtHttpConnTransportDestroy(pConnection, pTransport);
	xrtErrorFree(pError);
	__xrtHttpConnAdapterRelease(pConnection);
	xrtHttpServerDestroy(pConnection->Server);
	xrtSpinUnit(&pConnection->TransportLock);
	memset(pConnection, 0, sizeof(*pConnection));
	xrtFree(pConnection);
}



/* 在 Worker 串行域内安装唯一适配器上下文。 */
bool __xrtHttpConnAdapterSet(
	xhttpconn* pConnection,
	ptr pData,
	void (*pRelease)(ptr pData)
)
{
	if ( (pConnection == NULL) || (pData == NULL) ||
		(pRelease == NULL) ||
		(pConnection->AdapterData != NULL) ) {
		return false;
	}
	pConnection->AdapterData = pData;
	pConnection->AdapterRelease = pRelease;
	return true;
}



/* 在 Worker 串行域内借用适配器上下文。 */
ptr __xrtHttpConnAdapterData(xhttpconn* pConnection)
{
	return pConnection != NULL ?
		pConnection->AdapterData : NULL;
}



/* 在 Worker 串行域内取走适配器上下文。 */
ptr __xrtHttpConnAdapterTake(xhttpconn* pConnection)
{
	ptr pData;

	if ( pConnection == NULL ) {
		return NULL;
	}
	pData = pConnection->AdapterData;
	pConnection->AdapterData = NULL;
	pConnection->AdapterRelease = NULL;
	return pData;
}



/* 取得底层 TCP Stream 稳定引用，防止与 Close 回调交换所有权竞态。 */
xnetstream* __xrtHttpConnTcpRef(xhttpconn* pConnection)
{
	xnetstream* pStream = NULL;
	ptr pTransport;

	if ( pConnection == NULL ) {
		return NULL;
	}
	xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( (pTransport != NULL) &&
			(pConnection->TransportKind ==
			 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
			pStream = xrtTlsStreamTransport(
				(xtlsstream*)pTransport
			);
		} else {
			pStream = (xnetstream*)pTransport;
		}
	#else
		pStream = (xnetstream*)pTransport;
	#endif
	if ( pStream != NULL ) {
		pStream = xrtNetStreamRef(pStream);
	}
	xrtSpinUnlock(&pConnection->TransportLock);
	return pStream;
}



/* 返回 Connection 当前阶段。 */
XHTTP_SERVER_API xhttpconnstate xrtHttpConnState(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ?
		(xhttpconnstate)xrtAtomic32Load(
			&pConnection->State,
			XMEMORY_ACQUIRE
		) : XHTTP_CONN_CLOSED;
}



/* 返回借用 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpConnServer(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ? pConnection->Server : NULL;
}



/* 在所属 Worker 上返回当前借用请求。 */
XHTTP_SERVER_API const xhttpserverrequest* xrtHttpConnRequest(
	const xhttpconn* pConnection
)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		return NULL;
	}
	return xrtHttp1ServerExchangeRequest(
		pConnection->Exchange
	);
}



/* 返回借用 Worker。 */
XHTTP_SERVER_API xnetworker* xrtHttpConnWorker(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ? pConnection->Worker : NULL;
}



/* 在所属 Worker 上返回借用 TCP Stream。 */
XHTTP_SERVER_API xnetstream* xrtHttpConnTcp(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		return NULL;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( (pTransport != NULL) &&
			(pConnection->TransportKind ==
			 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
			return xrtTlsStreamTransport(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return (xnetstream*)pTransport;
}



/* 返回 Connection 是否使用 TLS 传输。 */
XHTTP_SERVER_API bool xrtHttpConnSecure(const xhttpconn* pConnection)
{
	return (pConnection != NULL) &&
		(pConnection->TransportKind ==
		 XRT_HTTP_CONN_TRANSPORT_TLS);
}



/* 返回接受当前 Connection 的逻辑端点。 */
XHTTP_SERVER_API size_t xrtHttpConnEndpoint(const xhttpconn* pConnection)
{
	return pConnection != NULL ? pConnection->Endpoint : SIZE_MAX;
}



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 在所属 Worker 上返回借用 TLS Stream。 */
XHTTP_SERVER_API xtlsstream* xrtHttpConnTls(xhttpconn* pConnection)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(pConnection->TransportKind !=
		 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
		return NULL;
	}
	return (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
}

#endif



/* 复制连接本地地址。 */
XHTTP_SERVER_API bool xrtHttpConnLocal(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
)
{
	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pAddress, sizeof(*pAddress)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-local",
			"HTTP connection or address output is null",
			NULL
		);
		return false;
	}
	memcpy((void*)pAddress, &pConnection->Local, sizeof(*pAddress));
	return true;
}



/* 复制连接远端地址。 */
XHTTP_SERVER_API bool xrtHttpConnRemote(
	const xhttpconn* pConnection,
	xnetaddr* pAddress
)
{
	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pAddress, sizeof(*pAddress)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-remote",
			"HTTP connection or address output is null",
			NULL
		);
		return false;
	}
	memcpy((void*)pAddress, &pConnection->Remote, sizeof(*pAddress));
	return true;
}



/* 返回借用稳定错误。 */
XHTTP_SERVER_API const xerror* xrtHttpConnError(
	const xhttpconn* pConnection
)
{
	return pConnection != NULL ?
		(const xerror*)xrtAtomicPtrLoad(
			&pConnection->Error,
			XMEMORY_ACQUIRE
		) : NULL;
}



/* 复制 Connection 统计。 */
XHTTP_SERVER_API bool xrtHttpConnStats(
	const xhttpconn* pConnection,
	xhttpconnstats* pStats
)
{
	xhttpconnstats Stats;
	ptr pTransport;

	if ( (pConnection == NULL) ||
		!__xrtHttpConnOutputValid(
			pConnection, pStats, sizeof(Stats)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"query-http-connection-stats",
			"HTTP connection or stats output is null",
			NULL
		);
		return false;
	}
	memset(&Stats, 0, sizeof(Stats));
	Stats.State = xrtHttpConnState(pConnection);
	Stats.Requests = xrtAtomic64Load(
		&pConnection->Requests,
		XMEMORY_RELAXED
	);
	Stats.RequestWireBytes = xrtAtomic64Load(
		&pConnection->RequestWireBytes,
		XMEMORY_RELAXED
	);
	Stats.ResponseWireBytes = xrtAtomic64Load(
		&pConnection->ResponseWireBytes,
		XMEMORY_RELAXED
	);
	Stats.RequestBodyPaused = xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.RequestActive = xrtAtomic32Load(
		&pConnection->RequestActive,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.FinalCommitted = xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) != 0;
	Stats.Secure = xrtHttpConnSecure(pConnection);
	pTransport = __xrtHttpConnTransportRef(
		(xhttpconn*)pConnection
	);
	if ( pTransport != NULL ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( Stats.Secure ) {
				xtlsstream* pTls = (xtlsstream*)pTransport;

				Stats.BufferedBytes =
					xrtTlsStreamAvailable(pTls);
				Stats.QueuedBytes =
					xrtTlsStreamPending(pTls);
			} else {
				xnetstream* pTcp =
					(xnetstream*)pTransport;

				Stats.BufferedBytes =
					xrtNetStreamAvailable(pTcp);
				Stats.QueuedBytes =
					xrtNetStreamPending(pTcp);
			}
		#else
			Stats.BufferedBytes = xrtNetStreamAvailable(
				(xnetstream*)pTransport
			);
			Stats.QueuedBytes = xrtNetStreamPending(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
	}
	memcpy((void*)pStats, &Stats, sizeof(Stats));
	return true;
}



/* 在 Headers 回调中把路由正文上限直接应用到唯一 Exchange。 */
XHTTP_SERVER_API bool xrtHttpConnSetRequestBodyLimit(
	xhttpconn* pConnection,
	uint64 iMaxBody
)
{
	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"limit-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttpConnState(pConnection) != XHTTP_CONN_REQUEST) ||
		!xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) || (xrtHttpConnRequest(pConnection) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"limit-http-request-body",
			"HTTP request body limit requires its Headers callback",
			NULL
		);
		return false;
	}
	if ( !xrtHttp1ServerExchangeSetBodyLimit(
		pConnection->Exchange, iMaxBody
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(pCause, XERR_STATE),
			XHTTP_SERVER_ERROR_PROTOCOL,
			"limit-http-request-body",
			"HTTP request body limit could not be applied",
			pCause
		);
		return false;
	}
	return true;
}



/* 处理一次 Exchange Feed 结果；定义位于请求输入驱动之后。 */
static bool __xrtHttpConnFeedStatus(
	xhttpconn* pConnection,
	xhttp1serverfeedstatus Status
);



/* 在所属 Worker 上恢复 Exchange，并继续处理已经缓冲的正文。 */
static void __xrtHttpConnResumeRequestBody(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	xrtAtomic32Store(
		&pConnection->RequestBodyResumePosted,
		0,
		XMEMORY_RELEASE
	);
	if ( xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) ) {
		if ( (xrtHttpConnState(pConnection) ==
			 XHTTP_CONN_BODY) &&
			xrtHttp1ServerExchangePaused(
				pConnection->Exchange
			) ) {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
			if ( !__xrtHttpConnResumeExchange(
				pConnection
			) ) {
				xrtHttpConnDestroy(pConnection);
				return;
			}
			__xrtHttpConnResumeInput(pConnection);
			__xrtHttpConnDriveInput(pConnection);
		} else {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
		}
	}
	xrtHttpConnDestroy(pConnection);
}



/* 合并跨线程恢复请求，并为嵌入命令持有一份 Connection 引用。 */
static bool __xrtHttpConnScheduleRequestBodyResume(
	xhttpconn* pConnection
)
{
	xerror* pError;
	uint32 iExpected = 0;

	if ( !xrtAtomic32CompareExchange(
		&pConnection->RequestBodyResumePosted,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		xrtAtomic32Store(
			&pConnection->RequestBodyResumePosted,
			0,
			XMEMORY_RELEASE
		);
		return false;
	}
	if ( xrtNetPost(
		pConnection->Worker,
		&pConnection->RequestBodyResumePost,
		__xrtHttpConnResumeRequestBody,
		pConnection
	) ) {
		return true;
	}
	pError = xrtTakeError();
	xrtAtomic32Store(
		&pConnection->RequestBodyResumePosted,
		0,
		XMEMORY_RELEASE
	);
	xrtHttpConnDestroy(pConnection);
	xrtSetError(pError);
	xrtErrorFree(pError);
	return false;
}



/* 在流式 Body 回调中暂停后续请求正文交付。 */
XHTTP_SERVER_API bool xrtHttpConnPauseRequestBody(
	xhttpconn* pConnection
)
{
	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"pause-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttpConnState(pConnection) != XHTTP_CONN_BODY) ||
		!xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"pause-http-request-body",
			"HTTP request body pause requires its streaming Body callback",
			NULL
		);
		return false;
	}
	if ( !xrtHttp1ServerExchangePause(
		pConnection->Exchange
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"pause-http-request-body",
			"HTTP request body is not available for pausing",
			xrtGetError()
		);
		return false;
	}
	xrtAtomic32Store(
		&pConnection->RequestBodyPaused,
		1,
		XMEMORY_RELEASE
	);
	__xrtHttpConnPauseInput(pConnection);
	return true;
}



/* 从任意线程请求恢复流式正文，并唤醒所属 Worker。 */
XHTTP_SERVER_API bool xrtHttpConnResumeRequestBody(
	xhttpconn* pConnection
)
{
	xhttpconnstate State;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"resume-http-request-body",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	State = xrtHttpConnState(pConnection);
	if ( (State == XHTTP_CONN_UPGRADED) ||
		(State == XHTTP_CONN_CLOSING) ||
		(State == XHTTP_CONN_CLOSED) ) {
		__xrtHttpServerSetError(
			State == XHTTP_CONN_CLOSED ? XERR_CLOSED : XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"resume-http-request-body",
			"HTTP connection no longer owns a resumable request body",
			NULL
		);
		return false;
	}
	if ( !xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) ) {
		return true;
	}
	return __xrtHttpConnScheduleRequestBodyResume(pConnection);
}



/* 返回应用请求正文暂停状态的并发快照。 */
XHTTP_SERVER_API bool xrtHttpConnRequestBodyPaused(
	const xhttpconn* pConnection
)
{
	return (pConnection != NULL) &&
		(xrtAtomic32Load(
			&pConnection->RequestBodyPaused,
			XMEMORY_ACQUIRE
		) != 0);
}



/* 拒绝在 HTTP 已经转移传输所有权后继续控制 Connection。 */
static bool __xrtHttpConnClaimClose(
	xhttpconn* pConnection,
	cstr sOperation
)
{
	uint32 iExpected = XRT_HTTP_CONN_GATE_OPEN;

	if ( xrtAtomic32CompareExchange(
		&pConnection->CloseGate,
		&iExpected,
		XRT_HTTP_CONN_GATE_CLOSING,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) || (iExpected == XRT_HTTP_CONN_GATE_CLOSING) ) {
		return true;
	}
	__xrtHttpServerSetError(
		XERR_STATE,
		XHTTP_SERVER_ERROR_STATE,
		sOperation,
		"HTTP connection transport belongs to the upgraded protocol",
		NULL
	);
	return false;
}



/* 从任意线程请求优雅关闭。 */
XHTTP_SERVER_API bool xrtHttpConnClose(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"close-http-connection",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnClaimClose(
		pConnection, "close-http-connection"
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSING,
		XMEMORY_RELEASE
	);
	pTransport = __xrtHttpConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		bool bResult;

		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				bResult = xrtTlsStreamClose(
					(xtlsstream*)pTransport
				);
			} else {
				bResult = xrtNetStreamClose(
					(xnetstream*)pTransport
				);
			}
		#else
			bResult = xrtNetStreamClose(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
		return bResult;
	}
	return true;
}



/* 从任意线程请求异常关闭。 */
XHTTP_SERVER_API bool xrtHttpConnAbort(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"abort-http-connection",
			"HTTP connection is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnClaimClose(
		pConnection, "abort-http-connection"
	) ) {
		return false;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSING,
		XMEMORY_RELEASE
	);
	pTransport = __xrtHttpConnTransportRef(pConnection);
	if ( pTransport != NULL ) {
		bool bResult;

		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				bResult = xrtTlsStreamAbort(
					(xtlsstream*)pTransport
				);
			} else {
				bResult = xrtNetStreamAbort(
					(xnetstream*)pTransport
				);
			}
		#else
			bResult = xrtNetStreamAbort(
				(xnetstream*)pTransport
			);
		#endif
		__xrtHttpConnTransportDestroy(
			pConnection, pTransport
		);
		return bResult;
	}
	return true;
}



/* 取消当前阶段 Timer。 */
void __xrtHttpConnCancelTimer(xhttpconn* pConnection)
{
	uint64 Id = pConnection->Timer;

	pConnection->Timer = 0;
	pConnection->TimerKind = XRT_HTTP_SERVER_TIMER_NONE;
	if ( Id != 0 ) {
		if ( !xrtNetEngineTimerCancelCurrent(
			pConnection->Server->Engine,
			Id
		) ) {
			(void)xrtNetEngineTimerCancel(
				pConnection->Server->Engine,
				Id
			);
		}
	}
}



/* Timer 到期时按明确协议阶段收敛连接。 */
static void __xrtHttpConnTimer(
	xnetworker* pWorker,
	uint64 Id,
	xnetresult Result,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	uint32 iKind = XRT_HTTP_SERVER_TIMER_NONE;
	int64 iRemaining = 0;

	(void)pWorker;
	if ( pConnection->Timer == Id ) {
		iKind = pConnection->TimerKind;
		pConnection->Timer = 0;
		pConnection->TimerKind =
			XRT_HTTP_SERVER_TIMER_NONE;
	}
	if ( (Result == XNET_RESULT_OK) &&
		(iKind != XRT_HTTP_SERVER_TIMER_NONE) ) {
		if ( iKind == XRT_HTTP_SERVER_TIMER_WRITE ) {
			iRemaining = __xrtWaitRemaining(
				pConnection->WriteDeadline
			);
			#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
				if ( (iRemaining == 0) &&
					pConnection->OutputDraining &&
					(pConnection->TransportKind ==
					 XRT_HTTP_CONN_TRANSPORT_TLS) ) {
					xtlsstream* pStream = (xtlsstream*)
						xrtAtomicPtrLoad(
							&pConnection->Transport,
							XMEMORY_ACQUIRE
						);
					size_t iPending =
						xrtTlsStreamPending(pStream);

					/*
					 * TLS 与 TCP 队列的持续下降属于有效写进度；
					 * 完全排空时直接推进响应，避免等待迟到的 Drain。
					 */
					if ( iPending == 0 ) {
						__xrtHttpConnDriveOutput(
							pConnection
						);
						xrtHttpConnDestroy(
							pConnection
						);
						return;
					}
					if ( iPending <
						pConnection->WritePending ) {
						pConnection->WritePending =
							iPending;
						pConnection->WriteDeadline =
							__xrtWaitAfter(
								pConnection->Server->
									Config.WriteTimeout
							);
						iRemaining =
							pConnection->Server->
								Config.WriteTimeout;
					}
				}
			#endif
		}
		if ( iRemaining != 0 ) {
			if ( !__xrtHttpConnArmTimer(
				pConnection,
				XRT_HTTP_SERVER_TIMER_WRITE,
				iRemaining
			) ) {
				(void)xrtHttpConnAbort(pConnection);
			}
			xrtHttpConnDestroy(pConnection);
			return;
		}
		(void)xrtAtomic64FetchAdd(
			&pConnection->Server->Timeouts,
			1,
			XMEMORY_RELAXED
		);
		if ( iKind == XRT_HTTP_SERVER_TIMER_HEADER ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				408,
				XHTTP_SERVER_ERROR_TIMEOUT_HEADER,
				"read-http-request-header",
				"HTTP request header timed out",
				NULL
			);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_BODY ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				408,
				XHTTP_SERVER_ERROR_TIMEOUT_BODY,
				"read-http-request-body",
				"HTTP request body made no progress before its timeout",
				NULL
			);
		} else if ( iKind ==
			XRT_HTTP_SERVER_TIMER_REQUEST ) {
			__xrtHttpConnProtocolFail(
				pConnection,
				504,
				XHTTP_SERVER_ERROR_TIMEOUT_REQUEST,
				"wait-http-response",
				"HTTP application did not submit a response before its timeout",
				NULL
			);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_IDLE ) {
			(void)xrtHttpConnClose(pConnection);
		} else if ( iKind == XRT_HTTP_SERVER_TIMER_WRITE ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_TIMEOUT,
				XHTTP_SERVER_ERROR_TIMEOUT_WRITE,
				"write-http-response",
				"HTTP response made no progress before its timeout",
				NULL
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		}
	}
	xrtHttpConnDestroy(pConnection);
}



/* 为当前阶段安装新 Timer。 */
bool __xrtHttpConnArmTimer(
	xhttpconn* pConnection,
	uint32 iKind,
	int64 iTimeout
)
{
	uint64 Id;

	__xrtHttpConnCancelTimer(pConnection);
	if ( iTimeout == 0 ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return false;
	}
	Id = xrtNetEngineAfter(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		iTimeout,
		__xrtHttpConnTimer,
		pConnection
	);
	if ( Id == 0 ) {
		xerror* pCause = xrtErrorRef(xrtGetError());

		__xrtHttpConnRememberError(
			pConnection,
			XERR_INTERNAL,
			XHTTP_SERVER_ERROR_INTERNAL,
			"schedule-http-connection-timeout",
			"HTTP connection could not schedule its phase timeout",
			pCause
		);
		xrtErrorFree(pCause);
		xrtHttpConnDestroy(pConnection);
		return false;
	}
	pConnection->Timer = Id;
	pConnection->TimerKind = iKind;
	return true;
}



/* 借用当前传输已经解密的连续块链。 */
const xnetbuf* __xrtHttpConnBuffer(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return NULL;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return xrtTlsStreamBuffer(
				(xtlsstream*)pTransport
			);
		}
	#endif
	return xrtNetStreamBuffer((xnetstream*)pTransport);
}



/* 消费当前传输的一段明文输入。 */
bool __xrtHttpConnConsume(
	xhttpconn* pConnection,
	size_t iSize
)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return false;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return false;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return xrtTlsStreamConsume(
				(xtlsstream*)pTransport,
				iSize
			);
		}
	#endif
	return xrtNetStreamConsume(
		(xnetstream*)pTransport,
		iSize
	);
}



/* 暂停明文 TCP 输入；TLS 会在留下未消费明文时自动暂停。 */
void __xrtHttpConnPauseInput(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return;
		}
	#endif
	xrtNetStreamPause((xnetstream*)pTransport);
}



/* 恢复明文 TCP 输入；TLS 会在消费完明文后自动恢复。 */
void __xrtHttpConnResumeInput(xhttpconn* pConnection)
{
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return;
		}
	#endif
	(void)xrtNetStreamResume((xnetstream*)pTransport);
}



/* 把 Exchange Header 事件提升到 Connection。 */
static xhttpserverbodypolicy __xrtHttpConnHeaders(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;
	xhttpserverbodypolicy Policy =
		XHTTP_SERVER_BODY_BUFFER;
	bool bHasBody =
		xrtHttpServerRequestBodyMode(pRequest) !=
		XHTTP1_BODY_NONE;

	if ( pServer->Events.Headers != NULL ) {
		Policy = pServer->Events.Headers(
			pServer,
			pConnection,
			pRequest,
			pServer->Events.Data
		);
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* 最终响应已经固定；无正文请求继续完成，上传请求停止在 Header 边界。 */
		return bHasBody ?
			XHTTP_SERVER_BODY_REJECT :
			XHTTP_SERVER_BODY_BUFFER;
	}
	if ( (Policy != XHTTP_SERVER_BODY_BUFFER) &&
		(Policy != XHTTP_SERVER_BODY_STREAM) &&
		(Policy != XHTTP_SERVER_BODY_REJECT) &&
		(Policy != XHTTP_SERVER_BODY_DISCARD) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_CALLBACK,
			"select-http-request-body",
			"HTTP Headers callback returned an invalid body policy",
			NULL
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( (Policy == XHTTP_SERVER_BODY_STREAM) &&
		(pServer->Events.Body == NULL) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_CALLBACK,
			"stream-http-request-body",
			"HTTP streaming body policy requires a Body callback",
			NULL
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( pConnection->ResponseInformation &&
		(pConnection->Response != NULL) ) {
		if ( bHasBody &&
			!xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-information",
				"HTTP request could not pause for its information response",
				xrtGetError()
			);
			return XHTTP_SERVER_BODY_REJECT;
		}
		return Policy;
	}
	if ( !bHasBody ) {
		return Policy;
	}
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_BODY,
		XMEMORY_RELEASE
	);
	if ( ((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_EXPECT_CONTINUE) != 0) &&
		(Policy != XHTTP_SERVER_BODY_REJECT) ) {
		if ( !xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-body",
				"HTTP request body could not pause for 100 Continue",
				xrtGetError()
			);
			return XHTTP_SERVER_BODY_REJECT;
		}
		pConnection->AutoContinue = true;
		return Policy;
	}
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_BODY,
		pServer->Config.BodyTimeout
	) ) {
		return XHTTP_SERVER_BODY_REJECT;
	}
	return Policy;
}



/* 把流式正文片段提升到 Connection 并刷新正文空闲时限。 */
static bool __xrtHttpConnBody(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;
	bool bAccepted = true;

	if ( pServer->Events.Body != NULL ) {
		bAccepted = pServer->Events.Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pServer->Events.Data
		);
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* 阻止底层循环继续消费同一输入片段中的正文或流水线后缀。 */
		if ( !xrtHttp1ServerExchangePause(pExchange) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_PROTOCOL,
				"pause-http-request-after-response",
				"HTTP request could not stop after its early response",
				xrtGetError()
			);
			return false;
		}
		return true;
	}
	return bAccepted;
}



/* 发布完整请求并等待应用提交最终响应。 */
static bool __xrtHttpConnComplete(
	xhttp1serverexchange* pExchange,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpserver* pServer = pConnection->Server;

	(void)pExchange;
	(void)xrtAtomic64FetchAdd(
		&pConnection->Requests,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pServer->Requests,
		1,
		XMEMORY_RELAXED
	);
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		/* Headers 已经提交最终响应时，只完成协议计数，不重复进入应用路由。 */
		return true;
	}
	__xrtHttpConnCancelTimer(pConnection);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_WAITING,
		XMEMORY_RELEASE
	);
	__xrtHttpConnPauseInput(pConnection);
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_REQUEST,
		pServer->Config.RequestTimeout
	) ) {
		return false;
	}
	if ( pServer->Events.Request != NULL ) {
		pServer->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pServer->Events.Data
		);
	} else {
		(void)xrtHttpConnReply(
			pConnection,
			404,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Not Found")
		);
	}
	if ( xrtHttpConnState(pConnection) ==
		XHTTP_CONN_WAITING ) {
		/*
		 * 明文 TCP 在应用异步生成响应时继续有界读取，以便及时观察
		 * EOF、RST；TLS 则由未消费明文自动施加同等有界背压。
		 */
		__xrtHttpConnResumeInput(pConnection);
	}
	return true;
}



/* 返回 Connection 使用的 Exchange 事件。 */
static void __xrtHttpConnExchangeEvents(
	xhttpconn* pConnection,
	xhttp1serverevents* pEvents
)
{
	memset(pEvents, 0, sizeof(*pEvents));
	pEvents->Headers = __xrtHttpConnHeaders;
	pEvents->Body = __xrtHttpConnBody;
	pEvents->Complete = __xrtHttpConnComplete;
	pEvents->Data = pConnection;
}



/* 把 Exchange 终态错误映射为服务端响应状态。 */
static void __xrtHttpConnExchangeFail(xhttpconn* pConnection)
{
	const xerror* pCause = xrtHttp1ServerExchangeError(
		pConnection->Exchange
	);
	int32 iCode = xrtErrorCode(pCause);
	uint16 iStatus = 400;
	xhttpservererror Code = XHTTP_SERVER_ERROR_PROTOCOL;

	if ( iCode == XHTTP1_SERVER_ERROR_EXPECTATION ) {
		iStatus = 417;
	} else if ( iCode == XHTTP1_SERVER_ERROR_BODY_LIMIT ) {
		iStatus = 413;
	} else if (
		((iCode == XHTTP1_SERVER_ERROR_HEAD) ||
		 (iCode == XHTTP1_SERVER_ERROR_TRAILER)) &&
		(xrtErrorIs(pCause, XERR_RANGE) != NULL)
	) {
		iStatus = 431;
	} else if (
		(iCode == XHTTP1_SERVER_ERROR_BODY_STORAGE) ||
		(iCode == XHTTP1_SERVER_ERROR_TRAILER_STORAGE) ||
		(xrtErrorIs(pCause, XERR_MEMORY) != NULL) ||
		(iCode == XHTTP1_SERVER_ERROR_HEADERS_CALLBACK) ||
		(iCode == XHTTP1_SERVER_ERROR_BODY_CALLBACK) ||
		(iCode == XHTTP1_SERVER_ERROR_COMPLETE_CALLBACK)
	) {
		iStatus = 500;
		Code = XHTTP_SERVER_ERROR_CALLBACK;
	}
	__xrtHttpConnProtocolFail(
		pConnection,
		iStatus,
		Code,
		"parse-http-request",
		"HTTP request could not be completed",
		pCause
	);
}



/* 处理一次 Exchange Feed 状态。 */
static bool __xrtHttpConnFeedStatus(
	xhttpconn* pConnection,
	xhttp1serverfeedstatus Status
)
{
	if ( Status == XHTTP1_SERVER_FEED_MORE ) {
		return true;
	}
	if ( Status == XHTTP1_SERVER_FEED_PAUSED ) {
		__xrtHttpConnPauseInput(pConnection);
		if ( pConnection->AutoContinue ) {
			pConnection->AutoContinue = false;
			if ( !__xrtHttpConnContinue(pConnection) ) {
				(void)xrtHttpConnAbort(pConnection);
			}
		}
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_COMPLETE ) {
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_REJECTED ) {
		if ( !xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
			const xerror* pError = xrtHttpConnError(
				pConnection
			);
			bool bCallback = (pError != NULL) &&
				(xrtErrorCode(pError) ==
				 XHTTP_SERVER_ERROR_CALLBACK);

			__xrtHttpConnProtocolFail(
				pConnection,
				bCallback ? 500 : 413,
				bCallback ?
					XHTTP_SERVER_ERROR_CALLBACK :
					XHTTP_SERVER_ERROR_PROTOCOL,
				"reject-http-request-body",
				bCallback ?
					"HTTP request callback rejected the request" :
					"HTTP request body was rejected by policy",
				pError
			);
		}
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_ERROR ) {
		__xrtHttpConnExchangeFail(pConnection);
		return false;
	}
	if ( Status == XHTTP1_SERVER_FEED_CLOSED ) {
		(void)xrtHttpConnClose(pConnection);
		return false;
	}
	return false;
}



/* 恢复协议正文，并在无需新字节时立即发布 Complete 或错误终态。 */
bool __xrtHttpConnResumeExchange(xhttpconn* pConnection)
{
	xhttp1serverfeedstatus Status;
	size_t iAccepted = 0;

	if ( !xrtHttp1ServerExchangePaused(pConnection->Exchange) ) {
		return true;
	}
	if ( !xrtHttp1ServerExchangeResume(pConnection->Exchange) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"resume-http-request-body",
			"HTTP request body state could not resume",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return false;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pConnection->Exchange,
		(xbytesview){ NULL, 0 },
		false,
		&iAccepted
	);
	if ( iAccepted != 0 ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"resume-http-request-body",
			"HTTP request body accepted bytes from empty input",
			NULL
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return false;
	}
	return __xrtHttpConnFeedStatus(pConnection, Status);
}



/* 消费 Stream 当前全部可处理输入，不吞入下一条流水线请求。 */
void __xrtHttpConnDriveInput(xhttpconn* pConnection)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xhttpconnstate State = xrtHttpConnState(pConnection);

	if ( (pTransport == NULL) ||
		(State == XHTTP_CONN_WAITING) ||
		(State == XHTTP_CONN_INFORMATION) ||
		(State == XHTTP_CONN_RESPONSE) ||
		(State == XHTTP_CONN_CLOSING) ||
		(State == XHTTP_CONN_CLOSED) ||
		(pConnection->Response != NULL) ) {
		return;
	}
	for ( ;; ) {
		const xnetbuf* pBuffer =
			__xrtHttpConnBuffer(pConnection);
		xnetspan Span;
		xhttp1serverfeedstatus Status;
		size_t iAccepted = 0;

		if ( (pBuffer == NULL) ||
			!xrtNetBufFront(pBuffer, &Span) ) {
			break;
		}
		if ( !xrtAtomic32Load(
			&pConnection->RequestActive,
			XMEMORY_ACQUIRE
		) ) {
			xrtAtomic32Store(
				&pConnection->RequestActive,
				1,
				XMEMORY_RELEASE
			);
			if ( pConnection->TimerKind !=
				XRT_HTTP_SERVER_TIMER_HEADER ) {
				if ( !__xrtHttpConnArmTimer(
					pConnection,
					XRT_HTTP_SERVER_TIMER_HEADER,
					pConnection->Server->Config.HeaderTimeout
				) ) {
					(void)xrtHttpConnAbort(pConnection);
					break;
				}
			}
		}
		Status = xrtHttp1ServerExchangeFeed(
			pConnection->Exchange,
			(xbytesview){ Span.Data, Span.Size },
			false,
			&iAccepted
		);
		if ( iAccepted != 0 ) {
			(void)__xrtHttpConnConsume(
				pConnection, iAccepted
			);
			(void)xrtAtomic64FetchAdd(
				&pConnection->RequestWireBytes,
				(uint64)iAccepted,
				XMEMORY_RELAXED
			);
			if ((xrtHttpConnState(pConnection) ==
				 XHTTP_CONN_BODY) &&
				!pConnection->AutoContinue &&
				!__xrtHttpConnArmTimer(
					pConnection,
					XRT_HTTP_SERVER_TIMER_BODY,
					pConnection->Server->Config.BodyTimeout
				) ) {
				(void)xrtHttpConnAbort(pConnection);
				break;
			}
		}
		if ( !__xrtHttpConnFeedStatus(
			pConnection, Status
		) ) {
			break;
		}
		if ( iAccepted == 0 ) {
			break;
		}
	}
}



/* 处理可靠 TCP 读端结束。 */
static void __xrtHttpConnEndInput(xhttpconn* pConnection)
{
	xhttp1serverfeedstatus Status;
	size_t iAccepted = 0;

	pConnection->InputEnded = true;
	pConnection->ForceClose = true;
	__xrtHttpConnDriveInput(pConnection);
	if ( (pConnection->Response != NULL) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_WAITING) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) == XHTTP_CONN_CLOSED) ) {
		return;
	}
	Status = xrtHttp1ServerExchangeFeed(
		pConnection->Exchange,
		(xbytesview){ NULL, 0 },
		true,
		&iAccepted
	);
	(void)__xrtHttpConnFeedStatus(pConnection, Status);
}



/* 传输打开后启动 Header 总时限并发布 Open。 */
static void __xrtHttpConnOpened(xhttpconn* pConnection)
{
	xhttpserver* pServer = pConnection->Server;

	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_HEADER,
		pServer->Config.HeaderTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( pServer->Events.Open != NULL ) {
		pServer->Events.Open(
			pServer,
			pConnection,
			pServer->Events.Data
		);
	}
}



/* TCP 打开事件进入统一 HTTP Open 路径。 */
static void __xrtHttpConnOpen(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnOpened((xhttpconn*)pData);
}



/* Stream 可读时推进请求。 */
static void __xrtHttpConnRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpConnDriveInput((xhttpconn*)pData);
}



/* TCP 对端结束写方向时完成 Exchange EOF。 */
static void __xrtHttpConnEnd(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnEndInput((xhttpconn*)pData);
}



/* TCP 写预算恢复时继续当前响应。 */
static void __xrtHttpConnWritable(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	(void)pStream;
	(void)iQueued;
	__xrtHttpConnDriveOutput((xhttpconn*)pData);
}



/* TCP 发送队列排空时继续或完成当前响应。 */
static void __xrtHttpConnDrain(
	xnetstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnDriveOutput((xhttpconn*)pData);
}



/* 传输终态回收协议对象并发布唯一 Connection Close。 */
static void __xrtHttpConnClosed(
	xhttpconn* pConnection,
	ptr pTransport,
	xnetresult Result,
	const xerror* pCause
)
{
	xhttpserver* pServer = pConnection->Server;
	ptr pOwned;
	const xerror* pError;
	__xrt_http_response_queue* pQueued;
	bool bHadError = xrtHttpConnError(pConnection) != NULL;
	bool bConnectionFinished = false;
	bool bReleaseRuntime;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		bool bUpgradePending;
	#endif

	if ( pCause != NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			xrtErrorKind(pCause),
			xrtHttpConnSecure(pConnection) ?
				XHTTP_SERVER_ERROR_TLS :
				XHTTP_SERVER_ERROR_CONNECTION,
			"run-http-connection",
			xrtHttpConnSecure(pConnection) ?
				"HTTPS TLS connection failed" :
			"HTTP TCP connection failed",
			pCause
		);
		if ( !bHadError &&
			(xrtErrorKind(pCause) == XERR_TIMEOUT) ) {
			(void)xrtAtomic64FetchAdd(
				&pServer->Timeouts,
				1,
				XMEMORY_RELAXED
			);
		}
	}
	__xrtHttpConnCancelTimer(pConnection);
	xrtAtomic32Store(
		&pConnection->RequestBodyPaused,
		0,
		XMEMORY_RELEASE
	);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrtHttpConnFutureDetach(pConnection, true);
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
		__xrtHttpConnBodyStop(pConnection, true);
	#endif
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_CLOSED,
		XMEMORY_RELEASE
	);
	xrtSpinLock(&pConnection->TransportLock);
	pOwned = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtSpinUnlock(&pConnection->TransportLock);
	pError = xrtHttpConnError(pConnection);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		bUpgradePending = __xrtHttpConnUpgradeFail(
			pConnection,
			Result,
			pError != NULL ? pError : pCause
		);
	#endif
	bReleaseRuntime = pConnection->RuntimeHeld
		#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
			&& !bUpgradePending
		#endif
	;
	if ( bReleaseRuntime && pConnection->Counted ) {
		pConnection->Counted = false;
		bConnectionFinished = true;
		__xrtHttpServerConnectionClosed(pServer);
	}
	if (
		#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
			!bUpgradePending &&
		#endif
		(pError != NULL) ) {
		__xrtHttpConnEmitError(pConnection);
	}
	if (
		#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
			!bUpgradePending &&
		#endif
		!pConnection->CloseNotified &&
		(pServer->Events.Close != NULL) ) {
		pConnection->CloseNotified = true;
		pServer->Events.Close(
			pServer,
			pConnection,
			Result,
			pError,
			pServer->Events.Data
		);
	}
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	pConnection->Response = NULL;
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	pConnection->ResponseTail = NULL;
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	pConnection->Exchange = NULL;
	__xrtHttpConnTransportDestroy(
		pConnection,
		pOwned != NULL ? pOwned : pTransport
	);
	__xrtHttpServerRemoveConnection(
		pServer, pConnection
	);
	if ( bReleaseRuntime ) {
		__xrtHttpConnAdapterRelease(pConnection);
		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
	}
	if ( bConnectionFinished ) {
		__xrtHttpServerConnectionFinished(pServer);
	}
}



/* TCP 终态进入统一 Connection 回收路径。 */
static void __xrtHttpConnCloseEvent(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pCause,
	ptr pData
)
{
	__xrtHttpConnClosed(
		(xhttpconn*)pData,
		pStream,
		Result,
		pCause
	);
}



/* 返回静态 TCP Stream 事件表。 */
const xnetstreamevents* __xrtHttpConnStreamEvents(void)
{
	static const xnetstreamevents Events = {
		__xrtHttpConnOpen,
		__xrtHttpConnRead,
		__xrtHttpConnEnd,
		NULL,
		__xrtHttpConnWritable,
		__xrtHttpConnDrain,
		__xrtHttpConnCloseEvent
	};

	return &Events;
}



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* TLS 握手完成后进入统一 HTTP Open 路径。 */
static void __xrtHttpConnTlsOpen(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnOpened((xhttpconn*)pData);
}



/* TLS 明文可读时推进请求。 */
static void __xrtHttpConnTlsRead(
	xtlsstream* pStream,
	const xnetbuf* pBuffer,
	ptr pData
)
{
	(void)pStream;
	(void)pBuffer;
	__xrtHttpConnDriveInput((xhttpconn*)pData);
}



/* TLS 对端认证关闭写方向时完成 Exchange EOF。 */
static void __xrtHttpConnTlsEnd(
	xtlsstream* pStream,
	ptr pData
)
{
	(void)pStream;
	__xrtHttpConnEndInput((xhttpconn*)pData);
}



/* TLS 明文写预算恢复时继续当前响应。 */
static void __xrtHttpConnTlsWritable(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pStream;
	pConnection->OutputQueued = false;
	__xrtHttpConnDriveOutput(pConnection);
}



/* TLS 密文完全排空时继续关闭或防御性恢复输出。 */
static void __xrtHttpConnTlsDrain(
	xtlsstream* pStream,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pStream;
	pConnection->OutputQueued = false;
	__xrtHttpConnDriveOutput(pConnection);
}



/* TLS 终态进入统一 Connection 回收路径。 */
static void __xrtHttpConnTlsClose(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pCause,
	ptr pData
)
{
	__xrtHttpConnClosed(
		(xhttpconn*)pData,
		pStream,
		Result,
		pCause
	);
}



/* 返回静态 TLS Stream 事件表。 */
static const xtlsstreamevents* __xrtHttpConnTlsEvents(void)
{
	static const xtlsstreamevents Events = {
		.Open = __xrtHttpConnTlsOpen,
		.Read = __xrtHttpConnTlsRead,
		.End = __xrtHttpConnTlsEnd,
		.Writable = __xrtHttpConnTlsWritable,
		.Drain = __xrtHttpConnTlsDrain,
		.Close = __xrtHttpConnTlsClose,
		.Ticket = NULL
	};

	return &Events;
}

#endif



/* 创建尚未绑定传输的 Worker 归属 HTTP Connection。 */
static xhttpconn* __xrtHttpConnCreateBase(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream,
	uint32 iTransportKind
)
{
	xhttp1serverevents ExchangeEvents;
	xhttpconn* pConnection;

	pConnection = (xhttpconn*)xrtCalloc(
		1, sizeof(*pConnection)
	);
	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !xrtNetPostInit(
		&pConnection->RequestBodyResumePost
	) ) {
		xrtFree(pConnection);
		return NULL;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
		if ( !xrtNetPostInit(&pConnection->UpgradePost) ) {
			xrtFree(pConnection);
			return NULL;
		}
	#endif
	pConnection->References = 1;
	xrtAtomic32Init(
		&pConnection->State, XHTTP_CONN_REQUEST
	);
	xrtAtomic32Init(
		&pConnection->CloseGate,
		XRT_HTTP_CONN_GATE_OPEN
	);
	xrtAtomic32Init(&pConnection->FinalGate, 0);
	xrtAtomic32Init(&pConnection->RequestActive, 0);
	xrtAtomic32Init(&pConnection->RequestBodyPaused, 0);
	xrtAtomic32Init(
		&pConnection->RequestBodyResumePosted, 0
	);
	xrtAtomic64Init(&pConnection->Requests, 0);
	xrtAtomic64Init(&pConnection->RequestWireBytes, 0);
	xrtAtomic64Init(&pConnection->ResponseWireBytes, 0);
	xrtAtomicPtrInit(&pConnection->Error, NULL);
	xrtAtomicPtrInit(&pConnection->Transport, NULL);
	xrtSpinInit(&pConnection->TransportLock);
	pConnection->Server = xrtHttpServerRef(pServer);
	pConnection->Worker = xrtNetStreamWorker(pStream);
	pConnection->Endpoint = iEndpoint;
	pConnection->TransportKind = iTransportKind;
	pConnection->RuntimeHeld = true;
	if ( (pConnection->Server == NULL) ||
		!xrtNetStreamLocal(pStream, &pConnection->Local) ||
		!xrtNetStreamRemote(pStream, &pConnection->Remote) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	__xrtHttpConnExchangeEvents(
		pConnection, &ExchangeEvents
	);
	pConnection->Exchange =
		xrtHttp1ServerExchangeCreate(
			&pServer->Config.Http1,
			&ExchangeEvents
		);
	if ( pConnection->Exchange == NULL ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return pConnection;
}



/* 创建并接管一个已接受的明文 TCP Stream。 */
xhttpconn* __xrtHttpConnCreateTcp(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
)
{
	xhttpconn* pConnection = __xrtHttpConnCreateBase(
		pServer,
		iEndpoint,
		pStream,
		XRT_HTTP_CONN_TRANSPORT_TCP
	);

	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !xrtNetStreamSetEvents(
		pStream,
		__xrtHttpConnStreamEvents(),
		pConnection
	) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pConnection->Transport,
		pStream,
		XMEMORY_RELEASE
	);
	return pConnection;
}



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 创建并接管一个已接受的 TLS Stream。 */
xhttpconn* __xrtHttpConnCreateTls(
	xhttpserver* pServer,
	size_t iEndpoint,
	xnetstream* pStream
)
{
	xhttpconn* pConnection = __xrtHttpConnCreateBase(
		pServer,
		iEndpoint,
		pStream,
		XRT_HTTP_CONN_TRANSPORT_TLS
	);
	xtlsstream* pTls = NULL;

	if ( pConnection == NULL ) {
		return NULL;
	}
	if ( !xrtTlsStreamAccept(
		pStream,
		&pServer->Tls,
		&pServer->TlsStream,
		__xrtHttpConnTlsEvents(),
		pConnection,
		&pTls
	) ) {
		xerror* pError = xrtTakeError();

		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	xrtAtomicPtrStore(
		&pConnection->Transport,
		pTls,
		XMEMORY_RELEASE
	);
	return pConnection;
}

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_transport.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER)



#if defined(XHTTP_FEATURE_HTTP_SERVER)

/* 描述符通过 memcpy 交给消息层，保持失败时不转移租约。 */
xhttp1serverresponse* __xrtHttpConnWireResponse(
	xhttpbody* pBody, uint64 iLength, bool bClose, bool bTunnel
)
{
	return xrtHttpBridgeServerWireBody(pBody, iLength, bClose, bTunnel);
}

xhttp1serverresponse* __xrtHttpConnWireRefsResponse(
	const xnetref* pRefs, size_t iCount, uint64 iLength, bool bClose, bool bTunnel
)
{
	_Static_assert(sizeof(xnetref) == sizeof(xhttpbodychunk), "wire chunk size");
	_Static_assert(offsetof(xnetref, Release) == offsetof(xhttpbodychunk, Release), "wire release offset");
	_Static_assert(offsetof(xnetref, Context) == offsetof(xhttpbodychunk, Context), "wire context offset");
	return xrtHttpBridgeServerWireChunks(
		(const xhttpbodychunk*)pRefs, iCount, iLength, bClose, bTunnel
	);
}

/* 发布当前 Connection 稳定错误事件。 */
void __xrtHttpConnEmitError(xhttpconn* pConnection)
{
	xhttpserver* pServer = pConnection->Server;
	const xerror* pError = xrtHttpConnError(pConnection);

	if ( (pError != NULL) &&
		!pConnection->ErrorNotified &&
		(pServer->Events.Error != NULL) ) {
		pConnection->ErrorNotified = true;
		pServer->Events.Error(
			pServer,
			pConnection,
			pError,
			pServer->Events.Data
		);
	}
}



/* 提取当前请求版本、方法和可复用事实，并合并连接终态。 */
static void __xrtHttpConnResponseFacts(
	xhttpconn* pConnection,
	xhttpversion* pVersion,
	xstrview* pMethod,
	uint32* pFlags
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	xhttpversion Version = XHTTP_VERSION_1_1;
	xstrview Method = XRT_STR_LITERAL("GET");
	uint32 iFlags = 0;
	bool bRequestComplete = xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	);

	if ( pRequest != NULL ) {
		Version = xrtHttpServerRequestVersion(pRequest);
		Method = xrtHttpServerRequestMethod(pRequest);
		iFlags = xrtHttpServerRequestFlags(pRequest);
		if ( xrtHttpServerRequestBodyMode(pRequest) ==
			XHTTP1_BODY_NONE ) {
			/* Header 已完整固定，无正文请求可以在 Headers 回调后安全复用。 */
			bRequestComplete = true;
		}
	}
	if ( pConnection->ForceClose ||
		pConnection->InputEnded ||
		!bRequestComplete ||
		(xrtHttpServerState(pConnection->Server) !=
		 XHTTP_SERVER_RUNNING) ) {
		iFlags &= ~XHTTP_SERVER_REQUEST_KEEP_ALIVE;
		pConnection->ForceClose = true;
	}
	*pVersion = Version;
	*pMethod = Method;
	*pFlags = iFlags;
}



/* 按当前请求与 Server 排空事实冻结最终响应。 */
static xhttp1serverresponse* __xrtHttpConnPrepareResponse(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	xhttpversion Version;
	xstrview Method;
	uint32 iFlags;

	__xrtHttpConnResponseFacts(
		pConnection,
		&Version,
		&Method,
		&iFlags
	);
	return xrtHttp1ServerResponsePrepare(
		Version, Method, iFlags, pReply
	);
}



/* 按当前请求版本冻结信息响应。 */
static xhttp1serverresponse* __xrtHttpConnPrepareInformation(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	xhttpversion Version = pRequest != NULL ?
		xrtHttpServerRequestVersion(pRequest) :
		XHTTP_VERSION_1_1;

	return xrtHttp1ServerResponseInform(Version, pReply);
}



/* 检查 Connection 能否在当前 Worker 提交响应。 */
bool __xrtHttpConnCanRespond(
	xhttpconn* pConnection,
	cstr sOperation,
	bool bRequireRequest
)
{
	if ( (pConnection == NULL) ||
		!xrtNetWorkerIsCurrent(
			pConnection != NULL ?
				pConnection->Worker : NULL
		) ) {
		__xrtHttpServerSetError(
			pConnection == NULL ? XERR_ARGUMENT : XERR_STATE,
			pConnection == NULL ?
				XHTTP_SERVER_ERROR_ARGUMENT :
				XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP response submission requires its connection Worker",
			NULL
		);
		return false;
	}
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ||
		(xrtAtomicPtrLoad(
			&pConnection->Transport,
			XMEMORY_ACQUIRE
		) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_CLOSED,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection is closing or closed",
			NULL
		);
		return false;
	}
	if ( bRequireRequest &&
		(xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		 ) == NULL) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP response submission requires an active request",
			NULL
		);
		return false;
	}
	return true;
}



/* 安装信息或最终响应并启动写时限。 */
static xnetresult __xrtHttpConnStartResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	bool bInformation
)
{
	__xrt_http_response_queue* pQueued;

	if ( pConnection->Response != NULL ) {
		pQueued = (__xrt_http_response_queue*)xrtCalloc(
			1, sizeof(*pQueued)
		);
		if ( pQueued == NULL ) {
			xrtHttp1ServerResponseDestroy(pResponse);
			__xrtHttpServerSetError(
				XERR_MEMORY,
				XHTTP_SERVER_ERROR_RESPONSE,
				bInformation ?
					"queue-http-information" :
					"queue-http-response",
				"HTTP response queue allocation failed",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
		pQueued->Response = pResponse;
		pQueued->Information = bInformation;
		if ( !bInformation ) {
			xrtAtomic32Store(
				&pConnection->RequestBodyPaused,
				0,
				XMEMORY_RELEASE
			);
		}
		xrtHttpBridge1ServerResponseOwnRefs(pResponse);
		if ( pConnection->ResponseTail != NULL ) {
			pConnection->ResponseTail->Next = pQueued;
		} else {
			pConnection->ResponseHead = pQueued;
		}
		pConnection->ResponseTail = pQueued;
		return XNET_RESULT_OK;
	}
	pConnection->Response = pResponse;
	pConnection->ResponseInformation = bInformation;
	if ( !bInformation ) {
		xrtAtomic32Store(
			&pConnection->RequestBodyPaused,
			0,
			XMEMORY_RELEASE
		);
	}
	xrtAtomic32Store(
		&pConnection->State,
		bInformation ?
			XHTTP_CONN_INFORMATION :
			XHTTP_CONN_RESPONSE,
		XMEMORY_RELEASE
	);
	__xrtHttpConnCancelTimer(pConnection);
	__xrtHttpConnPauseInput(pConnection);
	pConnection->WriteDeadline =
		pConnection->Server->Config.WriteTimeout != 0 ?
			__xrtWaitAfter(
				pConnection->Server->Config.WriteTimeout
			) : 0;
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_WRITE,
		pConnection->Server->Config.WriteTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return XNET_RESULT_ERROR;
	}
	xrtHttpBridge1ServerResponseOwnRefs(pResponse);
	__xrtHttpConnDriveOutput(pConnection);
	return XNET_RESULT_OK;
}



/*
	把已经拥有的响应计划提交给唯一最终响应门。
	普通响应拒绝 Tunnel，Upgrade 组合层可以显式接管该终态。
*/
xnetresult __xrtHttpConnCommitResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	cstr sOperation,
	bool bAllowTunnel
)
{
	uint32 iExpected = 0;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			sOperation,
			"HTTP final response plan is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( xrtHttp1ServerResponseTunnel(pResponse) &&
		!bAllowTunnel ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_UNSUPPORTED,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP tunnel response requires the Upgrade transport layer",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)
		__xrtHttpConnFutureDetach(pConnection, true);
	#endif
	if ( !xrtAtomic32CompareExchange(
		&pConnection->FinalGate,
		&iExpected,
		1,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection raced another final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	{
		xnetresult Result = __xrtHttpConnStartResponse(
			pConnection, pResponse, false
		);

		if ( Result != XNET_RESULT_OK ) {
			xrtAtomic32Store(
				&pConnection->FinalGate,
				0,
				XMEMORY_RELEASE
			);
		}
		return Result;
	}
}



/* 提交一条信息响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnInform(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	xhttp1serverresponse* pResponse;

	if ( !__xrtHttpConnCanRespond(
		pConnection, "inform-http-connection", true
	) || (pReply == NULL) ) {
		if ( pReply == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"inform-http-connection",
				"HTTP information Reply is null",
				NULL
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"inform-http-connection",
			"HTTP information cannot follow the final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( pConnection->InformationCount >=
		pConnection->Server->Config.MaxInformations ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_RESPONSE,
			"inform-http-connection",
			"HTTP information response limit was reached",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnPrepareInformation(
		pConnection, pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"inform-http-connection",
			"HTTP information could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	{
		xnetresult Result = __xrtHttpConnStartResponse(
		pConnection, pResponse, true
		);

		if ( Result == XNET_RESULT_OK ) {
			pConnection->InformationCount++;
		}
		return Result;
	}
}



/* 提交唯一最终响应；协议错误可在请求头尚未建立时使用此内部路径。 */
static xnetresult __xrtHttpConnSubmitFinal(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	cstr sOperation,
	bool bRequireRequest
)
{
	xhttp1serverresponse* pResponse;

	if ( !__xrtHttpConnCanRespond(
		pConnection, sOperation, bRequireRequest
	) || (pReply == NULL) ) {
		if ( pReply == NULL ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				sOperation,
				"HTTP final Reply is null",
				NULL
			);
		}
		return XNET_RESULT_ERROR;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection already committed its final response",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnPrepareResponse(
		pConnection, pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP final response could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		sOperation,
		false
	);
}



/* 提交应用层唯一最终响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespond(
	xhttpconn* pConnection,
	const xhttpreply* pReply
)
{
	return __xrtHttpConnSubmitFinal(
		pConnection,
		pReply,
		"respond-http-connection",
		true
	);
}



/* 验证直接响应的连接、状态和可选 Content-Type。 */
static bool __xrtHttpConnReplyValidate(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	cstr sOperation,
	bool bRequireRequest
)
{
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		sOperation,
		bRequireRequest
	) ) {
		return false;
	}
	if ( (iStatus < 100) || (iStatus > 999) ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response status is outside 100..999",
			NULL
		);
		return false;
	}
	if ( (ContentType.Data == NULL) &&
		(ContentType.Size != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response Content-Type view is invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpFieldValueValid(ContentType) ) {
		__xrtHttpServerSetError(
			XERR_VALUE,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response Content-Type contains an invalid field value",
			NULL
		);
		return false;
	}
	return true;
}



/* 用已经验证的输入冻结 Body 并提交唯一最终响应。 */
static xnetresult __xrtHttpConnSubmitBodyValidated(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody,
	cstr sOperation
)
{
	xhttpresponsesource Source;
	xhttpfield ContentTypeField;
	xhttp1serverresponse* pResponse;
	xhttpversion Version;
	xstrview Method;
	uint32 iFlags;

	memset(&Source, 0, sizeof(Source));
	ContentTypeField.Name = XRT_STR_LITERAL("Content-Type");
	ContentTypeField.Value = ContentType;
	Source.Status = iStatus;
	Source.Reason = xrtHttpStatusText(iStatus);
	Source.Headers = ContentType.Size != 0 ?
		&ContentTypeField : NULL;
	Source.HeaderCount = ContentType.Size != 0 ? 1 : 0;
	Source.Body = pBody;
	Source.BodyLength = pBody != NULL ?
		xrtHttpBodyLength(pBody) : 0;
	__xrtHttpConnResponseFacts(
		pConnection,
		&Version,
		&Method,
		&iFlags
	);
	pResponse = xrtHttpBridge1ServerResponsePrepareSource(
		Version,
		Method,
		iFlags,
		&Source
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(), XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response could not be prepared",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		sOperation,
		false
	);
}



/* 构建并提交固定正文；内部协议错误可以不依赖完整请求。 */
static xnetresult __xrtHttpConnSubmitFixed(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body,
	cstr sOperation,
	bool bRequireRequest
)
{
	xhttpbody* pBody = NULL;
	xnetresult Result;

	if ( !__xrtHttpConnReplyValidate(
		pConnection,
		iStatus,
		ContentType,
		sOperation,
		bRequireRequest
	) ) {
		return XNET_RESULT_ERROR;
	}
	if ( (Body.Data == NULL) && (Body.Size != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_RESPONSE,
			sOperation,
			"HTTP response body view is invalid",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( Body.Size != 0 ) {
		pBody = xrtHttpBodyCopy(Body);
		if ( pBody == NULL ) {
			__xrtHttpServerSetError(
				__xrtHttpServerCauseKind(
					xrtGetError(), XERR_MEMORY
				),
				XHTTP_SERVER_ERROR_RESPONSE,
				sOperation,
				"HTTP response body could not be copied",
				xrtGetError()
			);
			return XNET_RESULT_ERROR;
		}
	}
	Result = __xrtHttpConnSubmitBodyValidated(
		pConnection,
		iStatus,
		ContentType,
		pBody,
		sOperation
	);
	xrtHttpBodyDestroy(pBody);
	return Result;
}



/* 一次调用构建并提交常用固定正文。 */
XHTTP_SERVER_API xnetresult xrtHttpConnReply(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xbytesview Body
)
{
	return __xrtHttpConnSubmitFixed(
		pConnection,
		iStatus,
		ContentType,
		Body,
		"respond-http-connection",
		true
	);
}



/* 一次调用保留正文来源并提交常规 HTTP 响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnReplyBody(
	xhttpconn* pConnection,
	uint16 iStatus,
	xstrview ContentType,
	xhttpbody* pBody
)
{
	if ( !__xrtHttpConnReplyValidate(
		pConnection,
		iStatus,
		ContentType,
		"respond-http-connection",
		true
	) ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnSubmitBodyValidated(
		pConnection,
		iStatus,
		ContentType,
		pBody,
		"respond-http-connection"
	);
}



/* 发送自动 100 Continue。 */
bool __xrtHttpConnContinue(xhttpconn* pConnection)
{
	xhttpreply* pReply = xrtHttpReplyCreate(100);
	xnetresult Result;

	if ( pReply == NULL ) {
		return false;
	}
	Result = xrtHttpConnInform(pConnection, pReply);
	xrtHttpReplyDestroy(pReply);
	return Result == XNET_RESULT_OK;
}



/* 完成信息响应后恢复请求读取或等待最终响应。 */
static void __xrtHttpConnInformationDone(
	xhttpconn* pConnection
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);
	bool bRequestBodyPaused = xrtAtomic32Load(
		&pConnection->RequestBodyPaused,
		XMEMORY_ACQUIRE
	) != 0;

	if ( xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	) ) {
		xrtAtomic32Store(
			&pConnection->State,
			XHTTP_CONN_WAITING,
			XMEMORY_RELEASE
		);
		if ( !__xrtHttpConnArmTimer(
			pConnection,
			XRT_HTTP_SERVER_TIMER_REQUEST,
			pConnection->Server->Config.RequestTimeout
		) ) {
			(void)xrtHttpConnAbort(pConnection);
		}
		return;
	}
	if ( !bRequestBodyPaused &&
		xrtHttp1ServerExchangePaused(
			pConnection->Exchange
		) && !__xrtHttpConnResumeExchange(pConnection) ) {
		return;
	}
	xrtAtomic32Store(
		&pConnection->State,
		(pRequest != NULL) &&
		(xrtHttpServerRequestBodyMode(pRequest) !=
		 XHTTP1_BODY_NONE) ?
			XHTTP_CONN_BODY :
			XHTTP_CONN_REQUEST,
		XMEMORY_RELEASE
	);
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		pRequest != NULL ?
			XRT_HTTP_SERVER_TIMER_BODY :
			XRT_HTTP_SERVER_TIMER_HEADER,
		pRequest != NULL ?
			pConnection->Server->Config.BodyTimeout :
			pConnection->Server->Config.HeaderTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( bRequestBodyPaused ) {
		__xrtHttpConnPauseInput(pConnection);
		return;
	}
	__xrtHttpConnResumeInput(pConnection);
	__xrtHttpConnDriveInput(pConnection);
}



/* 完成最终响应后关闭或进入下一条 keep-alive 请求。 */
static void __xrtHttpConnFinalDone(
	xhttpconn* pConnection,
	bool bResponseClose
)
{
	xhttpserver* pServer;
	bool bClose;

	/* 捕获 Connection 持有的稳定 Server 引用，避免完成路径反复读取。 */
	if ( pConnection == NULL ) {
		return;
	}
	pServer = pConnection->Server;
	if ( pServer == NULL ) {
		return;
	}

	bClose = bResponseClose ||
		pConnection->ForceClose ||
		pConnection->InputEnded ||
		(xrtHttpServerState(pServer) !=
		 XHTTP_SERVER_RUNNING);

	(void)xrtAtomic64FetchAdd(
		&pServer->Responses,
		1,
		XMEMORY_RELAXED
	);
	if ( bClose ) {
		(void)xrtHttpConnClose(pConnection);
		return;
	}
	if ( !xrtHttp1ServerExchangeNext(
		pConnection->Exchange
	) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_PROTOCOL,
			"reuse-http-connection",
			"HTTP connection could not enter its next request",
			xrtGetError()
		);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	xrtAtomic32Store(
		&pConnection->FinalGate,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->RequestActive,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_REQUEST,
		XMEMORY_RELEASE
	);
	pConnection->ForceClose = false;
	pConnection->InformationCount = 0;
	if ( !__xrtHttpConnArmTimer(
		pConnection,
		XRT_HTTP_SERVER_TIMER_IDLE,
		pServer->Config.IdleTimeout
	) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	__xrtHttpConnResumeInput(pConnection);
	__xrtHttpConnDriveInput(pConnection);
}



/* 回收完成响应并推进 Connection 后续阶段。 */
static void __xrtHttpConnResponseDone(xhttpconn* pConnection)
{
	xhttp1serverresponse* pResponse;
	__xrt_http_response_queue* pQueued;
	bool bInformation;
	bool bClose;

	/* Connection 生命周期必须始终持有 Server；拒绝损坏状态下的空地址原子写。 */
	if ( (pConnection == NULL) || (pConnection->Server == NULL) ) {
		return;
	}
	pResponse = pConnection->Response;
	bInformation = pConnection->ResponseInformation;
	bClose = xrtHttp1ServerResponseClose(pResponse);

	__xrtHttpConnCancelTimer(pConnection);
	pConnection->WriteDeadline = 0;
	pConnection->WritePending = 0;
	pConnection->OutputDraining = false;
	pConnection->Response = NULL;
	pConnection->ResponseInformation = false;
	xrtHttp1ServerResponseDestroy(pResponse);
	if ( bInformation ) {
		(void)xrtAtomic64FetchAdd(
			&pConnection->Server->Informations,
			1,
			XMEMORY_RELAXED
		);
	}
	pQueued = pConnection->ResponseHead;
	if ( pQueued != NULL ) {
		pConnection->ResponseHead = pQueued->Next;
		if ( pConnection->ResponseHead == NULL ) {
			pConnection->ResponseTail = NULL;
		}
		pConnection->Response = pQueued->Response;
		pConnection->ResponseInformation =
			pQueued->Information;
		xrtAtomic32Store(
			&pConnection->State,
			pQueued->Information ?
				XHTTP_CONN_INFORMATION :
				XHTTP_CONN_RESPONSE,
			XMEMORY_RELEASE
		);
		xrtFree(pQueued);
		pConnection->WriteDeadline =
			pConnection->Server->Config.WriteTimeout != 0 ?
				__xrtWaitAfter(
					pConnection->Server->
						Config.WriteTimeout
				) : 0;
		if ( !__xrtHttpConnArmTimer(
			pConnection,
			XRT_HTTP_SERVER_TIMER_WRITE,
			pConnection->Server->Config.WriteTimeout
		) ) {
			(void)xrtHttpConnAbort(pConnection);
			return;
		}
		__xrtHttpConnDriveOutput(pConnection);
	} else if ( bInformation ) {
		__xrtHttpConnInformationDone(pConnection);
	} else {
		__xrtHttpConnFinalDone(pConnection, bClose);
	}
}



/* 把输出继续工作排入所属 Worker。 */
static bool __xrtHttpConnPostOutput(
	xhttpconn* pConnection
);



/* 一个零复制 TCP 租约离队后才推进 Response 所有权。 */
static void __xrtHttpConnOutputRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	xhttpconn* pConnection = (xhttpconn*)pContext;
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	if ( pConnection->OutputQueued &&
		(pConnection->Response != NULL) &&
		(pConnection->Offered == iSize) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSING) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSED) &&
		(pStream != NULL) &&
		(xrtNetStreamState(pStream) ==
		 XNET_STREAM_OPEN) ) {
		pConnection->OutputQueued = false;
		pConnection->Offered = 0;
		if ( !xrtHttp1ServerResponseOutputConsume(
			pConnection->Response, iSize
		) ) {
			__xrtHttpConnRememberError(
				pConnection,
				XERR_STATE,
				XHTTP_SERVER_ERROR_RESPONSE,
				"consume-http-response-output",
				"HTTP response rejected a completed TCP output lease",
				xrtGetError()
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		} else {
			(void)xrtAtomic64FetchAdd(
				&pConnection->ResponseWireBytes,
				(uint64)iSize,
				XMEMORY_RELAXED
			);
			if ( pConnection->Server->Config.WriteTimeout != 0 ) {
				pConnection->WriteDeadline =
					__xrtWaitAfter(
						pConnection->Server->
							Config.WriteTimeout
					);
			}
			if ( !__xrtHttpConnPostOutput(pConnection) ) {
				__xrtHttpConnRememberError(
					pConnection,
					XERR_INTERNAL,
					XHTTP_SERVER_ERROR_INTERNAL,
					"continue-http-response-output",
					"HTTP response output could not return to its Worker",
					xrtGetError()
				);
				__xrtHttpConnEmitError(pConnection);
				(void)xrtHttpConnAbort(pConnection);
			}
		}
	}
	(void)pData;
	xrtHttpConnDestroy(pConnection);
}



/* 向明文 TCP 提交当前 Response 的一个稳定租约。 */
static xnetresult __xrtHttpConnSendTcpOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	xnetstream* pStream = (xnetstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xnetresult Result;

	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return XNET_RESULT_ERROR;
	}
	pConnection->Offered = Data.Size;
	pConnection->OutputQueued = true;
	Result = xrtNetStreamSendRef(
		pStream,
		Data.Data,
		Data.Size,
		__xrtHttpConnOutputRelease,
		pConnection
	);
	if ( Result != XNET_RESULT_OK ) {
		pConnection->OutputQueued = false;
		pConnection->Offered = 0;
		xrtHttpConnDestroy(pConnection);
	}
	return Result;
}



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

/* 把 TLS 已经接收的明文前缀立即提交给 Response 所有权。 */
static bool __xrtHttpConnTlsOutputConsume(
	xhttpconn* pConnection,
	size_t iSize
)
{
	if ( !xrtHttp1ServerResponseOutputConsume(
		pConnection->Response, iSize
	) ) {
		return false;
	}
	(void)xrtAtomic64FetchAdd(
		&pConnection->ResponseWireBytes,
		(uint64)iSize,
		XMEMORY_RELAXED
	);
	if ( pConnection->Server->Config.WriteTimeout != 0 ) {
		pConnection->WriteDeadline = __xrtWaitAfter(
			pConnection->Server->Config.WriteTimeout
		);
	}
	return true;
}



/* 向 TLS 提交明文，处理短写并等待下一次 Writable 边沿。 */
static xnetresult __xrtHttpConnSendTlsOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	xtlsstream* pStream = (xtlsstream*)xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);
	xtlsresult Result;
	size_t iWritten = 0;

	if ( pStream == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	Result = xrtTlsStreamSend(
		pStream,
		Data.Data,
		Data.Size,
		&iWritten
	);
	if ( (iWritten != 0) &&
		!__xrtHttpConnTlsOutputConsume(
			pConnection, iWritten
		) ) {
		return XNET_RESULT_ERROR;
	}
	if ( Result == XTLS_ERROR ) {
		return XNET_RESULT_ERROR;
	}
	if ( Result == XTLS_CLOSED ) {
		return XNET_RESULT_CLOSED;
	}
	if ( iWritten < Data.Size ) {
		pConnection->OutputQueued = true;
		return XNET_RESULT_AGAIN;
	}
	pConnection->OutputPending = true;
	return XNET_RESULT_OK;
}

#endif



/* 按 Connection 传输类型提交当前 Response 输出。 */
static xnetresult __xrtHttpConnSendOutput(
	xhttpconn* pConnection,
	xbytesview Data
)
{
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			return __xrtHttpConnSendTlsOutput(
				pConnection, Data
			);
		}
	#endif
	return __xrtHttpConnSendTcpOutput(
		pConnection, Data
	);
}



/* 在下一轮 Worker 调度中继续达到本轮预算的输出泵。 */
static void __xrtHttpConnOutputTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	pConnection->OutputPosted = false;
	__xrtHttpConnDriveOutput(pConnection);
	xrtHttpConnDestroy(pConnection);
}



/* 把响应剩余工作排入所属 Worker，避免重入 TCP 缓冲变更。 */
static bool __xrtHttpConnPostOutput(
	xhttpconn* pConnection
)
{
	if ( pConnection->OutputPosted ) {
		return true;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		return false;
	}
	pConnection->OutputPosted = true;
	if ( !xrtNetEnginePost(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		__xrtHttpConnOutputTask,
		pConnection
	) ) {
		pConnection->OutputPosted = false;
		xrtHttpConnDestroy(pConnection);
		return false;
	}
	return true;
}



/* 单步推进当前响应输出；后续租约由释放回调重新投递。 */
void __xrtHttpConnDriveOutput(xhttpconn* pConnection)
{
	xhttp1serveroutputstatus Status;
	xbytesview Data;
	xnetresult Result;

	if ( pConnection == NULL ) {
		return;
	}
	if ( pConnection->OutputDriving ) {
		pConnection->OutputPending = true;
		return;
	}
	pConnection->OutputDriving = true;
	pConnection->OutputPending = false;
	if ( (pConnection->Response == NULL) ||
		pConnection->OutputQueued ||
		#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
			pConnection->BodyWaiting ||
		#endif
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		goto Finish;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->OutputDraining ) {
			xtlsstream* pStream = (xtlsstream*)
				xrtAtomicPtrLoad(
					&pConnection->Transport,
					XMEMORY_ACQUIRE
				);

			if ( xrtTlsStreamPending(pStream) != 0 ) {
				goto Finish;
			}
			pConnection->OutputDraining = false;
			pConnection->WritePending = 0;
			__xrtHttpConnResponseDone(pConnection);
			goto Finish;
		}
	#endif
	Status = xrtHttp1ServerResponseOutput(
		pConnection->Response,
		pConnection->Server->Config.WriteSize,
		&Data
	);
	if ( Status == XHTTP1_SERVER_OUTPUT_DATA ) {
		Result = __xrtHttpConnSendOutput(
			pConnection, Data
		);
		if ( (Result != XNET_RESULT_OK) &&
			(Result != XNET_RESULT_AGAIN) ) {
			__xrtHttpConnRememberError(
				pConnection,
				Result == XNET_RESULT_CLOSED ?
					XERR_CLOSED : XERR_IO,
				XHTTP_SERVER_ERROR_CONNECTION,
				"send-http-response",
				"HTTP response could not enter the transport send queue",
				xrtGetError()
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		}
	} else if ( Status == XHTTP1_SERVER_OUTPUT_DONE ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				xtlsstream* pStream = (xtlsstream*)
					xrtAtomicPtrLoad(
						&pConnection->Transport,
						XMEMORY_ACQUIRE
					);
				size_t iPending =
					xrtTlsStreamPending(pStream);

				if ( iPending != 0 ) {
					pConnection->WritePending =
						iPending;
					pConnection->OutputDraining =
						true;
					goto Finish;
				}
			}
		#endif
		__xrtHttpConnResponseDone(pConnection);
	} else if ( Status == XHTTP1_SERVER_OUTPUT_AGAIN ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)
			if ( __xrtHttpConnBodyWait(pConnection) ) {
				goto Finish;
			}
			__xrtHttpConnRememberError(
				pConnection,
				__xrtHttpServerCauseKind(
					xrtGetError(),
					XERR_IO
				),
				XHTTP_SERVER_ERROR_RESPONSE,
				"read-http-response-body",
				"HTTP response body readiness wait could not be installed",
				xrtGetError()
			);
		#else
			__xrtHttpConnRememberError(
				pConnection,
				XERR_UNSUPPORTED,
				XHTTP_SERVER_ERROR_RESPONSE,
				"read-http-response-body",
				"HTTP response body requires the async server body layer",
				NULL
			);
		#endif
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	} else if ( Status == XHTTP1_SERVER_OUTPUT_TUNNEL ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)
			__xrtHttpConnUpgradeFinish(pConnection);
		#else
			__xrtHttpConnRememberError(
				pConnection,
				XERR_UNSUPPORTED,
				XHTTP_SERVER_ERROR_RESPONSE,
				"upgrade-http-connection",
				"HTTP tunnel reached a server without Upgrade support",
				NULL
			);
			__xrtHttpConnEmitError(pConnection);
			(void)xrtHttpConnAbort(pConnection);
		#endif
	} else {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_IO,
			XHTTP_SERVER_ERROR_RESPONSE,
			"write-http-response",
			"HTTP response output failed",
			xrtHttp1ServerResponseError(
				pConnection->Response
			)
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}

Finish:
	pConnection->OutputDriving = false;
	if ( pConnection->OutputPending &&
		!pConnection->OutputQueued &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSING) &&
		(xrtHttpConnState(pConnection) !=
		 XHTTP_CONN_CLOSED) &&
		!__xrtHttpConnPostOutput(pConnection) ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_INTERNAL,
			XHTTP_SERVER_ERROR_INTERNAL,
			"continue-http-response-output",
			"HTTP response output could not yield to its Worker",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 保存协议错误并尽力发送一个关闭连接的固定状态响应。 */
void __xrtHttpConnProtocolFail(
	xhttpconn* pConnection,
	uint16 iStatus,
	xhttpservererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xstrview Reason;

	if ( (pConnection == NULL) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		return;
	}
	__xrtHttpConnRememberError(
		pConnection,
		Code >= XHTTP_SERVER_ERROR_TIMEOUT_HEADER &&
		Code <= XHTTP_SERVER_ERROR_TIMEOUT_WRITE ?
			XERR_TIMEOUT : XERR_PROTOCOL,
		Code,
		sOperation,
		sMessage,
		pCause
	);
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->ProtocolErrors,
		1,
		XMEMORY_RELAXED
	);
	__xrtHttpConnEmitError(pConnection);
	pConnection->ForceClose = true;
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) || (pConnection->Response != NULL) ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	Reason = xrtHttpStatusText(iStatus);
	if ( __xrtHttpConnSubmitFixed(
		pConnection,
		iStatus,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		(xbytesview){
			(cbytes)Reason.Data,
			Reason.Size
		},
		"respond-http-protocol-error",
		false
	) != XNET_RESULT_OK ) {
		(void)xrtHttpConnAbort(pConnection);
	}
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_tls.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)



#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)

static const xstrview __xrtHttpServerTlsProtocols[] = {
	XRT_STR_INIT("http/1.1")
};



/* 判断当前错误是否已经由 HTTP Server 层建立。 */
static bool __xrtHttpServerTlsErrorOwned(const xerror* pError)
{
	cstr sDomain;

	if ( pError == NULL ) {
		return false;
	}
	sDomain = xrtErrorDomain(pError);
	return (sDomain != NULL) &&
		(strcmp(sDomain, "xrt.http.server") == 0);
}



/* 判断一个 ALPN 标识是否是当前 Server 支持的 HTTP/1.1。 */
static bool __xrtHttpServerTlsProtocolValid(xstrview Protocol)
{
	static const char sHttp11[] = "http/1.1";

	return (Protocol.Size == (sizeof(sHttp11) - 1u)) &&
		xrtMemRangeValid(Protocol.Data, Protocol.Size) &&
		(memcmp(
			Protocol.Data,
			sHttp11,
			sizeof(sHttp11) - 1u
		) == 0);
}



/* 深复制 ALPN 数组和全部协议字节。 */
static bool __xrtHttpServerTlsProtocolsCopy(
	xhttpserver* pServer,
	const xtlsserverconfig* pConfig
)
{
	xstrview* pProtocols;
	xstrview Protocol;
	char* pBytes;

	if ( pConfig->ProtocolCount == 0 ) {
		pServer->Tls.Protocols = NULL;
		pServer->Tls.ProtocolCount = 0;
		return true;
	}
	if ( (pConfig->Protocols == NULL) ||
		(pConfig->ProtocolCount != 1u) ||
		!xrtMemRangeValid(
			pConfig->Protocols,
			sizeof(Protocol)
		) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS HTTP/1 server ALPN must be http/1.1",
			NULL
		);
		return false;
	}
	memcpy(&Protocol, (const void*)pConfig->Protocols, sizeof(Protocol));
	if ( !__xrtHttpServerTlsProtocolValid(Protocol) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS HTTP/1 server ALPN must be http/1.1",
			NULL
		);
		return false;
	}
	pProtocols = (xstrview*)xrtMalloc(
		sizeof(*pProtocols) + Protocol.Size
	);
	if ( pProtocols == NULL ) {
		return false;
	}
	pBytes = (char*)(pProtocols + 1u);
	pProtocols[0].Data = pBytes;
	pProtocols[0].Size = Protocol.Size;
	memcpy(pBytes, Protocol.Data, Protocol.Size);
	pServer->TlsProtocols = pProtocols;
	pServer->Tls.Protocols = pProtocols;
	pServer->Tls.ProtocolCount = 1u;
	return true;
}



/* 初始化 HTTP/1.1 TLS Server 默认配置。 */
XHTTP_SERVER_API void xrtHttpServerTlsConfigInit(
	xhttpservertlsconfig* pConfig
)
{
	xhttpservertlsconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"init-https-server-config",
			"HTTPS server TLS config range is invalid",
			NULL
		);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtTlsServerConfigInit(&Config.Handshake);
	xrtTlsStreamConfigInit(&Config.Stream);
	Config.Handshake.Protocols =
		__xrtHttpServerTlsProtocols;
	Config.Handshake.ProtocolCount =
		sizeof(__xrtHttpServerTlsProtocols) /
		sizeof(__xrtHttpServerTlsProtocols[0]);
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 读取可选 TLS 配置，并建立一次对齐且不可变的启动快照。 */
static bool __xrtHttpServerTlsConfigRead(
	const xhttpservertlsconfig* pInput,
	xhttpservertlsconfig* pConfig
)
{
	xrtHttpServerTlsConfigInit(pConfig);
	if ( pInput == NULL ) {
		return true;
	}
	if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"configure-https-server",
			"HTTPS server TLS config range is invalid",
			NULL
		);
		return false;
	}
	memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	return true;
}



/* 释放 Server 保存的 TLS 快照。 */
void __xrtHttpServerTlsCleanup(xhttpserver* pServer)
{
	if ( pServer == NULL ) {
		return;
	}
	xrtFree(pServer->TlsProtocols);
	xrtTlsIdentityRelease(pServer->TlsIdentity);
	xrtTlsContextRelease(pServer->TlsContext);
	memset(&pServer->Tls, 0, sizeof(pServer->Tls));
	memset(&pServer->TlsStream, 0, sizeof(pServer->TlsStream));
	pServer->TlsProtocols = NULL;
	pServer->TlsIdentity = NULL;
	pServer->TlsContext = NULL;
	pServer->Secure = false;
}



/* 保存 TLS 对象、ALPN 和回调配置，并提前验证组合边界。 */
static bool __xrtHttpServerTlsSetupSnapshot(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
)
{
	const xtlslimits* pLimits;
	xtlssession* pSession;
	xerror* pCause;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"configure-https-server",
			"HTTPS server is null",
			NULL
		);
		return false;
	}
	if ( pServer->Secure ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS server is already configured",
			NULL
		);
		return false;
	}
	pServer->Tls = pConfig->Handshake;
	pServer->TlsStream = pConfig->Stream;
	if ( pConfig->Handshake.Context != NULL ) {
		pServer->TlsContext = xrtTlsContextRetain(
			pConfig->Handshake.Context
		);
	} else {
		pServer->TlsContext = xrtTlsContextCreate(NULL);
	}
	if ( pServer->TlsContext == NULL ) {
		goto Failed;
	}
	pServer->Tls.Context = pServer->TlsContext;
	if ( pConfig->Handshake.Identity != NULL ) {
		pServer->TlsIdentity = xrtTlsIdentityRetain(
			pConfig->Handshake.Identity
		);
		if ( pServer->TlsIdentity == NULL ) {
			goto Failed;
		}
	}
	pServer->Tls.Identity = pServer->TlsIdentity;
	if ( !__xrtHttpServerTlsProtocolsCopy(
		pServer, &pConfig->Handshake
	) ) {
		goto Failed;
	}
	pLimits = xrtTlsContextLimits(pServer->TlsContext);
	if ( (pLimits == NULL) ||
		!__xrtHttpServerNetworkWriteLimit(
			&pServer->Config,
			pLimits->SendLimit
		) ) {
		__xrtHttpServerSetError(
			XERR_RANGE,
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS TCP write limit is smaller than the TLS send limit",
			NULL
		);
		goto Failed;
	}
	pSession = xrtTlsServerCreate(&pServer->Tls, NULL);
	if ( pSession == NULL ) {
		pCause = xrtErrorRef(xrtGetError());
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				pCause, XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS TLS server configuration is invalid",
			pCause
		);
		xrtErrorFree(pCause);
		goto Failed;
	}
	xrtTlsSessionDestroy(pSession);
	pServer->Secure = true;
	return true;

Failed:
	pCause = xrtErrorRef(xrtGetError());
	__xrtHttpServerTlsCleanup(pServer);
	if ( __xrtHttpServerTlsErrorOwned(pCause) ) {
		xrtSetError(pCause);
	} else {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				pCause, XERR_ARGUMENT
			),
			XHTTP_SERVER_ERROR_CONFIG,
			"configure-https-server",
			"HTTPS server could not save its TLS configuration",
			pCause
		);
	}
	xrtErrorFree(pCause);
	return false;
}



/* 校验并快照配置后，为尚未监听的 Server 建立 TLS 运行时。 */
bool __xrtHttpServerTlsSetup(
	xhttpserver* pServer,
	const xhttpservertlsconfig* pConfig
)
{
	xhttpservertlsconfig Config;

	if ( !__xrtHttpServerTlsConfigRead(pConfig, &Config) ) {
		return false;
	}
	return __xrtHttpServerTlsSetupSnapshot(pServer, &Config);
}



/* 创建并启动 TLS HTTP/1 Server。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	const xhttpserverevents* pEvents
)
{
	xhttpservertlsconfig Tls;
	xhttpserver* pServer;
	xerror* pError;

	if ( !__xrtHttpServerTlsConfigRead(pTls, &Tls) ) {
		return NULL;
	}
	pServer = __xrtHttpServerCreate(
		pEngine, pConfig, pEvents
	);
	if ( pServer == NULL ) {
		return NULL;
	}
	if ( !__xrtHttpServerTlsSetupSnapshot(pServer, &Tls) ) {
		pError = xrtTakeError();
		xrtHttpServerDestroy(pServer);
		xrtSetError(pError);
		xrtErrorFree(pError);
		return NULL;
	}
	return __xrtHttpServerListen(pServer);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_body_async.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)



#if defined(XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC)

/* 释放 Future 等待节点持有的 Connection 引用。 */
static void __xrtHttpConnBodyRelease(ptr pData)
{
	xrtHttpConnDestroy((xhttpconn*)pData);
}



/* 把非成功 Future 终态转换为可保留的结构化原因。 */
static xerror* __xrtHttpConnBodyError(
	const xfutureresult* pResult
)
{
	xerror* pError;

	if ( pResult->State == XFUTURE_FAILED ) {
		if ( pResult->Error != NULL ) {
			return xrtErrorRef(pResult->Error);
		}
		pError = xrtErrorCreate(
			XERR_INTERNAL,
			"xrt.future",
			(int32)XFUTURE_FAILED,
			"HTTP response body readiness Future failed without an error"
		);
	} else if ( pResult->State == XFUTURE_CANCELLED ) {
		pError = xrtErrorCreate(
			XERR_CANCELLED,
			"xrt.future",
			(int32)XFUTURE_CANCELLED,
			"HTTP response body readiness wait was cancelled"
		);
	} else {
		pError = xrtErrorCreate(
			XERR_CLOSED,
			"xrt.future",
			(int32)pResult->State,
			"HTTP response body readiness Future closed without a value"
		);
	}
	if ( pError == NULL ) {
		pError = xrtErrorRef(xrtGetError());
	}
	return pError;
}



/* 在所属 Worker 上消费 Future 终态并恢复或结束响应。 */
static void __xrtHttpConnBodyReady(
	xhttpconn* pConnection
)
{
	xfutureresult Result;
	xfuture* pFuture = pConnection->BodyFuture;
	xerror* pCause = NULL;

	if ( (pFuture == NULL) ||
		!pConnection->BodyWaiting ) {
		return;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	if ( !xrtFutureResult(pFuture, &Result) ) {
		pCause = xrtErrorRef(xrtGetError());
	} else if ( Result.State != XFUTURE_RESOLVED ) {
		pCause = __xrtHttpConnBodyError(&Result);
	}
	xrtFutureDestroy(pFuture);
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		xrtErrorFree(pCause);
		return;
	}
	if ( pCause != NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			__xrtHttpServerCauseKind(
				pCause,
				XERR_IO
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"wait-http-response-body",
			"HTTP response body readiness wait did not succeed",
			pCause
		);
		xrtErrorFree(pCause);
		__xrtHttpConnEmitError(pConnection);
		pConnection->ForceClose = true;
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	__xrtHttpConnDriveOutput(pConnection);
}



/* Engine 投递回到所属 Worker 后消费 Future 终态。 */
static void __xrtHttpConnBodyReadyTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	(void)pWorker;
	__xrtHttpConnBodyReady(pConnection);
	xrtHttpConnDestroy(pConnection);
}



/* Future 完成线程只负责把恢复动作送回 Connection Worker。 */
static void __xrtHttpConnBodyNotify(ptr pData)
{
	xhttpconn* pConnection = (xhttpconn*)pData;

	if ( xrtNetWorkerIsCurrent(pConnection->Worker) ) {
		__xrtHttpConnBodyReady(pConnection);
		return;
	}
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	if ( !xrtNetEnginePost(
		pConnection->Server->Engine,
		xrtNetWorkerIndex(pConnection->Worker),
		__xrtHttpConnBodyReadyTask,
		pConnection
	) ) {
		xrtHttpConnDestroy(pConnection);
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 订阅一次正文可读性 Future，并覆盖立即完成竞态。 */
bool __xrtHttpConnBodyWait(xhttpconn* pConnection)
{
	xfuture* pFuture;
	xfuturewatchresult WatchResult;

	if ( (pConnection == NULL) ||
		pConnection->BodyWaiting ||
		(pConnection->BodyFuture != NULL) ||
		(pConnection->Response == NULL) ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	pFuture = xrtHttp1ServerResponseWait(
		pConnection->Response
	);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !xrtFutureWatchInit(
		&pConnection->BodyWatch,
		__xrtHttpConnBodyNotify,
		__xrtHttpConnBodyRelease,
		pConnection
	) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pConnection->BodyFuture = pFuture;
	pConnection->BodyWaiting = true;
	if ( xrtHttpConnRef(pConnection) == NULL ) {
		pConnection->BodyFuture = NULL;
		pConnection->BodyWaiting = false;
		xrtFutureDestroy(pFuture);
		return false;
	}
	WatchResult = xrtFutureWatchAdd(
		pFuture,
		&pConnection->BodyWatch
	);
	if ( WatchResult == XFUTURE_WATCH_PENDING ) {
		/*
		 * 正文生产者挂起期间恢复明文 TCP 的有界读取；TLS 继续由
		 * 未消费明文施加背压，二者都不会推进下一条流水线请求。
		 */
		__xrtHttpConnResumeInput(pConnection);
		return true;
	}
	xrtHttpConnDestroy(pConnection);
	if ( WatchResult == XFUTURE_WATCH_READY ) {
		__xrtHttpConnBodyNotify(pConnection);
		return true;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	xrtFutureDestroy(pFuture);
	return false;
}



/* 停止连接时请求取消来源，并等待并发通知离开内嵌节点。 */
void __xrtHttpConnBodyStop(
	xhttpconn* pConnection,
	bool bCancel
)
{
	xfuture* pFuture;

	if ( pConnection == NULL ) {
		return;
	}
	pFuture = pConnection->BodyFuture;
	if ( pFuture == NULL ) {
		pConnection->BodyWaiting = false;
		return;
	}
	pConnection->BodyFuture = NULL;
	pConnection->BodyWaiting = false;
	if ( bCancel ) {
		(void)xrtFutureCancel(pFuture);
	}
	xrtFutureWatchRemove(
		pFuture,
		&pConnection->BodyWatch
	);
	xrtFutureDestroy(pFuture);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_raw.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)



#if defined(XHTTP_FEATURE_HTTP_SERVER_RAW)

/* 检查原始报文长度、标志、Worker 和唯一最终响应门。 */
static bool __xrtHttpConnRawValidate(
	xhttpconn* pConnection,
	uint64 iLength,
	uint32 iFlags
)
{
	if ( (iLength == 0) ||
		(iLength == XHTTP_BODY_UNKNOWN) ||
		((iFlags & ~XHTTP_SERVER_RAW_KEEP_ALIVE) != 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response requires a known nonzero length and valid flags",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		"respond-raw-http-connection",
		true
	) ) {
		return false;
	}
	if ( xrtAtomic32Load(
		&pConnection->FinalGate,
		XMEMORY_ACQUIRE
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"respond-raw-http-connection",
			"HTTP connection already committed its final response",
			NULL
		);
		return false;
	}
	return true;
}



/* 计算原始报文排空后能否安全进入下一条 HTTP 请求。 */
static bool __xrtHttpConnRawKeepAlive(
	xhttpconn* pConnection,
	uint32 iFlags
)
{
	const xhttpserverrequest* pRequest =
		xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		);

	return ((iFlags & XHTTP_SERVER_RAW_KEEP_ALIVE) != 0) &&
		(pRequest != NULL) &&
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_KEEP_ALIVE) != 0) &&
		xrtHttp1ServerExchangeComplete(
			pConnection->Exchange
		) &&
		!pConnection->ForceClose &&
		!pConnection->InputEnded &&
		(xrtHttpServerState(pConnection->Server) ==
		 XHTTP_SERVER_RUNNING);
}



/* 冻结原始 Body 为不附加任何协议字节的定长响应计划。 */
static xhttp1serverresponse* __xrtHttpConnRawPlan(
	xhttpconn* pConnection,
	xhttpbody* pBody,
	uint64 iLength,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse =
		__xrtHttpConnWireResponse(
			pBody,
			iLength,
			!__xrtHttpConnRawKeepAlive(
				pConnection,
				iFlags
			),
			false
		);

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response plan could not be created",
			xrtGetError()
		);
		return NULL;
	}
	return pResponse;
}



/* 验证并计算一组完整线缆引用的总长度。 */
static bool __xrtHttpConnRawRefsMeasure(
	const xnetref* pRefs,
	size_t iCount,
	uint64* pLength
)
{
	uint64 iLength = 0;

	if ( (pRefs == NULL) || (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(*pRefs))) ||
		!xrtMemRangeValid(pRefs, iCount * sizeof(*pRefs)) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response references are invalid",
			NULL
		);
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( pRefs[i].Size == 0 ) {
			continue;
		}
		if ( !xrtMemRangeValid(
			pRefs[i].Data,
			pRefs[i].Size
		) || (pRefs[i].Release == NULL) ) {
			__xrtHttpServerSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"respond-raw-http-connection",
				"Raw HTTP response reference requires valid bytes and release",
				NULL
			);
			return false;
		}
		#if SIZE_MAX > UINT64_MAX
			if ( pRefs[i].Size > (size_t)UINT64_MAX ) {
				__xrtHttpServerSetError(
					XERR_RANGE,
					XHTTP_SERVER_ERROR_ARGUMENT,
					"respond-raw-http-connection",
					"Raw HTTP response reference length overflowed",
					NULL
				);
				return false;
			}
		#endif
		if ( (uint64)pRefs[i].Size >
			(UINT64_MAX - iLength) ) {
			__xrtHttpServerSetError(
				XERR_RANGE,
				XHTTP_SERVER_ERROR_ARGUMENT,
				"respond-raw-http-connection",
				"Raw HTTP response reference length overflowed",
				NULL
			);
			return false;
		}
		iLength += (uint64)pRefs[i].Size;
	}
	if ( iLength == 0 ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response references are empty",
			NULL
		);
		return false;
	}
	*pLength = iLength;
	return true;
}



/* xrtMalloc 接管路径使用统一网络引用释放签名。 */
static void __xrtHttpConnRawTakeRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)iSize;
	xrtFree((ptr)pData);
}



/* 以已经验证的 Body 提交完整原始响应。 */
static xnetresult __xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pBody,
	uint64 iLength,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse =
		__xrtHttpConnRawPlan(
			pConnection,
			pBody,
			iLength,
			iFlags
		);

	if ( pResponse == NULL ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"respond-raw-http-connection",
		false
	);
}



/* 复制并提交一条完整的 HTTP/1 线缆响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	uint32 iFlags
)
{
	xhttpbody* pBody;
	xnetresult Result;

	if ( (Response.Size == 0) ||
		!xrtMemRangeValid(Response.Data, Response.Size) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response requires a complete nonempty byte range",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtHttpConnRawValidate(
		pConnection,
		(uint64)Response.Size,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	pBody = xrtHttpBodyCopy(Response);
	if ( pBody == NULL ) {
		__xrtHttpServerSetError(
			__xrtHttpServerCauseKind(
				xrtGetError(),
				XERR_MEMORY
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response bytes could not be copied",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	Result = __xrtHttpConnRespondRawBody(
		pConnection,
		pBody,
		(uint64)Response.Size,
		iFlags
	);
	xrtHttpBodyDestroy(pBody);
	return Result;
}



/* 原子接管一组完整线缆引用并提交零复制响应计划。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawRefs(
	xhttpconn* pConnection,
	const xnetref* pResponses,
	size_t iCount,
	uint32 iFlags
)
{
	xhttp1serverresponse* pResponse;
	uint64 iLength;

	if ( !__xrtHttpConnRawRefsMeasure(
		pResponses,
		iCount,
		&iLength
	) || !__xrtHttpConnRawValidate(
		pConnection,
		iLength,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnWireRefsResponse(
		pResponses,
		iCount,
		iLength,
		!__xrtHttpConnRawKeepAlive(pConnection, iFlags),
		false
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_RESPONSE,
			"respond-raw-http-connection",
			"Raw HTTP response reference plan could not be created",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"respond-raw-http-connection",
		false
	);
}



/* 接管一个完整线缆引用。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawRef(
	xhttpconn* pConnection,
	const xnetref* pResponse,
	uint32 iFlags
)
{
	return xrtHttpConnRespondRawRefs(
		pConnection,
		pResponse,
		1,
		iFlags
	);
}



/* 成功时接管由 xrtMalloc 分配的完整线缆响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawTake(
	xhttpconn* pConnection,
	ptr pResponse,
	size_t iSize,
	uint32 iFlags
)
{
	xnetref Ref = {
		(cbytes)pResponse,
		iSize,
		__xrtHttpConnRawTakeRelease,
		NULL
	};

	if ( (pResponse == NULL) || (iSize == 0) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response take requires nonempty xrtMalloc bytes",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnRespondRawRef(
		pConnection,
		&Ref,
		iFlags
	);
}



/* 保留一个已知长度 Body 并把它作为完整 HTTP/1 线缆响应。 */
XHTTP_SERVER_API xnetresult xrtHttpConnRespondRawBody(
	xhttpconn* pConnection,
	xhttpbody* pResponse,
	uint32 iFlags
)
{
	uint64 iLength;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"respond-raw-http-connection",
			"Raw HTTP response Body is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	iLength = xrtHttpBodyLength(pResponse);
	if ( !__xrtHttpConnRawValidate(
		pConnection,
		iLength,
		iFlags
	) ) {
		return XNET_RESULT_ERROR;
	}
	return __xrtHttpConnRespondRawBody(
		pConnection,
		pResponse,
		iLength,
		iFlags
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_upgrade.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)



#if defined(XHTTP_FEATURE_HTTP_SERVER_UPGRADE)

/* 释放与 HTTP Connection 传输类型匹配的 Upgrade 引用。 */
static void __xrtHttpUpgradeTransportAbort(
	uint32 iTransportKind,
	ptr pTransport
)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( iTransportKind == XRT_HTTP_CONN_TRANSPORT_TLS ) {
			(void)xrtTlsStreamAbort((xtlsstream*)pTransport);
			xrtTlsStreamDestroy((xtlsstream*)pTransport);
			return;
		}
	#else
		(void)iTransportKind;
	#endif
	(void)xrtNetStreamAbort((xnetstream*)pTransport);
	xrtNetStreamDestroy((xnetstream*)pTransport);
}



/* 异常关闭并清空一个尚未绑定到新协议对象的 Upgrade 传输。 */
XHTTP_SERVER_API void xrtHttpUpgradeAbort(xhttpupgrade* pUpgrade)
{
	xhttpupgrade Upgrade;
	xerror* pPrevious;
	xerror* pDiscard;

	if ( pUpgrade == NULL ) {
		return;
	}
	if ( !xrtMemRangeValid(pUpgrade, sizeof(Upgrade)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy(&Upgrade, (const void*)pUpgrade, sizeof(Upgrade));
	memset(pUpgrade, 0, sizeof(Upgrade));
	pPrevious = __xhttpErrorSwapOwned(NULL);
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( Upgrade.Tls != NULL ) {
			(void)xrtTlsStreamAbort(Upgrade.Tls);
			xrtTlsStreamDestroy(Upgrade.Tls);
		}
	#endif
	if ( Upgrade.Tcp != NULL ) {
		(void)xrtNetStreamAbort(Upgrade.Tcp);
		xrtNetStreamDestroy(Upgrade.Tcp);
	}
	pDiscard = __xhttpErrorSwapOwned(pPrevious);
	xrtErrorFree(pDiscard);
}



/* 检查延迟交付前传输是否仍然处于可接管状态。 */
static xnetresult __xrtHttpUpgradeTransportResult(
	uint32 iTransportKind,
	ptr pTransport,
	xerror** ppError
)
{
	*ppError = NULL;
	if ( pTransport == NULL ) {
		return XNET_RESULT_CLOSED;
	}
	#if !defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		(void)iTransportKind;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( iTransportKind == XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xtlsstream* pTls = (xtlsstream*)pTransport;
			xtlsstreamstate State = xrtTlsStreamState(pTls);

			if ( State == XTLS_STREAM_OPEN ) {
				return XNET_RESULT_OK;
			}
			*ppError = xrtErrorRef(xrtTlsStreamError(pTls));
			return State == XTLS_STREAM_FAILED ?
				XNET_RESULT_ERROR : XNET_RESULT_CLOSED;
		}
	#endif
	if ( xrtNetStreamState((xnetstream*)pTransport) ==
		XNET_STREAM_OPEN ) {
		return XNET_RESULT_OK;
	}
	*ppError = xrtErrorRef(
		xrtNetStreamError((xnetstream*)pTransport)
	);
	return *ppError != NULL ?
		XNET_RESULT_ERROR : XNET_RESULT_CLOSED;
}



/* 在下一次 Worker 调度中发布唯一 Upgrade 终态。 */
static void __xrtHttpConnUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	xhttpconn* pConnection = (xhttpconn*)pData;
	xhttpupgradeproc pProc = pConnection->Upgrade;
	ptr pUserData = pConnection->UpgradeData;
	ptr pTransport = pConnection->UpgradeTransport;
	xhttpserver* pServer = pConnection->Server;
	xerror* pError = pConnection->UpgradeError;
	xnetresult Result = pConnection->UpgradeResult;
	size_t iBuffered = pConnection->UpgradeBuffered;
	xhttpupgrade Upgrade;

	(void)pWorker;
	memset(&Upgrade, 0, sizeof(Upgrade));
	pConnection->Upgrade = NULL;
	pConnection->UpgradeData = NULL;
	pConnection->UpgradeTransport = NULL;
	pConnection->UpgradeError = NULL;
	pConnection->UpgradeBuffered = 0;
	pConnection->UpgradePosted = false;
	if ( Result == XNET_RESULT_OK ) {
		xerror* pTransportError = NULL;

		Result = __xrtHttpUpgradeTransportResult(
			pConnection->TransportKind,
			pTransport,
			&pTransportError
		);
		if ( Result != XNET_RESULT_OK ) {
			xrtErrorFree(pError);
			pError = pTransportError;
			__xrtHttpUpgradeTransportAbort(
				pConnection->TransportKind,
				pTransport
			);
			pTransport = NULL;
		}
	}
	if ( Result == XNET_RESULT_OK ) {
		#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
			if ( pConnection->TransportKind ==
				XRT_HTTP_CONN_TRANSPORT_TLS ) {
				Upgrade.Tls = (xtlsstream*)pTransport;
			} else
		#endif
		{
			Upgrade.Tcp = (xnetstream*)pTransport;
		}
		Upgrade.Buffered = iBuffered;
	}
	if ( pProc != NULL ) {
		pProc(
			pConnection,
			Result,
			Upgrade,
			pError,
			pUserData
		);
	}
	xrtErrorFree(pError);
	(void)xrtAtomic64FetchSub(
		&pServer->UpgradeCallbacks,
		1,
		XMEMORY_ACQ_REL
	);
	if ( pConnection->Counted ) {
		pConnection->Counted = false;
		__xrtHttpServerConnectionClosed(pServer);
	}
	if ( pConnection->RuntimeHeld ) {
		__xrtHttpConnAdapterRelease(pConnection);
		pConnection->RuntimeHeld = false;
		xrtHttpConnDestroy(pConnection);
	}
	__xrtHttpServerConnectionFinished(pServer);
}



/* 保存 Upgrade 终态并使用 Connection 内嵌命令延迟回调。 */
static bool __xrtHttpConnUpgradePost(
	xhttpconn* pConnection,
	xnetresult Result,
	ptr pTransport,
	size_t iBuffered,
	const xerror* pError
)
{
	if ( (pConnection == NULL) ||
		(pConnection->Upgrade == NULL) ||
		pConnection->UpgradePosted ) {
		return false;
	}
	pConnection->UpgradeResult = Result;
	pConnection->UpgradeTransport = pTransport;
	pConnection->UpgradeBuffered = iBuffered;
	pConnection->UpgradeError = xrtErrorRef(pError);
	pConnection->UpgradePosted = true;
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->UpgradeCallbacks,
		1,
		XMEMORY_ACQ_REL
	);
	if ( !xrtNetPost(
		pConnection->Worker,
		&pConnection->UpgradePost,
		__xrtHttpConnUpgradeTask,
		pConnection
	) ) {
		/* Connection 回调始终位于所属 Worker；失败时同步完成唯一终态。 */
		xrtClearError();
		__xrtHttpConnUpgradeTask(
			pConnection->Worker,
			pConnection
		);
	}
	return true;
}



/* 为连接关闭前已经受理的 Upgrade 安排唯一失败回调。 */
bool __xrtHttpConnUpgradeFail(
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError
)
{
	return __xrtHttpConnUpgradePost(
		pConnection,
		Result,
		NULL,
		0,
		pError
	);
}



/* 摘除 HTTP 事件并取得 Connection 唯一传输引用。 */
static ptr __xrtHttpConnUpgradeTransport(
	xhttpconn* pConnection,
	size_t* pBuffered
)
{
	ptr pTransport = xrtAtomicPtrLoad(
		&pConnection->Transport,
		XMEMORY_ACQUIRE
	);

	*pBuffered = 0;
	if ( pTransport == NULL ) {
		return NULL;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_TLS)
		if ( pConnection->TransportKind ==
			XRT_HTTP_CONN_TRANSPORT_TLS ) {
			xtlsstream* pTls = (xtlsstream*)pTransport;

			*pBuffered = xrtTlsStreamAvailable(pTls);
			if ( !xrtTlsStreamSetEvents(
				pTls, NULL, NULL
			) ) {
				return NULL;
			}
		} else
	#endif
	{
		xnetstream* pTcp = (xnetstream*)pTransport;

		*pBuffered = xrtNetStreamAvailable(pTcp);
		if ( !xrtNetStreamSetEvents(
			pTcp, NULL, NULL
		) ) {
			return NULL;
		}
	}
	xrtSpinLock(&pConnection->TransportLock);
	pTransport = xrtAtomicPtrExchange(
		&pConnection->Transport,
		NULL,
		XMEMORY_ACQ_REL
	);
	xrtSpinUnlock(&pConnection->TransportLock);
	return pTransport;
}



/* 释放已经离开 HTTP 的请求、响应与排队信息响应。 */
static void __xrtHttpConnUpgradeReleaseProtocol(
	xhttpconn* pConnection
)
{
	__xrt_http_response_queue* pQueued;

	__xrtHttpConnCancelTimer(pConnection);
	pConnection->WriteDeadline = 0;
	xrtHttp1ServerResponseDestroy(pConnection->Response);
	pConnection->Response = NULL;
	while ( pConnection->ResponseHead != NULL ) {
		pQueued = pConnection->ResponseHead;
		pConnection->ResponseHead = pQueued->Next;
		xrtHttp1ServerResponseDestroy(pQueued->Response);
		xrtFree(pQueued);
	}
	pConnection->ResponseTail = NULL;
	xrtHttp1ServerExchangeDestroy(pConnection->Exchange);
	pConnection->Exchange = NULL;
	xrtAtomic32Store(
		&pConnection->RequestActive,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pConnection->State,
		XHTTP_CONN_UPGRADED,
		XMEMORY_RELEASE
	);
	__xrtHttpServerRemoveConnection(
		pConnection->Server,
		pConnection
	);
}



/* 完成 Tunnel 输出，并在下一次 Worker 调度中交付传输。 */
void __xrtHttpConnUpgradeFinish(xhttpconn* pConnection)
{
	uint32 iExpected = XRT_HTTP_CONN_GATE_OPEN;
	size_t iBuffered = 0;
	ptr pTransport;

	if ( pConnection == NULL ) {
		return;
	}
	if ( pConnection->Upgrade == NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_UPGRADE,
			"finish-http-upgrade",
			"HTTP tunnel has no registered Upgrade owner",
			NULL
		);
		__xrtHttpConnEmitError(pConnection);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	/*
	 * 关闭和 Upgrade 只能有一方取得传输所有权；成功后任意线程的
	 * Close 都不能越过交付窗口误报成功。
	 */
	if ( !xrtAtomic32CompareExchange(
		&pConnection->CloseGate,
		&iExpected,
		XRT_HTTP_CONN_GATE_UPGRADED,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	) ) {
		return;
	}
	pTransport = __xrtHttpConnUpgradeTransport(
		pConnection,
		&iBuffered
	);
	if ( pTransport == NULL ) {
		__xrtHttpConnRememberError(
			pConnection,
			XERR_STATE,
			XHTTP_SERVER_ERROR_UPGRADE,
			"finish-http-upgrade",
			"HTTP transport could not leave its event owner",
			xrtGetError()
		);
		__xrtHttpConnEmitError(pConnection);
		xrtAtomic32Store(
			&pConnection->CloseGate,
			XRT_HTTP_CONN_GATE_CLOSING,
			XMEMORY_RELEASE
		);
		(void)xrtHttpConnAbort(pConnection);
		return;
	}
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->Responses,
		1,
		XMEMORY_RELAXED
	);
	(void)xrtAtomic64FetchAdd(
		&pConnection->Server->Upgraded,
		1,
		XMEMORY_RELAXED
	);
	(void)__xrtHttpConnUpgradePost(
		pConnection,
		XNET_RESULT_OK,
		pTransport,
		iBuffered,
		NULL
	);
	__xrtHttpConnUpgradeReleaseProtocol(pConnection);
}



/* 检查并登记唯一 Upgrade 完成过程。 */
static bool __xrtHttpConnUpgradeRegister(
	xhttpconn* pConnection,
	xhttpupgradeproc pProc,
	ptr pData,
	cstr sOperation
)
{
	if ( (pConnection == NULL) || (pProc == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			sOperation,
			"HTTP Upgrade connection or completion callback is null",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpConnCanRespond(
		pConnection,
		sOperation,
		true
	) ) {
		return false;
	}
	if ( !xrtHttp1ServerExchangeComplete(
		pConnection->Exchange
	) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP Upgrade requires a complete request",
			NULL
		);
		return false;
	}
	if ( (pConnection->Upgrade != NULL) ||
		xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			sOperation,
			"HTTP connection already committed its final owner",
			NULL
		);
		return false;
	}
	pConnection->Upgrade = pProc;
	pConnection->UpgradeData = pData;
	return true;
}



/* 清除尚未由响应门受理的 Upgrade 回调。 */
static void __xrtHttpConnUpgradeUnregister(
	xhttpconn* pConnection
)
{
	if ( (pConnection == NULL) ||
		pConnection->UpgradePosted ) {
		return;
	}
	pConnection->Upgrade = NULL;
	pConnection->UpgradeData = NULL;
}



/* 提交拥有型 Tunnel 响应计划并登记传输接管过程。 */
XHTTP_SERVER_API xnetresult xrtHttpConnUpgradeResponse(
	xhttpconn* pConnection,
	xhttp1serverresponse* pResponse,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	xnetresult Result;

	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-http-connection",
			"HTTP Upgrade response plan is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !xrtHttp1ServerResponseTunnel(pResponse) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-http-connection",
			"HTTP Upgrade requires a Tunnel response plan",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	if ( !__xrtHttpConnUpgradeRegister(
		pConnection,
		pProc,
		pData,
		"upgrade-http-connection"
	) ) {
		xrtHttp1ServerResponseDestroy(pResponse);
		return XNET_RESULT_ERROR;
	}
	Result = __xrtHttpConnCommitResponse(
		pConnection,
		pResponse,
		"upgrade-http-connection",
		true
	);
	if ( Result != XNET_RESULT_OK ) {
		__xrtHttpConnUpgradeUnregister(pConnection);
	}
	return Result;
}



/* 冻结 Reply 并提交常用 HTTP Upgrade。 */
XHTTP_SERVER_API xnetresult xrtHttpConnUpgrade(
	xhttpconn* pConnection,
	const xhttpreply* pReply,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	const xhttpserverrequest* pRequest;
	xhttp1serverresponse* pResponse;

	if ( (pConnection == NULL) || (pReply == NULL) ||
		(pProc == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-http-connection",
			"HTTP Upgrade connection, Reply or callback is null",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( pRequest == NULL ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"upgrade-http-connection",
			"HTTP Upgrade requires an active request",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = xrtHttp1ServerResponseCreate(
		pRequest,
		pReply
	);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-http-connection",
			"HTTP Upgrade Reply could not form a Tunnel response",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnUpgradeResponse(
		pConnection,
		pResponse,
		pProc,
		pData
	);
}



/* 复制完整切换响应，并用公共线缆计划工厂提交 Upgrade。 */
XHTTP_SERVER_API xnetresult xrtHttpConnUpgradeRaw(
	xhttpconn* pConnection,
	xbytesview Response,
	xhttpupgradeproc pProc,
	ptr pData
)
{
	const xhttpserverrequest* pRequest;
	xhttp1serverresponse* pResponse;
	xhttpbody* pBody;

	if ( (pConnection == NULL) || (pProc == NULL) ||
		(Response.Size == 0) ||
		!xrtMemRangeValid(Response.Data, Response.Size) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade requires a connection, complete byte range and callback",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( (pRequest == NULL) ||
		((xrtHttpServerRequestFlags(pRequest) &
		  XHTTP_SERVER_REQUEST_UPGRADE) == 0) ) {
		__xrtHttpServerSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade requires an accepted Upgrade request",
			NULL
		);
		return XNET_RESULT_ERROR;
	}
	pBody = xrtHttpBodyCopy(Response);
	if ( pBody == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade bytes could not be copied",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	pResponse = __xrtHttpConnWireResponse(
		pBody,
		(uint64)Response.Size,
		false,
		true
	);
	xrtHttpBodyDestroy(pBody);
	if ( pResponse == NULL ) {
		__xrtHttpServerSetError(
			XERR_MEMORY,
			XHTTP_SERVER_ERROR_UPGRADE,
			"upgrade-raw-http-connection",
			"Raw HTTP Upgrade response plan could not be created",
			xrtGetError()
		);
		return XNET_RESULT_ERROR;
	}
	return xrtHttpConnUpgradeResponse(
		pConnection,
		pResponse,
		pProc,
		pData
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_future.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)



#if defined(XHTTP_FEATURE_HTTP_SERVER_FUTURE)

/* Server 关闭等待节点独立持有 Server 和 Promise。 */
struct __xrt_http_server_wait {
	__xrt_http_server_wait* Next;
	xhttpserver* Server;
	xpromise* Promise;
	xcancelwatch* Watch;
	bool Linked;
};



/* 从 Server 等待链表中移除指定节点；调用方持有 Server Lock。 */
static bool __xrtHttpServerWaitRemove(
	xhttpserver* pServer,
	__xrt_http_server_wait* pWaiter
)
{
	__xrt_http_server_wait** ppCurrent =
		&pServer->WaitHead;
	__xrt_http_server_wait* pPrevious = NULL;

	while ( (*ppCurrent != NULL) &&
		(*ppCurrent != pWaiter) ) {
		pPrevious = *ppCurrent;
		ppCurrent = &(*ppCurrent)->Next;
	}
	if ( *ppCurrent == NULL ) {
		return false;
	}
	*ppCurrent = pWaiter->Next;
	if ( pServer->WaitTail == pWaiter ) {
		pServer->WaitTail = pPrevious;
	}
	pWaiter->Next = NULL;
	pWaiter->Linked = false;
	return true;
}



/* 完成一个已摘除的关闭等待并释放全部持有资源。 */
static void __xrtHttpServerWaitFinish(
	__xrt_http_server_wait* pWaiter,
	bool bCancelled
)
{
	xpromise* pPromise = pWaiter->Promise;
	xhttpserver* pServer = pWaiter->Server;

	xrtCancelUnwatch(pWaiter->Watch);
	pWaiter->Watch = NULL;
	xrtHttpServerDestroy(pServer);
	xrtFree(pWaiter);
	if ( bCancelled ) {
		(void)xrtPromiseCancel(pPromise);
	} else {
		(void)xrtPromiseResolve(pPromise, NULL);
	}
	xrtPromiseDestroy(pPromise);
}



/* 取消只摘除当前关闭等待，不改变 Server 生命周期。 */
static void __xrtHttpServerWaitCancel(ptr pData)
{
	__xrt_http_server_wait* pWaiter =
		(__xrt_http_server_wait*)pData;
	xhttpserver* pServer = pWaiter->Server;
	bool bRemoved;

	xrtSpinLock(&pServer->Lock);
	bRemoved = pWaiter->Linked &&
		__xrtHttpServerWaitRemove(pServer, pWaiter);
	xrtSpinUnlock(&pServer->Lock);
	if ( bRemoved ) {
		__xrtHttpServerWaitFinish(pWaiter, true);
	}
}



/* 摘除并成功完成 Server 的全部关闭等待。 */
void __xrtHttpServerFutureFinish(xhttpserver* pServer)
{
	__xrt_http_server_wait* pWaiter;

	if ( pServer == NULL ) {
		return;
	}
	xrtSpinLock(&pServer->Lock);
	pWaiter = pServer->WaitHead;
	pServer->WaitHead = NULL;
	pServer->WaitTail = NULL;
	for ( __xrt_http_server_wait* pCurrent = pWaiter;
		pCurrent != NULL;
		pCurrent = pCurrent->Next ) {
		pCurrent->Linked = false;
	}
	xrtSpinUnlock(&pServer->Lock);
	while ( pWaiter != NULL ) {
		__xrt_http_server_wait* pNext = pWaiter->Next;

		pWaiter->Next = NULL;
		__xrtHttpServerWaitFinish(pWaiter, false);
		pWaiter = pNext;
	}
}



/* 建立一个可独立取消的 Server 关闭 Future。 */
XHTTP_SERVER_API xfuture* xrtHttpServerWaitAsync(xhttpserver* pServer)
{
	__xrt_http_server_wait* pWaiter;
	xfuture* pFuture = NULL;
	xcancel* pCancel;
	xerror* pError;
	bool bClosed;

	if ( pServer == NULL ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"wait-http-server",
			"HTTP server is null",
			NULL
		);
		return NULL;
	}
	pWaiter = (__xrt_http_server_wait*)xrtCalloc(
		1,
		sizeof(*pWaiter)
	);
	if ( pWaiter == NULL ) {
		return NULL;
	}
	pWaiter->Server = xrtHttpServerRef(pServer);
	if ( pWaiter->Server == NULL ) {
		xrtFree(pWaiter);
		return NULL;
	}
	pWaiter->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pWaiter->Promise == NULL ) {
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtHttpServerDestroy(pWaiter->Server);
		xrtFree(pWaiter);
		return NULL;
	}
	pCancel = xrtPromiseCancelToken(pWaiter->Promise);
	if ( pCancel != NULL ) {
		pWaiter->Watch = xrtCancelWatch(
			pCancel,
			__xrtHttpServerWaitCancel,
			pWaiter
		);
		xrtCancelDestroy(pCancel);
	}
	if ( pWaiter->Watch == NULL ) {
		pError = xrtTakeError();
		xrtFutureDestroy(pFuture);
		xrtPromiseDestroy(pWaiter->Promise);
		xrtHttpServerDestroy(pWaiter->Server);
		xrtFree(pWaiter);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		}
		return NULL;
	}
	xrtSpinLock(&pServer->Lock);
	bClosed = xrtAtomic32Load(
		&pServer->ShutdownPublished,
		XMEMORY_ACQUIRE
	) != 0;
	if ( !bClosed ) {
		pWaiter->Linked = true;
		if ( pServer->WaitTail != NULL ) {
			pServer->WaitTail->Next = pWaiter;
		} else {
			pServer->WaitHead = pWaiter;
		}
		pServer->WaitTail = pWaiter;
	}
	xrtSpinUnlock(&pServer->Lock);
	if ( bClosed ) {
		__xrtHttpServerWaitFinish(pWaiter, false);
	}
	return pFuture;
}



/* Future 桥接上下文用独立锁切断 Connection 与任意完成线程的竞态。 */
struct __xrt_http_conn_future {
	volatile int32 References;
	xspinlock Lock;
	xhttpconn* Connection;
	xhttpconn* PostedConnection;
	xfuture* Source;
};



/* 增加 Future 桥接上下文引用。 */
static __xrt_http_conn_future* __xrtHttpConnFutureRef(
	__xrt_http_conn_future* pContext
)
{
	if ( (pContext == NULL) ||
		(xrtRefRetain(&pContext->References) < 0) ) {
		return NULL;
	}
	return pContext;
}



/* 释放 Future 桥接上下文及其独立持有的源 Future。 */
static void __xrtHttpConnFutureDestroy(
	__xrt_http_conn_future* pContext
)
{
	xhttpconn* pConnection;
	xhttpconn* pPosted;

	if ( (pContext == NULL) ||
		(xrtRefRelease(&pContext->References) != 0) ) {
		return;
	}
	xrtSpinLock(&pContext->Lock);
	pConnection = pContext->Connection;
	pPosted = pContext->PostedConnection;
	pContext->Connection = NULL;
	pContext->PostedConnection = NULL;
	xrtSpinUnlock(&pContext->Lock);
	xrtHttpConnDestroy(pPosted);
	xrtHttpConnDestroy(pConnection);
	xrtFutureDestroy(pContext->Source);
	xrtSpinUnit(&pContext->Lock);
	memset(pContext, 0, sizeof(*pContext));
	xrtFree(pContext);
}



/* 让 Future Owned 延续释放其桥接上下文引用。 */
static void __xrtHttpConnFutureFree(ptr pValue, ptr pData)
{
	(void)pData;
	__xrtHttpConnFutureDestroy(
		(__xrt_http_conn_future*)pValue
	);
}



/* 从 Future 终态选择稳定的应用错误响应。 */
static void __xrtHttpConnFutureFail(
	xhttpconn* pConnection,
	const xfutureresult* pResult,
	xerrkind Kind,
	cstr sMessage
)
{
	const xerror* pCause = pResult != NULL ?
		pResult->Error : NULL;
	uint16 iStatus = 500;

	if ( Kind == XERR_TIMEOUT ) {
		iStatus = 504;
	} else if ( (Kind == XERR_CANCELLED) ||
		(Kind == XERR_CLOSED) ) {
		iStatus = 503;
	}
	__xrtHttpConnRememberError(
		pConnection,
		Kind,
		XHTTP_SERVER_ERROR_CALLBACK,
		"complete-http-response-future",
		sMessage,
		pCause
	);
	__xrtHttpConnEmitError(pConnection);
	pConnection->ForceClose = true;
	if ( xrtHttpConnReply(
		pConnection,
		iStatus,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		iStatus == 504 ?
			XRT_BYTES_LITERAL("Gateway Timeout") :
			(iStatus == 503 ?
			 XRT_BYTES_LITERAL("Service Unavailable") :
			 XRT_BYTES_LITERAL("Internal Server Error"))
	) != XNET_RESULT_OK ) {
		(void)xrtHttpConnAbort(pConnection);
	}
}



/* 在 Connection Worker 上消费 Future 唯一终态。 */
static void __xrtHttpConnFutureTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xrt_http_conn_future* pContext =
		(__xrt_http_conn_future*)pData;
	xhttpconn* pConnection;
	xfutureresult Result;

	xrtSpinLock(&pContext->Lock);
	pConnection = pContext->PostedConnection;
	pContext->PostedConnection = NULL;
	xrtSpinUnlock(&pContext->Lock);
	if ( pConnection == NULL ) {
		__xrtHttpConnFutureDestroy(pContext);
		return;
	}
	(void)pWorker;
	if ( pConnection->Future != pContext ) {
		goto Finish;
	}
	__xrtHttpConnFutureDetach(pConnection, false);
	if ( (xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSING) ||
		(xrtHttpConnState(pConnection) ==
		 XHTTP_CONN_CLOSED) ) {
		goto Finish;
	}
	if ( !xrtFutureResult(pContext->Source, &Result) ) {
		__xrtHttpConnFutureFail(
			pConnection,
			NULL,
			XERR_INTERNAL,
			"HTTP response Future did not publish a terminal result"
		);
		goto Finish;
	}
	if ( Result.State == XFUTURE_RESOLVED ) {
		xerror* pCause;

		if ( Result.Value == NULL ) {
			__xrtHttpConnFutureFail(
				pConnection,
				&Result,
				XERR_VALUE,
				"HTTP response Future resolved without a Reply"
			);
			goto Finish;
		}
		if ( xrtHttpConnRespond(
			pConnection,
			(const xhttpreply*)Result.Value
		) == XNET_RESULT_OK ) {
			goto Finish;
		}
		pCause = xrtTakeError();
		__xrtHttpConnRememberError(
			pConnection,
			__xrtHttpServerCauseKind(
				pCause,
				XERR_PROTOCOL
			),
			XHTTP_SERVER_ERROR_RESPONSE,
			"complete-http-response-future",
			"HTTP response Future produced an invalid Reply",
			pCause
		);
		xrtErrorFree(pCause);
		__xrtHttpConnEmitError(pConnection);
		pConnection->ForceClose = true;
		if ( xrtHttpConnReply(
			pConnection,
			500,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Internal Server Error")
		) != XNET_RESULT_OK ) {
			(void)xrtHttpConnAbort(pConnection);
		}
		goto Finish;
	}
	if ( Result.State == XFUTURE_FAILED ) {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			Result.Error != NULL ?
				xrtErrorKind(Result.Error) :
				XERR_INTERNAL,
			"HTTP response Future failed"
		);
	} else if ( Result.State == XFUTURE_CANCELLED ) {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			XERR_CANCELLED,
			"HTTP response Future was cancelled"
		);
	} else {
		__xrtHttpConnFutureFail(
			pConnection,
			&Result,
			XERR_CLOSED,
			"HTTP response Future closed without a result"
		);
	}

Finish:
	xrtHttpConnDestroy(pConnection);
	__xrtHttpConnFutureDestroy(pContext);
}



/* Future 可从任意线程完成；这里只保留引用并投递回所属 Worker。 */
static void __xrtHttpConnFutureComplete(
	const xfutureresult* pResult,
	xpromise* pOutput,
	ptr pData
)
{
	__xrt_http_conn_future* pContext =
		(__xrt_http_conn_future*)pData;
	xhttpconn* pConnection = NULL;

	(void)pResult;
	xrtSpinLock(&pContext->Lock);
	if ( (pContext->Connection != NULL) &&
		(pContext->PostedConnection == NULL) ) {
		pConnection = xrtHttpConnRef(
			pContext->Connection
		);
		if ( (pConnection != NULL) &&
			(__xrtHttpConnFutureRef(pContext) != NULL) ) {
			pContext->PostedConnection = pConnection;
		} else {
			xrtHttpConnDestroy(pConnection);
			pConnection = NULL;
		}
	}
	xrtSpinUnlock(&pContext->Lock);
	if ( (pConnection != NULL) &&
		!xrtNetEnginePost(
			pConnection->Server->Engine,
			xrtNetWorkerIndex(pConnection->Worker),
			__xrtHttpConnFutureTask,
			pContext
		) ) {
		xrtSpinLock(&pContext->Lock);
		if ( pContext->PostedConnection ==
			pConnection ) {
			pContext->PostedConnection = NULL;
		}
		xrtSpinUnlock(&pContext->Lock);
		(void)xrtHttpConnAbort(pConnection);
		xrtHttpConnDestroy(pConnection);
		__xrtHttpConnFutureDestroy(pContext);
	}
	(void)xrtPromiseResolve(pOutput, NULL);
}



/* 解除连接持有的 Future 桥，并可请求生产过程协作取消。 */
void __xrtHttpConnFutureDetach(
	xhttpconn* pConnection,
	bool bCancel
)
{
	__xrt_http_conn_future* pContext;
	xhttpconn* pHeld = NULL;

	if ( pConnection == NULL ) {
		return;
	}
	pContext = pConnection->Future;
	if ( pContext == NULL ) {
		return;
	}
	pConnection->Future = NULL;
	xrtSpinLock(&pContext->Lock);
	if ( pContext->Connection == pConnection ) {
		pHeld = pContext->Connection;
		pContext->Connection = NULL;
	}
	xrtSpinUnlock(&pContext->Lock);
	if ( bCancel ) {
		(void)xrtFutureCancel(pContext->Source);
	}
	xrtHttpConnDestroy(pHeld);
	__xrtHttpConnFutureDestroy(pContext);
}



/* 把当前完整请求的唯一最终响应绑定到 Future。 */
XHTTP_SERVER_API bool xrtHttpConnRespondFuture(
	xhttpconn* pConnection,
	xfuture* pFuture
)
{
	__xrt_http_conn_future* pContext;
	xfuture* pContinue;

	if ( (pConnection == NULL) || (pFuture == NULL) ) {
		__xrtHttpServerSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ERROR_ARGUMENT,
			"bind-http-response-future",
			"HTTP connection and response Future are required",
			NULL
		);
		return false;
	}
	if ( !xrtNetWorkerIsCurrent(pConnection->Worker) ||
		(xrtHttp1ServerExchangeRequest(
			pConnection->Exchange
		 ) == NULL) ||
		!xrtHttp1ServerExchangeComplete(
			pConnection->Exchange
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"bind-http-response-future",
			"HTTP response Future requires its complete request Worker",
			NULL
		);
		return false;
	}
	if ( (pConnection->Future != NULL) ||
		xrtAtomic32Load(
			&pConnection->FinalGate,
			XMEMORY_ACQUIRE
		) ) {
		__xrtHttpServerSetError(
			XERR_STATE,
			XHTTP_SERVER_ERROR_STATE,
			"bind-http-response-future",
			"HTTP request already has a Future or final response",
			NULL
		);
		return false;
	}
	pContext = (__xrt_http_conn_future*)xrtCalloc(
		1,
		sizeof(*pContext)
	);
	if ( pContext == NULL ) {
		return false;
	}
	pContext->References = 2;
	xrtSpinInit(&pContext->Lock);
	pContext->Connection = xrtHttpConnRef(pConnection);
	pContext->Source = xrtFutureRef(pFuture);
	if ( (pContext->Connection == NULL) ||
		(pContext->Source == NULL) ) {
		xrtHttpConnDestroy(pContext->Connection);
		xrtFutureDestroy(pContext->Source);
		xrtSpinUnit(&pContext->Lock);
		xrtFree(pContext);
		return false;
	}
	pConnection->Future = pContext;
	pContinue = xrtFutureContinueOwned(
		pFuture,
		__xrtHttpConnFutureComplete,
		pContext,
		__xrtHttpConnFutureFree,
		NULL
	);
	if ( pContinue == NULL ) {
		pConnection->Future = NULL;
		xrtSpinLock(&pContext->Lock);
		pContext->Connection = NULL;
		xrtSpinUnlock(&pContext->Lock);
		xrtHttpConnDestroy(pConnection);
		__xrtHttpConnFutureDestroy(pContext);
		__xrtHttpConnFutureDestroy(pContext);
		return false;
	}
	xrtFutureDestroy(pContinue);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_file.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE)



#if defined(XHTTP_FEATURE_HTTP_SERVER_FILE)

/* 延续上下文只在 Reply 尚未转移给输出 Future 时拥有它。 */
typedef struct xrt_http_server_file_context {
	xhttpreply* Reply;
} xrt_http_server_file_context;



/* Future 最后释放文件 Reply 时销毁完整响应对象。 */
static void __xrtHttpServerFileReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 释放延续上下文仍然拥有的半成品 Reply。 */
static void __xrtHttpServerFileContextFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_file_context* pContext =
		(xrt_http_server_file_context*)pValue;

	(void)pData;
	if ( pContext != NULL ) {
		xrtHttpReplyDestroy(pContext->Reply);
		xrtFree(pContext);
	}
}



/* 用当前结构化错误完成输出 Promise；没有错误时补内部错误。 */
static void __xrtHttpServerFileReject(xpromise* pOutput)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		xrtSetErrorKind(XERR_INTERNAL);
		pError = xrtTakeError();
	}
	if ( pError != NULL ) {
		(void)xrtPromiseReject(pOutput, pError);
	} else {
		(void)xrtPromiseClose(pOutput);
	}
	xrtErrorFree(pError);
}



/* 把成功准备的文件 Body 安装到 Reply，并转移给输出 Future。 */
static void __xrtHttpServerFileBodyReady(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	xrt_http_server_file_context* pContext =
		(xrt_http_server_file_context*)pData;
	xhttpbody* pBody = pInput != NULL ?
		(xhttpbody*)pInput->Value : NULL;

	if ( (pContext == NULL) ||
		(pContext->Reply == NULL) ||
		(pBody == NULL) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		__xrtHttpServerFileReject(pOutput);
		return;
	}
	if ( !xrtHttpReplySetBody(
		pContext->Reply,
		pBody
	) ) {
		__xrtHttpServerFileReject(pOutput);
		return;
	}
	if ( xrtPromiseResolveOwned(
		pOutput,
		pContext->Reply,
		__xrtHttpServerFileReplyFree,
		NULL
	) ) {
		pContext->Reply = NULL;
	}
}



/* 创建 Reply、提交文件准备，并用通用 Future 延续组合两层所有权。 */
static xfuture* __xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	bool bRange
)
{
	xrt_http_server_file_context* pContext;
	xfuture* pBodyFuture;
	xfuture* pReplyFuture;

	if ( (pPool == NULL) ||
		(sPath == NULL) ||
		(sPath[0] == '\0') ||
		((ContentType.Size != 0) &&
		 (ContentType.Data == NULL)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pContext = (xrt_http_server_file_context*)xrtCalloc(
		1,
		sizeof(xrt_http_server_file_context)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Reply = xrtHttpReplyCreate(iStatus);
	if ( (pContext->Reply == NULL) ||
		((ContentType.Size != 0) &&
		 !xrtHttpReplySetHeader(
			pContext->Reply,
			XRT_STR_LITERAL("Content-Type"),
			ContentType
		 )) ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pBodyFuture = bRange ?
		xrtHttpBodyFileRangeFuture(
			pPool,
			sPath,
			iOffset,
			iLength,
			NULL
		) :
		xrtHttpBodyFileFuture(pPool, sPath, NULL);
	if ( pBodyFuture == NULL ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pReplyFuture = xrtFutureThenOwnedCancelSource(
		pBodyFuture,
		__xrtHttpServerFileBodyReady,
		pContext,
		__xrtHttpServerFileContextFree,
		NULL
	);
	xrtFutureDestroy(pBodyFuture);
	if ( pReplyFuture == NULL ) {
		__xrtHttpServerFileContextFree(
			pContext,
			NULL
		);
	}
	return pReplyFuture;
}



/* 绑定文件 Reply Future，并释放 Helper 持有的本地 Future 引用。 */
static bool __xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength,
	bool bRange
)
{
	xfuture* pFuture;
	bool bResult;

	if ( pConnection == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pFuture = __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		bRange
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bResult = xrtHttpConnRespondFuture(
		pConnection,
		pFuture
	);
	xrtFutureDestroy(pFuture);
	return bResult;
}



/* 异步构建完整文件 Reply。 */
XHTTP_SERVER_API xfuture* xrtHttpReplyFileFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
)
{
	return __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		0,
		0,
		false
	);
}



/* 异步构建严格文件区间 Reply。 */
XHTTP_SERVER_API xfuture* xrtHttpReplyFileRangeFuture(
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
)
{
	return __xrtHttpReplyFileFuture(
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		true
	);
}



/* 一次调用准备完整文件并绑定最终响应。 */
XHTTP_SERVER_API bool xrtHttpConnFile(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath
)
{
	return __xrtHttpConnFile(
		pConnection,
		pPool,
		iStatus,
		ContentType,
		sPath,
		0,
		0,
		false
	);
}



/* 一次调用准备严格文件区间并绑定最终响应。 */
XHTTP_SERVER_API bool xrtHttpConnFileRange(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	uint16 iStatus,
	xstrview ContentType,
	cstr sPath,
	uint64 iOffset,
	uint64 iLength
)
{
	return __xrtHttpConnFile(
		pConnection,
		pPool,
		iStatus,
		ContentType,
		sPath,
		iOffset,
		iLength,
		true
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_static.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)



#if defined(XHTTP_FEATURE_HTTP_SERVER_STATIC)

#define XRT_HTTP_STATIC_BOUNDARY_RANDOM 16u
#define XRT_HTTP_STATIC_BOUNDARY_PREFIX "xrt-"
#define XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE 4u
#define XRT_HTTP_STATIC_BOUNDARY_SIZE \
	(XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE + \
	 (XRT_HTTP_STATIC_BOUNDARY_RANDOM * 2u))

static const xstrview __xrtHttpServerStaticIndexes[] = {
	{ "index.html", sizeof("index.html") - 1u }
};



/* 异步组合上下文独立拥有请求、最终相对路径和全部可变配置文本。 */
typedef struct xrt_http_server_static_context {
	xhttpserverrequest* Request;
	str ContentType;
	str CacheControl;
	str Boundary;
	xhttpstaticreplyconfig Config;
} xrt_http_server_static_context;



/* 静态资源任务独立拥有全部候选路径，任务池和安全根只借用到任务结束。 */
typedef struct xrt_http_server_static_prepare {
	xtaskpool* Pool;
	xroot Root;
	str* Paths;
	size_t PathCount;
} xrt_http_server_static_prepare;



/* 静态资源结果把命中的路径与文件绑定，供 Reply 组装阶段共同消费。 */
typedef struct xrt_http_server_static_resource {
	xhttpstaticfile* File;
	str Path;
	size_t PathSize;
} xrt_http_server_static_resource;



/* 为静态服务阶段建立结构化错误，并接管当前错误作为原因。 */
static void __xrtHttpServerStaticError(
	xerrkind Kind,
	xhttpserverstaticerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pCause = xrtTakeError();
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = pCause != NULL ?
		xrtErrorKind(pCause) : Kind;
	Desc.Domain = "http.server.static";
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	} else if ( pCause != NULL ) {
		xrtSetError(pCause);
	}
	xrtErrorFree(pCause);
}



/* Future 成功值最终销毁完整 Reply。 */
static void __xrtHttpServerStaticReplyFree(
	ptr pValue,
	ptr pData
)
{
	(void)pData;
	xrtHttpReplyDestroy((xhttpreply*)pValue);
}



/* 释放异步组合上下文及其独立请求引用和文本。 */
static void __xrtHttpServerStaticContextFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_context* pContext =
		(xrt_http_server_static_context*)pValue;

	(void)pData;
	if ( pContext == NULL ) {
		return;
	}
	xrtHttpServerRequestDestroy(pContext->Request);
	xrtFree(pContext->ContentType);
	xrtFree(pContext->CacheControl);
	xrtFree(pContext->Boundary);
	xrtFree(pContext);
}



/* 用当前错误拒绝 Promise；极端错误分配失败时安全关闭输出。 */
static void __xrtHttpServerStaticReject(xpromise* pOutput)
{
	xerror* pError = xrtTakeError();

	if ( pError == NULL ) {
		xrtSetErrorKind(XERR_INTERNAL);
		pError = xrtTakeError();
	}
	if ( pError != NULL ) {
		(void)xrtPromiseReject(pOutput, pError);
	} else {
		(void)xrtPromiseClose(pOutput);
	}
	xrtErrorFree(pError);
}



/* 把一个完整 Reply 转移给输出 Promise。 */
static bool __xrtHttpServerStaticResolve(
	xpromise* pOutput,
	xhttpreply* pReply
)
{
	if ( (pOutput == NULL) || (pReply == NULL) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return false;
	}
	return xrtPromiseResolveOwned(
		pOutput,
		pReply,
		__xrtHttpServerStaticReplyFree,
		NULL
	);
}



/* 创建已经以空正文状态码成功完成的 Reply Future。 */
static xfuture* __xrtHttpServerStaticStatusFuture(
	uint16 iStatus
)
{
	xhttpreply* pReply;
	xfuture* pFuture = NULL;
	xpromise* pPromise;

	pReply = xrtHttpReplyCreate(iStatus);
	if ( pReply == NULL ) {
		return NULL;
	}
	pPromise = xrtPromiseCreate(&pFuture, NULL);
	if ( pPromise == NULL ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	if ( !__xrtHttpServerStaticResolve(
		pPromise,
		pReply
	) ) {
		xrtHttpReplyDestroy(pReply);
		xrtPromiseDestroy(pPromise);
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	xrtPromiseDestroy(pPromise);
	return pFuture;
}



/* 初始化静态 Reply 的独立默认策略。 */
XHTTP_SERVER_API void xrtHttpStaticReplyConfigInit(
	xhttpstaticreplyconfig* pConfig
)
{
	xhttpstaticreplyconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpStaticPlanConfigInit(&Config.Plan);
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 初始化安全根挂载和常用目录索引。 */
XHTTP_SERVER_API void xrtHttpStaticServeConfigInit(
	xhttpstaticserveconfig* pConfig
)
{
	xhttpstaticserveconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	xrtHttpStaticPathConfigInit(&Config.Path);
	xrtHttpStaticReplyConfigInit(&Config.Reply);
	Config.Indexes = __xrtHttpServerStaticIndexes;
	Config.IndexCount = sizeof(__xrtHttpServerStaticIndexes) /
		sizeof(__xrtHttpServerStaticIndexes[0]);
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 验证静态 Reply 配置并填充缺省值。 */
static bool __xrtHttpStaticReplyConfigResolve(
	const xhttpstaticreplyconfig* pInput,
	xhttpstaticreplyconfig* pConfig
)
{
	xhttpstaticreplyconfig Config;

	xrtHttpStaticReplyConfigInit(&Config);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(Config)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static Reply configuration range"
			);
			return false;
		}
		memcpy(&Config, (const void*)pInput, sizeof(Config));
	}
	if ( !__xhttpViewValid(Config.ContentType) ||
		!__xhttpViewValid(Config.CacheControl) ||
		!__xhttpViewValid(Config.Boundary) ||
		(Config.Plan.MaxRanges >
		 (SIZE_MAX / sizeof(xhttpbyterange))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static Reply configuration"
		);
		return false;
	}
	memcpy(pConfig, &Config, sizeof(Config));
	return true;
}



/* 验证一站式服务配置，包括挂载点、索引名称和 Reply 策略。 */
static bool __xrtHttpStaticServeConfigResolve(
	const xhttpstaticserveconfig* pInput,
	xhttpstaticserveconfig* pConfig
)
{
	xstrview Index;
	size_t iMapped = 0;
	size_t i;
	bool bTrailing = false;

	xrtHttpStaticServeConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pConfig)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static service configuration range"
			);
			return false;
		}
		memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	}
	if ( !__xrtHttpStaticReplyConfigResolve(
		&pConfig->Reply,
		&pConfig->Reply
	) ) {
		return false;
	}
	if ( (pConfig->IndexCount >
		 (SIZE_MAX / sizeof(xstrview))) ||
		((pConfig->IndexCount != 0) &&
		 !xrtMemRangeValid(
			pConfig->Indexes,
			pConfig->IndexCount * sizeof(xstrview)
		 )) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static directory index array"
		);
		return false;
	}

	/* 用挂载点自身执行一次无分配映射，统一复用路径模块的完整配置校验。 */
	if ( xrtHttpStaticPathWrite(
		pConfig->Path.Mount,
		&pConfig->Path,
		NULL,
		0,
		&iMapped,
		&bTrailing
	) == XHTTP_STATIC_PATH_ERROR ) {
		__xrtHttpServerStaticError(
			XERR_ARGUMENT,
			XHTTP_SERVER_STATIC_ERROR_CONFIG,
			"config",
			"invalid static path configuration"
		);
		return false;
	}
	for ( i = 0; i < pConfig->IndexCount; i++ ) {
		memcpy(
			&Index,
			(const void*)(pConfig->Indexes + i),
			sizeof(Index)
		);
		if ( !__xhttpViewValid(Index) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			__xrtHttpServerStaticError(
				XERR_ARGUMENT,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"invalid static directory index view"
			);
			return false;
		}
		if ( Index.Size == 0 ) {
			xrtSetErrorKind(XERR_VALUE);
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"empty static directory index"
			);
			return false;
		}
		if ( (memchr(Index.Data, '/', Index.Size) != NULL) ||
			(memchr(Index.Data, '\\', Index.Size) != NULL) ) {
			xrtSetErrorKind(XERR_VALUE);
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"static directory index contains a path separator"
			);
			return false;
		}
		if ( !xrtPathIsSafeEntry(Index, false) ) {
			xrtSetErrorKind(XERR_VALUE);
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"static directory index is not a portable file name"
			);
			return false;
		}
		if ( ((pConfig->Path.Flags &
			XHTTP_STATIC_PATH_ALLOW_HIDDEN) == 0u) &&
			(Index.Data[0] == '.') ) {
			xrtSetErrorKind(XERR_VALUE);
			__xrtHttpServerStaticError(
				XERR_VALUE,
				XHTTP_SERVER_STATIC_ERROR_CONFIG,
				"config",
				"hidden static directory index is disabled"
			);
			return false;
		}
	}
	return true;
}



/* 复制可选文本，并让目标视图指向独立存储。 */
static bool __xrtHttpServerStaticViewCopy(
	xstrview Source,
	str* ppStorage,
	xstrview* pTarget
)
{
	if ( Source.Size == 0 ) {
		*ppStorage = NULL;
		*pTarget = (xstrview){ NULL, 0 };
		return true;
	}
	*ppStorage = xrtStrDupView(Source);
	if ( *ppStorage == NULL ) {
		return false;
	}
	*pTarget = (xstrview){ *ppStorage, Source.Size };
	return true;
}



/* 为 multipart/byteranges 生成 128 位随机 token boundary。 */
static bool __xrtHttpServerStaticBoundary(
	char sBoundary[XRT_HTTP_STATIC_BOUNDARY_SIZE + 1u],
	xstrview* pBoundary
)
{
	uint8 Random[XRT_HTTP_STATIC_BOUNDARY_RANDOM];
	size_t iSize = 0;

	if ( !xrtSecureRandom(Random, sizeof(Random)) ) {
		__xrtHttpServerStaticError(
			XERR_IO,
			XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
			"boundary",
			"failed to generate a multipart boundary"
		);
		return false;
	}
	memcpy(
		sBoundary,
		XRT_HTTP_STATIC_BOUNDARY_PREFIX,
		XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE
	);
	if ( !xrtHexEncode(
		Random,
		sizeof(Random),
		sBoundary + XRT_HTTP_STATIC_BOUNDARY_PREFIX_SIZE,
		sizeof(Random) * 2u + 1u,
		&iSize,
		0
	) || (iSize != (sizeof(Random) * 2u)) ) {
		memset(Random, 0, sizeof(Random));
		__xrtHttpServerStaticError(
			XERR_INTERNAL,
			XHTTP_SERVER_STATIC_ERROR_BOUNDARY,
			"boundary",
			"failed to encode a multipart boundary"
		);
		return false;
	}
	memset(Random, 0, sizeof(Random));
	pBoundary->Data = sBoundary;
	pBoundary->Size = XRT_HTTP_STATIC_BOUNDARY_SIZE;
	return true;
}



/* 只复制静态响应状态和字段，正文由上层在最后一步安装。 */
/* 快照并验证纯静态响应描述符及其全部借用字段。 */
static bool __xrtHttpServerStaticResponseRead(
	const xhttpstaticresponse* pInput,
	xhttpstaticresponse* pResponse
)
{
	size_t i;

	if ( !xrtMemRangeValid(pInput, sizeof(*pResponse)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy(pResponse, (const void*)pInput, sizeof(*pResponse));
	if ( (pResponse->FieldCount >
		 XHTTP_STATIC_RESPONSE_MAX_FIELDS) ||
		!__xhttpViewValid(pResponse->ContentType) ||
		!__xhttpViewValid(pResponse->Boundary) ||
		!__xhttpFieldArrayValid(
			pResponse->Fields,
			pResponse->FieldCount
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	for ( i = 0; i < pResponse->FieldCount; i++ ) {
		if ( !xrtHttpTokenValid(
				pResponse->Fields[i].Name
			) || !xrtHttpFieldValueValid(
				pResponse->Fields[i].Value
			) ) {
			xrtSetErrorKind(XERR_VALUE);
			return false;
		}
	}
	if ( ((pResponse->ContentType.Size != 0) &&
		 !xrtHttpFieldValueValid(pResponse->ContentType)) ||
		(pResponse->Multipart &&
		 ((pResponse->Boundary.Size == 0) ||
		  !xrtHttpTokenValid(pResponse->Boundary))) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	return true;
}



static xhttpreply* __xrtHttpReplyStaticFields(
	const xhttpstaticresponse* pResponse
)
{
	xhttpreply* pReply;
	size_t i;

	pReply = xrtHttpReplyCreate(pResponse->Status);
	if ( pReply == NULL ) {
		return NULL;
	}
	for ( i = 0; i < pResponse->FieldCount; i++ ) {
		if ( !xrtHttpReplyAddHeader(
			pReply,
			pResponse->Fields[i].Name,
			pResponse->Fields[i].Value
		) ) {
			xrtHttpReplyDestroy(pReply);
			return NULL;
		}
	}
	return pReply;
}



/* 把静态协议描述桥接为拥有型 Reply。 */
XHTTP_SERVER_API xhttpreply* xrtHttpReplyFromStatic(
	const xhttpstaticresponse* pResponse,
	xhttpbody* pBody
)
{
	xhttpstaticresponse Response;
	xhttpreply* pReply;
	uint64 iLength;

	if ( !__xrtHttpServerStaticResponseRead(
		pResponse,
		&Response
	) ) {
		return NULL;
	}
	if ( Response.SendBody != (pBody != NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( pBody != NULL ) {
		iLength = xrtHttpBodyLength(pBody);
		if ( (iLength == XHTTP_BODY_UNKNOWN) ||
			(iLength != Response.BodyLength) ) {
			xrtSetErrorKind(XERR_RANGE);
			return NULL;
		}
	}
	pReply = __xrtHttpReplyStaticFields(&Response);
	if ( (pReply == NULL) ||
		((pBody != NULL) &&
		 !xrtHttpReplySetBody(pReply, pBody)) ) {
		xrtHttpReplyDestroy(pReply);
		return NULL;
	}
	return pReply;
}



/* 根据最终资源路径选择显式或内置媒体类型。 */
static xstrview __xrtHttpServerStaticContentType(
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pConfig
)
{
	xstrview ContentType;

	if ( pConfig->ContentType.Size != 0 ) {
		return pConfig->ContentType;
	}
	ContentType = xrtMimeByPath(ResourcePath);
	return ContentType.Size != 0 ?
		ContentType :
		XRT_STR_LITERAL("application/octet-stream");
}



/* 按静态计划从同一个安全文件资源取走准确正文。 */
static xhttpbody* __xrtHttpServerStaticBody(
	xhttpstaticfile* pFile,
	const xhttpstaticplan* pPlan,
	const xhttpbyterange* pRanges,
	xstrview ContentType,
	xstrview Boundary
)
{
	uint64 iLength;

	if ( pPlan->RangeCount == 0 ) {
		return xrtHttpStaticFileTakeBodyAll(pFile);
	}
	if ( pRanges == NULL ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return NULL;
	}
	if ( pPlan->RangeCount == 1u ) {
		iLength =
			(pRanges[0].Last - pRanges[0].First) +
			UINT64_C(1);
		return xrtHttpStaticFileTakeBody(
			pFile,
			pRanges[0].First,
			iLength
		);
	}
	return xrtHttpStaticFileTakeMultipartBody(
		pFile,
		pRanges,
		pPlan->RangeCount,
		ContentType,
		Boundary
	);
}



/* 构建请求对应的完整静态文件 Reply。 */
XHTTP_SERVER_API xhttpreply* xrtHttpReplyStatic(
	const xhttpserverrequest* pRequest,
	xhttpstaticfile* pFile,
	xstrview ResourcePath,
	const xhttpstaticreplyconfig* pInput
)
{
	xhttpstaticreplyconfig Config;
	xhttpstaticresponseconfig ResponseConfig;
	const xhttprepresentation* pCurrent;
	xhttpstaticresponse Response;
	xhttpstaticplan Plan;
	xhttpbyterange* pRanges = NULL;
	xhttpreply* pReply = NULL;
	xhttpbody* pBody = NULL;
	char* sWorkspace = NULL;
	char sBoundary[XRT_HTTP_STATIC_BOUNDARY_SIZE + 1u];
	xstrview ContentType;
	xstrview Boundary;
	size_t iWorkspace = 0;
	size_t iRangeBytes;

	if ( (pRequest == NULL) || (pFile == NULL) ||
		!__xhttpViewValid(ResourcePath) ||
		!__xrtHttpStaticReplyConfigResolve(
			pInput,
			&Config
		) ) {
		if ( (pRequest == NULL) || (pFile == NULL) ||
			!__xhttpViewValid(ResourcePath) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
		}
		return NULL;
	}
	iRangeBytes =
		Config.Plan.MaxRanges * sizeof(xhttpbyterange);
	if ( iRangeBytes != 0 ) {
		pRanges = (xhttpbyterange*)xrtMalloc(iRangeBytes);
		if ( pRanges == NULL ) {
			return NULL;
		}
	}
	pCurrent = xrtHttpStaticFileRepresentation(pFile);
	if ( (pCurrent == NULL) ||
		!xrtHttpStaticPlanBuild(
			xrtHttpServerRequestMethod(pRequest),
			xrtHttpServerRequestHeaderData(pRequest),
			xrtHttpServerRequestHeaderCount(pRequest),
			pCurrent,
			xrtHttpStaticFileSize(pFile),
			pRanges,
			Config.Plan.MaxRanges,
			&Config.Plan,
			&Plan
		) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_PLAN,
			"plan",
			"failed to build the static response plan"
		);
		xrtFree(pRanges);
		return NULL;
	}
	ContentType = __xrtHttpServerStaticContentType(
		ResourcePath,
		&Config
	);
	Boundary = Config.Boundary;
	if ( (Plan.RangeCount > 1u) &&
		(Boundary.Size == 0) &&
		!__xrtHttpServerStaticBoundary(
			sBoundary,
			&Boundary
		) ) {
		xrtFree(pRanges);
		return NULL;
	}
	xrtHttpStaticResponseConfigInit(&ResponseConfig);
	ResponseConfig.ContentType = ContentType;
	ResponseConfig.CacheControl = Config.CacheControl;
	ResponseConfig.Boundary = Boundary;
	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		pRanges,
		pCurrent,
		&ResponseConfig,
		NULL,
		0,
		&iWorkspace,
		NULL
	) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_RESPONSE,
			"response",
			"failed to measure the static response"
		);
		xrtFree(pRanges);
		return NULL;
	}
	if ( iWorkspace != 0 ) {
		sWorkspace = (char*)xrtMalloc(iWorkspace);
		if ( sWorkspace == NULL ) {
			xrtFree(pRanges);
			return NULL;
		}
	}
	if ( !xrtHttpStaticResponseBuild(
		&Plan,
		pRanges,
		pCurrent,
		&ResponseConfig,
		sWorkspace,
		iWorkspace,
		&iWorkspace,
		&Response
	) ) {
		__xrtHttpServerStaticError(
			XERR_PROTOCOL,
			XHTTP_SERVER_STATIC_ERROR_RESPONSE,
			"response",
			"failed to build the static response"
		);
		xrtFree(sWorkspace);
		xrtFree(pRanges);
		return NULL;
	}
	pReply = __xrtHttpReplyStaticFields(&Response);
	if ( pReply == NULL ) {
		__xrtHttpServerStaticError(
			XERR_MEMORY,
			XHTTP_SERVER_STATIC_ERROR_REPLY,
			"reply",
			"failed to create the static Reply"
		);
		xrtFree(sWorkspace);
		xrtFree(pRanges);
		return NULL;
	}

	/* 最后才取走文件正文，前面的可失败分配不会消耗静态文件。 */
	if ( Response.SendBody ) {
		pBody = __xrtHttpServerStaticBody(
			pFile,
			&Plan,
			pRanges,
			ContentType,
			Boundary
		);
		if ( (pBody == NULL) ||
			!xrtHttpReplySetBody(pReply, pBody) ) {
			__xrtHttpServerStaticError(
				XERR_IO,
				XHTTP_SERVER_STATIC_ERROR_BODY,
				"body",
				"failed to create the static response body"
			);
			xrtHttpBodyDestroy(pBody);
			xrtHttpReplyDestroy(pReply);
			xrtFree(sWorkspace);
			xrtFree(pRanges);
			return NULL;
		}
		xrtHttpBodyDestroy(pBody);
	}
	xrtFree(sWorkspace);
	xrtFree(pRanges);
	return pReply;
}



/* 判断打开失败是否应按静态资源不可见处理。 */
static bool __xrtHttpServerStaticNotFound(
	const xerror* pError
)
{
	xerrkind Kind;

	if ( pError == NULL ) {
		return false;
	}
	Kind = xrtErrorKind(pError);
	return (Kind == XERR_NOT_FOUND) ||
		(Kind == XERR_PERMISSION) ||
		(Kind == XERR_TYPE);
}



/* 把源 Future 的非成功终态原样发布到输出 Promise。 */
static void __xrtHttpServerStaticForward(
	const xfutureresult* pInput,
	xpromise* pOutput
)
{
	if ( (pInput == NULL) || (pOutput == NULL) ) {
		if ( pOutput != NULL ) {
			(void)xrtPromiseClose(pOutput);
		}
		return;
	}
	switch ( pInput->State ) {
		case XFUTURE_FAILED:
			if ( pInput->Error != NULL ) {
				(void)xrtPromiseReject(
					pOutput,
					pInput->Error
				);
			} else {
				(void)xrtPromiseClose(pOutput);
			}
			break;

		case XFUTURE_CANCELLED:
			(void)xrtPromiseCancel(pOutput);
			break;

		case XFUTURE_CLOSED:
			(void)xrtPromiseClose(pOutput);
			break;

		default:
			(void)xrtPromiseClose(pOutput);
			break;
	}
}



/* 文件打开终态转换为静态 Reply，同时把不可见资源规范化为 404。 */
static void __xrtHttpServerStaticFileReady(
	const xfutureresult* pInput,
	xpromise* pOutput,
	ptr pData
)
{
	xrt_http_server_static_context* pContext =
		(xrt_http_server_static_context*)pData;
	xrt_http_server_static_resource* pResource;
	xhttpreply* pReply;

	if ( (pInput != NULL) &&
		(pInput->State == XFUTURE_FAILED) &&
		__xrtHttpServerStaticNotFound(pInput->Error) ) {
		pReply = xrtHttpReplyCreate(
			XHTTP_STATUS_NOT_FOUND
		);
		if ( pReply == NULL ) {
			__xrtHttpServerStaticReject(pOutput);
		} else if ( !__xrtHttpServerStaticResolve(
			pOutput,
			pReply
		) ) {
			xrtHttpReplyDestroy(pReply);
		}
		return;
	}
	if ( (pInput == NULL) ||
		(pInput->State != XFUTURE_RESOLVED) ) {
		__xrtHttpServerStaticForward(
			pInput,
			pOutput
		);
		return;
	}
	pResource = (xrt_http_server_static_resource*)
		pInput->Value;
	if ( (pContext == NULL) ||
		(pContext->Request == NULL) ||
		(pResource == NULL) ||
		(pResource->File == NULL) ||
		(pResource->Path == NULL) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		__xrtHttpServerStaticReject(pOutput);
		return;
	}
	pReply = xrtHttpReplyStatic(
		pContext->Request,
		pResource->File,
		(xstrview){
			pResource->Path,
			pResource->PathSize
		},
		&pContext->Config
	);
	if ( pReply == NULL ) {
		__xrtHttpServerStaticReject(pOutput);
		return;
	}
	if ( !__xrtHttpServerStaticResolve(
		pOutput,
		pReply
	) ) {
		xrtHttpReplyDestroy(pReply);
	}
}



/* 为异步回调复制必要的请求引用和 Reply 配置文本。 */
static xrt_http_server_static_context*
	__xrtHttpServerStaticContextCreate(
		const xhttpserverrequest* pRequest,
		const xhttpstaticreplyconfig* pConfig
	)
{
	xrt_http_server_static_context* pContext;

	pContext = (xrt_http_server_static_context*)xrtCalloc(
		1,
		sizeof(xrt_http_server_static_context)
	);
	if ( pContext == NULL ) {
		return NULL;
	}
	pContext->Request = xrtHttpServerRequestRef(
		(xhttpserverrequest*)pRequest
	);
	if ( pContext->Request == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pContext->Config = *pConfig;
	if ( !__xrtHttpServerStaticViewCopy(
		pConfig->ContentType,
		&pContext->ContentType,
		&pContext->Config.ContentType
	) || !__xrtHttpServerStaticViewCopy(
		pConfig->CacheControl,
		&pContext->CacheControl,
		&pContext->Config.CacheControl
	) || !__xrtHttpServerStaticViewCopy(
		pConfig->Boundary,
		&pContext->Boundary,
		&pContext->Config.Boundary
	) ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	return pContext;
}



/* 释放任务持有的全部候选路径。 */
static void __xrtHttpServerStaticPrepareFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_prepare* pPrepare =
		(xrt_http_server_static_prepare*)pValue;
	size_t i;

	(void)pData;
	if ( pPrepare == NULL ) {
		return;
	}
	for ( i = 0; i < pPrepare->PathCount; i++ ) {
		xrtFree(pPrepare->Paths[i]);
	}
	xrtFree(pPrepare->Paths);
	xrtFree(pPrepare);
}



/* 释放已命中的静态资源和尚未被 Reply 正文取走的文件。 */
static void __xrtHttpServerStaticResourceFree(
	ptr pValue,
	ptr pData
)
{
	xrt_http_server_static_resource* pResource =
		(xrt_http_server_static_resource*)pValue;

	(void)pData;
	if ( pResource == NULL ) {
		return;
	}
	xrtHttpStaticFileDestroy(pResource->File);
	xrtFree(pResource->Path);
	xrtFree(pResource);
}



/* 为普通文件或尾随斜杠目录构造有序候选路径。 */
static xrt_http_server_static_prepare*
	__xrtHttpServerStaticPrepareCreate(
		xtaskpool* pPool,
		xroot Root,
		const xhttpstaticpath* pPath,
		const xstrview* pIndexes,
		size_t iIndexCount
	)
{
	xrt_http_server_static_prepare* pPrepare;
	xstrview Index;
	str sIndex;
	size_t iCount = pPath->TrailingSlash ?
		iIndexCount : 1u;
	size_t i;

	if ( (iCount == 0) ||
		(iCount > (SIZE_MAX / sizeof(str))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pPrepare = (xrt_http_server_static_prepare*)xrtCalloc(
		1,
		sizeof(xrt_http_server_static_prepare)
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	pPrepare->Paths = (str*)xrtCalloc(iCount, sizeof(str));
	if ( pPrepare->Paths == NULL ) {
		xrtFree(pPrepare);
		return NULL;
	}
	pPrepare->Pool = pPool;
	pPrepare->Root = Root;
	pPrepare->PathCount = iCount;
	if ( !pPath->TrailingSlash ) {
		pPrepare->Paths[0] = xrtStrDup(pPath->Path);
		if ( pPrepare->Paths[0] == NULL ) {
			__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
			return NULL;
		}
		return pPrepare;
	}
	for ( i = 0; i < iCount; i++ ) {
		memcpy(&Index, (const void*)(pIndexes + i), sizeof(Index));
		sIndex = xrtStrDupView(Index);
		if ( sIndex != NULL ) {
			pPrepare->Paths[i] = xrtPathJoin(
				pPath->Path,
				sIndex
			);
			xrtFree(sIndex);
		}
		if ( pPrepare->Paths[i] == NULL ) {
			__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
			return NULL;
		}
	}
	return pPrepare;
}



/* 在同一任务池工作线程内按顺序查找首个可见目录索引。 */
static xtaskoutcome __xrtHttpServerStaticPrepareTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	xrt_http_server_static_prepare* pPrepare =
		(xrt_http_server_static_prepare*)pData;
	xrt_http_server_static_resource* pResource;
	xhttpstaticfile* pFile;
	const xerror* pError;
	size_t i;

	if ( (pPrepare == NULL) || (pResult == NULL) ) {
		xrtSetErrorKind(XERR_INTERNAL);
		return XTASK_FAILED;
	}
	for ( i = 0; i < pPrepare->PathCount; i++ ) {
		if ( xrtCancelRequested(pCancel) ) {
			return XTASK_CANCELLED;
		}
		pFile = xrtHttpStaticFileOpen(
			pPrepare->Pool,
			pPrepare->Root,
			pPrepare->Paths[i]
		);
		if ( pFile != NULL ) {
			if ( xrtCancelRequested(pCancel) ) {
				xrtHttpStaticFileDestroy(pFile);
				return XTASK_CANCELLED;
			}
			pResource =
				(xrt_http_server_static_resource*)xrtCalloc(
					1,
					sizeof(xrt_http_server_static_resource)
				);
			if ( pResource == NULL ) {
				xrtHttpStaticFileDestroy(pFile);
				return XTASK_FAILED;
			}
			pResource->File = pFile;
			pResource->Path = pPrepare->Paths[i];
			pResource->PathSize = strlen(pResource->Path);
			pPrepare->Paths[i] = NULL;
			pResult->Value = pResource;
			pResult->Destroy =
				__xrtHttpServerStaticResourceFree;
			return XTASK_SUCCESS;
		}
		pError = xrtGetError();
		if ( !__xrtHttpServerStaticNotFound(pError) ||
			((i + 1u) == pPrepare->PathCount) ) {
			return xrtCancelRequested(pCancel) ?
				XTASK_CANCELLED : XTASK_FAILED;
		}
		xrtClearError();
	}
	xrtSetErrorKind(XERR_INTERNAL);
	return XTASK_FAILED;
}



/* 提交一次普通文件打开或有序目录索引查找。 */
static xfuture* __xrtHttpServerStaticPrepareFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticpath* pPath,
	const xstrview* pIndexes,
	size_t iIndexCount
)
{
	xrt_http_server_static_prepare* pPrepare;
	xtaskargs Args;
	xfuture* pFuture;

	pPrepare = __xrtHttpServerStaticPrepareCreate(
		pPool,
		Root,
		pPath,
		pIndexes,
		iIndexCount
	);
	if ( pPrepare == NULL ) {
		return NULL;
	}
	memset(&Args, 0, sizeof(Args));
	Args.Destroy = __xrtHttpServerStaticPrepareFree;
	pFuture = xrtTaskSubmit(
		pPool,
		__xrtHttpServerStaticPrepareTask,
		pPrepare,
		&Args
	);
	if ( pFuture == NULL ) {
		__xrtHttpServerStaticPrepareFree(pPrepare, NULL);
	}
	return pFuture;
}



/* 映射请求并启动根内文件准备 Future。 */
XHTTP_SERVER_API xfuture* xrtHttpReplyStaticFuture(
	xtaskpool* pPool,
	xroot Root,
	const xhttpserverrequest* pRequest,
	const xhttpstaticserveconfig* pInput
)
{
	xhttpstaticserveconfig Config;
	xrt_http_server_static_context* pContext;
	xhttpstaticpath Path;
	xhttptarget Target;
	xfuture* pFileFuture;
	xfuture* pReplyFuture;

	if ( (pPool == NULL) || (Root == NULL) ||
		(pRequest == NULL) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	if ( !__xrtHttpStaticServeConfigResolve(
		pInput,
		&Config
	) ) {
		return NULL;
	}
	if ( !xrtHttpServerRequestParseTarget(
		pRequest,
		&Target
	) ) {
		xrtClearError();
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_BAD_REQUEST
		);
	}
	if ( (Target.Form != XHTTP_TARGET_ORIGIN) &&
		(Target.Form != XHTTP_TARGET_ABSOLUTE) ) {
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	xrtHttpStaticPathInit(&Path);
	if ( !xrtHttpStaticPathMap(
		Target.Path,
		&Config.Path,
		&Path
	) ) {
		const xerror* pError = xrtGetError();

		if ( (pError != NULL) &&
			(xrtErrorKind(pError) == XERR_VALUE) ) {
			xrtClearError();
			return __xrtHttpServerStaticStatusFuture(
				XHTTP_STATUS_NOT_FOUND
			);
		}
		__xrtHttpServerStaticError(
			XERR_IO,
			XHTTP_SERVER_STATIC_ERROR_PATH,
			"path",
			"failed to map the static request path"
		);
		return NULL;
	}
	if ( !Path.Matched ) {
		xrtHttpStaticPathFree(&Path);
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	if ( Path.TrailingSlash &&
		(Config.IndexCount == 0) ) {
		xrtHttpStaticPathFree(&Path);
		return __xrtHttpServerStaticStatusFuture(
			XHTTP_STATUS_NOT_FOUND
		);
	}
	pContext = __xrtHttpServerStaticContextCreate(
		pRequest,
		&Config.Reply
	);
	if ( pContext == NULL ) {
		xrtHttpStaticPathFree(&Path);
		return NULL;
	}
	pFileFuture = __xrtHttpServerStaticPrepareFuture(
		pPool,
		Root,
		&Path,
		Config.Indexes,
		Config.IndexCount
	);
	xrtHttpStaticPathFree(&Path);
	if ( pFileFuture == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
		return NULL;
	}
	pReplyFuture = xrtFutureContinueOwnedCancelSource(
		pFileFuture,
		__xrtHttpServerStaticFileReady,
		pContext,
		__xrtHttpServerStaticContextFree,
		NULL
	);
	xrtFutureDestroy(pFileFuture);
	if ( pReplyFuture == NULL ) {
		__xrtHttpServerStaticContextFree(
			pContext,
			NULL
		);
	}
	return pReplyFuture;
}



/* 把当前请求的一站式静态 Reply Future 交给 Connection。 */
XHTTP_SERVER_API bool xrtHttpConnStatic(
	xhttpconn* pConnection,
	xtaskpool* pPool,
	xroot Root,
	const xhttpstaticserveconfig* pConfig
)
{
	const xhttpserverrequest* pRequest;
	xfuture* pFuture;
	bool bResult;

	if ( pConnection == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	pRequest = xrtHttpConnRequest(pConnection);
	if ( pRequest == NULL ) {
		xrtSetErrorKind(XERR_STATE);
		return false;
	}
	pFuture = xrtHttpReplyStaticFuture(
		pPool,
		Root,
		pRequest,
		pConfig
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bResult = xrtHttpConnRespondFuture(
		pConnection,
		pFuture
	);
	xrtFutureDestroy(pFuture);
	return bResult;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_route.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTE)




#if defined(XHTTP_FEATURE_HTTP_ROUTE)

/* 判断字节是否为 ASCII 字母。 */
static bool __xrtHttpRouteAlpha(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') &&
		(iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'z'));
}



/* 判断字节是否为 ASCII 十进制数字。 */
static bool __xrtHttpRouteDigit(unsigned char iByte)
{
	return (iByte >= (unsigned char)'0') &&
		(iByte <= (unsigned char)'9');
}



/* 判断字节是否为 ASCII 十六进制数字。 */
static bool __xrtHttpRouteHex(unsigned char iByte)
{
	return __xrtHttpRouteDigit(iByte) ||
		((iByte >= (unsigned char)'A') &&
		 (iByte <= (unsigned char)'F')) ||
		((iByte >= (unsigned char)'a') &&
		 (iByte <= (unsigned char)'f'));
}



/* 判断字节是否属于 RFC 3986 unreserved。 */
static bool __xrtHttpRouteUnreserved(unsigned char iByte)
{
	return __xrtHttpRouteAlpha(iByte) ||
		__xrtHttpRouteDigit(iByte) ||
		(iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'~');
}



/* 判断字节是否属于 RFC 3986 sub-delims。 */
static bool __xrtHttpRouteSubDelimiter(unsigned char iByte)
{
	return (iByte == (unsigned char)'!') ||
		(iByte == (unsigned char)'$') ||
		(iByte == (unsigned char)'&') ||
		(iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)',') ||
		(iByte == (unsigned char)';') ||
		(iByte == (unsigned char)'=');
}



/* 验证静态模板段或输入路径段中的 RFC 3986 pchar。 */
static bool __xrtHttpRoutePathBytesValid(xstrview Text)
{
	size_t i;

	for ( i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( __xrtHttpRouteUnreserved(iByte) ||
			__xrtHttpRouteSubDelimiter(iByte) ||
			(iByte == (unsigned char)':') ||
			(iByte == (unsigned char)'@') ||
			(iByte == (unsigned char)'/') ) {
			continue;
		}
		if ( (iByte == (unsigned char)'%') &&
			((i + 2u) < Text.Size) &&
			__xrtHttpRouteHex((unsigned char)Text.Data[i + 1u]) &&
			__xrtHttpRouteHex((unsigned char)Text.Data[i + 2u]) ) {
			i += 2u;
			continue;
		}
		return false;
	}
	return true;
}



/* 从可能未对齐的捕获数组读取一个固定描述符。 */
static void __xrtHttpRouteParamLoad(
	const xhttprouteparam* pParams,
	size_t iIndex,
	xhttprouteparam* pParam
)
{
	memcpy(
		pParam,
		(const uint8*)(const void*)pParams +
			(iIndex * sizeof(*pParam)),
		sizeof(*pParam)
	);
}



/* 向可能未对齐的捕获数组写入一个固定描述符。 */
static void __xrtHttpRouteParamStore(
	xhttprouteparam* pParams,
	size_t iIndex,
	xstrview Name,
	xstrview Value
)
{
	xhttprouteparam Param;

	Param.Name = Name;
	Param.Value = Value;
	memcpy(
		(uint8*)(void*)pParams + (iIndex * sizeof(Param)),
		&Param,
		sizeof(Param)
	);
}



/* 初始化保留重复斜杠和尾斜杠的路径段游标。 */
void __xrtHttpRouteCursorInit(
	xrt_http_route_cursor* pCursor,
	xstrview Text
)
{
	pCursor->Text = Text;
	pCursor->Position = Text.Size == 1u ? 2u : 1u;
}



/* 读取下一个路径段；空段是有效结果，根路径没有任何段。 */
bool __xrtHttpRouteCursorNext(
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
)
{
	size_t iStart;
	size_t iEnd;

	if ( pCursor->Position > pCursor->Text.Size ) {
		return false;
	}
	iStart = pCursor->Position;
	iEnd = iStart;
	while ( (iEnd < pCursor->Text.Size) &&
		(pCursor->Text.Data[iEnd] != '/') ) {
		iEnd++;
	}
	pSegment->Data = pCursor->Text.Data + iStart;
	pSegment->Size = iEnd - iStart;
	pCursor->Position = iEnd < pCursor->Text.Size ?
		iEnd + 1u : pCursor->Text.Size + 1u;
	return true;
}



/* 判断参数名是否符合稳定且便于语言绑定的 ASCII 标识符规则。 */
static bool __xrtHttpRouteNameValid(xstrview Name)
{
	size_t i;

	if ( (Name.Size == 0) ||
		(!__xrtHttpRouteAlpha((unsigned char)Name.Data[0]) &&
		 (Name.Data[0] != '_')) ) {
		return false;
	}
	for ( i = 1u; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( !__xrtHttpRouteAlpha(iByte) &&
			!__xrtHttpRouteDigit(iByte) &&
			(iByte != (unsigned char)'_') ) {
			return false;
		}
	}
	return true;
}



/* 把已经切分的模板段解析为静态段、参数段或末尾参数。 */
bool __xrtHttpRouteSegmentParse(
	xstrview Text,
	bool bLast,
	xrt_http_route_segment* pSegment
)
{
	size_t i;

	memset(pSegment, 0, sizeof(*pSegment));
	pSegment->Text = Text;
	for ( i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] == '{') || (Text.Data[i] == '}') ) {
			break;
		}
	}
	if ( i == Text.Size ) {
		if ( !__xrtHttpRoutePathBytesValid(Text) ) {
			return false;
		}
		pSegment->Kind = XRT_HTTP_ROUTE_STATIC;
		return true;
	}
	if ( (Text.Size < 3u) || (Text.Data[0] != '{') ||
		(Text.Data[Text.Size - 1u] != '}') ) {
		return false;
	}
	pSegment->Name.Data = Text.Data + 1u;
	pSegment->Name.Size = Text.Size - 2u;
	if ( (pSegment->Name.Size >= 3u) &&
		(memcmp(
			pSegment->Name.Data + pSegment->Name.Size - 3u,
			"...", 3u
		 ) == 0) ) {
		pSegment->Kind = XRT_HTTP_ROUTE_TAIL;
		pSegment->Name.Size -= 3u;
		if ( !bLast ) {
			return false;
		}
	} else {
		pSegment->Kind = XRT_HTTP_ROUTE_PARAMETER;
	}
	return __xrtHttpRouteNameValid(pSegment->Name);
}



/* 在当前段之前查找同名参数，防止匹配结果出现有歧义的重复名称。 */
static bool __xrtHttpRouteNameSeen(
	xstrview Pattern,
	size_t iEnd,
	xstrview Name
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( (Cursor.Position < iEnd) &&
		__xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		if ( !__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		) ) {
			return false;
		}
		if ( (Segment.Kind != XRT_HTTP_ROUTE_STATIC) &&
			(Segment.Name.Size == Name.Size) &&
			(memcmp(Segment.Name.Data, Name.Data, Name.Size) == 0) ) {
			return true;
		}
	}
	return false;
}



/* 验证完整模板并统计唯一参数。 */
static bool __xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iCount = 0;

	if ( (Pattern.Size == 0) || (Pattern.Data[0] != '/') ) {
		return false;
	}
	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iCurrent = (size_t)(Text.Data - Pattern.Data);

		if ( !__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		) ) {
			return false;
		}
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			continue;
		}
		if ( __xrtHttpRouteNameSeen(
			Pattern, iCurrent, Segment.Name
		) ) {
			return false;
		}
		if ( iCount == SIZE_MAX ) {
			return false;
		}
		iCount++;
	}
	*pParameters = iCount;
	return true;
}



/* 验证调用方提供的是不含 query 或 fragment 的绝对 RFC 3986 路径。 */
bool __xrtHttpRoutePathValid(xstrview Path)
{
	return (Path.Size != 0) && (Path.Data[0] == '/') &&
		__xrtHttpRoutePathBytesValid(Path);
}



/* 执行一次无写入匹配，命中时返回尾参数原始起点。 */
xhttproutestatus __xrtHttpRouteMatchValidated(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams
)
{
	xrt_http_route_cursor PatternCursor;
	xrt_http_route_cursor PathCursor;
	xrt_http_route_segment Route;
	xstrview PatternSegment;
	xstrview PathSegment;
	size_t iParameter = 0;

	__xrtHttpRouteCursorInit(&PatternCursor, Pattern);
	__xrtHttpRouteCursorInit(&PathCursor, Path);
	while ( __xrtHttpRouteCursorNext(
		&PatternCursor, &PatternSegment
	) ) {
		size_t iTailStart;

		(void)__xrtHttpRouteSegmentParse(
			PatternSegment,
			PatternCursor.Position > Pattern.Size,
			&Route
		);
		if ( Route.Kind == XRT_HTTP_ROUTE_TAIL ) {
			iTailStart = PathCursor.Position;
			if ( iTailStart > Path.Size ) {
				if ( (PatternSegment.Data == (Pattern.Data + 1u)) &&
					(Path.Size == 1u) ) {
					iTailStart = 1u;
				} else {
					return XHTTP_ROUTE_MISS;
				}
			}
			if ( pParams != NULL ) {
				xstrview Value;

				Value.Data = Path.Data + iTailStart;
				Value.Size = Path.Size - iTailStart;
				__xrtHttpRouteParamStore(
					pParams, iParameter, Route.Name, Value
				);
			}
			return XHTTP_ROUTE_MATCH;
		}
		if ( !__xrtHttpRouteCursorNext(
			&PathCursor, &PathSegment
		) ) {
			return XHTTP_ROUTE_MISS;
		}
		if ( Route.Kind == XRT_HTTP_ROUTE_STATIC ) {
			if ( (Route.Text.Size != PathSegment.Size) ||
				(memcmp(
					Route.Text.Data,
					PathSegment.Data,
					Route.Text.Size
				 ) != 0) ) {
				return XHTTP_ROUTE_MISS;
			}
			continue;
		}
		if ( PathSegment.Size == 0 ) {
			return XHTTP_ROUTE_MISS;
		}
		if ( pParams != NULL ) {
			__xrtHttpRouteParamStore(
				pParams, iParameter, Route.Name, PathSegment
			);
		}
		iParameter++;
	}
	if ( __xrtHttpRouteCursorNext(&PathCursor, &PathSegment) ) {
		return XHTTP_ROUTE_MISS;
	}
	return XHTTP_ROUTE_MATCH;
}



/* 验证绝对模板、参数语法和参数名称唯一性。 */
XHTTP_SERVER_API bool xrtHttpRouteValidate(
	xstrview Pattern,
	size_t* pParameters
)
{
	size_t iParameters = 0;

	if ( !xrtMemRangeValid(pParameters, sizeof(iParameters)) ||
		!__xhttpViewValid(Pattern) ||
		xrtMemRangesOverlap(
			pParameters, sizeof(iParameters),
			Pattern.Data, Pattern.Size
		) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	memcpy((void*)pParameters, &iParameters, sizeof(iParameters));
	if ( !__xrtHttpRouteValidate(Pattern, &iParameters) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	memcpy((void*)pParameters, &iParameters, sizeof(iParameters));
	return true;
}



/* 严格匹配原始路径，并在容量足够时一次性写入全部借用捕获。 */
XHTTP_SERVER_API xhttproutestatus xrtHttpRouteMatch(
	xstrview Pattern,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount
)
{
	xhttproutestatus Status;
	size_t iParameters;
	size_t iBytes;
	size_t iResult = 0;

	if ( !xrtMemRangeValid(pCount, sizeof(iResult)) ||
		!__xhttpViewValid(Pattern) ||
		!__xhttpViewValid(Path) ||
		((pParams == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pParams))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTE_ERROR;
	}
	iBytes = iCapacity * sizeof(*pParams);
	if ( !xrtMemRangeValid(pParams, iBytes) ||
		xrtMemRangesOverlap(
		pCount, sizeof(iResult), Pattern.Data, Pattern.Size
	) || xrtMemRangesOverlap(
		pCount, sizeof(iResult), Path.Data, Path.Size
	) || xrtMemRangesOverlap(
		pCount, sizeof(iResult), pParams, iBytes
	) || xrtMemRangesOverlap(
		pParams, iBytes, Pattern.Data, Pattern.Size
	) || xrtMemRangesOverlap(
		pParams, iBytes, Path.Data, Path.Size
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTE_ERROR;
	}
	memcpy((void*)pCount, &iResult, sizeof(iResult));
	if ( !__xrtHttpRouteValidate(Pattern, &iParameters) ||
		!__xrtHttpRoutePathValid(Path) ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_ROUTE_ERROR;
	}
	Status = __xrtHttpRouteMatchValidated(Pattern, Path, NULL);
	if ( Status != XHTTP_ROUTE_MATCH ) {
		return Status;
	}
	iResult = iParameters;
	memcpy((void*)pCount, &iResult, sizeof(iResult));
	if ( iCapacity < iParameters ) {
		return XHTTP_ROUTE_MORE;
	}
	if ( iParameters != 0 ) {
		Status = __xrtHttpRouteMatchValidated(
			Pattern, Path, pParams
		);
		if ( Status != XHTTP_ROUTE_MATCH ) {
			iResult = 0;
			memcpy((void*)pCount, &iResult, sizeof(iResult));
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_ROUTE_ERROR;
		}
	}
	return XHTTP_ROUTE_MATCH;
}



/* 按参数名查找捕获，正常未找到不会污染线程错误。 */
XHTTP_SERVER_API const xhttprouteparam* xrtHttpRouteParam(
	const xhttprouteparam* pParams,
	size_t iCount,
	xstrview Name
)
{
	xhttprouteparam Param;
	size_t iBytes;
	size_t i;

	if ( ((pParams == NULL) && (iCount != 0)) ||
		(iCount > (SIZE_MAX / sizeof(*pParams))) ||
		!__xhttpViewValid(Name) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	iBytes = iCount * sizeof(*pParams);
	if ( !xrtMemRangeValid(pParams, iBytes) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	for ( i = 0; i < iCount; i++ ) {
		__xrtHttpRouteParamLoad(pParams, i, &Param);
		if ( !__xhttpViewValid(Param.Name) ||
			!__xhttpViewValid(Param.Value) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return NULL;
		}
		if ( (Param.Name.Size == Name.Size) &&
			((Name.Size == 0) || (memcmp(
				Param.Name.Data, Name.Data, Name.Size
			 ) == 0)) ) {
			return (const xhttprouteparam*)(const void*)(
				(const uint8*)(const void*)pParams +
				(i * sizeof(Param))
			);
		}
	}
	return NULL;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_router.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTER)




#if defined(XHTTP_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_DEFAULT_ROUTES ((size_t)4096u)
#define XRT_HTTP_ROUTER_DEFAULT_NODES ((size_t)16384u)
#define XRT_HTTP_ROUTER_DEFAULT_BYTES ((size_t)4194304u)



/* 安全累加注册预检尺寸。 */
static bool __xrtHttpRouterAddSize(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( iLeft > (SIZE_MAX - iRight) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 按上限扩展任意连续内部数组，失败不改变原指针和容量。 */
static bool __xrtHttpRouterReserve(
	ptr* ppData,
	size_t* pCapacity,
	size_t iRequired,
	size_t iMaximum,
	size_t iUnit
)
{
	ptr pData;
	size_t iCapacity;

	if ( (iRequired > iMaximum) ||
		(iRequired > (SIZE_MAX / iUnit)) ) {
		xrtSetErrorKind(XERR_RANGE);
		return false;
	}
	if ( iRequired <= *pCapacity ) {
		return true;
	}
	iCapacity = *pCapacity != 0 ? *pCapacity :
		(iMaximum < 8u ? iMaximum : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (iMaximum / 2u) ?
			iMaximum : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pData = xrtRealloc(*ppData, iCapacity * iUnit);
	if ( pData == NULL ) {
		return false;
	}
	*ppData = pData;
	*pCapacity = iCapacity;
	return true;
}



/* 在线性注册链中查找同一父节点下的静态边。 */
static size_t __xrtHttpRouterBuildStatic(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Text
)
{
	size_t iEdge = pRouter->Nodes[iNode].BuildEdge;

	while ( iEdge != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_build_edge* pEdge =
			&pRouter->BuildEdges[iEdge];

		if ( (pEdge->Size == Text.Size) &&
			(memcmp(
				pRouter->Arena + pEdge->Text,
				Text.Data,
				Text.Size
			 ) == 0) ) {
			return pEdge->Child;
		}
		iEdge = pEdge->Next;
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 在叶节点查找已经注册的同名方法。 */
static bool __xrtHttpRouterMethodExists(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method
)
{
	size_t iRoute = pRouter->Nodes[iNode].Route;

	while ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[iRoute];

		if ( (pRoute->MethodSize == Method.Size) &&
			(memcmp(
				pRouter->Arena + pRoute->Method,
				Method.Data,
				Method.Size
			 ) == 0) ) {
			return true;
		}
		iRoute = pRoute->Next;
	}
	return false;
}



/* 预检结构路径，精确计算新增节点、静态边和静态文本字节。 */
static bool __xrtHttpRouterPreflight(
	const xhttprouter* pRouter,
	xstrview Pattern,
	xstrview Method,
	size_t* pNodes,
	size_t* pEdges,
	size_t* pBytes
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iNode = 0;
	size_t iNodes = 0;
	size_t iEdges = 0;
	size_t iBytes = 0;
	bool bMissing = false;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iChild = XRT_HTTP_ROUTER_NONE;

		(void)__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		);
		if ( !bMissing ) {
			if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
				iChild = __xrtHttpRouterBuildStatic(
					pRouter, iNode, Text
				);
			} else if ( Segment.Kind ==
				XRT_HTTP_ROUTE_PARAMETER ) {
				iChild = pRouter->Nodes[iNode].Parameter;
			} else {
				iChild = pRouter->Nodes[iNode].Tail;
			}
			if ( iChild != XRT_HTTP_ROUTER_NONE ) {
				iNode = iChild;
				continue;
			}
			bMissing = true;
		}
		if ( !__xrtHttpRouterAddSize(iNodes, 1u, &iNodes) ) {
			return false;
		}
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			if ( !__xrtHttpRouterAddSize(
				iEdges, 1u, &iEdges
			) || !__xrtHttpRouterAddSize(
				iBytes, Text.Size, &iBytes
			) ) {
				return false;
			}
		}
	}
	if ( !bMissing && __xrtHttpRouterMethodExists(
		pRouter, iNode, Method
	) ) {
		xrtSetErrorKind(XERR_VALUE);
		return false;
	}
	*pNodes = iNodes;
	*pEdges = iEdges;
	*pBytes = iBytes;
	return true;
}



/* 追加一个已经预留容量的空节点。 */
static size_t __xrtHttpRouterNodeAppend(
	xhttprouter* pRouter,
	size_t iParent,
	uint8 iParentKind
)
{
	xrt_http_router_node* pNode =
		&pRouter->Nodes[pRouter->NodeCount];
	size_t iNode = pRouter->NodeCount++;

	memset(pNode, 0, sizeof(*pNode));
	pNode->Parent = iParent;
	pNode->Depth = pRouter->Nodes[iParent].Depth + 1u;
	pNode->BuildEdge = XRT_HTTP_ROUTER_NONE;
	pNode->Parameter = XRT_HTTP_ROUTER_NONE;
	pNode->Tail = XRT_HTTP_ROUTER_NONE;
	pNode->Route = XRT_HTTP_ROUTER_NONE;
	pNode->ParentKind = iParentKind;
	return iNode;
}



/* 在已经预留的 Arena 尾部复制借用文本并返回偏移。 */
static size_t __xrtHttpRouterTextAppend(
	xhttprouter* pRouter,
	xstrview Text
)
{
	size_t iOffset = pRouter->ArenaSize;

	if ( Text.Size != 0 ) {
		memcpy(pRouter->Arena + iOffset, Text.Data, Text.Size);
	}
	pRouter->ArenaSize += Text.Size;
	return iOffset;
}



/* 追加静态边和子节点，并挂到父节点注册链表头。 */
static size_t __xrtHttpRouterStaticAppend(
	xhttprouter* pRouter,
	size_t iParent,
	xstrview Text
)
{
	xrt_http_router_build_edge* pEdge =
		&pRouter->BuildEdges[pRouter->BuildEdgeCount];
	size_t iChild = __xrtHttpRouterNodeAppend(
		pRouter, iParent, XRT_HTTP_ROUTER_PARENT_STATIC
	);
	size_t iEdge = pRouter->BuildEdgeCount++;

	pEdge->Parent = iParent;
	pEdge->Child = iChild;
	pEdge->Text = __xrtHttpRouterTextAppend(pRouter, Text);
	pEdge->Size = Text.Size;
	pEdge->Next = pRouter->Nodes[iParent].BuildEdge;
	pRouter->Nodes[iParent].BuildEdge = iEdge;
	return iChild;
}



/* 按已验证模板创建全部缺失节点并返回叶节点。 */
static size_t __xrtHttpRouterBuildPath(
	xhttprouter* pRouter,
	xstrview Pattern
)
{
	xrt_http_route_cursor Cursor;
	xrt_http_route_segment Segment;
	xstrview Text;
	size_t iNode = 0;

	__xrtHttpRouteCursorInit(&Cursor, Pattern);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Text) ) {
		size_t iChild;

		(void)__xrtHttpRouteSegmentParse(
			Text, Cursor.Position > Pattern.Size, &Segment
		);
		if ( Segment.Kind == XRT_HTTP_ROUTE_STATIC ) {
			iChild = __xrtHttpRouterBuildStatic(
				pRouter, iNode, Text
			);
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterStaticAppend(
					pRouter, iNode, Text
				);
			}
		} else if ( Segment.Kind == XRT_HTTP_ROUTE_PARAMETER ) {
			iChild = pRouter->Nodes[iNode].Parameter;
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterNodeAppend(
					pRouter, iNode,
					XRT_HTTP_ROUTER_PARENT_PARAMETER
				);
				pRouter->Nodes[iNode].Parameter = iChild;
			}
		} else {
			iChild = pRouter->Nodes[iNode].Tail;
			if ( iChild == XRT_HTTP_ROUTER_NONE ) {
				iChild = __xrtHttpRouterNodeAppend(
					pRouter, iNode,
					XRT_HTTP_ROUTER_PARENT_PARAMETER
				);
				pRouter->Nodes[iNode].Tail = iChild;
			}
		}
		iNode = iChild;
	}
	return iNode;
}



/* 比较两个冻结静态边的原始段文本。 */
static int __xrtHttpRouterEdgeCompare(
	const xhttprouter* pRouter,
	const xrt_http_router_edge* pLeft,
	const xrt_http_router_edge* pRight
)
{
	size_t iCommon = pLeft->Size < pRight->Size ?
		pLeft->Size : pRight->Size;
	int iCompare = memcmp(
		pRouter->Arena + pLeft->Text,
		pRouter->Arena + pRight->Text,
		iCommon
	);

	if ( iCompare != 0 ) {
		return iCompare;
	}
	return pLeft->Size < pRight->Size ? -1 :
		(pLeft->Size > pRight->Size ? 1 : 0);
}



/* 交换同一节点静态边区间中的两个元素。 */
static void __xrtHttpRouterEdgeSwap(
	xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	size_t iLeft,
	size_t iRight
)
{
	xrt_http_router_edge Edge =
		pRouter->Edges[pNode->EdgeStart + iLeft];

	pRouter->Edges[pNode->EdgeStart + iLeft] =
		pRouter->Edges[pNode->EdgeStart + iRight];
	pRouter->Edges[pNode->EdgeStart + iRight] = Edge;
}



/* 在静态边最大堆中向下恢复字典序。 */
static void __xrtHttpRouterEdgeSift(
	xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	size_t iRoot,
	size_t iEnd
)
{
	if ( iEnd == 0 ) {
		return;
	}
	while ( iRoot <= ((iEnd - 1u) / 2u) ) {
		size_t iChild = (iRoot * 2u) + 1u;

		if ( (iChild < iEnd) &&
			(__xrtHttpRouterEdgeCompare(
				pRouter,
				&pRouter->Edges[pNode->EdgeStart + iChild],
				&pRouter->Edges[pNode->EdgeStart + iChild + 1u]
			 ) < 0) ) {
			iChild++;
		}
		if ( __xrtHttpRouterEdgeCompare(
			pRouter,
			&pRouter->Edges[pNode->EdgeStart + iRoot],
			&pRouter->Edges[pNode->EdgeStart + iChild]
		) >= 0 ) {
			return;
		}
		__xrtHttpRouterEdgeSwap(
			pRouter, pNode, iRoot, iChild
		);
		iRoot = iChild;
	}
}



/* 对单个节点的静态边执行无分配堆排序。 */
static void __xrtHttpRouterEdgesSort(
	xhttprouter* pRouter,
	xrt_http_router_node* pNode
)
{
	size_t i;

	if ( pNode->EdgeCount < 2u ) {
		return;
	}
	for ( i = pNode->EdgeCount / 2u; i != 0; i-- ) {
		__xrtHttpRouterEdgeSift(
			pRouter, pNode, i - 1u, pNode->EdgeCount - 1u
		);
	}
	for ( i = pNode->EdgeCount - 1u; i != 0; i-- ) {
		__xrtHttpRouterEdgeSwap(pRouter, pNode, 0, i);
		__xrtHttpRouterEdgeSift(
			pRouter, pNode, 0, i - 1u
		);
	}
}



/* 初始化适合中型服务的显式默认限额。 */
XHTTP_SERVER_API void xrtHttpRouterConfigInit(xhttprouterconfig* pConfig)
{
	const xhttprouterconfig Config = {
		XRT_HTTP_ROUTER_DEFAULT_ROUTES,
		XRT_HTTP_ROUTER_DEFAULT_NODES,
		XRT_HTTP_ROUTER_DEFAULT_BYTES
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 创建只持有一个根节点的空 Router。 */
XHTTP_SERVER_API xhttprouter* xrtHttpRouterCreate(
	const xhttprouterconfig* pConfig
)
{
	xhttprouterconfig Config;
	xhttprouter* pRouter;

	if ( pConfig == NULL ) {
		xrtHttpRouterConfigInit(&Config);
	} else {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			xrtSetErrorKind(XERR_ARGUMENT);
			return NULL;
		}
		memcpy(&Config, (const void*)pConfig, sizeof(Config));
	}
	if ( (Config.MaxRoutes == 0) ||
		(Config.MaxNodes == 0) ||
		(Config.MaxBytes == 0) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return NULL;
	}
	pRouter = (xhttprouter*)xrtMalloc(sizeof(*pRouter));
	if ( pRouter == NULL ) {
		return NULL;
	}
	memset(pRouter, 0, sizeof(*pRouter));
	pRouter->Config = Config;
	if ( !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Nodes,
		&pRouter->NodeCapacity,
		1u,
		pRouter->Config.MaxNodes,
		sizeof(*pRouter->Nodes)
	) ) {
		xrtHttpRouterDestroy(pRouter);
		return NULL;
	}
	memset(&pRouter->Nodes[0], 0, sizeof(pRouter->Nodes[0]));
	pRouter->Nodes[0].Parent = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].BuildEdge = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Parameter = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Tail = XRT_HTTP_ROUTER_NONE;
	pRouter->Nodes[0].Route = XRT_HTTP_ROUTER_NONE;
	pRouter->NodeCount = 1u;
	return pRouter;
}



/* 释放注册图或冻结索引及其全部拥有文本。 */
XHTTP_SERVER_API void xrtHttpRouterDestroy(xhttprouter* pRouter)
{
	if ( pRouter == NULL ) {
		return;
	}
	xrtFree(pRouter->Nodes);
	xrtFree(pRouter->BuildEdges);
	xrtFree(pRouter->Edges);
	xrtFree(pRouter->Routes);
	xrtFree(pRouter->Arena);
	xrtFree(pRouter);
}



/* 原子预留后把一个方法和结构路径提交到注册图。 */
XHTTP_SERVER_API bool xrtHttpRouterAdd(
	xhttprouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	ptr pValue
)
{
	xrt_http_router_route* pRoute;
	size_t iParameters;
	size_t iAddNodes;
	size_t iAddEdges;
	size_t iAddBytes;
	size_t iNodes;
	size_t iEdges;
	size_t iRoutes;
	size_t iBytes;
	size_t iLeaf;

	if ( (pRouter == NULL) || pRouter->Frozen ||
		!__xhttpViewValid(Method) ||
		(Method.Size == 0) || !xrtHttpTokenValid(Method) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( !xrtHttpRouteValidate(Pattern, &iParameters) ||
		!__xrtHttpRouterPreflight(
			pRouter, Pattern, Method,
			&iAddNodes, &iAddEdges, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			pRouter->NodeCount, iAddNodes, &iNodes
		) || !__xrtHttpRouterAddSize(
			pRouter->BuildEdgeCount, iAddEdges, &iEdges
		) || !__xrtHttpRouterAddSize(
			pRouter->RouteCount, 1u, &iRoutes
		) || !__xrtHttpRouterAddSize(
			iAddBytes, Method.Size, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			iAddBytes, Pattern.Size, &iAddBytes
		) || !__xrtHttpRouterAddSize(
			pRouter->ArenaSize, iAddBytes, &iBytes
		) ) {
		return false;
	}
	if ( !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Nodes, &pRouter->NodeCapacity,
		iNodes, pRouter->Config.MaxNodes,
		sizeof(*pRouter->Nodes)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->BuildEdges,
		&pRouter->BuildEdgeCapacity,
		iEdges,
		pRouter->Config.MaxNodes - 1u,
		sizeof(*pRouter->BuildEdges)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Routes, &pRouter->RouteCapacity,
		iRoutes, pRouter->Config.MaxRoutes,
		sizeof(*pRouter->Routes)
	) || !__xrtHttpRouterReserve(
		(ptr*)&pRouter->Arena, &pRouter->ArenaCapacity,
		iBytes, pRouter->Config.MaxBytes, sizeof(char)
	) ) {
		return false;
	}
	iLeaf = __xrtHttpRouterBuildPath(pRouter, Pattern);
	pRoute = &pRouter->Routes[pRouter->RouteCount++];
	pRoute->Next = pRouter->Nodes[iLeaf].Route;
	pRoute->Method = __xrtHttpRouterTextAppend(pRouter, Method);
	pRoute->MethodSize = Method.Size;
	pRoute->Pattern = __xrtHttpRouterTextAppend(pRouter, Pattern);
	pRoute->PatternSize = Pattern.Size;
	pRoute->Parameters = iParameters;
	pRoute->Value = pValue;
	pRouter->Nodes[iLeaf].Route = pRouter->RouteCount - 1u;
	return true;
}



/* 构建节点连续有序静态边，成功后丢弃注册链表。 */
XHTTP_SERVER_API bool xrtHttpRouterFreeze(xhttprouter* pRouter)
{
	xrt_http_router_edge* pEdges = NULL;
	size_t iOffset = 0;
	size_t i;

	if ( pRouter == NULL ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	if ( pRouter->Frozen ) {
		return true;
	}
	if ( pRouter->BuildEdgeCount != 0 ) {
		if ( pRouter->BuildEdgeCount >
			(SIZE_MAX / sizeof(*pEdges)) ) {
			xrtSetErrorKind(XERR_RANGE);
			return false;
		}
		pEdges = (xrt_http_router_edge*)xrtMalloc(
			pRouter->BuildEdgeCount * sizeof(*pEdges)
		);
		if ( pEdges == NULL ) {
			return false;
		}
	}
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		pRouter->Nodes[i].EdgeCount = 0;
	}
	for ( i = 0; i < pRouter->BuildEdgeCount; i++ ) {
		pRouter->Nodes[pRouter->BuildEdges[i].Parent].EdgeCount++;
	}
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		pRouter->Nodes[i].EdgeStart = iOffset;
		pRouter->Nodes[i].EdgeWrite = iOffset;
		iOffset += pRouter->Nodes[i].EdgeCount;
	}
	for ( i = 0; i < pRouter->BuildEdgeCount; i++ ) {
		const xrt_http_router_build_edge* pBuild =
			&pRouter->BuildEdges[i];
		xrt_http_router_node* pNode =
			&pRouter->Nodes[pBuild->Parent];
		xrt_http_router_edge* pEdge =
			&pEdges[pNode->EdgeWrite++];

		pEdge->Child = pBuild->Child;
		pEdge->Text = pBuild->Text;
		pEdge->Size = pBuild->Size;
	}
	pRouter->Edges = pEdges;
	for ( i = 0; i < pRouter->NodeCount; i++ ) {
		__xrtHttpRouterEdgesSort(pRouter, &pRouter->Nodes[i]);
	}
	xrtFree(pRouter->BuildEdges);
	pRouter->BuildEdges = NULL;
	pRouter->BuildEdgeCount = 0;
	pRouter->BuildEdgeCapacity = 0;
	pRouter->Frozen = true;
	return true;
}



/* 返回不可变阶段事实。 */
XHTTP_SERVER_API bool xrtHttpRouterFrozen(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->Frozen : false;
}



/* 返回注册路由数量。 */
XHTTP_SERVER_API size_t xrtHttpRouterCount(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->RouteCount : 0;
}



/* 返回包含根节点的预编译节点数量。 */
XHTTP_SERVER_API size_t xrtHttpRouterNodes(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->NodeCount : 0;
}



/* 返回 Arena 实际使用字节。 */
XHTTP_SERVER_API size_t xrtHttpRouterBytes(const xhttprouter* pRouter)
{
	return pRouter != NULL ? pRouter->ArenaSize : 0;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_router_match.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTER)



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

/* 按字典序比较冻结静态边与输入路径段。 */
static int __xrtHttpRouterStaticCompare(
	const xhttprouter* pRouter,
	const xrt_http_router_edge* pEdge,
	xstrview Segment
)
{
	size_t iCommon = pEdge->Size < Segment.Size ?
		pEdge->Size : Segment.Size;
	int iCompare = memcmp(
		pRouter->Arena + pEdge->Text,
		Segment.Data,
		iCommon
	);

	if ( iCompare != 0 ) {
		return iCompare;
	}
	return pEdge->Size < Segment.Size ? -1 :
		(pEdge->Size > Segment.Size ? 1 : 0);
}



/* 在节点连续静态边区间中执行二分查找。 */
static size_t __xrtHttpRouterStaticFind(
	const xhttprouter* pRouter,
	const xrt_http_router_node* pNode,
	xstrview Segment
)
{
	size_t iLeft = 0;
	size_t iRight = pNode->EdgeCount;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		const xrt_http_router_edge* pEdge =
			&pRouter->Edges[pNode->EdgeStart + iMiddle];
		int iCompare = __xrtHttpRouterStaticCompare(
			pRouter, pEdge, Segment
		);

		if ( iCompare < 0 ) {
			iLeft = iMiddle + 1u;
		} else if ( iCompare > 0 ) {
			iRight = iMiddle;
		} else {
			return pEdge->Child;
		}
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 统计严格路径段数量；根路径为零，重复和尾斜杠均产生空段。 */
static size_t __xrtHttpRouterPathDepth(xstrview Path)
{
	xrt_http_route_cursor Cursor;
	xstrview Segment;
	size_t iDepth = 0;

	__xrtHttpRouteCursorInit(&Cursor, Path);
	while ( __xrtHttpRouteCursorNext(&Cursor, &Segment) ) {
		iDepth++;
	}
	return iDepth;
}



/* 仅在回溯时把路径游标恢复到指定的已消费段数。 */
static bool __xrtHttpRouterPathReplay(
	xstrview Path,
	size_t iCount,
	xrt_http_route_cursor* pCursor,
	xstrview* pSegment
)
{
	size_t i;

	__xrtHttpRouteCursorInit(pCursor, Path);
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xrtHttpRouteCursorNext(pCursor, pSegment) ) {
			return false;
		}
	}
	return true;
}



/* 判断方法视图是否与编译路由方法逐字节相同。 */
static bool __xrtHttpRouterMethodEqual(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute,
	xstrview Method
)
{
	return (pRoute->MethodSize == Method.Size) &&
		(memcmp(
			pRouter->Arena + pRoute->Method,
			Method.Data,
			Method.Size
		 ) == 0);
}



/* 在同一结构叶上按精确方法、HEAD 回退、任意方法顺序选择。 */
static size_t __xrtHttpRouterRouteSelect(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method,
	uint32* pFlags,
	bool* pHasMethod
)
{
	static const xstrview Head = XRT_STR_INIT("HEAD");
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Any = XRT_STR_INIT("*");
	size_t iRoute = pRouter->Nodes[iNode].Route;
	size_t iGet = XRT_HTTP_ROUTER_NONE;
	size_t iAny = XRT_HTTP_ROUTER_NONE;
	bool bHead = (Method.Size == Head.Size) &&
		(memcmp(Method.Data, Head.Data, Head.Size) == 0);

	if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		*pHasMethod = true;
	}
	while ( iRoute != XRT_HTTP_ROUTER_NONE ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[iRoute];

		if ( __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Method
		) ) {
			*pFlags = 0;
			return iRoute;
		}
		if ( bHead && __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Get
		) ) {
			iGet = iRoute;
		} else if ( __xrtHttpRouterMethodEqual(
			pRouter, pRoute, Any
		) ) {
			iAny = iRoute;
		}
		iRoute = pRoute->Next;
	}
	if ( iGet != XRT_HTTP_ROUTER_NONE ) {
		*pFlags = XHTTP_ROUTER_HEAD_FALLBACK;
		return iGet;
	}
	if ( iAny != XRT_HTTP_ROUTER_NONE ) {
		*pFlags = XHTTP_ROUTER_ANY_METHOD;
		return iAny;
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 尝试节点叶上的方法集合并累计结构路径存在事实。 */
static size_t __xrtHttpRouterCandidate(
	const xhttprouter* pRouter,
	size_t iNode,
	xstrview Method,
	uint32* pFlags,
	bool* pHasMethod
)
{
	return __xrtHttpRouterRouteSelect(
		pRouter, iNode, Method, pFlags, pHasMethod
	);
}



/*
	按静态、参数、尾参数优先级迭代深度优先搜索。
	子节点父关系携带回溯状态，路径位置只在回溯时按深度重放。
*/
static size_t __xrtHttpRouterSearch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	uint32* pFlags,
	bool* pHasMethod
)
{
	xrt_http_route_cursor PathCursor;
	size_t iPathDepth = __xrtHttpRouterPathDepth(Path);
	size_t iNode = 0;
	bool bEnter = true;

	__xrtHttpRouteCursorInit(&PathCursor, Path);
	for ( ;; ) {
		const xrt_http_router_node* pNode = &pRouter->Nodes[iNode];

		if ( bEnter ) {
			xstrview Segment;
			size_t iChild;
			size_t iRoute;

			if ( pNode->Depth == iPathDepth ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, iNode, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
				if ( (iNode == 0) &&
					(pNode->Tail != XRT_HTTP_ROUTER_NONE) ) {
					iRoute = __xrtHttpRouterCandidate(
						pRouter, pNode->Tail, Method,
						pFlags, pHasMethod
					);
					if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
						return iRoute;
					}
				}
				bEnter = false;
				continue;
			}
			if ( !__xrtHttpRouteCursorNext(
				&PathCursor, &Segment
			) ) {
				return XRT_HTTP_ROUTER_NONE;
			}
			iChild = __xrtHttpRouterStaticFind(
				pRouter, pNode, Segment
			);
			if ( iChild != XRT_HTTP_ROUTER_NONE ) {
				iNode = iChild;
				continue;
			}
			if ( (pNode->Parameter != XRT_HTTP_ROUTER_NONE) &&
				(Segment.Size != 0) ) {
				iNode = pNode->Parameter;
				continue;
			}
			if ( pNode->Tail != XRT_HTTP_ROUTER_NONE ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, pNode->Tail, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
			}
			bEnter = false;
			continue;
		}
		if ( iNode == 0 ) {
			break;
		}
		{
			size_t iParent = pNode->Parent;
			uint8 iKind = pNode->ParentKind;
			const xrt_http_router_node* pParent =
				&pRouter->Nodes[iParent];
			xstrview Segment;
			size_t iRoute;

			iNode = iParent;
			if ( !__xrtHttpRouterPathReplay(
				Path, pParent->Depth + 1u,
				&PathCursor, &Segment
			) ) {
				return XRT_HTTP_ROUTER_NONE;
			}
			if ( (iKind == XRT_HTTP_ROUTER_PARENT_STATIC) &&
				(pParent->Parameter != XRT_HTTP_ROUTER_NONE) &&
				(Segment.Size != 0) ) {
				iNode = pParent->Parameter;
				bEnter = true;
				continue;
			}
			if ( pParent->Tail != XRT_HTTP_ROUTER_NONE ) {
				iRoute = __xrtHttpRouterCandidate(
					pRouter, pParent->Tail, Method,
					pFlags, pHasMethod
				);
				if ( iRoute != XRT_HTTP_ROUTER_NONE ) {
					return iRoute;
				}
			}
			bEnter = false;
		}
	}
	return XRT_HTTP_ROUTER_NONE;
}



/* 验证输出互不覆盖输入和彼此，避免借用匹配过程中自修改。 */
static bool __xrtHttpRouterOutputsValid(
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iBytes,
	size_t* pCount,
	xhttproutermatch* pMatch
)
{
	return !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), Method.Data, Method.Size
	) && !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), Path.Data, Path.Size
	) && !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), pParams, iBytes
	) && !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), pMatch, sizeof(*pMatch)
	) && !xrtMemRangesOverlap(
		pMatch, sizeof(*pMatch), Method.Data, Method.Size
	) && !xrtMemRangesOverlap(
		pMatch, sizeof(*pMatch), Path.Data, Path.Size
	) && !xrtMemRangesOverlap(
		pMatch, sizeof(*pMatch), pParams, iBytes
	) && !xrtMemRangesOverlap(
		pParams, iBytes, Method.Data, Method.Size
	) && !xrtMemRangesOverlap(
		pParams, iBytes, Path.Data, Path.Size
	);
}



/* 匹配冻结索引，并仅在容量足够时写入借用参数。 */
XHTTP_SERVER_API xhttprouterstatus xrtHttpRouterMatch(
	const xhttprouter* pRouter,
	xstrview Method,
	xstrview Path,
	xhttprouteparam* pParams,
	size_t iCapacity,
	size_t* pCount,
	xhttproutermatch* pMatch
)
{
	const xrt_http_router_route* pRoute;
	xhttproutermatch Match;
	xhttproutestatus RouteStatus;
	size_t iCount = 0;
	size_t iRoute;
	size_t iBytes;
	uint32 iFlags = 0;
	bool bHasMethod = false;

	memset(&Match, 0, sizeof(Match));
	if ( (pRouter == NULL) || !pRouter->Frozen ||
		!xrtMemRangeValid(pCount, sizeof(iCount)) ||
		!xrtMemRangeValid(pMatch, sizeof(Match)) ||
		!__xhttpViewValid(Method) ||
		!__xhttpViewValid(Path) ||
		(Method.Size == 0) || !xrtHttpTokenValid(Method) ||
		((pParams == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pParams))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTER_ERROR;
	}
	iBytes = iCapacity * sizeof(*pParams);
	if ( !xrtMemRangeValid(pParams, iBytes) ||
		!__xrtHttpRouterOutputsValid(
		Method, Path, pParams, iBytes, pCount, pMatch
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTER_ERROR;
	}
	memcpy((void*)pCount, &iCount, sizeof(iCount));
	memcpy((void*)pMatch, &Match, sizeof(Match));
	if ( !__xrtHttpRoutePathValid(Path) ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_ROUTER_ERROR;
	}
	iRoute = __xrtHttpRouterSearch(
		pRouter, Method, Path, &iFlags, &bHasMethod
	);
	if ( iRoute == XRT_HTTP_ROUTER_NONE ) {
		return bHasMethod ? XHTTP_ROUTER_METHOD_NOT_ALLOWED :
			XHTTP_ROUTER_NOT_FOUND;
	}
	pRoute = &pRouter->Routes[iRoute];
	Match.Flags = iFlags;
	Match.Method = __xrtHttpRouterView(
		pRouter, pRoute->Method, pRoute->MethodSize
	);
	Match.Pattern = __xrtHttpRouterView(
		pRouter, pRoute->Pattern, pRoute->PatternSize
	);
	Match.Value = pRoute->Value;
	iCount = pRoute->Parameters;
	memcpy((void*)pMatch, &Match, sizeof(Match));
	memcpy((void*)pCount, &iCount, sizeof(iCount));
	if ( iCapacity < pRoute->Parameters ) {
		return XHTTP_ROUTER_MORE;
	}
	if ( pRoute->Parameters != 0 ) {
		RouteStatus = __xrtHttpRouteMatchValidated(
			Match.Pattern, Path, pParams
		);
		if ( RouteStatus != XHTTP_ROUTE_MATCH ) {
			iCount = 0;
			memset(&Match, 0, sizeof(Match));
			memcpy((void*)pCount, &iCount, sizeof(iCount));
			memcpy((void*)pMatch, &Match, sizeof(Match));
			xrtSetErrorKind(XERR_INTERNAL);
			return XHTTP_ROUTER_ERROR;
		}
	}
	return XHTTP_ROUTER_MATCH;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_router_methods.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_ROUTER)



#if defined(XHTTP_FEATURE_HTTP_ROUTER)

#define XRT_HTTP_ROUTER_LOCAL_METHODS ((size_t)16u)



/* 返回路由记录借用的方法。 */
static xstrview __xrtHttpRouterRouteMethod(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute
)
{
	return __xrtHttpRouterView(
		pRouter, pRoute->Method, pRoute->MethodSize
	);
}



/* 判断两个 HTTP 方法是否逐字节相同。 */
static bool __xrtHttpRouterMethodsEqual(
	xstrview Left,
	xstrview Right
)
{
	return (Left.Size == Right.Size) &&
		(memcmp(Left.Data, Right.Data, Left.Size) == 0);
}



/* 判断一个冻结路由模板是否匹配原始路径。 */
static bool __xrtHttpRouterRouteMatches(
	const xhttprouter* pRouter,
	const xrt_http_router_route* pRoute,
	xstrview Path
)
{
	xstrview Pattern = __xrtHttpRouterView(
		pRouter, pRoute->Pattern, pRoute->PatternSize
	);

	return __xrtHttpRouteMatchValidated(
		Pattern, Path, NULL
	) == XHTTP_ROUTE_MATCH;
}



/* 在小型方法集合中查找重复方法。 */
static bool __xrtHttpRouterMethodsContains(
	const xstrview* pMethods,
	size_t iCount,
	xstrview Method
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpRouterMethodsEqual(
			pMethods[i], Method
		) ) {
			return true;
		}
	}
	return false;
}



/* 向可能未对齐的方法数组写入一个借用视图。 */
static void __xrtHttpRouterMethodStore(
	xstrview* pMethods,
	size_t iIndex,
	xstrview Method
)
{
	memcpy(
		(uint8*)(void*)pMethods + (iIndex * sizeof(Method)),
		&Method,
		sizeof(Method)
	);
}



/* 向固定本地集合追加唯一方法，空间不足时报告需要慢速精确扫描。 */
static bool __xrtHttpRouterMethodsLocalAdd(
	xstrview* pMethods,
	size_t* pCount,
	xstrview Method
)
{
	if ( __xrtHttpRouterMethodsContains(
		pMethods, *pCount, Method
	) ) {
		return true;
	}
	if ( *pCount == XRT_HTTP_ROUTER_LOCAL_METHODS ) {
		return false;
	}
	pMethods[(*pCount)++] = Method;
	return true;
}



/* 常见方法集合用单次路由扫描收集到固定本地数组。 */
static bool __xrtHttpRouterMethodsLocal(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t* pCount
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < pRouter->RouteCount; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Method;

		if ( !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		Method = __xrtHttpRouterRouteMethod(pRouter, pRoute);
		if ( !__xrtHttpRouterMethodsLocalAdd(
			pMethods, &iCount, Method
		) ) {
			return false;
		}
		if ( __xrtHttpRouterMethodsEqual(Method, Get) &&
			!__xrtHttpRouterMethodsLocalAdd(
				pMethods, &iCount, Head
			) ) {
			return false;
		}
	}
	*pCount = iCount;
	return true;
}



/* 判断指定路由之前是否已有匹配路径发布同一个显式或合成方法。 */
static bool __xrtHttpRouterMethodSeenBefore(
	const xhttprouter* pRouter,
	xstrview Path,
	size_t iEnd,
	xstrview Method
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t i;

	for ( i = 0; i < iEnd; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Previous = __xrtHttpRouterRouteMethod(
			pRouter, pRoute
		);
		bool bSame = __xrtHttpRouterMethodsEqual(
			Previous, Method
		) || (__xrtHttpRouterMethodsEqual(Method, Head) &&
			__xrtHttpRouterMethodsEqual(Previous, Get));

		if ( !bSame || !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		return true;
	}
	return false;
}



/* 极端方法集合用无分配嵌套扫描精确计数或写出。 */
static size_t __xrtHttpRouterMethodsLarge(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods
)
{
	static const xstrview Get = XRT_STR_INIT("GET");
	static const xstrview Head = XRT_STR_INIT("HEAD");
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < pRouter->RouteCount; i++ ) {
		const xrt_http_router_route* pRoute =
			&pRouter->Routes[i];
		xstrview Method;

		if ( !__xrtHttpRouterRouteMatches(
			pRouter, pRoute, Path
		) ) {
			continue;
		}
		Method = __xrtHttpRouterRouteMethod(pRouter, pRoute);
		if ( !__xrtHttpRouterMethodSeenBefore(
			pRouter, Path, i, Method
		) ) {
			if ( pMethods != NULL ) {
				__xrtHttpRouterMethodStore(
					pMethods, iCount, Method
				);
			}
			iCount++;
		}
		if ( __xrtHttpRouterMethodsEqual(Method, Get) &&
			!__xrtHttpRouterMethodSeenBefore(
				pRouter, Path, i, Head
			) ) {
			if ( pMethods != NULL ) {
				__xrtHttpRouterMethodStore(
					pMethods, iCount, Head
				);
			}
			iCount++;
		}
	}
	return iCount;
}



/* 验证方法数组和计数输出不会覆盖路径或彼此。 */
static bool __xrtHttpRouterMethodsOutputsValid(
	xstrview Path,
	xstrview* pMethods,
	size_t iBytes,
	size_t* pCount
)
{
	return !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), Path.Data, Path.Size
	) && !xrtMemRangesOverlap(
		pCount, sizeof(*pCount), pMethods, iBytes
	) && !xrtMemRangesOverlap(
		pMethods, iBytes, Path.Data, Path.Size
	);
}



/* 列出匹配结构路径的唯一方法，并为 GET 合成稳定 HEAD 能力。 */
XHTTP_SERVER_API xhttprouterstatus xrtHttpRouterMethods(
	const xhttprouter* pRouter,
	xstrview Path,
	xstrview* pMethods,
	size_t iCapacity,
	size_t* pCount
)
{
	xstrview Local[XRT_HTTP_ROUTER_LOCAL_METHODS];
	size_t iCount = 0;
	size_t iBytes;
	bool bLocal;

	if ( (pRouter == NULL) || !pRouter->Frozen ||
		!xrtMemRangeValid(pCount, sizeof(iCount)) ||
		!__xhttpViewValid(Path) ||
		((pMethods == NULL) && (iCapacity != 0)) ||
		(iCapacity > (SIZE_MAX / sizeof(*pMethods))) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTER_ERROR;
	}
	iBytes = iCapacity * sizeof(*pMethods);
	if ( !xrtMemRangeValid(pMethods, iBytes) ||
		!__xrtHttpRouterMethodsOutputsValid(
		Path, pMethods, iBytes, pCount
	) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return XHTTP_ROUTER_ERROR;
	}
	memcpy((void*)pCount, &iCount, sizeof(iCount));
	if ( !__xrtHttpRoutePathValid(Path) ) {
		xrtSetErrorKind(XERR_VALUE);
		return XHTTP_ROUTER_ERROR;
	}
	bLocal = __xrtHttpRouterMethodsLocal(
		pRouter, Path, Local, &iCount
	);
	if ( !bLocal ) {
		iCount = __xrtHttpRouterMethodsLarge(
			pRouter, Path, NULL
		);
	}
	memcpy((void*)pCount, &iCount, sizeof(iCount));
	if ( iCount == 0 ) {
		return XHTTP_ROUTER_NOT_FOUND;
	}
	if ( iCapacity < iCount ) {
		return XHTTP_ROUTER_MORE;
	}
	if ( bLocal ) {
		memcpy((void*)pMethods, Local, iCount * sizeof(*pMethods));
	} else {
		(void)__xrtHttpRouterMethodsLarge(
			pRouter, Path, pMethods
		);
	}
	return XHTTP_ROUTER_MATCH;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_reply_compress.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS)




#if defined(XHTTP_FEATURE_HTTP_REPLY_COMPRESS)

#define XRT_HTTP_REPLY_COMPRESS_CODINGS \
	((uint32)XHTTP_CODING_GZIP | \
	 (uint32)XHTTP_CODING_DEFLATE)

#define XRT_HTTP_REPLY_COMPRESS_FLAGS \
	((uint32)XHTTP_REPLY_COMPRESS_ALLOW_ABSENT | \
	 (uint32)XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH | \
	 (uint32)XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE | \
	 (uint32)XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM | \
	 (uint32)XHTTP_REPLY_COMPRESS_KEEP_LARGER)



/* 保存一组与压缩选择相关的响应字段事实。 */
typedef struct xrt_http_reply_compress_fields {
	bool VaryCoding;
	bool VaryAny;
	bool KeepETag;
} xrt_http_reply_compress_fields;



/* 建立 Reply 压缩域错误。 */
void __xrtHttpReplyCompressError(
	xerrkind Kind,
	int32 iCode,
	cstr sOperation,
	cstr sMessage
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = iCode;
	Desc.Domain = "http.reply.compress";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetErrorTake(pError);
	}
}



/* 向可能未对齐的公开输出槽一次性发布 Reply 指针。 */
static void __xrtHttpReplyCompressPublish(
	xhttpreply** ppOutput,
	xhttpreply* pOutput
)
{
	memcpy((void*)ppOutput, &pOutput, sizeof(pOutput));
}



/* 把公开配置解析成完整默认值并一次验证全部边界。 */
static bool __xrtHttpReplyCompressConfigResolve(
	const xhttpreplycompressconfig* pInput,
	xhttpreplycompressconfig* pConfig
)
{
	xdeflateconfig Deflate;

	xrtHttpReplyCompressConfigInit(pConfig);
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(
			pInput, sizeof(*pInput)
		) ) {
			__xrtHttpReplyCompressError(
				XERR_ARGUMENT,
				XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
				"config",
				"HTTP Reply compression config range is invalid"
			);
			return false;
		}
		memcpy(pConfig, (const void*)pInput, sizeof(*pConfig));
	}
	if ( ((pConfig->Codings &
		  ~XRT_HTTP_REPLY_COMPRESS_CODINGS) != 0) ||
		((pConfig->Preferred != XHTTP_CODING_NONE) &&
		 (pConfig->Preferred != XHTTP_CODING_GZIP) &&
		 (pConfig->Preferred != XHTTP_CODING_DEFLATE)) ||
		((pConfig->Preferred != XHTTP_CODING_NONE) &&
		 ((pConfig->Codings &
		   (uint32)pConfig->Preferred) == 0)) ||
		(pConfig->MinimumSize > pConfig->MaximumSize) ||
		(pConfig->ReadSize == 0) ||
		((pConfig->Flags &
		  ~XRT_HTTP_REPLY_COMPRESS_FLAGS) != 0) ) {
		__xrtHttpReplyCompressError(
			XERR_VALUE,
			XHTTP_REPLY_COMPRESS_ERROR_CONFIG,
			"config",
			"HTTP Reply compression configuration is invalid"
		);
		return false;
	}
	xrtDeflateConfigInit(&Deflate);
	Deflate.Format = XDEFLATE_GZIP;
	Deflate.Level = pConfig->Level;
	Deflate.Strategy = pConfig->Strategy;
	Deflate.OutputLimit = pConfig->OutputLimit;
	return xrtDeflateConfigValid(&Deflate);
}



/* 统计一个响应字段，并借出唯一字段。 */
static size_t __xrtHttpReplyCompressField(
	const xhttpreply* pReply,
	xstrview Name,
	const xhttpfield** ppField
)
{
	const xhttpfield* pOnly = NULL;
	size_t iCount = 0;
	size_t i;

	for ( i = 0; i < xrtHttpReplyHeaderCount(pReply); i++ ) {
		const xhttpfield* pField =
			xrtHttpReplyHeaderAt(pReply, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(pField->Name, Name) ) {
			pOnly = pField;
			iCount++;
		}
	}
	if ( ppField != NULL ) {
		*ppField = iCount == 1 ? pOnly : NULL;
	}
	return iCount;
}



/* 严格扫描全部 Cache-Control 字段并识别 no-transform。 */
static bool __xrtHttpReplyCompressNoTransform(
	const xhttpreply* pReply,
	bool* pNoTransform
)
{
	xhttpcachecontrol Cache;
	size_t i;

	xrtHttpCacheControlInit(&Cache);
	for ( i = 0; i < xrtHttpReplyHeaderCount(pReply); i++ ) {
		const xhttpfield* pField =
			xrtHttpReplyHeaderAt(pReply, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL("Cache-Control")
			) && !xrtHttpCacheControlAdd(
				&Cache, pField->Value
			) ) {
			__xrtHttpReplyCompressError(
				XERR_PROTOCOL,
				XHTTP_REPLY_COMPRESS_ERROR_HEADER,
				"cache-control",
				"HTTP Cache-Control field is malformed"
			);
			return false;
		}
	}
	if ( (Cache.Flags & XHTTP_CACHE_INVALID) != 0 ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"cache-control",
			"HTTP Cache-Control directive argument is invalid"
		);
		return false;
	}
	*pNoTransform =
		(Cache.Flags & XHTTP_CACHE_NO_TRANSFORM) != 0;
	return true;
}



/* 读取完整 Vary 计划和已有 Accept-Encoding 选择维度。 */
static bool __xrtHttpReplyCompressVary(
	const xhttpreply* pReply,
	xrt_http_reply_compress_fields* pFields
)
{
	const xhttpfield* pHeaders = xrtHttpReplyHeaderData(pReply);
	size_t iCount = xrtHttpReplyHeaderCount(pReply);
	xhttpvaryplan Plan;
	xhttpvaryitem Item;
	xhttpnext Next;

	if ( !xrtHttpVaryPlan(
		pHeaders, iCount, &Plan
	) ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"vary",
			"HTTP Vary field is malformed"
		);
		return false;
	}
	Next = xrtHttpVaryFind(
		pHeaders,
		iCount,
		XRT_STR_LITERAL("Accept-Encoding"),
		&Item
	);
	if ( Next == XHTTP_NEXT_ERROR ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_HEADER,
			"vary",
			"HTTP Vary field is malformed"
		);
		return false;
	}
	pFields->VaryAny =
		(Plan.Flags & XHTTP_VARY_WILDCARD) != 0;
	pFields->VaryCoding = Next == XHTTP_NEXT_ITEM;
	return true;
}



/* 只让一个语法正确的弱 ETag 跨越表示编码变换。 */
static bool __xrtHttpReplyCompressKeepETag(
	const xhttpreply* pReply
)
{
	const xhttpfield* pField;
	xhttpetag Tag;
	xerror* pPrevious;
	bool bParsed;

	if ( __xrtHttpReplyCompressField(
		pReply, XRT_STR_LITERAL("ETag"), &pField
	) != 1 ) {
		return false;
	}
	pPrevious = xrtTakeError();
	bParsed = xrtHttpETagParse(pField->Value, &Tag);
	xrtClearError();
	if ( pPrevious != NULL ) {
		xrtSetError(pPrevious);
		xrtErrorFree(pPrevious);
	}
	return bParsed && Tag.Weak;
}



/* 判断状态、方法和已有表示元数据是否禁止自动变换。 */
static bool __xrtHttpReplyCompressHardSkip(
	xstrview Method,
	const xhttpreply* pReply,
	xhttpbody** ppBody,
	uint64* pLength
)
{
	uint16 iStatus = xrtHttpReplyStatus(pReply);
	xhttpbody* pBody = xrtHttpReplyBody(pReply);
	bool bHead = xrtHttpMethodEqual(
		Method, XRT_STR_LITERAL("HEAD")
	);

	if ( (!xrtHttpResponseContentAllowed(Method, iStatus) &&
		  !bHead) ||
		(iStatus == 206) ||
		(pBody == NULL) ||
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Content-Encoding")
		 ) != NULL) ||
		(xrtHttpReplyHeader(
			pReply, XRT_STR_LITERAL("Content-Range")
		 ) != NULL) ) {
		return true;
	}
	*pLength = xrtHttpBodyLength(pBody);
	if ( *pLength == 0 ) {
		return true;
	}
	*ppBody = pBody;
	return false;
}



/* 判断未编码表示是否满足自动压缩策略。 */
static int __xrtHttpReplyCompressEligible(
	const xhttpreply* pReply,
	uint64 iLength,
	const xhttpreplycompressconfig* pConfig
)
{
	const xhttpfield* pContentType;
	xmediatype MediaType;
	bool bNoTransform;
	size_t iContentTypes;

	if ( !__xrtHttpReplyCompressNoTransform(
		pReply, &bNoTransform
	) ) {
		return -1;
	}
	if ( bNoTransform &&
		((pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_IGNORE_NO_TRANSFORM) == 0) ) {
		return 0;
	}
	if ( iLength == XHTTP_BODY_UNKNOWN ) {
		if ( (pConfig->Flags &
			  XHTTP_REPLY_COMPRESS_ALLOW_UNKNOWN_LENGTH) == 0 ) {
			return 0;
		}
	} else if ( (iLength < pConfig->MinimumSize) ||
		(iLength > pConfig->MaximumSize) ) {
		return 0;
	}
	if ( pConfig->Codings == 0 ) {
		return 0;
	}
	if ( (pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_ALLOW_ANY_TYPE) != 0 ) {
		return 1;
	}
	iContentTypes = __xrtHttpReplyCompressField(
		pReply,
		XRT_STR_LITERAL("Content-Type"),
		&pContentType
	);
	if ( iContentTypes == 0 ) {
		return 0;
	}
	if ( iContentTypes != 1 ) {
		__xrtHttpReplyCompressError(
			XERR_PROTOCOL,
			XHTTP_REPLY_COMPRESS_ERROR_RESPONSE,
			"content-type",
			"HTTP Reply has multiple Content-Type fields"
		);
		return -1;
	}
	if ( !xrtHttpMediaTypeParse(
		pContentType->Value, &MediaType
	) ) {
		return -1;
	}
	return xrtHttpMediaTypeCompressible(&MediaType) ? 1 : 0;
}



/* 根据协商编码建立 Deflate 配置。 */
static void __xrtHttpReplyCompressDeflate(
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding,
	xdeflateconfig* pDeflate
)
{
	xrtDeflateConfigInit(pDeflate);
	pDeflate->Format = Coding == XHTTP_CODING_GZIP ?
		XDEFLATE_GZIP : XDEFLATE_ZLIB;
	pDeflate->Level = pConfig->Level;
	pDeflate->Strategy = pConfig->Strategy;
	pDeflate->OutputLimit = pConfig->OutputLimit;
}



/* 创建一次性压缩正文；当更大的结果可以回退时返回空正文和 Identity。 */
static xhttpbody* __xrtHttpReplyCompressEager(
	xhttpbody* pSource,
	const xhttpacceptencoding* pAccept,
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding,
	bool* pIdentity
)
{
	xdeflateconfig Deflate;
	xbytesview Input;
	bytes pOutput;
	xhttpbody* pBody;
	size_t iOutput;

	*pIdentity = false;
	if ( !xrtHttpBodyView(pSource, &Input) ||
		(Input.Size > pConfig->EagerLimit) ) {
		return NULL;
	}
	__xrtHttpReplyCompressDeflate(
		pConfig, Coding, &Deflate
	);
	pOutput = xrtDeflateAll(Input, &Deflate, &iOutput);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( (iOutput >= Input.Size) &&
		((pConfig->Flags &
		  XHTTP_REPLY_COMPRESS_KEEP_LARGER) == 0) &&
		(xrtHttpAcceptEncodingQuality(
			pAccept, XHTTP_CODING_IDENTITY
		 ) != 0) ) {
		xrtFree(pOutput);
		*pIdentity = true;
		return NULL;
	}
	pBody = xrtHttpBodyTake(pOutput, iOutput);
	if ( pBody == NULL ) {
		xrtFree(pOutput);
	}
	return pBody;
}



/* 创建延迟压缩正文，不读取来源。 */
static xhttpbody* __xrtHttpReplyCompressStream(
	xhttpbody* pSource,
	const xhttpreplycompressconfig* pConfig,
	xhttpcoding Coding
)
{
	xhttpbodydeflateconfig Body;

	memset(&Body, 0, sizeof(Body));
	__xrtHttpReplyCompressDeflate(
		pConfig, Coding, &Body.Deflate
	);
	Body.ReadSize = pConfig->ReadSize;
	Body.QueueLimit = pConfig->QueueLimit;
	return xrtHttpBodyDeflate(pSource, &Body);
}



/* 克隆 identity Reply，并按需补充 Accept-Encoding 的缓存维度。 */
static xhttpreply* __xrtHttpReplyCompressIdentity(
	const xhttpreply* pReply,
	const xrt_http_reply_compress_fields* pFields
)
{
	xhttpreply* pOutput = xrtHttpReplyClone(pReply);

	if ( (pOutput != NULL) &&
		!pFields->VaryAny &&
		!pFields->VaryCoding &&
		!xrtHttpReplyAddHeader(
			pOutput,
			XRT_STR_LITERAL("Vary"),
			XRT_STR_LITERAL("Accept-Encoding")
		) ) {
		xrtHttpReplyDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}



/* 删除内容编码变换后不再成立的表示和线路元数据。 */
static void __xrtHttpReplyCompressRemoveStale(
	xhttpreply* pReply,
	bool bKeepETag
)
{
	static const xstrview Headers[] = {
		XRT_STR_INIT("Content-Length"),
		XRT_STR_INIT("Transfer-Encoding"),
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest"),
		XRT_STR_INIT("Accept-Ranges"),
		XRT_STR_INIT("Trailer")
	};
	static const xstrview Trailers[] = {
		XRT_STR_INIT("Content-MD5"),
		XRT_STR_INIT("Digest"),
		XRT_STR_INIT("Content-Digest"),
		XRT_STR_INIT("Repr-Digest")
	};
	size_t i;

	for ( i = 0;
		  i < (sizeof(Headers) / sizeof(Headers[0]));
		  i++ ) {
		(void)xrtHttpReplyRemoveHeader(
			pReply, Headers[i]
		);
	}
	for ( i = 0;
		  i < (sizeof(Trailers) / sizeof(Trailers[0]));
		  i++ ) {
		(void)xrtHttpReplyRemoveTrailer(
			pReply, Trailers[i]
		);
	}
	if ( !bKeepETag ) {
		(void)xrtHttpReplyRemoveHeader(
			pReply, XRT_STR_LITERAL("ETag")
		);
	}
}



/* 克隆 Reply、替换正文并提交内容编码元数据。 */
static xhttpreply* __xrtHttpReplyCompressApply(
	const xhttpreply* pReply,
	xhttpbody* pBody,
	xhttpcoding Coding,
	const xrt_http_reply_compress_fields* pFields
)
{
	xhttpreply* pOutput = __xrtHttpReplyCompressIdentity(
		pReply, pFields
	);

	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtHttpReplySetBody(pOutput, pBody) ||
		!xrtHttpReplySetHeader(
			pOutput,
			XRT_STR_LITERAL("Content-Encoding"),
			xrtHttpCodingName(Coding)
		) ) {
		xrtHttpReplyDestroy(pOutput);
		return NULL;
	}
	__xrtHttpReplyCompressRemoveStale(
		pOutput, pFields->KeepETag
	);
	return pOutput;
}



/* 初始化稳定的自动压缩默认策略。 */
XHTTP_SERVER_API void xrtHttpReplyCompressConfigInit(
	xhttpreplycompressconfig* pConfig
)
{
	xhttpreplycompressconfig Config;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return;
	}
	memset(&Config, 0, sizeof(Config));
	Config.Codings =
		XHTTP_CODING_GZIP |
		XHTTP_CODING_DEFLATE;
	Config.Preferred = XHTTP_CODING_GZIP;
	Config.MinimumSize =
		XHTTP_REPLY_COMPRESS_MIN_DEFAULT;
	Config.MaximumSize = UINT64_MAX;
	Config.EagerLimit =
		XHTTP_REPLY_COMPRESS_EAGER_DEFAULT;
	Config.ReadSize =
		XHTTP_BODY_DEFLATE_READ_DEFAULT;
	Config.QueueLimit =
		XHTTP_BODY_DEFLATE_QUEUE_DEFAULT;
	Config.Level = XDEFLATE_LEVEL_DEFAULT;
	Config.Strategy = XDEFLATE_STRATEGY_DEFAULT;
	Config.OutputLimit = XDEFLATE_OUTPUT_UNLIMITED;
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 选择内容编码并失败原子地生成独立 Reply。 */
XHTTP_SERVER_API xhttpreplycompressstatus xrtHttpReplyCompress(
	const xhttpacceptencoding* pAccept,
	xstrview Method,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
)
{
	xhttpacceptencoding Accept;
	xhttpreplycompressconfig Config;
	xrt_http_reply_compress_fields Fields;
	xhttpcoding Coding;
	xhttpbody* pSource = NULL;
	xhttpbody* pBody = NULL;
	xhttpreply* pOutput;
	uint64 iLength = XHTTP_BODY_UNKNOWN;
	int iEligible;
	bool bIdentity = false;

	if ( !xrtMemRangeValid(ppOutput, sizeof(*ppOutput)) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression output is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( xrtMemRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pAccept, sizeof(*pAccept)
		) ||
		!xrtHttpBridgeReplyOutputValid(pReply, ppOutput, sizeof(*ppOutput)) ||
		((pConfig != NULL) &&
		 xrtMemRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pConfig, sizeof(*pConfig)
		 )) ||
		xrtMemRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			Method.Data, Method.Size
		) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	__xrtHttpReplyCompressPublish(ppOutput, NULL);
	if ( !xrtMemRangeValid(pAccept, sizeof(*pAccept)) ||
		(pReply == NULL) ||
		!xrtHttpTokenValid(Method) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	memcpy(&Accept, (const void*)pAccept, sizeof(Accept));
	if ( !xrtHttpAcceptEncodingValid(&Accept) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress",
			"HTTP Reply compression negotiation is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( !__xrtHttpReplyCompressConfigResolve(
		pConfig, &Config
	) ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( __xrtHttpReplyCompressHardSkip(
		Method, pReply, &pSource, &iLength
	) ) {
		return XHTTP_REPLY_COMPRESS_SKIP;
	}
	iEligible = __xrtHttpReplyCompressEligible(
		pReply, iLength, &Config
	);
	if ( iEligible < 0 ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( iEligible == 0 ) {
		Coding = xrtHttpAcceptEncodingSelect(
			&Accept,
			XHTTP_CODING_IDENTITY,
			XHTTP_CODING_IDENTITY
		);
		return Coding == XHTTP_CODING_NONE ?
			XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE :
			XHTTP_REPLY_COMPRESS_SKIP;
	}
	memset(&Fields, 0, sizeof(Fields));
	if ( !__xrtHttpReplyCompressVary(
		pReply, &Fields
	) ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	Fields.KeepETag =
		__xrtHttpReplyCompressKeepETag(pReply);
	if ( ((Accept.Flags &
		  XHTTP_ACCEPT_ENCODING_PRESENT) == 0) &&
		((Config.Flags &
		  XHTTP_REPLY_COMPRESS_ALLOW_ABSENT) == 0) ) {
		Coding = XHTTP_CODING_IDENTITY;
	} else {
		Coding = xrtHttpAcceptEncodingSelect(
			&Accept,
			Config.Codings | XHTTP_CODING_IDENTITY,
			Config.Preferred
		);
	}
	if ( Coding == XHTTP_CODING_NONE ) {
		return XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE;
	}
	if ( Coding == XHTTP_CODING_IDENTITY ) {
		pOutput = __xrtHttpReplyCompressIdentity(
			pReply, &Fields
		);
		if ( pOutput == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		__xrtHttpReplyCompressPublish(ppOutput, pOutput);
		return XHTTP_REPLY_COMPRESS_IDENTITY;
	}
	pBody = __xrtHttpReplyCompressEager(
		pSource, &Accept, &Config, Coding, &bIdentity
	);
	if ( bIdentity ) {
		pOutput = __xrtHttpReplyCompressIdentity(
			pReply, &Fields
		);
		if ( pOutput == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		__xrtHttpReplyCompressPublish(ppOutput, pOutput);
		return XHTTP_REPLY_COMPRESS_IDENTITY;
	}
	if ( pBody == NULL ) {
		xbytesview View;

		if ( xrtHttpBodyView(pSource, &View) &&
			(View.Size <= Config.EagerLimit) ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
		pBody = __xrtHttpReplyCompressStream(
			pSource, &Config, Coding
		);
		if ( pBody == NULL ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
	}
	pOutput = __xrtHttpReplyCompressApply(
		pReply, pBody, Coding, &Fields
	);
	xrtHttpBodyDestroy(pBody);
	if ( pOutput == NULL ) {
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	__xrtHttpReplyCompressPublish(ppOutput, pOutput);
	return XHTTP_REPLY_COMPRESS_APPLIED;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_compress.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS)




#if defined(XHTTP_FEATURE_HTTP_SERVER_COMPRESS)

/* 相邻压缩实现文件提供的本库私有错误入口。 */
void __xrtHttpReplyCompressError(
	xerrkind Kind, int32 iCode,
	cstr sOperation, cstr sMessage
);

/* 从服务端请求合并全部 Accept-Encoding 后调用无网络压缩层。 */
XHTTP_SERVER_API xhttpreplycompressstatus xrtHttpServerReplyCompress(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
)
{
	xhttpacceptencoding Accept;
	xhttpreply* pOutput = NULL;
	size_t i;

	if ( !xrtMemRangeValid(ppOutput, sizeof(*ppOutput)) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression output is invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	if ( !xrtHttpBridgeServerRequestOutputValid(pRequest, ppOutput, sizeof(*ppOutput)) ||
		!xrtHttpBridgeReplyOutputValid(pReply, ppOutput, sizeof(*ppOutput)) ||
		((pConfig != NULL) &&
		 xrtMemRangesOverlap(
			ppOutput, sizeof(*ppOutput),
			pConfig, sizeof(*pConfig)
		 )) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression output overlaps input"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	memcpy((void*)ppOutput, &pOutput, sizeof(pOutput));
	if ( (pRequest == NULL) || (pReply == NULL) ) {
		__xrtHttpReplyCompressError(
			XERR_ARGUMENT,
			XHTTP_REPLY_COMPRESS_ERROR_ARGUMENT,
			"compress-server-reply",
			"HTTP Server Reply compression arguments are invalid"
		);
		return XHTTP_REPLY_COMPRESS_ERROR;
	}
	xrtHttpAcceptEncodingInit(&Accept);
	for ( i = 0;
		  i < xrtHttpServerRequestHeaderCount(pRequest);
		  i++ ) {
		const xhttpfield* pField =
			xrtHttpServerRequestHeaderAt(pRequest, i);

		if ( (pField != NULL) &&
			xrtHttpFieldNameEqual(
				pField->Name,
				XRT_STR_LITERAL("Accept-Encoding")
			) &&
			!xrtHttpAcceptEncodingAdd(
				&Accept, pField->Value
			) ) {
			return XHTTP_REPLY_COMPRESS_ERROR;
		}
	}
	return xrtHttpReplyCompress(
		&Accept,
		xrtHttpServerRequestMethod(pRequest),
		pReply,
		pConfig,
		ppOutput
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_router.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/* 原因缺失或没有类别时返回调用点的稳定默认类别。 */
xerrkind __xrtHttpServerRouterCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 设置包含稳定域、代码、操作和原因链的服务端路由错误。 */
void __xrtHttpServerRouterSetError(
	xerrkind Kind,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind != XERR_NONE ? Kind :
		__xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.http.server.router";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把当前错误作为原因包装到稳定的服务端 Router 域。 */
void __xrtHttpServerRouterWrapError(
	xerrkind Default,
	xhttpserverroutererror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtHttpServerRouterSetError(
		__xrtHttpServerRouterCauseKind(pCause, Default),
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 初始化空的分阶段路由事件。 */
XHTTP_SERVER_API void xrtHttpServerRouteEventsInit(
	xhttpserverrouteevents* pEvents
)
{
	const xhttpserverrouteevents Events = { 0 };

	if ( !xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"init-http-server-route-events",
			"HTTP server route events range is invalid",
			NULL
		);
		return;
	}
	memcpy((void*)pEvents, &Events, sizeof(Events));
}



/* 创建拥有通用索引和回调表的服务端 Router。 */
XHTTP_SERVER_API xhttpserverrouter* xrtHttpServerRouterCreate(
	const xhttprouterconfig* pConfig
)
{
	xhttprouterconfig Config;
	xhttpserverrouter* pRouter;

	xrtHttpRouterConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerRouterSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
				"create-http-server-router",
				"HTTP server router config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, (const void*)pConfig, sizeof(Config));
	}
	pRouter = (xhttpserverrouter*)xrtCalloc(
		1, sizeof(*pRouter)
	);
	if ( pRouter == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"create-http-server-router",
			"HTTP server router allocation failed"
		);
		return NULL;
	}
	pRouter->Index = xrtHttpRouterCreate(&Config);
	if ( pRouter->Index == NULL ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);
		xhttpserverroutererror Code =
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
			Kind == XERR_ARGUMENT ?
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT :
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL;

		__xrtHttpServerRouterWrapError(
			Kind,
			Code,
			"create-http-server-router",
			"HTTP server router index creation failed"
		);
		xrtFree(pRouter);
		return NULL;
	}
	pRouter->References = 1;
	pRouter->MaxRoutes = Config.MaxRoutes;
	return pRouter;
}



/* 增加可跨运行中 Server 持有的 Router 引用。 */
XHTTP_SERVER_API xhttpserverrouter* xrtHttpServerRouterRef(
	xhttpserverrouter* pRouter
)
{
	if ( (pRouter == NULL) ||
		(xrtRefRetain(&pRouter->References) < 0) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"retain-http-server-router",
			"HTTP server router is null or released",
			NULL
		);
		return NULL;
	}
	return pRouter;
}



/* 释放最后一个 Router 引用和全部复制回调。 */
XHTTP_SERVER_API void xrtHttpServerRouterDestroy(
	xhttpserverrouter* pRouter
)
{
	size_t i;

	if ( (pRouter == NULL) ||
		(xrtRefRelease(&pRouter->References) != 0) ) {
		return;
	}
	for ( i = 0; i < pRouter->Count; i++ ) {
		if ( pRouter->Entries[i].Events.Release != NULL ) {
			pRouter->Entries[i].Events.Release(
				pRouter->Entries[i].Events.Data
			);
		}
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		__xrtHttpServerMiddlewareClear(pRouter);
	#endif
	xrtHttpRouterDestroy(pRouter->Index);
	xrtFree(pRouter->Entries);
	xrtFree(pRouter);
}



/* 为一个新回调记录预留稳定索引空间。 */
static bool __xrtHttpServerRouterReserve(
	xhttpserverrouter* pRouter
)
{
	size_t iRequired = pRouter->Count + 1u;
	size_t iCapacity;
	xrt_http_server_route_entry* pEntries;

	if ( iRequired > pRouter->MaxRoutes ) {
		__xrtHttpServerRouterSetError(
			XERR_RANGE,
			XHTTP_SERVER_ROUTER_ERROR_LIMIT,
			"register-http-server-route",
			"HTTP server route count exceeds the configured limit",
			NULL
		);
		return false;
	}
	if ( iRequired <= pRouter->Capacity ) {
		return true;
	}
	iCapacity = pRouter->Capacity != 0 ?
		pRouter->Capacity :
		(pRouter->MaxRoutes < 8u ? pRouter->MaxRoutes : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (pRouter->MaxRoutes / 2u) ?
			pRouter->MaxRoutes : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pEntries = (xrt_http_server_route_entry*)xrtRealloc(
		pRouter->Entries,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"register-http-server-route",
			"HTTP server route callback table allocation failed"
		);
		return false;
	}
	pRouter->Entries = pEntries;
	pRouter->Capacity = iCapacity;
	return true;
}



/* 复制一组路由回调，并用非空整数指针编码稳定记录索引。 */
XHTTP_SERVER_API bool xrtHttpServerRouteEvents(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	const xhttpserverrouteevents* pEvents
)
{
	xhttpserverrouteevents Events;
	size_t iEntry;

	if ( (pRouter == NULL) ||
		!xrtMemRangeValid(pEvents, sizeof(Events)) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-http-server-route",
			"HTTP server router, events or request handler is null",
			NULL
		);
		return false;
	}
	memcpy(&Events, (const void*)pEvents, sizeof(Events));
	if ( Events.Request == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"register-http-server-route",
			"HTTP server route request handler is null",
			NULL
		);
		return false;
	}
	if ( xrtHttpRouterFrozen(pRouter->Index) ) {
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"register-http-server-route",
			"HTTP server router is already frozen",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerRouterReserve(pRouter) ) {
		return false;
	}
	iEntry = pRouter->Count;
	if ( !xrtHttpRouterAdd(
		pRouter->Index,
		Method,
		Pattern,
		(ptr)(uintptr_t)(iEntry + 1u)
	) ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_PROTOCOL
		);
		xhttpserverroutererror Code =
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
			Kind == XERR_STATE ?
				XHTTP_SERVER_ROUTER_ERROR_STATE :
				XHTTP_SERVER_ROUTER_ERROR_TARGET;

		__xrtHttpServerRouterWrapError(
			Kind,
			Code,
			"register-http-server-route",
			"HTTP server route method or pattern was rejected"
		);
		return false;
	}
	pRouter->Entries[iEntry].Events = Events;
	pRouter->Count++;
	return true;
}



/* 用完整请求回调构造最常用的服务端路由。 */
XHTTP_SERVER_API bool xrtHttpServerRoute(
	xhttpserverrouter* pRouter,
	xstrview Method,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	xhttpserverrouteevents Events;

	xrtHttpServerRouteEventsInit(&Events);
	Events.Request = pHandler;
	Events.Data = pData;
	return xrtHttpServerRouteEvents(
		pRouter, Method, Pattern, &Events
	);
}



/* 注册 GET 路由。 */
XHTTP_SERVER_API bool xrtHttpServerGet(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("GET"),
		Pattern, pHandler, pData
	);
}



/* 注册 POST 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPost(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("POST"),
		Pattern, pHandler, pData
	);
}



/* 注册 PUT 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPut(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("PUT"),
		Pattern, pHandler, pData
	);
}



/* 注册 PATCH 路由。 */
XHTTP_SERVER_API bool xrtHttpServerPatch(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("PATCH"),
		Pattern, pHandler, pData
	);
}



/* 注册 DELETE 路由。 */
XHTTP_SERVER_API bool xrtHttpServerDelete(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("DELETE"),
		Pattern, pHandler, pData
	);
}



/* 注册任意方法路由。 */
XHTTP_SERVER_API bool xrtHttpServerAny(
	xhttpserverrouter* pRouter,
	xstrview Pattern,
	xhttpserverrouteproc pHandler,
	ptr pData
)
{
	return xrtHttpServerRoute(
		pRouter, XRT_STR_LITERAL("*"),
		Pattern, pHandler, pData
	);
}



/* 冻结通用结构索引和高层回调表。 */
XHTTP_SERVER_API bool xrtHttpServerRouterFreeze(
	xhttpserverrouter* pRouter
)
{
	if ( pRouter == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"freeze-http-server-router",
			"HTTP server router is null",
			NULL
		);
		return false;
	}
	if ( !xrtHttpRouterFreeze(pRouter->Index) ) {
		const xerror* pCause = xrtGetError();
		xerrkind Kind = __xrtHttpServerRouterCauseKind(
			pCause, XERR_INTERNAL
		);

		__xrtHttpServerRouterWrapError(
			Kind,
			Kind == XERR_MEMORY ?
				XHTTP_SERVER_ROUTER_ERROR_MEMORY :
			Kind == XERR_RANGE ?
				XHTTP_SERVER_ROUTER_ERROR_LIMIT :
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"freeze-http-server-router",
			"HTTP server router index freeze failed"
		);
		return false;
	}
	return true;
}



/* 返回高层 Router 是否冻结。 */
XHTTP_SERVER_API bool xrtHttpServerRouterFrozen(
	const xhttpserverrouter* pRouter
)
{
	return (pRouter != NULL) &&
		xrtHttpRouterFrozen(pRouter->Index);
}



/* 返回复制回调记录数量。 */
XHTTP_SERVER_API size_t xrtHttpServerRouterCount(
	const xhttpserverrouter* pRouter
)
{
	return pRouter != NULL ? pRouter->Count : 0;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_router_dispatch.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/* 判断借用文本是否与固定方法逐字节相同。 */
static bool __xrtHttpServerRouterTextEqual(
	xstrview Text,
	cstr sValue
)
{
	size_t iSize = strlen(sValue);

	return (Text.Size == iSize) &&
		(memcmp(Text.Data, sValue, iSize) == 0);
}



/* 释放匹配阶段为极端参数数量分配的描述符。 */
void __xrtHttpServerRouterMatchClear(
	xrt_http_server_route_match* pMatch
)
{
	if ( pMatch == NULL ) {
		return;
	}
	if ( (pMatch->Params != NULL) &&
		(pMatch->Params != pMatch->Local) ) {
		xrtFree(pMatch->Params);
	}
	pMatch->Params = NULL;
}



/* 把匹配结果的参数所有权转移到按需分配的请求期缓存。 */
xrt_http_server_route_match* __xrtHttpServerRouterMatchTake(
	xrt_http_server_route_match* pMatch
)
{
	xrt_http_server_route_match* pStored;

	if ( pMatch == NULL ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"cache-http-server-route",
			"HTTP server route match is null",
			NULL
		);
		return NULL;
	}
	pStored = (xrt_http_server_route_match*)xrtMalloc(
		sizeof(*pStored)
	);
	if ( pStored == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"cache-http-server-route",
			"HTTP server route cache allocation failed"
		);
		return NULL;
	}
	memcpy(pStored, pMatch, sizeof(*pStored));
	if ( pMatch->Params == pMatch->Local ) {
		pStored->Params = pStored->Local;
		if ( pMatch->Count != 0 ) {
			memcpy(
				pStored->Local,
				pMatch->Local,
				pMatch->Count * sizeof(*pMatch->Local)
			);
		}
	} else {
		pStored->Params = pMatch->Params;
	}
	pMatch->Params = NULL;
	return pStored;
}



/* 释放请求期路由缓存和极端参数模板的拥有存储。 */
void __xrtHttpServerRouterMatchDestroy(ptr pData)
{
	xrt_http_server_route_match* pMatch =
		(xrt_http_server_route_match*)pData;

	if ( pMatch == NULL ) {
		return;
	}
	__xrtHttpServerRouterMatchClear(pMatch);
	memset(pMatch, 0, sizeof(*pMatch));
	xrtFree(pMatch);
}



/* 把通用 Router 的非空整数 Value 解码为高层回调记录。 */
static bool __xrtHttpServerRouterEntry(
	const xhttpserverrouter* pRouter,
	ptr pValue,
	const xrt_http_server_route_entry** ppEntry
)
{
	uintptr_t iValue = (uintptr_t)pValue;

	if ( (iValue == 0) || (iValue > pRouter->Count) ) {
		__xrtHttpServerRouterSetError(
			XERR_INTERNAL,
			XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"match-http-server-route",
			"HTTP server route index is outside the callback table",
			NULL
		);
		return false;
	}
	*ppEntry = &pRouter->Entries[iValue - 1u];
	return true;
}



/* 解析 origin/absolute 请求目标并执行容量完整的结构匹配。 */
bool __xrtHttpServerRouterMatch(
	const xhttpserverrouter* pRouter,
	const xhttpserverrequest* pRequest,
	xrt_http_server_route_match* pMatch
)
{
	static const xstrview Root = XRT_STR_INIT("/");
	xhttptarget Target;
	xhttproutermatch Route;
	size_t iCount = 0;
	xhttprouterstatus Status;

	if ( (pRouter == NULL) || (pRequest == NULL) ||
		(pMatch == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerRouterSetError(
			(pRouter != NULL) && (pRequest != NULL) ?
				XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) && (pRequest != NULL) ?
				XHTTP_SERVER_ROUTER_ERROR_STATE :
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"match-http-server-route",
			"HTTP server router must be frozen and request must be valid",
			NULL
		);
		return false;
	}
	memset(pMatch, 0, sizeof(*pMatch));
	pMatch->Params = pMatch->Local;
	pMatch->Method = xrtHttpServerRequestMethod(pRequest);
	if ( !xrtHttpServerRequestParseTarget(
		pRequest, &Target
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_INTERNAL
			),
			XHTTP_SERVER_ROUTER_ERROR_TARGET,
			"match-http-server-route",
			"HTTP server request target could not be parsed",
			pCause
		);
		return false;
	}
	if ( (Target.Form != XHTTP_TARGET_ORIGIN) &&
		(Target.Form != XHTTP_TARGET_ABSOLUTE) ) {
		pMatch->Status = XHTTP_ROUTER_NOT_FOUND;
		return true;
	}
	pMatch->Path = Target.Path.Size != 0 ?
		Target.Path : Root;
	Status = xrtHttpRouterMatch(
		pRouter->Index,
		pMatch->Method,
		pMatch->Path,
		pMatch->Local,
		XRT_HTTP_SERVER_ROUTER_LOCAL_PARAMS,
		&iCount,
		&Route
	);
	if ( Status == XHTTP_ROUTER_ERROR ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_PROTOCOL
			),
			XHTTP_SERVER_ROUTER_ERROR_TARGET,
			"match-http-server-route",
			"HTTP server request path could not be routed",
			pCause
		);
		return false;
	}
	if ( Status == XHTTP_ROUTER_MORE ) {
		pMatch->Params = (xhttprouteparam*)xrtMalloc(
			iCount * sizeof(*pMatch->Params)
		);
		if ( pMatch->Params == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"match-http-server-route",
				"HTTP server route parameter allocation failed"
			);
			return false;
		}
		Status = xrtHttpRouterMatch(
			pRouter->Index,
			pMatch->Method,
			pMatch->Path,
			pMatch->Params,
			iCount,
			&pMatch->Count,
			&Route
		);
		if ( Status != XHTTP_ROUTER_MATCH ) {
			const xerror* pCause = xrtGetError();

			__xrtHttpServerRouterMatchClear(pMatch);
			__xrtHttpServerRouterSetError(
				XERR_INTERNAL,
				XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
				"match-http-server-route",
				"HTTP server route changed during frozen rematch",
				pCause
			);
			return false;
		}
	} else {
		pMatch->Count = iCount;
	}
	pMatch->Status = Status;
	if ( Status == XHTTP_ROUTER_MATCH ) {
		if ( !__xrtHttpServerRouterEntry(
			pRouter, Route.Value, &pMatch->Entry
		) ) {
			__xrtHttpServerRouterMatchClear(pMatch);
			return false;
		}
	}
	return true;
}



/* 收集一个路径的允许方法，常见集合完全使用栈空间。 */
static bool __xrtHttpServerRouterMethods(
	const xhttpserverrouter* pRouter,
	xstrview Path,
	xstrview* pLocal,
	xstrview** ppMethods,
	size_t* pCount
)
{
	xhttprouterstatus Status = xrtHttpRouterMethods(
		pRouter->Index,
		Path,
		pLocal,
		16u,
		pCount
	);

	*ppMethods = pLocal;
	if ( Status == XHTTP_ROUTER_MORE ) {
		if ( *pCount >=
			(SIZE_MAX / sizeof(**ppMethods)) ) {
			__xrtHttpServerRouterSetError(
				XERR_RANGE,
				XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
				"list-http-server-route-methods",
				"HTTP server route method list is too large",
				NULL
			);
			return false;
		}
		*ppMethods = (xstrview*)xrtMalloc(
			(*pCount + 1u) * sizeof(**ppMethods)
		);
		if ( *ppMethods == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"list-http-server-route-methods",
				"HTTP server route method list allocation failed"
			);
			return false;
		}
		Status = xrtHttpRouterMethods(
			pRouter->Index,
			Path,
			*ppMethods,
			*pCount,
			pCount
		);
	}
	if ( Status != XHTTP_ROUTER_MATCH ) {
		const xerror* pCause = xrtGetError();

		if ( *ppMethods != pLocal ) {
			xrtFree(*ppMethods);
			*ppMethods = pLocal;
		}
		__xrtHttpServerRouterSetError(
			XERR_INTERNAL,
			XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
			"list-http-server-route-methods",
			"HTTP server 405 path has no stable method set",
			pCause
		);
		return false;
	}
	return true;
}



/* 判断方法集合是否已经包含自动 OPTIONS。 */
static bool __xrtHttpServerRouterHasOptions(
	const xstrview* pMethods,
	size_t iCount
)
{
	size_t i;

	for ( i = 0; i < iCount; i++ ) {
		if ( __xrtHttpServerRouterTextEqual(
			pMethods[i], "OPTIONS"
		) ) {
			return true;
		}
	}
	return false;
}



/* 提交带 Allow 的 405 或自动 OPTIONS 响应。 */
static bool __xrtHttpServerRouterAllowReply(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	xstrview Path,
	bool bOptions
)
{
	xstrview LocalMethods[17];
	xstrview* pMethods;
	char LocalAllow[256];
	char* pAllow = LocalAllow;
	xhttpreply* pReply = NULL;
	size_t iCount = 0;
	size_t iAllow;
	bool bAddOptions;
	bool bSuccess = false;

	if ( !__xrtHttpServerRouterMethods(
		pRouter, Path, LocalMethods, &pMethods, &iCount
	) ) {
		return false;
	}
	bAddOptions = !__xrtHttpServerRouterHasOptions(
		pMethods, iCount
	);
	if ( bAddOptions ) {
		pMethods[iCount++] = XRT_STR_LITERAL("OPTIONS");
	}
	if ( !xrtHttpTokenListWrite(
		pMethods, iCount, NULL, 0, &iAllow
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_INTERNAL
			),
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			"format-http-allow",
			"HTTP Allow field could not be measured",
			pCause
		);
		goto Cleanup;
	}
	if ( iAllow > sizeof(LocalAllow) ) {
		pAllow = (char*)xrtMalloc(iAllow);
		if ( pAllow == NULL ) {
			__xrtHttpServerRouterWrapError(
				XERR_MEMORY,
				XHTTP_SERVER_ROUTER_ERROR_MEMORY,
				"format-http-allow",
				"HTTP Allow field allocation failed"
			);
			goto Cleanup;
		}
	}
	if ( !xrtHttpTokenListWrite(
		pMethods, iCount, pAllow, iAllow, &iAllow
	) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			"format-http-allow",
			"HTTP Allow field could not be written",
			pCause
		);
		goto Cleanup;
	}
	pReply = xrtHttpReplyCreate(
		bOptions ? XHTTP_STATUS_NO_CONTENT :
		XHTTP_STATUS_METHOD_NOT_ALLOWED
	);
	if ( (pReply == NULL) || !xrtHttpReplyAddHeader(
		pReply,
		XRT_STR_LITERAL("Allow"),
		(xstrview){ pAllow, iAllow }
	) || (!bOptions && !xrtHttpReplySetBytes(
		pReply,
		XRT_BYTES_LITERAL("Method Not Allowed"),
		XRT_STR_LITERAL("text/plain; charset=utf-8")
	)) || (xrtHttpConnRespond(
		pConnection, pReply
	) != XNET_RESULT_OK) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerRouterSetError(
			__xrtHttpServerRouterCauseKind(
				pCause, XERR_IO
			),
			XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
			bOptions ? "respond-http-options" :
				"respond-http-method-not-allowed",
			"HTTP server route default response failed",
			pCause
		);
		goto Cleanup;
	}
	bSuccess = true;

Cleanup:
	xrtHttpReplyDestroy(pReply);
	if ( pAllow != LocalAllow ) {
		xrtFree(pAllow);
	}
	if ( pMethods != LocalMethods ) {
		xrtFree(pMethods);
	}
	return bSuccess;
}



/* 提交 Router 未命中、方法拒绝或自动 OPTIONS 的默认响应。 */
bool __xrtHttpServerRouterDefault(
	const xhttpserverrouter* pRouter,
	xhttpconn* pConnection,
	const xrt_http_server_route_match* pMatch
)
{
	if ( pMatch->Status == XHTTP_ROUTER_NOT_FOUND ) {
		if ( xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_NOT_FOUND,
			XRT_STR_LITERAL("text/plain; charset=utf-8"),
			XRT_BYTES_LITERAL("Not Found")
		) == XNET_RESULT_OK ) {
			return true;
		}
		{
			const xerror* pCause = xrtGetError();

			__xrtHttpServerRouterSetError(
				__xrtHttpServerRouterCauseKind(
					pCause, XERR_IO
				),
				XHTTP_SERVER_ROUTER_ERROR_RESPONSE,
				"respond-http-not-found",
				"HTTP server route 404 response failed",
				pCause
			);
		}
		return false;
	}
	if ( pMatch->Status == XHTTP_ROUTER_METHOD_NOT_ALLOWED ) {
		return __xrtHttpServerRouterAllowReply(
			pRouter,
			pConnection,
			pMatch->Path,
			__xrtHttpServerRouterTextEqual(
				pMatch->Method, "OPTIONS"
			)
		);
	}
	__xrtHttpServerRouterSetError(
		XERR_INTERNAL,
		XHTTP_SERVER_ROUTER_ERROR_INTERNAL,
		"respond-http-route-default",
		"HTTP server route default received an invalid status",
		NULL
	);
	return false;
}



/* 对完整请求执行高层回调或标准默认响应。 */
XHTTP_SERVER_API xhttprouterstatus xrtHttpServerRouterDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest
)
{
	xrt_http_server_route_match Match;
	xhttprouterstatus Status;

	if ( (pServer == NULL) || (pConnection == NULL) ) {
		__xrtHttpServerRouterSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_ROUTER_ERROR_ARGUMENT,
			"dispatch-http-server-route",
			"HTTP server or connection is null",
			NULL
		);
		return XHTTP_ROUTER_ERROR;
	}
	if ( !__xrtHttpServerRouterMatch(
		pRouter, pRequest, &Match
	) ) {
		return XHTTP_ROUTER_ERROR;
	}
	Status = Match.Status;
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( !__xrtHttpServerMiddlewareDispatch(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			&Match,
			NULL
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, NULL
			);
			Status = XHTTP_ROUTER_ERROR;
		}
	#else
		if ( !__xrtHttpServerRouterDispatchTerminal(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			&Match,
			NULL
		) ) {
			Status = XHTTP_ROUTER_ERROR;
		}
	#endif
	if ( (Status != XHTTP_ROUTER_ERROR) &&
		(Status == XHTTP_ROUTER_METHOD_NOT_ALLOWED) &&
		__xrtHttpServerRouterTextEqual(
			Match.Method, "OPTIONS"
		) ) {
		Status = XHTTP_ROUTER_MATCH;
	}
	__xrtHttpServerRouterMatchClear(&Match);
	return Status;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_router_runtime.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)



#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER)

/* 转发 Connection 打开事件。 */
static void __xrtHttpServerRouterOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Open != NULL ) {
		pRuntime->Events.Open(
			pServer,
			pConnection,
			pRuntime->Events.Data
		);
	}
}



/* 报告路由内部失败并尽力提交固定 500，二次失败时异常关闭。 */
void __xrtHttpServerRouterDispatchFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverevents* pEvents
)
{
	xerror* pError = xrtErrorRef(xrtGetError());

	if ( (pError != NULL) &&
		(pEvents != NULL) &&
		(pEvents->Error != NULL) ) {
		pEvents->Error(
			pServer,
			pConnection,
			pError,
			pEvents->Data
		);
	}
	if ( xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_INTERNAL_SERVER_ERROR,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Internal Server Error")
	) != XNET_RESULT_OK ) {
		xrtClearError();
		(void)xrtHttpConnAbort(pConnection);
	}
	xrtErrorFree(pError);
}



/* 执行匹配路由、调用方回退或 Router 标准终端响应。 */
bool __xrtHttpServerRouterDispatchTerminal(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
)
{
	if ( pMatch->Status == XHTTP_ROUTER_MATCH ) {
		pMatch->Entry->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pMatch->Params,
			pMatch->Count,
			pMatch->Entry->Events.Data
		);
		return true;
	}
	if ( (pEvents != NULL) &&
		(pEvents->Request != NULL) ) {
		pEvents->Request(
			pServer,
			pConnection,
			pRequest,
			pEvents->Data
		);
		return true;
	}
	return __xrtHttpServerRouterDefault(
		pRouter, pConnection, pMatch
	);
}



/* 对未命中路由选择用户回退策略或立即提交标准响应并拒绝正文。 */
static xhttpserverbodypolicy __xrtHttpServerRouterMissingHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match* pMatch
)
{
	if ( pEvents->Request != NULL ) {
		if ( pEvents->Headers != NULL ) {
			return pEvents->Headers(
				pServer,
				pConnection,
				pRequest,
				pEvents->Data
			);
		}
		return XHTTP_SERVER_BODY_BUFFER;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( pRouter->MiddlewareCount != 0 ) {
			return XHTTP_SERVER_BODY_DISCARD;
		}
	#endif
	if ( !__xrtHttpServerRouterDefault(
		pRouter, pConnection, pMatch
	) ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, pEvents
		);
	}
	return XHTTP_SERVER_BODY_REJECT;
}



/* 在 Header 完整后使用指定 Router 选择正文策略。 */
xhttpserverbodypolicy __xrtHttpServerRouterDispatchHeaders(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	xrt_http_server_route_match** ppMatch
)
{
	xrt_http_server_route_match Match;
	xhttpserverbodypolicy Policy;
	xrt_http_server_route_match* pStored = NULL;

	if ( ppMatch != NULL ) {
		*ppMatch = NULL;
	}

	if ( !__xrtHttpServerRouterMatch(
		pRouter, pRequest, &Match
	) ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, pEvents
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( Match.Status != XHTTP_ROUTER_MATCH ) {
		Policy = __xrtHttpServerRouterMissingHeaders(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pEvents,
			&Match
		);
	} else if ( Match.Entry->Events.Headers != NULL ) {
		Policy = Match.Entry->Events.Headers(
			pServer,
			pConnection,
			pRequest,
			Match.Params,
			Match.Count,
			Match.Entry->Events.Data
		);
	} else if ( pEvents->Headers != NULL ) {
		Policy = pEvents->Headers(
			pServer,
			pConnection,
			pRequest,
			pEvents->Data
		);
	} else {
		Policy = XHTTP_SERVER_BODY_BUFFER;
	}
	if ( Policy == XHTTP_SERVER_BODY_STREAM ) {
		pStored = __xrtHttpServerRouterMatchTake(&Match);
		if ( pStored == NULL ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
			Policy = XHTTP_SERVER_BODY_REJECT;
		}
	}
	__xrtHttpServerRouterMatchClear(&Match);
	if ( ppMatch != NULL ) {
		*ppMatch = pStored;
	} else {
		__xrtHttpServerRouterMatchDestroy(pStored);
	}
	return Policy;
}



/* 把流式正文片段交给指定 Router，未命中时使用原始 Body 事件。 */
bool __xrtHttpServerRouterDispatchBody(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pCached
)
{
	xrt_http_server_route_match Match;
	const xrt_http_server_route_match* pMatch = pCached;
	bool bLocal = false;
	bool bResult;

	if ( pMatch == NULL ) {
		if ( !__xrtHttpServerRouterMatch(
			pRouter, pRequest, &Match
		) ) {
			return false;
		}
		pMatch = &Match;
		bLocal = true;
	}
	if ( (pMatch->Status == XHTTP_ROUTER_MATCH) &&
		(pMatch->Entry->Events.Body != NULL) ) {
		bResult = pMatch->Entry->Events.Body(
			pServer,
			pConnection,
			pRequest,
			pMatch->Params,
			pMatch->Count,
			Data,
			pMatch->Entry->Events.Data
		);
	} else if ( pEvents->Body != NULL ) {
		bResult = pEvents->Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pEvents->Data
		);
	} else {
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"stream-http-server-route-body",
			"HTTP route selected streaming without a Body handler",
			NULL
		);
		bResult = false;
	}
	if ( bLocal ) {
		__xrtHttpServerRouterMatchClear(&Match);
	}
	return bResult;
}



/* 使用指定 Router 分发完整请求；原始 Request 事件仅处理未命中路由。 */
void __xrtHttpServerRouterDispatchRequest(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttpserverevents* pEvents,
	const xrt_http_server_route_match* pCached
)
{
	xrt_http_server_route_match Match;
	const xrt_http_server_route_match* pMatch = pCached;
	bool bLocal = false;

	if ( pMatch == NULL ) {
		if ( !__xrtHttpServerRouterMatch(
			pRouter, pRequest, &Match
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
			return;
		}
		pMatch = &Match;
		bLocal = true;
	}
	#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)
		if ( !__xrtHttpServerMiddlewareDispatch(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pMatch,
			pEvents
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
		}
	#else
		if ( !__xrtHttpServerRouterDispatchTerminal(
			pRouter,
			pServer,
			pConnection,
			pRequest,
			pMatch,
			pEvents
		) ) {
			__xrtHttpServerRouterDispatchFail(
				pServer, pConnection, pEvents
			);
		}
	#endif
	if ( bLocal ) {
		__xrtHttpServerRouterMatchClear(&Match);
	}
}



/* 固定 Router 适配器把 Header 阶段转发给共用分发核心。 */
static xhttpserverbodypolicy __xrtHttpServerRouterHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	xrt_http_server_route_match* pMatch = NULL;
	xhttpserverbodypolicy Policy;

	Policy = __xrtHttpServerRouterDispatchHeaders(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		&pMatch
	);
	if ( (pMatch != NULL) && !__xrtHttpConnAdapterSet(
		pConnection,
		pMatch,
		__xrtHttpServerRouterMatchDestroy
	) ) {
		__xrtHttpServerRouterMatchDestroy(pMatch);
		__xrtHttpServerRouterSetError(
			XERR_STATE,
			XHTTP_SERVER_ROUTER_ERROR_STATE,
			"cache-http-server-route",
			"HTTP server connection already has a route cache",
			NULL
		);
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	return Policy;
}



/* 固定 Router 适配器把正文片段转发给共用分发核心。 */
static bool __xrtHttpServerRouterBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	return __xrtHttpServerRouterDispatchBody(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		Data,
		&pRuntime->Events,
		(const xrt_http_server_route_match*)
			__xrtHttpConnAdapterData(pConnection)
	);
}



/* 固定 Router 适配器把完整请求转发给共用分发核心。 */
static void __xrtHttpServerRouterRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	xrt_http_server_route_match* pMatch =
		(xrt_http_server_route_match*)
		__xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerRouterDispatchRequest(
		pRuntime->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		pMatch
	);
	__xrtHttpServerRouterMatchDestroy(pMatch);
}



/* 转发底层 Server 错误事件。 */
static void __xrtHttpServerRouterError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Error != NULL ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 转发 Connection 唯一关闭事件。 */
static void __xrtHttpServerRouterClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;
	ptr pMatch = __xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerRouterMatchDestroy(pMatch);

	if ( pRuntime->Events.Close != NULL ) {
		pRuntime->Events.Close(
			pServer,
			pConnection,
			Result,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 在底层 Server 发布最终关闭后转发 Shutdown 并释放适配器资产。 */
static void __xrtHttpServerRouterShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	xrt_http_server_router_runtime* pRuntime =
		(xrt_http_server_router_runtime*)pData;

	if ( pRuntime->Events.Shutdown != NULL ) {
		pRuntime->Events.Shutdown(
			pServer,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerRouterRuntimeDestroy(pRuntime);
}



/* 释放适配器内存及其持有的 Router 引用。 */
void __xrtHttpServerRouterRuntimeDestroy(
	xrt_http_server_router_runtime* pRuntime
)
{
	xhttpserverrouter* pRouter;

	if ( pRuntime == NULL ) {
		return;
	}
	pRouter = pRuntime->Router;
	xrtFree(pRuntime);
	xrtHttpServerRouterDestroy(pRouter);
}



/* 建立持有 Router 的完整事件适配器。 */
xrt_http_server_router_runtime* __xrtHttpServerRouterRuntimeCreate(
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
)
{
	xhttpserverevents Events = { 0 };
	xrt_http_server_router_runtime* pRuntime;
	bool bArguments = (pRouter == NULL) ||
		!xrtMemRangeValid(pOutput, sizeof(Events)) ||
		((pEvents != NULL) &&
		 !xrtMemRangeValid(pEvents, sizeof(Events)));

	if ( bArguments || !xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerRouterSetError(
			bArguments ? XERR_ARGUMENT : XERR_STATE,
			bArguments ?
				XHTTP_SERVER_ROUTER_ERROR_ARGUMENT :
				XHTTP_SERVER_ROUTER_ERROR_STATE,
			"start-http-server-router",
			bArguments ?
				"HTTP server router event ranges are invalid" :
				"HTTP server router must be frozen",
			NULL
		);
		return NULL;
	}
	if ( pEvents != NULL ) {
		memcpy(&Events, (const void*)pEvents, sizeof(Events));
	}
	pRuntime = (xrt_http_server_router_runtime*)xrtCalloc(
		1, sizeof(*pRuntime)
	);
	if ( pRuntime == NULL ) {
		__xrtHttpServerRouterWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_ROUTER_ERROR_MEMORY,
			"start-http-server-router",
			"HTTP server router runtime allocation failed"
		);
		return NULL;
	}
	pRuntime->Router = xrtHttpServerRouterRef(pRouter);
	if ( pRuntime->Router == NULL ) {
		xrtFree(pRuntime);
		return NULL;
	}
	pRuntime->Events = Events;
	xrtHttpServerEventsInit(pOutput);
	pOutput->Open = __xrtHttpServerRouterOpen;
	pOutput->Headers = __xrtHttpServerRouterHeaders;
	pOutput->Body = __xrtHttpServerRouterBody;
	pOutput->Request = __xrtHttpServerRouterRequest;
	pOutput->Close = __xrtHttpServerRouterClose;
	pOutput->Error = __xrtHttpServerRouterError;
	pOutput->Shutdown = __xrtHttpServerRouterShutdown;
	pOutput->Data = pRuntime;
	return pRuntime;
}



/* 启动明文 Router Server；启动失败时同步释放适配器引用。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerRouterStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_router_runtime* pRuntime =
		__xrtHttpServerRouterRuntimeCreate(
			pRouter, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStart(
		pEngine, pConfig, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerRouterRuntimeDestroy(pRuntime);
		__xrtHttpServerRouterWrapError(
			XERR_IO,
			XHTTP_SERVER_ROUTER_ERROR_START,
			"start-http-server-router",
			"HTTP server router listener start failed"
		);
	}
	return pServer;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_middleware.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)



#if defined(XHTTP_FEATURE_HTTP_SERVER_MIDDLEWARE)

/* 一次请求分派持有借用参数和不可变 Router，不产生每请求堆分配。 */
typedef struct xrt_http_server_middleware_dispatch {
	const xhttpserverrouter* Router;
	xhttpserver* Server;
	xhttpconn* Connection;
	const xhttpserverrequest* Request;
	const xrt_http_server_route_match* Match;
	const xhttpserverevents* Events;
	bool Failed;
} xrt_http_server_middleware_dispatch;



/* Next 是严格栈内令牌，记录下一层索引和单次调用状态。 */
struct xhttpservernext {
	xrt_http_server_middleware_dispatch* Dispatch;
	size_t Index;
	bool Called;
	bool Active;
};



/* 发布稳定的中间件错误并保留可选原因链。 */
static void __xrtHttpServerMiddlewareSetError(
	xerrkind Kind,
	xhttpservermiddlewareerror Code,
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
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.http.server.middleware";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 从指定索引执行中间件；末端进入唯一 Router 终端分派。 */
static bool __xrtHttpServerMiddlewareRun(
	xrt_http_server_middleware_dispatch* pDispatch,
	size_t iIndex
)
{
	const xrt_http_server_middleware_entry* pEntry;
	xhttpservernext Next;
	bool bResult;

	if ( iIndex == pDispatch->Router->MiddlewareCount ) {
		return __xrtHttpServerRouterDispatchTerminal(
			pDispatch->Router,
			pDispatch->Server,
			pDispatch->Connection,
			pDispatch->Request,
			pDispatch->Match,
			pDispatch->Events
		);
	}
	pEntry = &pDispatch->Router->Middleware[iIndex];
	memset(&Next, 0, sizeof(Next));
	Next.Dispatch = pDispatch;
	Next.Index = iIndex + 1u;
	Next.Active = true;
	bResult = pEntry->Handle(
		pDispatch->Server,
		pDispatch->Connection,
		pDispatch->Request,
		pDispatch->Match->Count != 0 ?
			pDispatch->Match->Params : NULL,
		pDispatch->Match->Count,
		&Next,
		pEntry->Data
	);
	Next.Active = false;
	if ( pDispatch->Failed ) {
		return false;
	}
	if ( !bResult ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMiddlewareSetError(
			xrtErrorKind(pCause) != XERR_NONE ?
				xrtErrorKind(pCause) : XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_CALLBACK,
			"dispatch-http-server-middleware",
			"HTTP server middleware callback failed",
			pCause
		);
		pDispatch->Failed = true;
		return false;
	}
	return true;
}



/* 继续下一层一次；重复、过期或空 Next 会固定当前请求失败。 */
XHTTP_SERVER_API bool xrtHttpServerNext(xhttpservernext* pNext)
{
	if ( (pNext == NULL) || !pNext->Active ||
		(pNext->Dispatch == NULL) || pNext->Called ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT,
			"continue-http-server-middleware",
			"HTTP server middleware Next is null, expired or already called",
			NULL
		);
		if ( (pNext != NULL) &&
			(pNext->Dispatch != NULL) ) {
			pNext->Dispatch->Failed = true;
		}
		return false;
	}
	pNext->Called = true;
	if ( !__xrtHttpServerMiddlewareRun(
		pNext->Dispatch, pNext->Index
	) ) {
		pNext->Dispatch->Failed = true;
		return false;
	}
	return true;
}



/* 为新记录增长回调表，最大数量与 Router 的显式路由限额一致。 */
static bool __xrtHttpServerMiddlewareReserve(
	xhttpserverrouter* pRouter
)
{
	size_t iRequired = pRouter->MiddlewareCount + 1u;
	size_t iCapacity;
	xrt_http_server_middleware_entry* pEntries;

	if ( iRequired > pRouter->MaxRoutes ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_RANGE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_LIMIT,
			"register-http-server-middleware",
			"HTTP server middleware count exceeds the Router limit",
			NULL
		);
		return false;
	}
	if ( iRequired <= pRouter->MiddlewareCapacity ) {
		return true;
	}
	iCapacity = pRouter->MiddlewareCapacity != 0 ?
		pRouter->MiddlewareCapacity :
		(pRouter->MaxRoutes < 4u ? pRouter->MaxRoutes : 4u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity > (pRouter->MaxRoutes / 2u) ?
			pRouter->MaxRoutes : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pEntries = (xrt_http_server_middleware_entry*)xrtRealloc(
		pRouter->Middleware,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMiddlewareSetError(
			XERR_MEMORY,
			XHTTP_SERVER_MIDDLEWARE_ERROR_MEMORY,
			"register-http-server-middleware",
			"HTTP server middleware table allocation failed",
			pCause
		);
		return false;
	}
	pRouter->Middleware = pEntries;
	pRouter->MiddlewareCapacity = iCapacity;
	return true;
}



/* 注册一层可选拥有用户数据的同步中间件。 */
XHTTP_SERVER_API bool xrtHttpServerUseOwned(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData,
	xhttpserverrouterreleaseproc pRelease
)
{
	xrt_http_server_middleware_entry* pEntry;

	if ( (pRouter == NULL) || (pMiddleware == NULL) ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MIDDLEWARE_ERROR_ARGUMENT,
			"register-http-server-middleware",
			"HTTP server Router or middleware callback is null",
			NULL
		);
		return false;
	}
	if ( xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMiddlewareSetError(
			XERR_STATE,
			XHTTP_SERVER_MIDDLEWARE_ERROR_STATE,
			"register-http-server-middleware",
			"HTTP server Router is already frozen",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMiddlewareReserve(pRouter) ) {
		return false;
	}
	pEntry = &pRouter->Middleware[pRouter->MiddlewareCount];
	pEntry->Handle = pMiddleware;
	pEntry->Release = pRelease;
	pEntry->Data = pData;
	pRouter->MiddlewareCount++;
	return true;
}



/* 注册由调用方管理用户数据的常用同步中间件。 */
XHTTP_SERVER_API bool xrtHttpServerUse(
	xhttpserverrouter* pRouter,
	xhttpservermiddlewareproc pMiddleware,
	ptr pData
)
{
	return xrtHttpServerUseOwned(
		pRouter, pMiddleware, pData, NULL
	);
}



/* 返回冻结前后都稳定可读的中间件数量。 */
XHTTP_SERVER_API size_t xrtHttpServerMiddlewareCount(
	const xhttpserverrouter* pRouter
)
{
	return pRouter != NULL ?
		pRouter->MiddlewareCount : 0;
}



/* 从最外层开始执行一次完整请求中间件链。 */
bool __xrtHttpServerMiddlewareDispatch(
	const xhttpserverrouter* pRouter,
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xrt_http_server_route_match* pMatch,
	const xhttpserverevents* pEvents
)
{
	xrt_http_server_middleware_dispatch Dispatch;

	memset(&Dispatch, 0, sizeof(Dispatch));
	Dispatch.Router = pRouter;
	Dispatch.Server = pServer;
	Dispatch.Connection = pConnection;
	Dispatch.Request = pRequest;
	Dispatch.Match = pMatch;
	Dispatch.Events = pEvents;
	return __xrtHttpServerMiddlewareRun(&Dispatch, 0u);
}



/* 逆序释放拥有型用户数据，再释放中间件记录数组。 */
void __xrtHttpServerMiddlewareClear(xhttpserverrouter* pRouter)
{
	size_t i;

	if ( pRouter == NULL ) {
		return;
	}
	for ( i = pRouter->MiddlewareCount; i > 0; i-- ) {
		xrt_http_server_middleware_entry* pEntry =
			&pRouter->Middleware[i - 1u];

		if ( pEntry->Release != NULL ) {
			pEntry->Release(pEntry->Data);
		}
	}
	xrtFree(pRouter->Middleware);
	pRouter->Middleware = NULL;
	pRouter->MiddlewareCount = 0;
	pRouter->MiddlewareCapacity = 0;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_mux.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)

/* 原因缺失或没有类别时使用调用点给出的稳定默认类别。 */
static xerrkind __xrtHttpServerMuxCauseKind(
	const xerror* pCause,
	xerrkind Default
)
{
	xerrkind Kind = xrtErrorKind(pCause);

	return Kind != XERR_NONE ? Kind : Default;
}



/* 设置包含稳定域、代码、操作和原因链的 Mux 错误。 */
void __xrtHttpServerMuxSetError(
	xerrkind Kind,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind != XERR_NONE ? Kind :
		__xrtHttpServerMuxCauseKind(pCause, XERR_INTERNAL);
	Desc.Code = (int32)Code;
	Desc.SystemCode = pCause != NULL ?
		xrtErrorSystemCode(pCause) : 0;
	Desc.Domain = "xrt.http.server.mux";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}



/* 把当前错误包装为稳定 Mux 错误。 */
void __xrtHttpServerMuxWrapError(
	xerrkind Default,
	xhttpservermuxerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	const xerror* pCause = xrtGetError();

	__xrtHttpServerMuxSetError(
		__xrtHttpServerMuxCauseKind(pCause, Default),
		Code,
		sOperation,
		sMessage,
		pCause
	);
}



/* 把一个 ASCII 字节折叠为 Host 比较使用的小写形式。 */
static uint8 __xrtHttpServerMuxFold(uint8 Value)
{
	return (Value >= (uint8)'A') && (Value <= (uint8)'Z') ?
		(uint8)(Value + ((uint8)'a' - (uint8)'A')) : Value;
}



/* 按 ASCII 不区分大小写顺序比较两个 Host。 */
static int __xrtHttpServerMuxCompare(
	xstrview Left,
	xstrview Right
)
{
	size_t iLimit = Left.Size < Right.Size ?
		Left.Size : Right.Size;
	size_t i;

	for ( i = 0; i < iLimit; i++ ) {
		uint8 A = __xrtHttpServerMuxFold(
			(uint8)Left.Data[i]
		);
		uint8 B = __xrtHttpServerMuxFold(
			(uint8)Right.Data[i]
		);

		if ( A != B ) {
			return A < B ? -1 : 1;
		}
	}
	return Left.Size == Right.Size ? 0 :
		(Left.Size < Right.Size ? -1 : 1);
}



/* 在有序 Host 表中返回命中位置或稳定插入位置。 */
static size_t __xrtHttpServerMuxSearch(
	const xhttpservermux* pMux,
	xstrview Host,
	bool* pFound
)
{
	size_t iLeft = 0;
	size_t iRight = pMux->Count;

	while ( iLeft < iRight ) {
		size_t iMiddle = iLeft + ((iRight - iLeft) / 2u);
		const xrt_http_server_mux_entry* pEntry =
			&pMux->Entries[iMiddle];
		int iOrder = __xrtHttpServerMuxCompare(
			Host,
			(xstrview){ pEntry->Host, pEntry->Size }
		);

		if ( iOrder == 0 ) {
			*pFound = true;
			return iMiddle;
		}
		if ( iOrder < 0 ) {
			iRight = iMiddle;
		} else {
			iLeft = iMiddle + 1u;
		}
	}
	*pFound = false;
	return iLeft;
}



/* 解析一个无端口 HTTP Host，并返回借用的规范 Host 部分。 */
static bool __xrtHttpServerMuxParseHost(
	xstrview Text,
	xstrview* pHost,
	cstr sOperation
)
{
	xhttpauthority Parsed;

	if ( (pHost == NULL) ||
		((Text.Data == NULL) && (Text.Size != 0)) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		(Text.Size == 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			sOperation,
			"HTTP server mux Host is empty or invalid",
			NULL
		);
		return false;
	}
	if ( !xrtHttpHostParse(Text, &Parsed) ||
		(Parsed.Host.Size == 0) ||
		((Parsed.Flags & XHTTP_AUTHORITY_HAS_PORT) != 0) ) {
		const xerror* pCause = xrtGetError();

		__xrtHttpServerMuxSetError(
			XERR_PROTOCOL,
			XHTTP_SERVER_MUX_ERROR_HOST,
			sOperation,
			"HTTP server mux Host must be valid and contain no port",
			pCause
		);
		return false;
	}
	*pHost = Parsed.Host;
	return true;
}



/* 分配一份小写 Host 副本，避免热路径重复折叠存储文本。 */
static str __xrtHttpServerMuxHostCopy(xstrview Host)
{
	str sHost;
	size_t i;

	if ( Host.Size == SIZE_MAX ) {
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux Host size overflows storage",
			NULL
		);
		return NULL;
	}
	sHost = (str)xrtMalloc(Host.Size + 1u);
	if ( sHost == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"set-http-server-mux-host",
			"HTTP server mux Host allocation failed"
		);
		return NULL;
	}
	for ( i = 0; i < Host.Size; i++ ) {
		sHost[i] = (char)__xrtHttpServerMuxFold(
			(uint8)Host.Data[i]
		);
	}
	sHost[Host.Size] = '\0';
	return sHost;
}



/* 为一个新 Host 记录扩展有序表容量。 */
static bool __xrtHttpServerMuxReserve(xhttpservermux* pMux)
{
	size_t iRequired = pMux->Count + 1u;
	size_t iCapacity;
	xrt_http_server_mux_entry* pEntries;

	if ( iRequired <= pMux->Capacity ) {
		return true;
	}
	iCapacity = pMux->Capacity != 0 ?
		pMux->Capacity :
		(pMux->Config.MaxHosts < 8u ?
		 pMux->Config.MaxHosts : 8u);
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity >
			(pMux->Config.MaxHosts / 2u) ?
			pMux->Config.MaxHosts : iCapacity * 2u;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	if ( iCapacity >
		(SIZE_MAX / sizeof(*pEntries)) ) {
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux table size overflows storage",
			NULL
		);
		return false;
	}
	pEntries = (xrt_http_server_mux_entry*)xrtRealloc(
		pMux->Entries,
		iCapacity * sizeof(*pEntries)
	);
	if ( pEntries == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"set-http-server-mux-host",
			"HTTP server mux table allocation failed"
		);
		return false;
	}
	pMux->Entries = pEntries;
	pMux->Capacity = iCapacity;
	return true;
}



/* 初始化有限但足以覆盖常见多租户服务的默认配置。 */
XHTTP_SERVER_API void xrtHttpServerMuxConfigInit(
	xhttpservermuxconfig* pConfig
)
{
	const xhttpservermuxconfig Config = {
		256u,
		64u * 1024u
	};

	if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"init-http-server-mux-config",
			"HTTP server mux config range is invalid",
			NULL
		);
		return;
	}
	memcpy((void*)pConfig, &Config, sizeof(Config));
}



/* 创建空 Mux 并初始化读写锁。 */
XHTTP_SERVER_API xhttpservermux* xrtHttpServerMuxCreate(
	const xhttpservermuxconfig* pConfig
)
{
	xhttpservermuxconfig Config;
	xhttpservermux* pMux;

	xrtHttpServerMuxConfigInit(&Config);
	if ( pConfig != NULL ) {
		if ( !xrtMemRangeValid(pConfig, sizeof(Config)) ) {
			__xrtHttpServerMuxSetError(
				XERR_ARGUMENT,
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
				"create-http-server-mux",
				"HTTP server mux config range is invalid",
				NULL
			);
			return NULL;
		}
		memcpy(&Config, (const void*)pConfig, sizeof(Config));
	}
	if ( (Config.MaxHosts == 0) ||
		(Config.MaxHostBytes == 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"create-http-server-mux",
			"HTTP server mux limits must be nonzero",
			NULL
		);
		return NULL;
	}
	pMux = (xhttpservermux*)xrtCalloc(1, sizeof(*pMux));
	if ( pMux == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"create-http-server-mux",
			"HTTP server mux allocation failed"
		);
		return NULL;
	}
	if ( !xrtRWLockInit(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"create-http-server-mux",
			"HTTP server mux rwlock initialization failed"
		);
		xrtFree(pMux);
		return NULL;
	}
	pMux->References = 1;
	pMux->Config = Config;
	return pMux;
}



/* 增加 Mux 引用并拒绝空或已经释放的对象。 */
XHTTP_SERVER_API xhttpservermux* xrtHttpServerMuxRef(
	xhttpservermux* pMux
)
{
	if ( (pMux == NULL) ||
		(xrtRefRetain(&pMux->References) < 0) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"retain-http-server-mux",
			"HTTP server mux is null or released",
			NULL
		);
		return NULL;
	}
	return pMux;
}



/* 释放最后一个引用持有的 Host、Router 和同步原语。 */
XHTTP_SERVER_API void xrtHttpServerMuxDestroy(
	xhttpservermux* pMux
)
{
	size_t i;

	if ( (pMux == NULL) ||
		(xrtRefRelease(&pMux->References) != 0) ) {
		return;
	}
	for ( i = 0; i < pMux->Count; i++ ) {
		xrtFree(pMux->Entries[i].Host);
		xrtHttpServerRouterDestroy(
			pMux->Entries[i].Router
		);
	}
	xrtHttpServerRouterDestroy(pMux->Default);
	xrtFree(pMux->Entries);
	(void)xrtRWLockUnit(&pMux->Lock);
	memset(pMux, 0, sizeof(*pMux));
	xrtFree(pMux);
}



/* 设置默认 Router，并把旧引用释放放到写锁之外。 */
XHTTP_SERVER_API bool xrtHttpServerMuxDefault(
	xhttpservermux* pMux,
	xhttpserverrouter* pRouter
)
{
	xhttpserverrouter* pHeld;
	xhttpserverrouter* pOld;

	if ( (pMux == NULL) || (pRouter == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMuxSetError(
			(pRouter != NULL) ? XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) ?
				XHTTP_SERVER_MUX_ERROR_STATE :
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"set-http-server-mux-default",
			"HTTP server mux and frozen default Router are required",
			NULL
		);
		return false;
	}
	pHeld = xrtHttpServerRouterRef(pRouter);
	if ( pHeld == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_STATE,
			"set-http-server-mux-default",
			"HTTP server mux default Router could not be retained"
		);
		return false;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		xrtHttpServerRouterDestroy(pHeld);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"set-http-server-mux-default",
			"HTTP server mux write lock failed"
		);
		return false;
	}
	pOld = pMux->Default;
	pMux->Default = pHeld;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	xrtHttpServerRouterDestroy(pOld);
	return true;
}



/* 设置精确 Host，并在同名项上原子替换 Router。 */
XHTTP_SERVER_API bool xrtHttpServerMuxHost(
	xhttpservermux* pMux,
	xstrview Text,
	xhttpserverrouter* pRouter
)
{
	xstrview Host;
	xhttpserverrouter* pHeld;
	xhttpserverrouter* pOld = NULL;
	str sHost;
	bool bFound;
	size_t iIndex;

	if ( (pMux == NULL) || (pRouter == NULL) ||
		!xrtHttpServerRouterFrozen(pRouter) ) {
		__xrtHttpServerMuxSetError(
			(pRouter != NULL) ? XERR_STATE : XERR_ARGUMENT,
			(pRouter != NULL) ?
				XHTTP_SERVER_MUX_ERROR_STATE :
				XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"set-http-server-mux-host",
			"HTTP server mux and frozen Host Router are required",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "set-http-server-mux-host"
	) ) {
		return false;
	}
	sHost = __xrtHttpServerMuxHostCopy(Host);
	if ( sHost == NULL ) {
		return false;
	}
	pHeld = xrtHttpServerRouterRef(pRouter);
	if ( pHeld == NULL ) {
		xrtFree(sHost);
		__xrtHttpServerMuxWrapError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_STATE,
			"set-http-server-mux-host",
			"HTTP server mux Host Router could not be retained"
		);
		return false;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"set-http-server-mux-host",
			"HTTP server mux write lock failed"
		);
		return false;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( bFound ) {
		pOld = pMux->Entries[iIndex].Router;
		pMux->Entries[iIndex].Router = pHeld;
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pOld);
		xrtFree(sHost);
		return true;
	}
	if ( (pMux->Count >= pMux->Config.MaxHosts) ||
		(Host.Size >
		 (pMux->Config.MaxHostBytes - pMux->HostBytes)) ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		__xrtHttpServerMuxSetError(
			XERR_RANGE,
			XHTTP_SERVER_MUX_ERROR_LIMIT,
			"set-http-server-mux-host",
			"HTTP server mux Host limits were exceeded",
			NULL
		);
		return false;
	}
	if ( !__xrtHttpServerMuxReserve(pMux) ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		xrtHttpServerRouterDestroy(pHeld);
		xrtFree(sHost);
		return false;
	}
	memmove(
		&pMux->Entries[iIndex + 1u],
		&pMux->Entries[iIndex],
		(pMux->Count - iIndex) * sizeof(*pMux->Entries)
	);
	pMux->Entries[iIndex].Host = sHost;
	pMux->Entries[iIndex].Size = Host.Size;
	pMux->Entries[iIndex].Router = pHeld;
	pMux->Count++;
	pMux->HostBytes += Host.Size;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	return true;
}



/* 移除精确 Host，并在锁外释放其文本和 Router。 */
XHTTP_SERVER_API xhttpservermuxstatus xrtHttpServerMuxRemove(
	xhttpservermux* pMux,
	xstrview Text
)
{
	xrt_http_server_mux_entry Removed;
	xstrview Host;
	bool bFound;
	size_t iIndex;

	if ( pMux == NULL ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"remove-http-server-mux-host",
			"HTTP server mux is null",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "remove-http-server-mux-host"
	) ) {
		return XHTTP_SERVER_MUX_ERROR;
	}
	if ( !xrtRWLockWrite(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"remove-http-server-mux-host",
			"HTTP server mux write lock failed"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( !bFound ) {
		(void)xrtRWLockWriteUnlock(&pMux->Lock);
		return XHTTP_SERVER_MUX_NOT_FOUND;
	}
	Removed = pMux->Entries[iIndex];
	memmove(
		&pMux->Entries[iIndex],
		&pMux->Entries[iIndex + 1u],
		(pMux->Count - iIndex - 1u) * sizeof(*pMux->Entries)
	);
	pMux->Count--;
	pMux->HostBytes -= Removed.Size;
	(void)xrtRWLockWriteUnlock(&pMux->Lock);
	xrtFree(Removed.Host);
	xrtHttpServerRouterDestroy(Removed.Router);
	return XHTTP_SERVER_MUX_HOST;
}



/* 在读锁内选择并保留精确 Host 或默认 Router。 */
xhttpservermuxstatus __xrtHttpServerMuxSelect(
	xhttpservermux* pMux,
	xstrview Host,
	xhttpserverrouter** ppRouter
)
{
	xhttpserverrouter* pRouter = NULL;
	xhttpservermuxstatus Status = XHTTP_SERVER_MUX_NOT_FOUND;
	bool bFound;
	size_t iIndex;

	if ( (pMux == NULL) ||
		!xrtMemRangeValid(ppRouter, sizeof(pRouter)) ||
		!xrtMemRangeValid(Host.Data, Host.Size) ||
		(Host.Size == 0) ||
		xrtMemRangesOverlap(
			ppRouter, sizeof(pRouter),
			pMux, sizeof(*pMux)
		) || xrtMemRangesOverlap(
			ppRouter, sizeof(pRouter),
			Host.Data, Host.Size
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"match-http-server-mux",
			"HTTP server mux match arguments are invalid",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy((void*)ppRouter, &pRouter, sizeof(pRouter));
	if ( !xrtRWLockRead(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"match-http-server-mux",
			"HTTP server mux read lock failed"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	iIndex = __xrtHttpServerMuxSearch(pMux, Host, &bFound);
	if ( bFound ) {
		pRouter = pMux->Entries[iIndex].Router;
		Status = XHTTP_SERVER_MUX_HOST;
	} else if ( pMux->Default != NULL ) {
		pRouter = pMux->Default;
		Status = XHTTP_SERVER_MUX_DEFAULT;
	}
	if ( pRouter != NULL ) {
		pRouter = xrtHttpServerRouterRef(pRouter);
	}
	(void)xrtRWLockReadUnlock(&pMux->Lock);
	if ( (Status != XHTTP_SERVER_MUX_NOT_FOUND) &&
		(pRouter == NULL) ) {
		__xrtHttpServerMuxWrapError(
			XERR_INTERNAL,
			XHTTP_SERVER_MUX_ERROR_INTERNAL,
			"match-http-server-mux",
			"HTTP server mux selected Router could not be retained"
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy((void*)ppRouter, &pRouter, sizeof(pRouter));
	return Status;
}



/* 解析公开 Host 后执行无分配 Mux 查找。 */
XHTTP_SERVER_API xhttpservermuxstatus xrtHttpServerMuxMatch(
	xhttpservermux* pMux,
	xstrview Text,
	xhttpserverrouter** ppRouter
)
{
	xstrview Host;
	xhttpserverrouter* pRouter = NULL;

	if ( (pMux == NULL) ||
		!xrtMemRangeValid(ppRouter, sizeof(pRouter)) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		xrtMemRangesOverlap(
			ppRouter, sizeof(pRouter),
			pMux, sizeof(*pMux)
		) || xrtMemRangesOverlap(
			ppRouter, sizeof(pRouter),
			Text.Data, Text.Size
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"match-http-server-mux",
			"HTTP server mux or output is null",
			NULL
		);
		return XHTTP_SERVER_MUX_ERROR;
	}
	memcpy((void*)ppRouter, &pRouter, sizeof(pRouter));
	if ( !__xrtHttpServerMuxParseHost(
		Text, &Host, "match-http-server-mux"
	) ) {
		return XHTTP_SERVER_MUX_ERROR;
	}
	return __xrtHttpServerMuxSelect(
		pMux, Host, ppRouter
	);
}



/* 复制受读锁保护的一致配置统计。 */
XHTTP_SERVER_API bool xrtHttpServerMuxStats(
	xhttpservermux* pMux,
	xhttpservermuxstats* pStats
)
{
	xhttpservermuxstats Stats = { 0 };

	if ( (pMux == NULL) ||
		!xrtMemRangeValid(pStats, sizeof(Stats)) ||
		xrtMemRangesOverlap(
			pStats, sizeof(Stats),
			pMux, sizeof(*pMux)
		) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"stat-http-server-mux",
			"HTTP server mux or stats output is null",
			NULL
		);
		return false;
	}
	if ( !xrtRWLockRead(&pMux->Lock) ) {
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_LOCK,
			"stat-http-server-mux",
			"HTTP server mux read lock failed"
		);
		return false;
	}
	Stats.Hosts = pMux->Count;
	Stats.HostBytes = pMux->HostBytes;
	Stats.HasDefault = pMux->Default != NULL;
	(void)xrtRWLockReadUnlock(&pMux->Lock);
	memcpy((void*)pStats, &Stats, sizeof(Stats));
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_mux_runtime.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)



#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX)

/* 释放一个 Connection 固定持有的当前请求 Router。 */
static void __xrtHttpServerMuxConnectionDestroy(ptr pData)
{
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)pData;

	if ( pContext == NULL ) {
		return;
	}
	__xrtHttpServerRouterMatchDestroy(pContext->Match);
	xrtHttpServerRouterDestroy(pContext->Router);
	xrtFree(pContext);
}



/* 报告 Mux 适配错误并异常关闭尚无可响应请求的连接。 */
static void __xrtHttpServerMuxOpenFail(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xrt_http_server_mux_runtime* pRuntime
)
{
	xerror* pError = xrtErrorRef(xrtGetError());

	if ( (pError != NULL) &&
		(pRuntime->Events.Error != NULL) ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
	xrtErrorFree(pError);
	(void)xrtHttpConnAbort(pConnection);
}



/* 为新连接安装 O(1) Mux 上下文，再转发用户 Open 事件。 */
static void __xrtHttpServerMuxOpen(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)xrtCalloc(
			1, sizeof(*pContext)
		);

	if ( pContext == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"open-http-server-mux-connection",
			"HTTP server mux connection context allocation failed"
		);
		__xrtHttpServerMuxOpenFail(
			pServer, pConnection, pRuntime
		);
		return;
	}
	if ( !__xrtHttpConnAdapterSet(
		pConnection,
		pContext,
		__xrtHttpServerMuxConnectionDestroy
	) ) {
		__xrtHttpServerMuxConnectionDestroy(pContext);
		__xrtHttpServerMuxSetError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_CONTEXT,
			"open-http-server-mux-connection",
			"HTTP server connection already has an adapter context",
			NULL
		);
		__xrtHttpServerMuxOpenFail(
			pServer, pConnection, pRuntime
		);
		return;
	}
	if ( pRuntime->Events.Open != NULL ) {
		pRuntime->Events.Open(
			pServer,
			pConnection,
			pRuntime->Events.Data
		);
	}
}



/* 返回当前 Connection 的 Mux 上下文并建立稳定错误。 */
static xrt_http_server_mux_connection* __xrtHttpServerMuxConnection(
	xhttpconn* pConnection,
	cstr sOperation
)
{
	xrt_http_server_mux_connection* pContext =
		(xrt_http_server_mux_connection*)
		__xrtHttpConnAdapterData(pConnection);

	if ( pContext == NULL ) {
		__xrtHttpServerMuxSetError(
			XERR_STATE,
			XHTTP_SERVER_MUX_ERROR_CONTEXT,
			sOperation,
			"HTTP server mux connection context is missing",
			NULL
		);
	}
	return pContext;
}



/* 替换当前请求固定 Router，并在赋值后释放上一请求引用。 */
static void __xrtHttpServerMuxConnectionRouter(
	xrt_http_server_mux_connection* pContext,
	xhttpserverrouter* pRouter
)
{
	xhttpserverrouter* pOld = pContext->Router;

	pContext->Router = pRouter;
	xrtHttpServerRouterDestroy(pOld);
}



/* 未配置目标 Host 时选择用户回退或提交标准 421。 */
static xhttpserverbodypolicy __xrtHttpServerMuxMissing(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xrt_http_server_mux_runtime* pRuntime
)
{
	if ( pRuntime->Events.Request != NULL ) {
		if ( pRuntime->Events.Headers != NULL ) {
			return pRuntime->Events.Headers(
				pServer,
				pConnection,
				pRequest,
				pRuntime->Events.Data
			);
		}
		return XHTTP_SERVER_BODY_BUFFER;
	}
	if ( xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_MISDIRECTED_REQUEST,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Misdirected Request")
	) != XNET_RESULT_OK ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
	}
	return XHTTP_SERVER_BODY_REJECT;
}



/* 在 Header 阶段按有效 authority 固定本次请求使用的 Router。 */
static xhttpserverbodypolicy __xrtHttpServerMuxHeaders(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-headers"
		);
	xhttpserverrouter* pRouter = NULL;
	xhttpservermuxstatus Status;
	xhttpauthority Authority;

	if ( pContext == NULL ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	__xrtHttpServerRouterMatchDestroy(pContext->Match);
	pContext->Match = NULL;
	if ( !xrtHttpServerRequestAuthority(
		pRequest, &Authority
	) ) {
		__xrtHttpServerMuxWrapError(
			XERR_PROTOCOL,
			XHTTP_SERVER_MUX_ERROR_HOST,
			"route-http-server-mux-headers",
			"HTTP server request authority could not be parsed"
		);
		__xrtHttpServerMuxConnectionRouter(pContext, NULL);
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	Status = __xrtHttpServerMuxSelect(
		pRuntime->Mux, Authority.Host, &pRouter
	);
	__xrtHttpServerMuxConnectionRouter(pContext, pRouter);
	if ( Status == XHTTP_SERVER_MUX_ERROR ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return XHTTP_SERVER_BODY_REJECT;
	}
	if ( Status == XHTTP_SERVER_MUX_NOT_FOUND ) {
		return __xrtHttpServerMuxMissing(
			pServer, pConnection, pRequest, pRuntime
		);
	}
	return __xrtHttpServerRouterDispatchHeaders(
		pContext->Router,
		pServer,
		pConnection,
		pRequest,
		&pRuntime->Events,
		&pContext->Match
	);
}



/* 把正文片段交给 Header 阶段固定的 Router 或用户回退。 */
static bool __xrtHttpServerMuxBody(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	xbytesview Data,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-body"
		);

	if ( pContext == NULL ) {
		return false;
	}
	if ( pContext->Router != NULL ) {
		return __xrtHttpServerRouterDispatchBody(
			pContext->Router,
			pServer,
			pConnection,
			pRequest,
			Data,
			&pRuntime->Events,
			pContext->Match
		);
	}
	if ( pRuntime->Events.Body != NULL ) {
		return pRuntime->Events.Body(
			pServer,
			pConnection,
			pRequest,
			Data,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerMuxSetError(
		XERR_STATE,
		XHTTP_SERVER_MUX_ERROR_CONTEXT,
		"route-http-server-mux-body",
		"HTTP server mux fallback selected streaming without a Body handler",
		NULL
	);
	return false;
}



/* 把完整请求交给固定 Router，或使用用户未命中回退。 */
static void __xrtHttpServerMuxRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	xrt_http_server_mux_connection* pContext =
		__xrtHttpServerMuxConnection(
			pConnection,
			"route-http-server-mux-request"
		);
	xrt_http_server_route_match* pMatch;

	if ( pContext == NULL ) {
		__xrtHttpServerRouterDispatchFail(
			pServer, pConnection, &pRuntime->Events
		);
		return;
	}
	if ( pContext->Router != NULL ) {
		pMatch = pContext->Match;
		pContext->Match = NULL;
		__xrtHttpServerRouterDispatchRequest(
			pContext->Router,
			pServer,
			pConnection,
			pRequest,
			&pRuntime->Events,
			pMatch
		);
		__xrtHttpServerRouterMatchDestroy(pMatch);
	} else if ( pRuntime->Events.Request != NULL ) {
		pRuntime->Events.Request(
			pServer,
			pConnection,
			pRequest,
			pRuntime->Events.Data
		);
	}
}



/* 转发底层 Server 错误事件。 */
static void __xrtHttpServerMuxError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;

	if ( pRuntime->Events.Error != NULL ) {
		pRuntime->Events.Error(
			pServer,
			pConnection,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 先归还 Connection 固定 Router，再转发唯一 Close 事件。 */
static void __xrtHttpServerMuxClose(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;
	ptr pContext = __xrtHttpConnAdapterTake(pConnection);

	__xrtHttpServerMuxConnectionDestroy(pContext);
	if ( pRuntime->Events.Close != NULL ) {
		pRuntime->Events.Close(
			pServer,
			pConnection,
			Result,
			pError,
			pRuntime->Events.Data
		);
	}
}



/* 转发最终 Shutdown 并释放 Server 持有的 Mux 适配器。 */
static void __xrtHttpServerMuxShutdown(
	xhttpserver* pServer,
	ptr pData
)
{
	xrt_http_server_mux_runtime* pRuntime =
		(xrt_http_server_mux_runtime*)pData;

	if ( pRuntime->Events.Shutdown != NULL ) {
		pRuntime->Events.Shutdown(
			pServer,
			pRuntime->Events.Data
		);
	}
	__xrtHttpServerMuxRuntimeDestroy(pRuntime);
}



/* 释放适配器与其持有的 Mux 引用。 */
void __xrtHttpServerMuxRuntimeDestroy(
	xrt_http_server_mux_runtime* pRuntime
)
{
	xhttpservermux* pMux;

	if ( pRuntime == NULL ) {
		return;
	}
	pMux = pRuntime->Mux;
	xrtFree(pRuntime);
	xrtHttpServerMuxDestroy(pMux);
}



/* 建立供明文和 TLS Server 共用的完整事件适配器。 */
xrt_http_server_mux_runtime* __xrtHttpServerMuxRuntimeCreate(
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents,
	xhttpserverevents* pOutput
)
{
	xrt_http_server_mux_runtime* pRuntime;

	if ( (pMux == NULL) || (pOutput == NULL) ) {
		__xrtHttpServerMuxSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_MUX_ERROR_ARGUMENT,
			"start-http-server-mux",
			"HTTP server mux and output events are required",
			NULL
		);
		return NULL;
	}
	pRuntime = (xrt_http_server_mux_runtime*)xrtCalloc(
		1, sizeof(*pRuntime)
	);
	if ( pRuntime == NULL ) {
		__xrtHttpServerMuxWrapError(
			XERR_MEMORY,
			XHTTP_SERVER_MUX_ERROR_MEMORY,
			"start-http-server-mux",
			"HTTP server mux runtime allocation failed"
		);
		return NULL;
	}
	pRuntime->Mux = xrtHttpServerMuxRef(pMux);
	if ( pRuntime->Mux == NULL ) {
		xrtFree(pRuntime);
		return NULL;
	}
	if ( pEvents != NULL ) {
		pRuntime->Events = *pEvents;
	}
	xrtHttpServerEventsInit(pOutput);
	pOutput->Open = __xrtHttpServerMuxOpen;
	pOutput->Headers = __xrtHttpServerMuxHeaders;
	pOutput->Body = __xrtHttpServerMuxBody;
	pOutput->Request = __xrtHttpServerMuxRequest;
	pOutput->Close = __xrtHttpServerMuxClose;
	pOutput->Error = __xrtHttpServerMuxError;
	pOutput->Shutdown = __xrtHttpServerMuxShutdown;
	pOutput->Data = pRuntime;
	return pRuntime;
}



/* 启动明文 Mux Server；失败时同步归还适配器引用。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerMuxStart(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_mux_runtime* pRuntime =
		__xrtHttpServerMuxRuntimeCreate(
			pMux, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStart(
		pEngine, pConfig, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerMuxRuntimeDestroy(pRuntime);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_START,
			"start-http-server-mux",
			"HTTP server mux listener start failed"
		);
	}
	return pServer;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_mux_tls.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)




#if defined(XHTTP_FEATURE_HTTP_SERVER_MUX_TLS)

/* 启动 TLS Mux Server；SNI 身份选择仍由独立 TLS Server 配置负责。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerMuxStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpservermux* pMux,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_mux_runtime* pRuntime =
		__xrtHttpServerMuxRuntimeCreate(
			pMux, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStartTls(
		pEngine, pConfig, pTls, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerMuxRuntimeDestroy(pRuntime);
		__xrtHttpServerMuxWrapError(
			XERR_IO,
			XHTTP_SERVER_MUX_ERROR_START,
			"start-https-server-mux",
			"HTTPS server mux listener start failed"
		);
	}
	return pServer;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xhttp-server/src/http/http_server_router_tls.c */
/* ========================================================================== */

#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)




#if defined(XHTTP_FEATURE_HTTP_SERVER_ROUTER_TLS)

/* 启动 TLS Router Server；失败时同步归还适配器和 Router 引用。 */
XHTTP_SERVER_API xhttpserver* xrtHttpServerRouterStartTls(
	xnetengine* pEngine,
	const xhttpserverconfig* pConfig,
	const xhttpservertlsconfig* pTls,
	xhttpserverrouter* pRouter,
	const xhttpserverevents* pEvents
)
{
	xhttpserverevents Events;
	xrt_http_server_router_runtime* pRuntime =
		__xrtHttpServerRouterRuntimeCreate(
			pRouter, pEvents, &Events
		);
	xhttpserver* pServer;

	if ( pRuntime == NULL ) {
		return NULL;
	}
	pServer = xrtHttpServerStartTls(
		pEngine, pConfig, pTls, &Events
	);
	if ( pServer == NULL ) {
		__xrtHttpServerRouterRuntimeDestroy(pRuntime);
		__xrtHttpServerRouterWrapError(
			XERR_IO,
			XHTTP_SERVER_ROUTER_ERROR_START,
			"start-https-server-router",
			"HTTPS server router listener start failed"
		);
	}
	return pServer;
}

#endif
#endif

#endif
