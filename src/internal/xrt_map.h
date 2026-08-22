#ifndef XRT_INTERNAL_MAP_H
#define XRT_INTERNAL_MAP_H

#include <xrt/map.h>



#if defined(XRT_FEATURE_MAP)

/* 类型封装使用该回调把映射值失败原子地移到调用方存储。 */
typedef bool (*xrtmapmoveproc)(ptr pTarget, ptr pSource, ptr pUserData);



/* 类型封装使用该回调失败原子地替换一个完整初始化值。 */
typedef bool (*xrtmapreplaceproc)(
	ptr pTarget,
	const void* pSource,
	ptr pUserData
);



/* 检查字节键映射公开状态是否自洽。 */
bool __xrtMapValid(const xmap* pMap);



/* 检查字节键映射当前是否允许查询和推进迭代器。 */
bool __xrtMapCanRead(const xmap* pMap);



/* 检查字节键映射当前是否允许修改结构和生命周期。 */
bool __xrtMapCanMutate(xmap* pMap);



/* 判断调用方字节区间是否触及映射结构、桶数组或条目。 */
bool __xrtMapOwnsRange(
	const xmap* pMap,
	const void* pMemory,
	size_t iSize
);



/* 判断映射是否仍使用精确二进制默认键策略。 */
bool __xrtMapUsesDefaultKeyPolicy(const xmap* pMap);



/* 在多步只读操作期间阻止回调修改映射结构。 */
bool __xrtMapProtectRead(const xmap* pMap, bool* pAcquired);
void __xrtMapUnprotectRead(const xmap* pMap, bool bAcquired);



/* 在用户回调期间拒绝当前映射的全部 API 重入。 */
bool __xrtMapCallbackBegin(const xmap* pMap);
void __xrtMapCallbackEnd(const xmap* pMap);



/* 在回调门禁内失败原子地替换已有值，并报告键是否存在。 */
bool __xrtMapReplaceValue(
	xmap* pMap,
	xbytesview Key,
	const void* pValue,
	xrtmapreplaceproc pReplace,
	ptr pUserData,
	bool* pFound
);



/* 使用类型移动器移出指定键的值，成功后删除映射条目。 */
bool __xrtMapMoveOut(
	xmap* pMap,
	xbytesview Key,
	ptr pValue,
	xrtmapmoveproc pMove,
	ptr pUserData
);

#endif

#endif
