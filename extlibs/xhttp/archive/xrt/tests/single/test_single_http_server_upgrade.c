#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件构建必须公开 Upgrade 值与参数错误。 */
int main(void)
{
	xhttpupgrade Upgrade;

	memset(&Upgrade, 0, sizeof(Upgrade));
	xrtHttpUpgradeAbort(&Upgrade);
	return (xrtHttpConnUpgradeRaw(
		NULL,
		XRT_BYTES_LITERAL("HTTP/1.1 101 Switching Protocols\r\n\r\n"),
		NULL,
		NULL
	) == XNET_RESULT_ERROR) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) ?
			0 : 1;
}
