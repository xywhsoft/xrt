#include <stdio.h>
#include <string.h>

#include <xws.h>



/*
	在 HTTP Request Worker 中提交 Upgrade Future。
	调用方把返回值交给任务或协程等待，并最终从 xwsopenresult 取走 Connection。
*/
static xfuture* upgradeRequest(xhttpconn* pHttp)
{
	xwsserverconfig Config;
	xwsconnevents Events;

	xrtWsServerConfigInit(&Config);
	memset(&Events, 0, sizeof(Events));
	Config.Protocols =
		XRT_STR_LITERAL("chat.v2, chat.v1");
	return xrtWsUpgradeAsync(
		pHttp,
		&Config,
		&Events,
		NULL
	);
}



/* 真实服务器从 Request 回调调用 upgradeRequest，并由任务层拥有返回的 Future。 */
int main(void)
{
	(void)upgradeRequest;
	printf("WebSocket server Upgrade Future handler is ready\n");
	return 0;
}
