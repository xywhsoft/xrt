#include <stdio.h>

#include <xrt/http_client.h>



/* 在客户端完成回调中读取第一条 Basic challenge。 */
static bool exampleHttpBasicChallenge(
	const xhttpresponse* pResponse
)
{
	xhttpauthcursor Cursor;
	xhttpbasicchallenge Challenge;
	char Output[256];
	size_t iSize;

	xrtHttpAuthCursorInit(&Cursor);
	if ( xrtHttpResponseBasicChallengeNext(
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
		"Basic realm=%.*s utf8=%s\n",
		(int)Challenge.Realm.Size,
		Challenge.Realm.Data,
		Challenge.Utf8 ? "yes" : "no"
	);
	return true;
}



/* Response 由真实 HTTP Client 回调提供。 */
int main(void)
{
	(void)exampleHttpBasicChallenge;
	return 0;
}
