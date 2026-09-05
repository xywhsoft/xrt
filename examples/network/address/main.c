#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/address —— 网络地址：IPv6 区域 ID、分类判定
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtNetAddrParseEndpoint   解析 "[host]:port" / "host:port" / host
 *   xrtNetAddrEndpointString  拥有式端点文本（IPv6 自动加方括号）
 *   xrtNetAddrIsLinkLocal     链路本地地址判定（zone 感知）
 * 模块宏：XRT_MODULE_NET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/address/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   endpoint=[fe80::1%3]:8080
 *   family=6
 *   link-local=yes
 *
 * "%3" 是 IPv6 区域 ID（scope zone）——链路本地地址
 *   必须指明挂在哪块网卡，Windows 用数字、Linux 常用
 *   接口名（%eth0）；解析保留区域、序列化原样回写，
 *   跨平台差异被收口。
 */


/* 展示数字地址、端点、分类和拥有文本的常用路径。 */
int main(void)
{
	xnetaddr Addr;
	str sEndpoint;

	if ( !xrtNetAddrParseEndpoint(&Addr, "[fe80::1%3]:8080", 0) ) {
		return 1;
	}
	sEndpoint = xrtNetAddrEndpointString(&Addr);
	if ( sEndpoint == NULL ) {
		return 1;
	}
	printf("endpoint=%s\nfamily=%u\nlink-local=%s\n",
		sEndpoint, (unsigned int)Addr.Family,
		xrtNetAddrIsLinkLocal(&Addr) ? "yes" : "no");
	xrtFree(sEndpoint);
	return 0;
}
