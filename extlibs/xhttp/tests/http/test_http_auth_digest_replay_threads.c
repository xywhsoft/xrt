#include "../test.h"

#include <xrt/http_auth.h>
#include <xrt/thread.h>



#define TEST_HTTP_DIGEST_REPLAY_THREADS 24u



/* 每个线程对同一个键提交同一个 nc，只有一个线程可以推进状态。 */
typedef struct test_http_digest_replay_thread {
	xhttpdigestreplay* Replay;
	xhttpdigestreplaykey Key;
	uint32 NonceCount;
	xhttpdigestreplaycheck Check;
} test_http_digest_replay_thread;



/* 执行一次共享重放表检查。 */
static int32 testHttpDigestReplayThreadEntry(ptr pData)
{
	test_http_digest_replay_thread* pThread =
		(test_http_digest_replay_thread*)pData;

	pThread->Check = xrtHttpDigestReplayCheckKey(
		pThread->Replay,
		&pThread->Key,
		pThread->NonceCount,
		1000,
		1000
	);
	return pThread->Check == XHTTP_DIGEST_REPLAY_ERROR ? 1 : 0;
}



/* 启动一轮竞争并验证同一 nc 恰好有一个胜者。 */
static void testHttpDigestReplayThreadRound(
	xhttpdigestreplay* pReplay,
	const xhttpdigestreplaykey* pKey,
	uint32 iNonceCount
)
{
	test_http_digest_replay_thread Contexts[
		TEST_HTTP_DIGEST_REPLAY_THREADS
	];
	xthread* Threads[TEST_HTTP_DIGEST_REPLAY_THREADS];
	size_t iAccepted = 0;
	size_t iReplayed = 0;

	memset(Contexts, 0, sizeof(Contexts));
	memset(Threads, 0, sizeof(Threads));
	for ( size_t i = 0; i < TEST_HTTP_DIGEST_REPLAY_THREADS; i++ ) {
		Contexts[i].Replay = pReplay;
		memcpy(&Contexts[i].Key, pKey, sizeof(*pKey));
		Contexts[i].NonceCount = iNonceCount;
		Threads[i] = xrtThreadCreate(
			testHttpDigestReplayThreadEntry,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP Digest replay thread create failed");
	}
	for ( size_t i = 0; i < TEST_HTTP_DIGEST_REPLAY_THREADS; i++ ) {
		testRequire(
			(xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"HTTP Digest replay worker failed"
		);
		xrtThreadDestroy(Threads[i]);
		if ( Contexts[i].Check == XHTTP_DIGEST_REPLAY_ACCEPTED ) {
			iAccepted++;
		} else if ( Contexts[i].Check ==
			XHTTP_DIGEST_REPLAY_REPLAY ) {
			iReplayed++;
		}
	}
	testRequire((iAccepted == 1u) &&
		(iReplayed == (TEST_HTTP_DIGEST_REPLAY_THREADS - 1u)),
		"HTTP Digest replay same-count race was not atomic");
}



int main(void)
{
	xhttpdigestreplayconfig Config = { 8u, 16u, 60 };
	xhttpdigestreplay* pReplay = xrtHttpDigestReplayCreate(&Config);
	xhttpdigestreplaykey Key;
	xhttpdigestreplaystats Stats;

	testRequire((pReplay != NULL) && xrtHttpDigestReplayKey(
		XRT_STR_LITERAL("thread-user"),
		XRT_STR_LITERAL("thread-nonce"),
		XRT_STR_LITERAL("thread-cnonce"),
		&Key
	), "HTTP Digest replay thread fixture failed");
	testHttpDigestReplayThreadRound(pReplay, &Key, 1u);
	testHttpDigestReplayThreadRound(pReplay, &Key, 2u);
	testRequire(xrtHttpDigestReplayStats(pReplay, &Stats) &&
		(Stats.Entries == 1u) && (Stats.Accepted == 2u) &&
		(Stats.Replayed ==
		 (2u * (TEST_HTTP_DIGEST_REPLAY_THREADS - 1u))),
		"HTTP Digest replay thread statistics mismatch");
	xrtHttpDigestReplayDestroy(pReplay);
	puts("[PASS] HTTP Digest replay threads");
	return 0;
}



#undef TEST_HTTP_DIGEST_REPLAY_THREADS
