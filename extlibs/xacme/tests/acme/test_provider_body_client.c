/* Exercise actual provider request builders with deliberately nonzero buffer slack.
 * HTTP responses are controlled; no provider credentials or external service are used. */
#define XACME_MODULE_DNS_CF
#define XACME_MODULE_DNS_TENCENT
#define XACME_MODULE_DNS_HUAWEI
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include "../../src/acme/xacme_http.c"

static unsigned Scenario;
static size_t PayloadSize;
static unsigned JsonCalls;
static bool FailTerminator;
static bool TerminationFailureTriggered;
static char CfRecord[1400];
static char HwRecord[1100];
static bool HwDeleted;
static char TcZone[256], TcRr[256], TcValue[201];
static unsigned TerminatorAttempts, FailTerminatorAt;

static bool poison_slack(xbuffer* buffer)
{
	if(!xrtBufferReserve(buffer, buffer->Size + 16u)) return false;
	PayloadSize = buffer->Size;
	if(PayloadSize != 0u && buffer->Data[PayloadSize - 1u] == 0u) PayloadSize--;
	memset(buffer->Data + buffer->Size, 'X', buffer->Capacity - buffer->Size);
	buffer->Data[buffer->Capacity - 1u] = 0u;
	return true;
}

static bool body_append(xbuffer* buffer, xbytesview bytes)
{
	return xrtBufferAppend(buffer, bytes) && poison_slack(buffer);
}

static bool body_append_byte(xbuffer* buffer, uint8 byte)
{
	if(byte == 0u) TerminatorAttempts++;
	if(byte == 0u && FailTerminator && TerminatorAttempts == FailTerminatorAt) {
		bool ok;
		/* Force this real append to grow, then fail its next allocation. */
		if(!xrtBufferTrim(buffer) || !xrtMemDebugFailAfter(0u)) return false;
		ok = xrtBufferAppendByte(buffer, byte);
		TerminationFailureTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		return ok;
	}
	return xrtBufferAppendByte(buffer, byte) && poison_slack(buffer);
}

static bool provider_exchange(xacmehttp* http, cstr method, cstr url, cstr type,
	xstrview body, const xacmehttpheader* extra, size_t count, xacmehttpresponse* response)
{
	cstr json;
	char sAck[1400];
	char requested_zone[256] = { 0 };
	(void)http; (void)url;
	if(body.Size != 0u) {
		xvalue* root;
		JsonCalls++;
		cstr want_type = Scenario != 0u && Scenario != 4u ?
			"application/json; charset=utf-8" : "application/json";
		if(body.Size != PayloadSize || type == NULL || strcmp(type, want_type) != 0) {
			fprintf(stderr, "scenario=%u JSON bytes=%zu transmitted bytes=%zu\n",
				Scenario, PayloadSize, body.Size);
			return false;
		}
		root = xrtJsonParse(body);
		if(root == NULL || !xrtValueIs(root, XVALUE_OBJECT)) {
			xrtValueRelease(root); return false;
		}
		if(Scenario != 0u && Scenario != 4u) {
			xstrview zone;
			if(xrtValueGetString(xrtValueObjectGet(root, XRT_STR_LITERAL("Domain")), &zone) && zone.Size < sizeof(requested_zone)) {
				memcpy(requested_zone, zone.Data, zone.Size); requested_zone[zone.Size] = '\0';
			}
		}
		xrtValueRelease(root);
	}
	if(Scenario == 0u) {
		json = strcmp(method, "GET") == 0 ?
			"{\"success\":true,\"result\":[{\"name\":\"example.com\",\"id\":\"zone\"}]}" :
			"{\"success\":true,\"result\":{\"id\":\"record\"}}";
		if(strcmp(method, "GET") == 0 && strstr(url, "/dns_records/") != NULL) json = CfRecord;
		if(strcmp(method, "POST") == 0) {
			snprintf(sAck, sizeof(sAck), "{\"success\":true,\"errors\":[],\"result\":{\"id\":\"record\",%.*s}}",
				(int)(body.Size - 2u), body.Data + 1u); json = sAck;
			strcpy(CfRecord, sAck);
		}
	} else if(Scenario == 4u) {
		json = strcmp(method, "GET") == 0 ?
			"{\"zones\":[{\"name\":\"example.com.\",\"id\":\"zone\"}]}" : "{\"id\":\"record\"}";
		if(strcmp(method, "POST") == 0) {
			snprintf(sAck, sizeof(sAck), "{\"id\":\"record\",\"zone_id\":\"zone\",%.*s}",
				(int)(body.Size - 2u), body.Data + 1u); json = sAck;
			strcpy(HwRecord, sAck);
		} else if(strstr(url, "/recordsets/") != NULL) {
			if(strcmp(method, "GET") == 0 && HwDeleted) json = "{\"error_code\":\"DNS.0313\",\"error_msg\":\"record not found\"}";
			else {
				snprintf(sAck, sizeof(sAck), "%.*s,\"default\":false,\"status\":\"%s\"}",
					(int)(strlen(HwRecord) - 1u), HwRecord,
					strcmp(method, "DELETE") == 0 ? "PENDING_DELETE" : "ACTIVE"); json = sAck;
			}
			if(strcmp(method, "DELETE") == 0) HwDeleted = true;
		}
	} else {
		cstr action = NULL;
		for(size_t i = 0u; i < count; i++) if(strcmp(extra[i].sName, "X-TC-Action") == 0) action = extra[i].sValue;
		if(action != NULL && strcmp(action, "DescribeDomain") == 0) {
			if(strcmp(requested_zone, TcZone) == 0)
				snprintf(sAck, sizeof(sAck), "{\"Response\":{\"DomainInfo\":{\"Domain\":\"%s\",\"DomainId\":101},\"RequestId\":\"probe\"}}", TcZone);
			else
				snprintf(sAck, sizeof(sAck), "{\"Response\":{\"Error\":{\"Code\":\"InvalidParameterValue.DomainNotExists\",\"Message\":\"missing\"},\"RequestId\":\"probe\"}}");
			json = sAck;
		} else if(action != NULL && strcmp(action, "DescribeRecord") == 0) {
			snprintf(sAck, sizeof(sAck), "{\"Response\":{\"RecordInfo\":{\"Id\":7,\"DomainId\":101,\"SubDomain\":\"%s\",\"RecordType\":\"TXT\",\"RecordLine\":\"默认\",\"RecordLineId\":\"0\",\"Enabled\":1,\"Value\":\"%s\"},\"RequestId\":\"probe\"}}", TcRr, TcValue); json = sAck;
		} else json = "{\"Response\":{\"RecordId\":7,\"RequestId\":\"probe\"}}";
	}
	memset(response, 0, sizeof(*response));
	response->iStatus = 200u;
	if(Scenario == 4u && strstr(url, "/recordsets/") != NULL)
		response->iStatus = strcmp(method, "DELETE") == 0 ? 202u : HwDeleted ? 404u : 200u;
	response->iBodySize = strlen(json);
	response->sBody = xrtMalloc(strlen(json) + 1u);
	if(response->sBody == NULL) return false;
	memcpy(response->sBody, json, strlen(json) + 1u);
	return true;
}

#define xrtBufferAppend body_append
#define xrtBufferAppendByte body_append_byte
#define xacmeHttpExchangeV provider_exchange
#include "../../src/dns/xacme_dns_cf.c"
#include "../../src/dns/xacme_dns_tencent.c"
#include "../../src/dns/xacme_dns_huawei.c"
#undef xacmeHttpExchangeV
#undef xrtBufferAppendByte
#undef xrtBufferAppend

static bool empty_memory(void)
{
	xmemdebugsnapshot snapshot;
	xrtClearError();
	xrtMemDebugSnapshot(&snapshot);
	if(snapshot.LiveCount != 0u || snapshot.LiveBytes != 0u ||
		snapshot.InvalidFreeCount != 0u || snapshot.DoubleFreeCount != 0u) {
		fprintf(stderr, "provider body leak: %zu allocations / %zu bytes\n",
			snapshot.LiveCount, snapshot.LiveBytes);
		return false;
	}
	return true;
}

static bool exercise(unsigned scenario, xstrview fqdn, xstrview txt, bool fail)
{
	xacmednscfcontext cf = { 0 };
	xacmednstencentcontext tc = { 0 };
	xacmednshuaaweicontext hw = { 0 };
	xacmednsprovider provider = { 0 };
	xacmednsrecords* records;
	xmutex* lock;
	xacmehttp* http;
	bool ok;
	Scenario = scenario; PayloadSize = 0u; JsonCalls = 0u;
	FailTerminator = fail; TerminationFailureTriggered = false;
	HwDeleted = false;
	TerminatorAttempts = 0u; FailTerminatorAt = scenario == 2u ? 3u : scenario == 3u ? 2u : 1u;
	xrtClearError();
	if(scenario == 0u) {
		strcpy(cf.sToken, "test-token"); strcpy(cf.sEndpoint, "api.example.test");
		provider.pContext = &cf; provider.Add = xacmeCfAdd; provider.Remove = xacmeCfRemove;
		records = &cf.Records;
		lock = &cf.Lock;
		http = &cf.Http;
	} else if(scenario == 4u) {
		strcpy(hw.sAk, "test-id"); strcpy(hw.sSk, "test-key");
		strcpy(hw.sEndpoint, "api.example.test");
		provider.pContext = &hw; provider.Add = xacmeHuaweiAdd; provider.Remove = xacmeHuaweiRemove;
		records = &hw.Records;
		lock = &hw.Lock;
		http = &hw.Http;
	} else {
		cstr zone = strchr(fqdn.Data, '.') + 1u;
		strcpy(tc.sId, "test-id"); strcpy(tc.sKey, "test-key");
		strcpy(tc.sEndpoint, "api.example.test");
		provider.pContext = &tc; provider.Add = xacmeTencentAdd; provider.Remove = xacmeTencentRemove;
		records = &tc.Records;
		lock = &tc.Lock;
		http = &tc.Http;
		snprintf(TcZone, sizeof(TcZone), "%s", zone);
		memcpy(TcRr, fqdn.Data, (size_t)(zone - fqdn.Data - 1)); TcRr[zone - fqdn.Data - 1] = '\0';
		memcpy(TcValue, txt.Data, txt.Size); TcValue[txt.Size] = '\0';
		if(scenario == 3u || scenario == 5u) {
			if(!xacmeDnsRecordRememberPair(records, zone, '|', "7", fqdn, txt)) return false;
			tc.iDomainIds[0] = 101; tc.uCreatedAt[0] = xrtTimer();
		}
	}
	if(!xrtMutexInit(lock)) return false;
	/* Public callbacks now require a live transport even though this probe
	 * replaces exchanges to inspect request bytes. Retain real resource owners. */
	if(!xacmeHttpInit(http, NULL, NULL, 2000000u)) {
		(void) xacmeHttpUnit(http); (void)xrtMutexUnit(lock); return false;
	}
	ok = scenario == 3u || scenario == 5u ? provider.Remove(&provider, fqdn, txt) : provider.Add(&provider, fqdn, txt);
	if(fail) {
		ok = !ok && TerminationFailureTriggered && JsonCalls == FailTerminatorAt - 1u &&
			xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
			(scenario == 3u || scenario == 5u ? records->sIds[0][0] != '\0' : records->iCount == 0u);
	} else {
		if(scenario != 3u && scenario != 5u) ok = ok && records->iCount == 1u && provider.Remove(&provider, fqdn, txt);
		ok = ok && records->sIds[0][0] == '\0' &&
			JsonCalls == (scenario == 1u || scenario == 2u ? 5u : scenario == 3u || scenario == 5u ? 2u : 1u);
	}
	FailTerminator = false;
	if(!xacmeHttpUnit(http)) ok = false;
	if(!xrtMutexUnit(lock)) ok = false;
	if(!empty_memory()) ok = false;
	if(!ok) fprintf(stderr, "provider request scenario=%u fail-terminator=%d failed\n", scenario, fail);
	else printf("  provider request scenario=%u fail-terminator=%d size=%zu: passed\n",
		scenario, fail, fqdn.Size);
	return ok;
}

int main(void)
{
	char label[64], fqdn[256], txt[201];
	bool ok = true;
	if(!xrtMemDebugEnable(true)) return 2;
	memset(label, 'a', 63u); label[63] = '\0';
	memset(txt, 'A', 200u); txt[200] = '\0';
	if(snprintf(fqdn, sizeof(fqdn), "_acme-challenge.%s.%s.%s.%.*s.example.com",
		label, label, label, 33, label) != 253) return 2;
	for(unsigned scenario = 0u; scenario < 6u; scenario++) {
		if(!exercise(scenario, XRT_STR_LITERAL("_acme-challenge.example.com"),
			XRT_STR_LITERAL("test-base64url-digest"), false)) ok = false;
		if(!exercise(scenario, (xstrview){ fqdn, 253u }, (xstrview){ txt, 200u }, false)) ok = false;
		if(!exercise(scenario, XRT_STR_LITERAL("_acme-challenge.example.com"),
			XRT_STR_LITERAL("test-base64url-digest"), true)) ok = false;
	}
	return ok ? 0 : 1;
}
