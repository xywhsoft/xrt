#define XHTTP_SERVER_IMPLEMENTATION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-server/features.h"
#include <xhttp/features.h>
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-server.h"




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
