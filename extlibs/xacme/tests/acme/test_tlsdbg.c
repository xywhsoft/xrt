#include "../test.h"

#include "../../src/internal/xacme_http.h"

#include <stdlib.h>
#include <string.h>

/* 环境门控 TLS 拨号诊断：XACME_TLS_HOST / XACME_TLS_CA 指定时输出因链。 */
int main(void)
{
	const char* sHost = getenv("XACME_TLS_HOST");
	const char* sCaPath = getenv("XACME_TLS_CA");
	xacmehttp Http;
	xacmehttpresponse R;

	if((sHost == NULL) || (sHost[0] == '\0'))
	{
		printf("[SKIP] acme tlsdbg (XACME_TLS_HOST unset)\n");
		return 0;
	}
	if((sCaPath != NULL) && (sCaPath[0] != '\0'))
	{
		FILE* f = fopen(sCaPath, "rb");
		str sCa;
		long iSize;
		testRequire(f != NULL, "acme tlsdbg ca open failed");
		fseek(f, 0, SEEK_END);
		iSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		sCa = (str)xrtMalloc((size_t)iSize + 1u);
		testRequire(
			(sCa != NULL) &&
				(fread(sCa, 1u, (size_t)iSize, f) == (size_t)iSize),
			"acme tlsdbg ca read failed");
		fclose(f);
		sCa[iSize] = '\0';
		testRequire(
			xacmeHttpInit(&Http, NULL, sCa, 0u),
			"acme tlsdbg http init failed");
		xrtFree(sCa);
	}
	else
	{
		testRequire(
			xacmeHttpInit(&Http, NULL, NULL, 0u),
			"acme tlsdbg http init failed");
	}
	memset(&R, 0, sizeof(R));
	if(!xacmeHttpExchange(
		&Http, "GET", sHost, NULL, (xstrview){ NULL, 0u }, &R))
	{
		const xerror* pError = xrtGetError();
		const xerror* pCause = pError;
		int i;
		printf("kind=%d code=%d\n",
			xrtErrorKind(pError), xrtErrorCode(pError));
		for(i = 0; (i < 8) && (pCause != NULL); i++)
		{
			printf("  L%d kind=%d code=%d dom=%s msg=%s\n",
				i, xrtErrorKind(pCause), xrtErrorCode(pCause),
				xrtErrorDomain(pCause),
				xrtErrorMessage(pCause) ? xrtErrorMessage(pCause) : "?");
			pCause = xrtErrorCause(pCause);
		}
		testRequire(false, "acme tlsdbg exchange failed");
	}
	printf("OK status=%u\n", R.iStatus);
	xacmeHttpResponseUnit(&R);
	xacmeHttpUnit(&Http);
	return 0;
}
