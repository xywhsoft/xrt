#ifndef XRT_HTTP_TE_H
#define XRT_HTTP_TE_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_TE) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_PARAM))
	#error "XRT HTTP TE support requires HTTP parameter support"
#endif



#if defined(XRT_FEATURE_HTTP_TE)

/* 单个 TE 成员标志区分 trailers、传输参数和显式权重。 */
typedef enum xhttptecodingflag {
	XHTTP_TE_CODING_NONE = 0,
	XHTTP_TE_CODING_TRAILERS = UINT32_C(0x00000001),
	XHTTP_TE_CODING_HAS_PARAMETERS = UINT32_C(0x00000002),
	XHTTP_TE_CODING_HAS_WEIGHT = UINT32_C(0x00000004)
} xhttptecodingflag;



/* TE 成员借用完整元素、编码名称和不含 q 权重的传输参数。 */
typedef struct xhttptecoding {
	xstrview Element;
	xstrview Coding;
	xstrview Parameters;
	size_t ParameterCount;
	uint16 Quality;
	uint32 Flags;
} xhttptecoding;



/* 单字段游标由初始化函数建立，调用方不得直接修改。 */
typedef struct xhttptecursor {
	size_t Offset;
	uint8 Validated;
} xhttptecursor;



/* 重复字段游标同时记录当前字段和字段内位置。 */
typedef struct xhttptefieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttptefieldcursor;



/* TE 汇总标志明确区分字段缺失、空字段和 trailers 能力。 */
typedef enum xhttpteflag {
	XHTTP_TE_NONE = 0,
	XHTTP_TE_PRESENT = UINT32_C(0x00000001),
	XHTTP_TE_ACCEPTS_TRAILERS = UINT32_C(0x00000002),
	XHTTP_TE_HAS_TRANSFER_CODINGS = UINT32_C(0x00000004)
} xhttpteflag;



/* TE 汇总保留字段、总成员和实际传输编码数量。 */
typedef struct xhttpteinfo {
	size_t FieldCount;
	size_t CodingCount;
	size_t TransferCodingCount;
	uint32 Flags;
} xhttpteinfo;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_TE)

/* 初始化单个 TE 字段值游标。 */
XRT_API void xrtHttpTeCursorInit(xhttptecursor* pCursor);



/* 初始化重复 TE 字段游标。 */
XRT_API void xrtHttpTeFieldCursorInit(
	xhttptefieldcursor* pCursor
);



/* 严格解析一个不含列表分隔逗号的 TE 成员。 */
XRT_API bool xrtHttpTeCodingParse(
	xstrview Element,
	xhttptecoding* pCoding
);



/* 完整验证一个 TE 字段值；HTTP 列表空成员会被忽略。 */
XRT_API bool xrtHttpTeValid(xstrview Value);



/* 完整验证并统计一个 TE 字段值中的非空成员。 */
XRT_API bool xrtHttpTeCount(
	xstrview Value,
	size_t* pCount
);



/* 按线路顺序迭代一个完整 TE 字段值。 */
XRT_API xhttpnext xrtHttpTeNext(
	xstrview Value,
	xhttptecursor* pCursor,
	xhttptecoding* pCoding
);



/* 跨重复 TE 字段行按线路顺序迭代全部成员。 */
XRT_API xhttpnext xrtHttpTeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttptefieldcursor* pCursor,
	xhttptecoding* pCoding
);



/* 完整解析全部重复 TE 字段并发布零分配汇总。 */
XRT_API bool xrtHttpTeParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpteinfo* pInfo
);



/* 返回指定传输编码的最高有效权重；缺失或不匹配返回零。 */
XRT_API uint16 xrtHttpTeQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Coding
);



/* 完整验证并判断客户端是否声明不会丢弃 Trailer。 */
XRT_API xhttpnext xrtHttpTeAcceptsTrailers(
	const xhttpfield* pFields,
	size_t iCount
);

#endif



XRT_EXTERN_C_END

#endif
