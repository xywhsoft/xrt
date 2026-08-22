#ifndef XRT_INTERNAL_HTTP_SSE_CLIENT_H
#define XRT_INTERNAL_HTTP_SSE_CLIENT_H

#include "xrt_http_client_runtime.h"
#include <xrt/http_sse_client.h>



#if defined(XHTTP_FEATURE_HTTP_SSE_CLIENT)

/* 当前响应分类只由所属 HTTP Worker 推进。 */
typedef enum xrt_http_sse_attempt {
	XRT_HTTP_SSE_ATTEMPT_WAITING = 0,
	XRT_HTTP_SSE_ATTEMPT_OPEN,
	XRT_HTTP_SSE_ATTEMPT_STOP,
	XRT_HTTP_SSE_ATTEMPT_REJECTED,
	XRT_HTTP_SSE_ATTEMPT_FAILED
} xrt_http_sse_attempt;



/* 会话不预分配正文区；Pending 只保存一次暂停点后的当前传输尾段。 */
struct xhttpsseclient {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CloseGate;
	xatomic32 FinishGate;
	xatomic32 PauseGate;
	xatomic32 ResumeGate;
	xatomic32 RequestedReason;
	xatomic32 Status;
	xatomic64 Retry;
	xatomic64 Messages;
	xatomic64 Comments;
	xatomic64 RetryUpdates;
	xatomic64 Reconnects;
	xrt_spinlock Lock;
	xhttpclient* Http;
	xhttprequest* Request;
	xhttpcall* Call;
	xcancel* Cancel;
	xcancelwatch* CancelWatch;
	xnetworker* Worker;
	xnetengine* Engine;
	uint64 Timer;
	xnetpost ClosePost;
	xhttpsseclientconfig Config;
	xhttpsseclientevents Events;
	xhttpsseparser Parser;
	xbuffer Pending;
	xerror* Error;
	xerror* AttemptError;
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_COOKIES)
		str CookiePartition;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_CACHE)
		str CachePartition;
	#endif
	#if defined(XHTTP_FEATURE_HTTP_CLIENT_PROXY)
		xnetproxy* Proxy;
	#endif
	xrt_http_sse_attempt Attempt;
	xhttpsseclosereason AttemptReason;
	bool RuntimeHeld;
	bool Constructing;
};

#endif

#endif
