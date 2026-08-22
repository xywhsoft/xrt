#include "../test.h"



/* 验证数字主机、方括号 IPv6、端口和地址族过滤。 */
static void testNetDNSNumeric(void)
{
	xnetaddrlist* pList;
	const xnetaddr* pAddr;
	xnetaddr Saved;
	xnetaddr Result;

	pList = xrtNetResolve("127.0.0.1", 8080, XNET_FAMILY_UNSPEC);
	testRequire(pList != NULL, "numeric IPv4 resolve failed");
	testRequire(xrtNetAddrListCount(pList) == 1,
		"numeric IPv4 result count mismatch");
	pAddr = xrtNetAddrListGet(pList, 0);
	testRequire((pAddr != NULL) &&
		(pAddr->Family == XNET_FAMILY_IPV4) &&
		(pAddr->Port == 8080), "numeric IPv4 result mismatch");
	testRequire(xrtNetAddrListRef(pList) == pList,
		"address list retain failed");
	xrtNetAddrListDestroy(pList);
	xrtNetAddrListDestroy(pList);
	pList = xrtNetLookup("127.0.0.1", XNET_FAMILY_IPV4);
	testRequire(pList != NULL, "numeric IPv4 lookup failed");
	pAddr = xrtNetAddrListGet(pList, 0);
	testRequire((pAddr != NULL) && (pAddr->Port == 0),
		"host lookup returned an endpoint port");
	xrtNetAddrListDestroy(pList);

	pList = xrtNetResolve("[::1]", 443, XNET_FAMILY_IPV6);
	testRequire(pList != NULL, "bracketed IPv6 resolve failed");
	pAddr = xrtNetAddrListGet(pList, 0);
	testRequire((pAddr != NULL) &&
		(pAddr->Family == XNET_FAMILY_IPV6) &&
		(pAddr->Port == 443), "bracketed IPv6 result mismatch");
	xrtNetAddrListDestroy(pList);

	memset(&Saved, 0xA5, sizeof(Saved));
	Result = Saved;
	testRequire(!xrtNetResolveOne(
		&Result,
		"127.0.0.1",
		80,
		XNET_FAMILY_IPV6
	), "family-mismatched numeric host resolved");
	testRequire(memcmp(&Result, &Saved, sizeof(Result)) == 0,
		"failed resolve-one modified output");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND,
		"family mismatch error kind mismatch");
}



/* 验证系统解析保留完整结果、去重、端口和请求地址族。 */
static void testNetDNSLocalhost(void)
{
	xnetaddrlist* pList;
	xnetaddr First;
	size_t iCount;

	pList = xrtNetResolve("localhost", 9000, XNET_FAMILY_UNSPEC);
	testRequire(pList != NULL, "localhost resolve failed");
	iCount = xrtNetAddrListCount(pList);
	testRequire(iCount != 0, "localhost returned no addresses");
	for ( size_t i = 0; i < iCount; i++ ) {
		const xnetaddr* pAddr = xrtNetAddrListGet(pList, i);

		testRequire((pAddr != NULL) && (pAddr->Port == 9000),
			"localhost port was not preserved");
		testRequire((pAddr->Family == XNET_FAMILY_IPV4) ||
			(pAddr->Family == XNET_FAMILY_IPV6),
			"localhost returned an unsupported family");
		for ( size_t j = 0; j < i; j++ ) {
			testRequire(!xrtNetAddrEqual(
				pAddr,
				xrtNetAddrListGet(pList, j)
			), "localhost returned a duplicate address");
		}
	}
	First = *xrtNetAddrListGet(pList, 0);
	xrtNetAddrListDestroy(pList);

	testRequire(xrtNetResolveOne(
		&First,
		"localhost",
		53,
		XNET_FAMILY_UNSPEC
	), "localhost resolve-one failed");
	testRequire(First.Port == 53,
		"localhost resolve-one port mismatch");
}



/* 验证反向解析返回独立拥有的名称。 */
static void testNetDNSReverse(void)
{
	xnetaddr Addr;
	str sHost;

	testRequire(xrtNetAddrLoopback(&Addr, XNET_FAMILY_IPV4, 0),
		"reverse setup failed");
	sHost = xrtNetReverse(&Addr);
	testRequire((sHost != NULL) && (sHost[0] != 0),
		"loopback reverse resolve failed");
	xrtFree(sHost);
}



/* 验证调用方地址可去重建表，并可用共享或复制路径统一设置端口。 */
static void testNetDNSAddressList(void)
{
	xnetaddr Addresses[3];
	xnetaddrlist* pList;
	xnetaddrlist* pSame;
	xnetaddrlist* pEndpoints;

	testRequire(xrtNetAddrLoopback(
		&Addresses[0],
		XNET_FAMILY_IPV4,
		0
	), "address-list IPv4 setup failed");
	Addresses[1] = Addresses[0];
	testRequire(xrtNetAddrLoopback(
		&Addresses[2],
		XNET_FAMILY_IPV6,
		0
	), "address-list IPv6 setup failed");
	pList = xrtNetAddrListCreate(Addresses, 3);
	testRequire(pList != NULL, "address-list create failed");
	testRequire(xrtNetAddrListCount(pList) == 2,
		"address-list duplicate was retained");

	pSame = xrtNetAddrListWithPort(pList, 0);
	testRequire(pSame == pList,
		"unchanged address-list was copied");
	xrtNetAddrListDestroy(pSame);
	pEndpoints = xrtNetAddrListWithPort(pList, 8443);
	testRequire((pEndpoints != NULL) && (pEndpoints != pList),
		"address-list port mapping did not copy");
	testRequire(xrtNetAddrListCount(pEndpoints) == 2,
		"address-list port mapping changed count");
	for ( size_t i = 0; i < xrtNetAddrListCount(pEndpoints); i++ ) {
		testRequire(
			xrtNetAddrListGet(pEndpoints, i)->Port == 8443,
			"address-list port mapping mismatch"
		);
		testRequire(
			xrtNetAddrListGet(pList, i)->Port == 0,
			"address-list port mapping modified source"
		);
	}
	xrtNetAddrListDestroy(pEndpoints);
	xrtNetAddrListDestroy(pList);
}



/* 验证空输入、非法地址族和列表越界均产生稳定错误。 */
static void testNetDNSInvalid(void)
{
	xnetaddrlist* pList;

	testRequire(xrtNetResolve(NULL, 0, XNET_FAMILY_UNSPEC) == NULL,
		"null host resolved");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null host error mismatch");
	testRequire(xrtNetResolve("", 0, XNET_FAMILY_UNSPEC) == NULL,
		"empty host resolved");
	testRequire(xrtNetResolve("localhost", 0, (xnetfamily)7) == NULL,
		"invalid DNS family resolved");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_FAMILY),
		"invalid DNS family error mismatch");

	pList = xrtNetResolve("127.0.0.1", 0, XNET_FAMILY_IPV4);
	testRequire(pList != NULL, "invalid-test address resolve failed");
	testRequire(xrtNetAddrListGet(pList, 1) == NULL,
		"out-of-range address list access succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_RANGE,
		"address list range error mismatch");
	xrtNetAddrListDestroy(pList);
}



/* DNS 同步层覆盖数字快路、系统解析、共享所有权和反向查询。 */
int main(void)
{
	testNetDNSNumeric();
	testNetDNSLocalhost();
	testNetDNSReverse();
	testNetDNSAddressList();
	testNetDNSInvalid();
	printf("[PASS] network DNS\n");
	return 0;
}
