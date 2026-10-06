/* 独立 Python TLS 服务端互操作探针；由 tools/test_oauth2_tls_interop.py 驱动。 */
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#define XRT_IMPLEMENTATION
#define XRT_MODULE_MEMORY_DEBUG
#include "support/runtime.h"

/* 保留独立观察引用，避免 Destroy 隐式清理掩盖传输层遗漏的 Abort。 */
static bool ObserveTransport;
static xtlsstream* ObservedTls;
static xtlsstream* observe_tls_ref(xtlsstream* stream)
{
	xtlsstream* reference = xrtTlsStreamRef(stream);
	if ( ObserveTransport && reference != NULL && ObservedTls == NULL )
		ObservedTls = xrtTlsStreamRef(reference);
	return reference;
}
static bool InjectRetirementError;
static xnetretireresult observe_engine_retire(xnetengine* engine)
{
	if ( InjectRetirementError ) {
		InjectRetirementError = false;
		xrtSetErrorInfo(XERR_STATE, "test", 1, "injected retirement error");
		return XNET_RETIRE_ERROR;
	}
	return xrtNetEngineTryDestroy(engine);
}
#define xrtTlsStreamRef observe_tls_ref
#define xrtNetEngineTryDestroy observe_engine_retire
#include "support/implementation.c"
#undef xrtTlsStreamRef
#undef xrtNetEngineTryDestroy

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static char* read_ca(const char* path)
{
	FILE* file = fopen(path, "rb");
	long size;
	char* data;
	if ( file == NULL ) return NULL;
	if ( fseek(file, 0, SEEK_END) != 0 ||
		(size = ftell(file)) <= 0 || size > 65536 ||
		fseek(file, 0, SEEK_SET) != 0 ) {
		fclose(file);
		return NULL;
	}
	data = (char*)malloc((size_t)size + 1u);
	if ( data == NULL ) {
		fclose(file);
		return NULL;
	}
	if ( fread(data, 1u, (size_t)size, file) != (size_t)size ) {
		free(data);
		fclose(file);
		return NULL;
	}
	data[size] = 0;
	fclose(file);
	return data;
}

typedef struct allocation_check {
	void* address;
	bool live;
} allocation_check;

static bool find_allocation(const xmemdebugallocation* allocation, ptr user)
{
	allocation_check* check = (allocation_check*)user;
	if ( allocation->Address == check->address ) check->live = true;
	return !check->live;
}

/* 真实 Pin 使 engine 保持 BUSY；检查失败后拥有权、重试和借用边界。 */
static bool lifecycle(const char* ca)
{
	static const char* labels[] = {
		"cleanup timeout and retry", "Unit preserves error and ownership",
		"Destroy retains busy heap handle", "retirement error and retry",
		"borrowed engine remains running"
	};
	bool result = xoauth2HttpXrtCleanup(NULL);
	for ( unsigned scenario = 0; scenario < 5u; scenario++ ) {
		xnetengine* borrowed = NULL;
		xoauth2httpxrt* http;
		xnetengine* engine;
		xerror* previous;
		bool pinned = false, ready;
		allocation_check check;
		xmemdebugsnapshot memory;
		xrtClearError();
		if ( scenario == 4u ) {
			xnetengineconfig config;
			xrtNetEngineConfigInit(&config);
			borrowed = xrtNetEngineCreate(&config);
			if ( borrowed == NULL || !xrtNetEngineStart(borrowed) ) return false;
		}
		http = xoauth2HttpXrtCreate(borrowed, ca, 20);
		if ( http == NULL ) return false;
		engine = http->pEngine;
		if ( scenario < 3u || scenario == 4u ) {
			pinned = xrtNetEnginePin(engine);
			if ( !pinned ) return false;
		}
		if ( scenario == 1u || scenario == 2u || scenario == 4u )
			xoauth2__error(XOAUTH2_ERROR_TOKEN_DENIED, "original operation error");
		previous = xrtErrorRef(xrtGetError());
		InjectRetirementError = scenario == 3u;
		if ( scenario == 1u ) {
			xoauth2HttpXrtUnit(http);
			ready = http->pEngine == NULL;
		} else if ( scenario == 2u ) {
			xoauth2HttpXrtDestroy(http);
			check = (allocation_check){ http, false };
			xrtMemDebugVisitLive(find_allocation, &check);
			/* 先证实句柄未释放，再解引用；避免错误实现触发测试自身的 UAF。 */
			if ( !check.live ) {
				fprintf(stderr, "Destroy discarded a busy heap handle\n");
				return false;
			}
			ready = http->pEngine == NULL;
		} else {
			ready = xoauth2HttpXrtCleanup(http);
		}
		result = result && ready == (scenario == 4u) &&
			(previous == NULL || xrtGetError() == previous) &&
			http->pResolver == NULL && http->pVerifier == NULL;
		if ( scenario < 4u ) {
			result = result && http->pEngine == engine && http->bEngineOwned &&
				xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
			if ( scenario == 0u ) result = result &&
				xoauth2LastError() == XOAUTH2_ERROR_NETWORK &&
				strcmp(xrtErrorMessage(xrtGetError()),
				 "http transport engine still has live objects during cleanup") == 0;
			if ( scenario == 3u ) result = result &&
				strcmp(xrtErrorMessage(xrtGetError()), "injected retirement error") == 0;
		} else {
			result = result && !http->bEngineOwned && http->pEngine == NULL &&
				xrtNetEngineState(engine) == XNET_ENGINE_RUNNING;
		}
		xrtErrorFree(previous);
		xrtClearError();
		{
			char* body = (char*)"sentinel";
			int status = -1;
			result = !xoauth2HttpXrt("GET", "https://localhost:9/token",
				NULL, NULL, &body, &status, http) && result;
			result = result && body == NULL && status == 0 &&
				xoauth2LastError() == XOAUTH2_ERROR_ARGUMENT &&
				strcmp(xrtErrorMessage(xrtGetError()),
				 "http transport is not initialized or has been cleaned up") == 0;
			xrtClearError();
		}
		if ( pinned && !xrtNetEngineUnpin(engine) ) return false;
		result = xoauth2HttpXrtCleanup(http) && result;
		result = xoauth2HttpXrtCleanup(http) && result;
		result = result && http->pEngine == NULL && !http->bEngineOwned;
		xoauth2HttpXrtDestroy(http);
		if ( borrowed != NULL && !xrtNetEngineDestroy(borrowed) ) return false;
		xrtClearError();
		xrtMemDebugSnapshot(&memory);
		result = result && memory.LiveCount == 0u && memory.LiveBytes == 0u &&
			memory.InvalidFreeCount == 0u && memory.DoubleFreeCount == 0u;
		if ( !result ) {
			fprintf(stderr, "%s: failed; live=%zu bytes=%zu\n",
				labels[scenario], memory.LiveCount, memory.LiveBytes);
			return false;
		}
		printf("  %s: passed\n", labels[scenario]);
	}
	{
		xmemdebugsnapshot memory;
		xoauth2httpxrt* failed = xoauth2HttpXrtCreate(NULL, "invalid CA", 1);
		result = result && failed == NULL &&
			xoauth2LastError() == XOAUTH2_ERROR_NETWORK;
		xoauth2HttpXrtDestroy(failed);
		xrtClearError();
		xrtMemDebugSnapshot(&memory);
		if ( !result || memory.LiveCount != 0u || memory.LiveBytes != 0u ) {
			fprintf(stderr, "failed initialization cleanup: live=%zu bytes=%zu\n",
				memory.LiveCount, memory.LiveBytes);
			return false;
		}
		puts("  failed initialization cleanup: passed");
	}
	{
		bool completed = false;
		unsigned faults = 0;
		for ( unsigned limit = 0; limit < 512u; limit++ ) {
			xmemdebugsnapshot memory;
			xoauth2httpxrt* http;
			bool triggered;
			xrtClearError();
			if ( !xrtMemDebugFailAfter(limit) ) return false;
			http = xoauth2HttpXrtCreate(NULL, ca, 1);
			triggered = xrtMemDebugFailTriggered();
			xrtMemDebugFailClear();
			if ( http != NULL ) http->uTimeoutMs = 5000u;
			xoauth2HttpXrtDestroy(http);
			if (!xoauth2HttpXrtCleanupPending(5000, NULL)) return false;
			xrtClearError();
			xrtMemDebugSnapshot(&memory);
			if ( memory.LiveCount != 0u || memory.LiveBytes != 0u ||
				memory.InvalidFreeCount != 0u || memory.DoubleFreeCount != 0u ) {
				fprintf(stderr, "constructor fault %u: live=%zu bytes=%zu\n",
					limit, memory.LiveCount, memory.LiveBytes);
				return false;
			}
			if ( triggered ) faults++;
			else {
				completed = http != NULL;
				break;
			}
		}
		if ( !completed || faults == 0u ) return false;
		printf("  constructor allocation failures: passed (%u fault positions)\n", faults);
	}
	return result;
}

static bool url_vectors(void)
{
	static const struct {
		const char *url, *host, *target, *verify;
		uint16_t port;
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
	xoauth2url url;
	char tail[1025], text[5300];
	bool ok = true;
	xmemdebugsnapshot memory;
	for ( size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++ ) {
		xtlsclientconfig tls;
		xtlsdialconfig dial;
		if ( !url_parse(vectors[i].url, &url) ) { ok = false; break; }
		xrtTlsClientConfigInit(&tls);
		xrtTlsDialConfigInit(&dial);
		url_tls_names(&url, &tls, &dial);
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
	ok = url_parse(text, &url) && strlen(url.sPath) == 1023u && ok;
	tail[1023] = 'x'; tail[1024] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = !url_parse(text, &url) && ok;
	tail[0] = '?'; tail[1022] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = url_parse(text, &url) && strlen(url.sPath) == 1023u &&
		url.sPath[0] == '/' && url.sPath[1] == '?' && ok;
	tail[1022] = 'x'; tail[1023] = 0;
	snprintf(text, sizeof(text), "https://localhost%s", tail);
	ok = !url_parse(text, &url) && ok;
	strcpy(text, "https://localhost/#");
	memset(text + strlen(text), 'f', 4096u);
	text[strlen("https://localhost/#") + 4096u] = 0;
	ok = url_parse(text, &url) && strcmp(url.sPath, "/") == 0 && ok;
	tail[0] = '/'; memset(tail + 1, 'x', 1022u); tail[1023] = 0;
	snprintf(text, sizeof(text), "https://localhost%s#fragment?value=%%23", tail);
	ok = url_parse(text, &url) && strlen(url.sPath) == 1023u && ok;
	for ( unsigned i = 1u; i <= 255u; i++ ) {
		bool allowed = i < 128u &&
			strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~!$&'()*+,;=:@/?", (int)i) != NULL;
		snprintf(text, sizeof(text), "https://localhost/#x%c", (int)i);
		ok = (url_parse(text, &url) == allowed) && ok;
	}
	xrtClearError();
	xrtMemDebugSnapshot(&memory);
	ok = ok && memory.LiveCount == 0u && memory.LiveBytes == 0u;
	if ( ok ) puts("  URL ports, TLS names, fragment grammar and 1023-byte target boundaries: passed");
	else fprintf(stderr, "URL positive/boundary vectors failed\n");
	return ok;
}

int main(int argc, char** argv)
{
	char* ca;
	char* body = NULL;
	char* upload = NULL;
	xoauth2httpxrt* http;
	int status = -1;
	bool ok, expected;
	int result;
	uint64_t timeout_us = 5000000u;
	bool bounded, large_body, send_failure, head_failure, body_failure, url_failure;
	xmemdebugsnapshot before, memory;
	if ( (argc != 4 && argc != 5) ||
		(strcmp(argv[3], "success") != 0 &&
		 strcmp(argv[3], "failure") != 0 &&
		 strcmp(argv[3], "chunked") != 0 &&
		 strcmp(argv[3], "large-body") != 0 &&
		 strcmp(argv[3], "send-failure") != 0 &&
		 strcmp(argv[3], "head-failure") != 0 &&
		 strcmp(argv[3], "body-failure") != 0 &&
		 strcmp(argv[3], "lifecycle") != 0 &&
		 strcmp(argv[3], "url-failure") != 0 &&
		 strcmp(argv[3], "url-vectors") != 0) ) return 2;
	if ( strcmp(argv[3], "url-vectors") == 0 ) {
		if ( !xrtMemDebugEnable(true) ) return 3;
		return url_vectors() ? 0 : 1;
	}
	char url_input[8192];
	const char* url = argc > 1 ? argv[1] : NULL;

	if ( strcmp(url, "--url-stdin") == 0 ) {
		size_t length = fread(url_input, 1u, sizeof(url_input) - 1u, stdin);
		if ( ferror(stdin) || !feof(stdin) ) return 2;
		url_input[length] = 0;
		url = url_input;
	}
	if ( argc == 5 ) {
		char* end = NULL;
		unsigned long long parsed;
		errno = 0;
		parsed = strtoull(argv[4], &end, 10);
		if ( errno != 0 || argv[4][0] == '-' || end == argv[4] ||
			*end != '\0' || parsed == 0 ) return 2;
		timeout_us = (uint64_t)parsed;
	}
	bounded = strcmp(argv[3], "chunked") == 0;
	large_body = strcmp(argv[3], "large-body") == 0;
	send_failure = strcmp(argv[3], "send-failure") == 0;
	head_failure = strcmp(argv[3], "head-failure") == 0;
	body_failure = strcmp(argv[3], "body-failure") == 0;
	url_failure = strcmp(argv[3], "url-failure") == 0;
	expected = strcmp(argv[3], "success") == 0 || bounded || large_body;
	ObserveTransport = send_failure;
	if ( !xrtMemDebugEnable(true) ) return 3;
	ca = read_ca(argv[2]);
	if ( ca == NULL ) return 3;
	if ( strcmp(argv[3], "lifecycle") == 0 ) {
		result = lifecycle(ca);
		free(ca);
		return result ? 0 : 1;
	}
	http = xoauth2HttpXrtCreate(NULL, ca, timeout_us);
	free(ca);
	if ( http == NULL ) return 4;
	xrtMemDebugSnapshot(&before);
	if ( send_failure ) {
		upload = (char*)malloc(8u * 1024u * 1024u + 1u);
		if ( upload == NULL ) { xoauth2HttpXrtDestroy(http); return 5; }
		memset(upload, 'x', 8u * 1024u * 1024u);
		upload[8u * 1024u * 1024u] = 0;
	}
	xrtClearError();
	ok = xoauth2HttpXrt(send_failure ? "POST" : "GET", url, upload,
		NULL, &body, &status, http);
	free(upload);
	result = expected ?
		(ok && status == 200 && body != NULL &&
		 (bounded || large_body ?
		  strlen(body) == (large_body ? 1024u * 1024u : 2048u) &&
		  strspn(body, "x") == (large_body ? 1024u * 1024u : 2048u) :
		  strcmp(body, "{\"ok\":true}") == 0)) :
		(!ok && status == 0 && body == NULL &&
		 xoauth2LastError() == (url_failure ? XOAUTH2_ERROR_ARGUMENT : XOAUTH2_ERROR_NETWORK));
	if ( url_failure ) result = result && strcmp(xrtErrorMessage(xrtGetError()),
		"http transport URL is invalid or unsupported") == 0;
	if ( send_failure || head_failure || body_failure ) {
		const xerror* error = xrtGetError();
		const char* message = error != NULL ? xrtErrorMessage(error) : NULL;
		const char* wanted = send_failure ? "http transport send failed" :
			head_failure ? "http transport response head invalid" :
			"http transport response body invalid";
		result = result && message != NULL && strcmp(message, wanted) == 0;
	}
	if ( bounded ) {
		xrtMemDebugSnapshot(&memory);
		fprintf(stderr, "TLS chunked memory: before=%zu peak=%zu\n",
			before.PeakBytes, memory.PeakBytes);
		result = result && memory.PeakBytes >= before.PeakBytes &&
			memory.PeakBytes - before.PeakBytes <= 4u * 1024u * 1024u;
	}
	if ( !result ) {
		const xerror* error = xrtGetError();
		const char* message = error != NULL ? xrtErrorMessage(error) : NULL;
		fprintf(stderr, "TLS interop: ok=%d status=%d error=%s\n",
			(int)ok, status,
			message != NULL ? message : "none");
	}
	if ( send_failure ) {
		result = result && ObservedTls != NULL;
		/* 驱动在收到此行后检查对端异常关闭，再允许销毁观察引用和 HTTP。 */
		puts("RETURNED");
		fflush(stdout);
		if ( getchar() != '\n' ) result = 0;
	}
	xrtFree(body);
	xrtTlsStreamDestroy(ObservedTls);
	{
		xerror* previous = xrtErrorRef(xrtGetError());
		xoauth2HttpXrtDestroy(http);
		if (!xoauth2HttpXrtCleanupPending(5000, NULL)) result = 0;
		if ( previous != NULL && xrtGetError() != previous ) {
			fprintf(stderr, "HTTP cleanup replaced the operation error\n");
			result = 0;
		}
		xrtErrorFree(previous);
		xrtClearError();
	}
	xrtMemDebugSnapshot(&memory);
	if ( memory.LiveCount != 0 || memory.LiveBytes != 0 ||
	     memory.InvalidFreeCount != 0 || memory.DoubleFreeCount != 0 ) {
		fprintf(stderr, "HTTP cleanup: live=%zu bytes=%zu invalid=%llu double=%llu\n",
			memory.LiveCount, memory.LiveBytes,
			(unsigned long long)memory.InvalidFreeCount,
			(unsigned long long)memory.DoubleFreeCount);
		result = 0;
	}
	return result ? 0 : 1;
}
