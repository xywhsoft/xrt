#include <stdio.h>

#include <xrt.h>



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
