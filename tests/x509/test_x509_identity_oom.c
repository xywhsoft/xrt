#include "../test_allocator.h"
#include "../fixtures/x509_profile_vectors.h"



/* 验证 DNS/IP 服务身份匹配的全部有效路径不分配内存。 */
int main(void)
{
	xx509cert Cert;
	xx509genname Name;

	testRequire(testInstallFailAllocator(),
		"X.509 identity failure allocator install failed");
	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	), "X.509 identity certificate allocated memory");
	testRequire(xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("api.example.test")
	) == X509_VALUE, "X.509 DNS identity allocated memory");
	testRequire((xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), &Name
	) == X509_VALUE) && (xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("127.0.0.1"), &Name
	) == X509_VALUE), "X.509 host identity allocated memory");
	printf("[PASS] x509_identity_oom\n");
	return 0;
}
