#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


typedef struct {
	volatile long iRecvCount;
	volatile long iRecvTotalBytes;
	volatile long iLastRecvBytes;
	volatile long iLastFromPort;
	volatile long iErrorCount;
	bool bEchoBack;
	char aLastRecvText[64];
} __test_xnet2_dgram_stats;

static void __Test_XNet2_DgramSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_DgramAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long __Test_XNet2_DgramAtomicAdd(volatile long* pValue, long iDelta)
{
	#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		long iPrev;
		long iNext;
		do {
			iPrev = (long)InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
			iNext = iPrev + iDelta;
		} while ( (long)InterlockedCompareExchange((volatile LONG*)pValue, (LONG)iNext, (LONG)iPrev) != iPrev );
		return iNext;
	#elif defined(_WIN32) || defined(_WIN64)
		return InterlockedAdd((volatile LONG*)pValue, iDelta);
	#else
		return __sync_add_and_fetch(pValue, iDelta);
	#endif
}

static long __Test_XNet2_DgramAtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static void __Test_XNet2_DgramAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static bool __Test_XNet2_DgramWaitMin(volatile long* pValue, long iExpectMin, uint32 iTimeoutMs)
{
	uint32 iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32 i = 0; i < iLoops; ++i ) {
		if ( __Test_XNet2_DgramAtomicLoad(pValue) >= iExpectMin ) return true;
		__Test_XNet2_DgramSleepMs(10);
	}
	return __Test_XNet2_DgramAtomicLoad(pValue) >= iExpectMin;
}

static void __Test_XNet2_DgramOnRecv(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain)
{
	__test_xnet2_dgram_stats* pStats = (__test_xnet2_dgram_stats*)pOwner;
	char aBuf[64];
	size_t iBytes = xrtNetChainBytes(pChain);
	if ( !pStats || !pFrom || !pChain ) return;
	memset(aBuf, 0, sizeof(aBuf));
	memset(pStats->aLastRecvText, 0, sizeof(pStats->aLastRecvText));
	if ( iBytes > 0 ) {
		size_t iCopy = iBytes < (sizeof(aBuf) - 1u) ? iBytes : (sizeof(aBuf) - 1u);
		(void)xrtNetChainPeek(pChain, aBuf, iCopy);
		memcpy(pStats->aLastRecvText, aBuf, iCopy);
	}
	__Test_XNet2_DgramAtomicInc(&pStats->iRecvCount);
	__Test_XNet2_DgramAtomicAdd(&pStats->iRecvTotalBytes, (long)iBytes);
	__Test_XNet2_DgramAtomicStore(&pStats->iLastRecvBytes, (long)iBytes);
	__Test_XNet2_DgramAtomicStore(&pStats->iLastFromPort, (long)pFrom->iPort);
	if ( pStats->bEchoBack ) {
		(void)xrtNetDgramSendTo(pSock, pFrom, aBuf, iBytes);
	}
}

static void __Test_XNet2_DgramOnError(ptr pOwner, xdgramsock* pSock, int iSysErr)
{
	__test_xnet2_dgram_stats* pStats = (__test_xnet2_dgram_stats*)pOwner;
	(void)pSock;
	(void)iSysErr;
	if ( !pStats ) return;
	__Test_XNet2_DgramAtomicInc(&pStats->iErrorCount);
}


void Test_XNet2_Dgram(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Datagram Test:\n\n");

	{
		xnetengineconfig tCfg;
		xnetdgramconfig tServerCfg;
		xnetdgramconfig tClientCfg;
		xnetdgramevents tEvents;
		xnetengine* pEngine;
		xdgramsock* pServer;
		xdgramsock* pClient;
		xnetaddr tServerPeerAddr;
		__test_xnet2_dgram_stats tServerStats;
		__test_xnet2_dgram_stats tClientStats;
		xnetspan arrVec[2];
		long iServerBaseRecv = 0;

		memset(&tServerStats, 0, sizeof(tServerStats));
		memset(&tClientStats, 0, sizeof(tClientStats));
		memset(&tEvents, 0, sizeof(tEvents));
		memset(&tServerPeerAddr, 0, sizeof(tServerPeerAddr));

		tEvents.OnRecv = __Test_XNet2_DgramOnRecv;
		tEvents.OnError = __Test_XNet2_DgramOnError;

		xrtNetEngineConfigInit(&tCfg);
		tCfg.iWorkerCount = 1;
		pEngine = xrtNetEngineCreate(&tCfg);
		printf("  Dgram engine create : %s\n", pEngine != NULL ? "PASS" : "FAIL");
		printf("  Dgram engine start : %s\n", pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");

		xrtNetDgramConfigInit(&tServerCfg);
		xrtNetAddrInitAny(&tServerCfg.tBindAddr, AF_INET, 0);
		tServerCfg.iRecvBatch = 32;
		pServer = pEngine ? xrtNetDgramCreate(pEngine, &tServerCfg, &tEvents, &tServerStats) : NULL;
		printf("  Dgram server create : %s\n", pServer != NULL ? "PASS" : "FAIL");
		printf("  Dgram server start : %s\n", pServer && xrtNetDgramStart(pServer) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram server bound port assigned : %s\n", pServer && pServer->tLocalAddr.iPort != 0 ? "PASS" : "FAIL");

		xrtNetDgramConfigInit(&tClientCfg);
		xrtNetAddrInitAny(&tClientCfg.tBindAddr, AF_INET, 0);
		tClientCfg.iRecvBatch = 32;
		pClient = pEngine ? xrtNetDgramCreate(pEngine, &tClientCfg, &tEvents, &tClientStats) : NULL;
		printf("  Dgram client create : %s\n", pClient != NULL ? "PASS" : "FAIL");
		printf("  Dgram client start : %s\n", pClient && xrtNetDgramStart(pClient) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram client bound port assigned : %s\n", pClient && pClient->tLocalAddr.iPort != 0 ? "PASS" : "FAIL");
		if ( pServer ) (void)xrtNetAddrParse(&tServerPeerAddr, "127.0.0.1", pServer->tLocalAddr.iPort);

		if ( pServer ) tServerStats.bEchoBack = true;
		printf("  Dgram send-to server : %s\n",
			pClient && pServer && xrtNetDgramSendTo(pClient, &tServerPeerAddr, "ping", 4) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram server recv fires : %s\n", __Test_XNet2_DgramWaitMin(&tServerStats.iRecvCount, 1, 300) ? "PASS" : "FAIL");
		printf("  Dgram server sees client port : %s\n",
			pClient && __Test_XNet2_DgramAtomicLoad(&tServerStats.iLastFromPort) == (long)pClient->tLocalAddr.iPort ? "PASS" : "FAIL");
		printf("  Dgram server sees ping payload : %s\n", strcmp(tServerStats.aLastRecvText, "ping") == 0 ? "PASS" : "FAIL");
		printf("  Dgram client gets echo : %s\n", __Test_XNet2_DgramWaitMin(&tClientStats.iRecvCount, 1, 300) ? "PASS" : "FAIL");
		printf("  Dgram client sees echoed ping : %s\n", strcmp(tClientStats.aLastRecvText, "ping") == 0 ? "PASS" : "FAIL");

		tServerStats.bEchoBack = false;
		arrVec[0].pData = "bu";
		arrVec[0].iLen = 2;
		arrVec[1].pData = "rst";
		arrVec[1].iLen = 3;
		iServerBaseRecv = __Test_XNet2_DgramAtomicLoad(&tServerStats.iRecvCount);
		printf("  Dgram send-vec to server : %s\n",
			pClient && pServer && xrtNetDgramSendVecTo(pClient, &tServerPeerAddr, arrVec, 2) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Dgram send-vec arrives : %s\n",
			__Test_XNet2_DgramWaitMin(&tServerStats.iRecvCount, iServerBaseRecv + 1, 300) ? "PASS" : "FAIL");
		printf("  Dgram send-vec payload merged : %s\n", strcmp(tServerStats.aLastRecvText, "burst") == 0 ? "PASS" : "FAIL");

		iServerBaseRecv = __Test_XNet2_DgramAtomicLoad(&tServerStats.iRecvCount);
		for ( int i = 0; i < 16; ++i ) {
			(void)xrtNetDgramSendTo(pClient, &tServerPeerAddr, "b", 1);
		}
		printf("  Dgram burst receives all packets : %s\n",
			__Test_XNet2_DgramWaitMin(&tServerStats.iRecvCount, iServerBaseRecv + 16, 500) ? "PASS" : "FAIL");
		printf("  Dgram burst stays error free : %s\n",
			__Test_XNet2_DgramAtomicLoad(&tServerStats.iErrorCount) == 0 &&
			__Test_XNet2_DgramAtomicLoad(&tClientStats.iErrorCount) == 0 ? "PASS" : "FAIL");

		if ( pClient ) {
			xrtNetDgramStop(pClient);
			xrtNetDgramDestroy(pClient);
		}
		if ( pServer ) {
			xrtNetDgramStop(pServer);
			xrtNetDgramDestroy(pServer);
		}
		if ( pEngine ) {
			xrtNetEngineStop(pEngine);
			xrtNetEngineDestroy(pEngine);
		}
	}
}
