#ifndef XRT_INTERNAL_CRYPTO_25519_H
#define XRT_INTERNAL_CRYPTO_25519_H

#include "xrt_crypto.h"



#if defined(XRT_FEATURE_CRYPTO_CURVE25519)

#define XRT_INTERNAL_25519_LIMBS 8u

typedef uint32 __xrt25519field[XRT_INTERNAL_25519_LIMBS];



/* 把有限域元素清零。 */
void __xrt25519Zero(__xrt25519field Value);



/* 把有限域元素设为一。 */
void __xrt25519One(__xrt25519field Value);



/* 复制一个有限域元素。 */
void __xrt25519Copy(
	__xrt25519field pOutput,
	const __xrt25519field pInput
);



/* 计算两个有限域元素之和。 */
void __xrt25519Add(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
);



/* 计算两个有限域元素之差。 */
void __xrt25519Subtract(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
);



/* 计算两个有限域元素之积，输出允许覆盖任一输入。 */
void __xrt25519Multiply(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	const __xrt25519field pRight
);



/* 计算有限域元素与一个 32 位小整数之积。 */
void __xrt25519MultiplySmall(
	__xrt25519field pOutput,
	const __xrt25519field pLeft,
	uint32 iRight
);



/* 计算有限域元素的平方。 */
void __xrt25519Square(
	__xrt25519field pOutput,
	const __xrt25519field pInput
);



/* 以常数时间掩码选择两个有限域元素。 */
void __xrt25519Select(
	__xrt25519field pOutput,
	const __xrt25519field pFalse,
	const __xrt25519field pTrue,
	uint32 iMask
);



/* 以全零或全一掩码交换两个有限域元素。 */
void __xrt25519Swap(
	__xrt25519field pLeft,
	__xrt25519field pRight,
	uint32 iMask
);



/* 从 32 字节小端序列读取元素，并清除编码最高位。 */
void __xrt25519Load(
	__xrt25519field pOutput,
	const uint8 pInput[32]
);



/* 把元素归约为唯一的 32 字节小端编码。 */
void __xrt25519Store(
	uint8 pOutput[32],
	const __xrt25519field pInput
);



/* 原位归约元素，并返回归约结果是否非零。 */
bool __xrt25519Canonical(__xrt25519field Value);

#endif

#endif
