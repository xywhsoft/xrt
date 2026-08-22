#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/winsock2.h>
		#include <winapi/ws2tcpip.h>
	#else
		#include <winsock2.h>
		#include <ws2tcpip.h>
	#endif
#else
	#include <netinet/in.h>
	#include <sys/socket.h>
#endif

#include "../test.h"



/* 解析地址并验证规范文本、地址族和端口。 */
static void testNetParseText(cstr sInput, uint16 iPort,
	xnetfamily Family, cstr sExpected)
{
	xnetaddr Addr;
	char sText[96];

	testRequire(xrtNetAddrParse(&Addr, sInput, iPort),
		"valid network address was rejected");
	testRequire((Addr.Family == (uint16)Family) && (Addr.Port == iPort),
		"parsed address family or port mismatch");
	testRequire(xrtNetAddrText(&Addr, sText, sizeof(sText)) == strlen(sExpected),
		"formatted address length mismatch");
	testRequire(strcmp(sText, sExpected) == 0,
		"canonical address text mismatch");
}



/* 旧版 Any、IPv4、IPv6 和 Scope 基线必须继续成立。 */
static void testNetAddressBasics(void)
{
	xnetaddr Addr;
	char sText[96];

	testRequire(xrtNetAddrAny(&Addr, XNET_FAMILY_IPV4, 8080),
		"IPv4 any initialization failed");
	testRequire(xrtNetAddrIsUnspecified(&Addr) && (Addr.Port == 8080),
		"IPv4 any address mismatch");
	testRequire(xrtNetAddrText(&Addr, sText, sizeof(sText)) == 7,
		"IPv4 any text length mismatch");
	testRequire(strcmp(sText, "0.0.0.0") == 0,
		"IPv4 any text mismatch");

	testRequire(xrtNetAddrAny(&Addr, XNET_FAMILY_IPV6, 9090),
		"IPv6 any initialization failed");
	testRequire(xrtNetAddrIsUnspecified(&Addr) && (Addr.Port == 9090),
		"IPv6 any address mismatch");
	testRequire(xrtNetAddrText(&Addr, sText, sizeof(sText)) == 2,
		"IPv6 any text length mismatch");
	testRequire(strcmp(sText, "::") == 0,
		"IPv6 any text mismatch");

	testNetParseText("127.0.0.1", 80, XNET_FAMILY_IPV4, "127.0.0.1");
	testNetParseText("::1", 443, XNET_FAMILY_IPV6, "::1");
	testNetParseText("fe80::1%42", 53, XNET_FAMILY_IPV6, "fe80::1%42");
}



/* IPv6 输出必须遵守 RFC 5952 的稳定零压缩和混合地址规则。 */
static void testNetIPv6Canonical(void)
{
	testNetParseText("2001:0db8:0000:0000:0000:0000:0000:0001",
		0, XNET_FAMILY_IPV6, "2001:db8::1");
	testNetParseText("2001:0:0:1:0:0:1:1",
		0, XNET_FAMILY_IPV6, "2001::1:0:0:1:1");
	testNetParseText("2001:db8:0:1:1:1:1:1",
		0, XNET_FAMILY_IPV6, "2001:db8:0:1:1:1:1:1");
	testNetParseText("0:0:0:0:0:ffff:192.0.2.1",
		0, XNET_FAMILY_IPV6, "::ffff:192.0.2.1");
	testNetParseText("2001:db8::192.0.2.1",
		0, XNET_FAMILY_IPV6, "2001:db8::c000:201");
}



/* 非法地址必须被严格拒绝，并且失败不能修改调用方输出。 */
static void testNetInvalidAddresses(void)
{
	static const cstr arrInvalid[] = {
		"", "not-an-ip", "127.1", "127.0.0.1.2", "256.0.0.1",
		"01.2.3.4", "1.2.3.-1", ":", "1:", ":::1", "1::2::3",
		"1:2:3:4:5:6:7", "1:2:3:4:5:6:7:8:9", "gggg::1",
		"::ffff:01.2.3.4", "fe80::1%", "fe80::1%name",
		"fe80::1%4294967296", "fe80::1%2%3"
	};
	xnetaddr Addr;
	xnetaddr Saved;
	size_t i;

	memset(&Saved, 0xA5, sizeof(Saved));
	for ( i = 0; i < (sizeof(arrInvalid) / sizeof(arrInvalid[0])); i++ ) {
		Addr = Saved;
		xrtClearError();
		testRequire(!xrtNetAddrParse(&Addr, arrInvalid[i], 7),
			"invalid network address was accepted");
		testRequire(memcmp(&Addr, &Saved, sizeof(Addr)) == 0,
			"failed address parse modified output");
		testRequire(xrtGetError() != NULL,
			"failed address parse did not report an error");
	}

	Addr = Saved;
	testRequire(!xrtNetAddrAny(&Addr, XNET_FAMILY_UNSPEC, 0),
		"unsupported family initialization succeeded");
	testRequire(memcmp(&Addr, &Saved, sizeof(Addr)) == 0,
		"failed family initialization modified output");
}



/* 端点解析必须明确区分 IPv4 端口、方括号 IPv6 和裸 IPv6。 */
static void testNetEndpoints(void)
{
	xnetaddr Addr;
	char sText[96];
	xnetaddr Saved;

	testRequire(xrtNetAddrParseEndpoint(&Addr, "127.0.0.1:8080", 9),
		"IPv4 endpoint parse failed");
	testRequire((Addr.Family == XNET_FAMILY_IPV4) && (Addr.Port == 8080),
		"IPv4 endpoint fields mismatch");
	testRequire(xrtNetAddrEndpointText(&Addr, sText, sizeof(sText)) == 14,
		"IPv4 endpoint text length mismatch");
	testRequire(strcmp(sText, "127.0.0.1:8080") == 0,
		"IPv4 endpoint text mismatch");

	testRequire(xrtNetAddrParseEndpoint(&Addr, "[fe80::1%7]:443", 9),
		"IPv6 endpoint parse failed");
	testRequire((Addr.Family == XNET_FAMILY_IPV6) &&
		(Addr.Scope == 7) && (Addr.Port == 443),
		"IPv6 endpoint fields mismatch");
	testRequire(xrtNetAddrEndpointText(&Addr, sText, sizeof(sText)) == 15,
		"IPv6 endpoint text length mismatch");
	testRequire(strcmp(sText, "[fe80::1%7]:443") == 0,
		"IPv6 endpoint text mismatch");

	testRequire(xrtNetAddrParseEndpoint(&Addr, "::1", 321),
		"bare IPv6 endpoint parse failed");
	testRequire((Addr.Family == XNET_FAMILY_IPV6) && (Addr.Port == 321),
		"bare IPv6 default port mismatch");
	testRequire(xrtNetAddrParseEndpoint(&Addr, "[::1]", 123),
		"bracketed IPv6 default port parse failed");
	testRequire(Addr.Port == 123,
		"bracketed IPv6 default port mismatch");

	memset(&Saved, 0x5A, sizeof(Saved));
	Addr = Saved;
	testRequire(!xrtNetAddrParseEndpoint(&Addr, "[::1]:65536", 0),
		"overflowing endpoint port was accepted");
	testRequire(memcmp(&Addr, &Saved, sizeof(Addr)) == 0,
		"failed endpoint parse modified output");
	testRequire(!xrtNetAddrParseEndpoint(&Addr, "[127.0.0.1]:80", 0),
		"bracketed IPv4 endpoint was accepted");
	testRequire(!xrtNetAddrParseEndpoint(&Addr, "127.0.0.1:", 0),
		"empty endpoint port was accepted");
	testRequire(!xrtNetAddrParseEndpoint(&Addr, "[::1]extra", 0),
		"trailing endpoint data was accepted");
}



/* 格式化接口必须支持大小查询、精确容量和安全截断。 */
static void testNetTextBuffers(void)
{
	xnetaddr Addr;
	char sExact[10];
	char sSmall[5];
	size_t iRequired;

	testRequire(xrtNetAddrParse(&Addr, "127.0.0.1", 80),
		"text buffer setup parse failed");
	iRequired = xrtNetAddrText(&Addr, NULL, 0);
	testRequire(iRequired == 9,
		"address text size query mismatch");
	testRequire(xrtNetAddrText(&Addr, sExact, sizeof(sExact)) == iRequired,
		"exact address text buffer failed");
	testRequire(strcmp(sExact, "127.0.0.1") == 0,
		"exact address text mismatch");

	memset(sSmall, 'x', sizeof(sSmall));
	xrtClearError();
	testRequire(xrtNetAddrText(&Addr, sSmall, sizeof(sSmall)) == iRequired,
		"small address buffer lost required length");
	testRequire(sSmall[sizeof(sSmall) - 1] == 0,
		"small address buffer was not terminated");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorDomain(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.net") == 0) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_BUFFER),
		"small address buffer error mismatch");

	xrtClearError();
	testRequire(xrtNetAddrText(&Addr, NULL, 1) == XRT_NPOS,
		"null nonzero text buffer was accepted");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null text buffer error mismatch");
}



/* 地址比较、分类和映射转换必须覆盖路由与访问控制常用判断。 */
static void testNetClassification(void)
{
	xnetaddr Left;
	xnetaddr Right;
	xnetaddr Result;

	testRequire(xrtNetAddrLoopback(&Left, XNET_FAMILY_IPV4, 80),
		"IPv4 loopback initialization failed");
	Right = Left;
	Right.Port = 81;
	testRequire(xrtNetAddrIsLoopback(&Left),
		"IPv4 loopback classification failed");
	testRequire(xrtNetAddrSameIP(&Left, &Right) &&
		!xrtNetAddrEqual(&Left, &Right) &&
		(xrtNetAddrCompare(&Left, &Right) < 0),
		"address comparison contract mismatch");

	testRequire(xrtNetAddrParse(&Left, "10.2.3.4", 0) &&
		xrtNetAddrIsPrivate(&Left), "IPv4 private classification failed");
	testRequire(xrtNetAddrParse(&Left, "169.254.2.3", 0) &&
		xrtNetAddrIsLinkLocal(&Left), "IPv4 link-local classification failed");
	testRequire(xrtNetAddrParse(&Left, "239.1.2.3", 0) &&
		xrtNetAddrIsMulticast(&Left), "IPv4 multicast classification failed");
	testRequire(xrtNetAddrParse(&Left, "fd00::1", 0) &&
		xrtNetAddrIsPrivate(&Left), "IPv6 private classification failed");
	testRequire(xrtNetAddrParse(&Left, "ff02::1", 0) &&
		xrtNetAddrIsMulticast(&Left), "IPv6 multicast classification failed");
	testRequire(xrtNetAddrParse(&Left, "::ffff:192.0.2.9", 91) &&
		xrtNetAddrIsMapped(&Left), "IPv4 mapped classification failed");
	testRequire(xrtNetAddrUnmap(&Left, &Result),
		"IPv4 mapped unmap failed");
	testRequire((Result.Family == XNET_FAMILY_IPV4) &&
		(Result.Port == 91) &&
		xrtNetAddrParse(&Right, "192.0.2.9", 91) &&
		xrtNetAddrEqual(&Result, &Right),
		"IPv4 mapped unmap result mismatch");
}



/* 分配型 Helper 必须返回独立所有权，不能复用线程局部环形缓冲。 */
static void testNetOwnedText(void)
{
	xnetaddr Addr;
	str sIP;
	str sEndpoint;

	testRequire(xrtNetAddrParse(&Addr, "2001:db8::1", 443),
		"owned text setup parse failed");
	sIP = xrtNetAddrString(&Addr);
	sEndpoint = xrtNetAddrEndpointString(&Addr);
	testRequire((sIP != NULL) && (sEndpoint != NULL),
		"owned address text allocation failed");
	testRequire(strcmp(sIP, "2001:db8::1") == 0,
		"owned IP text mismatch");
	testRequire(strcmp(sEndpoint, "[2001:db8::1]:443") == 0,
		"owned endpoint text mismatch");
	xrtFree(sIP);
	xrtFree(sEndpoint);
}



/* Native 逃生口必须无损往返 IPv4、IPv6、端口和 Scope。 */
static void testNetNative(void)
{
	xnetaddr Addr;
	xnetaddr Result;
	struct sockaddr_storage Native;
	struct sockaddr_in* pIPv4;
	size_t iSize;
	size_t iSmall;

	testRequire(xrtNetAddrLoopback(&Addr, XNET_FAMILY_IPV4, 8080),
		"native IPv4 loopback setup failed");
	iSize = sizeof(Native);
	testRequire(xrtNetAddrToNative(&Addr, &Native, &iSize),
		"native IPv4 loopback conversion failed");
	pIPv4 = (struct sockaddr_in*)&Native;
	testRequire((iSize == sizeof(struct sockaddr_in)) &&
		(pIPv4->sin_family == AF_INET) &&
		(ntohs(pIPv4->sin_port) == 8080) &&
		(((const unsigned char*)&pIPv4->sin_addr)[0] == 127) &&
		(((const unsigned char*)&pIPv4->sin_addr)[1] == 0) &&
		(((const unsigned char*)&pIPv4->sin_addr)[2] == 0) &&
		(((const unsigned char*)&pIPv4->sin_addr)[3] == 1),
		"native IPv4 loopback bytes mismatch");

	testRequire(xrtNetAddrParse(&Addr, "192.0.2.4", 8080),
		"native IPv4 setup parse failed");
	iSize = 0;
	testRequire(xrtNetAddrToNative(&Addr, NULL, &iSize) &&
		(iSize == sizeof(struct sockaddr_in)),
		"native IPv4 size query failed");
	iSmall = 1;
	testRequire(!xrtNetAddrToNative(&Addr, &Native, &iSmall) &&
		(iSmall == sizeof(struct sockaddr_in)),
		"small native IPv4 buffer was accepted");
	iSize = sizeof(Native);
	testRequire(xrtNetAddrToNative(&Addr, &Native, &iSize),
		"native IPv4 conversion failed");
	testRequire(xrtNetAddrFromNative(&Result, &Native, iSize) &&
		xrtNetAddrEqual(&Addr, &Result),
		"native IPv4 roundtrip mismatch");

	testRequire(xrtNetAddrParse(&Addr, "fe80::1234%9", 5353),
		"native IPv6 setup parse failed");
	iSize = sizeof(Native);
	testRequire(xrtNetAddrToNative(&Addr, &Native, &iSize),
		"native IPv6 conversion failed");
	testRequire((iSize == sizeof(struct sockaddr_in6)) &&
		xrtNetAddrFromNative(&Result, &Native, iSize) &&
		xrtNetAddrEqual(&Addr, &Result),
		"native IPv6 roundtrip mismatch");
}



/* 执行网络地址基础层完整回归。 */
int main(void)
{
	testNetAddressBasics();
	testNetIPv6Canonical();
	testNetInvalidAddresses();
	testNetEndpoints();
	testNetTextBuffers();
	testNetClassification();
	testNetOwnedText();
	testNetNative();
	return 0;
}
