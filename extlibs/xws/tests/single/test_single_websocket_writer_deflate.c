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



#include <stdio.h>



/* 单头文件必须独立导出压缩 Writer 与输出上界接口。 */
int main(void)
{
	size_t iBound = 0;

	if ( !xrtWsDeflaterBound(1024u, &iBound) ||
		(iBound <= 1024u) ) {
		return 1;
	}
	if ( xrtWsConnBeginBinaryCompressed(NULL) != NULL ) {
		return 2;
	}
	xrtClearError();
	printf("[PASS] single WebSocket compressed Writer\n");
	return 0;
}
