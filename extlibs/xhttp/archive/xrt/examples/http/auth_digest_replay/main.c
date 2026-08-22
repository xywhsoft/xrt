#include <xrt/http_auth.h>

#include <stdio.h>



/* 展示服务端在证明验证成功后原子推进 nonce-count。 */
int main(void)
{
	xhttpdigestreplayconfig Config;
	xhttpdigestreplay* pReplay;
	xhttpdigestreplaycheck Check;

	xrtHttpDigestReplayConfigInit(&Config);
	Config.Shards = 4u;
	Config.EntriesPerShard = 256u;
	Config.LifetimeSeconds = 300;
	pReplay = xrtHttpDigestReplayCreate(&Config);
	if ( pReplay == NULL ) {
		return 1;
	}
	Check = xrtHttpDigestReplayCheck(
		pReplay,
		XRT_STR_LITERAL("Mufasa"),
		XRT_STR_LITERAL("server-nonce"),
		XRT_STR_LITERAL("client-nonce"),
		1u,
		INT64_C(1700000000),
		INT64_C(1700000001)
	);
	printf("accepted=%s\n",
		Check == XHTTP_DIGEST_REPLAY_ACCEPTED ? "yes" : "no");
	xrtHttpDigestReplayDestroy(pReplay);
	return Check == XHTTP_DIGEST_REPLAY_ACCEPTED ? 0 : 2;
}
