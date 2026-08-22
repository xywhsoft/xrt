#include <xrt/http_router.h>

#include <stdio.h>



/* 把借用处理器标识注册到不可变 Router，并完成一次请求分派。 */
int main(void)
{
	xhttprouter* pRouter = xrtHttpRouterCreate(NULL);
	xhttproutermatch Match;
	xhttprouteparam Params[1];
	size_t iCount;
	int iUsers = 1;

	if ( (pRouter == NULL) || !xrtHttpRouterAdd(
		pRouter, XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL("/users/{id}"), &iUsers
	) || !xrtHttpRouterFreeze(pRouter) ||
		xrtHttpRouterMatch(
			pRouter, XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/users/42"),
			Params, 1, &iCount, &Match
		) != XHTTP_ROUTER_MATCH ) {
		xrtHttpRouterDestroy(pRouter);
		return 1;
	}
	printf(
		"handler=%d id=%.*s\n",
		*(int*)Match.Value,
		(int)Params[0].Value.Size,
		Params[0].Value.Data
	);
	xrtHttpRouterDestroy(pRouter);
	return 0;
}
