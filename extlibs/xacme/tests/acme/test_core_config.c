#include "../test.h"

#include <string.h>

/* 账户配置初始化与线程错误语义。 */
int main(void)
{
	xacmeaccountconfig Config;

	testRequire(
		strcmp(XACME_DIRECTORY_LE,
			"https://acme-v02.api.letsencrypt.org/directory") == 0,
		"acme directory LE preset mismatch"
	);
	testRequire(
		strstr(XACME_DIRECTORY_ZEROSSL, "zerossl.com") != NULL,
		"acme directory ZeroSSL preset mismatch"
	);

	xrtClearError();
	xrtAcmeAccountConfigInit(NULL);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"acme account config init null mismatch"
	);

	xrtAcmeAccountConfigInit(&Config);
	testRequire(
		(Config.sDirectoryUrl == NULL) && (Config.sAccountKeyPem == NULL) &&
			(Config.Eab.sKid == NULL) && (Config.Eab.sHmac == NULL) &&
			(Config.sContactEmail == NULL),
		"acme account config init fields mismatch"
	);

	printf("[PASS] acme account config\n");
	return 0;
}
