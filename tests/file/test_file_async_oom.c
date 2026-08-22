#include "../test.h"



/* 可指定失败序号的分配器用于覆盖同步受理和工作线程结果分配。 */
typedef struct testasyncfileoom {
	size_t Calls;
	size_t FailAt;
	size_t Live;
	bool Hit;
} testasyncfileoom;



/* 阻塞唯一工作线程，确保结果分配失败点可以确定性触发。 */
typedef struct testasyncfileoomblock {
	xmutex Lock;
	xcond Cond;
	bool Started;
	bool Release;
} testasyncfileoomblock;



/* 在指定分配序号失败，其余分配记录存活块。 */
static ptr testAsyncFileOomAlloc(
	ptr pData,
	size_t iSize
)
{
	testasyncfileoom* pState =
		(testasyncfileoom*)pData;
	ptr pMemory;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		pState->Live++;
	}
	return pMemory;
}



/* 重分配保留原块失败语义，并维护存活块计数。 */
static ptr testAsyncFileOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	testasyncfileoom* pState =
		(testasyncfileoom*)pData;
	ptr pResult;

	pState->Calls++;
	if ( pState->Calls == pState->FailAt ) {
		pState->Hit = true;
		return NULL;
	}
	if ( iSize == 0 ) {
		if ( pMemory != NULL ) {
			testRequire(
				pState->Live != 0,
				"async file OOM realloc counter underflow"
			);
			pState->Live--;
			free(pMemory);
		}
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		pState->Live++;
	}
	return pResult;
}



/* 释放存活块并检查计数不会下溢。 */
static void testAsyncFileOomFree(
	ptr pData,
	ptr pMemory
)
{
	testasyncfileoom* pState =
		(testasyncfileoom*)pData;

	if ( pMemory != NULL ) {
		testRequire(
			pState->Live != 0,
			"async file OOM free counter underflow"
		);
		pState->Live--;
		free(pMemory);
	}
}



/* 在系统临时目录下构造 OOM 测试路径。 */
static str testAsyncFileOomPath(void)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "async file OOM temp path failed");
	sPath = xrtPathJoin(
		sDirectory,
		"xrt-file-async-oom.tmp"
	);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "async file OOM path join failed");
	return sPath;
}



/* 在记录泄漏基线前建立唯一工作线程的小块缓存。 */
static xtaskoutcome testAsyncFileOomWarmTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	ptr pWarm;

	(void)pCancel;
	(void)pData;
	(void)pResult;
	pWarm = xrtMalloc(1u);
	if ( pWarm == NULL ) {
		return XTASK_FAILED;
	}
	xrtFree(pWarm);
	return XTASK_SUCCESS;
}



/* 占住唯一工作线程直到主线程开放结果分配失败点。 */
static xtaskoutcome testAsyncFileOomBlockTask(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
)
{
	testasyncfileoomblock* pBlock =
		(testasyncfileoomblock*)pData;
	ptr pWarm;
	bool bResult = true;

	(void)pCancel;
	(void)pResult;
	/* 在线程进入阻塞点前建立其小块缓存，隔离后续结果缓冲故障。 */
	pWarm = xrtMalloc(1u);
	if ( pWarm == NULL ) {
		return XTASK_FAILED;
	}
	xrtFree(pWarm);
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



/* 等待阻塞任务进入工作线程。 */
static bool testAsyncFileOomBlockStarted(
	testasyncfileoomblock* pBlock
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



/* 允许阻塞任务退出并执行结果分配。 */
static bool testAsyncFileOomBlockRelease(
	testasyncfileoomblock* pBlock
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



/* 关闭扫描阶段保留的异步文件，并等待无分配回收过程完成。 */
static void testAsyncFileOomCloseFixture(xasyncfile* pFile)
{
	xfuture* pFuture = xrtAsyncFileClose(pFile);

	testRequire(pFuture != NULL,
		"async file OOM fixture close failed");
	testRequire(
		xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"async file OOM fixture close wait failed"
	);
	testRequire(
		xrtFutureState(pFuture) == XFUTURE_RESOLVED,
		"async file OOM fixture close state mismatch"
	);
	xrtFutureDestroy(pFuture);
}



/* 验证创建、任务参数、写入副本和异步结果 OOM 都完整回滚。 */
int main(void)
{
	static testasyncfileoom State;
	xallocator Allocator = {
		&State,
		testAsyncFileOomAlloc,
		testAsyncFileOomRealloc,
		testAsyncFileOomFree
	};
	xtaskpoolconfig Config = { 1, 8, 0 };
	testasyncfileoomblock Block;
	xfileoptions Options;
	xtaskpool* pPool;
	xasyncfile* pFile;
	xasyncfile* arrOpen[512];
	xfuture* pWarmFuture;
	xfuture* pBlockFuture;
	xfuture* pReadFuture;
	xfuture* pCloseFuture;
	unsigned char* pSource;
	str sPath;
	size_t iBaseline;
	size_t iFailAt;
	size_t iOpenCount = 0;

	testRequire(
		xrtSetAllocator(&Allocator),
		"failed to install async file OOM allocator"
	);
	sPath = testAsyncFileOomPath();
	(void)xrtFileDelete(sPath);
	xrtClearError();
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "async file OOM pool create failed");
	pWarmFuture = xrtTaskSubmit(
		pPool,
		testAsyncFileOomWarmTask,
		NULL,
		NULL
	);
	testRequire(pWarmFuture != NULL,
		"async file OOM worker warm submit failed");
	testRequire(
		xrtFutureWaitFor(
			pWarmFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"async file OOM worker warm wait failed"
	);
	testRequire(
		xrtFutureState(pWarmFuture) == XFUTURE_RESOLVED,
		"async file OOM worker warm state mismatch"
	);
	xrtFutureDestroy(pWarmFuture);
	xrtFileOptionsInit(&Options);
	Options.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;

	/*
		保留成功创建的对象，直到必需尺寸类申请新 span。
		这样覆盖真实任务池、Promise 和文件所有权链，而不依赖私有结构尺寸。
	*/
	iBaseline = State.Live;
	iFailAt = State.Calls + 1u;
	State.Hit = false;
	State.FailAt = iFailAt;
	while ( iOpenCount < (sizeof(arrOpen) / sizeof(arrOpen[0])) ) {
		xasyncfile* pOpen = xrtAsyncFileOpen(
			pPool,
			sPath,
			&Options
		);

		if ( pOpen == NULL ) {
			break;
		}
		arrOpen[iOpenCount++] = pOpen;
	}
	testRequire(
		iOpenCount < (sizeof(arrOpen) / sizeof(arrOpen[0])),
		"async file object class did not exhaust"
	);
	testRequire(
		State.Hit && (State.Calls >= iFailAt),
		"async file open did not reach injected OOM"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"async file open OOM error mismatch"
	);
	State.FailAt = 0;
	xrtClearError();
	for ( size_t i = 0; i < iOpenCount; i++ ) {
		testAsyncFileOomCloseFixture(arrOpen[i]);
	}
	testRequire(
		State.Live == iBaseline,
		"async file open OOM leaked backing memory"
	);

	testRequire(
		xrtMutexInit(&Block.Lock),
		"async file OOM mutex init failed"
	);
	testRequire(
		xrtCondInit(&Block.Cond),
		"async file OOM condition init failed"
	);
	Block.Started = false;
	Block.Release = false;
	pBlockFuture = xrtTaskSubmit(
		pPool,
		testAsyncFileOomBlockTask,
		&Block,
		NULL
	);
	testRequire(pBlockFuture != NULL, "async file OOM blocker submit failed");
	testRequire(
		testAsyncFileOomBlockStarted(&Block),
		"async file OOM blocker did not start"
	);
	pFile = xrtAsyncFileOpen(pPool, sPath, &Options);
	testRequire(pFile != NULL, "async file OOM setup open failed");

	pSource = (unsigned char*)malloc(1024u * 1024u);
	testRequire(pSource != NULL, "async file OOM source allocation failed");
	memset(pSource, 0x5A, 1024u * 1024u);
	iBaseline = State.Live;
	State.Hit = false;
	State.FailAt = State.Calls + 1;
	testRequire(
		xrtAsyncFileWriteAt(
			pFile,
			0,
			(xbytesview){
				pSource,
				1024u * 1024u
			}
		) == NULL,
		"async file write survived copy OOM"
	);
	testRequire(State.Hit,
		"async file write did not reach injected OOM");
	free(pSource);
	xrtClearError();
	testRequire(
		State.Live == iBaseline,
		"async file write copy OOM leaked memory"
	);

	State.FailAt = 0;
	pReadFuture = xrtAsyncFileReadAt(
		pFile,
		0,
		1024u * 1024u
	);
	testRequire(pReadFuture != NULL, "async file OOM queued read failed");
	iBaseline = State.Live;
	State.Hit = false;
	State.FailAt = State.Calls + 1;
	testRequire(
		testAsyncFileOomBlockRelease(&Block),
		"async file OOM blocker release failed"
	);
	testRequire(
		xrtFutureWaitFor(
			pReadFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"async file result OOM wait failed"
	);
	testRequire(
		State.Hit &&
		(xrtFutureState(pReadFuture) == XFUTURE_FAILED) &&
		(xrtErrorKind(
			xrtFutureError(pReadFuture)
		) == XERR_MEMORY),
		"async file result OOM state mismatch"
	);
	testRequire(
		State.Live == iBaseline,
		"async file result OOM leaked backing memory"
	);

	State.FailAt = 0;
	pCloseFuture = xrtAsyncFileClose(pFile);
	testRequire(pCloseFuture != NULL, "async file OOM close failed");
	testRequire(
		xrtFutureWaitFor(
			pCloseFuture,
			UINT64_C(2000000)
		) == XWAIT_OK,
		"async file OOM close wait failed"
	);
	testRequire(
		xrtFutureState(pCloseFuture) == XFUTURE_RESOLVED,
		"async file OOM close state mismatch"
	);
	xrtFutureDestroy(pReadFuture);
	xrtFutureDestroy(pBlockFuture);
	xrtFutureDestroy(pCloseFuture);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file OOM pool destroy failed"
	);
	testRequire(
		xrtCondUnit(&Block.Cond) &&
		xrtMutexUnit(&Block.Lock),
		"async file OOM synchronization cleanup failed"
	);
	testRequire(
		xrtFileDelete(sPath),
		"async file OOM file cleanup failed"
	);
	xrtFree(sPath);
	xrtClearError();
	printf("[PASS] async file OOM\n");
	return 0;
}
