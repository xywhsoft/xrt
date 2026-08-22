#include <stdio.h>
#include <string.h>

#include <xws.h>



/* Upgrade 完成后，回调取得 WebSocket Connection 的调用方引用。 */
static void onWebSocket(
	xhttpconn* pHttp,
	xnetresult Result,
	xwsconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pHttp;
	(void)pError;
	(void)pData;

	if ( (Result != XNET_RESULT_OK) ||
		(pConnection == NULL) ) {
		return;
	}
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_TLS)
		if ( xrtWsConnTls(pConnection) == NULL ) {
			xrtWsConnDestroy(pConnection);
			return;
		}
	#endif
	(void)xrtWsConnText(
		pConnection,
		XRT_STR_LITERAL("connected")
	);
	(void)xrtWsConnClose(
		pConnection,
		XWS_CLOSE_NORMAL,
		(xstrview) { 0 }
	);
	xrtWsConnDestroy(pConnection);
}



/* HTTP Request 事件可以先完成路由和鉴权，再把当前连接升级为 WebSocket。 */
static void onRequest(
	xhttpserver* pServer,
	xhttpconn* pHttp,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xwsserverconfig Config;
	xwsconnevents Events;

	(void)pServer;
	(void)pRequest;
	xrtWsServerConfigInit(&Config);
	memset(&Events, 0, sizeof(Events));
	Config.Protocols =
		XRT_STR_LITERAL("chat.v2, chat.v1");
	#if defined(XWS_FEATURE_WEBSOCKET_SERVER_DEFLATE)
		Config.EnableDeflate = true;
	#endif
	(void)xrtWsUpgrade(
		pHttp,
		&Config,
		&Events,
		pData,
		onWebSocket,
		pData
	);
}



/* 真实服务器把 onRequest 安装到 xhttpserverevents.Request。 */
int main(void)
{
	(void)onRequest;
	printf("WebSocket server Upgrade handler is ready\n");
	return 0;
}
