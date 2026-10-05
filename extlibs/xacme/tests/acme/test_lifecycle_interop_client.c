#include <xrt/detail/wait.h>
/* Actual modular implementations, with deterministic retirement faults. */
#define XACME_MODULE_ALL
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include <xrt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TestCa;
static bool InjectRetirementError;
static unsigned RollbackFault;
static xnetengine* RollbackEngine;
static bool ForceStartFailure;
static bool RollbackPinned;
static unsigned Starts;
static xatomic32 HoldRetirement = { 0u };
static bool DnsSendFailure;
static bool DnsReceiveFailure;

static xnetresult dns_send(xnetudp* udp, const void* data, size_t size)
{
	if(!DnsSendFailure) return xrtNetUdpSend(udp, data, size);
	xrtSetErrorInfo(XERR_IO, "test", 6, "injected DNS send error");
	return XNET_RESULT_ERROR;
}

static xnetudppacket* dns_receive(xnetudp* udp, double deadline, xcancel* cancel)
{
	if(!DnsReceiveFailure) return __xrtNetUdpReceiveWait(udp, deadline, cancel);
	xrtSetErrorInfo(XERR_TIMEOUT, "test", 7, "injected DNS receive timeout");
	return NULL;
}

static bool start_engine(xnetengine* engine)
{
	Starts++;
	if(!xrtNetEngineStart(engine)) return false;
	if(!ForceStartFailure) return true;
	RollbackEngine = engine;
	RollbackPinned = xrtNetEnginePin(engine);
	xrtSetErrorInfo(XERR_STATE, "test", 4, "injected partial start failure");
	return false;
}

static double rollback_deadline(uint64 timeout)
{
	if(RollbackFault != 0u && timeout > 20000u) timeout = 20000u;
	return __xrtWaitAfter(timeout);
}

static xnetretireresult retire_engine(xnetengine* engine)
{
	if(RollbackFault != 0u) {
		if(RollbackEngine == NULL) {
			RollbackEngine = engine;
			if(RollbackFault == 2u) {
				RollbackPinned = xrtNetEnginePin(engine);
				if(!RollbackPinned) return XNET_RETIRE_ERROR;
			}
		}
		if(RollbackFault == 1u) {
			xrtSetErrorInfo(XERR_STATE, "test", 3, "persistent retirement error");
			return XNET_RETIRE_ERROR;
		}
	}
	{
		uint32 expected = 1u;
		if(xrtAtomic32CompareExchange(&HoldRetirement, &expected, 2u,
			XMEMORY_ACQ_REL, XMEMORY_RELAXED))
			while(xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) == 2u) xrtSleep(1u);
	}
	if(InjectRetirementError) {
		InjectRetirementError = false;
		xrtSetErrorInfo(XERR_STATE, "test", 1, "injected retirement error");
		return XNET_RETIRE_ERROR;
	}
	return xrtNetEngineTryDestroy(engine);
}

/* Keep provider construction independent of the host's trust-store contents. */
static xx509store* test_system_store(void)
{
	xx509store* store = xrtX509StoreCreate();
	size_t added = 0u;
	if(store == NULL || !xrtX509StoreAddPem(store, TestCa, strlen(TestCa), &added) || added == 0u) {
		xrtX509StoreFree(store); return NULL;
	}
	return store;
}

#define xrtNetEngineTryDestroy retire_engine
#define xrtNetEngineStart start_engine
#define xrtX509StoreSystem test_system_store
#define __xrtWaitAfter rollback_deadline
#include "../../src/acme/xacme_http.c"
#undef xrtNetEngineTryDestroy
#undef xrtNetEngineStart
#undef xrtX509StoreSystem
#undef __xrtWaitAfter
#include "../../src/acme/xacme_core.c"
#include "../../src/acme/xacme_csr.c"
#include "../../src/acme/xacme_jose.c"
#include "../../src/acme/xacme_store.c"
#include "../../src/acme/xacme_flow.c"
#include "../../src/acme/xacme_dns.c"
#define xrtNetUdpSend dns_send
#define __xrtNetUdpReceiveWait dns_receive
#define xrtNetEngineTryDestroy retire_engine
#define xrtNetEngineStart start_engine
#define __xrtWaitAfter rollback_deadline
#include "../../src/dns/xacme_dnstxt.c"
#undef xrtNetUdpSend
#undef __xrtNetUdpReceiveWait
#undef xrtNetEngineTryDestroy
#undef xrtNetEngineStart
#undef __xrtWaitAfter
#include "../../src/dns/xacme_dns_aws_txt.c"

static bool WatchProviderGuard;
static unsigned ProviderHttpCalls, ProviderSignCalls, ProviderRandomCalls, ProviderGuardChecks;

static bool guard_http_v(xacmehttp* http, cstr method, cstr url, cstr content_type,
	xstrview body, const xacmehttpheader* headers, size_t count, xacmehttpresponse* response)
{
	if(!WatchProviderGuard)
		return xacmeHttpExchangeV(http, method, url, content_type, body, headers, count, response);
	ProviderHttpCalls++;
	xrtSetErrorInfo(XERR_INTERNAL, "test.provider.guard", 1, "unexpected provider HTTP request");
	return false;
}

static bool guard_http_once_v(xacmehttp* http, cstr method, cstr url, cstr content_type,
	xstrview body, const xacmehttpheader* headers, size_t count, xacmehttpresponse* response)
{
	if(!WatchProviderGuard)
		return xacmeHttpExchangeOnceV(http, method, url, content_type, body, headers, count, response);
	return guard_http_v(http, method, url, content_type, body, headers, count, response);
}

static bool guard_hmac(const void* key, size_t key_size, const void* data, size_t size, void* mac)
{
	if(WatchProviderGuard) ProviderSignCalls++;
	return xrtHmacSha256(key, key_size, data, size, mac);
}

static bool guard_sha256(const void* data, size_t size, void* digest)
{
	if(WatchProviderGuard) ProviderSignCalls++;
	return xrtSha256(data, size, digest);
}

static bool guard_random(ptr data, size_t size)
{
	if(WatchProviderGuard) ProviderRandomCalls++;
	return xrtSecureRandom(data, size);
}

#define xacmeHttpExchangeV guard_http_v
#define xacmeHttpExchangeOnceV guard_http_once_v
#define xrtHmacSha256 guard_hmac
#define xrtSha256 guard_sha256
#define xrtSecureRandom guard_random
#include "../../src/dns/xacme_dns_ali.c"
#include "../../src/dns/xacme_dns_cf.c"
#include "../../src/dns/xacme_dns_tencent.c"
#include "../../src/dns/xacme_dns_aws.c"
#include "../../src/dns/xacme_dns_huawei.c"
#undef xacmeHttpExchangeV
#undef xacmeHttpExchangeOnceV
#undef xrtHmacSha256
#undef xrtSha256
#undef xrtSecureRandom

/* Only the Obtain dependencies are controlled; its actual Done path and
 * the client/HTTP/engine cleanup implementations remain under test. */
#include <xrt/acme_obtain.h>
static xacmeclient* obtain_client(const xacmeclientconfig* config);
static str obtain_account(cstr root, cstr directory);
static bool obtain_issue(xacmeclient* client, const xstrview* domains, size_t count,
	const xacmednsprovider* dns, cstr root, int days, xacmeissuegrant* grant, bool* renewed);
static bool ObtainSuccess;
#define xrtAcmeClientCreate obtain_client
#define xrtAcmeStoreLoadAccount obtain_account
#define xrtAcmeClientIssueStored obtain_issue
#include "../../src/acme/xacme_obtain.c"
#undef xrtAcmeClientCreate
#undef xrtAcmeStoreLoadAccount
#undef xrtAcmeClientIssueStored

typedef struct allocation_check {
	ptr address;
	bool live;
} allocation_check;

static bool find_allocation(const xmemdebugallocation* allocation, ptr data)
{
	allocation_check* check = (allocation_check*)data;
	if(allocation->Address == check->address) check->live = true;
	return !check->live;
}

static bool allocation_live(ptr address)
{
	allocation_check check = { address, false };
	xrtMemDebugVisitLive(find_allocation, &check);
	return check.live;
}

static bool memory_empty(void)
{
	xmemdebugsnapshot memory;
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	if(memory.LiveCount != 0u || memory.LiveBytes != 0u ||
		memory.InvalidFreeCount != 0u || memory.DoubleFreeCount != 0u) {
		fprintf(stderr, "ownership cleanup: live=%zu bytes=%zu invalid=%llu double=%llu\n",
			memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
			(unsigned long long)memory.DoubleFreeCount);
		return false;
	}
	return true;
}

static xnetengine* create_engine(void)
{
	xnetengineconfig config;
	xnetengine* engine;
	xrtNetEngineConfigInit(&config);
	engine = xrtNetEngineCreate(&config);
	if(engine == NULL || !xrtNetEngineStart(engine)) return NULL;
	return engine;
}

static bool provider_create(unsigned kind, xnetengine* borrowed, xacmednsprovider* provider)
{
	switch(kind) {
	case 0: {
		xacmednaliconfig config;
		xrtAcmeDnsAliConfigInit(&config);
		config.sAccessKeyId = "test-id"; config.sAccessKeySecret = "test-secret";
		return xrtAcmeDnsAli(&config, borrowed, provider);
	}
	case 1: {
		xacmednscfconfig config;
		xrtAcmeDnsCfConfigInit(&config); config.sApiToken = "test-token";
		return xrtAcmeDnsCf(&config, borrowed, provider);
	}
	case 2: {
		xacmednstencentconfig config;
		xrtAcmeDnsTencentConfigInit(&config);
		config.sSecretId = "test-id"; config.sSecretKey = "test-secret";
		return xrtAcmeDnsTencent(&config, borrowed, provider);
	}
	case 3: {
		xacmednsawsconfig config;
		xrtAcmeDnsAwsConfigInit(&config);
		config.sAccessKeyId = "test-id"; config.sSecretAccessKey = "test-secret";
		return xrtAcmeDnsAws(&config, borrowed, provider);
	}
	case 4: {
		xacmednshuaaweiconfig config;
		xrtAcmeDnsHuaweiConfigInit(&config);
		config.sAccessKey = "test-id"; config.sSecretKey = "test-secret";
		return xrtAcmeDnsHuawei(&config, borrowed, provider);
	}
	default: return false;
	}
}

static xacmehttp* provider_http(unsigned kind, xacmednsprovider* provider)
{
	switch(kind) {
	case 0: return &((xacmednsalicontext*)provider->pContext)->Http;
	case 1: return &((xacmednscfcontext*)provider->pContext)->Http;
	case 2: return &((xacmednstencentcontext*)provider->pContext)->Http;
	case 3: return &((xacmednsawscontext*)provider->pContext)->Http;
	case 4: return &((xacmednshuaaweicontext*)provider->pContext)->Http;
	default: return NULL;
	}
}

static bool provider_ownership(void)
{
	static const char* names[] = { "Ali", "Cf", "Tencent", "Aws", "Huawei" };
	static void (*const units[])(xacmednsprovider*) = {
		xrtAcmeDnsAliProviderUnit, xrtAcmeDnsCfProviderUnit, xrtAcmeDnsTencentProviderUnit,
		xrtAcmeDnsAwsProviderUnit, xrtAcmeDnsHuaweiProviderUnit
	};
	for(unsigned kind = 0u; kind < 5u; kind++) {
		for(unsigned scenario = 0u; scenario < 3u; scenario++) {
			xacmednsprovider provider = { 0 };
			xnetengine *borrowed = scenario == 2u ? create_engine() : NULL, *engine;
			xacmehttp* http;
			ptr context;
			xerror* previous;
			bool ok, pinned = scenario != 1u;
			if(scenario == 2u && borrowed == NULL) return false;
			if(!provider_create(kind, borrowed, &provider)) return false;
			context = provider.pContext;
			http = provider_http(kind, &provider);
			engine = http->pEngine;
			http->uTimeoutUs = 20000u;
			if(pinned && !xrtNetEnginePin(engine)) return false;
			xrtSetErrorInfo(XERR_PROTOCOL, "test", 2, "original operation error");
			previous = xrtErrorRef(xrtGetError());
			InjectRetirementError = scenario == 1u;
			units[kind](&provider);
			ok = xrtGetError() == previous;
			xrtErrorFree(previous);
			if(scenario < 2u) {
				/* Confirm the allocation before dereferencing a possibly discarded context. */
				if(provider.pContext != context || !allocation_live(context)) {
					fprintf(stderr, "%s Unit discarded a busy context\n", names[kind]);
					return false;
				}
				ok = ok && http->pEngine == engine && http->bEngineOwned &&
					http->pResolver == NULL && http->pVerifier == NULL &&
					xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			} else ok = ok && provider.pContext == NULL && !allocation_live(context) &&
				xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			xrtClearError();
			if(pinned && !xrtNetEngineUnpin(engine)) return false;
			if(scenario < 2u) http->uTimeoutUs = 5000000u;
			units[kind](&provider);
			units[kind](&provider);
			ok = ok && provider.pContext == NULL;
			if(borrowed != NULL && !xrtNetEngineDestroy(borrowed)) return false;
			if(!memory_empty() || !ok) { fprintf(stderr, "%s ownership %u failed\n", names[kind], scenario); return false; }
		}
		printf("  %s Unit timeout/error/borrowed ownership and retry: passed\n", names[kind]);
	}
	return true;
}

static bool guard_call(xacmednsaddproc operation, xacmednsprovider* provider,
	xstrview fqdn, xstrview txt, xerrkind kind, int32 code)
{
	const xerror* error;
	bool result, ok;
	/* A stale error must never make a callback which forgot to set one pass. */
	xrtClearError();
	xrtSetErrorInfo(XERR_PROTOCOL, "test.guard.sentinel", 91, "replace this error");
	result = operation(provider, fqdn, txt);
	error = xrtGetError();
	ok = !result && error != NULL && xrtErrorKind(error) == kind &&
		strcmp(xrtErrorDomain(error), "xrt.acme.dns") == 0 && xrtErrorCode(error) == code;
	if(!ok) fprintf(stderr, "provider guard returned=%d kind=%d domain=%s code=%d\n",
		(int)result, (int)xrtErrorKind(error), xrtErrorDomain(error), (int)xrtErrorCode(error));
	xrtClearError();
	if(ok) ProviderGuardChecks++;
	return ok;
}

static bool custom_guard_operation(xacmednsprovider* provider, xstrview fqdn, xstrview txt)
{
	(void)provider; (void)fqdn; (void)txt;
	return true;
}

static ptr provider_guard_payload(unsigned kind, xacmednsprovider* provider, size_t* size)
{
	/* Exclude the mutex: locking may change its private platform bookkeeping. */
	#define GUARD_PAYLOAD_CASE(index, type, field) \
		case index: { type* ctx = (type*)provider->pContext; \
			*size = sizeof(*ctx) - offsetof(type, field); return &ctx->field; }
	switch(kind) {
	GUARD_PAYLOAD_CASE(0, xacmednsalicontext, sKeyId)
	GUARD_PAYLOAD_CASE(1, xacmednscfcontext, sToken)
	GUARD_PAYLOAD_CASE(2, xacmednstencentcontext, sId)
	GUARD_PAYLOAD_CASE(3, xacmednsawscontext, sId)
	GUARD_PAYLOAD_CASE(4, xacmednshuaaweicontext, sAk)
	default: *size = 0u; return NULL;
	}
	#undef GUARD_PAYLOAD_CASE
}

static bool provider_guard_seed(unsigned kind, xacmednsprovider* provider)
{
	xacmednsrecords* records;
	switch(kind) {
	case 0: records = &((xacmednsalicontext*)provider->pContext)->Records; break;
	case 1: records = &((xacmednscfcontext*)provider->pContext)->Records; break;
	case 2: records = &((xacmednstencentcontext*)provider->pContext)->Records; break;
	case 4: records = &((xacmednshuaaweicontext*)provider->pContext)->Records; break;
	case 3: {
		xacmednsawscontext* ctx = (xacmednsawscontext*)provider->pContext;
		strcpy(ctx->Records[0].sZoneId, "test-zone");
		strcpy(ctx->Records[0].sFqdn, "_acme-challenge.api.example.com");
		strcpy(ctx->Records[0].sValue, "digest_01");
		ctx->iRecordCount = 1u;
		return true;
	}
	default: return false;
	}
	/* Synthetic local ownership exercises the reuse/remove paths without a cloud write. */
	return xacmeDnsRecordRemember(records, "test-record",
		XRT_STR_LITERAL("_acme-challenge.api.example.com"), XRT_STR_LITERAL("digest_01"));
}

static bool provider_guard_preserves(unsigned kind, xacmednsprovider* provider)
{
	size_t size;
	ptr payload = provider_guard_payload(kind, provider, &size);
	void* saved = malloc(size);
	xacmehttp* http = provider_http(kind, provider);
	unsigned char saved_http[sizeof(*http)];
	unsigned requests = ProviderHttpCalls, signs = ProviderSignCalls, randoms = ProviderRandomCalls;
	bool ok;
	if(payload == NULL || saved == NULL) { free(saved); return false; }
	memcpy(saved, payload, size);
	memcpy(saved_http, http, sizeof(saved_http));
	WatchProviderGuard = true;
	ok = guard_call(provider->Add, provider, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
		XRT_STR_LITERAL("digest_01"), XERR_STATE, XACME_DNS_ERROR_STATE);
	ok = guard_call(provider->Remove, provider, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
		XRT_STR_LITERAL("digest_01"), XERR_STATE, XACME_DNS_ERROR_STATE) && ok;
	ok = guard_call(provider->Remove, provider, XRT_STR_LITERAL("_acme-challenge.absent.example.org"),
		XRT_STR_LITERAL("untracked"), XERR_STATE, XACME_DNS_ERROR_STATE) && ok;
	WatchProviderGuard = false;
	ok = ok && requests == ProviderHttpCalls && signs == ProviderSignCalls &&
		randoms == ProviderRandomCalls && memcmp(saved, payload, size) == 0 &&
		memcmp(saved_http, http, sizeof(saved_http)) == 0;
	free(saved);
	return ok;
}

static bool provider_callback_guards(void)
{
	static const char* names[] = { "Ali", "Cf", "Tencent", "Aws", "Huawei" };
	static void (*const units[])(xacmednsprovider*) = {
		xrtAcmeDnsAliProviderUnit, xrtAcmeDnsCfProviderUnit, xrtAcmeDnsTencentProviderUnit,
		xrtAcmeDnsAwsProviderUnit, xrtAcmeDnsHuaweiProviderUnit
	};
	_Static_assert(XACME_DNS_ERROR_UNCERTAIN == 6 && XACME_DNS_ERROR_STATE == 7,
		"existing DNS error codes must remain stable");
	for(unsigned kind = 0u; kind < 5u; kind++) {
		for(unsigned scenario = 0u; scenario < 3u; scenario++) {
			xacmednsprovider provider = { 0 };
			xnetengine* borrowed = scenario == 2u ? create_engine() : NULL;
			xnetengine* engine;
			xacmehttp* http;
			ptr context;
			xacmednsaddproc add;
			xacmednsremoveproc remove;
			xerror* previous;
			bool ok;
			if((scenario == 2u && borrowed == NULL) || !provider_create(kind, borrowed, &provider)) return false;
			context = provider.pContext; http = provider_http(kind, &provider); engine = http->pEngine;
			add = provider.Add; remove = provider.Remove;
			if(!provider_guard_seed(kind, &provider)) return false;
			if(scenario == 0u) {
				/* Reject each incomplete transport before owned shortcuts or empty Remove. */
				for(unsigned missing = 0u; missing < 3u; missing++) {
					xacmehttp complete;
					memcpy(&complete, http, sizeof(complete));
					if(missing == 0u) http->pEngine = NULL;
					else if(missing == 1u) http->pResolver = NULL;
					else http->pVerifier = NULL;
					ok = provider_guard_preserves(kind, &provider);
					memcpy(http, &complete, sizeof(complete));
					if(!ok) { fprintf(stderr, "%s incomplete transport guard %u failed\n", names[kind], missing); return false; }
				}
				if(!xrtNetEnginePin(engine)) return false;
			}
			http->uTimeoutUs = 20000u;
			xrtSetErrorInfo(XERR_IO, "test.guard.cleanup", 92, "original cleanup diagnosis");
			previous = xrtErrorRef(xrtGetError());
			InjectRetirementError = scenario == 1u;
			units[kind](&provider);
			ok = xrtGetError() == previous;
			xrtErrorFree(previous);
			if(scenario < 2u) {
				if(provider.pContext != context || !allocation_live(context)) return false;
				ok = provider_guard_preserves(kind, &provider) && ok;
				if(scenario == 0u && !xrtNetEngineUnpin(engine)) return false;
				http->uTimeoutUs = 5000000u;
				xrtClearError();
				units[kind](&provider);
			}
			units[kind](&provider);
			units[kind](NULL);
			if(provider.pContext != NULL || allocation_live(context)) return false;
			/* Saved callbacks stay callable, but an owned engine is no longer readable. */
			WatchProviderGuard = true;
			ok = guard_call(add, &provider, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
				XRT_STR_LITERAL("digest_01"), XERR_STATE, XACME_DNS_ERROR_STATE) && ok;
			ok = guard_call(remove, &provider, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
				XRT_STR_LITERAL("digest_01"), XERR_STATE, XACME_DNS_ERROR_STATE) && ok;
			ok = guard_call(add, NULL, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
				XRT_STR_LITERAL("digest_01"), XERR_ARGUMENT, XACME_DNS_ERROR_ARGUMENT) && ok;
			ok = guard_call(remove, NULL, XRT_STR_LITERAL("_acme-challenge.api.example.com"),
				XRT_STR_LITERAL("digest_01"), XERR_ARGUMENT, XACME_DNS_ERROR_ARGUMENT) && ok;
			WatchProviderGuard = false;
			if(borrowed != NULL) {
				if(!allocation_live(borrowed) || xrtNetEngineState(borrowed) != XNET_ENGINE_RUNNING) return false;
				if(!xrtNetEngineDestroy(borrowed)) return false;
			}
			if(!memory_empty() || !ok || ProviderHttpCalls != 0u || ProviderSignCalls != 0u ||
				ProviderRandomCalls != 0u) { fprintf(stderr, "%s callback guards %u failed\n", names[kind], scenario); return false; }
		}
		printf("  %s callback guards: null/released, timeout/error, incomplete transport, no side effects passed\n", names[kind]);
	}
	{
		xacmednsprovider custom = { "custom", 0u, NULL, custom_guard_operation, custom_guard_operation, NULL };
		if(!xrtAcmeDnsProviderValidate(&custom) ||
			!custom.Add(&custom, XRT_STR_LITERAL("_acme-challenge.api.example.com"), XRT_STR_LITERAL("digest_01"))) return false;
	}
	printf("  provider lifecycle guards: %u exact-error checks, zero HTTP/signing/random calls\n", ProviderGuardChecks);
	return ProviderGuardChecks == 135u && memory_empty();
}

static bool client_ownership(void)
{
	if(!xrtAcmeClientCleanup(NULL) || !xacmeClientUnit(NULL)) return false;
	for(unsigned action = 0u; action < 3u; action++) {
		for(unsigned scenario = 0u; scenario < 3u; scenario++) {
			xacmeclient* client = (xacmeclient*)xrtCalloc(1u, sizeof(*client));
			xnetengine *borrowed = scenario == 2u ? create_engine() : NULL, *engine;
			xerror* previous;
			bool ready, ok, pinned = scenario != 1u;
			if(client == NULL || (scenario == 2u && borrowed == NULL)) return false;
			if(!xacmeHttpInit(&client->Http, borrowed, TestCa, 20000u)) return false;
			memset(&client->AccountKey, 0xa5, sizeof(client->AccountKey));
			if(action > 0u) {
				client->pCertKey = (xacmecertkey*)xrtCalloc(1u, sizeof(*client->pCertKey));
				if(client->pCertKey == NULL) return false;
			}
			engine = client->Http.pEngine;
			if(pinned && !xrtNetEnginePin(engine)) return false;
			xrtSetErrorInfo(XERR_PROTOCOL, "test", 2, "original operation error");
			previous = xrtErrorRef(xrtGetError());
			InjectRetirementError = scenario == 1u;
			if(action == 0u) ready = xacmeClientUnit(client);
			else if(action == 1u) ready = xrtAcmeClientCleanup(client);
			else {
				xrtAcmeClientDestroy(client);
				ready = !allocation_live(client);
			}
			ok = ready == (scenario == 2u) && xrtGetError() == previous;
			xrtErrorFree(previous);
			if(scenario < 2u) {
				if(!allocation_live(client)) {
					fprintf(stderr, "client action %u discarded a busy handle\n", action); return false;
				}
				ok = ok && client->Http.pEngine == engine && client->Http.bEngineOwned &&
					client->Http.pResolver == NULL && client->Http.pVerifier == NULL &&
					client->pCertKey == NULL && xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			} else ok = ok && xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			xrtClearError();
			if(pinned && !xrtNetEngineUnpin(engine)) return false;
			if(scenario < 2u) client->Http.uTimeoutUs = 5000000u;
			if(action != 2u || scenario < 2u) {
				ok = xrtAcmeClientCleanup(client) && ok;
				ok = xrtAcmeClientCleanup(client) && ok;
				xrtAcmeClientDestroy(client);
			}
			if(borrowed != NULL && !xrtNetEngineDestroy(borrowed)) return false;
			if(!memory_empty() || !ok) { fprintf(stderr, "client action %u ownership %u failed\n", action, scenario); return false; }
		}
		printf("  client action %u timeout/error/borrowed ownership and retry: passed\n", action);
	}
	return true;
}

static bool failed_client_create(void)
{
	for(unsigned scenario = 0u; scenario < 5u; scenario++) {
		xacmeaccountconfig account;
		xacmeclientconfig config;
		xacmeclient* client;
		char long_directory[600];
		xrtAcmeAccountConfigInit(&account);
		xrtAcmeClientConfigInit(&config);
		account.sDirectoryUrl = "https://localhost:9/directory";
		memset(long_directory, 'x', sizeof(long_directory) - 1u);
		long_directory[sizeof(long_directory) - 1u] = 0;
		if(scenario == 2u) account.sDirectoryUrl = NULL;
		if(scenario == 3u) account.sDirectoryUrl = "";
		if(scenario == 4u) account.sDirectoryUrl = long_directory;
		account.sAccountKeyPem = "invalid account private key";
		config.pAccount = &account;
		config.sCaPem = scenario == 0u ? "invalid CA" : TestCa;
		config.uTimeoutUs = 1u;
		client = xrtAcmeClientCreate(&config);
		if(client != NULL) { xrtAcmeClientDestroy(client); return false; }
		if(scenario >= 2u && xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) return false;
		if(!memory_empty()) { fprintf(stderr, "failed client create %u retained resources\n", scenario); return false; }
	}
	puts("  failed public client construction with a 1us deadline: passed");
	return true;
}

typedef struct rollback_owner_check {
	xnetengine* engine;
	ptr owner;
	size_t size;
} rollback_owner_check;

static bool find_rollback_owner(const xmemdebugallocation* allocation, ptr data)
{
	rollback_owner_check* check = (rollback_owner_check*)data;
	if(allocation->Size == check->size &&
		((xacmehttp*)allocation->Address)->pEngine == check->engine)
		check->owner = allocation->Address;
	return check->owner == NULL;
}

static bool bytes_zero(const void* data, size_t size)
{
	const unsigned char* bytes = (const unsigned char*)data;
	for(size_t i = 0u; i < size; i++) if(bytes[i] != 0u) return false;
	return true;
}

static bool error_contains(const xerror* error, const char* message)
{
	for(; error != NULL; error = xrtErrorCause(error))
		if(strcmp(xrtErrorMessage(error), message) == 0) return true;
	return false;
}

static bool rollback_failures(void)
{
	static const size_t sizes[] = {
		sizeof(xacmednsalicontext), sizeof(xacmednscfcontext), sizeof(xacmednstencentcontext),
		sizeof(xacmednsawscontext), sizeof(xacmednshuaaweicontext), sizeof(xacmeclient)
	};
	bool ok = true;
	for(unsigned kind = 0u; kind < 6u; kind++) {
	for(unsigned fault = 1u; fault <= 2u; fault++) {
		xacmeaccountconfig account;
		xacmeclientconfig config;
		rollback_owner_check check;
		xerror* original;
		size_t pending = SIZE_MAX;
		bool retired;
		xrtAcmeAccountConfigInit(&account);
		xrtAcmeClientConfigInit(&config);
		account.sDirectoryUrl = "https://localhost:9/directory";
		account.sAccountKeyPem = "invalid account private key";
		config.pAccount = &account; config.sCaPem = TestCa; config.uTimeoutUs = 1u;
		RollbackEngine = NULL; RollbackPinned = false; RollbackFault = fault;
		ForceStartFailure = kind < 5u;
		if(kind < 5u) {
			xacmednsprovider provider = { 0 };
			if(provider_create(kind, NULL, &provider) || provider.pContext != NULL) return false;
		} else if(xrtAcmeClientCreate(&config) != NULL) return false;
		ForceStartFailure = false;
		original = xrtErrorRef(xrtGetError());
		if(original == NULL || RollbackEngine == NULL) return false;
		if(kind < 5u && !error_contains(original, "injected partial start failure")) {
			fprintf(stderr, "factory discarded original start error kind=%u\n", kind); return false;
		}
		retired = xrtAcmeCleanupPending(0u, &pending);
		ok = ok && !retired && pending == 1u && xrtGetError() == original &&
			xrtNetEngineState(RollbackEngine) == XNET_ENGINE_RUNNING;
		check = (rollback_owner_check){ RollbackEngine, NULL, sizes[kind] };
		xrtMemDebugVisitLive(find_rollback_owner, &check);
		ok = ok && check.owner != NULL && bytes_zero(
			(uint8*)check.owner + sizeof(xacmehttp), check.size - sizeof(xacmehttp));
		{
			xacmehttp rejected, borrowed_http;
			xnetengine* borrowed;
			unsigned starts = Starts;
			xrtClearError();
			ok = !xacmeHttpInit(&rejected, NULL, TestCa, 1u) && ok;
			ok = ok && rejected.pEngine == NULL && rejected.pResolver == NULL &&
				rejected.pVerifier == NULL && Starts == starts &&
				strcmp(xrtErrorMessage(xrtGetError()),
				 "acme pending cleanup must finish before creating a private engine") == 0;
			borrowed = create_engine();
			if(borrowed == NULL) return false;
			ok = xacmeHttpInit(&borrowed_http, borrowed, TestCa, 1u) && ok;
			ok = xacmeHttpUnit(&borrowed_http) && ok;
			ok = ok && xrtNetEngineState(borrowed) == XNET_ENGINE_RUNNING;
			if(!xrtNetEngineDestroy(borrowed)) return false;
		}
		xrtClearError();
		retired = xrtAcmeCleanupPending(fault == 1u ? 5000000u : 20000u, &pending);
		ok = ok && !retired && pending == 1u &&
			(fault == 1u ? strcmp(xrtErrorMessage(xrtGetError()), "persistent retirement error") == 0 :
			 xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
			 xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_TIMEOUT);
		xrtSetError(original);
		RollbackFault = 0u;
		if(RollbackPinned && !xrtNetEngineUnpin(RollbackEngine)) return false;
		ok = xrtAcmeCleanupPending(5000000u, &pending) && ok;
		ok = ok && pending == 0u && xrtGetError() == original;
		xrtErrorFree(original);
		ok = xrtAcmeCleanupPending(0u, NULL) && ok;
		if(!memory_empty() || !ok) {
			fprintf(stderr, "factory deferred owner kind=%u fault=%u failed\n", kind, fault); return false;
		}
		printf("  factory deferred owner kind=%u fault=%u, admission and retry: passed\n", kind, fault);
	}
	}
	return ok;
}

static xacmeclient* pinned_client(void)
{
	xacmeclient* client = (xacmeclient*)xrtCalloc(1u, sizeof(*client));
	if(client == NULL || !xacmeHttpInit(&client->Http, NULL, TestCa, 1u)) return NULL;
	memset(&client->AccountKey, 0xa5, sizeof(client->AccountKey));
	if(!xrtNetEnginePin(client->Http.pEngine)) return NULL;
	return client;
}

static bool deferred_no_allocation(void)
{
	xacmeclient* client = pinned_client();
	xnetengine* engine;
	xerror* original;
	size_t pending = SIZE_MAX;
	bool ok;
	if(client == NULL) return false;
	engine = client->Http.pEngine;
	xrtSetErrorInfo(XERR_PROTOCOL, "test", 2, "original operation error");
	original = xrtErrorRef(xrtGetError());
	if(original == NULL || xrtAcmeClientCleanup(client)) return false;
	if(!xrtMemDebugFailAfter(0u)) return false;
	xacmeHttpDeferOwner(&client->Http, sizeof(*client));
	ok = !xrtAcmeCleanupPending(0u, &pending) && pending == 1u &&
		!xrtMemDebugFailTriggered() && xrtGetError() == original &&
		bytes_zero((uint8*)client + sizeof(xacmehttp), sizeof(*client) - sizeof(xacmehttp));
	xrtMemDebugFailClear();
	if(!xrtNetEngineUnpin(engine)) return false;
	ok = xrtAcmeCleanupPending(5000000u, &pending) && pending == 0u &&
		xrtGetError() == original && ok;
	xrtErrorFree(original);
	if(!memory_empty() || !ok) return false;
	puts("  deferred handoff/poll at allocation failure zero: passed");
	return true;
}

typedef struct cleanup_task {
	xacmeclient* client;
	bool loop;
	bool ready;
	size_t pending;
} cleanup_task;

static int32 cleanup_thread(ptr data)
{
	cleanup_task* task = (cleanup_task*)data;
	double deadline = __xrtWaitAfter(5000000u);
	if(task->client != NULL) {
		xacmeClientDiscard(task->client);
		return 0;
	}
	do {
		task->ready = xrtAcmeCleanupPending(0u, &task->pending);
		if(!task->loop || task->ready) return 0;
		xrtSleep(1u);
	} while(!__xrtWaitExpired(deadline));
	return 1;
}

static bool join_thread(xthread* thread)
{
	/* Leave room for the worker's own 5s failure deadline and context teardown. */
	xwaitresult result = thread != NULL ? xrtThreadWaitFor(thread, 8000000u) : XWAIT_ERROR;
	bool ok = result == XWAIT_OK;
	if(!ok) fprintf(stderr, "ownership thread wait=%d error=%s\n", (int)result,
		xrtErrorMessage(xrtGetError()));
	if(ok) xrtThreadDestroy(thread);
	return ok;
}

static bool deferred_claimed_owner(void)
{
	xacmeclient* first = pinned_client();
	xacmeclient* second = pinned_client();
	xnetengine *first_engine, *second_engine;
	cleanup_task task = { 0 };
	xthread* thread;
	double deadline;
	size_t pending = SIZE_MAX;
	bool ok;
	if(first == NULL || second == NULL) return false;
	first_engine = first->Http.pEngine; second_engine = second->Http.pEngine;
	xacmeClientDiscard(first);
	xrtClearError();
	if(!xrtNetEngineUnpin(first_engine)) return false;
	xrtAtomic32Store(&HoldRetirement, 1u, XMEMORY_RELEASE);
	thread = xrtThreadCreate(cleanup_thread, &task, 0u);
	if(thread == NULL) return false;
	deadline = __xrtWaitAfter(5000000u);
	while(xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) != 2u &&
		!__xrtWaitExpired(deadline)) xrtSleep(1u);
	ok = xrtAtomic32Load(&HoldRetirement, XMEMORY_ACQUIRE) == 2u &&
		!xrtAcmeCleanupPending(0u, &pending) && pending == 1u;
	/* This enqueue occurs while another cleanup owns the detached first item. */
	xacmeClientDiscard(second);
	ok = !xrtAcmeCleanupPending(0u, &pending) && pending == 2u && ok;
	xrtAtomic32Store(&HoldRetirement, 3u, XMEMORY_RELEASE);
	if(!join_thread(thread)) return false;
	xrtAtomic32Store(&HoldRetirement, 0u, XMEMORY_RELEASE);
	/* TryDestroy may need another worker turn even after the pin is released. */
	ok = !task.ready && task.pending >= 1u && task.pending <= 2u && ok;
	xrtClearError();
	deadline = __xrtWaitAfter(5000000u);
	do {
		ok = !xrtAcmeCleanupPending(0u, &pending) && ok;
		if(pending == 1u) break;
		xrtSleep(1u);
	} while(!__xrtWaitExpired(deadline));
	ok = pending == 1u && ok;
	if(!xrtNetEngineUnpin(second_engine)) return false;
	ok = xrtAcmeCleanupPending(5000000u, &pending) && pending == 0u && ok;
	if(!memory_empty() || !ok) {
		fprintf(stderr, "claimed ownership ready=%d count=%zu final=%zu\n",
			(int)task.ready, task.pending, pending);
		return false;
	}
	puts("  claimed owner count and concurrent enqueue/requeue: passed");
	return true;
}

static bool deferred_concurrent(void)
{
	cleanup_task tasks[4] = { { 0 } };
	xthread* threads[4];
	xnetengine* engines[4];
	size_t pending = SIZE_MAX;
	bool ok;
	for(unsigned i = 0u; i < 4u; i++) {
		tasks[i].client = pinned_client();
		if(tasks[i].client == NULL) return false;
		engines[i] = tasks[i].client->Http.pEngine;
	}
	for(unsigned i = 0u; i < 4u; i++) {
		threads[i] = xrtThreadCreate(cleanup_thread, &tasks[i], 0u);
		if(threads[i] == NULL) return false;
	}
	for(unsigned i = 0u; i < 4u; i++) if(!join_thread(threads[i])) return false;
	xrtClearError();
	/* First ERROR must retain every unvisited node of the detached batch. */
	InjectRetirementError = true;
	ok = !xrtAcmeCleanupPending(0u, &pending) && pending == 4u &&
		strcmp(xrtErrorMessage(xrtGetError()), "injected retirement error") == 0;
	xrtClearError();
	ok = !xrtAcmeCleanupPending(2000u, &pending) && pending == 4u &&
		xrtErrorKind(xrtGetError()) == XERR_TIMEOUT && ok;
	xrtClearError();
	for(unsigned i = 0u; i < 4u; i++) {
		if(!xrtNetEngineUnpin(engines[i])) return false;
		tasks[i] = (cleanup_task){ NULL, true, false, SIZE_MAX };
		threads[i] = xrtThreadCreate(cleanup_thread, &tasks[i], 0u);
		if(threads[i] == NULL) return false;
	}
	for(unsigned i = 0u; i < 4u; i++) {
		if(!join_thread(threads[i])) return false;
		ok = tasks[i].ready && tasks[i].pending == 0u && ok;
	}
	ok = xrtAcmeCleanupPending(0u, &pending) && pending == 0u && ok;
	if(!memory_empty() || !ok) {
		fprintf(stderr, "concurrent deferred ownership final=%zu failed\n", pending); return false;
	}
	puts("  four concurrent producers/drainers, ERROR and timeout tails: passed");
	return true;
}

static xacmeclient* obtain_client(const xacmeclientconfig* config)
{
	xacmeclient* client = (xacmeclient*)xrtCalloc(1u, sizeof(*client));
	if(client == NULL || !xacmeHttpInit(&client->Http, config->pBorrowedEngine,
		config->sCaPem, config->uTimeoutUs)) return NULL;
	memset(&client->AccountKey, 0xa5, sizeof(client->AccountKey));
	return client;
}

static str obtain_account(cstr root, cstr directory)
{
	str text = (str)xrtMalloc(8u);
	(void)root; (void)directory;
	if(text != NULL) memcpy(text, "account", 8u);
	return text;
}

static bool obtain_issue(xacmeclient* client, const xstrview* domains, size_t count,
	const xacmednsprovider* dns, cstr root, int days, xacmeissuegrant* grant, bool* renewed)
{
	(void)client; (void)domains; (void)count; (void)dns; (void)root; (void)days;
	grant->sFullchainPem = (str)xrtCalloc(8u, 1u);
	grant->sKeyPem = (str)xrtCalloc(8u, 1u);
	if(grant->sFullchainPem == NULL || grant->sKeyPem == NULL) return false;
	*renewed = ObtainSuccess;
	if(!ObtainSuccess) xrtSetErrorInfo(XERR_PROTOCOL, "test", 5, "original issue error");
	return ObtainSuccess;
}

static bool deferred_obtain(void)
{
	for(unsigned success = 0u; success < 2u; success++) {
	for(unsigned fault = 1u; fault <= 2u; fault++) {
		xacmeaccountconfig account;
		xacmeobtainconfig config;
		xacmednsprovider dns = { 0 };
		xacmeissuegrant grant;
		xstrview domain = { "example.test", 12u };
		bool renewed = true, result, ok;
		size_t pending = SIZE_MAX;
		xrtAcmeAccountConfigInit(&account); xrtAcmeObtainConfigInit(&config);
		account.sDirectoryUrl = "https://localhost:9/directory";
		config.pAccount = &account; config.sStoreRoot = "unused-test-store";
		config.sCaPem = TestCa; config.uTimeoutUs = 1u;
		ObtainSuccess = success != 0u;
		RollbackEngine = NULL; RollbackPinned = false; RollbackFault = fault;
		xrtClearError();
		result = xrtAcmeObtain(&config, &domain, 1u, &dns, &grant, &renewed);
		ok = result == ObtainSuccess && renewed == ObtainSuccess && RollbackEngine != NULL;
		if(success == 0u) ok = ok && grant.sFullchainPem == NULL && grant.sKeyPem == NULL &&
			strcmp(xrtErrorMessage(xrtGetError()), "original issue error") == 0;
		else ok = ok && grant.sFullchainPem != NULL && grant.sKeyPem != NULL;
		ok = !xrtAcmeCleanupPending(0u, &pending) && pending == 1u && ok;
		xrtAcmeGrantUnit(&grant);
		RollbackFault = 0u;
		if(RollbackPinned && !xrtNetEngineUnpin(RollbackEngine)) return false;
		ok = xrtAcmeCleanupPending(5000000u, &pending) && pending == 0u && ok;
		if(!memory_empty() || !ok) {
			fprintf(stderr, "Obtain deferred ownership success=%u fault=%u failed\n", success, fault);
			return false;
		}
		printf("  actual Obtain Done success=%u fault=%u deferred ownership: passed\n", success, fault);
	}
	}
	return true;
}

static bool deferred_ownership(void)
{
	return rollback_failures() && deferred_no_allocation() && deferred_claimed_owner() &&
		deferred_concurrent() && deferred_obtain();
}

static bool dns_ownership(void)
{
	for(unsigned scenario = 0u; scenario < 4u; scenario++) {
		xacmedns dns;
		xnetengine* engine;
		xerror* original;
		bool ok;
		RollbackFault = scenario == 1u ? 1u : scenario == 2u ? 0u : 2u;
		RollbackEngine = NULL; RollbackPinned = false;
		ForceStartFailure = scenario == 3u;
		if(scenario == 2u) {
			engine = create_engine();
			if(engine == NULL || !xacmeDnsInit(&dns, engine) || !xrtNetEnginePin(engine)) return false;
		} else {
			bool created = xacmeDnsInit(&dns, NULL);
			ForceStartFailure = false;
			if(created == (scenario == 3u)) return false;
			engine = dns.pEngine;
		}
		xrtSetErrorInfo(XERR_PROTOCOL, "test", 2, "original operation error");
		original = xrtErrorRef(xrtGetError());
		ok = xacmeDnsUnit(&dns) == (scenario == 2u) && xrtGetError() == original;
		if(scenario != 2u) ok = ok && dns.pEngine == engine && dns.bEngineOwned;
		else ok = ok && dns.pEngine == NULL;
		RollbackFault = 0u;
		if((RollbackPinned || scenario == 2u) && !xrtNetEngineUnpin(engine)) return false;
		if(scenario != 2u) ok = xacmeDnsUnit(&dns) && ok;
		else if(!xrtNetEngineDestroy(engine)) return false;
		ok = xacmeDnsUnit(&dns) && dns.pEngine == NULL && ok;
		xrtErrorFree(original);
		if(!memory_empty() || !ok) return false;
	}
	puts("  DNS private/borrowed ownership and partial-start cleanup: passed");
	return true;
}

static bool dns_query_lifecycle(uint16 port)
{
	static const char* names[] = { "success.test", "nx.test", "bad.test", "timeout.test",
		"wrong-id.test", "success.test", "success.test", "bad.test" };
	for(unsigned scenario = 0u; scenario < 8u; scenario++) {
		xacmedns dns;
		xnetengine* engine = create_engine();
		xnetenginestats stats;
		char records[4][XACME_TXT_RECORD_MAX];
		size_t count = SIZE_MAX;
		bool result, ok;
		double deadline;
		xerror* original;
		if(engine == NULL || !xacmeDnsInit(&dns, engine)) return false;
		DnsSendFailure = scenario == 5u; DnsReceiveFailure = scenario == 6u;
		xrtClearError();
		if(scenario == 7u) xrtSetErrorInfo(XERR_IO, "test.dns.stale", 92, "unrelated previous error");
		result = xacmeDnsTxtQuery(&dns, "127.0.0.1", port, names[scenario], records, 4u, &count);
		DnsSendFailure = false; DnsReceiveFailure = false;
		ok = result == (scenario == 0u || scenario == 1u || scenario == 4u) &&
			count == (scenario == 0u || scenario == 4u ? 1u : 0u);
		if(scenario == 0u || scenario == 4u) ok = ok && strcmp(records[0], "probe") == 0;
		if(!result) ok = ok && xrtGetError() != NULL &&
			strcmp(xrtErrorDomain(xrtGetError()), "xrt.acme.dns.txt") == 0;
		if(scenario == 2u || scenario == 7u) ok = ok &&
			xrtErrorKind(xrtGetError()) == XERR_PROTOCOL &&
			xrtErrorCode(xrtGetError()) == XACME_TXT_ERROR_PROTOCOL;
		original = xrtErrorRef(xrtGetError());
		/* UDP Close/Abort is asynchronous; wait for the borrowed engine's live objects. */
		deadline = __xrtWaitAfter(5000000u);
		do {
			if(!xrtNetEngineStats(engine, &stats)) return false;
			if(stats.LiveObjects == 0u) break;
			xrtSleep(1u);
		} while(!__xrtWaitExpired(deadline));
		if(stats.LiveObjects != 0u) {
			fprintf(stderr, "DNS query lifecycle scenario=%u live=%zu failed\n", scenario, stats.LiveObjects);
			return false;
		}
		ok = stats.LiveObjects == 0u && xacmeDnsUnit(&dns) &&
			xrtNetEngineState(engine) == XNET_ENGINE_RUNNING && xrtGetError() == original && ok;
		if(!xrtNetEngineDestroy(engine)) return false;
		xrtErrorFree(original);
		if(!memory_empty() || !ok) {
			fprintf(stderr, "DNS query lifecycle scenario=%u live=%zu failed\n", scenario, stats.LiveObjects);
			return false;
		}
		printf("  DNS query lifecycle scenario=%u, zero live UDP objects: passed\n", scenario);
	}
	return true;
}

static bool constructor_faults(void)
{
	for(unsigned kind = 0u; kind < 5u; kind++) {
		bool complete = false;
		for(unsigned limit = 0u; limit < 512u; limit++) {
			xacmednsprovider provider = { 0 };
			bool created, triggered;
			xrtClearError();
			if(!xrtMemDebugFailAfter(limit)) return false;
			created = provider_create(kind, NULL, &provider);
			triggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if(created) {
				provider_http(kind, &provider)->uTimeoutUs = 5000000u;
				switch(kind) {
				case 0: xrtAcmeDnsAliProviderUnit(&provider); break;
				case 1: xrtAcmeDnsCfProviderUnit(&provider); break;
				case 2: xrtAcmeDnsTencentProviderUnit(&provider); break;
				case 3: xrtAcmeDnsAwsProviderUnit(&provider); break;
				case 4: xrtAcmeDnsHuaweiProviderUnit(&provider); break;
				}
			}
			if(provider.pContext != NULL || !memory_empty()) {
				fprintf(stderr, "provider %u constructor fault %u retained resources\n", kind, limit); return false;
			}
			if(!triggered) {
				complete = created && limit > 0u;
				printf("  provider %u constructor OOM sweep: %u positions\n", kind, limit);
				break;
			}
		}
		if(!complete) return false;
	}
	for(unsigned limit = 0u; limit < 512u; limit++) {
		xacmeaccountconfig account;
		xacmeclientconfig config;
		xacmeclient* client;
		bool triggered;
		xrtAcmeAccountConfigInit(&account);
		xrtAcmeClientConfigInit(&config);
		account.sDirectoryUrl = "https://localhost:9/directory";
		account.sAccountKeyPem = "invalid account private key";
		config.pAccount = &account; config.sCaPem = TestCa; config.uTimeoutUs = 1u;
		xrtClearError();
		if(!xrtMemDebugFailAfter(limit)) return false;
		client = xrtAcmeClientCreate(&config);
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if(client != NULL) { xrtAcmeClientDestroy(client); return false; }
		if(!memory_empty()) { fprintf(stderr, "client constructor fault %u retained resources\n", limit); return false; }
		if(!triggered) {
			printf("  failed public client constructor OOM sweep: %u positions\n", limit);
			return limit > 0u;
		}
	}
	return false;
}

int main(int argc, char** argv)
{
	FILE* file;
	char* ca;
	long size;
	bool ok;
	if((argc != 2 && argc != 3 && argc != 4) || !xrtMemDebugEnable(true)) return 2;
	file = fopen(argv[1], "rb");
	if(file == NULL) return 2;
	if(fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 || size > 65536 ||
		fseek(file, 0, SEEK_SET) != 0) { fclose(file); return 2; }
	ca = (char*)malloc((size_t)size + 1u);
	if(ca == NULL) { fclose(file); return 2; }
	if(fread(ca, 1u, (size_t)size, file) != (size_t)size) { free(ca); fclose(file); return 2; }
	ca[size] = 0; fclose(file); TestCa = ca;
	if(argc == 4) ok = strcmp(argv[2], "dns") == 0 && dns_query_lifecycle((uint16)strtoul(argv[3], NULL, 10));
	else if(argc == 3) {
		if(strcmp(argv[2], "deferred") == 0) ok = deferred_ownership();
		else if(strcmp(argv[2], "claimed") == 0) ok = deferred_claimed_owner();
		else if(strcmp(argv[2], "concurrent") == 0) ok = deferred_concurrent();
		else if(strcmp(argv[2], "obtain") == 0) ok = deferred_obtain();
		else if(strcmp(argv[2], "dns-ownership") == 0) ok = dns_ownership();
		else ok = false;
	} else ok = provider_ownership() && provider_callback_guards() && client_ownership() && failed_client_create() &&
		constructor_faults() && deferred_ownership() && dns_ownership();
	free(ca);
	return ok ? 0 : 1;
}
