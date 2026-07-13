#ifndef TEST_XNET_NATIVE_CORE_H
#define TEST_XNET_NATIVE_CORE_H

#include "test_xnet_internal_env.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#elif defined(__linux__)
	#include <time.h>
	#include <unistd.h>
#endif

#if defined(XRT_NO_NETWORK)

// XNET原生核心WITHROUNDS测试
static int Test_XNetNativeCoreWithRounds(uint32 iPlainRounds, uint32 iTlsRounds)
{
	(void)iPlainRounds;
	(void)iTlsRounds;
	printf("XNet native backend core test: skipped (network disabled)\n");
	return 0;
}


// XNET原生核心测试
static int Test_XNetNativeCore(void)
{
	return Test_XNetNativeCoreWithRounds(0u, 0u);
}

#else

typedef struct
{
	char aRecv[256];
	size_t iRecvLen;
	volatile long iRecvCount;
	volatile long iErrorCount;
	bool bEchoBack;
} __test_xnet_native_core_stream_ctx;


// 内部函数：__Test_XNetNativeCore_FileExists
static bool __Test_XNetNativeCore_FileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_XNetNativeCore_SleepMs
static void __Test_XNetNativeCore_SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#elif defined(__linux__)
		usleep((useconds_t)iDelayMs * 1000u);
	#else
		(void)iDelayMs;
	#endif
}


// 内部函数：__Test_XNetNativeCore_NowMs
static int64_t __Test_XNetNativeCore_NowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64_t)GetTickCount64();
	#elif defined(__linux__)
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) return 0;
		return ((int64_t)tNow.tv_sec * 1000LL) + ((int64_t)tNow.tv_nsec / 1000000LL);
	#else
		return 0;
	#endif
}


// 内部函数：__Test_XNetNativeCore_OnRecv
static void __Test_XNetNativeCore_OnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xnet_native_core_stream_ctx* pCtx = (__test_xnet_native_core_stream_ctx*)pOwner;
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


// 内部函数：__Test_XNetNativeCore_OnError
static void __Test_XNetNativeCore_OnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xnet_native_core_stream_ctx* pCtx = (__test_xnet_native_core_stream_ctx*)pOwner;
	(void)pStream;
	(void)iSysErr;

	if ( pCtx ) {
		pCtx->iErrorCount++;
	}
}

static const xnetstreamevents __g_Test_XNetNativeCore_StreamEvents = {
	NULL,
	__Test_XNetNativeCore_OnRecv,
	NULL,
	NULL,
	__Test_XNetNativeCore_OnError,
	NULL,
	NULL,
	NULL
};


// 内部函数：__Test_XNetNativeCore_WaitOpen
static bool __Test_XNetNativeCore_WaitOpen(xnetstream* pStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNetNativeCore_NowMs() + (int64_t)iTimeoutMs;

	if ( !pStream ) return false;

	while ( __Test_XNetNativeCore_NowMs() < iDeadlineMs ) {
		if ( (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) return true;
		__Test_XNetNativeCore_SleepMs(5);
	}

	return (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0;
}


// 内部函数：__Test_XNetNativeCore_WaitRecv
static bool __Test_XNetNativeCore_WaitRecv(__test_xnet_native_core_stream_ctx* pCtx, const char* sExpect, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNetNativeCore_NowMs() + (int64_t)iTimeoutMs;

	if ( !pCtx || !sExpect ) return false;

	while ( __Test_XNetNativeCore_NowMs() < iDeadlineMs ) {
		if ( pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0 ) {
			return true;
		}
		__Test_XNetNativeCore_SleepMs(5);
	}

	return pCtx->iRecvCount > 0 && strcmp(pCtx->aRecv, sExpect) == 0;
}


// 内部函数：__Test_XNetNativeCore_DestroyStreamWait
static bool __Test_XNetNativeCore_DestroyStreamWait(xnetstream** ppStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs;
	const char* sErr;
	xnetstream* pStream;

	if ( !ppStream || !*ppStream ) return true;

	pStream = *ppStream;
	iDeadlineMs = __Test_XNetNativeCore_NowMs() + (int64_t)iTimeoutMs;

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
		if ( __Test_XNetNativeCore_NowMs() >= iDeadlineMs ) {
			return false;
		}
		__Test_XNetNativeCore_SleepMs(5);
	}
}


// 内部函数：__Test_XNetNativeCore_HasNativeBackend
static bool __Test_XNetNativeCore_HasNativeBackend(xnetengine* pEngine)
{
	#if defined(__linux__)
		return pEngine != NULL &&
			pEngine->iWorkerCount > 0 &&
			__xnetPortUringHasNativeRing(&pEngine->arrWorkers[0].tPort);
	#elif defined(_WIN32) || defined(_WIN64)
		return pEngine != NULL;
	#else
		(void)pEngine;
		return false;
	#endif
}


// 内部函数：__Test_XNetNativeCore_DefaultPlainRounds
static uint32 __Test_XNetNativeCore_DefaultPlainRounds(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return 32u;
	#elif defined(__linux__)
		return 16u;
	#else
		return 0u;
	#endif
}


// 内部函数：__Test_XNetNativeCore_DefaultTlsRounds
static uint32 __Test_XNetNativeCore_DefaultTlsRounds(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return 8u;
	#elif defined(__linux__)
		return 4u;
	#else
		return 0u;
	#endif
}


// 内部函数：__Test_XNetNativeCore_RunRounds
static bool __Test_XNetNativeCore_RunRounds(uint32 iRounds, bool bTls)
{
	xnetengineconfig tCfg;
	xnetlistenconfig tListenCfg;
	xnetconnectconfig tConnCfg;
	xtlsconfig tServerTls;
	xtlsconfig tClientTls;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	bool bOk = true;
	const char* sFailStage = NULL;
	uint32 iFailRound = 0;
	uint32 iOpenTimeoutMs = bTls ? 8000u : 3000u;
	uint32 iWaitTimeoutMs = bTls ? 6000u : 3000u;

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
	if ( !__Test_XNetNativeCore_HasNativeBackend(pEngine) ) {
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		return false;
	}

	pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &__g_Test_XNetNativeCore_StreamEvents, NULL);
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
		__test_xnet_native_core_stream_ctx tClientCtx;
		__test_xnet_native_core_stream_ctx tServerCtx;
		xnet_result iAcceptStatus;

		memset(&tClientCtx, 0, sizeof(tClientCtx));
		memset(&tServerCtx, 0, sizeof(tServerCtx));
		tServerCtx.bEchoBack = true;
		snprintf(aMsg, sizeof(aMsg), "native-%s-round-%u", bTls ? "tls" : "tcp", (unsigned)i);

		pAcceptFuture = xrtNetListenerAcceptFuture(pListener);
		pClient = xrtNetStreamCreate(pEngine, &__g_Test_XNetNativeCore_StreamEvents, &tClientCtx);
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
			iAcceptStatus = xrtNetFutureWait(pAcceptFuture, iWaitTimeoutMs);
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
			if ( !__Test_XNetNativeCore_WaitOpen(pClient, iOpenTimeoutMs) ) {
				sFailStage = "wait client open";
				iFailRound = i;
				bOk = false;
			}
			if ( bOk && !__Test_XNetNativeCore_WaitOpen(pAccepted, iOpenTimeoutMs) ) {
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
		if ( bOk && !__Test_XNetNativeCore_WaitRecv(&tServerCtx, aMsg, iOpenTimeoutMs) ) {
			sFailStage = "server recv/echo";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && !__Test_XNetNativeCore_WaitRecv(&tClientCtx, aMsg, iOpenTimeoutMs) ) {
			sFailStage = "client recv echo";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && (tClientCtx.iErrorCount != 0 || tServerCtx.iErrorCount != 0) ) {
			sFailStage = "error callback";
			iFailRound = i;
			bOk = false;
		}

		if ( pAcceptFuture ) xrtNetFutureDestroy(pAcceptFuture);
		if ( pClient ) xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
		if ( pAccepted ) xrtNetStreamClose(pAccepted, XNET_CLOSE_F_ABORT);
		if ( bOk && !__Test_XNetNativeCore_DestroyStreamWait(&pClient, iOpenTimeoutMs) ) {
			sFailStage = "client destroy";
			iFailRound = i;
			bOk = false;
		}
		if ( bOk && !__Test_XNetNativeCore_DestroyStreamWait(&pAccepted, iOpenTimeoutMs) ) {
			sFailStage = "accepted destroy";
			iFailRound = i;
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
			fprintf(stderr,
				"[xnet-native] mode=%s round=%u stage=%s client_open=%u accepted_open=%u client_recv=%ld server_recv=%ld client_err=%ld server_err=%ld client_msg='%s' server_msg='%s'\n",
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
				tServerCtx.aRecv);
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

	return bOk;
}


// XNET原生核心WITHROUNDS测试
static int Test_XNetNativeCoreWithRounds(uint32 iPlainRounds, uint32 iTlsRounds)
{
	bool bPlainOk;
	bool bTlsOk;
	bool bTlsSkip = false;

	#if !defined(_WIN32) && !defined(_WIN64) && !defined(__linux__)
		(void)iPlainRounds;
		(void)iTlsRounds;
		printf("XNet native backend core test: skipped (unsupported platform)\n");
		return 0;
	#else
		printf("XNet native backend core test:\n");
		printf("  plain rounds=%u tls rounds=%u\n", (unsigned)iPlainRounds, (unsigned)iTlsRounds);
		bPlainOk = __Test_XNetNativeCore_RunRounds(iPlainRounds, false);
		if ( !__Test_XNetNativeCore_FileExists("dev/xnet2_tls_test_cert.pem") ||
			!__Test_XNetNativeCore_FileExists("dev/xnet2_tls_test_key.pem") ) {
			bTlsSkip = true;
			bTlsOk = true;
		} else {
			bTlsOk = __Test_XNetNativeCore_RunRounds(iTlsRounds, true);
		}
		printf("  native TCP repeated connect/echo/close : %s\n", bPlainOk ? "PASS" : "FAIL");
		printf("  native TLS repeated connect/echo/close : %s\n", bTlsSkip ? "SKIP" : (bTlsOk ? "PASS" : "FAIL"));
		return (bPlainOk && bTlsOk) ? 0 : 1;
	#endif
}


// XNET原生核心测试
static int Test_XNetNativeCore(void)
{
	return Test_XNetNativeCoreWithRounds(
		__Test_XNetNativeCore_DefaultPlainRounds(),
		__Test_XNetNativeCore_DefaultTlsRounds());
}

#endif

#endif
