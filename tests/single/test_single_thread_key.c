#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 验证单头文件线程键的设置、取回和销毁。 */
int main(void)
{
	xthreadkey* pKey = xrtThreadKeyCreate(NULL);
	int iValue = 9;
	int iResult = 1;

	if ( (pKey != NULL) && xrtThreadKeySet(pKey, &iValue) &&
		(xrtThreadKeyGet(pKey) == &iValue) &&
		(xrtThreadKeyTake(pKey) == &iValue) && xrtThreadKeyDestroy(pKey) ) {
		iResult = 0;
	}
	return iResult;
}
