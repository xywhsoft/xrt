#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件调度协程执行一次异步睡眠。 */
static ptr testSingleCoSchedProc(ptr pData)
{
	return xrtCoSleep(100) == XWAIT_OK ? pData : NULL;
}



/* 验证单头文件包含完整调度器和协程核心。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();
	int iValue = 17;
	xcoro* pCo;
	int iResult = 1;

	if ( pSched == NULL ) {
		return 1;
	}
	pCo = xrtCoSpawn(pSched, testSingleCoSchedProc, &iValue, NULL);
	if ( (pCo != NULL) && xrtCoSchedRun(pSched) &&
		 (xrtCoResult(pCo) == &iValue) && xrtCoDestroy(pCo) ) {
		iResult = 0;
	}
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return iResult;
}
