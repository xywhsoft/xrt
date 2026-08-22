#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头文件中的 TLS 无状态协商。 */
int main(void)
{
	static const uint8 OfferedData[] = {
		0x13, 0x01, 0x13, 0x02
	};
	static const xtlscipher Preferred[] = {
		XTLS_AES_256_GCM_SHA384,
		XTLS_AES_128_GCM_SHA256
	};
	xtlsids Offered = { { OfferedData, sizeof(OfferedData) } };
	xtlscipher Cipher = (xtlscipher)0;
	const xtlssignatureinfo* pSignature;

	if ( xrtTlsCipherSelect(
		XTLS_VERSION_13, &Offered, XTLS_IDENTITY_NONE,
		Preferred, 2, &Cipher
	) != XTLS_ITEM_VALUE ) {
		return 1;
	}
	if ( Cipher != XTLS_AES_256_GCM_SHA384 ) {
		return 1;
	}
	pSignature = xrtTlsSignatureInfo(XTLS_SIGNATURE_ED25519);
	if ( (pSignature == NULL) ||
		(pSignature->Identity != XTLS_IDENTITY_ED25519) ||
		(pSignature->HashSize != 0u) ||
		(pSignature->Maximum != XTLS_VERSION_13) ) {
		return 1;
	}
	printf("[PASS] single-tls-negotiate\n");
	return 0;
}
