#ifndef XRT_MEMORY_STATS_H
#define XRT_MEMORY_STATS_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_MEMORY_STATS)

/* 全局堆固定使用 16 字节步长和 64 个池化尺寸类。 */
#define XRT_MEM_STATS_CLASS_STEP		16u
#define XRT_MEM_STATS_CLASS_CUTOFF	1024u
#define XRT_MEM_STATS_CLASS_COUNT	64u



/* 内存统计快照区分 API 请求和堆实际块流量。 */
typedef struct xmemstats {
	bool Enabled;
	uint32 ClassStep;
	uint32 ClassCutoff;
	uint32 ClassCount;
	uint64 MallocCalls;
	uint64 MallocBytes;
	uint64 CallocCalls;
	uint64 CallocBytes;
	uint64 ReallocCalls;
	uint64 ReallocBytes;
	uint64 MemDupCalls;
	uint64 MemDupBytes;
	uint64 FreeCalls;
	uint64 TempCalls;
	uint64 TempBytes;
	uint64 BlockAllocCalls;
	uint64 BlockAllocBytes;
	uint64 BlockFreeCalls;
	uint64 BlockFreeBytes;
	uint64 PooledAllocCalls;
	uint64 PooledAllocBytes;
	uint64 DirectAllocCalls;
	uint64 DirectAllocBytes;
	uint64 BackingAllocCalls;
	uint64 BackingAllocBytes;
	uint64 BackingReallocCalls;
	uint64 BackingReallocBytes;
	uint64 BackingFreeCalls;
	uint64 ClassCalls[XRT_MEM_STATS_CLASS_COUNT];
	uint64 ClassBytes[XRT_MEM_STATS_CLASS_COUNT];
} xmemstats;



XRT_EXTERN_C_BEGIN



/* 开启或关闭进程级内存统计。 */
XRT_API void xrtMemStatsEnable(bool bEnable);



/* 返回进程级内存统计是否开启。 */
XRT_API bool xrtMemStatsEnabled(void);



/* 在线性化边界清空所有内存统计。 */
XRT_API void xrtMemStatsReset(void);



/* 获取一份字段相互一致的内存统计快照。 */
XRT_API void xrtMemStatsGet(xmemstats* pStats);



XRT_EXTERN_C_END

#endif

#endif
