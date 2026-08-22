#include <stdio.h>

#include <xrt.h>



/* 演示可复制、可复现且不依赖全局状态的显式随机路径。 */
int main(void)
{
	xrng Rng;
	int arrOrder[] = { 1, 2, 3, 4, 5, 6 };
	uint8 arrBytes[8];

	xrtRngSeed(&Rng, 2026, 7);
	printf("explicit: %u\n", (unsigned int)xrtRng32(&Rng));
	printf("dice    : %lld\n", (long long)xrtRngRangeClosed(&Rng, 1, 6));
	printf("real    : %.12f\n", xrtRngReal(&Rng));
	if ( !xrtRngBytes(&Rng, arrBytes, sizeof(arrBytes)) ||
		 !xrtRngShuffle(&Rng, arrOrder,
			sizeof(arrOrder) / sizeof(arrOrder[0]), sizeof(arrOrder[0])) ) {
		return 1;
	}
	printf("bytes   : %02x%02x\n",
		(unsigned int)arrBytes[0], (unsigned int)arrBytes[1]);
	printf("shuffle : %d %d %d\n", arrOrder[0], arrOrder[1], arrOrder[2]);
	return 0;
}
