#define XRT_MODULE_EXECUTOR
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



static void testSingleExecutorRun(ptr pData)
{
	xatomic32* pCount = (xatomic32*)pData;

	(void)xrtAtomic32FetchAdd(pCount, 1, XMEMORY_RELAXED);
}



int main(void)
{
	xexecutorconfig Config = { 2, 8, 0 };
	xatomic32 Count;
	xexecutor* pExecutor;

	xrtAtomic32Init(&Count, 0);
	pExecutor = xrtExecutorCreate(&Config);
	if ( pExecutor == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < 8; i++ ) {
		if ( !xrtExecutorSubmit(
			pExecutor,
			testSingleExecutorRun,
			&Count,
			NULL,
			NULL
		) ) {
			return 2;
		}
	}
	if ( !xrtExecutorDestroy(pExecutor) ) {
		return 3;
	}
	return xrtAtomic32Load(&Count, XMEMORY_ACQUIRE) == 8 ? 0 : 4;
}
