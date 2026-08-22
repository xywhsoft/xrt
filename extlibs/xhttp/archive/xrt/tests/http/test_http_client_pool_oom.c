#include "../test.h"
#include "../../src/internal/xrt_http_client_runtime.h"
#include "../../src/internal/xrt_memory.h"



/* 只拒绝指定大小的一次分配，其他内存操作继续交给系统分配器。 */
typedef struct test_http_pool_allocator {
	size_t Target;
	bool Armed;
	bool Failed;
} test_http_pool_allocator;



/* 转发普通分配，并在命中目标大小时注入一次内存不足。 */
static ptr testHttpPoolAlloc(ptr pContext, size_t iSize)
{
	test_http_pool_allocator* pState =
		(test_http_pool_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return malloc(iSize);
}



/* 转发普通重分配，并与初始分配共享同一个故障点。 */
static ptr testHttpPoolRealloc(
	ptr pContext,
	ptr pMemory,
	size_t iSize
)
{
	test_http_pool_allocator* pState =
		(test_http_pool_allocator*)pContext;

	if ( pState->Armed && !pState->Failed &&
		(iSize == pState->Target) ) {
		pState->Failed = true;
		return NULL;
	}
	return realloc(pMemory, iSize);
}



/* 释放由测试分配器产生的内存。 */
static void testHttpPoolFree(ptr pContext, ptr pMemory)
{
	(void)pContext;
	free(pMemory);
}



/* 配置下一次精确逻辑大小的堆分配失败。 */
static void testHttpPoolArm(
	test_http_pool_allocator* pState,
	size_t iLogicalSize
)
{
	pState->Target = iLogicalSize +
		__xrtHeapHeaderSize() +
		__xrtMemDebugTailSize() +
		(XRT_HEAP_ALIGNMENT - 1u);
	pState->Armed = true;
	pState->Failed = false;
}



/* 验证公开默认值和空参数错误保持明确且可诊断。 */
static void testHttpPoolPublicContract(void)
{
	unsigned char ConfigStorage[
		sizeof(xhttpclientpoolconfig) + 2u
	];
	xhttpclientpoolconfig Config;
	xhttpclientstats Stats;
	xhttpclient Partial;

	xrtHttpClientPoolConfigInit(&Config);
	testRequire(
		(Config.MaxConnections == 0) &&
		(Config.MaxConnectionsPerOrigin == 0) &&
		(Config.MaxWaiting == 0) &&
		(Config.MaxWaitingPerOrigin == 0) &&
		(Config.MaxIdle == 128u) &&
		(Config.MaxIdlePerOrigin == 8u) &&
		(Config.IdleTimeout == UINT64_C(90000000)),
		"HTTP pool public defaults changed"
	);
	memset(ConfigStorage, 0xA5, sizeof(ConfigStorage));
	xrtHttpClientPoolConfigInit(
		(xhttpclientpoolconfig*)(void*)(ConfigStorage + 1u)
	);
	memcpy(&Config, ConfigStorage + 1u, sizeof(Config));
	testRequire(
		(Config.MaxConnections == 0) &&
		(Config.MaxConnectionsPerOrigin == 0) &&
		(Config.MaxWaiting == 0) &&
		(Config.MaxWaitingPerOrigin == 0) &&
		(Config.MaxIdle == 128u) &&
		(Config.MaxIdlePerOrigin == 8u) &&
		(Config.IdleTimeout == UINT64_C(90000000)) &&
		(ConfigStorage[0] == 0xA5) &&
		(ConfigStorage[sizeof(ConfigStorage) - 1u] == 0xA5),
		"HTTP pool config init did not support unaligned storage"
	);

	xrtClearError();
	xrtHttpClientPoolConfigInit(NULL);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP_CLIENT_ERROR_POOL),
		"HTTP pool null config error mismatch"
	);
	xrtClearError();
	xrtHttpClientPoolConfigInit(
		(xhttpclientpoolconfig*)(uintptr_t)(UINTPTR_MAX - 1u)
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(xrtErrorCode(xrtGetError()) ==
		 (int32)XHTTP_CLIENT_ERROR_POOL),
		"HTTP pool wrapping config error mismatch"
	);
	xrtClearError();
	testRequire(
		(xrtHttpClientCloseIdle(NULL) == 0) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP pool null close-idle error mismatch"
	);
	xrtClearError();
	memset(&Partial, 0, sizeof(Partial));
	testRequire(
		xrtHttpClientCloseIdle(&Partial) == 0,
		"HTTP pool partial construction close touched PoolLock"
	);
	xrtClearError();
	testRequire(
		!xrtHttpClientStats(NULL, &Stats) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP pool null stats error mismatch"
	);
	xrtClearError();
}



/*
	Origin Host 深复制失败时，已经分配的 Origin 必须回滚；
	修复内存条件后，同一个 Client 和 Call 状态仍可重新取得并释放配额。
*/
static void testHttpPoolOriginOom(
	test_http_pool_allocator* pAllocator
)
{
	char Host[3334];
	xhttpclient Client;
	xhttpcall Call;
	uint8 StatsStorage[sizeof(xhttpclientstats) + 2u];
	xhttpclientstats Stats;
	bool bReady;

	memset(Host, 'h', sizeof(Host) - 1u);
	Host[sizeof(Host) - 1u] = 0;
	memset(&Client, 0, sizeof(Client));
	xrtHttpClientPoolConfigInit(&Client.Config.Pool);
	Client.Config.Pool.MaxConnections = 1;
	Client.Config.Pool.MaxConnectionsPerOrigin = 1;
	testRequire(
		__xrtHttpPoolInit(&Client),
		"HTTP pool OOM fixture init failed"
	);
	memset(&Call, 0, sizeof(Call));
	Call.Client = &Client;
	Call.Host = Host;
	Call.Port = 80;

	testHttpPoolArm(pAllocator, sizeof(Host));
	bReady = true;
	testRequire(
		!__xrtHttpPoolAcquire(&Call, &bReady) &&
		pAllocator->Failed &&
		!bReady &&
		(Call.PoolOrigin == NULL) &&
		!Call.PoolReserved &&
		(xrtAtomic64Load(
			&Client.PoolConnections,
			XMEMORY_RELAXED
		) == 0) &&
		(xrtAtomic64Load(
			&Client.PoolWaiting,
			XMEMORY_RELAXED
		) == 0) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP pool Origin OOM was not transactional"
	);

	xrtClearError();
	pAllocator->Armed = false;
	testRequire(
		__xrtHttpPoolAcquire(&Call, &bReady) &&
		bReady &&
		Call.PoolReserved &&
		(Call.PoolOrigin != NULL) &&
		(xrtAtomic64Load(
			&Client.PoolConnections,
			XMEMORY_RELAXED
		) == 1),
		"HTTP pool did not recover after Origin OOM"
	);
	__xrtHttpPoolFinish(&Call);
	memset(StatsStorage, 0xA5, sizeof(StatsStorage));
	testRequire(
		!Call.PoolReserved &&
		(Call.PoolOrigin == NULL) &&
		(xrtAtomic64Load(
			&Client.PoolLive,
			XMEMORY_RELAXED
		) == 0) &&
		xrtHttpClientStats(
			&Client,
			(xhttpclientstats*)(void*)(StatsStorage + 1u)
		) &&
		(StatsStorage[0] == 0xA5) &&
		(StatsStorage[sizeof(StatsStorage) - 1u] == 0xA5),
		"HTTP pool unaligned stats storage mismatch"
	);
	memcpy(&Stats, StatsStorage + 1u, sizeof(Stats));
	testRequire(
		(Stats.ActiveConnections == 0) &&
		(Stats.IdleConnections == 0) &&
		(Stats.WaitingCalls == 0),
		"HTTP pool recovery retained Origin or quota state"
	);
	__xrtHttpPoolUnit(&Client);
}



/* 覆盖连接池公开边界和 Origin 创建的确定性分配失败。 */
int main(void)
{
	test_http_pool_allocator State;
	xallocator Allocator;

	memset(&State, 0, sizeof(State));
	Allocator.Context = &State;
	Allocator.Alloc = testHttpPoolAlloc;
	Allocator.Realloc = testHttpPoolRealloc;
	Allocator.Free = testHttpPoolFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP pool OOM allocator install failed"
	);

	testHttpPoolPublicContract();
	testHttpPoolOriginOom(&State);
	printf("[PASS] HTTP client connection pool OOM\n");
	return 0;
}
