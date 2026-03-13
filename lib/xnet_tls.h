#ifndef XRT_XNET_TLS_H
#define XRT_XNET_TLS_H

#include <stdlib.h>
#include <string.h>

#include "xnet_base.h"


/*
    XNet V2 - Builtin TLS Adapter

    This header bridges xnet-v2 stream transport with the existing in-tree TLS
    engine without introducing any external dependency.

    Phase-1 note:
      - handshake still reuses the legacy state machine against the owned socket
      - plaintext and ciphertext data paths are routed through xnet-v2 helpers
*/

#ifndef XNET_ALLOC
	#define XNET_ALLOC malloc
#endif

#ifndef XNET_FREE
	#define XNET_FREE free
#endif


/* ============================== Legacy TLS ABI bridge ============================== */

#if !defined(XXRTL_CORE)
	typedef struct xrt_tls_context xtlsctx;
	typedef struct __xnet_tls_global xrtGlobalData;

	struct xrt_tls_config {
		const char* sCertFile;
		const char* sKeyFile;
		const char* sCaFile;
		const char* sHostName;
		bool bVerifyPeer;
		void (*OnSNI)(xtlsctx* pCtx, const char* sHostName, ptr pUserData);
		ptr pSNIUserData;
		bool bAllowTLS12Ed25519;
	};
#endif

#if defined(XXRTL_CORE)
	typedef xnetconn __xnet_tls_legacy_conn;
#else
	typedef struct {
		uint32 iAddr;
		uint16 iPort;
		char sAddr[16];
	} __xnet_tls_legacy_addr;

	typedef struct {
		int iId;
		xsocket hSocket;
		__xnet_tls_legacy_addr tLocalAddr;
		__xnet_tls_legacy_addr tRemoteAddr;
		int iType;
		ptr pUserData;
		ptr pTlsCtx;
		bool bTlsEnabled;
	} __xnet_tls_legacy_conn;
#endif

extern xtlsctx* xrtTlsCreate(const xtlsconfig* pConfig, bool bIsServer);
extern void xrtTlsDestroy(xtlsctx* pCtx);
#if defined(XXRTL_CORE)
	extern xnet_result xrtTlsHandshake(xtlsctx* pCtx, xnetconn* pConn);
#else
	extern xnet_result xrtTlsHandshake(xtlsctx* pCtx, __xnet_tls_legacy_conn* pConn);
#endif
extern xnet_result xrtTlsRead(xtlsctx* pCtx, char* pBuf, size_t iLen, size_t* pRead);
extern xnet_result xrtTlsWrite(xtlsctx* pCtx, const char* pData, size_t iLen, size_t* pWritten);
extern xnet_result xrtTlsClose(xtlsctx* pCtx);
extern bool xrtTlsIsReady(xtlsctx* pCtx);
extern xnet_result xrtTlsFeed(xtlsctx* pCtx, const char* pData, size_t iLen);
extern size_t xrtTlsPendingSend(xtlsctx* pCtx);
extern size_t xrtTlsPendingRecv(xtlsctx* pCtx);
extern xnet_result xrtTlsPeekSend(xtlsctx* pCtx, char* pBuf, size_t iLen, size_t* pRead);
extern void xrtTlsConsumeSend(xtlsctx* pCtx, size_t iLen);
#if !defined(XXRTL_CORE)
	extern xrtGlobalData* xrtInit(void);
#endif


/* ============================== Session wrapper ============================== */

struct xrt_tls_session {
	xtlsctx* pCtx;
	bool bIsServer;
};

static xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer)
{
	xtlssession* pSession = (xtlssession*)XNET_ALLOC(sizeof(xtlssession));
	(void)xrtInit();
	if ( !pSession ) return NULL;
	memset(pSession, 0, sizeof(xtlssession));
	pSession->pCtx = xrtTlsCreate(pCfg, bIsServer);
	if ( !pSession->pCtx ) {
		XNET_FREE(pSession);
		return NULL;
	}
	pSession->bIsServer = bIsServer;
	return pSession;
}

static void xrtNetTlsSessionDestroy(xtlssession* pSession)
{
	if ( !pSession ) return;
	if ( pSession->pCtx ) {
		xrtTlsDestroy(pSession->pCtx);
		pSession->pCtx = NULL;
	}
	XNET_FREE(pSession);
}

static bool xrtNetTlsSessionIsReady(const xtlssession* pSession)
{
	return pSession && pSession->pCtx ? xrtTlsIsReady(pSession->pCtx) : false;
}

static xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession, xsocket hSocket)
{
	__xnet_tls_legacy_conn tConn;
	if ( !pSession || !pSession->pCtx || hSocket == XNET_SOCKET_INVALID ) return XRT_NET_ERROR;
	memset(&tConn, 0, sizeof(tConn));
	tConn.hSocket = hSocket;
	tConn.iType = 0;
	tConn.pTlsCtx = pSession->pCtx;
	tConn.bTlsEnabled = true;
	return xrtTlsHandshake(pSession->pCtx, &tConn);
}

static xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen)
{
	if ( !pSession || !pSession->pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsFeed(pSession->pCtx, (const char*)pData, iLen);
}

static size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession)
{
	return (pSession && pSession->pCtx) ? xrtTlsPendingSend(pSession->pCtx) : 0;
}

static size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession)
{
	return (pSession && pSession->pCtx) ? xrtTlsPendingRecv(pSession->pCtx) : 0;
}

static xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	if ( !pSession || !pSession->pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsPeekSend(pSession->pCtx, (char*)pBuf, iLen, pRead);
}

static void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen)
{
	if ( !pSession || !pSession->pCtx || iLen == 0 ) return;
	xrtTlsConsumeSend(pSession->pCtx, iLen);
}

static xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten)
{
	if ( !pSession || !pSession->pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsWrite(pSession->pCtx, (const char*)pData, iLen, pWritten);
}

static xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	if ( !pSession || !pSession->pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsRead(pSession->pCtx, (char*)pBuf, iLen, pRead);
}

static xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession)
{
	if ( !pSession || !pSession->pCtx ) return XRT_NET_ERROR;
	return xrtTlsClose(pSession->pCtx);
}


#endif
