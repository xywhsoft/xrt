#ifndef XRT_HTTP_VARY_H
#define XRT_HTTP_VARY_H

#include <xrt/http.h>



#if defined(XHTTP_FEATURE_HTTP_VARY) && \
	!defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Vary support requires HTTP support"
#endif



#if defined(XHTTP_FEATURE_HTTP_VARY)

/* Vary 计划区分字段存在、名称、星号、混合和空字段。 */
typedef enum xhttpvaryflag {
	XHTTP_VARY_NONE = 0,
	XHTTP_VARY_PRESENT = UINT32_C(0x00000001),
	XHTTP_VARY_NAMES = UINT32_C(0x00000002),
	XHTTP_VARY_WILDCARD = UINT32_C(0x00000004),
	XHTTP_VARY_MIXED = UINT32_C(0x00000008),
	XHTTP_VARY_EMPTY = UINT32_C(0x00000010)
} xhttpvaryflag;



/* 游标可在重复 Vary 字段之间无分配前向迭代。 */
typedef struct xhttpvarycursor {
	size_t Field;
	size_t Offset;
} xhttpvarycursor;



/* 条目借用原字段名称；Wildcard 表示特殊的星号成员。 */
typedef struct xhttpvaryitem {
	xstrview Name;
	bool Wildcard;
} xhttpvaryitem;



/* 计划保存完整列表事实，不绑定缓存键或字段值归一化策略。 */
typedef struct xhttpvaryplan {
	size_t FieldCount;
	size_t ItemCount;
	size_t NameCount;
	size_t EmptyFieldCount;
	size_t JoinedSize;
	uint32 Flags;
} xhttpvaryplan;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_VARY)

/* 初始化可重复使用的 Vary 前向游标；输出支持未对齐存储。 */
XRT_API void xrtHttpVaryCursorInit(xhttpvarycursor* pCursor);



/*
	按字段出现顺序读取全部 Vary 成员。
	星号仍返回 ITEM；列表语法错误返回 ERROR。
	字段数组、游标和条目描述符都支持未对齐存储。
*/
XRT_API xhttpnext xrtHttpVaryNext(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpvarycursor* pCursor,
	xhttpvaryitem* pItem
);



/*
	建立重复字段的零分配协议计划。
	列表包含星号和名称时设置 MIXED，但不把可观察的线路事实当作解析错误。
	计划输出在完整验证后一次性发布，可位于未对齐存储。
*/
XRT_API bool xrtHttpVaryPlan(
	const xhttpfield* pFields,
	size_t iCount,
	xhttpvaryplan* pPlan
);



/* 大小写不敏感地查找选择字段；未对齐输出在完整扫描后发布。 */
XRT_API xhttpnext xrtHttpVaryFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpvaryitem* pItem
);



/*
	按字段出现顺序写出以逗号空格连接的原始值，不附加零字符。
	NULL/0 成功查询精确大小，容量不足时不会写出部分结果。
	字段数组和大小输出支持未对齐存储。
*/
XRT_API bool xrtHttpVaryWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
