#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头 Future 入口必须拒绝空 Listener。 */
int main(void)
{
	xfuture* pFuture;

	xrtClearError();
	pFuture = xrtTlsListenerAcceptAsync(NULL);
	if ( (pFuture != NULL) ||
		(xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) ) {
		return 1;
	}
	return 0;
}
