#include "../internal/xrt_net.h"



#if defined(XRT_FEATURE_NET)

/* 地址文本的内部无分配写入器。 */
typedef struct __xrt_net_writer {
	char* Data;
	size_t Capacity;
	size_t Size;
} __xrt_net_writer;



/* 返回一个 ASCII 十六进制字符的数值。 */
static int __xrtNetHexValue(char iChar)
{
	if ( (iChar >= '0') && (iChar <= '9') ) {
		return iChar - '0';
	}
	if ( (iChar >= 'a') && (iChar <= 'f') ) {
		return iChar - 'a' + 10;
	}
	if ( (iChar >= 'A') && (iChar <= 'F') ) {
		return iChar - 'A' + 10;
	}
	return -1;
}



/* 严格解析一个有上限的十进制无符号整数。 */
static bool __xrtNetParseDecimal(cstr sText, size_t iSize,
	uint32 iMaximum, uint32* pValue)
{
	uint32 iValue = 0;
	size_t i;

	if ( (sText == NULL) || (iSize == 0) || (pValue == NULL) ) {
		return false;
	}
	for ( i = 0; i < iSize; i++ ) {
		uint32 iDigit;

		if ( (sText[i] < '0') || (sText[i] > '9') ) {
			return false;
		}
		iDigit = (uint32)(sText[i] - '0');
		if ( iValue > ((iMaximum - iDigit) / 10u) ) {
			return false;
		}
		iValue = (iValue * 10u) + iDigit;
	}
	*pValue = iValue;
	return true;
}



/* 严格解析四段十进制 IPv4，拒绝八进制式前导零。 */
static bool __xrtNetParseIPv4(cstr sText, size_t iSize, uint8* pAddress)
{
	uint8 Address[4];
	size_t iStart = 0;
	size_t iPart;

	if ( (sText == NULL) || (pAddress == NULL) || (iSize == 0) ) {
		return false;
	}
	for ( iPart = 0; iPart < 4; iPart++ ) {
		size_t iEnd = iStart;
		uint32 iValue;

		while ( (iEnd < iSize) && (sText[iEnd] != '.') ) {
			iEnd++;
		}
		if ( (iEnd == iStart) || ((iEnd - iStart) > 3) ||
			(((iEnd - iStart) > 1) && (sText[iStart] == '0')) ||
			!__xrtNetParseDecimal(sText + iStart, iEnd - iStart, 255u, &iValue) ) {
			return false;
		}
		Address[iPart] = (uint8)iValue;
		if ( iPart == 3 ) {
			if ( iEnd != iSize ) {
				return false;
			}
		} else {
			if ( (iEnd == iSize) || (sText[iEnd] != '.') ) {
				return false;
			}
			iStart = iEnd + 1;
		}
	}
	memcpy(pAddress, Address, sizeof(Address));
	return true;
}



/* 解析 RFC 4291 IPv6 文本，包括严格的嵌入式 IPv4 尾部。 */
static bool __xrtNetParseIPv6(cstr sText, size_t iSize, uint8* pAddress)
{
	uint8 Address[16] = { 0 };
	uint8* pWrite = Address;
	uint8* pEnd = Address + sizeof(Address);
	uint8* pCompress = NULL;
	cstr pRead = sText;
	cstr pLimit = sText + iSize;
	cstr pToken = sText;
	uint32 iValue = 0;
	uint32 iDigits = 0;
	bool bColon = false;

	if ( (sText == NULL) || (pAddress == NULL) || (iSize == 0) ) {
		return false;
	}
	if ( *pRead == ':' ) {
		if ( ((pRead + 1) == pLimit) || (pRead[1] != ':') ) {
			return false;
		}
		pCompress = pWrite;
		pRead += 2;
		pToken = pRead;
		bColon = true;
	}

	while ( pRead < pLimit ) {
		char iChar = *pRead++;
		int iDigit = __xrtNetHexValue(iChar);

		if ( iDigit >= 0 ) {
			if ( iDigits == 0 ) {
				pToken = pRead - 1;
			}
			iValue = (iValue << 4) | (uint32)iDigit;
			iDigits++;
			if ( iDigits > 4 ) {
				return false;
			}
			continue;
		}
		if ( iChar == ':' ) {
			bColon = true;
			if ( iDigits == 0 ) {
				if ( pCompress != NULL ) {
					return false;
				}
				pCompress = pWrite;
			} else {
				if ( (pWrite + 2) > pEnd ) {
					return false;
				}
				*pWrite++ = (uint8)(iValue >> 8);
				*pWrite++ = (uint8)iValue;
				iValue = 0;
				iDigits = 0;
			}
			pToken = pRead;
			if ( (pRead == pLimit) && (pCompress == NULL) ) {
				return false;
			}
			continue;
		}
		if ( (iChar == '.') && bColon && (iDigits != 0) &&
			((pWrite + 4) <= pEnd) &&
			__xrtNetParseIPv4(pToken, (size_t)(pLimit - pToken), pWrite) ) {
			pWrite += 4;
			iDigits = 0;
			pRead = pLimit;
			break;
		}
		return false;
	}

	if ( iDigits != 0 ) {
		if ( (pWrite + 2) > pEnd ) {
			return false;
		}
		*pWrite++ = (uint8)(iValue >> 8);
		*pWrite++ = (uint8)iValue;
	}
	if ( pCompress != NULL ) {
		size_t iTail;

		if ( pWrite == pEnd ) {
			return false;
		}
		iTail = (size_t)(pWrite - pCompress);
		memmove(pEnd - iTail, pCompress, iTail);
		memset(pCompress, 0, (size_t)((pEnd - iTail) - pCompress));
		pWrite = pEnd;
	}
	if ( pWrite != pEnd ) {
		return false;
	}

	memcpy(pAddress, Address, sizeof(Address));
	return true;
}



/* 地址视图解析结果区分格式错误与 Scope 错误。 */
typedef enum __xrt_net_addr_parse {
	__XRT_NET_ADDR_PARSE_FORMAT = 0,
	__XRT_NET_ADDR_PARSE_OK,
	__XRT_NET_ADDR_PARSE_SCOPE
} __xrt_net_addr_parse;



/* 解析 IPv6 数字或接口名称 Scope，并返回不含 Scope 的地址长度。 */
static bool __xrtNetParseScope(cstr sIP, size_t iSize,
	size_t* pAddressSize, uint32* pScope, bool bInterface)
{
	size_t i;

	*pAddressSize = iSize;
	*pScope = 0;
	for ( i = 0; i < iSize; i++ ) {
		if ( sIP[i] != '%' ) {
			continue;
		}
		if ( (i == 0) || (i + 1 == iSize) ||
			(memchr(sIP + i + 1, '%', iSize - i - 1) != NULL) ) {
			return false;
		}
		*pAddressSize = i;
		if ( __xrtNetParseDecimal(sIP + i + 1, iSize - i - 1,
			UINT32_MAX, pScope) ) {
			return true;
		}
		#if defined(XRT_FEATURE_NET_INTERFACE)
			if ( bInterface ) {
				xstrview Name;

				Name.Data = sIP + i + 1;
				Name.Size = iSize - i - 1;
				return __xrtNetInterfaceTryIndex(
					Name, XNET_FAMILY_IPV6, pScope
				);
			}
		#else
			(void)bInterface;
		#endif
		return false;
	}
	return true;
}



/* 返回有效地址结构中的地址字节数。 */
static size_t __xrtNetAddressSize(const xnetaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return 0;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		return 4;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		return 16;
	}
	return 0;
}



/* 向文本写入器追加一个字符。 */
static void __xrtNetWriteChar(__xrt_net_writer* pWriter, char iChar)
{
	if ( (pWriter->Data != NULL) && ((pWriter->Size + 1) < pWriter->Capacity) ) {
		pWriter->Data[pWriter->Size] = iChar;
	}
	pWriter->Size++;
}



/* 向文本写入器追加一个指定进制无符号整数。 */
static void __xrtNetWriteUInt(__xrt_net_writer* pWriter, uint32 iValue, uint32 iBase)
{
	static const char sDigits[] = "0123456789abcdef";
	char Text[16];
	size_t iSize = 0;

	do {
		Text[iSize++] = sDigits[iValue % iBase];
		iValue /= iBase;
	} while ( iValue != 0 );
	while ( iSize != 0 ) {
		__xrtNetWriteChar(pWriter, Text[--iSize]);
	}
}



/* 追加规范 IPv4 点分十进制文本。 */
static void __xrtNetWriteIPv4(__xrt_net_writer* pWriter, const uint8* pAddress)
{
	size_t i;

	for ( i = 0; i < 4; i++ ) {
		if ( i != 0 ) {
			__xrtNetWriteChar(pWriter, '.');
		}
		__xrtNetWriteUInt(pWriter, pAddress[i], 10);
	}
}



/* 找到 RFC 5952 要求压缩的第一个最长连续零段。 */
static void __xrtNetIPv6ZeroRun(const uint16* pWords,
	size_t* pBestStart, size_t* pBestSize)
{
	size_t i = 0;

	*pBestStart = 0;
	*pBestSize = 0;
	while ( i < 8 ) {
		size_t iStart;

		if ( pWords[i] != 0 ) {
			i++;
			continue;
		}
		iStart = i;
		while ( (i < 8) && (pWords[i] == 0) ) {
			i++;
		}
		if ( ((i - iStart) >= 2) && ((i - iStart) > *pBestSize) ) {
			*pBestStart = iStart;
			*pBestSize = i - iStart;
		}
	}
}



/* 追加 RFC 5952 规范 IPv6 文本。 */
static void __xrtNetWriteIPv6(__xrt_net_writer* pWriter, const xnetaddr* pAddr)
{
	uint16 Words[8];
	size_t iBestStart;
	size_t iBestSize;
	size_t i;

	if ( xrtNetAddrIsMapped(pAddr) ) {
		__xrtNetWriteChar(pWriter, ':');
		__xrtNetWriteChar(pWriter, ':');
		__xrtNetWriteUInt(pWriter, 0xFFFFu, 16);
		__xrtNetWriteChar(pWriter, ':');
		__xrtNetWriteIPv4(pWriter, pAddr->Address + 12);
		return;
	}

	for ( i = 0; i < 8; i++ ) {
		Words[i] = (uint16)(((uint16)pAddr->Address[i * 2] << 8) |
			(uint16)pAddr->Address[(i * 2) + 1]);
	}
	__xrtNetIPv6ZeroRun(Words, &iBestStart, &iBestSize);
	for ( i = 0; i < 8; ) {
		if ( (iBestSize != 0) && (i == iBestStart) ) {
			__xrtNetWriteChar(pWriter, ':');
			__xrtNetWriteChar(pWriter, ':');
			i += iBestSize;
			continue;
		}
		if ( (i != 0) && (i != (iBestStart + iBestSize)) ) {
			__xrtNetWriteChar(pWriter, ':');
		}
		__xrtNetWriteUInt(pWriter, Words[i], 16);
		i++;
	}
}



/* 追加 IP 地址和可选 IPv6 Scope。 */
static bool __xrtNetWriteAddress(__xrt_net_writer* pWriter, const xnetaddr* pAddr)
{
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		__xrtNetWriteIPv4(pWriter, pAddr->Address);
		return true;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		__xrtNetWriteIPv6(pWriter, pAddr);
		if ( pAddr->Scope != 0 ) {
			__xrtNetWriteChar(pWriter, '%');
			__xrtNetWriteUInt(pWriter, pAddr->Scope, 10);
		}
		return true;
	}
	return false;
}



/* 结束文本写入并保证已有输出缓冲以零字节结尾。 */
static void __xrtNetWriteFinish(__xrt_net_writer* pWriter)
{
	if ( (pWriter->Data != NULL) && (pWriter->Capacity != 0) ) {
		size_t iEnd = pWriter->Size < pWriter->Capacity ?
			pWriter->Size : pWriter->Capacity - 1;

		pWriter->Data[iEnd] = 0;
	}
}



/* 检查格式化缓冲参数并初始化写入器。 */
static bool __xrtNetWriterInit(__xrt_net_writer* pWriter,
	char* sText, size_t iCapacity)
{
	if ( (pWriter == NULL) || ((sText == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pWriter->Data = sText;
	pWriter->Capacity = iCapacity;
	pWriter->Size = 0;
	return true;
}



/* 报告地址文本缓冲不足，同时保留查询得到的所需长度。 */
static void __xrtNetCheckTextCapacity(const __xrt_net_writer* pWriter,
	cstr sOperation)
{
	if ( (pWriter->Capacity != 0) && (pWriter->Size >= pWriter->Capacity) ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			sOperation, "address text buffer is too small", 0);
	}
}



/* 初始化一个指定地址族的未指定地址。 */
XRT_API bool xrtNetAddrAny(xnetaddr* pAddr, xnetfamily Family, uint16 iPort)
{
	xnetaddr Addr;

	if ( pAddr == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (Family != XNET_FAMILY_IPV4) && (Family != XNET_FAMILY_IPV6) ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
			"initialize-address", "unsupported network address family", 0);
		return false;
	}
	memset(&Addr, 0, sizeof(Addr));
	Addr.Family = (uint16)Family;
	Addr.Port = iPort;
	*pAddr = Addr;
	return true;
}



/* 初始化一个指定地址族的回环地址。 */
XRT_API bool xrtNetAddrLoopback(xnetaddr* pAddr, xnetfamily Family, uint16 iPort)
{
	xnetaddr Addr;

	if ( !xrtNetAddrAny(&Addr, Family, iPort) ) {
		return false;
	}
	if ( Family == XNET_FAMILY_IPV4 ) {
		Addr.Address[0] = 127;
		Addr.Address[3] = 1;
	} else {
		Addr.Address[15] = 1;
	}
	*pAddr = Addr;
	return true;
}



/* 无错误副作用地解析地址视图，并按需允许接口名称 Scope。 */
static __xrt_net_addr_parse __xrtNetAddrParseView(
	xnetaddr* pAddr,
	xstrview IP,
	uint16 iPort,
	bool bInterface
)
{
	xnetaddr Addr;
	size_t iAddressSize;
	uint32 iScope;

	if ( (pAddr == NULL) || (IP.Data == NULL) || (IP.Size == 0) ) {
		return __XRT_NET_ADDR_PARSE_FORMAT;
	}
	memset(&Addr, 0, sizeof(Addr));
	Addr.Port = iPort;
	if ( (memchr(IP.Data, '%', IP.Size) == NULL) &&
		__xrtNetParseIPv4(IP.Data, IP.Size, Addr.Address) ) {
		Addr.Family = XNET_FAMILY_IPV4;
		*pAddr = Addr;
		return __XRT_NET_ADDR_PARSE_OK;
	}
	if ( !__xrtNetParseScope(
		IP.Data, IP.Size, &iAddressSize, &iScope, bInterface
	) ) {
		return __XRT_NET_ADDR_PARSE_SCOPE;
	}
	if ( __xrtNetParseIPv6(IP.Data, iAddressSize, Addr.Address) ) {
		Addr.Family = XNET_FAMILY_IPV6;
		Addr.Scope = iScope;
		*pAddr = Addr;
		return __XRT_NET_ADDR_PARSE_OK;
	}
	return __XRT_NET_ADDR_PARSE_FORMAT;
}



/* 无分配、无错误副作用地严格解析数字 IPv4 或 IPv6 地址视图。 */
bool __xrtNetAddrTryParse(
	xnetaddr* pAddr,
	xstrview IP,
	uint16 iPort
)
{
	return __xrtNetAddrParseView(
		pAddr, IP, iPort, false
	) == __XRT_NET_ADDR_PARSE_OK;
}



/* 严格解析数字 IPv4 或 IPv6 地址。 */
XRT_API bool xrtNetAddrParse(xnetaddr* pAddr, cstr sIP, uint16 iPort)
{
	xstrview IP;
	__xrt_net_addr_parse Result;

	if ( (pAddr == NULL) || (sIP == NULL) || (sIP[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	IP.Data = sIP;
	IP.Size = strlen(sIP);
	Result = __xrtNetAddrParseView(pAddr, IP, iPort, true);
	if ( Result == __XRT_NET_ADDR_PARSE_OK ) {
		return true;
	}
	if ( Result == __XRT_NET_ADDR_PARSE_SCOPE ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_SCOPE,
			"parse-address", "invalid IPv6 scope identifier", 0);
		return false;
	}
	__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
		"parse-address", "invalid numeric IP address", 0);
	return false;
}



/* 解析端点中的显式十进制端口。 */
static bool __xrtNetEndpointPort(cstr sText, size_t iSize, uint16* pPort)
{
	uint32 iPort;

	if ( !__xrtNetParseDecimal(sText, iSize, UINT16_MAX, &iPort) ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_PORT,
			"parse-endpoint", "invalid network endpoint port", 0);
		return false;
	}
	*pPort = (uint16)iPort;
	return true;
}



/* 解析端点中的地址切片并建立与独立地址入口相同的错误。 */
static bool __xrtNetEndpointAddress(xnetaddr* pAddr,
	cstr sText, size_t iSize, uint16 iPort)
{
	xstrview IP;
	__xrt_net_addr_parse Result;

	if ( iSize == 0 ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
			"parse-endpoint", "network endpoint address is invalid", 0);
		return false;
	}
	IP.Data = sText;
	IP.Size = iSize;
	Result = __xrtNetAddrParseView(pAddr, IP, iPort, true);
	if ( Result == __XRT_NET_ADDR_PARSE_OK ) {
		return true;
	}
	if ( Result == __XRT_NET_ADDR_PARSE_SCOPE ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_SCOPE,
			"parse-endpoint", "invalid IPv6 scope identifier", 0);
	} else {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
			"parse-endpoint", "invalid numeric IP address", 0);
	}
	return false;
}



/* 解析 IPv4、方括号 IPv6 或使用默认端口的裸地址。 */
XRT_API bool xrtNetAddrParseEndpoint(xnetaddr* pAddr,
	cstr sEndpoint, uint16 iDefaultPort)
{
	xnetaddr Addr;
	size_t iSize;
	size_t i;
	size_t iColons = 0;
	uint16 iPort = iDefaultPort;

	if ( (pAddr == NULL) || (sEndpoint == NULL) || (sEndpoint[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	iSize = strlen(sEndpoint);
	if ( sEndpoint[0] == '[' ) {
		cstr pClose = strchr(sEndpoint + 1, ']');
		size_t iAddressSize;

		if ( pClose == NULL ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
				"parse-endpoint", "IPv6 endpoint is missing a closing bracket", 0);
			return false;
		}
		iAddressSize = (size_t)(pClose - sEndpoint - 1);
		if ( pClose[1] != 0 ) {
			if ( (pClose[1] != ':') ||
				!__xrtNetEndpointPort(pClose + 2,
					iSize - (size_t)(pClose - sEndpoint) - 2, &iPort) ) {
				return false;
			}
		}
		if ( !__xrtNetEndpointAddress(
			&Addr, sEndpoint + 1, iAddressSize, iPort
		) ) {
			return false;
		}
		if ( Addr.Family != XNET_FAMILY_IPV6 ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
				"parse-endpoint", "bracketed endpoint is not IPv6", 0);
			return false;
		}
		*pAddr = Addr;
		return true;
	}

	for ( i = 0; i < iSize; i++ ) {
		if ( sEndpoint[i] == ':' ) {
			iColons++;
		}
	}
	if ( iColons == 1 ) {
		cstr pColon = strchr(sEndpoint, ':');
		size_t iAddressSize = (size_t)(pColon - sEndpoint);

		if ( !__xrtNetEndpointPort(pColon + 1,
				iSize - iAddressSize - 1, &iPort) ) {
			return false;
		}
		if ( !__xrtNetEndpointAddress(
			&Addr, sEndpoint, iAddressSize, iPort
		) ||
			(Addr.Family != XNET_FAMILY_IPV4) ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_FORMAT,
				"parse-endpoint", "unbracketed endpoint with a port must use IPv4", 0);
			return false;
		}
		*pAddr = Addr;
		return true;
	}
	if ( !__xrtNetEndpointAddress(
		&Addr, sEndpoint, iSize, iDefaultPort
	) ) {
		return false;
	}
	*pAddr = Addr;
	return true;
}



/* 输出规范 IP 文本并返回所需长度。 */
XRT_API size_t xrtNetAddrText(const xnetaddr* pAddr,
	char* sText, size_t iCapacity)
{
	__xrt_net_writer Writer;

	if ( (pAddr == NULL) || !__xrtNetWriterInit(&Writer, sText, iCapacity) ) {
		if ( pAddr == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XRT_NPOS;
	}
	if ( !__xrtNetWriteAddress(&Writer, pAddr) ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
			"format-address", "unsupported network address family", 0);
		return XRT_NPOS;
	}
	__xrtNetWriteFinish(&Writer);
	__xrtNetCheckTextCapacity(&Writer, "format-address");
	return Writer.Size;
}



/* 输出带端口的规范端点文本并返回所需长度。 */
XRT_API size_t xrtNetAddrEndpointText(const xnetaddr* pAddr,
	char* sText, size_t iCapacity)
{
	__xrt_net_writer Writer;

	if ( (pAddr == NULL) || !__xrtNetWriterInit(&Writer, sText, iCapacity) ) {
		if ( pAddr == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return XRT_NPOS;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		__xrtNetWriteChar(&Writer, '[');
	}
	if ( !__xrtNetWriteAddress(&Writer, pAddr) ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
			"format-endpoint", "unsupported network address family", 0);
		return XRT_NPOS;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		__xrtNetWriteChar(&Writer, ']');
	}
	__xrtNetWriteChar(&Writer, ':');
	__xrtNetWriteUInt(&Writer, pAddr->Port, 10);
	__xrtNetWriteFinish(&Writer);
	__xrtNetCheckTextCapacity(&Writer, "format-endpoint");
	return Writer.Size;
}



/* 分配任意地址格式函数的完整结果。 */
static str __xrtNetAddrAllocText(const xnetaddr* pAddr,
	size_t (*pFormat)(const xnetaddr*, char*, size_t))
{
	size_t iSize = pFormat(pAddr, NULL, 0);
	str sText;

	if ( iSize == XRT_NPOS ) {
		return NULL;
	}
	sText = (str)xrtMalloc(iSize + 1);
	if ( sText == NULL ) {
		return NULL;
	}
	if ( pFormat(pAddr, sText, iSize + 1) == XRT_NPOS ) {
		xrtFree(sText);
		return NULL;
	}
	return sText;
}



/* 分配并返回规范 IP 文本。 */
XRT_API str xrtNetAddrString(const xnetaddr* pAddr)
{
	return __xrtNetAddrAllocText(pAddr, xrtNetAddrText);
}



/* 分配并返回带端口的规范端点文本。 */
XRT_API str xrtNetAddrEndpointString(const xnetaddr* pAddr)
{
	return __xrtNetAddrAllocText(pAddr, xrtNetAddrEndpointText);
}



/* 只比较地址和 IPv6 Scope。 */
XRT_API bool xrtNetAddrSameIP(const xnetaddr* pLeft, const xnetaddr* pRight)
{
	size_t iSize;

	if ( (pLeft == NULL) || (pRight == NULL) ||
		(pLeft->Family != pRight->Family) ) {
		return false;
	}
	iSize = __xrtNetAddressSize(pLeft);
	if ( (iSize == 0) ||
		((pLeft->Family == XNET_FAMILY_IPV6) &&
		 (pLeft->Scope != pRight->Scope)) ) {
		return false;
	}
	return memcmp(pLeft->Address, pRight->Address, iSize) == 0;
}



/* 比较完整网络端点。 */
XRT_API bool xrtNetAddrEqual(const xnetaddr* pLeft, const xnetaddr* pRight)
{
	return xrtNetAddrSameIP(pLeft, pRight) && (pLeft->Port == pRight->Port);
}



/* 按字段顺序比较完整网络端点。 */
XRT_API int xrtNetAddrCompare(const xnetaddr* pLeft, const xnetaddr* pRight)
{
	size_t iSize;
	int iResult;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( pLeft->Family != pRight->Family ) {
		return pLeft->Family < pRight->Family ? -1 : 1;
	}
	iSize = __xrtNetAddressSize(pLeft);
	if ( iSize != 0 ) {
		iResult = memcmp(pLeft->Address, pRight->Address, iSize);
		if ( iResult != 0 ) {
			return iResult < 0 ? -1 : 1;
		}
	}
	if ( (pLeft->Family == XNET_FAMILY_IPV6) &&
		(pLeft->Scope != pRight->Scope) ) {
		return pLeft->Scope < pRight->Scope ? -1 : 1;
	}
	if ( pLeft->Port != pRight->Port ) {
		return pLeft->Port < pRight->Port ? -1 : 1;
	}
	return 0;
}



/* 判断地址字节是否全部为零。 */
static bool __xrtNetAddressZero(const uint8* pAddress, size_t iSize)
{
	size_t i;

	for ( i = 0; i < iSize; i++ ) {
		if ( pAddress[i] != 0 ) {
			return false;
		}
	}
	return true;
}



/* 判断地址是否为未指定地址。 */
XRT_API bool xrtNetAddrIsUnspecified(const xnetaddr* pAddr)
{
	size_t iSize = __xrtNetAddressSize(pAddr);

	return (iSize != 0) && __xrtNetAddressZero(pAddr->Address, iSize);
}



/* 判断地址是否为回环地址。 */
XRT_API bool xrtNetAddrIsLoopback(const xnetaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return false;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		return pAddr->Address[0] == 127;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		return __xrtNetAddressZero(pAddr->Address, 15) &&
			(pAddr->Address[15] == 1);
	}
	return false;
}



/* 判断地址是否为组播地址。 */
XRT_API bool xrtNetAddrIsMulticast(const xnetaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return false;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		return (pAddr->Address[0] & 0xF0u) == 0xE0u;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		return pAddr->Address[0] == 0xFFu;
	}
	return false;
}



/* 判断地址是否为链路本地地址。 */
XRT_API bool xrtNetAddrIsLinkLocal(const xnetaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return false;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		return (pAddr->Address[0] == 169) && (pAddr->Address[1] == 254);
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		return (pAddr->Address[0] == 0xFEu) &&
			((pAddr->Address[1] & 0xC0u) == 0x80u);
	}
	return false;
}



/* 判断地址是否属于明确的私有单播范围。 */
XRT_API bool xrtNetAddrIsPrivate(const xnetaddr* pAddr)
{
	if ( pAddr == NULL ) {
		return false;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		return (pAddr->Address[0] == 10) ||
			((pAddr->Address[0] == 172) &&
			 (pAddr->Address[1] >= 16) && (pAddr->Address[1] <= 31)) ||
			((pAddr->Address[0] == 192) && (pAddr->Address[1] == 168));
	}
	if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		return (pAddr->Address[0] & 0xFEu) == 0xFCu;
	}
	return false;
}



/* 判断地址是否为 IPv4 映射 IPv6 地址。 */
XRT_API bool xrtNetAddrIsMapped(const xnetaddr* pAddr)
{
	if ( (pAddr == NULL) || (pAddr->Family != XNET_FAMILY_IPV6) ) {
		return false;
	}
	return __xrtNetAddressZero(pAddr->Address, 10) &&
		(pAddr->Address[10] == 0xFFu) && (pAddr->Address[11] == 0xFFu);
}



/* 把 IPv4 映射地址转换为普通 IPv4。 */
XRT_API bool xrtNetAddrUnmap(const xnetaddr* pAddr, xnetaddr* pResult)
{
	xnetaddr Result;

	if ( (pAddr == NULL) || (pResult == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtNetAddressSize(pAddr) == 0 ) {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
			"unmap-address", "unsupported network address family", 0);
		return false;
	}
	if ( !xrtNetAddrIsMapped(pAddr) ) {
		*pResult = *pAddr;
		return true;
	}
	memset(&Result, 0, sizeof(Result));
	Result.Family = XNET_FAMILY_IPV4;
	Result.Port = pAddr->Port;
	memcpy(Result.Address, pAddr->Address + 12, 4);
	*pResult = Result;
	return true;
}



/* 转换为平台 sockaddr，保留旧版已经压实的字段映射。 */
XRT_API bool xrtNetAddrToNative(const xnetaddr* pAddr,
	void* pNative, size_t* pSize)
{
	size_t iRequired;

	if ( (pAddr == NULL) || (pSize == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		iRequired = sizeof(struct sockaddr_in);
	} else if ( pAddr->Family == XNET_FAMILY_IPV6 ) {
		iRequired = sizeof(struct sockaddr_in6);
	} else {
		__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
			"to-native-address", "unsupported network address family", 0);
		return false;
	}
	if ( pNative == NULL ) {
		*pSize = iRequired;
		return true;
	}
	if ( *pSize < iRequired ) {
		*pSize = iRequired;
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"to-native-address", "native address buffer is too small", 0);
		return false;
	}
	memset(pNative, 0, iRequired);
	if ( pAddr->Family == XNET_FAMILY_IPV4 ) {
		struct sockaddr_in* pAddress = (struct sockaddr_in*)pNative;

		pAddress->sin_family = AF_INET;
		pAddress->sin_port = htons(pAddr->Port);
		memcpy(&pAddress->sin_addr, pAddr->Address, 4);
	} else {
		struct sockaddr_in6* pAddress = (struct sockaddr_in6*)pNative;

		pAddress->sin6_family = AF_INET6;
		pAddress->sin6_port = htons(pAddr->Port);
		pAddress->sin6_scope_id = pAddr->Scope;
		memcpy(&pAddress->sin6_addr, pAddr->Address, 16);
	}
	*pSize = iRequired;
	return true;
}



/* 从平台 sockaddr 转换为稳定地址结构。 */
XRT_API bool xrtNetAddrFromNative(xnetaddr* pAddr,
	const void* pNative, size_t iSize)
{
	struct sockaddr Native;
	xnetaddr Addr;

	if ( (pAddr == NULL) || (pNative == NULL) ||
		(iSize < sizeof(struct sockaddr)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memcpy(&Native, pNative, sizeof(Native));
	memset(&Addr, 0, sizeof(Addr));
	if ( Native.sa_family == AF_INET ) {
		struct sockaddr_in Address;

		if ( iSize < sizeof(Address) ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_NATIVE,
				"from-native-address", "truncated IPv4 native address", 0);
			return false;
		}
		memcpy(&Address, pNative, sizeof(Address));
		Addr.Family = XNET_FAMILY_IPV4;
		Addr.Port = ntohs(Address.sin_port);
		memcpy(Addr.Address, &Address.sin_addr, 4);
	} else if ( Native.sa_family == AF_INET6 ) {
		struct sockaddr_in6 Address;

		if ( iSize < sizeof(Address) ) {
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_NATIVE,
				"from-native-address", "truncated IPv6 native address", 0);
			return false;
		}
		memcpy(&Address, pNative, sizeof(Address));
		Addr.Family = XNET_FAMILY_IPV6;
		Addr.Port = ntohs(Address.sin6_port);
		Addr.Scope = Address.sin6_scope_id;
		memcpy(Addr.Address, &Address.sin6_addr, 16);
	} else {
		__xrtNetSetError(XERR_UNSUPPORTED, XNET_ERROR_NATIVE,
			"from-native-address", "unsupported native address family", 0);
		return false;
	}
	*pAddr = Addr;
	return true;
}

#endif
