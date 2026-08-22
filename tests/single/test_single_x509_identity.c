#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_profile_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 DNS-ID 和 IP-ID 服务身份匹配。 */
int main(void)
{
	xx509cert Cert;
	xx509genname Name;

	if ( !xrtX509Parse(
		X509_PROFILE_VALID, sizeof(X509_PROFILE_VALID), &Cert
	) || (xrtX509MatchHost(
		&Cert, XRT_STR_LITERAL("api.example.test"), &Name
	) != X509_VALUE) || (Name.Type != X509_NAME_DNS) ||
		(xrtX509MatchHost(
			&Cert, XRT_STR_LITERAL("127.0.0.1"), &Name
		) != X509_VALUE) || (Name.Type != X509_NAME_IP) ) {
		return 1;
	}
	printf("[PASS] single-x509-identity\n");
	return 0;
}
