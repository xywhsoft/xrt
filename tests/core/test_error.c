#include "../test.h"

static void testErrorViews(void)
{
	char Input[] = { 'a', '\0', (char)0xe4, (char)0xbd, (char)0xa0 };
	const char Expected[] = { 'a', '\0', (char)0xe4, (char)0xbd, (char)0xa0 };
	xstrview Text = { Input, sizeof(Input) };
	xerrordescview Desc = { XERR_VALUE, 71, 72, Text, Text, Text, Text, NULL };
	xerrorlocationview Location = { Text, 3, 4 };
	xstrview (*Getters[])(const xerror*) = {
		xrtErrorDomainView, xrtErrorOperationView, xrtErrorMessageView,
		xrtErrorDataView, xrtErrorFileView
	};
	xerror* Cause = xrtErrorBuildViewAt(&Desc, &Location);
	xerror* Error;
	size_t i;
	testRequire(Cause != NULL, "exact error creation failed");
	memset(Input, 'z', sizeof(Input));
	for (i = 0; i < sizeof(Getters) / sizeof(Getters[0]); ++i) {
		xstrview Copy = Getters[i](Cause);
		testRequire(Copy.Size == sizeof(Expected) && Copy.Data != Input &&
			memcmp(Copy.Data, Expected, sizeof(Expected)) == 0 &&
			Copy.Data[Copy.Size] == '\0', "error view was borrowed, truncated or unterminated");
		testRequire(Getters[i](NULL).Size == 0, "NULL error view must be empty");
	}
	testRequire(strcmp(xrtErrorMessage(Cause), "a") == 0, "C-string interop changed");
	testRequire(xrtErrorFind(Cause, "a", 71) == NULL, "embedded domain suffix was ignored");
	testRequire(xrtErrorCode(Cause) == 71 && xrtErrorSystemCode(Cause) == 72 &&
		xrtErrorLine(Cause) == 3 && xrtErrorColumn(Cause) == 4, "exact error metadata changed");
	Desc.Cause = Cause;
	Error = xrtErrorBuildView(&Desc);
	testRequire(Error != NULL && xrtErrorCause(Error) == Cause, "exact cause ownership failed");
	xrtErrorFree(Cause);
	testRequire(xrtErrorMessageView(xrtErrorCause(Error)).Size == sizeof(Expected) &&
		memcmp(xrtErrorMessageView(xrtErrorCause(Error)).Data, Expected, sizeof(Expected)) == 0,
		"cause text did not survive releasing its source owner");
	xrtErrorFree(Error);
	Desc.Cause = NULL;
	{
		xstrview* Fields[] = { &Desc.Domain, &Desc.Operation, &Desc.Message, &Desc.Data, &Location.File };
		for (i = 0; i < sizeof(Fields) / sizeof(Fields[0]); ++i) {
			xstrview Saved = *Fields[i];
			*Fields[i] = (xstrview){ NULL, 1 };
			testRequire(xrtErrorBuildViewAt(&Desc, &Location) == NULL &&
				xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "NULL/nonzero error span accepted");
			xrtClearError();
			*Fields[i] = (xstrview){ Input, SIZE_MAX };
			testRequire(xrtErrorBuildViewAt(&Desc, &Location) == NULL &&
				xrtErrorKind(xrtGetError()) == XERR_RANGE, "error size overflow accepted");
			xrtClearError(); *Fields[i] = Saved;
		}
	}
	Desc.Domain = (xstrview){ Input, SIZE_MAX / 2 };
	Desc.Operation = Desc.Domain;
	testRequire(xrtErrorBuildView(&Desc) == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_RANGE, "combined error spans overflowed");
	xrtClearError();
	Desc = (xerrordescview){ 0 };
	Desc.Kind = XERR_STATE;
	Error = xrtErrorBuildView(&Desc);
	testRequire(Error != NULL && xrtErrorMessageView(Error).Size == 0 &&
		xrtErrorMessage(Error)[0] == '\0', "NULL/zero span must be an owned empty text");
	xrtErrorFree(Error);
	Location.Line = -1;
	testRequire(xrtErrorBuildViewAt(&Desc, &Location) == NULL &&
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "exact negative location accepted");
	xrtClearError();
}



#define TEST_ERROR_CHAIN_DEPTH 32768



/* 错误处理器测试状态。 */
typedef struct test_error_handler_context {
	int Count;
	bool ReplaceError;
} test_error_handler_context;



/* 验证错误处理器看到主错误，且内部错误不会递归通知。 */
static void testErrorHandler(const xerror* pError, ptr pUserData)
{
	test_error_handler_context* pContext =
		(test_error_handler_context*)pUserData;
	xerror* pNestedError;

	testRequire(pError == xrtGetError(), "handler must observe the current error");
	pContext->Count++;
	if ( !pContext->ReplaceError ) {
		return;
	}

	pNestedError = xrtErrorCreate(
		XERR_INTERNAL,
		"test.handler",
		1,
		"handler failure"
	);
	testRequire(pNestedError != NULL, "handler nested error creation failed");
	xrtSetError(pNestedError);
	xrtErrorFree(pNestedError);
}



/* 验证错误字段、原因链、引用和当前上下文所有权。 */
int main(void)
{
	static const xerrkind Kinds[] = {
		XERR_ARGUMENT, XERR_TYPE, XERR_VALUE, XERR_RANGE,
		XERR_STATE, XERR_MEMORY, XERR_IO, XERR_NOT_FOUND,
		XERR_EXISTS, XERR_PERMISSION, XERR_AGAIN, XERR_TIMEOUT,
		XERR_CANCELLED, XERR_CLOSED, XERR_PROTOCOL,
		XERR_UNSUPPORTED, XERR_INTERNAL
	};
	char sDomain[] = "test.io";
	char sOperation[] = "read";
	char sMessage[] = "read failed";
	char sData[] = "offset=12";
	char sFile[] = "sample.xl";
	xerrordesc tDesc;
	xerrorlocation tLocation;
	xerror* pCause;
	xerror* pError;
	xerror* pTaken;
	xerror* pChain;
	test_error_handler_context tHandler;

	memset(&tHandler, 0, sizeof(tHandler));
	testRequire(xrtGetError() == NULL, "new thread must not have an error");
	testErrorViews();
	pCause = xrtErrorCreate(XERR_TIMEOUT, "test.net", 7, "deadline reached");
	testRequire(pCause != NULL, "cause creation failed");

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = XERR_IO;
	tDesc.Code = 42;
	tDesc.SystemCode = 5;
	tDesc.Domain = sDomain;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	tDesc.Data = sData;
	tDesc.Cause = pCause;
	memset(&tLocation, 0, sizeof(tLocation));
	tLocation.File = sFile;
	tLocation.Line = 12;
	tLocation.Column = 7;
	pError = xrtErrorBuildAt(&tDesc, &tLocation);
	testRequire(pError != NULL, "error creation failed");

	memset(sDomain, 'x', sizeof(sDomain) - 1);
	memset(sOperation, 'x', sizeof(sOperation) - 1);
	memset(sMessage, 'x', sizeof(sMessage) - 1);
	memset(sData, 'x', sizeof(sData) - 1);
	memset(sFile, 'x', sizeof(sFile) - 1);
	testRequire(strcmp(xrtErrorDomain(pError), "test.io") == 0, "domain was not copied");
	testRequire(strcmp(xrtErrorOperation(pError), "read") == 0, "operation was not copied");
	testRequire(strcmp(xrtErrorMessage(pError), "read failed") == 0, "message was not copied");
	testRequire(strcmp(xrtErrorData(pError), "offset=12") == 0, "data was not copied");
	testRequire(strcmp(xrtErrorFile(pError), "sample.xl") == 0, "source file was not copied");
	testRequire(xrtErrorLine(pError) == 12, "source line mismatch");
	testRequire(xrtErrorColumn(pError) == 7, "source column mismatch");
	testRequire(xrtErrorKind(pError) == XERR_IO, "kind mismatch");
	testRequire(xrtErrorCode(pError) == 42, "code mismatch");
	testRequire(xrtErrorSystemCode(pError) == 5, "system code mismatch");
	testRequire(xrtErrorCause(pError) == pCause, "cause mismatch");
	testRequire(xrtErrorIs(pError, XERR_IO) == pError, "top-level kind lookup mismatch");
	testRequire(xrtErrorIs(pError, XERR_TIMEOUT) == pCause, "cause kind lookup mismatch");
	testRequire(xrtErrorIs(pError, XERR_AGAIN) == NULL, "missing kind lookup mismatch");
	testRequire(xrtErrorFind(pError, "test.io", 42) == pError, "top-level domain lookup mismatch");
	testRequire(xrtErrorFind(pError, "test.net", 7) == pCause, "cause domain lookup mismatch");
	testRequire(xrtErrorFind(pError, NULL, 7) == NULL, "null domain lookup mismatch");
	xrtSetErrorInfo(XERR_VALUE, "test.info", 8, "bad value");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.info") == 0) &&
		(xrtErrorCode(xrtGetError()) == 8) &&
		(strcmp(xrtErrorMessage(xrtGetError()), "bad value") == 0),
		"direct error setting mismatch");
	xrtClearError();
	for ( size_t i = 0; i < (sizeof(Kinds) / sizeof(Kinds[0])); i++ ) {
		xrtSetErrorKind(Kinds[i]);
		testRequire(
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == Kinds[i]),
			"common error kind mismatch"
		);
	}
	xrtSetErrorKind(XERR_NONE);
	testRequire(xrtGetError() == NULL, "NONE did not clear common error");
	xrtSetErrorKind((xerrkind)INT32_MAX);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"invalid common error kind mismatch"
	);
	xrtClearError();
	pTaken = xrtErrorCreate(XERR_STATE, "test.take", 9, "owned error");
	testRequire(pTaken != NULL, "owned error creation failed");
	xrtSetErrorTake(pTaken);
	testRequire(xrtGetError() == pTaken, "owned error transfer changed identity");
	pTaken = xrtTakeError();
	testRequire(pTaken != NULL, "owned error transfer lost ownership");
	xrtErrorFree(pTaken);

	/* 原因链释放必须迭代执行，不能把用户可控深度转化为 C 栈深度。 */
	pChain = xrtErrorCreate(
		XERR_IO,
		"test.chain.root",
		1,
		"root"
	);
	testRequire(pChain != NULL, "error chain root creation failed");
	for ( int i = 0; i < TEST_ERROR_CHAIN_DEPTH; i++ ) {
		xerror* pNext = xrtErrorWrap(
			pChain,
			XERR_IO,
			"test.chain",
			i + 2,
			"wrapped"
		);

		testRequire(pNext != NULL, "error chain creation failed");
		xrtErrorFree(pChain);
		pChain = pNext;
	}
	testRequire(
		xrtErrorFind(pChain, "test.chain.root", 1) != NULL,
		"deep cause lookup failed"
	);
	xrtErrorFree(pChain);

	xrtErrorFree(pCause);
	tHandler.ReplaceError = true;
	xrtSetErrorHandler(testErrorHandler, &tHandler);
	xrtSetError(pError);
	xrtErrorFree(pError);
	testRequire(tHandler.Count == 1, "error handler must not notify recursively");
	testRequire(strcmp(xrtErrorMessage(xrtGetError()), "read failed") == 0, "current error lifetime mismatch");
	testRequire(
		xrtErrorFind(xrtGetError(), "test.io", 42) != NULL,
		"handler failure replaced the observed error"
	);

	pTaken = xrtTakeError();
	testRequire((pTaken != NULL) && (xrtGetError() == NULL), "take error ownership mismatch");
	xrtErrorFree(pTaken);
	xrtSetErrorHandler(NULL, NULL);
	xrtClearError();
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = XERR_NONE;
	testRequire(xrtErrorBuild(&tDesc) == NULL, "non-error kind must be rejected");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "invalid kind error mismatch");
	xrtClearError();
	tDesc.Kind = XERR_IO;
	tLocation.Line = -1;
	testRequire(xrtErrorBuildAt(&tDesc, &tLocation) == NULL, "negative source location must be rejected");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT, "invalid source location error mismatch");
	xrtClearError();
	printf("[PASS] error\n");
	return 0;
}
