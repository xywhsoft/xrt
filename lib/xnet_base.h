#ifndef XRT_XNET_BASE_H
#define XRT_XNET_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <ws2tcpip.h>
#else
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <sys/socket.h>
#endif


/*
    XNet V2 - Base Types and Config

    This is the staging header for the xnet-v2 rewrite.
    It is intentionally self-contained and is not wired into xrt.h yet.

    Phase-1 note:
      - This header defines the widened IPv4/IPv6 address model used by xnet-v2.
      - It must be tested through the dedicated xnet2 harness, not the legacy
        omnibus test.c path, until xrt.h migration begins.
*/


/* ============================== Basic local types ============================== */

typedef void* ptr;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#if defined(_WIN32) || defined(_WIN64)
	typedef SOCKET xsocket;
	#define XNET_SOCKET_INVALID INVALID_SOCKET
#else
	typedef int xsocket;
	#define XNET_SOCKET_INVALID (-1)
#endif

typedef enum {
	XRT_NET_OK      =  0,
	XRT_NET_ERROR   = -1,
	XRT_NET_AGAIN   = -2,
	XRT_NET_TIMEOUT = -3,
	XRT_NET_CLOSED  = -4
} xnet_result;

typedef struct xrt_tls_config xtlsconfig;



/* ============================== Opaque handles ============================== */

typedef struct xrt_net_engine   xnetengine;
typedef struct xrt_net_mem_ctx  xnetmemctx;
typedef struct xrt_net_worker   xnetworker;
typedef struct xrt_net_listener xnetlistener;
typedef struct xrt_net_stream   xnetstream;
typedef struct xrt_net_dgram    xdgramsock;
typedef struct xrt_net_chain    xnetchain;
typedef struct xrt_net_future   xnetfuture;
typedef struct xrt_tls_session  xtlssession;

typedef void (*xnet_task_fn)(xnetworker* pWorker, ptr pArg);



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



/* ============================== Flags ============================== */

#define XNET_ENGINE_F_NONE            0x00000000u
#define XNET_ENGINE_F_AUTO_WORKERS    0x00000001u

#define XNET_LISTEN_F_NONE            0x00000000u
#define XNET_LISTEN_F_REUSE_ADDR      0x00000001u
#define XNET_LISTEN_F_REUSE_PORT      0x00000002u
#define XNET_LISTEN_F_NO_DELAY        0x00000004u
#define XNET_LISTEN_F_KEEPALIVE       0x00000008u

#define XNET_CONNECT_F_NONE           0x00000000u
#define XNET_CONNECT_F_NO_DELAY       0x00000001u
#define XNET_CONNECT_F_KEEPALIVE      0x00000002u

#define XNET_DGRAM_F_NONE             0x00000000u
#define XNET_DGRAM_F_REUSE_ADDR       0x00000001u
#define XNET_DGRAM_F_REUSE_PORT       0x00000002u

#define XNET_CLOSE_F_ABORT            0x00000001u
#define XNET_CLOSE_F_GRACEFUL         0x00000002u



/* ============================== Config structs ============================== */

typedef struct {
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
} xnetengineconfig;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iBacklog;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
} xnetlistenconfig;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iFlags;
	uint32 iConnectTimeoutMs;
	uint32 iHighWater;
	uint32 iLowWater;
	uint32 iRecvLimit;
	const xtlsconfig* pTlsConfig;
} xnetconnectconfig;

typedef struct {
	xnetaddr tBindAddr;
	uint32 iFlags;
	uint32 iRecvBatch;
	uint32 iSendQueueLimit;
} xnetdgramconfig;



/* ============================== Internal address helpers ============================== */

#define __XNET_ADDR_STR_CAP 64

#if defined(_MSC_VER)
	#define __XNET_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
	#define __XNET_THREAD_LOCAL __thread
#else
	#define __XNET_THREAD_LOCAL
#endif

static bool __xnetAddrFromSockAddr(xnetaddr* pAddr, const struct sockaddr* pSA)
{
	if ( !pAddr || !pSA ) return false;

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

static bool __xnetAddrToSockAddr(const xnetaddr* pAddr, struct sockaddr_storage* pStorage, socklen_t* pLen)
{
	if ( !pAddr || !pStorage || !pLen ) return false;
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

static void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort)
{
	if ( !pAddr ) return;
	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iFamily = (uint16)((iFamily == AF_INET6) ? AF_INET6 : AF_INET);
	pAddr->iPort = iPort;
}

static xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort)
{
	if ( !pAddr || !sIP || !sIP[0] ) return XRT_NET_ERROR;

	memset(pAddr, 0, sizeof(xnetaddr));
	pAddr->iPort = iPort;

	if ( inet_pton(AF_INET, sIP, pAddr->aAddr) == 1 ) {
		pAddr->iFamily = AF_INET;
		return XRT_NET_OK;
	}
	if ( inet_pton(AF_INET6, sIP, pAddr->aAddr) == 1 ) {
		pAddr->iFamily = AF_INET6;
		return XRT_NET_OK;
	}

	memset(pAddr, 0, sizeof(xnetaddr));
	return XRT_NET_ERROR;
}

static xnet_result xrtNetResolve(const char* sHost, xnetaddr* pAddr)
{
	if ( !sHost || !sHost[0] || !pAddr ) return XRT_NET_ERROR;

	{
		xnetaddr tParsed;
		if ( xrtNetAddrParse(&tParsed, sHost, pAddr->iPort) == XRT_NET_OK ) {
			tParsed.iScopeId = pAddr->iScopeId;
			*pAddr = tParsed;
			return XRT_NET_OK;
		}
	}

	struct addrinfo tHints;
	struct addrinfo* pRes = NULL;
	struct addrinfo* pIt = NULL;
	uint16 iPort = pAddr->iPort;
	memset(&tHints, 0, sizeof(tHints));
	tHints.ai_family = AF_UNSPEC;
	tHints.ai_socktype = SOCK_STREAM;

	if ( getaddrinfo(sHost, NULL, &tHints, &pRes) != 0 ) {
		return XRT_NET_ERROR;
	}

	for ( pIt = pRes; pIt; pIt = pIt->ai_next ) {
		if ( !pIt->ai_addr ) continue;
		if ( __xnetAddrFromSockAddr(pAddr, pIt->ai_addr) ) {
			pAddr->iPort = iPort;
			freeaddrinfo(pRes);
			return XRT_NET_OK;
		}
	}

	freeaddrinfo(pRes);
	return XRT_NET_ERROR;
}

static const char* xrtNetAddrToStr(const xnetaddr* pAddr)
{
	char* pBuf = __xnetAddrTempBuf();
	if ( !pAddr ) return pBuf;

	if ( pAddr->iFamily == AF_INET ) {
		if ( !inet_ntop(AF_INET, pAddr->aAddr, pBuf, __XNET_ADDR_STR_CAP) ) {
			pBuf[0] = '\0';
		}
		return pBuf;
	}
	if ( pAddr->iFamily == AF_INET6 ) {
		char aAddr[INET6_ADDRSTRLEN];
		if ( !inet_ntop(AF_INET6, pAddr->aAddr, aAddr, sizeof(aAddr)) ) {
			pBuf[0] = '\0';
			return pBuf;
		}
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

static void xrtNetEngineConfigInit(xnetengineconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetengineconfig));
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
}

static void xrtNetListenConfigInit(xnetlistenconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetlistenconfig));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iFlags = XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	pCfg->iBacklog = 512;
	pCfg->iHighWater = 262144;
	pCfg->iLowWater = 65536;
	pCfg->iRecvLimit = 1048576;
	pCfg->pTlsConfig = NULL;
}

static void xrtNetConnectConfigInit(xnetconnectconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetconnectconfig));
	pCfg->sHost = NULL;
	pCfg->iPort = 0;
	pCfg->iFlags = XNET_CONNECT_F_NO_DELAY;
	pCfg->iConnectTimeoutMs = 5000;
	pCfg->iHighWater = 262144;
	pCfg->iLowWater = 65536;
	pCfg->iRecvLimit = 1048576;
	pCfg->pTlsConfig = NULL;
}

static void xrtNetDgramConfigInit(xnetdgramconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xnetdgramconfig));
	xrtNetAddrInitAny(&pCfg->tBindAddr, AF_INET, 0);
	pCfg->iFlags = XNET_DGRAM_F_REUSE_ADDR;
	pCfg->iRecvBatch = 64;
	pCfg->iSendQueueLimit = 262144;
}


#endif
