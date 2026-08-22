#include <stdio.h>

#include <xrt.h>



/* 演示指针栈只管理释放顺序，不接管目标生命周期。 */
int main(void)
{
	xptrstack tResources;
	int pValues[] = { 10, 20, 30 };
	ptr pValue;

	if ( !xrtPtrStackInit(&tResources) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtPtrStackPush(&tResources, &pValues[i]) ) {
			xrtPtrStackUnit(&tResources);
			return 2;
		}
	}
	while ( xrtPtrStackPop(&tResources, &pValue) ) {
		printf("%d\n", *(int*)pValue);
	}
	xrtClearError();
	xrtPtrStackUnit(&tResources);
	return 0;
}
