#include "../test.h"



#define TEST_HTTP_SERVER_FILE_OOM_CLASSES	64u
#define TEST_HTTP_SERVER_FILE_OOM_LIMIT		24u
#define TEST_HTTP_SERVER_FILE_OOM_HELD		65536u



/* 故障分配器只在尺寸类耗尽窗口内限制新的 backing span。 */
typedef struct test_http_server_file_oom_allocator {
	xatomic32 Gate;
	xatomic32 Allow;
	xatomic64 Denied;
} test_http_server_file_oom_allocator;



/* 阻塞唯一文件工作线程，使同步构造失败点保持确定。 */
typedef struct test_http_server_file_oom_block {
	xmutex Lock;
	xcond Cond;
	bool Started;
	bool Release;
} test_http_server_file_oom_block;



/* 测试状态保存故障分配器和暂时取走的尺寸类空闲块。 */
typedef struct test_http_server_file_oom {
	test_http_server_file_oom_allocator Allocator;
	ptr* Held;
	size_t HeldCount;
	size_t HeldCapacity;
} test_http_server_file_oom;



/* 故障门关闭时使用系统堆，开启后只放行指定数量的 backing span。 */
static ptr testHttpServerFileOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_server_file_oom_allocator* pAllocator =
		(test_http_server_file_oom_allocator*)pData;

	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return malloc(iSize);
}



/* 重分配服从与初始分配相同的 backing span 故障门。 */
static ptr testHttpServerFileOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_server_file_oom_allocator* pAllocator =
		(test_http_server_file_oom_allocator*)pData;

	if ( xrtAtomic32Load(
		&pAllocator->Gate,
		XMEMORY_ACQUIRE
	) != 0 ) {
		uint32 iAllow = xrtAtomic32Load(
			&pAllocator->Allow,
			XMEMORY_ACQUIRE
		);

		if ( iAllow == 0 ) {
			(void)xrtAtomic64FetchAdd(
				&pAllocator->Denied,
				1,
				XMEMORY_RELAXED
			);
			return NULL;
		}
		(void)xrtAtomic32FetchSub(
			&pAllocator->Allow,
			1,
			XMEMORY_ACQ_REL
		);
	}
	return realloc(pMemory, iSize);
}



/* 释放故障窗口外成功取得的底层内存。 */
static void testHttpServerFileOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 占住唯一任务线程，避免文件准备与同步故障注入并发竞争。 */
static xtaskoutcome testHttpServerFileOomBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	test_http_server_file_oom_block* pBlock =
		(test_http_server_file_oom_block*)pData;
	bool bResult = true;

	(void)pCancel;
	(void)pResult;
	if ( !xrtMutexLock(&pBlock->Lock) ) {
		return XTASK_FAILED;
	}
	pBlock->Started = true;
	bResult = xrtCondBroadcast(&pBlock->Cond);
	while ( bResult && !pBlock->Release ) {
		bResult = xrtCondWait(
			&pBlock->Cond,
			&pBlock->Lock
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult ? XTASK_SUCCESS : XTASK_FAILED;
}



/* 等待阻塞任务进入唯一工作线程。 */
static bool testHttpServerFileOomBlockStarted(
	test_http_server_file_oom_block* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	while ( bResult && !pBlock->Started ) {
		bResult = xrtCondWaitFor(
			&pBlock->Cond,
			&pBlock->Lock,
			UINT64_C(2000000)
		) == XWAIT_OK;
	}
	(void)xrtMutexUnlock(&pBlock->Lock);
	return bResult;
}



/* 允许阻塞任务退出并开始处理已经受理的文件准备。 */
static bool testHttpServerFileOomBlockRelease(
	test_http_server_file_oom_block* pBlock
)
{
	bool bResult = xrtMutexLock(&pBlock->Lock);

	if ( bResult ) {
		pBlock->Release = true;
		bResult = xrtCondBroadcast(&pBlock->Cond);
		(void)xrtMutexUnlock(&pBlock->Lock);
	}
	return bResult;
}



/* 暂时取走所有池化尺寸类的空闲块，使下一次分配必须申请 span。 */
static void testHttpServerFileOomExhaust(
	test_http_server_file_oom* pState
)
{
	xrtAtomic32Store(
		&pState->Allocator.Allow,
		0,
		XMEMORY_RELEASE
	);
	xrtAtomic32Store(
		&pState->Allocator.Gate,
		1,
		XMEMORY_RELEASE
	);
	for ( size_t i = 1;
		i <= TEST_HTTP_SERVER_FILE_OOM_CLASSES;
		i++ ) {
		for ( ;; ) {
			ptr pMemory = xrtMalloc(i * 16);

			if ( pMemory == NULL ) {
				xrtClearError();
				break;
			}
			testRequire(
				pState->HeldCount <
					pState->HeldCapacity,
				"HTTP server file OOM exhaustion overflowed"
			);
			pState->Held[pState->HeldCount++] =
				pMemory;
		}
	}
}



/* 关闭故障门并归还尺寸类空闲块。 */
static void testHttpServerFileOomRestore(
	test_http_server_file_oom* pState
)
{
	xrtAtomic32Store(
		&pState->Allocator.Gate,
		0,
		XMEMORY_RELEASE
	);
	for ( size_t i = 0; i < pState->HeldCount; i++ ) {
		xrtFree(pState->Held[i]);
	}
	pState->HeldCount = 0;
}



/* 要求一次完整故障尝试没有留下任何逻辑堆块。 */
static void testHttpServerFileOomBalanced(
	const xmemstats* pBefore,
	uint32 iAllow,
	bool bAccepted
)
{
	xmemstats After;
	uint64 iAlloc;
	uint64 iFree;

	xrtMemStatsGet(&After);
	iAlloc = After.BlockAllocCalls -
		pBefore->BlockAllocCalls;
	iFree = After.BlockFreeCalls -
		pBefore->BlockFreeCalls;
	if ( iAlloc != iFree ) {
		fprintf(
			stderr,
			"[DIAG] HTTP server file OOM: allow=%u accepted=%d "
			"block-alloc=%llu block-free=%llu\n",
			(unsigned int)iAllow,
			bAccepted ? 1 : 0,
			(unsigned long long)iAlloc,
			(unsigned long long)iFree
		);
	}
	testRequire(
		iAlloc == iFree,
		"HTTP server file OOM leaked a heap block"
	);
}



/* 在指定 span 放行量下构建文件 Reply，并完整排空所有异步资源。 */
static bool testHttpServerFileOomAttempt(
	test_http_server_file_oom* pState,
	cstr sPath,
	uint32 iAllow
)
{
	xtaskpoolconfig PoolConfig = { 1, 8, 0 };
	test_http_server_file_oom_block Block;
	xmemstats Before;
	xtaskpool* pPool;
	xfuture* pBlockFuture;
	xfuture* pReplyFuture;
	uint64 iDenied;
	bool bAccepted;

	xrtMemStatsGet(&Before);
	memset(&Block, 0, sizeof(Block));
	testRequire(
		xrtMutexInit(&Block.Lock) &&
		xrtCondInit(&Block.Cond),
		"HTTP server file OOM blocker init failed"
	);
	pPool = xrtTaskPoolCreate(&PoolConfig);
	testRequire(
		pPool != NULL,
		"HTTP server file OOM pool create failed"
	);
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testHttpServerFileOomBlockTask,
		&Block,
		NULL
	);
	testRequire(
		(pBlockFuture != NULL) &&
		testHttpServerFileOomBlockStarted(&Block),
		"HTTP server file OOM blocker did not start"
	);

	testHttpServerFileOomExhaust(pState);
	iDenied = xrtAtomic64Load(
		&pState->Allocator.Denied,
		XMEMORY_ACQUIRE
	);
	xrtAtomic32Store(
		&pState->Allocator.Allow,
		iAllow,
		XMEMORY_RELEASE
	);
	pReplyFuture = xrtHttpReplyFileFuture(
		pPool,
		XHTTP_STATUS_OK,
		XRT_STR_LITERAL("text/plain"),
		sPath
	);
	bAccepted = pReplyFuture != NULL;
	if ( !bAccepted ) {
		testRequire(
			(xrtAtomic64Load(
				&pState->Allocator.Denied,
				XMEMORY_ACQUIRE
			 ) > iDenied) &&
			(xrtErrorKind(xrtGetError()) ==
			 XERR_MEMORY),
			"HTTP server file OOM failure mismatch"
		);
	}
	testHttpServerFileOomRestore(pState);
	xrtClearError();

	testRequire(
		testHttpServerFileOomBlockRelease(&Block) &&
		(xrtFutureWaitFor(
			pBlockFuture,
			UINT64_C(2000000)
		 ) == XWAIT_OK),
		"HTTP server file OOM blocker release failed"
	);
	if ( pReplyFuture != NULL ) {
		xwaitresult WaitResult = xrtFutureWaitFor(
			pReplyFuture,
			UINT64_C(2000000)
		);
		xfuturestate FutureState = xrtFutureState(pReplyFuture);

		if ( (WaitResult != XWAIT_OK) ||
			(FutureState != XFUTURE_RESOLVED) ) {
			fprintf(
				stderr,
				"[DIAG] HTTP server file OOM recovery: allow=%u "
				"wait=%d state=%d\n",
				(unsigned int)iAllow,
				(int)WaitResult,
				(int)FutureState
			);
		}
		testRequire(
			(WaitResult == XWAIT_OK) &&
			(FutureState == XFUTURE_RESOLVED),
			"HTTP server file did not recover after OOM"
		);
		xrtFutureDestroy(pReplyFuture);
	}
	xrtFutureDestroy(pBlockFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP server file OOM pool destroy failed"
	);
	testRequire(
		xrtCondDestroy(&Block.Cond) &&
		xrtMutexDestroy(&Block.Lock),
		"HTTP server file OOM blocker destroy failed"
	);
	testHttpServerFileOomBalanced(
		&Before,
		iAllow,
		bAccepted
	);
	return bAccepted;
}



/* 逐级穿过组合层全部同步分配边界，并要求失败后仍能成功构建。 */
int main(void)
{
	test_http_server_file_oom State;
	xallocator Allocator = {
		&State.Allocator,
		testHttpServerFileOomAlloc,
		testHttpServerFileOomRealloc,
		testHttpServerFileOomFree
	};
	xfile File;
	str sPath;
	size_t iFailures = 0;
	bool bSuccess = false;

	memset(&State, 0, sizeof(State));
	State.HeldCapacity = TEST_HTTP_SERVER_FILE_OOM_HELD;
	State.Held = (ptr*)malloc(
		State.HeldCapacity * sizeof(ptr)
	);
	testRequire(
		State.Held != NULL,
		"HTTP server file OOM held array failed"
	);
	xrtAtomic32Init(&State.Allocator.Gate, 0);
	xrtAtomic32Init(&State.Allocator.Allow, 0);
	xrtAtomic64Init(&State.Allocator.Denied, 0);
	testRequire(
		xrtSetAllocator(&Allocator),
		"HTTP server file OOM allocator install failed"
	);
	xrtMemStatsEnable(true);
	xrtMemStatsReset();

	File = xrtFileTemp(
		NULL,
		"xrt-http-server-file-oom-",
		".tmp",
		&sPath
	);
	testRequire(
		(File != NULL) &&
		xrtWriteFull(File, "data", 4, NULL) &&
		xrtClose(File),
		"HTTP server file OOM fixture failed"
	);

	for ( uint32 iAllow = 0;
		iAllow <= TEST_HTTP_SERVER_FILE_OOM_LIMIT;
		iAllow++ ) {
		if ( testHttpServerFileOomAttempt(
			&State,
			sPath,
			iAllow
		) ) {
			bSuccess = true;
			break;
		}
		iFailures++;
	}
	testRequire(
		(iFailures >= 2) && bSuccess,
		"HTTP server file OOM sweep missed a boundary"
	);
	testRequire(
		xrtFileDelete(sPath),
		"HTTP server file OOM fixture cleanup failed"
	);
	xrtFree(sPath);
	free(State.Held);
	printf("[PASS] HTTP server file OOM\n");
	return 0;
}
