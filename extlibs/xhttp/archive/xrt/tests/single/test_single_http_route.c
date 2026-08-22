#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include <stdio.h>



/* 验证单头发布保留严格路径结构和借用参数捕获。 */
int main(void)
{
	xhttprouteparam Params[2];
	size_t iCount;
	bool bPass;

	bPass = xrtHttpRouteMatch(
		XRT_STR_LITERAL("/users/{id}/{rest...}"),
		XRT_STR_LITERAL("/users/42/files/a.txt"),
		Params, 2, &iCount
	) == XHTTP_ROUTE_MATCH && (iCount == 2) &&
		(Params[0].Value.Size == 2u) &&
		(memcmp(Params[0].Value.Data, "42", 2u) == 0) &&
		(Params[1].Value.Size == 11u) &&
		(memcmp(Params[1].Value.Data, "files/a.txt", 11u) == 0);
	printf("%s single-http-route\n", bPass ? "[PASS]" : "[FAIL]");
	return bPass ? 0 : 1;
}
