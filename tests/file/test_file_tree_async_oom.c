#include "../test_budget_allocator.h"



/* 目录树路径和选项快照分配失败必须同步返回统一内存错误。 */
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
	testRequire(pPool != NULL, "async tree OOM pool create failed");
	Allocator.Allow = 0;
	testRequire(
		xrtDirCopyAsync(
			pPool,
			"xrt-tree-async-source",
			"xrt-tree-async-target",
			false
		) == NULL,
		"async tree task survived forced OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY) &&
		(Allocator.Denied != 0),
		"async tree OOM contract mismatch"
	);
	testRequire(xrtTaskPoolDestroy(pPool), "async tree OOM pool destroy failed");
	return 0;
}
