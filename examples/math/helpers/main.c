#include <stdio.h>

#include <xrt.h>



/* 演示常用数值、角度和稳定长度辅助函数。 */
int main(void)
{
	printf("clamp: %.1f\n", xrtMathClamp(12.0, 0.0, 10.0));
	printf("fract: %.2f\n", xrtMathFract(-1.25));
	printf("angle: %.1f\n", xrtMathDeg(XRT_PI));
	printf("hypot: %.1f\n", xrtMathHypot(3.0, 4.0));
	return 0;
}
