#ifndef XRT_HTTP_EXPECT_H
#define XRT_HTTP_EXPECT_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_EXPECT) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Expect support requires HTTP support"
#endif



#if defined(XRT_FEATURE_HTTP_EXPECT)

/* Expectation 标志区分扩展值、quoted-string 和参数。 */
typedef enum xhttpexpectflag {
	XHTTP_EXPECT_BARE = 0,
	XHTTP_EXPECT_HAS_VALUE = UINT32_C(0x00000001),
	XHTTP_EXPECT_VALUE_QUOTED = UINT32_C(0x00000002),
	XHTTP_EXPECT_HAS_PARAMETERS = UINT32_C(0x00000004)
} xhttpexpectflag;



/* Expectation 借用完整元素、名称、线路值和原始参数片段。 */
typedef struct xhttpexpectation {
	xstrview Element;
	xstrview Name;
	xstrview Value;
	xstrview Parameters;
	uint32 Flags;
} xhttpexpectation;



/* 单字段游标由初始化函数建立，调用方不得直接修改。 */
typedef struct xhttpexpectcursor {
	size_t Offset;
	uint8 Validated;
} xhttpexpectcursor;



/* 重复字段游标同时记录当前字段和字段内位置。 */
typedef struct xhttpexpectfieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttpexpectfieldcursor;



/* 字段分类保留语法错误与语法正确但不受支持的扩展差异。 */
typedef enum xhttpexpectresult {
	XHTTP_EXPECT_ERROR = -1,
	XHTTP_EXPECT_NONE = 0,
	XHTTP_EXPECT_CONTINUE = 1,
	XHTTP_EXPECT_UNSUPPORTED = 2
} xhttpexpectresult;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Expect 字段值游标。 */
XRT_API void xrtHttpExpectCursorInit(
	xhttpexpectcursor* pCursor
);



/* 初始化重复 Expect 字段游标。 */
XRT_API void xrtHttpExpectFieldCursorInit(
	xhttpexpectfieldcursor* pCursor
);



/* 严格解析一个不含列表分隔逗号的 expectation。 */
XRT_API bool xrtHttpExpectationParse(
	xstrview Element,
	xhttpexpectation* pExpectation
);



/* 完整验证一个 Expect 字段值；空列表符合 HTTP 列表语法。 */
XRT_API bool xrtHttpExpectValid(xstrview Value);



/* 完整验证并统计一个 Expect 字段值中的 expectation 数量。 */
XRT_API bool xrtHttpExpectCount(
	xstrview Value,
	size_t* pCount
);



/* 按线路顺序迭代一个完整 Expect 字段值。 */
XRT_API xhttpnext xrtHttpExpectNext(
	xstrview Value,
	xhttpexpectcursor* pCursor,
	xhttpexpectation* pExpectation
);



/* 跨重复 Expect 字段行按线路顺序迭代 expectation。 */
XRT_API xhttpnext xrtHttpExpectFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpexpectfieldcursor* pCursor,
	xhttpexpectation* pExpectation
);



/* 分类全部重复 Expect 字段并完整验证所有元素。 */
XRT_API xhttpexpectresult xrtHttpExpectFields(
	const xhttpfield* pFields,
	size_t iCount
);



XRT_EXTERN_C_END

#endif

#endif
