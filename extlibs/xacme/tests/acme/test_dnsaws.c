#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include <xrt/acme_dns_aws.h>

#include <stdlib.h>
#include <string.h>

/*
	AWS Route53 真实联测（环境门控）：
	  XACME_AWS_KEY / XACME_AWS_SECRET —— AccessKeyId/SecretAccessKey
	  XACME_AWS_REGION —— 可选（默认 us-east-1）
	  XACME_AWS_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add（SigV4 + hostedzonesbyname 上探 + UPSERT）→
	公共 resolver 确认 → Remove（DELETE 回放）。
*/

int main(void)
{
	const char* sKey = getenv("XACME_AWS_KEY");
	const char* sSecret = getenv("XACME_AWS_SECRET");
	const char* sRegion = getenv("XACME_AWS_REGION");
	const char* sFqdnEnv = getenv("XACME_AWS_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260915";
	xacmednsawsconfig Config;
	xacmednsprovider Provider;

	/* 参数错误语义（常跑）。 */
	{
		xacmednsawsconfig Bad;
		xacmednsprovider BadProvider;
		xrtAcmeDnsAwsConfigInit(&Bad);
		xrtClearError();
		testRequire(
			!xrtAcmeDnsAws(&Bad, NULL, &BadProvider) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_aws missing credentials mismatch"
		);
	}

	if((sKey == NULL) || (sKey[0] == '\0') || (sSecret == NULL) ||
		(sSecret[0] == '\0'))
	{
		printf("[SKIP] acme dns_aws live (XACME_AWS_KEY unset)\n");
		return 0;
	}

	xrtAcmeDnsAwsConfigInit(&Config);
	Config.sAccessKeyId = sKey;
	Config.sSecretAccessKey = sSecret;
	Config.sRegion = sRegion;
	testRequire(
		xrtAcmeDnsAws(&Config, NULL, &Provider),
		"acme dns_aws construct failed"
	);

	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }))
	{
		xrtAcmeDnsAwsProviderUnit(&Provider);
		testRequire(false, "acme dns_aws add failed");
	}

	{
		xacmedns Probe;
		testRequire(
			xacmeDnsInit(&Probe, NULL),
			"acme dns_aws probe init failed"
		);
		testRequire(
			xacmeDnsTxtWait(&Probe, "8.8.8.8", 53u, sFqdn, sValue, 60000u),
			"acme dns_aws txt not visible"
		);
		xacmeDnsUnit(&Probe);
	}
	printf("[aws] TXT added and visible via resolver\n");

	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_aws remove failed"
	);

	xrtAcmeDnsAwsProviderUnit(&Provider);
	printf("[PASS] acme dns_aws live\n");
	return 0;
}
