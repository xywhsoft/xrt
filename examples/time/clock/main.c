/* xrtTimer 用单调时钟返回 double 秒；直接相减测量耗时。 */
#include <stdio.h>

#include <xrt.h>



int main(void)
{
    double Start = xrtTimer();
    xrtSleep(10);
    printf("elapsed_s=%.9f\n", xrtTimer() - Start);
    return 0;
}
