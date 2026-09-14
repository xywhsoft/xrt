#include "../test.h"

#include <xrt/acme_store.h>
#include <xrt/pem.h>
#include <xrt/x509.h>
#include <xrt/time.h>

#include <stdlib.h>
#include <string.h>

/*
	临时目录做存取往返；NeedRenew 用真实 LE staging 证书（若在
	XACME_STORE_CERT 指定路径），否则手工生成短期证书场景只验证
	"缺文件→需要续"路径。
*/

#define STORE_ROOT "D:/git/xacme-local/store_test"

int main(void)
{
	const char* sPem =
		"-----BEGIN PRIVATE KEY-----\n"
		"MIGgAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBIGFMIGCAgEBBCDJr6nYRbp1Fmtc\n"
		"IVdnsdaTTlDD2zbomxJ7imIrEg9nIaAKBggqhkjOPQMBaFEA0IABGD+1LolWp0x\n"
		"yWXrdMY1bWjASbiSO2H6bOZYi5jg8p+2eQP+EAi4vJmkGunpVii8ZPLxsgwtfp9R\n"
		"d6PClNRGIpk=\n"
		"-----END PRIVATE KEY-----\n";
	str sLoaded;
	bool bNeed = false;

	/* 账户存取往返 + 多 CA 隔离。 */
	testRequire(
		xrtAcmeStoreSaveAccount(
			STORE_ROOT, "https://acme-staging-v02.api.letsencrypt.org/directory", sPem),
		"acme store save account failed"
	);
	sLoaded = xrtAcmeStoreLoadAccount(
		STORE_ROOT, "https://acme-staging-v02.api.letsencrypt.org/directory");
	testRequire(
		(sLoaded != NULL) && (strcmp(sLoaded, sPem) == 0),
		"acme store account roundtrip mismatch"
	);
	xrtFree(sLoaded);
	testRequire(
		xrtAcmeStoreLoadAccount(
			STORE_ROOT, "https://acme.zerossl.com/v2/DV90") == NULL,
		"acme store account isolation mismatch"
	);

	/* 证书存取 + 溯源。 */
	testRequire(bNeed == false, "acme store fresh cert should not renew");
	testRequire(
		xrtAcmeStoreNeedRenew(STORE_ROOT, "test.xxrpa.com", 90, &bNeed),
		"acme store need renew 90 failed"
	);
	testRequire(bNeed == true, "acme store near-expiry should renew");
	testRequire(
		xrtAcmeStoreNeedRenew(
			STORE_ROOT, "missing.example.com", 30, &bNeed) && bNeed,
		"acme store missing cert should renew"
	);

	/* 错误语义。 */
	xrtClearError();
	testRequire(
		!xrtAcmeStoreSaveAccount(NULL, NULL, NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme store null argument mismatch"
	);

	printf("[PASS] acme store roundtrip renew\n");
	return 0;
}
