/*
 * 范例：process/signal_tour —— 信号族补集（元数据/Owned/Once/收计数）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【元数据】  xrtSignalSupported / Name / Healthy
 *   【订阅族】  xrtSignalOnOwned（数据析构器）/ Once /
 *              OnceOwned（一次性双形态）/ Ref / Off / Active / Code
 *   【投递观测】 xrtSignalRaise / Count / Received / Clear
 *   【忽略恢复】 xrtSignalIgnore / Restore / RestoreAll
 * 模块宏：XRT_MODULE_SIGNAL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/process/signal_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   signal: supported=1/1 name=INT healthy=1
 *   signal: owned-on fired=1 destroyed=1
 *   signal: once fired=2 destroyed=1
 *   signal: count/received/clear ok
 *   signal: ignore/restore/all ok
 *
 * INT 在 Windows 控制台受支持、TERM 不受支持（探测两向）；
 *   Owned 形态的析构器在句柄最终释放时恰好执行一次。
 */

#include <stdio.h>
#include <xrt.h>

static volatile int g_Destroyed = 0;
static volatile int g_Fired = 0;

static void exampleFree(ptr pData)
{
	(void)pData;
	g_Destroyed = g_Destroyed + 1;
}

static void exampleOnCallback(xsignalwatch* pWatch,
	const xsignalevent* pEvent, ptr pData)
{
	(void)pWatch;
	(void)pEvent;
	g_Fired = g_Fired + 1;
}

static void exampleOnceCallback(xsignalwatch* pWatch,
	const xsignalevent* pEvent, ptr pData)
{
	(void)pWatch;
	(void)pEvent;
	g_Fired = g_Fired + 1;
}

static bool exampleSpinFlag(volatile int* pFlag, int iExpect)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( *pFlag < iExpect ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

int main(void)
{
	xsignalwatch* pOwned = NULL;
	xsignalwatch* pOnce = NULL;
	xsignalwatch* pOnceOwned = NULL;
	xsignalwatch* pRef = NULL;
	int iResult = 1;

	/* ---- 元数据：Supported 正反 + Name + Healthy ---- */
	if ( !xrtSignalSupported(XSIGNAL_INT) ||
		!xrtSignalSupported(XSIGNAL_TERM) ||  /* Windows 无 TERM */
		(xrtSignalName(XSIGNAL_INT) == NULL) ||
		(xrtSignalName(XSIGNAL_NONE) == NULL) ||
		!xrtSignalHealthy() ) {
		goto Cleanup;
	}
	printf("signal: supported=1/1 name=%s healthy=1\n",
		xrtSignalName(XSIGNAL_INT));

	/* ---- OnOwned：投递一次并等回调 + 析构恰好一次 ---- */
	g_Fired = 0;
	g_Destroyed = 0;
	pOwned = xrtSignalOnOwned(XSIGNAL_INT, exampleOnCallback,
		NULL, exampleFree);
	if ( (pOwned == NULL) ||
		!xrtSignalActive(pOwned) ||
		(xrtSignalCode(pOwned) != XSIGNAL_INT) ) {
		goto Cleanup;
	}
	/* Ref：增引用（Off 后再 Free 一次配平）。 */
	pRef = xrtSignalRef(pOwned);
	if ( (pRef != pOwned) || !xrtSignalActive(pRef) ) {
		goto Cleanup;
	}
	if ( !xrtSignalRaise(XSIGNAL_INT) ||
		!exampleSpinFlag(&g_Fired, 1) ) {
		goto Cleanup;
	}
	if ( !xrtSignalOff(pOwned) ||
		xrtSignalActive(pOwned) ) {
		goto Cleanup;
	}
	/* 析构器只在最后一个引用释放时执行一次：
	 * 先释放 OnOwned 那份（Ref 仍持有 → 不析构），
	 * 再释放 Ref 那份（最后一个 → 恰好析构一次）。 */
	xrtSignalFree(pOwned);
	pOwned = NULL;
	if ( g_Destroyed != 0 ) {
		goto Cleanup;
	}
	xrtSignalFree(pRef);
	pRef = NULL;
	if ( g_Destroyed != 1 ) {
		goto Cleanup;
	}
	printf("signal: owned-on fired=%d destroyed=%d\n", g_Fired,
		g_Destroyed);

	/* ---- Once：第一次投递触发后自动注销 ---- */
	g_Fired = 0;
	pOnce = xrtSignalOnce(XSIGNAL_INT, exampleOnceCallback, NULL);
	if ( (pOnce == NULL) ||
		!xrtSignalActive(pOnce) ||
		!xrtSignalRaise(XSIGNAL_INT) ||
		!exampleSpinFlag(&g_Fired, 1) ) {
		goto Cleanup;
	}
	xrtSignalFree(pOnce);
	pOnce = NULL;
	/* OnceOwned：一次性 + 析构器（fired 从上一段的 1 续计）。 */
	g_Destroyed = 0;
	pOnceOwned = xrtSignalOnceOwned(XSIGNAL_INT,
		exampleOnceCallback, NULL, exampleFree);
	if ( (pOnceOwned == NULL) ||
		!xrtSignalRaise(XSIGNAL_INT) ||
		!exampleSpinFlag(&g_Fired, 2) ) {  /* 上一 Once 累计 */
		goto Cleanup;
	}
	/* Once 在首次调度时已自动注销：无需再 Off，直接释放。 */
	xrtSignalFree(pOnceOwned);
	pOnceOwned = NULL;
	if ( g_Destroyed != 1 ) {
		goto Cleanup;
	}
	printf("signal: once fired=%d destroyed=%d\n", g_Fired,
		g_Destroyed);

	/* ---- Count / Received / Clear ---- */
	{
		uint64 iBefore = xrtSignalCount(XSIGNAL_INT);

		if ( !xrtSignalReceived(XSIGNAL_INT) ||
			(xrtSignalCount(XSIGNAL_INT) < 2u) ||
			(iBefore < 2u) ) {
			goto Cleanup;
		}
		if ( !xrtSignalClear(XSIGNAL_INT) ||
			xrtSignalReceived(XSIGNAL_INT) ||
			(xrtSignalCount(XSIGNAL_INT) != 0u) ) {
			goto Cleanup;
		}
	}
	printf("signal: count/received/clear ok\n");

	/* ---- Ignore / Restore / RestoreAll ---- */
	if ( !xrtSignalIgnore(XSIGNAL_INT) ||
		!xrtSignalRestore(XSIGNAL_INT) ||
		!xrtSignalIgnore(XSIGNAL_INT) ||
		!xrtSignalRestoreAll() ) {
		goto Cleanup;
	}
	printf("signal: ignore/restore/all ok\n");
	iResult = 0;

Cleanup:
	xrtSignalFree(pOnceOwned);
	xrtSignalFree(pOnce);
	xrtSignalFree(pRef);
	xrtSignalFree(pOwned);
	return iResult;
}
