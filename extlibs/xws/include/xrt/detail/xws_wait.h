#ifndef XRT_DETAIL_XWS_WAIT_H
#define XRT_DETAIL_XWS_WAIT_H
#include <xws.h>
#include <xrt/detail/wait.h>

XRT_EXTERN_C_BEGIN
#if (defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE))
XRT_API xwaitresult __xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	double iDeadline
);
#endif
#if (defined(XWS_FEATURE_WEBSOCKET_GROUP_FUTURE))
XRT_API xwaitresult __xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
