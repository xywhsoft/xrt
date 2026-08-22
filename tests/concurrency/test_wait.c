#include "../test.h"



/* 验证单调截止时间构造、饱和与剩余时间语义。 */
int main(void)
{
	xdeadline iDeadline;
	uint64 iRemaining;

	testRequire(
		xrtDeadlineAfter(UINT64_MAX) == XRT_DEADLINE_NEVER,
		"infinite timeout did not produce the never deadline"
	);
	testRequire(
		!xrtDeadlineExpired(XRT_DEADLINE_NEVER),
		"never deadline was reported expired"
	);
	testRequire(
		xrtDeadlineRemaining(XRT_DEADLINE_NEVER) == UINT64_MAX,
		"never deadline remaining time mismatch"
	);

	iDeadline = xrtDeadlineAfter(UINT64_C(20000));
	iRemaining = xrtDeadlineRemaining(iDeadline);
	testRequire(
		(iRemaining > 0) && (iRemaining <= UINT64_C(20000)),
		"finite deadline remaining time mismatch"
	);
	xrtSleepUntil(iDeadline);
	testRequire(xrtDeadlineExpired(iDeadline), "elapsed deadline was not expired");
	testRequire(
		xrtDeadlineRemaining(iDeadline) == 0,
		"elapsed deadline retained remaining time"
	);

	printf("[PASS] wait\n");
	return 0;
}
