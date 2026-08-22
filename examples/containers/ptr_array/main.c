#include <stdio.h>

#include <xrt.h>



/* 演示指针数组只管理引用，不接管目标对象。 */
int main(void)
{
	xptrarray tArray;
	int iFirst = 10;
	int iSecond = 20;
	int iThird = 30;

	if ( !xrtPtrArrayInit(&tArray) ) {
		return 1;
	}
	if (
		!xrtPtrArrayPush(&tArray, &iFirst) ||
		!xrtPtrArrayPush(&tArray, &iSecond) ||
		!xrtPtrArrayInsert(&tArray, 1, &iThird)
	) {
		xrtPtrArrayUnit(&tArray);
		return 2;
	}
	for ( size_t i = 0; i < tArray.Count; i++ ) {
		printf("[%zu] %d\n", i, *(int*)xrtPtrArrayGet(&tArray, i));
	}
	xrtPtrArrayUnit(&tArray);
	return 0;
}
