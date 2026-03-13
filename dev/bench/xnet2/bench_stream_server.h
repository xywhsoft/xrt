#ifndef XNET2_BENCH_STREAM_SERVER_H
#define XNET2_BENCH_STREAM_SERVER_H

#include "bench_common.h"
#include "../../../lib/xnet_stream.h"

typedef struct xbench_stream_server xbenchstreamserver;
typedef struct xbench_stream_conn xbenchstreamconn;

typedef struct {
	xnetlistenconfig tListenCfg;
	uint32_t iAcceptPollMs;
} xbenchstreamserverconfig;

typedef struct {
	bool (*OnAccept)(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn);
	void (*OnOpen)(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn);
	void (*OnRecv)(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnetchain* pChain);
	void (*OnClose)(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, int iSysErr);
} xbenchstreamserverevents;

struct xbench_stream_conn {
	struct xbench_stream_conn* pNext;
	volatile long iCleanupPosted;
	xbenchstreamserver* pServer;
	xnetstream* pStream;
	ptr pUserData;
};

struct xbench_stream_server {
	xnetengine* pEngine;
	xnetlistener* pListener;
	xbenchstreamserverconfig tConfig;
	xbenchstreamserverevents tEvents;
	ptr pUserData;
	volatile long iConnLock;
	volatile long bRunning;
	xbenchstreamconn* pConnHead;
#if defined(_WIN32) || defined(_WIN64)
	HANDLE hAcceptThread;
#else
	pthread_t hAcceptThread;
	bool bAcceptThreadStarted;
#endif
};

static bool xbenchStreamConnSendDirect(xbenchstreamconn* pConn, const void* pData, size_t iLen)
{
	xnetstream* pStream;
	if ( !pConn || !pConn->pStream || !pData || iLen == 0u ) return false;
	pStream = pConn->pStream;
	if ( pStream->pTls ) {
		if ( !__xnetStreamAppendTlsPlainCopy(pStream, pData, iLen) ) return false;
	} else {
		if ( !__xnetStreamAppendSendCopy(pStream, pData, iLen) ) return false;
		__xnetStreamKickWrite(pStream);
	}
	return true;
}

static void xbenchStreamServerConfigInit(xbenchstreamserverconfig* pCfg)
{
	if ( !pCfg ) return;
	memset(pCfg, 0, sizeof(xbenchstreamserverconfig));
	xrtNetListenConfigInit(&pCfg->tListenCfg);
	pCfg->iAcceptPollMs = 5u;
}

static void __xbenchStreamServerAddConn(xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	if ( !pServer || !pConn ) return;
	while ( __sync_lock_test_and_set(&pServer->iConnLock, 1) != 0 ) xbenchSleepMs(1u);
	pConn->pNext = pServer->pConnHead;
	pServer->pConnHead = pConn;
	__sync_lock_release(&pServer->iConnLock);
}

static void __xbenchStreamServerRemoveConn(xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	xbenchstreamconn** ppNode;
	if ( !pServer || !pConn ) return;
	while ( __sync_lock_test_and_set(&pServer->iConnLock, 1) != 0 ) xbenchSleepMs(1u);
	ppNode = &pServer->pConnHead;
	while ( *ppNode ) {
		if ( *ppNode == pConn ) {
			*ppNode = pConn->pNext;
			break;
		}
		ppNode = &(*ppNode)->pNext;
	}
	__sync_lock_release(&pServer->iConnLock);
}

static xbenchstreamconn* __xbenchStreamServerDetachAll(xbenchstreamserver* pServer)
{
	xbenchstreamconn* pHead;
	if ( !pServer ) return NULL;
	while ( __sync_lock_test_and_set(&pServer->iConnLock, 1) != 0 ) xbenchSleepMs(1u);
	pHead = pServer->pConnHead;
	pServer->pConnHead = NULL;
	__sync_lock_release(&pServer->iConnLock);
	return pHead;
}

static void __xbenchStreamConnCleanupTask(xnetworker* pWorker, ptr pArg)
{
	xbenchstreamconn* pConn = (xbenchstreamconn*)pArg;
	(void)pWorker;
	if ( !pConn ) return;
	if ( pConn->pServer ) {
		__xbenchStreamServerRemoveConn(pConn->pServer, pConn);
		pConn->pServer = NULL;
	}
	if ( pConn->pStream ) {
		xrtNetStreamDestroy(pConn->pStream);
		pConn->pStream = NULL;
	}
	XNET_FREE(pConn);
}

static void __xbenchStreamConnPostCleanup(xbenchstreamconn* pConn)
{
	if ( !pConn ) return;
	if ( __sync_val_compare_and_swap(&pConn->iCleanupPosted, 0, 1) != 0 ) return;
	if ( pConn->pServer && pConn->pServer->pEngine && pConn->pStream && pConn->pStream->pWorker ) {
		if ( xrtNetEnginePost(pConn->pServer->pEngine, pConn->pStream->pWorker->iId, __xbenchStreamConnCleanupTask, pConn) == XRT_NET_OK ) {
			return;
		}
	}
	__xbenchStreamConnCleanupTask(NULL, pConn);
}

static bool __xbenchStreamListenerOnAccept(ptr pOwner, xnetlistener* pListener, xnetstream* pStream)
{
	xbenchstreamserver* pServer = (xbenchstreamserver*)pOwner;
	xbenchstreamconn* pConn;
	(void)pListener;
	if ( !pServer || !pStream ) return false;
	pConn = (xbenchstreamconn*)xrtNetStreamGetUserData(pStream);
	if ( !pConn ) return false;
	pConn->pServer = pServer;
	pConn->pStream = pStream;
	__xbenchStreamServerAddConn(pServer, pConn);
	if ( pServer->tEvents.OnAccept ) {
		if ( !pServer->tEvents.OnAccept(pServer->pUserData, pServer, pConn) ) {
			xrtNetStreamClose(pStream, XNET_CLOSE_F_ABORT);
			return false;
		}
	}
	return true;
}

static void __xbenchStreamOnOpen(ptr pOwner, xnetstream* pStream)
{
	xbenchstreamconn* pConn = (xbenchstreamconn*)pOwner;
	xbenchstreamserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnOpen ) {
		pServer->tEvents.OnOpen(pServer->pUserData, pServer, pConn);
	}
}

static void __xbenchStreamOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	xbenchstreamconn* pConn = (xbenchstreamconn*)pOwner;
	xbenchstreamserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnRecv ) {
		pServer->tEvents.OnRecv(pServer->pUserData, pServer, pConn, pChain);
	}
}

static void __xbenchStreamOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	xbenchstreamconn* pConn = (xbenchstreamconn*)pOwner;
	xbenchstreamserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnClose ) {
		pServer->tEvents.OnClose(pServer->pUserData, pServer, pConn, iReason);
	}
	__xbenchStreamConnPostCleanup(pConn);
}

static void __xbenchStreamOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	xbenchstreamconn* pConn = (xbenchstreamconn*)pOwner;
	xbenchstreamserver* pServer = pConn ? pConn->pServer : NULL;
	(void)pStream;
	if ( pServer && pServer->tEvents.OnError ) {
		pServer->tEvents.OnError(pServer->pUserData, pServer, pConn, iSysErr);
	}
}

static const xnetlistenerevents* __xbenchStreamListenerEvents(void)
{
	static const xnetlistenerevents tEvents = {
		__xbenchStreamListenerOnAccept,
		NULL
	};
	return &tEvents;
}

static const xnetstreamevents* __xbenchStreamEvents(void)
{
	static const xnetstreamevents tEvents = {
		__xbenchStreamOnOpen,
		__xbenchStreamOnRecv,
		NULL,
		__xbenchStreamOnClose,
		__xbenchStreamOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

#if defined(_WIN32) || defined(_WIN64)
static DWORD WINAPI __xbenchStreamAcceptThread(LPVOID pArg)
#else
static void* __xbenchStreamAcceptThread(void* pArg)
#endif
{
	xbenchstreamserver* pServer = (xbenchstreamserver*)pArg;
	while ( pServer && xbenchAtomicLoad(&pServer->bRunning) != 0 ) {
		xbenchstreamconn* pConn = NULL;
		if ( pServer->pListener && pServer->pListener->bRunning ) {
			pConn = (xbenchstreamconn*)XNET_ALLOC(sizeof(xbenchstreamconn));
			if ( pConn ) {
				memset(pConn, 0, sizeof(xbenchstreamconn));
				pConn->pServer = pServer;
				if ( !__xnetListenerTryAcceptOne(pServer->pListener, pConn) ) {
					XNET_FREE(pConn);
				}
			}
		}
		xbenchSleepMs(pServer && pServer->tConfig.iAcceptPollMs > 0 ? pServer->tConfig.iAcceptPollMs : 5u);
	}
	#if !defined(_WIN32) && !defined(_WIN64)
		return NULL;
	#endif
}

static xbenchstreamserver* xbenchStreamServerCreate(xnetengine* pEngine, const xbenchstreamserverconfig* pCfg, const xbenchstreamserverevents* pEvents, ptr pUserData)
{
	xbenchstreamserver* pServer;
	if ( !pEngine ) return NULL;
	pServer = (xbenchstreamserver*)XNET_ALLOC(sizeof(xbenchstreamserver));
	if ( !pServer ) return NULL;
	memset(pServer, 0, sizeof(xbenchstreamserver));
	pServer->pEngine = pEngine;
	if ( pCfg ) pServer->tConfig = *pCfg;
	else xbenchStreamServerConfigInit(&pServer->tConfig);
	if ( pEvents ) pServer->tEvents = *pEvents;
	pServer->pUserData = pUserData;
	return pServer;
}

static uint16_t xbenchStreamServerBoundPort(const xbenchstreamserver* pServer)
{
	return (pServer && pServer->pListener) ? pServer->pListener->tConfig.tBindAddr.iPort : 0u;
}

static xnet_result xbenchStreamServerStart(xbenchstreamserver* pServer)
{
	if ( !pServer || !pServer->pEngine ) return XRT_NET_ERROR;
	pServer->pListener = xrtNetListenerCreate(pServer->pEngine, &pServer->tConfig.tListenCfg, __xbenchStreamListenerEvents(), __xbenchStreamEvents(), pServer);
	if ( !pServer->pListener ) return XRT_NET_ERROR;
	if ( xrtNetListenerStart(pServer->pListener) != XRT_NET_OK ) {
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
		return XRT_NET_ERROR;
	}
	pServer->bRunning = 1;
	#if defined(_WIN32) || defined(_WIN64)
		pServer->hAcceptThread = CreateThread(NULL, 0, __xbenchStreamAcceptThread, pServer, 0, NULL);
		if ( !pServer->hAcceptThread ) {
			pServer->bRunning = 0;
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
	#else
		if ( pthread_create(&pServer->hAcceptThread, NULL, __xbenchStreamAcceptThread, pServer) != 0 ) {
			pServer->bRunning = 0;
			xrtNetListenerStop(pServer->pListener);
			xrtNetListenerDestroy(pServer->pListener);
			pServer->pListener = NULL;
			return XRT_NET_ERROR;
		}
		pServer->bAcceptThreadStarted = true;
	#endif
	return XRT_NET_OK;
}

static void xbenchStreamServerStop(xbenchstreamserver* pServer)
{
	xbenchstreamconn* pConn;
	if ( !pServer ) return;
	if ( pServer->bRunning ) {
		pServer->bRunning = 0;
		#if defined(_WIN32) || defined(_WIN64)
			if ( pServer->hAcceptThread ) {
				WaitForSingleObject(pServer->hAcceptThread, INFINITE);
				CloseHandle(pServer->hAcceptThread);
				pServer->hAcceptThread = NULL;
			}
		#else
			if ( pServer->bAcceptThreadStarted ) {
				pthread_join(pServer->hAcceptThread, NULL);
				pServer->bAcceptThreadStarted = false;
			}
		#endif
	}
	if ( pServer->pListener ) {
		xrtNetListenerStop(pServer->pListener);
		xrtNetListenerDestroy(pServer->pListener);
		pServer->pListener = NULL;
	}
	pConn = __xbenchStreamServerDetachAll(pServer);
	while ( pConn ) {
		xbenchstreamconn* pNext = pConn->pNext;
		if ( pConn->pStream ) {
			xrtNetStreamDestroy(pConn->pStream);
			pConn->pStream = NULL;
		}
		XNET_FREE(pConn);
		pConn = pNext;
	}
}

static void xbenchStreamServerDestroy(xbenchstreamserver* pServer)
{
	if ( !pServer ) return;
	xbenchStreamServerStop(pServer);
	XNET_FREE(pServer);
}

#endif
