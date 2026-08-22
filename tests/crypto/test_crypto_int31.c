#include "../test.h"
#include "../../src/internal/xrt_crypto_int31.h"



static const uint32 TestModulus[] = { 7u, 101u };
static const uint32 TestMontgomerySquare[] = { 7u, 45u };



/* 计算测试范围内普通整数的位数。 */
static uint32 testInt31BitLength(uint32 iValue)
{
	uint32 iBits = 0;

	while ( iValue != 0 ) {
		iBits++;
		iValue >>= 1u;
	}
	return iBits;
}



/* 使用普通整数计算测试期望模幂。 */
static uint32 testInt31Power(uint32 iBase, uint32 iExponent, uint32 iModulus)
{
	uint64 iResult = 1;
	uint64 iValue = iBase;

	while ( iExponent != 0 ) {
		if ( (iExponent & 1u) != 0 ) {
			iResult = (iResult * iValue) % iModulus;
		}
		iValue = (iValue * iValue) % iModulus;
		iExponent >>= 1u;
	}
	return (uint32)iResult;
}



/* 验证严格模解码、编码和失败清零。 */
static void testInt31Codec(void)
{
	const uint8 Valid[] = { 0x0C };
	const uint8 Invalid[] = { 0x65 };
	uint8 Output[2] = { 0xAA, 0xAA };
	uint32 Value[2] = { 0, 0 };

	testRequire(__xrtI31DecodeMod(Value, Valid, sizeof(Valid), TestModulus) == 1,
		"int31 valid decode failed");
	testRequire((Value[0] == 7u) && (Value[1] == 12u),
		"int31 valid decode mismatch");
	__xrtI31Encode(Output, sizeof(Output), Value);
	testRequire((Output[0] == 0) && (Output[1] == 12u),
		"int31 encode mismatch");

	testRequire(__xrtI31DecodeMod(Value, Invalid, sizeof(Invalid), TestModulus) == 0,
		"int31 accepted the modulus itself");
	testRequire((Value[0] == 7u) && (__xrtI31IsZero(Value) == 1),
		"int31 rejected decode was not cleared");

	{
		const uint8 Wide[] = { 0x01, 0x23, 0x45, 0x67, 0x89 };
		uint8 Encoded[sizeof(Wide)];
		uint32 Decoded[3] = { 0, 0, 0 };

		__xrtI31Decode(Decoded, Wide, sizeof(Wide));
		__xrtI31Encode(Encoded, sizeof(Encoded), Decoded);
		testRequire(xrtConstTimeEqual(Encoded, Wide, sizeof(Wide)),
			"int31 unrestricted codec mismatch");
		__xrtI31RightShift(Decoded, 4);
		__xrtI31Encode(Encoded, sizeof(Encoded), Decoded);
		testRequire((Encoded[0] == 0) && (Encoded[1] == 0x12) &&
			(Encoded[2] == 0x34) && (Encoded[3] == 0x56) &&
			(Encoded[4] == 0x78), "int31 right shift mismatch");
	}
}



/* 验证条件加减操作不会在控制位关闭时修改输入。 */
static void testInt31ConditionalArithmetic(void)
{
	uint32 Left[2] = { 7u, 90u };
	const uint32 Right[2] = { 7u, 20u };

	testRequire(__xrtI31Add(Left, Right, 0) == 0,
		"int31 disabled add returned a carry");
	testRequire(Left[1] == 90u, "int31 disabled add changed the value");
	testRequire(__xrtI31Add(Left, Right, 1) == 0,
		"int31 add returned a carry");
	testRequire(Left[1] == 110u, "int31 add mismatch");
	testRequire(__xrtI31Subtract(Left, Right, 1) == 0,
		"int31 subtract returned a borrow");
	testRequire(Left[1] == 90u, "int31 subtract mismatch");
}



/* 验证 Montgomery 乘法和固定轨迹模幂。 */
static void testInt31ModularArithmetic(void)
{
	const uint8 Exponent[] = { 37u };
	uint32 Left[2] = { 7u, 12u };
	uint32 Right[2] = { 7u, 17u };
	uint32 LeftMontgomery[2];
	uint32 RightMontgomery[2];
	uint32 Product[2];
	uint32 TemporaryLeft[2];
	uint32 TemporaryRight[2];
	uint32 iInverse = __xrtI31NegativeInverse(TestModulus[1]);

	testRequire(((TestModulus[1] * iInverse) & XRT_I31_WORD_MASK) ==
		XRT_I31_WORD_MASK, "int31 negative inverse mismatch");
	__xrtI31MontgomeryMultiply(
		LeftMontgomery, Left, TestMontgomerySquare, TestModulus, iInverse
	);
	__xrtI31MontgomeryMultiply(
		RightMontgomery, Right, TestMontgomerySquare, TestModulus, iInverse
	);
	__xrtI31MontgomeryMultiply(
		Product, LeftMontgomery, RightMontgomery, TestModulus, iInverse
	);
	__xrtI31FromMontgomery(Product, TestModulus, iInverse);
	testRequire(Product[1] == 2u, "int31 Montgomery product mismatch");

	__xrtI31ModPower(
		Left,
		Exponent,
		sizeof(Exponent),
		TestModulus,
		TestMontgomerySquare,
		iInverse,
		TemporaryLeft,
		TemporaryRight
	);
	testRequire(Left[1] == 40u, "int31 modular power mismatch");
}



/* 用普通整数对数百组单字模运算做确定性差分。 */
static void testInt31Differential(void)
{
	for ( uint32 i = 0; i < 512u; i++ ) {
		uint32 iModulus = 101u + (i * 194u);
		uint32 iLeft = (17u + (i * 73u)) % iModulus;
		uint32 iRight = (29u + (i * 131u)) % iModulus;
		uint8 iExponent = (uint8)(1u + (i % 251u));
		uint64 iMontgomery = (UINT64_C(1) << 31u) % iModulus;
		uint32 Modulus[2];
		uint32 Square[2];
		uint32 Left[2];
		uint32 Right[2];
		uint32 LeftMontgomery[2];
		uint32 RightMontgomery[2];
		uint32 Product[2];
		uint32 TemporaryLeft[2];
		uint32 TemporaryRight[2];
		uint32 iInverse;

		iModulus |= 1u;
		Modulus[0] = testInt31BitLength(iModulus);
		Modulus[1] = iModulus;
		Square[0] = Modulus[0];
		Square[1] = (uint32)((iMontgomery * iMontgomery) % iModulus);
		Left[0] = Modulus[0];
		Left[1] = iLeft;
		Right[0] = Modulus[0];
		Right[1] = iRight;
		iInverse = __xrtI31NegativeInverse(iModulus);

		__xrtI31MontgomeryMultiply(
			LeftMontgomery, Left, Square, Modulus, iInverse
		);
		__xrtI31MontgomeryMultiply(
			RightMontgomery, Right, Square, Modulus, iInverse
		);
		__xrtI31MontgomeryMultiply(
			Product, LeftMontgomery, RightMontgomery, Modulus, iInverse
		);
		__xrtI31FromMontgomery(Product, Modulus, iInverse);
		testRequire(Product[1] ==
			(uint32)(((uint64)iLeft * iRight) % iModulus),
			"int31 differential product mismatch");

		__xrtI31ModPower(
			Left,
			&iExponent,
			1,
			Modulus,
			Square,
			iInverse,
			TemporaryLeft,
			TemporaryRight
		);
		testRequire(Left[1] == testInt31Power(iLeft, iExponent, iModulus),
			"int31 differential power mismatch");
	}
}



int main(void)
{
	testInt31Codec();
	testInt31ConditionalArithmetic();
	testInt31ModularArithmetic();
	testInt31Differential();
	printf("[PASS] crypto_int31\n");
	return 0;
}
