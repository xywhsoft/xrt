#include "bench_stream_server.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
} __bench_idle_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
} __bench_idle_client_ctx;

static void __benchIdleServerOnOpen(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn)
{
	__bench_idle_server_ctx* pCtx = (__bench_idle_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchIdleServerOnClose(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, xnet_result iReason)
{
	__bench_idle_server_ctx* pCtx = (__bench_idle_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchIdleServerOnError(ptr pOwner, xbenchstreamserver* pServer, xbenchstreamconn* pConn, int iSysErr)
{
	__bench_idle_server_ctx* pCtx = (__bench_idle_server_ctx*)pOwner;
	(void)pServer;
	(void)pConn;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchIdleClientOnOpen(ptr pOwner, xnetstream* pStream)
{
	__bench_idle_client_ctx* pCtx = (__bench_idle_client_ctx*)pOwner;
	(void)pStream;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchIdleClientOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__bench_idle_client_ctx* pCtx = (__bench_idle_client_ctx*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchIdleClientOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__bench_idle_client_ctx* pCtx = (__bench_idle_client_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static const xnetstreamevents* __benchIdleClientEvents(void)
{
	static const xnetstreamevents tEvents = {
		__benchIdleClientOnOpen,
		NULL,
		NULL,
		__benchIdleClientOnClose,
		__benchIdleClientOnError,
		NULL,
		NULL
	};
	return &tEvents;
}

int main(int argc, char** argv)
{
	uint32_t iConnCount = xbenchArgU32(argc, argv, 1, 512u);
	uint32_t iHoldMs = xbenchArgU32(argc, argv, 2, 1000u);
	xnetengineconfig tEngineCfg;
	xbenchstreamserverconfig tSrvCfg;
	xbenchstreamserverevents tSrvEvents;
	__bench_idle_server_ctx tSrvCtx;
	__bench_idle_client_ctx tCliCtx;
	xbenchstreamserver* pServer = NULL;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xnetstream** arrClients = NULL;
	xnetconnectconfig tConnCfg;
	xbenchtimer tConnectTimer;
	uint32_t iCreated = 0u;
	uint64_t iElapsedNs;

	printf("xnet2 bench_idle_conn\n");
	printf("connections=%u hold_ms=%u\n", (unsigned)iConnCount, (unsigned)iHoldMs);

	if ( !xbenchNetInit() ) return 1;
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	tSrvEvents.OnOpen = __benchIdleServerOnOpen;
	tSrvEvents.OnClose = __benchIdleServerOnClose;
	tSrvEvents.OnError = __benchIdleServerOnError;

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServerEngine || !pClientEngine ) goto cleanup;
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK ) goto cleanup;
	if ( xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) goto cleanup;

	xbenchStreamServerConfigInit(&tSrvCfg);
	(void)xrtNetAddrParse(&tSrvCfg.tListenCfg.tBindAddr, "127.0.0.1", 0);
	tSrvCfg.tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	pServer = xbenchStreamServerCreate(pServerEngine, &tSrvCfg, &tSrvEvents, &tSrvCtx);
	if ( !pServer ) goto cleanup;
	if ( xbenchStreamServerStart(pServer) != XRT_NET_OK ) goto cleanup;

	arrClients = (xnetstream**)calloc(iConnCount, sizeof(xnetstream*));
	if ( !arrClients ) goto cleanup;

	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = xbenchStreamServerBoundPort(pServer);
	tConnCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;

	xbenchTimerStart(&tConnectTimer);
	for ( uint32_t i = 0; i < iConnCount; ++i ) {
		xnetstream* pStream = xrtNetStreamCreate(pClientEngine, __benchIdleClientEvents(), &tCliCtx);
		if ( !pStream ) break;
		if ( xrtNetStreamConnect(pStream, &tConnCfg) != XRT_NET_OK ) {
			xrtNetStreamDestroy(pStream);
			break;
		}
		arrClients[i] = pStream;
		iCreated++;
	}
	(void)xbenchWaitMin(&tCliCtx.iOpenCount, (long)iCreated, 5000u);
	(void)xbenchWaitMin(&tSrvCtx.iOpenCount, (long)iCreated, 5000u);
	xbenchTimerStop(&tConnectTimer);
	iElapsedNs = xbenchTimerElapsedNs(&tConnectTimer);

	printf("created=%u\n", (unsigned)iCreated);
	xbenchPrintMetricU64("connect_elapsed_ns", iElapsedNs);
	xbenchPrintMetricDouble("connect_rate_per_sec", xbenchSafeRate(iCreated, iElapsedNs));
	printf("client_open=%ld server_open=%ld\n", xbenchAtomicLoad(&tCliCtx.iOpenCount), xbenchAtomicLoad(&tSrvCtx.iOpenCount));
	printf("client_errors=%ld server_errors=%ld\n", xbenchAtomicLoad(&tCliCtx.iErrorCount), xbenchAtomicLoad(&tSrvCtx.iErrorCount));

	xbenchSleepMs(iHoldMs);

cleanup:
	if ( arrClients ) {
		for ( uint32_t i = 0; i < iConnCount; ++i ) {
			if ( arrClients[i] ) {
				xrtNetStreamClose(arrClients[i], XNET_CLOSE_F_ABORT);
				xrtNetStreamDestroy(arrClients[i]);
			}
		}
		free(arrClients);
	}
	if ( pServer ) xbenchStreamServerDestroy(pServer);
	if ( pClientEngine ) {
		xrtNetEngineStop(pClientEngine);
		xrtNetEngineDestroy(pClientEngine);
	}
	if ( pServerEngine ) {
		xrtNetEngineStop(pServerEngine);
		xrtNetEngineDestroy(pServerEngine);
	}
	xbenchNetUnit();
	return 0;
}
