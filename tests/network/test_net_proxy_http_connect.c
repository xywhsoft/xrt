#include "../test.h"



/* 创建一个由测试独占的 HTTP CONNECT 代理对象。 */
static xnetproxy* testHttpProxy(
	xnetproxyauth Auth,
	xbytesview Username,
	xbytesview Password
)
{
	xnetproxyconfig Config;

	xrtNetProxyConfigInit(&Config);
	Config.Type = XNET_PROXY_HTTP_CONNECT;
	Config.Host = XRT_STR_LITERAL("proxy.test");
	Config.Port = 8080;
	Config.Auth = Auth;
	Config.Username = Username;
	Config.Password = Password;
	return xrtNetProxyCreate(&Config);
}



/* 创建握手并把目标端点复制到对象中。 */
static xnetproxyhandshake* testHttpHandshake(
	xnetproxy* pProxy,
	xstrview Host,
	uint16 iPort,
	size_t iReceiveLimit
)
{
	xnetproxyhandshakeconfig Config;

	xrtNetProxyHandshakeConfigInit(&Config);
	Config.Proxy = pProxy;
	Config.TargetHost = Host;
	Config.TargetPort = iPort;
	if ( iReceiveLimit != 0 ) {
		Config.ReceiveLimit = iReceiveLimit;
	}
	return xrtNetProxyHandshakeCreate(&Config);
}



/* 复制并确认全部待发送报文，返回线路长度。 */
static size_t testHttpOutput(
	xnetproxyhandshake* pHandshake,
	char* sOutput,
	size_t iCapacity,
	bool bSplit
)
{
	xnetspan Span;
	size_t iSize = 0;

	while ( xrtNetProxyHandshakeOutput(pHandshake, &Span) ) {
		size_t iTake = Span.Size;

		if ( bSplit && (iTake > 1) ) {
			iTake /= 2u;
			bSplit = false;
		}
		testRequire(iTake <= (iCapacity - iSize),
			"HTTP CONNECT output fixture overflow");
		memcpy(sOutput + iSize, Span.Data, iTake);
		iSize += iTake;
		testRequire(xrtNetProxyHandshakeSent(
			pHandshake, iTake
		) == iTake, "HTTP CONNECT output acknowledgement failed");
	}
	return iSize;
}



/* 创建已确认请求输出且正在等待响应的握手。 */
static xnetproxyhandshake* testHttpReadyToRead(
	xnetproxy* pProxy,
	xstrview Host,
	uint16 iPort,
	size_t iReceiveLimit
)
{
	char Output[4096];
	xnetproxyhandshake* pHandshake = testHttpHandshake(
		pProxy, Host, iPort, iReceiveLimit
	);

	testRequire(pHandshake != NULL, "HTTP CONNECT handshake create failed");
	(void)testHttpOutput(pHandshake, Output, sizeof(Output), false);
	testRequire(xrtNetProxyHandshakeState(pHandshake) ==
		XNET_PROXY_HANDSHAKE_READ,
		"HTTP CONNECT handshake did not enter READ");
	return pHandshake;
}



/* 验证匿名请求使用共享封包器生成最小标准 Header。 */
static void testHttpAnonymousRequest(void)
{
	static const char Expected[] =
		"CONNECT origin.test:443 HTTP/1.1\r\n"
		"Host: origin.test:443\r\n\r\n";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake;
	char Output[256];
	size_t iSize;

	testRequire(pProxy != NULL, "HTTP CONNECT anonymous proxy create failed");
	pHandshake = testHttpHandshake(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
	);
	testRequire(pHandshake != NULL, "HTTP CONNECT anonymous handshake failed");
	iSize = testHttpOutput(
		pHandshake, Output, sizeof(Output), true
	);
	testRequire((iSize == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP CONNECT anonymous request mismatch");
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 验证 Basic 凭据按 user:password 编码，且不会使用固定临时数组。 */
static void testHttpBasicRequest(void)
{
	static const char Expected[] =
		"CONNECT origin.test:8443 HTTP/1.1\r\n"
		"Host: origin.test:8443\r\n"
		"Proxy-Authorization: Basic YWxpY2U6c2VjcmV0\r\n\r\n";
	static const uint8 User[] = "alice";
	static const uint8 Password[] = "secret";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_REQUIRED,
		(xbytesview){ User, sizeof(User) - 1u },
		(xbytesview){ Password, sizeof(Password) - 1u }
	);
	xnetproxyhandshake* pHandshake;
	char Output[512];
	size_t iSize;

	testRequire(pProxy != NULL, "HTTP CONNECT Basic proxy create failed");
	pHandshake = testHttpHandshake(
		pProxy, XRT_STR_LITERAL("origin.test"), 8443, 0
	);
	testRequire(pHandshake != NULL, "HTTP CONNECT Basic handshake failed");
	iSize = testHttpOutput(pHandshake, Output, sizeof(Output), false);
	testRequire((iSize == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP CONNECT Basic request mismatch");
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 验证 IPv6 方括号与作用域百分号采用规范 authority 线路形式。 */
static void testHttpIpv6Authority(void)
{
	static const char Expected[] =
		"CONNECT [fe80::1%253]:443 HTTP/1.1\r\n"
		"Host: [fe80::1%253]:443\r\n\r\n";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake;
	char Output[256];
	size_t iSize;

	testRequire(pProxy != NULL, "HTTP CONNECT IPv6 proxy create failed");
	pHandshake = testHttpHandshake(
		pProxy, XRT_STR_LITERAL("fe80::1%3"), 443, 0
	);
	testRequire(pHandshake != NULL, "HTTP CONNECT IPv6 handshake failed");
	iSize = testHttpOutput(pHandshake, Output, sizeof(Output), false);
	testRequire((iSize == sizeof(Expected) - 1u) &&
		(memcmp(Output, Expected, iSize) == 0),
		"HTTP CONNECT IPv6 authority mismatch");
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 一个完整 2xx Header 只被消费到空行，隧道应用字节保持原位。 */
static void testHttpSuccessAndLeftover(void)
{
	static const char Response[] =
		"HTTP/1.1 204 Tunnel Ready\r\n"
		"Proxy-Agent: xrt-test\r\n"
		"Transfer-Encoding: chunked\r\n"
		"Content-Length: 99\r\n\r\nTLS";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake;
	xnetbuf Input;
	char Leftover[3];
	uint32 iCode = 0;
	xnetproxyendpoint Bound;

	testRequire(pProxy != NULL, "HTTP CONNECT success proxy create failed");
	pHandshake = testHttpReadyToRead(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
	);
	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppend(&Input, Response, sizeof(Response) - 1u),
		"HTTP CONNECT success input build failed");
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_READY,
		"HTTP CONNECT 2xx response or framing precedence was rejected");
	testRequire(xrtNetProxyHandshakeCode(pHandshake, &iCode) &&
		(iCode == 204), "HTTP CONNECT success code mismatch");
	testRequire((xrtNetBufSize(&Input) == sizeof(Leftover)) &&
		(xrtNetBufRead(&Input, Leftover, sizeof(Leftover)) ==
		 sizeof(Leftover)) &&
		(memcmp(Leftover, "TLS", sizeof(Leftover)) == 0),
		"HTTP CONNECT consumed tunneled bytes");
	xrtClearError();
	testRequire(!xrtNetProxyHandshakeBound(pHandshake, &Bound) &&
		(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND),
		"HTTP CONNECT fabricated a bound endpoint");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 每个响应分片点都必须保持 READ，完整后得到相同成功结果。 */
static void testHttpFragmentation(void)
{
	static const char Response[] =
		"HTTP/1.1 200 Connection Established\r\n"
		"Proxy-Agent: split-test\r\n\r\n";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	size_t i;

	testRequire(pProxy != NULL, "HTTP CONNECT split proxy create failed");
	for ( i = 0; i <= (sizeof(Response) - 1u); i++ ) {
		xnetproxyhandshake* pHandshake = testHttpReadyToRead(
			pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
		);
		xnetbuf Input;

		testRequire(xrtNetBufInit(&Input, NULL),
			"HTTP CONNECT split buffer init failed");
		if ( i != 0 ) {
			testRequire(xrtNetBufAppend(&Input, Response, i),
				"HTTP CONNECT first split append failed");
		}
		if ( i < (sizeof(Response) - 1u) ) {
			testRequire(xrtNetProxyHandshakeStep(
				pHandshake, &Input
			) == XNET_PROXY_HANDSHAKE_READ,
				"HTTP CONNECT partial response did not wait");
			testRequire(xrtNetBufAppend(
				&Input,
				Response + i,
				(sizeof(Response) - 1u) - i
			), "HTTP CONNECT second split append failed");
		}
		testRequire(xrtNetProxyHandshakeStep(
			pHandshake, &Input
		) == XNET_PROXY_HANDSHAKE_READY,
			"HTTP CONNECT fragmented response failed");
		xrtNetBufClear(&Input);
		xrtNetProxyHandshakeDestroy(pHandshake);
	}
	xrtNetProxyRelease(pProxy);
}



/* 100 等中间响应可以连续出现，但最终仍必须是 2xx。 */
static void testHttpInterimResponse(void)
{
	static const char Response[] =
		"HTTP/1.1 100 Continue\r\nX-Step: one\r\n\r\n"
		"HTTP/1.1 200 OK\r\n\r\nnext";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake = testHttpReadyToRead(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
	);
	xnetbuf Input;
	char Suffix[4];

	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppend(&Input, Response, sizeof(Response) - 1u),
		"HTTP CONNECT interim input build failed");
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_READY,
		"HTTP CONNECT interim response sequence failed");
	testRequire((xrtNetBufRead(&Input, Suffix, sizeof(Suffix)) ==
		sizeof(Suffix)) && (memcmp(Suffix, "next", 4) == 0),
		"HTTP CONNECT interim sequence consumed tunnel suffix");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 中间响应数量有固定上限，防止代理无限延长握手。 */
static void testHttpInterimLimit(void)
{
	static const char Interim[] = "HTTP/1.1 100 Continue\r\n\r\n";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake = testHttpReadyToRead(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
	);
	xnetbuf Input;
	size_t i;

	testRequire(xrtNetBufInit(&Input, NULL),
		"HTTP CONNECT interim-limit buffer init failed");
	for ( i = 0; i < 9; i++ ) {
		testRequire(xrtNetBufAppend(
			&Input, Interim, sizeof(Interim) - 1u
		), "HTTP CONNECT interim-limit append failed");
	}
	testRequire((xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_ERROR) &&
		(xrtErrorCode(xrtNetProxyHandshakeError(pHandshake)) ==
		 XNET_ERROR_PROXY_PROTOCOL),
		"HTTP CONNECT interim response limit mismatch");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 检查一个终态响应的状态码、网络错误和可选 HTTP/1 原因链。 */
static void testHttpRejectResponse(
	cstr sResponse,
	xneterror Code,
	bool bHttpCause
)
{
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake = testHttpReadyToRead(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 0
	);
	xnetbuf Input;
	const xerror* pError;

	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppend(&Input, sResponse, strlen(sResponse)),
		"HTTP CONNECT reject input build failed");
	testRequire(xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_ERROR,
		"HTTP CONNECT rejected response did not fail");
	pError = xrtNetProxyHandshakeError(pHandshake);
	testRequire((pError != NULL) &&
		(xrtErrorCode(pError) == (int32)Code),
		"HTTP CONNECT terminal network error mismatch");
	if ( bHttpCause ) {
		pError = xrtErrorCause(pError);
		testRequire((pError != NULL) &&
			(strcmp(xrtErrorDomain(pError), "xrt.http1") == 0),
			"HTTP CONNECT did not preserve the HTTP/1 cause");
	}
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* 验证认证、拒绝、非法切换和畸形响应保持不同错误语义。 */
static void testHttpFailures(void)
{
	testHttpRejectResponse(
		"HTTP/1.1 407 Proxy Authentication Required\r\n\r\n",
		XNET_ERROR_PROXY_AUTH, false
	);
	testHttpRejectResponse(
		"HTTP/1.1 503 Unavailable\r\n\r\n",
		XNET_ERROR_PROXY_CONNECT, false
	);
	testHttpRejectResponse(
		"HTTP/1.1 101 Switching Protocols\r\n\r\n",
		XNET_ERROR_PROXY_PROTOCOL, false
	);
	testHttpRejectResponse(
		"HTTP/1.1 200 OK\nBad: x\r\n\r\n",
		XNET_ERROR_PROXY_PROTOCOL, true
	);
}



/* 接收限额在完整 Header 到达前后都必须是硬上限。 */
static void testHttpReceiveLimit(void)
{
	static const char Response[] =
		"HTTP/1.1 200 OK\r\nLong: 12345678901234567890\r\n\r\n";
	xnetproxy* pProxy = testHttpProxy(
		XNET_PROXY_AUTH_NONE,
		(xbytesview){ NULL, 0 },
		(xbytesview){ NULL, 0 }
	);
	xnetproxyhandshake* pHandshake = testHttpReadyToRead(
		pProxy, XRT_STR_LITERAL("origin.test"), 443, 32
	);
	xnetbuf Input;

	testRequire(xrtNetBufInit(&Input, NULL) &&
		xrtNetBufAppend(&Input, Response, sizeof(Response) - 1u),
		"HTTP CONNECT limit input build failed");
	testRequire((xrtNetProxyHandshakeStep(pHandshake, &Input) ==
		XNET_PROXY_HANDSHAKE_ERROR) &&
		(xrtErrorCode(xrtNetProxyHandshakeError(pHandshake)) ==
		 XNET_ERROR_PROXY_LIMIT),
		"HTTP CONNECT receive limit mismatch");
	xrtNetBufClear(&Input);
	xrtNetProxyHandshakeDestroy(pHandshake);
	xrtNetProxyRelease(pProxy);
}



/* HTTP Basic 的 user-id 不能包含与密码分隔符冲突的冒号。 */
static void testHttpCredentialValidation(void)
{
	static const uint8 User[] = "bad:user";
	static const uint8 Password[] = "secret";

	testRequire(testHttpProxy(
		XNET_PROXY_AUTH_REQUIRED,
		(xbytesview){ User, sizeof(User) - 1u },
		(xbytesview){ Password, sizeof(Password) - 1u }
	) == NULL, "HTTP CONNECT accepted a colon in Basic user-id");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PROXY_CONFIG),
		"HTTP CONNECT bad user-id error mismatch");
}



/* 执行 HTTP CONNECT 传输无关握手回归。 */
int main(void)
{
	testHttpAnonymousRequest();
	testHttpBasicRequest();
	testHttpIpv6Authority();
	testHttpSuccessAndLeftover();
	testHttpFragmentation();
	testHttpInterimResponse();
	testHttpInterimLimit();
	testHttpFailures();
	testHttpReceiveLimit();
	testHttpCredentialValidation();
	printf("[PASS] net_proxy_http_connect\n");
	return 0;
}
