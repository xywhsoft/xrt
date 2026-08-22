#include "../test_allocator.h"



/* 资源错误不能被路径错误回调吞掉。 */
static xwalkerroraction testWalkOomError(cstr sPath,
	const xerror* pError, ptr pUserData)
{
	size_t* pCalls = (size_t*)pUserData;

	(void)sPath;
	(void)pError;
	(*pCalls)++;
	return XWALK_ERROR_SKIP;
}



/* OOM 必须保持统计输出不变并绕过可恢复错误回调。 */
int main(void)
{
	xwalkoptions Options;
	xwalkstats Stats;
	xwalkstats Saved;
	size_t iCalls = 0u;

	xrtWalkOptionsInit(&Options);
	Options.OnError = testWalkOomError;
	memset(&Stats, 0xA5, sizeof(Stats));
	Saved = Stats;
	testRequire(testInstallFailAllocator(),
		"walk failure allocator install failed");
	testRequire(!xrtFileWalk(".", &Options,
		NULL, &iCalls, &Stats), "file walk survived forced OOM");
	testRequire((iCalls == 0u) &&
		(memcmp(&Stats, &Saved, sizeof(Stats)) == 0),
		"walk OOM reached the recovery callback or modified statistics");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"walk OOM reported the wrong error");
	return 0;
}
