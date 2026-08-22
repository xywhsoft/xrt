#include "../test.h"
#include "../../src/internal/xrt_temp.h"



/* 模拟协程切换，验证绑定 arena 与线程宿主 arena 相互隔离。 */
int main(void)
{
	xtemparena tCoroutineArena;
	xtemparena* pPrevious;
	char* sHost;
	char* sCoroutine;

	memset(&tCoroutineArena, 0, sizeof(tCoroutineArena));
	sHost = (char*)xrtTemp(64);
	testRequire(sHost != NULL, "host temp allocation failed");
	memcpy(sHost, "host", 5);

	pPrevious = __xrtTempContextSwap(&tCoroutineArena);
	testRequire(pPrevious == NULL, "initial bound arena should be null");
	sCoroutine = (char*)xrtTemp(64);
	testRequire(sCoroutine != NULL, "bound temp allocation failed");
	memcpy(sCoroutine, "coroutine", 10);
	testRequire(__xrtTempContextSwap(pPrevious) == &tCoroutineArena, "bound arena restore mismatch");

	testRequire(strcmp(sHost, "host") == 0, "bound arena corrupted host data");
	testRequire(strcmp(sCoroutine, "coroutine") == 0, "bound arena data was not preserved");
	testRequire(xrtTempClear(), "host temp reset failed");
	xrtTempUnit(&tCoroutineArena);
	printf("[PASS] temp_context\n");
	return 0;
}
