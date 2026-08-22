#ifndef XRT_INTERNAL_HTTP_STRUCTURED_STATUS_H
#define XRT_INTERNAL_HTTP_STRUCTURED_STATUS_H

#include <xrt/http_structured.h>



#if defined(XHTTP_FEATURE_HTTP_STRUCTURED)

/* 类型化状态成员转换器；空输出只验证语义，失败不得修改输出。 */
typedef bool (*xrt_http_structured_status_read)(
	const xhttpstructuredmember* pMember,
	void* pOutput
);



/* 所有公开类型化状态字段的单值游标都使用这一布局。 */
typedef struct xrt_http_structured_status_cursor {
	const void* Source;
	size_t SourceSize;
	size_t Offset;
	uint8 Validated;
} xrt_http_structured_status_cursor;



/* 所有公开类型化状态字段的重复字段游标都使用这一布局。 */
typedef struct xrt_http_structured_status_field_cursor {
	xhttpstructuredfieldcursor Structured;
	uint8 Validated;
} xrt_http_structured_status_field_cursor;



/* 初始化一个布局兼容的类型化状态游标。 */
void __xrtHttpStructuredStatusCursorInit(
	void* pCursor,
	size_t iCursorSize
);



/* 初始化一个布局兼容的类型化重复字段游标。 */
void __xrtHttpStructuredStatusFieldCursorInit(
	void* pCursor,
	size_t iCursorSize
);



/* 验证完整 List 和每个协议成员。 */
bool __xrtHttpStructuredStatusValid(
	xstrview Value,
	xrt_http_structured_status_read pRead
);



/* 以完整预校验和失败原子性迭代一个类型化状态 List。 */
xhttpnext __xrtHttpStructuredStatusNext(
	xstrview Value,
	void* pCursor,
	size_t iCursorSize,
	void* pOutput,
	size_t iOutputSize,
	xrt_http_structured_status_read pRead
);



/* 跨重复字段行迭代一个类型化状态 List。 */
xhttpnext __xrtHttpStructuredStatusFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	void* pCursor,
	size_t iCursorSize,
	void* pOutput,
	size_t iOutputSize,
	xrt_http_structured_status_read pRead
);

#endif

#endif
