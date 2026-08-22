#include "../test.h"

#include "../test_fault_allocator.h"



#define TEST_LOG_CONSOLE_OOM_SINKS 1024u
#define TEST_LOG_CONSOLE_OOM_ROUNDS 4u



/* 创建并立即释放默认 Console Sink。 */
static bool testLogConsoleAttempt(void)
{
	xlogsink* pSink = xrtLogConsole(NULL);
	bool bResult = pSink != NULL;

	xrtLogSinkFree(pSink);
	return bResult;
}



/* 持续占用小块，直到下一次 backing-span 分配命中故障。 */
static void testLogConsoleOomRound(testfaultallocator* pAllocator)
{
	xlogsink* arrSinks[TEST_LOG_CONSOLE_OOM_SINKS];
	xlogsink* pRecovery;
	size_t iCount = 0;

	memset(arrSinks, 0, sizeof(arrSinks));
	pAllocator->FailAt = pAllocator->Calls + 1u;
	pAllocator->Hit = false;
	while ( iCount < TEST_LOG_CONSOLE_OOM_SINKS ) {
		arrSinks[iCount] = xrtLogConsole(NULL);
		if ( arrSinks[iCount] == NULL ) {
			break;
		}
		iCount++;
	}
	testRequire(
		(iCount < TEST_LOG_CONSOLE_OOM_SINKS) &&
		pAllocator->Hit &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Logger console backing OOM was not transactional"
	);
	xrtClearError();
	pAllocator->FailAt = SIZE_MAX;
	pRecovery = xrtLogConsole(NULL);
	testRequire(
		pRecovery != NULL,
		"Logger console did not recover after backing OOM"
	);
	xrtLogSinkFree(pRecovery);
	for ( size_t i = 0; i < iCount; i++ ) {
		xrtLogSinkFree(arrSinks[i]);
	}
}



/* 验证两个小对象尺寸类反复扩容失败都能完整回收。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);

	testRequire(
		xrtSetAllocator(&Allocator),
		"Logger console OOM allocator setup failed"
	);
	testRequire(testLogConsoleAttempt(), "Logger console OOM warm-up failed");
	for ( size_t i = 0; i < TEST_LOG_CONSOLE_OOM_ROUNDS; i++ ) {
		testLogConsoleOomRound(&State);
	}
	testRequire(
		testLogConsoleAttempt(),
		"Logger console final OOM recovery failed"
	);
	testMemoryDebugDrain("Logger console OOM memory debug reset failed");
	printf(
		"[PASS] Logger console OOM (%u backing rounds)\n",
		TEST_LOG_CONSOLE_OOM_ROUNDS
	);
	return 0;
}
