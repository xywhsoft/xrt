/*
 * 范例：network/udp_introspect —— UDP 自省族全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【打开形态】  xrtNetUdpOpen（local + peer 同时给出 → 连接式）
 *   【引用】      xrtNetUdpRef / Destroy 配对
 *   【对端】      xrtNetUdpConnected / xrtNetUdpPeer
 *   【Worker 族】 xrtNetUdpWorker（借用 Worker）
 *                  xrtNetUdpSocket / xrtNetUdpSetData（仅 Worker 内合法，
 *                  经 xrtNetPost 投递到所属 Worker 执行）
 *   【用户数据】  xrtNetUdpData（任意线程原子读取）
 *   【终态错误】  xrtNetUdpError（正常关闭后为空）
 *   【队列观测】  xrtNetUdpQueued / QueuedBytes / QueuedErrors /
 *                  QueuedErrorBytes（拉取队列四视角）
 *   【PMTU】      xrtNetUdpPathMtu（无错误队列确认时为零）
 *   【统计】      xrtNetUdpStats（无锁并发快照，关闭后仍可读）
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_introspect/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   introspect: open(local+peer) connected=1
 *   worker-only: socket+set-data+data = ok
 *   queues: rx=0/0 err=0/0 mtu=0 terminal=(none)
 *   stats: state=1 sent=1 received=1 queued=0
 *
 * Socket/SetData 的"仅 Worker 内"约束由实现强制——
 *   主线程直接调用会得到 STATE 错误；正确入口是
 *   xrtNetPost 把任务投递到 UDP 所属 Worker。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 自省任务上下文：跨线程收集 Worker 内调用的结果。 */
typedef struct exampletask {
	xnetudp* pUdp;
	volatile bool bDone;
	bool bSocketOk;
	bool bSetDataOk;
	bool bWorkerDataOk;
} exampletask;



/* 文件级标签：Worker 任务与主线程核对同一对象。 */
static uint64 g_Tag;



/* Worker 内任务：Socket/SetData 只在该线程合法。 */
static void exampleWorkerTask(xnetworker* pWorker, ptr pData)
{
	exampletask* pTask = (exampletask*)pData;

	(void)pWorker;
	pTask->bSocketOk = xrtNetUdpSocket(pTask->pUdp) != NULL;
	pTask->bSetDataOk = xrtNetUdpSetData(pTask->pUdp, &g_Tag);
	pTask->bWorkerDataOk = xrtNetUdpData(pTask->pUdp) == &g_Tag;
	pTask->bDone = true;
}



/* 在截止时间内轮询条件。 */
static bool exampleSpinUntil(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(3000000u);

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 单端自省：Open 连接式 + Worker 任务 + 队列/统计快照。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pServer = NULL;
	xnetudp* pUdp = NULL;
	xnetudp* pRef = NULL;
	xnetpost Post;
	exampletask Task;
	xnetaddr Local;
	xnetaddr Peer;
	xnetudpstats Stats;
	xdeadline iDeadline;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetUdpConfigInit(&UdpConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	(void)xrtNetAddrLoopback(&Local, XNET_FAMILY_IPV4, 0);
	pServer = xrtNetUdpBind(pEngine, &Local, 0, &UdpConfig,
		NULL, NULL);
	if ( (pServer == NULL) || !xrtNetUdpLocal(pServer, &Peer) ) {
		goto Cleanup;
	}
	pUdp = xrtNetUdpOpen(pEngine, &Local, &Peer, 0, &UdpConfig,
		NULL, NULL);
	if ( pUdp == NULL ) {
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtNetUdpState(pUdp) != XNET_UDP_OPEN ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			goto Cleanup;
		}
		xrtThreadYield();
	}
	printf("introspect: open(local+peer) connected=%d\n",
		xrtNetUdpConnected(pUdp) ? 1 : 0);
	if ( !xrtNetUdpPeer(pUdp, &Peer) || !xrtNetUdpConnected(pUdp) ) {
		iResult = 2;
		goto Cleanup;
	}

	/* Ref：增引用返回原指针，与 Destroy 配对平衡。 */
	pRef = xrtNetUdpRef(pUdp);
	if ( pRef != pUdp ) {
		iResult = 3;
		goto Cleanup;
	}

	/* Worker 族：Post 到所属 Worker 执行 Socket / SetData。 */
	memset(&Task, 0, sizeof(Task));
	Task.pUdp = pUdp;
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetUdpWorker(pUdp), &Post, exampleWorkerTask, &Task) ||
		 !exampleSpinUntil(&Task.bDone) ) {
		iResult = 4;
		goto Cleanup;
	}
	printf("worker-only: socket+set-data+data = ");
	if ( Task.bSocketOk && Task.bSetDataOk && Task.bWorkerDataOk &&
		 (xrtNetUdpData(pUdp) == &g_Tag) ) {
		printf("ok\n");
	}
	else {
		printf("FAIL\n");
		iResult = 5;
		goto Cleanup;
	}

	/* 客户端发一条、服务端回一条，让两端统计都有真实数据。 */
	if ( (xrtNetUdpSend(pUdp, "ping", 4) != XNET_RESULT_OK) ||
		 !xrtNetUdpLocal(pUdp, &Local) ||
		 (xrtNetUdpSendTo(pServer, &Local, "pong", 4) !=
		  XNET_RESULT_OK) ) {
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtNetUdpQueued(pUdp) == 0 ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			goto Cleanup;
		}
		xrtThreadYield();
	}
	{
		xnetudppacket* pPacket = xrtNetUdpReceive(pUdp);

		if ( (pPacket == NULL) || (xrtNetUdpPacketSize(pPacket) != 4) ) {
			iResult = 6;
			goto Cleanup;
		}
		xrtNetUdpPacketDestroy(pPacket);
	}
	printf("queues: rx=%zu/%zu err=%zu/%zu mtu=%zu terminal=%s\n",
		xrtNetUdpQueued(pUdp), xrtNetUdpQueuedBytes(pUdp),
		xrtNetUdpQueuedErrors(pUdp), xrtNetUdpQueuedErrorBytes(pUdp),
		xrtNetUdpPathMtu(pUdp),
		xrtNetUdpError(pUdp) == NULL ? "(none)" : "err");

	if ( !xrtNetUdpStats(pUdp, &Stats) ||
		 (Stats.SentPackets < 1u) || (Stats.ReceivedPackets < 1u) ) {
		iResult = 7;
		goto Cleanup;
	}
	printf("stats: state=%d sent=%llu received=%llu queued=%zu\n",
		(int)Stats.State,
		(unsigned long long)Stats.SentPackets,
		(unsigned long long)Stats.ReceivedPackets,
		Stats.QueuedPackets);

	/* 正常关闭：Error 仍为空，统计在关闭后依旧可读。 */
	if ( !xrtNetUdpClose(pUdp) ) {
		iResult = 8;
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	while ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			iResult = 9;
			goto Cleanup;
		}
		xrtThreadYield();
	}
	if ( (xrtNetUdpError(pUdp) != NULL) ||
		 !xrtNetUdpStats(pUdp, &Stats) ||
		 (Stats.State != XNET_UDP_CLOSED) ) {
		iResult = 10;
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	xrtNetUdpDestroy(pRef);
	xrtNetUdpDestroy(pUdp);
	if ( pServer != NULL ) {
		xdeadline iEnd = xrtDeadlineAfter(3000000u);

		(void)xrtNetUdpAbort(pServer);
		while ( xrtNetUdpState(pServer) != XNET_UDP_CLOSED ) {
			if ( xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetUdpDestroy(pServer);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 11;
	}
	return iResult;
}
