#include "../test.h"
#include "../test_thread.h"



/* 测试值记录线程键是否准确执行了一次析构。 */
typedef struct testkeyvalue {
	int Id;
	int Destroyed;
} testkeyvalue;



/* 重入析构值用于验证有限轮次清理和残留状态。 */
typedef struct testkeyreenter {
	xthreadkey* Key;
	int Destroyed;
	int Repeat;
} testkeyreenter;



/* 线程退出或值被替换时标记析构。 */
static void testThreadKeyDestroyValue(ptr pData)
{
	testkeyvalue* pValue = (testkeyvalue*)pData;

	pValue->Destroyed++;
}



/* 在指定次数内从析构过程重新安装同一个值。 */
static void testThreadKeyDestroyReenter(ptr pData)
{
	testkeyreenter* pValue = (testkeyreenter*)pData;

	pValue->Destroyed++;
	if ( pValue->Destroyed < pValue->Repeat ) {
		(void)xrtThreadKeySet(pValue->Key, pValue);
	}
}



/* 外部线程上下文验证不需要附加即可使用和显式清理线程键。 */
typedef struct testkeyworker {
	xthreadkey* Key;
	testkeyvalue* Value;
} testkeyworker;



/* 延迟退出线程用于验证键关闭后仍可安全清理既有槽。 */
typedef struct testkeydeferredworker {
	xthreadkey* Key;
	testkeyvalue* Value;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Ready;
		HANDLE Release;
	#else
		pthread_mutex_t Lock;
		pthread_cond_t Condition;
		bool Ready;
		bool Release;
	#endif
} testkeydeferredworker;



/* 在宿主直接创建的线程中设置并读取独立值。 */
static int testThreadKeyWorker(ptr pData)
{
	testkeyworker* pWorker = (testkeyworker*)pData;

	if ( xrtThreadKeyGet(pWorker->Key) != NULL ) {
		return 1;
	}
	if ( !xrtThreadKeySet(pWorker->Key, pWorker->Value) ) {
		return 2;
	}
	if ( xrtThreadKeyGet(pWorker->Key) != pWorker->Value ) {
		return 3;
	}
	if ( !xrtThreadKeysClear() ) {
		return 4;
	}
	return xrtThreadKeyGet(pWorker->Key) == NULL ? 0 : 5;
}



/* 安装值并等待主线程先关闭键，再执行线程槽清理。 */
static int testThreadKeyDeferredWorker(ptr pData)
{
	testkeydeferredworker* pWorker = (testkeydeferredworker*)pData;

	if ( !xrtThreadKeySet(pWorker->Key, pWorker->Value) ) {
		return 1;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( !SetEvent(pWorker->Ready) ) {
			return 2;
		}
		if ( WaitForSingleObject(pWorker->Release, INFINITE) != WAIT_OBJECT_0 ) {
			return 3;
		}
	#else
		(void)pthread_mutex_lock(&pWorker->Lock);
		pWorker->Ready = true;
		(void)pthread_cond_broadcast(&pWorker->Condition);
		while ( !pWorker->Release ) {
			(void)pthread_cond_wait(&pWorker->Condition, &pWorker->Lock);
		}
		(void)pthread_mutex_unlock(&pWorker->Lock);
	#endif
	return xrtThreadKeysClear() ? 0 : 4;
}



/* 等待工作线程已经安装非空线程槽。 */
static void testThreadKeyDeferredWait(testkeydeferredworker* pWorker)
{
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(
			WaitForSingleObject(pWorker->Ready, INFINITE) == WAIT_OBJECT_0,
			"deferred thread key worker ready wait failed"
		);
	#else
		(void)pthread_mutex_lock(&pWorker->Lock);
		while ( !pWorker->Ready ) {
			(void)pthread_cond_wait(&pWorker->Condition, &pWorker->Lock);
		}
		(void)pthread_mutex_unlock(&pWorker->Lock);
	#endif
}



/* 允许工作线程在键关闭后继续退出清理。 */
static void testThreadKeyDeferredRelease(testkeydeferredworker* pWorker)
{
	#if defined(_WIN32) || defined(_WIN64)
		testRequire(SetEvent(pWorker->Release),
			"deferred thread key worker release failed");
	#else
		(void)pthread_mutex_lock(&pWorker->Lock);
		pWorker->Release = true;
		(void)pthread_cond_broadcast(&pWorker->Condition);
		(void)pthread_mutex_unlock(&pWorker->Lock);
	#endif
}



/* 验证所有权、线程隔离、自动退出清理和销毁语义。 */
int main(void)
{
	xthreadkey* pKey = xrtThreadKeyCreate(testThreadKeyDestroyValue);
	testkeyvalue tFirst = { 1, 0 };
	testkeyvalue tSecond = { 2, 0 };
	testkeyvalue tThird = { 3, 0 };
	testkeyvalue tCleared = { 4, 0 };
	testkeyvalue arrValue[4];
	testkeyworker arrWorker[4];
	testthread arrThread[4];
	testkeyreenter tReenter;
	xthreadkey* pReenterKey;
	testkeydeferredworker tDeferred;
	testkeyvalue tDeferredValue = { 5, 0 };
	testthread tDeferredThread;
	xthreadkey* pDeferredKey;

	testRequire(pKey != NULL, "thread key create failed");
	testRequire(xrtThreadKeyGet(pKey) == NULL, "new thread key was not empty");
	testRequire(xrtThreadKeySet(pKey, &tFirst), "thread key first set failed");
	testRequire(xrtThreadKeyGet(pKey) == &tFirst, "thread key get mismatch");
	testRequire(xrtThreadKeySet(pKey, &tFirst), "thread key same-value set failed");
	testRequire(tFirst.Destroyed == 0, "same thread key value was destroyed");
	testRequire(xrtThreadKeySet(pKey, &tSecond), "thread key replacement failed");
	testRequire(tFirst.Destroyed == 1, "replaced thread key value was not destroyed");
	testRequire(xrtThreadKeyTake(pKey) == &tSecond, "thread key take mismatch");
	testRequire(tSecond.Destroyed == 0, "taken thread key value was destroyed");
	testRequire(xrtThreadKeyGet(pKey) == NULL, "taken thread key remained installed");

	memset(arrValue, 0, sizeof(arrValue));
	memset(arrWorker, 0, sizeof(arrWorker));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < 4; i++ ) {
		arrValue[i].Id = (int)i + 10;
		arrWorker[i].Key = pKey;
		arrWorker[i].Value = &arrValue[i];
		arrThread[i].Proc = testThreadKeyWorker;
		arrThread[i].Data = &arrWorker[i];
	}
	testThreadsStart(arrThread, 4);
	testThreadsJoin(arrThread, 4);
	for ( size_t i = 0; i < 4; i++ ) {
		testRequire(arrThread[i].Result == 0, "external thread key worker failed");
		testRequire(arrValue[i].Destroyed == 1, "thread exit did not destroy key value");
	}
	testRequire(xrtThreadKeyGet(pKey) == NULL, "worker thread value leaked into main thread");

	testRequire(xrtThreadKeySet(pKey, &tCleared), "thread key clear value set failed");
	testRequire(xrtThreadKeysClear(), "thread key current-thread clear failed");
	testRequire(tCleared.Destroyed == 1, "thread key clear did not destroy value");
	testRequire(xrtThreadKeyGet(pKey) == NULL, "thread key clear retained value");
	testRequire(xrtThreadKeysClear(), "empty thread key clear failed");

	testRequire(xrtThreadKeySet(pKey, &tThird), "thread key final set failed");
	testRequire(xrtThreadKeyDestroy(pKey), "thread key destroy failed");
	testRequire(tThird.Destroyed == 1, "thread key destroy did not destroy current value");
	testRequire(xrtThreadKeyDestroy(NULL), "null thread key destroy failed");
	testRequire(!xrtThreadKeySet(NULL, &tFirst), "null thread key set unexpectedly succeeded");
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null thread key error mismatch"
	);

	memset(&tReenter, 0, sizeof(tReenter));
	pReenterKey = xrtThreadKeyCreate(testThreadKeyDestroyReenter);
	testRequire(pReenterKey != NULL, "reentrant thread key create failed");
	tReenter.Key = pReenterKey;
	tReenter.Repeat = 3;
	testRequire(xrtThreadKeySet(pReenterKey, &tReenter), "reentrant value set failed");
	testRequire(xrtThreadKeysClear(), "finite reentrant thread key clear failed");
	testRequire(tReenter.Destroyed == 3, "finite reentrant clear pass mismatch");
	testRequire(xrtThreadKeyGet(pReenterKey) == NULL, "finite reentrant value remained");

	tReenter.Destroyed = 0;
	tReenter.Repeat = 5;
	testRequire(xrtThreadKeySet(pReenterKey, &tReenter), "overflow reentrant value set failed");
	xrtClearError();
	testRequire(!xrtThreadKeysClear(), "unbounded reentrant clear unexpectedly succeeded");
	testRequire(tReenter.Destroyed == 4, "reentrant clear limit mismatch");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_STATE,
		"reentrant clear limit error mismatch");
	testRequire(xrtThreadKeyTake(pReenterKey) == &tReenter,
		"reentrant residual value take failed");
	testRequire(xrtThreadKeyDestroy(pReenterKey), "reentrant thread key destroy failed");

	memset(&tDeferred, 0, sizeof(tDeferred));
	memset(&tDeferredThread, 0, sizeof(tDeferredThread));
	pDeferredKey = xrtThreadKeyCreate(testThreadKeyDestroyValue);
	testRequire(pDeferredKey != NULL, "deferred thread key create failed");
	tDeferred.Key = pDeferredKey;
	tDeferred.Value = &tDeferredValue;
	#if defined(_WIN32) || defined(_WIN64)
		tDeferred.Ready = CreateEvent(NULL, TRUE, FALSE, NULL);
		tDeferred.Release = CreateEvent(NULL, TRUE, FALSE, NULL);
		testRequire((tDeferred.Ready != NULL) && (tDeferred.Release != NULL),
			"deferred thread key event create failed");
	#else
		testRequire(pthread_mutex_init(&tDeferred.Lock, NULL) == 0,
			"deferred thread key mutex create failed");
		testRequire(pthread_cond_init(&tDeferred.Condition, NULL) == 0,
			"deferred thread key condition create failed");
	#endif
	tDeferredThread.Proc = testThreadKeyDeferredWorker;
	tDeferredThread.Data = &tDeferred;
	testThreadsStart(&tDeferredThread, 1);
	testThreadKeyDeferredWait(&tDeferred);
	testRequire(xrtThreadKeyDestroy(pDeferredKey),
		"thread key close with remote slot failed");
	testRequire(tDeferredValue.Destroyed == 0,
		"remote thread key value was destroyed on wrong thread");
	testThreadKeyDeferredRelease(&tDeferred);
	testThreadsJoin(&tDeferredThread, 1);
	testRequire(tDeferredThread.Result == 0,
		"deferred thread key worker cleanup failed");
	testRequire(tDeferredValue.Destroyed == 1,
		"deferred thread key value was not destroyed once");
	#if defined(_WIN32) || defined(_WIN64)
		CloseHandle(tDeferred.Release);
		CloseHandle(tDeferred.Ready);
	#else
		(void)pthread_cond_destroy(&tDeferred.Condition);
		(void)pthread_mutex_destroy(&tDeferred.Lock);
	#endif

	printf("[PASS] thread key\n");
	return 0;
}
