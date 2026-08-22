#include <stdio.h>

#include <xrt.h>



/* 演示显式 RNG 生成可复现的非安全随机文本。 */
int main(void)
{
	xrng Rng;
	str sText;

	xrtRngSeed(&Rng, 2026, 7);
	sText = xrtRngString(&Rng, 24);
	if ( sText == NULL ) {
		return 1;
	}
	printf("%s\n", sText);
	xrtFree(sText);
	return 0;
}
