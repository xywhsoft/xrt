#include "../test.h"
#include "../crypto/test_crypto_digest.h"
#include "../fixtures/x509_verify_vectors.h"



/* 验证 ECDSA 曲线分派、交叉摘要组合和错误包装。 */
int main(void)
{
	uint8 Public[97];
	uint8 Signature[102];
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_ECDSA;
	Scheme.Hash = X509_HASH_SHA384;
	PublicKey.Type = X509_KEY_EC;
	PublicKey.Curve = X509_CURVE_P256;
	testCryptoDecode(
		Public, 65u, X509_VERIFY_P256_PUBLIC,
		"X.509 P-256 public key size mismatch"
	);
	testCryptoDecode(
		Signature, 71u, X509_VERIFY_P256_SHA384,
		"X.509 P-256 SHA-384 signature size mismatch"
	);
	PublicKey.Key = (xbytesview) { Public, 65u };
	testRequire(xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, 71u },
		&PublicKey
	), "X.509 P-256 SHA-384 signature was rejected");
	Signature[70] ^= UINT8_C(1);
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, 71u },
		&PublicKey
	) && (xrtErrorCause(xrtGetError()) != NULL),
		"X.509 ECDSA failure cause was lost");

	Scheme.Hash = X509_HASH_SHA256;
	PublicKey.Curve = X509_CURVE_P384;
	testCryptoDecode(
		Public, 97u, X509_VERIFY_P384_PUBLIC,
		"X.509 P-384 public key size mismatch"
	);
	testCryptoDecode(
		Signature, 102u, X509_VERIFY_P384_SHA256,
		"X.509 P-384 SHA-256 signature size mismatch"
	);
	PublicKey.Key = (xbytesview) { Public, 97u };
	testRequire(xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, 102u },
		&PublicKey
	), "X.509 P-384 SHA-256 signature was rejected");
	PublicKey.Key.Size = 96u;
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, 102u },
		&PublicKey
	) && (xrtErrorCause(xrtGetError()) == NULL),
		"X.509 malformed P-384 key view used a stale error cause");
	PublicKey.Key.Size = 97u;

	PublicKey.Curve = X509_CURVE_P521;
	xrtClearError();
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, 102u },
		&PublicKey
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"X.509 unsupported P-521 backend was not explicit");
	printf("[PASS] x509_verify_ecdsa\n");
	return 0;
}
