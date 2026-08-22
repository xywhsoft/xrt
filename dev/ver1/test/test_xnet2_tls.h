#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif

#include "test_atomic_compat.h"


typedef struct {
	volatile long iOpenCount;
	volatile long iRecvCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iLastRecvLen;
	char aLastRecv[128];
	bool bEchoBack;
} __test_xnet2_tls_stats;


// 内部函数：__Test_XNet2_TLSSleepMs
static void __Test_XNet2_TLSSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


// 内部函数：__Test_XNet2_TLSAtomicInc
static long __Test_XNet2_TLSAtomicInc(volatile long* pValue)
{
	return __xrtTestAtomicAddFetchLong(pValue, 1);
}


// 内部函数：__Test_XNet2_TLSAtomicLoad
static long __Test_XNet2_TLSAtomicLoad(volatile long* pValue)
{
	return __xrtTestAtomicLoadLong(pValue);
}


// 内部函数：__Test_XNet2_TLSAtomicStore
static void __Test_XNet2_TLSAtomicStore(volatile long* pValue, long iValue)
{
	__xrtTestAtomicStoreLong(pValue, iValue);
}


// 内部函数：__Test_XNet2_TLSWaitMin
static bool __Test_XNet2_TLSWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XNet2_TLSAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XNet2_TLSSleepMs(10);
	}
	return __Test_XNet2_TLSAtomicLoad(pValue) >= iExpectMin;
}


// 内部函数：__Test_XNet2_TLSWaitStreamQuiet
static bool __Test_XNet2_TLSWaitStreamQuiet(xnetstream* pStream, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( !pStream || __xnetAtomicLoad32(&pStream->iAsyncHoldCount) == 0 ) return true;
		__Test_XNet2_TLSSleepMs(10);
	}
	return !pStream || __xnetAtomicLoad32(&pStream->iAsyncHoldCount) == 0;
}


// 内部函数：__Test_XNet2_TLSFileExists
static bool __Test_XNet2_TLSFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}


// 内部函数：__Test_XNet2_TLSOnOpen
static void __Test_XNet2_TLSOnOpen(ptr pOwner, xnetstream* pStream)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iOpenCount);
}


// 内部函数：__Test_XNet2_TLSOnRecv
static void __Test_XNet2_TLSOnRecv(ptr pOwner, xnetstream* pStream, xnetchain* pChain)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	size_t iBytes = xrtNetChainBytes(pChain);
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iRecvCount);
	__Test_XNet2_TLSAtomicStore(&pStats->iLastRecvLen, (long)iBytes);
	memset(pStats->aLastRecv, 0, sizeof(pStats->aLastRecv));
	if ( iBytes > 0 ) {
		size_t iCopy = iBytes < (sizeof(pStats->aLastRecv) - 1u) ? iBytes : (sizeof(pStats->aLastRecv) - 1u);
		(void)xrtNetChainPeek(pChain, pStats->aLastRecv, iCopy);
		if ( pStats->bEchoBack ) {
			(void)xrtNetStreamSend(pStream, pStats->aLastRecv, iCopy);
		}
	}
	xrtNetChainConsume(pChain, iBytes);
}


// 内部函数：__Test_XNet2_TLSOnClose
static void __Test_XNet2_TLSOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iCloseCount);
}


// 内部函数：__Test_XNet2_TLSOnError
static void __Test_XNet2_TLSOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iErrorCount);
}


// 统一记录测试结果，避免“输出 FAIL 但测试进程仍返回成功”。
static void __Test_XNet2_TLSResult(bool* pAllPass, const char* sName, bool bPass)
{
	if ( pAllPass && !bPass ) { *pAllPass = false; }
	printf("  %s : %s\n", sName, bPass ? "PASS" : "FAIL");
}


// XNET2TLS测试
int Test_XNet2_TLS(void)
{
	bool bAllPass = true;

	printf("\n\n\n------------------------------------\n\n XNet2 TLS Adapter Test:\n\n");

	{
		const char* sCertPath = "dev/xnet2_tls_test_cert.pem";
		const char* sKeyPath = "dev/xnet2_tls_test_key.pem";
		xnetengineconfig tEngineCfg;
		xnetlistenconfig tListenCfg;
		xnetconnectconfig tConnCfg;
		xnetstreamevents tEvents;
		xtlsconfig tServerTls;
		xtlsconfig tClientTls;
		__test_xnet2_tls_stats tClientStats;
		__test_xnet2_tls_stats tServerStats;
		xnetengine* pEngine;
		xnetlistener* pListener;
		xnetstream* pClient;
		xnetstream* pAccepted = NULL;
		int iAcceptRetry;

		if ( !__Test_XNet2_TLSFileExists(sCertPath) || !__Test_XNet2_TLSFileExists(sKeyPath) ) {
			printf("  TLS fixture files missing : SKIP\n");
			return 0;
		}

		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tServerTls, 0, sizeof(tServerTls));
		memset(&tClientTls, 0, sizeof(tClientTls));
		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tServerStats, 0, sizeof(tServerStats));

		tEvents.OnOpen = __Test_XNet2_TLSOnOpen;
		tEvents.OnRecv = __Test_XNet2_TLSOnRecv;
		tEvents.OnClose = __Test_XNet2_TLSOnClose;
		tEvents.OnError = __Test_XNet2_TLSOnError;

		tServerStats.bEchoBack = true;

		tServerTls.sCertFile = sCertPath;
		tServerTls.sKeyFile = sKeyPath;
		tServerTls.bVerifyPeer = false;

		tClientTls.sHostName = "localhost";
		tClientTls.bVerifyPeer = false;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;

		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		tListenCfg.pTlsConfig = &tServerTls;

		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = "127.0.0.1";
		tConnCfg.pTlsConfig = &tClientTls;

		pEngine = xrtNetEngineCreate(&tEngineCfg);
		__Test_XNet2_TLSResult(&bAllPass, "Engine create", pEngine != NULL);
		if ( !pEngine ) { return 1; }

		__Test_XNet2_TLSResult(&bAllPass, "Engine start", xrtNetEngineStart(pEngine) == XRT_NET_OK);

		pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &tEvents, NULL);
		__Test_XNet2_TLSResult(&bAllPass, "Listener create", pListener != NULL);
		__Test_XNet2_TLSResult(&bAllPass, "Listener start",
			pListener && xrtNetListenerStart(pListener) == XRT_NET_OK);

		if ( pListener ) { tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort; }
		pClient = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats);
		__Test_XNet2_TLSResult(&bAllPass, "Client create", pClient != NULL);
		__Test_XNet2_TLSResult(&bAllPass, "Client connect (TLS)",
			pListener && pClient && xrtNetStreamConnect(pClient, &tConnCfg) == XRT_NET_OK);
		__Test_XNet2_TLSResult(&bAllPass, "TLS send before ready = AGAIN",
			pClient && xrtNetStreamSend(pClient, "pre", 3) == XRT_NET_AGAIN);

		for ( iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted; ++iAcceptRetry ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tServerStats);
			if ( !pAccepted ) __Test_XNet2_TLSSleepMs(10);
		}
		__Test_XNet2_TLSResult(&bAllPass, "Listener accept TLS client", pAccepted != NULL);

		__Test_XNet2_TLSResult(&bAllPass, "Client TLS open",
			__Test_XNet2_TLSWaitMin(&tClientStats.iOpenCount, 1, 3000));
		__Test_XNet2_TLSResult(&bAllPass, "Server TLS open",
			__Test_XNet2_TLSWaitMin(&tServerStats.iOpenCount, 1, 3000));

		__Test_XNet2_TLSResult(&bAllPass, "TLS client send",
			pClient && xrtNetStreamSend(pClient, "hello tls", 9) == XRT_NET_OK);
		__Test_XNet2_TLSResult(&bAllPass, "TLS server recv",
			__Test_XNet2_TLSWaitMin(&tServerStats.iRecvCount, 1, 3000));
		__Test_XNet2_TLSResult(&bAllPass, "TLS client echo recv",
			__Test_XNet2_TLSWaitMin(&tClientStats.iRecvCount, 1, 3000));
		__Test_XNet2_TLSResult(&bAllPass, "TLS echo payload",
			strcmp(tClientStats.aLastRecv, "hello tls") == 0);
		__Test_XNet2_TLSResult(&bAllPass, "TLS error-free client path",
			__Test_XNet2_TLSAtomicLoad(&tClientStats.iErrorCount) == 0);
		__Test_XNet2_TLSResult(&bAllPass, "TLS error-free server path",
			__Test_XNet2_TLSAtomicLoad(&tServerStats.iErrorCount) == 0);

		if ( pClient ) xrtNetStreamClose(pClient, XNET_CLOSE_F_GRACEFUL);
		(void)__Test_XNet2_TLSWaitMin(&tClientStats.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitMin(&tServerStats.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pClient, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pAccepted, 3000);

		if ( pAccepted ) xrtNetStreamDestroy(pAccepted);
		if ( pClient ) xrtNetStreamDestroy(pClient);
		__Test_XNet2_TLSSleepMs(20);
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		__Test_XNet2_TLSSleepMs(20);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}

	__Test_XNet2_TLSSleepMs(100);

	{
		const char* sCertPath = "dev/xnet2_tls_test_cert.pem";
		const char* sKeyPath = "dev/xnet2_tls_test_key.pem";
		xnetengineconfig tEngineCfg;
		xnetlistenconfig tListenCfg;
		xnetconnectconfig tConnCfg;
		xnetstreamevents tEvents;
		xtlsconfig tServerTls;
		xtlsconfig tClientTls;
		__test_xnet2_tls_stats arrClientStats[4];
		__test_xnet2_tls_stats arrServerStats[4];
		xnetengine* pEngine;
		xnetlistener* pListener;
		bool bAllOpen = true;
		bool bAllClose = true;
		bool bAllEcho = true;
		bool bErrorFree = true;
		bool bEngineStarted = false;
		bool bListenerStarted = false;
		const int iRounds = 4;

		if ( !__Test_XNet2_TLSFileExists(sCertPath) || !__Test_XNet2_TLSFileExists(sKeyPath) ) {
			printf("  TLS repeated handshakes : SKIP\n");
			return 0;
		}

		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tServerTls, 0, sizeof(tServerTls));
		memset(&tClientTls, 0, sizeof(tClientTls));
		memset(arrClientStats, 0, sizeof(arrClientStats));
		memset(arrServerStats, 0, sizeof(arrServerStats));
		tEvents.OnOpen = __Test_XNet2_TLSOnOpen;
		tEvents.OnRecv = __Test_XNet2_TLSOnRecv;
		tEvents.OnClose = __Test_XNet2_TLSOnClose;
		tEvents.OnError = __Test_XNet2_TLSOnError;
		tServerTls.sCertFile = sCertPath;
		tServerTls.sKeyFile = sKeyPath;
		tServerTls.bVerifyPeer = false;
		tClientTls.sHostName = "localhost";
		tClientTls.bVerifyPeer = false;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		tListenCfg.pTlsConfig = &tServerTls;
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = "127.0.0.1";
		tConnCfg.pTlsConfig = &tClientTls;

		pEngine = xrtNetEngineCreate(&tEngineCfg);
		bEngineStarted = pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK;
		pListener = bEngineStarted ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &tEvents, NULL) : NULL;
		bListenerStarted = pListener && xrtNetListenerStart(pListener) == XRT_NET_OK;
		if ( bListenerStarted ) {
			tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
		} else {
			bAllOpen = false;
		}

		for ( int iRound = 0; bEngineStarted && bListenerStarted && iRound < iRounds; ++iRound ) {
			__test_xnet2_tls_stats* pClientStats = &arrClientStats[iRound];
			__test_xnet2_tls_stats* pServerStats = &arrServerStats[iRound];
			xnetstream* pClient = NULL;
			xnetstream* pAccepted = NULL;

			memset(pClientStats, 0, sizeof(*pClientStats));
			memset(pServerStats, 0, sizeof(*pServerStats));

			pClient = xrtNetStreamCreate(pEngine, &tEvents, pClientStats);
			if ( !pClient || xrtNetStreamConnect(pClient, &tConnCfg) != XRT_NET_OK ) {
				bAllOpen = false;
				if ( pClient ) xrtNetStreamDestroy(pClient);
				break;
			}

			for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted; ++iAcceptRetry ) {
				pAccepted = __xnetListenerTryAcceptOne(pListener, pServerStats);
				if ( !pAccepted ) __Test_XNet2_TLSSleepMs(10);
			}
			if ( !pAccepted ) {
				bAllOpen = false;
				xrtNetStreamClose(pClient, XNET_CLOSE_F_ABORT);
				xrtNetStreamDestroy(pClient);
				break;
			}

			if ( !__Test_XNet2_TLSWaitMin(&pClientStats->iOpenCount, 1, 3000)
				|| !__Test_XNet2_TLSWaitMin(&pServerStats->iOpenCount, 1, 3000) ) {
				bAllOpen = false;
			}
			if ( bAllOpen ) {
				char sPayload[32];
				snprintf(sPayload, sizeof(sPayload), "native tls %d", iRound);
				pServerStats->bEchoBack = true;
				if ( xrtNetStreamSend(pClient, sPayload, strlen(sPayload)) != XRT_NET_OK
					|| !__Test_XNet2_TLSWaitMin(&pServerStats->iRecvCount, 1, 3000)
					|| !__Test_XNet2_TLSWaitMin(&pClientStats->iRecvCount, 1, 3000)
					|| strcmp(pClientStats->aLastRecv, sPayload) != 0 ) {
					bAllEcho = false;
				}
			}
			if ( __Test_XNet2_TLSAtomicLoad(&pClientStats->iErrorCount) != 0
				|| __Test_XNet2_TLSAtomicLoad(&pServerStats->iErrorCount) != 0 ) {
				bErrorFree = false;
			}

			xrtNetStreamClose(pClient, XNET_CLOSE_F_GRACEFUL);
			if ( !__Test_XNet2_TLSWaitMin(&pClientStats->iCloseCount, 1, 3000)
				|| !__Test_XNet2_TLSWaitMin(&pServerStats->iCloseCount, 1, 3000) ) {
				bAllClose = false;
			}
			(void)__Test_XNet2_TLSWaitStreamQuiet(pClient, 3000);
			(void)__Test_XNet2_TLSWaitStreamQuiet(pAccepted, 3000);

			xrtNetStreamDestroy(pAccepted);
			xrtNetStreamDestroy(pClient);
			__Test_XNet2_TLSSleepMs(20);
			if ( !bAllOpen || !bAllClose || !bAllEcho || !bErrorFree ) break;
		}

		__Test_XNet2_TLSResult(&bAllPass, "TLS repeated sequential opens", bAllOpen);
		__Test_XNet2_TLSResult(&bAllPass, "TLS native recv regression echo", bAllEcho);
		__Test_XNet2_TLSResult(&bAllPass, "TLS repeated sequential closes", bAllClose);
		__Test_XNet2_TLSResult(&bAllPass, "TLS repeated error-free path", bErrorFree);

		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		if ( pEngine ) {
			__Test_XNet2_TLSSleepMs(20);
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	__Test_XNet2_TLSSleepMs(100);

	{
		const char* sCertPath = "dev/xnet2_tls_test_cert.pem";
		const char* sKeyPath = "dev/xnet2_tls_test_key.pem";
		xnetengineconfig tEngineCfg;
		xnetlistenconfig tListenCfg;
		xnetconnectconfig tConnCfg;
		xnetstreamevents tEvents;
		xtlsconfig tServerTls;
		xtlsconfig tClientTls;
		__test_xnet2_tls_stats tClientStats1;
		__test_xnet2_tls_stats tServerStats1;
		__test_xnet2_tls_stats tClientStats2;
		__test_xnet2_tls_stats tServerStats2;
		xnetengine* pEngine = NULL;
		xnetlistener* pListener = NULL;
		xnetstream* pClient1 = NULL;
		xnetstream* pAccepted1 = NULL;
		xnetstream* pClient2 = NULL;
		xnetstream* pAccepted2 = NULL;
		xtlsresume* pResume = NULL;
		bool bWarmOpen = false;
		bool bResumeOpen = false;
		bool bResumeExported = false;
		bool bClientResumed = false;
		bool bServerResumed = false;

		if ( !__Test_XNet2_TLSFileExists(sCertPath) || !__Test_XNet2_TLSFileExists(sKeyPath) ) {
			printf("  TLS session resume : SKIP\n");
			return 0;
		}

		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tServerTls, 0, sizeof(tServerTls));
		memset(&tClientTls, 0, sizeof(tClientTls));
		memset(&tClientStats1, 0, sizeof(tClientStats1));
		memset(&tServerStats1, 0, sizeof(tServerStats1));
		memset(&tClientStats2, 0, sizeof(tClientStats2));
		memset(&tServerStats2, 0, sizeof(tServerStats2));

		tEvents.OnOpen = __Test_XNet2_TLSOnOpen;
		tEvents.OnRecv = __Test_XNet2_TLSOnRecv;
		tEvents.OnClose = __Test_XNet2_TLSOnClose;
		tEvents.OnError = __Test_XNet2_TLSOnError;
		tServerStats1.bEchoBack = true;
		tServerStats2.bEchoBack = true;

		tServerTls.sCertFile = sCertPath;
		tServerTls.sKeyFile = sKeyPath;
		tServerTls.bVerifyPeer = false;
		tServerTls.iMaxVersion = 0x0303u;

		tClientTls.sHostName = "localhost";
		tClientTls.bVerifyPeer = false;
		tClientTls.iMaxVersion = 0x0303u;

		xrtNetEngineConfigInit(&tEngineCfg);
		tEngineCfg.iWorkerCount = 1;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetAddrInitAny(&tListenCfg.tBindAddr, AF_INET, 0);
		tListenCfg.pTlsConfig = &tServerTls;
		xrtNetConnectConfigInit(&tConnCfg);
		tConnCfg.sHost = "127.0.0.1";
		tConnCfg.pTlsConfig = &tClientTls;

		pEngine = xrtNetEngineCreate(&tEngineCfg);
		pListener = pEngine ? xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &tEvents, NULL) : NULL;
		if ( pEngine ) (void)xrtNetEngineStart(pEngine);
		if ( pListener ) (void)xrtNetListenerStart(pListener);
		if ( pListener ) tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;

		if ( pEngine && pListener ) {
			pClient1 = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats1);
		}
		if ( pClient1 && xrtNetStreamConnect(pClient1, &tConnCfg) == XRT_NET_OK ) {
			for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted1; ++iAcceptRetry ) {
				pAccepted1 = __xnetListenerTryAcceptOne(pListener, &tServerStats1);
				if ( !pAccepted1 ) __Test_XNet2_TLSSleepMs(10);
			}
		}
		if ( pAccepted1 ) {
			bWarmOpen = __Test_XNet2_TLSWaitMin(&tClientStats1.iOpenCount, 1, 3000)
				&& __Test_XNet2_TLSWaitMin(&tServerStats1.iOpenCount, 1, 3000);
		}
		if ( bWarmOpen && pClient1 && pClient1->pTls ) {
			pResume = xrtNetTlsSessionExportResume(pClient1->pTls);
			bResumeExported = (pResume != NULL);
		}
		if ( pClient1 ) xrtNetStreamClose(pClient1, XNET_CLOSE_F_GRACEFUL);
		(void)__Test_XNet2_TLSWaitMin(&tClientStats1.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitMin(&tServerStats1.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pClient1, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pAccepted1, 3000);
		if ( pAccepted1 ) xrtNetStreamDestroy(pAccepted1);
		if ( pClient1 ) xrtNetStreamDestroy(pClient1);
		pAccepted1 = NULL;
		pClient1 = NULL;
		__Test_XNet2_TLSSleepMs(30);

		if ( pResume ) {
			tClientTls.pResume = pResume;
			pClient2 = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats2);
			if ( pClient2 && xrtNetStreamConnect(pClient2, &tConnCfg) == XRT_NET_OK ) {
				for ( int iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted2; ++iAcceptRetry ) {
					pAccepted2 = __xnetListenerTryAcceptOne(pListener, &tServerStats2);
					if ( !pAccepted2 ) __Test_XNet2_TLSSleepMs(10);
				}
			}
			if ( pAccepted2 ) {
				bResumeOpen = __Test_XNet2_TLSWaitMin(&tClientStats2.iOpenCount, 1, 3000)
					&& __Test_XNet2_TLSWaitMin(&tServerStats2.iOpenCount, 1, 3000);
			}
			if ( bResumeOpen && pClient2 && pClient2->pTls ) {
				bClientResumed = xrtNetTlsSessionWasResumed(pClient2->pTls);
			}
			if ( bResumeOpen && pAccepted2 && pAccepted2->pTls ) {
				bServerResumed = xrtNetTlsSessionWasResumed(pAccepted2->pTls);
			}
			if ( bResumeOpen ) {
				(void)xrtNetStreamSend(pClient2, "resume tls", 10);
				(void)__Test_XNet2_TLSWaitMin(&tServerStats2.iRecvCount, 1, 3000);
				(void)__Test_XNet2_TLSWaitMin(&tClientStats2.iRecvCount, 1, 3000);
			}
		}

		__Test_XNet2_TLSResult(&bAllPass, "TLS resume warm full handshake", bWarmOpen);
		__Test_XNet2_TLSResult(&bAllPass, "TLS resume export", bResumeExported);
		__Test_XNet2_TLSResult(&bAllPass, "TLS resumed reconnect open", bResumeOpen);
		__Test_XNet2_TLSResult(&bAllPass, "TLS resumed client state", bClientResumed);
		__Test_XNet2_TLSResult(&bAllPass, "TLS resumed server state", bServerResumed);

		if ( pClient2 ) xrtNetStreamClose(pClient2, XNET_CLOSE_F_GRACEFUL);
		(void)__Test_XNet2_TLSWaitMin(&tClientStats2.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitMin(&tServerStats2.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pClient2, 3000);
		(void)__Test_XNet2_TLSWaitStreamQuiet(pAccepted2, 3000);
		if ( pAccepted2 ) xrtNetStreamDestroy(pAccepted2);
		if ( pClient2 ) xrtNetStreamDestroy(pClient2);
		if ( pResume ) xrtNetTlsResumeDestroy(pResume);
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		if ( pEngine ) {
			__Test_XNet2_TLSSleepMs(20);
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}

	return bAllPass ? 0 : 1;
}
