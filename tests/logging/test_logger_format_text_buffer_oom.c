#include "../test.h"

#include "../test_fault_allocator.h"



/* 格式化并立即释放大消息。 */
static bool testLogTextBufferAttempt(
	const xlogrecord* pRecord,
	const xlogtextconfig* pConfig
)
{
	str sText = xrtLogText(pRecord, pConfig, NULL);
	bool bResult = sText != NULL;

	xrtFree(sText);
	return bResult;
}



/* 扫描分配式文本 Helper 的全部稳定底层分配点。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	xlogtextconfig Config;
	xlogrecord Record;
	char arrMessage[4096];
	size_t iBaseline;
	size_t iCalls;

	memset(arrMessage, 'm', sizeof(arrMessage));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = (xstrview){ arrMessage, sizeof(arrMessage) };
	testRequire(
		xrtLogTextConfigInit(&Config, XLOG_TEXT_MESSAGE) &&
		xrtSetAllocator(&Allocator),
		"Logger text OOM fixture setup failed"
	);
	testRequire(
		testLogTextBufferAttempt(&Record, &Config),
		"Logger text OOM warm-up failed"
	);
	testMemoryDebugDrain("Logger text OOM memory debug reset failed");
	iBaseline = State.Live;
	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testLogTextBufferAttempt(&Record, &Config),
		"Logger text OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(iCalls != 0, "Logger text OOM reached no allocation");
	testMemoryDebugDrain("Logger text OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "Logger text OOM baseline leaked");
	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testLogTextBufferAttempt(&Record, &Config),
			"Logger text survived injected OOM"
		);
		testRequire(State.Hit, "Logger text OOM target was not reached");
		xrtClearError();
		testMemoryDebugDrain("Logger text OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "Logger text OOM leaked");
	}
	State.FailAt = SIZE_MAX;
	testRequire(
		testLogTextBufferAttempt(&Record, &Config),
		"Logger text did not recover after OOM"
	);
	testMemoryDebugDrain("Logger text OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "Logger text recovery leaked");
	printf("[PASS] Logger text buffer OOM (%zu allocation points)\n", iCalls);
	return 0;
}
