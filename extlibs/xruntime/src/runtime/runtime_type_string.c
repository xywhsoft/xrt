#include "../internal/xrt_runtime_type.h"

#include <xrt/hash.h>
#include <xrt/runtime_type_string.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_STRING)

/* 初始化一个拥有型字符串槽为空字符串。 */
static bool __xrtRuntimeTypeStringInit(
	ptr pValue,
	const xrttype* pType
)
{
	str sEmpty = NULL;
	(void)pType;

	memcpy(pValue, &sEmpty, sizeof(sEmpty));
	return true;
}



/* 复制来源字符串，成功后替换目标拥有的旧字符串。 */
static bool __xrtRuntimeTypeStringCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	str sSource;
	str sTarget;
	str sCopy = NULL;
	(void)pType;

	memcpy(&sSource, pSource, sizeof(sSource));
	if ( sSource != NULL ) {
		sCopy = xrtStrDup(sSource);
		if ( sCopy == NULL ) {
			return false;
		}
	}
	memcpy(&sTarget, pTarget, sizeof(sTarget));
	memcpy(pTarget, &sCopy, sizeof(sCopy));
	xrtFree(sTarget);
	return true;
}



/* 移交字符串所有权，清空来源并释放目标旧字符串。 */
static bool __xrtRuntimeTypeStringMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	str sSource;
	str sTarget;
	str sEmpty = NULL;
	(void)pType;

	memcpy(&sSource, pSource, sizeof(sSource));
	memcpy(&sTarget, pTarget, sizeof(sTarget));
	memcpy(pTarget, &sSource, sizeof(sSource));
	memcpy(pSource, &sEmpty, sizeof(sEmpty));
	xrtFree(sTarget);
	return true;
}



/* 释放字符串槽并恢复为空字符串。 */
static void __xrtRuntimeTypeStringDrop(
	ptr pValue,
	const xrttype* pType
)
{
	str sValue;
	str sEmpty = NULL;
	(void)pType;

	memcpy(&sValue, pValue, sizeof(sValue));
	memcpy(pValue, &sEmpty, sizeof(sEmpty));
	xrtFree(sValue);
}



/* 按无符号字节词典序比较两个字符串槽。 */
static int __xrtRuntimeTypeStringCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	str sLeft;
	str sRight;
	(void)pType;

	memcpy(&sLeft, pLeft, sizeof(sLeft));
	memcpy(&sRight, pRight, sizeof(sRight));
	return xrtStrCompare(xrtStrView(sLeft), xrtStrView(sRight));
}



/* 按字符串内容计算确定性的 64 位散列。 */
static uint64 __xrtRuntimeTypeStringHash(
	const void* pValue,
	const xrttype* pType
)
{
	str sValue;
	xstrview Text;
	(void)pType;

	memcpy(&sValue, pValue, sizeof(sValue));
	Text = xrtStrView(sValue);
	return xrtHash64(Text.Data, Text.Size);
}



static const xrttypeops __xrtRuntimeTypeStringOps = {
	.Init = __xrtRuntimeTypeStringInit,
	.Copy = __xrtRuntimeTypeStringCopy,
	.Move = __xrtRuntimeTypeStringMove,
	.Drop = __xrtRuntimeTypeStringDrop,
	.Clone = __xrtRuntimeTypeStringCopy,
	.Compare = __xrtRuntimeTypeStringCompare,
	.Hash = __xrtRuntimeTypeStringHash
};



static const xrttype __xrtRuntimeTypeString = {
	.Id = UINT64_C(0xFD3B54B7F64A3170),
	.Kind = XRT_TYPE_STRING,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("string"),
	.AbiName = XRT_STR_INIT("xrt.string"),
	.Size = sizeof(str),
	.Align = XRT_INTERNAL_ALIGNOF(str),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtRuntimeTypeStringOps
};



/* 返回拥有型 XRT 字符串槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeString(void)
{
	return &__xrtRuntimeTypeString;
}

#endif
