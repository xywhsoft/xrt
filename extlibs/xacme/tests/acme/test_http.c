#include "../test.h"

#include "../../src/internal/xacme_http.h"

#include <stdlib.h>
#include <string.h>

int main(void)
{
	xacmehttp Http;
	xacmehttpresponse Response;

	/* 生命周期：自建引擎 + 系统信任库。 */
	testRequire(xacmeHttpInit(&Http, NULL, NULL, 0u), "acme http init failed");
	xacmeHttpUnit(&Http);

	testRequire(
		!xacmeHttpInit(NULL, NULL, NULL, 0u) &&
			(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme http init null mismatch"
	);

	/* URL 与参数语义。 */
	testRequire(xacmeHttpInit(&Http, NULL, NULL, 0u), "acme http init 2");
	xrtClearError();
	testRequire(
		!xacmeHttpExchange(
			&Http, "GET", "ftp://example.com/", NULL,
			(xstrview){ NULL, 0u }, &Response)
			&& (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
			(xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_URL),
		"acme http url scheme mismatch"
	);
	xrtClearError();
	testRequire(
		!xacmeHttpExchange(
			&Http, "GET", "https:///", NULL,
			(xstrview){ NULL, 0u }, &Response)
			&& (xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_URL),
		"acme http url empty host mismatch"
	);
	xrtClearError();
	testRequire(
		!xacmeHttpExchange(
			NULL, "GET", "https://example.com/", NULL,
			(xstrview){ NULL, 0u }, &Response)
			&& (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"acme http null http mismatch"
	);

	/* 真网门控：设 XACME_TEST_URL 时做一次 GET（如 https://www.baidu.com/）。 */
	{
		const char* sLive = getenv("XACME_TEST_URL");
		if((sLive != NULL) && (sLive[0] != '\0'))
		{
			memset(&Response, 0, sizeof(Response));
			if(!xacmeHttpExchange(
				&Http, "GET", sLive, NULL,
				(xstrview){ NULL, 0u }, &Response))
			{
				const xerror* pError = xrtGetError();
				const xerror* pCause = pError;
				int i;
				printf(
					"[diag] live GET failed kind=%d code=%d\n",
					xrtErrorKind(pError), xrtErrorCode(pError));
				for(i = 0; (i < 5) && (pCause != NULL); i++)
				{
					printf(
						"[diag]   cause kind=%d code=%d dom=%s msg=%s\n",
						xrtErrorKind(pCause), xrtErrorCode(pCause),
						xrtErrorDomain(pCause),
						xrtErrorMessage(pCause) ?
							xrtErrorMessage(pCause) : "?");
					pCause = xrtErrorCause(pCause);
				}
				testRequire(false, "acme http live GET failed");
			}
			testRequire(
				Response.iStatus == 200u,
				"acme http live status mismatch"
			);
			testRequire(
				(Response.sBody != NULL) && (Response.iBodySize > 0u),
				"acme http live body empty"
			);
			xacmeHttpResponseUnit(&Response);
			printf("[PASS] acme http live exchange (%s)\n", sLive);
		}
		else
		{
			printf("[SKIP] acme http live exchange (XACME_TEST_URL unset)\n");
		}
	}

	xacmeHttpUnit(&Http);
	printf("[PASS] acme http transport\n");
	return 0;
}
