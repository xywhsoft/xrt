#include <stdio.h>

#include <xrt.h>



/* 展示读写锁的普通路径、升级路径和原子降级路径。 */
int main(void)
{
	xrwlock Lock;
	int iValue = 7;
	bool bOkay = false;

	if ( !xrtRWLockInit(&Lock) ) {
		return 1;
	}
	if ( !xrtRWLockRead(&Lock) ||
		!xrtRWLockUpgrade(&Lock) ) {
		goto cleanup;
	}
	iValue += 5;
	if ( !xrtRWLockDowngrade(&Lock) ||
		!xrtRWLockReadUnlock(&Lock) ||
		!xrtRWLockWrite(&Lock) ) {
		goto cleanup;
	}
	iValue += 3;
	if ( !xrtRWLockWriteUnlock(&Lock) ||
		!xrtRWLockTryRead(&Lock) ) {
		goto cleanup;
	}
	bOkay = xrtRWLockReadUnlock(&Lock);

cleanup:
	printf("value=%d\n", iValue);
	return xrtRWLockUnit(&Lock) && bOkay && (iValue == 15) ? 0 : 2;
}
