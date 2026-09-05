/*
 * 范例：containers/queue_mpmc —— MPMC 无锁队列：任意多对多的批量接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMPMCQueueInitBuffer  绑定调用方槽位数组
 *   xrtMPMCQueuePushBatch   批量发布（任意线程可并发调用）
 *   xrtMPMCQueueClose       关闭队列（全部生产者返回后调用）
 *   xrtMPMCQueuePopBatch    批量领取（任意线程可并发调用同一接口）
 * 模块宏：XRT_MODULE_QUEUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/queue_mpmc/main.c -lws2_32 -liphlpapi
 * 预期输出（FIFO）：
 *   10
 *   20
 *   30
 *   40
 *
 * MPMC = Multi-Producer Multi-Consumer：工作窃取、线程池任务分发的
 *   通用底座。单条中位 663 万 items/s，批量 32 条达 1.34 亿 items/s。
 * 与 SPSC/MPSC 的取舍：MPMC 通用性最强、单条路径最贵——
 *   能确定"一对一/多对一"拓扑时优先选更专用的队列。
 * 本例单线程演示协议；并发用法见 tests/ 下 _threads 变体。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xmpmcqueue Queue;
	xqueueslot Storage[8];
	int pValues[] = { 10, 20, 30, 40 };
	ptr pFirstBatch[] = { &pValues[0], &pValues[1] };
	ptr pSecondBatch[] = { &pValues[2], &pValues[3] };
	ptr pOutput[4];
	xqueuebatchresult Batch;

	/* 绑定外部槽位；MPMC 槽位与 MPSC 同类型（xqueueslot）。 */
	if ( !xrtMPMCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}

	/* 两批各 2 条：并发场景下任意线程都可这样批量发布。 */
	if ( xrtMPMCQueuePushBatch(&Queue, pFirstBatch, 2u).Count != 2u ) {
		xrtMPMCQueueUnit(&Queue);
		return 2;
	}
	if ( xrtMPMCQueuePushBatch(&Queue, pSecondBatch, 2u).Count != 2u ) {
		xrtMPMCQueueUnit(&Queue);
		return 3;
	}

	/*
	 * 关闭停机：并发程序必须在确认全部生产者结束后调用；
	 * 已入队数据仍可被消费者继续取走。
	 */
	xrtMPMCQueueClose(&Queue);

	/*
	 * 一次领取 4 条。实际程序可让多个消费者线程
	 * 并发执行同一个 PopBatch——队列内部保证每条消息只被领取一次。
	 */
	Batch = xrtMPMCQueuePopBatch(&Queue, pOutput, 4u);
	for ( size_t i = 0; i < Batch.Count; i++ ) {
		printf("%d\n", *(int*)pOutput[i]);
	}
	xrtMPMCQueueUnit(&Queue);
	return Batch.Count == 4u ? 0 : 4;
}
