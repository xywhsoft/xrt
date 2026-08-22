#include "../bench_common.h"

#define XRT_MODULE_NET_TCP_SYNC
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"
#define XRT_BENCH_NETWORK_DESTROY_HELPERS
#include "bench_network_common.h"



/* 大流接收端只消费公开缓冲，并记录完整字节数。 */
typedef struct benchnetworkreceiver {
	volatile long Bytes;
	volatile long Reads;
	volatile long Closed;
} benchnetworkreceiver;



/* 发送端记录水位、Drain 和零复制引用释放契约。 */
typedef struct benchnetworksender {
	volatile long Released;
	volatile long HighWater;
	volatile long LowWater;
	volatile long Drain;
	volatile long Closed;
} benchnetworksender;



/* Worker 屏障使跨线程发送确定性占满 WriteLimit。 */
typedef struct benchnetworkbarrier {
	volatile long Started;
	volatile long Release;
} benchnetworkbarrier;



/* 在所属 Worker 上取得线程归属缓冲池的实时统计。 */
typedef struct benchnetworkpoolquery {
	xnetbufpoolinfo Info;
	volatile long Done;
} benchnetworkpoolquery;



/* 拉取接受后在 Stream 所属 Worker 安装每连接事件和数据。 */
typedef struct benchnetworkeventinstall {
	xnetstream* Stream;
	const xnetstreamevents* Events;
	ptr Data;
	volatile long Success;
	volatile long Done;
} benchnetworkeventinstall;



/* 消费服务器收到的全部字节，避免便利接收对象和额外复制。 */
static void benchNetworkCapacityRead(
	xnetstream* pStream,
	xnetbuf* pBuffer,
	ptr pData
)
{
	benchnetworkreceiver* pReceiver = (benchnetworkreceiver*)pData;
	size_t iSize;

	(void)pStream;
	if ( (pReceiver == NULL) || (pBuffer == NULL) ) {
		return;
	}
	iSize = xrtNetBufSize(pBuffer);
	if ( iSize > (size_t)LONG_MAX ) {
		return;
	}
	if ( xrtNetBufConsume(pBuffer, iSize) != iSize ) {
		return;
	}
	xbenchAtomicAdd(&pReceiver->Bytes, (long)iSize);
	xbenchAtomicInc(&pReceiver->Reads);
}



/* 记录接收 Stream 终态。 */
static void benchNetworkCapacityReceiverClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	benchnetworkreceiver* pReceiver = (benchnetworkreceiver*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	if ( pReceiver != NULL ) {
		xbenchAtomicInc(&pReceiver->Closed);
	}
}



/* 记录一次发送引用已经离开队列。 */
static void benchNetworkCapacityRelease(
	ptr pData,
	cbytes pBytes,
	size_t iSize
)
{
	benchnetworksender* pSender = (benchnetworksender*)pData;

	(void)pBytes;
	(void)iSize;
	if ( pSender != NULL ) {
		xbenchAtomicInc(&pSender->Released);
	}
}



/* 记录发送队列进入高水位。 */
static void benchNetworkCapacityHighWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	benchnetworksender* pSender = (benchnetworksender*)pData;

	(void)pStream;
	(void)iQueued;
	if ( pSender != NULL ) {
		xbenchAtomicInc(&pSender->HighWater);
	}
}



/* 记录发送队列回落到低水位。 */
static void benchNetworkCapacityLowWater(
	xnetstream* pStream,
	size_t iQueued,
	ptr pData
)
{
	benchnetworksender* pSender = (benchnetworksender*)pData;

	(void)pStream;
	(void)iQueued;
	if ( pSender != NULL ) {
		xbenchAtomicInc(&pSender->LowWater);
	}
}



/* 记录发送队列完全排空。 */
static void benchNetworkCapacityDrain(xnetstream* pStream, ptr pData)
{
	benchnetworksender* pSender = (benchnetworksender*)pData;

	(void)pStream;
	if ( pSender != NULL ) {
		xbenchAtomicInc(&pSender->Drain);
	}
}



/* 记录发送 Stream 终态。 */
static void benchNetworkCapacitySenderClose(
	xnetstream* pStream,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	benchnetworksender* pSender = (benchnetworksender*)pData;

	(void)pStream;
	(void)Result;
	(void)pError;
	if ( pSender != NULL ) {
		xbenchAtomicInc(&pSender->Closed);
	}
}



/* 阻塞客户端 Worker，直到主线程完成硬上限断言。 */
static void benchNetworkCapacityBarrier(xnetworker* pWorker, ptr pData)
{
	benchnetworkbarrier* pBarrier = (benchnetworkbarrier*)pData;

	(void)pWorker;
	if ( pBarrier == NULL ) {
		return;
	}
	xbenchAtomicStore(&pBarrier->Started, 1);
	while ( xbenchAtomicLoad(&pBarrier->Release) == 0 ) {
		xrtThreadYield();
	}
}



/* 在 Worker 内复制缓冲池实时统计。 */
static void benchNetworkCapacityPoolQuery(xnetworker* pWorker, ptr pData)
{
	benchnetworkpoolquery* pQuery = (benchnetworkpoolquery*)pData;
	xnetbufpool* pPool;

	if ( pQuery == NULL ) {
		return;
	}
	pPool = xrtNetWorkerBufPool(pWorker);
	xrtNetBufPoolGet(pPool, &pQuery->Info);
	xbenchAtomicStore(&pQuery->Done, 1);
}



/* 在 Stream 所属 Worker 完成事件接管。 */
static void benchNetworkCapacityInstallEvents(
	xnetworker* pWorker,
	ptr pData
)
{
	benchnetworkeventinstall* pInstall =
		(benchnetworkeventinstall*)pData;

	(void)pWorker;
	if (
		(pInstall != NULL) &&
		xrtNetStreamSetEvents(
			pInstall->Stream,
			pInstall->Events,
			pInstall->Data
		)
	) {
		xbenchAtomicStore(&pInstall->Success, 1);
	}
	if ( pInstall != NULL ) {
		xbenchAtomicStore(&pInstall->Done, 1);
	}
}



/* 等待一个 long 计数达到精确目标。 */
static bool benchNetworkCapacityWait(
	volatile long* pValue,
	long iExpected,
	uint32 iTimeout
)
{
	uint64 iDeadline = xbenchDeadlineAfterMs(iTimeout);

	while ( !xbenchDeadlineReached(iDeadline) ) {
		if ( xbenchAtomicLoad(pValue) == iExpected ) {
			return true;
		}
		xbenchSleepMs(1);
	}
	return xbenchAtomicLoad(pValue) == iExpected;
}



/* 聚合全部 Worker 缓冲池的实时块和字节。 */
static bool benchNetworkCapacityPools(
	xnetengine* pEngine,
	size_t* pBlocks,
	size_t* pBytes
)
{
	benchnetworkpoolquery Queries[2];
	size_t iBlocks = 0;
	size_t iBytes = 0;

	if ( (pEngine == NULL) || (pBlocks == NULL) || (pBytes == NULL) ) {
		return false;
	}
	memset(Queries, 0, sizeof(Queries));
	for ( uint32 i = 0; i < 2; i++ ) {
		if ( !xrtNetEnginePost(
			pEngine,
			i,
			benchNetworkCapacityPoolQuery,
			&Queries[i]
		) ) {
			return false;
		}
	}
	for ( uint32 i = 0; i < 2; i++ ) {
		if ( !benchNetworkCapacityWait(&Queries[i].Done, 1, 5000) ) {
			return false;
		}
		if (
			(Queries[i].Info.LiveBlocks > (SIZE_MAX - iBlocks)) ||
			(Queries[i].Info.LiveBytes > (SIZE_MAX - iBytes))
		) {
			return false;
		}
		iBlocks += Queries[i].Info.LiveBlocks;
		iBytes += Queries[i].Info.LiveBytes;
	}
	*pBlocks = iBlocks;
	*pBytes = iBytes;
	return true;
}



/* 建立一对回环 Stream，并保留 Listener 供统一清理。 */
static bool benchNetworkCapacityPair(
	xnetengine* pEngine,
	const xnetstreamconfig* pConfig,
	const xnetstreamevents* pClientEvents,
	ptr pClientData,
	const xnetstreamevents* pServerEvents,
	ptr pServerData,
	xnetlistener** pListener,
	xnetstream** pClient,
	xnetstream** pServer
)
{
	benchnetworkeventinstall Install;
	xnetlistenconfig ListenConfig;
	xnetaddr Address;
	bool bOpen;

	if (
		(pEngine == NULL) ||
		(pConfig == NULL) ||
		(pListener == NULL) ||
		(pClient == NULL) ||
		(pServer == NULL)
	) {
		return false;
	}
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Stream = *pConfig;
	ListenConfig.Affinity = 0;
	ListenConfig.Distribution = XNET_ACCEPT_LOCAL;
	if ( !xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	) ) {
		return false;
	}
	*pListener = xrtNetListen(
		pEngine,
		&ListenConfig,
		NULL,
		NULL,
		NULL
	);
	if (
		(*pListener == NULL) ||
		!xrtNetListenerLocal(*pListener, &Address)
	) {
		return false;
	}
	*pClient = xrtNetStreamConnect(
		pEngine,
		&Address,
		1,
		pConfig,
		pClientEvents,
		pClientData
	);
	if ( *pClient == NULL ) {
		return false;
	}
	*pServer = xrtNetListenerAcceptWait(
		*pListener,
		xrtDeadlineAfter(UINT64_C(5000000)),
		NULL
	);
	bOpen =
		(*pServer != NULL) &&
		xrtNetStreamWait(
			*pClient,
			XNET_STREAM_WAIT_OPEN,
			xrtDeadlineAfter(UINT64_C(5000000)),
			NULL
		);
	if ( !bOpen || (pServerEvents == NULL) ) {
		return bOpen;
	}
	memset(&Install, 0, sizeof(Install));
	Install.Stream = *pServer;
	Install.Events = pServerEvents;
	Install.Data = pServerData;
	if ( !xrtNetEnginePost(
		pEngine,
		xrtNetWorkerIndex(xrtNetStreamWorker(*pServer)),
		benchNetworkCapacityInstallEvents,
		&Install
	) ) {
		return false;
	}
	return
		benchNetworkCapacityWait(&Install.Done, 1, 5000) &&
		(xbenchAtomicLoad(&Install.Success) == 1);
}



/* 验证 SendRef、硬背压、WRITE 恢复与 DRAIN 后的大流吞吐。 */
static bool benchNetworkCapacityFlow(
	uint32 iChunks,
	size_t iChunkSize,
	size_t iWriteLimit
)
{
	benchnetworkreceiver Receiver;
	benchnetworksender Sender;
	benchnetworkbarrier Barrier;
	xnetengineconfig EngineConfig;
	xnetstreamconfig StreamConfig;
	xnetstreamevents ClientEvents;
	xnetstreamevents ServerEvents;
	xnetstreamstats ClientStats;
	xnetenginestats EngineStats;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pServer = NULL;
	uint8* pPayload = NULL;
	xbenchtimer Timer;
	uint64 iElapsed = 0;
	uint64 iTotalBytes;
	uint32 iSent = 0;
	uint32 iPressureChunks;
	bool bResult = false;
	bool bClean = true;
	cstr sStage = "configure";

	memset(&Receiver, 0, sizeof(Receiver));
	memset(&Sender, 0, sizeof(Sender));
	memset(&Barrier, 0, sizeof(Barrier));
	memset(&ClientEvents, 0, sizeof(ClientEvents));
	memset(&ServerEvents, 0, sizeof(ServerEvents));
	memset(&ClientStats, 0, sizeof(ClientStats));
	memset(&EngineStats, 0, sizeof(EngineStats));
	iPressureChunks = (uint32)(iWriteLimit / iChunkSize);
	iTotalBytes = ((uint64)iChunks) * ((uint64)iChunkSize);
	if (
		(iPressureChunks == 0) ||
		(iChunks <= iPressureChunks) ||
		(iTotalBytes > (uint64)LONG_MAX)
	) {
		return false;
	}
	pPayload = (uint8*)malloc(iChunkSize);
	if ( pPayload == NULL ) {
		return false;
	}
	for ( size_t i = 0; i < iChunkSize; i++ ) {
		pPayload[i] = (uint8)((i * 29u) + 13u);
	}

	ClientEvents.HighWater = benchNetworkCapacityHighWater;
	ClientEvents.LowWater = benchNetworkCapacityLowWater;
	ClientEvents.Drain = benchNetworkCapacityDrain;
	ClientEvents.Close = benchNetworkCapacitySenderClose;
	ServerEvents.Read = benchNetworkCapacityRead;
	ServerEvents.Close = benchNetworkCapacityReceiverClose;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup;
	}
	if ( !xbenchPrintNetworkBackend(pEngine) ) {
		goto cleanup;
	}
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadMode = XNET_STREAM_READ_PROBE;
	StreamConfig.ReadSize = iChunkSize;
	StreamConfig.ReadLimit = iWriteLimit * 2u;
	StreamConfig.WriteHighWater = iWriteLimit / 2u;
	StreamConfig.WriteLowWater = iWriteLimit / 4u;
	StreamConfig.WriteLimit = iWriteLimit;
	sStage = "connect";
	if ( !benchNetworkCapacityPair(
		pEngine,
		&StreamConfig,
		&ClientEvents,
		&Sender,
		&ServerEvents,
		&Receiver,
		&pListener,
		&pClient,
		&pServer
	) ) {
		goto cleanup;
	}
	xrtNetStreamPause(pServer);

	sStage = "block-worker";
	if (
		!xrtNetEnginePost(
			pEngine,
			1,
			benchNetworkCapacityBarrier,
			&Barrier
		) ||
		!benchNetworkCapacityWait(&Barrier.Started, 1, 5000)
	) {
		goto cleanup;
	}
	sStage = "fill-limit";
	for ( iSent = 0; iSent < iPressureChunks; iSent++ ) {
		if ( xrtNetStreamSendRef(
			pClient,
			pPayload,
			iChunkSize,
			benchNetworkCapacityRelease,
			&Sender
		) != XNET_RESULT_OK ) {
			goto cleanup;
		}
	}
	if (
		(xrtNetStreamPending(pClient) != iWriteLimit) ||
		(xrtNetStreamWritable(pClient) != 0) ||
		(xrtNetStreamSendRef(
			pClient,
			pPayload,
			iChunkSize,
			benchNetworkCapacityRelease,
			&Sender
		) != XNET_RESULT_AGAIN)
	) {
		goto cleanup;
	}

	sStage = "transfer";
	xbenchTimerStart(&Timer);
	xbenchAtomicStore(&Barrier.Release, 1);
	sStage = "resume-reader";
	if ( !xrtNetStreamResume(pServer) ) {
		goto cleanup;
	}
	sStage = "send-flow";
	while ( iSent < iChunks ) {
		xnetresult Result = xrtNetStreamSendRef(
			pClient,
			pPayload,
			iChunkSize,
			benchNetworkCapacityRelease,
			&Sender
		);

		if ( Result == XNET_RESULT_OK ) {
			iSent++;
			continue;
		}
		if (
			(Result != XNET_RESULT_AGAIN) ||
			!xrtNetStreamWait(
				pClient,
				XNET_STREAM_WAIT_WRITE,
				xrtDeadlineAfter(UINT64_C(30000000)),
				NULL
			)
		) {
			goto cleanup;
		}
	}
	sStage = "wait-drain";
	if ( !xrtNetStreamWait(
		pClient,
		XNET_STREAM_WAIT_DRAIN,
		xrtDeadlineAfter(UINT64_C(30000000)),
		NULL
	) ) {
		goto cleanup;
	}
	sStage = "wait-release-receive";
	if (
		!benchNetworkCapacityWait(
			&Sender.Released,
			(long)iChunks,
			30000
		) ||
		!benchNetworkCapacityWait(
			&Receiver.Bytes,
			(long)iTotalBytes,
			30000
		)
	) {
		goto cleanup;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	sStage = "stats";
	if (
		!xrtNetStreamStats(pClient, &ClientStats) ||
		(ClientStats.SentBytes != iTotalBytes) ||
		(ClientStats.SendRejected == 0) ||
		(ClientStats.PeakQueuedBytes != iWriteLimit) ||
		(ClientStats.QueuedBytes != 0) ||
		ClientStats.WriteBackpressured ||
		(xbenchAtomicLoad(&Sender.HighWater) == 0) ||
		(xbenchAtomicLoad(&Sender.LowWater) == 0) ||
		(xbenchAtomicLoad(&Sender.Drain) == 0)
	) {
		goto cleanup;
	}

	xbenchPrintMetricU64("tcp_capacity_bytes", iTotalBytes);
	xbenchPrintMetricU64("tcp_capacity_write_limit", iWriteLimit);
	xbenchPrintMetricU64(
		"tcp_capacity_peak_queued_bytes",
		ClientStats.PeakQueuedBytes
	);
	xbenchPrintMetricU64(
		"tcp_capacity_send_rejected",
		ClientStats.SendRejected
	);
	xbenchPrintMetricU64("tcp_capacity_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"tcp_ref_mib_per_sec",
		xbenchSafeRate(iTotalBytes, iElapsed) / (1024.0 * 1024.0)
	);
	xbenchPrintMetricDouble(
		"tcp_ref_sends_per_sec",
		xbenchSafeRate(iChunks, iElapsed)
	);
	sStage = "done";
	bResult = true;

cleanup:
	xbenchAtomicStore(&Barrier.Release, 1);
	if ( !bResult ) {
		(void)xrtNetStreamStats(pClient, &ClientStats);
		fprintf(stderr, "TCP capacity benchmark failed at %s", sStage);
		xbenchPrintCurrentError();
		fprintf(
			stderr,
			" sent=%" PRIu32 " pending=%zu peak=%zu rejected=%" PRIu64
			" released=%ld received=%ld client_state=%d server_state=%d\n",
			iSent,
			ClientStats.QueuedBytes,
			ClientStats.PeakQueuedBytes,
			ClientStats.SendRejected,
			xbenchAtomicLoad(&Sender.Released),
			xbenchAtomicLoad(&Receiver.Bytes),
			(int)xrtNetStreamState(pClient),
			(int)xrtNetStreamState(pServer)
		);
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
	return bResult && bClean;
}



/* 验证 PROBE 空闲连接没有预分配每对象读缓冲。 */
static bool benchNetworkCapacityIdle(uint32 iConnections)
{
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetstreamconfig StreamConfig;
	xnetenginestats EngineStats;
	xnetengine* pEngine = NULL;
	xnetlistener* pListener = NULL;
	xnetstream** pClients = NULL;
	xnetstream** pServers = NULL;
	xnetaddr Address;
	xbenchtimer Timer;
	uint64 iElapsed = 0;
	size_t iLiveBlocks = 0;
	size_t iLiveBytes = 0;
	uint32 iCreated = 0;
	bool bResult = false;
	bool bClean = true;
	cstr sStage = "allocate";

	memset(&EngineStats, 0, sizeof(EngineStats));
	pClients = (xnetstream**)calloc(iConnections, sizeof(xnetstream*));
	pServers = (xnetstream**)calloc(iConnections, sizeof(xnetstream*));
	if ( (pClients == NULL) || (pServers == NULL) ) {
		goto cleanup;
	}
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto cleanup;
	}
	xrtNetStreamConfigInit(&StreamConfig);
	StreamConfig.ReadMode = XNET_STREAM_READ_PROBE;
	xrtNetListenConfigInit(&ListenConfig);
	ListenConfig.Stream = StreamConfig;
	ListenConfig.Affinity = 0;
	ListenConfig.Distribution = XNET_ACCEPT_ROUND_ROBIN;
	ListenConfig.AcceptConcurrency = 32;
	ListenConfig.AcceptQueueLimit = iConnections;
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
	xbenchTimerStart(&Timer);
	for ( iCreated = 0; iCreated < iConnections; ) {
		pClients[iCreated] = xrtNetStreamConnect(
			pEngine,
			&Address,
			(uint64)(iCreated & 1u),
			&StreamConfig,
			NULL,
			NULL
		);
		if ( pClients[iCreated] == NULL ) {
			goto cleanup;
		}
		pServers[iCreated] = xrtNetListenerAcceptWait(
			pListener,
			xrtDeadlineAfter(UINT64_C(5000000)),
			NULL
		);
		if (
			(pServers[iCreated] == NULL) ||
			!xrtNetStreamWait(
				pClients[iCreated],
				XNET_STREAM_WAIT_OPEN,
				xrtDeadlineAfter(UINT64_C(5000000)),
				NULL
			)
		) {
			iCreated++;
			goto cleanup;
		}
		iCreated++;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);
	xbenchSleepMs(20);

	sStage = "pool-stats";
	if (
		!benchNetworkCapacityPools(
			pEngine,
			&iLiveBlocks,
			&iLiveBytes
		) ||
		(iLiveBlocks != 0) ||
		(iLiveBytes != 0)
	) {
		goto cleanup;
	}
	xbenchPrintMetricU64("tcp_idle_connections", iConnections);
	xbenchPrintMetricU64("tcp_idle_connect_elapsed_ns", iElapsed);
	xbenchPrintMetricDouble(
		"tcp_idle_connects_per_sec",
		xbenchSafeRate(iConnections, iElapsed)
	);
	xbenchPrintMetricDouble(
		"tcp_idle_buffer_bytes_per_stream",
		(double)iLiveBytes / (double)(iConnections * 2u)
	);
	sStage = "done";
	bResult = true;

cleanup:
	if ( !bResult ) {
		fprintf(stderr, "TCP idle benchmark failed at %s", sStage);
		xbenchPrintCurrentError();
		fprintf(
			stderr,
			" created=%" PRIu32 " live_blocks=%zu live_bytes=%zu\n",
			iCreated,
			iLiveBlocks,
			iLiveBytes
		);
	}
	for ( uint32 i = 0; i < iCreated; i++ ) {
		bClean = xbenchNetworkStreamDestroy(
			pClients[i],
			!bResult
		) && bClean;
		bClean = xbenchNetworkStreamDestroy(
			pServers[i],
			!bResult
		) && bClean;
	}
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
	free(pServers);
	free(pClients);
	return bResult && bClean;
}



/* 运行网络硬容量与空闲连接内存两条发布基准。 */
int main(int argc, char** argv)
{
	uint32 iChunks = xbenchArgU32(argc, argv, 1, 4096);
	uint32 iChunkSize = xbenchArgU32(argc, argv, 2, 16384);
	uint32 iWriteLimit = xbenchArgU32(argc, argv, 3, 262144);
	uint32 iConnections = xbenchArgU32(argc, argv, 4, 512);
	uint64 iTotalBytes = ((uint64)iChunks) * ((uint64)iChunkSize);

	if (
		(iChunks == 0) ||
		(iChunkSize == 0) ||
		(iWriteLimit == 0) ||
		(iChunkSize > iWriteLimit) ||
		((iWriteLimit % iChunkSize) != 0) ||
		(iTotalBytes > (uint64)LONG_MAX) ||
		(iConnections == 0) ||
		(iConnections > 4096)
	) {
		fprintf(stderr, "invalid network capacity benchmark arguments.\n");
		return 1;
	}

	xbenchApplyCpuPinFromEnv();
	printf("xrt network capacity benchmark\n");
	if ( !benchNetworkCapacityFlow(
		iChunks,
		(size_t)iChunkSize,
		(size_t)iWriteLimit
	) ) {
		return 2;
	}
	if ( !benchNetworkCapacityIdle(iConnections) ) {
		return 3;
	}
	return 0;
}
