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



static int testMutexTryBusy(ptr Data)
{
    xmutex* M=Data;xrtSetErrorKind(XERR_ARGUMENT);const xerror* Old=xrtGetError();
    bool OK=xrtMutexTryLockResult(M)==XMUTEX_TRY_BUSY && xrtGetError()==Old && !xrtMutexTryLock(M) && xrtGetError()==Old;xrtClearError();return OK?0:1;
}
static void testMutexTryResult(void)
{
    xmutex M,Bad;memset(&Bad,0,sizeof(Bad));testRequire(xrtMutexInit(&M),"typed try init");
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"typed try budget");
    xrtSetErrorKind(XERR_ARGUMENT);const xerror* Old=xrtGetError();
    testRequire(xrtMutexTryLockResult(NULL)==XMUTEX_TRY_ERROR && xrtGetError()==Old,"typed invalid input distinct from busy despite identical error");
    testRequire(xrtMutexTryLockResult(&Bad)==XMUTEX_TRY_ERROR && xrtErrorKind(xrtGetError())==XERR_STATE,"typed uninitialized storage");Old=xrtGetError();
    testRequire(xrtMutexTryLockResult(&Bad)==XMUTEX_TRY_ERROR && xrtGetError()==Old,"same static failure still ERROR");
    for(unsigned I=0;I<20000;++I){
        testRequire(xrtMutexTryLockResult(&M)==XMUTEX_TRY_OK && xrtGetError()==Old,"typed immediate ownership");
        testRequire(xrtMutexTryLockResult(&M)==XMUTEX_TRY_BUSY && !xrtMutexTryLock(&M) && xrtGetError()==Old,"same-thread pending preserves ambient error");
        testRequire(xrtMutexUnlock(&M),"typed try release");
    }
    testRequire(xrtAllocationBudgetLeave(&Scope),"typed try budget leave");xallocationbudgetinfo Info={0};Info.Size=sizeof(Info);testRequire(xrtAllocationBudgetInfo(B,&Info) && !Info.LiveBytes && !Info.LiveCount && !Info.Rejections,"typed normal/static-error paths require no allocation");xrtAllocationBudgetFree(B);xrtClearError();
    testRequire(xrtMutexTryLock(&M),"legacy bool ownership");testthread T={0};T.Proc=testMutexTryBusy;T.Data=&M;testThreadsStart(&T,1);testThreadsJoin(&T,1);testRequire(!T.Result,"foreign thread typed busy does not block");testRequire(xrtMutexUnlock(&M) && xrtMutexUnit(&M),"typed try cleanup");
}

/* 验证嵌入式、拥有式、非递归和并发互斥语义。 */
int main(void)
{
    testMutexTryResult();
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
