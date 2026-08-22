#include "../test.h"



/* 随机层只验证生产入口和精确依赖；确定性事务由核心测试覆盖。 */
int main(void)
{
	xsshtransportcore Core;
	xsshkexexchange Exchange;

	testRequire(xrtSshTransportCoreInit(
		&Core,
		XSSH_ROLE_CLIENT,
		0u,
		NULL,
		0u
	) && xrtSshKexExchangeInit(
		&Exchange,
		NULL,
		XSSH_ROLE_CLIENT
	) && (xrtSshKexExchangeBegin(
		&Exchange,
		&Core,
		(xbytesview){ NULL, 0u }
	) == XSSH_ERROR_STATE), "ssh random KEX exchange readiness gate failed");
	xrtSshKexExchangeClear(&Exchange);
	xrtSshTransportCoreClear(&Core);
	puts("ssh KEX exchange secure ephemeral adapter enabled");
	return 0;
}
