#include "../test.h"
#include "../crypto/test_crypto_digest.h"

#define TEST_TLS_IDENTITY_FIXTURE_ED25519
#include "../fixtures/tls_identity_ec_ed.h"



/* 验证旧版原始种子路径、标准 PKCS#8 和真实 Ed25519 协议签名。 */
int main(void)
{
	static const uint8 Message[] = "xrt ed25519 identity";
	uint8 Seed[XRT_ED25519_SEED_SIZE];
	uint8 Public[XRT_ED25519_PUBLIC_SIZE];
	uint8 Certificate[512];
	uint8 Pkcs8[64];
	uint8 Nested[36];
	uint8 Signature[XRT_ED25519_SIGNATURE_SIZE];
	size_t iCertificateSize = 0;
	size_t iPkcs8Size = 0;
	size_t iSignatureSize = 0;
	xbytesview Chain;
	xtlsidentity* pIdentity;

	testCryptoDecode(
		Seed, sizeof(Seed),
		"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
		"Ed25519 identity seed vector size mismatch"
	);
	testRequire(xrtEd25519Public(Seed, Public) &&
		testTlsIdentityEdCertificate(
			Public, Certificate, sizeof(Certificate), &iCertificateSize
		) && testTlsIdentityEdPkcs8(
			Seed, Pkcs8, sizeof(Pkcs8), &iPkcs8Size
		), "Ed25519 identity fixtures failed");
	Chain = (xbytesview) { Certificate, iCertificateSize };
	pIdentity = xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Seed, sizeof(Seed) }
	);
	testRequire((pIdentity != NULL) &&
		(xrtTlsIdentityType(pIdentity) == XTLS_IDENTITY_ED25519) &&
		xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_12, XTLS_SIGNATURE_ED25519
		) && xrtTlsIdentitySign(
			pIdentity, XTLS_VERSION_13, XTLS_SIGNATURE_ED25519,
			(xbytesview) { Message, sizeof(Message) - 1u },
			Signature, sizeof(Signature), &iSignatureSize
		) && (iSignatureSize == sizeof(Signature)) && xrtEd25519Verify(
			Public, Message, sizeof(Message) - 1u, Signature
		), "Ed25519 identity signing failed");
	xrtTlsIdentityRelease(pIdentity);
	pIdentity = xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	testRequire(pIdentity != NULL, "Ed25519 PKCS#8 identity failed");
	xrtTlsIdentityRelease(pIdentity);
	Nested[0] = 0x04;
	Nested[1] = 0x22;
	Nested[2] = 0x04;
	Nested[3] = 0x20;
	memcpy(Nested + 4u, Seed, sizeof(Seed));
	pIdentity = xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Nested, sizeof(Nested) }
	);
	testRequire(pIdentity != NULL,
		"Ed25519 legacy nested OCTET identity failed");
	xrtTlsIdentityRelease(pIdentity);
	Pkcs8[11] = 0x05;
	testRequire(xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	) == NULL, "Ed25519 identity accepted a different algorithm OID");
	Seed[0] ^= 1u;
	testRequire(xrtTlsIdentityEd25519(
		&Chain, 1u, (xbytesview) { Seed, sizeof(Seed) }
	) == NULL, "Ed25519 identity accepted a mismatched seed");
	return 0;
}
