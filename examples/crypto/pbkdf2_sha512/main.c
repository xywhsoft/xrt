#include <stdio.h>

#include <xrt.h>



/* 使用 PBKDF2-HMAC-SHA512 派生 64 字节密码密钥。 */
int main(void)
{
	uint8 arrKey[64];
	static const uint8 Salt[16] = {
		0x28u, 0x98u, 0xF0u, 0x1Cu, 0x3Du, 0x8Eu, 0xC6u, 0x57u,
		0x17u, 0xC9u, 0x80u, 0xE4u, 0xA6u, 0x3Bu, 0x72u, 0x11u
	};

	if ( !xrtPbkdf2Sha512(
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
