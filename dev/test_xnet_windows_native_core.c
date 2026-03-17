#include "test_xnet_impl_env.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#else
	#include <stdio.h>
#endif

typedef struct {
	char aRecv[256];
	size_t iRecvLen;
	volatile long iRecvCount;
	volatile long iErrorCount;
	bool bEchoBack;
} __xnet_native_stream_ctx;

static void __xnet_native_sleep_ms(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		(void)iDelayMs;
	#endif
}

static int64_t __xnet_native_now_ms(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64_t)GetTickCount64();
	#else
		return 0;
	#endif
}

static void __xnet_native_on_recv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__xnet_native_stream_ctx* pCtx = (__xnet_native_stream_ctx*)pOwner;
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

static void __xnet_native_on_error(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__xnet_native_stream_ctx* pCtx = (__xnet_native_stream_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( pCtx ) {
		pCtx->iErrorCount++;
	}
}

static const xnetstreamevents __g_xnet_native_stream_events = {
	NULL,
	__xnet_native_on_recv,
	NULL,
	NULL,
	__xnet_native_on_error,
	NULL,
	NULL
};

static bool __xnet_native_wait_open(xnetstream* pStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __xnet_native_now_ms() + (int64_t)iTimeoutMs;
	if ( !pStream ) return false;
	while ( __xnet_native_now_ms() < iDeadlineMs ) {
		if ( (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) return true;
		__xnet_native_sleep_ms(5);
	}
	return (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0;
}

static bool __xnet_native_wait_recv(__xnet_native_stream_ctx* pCtx, const char* sExpect, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __xnet_native_now_ms() + (int64_t)iTimeoutMs;
	if ( !pCtx || !sExpect ) return false;
	while ( __xnet_native_now_ms() < iDeadlineMs ) {
		if ( pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0 ) {
			return true;
		}
		__xnet_native_sleep_ms(5);
	}
	return pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0;
}

static bool __xnet_native_destroy_stream_wait(xnetstream** ppStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs;
	const char* sErr;
	xnetstream* pStream;

	if ( !ppStream || !*ppStream ) return true;
	pStream = *ppStream;
	iDeadlineMs = __xnet_native_now_ms() + (int64_t)iTimeoutMs;

	for ( ;; ) {
		xrtClearError();
		xrtNetStreamDestroy(pStream);
		sErr = xrtGetError();
		if ( !sErr || sErr[0] == '\0' ) {
			*ppStream = NULL;
			return true;
		}
		if ( strcmp(sErr, "cannot destroy stream while an async waiter or task still holds it.") != 0 ) {
			return false;
		}
		if ( __xnet_native_now_ms() >= iDeadlineMs ) {
			return false;
		}
		__xnet_native_sleep_ms(5);
	}
}

static bool __xnet_native_run_rounds(uint32 iRounds, bool bTls)
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xtlsconfig tTlsCfg;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	bool bOk = true;

	xrtNetEngineConfigInit(&tCfg);
	tCfg.iWorkerCount = 1;
	xrtNetListenConfigInit(&tListenCfg);
	xrtNetConnectConfigInit(&tConnCfg);
	xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
	tConnCfg.sHost = "127.0.0.1";
	tConnCfg.iPort = 0;

	if ( bTls ) {
		memset(&tTlsCfg, 0, sizeof(tTlsCfg));
		tTlsCfg.sCertFile = "dev/xnet2_tls_test_cert.pem";
		tTlsCfg.sKeyFile = "dev/xnet2_tls_test_key.pem";
		tTlsCfg.sHostName = "localhost";
		tTlsCfg.bVerifyPeer = false;
		tListenCfg.pTlsConfig = &tTlsCfg;
		tConnCfg.pTlsConfig = &tTlsCfg;
	}

	pEngine = xrtNetEngineCreate(&tCfg);
	if ( !pEngine ) return false;
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		xrtNetEngineDestroy(pEngine);
		return false;
	}

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &__g_xnet_native_stream_events, NULL);
	if ( !pListener || xrtNetListenerStart(pListener) != XRT_NET_OK ) {
		if ( pListener ) xrtNetListenerDestroy(pListener);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		return false;
	}

	tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;

	for ( uint32 i = 0; i < iRounds && bOk; ++i ) {
		char aMsg[64];
		const char* sFailStage = NULL;
		xnetfuture* pAcceptFuture = NULL;
		xnetstream* pClient = NULL;
		xnetstream* pAccepted = NULL;
		__xnet_native_stream_ctx tClientCtx;
		__xnet_native_stream_ctx tServerCtx;
		xnet_result iAcceptStatus;

		memset(&tClientCtx, 0, sizeof(tClientCtx));
		memset(&tServerCtx, 0, sizeof(tServerCtx));
		tServerCtx.bEchoBack = true;
		snprintf(aMsg, sizeof(aMsg), "native-%s-round-%u", bTls ? "tls" : "tcp", (unsigned)i);

		pAcceptFuture = xrtNetListenerAcceptFuture(pListener);
		pClient = xrtNetStreamCreate(pEngine, &__g_xnet_native_stream_events, &tClientCtx);
		if ( !pAcceptFuture || !pClient ) {
			sFailStage = "future-or-client-create";
			bOk = false;
		}
		if ( bOk && xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
			sFailStage = "connect";
			bOk = false;
		}
		if ( bOk ) {
			iAcceptStatus = xrtNetFutureWait(pAcceptFuture, bTls ? 5000u : 2000u);
			if ( iAcceptStatus != XRT_NET_OK ) {
				sFailStage = "accept-wait";
				bOk = false;
			}
		}
		if ( bOk ) {
			pAccepted = (xnetstream*)xrtNetFutureValue(pAcceptFuture);
			if ( !pAccepted ) {
				sFailStage = "accept-value";
				bOk = false;
			}
		}
		if ( bOk ) {
			xrtNetStreamSetUserData(pAccepted, &tServerCtx);
			if ( !__xnet_native_wait_open(pClient, bTls ? 6000u : 2000u) ) {
				sFailStage = "client-open";
				bOk = false;
			}
			if ( bOk && !__xnet_native_wait_open(pAccepted, bTls ? 6000u : 2000u) ) {
				sFailStage = "accepted-open";
				bOk = false;
			}
		}
		if ( bOk && xrtNetStreamSend(pClient, aMsg, strlen(aMsg)) != XRT_NET_OK ) {
			sFailStage = "client-send";
			bOk = false;
		}
		if ( bOk && !__xnet_native_wait_recv(&tServerCtx, aMsg, bTls ? 6000u : 2000u) ) {
			sFailStage = "server-recv";
			bOk = false;
		}
		if ( bOk && !__xnet_native_wait_recv(&tClientCtx, aMsg, bTls ? 6000u : 2000u) ) {
			sFailStage = "client-recv";
			bOk = false;
		}
		if ( bOk && (tClientCtx.iErrorCount != 0 || tServerCtx.iErrorCount != 0) ) {
			sFailStage = "error-count";
			bOk = false;
		}

		if ( pAcceptFuture ) xrtNetFutureDestroy(pAcceptFuture);
		if ( pClient ) xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
		if ( pAccepted ) xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
		if ( bOk && !__xnet_native_destroy_stream_wait(&pClient, bTls ? 6000u : 2000u) ) {
			sFailStage = "client-destroy";
			bOk = false;
		}
		if ( bOk && !__xnet_native_destroy_stream_wait(&pAccepted, bTls ? 6000u : 2000u) ) {
			sFailStage = "accepted-destroy";
			bOk = false;
		}
		if ( pClient ) {
			xrtClearError();
			xrtNetStreamDestroy(pClient);
		}
		if ( pAccepted ) {
			xrtClearError();
			xrtNetStreamDestroy(pAccepted);
		}
		if ( !bOk ) {
			printf("    round %u fail stage=%s clientRecv=%ld serverRecv=%ld clientErr=%ld serverErr=%ld\n",
				(unsigned)i,
				sFailStage ? sFailStage : "unknown",
				tClientCtx.iRecvCount,
				tServerCtx.iRecvCount,
				tClientCtx.iErrorCount,
				tServerCtx.iErrorCount);
		}
	}

	xrtNetListenerStop(pListener);
	xrtNetListenerDestroy(pListener);
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	return bOk;
}

int main(int argc, char** argv)
{
	#if !defined(_WIN32) && !defined(_WIN64)
		(void)argc;
		(void)argv;
		printf("XNet Windows native backend core test: skipped (non-Windows)\n");
		return 0;
	#else
		WSADATA tWSA;
		xrtGlobalData* pCore;
		uint32 iPlainRounds = 32u;
		uint32 iTlsRounds = 8u;
		bool bPlainOk;
		bool bTlsOk;

		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
		if ( argc > 1 ) iPlainRounds = (uint32)strtoul(argv[1], NULL, 10);
		if ( argc > 2 ) iTlsRounds = (uint32)strtoul(argv[2], NULL, 10);
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) {
			return 1;
		}

		pCore = xrtInit();
		if ( !pCore ) {
			WSACleanup();
			return 1;
		}

		printf("XNet Windows native backend core test:\n");
		printf("  plain rounds=%u tls rounds=%u\n", (unsigned)iPlainRounds, (unsigned)iTlsRounds);
		bPlainOk = __xnet_native_run_rounds(iPlainRounds, false);
		bTlsOk = __xnet_native_run_rounds(iTlsRounds, true);
		printf("  native TCP repeated connect/echo/close : %s\n", bPlainOk ? "PASS" : "FAIL");
		printf("  native TLS repeated connect/echo/close : %s\n", bTlsOk ? "PASS" : "FAIL");

		xrtUnit();
		WSACleanup();
		return (bPlainOk && bTlsOk) ? 0 : 1;
	#endif
}
