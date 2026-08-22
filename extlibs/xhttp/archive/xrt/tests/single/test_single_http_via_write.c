#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 Via 规范写出。 */
int main(void)
{
	static const xhttpviavalue Via = {
		XRT_STR_INIT(""),
		XRT_STR_INIT("1.1"),
		XRT_STR_INIT("edge"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		0
	};
	char sOutput[16];
	size_t iSize;

	return xrtHttpViaWrite(
		&Via, 1u, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 8u) &&
		(memcmp(sOutput, "1.1 edge", 8u) == 0) ? 0 : 1;
}
