#ifndef XRT_HTTP_ORIGIN_H
#define XRT_HTTP_ORIGIN_H

#include <xrt/http.h>
#include <xrt/url.h>



#if defined(XRT_FEATURE_HTTP_ORIGIN) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_HTTP_HOST) || \
	 !defined(XRT_FEATURE_URL))
	#error "XRT HTTP Origin support requires HTTP Host and URL support"
#endif

#if defined(XRT_FEATURE_HTTP_ORIGIN_WRITE) && \
	!defined(XRT_FEATURE_HTTP_ORIGIN)
	#error "XRT HTTP Origin writer requires Origin parser support"
#endif



#if defined(XRT_FEATURE_HTTP_ORIGIN)

/* null Origin 没有可比较的 scheme、host 和 port 三元组。 */
#define XHTTP_ORIGIN_NULL UINT32_C(0x00000001)



/* Origin 借用输入或来源 URL；Text 只保存原始线路元素。 */
typedef struct xhttporigin {
	xstrview Text;
	xurl Url;
	uint32 Flags;
} xhttporigin;



/* Origin 列表游标绑定原字段值，调用方不得直接修改。 */
typedef struct xhttporigincursor {
	const void* Source;
	size_t Size;
	size_t Offset;
	size_t End;
	uint8 Validated;
} xhttporigincursor;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_ORIGIN)

/* 初始化 Origin 列表游标。 */
XRT_API void xrtHttpOriginCursorInit(
	xhttporigincursor* pCursor
);



/* 建立可用于比较和写出的 null Origin。 */
XRT_API void xrtHttpOriginNull(xhttporigin* pOrigin);



/* 严格解析 null 或一个 RFC 6454 serialized-origin。 */
XRT_API bool xrtHttpOriginParse(
	xstrview Text,
	xhttporigin* pOrigin
);



/* 从绝对分层 URL 提取 scheme、host 和 port，路径与查询不进入 Origin。 */
XRT_API bool xrtHttpOriginFromUrl(
	const xurl* pUrl,
	xhttporigin* pOrigin
);



/* 严格验证完整 Origin 字段值，包括 RFC 6454 的历史 Origin 列表形式。 */
XRT_API bool xrtHttpOriginValid(xstrview Value);



/* 完整预校验后按线路顺序迭代 Origin；游标在首次调用时绑定输入。 */
XRT_API xhttpnext xrtHttpOriginNext(
	xstrview Value,
	xhttporigincursor* pCursor,
	xhttporigin* pOrigin
);



/*
	读取唯一 Origin 字段和唯一 Origin 值。
	缺失返回 END；重复字段、历史列表或非法值返回 ERROR。
*/
XRT_API xhttpnext xrtHttpOriginFields(
	const xhttpfield* pFields,
	size_t iCount,
	xhttporigin* pOrigin
);



/*
	比较 scheme、host 和有效端口是否同源。
	null Origin 没有稳定身份，因此与任何 Origin 都不同源。
*/
XRT_API bool xrtHttpOriginSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
);

#endif



#if defined(XRT_FEATURE_HTTP_ORIGIN_WRITE)

/* 按 ASCII 规范写出一个 Origin，不附加零字符。 */
XRT_API bool xrtHttpOriginWrite(
	const xhttporigin* pOrigin,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/*
	写出单空格分隔的 RFC 6454 Origin 列表。
	null 只能单独写出，连续同源项会被拒绝。
*/
XRT_API bool xrtHttpOriginListWrite(
	const xhttporigin* pOrigins,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Origin 字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpOriginBuild(
	const xhttporigin* pOrigins,
	size_t iCount,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
