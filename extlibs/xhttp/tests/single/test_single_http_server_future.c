#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



int main(void)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = xrtPromiseCreate(
		&pFuture,
		NULL
	);
	bool bPassed = (pPromise != NULL) &&
		(pFuture != NULL) &&
		!xrtHttpServerWaitAsync(NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		!xrtHttpConnRespondFuture(NULL, pFuture) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);

	xrtPromiseDestroy(pPromise);
	xrtFutureDestroy(pFuture);
	return bPassed ? 0 : 1;
}
