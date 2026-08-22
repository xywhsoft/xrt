#include "../test.h"



/* 集成测试值记录跨执行上下文替换时的析构次数。 */
typedef struct testcontextvalue {
	int Id;
	int Destroyed;
} testcontextvalue;



/* 协程测试参数保存同一原生线程上依次可见的值。 */
typedef struct testcorokeycontext {
	xthreadkey* Key;
	testcontextvalue* Initial;
	testcontextvalue* FromCoroutine;
	testcontextvalue* FromHost;
	int Stage;
} testcorokeycontext;



/* XRT 线程测试参数用于验证包装器自动清理线程键。 */
typedef struct testthreadkeycontext {
	xthreadkey* Key;
	testcontextvalue* Value;
} testthreadkeycontext;



/* 记录值析构，但不释放测试栈上的存储。 */
static void testThreadKeyContextDestroy(ptr pData)
{
	testcontextvalue* pValue = (testcontextvalue*)pData;

	pValue->Destroyed++;
}



/* 在协程中验证线程值继承、修改和恢复后的双向可见性。 */
static ptr testThreadKeyCoroutine(ptr pData)
{
	testcorokeycontext* pContext = (testcorokeycontext*)pData;

	if ( xrtThreadKeyGet(pContext->Key) != pContext->Initial ) {
		pContext->Stage = -1;
		return NULL;
	}
	if ( !xrtThreadKeySet(pContext->Key, pContext->FromCoroutine) ) {
		pContext->Stage = -2;
		return NULL;
	}
	pContext->Stage = 1;
	if ( xrtCoYield() != XWAIT_OK ) {
		pContext->Stage = -3;
		return NULL;
	}
	if ( xrtThreadKeyGet(pContext->Key) != pContext->FromHost ) {
		pContext->Stage = -4;
		return NULL;
	}
	pContext->Stage = 2;
	return pContext->FromHost;
}



/* 在线程中留下一个值，由 XRT 线程包装器在退出路径自动析构。 */
static int32 testThreadKeyXrtWorker(ptr pData)
{
	testthreadkeycontext* pContext = (testthreadkeycontext*)pData;

	return xrtThreadKeySet(pContext->Key, pContext->Value) ? 0 : 1;
}



/* 验证线程局部语义不会在 Windows Fiber 与 POSIX 协程之间分叉。 */
int main(void)
{
	testcontextvalue tInitial = { 1, 0 };
	testcontextvalue tCoroutine = { 2, 0 };
	testcontextvalue tHost = { 3, 0 };
	testcontextvalue tWorker = { 4, 0 };
	testcorokeycontext tCoContext;
	testthreadkeycontext tThreadContext;
	xthreadkey* pKey;
	xcoro* pCo;
	xthread* pThread;

	memset(&tCoContext, 0, sizeof(tCoContext));
	memset(&tThreadContext, 0, sizeof(tThreadContext));
	pKey = xrtThreadKeyCreate(testThreadKeyContextDestroy);
	testRequire(pKey != NULL, "context thread key create failed");
	testRequire(xrtThreadKeySet(pKey, &tInitial), "initial thread key set failed");

	tCoContext.Key = pKey;
	tCoContext.Initial = &tInitial;
	tCoContext.FromCoroutine = &tCoroutine;
	tCoContext.FromHost = &tHost;
	pCo = xrtCoCreate(testThreadKeyCoroutine, &tCoContext, NULL);
	testRequire(pCo != NULL, "thread key coroutine create failed");
	testRequire(xrtCoResume(pCo), "thread key coroutine first resume failed");
	testRequire(tCoContext.Stage == 1, "coroutine did not inherit native thread value");
	testRequire(xrtThreadKeyGet(pKey) == &tCoroutine,
		"host did not observe coroutine thread value");
	testRequire(tInitial.Destroyed == 1, "coroutine replacement did not destroy old value");
	testRequire(xrtThreadKeySet(pKey, &tHost), "host thread key replacement failed");
	testRequire(tCoroutine.Destroyed == 1, "host replacement did not destroy coroutine value");
	testRequire(xrtCoResume(pCo), "thread key coroutine second resume failed");
	testRequire(tCoContext.Stage == 2, "coroutine did not observe host thread value");
	testRequire(xrtCoResult(pCo) == &tHost, "thread key coroutine result mismatch");
	testRequire(xrtCoDestroy(pCo), "thread key coroutine destroy failed");

	tThreadContext.Key = pKey;
	tThreadContext.Value = &tWorker;
	pThread = xrtThreadCreate(testThreadKeyXrtWorker, &tThreadContext, 0);
	testRequire(pThread != NULL, "XRT thread key worker create failed");
	testRequire(xrtThreadWait(pThread) == XWAIT_OK, "XRT thread key worker wait failed");
	testRequire(xrtThreadExitCode(pThread) == 0, "XRT thread key worker failed");
	xrtThreadDestroy(pThread);
	testRequire(tWorker.Destroyed == 1, "XRT thread exit did not clear thread key");
	testRequire(xrtThreadKeyGet(pKey) == &tHost, "worker value leaked into host thread");

	testRequire(xrtThreadKeyDestroy(pKey), "context thread key destroy failed");
	testRequire(tHost.Destroyed == 1, "context final value was not destroyed");
	testRequire(xrtCoThreadDetach(), "context coroutine runtime detach failed");

	printf("[PASS] thread key context\n");
	return 0;
}
