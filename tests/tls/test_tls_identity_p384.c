#include "../test.h"

#define TEST_TLS_IDENTITY_FIXTURE_EC
#include "../fixtures/tls_identity_ec_ed.h"



/* 验证 P-384 原始、SEC1、PKCS#8 身份和真实协议签名。 */
int main(void)
{
	static const uint8 Message[] = "xrt p384 identity";
	uint8 Private[XRT_P384_PRIVATE_SIZE] = { 0 };
	uint8 Public[XRT_P384_PUBLIC_SIZE];
	uint8 Certificate[512];
	uint8 Sec1[224];
	uint8 Pkcs8[320];
	uint8 Hash[XRT_SHA512_SIZE];
	uint8 Signature[112];
	size_t iCertificateSize = 0;
	size_t iSec1Size = 0;
	size_t iPkcs8Size = 0;
	size_t iSignatureSize = 0;
	xbytesview Chain;
	xtlsidentity* pIdentity;

	Private[sizeof(Private) - 1u] = 1u;
	testRequire(xrtP384Public(Private, Public) &&
		testTlsIdentityEcCertificate(
			Public, sizeof(Public), Certificate,
			sizeof(Certificate), &iCertificateSize
		) && testTlsIdentityEcSec1(
			Private, sizeof(Private), Public,
			Sec1, sizeof(Sec1), &iSec1Size
		) && testTlsIdentityEcPkcs8(
			Sec1, iSec1Size, sizeof(Private),
			Pkcs8, sizeof(Pkcs8), &iPkcs8Size
		), "P-384 identity fixtures failed");
	Chain = (xbytesview) { Certificate, iCertificateSize };
	pIdentity = xrtTlsIdentityP384(
		&Chain, 1u, (xbytesview) { Private, sizeof(Private) }
	);
	testRequire((pIdentity != NULL) &&
		(xrtTlsIdentityType(pIdentity) == XTLS_IDENTITY_ECDSA_P384) &&
		xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384
		) && !xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_13,
			XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256
		) && xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_12,
			XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256
		) && xrtTlsIdentityCanSign(
			pIdentity, XTLS_VERSION_12,
			XTLS_SIGNATURE_ECDSA_SECP521R1_SHA512
		), "P-384 identity capability mismatch");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_13,
		XTLS_SIGNATURE_ECDSA_SECP384R1_SHA384,
		(xbytesview) { Message, sizeof(Message) - 1u },
		Signature, sizeof(Signature), &iSignatureSize
	) && xrtSha384(Message, sizeof(Message) - 1u, Hash) &&
		xrtEcdsaP384VerifyDer(
			Hash, XRT_SHA384_SIZE, Signature, iSignatureSize, Public
		), "P-384 identity signature verification failed");
	testRequire(xrtTlsIdentitySign(
		pIdentity, XTLS_VERSION_12,
		XTLS_SIGNATURE_ECDSA_SECP256R1_SHA256,
		(xbytesview) { Message, sizeof(Message) - 1u },
		Signature, sizeof(Signature), &iSignatureSize
	) && xrtSha256(Message, sizeof(Message) - 1u, Hash) &&
		xrtEcdsaP384VerifyDer(
			Hash, XRT_SHA256_SIZE, Signature, iSignatureSize, Public
		), "P-384 TLS 1.2 SHA-256 identity signature failed");
	xrtTlsIdentityRelease(pIdentity);
	pIdentity = xrtTlsIdentityP384(
		&Chain, 1u, (xbytesview) { Sec1, iSec1Size }
	);
	testRequire(pIdentity != NULL, "P-384 SEC1 identity failed");
	xrtTlsIdentityRelease(pIdentity);
	pIdentity = xrtTlsIdentityP384(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size }
	);
	testRequire(pIdentity != NULL, "P-384 PKCS#8 identity failed");
	xrtTlsIdentityRelease(pIdentity);
	Pkcs8[iPkcs8Size] = 0;
	testRequire(xrtTlsIdentityP384(
		&Chain, 1u, (xbytesview) { Pkcs8, iPkcs8Size + 1u }
	) == NULL, "P-384 identity accepted trailing PKCS#8 bytes");
	return 0;
}
