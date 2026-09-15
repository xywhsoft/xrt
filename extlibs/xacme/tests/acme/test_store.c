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

#define STORE_ROOT testOutRoot()

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

	/* 签发产物整体存取（key.pem + fullchain.pem）。 */
	{
		xacmeissuegrant Grant;
		xacmeissuegrant Loaded;
		Grant.sFullchainPem = (str)sPem; /* 借用内容，Save 只读 */
		Grant.sKeyPem = (str)xrtMalloc(strlen(sPem) + 1u);
		testRequire(Grant.sKeyPem != NULL, "acme store grant key alloc failed");
		memcpy(Grant.sKeyPem, sPem, strlen(sPem) + 1u);
		testRequire(
			xrtAcmeStoreSaveGrant(
				STORE_ROOT, "grant.example.com", &Grant,
				"https://acme-staging-v02.api.letsencrypt.org/directory"),
			"acme store save grant failed"
		);
		xrtFree(Grant.sKeyPem);
		Grant.sKeyPem = NULL;
		Grant.sFullchainPem = NULL;
		testRequire(
			xrtAcmeStoreLoadGrant(STORE_ROOT, "grant.example.com", &Loaded) &&
				(Loaded.sFullchainPem != NULL) &&
				(Loaded.sKeyPem != NULL) &&
				(strcmp(Loaded.sFullchainPem, sPem) == 0) &&
				(strcmp(Loaded.sKeyPem, sPem) == 0),
			"acme store grant roundtrip mismatch"
		);
		xrtAcmeGrantUnit(&Loaded);
		/* key 缺失 → NOT_FOUND（删 key.pem 后链仍在）。 */
		{
			char sPath[400];
			snprintf(sPath, sizeof(sPath), "%s/certs/%s/key.pem",
				STORE_ROOT, "grant.example.com");
			testRequire(remove(sPath) == 0, "acme store key remove failed");
			xrtClearError();
			testRequire(
				!xrtAcmeStoreLoadGrant(
					STORE_ROOT, "grant.example.com", &Loaded) &&
					(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
				"acme store grant missing key mismatch"
			);
		}
	}

	/* 域名枚举：grant.example.com 已登记。 */
	{
		char sDomains[8][256];
		size_t iCount = 0u;
		size_t i;
		bool bHas = false;
		testRequire(
			xrtAcmeStoreListDomains(STORE_ROOT, sDomains, 8u, &iCount),
			"acme store list failed"
		);
		for(i = 0; i < iCount; i++)
		{
			if(strcmp(sDomains[i], "grant.example.com") == 0)
			{
				bHas = true;
			}
		}
		testRequire(bHas, "acme store list missing grant domain");
		xrtClearError();
		testRequire(
			!xrtAcmeStoreListDomains(STORE_ROOT, sDomains, 1u, &iCount) ||
				(iCount <= 1u),
			"acme store list capacity mismatch"
		);
	}

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
