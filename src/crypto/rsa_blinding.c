/*
 * Copyright (c) 2018 Thomas Pornin <pornin@bolet.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../internal/xrt_crypto_rsa.h"

#if defined(XRT_FEATURE_CRYPTO_RSA_PRIVATE)

/* Adapted from BearSSL i31_moddiv.c (2018): keep its fixed-iteration GCD.
 * Local changes: namespace, types, and memcpy bit casts for strict aliasing. */

/*
 * In this file, we handle big integers with a custom format, i.e.
 * without the usual one-word header. Value is split into 31-bit words,
 * each stored in a 32-bit slot (top bit is zero) in little-endian
 * order. The length (in words) is provided explicitly. In some cases,
 * the value can be negative (using two's complement representation). In
 * some cases, the top word is allowed to have a 32th bit.
 */

/*
 * Negate big integer conditionally. The value consists of 'len' words,
 * with 31 bits in each word (the top bit of each word should be 0,
 * except possibly for the last word). If 'ctl' is 1, the negation is
 * computed; otherwise, if 'ctl' is 0, then the value is unchanged.
 */
static void
__xrtRsaDivNegate(uint32 *a, size_t len, uint32 ctl)
{
	size_t k;
	uint32 cc, xm;

	cc = ctl;
	xm = -ctl >> 1;
	for (k = 0; k < len; k ++) {
		uint32 aw;

		aw = a[k];
		aw = (aw ^ xm) + cc;
		a[k] = aw & 0x7FFFFFFF;
		cc = aw >> 31;
	}
}

/*
 * Finish modular reduction. Rules on input parameters:
 *
 *   if neg = 1, then -m <= a < 0
 *   if neg = 0, then 0 <= a < 2*m
 *
 * If neg = 0, then the top word of a[] may use 32 bits.
 *
 * Also, modulus m must be odd.
 */
static void
__xrtRsaDivFinish(uint32 *a, size_t len, const uint32 *m, uint32 neg)
{
	size_t k;
	uint32 cc, xm, ym;

	/*
	 * First pass: compare a (assumed nonnegative) with m.
	 * Note that if the final word uses the top extra bit, then
	 * subtracting m must yield a value less than 2^31, since we
	 * assumed that a < 2*m.
	 */
	cc = 0;
	for (k = 0; k < len; k ++) {
		uint32 aw, mw;

		aw = a[k];
		mw = m[k];
		cc = (aw - mw - cc) >> 31;
	}

	/*
	 * At this point:
	 *   if neg = 1, then we must add m (regardless of cc)
	 *   if neg = 0 and cc = 0, then we must subtract m
	 *   if neg = 0 and cc = 1, then we must do nothing
	 */
	xm = -neg >> 1;
	ym = -(neg | (1 - cc));
	cc = neg;
	for (k = 0; k < len; k ++) {
		uint32 aw, mw;

		aw = a[k];
		mw = (m[k] ^ xm) & ym;
		aw = aw - mw - cc;
		a[k] = aw & 0x7FFFFFFF;
		cc = aw >> 31;
	}
}

/*
 * Compute:
 *   a <- (a*pa+b*pb)/(2^31)
 *   b <- (a*qa+b*qb)/(2^31)
 * The division is assumed to be exact (i.e. the low word is dropped).
 * If the final a is negative, then it is negated. Similarly for b.
 * Returned value is the combination of two bits:
 *   bit 0: 1 if a had to be negated, 0 otherwise
 *   bit 1: 1 if b had to be negated, 0 otherwise
 *
 * Factors pa, pb, qa and qb must be at most 2^31 in absolute value.
 * Source integers a and b must be nonnegative; top word is not allowed
 * to contain an extra 32th bit.
 */
static uint32
__xrtRsaDivReduce(uint32 *a, uint32 *b, size_t len,
	int64 pa, int64 pb, int64 qa, int64 qb)
{
	size_t k;
	int64 cca, ccb;
	uint32 nega, negb;

	cca = 0;
	ccb = 0;
	for (k = 0; k < len; k ++) {
		uint32 wa, wb;
		uint64 za, zb;
		uint64 tta, ttb;

		/*
		 * Since:
		 *   |pa| <= 2^31
		 *   |pb| <= 2^31
		 *   0 <= wa <= 2^31 - 1
		 *   0 <= wb <= 2^31 - 1
		 *   |cca| <= 2^32 - 1
		 * Then:
		 *   |za| <= (2^31-1)*(2^32) + (2^32-1) = 2^63 - 1
		 *
		 * Thus, the new value of cca is such that |cca| <= 2^32 - 1.
		 * The same applies to ccb.
		 */
		wa = a[k];
		wb = b[k];
		za = wa * (uint64)pa + wb * (uint64)pb + (uint64)cca;
		zb = wa * (uint64)qa + wb * (uint64)qb + (uint64)ccb;
		if (k > 0) {
			a[k - 1] = za & 0x7FFFFFFF;
			b[k - 1] = zb & 0x7FFFFFFF;
		}

		/*
		 * For the new values of cca and ccb, we need a signed
		 * right-shift; since, in C, right-shifting a signed
		 * negative value is implementation-defined, we use a
		 * custom portable sign extension expression.
		 */
#define XRT_RSA_DIV_SIGN_BIT   ((uint64)1 << 32)
		tta = za >> 31;
		ttb = zb >> 31;
		tta = (tta ^ XRT_RSA_DIV_SIGN_BIT) - XRT_RSA_DIV_SIGN_BIT;
		ttb = (ttb ^ XRT_RSA_DIV_SIGN_BIT) - XRT_RSA_DIV_SIGN_BIT;
		memcpy(&cca, &tta, sizeof cca);
		memcpy(&ccb, &ttb, sizeof ccb);
#undef XRT_RSA_DIV_SIGN_BIT
	}
	a[len - 1] = (uint32)cca;
	b[len - 1] = (uint32)ccb;

	nega = (uint32)((uint64)cca >> 63);
	negb = (uint32)((uint64)ccb >> 63);
	__xrtRsaDivNegate(a, len, nega);
	__xrtRsaDivNegate(b, len, negb);
	return nega | (negb << 1);
}

/*
 * Compute:
 *   a <- (a*pa+b*pb)/(2^31) mod m
 *   b <- (a*qa+b*qb)/(2^31) mod m
 *
 * m0i is equal to -1/m[0] mod 2^31.
 *
 * Factors pa, pb, qa and qb must be at most 2^31 in absolute value.
 * Source integers a and b must be nonnegative; top word is not allowed
 * to contain an extra 32th bit.
 */
static void
__xrtRsaDivReduceMod(uint32 *a, uint32 *b, size_t len,
	int64 pa, int64 pb, int64 qa, int64 qb,
	const uint32 *m, uint32 m0i)
{
	size_t k;
	int64 cca, ccb;
	uint32 fa, fb;

	cca = 0;
	ccb = 0;
	fa = ((a[0] * (uint32)pa + b[0] * (uint32)pb) * m0i) & 0x7FFFFFFF;
	fb = ((a[0] * (uint32)qa + b[0] * (uint32)qb) * m0i) & 0x7FFFFFFF;
	for (k = 0; k < len; k ++) {
		uint32 wa, wb;
		uint64 za, zb;
		uint64 tta, ttb;

		/*
		 * In this loop, carries 'cca' and 'ccb' always fit on
		 * 33 bits (in absolute value).
		 */
		wa = a[k];
		wb = b[k];
		za = wa * (uint64)pa + wb * (uint64)pb
			+ m[k] * (uint64)fa + (uint64)cca;
		zb = wa * (uint64)qa + wb * (uint64)qb
			+ m[k] * (uint64)fb + (uint64)ccb;
		if (k > 0) {
			a[k - 1] = (uint32)za & 0x7FFFFFFF;
			b[k - 1] = (uint32)zb & 0x7FFFFFFF;
		}

#define XRT_RSA_DIV_SIGN_BIT   ((uint64)1 << 32)
		tta = za >> 31;
		ttb = zb >> 31;
		tta = (tta ^ XRT_RSA_DIV_SIGN_BIT) - XRT_RSA_DIV_SIGN_BIT;
		ttb = (ttb ^ XRT_RSA_DIV_SIGN_BIT) - XRT_RSA_DIV_SIGN_BIT;
		memcpy(&cca, &tta, sizeof cca);
		memcpy(&ccb, &ttb, sizeof ccb);
#undef XRT_RSA_DIV_SIGN_BIT
	}
	a[len - 1] = (uint32)cca;
	b[len - 1] = (uint32)ccb;

	/*
	 * At this point:
	 *   -m <= a < 2*m
	 *   -m <= b < 2*m
	 * (this is a case of Montgomery reduction)
	 * The top word of 'a' and 'b' may have a 32-th bit set.
	 * We may have to add or subtract the modulus.
	 */
	__xrtRsaDivFinish(a, len, m, (uint32)((uint64)cca >> 63));
	__xrtRsaDivFinish(b, len, m, (uint32)((uint64)ccb >> 63));
}

/* see inner.h */
uint32
__xrtRsaModDivide(uint32 *x, const uint32 *y, const uint32 *m, uint32 m0i,
	uint32 *t)
{
	/*
	 * Algorithm is an extended binary GCD. We maintain four values
	 * a, b, u and v, with the following invariants:
	 *
	 *   a * x = y * u mod m
	 *   b * x = y * v mod m
	 *
	 * Starting values are:
	 *
	 *   a = y
	 *   b = m
	 *   u = x
	 *   v = 0
	 *
	 * The formal definition of the algorithm is a sequence of steps:
	 *
	 *   - If a is even, then a <- a/2 and u <- u/2 mod m.
	 *   - Otherwise, if b is even, then b <- b/2 and v <- v/2 mod m.
	 *   - Otherwise, if a > b, then a <- (a-b)/2 and u <- (u-v)/2 mod m.
	 *   - Otherwise, b <- (b-a)/2 and v <- (v-u)/2 mod m.
	 *
	 * Algorithm stops when a = b. At that point, they both are equal
	 * to GCD(y,m); the modular division succeeds if that value is 1.
	 * The result of the modular division is then u (or v: both are
	 * equal at that point).
	 *
	 * Each step makes either a or b shrink by at least one bit; hence,
	 * if m has bit length k bits, then 2k-2 steps are sufficient.
	 *
	 *
	 * Though complexity is quadratic in the size of m, the bit-by-bit
	 * processing is not very efficient. We can speed up processing by
	 * remarking that the decisions are taken based only on observation
	 * of the top and low bits of a and b.
	 *
	 * In the loop below, at each iteration, we use the two top words
	 * of a and b, and the low words of a and b, to compute reduction
	 * parameters pa, pb, qa and qb such that the new values for a
	 * and b are:
	 *
	 *   a' = (a*pa + b*pb) / (2^31)
	 *   b' = (a*qa + b*qb) / (2^31)
	 *
	 * the division being exact.
	 *
	 * Since the choices are based on the top words, they may be slightly
	 * off, requiring an optional correction: if a' < 0, then we replace
	 * pa with -pa, and pb with -pb. The total length of a and b is
	 * thus reduced by at least 30 bits at each iteration.
	 *
	 * The stopping conditions are still the same, though: when a
	 * and b become equal, they must be both odd (since m is odd,
	 * the GCD cannot be even), therefore the next operation is a
	 * subtraction, and one of the values becomes 0. At that point,
	 * nothing else happens, i.e. one value is stuck at 0, and the
	 * other one is the GCD.
	 */
	size_t len, k;
	uint32 *a, *b, *u, *v;
	uint32 num, r;

	len = (m[0] + 31) >> 5;
	a = t;
	b = a + len;
	u = x + 1;
	v = b + len;
	memcpy(a, y + 1, len * sizeof *y);
	memcpy(b, m + 1, len * sizeof *m);
	memset(v, 0, len * sizeof *v);

	/*
	 * Loop below ensures that a and b are reduced by some bits each,
	 * for a total of at least 30 bits.
	 */
	for (num = ((m[0] - (m[0] >> 5)) << 1) + 30; num >= 30; num -= 30) {
		size_t j;
		uint32 c0, c1;
		uint32 a0, a1, b0, b1;
		uint64 a_hi, b_hi;
		uint32 a_lo, b_lo;
		int64 pa, pb, qa, qb;
		int i;

		/*
		 * Extract top words of a and b. If j is the highest
		 * index >= 1 such that a[j] != 0 or b[j] != 0, then we want
		 * (a[j] << 31) + a[j - 1], and (b[j] << 31) + b[j - 1].
		 * If a and b are down to one word each, then we use a[0]
		 * and b[0].
		 */
		c0 = (uint32)-1;
		c1 = (uint32)-1;
		a0 = 0;
		a1 = 0;
		b0 = 0;
		b1 = 0;
		j = len;
		while (j -- > 0) {
			uint32 aw, bw;

			aw = a[j];
			bw = b[j];
			a0 ^= (a0 ^ aw) & c0;
			a1 ^= (a1 ^ aw) & c1;
			b0 ^= (b0 ^ bw) & c0;
			b1 ^= (b1 ^ bw) & c1;
			c1 = c0;
			c0 &= (((aw | bw) + 0x7FFFFFFF) >> 31) - (uint32)1;
		}

		/*
		 * If c1 = 0, then we grabbed two words for a and b.
		 * If c1 != 0 but c0 = 0, then we grabbed one word. It
		 * is not possible that c1 != 0 and c0 != 0, because that
		 * would mean that both integers are zero.
		 */
		a1 |= a0 & c1;
		a0 &= ~c1;
		b1 |= b0 & c1;
		b0 &= ~c1;
		a_hi = ((uint64)a0 << 31) + a1;
		b_hi = ((uint64)b0 << 31) + b1;
		a_lo = a[0];
		b_lo = b[0];

		/*
		 * Compute reduction factors:
		 *
		 *   a' = a*pa + b*pb
		 *   b' = a*qa + b*qb
		 *
		 * such that a' and b' are both multiple of 2^31, but are
		 * only marginally larger than a and b.
		 */
		pa = 1;
		pb = 0;
		qa = 0;
		qb = 1;
		for (i = 0; i < 31; i ++) {
			/*
			 * At each iteration:
			 *
			 *   a <- (a-b)/2 if: a is odd, b is odd, a_hi > b_hi
			 *   b <- (b-a)/2 if: a is odd, b is odd, a_hi <= b_hi
			 *   a <- a/2 if: a is even
			 *   b <- b/2 if: a is odd, b is even
			 *
			 * We multiply a_lo and b_lo by 2 at each
			 * iteration, thus a division by 2 really is a
			 * non-multiplication by 2.
			 */
			uint32 r, oa, ob, cAB, cBA, cA;
			uint64 rz;

			/*
			 * r = GT(a_hi, b_hi)
			 * But the GT() function works on uint32 operands,
			 * so we inline a 64-bit version here.
			 */
			rz = b_hi - a_hi;
			r = (uint32)((rz ^ ((a_hi ^ b_hi)
				& (a_hi ^ rz))) >> 63);

			/*
			 * cAB = 1 if b must be subtracted from a
			 * cBA = 1 if a must be subtracted from b
			 * cA = 1 if a is divided by 2, 0 otherwise
			 *
			 * Rules:
			 *
			 *   cAB and cBA cannot be both 1.
			 *   if a is not divided by 2, b is.
			 */
			oa = (a_lo >> i) & 1;
			ob = (b_lo >> i) & 1;
			cAB = oa & ob & r;
			cBA = oa & ob & __xrtI31Not(r);
			cA = cAB | __xrtI31Not(oa);

			/*
			 * Conditional subtractions.
			 */
			a_lo -= b_lo & -cAB;
			a_hi -= b_hi & -(uint64)cAB;
			pa -= qa & -(int64)cAB;
			pb -= qb & -(int64)cAB;
			b_lo -= a_lo & -cBA;
			b_hi -= a_hi & -(uint64)cBA;
			qa -= pa & -(int64)cBA;
			qb -= pb & -(int64)cBA;

			/*
			 * Shifting.
			 */
			a_lo += a_lo & (cA - 1);
			pa += pa & ((int64)cA - 1);
			pb += pb & ((int64)cA - 1);
			a_hi ^= (a_hi ^ (a_hi >> 1)) & -(uint64)cA;
			b_lo += b_lo & -cA;
			qa += qa & -(int64)cA;
			qb += qb & -(int64)cA;
			b_hi ^= (b_hi ^ (b_hi >> 1)) & ((uint64)cA - 1);
		}

		/*
		 * Replace a and b with new values a' and b'.
		 */
		r = __xrtRsaDivReduce(a, b, len, pa, pb, qa, qb);
		pa -= pa * ((r & 1) << 1);
		pb -= pb * ((r & 1) << 1);
		qa -= qa * (r & 2);
		qb -= qb * (r & 2);
		__xrtRsaDivReduceMod(u, v, len, pa, pb, qa, qb, m + 1, m0i);
	}

	/*
	 * Now one of the arrays should be 0, and the other contains
	 * the GCD. If a is 0, then u is 0 as well, and v contains
	 * the division result.
	 * Result is correct if and only if GCD is 1.
	 */
	r = (a[0] | b[0]) ^ 1;
	u[0] |= v[0];
	for (k = 1; k < len; k ++) {
		r |= a[k] | b[k];
		u[k] |= v[k];
	}
	return __xrtI31Equal(r, 0u);
}

/* 每次私钥运算使用全新的 r：先算 m*r^e，再私钥幂，最后乘 r^-1。
   公钥复核覆盖解除盲化结果；失败不发布输出，也不回退到未盲化运算。 */
__xrt_rsa_result __xrtRsaPrivatePower(
	const xrsaprivatekey* pKey,
	const void* pInput,
	size_t iInputSize,
	void* pOutput
)
{
	uint32 Work[8u * XRT_RSA_MAX_I31_WORDS] = { 0 };
	uint8 Encoded[XRT_RSA_MAX_MODULUS_SIZE];
	uint32* pModulus = Work;
	uint32* pRandom = pModulus + XRT_RSA_MAX_I31_WORDS;
	uint32* pInverse = pRandom + XRT_RSA_MAX_I31_WORDS;
	uint32* pSquare = pInverse + XRT_RSA_MAX_I31_WORDS;
	uint32* pValue = pSquare + XRT_RSA_MAX_I31_WORDS;
	uint32* pTemp = pValue + XRT_RSA_MAX_I31_WORDS;
	size_t iWords;
	uint32 iModulusInverse;
	uint32 iBits;
	bool bReady = false;
	__xrt_rsa_result Result = XRT_RSA_RESULT_KEY;

	if ( (pKey == NULL) || (pInput == NULL) || (pOutput == NULL) ) {
		return XRT_RSA_RESULT_ARGUMENT;
	}
	if ( !__xrtRsaKeyValid(&pKey->Public) ||
		(iInputSize != pKey->Public.ModulusSize) ) {
		goto cleanup;
	}
	__xrtI31Decode(pModulus, pKey->Public.Modulus, iInputSize);
	iWords = (pModulus[0] + 31u) >> 5u;
	if ( (iWords + 1u) > XRT_RSA_MAX_I31_WORDS ) {
		goto cleanup;
	}
	if ( !__xrtI31DecodeMod(pValue, pInput, iInputSize, pModulus) ) {
		Result = XRT_RSA_RESULT_INPUT;
		goto cleanup;
	}
	iBits = pModulus[0] - (pModulus[0] >> 5u);
	iModulusInverse = __xrtI31NegativeInverse(pModulus[1]);
	for ( size_t i = 0; i < 64u; i++ ) {
		uint32 iNotOne;

		if ( !xrtSecureRandom(Encoded, iInputSize) ) {
			Result = XRT_RSA_RESULT_RANDOM;
			goto cleanup;
		}
		Encoded[0] &= (uint8)(0xFFu >> ((0u - iBits) & 7u));
		if ( !__xrtI31DecodeMod(pRandom, Encoded, iInputSize, pModulus) ) {
			continue;
		}
		iNotOne = pRandom[1] ^ 1u;
		for ( size_t j = 2u; j <= iWords; j++ ) {
			iNotOne |= pRandom[j];
		}
		if ( __xrtI31IsZero(pRandom) || (iNotOne == 0) ) {
			continue;
		}
		__xrtI31Zero(pInverse, pModulus[0]);
		pInverse[1] = 1u;
		if ( __xrtRsaModDivide(pInverse, pRandom, pModulus,
			iModulusInverse, pTemp) ) {
			bReady = true;
			break;
		}
	}
	if ( !bReady ) {
		__xrtRsaError("crypto.rsa.blind",
			"secure random did not yield an invertible RSA blinding factor",
			XCRYPTO_ERROR_KEY);
		Result = XRT_RSA_RESULT_RANDOM;
		goto cleanup;
	}
	__xrtI31MontgomerySquare(pSquare, pModulus);
	__xrtI31ModPower(pRandom, (const uint8*)pKey->Public.Exponent,
		pKey->Public.ExponentSize, pModulus, pSquare, iModulusInverse,
		pTemp, pTemp + XRT_RSA_MAX_I31_WORDS);
	__xrtI31MontgomeryMultiply(pTemp, pValue, pSquare,
		pModulus, iModulusInverse);
	__xrtI31MontgomeryMultiply(pValue, pTemp, pRandom,
		pModulus, iModulusInverse);
	__xrtI31Encode(Encoded, iInputSize, pValue);
	Result = __xrtRsaPrivateCore(pKey, Encoded, iInputSize, Encoded);
	if ( Result != XRT_RSA_RESULT_OK ) {
		goto cleanup;
	}
	if ( !__xrtI31DecodeMod(pValue, Encoded, iInputSize, pModulus) ) {
		Result = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	__xrtI31MontgomeryMultiply(pTemp, pValue, pSquare,
		pModulus, iModulusInverse);
	__xrtI31MontgomeryMultiply(pValue, pTemp, pInverse,
		pModulus, iModulusInverse);
	__xrtI31Encode(Encoded, iInputSize, pValue);
	/* 临时整数区此后不再参与模运算，可复用为公钥复核的字节输出。 */
	Result = __xrtRsaPower(&pKey->Public, Encoded, iInputSize, pTemp);
	if ( (Result != XRT_RSA_RESULT_OK) ||
		!xrtConstTimeEqual(pTemp, pInput, iInputSize) ) {
		Result = XRT_RSA_RESULT_KEY;
		goto cleanup;
	}
	memcpy(pOutput, Encoded, iInputSize);

cleanup:
	xrtSecureZero(Work, sizeof(Work));
	xrtSecureZero(Encoded, sizeof(Encoded));
	return Result;
}

#endif
