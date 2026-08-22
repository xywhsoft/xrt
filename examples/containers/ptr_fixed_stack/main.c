#include <stdio.h>

#include <xrt.h>



/* 演示无分配固定容量资源回滚栈。 */
int main(void)
{
	xptrfixedstack tCleanup;
	ptr pStorage[4];
	int pResources[] = { 10, 20, 30 };
	ptr pResource;

	if ( !xrtPtrFixedStackInit(&tCleanup, pStorage, 4) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtPtrFixedStackPush(&tCleanup, &pResources[i]) ) {
			xrtPtrFixedStackUnit(&tCleanup);
			return 2;
		}
	}
	while ( xrtPtrFixedStackPop(&tCleanup, &pResource) ) {
		printf("%d\n", *(int*)pResource);
	}
	xrtClearError();
	xrtPtrFixedStackUnit(&tCleanup);
	return 0;
}
