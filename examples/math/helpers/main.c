/*
 * 范例：math/helpers —— 数值辅助：钳制、取小数、角度换算、稳定长度
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMathClamp  三点钳制（越界取边界，动画/滑杆标配）
 *   xrtMathFract  取小数部分（负数也返回正值：-1.25 → 0.75）
 *   xrtMathDeg    弧度 → 角度（配合 XRT_PI）
 *   xrtMathHypot  √(x²+y²) 的稳定实现（大数不溢出）
 * 模块宏：XRT_MODULE_MATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/helpers/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   clamp: 10.0
 *   fract: 0.75
 *   angle: 180.0
 *   hypot: 5.0
 *
 * 为什么要库函数而不是手写：
 *   Fract 负数语义（返回 +0.75 而非 -0.25）与着色器/图形学一致；
 *   Hypot 中间过程不先算 x²（3e200 会溢出 double），大坐标安全。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	printf("clamp: %.1f\n", xrtMathClamp(12.0, 0.0, 10.0));  /* 超上界→10 */
	printf("fract: %.2f\n", xrtMathFract(-1.25));           /* -1.25→0.75 */
	printf("angle: %.1f\n", xrtMathDeg(XRT_PI));            /* π rad→180° */
	printf("hypot: %.1f\n", xrtMathHypot(3.0, 4.0));        /* 勾股 3-4-5 */
	return 0;
}
