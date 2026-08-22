#include <stdio.h>

#include <xrt.h>



/* 计算 WebSocket 握手等兼容协议仍需要的 SHA-1 摘要。 */
int main(void)
{
	uint8 arrDigest[XRT_SHA1_SIZE];

	if ( !xrtSha1("hello", 5, arrDigest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrDigest); i++ ) {
		printf("%02x", (unsigned int)arrDigest[i]);
	}
	printf("\n");
	return 0;
}
