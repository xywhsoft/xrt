#include <stdio.h>

#include <xrt/tls.h>



/* 展示 SHA-384 与 ChaCha20-Poly1305 可选后端对应的公开密码套件元数据。 */
int main(void)
{
	static const xtlscipher Ciphers[] = {
		XTLS_AES_256_GCM_SHA384,
		XTLS_CHACHA20_POLY1305_SHA256
	};
	size_t i;

	for ( i = 0; i < (sizeof(Ciphers) / sizeof(Ciphers[0])); ++i ) {
		const xtlscipherinfo* pInfo = xrtTlsCipherInfo(
			Ciphers[i]
		);

		if ( pInfo == NULL ) {
			return 1;
		}
		printf(
			"%s hash=%d aead=%d key=%u\n",
			xrtTlsCipherName(Ciphers[i]),
			(int)pInfo->Hash,
			(int)pInfo->Aead,
			(unsigned)pInfo->KeySize
		);
	}
	return 0;
}
