/*
 * Copyright (c) 2016 Thomas Pornin <pornin@bolet.org>
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

#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_NIST)

#define NOT(value) __xrtI31Not(value)
#define MUX(control, left, right) __xrtI31Select(control, left, right)
#define EQ(left, right) __xrtI31Equal(left, right)
#define NEQ(left, right) __xrtI31NotEqual(left, right)
#define CCOPY(control, destination, source, size) \
	__xrtI31Copy(control, destination, source, size)

#define br_i31_add __xrtI31Add
#define br_i31_sub __xrtI31Subtract
#define br_i31_iszero __xrtI31IsZero
#define br_i31_decode_mod __xrtI31DecodeMod
#define br_i31_encode __xrtI31Encode
#define br_i31_montymul __xrtI31MontgomeryMultiply

#define BR_EC_secp256r1 XRT_NIST_P256
#define BR_EC_secp384r1 XRT_NIST_P384
#define BR_MAX_EC_SIZE 384

/*
 * Parameters for supported curves (field modulus, and 'b' equation
 * parameter; both values use the 'i31' format, and 'b' is in Montgomery
 * representation).
 */

static const uint32_t P256_P[] = {
	0x00000108,
	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x00000007,
	0x00000000, 0x00000000, 0x00000040, 0x7FFFFF80,
	0x000000FF
};

static const uint32_t P256_R2[] = {
	0x00000108,
	0x00014000, 0x00018000, 0x00000000, 0x7FF40000,
	0x7FEFFFFF, 0x7FF7FFFF, 0x7FAFFFFF, 0x005FFFFF,
	0x00000000
};

static const uint32_t P256_B[] = {
	0x00000108,
	0x6FEE1803, 0x6229C4BD, 0x21B139BE, 0x327150AA,
	0x3567802E, 0x3F7212ED, 0x012E4355, 0x782DD38D,
	0x0000000E
};

static const uint32_t P384_P[] = {
	0x0000018C,
	0x7FFFFFFF, 0x00000001, 0x00000000, 0x7FFFFFF8,
	0x7FFFFFEF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
	0x00000FFF
};

static const uint32_t P384_R2[] = {
	0x0000018C,
	0x00000000, 0x00000080, 0x7FFFFE00, 0x000001FF,
	0x00000800, 0x00000000, 0x7FFFE000, 0x00001FFF,
	0x00008000, 0x00008000, 0x00000000, 0x00000000,
	0x00000000
};

static const uint32_t P384_B[] = {
	0x0000018C,
	0x6E666840, 0x070D0392, 0x5D810231, 0x7651D50C,
	0x17E218D6, 0x1B192002, 0x44EFE441, 0x3A524E2B,
	0x2719BA5F, 0x41F02209, 0x36C5643E, 0x5813EFFE,
	0x000008A5
};

typedef struct {
	const uint32_t *p;
	const uint32_t *b;
	const uint32_t *R2;
	uint32_t p0i;
	size_t point_len;
} curve_params;



/* 把内部曲线编号映射为定长域参数。 */
static inline const curve_params *
id_to_curve(int curve)
{
	static const curve_params pp[] = {
		{ P256_P, P256_B, P256_R2, 0x00000001,  65 },
		{ P384_P, P384_B, P384_R2, 0x00000001,  97 }
	};

	return &pp[curve];
}

#define I31_LEN   ((BR_MAX_EC_SIZE + 61) / 31)

/*
 * Type for a point in Jacobian coordinates:
 * -- three values, x, y and z, in Montgomery representation
 * -- affine coordinates are X = x / z^2 and Y = y / z^3
 * -- for the point at infinity, z = 0
 */
typedef struct {
	uint32_t c[3][I31_LEN];
} jacobian;

/*
 * We use a custom interpreter that uses a dozen registers, and
 * only six operations:
 *    MSET(d, a)       copy a into d
 *    MADD(d, a)       d = d+a (modular)
 *    MSUB(d, a)       d = d-a (modular)
 *    MMUL(d, a, b)    d = a*b (Montgomery multiplication)
 *    MINV(d, a, b)    invert d modulo p; a and b are used as scratch registers
 *    MTZ(d)           clear return value if d = 0
 * Destination of MMUL (d) must be distinct from operands (a and b).
 * There is no such constraint for MSUB and MADD.
 *
 * Registers include the operand coordinates, and temporaries.
 */
#define MSET(d, a)      (0x0000 + ((d) << 8) + ((a) << 4))
#define MADD(d, a)      (0x1000 + ((d) << 8) + ((a) << 4))
#define MSUB(d, a)      (0x2000 + ((d) << 8) + ((a) << 4))
#define MMUL(d, a, b)   (0x3000 + ((d) << 8) + ((a) << 4) + (b))
#define MINV(d, a, b)   (0x4000 + ((d) << 8) + ((a) << 4) + (b))
#define MTZ(d)          (0x5000 + ((d) << 8))
#define ENDCODE         0

/*
 * Registers for the input operands.
 */
#define P1x    0
#define P1y    1
#define P1z    2
#define P2x    3
#define P2y    4
#define P2z    5

/*
 * Alternate names for the first input operand.
 */
#define Px     0
#define Py     1
#define Pz     2

/*
 * Temporaries.
 */
#define t1     6
#define t2     7
#define t3     8
#define t4     9
#define t5    10
#define t6    11
#define t7    12

/*
 * Extra scratch registers available when there is no second operand (e.g.
 * for "double" and "affine").
 */
#define t8     3
#define t9     4
#define t10    5

/*
 * Doubling formulas are:
 *
 *   s = 4*x*y^2
 *   m = 3*(x + z^2)*(x - z^2)
 *   x' = m^2 - 2*s
 *   y' = m*(s - x') - 8*y^4
 *   z' = 2*y*z
 *
 * If y = 0 (P has order 2) then this yields infinity (z' = 0), as it
 * should. This case should not happen anyway, because our curves have
 * prime order, and thus do not contain any point of order 2.
 *
 * If P is infinity (z = 0), then again the formulas yield infinity,
 * which is correct. Thus, this code works for all points.
 *
 * Cost: 8 multiplications
 */
static const uint16_t code_double[] = {
	/*
	 * Compute z^2 (in t1).
	 */
	MMUL(t1, Pz, Pz),

	/*
	 * Compute x-z^2 (in t2) and then x+z^2 (in t1).
	 */
	MSET(t2, Px),
	MSUB(t2, t1),
	MADD(t1, Px),

	/*
	 * Compute m = 3*(x+z^2)*(x-z^2) (in t1).
	 */
	MMUL(t3, t1, t2),
	MSET(t1, t3),
	MADD(t1, t3),
	MADD(t1, t3),

	/*
	 * Compute s = 4*x*y^2 (in t2) and 2*y^2 (in t3).
	 */
	MMUL(t3, Py, Py),
	MADD(t3, t3),
	MMUL(t2, Px, t3),
	MADD(t2, t2),

	/*
	 * Compute x' = m^2 - 2*s.
	 */
	MMUL(Px, t1, t1),
	MSUB(Px, t2),
	MSUB(Px, t2),

	/*
	 * Compute z' = 2*y*z.
	 */
	MMUL(t4, Py, Pz),
	MSET(Pz, t4),
	MADD(Pz, t4),

	/*
	 * Compute y' = m*(s - x') - 8*y^4. Note that we already have
	 * 2*y^2 in t3.
	 */
	MSUB(t2, Px),
	MMUL(Py, t1, t2),
	MMUL(t4, t3, t3),
	MSUB(Py, t4),
	MSUB(Py, t4),

	ENDCODE
};

/*
 * Addtions formulas are:
 *
 *   u1 = x1 * z2^2
 *   u2 = x2 * z1^2
 *   s1 = y1 * z2^3
 *   s2 = y2 * z1^3
 *   h = u2 - u1
 *   r = s2 - s1
 *   x3 = r^2 - h^3 - 2 * u1 * h^2
 *   y3 = r * (u1 * h^2 - x3) - s1 * h^3
 *   z3 = h * z1 * z2
 *
 * If both P1 and P2 are infinity, then z1 == 0 and z2 == 0, implying that
 * z3 == 0, so the result is correct.
 * If either of P1 or P2 is infinity, but not both, then z3 == 0, which is
 * not correct.
 * h == 0 only if u1 == u2; this happens in two cases:
 * -- if s1 == s2 then P1 and/or P2 is infinity, or P1 == P2
 * -- if s1 != s2 then P1 + P2 == infinity (but neither P1 or P2 is infinity)
 *
 * Thus, the following situations are not handled correctly:
 * -- P1 = 0 and P2 != 0
 * -- P1 != 0 and P2 = 0
 * -- P1 = P2
 * All other cases are properly computed. However, even in "incorrect"
 * situations, the three coordinates still are properly formed field
 * elements.
 *
 * The returned flag is cleared if r == 0. This happens in the following
 * cases:
 * -- Both points are on the same horizontal line (same Y coordinate).
 * -- Both points are infinity.
 * -- One point is infinity and the other is on line Y = 0.
 * The third case cannot happen with our curves (there is no valid point
 * on line Y = 0 since that would be a point of order 2). If the two
 * source points are non-infinity, then remains only the case where the
 * two points are on the same horizontal line.
 *
 * This allows us to detect the "P1 == P2" case, assuming that P1 != 0 and
 * P2 != 0:
 * -- If the returned value is not the point at infinity, then it was properly
 * computed.
 * -- Otherwise, if the returned flag is 1, then P1+P2 = 0, and the result
 * is indeed the point at infinity.
 * -- Otherwise (result is infinity, flag is 0), then P1 = P2 and we should
 * use the 'double' code.
 *
 * Cost: 16 multiplications
 */
static const uint16_t code_add[] = {
	/*
	 * Compute u1 = x1*z2^2 (in t1) and s1 = y1*z2^3 (in t3).
	 */
	MMUL(t3, P2z, P2z),
	MMUL(t1, P1x, t3),
	MMUL(t4, P2z, t3),
	MMUL(t3, P1y, t4),

	/*
	 * Compute u2 = x2*z1^2 (in t2) and s2 = y2*z1^3 (in t4).
	 */
	MMUL(t4, P1z, P1z),
	MMUL(t2, P2x, t4),
	MMUL(t5, P1z, t4),
	MMUL(t4, P2y, t5),

	/*
	 * Compute h = u2 - u1 (in t2) and r = s2 - s1 (in t4).
	 */
	MSUB(t2, t1),
	MSUB(t4, t3),

	/*
	 * Report cases where r = 0 through the returned flag.
	 */
	MTZ(t4),

	/*
	 * Compute u1*h^2 (in t6) and h^3 (in t5).
	 */
	MMUL(t7, t2, t2),
	MMUL(t6, t1, t7),
	MMUL(t5, t7, t2),

	/*
	 * Compute x3 = r^2 - h^3 - 2*u1*h^2.
	 * t1 and t7 can be used as scratch registers.
	 */
	MMUL(P1x, t4, t4),
	MSUB(P1x, t5),
	MSUB(P1x, t6),
	MSUB(P1x, t6),

	/*
	 * Compute y3 = r*(u1*h^2 - x3) - s1*h^3.
	 */
	MSUB(t6, P1x),
	MMUL(P1y, t4, t6),
	MMUL(t1, t5, t3),
	MSUB(P1y, t1),

	/*
	 * Compute z3 = h*z1*z2.
	 */
	MMUL(t1, P1z, P2z),
	MMUL(P1z, t1, t2),

	ENDCODE
};

/*
 * Check that the point is on the curve. This code snippet assumes the
 * following conventions:
 * -- Coordinates x and y have been freshly decoded in P1 (but not
 * converted to Montgomery coordinates yet).
 * -- P2x, P2y and P2z are set to, respectively, R^2, b*R and 1.
 */
static const uint16_t code_check[] = {

	/* Convert x and y to Montgomery representation. */
	MMUL(t1, P1x, P2x),
	MMUL(t2, P1y, P2x),
	MSET(P1x, t1),
	MSET(P1y, t2),

	/* Compute x^3 in t1. */
	MMUL(t2, P1x, P1x),
	MMUL(t1, P1x, t2),

	/* Subtract 3*x from t1. */
	MSUB(t1, P1x),
	MSUB(t1, P1x),
	MSUB(t1, P1x),

	/* Add b. */
	MADD(t1, P2y),

	/* Compute y^2 in t2. */
	MMUL(t2, P1y, P1y),

	/* Compare y^2 with x^3 - 3*x + b; they must match. */
	MSUB(t1, t2),
	MTZ(t1),

	/* Set z to 1 (in Montgomery representation). */
	MMUL(P1z, P2x, P2z),

	ENDCODE
};

/*
 * Conversion back to affine coordinates. This code snippet assumes that
 * the z coordinate of P2 is set to 1 (not in Montgomery representation).
 */
static const uint16_t code_affine[] = {

	/* Save z*R in t1. */
	MSET(t1, P1z),

	/* Compute z^3 in t2. */
	MMUL(t2, P1z, P1z),
	MMUL(t3, P1z, t2),
	MMUL(t2, t3, P2z),

	/* Invert to (1/z^3) in t2. */
	MINV(t2, t3, t4),

	/* Compute y. */
	MSET(t3, P1y),
	MMUL(P1y, t2, t3),

	/* Compute (1/z^2) in t3. */
	MMUL(t3, t2, t1),

	/* Compute x. */
	MSET(t2, P1x),
	MMUL(P1x, t2, t3),

	ENDCODE
};



/* 解释执行固定点公式，并清除包含私钥派生点的工作寄存器。 */
static uint32_t
run_code(jacobian *P1, const jacobian *P2,
	const curve_params *cc, const uint16_t *code)
{
	uint32_t r;
	uint32_t t[13][I31_LEN];
	size_t u;

	r = 1;

	/*
	 * Copy the two operands in the dedicated registers.
	 */
	memcpy(t[P1x], P1->c, 3 * I31_LEN * sizeof(uint32_t));
	memcpy(t[P2x], P2->c, 3 * I31_LEN * sizeof(uint32_t));

	/*
	 * Run formulas.
	 */
	for (u = 0;; u ++) {
		unsigned op, d, a, b;

		op = code[u];
		if (op == 0) {
			break;
		}
		d = (op >> 8) & 0x0F;
		a = (op >> 4) & 0x0F;
		b = op & 0x0F;
		op >>= 12;
		switch (op) {
			uint32_t ctl;
			size_t plen;
			unsigned char tp[(BR_MAX_EC_SIZE + 7) >> 3];

		case 0:
			memcpy(t[d], t[a], I31_LEN * sizeof(uint32_t));
			break;
		case 1:
			ctl = br_i31_add(t[d], t[a], 1);
			ctl |= NOT(br_i31_sub(t[d], cc->p, 0));
			br_i31_sub(t[d], cc->p, ctl);
			break;
		case 2:
			br_i31_add(t[d], cc->p, br_i31_sub(t[d], t[a], 1));
			break;
		case 3:
			br_i31_montymul(t[d], t[a], t[b], cc->p, cc->p0i);
			break;
		case 4:
			plen = (cc->p[0] - (cc->p[0] >> 5) + 7) >> 3;
			br_i31_encode(tp, plen, cc->p);
			tp[plen - 1] -= 2;
			__xrtI31ModPower(
				t[d],
				tp,
				plen,
				cc->p,
				cc->R2,
				cc->p0i,
				t[a],
				t[b]
			);
			break;
		default:
			r &= ~br_i31_iszero(t[d]);
			break;
		}
	}

	/*
	 * Copy back result.
	 */
	memcpy(P1->c, t[P1x], 3 * I31_LEN * sizeof(uint32_t));
	xrtSecureZero(t, sizeof(t));
	return r;
}



/* 按模数长度构造普通表示的一。 */
static void
set_one(uint32_t *x, const uint32_t *p)
{
	size_t plen;

	plen = (p[0] + 63) >> 5;
	memset(x, 0, plen * sizeof *x);
	x[0] = p[0];
	x[1] = 0x00000001;
}



/* 构造 Jacobian 无穷远点。 */
static void
point_zero(jacobian *P, const curve_params *cc)
{
	memset(P, 0, sizeof *P);
	P->c[0][0] = P->c[1][0] = P->c[2][0] = cc->p[0];
}



/* 使用固定公式完成 Jacobian 点倍乘。 */
static inline void
point_double(jacobian *P, const curve_params *cc)
{
	run_code(P, P, cc, code_double);
}



/* 使用固定公式完成两个 Jacobian 点相加。 */
static inline uint32_t
point_add(jacobian *P1, const jacobian *P2, const curve_params *cc)
{
	return run_code(P1, P2, cc, code_add);
}



/* 用两位窗口固定轨迹执行标量乘法。 */
static void
point_mul(jacobian *P, const unsigned char *x, size_t xlen,
	const curve_params *cc)
{
	/*
	 * We do a simple double-and-add ladder with a 2-bit window
	 * to make only one add every two doublings. We thus first
	 * precompute 2P and 3P in some local buffers.
	 *
	 * We always perform two doublings and one addition; the
	 * addition is with P, 2P and 3P and is done in a temporary
	 * array.
	 *
	 * The addition code cannot handle cases where one of the
	 * operands is infinity, which is the case at the start of the
	 * ladder. We therefore need to maintain a flag that controls
	 * this situation.
	 */
	uint32_t qz;
	jacobian P2, P3, Q, T, U;

	memcpy(&P2, P, sizeof P2);
	point_double(&P2, cc);
	memcpy(&P3, P, sizeof P3);
	point_add(&P3, &P2, cc);

	point_zero(&Q, cc);
	qz = 1;
	while (xlen -- > 0) {
		int k;

		for (k = 6; k >= 0; k -= 2) {
			uint32_t bits;
			uint32_t bnz;

			point_double(&Q, cc);
			point_double(&Q, cc);
			memcpy(&T, P, sizeof T);
			memcpy(&U, &Q, sizeof U);
			bits = (*x >> k) & (uint32_t)3;
			bnz = NEQ(bits, 0);
			CCOPY(EQ(bits, 2), &T, &P2, sizeof T);
			CCOPY(EQ(bits, 3), &T, &P3, sizeof T);
			point_add(&U, &T, cc);
			CCOPY(bnz & qz, &Q, &T, sizeof Q);
			CCOPY(bnz & ~qz, &Q, &U, sizeof Q);
			qz &= ~bnz;
		}
		x ++;
	}
	memcpy(P, &Q, sizeof Q);
	xrtSecureZero(&P2, sizeof(P2));
	xrtSecureZero(&P3, sizeof(P3));
	xrtSecureZero(&Q, sizeof(Q));
	xrtSecureZero(&T, sizeof(T));
	xrtSecureZero(&U, sizeof(U));
}

/*
 * Decode point into Jacobian coordinates. This function does not support
 * the point at infinity. If the point is invalid then this returns 0, but
 * the coordinates are still set to properly formed field elements.
 */
/* 解码并完整验证未压缩 SEC 1 公共点。 */
static uint32_t
point_decode(jacobian *P, const void *src, size_t len, const curve_params *cc)
{
	/*
	 * Points must use uncompressed format:
	 * -- first byte is 0x04;
	 * -- coordinates X and Y use unsigned big-endian, with the same
	 *    length as the field modulus.
	 *
	 * We don't support hybrid format (uncompressed, but first byte
	 * has value 0x06 or 0x07, depending on the least significant bit
	 * of Y) because it is rather useless, and explicitly forbidden
	 * by PKIX (RFC 5480, section 2.2).
	 *
	 * We don't support compressed format either, because it is not
	 * much used in practice (there are or were patent-related
	 * concerns about point compression, which explains the lack of
	 * generalised support). Also, point compression support would
	 * need a bit more code.
	 */
	const unsigned char *buf;
	size_t plen, zlen;
	uint32_t r;
	jacobian Q;

	buf = src;
	point_zero(P, cc);
	plen = (cc->p[0] - (cc->p[0] >> 5) + 7) >> 3;
	if (len != 1 + (plen << 1)) {
		return 0;
	}
	r = br_i31_decode_mod(P->c[0], buf + 1, plen, cc->p);
	r &= br_i31_decode_mod(P->c[1], buf + 1 + plen, plen, cc->p);

	/*
	 * Check first byte.
	 */
	r &= EQ(buf[0], 0x04);
	/* obsolete
	r &= EQ(buf[0], 0x04) | (EQ(buf[0] & 0xFE, 0x06)
		& ~(uint32_t)(buf[0] ^ buf[plen << 1]));
	*/

	/*
	 * Convert coordinates and check that the point is valid.
	 */
	zlen = ((cc->p[0] + 63) >> 5) * sizeof(uint32_t);
	memcpy(Q.c[0], cc->R2, zlen);
	memcpy(Q.c[1], cc->b, zlen);
	set_one(Q.c[2], cc->p);
	r &= ~run_code(P, &Q, cc, code_check);
	return r;
}

/*
 * Encode a point. This method assumes that the point is correct and is
 * not the point at infinity. Encoded size is always 1+2*plen, where
 * plen is the field modulus length, in bytes.
 */
/* 把有效非无穷远 Jacobian 点编码为未压缩 SEC 1 公共点。 */
static void
point_encode(void *dst, const jacobian *P, const curve_params *cc)
{
	unsigned char *buf;
	uint32_t xbl;
	size_t plen;
	jacobian Q, T;

	buf = dst;
	xbl = cc->p[0];
	xbl -= (xbl >> 5);
	plen = (xbl + 7) >> 3;
	buf[0] = 0x04;
	memcpy(&Q, P, sizeof *P);
	set_one(T.c[2], cc->p);
	run_code(&Q, &T, cc, code_affine);
	br_i31_encode(buf + 1, plen, Q.c[0]);
	br_i31_encode(buf + 1 + plen, plen, Q.c[1]);
	xrtSecureZero(&Q, sizeof(Q));
	xrtSecureZero(&T, sizeof(T));
}

static const uint8 P256_G[] = {
	0x04, 0x6B, 0x17, 0xD1, 0xF2, 0xE1, 0x2C, 0x42,
	0x47, 0xF8, 0xBC, 0xE6, 0xE5, 0x63, 0xA4, 0x40,
	0xF2, 0x77, 0x03, 0x7D, 0x81, 0x2D, 0xEB, 0x33,
	0xA0, 0xF4, 0xA1, 0x39, 0x45, 0xD8, 0x98, 0xC2,
	0x96, 0x4F, 0xE3, 0x42, 0xE2, 0xFE, 0x1A, 0x7F,
	0x9B, 0x8E, 0xE7, 0xEB, 0x4A, 0x7C, 0x0F, 0x9E,
	0x16, 0x2B, 0xCE, 0x33, 0x57, 0x6B, 0x31, 0x5E,
	0xCE, 0xCB, 0xB6, 0x40, 0x68, 0x37, 0xBF, 0x51,
	0xF5
};

static const uint8 P256_N[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xBC, 0xE6, 0xFA, 0xAD, 0xA7, 0x17, 0x9E, 0x84,
	0xF3, 0xB9, 0xCA, 0xC2, 0xFC, 0x63, 0x25, 0x51
};

static const uint8 P384_G[] = {
	0x04, 0xAA, 0x87, 0xCA, 0x22, 0xBE, 0x8B, 0x05,
	0x37, 0x8E, 0xB1, 0xC7, 0x1E, 0xF3, 0x20, 0xAD,
	0x74, 0x6E, 0x1D, 0x3B, 0x62, 0x8B, 0xA7, 0x9B,
	0x98, 0x59, 0xF7, 0x41, 0xE0, 0x82, 0x54, 0x2A,
	0x38, 0x55, 0x02, 0xF2, 0x5D, 0xBF, 0x55, 0x29,
	0x6C, 0x3A, 0x54, 0x5E, 0x38, 0x72, 0x76, 0x0A,
	0xB7, 0x36, 0x17, 0xDE, 0x4A, 0x96, 0x26, 0x2C,
	0x6F, 0x5D, 0x9E, 0x98, 0xBF, 0x92, 0x92, 0xDC,
	0x29, 0xF8, 0xF4, 0x1D, 0xBD, 0x28, 0x9A, 0x14,
	0x7C, 0xE9, 0xDA, 0x31, 0x13, 0xB5, 0xF0, 0xB8,
	0xC0, 0x0A, 0x60, 0xB1, 0xCE, 0x1D, 0x7E, 0x81,
	0x9D, 0x7A, 0x43, 0x1D, 0x7C, 0x90, 0xEA, 0x0E,
	0x5F
};

static const uint8 P384_N[] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xC7, 0x63, 0x4D, 0x81, 0xF4, 0x37, 0x2D, 0xDF,
	0x58, 0x1A, 0x0D, 0xB2, 0x48, 0xB0, 0xA7, 0x7A,
	0xEC, 0xEC, 0x19, 0x6A, 0xCC, 0xC5, 0x29, 0x73
};



/* 返回固定曲线的群阶及其大端字节长度。 */
const uint8* __xrtNistOrder(int iCurve, size_t* pSize)
{
	if ( iCurve == XRT_NIST_P256 ) {
		if ( pSize != NULL ) {
			*pSize = sizeof(P256_N);
		}
		return P256_N;
	}
	if ( iCurve == XRT_NIST_P384 ) {
		if ( pSize != NULL ) {
			*pSize = sizeof(P384_N);
		}
		return P384_N;
	}
	if ( pSize != NULL ) {
		*pSize = 0;
	}
	return NULL;
}



/* 返回曲线生成元及其 SEC 1 编码长度。 */
static const uint8* api_generator(int curve, size_t* len)
{
	if ( curve == XRT_NIST_P256 ) {
		*len = sizeof(P256_G);
		return P256_G;
	}
	if ( curve == XRT_NIST_P384 ) {
		*len = sizeof(P384_G);
		return P384_G;
	}
	*len = 0;
	return NULL;
}



/* 原位计算一个 SEC 1 公共点的标量乘积。 */
static uint32_t
api_mul(unsigned char *G, size_t Glen,
	const unsigned char *x, size_t xlen, int curve)
{
	uint32_t r;
	const curve_params *cc;
	jacobian P;

	cc = id_to_curve(curve);
	if (Glen != cc->point_len) {
		return 0;
	}
	r = point_decode(&P, G, Glen, cc);
	point_mul(&P, x, xlen, cc);
	point_encode(G, &P, cc);
	xrtSecureZero(&P, sizeof(P));
	return r;
}



/* 复制生成元并计算固定基点标量乘法。 */
static size_t
api_mulgen(unsigned char *R,
	const unsigned char *x, size_t xlen, int curve)
{
	const unsigned char *G;
	size_t Glen;

	G = api_generator(curve, &Glen);
	memcpy(R, G, Glen);
	api_mul(R, Glen, x, xlen, curve);
	return Glen;
}



/* 计算两个公共点的双标量线性组合。 */
static uint32_t
api_muladd(unsigned char *A, const unsigned char *B, size_t len,
	const unsigned char *x, size_t xlen,
	const unsigned char *y, size_t ylen, int curve)
{
	uint32_t r, t, z;
	const curve_params *cc;
	jacobian P, Q;

	/*
	 * TODO: see about merging the two ladders. Right now, we do
	 * two independent point multiplications, which is a bit
	 * wasteful of CPU resources (but yields short code).
	 */

	cc = id_to_curve(curve);
	if (len != cc->point_len) {
		return 0;
	}
	r = point_decode(&P, A, len, cc);
	if (B == NULL) {
		size_t Glen;

		B = api_generator(curve, &Glen);
	}
	r &= point_decode(&Q, B, len, cc);
	point_mul(&P, x, xlen, cc);
	point_mul(&Q, y, ylen, cc);

	/*
	 * We want to compute P+Q. Since the base points A and B are distinct
	 * from infinity, and the multipliers are non-zero and lower than the
	 * curve order, then we know that P and Q are non-infinity. This
	 * leaves two special situations to test for:
	 * -- If P = Q then we must use point_double().
	 * -- If P+Q = 0 then we must report an error.
	 */
	t = point_add(&P, &Q, cc);
	point_double(&Q, cc);
	z = br_i31_iszero(P.c[2]);

	/*
	 * If z is 1 then either P+Q = 0 (t = 1) or P = Q (t = 0). So we
	 * have the following:
	 *
	 *   z = 0, t = 0   return P (normal addition)
	 *   z = 0, t = 1   return P (normal addition)
	 *   z = 1, t = 0   return Q (a 'double' case)
	 *   z = 1, t = 1   report an error (P+Q = 0)
	 */
	CCOPY(z & ~t, &P, &Q, sizeof Q);
	point_encode(A, &P, cc);
	r &= ~(z & t);
	xrtSecureZero(&P, sizeof(P));
	xrtSecureZero(&Q, sizeof(Q));

	return r;
}

/* 验证固定曲线的未压缩 SEC 1 公共点。 */
uint32 __xrtNistPointValid(int iCurve, const void* pPoint, size_t iPointSize)
{
	jacobian Point;
	uint32 iValid;

	if ( (iCurve < XRT_NIST_P256) || (iCurve > XRT_NIST_P384) ) {
		return 0;
	}
	iValid = point_decode(&Point, pPoint, iPointSize, id_to_curve(iCurve));
	xrtSecureZero(&Point, sizeof(Point));
	return iValid;
}



/* 用标量乘一个经过 SEC 1 编码的公共点，并原位写回结果。 */
uint32 __xrtNistPointMultiply(
	int iCurve,
	void* pPoint,
	size_t iPointSize,
	const void* pScalar,
	size_t iScalarSize
)
{
	if ( (iCurve < XRT_NIST_P256) || (iCurve > XRT_NIST_P384) ) {
		return 0;
	}
	return api_mul(
		(uint8*)pPoint,
		iPointSize,
		(const uint8*)pScalar,
		iScalarSize,
		iCurve
	);
}



/* 用标量乘固定曲线生成元。 */
size_t __xrtNistPointMultiplyBase(
	int iCurve,
	void* pPoint,
	const void* pScalar,
	size_t iScalarSize
)
{
	if ( (iCurve < XRT_NIST_P256) || (iCurve > XRT_NIST_P384) ) {
		return 0;
	}
	return api_mulgen(
		(uint8*)pPoint,
		(const uint8*)pScalar,
		iScalarSize,
		iCurve
	);
}



/* 计算 left * leftScalar + right * rightScalar，并原位写回 left。 */
uint32 __xrtNistPointMultiplyAdd(
	int iCurve,
	void* pLeft,
	const void* pRight,
	size_t iPointSize,
	const void* pLeftScalar,
	size_t iLeftScalarSize,
	const void* pRightScalar,
	size_t iRightScalarSize
)
{
	if ( (iCurve < XRT_NIST_P256) || (iCurve > XRT_NIST_P384) ) {
		return 0;
	}
	return api_muladd(
		(uint8*)pLeft,
		(const uint8*)pRight,
		iPointSize,
		(const uint8*)pLeftScalar,
		iLeftScalarSize,
		(const uint8*)pRightScalar,
		iRightScalarSize,
		 iCurve
	);
}



/* 设置 NIST 曲线密钥或密钥协商错误。 */
void __xrtNistError(
	cstr sOperation,
	cstr sMessage,
	int iCode
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 常量时间验证大端标量处于 [1, order) 范围。 */
uint32 __xrtNistScalarValid(
	const void* pScalar,
	const void* pOrder,
	size_t iScalarSize
)
{
	const uint8* pValue = (const uint8*)pScalar;
	const uint8* pLimit = (const uint8*)pOrder;
	uint32 iBorrow = 0;
	uint32 iCombined = 0;

	for ( size_t i = iScalarSize; i > 0; i-- ) {
		uint32 iDifference = (uint32)pValue[i - 1u] -
			(uint32)pLimit[i - 1u] - iBorrow;

		iCombined |= pValue[i - 1u];
		iBorrow = iDifference >> 31u;
	}
	return iBorrow & __xrtI31NotEqual(iCombined, 0);
}



/* 验证输入后执行公开曲线点乘，并保证失败时输出不变。 */
bool __xrtNistMultiplyApi(
	int iCurve,
	const void* pScalar,
	const void* pOrder,
	size_t iScalarSize,
	const void* pPoint,
	size_t iPointSize,
	void* pOutput,
	cstr sOperation
)
{
	uint8 Point[97];
	uint32 iScalarValid = __xrtNistScalarValid(
		pScalar, pOrder, iScalarSize
	);
	uint32 iPointValid = __xrtNistPointValid(iCurve, pPoint, iPointSize);
	uint32 iResult;

	memcpy(Point, pPoint, iPointSize);
	iResult = __xrtNistPointMultiply(
		iCurve, Point, iPointSize, pScalar, iScalarSize
	);
	if ( (iScalarValid & iPointValid & iResult) == 0 ) {
		xrtSecureZero(Point, sizeof(Point));
		__xrtNistError(
			sOperation,
			"the private scalar or public point is invalid",
			XCRYPTO_ERROR_KEY
		);
		return false;
	}
	memcpy(pOutput, Point, iPointSize);
	xrtSecureZero(Point, sizeof(Point));
	return true;
}



/* 验证私钥后派生公开曲线公钥，并保证失败时输出不变。 */
bool __xrtNistPublicApi(
	int iCurve,
	const void* pPrivate,
	const void* pOrder,
	size_t iPrivateSize,
	size_t iPublicSize,
	void* pPublic,
	cstr sOperation
)
{
	uint8 Public[97];

	if ( __xrtNistScalarValid(pPrivate, pOrder, iPrivateSize) == 0 ) {
		__xrtNistError(sOperation, "the private scalar is invalid", XCRYPTO_ERROR_KEY);
		return false;
	}
	if ( __xrtNistPointMultiplyBase(
		iCurve, Public, pPrivate, iPrivateSize
	) != iPublicSize ) {
		xrtSecureZero(Public, sizeof(Public));
		__xrtNistError(sOperation, "public key derivation failed", XCRYPTO_ERROR_KEY);
		return false;
	}
	memcpy(pPublic, Public, iPublicSize);
	xrtSecureZero(Public, sizeof(Public));
	return true;
}



/* 验证两个公共点后执行点加，并保证失败时输出不变。 */
bool __xrtNistAddApi(
	int iCurve,
	const void* pLeft,
	const void* pRight,
	size_t iPointSize,
	void* pOutput,
	cstr sOperation
)
{
	const uint8 One[] = { 1 };
	uint8 Left[97];
	uint8 Right[97];
	uint32 iValid = __xrtNistPointValid(iCurve, pLeft, iPointSize) &
		__xrtNistPointValid(iCurve, pRight, iPointSize);

	memcpy(Left, pLeft, iPointSize);
	memcpy(Right, pRight, iPointSize);
	iValid &= __xrtNistPointMultiplyAdd(
		iCurve, Left, Right, iPointSize, One, sizeof(One), One, sizeof(One)
	);
	if ( iValid == 0 ) {
		xrtSecureZero(Left, sizeof(Left));
		xrtSecureZero(Right, sizeof(Right));
		__xrtNistError(
			sOperation,
			"the points are invalid or their sum is the point at infinity",
			XCRYPTO_ERROR_KEY
		);
		return false;
	}
	memcpy(pOutput, Left, iPointSize);
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	return true;
}



/* 验证私钥和对端公钥后计算 ECDH 横坐标。 */
bool __xrtNistSharedApi(
	int iCurve,
	const void* pPrivate,
	const void* pOrder,
	size_t iPrivateSize,
	const void* pPeerPublic,
	size_t iPublicSize,
	void* pShared,
	cstr sOperation
)
{
	uint8 Point[97];
	uint32 iValid = __xrtNistScalarValid(pPrivate, pOrder, iPrivateSize) &
		__xrtNistPointValid(iCurve, pPeerPublic, iPublicSize);

	memcpy(Point, pPeerPublic, iPublicSize);
	iValid &= __xrtNistPointMultiply(
		iCurve, Point, iPublicSize, pPrivate, iPrivateSize
	);
	if ( iValid == 0 ) {
		xrtSecureZero(Point, sizeof(Point));
		__xrtNistError(
			sOperation,
			"the private key or peer public key is invalid",
			XCRYPTO_ERROR_KEY_AGREEMENT
		);
		return false;
	}
	memcpy(pShared, Point + 1, iPrivateSize);
	xrtSecureZero(Point, sizeof(Point));
	return true;
}



/* 清理本实现使用的局部适配宏，避免污染单头文件中的后续模块。 */
#undef t10
#undef t9
#undef t8
#undef t7
#undef t6
#undef t5
#undef t4
#undef t3
#undef t2
#undef t1
#undef Pz
#undef Py
#undef Px
#undef P2z
#undef P2y
#undef P2x
#undef P1z
#undef P1y
#undef P1x
#undef ENDCODE
#undef MTZ
#undef MINV
#undef MMUL
#undef MSUB
#undef MADD
#undef MSET
#undef I31_LEN
#undef BR_MAX_EC_SIZE
#undef BR_EC_secp384r1
#undef BR_EC_secp256r1
#undef br_i31_montymul
#undef br_i31_encode
#undef br_i31_decode_mod
#undef br_i31_iszero
#undef br_i31_sub
#undef br_i31_add
#undef CCOPY
#undef NEQ
#undef EQ
#undef MUX
#undef NOT

#endif
