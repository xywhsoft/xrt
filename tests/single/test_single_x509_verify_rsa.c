#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_legacy_cert.h"



/* 验证单头文件可以完成旧版真实 RSA 证书自签名验证。 */
int main(void)
{
	xx509cert Certificate;

	return (!xrtX509Parse(
		X509_LEGACY_RSA_CERT,
		sizeof(X509_LEGACY_RSA_CERT),
		&Certificate
	) || !xrtX509CertificateVerify(
		&Certificate, &Certificate
	)) ? 1 : 0;
}
