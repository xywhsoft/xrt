#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_P256)

/* 验证 65 字节未压缩 SEC 1 公钥是否为有效 P-256 曲线点。 */
XRT_API bool xrtP256Valid(const void* pPublic)
{
	if ( pPublic == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistPointValid(
		XRT_NIST_P256, pPublic, XRT_P256_PUBLIC_SIZE
	) != 0;
}



/* 计算 scalar * point；三个固定长度缓冲可任意重叠。 */
XRT_API bool xrtP256Multiply(
	const void* pScalar,
	const void* pPoint,
	void* pOutput
)
{
	if ( (pScalar == NULL) || (pPoint == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistMultiplyApi(
		XRT_NIST_P256,
		pScalar,
		__xrtNistOrder(XRT_NIST_P256, NULL),
		XRT_P256_PRIVATE_SIZE,
		pPoint,
		XRT_P256_PUBLIC_SIZE,
		pOutput,
		"p256-multiply"
	);
}



/* 计算两个未压缩 P-256 公共点之和；输入输出可任意重叠。 */
XRT_API bool xrtP256Add(
	const void* pLeft,
	const void* pRight,
	void* pOutput
)
{
	if ( (pLeft == NULL) || (pRight == NULL) || (pOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistAddApi(
		XRT_NIST_P256,
		pLeft,
		pRight,
		XRT_P256_PUBLIC_SIZE,
		pOutput,
		"p256-add"
	);
}



/* 从 32 字节私钥派生未压缩 P-256 公钥。 */
XRT_API bool xrtP256Public(const void* pPrivate, void* pPublic)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistPublicApi(
		XRT_NIST_P256,
		pPrivate,
		__xrtNistOrder(XRT_NIST_P256, NULL),
		XRT_P256_PRIVATE_SIZE,
		XRT_P256_PUBLIC_SIZE,
		pPublic,
		"p256-public"
	);
}



/* 计算经过完整私钥和对端公钥验证的 P-256 ECDH 横坐标。 */
XRT_API bool xrtP256Shared(
	const void* pPrivate,
	const void* pPeerPublic,
	void* pShared
)
{
	if ( (pPrivate == NULL) || (pPeerPublic == NULL) || (pShared == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistSharedApi(
		XRT_NIST_P256,
		pPrivate,
		__xrtNistOrder(XRT_NIST_P256, NULL),
		XRT_P256_PRIVATE_SIZE,
		pPeerPublic,
		XRT_P256_PUBLIC_SIZE,
		pShared,
		"p256-shared"
	);
}

#endif
