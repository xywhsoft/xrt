#include "../test.h"

#include <xrt/http_auth.h>



/* 扫描重放表创建过程中的全部逻辑分配点。 */
static void testHttpDigestReplayCreateOom(void)
{
	xhttpdigestreplayconfig Config = { 2u, 4u, 60 };
	bool bComplete = false;
	size_t iFailures = 0;

	for ( size_t i = 0; i < 32u; i++ ) {
		xhttpdigestreplay* pReplay;
		bool bTriggered;

		testRequire(xrtMemDebugFailAfter((uint64)i),
			"HTTP Digest replay create OOM setup failed");
		pReplay = xrtHttpDigestReplayCreate(&Config);
		bTriggered = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pReplay == NULL ) {
			testRequire(bTriggered &&
				(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
				"HTTP Digest replay create failed without injected OOM");
			iFailures++;
		} else {
			testRequire(!bTriggered,
				"HTTP Digest replay create ignored allocation fault");
			xrtHttpDigestReplayDestroy(pReplay);
			bComplete = true;
		}
		xrtClearError();
		testMemoryDebugDrain(
			"HTTP Digest replay create OOM leaked storage"
		);
		if ( bComplete ) {
			break;
		}
	}
	testRequire(bComplete && (iFailures != 0),
		"HTTP Digest replay create OOM sweep did not converge");
}



/* 验证创建后的键派生、新键插入和已有键推进全部保持零分配。 */
static void testHttpDigestReplayHotPathNoAlloc(void)
{
	xhttpdigestreplayconfig Config = { 1u, 2u, 60 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaykey Key;
	xhttpdigestreplaystats Stats;

	testRequire(pReplay != NULL,
		"HTTP Digest replay no-allocation fixture failed");
	testRequire(xrtMemDebugFailAfter(0),
		"HTTP Digest replay hot-path allocation probe failed");
	testRequire(xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("user"),
		XRT_STR_LITERAL("nonce"),
		XRT_STR_LITERAL("cnonce"),
		&Key
	) && (xrtHttpDigestReplayCheckKey(
		pReplay, &Key, 1u, 1000, 1000
	) == XHTTP_DIGEST_REPLAY_ACCEPTED),
		"HTTP Digest replay new-key hot path failed");
	testRequire(!xrtMemDebugFailTriggered(),
		"HTTP Digest replay new-key hot path allocated");
	testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
		(Stats.Entries == 1u) && (Stats.Accepted == 1u),
		"HTTP Digest replay new-key state mismatch");
	testRequire(xrtHttpDigestReplayCheckKey(
		pReplay, &Key, 2u, 1000, 1000
	) == XHTTP_DIGEST_REPLAY_ACCEPTED,
		"HTTP Digest replay existing update failed");
	testRequire(!xrtMemDebugFailTriggered(),
		"HTTP Digest replay existing update allocated");
	xrtMemDebugFailClear();
	xrtHttpDigestReplayDestroy(pReplay);
	testMemoryDebugDrain(
		"HTTP Digest replay hot-path probe leaked storage"
	);
}



int main(void)
{
	testHttpDigestReplayCreateOom();
	testHttpDigestReplayHotPathNoAlloc();
	puts("[PASS] HTTP Digest replay OOM");
	return 0;
}
