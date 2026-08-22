#include "../test.h"



/* 验证默认程序入口拒绝空目标和非法 UTF-8，并保留稳定错误。 */
int main(void)
{
	static const char sInvalid[] = { (char)0xC0, (char)0xAF, 0 };
	const xerror* pError;

	testRequire(
		!xrtProcessOpen(NULL),
		"process open accepted a null target"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(strcmp(xrtErrorDomain(pError), "xrt.process") == 0) &&
		(xrtErrorCode(pError) == XPROCESS_ERROR_OPEN) &&
		(xrtErrorKind(pError) == XERR_ARGUMENT),
		"process open null error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtProcessOpen(""),
		"process open accepted an empty target"
	);
	testRequire(
		xrtErrorCode(xrtGetError()) == XPROCESS_ERROR_OPEN,
		"process open empty error mismatch"
	);

	xrtClearError();
	testRequire(
		!xrtProcessOpen(sInvalid),
		"process open accepted invalid UTF-8"
	);
	pError = xrtGetError();
	testRequire(
		(pError != NULL) &&
		(xrtErrorCode(pError) == XPROCESS_ERROR_OPEN) &&
		(xrtErrorKind(pError) == XERR_VALUE),
		"process open UTF-8 error mismatch"
	);
	return 0;
}
