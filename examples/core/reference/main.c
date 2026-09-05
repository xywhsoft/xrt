/*
 * 范例：core/reference —— 用核心引用计数原语管理共享对象生命周期
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRefRetain   原子增加引用计数，返回新计数值（溢出保护）
 *   xrtRefRelease  原子减少引用计数，返回新计数值；归零者负责析构
 *   xrtMalloc / xrtFree  统一内存收口（创建/析构用）
 * 模块宏：XRT_MODULE_CORE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/core/reference/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value: 42
 *
 * 原语约定（core.h）：
 *   - 计数字段类型必须是 volatile int32（Retain/Release 以指针操作它）；
 *   - Retain 返回 -1 表示溢出保护触发（计数已经过大，拒绝继续增加）；
 *   - Release 返回 0 表示"你是最后一个持有者"，应当执行析构。
 *   XRT 全库不可变对象（xregex、xtemplate、x509 证书等）
 *   都建立在这对原语之上。
 */

#include <stdio.h>

#include <xrt.h>



/*
 * 示例对象：把引用计数内嵌为第一个字段。
 * volatile 是硬性要求——计数可能被多线程并发增减。
 */
typedef struct example_object {
	volatile int32 RefCount;
	int Value;
} example_object;



/*
 * 创建对象：初始引用计数为 1（创建者持有的那一票）。
 * 约定：Create 成功即"你拥有一个引用"，用完必须 Free 一次。
 */
static example_object* exampleCreate(int iValue)
{
	example_object* pObject = (example_object*)xrtMalloc(sizeof(example_object));

	if ( pObject != NULL ) {
		pObject->RefCount = 1;
		pObject->Value = iValue;
	}
	return pObject;
}



/*
 * 增加引用：第二个持有者接管对象前先 Retain。
 * 返回 -1 说明触发溢出保护（异常状态），按失败处理返回 NULL。
 */
static example_object* exampleRef(example_object* pObject)
{
	if ( (pObject == NULL) || (xrtRefRetain(&pObject->RefCount) < 0) ) {
		return NULL;
	}
	return pObject;
}



/*
 * 释放引用：Release 返回 0 的人是最后一个持有者，
 * 由它统一执行析构（这里就是 xrtFree）。
 * 先 Release 再判断，保证多线程下只有一个线程看到 0。
 */
static void exampleFree(example_object* pObject)
{
	if ( (pObject != NULL) && (xrtRefRelease(&pObject->RefCount) == 0) ) {
		xrtFree(pObject);
	}
}



int main(void)
{
	/* 第一个持有者：计数 1。 */
	example_object* pFirst = exampleCreate(42);

	/* 第二个持有者共享同一对象：Retain 后计数 2，两个指针等价。 */
	example_object* pSecond = exampleRef(pFirst);

	if ( pSecond == NULL ) {
		exampleFree(pFirst);
		return 1;
	}

	/* 通过第二个持有者读取：与第一个看到的是同一份对象。 */
	printf("value: %d\n", pSecond->Value);

	/*
	 * 两次释放：第一次后计数 1（对象仍存活），
	 * 第二次归零触发析构。顺序无关紧要——计数才是唯一事实。
	 */
	exampleFree(pSecond);
	exampleFree(pFirst);
	return 0;
}
