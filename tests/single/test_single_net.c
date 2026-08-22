#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供平台无关的网络地址契约。 */
int main(void)
{
	xnetaddr Addr;
	char sText[64];

	if ( !xrtNetAddrParseEndpoint(&Addr, "[::1]:8080", 0) ) {
		return 1;
	}
	if ( xrtNetAddrEndpointText(&Addr, sText, sizeof(sText)) != 10 ) {
		return 1;
	}
	return strcmp(sText, "[::1]:8080") == 0 ? 0 : 1;
}
