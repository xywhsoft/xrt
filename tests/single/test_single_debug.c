#define XRT_EXCLUDE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



#if !defined(XRT_FEATURE_MEMORY_DEBUG)
	#error "explicit memory debug selection must override the ALL exclusion"
#endif

#if defined(XRT_FEATURE_MEMORY_DEBUG_REPORT)
	#error "selecting memory debug alone must not enable the report layer"
#endif



/* 验证显式内存调试根模块优先于 MODULE_ALL 排除宏。 */
int main(void)
{
	xmemdebugsnapshot tSnapshot;
	ptr pMemory;

	pMemory = xrtMalloc(32);
	if ( pMemory == NULL ) {
		return 1;
	}
	xrtFree(pMemory);
	xrtMemDebugSnapshot(&tSnapshot);
	if ( (tSnapshot.AllocCount != 1) || (tSnapshot.FreeCount != 1) ) {
		return 2;
	}
	if ( !xrtMemDebugReset() ) {
		return 3;
	}
	return 0;
}
