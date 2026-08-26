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



/* 仅供拥有型封装配置最终值释放顺序；键迭代顺序不变。 */
bool __xrtMapSetDropReverse(xmap* pMap, bool bReverse);
bool __xrtMapDropsReverse(const xmap* pMap);



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



/* 一次查询完成已有值替换或缺失值初始化，并报告是否新建。 */
ptr __xrtMapSetOrInit(
	xmap* pMap,
	xbytesview Key,
	const void* pValue,
	xrtmapreplaceproc pReplace,
	ptr pReplaceData,
	xmapinit pInit,
	ptr pInitData,
	bool* pNew
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
