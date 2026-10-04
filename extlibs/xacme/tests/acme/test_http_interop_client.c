/* Independent wire tests build only ACME HTTP and its actual dependency closure. */
#define XACME_MODULE_ACME_HTTP
#include <xacme/features.h>
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include <xrt.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool ObserveTransport;
static xtlsstream* ObservedTls;
static bool InjectHeader;
static bool HeaderFaultFired;
static xerror* HeaderFaultError;
static size_t HeaderFaultSize = sizeof("/issued");
static bool InjectRetirementError;

static xnetretireresult retire_engine(xnetengine* engine)
{
	if(InjectRetirementError) {
		InjectRetirementError = false;
		xrtSetErrorInfo(XERR_STATE, "test", 1, "injected retirement error");
		return XNET_RETIRE_ERROR;
	}
	return xrtNetEngineTryDestroy(engine);
}

static xtlsstream* observe_tls_ref(xtlsstream* stream)
{
	xtlsstream* result = xrtTlsStreamRef(stream);
	if(ObserveTransport && result != NULL)
		ObservedTls = xrtTlsStreamRef(result);
	return result;
}

static ptr header_fault_malloc(size_t size)
{
	ptr result;
	if(!InjectHeader || size != HeaderFaultSize) return xrtMalloc(size);
	InjectHeader = false;
	HeaderFaultFired = true;
	xrtMemDebugFailAfter(0);
	result = xrtMalloc(size);
	if(result == NULL) HeaderFaultError = xrtErrorRef(xrtGetError());
	xrtMemDebugFailAfter(-1);
	return result;
}

#define xrtTlsStreamRef observe_tls_ref
#define xrtNetEngineTryDestroy retire_engine
#undef xrtMalloc
#define xrtMalloc header_fault_malloc
#include "../../src/acme/xacme_http.c"
#undef xrtMalloc
#undef xrtTlsStreamRef
#undef xrtNetEngineTryDestroy

static char* read_ca(const char* path)
{
	FILE* file = fopen(path, "rb");
	char* ca;
	long size;
	if(file == NULL) return NULL;
	if(fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) <= 0 ||
		size > 1024 * 1024 || fseek(file, 0, SEEK_SET) != 0) {
		fclose(file); return NULL;
	}
	ca = (char*)malloc((size_t)size + 1u);
	if(ca == NULL) { fclose(file); return NULL; }
	if(fread(ca, 1, (size_t)size, file) != (size_t)size) {
		free(ca); fclose(file); return NULL;
	}
	ca[size] = 0;
	fclose(file);
	return ca;
}

static bool response_empty(const xacmehttpresponse* r)
{
	return r->iStatus == 0 && r->iBodySize == 0 && r->sBody == NULL &&
		r->sLocation == NULL && r->sReplayNonce == NULL && r->sRetryAfter == NULL &&
		r->sLink == NULL && r->sContentType == NULL;
}

static bool report_live(const xmemdebugallocation* allocation, ptr data)
{
	(void)data;
	fprintf(stderr, "ACME live allocation: size=%zu at=%s:%u\n", allocation->Size,
		allocation->File, (unsigned)allocation->Line);
	return true;
}

static bool init_failure(void)
{
	xacmehttp http;
	xmemdebugsnapshot memory;
	bool ok = !xacmeHttpInit(&http, NULL, "invalid CA", 1u);
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	ok = ok && http.pEngine == NULL && !http.bEngineOwned &&
		http.pResolver == NULL && http.pVerifier == NULL &&
		memory.LiveCount == 0u && memory.LiveBytes == 0u;
	if(!ok) fprintf(stderr, "failed initialization retained resources: live=%zu bytes=%zu engine=%p\n",
		memory.LiveCount, memory.LiveBytes, (void*)http.pEngine);
	/* Keep the failing probe itself from orphaning its retained engine. */
	http.uTimeoutUs = 5000000u;
	xacmeHttpUnit(&http);
	xrtClearError();
	return ok;
}

static bool after_cleanup(const char* url, const char* ca)
{
	xacmehttp http;
	xacmehttpresponse response;
	xnetengine* engine;
	bool ok;
	if(!xacmeHttpInit(&http, NULL, ca, 20000u)) return false;
	engine = http.pEngine;
	if(!xrtNetEnginePin(engine)) return false;
	xacmeHttpUnit(&http);
	xrtClearError();
	memset(&response, 0xa5, sizeof(response));
	ok = !xacmeHttpExchangeOnceV(&http, "GET", url, NULL,
		(xstrview){ NULL, 0u }, NULL, 0u, &response) && response_empty(&response) &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
		xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_ARGUMENT;
	if(!ok) fprintf(stderr, "request after cleanup: status=%u code=%d error=%s\n",
		(unsigned)response.iStatus, xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
	xacmeHttpResponseUnit(&response);
	xrtNetEngineUnpin(engine);
	http.uTimeoutUs = 5000000u;
	xacmeHttpUnit(&http);
	xrtClearError();
	return ok;
}

static bool memory_empty(void)
{
	xmemdebugsnapshot memory;
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	if(memory.LiveCount != 0u || memory.LiveBytes != 0u ||
		memory.InvalidFreeCount != 0u || memory.DoubleFreeCount != 0u) {
		fprintf(stderr, "lifecycle cleanup: live=%zu bytes=%zu invalid=%llu double=%llu\n",
			memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
			(unsigned long long)memory.DoubleFreeCount);
		xrtMemDebugVisitLive(report_live, NULL);
		return false;
	}
	return true;
}

static bool lifecycle(const char* url, const char* ca)
{
	static const char* labels[] = {
		"retirement timeout and retry", "retirement preserves original error",
		"retirement error and retry", "borrowed engine remains running"
	};
	if(!xacmeHttpUnit(NULL)) return false;
	for(unsigned scenario = 0u; scenario < 4u; scenario++) {
		xacmehttp http;
		xnetengine *engine, *borrowed = NULL;
		xerror* previous;
		bool pinned, ready, ok;
		xrtClearError();
		if(scenario == 3u) {
			xnetengineconfig config;
			xrtNetEngineConfigInit(&config);
			borrowed = xrtNetEngineCreate(&config);
			if(borrowed == NULL || !xrtNetEngineStart(borrowed)) return false;
		}
		if(!xacmeHttpInit(&http, borrowed, ca, 20000u)) return false;
		engine = http.pEngine;
		pinned = scenario != 2u;
		if(pinned && !xrtNetEnginePin(engine)) return false;
		if(scenario == 1u || scenario == 3u)
			xrtSetErrorInfo(XERR_PROTOCOL, "test", 2, "original operation error");
		previous = xrtErrorRef(xrtGetError());
		InjectRetirementError = scenario == 2u;
		ready = xacmeHttpUnit(&http);
		ok = ready == (scenario == 3u) && http.pResolver == NULL && http.pVerifier == NULL &&
			(previous == NULL || xrtGetError() == previous);
		if(scenario < 3u) {
			ok = ok && http.pEngine == engine && http.bEngineOwned &&
				xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			if(scenario == 0u) ok = ok && xrtErrorKind(xrtGetError()) == XERR_TIMEOUT &&
				xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_TIMEOUT;
			if(scenario == 2u) ok = ok && strcmp(xrtErrorMessage(xrtGetError()),
				"injected retirement error") == 0;
		} else ok = ok && http.pEngine == NULL && !http.bEngineOwned &&
			xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
		xrtErrorFree(previous);
		{
			xacmehttpresponse response;
			memset(&response, 0xa5, sizeof(response));
			ok = !xacmeHttpExchange(&http, "GET", url, NULL,
				(xstrview){ NULL, 0u }, &response) && ok;
			ok = ok && response_empty(&response) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
				xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_ARGUMENT;
			xacmeHttpResponseUnit(&response);
		}
		xrtClearError();
		if(pinned && !xrtNetEngineUnpin(engine)) return false;
		http.uTimeoutUs = 5000000u;
		ok = xacmeHttpUnit(&http) && ok;
		ok = xacmeHttpUnit(&http) && ok;
		if(borrowed != NULL && !xrtNetEngineDestroy(borrowed)) return false;
		if(!memory_empty() || !ok) { fprintf(stderr, "%s: failed\n", labels[scenario]); return false; }
		printf("  %s: passed\n", labels[scenario]);
	}
	if(!init_failure() || !memory_empty()) return false;
	puts("  failed initialization with a 1us deadline: passed");
	for(unsigned limit = 0u; limit < 512u; limit++) {
		xacmehttp http;
		bool created, triggered, clean;
		xrtClearError();
		if(!xrtMemDebugFailAfter(limit)) return false;
		created = xacmeHttpInit(&http, NULL, ca, 1u);
		triggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		/* A failing constructor must already have released its complete dependency graph. */
		clean = created || (http.pEngine == NULL && !http.bEngineOwned &&
			http.pResolver == NULL && http.pVerifier == NULL);
		http.uTimeoutUs = 5000000u;
		clean = xacmeHttpUnit(&http) && clean;
		if(!memory_empty() || !clean) {
			fprintf(stderr, "constructor allocation fault %u: failed\n", limit); return false;
		}
		if(!triggered) {
			if(!created || limit == 0u) return false;
			printf("  constructor allocation failures: passed (%u positions)\n", limit);
			return true;
		}
	}
	return false;
}

static bool url_vectors(void)
{
	static const struct {
		const char *url, *host, *target, *verify;
		uint16 port;
		bool tls, ip;
	} vectors[] = {
		{ "http://example.test", "example.test", "/", "example.test", 80u, false, false },
		{ "HTTPS://example.test:00443?value=%23#ignored", "example.test", "/?value=%23", "example.test", 443u, true, false },
		{ "hTtP://example.test:1/../a?x=/b%2Fc#frag", "example.test", "/../a?x=/b%2Fc", "example.test", 1u, false, false },
		{ "https://127.0.0.1:65535/token?", "127.0.0.1", "/token?", "127.0.0.1", 65535u, true, true },
		{ "https://[::ffff:127.0.0.1]:65535/token", "[::ffff:127.0.0.1]", "/token", "::ffff:127.0.0.1", 65535u, true, true },
		{ "https://localhost./", "localhost.", "/", "localhost", 443u, true, false },
		{ "http://example.test#ignored?foo", "example.test", "/", "example.test", 80u, false, false },
		{ "http://example.test/#", "example.test", "/", "example.test", 80u, false, false },
		{ "https://[::1]/#AZaz09-._~!$&'()*+,;=:@/?%00%23%FF", "[::1]", "/", "::1", 443u, true, true },
		{ "https://0xdead.example/token", "0xdead.example", "/token", "0xdead.example", 443u, true, false },
		{ "https://a.0x1./token", "a.0x1.", "/token", "a.0x1", 443u, true, false },
		{ "https://0xg.example/token", "0xg.example", "/token", "0xg.example", 443u, true, false },
	};
	xacmeurl url;
	char tail[1025], text[5300];
	size_t i;
	bool ok = true;
	xmemdebugsnapshot memory;
	for(i = 0u; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
		xtlsclientconfig tls;
		xtlsdialconfig dial;
		if(!xacmeUrlParse(vectors[i].url, &url)) { ok = false; break; }
		xrtTlsClientConfigInit(&tls);
		xrtTlsDialConfigInit(&dial);
		xacmeUrlTlsNames(&url, &tls, &dial);
		ok = ok && strcmp(url.sHost, vectors[i].host) == 0 &&
			strcmp(url.sPath, vectors[i].target) == 0 && url.iPort == vectors[i].port &&
			url.bTls == vectors[i].tls && url.bIpLiteral == vectors[i].ip &&
			tls.VerifyName.Size == strlen(vectors[i].verify) &&
			memcmp(tls.VerifyName.Data, vectors[i].verify, tls.VerifyName.Size) == 0 &&
			!dial.ServerNameFromHost &&
			(vectors[i].ip ? tls.ServerName.Size == 0u :
			 tls.ServerName.Size == tls.VerifyName.Size && tls.ServerName.Data == tls.VerifyName.Data);
	}
	tail[0] = '/'; memset(tail + 1, 'x', 1022u); tail[1023] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = ok && xacmeUrlParse(text, &url) && strlen(url.sPath) == 1023u;
	tail[1023] = 'x'; tail[1024] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = ok && !xacmeUrlParse(text, &url);
	/* Fragments share query grammar but never consume request-target capacity. */
	strcpy(text, "https://localhost/#");
	memset(text + strlen(text), 'f', 4096u);
	text[strlen("https://localhost/#") + 4096u] = 0;
	ok = ok && xacmeUrlParse(text, &url) && strcmp(url.sPath, "/") == 0;
	tail[0] = '/'; memset(tail + 1, 'x', 1022u); tail[1023] = 0;
	snprintf(text, sizeof(text), "https://localhost%s#fragment?value=%%23", tail);
	ok = ok && xacmeUrlParse(text, &url) && strlen(url.sPath) == 1023u;
	/* Exhaust every non-NUL octet, independently specifying the ASCII grammar. */
	for(i = 1u; i <= 255u; i++) {
		bool allowed = i < 128u &&
			strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~!$&'()*+,;=:@/?", (int)i) != NULL;
		snprintf(text, sizeof(text), "https://localhost/#x%c", (int)i);
		ok = (xacmeUrlParse(text, &url) == allowed) && ok;
	}
	tail[0] = '?'; tail[1022] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = ok && xacmeUrlParse(text, &url) && strlen(url.sPath) == 1023u &&
		url.sPath[0] == '/' && url.sPath[1] == '?';
	tail[1022] = 'x'; tail[1023] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = ok && !xacmeUrlParse(text, &url);
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	ok = ok && memory.LiveCount == 0u && memory.LiveBytes == 0u;
	if(ok) puts("  URL ports, TLS names, fragment grammar and 1023-byte target boundaries: passed");
	else fprintf(stderr, "URL positive/boundary vectors failed\n");
	return ok;
}

static bool link_bounds(void)
{
	xhttpfield fields[] = {
		{ XRT_STR_LITERAL("Link"), { "", SIZE_MAX } },
		{ XRT_STR_LITERAL("link"), XRT_STR_LITERAL("b") }
	};
	xhttp1head head;
	str value = (str)"sentinel";
	bool ok;
	memset(&head, 0, sizeof(head));
	head.Fields = fields; head.FieldCount = 2u;
	ok = !xacmeHeaderTakeLinks(&head, &value) && value == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_PROTOCOL;
	xrtClearError();
	fields[0].Value.Size = SIZE_MAX - 1u;
	ok = !xacmeHeaderTakeLinks(&head, &value) && value == NULL && ok;
	xrtClearError();
	fields[0].Value.Size = SIZE_MAX - 3u;
	ok = !xacmeHeaderTakeLinks(&head, &value) && value == NULL && ok;
	xrtClearError();
	head.FieldCount = 0u;
	ok = xacmeHeaderTakeLinks(&head, &value) && value == NULL && ok;
	head.FieldCount = 2u; fields[0].Value.Size = 0u;
	ok = xacmeHeaderTakeLinks(&head, &value) && value != NULL && strcmp(value, ", b") == 0 && ok;
	xrtFree(value);
	if(!memory_empty()) ok = false;
	if(ok) puts("  Link aggregate size overflow, absent and empty fields: passed");
	return ok;
}

int main(int argc, char** argv)
{
	xacmehttp http;
	xacmehttpresponse response;
	xmemdebugsnapshot before, memory;
	char *ca, *upload = NULL;
	uint64 timeout_us = 3000000;
	bool bounded, large_body, send_failure, head_failure, body_failure;
	bool headers, header_oom, link_fields, expected, ok, result;
	bool request_max, url_failure;
	xacmehttpheader extra[94];
	char names[94][24];
	const xerror* error;
	xerror* preserved;
	if(argc < 4 || argc > 5) return 2;
	if(strcmp(argv[3], "init-failure") == 0) {
		if(!xrtMemDebugEnable(true)) return 3;
		return init_failure() ? 0 : 1;
	}
	if(strcmp(argv[3], "after-cleanup") == 0) {
		if(!xrtMemDebugEnable(true)) return 3;
		ca = read_ca(argv[2]);
		if(ca == NULL) return 3;
		result = after_cleanup(argv[1], ca);
		free(ca);
		return result ? 0 : 1;
	}
	if(strcmp(argv[3], "lifecycle") == 0) {
		if(!xrtMemDebugEnable(true)) return 3;
		ca = read_ca(argv[2]);
		if(ca == NULL) return 3;
		result = lifecycle(argv[1], ca);
		free(ca);
		return result ? 0 : 1;
	}
	if(strcmp(argv[3], "url-vectors") == 0) {
		if(!xrtMemDebugEnable(true)) return 3;
		return url_vectors() ? 0 : 1;
	}
	if(strcmp(argv[3], "link-bounds") == 0) {
		if(!xrtMemDebugEnable(true)) return 3;
		return link_bounds() ? 0 : 1;
	}
	bounded = strcmp(argv[3], "chunked") == 0;
	large_body = strcmp(argv[3], "large-body") == 0;
	send_failure = strcmp(argv[3], "send-failure") == 0;
	head_failure = strcmp(argv[3], "head-failure") == 0;
	body_failure = strcmp(argv[3], "body-failure") == 0;
	headers = strcmp(argv[3], "headers") == 0;
	link_fields = strcmp(argv[3], "link-fields") == 0;
	header_oom = strcmp(argv[3], "header-oom") == 0;
	if(strcmp(argv[3], "header-oom-nonce") == 0) {
		header_oom = true; HeaderFaultSize = sizeof("nonce");
	} else if(strcmp(argv[3], "header-oom-retry") == 0) {
		header_oom = true; HeaderFaultSize = sizeof("7");
	} else if(strcmp(argv[3], "header-oom-link") == 0) {
		header_oom = true; HeaderFaultSize = sizeof("</issuer>; rel=\"up\"");
	} else if(strcmp(argv[3], "header-oom-type") == 0) {
		header_oom = true; HeaderFaultSize = sizeof("application/json");
	} else if(strcmp(argv[3], "header-oom-link-fields") == 0) {
		header_oom = true; HeaderFaultSize = sizeof("</issuer>; rel=\"up\", </alternate>; rel=\"alternate\"");
	}
	request_max = strcmp(argv[3], "request-max") == 0;
	url_failure = strcmp(argv[3], "url-failure") == 0;
	expected = strcmp(argv[3], "success") == 0 || bounded || large_body || headers || link_fields || request_max;
	if(!expected && !send_failure && !head_failure && !body_failure && !header_oom && !url_failure &&
		strcmp(argv[3], "failure") != 0) return 2;
	if(argc == 5) {
		char* end;
		unsigned long long parsed;
		errno = 0;
		parsed = strtoull(argv[4], &end, 10);
		if(errno != 0 || argv[4][0] == '-' || end == argv[4] || *end != 0 || parsed == 0)
			return 2;
		timeout_us = (uint64)parsed;
	}
	if(!xrtMemDebugEnable(true)) return 3;
	ca = read_ca(argv[2]);
	if(ca == NULL) return 3;
	ok = xacmeHttpInit(&http, NULL, ca, timeout_us);
	free(ca);
	if(!ok) return 4;
	xrtMemDebugSnapshot(&before);
	if(send_failure) {
		upload = (char*)malloc(8u * 1024u * 1024u);
		if(upload == NULL) { xacmeHttpUnit(&http); return 5; }
		memset(upload, 'x', 8u * 1024u * 1024u);
	}
	ObserveTransport = send_failure;
	InjectHeader = header_oom;
	if(request_max) {
		size_t i;
		for(i = 0u; i < 94u; i++) {
			snprintf(names[i], sizeof(names[i]), "X-Extra-%u", (unsigned)i);
			extra[i] = (xacmehttpheader){ names[i], "v" };
		}
	}
	xrtClearError();
	ok = xacmeHttpExchangeOnceV(&http, send_failure || request_max ? "POST" : "GET", argv[1],
		send_failure || request_max ? "text/plain" : NULL,
		request_max ? XRT_STR_LITERAL("request") :
		(xstrview){ upload, send_failure ? 8u * 1024u * 1024u : 0u },
		request_max ? extra : NULL, request_max ? 94u : 0u, &response);
	free(upload);
	error = xrtGetError();
	if(expected) {
		size_t size = large_body ? 4u * 1024u * 1024u : 2048u;
		result = ok && response.iStatus == 200 && response.sBody != NULL &&
			(bounded || large_body ? response.iBodySize == size &&
			 strlen(response.sBody) == size && strspn(response.sBody, "x") == size :
			 response.iBodySize == 11u && strcmp(response.sBody, "{\"ok\":true}") == 0);
	} else {
		result = !ok && response_empty(&response) && error != NULL;
	}
	if(headers || bounded || header_oom) {
		if(header_oom) result = result && HeaderFaultFired && error == HeaderFaultError &&
			xrtErrorKind(error) == XERR_MEMORY;
		else result = result && response.sLocation != NULL && strcmp(response.sLocation, "/issued") == 0 &&
			response.sReplayNonce != NULL && strcmp(response.sReplayNonce, "nonce") == 0 &&
			response.sRetryAfter != NULL && strcmp(response.sRetryAfter, "7") == 0 &&
			response.sLink != NULL && strcmp(response.sLink, "</issuer>; rel=\"up\"") == 0 &&
			response.sContentType != NULL && strcmp(response.sContentType, "application/json") == 0;
	}
	if(link_fields) result = result && response.sLink != NULL &&
		strcmp(response.sLink, "</issuer>; rel=\"up\", </alternate>; rel=\"alternate\"") == 0;
	if(head_failure || body_failure) {
		const char* wanted = head_failure ? "acme http response head invalid" :
			"acme http response body invalid";
		result = result && xrtErrorKind(error) == XERR_PROTOCOL &&
			xrtErrorCode(error) == XACME_HTTP_ERROR_PROTOCOL &&
			strcmp(xrtErrorMessage(error), wanted) == 0;
	}
	if(url_failure) result = result && xrtErrorKind(error) == XERR_ARGUMENT &&
		xrtErrorCode(error) == XACME_HTTP_ERROR_URL;
	if(send_failure) result = result && ObservedTls != NULL &&
		xrtErrorCode(error) == XACME_HTTP_ERROR_UNCERTAIN &&
		xrtErrorCode(xrtErrorCause(error)) == XACME_HTTP_ERROR_SEND;
	if(bounded) {
		xrtMemDebugSnapshot(&memory);
		fprintf(stderr, "TLS chunked memory: before=%zu peak=%zu\n", before.PeakBytes, memory.PeakBytes);
		result = result && memory.PeakBytes >= before.PeakBytes &&
			memory.PeakBytes - before.PeakBytes <= 4u * 1024u * 1024u;
	}
	if(!result) fprintf(stderr, "ACME HTTP interop: ok=%d status=%u size=%zu kind=%d code=%d error=%s\n",
		(int)ok, (unsigned)response.iStatus, response.iBodySize, xrtErrorKind(error),
		xrtErrorCode(error), error != NULL ? xrtErrorMessage(error) : "none");
	if(send_failure) {
		puts("RETURNED"); fflush(stdout);
		if(getchar() != '\n') result = false;
	}
	xacmeHttpResponseUnit(&response);
	xrtTlsStreamDestroy(ObservedTls);
	preserved = xrtErrorRef(xrtGetError());
	xacmeHttpUnit(&http);
	if(preserved != NULL && xrtGetError() != preserved) {
		fprintf(stderr, "ACME HTTP cleanup replaced the original error\n");
		result = false;
	}
	xrtErrorFree(preserved);
	xrtErrorFree(HeaderFaultError);
	{
		xrtClearError();
		xrtMemDebugSnapshot(&memory);
		if(memory.LiveCount != 0u || memory.LiveBytes != 0u ||
			memory.InvalidFreeCount != 0u || memory.DoubleFreeCount != 0u) {
			fprintf(stderr, "ACME HTTP cleanup: live=%zu bytes=%zu invalid=%llu double=%llu\n",
				memory.LiveCount, memory.LiveBytes, (unsigned long long)memory.InvalidFreeCount,
				(unsigned long long)memory.DoubleFreeCount);
			xrtMemDebugVisitLive(report_live, NULL);
			result = false;
		}
	}
	return result ? 0 : 1;
}
