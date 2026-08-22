#ifndef XRT_HTTP_ENCODING_H
#define XRT_HTTP_ENCODING_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_ENCODING) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP encoding negotiation requires HTTP support"
#endif



#if defined(XRT_FEATURE_HTTP_ENCODING)

/* 内置编码值同时可作为可用编码位掩码，NONE 表示没有可接受表示。 */
typedef enum xhttpcoding {
	XHTTP_CODING_NONE = 0,
	XHTTP_CODING_IDENTITY = UINT32_C(0x00000001),
	XHTTP_CODING_GZIP = UINT32_C(0x00000002),
	XHTTP_CODING_DEFLATE = UINT32_C(0x00000004)
} xhttpcoding;



/* 解析标志区分 Header 缺失与各个显式编码成员。 */
typedef enum xhttpacceptencodingflag {
	XHTTP_ACCEPT_ENCODING_NONE = 0,
	XHTTP_ACCEPT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_ACCEPT_ENCODING_GZIP = UINT32_C(0x00000002),
	XHTTP_ACCEPT_ENCODING_DEFLATE = UINT32_C(0x00000004),
	XHTTP_ACCEPT_ENCODING_IDENTITY = UINT32_C(0x00000008),
	XHTTP_ACCEPT_ENCODING_WILDCARD = UINT32_C(0x00000010)
} xhttpacceptencodingflag;



/*
	质量值使用 0 到 1000 的定点表示。
	同一编码重复出现时保留最高质量，Flags 记录是否显式出现。
*/
typedef struct xhttpacceptencoding {
	uint16 Gzip;
	uint16 Deflate;
	uint16 Identity;
	uint16 Wildcard;
	uint32 Flags;
} xhttpacceptencoding;



/* Content-Encoding 计划保留字段存在性、容错层和未知扩展。 */
typedef enum xhttpcontentencodingflag {
	XHTTP_CONTENT_ENCODING_NONE = 0,
	XHTTP_CONTENT_ENCODING_PRESENT = UINT32_C(0x00000001),
	XHTTP_CONTENT_ENCODING_IDENTITY = UINT32_C(0x00000002),
	XHTTP_CONTENT_ENCODING_UNKNOWN = UINT32_C(0x00000004),
	XHTTP_CONTENT_ENCODING_LEGACY = UINT32_C(0x00000008)
} xhttpcontentencodingflag;



/* 游标可在重复 Content-Encoding 字段之间无分配前向迭代。 */
typedef struct xhttpcontentencodingcursor {
	size_t Field;
	size_t Offset;
} xhttpcontentencodingcursor;



/* 每个成员保留原 token，并把内置编码映射到统一枚举。 */
typedef struct xhttpcontentencodingitem {
	xstrview Token;
	xhttpcoding Coding;
} xhttpcontentencodingitem;



/* 计划只保存解析事实，不绑定具体解码算法或未知编码策略。 */
typedef struct xhttpcontentencodingplan {
	size_t FieldCount;
	size_t CodingCount;
	size_t DecoderCount;
	size_t UnknownCount;
	size_t JoinedSize;
	uint32 Flags;
} xhttpcontentencodingplan;



/* Content-Encoding 解析与通用 Body 解码共享的安全层数边界。 */
#define XHTTP_CONTENT_CODINGS_DEFAULT 4u
#define XHTTP_CONTENT_CODINGS_MAX 16u

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_ENCODING)

/* 初始化为 Header 缺失状态；按 RFC 该状态接受任意内容编码。 */
XRT_API void xrtHttpAcceptEncodingInit(
	xhttpacceptencoding* pAccept
);



/* 判断公开协商状态字段是否自洽；纯查询不修改线程原有错误。 */
XRT_API bool xrtHttpAcceptEncodingValid(
	const xhttpacceptencoding* pAccept
);



/*
	失败原子地合并一个 Accept-Encoding 字段值。
	空值只记录 Header 存在；未知编码语法有效但不进入内置编码集合。
*/
XRT_API bool xrtHttpAcceptEncodingAdd(
	xhttpacceptencoding* pAccept,
	xstrview Value
);



/*
	扫描全部同名字段并构建零分配协商状态。
	Fields 为空且 Count 为零表示请求没有任何字段。
*/
XRT_API bool xrtHttpAcceptEncodingParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptencoding* pAccept
);



/* 返回指定内置编码的有效质量；参数错误返回零并设置错误。 */
XRT_API uint16 xrtHttpAcceptEncodingQuality(
	const xhttpacceptencoding* pAccept,
	xhttpcoding Coding
);



/*
	从 Available 位掩码中选择最高质量编码。
	等质量时先选择 Preferred，再按 gzip、deflate、identity 的顺序选择。
*/
XRT_API xhttpcoding xrtHttpAcceptEncodingSelect(
	const xhttpacceptencoding* pAccept,
	uint32 iAvailable,
	xhttpcoding Preferred
);



/* 返回 identity、gzip 或 deflate 的静态小写 token；NONE 返回空视图。 */
XRT_API xstrview xrtHttpCodingName(xhttpcoding Coding);



/*
	把合法 token 映射到内置编码。
	gzip 与兼容别名 x-gzip 返回 GZIP；未知或非法 token 返回 NONE。
*/
XRT_API xhttpcoding xrtHttpCodingParse(xstrview Token);



/* 初始化可重复使用的 Content-Encoding 前向游标。 */
XRT_API void xrtHttpContentEncodingCursorInit(
	xhttpcontentencodingcursor* pCursor
);



/*
	按字段出现顺序迭代全部 Content-Encoding 成员。
	未知扩展仍返回 ITEM，且 Item.Coding 为 NONE；语法错误返回 ERROR。
*/
XRT_API xhttpnext xrtHttpContentEncodingNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingcursor* pCursor,
	xhttpcontentencodingitem* pItem
);



/*
	无分配构建完整 Content-Encoding 计划。
	DecoderCount 只统计 gzip 和 deflate；JoinedSize 是以逗号空格连接字段值的大小。
*/
XRT_API bool xrtHttpContentEncodingPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpcontentencodingplan* pPlan
);



/*
	按字段出现顺序写出以逗号空格连接的原始值，不附加零字符。
	空输出可查询精确大小，容量不足不会写入部分结果。
*/
XRT_API bool xrtHttpContentEncodingWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
