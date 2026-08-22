#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头同步入口必须拒绝空 Listener。 */
int main(void)
{
	xrtClearError();
	if ( (xrtTlsListenerAcceptWait(
		NULL,
		XRT_DEADLINE_NEVER,
		NULL
	) != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}
