/*
 * 范例：websocket/extension_tour —— 握手校验/协议/扩展/压缩变换全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【握手纯函数】 xrtWsKeyValid / xrtWsAcceptValid /
 *                  xrtWsCloseCodeValid
 *   【子协议】    xrtWsProtocolNext（游标迭代）/
 *                  xrtWsProtocolsValid / xrtWsProtocolsHas
 *   【扩展】      xrtWsExtensionCount / xrtWsExtensionWrite
 *   【压缩协商】  xrtWsDeflateIs / DeflateOfferWrite /
 *                  DeflateResponseParse / DeflateResponseCheck /
 *                  DeflateDirection / InflaterConfigApply /
 *                  DeflaterConfigApply
 *   【压缩变换】  xrtWsDeflaterReset / DeflaterFlush / DeflaterAbort /
 *                  DeflaterBound / DeflaterSize(Inflater 版) /
 *                  xrtWsInflaterReset / InflaterSize
 *   【消息状态】  xrtWsMessageConfigInit / InitSafe / Reset
 * 模块宏：XRT_MODULE_WEBSOCKET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/extension_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   ws: key/accept/close = 1/1/1
 *   ws: protocols iter=2 has=1
 *   ws: extensions count=1 write=45
 *   ws: deflate offer->response->direction ok (bits=10/15)
 *   ws: transforms reset+flush+abort size=0
 *   ws: message configs ok
 *
 * 压缩协商走完整闭环：构造 offer → 写出 → 解析 response →
 *   Check 校验 → Direction 拆出双方向参数 → 应用到收发配置；
 *   变换层演示 Reset 复用、Flush 同步边界、Abort 丢弃上下文。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* 有效的十六字节 WebSocket nonce（base64 编码 24 字符）。 */
#define EXAMPLE_KEY "dGhlIHNhbXBsZSBub25jZQ=="
/* 对应 GUID 拼接 SHA-1 的 base64，RFC 6455 样例值。 */
#define EXAMPLE_ACCEPT "s3pPLMBiTxaQ9kYGzzhZRbK+xOo="

static bool exampleCollect(xbytesview Data, ptr pData);

typedef struct examplesink {
	size_t Size;
} examplesink;

static bool exampleCollect(xbytesview Data, ptr pData)
{
	examplesink* pSink = (examplesink*)pData;

	pSink->Size = pSink->Size + Data.Size;
	return true;
}

int main(void)
{
	xwsdeflate Offer;
	xwsdeflate Response;
	xwsdeflatedirection SendDir;
	xwsdeflatedirection RecvDir;
	xwsinflaterconfig InflaterConfig;
	xwsdeflaterconfig DeflaterConfig;
	xwsinflater* pInflater = NULL;
	xwsdeflater* pDeflater = NULL;
	xwsmessageconfig MsgConfig;
	xwsmessageconfig MsgSafe;
	xwsmessagestate MsgState;
	xwsextension Extension;
	xstrview Protocol;
	examplesink Sink;
	char OfferText[96];
	char ExtText[64];
	size_t iOffset = 0;
	size_t iSize = 0;
	size_t iBound = 0;
	int iProtocols = 0;
	int iResult = 1;

	/* ---- 握手纯函数 ---- */
	if ( !xrtWsKeyValid(SV(EXAMPLE_KEY)) ||
		xrtWsKeyValid(SV("short")) ||
		!xrtWsAcceptValid(SV(EXAMPLE_KEY), SV(EXAMPLE_ACCEPT)) ||
		xrtWsAcceptValid(SV(EXAMPLE_KEY), SV("wrong-wrong-wrong-wrong")) ||
		!xrtWsCloseCodeValid(1000u) ||
		!xrtWsCloseCodeValid(3000u) ||
		xrtWsCloseCodeValid(999u) ||
		xrtWsCloseCodeValid(1005u) ) {
		goto Cleanup;
	}
	printf("ws: key/accept/close = 1/1/1\n");

	/* ---- 子协议：迭代 + 校验 + 成员 ---- */
	if ( !xrtWsProtocolsValid(SV("chat, superchat")) ||
		xrtWsProtocolsValid(SV("chat,,x")) ||  /* 空项非法 */
		!xrtWsProtocolsHas(SV("chat, superchat"), SV("superchat")) ||
		xrtWsProtocolsHas(SV("chat"), SV("super")) ) {
		goto Cleanup;
	}
	while ( xrtWsProtocolNext(SV("chat, superchat"), &iOffset,
			&Protocol) == XHTTP_NEXT_ITEM ) {
		++iProtocols;
	}
	if ( iProtocols != 2 ) {
		goto Cleanup;
	}
	printf("ws: protocols iter=%d has=1\n", iProtocols);

	/* ---- 扩展：计数与写出 ---- */
	if ( !xrtWsExtensionCount(SV("permessage-deflate; client_max_window_bits"),
			&iSize) ||
		(iSize != 1u) ||
		!xrtWsExtensionWrite(SV("permessage-deflate"),
			SV("client_max_window_bits=12"),
			ExtText, sizeof(ExtText), &iSize) ||
		(iSize == 0u) ||
		(iSize >= sizeof(ExtText)) ) {
		goto Cleanup;
	}
	printf("ws: extensions count=1 write=%zu\n", iSize);

	/* ---- 压缩协商闭环 ---- */
	/* 1) 构造 offer：双方向窗口位。 */
	(void)xrtWsDeflateInit(&Offer);
	/* 窗口参数必须伴随存在标志（Flags 表达参数是否出现）。 */
	Offer.Flags = XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW;
	Offer.ServerMaxWindowBits = 10u;
	Offer.ClientMaxWindowBits = 12u;
	/* 2) 写出 offer 文本（两段式容量查询 + 写入）。 */
	if ( !xrtWsDeflateOfferWrite(&Offer, NULL, 0u, &iSize) ||
		(iSize >= sizeof(OfferText)) ||
		!xrtWsDeflateOfferWrite(&Offer, OfferText,
			sizeof(OfferText), &iSize) ) {
		goto Cleanup;
	}
	/* 3) Extension 视图 + 名称判定 + 解析 response。 */
	Extension.Name = SV("permessage-deflate");
	Extension.Parameters = (xstrview) { OfferText, iSize };
	if ( !xrtWsDeflateIs(&Extension) ) {
		goto Cleanup;
	}
	(void)xrtWsDeflateInit(&Response);
	/* 最小合规响应 = Accept(Offer) 的产物；直接用服务端约束
	 * 手工构造（server_no_context_takeover + server_bits=10）。 */
	/* 响应的参数同样需要存在标志（server 窗口 10 被确认）。 */
	Response.Flags = XWS_DEFLATE_SERVER_MAX_WINDOW;
	Response.ServerMaxWindowBits = 10u;
	/* client 方向响应未提及：保留 Init 默认（显式 0 非法）。 */
	if ( !xrtWsDeflateResponseCheck(&Offer, &Response) ) {
		goto Cleanup;
	}
	/* 4) Direction：按服务端角色拆出发送/接收方向参数。 */
	/* Direction 从响应推导：server 方向确认 10；client 方向
	 * 响应未确认窗口 → 回落默认 15（offer 的 12 只是上限）。 */
	if ( !xrtWsDeflateDirection(&Response, XWS_ROLE_SERVER, true,
			&SendDir) ||
		!xrtWsDeflateDirection(&Response, XWS_ROLE_SERVER, false,
			&RecvDir) ||
		(SendDir.WindowBits != 10u) ||
		(RecvDir.WindowBits != 15u) ) {
		goto Cleanup;
	}
	/* 5) 应用到收发配置。 */
	(void)xrtWsInflaterConfigInit(&InflaterConfig);
	(void)xrtWsDeflaterConfigInit(&DeflaterConfig);
	if ( !xrtWsInflaterConfigApply(&InflaterConfig, &RecvDir) ||
		!xrtWsDeflaterConfigApply(&DeflaterConfig, &SendDir) ) {
		goto Cleanup;
	}
	printf("ws: deflate offer->response->direction ok"
		" (bits=%u/%u)\n",
		(unsigned)SendDir.WindowBits,
		(unsigned)RecvDir.WindowBits);
	/* ResponseParse：解析响应字段的参数段（server 窗口 10）。 */
	{
		xwsextension RespExt;
		xwsdeflate Parsed;

		RespExt.Name = SV("permessage-deflate");
		RespExt.Parameters = SV(
			"server_max_window_bits=10");
		(void)xrtWsDeflateInit(&Parsed);
		if ( !xrtWsDeflateResponseParse(&RespExt, &Parsed) ||
			(Parsed.ServerMaxWindowBits != 10u) ) {
			goto Cleanup;
		}
	}

	/* ---- 变换层：Reset/Flush/Abort/Bound/Size ---- */
	pInflater = xrtWsInflaterCreate(&InflaterConfig);
	pDeflater = xrtWsDeflaterCreate(&DeflaterConfig);
	if ( (pInflater == NULL) || (pDeflater == NULL) ||
		!xrtWsDeflaterBound(64u, &iBound) ||
		(iBound < 64u) ) {
		goto Cleanup;
	}
	/* Flush：压缩一段并建立同步边界（输出计入 Sink）。 */
	Sink.Size = 0;
	if ( !xrtWsDeflaterBegin(pDeflater, true) ||
		!xrtWsDeflaterWrite(pDeflater,
			XRT_BYTES_LITERAL("boundary-data-boundary-data"),
			exampleCollect, &Sink) ||
		!xrtWsDeflaterFlush(pDeflater, exampleCollect, &Sink) ||
		!xrtWsDeflaterEnd(pDeflater, exampleCollect, &Sink) ) {
		goto Cleanup;
	}
	/* InflaterSize：无消息交付时为零。 */
	if ( xrtWsInflaterSize(pInflater) != 0u ) {
		goto Cleanup;
	}
	/* DeflaterSize：当前/最近消息的线路字节数，与 Sink 计数一致。
	 * InflaterSize 才统计解压后的语义字节数。 */
	if ( (xrtWsDeflaterSize(pDeflater) == 0u) ||
		(xrtWsDeflaterSize(pDeflater) != (uint64)Sink.Size) ) {
		goto Cleanup;
	}
	/* Abort：无活动消息时主动丢弃上下文历史。 */
	if ( !xrtWsDeflaterAbort(pDeflater) ) {
		goto Cleanup;
	}
	/* Reset：复位到新连接（配置复用）。 */
	if ( !xrtWsInflaterReset(pInflater, &InflaterConfig) ||
		!xrtWsDeflaterReset(pDeflater, &DeflaterConfig) ) {
		goto Cleanup;
	}
	printf("ws: transforms reset+flush+abort size=0\n");

	/* ---- 消息配置 ---- */
	xrtWsMessageConfigInit(&MsgConfig);
	xrtWsMessageConfigInitSafe(&MsgSafe);
	if ( (MsgConfig.MaxSize != SIZE_MAX) ||
		(MsgSafe.MaxSize != (16u << 20)) ) {
		goto Cleanup;
	}
	(void)xrtWsMessageInit(&MsgState, &MsgConfig);
	xrtWsMessageReset(&MsgState);  /* 复位后可复用于新连接 */
	printf("ws: message configs ok\n");
	iResult = 0;

Cleanup:
	xrtWsDeflaterDestroy(pDeflater);
	xrtWsInflaterDestroy(pInflater);
	return iResult;
}
