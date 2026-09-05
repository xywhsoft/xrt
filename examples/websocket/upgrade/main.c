#include <stdio.h>

#include <xrt.h>



/*
 * 范例：websocket/upgrade —— 客户端升级请求一键生成
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsKeyGenerate               随机 16 字节 → Base64 Sec-Key
 *   xrtWsUpgradeRequestFields      生成 Upgrade 全套请求字段
 *   xrtHttp1RequestWrite           字段 → 完整 HTTP/1.1 请求字节
 * 模块宏：XRT_MODULE_WEBSOCKET_UPGRADE（依赖 HTTP1）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/upgrade/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   GET /socket HTTP/1.1
 *   Host: example.test
 *   Upgrade: websocket
 *   （后续还含 Connection/Sec-WebSocket-Key/Version/Protocol 行）
 *
 * 三步拼出合法升级请求：随机 Key（每次不同）→ 必需字段
 *   （含子协议 chat）→ 复用 HTTP/1 写出器封包。
 *   产物可直接交给 TCP/TLS 流发送——无网络对象参与，
 *   协议核心与传输解耦的设计体现。
 */


/* 生成可直接交给 TCP 或 TLS Stream 发送的 WebSocket 请求 Header。 */
int main(void)
{
	char Key[XWS_KEY_CAPACITY];
	char Request[1024];
	xhttpfield Fields[XWS_UPGRADE_REQUEST_FIELDS_MAX];
	size_t iFieldCount = 0;
	size_t iRequest = 0;

	if ( !xrtWsKeyGenerate(Key, sizeof(Key)) ||
		!xrtWsUpgradeRequestFields(
			XRT_STR_LITERAL("example.test"),
			(xstrview) { Key, XWS_KEY_SIZE },
			XRT_STR_LITERAL("chat"),
			(xstrview) { 0 },
			Fields,
			sizeof(Fields) / sizeof(Fields[0]),
			&iFieldCount
		) || !xrtHttp1RequestWrite(
			XRT_STR_LITERAL("GET"),
			XRT_STR_LITERAL("/socket"),
			XHTTP_VERSION_1_1,
			Fields,
			iFieldCount,
			Request,
			sizeof(Request),
			&iRequest
		) ) {
		return 1;
	}
	printf("%.*s", (int)iRequest, Request);
	return 0;
}
