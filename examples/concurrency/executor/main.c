#include <stdio.h>
#include <xrt.h>



/* 示例工作不需要返回值或 Future。 */
static void executorExampleRun(ptr pData)
{
	xatomic32* pCount = (xatomic32*)pData;

	(void)xrtAtomic32FetchAdd(pCount, 1, XMEMORY_RELAXED);
}



int main(void)
{
	xexecutor* pExecutor = xrtExecutorCreate(NULL);
	xatomic32 Count;

	xrtAtomic32Init(&Count, 0);
	if ( pExecutor == NULL ) {
		return 1;
	}
	for ( size_t i = 0; i < 1000; i++ ) {
		if ( !xrtExecutorSubmit(
			pExecutor,
			executorExampleRun,
			&Count,
			NULL,
			NULL
		) ) {
			(void)xrtExecutorCancel(pExecutor);
			(void)xrtExecutorDestroy(pExecutor);
			return 2;
		}
	}
	if ( !xrtExecutorDestroy(pExecutor) ) {
		return 3;
	}
	printf("completed: %u\n", xrtAtomic32Load(&Count, XMEMORY_ACQUIRE));
	return 0;
}
