#include <stdio.h>

#include <xrt/http_upgrade.h>



/* 解析重复字段并规范生成一个 Upgrade 选择列表。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Upgrade"), XRT_STR_INIT("h2c, websocket") },
		{ XRT_STR_INIT("upgrade"), XRT_STR_INIT("HTTP/2.0") }
	};
	static const xhttpupgradeitem Offered[] = {
		{ XRT_STR_INIT("websocket"), { NULL, 0 } },
		{ XRT_STR_INIT("HTTP"), XRT_STR_INIT("2.0") }
	};
	xhttpupgradefieldcursor Cursor;
	xhttpupgradeitem Upgrade;
	xhttpnext Next;
	char sOutput[64];
	size_t iSize;

	xrtHttpUpgradeFieldCursorInit(&Cursor);
	while ( (Next = xrtHttpUpgradeFieldNext(
		Fields, 2u, &Cursor, &Upgrade
	)) == XHTTP_NEXT_ITEM ) {
		printf(
			"protocol: %.*s",
			(int)Upgrade.Protocol.Size,
			Upgrade.Protocol.Data
		);
		if ( Upgrade.Version.Size != 0 ) {
			printf(
				"/%.*s",
				(int)Upgrade.Version.Size,
				Upgrade.Version.Data
			);
		}
		putchar('\n');
	}
	if ( (Next == XHTTP_NEXT_ERROR) ||
		!xrtHttpUpgradeWrite(
			Offered,
			2u,
			sOutput,
			sizeof(sOutput),
			&iSize
		) ) {
		return 1;
	}
	printf("field: %.*s\n", (int)iSize, sOutput);
	return 0;
}
