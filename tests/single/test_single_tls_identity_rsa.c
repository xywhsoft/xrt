#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/tls_identity_legacy.h"



/* 验证单头文件中的旧版 RSA 身份资产和实际签名路径。 */
int main(void)
{
	static const uint8 Message[] = { 1, 2, 3 };
	uint8 PrivateDer[2048];
	uint8 Signature[XRT_RSA_MODULUS_MAX_SIZE];
	size_t iPrivateSize = 0;
	size_t iSignatureSize = 0;
	xbytesview Chain = {
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT)
	};
	xtlsidentity* pIdentity;

	if ( !testTlsIdentityLegacyKey(
		PrivateDer, sizeof(PrivateDer), &iPrivateSize
	) ) {
		return 1;
	}
	pIdentity = xrtTlsIdentityRsa(
		&Chain, 1u, (xbytesview) { PrivateDer, iPrivateSize }
	);
	if ( (pIdentity == NULL) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_RSA_PSS_RSAE_SHA256,
		(xbytesview) { Message, sizeof(Message) },
		Signature, sizeof(Signature), &iSignatureSize
	) || (iSignatureSize != 256u) ) {
		xrtTlsIdentityRelease(pIdentity);
		return 1;
	}
	xrtTlsIdentityRelease(pIdentity);
	return 0;
}
