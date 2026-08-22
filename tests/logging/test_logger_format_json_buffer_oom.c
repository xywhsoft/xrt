#include "../test.h"

#include "../test_fault_allocator.h"



/* 格式化并立即释放大 JSON 消息。 */
static bool testLogJsonBufferAttempt(
	const xlogrecord* pRecord,
	const xlogjsonconfig* pConfig
)
{
	str sJson = xrtLogJson(pRecord, pConfig, NULL);
	bool bResult = sJson != NULL;

	xrtFree(sJson);
	return bResult;
}



/* 扫描分配式 JSON Helper 的全部稳定底层分配点。 */
int main(void)
{
	static testfaultallocator State = { 0, SIZE_MAX, 0, false };
	xallocator Allocator = testFaultAllocator(&State);
	xlogjsonconfig Config;
	xlogrecord Record;
	char arrMessage[4096];
	size_t iBaseline;
	size_t iCalls;

	memset(arrMessage, 'm', sizeof(arrMessage));
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = (xstrview){ arrMessage, sizeof(arrMessage) };
	testRequire(
		xrtLogJsonConfigInit(&Config) &&
		xrtSetAllocator(&Allocator),
		"Logger JSON OOM fixture setup failed"
	);
	Config.Flags = XLOG_JSON_MESSAGE;
	testRequire(
		testLogJsonBufferAttempt(&Record, &Config),
		"Logger JSON OOM warm-up failed"
	);
	testMemoryDebugDrain("Logger JSON OOM memory debug reset failed");
	iBaseline = State.Live;
	State.Calls = 0;
	State.FailAt = SIZE_MAX;
	testRequire(
		testLogJsonBufferAttempt(&Record, &Config),
		"Logger JSON OOM baseline failed"
	);
	iCalls = State.Calls;
	testRequire(iCalls != 0, "Logger JSON OOM reached no allocation");
	testMemoryDebugDrain("Logger JSON OOM baseline reset failed");
	testRequire(State.Live == iBaseline, "Logger JSON OOM baseline leaked");
	for ( size_t iFail = 1u; iFail <= iCalls; iFail++ ) {
		State.Calls = 0;
		State.FailAt = iFail;
		State.Hit = false;
		testRequire(
			!testLogJsonBufferAttempt(&Record, &Config),
			"Logger JSON survived injected OOM"
		);
		testRequire(State.Hit, "Logger JSON OOM target was not reached");
		xrtClearError();
		testMemoryDebugDrain("Logger JSON OOM memory debug reset failed");
		testRequire(State.Live == iBaseline, "Logger JSON OOM leaked");
	}
	State.FailAt = SIZE_MAX;
	testRequire(
		testLogJsonBufferAttempt(&Record, &Config),
		"Logger JSON did not recover after OOM"
	);
	testMemoryDebugDrain("Logger JSON OOM recovery reset failed");
	testRequire(State.Live == iBaseline, "Logger JSON recovery leaked");
	printf("[PASS] Logger JSON buffer OOM (%zu allocation points)\n", iCalls);
	return 0;
}
