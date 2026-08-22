#ifndef XRT_HTTP_VIA_H
#define XRT_HTTP_VIA_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_VIA) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Via support requires HTTP support"
#endif

#if defined(XRT_FEATURE_HTTP_VIA_WRITE) && \
	!defined(XRT_FEATURE_HTTP_VIA)
	#error "XRT HTTP Via writer requires Via parser support"
#endif



#if defined(XRT_FEATURE_HTTP_VIA)

/* Via 元素存在位明确区分省略项与显式空端口或空注释。 */
typedef enum xhttpviaflag {
	XHTTP_VIA_HAS_PROTOCOL_NAME = UINT32_C(0x00000001),
	XHTTP_VIA_HAS_PORT = UINT32_C(0x00000002),
	XHTTP_VIA_HAS_COMMENT = UINT32_C(0x00000004)
} xhttpviaflag;



/*
	Via 元素全部借用原字段值。
	Comment 包含最外层括号并保留 quoted-pair 和嵌套注释线路形式。
*/
typedef struct xhttpvia {
	xstrview Element;
	xstrview ProtocolName;
	xstrview ProtocolVersion;
	xstrview ReceivedBy;
	xstrview Pseudonym;
	xstrview Port;
	xstrview Comment;
	uint32 Flags;
} xhttpvia;



/* 单字段游标绑定原字段值，调用方不得直接修改。 */
typedef struct xhttpviacursor {
	const void* Source;
	size_t Size;
	size_t Offset;
	uint8 Validated;
} xhttpviacursor;



/* 重复字段游标绑定原字段数组并记录当前字段和字段内位置。 */
typedef struct xhttpviafieldcursor {
	const void* Source;
	size_t Count;
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttpviafieldcursor;

#endif



#if defined(XRT_FEATURE_HTTP_VIA_WRITE)

/*
	Via 写入值使用已经解码的普通注释文本。
	三个存在位分别控制协议名、端口和注释，可明确写出空端口或空注释。
*/
typedef struct xhttpviavalue {
	xstrview ProtocolName;
	xstrview ProtocolVersion;
	xstrview Pseudonym;
	xstrview Port;
	xstrview Comment;
	uint32 Flags;
} xhttpviavalue;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_VIA)

/* 初始化单个 Via 字段值游标。 */
XRT_API void xrtHttpViaCursorInit(xhttpviacursor* pCursor);



/* 初始化重复 Via 字段行游标。 */
XRT_API void xrtHttpViaFieldCursorInit(
	xhttpviafieldcursor* pCursor
);



/* 严格解析一个不含列表逗号的非空 Via 元素。 */
XRT_API bool xrtHttpViaElementParse(
	xstrview Element,
	xhttpvia* pVia
);



/* 严格验证完整 Via 字段值，包括 HTTP 列表语法允许的空列表。 */
XRT_API bool xrtHttpViaValid(xstrview Value);



/* 按线路顺序迭代一个完整 Via 字段值，游标在首次调用时绑定输入。 */
XRT_API xhttpnext xrtHttpViaNext(
	xstrview Value,
	xhttpviacursor* pCursor,
	xhttpvia* pVia
);



/* 跨重复 Via 字段行按线路顺序迭代代理节点，游标在首次调用时绑定输入。 */
XRT_API xhttpnext xrtHttpViaFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpviafieldcursor* pCursor,
	xhttpvia* pVia
);



/*
	解码一个包含最外层括号的完整 Via comment。
	嵌套括号保留为正文，quoted-pair 去除反斜线；空输出可查询长度。
*/
XRT_API bool xrtHttpViaCommentDecode(
	xstrview Comment,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_VIA_WRITE)

/* 规范写出一个 Via 元素，不附加字段名称或 CRLF。 */
XRT_API bool xrtHttpViaElementWrite(
	const xhttpviavalue* pVia,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范写出逗号分隔的非空 Via 元素数组。 */
XRT_API bool xrtHttpViaWrite(
	const xhttpviavalue* pVia,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Via 字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpViaBuild(
	const xhttpviavalue* pVia,
	size_t iCount,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
