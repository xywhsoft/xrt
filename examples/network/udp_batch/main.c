/*
 * 范例：network/udp_batch —— 批量接收 + 截断包 + 队列条件
 * ----------------------------------------------------------------
 * 演示 API：
 *   【批量拉取】  xrtNetUdpReceiveBatch（一次锁内取一批）
 *                  xrtNetUdpReceiveBatchWait（阻塞批量）
 *                  xrtNetUdpReceiveBatchAsync（Future 批量）
 *   【批量结果】  xrtNetUdpBatchCount / xrtNetUdpBatchPacket（借用）
 *                  xrtNetUdpBatchTake（转移）/ xrtNetUdpBatchRef /
 *                  xrtNetUdpBatchDestroy
 *   【引用计数】  xrtNetUdpPacketRef（跨批量保留零复制结果）
 *   【截断语义】  xrtNetUdpPacketTruncated
 *                  （ReceiveSize 小于对端载荷 + TRUNCATE_DELIVER）
 *   【错误队列】  xrtNetUdpReceiveError / xrtNetUdpReceiveErrorBatch
 *                  （空队列的 NULL/0 路径）
 *                  xrtNetUdpReceiveErrorAsync（关闭使未决 Future 终结）
 *   【发送条件】  xrtNetUdpWritable / xrtNetUdpWritableAsync
 *                  （队列空闲时立即可写）
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_batch/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   batch: truncated prefix=16 of 64
 *   pull batches: 3 packets in 1 batch
 *   wait batch: 2 packets (borrow=take=ok)
 *   future batch: 2 packets, ref kept size=2
 *   error queue: empty pull=0/0, async terminal=4
 *   writable: sync=1 async=1
 *
 * 同一批内演示借用（BatchPacket）与转移（BatchTake）
 *   两种访问方式——转移出的包脱离批量结果生命周期，
 *   BatchDestroy 只销毁剩余未转移项。
 * FutureValue 是借用而非移交：引用计数归 Future 持有，
 *   要跨 FutureDestroy 保留批量/数据包，必须先 BatchRef /
 *   PacketRef 自持引用——替 Future 释放会造成双重释放。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 等 Future 成功解析。 */
static bool exampleFutureWait(xfuture* pFuture)
{
	return (pFuture != NULL) &&
		(xrtFutureWaitFor(pFuture, UINT64_C(3000000)) == XWAIT_OK) &&
		(xrtFutureState(pFuture) == XFUTURE_RESOLVED);
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



/* 收满 count 条（普通单包拉取，等待全部到齐）。 */
static size_t exampleDrainCount(xnetudp* pUdp, size_t iWant)
{
	size_t iGot = 0;
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( iGot < iWant ) {
		xnetudppacket* pPacket = xrtNetUdpReceive(pUdp);

		if ( pPacket != NULL ) {
			xrtNetUdpPacketDestroy(pPacket);
			++iGot;
			iDeadline = xrtDeadlineAfter(3000000u);
			continue;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			break;
		}
		xrtThreadYield();
	}
	return iGot;
}



/* 服务端绑小接收槽演示截断；客户端连发演示三种批量接收。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pServer = NULL;
	xnetudp* pClient = NULL;
	xnetaddr Address;
	xnetudppacket* pPackets[8];
	xnetudppacket* pKept = NULL;
	xnetudpbatch* pBatch = NULL;
	xnetudpbatch* pBatchRef = NULL;
	xfuture* pFuture = NULL;
	xfuture* pWritable = NULL;
	xfuture* pErrorFuture = NULL;
	char Big[64];
	size_t iCount;
	size_t i;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetUdpConfigInit(&UdpConfig);
	UdpConfig.ReceiveSize = 16u;  /* 对端发 64 字节 → 截断前缀 */
	UdpConfig.Truncation = XNET_UDP_TRUNCATE_DELIVER;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(pEngine, &Address, 0, &UdpConfig,
		NULL, NULL);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Address) ) {
		iResult = 2;
		goto Cleanup;
	}
	pClient = xrtNetUdpConnect(pEngine, &Address, 0, NULL,
		NULL, NULL);
	if ( (pClient == NULL) ||
		 !exampleWaitState(pServer, XNET_UDP_OPEN) ||
		 !exampleWaitState(pClient, XNET_UDP_OPEN) ) {
		iResult = 3;
		goto Cleanup;
	}

	/* 截断演示：64 字节载荷到达 16 字节接收槽。 */
	memset(Big, 'A', sizeof(Big));
	if ( xrtNetUdpSend(pClient, Big, sizeof(Big)) != XNET_RESULT_OK ) {
		goto Cleanup;
	}
	iCount = exampleDrainCount(pServer, 1);
	if ( iCount != 1u ) {
		iResult = 4;
		goto Cleanup;
	}
	{
		/* 重发一条并直接取包核对截断标志与前缀长度。 */
		xnetudppacket* pPacket = NULL;
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		if ( xrtNetUdpSend(pClient, Big, sizeof(Big)) !=
			 XNET_RESULT_OK ) {
			goto Cleanup;
		}
		while ( (pPacket = xrtNetUdpReceive(pServer)) == NULL ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				break;
			}
			xrtThreadYield();
		}
		if ( (pPacket == NULL) || !xrtNetUdpPacketTruncated(pPacket) ||
			 (xrtNetUdpPacketSize(pPacket) != 16u) ) {
			xrtNetUdpPacketDestroy(pPacket);
			iResult = 5;
			goto Cleanup;
		}
		printf("batch: truncated prefix=%zu of %zu\n",
			xrtNetUdpPacketSize(pPacket), sizeof(Big));
		xrtNetUdpPacketDestroy(pPacket);
	}

	/* 拉取批量：三条齐发，等队列到齐后一次 ReceiveBatch 全取。 */
	for ( i = 0; i < 3u; ++i ) {
		if ( xrtNetUdpSend(pClient, "bb", 2) != XNET_RESULT_OK ) {
			goto Cleanup;
		}
	}
	memset(pPackets, 0, sizeof(pPackets));
	{
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		while ( xrtNetUdpQueued(pServer) < 3u ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 6;
				goto Cleanup;
			}
			xrtThreadYield();
		}
		iCount = xrtNetUdpReceiveBatch(pServer, pPackets, 8);
		if ( iCount != 3u ) {
			iResult = 6;
			goto Cleanup;
		}
	}
	printf("pull batches: %zu packets in 1 batch\n", iCount);
	for ( i = 0; i < iCount; ++i ) {
		xrtNetUdpPacketDestroy(pPackets[i]);
	}

	/* 阻塞批量：两条齐发，BatchPacket 借用 + BatchTake 转移。 */
	for ( i = 0; i < 2u; ++i ) {
		if ( xrtNetUdpSend(pClient, "ww", 2) != XNET_RESULT_OK ) {
			goto Cleanup;
		}
	}
	{
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		while ( xrtNetUdpQueued(pServer) < 2u ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 7;
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}
	pBatch = xrtNetUdpReceiveBatchWait(pServer, 4,
		xrtDeadlineAfter(3000000u), NULL);
	if ( (pBatch == NULL) || (xrtNetUdpBatchCount(pBatch) < 1u) ) {
		iResult = 7;
		goto Cleanup;
	}
	{
		xnetudppacket* pBorrowed = xrtNetUdpBatchPacket(pBatch, 0);
		xnetudppacket* pTaken = xrtNetUdpBatchTake(pBatch,
			xrtNetUdpBatchCount(pBatch) - 1u);

		if ( (pBorrowed == NULL) || (pTaken == NULL) ||
			 (xrtNetUdpPacketSize(pBorrowed) != 2u) ||
			 (xrtNetUdpBatchPacket(pBatch,
				xrtNetUdpBatchCount(pBatch) - 1u) != NULL) ) {
			xrtNetUdpPacketDestroy(pTaken);
			iResult = 8;
			goto Cleanup;
		}
		printf("wait batch: %zu packets (borrow=take=ok)\n",
			xrtNetUdpBatchCount(pBatch));
		xrtNetUdpPacketDestroy(pTaken);
	}
	xrtNetUdpBatchDestroy(pBatch);
	pBatch = NULL;

	/* Future 批量：两条齐发 + 双引用把包与批量带出生命周期。 */
	for ( i = 0; i < 2u; ++i ) {
		if ( xrtNetUdpSend(pClient, "ff", 2) != XNET_RESULT_OK ) {
			goto Cleanup;
		}
	}
	{
		xdeadline iDeadline = xrtDeadlineAfter(3000000u);

		while ( xrtNetUdpQueued(pServer) < 2u ) {
			if ( xrtDeadlineExpired(iDeadline) ) {
				iResult = 9;
				goto Cleanup;
			}
			xrtThreadYield();
		}
	}
	pFuture = xrtNetUdpReceiveBatchAsync(pServer, 4);
	if ( !exampleFutureWait(pFuture) ) {
		iResult = 9;
		goto Cleanup;
	}
	pBatch = (xnetudpbatch*)xrtFutureValue(pFuture);
	if ( (pBatch == NULL) || (xrtNetUdpBatchCount(pBatch) < 1u) ) {
		iResult = 10;
		goto Cleanup;
	}
	/* FutureValue 是借用——Future 持有那份引用；要跨 Future 保留
	 * 必须自己 Ref（包与整批各一份），不能替 Future 释放。 */
	pKept = xrtNetUdpPacketRef(xrtNetUdpBatchPacket(pBatch, 0));
	pBatchRef = xrtNetUdpBatchRef(pBatch);
	if ( (pKept == NULL) || (pBatchRef == NULL) ) {
		iResult = 11;
		goto Cleanup;
	}
	printf("future batch: %zu packets, ref kept size=%zu\n",
		xrtNetUdpBatchCount(pBatch),
		xrtNetUdpPacketSize(pKept));
	/* 先销毁 Future（释放其引用）：两份 Ref 让包与批量继续存活。 */
	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	if ( (xrtNetUdpBatchCount(pBatchRef) < 1u) ||
		 (xrtNetUdpPacketSize(pKept) != 2u) ) {
		iResult = 12;
		goto Cleanup;
	}
	xrtNetUdpPacketDestroy(pKept);
	pKept = NULL;
	xrtNetUdpBatchDestroy(pBatchRef);
	pBatchRef = NULL;
	pBatch = NULL;

	/* 错误队列空路径：拉取 NULL/0 + Future 由关闭终结。 */
	{
		xnetudperrorpacket* pErrors[4];

		printf("error queue: empty pull=%zu/%zu",
			(size_t)(xrtNetUdpReceiveError(pServer) != NULL ?
				1u : 0u),
			xrtNetUdpReceiveErrorBatch(pServer, pErrors, 4));
	}
	pErrorFuture = xrtNetUdpReceiveErrorAsync(pServer);
	(void)xrtNetUdpAbort(pServer);
	if ( !exampleWaitState(pServer, XNET_UDP_CLOSED) ) {
		iResult = 13;
		goto Cleanup;
	}
	/* 关闭使未决错误 Future 进入终态（RESOLVED=1..CLOSED=4）。 */
	(void)xrtFutureWaitFor(pErrorFuture, UINT64_C(3000000));
	printf(", async terminal=%d\n",
		(int)xrtFutureState(pErrorFuture));

	/* 发送条件：空闲队列立即可写（同步 + Future 双形态）。 */
	printf("writable: sync=%d",
		xrtNetUdpWritable(pClient, 64, xrtDeadlineAfter(3000000u),
			NULL) ? 1 : 0);
	pWritable = xrtNetUdpWritableAsync(pClient, 64);
	printf(" async=%d\n", exampleFutureWait(pWritable) ? 1 : 0);
	iResult = 0;

Cleanup:
	xrtFutureDestroy(pWritable);
	xrtFutureDestroy(pErrorFuture);
	xrtFutureDestroy(pFuture);
	xrtNetUdpBatchDestroy(pBatch);
	xrtNetUdpBatchDestroy(pBatchRef);
	xrtNetUdpPacketDestroy(pKept);
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
		iResult = 14;
	}
	return iResult;
}
