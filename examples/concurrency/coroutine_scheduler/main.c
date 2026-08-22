#include <xrt.h>
#include <stdio.h>



/* 示例任务异步等待后打印编号。 */
static ptr exampleTask(ptr pData)
{
	int iTask = (int)(intptr_t)pData;

	(void)xrtCoSleep((uint64)iTask * 1000u);
	printf("task %d\n", iTask);
	return pData;
}



/* 调度器 post 在所属线程中创建真正的协程任务。 */
static void examplePost(xcosched* pSched, ptr pData)
{
	(void)xrtCoGo(pSched, exampleTask, pData, NULL);
}



/* 先投递多个轻量任务，再运行到投递和协程全部完成。 */
int main(void)
{
	xcosched* pSched = xrtCoSchedCreate();

	if ( pSched == NULL ) {
		return 1;
	}
	for ( int i = 1; i <= 3; i++ ) {
		if ( !xrtCoSchedPost(
			pSched,
			examplePost,
			(ptr)(intptr_t)i
		) ) {
			return 2;
		}
	}
	if ( !xrtCoSchedRun(pSched) ) {
		return 3;
	}
	(void)xrtCoSchedDestroy(pSched);
	(void)xrtCoThreadDetach();
	return 0;
}
