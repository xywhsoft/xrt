#include "../internal/xrt_value.h"

#include <math.h>



#if defined(XRT_FEATURE_VALUE)

/* null 和布尔值不分配内存，统一允许 Retain 和 Release。 */
static xvalue __xrtValueNull = {
	INT32_MAX, XVALUE_NULL, XRT_VALUE_FLAG_STATIC, 0, { 0 }
};
static xvalue __xrtValueFalse = {
	INT32_MAX, XVALUE_BOOL, XRT_VALUE_FLAG_STATIC, 0, { 0 }
};
static xvalue __xrtValueTrue = {
	INT32_MAX, XVALUE_BOOL, XRT_VALUE_FLAG_STATIC, 0, { .Bool = true }
};



/* 混合整数位，避免数值哈希依赖主机字节序。 */
static uint64 __xrtValueMix(uint64 iValue)
{
	iValue ^= iValue >> 30u;
	iValue *= UINT64_C(0xBF58476D1CE4E5B9);
	iValue ^= iValue >> 27u;
	iValue *= UINT64_C(0x94D049BB133111EB);
	iValue ^= iValue >> 31u;
	return iValue;
}



/* 把类型和内容哈希组合成独立命名空间。 */
static uint64 __xrtValueTaggedHash(xvaluetype Type, uint64 iHash)
{
	return __xrtValueMix(iHash ^ ((uint64)(uint32)Type * UINT64_C(0x9E3779B97F4A7C15)));
}



/* 只在浮点数能够无损表示为 int64 时完成转换。 */
static bool __xrtValueFloatToInt(double fValue, int64* pValue)
{
	#if defined(__TINYC__)
		int64 iValue;

		if (
			!(fValue >= -9223372036854775808.0) ||
			!(fValue < 9223372036854775808.0)
		) {
			return false;
		}
		iValue = (int64)fValue;
		if ( (double)iValue != fValue ) {
			return false;
		}
		*pValue = iValue;
		return true;
	#else
		if (
			!isfinite(fValue) ||
			(fValue < -9223372036854775808.0) ||
			(fValue >= 9223372036854775808.0) ||
			(floor(fValue) != fValue)
		) {
			return false;
		}
		*pValue = (int64)fValue;
		return true;
	#endif
}



/* 只在浮点数能够无损表示为 uint64 时完成转换。 */
static bool __xrtValueFloatToUInt(double fValue, uint64* pValue)
{
	#if defined(__TINYC__)
		uint64 iValue;

		if ( !(fValue >= 0.0) || !(fValue < 18446744073709551616.0) ) {
			return false;
		}
		iValue = (uint64)fValue;
		if ( (double)iValue != fValue ) {
			return false;
		}
		*pValue = iValue;
		return true;
	#else
		if (
			!isfinite(fValue) || (fValue < 0.0) ||
			(fValue >= 18446744073709551616.0) || (floor(fValue) != fValue)
		) {
			return false;
		}
		*pValue = (uint64)fValue;
		return true;
	#endif
}



/* 检查借用视图的数据指针与长度是否一致。 */
static bool __xrtValueViewValid(const void* pData, size_t iSize)
{
	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 检查值是否仍允许安全读取或增加引用。 */
static bool __xrtValueCanRead(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return true;
}



/* 判断当前项是否是保护列表中第一次出现的同一外壳。 */
static bool __xrtValueCallbackFirst(
	const xvalue* const* pValues,
	size_t iIndex
)
{
	for ( size_t i = 0; i < iIndex; i++ ) {
		if ( pValues[i] == pValues[iIndex] ) {
			return false;
		}
	}
	return true;
}



/* 在用户策略回调期间保护一组 Value 外壳。 */
bool __xrtValueCallbackProtect(
	const xvalue* const* pValues,
	size_t iCount
)
{
	if ( (pValues == NULL) && (iCount != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		const xvalue* pValue = pValues[i];

		if ( pValue == NULL ) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		if ( !__xrtValueCallbackFirst(pValues, i) ||
			 ((pValue->Flags & XRT_VALUE_FLAG_STATIC) != 0) ) {
			continue;
		}
		if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
			__xrtErrorSetInvalidState();
			return false;
		}
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xvalue* pValue = (xvalue*)pValues[i];

		if ( __xrtValueCallbackFirst(pValues, i) &&
			 ((pValue->Flags & XRT_VALUE_FLAG_STATIC) == 0) ) {
			pValue->Flags |= XRT_VALUE_FLAG_BUSY;
		}
	}
	return true;
}



/* 解除一组 Value 外壳的用户策略回调保护。 */
void __xrtValueCallbackUnprotect(
	const xvalue* const* pValues,
	size_t iCount
)
{
	if ( pValues == NULL ) {
		return;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		xvalue* pValue = (xvalue*)pValues[i];

		if ( (pValue != NULL) &&
			 __xrtValueCallbackFirst(pValues, i) &&
			 ((pValue->Flags & XRT_VALUE_FLAG_STATIC) == 0) ) {
			pValue->Flags &= ~XRT_VALUE_FLAG_BUSY;
		}
	}
}



/* 判断输出区间是否会覆盖值外壳或值拥有的可见字节。 */
static bool __xrtValueOutputValid(
	const xvalue* pValue,
	const void* pOutput,
	size_t iOutputSize
)
{
	size_t iBlobSize;

	if ( pOutput == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( __xrtRangesOverlap(
		pOutput,
		iOutputSize,
		pValue,
		sizeof(xvalue)
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if (
		(pValue->Type != XVALUE_STRING) &&
		(pValue->Type != XVALUE_BYTES)
	) {
		if (
			(pValue->Type == XVALUE_HANDLE) &&
			(pValue->Data.Handle.Data != NULL) &&
			__xrtRangesOverlap(
				pOutput,
				iOutputSize,
				pValue->Data.Handle.Data,
				1u
			)
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
		return true;
	}
	iBlobSize = pValue->Data.Blob.Size;
	if ( pValue->Type == XVALUE_STRING ) {
		iBlobSize++;
	}
	if ( __xrtRangesOverlap(
		pOutput,
		iOutputSize,
		pValue->Data.Blob.Data,
		iBlobSize
	) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 检查 Take 来源槽不会被重分配或清空自身所破坏。 */
static bool __xrtValueTakeSlotValid(
	const void* pSlot,
	const void* pData,
	size_t iDataSize
)
{
	if (
		(pData != NULL) &&
		__xrtRangesOverlap(pSlot, sizeof(ptr), pData, iDataSize)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 创建内联保存副本的字符串或二进制值。 */
static xvalue* __xrtValueBlob(
	xvaluetype Type,
	const void* pData,
	size_t iSize
)
{
	size_t iAllocationSize;
	xvalue* pValue;
	bytes pCopy;

	if ( !__xrtValueViewValid(pData, iSize) ) {
		return NULL;
	}
	if ( iSize > (SIZE_MAX - sizeof(xvalue) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iAllocationSize = sizeof(xvalue) + iSize + 1u;
	pValue = (xvalue*)xrtMalloc(iAllocationSize);
	if ( pValue == NULL ) {
		return NULL;
	}
	memset(pValue, 0, sizeof(xvalue));
	pValue->RefCount = 1;
	pValue->Type = (uint16)Type;
	pCopy = (bytes)(pValue + 1);
	if ( iSize != 0 ) {
		memcpy(pCopy, pData, iSize);
	}
	pCopy[iSize] = 0;
	pValue->Data.Blob.Data = pCopy;
	pValue->Data.Blob.Size = iSize;
	return pValue;
}



/* 检查值与输出参数后精确匹配一个类型。 */
static bool __xrtValueGetValid(
	const xvalue* pValue,
	xvaluetype Type,
	const void* pResult,
	size_t iResultSize
)
{
	if ( !__xrtValueCanRead(pValue) ) {
		return false;
	}
	if ( pValue->Type != (uint16)Type ) {
		__xrtErrorSetType();
		return false;
	}
	return __xrtValueOutputValid(pValue, pResult, iResultSize);
}



/* 为内部模块创建指定类型的零初始化值外壳。 */
xvalue* __xrtValueCreate(xvaluetype Type)
{
	xvalue* pValue;

	if ( (Type <= XVALUE_INVALID) || (Type > XVALUE_UINT) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	pValue = (xvalue*)xrtCalloc(1, sizeof(xvalue));
	if ( pValue == NULL ) {
		return NULL;
	}
	pValue->RefCount = 1;
	pValue->Type = (uint16)Type;
	return pValue;
}



/* 判断类型是否属于基础容器。 */
bool __xrtValueContainerType(xvaluetype Type)
{
	return (Type == XVALUE_ARRAY) || (Type == XVALUE_INT_MAP) ||
		(Type == XVALUE_SET) || (Type == XVALUE_OBJECT);
}



/* 返回进程期不可变的 null 单例。 */
XRT_API xvalue* xrtValueNull(void)
{
	return &__xrtValueNull;
}



/* 返回进程期不可变的布尔单例。 */
XRT_API xvalue* xrtValueBool(bool bValue)
{
	return bValue ? &__xrtValueTrue : &__xrtValueFalse;
}



/* 创建不可变的 64 位整数值。 */
XRT_API xvalue* xrtValueInt(int64 iValue)
{
	xvalue* pValue = __xrtValueCreate(XVALUE_INT);

	if ( pValue != NULL ) {
		pValue->Data.Int = iValue;
	}
	return pValue;
}



/* 创建不可变的 64 位无符号整数值。 */
XRT_API xvalue* xrtValueUInt(uint64 iValue)
{
	xvalue* pValue = __xrtValueCreate(XVALUE_UINT);

	if ( pValue != NULL ) {
		pValue->Data.UInt = iValue;
	}
	return pValue;
}



/* 创建不可变的双精度浮点值。 */
XRT_API xvalue* xrtValueFloat(double fValue)
{
	xvalue* pValue = __xrtValueCreate(XVALUE_FLOAT);

	if ( pValue != NULL ) {
		pValue->Data.Float = fValue;
	}
	return pValue;
}



/* 复制字节并创建带末尾零的字符串值。 */
XRT_API xvalue* xrtValueString(xstrview Text)
{
	return __xrtValueBlob(XVALUE_STRING, Text.Data, Text.Size);
}



/* 接管 XRT 分配的字符串并保证末尾零。 */
XRT_API xvalue* xrtValueStringTake(str* pText, size_t iSize)
{
	xvalue* pValue;
	str sOwned;

	if ( (pText == NULL) || ((*pText == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( iSize == SIZE_MAX ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	if ( !__xrtValueTakeSlotValid(pText, *pText, iSize + 1u) ) {
		return NULL;
	}
	pValue = __xrtValueCreate(XVALUE_STRING);
	if ( pValue == NULL ) {
		return NULL;
	}
	sOwned = (str)xrtRealloc(*pText, iSize + 1u);
	if ( sOwned == NULL ) {
		xrtFree(pValue);
		return NULL;
	}
	sOwned[iSize] = '\0';
	pValue->Flags |= XRT_VALUE_FLAG_OWNED_DATA;
	pValue->Data.Blob.Data = (cbytes)sOwned;
	pValue->Data.Blob.Size = iSize;
	*pText = NULL;
	return pValue;
}



/* 复制任意字节并创建二进制值。 */
XRT_API xvalue* xrtValueBytes(xbytesview Data)
{
	return __xrtValueBlob(XVALUE_BYTES, Data.Data, Data.Size);
}



/* 接管 XRT 分配的二进制块。 */
XRT_API xvalue* xrtValueBytesTake(bytes* pData, size_t iSize)
{
	xvalue* pValue;
	size_t iOwnedSize;

	if ( (pData == NULL) || ((*pData == NULL) && (iSize != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	iOwnedSize = iSize != 0 ? iSize : 1u;
	if ( !__xrtValueTakeSlotValid(pData, *pData, iOwnedSize) ) {
		return NULL;
	}
	pValue = __xrtValueCreate(XVALUE_BYTES);
	if ( pValue == NULL ) {
		return NULL;
	}
	pValue->Flags |= XRT_VALUE_FLAG_OWNED_DATA;
	pValue->Data.Blob.Data = *pData;
	pValue->Data.Blob.Size = iSize;
	*pData = NULL;
	return pValue;
}



/* 创建使用 Unix Epoch 微秒表示的时间值。 */
XRT_API xvalue* xrtValueTime(xtime Time)
{
	xvalue* pValue = __xrtValueCreate(XVALUE_TIME);

	if ( pValue != NULL ) {
		pValue->Data.Time = Time;
	}
	return pValue;
}



/* 创建不拥有目标生命周期的裸指针值。 */
XRT_API xvalue* xrtValuePointer(ptr pPointer)
{
	xvalue* pValue = __xrtValueCreate(XVALUE_POINTER);

	if ( pValue != NULL ) {
		pValue->Data.Pointer = pPointer;
	}
	return pValue;
}



/* 接管一个带显式生命周期的句柄。 */
XRT_API xvalue* xrtValueHandleTake(
	ptr* pHandle,
	const xvaluehandleops* pOps,
	ptr pUserData
)
{
	xvalue* pValue;

	if ( (pHandle == NULL) || (pOps == NULL) || (pOps->Drop == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( (pOps->Hash == NULL) != (pOps->Equal == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtValueTakeSlotValid(pHandle, *pHandle, 1u) ) {
		return NULL;
	}
	pValue = __xrtValueCreate(XVALUE_HANDLE);
	if ( pValue == NULL ) {
		return NULL;
	}
	pValue->Data.Handle.Data = *pHandle;
	pValue->Data.Handle.Ops = pOps;
	pValue->Data.Handle.UserData = pUserData;
	*pHandle = NULL;
	return pValue;
}



/* 增加值外壳引用并返回原指针。 */
XRT_API xvalue* xrtValueRetain(const xvalue* pValue)
{
	if ( !__xrtValueCanRead(pValue) ) {
		return NULL;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_STATIC) != 0 ) {
		return (xvalue*)pValue;
	}
	if ( xrtRefRetain((volatile int32*)&pValue->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return (xvalue*)pValue;
}



/* 释放值外壳引用和最后一个拥有资源。 */
XRT_API void xrtValueRelease(xvalue* pValue)
{
	int32 iReferences;

	if ( (pValue == NULL) || ((pValue->Flags & XRT_VALUE_FLAG_STATIC) != 0) ) {
		return;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	iReferences = xrtRefRelease(&pValue->RefCount);
	if ( iReferences < 0 ) {
		__xrtErrorSetInvalidState();
		return;
	}
	if ( iReferences != 0 ) {
		return;
	}
	pValue->Flags |= XRT_VALUE_FLAG_BUSY;
	if ( ((pValue->Type == XVALUE_STRING) || (pValue->Type == XVALUE_BYTES)) &&
		 ((pValue->Flags & XRT_VALUE_FLAG_OWNED_DATA) != 0) ) {
		xrtFree((ptr)pValue->Data.Blob.Data);
	} else if ( pValue->Type == XVALUE_HANDLE ) {
		pValue->Data.Handle.Ops->Drop(
			pValue->Data.Handle.Data,
			pValue->Data.Handle.UserData
		);
	#if defined(XRT_FEATURE_VALUE_CONTAINER)
	} else if ( __xrtValueContainerType((xvaluetype)pValue->Type) ) {
		__xrtValueContainerRelease(pValue);
	#endif
	}
	xrtFree(pValue);
}



/* 标量增加引用，容器创建共享 backing 的独立 COW 外壳。 */
XRT_API xvalue* xrtValueClone(const xvalue* pValue)
{
	if ( !__xrtValueCanRead(pValue) ) {
		return NULL;
	}
	#if defined(XRT_FEATURE_VALUE_CONTAINER)
		if ( __xrtValueContainerType((xvaluetype)pValue->Type) ) {
			return __xrtValueContainerClone(pValue);
		}
	#endif
	return xrtValueRetain(pValue);
}



/* 返回值类型，空指针返回 INVALID。 */
XRT_API xvaluetype xrtValueType(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		return XVALUE_INVALID;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return XVALUE_INVALID;
	}
	return (xvaluetype)pValue->Type;
}



/* 返回调用者绑定的不透明语义类型身份。 */
XRT_API uint64 xrtValueTypeId(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		return 0;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return 0;
	}
	return pValue->TypeId;
}



/* 一次性绑定非零语义类型身份，禁止共享值被重新解释。 */
XRT_API bool xrtValueTypeIdBind(xvalue* pValue, uint64 iTypeId)
{
	if ( (pValue == NULL) || (iTypeId == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pValue->Flags & (XRT_VALUE_FLAG_STATIC | XRT_VALUE_FLAG_BUSY)) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( (pValue->TypeId != 0) && (pValue->TypeId != iTypeId) ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pValue->TypeId = iTypeId;
	return true;
}



/* 只允许未发布且唯一拥有的外壳替换语义类型身份。 */
XRT_API bool xrtValueTypeIdRebind(xvalue* pValue, uint64 iTypeId)
{
	if ( (pValue == NULL) || (iTypeId == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pValue->Flags & (XRT_VALUE_FLAG_STATIC | XRT_VALUE_FLAG_BUSY)) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( pValue->TypeId == iTypeId ) {
		return true;
	}
	if ( pValue->RefCount != 1 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	pValue->TypeId = iTypeId;
	return true;
}



/* 返回稳定的类型名称。 */
XRT_API cstr xrtValueTypeName(xvaluetype Type)
{
	switch ( Type ) {
		case XVALUE_NULL: return "null";
		case XVALUE_BOOL: return "bool";
		case XVALUE_INT: return "int";
		case XVALUE_FLOAT: return "float";
		case XVALUE_STRING: return "string";
		case XVALUE_BYTES: return "bytes";
		case XVALUE_TIME: return "time";
		case XVALUE_POINTER: return "pointer";
		case XVALUE_HANDLE: return "handle";
		case XVALUE_ARRAY: return "array";
		case XVALUE_INT_MAP: return "int_map";
		case XVALUE_SET: return "set";
		case XVALUE_OBJECT: return "object";
		case XVALUE_UINT: return "uint";
		case XVALUE_INVALID:
		default:
			return "invalid";
	}
}



/* 判断值是否具有指定类型。 */
XRT_API bool xrtValueIs(const xvalue* pValue, xvaluetype Type)
{
	if ( pValue == NULL ) {
		return false;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return pValue->Type == (uint16)Type;
}



/* 判断值是否为整数或浮点数。 */
XRT_API bool xrtValueIsNumber(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		return false;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return (pValue->Type == XVALUE_INT) || (pValue->Type == XVALUE_UINT) ||
		(pValue->Type == XVALUE_FLOAT);
}



/* 判断值是否为四种基础容器之一。 */
XRT_API bool xrtValueIsContainer(const xvalue* pValue)
{
	if ( pValue == NULL ) {
		return false;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	return __xrtValueContainerType((xvaluetype)pValue->Type);
}



/* 按 xlang 语义返回值的真值。 */
XRT_API bool xrtValueTruthy(const xvalue* pValue)
{
	if ( (pValue == NULL) || (pValue->Type == XVALUE_NULL) ) {
		return false;
	}
	if ( (pValue->Flags & XRT_VALUE_FLAG_BUSY) != 0 ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	switch ( (xvaluetype)pValue->Type ) {
		case XVALUE_BOOL:
			return pValue->Data.Bool;
		case XVALUE_INT:
			return pValue->Data.Int != 0;
		case XVALUE_UINT:
			return pValue->Data.UInt != 0;
		case XVALUE_FLOAT:
			return pValue->Data.Float != 0.0;
		case XVALUE_STRING:
		case XVALUE_BYTES:
			return pValue->Data.Blob.Size != 0;
		case XVALUE_POINTER:
		case XVALUE_HANDLE:
		case XVALUE_TIME:
			return true;
		case XVALUE_ARRAY:
		case XVALUE_INT_MAP:
		case XVALUE_SET:
		case XVALUE_OBJECT:
			#if defined(XRT_FEATURE_VALUE_CONTAINER)
				return __xrtValueContainerCount(pValue) != 0;
			#else
				return false;
			#endif
		case XVALUE_NULL:
		case XVALUE_INVALID:
		default:
			return false;
	}
}



/* 精确读取布尔值。 */
XRT_API bool xrtValueGetBool(const xvalue* pValue, bool* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_BOOL,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.Bool;
	return true;
}



/* 精确读取整数值。 */
XRT_API bool xrtValueGetInt(const xvalue* pValue, int64* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_INT,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.Int;
	return true;
}



/* 精确读取无符号整数值。 */
XRT_API bool xrtValueGetUInt(const xvalue* pValue, uint64* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_UINT,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.UInt;
	return true;
}



/* 精确读取浮点值。 */
XRT_API bool xrtValueGetFloat(const xvalue* pValue, double* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_FLOAT,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.Float;
	return true;
}



/* 借用字符串视图。 */
XRT_API bool xrtValueGetString(const xvalue* pValue, xstrview* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_STRING,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	pResult->Data = (cstr)pValue->Data.Blob.Data;
	pResult->Size = pValue->Data.Blob.Size;
	return true;
}



/* 借用二进制视图。 */
XRT_API bool xrtValueGetBytes(const xvalue* pValue, xbytesview* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_BYTES,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	pResult->Data = pValue->Data.Blob.Data;
	pResult->Size = pValue->Data.Blob.Size;
	return true;
}



/* 精确读取时间值。 */
XRT_API bool xrtValueGetTime(const xvalue* pValue, xtime* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_TIME,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.Time;
	return true;
}



/* 精确读取不拥有目标的裸指针。 */
XRT_API bool xrtValueGetPointer(const xvalue* pValue, ptr* pResult)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_POINTER,
		pResult,
		sizeof(*pResult)
	) ) {
		return false;
	}
	*pResult = pValue->Data.Pointer;
	return true;
}



/* 借用句柄及其策略数据。 */
XRT_API bool xrtValueGetHandle(
	const xvalue* pValue,
	ptr* pHandle,
	const xvaluehandleops** pOps,
	ptr* pUserData
)
{
	if ( !__xrtValueGetValid(
		pValue,
		XVALUE_HANDLE,
		pHandle,
		sizeof(*pHandle)
	) ) {
		return false;
	}
	if (
		((pOps != NULL) && !__xrtValueOutputValid(
			pValue,
			pOps,
			sizeof(*pOps)
		)) ||
		((pUserData != NULL) && !__xrtValueOutputValid(
			pValue,
			pUserData,
			sizeof(*pUserData)
		)) ||
		((pOps != NULL) && __xrtRangesOverlap(
			pHandle,
			sizeof(*pHandle),
			pOps,
			sizeof(*pOps)
		)) ||
		((pUserData != NULL) && __xrtRangesOverlap(
			pHandle,
			sizeof(*pHandle),
			pUserData,
			sizeof(*pUserData)
		)) ||
		((pOps != NULL) && (pUserData != NULL) &&
		 __xrtRangesOverlap(
			pOps,
			sizeof(*pOps),
			pUserData,
			sizeof(*pUserData)
		 ))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pHandle = pValue->Data.Handle.Data;
	if ( pOps != NULL ) {
		*pOps = pValue->Data.Handle.Ops;
	}
	if ( pUserData != NULL ) {
		*pUserData = pValue->Data.Handle.UserData;
	}
	return true;
}



/* 不报告错误地计算已验证可哈希值。 */
uint64 __xrtValueHashKnown(const xvalue* pValue)
{
	uint64 iBits;
	uint64 iUnsigned;
	int64 iInteger;

	switch ( (xvaluetype)pValue->Type ) {
		case XVALUE_NULL:
			return __xrtValueTaggedHash(XVALUE_NULL, 0);
		case XVALUE_BOOL:
			return __xrtValueTaggedHash(XVALUE_BOOL, pValue->Data.Bool ? 1u : 0u);
		case XVALUE_INT:
			return __xrtValueTaggedHash(XVALUE_INT, (uint64)pValue->Data.Int);
		case XVALUE_UINT:
			return pValue->Data.UInt <= (uint64)INT64_MAX
				? __xrtValueTaggedHash(XVALUE_INT, pValue->Data.UInt)
				: __xrtValueTaggedHash(XVALUE_UINT, pValue->Data.UInt);
		case XVALUE_FLOAT:
			if ( __xrtValueFloatToInt(pValue->Data.Float, &iInteger) ) {
				return __xrtValueTaggedHash(XVALUE_INT, (uint64)iInteger);
			}
			if ( __xrtValueFloatToUInt(pValue->Data.Float, &iUnsigned) ) {
				return __xrtValueTaggedHash(XVALUE_UINT, iUnsigned);
			}
			if ( isnan(pValue->Data.Float) ) {
				iBits = UINT64_C(0x7FF8000000000000);
			} else {
				memcpy(&iBits, &pValue->Data.Float, sizeof(iBits));
			}
			return __xrtValueTaggedHash(XVALUE_FLOAT, iBits);
		case XVALUE_STRING:
			return __xrtValueTaggedHash(
				XVALUE_STRING,
				xrtHash64(pValue->Data.Blob.Data, pValue->Data.Blob.Size)
			);
		case XVALUE_BYTES:
			return __xrtValueTaggedHash(
				XVALUE_BYTES,
				xrtHash64(pValue->Data.Blob.Data, pValue->Data.Blob.Size)
			);
		case XVALUE_TIME:
			return __xrtValueTaggedHash(XVALUE_TIME, (uint64)pValue->Data.Time);
		case XVALUE_POINTER:
			return __xrtValueTaggedHash(
				XVALUE_POINTER,
				(uint64)(uintptr_t)pValue->Data.Pointer
			);
		case XVALUE_HANDLE:
			return __xrtValueTaggedHash(
				XVALUE_HANDLE,
				pValue->Data.Handle.Ops->Hash(
					pValue->Data.Handle.Data,
					pValue->Data.Handle.UserData
				)
			);
		default:
			return 0;
	}
}



/* 判断两个数值是否在无损转换后相等。 */
static bool __xrtValueNumberEqual(const xvalue* pLeft, const xvalue* pRight)
{
	int64 iInteger;
	uint64 iUnsigned;

	if ( (pLeft->Type == XVALUE_INT) && (pRight->Type == XVALUE_INT) ) {
		return pLeft->Data.Int == pRight->Data.Int;
	}
	if ( (pLeft->Type == XVALUE_UINT) && (pRight->Type == XVALUE_UINT) ) {
		return pLeft->Data.UInt == pRight->Data.UInt;
	}
	if ( (pLeft->Type == XVALUE_FLOAT) && (pRight->Type == XVALUE_FLOAT) ) {
		return (pLeft->Data.Float == pRight->Data.Float) ||
			(isnan(pLeft->Data.Float) && isnan(pRight->Data.Float));
	}
	if ( (pLeft->Type == XVALUE_INT) && (pRight->Type == XVALUE_UINT) ) {
		return pLeft->Data.Int >= 0 &&
			(uint64)pLeft->Data.Int == pRight->Data.UInt;
	}
	if ( (pLeft->Type == XVALUE_UINT) && (pRight->Type == XVALUE_INT) ) {
		return pRight->Data.Int >= 0 &&
			pLeft->Data.UInt == (uint64)pRight->Data.Int;
	}
	if ( pLeft->Type == XVALUE_FLOAT ) {
		if ( pRight->Type == XVALUE_UINT ) {
			return __xrtValueFloatToUInt(pLeft->Data.Float, &iUnsigned) &&
				iUnsigned == pRight->Data.UInt;
		}
		return __xrtValueFloatToInt(pLeft->Data.Float, &iInteger) &&
			iInteger == pRight->Data.Int;
	}
	if ( pLeft->Type == XVALUE_UINT ) {
		return __xrtValueFloatToUInt(pRight->Data.Float, &iUnsigned) &&
			pLeft->Data.UInt == iUnsigned;
	}
	return __xrtValueFloatToInt(pRight->Data.Float, &iInteger) &&
		iInteger == pLeft->Data.Int;
}



/* 不报告错误地比较两个已验证可哈希值。 */
bool __xrtValueEqualKnown(const xvalue* pLeft, const xvalue* pRight)
{
	if ( pLeft == pRight ) {
		return true;
	}
	if ( ((pLeft->Type == XVALUE_INT) || (pLeft->Type == XVALUE_UINT) ||
		  (pLeft->Type == XVALUE_FLOAT)) &&
		 ((pRight->Type == XVALUE_INT) || (pRight->Type == XVALUE_UINT) ||
		  (pRight->Type == XVALUE_FLOAT)) ) {
		return __xrtValueNumberEqual(pLeft, pRight);
	}
	if ( pLeft->Type != pRight->Type ) {
		return false;
	}
	switch ( (xvaluetype)pLeft->Type ) {
		case XVALUE_NULL:
			return true;
		case XVALUE_BOOL:
			return pLeft->Data.Bool == pRight->Data.Bool;
		case XVALUE_STRING:
		case XVALUE_BYTES:
			return (pLeft->Data.Blob.Size == pRight->Data.Blob.Size) &&
				((pLeft->Data.Blob.Size == 0) ||
				 (memcmp(
					pLeft->Data.Blob.Data,
					pRight->Data.Blob.Data,
					pLeft->Data.Blob.Size
				 ) == 0));
		case XVALUE_TIME:
			return pLeft->Data.Time == pRight->Data.Time;
		case XVALUE_POINTER:
			return pLeft->Data.Pointer == pRight->Data.Pointer;
		case XVALUE_HANDLE:
			return (pLeft->Data.Handle.Ops == pRight->Data.Handle.Ops) &&
				(pLeft->Data.Handle.UserData == pRight->Data.Handle.UserData) &&
				(pLeft->Data.Handle.Ops->Equal != NULL) &&
				pLeft->Data.Handle.Ops->Equal(
					pLeft->Data.Handle.Data,
					pRight->Data.Handle.Data,
					pLeft->Data.Handle.UserData
				);
		default:
			return false;
	}
}



/* 为可哈希标量计算稳定哈希。 */
XRT_API bool xrtValueHash(const xvalue* pValue, uint64* pHash)
{
	const xvalue* tValues[1];

	if ( !__xrtValueCanRead(pValue) ||
		!__xrtValueOutputValid(pValue, pHash, sizeof(*pHash)) ) {
		return false;
	}
	if ( __xrtValueContainerType((xvaluetype)pValue->Type) ||
		 ((pValue->Type == XVALUE_HANDLE) &&
		  ((pValue->Data.Handle.Ops->Hash == NULL) ||
		   (pValue->Data.Handle.Ops->Equal == NULL))) ) {
		__xrtErrorSetType();
		return false;
	}
	if ( pValue->Type == XVALUE_HANDLE ) {
		tValues[0] = pValue;
		if ( !__xrtValueCallbackProtect(tValues, 1) ) {
			return false;
		}
		*pHash = __xrtValueHashKnown(pValue);
		__xrtValueCallbackUnprotect(tValues, 1);
		return true;
	}
	*pHash = __xrtValueHashKnown(pValue);
	return true;
}



/* 按数值与标量内容判断两个值相等。 */
XRT_API bool xrtValueScalarEqual(
	const xvalue* pLeft,
	const xvalue* pRight
)
{
	const xvalue* tValues[2];
	bool bEqual;

	if ( !__xrtValueCanRead(pLeft) || !__xrtValueCanRead(pRight) ) {
		return false;
	}
	if (
		__xrtValueContainerType((xvaluetype)pLeft->Type) ||
		__xrtValueContainerType((xvaluetype)pRight->Type)
	) {
		__xrtErrorSetType();
		return false;
	}
	if (
		(pLeft != pRight) &&
		(
			((pLeft->Type == XVALUE_HANDLE) &&
			 (pLeft->Data.Handle.Ops->Equal == NULL)) ||
			((pRight->Type == XVALUE_HANDLE) &&
			 (pRight->Data.Handle.Ops->Equal == NULL))
		)
	) {
		__xrtErrorSetType();
		return false;
	}
	if ( (pLeft != pRight) &&
		 (pLeft->Type == XVALUE_HANDLE) &&
		 (pRight->Type == XVALUE_HANDLE) &&
		 (pLeft->Data.Handle.Ops == pRight->Data.Handle.Ops) &&
		 (pLeft->Data.Handle.UserData == pRight->Data.Handle.UserData) ) {
		tValues[0] = pLeft;
		tValues[1] = pRight;
		if ( !__xrtValueCallbackProtect(tValues, 2) ) {
			return false;
		}
		bEqual = __xrtValueEqualKnown(pLeft, pRight);
		__xrtValueCallbackUnprotect(tValues, 2);
		return bEqual;
	}
	return __xrtValueEqualKnown(pLeft, pRight);
}

#endif
