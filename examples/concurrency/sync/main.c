#include <stdio.h>

#include <xrt.h>



/* 展示嵌入式同步对象的基础用法。 */
int main(void)
{
	xmutex tMutex;

	if ( !xrtMutexInit(&tMutex) ) {
		return 1;
	}
	(void)xrtMutexLock(&tMutex);
	printf("protected section\n");
	(void)xrtMutexUnlock(&tMutex);
	(void)xrtMutexUnit(&tMutex);
	return 0;
}
