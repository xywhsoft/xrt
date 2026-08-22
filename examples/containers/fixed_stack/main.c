#include <stdio.h>

#include <xrt.h>



/* 演示用调用帧直接存放在本地固定缓冲中。 */
typedef struct exampleframe {
	int Function;
	int Position;
} exampleframe;



/* 演示已知最大深度、运行期零分配的工作栈。 */
int main(void)
{
	xfixedstack tFrames;
	exampleframe pStorage[8];
	exampleframe pInput[] = {
		{ 1, 10 },
		{ 2, 20 },
		{ 3, 30 }
	};
	exampleframe tFrame;

	if ( !xrtFixedStackInit(&tFrames, pStorage, sizeof(pStorage), sizeof(exampleframe)) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtFixedStackPush(&tFrames, &pInput[i]) ) {
			return 2;
		}
	}
	while ( xrtFixedStackPop(&tFrames, &tFrame) ) {
		printf("function=%d position=%d\n", tFrame.Function, tFrame.Position);
	}
	xrtClearError();
	xrtFixedStackUnit(&tFrames);
	return 0;
}
