#include "../internal/xrt_x509.h"



#if defined(XRT_FEATURE_X509_DIGEST)

/* 计算证书签名方案指定的摘要并返回实际字节数。 */
bool __xrtX509Digest(
	xx509hash Hash,
	xbytesview Content,
	uint8 pDigest[XRT_X509_DIGEST_MAX_SIZE],
	size_t* pDigestSize
)
{
	bool bResult;

	if ( (pDigest == NULL) || (pDigestSize == NULL) ||
		((Content.Data == NULL) && (Content.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	switch ( Hash ) {
		case X509_HASH_SHA1:
			*pDigestSize = XRT_SHA1_SIZE;
			bResult = xrtSha1(Content.Data, Content.Size, pDigest);
			break;
		case X509_HASH_SHA224:
			*pDigestSize = XRT_SHA224_SIZE;
			bResult = xrtSha224(Content.Data, Content.Size, pDigest);
			break;
		case X509_HASH_SHA256:
			*pDigestSize = XRT_SHA256_SIZE;
			bResult = xrtSha256(Content.Data, Content.Size, pDigest);
			break;
		case X509_HASH_SHA384:
			*pDigestSize = XRT_SHA384_SIZE;
			bResult = xrtSha384(Content.Data, Content.Size, pDigest);
			break;
		case X509_HASH_SHA512:
			*pDigestSize = XRT_SHA512_SIZE;
			bResult = xrtSha512(Content.Data, Content.Size, pDigest);
			break;
		default:
			__xrtErrorSetUnsupported();
			return false;
	}
	return bResult;
}



/* 把 X.509 摘要标识转换为密码底座的同义标识。 */
bool __xrtX509CryptoHash(
	xx509hash Hash,
	xcryptohash* pCryptoHash
)
{
	if ( pCryptoHash == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	switch ( Hash ) {
		case X509_HASH_SHA1:
			*pCryptoHash = XCRYPTO_HASH_SHA1;
			break;
		case X509_HASH_SHA224:
			*pCryptoHash = XCRYPTO_HASH_SHA224;
			break;
		case X509_HASH_SHA256:
			*pCryptoHash = XCRYPTO_HASH_SHA256;
			break;
		case X509_HASH_SHA384:
			*pCryptoHash = XCRYPTO_HASH_SHA384;
			break;
		case X509_HASH_SHA512:
			*pCryptoHash = XCRYPTO_HASH_SHA512;
			break;
		default:
			__xrtErrorSetUnsupported();
			return false;
	}
	return true;
}

#endif
