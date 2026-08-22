#include "../internal/xrt_net.h"

#if !defined(_WIN32) && !defined(_WIN64)
	#include <netdb.h>
	#if defined(__linux__) && !defined(EAI_BADFLAGS)
		/* 严格 C 模式可能隐藏稳定的 POSIX 名称解析 ABI。 */
		#define EAI_BADFLAGS (-1)
		#define EAI_NONAME (-2)
		#define EAI_AGAIN (-3)
		#define EAI_FAMILY (-6)
		#define EAI_SOCKTYPE (-7)
		#define EAI_MEMORY (-10)
		#define NI_NAMEREQD 8

		struct addrinfo {
			int ai_flags;
			int ai_family;
			int ai_socktype;
			int ai_protocol;
			socklen_t ai_addrlen;
			struct sockaddr* ai_addr;
			char* ai_canonname;
			struct addrinfo* ai_next;
		};

		extern int getaddrinfo(
			const char* sNode,
			const char* sService,
			const struct addrinfo* pHints,
			struct addrinfo** ppResult
		);
		extern void freeaddrinfo(struct addrinfo* pResult);
		extern int getnameinfo(
			const struct sockaddr* pAddress,
			socklen_t iAddressSize,
			char* sHost,
			socklen_t iHostSize,
			char* sService,
			socklen_t iServiceSize,
			int iFlags
		);
	#endif
#endif



#if defined(XRT_FEATURE_NET_DNS)

#define XRT_NET_DNS_INITIAL_CAPACITY 8u
#define XRT_NET_DNS_NAME_CAPACITY 1025u



/* 地址列表把引用、有效数量和预留容量放在单次分配中。 */
struct xnetaddrlist {
	volatile int32 References;
	size_t Count;
	size_t Capacity;
	xnetaddr Addresses[];
};



/* 校验公开地址族并映射到系统 getaddrinfo 常量。 */
static bool __xrtNetDNSFamily(xnetfamily Family, int* pNative)
{
	if ( pNative == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Family == XNET_FAMILY_UNSPEC ) {
		*pNative = AF_UNSPEC;
		return true;
	}
	if ( Family == XNET_FAMILY_IPV4 ) {
		*pNative = AF_INET;
		return true;
	}
	if ( Family == XNET_FAMILY_IPV6 ) {
		*pNative = AF_INET6;
		return true;
	}

	__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
		"resolve", "unsupported DNS address family", 0);
	return false;
}



/* 按预留容量建立一个空的不可变结果构造体。 */
static xnetaddrlist* __xrtNetAddrListReserve(size_t iCapacity)
{
	size_t iHeader = offsetof(xnetaddrlist, Addresses);
	size_t iBytes;
	xnetaddrlist* pList;

	if ( (iCapacity == 0) ||
		 (iCapacity > ((SIZE_MAX - iHeader) / sizeof(xnetaddr))) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = iHeader + (iCapacity * sizeof(xnetaddr));
	pList = (xnetaddrlist*)xrtMalloc(iBytes);
	if ( pList == NULL ) {
		return NULL;
	}
	pList->References = 1;
	pList->Count = 0;
	pList->Capacity = iCapacity;
	return pList;
}



/* 扩展构造中的列表并追加一个尚未出现的地址。 */
static bool __xrtNetAddrListAppend(
	xnetaddrlist** ppList,
	const xnetaddr* pAddr
)
{
	xnetaddrlist* pList = *ppList;

	for ( size_t i = 0; i < pList->Count; i++ ) {
		if ( xrtNetAddrEqual(&pList->Addresses[i], pAddr) ) {
			return true;
		}
	}
	if ( pList->Count == pList->Capacity ) {
		size_t iHeader = offsetof(xnetaddrlist, Addresses);
		size_t iCapacity = pList->Capacity * 2u;
		size_t iBytes;

		if ( (iCapacity < pList->Capacity) ||
			 (iCapacity > ((SIZE_MAX - iHeader) / sizeof(xnetaddr))) ) {
			__xrtErrorSetSizeOverflow();
			return false;
		}
		iBytes = iHeader + (iCapacity * sizeof(xnetaddr));
		pList = (xnetaddrlist*)xrtRealloc(pList, iBytes);
		if ( pList == NULL ) {
			return false;
		}
		pList->Capacity = iCapacity;
		*ppList = pList;
	}
	pList->Addresses[pList->Count++] = *pAddr;
	return true;
}



/* 把 getaddrinfo 错误映射到稳定错误类别。 */
static xerrkind __xrtNetDNSErrorKind(int iCode)
{
	if ( iCode == EAI_AGAIN ) {
		return XERR_AGAIN;
	}
	if ( iCode == EAI_MEMORY ) {
		return XERR_MEMORY;
	}
	if ( iCode == EAI_NONAME ) {
		return XERR_NOT_FOUND;
	}
	#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
		if ( iCode == EAI_NODATA ) {
			return XERR_NOT_FOUND;
		}
	#endif
	if ( (iCode == EAI_FAMILY) || (iCode == EAI_BADFLAGS) ||
		 (iCode == EAI_SOCKTYPE) ) {
		return XERR_UNSUPPORTED;
	}
	return XERR_IO;
}



/* 识别数字主机；返回 1 表示成功，-1 表示已识别但地址族不匹配。 */
static int __xrtNetDNSNumeric(
	cstr sHost,
	uint16 iPort,
	xnetfamily Family,
	xnetaddr* pAddr
)
{
	char sNumeric[96];
	cstr sParse = sHost;
	size_t iSize = strlen(sHost);

	if ( (iSize >= 2) && (sHost[0] == '[') &&
		 (sHost[iSize - 1u] == ']') ) {
		size_t iInner = iSize - 2u;

		if ( iInner >= sizeof(sNumeric) ) {
			return 0;
		}
		memcpy(sNumeric, sHost + 1, iInner);
		sNumeric[iInner] = 0;
		sParse = sNumeric;
	}
	if ( !xrtNetAddrParse(pAddr, sParse, iPort) ) {
		xrtClearError();
		return 0;
	}
	if ( (Family != XNET_FAMILY_UNSPEC) &&
		 (pAddr->Family != (uint16)Family) ) {
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_DNS_RESULT,
			"resolve", "numeric host does not match requested family", 0);
		return -1;
	}
	return 1;
}



/* 解析主机的完整去重地址列表，不施加人为结果数量上限。 */
static xnetaddrlist* __xrtNetResolve(
	cstr sHost,
	uint16 iPort,
	xnetfamily Family
)
{
	struct addrinfo Hints;
	struct addrinfo* pResult = NULL;
	struct addrinfo* pCurrent;
	xnetaddrlist* pList;
	xnetaddr Numeric;
	int iFamily;
	int iNumeric;
	int iResult;

	if ( (sHost == NULL) || (sHost[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtNetDNSFamily(Family, &iFamily) ) {
		return NULL;
	}
	iNumeric = __xrtNetDNSNumeric(sHost, iPort, Family, &Numeric);
	if ( iNumeric > 0 ) {
		pList = __xrtNetAddrListReserve(1);
		if ( pList != NULL ) {
			pList->Addresses[0] = Numeric;
			pList->Count = 1;
		}
		return pList;
	}
	if ( iNumeric < 0 ) {
		return NULL;
	}
	if ( !__xrtNetEnsure() ) {
		return NULL;
	}

	memset(&Hints, 0, sizeof(Hints));
	Hints.ai_family = iFamily;
	iResult = getaddrinfo(sHost, NULL, &Hints, &pResult);
	if ( iResult != 0 ) {
		__xrtNetSetError(__xrtNetDNSErrorKind(iResult),
			XNET_ERROR_DNS_RESOLVE, "resolve",
			"host name resolution failed", iResult);
		return NULL;
	}
	pList = __xrtNetAddrListReserve(XRT_NET_DNS_INITIAL_CAPACITY);
	if ( pList == NULL ) {
		freeaddrinfo(pResult);
		return NULL;
	}

	for ( pCurrent = pResult; pCurrent != NULL;
		pCurrent = pCurrent->ai_next ) {
		xnetaddr Address;

		if ( (pCurrent->ai_addr == NULL) ||
			 !xrtNetAddrFromNative(
				&Address,
				pCurrent->ai_addr,
				(size_t)pCurrent->ai_addrlen
			) ) {
			xrtClearError();
			continue;
		}
		Address.Port = iPort;
		if ( !__xrtNetAddrListAppend(&pList, &Address) ) {
			freeaddrinfo(pResult);
			xrtNetAddrListDestroy(pList);
			return NULL;
		}
	}
	freeaddrinfo(pResult);
	if ( pList->Count == 0 ) {
		xrtNetAddrListDestroy(pList);
		__xrtNetSetError(XERR_NOT_FOUND, XNET_ERROR_DNS_RESULT,
			"resolve", "resolver returned no supported addresses", 0);
		return NULL;
	}
	return pList;
}



/* 解析并复制第一个地址，失败时保持调用方输出不变。 */
/* 只查询主机地址，地址端口保持为零，供 Resolver 缓存完整结果。 */
XRT_API xnetaddrlist* xrtNetLookup(cstr sHost, xnetfamily Family)
{
	return __xrtNetResolve(sHost, 0, Family);
}



/* 查询主机地址并把调用方端口写入全部结果。 */
XRT_API xnetaddrlist* xrtNetResolve(
	cstr sHost,
	uint16 iPort,
	xnetfamily Family
)
{
	return __xrtNetResolve(sHost, iPort, Family);
}



XRT_API bool xrtNetResolveOne(
	xnetaddr* pAddr,
	cstr sHost,
	uint16 iPort,
	xnetfamily Family
)
{
	xnetaddrlist* pList;
	xnetaddr Address;

	if ( pAddr == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	pList = xrtNetResolve(sHost, iPort, Family);
	if ( pList == NULL ) {
		return false;
	}
	Address = pList->Addresses[0];
	xrtNetAddrListDestroy(pList);
	*pAddr = Address;
	return true;
}



/* 通过系统名称服务执行 PTR 反向解析，不把数字回退伪装成主机名。 */
XRT_API str xrtNetReverse(const xnetaddr* pAddr)
{
	unsigned char Native[128];
	size_t iNativeSize = sizeof(Native);
	char sHost[XRT_NET_DNS_NAME_CAPACITY];
	int iResult;
	str sResult;

	if ( pAddr == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtNetEnsure() ||
		 !xrtNetAddrToNative(pAddr, Native, &iNativeSize) ) {
		return NULL;
	}
	iResult = getnameinfo(
		(const struct sockaddr*)Native,
		(socklen_t)iNativeSize,
		sHost,
		(socklen_t)sizeof(sHost),
		NULL,
		0,
		NI_NAMEREQD
	);
	if ( iResult != 0 ) {
		__xrtNetSetError(__xrtNetDNSErrorKind(iResult),
			XNET_ERROR_DNS_REVERSE, "reverse",
			"reverse host name resolution failed", iResult);
		return NULL;
	}
	sResult = (str)xrtMemDup(sHost, strlen(sHost) + 1u);
	return sResult;
}



/* 复制、校验并按完整端点去重，建立调用方可共享的不可变列表。 */
XRT_API xnetaddrlist* xrtNetAddrListCreate(
	const xnetaddr* pAddresses,
	size_t iCount
)
{
	xnetaddrlist* pList;

	if ( (pAddresses == NULL) || (iCount == 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pList = __xrtNetAddrListReserve(iCount);
	if ( pList == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (pAddresses[i].Family != XNET_FAMILY_IPV4) &&
			 (pAddresses[i].Family != XNET_FAMILY_IPV6) ) {
			xrtNetAddrListDestroy(pList);
			__xrtNetSetError(XERR_VALUE, XNET_ERROR_FAMILY,
				"create-addresses", "address list contains an invalid family", 0);
			return NULL;
		}
		if ( !__xrtNetAddrListAppend(&pList, &pAddresses[i]) ) {
			xrtNetAddrListDestroy(pList);
			return NULL;
		}
	}
	return pList;
}



/* 在保持地址顺序的同时统一端口；无变化时直接共享原列表。 */
XRT_API xnetaddrlist* xrtNetAddrListWithPort(
	xnetaddrlist* pList,
	uint16 iPort
)
{
	xnetaddrlist* pResult;
	bool bSame = true;

	if ( pList == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	for ( size_t i = 0; i < pList->Count; i++ ) {
		if ( pList->Addresses[i].Port != iPort ) {
			bSame = false;
			break;
		}
	}
	if ( bSame ) {
		return xrtNetAddrListRef(pList);
	}
	pResult = __xrtNetAddrListReserve(pList->Count);
	if ( pResult == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; i < pList->Count; i++ ) {
		xnetaddr Address = pList->Addresses[i];

		Address.Port = iPort;
		if ( !__xrtNetAddrListAppend(&pResult, &Address) ) {
			xrtNetAddrListDestroy(pResult);
			return NULL;
		}
	}
	return pResult;
}



XRT_API xnetaddrlist* xrtNetAddrListRef(xnetaddrlist* pList)
{
	if ( pList == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pList->References) < 0 ) {
		__xrtNetSetError(XERR_STATE, XNET_ERROR_DNS_RESULT,
			"retain-addresses", "address list reference is invalid", 0);
		return NULL;
	}
	return pList;
}



/* 释放不可变地址列表的最后一个引用。 */
XRT_API void xrtNetAddrListDestroy(xnetaddrlist* pList)
{
	if ( pList == NULL ) {
		return;
	}
	if ( xrtRefRelease(&pList->References) == 0 ) {
		xrtFree(pList);
	}
}



/* 返回地址列表数量。 */
XRT_API size_t xrtNetAddrListCount(const xnetaddrlist* pList)
{
	return pList != NULL ? pList->Count : 0;
}



/* 返回指定位置的借用地址。 */
XRT_API const xnetaddr* xrtNetAddrListGet(
	const xnetaddrlist* pList,
	size_t iIndex
)
{
	if ( pList == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iIndex >= pList->Count ) {
		__xrtNetSetError(XERR_RANGE, XNET_ERROR_DNS_RESULT,
			"get-address", "address list index is out of range", 0);
		return NULL;
	}
	return &pList->Addresses[iIndex];
}

#endif
