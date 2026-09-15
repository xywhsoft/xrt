#include "../test.h"

#include "../../src/internal/xacme_flow.h"
#include <xrt/acme.h>

#include <stdlib.h>
#include <string.h>

/*
	端到端真实联测（环境门控）：
	  XACME_LIVE=1 以及 XACME_ALI_KEY / XACME_ALI_SECRET。
	流程：ali provider（V3 + 公共 resolver 传播确认）→ LE staging
	完整签发 → 断言证书链。域名固定 test.xxrpa.com（非通配，
	通配语义已由 CSR/Pebble 覆盖）。
*/

int main(void)
{
	const char* sLive = getenv("XACME_LIVE");
	const char* sKey = getenv("XACME_ALI_KEY");
	const char* sSecret = getenv("XACME_ALI_SECRET");
	xacmednaliconfig AliConfig;
	xacmednsprovider Ali;
	xacmeclient Client;
	xacmeaccountconfig Account;
	xstrview Domains[1];
	xacmeissuegrant Grant;

	if((sLive == NULL) || (sLive[0] == '\0') || (sKey == NULL) ||
		(sKey[0] == '\0') || (sSecret == NULL) || (sSecret[0] == '\0'))
	{
		printf("[SKIP] acme flow live (XACME_LIVE unset)\n");
		return 0;
	}

	xrtAcmeDnsAliConfigInit(&AliConfig);
	AliConfig.sAccessKeyId = sKey;
	AliConfig.sAccessKeySecret = sSecret;
	testRequire(
		xrtAcmeDnsAli(&AliConfig, NULL, &Ali),
		"acme live ali construct failed"
	);

	xrtAcmeAccountConfigInit(&Account);
	Account.sDirectoryUrl = XACME_DIRECTORY_LE_STAGING;
	if(!xacmeClientInit(&Client, NULL, NULL, &Account, 0u))
	{
		const xerror* pE = xrtGetError();
		const xerror* pC = pE;
		int i;
		printf("[diag] init failed kind=%d\n", xrtErrorKind(pE));
		for(i = 0; (i < 6) && (pC != NULL); i++)
		{
			printf("[diag]   L%d kind=%d dom=%s msg=%.140s\n",
				i, xrtErrorKind(pC), xrtErrorDomain(pC),
				xrtErrorMessage(pC) ? xrtErrorMessage(pC) : "?");
			pC = xrtErrorCause(pC);
		}
		xrtAcmeDnsAliProviderUnit(&Ali);
		testRequire(false, "acme live client init failed");
	}
	printf("[live] kid=%s\n", Client.sKid);

	Domains[0] = XRT_STR_LITERAL("test.xxrpa.com");
	if(!xacmeClientIssue(&Client, Domains, 1u, &Ali, &Grant))
	{
		const xerror* pE = xrtGetError();
		const xerror* pC = pE;
		int i;
		printf("[diag] issue failed kind=%d code=%d\n",
			xrtErrorKind(pE), xrtErrorCode(pE));
		for(i = 0; (i < 6) && (pC != NULL); i++)
		{
			printf("[diag]   L%d kind=%d code=%d dom=%s msg=%.160s\n",
				i, xrtErrorKind(pC), xrtErrorCode(pC),
				xrtErrorDomain(pC),
				xrtErrorMessage(pC) ? xrtErrorMessage(pC) : "?");
			pC = xrtErrorCause(pC);
		}
		/* 尽力清理遗留 TXT。 */
		(void)Ali.Remove(&Ali,
			XRT_STR_LITERAL("_acme-challenge.test.xxrpa.com"),
			(xstrview){ NULL, 0u });
		xacmeClientUnit(&Client);
		xrtAcmeDnsAliProviderUnit(&Ali);
		testRequire(false, "acme live issue failed");
	}

	testRequire(
		(Grant.sFullchainPem != NULL) &&
			(strstr(Grant.sFullchainPem, "-----BEGIN CERTIFICATE-----") !=
				NULL),
		"acme live chain missing certificate"
	);
	testRequire(
		(Grant.sKeyPem != NULL) &&
			(strstr(Grant.sKeyPem, "BEGIN PRIVATE KEY") != NULL),
		"acme live grant missing private key"
	);
	{
		char sDump[320];
		FILE* f;
		snprintf(sDump, sizeof(sDump), "%s/le_staging_issued.pem",
			testOutRoot());
		f = fopen(sDump, "wb");
		if(f != NULL)
		{
			fwrite(Grant.sFullchainPem, 1u,
				strlen(Grant.sFullchainPem), f);
			fclose(f);
		}
	}
	printf("[live] chain bytes=%zu saved\n", strlen(Grant.sFullchainPem));
	xrtAcmeGrantUnit(&Grant);

	xacmeClientUnit(&Client);
	xrtAcmeDnsAliProviderUnit(&Ali);
	printf("[PASS] acme flow live LE staging\n");
	return 0;
}
