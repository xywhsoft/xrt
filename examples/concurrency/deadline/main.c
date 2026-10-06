/* 相对毫秒等待和 double 秒计时。 */
#include <stdio.h>
#include <xrt.h>
int main(void)
{
    double Start = xrtTimer();
    xrtSleep(50);
    printf("elapsed_s=%.6f forever_ms=%lld\n", xrtTimer() - Start, (long long)XRT_WAIT_FOREVER);
    return 0;
}
