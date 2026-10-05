#define XHTTP_MODULE_HTTP_SERVER_MUX_TLS
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 单头文件 Mux TLS 入口必须保留稳定失败与清理语义。 */
int main(void)
{
	xhttpservermux* pMux = xrtHttpServerMuxCreate(NULL);

	if ( (pMux == NULL) ||
		(xrtHttpServerMuxStartTls(
			NULL, NULL, NULL, pMux, NULL
		 ) != NULL) ||
		(xrtErrorCode(xrtGetError()) !=
		 (int32)XHTTP_SERVER_MUX_ERROR_START) ) {
		xrtHttpServerMuxDestroy(pMux);
		return 1;
	}
	xrtClearError();
	xrtHttpServerMuxDestroy(pMux);
	puts("[PASS] single HTTPS server mux");
	return 0;
}
