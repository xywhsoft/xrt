#include "../internal/xrt_internal.h"
#include "../internal/xrt_logger.h"
#include <xrt/logger.h>



#if defined(XRT_FEATURE_LOGGER_CORE)

/* 创建日志域结构化错误；构建失败时保留原因或底层构建错误。 */
xerror* __xrtLogErrorCreate(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
	xerror* pBuildError;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = Kind;
	Desc.Code = (int32)Code;
	Desc.Domain = "xrt.log";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	Desc.Cause = pCause;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		xrtErrorFree(pCause);
		return pError;
	}

	pBuildError = xrtTakeError();
	if ( pCause != NULL ) {
		xrtErrorFree(pBuildError);
		return pCause;
	}
	return pBuildError;
}



/* 用当前线程错误作为原因设置日志域错误。 */
void __xrtLogErrorSet(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	xerror* pError = __xrtLogErrorCreate(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);

	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}




/* Sink 保存不可变回调、名称以及可并发修改的阈值和统计。 */
struct xlogsink {
	volatile int32 RefCount;
	xatomic32 Level;
	xatomic64 Submitted;
	xatomic64 Written;
	xatomic64 Skipped;
	xatomic64 Dropped;
	xatomic64 Failed;
	xlogsinkwriteproc Write;
	xlogsinkflushproc Flush;
	xlogsinkdropproc Drop;
	ptr UserData;
	xstrview Name;
	char Text[];
};



/* 不可变快照持有 Sink 引用，使回调期间允许附加、移除和递归记录。 */
typedef struct xlogsinksnapshot {
	volatile int32 RefCount;
	size_t Count;
	xlogsink* Items[];
} xlogsinksnapshot;



/* Logger 只在替换 Sink 快照时加锁，热路径统计和阈值使用原子值。 */
struct xlogger {
	volatile int32 RefCount;
	xmutex Lock;
	xatomic32 Level;
	xatomic64 Submitted;
	xatomic64 Written;
	xatomic64 Skipped;
	xatomic64 Dropped;
	xatomic64 Failed;
	xlogsinksnapshot* Sinks;
	xstrview Name;
	char Text[];
};



/* 进程默认 Logger 用短临界区保护，旧对象在临界区外释放。 */
static xrt_spinlock __xrtLogDefaultLock;
static xlogger* __xrtLogDefaultLogger;



/* 判断过滤阈值是否合法。 */
static bool __xrtLogThresholdValid(xloglevel Level)
{
	return (Level >= XLOG_TRACE) && (Level <= XLOG_OFF);
}



/* 判断记录级别是否合法。 */
static bool __xrtLogRecordLevelValid(xloglevel Level)
{
	return (Level >= XLOG_TRACE) && (Level <= XLOG_FATAL);
}



/* 判断借用视图是否满足空视图契约。 */
static bool __xrtLogViewValid(xstrview View)
{
	return (View.Data != NULL) || (View.Size == 0);
}



/* 校验完整记录中全部借用范围和字段类型。 */
XRT_API bool xrtLogRecordValidate(const xlogrecord* pRecord)
{
	if (
		(pRecord == NULL) ||
		!__xrtLogRecordLevelValid(pRecord->Level) ||
		!__xrtLogViewValid(pRecord->Logger) ||
		!__xrtLogViewValid(pRecord->Message) ||
		!__xrtLogViewValid(pRecord->File) ||
		!__xrtLogViewValid(pRecord->Function) ||
		((pRecord->Fields == NULL) && (pRecord->FieldCount != 0))
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		const xlogfield* pField = &pRecord->Fields[i];

		if (
			!__xrtLogViewValid(pField->Name) ||
			(pField->Type < XLOG_FIELD_NULL) ||
			(pField->Type > XLOG_FIELD_ERROR) ||
			((pField->Type == XLOG_FIELD_STRING) &&
			 !__xrtLogViewValid(pField->Value.String))
		) {
			__xrtErrorSetInvalidArgument();
			return false;
		}
	}
	return true;
}



/* 安全增加拥有型记录的连续存储大小。 */
static bool __xrtLogOwnedSizeAdd(size_t* pSize, size_t iAdd)
{
	if ( iAdd > (SIZE_MAX - *pSize) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	*pSize += iAdd;
	return true;
}



/* 计算记录、字段数组和全部文本副本所需的连续存储。 */
bool __xrtLogOwnedSize(
	const xlogrecord* pRecord,
	size_t* pSize
)
{
	size_t iSize = sizeof(xlogrecord);

	if ( (pRecord == NULL) || (pSize == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pRecord->FieldCount >
		((SIZE_MAX - iSize) / sizeof(xlogfield)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iSize += pRecord->FieldCount * sizeof(xlogfield);
	if ( !__xrtLogOwnedSizeAdd(&iSize, pRecord->Logger.Size) ||
		 !__xrtLogOwnedSizeAdd(&iSize, pRecord->Message.Size) ||
		 !__xrtLogOwnedSizeAdd(&iSize, pRecord->File.Size) ||
		 !__xrtLogOwnedSizeAdd(&iSize, pRecord->Function.Size) ) {
		return false;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		if ( !__xrtLogOwnedSizeAdd(
			&iSize,
			pRecord->Fields[i].Name.Size
		) ) {
			return false;
		}
		if ( (pRecord->Fields[i].Type == XLOG_FIELD_STRING) &&
			 !__xrtLogOwnedSizeAdd(
				&iSize,
				pRecord->Fields[i].Value.String.Size
			) ) {
			return false;
		}
	}
	*pSize = iSize;
	return true;
}



/* 把一个文本视图复制到拥有型记录尾部。 */
static void __xrtLogOwnedViewCopy(
	xstrview* pTarget,
	xstrview Source,
	char** psWrite
)
{
	if ( Source.Size == 0u ) {
		pTarget->Data = NULL;
		pTarget->Size = 0u;
		return;
	}
	memcpy(*psWrite, Source.Data, Source.Size);
	pTarget->Data = *psWrite;
	pTarget->Size = Source.Size;
	*psWrite += Source.Size;
}



/* 释放一条拥有型记录持有的全部错误引用。 */
void __xrtLogOwnedClear(xlogrecord* pRecord)
{
	if ( pRecord == NULL ) {
		return;
	}
	for ( size_t i = 0; i < pRecord->FieldCount; i++ ) {
		if ( pRecord->Fields[i].Type == XLOG_FIELD_ERROR ) {
			xrtErrorFree((xerror*)pRecord->Fields[i].Value.Error);
		}
	}
	memset(pRecord, 0, sizeof(*pRecord));
}



/* 在调用方连续存储中完成记录深拷贝和错误引用保留。 */
bool __xrtLogOwnedCopy(
	const xlogrecord* pSource,
	xlogrecord* pTarget,
	size_t iCapacity
)
{
	xlogfield* pFields;
	char* sWrite;
	size_t iRequired = 0u;

	if ( (pSource == NULL) || (pTarget == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtLogOwnedSize(pSource, &iRequired) ) {
		return false;
	}
	if ( iRequired > iCapacity ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	memset(pTarget, 0, iRequired);
	*pTarget = *pSource;
	pFields = (xlogfield*)(pTarget + 1);
	pTarget->Fields = pFields;
	sWrite = (char*)(pFields + pSource->FieldCount);
	__xrtLogOwnedViewCopy(&pTarget->Logger, pSource->Logger, &sWrite);
	__xrtLogOwnedViewCopy(&pTarget->Message, pSource->Message, &sWrite);
	__xrtLogOwnedViewCopy(&pTarget->File, pSource->File, &sWrite);
	__xrtLogOwnedViewCopy(&pTarget->Function, pSource->Function, &sWrite);
	for ( size_t i = 0; i < pSource->FieldCount; i++ ) {
		pFields[i] = pSource->Fields[i];
		__xrtLogOwnedViewCopy(
			&pFields[i].Name,
			pSource->Fields[i].Name,
			&sWrite
		);
		if ( pFields[i].Type == XLOG_FIELD_STRING ) {
			__xrtLogOwnedViewCopy(
				&pFields[i].Value.String,
				pSource->Fields[i].Value.String,
				&sWrite
			);
		} else if ( (pFields[i].Type == XLOG_FIELD_ERROR) &&
			(pFields[i].Value.Error != NULL) ) {
			pFields[i].Value.Error = xrtErrorRef(
				pFields[i].Value.Error
			);
			if ( pFields[i].Value.Error == NULL ) {
				pTarget->FieldCount = i;
				__xrtLogOwnedClear(pTarget);
				return false;
			}
		}
	}
	return true;
}



/* 无锁读取一组原子统计。 */
static void __xrtLogStatsRead(
	const xatomic64* pSubmitted,
	const xatomic64* pWritten,
	const xatomic64* pSkipped,
	const xatomic64* pDropped,
	const xatomic64* pFailed,
	xlogstats* pStats
)
{
	pStats->Submitted = xrtAtomic64Load(pSubmitted, XMEMORY_RELAXED);
	pStats->Written = xrtAtomic64Load(pWritten, XMEMORY_RELAXED);
	pStats->Skipped = xrtAtomic64Load(pSkipped, XMEMORY_RELAXED);
	pStats->Dropped = xrtAtomic64Load(pDropped, XMEMORY_RELAXED);
	pStats->Failed = xrtAtomic64Load(pFailed, XMEMORY_RELAXED);
}



/* 按最终结果增加恰好一个统计分类。 */
static void __xrtLogStatsAdd(
	xatomic64* pWritten,
	xatomic64* pSkipped,
	xatomic64* pDropped,
	xatomic64* pFailed,
	xlogresult Result
)
{
	if ( Result == XLOG_RESULT_WRITTEN ) {
		(void)xrtAtomic64FetchAdd(pWritten, 1u, XMEMORY_RELAXED);
	} else if ( Result == XLOG_RESULT_SKIPPED ) {
		(void)xrtAtomic64FetchAdd(pSkipped, 1u, XMEMORY_RELAXED);
	} else if ( Result == XLOG_RESULT_DROPPED ) {
		(void)xrtAtomic64FetchAdd(pDropped, 1u, XMEMORY_RELAXED);
	} else {
		(void)xrtAtomic64FetchAdd(pFailed, 1u, XMEMORY_RELAXED);
	}
}



/* 建立没有具体下层错误时的稳定日志错误。 */
static void __xrtLogCallbackError(cstr sOperation, cstr sMessage)
{
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	Desc.Kind = XERR_IO;
	Desc.Code = XLOG_ERROR_CALLBACK;
	Desc.Domain = "xrt.log";
	Desc.Operation = sOperation;
	Desc.Message = sMessage;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 增加 Sink 快照引用。 */
static xlogsinksnapshot* __xrtLogSnapshotRef(xlogsinksnapshot* pSnapshot)
{
	if (
		(pSnapshot != NULL) &&
		(xrtRefRetain(&pSnapshot->RefCount) < 0)
	) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pSnapshot;
}



/* 释放快照及其持有的全部 Sink 引用。 */
static void __xrtLogSnapshotFree(xlogsinksnapshot* pSnapshot)
{
	if (
		(pSnapshot == NULL) ||
		(xrtRefRelease(&pSnapshot->RefCount) != 0)
	) {
		return;
	}
	for ( size_t i = 0; i < pSnapshot->Count; i++ ) {
		xrtLogSinkFree(pSnapshot->Items[i]);
	}
	xrtFree(pSnapshot);
}



/* 为指定数量分配空快照。 */
static xlogsinksnapshot* __xrtLogSnapshotCreate(size_t iCount)
{
	xlogsinksnapshot* pSnapshot;
	size_t iSize;

	if ( iCount > ((SIZE_MAX - sizeof(xlogsinksnapshot)) / sizeof(xlogsink*)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = sizeof(xlogsinksnapshot) + (iCount * sizeof(xlogsink*));
	pSnapshot = (xlogsinksnapshot*)xrtMalloc(iSize);
	if ( pSnapshot == NULL ) {
		return NULL;
	}
	pSnapshot->RefCount = 1;
	pSnapshot->Count = iCount;
	return pSnapshot;
}



/* 释放尚未发布且只初始化到指定位置的快照。 */
static void __xrtLogSnapshotAbort(
	xlogsinksnapshot* pSnapshot,
	size_t iInitialized
)
{
	for ( size_t i = 0; i < iInitialized; i++ ) {
		xrtLogSinkFree(pSnapshot->Items[i]);
	}
	xrtFree(pSnapshot);
}



/* 复制名称到柔性数组对象尾部。 */
static void __xrtLogNameCopy(char* sTarget, xstrview* pName, xstrview Name)
{
	if ( Name.Size != 0 ) {
		memcpy(sTarget, Name.Data, Name.Size);
	}
	sTarget[Name.Size] = 0;
	pName->Data = sTarget;
	pName->Size = Name.Size;
}



/* 保存当前错误并暂时清空执行上下文。 */
static xerror* __xrtLogErrorHold(void)
{
	xerror* pError = xrtErrorRef(xrtGetError());

	xrtClearError();
	return pError;
}



/* 恢复由调用方持有的错误所有权。 */
static void __xrtLogErrorRestore(xerror* pError)
{
	__xrtErrorSetOwned(pError);
}



/* 返回稳定的英文日志级别名称。 */
XRT_API cstr xrtLogLevelName(xloglevel Level)
{
	static const cstr arrNames[] = {
		"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"
	};

	if ( !__xrtLogThresholdValid(Level) ) {
		__xrtErrorSetInvalidArgument();
		return "";
	}
	return arrNames[(size_t)Level];
}



/* 构造空字段。 */
XRT_API xlogfield xrtLogFieldNull(xstrview Name)
{
	xlogfield Field;

	memset(&Field, 0, sizeof(Field));
	Field.Name = Name;
	Field.Type = XLOG_FIELD_NULL;
	return Field;
}



/* 构造布尔字段。 */
XRT_API xlogfield xrtLogFieldBool(xstrview Name, bool bValue)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_BOOL;
	Field.Value.Boolean = bValue;
	return Field;
}



/* 构造有符号整数字段。 */
XRT_API xlogfield xrtLogFieldInt(xstrview Name, int64 iValue)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_INT;
	Field.Value.Integer = iValue;
	return Field;
}



/* 构造无符号整数字段。 */
XRT_API xlogfield xrtLogFieldUInt(xstrview Name, uint64 iValue)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_UINT;
	Field.Value.Unsigned = iValue;
	return Field;
}



/* 构造浮点字段。 */
XRT_API xlogfield xrtLogFieldFloat(xstrview Name, double fValue)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_FLOAT;
	Field.Value.Float = fValue;
	return Field;
}



/* 构造字符串字段。 */
XRT_API xlogfield xrtLogFieldString(xstrview Name, xstrview Value)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_STRING;
	Field.Value.String = Value;
	return Field;
}



/* 构造时间字段。 */
XRT_API xlogfield xrtLogFieldTime(xstrview Name, xtime iValue)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_TIME;
	Field.Value.Time = iValue;
	return Field;
}



/* 构造借用错误字段。 */
XRT_API xlogfield xrtLogFieldError(xstrview Name, const xerror* pError)
{
	xlogfield Field = xrtLogFieldNull(Name);

	Field.Type = XLOG_FIELD_ERROR;
	Field.Value.Error = pError;
	return Field;
}



/* 创建同步 Logger。 */
XRT_API xlogger* xrtLogCreate(xstrview Name, xloglevel Level)
{
	xlogger* pLogger;
	size_t iSize;

	if ( !__xrtLogViewValid(Name) || !__xrtLogThresholdValid(Level) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( Name.Size > (SIZE_MAX - sizeof(xlogger) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = sizeof(xlogger) + Name.Size + 1u;
	pLogger = (xlogger*)xrtMalloc(iSize);
	if ( pLogger == NULL ) {
		return NULL;
	}
	memset(pLogger, 0, sizeof(xlogger));
	if ( !xrtMutexInit(&pLogger->Lock) ) {
		xrtFree(pLogger);
		return NULL;
	}
	pLogger->RefCount = 1;
	xrtAtomic32Init(&pLogger->Level, (uint32)Level);
	xrtAtomic64Init(&pLogger->Submitted, 0);
	xrtAtomic64Init(&pLogger->Written, 0);
	xrtAtomic64Init(&pLogger->Skipped, 0);
	xrtAtomic64Init(&pLogger->Dropped, 0);
	xrtAtomic64Init(&pLogger->Failed, 0);
	__xrtLogNameCopy(pLogger->Text, &pLogger->Name, Name);
	return pLogger;
}



/* 增加 Logger 引用。 */
XRT_API xlogger* xrtLogRef(xlogger* pLogger)
{
	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pLogger->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pLogger;
}



/* 释放 Logger 引用和最终 Sink 快照。 */
XRT_API void xrtLogFree(xlogger* pLogger)
{
	xlogsinksnapshot* pSnapshot;
	xerror* pPrevious;

	if (
		(pLogger == NULL) ||
		(xrtRefRelease(&pLogger->RefCount) != 0)
	) {
		return;
	}
	pPrevious = __xrtLogErrorHold();
	pSnapshot = pLogger->Sinks;
	(void)xrtMutexUnit(&pLogger->Lock);
	xrtFree(pLogger);
	__xrtLogSnapshotFree(pSnapshot);
	__xrtLogErrorRestore(pPrevious);
}



/* 返回 Logger 名称。 */
XRT_API xstrview xrtLogName(const xlogger* pLogger)
{
	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return pLogger->Name;
}



/* 读取 Logger 阈值。 */
XRT_API xloglevel xrtLogLevel(const xlogger* pLogger)
{
	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XLOG_OFF;
	}
	return (xloglevel)xrtAtomic32Load(&pLogger->Level, XMEMORY_RELAXED);
}



/* 修改 Logger 阈值。 */
XRT_API bool xrtLogSetLevel(xlogger* pLogger, xloglevel Level)
{
	if ( (pLogger == NULL) || !__xrtLogThresholdValid(Level) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtAtomic32Store(&pLogger->Level, (uint32)Level, XMEMORY_RELAXED);
	return true;
}



/* 创建自定义 Sink。 */
XRT_API xlogsink* xrtLogSinkCreate(const xlogsinkconfig* pConfig)
{
	xlogsink* pSink;
	size_t iSize;

	if (
		(pConfig == NULL) || (pConfig->Write == NULL) ||
		!__xrtLogViewValid(pConfig->Name) ||
		!__xrtLogThresholdValid(pConfig->Level)
	) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pConfig->Name.Size > (SIZE_MAX - sizeof(xlogsink) - 1u) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSize = sizeof(xlogsink) + pConfig->Name.Size + 1u;
	pSink = (xlogsink*)xrtMalloc(iSize);
	if ( pSink == NULL ) {
		return NULL;
	}
	memset(pSink, 0, sizeof(xlogsink));
	pSink->RefCount = 1;
	xrtAtomic32Init(&pSink->Level, (uint32)pConfig->Level);
	xrtAtomic64Init(&pSink->Submitted, 0);
	xrtAtomic64Init(&pSink->Written, 0);
	xrtAtomic64Init(&pSink->Skipped, 0);
	xrtAtomic64Init(&pSink->Dropped, 0);
	xrtAtomic64Init(&pSink->Failed, 0);
	pSink->Write = pConfig->Write;
	pSink->Flush = pConfig->Flush;
	pSink->Drop = pConfig->Drop;
	pSink->UserData = pConfig->UserData;
	__xrtLogNameCopy(pSink->Text, &pSink->Name, pConfig->Name);
	return pSink;
}



/* 增加 Sink 引用。 */
XRT_API xlogsink* xrtLogSinkRef(xlogsink* pSink)
{
	if ( pSink == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( xrtRefRetain(&pSink->RefCount) < 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pSink;
}



/* 释放 Sink 引用并在最后一次释放时调用 Drop。 */
XRT_API void xrtLogSinkFree(xlogsink* pSink)
{
	xerror* pPrevious;

	if (
		(pSink == NULL) ||
		(xrtRefRelease(&pSink->RefCount) != 0)
	) {
		return;
	}
	pPrevious = __xrtLogErrorHold();
	if ( pSink->Drop != NULL ) {
		pSink->Drop(pSink->UserData);
	}
	xrtFree(pSink);
	__xrtLogErrorRestore(pPrevious);
}



/* 返回 Sink 名称。 */
XRT_API xstrview xrtLogSinkName(const xlogsink* pSink)
{
	if ( pSink == NULL ) {
		__xrtErrorSetInvalidArgument();
		return (xstrview){ NULL, 0 };
	}
	return pSink->Name;
}



/* 读取 Sink 阈值。 */
XRT_API xloglevel xrtLogSinkLevel(const xlogsink* pSink)
{
	if ( pSink == NULL ) {
		__xrtErrorSetInvalidArgument();
		return XLOG_OFF;
	}
	return (xloglevel)xrtAtomic32Load(&pSink->Level, XMEMORY_RELAXED);
}



/* 修改 Sink 阈值。 */
XRT_API bool xrtLogSinkSetLevel(xlogsink* pSink, xloglevel Level)
{
	if ( (pSink == NULL) || !__xrtLogThresholdValid(Level) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	xrtAtomic32Store(&pSink->Level, (uint32)Level, XMEMORY_RELAXED);
	return true;
}



/* 直接提交一条记录并隔离回调错误上下文。 */
XRT_API xlogresult xrtLogSinkSubmit(
	xlogsink* pSink,
	const xlogrecord* pRecord
)
{
	xerror* pPrevious;
	xlogresult Result;
	xloglevel Threshold;

	if ( (pSink == NULL) || !xrtLogRecordValidate(pRecord) ) {
		return XLOG_RESULT_ERROR;
	}
	(void)xrtAtomic64FetchAdd(&pSink->Submitted, 1u, XMEMORY_RELAXED);
	Threshold = xrtLogSinkLevel(pSink);
	if (
		(pRecord->Level < Threshold) ||
		(Threshold == XLOG_OFF)
	) {
		Result = XLOG_RESULT_SKIPPED;
		__xrtLogStatsAdd(
			&pSink->Written,
			&pSink->Skipped,
			&pSink->Dropped,
			&pSink->Failed,
			Result
		);
		return Result;
	}
	pPrevious = __xrtLogErrorHold();
	Result = pSink->Write(pRecord, pSink->UserData);
	if (
		(Result < XLOG_RESULT_ERROR) ||
		(Result > XLOG_RESULT_DROPPED)
	) {
		__xrtErrorSetInternal();
		Result = XLOG_RESULT_ERROR;
	}
	if ( Result == XLOG_RESULT_ERROR ) {
		if ( xrtGetError() == NULL ) {
			__xrtLogCallbackError("write", "log sink write callback failed");
		}
		xrtErrorFree(pPrevious);
	} else {
		__xrtLogErrorRestore(pPrevious);
	}
	__xrtLogStatsAdd(
		&pSink->Written,
		&pSink->Skipped,
		&pSink->Dropped,
		&pSink->Failed,
		Result
	);
	return Result;
}



/* 提交一个 Sink 已经接受的内容。 */
XRT_API bool xrtLogSinkFlush(xlogsink* pSink)
{
	xerror* pPrevious;
	bool bResult;

	if ( pSink == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( pSink->Flush == NULL ) {
		return true;
	}
	pPrevious = __xrtLogErrorHold();
	bResult = pSink->Flush(pSink->UserData);
	if ( bResult ) {
		__xrtLogErrorRestore(pPrevious);
	} else {
		xrtErrorFree(pPrevious);
		if ( xrtGetError() == NULL ) {
			__xrtLogCallbackError("flush", "log sink flush callback failed");
		}
	}
	return bResult;
}



/* 读取 Sink 统计。 */
XRT_API bool xrtLogSinkStats(const xlogsink* pSink, xlogstats* pStats)
{
	if ( (pSink == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtLogStatsRead(
		&pSink->Submitted,
		&pSink->Written,
		&pSink->Skipped,
		&pSink->Dropped,
		&pSink->Failed,
		pStats
	);
	return true;
}



/* 验证 Sink 的不可变写回调并返回内部扩展状态。 */
bool __xrtLogSinkData(
	const xlogsink* pSink,
	xlogsinkwriteproc pWrite,
	ptr* ppUserData
)
{
	if (
		(pSink == NULL) ||
		(pWrite == NULL) ||
		(ppUserData == NULL) ||
		(pSink->Write != pWrite)
	) {
		return false;
	}
	*ppUserData = pSink->UserData;
	return true;
}



/* 附加一个 Sink，并在锁外释放旧快照。 */
XRT_API bool xrtLogAttach(xlogger* pLogger, xlogsink* pSink)
{
	xlogsinksnapshot* pOld;
	xlogsinksnapshot* pNew;
	size_t iInitialized = 0;
	bool bResult = false;
	bool bUnlocked;

	if ( (pLogger == NULL) || (pSink == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&pLogger->Lock) ) {
		return false;
	}
	pOld = pLogger->Sinks;
	for ( size_t i = 0; (pOld != NULL) && (i < pOld->Count); i++ ) {
		if ( pOld->Items[i] == pSink ) {
			__xrtErrorSetExists();
			goto Finish;
		}
	}
	pNew = __xrtLogSnapshotCreate((pOld != NULL ? pOld->Count : 0) + 1u);
	if ( pNew == NULL ) {
		goto Finish;
	}
	for ( size_t i = 0; (pOld != NULL) && (i < pOld->Count); i++ ) {
		pNew->Items[iInitialized] = xrtLogSinkRef(pOld->Items[i]);
		if ( pNew->Items[iInitialized] == NULL ) {
			__xrtLogSnapshotAbort(pNew, iInitialized);
			goto Finish;
		}
		iInitialized++;
	}
	pNew->Items[iInitialized] = xrtLogSinkRef(pSink);
	if ( pNew->Items[iInitialized] == NULL ) {
		__xrtLogSnapshotAbort(pNew, iInitialized);
		goto Finish;
	}
	pLogger->Sinks = pNew;
	bResult = true;

Finish:
	bUnlocked = xrtMutexUnlock(&pLogger->Lock);
	if ( bResult ) {
		__xrtLogSnapshotFree(pOld);
	}
	return bUnlocked && bResult;
}



/* 移除一个 Sink，并让活动回调通过旧快照自然完成。 */
XRT_API bool xrtLogDetach(xlogger* pLogger, xlogsink* pSink)
{
	xlogsinksnapshot* pOld;
	xlogsinksnapshot* pNew = NULL;
	size_t iFound = SIZE_MAX;
	size_t iInitialized = 0;
	bool bResult = false;
	bool bUnlocked;

	if ( (pLogger == NULL) || (pSink == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !xrtMutexLock(&pLogger->Lock) ) {
		return false;
	}
	pOld = pLogger->Sinks;
	for ( size_t i = 0; (pOld != NULL) && (i < pOld->Count); i++ ) {
		if ( pOld->Items[i] == pSink ) {
			iFound = i;
			break;
		}
	}
	if ( iFound == SIZE_MAX ) {
		goto Finish;
	}
	if ( pOld->Count > 1u ) {
		pNew = __xrtLogSnapshotCreate(pOld->Count - 1u);
		if ( pNew == NULL ) {
			goto Finish;
		}
		for ( size_t i = 0; i < pOld->Count; i++ ) {
			if ( i == iFound ) {
				continue;
			}
			pNew->Items[iInitialized] = xrtLogSinkRef(pOld->Items[i]);
			if ( pNew->Items[iInitialized] == NULL ) {
				__xrtLogSnapshotAbort(pNew, iInitialized);
				goto Finish;
			}
			iInitialized++;
		}
	}
	pLogger->Sinks = pNew;
	bResult = true;

Finish:
	bUnlocked = xrtMutexUnlock(&pLogger->Lock);
	if ( bResult ) {
		__xrtLogSnapshotFree(pOld);
	}
	return bUnlocked && bResult;
}



/* 原子移除全部 Sink 快照。 */
XRT_API size_t xrtLogDetachAll(xlogger* pLogger)
{
	xlogsinksnapshot* pOld;
	size_t iCount;
	bool bUnlocked;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtMutexLock(&pLogger->Lock) ) {
		return 0;
	}
	pOld = pLogger->Sinks;
	pLogger->Sinks = NULL;
	iCount = pOld != NULL ? pOld->Count : 0;
	bUnlocked = xrtMutexUnlock(&pLogger->Lock);
	__xrtLogSnapshotFree(pOld);
	return bUnlocked ? iCount : 0;
}



/* 读取快照中的 Sink 数量。 */
XRT_API size_t xrtLogSinkCount(xlogger* pLogger)
{
	size_t iCount;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !xrtMutexLock(&pLogger->Lock) ) {
		return 0;
	}
	iCount = pLogger->Sinks != NULL ? pLogger->Sinks->Count : 0;
	if ( !xrtMutexUnlock(&pLogger->Lock) ) {
		return 0;
	}
	return iCount;
}



/* 取得一个允许回调期间修改 Logger 的不可变快照。 */
static bool __xrtLogSinksAcquire(
	xlogger* pLogger,
	xlogsinksnapshot** pSnapshot
)
{
	bool bHadSnapshot;

	if ( !xrtMutexLock(&pLogger->Lock) ) {
		return false;
	}
	bHadSnapshot = pLogger->Sinks != NULL;
	*pSnapshot = __xrtLogSnapshotRef(pLogger->Sinks);
	if ( !xrtMutexUnlock(&pLogger->Lock) ) {
		__xrtLogSnapshotFree(*pSnapshot);
		*pSnapshot = NULL;
		return false;
	}
	if ( bHadSnapshot && (*pSnapshot == NULL) ) {
		return false;
	}
	return true;
}



/* 按所有 Sink 的结果计算一条记录的最终结果。 */
static xlogresult __xrtLogResultMerge(
	xlogresult Current,
	xlogresult Next
)
{
	if ( (Current == XLOG_RESULT_ERROR) || (Next == XLOG_RESULT_ERROR) ) {
		return XLOG_RESULT_ERROR;
	}
	if ( (Current == XLOG_RESULT_WRITTEN) || (Next == XLOG_RESULT_WRITTEN) ) {
		return XLOG_RESULT_WRITTEN;
	}
	if ( (Current == XLOG_RESULT_DROPPED) || (Next == XLOG_RESULT_DROPPED) ) {
		return XLOG_RESULT_DROPPED;
	}
	return XLOG_RESULT_SKIPPED;
}



/* 提交完整记录并在回调之间保存第一个真实错误。 */
XRT_API xlogresult xrtLogSubmit(
	xlogger* pLogger,
	const xlogrecord* pRecord
)
{
	xlogsinksnapshot* pSnapshot;
	xlogrecord Record;
	xlogresult Result = XLOG_RESULT_SKIPPED;
	xerror* pPrevious;
	xerror* pFirstError = NULL;

	if ( (pLogger == NULL) || !xrtLogRecordValidate(pRecord) ) {
		return XLOG_RESULT_ERROR;
	}
	(void)xrtAtomic64FetchAdd(&pLogger->Submitted, 1u, XMEMORY_RELAXED);
	if (
		(pRecord->Level < xrtLogLevel(pLogger)) ||
		(xrtLogLevel(pLogger) == XLOG_OFF)
	) {
		__xrtLogStatsAdd(
			&pLogger->Written,
			&pLogger->Skipped,
			&pLogger->Dropped,
			&pLogger->Failed,
			XLOG_RESULT_SKIPPED
		);
		return XLOG_RESULT_SKIPPED;
	}
	if ( !__xrtLogSinksAcquire(pLogger, &pSnapshot) ) {
		__xrtLogStatsAdd(
			&pLogger->Written,
			&pLogger->Skipped,
			&pLogger->Dropped,
			&pLogger->Failed,
			XLOG_RESULT_ERROR
		);
		return XLOG_RESULT_ERROR;
	}
	if ( pSnapshot == NULL ) {
		__xrtLogStatsAdd(
			&pLogger->Written,
			&pLogger->Skipped,
			&pLogger->Dropped,
			&pLogger->Failed,
			XLOG_RESULT_SKIPPED
		);
		return XLOG_RESULT_SKIPPED;
	}
	Record = *pRecord;
	Record.Logger = pLogger->Name;
	pPrevious = xrtErrorRef(xrtGetError());
	for ( size_t i = 0; i < pSnapshot->Count; i++ ) {
		xlogresult Next = xrtLogSinkSubmit(pSnapshot->Items[i], &Record);

		if ( (Next == XLOG_RESULT_ERROR) && (pFirstError == NULL) ) {
			pFirstError = xrtErrorRef(xrtGetError());
		}
		Result = __xrtLogResultMerge(Result, Next);
		__xrtErrorSetOwned(xrtErrorRef(pPrevious));
	}
	__xrtLogSnapshotFree(pSnapshot);
	if ( pFirstError != NULL ) {
		__xrtErrorSetOwned(pFirstError);
		xrtErrorFree(pPrevious);
	} else {
		__xrtErrorSetOwned(pPrevious);
	}
	__xrtLogStatsAdd(
		&pLogger->Written,
		&pLogger->Skipped,
		&pLogger->Dropped,
		&pLogger->Failed,
		Result
	);
	return Result;
}



/* 使用当前时间提交常用文本。 */
XRT_API xlogresult xrtLog(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message
)
{
	return xrtLogFields(pLogger, Level, Message, NULL, 0);
}



/* 使用当前时间提交结构化字段。 */
XRT_API xlogresult xrtLogFields(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount
)
{
	return xrtLogSource(
		pLogger,
		Level,
		Message,
		pFields,
		iFieldCount,
		(xstrview){ NULL, 0 },
		(xstrview){ NULL, 0 },
		0,
		0
	);
}



/* 使用当前时间提交带源码位置的结构化记录。 */
XRT_API xlogresult xrtLogSource(
	xlogger* pLogger,
	xloglevel Level,
	xstrview Message,
	const xlogfield* pFields,
	size_t iFieldCount,
	xstrview File,
	xstrview Function,
	uint32 iLine,
	uint64 iThreadId
)
{
	xlogrecord Record;

	memset(&Record, 0, sizeof(Record));
	Record.Time = xrtNow();
	Record.Level = Level;
	Record.Message = Message;
	Record.Fields = pFields;
	Record.FieldCount = iFieldCount;
	Record.File = File;
	Record.Function = Function;
	Record.Line = iLine;
	Record.ThreadId = iThreadId;
	return xrtLogSubmit(pLogger, &Record);
}



/* Flush 所有当前 Sink，并保留第一个失败原因。 */
XRT_API bool xrtLogFlush(xlogger* pLogger)
{
	xlogsinksnapshot* pSnapshot;
	xerror* pPrevious;
	xerror* pFirstError = NULL;
	bool bResult = true;

	if ( pLogger == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( !__xrtLogSinksAcquire(pLogger, &pSnapshot) ) {
		return false;
	}
	if ( pSnapshot == NULL ) {
		return true;
	}
	pPrevious = xrtErrorRef(xrtGetError());
	for ( size_t i = 0; i < pSnapshot->Count; i++ ) {
		if ( !xrtLogSinkFlush(pSnapshot->Items[i]) ) {
			if ( pFirstError == NULL ) {
				pFirstError = xrtErrorRef(xrtGetError());
			}
			bResult = false;
		}
		__xrtErrorSetOwned(xrtErrorRef(pPrevious));
	}
	__xrtLogSnapshotFree(pSnapshot);
	if ( pFirstError != NULL ) {
		__xrtErrorSetOwned(pFirstError);
		xrtErrorFree(pPrevious);
	} else {
		__xrtErrorSetOwned(pPrevious);
	}
	return bResult;
}



/* 读取 Logger 统计。 */
XRT_API bool xrtLogStats(const xlogger* pLogger, xlogstats* pStats)
{
	if ( (pLogger == NULL) || (pStats == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	__xrtLogStatsRead(
		&pLogger->Submitted,
		&pLogger->Written,
		&pLogger->Skipped,
		&pLogger->Dropped,
		&pLogger->Failed,
		pStats
	);
	return true;
}



/* 返回进程默认 Logger 的新引用。 */
XRT_API xlogger* xrtLogDefault(void)
{
	xlogger* pLogger;

	__xrtSpinLock(&__xrtLogDefaultLock);
	pLogger = __xrtLogDefaultLogger != NULL
		? xrtLogRef(__xrtLogDefaultLogger)
		: NULL;
	__xrtSpinUnlock(&__xrtLogDefaultLock);
	return pLogger;
}



/* 原子替换进程默认 Logger。 */
XRT_API bool xrtLogSetDefault(xlogger* pLogger)
{
	xlogger* pOldLogger;
	xlogger* pNewLogger = NULL;

	if ( pLogger != NULL ) {
		pNewLogger = xrtLogRef(pLogger);
		if ( pNewLogger == NULL ) {
			return false;
		}
	}
	__xrtSpinLock(&__xrtLogDefaultLock);
	pOldLogger = __xrtLogDefaultLogger;
	__xrtLogDefaultLogger = pNewLogger;
	__xrtSpinUnlock(&__xrtLogDefaultLock);
	xrtLogFree(pOldLogger);
	return true;
}

#endif
