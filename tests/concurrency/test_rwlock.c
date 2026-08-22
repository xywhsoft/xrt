#include "../test.h"
#include "../test_thread.h"



typedef struct testrwlockstate {
	xrwlock* Lock;
	int Value;
	int Iterations;
} testrwlockstate;



/* 两个升级线程共享的无锁测试屏障。 */
typedef struct testrwlockupgrade {
	xrwlock* Lock;
	xmutex Barrier;
	int32 Ready;
	int32 Upgraded;
} testrwlockupgrade;



/* 在测试屏障锁保护下增加计数。 */
static int32 testRWLockCounterIncrement(xmutex* pBarrier, int32* pValue)
{
	int32 iValue;

	if ( !xrtMutexLock(pBarrier) ) {
		return -1;
	}
	(*pValue)++;
	iValue = *pValue;
	(void)xrtMutexUnlock(pBarrier);
	return iValue;
}



/* 在测试屏障锁保护下读取计数。 */
static int32 testRWLockCounterLoad(xmutex* pBarrier, int32* pValue)
{
	int32 iValue;

	if ( !xrtMutexLock(pBarrier) ) {
		return -1;
	}
	iValue = *pValue;
	(void)xrtMutexUnlock(pBarrier);
	return iValue;
}



/* 在写锁保护下累加共享值。 */
static int testRWLockWriter(ptr pData)
{
	testrwlockstate* pState = (testrwlockstate*)pData;

	for ( int i = 0; i < pState->Iterations; i++ ) {
		if ( !xrtRWLockWrite(pState->Lock) ) {
			return 1;
		}
		pState->Value++;
		if ( !xrtRWLockWriteUnlock(pState->Lock) ) {
			return 2;
		}
	}
	return 0;
}



/* 在读锁保护下反复读取共享值。 */
static int testRWLockReader(ptr pData)
{
	testrwlockstate* pState = (testrwlockstate*)pData;

	for ( int i = 0; i < pState->Iterations; i++ ) {
		int iValue;

		if ( !xrtRWLockRead(pState->Lock) ) {
			return 1;
		}
		iValue = pState->Value;
		if ( !xrtRWLockReadUnlock(pState->Lock) ) {
			return 2;
		}
		if ( iValue < 0 ) {
			return 3;
		}
	}
	return 0;
}



/* 获取并释放一次写锁，用于验证写者排队。 */
static int testRWLockWriterOnce(ptr pData)
{
	xrwlock* pLock = (xrwlock*)pData;

	if ( !xrtRWLockWrite(pLock) ) {
		return 1;
	}
	return xrtRWLockWriteUnlock(pLock) ? 0 : 2;
}



/* 非持有线程不能释放另一个线程的写锁。 */
static int testRWLockWrongWriter(ptr pData)
{
	xrwlock* pLock = (xrwlock*)pData;

	if ( xrtRWLockWriteUnlock(pLock) ) {
		return 1;
	}
	return xrtErrorKind(xrtGetError()) == XERR_STATE ? 0 : 2;
}



/* 同步取得读锁后并发升级，验证升级者不会互相保留读锁。 */
static int testRWLockUpgradeWorker(ptr pData)
{
	testrwlockupgrade* pState = (testrwlockupgrade*)pData;

	if ( !xrtRWLockRead(pState->Lock) ) {
		return 1;
	}
	if ( testRWLockCounterIncrement(&pState->Barrier, &pState->Ready) < 0 ) {
		return 4;
	}
	while ( testRWLockCounterLoad(&pState->Barrier, &pState->Ready) != 2 ) {
		testThreadYield();
	}
	if ( !xrtRWLockUpgrade(pState->Lock) ) {
		return 2;
	}
	if ( testRWLockCounterIncrement(&pState->Barrier, &pState->Upgraded) < 0 ) {
		return 5;
	}
	return xrtRWLockWriteUnlock(pState->Lock) ? 0 : 3;
}



/* 验证读写、升级、降级、错误保护和并发互斥。 */
int main(void)
{
	xrwlock tLock;
	xrwlock* pLock;
	testrwlockstate tState;
	testthread arrThreads[6];
	testthread tWriter;
	testthread tWrongWriter;
	testthread arrUpgraders[2];
	testrwlockupgrade tUpgrade;
	bool bWriterQueued = false;

	memset(&tLock, 0, sizeof(tLock));
	testRequire(xrtRWLockInit(&tLock), "in-place rwlock init failed");
	testRequire(!xrtRWLockUpgrade(&tLock), "rwlock upgrade without read lock succeeded");
	xrtClearError();
	testRequire(xrtRWLockTryRead(&tLock), "rwlock try-read failed");
	testRequire(!xrtRWLockTryWrite(&tLock), "rwlock try-write ignored active reader");
	testRequire(!xrtRWLockUnit(&tLock), "busy rwlock unit succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "busy rwlock unit error mismatch");
	xrtClearError();
	testRequire(xrtRWLockReadUnlock(&tLock), "rwlock try-read unlock failed");
	testRequire(xrtRWLockRead(&tLock), "rwlock read failed");
	testRequire(xrtRWLockUpgrade(&tLock), "rwlock upgrade failed");
	testRequire(xrtRWLockDowngrade(&tLock), "rwlock downgrade failed");
	testRequire(xrtRWLockReadUnlock(&tLock), "downgraded rwlock read unlock failed");
	testRequire(xrtRWLockTryWrite(&tLock), "rwlock try-write failed");
	testRequire(!xrtRWLockWrite(&tLock), "recursive rwlock write succeeded");
	xrtClearError();
	memset(&tWrongWriter, 0, sizeof(tWrongWriter));
	tWrongWriter.Proc = testRWLockWrongWriter;
	tWrongWriter.Data = &tLock;
	testThreadsStart(&tWrongWriter, 1);
	testThreadsJoin(&tWrongWriter, 1);
	testRequire(tWrongWriter.Result == 0, "wrong-owner rwlock write unlock was not rejected");
	testRequire(xrtRWLockWriteUnlock(&tLock), "rwlock write unlock failed");
	testRequire(!xrtRWLockReadUnlock(&tLock), "unowned rwlock read unlock succeeded");
	xrtClearError();
	testRequire(!xrtRWLockWriteUnlock(&tLock), "unowned rwlock write unlock succeeded");
	xrtClearError();
	testRequire(!xrtRWLockDowngrade(&tLock), "unowned rwlock downgrade succeeded");
	xrtClearError();

	testRequire(xrtRWLockRead(&tLock), "writer-priority guard read failed");
	memset(&tWriter, 0, sizeof(tWriter));
	tWriter.Proc = testRWLockWriterOnce;
	tWriter.Data = &tLock;
	testThreadsStart(&tWriter, 1);
	for ( size_t i = 0; i < 100000; i++ ) {
		if ( !xrtRWLockTryRead(&tLock) ) {
			bWriterQueued = true;
			break;
		}
		testRequire(xrtRWLockReadUnlock(&tLock), "writer-priority probe unlock failed");
		testThreadYield();
	}
	testRequire(bWriterQueued, "rwlock writer did not block later readers");
	testRequire(xrtRWLockReadUnlock(&tLock), "writer-priority guard unlock failed");
	testThreadsJoin(&tWriter, 1);
	testRequire(tWriter.Result == 0, "queued rwlock writer failed");
	testRequire(xrtRWLockUnit(&tLock), "in-place rwlock unit failed");

	pLock = xrtRWLockCreate();
	testRequire(pLock != NULL, "owned rwlock create failed");
	tState.Lock = pLock;
	tState.Value = 0;
	tState.Iterations = 10000;
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < 2; i++ ) {
		arrThreads[i].Proc = testRWLockWriter;
		arrThreads[i].Data = &tState;
	}
	for ( size_t i = 2; i < 6; i++ ) {
		arrThreads[i].Proc = testRWLockReader;
		arrThreads[i].Data = &tState;
	}
	testThreadsStart(arrThreads, 6);
	testThreadsJoin(arrThreads, 6);
	for ( size_t i = 0; i < 6; i++ ) {
		testRequire(arrThreads[i].Result == 0, "rwlock worker failed");
	}
	testRequire(tState.Value == 20000, "rwlock lost protected writes");

	memset(&tUpgrade, 0, sizeof(tUpgrade));
	memset(arrUpgraders, 0, sizeof(arrUpgraders));
	tUpgrade.Lock = pLock;
	testRequire(xrtMutexInit(&tUpgrade.Barrier), "rwlock upgrade barrier init failed");
	for ( size_t i = 0; i < 2; i++ ) {
		arrUpgraders[i].Proc = testRWLockUpgradeWorker;
		arrUpgraders[i].Data = &tUpgrade;
	}
	testThreadsStart(arrUpgraders, 2);
	testThreadsJoin(arrUpgraders, 2);
	for ( size_t i = 0; i < 2; i++ ) {
		testRequire(arrUpgraders[i].Result == 0, "concurrent rwlock upgrade failed");
	}
	testRequire(tUpgrade.Upgraded == 2, "rwlock lost a concurrent upgrader");
	testRequire(xrtMutexUnit(&tUpgrade.Barrier), "rwlock upgrade barrier unit failed");
	testRequire(xrtRWLockDestroy(pLock), "owned rwlock destroy failed");

	printf("[PASS] rwlock\n");
	return 0;
}
