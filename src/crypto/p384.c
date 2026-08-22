#include "../internal/xrt_crypto_nist.h"



#if defined(XRT_FEATURE_CRYPTO_P384)

/* 验证 97 字节未压缩 SEC 1 公钥是否为有效 P-384 曲线点。 */
XRT_API bool xrtP384Valid(const void* pPublic)
{
	if ( pPublic == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistPointValid(
		XRT_NIST_P384, pPublic, XRT_P384_PUBLIC_SIZE
	) != 0;
}



/* 计算 scalar * point；三个固定长度缓冲可任意重叠。 */
XRT_API bool xrtP384Multiply(
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
		XRT_NIST_P384,
		pScalar,
		__xrtNistOrder(XRT_NIST_P384, NULL),
		XRT_P384_PRIVATE_SIZE,
		pPoint,
		XRT_P384_PUBLIC_SIZE,
		pOutput,
		"p384-multiply"
	);
}



/* 计算两个未压缩 P-384 公共点之和；输入输出可任意重叠。 */
XRT_API bool xrtP384Add(
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
		XRT_NIST_P384,
		pLeft,
		pRight,
		XRT_P384_PUBLIC_SIZE,
		pOutput,
		"p384-add"
	);
}



/* 从 48 字节私钥派生未压缩 P-384 公钥。 */
XRT_API bool xrtP384Public(const void* pPrivate, void* pPublic)
{
	if ( (pPrivate == NULL) || (pPublic == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return __xrtNistPublicApi(
		XRT_NIST_P384,
		pPrivate,
		__xrtNistOrder(XRT_NIST_P384, NULL),
		XRT_P384_PRIVATE_SIZE,
		XRT_P384_PUBLIC_SIZE,
		pPublic,
		"p384-public"
	);
}



/* 计算经过完整私钥和对端公钥验证的 P-384 ECDH 横坐标。 */
XRT_API bool xrtP384Shared(
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
		XRT_NIST_P384,
		pPrivate,
		__xrtNistOrder(XRT_NIST_P384, NULL),
		XRT_P384_PRIVATE_SIZE,
		pPeerPublic,
		XRT_P384_PUBLIC_SIZE,
		pShared,
		"p384-shared"
	);
}

#endif
