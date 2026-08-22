#ifndef XRT_HTTP_SEMANTICS_H
#define XRT_HTTP_SEMANTICS_H

#include <xrt/http.h>

#if defined(XRT_FEATURE_HTTP_PRECONDITION)
	#include <xrt/time.h>
#endif



#if defined(XRT_FEATURE_HTTP_ETAG) && !defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP entity-tag support requires XRT_FEATURE_HTTP"
#endif

#if defined(XRT_FEATURE_HTTP_PRECONDITION) && \
	(!defined(XRT_FEATURE_HTTP_ETAG) || !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRT HTTP precondition support requires entity-tag and time text support"
#endif

#if defined(XRT_FEATURE_HTTP_RANGE) && !defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP range support requires XRT_FEATURE_HTTP"
#endif

#if defined(XRT_FEATURE_HTTP_RANGE_MULTIPART) && \
	!defined(XRT_FEATURE_HTTP_RANGE)
	#error "XRT HTTP range multipart support requires HTTP ranges"
#endif



#if defined(XRT_FEATURE_HTTP_ETAG)

/* 实体标签借用不含双引号的 opaque-tag 正文。 */
typedef struct xhttpetag {
	xstrview Opaque;
	bool Weak;
} xhttpetag;



/* 实体标签列表条目区分普通标签和只能单独出现的星号。 */
typedef enum xhttpetagkind {
	XHTTP_ETAG_VALUE = 1,
	XHTTP_ETAG_ANY
} xhttpetagkind;



/* 实体标签列表迭代结果借用原字段值。 */
typedef struct xhttpetagitem {
	xhttpetagkind Kind;
	xhttpetag Tag;
} xhttpetagitem;

#endif



#if defined(XRT_FEATURE_HTTP_PRECONDITION)

/* 当前选定表示的验证器；时间使用 Unix Epoch 微秒。 */
typedef struct xhttprepresentation {
	bool Exists;
	bool HasETag;
	bool HasLastModified;
	bool LastModifiedStrong;
	xhttpetag ETag;
	xtime LastModified;
} xhttprepresentation;



/* 条件请求结果可直接映射为继续处理、304 或 412。 */
typedef enum xhttpprecondition {
	XHTTP_PRECONDITION_ERROR = -1,
	XHTTP_PRECONDITION_PROCEED = 0,
	XHTTP_PRECONDITION_NOT_MODIFIED = 304,
	XHTTP_PRECONDITION_FAILED = 412
} xhttpprecondition;

#endif



#if defined(XRT_FEATURE_HTTP_RANGE)

/* 字节范围项区分闭区间、开放尾部和后缀长度。 */
typedef enum xhttprangespecform {
	XHTTP_RANGE_SPEC_CLOSED = 1,
	XHTTP_RANGE_SPEC_OPEN,
	XHTTP_RANGE_SPEC_SUFFIX
} xhttprangespecform;



/*
	解析后的字节范围项使用 uint64，避免在 32 位平台截断线路值。
	SUFFIX 只使用 First 表示后缀长度，OPEN 只使用 First 表示起点。
*/
typedef struct xhttprangespec {
	xhttprangespecform Form;
	uint64 First;
	uint64 Last;
} xhttprangespec;



/* 已解析到表示长度内的闭区间。 */
typedef struct xhttpbyterange {
	uint64 First;
	uint64 Last;
} xhttpbyterange;



/* 字节范围解析结果区分语义错误、不满足和有效区间。 */
typedef enum xhttprangeresult {
	XHTTP_RANGE_ERROR = -1,
	XHTTP_RANGE_UNSATISFIED = 0,
	XHTTP_RANGE_SATISFIED = 1,
	XHTTP_RANGE_EMPTY = 2
} xhttprangeresult;



/*
	Content-Range 同时表达满足与不满足形式。
	满足形式允许 HasLength 为 false，对应未知完整长度的星号。
*/
typedef struct xhttpcontentrange {
	bool Satisfied;
	bool HasLength;
	uint64 First;
	uint64 Last;
	uint64 Length;
} xhttpcontentrange;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_ETAG)

/* 严格解析一个 entity-tag；未对齐 Tag 可用，Opaque 借用输入且不含双引号。 */
XRT_API bool xrtHttpETagParse(
	xstrview Text,
	xhttpetag* pTag
);



/*
	迭代 If-Match 或 If-None-Match 字段值。
	Offset 初始为零；空列表元素会被忽略，星号只能作为整个字段值出现。
	Offset 和 Item 可以未对齐，但必须覆盖完整且不回绕的内存范围。
*/
XRT_API xhttpnext xrtHttpETagNext(
	xstrview List,
	size_t* pOffset,
	xhttpetagitem* pItem
);



/* 按强规则比较两个可未对齐的实体标签，任一弱标签都不匹配。 */
XRT_API bool xrtHttpETagStrongEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);



/* 按弱规则比较两个可未对齐的实体标签，只比较 opaque-tag 正文。 */
XRT_API bool xrtHttpETagWeakEqual(
	const xhttpetag* pLeft,
	const xhttpetag* pRight
);



/*
	按强比较规则检查完整实体标签列表。
	星号视为命中；语法错误与没有命中都返回 false，前者会设置错误。
*/
XRT_API bool xrtHttpETagListStrongHas(
	xstrview List,
	const xhttpetag* pTag
);



/*
	按弱比较规则检查完整实体标签列表。
	星号视为命中；即使命中也会继续验证余下全部条目。
*/
XRT_API bool xrtHttpETagListWeakHas(
	xstrview List,
	const xhttpetag* pTag
);



/* 写出可未对齐的 entity-tag；未对齐 Size 可用，结果不附加零字符。 */
XRT_API bool xrtHttpETagWrite(
	const xhttpetag* pTag,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 从可未对齐的 entity-tag 构建零结尾文本，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpETagBuild(
	const xhttpetag* pTag,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_PRECONDITION)

/*
	按 RFC 顺序评估字段数组中的条件请求。
	字段数组可以直接来自 xhttpheaders；无关字段会被忽略。
	字段描述符数组和 Current 可以未对齐，但必须覆盖完整且不回绕的内存范围。
*/
XRT_API xhttpprecondition xrtHttpPreconditionsEvaluate(
	xstrview Method,
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xhttprepresentation* pCurrent
);



/*
	判断 If-Range 是否允许发送范围响应。
	无效、弱标签、不匹配或不够强的日期验证器都返回 false。
	Current 可以未对齐，但必须覆盖完整且不回绕的结构范围。
*/
XRT_API bool xrtHttpIfRangeMatch(
	xstrview Value,
	const xhttprepresentation* pCurrent
);

#endif



#if defined(XRT_FEATURE_HTTP_RANGE)

/* 把 Range 字段拆成范围单位和单位专用集合；未对齐输出仍借用输入。 */
XRT_API bool xrtHttpRangeParse(
	xstrview Value,
	xstrview* pUnit,
	xstrview* pSet
);



/* 迭代 byte-range-set；Offset 和 Spec 可以未对齐，空列表元素会被忽略。 */
XRT_API xhttpnext xrtHttpByteRangeNext(
	xstrview Set,
	size_t* pOffset,
	xhttprangespec* pSpec
);



/* 验证并统计完整 byte-range-set；未对齐 Count 可用，集合必须至少包含一项。 */
XRT_API bool xrtHttpByteRangeCount(
	xstrview Set,
	size_t* pCount
);



/*
	把一个范围项解析到指定完整表示长度内。
	EMPTY 表示零长度表示上的非零后缀范围，它按规范可满足但不选择字节。
	Spec 和 Range 可以未对齐，但必须覆盖完整且不回绕的结构范围。
*/
XRT_API xhttprangeresult xrtHttpByteRangeResolve(
	const xhttprangespec* pSpec,
	uint64 iLength,
	xhttpbyterange* pRange
);



/*
	把完整 byte-range-set 解析到表示长度内，并原地排序、合并重叠或相邻区间。
	MergeGap 允许额外合并间隔不超过该字节数的区间；容量不足时不修改任何输出。
	SATISFIED 返回至少一个区间，EMPTY 只表示零长度表示上的非零后缀范围。
	范围数组和标量输出可以未对齐，但必须覆盖各自完整且不回绕的内存范围。
*/
XRT_API xhttprangeresult xrtHttpByteRangesResolve(
	xstrview Set,
	uint64 iLength,
	xhttpbyterange* pRanges,
	size_t iCapacity,
	uint64 iMergeGap,
	size_t* pCount,
	uint64* pSelectedLength
);



/* 写出可未对齐的 Range 项数组；空输出查询长度，Size 也可以未对齐。 */
XRT_API bool xrtHttpRangeWrite(
	const xhttprangespec* pSpecs,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 从可未对齐的 Range 项数组构建零结尾字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpRangeBuild(
	const xhttprangespec* pSpecs,
	size_t iCount,
	size_t* pSize
);



/* 严格解析 bytes Content-Range 字段值，Range 输出可以未对齐。 */
XRT_API bool xrtHttpContentRangeParse(
	xstrview Value,
	xhttpcontentrange* pRange
);



/* 写出可未对齐的 Content-Range；空输出查询长度，Size 也可未对齐。 */
XRT_API bool xrtHttpContentRangeWrite(
	const xhttpcontentrange* pRange,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 从可未对齐的 Content-Range 构建零结尾文本，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpContentRangeBuild(
	const xhttpcontentrange* pRange,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_RANGE_MULTIPART)

/*
	计算 canonical multipart/byteranges 正文长度。
	范围必须按起点严格递增且互不重叠；空媒体类型使用二进制默认值。
	范围数组和 Length 输出可以未对齐，但必须覆盖完整且不回绕的内存范围。
*/
XRT_API bool xrtHttpRangeMultipartLength(
	const xhttpbyterange* pRanges,
	size_t iRangeCount,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	uint64* pLength
);



/*
	写出一个范围 Part 的 boundary、Content-Type、Content-Range 和终止空行。
	空输出只查询长度，输出不包含范围数据和数据后的 CRLF。
	Range 和 Size 可以未对齐，但必须覆盖完整且不回绕的内存范围。
*/
XRT_API bool xrtHttpRangeMultipartHeadWrite(
	const xhttpbyterange* pRange,
	uint64 iCompleteLength,
	xstrview ContentType,
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出一个范围 Part 数据后的 CRLF；Size 输出可以未对齐。 */
XRT_API bool xrtHttpRangeMultipartEndWrite(
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出最后一个 Part 之后的关闭 boundary；Size 输出可以未对齐。 */
XRT_API bool xrtHttpRangeMultipartCloseWrite(
	xstrview Boundary,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
