#include "../test.h"



/* 验证错误格式化的短消息、动态长度和无副作用契约。 */
int main(void)
{
	char sLongMessage[700];
	int iWritten = 0;

	xrtSetErrorFormat(
		XERR_IO,
		"test.format",
		9,
		"read %s at %d",
		"failed",
		12
	);
	testRequire((xrtErrorKind(xrtGetError()) == XERR_IO) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "test.format") == 0) &&
		(xrtErrorCode(xrtGetError()) == 9) &&
		(strcmp(xrtErrorMessage(xrtGetError()), "read failed at 12") == 0),
		"formatted error setting mismatch");

	memset(sLongMessage, 'x', sizeof(sLongMessage) - 1u);
	sLongMessage[sizeof(sLongMessage) - 1u] = '\0';
	xrtSetErrorFormat(XERR_RANGE, "test.format", 10, "%s", sLongMessage);
	testRequire((xrtErrorKind(xrtGetError()) == XERR_RANGE) &&
		(strlen(xrtErrorMessage(xrtGetError())) ==
			sizeof(sLongMessage) - 1u) &&
		(strcmp(xrtErrorMessage(xrtGetError()), sLongMessage) == 0),
		"long formatted error setting mismatch");

	xrtSetErrorFormat(XERR_IO, "test.format", 11, "%n", &iWritten);
	testRequire(iWritten == 0, "unsafe format modified caller memory");
	testRequire((xrtErrorKind(xrtGetError()) == XERR_VALUE) &&
		(strcmp(xrtErrorDomain(xrtGetError()), "xrt.error") == 0),
		"unsafe format error mismatch");

	xrtSetErrorFormat(XERR_IO, "test.format", 12, NULL);
	testRequire(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"null format error mismatch");
	xrtClearError();
	printf("[PASS] error-format\n");
	return 0;
}
