#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留重复 Upgrade 字段解析。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("h2c, websocket") }
	};
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	return (xrtHttpUpgradeFieldNext(
		Fields, 1u, &Cursor, &Upgrade
	) == XHTTP_NEXT_ITEM) ? 0 : 1;
}
