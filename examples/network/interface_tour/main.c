/*
 * 范例：network/interface_tour —— 接口与本地信息缓冲版补集
 * ----------------------------------------------------------------
 * 演示 API：
 *   【接口】    xrtNetInterfaceIndex（名→索引）/ InterfaceName
 *              （索引→名，两段式查询+写入）
 *   【本地信息】 xrtNetLocalAddress（结构体出参）/
 *              LocalAddressText（文本两段式）/
 *              LocalHardware（原始字节两段式）/
 *              LocalHardwareText（大写紧凑 HEX）/
 *              HostName（缓冲两段式）
 * 模块宏：XRT_MODULE_NET_INTERFACE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/interface_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   iface: loopback index!=0 name=[[GUID]] ok
 *   iface: local addr=[192.168.0.66] hw=00E04C7D7E66 host=[DESKTOP-DQCMPS1] ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	char sName[64];
	char sText[64];
	uint8 Hardware[16];
	xnetaddr Address;
	uint32 iIndex;
	size_t iSize = 0;
	size_t iNeed = 0;
	int iResult = 1;

	/* ---- InterfaceIndex / InterfaceName：回环往返 ---- */
	iIndex = xrtNetInterfaceIndex("loopback4", XNET_FAMILY_IPV4);
	if ( iIndex == 0u ) {
		/* Windows 命名不同：回环显示名 "Loopback Pseudo-Interface 1"。 */
		iIndex = xrtNetInterfaceIndex(
			"Loopback Pseudo-Interface 1", XNET_FAMILY_IPV4);
	}
	if ( iIndex == 0u ) {
		goto Cleanup;  /* 找不到回环：环境异常 */
	}
	/* InterfaceName 两段式：查询 → 写入 → 回环索引反查一致。 */
	iNeed = xrtNetInterfaceName(iIndex, XNET_FAMILY_IPV4, NULL,
		0u);
	if ( (iNeed == 0u) || (iNeed >= sizeof(sName)) ) {
		goto Cleanup;
	}
	iSize = xrtNetInterfaceName(iIndex, XNET_FAMILY_IPV4, sName,
		sizeof(sName));
	if ( (iSize == 0u) || (iSize != iNeed) ) {
		goto Cleanup;
	}
	/* 名字反查索引一致（roundtrip）。 */
	if ( xrtNetInterfaceIndex(sName, XNET_FAMILY_IPV4) !=
		iIndex ) {
		/* 显示名与规范名可能不同：跳过反查但记录。 */
		printf("iface: (display-name index differs) ");
	}
	printf("iface: loopback index!=0 name=[%s] ok\n", sName);

	/* ---- LocalAddress / Text ---- */
	if ( !xrtNetLocalAddress(&Address, XNET_FAMILY_IPV4) ||
		(Address.Family != XNET_FAMILY_IPV4) ||
		(Address.Port != 0u) ) {
		goto Cleanup;
	}
	iNeed = xrtNetLocalAddressText(XNET_FAMILY_IPV4, NULL, 0u);
	if ( (iNeed == 0u) || (iNeed >= sizeof(sText)) ) {
		goto Cleanup;
	}
	iSize = xrtNetLocalAddressText(XNET_FAMILY_IPV4, sText,
		sizeof(sText));
	if ( (iSize == 0u) || (iSize != iNeed) ||
		(strchr(sText, '.') == NULL) ) {
		goto Cleanup;  /* IPv4 文本必含点 */
	}
	printf("iface: local addr=[%s]", sText);

	/* ---- LocalHardware / Text：MAC 至少 6 字节。 ---- */
	iNeed = xrtNetLocalHardware(NULL, 0u);
	if ( (iNeed < 6u) || (iNeed > sizeof(Hardware)) ) {
		goto Cleanup;
	}
	iSize = xrtNetLocalHardware(Hardware, sizeof(Hardware));
	if ( iSize != iNeed ) {
		goto Cleanup;
	}
	iNeed = xrtNetLocalHardwareText(NULL, 0u);
	if ( (iNeed < 12u) || (iNeed >= sizeof(sText)) ) {
		goto Cleanup;  /* 6 字节 → 12 个 HEX 字符 */
	}
	iSize = xrtNetLocalHardwareText(sText, sizeof(sText));
	if ( (iSize != iNeed) ) {
		goto Cleanup;
	}
	printf(" hw=%s", sText);

	/* ---- HostName：非空且可写入。 ---- */
	iNeed = xrtNetHostName(NULL, 0u);
	if ( (iNeed == 0u) || (iNeed >= sizeof(sName)) ) {
		goto Cleanup;
	}
	iSize = xrtNetHostName(sName, sizeof(sName));
	if ( (iSize == 0u) || (iSize != iNeed) ) {
		goto Cleanup;
	}
	printf(" host=[%s] ok\n", sName);
	iResult = 0;

Cleanup:
	return iResult;
}
