#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Upgrade 规范写出。 */
int main(void)
{
	static const xhttpupgradeitem Upgrade = {
		XRT_STR_INIT("HTTP"), XRT_STR_INIT("2.0")
	};
	char sOutput[16];
	size_t iSize;

	return xrtHttpUpgradeElementWrite(
		&Upgrade, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 8u) ? 0 : 1;
}
