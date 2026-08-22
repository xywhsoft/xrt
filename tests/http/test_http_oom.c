#include "../test.h"



/* 验证 token-list Build 的分配失败不发布部分结果。 */
int main(void)
{
	static const xstrview Tokens[] = {
		XRT_STR_INIT("GET"),
		XRT_STR_INIT("HEAD")
	};
	str sBuilt;
	size_t iSize = 71u;

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP token-list Build OOM setup failed");
	sBuilt = xrtHttpTokenListBuild(Tokens, 2u, &iSize);
	testRequire((sBuilt == NULL) && (iSize == 71u) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP token-list Build OOM was not atomic");
	xrtMemDebugFailClear();
	testMemoryDebugDrain(
		"HTTP token-list Build OOM leaked storage"
	);
	puts("[PASS] http_oom");
	return 0;
}
