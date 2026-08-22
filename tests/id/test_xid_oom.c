#include "../test.h"

#include "../test_fault_allocator.h"



/* 格式化固定 XID，失败和成功都不保留临时分配。 */
static bool testXidOomAttempt(const xid* pXid)
{
	str sText = xrtXidFormat(pXid);
	bool bComplete = sText != NULL;

	xrtFree(sText);
	return bComplete;
}



/* 验证唯一文本分配失败可见，并且堆在失败后可以继续使用。 */
int main(void)
{
	xid Value = XID_ZERO;
	testfaultallocator State = { 0, 2u, 0, false };
	xallocator Allocator = testFaultAllocator(&State);

	for ( size_t i = 0; i < XID_BINARY_SIZE; i++ ) {
		Value.Data[i] = (uint8)(i + 1u);
	}
	testRequire(
		xrtSetAllocator(&Allocator),
		"XID OOM allocator install failed"
	);
	testRequire(
		!testXidOomAttempt(&Value),
		"XID formatting did not propagate span-allocation OOM"
	);
	testRequire(
		State.Hit && (State.Calls == 2u),
		"XID OOM span allocation point was not reached"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_MEMORY,
		"XID OOM did not preserve the memory error"
	);
	xrtClearError();
	State.FailAt = SIZE_MAX;
	testRequire(
		testXidOomAttempt(&Value),
		"XID formatting did not recover after OOM"
	);
	xrtClearError();
	printf("[PASS] XID OOM\n");
	return 0;
}
