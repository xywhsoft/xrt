#include "../test.h"



/* 验证引用计数正常路径和全部失效边界。 */
int main(void)
{
	volatile int32 iCount = 1;
	volatile int32 iZero = 0;
	volatile int32 iNegative = -1;
	volatile int32 iMaximum = INT32_MAX;

	testRequire(xrtRefRetain(&iCount) == 2, "retain result mismatch");
	testRequire(xrtRefRelease(&iCount) == 1, "release result mismatch");
	testRequire(xrtRefRelease(&iCount) == 0, "final release mismatch");
	testRequire(xrtRefRetain(&iCount) == -1, "dead reference must not revive");
	testRequire(xrtRefRelease(&iCount) == -1, "dead reference must not underflow");
	testRequire(xrtRefRetain(&iZero) == -1, "zero retain must fail");
	testRequire(xrtRefRelease(&iZero) == -1, "zero release must fail");
	testRequire(xrtRefRetain(&iNegative) == -1, "negative retain must fail");
	testRequire(xrtRefRelease(&iNegative) == -1, "negative release must fail");
	testRequire(xrtRefRetain(&iMaximum) == -1, "maximum retain must not overflow");
	testRequire(iMaximum == INT32_MAX, "failed retain changed maximum count");
	testRequire(xrtRefRetain(NULL) == -1, "null retain must fail");
	testRequire(xrtRefRelease(NULL) == -1, "null release must fail");
	printf("[PASS] ref\n");
	return 0;
}
