#include <stdio.h>

#include <xrt.h>



/* 按应用策略选择对端提供的 TLS 版本和密码套件。 */
int main(void)
{
	static const uint8 VersionData[] = {
		0x03, 0x04, 0x03, 0x03
	};
	static const uint8 CipherData[] = {
		0x13, 0x01, 0x13, 0x02, 0xC0, 0x2F
	};
	static const xtlsversion Versions[] = {
		XTLS_VERSION_13, XTLS_VERSION_12
	};
	static const xtlscipher Ciphers[] = {
		XTLS_CHACHA20_POLY1305_SHA256,
		XTLS_AES_128_GCM_SHA256,
		XTLS_AES_256_GCM_SHA384,
		XTLS_ECDHE_RSA_AES_128_GCM_SHA256
	};
	xtlsids OfferedVersions = {
		{ VersionData, sizeof(VersionData) }
	};
	xtlsids OfferedCiphers = {
		{ CipherData, sizeof(CipherData) }
	};
	xtlsversion Version;
	xtlscipher Cipher;

	if ( xrtTlsVersionSelect(
		&OfferedVersions, Versions, 2, &Version
	) != XTLS_ITEM_VALUE ) {
		return 1;
	}
	if ( xrtTlsCipherSelect(
		Version, &OfferedCiphers, XTLS_IDENTITY_RSA,
		Ciphers, sizeof(Ciphers) / sizeof(Ciphers[0]), &Cipher
	) != XTLS_ITEM_VALUE ) {
		return 1;
	}
	printf(
		"version=%s cipher=%s\n",
		xrtTlsVersionName(Version), xrtTlsCipherName(Cipher)
	);
	return 0;
}
