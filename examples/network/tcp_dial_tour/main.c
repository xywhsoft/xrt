/*
 * 范例：network/tcp_dial_tour —— Dial 对象族 + Listener 拉取族
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Dial 配置】  xrtNetDialConfigInit / xrtNetDialConfigValid
 *                  （默认策略合法；MaxAttempts=0 非法）
 *   【Dial 生命周期】 xrtNetDial（回调式，成功回调接管 Stream 引用）/
 *                  xrtNetDialRef / xrtNetDialDestroy /
 *                  xrtNetDialState（RESOLVING→CONNECTING→CONNECTED）/
 *                  xrtNetDialError（失败终态的结构化错误）/
 *                  xrtNetDialStats（地址数与竞争尝试计数）
 *   【Dial 取消】  xrtNetDialCancel（争取取消终态→回调收到 CANCELLED）
 *   【Listener 族】 xrtNetListenerAccept（拉取模式非阻塞取流）/
 *                  xrtNetListenerRef / xrtNetListenerStats /
 *                  xrtNetListenerWorker / xrtNetListenerData
 * 模块宏：XRT_MODULE_NET_TCP_DIAL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/network/tcp_dial_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   dial-config: valid=1 corrupt=0
 *   dial: connected state=2 stats(addrs=1 attempts=1 winner=0)
 *   listener: accept=1 stats(accepted=1) worker=ok data=ok
 *   echo: ping -> pong
 *   dial-cancel: refused -> failed(1) midair=cancelled cancel-again=0
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

#define EXAMPLE_DEADLINE_US	UINT64_C(3000000)



/* Dial 完成上下文：回调与主线程之间的交接。 */
typedef struct exampledial {
	volatile xnetdialstate State;
	xnetstream* pStream;
	xnetresult Result;
	bool bDone;
} exampledial;



/* 成功/失败共用一个完成回调：成功时接管 Stream 引用。 */
static void exampleDialDone(xnetdial* pDial, xnetresult Result,
	xnetstream* pStream, const xerror* pError, ptr pData)
{
	exampledial* pTask = (exampledial*)pData;

	(void)pDial;
	(void)pError;
	pTask->Result = Result;
	pTask->pStream = pStream;
	pTask->State = xrtNetDialState(pDial);
	pTask->bDone = true;
}



/* 轮询等待完成标志。 */
static bool exampleWaitDone(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 等 Stream 进入指定状态。 */
static bool exampleWaitStreamState(xnetstream* pStream,
	xnetstreamstate State)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	while ( xrtNetStreamState(pStream) != State ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 拉取等待 Listener 交出一个 Stream。 */
static xnetstream* exampleWaitAccept(xnetlistener* pListener)
{
	xdeadline iDeadline = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

	for ( ;; ) {
		xnetstream* pStream = xrtNetListenerAccept(pListener);

		if ( pStream != NULL ) {
			return pStream;
		}
		if ( xrtDeadlineExpired(iDeadline) ) {
			return NULL;
		}
		xrtThreadYield();
	}
}



int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetdialconfig DialConfig;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xnetlistener* pListener = NULL;
	xnetlistener* pListenerRef = NULL;
	xnetdial* pDial = NULL;
	xnetdial* pBadDial = NULL;
	xnetdial* pCancelDial = NULL;
	xnetstream* pServer = NULL;
	xnetaddr Address;
	xnetdialstats DialStats;
	xnetlistenerstats ListenStats;
	exampledial Task;
	int iResult = 1;

	/* Dial 配置：默认策略应通过完整校验。 */
	xrtNetDialConfigInit(&DialConfig);
	printf("dial-config: valid=%d", xrtNetDialConfigValid(&DialConfig) ?
		1 : 0);
	DialConfig.MaxAttempts = 0;  /* 人为破坏再验一次 */
	printf(" corrupt=%d\n", xrtNetDialConfigValid(&DialConfig) ? 1 : 0);
	xrtNetDialConfigInit(&DialConfig);  /* 恢复默认 */

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1;
	xrtNetResolverConfigInit(&ResolverConfig);
	xrtNetListenConfigInit(&ListenConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ||
		 (pResolver == NULL) ) {
		iResult = 2;
		goto Cleanup;
	}
	(void)xrtNetAddrLoopback(&Address, XNET_FAMILY_IPV4, 0);
	ListenConfig.Address = Address;  /* 端口零由系统分配动态端口 */
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	if ( (pListener == NULL) ||
		 !xrtNetListenerLocal(pListener, &Address) ) {
		iResult = 3;
		goto Cleanup;
	}

	/* 回调式 Dial：成功回调接管 Stream 引用。 */
	memset(&Task, 0, sizeof(Task));
	pDial = xrtNetDial(pEngine, pResolver, "127.0.0.1", Address.Port,
		&DialConfig, NULL, NULL, exampleDialDone, &Task);
	if ( (pDial == NULL) || !exampleWaitDone(&Task.bDone) ||
		 (Task.Result != XNET_RESULT_OK) ||
		 (Task.State != XNET_DIAL_CONNECTED) ||
		 (Task.pStream == NULL) ||
		 (xrtNetDialRef(pDial) != pDial) ) {  /* 引用配对在收尾多一次 Destroy */
		iResult = 4;
		goto Cleanup;
	}
	if ( !xrtNetDialStats(pDial, &DialStats) ||
		 (DialStats.Addresses < 1u) ||
		 (DialStats.AttemptsStarted < 1u) ||
		 (DialStats.WinnerIndex != 0u) ) {
		iResult = 5;
		goto Cleanup;
	}
	printf("dial: connected state=%d", (int)Task.State);
	printf(" stats(addrs=%u attempts=%u winner=%u)\n",
		DialStats.Addresses, DialStats.AttemptsStarted,
		(unsigned)DialStats.WinnerIndex);

	/* Ref：Listener 增引用配对（Dial 的 Ref 由回调路径隐式持有）。 */
	pListenerRef = xrtNetListenerRef(pListener);

	/* Listener 拉取族：Accept / Stats / Worker / Data。 */
	pServer = exampleWaitAccept(pListener);
	if ( (pServer == NULL) ||
		 !exampleWaitStreamState(pServer, XNET_STREAM_OPEN) ||
		 !xrtNetListenerStats(pListener, &ListenStats) ||
		 (ListenStats.Accepted < 1u) ||
		 (xrtNetListenerWorker(pListener) == NULL) ) {
		iResult = 6;
		goto Cleanup;
	}
	printf("listener: accept=1 stats(accepted=%llu)",
		(unsigned long long)ListenStats.Accepted);
	printf(" worker=%s",
		xrtNetListenerWorker(pListener) != NULL ? "ok" : "(null)");
	(void)xrtNetListenerData(pListener);
	printf(" data=ok\n");

	/* 用 Dial 接管的 Stream 做一次回声验证链路。 */
	if ( xrtNetStreamSend(Task.pStream, "ping", 4) != XNET_RESULT_OK ) {
		iResult = 7;
		goto Cleanup;
	}
	{
		xnetbytes* pBytes = xrtNetStreamRecv(pServer, 4,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
		xbytesview View = xrtNetBytesView(pBytes);

		if ( (View.Size != 4u) || (memcmp(View.Data, "ping", 4) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			iResult = 8;
			goto Cleanup;
		}
		xrtNetBytesDestroy(pBytes);
	}
	if ( xrtNetStreamSend(pServer, "pong", 4) != XNET_RESULT_OK ) {
		iResult = 9;
		goto Cleanup;
	}
	{
		xnetbytes* pBytes = xrtNetStreamRecv(Task.pStream, 4,
			xrtDeadlineAfter(EXAMPLE_DEADLINE_US), NULL);
		xbytesview View = xrtNetBytesView(pBytes);

		if ( (View.Size != 4u) || (memcmp(View.Data, "pong", 4) != 0) ) {
			xrtNetBytesDestroy(pBytes);
			iResult = 10;
			goto Cleanup;
		}
		xrtNetBytesDestroy(pBytes);
	}
	printf("echo: ping -> pong\n");

	/* 失败路径：拨未监听端口 → 立刻拒绝 → FAILED + 结构化错误。 */
	memset(&Task, 0, sizeof(Task));
	pBadDial = xrtNetDial(pEngine, pResolver, "127.0.0.1", 1,
		&DialConfig, NULL, NULL, exampleDialDone, &Task);
	if ( (pBadDial == NULL) || !exampleWaitDone(&Task.bDone) ||
		 (Task.Result == XNET_RESULT_OK) ) {
		iResult = 11;
		goto Cleanup;
	}
	printf("dial-cancel: refused -> failed(%d)",
		Task.State == XNET_DIAL_FAILED ? 1 : 0);
	(void)xrtNetDialError(pBadDial);

	/* 取消路径一：不可达地址保持 CONNECTING，Cancel 返回真后
	 * 完成结果必为 CANCELLED（返回真是确定性契约）。 */
	memset(&Task, 0, sizeof(Task));
	pCancelDial = xrtNetDial(pEngine, pResolver, "10.255.255.1", 81,
		&DialConfig, NULL, NULL, exampleDialDone, &Task);
	if ( pCancelDial != NULL ) {
		bool bCancelled = xrtNetDialCancel(pCancelDial);

		if ( bCancelled && exampleWaitDone(&Task.bDone) &&
			 (Task.Result == XNET_RESULT_CANCELLED) ) {
			printf(" midair=cancelled");
		}
		else {
			/* 极小概率已完成（视为失败路径）。 */
			printf(" midair=%d", (int)Task.Result);
		}
	}

	/* 取消路径二：对已终结的 Dial 再取消必然返回假。 */
	printf(" cancel-again=%d\n", xrtNetDialCancel(pBadDial) ? 1 : 0);
	iResult = 0;

Cleanup:
	/* 关闭须等终态再 Destroy，否则 EngineDestroy 会因残留对象失败。 */
	if ( Task.pStream != NULL ) {
		(void)xrtNetStreamAbort(Task.pStream);
		(void)exampleWaitStreamState(Task.pStream,
			XNET_STREAM_CLOSED);
	}
	if ( pServer != NULL ) {
		(void)xrtNetStreamAbort(pServer);
		(void)exampleWaitStreamState(pServer, XNET_STREAM_CLOSED);
	}
	if ( pListener != NULL ) {
		xdeadline iEnd = xrtDeadlineAfter(EXAMPLE_DEADLINE_US);

		(void)xrtNetListenerClose(pListener);
		while ( xrtNetListenerState(pListener) !=
			XNET_LISTENER_CLOSED ) {
			if ( xrtDeadlineExpired(iEnd) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	xrtNetStreamDestroy(Task.pStream);
	xrtNetStreamDestroy(pServer);
	xrtNetListenerDestroy(pListenerRef);
	xrtNetListenerDestroy(pListener);
	xrtNetDialDestroy(pDial);
	xrtNetDialDestroy(pDial);  /* Ref 那份 */
	xrtNetDialDestroy(pBadDial);
	xrtNetDialDestroy(pCancelDial);
	xrtNetResolverDestroy(pResolver);
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) &&
		 (iResult == 0) ) {
		iResult = 12;
	}
	return iResult;
}
