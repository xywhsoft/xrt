#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"



/* 测试分配器可以让下一次堆分配确定性失败。 */
typedef struct test_http_resume_allocator {
	bool Armed;
	bool Failed;
} test_http_resume_allocator;



/* 转发普通分配，并在武装后拒绝一次分配。 */
static ptr testHttpResumeAlloc(ptr pContext, size_t iSize)
{
	test_http_resume_allocator* pState =
		(test_http_resume_allocator*)pContext;

	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并共享下一次分配故障点。 */
static ptr testHttpResumeRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_resume_allocator* pState =
		(test_http_resume_allocator*)pContext;

	if ( pState->Armed && !pState->Failed ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放测试分配器产生的内存。 */
static void testHttpResumeFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 创建 SNI 为空但带有完整 TLS 1.3 PSK 的测试票据。 */
static xtlsresume* testHttpResumeCreate(uint8 iTicket)
{
	static const uint8 Secret[32] = {
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
		0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x10,
		0x21, 0x32, 0x43, 0x54, 0x65, 0x76, 0x87, 0x98,
		0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x20
	};
	static const uint8 Protocol[] = "http/1.1";
	xtlsresumeconfig Config;

	xrtTlsResumeConfigInit(&Config);
	Config.Cipher = XTLS_AES_128_GCM_SHA256;
	Config.Ticket.Data = &iTicket;
	Config.Ticket.Size = 1u;
	Config.Secret.Data = Secret;
	Config.Secret.Size = sizeof(Secret);
	Config.Protocol.Data = Protocol;
	Config.Protocol.Size = sizeof(Protocol) - 1u;
	Config.Lifetime = 60u;
	return xrtTlsResumeCreate(&Config);
}



/* 验证默认值和所有空参数都产生稳定的公开错误。 */
static void testHttpResumePublicContract(void)
{
	uint8 Storage[sizeof(xhttpresumeconfig) + 2u];
	xhttpresumeconfig Config;
	xhttpresumestats Stats;

	memset(Storage, 0xA5, sizeof(Storage));
	xrtHttpResumeConfigInit(
		(xhttpresumeconfig*)(void*)(Storage + 1u)
	);
	memcpy(&Config, Storage + 1u, sizeof(Config));
	testRequire(
		(Storage[0] == 0xA5) &&
		(Storage[sizeof(Storage) - 1u] == 0xA5) &&
		(Config.MaxEntries == XHTTP_RESUME_ENTRIES_DEFAULT) &&
		(Config.MaxEntriesPerOrigin ==
		 XHTTP_RESUME_ORIGIN_DEFAULT),
		"HTTP resume public defaults changed"
	);
	xrtClearError();
	xrtHttpResumeConfigInit(
		(xhttpresumeconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP resume wrapping config error mismatch"
	);

	xrtClearError();
	xrtHttpResumeConfigInit(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP_CLIENT_ERROR_ARGUMENT),
		"HTTP resume null config error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtHttpClientResumeClear(NULL) == 0) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP resume null clear error mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtHttpClientResumeStats(NULL, &Stats) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP resume null client stats error mismatch"
	);
	xrtClearError();
}



/*
	IP 地址不发送 SNI，缓存仍必须按 HTTP 验证 host 命中；
	缓存项 OOM 只损失优化，不能污染调用线程错误状态。
*/
static void testHttpResumeRouteAndOom(
	test_http_resume_allocator* pAllocator
)
{
	static const char Host[] = "127.0.0.1";
	char OomHost[3334];
	xhttpclient Client;
	xhttpcall Call;
	__xrt_http_resume_route Route;
	uint8 StatsStorage[sizeof(xhttpresumestats) + 2u];
	xhttpresumestats Stats;
	xtlsresume* pResume;
	xtlsresume* pTaken;

	memset(&Client, 0, sizeof(Client));
	memset(&Call, 0, sizeof(Call));
	memset(&Route, 0, sizeof(Route));
	xrtHttpResumeConfigInit(&Client.Config.Resume);
	testRequire(
		__xrtHttpResumeInit(&Client),
		"HTTP resume cache fixture init failed"
	);
	Route.Client = &Client;
	Route.Host.Data = Host;
	Route.Host.Size = sizeof(Host) - 1u;
	Route.Port = 443u;
	Call.Client = &Client;
	Call.Host = (str)Host;
	Call.Port = 443u;

	pResume = testHttpResumeCreate(1u);
	testRequire(
		pResume != NULL,
		"HTTP resume IP ticket creation failed"
	);
	__xrtHttpResumeStore(&Route, pResume);
	memset(StatsStorage, 0xA5, sizeof(StatsStorage));
	testRequire(
		xrtHttpClientResumeStats(
			&Client,
			(xhttpresumestats*)(void*)(StatsStorage + 1u)
		) &&
		(StatsStorage[0] == 0xA5) &&
		(StatsStorage[sizeof(StatsStorage) - 1u] == 0xA5),
		"HTTP resume unaligned stats storage mismatch"
	);
	memcpy(&Stats, StatsStorage + 1u, sizeof(Stats));
	testRequire(
		(Stats.Entries == 1u) &&
		(Stats.Stored == 1u),
		"HTTP resume IP route was not stored"
	);
	pTaken = __xrtHttpResumeTake(&Call);
	testRequire(
		(pTaken == pResume) &&
		xrtHttpClientResumeStats(&Client, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Hits == 1u) &&
		(Stats.Misses == 0),
		"HTTP resume IP route did not hit without SNI"
	);
	xrtTlsResumeRelease(pTaken);

	pResume = testHttpResumeCreate(2u);
	testRequire(
		pResume != NULL,
		"HTTP resume OOM ticket creation failed"
	);
	memset(OomHost, 'h', sizeof(OomHost) - 1u);
	OomHost[sizeof(OomHost) - 1u] = 0;
	Route.Host.Data = OomHost;
	Route.Host.Size = sizeof(OomHost) - 1u;
	pAllocator->Armed = true;
	pAllocator->Failed = false;
	xrtClearError();
	__xrtHttpResumeStore(&Route, pResume);
	if ( !pAllocator->Failed ||
		(xrtGetError() != NULL) ||
		!xrtHttpClientResumeStats(&Client, &Stats) ||
		(Stats.Entries != 0) ||
		(Stats.Stored != 1u) ||
		(Stats.Dropped != 1u) ) {
		fprintf(
			stderr,
			"failed=%d error=%p entries=%zu stored=%llu dropped=%llu\n",
			pAllocator->Failed ? 1 : 0,
			(const void*)xrtGetError(),
			Stats.Entries,
			(unsigned long long)Stats.Stored,
			(unsigned long long)Stats.Dropped
		);
	}
	testRequire(
		pAllocator->Failed &&
		(xrtGetError() == NULL) &&
		xrtHttpClientResumeStats(&Client, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Stored == 1u) &&
		(Stats.Dropped == 1u),
		"HTTP resume cache OOM was not fail-open"
	);

	pAllocator->Armed = false;
	Route.Host.Data = Host;
	Route.Host.Size = sizeof(Host) - 1u;
	pResume = testHttpResumeCreate(3u);
	testRequire(
		pResume != NULL,
		"HTTP resume recovery ticket creation failed"
	);
	__xrtHttpResumeStore(&Route, pResume);
	testRequire(
		(xrtHttpClientResumeClear(&Client) == 1u) &&
		xrtHttpClientResumeStats(&Client, &Stats) &&
		(Stats.Entries == 0) &&
		(Stats.Stored == 2u) &&
		(Stats.Dropped == 1u),
		"HTTP resume cache did not recover after OOM"
	);

	xrtClearError();
	testRequire(
		!xrtHttpClientResumeStats(&Client, NULL) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP resume null stats output error mismatch"
	);
	xrtClearError();
	__xrtHttpResumeUnit(&Client);
}



/* 覆盖恢复缓存公开边界、IP 路由身份和确定性 OOM。 */
int main(void)
{
	test_http_resume_allocator State;
	xallocator Allocator;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testHttpResumeAlloc;
	Allocator.Realloc = testHttpResumeRealloc;
	Allocator.Free = testHttpResumeFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP resume OOM allocator install failed"
	);

	testHttpResumePublicContract();
	testHttpResumeRouteAndOom(&State);
	printf("[PASS] HTTP client resume cache contract and OOM\n");
	return 0;
}


