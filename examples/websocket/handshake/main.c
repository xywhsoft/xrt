#include <stdio.h>

#include <xrt.h>



/*
 * 范例：websocket/handshake —— Accept 键计算与子协议协商
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWsAccept           Sec-WebSocket-Accept 规范计算
 *   xrtWsProtocolSelect   客户端提议 ∩ 服务端支持
 * 模块宏：XRT_MODULE_WEBSOCKET（依赖 CRYPTO SHA1）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/websocket/handshake/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   accept=s3pPLMBiTxaQ9kYGzzhZRbK+xOo= protocol=superchat
 *
 * accept 值是 RFC 6455 的标准测试向量：样例 nonce
 *   "dGhlIHNhbXBsZSBub25jZQ==" 经 SHA1(magic) + Base64
 *   必得 s3pPLMBiTxaQ9kYGzzhZRbK+xOo= ——对上即证明实现正确。
 * 协议选择取客户端偏好顺序中服务端也支持的第一个（superchat）。
 */


/* 展示无需网络对象的 RFC Accept 计算与子协议协商。 */
int main(void)
{
	char Accept[XWS_ACCEPT_CAPACITY];
	xstrview Selected;

	if ( !xrtWsAccept(
		XRT_STR_LITERAL("dGhlIHNhbXBsZSBub25jZQ=="),
		Accept,
		sizeof(Accept)
	) ) {
		return 1;
	}
	if ( !xrtWsProtocolSelect(
		XRT_STR_LITERAL("chat, superchat"),
		XRT_STR_LITERAL("superchat, binary"),
		&Selected
	) ) {
		return 2;
	}
	printf(
		"accept=%s protocol=%.*s\n",
		Accept,
		(int)Selected.Size,
		Selected.Data != NULL ? Selected.Data : ""
	);
	return 0;
}
