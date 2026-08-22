#include <stdio.h>

#include <xssh.h>



/* 展示 SSH Dial 与 TCP Dial 共享同一套可裁剪连接策略。 */
int main(void)
{
	xnetdialconfig DialConfig;
	xsshclientconfig ClientConfig;
	xsshclient Client;

	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Timeout = 10000000u;
	DialConfig.FallbackDelay = 250000u;
	if ( !xrtSshClientConfigInit(&ClientConfig) ||
		!xrtSshClientInit(&Client, &ClientConfig, NULL, NULL) ) {
		return 1;
	}
	printf("dial-timeout=%llu fallback=%llu state=%d\n",
		(unsigned long long)DialConfig.Timeout,
		(unsigned long long)DialConfig.FallbackDelay,
		(int)xrtSshClientState(&Client));
	return xrtSshClientClear(&Client) ? 0 : 1;
}
