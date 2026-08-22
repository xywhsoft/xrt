#include <stdio.h>
#include <string.h>

#include <xws.h>



/* 完成回调取得非空 Response，以及成功时的 Connection 调用方引用。 */
static void onConnect(
	xhttpcall* pCall,
	xnetresult Result,
	xwsconn* pConnection,
	xhttpresponse* pResponse,
	const xerror* pError,
	ptr pData
)
{
	(void)pCall;
	(void)pError;
	(void)pData;

	if ( (Result == XNET_RESULT_OK) &&
		(pConnection != NULL) ) {
		#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_HTTPS)
			if ( xrtWsConnTls(pConnection) == NULL ) {
				xrtWsConnDestroy(pConnection);
				xrtHttpResponseDestroy(pResponse);
				return;
			}
		#endif
		#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
			(void)xrtWsConnTextCompressed(
				pConnection,
				XRT_STR_LITERAL("hello")
			);
		#else
			(void)xrtWsConnText(
				pConnection,
				XRT_STR_LITERAL("hello")
			);
		#endif
		(void)xrtWsConnClose(
			pConnection,
			XWS_CLOSE_NORMAL,
			(xstrview) { 0 }
		);
		xrtWsConnDestroy(pConnection);
	}
	xrtHttpResponseDestroy(pResponse);
}



/* Client 和返回的 Call 由调用方持有，完成后仍需分别销毁。 */
static xhttpcall* connectWebSocket(
	xhttpclient* pClient
)
{
	xwsclientconfig Config;
	xwsconnevents Events;

	xrtWsClientConfigInit(&Config);
	memset(&Events, 0, sizeof(Events));
	Config.Protocols =
		XRT_STR_LITERAL("chat.v1, chat.v2");
	#if defined(XWS_FEATURE_WEBSOCKET_CLIENT_DEFLATE)
		Config.EnableDeflate = true;
		Config.Deflate.Flags =
			XWS_DEFLATE_CLIENT_MAX_WINDOW |
			XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	#endif
	return xrtWsConnect(
		pClient,
		XRT_STR_LITERAL("wss://example.com/chat"),
		&Config,
		&Events,
		NULL,
		onConnect,
		NULL
	);
}



/* 真实客户端在网络 Engine 与 HTTP Client 启动后调用 connectWebSocket。 */
int main(void)
{
	(void)connectWebSocket;
	printf("WebSocket client connector is ready\n");
	return 0;
}
