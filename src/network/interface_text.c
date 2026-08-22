#include "../internal/xrt_net.h"



#if defined(XRT_FEATURE_NET_INTERFACE_TEXT)

/* 读取完整硬件地址，处理两次接口快照之间的尺寸变化。 */
static bytes __xrtNetLocalHardwareRead(size_t* pSize)
{
	size_t iSize = xrtNetLocalHardware(NULL, 0);
	bytes pAddress;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	for ( ;; ) {
		size_t iRequired;

		pAddress = (bytes)xrtMalloc(iSize);
		if ( pAddress == NULL ) {
			return NULL;
		}
		iRequired = xrtNetLocalHardware(pAddress, iSize);
		if ( iRequired == XRT_NPOS ) {
			xrtFree(pAddress);
			return NULL;
		}
		if ( iRequired <= iSize ) {
			*pSize = iRequired;
			return pAddress;
		}
		xrtFree(pAddress);
		iSize = iRequired;
	}
}



/* 输出首选本机地址的规范文本。 */
XRT_API size_t xrtNetLocalAddressText(xnetfamily Family,
	char* sAddress, size_t iCapacity)
{
	xnetaddr Address;

	if ( !xrtNetLocalAddress(&Address, Family) ) {
		return XRT_NPOS;
	}
	return xrtNetAddrText(&Address, sAddress, iCapacity);
}



/* 分配首选本机地址的规范文本。 */
XRT_API str xrtNetLocalAddressString(xnetfamily Family)
{
	xnetaddr Address;

	if ( !xrtNetLocalAddress(&Address, Family) ) {
		return NULL;
	}
	return xrtNetAddrString(&Address);
}



/* 输出首选接口硬件地址的大写紧凑 HEX 文本。 */
XRT_API size_t xrtNetLocalHardwareText(char* sAddress, size_t iCapacity)
{
	bytes pAddress;
	size_t iAddressSize;
	size_t iRequired;

	if ( (sAddress == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	pAddress = __xrtNetLocalHardwareRead(&iAddressSize);
	if ( pAddress == NULL ) {
		return XRT_NPOS;
	}
	if ( iAddressSize > ((SIZE_MAX - 1u) / 2u) ) {
		xrtFree(pAddress);
		__xrtErrorSetSizeOverflow();
		return XRT_NPOS;
	}
	iRequired = iAddressSize * 2u;
	if ( (sAddress != NULL) && (iCapacity > iRequired) ) {
		size_t iWritten = 0;

		if ( !xrtHexEncode(pAddress, iAddressSize,
			sAddress, iCapacity, &iWritten, XHEX_UPPER) ) {
			xrtFree(pAddress);
			return XRT_NPOS;
		}
	}
	xrtFree(pAddress);
	if ( (sAddress != NULL) && (iCapacity <= iRequired) ) {
		if ( iCapacity != 0 ) {
			sAddress[0] = 0;
		}
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"local-hardware-text",
			"hardware address text buffer is too small", 0);
	}
	return iRequired;
}



/* 分配首选接口硬件地址的大写紧凑 HEX 文本。 */
XRT_API str xrtNetLocalHardwareString(void)
{
	bytes pAddress;
	size_t iAddressSize;
	str sResult;

	pAddress = __xrtNetLocalHardwareRead(&iAddressSize);
	if ( pAddress == NULL ) {
		return NULL;
	}
	sResult = xrtHexEncodeNew(pAddress, iAddressSize, XHEX_UPPER);
	xrtFree(pAddress);
	return sResult;
}



/* 分配并返回当前主机名。 */
XRT_API str xrtNetHostNameString(void)
{
	size_t iSize = xrtNetHostName(NULL, 0);

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	for ( ;; ) {
		str sName;
		size_t iRequired;

		if ( iSize == SIZE_MAX ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		sName = (str)xrtMalloc(iSize + 1u);
		if ( sName == NULL ) {
			return NULL;
		}
		iRequired = xrtNetHostName(sName, iSize + 1u);
		if ( iRequired == XRT_NPOS ) {
			xrtFree(sName);
			return NULL;
		}
		if ( iRequired <= iSize ) {
			return sName;
		}
		xrtFree(sName);
		iSize = iRequired;
	}
}

#endif
