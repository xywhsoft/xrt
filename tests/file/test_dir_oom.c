#include "../test_allocator.h"



/* 目录迭代和根列表必须原样暴露分配失败。 */
int main(void)
{
	xdirroots Roots;
	xdirroots Saved;

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtDirOpen(".", 0u) == NULL,
		"directory open survived forced OOM");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"directory open OOM reported the wrong error");
	xrtClearError();
	memset(&Roots, 0xA5, sizeof(Roots));
	Saved = Roots;
	testRequire(!xrtDirRoots(&Roots), "root list survived forced OOM");
	testRequire(memcmp(&Roots, &Saved, sizeof(Roots)) == 0,
		"failed root list modified its output");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"root list OOM reported the wrong error");
	return 0;
}
