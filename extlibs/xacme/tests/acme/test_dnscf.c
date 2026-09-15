#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include <xrt/acme_dns_cf.h>

#include <stdlib.h>
#include <string.h>

/*
	Cloudflare 真实联测（环境门控）：
	  XACME_CF_TOKEN —— API Token（zone DNS Edit 权限）
	  XACME_CF_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add → 公共 resolver 确认可见 → Remove。
*/

int main(void)
{
	const char* sToken = getenv("XACME_CF_TOKEN");
	const char* sFqdnEnv = getenv("XACME_CF_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260915";
	xacmednscfconfig Config;
	xacmednsprovider Provider;

	/* 参数错误语义（常跑）。 */
	{
		xacmednscfconfig Bad;
		xacmednsprovider BadProvider;
		xrtAcmeDnsCfConfigInit(&Bad);
		xrtClearError();
		testRequire(
			!xrtAcmeDnsCf(&Bad, NULL, &BadProvider) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_cf missing token mismatch"
		);
	}

	if((sToken == NULL) || (sToken[0] == '\0'))
	{
		printf("[SKIP] acme dns_cf live (XACME_CF_TOKEN unset)\n");
		return 0;
	}

	xrtAcmeDnsCfConfigInit(&Config);
	Config.sApiToken = sToken;
	testRequire(
		xrtAcmeDnsCf(&Config, NULL, &Provider),
		"acme dns_cf construct failed"
	);

	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }))
	{
		xrtAcmeDnsCfProviderUnit(&Provider);
		testRequire(false, "acme dns_cf add failed");
	}

	{
		xacmedns Probe;
		testRequire(
			xacmeDnsInit(&Probe, NULL),
			"acme dns_cf probe init failed"
		);
		testRequire(
			xacmeDnsTxtWait(&Probe, "1.1.1.1", 53u, sFqdn, sValue, 60000u),
			"acme dns_cf txt not visible"
		);
		xacmeDnsUnit(&Probe);
	}
	printf("[cf] TXT added and visible via resolver\n");

	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_cf remove failed"
	);

	xrtAcmeDnsCfProviderUnit(&Provider);
	printf("[PASS] acme dns_cf live\n");
	return 0;
}
