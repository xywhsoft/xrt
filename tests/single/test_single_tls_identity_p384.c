#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#define TEST_TLS_IDENTITY_FIXTURE_EC
#include "../fixtures/tls_identity_ec_ed.h"



/* 验证单头文件中的 P-384 身份构造和签名路径。 */
int main(void)
{
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Certificate[512];
	uint8 Sec1[224];
	uint8 Pkcs8[320];
	uint8 Signature[112];
	static const uint8 Message[] = { 'x' };
	size_t iCertificateSize = 0;
	size_t iSec1Size = 0;
	size_t iPkcs8Size = 0;
	size_t iSignatureSize = 0;
	xbytesview Chain;
	xtlsidentity* pIdentity;

	Private[sizeof(Private) - 1u] = 1u;
	if ( !xrtP384Public(Private, Public) ||
		!testTlsIdentityEcCertificate(
			Public, sizeof(Public), Certificate,
			sizeof(Certificate), &iCertificateSize
		) || !testTlsIdentityEcSec1(
			Private, sizeof(Private), Public,
			Sec1, sizeof(Sec1), &iSec1Size
		) || !testTlsIdentityEcPkcs8(
			Sec1, iSec1Size, sizeof(Private),
			Pkcs8, sizeof(Pkcs8), &iPkcs8Size
		) ) {
		return 1;
	}
	Chain = (xbytesview) { Certificate, iCertificateSize };
	pIdentity = xrtTlsIdentityP384(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	if ( (pIdentity == NULL) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
		(xbytesview) { Message, sizeof(Message) }, Signature,
		sizeof(Signature), &iSignatureSize
	) ) {
		xrtTlsIdentityRelease(pIdentity);
		return 1;
	}
	xrtTlsIdentityRelease(pIdentity);
	return 0;
}
