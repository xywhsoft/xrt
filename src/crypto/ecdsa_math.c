#include "../internal/xrt_crypto_ecdsa.h"
#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_MATH)

/* P-256 群阶在 31 位字 Montgomery 域中的 R^2。 */
static const uint32 __xrtEcdsaP256Square[] = {
	UINT32_C(0x00000108), UINT32_C(0x321E6CF2),
	UINT32_C(0x68F9A8D5), UINT32_C(0x0692A784),
	UINT32_C(0x06492C06), UINT32_C(0x5AAD4743),
	UINT32_C(0x62CB0C83), UINT32_C(0x4A3D0B3E),
	UINT32_C(0x76A38390), UINT32_C(0x000000A0)
};



/* P-384 群阶在 31 位字 Montgomery 域中的 R^2。 */
static const uint32 __xrtEcdsaP384Square[] = {
	UINT32_C(0x0000018C), UINT32_C(0x59C784C2),
	UINT32_C(0x3B7725B9), UINT32_C(0x73D64C5B),
	UINT32_C(0x56037D5D), UINT32_C(0x35AB7FF4),
	UINT32_C(0x50202287), UINT32_C(0x30606DDA),
	UINT32_C(0x6398B7E0), UINT32_C(0x5245D2AA),
	UINT32_C(0x344AEA06), UINT32_C(0x5B7A2826),
	UINT32_C(0x7E427F60), UINT32_C(0x00000CE6)
};



/* 返回固定曲线群阶对应的 Montgomery R^2。 */
const uint32* __xrtEcdsaSquare(int iCurve)
{
	return (iCurve == XRT_NIST_P256) ?
		__xrtEcdsaP256Square : __xrtEcdsaP384Square;
}



/* 把小于 2^bits 的普通整数按群阶归约一次。 */
void __xrtEcdsaReduce(uint32* pValue, const uint32* pOrder)
{
	uint32 iBorrow = __xrtI31Subtract(pValue, pOrder, 0);

	(void)__xrtI31Subtract(pValue, pOrder, __xrtI31Not(iBorrow));
}



/* 计算两个普通表示整数的群阶模乘积。 */
void __xrtEcdsaMultiply(
	uint32* pOutput,
	const uint32* pLeft,
	const uint32* pRight,
	const uint32* pOrder,
	const uint32* pSquare,
	uint32 iInverse,
	uint32* pTemporary
)
{
	__xrtI31MontgomeryMultiply(
		pTemporary, pLeft, pSquare, pOrder, iInverse
	);
	__xrtI31MontgomeryMultiply(
		pOutput, pTemporary, pRight, pOrder, iInverse
	);
}



/* 计算两个普通表示群阶整数的模和。 */
void __xrtEcdsaAdd(
	uint32* pLeft,
	const uint32* pRight,
	const uint32* pOrder
)
{
	uint32 iCarry = __xrtI31Add(pLeft, pRight, 1);
	uint32 iBorrow = __xrtI31Subtract(pLeft, pOrder, 0);

	(void)__xrtI31Subtract(
		pLeft, pOrder, iCarry | __xrtI31Not(iBorrow)
	);
}

#endif
