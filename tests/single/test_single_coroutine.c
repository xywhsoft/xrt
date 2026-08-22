#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件协程过程立即返回输入。 */
static ptr testSingleCoroutineProc(ptr pData)
{
	return pData;
}



/* 验证单头文件包含完整协程核心实现。 */
int main(void)
{
	int iValue = 9;
	xcoro* pCo = xrtCoCreate(testSingleCoroutineProc, &iValue, NULL);

	if ( (pCo == NULL) || !xrtCoResume(pCo) || (xrtCoResult(pCo) != &iValue) ) {
		return 1;
	}
	if ( !xrtCoDestroy(pCo) || !xrtCoThreadDetach() ) {
		return 2;
	}
	return 0;
}
