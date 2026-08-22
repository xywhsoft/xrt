#include "../test.h"

#include <xrt/http_origin.h>



/* 确定性伪随机数让线路往返失败可以稳定复现。 */
static uint32 testOriginRandom(uint32* pState)
{
	*pState = (*pState * UINT32_C(1664525)) +
		UINT32_C(1013904223);
	return *pState;
}



/* 随机 Origin 必须满足写出、回读和同源比较闭环。 */
int main(void)
{
	static const cstr Schemes[] = {
		"http", "https", "ws", "wss", "custom"
	};
	char sInput[96];
	char sOutput[96];
	xhttporigin Input;
	xhttporigin Output;
	uint32 iState = UINT32_C(0x7294A13D);
	size_t iSize;
	size_t i;

	for ( i = 0; i < 6000u; i++ ) {
		cstr sScheme = Schemes[
			testOriginRandom(&iState) %
			(sizeof(Schemes) / sizeof(Schemes[0]))
		];
		uint32 iHost = testOriginRandom(&iState) % 100000u;
		uint32 iPort = testOriginRandom(&iState) % 65536u;
		bool bPort = (testOriginRandom(&iState) & 1u) != 0;
		int iWritten;

		if ( bPort ) {
			iWritten = snprintf(
				sInput, sizeof(sInput), "%s://Node-%u.Test:%u",
				sScheme, iHost, iPort
			);
		} else {
			iWritten = snprintf(
				sInput, sizeof(sInput), "%s://Node-%u.Test",
				sScheme, iHost
			);
		}
		testRequire(
			(iWritten > 0) &&
			xrtHttpOriginParse(
				(xstrview){ sInput, (size_t)iWritten }, &Input
			) && xrtHttpOriginWrite(
				&Input, sOutput, sizeof(sOutput), &iSize
			) && xrtHttpOriginParse(
				(xstrview){ sOutput, iSize }, &Output
			) && xrtHttpOriginSame(&Input, &Output),
			"Origin randomized round trip failed"
		);
	}
	printf("[PASS] http_origin_mutation\n");
	return 0;
}
