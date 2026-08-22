#ifndef XRT_INTERNAL_CRYPTO_H
#define XRT_INTERNAL_CRYPTO_H

#include "xrt_internal.h"



/* 从字节序列读取一个大端 32 位字。 */
static inline uint32 __xrtCryptoLoadBe32(const uint8* pData)
{
	return ((uint32)pData[0] << 24u) |
		((uint32)pData[1] << 16u) |
		((uint32)pData[2] << 8u) |
		(uint32)pData[3];
}



/* 从字节序列读取一个大端 64 位字。 */
static inline uint64 __xrtCryptoLoadBe64(const uint8* pData)
{
	return ((uint64)__xrtCryptoLoadBe32(pData) << 32u) |
		(uint64)__xrtCryptoLoadBe32(pData + 4);
}



/* 从字节序列读取一个小端 32 位字。 */
static inline uint32 __xrtCryptoLoadLe32(const uint8* pData)
{
	return (uint32)pData[0] |
		((uint32)pData[1] << 8u) |
		((uint32)pData[2] << 16u) |
		((uint32)pData[3] << 24u);
}



/* 向字节序列写入一个大端 32 位字。 */
static inline void __xrtCryptoStoreBe32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)(iValue >> 24u);
	pData[1] = (uint8)(iValue >> 16u);
	pData[2] = (uint8)(iValue >> 8u);
	pData[3] = (uint8)iValue;
}



/* 向字节序列写入一个大端 64 位字。 */
static inline void __xrtCryptoStoreBe64(uint8* pData, uint64 iValue)
{
	__xrtCryptoStoreBe32(pData, (uint32)(iValue >> 32u));
	__xrtCryptoStoreBe32(pData + 4, (uint32)iValue);
}



/* 向字节序列写入一个小端 32 位字。 */
static inline void __xrtCryptoStoreLe32(uint8* pData, uint32 iValue)
{
	pData[0] = (uint8)iValue;
	pData[1] = (uint8)(iValue >> 8u);
	pData[2] = (uint8)(iValue >> 16u);
	pData[3] = (uint8)(iValue >> 24u);
}



/* 向字节序列写入一个小端 64 位字。 */
static inline void __xrtCryptoStoreLe64(uint8* pData, uint64 iValue)
{
	__xrtCryptoStoreLe32(pData, (uint32)iValue);
	__xrtCryptoStoreLe32(pData + 4, (uint32)(iValue >> 32u));
}



/* 循环左移一个 32 位字。 */
static inline uint32 __xrtCryptoRotateLeft32(uint32 iValue, uint32 iBits)
{
	return (iValue << iBits) | (iValue >> (32u - iBits));
}



/* 循环右移一个 32 位字。 */
static inline uint32 __xrtCryptoRotateRight32(uint32 iValue, uint32 iBits)
{
	return (iValue >> iBits) | (iValue << (32u - iBits));
}



/* 循环右移一个 64 位字。 */
static inline uint64 __xrtCryptoRotateRight64(uint64 iValue, uint32 iBits)
{
	return (iValue >> iBits) | (iValue << (64u - iBits));
}



/* 判断两个非空字节区间是否按地址重叠，避免末地址加法溢出。 */
static inline bool __xrtCryptoRangesOverlap(
	const void* pLeft,
	size_t iLeftSize,
	const void* pRight,
	size_t iRightSize
)
{
	uintptr_t iLeft;
	uintptr_t iRight;

	if ( (iLeftSize == 0) || (iRightSize == 0) ) {
		return false;
	}
	iLeft = (uintptr_t)pLeft;
	iRight = (uintptr_t)pRight;
	if ( iLeft <= iRight ) {
		return (iRight - iLeft) < iLeftSize;
	}
	return (iLeft - iRight) < iRightSize;
}



#if defined(XRT_FEATURE_CRYPTO_AES)

#define XRT_INTERNAL_AES_EXPANDED_WORDS 120u
#define XRT_INTERNAL_AES_BACKEND_AESNI UINT32_C(0x00000001)
#define XRT_INTERNAL_AES_BACKEND_PCLMUL UINT32_C(0x00000002)
#define XRT_INTERNAL_AES_BACKEND_ARM_AES UINT32_C(0x00000004)
#define XRT_INTERNAL_AES_BACKEND_ARM_PMULL UINT32_C(0x00000008)
#define XRT_INTERNAL_AES_BACKEND_MASK ( \
	XRT_INTERNAL_AES_BACKEND_AESNI | \
	XRT_INTERNAL_AES_BACKEND_PCLMUL | \
	XRT_INTERNAL_AES_BACKEND_ARM_AES | \
	XRT_INTERNAL_AES_BACKEND_ARM_PMULL \
)

/* 验证 AES 状态，并把标准轮密钥转换为四路 64 位位切片轮密钥。 */
bool __xrtAesExpand(
	const xaes* pState,
	uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS]
);



/* 判断状态是否启用了运行时验证过的硬件后端。 */
bool __xrtAesHasBackend(const xaes* pState, uint32 iBackend);



/* 使用已经展开的位切片轮密钥批量加密完整块。 */
void __xrtAesEncryptBlocks(
	const xaes* pState,
	const uint64 pExpanded[XRT_INTERNAL_AES_EXPANDED_WORDS],
	const uint8* pInput,
	uint8* pOutput,
	size_t iBlocks
);

#endif



#if defined(XRT_FEATURE_CRYPTO_CHACHA20)

/* 生成一个内部 ChaCha20 密钥流块。 */
void __xrtChaCha20Block(
	const uint8 pKey[XRT_CHACHA20_KEY_SIZE],
	const uint8 pNonce[XRT_CHACHA20_NONCE_SIZE],
	uint32 iCounter,
	uint8 pOutput[XRT_CHACHA20_BLOCK_SIZE]
);

#endif

#endif
