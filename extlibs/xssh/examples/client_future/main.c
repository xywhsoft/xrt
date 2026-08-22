#include <stdio.h>

#include <xssh.h>



/* 展示等待类型独立于任务和协程调度器。 */
int main(void)
{
	xsshclientconfig Config;
	xsshclient Client;

	if ( !xrtSshClientConfigInit(&Config) ||
		!xrtSshClientInit(&Client, &Config, NULL, NULL) ) {
		return 1;
	}
	printf("ready-wait=%d channel-open-wait=%d\n",
		(int)XSSH_CLIENT_WAIT_READY,
		(int)XSSH_CLIENT_CHANNEL_WAIT_OPEN);
	return xrtSshClientClear(&Client) ? 0 : 1;
}
