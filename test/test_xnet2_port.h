#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <unistd.h>
#endif


static void __Test_XNet2_SleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}


void Test_XNet2_Port(void)
{
	printf("\n\n\n------------------------------------\n\n XNet2 Port Skeleton Test:\n\n");

	{
		xnetportconfig tCfg;
		xrtNetPortConfigInit(&tCfg);
		printf("  Port cfg backend auto : %s\n", tCfg.iBackend == XNET_PORT_BACKEND_AUTO ? "PASS" : "FAIL");
		printf("  Port cfg CQ entries : %s\n", tCfg.iCqEntries == 8192 ? "PASS" : "FAIL");
		printf("  Port cfg event batch : %s\n", tCfg.iEventBatch == 256 ? "PASS" : "FAIL");
	}

	#if defined(_WIN32) || defined(_WIN64)
	{
		xnetport tPort;
		xnetportconfig tCfg;
		xnetportevent arrEvents[4];
		const xnetportops* pOps = xrtNetPortIOCPOps();
		uint32 iCount = 0;

		xrtNetPortConfigInit(&tCfg);
		printf("  IOCP ops available : %s\n", pOps != NULL ? "PASS" : "FAIL");
		printf("  IOCP init : %s\n", pOps && xrtNetPortInit(&tPort, pOps, &tCfg, NULL) == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pOps && tPort.pCtx ) {
			xnetportsubmit tSubmit;
			memset(&tSubmit, 0, sizeof(tSubmit));
			tSubmit.iOpType = XNET_PORT_OP_RECV;
			printf("  IOCP submit skeleton : %s\n", xrtNetPortSubmit(&tPort, &tSubmit, 1) == XRT_NET_OK ? "PASS" : "FAIL");
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  IOCP submit harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_RECV ? "PASS" : "FAIL");

			printf("  IOCP wake post : %s\n", xrtNetPortWake(&tPort) == XRT_NET_OK ? "PASS" : "FAIL");
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  IOCP wake harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_WAKE ? "PASS" : "FAIL");

			printf("  IOCP arm timer : %s\n", xrtNetPortArmTimer(&tPort, 1001, 10) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_SleepMs(20);
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  IOCP timer harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_TIMER && arrEvents[0].iOpId == 1001 ? "PASS" : "FAIL");

			printf("  IOCP cancel timer : %s\n", xrtNetPortArmTimer(&tPort, 1002, 50) == XRT_NET_OK &&
				xrtNetPortCancelTimer(&tPort, 1002) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_SleepMs(60);
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  IOCP cancelled timer silent : %s\n", iCount == 0 ? "PASS" : "FAIL");

			xrtNetPortUnit(&tPort);
		}
	}

	{
		const xnetportops* pOps = xrtNetPortUringOps();
		printf("  io_uring ops absent on Windows : %s\n", pOps == NULL ? "PASS" : "FAIL");
	}
	#elif defined(__linux__)
	{
		xnetport tPort;
		xnetportconfig tCfg;
		xnetportevent arrEvents[4];
		const xnetportops* pOps = xrtNetPortUringOps();
		uint32 iCount = 0;

		xrtNetPortConfigInit(&tCfg);
		printf("  io_uring ops available : %s\n", pOps != NULL ? "PASS" : "FAIL");
		printf("  io_uring init : %s\n", pOps && xrtNetPortInit(&tPort, pOps, &tCfg, NULL) == XRT_NET_OK ? "PASS" : "FAIL");

		if ( pOps && tPort.pCtx ) {
			xnetportsubmit tSubmit;
			memset(&tSubmit, 0, sizeof(tSubmit));
			tSubmit.iOpType = XNET_PORT_OP_RECV;
			printf("  io_uring submit skeleton : %s\n", xrtNetPortSubmit(&tPort, &tSubmit, 1) == XRT_NET_OK ? "PASS" : "FAIL");
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  io_uring submit harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_RECV ? "PASS" : "FAIL");

			printf("  io_uring wake post : %s\n", xrtNetPortWake(&tPort) == XRT_NET_OK ? "PASS" : "FAIL");
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 50);
			printf("  io_uring wake harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_WAKE ? "PASS" : "FAIL");

			printf("  io_uring arm timer : %s\n", xrtNetPortArmTimer(&tPort, 2001, 10) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_SleepMs(20);
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  io_uring timer harvest : %s\n",
				iCount >= 1 && arrEvents[0].iType == XNET_PORT_EVENT_TIMER && arrEvents[0].iOpId == 2001 ? "PASS" : "FAIL");

			printf("  io_uring cancel timer : %s\n", xrtNetPortArmTimer(&tPort, 2002, 50) == XRT_NET_OK &&
				xrtNetPortCancelTimer(&tPort, 2002) == XRT_NET_OK ? "PASS" : "FAIL");
			__Test_XNet2_SleepMs(60);
			memset(arrEvents, 0, sizeof(arrEvents));
			iCount = xrtNetPortHarvest(&tPort, arrEvents, 4, 0);
			printf("  io_uring cancelled timer silent : %s\n", iCount == 0 ? "PASS" : "FAIL");

			xrtNetPortUnit(&tPort);
		}
	}

	{
		const xnetportops* pOps = xrtNetPortIOCPOps();
		printf("  IOCP ops absent on Linux : %s\n", pOps == NULL ? "PASS" : "FAIL");
	}
	#else
	{
		printf("  No native port smoke test on this platform : PASS\n");
	}
	#endif
}
