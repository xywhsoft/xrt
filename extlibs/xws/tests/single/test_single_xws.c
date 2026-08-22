#define XWS_MODULE_ALL
#define XWS_IMPLEMENTATION
#include "../../single/xws.h"



/* 完整单头必须同时闭合连接、HTTP 适配、TLS、压缩、路由和连接组。 */
int main(void)
{
	xwsconnconfig Connection;
	xwsserverconfig Server;
	xwsclientconfig Client;

	xrtWsConnConfigInit(&Connection);
	xrtWsServerConfigInit(&Server);
	xrtWsClientConfigInit(&Client);
	return (Connection.MessageLimit == XWS_CONN_MESSAGE_LIMIT_DEFAULT) &&
		(Server.Connection.MessageLimit == XWS_CONN_MESSAGE_LIMIT_DEFAULT) &&
		(Client.Connection.Role == XWS_ROLE_CLIENT) ? 0 : 1;
}
