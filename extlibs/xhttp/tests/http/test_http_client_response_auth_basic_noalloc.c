#include "http_client_response_auth_noalloc.h"



/* 响应 Basic challenge 的过滤、查询和解码不得动态分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"skip\", "
				"Basic realm=\"api\", charset=UTF-8"
			)
		}
	};
	test_http_response_auth_allocator Allocator;
	xhttpresponse* pResponse;
	xhttpauthcursor Cursor;
	xhttpbasicchallenge Challenge;
	char Output[8];
	size_t iSize;

	testHttpResponseAuthAllocatorInstall(&Allocator);
	pResponse = testHttpResponseFixtureCreate(Fields, 1u);
	Allocator.Reject = true;
	xrtHttpAuthCursorInit(&Cursor);
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse, &Cursor, NULL, 0, &iSize, &Challenge
		) == XHTTP_NEXT_ITEM) &&
		(iSize == 3u) && (Allocator.Denied == 0u),
		"HTTP response Basic challenge query allocated"
	);
	testRequire(
		(xrtHttpResponseBasicChallengeNext(
			pResponse,
			&Cursor,
			Output,
			sizeof(Output),
			&iSize,
			&Challenge
		) == XHTTP_NEXT_ITEM) &&
		(Allocator.Denied == 0u),
		"HTTP response Basic challenge decode allocated"
	);
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Basic authentication no allocation");
	return 0;
}
