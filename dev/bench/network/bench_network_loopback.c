#include "../bench_common.h"

#define XRT_MODULE_NET_TCP_SYNC
#define XRT_MODULE_NET_TCP_SERVER_SYNC
#define XRT_MODULE_NET_UDP_SYNC
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#define XRT_BENCH_NETWORK_DESTROY_HELPERS
#include "bench_network_common.h"



/* 在截止时间内接收并校验一段可能被 TCP 分片的完整字节序列。 */
static bool benchNetworkRecvExact(
	xnetstream* pStream,
	const uint8* pExpected,
	size_t iSize
)
{
	size_t iOffset = 0;

	while ( iOffset < iSize ) {
		xnetbytes* pBytes = xrtNetStreamRecv(
			pStream,
			iSize - iOffset,
			xrtDeadlineAfter(5000000u),
			NULL
		);
		xbytesview View;

		if ( pBytes == NULL ) {
			return false;
		}
		View = xrtNetBytesView(pBytes);
		if (
			(View.Size == 0) ||
			(View.Size > (iSize - iOffset)) ||
			(memcmp(View.Data, pExpected + iOffset, View.Size) != 0)
		) {
			xrtNetBytesDestroy(pBytes);
			return false;
		}
		iOffset += View.Size;
		xrtNetBytesDestroy(pBytes);
	}
	return true;
}



/* 测量同步 TCP facade 的回环连接建立、完整 ping-pong 和尾延迟。 */
static bool benchNetworkTcp(uint32 iIterations, size_t iMessageSize)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamstats ClientStats;
	xnetstreamstats ServerStats;
	xnetenginestats EngineStats;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pServer = NULL;
	xnetaddr Address;
	uint8* pPayload = NULL;
	uint64* pLatencies = NULL;
	xbenchtimer SetupTimer;
	xbenchtimer RunTimer;
	uint64 iSetupElapsed;
	uint64 iRunElapsed;
	uint64 iExpectedBytes;
	uint32 iFailedIteration = 0;
	cstr sStage = "allocate";
	bool bResult = false;
	bool bClean = true;

	memset(&ClientStats, 0, sizeof(ClientStats));
	memset(&ServerStats, 0, sizeof(ServerStats));
	memset(&EngineStats, 0, sizeof(EngineStats));
	pPayload = (uint8*)malloc(iMessageSize);
	pLatencies = (uint64*)calloc(iIterations, sizeof(uint64));
	if ( (pPayload == NULL) || (pLatencies == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iMessageSize; i++ ) {
		pPayload[i] = (uint8)((i * 31u) + 7u);
	}

	sStage = "engine";
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup;
	}
	if ( !xbenchPrintNetworkBackend(pEngine) ) {
		goto cleanup;
	}
	sStage = "listen";
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Stream.NoDelay = true;
	if ( !xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		goto cleanup;
	}
	pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	if (
		(pListener == NULL) ||
		!xrtNetListenerLocal(pListener, &Address)
	) {
		goto cleanup;
	}

	sStage = "connect";
	xbenchTimerStart(&SetupTimer);
	pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		&ListenConfig.Stream,
		NULL,
		NULL
	);
	if ( pClient == NULL ) {
		goto cleanup;
	}
	sStage = "accept";
	pServer = xrtNetListenerAcceptWait(
		pListener,
		xrtDeadlineAfter(5000000u),
		NULL
	);
	if ( pServer == NULL ) {
		goto cleanup;
	}
	sStage = "open";
	if (
		!xrtNetStreamWait(
			pClient,
			XNET_STREAM_WAIT_OPEN,
			xrtDeadlineAfter(5000000u),
			NULL
		)
	) {
		goto cleanup;
	}
	xbenchTimerStop(&SetupTimer);
	iSetupElapsed = xbenchTimerElapsedNs(&SetupTimer);

	xbenchTimerStart(&RunTimer);
	for ( uint32 i = 0; i < iIterations; i++ ) {
		uint64 iStart = xbenchNowNs();

		iFailedIteration = i;
		sStage = "client-send";
		if ( xrtNetStreamSend(
			pClient,
			pPayload,
			iMessageSize
		) != XNET_RESULT_OK ) {
			goto cleanup;
		}
		sStage = "server-receive";
		if ( !benchNetworkRecvExact(pServer, pPayload, iMessageSize) ) {
			goto cleanup;
		}
		sStage = "server-send";
		if ( xrtNetStreamSend(
			pServer,
			pPayload,
			iMessageSize
		) != XNET_RESULT_OK ) {
			goto cleanup;
		}
		sStage = "client-receive";
		if ( !benchNetworkRecvExact(pClient, pPayload, iMessageSize) ) {
			goto cleanup;
		}
		pLatencies[i] = xbenchNowNs() - iStart;
	}
	xbenchTimerStop(&RunTimer);
	iRunElapsed = xbenchTimerElapsedNs(&RunTimer);
	iExpectedBytes = ((uint64)iIterations) * ((uint64)iMessageSize);
	sStage = "stats";
	if (
		!xrtNetStreamStats(pClient, &ClientStats) ||
		!xrtNetStreamStats(pServer, &ServerStats) ||
		(ClientStats.SentBytes != iExpectedBytes) ||
		(ClientStats.ReceivedBytes != iExpectedBytes) ||
		(ServerStats.SentBytes != iExpectedBytes) ||
		(ServerStats.ReceivedBytes != iExpectedBytes) ||
		(ClientStats.SendRejected != 0) ||
		(ServerStats.SendRejected != 0) ||
		(ClientStats.BufferedBytes != 0) ||
		(ServerStats.BufferedBytes != 0)
	) {
		goto cleanup;
	}

	qsort(
		pLatencies,
		iIterations,
		sizeof(uint64),
		xbenchCompareU64
	);
	xbenchPrintMetricU64("tcp_setup_elapsed_ns", iSetupElapsed);
	xbenchPrintMetricU64("tcp_round_trips", iIterations);
	xbenchPrintMetricU64("tcp_message_size", iMessageSize);
	xbenchPrintMetricU64("tcp_elapsed_ns", iRunElapsed);
	xbenchPrintMetricDouble(
		"tcp_round_trips_per_sec",
		xbenchSafeRate(iIterations, iRunElapsed)
	);
	xbenchPrintMetricDouble(
		"tcp_bytes_per_sec",
		xbenchSafeRate(iExpectedBytes * 2u, iRunElapsed)
	);
	xbenchPrintMetricDouble(
		"tcp_latency_p50_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.50)
	);
	xbenchPrintMetricDouble(
		"tcp_latency_p95_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.95)
	);
	xbenchPrintMetricDouble(
		"tcp_latency_p99_us",
		xbenchPercentileUs(pLatencies, iIterations, 0.99)
	);
	sStage = "done";
	bResult = true;

cleanup:
	if ( !bResult ) {
		fprintf(
			stderr,
			"TCP benchmark failed at %s iteration=%" PRIu32,
			sStage,
			iFailedIteration
		);
		xbenchPrintCurrentError();
		fprintf(stderr, "\n");
	}
	bClean = xbenchNetworkStreamDestroy(pClient, !bResult) && bClean;
	bClean = xbenchNetworkStreamDestroy(pServer, !bResult) && bClean;
	bClean = xbenchNetworkListenerDestroy(pListener) && bClean;
	if ( pEngine != NULL ) {
		if (
			!xrtNetEngineStats(pEngine, &EngineStats) ||
			(EngineStats.LiveObjects != 0) ||
			(EngineStats.WaitErrors != 0)
		) {
			bClean = false;
		}
		bClean = xrtNetEngineDestroy(pEngine) && bClean;
	}
	free(pPayload);
	free(pLatencies);
	return bResult && bClean;
}



/* 正常关闭并释放 UDP 对象。 */
static bool benchNetworkUdpDestroy(xnetudp* pUdp)
{
	bool bResult = true;

	if ( pUdp == NULL ) {
		return true;
	}
	if ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		bResult = xrtNetUdpClose(pUdp);
		if (
			bResult &&
			!xrtNetUdpWait(
				pUdp,
				XNET_UDP_WAIT_CLOSE,
				xrtDeadlineAfter(5000000u),
				NULL
			)
		) {
			bResult = false;
		}
	}
	xrtNetUdpDestroy(pUdp);
	return bResult;
}



/* 校验并销毁一批拥有型 UDP 数据包。 */
static bool benchNetworkUdpBatch(
	xnetudpbatch* pBatch,
	const uint8* pPayload,
	size_t iSize,
	size_t* pCount
)
{
	size_t iCount;
	bool bResult = true;

	if ( (pBatch == NULL) || (pCount == NULL) ) {
		return false;
	}
	iCount = xrtNetUdpBatchCount(pBatch);
	for ( size_t i = 0; i < iCount; i++ ) {
		xnetudppacket* pPacket = xrtNetUdpBatchPacket(pBatch, i);

		if (
			(pPacket == NULL) ||
			(xrtNetUdpPacketSize(pPacket) != iSize) ||
			(memcmp(xrtNetUdpPacketData(pPacket), pPayload, iSize) != 0)
		) {
			bResult = false;
			break;
		}
	}
	*pCount = iCount;
	xrtNetUdpBatchDestroy(pBatch);
	return bResult && (iCount != 0);
}



/* 测量有界窗口内的 UDP 批量受理、内核传输、批量拉取和所有权回收。 */
static bool benchNetworkUdp(
	uint32 iPacketCount,
	size_t iPacketSize,
	size_t iWindow
)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetudpstats ClientStats;
	xnetudpstats ServerStats;
	xnetenginestats EngineStats;
	xnetdgramsend* pItems = NULL;
	xnetengine* pEngine = NULL;
	xnetudp* pServer = NULL;
	xnetudp* pClient = NULL;
	xnetaddr Address;
	uint8* pPayload = NULL;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iExpectedBytes;
	uint32 iCompleted = 0;
	bool bResult = false;
	bool bClean = true;
	cstr sStage = "allocate";

	memset(&ClientStats, 0, sizeof(ClientStats));
	memset(&ServerStats, 0, sizeof(ServerStats));
	memset(&EngineStats, 0, sizeof(EngineStats));
	pPayload = (uint8*)malloc(iPacketSize);
	pItems = (xnetdgramsend*)calloc(iWindow, sizeof(xnetdgramsend));
	if ( (pPayload == NULL) || (pItems == NULL) ) {
		goto cleanup;
	}
	for ( size_t i = 0; i < iPacketSize; i++ ) {
		pPayload[i] = (uint8)((i * 17u) + 11u);
	}
	for ( size_t i = 0; i < iWindow; i++ ) {
		pItems[i].Data = pPayload;
		pItems[i].Size = iPacketSize;
	}

	sStage = "engine";
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup;
	}
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveConcurrency = 4;
	UdpConfig.ReceiveBatch = (uint32)iWindow;
	UdpConfig.ReceiveQueueLimit = iWindow * 2u;
	UdpConfig.ReceiveQueueByteLimit = iWindow * iPacketSize * 2u;
	UdpConfig.SendLimit = iWindow * iPacketSize * 2u;
	UdpConfig.SendHighWater = (UdpConfig.SendLimit * 3u) / 4u;
	UdpConfig.SendLowWater = UdpConfig.SendLimit / 2u;
	UdpConfig.SendPacketLimit = iWindow * 2u;
	UdpConfig.SendConcurrency = 4;
	sStage = "address";
	if ( !xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0) ) {
		goto cleanup;
	}
	sStage = "bind";
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		goto cleanup;
	}
	sStage = "connect";
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	if (
		(pClient == NULL) ||
		!xrtNetUdpWait(
			pServer,
			XNET_UDP_WAIT_OPEN,
			xrtDeadlineAfter(5000000u),
			NULL
		) ||
		!xrtNetUdpWait(
			pClient,
			XNET_UDP_WAIT_OPEN,
			xrtDeadlineAfter(5000000u),
			NULL
		)
	) {
		goto cleanup;
	}

	xbenchTimerStart(&Timer);
	while ( iCompleted < iPacketCount ) {
		size_t iBatch = iWindow;
		size_t iAccepted = 0;
		size_t iReceived = 0;

		if ( iBatch > (size_t)(iPacketCount - iCompleted) ) {
			iBatch = (size_t)(iPacketCount - iCompleted);
		}
		sStage = "send";
		while ( iAccepted < iBatch ) {
			size_t iStep = 0;
			xnetresult Result = xrtNetUdpSendBatch(
				pClient,
				pItems + iAccepted,
				iBatch - iAccepted,
				&iStep
			);

			iAccepted += iStep;
			if ( Result == XNET_RESULT_OK ) {
				continue;
			}
			if (
				(Result != XNET_RESULT_AGAIN) ||
				!xrtNetUdpWritable(
					pClient,
					iPacketSize,
					xrtDeadlineAfter(5000000u),
					NULL
				)
			) {
				goto cleanup;
			}
		}
		sStage = "receive";
		while ( iReceived < iBatch ) {
			xnetudpbatch* pBatch = xrtNetUdpReceiveBatchWait(
				pServer,
				iBatch - iReceived,
				xrtDeadlineAfter(5000000u),
				NULL
			);
			size_t iStep = 0;

			if (
				!benchNetworkUdpBatch(
					pBatch,
					pPayload,
					iPacketSize,
					&iStep
				) ||
				(iStep > (iBatch - iReceived))
			) {
				goto cleanup;
			}
			iReceived += iStep;
		}
		iCompleted += (uint32)iBatch;
	}
	sStage = "drain";
	if ( !xrtNetUdpWait(
		pClient,
		XNET_UDP_WAIT_DRAIN,
		xrtDeadlineAfter(5000000u),
		NULL
	) ) {
		goto cleanup;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	iExpectedBytes = ((uint64)iPacketCount) * ((uint64)iPacketSize);
	sStage = "stats";
	if (
		!xrtNetUdpStats(pClient, &ClientStats) ||
		!xrtNetUdpStats(pServer, &ServerStats) ||
		(ClientStats.SentPackets != iPacketCount) ||
		(ClientStats.SentBytes != iExpectedBytes) ||
		(ServerStats.ReceivedPackets != iPacketCount) ||
		(ServerStats.ReceivedBytes != iExpectedBytes) ||
		(ClientStats.SendErrors != 0) ||
		(ServerStats.ReceiveErrors != 0) ||
		(ServerStats.DroppedNewest != 0) ||
		(ServerStats.DroppedOldest != 0) ||
		(ServerStats.Truncated != 0) ||
		(ServerStats.ReceiveQueued != 0) ||
		(ServerStats.ReceiveQueuedBytes != 0)
	) {
		goto cleanup;
	}

	xbenchPrintMetricU64("udp_packets", iPacketCount);
	xbenchPrintMetricU64("udp_packet_size", iPacketSize);
	xbenchPrintMetricU64("udp_window", iWindow);
	xbenchPrintMetricU64(
		"udp_backpressure_events",
		ClientStats.SendRejected
	);
	xbenchPrintMetricU64("udp_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"udp_packets_per_sec",
		xbenchSafeRate(iPacketCount, iElapsed)
	);
	xbenchPrintMetricDouble(
		"udp_bytes_per_sec",
		xbenchSafeRate(iExpectedBytes, iElapsed)
	);
	sStage = "done";
	bResult = true;

cleanup:
	if ( !bResult ) {
		fprintf(stderr, "UDP benchmark failed at %s", sStage);
		xbenchPrintCurrentError();
		fprintf(stderr, "\n");
		if ( pClient != NULL ) {
			(void)xrtNetUdpStats(pClient, &ClientStats);
		}
		if ( pServer != NULL ) {
			(void)xrtNetUdpStats(pServer, &ServerStats);
		}
		fprintf(
			stderr,
			"UDP stats sent=%" PRIu64 "/%" PRIu64
			" received=%" PRIu64 "/%" PRIu64
			" rejected=%" PRIu64 " send_errors=%" PRIu64
			" receive_errors=%" PRIu64 " newest=%" PRIu64
			" oldest=%" PRIu64 " truncated=%" PRIu64
			" queued=%zu queued_bytes=%zu completed=%" PRIu32 "\n",
			ClientStats.SentPackets,
			ClientStats.SentBytes,
			ServerStats.ReceivedPackets,
			ServerStats.ReceivedBytes,
			ClientStats.SendRejected,
			ClientStats.SendErrors,
			ServerStats.ReceiveErrors,
			ServerStats.DroppedNewest,
			ServerStats.DroppedOldest,
			ServerStats.Truncated,
			ServerStats.ReceiveQueued,
			ServerStats.ReceiveQueuedBytes,
			iCompleted
		);
	}
	bClean = benchNetworkUdpDestroy(pClient) && bClean;
	bClean = benchNetworkUdpDestroy(pServer) && bClean;
	if ( pEngine != NULL ) {
		if (
			!xrtNetEngineStats(pEngine, &EngineStats) ||
			(EngineStats.LiveObjects != 0) ||
			(EngineStats.WaitErrors != 0)
		) {
			bClean = false;
		}
		bClean = xrtNetEngineDestroy(pEngine) && bClean;
	}
	free(pItems);
	free(pPayload);
	return bResult && bClean;
}



/* 运行 TCP ping-pong 与 UDP batch 两条新网络标准库基准。 */
int main(int argc, char** argv)
{
	uint32 iTcpIterations = xbenchArgU32(argc, argv, 1, 5000u);
	uint32 iTcpMessageSize = xbenchArgU32(argc, argv, 2, 64u);
	uint32 iUdpPackets = xbenchArgU32(argc, argv, 3, 50000u);
	uint32 iUdpPacketSize = xbenchArgU32(argc, argv, 4, 256u);
	uint32 iUdpWindow = xbenchArgU32(argc, argv, 5, 64u);

	if (
		(iTcpIterations == 0) ||
		(iTcpMessageSize == 0) ||
		(iTcpMessageSize > 65536u) ||
		(iUdpPackets == 0) ||
		(iUdpPacketSize == 0) ||
		(iUdpPacketSize > 65507u) ||
		(iUdpWindow == 0) ||
		(iUdpWindow > 1024u)
	) {
		fprintf(stderr, "invalid network benchmark arguments.\n");
		return 1;
	}

	xbenchApplyCpuPinFromEnv();
	printf("xrt network loopback benchmark\n");
	if ( !benchNetworkTcp(iTcpIterations, iTcpMessageSize) ) {
		return 2;
	}
	if ( !benchNetworkUdp(iUdpPackets, iUdpPacketSize, iUdpWindow) ) {
		return 3;
	}
	return 0;
}
