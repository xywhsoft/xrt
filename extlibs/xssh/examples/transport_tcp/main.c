#include <stdio.h>
#include <xssh.h>



/* 展示 TCP transport 的动态缓冲配置；真实连接应传入 Stream Worker 缓冲池。 */
int main(void)
{
	xsshtransporttcpconfig Config;
	xsshtransporttcp Transport;

	if ( !xrtSshTransportTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshTransportTcpInit(
		&Transport,
		NULL,
		&Config,
		0u
	) ) {
		return 1;
	}
	printf("transport-tcp=%zu fixed-output=%zu banner-limit=%zu\n",
		sizeof(Transport),
		xrtSshTransportTcpWriteSize(&Transport),
		Config.MaxBannerBytes);
	xrtSshTransportTcpClear(&Transport);
	return 0;
}
