/*
 * 范例：network/udp_send_tour —— UDP 发送族全形态巡礼
 * ----------------------------------------------------------------
 * 演示 API：
 *   【复制发送】  xrtNetUdpSendVec / xrtNetUdpSendVecTo
 *                  （多段 Span 聚集为一个数据报，两方向各演示一次）
 *   【引用发送】  xrtNetUdpSendRef / xrtNetUdpSendRefTo
 *                  （零复制 + 释放回调，回调在数据报离队时执行一次）
 *   【接管发送】  xrtNetUdpSendTake / xrtNetUdpSendTakeTo
 *                  （数据须来自 XRT 分配器，成功后所有权转移）
 *   【逐包控制】  xrtNetUdpSendMsgRef / xrtNetUdpSendMsgTake
 *                  （Control 为空或 Flags 为零时按普通发送收编）
 *   【批量受理】  xrtNetUdpSendBatch（按前缀受理并输出计数）
 *   【队列观测】  xrtNetUdpPending（发送队列字节数归零轮询）
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_send_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   send-tour: server=127.0.0.1:NNNNN
 *   vec/ref/take/msgref/batch = ok
 *   releases=3 pending=0
 *   server verified 7 datagrams
 *   client verified 4 datagrams
 *
 * 十种受理形态各发一条可辨识载荷，服务端与客户端
 *   互相核对收到的内容集合——每种形态的真实语义
 *   （复制/引用/接管/控制/批量）由对端校验兜底。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 释放回调：统计数据报离队次数（SendRef 族终态各执行一次）。 */
static void countRelease(ptr pContext, cbytes pData, size_t iSize)
{
	volatile uint32* pCount = (volatile uint32*)pContext;

	(void)pData;
	(void)iSize;
	*pCount = *pCount + 1u;
}



/* 在截止时间内等 UDP 进入指定状态。 */
static bool exampleWaitState(xnetudp* pUdp, xnetudpstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( xrtNetUdpState(pUdp) != State ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 收满指定数量的数据包并逐一核对内容标签。 */
static bool exampleDrain(
	xnetudp* pUdp,
	const char* const* pTags,
	size_t iTagCount
)
{
	size_t iMatched = 0;
	size_t i;
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( iMatched < iTagCount ) {
		xnetudppacket* pPacket = xrtNetUdpReceive(pUdp);

		if ( pPacket != NULL ) {
			size_t iSize = xrtNetUdpPacketSize(pPacket);
			bool bFound = false;

			for ( i = 0; i < iTagCount; ++i ) {
				size_t iTag = strlen(pTags[i]);

				if ( (iSize == iTag) &&
					 (memcmp(xrtNetUdpPacketData(pPacket),
						pTags[i], iTag) == 0) ) {
					bFound = true;
					break;
				}
			}
			xrtNetUdpPacketDestroy(pPacket);
			if ( !bFound ) {
				return false;
			}
			++iMatched;
			iDeadline = xrtDeadlineAfter(3000000u);
			continue;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 建立回环双端，按十种形态各发一条并双向核对。 */
int main(void)
{
	static const char* const sServerTags[] = {
		"vec-gather", "ref-zero-copy", "take-owned",
		"msgref-owned", "batch-1", "batch-2", "batch-3"
	};
	static const char* const sClientTags[] = {
		"svto-gather", "sref-zero-copy", "stake-owned", "smsg-take"
	};
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pServer = NULL;
	xnetudp* pClient = NULL;
	xnetaddr Address;
	xnetaddr ClientAddress;
	xnetdgramcontrol Control;
	xnetspan VecIn[2];
	xnetspan VecOut[2];
	xnetdgramsend Batch[3];
	ptr pTakeIn = NULL;
	ptr pTakeOut = NULL;
	uint32 iReleases = 0;
	xdeadline iDeadline;
	str sEndpoint = NULL;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetUdpConfigInit(&UdpConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(pEngine, &Address, 0, &UdpConfig, NULL, NULL);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		iResult = 3;
		goto Cleanup;
	}
	printf("send-tour: server=%s\n", sEndpoint);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	pClient = xrtNetUdpConnect(pEngine, &Address, 0, &UdpConfig,
		NULL, NULL);
	if ( (pClient == NULL) ||
		 !exampleWaitState(pServer, XNET_UDP_OPEN) ||
		 !exampleWaitState(pClient, XNET_UDP_OPEN) ) {
		iResult = 4;
		goto Cleanup;
	}

	/* 服务端回发需要客户端的实际本地地址（连接端自动绑定）。 */
	if ( !xrtNetUdpLocal(pClient, &ClientAddress) ) {
		iResult = 5;
		goto Cleanup;
	}

	/* SendMsg 的 Control：源地址控制（取客户端本地地址，端口清零）。 */
	memset(&Control, 0, sizeof(Control));
	Control.Flags = XNET_DGRAM_CONTROL_SOURCE;
	Control.Source = ClientAddress;
	Control.Source.Port = 0;

	/* 客户端方向（连接式，固定 Peer）：Vec / Ref / Take / MsgRef。 */
	VecIn[0].Data = "vec-";
	VecIn[0].Size = 4;
	VecIn[1].Data = "gather";
	VecIn[1].Size = 6;
	if ( xrtNetUdpSendVec(pClient, VecIn, 2) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	if ( xrtNetUdpSendRef(pClient, "ref-zero-copy", 13,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	pTakeIn = xrtMalloc(10);
	if ( pTakeIn == NULL ) {
		goto Cleanup;
	}
	memcpy(pTakeIn, "take-owned", 10);
	if ( xrtNetUdpSendTake(pClient, pTakeIn, 10) == XNET_RESULT_OK ) {
		pTakeIn = NULL;  /* 受理成功，所有权已转移 */
	}
	else {
		goto Cleanup;
	}
	if ( xrtNetUdpSendMsgRef(pClient, NULL, NULL, "msgref-owned", 12,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
		goto Cleanup;
	}

	/* 客户端批量：三项全空远端（连接式固定 Peer），按前缀一次受理。 */
	Batch[0].Remote = NULL;
	Batch[0].Data = "batch-1";
	Batch[0].Size = 7;
	Batch[1].Remote = NULL;
	Batch[1].Data = "batch-2";
	Batch[1].Size = 7;
	Batch[2].Remote = NULL;
	Batch[2].Data = "batch-3";
	Batch[2].Size = 7;
	{
		size_t iAccepted = 0;

		if ( (xrtNetUdpSendBatch(pClient, Batch, 3, &iAccepted) !=
			  XNET_RESULT_OK) || (iAccepted != 3) ) {
			goto Cleanup;
		}
	}

	/* 服务端方向（无连接，显式远端）：VecTo / RefTo / TakeTo / MsgTake。 */
	VecOut[0].Data = "svto-";
	VecOut[0].Size = 5;
	VecOut[1].Data = "gather";
	VecOut[1].Size = 6;
	if ( xrtNetUdpSendVecTo(pServer, &ClientAddress, VecOut, 2) !=
		 XNET_RESULT_OK ) {
		goto Cleanup;
	}
	if ( xrtNetUdpSendRefTo(pServer, &ClientAddress, "sref-zero-copy", 14,
		countRelease, &iReleases) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	pTakeOut = xrtMalloc(11);
	if ( pTakeOut == NULL ) {
		goto Cleanup;
	}
	memcpy(pTakeOut, "stake-owned", 11);
	if ( xrtNetUdpSendTakeTo(pServer, &ClientAddress, pTakeOut, 11) ==
		 XNET_RESULT_OK ) {
		pTakeOut = NULL;
	}
	else {
		goto Cleanup;
	}
	/* MsgTake：Control.Source 指定源地址（端口清零），接管 XRT 分配的载荷。 */
	pTakeOut = xrtMalloc(9);
	if ( pTakeOut == NULL ) {
		goto Cleanup;
	}
	memcpy(pTakeOut, "smsg-take", 9);
	if ( xrtNetUdpSendMsgTake(pServer, &ClientAddress, &Control,
		pTakeOut, 9) == XNET_RESULT_OK ) {
		pTakeOut = NULL;
	}
	else {
		goto Cleanup;
	}
	printf("vec/ref/take/msgref/batch = ok\n");

	/* 等三份引用载荷全部离队：释放回调恰好执行三次。 */
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( iReleases < 3u ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			goto Cleanup;
		}
		xrtThreadYield();
	}
	/* 发送队列排空：Pending 归零（复制/接管形态无排队字节）。 */
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( (xrtNetUdpPending(pClient) != 0) ||
			(xrtNetUdpPending(pServer) != 0) ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			goto Cleanup;
		}
		xrtThreadYield();
	}
	printf("releases=%u pending=0\n", iReleases);

	if ( !exampleDrain(pServer, sServerTags, 7) ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("server verified 7 datagrams\n");
	if ( !exampleDrain(pClient, sClientTags, 4) ) {
		iResult = 7;
		goto Cleanup;
	}
	printf("client verified 4 datagrams\n");
	iResult = 0;

Cleanup:
	xrtFree(pTakeIn);
	xrtFree(pTakeOut);
	xrtFree(sEndpoint);
	if ( pClient != NULL ) {
		(void)xrtNetUdpAbort(pClient);
		(void)exampleWaitState(pClient, XNET_UDP_CLOSED);
	}
	if ( pServer != NULL ) {
		(void)xrtNetUdpAbort(pServer);
		(void)exampleWaitState(pServer, XNET_UDP_CLOSED);
	}
	xrtNetUdpDestroy(pClient);
	xrtNetUdpDestroy(pServer);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 8;
	}
	return iResult;
}
