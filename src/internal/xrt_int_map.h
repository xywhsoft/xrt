#ifndef XRT_INTERNAL_INT_MAP_H
#define XRT_INTERNAL_INT_MAP_H

#include "xrt_avl.h"



#if defined(XRT_FEATURE_INT_MAP)
	/* 检查整数映射公开状态是否自洽。 */
	bool __xrtIntMapValid(const xintmap* pMap);



	/* 检查整数映射当前是否允许修改结构和生命周期。 */
	bool __xrtIntMapCanMutate(const xintmap* pMap);



	/* 判断调用方字节区间是否触及映射结构或节点池存储。 */
	bool __xrtIntMapOwnsRange(
		const xintmap* pMap,
		const void* pMemory,
		size_t iSize
	);
#endif

#endif
