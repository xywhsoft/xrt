#define XRT_MODULE_LOGGER_RING
#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头测试目标统计 Ring 工作线程收到的记录。 */
static xlogresult testSingleLogRingWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	size_t* pCount = (size_t*)pUserData;

	if (
		(pRecord->Message.Size != 4u) ||
		(memcmp(pRecord->Message.Data, "ring", 4u) != 0)
	) {
		return XLOG_RESULT_ERROR;
	}
	(*pCount)++;
	return XLOG_RESULT_WRITTEN;
}



/* 验证 Ring 单头闭包只拉入两种队列、线程和 Logger 核心。 */
int main(void)
{
	xlogsinkconfig TargetConfig;
	xlogringstats Stats;
	xlogrecord Record;
	xlogsink* pTarget;
	xlogsink* pRing;
	size_t iCount = 0u;

	#if !defined(XRT_FEATURE_LOGGER_RING) || \
		!defined(XRT_FEATURE_LOGGER_CORE) || \
		!defined(XRT_FEATURE_QUEUE_MPSC) || \
		!defined(XRT_FEATURE_QUEUE_MPMC) || \
		!defined(XRT_FEATURE_THREAD) || \
		defined(XRT_FEATURE_EVENT) || \
		defined(XRT_FEATURE_COND) || \
		defined(XRT_FEATURE_BUFFER) || \
		defined(XRT_FEATURE_FILE)
		#error "XRT_MODULE_LOGGER_RING dependency closure is incorrect"
	#endif

	memset(&TargetConfig, 0, sizeof(TargetConfig));
	TargetConfig.Level = XLOG_TRACE;
	TargetConfig.Write = testSingleLogRingWrite;
	TargetConfig.UserData = &iCount;
	pTarget = xrtLogSinkCreate(&TargetConfig);
	if ( pTarget == NULL ) {
		return 1;
	}
	pRing = xrtLogRing(pTarget, NULL);
	if ( pRing == NULL ) {
		xrtLogSinkFree(pTarget);
		return 2;
	}
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("ring");
	if ( xrtLogSinkSubmit(pRing, &Record) != XLOG_RESULT_WRITTEN ) {
		return 3;
	}
	if ( !xrtLogSinkFlush(pRing) ) {
		return 4;
	}
	if (
		!xrtLogRingStats(pRing, &Stats) ||
		(iCount != 1u) ||
		(Stats.Written != 1u)
	) {
		return 5;
	}
	xrtLogSinkFree(pRing);
	xrtLogSinkFree(pTarget);
	return 0;
}
