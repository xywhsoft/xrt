#ifndef XRT_XNET_TLS_H
#define XRT_XNET_TLS_H

#include <stdlib.h>
#include <string.h>

#include "xnet_base.h"


/*
    XRT mainline builtin TLS adapter.

    This header bridges xnet stream transport with the in-tree TLS engine
    without introducing any external dependency.

    The TLS core no longer depends on the archived v1 network ABI. The adapter
    only coordinates owned sockets plus plaintext/ciphertext flow for the
    modern xnet stream layer.
*/

#ifndef XNET_ALLOC
	#define XNET_ALLOC malloc
#endif

#ifndef XNET_FREE
	#define XNET_FREE free
#endif


/* ============================== TLS core bridge ============================== */

#if !defined(XXRTL_CORE)
	typedef struct xrt_tls_context xtlsctx;
	typedef struct xrt_tls_resume xtlsresume;
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
		uint16 iMaxVersion;
		const xtlsresume* pResume;
	};
#endif

extern xtlsctx* xrtTlsCreate(const xtlsconfig* pConfig, bool bIsServer);
extern void xrtTlsDestroy(xtlsctx* pCtx);
extern xnet_result xrtTlsHandshake(xtlsctx* pCtx, xsocket hSocket);
extern xnet_result xrtTlsDrive(xtlsctx* pCtx);
extern xnet_result xrtTlsRead(xtlsctx* pCtx, char* pBuf, size_t iLen, size_t* pRead);
extern xnet_result xrtTlsWrite(xtlsctx* pCtx, const char* pData, size_t iLen, size_t* pWritten);
extern xnet_result xrtTlsClose(xtlsctx* pCtx);
extern bool xrtTlsIsReady(xtlsctx* pCtx);
extern xnet_result xrtTlsFeed(xtlsctx* pCtx, const char* pData, size_t iLen);
extern size_t xrtTlsPendingSend(xtlsctx* pCtx);
extern size_t xrtTlsPendingRecv(xtlsctx* pCtx);
extern xnet_result xrtTlsPeekSend(xtlsctx* pCtx, char* pBuf, size_t iLen, size_t* pRead);
extern void xrtTlsConsumeSend(xtlsctx* pCtx, size_t iLen);
extern xtlsresume* xrtTlsExportResume(xtlsctx* pCtx);
extern void xrtTlsResumeDestroy(xtlsresume* pResume);
extern bool xrtTlsWasResumed(xtlsctx* pCtx);
#if !defined(XXRTL_CORE)
	extern xrtGlobalData* xrtInit(void);
#endif


/* ============================== Session wrapper ============================== */

struct xrt_tls_session {
	xtlsctx* pCtx;
	bool bIsServer;
};

XXAPI xtlssession* xrtNetTlsSessionCreate(const xtlsconfig* pCfg, bool bIsServer)
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

XXAPI void xrtNetTlsSessionDestroy(xtlssession* pSession)
{
	if ( !pSession ) return;
	if ( pSession->pCtx ) {
		xrtTlsDestroy(pSession->pCtx);
		pSession->pCtx = NULL;
	}
	XNET_FREE(pSession);
}

XXAPI bool xrtNetTlsSessionIsReady(const xtlssession* pSession)
{
	return pSession && pSession->pCtx ? xrtTlsIsReady(pSession->pCtx) : false;
}

XXAPI xnet_result xrtNetTlsSessionDriveHandshake(xtlssession* pSession)
{
	if ( !pSession || !pSession->pCtx ) return XRT_NET_ERROR;
	return xrtTlsDrive(pSession->pCtx);
}

XXAPI xnet_result xrtNetTlsSessionFeedCipher(xtlssession* pSession, const void* pData, size_t iLen)
{
	if ( !pSession || !pSession->pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsFeed(pSession->pCtx, (const char*)pData, iLen);
}

XXAPI size_t xrtNetTlsSessionPendingCipher(const xtlssession* pSession)
{
	return (pSession && pSession->pCtx) ? xrtTlsPendingSend(pSession->pCtx) : 0;
}

XXAPI size_t xrtNetTlsSessionPendingRecv(const xtlssession* pSession)
{
	return (pSession && pSession->pCtx) ? xrtTlsPendingRecv(pSession->pCtx) : 0;
}

XXAPI xnet_result xrtNetTlsSessionPeekCipher(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	if ( !pSession || !pSession->pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsPeekSend(pSession->pCtx, (char*)pBuf, iLen, pRead);
}

XXAPI void xrtNetTlsSessionConsumeCipher(xtlssession* pSession, size_t iLen)
{
	if ( !pSession || !pSession->pCtx || iLen == 0 ) return;
	xrtTlsConsumeSend(pSession->pCtx, iLen);
}

XXAPI xnet_result xrtNetTlsSessionWritePlain(xtlssession* pSession, const void* pData, size_t iLen, size_t* pWritten)
{
	if ( !pSession || !pSession->pCtx || !pData || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsWrite(pSession->pCtx, (const char*)pData, iLen, pWritten);
}

XXAPI xnet_result xrtNetTlsSessionReadPlain(xtlssession* pSession, void* pBuf, size_t iLen, size_t* pRead)
{
	if ( !pSession || !pSession->pCtx || !pBuf || iLen == 0 ) return XRT_NET_ERROR;
	return xrtTlsRead(pSession->pCtx, (char*)pBuf, iLen, pRead);
}

XXAPI xnet_result xrtNetTlsSessionQueueClose(xtlssession* pSession)
{
	if ( !pSession || !pSession->pCtx ) return XRT_NET_ERROR;
	return xrtTlsClose(pSession->pCtx);
}


#endif
