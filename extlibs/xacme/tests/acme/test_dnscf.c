#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_dnscommon.h"
#include <xrt/acme_dns_cf.h>

#include <stdlib.h>
#include <string.h>

typedef struct testdnsdeletecontext {
	size_t iCalls;
	cstr sFailId;
} testdnsdeletecontext;

static bool testDnsDelete(void* pContext, cstr sId)
{
	testdnsdeletecontext* pTest = (testdnsdeletecontext*)pContext;
	pTest->iCalls++;
	return (pTest->sFailId == NULL) || strcmp(pTest->sFailId, sId) != 0;
}

/*
	Cloudflare 真实联测（环境门控）：
	  XACME_CF_TOKEN —— API Token（zone DNS Edit 权限）
	  XACME_CF_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add → 公共 resolver 确认可见 → Remove。
*/

int main(void)
{
	/* 删除句柄要容纳两个完整的 32 字符 ID 和长 zone。 */
	{
		xacmednsrecords Records = { 0 };
		char sZoneId[33];
		char sRecordId[33];
		char sLongZone[254];
		char sOutZone[256];
		char sOutRecord[64];
		testdnsdeletecontext Delete = { 0 };
		xstrview Fqdn = XRT_STR_LITERAL("_acme-challenge.example.com");
		xstrview Txt = XRT_STR_LITERAL("first-value");
		xstrview OtherTxt = XRT_STR_LITERAL("second-value");
		testRequire(!xacmeDnsChallengeValid(
			(xstrview){ "_acme-challenge.example.com\0bad",
				sizeof("_acme-challenge.example.com\0bad") - 1u }, Txt) &&
			!xacmeDnsChallengeValid(Fqdn,
				XRT_STR_LITERAL("bad\"value")),
			"acme dns must reject embedded NUL and non-base64url TXT");
		memset(sZoneId, 'a', 32u);
		sZoneId[32] = '\0';
		memset(sRecordId, 'b', 32u);
		sRecordId[32] = '\0';
		testRequire(xacmeDnsRecordRememberPair(&Records,
			sZoneId, '/', sRecordId, Fqdn, Txt),
			"acme dns must remember full 32/32 record handle");
		testRequire(Records.iCount == 1u &&
			xacmeDnsRecordSplit(Records.sIds[0], '/', sOutZone,
				sizeof(sOutZone), sOutRecord, sizeof(sOutRecord)) &&
			strcmp(sOutZone, sZoneId) == 0 &&
			strcmp(sOutRecord, sRecordId) == 0,
			"acme dns must roundtrip full 32/32 record handle");
		memset(sLongZone, 'a', 253u);
		sLongZone[253] = '\0';
		testRequire(xacmeDnsRecordRememberPair(&Records,
			sLongZone, '|', "12345678901234567890", Fqdn,
			OtherTxt) &&
			xacmeDnsRecordSplit(Records.sIds[1], '|', sOutZone,
				sizeof(sOutZone), sOutRecord, sizeof(sOutRecord)) &&
			strcmp(sOutZone, sLongZone) == 0 &&
			strcmp(sOutRecord, "12345678901234567890") == 0,
			"acme dns must roundtrip long zone record handle");
		testRequire(xacmeDnsRecordRememberPair(&Records,
			"zone", '|', "2", Fqdn, Txt),
			"acme dns must remember second matching record");
		Delete.sFailId = "zone|2";
		testRequire(!xacmeDnsRecordRemoveMatching(&Records, Fqdn, Txt,
			testDnsDelete, &Delete) && Delete.iCalls == 2u &&
			Records.sIds[0][0] == '\0' &&
			Records.sIds[1][0] != '\0' &&
			Records.sIds[2][0] != '\0',
			"acme dns partial delete must retain failed and unrelated records");
		Delete.sFailId = NULL;
		Delete.iCalls = 0u;
		testRequire(xacmeDnsRecordRemoveMatching(&Records, Fqdn, Txt,
			testDnsDelete, &Delete) && Delete.iCalls == 1u &&
			Records.sIds[2][0] == '\0' && Records.sIds[1][0] != '\0',
			"acme dns delete retry must target only remaining matching record");
		testRequire(xacmeDnsRecordRememberPair(&Records,
			"zone", '|', "3", Fqdn, Txt) && Records.iCount == 3u,
			"acme dns must reuse released record slot");
	}
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
	{
		char sTooLong[256];
		xacmednscfconfig Bad;
		xacmednsprovider BadProvider;
		memset(sTooLong, 'x', sizeof(sTooLong) - 1u);
		sTooLong[sizeof(sTooLong) - 1u] = '\0';
		xrtAcmeDnsCfConfigInit(&Bad);
		Bad.sApiToken = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsCf(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_cf must reject truncated token");
		Bad.sApiToken = "token";
		Bad.sEndpoint = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsCf(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_cf must reject truncated endpoint");
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
