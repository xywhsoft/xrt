#include "../test.h"

#include "../../src/internal/xacme_http.h"

#include <stdlib.h>
#include <string.h>

static void testRequestArguments(xacmehttp* pHttp)
{
	xacmehttpresponse Response;
	xacmehttpheader Extra = { "X-Probe", "ok" };
	static const char* const Owned[] = {
		"hOsT", "User-Agent", "Accept", "Connection", "Content-Type",
		"cOnTeNt-LeNgTh", "TRANSFER-ENCODING"
	};
	size_t i;
	for(i = 0u; i < 13u; i++)
	{
		const xacmehttpheader* pHeaders = &Extra;
		size_t iCount = 1u;
		xstrview Body = { NULL, 0u };
		Extra = (xacmehttpheader){ "X-Probe", "ok" };
		if(i == 0u) pHeaders = NULL;
		else if(i == 1u) iCount = 95u; /* 4 defaults + Content-Type + Length + 95 > 100. */
		else if(i == 2u) iCount = SIZE_MAX;
		else if(i == 3u) Extra.sName = NULL;
		else if(i == 4u) Extra.sValue = NULL;
		else if(i == 5u) Body.Size = 1u;
		else Extra.sName = Owned[i - 6u];
		memset(&Response, 0xA5, sizeof(Response));
		xrtClearError();
		testRequire(!xacmeHttpExchangeV(pHttp, "POST", "http://127.0.0.1:1/token",
			"text/plain", Body, pHeaders, iCount, &Response) &&
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
			xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_ARGUMENT,
			"acme invalid request arguments reached transport");
		testRequire(Response.iStatus == 0u && Response.iBodySize == 0u &&
			Response.sBody == NULL && Response.sLocation == NULL &&
			Response.sReplayNonce == NULL && Response.sRetryAfter == NULL &&
			Response.sLink == NULL && Response.sContentType == NULL,
			"acme invalid request retained response");
	}
	xrtClearError();
}

static void testUrlArguments(xacmehttp* pHttp)
{
	static const char* const Invalid[] = {
		"http://127.0.0.1:1junk/token", "http://127.0.0.1:1@other.invalid/token",
		"http://127.0.0.1:+1/token", "http://127.0.0.1:65536/token",
		"http://127.0.0.1:999999999999999999999/token", "http://127.1/token",
		"http://2130706433/token", "http://0x7f000001/token", "http://127.0.0.01/token",
		"http://[]/token", "http://[1::2::3]/token", "http://[::1]:1junk/token",
		"http://[fe80::1%25lo]/token", "http://127.0.0.1:1/bad%GG",
		"http://127.0.0.1:1/bad\\path", "http://127.0.0.1:1/bad\r\nInjected:x"
	};
	size_t i;
	for(i = 0u; i < sizeof(Invalid) / sizeof(Invalid[0]); i++)
	{
		xacmehttpresponse Response;
		memset(&Response, 0xA5, sizeof(Response));
		xrtClearError();
		testRequire(!xacmeHttpExchangeOnceV(pHttp, "GET", Invalid[i], NULL,
			(xstrview){ NULL, 0u }, NULL, 0u, &Response) &&
			xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
			xrtErrorCode(xrtGetError()) == XACME_HTTP_ERROR_URL,
			"acme malformed URL reached transport");
		testRequire(Response.iStatus == 0u && Response.iBodySize == 0u &&
			Response.sBody == NULL && Response.sLocation == NULL &&
			Response.sReplayNonce == NULL && Response.sRetryAfter == NULL &&
			Response.sLink == NULL && Response.sContentType == NULL,
			"acme malformed URL retained response");
	}
	xrtClearError();
}

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
	testRequestArguments(&Http);
	testUrlArguments(&Http);
	if(getenv("XACME_TEST_LOCAL_TIMEOUT") != NULL)
	{
		Http.uTimeoutMs = UINT64_C(1000);
	}
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

	/* 本地故障服务器门控：响应头后截断或停顿必须释放部分响应。 */
	{
		const char* sFault = getenv("XACME_TEST_FAULT_URL");
		if((sFault != NULL) && (sFault[0] != '\0'))
		{
			const char* sKind = getenv("XACME_TEST_FAULT_KIND");
			bool bStalled = (sKind != NULL &&
				strcmp(sKind, "timeout") == 0);
			xerrkind Actual;
			Http.uTimeoutMs = bStalled ?
				UINT64_C(1000000) : UINT64_C(5000000);
			xrtClearError();
			testRequire(!xacmeHttpExchange(
				&Http, "GET", sFault, NULL,
				(xstrview){ NULL, 0u }, &Response),
				"acme http fault exchange unexpectedly succeeded");
			Actual = xrtErrorKind(xrtGetError());
			testRequire(bStalled ? (Actual == XERR_TIMEOUT) :
				((Actual == XERR_PROTOCOL) ||
				 (Actual == XERR_TIMEOUT) ||
				 (Actual == XERR_IO)),
				"acme http fault error kind mismatch");
			testRequire(Response.iStatus == 0u &&
				Response.sLocation == NULL &&
				Response.sReplayNonce == NULL &&
				Response.sRetryAfter == NULL &&
				Response.sLink == NULL &&
				Response.sContentType == NULL &&
				Response.sBody == NULL &&
				Response.iBodySize == 0u,
				"acme http fault retained partial response");
			xrtClearError();
			printf("[PASS] acme http partial-response cleanup (%s)\n",
				bStalled ? "timeout" : "truncated");
		}
	}

	/* 服务端收下 POST 后断线：传输层不得自动重放写请求。 */
	{
		const char* sDrop = getenv("XACME_TEST_POST_DROP_URL");
		if((sDrop != NULL) && (sDrop[0] != '\0'))
		{
			bool bOnce = getenv("XACME_TEST_POST_DROP_ONCE") != NULL;
			bool bOk;
			Http.uTimeoutMs = UINT64_C(1000);
			xrtClearError();
			bOk = bOnce ? xacmeHttpExchangeOnceV(
				&Http, "POST", sDrop, "text/plain",
				XRT_STR_LITERAL("data"), NULL, 0u, &Response) :
				xacmeHttpExchange(&Http, "POST", sDrop,
					"text/plain", XRT_STR_LITERAL("data"), &Response);
			testRequire(!bOk,
				"acme http dropped POST unexpectedly succeeded");
			testRequire(xrtErrorKind(xrtGetError()) == XERR_IO ||
				xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
				"acme http dropped POST error kind mismatch");
			testRequire(xrtErrorCode(xrtGetError()) ==
				XACME_HTTP_ERROR_UNCERTAIN,
				"acme http dropped POST outcome was not marked uncertain");
			testRequire(Response.iStatus == 0u &&
				Response.sLocation == NULL && Response.sBody == NULL,
				"acme http dropped POST retained response");
			xrtClearError();
			printf("[PASS] acme http dropped POST was not replayed\n");
		}
	}

	/* 慢读对端不得把每个发送分块的等待时间累加成无限总时长。 */
	{
		const char* sSlow = getenv("XACME_TEST_SLOW_SEND_URL");
		if((sSlow != NULL) && (sSlow[0] != '\0'))
		{
			size_t iSize = 8u * 1024u * 1024u;
			char* sBody = (char*)xrtMalloc(iSize);
			testRequire(sBody != NULL, "acme slow-send body allocated");
			memset(sBody, 'x', iSize);
			Http.uTimeoutMs = UINT64_C(1000);
			xrtClearError();
			testRequire(!xacmeHttpExchangeOnceV(
				&Http, "POST", sSlow, "text/plain",
				(xstrview){ sBody, iSize }, NULL, 0u, &Response),
				"acme slow-send POST unexpectedly succeeded");
			testRequire(xrtErrorKind(xrtGetError()) == XERR_IO ||
				xrtErrorKind(xrtGetError()) == XERR_TIMEOUT,
				"acme slow-send error kind mismatch");
			testRequire(xrtErrorCode(xrtGetError()) ==
				XACME_HTTP_ERROR_UNCERTAIN,
				"acme slow-send outcome was not marked uncertain");
			testRequire(xrtErrorCause(xrtGetError()) != NULL &&
				xrtErrorCode(xrtErrorCause(xrtGetError())) ==
				XACME_HTTP_ERROR_SEND,
				"acme slow-send did not fail in the send phase");
			testRequire(Response.iStatus == 0u &&
				Response.sLocation == NULL && Response.sBody == NULL,
				"acme slow-send retained response");
			xrtFree(sBody);
			xrtClearError();
			printf("[PASS] acme http total send deadline\n");
		}
	}

	xacmeHttpUnit(&Http);
	printf("[PASS] acme http transport\n");
	return 0;
}
