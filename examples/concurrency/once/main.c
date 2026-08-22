#include <stdio.h>

#include <xrt.h>



/* 延迟初始化一个进程级配置值。 */
static bool initializeConfig(ptr pData)
{
	*(int*)pData = 42;
	return true;
}



/* 多次访问只执行一次初始化过程。 */
int main(void)
{
	static xonce tOnce = XRT_ONCE_INIT;
	static int iConfig;

	if ( !xrtOnce(&tOnce, initializeConfig, &iConfig) ) {
		return 1;
	}
	printf("config: %d\n", iConfig);
	return 0;
}
