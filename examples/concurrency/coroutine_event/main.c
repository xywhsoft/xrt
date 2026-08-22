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
