#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件 Once 初始化一个整数。 */
static bool singleOnceInitialize(ptr pData)
{
	(*(int*)pData)++;
	return true;
}



/* 验证单头文件 Once 只执行一次。 */
int main(void)
{
	xonce tOnce = XRT_ONCE_INIT;
	int iCount = 0;

	return xrtOnce(&tOnce, singleOnceInitialize, &iCount) &&
		xrtOnce(&tOnce, singleOnceInitialize, &iCount) &&
		(iCount == 1) ? 0 : 1;
}
