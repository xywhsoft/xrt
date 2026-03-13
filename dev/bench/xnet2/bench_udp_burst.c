#include "bench_common.h"
#include "../../../lib/xnet_dgram.h"

typedef struct {
	volatile long iPacketCount;
	volatile long iByteCount;
	volatile long iErrorCount;
} __bench_udp_ctx;

static void __benchUdpOnRecv(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain)
{
	__bench_udp_ctx* pCtx = (__bench_udp_ctx*)pOwner;
	size_t iBytes = xrtNetChainBytes(pChain);
	(void)pSock;
	(void)pFrom;
	if ( !pCtx || !pChain ) return;
	xbenchAtomicInc(&pCtx->iPacketCount);
	xbenchAtomicAdd(&pCtx->iByteCount, (long)iBytes);
	xrtNetChainConsume(pChain, iBytes);
}

static void __benchUdpOnError(ptr pOwner, xdgramsock* pSock, int iSysErr)
{
	__bench_udp_ctx* pCtx = (__bench_udp_ctx*)pOwner;
	(void)pSock;
	(void)iSysErr;
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

int main(int argc, char** argv)
{
	uint32_t iPacketCount = xbenchArgU32(argc, argv, 1, 20000u);
	uint32_t iPacketSize = xbenchArgU32(argc, argv, 2, 256u);
	xnetengineconfig tEngineCfg;
	xnetdgramconfig tServerCfg;
	xnetdgramconfig tClientCfg;
	xnetdgramevents tEvents;
	__bench_udp_ctx tServerCtx;
	xnetengine* pServerEngine = NULL;
	xnetengine* pClientEngine = NULL;
	xdgramsock* pServerSock = NULL;
	xdgramsock* pClientSock = NULL;
	xnetaddr tTarget;
	char* pPayload = NULL;
	xbenchtimer tSendTimer;
	xbenchtimer tDrainTimer;
	uint64_t iSendNs;
	uint64_t iDrainNs;

	printf("xnet2 bench_udp_burst\n");
	printf("packets=%u packet_size=%u\n", (unsigned)iPacketCount, (unsigned)iPacketSize);

	if ( !xbenchNetInit() ) return 1;
	memset(&tServerCtx, 0, sizeof(tServerCtx));
	memset(&tEvents, 0, sizeof(tEvents));
	tEvents.OnRecv = __benchUdpOnRecv;
	tEvents.OnError = __benchUdpOnError;

	pPayload = (char*)malloc(iPacketSize);
	if ( !pPayload ) goto cleanup;
	memset(pPayload, 'U', iPacketSize);

	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;
	pServerEngine = xrtNetEngineCreate(&tEngineCfg);
	pClientEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( !pServerEngine || !pClientEngine ) goto cleanup;
	if ( xrtNetEngineStart(pServerEngine) != XRT_NET_OK ) goto cleanup;
	if ( xrtNetEngineStart(pClientEngine) != XRT_NET_OK ) goto cleanup;

	xrtNetDgramConfigInit(&tServerCfg);
	(void)xrtNetAddrParse(&tServerCfg.tBindAddr, "127.0.0.1", 0);
	pServerSock = xrtNetDgramCreate(pServerEngine, &tServerCfg, &tEvents, &tServerCtx);
	if ( !pServerSock ) goto cleanup;
	if ( xrtNetDgramStart(pServerSock) != XRT_NET_OK ) goto cleanup;

	xrtNetDgramConfigInit(&tClientCfg);
	(void)xrtNetAddrParse(&tClientCfg.tBindAddr, "127.0.0.1", 0);
	pClientSock = xrtNetDgramCreate(pClientEngine, &tClientCfg, NULL, NULL);
	if ( !pClientSock ) goto cleanup;
	if ( xrtNetDgramStart(pClientSock) != XRT_NET_OK ) goto cleanup;

	tTarget = pServerSock->tLocalAddr;
	xbenchTimerStart(&tSendTimer);
	for ( uint32_t i = 0; i < iPacketCount; ++i ) {
		if ( xrtNetDgramSendTo(pClientSock, &tTarget, pPayload, iPacketSize) != XRT_NET_OK ) break;
	}
	xbenchTimerStop(&tSendTimer);
	iSendNs = xbenchTimerElapsedNs(&tSendTimer);

	xbenchTimerStart(&tDrainTimer);
	(void)xbenchWaitMin(&tServerCtx.iPacketCount, (long)iPacketCount, 10000u);
	xbenchTimerStop(&tDrainTimer);
	iDrainNs = xbenchTimerElapsedNs(&tDrainTimer);

	xbenchPrintMetricU64("send_elapsed_ns", iSendNs);
	xbenchPrintMetricDouble("send_packets_per_sec", xbenchSafeRate(iPacketCount, iSendNs));
	xbenchPrintMetricDouble("send_bytes_per_sec", xbenchSafeRate((uint64_t)iPacketCount * iPacketSize, iSendNs));
	xbenchPrintMetricU64("drain_elapsed_ns", iDrainNs);
	xbenchPrintMetricDouble("recv_packets_per_sec", xbenchSafeRate((uint64_t)xbenchAtomicLoad(&tServerCtx.iPacketCount), iDrainNs));
	printf("recv_packets=%ld recv_bytes=%ld errors=%ld\n",
		xbenchAtomicLoad(&tServerCtx.iPacketCount),
		xbenchAtomicLoad(&tServerCtx.iByteCount),
		xbenchAtomicLoad(&tServerCtx.iErrorCount));

cleanup:
	if ( pClientSock ) {
		xrtNetDgramStop(pClientSock);
		xrtNetDgramDestroy(pClientSock);
	}
	if ( pServerSock ) {
		xrtNetDgramStop(pServerSock);
		xrtNetDgramDestroy(pServerSock);
	}
	if ( pClientEngine ) {
		xrtNetEngineStop(pClientEngine);
		xrtNetEngineDestroy(pClientEngine);
	}
	if ( pServerEngine ) {
		xrtNetEngineStop(pServerEngine);
		xrtNetEngineDestroy(pServerEngine);
	}
	free(pPayload);
	xbenchNetUnit();
	return 0;
}
