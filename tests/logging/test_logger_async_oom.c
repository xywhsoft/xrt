#include "../test.h"



/* 异步故障分配器使用原子计数，允许工作线程并发释放记录。 */
typedef struct testlogasyncallocator {
	xatomic64 Calls;
	xatomic64 FailAt;
	xatomic64 Live;
	xatomic32 Hit;
} testlogasyncallocator;



/* 在指定原子序号拒绝一次分配，其余请求交给 C 运行库。 */
static ptr testLogAsyncAlloc(ptr pContext, size_t iSize)
{
	testlogasyncallocator* pState = (testlogasyncallocator*)pContext;
	uint64 iCall = xrtAtomic64FetchAdd(
		&pState->Calls,
		1u,
		XMEMORY_ACQ_REL
	) + 1u;
	ptr pMemory;

	if ( iCall == xrtAtomic64Load(&pState->FailAt, XMEMORY_ACQUIRE) ) {
		xrtAtomic32Store(&pState->Hit, 1u, XMEMORY_RELEASE);
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		(void)xrtAtomic64FetchAdd(&pState->Live, 1u, XMEMORY_ACQ_REL);
	}
	return pMemory;
}



/* 重分配保持原块失败语义，并只在新建块时增加存活计数。 */
static ptr testLogAsyncRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testlogasyncallocator* pState = (testlogasyncallocator*)pContext;
	uint64 iCall = xrtAtomic64FetchAdd(
		&pState->Calls,
		1u,
		XMEMORY_ACQ_REL
	) + 1u;
	ptr pResult;

	if ( iCall == xrtAtomic64Load(&pState->FailAt, XMEMORY_ACQUIRE) ) {
		xrtAtomic32Store(&pState->Hit, 1u, XMEMORY_RELEASE);
		return NULL;
	}
	if ( iSize == 0u ) {
		if ( pMemory != NULL ) {
			(void)xrtAtomic64FetchSub(
				&pState->Live,
				1u,
				XMEMORY_ACQ_REL
			);
		}
		free(pMemory);
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		(void)xrtAtomic64FetchAdd(&pState->Live, 1u, XMEMORY_ACQ_REL);
	}
	return pResult;
}



/* 工作线程和提交线程都可以安全归还故障分配器创建的块。 */
static void testLogAsyncFree(ptr pContext, ptr pMemory)
{
	testlogasyncallocator* pState = (testlogasyncallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	(void)xrtAtomic64FetchSub(&pState->Live, 1u, XMEMORY_ACQ_REL);
	free(pMemory);
}



/* 最小目标只统计工作线程成功消费的记录。 */
static xlogresult testLogAsyncOomWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	(void)pRecord;
	(*pCount)++;
	return XLOG_RESULT_WRITTEN;
}



/* 创建使用调用方计数的同步目标。 */
static xlogsink* testLogAsyncOomTarget(size_t* pCount)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("oom-target");
	Config.Level = XLOG_TRACE;
	Config.Write = testLogAsyncOomWrite;
	Config.UserData = pCount;
	return xrtLogSinkCreate(&Config);
}



/* 验证状态、记录和 Flush 栅栏分配失败都保持事务性并可以恢复。 */
int main(void)
{
	testlogasyncallocator State;
	xallocator Allocator;
	xlogasyncstats Stats;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pAsync;
	char arrLarge[64u * 1024u];
	uint64 iNext;
	size_t iCount = 0;

	xrtAtomic64Init(&State.Calls, 0u);
	xrtAtomic64Init(&State.FailAt, UINT64_MAX);
	xrtAtomic64Init(&State.Live, 0u);
	xrtAtomic32Init(&State.Hit, 0u);
	Allocator.Context = &State;
	Allocator.Alloc = testLogAsyncAlloc;
	Allocator.Realloc = testLogAsyncRealloc;
	Allocator.Free = testLogAsyncFree;
	testRequire(
		xrtSetAllocator(&Allocator),
		"Logger async OOM allocator setup failed"
	);
	pTarget = testLogAsyncOomTarget(&iCount);
	testRequire(pTarget != NULL, "Logger async OOM target create failed");

	iNext = xrtAtomic64Load(&State.Calls, XMEMORY_ACQUIRE) + 1u;
	xrtAtomic64Store(&State.FailAt, iNext, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.Hit, 0u, XMEMORY_RELEASE);
	pAsync = xrtLogAsync(pTarget, NULL);
	testRequire(
		(pAsync == NULL) &&
		(xrtAtomic32Load(&State.Hit, XMEMORY_ACQUIRE) != 0u) &&
		(xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_ASYNC_CONFIG
		) != NULL) &&
		(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL),
		"Logger async state OOM was not wrapped"
	);
	xrtClearError();
	xrtAtomic64Store(&State.FailAt, UINT64_MAX, XMEMORY_RELEASE);
	pAsync = xrtLogAsync(pTarget, NULL);
	testRequire(pAsync != NULL, "Logger async did not recover after create OOM");

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	memset(arrLarge, 'r', sizeof(arrLarge));
	Record.Message = (xstrview){ arrLarge, sizeof(arrLarge) };
	iNext = xrtAtomic64Load(&State.Calls, XMEMORY_ACQUIRE) + 1u;
	xrtAtomic64Store(&State.FailAt, iNext, XMEMORY_RELEASE);
	xrtAtomic32Store(&State.Hit, 0u, XMEMORY_RELEASE);
	testRequire(
		(xrtLogSinkSubmit(pAsync, &Record) == XLOG_RESULT_ERROR) &&
		(xrtAtomic32Load(&State.Hit, XMEMORY_ACQUIRE) != 0u) &&
		(xrtErrorFind(
			xrtGetError(),
			"xrt.log",
			XLOG_ERROR_ASYNC_RECORD
		) != NULL) &&
		(xrtErrorIs(xrtGetError(), XERR_MEMORY) != NULL),
		"Logger async record OOM was not transactional"
	);
	xrtClearError();
	testRequire(iCount == 0u, "Logger async OOM submitted a partial record");

	xrtAtomic64Store(&State.FailAt, UINT64_MAX, XMEMORY_RELEASE);
	Record.Message = XRT_STR_LITERAL("record");
	testRequire(
		xrtLogSinkSubmit(pAsync, &Record) == XLOG_RESULT_WRITTEN,
		"Logger async did not recover after record OOM"
	);
	testRequire(xrtLogSinkFlush(pAsync), "Logger async OOM recovery flush failed");
	testRequire(
		(iCount == 1u) &&
		xrtLogAsyncStats(pAsync, &Stats) &&
		(Stats.Enqueued == 1u) &&
		(Stats.Processed == 1u) &&
		(Stats.Written == 1u) &&
		(Stats.Queued == 0u) &&
		(Stats.QueueBytes == 0u),
		"Logger async OOM recovery state mismatch"
	);
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	xrtClearError();
	printf("[PASS] Logger async OOM\n");
	return 0;
}
