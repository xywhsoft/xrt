#include "test_xnet_impl_env.h"

#if defined(__linux__)
	#include <stdio.h>
	#include <time.h>
	#include <unistd.h>
#else
	#include <stdio.h>
#endif

typedef struct {
	char aRecv[256];
	size_t iRecvLen;
	volatile long iRecvCount;
	volatile long iErrorCount;
	bool bEchoBack;
} __xnet_linux_native_stream_ctx;

static void __xnet_linux_sleep_ms(uint32 iDelayMs)
{
	#if defined(__linux__)
		usleep((useconds_t)iDelayMs * 1000u);
	#else
		(void)iDelayMs;
	#endif
}

static int64_t __xnet_linux_now_ms(void)
{
	#if defined(__linux__)
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return (int64_t)tNow.tv_sec * 1000 + (int64_t)(tNow.tv_nsec / 1000000);
	#else
		return 0;
	#endif
}

static void __xnet_linux_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__xnet_linux_native_stream_ctx* pCtx = (__xnet_linux_native_stream_ctx*)pOwner;
	size_t iBytes;
	size_t iCopy;
	if ( !pCtx || !pChain ) return;
	iBytes = xrtNetChainBytes(pChain);
	if ( iBytes > 0 ) {
		iCopy = iBytes;
		if ( iCopy > sizeof(pCtx->aRecv) - 1u ) iCopy = sizeof(pCtx->aRecv) - 1u;
		(void)xrtNetChainPeek(pChain, pCtx->aRecv, iCopy);
		pCtx->aRecv[iCopy] = '\0';
		pCtx->iRecvLen = iCopy;
		if ( pCtx->bEchoBack && pStream ) {
			(void)xrtNetStreamSend(pStream, pCtx->aRecv, iCopy);
		}
		xrtNetChainConsume(pChain, iBytes);
	}
	pCtx->iRecvCount++;
}

static void __xnet_linux_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__xnet_linux_native_stream_ctx* pCtx = (__xnet_linux_native_stream_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx ) {
		pCtx->iErrorCount++;
	}
}

static const xnetstreamevents __g_xnet_linux_native_stream_events = {
	NULL,
	__xnet_linux_on_recv,
	NULL,
	NULL,
	__xnet_linux_on_error,
	NULL,
	NULL
};

static bool __xnet_linux_wait_open(xnetstream* pStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __xnet_linux_now_ms() + (int64_t)iTimeoutMs;
	if ( !pStream ) return false;
	while ( __xnet_linux_now_ms() < iDeadlineMs ) {
		if ( (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) return true;
		__xnet_linux_sleep_ms(5);
	}
	return (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0;
}

static bool __xnet_linux_wait_recv(__xnet_linux_native_stream_ctx* pCtx, const char* sExpect, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __xnet_linux_now_ms() + (int64_t)iTimeoutMs;
	if ( !pCtx || !sExpect ) return false;
	while ( __xnet_linux_now_ms() < iDeadlineMs ) {
		if ( pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0 ) {
			return true;
		}
		__xnet_linux_sleep_ms(5);
	}
	return pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0;
}

static bool __xnet_linux_run_rounds(uint32 iRounds, bool bTls)
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xtlsconfig tServerTls;
	xtlsconfig tClientTls;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	bool bOk = true;
	bool bRingReady = false;
	const char* sFailStage = NULL;
	uint32 iFailRound = 0;

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetConnectConfigInit(&tConnCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = 0;

	if ( bTls ) {
		memset(&tServerTls, 0, sizeof(tServerTls));
		memset(&tClientTls, 0, sizeof(tClientTls));
		tServerTls.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tServerTls.sKeyFile = "dev/xnet2_tls_test_key.pem";
		tServerTls.bVerifyPeer = false;
		tClientTls.sHostName = "localhost";
		tClientTls.bVerifyPeer = false;
		tListenCfg.pTlsConfig = &tServerTls;
		tConnCfg.pTlsConfig = &tClientTls;
	}

	pEngine = xrtNetEngineCreate(&tCfg);
	if ( !pEngine ) return false;
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		xrtNetEngineDestroy(pEngine);
		return false;
	}
	if ( pEngine->iWorkerCount == 0 || !__xnetPortUringHasNativeRing(&pEngine->arrWorkers[0].tPort) ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		return false;
	}
	bRingReady = true;

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &__g_xnet_linux_native_stream_events, NULL);
	if ( !pListener || xrtNetListenerStart(pListener) != XRT_NET_OK ) {
		if ( pListener ) xrtNetListenerDestroy(pListener);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		return false;
	}

	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;

	for ( uint32 i = 0; i < iRounds && bOk; ++i ) {
		char aMsg[64];
		xnetfuture* pAcceptFuture = NULL;
		xnetstream* pClient = NULL;
		xnetstream* pAccepted = NULL;
		__xnet_linux_native_stream_ctx tClientCtx;
		__xnet_linux_native_stream_ctx tServerCtx;
		xnet_result iAcceptStatus;

		memset(&tClientCtx, 0, sizeof(tClientCtx));
		memset(&tServerCtx, 0, sizeof(tServerCtx));
		tServerCtx.bEchoBack = true;
		snprintf(aMsg, sizeof(aMsg), "linux-native-%s-%u", bTls ? "tls" : "tcp", (unsigned)i);

		pAcceptFuture = xrtNetListenerAcceptFuture(pListener);
		pClient = xrtNetStreamCreate(pEngine, &__g_xnet_linux_native_stream_events, &tClientCtx);
		if ( !pAcceptFuture || !pClient ) {
			sFailStage = "create client/accept future";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
			sFailStage = "connect submit";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk ) {
			iAcceptStatus = xrtNetFutureWait(pAcceptFuture, bTls ? 6000u : 3000u);
			if ( iAcceptStatus != XRT_NET_OK ) {
				sFailStage = "accept future wait";
				iFailRound = i;
				bOk = false;
			}
		}
		if ( bOk ) {
			pAccepted = (xnetstream*)xrtNetFutureValue(pAcceptFuture);
			if ( !pAccepted ) {
				sFailStage = "accept future value";
				iFailRound = i;
				bOk = false;
			}
		}
		if ( bOk ) {
			xrtNetStreamSetUserData(pAccepted, &tServerCtx);
			if ( !__xnet_linux_wait_open(pClient, bTls ? 8000u : 3000u) ) {
				sFailStage = "wait client open";
				iFailRound = i;
				bOk = false;
			}
			if ( bOk && !__xnet_linux_wait_open(pAccepted, bTls ? 8000u : 3000u) ) {
				sFailStage = "wait accepted open";
				iFailRound = i;
				bOk = false;
			}
		}
		if ( bOk && xrtNetStreamSend(pClient, aMsg, strlen(aMsg)) != XRT_NET_OK ) {
			sFailStage = "client send";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && !__xnet_linux_wait_recv(&tServerCtx, aMsg, bTls ? 8000u : 3000u) ) {
			sFailStage = "server recv/echo";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && !__xnet_linux_wait_recv(&tClientCtx, aMsg, bTls ? 8000u : 3000u) ) {
			sFailStage = "client recv echo";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && (tClientCtx.iErrorCount != 0 || tServerCtx.iErrorCount != 0) ) {
			sFailStage = "error callback";
			iFailRound = i;
			bOk = false;
		}

		if ( !bOk ) {
			fprintf(stderr,
				"[linux-native] mode=%s round=%u stage=%s client_open=%u accepted_open=%u client_recv=%ld server_recv=%ld client_err=%ld server_err=%ld client_msg='%s' server_msg='%s' pending_client=%u pending_accepted=%u\n",
				bTls ? "tls" : "tcp",
				(unsigned)iFailRound,
				sFailStage ? sFailStage : "unknown",
				pClient ? (((pClient->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0) ? 1u : 0u) : 0u,
				pAccepted ? (((pAccepted->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0) ? 1u : 0u) : 0u,
				tClientCtx.iRecvCount,
				tServerCtx.iRecvCount,
				tClientCtx.iErrorCount,
				tServerCtx.iErrorCount,
				tClientCtx.aRecv,
				tServerCtx.aRecv,
				pClient ? (unsigned)xrtNetStreamPendingSend(pClient) : 0u,
				pAccepted ? (unsigned)xrtNetStreamPendingSend(pAccepted) : 0u);
		}

		if ( pAcceptFuture ) xrtNetFutureDestroy(pAcceptFuture);
		if ( pClient ) {
			xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pClient);
		}
		if ( pAccepted ) {
			xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
			xrtNetStreamDestroy(pAccepted);
		}
	}

	if ( pListener ) {
		xrtNetListenerStop(pListener);
		xrtNetListenerDestroy(pListener);
	}
	if ( pEngine ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	return bOk && bRingReady;
}

int main(int argc, char** argv)
{
	#if !defined(__linux__)
		(void)argc;
		(void)argv;
		printf("XNet Linux native backend core test: skipped (non-Linux)\n");
		return 0;
	#else
		xrtGlobalData* pCore;
		uint32 iPlainRounds = 16u;
		uint32 iTlsRounds = 4u;
		bool bPlainOk;
		bool bTlsOk;

		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		if ( argc > 1 ) iPlainRounds = (uint32)strtoul(argv[1], NULL, 10);
		if ( argc > 2 ) iTlsRounds = (uint32)strtoul(argv[2], NULL, 10);

		pCore = xrtInit();
		if ( !pCore ) {
			return 1;
		}

		printf("XNet Linux native backend core test:\n");
		printf("  plain rounds=%u tls rounds=%u\n", (unsigned)iPlainRounds, (unsigned)iTlsRounds);
		bPlainOk = __xnet_linux_run_rounds(iPlainRounds, false);
		bTlsOk = __xnet_linux_run_rounds(iTlsRounds, true);
		printf("  native TCP repeated connect/echo/close : %s\n", bPlainOk ? "PASS" : "FAIL");
		printf("  native TLS repeated connect/echo/close : %s\n", bTlsOk ? "PASS" : "FAIL");

		xrtUnit();
		return (bPlainOk && bTlsOk) ? 0 : 1;
	#endif
}
