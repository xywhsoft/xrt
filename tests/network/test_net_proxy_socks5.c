#include "../test.h"



/* 创建一个 SOCKS5 代理和面向指定目标的增量握手。 */
static xnetproxyhandshake* testSocks5Create(
	xnetproxyauth Auth,
	xbytesview Username,
	xbytesview Password,
	xstrview Target,
	uint16 iPort,
	size_t iLimit
)
{
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetproxyconfig ProxyConfig;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;

	xrtNetProxyConfigInit(&ProxyConfig);
	ProxyConfig.Host = XRT_STR_LITERAL("proxy.example");
	ProxyConfig.Port = 1080;
	ProxyConfig.Auth = Auth;
	ProxyConfig.Username = Username;
	ProxyConfig.Password = Password;
	pProxy = xrtNetProxyCreate(&ProxyConfig);
	testRequire(pProxy != NULL, "SOCKS5 proxy create failed");

	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
	HandshakeConfig.Proxy = pProxy;
	HandshakeConfig.TargetHost = Target;
	HandshakeConfig.TargetPort = iPort;
	HandshakeConfig.ReceiveLimit = iLimit;
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
	xrtNetProxyRelease(pProxy);
	testRequire(pHandshake != NULL, "SOCKS5 handshake create failed");
	return pHandshake;
}



/* 比较当前输出，并允许分两次确认发送以覆盖部分写入。 */
static void testSocks5Output(
	xnetproxyhandshake* pHandshake,
	const uint8* pExpected,
	size_t iSize,
	bool bPartial
)
{
	xnetspan Output;

	testRequire(xrtNetProxyHandshakeState(pHandshake) ==
		XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 handshake is not waiting for write");
	testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output),
		"SOCKS5 output is missing");
	testRequire((Output.Size == iSize) &&
		(memcmp(Output.Data, pExpected, iSize) == 0),
		"SOCKS5 output mismatch");
	if ( bPartial && (iSize > 1) ) {
		testRequire(xrtNetProxyHandshakeSent(pHandshake, 1) == 1,
			"SOCKS5 partial output acknowledgement failed");
		testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output) &&
			(Output.Size == (iSize - 1u)) &&
			(memcmp(Output.Data, pExpected + 1, iSize - 1u) == 0),
			"SOCKS5 partial output remainder mismatch");
		testRequire(xrtNetProxyHandshakeSent(
			pHandshake, iSize - 1u
		) == (iSize - 1u),
			"SOCKS5 remaining output acknowledgement failed");
	} else {
		testRequire(xrtNetProxyHandshakeSent(pHandshake, iSize) == iSize,
			"SOCKS5 output acknowledgement failed");
	}
	testRequire(xrtNetProxyHandshakeState(pHandshake) ==
		XNET_PROXY_HANDSHAKE_READ,
		"SOCKS5 handshake did not switch to read");
}



/* 追加一段服务器输入并推进握手。 */
static xnetproxyhandshakestate testSocks5Feed(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput,
	const void* pData,
	size_t iSize
)
{
	testRequire(xrtNetBufAppend(pInput, pData, iSize),
		"SOCKS5 input append failed");
	return xrtNetProxyHandshakeStep(pHandshake, pInput);
}



/* 匿名域名 CONNECT 必须支持分片回复并保留同包应用数据。 */
static void testSocks5DomainAndLeftover(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x00 };
	static const uint8 Method[] = { 0x05, 0x00 };
	static const uint8 Request[] = {
		0x05, 0x01, 0x00, 0x03, 0x0E,
		'o', 'r', 'i', 'g', 'i', 'n', '.',
		'e', 'x', 'a', 'm', 'p', 'l', 'e',
		0x01, 0xBB
	};
	static const uint8 Reply[] = {
		0x05, 0x00, 0x00, 0x01,
		127, 0, 0, 1, 0x1F, 0x90
	};
	xnetproxyhandshake* pHandshake;
	xnetproxyendpoint Bound;
	xnetbuf Input;
	uint32 iCode;
	char sLeftover[6] = { 0 };

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 },
		XRT_STR_LITERAL("origin.example"), 443, 65536
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 input init failed");
	testSocks5Output(pHandshake, Greeting, sizeof(Greeting), true);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method, 1
	) == XNET_PROXY_HANDSHAKE_READ,
		"fragmented SOCKS5 method completed too early");
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method + 1, 1
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"fragmented SOCKS5 method did not complete");
	testSocks5Output(pHandshake, Request, sizeof(Request), false);

	for ( size_t i = 0; i + 1u < sizeof(Reply); i++ ) {
		testRequire(testSocks5Feed(
			pHandshake, &Input, Reply + i, 1
		) == XNET_PROXY_HANDSHAKE_READ,
			"fragmented SOCKS5 CONNECT completed too early");
	}
	testRequire(xrtNetBufAppend(&Input, Reply + sizeof(Reply) - 1u, 1) &&
		xrtNetBufAppend(&Input, "hello", 5),
		"SOCKS5 coalesced final input append failed");
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_READY,
		"SOCKS5 CONNECT did not become ready");
	testRequire((xrtNetBufSize(&Input) == 5) &&
		(xrtNetBufRead(&Input, sLeftover, 5) == 5) &&
		(memcmp(sLeftover, "hello", 5) == 0),
		"SOCKS5 handshake consumed application leftovers");
	testRequire(xrtNetProxyHandshakeBound(pHandshake, &Bound) &&
		(Bound.Address.Family == XNET_FAMILY_IPV4) &&
		(Bound.Address.Port == 8080) &&
		(Bound.Address.Address[0] == 127) &&
		(Bound.Address.Address[3] == 1),
		"SOCKS5 IPv4 bound endpoint mismatch");
	testRequire(xrtNetProxyHandshakeCode(pHandshake, &iCode) &&
		(iCode == XNET_SOCKS5_SUCCEEDED),
		"SOCKS5 success reply code mismatch");

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 必需认证只允许用户名密码，并正确编码数字 IPv4 目标。 */
static void testSocks5RequiredAuth(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x02 };
	static const uint8 Method[] = { 0x05, 0x02 };
	static const uint8 Auth[] = {
		0x01, 0x04, 'u', 's', 'e', 'r',
		0x04, 'p', 'a', 's', 's'
	};
	static const uint8 AuthReply[] = { 0x01, 0x00 };
	static const uint8 Request[] = {
		0x05, 0x01, 0x00, 0x01,
		192, 0, 2, 10, 0x23, 0x28
	};
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_REQUIRED,
		XRT_BYTES_LITERAL("user"),
		XRT_BYTES_LITERAL("pass"),
		XRT_STR_LITERAL("192.0.2.10"), 9000, 65536
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 auth input init failed");
	testSocks5Output(pHandshake, Greeting, sizeof(Greeting), false);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method, sizeof(Method)
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 password method did not produce auth output");
	testSocks5Output(pHandshake, Auth, sizeof(Auth), true);
	testRequire(testSocks5Feed(
		pHandshake, &Input, AuthReply, sizeof(AuthReply)
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 auth success did not produce CONNECT");
	testSocks5Output(pHandshake, Request, sizeof(Request), false);

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 可选认证显式提供两个方法，并允许服务器选择匿名。 */
static void testSocks5OptionalAuth(void)
{
	static const uint8 Greeting[] = { 0x05, 0x02, 0x00, 0x02 };
	static const uint8 Method[] = { 0x05, 0x00 };
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_OPTIONAL,
		XRT_BYTES_LITERAL("user"),
		XRT_BYTES_LITERAL("pass"),
		XRT_STR_LITERAL("origin.example"), 80, 65536
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 optional input init failed");
	testSocks5Output(pHandshake, Greeting, sizeof(Greeting), false);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method, sizeof(Method)
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 optional anonymous selection failed");

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 数字 IPv6 目标必须使用 16 字节线路地址而不是域名回退。 */
static void testSocks5IPv6Target(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x00 };
	static const uint8 Method[] = { 0x05, 0x00 };
	static const uint8 Request[] = {
		0x05, 0x01, 0x00, 0x04,
		0x20, 0x01, 0x0D, 0xB8,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 1, 0x01, 0xBB
	};
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 },
		XRT_STR_LITERAL("2001:db8::1"), 443, 65536
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 IPv6 input init failed");
	testSocks5Output(pHandshake, Greeting, sizeof(Greeting), false);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method, sizeof(Method)
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 IPv6 method negotiation failed");
	testSocks5Output(pHandshake, Request, sizeof(Request), false);

	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 所有标准失败码必须保留线路值并进入带结构化错误的终态。 */
static void testSocks5FailureCodes(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x00 };
	static const uint8 Method[] = { 0x05, 0x00 };

	for ( uint8 iReply = 1; iReply <= 8; iReply++ ) {
		xnetproxyhandshake* pHandshake = testSocks5Create(
			XNET_PROXY_AUTH_NONE,
			(xbytesview){ NULL, 0 },
			(xbytesview){ NULL, 0 },
			XRT_STR_LITERAL("origin.example"), 80, 65536
		);
		xnetbuf Input;
		uint8 Reply[] = { 0x05, iReply, 0x00, 0x01 };
		uint32 iCode = 0;

		testRequire(xrtNetBufInit(&Input, NULL),
			"SOCKS5 failure input init failed");
		testSocks5Output(pHandshake, Greeting, sizeof(Greeting), false);
		testRequire(testSocks5Feed(
			pHandshake, &Input, Method, sizeof(Method)
		) == XNET_PROXY_HANDSHAKE_WRITE,
			"SOCKS5 failure setup method failed");
		{
			xnetspan Output;

			testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output),
				"SOCKS5 failure CONNECT output missing");
			testRequire(xrtNetProxyHandshakeSent(
				pHandshake, Output.Size
			) == Output.Size,
				"SOCKS5 failure CONNECT output acknowledgement failed");
		}
		testRequire(testSocks5Feed(
			pHandshake, &Input, Reply, sizeof(Reply)
		) == XNET_PROXY_HANDSHAKE_ERROR,
			"SOCKS5 failure reply did not fail");
		testRequire(xrtNetProxyHandshakeCode(pHandshake, &iCode) &&
			(iCode == iReply),
			"SOCKS5 failure reply code was not preserved");
		testRequire((xrtNetProxyHandshakeError(pHandshake) != NULL) &&
			(xrtErrorCode(
				xrtNetProxyHandshakeError(pHandshake)
			) == XNET_ERROR_PROXY_CONNECT),
			"SOCKS5 failure structured error mismatch");

		xrtNetBufClear(&Input);
		xrtNetProxyHandshakeDestroy(pHandshake);
	}
}



/* 必需认证拒绝匿名降级，回复长度也必须遵守调用方硬上限。 */
static void testSocks5SecurityAndLimits(void)
{
	static const uint8 Greeting[] = { 0x05, 0x01, 0x02 };
	static const uint8 Anonymous[] = { 0x05, 0x00 };
	static const uint8 NoAuthGreeting[] = { 0x05, 0x01, 0x00 };
	static const uint8 Method[] = { 0x05, 0x00 };
	static const uint8 DomainReply[] = {
		0x05, 0x00, 0x00, 0x03, 0x03,
		'f', 'o', 'o', 0x00, 0x50
	};
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_REQUIRED,
		XRT_BYTES_LITERAL("user"),
		XRT_BYTES_LITERAL("pass"),
		XRT_STR_LITERAL("origin.example"), 80, 65536
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 downgrade input init failed");
	testSocks5Output(pHandshake, Greeting, sizeof(Greeting), false);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Anonymous, sizeof(Anonymous)
	) == XNET_PROXY_HANDSHAKE_ERROR,
		"SOCKS5 required auth accepted anonymous downgrade");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);

	pHandshake = testSocks5Create(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 },
		XRT_STR_LITERAL("origin.example"), 80, 8
	);
	testRequire(xrtNetBufInit(&Input, NULL),
		"SOCKS5 limit input init failed");
	testSocks5Output(
		pHandshake, NoAuthGreeting, sizeof(NoAuthGreeting), false
	);
	testRequire(testSocks5Feed(
		pHandshake, &Input, Method, sizeof(Method)
	) == XNET_PROXY_HANDSHAKE_WRITE,
		"SOCKS5 limit setup method failed");
	{
		xnetspan Output;

		testRequire(xrtNetProxyHandshakeOutput(pHandshake, &Output),
			"SOCKS5 limit CONNECT output missing");
		(void)xrtNetProxyHandshakeSent(pHandshake, Output.Size);
	}
	testRequire(testSocks5Feed(
		pHandshake, &Input, DomainReply, sizeof(DomainReply)
	) == XNET_PROXY_HANDSHAKE_ERROR,
		"SOCKS5 receive limit was not enforced");
	testRequire((xrtNetProxyHandshakeError(pHandshake) != NULL) &&
		(xrtErrorCode(
			xrtNetProxyHandshakeError(pHandshake)
		) == XNET_ERROR_PROXY_LIMIT),
		"SOCKS5 receive limit error mismatch");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
}



/* 执行 SOCKS5 协议状态机边界回归。 */
int main(void)
{
	testSocks5DomainAndLeftover();
	testSocks5RequiredAuth();
	testSocks5OptionalAuth();
	testSocks5IPv6Target();
	testSocks5FailureCodes();
	testSocks5SecurityAndLimits();
	return 0;
}
