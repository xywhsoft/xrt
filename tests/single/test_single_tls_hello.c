#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供严格 TLS Hello 解析。 */
int main(void)
{
	static const uint8 Body[] = {
		3, 3,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0,
		0, 2, 0x13, 0x01,
		1, 0
	};
	xtlsclienthello Hello;

	return xrtTlsClientHelloParse(
		(xbytesview) { Body, sizeof(Body) }, &Hello
	) && (xrtTlsIdsCount(&Hello.CipherSuites) == 1u) ? 0 : 1;
}
