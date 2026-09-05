/*
 * 范例：containers/queue_spsc —— 固定缓冲 SPSC 无锁环形队列（单线程演示）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSPSCQueueInitBuffer  绑定调用方槽位数组（运行期零分配）
 *   xrtSPSCQueueTryPush     尝试入队；满返回 XQUEUE_FULL
 *   xrtSPSCQueueClose       关闭队列：消费者排空后自然结束
 *   xrtSPSCQueueTryPop      尝试出队；关闭且空返回 XQUEUE_CLOSED
 *   xrtSPSCQueueUnit        归还句柄
 * 模块宏：XRT_MODULE_QUEUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/queue_spsc/main.c -lws2_32 -liphlpapi
 * 预期输出（FIFO）：
 *   10
 *   20
 *   30
 *
 * SPSC = Single-Producer Single-Consumer：专为一对一并发设计，
 *   无锁实现只需 acquire/release 语义，是三种无锁队列中最快的
 *   （基准：单条中位 2,880 万 items/s）。
 * 本例在单线程内演示协议；真实并发用法见 tests/ 下 _threads 变体。
 * 结果枚举：XQUEUE_OK / XQUEUE_FULL / XQUEUE_CLOSED——
 *   "满/关闭"是正常控制流，不是错误。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xspscqueue Queue;
	ptr Storage[8];          /* 槽位由调用方提供：地址稳定、零分配 */
	int pValues[] = { 10, 20, 30 };
	ptr pValue;

	/* 绑定外部槽位：容量 8，Push/Pop 全在这块内存上进行。 */
	if ( !xrtSPSCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}

	/* 依次入队三个 int 指针（队列搬运的是 ptr，不拥有对象）。 */
	for ( size_t i = 0; i < 3u; i++ ) {
		if ( xrtSPSCQueueTryPush(&Queue, &pValues[i]) != XQUEUE_OK ) {
			xrtSPSCQueueUnit(&Queue);
			return 2;
		}
	}

	/*
	 * 关闭：通知"不再有新数据"。此后 Pop 排空剩余元素后
	 * 返回 XQUEUE_CLOSED，循环自然终止——这是无锁队列的
	 * 标准停机协议（并发场景须等生产者全部返回再 Close）。
	 */
	xrtSPSCQueueClose(&Queue);
	while ( xrtSPSCQueueTryPop(&Queue, &pValue) == XQUEUE_OK ) {
		printf("%d\n", *(int*)pValue);
	}
	xrtSPSCQueueUnit(&Queue);
	return 0;
}
