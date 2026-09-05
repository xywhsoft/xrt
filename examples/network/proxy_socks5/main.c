/*
 * 范例：network/proxy_socks5 —— SOCKS5 增量握手（纯协议层）
 * ----------------------------------------------------------------
 * 演示 API：
 *   SOCKS5 greeting / CONNECT 报文生成（分步）
 * 模块宏：XRT_MODULE_PROXY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/proxy_socks5/main.c -lws2_32 -liphlpapi
 * 预期输出（十六进制线路内容）：
 *   greeting: 05 01 00
 *   connect: 05 01 00 03 0E 6F 72 69 ...
 *
 * 增量握手：greeting（无认证）→ CONNECT（域名目标）——
 *   分步生成对应状态机的两次往返；打印的是可直接
 *   上线路的二进制（03 = 域名地址类型、0x01BB = 443）。
 */


#include <stdio.h>
#include <xrt.h>



/* 打印一个二进制协议报文，便于观察纯协议层生成的线路内容。 */
static void printPacket(cstr sName, xnetspan Packet)
{
	printf("%s:", sName);
	for ( size_t i = 0; i < Packet.Size; i++ ) {
		printf(" %02X", (unsigned)Packet.Data[i]);
	}
	printf("\n");
}



/* 演示不依赖 Socket 的 SOCKS5 增量握手。 */
int main(void)
{
	static const uint8 MethodReply[] = { 0x05, 0x00 };
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;
	xnetspan Output;
	xnetbuf Input;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Host = XRT_STR_LITERAL("127.0.0.1");
	ProxyConfig.Port = 1080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		return 1;
	}

	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
	HandshakeConfig.Proxy = pProxy;
	HandshakeConfig.TargetHost = XRT_STR_LITERAL("origin.example");
	HandshakeConfig.TargetPort = 443;
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
	xrtNetProxyRelease(pProxy);
	if ( (pHandshake == NULL) || !xrtNetBufInit(&Input, NULL) ) {
		xrtNetProxyHandshakeDestroy(pHandshake);
		return 1;
	}

	if ( !xrtNetProxyHandshakeOutput(pHandshake, &Output) ) {
		xrtNetBufClear(&Input);
		xrtNetProxyHandshakeDestroy(pHandshake);
		return 1;
	}
	printPacket("greeting", Output);
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);

	if ( !xrtNetBufAppend(&Input, MethodReply, sizeof(MethodReply)) ||
		(xrtNetProxyHandshakeStep(pHandshake, &Input) !=
		 XNET_PROXY_HANDSHAKE_WRITE) ||
		!xrtNetProxyHandshakeOutput(pHandshake, &Output) ) {
		xrtNetBufClear(&Input);
		xrtNetProxyHandshakeDestroy(pHandshake);
		return 1;
	}
	printPacket("connect", Output);

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	return 0;
}
