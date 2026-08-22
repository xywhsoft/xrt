#include "../test.h"
#include "../test_thread.h"



typedef struct testmutexstate {
	xmutex* Mutex;
	int Counter;
} testmutexstate;



/* 在互斥锁保护下累加共享计数。 */
static int testMutexWorker(ptr pData)
{
	testmutexstate* pState = (testmutexstate*)pData;

	for ( size_t i = 0; i < 20000; i++ ) {
		if ( !xrtMutexLock(pState->Mutex) ) {
			return 1;
		}
		pState->Counter++;
		if ( !xrtMutexUnlock(pState->Mutex) ) {
			return 2;
		}
	}
	return 0;
}



/* 非持有线程不能释放另一个线程持有的互斥锁。 */
static int testMutexWrongOwner(ptr pData)
{
	xmutex* pMutex = (xmutex*)pData;

	if ( xrtMutexUnlock(pMutex) ) {
		return 1;
	}
	return xrtErrorKind(xrtGetError()) == XERR_STATE ? 0 : 2;
}



/* 验证嵌入式、拥有式、非递归和并发互斥语义。 */
int main(void)
{
	xmutex tMutex;
	xmutex* pMutex;
	testmutexstate tState;
	testthread arrThreads[4];
	testthread tWrongOwner;

	memset(&tMutex, 0, sizeof(tMutex));
	testRequire(xrtMutexInit(&tMutex), "in-place mutex init failed");
	testRequire(xrtMutexLock(&tMutex), "in-place mutex lock failed");
	testRequire(!xrtMutexTryLock(&tMutex), "recursive mutex try-lock succeeded");
	testRequire(!xrtMutexLock(&tMutex), "recursive mutex lock succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "recursive mutex error mismatch");
	xrtClearError();
	memset(&tWrongOwner, 0, sizeof(tWrongOwner));
	tWrongOwner.Proc = testMutexWrongOwner;
	tWrongOwner.Data = &tMutex;
	testThreadsStart(&tWrongOwner, 1);
	testThreadsJoin(&tWrongOwner, 1);
	testRequire(tWrongOwner.Result == 0, "wrong-owner mutex unlock was not rejected");
	testRequire(xrtMutexUnlock(&tMutex), "in-place mutex unlock failed");
	testRequire(!xrtMutexUnlock(&tMutex), "unowned mutex unlock succeeded");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE, "unowned mutex error mismatch");
	xrtClearError();
	testRequire(xrtMutexUnit(&tMutex), "in-place mutex unit failed");

	pMutex = xrtMutexCreate();
	testRequire(pMutex != NULL, "owned mutex create failed");
	tState.Mutex = pMutex;
	tState.Counter = 0;
	memset(arrThreads, 0, sizeof(arrThreads));
	for ( size_t i = 0; i < 4; i++ ) {
		arrThreads[i].Proc = testMutexWorker;
		arrThreads[i].Data = &tState;
	}
	testThreadsStart(arrThreads, 4);
	testThreadsJoin(arrThreads, 4);
	for ( size_t i = 0; i < 4; i++ ) {
		testRequire(arrThreads[i].Result == 0, "mutex worker failed");
	}
	testRequire(tState.Counter == 80000, "mutex lost protected updates");
	testRequire(xrtMutexDestroy(pMutex), "owned mutex destroy failed");

	printf("[PASS] mutex\n");
	return 0;
}
