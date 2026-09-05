#include <stdio.h>
#include <xrt.h>



/* 生产者完成异步工作后置位事件。 */
static ptr producer(ptr pData)
{
	xcoevent* pEvent = (xcoevent*)pData;

	if ( xrtCoSleep(1000) != XWAIT_OK ) {
		return NULL;
	}
	return xrtCoEventSet(pEvent) ? pEvent : NULL;
}



/* 消费者不阻塞调度线程地等待生产者。 */
static ptr consumer(ptr pData)
{
	xcoevent* pEvent = (xcoevent*)pData;

	if ( xrtCoEventAwait(pEvent) != XWAIT_OK ) {
		return NULL;
	}
	printf("event received\n");
	return pEvent;
}



/*
 * 范例：concurrency/coroutine_event —— 事件：协程间同步原语
 * ----------------------------------------------------------------
 * 演示 API：
 *   事件置位（生产者完成异步工作后）
 *   协程等待事件（不阻塞调度线程）
 * 模块宏：XRT_MODULE_COROUTINE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/coroutine_event/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   event received
 *
 * 同一调度器里的生产者/消费者经事件同步：消费者
 *   协程挂起等待，事件置位后被调度器唤醒——
 *   全程零原生线程阻塞。
 */


/* 展示同一调度器中生产者和消费者的事件同步。 */
int main(void)
{
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pConsumer;
	bool bOkay;

	if ( !xrtCoEventInit(&tEvent, false, false) ) {
		return 1;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		(void)xrtCoEventUnit(&tEvent);
		return 1;
	}
	pConsumer = xrtCoSpawn(pSched, consumer, &tEvent, NULL);
	bOkay =
		(pConsumer != NULL) &&
		xrtCoGo(pSched, producer, &tEvent, NULL) &&
		xrtCoSchedRun(pSched) &&
		(xrtCoResult(pConsumer) == &tEvent) &&
		xrtCoDestroy(pConsumer);
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoEventUnit(&tEvent);
	(void)xrtCoThreadDetach();
	return bOkay ? 0 : 1;
}
