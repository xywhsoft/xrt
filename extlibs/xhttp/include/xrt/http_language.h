#ifndef XRT_HTTP_LANGUAGE_H
#define XRT_HTTP_LANGUAGE_H

#include <xrt/http.h>



#if defined(XHTTP_FEATURE_HTTP_LANGUAGE_CORE) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP language core requires HTTP support"
#endif

#if defined(XHTTP_FEATURE_HTTP_LANGUAGE) && \
	!defined(XHTTP_FEATURE_HTTP_LANGUAGE_CORE)
	#error "XRT HTTP language negotiation requires HTTP language core support"
#endif



#if defined(XHTTP_FEATURE_HTTP_LANGUAGE)

/* 语言范围借用字段值，SubtagCount 不包含通配符。 */
typedef struct xhttplanguagerange {
	xstrview Range;
	size_t SubtagCount;
	uint16 Quality;
} xhttplanguagerange;



/* 游标可跨越重复 Accept-Language 字段逐项迭代。 */
typedef struct xhttplanguagecursor {
	size_t Field;
	size_t Offset;
} xhttplanguagecursor;



/* 有效匹配保留决定质量值的范围位置和具体度。 */
typedef struct xhttplanguagematch {
	size_t Field;
	size_t Order;
	size_t SubtagCount;
	uint16 Quality;
} xhttplanguagematch;



XRT_EXTERN_C_BEGIN



/* 判断文本是否是 RFC 4647 basic language range；纯谓词不修改错误槽。 */
XRT_API bool xrtHttpLanguageRangeValid(xstrview Range);



/* 判断文本是否是可用于匹配的基本语言标签；纯谓词不修改错误槽。 */
XRT_API bool xrtHttpLanguageTagValid(xstrview Tag);



/* 初始化可重复使用的 Accept-Language 字段游标。 */
XRT_API void xrtHttpLanguageCursorInit(xhttplanguagecursor* pCursor);



/* 严格迭代一个 Accept-Language 字段值。 */
XRT_API xhttpnext xrtHttpLanguageRangeNext(
	xstrview List,
	size_t* pOffset,
	xhttplanguagerange* pRange
);



/* 按线路顺序跨越全部重复 Accept-Language 字段迭代语言范围。 */
XRT_API xhttpnext xrtHttpAcceptLanguageNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttplanguagecursor* pCursor,
	xhttplanguagerange* pRange
);



/* 按 RFC 4647 Basic Filtering 判断语言范围是否匹配语言标签。 */
XRT_API xhttpnext xrtHttpLanguageBasicMatch(
	xstrview Range,
	xstrview Tag
);



/* 计算语言标签的有效 Accept-Language 匹配；缺失字段质量为 1000。 */
XRT_API bool xrtHttpAcceptLanguageMatch(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Tag,
	xhttplanguagematch* pMatch
);



/* 返回语言标签的有效质量；错误与不可接受都返回零，错误槽区分二者。 */
XRT_API uint16 xrtHttpAcceptLanguageQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Tag
);



/* 使用 Basic Filtering 从服务端偏好顺序中选择最高质量语言。 */
XRT_API xhttpnext xrtHttpAcceptLanguageSelect(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pTags,
	size_t iTagCount,
	size_t* pIndex
);



/* 按 RFC 4647 Lookup 逐级回退，并返回可用语言数组中的索引。 */
XRT_API xhttpnext xrtHttpAcceptLanguageLookup(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pTags,
	size_t iTagCount,
	size_t* pIndex
);



XRT_EXTERN_C_END

#endif

#endif
