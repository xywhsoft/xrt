/*
 * 范例：websocket/upgrade_tour —— HTTP/1.1 Upgrade 握手协商闭环
 * ----------------------------------------------------------------
 * 演示 API：
 *   【配置】      xrtWsUpgradeClientConfigInit / Valid
 *                 xrtWsUpgradeServerConfigInit / Valid
 *   【服务端】    xrtWsUpgradeRequestCheck（校验 Upgrade 请求）
 *                 xrtWsUpgradeResponseFields（101 响应字段）
 *   【客户端】    xrtWsUpgradeResponseCheck（校验 Key/协议绑定）
 *   【流配置】    xrtWsUpgradeStreamConfig（协商结果 → Stream 配置）
 * 模块宏：XRT_MODULE_WEBSOCKET（+ WEBSOCKET_UPGRADE[_STREAM]）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/upgrade_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   upgrade: client/server configs init+valid ok
 *   upgrade: request check + response fields ok
 *   upgrade: response check binds key+protocol ok
 *   upgrade: stream config from upgrade ok
 *
 * 全链闭环：KeyGenerate → RequestFields 组请求头 → 服务端
 *   RequestCheck 出 Accept → ResponseFields 组 101 → 客户端
 *   ResponseCheck 验绑定 → StreamConfig 落成 Stream 配置。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 把起始行与字段数组拼成一段完整 HTTP/1 请求/响应文本。 */
static size_t exampleJoin(char* sOut, size_t iCapacity, cstr sStart,
	const xhttpfield* pFields, size_t iCount)
{
	size_t iOffset = strlen(sStart);

	memcpy(sOut, sStart, iOffset);
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t n;

		if ( iOffset + 2u >= iCapacity ) {
			return 0u;
		}
		memcpy(sOut + iOffset, "\r\n", 2u);
		iOffset += 2u;
		n = pFields[i].Name.Size;
		if ( iOffset + n + 2u >= iCapacity ) {
			return 0u;
		}
		memcpy(sOut + iOffset, pFields[i].Name.Data, n);
		iOffset += n;
		sOut[iOffset++] = ':';
		sOut[iOffset++] = ' ';
		n = pFields[i].Value.Size;
		if ( iOffset + n + 4u >= iCapacity ) {
			return 0u;
		}
		memcpy(sOut + iOffset, pFields[i].Value.Data, n);
		iOffset += n;
	}
	memcpy(sOut + iOffset, "\r\n\r\n", 4u);
	return iOffset + 4u;
}

int main(void)
{
	xwsupgradeclientconfig ClientConfig;
	xwsupgradeserverconfig ServerConfig;
	xwsupgrade Upgrade;
	xwsupgrade ClientUpgrade;
	xwsstreamconfig StreamConfig;
	xhttp1limits Limits;
	xhttp1errorinfo Error;
	xhttp1head Head;
	xhttpfield Fields[8];
	xhttpfield ClientFields[8];
	char arrKey[XWS_KEY_CAPACITY];
	char arrRequest[1024];
	char arrResponse[1024];
	size_t iCount = 0;
	size_t iSize;
	int iResult = 1;

	/* ---- 配置初始化与校验：默认形态合法、清零形态拒绝。 ---- */
	xrtWsUpgradeClientConfigInit(&ClientConfig);
	xrtWsUpgradeServerConfigInit(&ServerConfig);
	/* 双方都声明子协议——服务端才会在 101 中选定 chat。 */
	ClientConfig.Protocols = XRT_STR_LITERAL("chat, superchat");
	ServerConfig.Protocols = XRT_STR_LITERAL("chat, superchat");
	if ( !xrtWsUpgradeClientConfigValid(&ClientConfig) ||
		!xrtWsUpgradeServerConfigValid(&ServerConfig) ||
		/* 清零客户端配置=最小合法形态（无协议无压缩）。 */
		!xrtWsUpgradeClientConfigValid(
			&(xwsupgradeclientconfig) { 0 }) ) {
		goto Cleanup;
	}
	if ( !xrtWsKeyGenerate(arrKey, sizeof(arrKey)) ||
		!xrtWsKeyValid((xstrview) { arrKey,
			strlen(arrKey) }) ) {
		goto Cleanup;
	}
	printf("upgrade: client/server configs init+valid ok\n");

	/* ---- 服务端视角：组请求 → RequestCheck → 出 101 字段。 ---- */
	xrtHttp1LimitsInit(&Limits);
	if ( !xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { arrKey, strlen(arrKey) },
			XRT_STR_LITERAL("chat, superchat"),
			XRT_STR_LITERAL(""),
			Fields, 8u, &iCount) ||
		(iCount == 0u) ||
		((iSize = exampleJoin(arrRequest, sizeof(arrRequest),
			"GET /ws HTTP/1.1", Fields, iCount)) == 0u) ) {
		goto Cleanup;
	}
	/* 解析请求（独立字段数组）。 */
	{
		xhttpfield ParseFields[8];

		xrtHttp1HeadInit(&Head, ParseFields, 8);
		if ( (xrtHttp1RequestParse(
				(xbytesview) { (const uint8*)arrRequest,
					iSize },
				&Head, &Limits, &Error) != XHTTP1_READY) ||
			!xrtWsUpgradeRequestCheck(&Head, &ServerConfig,
				&Upgrade) ) {
			goto Cleanup;
		}
	}
	/* 101 响应字段：Accept 由 RequestCheck 结果持有。 */
	if ( !xrtWsUpgradeResponseFields(
			(xstrview) { Upgrade.Accept,
				strlen(Upgrade.Accept) },
			Upgrade.Protocol,
			XRT_STR_LITERAL(""),
			ClientFields, 8u, &iCount) ||
		(iCount == 0u) ||
		((iSize = exampleJoin(arrResponse, sizeof(arrResponse),
			"HTTP/1.1 101 Switching Protocols", ClientFields,
			iCount)) == 0u) ) {
		goto Cleanup;
	}
	printf("upgrade: request check + response fields ok\n");

	/* ---- 客户端视角：解析 101 → ResponseCheck 验绑定。 ---- */
	{
		xhttpfield ParseFields[8];

		xrtHttp1HeadInit(&Head, ParseFields, 8);
		if ( (xrtHttp1ResponseParse(
				(xbytesview) { (const uint8*)arrResponse,
					iSize },
				&Head, &Limits, &Error) != XHTTP1_READY) ||
			!xrtWsUpgradeResponseCheck(&Head,
				(xstrview) { arrKey, strlen(arrKey) },
				&ClientConfig, &ClientUpgrade) ||
			(ClientUpgrade.Protocol.Size != 4u) ||
			(memcmp(ClientUpgrade.Protocol.Data, "chat",
				4u) != 0) ) {
			goto Cleanup;
		}
	}
	printf("upgrade: response check binds key+protocol ok\n");

	/* ---- 协商结果落成 Stream 配置（客户端角色）。 ---- */
	if ( !xrtWsUpgradeStreamConfig(&StreamConfig, XWS_ROLE_CLIENT,
			&ClientUpgrade) ) {
		goto Cleanup;
	}
	printf("upgrade: stream config from upgrade ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
