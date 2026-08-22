#include "../test.h"



/* 测试目标用原子开关确定性阻塞首条记录，并保存工作线程实际读取的数据。 */
typedef struct testlogringtarget {
	xatomic32 Entered;
	xatomic32 Release;
	xlogsink* Ring;
	bool Recurse;
	xlogresult RecursiveResult;
	size_t Count;
	size_t Flushes;
	char Message[64];
	char FieldName[64];
	char FieldValue[64];
} testlogringtarget;



/* 把借用视图复制到固定测试缓冲并补零。 */
static void testLogRingCopy(
	char* sTarget,
	size_t iCapacity,
	xstrview Value
)
{
	size_t iSize = Value.Size < (iCapacity - 1u)
		? Value.Size
		: (iCapacity - 1u);

	if ( iSize != 0u ) {
		memcpy(sTarget, Value.Data, iSize);
	}
	sTarget[iSize] = 0;
}



/* 首次调用阻塞到主线程释放，以验证固定槽和满载流控。 */
static xlogresult testLogRingWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	testlogringtarget* pTarget = (testlogringtarget*)pUserData;

	if ( pTarget->Count == 0u ) {
		xrtAtomic32Store(&pTarget->Entered, 1u, XMEMORY_RELEASE);
		while ( xrtAtomic32Load(&pTarget->Release, XMEMORY_ACQUIRE) == 0u ) {
			xrtThreadYield();
		}
		testLogRingCopy(
			pTarget->Message,
			sizeof(pTarget->Message),
			pRecord->Message
		);
		if ( pRecord->FieldCount != 0u ) {
			testLogRingCopy(
				pTarget->FieldName,
				sizeof(pTarget->FieldName),
				pRecord->Fields[0].Name
			);
			testLogRingCopy(
				pTarget->FieldValue,
				sizeof(pTarget->FieldValue),
				pRecord->Fields[0].Value.String
			);
		}
	}
	pTarget->Count++;
	if ( pTarget->Recurse ) {
		pTarget->Recurse = false;
		pTarget->RecursiveResult = xrtLogSinkSubmit(pTarget->Ring, pRecord);
	}
	return XLOG_RESULT_WRITTEN;
}



/* 统计目标收到的精确刷新次数。 */
static bool testLogRingFlush(ptr pUserData)
{
	testlogringtarget* pTarget = (testlogringtarget*)pUserData;

	pTarget->Flushes++;
	return true;
}



/* 创建借用测试状态的同步目标 Sink。 */
static xlogsink* testLogRingTarget(testlogringtarget* pTarget)
{
	xlogsinkconfig Config;

	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("ring-target");
	Config.Level = XLOG_TRACE;
	Config.Write = testLogRingWrite;
	Config.Flush = testLogRingFlush;
	Config.UserData = pTarget;
	return xrtLogSinkCreate(&Config);
}



/* 验证深拷贝、有界流控、递归保护、Flush 顺序和无锁统计快照。 */
int main(void)
{
	testlogringtarget Target;
	xlogringconfig Config;
	xlogringstats Stats;
	xlogsink* pTarget;
	xlogsink* pRing;
	xlogrecord Record;
	xlogfield Field;
	char sMessage[] = "borrowed-message";
	char sFieldName[] = "request";
	char sFieldValue[] = "borrowed-field";
	char sLarge[256];

	memset(&Target, 0, sizeof(Target));
	xrtAtomic32Init(&Target.Entered, 0u);
	xrtAtomic32Init(&Target.Release, 0u);
	pTarget = testLogRingTarget(&Target);
	testRequire(pTarget != NULL, "Logger ring target create failed");
	testRequire(xrtLogRingConfigInit(&Config), "Logger ring config init failed");
	Config.Capacity = 2u;
	Config.RecordLimit = 192u;
	Config.Batch = 2u;
	Config.IdleWait = 0u;
	pRing = xrtLogRing(pTarget, &Config);
	testRequire(pRing != NULL, "Logger ring create failed");
	Target.Ring = pRing;
	Target.Recurse = true;

	memset(&Field, 0, sizeof(Field));
	Field.Name = (xstrview){ sFieldName, strlen(sFieldName) };
	Field.Type = XLOG_FIELD_STRING;
	Field.Value.String = (xstrview){ sFieldValue, strlen(sFieldValue) };
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = (xstrview){ sMessage, strlen(sMessage) };
	Record.Fields = &Field;
	Record.FieldCount = 1u;
	testRequire(
		xrtLogSinkSubmit(pRing, &Record) == XLOG_RESULT_WRITTEN,
		"Logger ring first submit failed"
	);
	while ( xrtAtomic32Load(&Target.Entered, XMEMORY_ACQUIRE) == 0u ) {
		xrtThreadYield();
	}

	memset(sMessage, 'x', strlen(sMessage));
	memset(sFieldName, 'y', strlen(sFieldName));
	memset(sFieldValue, 'z', strlen(sFieldValue));
	Record.Fields = NULL;
	Record.FieldCount = 0u;
	Record.Message = XRT_STR_LITERAL("queued");
	testRequire(
		xrtLogSinkSubmit(pRing, &Record) == XLOG_RESULT_WRITTEN,
		"Logger ring second submit failed"
	);
	testRequire(
		xrtLogSinkSubmit(pRing, &Record) == XLOG_RESULT_DROPPED,
		"Logger ring full queue must drop immediately"
	);

	memset(sLarge, 'q', sizeof(sLarge));
	Record.Message = (xstrview){ sLarge, sizeof(sLarge) };
	testRequire(
		xrtLogSinkSubmit(pRing, &Record) == XLOG_RESULT_DROPPED,
		"Logger ring oversized record must drop immediately"
	);
	xrtAtomic32Store(&Target.Release, 1u, XMEMORY_RELEASE);
	testRequire(xrtLogSinkFlush(pRing), "Logger ring flush failed");
	testRequire(Target.Count == 2u, "Logger ring target count mismatch");
	testRequire(
		strcmp(Target.Message, "borrowed-message") == 0,
		"Logger ring message deep copy failed"
	);
	testRequire(
		strcmp(Target.FieldName, "request") == 0,
		"Logger ring field name deep copy failed"
	);
	testRequire(
		strcmp(Target.FieldValue, "borrowed-field") == 0,
		"Logger ring field value deep copy failed"
	);
	testRequire(
		Target.RecursiveResult == XLOG_RESULT_DROPPED,
		"Logger ring recursive write must be dropped"
	);
	testRequire(xrtLogRingStats(pRing, &Stats), "Logger ring stats failed");
	testRequire(Stats.Enqueued == 2u, "Logger ring enqueued count mismatch");
	testRequire(Stats.Processed == 2u, "Logger ring processed count mismatch");
	testRequire(Stats.Written == 2u, "Logger ring written count mismatch");
	testRequire(Stats.Dropped == 1u, "Logger ring dropped count mismatch");
	testRequire(Stats.Oversized == 1u, "Logger ring oversized count mismatch");
	testRequire(
		Stats.ReentrantDrops == 1u,
		"Logger ring recursive drop count mismatch"
	);
	testRequire(Stats.Queued == 0u, "Logger ring queue must be empty after flush");
	testRequire(
		Stats.QueueBytes == 0u,
		"Logger ring queue bytes must be zero after flush"
	);
	testRequire(Target.Flushes == 1u, "Logger ring explicit flush mismatch");
	testRequire(xrtLogRingTarget(pRing) == pTarget, "Logger ring target mismatch");
	testRequire(xrtLogRingLastError(pRing) == NULL, "Logger ring unexpected error");

	xrtLogSinkFree(pRing);
	testRequire(Target.Flushes == 2u, "Logger ring shutdown flush mismatch");
	xrtLogSinkFree(pTarget);
	puts("[PASS] Logger ring");
	return 0;
}
