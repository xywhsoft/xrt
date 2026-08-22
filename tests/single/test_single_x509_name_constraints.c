#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_name_constraints_vectors.h"
#include "../fixtures/x509_profile_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 NameConstraints 解析与证书检查。 */
int main(void)
{
	xx509cert Certificate;
	xx509nameconstraints Constraints;

	if ( !xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Certificate
	) || !xrtX509NameConstraintsParse(
		(xbytesview) {
			X509_NAME_CONSTRAINTS_DNS,
			sizeof(X509_NAME_CONSTRAINTS_DNS)
		}, &Constraints
	) || !xrtX509NameConstraintsCheck(&Constraints, &Certificate) ) {
		return 1;
	}
	printf("[PASS] single-x509-name-constraints\n");
	return 0;
}
