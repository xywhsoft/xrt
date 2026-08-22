#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 CORS 策略判断。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Origin"), XRT_STR_INIT("https://app.example") }
	};
	xhttpcorspolicy Policy = { 0 };
	xhttpcorsdecision Decision;

	Policy.Flags = XHTTP_CORS_POLICY_ANY_ORIGIN;
	if ( !xrtHttpCorsPolicyCheck(
		&Policy, XRT_STR_LITERAL("GET"), Fields, 1u, &Decision
	) || ((Decision.Flags & XHTTP_CORS_DECISION_ALLOW) == 0) ) {
		return 1;
	}
	return 0;
}
