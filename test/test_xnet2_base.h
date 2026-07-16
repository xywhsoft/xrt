// XNET2基础测试
typedef struct {
	volatile long iRan;
	volatile long iDiscarded;
} __test_xnet_owned_timer_ctx;


static void __Test_XNetOwnedTimerTask(xnetworker* pWorker, ptr pArg)
{
	__test_xnet_owned_timer_ctx* pCtx = (__test_xnet_owned_timer_ctx*)pArg;
	(void)pWorker;
	if ( pCtx ) { (void)__xnetAtomicAddFetch32(&pCtx->iRan, 1); }
}


static void __Test_XNetOwnedTimerDiscard(ptr pArg)
{
	__test_xnet_owned_timer_ctx* pCtx = (__test_xnet_owned_timer_ctx*)pArg;
	if ( pCtx ) { (void)__xnetAtomicAddFetch32(&pCtx->iDiscarded, 1); }
}


void Test_XNet2_Base(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Base Address Test:\n\n");

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET, 8080);
		printf("  InitAny IPv4 family : %s\n", tAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  InitAny IPv4 port : %s\n", tAddr.iPort == 8080 ? "PASS" : "FAIL");
		printf("  InitAny IPv4 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "0.0.0.0") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET6, 9090);
		printf("  InitAny IPv6 family : %s\n", tAddr.iFamily == AF_INET6 ? "PASS" : "FAIL");
		printf("  InitAny IPv6 port : %s\n", tAddr.iPort == 9090 ? "PASS" : "FAIL");
		printf("  InitAny IPv6 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "::") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "127.0.0.1", 80);
		printf("  Parse IPv4 literal : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Parse IPv4 family : %s\n", tAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  Parse IPv4 port : %s\n", tAddr.iPort == 80 ? "PASS" : "FAIL");
		printf("  Parse IPv4 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "127.0.0.1") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "::1", 443);
		printf("  Parse IPv6 literal : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Parse IPv6 family : %s\n", tAddr.iFamily == AF_INET6 ? "PASS" : "FAIL");
		printf("  Parse IPv6 port : %s\n", tAddr.iPort == 443 ? "PASS" : "FAIL");
		printf("  Parse IPv6 text : %s\n", strcmp(xrtNetAddrToStr(&tAddr), "::1") == 0 ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xnet_result iRet = xrtNetAddrParse(&tAddr, "not-an-ip", 0);
		printf("  Reject invalid literal : %s\n", iRet == XRT_NET_ERROR ? "PASS" : "FAIL");
	}

	{
		xnetaddr tAddr;
		xrtNetAddrInitAny(&tAddr, AF_INET, 1234);
		xnet_result iRet = xrtNetResolve("localhost", &tAddr);
		printf("  Resolve localhost : %s\n", iRet == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Resolve localhost family : %s\n",
			iRet == XRT_NET_OK && (tAddr.iFamily == AF_INET || tAddr.iFamily == AF_INET6) ? "PASS" : "FAIL");
		printf("  Resolve localhost preserves port : %s\n",
			iRet == XRT_NET_OK && tAddr.iPort == 1234 ? "PASS" : "FAIL");
		printf("  Resolve localhost text non-empty : %s\n",
			iRet == XRT_NET_OK && xrtNetAddrToStr(&tAddr)[0] != '\0' ? "PASS" : "FAIL");
	}

	{
		xnetlistenconfig tListenCfg;
		xnetdgramconfig tDgramCfg;
		xrtNetListenConfigInit(&tListenCfg);
		xrtNetDgramConfigInit(&tDgramCfg);
		printf("  Listen config bind family : %s\n", tListenCfg.tBindAddr.iFamily == AF_INET ? "PASS" : "FAIL");
		printf("  Dgram config bind family : %s\n", tDgramCfg.tBindAddr.iFamily == AF_INET ? "PASS" : "FAIL");
	}

	{
		typedef struct {
			xnetconnectconfig tConfig;
			uint64 iFutureField;
		} __test_xnet_future_connect_config;
		xnetconnectconfig tV1;
		xnetconnectconfig tCopied;
		__test_xnet_future_connect_config tFuture;
		bool bCopied;

		xrtNetConnectConfigInit(&tV1);
		xrtNetConnectConfigInit(&tCopied);
		tV1.iSize = XNET_CONNECT_CONFIG_V1_SIZE;
		tV1.iMaxQueuedBytes = 777777u;
		tV1.iFallbackDelayMs = 999u;
		bCopied = __xnetConfigCopyKnown(&tCopied, sizeof(tCopied), &tV1,
			tV1.iSize, tV1.iVersion, XNET_CONNECT_CONFIG_V1_SIZE);
		printf("  Config V1 prefix accepted : %s\n", bCopied ? "PASS" : "FAIL");
		printf("  Config V1 known field copied : %s\n",
			tCopied.iMaxQueuedBytes == 777777u ? "PASS" : "FAIL");
		printf("  Config V1 appended field keeps default : %s\n",
			tCopied.iFallbackDelayMs == 250u ? "PASS" : "FAIL");

		memset(&tFuture, 0, sizeof(tFuture));
		xrtNetConnectConfigInit(&tFuture.tConfig);
		tFuture.tConfig.iSize = (uint32)sizeof(tFuture);
		tFuture.tConfig.iFallbackDelayMs = 123u;
		tFuture.iFutureField = UINT64_C(0x1122334455667788);
		xrtNetConnectConfigInit(&tCopied);
		bCopied = __xnetConfigCopyKnown(&tCopied, sizeof(tCopied), &tFuture,
			tFuture.tConfig.iSize, tFuture.tConfig.iVersion, XNET_CONNECT_CONFIG_V1_SIZE);
		printf("  Future oversized config accepted : %s\n", bCopied ? "PASS" : "FAIL");
		printf("  Future oversized config copies known prefix : %s\n",
			tCopied.iFallbackDelayMs == 123u ? "PASS" : "FAIL");
		printf("  Undersized config rejected : %s\n",
			!__xnetConfigCopyKnown(&tCopied, sizeof(tCopied), &tV1,
				XNET_CONNECT_CONFIG_V1_SIZE - 1u, tV1.iVersion,
				XNET_CONNECT_CONFIG_V1_SIZE) ? "PASS" : "FAIL");
		printf("  Unknown config version rejected : %s\n",
			!__xnetConfigCopyKnown(&tCopied, sizeof(tCopied), &tV1,
				tV1.iSize, tV1.iVersion + 1u, XNET_CONNECT_CONFIG_V1_SIZE) ? "PASS" : "FAIL");
	}

	{
		xnetengineconfig tConfig;
		xnetengine* pEngine;
		__test_xnet_owned_timer_ctx tCancelled;
		__test_xnet_owned_timer_ctx tStopped;
		uint64 iTimerId = 0u;
		bool bCancelledDiscarded = false;

		memset(&tCancelled, 0, sizeof(tCancelled));
		memset(&tStopped, 0, sizeof(tStopped));
		xrtNetEngineConfigInit(&tConfig);
		tConfig.iWorkerCount = 1u;
		pEngine = xrtNetEngineCreate(&tConfig);
		printf("  Owned timer engine create : %s\n", pEngine ? "PASS" : "FAIL");
		printf("  Owned timer engine start : %s\n",
			pEngine && xrtNetEngineStart(pEngine) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Owned timer schedule for cancel : %s\n",
			pEngine && __xnetEngineScheduleTimerOwned(pEngine, 0u, 60000u,
				__Test_XNetOwnedTimerTask, __Test_XNetOwnedTimerDiscard,
				&tCancelled, &iTimerId) == XRT_NET_OK ? "PASS" : "FAIL");
		printf("  Owned timer cancel request : %s\n",
			pEngine && iTimerId != 0u && xrtNetEngineCancelTimer(pEngine, iTimerId) == XRT_NET_OK ? "PASS" : "FAIL");
		for ( uint32 i = 0u; i < 100u; ++i ) {
			if ( __xnetAtomicLoad32(&tCancelled.iDiscarded) == 1 ) {
				bCancelledDiscarded = true;
				break;
			}
			__xnetEngineSleepMs(2u);
		}
		printf("  Owned timer cancel discards argument : %s\n",
			bCancelledDiscarded && __xnetAtomicLoad32(&tCancelled.iRan) == 0 ? "PASS" : "FAIL");
		printf("  Owned timer schedule for stop : %s\n",
			pEngine && __xnetEngineScheduleTimerOwned(pEngine, 0u, 60000u,
				__Test_XNetOwnedTimerTask, __Test_XNetOwnedTimerDiscard,
				&tStopped, NULL) == XRT_NET_OK ? "PASS" : "FAIL");
		if ( pEngine ) { xrtNetEngineStop(pEngine); }
		printf("  Owned timer engine stop discards argument : %s\n",
			__xnetAtomicLoad32(&tStopped.iDiscarded) == 1 &&
			__xnetAtomicLoad32(&tStopped.iRan) == 0 ? "PASS" : "FAIL");
		if ( pEngine ) { xrtNetEngineDestroy(pEngine); }
	}
}
