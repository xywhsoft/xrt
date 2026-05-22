#include "bench_v1_common.h"

typedef struct {
	volatile long iPacketCount;
	volatile long iByteCount;
	volatile long iErrorCount;
} __bench_v1_udp_ctx;

static void __benchV1UdpOnRecv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__bench_v1_udp_ctx* pCtx;
	(void)pConn;
	(void)pData;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_udp_ctx*)xrtUdpServerGetUserData((xudpserver*)pOwner);
	if ( !pCtx ) return;
	xbenchAtomicInc(&pCtx->iPacketCount);
	xbenchAtomicAdd(&pCtx->iByteCount, (long)iLen);
}

static void __benchV1UdpOnRecvFrom(ptr pOwner, xnetconn* pConn, const xnetaddr* pAddr, const char* pData, size_t iLen)
{
	__bench_v1_udp_ctx* pCtx;
	(void)pConn;
	(void)pAddr;
	(void)pData;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_udp_ctx*)xrtUdpServerGetUserData((xudpserver*)pOwner);
	if ( !pCtx ) return;
	xbenchAtomicInc(&pCtx->iPacketCount);
	xbenchAtomicAdd(&pCtx->iByteCount, (long)iLen);
}

static void __benchV1UdpOnError(ptr pOwner, xnetconn* pConn, int iErrorCode)
{
	__bench_v1_udp_ctx* pCtx = NULL;
	(void)pConn;
	(void)iErrorCode;
	if ( pOwner ) pCtx = (__bench_v1_udp_ctx*)xrtUdpServerGetUserData((xudpserver*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

int main(int argc, char** argv)
{
	uint32_t iPacketCount = xbenchArgU32(argc, argv, 1, 20000u);
	uint32_t iPacketSize = xbenchArgU32(argc, argv, 2, 256u);
	uint16_t iPort = (uint16_t)xbenchArgU32(argc, argv, 3, 19183u);
	xnetconfig tCfg;
	xnetevents tEvents;
	__bench_v1_udp_ctx tServerCtx;
	xudpserver* pServer = NULL;
	xudpclient* pClient = NULL;
	xnetaddr tTarget;
	char* pPayload = NULL;
	xbenchtimer tSendTimer;
	xbenchtimer tDrainTimer;
	uint64_t iSendNs = 0u;
	uint64_t iDrainNs = 0u;
	int iExitCode = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	printf("xnet-v1 bench_udp_burst\n");
	printf("packets=%u packet_size=%u port=%u\n", (unsigned)iPacketCount, (unsigned)iPacketSize, (unsigned)iPort);

	if ( !xbenchNetInit() ) return 1;
	if ( !xrtInit() ) {
		xbenchNetUnit();
		return 1;
	}
	memset(&tServerCtx, 0, sizeof(tServerCtx));
	memset(&tEvents, 0, sizeof(tEvents));
	xbenchV1ConfigInit(&tCfg);
	tEvents.OnRecv = __benchV1UdpOnRecv;
	tEvents.OnRecvFrom = __benchV1UdpOnRecvFrom;
	tEvents.OnError = __benchV1UdpOnError;

	pPayload = (char*)malloc(iPacketSize);
	if ( !pPayload ) goto cleanup;
	memset(pPayload, 'U', iPacketSize);

	pServer = xrtUdpServerCreate("127.0.0.1", iPort, &tCfg, &tEvents);
	if ( !pServer ) goto cleanup;
	xrtUdpServerSetUserData(pServer, &tServerCtx);
	if ( xrtUdpServerStart(pServer) != XRT_NET_OK ) goto cleanup;

	pClient = xrtUdpClientCreate(&tCfg, NULL);
	if ( !pClient ) goto cleanup;
	if ( xrtUdpClientStart(pClient) != XRT_NET_OK ) goto cleanup;
	xbenchSleepMs(100u);

	xbenchV1AddrInit(&tTarget, "127.0.0.1", iPort);
	xbenchTimerStart(&tSendTimer);
	for ( uint32_t i = 0; i < iPacketCount; ++i ) {
		if ( xrtUdpClientSendTo(pClient, &tTarget, pPayload, iPacketSize) != XRT_NET_OK ) {
			iExitCode = 2;
			break;
		}
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
	if ( xbenchAtomicLoad(&tServerCtx.iErrorCount) != 0 || xbenchAtomicLoad(&tServerCtx.iPacketCount) != (long)iPacketCount ) {
		printf("invalid_udp_run: expected=%u recv_packets=%ld errors=%ld\n",
			(unsigned)iPacketCount,
			xbenchAtomicLoad(&tServerCtx.iPacketCount),
			xbenchAtomicLoad(&tServerCtx.iErrorCount));
		iExitCode = 2;
	}

cleanup:
	if ( pClient ) {
		xrtUdpClientStop(pClient);
		xrtUdpClientDestroy(pClient);
	}
	if ( pServer ) {
		xrtUdpServerStop(pServer);
		xrtUdpServerDestroy(pServer);
	}
	free(pPayload);
	xrtUnit();
	xbenchNetUnit();
	return iExitCode;
}
