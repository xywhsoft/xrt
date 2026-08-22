#include "../test.h"
#include "../test_thread.h"



#define TEST_CALL_THREAD_COUNT 4
#define TEST_CALL_ITERATIONS 20000



typedef struct testcallthreadenv {
	volatile int32 DropCount;
} testcallthreadenv;



typedef struct testcallthreadcontext {
	xrtcallable* Callable;
	xvalue* Argument;
} testcallthreadcontext;



/* 并发入口直接增加参数引用作为单返回值，避免测试循环的分配噪声。 */
static bool testCallThreadEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	return xrtCallResultPush(
		pResult,
		xrtCallFrameArgument(pFrame, 0u)
	);
}



/* 原子记录环境析构次数。 */
static void testCallThreadDrop(ptr pEnvironment)
{
	testcallthreadenv* pEnv = (testcallthreadenv*)pEnvironment;

	#if defined(_WIN32) || defined(_WIN64)
		(void)InterlockedIncrement((volatile LONG*)&pEnv->DropCount);
	#elif defined(__GNUC__) || defined(__clang__)
		(void)__sync_add_and_fetch(&pEnv->DropCount, 1);
	#else
		pEnv->DropCount++;
	#endif
}



/* 并发增加引用、调用和释放同一个不可变 callable。 */
static int testCallThreadRun(ptr pData)
{
	testcallthreadcontext* pContext = (testcallthreadcontext*)pData;
	xvalue* Arguments[1] = { pContext->Argument };
	xrtcallframe Frame = {
		.ArgumentCount = 1u,
		.Arguments = Arguments
	};

	for ( int i = 0; i < TEST_CALL_ITERATIONS; i++ ) {
		xrtcallresult Result = XRT_CALL_RESULT_INIT;
		xrtcallable* pTemporary = xrtCallableRef(pContext->Callable);

		if ( pTemporary == NULL ) {
			return 1;
		}
		xrtCallableUnref(pTemporary);
		if ( !xrtCallableInvoke(pContext->Callable, &Frame, &Result) ) {
			return 2;
		}
		if (
			(xrtCallResultCount(&Result) != 1u) ||
			(xrtCallResultGet(&Result, 0u) != pContext->Argument)
		) {
			xrtCallResultUnit(&Result);
			return 3;
		}
		xrtCallResultUnit(&Result);
	}
	xrtValueRelease(pContext->Argument);
	xrtCallableUnref(pContext->Callable);
	return 0;
}



/* 验证不可变 callable 的并发调用和最后引用析构边界。 */
int main(void)
{
	xrtparamdesc Param = {
		XRT_STR_INIT("value"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u
	};
	const xrttype* Return = xrtTypeInt64();
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("identity"),
		.ParamCount = 1u,
		.Params = &Param,
		.ReturnCount = 1u,
		.ReturnTypes = &Return
	};
	testcallthreadenv Env = { 0 };
	xrtcallable* pCallable = xrtCallableCreate(
		&Signature,
		testCallThreadEntry,
		&Env,
		testCallThreadDrop
	);
	testcallthreadcontext arrContext[TEST_CALL_THREAD_COUNT];
	testthread arrThreads[TEST_CALL_THREAD_COUNT];

	testRequire(pCallable != NULL, "thread callable creation failed");
	for ( int i = 0; i < TEST_CALL_THREAD_COUNT; i++ ) {
		arrContext[i].Callable = xrtCallableRef(pCallable);
		arrContext[i].Argument = xrtValueInt(i);
		testRequire(
			(arrContext[i].Callable != NULL) &&
			(arrContext[i].Argument != NULL),
			"thread callable context creation failed"
		);
		arrThreads[i].Proc = testCallThreadRun;
		arrThreads[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThreads, TEST_CALL_THREAD_COUNT);
	xrtCallableUnref(pCallable);
	testThreadsJoin(arrThreads, TEST_CALL_THREAD_COUNT);
	for ( int i = 0; i < TEST_CALL_THREAD_COUNT; i++ ) {
		testRequire(arrThreads[i].Result == 0,
			"concurrent callable worker failed");
	}
	testRequire(Env.DropCount == 1,
		"concurrent callable environment drop count mismatch");
	printf("[PASS] runtime call threads\n");
	return 0;
}
