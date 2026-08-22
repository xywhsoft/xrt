#include <stdio.h>

#include <xrt.h>



/* 展示整数单调时钟和旧版保留的浮点秒便捷计时器。 */
int main(void)
{
	uint64 iStart = xrtClock();
	double fStart = xrtTimer();

	xrtSleep(10);
	printf("elapsed_us=%llu\n", (unsigned long long)(xrtClock() - iStart));
	printf("elapsed_s=%.6f\n", xrtTimer() - fStart);
	return 0;
}
