#include "../test.h"



/* 验证基础分派层的参数契约和缺失后端错误。 */
int main(void)
{
	static const uint8 Byte = 1;
	xx509signature Scheme;
	xx509pubkey PublicKey;
	xbytesview Content = { NULL, 0 };
	xbytesview Signature = { &Byte, 1u };

	memset(&Scheme, 0, sizeof(Scheme));
	memset(&PublicKey, 0, sizeof(PublicKey));
	Scheme.Type = X509_SIGNATURE_RSA_PKCS1;
	Scheme.Hash = X509_HASH_SHA256;
	PublicKey.Type = X509_KEY_RSA;

#if !defined(XRT_FEATURE_X509_VERIFY_RSA)
	xrtClearError();
	testRequire(!xrtX509SignatureVerify(
		&Scheme, Content, Signature, &PublicKey
	) && (xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.x509") == 0),
		"X.509 missing verification backend was not explicit");
#endif

	Scheme.SaltSize = 1u;
	xrtClearError();
	testRequire(!xrtX509SignatureVerify(
		&Scheme, Content, Signature, &PublicKey
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"X.509 inconsistent signature descriptor was accepted");
	Scheme.SaltSize = 0;
	testRequire(!xrtX509SignatureVerify(
		NULL, Content, Signature, &PublicKey
	) && !xrtX509SignatureVerify(
		&Scheme, Content, (xbytesview) { NULL, 0 }, &PublicKey
	), "X.509 verification accepted null arguments");
	printf("[PASS] x509_verify\n");
	return 0;
}
