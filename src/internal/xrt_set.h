#ifndef XRT_INTERNAL_SET_H
#define XRT_INTERNAL_SET_H

#include <xrt/set.h>



#if defined(XRT_FEATURE_SET)

/* 类型封装使用该回调把规范元素失败原子地移到调用方存储。 */
typedef bool (*xrtsetmoveproc)(ptr pTarget, ptr pSource, ptr pUserData);



/* 检查集合公开状态是否自洽，供集合类型薄封装复用。 */
bool __xrtSetValid(const xset* pSet);



/* 检查集合当前是否允许查询或推进外置迭代器。 */
bool __xrtSetCanRead(const xset* pSet);



/* 检查集合当前是否允许修改结构和生命周期。 */
bool __xrtSetCanMutate(xset* pSet);



/* 判断调用方字节区间是否触及集合结构、桶数组或元素条目。 */
bool __xrtSetOwnsRange(
	const xset* pSet,
	const void* pMemory,
	size_t iSize
);



/* 在用户回调期间拒绝当前集合的全部 API 重入。 */
bool __xrtSetCallbackBegin(const xset* pSet);
void __xrtSetCallbackEnd(const xset* pSet);



/* 使用类型移动器移出规范元素，成功后删除集合条目。 */
bool __xrtSetMoveOut(
	xset* pSet,
	const void* pItem,
	ptr pValue,
	xrtsetmoveproc pMove,
	ptr pUserData
);



/* 接管堆集合的全部存储并释放其外层结构。 */
bool __xrtSetAdoptHeap(xset* pTarget, xset* pSource);

#endif

#endif
