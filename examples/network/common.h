#ifndef XRT_NETWORK_EXAMPLE_COMMON_H
#define XRT_NETWORK_EXAMPLE_COMMON_H

#include <stdio.h>
#include <string.h>

typedef struct {
	char aRecv[256];
	volatile long iRecvCount;
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	bool bEchoBack;
	bool bSendOnOpen;
	char aSend[256];
} exnet_stream_ctx;

typedef struct {
	char aRecv[256];
	volatile long iRecvCount;
	volatile long iErrorCount;
	bool bEchoBack;
} exnet_dgram_ctx;



static bool exNetWaitMin(volatile long* pValue, long iMin, uint32 iTimeoutMs)
{
	double fDeadline = xrtTimer() + ((double)iTimeoutMs / 1000.0);

	while ( xrtTimer() < fDeadline ) {
		if ( *pValue >= iMin ) {
			return TRUE;
		}
		xrtSleep(5);
	}

	return *pValue >= iMin;
}



static void exNetOnOpen(ptr pOwner, xnetstream* pStream)
{
	exnet_stream_ctx* pCtx = (exnet_stream_ctx*)pOwner;

	if ( !pCtx ) return;
	pCtx->iOpenCount++;

	if ( pCtx->bSendOnOpen && pCtx->aSend[0] != '\0' ) {
		(void)xrtNetStreamSend(pStream, pCtx->aSend, strlen(pCtx->aSend));
	}
}



static void exNetOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	exnet_stream_ctx* pCtx = (exnet_stream_ctx*)pOwner;
	size_t iBytes;
	size_t iCopy;

	if ( !pCtx || !pChain ) return;

	iBytes = xrtNetChainBytes(pChain);
	iCopy = iBytes;
	if ( iCopy >= sizeof(pCtx->aRecv) ) {
		iCopy = sizeof(pCtx->aRecv) - 1u;
	}

	if ( iCopy > 0 ) {
		(void)xrtNetChainPeek(pChain, pCtx->aRecv, iCopy);
		pCtx->aRecv[iCopy] = '\0';
	}

	if ( pCtx->bEchoBack && (iCopy > 0) ) {
		(void)xrtNetStreamSend(pStream, pCtx->aRecv, iCopy);
	}

	xrtNetChainConsume(pChain, iBytes);
	pCtx->iRecvCount++;
}



static void exNetOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	exnet_stream_ctx* pCtx = (exnet_stream_ctx*)pOwner;
	(void)pStream;
	(void)iReason;

	if ( pCtx ) {
		pCtx->iCloseCount++;
	}
}



static void exNetOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	exnet_stream_ctx* pCtx = (exnet_stream_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;

	if ( pCtx ) {
		pCtx->iErrorCount++;
	}
}



static void exNetOnDgramRecv(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain)
{
	exnet_dgram_ctx* pCtx = (exnet_dgram_ctx*)pOwner;
	size_t iBytes;
	size_t iCopy;

	if ( !pCtx || !pChain ) return;

	iBytes = xrtNetChainBytes(pChain);
	iCopy = iBytes;
	if ( iCopy >= sizeof(pCtx->aRecv) ) {
		iCopy = sizeof(pCtx->aRecv) - 1u;
	}

	if ( iCopy > 0 ) {
		(void)xrtNetChainPeek(pChain, pCtx->aRecv, iCopy);
		pCtx->aRecv[iCopy] = '\0';
		if ( pCtx->bEchoBack ) {
			(void)xrtNetDgramSendTo(pSock, pFrom, pCtx->aRecv, iCopy);
		}
	}

	xrtNetChainConsume(pChain, iBytes);
	pCtx->iRecvCount++;
}



static void exNetOnDgramError(ptr pOwner, xdgramsock* pSock, int iSysErr)
{
	exnet_dgram_ctx* pCtx = (exnet_dgram_ctx*)pOwner;
	(void)pSock;
	(void)iSysErr;

	if ( pCtx ) {
		pCtx->iErrorCount++;
	}
}



static const xnetstreamevents gExNetStreamEvents = {
	exNetOnOpen,
	exNetOnRecv,
	NULL,
	exNetOnClose,
	exNetOnError,
	NULL,
	NULL
};



static const xnetdgramevents gExNetDgramEvents = {
	exNetOnDgramRecv,
	exNetOnDgramError
};

#endif
