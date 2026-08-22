#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_crl_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 CRL 签名分派和缺失后端错误。 */
int main(void)
{
	xx509crl Crl;

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	uint8 Key[32] = { 0 };
	xx509pubkey PublicKey;

	memset(&PublicKey, 0, sizeof(PublicKey));
	PublicKey.Type = X509_KEY_ED25519;
	PublicKey.Key = (xbytesview) { Key, sizeof(Key) };

#endif

	if ( !xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	) ) {
		return 1;
	}

#if !defined(XRT_FEATURE_X509_VERIFY_ED25519)
	if ( xrtX509CrlVerifyKey(&Crl, &PublicKey) ||
		(xrtErrorKind(xrtGetError()) != XERR_UNSUPPORTED) ) {
		return 1;
	}
#endif

	printf("[PASS] single-x509-crl-verify\n");
	return 0;
}
