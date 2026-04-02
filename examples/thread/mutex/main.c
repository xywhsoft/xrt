/*
 * XRT Example - Mutex
 * XRT 范例 - 互斥体
 *
 * Description / 说明:
 *   EN: Demonstrates heap mutex and embedded mutex usage.
 *   CN: 演示堆分配互斥体与嵌入式互斥体两种用法。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_mutex.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_mutex -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"

typedef struct {
	xmutex hMutex;
	volatile int iValue;
} mutex_ctx;


static uint32 MutexWorker(ptr pParam)
{
	mutex_ctx* pCtx = (mutex_ctx*)pParam;
	int i;

	for ( i = 0; i < 1000; i++ ) {
		xrtMutexLock(pCtx->hMutex);
		pCtx->iValue++;
		xrtMutexUnlock(pCtx->hMutex);
	}

	return 0;
}



int main()
{
	xmutex_struct tEmbedded;
	xmutex hMutex = NULL;
	mutex_ctx tCtx = { 0 };
	xthread hA = NULL;
	xthread hB = NULL;
	bool bTryLock = FALSE;

	xrtInit();

	xrtMutexInit(&tEmbedded);
	xrtMutexLock(&tEmbedded);
	bTryLock = xrtMutexTryLock(&tEmbedded);
	xrtMutexUnlock(&tEmbedded);
	xrtMutexUnit(&tEmbedded);

	hMutex = xrtMutexCreate();
	tCtx.hMutex = hMutex;

	hA = xrtThreadCreate((ptr)MutexWorker, &tCtx, 0);
	hB = xrtThreadCreate((ptr)MutexWorker, &tCtx, 0);
	xrtThreadWait(hA);
	xrtThreadWait(hB);

	printf("embedded_try_lock_when_owned = %s\n", bTryLock ? "TRUE" : "FALSE");
	printf("protected_value = %d\n", tCtx.iValue);

	xrtThreadDestroy(hA);
	xrtThreadDestroy(hB);
	xrtMutexDestroy(hMutex);
	xrtUnit();
	return 0;
}
