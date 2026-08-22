#ifndef XRT_HTTP_LINK_H
#define XRT_HTTP_LINK_H

#include <xrt/http_language.h>
#include <xrt/url.h>



#if defined(XRT_FEATURE_HTTP_LINK) && \
	(!defined(XRT_FEATURE_HTTP_PARAM) || \
	 !defined(XRT_FEATURE_HTTP_EXT_VALUE) || \
	 !defined(XRT_FEATURE_HTTP_LANGUAGE) || \
	 !defined(XRT_FEATURE_URL))
	#error "XRT HTTP Link support requires parameters, extended values, language, and URL support"
#endif

#if defined(XRT_FEATURE_HTTP_LINK_WRITE) && \
	!defined(XRT_FEATURE_HTTP_LINK)
	#error "XRT HTTP Link writer requires HTTP Link support"
#endif



#if defined(XRT_FEATURE_HTTP_LINK)

/* 已识别的 RFC 8288 参数存在位；重复单值参数只保留第一个。 */
typedef enum xhttplinkflags {
	XHTTP_LINK_HAS_REL = 0x01,
	XHTTP_LINK_HAS_ANCHOR = 0x02,
	XHTTP_LINK_HAS_REV = 0x04,
	XHTTP_LINK_HAS_MEDIA = 0x08,
	XHTTP_LINK_HAS_TITLE = 0x10,
	XHTTP_LINK_HAS_TITLE_EXT = 0x20,
	XHTTP_LINK_HAS_TYPE = 0x40
} xhttplinkflags;



/* 一个 Link 元素借用原字段值；Parameters 不包含第一个分号。 */
typedef struct xhttplink {
	xstrview Element;
	xstrview Target;
	xstrview Parameters;
	xhttpparam Rel;
	xhttpparam Anchor;
	xhttpparam Rev;
	xhttpparam Media;
	xhttpparam Title;
	xhttpparam TitleExt;
	xhttpparam Type;
	size_t ParamCount;
	size_t HrefLangCount;
	uint32 Flags;
} xhttplink;



/* 单字段游标绑定首次迭代的不可变字段值，调用方不得直接修改。 */
typedef struct xhttplinkcursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 Validated;
} xhttplinkcursor;



/* 重复字段游标绑定首次迭代的不可变字段数组并记录当前位置。 */
typedef struct xhttplinkfieldcursor {
	const void* Source;
	size_t SourceSize;
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttplinkfieldcursor;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Link 字段值游标。 */
XRT_API void xrtHttpLinkCursorInit(xhttplinkcursor* pCursor);



/* 初始化重复 Link 字段行游标。 */
XRT_API void xrtHttpLinkFieldCursorInit(
	xhttplinkfieldcursor* pCursor
);



/* 严格解析一个 link-value；rel 必须存在，结果借用 Element。 */
XRT_API bool xrtHttpLinkElementParse(
	xstrview Element,
	xhttplink* pLink
);



/* 严格验证完整 Link 字段值；空列表符合 RFC 的列表语法。 */
XRT_API bool xrtHttpLinkValid(xstrview Value);



/* 按线路顺序迭代一个完整 Link 字段值。 */
XRT_API xhttpnext xrtHttpLinkNext(
	xstrview Value,
	xhttplinkcursor* pCursor,
	xhttplink* pLink
);



/* 跨重复 Link 字段行按线路顺序迭代链接。 */
XRT_API xhttpnext xrtHttpLinkFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttplinkfieldcursor* pCursor,
	xhttplink* pLink
);



/* 严格查找第一个参数；未命中返回 END，结果借用 Link 字段。 */
XRT_API xhttpnext xrtHttpLinkParam(
	const xhttplink* pLink,
	xstrview Name,
	xhttpparam* pParam
);



/* 按 RFC 8288 的大小写不敏感规则查找关系类型。 */
XRT_API xhttpnext xrtHttpLinkRelationFind(
	const xhttplink* pLink,
	xstrview Relation
);



/* 解码 anchor 参数；不存在时返回 VALUE 错误，不执行 URI 解析。 */
XRT_API bool xrtHttpLinkAnchorWrite(
	const xhttplink* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 anchor，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpLinkAnchorBuild(
	const xhttplink* pLink,
	size_t* pSize
);



/* 读取 UTF-8 标题；优先 title*，不支持的字符集回退 title。 */
XRT_API bool xrtHttpLinkTitleWrite(
	const xhttplink* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 UTF-8 标题，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpLinkTitleBuild(
	const xhttplink* pLink,
	size_t* pSize
);



/* 零分配使用 RFC 3986 Base 解析链接目标；输出不带零结尾。 */
XRT_API bool xrtHttpLinkTargetResolve(
	const xhttplink* pLink,
	const xurl* pBase,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 分配并解析链接目标，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpLinkTargetResolveBuild(
	const xhttplink* pLink,
	const xurl* pBase,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_HTTP_LINK_WRITE)

/* Link 写入参数使用 xhttpparamflags 表达省略值、token 或强制引号。 */
typedef struct xhttplinkparamvalue {
	xstrview Name;
	xstrview Value;
	uint32 Flags;
} xhttplinkparamvalue;



/* 写入元素把必需的 rel 与其他参数分开，避免重复或遗漏关系类型。 */
typedef struct xhttplinkvalue {
	xstrview Target;
	xstrview Relations;
	const xhttplinkparamvalue* Parameters;
	size_t ParameterCount;
} xhttplinkvalue;



XRT_EXTERN_C_BEGIN



/* 规范写出一个 link-value，不附加字段名称或 CRLF。 */
XRT_API bool xrtHttpLinkElementWrite(
	const xhttplinkvalue* pLink,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范写出逗号分隔的 Link 元素数组。 */
XRT_API bool xrtHttpLinkWrite(
	const xhttplinkvalue* pLinks,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Link 字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpLinkBuild(
	const xhttplinkvalue* pLinks,
	size_t iCount,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
