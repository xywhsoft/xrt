#include "../test.h"

#include <float.h>
#include <math.h>



/* 基础辅助函数必须明确传播 NaN 并规范处理正负零。 */
static void testBasicHelpers(void)
{
	double fNaN = 0.0 / 0.0;
	double fNegativeZero = -0.0;

	testRequire(xrtMathMin(-2.0, 5.0) == -2.0, "math min failed");
	testRequire(xrtMathMax(-2.0, 5.0) == 5.0, "math max failed");
	testRequire(signbit(xrtMathMin(0.0, fNegativeZero)),
		"math min lost negative zero");
	testRequire(!signbit(xrtMathMax(fNegativeZero, 0.0)),
		"math max did not prefer positive zero");
	testRequire(xrtMathIsNaN(xrtMathMin(fNaN, 1.0)), "math min did not propagate NaN");
	testRequire(xrtMathIsNaN(xrtMathMax(1.0, fNaN)), "math max did not propagate NaN");
	testRequire(xrtMathClamp(12.0, 0.0, 10.0) == 10.0, "math clamp high failed");
	testRequire(xrtMathClamp(-2.0, 10.0, 0.0) == 0.0,
		"math clamp did not normalize bounds");
	testRequire(xrtMathIsNaN(xrtMathClamp(1.0, fNaN, 2.0)),
		"math clamp did not propagate NaN");
	testRequire(xrtMathSign(-3.0) == -1, "math negative sign failed");
	testRequire(xrtMathSign(0.0) == 0, "math zero sign failed");
	testRequire(xrtMathSign(fNaN) == 0, "math NaN sign failed");
}



/* 数值转换和特殊值判断覆盖负数、零、NaN 与无穷。 */
static void testNumericHelpers(void)
{
	double fNaN = 0.0 / 0.0;
	double fInf = HUGE_VAL;

	testRequire(xrtMathTrunc(3.9) == 3.0, "positive truncation failed");
	testRequire(xrtMathTrunc(-3.9) == -3.0, "negative truncation failed");
	testRequire(fabs(xrtMathFract(-1.25) - 0.75) <= 1e-15,
		"negative fract failed");
	testRequire(xrtMathMod(-7.0, 4.0) == -3.0, "floating modulo failed");
	testRequire(fabs(xrtMathDeg(XRT_PI) - 180.0) <= 1e-14,
		"radian to degree conversion failed");
	testRequire(fabs(xrtMathRad(180.0) - XRT_PI) <= 1e-14,
		"degree to radian conversion failed");
	testRequire(xrtMathIsNaN(fNaN), "NaN detection failed");
	testRequire(xrtMathIsInf(fInf) && xrtMathIsInf(-fInf), "infinity detection failed");
	testRequire(!xrtMathIsFinite(fNaN) && !xrtMathIsFinite(fInf),
		"non-finite detection failed");
	testRequire(xrtMathIsFinite(DBL_MAX) && xrtMathIsFinite(-DBL_MAX),
		"finite extreme detection failed");
}



/* 兼容数学函数在小值和大值上都要保持精度与范围。 */
static void testTranscendentals(void)
{
	double fTiny = 1e-12;
	double fLargeHypot = xrtMathHypot(DBL_MAX / 4.0, DBL_MAX / 4.0);

	testRequire(fabs(xrtMathLog2(8.0) - 3.0) <= 1e-14, "log2 failed");
	testRequire(fabs(xrtMathExp2(10.0) - 1024.0) <= 1e-12, "exp2 failed");
	testRequire(fabs(xrtMathLog1p(fTiny) - 9.999999999995e-13) <= 1e-27,
		"log1p lost small input precision");
	testRequire(fabs(xrtMathExpm1(fTiny) - 1.0000000000005e-12) <= 1e-27,
		"expm1 lost small input precision");
	testRequire(signbit(xrtMathLog1p(-0.0)),
		"log1p lost negative zero");
	testRequire(signbit(xrtMathExpm1(-0.0)),
		"expm1 lost negative zero");
	testRequire(fabs(xrtMathCbrt(27.0) - 3.0) <= 1e-14, "positive cbrt failed");
	testRequire(fabs(xrtMathCbrt(-125.0) + 5.0) <= 1e-14, "negative cbrt failed");
	testRequire(fabs(xrtMathHypot(3.0, 4.0) - 5.0) <= 1e-15, "hypot failed");
	testRequire(xrtMathIsFinite(fLargeHypot), "scaled hypot overflowed");
	testRequire(xrtMathIsInf(xrtMathHypot(HUGE_VAL, 1.0)),
		"hypot did not preserve infinity");
}



/* 显式容差比较必须处理零附近、极值、NaN 与无穷。 */
static void testNear(void)
{
	double fNaN = 0.0 / 0.0;
	double fInf = HUGE_VAL;

	testRequire(xrtMathNear(100.0, 100.05, 0.0, 0.001),
		"relative near comparison failed");
	testRequire(!xrtMathNear(100.0, 100.2, 0.0, 0.001),
		"relative near accepted a distant value");
	testRequire(xrtMathNear(10000.0, 9900.0, 0.0, 0.01),
		"legacy relative tolerance boundary changed");
	testRequire(!xrtMathNear(10000.0, 9899.0, 0.0, 0.01),
		"legacy relative tolerance accepted an out-of-range value");
	testRequire(xrtMathNear(0.0, 1e-10, 1e-9, 0.0),
		"absolute near comparison failed near zero");
	testRequire(xrtMathNear(fInf, fInf, 0.0, 0.0),
		"equal infinities were not near");
	testRequire(!xrtMathNear(fInf, DBL_MAX, HUGE_VAL, HUGE_VAL),
		"finite and infinite values were near");
	testRequire(!xrtMathNear(fNaN, fNaN, HUGE_VAL, HUGE_VAL),
		"NaN values were near");

	testRequire(xrtMathIntNear(INT64_MIN, INT64_MIN + 7, 7),
		"integer near failed at INT64_MIN");
	testRequire(!xrtMathIntNear(INT64_MIN, INT64_MAX, UINT64_MAX - 1u),
		"integer near overflowed full-domain difference");
	testRequire(xrtMathIntNear(INT64_MIN, INT64_MAX, UINT64_MAX),
		"integer near rejected full-domain tolerance");

	xrtClearError();
	testRequire(!xrtMathNear(1.0, 1.0, -1.0, 0.0),
		"negative tolerance did not fail");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"negative tolerance reported the wrong error");
}



/* 执行数学辅助、特殊值、精度和显式容差测试。 */
int main(void)
{
	testBasicHelpers();
	testNumericHelpers();
	testTranscendentals();
	testNear();
	return 0;
}
