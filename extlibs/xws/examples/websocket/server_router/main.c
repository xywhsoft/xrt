#include <stdio.h>

#include <xws.h>



/* 固定端点建立后发送一条欢迎消息；Connection 只在当前回调内借用。 */
static void onOpen(
	xhttpconn* pHttp,
	xwsconn* pConnection,
	ptr pData
)
{
	(void)pHttp;
	(void)pData;
	(void)xrtWsConnText(
		pConnection,
		XRT_STR_LITERAL("connected")
	);
}



/* 注册失败时输出当前结构化错误的可读消息。 */
static int fail(cstr sOperation)
{
	const xerror* pError = xrtGetError();

	fprintf(
		stderr,
		"%s: %s\n",
		sOperation,
		pError != NULL ?
			xrtErrorMessage(pError) : "unknown error"
	);
	return 1;
}



/* 创建一个可供明文或 TLS HTTP Router 共用的固定 WebSocket 端点。 */
int main(void)
{
	xhttpserverrouter* pRouter;
	xwsserverrouteconfig Config;

	pRouter = xrtHttpServerRouterCreate(NULL);
	if ( pRouter == NULL ) {
		return fail("create router");
	}
	xrtWsServerRouteConfigInit(&Config);
	Config.Server.Protocols =
		XRT_STR_LITERAL("chat.v2, chat.v1");
	Config.Open = onOpen;
	if ( !xrtWsServerRoute(
		pRouter,
		XRT_STR_LITERAL("/chat"),
		&Config
	) || !xrtHttpServerRouterFreeze(pRouter) ) {
		xrtHttpServerRouterDestroy(pRouter);
		return fail("register websocket route");
	}

	printf("WebSocket route /chat is ready\n");
	xrtHttpServerRouterDestroy(pRouter);
	return 0;
}
