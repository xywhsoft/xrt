#include "../bench_common.h"

#define XRT_MODULE_LOGGER_RING
#define XRT_IMPLEMENTATION
#include "../../../single/xrt.h"



/* 最小目标只统计已经由 Ring 工作线程消费的记录。 */
typedef struct benchlogringtarget {
	uint64 Count;
} benchlogringtarget;



/* 消费一条记录，不把格式化或设备写入成本混入 Ring 调度基准。 */
static xlogresult benchLogRingWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	benchlogringtarget* pTarget = (benchlogringtarget*)pUserData;

	if ( (pRecord == NULL) || (pTarget == NULL) ) {
		return XLOG_RESULT_ERROR;
	}
	pTarget->Count++;
	return XLOG_RESULT_WRITTEN;
}



/* 目标没有外部缓冲，刷新始终立即成功。 */
static bool benchLogRingFlush(ptr pUserData)
{
	return pUserData != NULL;
}



/* 测量单生产者饱和提交、固定槽复制、批量消费和 Flush 屏障。 */
int main(int argc, char** argv)
{
	uint32 iCount = xbenchArgU32(argc, argv, 1, 1000000u);
	uint32 iCapacity = xbenchArgU32(argc, argv, 2, 4096u);
	benchlogringtarget Target;
	xlogsinkconfig TargetConfig;
	xlogringconfig RingConfig;
	xlogringstats Stats;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pRing;
	xbenchtimer Timer;
	uint64 iElapsed;
	uint64 iRetries = 0u;

	if (
		(iCount == 0u) ||
		(iCapacity == 0u) ||
		(iCapacity > XRT_QUEUE_MAX_CAPACITY)
	) {
		fprintf(stderr, "invalid logger ring benchmark arguments.\n");
		return 1;
	}

	memset(&Target, 0, sizeof(Target));
	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Name = XRT_STR_LITERAL("benchmark-target");
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = benchLogRingWrite;
	TargetConfig.Flush = benchLogRingFlush;
	TargetConfig.UserData = &Target;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	if ( pTarget == NULL ) {
		return 2;
	}

	if ( !xrtLogRingConfigInit(&RingConfig) ) {
		xrtLogSinkFree(pTarget);
		return 3;
	}
	RingConfig.Capacity = iCapacity;
	RingConfig.RecordLimit = 256u;
	RingConfig.Batch = XLOG_RING_BATCH_MAX;
	RingConfig.IdleWait = 0u;
	pRing = xrtLogRing(pTarget, &RingConfig);
	if ( pRing == NULL ) {
		xrtLogSinkFree(pTarget);
		return 4;
	}

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("benchmark");
	Record.Message = XRT_STR_LITERAL("ring hot path record");
	xbenchApplyCpuPinFromEnv();
	xbenchTimerStart(&Timer);
	for ( uint32 i = 0u; i < iCount; ) {
		xlogresult Result = xrtLogSinkSubmit(pRing, &Record);

		if ( Result == XLOG_RESULT_WRITTEN ) {
			i++;
			continue;
		}
		if ( Result != XLOG_RESULT_DROPPED ) {
			xrtLogSinkFree(pRing);
			xrtLogSinkFree(pTarget);
			return 5;
		}
		iRetries++;
		xrtThreadYield();
	}
	if ( !xrtLogSinkFlush(pRing) ) {
		xrtLogSinkFree(pRing);
		xrtLogSinkFree(pTarget);
		return 6;
	}
	xbenchTimerStop(&Timer);
	iElapsed = xbenchTimerElapsedNs(&Timer);

	if (
		!xrtLogRingStats(pRing, &Stats) ||
		(Target.Count != iCount) ||
		(Stats.Enqueued != iCount) ||
		(Stats.Processed != iCount) ||
		(Stats.Written != iCount) ||
		(Stats.Failed != 0u) ||
		(Stats.Oversized != 0u) ||
		(Stats.Queued != 0u) ||
		(Stats.QueueBytes != 0u)
	) {
		xrtLogSinkFree(pRing);
		xrtLogSinkFree(pTarget);
		return 7;
	}

	printf("logger_ring_records: %" PRIu32 "\n", iCount);
	printf("logger_ring_capacity: %" PRIu32 "\n", iCapacity);
	printf("logger_ring_retry_drops: %" PRIu64 "\n", iRetries);
	printf("logger_ring_elapsed_ns: %" PRIu64 "\n", iElapsed);
	printf(
		"logger_ring_records_per_sec: %.3f\n",
		xbenchSafeRate(iCount, iElapsed)
	);

	xrtLogSinkFree(pRing);
	xrtLogSinkFree(pTarget);
	return 0;
}
