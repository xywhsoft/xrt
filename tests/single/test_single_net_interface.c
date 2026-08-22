#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须完整提供接口快照和名称索引转换。 */
int main(void)
{
	xnetinterfacelist List;
	const xnetinterface* pInterface;
	xnetaddr Address;
	char sHost[512];
	xnetfamily Family;
	uint32 iIndex;

	memset(&List, 0, sizeof(List));
	if ( !xrtNetInterfaces(&List) || (List.Count == 0) ) {
		return 1;
	}
	pInterface = &List.Items[0];
	Family = pInterface->IPv6Index != 0 ?
		XNET_FAMILY_IPV6 : XNET_FAMILY_IPV4;
	iIndex = xrtNetInterfaceIndex(pInterface->Name.Data, Family);
	xrtNetInterfacesFree(&List);
	if ( (iIndex == 0) ||
		!xrtNetLocalAddress(&Address, XNET_FAMILY_UNSPEC) ||
		(xrtNetHostName(sHost, sizeof(sHost)) == XRT_NPOS) ) {
		return 1;
	}
	return 0;
}
