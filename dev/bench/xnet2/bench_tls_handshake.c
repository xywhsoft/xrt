#include "bench_common.h"
#include "../../../lib/xnet_stream.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile uint64_t iLastOpenNs;
} __bench_tls_hs_stats;

static int __benchTlsHsLatencyCmp(const void* pA, const void* pB)
{
	uint64_t a = *(const uint64_t*)pA;
	uint64_t b = *(const uint64_t*)pB;
	return (a > b) - (a < b);
}

static void __benchTlsHsOnOpen(ptr pOwner, xnetstream* pStream)
{
	__bench_tls_hs_stats* pStats = (__bench_tls_hs_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	pStats->iLastOpenNs = xbenchNowNs();
	xbenchAtomicInc(&pStats->iOpenCount);
}

static void __benchTlsHsOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	(void)pOwner;
	(void)pStream;
	if ( pChain ) {
		xrtNetChainConsume(pChain, xrtNetChainBytes(pChain));
	}
}

static void __benchTlsHsOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__bench_tls_hs_stats* pStats = (__bench_tls_hs_stats*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pStats ) return;
	xbenchAtomicInc(&pStats->iCloseCount);
}

static void __benchTlsHsOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__bench_tls_hs_stats* pStats = (__bench_tls_hs_stats*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pStats ) return;
	xbenchAtomicInc(&pStats->iErrorCount);
}

int main(int argc, char** argv)
{
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 200u);
	const char* sCertPath = argc > 2 ? argv[2] : "dev/xnet2_tls_test_cert.pem";
	const char* sKeyPath = argc > 3 ? argv[3] : "dev/xnet2_tls_test_key.pem";
	xnetengineconfig tEngineCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xnetstreamevents tEvents;
	xtlsconfig tServerTls;
	xtlsconfig tClientTls;
	__bench_tls_hs_stats tClientStats;
	__bench_tls_hs_stats tServerStats;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	uint64_t* arrHandshakeNs = NULL;
	uint64_t iHandshakeSumNs = 0u;
	uint64_t iLoopStartNs = 0u;
	uint64_t iLoopEndNs = 0u;
	double fP50 = 0.0;
	double fP95 = 0.0;
	double fP99 = 0.0;
	int iExitCode = 0;

	printf("xnet2 bench_tls_handshake\n");
	printf("iterations=%u cert=%s key=%s\n", (unsigned)iIterations, sCertPath, sKeyPath);

	if ( iIterations == 0u ) {
		printf("iterations must be > 0\n");
		return 1;
	}
	if ( !xbenchFileExists(sCertPath) || !xbenchFileExists(sKeyPath) ) {
		printf("tls fixture files missing\n");
		return 1;
	}
	if ( !xbenchNetInit() ) return 1;

	arrHandshakeNs = (uint64_t*)calloc(iIterations, sizeof(uint64_t));
	if ( !arrHandshakeNs ) {
		iExitCode = 1;
		goto cleanup;
	}

	memset(&tEvents, 0, sizeof(tEvents));
	memset(&tServerTls, 0, sizeof(tServerTls));
	memset(&tClientTls, 0, sizeof(tClientTls));
	tEvents.OnOpen = __benchTlsHsOnOpen;
	tEvents.OnRecv = __benchTlsHsOnRecv;
	tEvents.OnClose = __benchTlsHsOnClose;
	tEvents.OnError = __benchTlsHsOnError;
	tServerTls.sCertFile = sCertPath;
	tServerTls.sKeyFile = sKeyPath;
	tServerTls.bVerifyPeer = false;
	tClientTls.sHostName = "localhost";
	tClientTls.bVerifyPeer = false;

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
	tListenCfg.iFlags |= XNET_LISTEN_F_REUSE_ADDR | XNET_LISTEN_F_NO_DELAY;
	tListenCfg.pTlsConfig = &tServerTls;
	xrtNetConnectConfigInit(&tConnCfg);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iFlags |= XNET_CONNECT_F_NO_DELAY;
	tConnCfg.pTlsConfig = &tClientTls;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pEngine || xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		iExitCode = 1;
		goto cleanup;
	}

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &tEvents, NULL);
	if ( !pListener || xrtNetListenerStart(pListener) != XRT_NET_OK ) {
		iExitCode = 1;
		goto cleanup;
	}
	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;

	iLoopStartNs = xbenchNowNs();
	for ( uint32_t i = 0; i < iIterations; ++i ) {
		xnetstream* pClient = NULL;
		xnetstream* pAccepted = NULL;
		uint64_t iConnectStartNs;
		uint64_t iClientOpenNs;
		uint64_t iServerOpenNs;
		uint64_t iHandshakeNs;

		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tServerStats, 0, sizeof(tServerStats));

		pClient = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats);
		if ( !pClient ) {
			printf("create_failed_at=%u\n", (unsigned)i);
			iExitCode = 2;
			goto cleanup;
		}

		iConnectStartNs = xbenchNowNs();
		if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
			printf("connect_failed_at=%u\n", (unsigned)i);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted; ++iAcceptRetry ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tServerStats);
			if ( !pAccepted ) xbenchSleepMs(10u);
		}
		if ( !pAccepted ) {
			printf("accept_failed_at=%u\n", (unsigned)i);
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		if ( !xbenchWaitMin(&tClientStats.iOpenCount, 1, 3000u) ||
			!xbenchWaitMin(&tServerStats.iOpenCount, 1, 3000u) ) {
			printf("handshake_timeout_at=%u client_open=%ld server_open=%ld\n",
				(unsigned)i,
				xbenchAtomicLoad(&tClientStats.iOpenCount),
				xbenchAtomicLoad(&tServerStats.iOpenCount));
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		iClientOpenNs = xbenchAtomicLoadU64(&tClientStats.iLastOpenNs);
		iServerOpenNs = xbenchAtomicLoadU64(&tServerStats.iLastOpenNs);
		iHandshakeNs = ((iClientOpenNs > iServerOpenNs) ? iClientOpenNs : iServerOpenNs) - iConnectStartNs;
		arrHandshakeNs[i] = iHandshakeNs;
		iHandshakeSumNs += iHandshakeNs;

		xrtNetStreamClose(pClient, XNET_CLOSE_F_GRACEFUL);
		if ( !xbenchWaitMin(&tClientStats.iCloseCount, 1, 3000u) ||
			!xbenchWaitMin(&tServerStats.iCloseCount, 1, 3000u) ) {
			printf("close_timeout_at=%u client_close=%ld server_close=%ld\n",
				(unsigned)i,
				xbenchAtomicLoad(&tClientStats.iCloseCount),
				xbenchAtomicLoad(&tServerStats.iCloseCount));
			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		if ( xbenchAtomicLoad(&tClientStats.iErrorCount) != 0 ||
			xbenchAtomicLoad(&tServerStats.iErrorCount) != 0 ) {
			printf("error_path_at=%u client_errors=%ld server_errors=%ld\n",
				(unsigned)i,
				xbenchAtomicLoad(&tClientStats.iErrorCount),
				xbenchAtomicLoad(&tServerStats.iErrorCount));
			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		xrtNetStreamDestroy(pAccepted);
		xrtNetStreamDestroy(pClient);
		xbenchSleepMs(20u);
	}
	iLoopEndNs = xbenchNowNs();

	qsort(arrHandshakeNs, iIterations, sizeof(uint64_t), __benchTlsHsLatencyCmp);
	fP50 = (double)arrHandshakeNs[(iIterations - 1u) / 2u] / 1000.0;
	fP95 = (double)arrHandshakeNs[(uint32_t)((double)(iIterations - 1u) * 0.95)] / 1000.0;
	fP99 = (double)arrHandshakeNs[(uint32_t)((double)(iIterations - 1u) * 0.99)] / 1000.0;

	xbenchPrintMetricU64("handshake_sum_ns", iHandshakeSumNs);
	xbenchPrintMetricDouble("handshakes_per_sec_active", xbenchSafeRate(iIterations, iHandshakeSumNs));
	xbenchPrintMetricDouble("handshake_avg_us", (double)iHandshakeSumNs / (double)iIterations / 1000.0);
	xbenchPrintMetricDouble("handshake_p50_us", fP50);
	xbenchPrintMetricDouble("handshake_p95_us", fP95);
	xbenchPrintMetricDouble("handshake_p99_us", fP99);
	xbenchPrintMetricU64("loop_elapsed_ns", iLoopEndNs - iLoopStartNs);
	xbenchPrintMetricDouble("handshakes_per_sec_loop", xbenchSafeRate(iIterations, iLoopEndNs - iLoopStartNs));

cleanup:
	if ( pListener ) {
		xrtNetListenerStop(pListener);
		xrtNetListenerDestroy(pListener);
	}
	if ( pEngine ) {
		xbenchSleepMs(20u);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	free(arrHandshakeNs);
	xbenchNetUnit();
	return iExitCode;
}
