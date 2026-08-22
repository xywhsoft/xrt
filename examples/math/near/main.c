#include <stdio.h>

#include <xrt.h>



/* 演示零附近绝对容差和普通数值相对容差。 */
int main(void)
{
	printf("relative: %s\n",
		xrtMathNear(100.0, 100.05, 0.0, 0.001) ? "near" : "different");
	printf("absolute: %s\n",
		xrtMathNear(0.0, 1e-10, 1e-9, 0.0) ? "near" : "different");
	printf("integer : %s\n",
		xrtMathIntNear(1000, 1003, 5) ? "near" : "different");
	return 0;
}
