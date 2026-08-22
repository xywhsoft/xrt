#ifndef XRT_HTTP_PRIORITY_H
#define XRT_HTTP_PRIORITY_H

#include <xrt/http_structured.h>



#if defined(XHTTP_FEATURE_HTTP_PRIORITY) && \
	!defined(XHTTP_FEATURE_HTTP_STRUCTURED)
	#error "XRT HTTP priority requires Structured Fields support"
#endif

#if defined(XHTTP_FEATURE_HTTP_PRIORITY_WRITE) && \
	(!defined(XHTTP_FEATURE_HTTP_PRIORITY) || \
	 !defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE))
	#error "XRT HTTP priority writer requires priority and Structured Fields writer support"
#endif



#if defined(XHTTP_FEATURE_HTTP_PRIORITY)

#define XHTTP_PRIORITY_URGENCY_DEFAULT 3u
#define XHTTP_PRIORITY_URGENCY_MAX 7u



/* 标志位区分有效值与线路中显式出现的参数。 */
typedef enum xhttppriorityflag {
	XHTTP_PRIORITY_HAS_URGENCY = 0x01,
	XHTTP_PRIORITY_HAS_INCREMENTAL = 0x02
} xhttppriorityflag;



/* RFC 9218 优先级；未设置标志的参数使用协议默认值。 */
typedef struct xhttppriority {
	uint8 Urgency;
	uint8 Incremental;
	uint8 Flags;
} xhttppriority;



XRT_EXTERN_C_BEGIN



/* 初始化请求语义的默认优先级，并清除显式参数标志。 */
XRT_API void xrtHttpPriorityInit(xhttppriority* pPriority);



/* 严格解析一个 Priority 字段值；未知或无效的参数会被忽略。 */
XRT_API bool xrtHttpPriorityValueParse(
	xstrview Value,
	xhttppriority* pPriority
);



/* 解析全部重复 Priority 字段行；缺失字段产生协议默认值。 */
XRT_API bool xrtHttpPriorityParse(
	const xhttpfield* pFields,
	size_t iCount,
	xhttppriority* pPriority
);



/* 用 Update 中显式出现的参数覆盖 Base，其余参数保持不变。 */
XRT_API bool xrtHttpPriorityOverlay(
	const xhttppriority* pBase,
	const xhttppriority* pUpdate,
	xhttppriority* pPriority
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_HTTP_PRIORITY_WRITE)

XRT_EXTERN_C_BEGIN



/* 规范写出显式 Priority 参数；输出不附加零字节。 */
XRT_API bool xrtHttpPriorityWrite(
	const xhttppriority* pPriority,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
