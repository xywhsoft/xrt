#include <stdio.h>

#include <xrt.h>



/* 使用显式 salt 和工作因子派生 32 字节密码密钥。 */
int main(void)
{
	uint8 arrKey[32];
	static const uint8 Salt[16] = {
		0x8Du, 0xCBu, 0x89u, 0x2Eu, 0xD0u, 0x31u, 0x42u, 0x49u,
		0xA1u, 0x7Eu, 0x1Fu, 0x5Bu, 0x12u, 0x6Du, 0xE9u, 0x75u
	};

	if ( !xrtPbkdf2Sha256(
			"correct horse battery staple", 28,
			Salt, sizeof(Salt), 100000, arrKey, sizeof(arrKey)
		) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(arrKey); i++ ) {
		printf("%02x", (unsigned int)arrKey[i]);
	}
	printf("\n");
	xrtSecureZero(arrKey, sizeof(arrKey));
	return 0;
}
