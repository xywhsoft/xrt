#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include <xrt/acme_dns_huawei.h>

#include <stdlib.h>
#include <string.h>

/*
	华为云 DNS 真实联测（环境门控）：
	  XACME_HUAWEI_AK / XACME_HUAWEI_SK —— AccessKey/SecretKey
	  XACME_HUAWEI_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add（SDK-HMAC-SHA256 + zone 上探 + recordsets）→
	公共 resolver 确认 → Remove。
*/

int main(void)
{
	const char* sAk = getenv("XACME_HUAWEI_AK");
	const char* sSk = getenv("XACME_HUAWEI_SK");
	const char* sFqdnEnv = getenv("XACME_HUAWEI_FQDN");
	cstr sFqdn = ((sFqdnEnv != NULL) && (sFqdnEnv[0] != '\0')) ?
		sFqdnEnv : "_acme-challenge.test.xxrpa.com";
	const char* sValue = "xacme-live-probe-20260915";
	xacmednshuaaweiconfig Config;
	xacmednsprovider Provider;

	/* 参数错误语义（常跑）。 */
	{
		xacmednshuaaweiconfig Bad;
		xacmednsprovider BadProvider;
		xrtAcmeDnsHuaweiConfigInit(&Bad);
		Bad.sAccessKey = "ak-only";
		xrtClearError();
		testRequire(
			!xrtAcmeDnsHuawei(&Bad, NULL, &BadProvider) &&
				(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
			"acme dns_huawei missing secret mismatch"
		);
	}

	if((sAk == NULL) || (sAk[0] == '\0') || (sSk == NULL) ||
		(sSk[0] == '\0'))
	{
		printf("[SKIP] acme dns_huawei live (XACME_HUAWEI_AK unset)\n");
		return 0;
	}

	xrtAcmeDnsHuaweiConfigInit(&Config);
	Config.sAccessKey = sAk;
	Config.sSecretKey = sSk;
	testRequire(
		xrtAcmeDnsHuawei(&Config, NULL, &Provider),
		"acme dns_huawei construct failed"
	);

	if(!Provider.Add(&Provider, (xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }))
	{
		xrtAcmeDnsHuaweiProviderUnit(&Provider);
		testRequire(false, "acme dns_huawei add failed");
	}

	{
		xacmedns Probe;
		testRequire(
			xacmeDnsInit(&Probe, NULL),
			"acme dns_huawei probe init failed"
		);
		testRequire(
			xacmeDnsTxtWait(&Probe, "223.5.5.5", 53u, sFqdn, sValue,
				60000u),
			"acme dns_huawei txt not visible"
		);
		xacmeDnsUnit(&Probe);
	}
	printf("[huawei] TXT added and visible via resolver\n");

	testRequire(
		Provider.Remove(&Provider,
			(xstrview){ sFqdn, strlen(sFqdn) },
			(xstrview){ sValue, strlen(sValue) }),
		"acme dns_huawei remove failed"
	);

	xrtAcmeDnsHuaweiProviderUnit(&Provider);
	printf("[PASS] acme dns_huawei live\n");
	return 0;
}
