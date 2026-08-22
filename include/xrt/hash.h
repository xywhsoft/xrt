#ifndef XRT_HASH_H
#define XRT_HASH_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_HASH_KEYED)

/* SipHash 使用完整的 128 位密钥，不能用普通 64 位 seed 代替。 */
typedef struct xsipkey {
	uint64 Low;
	uint64 High;
} xsipkey;



/* 流式状态可以放在栈上；字段公开只为避免隐藏分配，不应直接修改。 */
typedef struct xsiphash {
	uint64 State[4];
	uint64 Total;
	unsigned char Tail[8];
	uint32 Guard;
	uint8 TailSize;
} xsiphash;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HASH32)

/* 使用固定算法和默认 seed 计算确定性的 32 位非密码哈希。 */
XRT_API uint32 xrtHash32(const void* pData, size_t iSize);



/* 使用调用者提供的 seed 计算确定性的 32 位非密码哈希。 */
XRT_API uint32 xrtHash32Seed(const void* pData, size_t iSize, uint32 iSeed);

#endif



#if defined(XRT_FEATURE_HASH64)

/* 使用固定算法和默认 seed 计算确定性的 64 位非密码哈希。 */
XRT_API uint64 xrtHash64(const void* pData, size_t iSize);



/* 使用调用者提供的 seed 计算确定性的 64 位非密码哈希。 */
XRT_API uint64 xrtHash64Seed(const void* pData, size_t iSize, uint64 iSeed);

#endif



#if defined(XRT_FEATURE_HASH_KEYED)

/* 从两个 64 位字创建 SipHash 密钥。 */
XRT_API xsipkey xrtSipKey(uint64 iLow, uint64 iHigh);



/* 使用 SipHash-2-4 对一段连续数据执行带密钥哈希。 */
XRT_API uint64 xrtSipHash(const void* pData, size_t iSize, xsipkey Key);



/* 初始化一个可跨任意分块边界工作的 SipHash-2-4 状态。 */
XRT_API void xrtSipHashInit(xsiphash* pState, xsipkey Key);



/* 向状态追加字节；输入不得与状态重叠，失败时状态不变。 */
XRT_API bool xrtSipHashUpdate(xsiphash* pState, const void* pData, size_t iSize);



/* 计算当前状态的哈希值，不修改状态，可重复调用或继续追加。 */
XRT_API uint64 xrtSipHashFinal(const xsiphash* pState);

#endif



XRT_EXTERN_C_END

#endif
