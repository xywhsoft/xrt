#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头裁剪必须保留通用握手配置，并明确拒绝没有编译的协议后端。 */
int main(void)
{
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;

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
	#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		if ( pHandshake == NULL ) {
			return 2;
		}
		xrtNetProxyHandshakeDestroy(pHandshake);
	#else
		if ( pHandshake != NULL ) {
			xrtNetProxyHandshakeDestroy(pHandshake);
			return 2;
		}
		if ( (xrtGetError() == NULL) ||
			(xrtErrorCode(xrtGetError()) !=
			 XNET_ERROR_PROXY_UNSUPPORTED) ) {
			return 3;
		}
	#endif
	return 0;
}
