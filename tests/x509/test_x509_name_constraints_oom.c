#include "../test_allocator.h"
#include "../fixtures/x509_name_constraints_vectors.h"
#include "../fixtures/x509_profile_vectors.h"



/* 验证 DNS/IP NameConstraints 解析与检查不依赖堆分配。 */
int main(void)
{
	xx509cert Certificate;
	xx509nameconstraints Constraints;

	testRequire(xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	), "NameConstraints OOM certificate parse failed");
	testRequire(testInstallFailAllocator(),
		"NameConstraints failure allocator install failed");
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_DNS,
			sizeof(X509_NAME_CONSTRAINTS_DNS)
		}, &Constraints
	) && xrtX509NameConstraintsCheck(&Constraints, &Certificate),
		"NameConstraints DNS path allocated memory");
	testRequire(xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_LOOPBACK,
			sizeof(X509_NAME_CONSTRAINTS_LOOPBACK)
		}, &Constraints
	) && !xrtX509NameConstraintsCheck(&Constraints, &Certificate),
		"NameConstraints excluded IP path changed semantics under OOM");
	printf("[PASS] x509_name_constraints_oom\n");
	return 0;
}
