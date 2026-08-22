#include "../test.h"



/* 随机便利入口在 KEX 未就绪时不得消耗随机源或修改状态。 */
int main(void)
{
	xsshtransportcore Core;
	xsshsessioncore Session;

	testRequire(xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) && xrtSshSessionCoreInit(
		&Session,
		NULL,
		XSSH_ROLE_CLIENT,
		NULL,
		NULL,
		NULL
	), "ssh random session core initialization failed");
	testRequire(xrtSshSessionCoreKexBegin(
		&Session,
		&Core,
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_STATE, "ssh random session ignored KEX readiness");
	xrtSshSessionCoreClear(&Session);
	xrtSshTransportCoreClear(&Core);
	puts("ssh random session core tests passed");
	return 0;
}
