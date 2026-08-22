#include <stdio.h>

#include <xrt.h>



/* 展示 HTTP CONNECT 握手如何与任意同步或异步传输组合。 */
int main(void)
{
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshakeconfig Config;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;
	xnetspan Output;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Type = XNET_PROXY_HTTP_CONNECT;
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.example");
	ProxyConfig.Port = 8080;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	if ( pProxy == NULL ) {
		return 1;
	}
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pProxy;
	Config.TargetHost = XRT_STR_LITERAL("origin.example");
	Config.TargetPort = 443;
	pHandshake = xrtNetProxyHandshakeCreate(&Config);
	if ( pHandshake == NULL ) {
		xrtNetProxyRelease(pProxy);
		return 2;
	}
	while ( xrtNetProxyHandshakeOutput(pHandshake, &Output) ) {
		fwrite(Output.Data, 1, Output.Size, stdout);
		(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
	}
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
	return 0;
}
