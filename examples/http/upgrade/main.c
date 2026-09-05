#include <stdio.h>

#include <xrt/http_upgrade.h>



/*
 * 范例：http/upgrade —— 协议升级：解析提议与生成应答列表
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtHttpUpgradeFieldCursorInit / FieldNext   迭代客户端提议
 *   xrtHttpUpgradeWrite   把"我支持的子集"写成规范 Upgrade 值
 * 模块宏：XRT_MODULE_HTTP_UPGRADE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/upgrade/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   protocol: h2c
 *   protocol: websocket
 *   protocol: HTTP/2.0
 *   field: websocket, HTTP/2.0
 *
 * 握手流程：客户端提议（可重复字段、逗号列表、名字大小写
 *   不敏感）→ 服务端从支持列表里选出交集写成应答 →
 *   101 Switching Protocols 后切换协议。WebSocket 升级
 *   （websocket_upgrade 模块）建立在这套语义之上。
 */


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
