#include "../test.h"
#include "../test_thread.h"

#include <stdio.h>



#define TEST_LOG_ASYNC_THREADS 4u
#define TEST_LOG_ASYNC_RECORDS 500u



/* 多生产者测试目标只由 Async Sink 的唯一工作线程调用。 */
typedef struct testlogasynccollector {
	size_t Count;
	bool Invalid;
} testlogasynccollector;



/* 每个生产者保存共享 Sink 和自身稳定编号。 */
typedef struct testlogasyncproducer {
	xlogsink* Sink;
	uint32 Index;
} testlogasyncproducer;



/* 验证每条深拷贝记录保持完整的固定文本布局。 */
static xlogresult testLogAsyncThreadWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogasynccollector* pCollector = (testlogasynccollector*)pUserData;
	const char* sMessage = pRecord->Message.Data;

	if (
		(pRecord->Message.Size != 7u) ||
		(sMessage[0] != 'T') ||
		(sMessage[1] < '0') ||
		(sMessage[1] > '3') ||
		(sMessage[2] != '-') ||
		(sMessage[3] < '0') ||
		(sMessage[3] > '9') ||
		(sMessage[4] < '0') ||
		(sMessage[4] > '9') ||
		(sMessage[5] < '0') ||
		(sMessage[5] > '9') ||
		(sMessage[6] < '0') ||
		(sMessage[6] > '9')
	) {
		pCollector->Invalid = true;
	}
	pCollector->Count++;
	return XLOG_RESULT_WRITTEN;
}



/* 生产者反复复用栈缓冲，验证 BLOCK 策略不会丢失或借用原数据。 */
static int testLogAsyncProducerRun(ptr pData)
{
	testlogasyncproducer* pProducer = (testlogasyncproducer*)pData;
	xlogrecord Record;
	char sMessage[16];

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	for ( size_t i = 0; i < TEST_LOG_ASYNC_RECORDS; i++ ) {
		int iSize = snprintf(
			sMessage,
			sizeof(sMessage),
			"T%u-%04u",
			(unsigned int)pProducer->Index,
			(unsigned int)i
		);

		if ( iSize != 7 ) {
			return 1;
		}
		Record.Message = (xstrview){ sMessage, (size_t)iSize };
		if (
			xrtLogSinkSubmit(pProducer->Sink, &Record) !=
			XLOG_RESULT_WRITTEN
		) {
			return 2;
		}
		memset(sMessage, 'x', (size_t)iSize);
	}
	return 0;
}



/* 验证多生产者硬背压下的无损消费和有界队列统计。 */
int main(void)
{
	testlogasynccollector Collector;
	testlogasyncproducer arrProducer[TEST_LOG_ASYNC_THREADS];
	testthread arrThread[TEST_LOG_ASYNC_THREADS];
	xlogsinkconfig TargetConfig;
	xlogasyncconfig Config;
	xlogasyncstats Stats;
	xlogsink* pTarget;
	xlogsink* pAsync;

	memset(&Collector, 0, sizeof(Collector));
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Name = XRT_STR_LITERAL("thread-target");
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = testLogAsyncThreadWrite;
	TargetConfig.UserData = &Collector;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	testRequire(pTarget != NULL, "Logger async thread target create failed");
	testRequire(xrtLogAsyncConfigInit(&Config), "Logger async thread config failed");
	Config.Capacity = 16u;
	Config.RecordLimit = 256u;
	Config.ByteLimit = 512u;
	Config.Full = XLOG_ASYNC_BLOCK;
	pAsync = xrtLogAsync(pTarget, &Config);
	testRequire(pAsync != NULL, "Logger async thread sink create failed");

	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < TEST_LOG_ASYNC_THREADS; i++ ) {
		arrProducer[i].Sink = pAsync;
		arrProducer[i].Index = (uint32)i;
		arrThread[i].Proc = testLogAsyncProducerRun;
		arrThread[i].Data = &arrProducer[i];
	}
	testThreadsStart(arrThread, TEST_LOG_ASYNC_THREADS);
	testThreadsJoin(arrThread, TEST_LOG_ASYNC_THREADS);
	for ( size_t i = 0; i < TEST_LOG_ASYNC_THREADS; i++ ) {
		testRequire(
			arrThread[i].Result == 0,
			"Logger async producer failed"
		);
	}
	testRequire(xrtLogSinkFlush(pAsync), "Logger async thread flush failed");
	testRequire(
		!Collector.Invalid &&
		(Collector.Count ==
			(TEST_LOG_ASYNC_THREADS * TEST_LOG_ASYNC_RECORDS)),
		"Logger async concurrent records were lost or corrupted"
	);
	testRequire(
		xrtLogAsyncStats(pAsync, &Stats) &&
		(Stats.Enqueued == Collector.Count) &&
		(Stats.Processed == Collector.Count) &&
		(Stats.Written == Collector.Count) &&
		(Stats.DroppedNewest == 0u) &&
		(Stats.DroppedOldest == 0u) &&
		(Stats.Failed == 0u) &&
		(Stats.PeakQueued <= Config.Capacity) &&
		(Stats.PeakBytes <= Config.ByteLimit),
		"Logger async concurrent statistics mismatch"
	);
	xrtLogSinkFree(pAsync);
	xrtLogSinkFree(pTarget);
	printf("[PASS] Logger async threads\n");
	return 0;
}
