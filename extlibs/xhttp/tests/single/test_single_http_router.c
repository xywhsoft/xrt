#define XHTTP_MODULE_HTTP_ROUTER
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"

#include <stdio.h>



/* 验证单头发布保留预编译 Router、方法选择和参数捕获。 */
int main(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	xhttproutermatch Match;
	xhttprouteparam Params[1];
	size_t iCount;
	bool bPass;

	bPass = (pRouter != NULL) && xrtHttpRouterAdd(
		pRouter, XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/users/{id}"), (ptr)42
	) && xrtHttpRouterFreeze(pRouter) &&
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("HEAD"),
			XRT_STR_LITERAL("/users/7"),
			Params, 1, &iCount, &Match
		) == XHTTP_ROUTER_MATCH && (iCount == 1) &&
		(Match.Value == (ptr)42) &&
		((Match.Flags & XHTTP_ROUTER_HEAD_FALLBACK) != 0);
	xrtHttpRouterDestroy(pRouter);
	printf("%s single-http-router\n", bPass ? "[PASS]" : "[FAIL]");
	return bPass ? 0 : 1;
}
