#include "../test_allocator.h"



/* 目录树前置路径分配失败时不得创建目标或修改统计输出。 */
int main(void)
{
	static const char sTarget[] = "xrt-file-tree-oom-target";
	xwalkstats Stats;
	xwalkstats Saved;

	memset(&Stats, 0xA5, sizeof(Stats));
	Saved = Stats;
	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(!xrtFileTreeCopy(".", sTarget, NULL, &Stats),
		"tree copy survived forced OOM");
	testRequire(memcmp(&Stats, &Saved, sizeof(Stats)) == 0,
		"failed OOM tree copy modified statistics output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"tree copy OOM reported the wrong error");
	return 0;
}
