#include <stdio.h>

#include <xssh.h>



/* 展示安全随机 KEX 便利入口仍服从会话阶段。 */
int main(void)
{
	xsshtransportcore Core;
	xsshsessioncore Session;
	xsshcode Code;

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
	Code = xrtSshSessionCoreKexBegin(
		&Session,
		&Core,
		(xbytesview){ NULL, 0u }
	);
	printf("not_ready=%d\n", (int)Code);
	xrtSshSessionCoreClear(&Session);
	xrtSshTransportCoreClear(&Core);
	return Code == XSSH_ERROR_STATE ? 0 : 1;
}
