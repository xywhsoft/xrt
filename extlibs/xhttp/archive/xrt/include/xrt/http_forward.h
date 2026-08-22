#ifndef XRT_HTTP_FORWARD_H
#define XRT_HTTP_FORWARD_H

#include <xrt/http_connection.h>



#if defined(XRT_FEATURE_HTTP_FORWARD) && \
	!defined(XRT_FEATURE_HTTP_CONNECTION)
	#error "XRT HTTP forwarding support requires Connection support"
#endif



#if defined(XRT_FEATURE_HTTP_FORWARD)

/* Max-Forwards 更新结果区分非法值、本节点处理和继续转发。 */
typedef enum xhttpforwardstatus {
	XHTTP_FORWARD_ERROR = -1,
	XHTTP_FORWARD_FINAL = 0,
	XHTTP_FORWARD_NEXT = 1
} xhttpforwardstatus;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_FORWARD)

/* 判断字段名是否属于 RFC 9110 要求中间节点移除的固定逐跳集合。 */
XRT_API bool xrtHttpHopFieldKnown(xstrview Name);



/*
	判断字段名是固定逐跳字段或被 Connection 提名。
	返回 ITEM 表示逐跳，END 表示端到端，ERROR 表示参数或 Connection 非法。
*/
XRT_API xhttpnext xrtHttpHopField(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);



/* 严格解析非空十进制 Max-Forwards，溢出时失败并把输出保持为零。 */
XRT_API bool xrtHttpMaxForwardsParse(
	xstrview Value,
	uint64* pForwards
);



/*
	按 RFC 9110 更新 Max-Forwards。
	零值返回 FINAL；非零值把 min(Value - 1, Maximum) 写入输出并返回 NEXT。
*/
XRT_API xhttpforwardstatus xrtHttpMaxForwardsUpdate(
	xstrview Value,
	uint64 iMaximum,
	uint64* pNext
);



/* 规范写出十进制 Max-Forwards；空输出可精确查询长度。 */
XRT_API bool xrtHttpMaxForwardsWrite(
	uint64 iForwards,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);

#endif



XRT_EXTERN_C_END

#endif
