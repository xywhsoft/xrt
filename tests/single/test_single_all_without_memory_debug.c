#define XRT_MODULE_ALL
#define XRT_EXCLUDE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if defined(XRT_FEATURE_MEMORY_DEBUG)
	#error "XRT_EXCLUDE_MEMORY_DEBUG must exclude memory debug from MODULE_ALL"
#endif

#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT)
	#error "XRT_EXCLUDE_MEMORY_DEBUG must also exclude memory debug reports"
#endif

#if !defined(XRT_FEATURE_MEMORY_STATS)
	#error "XRT_EXCLUDE_MEMORY_DEBUG must preserve memory statistics"
#endif

#if !defined(XRT_FEATURE_STRING)
	#error "XRT_EXCLUDE_MEMORY_DEBUG must preserve unrelated MODULE_ALL features"
#endif

#if defined(xrtMalloc)
	#error "the non-debug allocator API must not be redirected to xrtMallocAt"
#endif



/* 验证完整单头可以只排除侵入式内存调试实现。 */
int main(void)
{
	ptr pMemory = xrtMalloc(32u);

	if ( pMemory == NULL ) {
		return 1;
	}
	xrtFree(pMemory);
	return 0;
}
