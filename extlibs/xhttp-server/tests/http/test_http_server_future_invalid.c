#include "../test.h"



int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(
		&pFuture,
		NULL
	);

	testRequire(
		(pPromise != NULL) &&
		(pFuture != NULL),
		"HTTP server Future invalid pair creation failed"
	);
	testRequire(
		!xrtHttpConnRespondFuture(NULL, pFuture) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server Future accepted a null connection"
	);
	xrtClearError();
	testRequire(
		!xrtHttpConnRespondFuture(NULL, NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"HTTP server Future accepted null arguments"
	);
	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	printf("[PASS] HTTP server Future invalid boundaries\n");
	return 0;
}
