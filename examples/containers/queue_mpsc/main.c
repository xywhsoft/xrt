/*
 * 范例：containers/queue_mpsc —— MPSC 队列的批量发布与批量消费
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtMPSCQueueInitBuffer  绑定调用方槽位数组（xqueueslot）
 *   xrtMPSCQueuePushBatch   一次发布多个指针，返回 xqueuebatchresult
 *   xrtMPSCQueueClose       关闭队列（等全部生产者返回后调用）
 *   xrtMPSCQueuePopBatch    一次取出最多 N 个指针到调用方数组
 * 模块宏：XRT_MODULE_QUEUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/containers/queue_mpsc/main.c -lws2_32 -liphlpapi
 * 预期输出（FIFO）：
 *   10
 *   20
 *   30
 *   40
 *
 * 批量的价值：MPSC 单条中位 813 万 items/s，批量 32 条时
 *   达 9,830 万 items/s——把每条消息的同步开销摊到整批。
 * PushBatch 返回结构含 Count（成功条数）与 Head/Tail 视图，
 *   满时只写入放得下的部分，调用方据此重试剩余。
 * 本例单线程演示协议；真实多生产者用法见 tests/ 下 _threads 变体。
 */

#include <stdio.h>
#include <xrt.h>



int main(void)
{
	xmpscqueue Queue;
	xqueueslot Storage[8];   /* MPSC 槽位类型与 SPSC 不同（含分配器指针） */
	int pValues[] = { 10, 20, 30, 40 };
	ptr pFirstBatch[] = { &pValues[0], &pValues[1] };
	ptr pSecondBatch[] = { &pValues[2], &pValues[3] };
	ptr pOutput[4];
	xqueuebatchresult Batch;

	if ( !xrtMPSCQueueInitBuffer(&Queue, Storage, 8u) ) {
		return 1;
	}

	/*
	 * 两批各 2 条：模拟两个生产者的批量发布。
	 * 返回值 Count 必须等于请求条数才认为整批成功
	 *（容量 8 足够，这里必然全成）。
	 */
	if ( xrtMPSCQueuePushBatch(&Queue, pFirstBatch, 2u).Count != 2u ) {
		xrtMPSCQueueUnit(&Queue);
		return 2;
	}
	if ( xrtMPSCQueuePushBatch(&Queue, pSecondBatch, 2u).Count != 2u ) {
		xrtMPSCQueueUnit(&Queue);
		return 3;
	}

	/* 实际并发程序必须等待全部生产者返回后再关闭。 */
	xrtMPSCQueueClose(&Queue);

	/*
	 * 一次最多取 4 条：返回结构与 Push 对称，
	 * Count 是实际取到的条数（关闭后不足额返回已有部分）。
	 */
	Batch = xrtMPSCQueuePopBatch(&Queue, pOutput, 4u);
	for ( size_t i = 0; i < Batch.Count; i++ ) {
		printf("%d\n", *(int*)pOutput[i]);
	}
	xrtMPSCQueueUnit(&Queue);
	return Batch.Count == 4u ? 0 : 4;
}
