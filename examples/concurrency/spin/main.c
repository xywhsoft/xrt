#include <xrt.h>

#include <stdio.h>



/* 展示栈上短临界区锁的生命周期。 */
int main(void)
{
	xspinlock Spin;
	uint64 iCounter = 0u;

	if ( !xrtSpinInit(&Spin) ) {
		return 1;
	}
	if ( !xrtSpinLock(&Spin) ) {
		return 2;
	}
	iCounter++;
	if ( !xrtSpinUnlock(&Spin) || !xrtSpinUnit(&Spin) ) {
		return 3;
	}
	printf("counter=%llu\n", (unsigned long long)iCounter);
	return 0;
}
