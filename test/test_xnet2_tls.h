#include "../lib/xnet_stream.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


typedef struct {
	volatile long iOpenCount;
	volatile long iRecvCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iLastRecvLen;
	char aLastRecv[128];
	bool bEchoBack;
} __test_xnet2_tls_stats;


static void __Test_XNet2_TLSSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_TLSAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XNet2_TLSAtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static void __Test_XNet2_TLSAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static bool __Test_XNet2_TLSWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XNet2_TLSAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XNet2_TLSSleepMs(10);
	}
	return __Test_XNet2_TLSAtomicLoad(pValue) >= iExpectMin;
}

static bool __Test_XNet2_TLSFileExists(const char* sPath)
{
	FILE* pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static void __Test_XNet2_TLSOnOpen(ptr pOwner, xnetstream* pStream)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iOpenCount);
}

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

static void __Test_XNet2_TLSOnClose(ptr pOwner, xnetstream* pStream, xnet_result iReason)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	(void)iReason;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iCloseCount);
}

static void __Test_XNet2_TLSOnError(ptr pOwner, xnetstream* pStream, int iSysErr)
{
	__test_xnet2_tls_stats* pStats = (__test_xnet2_tls_stats*)pOwner;
	(void)pStream;
	(void)iSysErr;
	if ( !pStats ) return;
	__Test_XNet2_TLSAtomicInc(&pStats->iErrorCount);
}


void Test_XNet2_TLS(void)
{
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
			return;
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
		printf("  Engine create : %s\n", pEngine ? "PASS" : "FAIL");
		if ( !pEngine ) return;

		printf("  Engine start : %s\n", xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		pListener = xrtNetListenerCreate(pEngine, &tListenCfg, NULL, &tEvents, NULL);
		printf("  Listener create : %s\n", pListener ? "PASS" : "FAIL");
		printf("  Listener start : %s\n", pListener && xrtNetListenerStart(pListener) == XRT_NET_OK ? "PASS" : "FAIL");

		tConnCfg.iPort = pListener->tConfig.tBindAddr.iPort;
		pClient = xrtNetStreamCreate(pEngine, &tEvents, &tClientStats);
		printf("  Client create : %s\n", pClient ? "PASS" : "FAIL");
		printf("  Client connect (TLS) : %s\n", pClient && xrtNetStreamConnect(pClient, &tConnCfg) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  TLS send before ready = AGAIN : %s\n",
			pClient && xrtNetStreamSend(pClient, "pre", 3) == XRT_NET_AGAIN ? "PASS" : "FAIL");

		for ( iAcceptRetry = 0; iAcceptRetry < 100 && !pAccepted; ++iAcceptRetry ) {
			pAccepted = __xnetListenerTryAcceptOne(pListener, &tServerStats);
			if ( !pAccepted ) __Test_XNet2_TLSSleepMs(10);
		}
		printf("  Listener accept TLS client : %s\n", pAccepted ? "PASS" : "FAIL");

		printf("  Client TLS open : %s\n", __Test_XNet2_TLSWaitMin(&tClientStats.iOpenCount, 1, 3000) ? "PASS" : "FAIL");
		printf("  Server TLS open : %s\n", __Test_XNet2_TLSWaitMin(&tServerStats.iOpenCount, 1, 3000) ? "PASS" : "FAIL");

		printf("  TLS client send : %s\n", pClient && xrtNetStreamSend(pClient, "hello tls", 9) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  TLS server recv : %s\n", __Test_XNet2_TLSWaitMin(&tServerStats.iRecvCount, 1, 3000) ? "PASS" : "FAIL");
		printf("  TLS client echo recv : %s\n", __Test_XNet2_TLSWaitMin(&tClientStats.iRecvCount, 1, 3000) ? "PASS" : "FAIL");
		printf("  TLS echo payload : %s\n", strcmp(tClientStats.aLastRecv, "hello tls") == 0 ? "PASS" : "FAIL");
		printf("  TLS error-free client path : %s\n", __Test_XNet2_TLSAtomicLoad(&tClientStats.iErrorCount) == 0 ? "PASS" : "FAIL");
		printf("  TLS error-free server path : %s\n", __Test_XNet2_TLSAtomicLoad(&tServerStats.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pClient ) xrtNetStreamClose(pClient, XNET_CLOSE_F_GRACEFUL);
		(void)__Test_XNet2_TLSWaitMin(&tClientStats.iCloseCount, 1, 3000);
		(void)__Test_XNet2_TLSWaitMin(&tServerStats.iCloseCount, 1, 3000);

		if ( pAccepted ) xrtNetStreamDestroy(pAccepted);
		if ( pClient ) xrtNetStreamDestroy(pClient);
		if ( pListener ) {
			xrtNetListenerStop(pListener);
			xrtNetListenerDestroy(pListener);
		}
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
}
