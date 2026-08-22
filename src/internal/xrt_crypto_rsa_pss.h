#ifndef XRT_INTERNAL_CRYPTO_RSA_PSS_H
#define XRT_INTERNAL_CRYPTO_RSA_PSS_H

#include "xrt_crypto_rsa.h"



#if defined(XRT_FEATURE_CRYPTO_RSA_PSS)

/* 返回受支持摘要的固定输出长度。 */
size_t __xrtRsaHashSize(xcryptohash iHash);



/* 使用指定摘要算法计算一段连续数据。 */
bool __xrtRsaHash(
	xcryptohash iHash,
	const void* pData,
	size_t iSize,
	void* pDigest
);



/* 计算 PSS 的 Hash(0x00 * 8 || messageHash || salt)，盐长度不受栈缓冲限制。 */
bool __xrtRsaPssHash(
	xcryptohash iHash,
	const void* pHash,
	const void* pSalt,
	size_t iSaltSize,
	void* pDigest
);



/* 按 RFC 8017 的 MGF1 规则分块生成掩码并原位异或数据。 */
bool __xrtRsaMgf1Xor(
	xcryptohash iHash,
	const uint8* pSeed,
	size_t iSeedSize,
	uint8* pData,
	size_t iDataSize
);



/* 返回大端模数去除前导零后的真实位数。 */
size_t __xrtRsaModulusBits(const xrsapublickey* pKey);

#endif

#endif
