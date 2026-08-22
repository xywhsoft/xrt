#include <stdio.h>

#include <xrt.h>



/* 创建原因链，并通过当前错误槽交给上层处理。 */
int main(void)
{
	xerror* pCause = xrtErrorCreate(XERR_TIMEOUT, "example.net", 1, "connect timeout");
	xerror* pError;

	if ( pCause == NULL ) {
		return 1;
	}
	pError = xrtErrorWrap(pCause, XERR_IO, "example.client", 2, "request failed");
	xrtErrorFree(pCause);
	if ( pError == NULL ) {
		return 2;
	}
	xrtSetError(pError);
	xrtErrorFree(pError);

	printf("error: %s\n", xrtErrorMessage(xrtGetError()));
	printf("timeout cause: %s\n",
		xrtErrorIs(xrtGetError(), XERR_TIMEOUT) != NULL ? "yes" : "no");
	xrtClearError();
	return 0;
}
