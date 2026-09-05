/*
 * 范例：network/proxy_http_connect —— CONNECT 握手纯协议层
 * ----------------------------------------------------------------
 * 演示 API：
 *   HTTP CONNECT 请求生成（与任意传输组合）
 * 模块宏：XRT_MODULE_PROXY
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/proxy_http_connect/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   CONNECT origin.example:443 HTTP/1.1
 *   Host: origin.example:443
 *
 * 纯协议层：只生成握手字节，不依赖 Socket——
 *   同步/异步传输都能接（与 tls/server 的
 *   "传输无关"设计同一哲学）。
 */


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
