#include <stdio.h>

#include <xssh.h>



/* 展示不接管 Stream、等待和外部 channel 表的 TCP 会话对象。 */
int main(void)
{
	xsshsessiontcpconfig Config;
	xsshsessiontcp Session;

	if ( !xrtSshSessionTcpConfigInit(
		&Config,
		XSSH_ROLE_CLIENT
	) || !xrtSshSessionTcpInit(
		&Session,
		NULL,
		&Config,
		0u
	) ) {
		return 1;
	}
	printf(
		"phase=%d action=%d session_tcp_bytes=%zu\n",
		(int)xrtSshSessionTcpPhase(&Session),
		(int)xrtSshSessionTcpAction(&Session),
		sizeof(Session)
	);
	xrtSshSessionTcpClear(&Session);
	return 0;
}
