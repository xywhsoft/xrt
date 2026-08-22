#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件线程返回固定值。 */
static int32 singleThreadEntry(ptr pData)
{
	return (int32)(intptr_t)pData;
}



/* 验证单头文件的线程创建、等待和退出码。 */
int main(void)
{
	xthread* pThread = xrtThreadCreate(singleThreadEntry, (ptr)(intptr_t)9, 0);
	int iResult = 1;

	if ( (pThread != NULL) && (xrtThreadWait(pThread) == XWAIT_OK) &&
		 (xrtThreadExitCode(pThread) == 9) ) {
		iResult = 0;
	}
	xrtThreadDestroy(pThread);
	return iResult;
}
