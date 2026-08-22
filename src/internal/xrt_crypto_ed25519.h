#ifndef XRT_INTERNAL_CRYPTO_ED25519_H
#define XRT_INTERNAL_CRYPTO_ED25519_H

#include "xrt_crypto_25519.h"



#if defined(XRT_FEATURE_CRYPTO_ED25519)

#define XRT_INTERNAL_ED25519_KEY_GUARD UINT32_C(0x45443235)

typedef struct __xrt_ed25519_point {
	__xrt25519field X;
	__xrt25519field Y;
	__xrt25519field Z;
	__xrt25519field T;
} __xrted25519point;



/* 设置 Ed25519 密钥或签名错误。 */
void __xrtEd25519Error(
	cstr sOperation,
	cstr sMessage,
	int iCode
);



/* 验证展开签名密钥的完整性标记。 */
bool __xrtEd25519ValidateKey(const xed25519key* pKey);



/* 初始化带 RFC 8032 域分离前缀的 SHA-512 状态。 */
bool __xrtEd25519HashInit(
	xsha512* pHash,
	xed25519mode iMode,
	const void* pContext,
	size_t iContextSize
);



/* 把任意 512 位小端整数约简到 Ed25519 标量域。 */
void __xrtEd25519ScalarReduce(
	uint8 pOutput[32],
	const uint8 pInput[64]
);



/* 计算 (pAdd + pLeft * pRight) mod L。 */
void __xrtEd25519ScalarMultiplyAdd(
	uint8 pOutput[32],
	const uint8 pAdd[32],
	const uint8 pLeft[32],
	const uint8 pRight[32]
);



/* 判断 32 字节小端标量是否严格小于 L。 */
bool __xrtEd25519ScalarCanonical(const uint8 pScalar[32]);



/* 返回 Ed25519 标准基点。 */
void __xrtEd25519PointBase(__xrted25519point* pPoint);



/* 以固定窗口和常数时间选点计算标量乘基点。 */
void __xrtEd25519PointMultiplyBase(
	__xrted25519point* pOutput,
	const uint8 pScalar[32]
);



/* 以公开标量计算任意点乘法。 */
void __xrtEd25519PointMultiplyPublic(
	__xrted25519point* pOutput,
	const __xrted25519point* pPoint,
	const uint8 pScalar[32]
);



/* 计算两个扩展坐标点之和。 */
void __xrtEd25519PointAdd(
	__xrted25519point* pOutput,
	const __xrted25519point* pLeft,
	const __xrted25519point* pRight
);



/* 严格解码规范的 Ed25519 点。 */
bool __xrtEd25519PointDecode(
	__xrted25519point* pPoint,
	const uint8 pInput[32]
);



/* 把扩展坐标点编码为规范的 Ed25519 字节序列。 */
void __xrtEd25519PointEncode(
	uint8 pOutput[32],
	const __xrted25519point* pPoint
);



/* 判断点是否为单位元。 */
bool __xrtEd25519PointIdentity(const __xrted25519point* pPoint);



/* 判断点是否属于阶为 L 的主子群。 */
bool __xrtEd25519PointMainSubgroup(const __xrted25519point* pPoint);

#endif

#endif
