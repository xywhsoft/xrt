#include "../test.h"

#include <xrt/http_upgrade.h>



/* 比较 Upgrade 借用视图与期望协议和版本。 */
static bool testHttpUpgradeEqual(
	const xhttpupgradeitem* pUpgrade,
	cstr sProtocol,
	cstr sVersion
)
{
	size_t iProtocol = strlen(sProtocol);
	size_t iVersion = strlen(sVersion);

	return (pUpgrade->Protocol.Size == iProtocol) &&
		(pUpgrade->Version.Size == iVersion) &&
		(memcmp(
			pUpgrade->Protocol.Data, sProtocol, iProtocol
		) == 0) && ((iVersion == 0) ||
		(memcmp(
			pUpgrade->Version.Data, sVersion, iVersion
		) == 0));
}



/* 验证单元素解析、版本边界和严格语法。 */
static void testHttpUpgradeParse(void)
{
	static const cstr Invalid[] = {
		"",
		"/13",
		"websocket/",
		"web socket",
		"websocket /13",
		"websocket/ 13",
		"websocket, h2c"
	};
	xhttpupgradeitem Upgrade;

	testRequire(xrtHttpUpgradeParse(
		XRT_STR_LITERAL(" websocket "), &Upgrade
	) && testHttpUpgradeEqual(
		&Upgrade, "websocket", ""
	), "HTTP Upgrade versionless parse mismatch");
	testRequire(xrtHttpUpgradeParse(
		XRT_STR_LITERAL(" HTTP/2.0\t"), &Upgrade
	) && testHttpUpgradeEqual(
		&Upgrade, "HTTP", "2.0"
	), "HTTP Upgrade version parse mismatch");
	for ( size_t i = 0;
		i < (sizeof(Invalid) / sizeof(Invalid[0]));
		i++ ) {
		testRequire(!xrtHttpUpgradeParse(
			(xstrview){ Invalid[i], strlen(Invalid[i]) },
			&Upgrade
		), "HTTP Upgrade accepted an invalid element");
		xrtClearError();
	}
}



/* 验证完整列表在发布首项前检查畸形尾部。 */
static void testHttpUpgradeList(void)
{
	xhttpupgradecursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpupgradeitem Empty;
	size_t iCount;

	testRequire(xrtHttpUpgradeCount(
		XRT_STR_LITERAL(", websocket, , HTTP/2.0,"),
		&iCount
	) && (iCount == 2u),
		"HTTP Upgrade list count mismatch");
	xrtHttpUpgradeCursorInit(&Cursor);
	testRequire(xrtHttpUpgradeNext(
		XRT_STR_LITERAL(", websocket, , HTTP/2.0,"),
		&Cursor,
		&Upgrade
	) == XHTTP_NEXT_ITEM && testHttpUpgradeEqual(
		&Upgrade, "websocket", ""
	), "HTTP Upgrade first list item mismatch");
	testRequire(xrtHttpUpgradeNext(
		XRT_STR_LITERAL(", websocket, , HTTP/2.0,"),
		&Cursor,
		&Upgrade
	) == XHTTP_NEXT_ITEM && testHttpUpgradeEqual(
		&Upgrade, "HTTP", "2.0"
	), "HTTP Upgrade second list item mismatch");
	testRequire(xrtHttpUpgradeNext(
		XRT_STR_LITERAL(", websocket, , HTTP/2.0,"),
		&Cursor,
		&Upgrade
	) == XHTTP_NEXT_END &&
		(Upgrade.Protocol.Size == 0) &&
		(Upgrade.Version.Size == 0),
		"HTTP Upgrade list end mismatch");

	xrtHttpUpgradeCursorInit(&Cursor);
	memset(&Upgrade, 0xA5, sizeof(Upgrade));
	memset(&Empty, 0, sizeof(Empty));
	testRequire(xrtHttpUpgradeNext(
		XRT_STR_LITERAL("websocket, broken/"),
		&Cursor,
		&Upgrade
	) == XHTTP_NEXT_ERROR &&
		(Cursor.Offset == 0) && (Cursor.Validated == 0) &&
		(memcmp(&Upgrade, &Empty, sizeof(Upgrade)) == 0),
		"HTTP Upgrade malformed tail published a prefix");
	xrtClearError();
}



/* 验证重复字段按线路顺序组合，并在首项前验证全部字段。 */
static void testHttpUpgradeFields(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("X-Test"), XRT_STR_INIT("value") },
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("h2c, websocket") },
		{ XRT_STR_INIT("upgrade"), XRT_STR_INIT("HTTP/2.0") }
	};
	static const xhttpfield Invalid[] = {
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("websocket") },
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("broken/") }
	};
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpupgradeitem Empty;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	testRequire(xrtHttpUpgradeFieldNext(
		Fields, 3u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ITEM && testHttpUpgradeEqual(
		&Upgrade, "h2c", ""
	), "HTTP Upgrade repeated first item mismatch");
	testRequire(xrtHttpUpgradeFieldNext(
		Fields, 3u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ITEM && testHttpUpgradeEqual(
		&Upgrade, "websocket", ""
	), "HTTP Upgrade repeated second item mismatch");
	testRequire(xrtHttpUpgradeFieldNext(
		Fields, 3u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ITEM && testHttpUpgradeEqual(
		&Upgrade, "HTTP", "2.0"
	), "HTTP Upgrade repeated third item mismatch");
	testRequire(xrtHttpUpgradeFieldNext(
		Fields, 3u, &Cursor, &Upgrade
	) == XHTTP_NEXT_END,
		"HTTP Upgrade repeated list did not end");

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	memset(&Upgrade, 0xA5, sizeof(Upgrade));
	memset(&Empty, 0, sizeof(Empty));
	testRequire(xrtHttpUpgradeFieldNext(
		Invalid, 2u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ERROR &&
		(Cursor.Field == 0) && (Cursor.Offset == 0) &&
		(Cursor.Validated == 0) &&
		(memcmp(&Upgrade, &Empty, sizeof(Upgrade)) == 0),
		"HTTP Upgrade later bad field published a prefix");
	xrtClearError();
}



/* 验证公开游标与输出描述符支持未对齐存储。 */
static void testHttpUpgradeUnaligned(void)
{
	uint8 CursorStorage[sizeof(xhttpupgradecursor) + 1u];
	uint8 UpgradeStorage[sizeof(xhttpupgradeitem) + 1u];
	xhttpupgradeitem Upgrade;

	xrtHttpUpgradeCursorInit(
		(xhttpupgradecursor*)(void*)(CursorStorage + 1u)
	);
	testRequire(xrtHttpUpgradeNext(
		XRT_STR_LITERAL("websocket"),
		(xhttpupgradecursor*)(void*)(CursorStorage + 1u),
		(xhttpupgradeitem*)(void*)(UpgradeStorage + 1u)
	) == XHTTP_NEXT_ITEM,
		"HTTP Upgrade unaligned iteration failed");
	memcpy(&Upgrade, UpgradeStorage + 1u, sizeof(Upgrade));
	testRequire(testHttpUpgradeEqual(
		&Upgrade, "websocket", ""
	), "HTTP Upgrade unaligned output mismatch");
}



/* 验证所有公开入口都会拒绝发生地址回绕的借用输入。 */
static void testHttpUpgradeWrappedRange(void)
{
	xstrview Wrapped = {
		(cstr)(uintptr_t)(UINTPTR_MAX - 1u), 4u
	};
	xhttpupgradecursor Cursor;
	xhttpupgradeitem Upgrade;
	size_t iCount = 77u;

	testRequire(!xrtHttpUpgradeParse(
		Wrapped, &Upgrade
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Upgrade parser accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpUpgradeValid(Wrapped) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Upgrade validator accepted a wrapped input range");
	xrtClearError();
	testRequire(!xrtHttpUpgradeCount(Wrapped, &iCount) &&
		(iCount == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Upgrade counter accepted a wrapped input range");
	xrtClearError();
	xrtHttpUpgradeCursorInit(&Cursor);
	testRequire((xrtHttpUpgradeNext(
		Wrapped, &Cursor, &Upgrade
	) == XHTTP_NEXT_ERROR) &&
		(Cursor.Offset == 0) && (Cursor.Validated == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP Upgrade iterator accepted a wrapped input range");
	xrtClearError();
}



int main(void)
{
	testHttpUpgradeParse();
	testHttpUpgradeList();
	testHttpUpgradeFields();
	testHttpUpgradeUnaligned();
	testHttpUpgradeWrappedRange();
	puts("[PASS] http_upgrade");
	return 0;
}
