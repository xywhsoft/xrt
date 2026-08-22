#include "../test_budget_allocator.h"



/* 任务参数复制分配失败必须同步返回统一内存错误。 */
int main(void)
{
	xtaskpoolconfig Config = { 1, 1, 0 };
	testbudgetallocator Allocator;
	xtaskpool* pPool;

	testRequire(
		testInstallBudgetAllocator(&Allocator, SIZE_MAX),
		"budget allocator install failed"
	);
	pPool = xrtTaskPoolCreate(&Config);
	testRequire(pPool != NULL, "async whole-file OOM pool create failed");
	Allocator.Allow = 0;
	testRequire(
		xrtFileWriteAllAsync(
			pPool,
			"xrt-file-async-whole-oom.tmp",
			XRT_BYTES_LITERAL("data")
		) == NULL,
		"async whole-file task survived forced OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"async whole-file OOM reported the wrong error"
	);
	testRequire(
		Allocator.Denied != 0,
		"async whole-file task did not reach injected OOM"
	);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async whole-file OOM pool destroy failed"
	);
	return 0;
}
