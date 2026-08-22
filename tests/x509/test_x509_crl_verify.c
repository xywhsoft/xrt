#include "../test.h"
#include "../fixtures/x509_crl_vectors.h"



/* 验证 CRL 签名薄层的参数契约和缺失后端错误。 */
int main(void)
{
	uint8 Key[32] = { 0 };
	xx509crl Crl;
	xx509pubkey PublicKey;

	memset(&PublicKey, 0, sizeof(PublicKey));
	PublicKey.Type = X509_KEY_ED25519;
	PublicKey.Key = (xbytesview) { Key, sizeof(Key) };
	testRequire(xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	), "CRL verification fixture parse failed");

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	xrtClearError();
	testRequire(!xrtX509CrlVerifyKey(&Crl, &PublicKey) &&
		(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) &&
		(xrtErrorCode(xrtGetError()) == X509_ERROR_SIGNATURE),
		"CRL missing verification backend was not explicit");
#endif

	testRequire(!xrtX509CrlVerifyKey(NULL, &PublicKey) &&
		!xrtX509CrlVerifyKey(&Crl, NULL) &&
		!xrtX509CrlVerify(NULL, NULL),
		"CRL verification accepted null arguments");
	printf("[PASS] x509_crl_verify\n");
	return 0;
}
