/*
 * 范例：network/addr_tour —— 地址判定/比较/原生转换 + 地址列表
 * ----------------------------------------------------------------
 * 演示 API：
 *   【比较】      xrtNetAddrEqual（完整端点）
 *                 xrtNetAddrSameIP（忽略端口）
 *                 xrtNetAddrCompare（Map/排序用全序）
 *   【判定】      xrtNetAddrIsUnspecified / IsLoopback /
 *                 IsMulticast / IsPrivate / IsMapped
 *   【转换】      xrtNetAddrUnmap（::ffff:x → IPv4）
 *                 xrtNetAddrToNative / FromNative（sockaddr 往返）
 *   【列表】      xrtNetAddrListWithPort（统一换端口）
 *                 xrtNetAddrListRef（共享引用）
 *   【DNS】       xrtNetLookup / ResolveOne / Reverse
 * 模块宏：XRT_MODULE_NET（DNS 三接口需 XRT_MODULE_NET_DNS）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/addr_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   addr: equal/sameip/compare ok
 *   addr: classes unspec/loopback/multicast/private ok
 *   addr: unmap mapped->ipv4 ok
 *   addr: native sockaddr roundtrip ok
 *   addr: list with-port + ref ok
 *   addr: dns localhost lookup/resolve/reverse ok
 *
 * 纯函数巡礼：全部判定用已知地址断言；
 *   sockaddr 往返要求 ToNative 的 16 字节 IPv4 布局
 *   被 FromNative 无损读回。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	xnetaddr Loopback;
	xnetaddr Private;
	xnetaddr Any;
	xnetaddr Other;
	xnetaddr Mapped;
	xnetaddr Unmapped;
	xnetaddr Native;
	uint8 arrSockaddr[64];
	size_t iSize = sizeof(arrSockaddr);
	xnetaddrlist* pList = NULL;
	xnetaddrlist* pPortList = NULL;
	xnetaddrlist* pRef = NULL;
	str sHost = NULL;
	int iResult = 1;

	/* ---- 比较：Equal 收全端点，SameIP 无视端口。 ---- */
	if ( !xrtNetAddrParse(&Loopback, "127.0.0.1", 8080u) ||
		!xrtNetAddrParse(&Private, "10.0.0.5", 8080u) ||
		!xrtNetAddrParse(&Other, "10.0.0.5", 9000u) ) {
		goto Cleanup;
	}
	/* SameIP 与 Equal 的分界：同 IP 换端口。 */
	if ( xrtNetAddrEqual(&Loopback, &Private) ||
		!xrtNetAddrEqual(&Private, &Private) ||
		xrtNetAddrEqual(&Private, &Other) ||
		xrtNetAddrSameIP(&Loopback, &Private) ||
		!xrtNetAddrSameIP(&Private, &Other) ) {
		goto Cleanup;
	}
	/* Compare 全序：同地址等零；10.0.0.5 < 127.0.0.1（首字节序）。 */
	if ( (xrtNetAddrCompare(&Private, &Private) != 0) ||
		(xrtNetAddrCompare(&Private, &Loopback) >= 0) ||
		(xrtNetAddrCompare(&Loopback, &Private) <= 0) ) {
		goto Cleanup;
	}
	printf("addr: equal/sameip/compare ok\n");

	/* ---- 分类判定：四类已知地址正反断言。 ---- */
	if ( !xrtNetAddrAny(&Any, XNET_FAMILY_IPV4, 0u) ||
		xrtNetAddrIsUnspecified(&Loopback) ||
		!xrtNetAddrIsUnspecified(&Any) ||
		!xrtNetAddrIsLoopback(&Loopback) ||
		xrtNetAddrIsLoopback(&Private) ||
		!xrtNetAddrParse(&Other, "224.0.0.1", 0u) ||
		!xrtNetAddrIsMulticast(&Other) ||
		xrtNetAddrIsMulticast(&Private) ||
		!xrtNetAddrParse(&Other, "192.168.1.1", 0u) ||
		!xrtNetAddrIsPrivate(&Other) ||
		!xrtNetAddrIsPrivate(&Private) ||
		!xrtNetAddrParse(&Other, "8.8.8.8", 0u) ||
		xrtNetAddrIsPrivate(&Other) ) {
		goto Cleanup;
	}
	printf("addr: classes unspec/loopback/multicast/private ok\n");

	/* ---- IPv4 映射地址：IsMapped + Unmap 还原。 ---- */
	if ( !xrtNetAddrParse(&Mapped, "::ffff:192.168.0.1", 443u) ||
		!xrtNetAddrIsMapped(&Mapped) ||
		xrtNetAddrIsMapped(&Loopback) ||
		!xrtNetAddrUnmap(&Mapped, &Unmapped) ||
		!xrtNetAddrParse(&Other, "192.168.0.1", 443u) ||
		!xrtNetAddrEqual(&Unmapped, &Other) ||
		/* 非映射地址原样复制。 */
		!xrtNetAddrUnmap(&Loopback, &Unmapped) ||
		!xrtNetAddrEqual(&Unmapped, &Loopback) ) {
		goto Cleanup;
	}
	printf("addr: unmap mapped->ipv4 ok\n");

	/* ---- sockaddr 往返：先空输出查询大小，再实际写入。 ---- */
	if ( !xrtNetAddrToNative(&Loopback, NULL, &iSize) ||
		(iSize != 16u) ||
		!xrtNetAddrToNative(&Loopback, arrSockaddr, &iSize) ||
		!xrtNetAddrFromNative(&Native, arrSockaddr, iSize) ||
		!xrtNetAddrEqual(&Native, &Loopback) ) {
		goto Cleanup;
	}
	printf("addr: native sockaddr roundtrip ok\n");

	/* ---- 地址列表：统一换端口 + 共享引用。 ---- */
	{
		xnetaddr arrTwo[2];

		arrTwo[0] = Loopback;
		arrTwo[1] = Private;
		pList = xrtNetAddrListCreate(arrTwo, 2u);
		if ( (pList == NULL) ||
			(xrtNetAddrListCount(pList) != 2u) ) {
			goto Cleanup;
		}
		pPortList = xrtNetAddrListWithPort(pList, 443u);
		if ( (pPortList == NULL) ||
			(xrtNetAddrListCount(pPortList) != 2u) ||
			(xrtNetAddrListGet(pPortList, 0u)->Port != 443u) ||
			(xrtNetAddrListGet(pPortList, 1u)->Port != 443u) ) {
			goto Cleanup;
		}
		pRef = xrtNetAddrListRef(pPortList);
		if ( pRef != pPortList ) {
			goto Cleanup;
		}
	}
	printf("addr: list with-port + ref ok\n");

	/* ---- DNS 三接口：localhost 本机即可解析，不依赖外网。 ---- */
	{
		xnetaddrlist* pLocal = xrtNetLookup("localhost",
			XNET_FAMILY_IPV4);
		xnetaddr Resolved;

		if ( (pLocal == NULL) ||
			(xrtNetAddrListCount(pLocal) < 1u) ) {
			xrtNetAddrListDestroy(pLocal);
			goto Cleanup;
		}
		xrtNetAddrListDestroy(pLocal);
		if ( !xrtNetResolveOne(&Resolved, "localhost", 80u,
				XNET_FAMILY_IPV4) ||
			(Resolved.Port != 80u) ) {
			goto Cleanup;
		}
		sHost = xrtNetReverse(&Loopback);
		if ( sHost == NULL ) {
			goto Cleanup;
		}
	}
	printf("addr: dns localhost lookup/resolve/reverse ok\n");
	iResult = 0;

Cleanup:
	xrtFree(sHost);
	xrtNetAddrListDestroy(pRef);
	xrtNetAddrListDestroy(pPortList);
	xrtNetAddrListDestroy(pList);
	return iResult;
}
