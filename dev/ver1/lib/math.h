


/*
	PCG Random Number Generation for C. [ Update : 2025/08/19 from https://github.com/imneme/pcg-c-basic ]
	修改：
		整合到 xrt 库
		提供 Ex 版本 API（调用者管理状态，线程安全）
		普通版本 API 使用线程状态（线程隔离，性能优先）
	使用协议注意事项：
		Apache License, Version 2.0 协议
		允许个人使用、商业使用
		复制、分发、修改，除了加上作者的版权信息，还必须保留免责声明，免除作者的责任
*/



/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */



// 初始化随机数生成器
XXAPI void xrtRandSeed(xrand* rng, uint64 seed, uint64 seq)
{
	if ( rng == NULL ) {
		xrtSetError("random generator is null.", FALSE);
		return;
	}
	rng->state = 0U;
	rng->inc = (seq << 1u) | 1u;
	// 运算两次
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	rng->state += seed;
	oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
}



// 初始化当前线程默认随机数生成器
XXAPI void xrtRandSeedThread(uint64 seed, uint64 seq)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return;
	}

	xrtRandSeed(&pThreadData->rand32, seed, seq);
	xrtRandSeed(&pThreadData->rand64_low, seed ^ 0x5851f42d4c957f2dULL, seq ^ 0xda3e39cb94b95bdbULL);
	xrtRandSeed(&pThreadData->rand64_high, seed ^ 0x14057b7ef767814fULL, seq ^ 0x853c49e6748fea9bULL);
}



static void __xrtRandSeedObj(xrtRandObj* pRand, uint64 seed, uint64 seq)
{
	if ( pRand == NULL ) {
		return;
	}
	xrtRandSeed(&pRand->rand32, seed, seq);
	xrtRandSeed(&pRand->rand64_low, seed ^ 0x5851f42d4c957f2dULL, seq ^ 0xda3e39cb94b95bdbULL);
	xrtRandSeed(&pRand->rand64_high, seed ^ 0x14057b7ef767814fULL, seq ^ 0x853c49e6748fea9bULL);
}



// 创建独立随机数对象（需使用 xrtRandDestroy 释放）
XXAPI ptr xrtRandCreate(uint64 seed, uint64 seq)
{
	xrtRandObj* pRand = (xrtRandObj*)xrtMalloc(sizeof(xrtRandObj));

	if ( pRand == NULL ) {
		xrtSetError("create random generator failed.", FALSE);
		return NULL;
	}
	__xrtRandSeedObj(pRand, seed, seq);
	return (ptr)pRand;
}



// 释放独立随机数对象
XXAPI int64 xrtRandDestroy(ptr pRand)
{
	xrtFree(pRand);
	return 0;
}



// 重置独立随机数对象种子
XXAPI void xrtRandSeedObj(ptr pRand, uint64 seed, uint64 seq)
{
	if ( pRand == NULL ) {
		xrtSetError("random object is null.", FALSE);
		return;
	}
	__xrtRandSeedObj((xrtRandObj*)pRand, seed, seq);
}



// 生成 32 位随机数 - 线程安全
XXAPI uint32 xrtRand32Ex(xrand* rng)
{
	if ( rng == NULL ) {
		xrtSetError("random generator is null.", FALSE);
		return 0;
	}
	uint64 oldstate = rng->state;
	rng->state = oldstate * 6364136223846793005ULL + rng->inc;
	uint32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32 rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}



// 生成 64 位随机数 - 线程安全
XXAPI uint64 xrtRand64Ex(xrand* rngLow, xrand* rngHigh)
{
	uint32 iLow = xrtRand32Ex(rngLow);
	uint32 iHigh = xrtRand32Ex(rngHigh);
	return ((uint64)iHigh << 32) | (uint64)iLow;
}



// 生成范围随机数 - 线程安全
XXAPI int xrtRandRangeEx(xrand* rng, int min, int max)
{
	if ( rng == NULL ) {
		xrtSetError("random generator is null.", FALSE);
		return 0;
	}
	if ( min <= max ) {
		uint32 iRange = (uint32)(((int64)max - (int64)min) + 1);
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + min;
		}
	} else {
		uint32 iRange = (uint32)(((int64)min - (int64)max) + 1);
		uint32 threshold = -iRange % iRange;
		for (;;) {
			uint32 r = xrtRand32Ex(rng);
			if (r >= threshold)
				return (r % iRange) + max;
		}
	}
	return 0;
}



// 使用独立随机数对象生成 32 位随机数
XXAPI uint32 xrtRand32Obj(ptr pRand)
{
	if ( pRand == NULL ) {
		xrtSetError("random object is null.", FALSE);
		return 0;
	}
	return xrtRand32Ex(&((xrtRandObj*)pRand)->rand32);
}



// 使用独立随机数对象生成 64 位随机数
XXAPI uint64 xrtRand64Obj(ptr pRand)
{
	xrtRandObj* pObj = (xrtRandObj*)pRand;

	if ( pObj == NULL ) {
		xrtSetError("random object is null.", FALSE);
		return 0;
	}
	return xrtRand64Ex(&pObj->rand64_low, &pObj->rand64_high);
}



// 使用独立随机数对象生成范围随机数
XXAPI int xrtRandRangeObj(ptr pRand, int min, int max)
{
	if ( pRand == NULL ) {
		xrtSetError("random object is null.", FALSE);
		return 0;
	}
	return xrtRandRangeEx(&((xrtRandObj*)pRand)->rand32, min, max);
}



// 获取 32 位随机数
XXAPI uint32 xrtRand32()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}

	return xrtRand32Ex(&pThreadData->rand32);
}



// 获取 64 位随机数
XXAPI uint64 xrtRand64()
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}

	return xrtRand64Ex(&pThreadData->rand64_low, &pThreadData->rand64_high);
}



// 获取 32 位范围随机数
XXAPI int xrtRandRange(int min, int max)
{
	xrtThreadData* pThreadData = xrtThreadGetCurrent();

	if ( pThreadData == NULL ) {
		xrtSetError("current thread is not attached to xrt runtime.", FALSE);
		return 0;
	}

	return xrtRandRangeEx(&pThreadData->rand32, min, max);
}



// 整数约等于
XXAPI bool xrtIntApprox(int64 a, int64 b)
{
	if ( a == b ) { return TRUE; }
	
	int64 diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxIntMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxIntTol <= 0.0 ) { return FALSE; }
		int64 maxAbs = (a >= 0 ? a : -a);
		int64 bAbs = (b >= 0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0 ) { return TRUE; }
		double ratio = (double)diff / (double)maxAbs;
		return (ratio <= xCore.fApproxIntTol);
	} else {
		// 差值模式
		return (diff <= (int64)xCore.fApproxIntTol);
	}
}



// 浮点数约等于
XXAPI bool xrtNumApprox(double a, double b)
{
	if ( a == b ) { return TRUE; }
	
	double diff = (a > b) ? (a - b) : (b - a);
	
	if ( xCore.iApproxNumMode == XRT_APPROX_PERCENT ) {
		// 百分比模式
		if ( xCore.fApproxNumTol <= 0.0 ) { return FALSE; }
		double maxAbs = (a >= 0.0 ? a : -a);
		double bAbs = (b >= 0.0 ? b : -b);
		if ( bAbs > maxAbs ) { maxAbs = bAbs; }
		if ( maxAbs == 0.0 ) { return TRUE; }
		double ratio = diff / maxAbs;
		return (ratio <= xCore.fApproxNumTol);
	} else {
		// 差值模式
		return (diff <= xCore.fApproxNumTol);
	}
}



// 返回两个浮点数中的较小值，任一参数为 NaN 时传播 NaN
XXAPI double xrtMathMin(double a, double b)
{
	if ( xrtMathIsNaN(a) ) { return a; }
	if ( xrtMathIsNaN(b) ) { return b; }
	return a < b ? a : b;
}



// 返回两个浮点数中的较大值，任一参数为 NaN 时传播 NaN
XXAPI double xrtMathMax(double a, double b)
{
	if ( xrtMathIsNaN(a) ) { return a; }
	if ( xrtMathIsNaN(b) ) { return b; }
	return a > b ? a : b;
}



// 将数值限制在闭区间内；边界顺序写反时自动归一化
XXAPI double xrtMathClamp(double value, double minValue, double maxValue)
{
	double tmp;
	
	if ( minValue > maxValue ) {
		tmp = minValue;
		minValue = maxValue;
		maxValue = tmp;
	}
	if ( value < minValue ) { return minValue; }
	if ( value > maxValue ) { return maxValue; }
	return value;
}



// 返回符号：负数为 -1，正数为 1，0/NaN 为 0
XXAPI int xrtMathSign(double value)
{
	if ( xrtMathIsNaN(value) || value == 0.0 ) {
		return 0;
	}
	return value > 0.0 ? 1 : -1;
}



// 向 0 截断浮点数
XXAPI double xrtMathTrunc(double value)
{
	if ( !xrtMathIsFinite(value) || value == 0.0 ) {
		return value;
	}
	return value > 0.0 ? floor(value) : ceil(value);
}



// 返回小数部分，语义与 shader/Python 风格 fract 一致：value - floor(value)
XXAPI double xrtMathFract(double value)
{
	return value - floor(value);
}



// 浮点取模，底层采用 C fmod
XXAPI double xrtMathMod(double value, double divisor)
{
	return fmod(value, divisor);
}



// 角度转弧度
XXAPI double xrtMathRad(double degrees)
{
	return degrees * (XRT_MATH_PI / 180.0);
}



// 弧度转角度
XXAPI double xrtMathDeg(double radians)
{
	return radians * (180.0 / XRT_MATH_PI);
}



// 判断 NaN，避免依赖不同 CRT 的非标准符号
XXAPI bool xrtMathIsNaN(double value)
{
	return value != value;
}



// 判断正负无穷，避免依赖不同 CRT 的非标准符号
XXAPI bool xrtMathIsInf(double value)
{
	return value == value && (value - value) != 0.0;
}



// 判断有限数
XXAPI bool xrtMathIsFinite(double value)
{
	return value == value && (value - value) == 0.0;
}



// 以 2 为底的对数
XXAPI double xrtMathLog2(double value)
{
	return log(value) / log(2.0);
}



// 2 的 value 次幂
XXAPI double xrtMathExp2(double value)
{
	return pow(2.0, value);
}



// 高精度 log(1 + value)，小值走级数，普通值交给 C log
XXAPI double xrtMathLog1p(double value)
{
	double av = fabs(value);
	
	if ( av > 0.0 && av < 0.0001 ) {
		double sum = 0.0;
		double term = value;
		int i;
		
		for ( i = 1; i <= 32; ++i ) {
			sum += term / (double)i;
			term *= -value;
		}
		return sum;
	}
	return log(1.0 + value);
}



// 高精度 exp(value) - 1，小值走级数，普通值交给 C exp
XXAPI double xrtMathExpm1(double value)
{
	double av = fabs(value);
	
	if ( av > 0.0 && av < 0.0001 ) {
		double sum = value;
		double term = value;
		int i;
		
		for ( i = 2; i <= 32; ++i ) {
			term *= value / (double)i;
			sum += term;
		}
		return sum;
	}
	return exp(value) - 1.0;
}



// 立方根；旧 CRT 缺少 cbrt 时仍保持可用
XXAPI double xrtMathCbrt(double value)
{
	if ( value == 0.0 ) {
		return value;
	}
	if ( value < 0.0 ) {
		return -pow(-value, 1.0 / 3.0);
	}
	return pow(value, 1.0 / 3.0);
}



// 稳定计算 sqrt(x*x + y*y)，避免普通平方求和更容易溢出
XXAPI double xrtMathHypot(double x, double y)
{
	double ax;
	double ay;
	double r;
	double tmp;
	
	if ( xrtMathIsInf(x) || xrtMathIsInf(y) ) {
		return XRT_MATH_INF;
	}
	if ( xrtMathIsNaN(x) ) { return x; }
	if ( xrtMathIsNaN(y) ) { return y; }
	
	ax = fabs(x);
	ay = fabs(y);
	if ( ax < ay ) {
		tmp = ax;
		ax = ay;
		ay = tmp;
	}
	if ( ax == 0.0 ) {
		return 0.0;
	}
	r = ay / ax;
	return ax * sqrt(1.0 + r * r);
}

