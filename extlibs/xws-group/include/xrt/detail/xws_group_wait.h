#ifndef XWS_GROUP_DETAIL_WAIT_H
#define XWS_GROUP_DETAIL_WAIT_H
#include <xws-group.h>
#include <xrt/detail/wait.h>

XRT_EXTERN_C_BEGIN
#if (defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE))
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntil(
	xwsgroupop* pOperation,
	double iDeadline
);
#endif
#if (defined(XWS_GROUP_FEATURE_WEBSOCKET_GROUP_FUTURE))
XWS_GROUP_API xwaitresult __xrtWsGroupOpWaitUntilCancel(
	xwsgroupop* pOperation,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
