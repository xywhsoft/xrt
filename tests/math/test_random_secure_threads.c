#include "../test.h"
#include "../test_thread.h"



#define TEST_RANDOM_SECURE_THREADS 8
#define TEST_RANDOM_SECURE_ROUNDS 256



typedef struct test_random_secure_context {
	uint8 Last[32];
} test_random_secure_context;



/* 每个线程反复读取系统随机源，验证并发调用不会共享可见中间状态。 */
static int testRandomSecureThread(ptr pData)
{
	test_random_secure_context* pContext =
		(test_random_secure_context*)pData;
	uint8 arrZero[sizeof(pContext->Last)];

	memset(arrZero, 0, sizeof(arrZero));
	for ( int i = 0; i < TEST_RANDOM_SECURE_ROUNDS; i++ ) {
		if ( !xrtSecureRandom(pContext->Last, sizeof(pContext->Last)) ||
			 (memcmp(pContext->Last, arrZero,
				sizeof(pContext->Last)) == 0) ) {
			return 1;
		}
	}
	return 0;
}



/* 多线程系统随机读取必须全部完成，最终样本也不得重复。 */
int main(void)
{
	test_random_secure_context arrContext[TEST_RANDOM_SECURE_THREADS];
	testthread arrThread[TEST_RANDOM_SECURE_THREADS];

	memset(arrContext, 0, sizeof(arrContext));
	for ( int i = 0; i < TEST_RANDOM_SECURE_THREADS; i++ ) {
		arrThread[i].Proc = testRandomSecureThread;
		arrThread[i].Data = &arrContext[i];
	}
	testThreadsStart(arrThread, TEST_RANDOM_SECURE_THREADS);
	testThreadsJoin(arrThread, TEST_RANDOM_SECURE_THREADS);
	for ( int i = 0; i < TEST_RANDOM_SECURE_THREADS; i++ ) {
		testRequire(arrThread[i].Result == 0,
			"concurrent secure random worker failed");
		for ( int j = 0; j < i; j++ ) {
			testRequire(memcmp(arrContext[i].Last, arrContext[j].Last,
				sizeof(arrContext[i].Last)) != 0,
				"concurrent secure random samples were identical");
		}
	}
	return 0;
}
