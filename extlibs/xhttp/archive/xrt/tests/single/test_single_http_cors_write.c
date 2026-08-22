#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 CORS 原始字段写出。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") }
	};
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;
	char Output[96];
	size_t iSize;

	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	if ( !xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Fields, 1u, &Decision
	) || !xrtHttpCorsDecisionWrite(
		&Decision, Fields, 1u, Output, sizeof(Output), &iSize
	) || (iSize != 32u) ) {
		return 1;
	}
	return 0;
}
