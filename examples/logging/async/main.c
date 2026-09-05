/*
 * 范例：logging/async —— 异步 Sink 包装：业务线程零阻塞落日志
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogAsyncConfigInit   异步配置（容量/字节上限等背压参数）
 *   xrtLogAsync             用后台线程包装任意同步 Sink
 *   xrtLogSinkSubmit        向 Sink 提交记录（异步下=入队）
 *   xrtLogSinkFlush         栅栏冲刷：排空队列并调用目标 Flush
 * 模块宏：XRT_MODULE_LOGGER（ASYNC 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/async/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   asynchronous sink ready
 *
 * 异步层的分工：
 *   业务线程 —— 只做"记录的有界深拷贝 + 入队"（微秒级），
 *               格式化与 IO 全部移交后台线程；
 *   后台线程 —— 按提交顺序逐条调用目标 Sink，Flush 栅栏保证
 *               "Flush 返回 = 此前全部记录已落盘"。
 * 背压参数：Capacity 256 条队列深度、ByteLimit 2MB 拷贝预算——
 *   生产远慢于消费时按上限丢弃/阻塞（策略可配），不会无限涨内存。
 * 包装器不依赖格式器/持久化：任何同步 Sink（控制台/文件/自定义）
 *   都能一行变异步。
 */

#include <xrt.h>

#include <stdio.h>
#include <string.h>



/* 目标 Sink：直接输出原始消息（异步层不关心格式）。 */
static xlogresult exampleLogAsyncWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	FILE* pFile = (FILE*)pUserData;

	if (
		fwrite(
			pRecord->Message.Data,
			1u,
			pRecord->Message.Size,
			pFile
		) != pRecord->Message.Size ||
		fputc('\n', pFile) == EOF
	) {
		return XLOG_RESULT_ERROR;
	}
	return XLOG_RESULT_WRITTEN;
}



/* 目标 Flush：由异步栅栏在工作线程上按序调用。 */
static bool exampleLogAsyncFlush(ptr pUserData)
{
	return fflush((FILE*)pUserData) == 0;
}



int main(void)
{
	xlogsinkconfig TargetConfig;
	xlogasyncconfig AsyncConfig;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pAsync;

	/* 先造同步目标 Sink（stdout 直写）。 */
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Name = XRT_STR_LITERAL("stdout");
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = exampleLogAsyncWrite;
	TargetConfig.Flush = exampleLogAsyncFlush;
	TargetConfig.UserData = stdout;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	if ( pTarget == NULL ) {
		return 1;
	}

	/* 配置背压参数后包装：Async 持有目标引用，本地引用可释放。 */
	if ( !xrtLogAsyncConfigInit(&AsyncConfig) ) {
		xrtLogSinkFree(pTarget);
		return 2;
	}
	AsyncConfig.Capacity = 256u;
	AsyncConfig.ByteLimit = 2u * 1024u * 1024u;
	pAsync = xrtLogAsync(pTarget, &AsyncConfig);
	xrtLogSinkFree(pTarget);
	if ( pAsync == NULL ) {
		return 3;
	}

	/* 提交 = 入队即返回；业务线程不等待写盘。 */
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("asynchronous sink ready");
	if ( xrtLogSinkSubmit(pAsync, &Record) != XLOG_RESULT_WRITTEN ) {
		xrtLogSinkFree(pAsync);
		return 4;
	}

	/* 栅栏冲刷：返回即保证该条已真实写出。 */
	if ( !xrtLogSinkFlush(pAsync) ) {
		xrtLogSinkFree(pAsync);
		return 5;
	}
	xrtLogSinkFree(pAsync);
	return 0;
}
