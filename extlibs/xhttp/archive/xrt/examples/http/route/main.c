#include <xrt/http_route.h>

#include <stdio.h>



/* 匹配原始路径，并直接使用借用参数而不创建请求字典。 */
int main(void)
{
	xhttprouteparam Params[2];
	size_t iCount;

	if ( xrtHttpRouteMatch(
		XRT_STR_LITERAL("/users/{id}/files/{path...}"),
		XRT_STR_LITERAL("/users/42/files/docs/readme.txt"),
		Params, 2, &iCount
	) != XHTTP_ROUTE_MATCH ) {
		return 1;
	}
	printf(
		"id=%.*s path=%.*s\n",
		(int)Params[0].Value.Size, Params[0].Value.Data,
		(int)Params[1].Value.Size, Params[1].Value.Data
	);
	return 0;
}
