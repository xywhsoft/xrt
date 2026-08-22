#include <stdio.h>

#include <xssh.h>



/* 随机便利层不得绕过 identification 与 KEX 就绪条件。 */
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
	if ( xrtSshSessionTcpKexBegin(
		&Session,
		(xbytesview){ NULL, 0u }
	) != XSSH_ERROR_STATE ) {
		fprintf(stderr, "ssh random TCP session bypassed KEX readiness\n");
		xrtSshSessionTcpClear(&Session);
		return 1;
	}
	xrtSshSessionTcpClear(&Session);
	return 0;
}
