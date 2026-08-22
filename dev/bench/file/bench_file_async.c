#include "../bench_common.h"

#define XRT_MODULE_FILE_ASYNC
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 共享只读写入块由基准主体持有，单次异步操作结束时无需释放。 */
static void benchFileAsyncBorrowRelease(
	ptr pContext,
	cbytes pData,
	size_t iSize
)
{
	(void)pContext;
	(void)pData;
	(void)iSize;
}



/* 等待并验证一个定位写 Future 的完整写入结果。 */
static bool benchFileAsyncWriteDone(xfuture* pFuture, size_t iBlockSize)
{
	xfilechange* pChange;

	if (
		(pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	) {
		return false;
	}
	pChange = (xfilechange*)xrtFutureValue(pFuture);
	return (pChange != NULL) && (pChange->Size == (uint64)iBlockSize);
}



/* 等待并验证一个定位读 Future 的长度和固定探针字节。 */
static bool benchFileAsyncReadDone(
	xfuture* pFuture,
	size_t iBlockSize,
	unsigned char iProbe
)
{
	xfiledata* pData;

	if (
		(pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	) {
		return false;
	}
	pData = (xfiledata*)xrtFutureValue(pFuture);
	return
		(pData != NULL) &&
		(pData->Size == iBlockSize) &&
		!pData->End &&
		(pData->Data[0] == iProbe) &&
		(pData->Data[iBlockSize - 1u] == iProbe);
}



/* 以固定在途窗口提交零复制引用写入并等待全部终态。 */
static bool benchFileAsyncWrite(
	xasyncfile* pFile,
	xfuture** pFutures,
	size_t iWindow,
	cbytes pBlock,
	size_t iBlockSize,
	uint64 iOperations
)
{
	for ( uint64 i = 0; i < iOperations; i++ ) {
		size_t iSlot = (size_t)(i % (uint64)iWindow);

		if ( pFutures[iSlot] != NULL ) {
			if ( !benchFileAsyncWriteDone(pFutures[iSlot], iBlockSize) ) {
				return false;
			}
			xrtFutureDestroy(pFutures[iSlot]);
			pFutures[iSlot] = NULL;
		}
		pFutures[iSlot] = xrtAsyncFileWriteAtRef(
			pFile,
			i * (uint64)iBlockSize,
			(xbytesview){ pBlock, iBlockSize },
			benchFileAsyncBorrowRelease,
			NULL
		);
		if ( pFutures[iSlot] == NULL ) {
			return false;
		}
	}
	for ( size_t i = 0; i < iWindow; i++ ) {
		if ( pFutures[i] == NULL ) {
			continue;
		}
		if ( !benchFileAsyncWriteDone(pFutures[i], iBlockSize) ) {
			return false;
		}
		xrtFutureDestroy(pFutures[i]);
		pFutures[i] = NULL;
	}
	return true;
}



/* 以固定在途窗口提交拥有型读取并验证全部返回数据。 */
static bool benchFileAsyncRead(
	xasyncfile* pFile,
	xfuture** pFutures,
	size_t iWindow,
	size_t iBlockSize,
	uint64 iOperations,
	unsigned char iProbe
)
{
	for ( uint64 i = 0; i < iOperations; i++ ) {
		size_t iSlot = (size_t)(i % (uint64)iWindow);

		if ( pFutures[iSlot] != NULL ) {
			if ( !benchFileAsyncReadDone(
				pFutures[iSlot],
				iBlockSize,
				iProbe
			) ) {
				return false;
			}
			xrtFutureDestroy(pFutures[iSlot]);
			pFutures[iSlot] = NULL;
		}
		pFutures[iSlot] = xrtAsyncFileReadAt(
			pFile,
			i * (uint64)iBlockSize,
			iBlockSize
		);
		if ( pFutures[iSlot] == NULL ) {
			return false;
		}
	}
	for ( size_t i = 0; i < iWindow; i++ ) {
		if ( pFutures[i] == NULL ) {
			continue;
		}
		if ( !benchFileAsyncReadDone(
			pFutures[i],
			iBlockSize,
			iProbe
		) ) {
			return false;
		}
		xrtFutureDestroy(pFutures[i]);
		pFutures[i] = NULL;
	}
	return true;
}



/* 释放尚未归还的 Future，供任意失败出口统一回收。 */
static void benchFileAsyncFuturesFree(xfuture** pFutures, size_t iWindow)
{
	if ( pFutures == NULL ) {
		return;
	}
	for ( size_t i = 0; i < iWindow; i++ ) {
		xrtFutureDestroy(pFutures[i]);
	}
	free(pFutures);
}



/* 测量任务池型异步文件的有界并发定位读写，不把 flush 计入写吞吐。 */
int main(int argc, char** argv)
{
	static const char sPath[] = "xrt-file-async-benchmark.tmp";
	uint64 iOperations = xbenchArgU64(argc, argv, 1, 1024u);
	size_t iBlockSize = (size_t)xbenchArgU64(argc, argv, 2, 65536u);
	uint32 iThreads = xbenchArgU32(argc, argv, 3, 4u);
	size_t iWindow = (size_t)xbenchArgU64(argc, argv, 4, 128u);
	const unsigned char iProbe = UINT8_C(0xA5);
	xtaskpoolconfig PoolConfig;
	xfileoptions FileOptions;
	xtaskpool* pPool = NULL;
	xasyncfile* pFile = NULL;
	xfuture** pFutures = NULL;
	xfuture* pFuture = NULL;
	bytes pBlock = NULL;
	xbenchtimer Timer;
	uint64 iWriteElapsed = 0;
	uint64 iReadElapsed = 0;
	double fMebibytes;
	int iResult = 1;

	if (
		(iOperations == 0) ||
		(iBlockSize == 0) ||
		(iThreads == 0) ||
		(iThreads > XRT_TASK_POOL_THREAD_LIMIT) ||
		(iWindow == 0) ||
		(iWindow > SIZE_MAX / sizeof(xfuture*)) ||
		(iOperations > ((uint64)INT64_MAX / (uint64)iBlockSize))
	) {
		return 1;
	}
	if ( iWindow > (size_t)iOperations ) {
		iWindow = (size_t)iOperations;
	}
	pBlock = (bytes)malloc(iBlockSize);
	pFutures = (xfuture**)calloc(iWindow, sizeof(xfuture*));
	if ( (pBlock == NULL) || (pFutures == NULL) ) {
		goto Exit;
	}
	memset(pBlock, iProbe, iBlockSize);
	memset(&PoolConfig, 0, sizeof(PoolConfig));
	PoolConfig.Threads = iThreads;
	PoolConfig.QueueLimit = iWindow;
	pPool = xrtTaskPoolCreate(&PoolConfig);
	if ( pPool == NULL ) {
		goto Exit;
	}
	xrtFileOptionsInit(&FileOptions);
	FileOptions.Flags = XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE;
	pFile = xrtAsyncFileOpen(pPool, sPath, &FileOptions);
	if ( pFile == NULL ) {
		goto Exit;
	}

	xbenchTimerStart(&Timer);
	if ( !benchFileAsyncWrite(
		pFile,
		pFutures,
		iWindow,
		pBlock,
		iBlockSize,
		iOperations
	) ) {
		goto Exit;
	}
	xbenchTimerStop(&Timer);
	iWriteElapsed = xbenchTimerElapsedNs(&Timer);
	pFuture = xrtAsyncFileFlush(pFile);
	if (
		(pFuture == NULL) ||
		(xrtFutureWait(pFuture) != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED)
	) {
		goto Exit;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;

	xbenchTimerStart(&Timer);
	if ( !benchFileAsyncRead(
		pFile,
		pFutures,
		iWindow,
		iBlockSize,
		iOperations,
		iProbe
	) ) {
		goto Exit;
	}
	xbenchTimerStop(&Timer);
	iReadElapsed = xbenchTimerElapsedNs(&Timer);
	fMebibytes = ((double)iOperations * (double)iBlockSize) /
		(1024.0 * 1024.0);
	printf("file_async_threads: %u\n", iThreads);
	printf("file_async_window: %zu\n", iWindow);
	xbenchPrintMetricDouble(
		"file_async_write_mib_per_sec",
		iWriteElapsed != 0 ?
			(fMebibytes * 1000000000.0 / (double)iWriteElapsed) : 0.0
	);
	xbenchPrintMetricDouble(
		"file_async_read_mib_per_sec",
		iReadElapsed != 0 ?
			(fMebibytes * 1000000000.0 / (double)iReadElapsed) : 0.0
	);
	iResult = 0;

Exit:
	xrtFutureDestroy(pFuture);
	if ( pFile != NULL ) {
		pFuture = xrtAsyncFileClose(pFile);
		if ( pFuture != NULL ) {
			if ( xrtFutureWait(pFuture) != XWAIT_OK ) {
				iResult = 2;
			}
			xrtFutureDestroy(pFuture);
		}
	}
	benchFileAsyncFuturesFree(pFutures, iWindow);
	if ( (pPool != NULL) && !xrtTaskPoolDestroy(pPool) ) {
		iResult = 3;
	}
	(void)xrtFileDelete(sPath);
	xrtClearError();
	free(pBlock);
	return iResult;
}
