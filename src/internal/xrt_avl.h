#ifndef XRT_INTERNAL_AVL_H
#define XRT_INTERNAL_AVL_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_AVL_TREE)
	#include "xrt_pool.h"



	/* 新对象初始化器失败时自行清理部分状态，成功后可由回滚器撤销。 */
	typedef bool (*xavltreeinit)(ptr pItem, const void* pKey, ptr pUserData);
	typedef void (*xavltreerollback)(ptr pItem, ptr pUserData);



	/* 检查拥有式树的公开状态是否自洽。 */
	bool __xrtAVLTreeValid(const xavltree* pTree);



	/* 检查拥有式树当前是否允许修改结构和生命周期。 */
	bool __xrtAVLTreeCanMutate(const xavltree* pTree);



	/* 判断调用方字节区间是否触及树结构或固定池内部存储。 */
	bool __xrtAVLTreeOwnsRange(
		const xavltree* pTree,
		const void* pMemory,
		size_t iSize
	);



	/* 在受保护状态下调用对象释放器。 */
	void __xrtAVLTreeDropItem(xavltree* pTree, ptr pItem);



	/* 命中时直接返回已有对象，缺失时原地分配、清零并初始化新对象。 */
	ptr __xrtAVLTreeGetOrAdd(
		xavltree* pTree,
		const void* pKey,
		xavltreeinit pInit,
		ptr pInitUserData,
		xavltreerollback pRollback,
		ptr pRollbackUserData,
		bool* pNew
	);



	/* 删除对象并移交指定字节区间，不调用对象释放器。 */
	bool __xrtAVLTreeTakePart(
		xavltree* pTree,
		const void* pKey,
		size_t iOffset,
		size_t iSize,
		ptr pOutput
	);
#endif

#endif
