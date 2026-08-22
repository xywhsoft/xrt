#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头发布必须保留有界重放表的单调 nonce-count 契约。 */
int main(void)
{
	xhttpdigestreplayconfig Config = { 1u, 2u, 60 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaycheck First;
	xhttpdigestreplaycheck Second;

	if ( pReplay == NULL ) {
		return 1;
	}
	First = xrtHttpDigestReplayCheck(
		pReplay,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		1u, 1000, 1000
	);
	Second = xrtHttpDigestReplayCheck(
		pReplay,
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		1u, 1000, 1000
	);
	xrtHttpDigestReplayDestroy(pReplay);
	return (First == XHTTP_DIGEST_REPLAY_ACCEPTED) &&
		(Second == XHTTP_DIGEST_REPLAY_REPLAY) ? 0 : 2;
}
