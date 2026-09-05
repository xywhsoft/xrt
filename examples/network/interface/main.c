#include <stdio.h>

#include <xrt.h>



/*
 * 范例：network/interface —— 本机网卡接口枚举与地址元数据
 * ----------------------------------------------------------------
 * 演示 API：
 *   接口枚举   名称/索引/状态/地址族
 *   地址元数据 绑定候选 / scope zone / 多播能力
 * 模块宏：XRT_MODULE_NET_INTERFACE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/interface/main.c -lws2_32 -liphlpapi
 * 预期输出：（本机接口列表，随机器变化）
 *
 * 用途：服务绑定选网卡、IPv6 链路本地地址要拿
 *   zone-id（对应 address 范例的 %3）、组播成员
 *   管理都需要接口枚举——跨平台收口在一处。
 */


/* 列出本机接口以及可用于绑定、Scope 和多播的地址元数据。 */
int main(void)
{
	xnetinterfacelist List;
	size_t i;

	if ( !xrtNetInterfaces(&List) ) {
		return 1;
	}
	for ( i = 0; i < List.Count; i++ ) {
		const xnetinterface* pInterface = &List.Items[i];
		size_t j;

		printf("%.*s (%.*s) index4=%u index6=%u mtu=%u\n",
			(int)pInterface->Name.Size, pInterface->Name.Data,
			(int)pInterface->DisplayName.Size, pInterface->DisplayName.Data,
			(unsigned int)pInterface->IPv4Index,
			(unsigned int)pInterface->IPv6Index,
			(unsigned int)pInterface->Mtu);
		for ( j = 0; j < pInterface->AddressCount; j++ ) {
			const xnetinterfaceaddress* pAddress = &pInterface->Addresses[j];
			char sAddress[96];

			if ( xrtNetAddrText(
				&pAddress->Address, sAddress, sizeof(sAddress)
			) == XRT_NPOS ) {
				xrtNetInterfacesFree(&List);
				return 1;
			}
			printf("  %s/%u\n", sAddress,
				(unsigned int)pAddress->PrefixLength);
		}
	}
	xrtNetInterfacesFree(&List);
	return 0;
}
