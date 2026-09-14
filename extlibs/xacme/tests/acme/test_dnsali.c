#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"

#include <stdlib.h>
#include <string.h>

/*
	阿里云真实联测（环境门控）：
	  XACME_ALI_KEY / XACME_ALI_SECRET —— AK/SK
	  XACME_ALI_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 provider → Add（V3 签名 + zone 试探）→ 公共 resolver
	确认可见 → Remove → 复查不可见。
*/

int main(void)
{
	const char* sKey = getenv("XACME_ALI_KEY");
	const char* sSecret = getenv("XACME_ALI_SECRET");
	const char* sFqdnEnv = getenv("XACME_ALI_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260914";
	xacmednaliconfig Config;
	xacmednsprovider Provider;

	if((sKey == NULL) || (sKey[0] == '\0') || (sSecret == NULL) ||
		(sSecret[0] == '\0'))
	{
		printf("[SKIP] acme dns_ali live (XACME_ALI_KEY unset)\n");
		return 0;
	}

	xrtAcmeDnsAliConfigInit(&Config);
	Config.sAccessKeyId = sKey;
	Config.sAccessKeySecret = sSecret;
	Config.sVerifyResolver = "223.5.5.1";
	testRequire(
		xrtAcmeDnsAli(&Config, &Provider),
		"acme dns_ali construct failed"
	);

	/* Add（内部已含公共 resolver 传播确认，60s 上限）。 */
	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
		(xstrview){ sValue, strlen(sValue) }))
	{
		const xerror* pE = xrtGetError();
		const xerror* pC = pE;
		int i;
		printf("[diag] add failed kind=%d code=%d\n",
			xrtErrorKind(pE), xrtErrorCode(pE));
		for(i = 0; (i < 6) && (pC != NULL); i++)
		{
			printf("[diag]   L%d kind=%d code=%d dom=%s msg=%.120s\n",
				i, xrtErrorKind(pC), xrtErrorCode(pC),
				xrtErrorDomain(pC),
				xrtErrorMessage(pC) ? xrtErrorMessage(pC) : "?");
			pC = xrtErrorCause(pC);
		}
		xrtAcmeDnsAliProviderUnit(&Provider);
		testRequire(false, "acme dns_ali add failed");
	}
	printf("[ali] TXT added and visible via resolver\n");

	/* Remove 后复查：不再可见（允许解析器缓存，查询零记录或不含值）。 */
	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_ali remove failed"
	);
	printf("[ali] TXT removed\n");

	xrtAcmeDnsAliProviderUnit(&Provider);
	printf("[PASS] acme dns_ali live\n");
	return 0;
}
