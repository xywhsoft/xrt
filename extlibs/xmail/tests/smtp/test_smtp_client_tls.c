#include "../test.h"



/* 验证 SMTP TLS 客户端公开 STARTTLS 配置而不改变默认明文端口。 */
int main(void)
{
	xsmtpclientconfig Config;

	xrtSmtpClientConfigInit(&Config);
	Config.Net.Security = XMAIL_SECURITY_STARTTLS;
	testRequire((Config.Net.Port == 25u) &&
		(Config.Net.Security == XMAIL_SECURITY_STARTTLS),
		"SMTP STARTTLS configuration mismatch");
	return 0;
}
