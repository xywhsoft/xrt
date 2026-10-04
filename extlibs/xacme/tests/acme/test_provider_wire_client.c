/* Embed actual providers and transport; only replace the system trust source. */
#define XACME_MODULE_DNS_ALI
#define XACME_MODULE_DNS_AWS
#define XACME_MODULE_DNS_CF
#define XACME_MODULE_DNS_TENCENT
#define XACME_MODULE_DNS_HUAWEI
#define XACME_MODULE_ACME_FLOW
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include <xrt.h>
#include <xrt/acme_http.h>
#include <xrt/acme_dns.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* TestCa;
static bool UncertaintyErrorOom;
static unsigned UncertaintyOomTriggered;
static bool CreateParseOom;
static bool CreateParseOomTriggered;
static bool ZoneParseOom;
static bool ZoneParseOomTriggered;
static size_t ZoneParseAfter;
static bool ZoneParseSweep;
static xerror* ZoneParseError;
static bool RecordParseOom;
static bool RecordParseOomTriggered;
static unsigned RecordParseAfter;
static bool DeleteParseOom;
static bool DeleteParseOomTriggered;
static bool SigningFailure;
static bool RestPresendFailure;
static unsigned HuaweiDeleteCalls;
static bool HuaweiDeletePresendFailure;
static unsigned TencentDeleteCalls;
static unsigned TencentInventoryCalls;
static bool TencentDeletePresendFailure;
static uint64 TencentTestClock;
static bool DomainParseOom, DomainParseOomTriggered;
static bool ListParseOom, ListParseOomTriggered;
static uint64 tencent_clock(void) { return TencentTestClock != 0u ? TencentTestClock : xrtClock(); }
static bool ali_secure_random(ptr data, size_t size)
{
	if(SigningFailure) { xrtSetErrorKind(XERR_MEMORY); return false; }
	return xrtSecureRandom(data, size);
}
static xacmednsaddproc ActualAdd;
static unsigned FlowAddCalls;
static bool CaptureReadFailure;
static xerrkind FirstReadFailureKind;
static int32 FirstReadFailureCode;
static bool counted_add(xacmednsprovider* provider, xstrview fqdn, xstrview txt)
{
	bool ok;
	FlowAddCalls++;
	ok = ActualAdd(provider, fqdn, txt);
	if(CaptureReadFailure && !ok && FlowAddCalls == 1u) {
		FirstReadFailureKind = xrtErrorKind(xrtGetError());
		FirstReadFailureCode = xrtErrorCode(xrtGetError());
	}
	return ok;
}
static xvalue* create_json_parse(xstrview text)
{
	xvalue* result;
	bool zone = strstr(text.Data, "DomainRecords") != NULL || strstr(text.Data, "zone-probe") != NULL;
	bool record = !zone && (strstr(text.Data, "DomainName") != NULL || strstr(text.Data, "record-probe") != NULL);
	bool deleted = strstr(text.Data, "delete-probe") != NULL;
	bool domain = strstr(text.Data, "domain-probe") != NULL;
	bool list = strstr(text.Data, "list-probe") != NULL;
	if((DomainParseOom && domain) || (ListParseOom && list)) {
		xrtMemDebugFailAfter(0u); result = xrtJsonParse(text);
		if(domain) DomainParseOomTriggered = xrtMemDebugFailTriggered();
		else ListParseOomTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear(); return result;
	}
	if(RecordParseOom && record && RecordParseAfter != 0u) {
		RecordParseAfter--; return xrtJsonParse(text);
	}
	if(!(DeleteParseOom && deleted) && !(RecordParseOom && record) && !(ZoneParseOom && zone) && !(CreateParseOom && !zone && !record &&
		(strstr(text.Data, "RecordId") != NULL || strstr(text.Data, "create-probe") != NULL))) return xrtJsonParse(text);
	xrtMemDebugFailAfter((ZoneParseOom && zone) ? ZoneParseAfter : 0u);
	result = xrtJsonParse(text);
	if(DeleteParseOom && deleted) DeleteParseOomTriggered = xrtMemDebugFailTriggered();
	else if(RecordParseOom && record) RecordParseOomTriggered = xrtMemDebugFailTriggered();
	else if(ZoneParseOom && zone) ZoneParseOomTriggered = xrtMemDebugFailTriggered();
	else CreateParseOomTriggered = xrtMemDebugFailTriggered();
	xrtMemDebugFailClear();
	if(ZoneParseSweep && zone && ZoneParseOomTriggered) {
		ZoneParseError = xrtErrorRef(xrtGetError());
	}
	return result;
}
static xerror* wrap_error(const xerror* cause, xerrkind kind, cstr domain, int32 code, cstr message)
{
	if(UncertaintyErrorOom &&
		((strcmp(domain, "xrt.acme.http") == 0 && code == XACME_HTTP_ERROR_UNCERTAIN) ||
		 (strcmp(domain, "xrt.acme.dns") == 0 && code == XACME_DNS_ERROR_UNCERTAIN))) {
		xerror* result;
		xrtMemDebugFailAfter(0u);
		result = xrtErrorWrap(cause, kind, domain, code, message);
		if(xrtMemDebugFailTriggered()) UncertaintyOomTriggered++;
		xrtMemDebugFailClear();
		return result;
	}
	return xrtErrorWrap(cause, kind, domain, code, message);
}
static xx509store* test_store(void)
{
	xx509store* store = xrtX509StoreCreate();
	size_t added = 0u;
	if(store == NULL || !xrtX509StoreAddPem(store, TestCa, strlen(TestCa), &added) || !added) {
		xrtX509StoreFree(store); return NULL;
	}
	return store;
}
#define xrtX509StoreSystem test_store
#define xrtErrorWrap wrap_error
#include "../../src/acme/xacme_http.c"
#undef xrtX509StoreSystem
#define xrtJsonParse create_json_parse
#include "../../src/dns/xacme_dns_aws_txt.c"
#include "../../src/dns/xacme_dns_aws.c"
#define xrtSecureRandom ali_secure_random
#include "../../src/dns/xacme_dns_ali.c"
#undef xrtSecureRandom
static bool rest_exchange(xacmehttp* http, cstr method, cstr url, cstr type,
	xstrview body, const xacmehttpheader* extra, size_t count, xacmehttpresponse* response)
{
	if(RestPresendFailure && strcmp(method, "POST") == 0 && body.Size != 0u) {
		/* Fail before invoking the transport; the provider must reset old uncertainty. */
		xrtSetErrorKind(XERR_MEMORY); return false;
	}
	if(HuaweiDeletePresendFailure && strcmp(method, "DELETE") == 0) {
		xrtSetErrorKind(XERR_MEMORY); return false;
	}
	if(strcmp(method, "DELETE") == 0 && strstr(url, "/v2/zones/") != NULL) HuaweiDeleteCalls++;
	for(size_t i = 0u; i < count; i++) if(strcmp(extra[i].sName, "X-TC-Action") == 0 && strcmp(extra[i].sValue, "DescribeRecordList") == 0) TencentInventoryCalls++;
	for(size_t i = 0u; i < count; i++) if(strcmp(extra[i].sName, "X-TC-Action") == 0 && strcmp(extra[i].sValue, "DeleteRecord") == 0) {
		if(TencentDeletePresendFailure) { xrtSetErrorKind(XERR_MEMORY); return false; }
		TencentDeleteCalls++;
	}
	return xacmeHttpExchangeV(http, method, url, type, body, extra, count, response);
}
#define xacmeHttpExchangeV rest_exchange
#include "../../src/dns/xacme_dns_cf.c"
#define xrtClock tencent_clock
#include "../../src/dns/xacme_dns_tencent.c"
#undef xrtClock
#include "../../src/dns/xacme_dns_huawei.c"
#undef xacmeHttpExchangeV
#undef xrtJsonParse
#undef xrtErrorWrap
#include "../../src/acme/xacme_core.c"
#include "../../src/acme/xacme_csr.c"
#include "../../src/acme/xacme_jose.c"
#include "../../src/acme/xacme_store.c"
#include "../../src/acme/xacme_dns.c"
#include "../../src/dns/xacme_dnstxt.c"
#include "../../src/acme/xacme_flow.c"

static bool tracked(xacmednsprovider* provider, bool aws)
{
	if(aws) {
		xacmednsawscontext* context = provider->pContext;
		for(size_t i = 0u; i < context->iRecordCount; i++)
			if(context->Records[i].sFqdn[0] != '\0' && !context->Records[i].bUncertain) return true;
		return false;
	}
	{
		xacmednsalicontext* context = provider->pContext;
		for(size_t i = 0u; i < context->Records.iCount; i++)
			if(context->Records.sIds[i][0] != '\0') return true;
		return false;
	}
}

typedef struct parallel_add {
	xacmednsprovider* provider;
	xatomic32* start;
	xstrview owner;
	char value[40];
	bool ok;
} parallel_add;

static int32 add_in_thread(ptr data)
{
	parallel_add* task = data;
	xstrview owner = XRT_STR_LITERAL("_acme-challenge.api.example.com");
	if(task->owner.Data != NULL) owner = task->owner;
	while(xrtAtomic32Load(task->start, XMEMORY_ACQUIRE) == 0u) xrtSleep(1u);
	task->ok = task->provider->Add(task->provider,
		owner,
		(xstrview){ task->value, strlen(task->value) });
	xrtClearError();
	return task->ok ? 0 : 1;
}

static bool exercise(xacmednsprovider* p, bool aws, const char* mode)
{
	xstrview owner = XRT_STR_LITERAL("_acme-challenge.api.example.com");
	xstrview value = XRT_STR_LITERAL("first_digest-01");
	bool ok;
	if(aws && strncmp(mode, "aws-zone-", 9u) == 0) {
		xacmednsawscontext* context = p->pContext;
		xstrview first = XRT_STR_LITERAL("_acme-challenge.first.example.com");
		xstrview second = XRT_STR_LITERAL("second_digest-02");
		bool siblings = strcmp(mode, "aws-zone-parent-child") == 0 ||
			(strncmp(mode, "aws-zone-child-", 15u) == 0 && strcmp(mode, "aws-zone-child-replaced") != 0);
		if(strcmp(mode, "aws-zone-no-zone-stale-error") == 0)
			xrtSetErrorInfo(XERR_PERMISSION, "sentinel", 777, "prior operation failed");
		if(strncmp(mode, "aws-zone-no-zone", 16u) == 0)
			return !p->Add(p, owner, value) && context->iRecordCount == 0u &&
				xrtErrorKind(xrtGetError()) == XERR_PROTOCOL &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_ZONE && p->Remove(p, owner, value);
		if(strcmp(mode, "aws-zone-owner-apex") == 0)
			return p->Add(p, owner, value) && context->iRecordCount == 1u &&
				strcmp(context->Records[0].sZoneId, "ZAPEX") == 0 &&
				p->Remove(p, owner, value) && !tracked(p, true);
		if(strcmp(mode, "aws-zone-borrowed-refresh") == 0) {
			if(!p->Add(p, owner, value) || context->iRecordCount != 0u ||
				!p->Add(p, owner, value) || context->iRecordCount != 1u ||
				strcmp(context->Records[0].sZoneId, "ZCHILD") != 0) return false;
			return p->Remove(p, owner, value) && !tracked(p, true) && p->Remove(p, owner, value);
		}
		if(!p->Add(p, siblings ? first : owner, value) || context->iRecordCount != 1u ||
			strcmp(context->Records[0].sZoneId,
				strcmp(mode, "aws-zone-child-replaced") == 0 ? "ZCHILD" : "ZTEST") != 0) return false;
		if(strcmp(mode, "aws-zone-owned-pinned") == 0 || strcmp(mode, "aws-zone-owned-restore") == 0) {
			if(!p->Add(p, owner, value) || context->iRecordCount != 1u ||
				strcmp(context->Records[0].sZoneId, "ZTEST") != 0) return false;
		}
		if(strncmp(mode, "aws-zone-child-", 15u) == 0 &&
			strcmp(mode, "aws-zone-child-private") != 0 && strcmp(mode, "aws-zone-child-replaced") != 0) {
			if(p->Add(p, owner, second) || context->iRecordCount != 1u ||
				xrtErrorKind(xrtGetError()) != XERR_PROTOCOL ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_ZONE) return false;
			xrtClearError();
			if(!p->Remove(p, owner, second) || context->iRecordCount != 1u) return false;
		}
		if(!p->Add(p, owner, second) || context->iRecordCount != 2u ||
			strcmp(context->Records[1].sZoneId,
				strcmp(mode, "aws-zone-child-private") == 0 ? "ZTEST" :
				strcmp(mode, "aws-zone-child-replaced") == 0 ? "ZNEW" : "ZCHILD") != 0) return false;
		return p->Remove(p, siblings ? first : owner, value) &&
			p->Remove(p, owner, second) && !tracked(p, true) && p->Remove(p, owner, second);
	}
	if(aws && strncmp(mode, "aws-alias-", 10u) == 0 && strcmp(mode, "aws-alias-concurrent-same") != 0) {
		xstrview upper = XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM");
		xstrview mixed = XRT_STR_LITERAL("_AcMe-ChAlLeNgE.ApI.ExAmPlE.CoM");
		xacmednsawscontext* context = p->pContext;
		if(strncmp(mode, "aws-alias-uncertain", 19u) == 0) {
			xacmednsprovider flow = *p;
			xstrview second = XRT_STR_LITERAL("FIRST_DIGEST-01");
			xstrview first = strcmp(mode, "aws-alias-uncertain-upper") == 0 ? upper : owner;
			if(p->Add(p, first, value) || context->iRecordCount != 1u || !context->Records[0].bUncertain || tracked(p, true)) return false;
			xrtClearError();
			if(p->Remove(p, mixed, value) || xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			xrtClearError(); ActualAdd = p->Add; flow.Add = counted_add;
			if(xacmeFlowDnsAdd(&flow, upper.Data, value.Data) || FlowAddCalls != 1u || context->iRecordCount != 1u ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			xrtClearError();
			return p->Add(p, upper, second) && p->Remove(p, mixed, second) && !tracked(p, true) &&
				!p->Add(p, owner, value) && !p->Remove(p, upper, value) && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		}
		if(strcmp(mode, "aws-alias-capacity") == 0) {
			for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				if(!p->Add(p, i % 2u == 0u ? upper : mixed, (xstrview){ text, (size_t)n })) return false;
			}
			if(!p->Add(p, owner, XRT_STR_LITERAL("digest_0")) || context->iRecordCount != XACME_DNS_RECORD_MAX ||
				p->Add(p, mixed, XRT_STR_LITERAL("over_capacity")) || xrtErrorKind(xrtGetError()) != XERR_RANGE) return false;
			xrtClearError();
			for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				if(!p->Remove(p, owner, (xstrview){ text, (size_t)n })) return false;
			}
			return !tracked(p, true);
		}
		if(!p->Add(p, upper, value)) return false;
		if(strcmp(mode, "aws-alias-borrowed") == 0)
			return !tracked(p, true) && p->Add(p, owner, value) && context->iRecordCount == 0u &&
				p->Remove(p, mixed, value) && p->Remove(p, owner, value);
		if(strcmp(mode, "aws-alias-repeat-owned") == 0 &&
			(!p->Add(p, mixed, value) || context->iRecordCount != 1u)) return false;
		return p->Remove(p, mixed, value) && !tracked(p, true) && p->Remove(p, owner, value);
	}
	if(!aws && strcmp(mode, "rpc-signing-reset") == 0) {
		xacmednsalicontext* context = p->pContext;
		xstrview second = XRT_STR_LITERAL("second_digest-02");
		if(p->Add(p, owner, value) || !context->Http.bWriteUncertain || context->Records.iCount != 1u) return false;
		xrtClearError(); SigningFailure = true;
		ok = !p->Add(p, owner, second) && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
			!context->Http.bWriteUncertain && context->Records.iCount == 1u;
		SigningFailure = false; xrtClearError();
		return ok && p->Add(p, owner, second) && p->Remove(p, owner, second) &&
			!p->Add(p, owner, value) && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
	}
	if(!aws && strncmp(mode, "read-", 5u) == 0) {
		xacmednsalicontext* context = p->pContext;
		xacmednsprovider flow = *p;
		bool memory = strcmp(mode, "read-memory-wrap") == 0;
		bool limit = strcmp(mode, "read-flow-limit") == 0;
		if(!p->Add(p, owner, value)) return false;
		if(strcmp(mode, "read-delete-once") == 0)
			return p->Remove(p, owner, value) && !tracked(p, false) && p->Remove(p, owner, value);
		if(strcmp(mode, "read-repeat-exhausted") == 0) {
			if(p->Add(p, owner, value) || xrtErrorKind(xrtGetError()) != XERR_IO ||
				xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_UNCERTAIN || context->Http.bWriteUncertain ||
				!tracked(p, false) || context->bUncertain[0]) return false;
			xrtClearError();
		}
		ActualAdd = p->Add; flow.Add = counted_add;
		CaptureReadFailure = true; UncertaintyErrorOom = memory;
		ok = xacmeFlowDnsAdd(&flow, owner.Data, value.Data);
		if(memory || limit) {
			ok = !ok && FlowAddCalls == (memory ? 1u : 3u) &&
				xrtErrorKind(xrtGetError()) == (memory ? XERR_MEMORY : XERR_IO);
			if(memory) ok = ok && UncertaintyOomTriggered == 1u;
		} else ok = ok && FlowAddCalls == (strcmp(mode, "read-flow-exhausted") == 0 ? 2u : 1u);
		if(limit || strcmp(mode, "read-flow-exhausted") == 0)
			ok = ok && FirstReadFailureKind == XERR_IO && FirstReadFailureCode != XACME_HTTP_ERROR_UNCERTAIN;
		ok = ok && !context->Http.bWriteUncertain && tracked(p, false) && !context->bUncertain[0] && context->Records.iCount == 1u;
		CaptureReadFailure = false; UncertaintyErrorOom = false; xrtClearError();
		return ok && p->Remove(p, owner, value) && !tracked(p, false) && p->Remove(p, owner, value);
	}
	if(!aws && strncmp(mode, "collision-", 10u) == 0) {
		xacmednsalicontext* context = p->pContext;
		xstrview mixed = XRT_STR_LITERAL("_AcMe-ChAlLeNgE.ApI.ExAmPlE.CoM");
		xstrview second = XRT_STR_LITERAL("second_digest-02");
		if(strcmp(mode, "collision-last-slot") == 0) {
			for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				bool added = p->Add(p, mixed, (xstrview){ text, (size_t)n });
				if(i + 1u == XACME_DNS_RECORD_MAX) {
					if(added || xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN ||
						!context->bUncertain[i] || context->Records.sIds[i][0] != 0) return false;
				} else if(!added) return false;
			}
			xrtClearError();
			if(p->Add(p, owner, XRT_STR_LITERAL("over_capacity")) || xrtErrorKind(xrtGetError()) != XERR_RANGE) return false;
			xrtClearError();
			for(unsigned i = 0u; i + 1u < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				if(!p->Remove(p, owner, (xstrview){ text, (size_t)n })) return false;
			}
			return !tracked(p, false) && !p->Add(p, owner, XRT_STR_LITERAL("digest_7")) &&
				!p->Remove(p, mixed, XRT_STR_LITERAL("digest_7")) && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		}
		if(!p->Add(p, owner, value)) return false;
		if(strcmp(mode, "collision-legacy-slots") == 0) {
			char saved[XACME_DNS_RECORD_TEXT_CAP];
			if(!p->Add(p, owner, second)) return false;
			strcpy(saved, context->Records.sIds[1]);
			/* Simulate an older ledger with one id assigned to different tuples. */
			strcpy(context->Records.sIds[1], context->Records.sIds[0]);
			if(p->Remove(p, mixed, second) || xrtErrorKind(xrtGetError()) != XERR_PROTOCOL ||
				context->Records.sIds[0][0] == 0 || context->Records.sIds[1][0] == 0) return false;
			strcpy(context->Records.sIds[1], saved); xrtClearError();
			return p->Remove(p, owner, value) && p->Remove(p, owner, second) && !tracked(p, false);
		}
		UncertaintyErrorOom = strcmp(mode, "collision-error-oom") == 0;
		ok = !p->Add(p, mixed, second) && context->Records.iCount == 2u &&
			strcmp(context->Records.sIds[0], "11") == 0 && !context->bUncertain[0] &&
			context->Records.sIds[1][0] == 0 && context->bUncertain[1];
		if(UncertaintyErrorOom) ok = ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY && UncertaintyOomTriggered == 1u;
		else ok = ok && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		UncertaintyErrorOom = false; xrtClearError();
		if(!ok || p->Add(p, owner, second) || p->Remove(p, mixed, second) ||
			xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
		xrtClearError();
		if(!p->Remove(p, mixed, value) || !p->Add(p, owner, XRT_STR_LITERAL("third_digest")) ||
			!p->Remove(p, owner, XRT_STR_LITERAL("third_digest")) || tracked(p, false)) return false;
		return !p->Add(p, mixed, second) && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
	}
	if(!aws && strncmp(mode, "record-", 7u) == 0) {
		bool repeat = strncmp(mode, "record-repeat-", 14u) == 0;
		bool fail = strstr(mode, "-failure-") != NULL || strstr(mode, "-parse-oom") != NULL ||
			strcmp(mode, "record-repeat-disabled") == 0;
		xerrkind expected = strstr(mode, "-permission") != NULL ? XERR_PERMISSION :
			strstr(mode, "-network") != NULL ? XERR_IO : strstr(mode, "-parse-oom") != NULL ? XERR_MEMORY :
			strcmp(mode, "record-repeat-disabled") == 0 ? XERR_STATE : XERR_PROTOCOL;
		if(!p->Add(p, owner, value) || !tracked(p, false)) return false;
		RecordParseOom = strstr(mode, "-parse-oom") != NULL;
		ok = repeat ? p->Add(p, owner, value) : p->Remove(p, owner, value);
		if(fail) {
			ok = !ok && tracked(p, false) && xrtErrorKind(xrtGetError()) == expected;
			if(RecordParseOom) ok = ok && RecordParseOomTriggered;
		} else ok = ok && (repeat ? tracked(p, false) : !tracked(p, false));
		RecordParseOom = false; xrtClearError();
		if(strstr(mode, "-persistent") != NULL)
			return ok && !p->Remove(p, owner, value) && tracked(p, false) && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL;
		return ok && p->Remove(p, owner, value) && !tracked(p, false) && p->Remove(p, owner, value);
	}
	if(!aws && strncmp(mode, "alias-", 6u) == 0 && strcmp(mode, "alias-concurrent-same") != 0) {
		xstrview upper = XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM");
		xstrview mixed = XRT_STR_LITERAL("_AcMe-ChAlLeNgE.ApI.ExAmPlE.CoM");
		xacmednsalicontext* context = p->pContext;
		if(strncmp(mode, "alias-uncertain", 15u) == 0) {
			xacmednsprovider flow = *p;
			xstrview first = strcmp(mode, "alias-uncertain-upper") == 0 ? upper : owner;
			xstrview second = XRT_STR_LITERAL("FIRST_DIGEST-01");
			if(p->Add(p, first, value) || tracked(p, false) || context->Records.iCount != 1u ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			xrtClearError();
			if(p->Remove(p, mixed, value) || xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			xrtClearError();
			ActualAdd = p->Add; flow.Add = counted_add;
			if(xacmeFlowDnsAdd(&flow, upper.Data, value.Data) || FlowAddCalls != 1u ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN || context->Records.iCount != 1u) return false;
			xrtClearError();
			/* DNS names fold ASCII case; TXT digest bytes must remain distinct. */
			if(!p->Add(p, upper, second) || !tracked(p, false) ||
				!p->Remove(p, mixed, second) || tracked(p, false)) return false;
			return !p->Add(p, owner, value) && !p->Remove(p, upper, value) &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		}
		if(strcmp(mode, "alias-capacity-owned") == 0) {
			for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				if(n <= 0 || !p->Add(p, i % 2u == 0u ? upper : mixed, (xstrview){ text, (size_t)n })) return false;
			}
			if(!p->Add(p, owner, XRT_STR_LITERAL("digest_0")) || context->Records.iCount != XACME_DNS_RECORD_MAX ||
				p->Add(p, upper, XRT_STR_LITERAL("over_capacity")) || xrtErrorKind(xrtGetError()) != XERR_RANGE) return false;
			xrtClearError();
			for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
				if(n <= 0 || !p->Remove(p, owner, (xstrview){ text, (size_t)n })) return false;
			}
			return !tracked(p, false);
		}
		if(!p->Add(p, upper, value)) return false;
		if(strcmp(mode, "alias-repeat-owned") == 0 &&
			(!p->Add(p, mixed, value) || !p->Add(p, owner, value) || context->Records.iCount != 1u)) return false;
		return p->Remove(p, owner, value) && !tracked(p, false) && p->Remove(p, mixed, value);
	}
	if(!aws && strncmp(mode, "discovery-", 10u) == 0) {
		if(strcmp(mode, "discovery-long-owner") == 0) {
			char text[256]; size_t n = strlen("_acme-challenge.");
			memcpy(text, "_acme-challenge.", n);
			for(unsigned i = 0u; i < 3u; i++) { memset(text + n, 'a', 63u); n += 63u; text[n++] = '.'; }
			memcpy(text + n, "example.com", 12u); n += 11u;
			return p->Add(p, (xstrview){ text, n }, value) && p->Remove(p, (xstrview){ text, n }, value) && !tracked(p, false);
		}
		if(strcmp(mode, "discovery-no-zone") == 0) {
			for(unsigned i = 0u; i < 2u; i++) {
				if(p->Add(p, owner, value) || xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND ||
					xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_ZONE || tracked(p, false)) return false;
				xrtClearError();
			}
			return p->Remove(p, owner, value);
		}
		if(strcmp(mode, "discovery-cache-capacity") == 0) {
			const unsigned indices[] = { 0u, 1u, 2u, 3u, 4u, 5u, 0u, 5u };
			for(size_t i = 0u; i < sizeof(indices) / sizeof(indices[0]); i++) {
				char text[128]; int n = snprintf(text, sizeof(text), "_acme-challenge.name%u.example%u.com", indices[i], indices[i]);
				if(n <= 0 || !p->Add(p, (xstrview){ text, (size_t)n }, value) ||
					!p->Remove(p, (xstrview){ text, (size_t)n }, value)) return false;
			}
			return ((xacmednsalicontext*)p->pContext)->iZoneCount == XACME_ALI_ZONE_MAX && !tracked(p, false);
		}
		if(strcmp(mode, "discovery-specific-zone") == 0) {
			xstrview other = XRT_STR_LITERAL("_acme-challenge.other.example.com");
			return p->Add(p, other, value) && p->Add(p, owner, XRT_STR_LITERAL("child_digest")) &&
				p->Remove(p, other, value) && p->Remove(p, owner, XRT_STR_LITERAL("child_digest")) && !tracked(p, false);
		}
		if(!p->Add(p, owner, value) || !p->Remove(p, owner, value)) return false;
		return p->Add(p, XRT_STR_LITERAL("_acme-challenge.API.EXAMPLE.COM"), value) &&
			p->Remove(p, XRT_STR_LITERAL("_acme-challenge.API.EXAMPLE.COM"), value) && !tracked(p, false);
	}
	if(!aws && strncmp(mode, "zone-", 5u) == 0) {
		xacmednsalicontext* context = p->pContext;
		xerrkind expected = strcmp(mode, "zone-credential") == 0 ? XERR_PERMISSION :
			strcmp(mode, "zone-network") == 0 ? XERR_IO :
			strcmp(mode, "zone-busy") == 0 ? XERR_AGAIN :
			strcmp(mode, "zone-memory") == 0 ? XERR_MEMORY : XERR_PROTOCOL;
		ZoneParseOom = expected == XERR_MEMORY;
		ok = !p->Add(p, owner, value) && !tracked(p, false) &&
			context->Records.iCount == 0u && context->iZoneCount == 0u &&
			xrtErrorKind(xrtGetError()) == expected;
		if(expected != XERR_MEMORY) ok = ok && xrtErrorCode(xrtGetError()) ==
			(expected == XERR_PERMISSION ? XACME_DNS_ERROR_CREDENTIAL :
			 expected == XERR_PROTOCOL ? XACME_DNS_ERROR_PROTOCOL : XACME_DNS_ERROR_NETWORK);
		if(ZoneParseOom) ok = ok && ZoneParseOomTriggered;
		ZoneParseOom = false; xrtClearError();
		return ok && p->Remove(p, owner, value) && p->Add(p, owner, value) &&
			p->Remove(p, owner, value) && !tracked(p, false);
	}
	if(!aws && strncmp(mode, "flow-", 5u) == 0) {
		xacmednsprovider flow_provider = *p;
		ActualAdd = p->Add; flow_provider.Add = counted_add;
		CreateParseOom = strcmp(mode, "flow-parse-oom") == 0;
		UncertaintyErrorOom = strcmp(mode, "flow-uncertain-error-oom") == 0 ||
			strcmp(mode, "flow-diagnostic-oom") == 0;
		ok = !xacmeFlowDnsAdd(&flow_provider, owner.Data, value.Data) &&
			FlowAddCalls == 1u && !tracked(p, false);
		if(CreateParseOom) ok = ok && CreateParseOomTriggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		else if(UncertaintyErrorOom) ok = ok && UncertaintyOomTriggered == 1u && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		else ok = ok && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		if(!ok) fprintf(stderr, "flow add failed checks: kind=%d code=%d parse-oom=%u wrap-oom=%u\n",
			(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()),
			(unsigned)CreateParseOomTriggered, UncertaintyOomTriggered);
		CreateParseOom = false; UncertaintyErrorOom = false; xrtClearError();
		return ok && !p->Add(p, owner, value) && !p->Remove(p, owner, value) &&
			xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
	}
	if(!aws && (strcmp(mode, "ali-add-uncertain") == 0 ||
		strcmp(mode, "uncertain-isolation") == 0 || strcmp(mode, "capacity") == 0)) {
		ok = !p->Add(p, owner, value) && !tracked(p, false) &&
			xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		xrtClearError();
		if(!ok) return false;
		if(strcmp(mode, "uncertain-isolation") == 0) {
			xstrview second = XRT_STR_LITERAL("second_digest-02");
			if(!p->Add(p, owner, second) || !tracked(p, false) ||
				!p->Remove(p, owner, second) || tracked(p, false)) return false;
		}
		if(strcmp(mode, "capacity") == 0) {
			for(unsigned i = 1u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int size = snprintf(text, sizeof(text), "digest_%u", i);
				if(size <= 0 || !p->Add(p, owner, (xstrview){ text, (size_t)size })) return false;
			}
			if(p->Add(p, owner, XRT_STR_LITERAL("over_capacity")) ||
				xrtErrorKind(xrtGetError()) != XERR_RANGE) return false;
			xrtClearError();
			if(!p->Remove(p, owner, XRT_STR_LITERAL("digest_1")) ||
				!p->Add(p, owner, XRT_STR_LITERAL("reused_slot")) ||
				!p->Remove(p, owner, XRT_STR_LITERAL("reused_slot"))) return false;
			for(unsigned i = 2u; i < XACME_DNS_RECORD_MAX; i++) {
				char text[40]; int size = snprintf(text, sizeof(text), "digest_%u", i);
				if(size <= 0 || !p->Remove(p, owner, (xstrview){ text, (size_t)size })) return false;
			}
		}
		return !tracked(p, false) && !p->Add(p, owner, value) && !p->Remove(p, owner, value) &&
			xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
	}
	if((!aws && (strcmp(mode, "concurrent") == 0 || strcmp(mode, "alias-concurrent-same") == 0)) ||
		(aws && strcmp(mode, "aws-alias-concurrent-same") == 0)) {
		parallel_add tasks[XACME_DNS_RECORD_MAX] = { 0 };
		xthread* threads[XACME_DNS_RECORD_MAX] = { 0 };
		xatomic32 start = { 0u };
		unsigned count = 0u;
		bool same = strstr(mode, "concurrent-same") != NULL;
		xstrview aliases[] = { XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"),
			XRT_STR_LITERAL("_AcMe-ChAlLeNgE.ApI.ExAmPlE.CoM") };
		for(; count < XACME_DNS_RECORD_MAX; count++) {
			tasks[count].provider = p; tasks[count].start = &start;
			if(same) { tasks[count].owner = aliases[count % 2u]; strcpy(tasks[count].value, value.Data); }
			else snprintf(tasks[count].value, sizeof(tasks[count].value), "parallel_%u", count);
			threads[count] = xrtThreadCreate(add_in_thread, &tasks[count], 0u);
			if(threads[count] == NULL) break;
		}
		xrtAtomic32Store(&start, 1u, XMEMORY_RELEASE);
		ok = count == XACME_DNS_RECORD_MAX;
		for(unsigned i = 0u; i < count; i++) {
			if(xrtThreadWaitFor(threads[i], 60000000u) != XWAIT_OK) _Exit(2);
			ok = ok && tasks[i].ok; xrtThreadDestroy(threads[i]);
		}
		if(same) ok = ok && (aws ? ((xacmednsawscontext*)p->pContext)->iRecordCount :
			((xacmednsalicontext*)p->pContext)->Records.iCount) == 1u;
		for(unsigned i = 0u; i < count; i++)
			if(!p->Remove(p, owner, (xstrview){ tasks[i].value, strlen(tasks[i].value) })) ok = false;
		return ok && !tracked(p, aws);
	}
	if(!aws && strcmp(mode, "invalid-input") == 0) {
		const xstrview bad[] = { XRT_STR_LITERAL(".api.example.com"), XRT_STR_LITERAL("api..example.com"),
			XRT_STR_LITERAL("api.example.com."), XRT_STR_LITERAL("localhost"),
			XRT_STR_LITERAL("api.example.com\0suffix"), XRT_STR_LITERAL("api/example.com") };
		for(size_t i = 0u; i < sizeof(bad) / sizeof(bad[0]); i++) {
			if(p->Add(p, bad[i], value) || p->Remove(p, bad[i], value) ||
				xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return false;
			xrtClearError();
		}
		return !p->Add(p, owner, XRT_STR_LITERAL("bad&query")) &&
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT;
	}
	if(aws && (strcmp(mode, "add-uncertain") == 0 || strcmp(mode, "add-uncertain-oom") == 0 ||
		strcmp(mode, "uncertain-isolation") == 0)) {
		xacmednsawscontext* context = p->pContext;
		UncertaintyErrorOom = strcmp(mode, "add-uncertain-oom") == 0;
		ok = !p->Add(p, owner, value) && !tracked(p, true) &&
			context->iRecordCount == 1u && context->Records[0].bUncertain;
		if(UncertaintyErrorOom) ok = ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
			context->Http.bWriteUncertain && UncertaintyOomTriggered == 2u;
		else ok = ok && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		UncertaintyErrorOom = false; xrtClearError();
		if(strcmp(mode, "uncertain-isolation") == 0) {
			xstrview second = XRT_STR_LITERAL("second_digest-02");
			ok = ok && p->Add(p, owner, second) && tracked(p, true) && !context->Http.bWriteUncertain &&
				p->Remove(p, owner, second) && !tracked(p, true);
		}
		return ok && !p->Add(p, owner, value) && !p->Remove(p, owner, value) &&
			xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
	}
	if(aws && strcmp(mode, "borrowed") == 0) {
		return p->Add(p, owner, value) && !tracked(p, true) &&
			p->Add(p, owner, value) && !tracked(p, true) &&
			p->Remove(p, owner, value) && p->Remove(p, owner, value);
	}
	if(aws && strncmp(mode, "aws-error-", 10u) == 0) {
		xacmednsawscontext* context = p->pContext;
		if(strcmp(mode, "aws-error-recovered") == 0) {
			return p->Add(p, owner, value) && tracked(p, true) && xrtErrorKind(xrtGetError()) == XERR_NONE &&
				p->Remove(p, owner, value) && !tracked(p, true) && xrtErrorKind(xrtGetError()) == XERR_NONE &&
				p->Remove(p, owner, value);
		}
		if(strcmp(mode, "aws-error-uncertain") == 0) {
			xacmednsprovider flow_provider = *p;
			ActualAdd = p->Add; flow_provider.Add = counted_add;
			ok = !xacmeFlowDnsAdd(&flow_provider, owner.Data, value.Data) && FlowAddCalls == 1u &&
				context->iRecordCount == 1u && context->Records[0].bUncertain && !tracked(p, true) &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
			xrtClearError();
			return ok && !p->Add(p, owner, value) && !p->Remove(p, owner, value) &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		}
		if(strcmp(mode, "aws-error-delete-limit") == 0) {
			return p->Add(p, owner, value) && !p->Remove(p, owner, value) && tracked(p, true) &&
				xrtErrorKind(xrtGetError()) == XERR_AGAIN && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_NETWORK;
		}
		ok = !p->Add(p, owner, value) && context->iRecordCount == 0u;
		if(strcmp(mode, "aws-error-add-limit") == 0)
			ok = ok && xrtErrorKind(xrtGetError()) == XERR_AGAIN && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_NETWORK;
		else if(strcmp(mode, "aws-error-permission") == 0)
			ok = ok && xrtErrorKind(xrtGetError()) == XERR_PERMISSION && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_CREDENTIAL;
		else ok = ok && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL;
		return ok && p->Remove(p, owner, value);
	}
	if(aws && strcmp(mode, "capacity") == 0) {
		for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
			char text[40]; int size = snprintf(text, sizeof(text), "digest_%u", i);
			if(size <= 0 || !p->Add(p, owner, (xstrview){ text, (size_t)size })) return false;
		}
		if(!p->Add(p, owner, XRT_STR_LITERAL("digest_0")) ||
			p->Add(p, owner, XRT_STR_LITERAL("over_capacity")) ||
			xrtErrorKind(xrtGetError()) != XERR_RANGE) return false;
		xrtClearError();
		if(!p->Remove(p, owner, XRT_STR_LITERAL("digest_0")) ||
			!p->Add(p, owner, XRT_STR_LITERAL("reused_slot")) ||
			!p->Remove(p, owner, XRT_STR_LITERAL("reused_slot"))) return false;
		for(unsigned i = 1u; i < XACME_DNS_RECORD_MAX; i++) {
			char text[40]; int size = snprintf(text, sizeof(text), "digest_%u", i);
			if(!p->Remove(p, owner, (xstrview){ text, (size_t)size })) return false;
		}
		return true;
	}
	if(strcmp(mode, "add-failure") == 0 || strcmp(mode, "add-uncertain") == 0) {
		ok = !p->Add(p, owner, value) && !tracked(p, aws);
		if(strcmp(mode, "add-uncertain") == 0)
			ok = ok && xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_UNCERTAIN;
		return ok && p->Remove(p, owner, value);
	}
	if(!p->Add(p, owner, value) || !tracked(p, aws)) return false;
	if(!aws && strcmp(mode, "delete-parse-oom") == 0) {
		CreateParseOom = true;
		ok = !p->Remove(p, owner, value) && tracked(p, false) &&
			CreateParseOomTriggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		CreateParseOom = false;
		xrtClearError();
		return ok && p->Remove(p, owner, value) && !tracked(p, false);
	}
	if(aws && strcmp(mode, "repeat-owned") == 0) {
		xacmednsawscontext* context = p->pContext;
		return p->Add(p, owner, value) && context->iRecordCount == 1u &&
			p->Remove(p, owner, value) && p->Remove(p, owner, value) && !tracked(p, true);
	}
	if(aws && strcmp(mode, "cross-instance") == 0) {
		xacmednsprovider borrower = { 0 };
		xacmednsawsconfig config;
		xrtAcmeDnsAwsConfigInit(&config);
		config.sAccessKeyId = "test-id"; config.sSecretAccessKey = "test-key";
		config.sEndpoint = ((xacmednsawscontext*)p->pContext)->sEndpoint;
		if(!xrtAcmeDnsAws(&config, NULL, &borrower)) return false;
		ok = borrower.Add(&borrower, owner, value) && !tracked(&borrower, true) &&
			borrower.Remove(&borrower, owner, value);
		xrtAcmeDnsAwsProviderUnit(&borrower);
		return ok && borrower.pContext == NULL && p->Remove(p, owner, value) && !tracked(p, true);
	}
	if(strcmp(mode, "delete-failure") == 0 || strcmp(mode, "delete-uncertain") == 0) {
		ok = !p->Remove(p, owner, value) && tracked(p, aws);
		if(strcmp(mode, "delete-uncertain") == 0)
			ok = ok && xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_UNCERTAIN;
		xrtClearError();
		return ok && p->Remove(p, owner, value) && !tracked(p, aws) && p->Remove(p, owner, value);
	}
	if(strcmp(mode, "multi") == 0) {
		xstrview second = XRT_STR_LITERAL("second_digest-02");
		xstrview other = XRT_STR_LITERAL("_acme-challenge.other.example.org");
		return p->Add(p, owner, second) && p->Add(p, other, value) &&
			p->Remove(p, owner, XRT_STR_LITERAL("unowned_digest")) &&
			p->Remove(p, owner, value) && p->Remove(p, owner, value) &&
			p->Remove(p, other, value) && p->Remove(p, owner, second) && !tracked(p, aws);
	}
	return p->Remove(p, owner, value) && !tracked(p, aws) && p->Remove(p, owner, value);
}

static xacmednsrecords* rest_records(xacmednsprovider* p, cstr name)
{
	if(strcmp(name, "cf") == 0) return &((xacmednscfcontext*)p->pContext)->Records;
	if(strcmp(name, "tencent") == 0) return &((xacmednstencentcontext*)p->pContext)->Records;
	return &((xacmednshuaaweicontext*)p->pContext)->Records;
}

static bool rest_blocked(xacmednsprovider* p, xstrview owner, xstrview value)
{
	xstrview upper = XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM");
	xrtClearError();
	if(p->Add(p, upper, value) || xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) {
		fprintf(stderr, "repeat Add lost uncertainty guard\n"); return false;
	}
	xrtClearError();
	if(p->Remove(p, owner, value) || xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) {
		fprintf(stderr, "Remove lost uncertainty guard\n"); return false;
	}
	xrtClearError();
	return !p->Remove(p, upper, value) && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
}

/* Seed the legacy state of two confirmed IDs for one pair. Both real creates
 * still validate acknowledgements; only the new public repeat guard is bypassed. */
static bool seed_legacy_duplicate(xacmednsprovider* p, cstr name, xstrview owner, xstrview value)
{
	xmutex* lock;
	xacmednsaddproc create;
	bool ok;
	if(strcmp(name, "cf") == 0) {
		lock = &((xacmednscfcontext*)p->pContext)->Lock; create = xacmeCfAddLocked;
	} else if(strcmp(name, "tencent") == 0) {
		lock = &((xacmednstencentcontext*)p->pContext)->Lock; create = xacmeTencentAddLocked;
	} else if(strcmp(name, "huawei") == 0) {
		lock = &((xacmednshuaaweicontext*)p->pContext)->Lock; create = xacmeHuaweiAddLocked;
	} else return false;
	if(!xrtMutexLock(lock)) return false;
	ok = create(p, owner, value);
	(void)xrtMutexUnlock(lock);
	return ok;
}

static bool exercise_rest(xacmednsprovider* p, cstr name, cstr mode)
{
	xstrview owner = XRT_STR_LITERAL("_acme-challenge.api.example.com");
	xstrview value = XRT_STR_LITERAL("first_digest-01");
	xstrview second = XRT_STR_LITERAL("second_digest-02");
	xacmednsrecords* records = rest_records(p, name);
	bool ok;
	if(strncmp(mode, "rest-tc-zone-", 13u) == 0) {
		cstr fault = mode + 13u;
		xacmednstencentcontext* context = p->pContext;
		char long_owner[256], label[64], handle[XACME_DNS_RECORD_TEXT_CAP];
		bool bad_discovery = strcmp(fault, "denied") == 0 || strcmp(fault, "503") == 0 ||
			strcmp(fault, "malformed") == 0 || strcmp(fault, "nul") == 0 ||
			strcmp(fault, "wrong-domain") == 0 || strcmp(fault, "missing-request") == 0;
		bool apex = strncmp(fault, "apex-read-", 10u) == 0 ||
			strcmp(fault, "owner-apex") == 0 || strcmp(fault, "owner-case") == 0 || strcmp(fault, "owner-long") == 0;
		bool pairs = strcmp(fault, "parent-child") == 0 || strcmp(fault, "appears") == 0 ||
			strcmp(fault, "disappears") == 0 || strcmp(fault, "replacement") == 0;
		if(strcmp(name, "tencent") != 0) return false;
		if(strcmp(fault, "owner-long") == 0) {
			memset(label, 'a', 63u); label[63] = '\0';
			if(snprintf(long_owner, sizeof(long_owner), "_acme-challenge.%s.%s.%s.%.*s.example.com",
				label, label, label, 33, label) != 253) return false;
			owner = (xstrview){ long_owner, 253u };
		}
		if(strncmp(fault, "no-zone", 7u) == 0) {
			if(strcmp(fault, "no-zone-stale-error") == 0)
				xrtSetErrorInfo(XERR_MEMORY, "unrelated", 918, "stale error");
			return !p->Add(p, owner, value) && records->iCount == 0u &&
				xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND &&
				strcmp(xrtErrorDomain(xrtGetError()), "xrt.acme.dns") == 0 &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_PROTOCOL;
		}
		if(bad_discovery) {
			xerrkind expected = strcmp(fault, "denied") == 0 ? XERR_PERMISSION : strcmp(fault, "503") == 0 ? XERR_AGAIN : XERR_PROTOCOL;
			if(p->Add(p, owner, value) || records->iCount != 0u || xrtErrorKind(xrtGetError()) != expected) return false;
			xrtClearError();
			return p->Add(p, owner, value) && p->Remove(p, owner, value) && records->sIds[0][0] == '\0';
		}
		if(pairs) {
			xstrview first_owner = strcmp(fault, "parent-child") == 0 ? XRT_STR_LITERAL("_acme-challenge.first.example.com") : owner;
			return p->Add(p, first_owner, value) && p->Add(p, owner, second) && records->iCount == 2u &&
				p->Remove(p, first_owner, value) && p->Remove(p, owner, second) &&
				records->sIds[0][0] == '\0' && records->sIds[1][0] == '\0';
		}
		if(!p->Add(p, strcmp(fault, "owner-case") == 0 ? XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM") : owner, value) ||
			records->iCount != 1u || context->iDomainIds[0] != (apex ? 303 : 101) ||
			strcmp(records->sFqdns[0], owner.Data) != 0) return false;
		strcpy(handle, records->sIds[0]);
		if(strncmp(fault, "apex-read-", 10u) == 0) {
			RecordParseOom = strcmp(fault, "apex-read-oom") == 0;
			ok = !p->Remove(p, owner, value) && strcmp(records->sIds[0], handle) == 0 &&
				context->iDomainIds[0] == 303 && !context->bDeletePending[0] && TencentDeleteCalls == 0u &&
				xrtErrorKind(xrtGetError()) == (RecordParseOom ? XERR_MEMORY : XERR_PROTOCOL);
			if(RecordParseOom) ok = ok && RecordParseOomTriggered;
			RecordParseOom = false;
			if(!ok) return false;
			xrtClearError();
		} else if(!p->Add(p, owner, value) || strcmp(records->sIds[0], handle) != 0 || records->iCount != 1u) return false;
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && p->Remove(p, owner, value) && TencentDeleteCalls == 1u;
	}
	if(strncmp(mode, "rest-owned-", 11u) == 0) {
		cstr fault = mode + 11u;
		char handle[XACME_DNS_RECORD_TEXT_CAP];
		bool tencent = strcmp(name, "tencent") == 0, huawei = strcmp(name, "huawei") == 0;
		bool missing = strcmp(fault, "missing") == 0;
		bool memory = strcmp(fault, "read-oom") == 0 || strcmp(fault, "domain-oom") == 0;
		bool failing = (strncmp(fault, "read-", 5u) == 0 && strcmp(fault, "read-lost-once") != 0) ||
			strncmp(fault, "domain-", 7u) == 0 || strcmp(fault, "handle-invalid") == 0 || (missing && tencent);
		xstrview upper = XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM");
		if(tencent) TencentTestClock = UINT64_C(1000000000);
		if(strcmp(fault, "concurrent-start") == 0) {
			parallel_add tasks[8] = { 0 };
			xthread* threads[8]; size_t count = 0u;
			xatomic32 start; xrtAtomic32Init(&start, 0u);
			for(size_t i = 0u; i < 8u; i++) {
				tasks[i].provider = p; tasks[i].start = &start; tasks[i].owner = (i % 2u == 0u) ? owner : upper;
				strcpy(tasks[i].value, value.Data);
				threads[i] = xrtThreadCreate(add_in_thread, &tasks[i], 0u);
				if(threads[i] == NULL) break;
				count++;
			}
			xrtAtomic32Store(&start, 1u, XMEMORY_RELEASE);
			ok = count == 8u;
			for(size_t i = 0u; i < count; i++) {
				if(xrtThreadWaitFor(threads[i], 60000000u) != XWAIT_OK) _Exit(2);
				if(!tasks[i].ok) ok = false;
				xrtThreadDestroy(threads[i]);
			}
			return ok && records->iCount == 1u && records->sIds[0][0] != '\0' && p->Remove(p, owner, value);
		}
		if(!p->Add(p, strcmp(fault, "owner-case") == 0 ? upper : owner, value) ||
			records->iCount != 1u || records->sIds[0][0] == '\0') return false;
		strcpy(handle, records->sIds[0]);
		if(strcmp(fault, "capacity") == 0) {
			for(size_t i = 1u; i < 8u; i++) {
				char digest[40]; snprintf(digest, sizeof(digest), "capacity_%zu", i);
				if(!p->Add(p, owner, (xstrview){ digest, strlen(digest) })) return false;
			}
			xrtClearError();
			if(!p->Add(p, upper, value) || records->iCount != 8u || strcmp(records->sIds[0], handle) != 0) return false;
			if(!p->Remove(p, owner, value)) return false;
			for(size_t i = 1u; i < 8u; i++) {
				char digest[40]; snprintf(digest, sizeof(digest), "capacity_%zu", i);
				if(!p->Remove(p, owner, (xstrview){ digest, strlen(digest) })) return false;
			}
			return true;
		}
		RecordParseOom = strcmp(fault, "read-oom") == 0;
		DomainParseOom = strcmp(fault, "domain-oom") == 0;
		if(strcmp(fault, "handle-invalid") == 0) strcpy(records->sIds[0], "invalid-handle");
		if(strcmp(fault, "domain-zero") == 0) ((xacmednstencentcontext*)p->pContext)->iDomainIds[0] = 0;
		xrtClearError(); ok = p->Add(p, upper, value);
		if(failing) {
			if(ok || records->iCount != 1u || strcmp(records->sIds[0], strcmp(fault, "handle-invalid") == 0 ? "invalid-handle" : handle) != 0 ||
				strcmp(records->sFqdns[0], owner.Data) != 0 || strcmp(records->sTxts[0], value.Data) != 0 ||
				xrtErrorKind(xrtGetError()) == XERR_NONE) return false;
			if(memory && (xrtErrorKind(xrtGetError()) != XERR_MEMORY || !(RecordParseOomTriggered || DomainParseOomTriggered))) return false;
			if((strcmp(fault, "read-denied") == 0 || strcmp(fault, "domain-denied") == 0) && xrtErrorKind(xrtGetError()) != XERR_PERMISSION) return false;
			if((strncmp(fault, "read-busy-", 10u) == 0 || strcmp(fault, "read-deleting") == 0 || (missing && tencent)) && xrtErrorKind(xrtGetError()) != XERR_AGAIN) return false;
			if((strncmp(fault, "read-inactive-", 14u) == 0 || strcmp(fault, "read-disabled") == 0) && xrtErrorKind(xrtGetError()) != XERR_STATE) return false;
			if(tencent && ((xacmednstencentcontext*)p->pContext)->bDeletePending[0]) return false;
			RecordParseOom = false; DomainParseOom = false;
			strcpy(records->sIds[0], handle);
			if(strcmp(fault, "domain-zero") == 0) ((xacmednstencentcontext*)p->pContext)->iDomainIds[0] = 101;
			if(strcmp(fault, "read-deleting") == 0) {
				xacmednshuaaweicontext* context = p->pContext;
				if(!huawei || !context->bDeletePending[0] || !context->bDeleteAccepted[0]) return false;
				xrtClearError();
				if(p->Add(p, owner, value) || xrtErrorKind(xrtGetError()) != XERR_AGAIN || strcmp(records->sIds[0], handle) != 0) return false;
				return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && !context->bDeletePending[0] && !context->bDeleteAccepted[0];
			}
			xrtClearError();
			if(!p->Add(p, owner, value) || records->iCount != 1u || strcmp(records->sIds[0], handle) != 0) return false;
		} else if(!ok || records->iCount != 1u || (!missing && strcmp(records->sIds[0], handle) != 0) ||
			(missing && strcmp(records->sIds[0], handle) == 0)) return false;
		if(strcmp(fault, "repeat-many") == 0) for(size_t i = 1u; i < 10u; i++)
			if(!p->Add(p, owner, value) || records->iCount != 1u || strcmp(records->sIds[0], handle) != 0) return false;
		if(strcmp(fault, "zone-changed") == 0) {
			if(!p->Add(p, owner, second) || records->iCount != 2u || strcmp(records->sIds[0], handle) != 0) return false;
			return p->Remove(p, owner, value) && p->Remove(p, owner, second) && records->sIds[0][0] == '\0' && records->sIds[1][0] == '\0';
		}
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && p->Remove(p, owner, value);
	}
	if(strncmp(mode, "rest-zone-input-", 16u) == 0) {
		cstr fault = mode + 16u;
		bool sweep = strncmp(fault, "oom-", 4u) == 0;
		xerrkind wanted = (strcmp(fault, "401") == 0 || strcmp(fault, "403") == 0 || strcmp(fault, "stale-401") == 0) ? XERR_PERMISSION :
			(strcmp(fault, "429") == 0 || strcmp(fault, "503") == 0 || strcmp(fault, "stale-503") == 0) ? XERR_AGAIN : XERR_PROTOCOL;
		if(strcmp(name, "cf") != 0 && strcmp(name, "huawei") != 0) return false;
		if(sweep) {
			size_t failed = 0u;
			bool complete = false;
			for(size_t point = 0u; point < 256u; point++) {
				ZoneParseAfter = point; ZoneParseOomTriggered = false;
				ZoneParseOom = ZoneParseSweep = true;
				xrtClearError();
				bool added = p->Add(p, owner, value);
				ZoneParseOom = ZoneParseSweep = false;
				if(ZoneParseOomTriggered) {
					bool preserved = !added && records->iCount == 0u &&
						xrtErrorKind(xrtGetError()) == XERR_MEMORY && xrtGetError() == ZoneParseError;
					xrtErrorFree(ZoneParseError); ZoneParseError = NULL;
					if(!preserved) { fprintf(stderr, "zone parse OOM point=%zu kind=%d\n", point, (int)xrtErrorKind(xrtGetError())); return false; }
					failed++;
				} else {
					if(!added || records->iCount != 1u || failed == 0u) return false;
					complete = true; break;
				}
			}
			if(!complete) return false;
			printf("zone parse %s OOM allocation points=%zu\n", fault, failed);
			return p->Remove(p, owner, value) && records->sIds[0][0] == '\0';
		}
		if(strncmp(fault, "stale-", 6u) == 0)
			xrtSetErrorInfo(XERR_MEMORY, "sentinel", 777, "prior unrelated memory error");
		else xrtClearError();
		if(p->Add(p, owner, value) || records->iCount != 0u || xrtErrorKind(xrtGetError()) != wanted) return false;
		/* The service supplies one failure, then normal discovery. Read failure may be retried by the caller. */
		xrtClearError();
		return p->Add(p, owner, value) && records->iCount == 1u && p->Remove(p, owner, value) && records->sIds[0][0] == '\0';
	}
	if(strncmp(mode, "rest-zone-", 10u) == 0) {
		cstr fault = mode + 10u;
		char first_handle[XACME_DNS_RECORD_TEXT_CAP];
		char separator = strcmp(name, "cf") == 0 ? '/' : '|';
		bool failing = strcmp(fault, "denied") == 0 || strcmp(fault, "malformed") == 0 || strcmp(fault, "ambiguous") == 0;
		bool siblings = failing || strcmp(fault, "parent-child") == 0;
		xstrview first = siblings ? XRT_STR_LITERAL("_acme-challenge.first.example.com") : owner;
		cstr first_zone = strcmp(fault, "owner-apex") == 0 ? "apex" :
			strcmp(fault, "replacement") == 0 ? "child-old" : strcmp(fault, "disappears") == 0 ? "child" : "parent";
		char expected[XACME_DNS_RECORD_TEXT_CAP];
		if(strcmp(name, "cf") != 0 && strcmp(name, "huawei") != 0) return false;
		if(strncmp(fault, "no-zone", 7u) == 0) {
			if(strcmp(fault, "no-zone-stale-error") == 0)
				xrtSetErrorInfo(XERR_PERMISSION, "sentinel", 777, "prior operation failed");
			return !p->Add(p, owner, value) && records->iCount == 0u &&
				xrtErrorKind(xrtGetError()) == XERR_PROTOCOL &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_ZONE && p->Remove(p, owner, value);
		}
		snprintf(expected, sizeof(expected), "%s%c11", first_zone, separator);
		if(!p->Add(p, first, value) || records->iCount != 1u || strcmp(records->sIds[0], expected) != 0) return false;
		strcpy(first_handle, records->sIds[0]);
		if(strcmp(fault, "owner-apex") == 0)
			return p->Remove(p, owner, value) && records->sIds[0][0] == '\0';
		if(failing) {
			xrtClearError();
			if(p->Add(p, owner, second) || records->iCount != 1u ||
				strcmp(records->sIds[0], first_handle) != 0 || xrtErrorKind(xrtGetError()) != (strcmp(fault, "denied") == 0 ? XERR_PERMISSION : XERR_PROTOCOL)) return false;
			xrtClearError();
			return p->Remove(p, owner, second) && p->Remove(p, first, value) && records->sIds[0][0] == '\0';
		}
		snprintf(expected, sizeof(expected), "%s%c12", strcmp(fault, "disappears") == 0 ? "parent" :
			strcmp(fault, "replacement") == 0 ? "child-new" : "child", separator);
		if(!p->Add(p, owner, second) || records->iCount != 2u ||
			strcmp(records->sIds[0], first_handle) != 0 || strcmp(records->sIds[1], expected) != 0) return false;
		return p->Remove(p, first, value) && records->sIds[0][0] == '\0' &&
			p->Remove(p, owner, second) && records->sIds[1][0] == '\0';
	}
	if(strncmp(mode, "rest-tc-late-index-", 19u) == 0) {
		cstr fault = mode + 19u;
		xacmednstencentcontext* context = p->pContext;
		char handle[XACME_DNS_RECORD_TEXT_CAP];
		unsigned attempt, attempts = strcmp(fault, "repeat") == 0 ? 2u : 1u;
		if(strcmp(name, "tencent") != 0) return false;
		TencentTestClock = UINT64_C(1000000000);
		if(!p->Add(p, owner, value) || records->iCount != 1u) return false;
		strcpy(handle, records->sIds[0]);
		TencentTestClock += UINT64_C(86400000000);
		for(attempt = 0u; attempt < attempts; attempt++) {
			xrtClearError();
			if(p->Remove(p, owner, value) || strcmp(records->sIds[0], handle) != 0 ||
				!context->bDeletePending[0] || xrtErrorKind(xrtGetError()) != XERR_AGAIN) return false;
			xrtClearError();
			if(p->Add(p, owner, value) || strcmp(records->sIds[0], handle) != 0 ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			TencentTestClock += UINT64_C(86400000000);
		}
		xrtClearError();
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' &&
			TencentDeleteCalls == (strcmp(fault, "delete") == 0 ? 2u : 1u) && p->Remove(p, owner, value);
	}
	if(strncmp(mode, "rest-tc-domain-", 15u) == 0) {
		cstr fault = mode + 15u;
		if(strcmp(name, "tencent") != 0) return false;
		DomainParseOom = strcmp(fault, "oom") == 0;
		xrtClearError(); ok = !p->Add(p, owner, value) && records->iCount == 0u && xrtErrorKind(xrtGetError()) != XERR_NONE;
		if(DomainParseOom) ok = ok && DomainParseOomTriggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		if(strcmp(fault, "denied") == 0) ok = ok && xrtErrorKind(xrtGetError()) == XERR_PERMISSION;
		DomainParseOom = false;
		if(!ok) return false;
		xrtClearError(); return p->Add(p, owner, value) && p->Remove(p, owner, value) && TencentDeleteCalls == 1u;
	}
	if(strncmp(mode, "rest-tc-record-", 15u) == 0) {
		cstr fault = mode + 15u;
		xacmednstencentcontext* context = p->pContext;
		char handle[XACME_DNS_RECORD_TEXT_CAP];
		bool early = strcmp(fault, "early-index") == 0 || strcmp(fault, "early-missing") == 0 || strcmp(fault, "clock-back") == 0;
		bool absent = strcmp(fault, "missing") == 0 || strcmp(fault, "empty-inventory") == 0 || strcmp(fault, "full-inventory") == 0 ||
			strcmp(fault, "early-missing") == 0 || strcmp(fault, "clock-back") == 0 || strncmp(fault, "list-", 5u) == 0 ||
			(strncmp(fault, "domain-", 7u) == 0 && strcmp(fault, "domain-zero") != 0) ||
			strncmp(fault, "committed-", 10u) == 0 || strncmp(fault, "reconcile-", 10u) == 0;
		bool memory = strstr(fault, "oom") != NULL || strcmp(fault, "delete-presend") == 0;
		bool readonly = strncmp(fault, "read-", 5u) == 0 || strncmp(fault, "list-", 5u) == 0 || strncmp(fault, "domain-", 7u) == 0 || early;
		bool failed = absent || early || strncmp(fault, "uncommitted-", 12u) == 0 || strncmp(fault, "reconcile-", 10u) == 0 ||
			strncmp(fault, "list-", 5u) == 0 || strncmp(fault, "domain-", 7u) == 0 || strcmp(fault, "committed-oom") == 0 ||
			strcmp(fault, "delete-presend") == 0 ||
			(strncmp(fault, "read-", 5u) == 0 && strcmp(fault, "read-lost-once") != 0 && strcmp(fault, "read-disabled") != 0);
		unsigned wanted = strcmp(fault, "missing") == 0 || strcmp(fault, "early-missing") == 0 || strcmp(fault, "clock-back") == 0 ||
			strcmp(fault, "empty-inventory") == 0 || strcmp(fault, "full-inventory") == 0 ||
			strncmp(fault, "list-", 5u) == 0 || strncmp(fault, "domain-", 7u) == 0 ? 0u :
			strncmp(fault, "uncommitted-", 12u) == 0 ? 2u : 1u;
		if(strcmp(name, "tencent") != 0) return false;
		TencentTestClock = 1000000000u;
		if(!p->Add(p, owner, value) || records->iCount != 1u || context->iDomainIds[0] != 101) return false;
		strcpy(handle, records->sIds[0]);
		TencentTestClock += early ? (strcmp(fault, "clock-back") == 0 ? 0u : 29999999u) : 30000000u;
		if(strcmp(fault, "clock-back") == 0) TencentTestClock--;
		if(strcmp(fault, "partial") == 0 || strcmp(fault, "more-specific") == 0) {
			if(!(strcmp(fault, "partial") == 0 ? seed_legacy_duplicate(p, name, owner, value) : p->Add(p, owner, second)) ||
				records->iCount != 2u) return false;
			if(strcmp(fault, "more-specific") == 0) {
				if(context->iDomainIds[1] != 202) return false;
				return p->Remove(p, owner, value) && p->Remove(p, owner, second) &&
					records->sIds[0][0] == '\0' && records->sIds[1][0] == '\0' && TencentDeleteCalls == 2u;
			}
			strcpy(handle, records->sIds[1]); xrtClearError();
			if(p->Remove(p, owner, value) || records->sIds[0][0] != '\0' || strcmp(records->sIds[1], handle) != 0 ||
				xrtErrorKind(xrtGetError()) != XERR_PERMISSION) return false;
			xrtClearError(); return p->Remove(p, owner, value) && records->sIds[1][0] == '\0' && TencentDeleteCalls == 2u;
		}
		if(strncmp(fault, "handle-", 7u) == 0 || strcmp(fault, "domain-zero") == 0) {
			if(strcmp(fault, "domain-zero") == 0) context->iDomainIds[0] = 0;
			else snprintf(records->sIds[0], sizeof(records->sIds[0]), "example.com|%s", strcmp(fault, "handle-zero") == 0 ? "0" :
				strcmp(fault, "handle-overflow") == 0 ? "9223372036854775808" : "11,\"RecordId\":12");
			xrtClearError(); ok = !p->Remove(p, owner, value) && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL && TencentDeleteCalls == 0u;
			strcpy(records->sIds[0], handle); context->iDomainIds[0] = 101;
			xrtClearError(); return ok && p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && TencentDeleteCalls == 1u;
		}
		RecordParseOom = strcmp(fault, "read-oom") == 0 || strcmp(fault, "reconcile-oom") == 0;
		RecordParseAfter = strcmp(fault, "reconcile-oom") == 0 ? 1u : 0u;
		DomainParseOom = strcmp(fault, "domain-oom") == 0;
		ListParseOom = strcmp(fault, "list-oom") == 0;
		DeleteParseOom = strcmp(fault, "committed-oom") == 0 || strcmp(fault, "uncommitted-oom") == 0;
		UncertaintyErrorOom = strcmp(fault, "uncommitted-error-oom") == 0;
		TencentDeletePresendFailure = strcmp(fault, "delete-presend") == 0;
		xrtClearError(); ok = p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
		if(failed) {
			ok = !ok && strcmp(records->sIds[0], handle) == 0 && strcmp(records->sFqdns[0], owner.Data) == 0 &&
				strcmp(records->sTxts[0], value.Data) == 0 && context->iDomainIds[0] == 101;
			if(memory) ok = ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
				(TencentDeletePresendFailure || RecordParseOomTriggered || DeleteParseOomTriggered || DomainParseOomTriggered || ListParseOomTriggered || UncertaintyOomTriggered != 0u);
			else if(strstr(fault, "denied") != NULL && strstr(fault, "prefix") == NULL) ok = ok && xrtErrorKind(xrtGetError()) == XERR_PERMISSION;
			else if(early || strcmp(fault, "list-owned") == 0) ok = ok && xrtErrorKind(xrtGetError()) == XERR_AGAIN;
			else if(strncmp(fault, "list-", 5u) == 0 && strcmp(fault, "list-500") != 0) ok = ok && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL;
			else ok = ok && xrtErrorKind(xrtGetError()) != XERR_NONE;
		} else ok = ok && records->sIds[0][0] == '\0';
		if(TencentDeletePresendFailure) ok = ok && !context->bDeletePending[0];
		if(early) ok = ok && TencentInventoryCalls == 0u && context->bDeletePending[0];
		RecordParseOom = false; DeleteParseOom = false; DomainParseOom = false; ListParseOom = false; UncertaintyErrorOom = false; TencentDeletePresendFailure = false;
		if(!ok) { fprintf(stderr, "Tencent delete state contract failed: %s\n", fault); return false; }
		if(failed && (context->bDeletePending[0] || !readonly) && strcmp(fault, "uncommitted-denied") != 0 &&
			strcmp(fault, "uncommitted-operation-denied") != 0 && strcmp(fault, "delete-presend") != 0) {
			xrtClearError();
			if(p->Add(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value) || strcmp(records->sIds[0], handle) != 0 || xrtErrorKind(xrtGetError()) == XERR_NONE) return false;
		}
		TencentTestClock = 1030000000u; xrtClearError();
		if(absent) {
			if(p->Remove(p, owner, value) || strcmp(records->sIds[0], handle) != 0 || !context->bDeletePending[0] ||
				xrtErrorKind(xrtGetError()) != XERR_AGAIN || TencentDeleteCalls != wanted) return false;
			xrtClearError();
			return !p->Add(p, owner, value) && strcmp(records->sIds[0], handle) == 0 &&
				xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		}
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && TencentDeleteCalls == wanted &&
			p->Remove(p, owner, value) && p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
	}
	if(strncmp(mode, "rest-hw-record-", 15u) == 0) {
		cstr fault = mode + 15u;
		char handle[XACME_DNS_RECORD_TEXT_CAP];
		bool memory = strcmp(fault, "read-oom") == 0 || strcmp(fault, "committed-oom") == 0 ||
			strcmp(fault, "uncommitted-oom") == 0 || strcmp(fault, "uncommitted-error-oom") == 0 ||
			strcmp(fault, "reconcile-oom") == 0 || strcmp(fault, "accepted-read-oom") == 0 || strcmp(fault, "delete-presend") == 0;
		bool stable = strcmp(fault, "read-disabled") == 0 || strcmp(fault, "read-frozen") == 0 ||
			strcmp(fault, "read-illegal") == 0 || strcmp(fault, "read-police") == 0 || strcmp(fault, "read-error") == 0;
		bool failed = (strncmp(fault, "read-", 5u) == 0 && strcmp(fault, "read-lost-once") != 0 && !stable) ||
			strncmp(fault, "uncommitted-", 12u) == 0 || strncmp(fault, "reconcile-", 10u) == 0 ||
			strncmp(fault, "accepted-read-", 14u) == 0 || strcmp(fault, "committed-oom") == 0 ||
			strcmp(fault, "pending-exhausted") == 0 || strcmp(fault, "pending-active") == 0 || strcmp(fault, "delete-presend") == 0;
		unsigned wanted = strcmp(fault, "missing") == 0 || strcmp(fault, "initial-pending") == 0 ? 0u :
			strncmp(fault, "uncommitted-", 12u) == 0 ? 2u : 1u;
		if(strcmp(name, "huawei") != 0 || !p->Add(p, owner, value) || records->iCount != 1u ||
			records->sIds[0][0] == '\0') return false;
		strcpy(handle, records->sIds[0]);
		if(strcmp(fault, "partial") == 0) {
			if(!seed_legacy_duplicate(p, name, owner, value) || records->iCount != 2u || records->sIds[1][0] == '\0') return false;
			strcpy(handle, records->sIds[1]); xrtClearError();
			if(p->Remove(p, owner, value) || records->sIds[0][0] != '\0' ||
				strcmp(records->sIds[1], handle) != 0 || xrtErrorKind(xrtGetError()) != XERR_PERMISSION) return false;
			xrtClearError();
			return p->Remove(p, owner, value) && records->sIds[1][0] == '\0' &&
				p->Remove(p, owner, value) && HuaweiDeleteCalls == 2u;
		}
		RecordParseOom = strcmp(fault, "read-oom") == 0 || strcmp(fault, "reconcile-oom") == 0 || strcmp(fault, "accepted-read-oom") == 0;
		RecordParseAfter = strcmp(fault, "reconcile-oom") == 0 || strcmp(fault, "accepted-read-oom") == 0 ? 1u : 0u;
		DeleteParseOom = strcmp(fault, "committed-oom") == 0 || strcmp(fault, "uncommitted-oom") == 0;
		UncertaintyErrorOom = strcmp(fault, "uncommitted-error-oom") == 0;
		HuaweiDeletePresendFailure = strcmp(fault, "delete-presend") == 0;
		xrtClearError(); ok = p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
		if(failed) {
			ok = !ok && strcmp(records->sIds[0], handle) == 0 &&
				strcmp(records->sFqdns[0], owner.Data) == 0 && strcmp(records->sTxts[0], value.Data) == 0;
			if(memory) ok = ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
				(HuaweiDeletePresendFailure || (RecordParseOom ? RecordParseOomTriggered : DeleteParseOom ? DeleteParseOomTriggered : UncertaintyOomTriggered != 0u));
			else if(strcmp(fault, "read-denied") == 0 || strcmp(fault, "uncommitted-denied") == 0 ||
				strcmp(fault, "reconcile-denied") == 0 || strcmp(fault, "accepted-read-denied") == 0)
				ok = ok && xrtErrorKind(xrtGetError()) == XERR_PERMISSION;
			else if(strncmp(fault, "read-busy-", 10u) == 0 || strcmp(fault, "pending-exhausted") == 0 || strcmp(fault, "pending-active") == 0)
				ok = ok && xrtErrorKind(xrtGetError()) == XERR_AGAIN;
			else ok = ok && xrtErrorKind(xrtGetError()) != XERR_NONE;
		} else ok = ok && records->sIds[0][0] == '\0';
		if(HuaweiDeletePresendFailure) {
			xacmednshuaaweicontext* context = (xacmednshuaaweicontext*)p->pContext;
			ok = ok && !context->bDeletePending[0] && !context->bDeleteAccepted[0];
		}
		RecordParseOom = false; DeleteParseOom = false; UncertaintyErrorOom = false;
		HuaweiDeletePresendFailure = false;
		if(!ok) { fprintf(stderr, "Huawei delete state contract failed: %s\n", fault); return false; }
		if(failed && strncmp(fault, "read-", 5u) != 0 && strcmp(fault, "uncommitted-denied") != 0 && strcmp(fault, "delete-presend") != 0) {
			xrtClearError();
			if(p->Add(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value) ||
				strcmp(records->sIds[0], handle) != 0 || xrtErrorKind(xrtGetError()) == XERR_NONE) return false;
		}
		xrtClearError();
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' && HuaweiDeleteCalls == wanted &&
			p->Remove(p, owner, value) && p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
	}
	if(strncmp(mode, "rest-cf-record-", 15u) == 0) {
		cstr fault = mode + 15u;
		char handle[XACME_DNS_RECORD_TEXT_CAP];
		bool memory = strcmp(fault, "read-oom") == 0 || strcmp(fault, "reconcile-oom") == 0 || strcmp(fault, "committed-oom") == 0 ||
			strcmp(fault, "uncommitted-oom") == 0 || strcmp(fault, "uncommitted-error-oom") == 0;
		bool failed = (strncmp(fault, "read-", 5u) == 0 && strcmp(fault, "read-lost-once") != 0) || strncmp(fault, "uncommitted-", 12u) == 0 ||
			strncmp(fault, "reconcile-", 10u) == 0 || strcmp(fault, "committed-oom") == 0;
		if(strcmp(name, "cf") != 0 || !p->Add(p, owner, value) || records->iCount != 1u ||
			records->sIds[0][0] == '\0') return false;
		strcpy(handle, records->sIds[0]);
		if(strcmp(fault, "partial") == 0) {
			if(!seed_legacy_duplicate(p, name, owner, value) || records->iCount != 2u || records->sIds[1][0] == '\0') return false;
			strcpy(handle, records->sIds[1]); xrtClearError();
			if(p->Remove(p, owner, value) || records->sIds[0][0] != '\0' ||
				strcmp(records->sIds[1], handle) != 0 || xrtErrorKind(xrtGetError()) != XERR_PERMISSION) return false;
			xrtClearError();
			return p->Remove(p, owner, value) && records->sIds[1][0] == '\0' && p->Remove(p, owner, value);
		}
		RecordParseOom = strcmp(fault, "read-oom") == 0 || strcmp(fault, "reconcile-oom") == 0;
		RecordParseAfter = strcmp(fault, "reconcile-oom") == 0 ? 1u : 0u;
		DeleteParseOom = strcmp(fault, "committed-oom") == 0 || strcmp(fault, "uncommitted-oom") == 0;
		UncertaintyErrorOom = strcmp(fault, "uncommitted-error-oom") == 0;
		xrtClearError();
		ok = p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
		if(failed) {
			ok = !ok && strcmp(records->sIds[0], handle) == 0 &&
				strcmp(records->sFqdns[0], owner.Data) == 0 && strcmp(records->sTxts[0], value.Data) == 0;
			if(memory) ok = ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY &&
				(RecordParseOom ? RecordParseOomTriggered : DeleteParseOom ? DeleteParseOomTriggered : UncertaintyOomTriggered != 0u);
			else if(strcmp(fault, "read-denied") == 0 || strcmp(fault, "uncommitted-denied") == 0 ||
				strcmp(fault, "reconcile-denied") == 0) ok = ok && xrtErrorKind(xrtGetError()) == XERR_PERMISSION;
			else ok = ok && xrtErrorKind(xrtGetError()) != XERR_NONE;
		} else ok = ok && records->sIds[0][0] == '\0';
		RecordParseOom = false; DeleteParseOom = false; UncertaintyErrorOom = false;
		if(!ok) { fprintf(stderr, "CF delete state contract failed: %s\n", fault); return false; }
		xrtClearError();
		return p->Remove(p, owner, value) && records->sIds[0][0] == '\0' &&
			p->Remove(p, owner, value) && p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value);
	}
	if(strcmp(mode, "rest-capacity") == 0) {
		for(unsigned i = 0u; i < XACME_DNS_RECORD_MAX; i++) {
			char text[40]; int n = snprintf(text, sizeof(text), "digest_%u", i);
			if(p->Add(p, owner, (xstrview){ text, (size_t)n }) ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			xrtClearError();
		}
		return records->iCount == XACME_DNS_RECORD_MAX && !p->Add(p, owner, second) &&
			xrtErrorKind(xrtGetError()) == XERR_RANGE;
	}
	if(strcmp(mode, "rest-concurrent") == 0) {
		xatomic32 start = { 0u }; xthread* threads[2] = { 0 }; parallel_add tasks[2] = { 0 };
		for(unsigned i = 0u; i < 2u; i++) {
			tasks[i].provider = p; tasks[i].start = &start;
			tasks[i].owner = i ? XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM") : owner;
			strcpy(tasks[i].value, value.Data);
			threads[i] = xrtThreadCreate(add_in_thread, &tasks[i], 0u);
			if(threads[i] == NULL) {
				xrtAtomic32Store(&start, 1u, XMEMORY_RELEASE);
				if(i != 0u) { (void)xrtThreadWait(threads[0]); xrtThreadDestroy(threads[0]); }
				return false;
			}
		}
		xrtAtomic32Store(&start, 1u, XMEMORY_RELEASE);
		for(unsigned i = 0u; i < 2u; i++) {
			if(xrtThreadWaitFor(threads[i], 60000000u) != XWAIT_OK) _Exit(2);
			xrtThreadDestroy(threads[i]);
		}
		return !tasks[0].ok && !tasks[1].ok && records->iCount == 1u && rest_blocked(p, owner, value);
	}
	if(strcmp(mode, "rest-normal") == 0 || strcmp(mode, "rest-collision") == 0) {
		if(!p->Add(p, owner, value) || records->iCount != 1u || records->sIds[0][0] == '\0') {
			fprintf(stderr, "normal Add count=%zu handle=%s\n", records->iCount, records->sIds[0]); return false;
		}
		if(strcmp(mode, "rest-collision") == 0) {
			if(p->Add(p, owner, second) || records->iCount != 2u || records->sIds[1][0] != '\0' ||
				xrtErrorCode(xrtGetError()) != XACME_DNS_ERROR_UNCERTAIN) return false;
			if(!rest_blocked(p, owner, second)) return false;
		}
		xrtClearError();
		return p->Remove(p, XRT_STR_LITERAL("_ACME-CHALLENGE.API.EXAMPLE.COM"), value) &&
			records->sIds[0][0] == '\0' && p->Remove(p, owner, value);
	}
	if(strcmp(mode, "rest-rejected") == 0) {
		if(p->Add(p, owner, value) || records->iCount != 0u ||
			xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN || xrtErrorKind(xrtGetError()) == XERR_NONE) return false;
		if(strcmp(name, "tencent") == 0 && xrtErrorKind(xrtGetError()) != XERR_PERMISSION) return false;
		xrtClearError();
		return p->Add(p, owner, value) && p->Remove(p, owner, value);
	}
	{
		xacmednsprovider flow = *p;
		ActualAdd = p->Add; flow.Add = counted_add;
		CreateParseOom = strcmp(mode, "rest-parse-oom") == 0;
		UncertaintyErrorOom = strcmp(mode, "rest-error-oom") == 0;
		ok = !xacmeFlowDnsAdd(&flow, owner.Data, value.Data) && FlowAddCalls == 1u &&
			records->iCount == 1u && records->sIds[0][0] == '\0' &&
			strcmp(records->sFqdns[0], owner.Data) == 0 && strcmp(records->sTxts[0], value.Data) == 0;
		if(CreateParseOom) ok = ok && CreateParseOomTriggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		else if(UncertaintyErrorOom) ok = ok && UncertaintyOomTriggered && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
		else ok = ok && xrtErrorCode(xrtGetError()) == XACME_DNS_ERROR_UNCERTAIN;
		CreateParseOom = false; UncertaintyErrorOom = false;
		if(!ok) {
			fprintf(stderr, "flow guard failed: calls=%u slots=%zu handle=%s owner=%s value=%s kind=%d code=%d\n",
				FlowAddCalls, records->iCount, records->sIds[0], records->sFqdns[0], records->sTxts[0],
				(int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError())); return false;
		}
		if(!rest_blocked(p, owner, value)) return false;
	}
	if(strcmp(mode, "rest-isolation") == 0 || strcmp(mode, "rest-presend-reset") == 0) {
		if(strcmp(mode, "rest-presend-reset") == 0) {
			RestPresendFailure = true; xrtClearError();
			ok = !p->Add(p, owner, second) && records->iCount == 1u && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
			RestPresendFailure = false;
			if(!ok) return false;
		}
		xrtClearError();
		if(!p->Add(p, owner, second) || !p->Remove(p, owner, second) ||
			records->sIds[0][0] != '\0' || records->sFqdns[0][0] == '\0') return false;
		return rest_blocked(p, owner, value);
	}
	return true;
}

/* Run before constructing a network engine: global debug counts then measure
 * parser/error ownership without unrelated worker allocation or retirement. */
static bool zone_parser_cleanup(cstr name, bool found)
{
	char body[400], id[80];
	bool cf = strcmp(name, "cf") == 0;
	cstr owner = cf ? "_acme-challenge.api.example.com" : "_acme-challenge.api.example.com.";
	int size = snprintf(body, sizeof(body), "%s%s%s%s",
		cf ? "{\"success\":true,\"result\":[" : "{\"zones\":[",
		found ? "{\"id\":\"apex\",\"name\":\"" : "",
		found ? owner : "", found ? "\"}],\"zone-probe\":true}" : "],\"zone-probe\":true}");
	if(size <= 0 || (size_t)size >= sizeof(body)) return false;
	for(size_t point = 0u; point < 256u; point++) {
		xmemdebugsnapshot before, after;
		xacmednszoneresult result;
		bool triggered, ok;
		xrtClearError(); xrtMemDebugSnapshot(&before);
		if(!xrtMemDebugFailAfter(point)) return false;
		result = xacmeDnsJsonZoneId((xstrview){ body, (size_t)size },
			cf ? "result" : "zones", owner, cf, cf ? 5u : 2u, id, sizeof(id));
		triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
		ok = triggered ? result == XACME_DNS_ZONE_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY && id[0] == '\0' :
			result == (found ? XACME_DNS_ZONE_FOUND : XACME_DNS_ZONE_MISSING);
		xrtClearError(); xrtMemDebugSnapshot(&after);
		if(!ok || before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) {
			fprintf(stderr, "isolated zone parser point=%zu before=%zu/%zu after=%zu/%zu\n", point,
				before.LiveCount, before.LiveBytes, after.LiveCount, after.LiveBytes); return false;
		}
		if(!triggered) {
			printf("zone parser %s/%s cleanup OOM allocation points=%zu\n", name, found ? "found" : "empty", point);
			return point != 0u;
		}
	}
	return false;
}

static bool report_live_allocation(const xmemdebugallocation* allocation, ptr unused)
{
	(void)unused;
	fprintf(stderr, "provider exit live allocation: address=%p bytes=%zu source=%s:%u\n",
		allocation->Address, allocation->Size, allocation->File ? allocation->File : "unknown",
		(unsigned)allocation->Line);
	return true;
}

int main(int argc, char** argv)
{
	xacmednsprovider provider = { 0 };
	xmemdebugsnapshot stats;
	size_t pending = 0u;
	FILE* file;
	long size;
	bool aws, ok;
	if(argc != 5 || !xrtMemDebugEnable(true)) return 2;
	file = fopen(argv[2], "rb");
	if(file == NULL || fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
		fseek(file, 0, SEEK_SET)) return 2;
	TestCa = malloc((size_t)size + 1u);
	if(TestCa == NULL || fread(TestCa, 1u, (size_t)size, file) != (size_t)size) return 2;
	TestCa[size] = '\0'; fclose(file);
	if(strncmp(argv[4], "rest-zone-input-oom-", 20u) == 0 &&
		!zone_parser_cleanup(argv[3], strcmp(argv[4], "rest-zone-input-oom-found") == 0)) {
		free(TestCa); return 1;
	}
	aws = strcmp(argv[3], "aws") == 0;
	if(aws) {
		xacmednsawsconfig config;
		xrtAcmeDnsAwsConfigInit(&config);
		config.sAccessKeyId = "test-id"; config.sSecretAccessKey = "test-key";
		config.sEndpoint = argv[1];
		ok = xrtAcmeDnsAws(&config, NULL, &provider);
	} else if(strcmp(argv[3], "cf") == 0) {
		xacmednscfconfig config;
		xrtAcmeDnsCfConfigInit(&config); config.sApiToken = "test-token"; config.sEndpoint = argv[1];
		ok = xrtAcmeDnsCf(&config, NULL, &provider);
	} else if(strcmp(argv[3], "tencent") == 0) {
		xacmednstencentconfig config;
		xrtAcmeDnsTencentConfigInit(&config); config.sSecretId = "test-id"; config.sSecretKey = "test-key";
		config.sEndpoint = argv[1]; ok = xrtAcmeDnsTencent(&config, NULL, &provider);
	} else if(strcmp(argv[3], "huawei") == 0) {
		xacmednshuaaweiconfig config;
		xrtAcmeDnsHuaweiConfigInit(&config); config.sAccessKey = "test-id"; config.sSecretKey = "test-key";
		config.sEndpoint = argv[1]; ok = xrtAcmeDnsHuawei(&config, NULL, &provider);
	} else {
		xacmednaliconfig config;
		xrtAcmeDnsAliConfigInit(&config);
		config.sAccessKeyId = "test-id"; config.sAccessKeySecret = "test-key";
		config.sEndpoint = argv[1];
		ok = xrtAcmeDnsAli(&config, NULL, &provider);
	}
	if(ok) ok = strncmp(argv[4], "rest-", 5u) == 0 ? exercise_rest(&provider, argv[3], argv[4]) : exercise(&provider, aws, argv[4]);
	if(!ok) fprintf(stderr, "provider=%s mode=%s failed: error kind=%d code=%d\n",
		argv[3], argv[4], (int)xrtErrorKind(xrtGetError()), (int)xrtErrorCode(xrtGetError()));
	if(aws) xrtAcmeDnsAwsProviderUnit(&provider);
	else if(strcmp(argv[3], "cf") == 0) xrtAcmeDnsCfProviderUnit(&provider);
	else if(strcmp(argv[3], "tencent") == 0) xrtAcmeDnsTencentProviderUnit(&provider);
	else if(strcmp(argv[3], "huawei") == 0) xrtAcmeDnsHuaweiProviderUnit(&provider);
	else xrtAcmeDnsAliProviderUnit(&provider);
	if(provider.pContext != NULL) {
		fprintf(stderr, "provider=%s mode=%s exit retained context: error kind=%d domain=%s code=%d\n",
			argv[3], argv[4], (int)xrtErrorKind(xrtGetError()), xrtErrorDomain(xrtGetError()),
			(int)xrtErrorCode(xrtGetError()));
		ok = false;
	}
	xrtClearError(); free(TestCa);
	if(!xrtAcmeCleanupPending(2000000u, &pending)) {
		fprintf(stderr, "provider=%s mode=%s exit pending cleanup: count=%zu error kind=%d domain=%s code=%d\n",
			argv[3], argv[4], pending, (int)xrtErrorKind(xrtGetError()), xrtErrorDomain(xrtGetError()),
			(int)xrtErrorCode(xrtGetError()));
		ok = false;
	}
	xrtClearError();
	xrtMemDebugSnapshot(&stats);
	if(stats.LiveCount || stats.LiveBytes || stats.InvalidFreeCount || stats.DoubleFreeCount) {
		fprintf(stderr, "provider=%s mode=%s exit memory: live=%zu bytes=%zu invalid=%llu double=%llu\n",
			argv[3], argv[4], stats.LiveCount, stats.LiveBytes,
			(unsigned long long)stats.InvalidFreeCount, (unsigned long long)stats.DoubleFreeCount);
		(void)xrtMemDebugVisitLive(report_live_allocation, NULL);
		ok = false;
	}
	return ok ? 0 : 1;
}
