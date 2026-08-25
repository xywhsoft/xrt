#include <stdlib.h>
#include <string.h>

#include <xrt/error.h>
#include <xrt/net.h>



#define XRT_NET_ADDRESS_FUZZ_TEXT_MAX ((size_t)4096u)



/* 成功解析的地址必须可通过文本和原生结构精确往返。 */
static void __xrtNetAddressFuzzRoundTrip(const xnetaddr* pAddress)
{
	uint8 Native[128];
	char Text[128];
	size_t iNativeSize = sizeof(Native);
	size_t iTextSize;
	xnetaddr Parsed;

	iTextSize = xrtNetAddrEndpointText(
		pAddress, Text, sizeof(Text)
	);
	if ( (iTextSize == 0) || (iTextSize >= sizeof(Text)) ||
		!xrtNetAddrParseEndpoint(&Parsed, Text, 0) ||
		!xrtNetAddrEqual(pAddress, &Parsed) ) {
		abort();
	}
	if ( !xrtNetAddrToNative(
		pAddress, Native, &iNativeSize
	) || !xrtNetAddrFromNative(
		&Parsed, Native, iNativeSize
	) || !xrtNetAddrEqual(pAddress, &Parsed) ) {
		abort();
	}
}



/* 覆盖数字地址、端点和 DNS 数字主机捷径，不执行外部名称查询。 */
static void __xrtNetAddressFuzzText(
	const uint8* pData,
	size_t iSize
)
{
	char Text[XRT_NET_ADDRESS_FUZZ_TEXT_MAX + 1u];
	xnetaddr Address;
	xnetaddr Resolved;
	size_t iTextSize = iSize > XRT_NET_ADDRESS_FUZZ_TEXT_MAX ?
		XRT_NET_ADDRESS_FUZZ_TEXT_MAX : iSize;
	uint16 iPort = iTextSize >= 2u ?
		(uint16)(((uint16)pData[0] << 8u) | pData[1]) : 0;

	if ( iTextSize != 0 ) {
		memcpy(Text, pData, iTextSize);
	}
	Text[iTextSize] = 0;
	if ( xrtNetAddrParse(&Address, Text, iPort) ) {
		__xrtNetAddressFuzzRoundTrip(&Address);
		if ( !xrtNetResolveOne(
			&Resolved, Text, iPort, (xnetfamily)Address.Family
		) || !xrtNetAddrEqual(&Address, &Resolved) ) {
			abort();
		}
	}
	xrtClearError();
	if ( xrtNetAddrParseEndpoint(&Address, Text, iPort) ) {
		__xrtNetAddressFuzzRoundTrip(&Address);
	}
	xrtClearError();
}



/* 让不可变地址列表校验任意结构，并检查成功结果的公开边界。 */
static void __xrtNetAddressFuzzList(
	const uint8* pData,
	size_t iSize
)
{
	xnetaddr Addresses[8];
	size_t iCount = iSize / sizeof(xnetaddr);
	xnetaddrlist* pList;

	if ( iCount > (sizeof(Addresses) / sizeof(Addresses[0])) ) {
		iCount = sizeof(Addresses) / sizeof(Addresses[0]);
	}
	if ( iCount != 0 ) {
		memcpy(Addresses, pData, iCount * sizeof(xnetaddr));
	}
	pList = xrtNetAddrListCreate(Addresses, iCount);
	if ( pList == NULL ) {
		xrtClearError();
		return;
	}
	if ( xrtNetAddrListCount(pList) > iCount ) {
		abort();
	}
	for ( size_t i = 0; i < xrtNetAddrListCount(pList); i++ ) {
		const xnetaddr* pAddress = xrtNetAddrListGet(pList, i);

		if ( pAddress == NULL ) {
			abort();
		}
		__xrtNetAddressFuzzRoundTrip(pAddress);
	}
	xrtNetAddrListDestroy(pList);
}



/* 统一公开确定性回归和 libFuzzer 使用的地址/DNS 输入入口。 */
int xrtNetAddressFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		return 0;
	}
	__xrtNetAddressFuzzText(pData, iSize);
	__xrtNetAddressFuzzList(pData, iSize);
	return 0;
}



#if defined(XRT_NET_ADDRESS_FUZZ_LIBFUZZER)

/* 把独立地址入口适配为 Clang/libFuzzer 约定符号。 */
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtNetAddressFuzzerTestOneInput(pData, iSize);
}

#endif
