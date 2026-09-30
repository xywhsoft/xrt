#include "../test.h"



/* 映射对象分配失败时不得创建半初始化映射。 */
int main(void)
{
	static const char sPath[] = "xrt-file-map-oom.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);

	testRequire((File != NULL) &&
		xrtWriteFull(File, "map", 3u, NULL),
		"file map OOM fixture creation failed");
	testRequire(xrtMemDebugFailAfter(0u),
		"file map OOM injection setup failed");
	testRequire(xrtFileMap(File, 0u, 0u,
		XFILE_MAP_READ) == NULL,
		"file mapping survived object OOM");
	testRequire(xrtMemDebugFailTriggered() && (xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"file mapping OOM reported the wrong error");
	xrtMemDebugFailClear();
	xrtClearError();
	testRequire(xrtClose(File), "file map OOM fixture close failed");
	testRequire(remove(sPath) == 0,
		"file map OOM fixture delete failed");
	testMemoryDebugDrain("file map OOM test leaked memory");
	return 0;
}
