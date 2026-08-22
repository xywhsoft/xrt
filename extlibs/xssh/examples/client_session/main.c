#include <stdio.h>

#include <xrt/ssh_client_session.h>



/* 展示 session 应用层的配置入口；网络接入由调用方 Engine 负责。 */
int main(void)
{
	xsshclientconfig Config;

	if ( !xrtSshClientConfigInit(&Config) ) {
		return 1;
	}
	printf(
		"session-window=%u packet=%u\n",
		Config.Channels.ReceiveWindow,
		Config.Channels.ReceiveMaxPacket
	);
	return 0;
}
