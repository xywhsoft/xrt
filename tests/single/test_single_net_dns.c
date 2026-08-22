#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须提供不依赖外部 DNS 的数字主机解析路径。 */
int main(void)
{
	xnetaddrlist* pList = xrtNetResolve(
		"127.0.0.1",
		8080,
		XNET_FAMILY_IPV4
	);
	const xnetaddr* pAddr;
	int iResult = 1;

	if ( pList == NULL ) {
		return 1;
	}
	pAddr = xrtNetAddrListGet(pList, 0);
	if ( (pAddr != NULL) && (pAddr->Family == XNET_FAMILY_IPV4) &&
		 (pAddr->Port == 8080) ) {
		iResult = 0;
	}
	xrtNetAddrListDestroy(pList);
	return iResult;
}
