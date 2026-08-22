#include "../test.h"



typedef struct testcallenv {
	int64 Offset;
	int* DropCount;
} testcallenv;



/* 验证最近一次错误属于动态调用模块的指定操作。 */
static void testCallError(xcallerror Code, cstr sOperation)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime call error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.call") == 0,
		"runtime call error domain mismatch"
	);
	testRequire(xrtErrorCode(pError) == (int32)Code,
		"runtime call error code mismatch");
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime call error operation mismatch"
	);
}



/* 创建整数并把唯一引用追加到调用结果。 */
static bool testCallPushInt(xrtcallresult* pResult, int64 iValue)
{
	xvalue* pValue = xrtValueInt(iValue);

	if ( pValue == NULL ) {
		return false;
	}
	if ( !xrtCallResultPushTake(pResult, &pValue) ) {
		xrtValueRelease(pValue);
		return false;
	}
	return true;
}



/* 复用旧版测试场景，读取位置参数和关键字并返回五个值。 */
static bool testCallEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	testcallenv* pEnv = (testcallenv*)pEnvironment;
	xvalue* pBase = xrtCallFrameParameter(pFrame, 0u);
	xvalue* pBonus = xrtCallFrameParameter(pFrame, 1u);
	int64 iBase;
	int64 iBonus = 0;

	if (
		(pFrame->Context != pEnvironment) ||
		!xrtValueGetInt(pBase, &iBase) ||
		((pBonus != NULL) && !xrtValueGetInt(pBonus, &iBonus))
	) {
		return false;
	}
	return testCallPushInt(pResult, iBase + iBonus + pEnv->Offset) &&
		testCallPushInt(pResult, (int64)pFrame->KeywordCount) &&
		testCallPushInt(pResult, 22) &&
		testCallPushInt(pResult, 33) &&
		testCallPushInt(pResult, 444);
}



/* 在写入部分临时结果后失败，用于验证失败原子提交和原因链。 */
static bool testCallFailEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	int64 iUnused;

	(void)pEnvironment;
	(void)pFrame;
	if ( !testCallPushInt(pResult, 123) ) {
		return false;
	}
	(void)xrtValueGetInt(xrtValueBool(true), &iUnused);
	return false;
}



/* 返回错误数量的值，用于验证签名返回契约。 */
static bool testCallShortEntry(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	(void)pEnvironment;
	(void)pFrame;
	return testCallPushInt(pResult, 1);
}



/* 记录 callable 环境只被最后一个引用释放一次。 */
static void testCallDrop(ptr pEnvironment)
{
	testcallenv* pEnv = (testcallenv*)pEnvironment;

	(*pEnv->DropCount)++;
}



/* 验证普通命名参数参与签名身份且全部参数名保持唯一。 */
static void testCallSignatureNames(void)
{
	xrtparamdesc LeftParam = {
		XRT_STR_INIT("left"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u
	};
	xrtparamdesc RightParam = {
		XRT_STR_INIT("right"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u
	};
	xrtparamdesc Duplicate[2] = { LeftParam, LeftParam };
	xrtfunctionsig Left = {
		.Name = XRT_STR_INIT("named"),
		.ParamCount = 1u,
		.Params = &LeftParam
	};
	xrtfunctionsig Right = Left;
	xrtfunctionsig Bad = Left;

	Right.Params = &RightParam;
	Bad.ParamCount = 2u;
	Bad.Params = Duplicate;
	testRequire(xrtFunctionSigValidate(&Left),
		"valid named signature was rejected");
	testRequire(
		xrtFunctionSigId(&Left) != xrtFunctionSigId(&Right),
		"ordinary parameter names did not affect signature identity"
	);
	xrtClearError();
	testRequire(!xrtFunctionSigValidate(&Bad),
		"duplicate parameter names were accepted");
	testRequire(
		strcmp(xrtErrorOperation(xrtGetError()), "signature-validate") == 0,
		"signature validation operation mismatch"
	);
}



/* 验证位置、关键字、可选、变长和重复参数边界。 */
static void testCallFrameBoundaries(const xrtfunctionsig* pSignature)
{
	xvalue* pArgs[2] = { xrtValueInt(1), xrtValueInt(2) };
	xvalue* pKeywords[2] = { xrtValueInt(3), xrtValueInt(4) };
	xstrview Names[2] = {
		XRT_STR_LITERAL("bonus"), XRT_STR_LITERAL("bonus")
	};
	xrtcallframe Frame = {
		.Signature = pSignature,
		.ArgumentCount = 1u,
		.Arguments = pArgs
	};
	xrtfunctionsig Flexible = *pSignature;

	testRequire(xrtCallFrameValidate(&Frame),
		"valid positional frame was rejected");
	Frame.ArgumentCount = 0u;
	testRequire(!xrtCallFrameValidate(&Frame),
		"missing required parameter was accepted");
	testCallError(XCALL_ERROR_FRAME, "frame-validate");

	Frame.ArgumentCount = 2u;
	testRequire(!xrtCallFrameValidate(&Frame),
		"extra positional parameter was accepted");

	Frame.ArgumentCount = 1u;
	Frame.KeywordCount = 1u;
	Frame.KeywordNames = Names;
	Frame.KeywordValues = pKeywords;
	testRequire(xrtCallFrameValidate(&Frame),
		"known optional keyword was rejected");
	Names[0] = XRT_STR_LITERAL("base");
	testRequire(!xrtCallFrameValidate(&Frame),
		"duplicate positional and keyword parameter was accepted");

	Names[0] = XRT_STR_LITERAL("unknown");
	testRequire(!xrtCallFrameValidate(&Frame),
		"unknown keyword was accepted");
	Frame.KeywordCount = 2u;
	Names[0] = XRT_STR_LITERAL("bonus");
	testRequire(!xrtCallFrameValidate(&Frame),
		"duplicate keyword names were accepted");

	Flexible.Flags = XRT_FUNCTION_FLAG_VARARGS | XRT_FUNCTION_FLAG_KWARGS;
	Frame.Signature = &Flexible;
	Frame.ArgumentCount = 2u;
	Frame.KeywordCount = 1u;
	Names[0] = XRT_STR_LITERAL("unknown");
	testRequire(xrtCallFrameValidate(&Frame),
		"varargs and kwargs frame was rejected");

	xrtValueRelease(pArgs[0]);
	xrtValueRelease(pArgs[1]);
	xrtValueRelease(pKeywords[0]);
	xrtValueRelease(pKeywords[1]);
}



/* 验证结果内联、溢出、替换、移动和所有权接口。 */
static void testCallResultLifecycle(void)
{
	xrtcallresult Source = XRT_CALL_RESULT_INIT;
	xrtcallresult Target = XRT_CALL_RESULT_INIT;
	xvalue* pOwned = xrtValueInt(90);
	int64 iValue;

	for ( int64 i = 0; i < 5; i++ ) {
		testRequire(testCallPushInt(&Source, i),
			"result append failed");
	}
	testRequire(Source.Count == 5u, "result count mismatch");
	testRequire(Source.Overflow != NULL,
		"fifth result did not use overflow storage");
	testRequire(
		xrtCallResultSetTake(&Source, 2u, &pOwned) && (pOwned == NULL),
		"result take replacement failed"
	);
	testRequire(
		xrtValueGetInt(xrtCallResultGet(&Source, 2u), &iValue) &&
		(iValue == 90),
		"result replacement value mismatch"
	);
	testRequire(xrtCallResultMove(&Target, &Source),
		"result move failed");
	testRequire((Source.Count == 0u) && (Source.Overflow == NULL),
		"result move did not clear source");
	testRequire(xrtCallResultCount(&Target) == 5u,
		"result move target count mismatch");
	xrtCallResultClear(&Target);
	testRequire(
		(Target.Count == 0u) && (Target.Overflow != NULL),
		"result clear did not preserve capacity"
	);
	xrtCallResultUnit(&Target);
	xrtCallResultUnit(&Source);
}



/* 验证 callable 的调用、失败原子性、签名和环境生命周期。 */
int main(void)
{
	xrtparamdesc Params[2] = {
		{ XRT_STR_INIT("base"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u },
		{ XRT_STR_INIT("bonus"), xrtTypeInt64(), XRT_PARAM_DEFAULT,
			XRT_PARAM_FLAG_OPTIONAL | XRT_PARAM_FLAG_NAMED_ONLY }
	};
	const xrttype* Returns[5] = {
		xrtTypeInt64(), xrtTypeInt64(), xrtTypeInt64(),
		xrtTypeInt64(), xrtTypeInt64()
	};
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("calculate"),
		.ParamCount = 2u,
		.Params = Params,
		.ReturnCount = 5u,
		.ReturnTypes = Returns
	};
	int iDropCount = 0;
	testcallenv Env = { 10, &iDropCount };
	xvalue* pArgument = xrtValueInt(24);
	xvalue* pBonus = xrtValueInt(8);
	xvalue* Arguments[1] = { pArgument };
	xvalue* Keywords[1] = { pBonus };
	xstrview Names[1] = { XRT_STR_LITERAL("bonus") };
	xvalue* NamedKeywords[2] = { pArgument, pBonus };
	xstrview NamedNames[2] = {
		XRT_STR_LITERAL("base"), XRT_STR_LITERAL("bonus")
	};
	xrtcallframe Frame = {
		.ArgumentCount = 1u,
		.Arguments = Arguments,
		.KeywordCount = 1u,
		.KeywordNames = Names,
		.KeywordValues = Keywords,
		.Context = &Env
	};
	xrtcallframe NamedFrame = {
		.KeywordCount = 2u,
		.KeywordNames = NamedNames,
		.KeywordValues = NamedKeywords,
		.Context = &Env
	};
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	xrtfunctionsig OtherSignature = Signature;
	xrtcallable* pCallable;
	xrtcallable* pReference;
	xrtcallable* pFail;
	xrtcallable* pShort;
	int64 iValue;

	testCallSignatureNames();
	testCallFrameBoundaries(&Signature);
	testCallResultLifecycle();
	pCallable = xrtCallableCreate(&Signature, testCallEntry, &Env, testCallDrop);
	testRequire(pCallable != NULL, "callable creation failed");
	testRequire(xrtCallableSignature(pCallable) == &Signature,
		"callable signature mismatch");
	testRequire(
		xrtCallableSignatureId(pCallable) == xrtFunctionSigId(&Signature),
		"callable signature ID mismatch"
	);
	testRequire(xrtCallableInvoke(pCallable, &Frame, &Result),
		"callable invocation failed");
	testRequire(xrtCallResultCount(&Result) == 5u,
		"callable multi-result count mismatch");
	testRequire(
		xrtValueGetInt(xrtCallResultGet(&Result, 0u), &iValue) &&
		(iValue == 42),
		"callable first result mismatch"
	);
	testRequire(
		xrtValueGetInt(xrtCallResultGet(&Result, 4u), &iValue) &&
		(iValue == 444),
		"callable overflow result mismatch"
	);
	testRequire(xrtCallableInvoke(pCallable, &NamedFrame, &Result),
		"named positional parameter invocation failed");
	testRequire(
		xrtValueGetInt(xrtCallResultGet(&Result, 0u), &iValue) &&
		(iValue == 42),
		"named positional parameter result mismatch"
	);

	OtherSignature.Flags = XRT_FUNCTION_FLAG_VARARGS;
	Frame.Signature = &OtherSignature;
	testRequire(!xrtCallableInvoke(pCallable, &Frame, &Result),
		"mismatched explicit frame signature was accepted");
	testCallError(XCALL_ERROR_SIGNATURE, "invoke");
	testRequire(xrtCallResultCount(&Result) == 5u,
		"signature mismatch changed existing result");
	Frame.Signature = NULL;

	pFail = xrtCallableCreate(NULL, testCallFailEntry, NULL, NULL);
	testRequire(pFail != NULL, "failing callable creation failed");
	testRequire(!xrtCallableInvoke(pFail, NULL, &Result),
		"failing callable invocation succeeded");
	testCallError(XCALL_ERROR_ENTRY, "invoke");
	testRequire(xrtErrorCause(xrtGetError()) != NULL,
		"failing callable lost its cause error");
	testRequire(xrtCallResultCount(&Result) == 5u,
		"failed invocation changed existing result");

	pShort = xrtCallableCreate(&Signature, testCallShortEntry, NULL, NULL);
	testRequire(pShort != NULL, "short callable creation failed");
	testRequire(!xrtCallableInvoke(pShort, &Frame, &Result),
		"wrong return count was accepted");
	testCallError(XCALL_ERROR_RESULT, "invoke");
	testRequire(xrtCallResultCount(&Result) == 5u,
		"return count failure changed existing result");

	pReference = xrtCallableRef(pCallable);
	testRequire(pReference == pCallable, "callable ref returned another object");
	xrtCallableUnref(pCallable);
	testRequire(iDropCount == 0, "callable environment dropped too early");
	xrtCallableUnref(pReference);
	testRequire(iDropCount == 1, "callable environment drop count mismatch");

	xrtCallableUnref(pShort);
	xrtCallableUnref(pFail);
	xrtCallResultUnit(&Result);
	xrtValueRelease(pBonus);
	xrtValueRelease(pArgument);
	xrtClearError();
	printf("[PASS] runtime call\n");
	return 0;
}
