#ifndef XRT_XNET_BASE_H
#define XRT_XNET_BASE_H

/*
	XRT mainline network base model.

	This header defines the shared public types used by the modern xnet stack.
	It is designed to work both inside xrt.h and as a focused standalone header
	for transport development and testing.

	Public responsibilities:
	  - IPv4/IPv6 address and endpoint representation
	  - shared result codes, socket handles, and config structures
	  - forward declarations for engine, stream, datagram, listener, and future
		objects
*/


#if defined(XXRTL_CORE)
	#define __XNET_IN_XRT_CORE 1
#else
	#define __XNET_IN_XRT_CORE 0
#endif

/* ============================== Basic local types ============================== */

#if !__XNET_IN_XRT_CORE
	typedef void* ptr;
	typedef uint8_t uint8;
	typedef uint16_t uint16;
	typedef uint32_t uint32;
	typedef uint64_t uint64;
#endif

#if defined(_WIN32) || defined(_WIN64)
	#if !defined(XRT_XSOCKET_DEFINED)
		typedef SOCKET xsocket;
		#define XRT_XSOCKET_DEFINED 1
	#endif
	#ifndef XSOCKET_INVALID
		#define XSOCKET_INVALID INVALID_SOCKET
	#endif
#else
	#if !defined(XRT_XSOCKET_DEFINED)
		typedef int xsocket;
		#define XRT_XSOCKET_DEFINED 1
	#endif
	#ifndef XSOCKET_INVALID
		#define XSOCKET_INVALID (-1)
	#endif
#endif

#ifndef XNET_SOCKET_INVALID
	#define XNET_SOCKET_INVALID XSOCKET_INVALID
#endif

#if !__XNET_IN_XRT_CORE
	typedef enum {
		XRT_NET_OK      =  0,
		XRT_NET_ERROR   = -1,
		XRT_NET_AGAIN   = -2,
		XRT_NET_TIMEOUT = -3,
		XRT_NET_CLOSED  = -4,
		XRT_NET_CANCELLED = -5
	} xnet_result;
#endif

#if !__XNET_IN_XRT_CORE
	typedef struct xrt_tls_config xtlsconfig;
#endif



/* ============================== Opaque handles ============================== */

#if !defined(XRT_BUILD_CORE)

typedef struct xrt_net_engine   xnetengine;
typedef struct xrt_net_mem_ctx  xnetmemctx;
typedef struct xrt_net_worker   xnetworker;
typedef struct xrt_net_listener xnetlistener;
typedef struct xrt_net_stream   xnetstream;
typedef struct xrt_net_dgram    xdgramsock;
typedef struct xrt_net_chain    xnetchain;
typedef struct xrt_net_future   xnetfuture;
typedef struct xrt_net_proxy    xnetproxy;
typedef struct xrt_net_cancel   xnetcancel;
typedef struct xrt_net_cancel_watch xnetcancelwatch;
typedef struct xrt_net_addr_list xnetaddrlist;
typedef struct xrt_tls_session  xtlssession;

typedef void (*xnet_task_fn)(xnetworker* pWorker, ptr pArg);
typedef void (*xnetcancelfn)(ptr pArg);



/* ============================== Address model ============================== */

typedef struct {
	uint16 iFamily;
	uint16 iPort;
	uint32 iScopeId;
	uint8 aAddr[16];
} xnetaddr;



/* ============================== Small public helpers ============================== */

typedef struct {
	const void* pData;
	uint32 iLen;
} xnetspan;

typedef struct {
	const void* pData;
	uint32 iLen;
	void (*pfnRelease)(ptr pCtx, const void* pData, size_t iLen);
	ptr pReleaseCtx;
} xnetbufref;

typedef struct {
	void* pData;
	size_t iLen;
} xnetbytes;

#define XNET_CONTEXT_VERSION 1u
#define XNET_ERROR_VERSION   1u
#define XNET_CONFIG_VERSION  1u
#define XNET_ERROR_MESSAGE_CAP 192u

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iFlags;
	uint32 iReserved;
	int64_t iDeadlineMs;
	xnetcancel* pCancel;
	ptr pUserData;
} xnetcontext;

#define XNET_CONTEXT_V1_SIZE ((uint32)(offsetof(xnetcontext, pUserData) + sizeof(((xnetcontext*)0)->pUserData)))

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xnet_result iResult;
	uint32 iOperation;
	uint32 iPhase;
	uint32 iBackend;
	int iSystemError;
	char sMessage[XNET_ERROR_MESSAGE_CAP];
} xneterror;



/* ============================== Flags ============================== */

#define XNET_ENGINE_F_NONE            0x00000000u
#define XNET_ENGINE_F_AUTO_WORKERS    0x00000001u

#define XNET_LISTEN_F_NONE            0x00000000u
#define XNET_LISTEN_F_REUSE_ADDR      0x00000001u
#define XNET_LISTEN_F_REUSE_PORT      0x00000002u
#define XNET_LISTEN_F_NO_DELAY        0x00000004u
#define XNET_LISTEN_F_KEEPALIVE       0x00000008u
#define XNET_LISTEN_F_PULL_ACCEPT      0x00000010u
#define XNET_LISTEN_F_EXCLUSIVE_ADDR   0x00000020u

#define XNET_CONNECT_F_NONE           0x00000000u
#define XNET_CONNECT_F_NO_DELAY       0x00000001u
#define XNET_CONNECT_F_KEEPALIVE      0x00000002u

#define XNET_PROXY_NONE               0
#define XNET_PROXY_SOCKS5             1
#define XNET_PROXY_HTTP_CONNECT       2

#define XNET_PROXY_HOST_CAP           256u
#define XNET_PROXY_USER_CAP           128u
#define XNET_PROXY_PASS_CAP           128u

#define XNET_DGRAM_F_NONE             0x00000000u
#define XNET_DGRAM_F_REUSE_ADDR       0x00000001u
#define XNET_DGRAM_F_REUSE_PORT       0x00000002u
#define XNET_DGRAM_F_BROADCAST        0x00000004u
#define XNET_DGRAM_F_IPV6_ONLY        0x00000008u
#define XNET_DGRAM_F_DUAL_STACK       0x00000010u
#define XNET_DGRAM_F_CONNECTED        0x00000020u

#define XNET_DGRAM_OVERFLOW_DROP_OLDEST 0u
#define XNET_DGRAM_OVERFLOW_DROP_NEWEST 1u
#define XNET_DGRAM_OVERFLOW_ERROR       2u

#define XNET_CLOSE_F_ABORT            0x00000001u
#define XNET_CLOSE_F_GRACEFUL         0x00000002u
#define XNET_CLOSE_F_WAIT_PEER        0x00000004u

#define XNET_OP_NONE                  0u
#define XNET_OP_RESOLVE               1u
#define XNET_OP_CONNECT               2u
#define XNET_OP_ACCEPT                3u
#define XNET_OP_READ                  4u
#define XNET_OP_WRITE                 5u
#define XNET_OP_SEND                  6u
#define XNET_OP_RECV                  7u
#define XNET_OP_WAIT                  8u
#define XNET_OP_CLOSE                 9u

#define XNET_PHASE_NONE               0u
#define XNET_PHASE_VALIDATE           1u
#define XNET_PHASE_SUBMIT             2u
#define XNET_PHASE_COMPLETE           3u
#define XNET_PHASE_TIMEOUT            4u
#define XNET_PHASE_CANCEL             5u

#define XNET_BACKEND_NONE             0u
#define XNET_BACKEND_IOCP             1u
#define XNET_BACKEND_IO_URING         2u
#define XNET_BACKEND_EPOLL            3u
#define XNET_BACKEND_KQUEUE           4u
#define XNET_BACKEND_SELECT           5u



/* ============================== Config structs ============================== */

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint32 iWorkerCount;
	uint32 iFlags;
	uint32 iSqEntries;
	uint32 iCqEntries;
	uint32 iAcceptBatch;
	uint32 iCmdQueueSize;
	uint32 iTimerTickMs;
	uint32 iTimerWheelSlots;
	uint32 iDefaultHighWater;
	uint32 iDefaultLowWater;
	uint32 iSmallBlockSize;
	uint32 iMediumBlockSize;
	uint32 iLargeBlockSize;
	uint32 iBlockCachePerWorker;
	uint32 iMaxConnsPerWorker;
	uint32 iDefaultMaxQueuedBytes;
	uint32 iResolverThreads;
	uint32 iResolverQueueLimit;
	uint32 iResolverCacheEntries;
	uint32 iResolverCacheTtlMs;
} xnetengineconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	uint32 iMaxQueuedBytes;
	uint32 iAcceptConcurrency;
	uint32 iAcceptQueueLimit;
} xnetlistenconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iType;
	char sHost[XNET_PROXY_HOST_CAP];
	uint16 iPort;
	char sUser[XNET_PROXY_USER_CAP];
	char sPass[XNET_PROXY_PASS_CAP];
} xnetproxyconfig;

struct xrt_net_proxy {
	volatile long iRefCount;
	xnetproxyconfig tConfig;
};

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	const char* sHost;
	uint16 iPort;
	uint16 iAddressFamily;
	uint32 iFlags;
	uint32 iConnectTimeoutMs;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
	xnetproxy* pProxy;
	uint32 iMaxQueuedBytes;
	uint32 iFallbackDelayMs;
} xnetconnectconfig;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iRecvBatch;
	uint32 iSendQueueLimit;
	uint32 iRecvQueueLimit;
	uint32 iRecvOverflowPolicy;
	xnetaddr tPeerAddr;
	uint32 iRecvBufferSize;
	uint32 iSendBufferSize;
	int32 iHopLimit;
	int32 iTrafficClass;
} xnetdgramconfig;

typedef struct {
	const xnetaddr* pTo;
	const void* pData;
	size_t iLen;
} xnetdgramsenditem;

typedef struct {
	uint64 iRecvPackets;
	uint64 iRecvBytes;
	uint64 iSendPackets;
	uint64 iSendBytes;
	uint64 iRecvDropped;
	uint64 iSendAgain;
	uint64 iErrors;
	uint64 iPendingSendBytes;
	uint32 iRecvQueued;
	uint32 iRecvArmed;
} xnetdgramstats;

#endif /* !XRT_BUILD_CORE */

struct xrt_net_addr_list {
	uint32 iCount;
	xnetaddr* arrAddrs;
};

#ifndef XNET_ENGINE_CONFIG_V1_SIZE
	#define XNET_ENGINE_CONFIG_V1_SIZE  ((uint32)(offsetof(xnetengineconfig, iResolverCacheTtlMs) + sizeof(((xnetengineconfig*)0)->iResolverCacheTtlMs)))
	#define XNET_LISTEN_CONFIG_V1_SIZE  ((uint32)(offsetof(xnetlistenconfig, iAcceptQueueLimit) + sizeof(((xnetlistenconfig*)0)->iAcceptQueueLimit)))
	#define XNET_PROXY_CONFIG_V1_SIZE   ((uint32)(offsetof(xnetproxyconfig, sPass) + sizeof(((xnetproxyconfig*)0)->sPass)))
	#define XNET_CONNECT_CONFIG_V1_SIZE ((uint32)(offsetof(xnetconnectconfig, iMaxQueuedBytes) + sizeof(((xnetconnectconfig*)0)->iMaxQueuedBytes)))
	#define XNET_DGRAM_CONFIG_V1_SIZE   ((uint32)(offsetof(xnetdgramconfig, iTrafficClass) + sizeof(((xnetdgramconfig*)0)->iTrafficClass)))
#endif



/* ============================== Internal address helpers ============================== */

#define __XNET_ADDR_STR_CAP 80

#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
	#define __XNET_THREAD_LOCAL __declspec(thread)
#elif defined(_MSC_VER)
	#define __XNET_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
	#define __XNET_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
	#define __XNET_THREAD_LOCAL _Thread_local
#else
	#define __XNET_THREAD_LOCAL
#endif

static __XNET_THREAD_LOCAL xneterror __g_xnetLastError;

static void __xnetErrorSetEx(xnet_result iResult, uint32 iOperation, uint32 iPhase,
	uint32 iBackend, int iSystemError, const char* sMessage)
{
	xneterror* pError = &__g_xnetLastError;
	memset(pError, 0, sizeof(*pError));
	pError->iSize = (uint32)sizeof(*pError);
	pError->iVersion = XNET_ERROR_VERSION;
	pError->iResult = iResult;
	pError->iOperation = iOperation;
	pError->iPhase = iPhase;
	pError->iBackend = iBackend;
	pError->iSystemError = iSystemError;
	if ( sMessage ) {
		strncpy(pError->sMessage, sMessage, sizeof(pError->sMessage) - 1u);
		pError->sMessage[sizeof(pError->sMessage) - 1u] = '\0';
	}
	#if __XNET_IN_XRT_CORE
		if ( sMessage && sMessage[0] != '\0' ) { xrtSetError((str)sMessage, FALSE); }
	#endif
}

XXAPI void xrtNetErrorClear(void)
{
	memset(&__g_xnetLastError, 0, sizeof(__g_xnetLastError));
	__g_xnetLastError.iSize = (uint32)sizeof(__g_xnetLastError);
	__g_xnetLastError.iVersion = XNET_ERROR_VERSION;
	__g_xnetLastError.iResult = XRT_NET_OK;
}

XXAPI bool xrtNetErrorGet(xneterror* pError)
{
	if ( !pError ) { return false; }
	*pError = __g_xnetLastError;
	if ( pError->iSize == 0 ) {
		pError->iSize = (uint32)sizeof(*pError);
		pError->iVersion = XNET_ERROR_VERSION;
		pError->iResult = XRT_NET_OK;
	}
	return pError->iResult != XRT_NET_OK;
}

// 内部函数：__xnetAtomicCompareExchange32
static long __xnetAtomicCompareExchange32(volatile long* pValue, long iExchange, long iComparand)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicCompareExchange32(pValue, iExchange, iComparand);
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iExchange, (LONG)iComparand);
	#else
		return __sync_val_compare_and_swap(pValue, iComparand, iExchange);
	#endif
}


// 内部函数：__xnetAtomicExchange32
static long __xnetAtomicExchange32(volatile long* pValue, long iValue)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicExchange32(pValue, iValue);
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)InterlockedExchange((volatile LONG*)pValue, (LONG)iValue);
	#else
		return __sync_lock_test_and_set(pValue, iValue);
	#endif
}


// 内部函数：__xnetAtomicAddFetch32
static long __xnetAtomicAddFetch32(volatile long* pValue, long iDelta)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64) && (defined(__x86_64__) || defined(_M_X64))
		return __xrtAtomicAddFetch32(pValue, iDelta);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		long iPrev;
		long iNext;
		do {
			iPrev = (long)InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
			iNext = iPrev + iDelta;
		} while ( (long)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iNext, (LONG)iPrev) != iPrev );
		return iNext;
	#elif defined(_WIN32) || defined(_WIN64)
		return (long)(InterlockedExchangeAdd((volatile LONG*)pValue, (LONG)iDelta) + (LONG)iDelta);
	#else
		return __sync_add_and_fetch(pValue, iDelta);
	#endif
}


// 内部函数：__xnetAtomicAddFetch64
static int64 __xnetAtomicAddFetch64(volatile int64* pValue, int64 iDelta)
{
	return __xrtAtomicAddFetch64(pValue, iDelta);
}


static int64 __xnetAtomicCompareExchange64(volatile int64* pValue, int64 iExchange, int64 iComparand)
{
	return __xrtAtomicCompareExchange64(pValue, iExchange, iComparand);
}


// 内部函数：__xnetAtomicLoad64
static int64 __xnetAtomicLoad64(const volatile int64* pValue)
{
	return __xrtAtomicLoad64(pValue);
}


// 内部函数：__xnetAtomicLoad32
static long __xnetAtomicLoad32(const volatile long* pValue)
{
	return __xnetAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}


// 内部函数：__xnetAddrFromSockAddr
static bool __xnetAddrFromSockAddr(xnetaddr* pAddr, const struct sockaddr* pSA)
{
	if ( !pAddr || !pSA ) { return false; }

	memset(pAddr, 0, sizeof(xnetaddr));
	if ( pSA->sa_family == AF_INET ) {
		const struct sockaddr_in* pSA4 = (const struct sockaddr_in*)pSA;
		pAddr->iFamily = AF_INET;
		pAddr->iPort = ntohs(pSA4->sin_port);
		memcpy(pAddr->aAddr, &pSA4->sin_addr, 4);
		return true;
	}
	if ( pSA->sa_family == AF_INET6 ) {
		const struct sockaddr_in6* pSA6 = (const struct sockaddr_in6*)pSA;
		pAddr->iFamily = AF_INET6;
		pAddr->iPort = ntohs(pSA6->sin6_port);
		pAddr->iScopeId = pSA6->sin6_scope_id;
		memcpy(pAddr->aAddr, &pSA6->sin6_addr, 16);
		return true;
	}
	return false;
}


// 内部函数：复制固定长度字符串
static void __xnetCopyFixedString(char* sDst, size_t iDstCap, const char* sSrc)
{
	size_t iLen;
	if ( !sDst || iDstCap == 0 ) { return; }
	sDst[0] = '\0';
	if ( !sSrc || !sSrc[0] ) { return; }
	iLen = strlen(__xrt_cstr(sSrc));
	if ( iLen >= iDstCap ) { iLen = iDstCap - 1u; }
	memcpy(sDst, sSrc, iLen);
	sDst[iLen] = '\0';
}


// 内部函数：判断代理配置是否有效
static bool __xnetConfigHeaderIsValid(uint32 iSize, uint32 iVersion, size_t iExpectedSize)
{
	return iVersion == XNET_CONFIG_VERSION && iSize >= iExpectedSize;
}


static bool __xnetConfigCopyKnown(void* pDst, size_t iDstSize, const void* pSrc,
	uint32 iSrcSize, uint32 iVersion, size_t iMinSize)
{
	size_t iCopySize;
	if ( !pDst || !pSrc || !__xnetConfigHeaderIsValid(iSrcSize, iVersion, iMinSize) ) {
		return false;
	}
	iCopySize = iSrcSize < iDstSize ? (size_t)iSrcSize : iDstSize;
	memcpy(pDst, pSrc, iCopySize);
	return true;
}


static bool __xnetProxyConfigIsValid(const xnetproxyconfig* pCfg)
{
	if ( !pCfg ) { return false; }
	if ( !__xnetConfigHeaderIsValid(pCfg->iSize, pCfg->iVersion, XNET_PROXY_CONFIG_V1_SIZE) ) { return false; }
	if ( pCfg->iType != XNET_PROXY_SOCKS5 && pCfg->iType != XNET_PROXY_HTTP_CONNECT ) { return false; }
	if ( !pCfg->sHost[0] || pCfg->iPort == 0 ) { return false; }
	return true;
}


// 内部函数：__xnetAddrToSockAddr
static bool __xnetAddrToSockAddr(const xnetaddr* pAddr, struct sockaddr_storage* pStorage, socklen_t* pLen)
{
	if ( !pAddr || !pStorage || !pLen ) { return false; }
	memset(pStorage, 0, sizeof(struct sockaddr_storage));
	if ( pAddr->iFamily == AF_INET ) {
		struct sockaddr_in* pSA4 = (struct sockaddr_in*)pStorage;
		pSA4->sin_family = AF_INET;
		pSA4->sin_port = htons(pAddr->iPort);
		memcpy(&pSA4->sin_addr, pAddr->aAddr, 4);
		*pLen = sizeof(struct sockaddr_in);
		return true;
	}
	if ( pAddr->iFamily == AF_INET6 ) {
		struct sockaddr_in6* pSA6 = (struct sockaddr_in6*)pStorage;
		pSA6->sin6_family = AF_INET6;
		pSA6->sin6_port = htons(pAddr->iPort);
		pSA6->sin6_scope_id = pAddr->iScopeId;
		memcpy(&pSA6->sin6_addr, pAddr->aAddr, 16);
		*pLen = sizeof(struct sockaddr_in6);
		return true;
	}
	return false;
}


// 内部函数：__xnetAddrTempBuf
static char* __xnetAddrTempBuf(void)
{
	static __XNET_THREAD_LOCAL char aRing[4][__XNET_ADDR_STR_CAP];
	static __XNET_THREAD_LOCAL uint32 iIndex = 0;
	char* pBuf = aRing[iIndex & 3u];
	iIndex++;
	pBuf[0] = '\0';
	return pBuf;
}



/* ============================== Address public helpers ============================== */

XXAPI void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort)
{
	if ( !pAddr ) { return; }
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iFamily = (uint16)((iFamily == AF_INET6) ? AF_INET6 : AF_INET);
	pAddr->iPort = iPort;
}


// xrtNetAddrParse 相关处理
XXAPI xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort)
{
	#if defined(_WIN32) || defined(_WIN64)
		struct sockaddr_storage tStorage;
		int iStorageLen;
		char sAddrBuf[64];
	#endif
	if ( !pAddr || !sIP || !sIP[0] ) { return XRT_NET_ERROR; }

	// 清零地址结构并设置端口
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iPort = iPort;

	#if defined(_WIN32) || defined(_WIN64)
		// Windows 平台：先尝试作为 IPv4 地址解析
		// 利用 WSAStringToAddressA 将点分十进制字符串转换为 sockaddr_in，
		//      再从 sin_addr 中拷贝 4 字节网络序地址到 xnetaddr.aAddr
		memset(&tStorage, 0, sizeof(tStorage));
		iStorageLen = (int)sizeof(tStorage);
		snprintf(sAddrBuf, sizeof(sAddrBuf), "%s", sIP);
		if ( WSAStringToAddressA(sAddrBuf, AF_INET, NULL, (struct sockaddr*)&tStorage, &iStorageLen) == 0 ) {
			pAddr->iFamily = AF_INET;
			memcpy(pAddr->aAddr, &((struct sockaddr_in*)&tStorage)->sin_addr, 4u);
			return XRT_NET_OK;
		}
		// IPv4 解析失败，尝试作为 IPv6 地址解析
		// IPv6 额外保存 sin6_scope_id 用于链路本地地址的接口标识
		memset(&tStorage, 0, sizeof(tStorage));
		iStorageLen = (int)sizeof(tStorage);
		snprintf(sAddrBuf, sizeof(sAddrBuf), "%s", sIP);
		if ( WSAStringToAddressA(sAddrBuf, AF_INET6, NULL, (struct sockaddr*)&tStorage, &iStorageLen) == 0 ) {
			pAddr->iFamily = AF_INET6;
			pAddr->iScopeId = ((struct sockaddr_in6*)&tStorage)->sin6_scope_id;
			memcpy(pAddr->aAddr, &((struct sockaddr_in6*)&tStorage)->sin6_addr, 16u);
			return XRT_NET_OK;
		}
	#else
		// 非 Windows 平台：先尝试作为 IPv4 地址解析
		// inet_pton 将文本地址转为网络序二进制并直接写入 aAddr
		if ( inet_pton(AF_INET, sIP, pAddr->aAddr) == 1 ) {
			pAddr->iFamily = AF_INET;
			return XRT_NET_OK;
		}
		// IPv4 解析失败，尝试作为 IPv6 地址解析
		if ( inet_pton(AF_INET6, sIP, pAddr->aAddr) == 1 ) {
			pAddr->iFamily = AF_INET6;
			return XRT_NET_OK;
		}
	#endif

	// 两种格式均无法解析，重置地址并返回错误
	memset(pAddr, 0, sizeof(xnetaddr));
	return XRT_NET_ERROR;
}


// 解析网络
XXAPI xnet_result xrtNetResolve(const char* sHost, xnetaddr* pAddr)
{
	if ( !sHost || !sHost[0] || !pAddr ) { return XRT_NET_ERROR; }

	// 先尝试将 sHost 直接解析为 IP 地址字符串
	{
		xnetaddr tParsed;
		if ( xrtNetAddrParse(&tParsed, sHost, pAddr->iPort) == XRT_NET_OK ) {
			if ( (pAddr->iFamily == AF_INET || pAddr->iFamily == AF_INET6) &&
				tParsed.iFamily != pAddr->iFamily ) {
				return XRT_NET_ERROR;
			}
			tParsed.iScopeId = pAddr->iScopeId;
			*pAddr = tParsed;
			return XRT_NET_OK;
		}
	}

	// 直接解析失败，通过 DNS 解析主机名
	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	struct addrinfo* pIt = NULL;
	xnetaddr tFallback;
	bool bHasFallback = false;
	uint16 iPort = pAddr->iPort;
	uint16 iRequestedFamily = pAddr->iFamily;
	memset(&tFallback, 0, sizeof(tFallback));
	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = (iRequestedFamily == AF_INET || iRequestedFamily == AF_INET6)
		? (int)iRequestedFamily : AF_UNSPEC;
	tHints.ai_socktype = 0;

	if ( getaddrinfo(sHost, NULL, &tHints, &pRes) != 0 ) {
		return XRT_NET_ERROR;
	}

	// 遍历 DNS 返回结果，选取第一个可用地址
	for ( pIt = pRes; pIt; pIt = pIt->ai_next ) {
		xnetaddr tCandidate;
		if ( !pIt->ai_addr ) { continue; }
		memset(&tCandidate, 0, sizeof(tCandidate));
		if ( __xnetAddrFromSockAddr(&tCandidate, pIt->ai_addr) ) {
			tCandidate.iPort = iPort;
			if ( iRequestedFamily != 0u || tCandidate.iFamily == AF_INET ) {
				*pAddr = tCandidate;
				freeaddrinfo(pRes);
				return XRT_NET_OK;
			}
			if ( !bHasFallback ) {
				tFallback = tCandidate;
				bHasFallback = true;
			}
		}
	}
	if ( bHasFallback ) {
		*pAddr = tFallback;
		freeaddrinfo(pRes);
		return XRT_NET_OK;
	}

	// 所有结果均无法转换，释放资源并返回错误
	freeaddrinfo(pRes);
	return XRT_NET_ERROR;
}


XXAPI xnetaddrlist* xrtNetResolveAll(const char* sHost, uint16 iPort, int iFamily)
{
	#define __XNET_RESOLVE_ALL_LIMIT 64u
	struct addrinfo tHints;
	struct addrinfo* pResult = NULL;
	struct addrinfo* pIt;
	xnetaddr arrTemp[__XNET_RESOLVE_ALL_LIMIT];
	xnetaddr tNumeric;
	xnetaddrlist* pList;
	uint32 iCount = 0u;
	uint32 i;
	if ( !sHost || !sHost[0] || iPort == 0u ||
		(iFamily != AF_UNSPEC && iFamily != AF_INET && iFamily != AF_INET6) ) { return NULL; }
	memset(&tNumeric, 0, sizeof(tNumeric));
	if ( xrtNetAddrParse(&tNumeric, sHost, iPort) == XRT_NET_OK ) {
		if ( iFamily != AF_UNSPEC && tNumeric.iFamily != iFamily ) { return NULL; }
		arrTemp[iCount++] = tNumeric;
	} else {
		memset(&tHints, 0, sizeof(tHints));
		tHints.ai_family = iFamily;
		tHints.ai_socktype = 0;
		if ( getaddrinfo(sHost, NULL, &tHints, &pResult) != 0 ) { return NULL; }
		for ( pIt = pResult; pIt && iCount < __XNET_RESOLVE_ALL_LIMIT; pIt = pIt->ai_next ) {
			xnetaddr tCandidate;
			bool bDuplicate = false;
			if ( !pIt->ai_addr ) { continue; }
			memset(&tCandidate, 0, sizeof(tCandidate));
			if ( !__xnetAddrFromSockAddr(&tCandidate, pIt->ai_addr) ) { continue; }
			tCandidate.iPort = iPort;
			for ( i = 0u; i < iCount; ++i ) {
				if ( arrTemp[i].iFamily == tCandidate.iFamily &&
					arrTemp[i].iScopeId == tCandidate.iScopeId &&
					memcmp(arrTemp[i].aAddr, tCandidate.aAddr,
						tCandidate.iFamily == AF_INET ? 4u : 16u) == 0 ) {
					bDuplicate = true;
					break;
				}
			}
			if ( !bDuplicate ) { arrTemp[iCount++] = tCandidate; }
		}
		freeaddrinfo(pResult);
	}
	if ( iCount == 0u ) { return NULL; }
	pList = (xnetaddrlist*)xrtMalloc(sizeof(*pList));
	if ( !pList ) { return NULL; }
	memset(pList, 0, sizeof(*pList));
	pList->arrAddrs = (xnetaddr*)xrtMalloc(sizeof(xnetaddr) * iCount);
	if ( !pList->arrAddrs ) {
		xrtFree(pList);
		return NULL;
	}
	memcpy(pList->arrAddrs, arrTemp, sizeof(xnetaddr) * iCount);
	pList->iCount = iCount;
	return pList;
	#undef __XNET_RESOLVE_ALL_LIMIT
}


XXAPI uint32 xrtNetAddrListCount(const xnetaddrlist* pList)
{
	return pList ? pList->iCount : 0u;
}


XXAPI const xnetaddr* xrtNetAddrListGet(const xnetaddrlist* pList, uint32 iIndex)
{
	return pList && pList->arrAddrs && iIndex < pList->iCount ? &pList->arrAddrs[iIndex] : NULL;
}


XXAPI void xrtNetAddrListDestroy(xnetaddrlist* pList)
{
	if ( !pList ) { return; }
	xrtFree(pList->arrAddrs);
	xrtFree(pList);
}


// xrtNetAddrToStr 相关处理
XXAPI const char* xrtNetAddrToStr(const xnetaddr* pAddr)
{
	char* pBuf = __xnetAddrTempBuf();
	if ( !pAddr ) { return pBuf; }

	// IPv4 地址：直接转换为点分十进制字符串
	// inet_ntop 将 4 字节网络序地址转为可读文本
	if ( pAddr->iFamily == AF_INET ) {
		if ( !inet_ntop(AF_INET, pAddr->aAddr, pBuf, __XNET_ADDR_STR_CAP) ) {
			pBuf[0] = '\0';
		}
		return pBuf;
	}
	// IPv6 地址：转换后处理 scope id 后缀
	// inet_ntop 将 16 字节网络序地址转为冒号分隔文本，
	//      scope id 用 % 后缀标识链路本地地址所属的接口编号
	if ( pAddr->iFamily == AF_INET6 ) {
		char aAddr[INET6_ADDRSTRLEN];
		if ( !inet_ntop(AF_INET6, pAddr->aAddr, aAddr, sizeof(aAddr)) ) {
			pBuf[0] = '\0';
			return pBuf;
		}
		// 拼接 scope id（如 %42），用于链路本地地址标识接口
		if ( pAddr->iScopeId != 0 ) {
			snprintf(pBuf, __XNET_ADDR_STR_CAP, "%s%%%u", aAddr, pAddr->iScopeId);
		} else {
			strncpy(pBuf, aAddr, __XNET_ADDR_STR_CAP - 1);
			pBuf[__XNET_ADDR_STR_CAP - 1] = '\0';
		}
		return pBuf;
	}

	pBuf[0] = '\0';
	return pBuf;
}



/* ============================== Default init helpers ============================== */

XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xnetengineconfig));
	pCfg->iSize = (uint32)sizeof(*pCfg);
	pCfg->iVersion = XNET_CONFIG_VERSION;
	pCfg->iWorkerCount = 0;
	pCfg->iFlags = XNET_ENGINE_F_AUTO_WORKERS;
	pCfg->iSqEntries = 4096;
	pCfg->iCqEntries = 8192;
	pCfg->iAcceptBatch = 64;
	pCfg->iCmdQueueSize = 65536;
	pCfg->iTimerTickMs = 10;
	pCfg->iTimerWheelSlots = 4096;
	pCfg->iDefaultHighWater = 262144;
	pCfg->iDefaultLowWater = 65536;
	pCfg->iSmallBlockSize = 256;
	pCfg->iMediumBlockSize = 2048;
	pCfg->iLargeBlockSize = 16384;
	pCfg->iBlockCachePerWorker = 256;
	pCfg->iMaxConnsPerWorker = 0;
	pCfg->iDefaultMaxQueuedBytes = 1048576;
	pCfg->iResolverThreads = 2;
	pCfg->iResolverQueueLimit = 4096;
	pCfg->iResolverCacheEntries = 256;
	pCfg->iResolverCacheTtlMs = 60000;
}


// 初始化网络 listen 配置
XXAPI void xrtNetListenConfigInit(xnetlistenconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xnetlistenconfig));
	pCfg->iSize = (uint32)sizeof(*pCfg);
	pCfg->iVersion = XNET_CONFIG_VERSION;
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	#if defined(_WIN32) || defined(_WIN64)
		/* Windows 的 SO_REUSEADDR 会允许另一个进程抢占活动监听地址。 */
		pCfg->iFlags = XNET_LISTEN_F_EXCLUSIVE_ADDR | XNET_LISTEN_F_NO_DELAY;
	#else
		pCfg->iFlags = XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	#endif
	pCfg->iBacklog = 512;
	pCfg->iHighWater = 262144;
	pCfg->iLowWater = 65536;
	pCfg->iRecvLimit = 1048576;
	pCfg->pTlsConfig = NULL;
	pCfg->iMaxQueuedBytes = 1048576;
	pCfg->iAcceptConcurrency = 0;
	pCfg->iAcceptQueueLimit = 256;
}


// 初始化网络代理配置
XXAPI void xrtNetProxyConfigInit(xnetproxyconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xnetproxyconfig));
	pCfg->iSize = (uint32)sizeof(*pCfg);
	pCfg->iVersion = XNET_CONFIG_VERSION;
	pCfg->iType = XNET_PROXY_NONE;
}


// 创建网络代理
XXAPI xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pCfg)
{
	xnetproxy* pProxy;
	if ( !__xnetProxyConfigIsValid(pCfg) ) { return NULL; }
	pProxy = (xnetproxy*)xrtMalloc(sizeof(xnetproxy));
	if ( !pProxy ) { return NULL; }
	memset(pProxy, 0, sizeof(xnetproxy));
	pProxy->iRefCount = 1;
	pProxy->tConfig.iSize = (uint32)sizeof(pProxy->tConfig);
	pProxy->tConfig.iVersion = XNET_CONFIG_VERSION;
	pProxy->tConfig.iType = pCfg->iType;
	pProxy->tConfig.iPort = pCfg->iPort;
	__xnetCopyFixedString(pProxy->tConfig.sHost, sizeof(pProxy->tConfig.sHost), pCfg->sHost);
	__xnetCopyFixedString(pProxy->tConfig.sUser, sizeof(pProxy->tConfig.sUser), pCfg->sUser);
	__xnetCopyFixedString(pProxy->tConfig.sPass, sizeof(pProxy->tConfig.sPass), pCfg->sPass);
	return pProxy;
}


// 增加网络代理 ref
XXAPI xnetproxy* xrtNetProxyAddRef(xnetproxy* pProxy)
{
	if ( !pProxy ) { return NULL; }
	(void)__xnetAtomicAddFetch32(&pProxy->iRefCount, 1);
	return pProxy;
}


// 释放网络代理
XXAPI void xrtNetProxyRelease(xnetproxy* pProxy)
{
	if ( !pProxy ) { return; }
	if ( __xnetAtomicAddFetch32(&pProxy->iRefCount, -1) == 0 ) {
		xrtFree(pProxy);
	}
}


// 初始化网络 connect 配置
XXAPI void xrtNetConnectConfigInit(xnetconnectconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xnetconnectconfig));
	pCfg->iSize = (uint32)sizeof(*pCfg);
	pCfg->iVersion = XNET_CONFIG_VERSION;
	pCfg->sHost = NULL;
	pCfg->iPort = 0;
	pCfg->iAddressFamily = AF_UNSPEC;
	pCfg->iFlags = XNET_CONNECT_F_NO_DELAY;
	pCfg->iConnectTimeoutMs = 5000;
	pCfg->iHighWater = 262144;
	pCfg->iLowWater = 65536;
	pCfg->iRecvLimit = 1048576;
	pCfg->pTlsConfig = NULL;
	pCfg->pProxy = NULL;
	pCfg->iMaxQueuedBytes = 1048576;
	pCfg->iFallbackDelayMs = 250u;
}


// 初始化网络数据报配置
XXAPI void xrtNetDgramConfigInit(xnetdgramconfig* pCfg)
{
	if ( !pCfg ) { return; }
	memset(pCfg, 0, sizeof(xnetdgramconfig));
	pCfg->iSize = (uint32)sizeof(*pCfg);
	pCfg->iVersion = XNET_CONFIG_VERSION;
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iFlags = XNET_DGRAM_F_REUSE_ADDR;
	pCfg->iRecvBatch = 1;
	pCfg->iSendQueueLimit = 262144;
	pCfg->iRecvQueueLimit = 256;
	pCfg->iRecvOverflowPolicy = XNET_DGRAM_OVERFLOW_DROP_OLDEST;
	pCfg->iHopLimit = -1;
	pCfg->iTrafficClass = -1;
}


#endif
