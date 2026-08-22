#ifndef XRT_HTTP_ACCEPT_H
#define XRT_HTTP_ACCEPT_H

#include <xrt/mime.h>



#if defined(XRT_FEATURE_HTTP_ACCEPT) && \
	!defined(XRT_FEATURE_MIME)
	#error "XRT HTTP Accept negotiation requires MIME support"
#endif



#if defined(XRT_FEATURE_HTTP_ACCEPT)

/* 媒体范围按任意类型、指定主类型和完整类型递增具体度。 */
typedef enum xhttpmediarangespecificity {
	XHTTP_MEDIA_RANGE_ANY = 0,
	XHTTP_MEDIA_RANGE_TYPE,
	XHTTP_MEDIA_RANGE_EXACT
} xhttpmediarangespecificity;



/* 媒体范围借用字段值；Parameters 保留完整参数序列，ParameterCount 不含 q。 */
typedef struct xhttpmediarange {
	xstrview Type;
	xstrview Subtype;
	xstrview Parameters;
	size_t ParameterCount;
	uint16 Quality;
	xhttpmediarangespecificity Specificity;
} xhttpmediarange;



/* 游标可跨越重复 Accept 字段逐项迭代。 */
typedef struct xhttpacceptcursor {
	size_t Field;
	size_t Offset;
} xhttpacceptcursor;



/* 匹配结果保留决定质量值的范围位置与具体度。 */
typedef struct xhttpacceptmatch {
	size_t Field;
	size_t Order;
	size_t ParameterCount;
	uint16 Quality;
	xhttpmediarangespecificity Specificity;
} xhttpacceptmatch;



XRT_EXTERN_C_BEGIN



/* 初始化可重复使用的 Accept 字段游标。 */
XRT_API void xrtHttpAcceptCursorInit(xhttpacceptcursor* pCursor);



/* 严格迭代一个 Accept 字段值，忽略由连续逗号产生的空成员。 */
XRT_API xhttpnext xrtHttpMediaRangeNext(
	xstrview List,
	size_t* pOffset,
	xhttpmediarange* pRange
);



/* 迭代媒体范围的非 q 参数；描述符、游标和结果均支持未对齐存储。 */
XRT_API xhttpnext xrtHttpMediaRangeParamNext(
	const xhttpmediarange* pRange,
	size_t* pOffset,
	xhttpparam* pParam
);



/* 按线路顺序跨越全部重复 Accept 字段迭代媒体范围。 */
XRT_API xhttpnext xrtHttpAcceptNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpacceptcursor* pCursor,
	xhttpmediarange* pRange
);



/* 判断已解析媒体范围是否匹配已解析媒体类型。 */
XRT_API xhttpnext xrtHttpMediaRangeMatch(
	const xhttpmediarange* pRange,
	const xmediatype* pType
);



/* 计算一个媒体类型的有效 Accept 匹配；缺失字段默认质量为 1000。 */
XRT_API bool xrtHttpAcceptMatch(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview MediaType,
	xhttpacceptmatch* pMatch
);



/* 返回媒体类型的有效质量值；错误与不可接受都返回零，错误槽区分二者。 */
XRT_API uint16 xrtHttpAcceptQuality(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview MediaType
);



/* 从按服务端偏好排序的媒体类型数组中选择最高质量项。 */
XRT_API xhttpnext xrtHttpAcceptSelect(
	const xhttpfield* pFields,
	size_t iFieldCount,
	const xstrview* pMediaTypes,
	size_t iMediaTypeCount,
	size_t* pIndex
);



XRT_EXTERN_C_END

#endif

#endif
