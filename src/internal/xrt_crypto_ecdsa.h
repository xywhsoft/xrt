#ifndef XRT_INTERNAL_CRYPTO_ECDSA_H
#define XRT_INTERNAL_CRYPTO_ECDSA_H

#include "xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_ECDSA_CORE)

/* 设置签名编码、签名生成或签名验证错误。 */
void __xrtEcdsaError(cstr sOperation, cstr sMessage);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY)

/* 验证固定 NIST 曲线和任意非空摘要上的 raw r||s 签名。 */
bool __xrtEcdsaVerify(
	int iCurve,
	const void* pHash,
	size_t iHashSize,
	const void* pSignature,
	const void* pPublic,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_MATH)

#define XRT_ECDSA_I31_MAX_WORDS 14u
#define XRT_ECDSA_SCALAR_MAX_SIZE 48u
#define XRT_ECDSA_PUBLIC_MAX_SIZE 97u



/* 返回固定曲线群阶对应的 Montgomery R^2。 */
const uint32* __xrtEcdsaSquare(int iCurve);



/* 把小于 2^bits 的普通整数按群阶归约一次。 */
void __xrtEcdsaReduce(uint32* pValue, const uint32* pOrder);



/* 计算两个普通表示整数的群阶模乘积。 */
void __xrtEcdsaMultiply(
	uint32* pOutput,
	const uint32* pLeft,
	const uint32* pRight,
	const uint32* pOrder,
	const uint32* pSquare,
	uint32 iInverse,
	uint32* pTemporary
);



/* 计算两个普通表示群阶整数的模和。 */
void __xrtEcdsaAdd(
	uint32* pLeft,
	const uint32* pRight,
	const uint32* pOrder
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_VERIFY_DER)

/* 严格解码 DER 后调用固定曲线的 raw ECDSA 验证层。 */
bool __xrtEcdsaVerifyDer(
	int iCurve,
	const void* pHash,
	size_t iHashSize,
	const void* pDer,
	size_t iDerSize,
	const void* pPublic,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN)

typedef bool (*xrt_ecdsa_hmac_fn)(
	const void* pKey,
	size_t iKeySize,
	const void* pData,
	size_t iDataSize,
	void* pMac
);



/* 使用 RFC 6979 nonce 生成固定曲线的 low-S raw ECDSA 签名。 */
bool __xrtEcdsaSign(
	int iCurve,
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pSignature,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_CRYPTO_ECDSA_SIGN_DER)

/* 生成固定曲线的确定性签名并编码为规范 DER。 */
bool __xrtEcdsaSignDer(
	int iCurve,
	xcryptohash Hash,
	const void* pHash,
	const void* pPrivate,
	void* pDer,
	size_t iCapacity,
	size_t* pSize,
	size_t iScalarSize,
	size_t iPublicSize,
	cstr sOperation
);

#endif

#endif
