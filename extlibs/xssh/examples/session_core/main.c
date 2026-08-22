#include <stdio.h>

#include <xssh.h>



/* 展示无 socket 的连接级 SSH 协议对象。 */
int main(void)
{
	xsshtransportcore Core;
	xsshsessioncore Session;

	if ( !xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) || !xrtSshSessionCoreInit(
		&Session,
		NULL,
		XSSH_ROLE_CLIENT,
		NULL,
		NULL,
		NULL
	) ) {
		return 1;
	}
	printf(
		"phase=%d action=%d session_bytes=%zu\n",
		(int)xrtSshSessionCorePhase(&Session, &Core),
		(int)xrtSshSessionCoreAction(&Session, &Core),
		sizeof(Session)
	);
	xrtSshSessionCoreClear(&Session);
	xrtSshTransportCoreClear(&Core);
	return 0;
}
