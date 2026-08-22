#include <stdio.h>

#include <xrt.h>



/* 示例协程分两段完成工作。 */
static ptr exampleCoroutine(ptr pData)
{
	int* pValue = (int*)pData;

	*pValue += 1;
	if ( xrtCoYield() != XWAIT_OK ) {
		return NULL;
	}
	*pValue *= 2;
	return pValue;
}



/* 展示低级协程的创建、恢复和结果读取。 */
int main(void)
{
	int iValue = 20;
	xcoro* pCo = xrtCoCreate(exampleCoroutine, &iValue, NULL);

	if ( pCo == NULL ) {
		return 1;
	}
	(void)xrtCoResume(pCo);
	printf("after yield: %d\n", iValue);
	(void)xrtCoResume(pCo);
	printf("result: %d\n", *(int*)xrtCoResult(pCo));
	(void)xrtCoDestroy(pCo);
	(void)xrtCoThreadDetach();
	return 0;
}
