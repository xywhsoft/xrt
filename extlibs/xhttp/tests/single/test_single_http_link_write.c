#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_LINK_WRITE
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"



/* 单头发布必须保留 Link 规范写出。 */
int main(void)
{
	static const xhttplinkvalue Link = {
		XRT_STR_INIT("/next"),
		XRT_STR_INIT("next alternate"),
		NULL,
		0
	};
	char sOutput[64];
	size_t iSize;

	return xrtHttpLinkElementWrite(
		&Link, sOutput, sizeof(sOutput), &iSize
	) && (iSize != 0) ? 0 : 1;
}
