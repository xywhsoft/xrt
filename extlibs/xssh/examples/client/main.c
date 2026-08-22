#include <stdio.h>

#include <xssh.h>



/* 展示客户端由外部 Engine 和 Stream 驱动，不创建隐藏运行时。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	if ( !xrtSshClientConfigInit(&Config) ||
		!xrtSshClientInit(&Client, &Config, NULL, NULL) ) {
		return 1;
	}
	printf("state=%d channels=%zu\n", (int)xrtSshClientState(&Client),
		Config.Channels.MaxChannels);
	return xrtSshClientClear(&Client) ? 0 : 1;
}
