#include "../test.h"



/* 默认配置不携带固定容量数组，并使用安全的自动认证策略。 */
static void testNetProxyDefaults(void)
{
	xnetproxyconfig Config;

	xrtNetProxyConfigInit(&Config);
	testRequire(Config.Type == XNET_PROXY_SOCKS5,
		"proxy default type mismatch");
	testRequire(Config.Auth == XNET_PROXY_AUTH_AUTO,
		"proxy default auth policy mismatch");
	testRequire((Config.Host.Data == NULL) &&
		(Config.Username.Data == NULL) &&
		(Config.Password.Data == NULL),
		"proxy default views are not empty");
}



/* 创建过程必须深拷贝端点和凭据，并把 AUTO 规范化为 REQUIRED。 */
static void testNetProxyDeepCopy(void)
{
	char sHost[] = "proxy.example";
	uint8 Username[] = "alice";
	uint8 Password[] = "secret";
	xnetproxyconfig Config;
	xnetproxyinfo Info;
	xnetproxy* pProxy;
	xnetproxy* pRetained;

	xrtNetProxyConfigInit(&Config);
	Config.Host.Data = sHost;
	Config.Host.Size = strlen(sHost);
	Config.Port = 1080;
	Config.Username.Data = Username;
	Config.Username.Size = sizeof(Username) - 1u;
	Config.Password.Data = Password;
	Config.Password.Size = sizeof(Password) - 1u;
	pProxy = xrtNetProxyCreate(&Config);
	testRequire(pProxy != NULL, "proxy deep-copy create failed");

	memset(sHost, 'x', sizeof(sHost) - 1u);
	memset(Username, 'x', sizeof(Username) - 1u);
	memset(Password, 'x', sizeof(Password) - 1u);
	testRequire(xrtNetProxyInfo(pProxy, &Info),
		"proxy info failed");
	testRequire((Info.Type == XNET_PROXY_SOCKS5) &&
		(Info.Port == 1080) &&
		(Info.Auth == XNET_PROXY_AUTH_REQUIRED),
		"proxy canonical info mismatch");
	testRequire((Info.Host.Size == 13) &&
		(memcmp(Info.Host.Data, "proxy.example", 13) == 0),
		"proxy host was not deep-copied");
	testRequire((Info.Username.Size == 5) &&
		(memcmp(Info.Username.Data, "alice", 5) == 0) &&
		(Info.Password.Size == 6) &&
		(memcmp(Info.Password.Data, "secret", 6) == 0),
		"proxy credentials were not deep-copied");

	pRetained = xrtNetProxyRetain(pProxy);
	testRequire(pRetained == pProxy,
		"proxy retain did not return the same object");
	xrtNetProxyRelease(pProxy);
	testRequire(xrtNetProxyInfo(pRetained, &Info),
		"retained proxy did not remain alive");
	xrtNetProxyRelease(pRetained);
}



/* 明确匿名策略不能悄悄忽略凭据，认证策略也不能缺少凭据。 */
static void testNetProxyValidation(void)
{
	xnetproxyconfig Config;
	uint8 User[] = "u";
	uint8 LongUser[256];

	xrtNetProxyConfigInit(&Config);
	testRequire(xrtNetProxyCreate(&Config) == NULL,
		"empty proxy config unexpectedly succeeded");

	Config.Host = XRT_STR_LITERAL("proxy.example");
	Config.Port = 1080;
	Config.Auth = XNET_PROXY_AUTH_REQUIRED;
	testRequire(xrtNetProxyCreate(&Config) == NULL,
		"required auth without credentials unexpectedly succeeded");

	Config.Auth = XNET_PROXY_AUTH_NONE;
	Config.Username.Data = User;
	Config.Username.Size = 1;
	testRequire(xrtNetProxyCreate(&Config) == NULL,
		"anonymous proxy with credentials unexpectedly succeeded");

	memset(LongUser, 'u', sizeof(LongUser));
	Config.Auth = XNET_PROXY_AUTH_REQUIRED;
	Config.Username.Data = LongUser;
	Config.Username.Size = sizeof(LongUser);
	testRequire(xrtNetProxyCreate(&Config) == NULL,
		"oversized SOCKS5 username unexpectedly succeeded");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PROXY_LIMIT),
		"oversized SOCKS5 username error mismatch");
}



#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE) && \
	!defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)

/* HTTP CONNECT 配置可以独立共享，但裁剪掉协议后端时握手必须明确拒绝。 */
static void testNetProxyUnsupportedHandshake(void)
{
	xnetproxyconfig Config;
	xnetproxyhandshakeconfig HandshakeConfig;
	xnetproxyhandshake* pHandshake;
	xnetproxy* pProxy;

	xrtNetProxyConfigInit(&Config);
	Config.Type = XNET_PROXY_HTTP_CONNECT;
	Config.Host = XRT_STR_LITERAL("proxy.example");
	Config.Port = 8080;
	pProxy = xrtNetProxyCreate(&Config);
	testRequire(pProxy != NULL,
		"HTTP CONNECT proxy endpoint create failed");

	xrtNetProxyHandshakeConfigInit(&HandshakeConfig);
	HandshakeConfig.Proxy = pProxy;
	HandshakeConfig.TargetHost = XRT_STR_LITERAL("origin.example");
	HandshakeConfig.TargetPort = 443;
	pHandshake = xrtNetProxyHandshakeCreate(&HandshakeConfig);
	testRequire(pHandshake == NULL,
		"uncompiled HTTP CONNECT handshake unexpectedly succeeded");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_PROXY_UNSUPPORTED),
		"uncompiled HTTP CONNECT error mismatch");
	xrtNetProxyRelease(pProxy);
}

#endif



/* 执行代理配置和不可变对象回归。 */
int main(void)
{
	testNetProxyDefaults();
	testNetProxyDeepCopy();
	testNetProxyValidation();
	#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE) && \
		!defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)
		testNetProxyUnsupportedHandshake();
	#endif
	return 0;
}
