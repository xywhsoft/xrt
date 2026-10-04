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

static bool testDnsDelete(void* pContext, cstr sId, xstrview sFqdn, xstrview sTxt)
{
	testdnsdeletecontext* pTest = (testdnsdeletecontext*)pContext;
	testRequire(xacmeDnsChallengeValid(sFqdn, sTxt),
		"record cleanup must receive the matching owner and TXT identity");
	pTest->iCalls++;
	return (pTest->sFailId == NULL) || strcmp(pTest->sFailId, sId) != 0;
}

static void testDnsJsonPathIds(void)
{
	static const cstr BadIds[] = {
		"{\"id\":\"\"}",
		"{\"id\":\".\"}",
		"{\"id\":\"..\"}",
		"{\"id\":\"part/other\"}",
		"{\"id\":\"part\\u002fother\"}",
		"{\"id\":\"part?other\"}",
		"{\"id\":\"part#other\"}",
		"{\"id\":\"part%2Fother\"}",
		"{\"id\":\"part\\u0000other\"}",
		"{\"id\":\"part\\r\\nother\"}",
		"{\"id\":\"part other\"}"
	};
	char sId[80];
	xvalue* pRoot;
	size_t i;
	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"id\":\"abcdef0123456789\",\"name\":\"example.com\"}"));
	testRequire(pRoot != NULL &&
		xacmeDnsJsonPathId(pRoot, "id", sId, sizeof(sId)) &&
		strcmp(sId, "abcdef0123456789") == 0,
		"acme DNS must accept a plain provider path id");
	xrtValueRelease(pRoot);
	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}"));
	testRequire(pRoot != NULL &&
		xacmeDnsJsonPathId(pRoot, "id", sId, sizeof(sId)),
		"acme DNS must accept a UUID provider path id");
	xrtValueRelease(pRoot);
	for(i = 0u; i < sizeof(BadIds) / sizeof(BadIds[0]); i++)
	{
		pRoot = xrtJsonParse((xstrview){ BadIds[i], strlen(BadIds[i]) });
		testRequire(pRoot != NULL &&
			!xacmeDnsJsonPathId(pRoot, "id", sId, sizeof(sId)),
			"acme DNS must reject an unsafe provider path id");
		xrtValueRelease(pRoot);
	}
	pRoot = xrtJsonParse(XRT_STR_LITERAL(
		"{\"name\":\"example.com\\u0000.other\"}"));
	testRequire(pRoot != NULL &&
		!xacmeDnsJsonText(pRoot, "name", sId, sizeof(sId)),
		"acme DNS must reject a NUL-suffixed zone name");
	xrtValueRelease(pRoot);
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"success\":true,\"result\":[{\"name\":\"example.com\",\"id\":\"zone-123\"}]}"),
		"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_FOUND && strcmp(sId, "zone-123") == 0 &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL("{\"success\":true,\"result\":[]}"),
			"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_MISSING,
		"acme DNS zone lookup must distinguish found from absent");
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"success\":false,\"result\":[]}"),
		"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL("{\"result\":[]}"),
			"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL(
			"{\"success\":\"true\",\"result\":[]}"),
			"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR,
		"acme DNS Cloudflare failure status must not become parent-zone miss");
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"success\":true,\"result\":[{\"name\":\"example.com\",\"id\":\"zone?bad\"}]}"),
		"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL(
			"{\"success\":true,\"result\":[{\"name\":\"other\\u0000bad\",\"id\":\"x\"}]}"),
			"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL("{\"success\":true,\"result\":{}}"),
			"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR,
		"acme DNS malformed zone response must not become parent-zone miss");
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"zones\":[{\"name\":\"example.com.\","
		"\"id\":\"123e4567-e89b-12d3-a456-426614174000\"}]}"),
		"zones", "example.com.", false, 2u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_FOUND,
		"acme DNS Huawei-style zone response must accept a UUID id");
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"success\":true,\"result\":["
		"{\"name\":\"example.com\",\"id\":\"first\"},"
		"{\"name\":\"example.com\",\"id\":\"second\"}]}"),
		"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR && sId[0] == '\0',
		"acme DNS duplicate exact zones must be rejected");
	testRequire(xacmeDnsJsonZoneId(XRT_STR_LITERAL(
		"{\"success\":true,\"result\":["
		"{\"name\":\"example.com\",\"id\":\"first\"},"
		"{\"name\":\"a\"},{\"name\":\"b\"},"
		"{\"name\":\"c\"},{\"name\":\"d\"}]}"),
		"result", "example.com", true, 5u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR && sId[0] == '\0' &&
		xacmeDnsJsonZoneId(XRT_STR_LITERAL(
			"{\"zones\":[{\"name\":\"example.com.\",\"id\":\"first\"},"
			"{\"name\":\"other.com.\",\"id\":\"second\"}]}"),
			"zones", "example.com.", false, 2u, sId, sizeof(sId)) ==
			XACME_DNS_ZONE_ERROR && sId[0] == '\0',
		"acme DNS full zone page must not select an unverified first result");
}

/*
	Cloudflare 真实联测（环境门控）：
	  XACME_CF_TOKEN —— API Token（zone DNS Edit 权限）
	  XACME_CF_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add → 公共 resolver 确认可见 → Remove。
*/

int main(void)
{
	testDnsJsonPathIds();
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
	{
		char sEndpoint[160];
		char sZone[244];
		char sPath[300];
		char sUrl[512];
		char sShort[400];
		int iPathSize;
		memset(sEndpoint, 'e', sizeof(sEndpoint) - 1u);
		sEndpoint[sizeof(sEndpoint) - 1u] = '\0';
		memset(sZone, 'z', sizeof(sZone) - 1u);
		sZone[60] = sZone[121] = sZone[182] = '.';
		sZone[sizeof(sZone) - 1u] = '\0';
		iPathSize = snprintf(sPath, sizeof(sPath),
			"/client/v4/zones?name=%s&per_page=5", sZone);
		testRequire(iPathSize > 0 && (size_t)iPathSize < sizeof(sPath) &&
			xacmeDnsHttpsUrl(sUrl, sizeof(sUrl), sEndpoint, sPath) &&
			strlen(sUrl) == 8u + strlen(sEndpoint) + strlen(sPath) &&
			strcmp(sUrl + 8u + strlen(sEndpoint), sPath) == 0,
			"acme dns_cf long zone URL must retain the complete query");
		sShort[0] = 'x';
		xrtClearError();
		testRequire(!xacmeDnsHttpsUrl(sShort, sizeof(sShort),
			sEndpoint, sPath) && sShort[0] == '\0' &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_cf short URL buffer must fail closed");
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
