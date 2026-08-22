#ifndef XRT_INTERNAL_CRYPTO_NIST_H
#define XRT_INTERNAL_CRYPTO_NIST_H

#include "xrt_crypto_int31.h"



#if defined(XRT_FEATURE_CRYPTO_NIST)

#define XRT_NIST_P256 0
#define XRT_NIST_P384 1



/* 设置 NIST 曲线密钥、签名或密钥协商错误。 */
void __xrtNistError(cstr sOperation, cstr sMessage, int iCode);



/* 返回固定曲线的群阶及其大端字节长度。 */
const uint8* __xrtNistOrder(int iCurve, size_t* pSize);



/* 验证固定曲线的未压缩 SEC 1 公共点。 */
uint32 __xrtNistPointValid(int iCurve, const void* pPoint, size_t iPointSize);



/* 用标量乘一个经过 SEC 1 编码的公共点，并原位写回结果。 */
uint32 __xrtNistPointMultiply(
	int iCurve,
	void* pPoint,
	size_t iPointSize,
	const void* pScalar,
	size_t iScalarSize
);



/* 用标量乘固定曲线生成元。 */
size_t __xrtNistPointMultiplyBase(
	int iCurve,
	void* pPoint,
	const void* pScalar,
	size_t iScalarSize
);



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
);



/* 常量时间验证大端标量处于 [1, order) 范围。 */
uint32 __xrtNistScalarValid(
	const void* pScalar,
	const void* pOrder,
	size_t iScalarSize
);



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
);



/* 验证私钥后派生公开曲线公钥，并保证失败时输出不变。 */
bool __xrtNistPublicApi(
	int iCurve,
	const void* pPrivate,
	const void* pOrder,
	size_t iPrivateSize,
	size_t iPublicSize,
	void* pPublic,
	cstr sOperation
);



/* 验证两个公共点后执行点加，并保证失败时输出不变。 */
bool __xrtNistAddApi(
	int iCurve,
	const void* pLeft,
	const void* pRight,
	size_t iPointSize,
	void* pOutput,
	cstr sOperation
);



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
);



#if defined(XRT_FEATURE_CRYPTO_NIST_KEYPAIR)

/* 从安全随机源采样私钥并原子发布密钥对。 */
bool __xrtNistKeyPairApi(
	int iCurve,
	size_t iPrivateSize,
	size_t iPublicSize,
	void* pPrivate,
	void* pPublic,
	cstr sOperation
);

#endif

#endif

#endif
