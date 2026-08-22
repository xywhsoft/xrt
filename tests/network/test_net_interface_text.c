#include "../test.h"



/* 文本便捷层必须与基础地址、主机名和硬件字节查询保持一致。 */
int main(void)
{
	xnetaddr Address;
	str sAddress;
	str sHost;
	str sHardware;
	str sHardwareBuffer;
	char sActual[96];
	char sExpected[96];
	char sSmall[1];
	size_t iRequired;

	testRequire(xrtNetLocalAddress(&Address, XNET_FAMILY_UNSPEC),
		"local address query failed");
	testRequire(xrtNetAddrText(
		&Address, sExpected, sizeof(sExpected)
	) != XRT_NPOS, "local address format failed");
	testRequire(xrtNetLocalAddressText(
		XNET_FAMILY_UNSPEC, sActual, sizeof(sActual)
	) == strlen(sExpected), "local address text helper failed");
	testRequire(strcmp(sActual, sExpected) == 0,
		"local address text helper mismatch");
	sAddress = xrtNetLocalAddressString(XNET_FAMILY_UNSPEC);
	testRequire((sAddress != NULL) &&
		(strcmp(sAddress, sExpected) == 0),
		"allocated local address text mismatch");
	xrtFree(sAddress);

	sHost = xrtNetHostNameString();
	testRequire((sHost != NULL) && (sHost[0] != 0),
		"allocated host name failed");
	testRequire(strlen(sHost) == xrtNetHostName(NULL, 0),
		"allocated host name length mismatch");
	xrtFree(sHost);

	xrtClearError();
	iRequired = xrtNetLocalHardwareText(NULL, 0);
	if ( iRequired == XRT_NPOS ) {
		testRequire(xrtErrorCode(xrtGetError()) ==
			XNET_ERROR_INTERFACE_HARDWARE,
			"missing hardware text error mismatch");
		return 0;
	}
	testRequire((iRequired != 0) && ((iRequired % 2u) == 0),
		"hardware text size is invalid");
	sHardware = xrtNetLocalHardwareString();
	testRequire((sHardware != NULL) &&
		(strlen(sHardware) == iRequired),
		"allocated hardware text mismatch");
	sHardwareBuffer = (str)xrtMalloc(iRequired + 1u);
	testRequire((sHardwareBuffer != NULL) &&
		(xrtNetLocalHardwareText(
			sHardwareBuffer, iRequired + 1u
		) == iRequired) &&
		(strcmp(sHardwareBuffer, sHardware) == 0),
		"buffer and allocated hardware text disagree");
	xrtFree(sHardwareBuffer);
	xrtFree(sHardware);

	xrtClearError();
	sSmall[0] = 'x';
	testRequire(xrtNetLocalHardwareText(
		sSmall, sizeof(sSmall)
	) == iRequired, "small hardware text buffer lost required length");
	testRequire((sSmall[0] == 0) &&
		(xrtErrorCode(xrtGetError()) == XNET_ERROR_BUFFER),
		"small hardware text buffer contract mismatch");
	return 0;
}
