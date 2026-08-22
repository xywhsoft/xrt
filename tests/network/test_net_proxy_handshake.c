#include "../test.h"



/* 创建一份供通用握手边界使用的不可变 SOCKS5 代理配置。 */
static xnetproxy* testProxyHandshakeProxy(void)
{
	xnetproxyconfig Config;

	xrtNetProxyConfigInit(&Config);
	Config.Host = XRT_STR_LITERAL("proxy.example");
	Config.Port = 1080;
	return xrtNetProxyCreate(&Config);
}



/* 默认配置必须保持空所有权，并提供统一的协议接收硬上限。 */
static void testProxyHandshakeDefaults(void)
{
	xnetproxyhandshakeconfig Config;

	memset(&Config, 0xA5, sizeof(Config));
	xrtNetProxyHandshakeConfigInit(&Config);
	testRequire((Config.Proxy == NULL) &&
		(Config.TargetHost.Data == NULL) &&
		(Config.TargetHost.Size == 0) &&
		(Config.TargetPort == 0) &&
		(Config.ReceiveLimit == 65536u) &&
		(Config.Pool == NULL),
		"proxy handshake defaults mismatch");
}



/* 配置验证必须拒绝空目标、嵌入零字节、非法端口和过小接收上限。 */
static void testProxyHandshakeValidation(void)
{
	char sEmbedded[] = { 'a', 0, 'b' };
	char sLong[1025];
	xnetproxyhandshakeconfig Config;
	xnetproxy* pProxy = testProxyHandshakeProxy();

	testRequire(pProxy != NULL,
		"proxy handshake validation proxy create failed");
	xrtNetProxyHandshakeConfigInit(&Config);
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted a missing proxy");

	Config.Proxy = pProxy;
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted an empty target");
	Config.TargetHost = XRT_STR_LITERAL("origin.example");
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted a zero target port");

	Config.TargetPort = 443;
	Config.ReceiveLimit = 3;
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted a tiny receive limit");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PROXY_LIMIT),
		"proxy handshake tiny limit error mismatch");

	Config.ReceiveLimit = 65536;
	Config.TargetHost.Data = sEmbedded;
	Config.TargetHost.Size = sizeof(sEmbedded);
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted a null byte in the target");

	memset(sLong, 'a', sizeof(sLong));
	Config.TargetHost.Data = sLong;
	Config.TargetHost.Size = sizeof(sLong);
	testRequire(xrtNetProxyHandshakeCreate(&Config) == NULL,
		"proxy handshake accepted an oversized target");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PROXY_LIMIT),
		"proxy handshake oversized target error mismatch");

	xrtNetProxyRelease(pProxy);
}



/* 无协议后端的裁剪闭包必须明确返回不支持，而不是留下半初始化对象。 */
static void testProxyHandshakeBackendGate(void)
{
	#if !defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		xnetproxyhandshakeconfig Config;
		xnetproxyhandshake* pHandshake;
		xnetproxy* pProxy = testProxyHandshakeProxy();

		testRequire(pProxy != NULL,
			"proxy handshake backend proxy create failed");
		xrtNetProxyHandshakeConfigInit(&Config);
		Config.Proxy = pProxy;
		Config.TargetHost = XRT_STR_LITERAL("origin.example");
		Config.TargetPort = 443;
		pHandshake = xrtNetProxyHandshakeCreate(&Config);
		testRequire(pHandshake == NULL,
			"proxy handshake without a backend unexpectedly succeeded");
		testRequire((xrtGetError() != NULL) &&
			(xrtErrorCode(xrtGetError()) ==
			 XNET_ERROR_PROXY_UNSUPPORTED),
			"proxy handshake missing backend error mismatch");
		xrtNetProxyRelease(pProxy);
	#endif
}



/* 输出查询失败必须清空 Span，其他空对象查询也必须保持稳定结果。 */
static void testProxyHandshakeInvalidQueries(void)
{
	xnetproxyendpoint Endpoint;
	xnetspan Output;
	uint32 iCode = 0;

	Output.Data = (const void*)(uintptr_t)1u;
	Output.Size = 99;
	testRequire(!xrtNetProxyHandshakeOutput(NULL, &Output) &&
		(Output.Data == NULL) && (Output.Size == 0),
		"proxy handshake invalid output was not normalized");
	testRequire(xrtNetProxyHandshakeState(NULL) ==
		XNET_PROXY_HANDSHAKE_ERROR,
		"proxy handshake null state mismatch");
	testRequire(xrtNetProxyHandshakeSent(NULL, 1) == 0,
		"proxy handshake null acknowledgement succeeded");
	testRequire(!xrtNetProxyHandshakeBound(NULL, &Endpoint),
		"proxy handshake null bound query succeeded");
	testRequire(!xrtNetProxyHandshakeCode(NULL, &iCode),
		"proxy handshake null code query succeeded");
	testRequire(xrtNetProxyHandshakeError(NULL) == NULL,
		"proxy handshake null error query mismatch");
}



#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)

/* 成功路径验证目标深拷贝、部分确认、剩余输入和终态查询。 */
static void testProxyHandshakeLifecycle(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x00 };
	static const uint8 Method[] = { 0x05, 0x00 };
	static const uint8 Reply[] = {
		0x05, 0x00, 0x00, 0x01,
		127, 0, 0, 1, 0x01, 0xBB
	};
	char sTarget[] = "origin.example";
	xnetproxyhandshakeconfig Config;
	xnetproxyendpoint Bound;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy = testProxyHandshakeProxy();
	xnetbuf Input;
	xnetspan Output;
	uint32 iCode;
	char iLeftover;

	testRequire(pProxy != NULL,
		"proxy handshake lifecycle proxy create failed");
	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pProxy;
	Config.TargetHost.Data = sTarget;
	Config.TargetHost.Size = strlen(sTarget);
	Config.TargetPort = 443;
	pHandshake = xrtNetProxyHandshakeCreate(&Config);
	xrtNetProxyRelease(pProxy);
	testRequire(pHandshake != NULL,
		"proxy handshake lifecycle create failed");
	memset(sTarget, 'x', strlen(sTarget));

	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(Output.Size == sizeof(Greeting)) &&
		(memcmp(Output.Data, Greeting, sizeof(Greeting)) == 0),
		"proxy handshake initial output mismatch");
	xrtClearError();
	testRequire((xrtNetProxyHandshakeSent(
		pHandshake, Output.Size + 1u
	) == 0) && (xrtGetError() != NULL) &&
		(xrtNetProxyHandshakeState(pHandshake) ==
		 XNET_PROXY_HANDSHAKE_WRITE),
		"proxy handshake accepted an oversized acknowledgement");
	testRequire((xrtNetProxyHandshakeSent(pHandshake, 1) == 1) &&
		xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(Output.Size == (sizeof(Greeting) - 1u)) &&
		(xrtNetProxyHandshakeSent(
			pHandshake, Output.Size
		) == Output.Size),
		"proxy handshake partial acknowledgement failed");
	Output.Data = (const void*)(uintptr_t)1u;
	Output.Size = 99;
	testRequire(!xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(Output.Data == NULL) && (Output.Size == 0),
		"proxy handshake empty output was not normalized");

	xrtClearError();
	testRequire((xrtNetProxyHandshakeStep(pHandshake, NULL) ==
		XNET_PROXY_HANDSHAKE_READ) &&
		(xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"proxy handshake null input corrupted its state");
	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppend(&Input, Method, sizeof(Method)) &&
		(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		 XNET_PROXY_HANDSHAKE_WRITE),
		"proxy handshake method step failed");
	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
		(Output.Size >= (5u + strlen("origin.example"))) &&
		(memcmp(
			(const uint8*)Output.Data + 5u,
			"origin.example",
			strlen("origin.example")
		) == 0),
		"proxy handshake did not deep-copy the target");
	testRequire(xrtNetProxyHandshakeSent(
		pHandshake, Output.Size
	) == Output.Size,
		"proxy handshake CONNECT acknowledgement failed");

	testRequire(xrtNetBufAppend(&Input, Reply, sizeof(Reply)) &&
		xrtNetBufAppend(&Input, "x", 1) &&
		(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		 XNET_PROXY_HANDSHAKE_READY),
		"proxy handshake did not reach ready");
	testRequire((xrtNetBufRead(&Input, &iLeftover, 1) == 1) &&
		(iLeftover == 'x'),
		"proxy handshake consumed tunnel leftovers");
	testRequire(xrtNetProxyHandshakeBound(pHandshake, &Bound) &&
		(Bound.Address.Family == XNET_FAMILY_IPV4) &&
		(Bound.Address.Port == 443),
		"proxy handshake bound endpoint mismatch");
	testRequire(xrtNetProxyHandshakeCode(pHandshake, &iCode) &&
		(iCode == XNET_SOCKS5_SUCCEEDED),
		"proxy handshake reply code mismatch");
	testRequire(xrtNetProxyHandshakeError(pHandshake) == NULL,
		"proxy handshake success retained an error");

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}

#endif



/* 执行传输无关代理握手的通用边界回归。 */
int main(void)
{
	testProxyHandshakeDefaults();
	testProxyHandshakeValidation();
	testProxyHandshakeBackendGate();
	testProxyHandshakeInvalidQueries();
	#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)
		testProxyHandshakeLifecycle();
	#endif
	printf("[PASS] net proxy handshake\n");
	return 0;
}
