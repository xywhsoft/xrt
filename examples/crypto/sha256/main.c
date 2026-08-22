#include <stdio.h>

#include <xrt.h>



/* 分块计算 SHA-256，并从不消耗状态的快照取得摘要。 */
int main(void)
{
	xsha256 State;
	uint8 arrDigest[XRT_SHA256_SIZE];

	xrtSha256Init(&State);
	if ( !xrtSha256Update(&State, "hello ", 6) ||
		 !xrtSha256Update(&State, "world", 5) ||
		 !xrtSha256Final(&State, arrDigest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrDigest); i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
