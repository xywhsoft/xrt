#include "../internal/xrt_runtime_type.h"
#include <xrt/runtime_type_future.h>



#if defined(XRUNTIME_FEATURE_RUNTIME_TYPE_FUTURE)

/* 初始化一个拥有 Future 引用的槽位为空。 */
static bool __xrtRuntimeTypeFutureInit(
	ptr pValue,
	const xrttype* pType
)
{
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	return true;
}



/* 增加源 Future 引用，成功后替换目标槽。 */
static bool __xrtRuntimeTypeFutureCopy(
	ptr pTarget,
	const void* pSource,
	const xrttype* pType
)
{
	xfuture* pSourceFuture;
	xfuture* pTargetFuture;
	xfuture* pReference = NULL;
	(void)pType;

	memcpy(&pSourceFuture, pSource, sizeof(pSourceFuture));
	if ( pSourceFuture != NULL ) {
		pReference = xrtFutureRef(pSourceFuture);
		if ( pReference == NULL ) {
			return false;
		}
	}
	memcpy(&pTargetFuture, pTarget, sizeof(pTargetFuture));
	memcpy(pTarget, &pReference, sizeof(pReference));
	xrtFutureDestroy(pTargetFuture);
	return true;
}



/* 移交 Future 引用，清空源槽并释放目标旧引用。 */
static bool __xrtRuntimeTypeFutureMove(
	ptr pTarget,
	ptr pSource,
	const xrttype* pType
)
{
	xfuture* pSourceFuture;
	xfuture* pTargetFuture;
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(&pSourceFuture, pSource, sizeof(pSourceFuture));
	memcpy(&pTargetFuture, pTarget, sizeof(pTargetFuture));
	memcpy(pTarget, &pSourceFuture, sizeof(pSourceFuture));
	memcpy(pSource, &pEmpty, sizeof(pEmpty));
	xrtFutureDestroy(pTargetFuture);
	return true;
}



/* 释放槽位拥有的 Future 引用并恢复为空。 */
static void __xrtRuntimeTypeFutureDrop(
	ptr pValue,
	const xrttype* pType
)
{
	xfuture* pFuture;
	xfuture* pEmpty = NULL;
	(void)pType;

	memcpy(&pFuture, pValue, sizeof(pFuture));
	memcpy(pValue, &pEmpty, sizeof(pEmpty));
	xrtFutureDestroy(pFuture);
}



/* Future 槽按进程内稳定身份比较。 */
static int __xrtRuntimeTypeFutureCompare(
	const void* pLeft,
	const void* pRight,
	const xrttype* pType
)
{
	xfuture* pLeftFuture;
	xfuture* pRightFuture;
	uintptr_t iLeft;
	uintptr_t iRight;
	(void)pType;

	memcpy(&pLeftFuture, pLeft, sizeof(pLeftFuture));
	memcpy(&pRightFuture, pRight, sizeof(pRightFuture));
	iLeft = (uintptr_t)pLeftFuture;
	iRight = (uintptr_t)pRightFuture;
	return (iLeft > iRight) - (iLeft < iRight);
}



/* Future 槽按进程内稳定身份散列。 */
static uint64 __xrtRuntimeTypeFutureHash(
	const void* pValue,
	const xrttype* pType
)
{
	xfuture* pFuture;
	(void)pType;

	memcpy(&pFuture, pValue, sizeof(pFuture));
	return (uint64)(uintptr_t)pFuture;
}



static const xrttypeops __xrtRuntimeTypeFutureOps = {
	.Init = __xrtRuntimeTypeFutureInit,
	.Copy = __xrtRuntimeTypeFutureCopy,
	.Move = __xrtRuntimeTypeFutureMove,
	.Drop = __xrtRuntimeTypeFutureDrop,
	.Clone = __xrtRuntimeTypeFutureCopy,
	.Compare = __xrtRuntimeTypeFutureCompare,
	.Hash = __xrtRuntimeTypeFutureHash
};



static const xrttype __xrtRuntimeTypeFuture = {
	.Id = UINT64_C(0x144E843036511A0E),
	.Kind = XRT_TYPE_FUTURE,
	.Flags = XRT_TYPE_FLAG_COPYABLE | XRT_TYPE_FLAG_REFERENCE |
		XRT_TYPE_FLAG_NULLABLE | XRT_TYPE_FLAG_FINAL |
		XRT_TYPE_FLAG_RELOCATABLE,
	.Name = XRT_STR_INIT("future"),
	.AbiName = XRT_STR_INIT("xrt.future"),
	.Size = sizeof(xfuture*),
	.Align = XRT_INTERNAL_ALIGNOF(xfuture*),
	.InstanceSize = 0u,
	.InstanceAlign = 1u,
	.Ops = &__xrtRuntimeTypeFutureOps
};



/* 返回 Future 消费端引用槽的稳定运行时类型。 */
XRT_API const xrttype* xrtTypeFuture(void)
{
	return &__xrtRuntimeTypeFuture;
}

#endif
