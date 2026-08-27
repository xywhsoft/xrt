#include "../test.h"



typedef struct testcallableenv {
	int DropCount;
	int ProgressCount;
	uint64 ProgressInput;
	uint64 ProgressTotal;
	uint64 ProgressOutput;
} testcallableenv;



/* 返回调用帧中的第一个参数。 */
static bool testCallableEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	return xrtCallResultPush(
		pResult, xrtCallFrameParameter(pFrame, 0u));
}



/* 记录三项进度参数，并在输入尚未达到总量时允许继续。 */
static bool testProgressEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	testcallableenv* pEnv = (testcallableenv*)pEnvironment;
	bool bContinue;

	if ( (pFrame == NULL) || (pFrame->ArgumentCount != 3u) ||
		 !xrtValueGetUInt(pFrame->Arguments[0], &pEnv->ProgressInput) ||
		 !xrtValueGetUInt(pFrame->Arguments[1], &pEnv->ProgressTotal) ||
		 !xrtValueGetUInt(pFrame->Arguments[2], &pEnv->ProgressOutput) ) {
		return false;
	}
	pEnv->ProgressCount++;
	bContinue = pEnv->ProgressInput < pEnv->ProgressTotal;
	return xrtCallResultPush(pResult, xrtValueBool(bContinue));
}



/* 记录 callable 环境析构。 */
static void testCallableDrop(ptr pEnvironment)
{
	testcallableenv* pEnv = (testcallableenv*)pEnvironment;

	pEnv->DropCount++;
}



/* 验证最近一次错误来自运行时 callable Value 桥接层。 */
static void testRuntimeValueCallableError(
	xruntimevalueerror Code,
	cstr sOperation
)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime callable Value error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.runtime-value") == 0,
		"runtime callable Value error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code,
		"runtime callable Value error code mismatch");
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime callable Value error operation mismatch"
	);
}



/* 验证 callable 装箱、调用、身份比较、签名和 Take 所有权。 */
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
	testcallableenv Env = { 0 };
	testcallableenv TakeEnv = { 0 };
	testcallableenv ProgressEnv = { 0 };
	xrtcallable* pCallable = xrtCallableCreate(
		&Signature, testCallableEntry, &Env, testCallableDrop);
	xrtcallable* pTaken = xrtCallableCreate(
		&Signature, testCallableEntry, &TakeEnv, testCallableDrop);
	xrtcallable* pEmpty = NULL;
	xrtcallable* pProgressCallable = xrtCallableCreate(
		NULL, testProgressEntry, &ProgressEnv, testCallableDrop);
	xvalue* pFirst;
	xvalue* pSecond;
	xvalue* pTakenValue;
	xvalue* pProgressValue;
	xvalue* pArgument = xrtValueInt(42);
	xvalue* Arguments[1] = { pArgument };
	xrtcallframe Frame = {
		.ArgumentCount = 1u,
		.Arguments = Arguments
	};
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	uint64 iFirstHash;
	uint64 iSecondHash;
	int64 iValue;
	xrtprogresscall ProgressCall;
	xrtprogress Progress = {
		.iSize = sizeof(Progress),
		.iVersion = XRT_PROGRESS_VERSION,
		.iInputBytes = 4u,
		.iTotalInputBytes = 9u,
		.iOutputBytes = 2u
	};

	testRequire((pCallable != NULL) && (pTaken != NULL) &&
		(pProgressCallable != NULL) && (pArgument != NULL),
		"runtime callable Value fixture failed");
	pFirst = xrtValueCallable(pCallable);
	pSecond = xrtValueCallable(pCallable);
	testRequire((pFirst != NULL) && (pSecond != NULL),
		"runtime callable Value creation failed");
	testRequire(xrtValueIsCallable(pFirst),
		"callable Value kind was not recognized");
	testRequire(!xrtValueIsCallable(xrtValueBool(true)),
		"boolean was recognized as callable");
	testRequire(xrtValueGetCallable(pFirst) == pCallable,
		"callable Value identity mismatch");
	testRequire(xrtValueCallableSignature(pFirst) == &Signature,
		"callable Value signature mismatch");
	testRequire(
		xrtValueHash(pFirst, &iFirstHash) &&
		xrtValueHash(pSecond, &iSecondHash) &&
		(iFirstHash == iSecondHash) &&
		xrtValueScalarEqual(pFirst, pSecond),
		"callable Value identity hash/equality mismatch"
	);
	testRequire(xrtValueInvoke(pFirst, &Frame, &Result),
		"callable Value invocation failed");
	testRequire(
		xrtValueGetInt(xrtCallResultGet(&Result, 0u), &iValue) &&
		(iValue == 42),
		"callable Value result mismatch"
	);
	pProgressValue = xrtValueCallable(pProgressCallable);
	testRequire(pProgressValue != NULL,
		"progress callable Value creation failed");
	xrtProgressCallInit(&ProgressCall, pProgressValue);
	testRequire(xrtProgressCallInvoke(&Progress, &ProgressCall) &&
		!ProgressCall.InvokeFailed &&
		(ProgressEnv.ProgressCount == 1) &&
		(ProgressEnv.ProgressInput == 4) &&
		(ProgressEnv.ProgressTotal == 9) &&
		(ProgressEnv.ProgressOutput == 2),
		"progress callable bridge mismatch");
	testRequire(!xrtProgressCallInvoke(NULL, &ProgressCall) &&
		ProgressCall.InvokeFailed,
		"invalid progress event did not fail the bridge");
	xrtProgressCallInit(&ProgressCall, xrtValueNull());
	testRequire(xrtProgressCallInvoke(NULL, &ProgressCall) &&
		!ProgressCall.InvokeFailed,
		"disabled progress bridge must be a no-op");
	xrtValueRelease(pProgressValue);
	xrtCallableUnref(pProgressCallable);
	testRequire(ProgressEnv.DropCount == 1,
		"progress callable environment drop count mismatch");

	pTakenValue = xrtValueCallableTake(&pTaken);
	testRequire((pTakenValue != NULL) && (pTaken == NULL),
		"callable Value Take did not consume source");
	xrtCallableUnref(pCallable);
	testRequire(Env.DropCount == 0,
		"callable Value released environment too early");
	xrtValueRelease(pSecond);
	xrtValueRelease(pFirst);
	testRequire(Env.DropCount == 1,
		"callable Value environment drop count mismatch");
	xrtValueRelease(pTakenValue);
	testRequire(TakeEnv.DropCount == 1,
		"taken callable Value environment drop count mismatch");

	/* 构造、Take、Getter、签名和调用使用统一的桥接错误。 */
	xrtClearError();
	testRequire(xrtValueCallable(NULL) == NULL,
		"null callable produced a Value");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_CALLABLE, "callable");
	xrtClearError();
	testRequire(xrtValueCallableTake(NULL) == NULL,
		"null callable source was accepted");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_OWNERSHIP, "callable-take");
	xrtClearError();
	testRequire(xrtValueCallableTake(&pEmpty) == NULL,
		"empty callable source was accepted");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_OWNERSHIP, "callable-take");
	xrtClearError();
	testRequire(xrtValueGetCallable(xrtValueBool(true)) == NULL,
		"non-callable Value produced a callable");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_TYPE, "callable-get");
	xrtClearError();
	testRequire(xrtValueCallableSignature(xrtValueBool(true)) == NULL,
		"non-callable Value produced a callable signature");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_TYPE, "callable-get");
	xrtClearError();
	testRequire(!xrtValueInvoke(
		xrtValueBool(true), &Frame, &Result
	), "non-callable Value was invoked");
	testRuntimeValueCallableError(
		XRUNTIME_VALUE_ERROR_TYPE, "callable-get");

	xrtCallResultUnit(&Result);
	xrtValueRelease(pArgument);
	xrtClearError();
	printf("[PASS] runtime Value callable\n");
	return 0;
}
