#include <stdio.h>
#include <xrt.h>



/* 常用路由可以直接提交固定正文，不要求应用创建 Reply。 */
static void exampleHttpServerRoute(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	const xhttprouteparam* pParams,
	size_t iParamCount,
	ptr pData
)
{
	(void)pServer;
	(void)pRequest;
	(void)pParams;
	(void)iParamCount;
	(void)pData;
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("application/json; charset=utf-8"),
		XRT_BYTES_LITERAL("{\"code\":200,\"msg\":\"OK\"}")
	);
}



/* 注册常用方法并冻结为可无锁并发匹配的 Server Router。 */
int main(void)
{
	xhttpserverrouter* pRouter = xrtHttpServerRouterCreate(NULL);

	if ( (pRouter == NULL) ||
		!xrtHttpServerGet(
			pRouter,
			XRT_STR_LITERAL("/users/{id}"),
			exampleHttpServerRoute,
			NULL
		) || !xrtHttpServerPost(
			pRouter,
			XRT_STR_LITERAL("/users"),
			exampleHttpServerRoute,
			NULL
		) || !xrtHttpServerRouterFreeze(pRouter) ) {
		xrtHttpServerRouterDestroy(pRouter);
		return 1;
	}
	printf(
		"routes: %u, frozen: %s\n",
		(unsigned)xrtHttpServerRouterCount(pRouter),
		xrtHttpServerRouterFrozen(pRouter) ? "yes" : "no"
	);
	xrtHttpServerRouterDestroy(pRouter);
	return 0;
}
