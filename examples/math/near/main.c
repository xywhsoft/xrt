/*
 * 范例：math/near —— 浮点比较：绝对容差 + 相对容差的正确组合
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMathNear    |a-b| ≤ max(abs, rel×|a|) 的组合容差判断
 *   xrtMathIntNear 整数"接近"判断（|a-b| ≤ 容差）
 * 模块宏：XRT_MODULE_MATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/math/near/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   relative: near
 *   absolute: near
 *   integer : near
 *
 * 为什么不能用 == 比较浮点：0.1+0.2 != 0.3 是浮点常识。
 * 但单一容差也不够：
 *   相对容差在 0 附近失效（0 与 1e-10 差 10 倍却都该算"零"）；
 *   绝对容差在数值大时失效（1e9 差 0.05 精度上无意义）。
 * Near 同时接受两个容差并取 max——两组场景各自传所需的那个。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 相对容差：100 vs 100.05，差 0.05 ≤ 0.001×100 → near。 */
	printf("relative: %s\n",
		xrtMathNear(100.0, 100.05, 0.0, 0.001) ? "near" : "different");

	/* 绝对容差：零附近，1e-10 ≤ 1e-9 → near（相对容差在此失效）。 */
	printf("absolute: %s\n",
		xrtMathNear(0.0, 1e-10, 1e-9, 0.0) ? "near" : "different");

	/* 整数版：|1000-1003| = 3 ≤ 5 → near（采样抖动判断等场景）。 */
	printf("integer : %s\n",
		xrtMathIntNear(1000, 1003, 5) ? "near" : "different");
	return 0;
}
