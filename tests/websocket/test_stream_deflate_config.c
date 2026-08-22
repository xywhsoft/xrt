#include "../test.h"



/* 验证压缩 Connection 的默认配置和未协商调用边界。 */
int main(void)
{
	xwsstreamconfig Config;
	xwsdeflate Deflate;

	memset(&Deflate, 0xA5, sizeof(Deflate));
	xrtWsStreamConfigInit(&Config);
	testRequire(
		!Config.DeflateEnabled &&
		(Config.Deflate.Flags == 0) &&
		(Config.Deflate.ServerMaxWindowBits ==
		 XWS_DEFLATE_WINDOW_MAX) &&
		(Config.Deflate.ClientMaxWindowBits ==
		 XWS_DEFLATE_WINDOW_MAX) &&
		(Config.Inflater.WindowBits ==
		 XWS_DEFLATE_WINDOW_MAX) &&
		(Config.Deflater.WindowBits ==
		 XWS_DEFLATE_WINDOW_MAX),
		"WebSocket compressed Connection defaults mismatch"
	);
	testRequire(
		!xrtWsStreamDeflate(NULL, &Deflate) &&
		!xrtWsStreamDeflate(NULL, NULL) &&
		(Deflate.Flags == UINT32_C(0xA5A5A5A5)),
		"WebSocket compressed Connection null query mismatch"
	);
	xrtClearError();
	printf("[PASS] WebSocket compressed Connection config\n");
	return 0;
}
