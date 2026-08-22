#include "../test.h"



/* 故障分配器记录 Ring 创建后的全部堆请求。 */
typedef struct testlogringallocator {
	xatomic64 Calls;
	xatomic64 Live;
	xatomic32 Fail;
} testlogringallocator;



/* 关闭 Fail 后交给 C 堆，开启后拒绝每个新增块。 */
static ptr testLogRingAlloc(ptr pContext, size_t iSize)
{
	testlogringallocator* pState = (testlogringallocator*)pContext;
	ptr pMemory;

	(void)xrtAtomic64FetchAdd(&pState->Calls, 1u, XMEMORY_ACQ_REL);
	if ( xrtAtomic32Load(&pState->Fail, XMEMORY_ACQUIRE) != 0u ) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		(void)xrtAtomic64FetchAdd(&pState->Live, 1u, XMEMORY_ACQ_REL);
	}
	return pMemory;
}



/* 重分配保持原块失败语义并维护存活块计数。 */
static ptr testLogRingRealloc(ptr pContext, ptr pMemory, size_t iSize)
{
	testlogringallocator* pState = (testlogringallocator*)pContext;
	ptr pResult;

	(void)xrtAtomic64FetchAdd(&pState->Calls, 1u, XMEMORY_ACQ_REL);
	if ( xrtAtomic32Load(&pState->Fail, XMEMORY_ACQUIRE) != 0u ) {
		return NULL;
	}
	if ( iSize == 0u ) {
		if ( pMemory != NULL ) {
			(void)xrtAtomic64FetchSub(&pState->Live, 1u, XMEMORY_ACQ_REL);
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



/* 释放由故障分配器创建的块。 */
static void testLogRingFree(ptr pContext, ptr pMemory)
{
	testlogringallocator* pState = (testlogringallocator*)pContext;

	if ( pMemory == NULL ) {
		return;
	}
	(void)xrtAtomic64FetchSub(&pState->Live, 1u, XMEMORY_ACQ_REL);
	free(pMemory);
}



/* 最小目标只统计成功消费。 */
static xlogresult testLogRingOomWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	(void)pRecord;
	(*pCount)++;
	return XLOG_RESULT_WRITTEN;
}



/* 验证 Ring 创建完成后提交和 Flush 不依赖任何新堆块。 */
int main(void)
{
	testlogringallocator State;
	xallocator Allocator;
	xlogsinkconfig TargetConfig;
	xlogringconfig RingConfig;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pRing;
	uint64 iCalls;
	uint64 iLive;
	size_t iCount = 0u;

	xrtAtomic64Init(&State.Calls, 0u);
	xrtAtomic64Init(&State.Live, 0u);
	xrtAtomic32Init(&State.Fail, 0u);
	Allocator.Context = &State;
	Allocator.Alloc = testLogRingAlloc;
	Allocator.Realloc = testLogRingRealloc;
	Allocator.Free = testLogRingFree;
	testRequire(xrtSetAllocator(&Allocator), "Logger ring allocator setup failed");
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		/* 本测试观察底层释放，不能让调试隔离队列延迟归还内存。 */
		testRequire(
			xrtMemDebugEnable(false),
			"Logger ring allocator quarantine disable failed"
		);
	#endif
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = testLogRingOomWrite;
	TargetConfig.UserData = &iCount;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	testRequire(pTarget != NULL, "Logger ring OOM target create failed");
	testRequire(xrtLogRingConfigInit(&RingConfig), "Logger ring OOM config failed");
	RingConfig.Capacity = 16u;
	RingConfig.RecordLimit = 256u;
	RingConfig.Batch = 16u;
	RingConfig.IdleWait = 0u;
	pRing = xrtLogRing(pTarget, &RingConfig);
	testRequire(pRing != NULL, "Logger ring OOM create failed");
	iLive = xrtAtomic64Load(&State.Live, XMEMORY_ACQUIRE);

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("preallocated");
	iCalls = xrtAtomic64Load(&State.Calls, XMEMORY_ACQUIRE);
	xrtAtomic32Store(&State.Fail, 1u, XMEMORY_RELEASE);
	for ( size_t i = 0; i < 8u; i++ ) {
		testRequire(
			xrtLogSinkSubmit(pRing, &Record) == XLOG_RESULT_WRITTEN,
			"Logger ring hot path allocated or failed"
		);
	}
	testRequire(xrtLogSinkFlush(pRing), "Logger ring zero-allocation flush failed");
	testRequire(iCount == 8u, "Logger ring zero-allocation count mismatch");
	testRequire(
		xrtAtomic64Load(&State.Calls, XMEMORY_ACQUIRE) == iCalls,
		"Logger ring hot path requested heap memory"
	);
	xrtAtomic32Store(&State.Fail, 0u, XMEMORY_RELEASE);
	xrtLogSinkFree(pRing);
	xrtLogSinkFree(pTarget);
	if ( xrtAtomic64Load(&State.Live, XMEMORY_ACQUIRE) >= iLive ) {
		fprintf(
			stderr,
			"[DETAIL] ring_live_before=%llu after=%llu calls=%llu\n",
			(unsigned long long)iLive,
			(unsigned long long)xrtAtomic64Load(
				&State.Live,
				XMEMORY_ACQUIRE
			),
			(unsigned long long)xrtAtomic64Load(
				&State.Calls,
				XMEMORY_ACQUIRE
			)
		);
	}
	testRequire(
		xrtAtomic64Load(&State.Live, XMEMORY_ACQUIRE) < iLive,
		"Logger ring lifecycle did not release owned blocks"
	);
	puts("[PASS] Logger ring OOM");
	return 0;
}
