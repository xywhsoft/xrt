#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须能够创建并启动一个不依赖 Socket 的 SOCKS5 握手。 */
int main(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x00 };
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;
	xnetspan Output;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.example");
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
	if ( (pHandshake == NULL) ||
		!xrtNetProxyHandshakeOutput(pHandshake, &Output) ||
		(Output.Size != sizeof(Greeting)) ||
		(memcmp(Output.Data, Greeting, sizeof(Greeting)) != 0) ) {
		xrtNetProxyHandshakeDestroy(pHandshake);
		return 1;
	}
	xrtNetProxyHandshakeDestroy(pHandshake);
	return 0;
}
