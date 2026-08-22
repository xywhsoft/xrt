#include "../test.h"
#include "../crypto/test_crypto_digest.h"
#include "../fixtures/x509_verify_vectors.h"



/* 验证纯 Ed25519 分派、损坏签名和 Ed448 缺失后端契约。 */
int main(void)
{
	uint8 Public[32];
	uint8 Signature[64];
	xx509signature Scheme;
	xx509pubkey PublicKey;

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_ED25519;
	PublicKey.Type = X509_KEY_ED25519;
	testCryptoDecode(
		Public, sizeof(Public), X509_VERIFY_ED25519_PUBLIC,
		"X.509 Ed25519 public key size mismatch"
	);
	testCryptoDecode(
		Signature, sizeof(Signature), X509_VERIFY_ED25519_SIGNATURE,
		"X.509 Ed25519 signature size mismatch"
	);
	PublicKey.Key = (xbytesview) { Public, sizeof(Public) };
	testRequire(xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	), "X.509 Ed25519 signature was rejected");
	Signature[0] ^= UINT8_C(1);
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	) && (xrtErrorCause(xrtGetError()) != NULL),
		"X.509 Ed25519 failure cause was lost");

	Scheme.Type = X509_SIGNATURE_ED448;
	PublicKey.Type = X509_KEY_ED448;
	xrtClearError();
	testRequire(!xrtX509SignatureVerify(
		&Scheme,
		(xbytesview) {
			X509_VERIFY_MESSAGE, sizeof(X509_VERIFY_MESSAGE) - 1u
		},
		(xbytesview) { Signature, sizeof(Signature) },
		&PublicKey
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED),
		"X.509 unavailable Ed448 backend was not explicit");
	printf("[PASS] x509_verify_ed25519\n");
	return 0;
}
