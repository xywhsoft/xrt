#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_BODY
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 验证固定正文的拥有、查看和释放主路径。 */
int main(void)
{
	xhttpbody* pBody = xrtHttpBodyCopy((xbytesview){
		(cbytes)"payload", 7u
	});
	xbytesview View;
	int iResult = 0;

	if ( (pBody == NULL) || !xrtHttpBodyView(pBody, &View) ||
		(View.Size != 7u) || (memcmp(View.Data, "payload", 7u) != 0) ) {
		iResult = 1;
	}
	xrtHttpBodyDestroy(pBody);
	return iResult;
}
