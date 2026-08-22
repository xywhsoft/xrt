#include <stdio.h>

#include <xrt.h>



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
