#ifndef XRT_INTERNAL_RUNTIME_TYPE_H
#define XRT_INTERNAL_RUNTIME_TYPE_H

#include "xrt_internal.h"

#include <float.h>
#include <xrt/runtime_type.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE)

#define XRT_RUNTIME_TYPE_INHERITANCE_MAX 256u



/* 检查借用字符串视图的指针、长度和空文本约束。 */
static inline bool __xrtTypeViewValid(
	const xstrview* pText,
	bool bAllowEmpty
)
{
	if ( pText == NULL ) {
		return false;
	}
	if ( (pText->Data == NULL) && (pText->Size != 0u) ) {
		return false;
	}
	return bAllowEmpty || (pText->Size != 0u);
}



/* 按字节比较两个不要求零结尾的借用字符串视图。 */
static inline bool __xrtTypeViewEqual(
	const xstrview* pLeft,
	const xstrview* pRight
)
{
	return (pLeft->Size == pRight->Size) &&
		((pLeft->Size == 0u) ||
		 ((pLeft->Data != NULL) &&
		  (pRight->Data != NULL) &&
		  (memcmp(pLeft->Data, pRight->Data, pLeft->Size) == 0)));
}



/* 无对齐读取 C bool 或 32 位 ABI 布尔，并归一化为真假值。 */
static inline bool __xrtTypeReadBool(
	const void* pValue,
	size_t iSize,
	bool* pResult
)
{
	if ( iSize == sizeof(bool) ) {
		bool bValue;

		memcpy(&bValue, pValue, sizeof(bValue));
		*pResult = bValue;
		return true;
	}
	if ( iSize == sizeof(int32) ) {
		int32 iValue;

		memcpy(&iValue, pValue, sizeof(iValue));
		*pResult = iValue != 0;
		return true;
	}
	return false;
}



/* 把真假值规范写为 C bool 或 32 位 ABI 布尔。 */
static inline bool __xrtTypeWriteBool(
	bool bValue,
	size_t iSize,
	ptr pTarget
)
{
	if ( iSize == sizeof(bool) ) {
		memcpy(pTarget, &bValue, sizeof(bValue));
		return true;
	}
	if ( iSize == sizeof(int32) ) {
		int32 iValue = bValue ? 1 : 0;

		memcpy(pTarget, &iValue, sizeof(iValue));
		return true;
	}
	return false;
}



/* 无对齐读取受支持宽度的有符号整数。 */
static inline bool __xrtTypeReadSigned(
	const void* pValue,
	size_t iSize,
	int64* pResult
)
{
	switch ( iSize ) {
		case 1u: {
			int8 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 2u: {
			int16 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 4u: {
			int32 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (int64)iValue;
			return true;
		}
		case 8u: {
			int64 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = iValue;
			return true;
		}
		default:
			return false;
	}
}



/* 无对齐读取受支持宽度的无符号整数。 */
static inline bool __xrtTypeReadUnsigned(
	const void* pValue,
	size_t iSize,
	uint64* pResult
)
{
	switch ( iSize ) {
		case 1u: {
			uint8 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 2u: {
			uint16 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 4u: {
			uint32 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = (uint64)iValue;
			return true;
		}
		case 8u: {
			uint64 iValue;

			memcpy(&iValue, pValue, sizeof(iValue));
			*pResult = iValue;
			return true;
		}
		default:
			return false;
	}
}



/* 无对齐写入受支持宽度的有符号整数，超出目标范围时不修改输出。 */
static inline bool __xrtTypeWriteSigned(
	int64 iValue,
	size_t iSize,
	ptr pTarget
)
{
	switch ( iSize ) {
		case 1u: {
			int8 iResult;

			if ( (iValue < INT8_MIN) || (iValue > INT8_MAX) ) {
				return false;
			}
			iResult = (int8)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 2u: {
			int16 iResult;

			if ( (iValue < INT16_MIN) || (iValue > INT16_MAX) ) {
				return false;
			}
			iResult = (int16)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 4u: {
			int32 iResult;

			if ( (iValue < INT32_MIN) || (iValue > INT32_MAX) ) {
				return false;
			}
			iResult = (int32)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 8u:
			memcpy(pTarget, &iValue, sizeof(iValue));
			return true;
		default:
			return false;
	}
}



/* 无对齐写入受支持宽度的无符号整数，超出目标范围时不修改输出。 */
static inline bool __xrtTypeWriteUnsigned(
	uint64 iValue,
	size_t iSize,
	ptr pTarget
)
{
	switch ( iSize ) {
		case 1u: {
			uint8 iResult;

			if ( iValue > UINT8_MAX ) {
				return false;
			}
			iResult = (uint8)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 2u: {
			uint16 iResult;

			if ( iValue > UINT16_MAX ) {
				return false;
			}
			iResult = (uint16)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 4u: {
			uint32 iResult;

			if ( iValue > UINT32_MAX ) {
				return false;
			}
			iResult = (uint32)iValue;
			memcpy(pTarget, &iResult, sizeof(iResult));
			return true;
		}
		case 8u:
			memcpy(pTarget, &iValue, sizeof(iValue));
			return true;
		default:
			return false;
	}
}



/* 无对齐读取受支持宽度的 IEEE-754 浮点值。 */
static inline bool __xrtTypeReadFloat(
	const void* pValue,
	size_t iSize,
	double* pResult
)
{
	if ( iSize == sizeof(float) ) {
		float fValue;

		memcpy(&fValue, pValue, sizeof(fValue));
		*pResult = (double)fValue;
		return true;
	}
	if ( iSize == sizeof(double) ) {
		memcpy(pResult, pValue, sizeof(*pResult));
		return true;
	}
	return false;
}



/* 写入浮点值；无损模式拒绝精度变化，显式模式只拒绝有限值溢出。 */
static inline bool __xrtTypeWriteFloat(
	double fValue,
	size_t iSize,
	bool bLossless,
	ptr pTarget
)
{
	if ( iSize == sizeof(double) ) {
		memcpy(pTarget, &fValue, sizeof(fValue));
		return true;
	}
	if ( iSize == sizeof(float) ) {
		float fResult;

		if (
			!bLossless && (fValue == fValue) &&
			((fValue > FLT_MAX) || (fValue < -FLT_MAX))
		) {
			return false;
		}
		fResult = (float)fValue;
		if (
			bLossless && (fValue == fValue) &&
			((double)fResult != fValue)
		) {
			return false;
		}
		memcpy(pTarget, &fResult, sizeof(fResult));
		return true;
	}
	return false;
}

#endif

#endif
