#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/local_info —— 本机网络信息一览（诊断页）
 * ----------------------------------------------------------------
 * 演示 API：
 *   本机信息聚合入口   主机名/接口/路由相关摘要
 * 模块宏：XRT_MODULE_NET_INTERFACE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/local_info/main.c -lws2_32 -liphlpapi
 * 预期输出：（随机器变化）
 *
 * 启动日志打印网络环境、管理端点 /health 页展示
 *   "我跑在什么网络上"——一次调用聚合全部信息。
 */


/* 一次读取适合启动日志和诊断页展示的本机网络信息。 */
int main(void)
{
	str sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
	str sHost = xrtNetHostNameString();
	str sHardware = xrtNetLocalHardwareString();

	if ( (sAddress == NULL) || (sHost == NULL) ) {
		xrtFree(sAddress);
		xrtFree(sHost);
		xrtFree(sHardware);
		return 1;
	}
	printf("host = %s\n", sHost);
	printf("address = %s\n", sAddress);
	printf("hardware = %s\n", sHardware != NULL ? sHardware : "unavailable");
	xrtFree(sHardware);
	xrtFree(sHost);
	xrtFree(sAddress);
	return 0;
}
