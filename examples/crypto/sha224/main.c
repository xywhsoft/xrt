#include <stdio.h>

#include <xrt.h>



/* 演示一次计算 SHA-224 摘要。 */
int main(void)
{
	uint8 Digest[XRT_SHA224_SIZE];

	if ( !xrtSha224("xrt", 3, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		printf("%02x", (unsigned)Digest[i]);
	}
	putchar('\n');
	return 0;
}
