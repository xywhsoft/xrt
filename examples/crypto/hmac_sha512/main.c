#include <stdio.h>

#include <xrt.h>



/* 复用预计算密钥状态，流式计算 HMAC-SHA384。 */
int main(void)
{
	xhmacsha384 State;
	uint8 arrMac[XRT_SHA384_SIZE];

	if ( !xrtHmacSha384Init(&State, "secret", 6) ||
		 !xrtHmacSha384Update(&State, "hello ", 6) ||
		 !xrtHmacSha384Update(&State, "world", 5) ||
		 !xrtHmacSha384Final(&State, arrMac) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrMac); i++ ) {
		printf("%02x", (unsigned int)arrMac[i]);
	}
	printf("\n");
	return 0;
}
