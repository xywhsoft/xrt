#ifndef XRT_HTTP_UPGRADE_H
#define XRT_HTTP_UPGRADE_H

#include <xrt/http.h>



#if defined(XRT_FEATURE_HTTP_UPGRADE) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Upgrade support requires HTTP support"
#endif

#if defined(XRT_FEATURE_HTTP_UPGRADE_WRITE) && \
	!defined(XRT_FEATURE_HTTP_UPGRADE)
	#error "XRT HTTP Upgrade writer requires HTTP Upgrade support"
#endif



#if defined(XRT_FEATURE_HTTP_UPGRADE)

/* 一个 Upgrade 协议借用原字段值；空 Version 表示线路中没有版本。 */
typedef struct xhttpupgradeitem {
	xstrview Protocol;
	xstrview Version;
} xhttpupgradeitem;



/* 单字段游标由初始化函数建立，调用方不得直接修改。 */
typedef struct xhttpupgradecursor {
	size_t Offset;
	uint8 Validated;
} xhttpupgradecursor;



/* 重复字段游标同时记录当前字段和字段内位置。 */
typedef struct xhttpupgradefieldcursor {
	size_t Field;
	size_t Offset;
	uint8 Validated;
} xhttpupgradefieldcursor;



XRT_EXTERN_C_BEGIN



/* 初始化单个 Upgrade 字段值游标。 */
XRT_API void xrtHttpUpgradeCursorInit(
	xhttpupgradecursor* pCursor
);



/* 初始化重复 Upgrade 字段游标。 */
XRT_API void xrtHttpUpgradeFieldCursorInit(
	xhttpupgradefieldcursor* pCursor
);



/* 严格解析一个 protocol-name[/protocol-version] 元素。 */
XRT_API bool xrtHttpUpgradeParse(
	xstrview Text,
	xhttpupgradeitem* pUpgrade
);



/* 完整验证一个 Upgrade 字段值；空列表符合 HTTP 列表语法。 */
XRT_API bool xrtHttpUpgradeValid(xstrview Value);



/* 完整验证并统计一个 Upgrade 字段值中的协议数量。 */
XRT_API bool xrtHttpUpgradeCount(
	xstrview Value,
	size_t* pCount
);



/* 按线路顺序迭代一个完整 Upgrade 字段值。 */
XRT_API xhttpnext xrtHttpUpgradeNext(
	xstrview Value,
	xhttpupgradecursor* pCursor,
	xhttpupgradeitem* pUpgrade
);



/* 跨重复 Upgrade 字段行按线路顺序迭代协议。 */
XRT_API xhttpnext xrtHttpUpgradeFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpupgradefieldcursor* pCursor,
	xhttpupgradeitem* pUpgrade
);

#endif



#if defined(XRT_FEATURE_HTTP_UPGRADE_WRITE)

/* 规范写出一个或多个 Upgrade 协议；空输出可精确查询长度。 */
XRT_API bool xrtHttpUpgradeWrite(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范写出一个 Upgrade 协议元素。 */
XRT_API bool xrtHttpUpgradeElementWrite(
	const xhttpupgradeitem* pUpgrade,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Upgrade 字段值，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpUpgradeBuild(
	const xhttpupgradeitem* pUpgrades,
	size_t iCount,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_UPGRADE)

XRT_EXTERN_C_END

#endif

#endif
