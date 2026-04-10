/*
 * XRT Example - RWLock
 * XRT 范例 - 读写锁
 *
 * Description / 说明:
 *   EN: Demonstrates read lock, upgrade, downgrade and write lock.
 *   CN: 演示读锁、升级、降级与写锁操作。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_rwlock.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_rwlock -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


int main()
{
	xrwlock hLock = NULL;
	int iValue = 7;
	bool bTryRead = FALSE;
	bool bUpgrade = FALSE;

	xrtInit();

	hLock = xrtRWLockCreate();

	xrtRWLockReadLock(hLock);
	bTryRead = xrtRWLockTryReadLock(hLock);
	bUpgrade = xrtRWLockUpgrade(hLock);
	if ( bUpgrade ) {
		iValue += 5;
		xrtRWLockDowngrade(hLock);
	}
	xrtRWLockReadUnlock(hLock);

	xrtRWLockWriteLock(hLock);
	iValue += 3;
	xrtRWLockWriteUnlock(hLock);

	printf("try_read_while_reading = %s\n", bTryRead ? "TRUE" : "FALSE");
	printf("upgrade_success = %s\n", bUpgrade ? "TRUE" : "FALSE");
	printf("value = %d\n", iValue);

	xrtRWLockDestroy(hLock);
	xrtUnit();
	return 0;
}
