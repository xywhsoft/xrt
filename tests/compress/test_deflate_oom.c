#include "../test.h"



/* 故障分配器按底层请求次数放行，用于区分状态和输出分配。 */
typedef struct test_deflate_oom_allocator {
	uint32 Allow;
	uint32 Denied;
} test_deflate_oom_allocator;



/* 在放行预算耗尽后拒绝新的底层内存。 */
static ptr testDeflateOomAlloc(
	ptr pData,
	size_t iSize
)
{
	test_deflate_oom_allocator* pAllocator =
		(test_deflate_oom_allocator*)pData;

	if ( pAllocator->Allow == 0 ) {
		pAllocator->Denied++;
		return NULL;
	}
	pAllocator->Allow--;
	return malloc(iSize);
}



/* 重分配使用同一故障预算，并在拒绝时保留原块。 */
static ptr testDeflateOomRealloc(
	ptr pData,
	ptr pMemory,
	size_t iSize
)
{
	test_deflate_oom_allocator* pAllocator =
		(test_deflate_oom_allocator*)pData;

	if ( pAllocator->Allow == 0 ) {
		pAllocator->Denied++;
		return NULL;
	}
	pAllocator->Allow--;
	return realloc(pMemory, iSize);
}



/* 归还故障窗口外成功取得的底层内存。 */
static void testDeflateOomFree(
	ptr pData,
	ptr pMemory
)
{
	(void)pData;
	free(pMemory);
}



/* 验证编码器状态和整块输出是两个独立、失败原子的分配阶段。 */
int main(void)
{
	test_deflate_oom_allocator State;
	xallocator Allocator = {
		&State,
		testDeflateOomAlloc,
		testDeflateOomRealloc,
		testDeflateOomFree
	};
	xdeflateconfig Config;
	xdeflate* pDeflate;
	bytes pOutput;
	size_t iSize = 17;
	uint32 iDenied;

	memset(&State, 0, sizeof(State));
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_GZIP;
	testRequire(
		xrtSetAllocator(&Allocator),
		"Deflate failure allocator install failed"
	);

	pDeflate = xrtDeflateCreate(&Config);
	testRequire(
		(pDeflate == NULL) &&
		(State.Denied != 0) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_MEMORY),
		"Deflate state allocation OOM mismatch"
	);
	xrtClearError();

	iDenied = State.Denied;
	State.Allow = 1;
	pOutput = xrtDeflateAll(
		XRT_BYTES_LITERAL(
			"output allocation must fail"
		),
		&Config,
		&iSize
	);
	testRequire(
		(pOutput == NULL) &&
		(iSize == 17) &&
		(State.Denied > iDenied) &&
		(xrtErrorKind(xrtGetError()) ==
		 XERR_MEMORY),
		"Deflate output allocation OOM mismatch"
	);
	xrtClearError();

	State.Allow = 4;
	pOutput = xrtDeflateAll(
		XRT_BYTES_LITERAL("recovered"),
		&Config,
		&iSize
	);
	testRequire(
		(pOutput != NULL) &&
		(iSize > 18u),
		"Deflate did not recover after OOM"
	);
	xrtFree(pOutput);
	printf("[PASS] deflate_oom\n");
	return 0;
}
