#include "../internal/xrt_crypto_ed25519.h"



#if defined(XRT_FEATURE_CRYPTO_ED25519)

static const __xrt25519field __xrtEd25519D = {
	UINT32_C(0x135978A3), UINT32_C(0x75EB4DCA),
	UINT32_C(0x4141D8AB), UINT32_C(0x00700A4D),
	UINT32_C(0x7779E898), UINT32_C(0x8CC74079),
	UINT32_C(0x2B6FFE73), UINT32_C(0x52036CEE)
};

static const __xrt25519field __xrtEd25519SqrtMinusOne = {
	UINT32_C(0x4A0EA0B0), UINT32_C(0xC4EE1B27),
	UINT32_C(0xAD2FE478), UINT32_C(0x2F431806),
	UINT32_C(0x3DFBD7A7), UINT32_C(0x2B4D0099),
	UINT32_C(0x4FC1DF0B), UINT32_C(0x2B832480)
};

static const uint8 __xrtEd25519Order[32] = {
	0xED, 0xD3, 0xF5, 0x5C, 0x1A, 0x63, 0x12, 0x58,
	0xD6, 0x9C, 0xF7, 0xA2, 0xDE, 0xF9, 0xDE, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static const uint32 __xrtEd25519OrderWords[9] = {
	UINT32_C(0x5CF5D3ED), UINT32_C(0x5812631A),
	UINT32_C(0xA2F79CD6), UINT32_C(0x14DEF9DE),
	0, 0, 0, UINT32_C(0x10000000), 0
};



/* 计算有限域元素的固定公开指数幂。 */
static void __xrtEd25519FieldPower(
	__xrt25519field pOutput,
	const __xrt25519field pInput,
	const uint8 pExponent[32],
	int iTopBit
)
{
	__xrt25519field Result;

	__xrt25519One(Result);
	for ( int i = iTopBit; i >= 0; i-- ) {
		__xrt25519Square(Result, Result);
		if ( ((pExponent[i / 8] >> (i % 8)) & 1u) != 0 ) {
			__xrt25519Multiply(Result, Result, pInput);
		}
	}
	__xrt25519Copy(pOutput, Result);
	xrtSecureZero(Result, sizeof(Result));
}



/* 计算有限域乘法逆元。 */
static void __xrtEd25519FieldInvert(
	__xrt25519field pOutput,
	const __xrt25519field pInput
)
{
	static const uint8 Exponent[32] = {
		0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F
	};

	__xrtEd25519FieldPower(pOutput, pInput, Exponent, 254);
}



/* 计算 z^((p - 5) / 8)。 */
static void __xrtEd25519FieldPowerP58(
	__xrt25519field pOutput,
	const __xrt25519field pInput
)
{
	static const uint8 Exponent[32] = {
		0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F
	};

	__xrtEd25519FieldPower(pOutput, pInput, Exponent, 251);
}



/* 比较两个有限域元素的规范编码。 */
static bool __xrtEd25519FieldEqual(
	const __xrt25519field pLeft,
	const __xrt25519field pRight
)
{
	uint8 Left[32];
	uint8 Right[32];
	bool bEqual;

	__xrt25519Store(Left, pLeft);
	__xrt25519Store(Right, pRight);
	bEqual = xrtConstTimeEqual(Left, Right, sizeof(Left));
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	return bEqual;
}



/* 判断有限域元素是否为零。 */
static bool __xrtEd25519FieldZero(const __xrt25519field Value)
{
	uint8 Encoded[32];
	uint8 iOr = 0;

	__xrt25519Store(Encoded, Value);
	for ( size_t i = 0; i < sizeof(Encoded); i++ ) {
		iOr |= Encoded[i];
	}
	xrtSecureZero(Encoded, sizeof(Encoded));
	return iOr == 0;
}



/* 返回有限域元素规范编码的最低位。 */
static uint32 __xrtEd25519FieldOdd(const __xrt25519field Value)
{
	uint8 Encoded[32];
	uint32 iOdd;

	__xrt25519Store(Encoded, Value);
	iOdd = Encoded[0] & 1u;
	xrtSecureZero(Encoded, sizeof(Encoded));
	return iOdd;
}



/* 计算有限域元素的相反数。 */
static void __xrtEd25519FieldNegate(
	__xrt25519field pOutput,
	const __xrt25519field pInput
)
{
	__xrt25519field Zero;

	__xrt25519Zero(Zero);
	__xrt25519Subtract(pOutput, Zero, pInput);
}



/* 计算 u / v 的平方根；只执行一次固定指数幂。 */
static bool __xrtEd25519FieldSqrtRatio(
	__xrt25519field pOutput,
	const __xrt25519field pU,
	const __xrt25519field pV
)
{
	__xrt25519field V2;
	__xrt25519field V3;
	__xrt25519field V7;
	__xrt25519field Base;
	__xrt25519field Root;
	__xrt25519field Check;
	__xrt25519field NegativeU;
	bool bResult = true;

	__xrt25519Square(V2, pV);
	__xrt25519Multiply(V3, V2, pV);
	__xrt25519Square(V7, V3);
	__xrt25519Multiply(V7, V7, pV);
	__xrt25519Multiply(Base, pU, V7);
	__xrtEd25519FieldPowerP58(Root, Base);
	__xrt25519Multiply(Root, Root, pU);
	__xrt25519Multiply(Root, Root, V3);

	__xrt25519Square(Check, Root);
	__xrt25519Multiply(Check, Check, pV);
	if ( !__xrtEd25519FieldEqual(Check, pU) ) {
		__xrtEd25519FieldNegate(NegativeU, pU);
		if ( !__xrtEd25519FieldEqual(Check, NegativeU) ) {
			bResult = false;
		} else {
			__xrt25519Multiply(Root, Root, __xrtEd25519SqrtMinusOne);
		}
	}
	if ( bResult ) {
		__xrt25519Copy(pOutput, Root);
	}
	xrtSecureZero(V2, sizeof(V2));
	xrtSecureZero(V3, sizeof(V3));
	xrtSecureZero(V7, sizeof(V7));
	xrtSecureZero(Base, sizeof(Base));
	xrtSecureZero(Root, sizeof(Root));
	xrtSecureZero(Check, sizeof(Check));
	xrtSecureZero(NegativeU, sizeof(NegativeU));
	return bResult;
}



/* 设置扩展坐标单位元。 */
static void __xrtEd25519PointSetIdentity(__xrted25519point* pPoint)
{
	__xrt25519Zero(pPoint->X);
	__xrt25519One(pPoint->Y);
	__xrt25519One(pPoint->Z);
	__xrt25519Zero(pPoint->T);
}



/* 复制扩展坐标点。 */
static void __xrtEd25519PointCopy(
	__xrted25519point* pOutput,
	const __xrted25519point* pInput
)
{
	memcpy(pOutput, pInput, sizeof(*pOutput));
}



/* 以常数时间掩码选择扩展坐标点。 */
static void __xrtEd25519PointSelect(
	__xrted25519point* pOutput,
	const __xrted25519point* pFalse,
	const __xrted25519point* pTrue,
	uint32 iMask
)
{
	__xrt25519Select(pOutput->X, pFalse->X, pTrue->X, iMask);
	__xrt25519Select(pOutput->Y, pFalse->Y, pTrue->Y, iMask);
	__xrt25519Select(pOutput->Z, pFalse->Z, pTrue->Z, iMask);
	__xrt25519Select(pOutput->T, pFalse->T, pTrue->T, iMask);
}



/* 计算两个扩展坐标点之和。 */
void __xrtEd25519PointAdd(
	__xrted25519point* pOutput,
	const __xrted25519point* pLeft,
	const __xrted25519point* pRight
)
{
	__xrt25519field A;
	__xrt25519field B;
	__xrt25519field C;
	__xrt25519field D;
	__xrt25519field E;
	__xrt25519field F;
	__xrt25519field G;
	__xrt25519field H;
	__xrt25519field First;
	__xrt25519field Second;

	__xrt25519Subtract(First, pLeft->Y, pLeft->X);
	__xrt25519Subtract(Second, pRight->Y, pRight->X);
	__xrt25519Multiply(A, First, Second);
	__xrt25519Add(First, pLeft->Y, pLeft->X);
	__xrt25519Add(Second, pRight->Y, pRight->X);
	__xrt25519Multiply(B, First, Second);
	__xrt25519Multiply(C, pLeft->T, pRight->T);
	__xrt25519Multiply(C, C, __xrtEd25519D);
	__xrt25519Add(C, C, C);
	__xrt25519Multiply(D, pLeft->Z, pRight->Z);
	__xrt25519Add(D, D, D);
	__xrt25519Subtract(E, B, A);
	__xrt25519Subtract(F, D, C);
	__xrt25519Add(G, D, C);
	__xrt25519Add(H, B, A);
	__xrt25519Multiply(pOutput->X, E, F);
	__xrt25519Multiply(pOutput->Y, G, H);
	__xrt25519Multiply(pOutput->T, E, H);
	__xrt25519Multiply(pOutput->Z, F, G);
}



/* 计算扩展坐标点的二倍。 */
static void __xrtEd25519PointDouble(
	__xrted25519point* pOutput,
	const __xrted25519point* pInput
)
{
	__xrt25519field A;
	__xrt25519field B;
	__xrt25519field C;
	__xrt25519field D;
	__xrt25519field E;
	__xrt25519field F;
	__xrt25519field G;
	__xrt25519field H;
	__xrt25519field Temp;

	__xrt25519Square(A, pInput->X);
	__xrt25519Square(B, pInput->Y);
	__xrt25519Square(C, pInput->Z);
	__xrt25519Add(C, C, C);
	__xrtEd25519FieldNegate(D, A);
	__xrt25519Add(Temp, pInput->X, pInput->Y);
	__xrt25519Square(E, Temp);
	__xrt25519Subtract(E, E, A);
	__xrt25519Subtract(E, E, B);
	__xrt25519Add(G, D, B);
	__xrt25519Subtract(F, G, C);
	__xrt25519Subtract(H, D, B);
	__xrt25519Multiply(pOutput->X, E, F);
	__xrt25519Multiply(pOutput->Y, G, H);
	__xrt25519Multiply(pOutput->T, E, H);
	__xrt25519Multiply(pOutput->Z, F, G);
}



/* 以常数时间生成两个 4 位表索引是否相同的掩码。 */
static uint32 __xrtEd25519IndexMask(uint32 iLeft, uint32 iRight)
{
	uint32 iDifference = iLeft ^ iRight;

	iDifference |= 0u - iDifference;
	return 0u - ((iDifference >> 31u) ^ 1u);
}



/* 返回 Ed25519 标准基点。 */
void __xrtEd25519PointBase(__xrted25519point* pPoint)
{
	static const __xrt25519field BaseX = {
		UINT32_C(0x8F25D51A), UINT32_C(0xC9562D60),
		UINT32_C(0x9525A7B2), UINT32_C(0x692CC760),
		UINT32_C(0xFDD6DC5C), UINT32_C(0xC0A4E231),
		UINT32_C(0xCD6E53FE), UINT32_C(0x216936D3)
	};
	static const __xrt25519field BaseY = {
		UINT32_C(0x66666658), UINT32_C(0x66666666),
		UINT32_C(0x66666666), UINT32_C(0x66666666),
		UINT32_C(0x66666666), UINT32_C(0x66666666),
		UINT32_C(0x66666666), UINT32_C(0x66666666)
	};

	__xrt25519Copy(pPoint->X, BaseX);
	__xrt25519Copy(pPoint->Y, BaseY);
	__xrt25519One(pPoint->Z);
	__xrt25519Multiply(pPoint->T, BaseX, BaseY);
}



/* 以固定窗口和常数时间选点计算标量乘基点。 */
void __xrtEd25519PointMultiplyBase(
	__xrted25519point* pOutput,
	const uint8 pScalar[32]
)
{
	__xrted25519point Table[16];
	__xrted25519point Result;
	__xrted25519point Selected;
	__xrted25519point Temp;

	__xrtEd25519PointSetIdentity(&Table[0]);
	__xrtEd25519PointBase(&Table[1]);
	for ( size_t i = 2; i < 16u; i++ ) {
		__xrtEd25519PointAdd(&Table[i], &Table[i - 1u], &Table[1]);
	}
	__xrtEd25519PointSetIdentity(&Result);
	for ( int i = 63; i >= 0; i-- ) {
		uint32 iNibble = (pScalar[i / 2] >> ((i & 1) * 4)) & 0x0Fu;

		for ( size_t j = 0; j < 4u; j++ ) {
			__xrtEd25519PointDouble(&Temp, &Result);
			__xrtEd25519PointCopy(&Result, &Temp);
		}
		__xrtEd25519PointSetIdentity(&Selected);
		for ( uint32 j = 0; j < 16u; j++ ) {
			__xrtEd25519PointSelect(
				&Selected, &Selected, &Table[j],
				__xrtEd25519IndexMask(iNibble, j)
			);
		}
		__xrtEd25519PointAdd(&Temp, &Result, &Selected);
		__xrtEd25519PointCopy(&Result, &Temp);
	}
	__xrtEd25519PointCopy(pOutput, &Result);
	xrtSecureZero(Table, sizeof(Table));
	xrtSecureZero(&Result, sizeof(Result));
	xrtSecureZero(&Selected, sizeof(Selected));
	xrtSecureZero(&Temp, sizeof(Temp));
}



/* 以公开标量计算任意点乘法。 */
void __xrtEd25519PointMultiplyPublic(
	__xrted25519point* pOutput,
	const __xrted25519point* pPoint,
	const uint8 pScalar[32]
)
{
	__xrted25519point Result;
	__xrted25519point Temp;

	__xrtEd25519PointSetIdentity(&Result);
	for ( int i = 255; i >= 0; i-- ) {
		__xrtEd25519PointDouble(&Temp, &Result);
		__xrtEd25519PointCopy(&Result, &Temp);
		if ( ((pScalar[i / 8] >> (i % 8)) & 1u) != 0 ) {
			__xrtEd25519PointAdd(&Temp, &Result, pPoint);
			__xrtEd25519PointCopy(&Result, &Temp);
		}
	}
	__xrtEd25519PointCopy(pOutput, &Result);
}



/* 严格解码规范的 Ed25519 点。 */
bool __xrtEd25519PointDecode(
	__xrted25519point* pPoint,
	const uint8 pInput[32]
)
{
	uint8 YEncoded[32];
	uint8 Canonical[32];
	uint32 iSign;
	__xrt25519field Y;
	__xrt25519field Y2;
	__xrt25519field U;
	__xrt25519field V;
	__xrt25519field X;
	__xrt25519field One;
	bool bResult = false;

	memcpy(YEncoded, pInput, sizeof(YEncoded));
	iSign = (YEncoded[31] >> 7u) & 1u;
	YEncoded[31] &= 0x7Fu;
	__xrt25519Load(Y, YEncoded);
	__xrt25519Store(Canonical, Y);
	if ( !xrtConstTimeEqual(YEncoded, Canonical, sizeof(YEncoded)) ) {
		goto cleanup;
	}
	__xrt25519Square(Y2, Y);
	__xrt25519One(One);
	__xrt25519Subtract(U, Y2, One);
	__xrt25519Multiply(V, __xrtEd25519D, Y2);
	__xrt25519Add(V, V, One);
	if ( !__xrtEd25519FieldSqrtRatio(X, U, V) ) {
		goto cleanup;
	}
	if ( __xrtEd25519FieldZero(X) && (iSign != 0) ) {
		goto cleanup;
	}
	if ( __xrtEd25519FieldOdd(X) != iSign ) {
		__xrtEd25519FieldNegate(X, X);
	}
	__xrt25519Copy(pPoint->X, X);
	__xrt25519Copy(pPoint->Y, Y);
	__xrt25519One(pPoint->Z);
	__xrt25519Multiply(pPoint->T, X, Y);
	bResult = true;

cleanup:
	xrtSecureZero(YEncoded, sizeof(YEncoded));
	xrtSecureZero(Canonical, sizeof(Canonical));
	xrtSecureZero(Y, sizeof(Y));
	xrtSecureZero(Y2, sizeof(Y2));
	xrtSecureZero(U, sizeof(U));
	xrtSecureZero(V, sizeof(V));
	xrtSecureZero(X, sizeof(X));
	xrtSecureZero(One, sizeof(One));
	return bResult;
}



/* 把扩展坐标点编码为规范的 Ed25519 字节序列。 */
void __xrtEd25519PointEncode(
	uint8 pOutput[32],
	const __xrted25519point* pPoint
)
{
	__xrt25519field InverseZ;
	__xrt25519field X;
	__xrt25519field Y;

	__xrtEd25519FieldInvert(InverseZ, pPoint->Z);
	__xrt25519Multiply(X, pPoint->X, InverseZ);
	__xrt25519Multiply(Y, pPoint->Y, InverseZ);
	__xrt25519Store(pOutput, Y);
	pOutput[31] |= (uint8)(__xrtEd25519FieldOdd(X) << 7u);
	xrtSecureZero(InverseZ, sizeof(InverseZ));
	xrtSecureZero(X, sizeof(X));
	xrtSecureZero(Y, sizeof(Y));
}



/* 判断点是否为单位元。 */
bool __xrtEd25519PointIdentity(const __xrted25519point* pPoint)
{
	return __xrtEd25519FieldZero(pPoint->X) &&
		__xrtEd25519FieldEqual(pPoint->Y, pPoint->Z);
}



/* 判断点是否属于阶为 L 的主子群。 */
bool __xrtEd25519PointMainSubgroup(const __xrted25519point* pPoint)
{
	__xrted25519point Product;
	bool bResult;

	__xrtEd25519PointMultiplyPublic(&Product, pPoint, __xrtEd25519Order);
	bResult = __xrtEd25519PointIdentity(&Product);
	xrtSecureZero(&Product, sizeof(Product));
	return bResult;
}



/* 把任意 512 位小端整数约简到 Ed25519 标量域。 */
void __xrtEd25519ScalarReduce(
	uint8 pOutput[32],
	const uint8 pInput[64]
)
{
	uint32 Remainder[9] = { 0 };
	uint32 Difference[9];

	for ( int i = 511; i >= 0; i-- ) {
		uint32 iCarry = (pInput[i / 8] >> (i % 8)) & 1u;
		uint64 iBorrow = 0;
		uint32 iMask;

		for ( size_t j = 0; j < 9u; j++ ) {
			uint32 iNext = Remainder[j] >> 31u;

			Remainder[j] = (Remainder[j] << 1u) | iCarry;
			iCarry = iNext;
		}
		for ( size_t j = 0; j < 9u; j++ ) {
			uint64 iSubtract = (uint64)__xrtEd25519OrderWords[j] + iBorrow;
			uint32 iValue = Remainder[j];

			Difference[j] = iValue - (uint32)iSubtract;
			iBorrow = ((uint64)iValue < iSubtract) ? 1u : 0u;
		}
		iMask = 0u - (uint32)(iBorrow ^ 1u);
		for ( size_t j = 0; j < 9u; j++ ) {
			Remainder[j] ^= (Remainder[j] ^ Difference[j]) & iMask;
		}
	}
	for ( size_t i = 0; i < 8u; i++ ) {
		__xrtCryptoStoreLe32(pOutput + (i * 4u), Remainder[i]);
	}
	xrtSecureZero(Remainder, sizeof(Remainder));
	xrtSecureZero(Difference, sizeof(Difference));
}



/* 计算 (pAdd + pLeft * pRight) mod L。 */
void __xrtEd25519ScalarMultiplyAdd(
	uint8 pOutput[32],
	const uint8 pAdd[32],
	const uint8 pLeft[32],
	const uint8 pRight[32]
)
{
	uint32 Left[8];
	uint32 Right[8];
	uint32 Product[16] = { 0 };
	uint8 Encoded[64];
	uint64 iCarry;

	for ( size_t i = 0; i < 8u; i++ ) {
		Left[i] = __xrtCryptoLoadLe32(pLeft + (i * 4u));
		Right[i] = __xrtCryptoLoadLe32(pRight + (i * 4u));
	}
	for ( size_t i = 0; i < 8u; i++ ) {
		iCarry = 0;
		for ( size_t j = 0; j < 8u; j++ ) {
			uint64 iValue = (uint64)Left[i] * Right[j] +
				Product[i + j] + iCarry;

			Product[i + j] = (uint32)iValue;
			iCarry = iValue >> 32u;
		}
		Product[i + 8u] = (uint32)iCarry;
	}
	iCarry = 0;
	for ( size_t i = 0; i < 8u; i++ ) {
		uint64 iValue = (uint64)Product[i] +
			__xrtCryptoLoadLe32(pAdd + (i * 4u)) + iCarry;

		Product[i] = (uint32)iValue;
		iCarry = iValue >> 32u;
	}
	for ( size_t i = 8u; i < 16u; i++ ) {
		uint64 iValue = (uint64)Product[i] + iCarry;

		Product[i] = (uint32)iValue;
		iCarry = iValue >> 32u;
	}
	for ( size_t i = 0; i < 16u; i++ ) {
		__xrtCryptoStoreLe32(Encoded + (i * 4u), Product[i]);
	}
	__xrtEd25519ScalarReduce(pOutput, Encoded);
	xrtSecureZero(Left, sizeof(Left));
	xrtSecureZero(Right, sizeof(Right));
	xrtSecureZero(Product, sizeof(Product));
	xrtSecureZero(Encoded, sizeof(Encoded));
}



/* 判断 32 字节小端标量是否严格小于 L。 */
bool __xrtEd25519ScalarCanonical(const uint8 pScalar[32])
{
	uint32 iBorrow = 0;

	for ( size_t i = 0; i < 8u; i++ ) {
		uint64 iSubtract = (uint64)__xrtEd25519OrderWords[i] + iBorrow;
		uint32 iValue = __xrtCryptoLoadLe32(pScalar + (i * 4u));

		iBorrow = ((uint64)iValue < iSubtract) ? 1u : 0u;
	}
	return iBorrow != 0;
}



/* 设置 Ed25519 密钥或签名错误。 */
void __xrtEd25519Error(
	cstr sOperation,
	cstr sMessage,
	int iCode
)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = (iCode == XCRYPTO_ERROR_KEY) ? XERR_VALUE : XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = iCode;
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 验证展开签名密钥的完整性标记。 */
bool __xrtEd25519ValidateKey(const xed25519key* pKey)
{
	if ( pKey == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pKey->Guard != XRT_INTERNAL_ED25519_KEY_GUARD ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 初始化带 RFC 8032 域分离前缀的 SHA-512 状态。 */
bool __xrtEd25519HashInit(
	xsha512* pHash,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize
)
{
	static const uint8 Domain[] = "SigEd25519 no Ed25519 collisions";
	uint8 Header[2];

	if ( pHash == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (iMode < XED25519_PURE) || (iMode > XED25519_PREHASH) ||
		 (iContextSize > XRT_ED25519_CONTEXT_MAX_SIZE) ||
		 ((pContext == NULL) && (iContextSize != 0)) ||
		 ((iMode == XED25519_PURE) && (iContextSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtSha512Init(pHash);
	if ( iMode == XED25519_PURE ) {
		return true;
	}
	Header[0] = (iMode == XED25519_PREHASH) ? 1u : 0u;
	Header[1] = (uint8)iContextSize;
	return xrtSha512Update(pHash, Domain, sizeof(Domain) - 1u) &&
		xrtSha512Update(pHash, Header, sizeof(Header)) &&
		xrtSha512Update(pHash, pContext, iContextSize);
}



/* 从 32 字节种子展开可重复使用的签名密钥。 */
XRT_API bool xrtEd25519KeyInit(
	xed25519key* pKey,
	const void* pSeed
)
{
	xed25519key Key;
	uint8 Hash[XRT_SHA512_SIZE];
	__xrted25519point Public;

	if ( (pKey == NULL) || (pSeed == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtSha512(pSeed, XRT_ED25519_SEED_SIZE, Hash) ) {
		return false;
	}
	memcpy(Key.Scalar, Hash, sizeof(Key.Scalar));
	Key.Scalar[0] &= 248u;
	Key.Scalar[31] &= 63u;
	Key.Scalar[31] |= 64u;
	memcpy(Key.Prefix, Hash + 32u, sizeof(Key.Prefix));
	__xrtEd25519PointMultiplyBase(&Public, Key.Scalar);
	__xrtEd25519PointEncode(Key.Public, &Public);
	Key.Guard = XRT_INTERNAL_ED25519_KEY_GUARD;
	memcpy(pKey, &Key, sizeof(Key));
	xrtSecureZero(Hash, sizeof(Hash));
	xrtSecureZero(&Public, sizeof(Public));
	xrtSecureZero(&Key, sizeof(Key));
	return true;
}



/* 清除展开后的 Ed25519 密钥。 */
XRT_API void xrtEd25519KeyClear(xed25519key* pKey)
{
	if ( pKey != NULL ) {
		xrtSecureZero(pKey, sizeof(*pKey));
	}
}



/* 从 32 字节种子导出 Ed25519 公钥。 */
XRT_API bool xrtEd25519Public(
	const void* pSeed,
	void* pPublic
)
{
	xed25519key Key;

	if ( (pSeed == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtEd25519KeyInit(&Key, pSeed) ) {
		return false;
	}
	memcpy(pPublic, Key.Public, XRT_ED25519_PUBLIC_SIZE);
	xrtEd25519KeyClear(&Key);
	return true;
}

#endif
