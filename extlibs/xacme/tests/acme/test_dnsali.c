#include "../test.h"

#include "../../src/internal/xacme_dnstxt.h"
#include "../../src/internal/xacme_dns_ali_internal.h"
#include <xrt/json.h>

#include <stdlib.h>
#include <string.h>

/* 固定向量由 Python 标准库 hashlib/hmac 按 ACS3 公式独立计算。 */
static void testAliAuthorization(void)
{
	static const char* sExpected =
		"ACS3-HMAC-SHA256 Credential=YourAccessKeyId,"
		"SignedHeaders=content-type;host;x-acs-action;"
		"x-acs-content-sha256;x-acs-date;x-acs-signature-nonce;"
		"x-acs-version,Signature="
		"3806914bf37aaee501a248f85ec70ad690232e17cd00b2c3380dbab45a2f93da";
	static const char* sNonce = "00112233445566778899aabbccddeeff";
	char sAuth[640];
	char sDate[24];
	char sQuery[1024];
	char sShort[8];
	xtime Now;
	testRequire(xrtTimeFromUnix(INT64_C(1698315752), &Now),
		"acme dns_ali vector time creation failed");
	testRequire(xacmeDnsAliAuthorization(
		"YourAccessKeyId", "YourAccessKeySecret",
		"alidns.aliyuncs.com", "AddDomainRecord",
		"Value=abc-_123&Type=TXT&RR=_acme-challenge&DomainName=example.com",
		sNonce, Now, sAuth, sizeof(sAuth), sDate, sizeof(sDate),
		sQuery, sizeof(sQuery)) &&
		strcmp(sDate, "2023-10-26T10:22:32Z") == 0 &&
		strcmp(sQuery,
			"DomainName=example.com&RR=_acme-challenge&Type=TXT&Value=abc-_123")
			== 0 && strcmp(sAuth, sExpected) == 0,
		"acme dns_ali ACS3 authorization vector mismatch");
	testRequire(xacmeDnsAliAuthorization(
		"YourAccessKeyId", "YourAccessKeySecret",
		"alidns.aliyuncs.com", "AddDomainRecord",
		"DomainName=example.com&RR=_acme-challenge&Type=TXT&Value=abc-_123",
		sNonce, Now, sAuth, sizeof(sAuth), sDate, sizeof(sDate),
		sQuery, sizeof(sQuery)) && strcmp(sAuth, sExpected) == 0,
		"acme dns_ali query order changed signature");
	testRequire(xacmeDnsAliAuthorization(
		"YourAccessKeyId", "YourAccessKeySecret",
		"alidns.aliyuncs.com", "AddDomainRecord",
		"DomainName=example.com&RR=_acme-challenge&Type=TXT&Value=abc-_123",
		"00112233445566778899aabbccddeefe", Now, sAuth,
		sizeof(sAuth), sDate, sizeof(sDate), sQuery, sizeof(sQuery)) &&
		strcmp(sAuth, sExpected) != 0,
		"acme dns_ali changed nonce did not change signature");
	xrtClearError();
	testRequire(!xacmeDnsAliAuthorization(
		"YourAccessKeyId", "YourAccessKeySecret",
		"alidns.aliyuncs.com", "AddDomainRecord",
		"DomainName=example.com", sNonce, Now,
		sShort, sizeof(sShort), sDate, sizeof(sDate),
		sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali truncated authorization was accepted");
	xrtClearError();
	testRequire(!xacmeDnsAliAuthorization(
		"YourAccessKeyId", "YourAccessKeySecret",
		"alidns.aliyuncs.com", "AddDomainRecord",
		"DomainName=example.com", sNonce, Now,
		sAuth, sizeof(sAuth), sShort, sizeof(sShort),
		sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali truncated date was accepted");
	xrtClearError();
	testRequire(!xacmeDnsAliCanonicalQuery(
		"DomainName=example.com&DomainName=other.com",
		sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali duplicate query key was accepted");
	xrtClearError();
	testRequire(!xacmeDnsAliCanonicalQuery(
		"RecordId=123%26Action=DeleteDomainRecord",
		sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali encoded delimiter was accepted");
	xrtClearError();
	testRequire(!xacmeDnsAliCanonicalQuery(
		"RecordId=123=bad", sQuery, sizeof(sQuery)) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali unencoded delimiter was accepted");
	xrtClearError();
}

static void testAliCreateResponseId(void)
{
	char sId[320];
	char sLongId[320];
	char sResponse[350];
	size_t i;
	testRequire(xacmeDnsAliCreateResponseId(
		"{\"RecordId\":\"12345-abc_DEF.~\"}", sId, sizeof(sId)) &&
		strcmp(sId, "12345-abc_DEF.~") == 0,
		"acme dns_ali valid create response id was rejected");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{\"RecordId\":\"123&Action=DeleteDomainRecord\"}",
		sId, sizeof(sId)) && sId[0] == '\0',
		"acme dns_ali create response query injection was accepted");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{\"RecordId\":\"123%26bad\"}", sId, sizeof(sId)) &&
		sId[0] == '\0',
		"acme dns_ali create response encoded delimiter was accepted");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{\"RecordId\":\"123\\nHeader: injected\"}", sId, sizeof(sId)) &&
		sId[0] == '\0',
		"acme dns_ali create response control character was accepted");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{\"RecordId\":12345}", sId, sizeof(sId)) &&
		sId[0] == '\0',
		"acme dns_ali non-string create response id was accepted");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{}", sId, sizeof(sId)) && sId[0] == '\0',
		"acme dns_ali missing create response id was accepted");
	testRequire(!xacmeDnsAliCreateResponseId(
		"{\"RecordId\":\"123\",\"Code\":\"Forbidden\"}", sId, sizeof(sId)) && sId[0] == '\0',
		"acme dns_ali error envelope established record ownership");
	for(i = 0u; i < sizeof(sLongId) - 1u; ++i)
	{
		sLongId[i] = 'A';
	}
	sLongId[sizeof(sLongId) - 1u] = '\0';
	snprintf(sResponse, sizeof(sResponse),
		"{\"RecordId\":\"%s\"}", sLongId);
	testRequire(xacmeDnsAliCreateResponseId(
		sResponse, sId, sizeof(sId)) &&
		strlen(sId) == sizeof(sId) - 1u,
		"acme dns_ali maximum storable id was rejected");
	testRequire(!xacmeDnsAliCreateResponseId(
		sResponse, sId, sizeof(sId) - 1u) &&
		sId[0] == '\0',
		"acme dns_ali truncated create response id was accepted");
}

static void testAliZoneResponse(void)
{
	const char* valid = "{\"TotalCount\":0,\"PageNumber\":1,\"PageSize\":1,"
		"\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}";
	const char* invalid[] = {
		"", "{", "[]", "{}",
		"{\"Code\":\"InvalidDomainName.NoExist\\u0000extra\"}",
		"{\"TotalCount\":\"0\",\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":-1,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":0.0,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":0,\"PageNumber\":2,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":0,\"PageNumber\":1,\"PageSize\":20,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":1,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":0,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\\u0000extra\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":0,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":\"invalid\"}}",
		"{\"TotalCount\":0,\"TotalCount\":0,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[]}}",
		"{\"TotalCount\":1,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[{\"DomainName\":\"other.com\",\"RecordId\":\"123\"}]}}"
	};
	testRequire(xacmeDnsAliZoneResponse(200u, valid, "example.com") == XACME_ALI_ZONE_FOUND,
		"Ali valid empty discovery page rejected");
	testRequire(xacmeDnsAliZoneResponse(200u,
		"{\"TotalCount\":9,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\",\"DomainRecords\":{\"Record\":[{\"DomainName\":\"EXAMPLE.COM\",\"RecordId\":\"123\"}]}}",
		"example.com") == XACME_ALI_ZONE_FOUND, "Ali valid full discovery page rejected");
	for(size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		xrtClearError();
		testRequire(xacmeDnsAliZoneResponse(200u, invalid[i], "example.com") == XACME_ALI_ZONE_ERROR &&
			xrtErrorKind(xrtGetError()) == XERR_PROTOCOL, "Ali invalid discovery page accepted");
	}
	testRequire(xacmeDnsAliZoneResponse(400u, "{\"Code\":\"InvalidDomainName.NoExist\"}", "example.com") ==
		XACME_ALI_ZONE_MISSING, "Ali explicit missing domain rejected");
	testRequire(xacmeDnsAliZoneResponse(404u, "{\"Code\":\"DomainNotFound\"}", "example.com") ==
		XACME_ALI_ZONE_MISSING, "Ali missing domain alias rejected");
	testRequire(xacmeDnsAliZoneResponse(200u, "{\"Code\":\"DomainNotFound\"}", "example.com") ==
		XACME_ALI_ZONE_ERROR, "Ali success error envelope permitted fallback");
	testRequire(xacmeDnsAliZoneResponse(403u, "{\"Code\":\"InvalidDomainName.NoExist\"}", "example.com") ==
		XACME_ALI_ZONE_ERROR && xrtErrorKind(xrtGetError()) == XERR_PERMISSION,
		"Ali permission status permitted missing-domain fallback");
	xrtClearError();
}

static void testAliZoneResponseOom(void)
{
	const char* body = "{\"TotalCount\":9,\"PageNumber\":1,\"PageSize\":1,\"RequestId\":\"fixture\","
		"\"DomainRecords\":{\"Record\":[{\"DomainName\":\"example.com\",\"RecordId\":\"123\"}]}}";
	bool complete = false;
	size_t failures = 0u;
	testRequire(xrtMemDebugEnable(true), "Ali discovery OOM tracer failed");
	for(size_t i = 0u; i < 256u; i++) {
		xmemdebugsnapshot before, after;
		xacmednsalizoneoutcome result;
		bool triggered;
		xrtClearError(); xrtMemDebugSnapshot(&before);
		xrtMemDebugFailAfter(i);
		result = xacmeDnsAliZoneResponse(200u, body, "example.com");
		triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		if(triggered) {
			testRequire(result == XACME_ALI_ZONE_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
				"Ali discovery OOM lost original memory failure");
			failures++;
		} else {
			testRequire(result == XACME_ALI_ZONE_FOUND, "Ali discovery did not recover after OOM scan");
			complete = true;
		}
		xrtClearError(); xrtMemDebugSnapshot(&after);
		testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
			before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount,
			"Ali discovery OOM leaked resources");
		if(complete) break;
	}
	testRequire(complete && failures != 0u, "Ali discovery OOM scan was incomplete");
	printf("Ali discovery parser OOM scan: %zu positions, original errors and release verified\n", failures);
}

static void testAliRecordResponse(void)
{
	const char* fields[] = { "123", "EXAMPLE.COM", "_ACME-CHALLENGE.API", "digest_A", "TXT", "default", "Enable", "fixture" };
	const char* wrong[] = { "outside", "other.com", "unrelated", "DIGEST_A", "CNAME", "telecom", "unknown", "" };
	const char* missing = "{\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"fixture\"}";
	const char* invalid[] = {
		"", "{", "[]", "{}", "{\"Code\":17}",
		"{\"Code\":\"DomainRecordNotBelongToUser\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"fixture\\u0000extra\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\\u0000extra\",\"RequestId\":\"fixture\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"fixture\",\"RecordId\":\"123\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\",\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"fixture\"}",
		"{\"RecordId\":\"123\\u0000extra\",\"RequestId\":\"fixture\"}"
	};
	char body[1024];
	bool enabled;
	for(size_t i = 0u; i <= 14u; i++) {
		const char* f[8];
		memcpy(f, fields, sizeof(f));
		if(i < 8u) f[i] = wrong[i];
		if(i == 9u) f[6] = "Disable";
		if(i == 10u) f[6] = "ENABLE";
		if(i == 11u) f[6] = "DISABLE";
		if(i == 12u) f[6] = "enable";
		if(i == 13u) f[6] = "ENABLED";
		if(i == 14u) f[6] = "Enable\\u0000extra";
		snprintf(body, sizeof(body), "{\"RecordId\":\"%s\",\"DomainName\":\"%s\",\"RR\":\"%s\",\"Value\":\"%s\","
			"\"Type\":\"%s\",\"Line\":\"%s\",\"Status\":\"%s\",\"RequestId\":\"%s\"}",
			f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7]);
		xrtClearError(); enabled = true;
		testRequire(xacmeDnsAliRecordResponse(200u, body, "123", "_acme-challenge.api.example.com", "digest_A", &enabled) ==
			(i < 8u || i > 11u ? XACME_ALI_RECORD_ERROR : XACME_ALI_RECORD_FOUND) && enabled == (i == 8u || i == 10u),
			"Ali record tuple or enabled status was misclassified");
	}
	for(size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		xrtClearError(); enabled = true;
		testRequire(xacmeDnsAliRecordResponse(400u, invalid[i], "123", "owner", "digest", &enabled) == XACME_ALI_RECORD_ERROR &&
			!enabled && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL, "Ali malformed missing-record envelope accepted");
	}
	for(unsigned status = 0u; status < 5u; status++) {
		const uint16 statuses[] = { 400u, 404u, 200u, 403u, 503u };
		xrtClearError();
		testRequire(xacmeDnsAliRecordResponse(statuses[status], missing, "123", "owner", "digest", NULL) ==
			(status < 2u ? XACME_ALI_RECORD_MISSING : XACME_ALI_RECORD_ERROR), "Ali unsafe missing-record status accepted");
		if(status >= 2u) testRequire(xrtErrorKind(xrtGetError()) ==
			(status == 3u ? XERR_PERMISSION : status == 4u ? XERR_IO : XERR_PROTOCOL), "Ali record failure classification lost");
	}
	{
		const char* absent = "{\"Code\":\"InvalidRR.NoExist\",\"RequestId\":\"fixture\"}";
		const uint16 statuses[] = { 400u, 404u, 200u, 403u, 503u };
		for(size_t i = 0u; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
			xrtClearError(); enabled = true;
			testRequire(xacmeDnsAliRecordResponse(statuses[i], absent, "123", "owner", "digest", &enabled) ==
				(i < 2u ? XACME_ALI_RECORD_MISSING : XACME_ALI_RECORD_ERROR) && !enabled,
				"Ali actual absent-record response was misclassified");
		}
	}
	xrtClearError();
	testRequire(xacmeDnsAliRecordResponse(200u, NULL, "123", "owner", "digest", NULL) == XACME_ALI_RECORD_ERROR &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "Ali null record response accepted");
	xrtClearError();
}

static void testAliRecordResponseOom(void)
{
	const char* bodies[] = {
		"{\"RecordId\":\"123\",\"DomainName\":\"example.com\",\"RR\":\"_acme-challenge.api\",\"Value\":\"digest_A\","
		"\"Type\":\"TXT\",\"Line\":\"default\",\"Status\":\"Enable\",\"RequestId\":\"fixture\"}",
		"{\"Code\":\"DomainRecordNotBelongToUser\",\"RequestId\":\"fixture\"}"
	};
	testRequire(xrtMemDebugEnable(true), "Ali record OOM tracer failed");
	for(size_t b = 0u; b < 2u; b++) {
		bool complete = false;
		size_t failures = 0u;
		for(size_t i = 0u; i < 256u; i++) {
			xmemdebugsnapshot before, after;
			xacmednsalirecordoutcome result;
			bool enabled = true, triggered;
			xrtClearError(); xrtMemDebugSnapshot(&before); xrtMemDebugFailAfter(i);
			result = xacmeDnsAliRecordResponse(b == 0u ? 200u : 404u, bodies[b], "123", "_acme-challenge.api.example.com", "digest_A", &enabled);
			triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
			if(triggered) {
				testRequire(result == XACME_ALI_RECORD_ERROR && !enabled && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
					"Ali record parser OOM lost its error or accepted ownership");
				failures++;
			} else {
				testRequire(result == (b == 0u ? XACME_ALI_RECORD_FOUND : XACME_ALI_RECORD_MISSING) && enabled == (b == 0u),
					"Ali record parser failed to recover after OOM");
				complete = true;
			}
			xrtClearError(); xrtMemDebugSnapshot(&after);
			testRequire(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
				before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount,
				"Ali record parser OOM leaked resources");
			if(complete) break;
		}
		testRequire(complete && failures != 0u, "Ali record parser OOM scan incomplete");
		printf("Ali record parser OOM scan %zu: %zu positions, original errors and release verified\n", b, failures);
	}
}

/* Live Add/Remove requires explicit opt-in and an authorized TXT tuple. */
int main(void)
{
	testAliAuthorization();
	testAliCreateResponseId();
	testAliZoneResponse();
	testAliZoneResponseOom();
	testAliRecordResponse();
	testAliRecordResponseOom();
	const char* sKey = getenv("XACME_ALI_KEY");
	const char* sSecret = getenv("XACME_ALI_SECRET");
	const char* sLive = getenv("XACME_LIVE");
	const char* sFqdn = getenv("XACME_ALI_FQDN");
	const char* sValue = getenv("XACME_ALI_VALUE");
	xacmednaliconfig Config;
	xacmednsprovider Provider;
	char sLongKey[161];
	bool added = false, ok = false;
	size_t pending = 0u;

	/* 超长凭据必须在发起任何网络请求前拒绝，不能静默截断。 */
	memset(sLongKey, 'A', sizeof(sLongKey) - 1u);
	sLongKey[sizeof(sLongKey) - 1u] = '\0';
	xrtAcmeDnsAliConfigInit(&Config);
	Config.sAccessKeyId = sLongKey;
	Config.sAccessKeySecret = "secret";
	testRequire(!xrtAcmeDnsAli(&Config, NULL, &Provider) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali overlong credential must be rejected");
	xrtClearError();
	Config.sAccessKeyId = "key";
	Config.sEndpoint = sLongKey;
	testRequire(!xrtAcmeDnsAli(&Config, NULL, &Provider) &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"acme dns_ali overlong endpoint must be rejected");
	xrtClearError();
	Config.sEndpoint = NULL;
	testRequire(xrtAcmeDnsAli(&Config, NULL, &Provider),
		"acme dns_ali offline provider construct failed");
	testRequire(!Provider.Add(&Provider,
		XRT_STR_LITERAL("_acme-challenge.example.com"),
		XRT_STR_LITERAL("unsafe&value")) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"acme dns_ali query delimiter must be rejected");
	xrtAcmeDnsAliProviderUnit(&Provider);
	xrtClearError();

	if(sLive == NULL || strcmp(sLive, "1") != 0)
	{
		puts("[SKIP] acme dns_ali live (requires XACME_LIVE=1)");
		return 0;
	}
	if(sKey == NULL || sKey[0] == '\0' || sSecret == NULL ||
		sSecret[0] == '\0' || sFqdn == NULL || sFqdn[0] == '\0' ||
		sValue == NULL || sValue[0] == '\0')
	{
		fputs("[FAIL] live DNS test requires ALI_KEY, ALI_SECRET, "
			"ALI_FQDN and a unique ALI_VALUE runtime configuration\n", stderr);
		return 2;
	}

	xrtAcmeDnsAliConfigInit(&Config);
	Config.sAccessKeyId = sKey;
	Config.sAccessKeySecret = sSecret;
	testRequire(
		xrtAcmeDnsAli(&Config, NULL, &Provider),
		"acme dns_ali construct failed"
	);

	added = Provider.Add(&Provider, xrtStrView(sFqdn), xrtStrView(sValue));
	if(!added)
	{
		fprintf(stderr, "[FAIL] DNS Add: kind=%d code=%d\n",
			(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
		goto LiveDone;
	}

	/* 库内 TXT 探测器确认公共 resolver 可见（60s 上限）。 */
	{
		xacmedns Probe;
		if(xacmeDnsInit(&Probe, NULL))
		{
			ok = xacmeDnsTxtWait(&Probe, "223.5.5.5", 53u,
				sFqdn, sValue, 60000u);
			xacmeDnsUnit(&Probe);
		}
		if(!ok) fprintf(stderr, "[FAIL] DNS propagation: kind=%d code=%d\n",
			(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
	}
LiveDone:
	/* Propagation failures must still attempt exact owned-TXT cleanup. */
	if(added && !Provider.Remove(&Provider, xrtStrView(sFqdn), xrtStrView(sValue)))
	{
		fprintf(stderr, "[FAIL] DNS Remove: kind=%d code=%d\n",
			(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
		ok = false;
	}
	xrtAcmeDnsAliProviderUnit(&Provider);
	if(Provider.pContext != NULL ||
		!xrtAcmeCleanupPending(INT64_C(30), &pending) || pending != 0u)
		ok = false;
	testRequire(ok, "acme dns_ali live acceptance failed");
	puts("[PASS] acme dns_ali live");
	return 0;
}
