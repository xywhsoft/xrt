#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



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
