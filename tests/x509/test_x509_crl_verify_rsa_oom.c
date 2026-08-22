#include "../test_allocator.h"
#include "../fixtures/x509_crl_vectors.h"



/* 验证真实 RSA CRL 的解析和验签成功路径完全不分配内存。 */
int main(void)
{
	xx509cert Issuer;
	xx509crl Crl;

	testRequire(testInstallFailAllocator(),
		"CRL RSA verify failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_CRL_LEGACY_ROOT, sizeof(X509_CRL_LEGACY_ROOT), &Issuer
	) && xrtX509CrlParse(
		X509_CRL_LEGACY_REVOKED, sizeof(X509_CRL_LEGACY_REVOKED), &Crl
	) && xrtX509CrlVerify(&Crl, &Issuer),
		"CRL RSA verification allocated memory");
	printf("[PASS] x509_crl_verify_rsa_oom\n");
	return 0;
}
