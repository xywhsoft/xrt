#include "../test_budget_allocator.h"



/* 目录路径快照分配失败必须同步返回统一内存错误。 */
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
	testRequire(pPool != NULL, "async directory OOM pool create failed");
	Allocator.Allow = 0;
	testRequire(
		xrtDirCreateAsync(pPool, "xrt-dir-async-oom") == NULL,
		"async directory task survived forced OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Allocator.Denied != 0),
		"async directory OOM contract mismatch"
	);
	testRequire(xrtTaskPoolDestroy(pPool), "async directory OOM pool destroy failed");
	return 0;
}
