#include <stdio.h>

#include <xrt/http_client.h>



/* 验证服务端 Digest 回执，并把 nextnonce 原子更新到会话。 */
static bool exampleHttpDigestAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange
)
{
	xhttpdigestsessioncheck Check;
	xhttpnext Next = xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		(xstrview){ NULL, 0 },
		XRT_STR_LITERAL("next-client-nonce"),
		&Check
	);

	if ( Next != XHTTP_NEXT_ITEM ) {
		return false;
	}
	printf("Digest receipt=%d\n", (int)Check);
	return (Check == XHTTP_DIGEST_SESSION_VALID) ||
		(Check == XHTTP_DIGEST_SESSION_UPDATED);
}



/* Response、Session 与 Exchange 由同一次真实调用提供。 */
int main(void)
{
	(void)exampleHttpDigestAccept;
	return 0;
}
