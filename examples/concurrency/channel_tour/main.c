/*
 * 范例：concurrency/channel_tour —— 通道全接口（阻塞/取消/Select/协程）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【构造与自省】 xrtChannelInitBuffer（调用方存储的内嵌形态）/
 *                  TryRecv / Count / Capacity / IsClosed / IsDrained /
 *                  Reset
 *   【阻塞族】    xrtChannelSend / SendFor / SendUntil /
 *                  RecvFor / RecvUntil（线程内阻塞等待）
 *   【取消族】    SendCancel / SendForCancel / SendUntilCancel /
 *                  RecvForCancel / RecvUntilCancel（令牌中断挂起）
 *   【Select 族】 CaseSend / CaseRecv / SelectTry / SelectFor /
 *                  SelectUntil
 *   【协程族】    SendAwaitUntil / RecvAwaitFor / SelectAwaitFor
 *                  （挂起而非阻塞调度线程）
 * 模块宏：XRT_MODULE_CHANNEL（依赖 COROUTINE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/channel_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   channel: buffer count=1/2 recv=try-ok reset=ok
 *   channel: send/recv for+until = ok (timeout=1)
 *   channel: cancel send=2 recv=2
 *   channel: select try=1 for-timeout until=1
 *   channel: coroutine await for/until + select = 7 coroutines
 *
 * 容量 2 的有缓冲通道：超时路径用"满发/空收"制造；
 *   取消路径把堆令牌放进任务结构，等待线程挂起后由主线程
 *   Request 中断；协程族在调度器内挂起唤醒
 *   （RecvAwaitFor 到期返回 TIMEOUT）。
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <xrt.h>

#define EXAMPLE_TIMEOUT_US	UINT64_C(200000)

/* 取消任务上下文：堆令牌由主线程创建、等待线程传入挂起调用。 */
typedef struct examplecancel {
	xchannel* pChannel;
	xcancel* pCancel;
	volatile xwaitresult Result;
	volatile bool bDone;
} examplecancel;

static bool exampleSpinUntil(volatile bool* pFlag)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( !*pFlag ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}

/* 发送端线程：满通道上 SendForCancel 挂起直到令牌触发。 */
static int32 exampleSendWaiter(ptr pArg)
{
	examplecancel* pJob = (examplecancel*)pArg;

	pJob->Result = xrtChannelSendForCancel(pJob->pChannel, (ptr)1,
		UINT64_C(10000000), pJob->pCancel);
	pJob->bDone = true;
	return 0;
}

/* 接收端线程：空通道上 RecvUntilCancel 挂起直到令牌触发。 */
static int32 exampleRecvWaiter(ptr pArg)
{
	examplecancel* pJob = (examplecancel*)pArg;
	ptr pItem = NULL;

	pJob->Result = xrtChannelRecvUntilCancel(pJob->pChannel, &pItem,
		xrtDeadlineAfter(UINT64_C(10000000)), pJob->pCancel);
	pJob->bDone = true;
	return 0;
}

/* ---- 协程族 ---- */

/* 协程一：空通道 RecvAwaitFor 到期（返回 TIMEOUT）。 */
static ptr exampleCoRecvTimeout(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	ptr pItem = NULL;

	return (ptr)(uintptr_t)xrtChannelRecvAwaitFor(pChannel, &pItem,
		EXAMPLE_TIMEOUT_US);
}

/* 协程二：满通道 SendAwaitUntil 由接收协程唤醒。 */
static ptr exampleCoSendBlocked(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;

	return (ptr)(uintptr_t)xrtChannelSendAwaitUntil(pChannel,
		(ptr)77, xrtDeadlineAfter(UINT64_C(3000000)));
}

/* 协程三：SelectAwaitFor 在两条通道间选择（第二条预填）。 */
static ptr exampleCoSelect(ptr pData)
{
	xchannel* arrChannel = (xchannel*)pData;
	ptr pItem = NULL;
	xchannelcase Cases[2];
	xchannelselectresult Result;

	Cases[0] = xrtChannelCaseRecv(&arrChannel[0], &pItem);
	Cases[1] = xrtChannelCaseRecv(&arrChannel[1], &pItem);
	Result = xrtChannelSelectAwaitFor(Cases, 2u, UINT64_C(3000000));
	if ( Result.Wait == XWAIT_OK ) {
		return (ptr)(uintptr_t)(0x10 + Result.Index);
	}
	return (ptr)(uintptr_t)Result.Wait;
}

/* 协程四：满通道 SendAwaitFor 到期（TIMEOUT）。 */
static ptr exampleCoSendTimeout(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;

	return (ptr)(uintptr_t)xrtChannelSendAwaitFor(pChannel, (ptr)1,
		EXAMPLE_TIMEOUT_US);
}

/* 协程五：过期截止的 RecvAwaitUntil 立即返回 TIMEOUT。 */
static ptr exampleCoRecvExpired(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	ptr pItem = NULL;

	return (ptr)(uintptr_t)xrtChannelRecvAwaitUntil(pChannel, &pItem,
		xrtDeadlineAfter(UINT64_C(1)));
}

/* 协程六：SelectAwait 无限期版（预填 case0 保证完成）。 */
static ptr exampleCoSelectForever(ptr pData)
{
	xchannel* arrChannel = (xchannel*)pData;
	ptr pItem = NULL;
	xchannelcase Cases[2];
	xchannelselectresult Result;

	Cases[0] = xrtChannelCaseRecv(&arrChannel[0], &pItem);
	Cases[1] = xrtChannelCaseRecv(&arrChannel[1], &pItem);
	Result = xrtChannelSelectAwait(Cases, 2u);
	if ( Result.Wait == XWAIT_OK ) {
		return (ptr)(uintptr_t)(0x20 + Result.Index);
	}
	return (ptr)(uintptr_t)Result.Wait;
}

/* 协程七：SelectAwaitUntil 到期（专用空通道，无 case 就绪）。 */
static ptr exampleCoSelectExpired(ptr pData)
{
	xchannel* pEmpty = (xchannel*)pData;
	ptr pItem = NULL;
	xchannelcase Cases[2];
	xchannelselectresult Result;

	Cases[0] = xrtChannelCaseRecv(pEmpty, &pItem);
	Cases[1] = xrtChannelCaseSend(pEmpty, (ptr)1);
	Result = xrtChannelSelectAwaitUntil(Cases, 2u,
		xrtDeadlineAfter(UINT64_C(1)));
	return (ptr)(uintptr_t)Result.Wait;
}

int main(void)
{
	xchannel tEmbedded;
	xchannel tExpire;
	xchannel tFull;
	xchannel tSilent;
	ptr arrFullSlot[1];
	ptr arrSlots[2];
	xchannel* pHeap = NULL;
	xchannel* pEmpty = NULL;
	xthread* pSendThread = NULL;
	xthread* pRecvThread = NULL;
	examplecancel SendJob;
	examplecancel RecvJob;
	xcosched* pSched = NULL;
	xcoro* pCoTimeout = NULL;
	xcoro* pCoSend = NULL;
	xcoro* pCoSelect = NULL;
	xcoro* pCoSendTo = NULL;
	xcoro* pCoRecvEx = NULL;
	xcoro* pCoSelEver = NULL;
	xcoro* pCoSelEx = NULL;
	ptr pItem = NULL;
	xchannelcase Cases[2];
	xchannelselectresult Select;
	int iResult = 1;

	memset(&SendJob, 0, sizeof(SendJob));
	memset(&RecvJob, 0, sizeof(RecvJob));

	/* ---- 内嵌缓冲形态 + 自省 ---- */
	if ( !xrtChannelInitBuffer(&tEmbedded, arrSlots, 2u) ||
		(xrtChannelCapacity(&tEmbedded) != 2u) ) {
		goto Cleanup;
	}
	if ( (xrtChannelTrySend(&tEmbedded, (ptr)5) != XCHANNEL_OK) ||
		(xrtChannelCount(&tEmbedded) != 1u) ||
		(xrtChannelTryRecv(&tEmbedded, &pItem) != XCHANNEL_OK) ||
		(pItem != (ptr)5) ) {
		goto Cleanup;
	}
	xrtChannelClose(&tEmbedded);
	if ( !xrtChannelIsClosed(&tEmbedded) ||
		!xrtChannelIsDrained(&tEmbedded) ||
		!xrtChannelReset(&tEmbedded) ||
		xrtChannelIsClosed(&tEmbedded) ) {
		goto Cleanup;
	}
	printf("channel: buffer count=1/2 recv=try-ok reset=ok\n");
	xrtChannelUnit(&tEmbedded);

	/* ---- 阻塞族：Send/SendFor/SendUntil/RecvFor/RecvUntil ----
	 * 容量 2：先满发验证超时，再收空验证正常与空收超时。 */
	pHeap = xrtChannelCreate(2u);
	if ( (pHeap == NULL) ||
		(xrtChannelSend(pHeap, (ptr)1) != XWAIT_OK) ||
		(xrtChannelSendFor(pHeap, (ptr)2,
			EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
		(xrtChannelSendFor(pHeap, (ptr)4,
			EXAMPLE_TIMEOUT_US) != XWAIT_TIMEOUT) ||
		(xrtChannelSendUntil(pHeap, (ptr)4,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
			XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	if ( (xrtChannelRecvFor(pHeap, &pItem,
			EXAMPLE_TIMEOUT_US) != XWAIT_OK) ||
		(pItem != (ptr)1) ||
		(xrtChannelRecvUntil(pHeap, &pItem,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
			XWAIT_OK) ||
		(pItem != (ptr)2) ||
		(xrtChannelRecvFor(pHeap, &pItem,
			EXAMPLE_TIMEOUT_US) != XWAIT_TIMEOUT) ||
		(xrtChannelRecvUntil(pHeap, &pItem,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) !=
			XWAIT_TIMEOUT) ) {
		goto Cleanup;
	}
	/* SendUntil 的正常路径：空通道 + 远期截止。 */
	if ( xrtChannelSendUntil(pHeap, (ptr)3,
			xrtDeadlineAfter(EXAMPLE_TIMEOUT_US)) != XWAIT_OK ) {
		goto Cleanup;
	}
	(void)xrtChannelTryRecv(pHeap, &pItem);
	printf("channel: send/recv for+until = ok (timeout=1)\n");

	/* ---- 取消族：共享堆令牌，等待线程挂起后主线程 Request ---- */
	pEmpty = xrtChannelCreate(1u);
	SendJob.pChannel = pHeap;
	SendJob.pCancel = xrtCancelCreate();
	RecvJob.pChannel = pEmpty;
	RecvJob.pCancel = xrtCancelCreate();
	pSendThread = SendJob.pCancel != NULL ?
		xrtThreadCreate(exampleSendWaiter, &SendJob, 0u) : NULL;
	/* 发送要满通道：先填 2 个，等待线程必然挂起。 */
	(void)xrtChannelTrySend(pHeap, (ptr)10);
	(void)xrtChannelTrySend(pHeap, (ptr)11);
	if ( (pEmpty == NULL) ||
		(xrtChannelCount(pHeap) != 2u) ) {
		goto Cleanup;
	}
	pRecvThread = RecvJob.pCancel != NULL ?
		xrtThreadCreate(exampleRecvWaiter, &RecvJob, 0u) : NULL;
	if ( (pSendThread == NULL) || (pRecvThread == NULL) ) {
		goto Cleanup;
	}
	{
		xdeadline iGrace = xrtDeadlineAfter(UINT64_C(300000));

		while ( (SendJob.bDone || RecvJob.bDone) == false ) {
			if ( xrtDeadlineExpired(iGrace) ) {
				break;
			}
			xrtThreadYield();
		}
	}
	(void)xrtCancelRequest(SendJob.pCancel);
	(void)xrtCancelRequest(RecvJob.pCancel);
	if ( !exampleSpinUntil(&SendJob.bDone) ||
		!exampleSpinUntil(&RecvJob.bDone) ||
		(SendJob.Result != XWAIT_CANCELLED) ||
		(RecvJob.Result != XWAIT_CANCELLED) ) {
		goto Cleanup;
	}
	(void)xrtThreadWait(pSendThread);
	xrtThreadDestroy(pSendThread);
	pSendThread = NULL;
	(void)xrtThreadWait(pRecvThread);
	xrtThreadDestroy(pRecvThread);
	pRecvThread = NULL;
	/* 已触发令牌的立即返回路径（五个变体一次覆盖）。 */
	{
		ptr pOne = NULL;

		if ( (xrtChannelSendCancel(pHeap, (ptr)9,
				SendJob.pCancel) != XWAIT_CANCELLED) ||
			(xrtChannelSendUntilCancel(pHeap, (ptr)9,
				xrtDeadlineAfter(UINT64_C(1000000)),
				SendJob.pCancel) != XWAIT_CANCELLED) ||
			(xrtChannelSendForCancel(pHeap, (ptr)9,
				UINT64_C(1000000), SendJob.pCancel) !=
				XWAIT_CANCELLED) ||
			(xrtChannelRecvForCancel(pEmpty, &pOne,
				UINT64_C(1000000), RecvJob.pCancel) !=
				XWAIT_CANCELLED) ||
			(xrtChannelRecvUntilCancel(pEmpty, &pOne,
				xrtDeadlineAfter(UINT64_C(1000000)),
				RecvJob.pCancel) != XWAIT_CANCELLED) ) {
			goto Cleanup;
		}
	}
	xrtCancelDestroy(SendJob.pCancel);
	SendJob.pCancel = NULL;
	xrtCancelDestroy(RecvJob.pCancel);
	RecvJob.pCancel = NULL;
	printf("channel: cancel send=%d recv=%d\n",
		(int)SendJob.Result, (int)RecvJob.Result);

	/* ---- Select 族：Try / For / Until ----
	 * case0=发送(Heap)、case1=接收(Empty)；逐步控制就绪侧。 */
	(void)xrtChannelTryRecv(pHeap, &pItem);
	(void)xrtChannelTryRecv(pHeap, &pItem);  /* 排空 Heap */
	Cases[0] = xrtChannelCaseSend(pHeap, (ptr)20);
	Cases[1] = xrtChannelCaseRecv(pEmpty, &pItem);
	/* 1) 只有发送就绪：SelectTry 立即选中 case0。 */
	Select = xrtChannelSelectTry(Cases, 2u);
	if ( (Select.Wait != XWAIT_OK) || (Select.Index != 0u) ||
		(Select.Result != XCHANNEL_OK) ) {
		goto Cleanup;
	}
	/* 2) Heap 填满 + Empty 仍空：两个 case 都不就绪 → 超时。 */
	(void)xrtChannelTrySend(pHeap, (ptr)21);
	Select = xrtChannelSelectFor(Cases, 2u, EXAMPLE_TIMEOUT_US);
	if ( Select.Wait != XWAIT_TIMEOUT ) {
		goto Cleanup;
	}
	/* 3) Empty 放入一条：SelectUntil 选中接收 case1。 */
	(void)xrtChannelTrySend(pEmpty, (ptr)30);
	Select = xrtChannelSelectUntil(Cases, 2u,
		xrtDeadlineAfter(UINT64_C(1000000)));
	if ( (Select.Wait != XWAIT_OK) || (Select.Index != 1u) ||
		(pItem != (ptr)30) ) {
		goto Cleanup;
	}
	printf("channel: select try=1 for-timeout until=1\n");

	/* ---- 协程族：RecvAwaitFor / SendAwaitUntil / SelectAwaitFor ---- */
	{
		static xchannel arrSelect[2];
		static ptr arrSelectSlots[2];

		if ( !xrtChannelInitBuffer(&arrSelect[0],
				&arrSelectSlots[0], 1u) ||
			!xrtChannelInitBuffer(&arrSelect[1],
				&arrSelectSlots[1], 1u) ) {
			goto Cleanup;
		}
		pSched = xrtCoSchedCreate();
		if ( pSched == NULL ) {
			goto Cleanup;
		}
		pCoTimeout = xrtCoSpawn(pSched, exampleCoRecvTimeout,
			&arrSelect[1], NULL);
		/* 协程二的通道先填满（容量 1），发送必然挂起，
		 * 由调度器内另一侧的接收协程（协程三的 case 0）唤醒。 */
		(void)xrtChannelTrySend(&arrSelect[0], (ptr)60);
		pCoSend = xrtCoSpawn(pSched, exampleCoSendBlocked,
			&arrSelect[0], NULL);
		pCoSelect = xrtCoSpawn(pSched, exampleCoSelect,
			arrSelect, NULL);
		/* 协程四~七：满发 AwaitFor 超时 / 过期 AwaitUntil /
		 * 无限期 SelectAwait（预填命中）/ 过期 SelectAwaitUntil。 */
		pCoSendTo = xrtCoSpawn(pSched, exampleCoSendTimeout,
			&tFull, NULL);
		pCoRecvEx = xrtCoSpawn(pSched, exampleCoRecvExpired,
			&tExpire, NULL);
		pCoSelEver = xrtCoSpawn(pSched, exampleCoSelectForever,
			arrSelect, NULL);
		pCoSelEx = xrtCoSpawn(pSched, exampleCoSelectExpired,
			&tSilent, NULL);
		if ( (pCoTimeout == NULL) || (pCoSend == NULL) ||
			(pCoSelect == NULL) || (pCoSendTo == NULL) ||
			(pCoRecvEx == NULL) || (pCoSelEver == NULL) ||
			(pCoSelEx == NULL) ) {
			goto Cleanup;
		}
		/* 首轮流结束 select[0] 里留着发送协程的 77：排空后
		 * 填 31 使其满（协程四超时），协程六因 31 命中 case0。 */
		(void)xrtChannelTryRecv(&arrSelect[0], &pItem);
		(void)xrtChannelTrySend(&arrSelect[0], (ptr)31);
		/* tFull：容量 1 预填，无人腾位 → 协程四必然超时；
		 * tExpire：容量 0 无发送者 → 协程五必然到期。 */
		if ( !xrtChannelInitBuffer(&tFull, arrFullSlot, 1u) ||
			(xrtChannelTrySend(&tFull, (ptr)99) !=
				XCHANNEL_OK) ||
			!xrtChannelInit(&tExpire, 0u) ) {
			goto Cleanup;
		}
		if ( !xrtChannelInit(&tSilent, 0u) ) {
			goto Cleanup;
		}
		if ( !xrtCoSchedRun(pSched) ) {
			goto Cleanup;
		}
		xrtChannelUnit(&tExpire);
		xrtChannelUnit(&tSilent);

		/* 超时协程返回 TIMEOUT=1；发送协程 OK=0；
		 * 选择协程命中 case 0（发送协程的 77）返回 0x10。 */
		if ( (xrtCoResult(pCoTimeout) != (ptr)XWAIT_TIMEOUT) ||
			(xrtCoResult(pCoSend) != (ptr)XWAIT_OK) ||
			(xrtCoResult(pCoSelect) != (ptr)0x10) ||
			(xrtCoResult(pCoSendTo) != (ptr)XWAIT_TIMEOUT) ||
			(xrtCoResult(pCoRecvEx) != (ptr)XWAIT_TIMEOUT) ||
			(xrtCoResult(pCoSelEver) != (ptr)0x20) ||
			(xrtCoResult(pCoSelEx) != (ptr)XWAIT_TIMEOUT) ) {
			goto Cleanup;
		}
		printf("channel: coroutine await for/until + select"
			" = 7 coroutines\n");
		xrtCoDestroy(pCoTimeout);
		pCoTimeout = NULL;
		xrtCoDestroy(pCoSend);
		pCoSend = NULL;
		xrtCoDestroy(pCoSelect);
		pCoSelect = NULL;
		xrtCoDestroy(pCoSendTo);
		pCoSendTo = NULL;
		xrtCoDestroy(pCoRecvEx);
		pCoRecvEx = NULL;
		xrtCoDestroy(pCoSelEver);
		pCoSelEver = NULL;
		xrtCoDestroy(pCoSelEx);
		pCoSelEx = NULL;
		xrtCoSchedDestroy(pSched);
		pSched = NULL;
		xrtCoThreadDetach();
		xrtChannelUnit(&arrSelect[0]);
		xrtChannelUnit(&arrSelect[1]);
	}
	iResult = 0;

Cleanup:
	xrtCoDestroy(pCoTimeout);
	xrtCoDestroy(pCoSend);
	xrtCoDestroy(pCoSelect);
	xrtCoDestroy(pCoSendTo);
	xrtCoDestroy(pCoRecvEx);
	xrtCoDestroy(pCoSelEver);
	xrtCoDestroy(pCoSelEx);
	if ( pSched != NULL ) {
		xrtCoSchedDestroy(pSched);
		xrtCoThreadDetach();
	}
	if ( pSendThread != NULL ) {
		(void)xrtThreadWait(pSendThread);
		xrtThreadDestroy(pSendThread);
	}
	if ( pRecvThread != NULL ) {
		(void)xrtThreadWait(pRecvThread);
		xrtThreadDestroy(pRecvThread);
	}
	if ( SendJob.pCancel != NULL ) {
		xrtCancelDestroy(SendJob.pCancel);
	}
	if ( RecvJob.pCancel != NULL ) {
		xrtCancelDestroy(RecvJob.pCancel);
	}
	xrtChannelDestroy(pHeap);
	xrtChannelDestroy(pEmpty);
	return iResult;
}
