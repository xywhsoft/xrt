#include <xrt.h>

#include "../../../tests/fixtures/x509_legacy_cert.h"

#include <stdio.h>



/* 解析并验证从旧版示例继承的真实自签名 RSA 证书。 */
int main(void)
{
	xx509cert Certificate;

	if ( !xrtX509Parse(
		X509_LEGACY_RSA_CERT,
		sizeof(X509_LEGACY_RSA_CERT),
		&Certificate
	) || !xrtX509CertificateVerify(
		&Certificate, &Certificate
	) ) {
		fprintf(stderr, "%s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	printf("certificate signature is valid\n");
	return 0;
}
