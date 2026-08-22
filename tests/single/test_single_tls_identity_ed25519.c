#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../test.h"
#include "../crypto/test_crypto_digest.h"

#define TEST_TLS_IDENTITY_FIXTURE_ED25519
#include "../fixtures/tls_identity_ec_ed.h"



/* 验证单头文件中的 Ed25519 身份构造和签名路径。 */
int main(void)
{
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Certificate[512];
	uint8 Pkcs8[64];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	static const uint8 Message[] = { 'x' };
	size_t iCertificateSize = 0;
	size_t iPkcs8Size = 0;
	size_t iSignatureSize = 0;
	xbytesview Chain;
	xtlsidentity* pIdentity;

	testCryptoDecode(
		Seed, sizeof(Seed),
		"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
		"single Ed25519 seed mismatch"
	);
	if ( !xrtEd25519Public(Seed, Public) ||
		!testTlsIdentityEdCertificate(
			Public, Certificate, sizeof(Certificate), &iCertificateSize
		) || !testTlsIdentityEdPkcs8(
			Seed, Pkcs8, sizeof(Pkcs8), &iPkcs8Size
		) ) {
		return 1;
	}
	Chain = (xbytesview) { Certificate, iCertificateSize };
	pIdentity = xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	if ( (pIdentity == NULL) || !xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13, XTLS_SIGNATURE_ED25519,
		(xbytesview) { Message, sizeof(Message) }, Signature,
		sizeof(Signature), &iSignatureSize
	) ) {
		xrtTlsIdentityRelease(pIdentity);
		return 1;
	}
	xrtTlsIdentityRelease(pIdentity);
	return 0;
}
