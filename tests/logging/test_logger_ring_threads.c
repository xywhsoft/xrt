#include "../test.h"
#include "../test_thread.h"



#define TEST_LOG_RING_THREADS 4u
#define TEST_LOG_RING_RECORDS 2000u



/* 单消费者目标校验多生产者复用栈缓冲后的消息布局。 */
typedef struct testlogringcollector {
	size_t Count;
	bool Invalid;
} testlogringcollector;



/* 每个生产者持有共享 Ring 和自身稳定编号。 */
typedef struct testlogringproducer {
	xlogsink* Sink;
	uint32 Index;
	size_t Written;
	size_t Dropped;
} testlogringproducer;



/* 校验每条固定格式消息没有发生撕裂或借用悬空。 */
static xlogresult testLogRingThreadWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogringcollector* pCollector = (testlogringcollector*)pUserData;
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



/* 生产者统计本地成功与容量丢弃，并立即覆写借用缓冲。 */
static int testLogRingProducerRun(ptr pData)
{
	testlogringproducer* pProducer = (testlogringproducer*)pData;
	xlogrecord Record;
	char sMessage[16];

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	for ( size_t i = 0; i < TEST_LOG_RING_RECORDS; i++ ) {
		int iSize = snprintf(
			sMessage,
			sizeof(sMessage),
			"T%u-%04u",
			(unsigned int)pProducer->Index,
			(unsigned int)i
		);
		xlogresult Result;

		if ( iSize != 7 ) {
			return 1;
		}
		Record.Message = (xstrview){ sMessage, (size_t)iSize };
		Result = xrtLogSinkSubmit(pProducer->Sink, &Record);
		if ( Result == XLOG_RESULT_WRITTEN ) {
			pProducer->Written++;
		} else if ( Result == XLOG_RESULT_DROPPED ) {
			pProducer->Dropped++;
		} else {
			return 2;
		}
		memset(sMessage, 'x', (size_t)iSize);
	}
	return 0;
}



/* 验证 MPSC 热路径在竞争和有界丢弃下不损坏数据与统计。 */
int main(void)
{
	testlogringcollector Collector;
	testlogringproducer arrProducer[TEST_LOG_RING_THREADS];
	testthread arrThread[TEST_LOG_RING_THREADS];
	xlogsinkconfig TargetConfig;
	xlogringconfig Config;
	xlogringstats Stats;
	xlogsink* pTarget;
	xlogsink* pRing;
	size_t iWritten = 0u;
	size_t iDropped = 0u;

	memset(&Collector, 0, sizeof(Collector));
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = testLogRingThreadWrite;
	TargetConfig.UserData = &Collector;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	testRequire(pTarget != NULL, "Logger ring thread target create failed");
	testRequire(xrtLogRingConfigInit(&Config), "Logger ring thread config failed");
	Config.Capacity = 1024u;
	Config.RecordLimit = 256u;
	Config.Batch = 128u;
	Config.IdleWait = 0u;
	pRing = xrtLogRing(pTarget, &Config);
	testRequire(pRing != NULL, "Logger ring thread sink create failed");

	memset(arrProducer, 0, sizeof(arrProducer));
	memset(arrThread, 0, sizeof(arrThread));
	for ( size_t i = 0; i < TEST_LOG_RING_THREADS; i++ ) {
		arrProducer[i].Sink = pRing;
		arrProducer[i].Index = (uint32)i;
		arrThread[i].Proc = testLogRingProducerRun;
		arrThread[i].Data = &arrProducer[i];
	}
	testThreadsStart(arrThread, TEST_LOG_RING_THREADS);
	testThreadsJoin(arrThread, TEST_LOG_RING_THREADS);
	for ( size_t i = 0; i < TEST_LOG_RING_THREADS; i++ ) {
		testRequire(arrThread[i].Result == 0, "Logger ring producer failed");
		iWritten += arrProducer[i].Written;
		iDropped += arrProducer[i].Dropped;
	}
	testRequire(xrtLogSinkFlush(pRing), "Logger ring thread flush failed");
	testRequire(!Collector.Invalid, "Logger ring concurrent record corrupted");
	testRequire(
		Collector.Count == iWritten,
		"Logger ring accepted record count mismatch"
	);
	testRequire(
		xrtLogRingStats(pRing, &Stats) &&
		(Stats.Enqueued == iWritten) &&
		(Stats.Processed == iWritten) &&
		(Stats.Written == iWritten) &&
		(Stats.Dropped == iDropped) &&
		(Stats.Oversized == 0u) &&
		(Stats.Failed == 0u) &&
		(Stats.Queued == 0u) &&
		(Stats.QueueBytes == 0u) &&
		(Stats.PeakQueued <= Config.Capacity),
		"Logger ring concurrent statistics mismatch"
	);
	xrtLogSinkFree(pRing);
	xrtLogSinkFree(pTarget);
	puts("[PASS] Logger ring threads");
	return 0;
}
