#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须提供可独立裁剪的本机网络信息文本便捷层。 */
int main(void)
{
	str sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
	str sHost = xrtNetHostNameString();

	if ( (sAddress == NULL) || (sHost == NULL) ) {
		xrtFree(sAddress);
		xrtFree(sHost);
		return 1;
	}
	xrtFree(sAddress);
	xrtFree(sHost);
	return 0;
}
