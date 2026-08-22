#include "../test_allocator.h"
#include "../fixtures/x509_legacy_cert.h"



/* 验证解析和 RSA 证书验签的成功路径完全不分配内存。 */
int main(void)
{
	xx509cert Certificate;

	testRequire(testInstallFailAllocator(),
		"X.509 RSA verify failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_LEGACY_RSA_CERT,
		sizeof(X509_LEGACY_RSA_CERT),
		&Certificate
	) && xrtX509CertificateVerify(&Certificate, &Certificate),
		"X.509 RSA verification allocated memory");
	printf("[PASS] x509_verify_rsa_oom\n");
	return 0;
}
