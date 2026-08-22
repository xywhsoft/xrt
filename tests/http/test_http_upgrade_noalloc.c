#include "../test.h"

#include <xrt/http_upgrade.h>



/* 验证解析、重复字段迭代和直接写出路径不执行逻辑分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("h2c, websocket") }
	};
	static const xhttpupgradeitem Values[] = {
		{ XRT_STR_INIT("websocket"), { NULL, 0 } },
		{ XRT_STR_INIT("HTTP"), XRT_STR_INIT("2.0") }
	};
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	char sOutput[32];
	str sBuilt;
	size_t iSize;

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Upgrade no-allocation probe setup failed");
	xrtHttpUpgradeFieldCursorInit(&Cursor);
	testRequire(xrtHttpUpgradeFieldNext(
		Fields, 1u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ITEM && xrtHttpUpgradeWrite(
		Values, 2u, sOutput, sizeof(sOutput), &iSize
	) && (iSize == 19u) && !xrtMemDebugFailTriggered(),
		"HTTP Upgrade direct path allocated memory");
	xrtMemDebugFailClear();
	xrtClearError();
	iSize = 71u;
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Upgrade Build OOM probe setup failed");
	sBuilt = xrtHttpUpgradeBuild(Values, 2u, &iSize);
	testRequire((sBuilt == NULL) && (iSize == 71u) &&
		xrtMemDebugFailTriggered() &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Upgrade Build OOM was not atomic");
	xrtMemDebugFailClear();
	testMemoryDebugDrain(
		"HTTP Upgrade no-allocation probe leaked storage"
	);
	puts("[PASS] http_upgrade_noalloc");
	return 0;
}
