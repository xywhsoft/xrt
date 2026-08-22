#ifndef XRT_MATH_H
#define XRT_MATH_H

#include <xrt/core.h>



/* XRT 固定使用的双精度数学常量。 */
#define XRT_PI	3.14159265358979323846264338327950288
#define XRT_TAU	(2.0 * XRT_PI)
#define XRT_E		2.71828182845904523536028747135266250



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_MATH)

/* 返回较小值；任一参数为 NaN 时传播该 NaN。 */
XRT_API double xrtMathMin(double fLeft, double fRight);



/* 返回较大值；任一参数为 NaN 时传播该 NaN。 */
XRT_API double xrtMathMax(double fLeft, double fRight);



/* 把数值限制到闭区间内；边界写反时自动交换。 */
XRT_API double xrtMathClamp(double fValue, double fMin, double fMax);



/* 返回 -1、0 或 1；NaN 与正负零返回 0。 */
XRT_API int xrtMathSign(double fValue);



/* 向零方向截断浮点数。 */
XRT_API double xrtMathTrunc(double fValue);



/* 返回 fValue - floor(fValue)，负数的小数部分仍位于 [0, 1)。 */
XRT_API double xrtMathFract(double fValue);



/* 使用 C fmod 语义计算浮点余数。 */
XRT_API double xrtMathMod(double fValue, double fDivisor);



/* 把角度转换为弧度。 */
XRT_API double xrtMathRad(double fDegrees);



/* 把弧度转换为角度。 */
XRT_API double xrtMathDeg(double fRadians);



/* 判断值是否为 NaN。 */
XRT_API bool xrtMathIsNaN(double fValue);



/* 判断值是否为正无穷或负无穷。 */
XRT_API bool xrtMathIsInf(double fValue);



/* 判断值是否为有限数。 */
XRT_API bool xrtMathIsFinite(double fValue);



/* 以 2 为底计算对数。 */
XRT_API double xrtMathLog2(double fValue);



/* 计算 2 的 fValue 次幂。 */
XRT_API double xrtMathExp2(double fValue);



/* 在接近零时仍高精度计算 log(1 + fValue)。 */
XRT_API double xrtMathLog1p(double fValue);



/* 在接近零时仍高精度计算 exp(fValue) - 1。 */
XRT_API double xrtMathExpm1(double fValue);



/* 计算保留符号的实数立方根。 */
XRT_API double xrtMathCbrt(double fValue);



/* 稳定计算 sqrt(fX * fX + fY * fY)。 */
XRT_API double xrtMathHypot(double fX, double fY);



/* 使用显式绝对与相对容差比较两个浮点数。 */
XRT_API bool xrtMathNear(double fLeft, double fRight,
	double fAbsoluteTolerance, double fRelativeTolerance);



/* 使用无符号绝对差容差比较两个 int64，计算过程不会溢出。 */
XRT_API bool xrtMathIntNear(int64 iLeft, int64 iRight, uint64 iTolerance);

#endif



XRT_EXTERN_C_END

#endif
