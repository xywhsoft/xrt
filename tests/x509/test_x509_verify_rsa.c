#include "../test.h"
#include "../crypto/rsa_fixture.h"
#include "../crypto/test_crypto_digest.h"
#include "../fixtures/x509_legacy_cert.h"
#include "../fixtures/x509_signature_vectors.h"
#include "../fixtures/x509_verify_vectors.h"



/* 初始化复用旧版 RSA 固定密钥的 X.509 公钥视图。 */
static void testRsaKey(
	xx509pubkey* pPublicKey,
	uint8 pModulus[128]
)
{
	xrsapublickey Key;
	uint8 Hash[32];

	testRequire(__xrtTestRsaFixture(&Key, pModulus, Hash),
		"X.509 RSA legacy fixture initialization failed");
	memset(pPublicKey, 0, sizeof(*pPublicKey));
	pPublicKey->Type = X509_KEY_RSA;
	pPublicKey->Modulus = (xbytesview) { Key.Modulus, Key.ModulusSize };
	pPublicKey->Exponent = (xbytesview) { Key.Exponent, Key.ExponentSize };
}



/* 从固定 AlgorithmIdentifier 向量读取验签方案。 */
static xx509signature testRsaScheme(
	const uint8* pDer,
	size_t iSize,
	xx509algorithm* pAlgorithm
)
{
	xx509signature Scheme;

	testRequire(xrtX509AlgorithmParse(
		(xbytesview) { pDer, iSize }, pAlgorithm),
		"X.509 RSA algorithm parse failed");
	testRequire(xrtX509SignatureParse(pAlgorithm, &Scheme) == X509_VALUE,
		"X.509 RSA signature scheme parse failed");
	return Scheme;
}



/* 验证 PKCS#1、PSS、公钥限制和结构化原因链。 */
static void testRsaSignatures(void)
{
	uint8 Modulus[128];
	uint8 Signature[128];
	uint8 RestrictedDer[sizeof(X509_SIGNATURE_PSS_CUSTOM)];
	xx509pubkey PublicKey;
	xx509algorithm Algorithm;
	xx509signature Scheme;
	xx509signature WrongScheme;

	testRsaKey(&PublicKey, Modulus);
	Scheme = testRsaScheme(
		X509_SIGNATURE_RSA_SHA224,
		sizeof(X509_SIGNATURE_RSA_SHA224),
		&Algorithm
	);
	testCryptoDecode(
		Signature, sizeof(Signature), X509_VERIFY_RSA_PKCS1_SHA224,
		"X.509 RSA-PKCS1 signature size mismatch"
	);
	testRequire(xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 RSA-PKCS1 SHA-224 signature was rejected");
	Signature[0] ^= UINT8_C(1);
	xrtClearError();
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	) && (xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE) &&
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(
			xrtErrorDomain(xrtErrorCause(xrtGetError())), "xrt.crypto"
		) == 0), "X.509 RSA failure cause chain was lost");

	Scheme = testRsaScheme(
		X509_SIGNATURE_PSS_CUSTOM,
		sizeof(X509_SIGNATURE_PSS_CUSTOM),
		&Algorithm
	);
	testCryptoDecode(
		Signature, sizeof(Signature), X509_VERIFY_RSA_PSS_SHA256,
		"X.509 RSA-PSS signature size mismatch"
	);
	PublicKey.Type = X509_KEY_RSA_PSS;
	PublicKey.Algorithm = Algorithm;
	testRequire(xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 restricted RSA-PSS signature was rejected");

	memcpy(
		RestrictedDer, X509_SIGNATURE_PSS_CUSTOM,
		sizeof(RestrictedDer)
	);
	RestrictedDer[sizeof(RestrictedDer) - 6u] = UINT8_C(0x21);
	(void)testRsaScheme(
		RestrictedDer, sizeof(RestrictedDer), &PublicKey.Algorithm
	);
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 RSA-PSS key minimum salt was ignored");

	PublicKey.Algorithm = Algorithm;
	WrongScheme = Scheme;
	WrongScheme.Hash = X509_HASH_SHA384;
	testRequire(!xrtX509SignatureVerify(
		&WrongScheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 RSA-PSS key hash restriction was ignored");

	Scheme = testRsaScheme(
		X509_SIGNATURE_RSA_SHA224,
		sizeof(X509_SIGNATURE_RSA_SHA224),
		&Algorithm
	);
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 RSA-PSS-only key accepted a PKCS#1 signature");
}



/* 验证从旧版示例继承的真实自签名证书及损坏签名。 */
static void testLegacyCertificate(void)
{
	uint8 Modified[sizeof(X509_LEGACY_RSA_CERT)];
	xx509cert Certificate;
	xx509cert Damaged;
	xx509pubkey PublicKey;

	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT, sizeof(X509_LEGACY_RSA_CERT), &Certificate
	) && xrtX509PublicKey(&Certificate, &PublicKey) &&
		xrtX509CertificateVerifyKey(&Certificate, &PublicKey) &&
		xrtX509CertificateVerify(&Certificate, &Certificate),
		"legacy XRT self-signed certificate verification failed");
	memcpy(Modified, X509_LEGACY_RSA_CERT, sizeof(Modified));
	Modified[sizeof(Modified) - 1u] ^= UINT8_C(1);
	testRequire(xrtX509Parse(
		Modified, sizeof(Modified), &Damaged
	) && !xrtX509CertificateVerify(&Damaged, &Damaged),
		"damaged legacy XRT certificate signature was accepted");
}



int main(void)
{
	testRsaSignatures();
	testLegacyCertificate();
	printf("[PASS] x509_verify_rsa\n");
	return 0;
}
