#include <stdio.h>
#include <xrt.h>



/* 展示无需运行期分配的固定缓冲 SPSC 消息环。 */
int main(void)
{
	xspscqueue Queue;
	ptr Storage[8];
	int pValues[] = { 10, 20, 30 };
	ptr pValue;

	if ( !xrtSPSCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}
	for ( size_t i = 0; i < 3u; i++ ) {
		if ( xrtSPSCQueueTryPush(&Queue, &pValues[i]) != XQUEUE_OK ) {
			xrtSPSCQueueUnit(&Queue);
			return 2;
		}
	}
	xrtSPSCQueueClose(&Queue);
	while ( xrtSPSCQueueTryPop(&Queue, &pValue) == XQUEUE_OK ) {
		printf("%d\n", *(int*)pValue);
	}
	xrtSPSCQueueUnit(&Queue);
	return 0;
}
