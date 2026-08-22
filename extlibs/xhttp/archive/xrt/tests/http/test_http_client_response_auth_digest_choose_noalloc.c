#include "http_client_response_fixture.h"



/* 成功选择并解码响应 Digest challenge 不得分配堆内存。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("WWW-Authenticate"),
			XRT_STR_INIT(
				"Digest realm=\"api\", nonce=\"n\", "
				"algorithm=SHA-256, qop=\"auth\""
			)
		}
	};
	xhttpresponse* pResponse = testHttpResponseFixtureCreate(Fields, 1u);
	xhttpdigestchallenge Challenge;
	xhttpdigestchoice Choice;
	char Output[8];
	size_t iSize;

	testRequire(xrtMemDebugFailAfter(0),
		"HTTP response Digest choice failure injection failed");
	testRequire((xrtHttpResponseDigestChallengeChoose(
		pResponse,
		NULL,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge,
		&Choice
	) == XHTTP_NEXT_ITEM) &&
		(Choice.Algorithm == XHTTP_DIGEST_ALGORITHM_SHA256),
		"HTTP response Digest choice allocated");
	xrtHttpResponseDestroy(pResponse);
	puts("[PASS] HTTP client response Digest choice no allocation");
	return 0;
}
