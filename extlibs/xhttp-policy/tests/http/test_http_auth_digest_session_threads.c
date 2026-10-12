#include "../test.h"

#include <xrt/http_auth.h>
#include <xrt/thread.h>



#define TEST_HTTP_DIGEST_SESSION_THREADS 16u
#define TEST_HTTP_DIGEST_SESSION_REQUESTS 64u



/* 每个线程把取得的 nonce-count 写入自己独占的结果区间。 */
typedef struct test_http_digest_session_thread {
	xhttpdigestsession* Session;
	uint32 Counts[TEST_HTTP_DIGEST_SESSION_REQUESTS];
} test_http_digest_session_thread;



/* 并发创建和释放 Exchange，模拟连接池上的并行请求。 */
static int32 testHttpDigestSessionThreadEntry(ptr pData)
{
	test_http_digest_session_thread* pContext =
		(test_http_digest_session_thread*)pData;

	for ( size_t i = 0;
		i < TEST_HTTP_DIGEST_SESSION_REQUESTS;
		i++ ) {
		xhttpdigestexchange* pExchange =
			xrtHttpDigestSessionAuthorize(
				pContext->Session,
				XRT_STR_LITERAL("GET"),
				XRT_STR_LITERAL("/parallel"),
				(xstrview){ NULL, 0 }
			);
		const xhttpdigestproof* pProof =
			xrtHttpDigestExchangeProof(pExchange);

		if ( pProof == NULL ) {
			xrtHttpDigestExchangeRelease(pExchange);
			return 1;
		}
		pContext->Counts[i] = pProof->NonceCount;
		xrtHttpDigestExchangeRelease(pExchange);
	}
	return 0;
}



/* 验证竞争下每个 nonce-count 恰好出现一次且没有间隙。 */
int main(void)
{
	xhttpdigestchallenge Challenge = {
		XHTTP_DIGEST_CHALLENGE_QOP_AUTH |
		XHTTP_DIGEST_CHALLENGE_ALGORITHM_EXPLICIT,
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("nonce"),
		{ NULL, 0 },
		{ NULL, 0 }
	};
	xhttpdigestchoice Choice = {
		XHTTP_DIGEST_ALGORITHM_SHA256,
		XHTTP_DIGEST_QOP_AUTH,
		false
	};
	xhttpdigestsessionconfig Config;
	xhttpdigestsession* pSession;
	test_http_digest_session_thread Contexts[
		TEST_HTTP_DIGEST_SESSION_THREADS
	];
	xthread* Threads[TEST_HTTP_DIGEST_SESSION_THREADS];
	bool Seen[
		(TEST_HTTP_DIGEST_SESSION_THREADS *
		 TEST_HTTP_DIGEST_SESSION_REQUESTS) + 1u
	];
	char Secret[XHTTP_DIGEST_MAX_TEXT_SIZE];
	size_t iSecretSize;

	testRequire(xrtHttpDigestSecret(
		Choice.Algorithm,
		XRT_STR_LITERAL("user"),
		Challenge.Realm,
		XRT_STR_LITERAL("password"),
		Secret,
		sizeof(Secret),
		&iSecretSize
	), "HTTP Digest session thread secret failed");
	Config = (xhttpdigestsessionconfig){
		0,
		&Challenge,
		&Choice,
		XRT_STR_INIT("user"),
		{ NULL, 0 },
		{ Secret, iSecretSize },
		XRT_STR_INIT("client")
	};
	pSession = xrtHttpDigestSessionCreate(&Config);
	testRequire(pSession != NULL,
		"HTTP Digest session thread fixture failed");
	memset(Contexts, 0, sizeof(Contexts));
	memset(Threads, 0, sizeof(Threads));
	memset(Seen, 0, sizeof(Seen));
	for ( size_t i = 0;
		i < TEST_HTTP_DIGEST_SESSION_THREADS;
		i++ ) {
		Contexts[i].Session = pSession;
		Threads[i] = xrtThreadCreate(
			testHttpDigestSessionThreadEntry,
			&Contexts[i],
			0
		);
		testRequire(Threads[i] != NULL,
			"HTTP Digest session thread create failed");
	}
	for ( size_t i = 0;
		i < TEST_HTTP_DIGEST_SESSION_THREADS;
		i++ ) {
		testRequire((xrtThreadWait(Threads[i]) == XWAIT_OK) &&
			(xrtThreadExitCode(Threads[i]) == 0),
			"HTTP Digest session worker failed");
		xrtThreadDestroy(Threads[i]);
		for ( size_t j = 0;
			j < TEST_HTTP_DIGEST_SESSION_REQUESTS;
			j++ ) {
			uint32 iCount = Contexts[i].Counts[j];

			testRequire((iCount != 0) &&
				(iCount < (sizeof(Seen) / sizeof(Seen[0]))) &&
				!Seen[iCount],
				"HTTP Digest session nonce-count was duplicated");
			Seen[iCount] = true;
		}
	}
	for ( size_t i = 1;
		i < (sizeof(Seen) / sizeof(Seen[0]));
		i++ ) {
		testRequire(Seen[i],
			"HTTP Digest session nonce-count contains a gap");
	}
	xrtHttpDigestSessionRelease(pSession);
	puts("[PASS] HTTP Digest client session threads");
	return 0;
}



#undef TEST_HTTP_DIGEST_SESSION_THREADS
#undef TEST_HTTP_DIGEST_SESSION_REQUESTS
