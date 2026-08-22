#include <stdio.h>

#include <xssh.h>



/* 展示带系统安全随机 KEX 和 packet padding 的 TCP 会话闭包。 */
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
		"phase=%d secure_random=ready\n",
		(int)xrtSshSessionTcpPhase(&Session)
	);
	xrtSshSessionTcpClear(&Session);
	return 0;
}
