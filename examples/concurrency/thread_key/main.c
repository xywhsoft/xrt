#include <stdio.h>

#include <xrt.h>



/* 释放线程退出时仍保存在键中的堆对象。 */
static void destroyValue(ptr pValue)
{
	xrtFree(pValue);
}



/* 为当前线程创建、读取并销毁一个拥有所有权的局部值。 */
int main(void)
{
	xthreadkey* pKey = xrtThreadKeyCreate(destroyValue);
	int* pValue;

	if ( pKey == NULL ) {
		return 1;
	}
	pValue = (int*)xrtMalloc(sizeof(int));
	if ( pValue == NULL ) {
		xrtThreadKeyDestroy(pKey);
		return 1;
	}
	*pValue = 42;
	if ( !xrtThreadKeySet(pKey, pValue) ) {
		xrtFree(pValue);
		xrtThreadKeyDestroy(pKey);
		return 1;
	}
	printf("thread value: %d\n", *(int*)xrtThreadKeyGet(pKey));
	return xrtThreadKeyDestroy(pKey) ? 0 : 1;
}
