#ifndef XRT_POOL_H
#define XRT_POOL_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_POOL) && !defined(XRT_FEATURE_POOL_PAGE)
	#error "XRT_FEATURE_POOL requires XRT_FEATURE_POOL_PAGE"
#endif

#if defined(XRT_FEATURE_MEMORY_POOL) && !defined(XRT_FEATURE_POOL)
	#error "XRT_FEATURE_MEMORY_POOL requires XRT_FEATURE_POOL"
#endif



#define XRT_POOL_PAGE_CAPACITY		256u
#define XRT_POOL_PAGE_BYTES_DEFAULT	65536u
#define XRT_POOL_ALIGNMENT_DEFAULT	16u
#define XRT_MEMPOOL_CLASS_STEP		16u
#define XRT_MEMPOOL_CUTOFF_DEFAULT	1024u



/* 内存池错误代码在 xrt.pool 域内稳定。 */
typedef enum xpoolerror {
	XPOOL_ERROR_INVALID_POINTER = 1,
	XPOOL_ERROR_NOT_ALLOCATED,
	XPOOL_ERROR_PAGE_FULL,
	XPOOL_ERROR_INVALID_ALIGNMENT,
	XPOOL_ERROR_INVALID_SIZE,
	XPOOL_ERROR_INDEX_OUT_OF_RANGE,
	XPOOL_ERROR_VISIT_ACTIVE,
	XPOOL_ERROR_INVALID_CAPACITY
} xpoolerror;



#if defined(XRT_FEATURE_POOL_PAGE)

/* 单页管理 1 到 256 个槽，状态和标记位不占用用户数据。 */
typedef struct xpoolpage {
	struct xpoolpage* Prev;
	struct xpoolpage* Next;
	struct xpoolpage* AvailablePrev;
	struct xpoolpage* AvailableNext;
	ptr Allocation;
	bytes Memory;
	ptr Parent;
	size_t ItemSize;
	size_t Stride;
	size_t Alignment;
	size_t MemorySize;
	uint64 Used[4];
	uint64 Marked[4];
	uint8 FreeList[XRT_POOL_PAGE_CAPACITY];
	uint16 Capacity;
	uint16 LiveCount;
	uint16 NextIndex;
	uint16 FreeCount;
	uint16 Flags;
} xpoolpage;



/* 单页信息区分用户大小、实际步长和当前槽状态。 */
typedef struct xpoolpageinfo {
	size_t ItemSize;
	size_t Stride;
	size_t Alignment;
	size_t LiveCount;
	size_t FreeCount;
	size_t Capacity;
} xpoolpageinfo;



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节对齐初始化一个空的 256 槽页。 */
XRT_API bool xrtPoolPageInit(xpoolpage* pPage, size_t iItemSize);



/* 使用指定的二次幂对齐初始化一个空的 256 槽页。 */
XRT_API bool xrtPoolPageInitAligned(xpoolpage* pPage, size_t iItemSize, size_t iAlignment);



/* 使用显式对齐和槽数初始化一个空页。 */
XRT_API bool xrtPoolPageInitLayout(
	xpoolpage* pPage,
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
);



/* 创建一个使用默认 16 字节对齐的 256 槽页。 */
XRT_API xpoolpage* xrtPoolPageCreate(size_t iItemSize);



/* 创建一个使用指定对齐的 256 槽页。 */
XRT_API xpoolpage* xrtPoolPageCreateAligned(size_t iItemSize, size_t iAlignment);



/* 创建一个使用显式对齐和槽数的页。 */
XRT_API xpoolpage* xrtPoolPageCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iCapacity
);



/* 释放页持有的槽内存，但不释放页结构。 */
XRT_API void xrtPoolPageUnit(xpoolpage* pPage);



/* 释放页持有的全部资源和页结构。 */
XRT_API void xrtPoolPageDestroy(xpoolpage* pPage);



/* 分配一个未初始化槽，页满时返回空指针并设置 XERR_AGAIN。 */
XRT_API ptr xrtPoolPageAlloc(xpoolpage* pPage);



/* 分配并清零一个槽。 */
XRT_API ptr xrtPoolPageCalloc(xpoolpage* pPage);



/* 安全释放一个活动槽，非法、跨页或重复释放均返回 false。 */
XRT_API bool xrtPoolPageFree(xpoolpage* pPage, ptr pMemory);



/* 按槽索引释放活动对象。 */
XRT_API bool xrtPoolPageFreeAt(xpoolpage* pPage, size_t iIndex);



/* 返回指定索引处的活动对象，空闲或越界时返回空指针。 */
XRT_API ptr xrtPoolPageGet(const xpoolpage* pPage, size_t iIndex);



/* 获取活动对象的槽索引。 */
XRT_API bool xrtPoolPageIndex(const xpoolpage* pPage, const void* pMemory, size_t* pIndex);



/* 判断指针当前是否属于该页的活动槽。 */
XRT_API bool xrtPoolPageOwns(const xpoolpage* pPage, const void* pMemory);



/* 将一个活动槽标记为本轮可达对象。 */
XRT_API bool xrtPoolPageMark(xpoolpage* pPage, ptr pMemory);



/* 释放未标记槽，并清除幸存槽的标记。 */
XRT_API size_t xrtPoolPageSweep(xpoolpage* pPage);



/* 释放已标记槽，适合显式批量选择释放。 */
XRT_API size_t xrtPoolPageFreeMarked(xpoolpage* pPage);



/* 将页内全部槽恢复为空闲状态并返回释放的活动槽数。 */
XRT_API size_t xrtPoolPageReset(xpoolpage* pPage);



/* 获取单页当前状态。 */
XRT_API void xrtPoolPageGetInfo(const xpoolpage* pPage, xpoolpageinfo* pInfo);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_POOL)

/* 固定对象池按目标字节数选择页槽数，默认保留一个空页。 */
typedef struct xpool {
	xpoolpage* Pages;
	xpoolpage* Available;
	xpoolpage** Index;
	size_t ItemSize;
	size_t Alignment;
	size_t PageCapacity;
	size_t PageCount;
	size_t EmptyPages;
	size_t LiveCount;
	size_t PeakCount;
	uint64 AllocCount;
	uint64 FreeCount;
	size_t IndexCapacity;
	size_t RetainEmpty;
	uint32 Flags;
} xpool;



/* 固定对象池信息用于容量、复用和泄漏诊断。 */
typedef struct xpoolinfo {
	size_t ItemSize;
	size_t Stride;
	size_t Alignment;
	size_t PageCapacity;
	size_t PageCount;
	size_t EmptyPages;
	size_t LiveCount;
	size_t PeakCount;
	size_t Capacity;
	uint64 AllocCount;
	uint64 FreeCount;
} xpoolinfo;



/* 活动对象访问器返回 false 时停止遍历。 */
typedef bool (*xpoolvisitor)(ptr pObject, size_t iIndex, ptr pUserData);



XRT_EXTERN_C_BEGIN



/* 使用默认 16 字节对齐初始化固定对象池。 */
XRT_API bool xrtPoolInit(xpool* pPool, size_t iItemSize);



/* 使用指定的二次幂对齐初始化固定对象池。 */
XRT_API bool xrtPoolInitAligned(xpool* pPool, size_t iItemSize, size_t iAlignment);



/* 使用显式对齐和每页槽数初始化固定对象池。 */
XRT_API bool xrtPoolInitLayout(
	xpool* pPool,
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
);



/* 创建使用默认 16 字节对齐的固定对象池。 */
XRT_API xpool* xrtPoolCreate(size_t iItemSize);



/* 创建使用指定对齐的固定对象池。 */
XRT_API xpool* xrtPoolCreateAligned(size_t iItemSize, size_t iAlignment);



/* 创建使用显式对齐和每页槽数的固定对象池。 */
XRT_API xpool* xrtPoolCreateLayout(
	size_t iItemSize,
	size_t iAlignment,
	size_t iPageCapacity
);



/* 释放池持有的全部页，但不释放池结构。 */
XRT_API void xrtPoolUnit(xpool* pPool);



/* 释放池持有的全部资源和池结构。 */
XRT_API void xrtPoolDestroy(xpool* pPool);



/* 分配一个未初始化对象。 */
XRT_API ptr xrtPoolAlloc(xpool* pPool);



/* 分配并清零一个对象。 */
XRT_API ptr xrtPoolCalloc(xpool* pPool);



/* 安全释放活动对象，非法、跨池或重复释放均返回 false。 */
XRT_API bool xrtPoolFree(xpool* pPool, ptr pObject);



/* 判断指针当前是否属于该池的活动对象。 */
XRT_API bool xrtPoolOwns(const xpool* pPool, const void* pObject);



/* 标记一个活动对象为本轮可达。 */
XRT_API bool xrtPoolMark(xpool* pPool, ptr pObject);



/* 释放全部未标记对象，并清除幸存对象标记。 */
XRT_API size_t xrtPoolSweep(xpool* pPool);



/* 释放全部已标记对象。 */
XRT_API size_t xrtPoolFreeMarked(xpool* pPool);



/* 释放全部活动对象，并按保留策略回收空页。 */
XRT_API size_t xrtPoolReset(xpool* pPool);



/* 回收多余空页，返回真正释放的页数。 */
XRT_API size_t xrtPoolTrim(xpool* pPool, size_t iRetainEmpty);



/* 设置自动保留的空页数，并立即执行一次裁剪。 */
XRT_API void xrtPoolSetRetain(xpool* pPool, size_t iRetainEmpty);



/* 获取固定对象池当前状态。 */
XRT_API void xrtPoolGet(const xpool* pPool, xpoolinfo* pInfo);



/* 访问活动对象；遍历期间只允许查询和标记，不得改变池的分配集合。 */
XRT_API size_t xrtPoolVisit(xpool* pPool, xpoolvisitor pVisitor, ptr pUserData);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_MEMORY_POOL)

typedef struct xmempoolbucket xmempoolbucket;
typedef struct xmempoollarge xmempoollarge;



/* 变长池使用 16 字节尺寸类，小块池化，大块单独登记。 */
typedef struct xmempool {
	xmempoolbucket* Buckets;
	xpoolpage** Pages;
	xmempoollarge* Large;
	size_t Cutoff;
	size_t ClassCount;
	size_t PageCount;
	size_t PageCapacity;
	size_t LargeCount;
	size_t LargeDeleted;
	size_t LargeCapacity;
	size_t LiveCount;
	size_t PeakCount;
	size_t LiveBytes;
	size_t PeakBytes;
	uint64 AllocCount;
	uint64 FreeCount;
	uint32 Flags;
} xmempool;



/* 变长池信息区分池化对象和独立大块。 */
typedef struct xmempoolinfo {
	size_t Cutoff;
	size_t ClassStep;
	size_t ClassCount;
	size_t PageCount;
	size_t SmallCount;
	size_t LargeCount;
	size_t LiveCount;
	size_t PeakCount;
	size_t LiveBytes;
	size_t PeakBytes;
	uint64 AllocCount;
	uint64 FreeCount;
} xmempoolinfo;



/* 活动块访问器接收可用大小和对齐，返回 false 时停止遍历。 */
typedef bool (*xmempoolvisitor)(
	ptr pMemory,
	size_t iSize,
	size_t iAlignment,
	ptr pUserData
);



XRT_EXTERN_C_BEGIN



/* 初始化变长池，cutoff 为零时使用默认值 1024。 */
XRT_API bool xrtMemPoolInit(xmempool* pPool, size_t iCutoff);



/* 创建变长池，cutoff 为零时使用默认值 1024。 */
XRT_API xmempool* xrtMemPoolCreate(size_t iCutoff);



/* 释放池持有的全部资源，但不释放池结构。 */
XRT_API void xrtMemPoolUnit(xmempool* pPool);



/* 释放池持有的全部资源和池结构。 */
XRT_API void xrtMemPoolDestroy(xmempool* pPool);



/* 按 16 字节对齐分配内存，大小为零时仍返回至少一个可用字节。 */
XRT_API ptr xrtMemPoolAlloc(xmempool* pPool, size_t iSize);



/* 乘法溢出时失败；总大小为零时仍分配并清零至少一个字节。 */
XRT_API ptr xrtMemPoolCalloc(xmempool* pPool, size_t iCount, size_t iSize);



/* 按指定二次幂对齐分配，零大小仍有效，超过 16 字节对齐时走独立大块。 */
XRT_API ptr xrtMemPoolAllocAligned(xmempool* pPool, size_t iSize, size_t iAlignment);



/* 调整池内块大小并保留已有内容，大小为零时释放。 */
XRT_API ptr xrtMemPoolRealloc(xmempool* pPool, ptr pMemory, size_t iSize);



/* 安全释放池内活动块，非法、跨池或重复释放均返回 false。 */
XRT_API bool xrtMemPoolFree(xmempool* pPool, ptr pMemory);



/* 返回活动块可安全使用的字节数，不属于该池时返回零。 */
XRT_API size_t xrtMemPoolSize(const xmempool* pPool, const void* pMemory);



/* 判断指针当前是否属于该池的活动块。 */
XRT_API bool xrtMemPoolOwns(const xmempool* pPool, const void* pMemory);



/* 标记一个活动块为本轮可达。 */
XRT_API bool xrtMemPoolMark(xmempool* pPool, ptr pMemory);



/* 释放全部未标记块，并清除幸存块标记。 */
XRT_API size_t xrtMemPoolSweep(xmempool* pPool);



/* 释放全部已标记块。 */
XRT_API size_t xrtMemPoolFreeMarked(xmempool* pPool);



/* 释放全部活动块，并保留每个尺寸类的一个空页。 */
XRT_API size_t xrtMemPoolReset(xmempool* pPool);



/* 将每个尺寸类的空页裁剪到指定数量。 */
XRT_API size_t xrtMemPoolTrim(xmempool* pPool, size_t iRetainEmptyPerClass);



/* 获取变长池当前状态。 */
XRT_API void xrtMemPoolGet(const xmempool* pPool, xmempoolinfo* pInfo);



/* 访问活动块；遍历期间只允许查询和标记，不得改变池的分配集合。 */
XRT_API size_t xrtMemPoolVisit(
	xmempool* pPool,
	xmempoolvisitor pVisitor,
	ptr pUserData
);



XRT_EXTERN_C_END

#endif

#endif
