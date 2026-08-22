#ifndef XRT_HTTP_RETRY_H
#define XRT_HTTP_RETRY_H

#include <xrt/http.h>
#include <xrt/time.h>



#if defined(XHTTP_FEATURE_HTTP_RETRY) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_TIME_TEXT))
	#error "XRT HTTP retry support requires HTTP and time text support"
#endif



#if defined(XHTTP_FEATURE_HTTP_RETRY)

/* Retry-After 可以使用相对秒数，也可以使用绝对 HTTP 日期。 */
typedef enum xhttpretryafterkind {
	XHTTP_RETRY_AFTER_NONE = 0,
	XHTTP_RETRY_AFTER_DELAY,
	XHTTP_RETRY_AFTER_DATE
} xhttpretryafterkind;



/* 解析结果保留线路值，不提前绑定墙钟或单调时钟。 */
typedef struct xhttpretryafter {
	xhttpretryafterkind Kind;
	uint64 Seconds;
	xtime Date;
} xhttpretryafter;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XHTTP_FEATURE_HTTP_RETRY)

/*
	严格解析完整 Retry-After 值，两端允许 OWS。
	语法或数值失败时清空输出；参数错误不修改输出。
	delay-seconds 超出 uint64 时报告溢出。
*/
XRT_API bool xrtHttpRetryAfterParse(
	xstrview Value,
	xhttpretryafter* pRetry
);



/*
	把 Retry-After 转换为当前应等待的微秒数。
	过去的绝对日期得到零；转换溢出时不修改输出。
*/
XRT_API bool xrtHttpRetryAfterDelay(
	const xhttpretryafter* pRetry,
	xtime iNow,
	uint64* pDelay
);



/*
	读取唯一 Retry-After 字段并转换为微秒数。
	缺失返回 END，唯一有效值返回 ITEM，重复或非法值返回 ERROR。
	缺失和错误都把输出清零。
*/
XRT_API xhttpnext xrtHttpRetryAfterFields(
	const xhttpfield* pFields,
	size_t iCount,
	xtime iNow,
	uint64* pDelay
);



/*
	规范写出 Retry-After 值，不附加零字节。
	空输出用于查询精确长度；短缓冲返回所需长度且不写入部分结果。
*/
XRT_API bool xrtHttpRetryAfterWrite(
	const xhttpretryafter* pRetry,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 Retry-After 值；返回值由 xrtFree 释放。 */
XRT_API str xrtHttpRetryAfterBuild(
	const xhttpretryafter* pRetry,
	size_t* pSize
);



/* 判断状态码是否属于默认的临时失败重试集合。 */
XRT_API bool xrtHttpRetryStatusDefault(uint16 iStatus);



/*
	计算以零开始编号的封顶指数退避，单位由调用方决定。
	Maximum 必须非零且 Base 不得大于 Maximum；失败时不修改输出。
*/
XRT_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
);

#endif



XRT_EXTERN_C_END

#endif


