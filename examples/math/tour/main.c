/*
 * 范例：math/tour —— 数学函数补集（关系/分类/指对数/立方根）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【关系】    xrtMathMin / Max / Sign / Trunc / Mod / Rad
 *   【分类】    xrtMathIsNaN / IsInf / IsFinite（NaN/Inf/有限三分）
 *   【指对数】  xrtMathLog2 / Exp2 / Log1p / Expm1
 *   【根】      xrtMathCbrt（负数立方根）
 * 模块宏：XRT_MODULE_MATH
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/math/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   math: min/max/sign/trunc/mod/rad ok
 *   math: isnan/isinf/isfinite = 1/1/1
 *   math: log2/exp2/log1p/expm1 ok
 *   math: cbrt(-27) = -3
 *
 * 每个函数都用已知常数双向断言；分类三函数用
 *   0.0/0.0（NaN）、1.0/0.0（Inf）、1.5（有限）做三分样本。
 */

#include <stdio.h>
#include <math.h>
#include <xrt.h>

/* 浮点近似比较（容差 1e-9）。 */
static bool exampleNear(double fLeft, double fRight)
{
	return xrtMathNear(fLeft, fRight, 1e-9, 0.0);
}

int main(void)
{
	int iResult = 1;

	/* ---- 关系族 ---- */
	if ( (xrtMathMin(2.0, 3.0) != 2.0) ||
		(xrtMathMin(3.0, 2.0) != 2.0) ||
		(xrtMathMax(2.0, 3.0) != 3.0) ||
		(xrtMathMax(3.0, 2.0) != 3.0) ||
		(xrtMathMin(-1.0, 1.0) != -1.0) ) {
		goto Cleanup;
	}
	if ( (xrtMathSign(-5.0) != -1) ||
		(xrtMathSign(5.0) != 1) ||
		(xrtMathSign(0.0) != 0) ) {
		goto Cleanup;
	}
	if ( (xrtMathTrunc(2.7) != 2.0) ||
		(xrtMathTrunc(-2.7) != -2.0) ) {
		goto Cleanup;
	}
	if ( !exampleNear(xrtMathMod(7.0, 3.0), 1.0) ||
		!exampleNear(xrtMathMod(-7.0, 3.0), -1.0) ) {
		goto Cleanup;  /* 取模随被除数符号（C 的 fmod 截断语义） */
	}
	if ( !exampleNear(xrtMathRad(180.0), 3.14159265358979) ) {
		goto Cleanup;
	}
	printf("math: min/max/sign/trunc/mod/rad ok\n");

	/* ---- 分类族：NaN / Inf / 有限三分 ---- */
	{
		double fNaN = 0.0 / 0.0;
		double fInf = 1.0 / 0.0;

		if ( !xrtMathIsNaN(fNaN) ||
			xrtMathIsNaN(1.0) ||
			xrtMathIsNaN(fInf) ||
			!xrtMathIsInf(fInf) ||
			xrtMathIsInf(1.0) ||
			xrtMathIsInf(fNaN) ||
			!xrtMathIsFinite(1.5) ||
			xrtMathIsFinite(fNaN) ||
			xrtMathIsFinite(fInf) ) {
			goto Cleanup;
		}
	}
	printf("math: isnan/isinf/isfinite = 1/1/1\n");

	/* ---- 指对数族 ---- */
	if ( !exampleNear(xrtMathLog2(8.0), 3.0) ||
		!exampleNear(xrtMathLog2(1.0), 0.0) ||
		!exampleNear(xrtMathExp2(10.0), 1024.0) ||
		!exampleNear(xrtMathLog1p(0.0), 0.0) ||
		!exampleNear(xrtMathExpm1(0.0), 0.0) ||
		!exampleNear(xrtMathLog1p(1.0), 0.693147180559945) ||
		!exampleNear(xrtMathExpm1(1.0), 1.718281828459045) ) {
		goto Cleanup;
	}
	printf("math: log2/exp2/log1p/expm1 ok\n");

	/* ---- 立方根：负数域 ---- */
	if ( !exampleNear(xrtMathCbrt(27.0), 3.0) ||
		!exampleNear(xrtMathCbrt(-27.0), -3.0) ||
		!exampleNear(xrtMathCbrt(0.0), 0.0) ) {
		goto Cleanup;
	}
	printf("math: cbrt(-27) = %g\n", xrtMathCbrt(-27.0));
	iResult = 0;

Cleanup:
	return iResult;
}
