#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_crl_vectors.h"

#include <stdio.h>



/* 验证单头文件中的真实 RSA CRL 签名。 */
int main(void)
{
	xx509cert Issuer;
	xx509crl Crl;

	if ( !xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	) || !xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Crl
	) || !xrtX509CrlVerify(&Crl, &Issuer) ) {
		return 1;
	}
	printf("[PASS] single-x509-crl-verify-rsa\n");
	return 0;
}
