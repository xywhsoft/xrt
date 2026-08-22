#include <stdio.h>

#include <xrt.h>



/* 在线程中执行一个短任务。 */
static int32 worker(ptr pData)
{
	cstr sName = (cstr)pData;

	printf("worker %s: %llu\n", sName,
		(unsigned long long)xrtThreadCurrentId());
	return 42;
}



/* 创建、等待并释放线程。 */
int main(void)
{
	xthread* pThread = xrtThreadCreate(worker, (ptr)"alpha", 0);

	if ( pThread == NULL ) {
		return 1;
	}
	if ( xrtThreadWait(pThread) != XWAIT_OK ) {
		xrtThreadDestroy(pThread);
		return 1;
	}
	printf("exit: %d\n", xrtThreadExitCode(pThread));
	xrtThreadDestroy(pThread);
	return 0;
}
