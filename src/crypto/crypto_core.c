#include "../internal/xrt_internal.h"



#if defined(XRT_FEATURE_CRYPTO_CORE)

/* 返回摘要标识对应的固定输出长度，不把实现是否裁剪混入元数据。 */
XRT_API size_t xrtCryptoHashSize(xcryptohash Hash)
{
	switch ( Hash ) {
		case XCRYPTO_HASH_MD5:
			return XRT_MD5_SIZE;
		case XCRYPTO_HASH_SHA1:
			return XRT_SHA1_SIZE;
		case XCRYPTO_HASH_SHA224:
			return XRT_SHA224_SIZE;
		case XCRYPTO_HASH_SHA256:
			return XRT_SHA256_SIZE;
		case XCRYPTO_HASH_SHA384:
			return XRT_SHA384_SIZE;
		case XCRYPTO_HASH_SHA512:
			return XRT_SHA512_SIZE;
		case XCRYPTO_HASH_SHA512_256:
			return XRT_SHA512_256_SIZE;
		default:
			return 0;
	}
}




/* 不根据数据内容提前退出，返回两段内存是否完全相同。 */
XRT_API bool xrtConstTimeEqual(
	const void* pLeft,
	const void* pRight,
	size_t iSize
)
{
	const uint8* pLeftBytes = (const uint8*)pLeft;
	const uint8* pRightBytes = (const uint8*)pRight;
	uint8 iDifference = 0;

	if ( iSize == 0 ) {
		return true;
	}
	if ( (pLeftBytes == NULL) || (pRightBytes == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iSize; i++ ) {
		iDifference |= pLeftBytes[i] ^ pRightBytes[i];
	}
	return iDifference == 0;
}

#endif
