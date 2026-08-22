#include "../test_allocator.h"



/* 检查系统路径入口是否保留统一内存错误。 */
static void testPathSystemMemory(cstr sMessage)
{
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY), sMessage);
	xrtClearError();
}



/* 每个返回拥有字符串的系统路径入口都必须原样暴露 OOM。 */
int main(void)
{
	testRequire(testInstallFailAllocator(), "failure allocator install failed");

	testRequire(xrtPathCwd() == NULL, "path cwd allocation should fail");
	testPathSystemMemory("path cwd OOM error mismatch");
	testRequire(xrtPathAbs(".") == NULL,
		"path absolute allocation should fail");
	testPathSystemMemory("path absolute OOM error mismatch");
	testRequire(xrtPathReal(".") == NULL,
		"path real allocation should fail");
	testPathSystemMemory("path real OOM error mismatch");
	testRequire(xrtPathRel(".", "child") == NULL,
		"path relative system allocation should fail");
	testPathSystemMemory("path relative system OOM error mismatch");
	testRequire(xrtPathHome() == NULL,
		"path home allocation should fail");
	testPathSystemMemory("path home OOM error mismatch");
	testRequire(xrtPathTemp() == NULL,
		"path temp allocation should fail");
	testPathSystemMemory("path temp OOM error mismatch");
	testRequire(xrtPathExecutable() == NULL,
		"path executable allocation should fail");
	testPathSystemMemory("path executable OOM error mismatch");
	testRequire(xrtPathAppDir() == NULL,
		"path application directory allocation should fail");
	testPathSystemMemory("path application directory OOM error mismatch");
	return 0;
}
