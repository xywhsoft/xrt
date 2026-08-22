#include <stdio.h>

#include <xrt.h>



/* 演示零分配 DNS-ID 通配符匹配。 */
int main(void)
{
	xx509result Result = xrtX509MatchDns(
		XRT_STR_LITERAL("*.example.test"),
		XRT_STR_LITERAL("api.example.test")
	);

	printf("matched=%s\n", (Result == X509_VALUE) ? "yes" : "no");
	return (Result == X509_VALUE) ? 0 : 1;
}
