#include "../test.h"



/* 验证 POP3 TLS 客户端公开 STLS 配置。 */
int main(void)
{
	xpop3clientconfig Config;

	xrtPop3ClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	testRequire((Config.Net.Port == 110u) && Config.ReadCapabilities &&
		(Config.Net.Security == XMAIL_SECURITY_STARTTLS),
		"POP3 STLS configuration mismatch");
	return 0;
}
