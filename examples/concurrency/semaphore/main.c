#include <stdio.h>
#include <xrt.h>



/* 用计数信号量交付一个工作许可。 */
int main(void)
{
	xsem Semaphore;
	xwaitresult Result;

	if ( !xrtSemInit(&Semaphore, 0, 1) ||
		!xrtSemPost(&Semaphore) ) {
		return 1;
	}
	Result = xrtSemWaitUntil(
		&Semaphore,
		xrtDeadlineAfter(UINT64_C(1000000))
	);
	printf("semaphore wait result: %d\n", (int)Result);
	return xrtSemUnit(&Semaphore) &&
		(Result == XWAIT_OK) ? 0 : 2;
}
