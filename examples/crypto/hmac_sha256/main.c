#include <stdio.h>

#include <xrt.h>



/* 用一次性入口计算请求正文的 HMAC-SHA256。 */
int main(void)
{
	uint8 arrMac[XRT_SHA256_SIZE];

	if ( !xrtHmacSha256("secret", 6, "message", 7, arrMac) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrMac); i++ ) {
		printf("%02x", (unsigned int)arrMac[i]);
	}
	printf("\n");
	return 0;
}
