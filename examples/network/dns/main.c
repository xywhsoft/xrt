#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/dns —— 域名解析：完整地址列表遍历
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetResolve             同步解析 → 地址列表（拥有式）
 *   xrtNetAddrListCount / Get 列表遍历
 *   xrtNetAddrListDestroy     释放列表
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/dns/main.c -lws2_32 -liphlpapi
 * 预期输出（localhost 双栈，顺序可能互換）：
 *   [::1]:443
 *   127.0.0.1:443
 *
 * 列表语义：一个域名可能解析到多个地址（多网卡/双栈/
 *   负载均衡）——happy-eyeballs 连接策略按序尝试的
 *   输入就是这张表。异步版见 resolver_future 范例。
 */


/* 展示完整地址列表与单地址便捷入口。 */
int main(void)
{
	xnetaddrlist* pList = xrtNetResolve(
		"localhost",
		443,
		XNET_FAMILY_UNSPEC
	);

	if ( pList == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < xrtNetAddrListCount(pList); i++ ) {
		str sEndpoint = xrtNetAddrEndpointString(
			xrtNetAddrListGet(pList, i)
		);

		if ( sEndpoint == NULL ) {
			xrtNetAddrListDestroy(pList);
			return 1;
		}
		printf("%s\n", sEndpoint);
		xrtFree(sEndpoint);
	}
	xrtNetAddrListDestroy(pList);
	return 0;
}
