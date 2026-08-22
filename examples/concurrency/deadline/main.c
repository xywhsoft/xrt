#include <stdio.h>

#include <xrt.h>



/* 展示相对超时与绝对截止时间之间的统一转换。 */
int main(void)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(50000));

	printf("remaining: %llu us\n",
		(unsigned long long)xrtDeadlineRemaining(iDeadline));
	xrtSleepUntil(iDeadline);
	printf("expired: %s\n", xrtDeadlineExpired(iDeadline) ? "yes" : "no");
	return 0;
}
