/*
 * XRT Example - Semaphore
 * XRT 范例 - 信号量
 *
 * Description / 说明:
 *   EN: Demonstrates wait, post and post-multiple semaphore operations.
 *   CN: 演示等待、释放与批量释放信号量。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/thread_semaphore.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/thread_semaphore -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"


int main()
{
	xsem hSem = NULL;
	bool bTryWait = FALSE;

	xrtInit();

	hSem = xrtSemCreate(0, 4);
	bTryWait = xrtSemTryWait(hSem);

	xrtSemPost(hSem);
	xrtSemWait(hSem);

	xrtSemPostMultiple(hSem, 2);
	xrtSemWait(hSem);
	xrtSemWait(hSem);

	printf("try_wait_before_post = %s\n", bTryWait ? "TRUE" : "FALSE");
	printf("wait_timeout_after_drain = %d\n", xrtSemWaitTimeout(hSem, 10));

	xrtSemDestroy(hSem);
	xrtUnit();
	return 0;
}
