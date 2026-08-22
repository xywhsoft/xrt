#include "../test.h"



/* 验证 POP3 客户端默认配置和空对象查询。 */
int main(void)
{
	xpop3clientconfig Config;

	xrtPop3ClientConfigInit(&Config);
	testRequire((Config.Net.Port == 110u) &&
		(Config.Net.Security == XMAIL_SECURITY_PLAIN) &&
		Config.ReadCapabilities,
		"POP3 client default configuration mismatch");
	testRequire(!xrtPop3ClientConfigValid(&Config),
		"POP3 client accepted missing engine and resolver");
	testRequire(xrtPop3ClientState(NULL) == XPOP3_CLIENT_FAILED,
		"POP3 null client state mismatch");
	testRequire(xrtPop3ClientCapabilities(NULL) == 0,
		"POP3 null client capabilities mismatch");
	testRequire(!xrtPop3ClientAbort(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"POP3 null client abort mismatch");
	xrtClearError();
	return 0;
}
