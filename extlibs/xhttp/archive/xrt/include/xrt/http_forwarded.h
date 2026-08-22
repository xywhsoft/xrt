#ifndef XRT_HTTP_FORWARDED_H
#define XRT_HTTP_FORWARDED_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_FORWARDED) && \
	(!defined(XRT_FEATURE_HTTP_PARAM) || \
	 !defined(XRT_FEATURE_HTTP_HOST))
	#error "XRT HTTP Forwarded support requires HTTP parameters and Host support"
#endif

#if defined(XRT_FEATURE_HTTP_FORWARDED_WRITE) && \
	!defined(XRT_FEATURE_HTTP_FORWARDED)
	#error "XRT HTTP Forwarded writer requires Forwarded parser support"
#endif



#if defined(XRT_FEATURE_HTTP_FORWARDED)

/* 已知参数存在位；未知扩展仍可通过 Element 与 PairNext 访问。 */
typedef enum xhttpforwardedflags {
	XHTTP_FORWARDED_HAS_FOR = 0x01,
	XHTTP_FORWARDED_HAS_BY = 0x02,
	XHTTP_FORWARDED_HAS_HOST = 0x04,
	XHTTP_FORWARDED_HAS_PROTO = 0x08
} xhttpforwardedflags;



/* 一个代理节点借用原字段元素与四个标准参数。 */
typedef struct xhttpforwarded {
	xstrview Element;
	xhttpparam For;
	xhttpparam By;
	xhttpparam Host;
	xhttpparam Proto;
	size_t PairCount;
	uint32 Flags;
} xhttpforwarded;



/* 单字段游标绑定首次迭代的不可变字段值，调用方不得直接修改。 */
typedef struct xhttpforwardedcursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 State;
} xhttpforwardedcursor;



/* 重复字段游标绑定首次迭代的不可变字段数组。 */
typedef struct xhttpforwardedfieldcursor {
	const void* Source;
	size_t SourceSize;
	size_t Field;
	size_t Offset;
	uint8 State;
} xhttpforwardedfieldcursor;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Forwarded 字段值游标。 */
XRT_API void xrtHttpForwardedCursorInit(
	xhttpforwardedcursor* pCursor
);



/* 初始化重复 Forwarded 字段行游标。 */
XRT_API void xrtHttpForwardedFieldCursorInit(
	xhttpforwardedfieldcursor* pCursor
);



/* 严格读取下一项 name=value，忽略 RFC 允许的空分号项。 */
XRT_API xhttpnext xrtHttpForwardedPairNext(
	xstrview Element,
	size_t* pOffset,
	xhttpparam* pPair
);



/* 严格解析一个代理元素；元素可为空，重复参数非法。 */
XRT_API bool xrtHttpForwardedElementParse(
	xstrview Element,
	xhttpforwarded* pForwarded
);



/* 严格验证完整 Forwarded 字段值和所有标准参数语义。 */
XRT_API bool xrtHttpForwardedValid(xstrview Value);



/* 完整验证并统计一个 Forwarded 字段值中的代理元素数量。 */
XRT_API bool xrtHttpForwardedCount(
	xstrview Value,
	size_t* pCount
);



/* 按代理链路顺序迭代一个完整 Forwarded 字段值。 */
XRT_API xhttpnext xrtHttpForwardedNext(
	xstrview Value,
	xhttpforwardedcursor* pCursor,
	xhttpforwarded* pForwarded
);



/* 完整验证并统计全部同名字段行中的代理元素数量。 */
XRT_API bool xrtHttpForwardedFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	size_t* pCount
);



/* 跨重复 Forwarded 字段行按线路顺序迭代代理元素。 */
XRT_API xhttpnext xrtHttpForwardedFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpforwardedfieldcursor* pCursor,
	xhttpforwarded* pForwarded
);



/* 验证已经解码的 by 或 for 节点标识。 */
XRT_API bool xrtHttpForwardedNodeValid(xstrview Node);



/* 验证已解码的 Host ABNF；空主机和任意长度十进制端口合法。 */
XRT_API bool xrtHttpForwardedHostValid(xstrview Host);



/* 验证已经解码的 proto 参数值是 URI scheme。 */
XRT_API bool xrtHttpForwardedProtoValid(xstrview Proto);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_HTTP_FORWARDED_WRITE)

/* 扩展参数写入项保存参数名称和已经解码的语义值。 */
typedef struct xhttpforwardedpairvalue {
	xstrview Name;
	xstrview Value;
} xhttpforwardedpairvalue;



/* 写入元素通过存在位选择标准参数，并允许追加扩展参数。 */
typedef struct xhttpforwardedvalue {
	xstrview For;
	xstrview By;
	xstrview Host;
	xstrview Proto;
	const xhttpforwardedpairvalue* Extensions;
	size_t ExtensionCount;
	uint32 Flags;
} xhttpforwardedvalue;



XRT_EXTERN_C_BEGIN



/* 规范写出一个 Forwarded 元素，不附加字段名称或 CRLF。 */
XRT_API bool xrtHttpForwardedElementWrite(
	const xhttpforwardedvalue* pElement,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 原子写出逗号分隔的 Forwarded 元素数组。 */
XRT_API bool xrtHttpForwardedWrite(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Forwarded 字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpForwardedBuild(
	const xhttpforwardedvalue* pElements,
	size_t iCount,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
