#include <stdio.h>

#include <xrt.h>



/* 示例协程分两段完成工作。 */
static ptr exampleCoroutine(ptr pData)
{
	int* pValue = (int*)pData;

	*pValue += 1;
	if ( xrtCoYield() != XWAIT_OK ) {
		return NULL;
	}
	*pValue *= 2;
	return pValue;
}



/*
 * 范例：concurrency/coroutine —— 有栈协程：创建、让出、返回值
 * ----------------------------------------------------------------
 * 演示 API：
 *   低级协程创建 / 恢复（Create/Resume 族）
 *   xrtCoYield   让出（下次恢复从让出点继续）
 *   终态与返回值读取（RETURNED + result）
 * 模块宏：XRT_MODULE_COROUTINE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/coroutine/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   after yield: 21
 *   result: 42
 *
 * 有栈协程：切换发生在让出点——函数局部状态跨让出
 *   存活（局部值 21 在 yield 后仍可读）；终态 RETURNED
 *   携带返回值 42。基准：切换 2,620 万次/秒（Win）。
 */


/* 展示低级协程的创建、恢复和结果读取。 */
int main(void)
{
	int iValue = 20;
	xcoro* pCo = xrtCoCreate(exampleCoroutine, &iValue, NULL);

	if ( pCo == NULL ) {
		return 1;
	}
	(void)xrtCoResume(pCo);
	printf("after yield: %d\n", iValue);
	(void)xrtCoResume(pCo);
	printf("result: %d\n", *(int*)xrtCoResult(pCo));
	(void)xrtCoDestroy(pCo);
	(void)xrtCoThreadDetach();
	return 0;
}
