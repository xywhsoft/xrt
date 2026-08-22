#include <stdio.h>

#include <xrt.h>



/* 演示深度未知的整数工作栈。 */
int main(void)
{
	xstack tValues;
	int iValue;

	if ( !xrtStackInit(&tValues, sizeof(int)) ) {
		return 1;
	}
	for ( int i = 1; i <= 20; i++ ) {
		iValue = i * 10;
		if ( !xrtStackPush(&tValues, &iValue) ) {
			xrtStackUnit(&tValues);
			return 2;
		}
	}
	while ( xrtStackPop(&tValues, &iValue) ) {
		printf("%d\n", iValue);
	}
	xrtClearError();
	xrtStackUnit(&tValues);
	return 0;
}
