#include "bench_common.h"
#include "../../../lib/xnet_stream.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iRecvCount;
	volatile long iResumedCount;
	volatile uint64_t iLastOpenNs;
	char aLastRecv[128];
	bool bEchoBack;
} __bench_tls_resume_stats;

static int __benchTlsResumeLatencyCmp(const void* pA, const void* pB)
{
	uint64_t a = *(const uint64_t*)pA;
	uint64_t b = *(const uint64_t*)pB;
	return (a > b) - (a < b);
}

static void __benchTlsResumeOnOpen(ptr pOwner, xnetstream* pStream)
{
	__bench_tls_resume_stats* pStats = (__bench_tls_resume_stats*)pOwner;
	if ( !pStats ) return;
	(void)xbenchAtomicSetOnceU64(&pStats->iLastOpenNs, xbenchNowNs());
	if ( pStream && pStream->pTls && xrtNetTlsSessionWasResumed(pStream->pTls) ) {
		xbenchAtomicInc(&pStats->iResumedCount);
	}
	xbenchAtomicInc(&pStats->iOpenCount);
}

static void __benchTlsResumeOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__bench_tls_resume_stats* pStats = (__bench_tls_resume_stats*)pOwner;
	size_t iBytes;
	size_t iCopy;
	if ( !pStats || !pChain ) return;
	iBytes = xrtNetChainBytes(pChain);
	memset(pStats->aLastRecv, 0, sizeof(pStats->aLastRecv));
	if ( iBytes > 0 ) {
		iCopy = iBytes;
		if ( iCopy > sizeof(pStats->aLastRecv) - 1u ) iCopy = sizeof(pStats->aLastRecv) - 1u;
		(void)xrtNetChainPeek(pChain, pStats->aLastRecv, iCopy);
		if ( pStats->bEchoBack && pStream ) {
			(void)xrtNetStreamSend(pStream, pStats->aLastRecv, iCopy);
		}
		xrtNetChainConsume(pChain, iBytes);
	}
	xbenchAtomicInc(&pStats->iRecvCount);
}

static void __benchTlsResumeOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__bench_tls_resume_stats* pStats = (__bench_tls_resume_stats*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pStats ) return;
	xbenchAtomicInc(&pStats->iCloseCount);
}

static void __benchTlsResumeOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__bench_tls_resume_stats* pStats = (__bench_tls_resume_stats*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pStats ) return;
	xbenchAtomicInc(&pStats->iErrorCount);
}

int main(int argc, char** argv)
{
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 64u);
	const char* sCertPath = argc > 2 ? argv[2] : "dev/xnet2_tls_test_cert.pem";
	const char* sKeyPath = argc > 3 ? argv[3] : "dev/xnet2_tls_test_key.pem";
	xnetengineconfig tEngineCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xnetstreamevents tEvents;
	xtlsconfig tServerTls;
	xtlsconfig tClientTls;
	__bench_tls_resume_stats tWarmClientStats;
	__bench_tls_resume_stats tWarmServerStats;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xtlsresume* pResume = NULL;
	uint64_t* arrResumeNs = NULL;
	uint64_t iWarmHandshakeNs = 0u;
	uint64_t iResumeSumNs = 0u;
	uint64_t iResumeLoopStartNs = 0u;
	uint64_t iResumeLoopEndNs = 0u;
	uint32_t iClientResumedOkCount = 0u;
	uint32_t iServerResumedOkCount = 0u;
	uint32_t iPayloadOkCount = 0u;
	double fP50 = 0.0;
	double fP95 = 0.0;
	double fP99 = 0.0;
	int iExitCode = 0;

	printf("xnet2 bench_tls_resume\n");
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

	arrResumeNs = (uint64_t*)calloc(iIterations, sizeof(uint64_t));
	if ( !arrResumeNs ) {
		iExitCode = 1;
		goto cleanup;
	}

	memset(&tEvents, 0, sizeof(tEvents));
	memset(&tServerTls, 0, sizeof(tServerTls));
	memset(&tClientTls, 0, sizeof(tClientTls));
	memset(&tWarmClientStats, 0, sizeof(tWarmClientStats));
	memset(&tWarmServerStats, 0, sizeof(tWarmServerStats));
	tEvents.OnOpen = __benchTlsResumeOnOpen;
	tEvents.OnRecv = __benchTlsResumeOnRecv;
	tEvents.OnClose = __benchTlsResumeOnClose;
	tEvents.OnError = __benchTlsResumeOnError;
	tWarmServerStats.bEchoBack = true;
	tServerTls.sCertFile = sCertPath;
	tServerTls.sKeyFile = sKeyPath;
	tServerTls.bVerifyPeer = false;
	tServerTls.iMaxVersion = 0x0303u;
	tClientTls.sHostName = "localhost";
	tClientTls.bVerifyPeer = false;
	tClientTls.iMaxVersion = 0x0303u;

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

	{
		xnetstream* pWarmClient = NULL;
		xnetstream* pWarmAccepted = NULL;
		uint64_t iConnectStartNs;
		uint64_t iClientOpenNs;
		uint64_t iServerOpenNs;

		pWarmClient = xrtNetStreamCreate(pEngine, &tEvents, &tWarmClientStats);
		if ( !pWarmClient ) {
			printf("warm_create_failed\n");
			iExitCode = 2;
			goto cleanup;
		}

		iConnectStartNs = xbenchNowNs();
		if ( xrtNetStreamConnect(pWarmClient, &tConnCfg) != XRT_NET_OK ) {
			printf("warm_connect_failed\n");
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pWarmAccepted; ++iAcceptRetry ) {
			pWarmAccepted = __xnetListenerTryAcceptOne(pListener, &tWarmServerStats);
			if ( !pWarmAccepted ) xbenchSleepMs(10u);
		}
		if ( !pWarmAccepted ) {
			printf("warm_accept_failed\n");
			xrtNetStreamClose(pWarmClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		if ( !xbenchWaitMin(&tWarmClientStats.iOpenCount, 1, 3000u) ||
			!xbenchWaitMin(&tWarmServerStats.iOpenCount, 1, 3000u) ) {
			printf("warm_open_timeout client_open=%ld server_open=%ld\n",
				xbenchAtomicLoad(&tWarmClientStats.iOpenCount),
				xbenchAtomicLoad(&tWarmServerStats.iOpenCount));
			xrtNetStreamClose(pWarmAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pWarmClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pWarmAccepted);
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		iClientOpenNs = xbenchAtomicLoadU64(&tWarmClientStats.iLastOpenNs);
		iServerOpenNs = xbenchAtomicLoadU64(&tWarmServerStats.iLastOpenNs);
		iWarmHandshakeNs = ((iClientOpenNs > iServerOpenNs) ? iClientOpenNs : iServerOpenNs) - iConnectStartNs;

		if ( xrtNetStreamSend(pWarmClient, "warm tls", 8) != XRT_NET_OK ||
			!xbenchWaitMin(&tWarmServerStats.iRecvCount, 1, 3000u) ||
			!xbenchWaitMin(&tWarmClientStats.iRecvCount, 1, 3000u) ||
			strcmp(tWarmClientStats.aLastRecv, "warm tls") != 0 ) {
			printf("warm_payload_failed client_recv=%ld server_recv=%ld payload='%s'\n",
				xbenchAtomicLoad(&tWarmClientStats.iRecvCount),
				xbenchAtomicLoad(&tWarmServerStats.iRecvCount),
				tWarmClientStats.aLastRecv);
			xrtNetStreamClose(pWarmAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pWarmClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pWarmAccepted);
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		if ( pWarmClient->pTls ) {
			pResume = xrtNetTlsSessionExportResume(pWarmClient->pTls);
		}
		if ( !pResume ) {
			printf("warm_resume_export_failed\n");
			xrtNetStreamClose(pWarmAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pWarmClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pWarmAccepted);
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		xrtNetStreamClose(pWarmClient, XNET_CLOSE_F_GRACEFUL);
		if ( !xbenchWaitMin(&tWarmClientStats.iCloseCount, 1, 3000u) ||
			!xbenchWaitMin(&tWarmServerStats.iCloseCount, 1, 3000u) ) {
			printf("warm_close_timeout client_close=%ld server_close=%ld\n",
				xbenchAtomicLoad(&tWarmClientStats.iCloseCount),
				xbenchAtomicLoad(&tWarmServerStats.iCloseCount));
			xrtNetStreamDestroy(pWarmAccepted);
			xrtNetStreamDestroy(pWarmClient);
			iExitCode = 2;
			goto cleanup;
		}

		xrtNetStreamDestroy(pWarmAccepted);
		xrtNetStreamDestroy(pWarmClient);
		xbenchSleepMs(30u);
	}

	tClientTls.pResume = pResume;
	iResumeLoopStartNs = xbenchNowNs();
	for ( uint32_t i = 0; i < iIterations; ++i ) {
		__bench_tls_resume_stats tClientStats;
		__bench_tls_resume_stats tServerStats;
		xnetstream* pClient = NULL;
		xnetstream* pAccepted = NULL;
		uint64_t iConnectStartNs;
		uint64_t iClientOpenNs;
		uint64_t iServerOpenNs;
		uint64_t iResumeNs;
		char aMsg[64];

		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tServerStats, 0, sizeof(tServerStats));
		tServerStats.bEchoBack = true;
		snprintf(aMsg, sizeof(aMsg), "resume-%u", (unsigned)i);

		pClient = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats);
		if ( !pClient ) {
			printf("resume_create_failed_at=%u\n", (unsigned)i);
			iExitCode = 2;
			goto cleanup;
		}

		iConnectStartNs = xbenchNowNs();
		if ( xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
			printf("resume_connect_failed_at=%u\n", (unsigned)i);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted; ++iAcceptRetry ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tServerStats);
			if ( !pAccepted ) xbenchSleepMs(10u);
		}
		if ( !pAccepted ) {
			printf("resume_accept_failed_at=%u\n", (unsigned)i);
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}

		if ( !xbenchWaitMin(&tClientStats.iOpenCount, 1, 3000u) ||
			!xbenchWaitMin(&tServerStats.iOpenCount, 1, 3000u) ) {
			printf("resume_open_timeout_at=%u client_open=%ld server_open=%ld\n",
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

		if ( xbenchAtomicLoad(&tClientStats.iResumedCount) <= 0 ||
			xbenchAtomicLoad(&tServerStats.iResumedCount) <= 0 ) {
			printf("resume_state_failed_at=%u client_resumed=%ld server_resumed=%ld\n",
				(unsigned)i,
				xbenchAtomicLoad(&tClientStats.iResumedCount),
				xbenchAtomicLoad(&tServerStats.iResumedCount));
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}
		++iClientResumedOkCount;
		++iServerResumedOkCount;

		iClientOpenNs = xbenchAtomicLoadU64(&tClientStats.iLastOpenNs);
		iServerOpenNs = xbenchAtomicLoadU64(&tServerStats.iLastOpenNs);
		iResumeNs = ((iClientOpenNs > iServerOpenNs) ? iClientOpenNs : iServerOpenNs) - iConnectStartNs;
		arrResumeNs[i] = iResumeNs;
		iResumeSumNs += iResumeNs;

		if ( xrtNetStreamSend(pClient, aMsg, strlen(aMsg)) != XRT_NET_OK ||
			!xbenchWaitMin(&tServerStats.iRecvCount, 1, 3000u) ||
			!xbenchWaitMin(&tClientStats.iRecvCount, 1, 3000u) ||
			strcmp(tClientStats.aLastRecv, aMsg) != 0 ) {
			printf("resume_payload_failed_at=%u client_recv=%ld server_recv=%ld payload='%s'\n",
				(unsigned)i,
				xbenchAtomicLoad(&tClientStats.iRecvCount),
				xbenchAtomicLoad(&tServerStats.iRecvCount),
				tClientStats.aLastRecv);
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			iExitCode = 2;
			goto cleanup;
		}
		++iPayloadOkCount;

		xrtNetStreamClose(pClient, XNET_CLOSE_F_GRACEFUL);
		if ( !xbenchWaitMin(&tClientStats.iCloseCount, 1, 3000u) ||
			!xbenchWaitMin(&tServerStats.iCloseCount, 1, 3000u) ) {
			printf("resume_close_timeout_at=%u client_close=%ld server_close=%ld\n",
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
			printf("resume_error_path_at=%u client_errors=%ld server_errors=%ld\n",
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
	iResumeLoopEndNs = xbenchNowNs();

	qsort(arrResumeNs, iIterations, sizeof(uint64_t), __benchTlsResumeLatencyCmp);
	fP50 = (double)arrResumeNs[(iIterations - 1u) / 2u] / 1000.0;
	fP95 = (double)arrResumeNs[(uint32_t)((double)(iIterations - 1u) * 0.95)] / 1000.0;
	fP99 = (double)arrResumeNs[(uint32_t)((double)(iIterations - 1u) * 0.99)] / 1000.0;

	xbenchPrintMetricU64("warm_handshake_ns", iWarmHandshakeNs);
	xbenchPrintMetricDouble("warm_handshake_us", (double)iWarmHandshakeNs / 1000.0);
	xbenchPrintMetricU64("resume_sum_ns", iResumeSumNs);
	xbenchPrintMetricDouble("resumed_handshakes_per_sec_active", xbenchSafeRate(iIterations, iResumeSumNs));
	xbenchPrintMetricDouble("resume_avg_us", (double)iResumeSumNs / (double)iIterations / 1000.0);
	xbenchPrintMetricDouble("resume_p50_us", fP50);
	xbenchPrintMetricDouble("resume_p95_us", fP95);
	xbenchPrintMetricDouble("resume_p99_us", fP99);
	xbenchPrintMetricU64("resume_loop_elapsed_ns", iResumeLoopEndNs - iResumeLoopStartNs);
	xbenchPrintMetricDouble("resumed_handshakes_per_sec_loop", xbenchSafeRate(iIterations, iResumeLoopEndNs - iResumeLoopStartNs));
	xbenchPrintMetricU64("resumed_client_ok_count", iClientResumedOkCount);
	xbenchPrintMetricU64("resumed_server_ok_count", iServerResumedOkCount);
	xbenchPrintMetricU64("resumed_payload_ok_count", iPayloadOkCount);

cleanup:
	if ( pResume ) xrtNetTlsResumeDestroy(pResume);
	if ( pListener ) {
		xrtNetListenerStop(pListener);
		xrtNetListenerDestroy(pListener);
	}
	if ( pEngine ) {
		xbenchSleepMs(20u);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	free(arrResumeNs);
	xbenchNetUnit();
	return iExitCode;
}
