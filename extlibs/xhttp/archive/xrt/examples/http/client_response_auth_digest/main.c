#include <stdio.h>

#include <xrt/http_client.h>



/* 按本地安全策略选择服务端提供的最佳 Digest challenge。 */
static bool exampleHttpDigestChallenge(
	const xhttpresponse* pResponse,
	xhttpdigestchallenge* pChallenge,
	xhttpdigestchoice* pChoice
)
{
	xhttpdigestpolicy Policy;
	char Output[512];
	size_t iSize;

	xrtHttpDigestPolicyInit(&Policy);
	if ( xrtHttpResponseDigestChallengeChoose(
		pResponse,
		&Policy,
		Output,
		sizeof(Output),
		&iSize,
		pChallenge,
		pChoice
	) != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf(
		"Digest realm=%.*s algorithm=%d qop=%d\n",
		(int)pChallenge->Realm.Size,
		pChallenge->Realm.Data,
		(int)pChoice->Algorithm,
		(int)pChoice->Qop
	);
	return true;
}



/* Response 由真实 HTTP Client 回调提供。 */
int main(void)
{
	(void)exampleHttpDigestChallenge;
	return 0;
}
