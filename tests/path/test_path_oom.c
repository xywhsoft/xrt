#include "../test_allocator.h"



/* 所有分配型路径 Helper 必须原样暴露 OOM。 */
int main(void)
{
	xpathparts Parts;
	xpathiter Iterator;
	xpathcomponent Component;
	xstrview arrParts[2] = {
		XRT_STR_LITERAL("alpha"),
		XRT_STR_LITERAL("beta")
	};

	testRequire(testInstallFailAllocator(), "failure allocator install failed");
	testRequire(xrtPathParse(XRT_STR_LITERAL("alpha/file.txt"),
		XPATH_POSIX, &Parts), "zero-allocation path parse should survive OOM");
	testRequire(xrtPathIterInit(&Iterator,
		XRT_STR_LITERAL("alpha/../beta"), XPATH_POSIX) &&
		xrtPathNext(&Iterator, &Component),
		"zero-allocation path iteration should survive OOM");
	testRequire(xrtPathIsLocal(XRT_STR_LITERAL("alpha/../beta"), XPATH_POSIX),
		"zero-allocation local path check should survive OOM");

	/* 四个分解 Helper 都必须原样暴露分配失败。 */
	testRequire(xrtPathName("alpha/file.txt") == NULL,
		"path name allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"path name OOM error mismatch");
	xrtClearError();
	testRequire(xrtPathStem("alpha/file.txt") == NULL,
		"path stem allocation should fail");
	testRequire(xrtPathExt("alpha/file.txt") == NULL,
		"path extension allocation should fail");
	testRequire(xrtPathParent("alpha/file.txt") == NULL,
		"path parent allocation should fail");

	/* 所有路径构建和修改入口都必须在失败时返回 NULL。 */
	testRequire(xrtPathClean(XRT_STR_LITERAL("alpha/../beta"),
		XPATH_POSIX) == NULL, "path clean allocation should fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"path clean OOM error mismatch");
	testRequire(xrtPathBuild(arrParts, 2, XPATH_POSIX) == NULL,
		"path build allocation should fail");
	testRequire(xrtPathJoin("alpha", "beta") == NULL,
		"path join allocation should fail");
	testRequire(xrtPathRelative(XRT_STR_LITERAL("/alpha"),
		XRT_STR_LITERAL("/beta"), XPATH_POSIX) == NULL,
		"path relative allocation should fail");
	testRequire(xrtPathWithName("alpha/file.txt", "next.txt") == NULL,
		"path name replacement allocation should fail");
	testRequire(xrtPathWithExt("alpha/file.txt", "bin") == NULL,
		"path extension replacement allocation should fail");
	return 0;
}
