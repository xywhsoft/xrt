#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 单头发布必须保留 Forwarded 规范写出。 */
int main(void)
{
	static const xhttpforwardedvalue Element = {
		XRT_STR_INIT("[2001:db8::1]:443"),
		XRT_STR_INIT(""),
		XRT_STR_INIT(""),
		XRT_STR_INIT("https"),
		NULL,
		0,
		XHTTP_FORWARDED_HAS_FOR |
		XHTTP_FORWARDED_HAS_PROTO
	};
	char sOutput[96];
	size_t iSize;

	return xrtHttpForwardedElementWrite(
		&Element, sOutput, sizeof(sOutput), &iSize
	) && (iSize != 0) ? 0 : 1;
}
