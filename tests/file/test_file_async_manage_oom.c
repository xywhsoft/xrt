#include "../test_budget_allocator.h"



/* 路径快照分配失败必须同步返回统一内存错误。 */
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
	testRequire(pPool != NULL, "async file management OOM pool create failed");
	Allocator.Allow = 0;
	testRequire(
		xrtFileCopyAsync(
			pPool,
			"xrt-file-async-source.tmp",
			"xrt-file-async-target.tmp",
			false
		) == NULL,
		"async file management task survived forced OOM"
	);
	testRequire(
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"async file management OOM reported the wrong error"
	);
	testRequire(
		Allocator.Denied != 0,
		"async file management task did not reach injected OOM"
	);
	testRequire(
		xrtTaskPoolDestroy(pPool),
		"async file management OOM pool destroy failed"
	);
	return 0;
}
