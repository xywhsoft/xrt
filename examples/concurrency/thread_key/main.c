#include <stdio.h>

#include <xrt.h>



/* 释放线程退出时仍保存在键中的堆对象。 */
static void destroyValue(ptr pValue)
{
	xrtFree(pValue);
}



/*
 * 范例：concurrency/thread_key —— 线程局部存储：每线程一份值
 * ----------------------------------------------------------------
 * 演示 API：
 *   线程局部键创建 / 读写 / 销毁
 *   析构回调（线程退出时释放拥有值）
 * 模块宏：XRT_MODULE_THREAD
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/thread_key/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   thread value: 42
 *
 * TLS：每线程独立的存储槽——错误栈（xrtGetError）、
 *   线程 RNG 状态都建立在它上面；析构回调保证
 *   线程退出时资源不泄漏。
 */


/* 为当前线程创建、读取并销毁一个拥有所有权的局部值。 */
int main(void)
{
	xthreadkey* pKey = xrtThreadKeyCreate(destroyValue);
	int* pValue;

	if ( pKey == NULL ) {
		return 1;
	}
	pValue = (int*)xrtMalloc(sizeof(int));
	if ( pValue == NULL ) {
		xrtThreadKeyDestroy(pKey);
		return 1;
	}
	*pValue = 42;
	if ( !xrtThreadKeySet(pKey, pValue) ) {
		xrtFree(pValue);
		xrtThreadKeyDestroy(pKey);
		return 1;
	}
	printf("thread value: %d\n", *(int*)xrtThreadKeyGet(pKey));
	return xrtThreadKeyDestroy(pKey) ? 0 : 1;
}
