#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_dns_huawei_internal.h"
#include <xrt/acme_dns_huawei.h>
#include <xrt/json.h>
#include <xrt/value.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* 固定向量由 Python 标准库 hashlib/hmac 按华为云签名公式独立计算。 */
static void testHuaweiAuthorization(void)
{
	static const char* sGetExpected =
		"SDK-HMAC-SHA256 Access=AKIDEXAMPLE, "
		"SignedHeaders=content-type;host;x-sdk-date, "
		"Signature=0e2bd91df34a9c9269ada3cad1ea1d15e132057140d7d1559fd80c5c83d3b0a7";
	static const char* sExactGetExpected =
		"SDK-HMAC-SHA256 Access=AKIDEXAMPLE, "
		"SignedHeaders=content-type;host;x-sdk-date, "
		"Signature=6a4ff4b906cc4167b3c2e4d5150400af13e8eb703c771ad73beca20360214fad";
	static const char* sPostExpected =
		"SDK-HMAC-SHA256 Access=AKIDEXAMPLE, "
		"SignedHeaders=content-type;host;x-sdk-date, "
		"Signature=cac21b2d6c23a160328d0062a190bd9593d1b84da63508f4559a5a08dd9aa759";
	char sAuth[560];
	char sStamp[24];
	char sUri[160];
	char sQuery[160];
	char sShort[8];
	char sLongHost[400];
	char sMaxZone[254];
	char sMaxTarget[300];
	char sMaxQuery[320];
	char sMaxUrl[512];
	xtime Now;

	testRequire(xrtTimeFromUnix(INT64_C(1577934245), &Now),
		"acme dns_huawei vector time creation failed");
	testRequire(xacmeDnsHuaweiCanonicalTarget(
		"/v2/zones?name=example.com.&limit=1",
		sUri, sizeof(sUri), sQuery, sizeof(sQuery)) &&
		strcmp(sUri, "/v2/zones/") == 0 &&
		strcmp(sQuery, "limit=1&name=example.com.") == 0,
		"acme dns_huawei canonical GET target mismatch");
	testRequire(xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "GET",
		"/v2/zones?name=example.com.&limit=1", NULL, Now,
		sAuth, sizeof(sAuth), sStamp, sizeof(sStamp)) &&
		strcmp(sStamp, "20200102T030405Z") == 0 &&
		strcmp(sAuth, sGetExpected) == 0,
		"acme dns_huawei GET authorization vector mismatch");
	testRequire(xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "GET",
		"/v2/zones?limit=1&name=example.com.", NULL, Now,
		sAuth, sizeof(sAuth), sStamp, sizeof(sStamp)) &&
		strcmp(sAuth, sGetExpected) == 0,
		"acme dns_huawei query order changed signature");
	testRequire(xacmeDnsHuaweiCanonicalTarget(
		"/v2/zones?name=example.com.&limit=2&search_mode=equal",
		sUri, sizeof(sUri), sQuery, sizeof(sQuery)) &&
		strcmp(sUri, "/v2/zones/") == 0 &&
		strcmp(sQuery, "limit=2&name=example.com.&search_mode=equal") == 0,
		"acme dns_huawei exact zone query canonical target mismatch");
	testRequire(xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "GET",
		"/v2/zones?name=example.com.&limit=2&search_mode=equal", NULL, Now,
		sAuth, sizeof(sAuth), sStamp, sizeof(sStamp)) &&
		strcmp(sAuth, sExactGetExpected) == 0,
		"acme dns_huawei exact zone GET authorization vector mismatch");
	memset(sMaxZone, 'a', sizeof(sMaxZone) - 1u);
	sMaxZone[63] = '.';
	sMaxZone[127] = '.';
	sMaxZone[191] = '.';
	sMaxZone[sizeof(sMaxZone) - 1u] = '\0';
	testRequire(snprintf(sMaxTarget, sizeof(sMaxTarget),
		"/v2/zones?name=%s.&limit=2&search_mode=equal", sMaxZone) <
		(int)sizeof(sMaxTarget) &&
		xacmeDnsHuaweiCanonicalTarget(sMaxTarget, sUri, sizeof(sUri),
			sMaxQuery, sizeof(sMaxQuery)) &&
		xacmeDnsHuaweiBuildUrl(sMaxUrl, sizeof(sMaxUrl),
			"dns.myhuaweicloud.com", sMaxTarget),
		"acme dns_huawei longest zone query does not fit signing or URL buffers");
	testRequire(xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "POST",
		"/v2/zones/zone123/recordsets", "{\"records\":[\"abc\"]}", Now,
		sAuth, sizeof(sAuth), sStamp, sizeof(sStamp)) &&
		strcmp(sAuth, sPostExpected) == 0,
		"acme dns_huawei POST authorization vector mismatch");
	xrtClearError();
	testRequire(!xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "GET",
		"/v2/zones?name=example.com.&limit=1", NULL, Now,
		sAuth, sizeof(sAuth), sShort, sizeof(sShort)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei truncated timestamp was accepted");
	xrtClearError();
	testRequire(!xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", "dns.myhuaweicloud.com", "GET",
		"/v2/zones?name=example.com.&limit=1", NULL, Now,
		sShort, sizeof(sShort), sStamp, sizeof(sStamp)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei truncated authorization was accepted");
	memset(sLongHost, 'h', sizeof(sLongHost) - 1u);
	sLongHost[sizeof(sLongHost) - 1u] = '\0';
	xrtClearError();
	testRequire(!xacmeDnsHuaweiAuthorization("AKIDEXAMPLE",
		"SECRETEXAMPLE", sLongHost, "GET", "/v2/zones",
		NULL, Now, sAuth, sizeof(sAuth), sStamp, sizeof(sStamp)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei truncated signed host was accepted");
	xrtClearError();
	testRequire(!xacmeDnsHuaweiCanonicalTarget(
		"/v2/zones?name=example.com.%26evil=1",
		sUri, sizeof(sUri), sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei encoded query delimiter was accepted");
	xrtClearError();
	testRequire(!xacmeDnsHuaweiCanonicalTarget(
		"/v2/zones/../recordsets",
		sUri, sizeof(sUri), sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei relative path segment was accepted");
	xrtClearError();
	testRequire(!xacmeDnsHuaweiCanonicalTarget(
		"/v2/zones?name=example.com.=hidden",
		sUri, sizeof(sUri), sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_huawei unencoded query delimiter was accepted");
	xrtClearError();
}

/*
	华为云 DNS 真实联测（环境门控）：
	  XACME_HUAWEI_AK / XACME_HUAWEI_SK —— AccessKey/SecretKey
	  XACME_HUAWEI_FQDN —— TXT 属主（默认 _acme-challenge.test.xxrpa.com）
	流程：构造 → Add（SDK-HMAC-SHA256 + zone 上探 + recordsets）→
	公共 resolver 确认 → Remove。
*/

int main(void)
{
	testHuaweiAuthorization();
	{
		xbuffer Body;
		xvalue* pRoot;
		xvalue* pName;
		xvalue* pRecords;
		xvalue* pValue;
		xstrview Text;
		xrtBufferInit(&Body);
		testRequire(xacmeDnsHuaweiBuildCreateBody(&Body,
			XRT_STR_LITERAL("_acme-challenge.example.com"),
			XRT_STR_LITERAL("abc-_123")),
			"acme dns_huawei create body build failed");
		pRoot = xrtJsonParse((xstrview){ (cstr)Body.Data, Body.Size });
		testRequire(pRoot != NULL && xrtValueIs(pRoot, XVALUE_OBJECT),
			"acme dns_huawei create body must be valid JSON");
		pName = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("name"));
		testRequire(pName != NULL && xrtValueGetString(pName, &Text) &&
			Text.Size == strlen("_acme-challenge.example.com.") &&
			memcmp(Text.Data, "_acme-challenge.example.com.", Text.Size) == 0,
			"acme dns_huawei create name must include terminal dot");
		pRecords = xrtValueObjectGet(pRoot, XRT_STR_LITERAL("records"));
		pValue = (pRecords != NULL && xrtValueIs(pRecords, XVALUE_ARRAY) &&
			xrtValueCount(pRecords) == 1u) ?
			xrtValueArrayGet(pRecords, 0u) : NULL;
		testRequire(pValue != NULL && xrtValueGetString(pValue, &Text) &&
			Text.Size == strlen("\"abc-_123\"") &&
			memcmp(Text.Data, "\"abc-_123\"", Text.Size) == 0,
			"acme dns_huawei create TXT must be quoted");
		xrtValueRelease(pRoot);
		xrtBufferUnit(&Body);
	}
	{
		char sEndpoint[160];
		char sPath[300];
		char sUrl[512];
		char sShort[466];
		memset(sEndpoint, 'e', sizeof(sEndpoint) - 1u);
		sEndpoint[sizeof(sEndpoint) - 1u] = '\0';
		sPath[0] = '/';
		memset(sPath + 1u, 'p', sizeof(sPath) - 2u);
		sPath[sizeof(sPath) - 1u] = '\0';
		testRequire(xacmeDnsHuaweiBuildUrl(sUrl, sizeof(sUrl),
			sEndpoint, sPath) && strlen(sUrl) == 466u &&
			strncmp(sUrl, "https://", 8u) == 0 &&
			strcmp(sUrl + 8u + strlen(sEndpoint), sPath) == 0,
			"acme dns_huawei long URL must not truncate signed path");
		sShort[0] = 'x';
		xrtClearError();
		testRequire(!xacmeDnsHuaweiBuildUrl(sShort, sizeof(sShort),
			sEndpoint, sPath) && sShort[0] == '\0' &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_huawei short URL buffer must fail closed");
	}
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
	{
		char sTooLong[256];
		xacmednshuaaweiconfig Bad;
		xacmednsprovider BadProvider;
		memset(sTooLong, 'x', sizeof(sTooLong) - 1u);
		sTooLong[sizeof(sTooLong) - 1u] = '\0';
		xrtAcmeDnsHuaweiConfigInit(&Bad);
		Bad.sAccessKey = sTooLong;
		Bad.sSecretKey = "key";
		xrtClearError();
		testRequire(!xrtAcmeDnsHuawei(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_huawei must reject truncated access key");
		Bad.sAccessKey = "ak";
		Bad.sSecretKey = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsHuawei(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_huawei must reject truncated secret key");
		Bad.sSecretKey = "key";
		Bad.sEndpoint = sTooLong;
		xrtClearError();
		testRequire(!xrtAcmeDnsHuawei(&Bad, NULL, &BadProvider) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE,
			"acme dns_huawei must reject truncated endpoint");
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
