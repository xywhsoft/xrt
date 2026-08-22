#include "../internal/xrt_crypto_25519.h"



#if defined(XRT_FEATURE_CRYPTO_X25519)

static const uint8 __xrtX25519Base[XRT_X25519_PUBLIC_SIZE] = { 9 };



/* 执行 Montgomery ladder 的前半轮。 */
static void __xrtX25519LadderFirst(__xrt25519field Values[5])
{
	uint32* pX2 = Values[0];
	uint32* pZ2 = Values[1];
	uint32* pX3 = Values[2];
	uint32* pZ3 = Values[3];
	uint32* pTemp = Values[4];

	__xrt25519Add(pTemp, pX2, pZ2);
	__xrt25519Subtract(pZ2, pX2, pZ2);
	__xrt25519Add(pX2, pX3, pZ3);
	__xrt25519Subtract(pZ3, pX3, pZ3);
	__xrt25519Multiply(pZ3, pZ3, pTemp);
	__xrt25519Multiply(pX2, pX2, pZ2);
	__xrt25519Add(pX3, pZ3, pX2);
	__xrt25519Subtract(pZ3, pZ3, pX2);
	__xrt25519Square(pTemp, pTemp);
	__xrt25519Square(pZ2, pZ2);
	__xrt25519Subtract(pX2, pTemp, pZ2);
	__xrt25519MultiplySmall(pZ2, pX2, 121665u);
	__xrt25519Add(pZ2, pZ2, pTemp);
}



/* 执行 Montgomery ladder 的后半轮。 */
static void __xrtX25519LadderSecond(
	__xrt25519field Values[5],
	const __xrt25519field pPoint
)
{
	uint32* pX2 = Values[0];
	uint32* pZ2 = Values[1];
	uint32* pX3 = Values[2];
	uint32* pZ3 = Values[3];
	uint32* pTemp = Values[4];

	__xrt25519Square(pZ3, pZ3);
	__xrt25519Multiply(pZ3, pZ3, pPoint);
	__xrt25519Square(pX3, pX3);
	__xrt25519Multiply(pZ2, pZ2, pX2);
	__xrt25519Subtract(pX2, pTemp, pX2);
	__xrt25519Multiply(pX2, pX2, pTemp);
}



/* 按固定 256 轮执行 Montgomery ladder。 */
static void __xrtX25519Ladder(
	__xrt25519field Values[5],
	const uint8 pScalar[XRT_X25519_PRIVATE_SIZE],
	const uint8 pPoint[XRT_X25519_PUBLIC_SIZE]
)
{
	__xrt25519field Point;
	uint32 iSwap = 0;
	uint32* pX2 = Values[0];
	uint32* pZ2 = Values[1];
	uint32* pX3 = Values[2];
	uint32* pZ3 = Values[3];

	memset(Values, 0, sizeof(__xrt25519field) * 4u);
	pX2[0] = 1u;
	pZ3[0] = 1u;
	__xrt25519Load(Point, pPoint);
	__xrt25519Copy(pX3, Point);
	for ( int i = 255; i >= 0; i-- ) {
		uint32 iBit = (uint32)((pScalar[i / 8] >> (i % 8)) & 1u);
		uint32 iMask = 0u - iBit;

		__xrt25519Swap(pX2, pX3, iSwap ^ iMask);
		__xrt25519Swap(pZ2, pZ3, iSwap ^ iMask);
		iSwap = iMask;
		__xrtX25519LadderFirst(Values);
		__xrtX25519LadderSecond(Values, Point);
	}
	__xrt25519Swap(pX2, pX3, iSwap);
	__xrt25519Swap(pZ2, pZ3, iSwap);
	xrtSecureZero(Point, sizeof(Point));
}



/* 计算 z^(p-2)、转换为仿射坐标并编码结果。 */
static bool __xrtX25519Compute(
	uint8 pOutput[XRT_X25519_SHARED_SIZE],
	const uint8 pScalarInput[XRT_X25519_PRIVATE_SIZE],
	const uint8 pPointInput[XRT_X25519_PUBLIC_SIZE]
)
{
	static const struct {
		uint8 Output;
		uint8 Multiply;
		uint8 Squares;
	} Steps[13] = {
		{ 2, 1, 1 }, { 2, 1, 1 }, { 4, 2, 3 }, { 2, 4, 6 },
		{ 3, 1, 1 }, { 3, 2, 12 }, { 4, 3, 25 }, { 2, 3, 25 },
		{ 2, 4, 50 }, { 3, 2, 125 }, { 3, 1, 2 }, { 3, 1, 2 },
		{ 3, 1, 1 }
	};
	uint8 Scalar[XRT_X25519_PRIVATE_SIZE];
	uint8 PointBytes[XRT_X25519_PUBLIC_SIZE];
	__xrt25519field Values[5];
	__xrt25519field Result;
	uint32* pPrevious;
	bool bNonZero;

	memcpy(Scalar, pScalarInput, sizeof(Scalar));
	memcpy(PointBytes, pPointInput, sizeof(PointBytes));
	Scalar[0] &= 248u;
	Scalar[31] &= 127u;
	Scalar[31] |= 64u;
	PointBytes[31] &= 127u;
	__xrtX25519Ladder(Values, Scalar, PointBytes);

	pPrevious = Values[1];
	for ( size_t i = 0; i < (sizeof(Steps) / sizeof(Steps[0])); i++ ) {
		uint32* pCurrent = Values[Steps[i].Output];

		for ( uint8 j = 0; j < Steps[i].Squares; j++ ) {
			__xrt25519Square(pCurrent, pPrevious);
			pPrevious = pCurrent;
		}
		__xrt25519Multiply(
			pCurrent, pCurrent, Values[Steps[i].Multiply]
		);
	}
	__xrt25519Multiply(Result, Values[0], Values[3]);
	bNonZero = __xrt25519Canonical(Result);
	for ( size_t i = 0; i < XRT_INTERNAL_25519_LIMBS; i++ ) {
		__xrtCryptoStoreLe32(pOutput + (i * 4u), Result[i]);
	}

	xrtSecureZero(Scalar, sizeof(Scalar));
	xrtSecureZero(PointBytes, sizeof(PointBytes));
	xrtSecureZero(Values, sizeof(Values));
	xrtSecureZero(Result, sizeof(Result));
	return bNonZero;
}



/* 设置低阶对端公钥导致的密钥协商错误。 */
static void __xrtX25519AgreementError(void)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_PROTOCOL;
	Desc.Domain = "xrt.crypto";
	Desc.Code = XCRYPTO_ERROR_KEY_AGREEMENT;
	Desc.Operation = "x25519-shared";
	Desc.Message = "the peer X25519 public key produced an all-zero shared secret";
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 执行底层 X25519 标量乘法，并允许输出覆盖任一输入。 */
XRT_API bool xrtX25519(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
)
{
	uint8 Output[XRT_X25519_SHARED_SIZE];

	if ( (pScalar == NULL) || (pPoint == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	(void)__xrtX25519Compute(
		Output, (const uint8*)pScalar, (const uint8*)pPoint
	);
	memcpy(pOutput, Output, sizeof(Output));
	xrtSecureZero(Output, sizeof(Output));
	return true;
}



/* 从调用方提供的私钥导出 X25519 公钥。 */
XRT_API bool xrtX25519Public(const void* pPrivate, void* pPublic)
{
	return xrtX25519(pPrivate, __xrtX25519Base, pPublic);
}



/* 计算并验证可用于密钥协商的非零共享秘密。 */
XRT_API bool xrtX25519Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
)
{
	uint8 Shared[XRT_X25519_SHARED_SIZE];
	bool bNonZero;

	if ( (pPrivate == NULL) || (pPeerPublic == NULL) || (pShared == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	bNonZero = __xrtX25519Compute(
		Shared, (const uint8*)pPrivate, (const uint8*)pPeerPublic
	);
	if ( !bNonZero ) {
		xrtSecureZero(Shared, sizeof(Shared));
		__xrtX25519AgreementError();
		return false;
	}
	memcpy(pShared, Shared, sizeof(Shared));
	xrtSecureZero(Shared, sizeof(Shared));
	return true;
}

#endif
