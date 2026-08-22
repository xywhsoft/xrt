#include <stdio.h>

#include <xrt.h>



/* 用对称的流 API 分别计算 SHA-384 和 SHA-512。 */
int main(void)
{
	xsha384 State;
	uint8 arrDigest[XRT_SHA512_SIZE];

	xrtSha384Init(&State);
	if ( !xrtSha384Update(&State, "hello ", 6) ||
		 !xrtSha384Update(&State, "world", 5) ||
		 !xrtSha384Final(&State, arrDigest) ) {
		return 1;
	}
	printf("SHA-384: ");
	for ( size_t i = 0; i < XRT_SHA384_SIZE; i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	if ( !xrtSha512("hello world", 11, arrDigest) ) {
		return 1;
	}
	printf("SHA-512: ");
	for ( size_t i = 0; i < XRT_SHA512_SIZE; i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
