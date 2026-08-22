#include "../test.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#endif



#define TEST_HTTP_BODY_FILE_OOM_LONG_PATH	4096u
#define TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT	256u



/* 测试计数器只需要覆盖远小于 32 位上限的短期分配。 */
#if defined(_WIN32) || defined(_WIN64)
	typedef volatile LONG test_http_body_file_atomic;
#else
	typedef volatile uint32 test_http_body_file_atomic;
#endif



/* 无分配地读取测试原子计数器。 */
static uint32 testHttpBodyFileAtomicLoad(
	const test_http_body_file_atomic* pValue
)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint32)InterlockedCompareExchange(
			(volatile LONG*)pValue,
			0,
			0
		);
	#else
		return __atomic_load_n(pValue, __ATOMIC_RELAXED);
	#endif
}



/* 无分配地写入测试原子计数器。 */
static void testHttpBodyFileAtomicStore(
	test_http_body_file_atomic* pValue,
	uint32 iValue
)
{
	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedExchange(
			(volatile LONG*)pValue,
			(LONG)iValue
		);
	#else
		__atomic_store_n(pValue, iValue, __ATOMIC_RELAXED);
	#endif
}



/* 无分配地增加测试原子计数器并返回旧值。 */
static uint32 testHttpBodyFileAtomicAdd(
	test_http_body_file_atomic* pValue,
	uint32 iValue
)
{
	#if defined(_WIN32) || defined(_WIN64)
		LONG iOld;
		LONG iNew;

		do {
			iOld = InterlockedCompareExchange(
				(volatile LONG*)pValue,
				0,
				0
			);
			iNew = iOld + (LONG)iValue;
		} while (
			InterlockedCompareExchange(
				(volatile LONG*)pValue,
				iNew,
				iOld
			) != iOld
		);
		return (uint32)iOld;
	#else
		return __atomic_fetch_add(
			pValue,
			iValue,
			__ATOMIC_RELAXED
		);
	#endif
}



/* 无分配地减少测试原子计数器并返回旧值。 */
static uint32 testHttpBodyFileAtomicSub(
	test_http_body_file_atomic* pValue,
	uint32 iValue
)
{
	return testHttpBodyFileAtomicAdd(pValue, 0u - iValue);
}



/* 可按尺寸或持续失败的分配器用于验证同步构造边界。 */
typedef struct test_http_body_file_oom {
	test_http_body_file_atomic Live;
	test_http_body_file_atomic RejectAtLeast;
	test_http_body_file_atomic FailAll;
} test_http_body_file_oom;



/* 按当前故障模式拒绝申请，并原子记录其他存活块。 */
static ptr testHttpBodyFileOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_http_body_file_oom* pState =
		(test_http_body_file_oom*)pData;
	uint32 iRejectAtLeast;
	ptr pMemory;

	iRejectAtLeast = testHttpBodyFileAtomicLoad(
		&pState->RejectAtLeast
	);
	if (
		(testHttpBodyFileAtomicLoad(&pState->FailAll) != 0u) ||
		((iRejectAtLeast != 0u) &&
			(iSize >= (size_t)iRejectAtLeast))
	) {
		return NULL;
	}
	pMemory = malloc(iSize);
	if ( pMemory != NULL ) {
		(void)testHttpBodyFileAtomicAdd(
			&pState->Live,
			1u
		);
	}
	return pMemory;
}



/* 重分配使用相同故障模式并原子维护存活计数。 */
static ptr testHttpBodyFileOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_http_body_file_oom* pState =
		(test_http_body_file_oom*)pData;
	uint32 iRejectAtLeast;
	uint32 iLive;
	ptr pResult;

	iRejectAtLeast = testHttpBodyFileAtomicLoad(
		&pState->RejectAtLeast
	);
	if (
		(testHttpBodyFileAtomicLoad(&pState->FailAll) != 0u) ||
		((iRejectAtLeast != 0u) &&
			(iSize >= (size_t)iRejectAtLeast))
	) {
		return NULL;
	}
	if ( iSize == 0 ) {
		if ( pMemory != NULL ) {
			iLive = testHttpBodyFileAtomicSub(
				&pState->Live,
				1u
			);
			testRequire(
				iLive != 0u,
				"HTTP file body OOM realloc counter underflow"
			);
			free(pMemory);
		}
		return NULL;
	}
	pResult = realloc(pMemory, iSize);
	if ( (pResult != NULL) && (pMemory == NULL) ) {
		(void)testHttpBodyFileAtomicAdd(
			&pState->Live,
			1u
		);
	}
	return pResult;
}



/* 释放存活块并原子检查计数。 */
static void testHttpBodyFileOomFree(
	ptr pData,
	ptr pMemory
)
{
	test_http_body_file_oom* pState =
		(test_http_body_file_oom*)pData;
	uint32 iLive;

	if ( pMemory != NULL ) {
		iLive = testHttpBodyFileAtomicSub(
			&pState->Live,
			1u
		);
		testRequire(
			iLive != 0u,
			"HTTP file body OOM free counter underflow"
		);
		free(pMemory);
	}
}



/* 在系统临时目录下创建 OOM 测试路径。 */
static str testHttpBodyFileOomPath(void)
{
	str sDirectory = xrtPathTemp();
	str sPath;

	testRequire(sDirectory != NULL, "HTTP file body OOM temp path failed");
	sPath = xrtPathJoin(
		sDirectory,
		"xrt-http-body-file-oom.tmp"
	);
	xrtFree(sDirectory);
	testRequire(sPath != NULL, "HTTP file body OOM path join failed");
	return sPath;
}



/* 创建一个可由异步文件对象采用的只读文件。 */
static xasyncfile* testHttpBodyFileOomAsync(
	xtaskpool* pPool,
	cstr sPath
)
{
	xfile File = xrtOpen(sPath, XFILE_READ);
	xasyncfile* pAsync;

	testRequire(File != NULL, "HTTP file body OOM source open failed");
	pAsync = xrtAsyncFileAdopt(pPool, File);
	testRequire(pAsync != NULL, "HTTP file body OOM async adoption failed");
	return pAsync;
}



/* 等待并释放异步文件关闭 Future。 */
static void testHttpBodyFileOomClose(xasyncfile* pFile)
{
	xfuture* pFuture = xrtAsyncFileClose(pFile);

	testRequire(
		(pFuture != NULL) &&
		(xrtFutureWaitFor(
			pFuture,
			UINT64_C(2000000)
		) == XWAIT_OK),
		"HTTP file body OOM async close failed"
	);
	xrtFutureDestroy(pFuture);
}



/* 文件准备参数和采用构造 OOM 必须保留所有权并完整回滚。 */
int main(void)
{
	static test_http_body_file_oom State = {
		0,
		0,
		0
	};
	xallocator Allocator = {
		&State,
		testHttpBodyFileOomAlloc,
		testHttpBodyFileOomRealloc,
		testHttpBodyFileOomFree
	};
	xtaskpoolconfig Config = { 1, 8, 0 };
	xtaskpool* pPool;
	xasyncfile* Files[TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT] = { 0 };
	xhttpbody* Bodies[TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT] = { 0 };
	xfuture* pFuture;
	xfile File;
	str sPath;
	char LongPath[TEST_HTTP_BODY_FILE_OOM_LONG_PATH + 1u];
	uint32 iBaseline;
	size_t iBodyCount = 0;
	size_t iFailed = SIZE_MAX;
	size_t i;

	testRequire(
		xrtSetAllocator(&Allocator),
		"failed to install HTTP file body OOM allocator"
	);
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "HTTP file body OOM pool create failed");

	/* 超过池化上限的参数副本应直接命中尺寸故障点。 */
	memset(LongPath, 'a', TEST_HTTP_BODY_FILE_OOM_LONG_PATH);
	LongPath[TEST_HTTP_BODY_FILE_OOM_LONG_PATH] = '\0';
	iBaseline = testHttpBodyFileAtomicLoad(&State.Live);
	testHttpBodyFileAtomicStore(
		&State.RejectAtLeast,
		TEST_HTTP_BODY_FILE_OOM_LONG_PATH
	);
	pFuture = xrtHttpBodyFileFuture(pPool, LongPath, NULL);
	testHttpBodyFileAtomicStore(&State.RejectAtLeast, 0u);
	if ( pFuture != NULL ) {
		xrtFutureDestroy(pFuture);
	}
	testRequire(
		pFuture == NULL,
		"HTTP file body prepare survived argument-copy OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(testHttpBodyFileAtomicLoad(&State.Live) == iBaseline),
		"HTTP file body prepare OOM mismatch"
	);
	xrtClearError();

	sPath = testHttpBodyFileOomPath();
	(void)xrtFileDelete(sPath);
	xrtClearError();
	File = xrtOpen(
		sPath,
		XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE
	);
	testRequire(File != NULL, "HTTP file body OOM fixture open failed");
	testRequire(
		xrtWriteFull(File, "data", 4, NULL) &&
		xrtClose(File),
		"HTTP file body OOM fixture write failed"
	);

	/* 先准备文件对象，避免故障模式影响被测采用操作之外的步骤。 */
	for ( i = 0; i < TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT; i++ ) {
		Files[i] = testHttpBodyFileOomAsync(pPool, sPath);
	}

	/*
		禁止底层分配器继续增长，再持有每个成功正文。
		已有尺寸类耗尽后，采用操作必须失败并保留文件所有权。
	*/
	iBaseline = testHttpBodyFileAtomicLoad(&State.Live);
	testHttpBodyFileAtomicStore(&State.FailAll, 1u);
	for ( i = 0; i < TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT; i++ ) {
		Bodies[i] = xrtHttpBodyFileAdopt(
			Files[i],
			0,
			4,
			NULL
		);
		if ( Bodies[i] == NULL ) {
			iFailed = i;
			break;
		}
		Files[i] = NULL;
		iBodyCount++;
	}
	testHttpBodyFileAtomicStore(&State.FailAll, 0u);
	testRequire(
		iFailed != SIZE_MAX,
		"HTTP file body adopt did not exhaust pooled storage"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(testHttpBodyFileAtomicLoad(&State.Live) == iBaseline),
		"HTTP file body adopt OOM leaked or changed error"
	);
	xrtClearError();

	/* 成功正文拥有文件，失败项和未使用项仍由调用方关闭。 */
	for ( i = 0; i < iBodyCount; i++ ) {
		xrtHttpBodyDestroy(Bodies[i]);
	}
	for ( i = 0; i < TEST_HTTP_BODY_FILE_OOM_ADOPT_LIMIT; i++ ) {
		if ( Files[i] != NULL ) {
			testHttpBodyFileOomClose(Files[i]);
		}
	}

	testRequire(
		xrtTaskPoolDestroy(pPool),
		"HTTP file body OOM pool destroy failed"
	);
	testRequire(
		xrtFileDelete(sPath),
		"HTTP file body OOM fixture cleanup failed"
	);
	xrtFree(sPath);
	xrtClearError();
	printf("[PASS] HTTP file body OOM\n");
	return 0;
}

