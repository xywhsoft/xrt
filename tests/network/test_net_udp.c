#include "../test.h"



#if defined(__linux__)
	#include <errno.h>
#endif



#if !defined(TEST_UDP_BACKEND)
	#define TEST_UDP_BACKEND XNET_PORT_SELECT
	#define TEST_UDP_BACKEND_NAME "select"
#endif



/* 在截止时间内等待 UDP 进入指定状态。 */
static void testUdpWaitState(xnetudp* pUdp, xnetudpstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtNetUdpState(pUdp) != State ) {
		testRequire(!xrtDeadlineExpired(iDeadline),
			"UDP state transition timed out");
		xrtThreadYield();
	}
}



/* 在截止时间内拉取一个 UDP 数据包。 */
static xnetudppacket* testUdpReceive(xnetudp* pUdp)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);
	xnetudppacket* pPacket;

	for ( ;; ) {
		pPacket = xrtNetUdpReceive(pUdp);
		if ( pPacket != NULL ) {
			return pPacket;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			xnetudpstats Stats;
			const xerror* pError = xrtNetUdpError(pUdp);

			(void)xrtNetUdpStats(pUdp, &Stats);
			fprintf(
				stderr,
				"[UDP timeout] state=%d recv=%llu send=%llu "
				"recv_errors=%llu send_errors=%llu active=%u "
				"queued=%zu error=%d/%d/%d\n",
				(int)Stats.State,
				(unsigned long long)Stats.ReceivedPackets,
				(unsigned long long)Stats.SentPackets,
				(unsigned long long)Stats.ReceiveErrors,
				(unsigned long long)Stats.SendErrors,
				Stats.ActiveReceives,
				Stats.QueuedPackets,
				pError != NULL ? (int)xrtErrorKind(pError) : 0,
				pError != NULL ? xrtErrorCode(pError) : 0,
				pError != NULL ? xrtErrorSystemCode(pError) : 0
			);
			testRequire(false, "UDP receive timed out");
		}
		xrtThreadYield();
	}
}



/* 高层 UDP 必须原样贯通分段发送和合并接收元数据。 */
static void testUdpSegmentation(xnetengine* pEngine)
{
	xnetudpconfig ServerConfig;
	xnetudpconfig ClientConfig;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetsocket Probe;
	xnetaddr Address;
	xnetaddr ClientAddress;
	xnetdgramcontrol Control;
	uint32 iCapabilities;
	char sPayload[8] = { 0 };
	size_t iTotal = 0;

	Probe = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(Probe != NULL, "UDP segmentation probe open failed");
	iCapabilities = xrtNetSocketDgramCapabilities(Probe);
	testRequire(xrtNetSocketClose(Probe),
		"UDP segmentation probe close failed");
	if ( ((iCapabilities & XNET_DGRAM_CAP_SEGMENT_SEND) == 0) ||
		 ((iCapabilities & XNET_DGRAM_CAP_SEGMENT_RECEIVE) == 0) ) {
		return;
	}

	xrtNetUdpConfigInit(&ServerConfig);
	ServerConfig.ReceiveMeta = XNET_DGRAM_META_SEGMENT_SIZE;
	xrtNetUdpConfigInit(&ClientConfig);
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	) && xrtNetAddrLoopback(
		&ClientAddress,
		XNET_FAMILY_IPV4,
		0
	), "UDP segmentation address setup failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&ServerConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) && xrtNetUdpLocal(pServer, &Address),
		"UDP segmentation receiver open failed");
	pClient = xrtNetUdpBind(
		pEngine,
		&ClientAddress,
		1,
		&ClientConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "UDP segmentation sender open failed");
	testUdpWaitState(pServer, XNET_UDP_OPEN);
	testUdpWaitState(pClient, XNET_UDP_OPEN);
	testRequire((xrtNetUdpSendControlAvailable(pClient) &
		XNET_DGRAM_CONTROL_SEGMENT_SIZE) != 0,
		"high-level UDP segmentation send is unavailable");

	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SEGMENT_SIZE;
	Control.SegmentSize = 3;
	testRequire(xrtNetUdpSendMsg(
		pClient,
		&Address,
		&Control,
		"abcdefg",
		7
	) == XNET_RESULT_OK, "high-level UDP segmented send failed");

	/* 合并是平台优化而不是交付保证，未合并时仍保持原始数据报边界。 */
	while ( iTotal < 7 ) {
		size_t iSize;
		const xnetdgrammeta* pMeta;

		pPacket = testUdpReceive(pServer);
		iSize = xrtNetUdpPacketSize(pPacket);
		pMeta = xrtNetUdpPacketMeta(pPacket);
		testRequire((iSize != 0) && (iSize <= (7 - iTotal)) &&
			(pMeta != NULL), "high-level UDP segmented receive failed");
		if ( (pMeta->Flags & XNET_DGRAM_META_SEGMENT_SIZE) != 0 ) {
			testRequire(pMeta->SegmentSize == 3,
				"high-level UDP coalesced segment size mismatch");
		} else {
			testRequire(iSize <= 3,
				"high-level UDP unmarked receive exceeded one segment");
		}
		memcpy(sPayload + iTotal, xrtNetUdpPacketData(pPacket), iSize);
		iTotal += iSize;
		xrtNetUdpPacketDestroy(pPacket);
	}
	testRequire(memcmp(sPayload, "abcdefg", 7) == 0,
		"high-level UDP segmented payload mismatch");

	testRequire(xrtNetUdpClose(pClient) && xrtNetUdpClose(pServer),
		"UDP segmentation close request failed");
	testUdpWaitState(pClient, XNET_UDP_CLOSED);
	testUdpWaitState(pServer, XNET_UDP_CLOSED);
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
}



#if defined(_WIN32) || defined(_WIN64)

/* 远端端口关闭产生的 ICMP 不能终止共享的无连接 IOCP UDP 端点。 */
static void testUdpClosedPeerReset(xnetengine* pEngine)
{
	xnetudpconfig Config;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetsocket Probe;
	xnetaddr ClosedPeer;
	xnetaddr ServerAddress;
	xnetaddr ClientAddress;
	xdeadline Deadline;
	const char sPayload[] = "udp-still-open";

	if ( TEST_UDP_BACKEND != XNET_PORT_IOCP ) {
		return;
	}
	Probe = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(Probe != NULL) &&
		xrtNetAddrLoopback(&ClosedPeer, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Probe, &ClosedPeer) &&
		xrtNetSocketLocal(Probe, &ClosedPeer) &&
		xrtNetSocketClose(Probe),
		"UDP closed-peer probe setup failed"
	);

	xrtNetUdpConfigInit(&Config);
	testRequire(
		xrtNetAddrLoopback(&ServerAddress, XNET_FAMILY_IPV4, 0) &&
		xrtNetAddrLoopback(&ClientAddress, XNET_FAMILY_IPV4, 0),
		"UDP closed-peer address setup failed"
	);
	pServer = xrtNetUdpBind(
		pEngine, &ServerAddress, 0, &Config, NULL, NULL
	);
	testRequire((pServer != NULL) &&
		xrtNetUdpLocal(pServer, &ServerAddress),
		"UDP closed-peer server open failed");
	pClient = xrtNetUdpConnect(
		pEngine, &ServerAddress, 1, &Config, NULL, NULL
	);
	testRequire(pClient != NULL, "UDP closed-peer client open failed");
	testUdpWaitState(pServer, XNET_UDP_OPEN);
	testUdpWaitState(pClient, XNET_UDP_OPEN);

	for ( size_t i = 0; i < 64u; i++ ) {
		testRequire(xrtNetUdpSendTo(
			pServer, &ClosedPeer, &i, sizeof(i)
		) == XNET_RESULT_OK, "UDP closed-peer burst send failed");
	}
	Deadline = xrtDeadlineAfter(3000000u);
	while ( xrtNetUdpPending(pServer) != 0 ) {
		testRequire(!xrtDeadlineExpired(Deadline),
			"UDP closed-peer burst did not drain");
		xrtThreadYield();
	}
	xrtSleepUs(250000u);
	testRequire(xrtNetUdpState(pServer) == XNET_UDP_OPEN,
		"closed peer terminated unconnected UDP");
	testRequire(xrtNetUdpSend(
		pClient, sPayload, sizeof(sPayload) - 1u
	) == XNET_RESULT_OK, "UDP post-reset send failed");
	pPacket = testUdpReceive(pServer);
	testRequire(
		(xrtNetUdpPacketSize(pPacket) == (sizeof(sPayload) - 1u)) &&
		(memcmp(
			xrtNetUdpPacketData(pPacket),
			sPayload,
			sizeof(sPayload) - 1u
		) == 0),
		"UDP did not recover after closed-peer ICMP"
	);
	xrtNetUdpPacketDestroy(pPacket);

	testRequire(xrtNetUdpClose(pClient) && xrtNetUdpClose(pServer),
		"UDP closed-peer test close request failed");
	testUdpWaitState(pClient, XNET_UDP_CLOSED);
	testUdpWaitState(pServer, XNET_UDP_CLOSED);
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
}

#endif



#if defined(__linux__)

/* 在 Linux io_uring 路径验证拥有型异步错误队列。 */
static void testUdpErrors(xnetengine* pEngine)
{
	xnetudpconfig Config;
	xnetudpstats Stats;
	xnetudp* pUdp;
	xnetudperrorpacket* pPacket;
	xnetudperrorpacket* pReference;
	const xnetdgramerror* pError;
	xnetsocket Reserved;
	xnetaddr Target;
	xdeadline Deadline;

	if ( TEST_UDP_BACKEND != XNET_PORT_URING ) {
		return;
	}
	Reserved = xrtNetSocketOpen(
		XNET_FAMILY_IPV4,
		XNET_SOCKET_DGRAM,
		XNET_SOCKET_NONBLOCK
	);
	testRequire(
		(Reserved != NULL) &&
		xrtNetAddrLoopback(&Target, XNET_FAMILY_IPV4, 0) &&
		xrtNetSocketBind(Reserved, &Target) &&
		xrtNetSocketLocal(Reserved, &Target) &&
		xrtNetSocketClose(Reserved),
		"UDP error target setup failed"
	);

	xrtNetUdpConfigInit(&Config);
	Config.ReceiveErrors = true;
	Config.ErrorSize = 64;
	Config.ErrorQueueLimit = 4;
	Config.ErrorQueueByteLimit = 256;
	Config.PathMtu = XNET_PMTU_DISCOVER;
	pUdp = xrtNetUdpConnect(
		pEngine,
		&Target,
		0,
		&Config,
		NULL,
		NULL
	);
	testRequire(pUdp != NULL, "UDP error receiver open failed");
	testUdpWaitState(pUdp, XNET_UDP_OPEN);
	testRequire(
		xrtNetUdpSend(pUdp, "error", 5) == XNET_RESULT_OK,
		"UDP error trigger send failed"
	);
	Deadline = xrtDeadlineAfter(5000000u);
	for ( ;; ) {
		pPacket = xrtNetUdpReceiveError(pUdp);
		if ( pPacket != NULL ) {
			break;
		}
		testRequire(
			!xrtDeadlineExpired(Deadline),
			"UDP asynchronous error timed out"
		);
		xrtThreadYield();
	}
	pError = xrtNetUdpErrorPacketInfo(pPacket);
	testRequire(
		(pError != NULL) &&
		(pError->SystemCode == ECONNREFUSED) &&
		(pError->Kind == XERR_IO) &&
		(pError->Origin == XNET_DGRAM_ERROR_ICMP) &&
		((pError->Flags & XNET_DGRAM_ERROR_REMOTE) != 0) &&
		xrtNetAddrEqual(&pError->Remote, &Target) &&
		(xrtNetUdpErrorPacketSize(pPacket) == 5) &&
		(memcmp(xrtNetUdpErrorPacketData(pPacket), "error", 5) == 0),
		"UDP asynchronous error packet mismatch"
	);
	pReference = xrtNetUdpErrorPacketRef(pPacket);
	testRequire(pReference == pPacket, "UDP error packet ref failed");
	xrtNetUdpErrorPacketDestroy(pReference);
	xrtNetUdpErrorPacketDestroy(pPacket);
	testRequire(
		xrtNetUdpStats(pUdp, &Stats) &&
		(Stats.DatagramErrors == 1) &&
		(Stats.DatagramErrorsDropped == 0) &&
		(Stats.ErrorQueued == 0) &&
		(Stats.PeakErrorQueued == 1) &&
		(Stats.PeakErrorQueuedBytes == 5) &&
		(xrtNetUdpQueuedErrors(pUdp) == 0) &&
		(xrtNetUdpQueuedErrorBytes(pUdp) == 0),
		"UDP asynchronous error statistics mismatch"
	);
	testRequire(xrtNetUdpClose(pUdp), "UDP error receiver close failed");
	testUdpWaitState(pUdp, XNET_UDP_CLOSED);
	xrtNetUdpDestroy(pUdp);
}

#endif



/* 验证 UDP 绑定、连接、零长度、向量、批量和双向收发。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine;
	xnetudp* pServer;
	xnetudp* pClient;
	xnetudppacket* pPacket;
	xnetudppacket* Packets[3];
	xnetdgramsend Batch[3];
	xnetspan Spans[2];
	xnetudpstats ServerStats;
	xnetudpstats ClientStats;
	xnetaddr Address;
	xnetaddr Peer;
	xnetdgrammeta ReplyMeta;
	xnetdgramcontrol ReplyControl;
	uint32 iSendControl;
	size_t iAccepted = 0;
	size_t iCount;
	char sBatch[3] = {'1', '2', '3'};

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = TEST_UDP_BACKEND;
	EngineConfig.Workers = 2;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"UDP engine start failed");
	#if defined(_WIN32) || defined(_WIN64)
		testUdpClosedPeerReset(pEngine);
	#endif
	testUdpSegmentation(pEngine);
	#if defined(__linux__)
		testUdpErrors(pEngine);
	#endif

	xrtNetUdpConfigInit(&UdpConfig);
	testRequire((UdpConfig.ReceiveSize == 2048u) &&
		 (UdpConfig.ReceiveConcurrency == 1) &&
		 (UdpConfig.ReceiveBatch >= 1) &&
		 (UdpConfig.ErrorSize == 256u) &&
		 (UdpConfig.ErrorQueueLimit != 0) &&
		 (UdpConfig.PathMtu == XNET_PMTU_SYSTEM) &&
		 !UdpConfig.ReceiveErrors &&
		 (UdpConfig.SendConcurrency == 1) &&
		 (UdpConfig.SendLowWater <= UdpConfig.SendHighWater) &&
		 (UdpConfig.SendHighWater <= UdpConfig.SendLimit),
		"UDP defaults are invalid");
	UdpConfig.ReceiveMeta = XNET_DGRAM_META_DESTINATION |
		XNET_DGRAM_META_INTERFACE;
	testRequire(xrtNetAddrLoopback(
		&Address,
		XNET_FAMILY_IPV4,
		0
	), "UDP loopback address failed");
	pServer = xrtNetUdpBind(
		pEngine,
		&Address,
		0,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire((pServer != NULL) &&
		 xrtNetUdpLocal(pServer, &Address) &&
		 (Address.Port != 0), "UDP bind failed");
	pClient = xrtNetUdpConnect(
		pEngine,
		&Address,
		1,
		&UdpConfig,
		NULL,
		NULL
	);
	testRequire(pClient != NULL, "connected UDP open failed");
	testUdpWaitState(pServer, XNET_UDP_OPEN);
	testUdpWaitState(pClient, XNET_UDP_OPEN);
	testRequire(xrtNetUdpConnected(pClient) &&
		 !xrtNetUdpConnected(pServer) &&
		 xrtNetUdpPeer(pClient, &Peer) &&
		 xrtNetAddrEqual(&Peer, &Address),
		"connected UDP peer mismatch");

	testRequire(xrtNetUdpSend(pClient, NULL, 0) == XNET_RESULT_OK,
		"zero-length UDP send failed");
	testRequire(xrtNetUdpSend(pClient, "hello", 5) == XNET_RESULT_OK,
		"UDP copy send failed");
	Spans[0].Data = (cbytes)"vec";
	Spans[0].Size = 3;
	Spans[1].Data = (cbytes)"tor";
	Spans[1].Size = 3;
	testRequire(xrtNetUdpSendVec(pClient, Spans, 2) == XNET_RESULT_OK,
		"UDP vector send failed");
	{
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		for ( ;; ) {
			(void)xrtNetUdpStats(pClient, &ClientStats);
			if ( ClientStats.SentPackets == 3 ) {
				break;
			}
			if ( xrtDeadlineExpired(iDeadline) ) {
				fprintf(
					stderr,
					"[UDP send timeout] state=%d sent=%llu "
					"errors=%llu queued=%zu active=%u\n",
					(int)ClientStats.State,
					(unsigned long long)ClientStats.SentPackets,
					(unsigned long long)ClientStats.SendErrors,
					ClientStats.QueuedPackets,
					ClientStats.ActiveReceives
				);
				testRequire(false, "UDP send completion timed out");
			}
			xrtThreadYield();
		}
	}

	pPacket = testUdpReceive(pServer);
	testRequire((xrtNetUdpPacketSize(pPacket) == 0) &&
		 !xrtNetUdpPacketTruncated(pPacket) &&
		 (xrtNetUdpPacketMeta(pPacket) != NULL) &&
		 ((xrtNetUdpPacketMeta(pPacket)->Flags &
			(XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE)) ==
		  (XNET_DGRAM_META_DESTINATION | XNET_DGRAM_META_INTERFACE)) &&
		 xrtNetAddrIsLoopback(
			 &xrtNetUdpPacketMeta(pPacket)->Destination
		 ) && (xrtNetUdpPacketMeta(pPacket)->Interface != 0),
		"zero-length UDP receive mismatch");
	xrtNetUdpPacketDestroy(pPacket);
	pPacket = testUdpReceive(pServer);
	testRequire((xrtNetUdpPacketSize(pPacket) == 5) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "hello", 5) == 0),
		"UDP copy receive mismatch");
	Peer = *xrtNetUdpPacketRemote(pPacket);
	ReplyMeta = *xrtNetUdpPacketMeta(pPacket);
	xrtNetUdpPacketDestroy(pPacket);
	pPacket = testUdpReceive(pServer);
	testRequire((xrtNetUdpPacketSize(pPacket) == 6) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "vector", 6) == 0),
		"UDP vector receive mismatch");
	xrtNetUdpPacketDestroy(pPacket);

	/* 回包可直接复用接收元数据，不需要修改整个套接字的发送选项。 */
	iSendControl = xrtNetUdpSendControlAvailable(pServer);
	testRequire((iSendControl & XNET_DGRAM_CONTROL_SOURCE) != 0,
		"UDP source control is unavailable");
	memset(&ReplyControl, 0, sizeof(ReplyControl));
	ReplyControl.Flags = XNET_DGRAM_CONTROL_SOURCE;
	ReplyControl.Source = ReplyMeta.Destination;
	ReplyControl.Source.Port = 0;
	testRequire(xrtNetUdpSendMsg(
		pServer,
		&Peer,
		&ReplyControl,
		"reply",
		5
	) == XNET_RESULT_OK, "UDP reply send failed");
	pPacket = testUdpReceive(pClient);
	testRequire((xrtNetUdpPacketSize(pPacket) == 5) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "reply", 5) == 0),
		"UDP reply receive mismatch");
	xrtNetUdpPacketDestroy(pPacket);

	for ( size_t i = 0; i < 3; i++ ) {
		Batch[i].Remote = NULL;
		Batch[i].Data = &sBatch[i];
		Batch[i].Size = 1;
	}
	testRequire((xrtNetUdpSendBatch(
		pClient,
		Batch,
		3,
		&iAccepted
	) == XNET_RESULT_OK) && (iAccepted == 3),
		"UDP batch send failed");
	{
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		while ( xrtNetUdpQueued(pServer) < 3 ) {
			testRequire(!xrtDeadlineExpired(iDeadline),
				"UDP batch receive timed out");
			xrtThreadYield();
		}
	}
	iCount = xrtNetUdpReceiveBatch(pServer, Packets, 3);
	testRequire(iCount == 3, "UDP receive batch count mismatch");
	for ( size_t i = 0; i < iCount; i++ ) {
		testRequire((xrtNetUdpPacketSize(Packets[i]) == 1) &&
			 (*(xrtNetUdpPacketData(Packets[i])) == (uint8)('1' + i)),
			"UDP receive batch order mismatch");
		xrtNetUdpPacketDestroy(Packets[i]);
	}

	testRequire(xrtNetUdpClose(pClient) && xrtNetUdpClose(pServer),
		"UDP close request failed");
	testUdpWaitState(pClient, XNET_UDP_CLOSED);
	testUdpWaitState(pServer, XNET_UDP_CLOSED);
	testRequire(xrtNetUdpStats(pServer, &ServerStats) &&
		 xrtNetUdpStats(pClient, &ClientStats) &&
		 (ServerStats.ReceivedPackets == 6) &&
		 (ClientStats.ReceivedPackets == 1) &&
		 (ServerStats.SentPackets == 1) &&
		 (ClientStats.SentPackets == 6) &&
		 (ServerStats.QueuedPackets == 0) &&
		 (ClientStats.QueuedPackets == 0) &&
		 (ServerStats.ActiveReceives == 0) &&
		 (ClientStats.ActiveReceives == 0) &&
		 (ServerStats.ActiveSends == 0) &&
		 (ClientStats.ActiveSends == 0),
		"UDP final stats mismatch");
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	testRequire(xrtNetEngineDestroy(pEngine), "UDP engine destroy failed");
	printf("[PASS] network UDP %s\n", TEST_UDP_BACKEND_NAME);
	return 0;
}
