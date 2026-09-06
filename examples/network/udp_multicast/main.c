/*
 * 范例：network/udp_multicast —— 多播成员族（Worker 内调用范式）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【成员管理】  xrtNetUdpJoin / xrtNetUdpLeave
 *   【多播选项】  xrtNetUdpMulticastLoop（回环开关）
 *                  xrtNetUdpMulticastHopLimit（TTL 跳数）
 *                  xrtNetUdpMulticastInterface（出接口选择，
 *                  空接口恢复系统默认）
 *   【Worker 范式】以上均"只在 UDP Worker 内合法"——
 *                  经 xrtNetUdpWorker + xrtNetPost 投递执行
 * 模块宏：XRT_MODULE_NET_UDP
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/udp_multicast/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   multicast: group=239.255.0.1:NNNNN
 *   join: loop+hop+if+group = ok
 *   self-received: mcast-loop
 *   leave: group = ok
 *
 * 单套接字自回环的成功配方（Windows 实测）：
 *   通配地址绑定（0.0.0.0）才能收到任意接口上的多播；
 *   Join 与 MulticastInterface 都指向环回接口，
 *   再开 Loop 回环——协议栈把发往组的数据报回送给自己，
 *   无需第二个成员即可验证 Join 生效。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_GROUP_TEXT "239.255.0.1"



/* Worker 任务上下文：跨线程收集多播调用结果。 */
typedef struct examplemcast {
	xnetudp* pUdp;
	xnetaddr Group;
	xnetaddr Iface;
	volatile bool bDone;
	bool bLoop;
	bool bHop;
	bool bIface;
	bool bJoin;
	bool bLeave;
} examplemcast;



/* Worker 内任务一：设置三个多播选项并加入组。 */
static void exampleJoinTask(xnetworker* pWorker, ptr pData)
{
	examplemcast* pTask = (examplemcast*)pData;

	(void)pWorker;
	pTask->bLoop = xrtNetUdpMulticastLoop(pTask->pUdp, true);
	pTask->bHop = xrtNetUdpMulticastHopLimit(pTask->pUdp, 1);
	pTask->bIface = xrtNetUdpMulticastInterface(pTask->pUdp,
		&pTask->Iface);
	pTask->bJoin = xrtNetUdpJoin(pTask->pUdp, &pTask->Group,
		&pTask->Iface);
	pTask->bDone = true;
}



/* Worker 内任务二：离开组。 */
static void exampleLeaveTask(xnetworker* pWorker, ptr pData)
{
	examplemcast* pTask = (examplemcast*)pData;

	(void)pWorker;
	pTask->bLeave = xrtNetUdpLeave(pTask->pUdp, &pTask->Group, NULL);
	pTask->bDone = true;
}



/* 在截止时间内轮询标志。 */
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



/* 单套接字多播自回环：加入组 → 发往组 → 收到自己 → 离开组。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetudpconfig UdpConfig;
	xnetengine* pEngine = NULL;
	xnetudp* pUdp = NULL;
	examplemcast Task;
	xnetpost Post;
	xnetudppacket* pPacket = NULL;
	xdeadline iDeadline;
	uint16 iPort;
	int iResult = 1;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetUdpConfigInit(&UdpConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	/* 绑定用通配地址（可收任意接口多播），出接口/加入用环回。 */
	if ( !xrtNetAddrAny(&Task.Iface, XNET_FAMILY_IPV4, 0) ||
		 !xrtNetAddrParse(&Task.Group, EXAMPLE_GROUP_TEXT, 0) ) {
		iResult = 2;
		goto Cleanup;
	}
	pUdp = xrtNetUdpBind(pEngine, &Task.Iface, 0, &UdpConfig,
		NULL, NULL);  /* Iface 此刻是 0.0.0.0:any 通配 */
	if ( (pUdp == NULL) || !xrtNetUdpLocal(pUdp, &Task.Iface) ) {
		iResult = 3;
		goto Cleanup;
	}
	iPort = Task.Iface.Port;
	Task.Group.Port = iPort;
	(void)xrtNetAddrLoopback(&Task.Iface, XNET_FAMILY_IPV4, 0);
	Task.pUdp = pUdp;
	Task.bDone = false;
	printf("multicast: group=%s:%u\n", EXAMPLE_GROUP_TEXT,
		(unsigned)iPort);

	/* 任务一（Worker 内）：Loop / HopLimit / Interface / Join。 */
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetUdpWorker(pUdp), &Post, exampleJoinTask, &Task) ||
		 !exampleSpinUntil(&Task.bDone) ) {
		iResult = 4;
		goto Cleanup;
	}
	printf("join: loop+hop+if+group = ");
	if ( Task.bLoop && Task.bHop && Task.bIface && Task.bJoin ) {
		printf("ok\n");
	}
	else {
		printf("unavailable on this platform\n");
		iResult = 0;  /* 能力探测失败不算范例失败 */
		goto Cleanup;
	}

	/* 发往组地址：回环开启时协议栈回送给自己。 */
	if ( xrtNetUdpSendTo(pUdp, &Task.Group, "mcast-loop", 10) !=
		 XNET_RESULT_OK ) {
		goto Cleanup;
	}
	iDeadline = xrtDeadlineAfter(3000000u);
	for ( ;; ) {
		pPacket = xrtNetUdpReceive(pUdp);
		if ( pPacket != NULL ) {
			break;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			break;
		}
		xrtThreadYield();
	}
	if ( (pPacket != NULL) &&
		 (xrtNetUdpPacketSize(pPacket) == 10) &&
		 (memcmp(xrtNetUdpPacketData(pPacket), "mcast-loop",
			10) == 0) ) {
		printf("self-received: %.*s\n",
			(int)xrtNetUdpPacketSize(pPacket),
			(cstr)xrtNetUdpPacketData(pPacket));
	}
	else {
		/* 个别平台不回送环回多播：成员管理已验证，如实报告。 */
		printf("self-receive: not looped back on this platform\n");
	}
	xrtNetUdpPacketDestroy(pPacket);
	pPacket = NULL;

	/* 任务二（Worker 内）：Leave。 */
	Task.bDone = false;
	if ( !xrtNetPostInit(&Post) || !xrtNetPost(
		xrtNetUdpWorker(pUdp), &Post, exampleLeaveTask, &Task) ||
		 !exampleSpinUntil(&Task.bDone) ) {
		iResult = 5;
		goto Cleanup;
	}
	printf("leave: group = %s\n", Task.bLeave ? "ok" : "fail");
	iResult = Task.bLeave ? 0 : 6;

Cleanup:
	if ( pUdp != NULL ) {
		xdeadline iEnd = xrtDeadlineAfter(3000000u);

		(void)xrtNetUdpAbort(pUdp);
		while ( xrtNetUdpState(pUdp) != XNET_UDP_CLOSED ) {
			if ( xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetUdpDestroy(pUdp);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 7;
	}
	return iResult;
}
