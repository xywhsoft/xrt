#include <stdio.h>

#include <xrt.h>



/* 演示无需运行时附着的当前线程随机便捷接口。 */
int main(void)
{
	xrtRandSeed(2026, 7);
	printf("value: %u\n", (unsigned int)xrtRand32());
	printf("dice : %lld\n", (long long)xrtRandRangeClosed(1, 6));
	printf("real : %.12f\n", xrtRandReal());
	return 0;
}
