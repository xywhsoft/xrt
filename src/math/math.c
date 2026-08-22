#include "../internal/xrt_internal.h"

#include <float.h>
#include <math.h>



#if defined(XRT_FEATURE_MATH)

/* 返回较小值，并为相等的正负零选择负零。 */
XRT_API double xrtMathMin(double fLeft, double fRight)
{
	if ( xrtMathIsNaN(fLeft) ) {
		return fLeft;
	}
	if ( xrtMathIsNaN(fRight) ) {
		return fRight;
	}
	if ( (fLeft == 0.0) && (fRight == 0.0) ) {
		return (signbit(fLeft) || signbit(fRight)) ? -0.0 : 0.0;
	}
	return fLeft < fRight ? fLeft : fRight;
}



/* 返回较大值，并为相等的正负零选择正零。 */
XRT_API double xrtMathMax(double fLeft, double fRight)
{
	if ( xrtMathIsNaN(fLeft) ) {
		return fLeft;
	}
	if ( xrtMathIsNaN(fRight) ) {
		return fRight;
	}
	if ( (fLeft == 0.0) && (fRight == 0.0) ) {
		return (signbit(fLeft) && signbit(fRight)) ? -0.0 : 0.0;
	}
	return fLeft > fRight ? fLeft : fRight;
}



/* 传播 NaN 后把值限制到规范化的闭区间。 */
XRT_API double xrtMathClamp(double fValue, double fMin, double fMax)
{
	double fSwap;

	if ( xrtMathIsNaN(fValue) ) {
		return fValue;
	}
	if ( xrtMathIsNaN(fMin) ) {
		return fMin;
	}
	if ( xrtMathIsNaN(fMax) ) {
		return fMax;
	}
	if ( fMin > fMax ) {
		fSwap = fMin;
		fMin = fMax;
		fMax = fSwap;
	}
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}



/* 返回数值的三态符号。 */
XRT_API int xrtMathSign(double fValue)
{
	if ( xrtMathIsNaN(fValue) || (fValue == 0.0) ) {
		return 0;
	}
	return fValue > 0.0 ? 1 : -1;
}



/* 使用 floor 和 ceil 保持旧 CRT 上的可移植截断。 */
XRT_API double xrtMathTrunc(double fValue)
{
	if ( !xrtMathIsFinite(fValue) || (fValue == 0.0) ) {
		return fValue;
	}
	return fValue > 0.0 ? floor(fValue) : ceil(fValue);
}



/* 按 shader 风格返回位于 [0, 1) 的小数部分。 */
XRT_API double xrtMathFract(double fValue)
{
	return fValue - floor(fValue);
}



/* 直接保留 C fmod 的符号和特殊值规则。 */
XRT_API double xrtMathMod(double fValue, double fDivisor)
{
	return fmod(fValue, fDivisor);
}



/* 用固定圆周率把角度转换为弧度。 */
XRT_API double xrtMathRad(double fDegrees)
{
	return fDegrees * (XRT_PI / 180.0);
}



/* 用固定圆周率把弧度转换为角度。 */
XRT_API double xrtMathDeg(double fRadians)
{
	return fRadians * (180.0 / XRT_PI);
}



/* 通过 NaN 不等于自身的标准性质完成判断。 */
XRT_API bool xrtMathIsNaN(double fValue)
{
	return fValue != fValue;
}



/* 使用 DBL_MAX 比较，避免执行无穷减无穷。 */
XRT_API bool xrtMathIsInf(double fValue)
{
	return (fValue > DBL_MAX) || (fValue < -DBL_MAX);
}



/* NaN 和正负无穷之外的值都是有限数。 */
XRT_API bool xrtMathIsFinite(double fValue)
{
	return (fValue <= DBL_MAX) && (fValue >= -DBL_MAX);
}



/* 用通用 log 保持旧 Windows CRT 的可用性。 */
XRT_API double xrtMathLog2(double fValue)
{
	return log(fValue) * 1.44269504088896340735992468100189214;
}



/* 用通用 pow 保持旧 Windows CRT 的可用性。 */
XRT_API double xrtMathExp2(double fValue)
{
	return pow(2.0, fValue);
}



/* 小输入使用交错级数，避免 1 + x 舍入为 1。 */
XRT_API double xrtMathLog1p(double fValue)
{
	double fAbsolute = fabs(fValue);

	if ( fValue == 0.0 ) {
		return fValue;
	}
	if ( (fAbsolute > 0.0) && (fAbsolute < 0.0001) ) {
		double fSum = 0.0;
		double fTerm = fValue;

		for ( int i = 1; i <= 32; i++ ) {
			fSum += fTerm / (double)i;
			fTerm *= -fValue;
		}
		return fSum;
	}
	return log(1.0 + fValue);
}



/* 小输入使用指数级数，避免 exp(x) 舍入为 1。 */
XRT_API double xrtMathExpm1(double fValue)
{
	double fAbsolute = fabs(fValue);

	if ( fValue == 0.0 ) {
		return fValue;
	}
	if ( (fAbsolute > 0.0) && (fAbsolute < 0.0001) ) {
		double fSum = fValue;
		double fTerm = fValue;

		for ( int i = 2; i <= 32; i++ ) {
			fTerm *= fValue / (double)i;
			fSum += fTerm;
		}
		return fSum;
	}
	return exp(fValue) - 1.0;
}



/* 用 pow 得到初值，再执行一次牛顿迭代提高普通值精度。 */
XRT_API double xrtMathCbrt(double fValue)
{
	double fAbsolute;
	double fRoot;

	if ( !xrtMathIsFinite(fValue) || (fValue == 0.0) ) {
		return fValue;
	}
	fAbsolute = fabs(fValue);
	fRoot = pow(fAbsolute, 1.0 / 3.0);
	fRoot = ((2.0 * fRoot) + (fAbsolute / (fRoot * fRoot))) / 3.0;
	return fValue < 0.0 ? -fRoot : fRoot;
}



/* 缩放较小分量，避免直接平方更容易上溢或下溢。 */
XRT_API double xrtMathHypot(double fX, double fY)
{
	double fAbsoluteX;
	double fAbsoluteY;
	double fRatio;
	double fSwap;

	if ( xrtMathIsInf(fX) || xrtMathIsInf(fY) ) {
		return HUGE_VAL;
	}
	if ( xrtMathIsNaN(fX) ) {
		return fX;
	}
	if ( xrtMathIsNaN(fY) ) {
		return fY;
	}

	fAbsoluteX = fabs(fX);
	fAbsoluteY = fabs(fY);
	if ( fAbsoluteX < fAbsoluteY ) {
		fSwap = fAbsoluteX;
		fAbsoluteX = fAbsoluteY;
		fAbsoluteY = fSwap;
	}
	if ( fAbsoluteX == 0.0 ) {
		return 0.0;
	}
	fRatio = fAbsoluteY / fAbsoluteX;
	return fAbsoluteX * sqrt(1.0 + (fRatio * fRatio));
}



/* 按 Python math.isclose 风格组合绝对与相对容差。 */
XRT_API bool xrtMathNear(double fLeft, double fRight,
	double fAbsoluteTolerance, double fRelativeTolerance)
{
	double fDifference;
	double fScale;

	if ( (fAbsoluteTolerance < 0.0) || (fRelativeTolerance < 0.0) ||
		 xrtMathIsNaN(fAbsoluteTolerance) || xrtMathIsNaN(fRelativeTolerance) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( fLeft == fRight ) {
		return true;
	}
	if ( !xrtMathIsFinite(fLeft) || !xrtMathIsFinite(fRight) ) {
		return false;
	}

	fDifference = fabs(fLeft - fRight);
	fScale = xrtMathMax(fabs(fLeft), fabs(fRight));
	return (fDifference <= fAbsoluteTolerance) ||
		(fDifference <= (fRelativeTolerance * fScale));
}



/* 通过无符号减法取得完整 int64 域内的绝对差。 */
XRT_API bool xrtMathIntNear(int64 iLeft, int64 iRight, uint64 iTolerance)
{
	uint64 iDifference = iLeft >= iRight ?
		((uint64)iLeft - (uint64)iRight) : ((uint64)iRight - (uint64)iLeft);

	return iDifference <= iTolerance;
}

#endif
