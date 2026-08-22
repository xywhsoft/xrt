#include <stdio.h>

#include <xrt.h>



/* 一行派生 32 字节会话密钥。 */
int main(void)
{
	uint8 arrKey[32];

	if ( !xrtHkdfSha256(
			"salt", 4, "input key material", 18,
			"session", 7, arrKey, sizeof(arrKey)
		) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	return 0;
}
