#ifndef XRT_INTERNAL_MEMORY_H
#define XRT_INTERNAL_MEMORY_H

#include "xrt_internal.h"



/* 全局堆块头的稳定内部标识。 */
#define XRT_HEAP_ALIGNMENT		16u
#define XRT_HEAP_MAGIC			0x584D454Du
#define XRT_HEAP_FLAG_POOLED		0x0001u
#define XRT_HEAP_FLAG_BACKING	0x0002u



/* 内存统计使用的 API 操作编号。 */
#define XRT_MEM_STATS_OP_MALLOC	1u
#define XRT_MEM_STATS_OP_CALLOC	2u
#define XRT_MEM_STATS_OP_REALLOC	3u
#define XRT_MEM_STATS_OP_MEMDUP	4u
#define XRT_MEM_STATS_OP_FREE	5u
#define XRT_MEM_STATS_OP_BACKING_ALLOC	6u
#define XRT_MEM_STATS_OP_BACKING_REALLOC	7u
#define XRT_MEM_STATS_OP_BACKING_FREE	8u



/* 调试释放明确区分错误、调试器接管和堆继续回收。 */
#define XRT_MEMDEBUG_FREE_INVALID	(-1)
#define XRT_MEMDEBUG_FREE_CONSUMED	0
#define XRT_MEMDEBUG_FREE_RECLAIM	1

#if defined(XRT_FEATURE_MEMORY_DEBUG)
	#define XRT_HEAP_FRONT_BOUNDARY_BYTES sizeof(uint32)
#else
	#define XRT_HEAP_FRONT_BOUNDARY_BYTES 0u
#endif



/* 每个分配块记录归属、大小和可选调试信息。 */
typedef struct xrt_heap_header {
	uint32 Magic;
	uint16 Class;
	uint16 Flags;
	size_t Size;
	ptr Allocation;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		struct xrt_heap_header* DebugPrev;
		struct xrt_heap_header* DebugNext;
		cstr AllocFile;
		cstr FreeFile;
		uint32 AllocLine;
		uint32 FreeLine;
		uint32 DebugState;
	#endif
} xrt_heap_header;



/* 返回包含显式前边界空间的对齐块头大小。 */
static inline size_t __xrtHeapHeaderSize(void)
{
	size_t iSize = sizeof(xrt_heap_header) + XRT_HEAP_FRONT_BOUNDARY_BYTES;

	return (iSize + (XRT_HEAP_ALIGNMENT - 1)) & ~((size_t)XRT_HEAP_ALIGNMENT - 1);
}



/* 锁定底层分配器并申请原始内存。 */
ptr __xrtBackingAlloc(size_t iSize);



/* 使用底层分配器调整原始内存。 */
ptr __xrtBackingRealloc(ptr pMemory, size_t iSize);



/* 使用底层分配器释放原始内存。 */
void __xrtBackingFree(ptr pMemory);



#if defined(XRT_FEATURE_MEMORY_DEBUG)
/* 返回调试尾部 canary 需要的额外字节。 */
size_t __xrtMemDebugTailSize(void);



/* 记录一个新分配并写入 canary。 */
void __xrtMemDebugAlloc(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine);



/* 检查池化空闲块是否在释放后被修改。 */
void __xrtMemDebugReuse(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine);



/* 验证并记录释放，返回明确的后续回收方式。 */
int __xrtMemDebugFree(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine);



/* 验证重分配前的块边界。 */
bool __xrtMemDebugCheck(xrt_heap_header* pHeader, ptr pMemory, cstr sFile, uint32 iLine);



/* 记录同一内存块上的大小变化。 */
void __xrtMemDebugResize(xrt_heap_header* pHeader, ptr pMemory, size_t iOldSize, cstr sFile, uint32 iLine);



/* 记录跨内存块完成的重分配。 */
void __xrtMemDebugRealloc(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine);



/* 记录无法识别的释放请求。 */
void __xrtMemDebugInvalidFree(ptr pMemory, cstr sFile, uint32 iLine);



/* 从活动分配或隔离队列中安全查找块头。 */
bool __xrtMemDebugFindHeader(ptr pMemory, xrt_heap_header** ppHeader);



/* 判断当前线程的下一次逻辑分配是否应当注入失败。 */
bool __xrtMemDebugShouldFailAlloc(void);



/* 捕获一份由底层分配器持有的完整活动分配快照。 */
bool __xrtMemDebugCaptureLive(xmemdebugallocation** ppAllocations, size_t* pCount);



/* 记录临时 arena 增加的对齐字节。 */
void __xrtMemDebugTempAlloc(size_t iSize, cstr sFile, uint32 iLine);



/* 记录临时 arena 回退或重置释放的对齐字节。 */
void __xrtMemDebugTempRelease(size_t iSize, xmemdebugeventkind Kind, cstr sFile, uint32 iLine);
#else
	#define __xrtMemDebugTailSize() 0u
	#define __xrtMemDebugAlloc(pHeader, pMemory, iCapacity, sFile, iLine) \
		((void)(pHeader), (void)(pMemory), (void)(iCapacity), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugReuse(pHeader, pMemory, iCapacity, sFile, iLine) \
		((void)(pHeader), (void)(pMemory), (void)(iCapacity), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugFree(pHeader, pMemory, iCapacity, sFile, iLine) \
		((void)(pHeader), (void)(pMemory), (void)(iCapacity), (void)(sFile), (void)(iLine), XRT_MEMDEBUG_FREE_RECLAIM)
	#define __xrtMemDebugCheck(pHeader, pMemory, sFile, iLine) \
		((void)(pHeader), (void)(pMemory), (void)(sFile), (void)(iLine), true)
	#define __xrtMemDebugResize(pHeader, pMemory, iOldSize, sFile, iLine) \
		((void)(pHeader), (void)(pMemory), (void)(iOldSize), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugRealloc(pMemory, iSize, sFile, iLine) \
		((void)(pMemory), (void)(iSize), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugInvalidFree(pMemory, sFile, iLine) \
		((void)(pMemory), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugShouldFailAlloc() false
	#define __xrtMemDebugTempAlloc(iSize, sFile, iLine) \
		((void)(iSize), (void)(sFile), (void)(iLine))
	#define __xrtMemDebugTempRelease(iSize, Kind, sFile, iLine) \
		((void)(iSize), (void)(sFile), (void)(iLine))
#endif



#if defined(XRT_FEATURE_MEMORY_STATS)
/* 记录一次公开内存 API 或底层分配器请求。 */
void __xrtMemStatsRecord(uint32 iOperation, size_t iSize);



/* 记录全局堆实际取得一个逻辑块。 */
void __xrtMemStatsBlockAlloc(size_t iSize, uint16 iClass, bool bPooled);



/* 记录全局堆实际释放一个逻辑块。 */
void __xrtMemStatsBlockFree(size_t iSize, uint16 iClass, bool bPooled);



/* 记录一次临时内存请求。 */
void __xrtMemStatsTemp(size_t iSize);
#else
	#define __xrtMemStatsRecord(iOperation, iSize) ((void)(iOperation), (void)(iSize))
	#define __xrtMemStatsBlockAlloc(iSize, iClass, bPooled) \
		((void)(iSize), (void)(iClass), (void)(bPooled))
	#define __xrtMemStatsBlockFree(iSize, iClass, bPooled) \
		((void)(iSize), (void)(iClass), (void)(bPooled))
	#define __xrtMemStatsTemp(iSize) ((void)(iSize))
#endif

#endif
