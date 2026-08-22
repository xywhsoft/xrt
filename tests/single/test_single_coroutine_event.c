#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件协程等待一个外部置位的自动复位事件。 */
static ptr testSingleCoEventWait(ptr pData)
{
	xcoevent* pEvent = (xcoevent*)pData;

	return xrtCoEventAwait(pEvent) == XWAIT_OK ? pEvent : NULL;
}



/* 验证单头文件包含完整协程事件适配器。 */
int main(void)
{
	xcoevent tEvent;
	xcosched* pSched;
	xcoro* pCo;
	int iResult = 1;

	if ( !xrtCoEventInit(&tEvent, false, false) ) {
		return 1;
	}
	pSched = xrtCoSchedCreate();
	if ( pSched == NULL ) {
		(void)xrtCoEventUnit(&tEvent);
		return 1;
	}
	pCo = xrtCoSpawn(pSched, testSingleCoEventWait, &tEvent, NULL);
	if (
		(pCo != NULL) &&
		(xrtCoSchedStep(pSched) == XWAIT_OK) &&
		xrtCoEventSet(&tEvent) &&
		xrtCoSchedRun(pSched) &&
		(xrtCoResult(pCo) == &tEvent) &&
		xrtCoDestroy(pCo)
	) {
		iResult = 0;
	}
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoEventUnit(&tEvent);
	(void)xrtCoThreadDetach();
	return iResult;
}
