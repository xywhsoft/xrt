#include "../test_allocator.h"
#include "../fixtures/x509_profile_vectors.h"



/* 验证信任锚与发行者筛选的有效路径不依赖堆分配。 */
int main(void)
{
	xx509cert Certificate;
	xx509cert Child;
	xx509anchor Anchor;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	), "X.509 path OOM fixture parse failed");
	Child = Certificate;
	Child.Issuer = Certificate.Subject;
	testRequire(testInstallFailAllocator(),
		"X.509 path failure allocator install failed");
	testRequire(xrtX509Anchor(&Certificate, &Anchor) &&
		(xrtX509IssuerMatch(&Child, &Certificate) == X509_VALUE),
		"X.509 path metadata required heap allocation");
	printf("[PASS] x509_path_oom\n");
	return 0;
}
