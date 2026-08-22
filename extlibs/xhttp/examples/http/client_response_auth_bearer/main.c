#include <stdio.h>

#include <xrt/http_client.h>



/* 在客户端完成回调中读取第一条 Bearer challenge。 */
static bool exampleHttpBearerChallenge(
	const xhttpresponse* pResponse
)
{
	xhttpauthcursor Cursor;
	xhttpbearerchallenge Challenge;
	char Output[512];
	size_t iSize;

	xrtHttpAuthCursorInit(&Cursor);
	if ( xrtHttpResponseBearerChallengeNext(
		pResponse,
		&Cursor,
		Output,
		sizeof(Output),
		&iSize,
		&Challenge
	) != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf(
		"Bearer realm=%.*s scope=%.*s\n",
		(int)Challenge.Realm.Size,
		Challenge.Realm.Data,
		(int)Challenge.Scope.Size,
		Challenge.Scope.Data
	);
	return true;
}



/* Response 由真实 HTTP Client 回调提供。 */
int main(void)
{
	(void)exampleHttpBearerChallenge;
	return 0;
}
