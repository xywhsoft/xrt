#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件保留完整 Cache-Control 汇总。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Cache-Control"),
			XRT_STR_INIT(
				"max-age=60, no-transform, x-mode=fast"
			)
		}
	};
	xhttpcachecontrol Control;
	bool bPass;

	bPass = xrtHttpCacheControlParse(
		Fields, 1, &Control
	) && (Control.MaxAge == 60) &&
		(Control.UnknownCount == 1) &&
		((Control.Flags &
		  XHTTP_CACHE_NO_TRANSFORM) != 0);
	printf(
		"%s single-http-cache\n",
		bPass ? "[PASS]" : "[FAIL]"
	);
	return bPass ? 0 : 1;
}
