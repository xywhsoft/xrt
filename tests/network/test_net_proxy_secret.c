#include "../test.h"
#include "../../src/internal/xrt_proxy.h"



/* 在缓冲池缓存中查找刚刚释放的拥有块。 */
static xnetblock* testProxySecretCachedBlock(xnetbufpool* pPool)
{
	for ( uint32 i = 0; i < XNET_BUFFER_CLASS_COUNT; i++ ) {
		if ( pPool->Free[i] != NULL ) {
			return pPool->Free[i];
		}
	}
	return NULL;
}



/* 在二进制报文中查找一段敏感标记。 */
static bool testProxySecretContains(
	cbytes pData,
	size_t iSize,
	cbytes pNeedle,
	size_t iNeedleSize
)
{
	if ( iNeedleSize > iSize ) {
		return false;
	}
	for ( size_t i = 0; i <= (iSize - iNeedleSize); i++ ) {
		if ( memcmp(pData + i, pNeedle, iNeedleSize) == 0 ) {
			return true;
		}
	}
	return false;
}



/* 已发送的用户名密码报文必须在返回缓冲池前清零。 */
int main(void)
{
	static const uint8 Method[] = { 0x05, 0x02 };
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetbufpoolconfig PoolConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshake* pHandshake;
	xnetbufpool* pPool;
	xnetproxy* pProxy;
	xnetblock* pBlock;
	xnetspan Output;
	xnetbuf Input;
	size_t iAuthSize;

	xrtNetBufPoolConfigInit(&PoolConfig);
	pPool = xrtNetBufPoolCreate(&PoolConfig);
	testRequire(pPool != NULL, "proxy secret buffer pool create failed");
	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.example");
	ProxyConfig.Port = 1080;
	ProxyConfig.Username = XRT_BYTES_LITERAL("sensitive-user");
	ProxyConfig.Password = XRT_BYTES_LITERAL("sensitive-password");
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(pProxy != NULL, "proxy secret endpoint create failed");

	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
	HandshakeConfig.Proxy = pProxy;
	HandshakeConfig.TargetHost = XRT_STR_LITERAL("origin.example");
	HandshakeConfig.TargetPort = 443;
	HandshakeConfig.Pool = pPool;
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
	xrtNetProxyRelease(pProxy);
	testRequire((pHandshake != NULL) && xrtNetBufInit(&Input, NULL),
		"proxy secret handshake create failed");

	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output),
		"proxy secret greeting output missing");
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
	testRequire(xrtNetBufAppend(&Input, Method, sizeof(Method)) &&
		(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		 XNET_PROXY_HANDSHAKE_WRITE),
		"proxy secret auth output was not generated");
	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		testProxySecretContains(
			Output.Data, Output.Size,
			(cbytes)"sensitive-password", 18
		),
		"proxy secret password is missing from auth packet");
	iAuthSize = Output.Size;
	(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);

	pBlock = testProxySecretCachedBlock(pPool);
	testRequire(pBlock != NULL,
		"proxy secret output block was not returned to the pool");
	for ( size_t i = 0; i < iAuthSize; i++ ) {
		testRequire(pBlock->Data[i] == 0,
			"proxy secret output remained in a cached block");
	}

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	testRequire(xrtNetBufPoolDestroy(pPool),
		"proxy secret buffer pool destroy failed");
	return 0;
}
