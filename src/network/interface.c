#if defined(__linux__) && !defined(_DEFAULT_SOURCE) && \
	!defined(XRT_SINGLE_HEADER)
	#define _DEFAULT_SOURCE 1
#endif

#include "../internal/xrt_net.h"

#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
	#if defined(__TINYC__)
		#include <winapi/iphlpapi.h>
		#include <winapi/ipifcons.h>
	#else
		#include <iphlpapi.h>
		#include <ipifcons.h>
	#endif
#else
	#include <ifaddrs.h>
	#include <net/if.h>
	#include <sys/ioctl.h>
	#include <unistd.h>
	#if defined(__linux__)
		#include <netpacket/packet.h>
	#elif defined(__APPLE__) || defined(__FreeBSD__) || \
		defined(__OpenBSD__) || defined(__NetBSD__) || \
		defined(__DragonFly__)
		#include <net/if_dl.h>
	#endif
#endif



#if defined(XRT_FEATURE_NET_INTERFACE)

/* 接口快照计量结果用于一次分配全部公开借用数据。 */
typedef struct __xrt_net_interface_measure {
	size_t Interfaces;
	size_t Addresses;
	size_t Bytes;
} __xrt_net_interface_measure;



/* 安全累加接口快照尺寸。 */
static bool __xrtNetInterfaceSizeAdd(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 安全累加接口快照数组尺寸。 */
static bool __xrtNetInterfaceSizeArray(size_t* pSize,
	size_t iCount, size_t iItemSize)
{
	if ( (iItemSize != 0) && (iCount > (SIZE_MAX / iItemSize)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	return __xrtNetInterfaceSizeAdd(pSize, iCount * iItemSize);
}



/* 一次分配接口、地址和变长字节，并返回三个连续写区。 */
static bool __xrtNetInterfaceAllocate(
	const __xrt_net_interface_measure* pMeasure,
	xnetinterface** ppInterfaces,
	xnetinterfaceaddress** ppAddresses,
	bytes* ppBytes
)
{
	size_t iSize = 0;
	bytes pStorage;

	*ppInterfaces = NULL;
	*ppAddresses = NULL;
	*ppBytes = NULL;
	if ( pMeasure->Interfaces == 0 ) {
		return true;
	}
	if ( !__xrtNetInterfaceSizeArray(
		&iSize, pMeasure->Interfaces, sizeof(xnetinterface)
	) || !__xrtNetInterfaceSizeArray(
		&iSize, pMeasure->Addresses, sizeof(xnetinterfaceaddress)
	) || !__xrtNetInterfaceSizeAdd(&iSize, pMeasure->Bytes) ) {
		return false;
	}
	pStorage = (bytes)xrtMalloc(iSize);
	if ( pStorage == NULL ) {
		return false;
	}
	memset(pStorage, 0, iSize);
	*ppInterfaces = (xnetinterface*)pStorage;
	*ppAddresses = (xnetinterfaceaddress*)(
		*ppInterfaces + pMeasure->Interfaces
	);
	*ppBytes = (bytes)(*ppAddresses + pMeasure->Addresses);
	return true;
}



/* 比较不要求零结尾的名称和零结尾平台名称。 */
static bool __xrtNetInterfaceNameEqual(xstrview Name, cstr sName)
{
	size_t iSize;

	if ( (Name.Data == NULL) || (sName == NULL) ) {
		return false;
	}
	iSize = strlen(sName);
	return (Name.Size == iSize) &&
		(memcmp(Name.Data, sName, iSize) == 0);
}



/* 判断一个接口条目是否使用指定地址族索引。 */
static bool __xrtNetInterfaceHasIndex(const xnetinterface* pInterface,
	uint32 iIndex, xnetfamily Family)
{
	if ( Family == XNET_FAMILY_IPV4 ) {
		return pInterface->IPv4Index == iIndex;
	}
	if ( Family == XNET_FAMILY_IPV6 ) {
		return pInterface->IPv6Index == iIndex;
	}
	return (pInterface->IPv6Index == iIndex) ||
		(pInterface->IPv4Index == iIndex);
}



/* 按地址族选择接口索引，UNSPEC 优先 IPv6。 */
static uint32 __xrtNetInterfaceSelectIndex(
	const xnetinterface* pInterface,
	xnetfamily Family
)
{
	if ( Family == XNET_FAMILY_IPV4 ) {
		return pInterface->IPv4Index;
	}
	if ( Family == XNET_FAMILY_IPV6 ) {
		return pInterface->IPv6Index;
	}
	return pInterface->IPv6Index != 0 ?
		pInterface->IPv6Index : pInterface->IPv4Index;
}



/* 判断接口查询地址族参数。 */
static bool __xrtNetInterfaceFamilyValid(xnetfamily Family)
{
	return (Family == XNET_FAMILY_UNSPEC) ||
		(Family == XNET_FAMILY_IPV4) ||
		(Family == XNET_FAMILY_IPV6);
}



/* 判断地址是否是接口选择器可以返回的单播地址。 */
static bool __xrtNetLocalAddressUsable(const xnetaddr* pAddress)
{
	if ( pAddress->Family == XNET_FAMILY_IPV4 ) {
		return ((pAddress->Address[0] | pAddress->Address[1] |
			pAddress->Address[2] | pAddress->Address[3]) != 0) &&
			((pAddress->Address[0] & 0xf0u) != 0xe0u);
	}
	if ( pAddress->Family == XNET_FAMILY_IPV6 ) {
		static const uint8 Zero[16] = { 0 };

		return (memcmp(pAddress->Address, Zero, sizeof(Zero)) != 0) &&
			(pAddress->Address[0] != 0xffu);
	}
	return false;
}



/* 判断地址是否是只在本链路上有效的地址。 */
static bool __xrtNetLocalAddressLink(const xnetaddr* pAddress)
{
	if ( pAddress->Family == XNET_FAMILY_IPV4 ) {
		return (pAddress->Address[0] == 169u) &&
			(pAddress->Address[1] == 254u);
	}
	return (pAddress->Family == XNET_FAMILY_IPV6) &&
		((pAddress->Address[0] == 0xfeu) &&
		 ((pAddress->Address[1] & 0xc0u) == 0x80u));
}



/* 为本机诊断查询计算接口基础偏好。 */
static uint32 __xrtNetLocalInterfaceScore(
	const xnetinterface* pInterface
)
{
	uint32 iScore = 0;

	if ( (pInterface->Flags & XNET_INTERFACE_UP) != 0 ) {
		iScore += 64u;
	}
	if ( (pInterface->Flags & XNET_INTERFACE_RUNNING) != 0 ) {
		iScore += 32u;
	}
	if ( (pInterface->Flags & XNET_INTERFACE_LOOPBACK) == 0 ) {
		iScore += 16u;
	}
	return iScore;
}



/* 为一个接口地址计算完整偏好，UNSPEC 在其余条件相同时优先 IPv6。 */
static uint32 __xrtNetLocalAddressScore(
	const xnetinterface* pInterface,
	const xnetaddr* pAddress,
	xnetfamily Family
)
{
	uint32 iScore = __xrtNetLocalInterfaceScore(pInterface);

	if ( !__xrtNetLocalAddressLink(pAddress) ) {
		iScore += 8u;
	}
	if ( (Family == XNET_FAMILY_UNSPEC) &&
		(pAddress->Family == XNET_FAMILY_IPV6) ) {
		iScore += 1u;
	}
	return iScore;
}



/* 检查硬件地址不是全零占位值。 */
static bool __xrtNetLocalHardwareUsable(xbytesview Address)
{
	size_t i;

	if ( (Address.Data == NULL) || (Address.Size == 0) ) {
		return false;
	}
	for ( i = 0; i < Address.Size; i++ ) {
		if ( Address.Data[i] != 0 ) {
			return true;
		}
	}
	return false;
}



/* 动态读取完整主机名，避免把平台长度上限暴露为固定公共缓冲。 */
static bool __xrtNetHostNameRead(str* psName, size_t* pSize)
{
	size_t iCapacity = 256u;
	str sName;

	if ( !__xrtNetEnsure() ) {
		return false;
	}
	sName = (str)xrtMalloc(iCapacity);
	if ( sName == NULL ) {
		return false;
	}
	for ( ;; ) {
		int iResult;
		str sEnd;

		memset(sName, 0xff, iCapacity);
		#if defined(_WIN32) || defined(_WIN64)
			iResult = gethostname(sName, (int)iCapacity);
		#else
			errno = 0;
			iResult = gethostname(sName, iCapacity);
		#endif
		sEnd = (str)memchr(sName, 0, iCapacity);
		if ( (iResult == 0) && (sEnd != NULL) && (sEnd != sName) ) {
			*psName = sName;
			*pSize = (size_t)(sEnd - sName);
			return true;
		}
		if ( (iResult == 0) && (sEnd == sName) ) {
			xrtFree(sName);
			__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_HOST_NAME,
				"host-name", "host name is empty", 0);
			return false;
		}
		if ( iResult != 0 ) {
			#if defined(_WIN32) || defined(_WIN64)
				int iError = WSAGetLastError();

				if ( iError != WSAEFAULT ) {
					xrtFree(sName);
					__xrtNetSetError(XERR_IO, XNET_ERROR_HOST_NAME,
						"host-name", "host name query failed", iError);
					return false;
				}
			#else
				if ( errno != ENAMETOOLONG ) {
					int iError = errno;

					xrtFree(sName);
					__xrtNetSetError(XERR_IO, XNET_ERROR_HOST_NAME,
						"host-name", "host name query failed", iError);
					return false;
				}
			#endif
		}
		#if defined(_WIN32) || defined(_WIN64)
			if ( iCapacity > ((size_t)INT_MAX / 2u) ) {
				xrtFree(sName);
				__xrtErrorSetSizeOverflow();
				return false;
			}
		#else
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				xrtFree(sName);
				__xrtErrorSetSizeOverflow();
				return false;
			}
		#endif
		iCapacity *= 2u;
		{
			str sNew = (str)xrtRealloc(sName, iCapacity);

			if ( sNew == NULL ) {
				xrtFree(sName);
				return false;
			}
			sName = sNew;
		}
	}
}



#if !defined(_WIN32) && !defined(_WIN64)

/* 统计连续网络掩码的前缀位数，非连续掩码返回未知。 */
static uint8 __xrtNetInterfacePrefix(
	const uint8* pMask,
	size_t iSize
)
{
	uint32 iPrefix = 0;
	bool bZero = false;
	size_t i;

	if ( pMask == NULL ) {
		return XNET_INTERFACE_PREFIX_UNKNOWN;
	}
	for ( i = 0; i < iSize; i++ ) {
		uint8 iBit;

		for ( iBit = 0x80u; iBit != 0; iBit >>= 1 ) {
			if ( (pMask[i] & iBit) != 0 ) {
				if ( bZero ) {
					return XNET_INTERFACE_PREFIX_UNKNOWN;
				}
				iPrefix++;
			} else {
				bZero = true;
			}
		}
	}
	return (uint8)iPrefix;
}

#endif



#if defined(_WIN32) || defined(_WIN64)

/* 读取 Windows 适配器链；长度变化时使用系统返回的新容量重试。 */
static bool __xrtNetWindowsAdapters(PIP_ADAPTER_ADDRESSES* ppAdapters)
{
	PIP_ADAPTER_ADDRESSES pAdapters = NULL;
	ULONG iCapacity = 15000u;
	ULONG iResult = ERROR_BUFFER_OVERFLOW;
	uint32 iAttempt;

	*ppAdapters = NULL;
	for ( iAttempt = 0; iAttempt < 8u; iAttempt++ ) {
		PIP_ADAPTER_ADDRESSES pBuffer =
			(PIP_ADAPTER_ADDRESSES)xrtRealloc(pAdapters, (size_t)iCapacity);

		if ( pBuffer == NULL ) {
			xrtFree(pAdapters);
			return false;
		}
		pAdapters = pBuffer;
		iResult = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX |
				GAA_FLAG_SKIP_ANYCAST |
				GAA_FLAG_SKIP_MULTICAST |
				GAA_FLAG_SKIP_DNS_SERVER,
			NULL,
			pAdapters,
			&iCapacity
		);
		if ( iResult == NO_ERROR ) {
			*ppAdapters = pAdapters;
			return true;
		}
		if ( iResult == ERROR_NO_DATA ) {
			xrtFree(pAdapters);
			return true;
		}
		if ( iResult != ERROR_BUFFER_OVERFLOW ) {
			break;
		}
	}
	xrtFree(pAdapters);
	__xrtNetSetError(XERR_IO, XNET_ERROR_INTERFACE_QUERY,
		"interfaces", "failed to enumerate network interfaces", (int)iResult);
	return false;
}



/* 判断 Windows 单播地址是否能转换为稳定地址结构。 */
static bool __xrtNetWindowsAddressValid(const SOCKET_ADDRESS* pAddress)
{
	if ( (pAddress == NULL) || (pAddress->lpSockaddr == NULL) ) {
		return false;
	}
	if ( pAddress->lpSockaddr->sa_family == AF_INET ) {
		return pAddress->iSockaddrLength >= (int)sizeof(struct sockaddr_in);
	}
	if ( pAddress->lpSockaddr->sa_family == AF_INET6 ) {
		return pAddress->iSockaddrLength >= (int)sizeof(struct sockaddr_in6);
	}
	return false;
}



/* 测量 UTF-16 显示名称的 UTF-8 长度，不包含零结尾。 */
static bool __xrtNetWindowsTextSize(const wchar_t* sText, size_t* pSize)
{
	int iSize;

	*pSize = 0;
	if ( (sText == NULL) || (sText[0] == L'\0') ) {
		return true;
	}
	iSize = WideCharToMultiByte(
		CP_UTF8, 0, sText, -1, NULL, 0, NULL, NULL
	);
	if ( iSize <= 0 ) {
		__xrtNetSetError(XERR_IO, XNET_ERROR_INTERFACE_QUERY,
			"interfaces", "failed to measure an interface display name",
			(int)GetLastError());
		return false;
	}
	*pSize = (size_t)iSize - 1u;
	return true;
}



/* 将 UTF-16 显示名称写入已经精确计量的 UTF-8 缓冲。 */
static bool __xrtNetWindowsTextWrite(const wchar_t* sText,
	char* sOutput, size_t iSize)
{
	int iWritten = WideCharToMultiByte(
		CP_UTF8, 0, sText, -1, sOutput, (int)(iSize + 1u), NULL, NULL
	);

	if ( (iWritten <= 0) || ((size_t)iWritten != (iSize + 1u)) ) {
		__xrtNetSetError(XERR_IO, XNET_ERROR_INTERFACE_QUERY,
			"interfaces", "failed to convert an interface display name",
			(int)GetLastError());
		return false;
	}
	return true;
}



/* 无错误副作用地比较 UTF-8 名称和任意长度的 UTF-16 显示名称。 */
static bool __xrtNetWindowsDisplayNameEqual(
	xstrview Name,
	const wchar_t* sDisplay,
	HANDLE hHeap
)
{
	char Local[256];
	char* sText = Local;
	int iRequired;
	int iWritten;
	bool bEqual;

	if (
		(Name.Data == NULL) ||
		(sDisplay == NULL) ||
		(sDisplay[0] == L'\0')
	) {
		return false;
	}
	iRequired = WideCharToMultiByte(
		CP_UTF8, 0, sDisplay, -1, NULL, 0, NULL, NULL
	);
	if (
		(iRequired <= 1) ||
		(Name.Size != ((size_t)iRequired - 1u))
	) {
		return false;
	}
	if ( (size_t)iRequired > sizeof(Local) ) {
		sText = (char*)HeapAlloc(hHeap, 0, (SIZE_T)iRequired);
		if ( sText == NULL ) {
			return false;
		}
	}
	iWritten = WideCharToMultiByte(
		CP_UTF8, 0, sDisplay, -1, sText, iRequired, NULL, NULL
	);
	bEqual = (iWritten == iRequired) &&
		(memcmp(Name.Data, sText, Name.Size) == 0);
	if ( sText != Local ) {
		(void)HeapFree(hHeap, 0, sText);
	}
	return bEqual;
}



/* 测量 Windows 适配器快照。 */
static bool __xrtNetWindowsMeasure(PIP_ADAPTER_ADDRESSES pAdapters,
	__xrt_net_interface_measure* pMeasure)
{
	PIP_ADAPTER_ADDRESSES pAdapter;

	memset(pMeasure, 0, sizeof(*pMeasure));
	for ( pAdapter = pAdapters; pAdapter != NULL; pAdapter = pAdapter->Next ) {
		PIP_ADAPTER_UNICAST_ADDRESS pAddress;
		size_t iDisplaySize;
		size_t iHardwareSize;
		size_t iNameSize;

		if ( (pAdapter->AdapterName == NULL) ||
			(pAdapter->AdapterName[0] == 0) ) {
			continue;
		}
		iNameSize = strlen(pAdapter->AdapterName);
		iHardwareSize = (size_t)pAdapter->PhysicalAddressLength;
		if ( iHardwareSize > sizeof(pAdapter->PhysicalAddress) ) {
			iHardwareSize = sizeof(pAdapter->PhysicalAddress);
		}
		if ( !__xrtNetWindowsTextSize(
			pAdapter->FriendlyName, &iDisplaySize
		) || !__xrtNetInterfaceSizeAdd(
			&pMeasure->Bytes, iNameSize + 1u
		) || ((iDisplaySize != 0) && !__xrtNetInterfaceSizeAdd(
			&pMeasure->Bytes, iDisplaySize + 1u
		)) || !__xrtNetInterfaceSizeAdd(
			&pMeasure->Bytes,
			iHardwareSize
		) ) {
			return false;
		}
		pMeasure->Interfaces++;
		for ( pAddress = pAdapter->FirstUnicastAddress;
			pAddress != NULL; pAddress = pAddress->Next ) {
			if ( __xrtNetWindowsAddressValid(&pAddress->Address) ) {
				pMeasure->Addresses++;
			}
		}
	}
	return true;
}



/* 比较地址前缀的高位。 */
static bool __xrtNetWindowsPrefixEqual(const uint8* pAddress,
	const uint8* pPrefix, uint32 iPrefix)
{
	size_t iBytes = (size_t)(iPrefix / 8u);
	uint8 iBits = (uint8)(iPrefix % 8u);

	if ( (iBytes != 0) &&
		(memcmp(pAddress, pPrefix, iBytes) != 0) ) {
		return false;
	}
	if ( iBits != 0 ) {
		uint8 iMask = (uint8)(0xFFu << (8u - iBits));

		return (pAddress[iBytes] & iMask) ==
			(pPrefix[iBytes] & iMask);
	}
	return true;
}



/* 从 Windows 前缀链选择包含该地址的最长非主机前缀。 */
static uint8 __xrtNetWindowsPrefixFallback(
	PIP_ADAPTER_ADDRESSES pAdapter,
	const xnetaddr* pAddress
)
{
	PIP_ADAPTER_PREFIX pPrefix;
	uint32 iBest = XNET_INTERFACE_PREFIX_UNKNOWN;
	uint32 iMaximum = pAddress->Family == XNET_FAMILY_IPV4 ? 32u : 128u;

	for ( pPrefix = pAdapter->FirstPrefix;
		pPrefix != NULL; pPrefix = pPrefix->Next ) {
		const SOCKET_ADDRESS* pNative = &pPrefix->Address;
		const uint8* pBytes = NULL;
		uint32 iLength = (uint32)pPrefix->PrefixLength;

		if ( !__xrtNetWindowsAddressValid(pNative) ||
			(iLength >= iMaximum) ) {
			continue;
		}
		if ( (pAddress->Family == XNET_FAMILY_IPV4) &&
			(pNative->lpSockaddr->sa_family == AF_INET) ) {
			pBytes = (const uint8*)&((const struct sockaddr_in*)
				pNative->lpSockaddr)->sin_addr;
		} else if ( (pAddress->Family == XNET_FAMILY_IPV6) &&
			(pNative->lpSockaddr->sa_family == AF_INET6) ) {
			pBytes = (const uint8*)&((const struct sockaddr_in6*)
				pNative->lpSockaddr)->sin6_addr;
		}
		if ( (pBytes != NULL) && __xrtNetWindowsPrefixEqual(
			pAddress->Address, pBytes, iLength
		) && ((iBest == XNET_INTERFACE_PREFIX_UNKNOWN) ||
			(iLength > iBest)) ) {
			iBest = iLength;
		}
	}
	return (uint8)iBest;
}



/* 优先读取 Vista 单播记录中的准确 OnLinkPrefixLength。 */
static uint8 __xrtNetWindowsPrefixLength(
	PIP_ADAPTER_ADDRESSES pAdapter,
	PIP_ADAPTER_UNICAST_ADDRESS pUnicast,
	const xnetaddr* pAddress
)
{
	size_t iOffset = offsetof(
		IP_ADAPTER_UNICAST_ADDRESS, LeaseLifetime
	) + sizeof(ULONG);
	uint32 iMaximum = pAddress->Family == XNET_FAMILY_IPV4 ? 32u : 128u;

	if ( (size_t)pUnicast->Length > iOffset ) {
		uint8 iPrefix = *((const uint8*)pUnicast + iOffset);

		if ( iPrefix <= iMaximum ) {
			return iPrefix;
		}
	}
	return __xrtNetWindowsPrefixFallback(pAdapter, pAddress);
}



/* 把 Windows 运行状态与接口类型映射到稳定标志。 */
static uint32 __xrtNetWindowsFlags(PIP_ADAPTER_ADDRESSES pAdapter)
{
	uint32 iFlags = 0;

	if ( pAdapter->OperStatus == IfOperStatusUp ) {
		iFlags |= XNET_INTERFACE_UP | XNET_INTERFACE_RUNNING;
	}
	if ( pAdapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ) {
		iFlags |= XNET_INTERFACE_LOOPBACK;
	}
	if ( (pAdapter->IfType == IF_TYPE_PPP) ||
		(pAdapter->IfType == IF_TYPE_TUNNEL) ) {
		iFlags |= XNET_INTERFACE_POINT_TO_POINT;
	}
	if ( (pAdapter->Flags & IP_ADAPTER_NO_MULTICAST) == 0 ) {
		iFlags |= XNET_INTERFACE_MULTICAST;
	}
	return iFlags;
}



/* 填充 Windows 接口快照。 */
static bool __xrtNetWindowsFill(PIP_ADAPTER_ADDRESSES pAdapters,
	xnetinterface* pInterfaces, xnetinterfaceaddress* pAddresses, bytes pBytes)
{
	PIP_ADAPTER_ADDRESSES pAdapter;
	size_t iInterface = 0;

	for ( pAdapter = pAdapters; pAdapter != NULL; pAdapter = pAdapter->Next ) {
		xnetinterface* pInterface;
		PIP_ADAPTER_UNICAST_ADDRESS pAddress;
		size_t iDisplaySize;
		size_t iNameSize;
		size_t iHardwareSize;

		if ( (pAdapter->AdapterName == NULL) ||
			(pAdapter->AdapterName[0] == 0) ) {
			continue;
		}
		pInterface = &pInterfaces[iInterface++];
		pInterface->IPv4Index = (uint32)pAdapter->IfIndex;
		pInterface->IPv6Index = (uint32)pAdapter->Ipv6IfIndex;
		pInterface->Flags = __xrtNetWindowsFlags(pAdapter);
		pInterface->Mtu = pAdapter->Mtu == UINT32_MAX ?
			0 : (uint32)pAdapter->Mtu;

		iNameSize = strlen(pAdapter->AdapterName);
		memcpy(pBytes, pAdapter->AdapterName, iNameSize + 1u);
		pInterface->Name.Data = (cstr)pBytes;
		pInterface->Name.Size = iNameSize;
		pBytes += iNameSize + 1u;

		if ( !__xrtNetWindowsTextSize(
			pAdapter->FriendlyName, &iDisplaySize
		) ) {
			return false;
		}
		if ( iDisplaySize != 0 ) {
			if ( !__xrtNetWindowsTextWrite(
				pAdapter->FriendlyName, (char*)pBytes, iDisplaySize
			) ) {
				return false;
			}
			pInterface->DisplayName.Data = (cstr)pBytes;
			pInterface->DisplayName.Size = iDisplaySize;
			pBytes += iDisplaySize + 1u;
		} else {
			pInterface->DisplayName = pInterface->Name;
		}

		iHardwareSize = (size_t)pAdapter->PhysicalAddressLength;
		if ( iHardwareSize > sizeof(pAdapter->PhysicalAddress) ) {
			iHardwareSize = sizeof(pAdapter->PhysicalAddress);
		}
		if ( iHardwareSize != 0 ) {
			memcpy(pBytes, pAdapter->PhysicalAddress, iHardwareSize);
			pInterface->HardwareAddress.Data = pBytes;
			pInterface->HardwareAddress.Size = iHardwareSize;
			pBytes += iHardwareSize;
		}

		pInterface->Addresses = pAddresses;
		for ( pAddress = pAdapter->FirstUnicastAddress;
			pAddress != NULL; pAddress = pAddress->Next ) {
			xnetinterfaceaddress* pItem;

			if ( !__xrtNetWindowsAddressValid(&pAddress->Address) ) {
				continue;
			}
			pItem = pAddresses++;
			if ( !xrtNetAddrFromNative(
				&pItem->Address,
				pAddress->Address.lpSockaddr,
				(size_t)pAddress->Address.iSockaddrLength
			) ) {
				return false;
			}
			pItem->Address.Port = 0;
			if ( (pItem->Address.Family == XNET_FAMILY_IPV6) &&
				xrtNetAddrIsLinkLocal(&pItem->Address) &&
				(pItem->Address.Scope == 0) ) {
				pItem->Address.Scope = pInterface->IPv6Index;
			}
			pItem->PrefixLength = __xrtNetWindowsPrefixLength(
				pAdapter, pAddress, &pItem->Address
			);
			pInterface->AddressCount++;
		}
		if ( pInterface->AddressCount == 0 ) {
			pInterface->Addresses = NULL;
		}
	}
	return true;
}



/* Windows 平台创建接口快照。 */
static bool __xrtNetInterfacesPlatform(xnetinterfacelist* pList)
{
	PIP_ADAPTER_ADDRESSES pAdapters;
	__xrt_net_interface_measure Measure;
	xnetinterface* pInterfaces;
	xnetinterfaceaddress* pAddresses;
	bytes pBytes;

	if ( !__xrtNetWindowsAdapters(&pAdapters) ) {
		return false;
	}
	if ( !__xrtNetWindowsMeasure(pAdapters, &Measure) ||
		!__xrtNetInterfaceAllocate(
			&Measure, &pInterfaces, &pAddresses, &pBytes
		) ) {
		xrtFree(pAdapters);
		return false;
	}
	if ( (Measure.Interfaces != 0) && !__xrtNetWindowsFill(
		pAdapters, pInterfaces, pAddresses, pBytes
	) ) {
		xrtFree(pInterfaces);
		xrtFree(pAdapters);
		return false;
	}
	xrtFree(pAdapters);
	pList->Items = pInterfaces;
	pList->Count = Measure.Interfaces;
	return true;
}



/* 无 XRT 错误副作用地在 Windows 适配器链中查询名称。 */
static bool __xrtNetWindowsTryIndex(xstrview Name,
	xnetfamily Family, uint32* pIndex)
{
	ULONG iCapacity = 15000u;
	PIP_ADAPTER_ADDRESSES pAdapters = NULL;
	PIP_ADAPTER_ADDRESSES pAdapter;
	HANDLE hHeap = GetProcessHeap();
	ULONG iResult;
	uint32 iAttempt;
	bool bFound = false;

	for ( iAttempt = 0; iAttempt < 8u; iAttempt++ ) {
		PIP_ADAPTER_ADDRESSES pBuffer = (PIP_ADAPTER_ADDRESSES)
			(pAdapters == NULL ?
			 HeapAlloc(hHeap, 0, (SIZE_T)iCapacity) :
			 HeapReAlloc(hHeap, 0, pAdapters, (SIZE_T)iCapacity));

		if ( pBuffer == NULL ) {
			if ( pAdapters != NULL ) {
				(void)HeapFree(hHeap, 0, pAdapters);
			}
			return false;
		}
		pAdapters = pBuffer;
		iResult = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_SKIP_UNICAST |
				GAA_FLAG_SKIP_ANYCAST |
				GAA_FLAG_SKIP_MULTICAST |
				GAA_FLAG_SKIP_DNS_SERVER,
			NULL, pAdapters, &iCapacity
		);
		if ( iResult != ERROR_BUFFER_OVERFLOW ) {
			break;
		}
	}
	if ( iResult == NO_ERROR ) {
		for ( pAdapter = pAdapters;
			pAdapter != NULL; pAdapter = pAdapter->Next ) {
			uint32 iIndex = Family == XNET_FAMILY_IPV4 ?
				(uint32)pAdapter->IfIndex :
				(Family == XNET_FAMILY_IPV6 ?
				 (uint32)pAdapter->Ipv6IfIndex :
				 ((pAdapter->Ipv6IfIndex != 0) ?
				  (uint32)pAdapter->Ipv6IfIndex :
				  (uint32)pAdapter->IfIndex));
			bool bMatch = __xrtNetInterfaceNameEqual(
				Name, pAdapter->AdapterName
			);

			if ( !bMatch ) {
				bMatch = __xrtNetWindowsDisplayNameEqual(
					Name, pAdapter->FriendlyName, hHeap
				);
			}
			if ( bMatch && (iIndex != 0) ) {
				*pIndex = iIndex;
				bFound = true;
				break;
			}
		}
	}
	if ( pAdapters != NULL ) {
		(void)HeapFree(hHeap, 0, pAdapters);
	}
	return bFound;
}

#else

/* 判断当前 getifaddrs 节点是否是该名称的第一个节点。 */
static bool __xrtNetPosixFirstName(
	const struct ifaddrs* pHead,
	const struct ifaddrs* pItem
)
{
	const struct ifaddrs* pBefore;

	for ( pBefore = pHead; pBefore != pItem; pBefore = pBefore->ifa_next ) {
		if ( (pBefore->ifa_name != NULL) &&
			(strcmp(pBefore->ifa_name, pItem->ifa_name) == 0) ) {
			return false;
		}
	}
	return true;
}



/* 返回平台地址可转换为稳定地址结构时所需的原生尺寸。 */
static size_t __xrtNetPosixAddressSize(const struct sockaddr* pAddress)
{
	if ( pAddress == NULL ) {
		return 0;
	}
	if ( pAddress->sa_family == AF_INET ) {
		return sizeof(struct sockaddr_in);
	}
	if ( pAddress->sa_family == AF_INET6 ) {
		return sizeof(struct sockaddr_in6);
	}
	return 0;
}



/* 从 POSIX 链路地址中返回借用的硬件地址。 */
static xbytesview __xrtNetPosixHardware(const struct sockaddr* pAddress)
{
	xbytesview Hardware = { NULL, 0 };

	#if defined(__linux__)
		if ( (pAddress != NULL) && (pAddress->sa_family == AF_PACKET) ) {
			const struct sockaddr_ll* pLink =
				(const struct sockaddr_ll*)pAddress;
			size_t iSize = (size_t)pLink->sll_halen;

			if ( iSize > sizeof(pLink->sll_addr) ) {
				iSize = sizeof(pLink->sll_addr);
			}
			Hardware.Data = pLink->sll_addr;
			Hardware.Size = iSize;
		}
	#elif defined(AF_LINK)
		if ( (pAddress != NULL) && (pAddress->sa_family == AF_LINK) ) {
			const struct sockaddr_dl* pLink =
				(const struct sockaddr_dl*)pAddress;

			Hardware.Data = (cbytes)LLADDR(pLink);
			Hardware.Size = (size_t)pLink->sdl_alen;
		}
	#else
		(void)pAddress;
	#endif
	return Hardware;
}



/* 在同名 getifaddrs 节点中找到硬件地址。 */
static xbytesview __xrtNetPosixFindHardware(
	const struct ifaddrs* pHead,
	cstr sName
)
{
	const struct ifaddrs* pItem;

	for ( pItem = pHead; pItem != NULL; pItem = pItem->ifa_next ) {
		xbytesview Hardware;

		if ( (pItem->ifa_name == NULL) ||
			(strcmp(pItem->ifa_name, sName) != 0) ) {
			continue;
		}
		Hardware = __xrtNetPosixHardware(pItem->ifa_addr);
		if ( Hardware.Size != 0 ) {
			return Hardware;
		}
	}
	{
		xbytesview Empty = { NULL, 0 };

		return Empty;
	}
}



/* 查询 POSIX 接口 MTU；平台不提供时保留零而不让枚举失败。 */
static uint32 __xrtNetPosixMtu(cstr sName)
{
	struct ifreq Request;
	int hSocket;
	size_t iNameSize = strlen(sName);
	uint32 iMtu = 0;

	if ( iNameSize >= sizeof(Request.ifr_name) ) {
		return 0;
	}
	hSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if ( hSocket < 0 ) {
		return 0;
	}
	memset(&Request, 0, sizeof(Request));
	memcpy(Request.ifr_name, sName, iNameSize + 1u);
	if ( ioctl(hSocket, SIOCGIFMTU, &Request) == 0 ) {
		if ( Request.ifr_mtu > 0 ) {
			iMtu = (uint32)Request.ifr_mtu;
		}
	}
	(void)close(hSocket);
	return iMtu;
}



/* 把 POSIX IFF 位映射到稳定接口标志。 */
static uint32 __xrtNetPosixFlags(unsigned int iNative)
{
	uint32 iFlags = 0;

	#ifdef IFF_UP
		if ( (iNative & IFF_UP) != 0 ) {
			iFlags |= XNET_INTERFACE_UP;
		}
	#endif
	#ifdef IFF_RUNNING
		if ( (iNative & IFF_RUNNING) != 0 ) {
			iFlags |= XNET_INTERFACE_RUNNING;
		}
	#endif
	#ifdef IFF_LOOPBACK
		if ( (iNative & IFF_LOOPBACK) != 0 ) {
			iFlags |= XNET_INTERFACE_LOOPBACK;
		}
	#endif
	#ifdef IFF_BROADCAST
		if ( (iNative & IFF_BROADCAST) != 0 ) {
			iFlags |= XNET_INTERFACE_BROADCAST;
		}
	#endif
	#ifdef IFF_POINTOPOINT
		if ( (iNative & IFF_POINTOPOINT) != 0 ) {
			iFlags |= XNET_INTERFACE_POINT_TO_POINT;
		}
	#endif
	#ifdef IFF_MULTICAST
		if ( (iNative & IFF_MULTICAST) != 0 ) {
			iFlags |= XNET_INTERFACE_MULTICAST;
		}
	#endif
	return iFlags;
}



/* 从 POSIX 地址掩码取得 IPv4 或 IPv6 前缀长度。 */
static uint8 __xrtNetPosixPrefix(const struct sockaddr* pMask,
	xnetfamily Family)
{
	if ( (Family == XNET_FAMILY_IPV4) && (pMask != NULL) &&
		(pMask->sa_family == AF_INET) ) {
		return __xrtNetInterfacePrefix(
			(const uint8*)&((const struct sockaddr_in*)pMask)->sin_addr, 4u
		);
	}
	if ( (Family == XNET_FAMILY_IPV6) && (pMask != NULL) &&
		(pMask->sa_family == AF_INET6) ) {
		return __xrtNetInterfacePrefix(
			(const uint8*)&((const struct sockaddr_in6*)pMask)->sin6_addr, 16u
		);
	}
	return XNET_INTERFACE_PREFIX_UNKNOWN;
}



/* 测量 POSIX 接口快照。 */
static bool __xrtNetPosixMeasure(const struct ifaddrs* pHead,
	__xrt_net_interface_measure* pMeasure)
{
	const struct ifaddrs* pItem;

	memset(pMeasure, 0, sizeof(*pMeasure));
	for ( pItem = pHead; pItem != NULL; pItem = pItem->ifa_next ) {
		if ( (pItem->ifa_name == NULL) || (pItem->ifa_name[0] == 0) ) {
			continue;
		}
		if ( __xrtNetPosixFirstName(pHead, pItem) ) {
			xbytesview Hardware = __xrtNetPosixFindHardware(
				pHead, pItem->ifa_name
			);

			pMeasure->Interfaces++;
			if ( !__xrtNetInterfaceSizeAdd(
				&pMeasure->Bytes, strlen(pItem->ifa_name) + 1u
			) || !__xrtNetInterfaceSizeAdd(
				&pMeasure->Bytes, Hardware.Size
			) ) {
				return false;
			}
		}
		if ( __xrtNetPosixAddressSize(pItem->ifa_addr) != 0 ) {
			pMeasure->Addresses++;
		}
	}
	return true;
}



/* 填充 POSIX 接口快照。 */
static bool __xrtNetPosixFill(const struct ifaddrs* pHead,
	xnetinterface* pInterfaces, xnetinterfaceaddress* pAddresses, bytes pBytes)
{
	const struct ifaddrs* pItem;
	size_t iInterface = 0;

	for ( pItem = pHead; pItem != NULL; pItem = pItem->ifa_next ) {
		xnetinterface* pInterface;
		const struct ifaddrs* pAddress;
		xbytesview Hardware;
		size_t iNameSize;
		uint32 iIndex;

		if ( (pItem->ifa_name == NULL) || (pItem->ifa_name[0] == 0) ||
			!__xrtNetPosixFirstName(pHead, pItem) ) {
			continue;
		}
		iIndex = (uint32)if_nametoindex(pItem->ifa_name);
		if ( iIndex == 0 ) {
			__xrtNetSetError(XERR_IO, XNET_ERROR_INTERFACE_INDEX,
				"interfaces", "failed to read a network interface index", errno);
			return false;
		}
		pInterface = &pInterfaces[iInterface++];
		pInterface->IPv4Index = iIndex;
		pInterface->IPv6Index = iIndex;
		pInterface->Flags = __xrtNetPosixFlags(pItem->ifa_flags);
		pInterface->Mtu = __xrtNetPosixMtu(pItem->ifa_name);

		iNameSize = strlen(pItem->ifa_name);
		memcpy(pBytes, pItem->ifa_name, iNameSize + 1u);
		pInterface->Name.Data = (cstr)pBytes;
		pInterface->Name.Size = iNameSize;
		pInterface->DisplayName = pInterface->Name;
		pBytes += iNameSize + 1u;

		Hardware = __xrtNetPosixFindHardware(pHead, pItem->ifa_name);
		if ( Hardware.Size != 0 ) {
			memcpy(pBytes, Hardware.Data, Hardware.Size);
			pInterface->HardwareAddress.Data = pBytes;
			pInterface->HardwareAddress.Size = Hardware.Size;
			pBytes += Hardware.Size;
		}

		pInterface->Addresses = pAddresses;
		for ( pAddress = pHead;
			pAddress != NULL; pAddress = pAddress->ifa_next ) {
			size_t iNativeSize;
			xnetinterfaceaddress* pOutput;

			if ( (pAddress->ifa_name == NULL) ||
				(strcmp(pAddress->ifa_name, pItem->ifa_name) != 0) ) {
				continue;
			}
			iNativeSize = __xrtNetPosixAddressSize(pAddress->ifa_addr);
			if ( iNativeSize == 0 ) {
				continue;
			}
			pOutput = pAddresses++;
			if ( !xrtNetAddrFromNative(
				&pOutput->Address, pAddress->ifa_addr, iNativeSize
			) ) {
				return false;
			}
			pOutput->Address.Port = 0;
			if ( (pOutput->Address.Family == XNET_FAMILY_IPV6) &&
				xrtNetAddrIsLinkLocal(&pOutput->Address) &&
				(pOutput->Address.Scope == 0) ) {
				pOutput->Address.Scope = iIndex;
			}
			pOutput->PrefixLength = __xrtNetPosixPrefix(
				pAddress->ifa_netmask,
				(xnetfamily)pOutput->Address.Family
			);
			pInterface->AddressCount++;
		}
		if ( pInterface->AddressCount == 0 ) {
			pInterface->Addresses = NULL;
		}
	}
	return true;
}



/* POSIX 平台创建接口快照。 */
static bool __xrtNetInterfacesPlatform(xnetinterfacelist* pList)
{
	struct ifaddrs* pAddresses = NULL;
	__xrt_net_interface_measure Measure;
	xnetinterface* pInterfaces;
	xnetinterfaceaddress* pInterfaceAddresses;
	bytes pBytes;
	int iCode;

	if ( getifaddrs(&pAddresses) != 0 ) {
		iCode = errno;
		__xrtNetSetError(XERR_IO, XNET_ERROR_INTERFACE_QUERY,
			"interfaces", "failed to enumerate network interfaces", iCode);
		return false;
	}
	if ( !__xrtNetPosixMeasure(pAddresses, &Measure) ||
		!__xrtNetInterfaceAllocate(
			&Measure, &pInterfaces, &pInterfaceAddresses, &pBytes
		) ) {
		freeifaddrs(pAddresses);
		return false;
	}
	if ( (Measure.Interfaces != 0) && !__xrtNetPosixFill(
		pAddresses, pInterfaces, pInterfaceAddresses, pBytes
	) ) {
		xrtFree(pInterfaces);
		freeifaddrs(pAddresses);
		return false;
	}
	freeifaddrs(pAddresses);
	pList->Items = pInterfaces;
	pList->Count = Measure.Interfaces;
	return true;
}



/* 无错误副作用地把 POSIX 接口名称转换为索引。 */
static bool __xrtNetPosixTryIndex(xstrview Name, uint32* pIndex)
{
	char sName[IF_NAMESIZE];
	unsigned int iIndex;

	if ( (Name.Data == NULL) || (Name.Size == 0) ||
		(Name.Size >= sizeof(sName)) ||
		(memchr(Name.Data, 0, Name.Size) != NULL) ) {
		return false;
	}
	memcpy(sName, Name.Data, Name.Size);
	sName[Name.Size] = 0;
	iIndex = if_nametoindex(sName);
	if ( iIndex == 0 ) {
		return false;
	}
	*pIndex = (uint32)iIndex;
	return true;
}

#endif



/* 无错误副作用地把接口名称转换为地址族索引。 */
bool __xrtNetInterfaceTryIndex(xstrview Name,
	xnetfamily Family, uint32* pIndex)
{
	if ( (pIndex == NULL) || !__xrtNetInterfaceFamilyValid(Family) ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return __xrtNetWindowsTryIndex(Name, Family, pIndex);
	#else
		(void)Family;
		return __xrtNetPosixTryIndex(Name, pIndex);
	#endif
}



/* 创建当前系统接口、地址和元数据的一致快照。 */
XRT_API bool xrtNetInterfaces(xnetinterfacelist* pList)
{
	xnetinterfacelist List;

	if ( pList == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(&List, 0, sizeof(List));
	if ( !__xrtNetInterfacesPlatform(&List) ) {
		return false;
	}
	*pList = List;
	return true;
}



/* 释放接口快照拥有的全部存储并清零。 */
XRT_API void xrtNetInterfacesFree(xnetinterfacelist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	xrtFree((ptr)pList->Items);
	pList->Items = NULL;
	pList->Count = 0;
}



/* 把规范名称或显示名称转换为接口索引。 */
XRT_API uint32 xrtNetInterfaceIndex(cstr sName, xnetfamily Family)
{
	xnetinterfacelist List;
	xstrview Name;
	uint32 iResult = 0;
	size_t i;

	if ( (sName == NULL) || (sName[0] == 0) ||
		!__xrtNetInterfaceFamilyValid(Family) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	Name.Data = sName;
	Name.Size = strlen(sName);
	if ( !xrtNetInterfaces(&List) ) {
		return 0;
	}
	for ( i = 0; i < List.Count; i++ ) {
		const xnetinterface* pInterface = &List.Items[i];
		uint32 iIndex;

		if ( !__xrtNetInterfaceNameEqual(Name, pInterface->Name.Data) &&
			!__xrtNetInterfaceNameEqual(Name, pInterface->DisplayName.Data) ) {
			continue;
		}
		iIndex = __xrtNetInterfaceSelectIndex(pInterface, Family);
		if ( iIndex == 0 ) {
			continue;
		}
		if ( (iResult != 0) && (iResult != iIndex) ) {
			iResult = 0;
			break;
		}
		iResult = iIndex;
	}
	xrtNetInterfacesFree(&List);
	if ( iResult == 0 ) {
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_INTERFACE_INDEX,
			"interface-index", "network interface name was not found", 0);
	}
	return iResult;
}



/* 输出指定接口索引的规范名称并返回所需长度。 */
XRT_API size_t xrtNetInterfaceName(uint32 iIndex, xnetfamily Family,
	char* sName, size_t iCapacity)
{
	xnetinterfacelist List;
	xstrview Name = { NULL, 0 };
	size_t i;

	if ( (iIndex == 0) || !__xrtNetInterfaceFamilyValid(Family) ||
		((sName == NULL) && (iCapacity != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !xrtNetInterfaces(&List) ) {
		return XRT_NPOS;
	}
	for ( i = 0; i < List.Count; i++ ) {
		if ( __xrtNetInterfaceHasIndex(&List.Items[i], iIndex, Family) ) {
			Name = List.Items[i].Name;
			break;
		}
	}
	if ( Name.Data == NULL ) {
		xrtNetInterfacesFree(&List);
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_INTERFACE_NAME,
			"interface-name", "network interface index was not found", 0);
		return XRT_NPOS;
	}
	if ( (sName != NULL) && (iCapacity != 0) ) {
		size_t iCopy = Name.Size < (iCapacity - 1u) ?
			Name.Size : iCapacity - 1u;

		memcpy(sName, Name.Data, iCopy);
		sName[iCopy] = 0;
	}
	xrtNetInterfacesFree(&List);
	if ( (iCapacity != 0) && (Name.Size >= iCapacity) ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"interface-name", "interface name buffer is too small", 0);
	}
	return Name.Size;
}



/* 选择一个适合诊断展示的本机单播地址。 */
XRT_API bool xrtNetLocalAddress(xnetaddr* pAddress, xnetfamily Family)
{
	xnetinterfacelist List;
	xnetaddr Selected;
	uint32 iBest = 0;
	bool bFound = false;
	size_t i;

	if ( (pAddress == NULL) || !__xrtNetInterfaceFamilyValid(Family) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtNetInterfaces(&List) ) {
		return false;
	}
	for ( i = 0; i < List.Count; i++ ) {
		const xnetinterface* pInterface = &List.Items[i];
		size_t j;

		for ( j = 0; j < pInterface->AddressCount; j++ ) {
			const xnetaddr* pCandidate =
				&pInterface->Addresses[j].Address;
			uint32 iScore;

			if ( ((Family != XNET_FAMILY_UNSPEC) &&
				(pCandidate->Family != Family)) ||
				!__xrtNetLocalAddressUsable(pCandidate) ) {
				continue;
			}
			iScore = __xrtNetLocalAddressScore(
				pInterface, pCandidate, Family
			);
			if ( !bFound || (iScore > iBest) ) {
				Selected = *pCandidate;
				iBest = iScore;
				bFound = true;
			}
		}
	}
	xrtNetInterfacesFree(&List);
	if ( !bFound ) {
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_INTERFACE_ADDRESS,
			"local-address", "no usable local interface address was found", 0);
		return false;
	}
	Selected.Port = 0;
	*pAddress = Selected;
	return true;
}



/* 输出首选活动接口的原始硬件地址。 */
XRT_API size_t xrtNetLocalHardware(void* pAddress, size_t iCapacity)
{
	xnetinterfacelist List;
	xbytesview Selected = { NULL, 0 };
	uint32 iBest = 0;
	bool bFound = false;
	size_t i;

	if ( (pAddress == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !xrtNetInterfaces(&List) ) {
		return XRT_NPOS;
	}
	for ( i = 0; i < List.Count; i++ ) {
		const xnetinterface* pInterface = &List.Items[i];
		uint32 iScore;

		if ( !__xrtNetLocalHardwareUsable(
			pInterface->HardwareAddress
		) ) {
			continue;
		}
		iScore = __xrtNetLocalInterfaceScore(pInterface);
		if ( !bFound || (iScore > iBest) ) {
			Selected = pInterface->HardwareAddress;
			iBest = iScore;
			bFound = true;
		}
	}
	if ( !bFound ) {
		xrtNetInterfacesFree(&List);
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_INTERFACE_HARDWARE,
			"local-hardware", "no usable local hardware address was found", 0);
		return XRT_NPOS;
	}
	if ( (pAddress != NULL) && (iCapacity >= Selected.Size) ) {
		memcpy(pAddress, Selected.Data, Selected.Size);
	}
	xrtNetInterfacesFree(&List);
	if ( (pAddress != NULL) && (iCapacity < Selected.Size) ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"local-hardware", "hardware address buffer is too small", 0);
	}
	return Selected.Size;
}



/* 输出本机主机名并返回完整所需长度。 */
XRT_API size_t xrtNetHostName(char* sName, size_t iCapacity)
{
	str sValue;
	size_t iSize;

	if ( (sName == NULL) && (iCapacity != 0) ) {
		__xrtErrorSetInvalidArgument();
		return XRT_NPOS;
	}
	if ( !__xrtNetHostNameRead(&sValue, &iSize) ) {
		return XRT_NPOS;
	}
	if ( (sName != NULL) && (iCapacity != 0) ) {
		size_t iCopy = iSize < (iCapacity - 1u) ?
			iSize : iCapacity - 1u;

		memcpy(sName, sValue, iCopy);
		sName[iCopy] = 0;
	}
	xrtFree(sValue);
	if ( (iCapacity != 0) && (iSize >= iCapacity) ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_BUFFER,
			"host-name", "host name buffer is too small", 0);
	}
	return iSize;
}

#endif
