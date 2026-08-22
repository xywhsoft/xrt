#include <stdio.h>

#include <xws.h>



/* Upgrade 成功后把借用连接加入应用连接组。 */
static void connectionOpen(xwsgroup* pGroup, xwsconn* pConnection)
{
	(void)xrtWsGroupAdd(pGroup, pConnection);
}



/* 唯一 Close 回调中移除连接，归还连接组持有的引用。 */
static void connectionClose(xwsgroup* pGroup, xwsconn* pConnection)
{
	(void)xrtWsGroupRemove(pGroup, pConnection);
}



/* 示例保留回调形状，实际服务器可直接把组放入路由上下文。 */
int main(void)
{
	xwsgroup* pGroup = xrtWsGroupCreate(0);

	if ( pGroup == NULL ) {
		return 1;
	}
	(void)connectionOpen;
	(void)connectionClose;
	printf("WebSocket connection group is ready\n");
	xrtWsGroupDestroy(pGroup);
	return 0;
}
