#include <stdio.h>
#include <xrt.h>



/* 接收协程等待消息并返回收到的指针值。 */
static ptr receiveMessage(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;
	ptr pMessage = NULL;

	if ( xrtChannelRecvAwait(pChannel, &pMessage) != XWAIT_OK ) {
		return NULL;
	}
	return pMessage;
}



/* 发送协程与接收协程执行一次无缓冲 rendezvous。 */
static ptr sendMessage(ptr pData)
{
	xchannel* pChannel = (xchannel*)pData;

	if (
		xrtChannelSendAwait(
			pChannel,
			(ptr)(uintptr_t)42u
		) != XWAIT_OK
	) {
		return NULL;
	}
	return pChannel;
}



/*
 * 范例：concurrency/channel_coroutine —— 协程 × 通道：挂起式收发
 * ----------------------------------------------------------------
 * 演示 API：
 *   通道等待在协程内的挂起变体（Await 族）
 * 模块宏：XRT_MODULE_CHANNEL（依赖 COROUTINE）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/channel_coroutine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   message: 42
 *
 * 通道 Recv 在协程里变成挂起而非阻塞——调度线程
 *   被释放去跑别的协程，数据到达再唤醒本协程。
 *   Go 语法的 channel + goroutine 在 C 里的对应物。
 */


/* 演示 Channel 等待挂起协程而不阻塞调度线程。 */
int main(void)
{
	xchannel tChannel;
	xcosched* pSched;
	xcoro* pRecv;
	xcoro* pSend;
	int iResult = 1;

	if ( !xrtChannelInit(&tChannel, 0) ) {
		return 1;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		xrtChannelUnit(&tChannel);
		return 1;
	}
	pRecv = xrtCoSpawn(pSched, receiveMessage, &tChannel, NULL);
	pSend = xrtCoSpawn(pSched, sendMessage, &tChannel, NULL);
	if (
		(pRecv != NULL) &&
		(pSend != NULL) &&
		xrtCoSchedRun(pSched)
	) {
		printf(
			"message: %llu\n",
			(unsigned long long)(uintptr_t)xrtCoResult(pRecv)
		);
		iResult = 0;
	}
	xrtCoDestroy(pRecv);
	xrtCoDestroy(pSend);
	xrtCoSchedDestroy(pSched);
	xrtCoThreadDetach();
	xrtChannelUnit(&tChannel);
	return iResult;
}
