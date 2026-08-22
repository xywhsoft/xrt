#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须公开 Future 类型、条件和参数拒绝契约。 */
int main(void)
{
	xfuture* pFuture = xrtWsConnWaitAsync(
		NULL,
		XWS_CONN_WAIT_CLOSE
	);

	if ( (pFuture != NULL) ||
		(xrtErrorCode(xrtGetError()) !=
		 XWS_CONN_ERROR_ARGUMENT) ) {
		return 1;
	}
	xrtClearError();
	return 0;
}
