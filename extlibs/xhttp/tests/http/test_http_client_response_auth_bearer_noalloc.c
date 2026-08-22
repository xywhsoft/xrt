#include "http_client_response_auth_noalloc.h"



/* 响应 Bearer challenge 的过滤、查询和解码不得动态分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Basic realm=\"skip\", Bearer realm=\"api\", "
				"error=\"invalid_token\""
			)
		}
	};
	test_http_response_auth_allocator Allocator;
	xhttpresponse* pResponse;
	xhttpauthcursor Cursor;
	xhttpbearerchallenge Challenge;
	char Output[32];
	size_t iSize;

	testHttpResponseAuthAllocatorInstall(&Allocator);
	pResponse = testHttpResponseFixtureCreate(Fields, 1u);
	Allocator.Reject = true;
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse, &Cursor, NULL, 0, &iSize, &Challenge
		) == XHTTP_NEXT_ITEM) &&
		(Allocator.Denied == 0u),
		"HTTP response Bearer challenge query allocated"
	);
	testRequire(
		(xrtHttpResponseBearerChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(Allocator.Denied == 0u),
		"HTTP response Bearer challenge decode allocated"
	);
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Bearer authentication no allocation");
	return 0;
}
