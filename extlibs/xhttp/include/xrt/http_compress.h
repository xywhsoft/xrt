#ifndef XRT_HTTP_COMPRESS_H
#define XRT_HTTP_COMPRESS_H

#include <xrt/http_body_deflate.h>
#include <xrt/http_cache.h>
#include <xrt/http_encoding.h>
#include <xrt/http_semantics.h>
#include <xrt/http_server.h>
#include <xrt/http_vary.h>
#include <xrt/mime.h>



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
XRT_API void xrtHttpReplyCompressConfigInit(
	xhttpreplycompressconfig* pConfig
);



/*
	按协商状态生成独立 Reply，绝不修改输入 Reply。
	IDENTITY 与 APPLIED 返回拥有型 Output；其他结果把有效且不重叠的 Output 置为空。
	Output 槽允许未对齐存储，但完整可写区间不得回绕或覆盖输入。
*/
XRT_API xhttpreplycompressstatus xrtHttpReplyCompress(
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
XRT_API xhttpreplycompressstatus xrtHttpServerReplyCompress(
	const xhttpserverrequest* pRequest,
	const xhttpreply* pReply,
	const xhttpreplycompressconfig* pConfig,
	xhttpreply** ppOutput
);

#endif



XRT_EXTERN_C_END

#endif
