#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留 HTTP CONNECT 请求、响应和隧道剩余字节。 */
int main(void)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\nProxy-Agent: single\r\n\r\nx";
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshakeconfig Config;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;
	xnetbuf Input;
	xnetspan Output;
	char Suffix;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.test");
	ProxyConfig.Port = 8080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		return 1;
	}
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pProxy;
	Config.TargetHost = XRT_STR_LITERAL("origin.test");
	Config.TargetPort = 443;
	pHandshake = xrtNetProxyHandshakeCreate(&Config);
	if ( (pHandshake == NULL) ||
		!xrtNetProxyHandshakeOutput(pHandshake, &Output) ||
		(xrtNetProxyHandshakeSent(pHandshake, Output.Size) != Output.Size) ) {
		return 2;
	}
	if ( !xrtNetBufInit(&Input, NULL) ||
		!xrtNetBufAppend(&Input, Response, sizeof(Response) - 1u) ||
		(xrtNetProxyHandshakeStep(pHandshake, &Input) !=
		 XNET_PROXY_HANDSHAKE_READY) ||
		(xrtNetBufRead(&Input, &Suffix, 1) != 1) || (Suffix != 'x') ) {
		return 3;
	}
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
	return 0;
}
