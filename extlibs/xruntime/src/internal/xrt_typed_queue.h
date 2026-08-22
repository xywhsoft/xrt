#ifndef XRT_INTERNAL_TYPED_QUEUE_H
#define XRT_INTERNAL_TYPED_QUEUE_H

#include "xrt_internal.h"
#include <xrt/typed_queue.h>



#if defined(XRUNTIME_FEATURE_TYPED_QUEUE)

#define XRT_TYPED_QUEUE_STATE_EMPTY     0u
#define XRT_TYPED_QUEUE_STATE_READY     1u
#define XRT_TYPED_QUEUE_STATE_BROKEN    2u
#define XRT_TYPED_QUEUE_STATE_EXCLUSIVE 3u



/* 设置类型队列结构化错误。 */
void __xrtTypedQueueError(
	xerrkind Kind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 为下层类型或基础队列错误补充类型队列上下文。 */
void __xrtTypedQueueWrap(
	xerrkind DefaultKind,
	xtypedqueueerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 初始化固定数量的对齐值槽，但暂不把核心发布为可用。 */
bool __xrtTypedQueueCoreInit(
	xtypedqueuecore* pCore,
	const xrttype* pItemType,
	size_t iCapacity,
	cstr sOperation
);



/* 发布已经完成全部基础队列初始化的类型队列核心。 */
void __xrtTypedQueueCoreActivate(xtypedqueuecore* pCore);



/* 验证类型队列核心及其公开拥有者布局。 */
bool __xrtTypedQueueCoreValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
);



/* 进入和退出一次允许并发的类型值操作。 */
bool __xrtTypedQueueCoreEnter(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	cstr sOperation
);
void __xrtTypedQueueCoreLeave(xtypedqueuecore* pCore);



/* 独占核心并等待已经进入的操作退出；返回此前的稳定状态。 */
bool __xrtTypedQueueCoreExclusive(
	xtypedqueuecore* pCore,
	bool bAllowBroken,
	uint32* pPrevious,
	cstr sOperation
);



/* 结束临时独占并恢复此前的稳定状态。 */
void __xrtTypedQueueCoreShared(
	xtypedqueuecore* pCore,
	uint32 iPrevious
);



/* 标记不可恢复的内部基础队列状态错误。 */
void __xrtTypedQueueCoreBreak(
	xtypedqueuecore* pCore,
	cstr sOperation,
	cstr sMessage
);



/* 销毁每一个已初始化值槽并释放连续存储。 */
void __xrtTypedQueueCoreUnit(xtypedqueuecore* pCore);



/* 返回指定固定槽的地址。 */
ptr __xrtTypedQueueCell(xtypedqueuecore* pCore, size_t iIndex);



/* 验证单值或连续值区间不与队列对象和内部值槽重叠。 */
bool __xrtTypedQueueValueValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValue,
	cstr sOperation
);
bool __xrtTypedQueueValuesValid(
	const xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	const void* pValues,
	size_t iCount,
	cstr sOperation
);



/* 在内部值槽和外部已初始化值之间复制或移动所有权。 */
bool __xrtTypedQueueCopyIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	const void* pItem,
	cstr sOperation
);
bool __xrtTypedQueueMoveIn(
	const xtypedqueuecore* pCore,
	ptr pCell,
	ptr pItem,
	cstr sOperation
);
bool __xrtTypedQueueMoveOut(
	const xtypedqueuecore* pCore,
	ptr pValue,
	ptr pCell,
	cstr sOperation
);



/* 合并两个并发近似数量并限制在固定容量内。 */
size_t __xrtTypedQueueCount(
	const xtypedqueuecore* pCore,
	size_t iReady,
	size_t iRetry
);



/* 验证对象队列类型描述中的元素、容量和实例操作。 */
bool __xrtTypedQueueTypeValidate(
	const xrttype* pType,
	size_t iInstanceSize,
	size_t iInstanceAlign,
	const xrtinstanceops* pInstanceOps,
	cstr sOperation
);



/* 在独占状态下追踪全部固定值槽。 */
bool __xrtTypedQueueTrace(
	xtypedqueuecore* pCore,
	const void* pOwner,
	size_t iOwnerSize,
	xrtobjectvisitor pVisit,
	ptr pContext,
	cstr sOperation
);

#endif

#endif
