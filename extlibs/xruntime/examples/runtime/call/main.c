#include <stdio.h>
#include <xruntime.h>



/* 把两个整数参数相加并移交唯一的返回值引用。 */
static bool add(
	ptr pEnvironment,
	const xrtcallframe* pFrame,
	xrtcallresult* pResult
)
{
	int64 iLeft;
	int64 iRight;
	xvalue* pValue;

	(void)pEnvironment;
	if (
		!xrtValueGetInt(xrtCallFrameParameter(pFrame, 0u), &iLeft) ||
		!xrtValueGetInt(xrtCallFrameParameter(pFrame, 1u), &iRight)
	) {
		return false;
	}
	pValue = xrtValueInt(iLeft + iRight);
	if ( pValue == NULL ) {
		return false;
	}
	if ( !xrtCallResultPushTake(pResult, &pValue) ) {
		xrtValueRelease(pValue);
		return false;
	}
	return true;
}



int main(void)
{
	xrtparamdesc Params[2] = {
		{ XRT_STR_INIT("left"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u },
		{ XRT_STR_INIT("right"), xrtTypeInt64(), XRT_PARAM_DEFAULT, 0u }
	};
	const xrttype* ReturnType = xrtTypeInt64();
	xrtfunctionsig Signature = {
		.Name = XRT_STR_INIT("add"),
		.ParamCount = 2u,
		.Params = Params,
		.ReturnCount = 1u,
		.ReturnTypes = &ReturnType
	};
	xvalue* Arguments[2] = { xrtValueInt(7), xrtValueInt(5) };
	xrtcallframe Frame = {
		.ArgumentCount = 2u,
		.Arguments = Arguments
	};
	xrtcallresult Result = XRT_CALL_RESULT_INIT;
	xrtcallable* pCallable = xrtCallableCreate(
		&Signature, add, NULL, NULL);
	int64 iSum;
	int iResult = 0;

	if (
		(pCallable == NULL) ||
		!xrtCallableInvoke(pCallable, &Frame, &Result) ||
		!xrtValueGetInt(xrtCallResultGet(&Result, 0u), &iSum)
	) {
		iResult = 1;
	} else {
		printf("type=%.*s\n", (int)xrtTypeCallable()->Name.Size,
			xrtTypeCallable()->Name.Data);
		printf("7 + 5 = %lld\n", (long long)iSum);
	}
	xrtCallResultUnit(&Result);
	xrtCallableUnref(pCallable);
	xrtValueRelease(Arguments[1]);
	xrtValueRelease(Arguments[0]);
	return iResult;
}
