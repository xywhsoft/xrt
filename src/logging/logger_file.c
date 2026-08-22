#include "../internal/xrt_internal.h"
#include "../internal/xrt_logger.h"
#include <xrt/logger.h>

#include <stdio.h>



#if defined(XRT_FEATURE_LOGGER_FILE)

/* 文件 Sink 保存独占句柄、精确大小、复用缓冲和串行化状态。 */
typedef struct xlogfilestate {
	xmutex Lock;
	xatomic64 Owner;
	xlogfileoptions Options;
	xfile File;
	xbuffer Record;
	xlogformatproc Format;
	xlogformatdropproc Drop;
	ptr UserData;
	cstr Path;
	str BackupSource;
	str BackupTarget;
	size_t PathSize;
	uint64 CurrentBytes;
	uint64 WrittenBytes;
	uint64 Records;
	uint64 Rotations;
	uint64 Reopens;
	uint64 Syncs;
	uint64 LastSync;
	char Text[];
} xlogfilestate;



/* Buffer Writer 同时约束一条编码后记录的硬上限。 */
typedef struct xlogfilewriter {
	xlogfilestate* State;
	bool Failed;
	bool Limited;
} xlogfilewriter;



/* 建立带可选下层原因的稳定文件 Sink 错误。 */
static void __xrtLogFileErrorCause(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
)
{
	xerrordesc Desc;
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
		__xrtErrorSetOwned(pError);
	} else if ( pCause != NULL ) {
		__xrtErrorSetOwned(xrtErrorRef(pCause));
	}
	xrtErrorFree(pCause);
}



/* 取走当前下层错误并包裹为文件 Sink 操作错误。 */
static void __xrtLogFileWrap(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
)
{
	__xrtLogFileErrorCause(
		Kind,
		Code,
		sOperation,
		sMessage,
		xrtTakeError()
	);
}



/* 校验公共文件选项，不读取格式器数据。 */
static bool __xrtLogFileOptionsValid(const xlogfileoptions* pOptions)
{
	return
		(pOptions != NULL) &&
		(pOptions->Path != NULL) &&
		(pOptions->Path[0] != 0) &&
		(pOptions->Level >= XLOG_TRACE) &&
		(pOptions->Level <= XLOG_OFF) &&
		(pOptions->Mode >= XLOG_FILE_APPEND) &&
		(pOptions->Mode <= XLOG_FILE_TRUNCATE) &&
		(pOptions->Sync >= XLOG_FILE_SYNC_MANUAL) &&
		(pOptions->Sync <= XLOG_FILE_SYNC_INTERVAL) &&
		(pOptions->RecordLimit != 0u) &&
		(
			(pOptions->Sync != XLOG_FILE_SYNC_INTERVAL) ||
			(pOptions->SyncInterval != 0u)
		);
}



/* 打开追加句柄并读取当前精确文件大小。 */
static xfile __xrtLogFileOpenPath(
	cstr sPath,
	bool bTruncate,
	uint64* pSize
)
{
	uint32 iFlags = XFILE_WRITE | XFILE_CREATE | XFILE_APPEND;
	xfile File;
	xerror* pError;

	if ( bTruncate ) {
		iFlags |= XFILE_TRUNCATE;
	}
	File = xrtOpen(sPath, iFlags);
	if ( File == NULL ) {
		return NULL;
	}
	if ( xrtFileSize(File, pSize) ) {
		return File;
	}
	pError = xrtTakeError();
	(void)xrtClose(File);
	__xrtErrorSetOwned(pError);
	return NULL;
}



/* 生成一个复用的 path.N 备份路径。 */
static cstr __xrtLogFileBackupPath(
	xlogfilestate* pState,
	str sBuffer,
	uint32 iIndex
)
{
	int iSize = snprintf(
		sBuffer + pState->PathSize,
		12u,
		".%u",
		(unsigned int)iIndex
	);

	if ( (iSize <= 0) || (iSize >= 12) ) {
		__xrtErrorSetInternal();
		return NULL;
	}
	return sBuffer;
}



/* 失败后释放大记录占用，同时让常用小记录保留复用容量。 */
static void __xrtLogFileRecordClear(xlogfilestate* pState)
{
	xrtBufferClear(&pState->Record);
	if ( pState->Record.Capacity > pState->Options.BufferLimit ) {
		xrtBufferUnit(&pState->Record);
		(void)xrtBufferInit(&pState->Record);
	}
}



/* 格式器分段输出只追加到当前记录缓冲，不执行文件 I/O。 */
static bool __xrtLogFileBufferWrite(xbytesview Data, ptr pUserData)
{
	xlogfilewriter* pWriter = (xlogfilewriter*)pUserData;
	xlogfilestate* pState = pWriter->State;
	bool bResult;

	if ( pWriter->Failed ) {
		return false;
	}

	if (
		(pState->Record.Size > pState->Options.RecordLimit) ||
		(Data.Size > (pState->Options.RecordLimit - pState->Record.Size))
	) {
		pWriter->Failed = true;
		pWriter->Limited = true;
		__xrtLogFileErrorCause(
			XERR_RANGE,
			XLOG_ERROR_FILE_LIMIT,
			"file-format",
			"formatted log record exceeds the configured byte limit",
			NULL
		);
		return false;
	}
	bResult = xrtBufferAppend(&pState->Record, Data);
	if ( !bResult ) {
		pWriter->Failed = true;
	}
	return bResult;
}



/* 判断精确编码后的下一条记录是否需要先滚动。 */
static bool __xrtLogFileShouldRotate(
	const xlogfilestate* pState,
	size_t iRecordSize
)
{
	if ( (pState->Options.MaxBytes == 0u) || (pState->CurrentBytes == 0u) ) {
		return false;
	}
	if ( pState->CurrentBytes >= pState->Options.MaxBytes ) {
		return true;
	}
	return ((uint64)iRecordSize >
		(pState->Options.MaxBytes - pState->CurrentBytes));
}



/* 尽力恢复一个可继续追加的当前路径句柄。 */
static void __xrtLogFileRecover(xlogfilestate* pState)
{
	uint64 iSize = 0;
	xfile File;

	xrtClearError();
	File = __xrtLogFileOpenPath(pState->Path, false, &iSize);
	if ( File != NULL ) {
		pState->File = File;
		pState->CurrentBytes = iSize;
	}
	xrtClearError();
}



/* 在锁内关闭、移动备份并创建新的追加文件。 */
static bool __xrtLogFileRotateLocked(xlogfilestate* pState)
{
	xfile File = pState->File;
	xerror* pCause;
	uint64 iSize = 0;

	pState->File = NULL;
	if ( (File != NULL) && !xrtClose(File) ) {
		pCause = xrtTakeError();
		__xrtLogFileRecover(pState);
		__xrtLogFileErrorCause(
			XERR_IO,
			XLOG_ERROR_FILE_CLOSE,
			"file-rotate-close",
			"failed to close the active log file before rotation",
			pCause
		);
		return false;
	}
	if ( pState->Options.BackupCount != 0u ) {
		for ( uint32 i = pState->Options.BackupCount; i > 1u; i-- ) {
			cstr sSource = __xrtLogFileBackupPath(
				pState,
				pState->BackupSource,
				i - 1u
			);
			cstr sTarget = __xrtLogFileBackupPath(
				pState,
				pState->BackupTarget,
				i
			);

			if (
				(sSource == NULL) ||
				(sTarget == NULL) ||
				(
					xrtPathExists(sSource) &&
					!xrtPathRename(sSource, sTarget, true)
				)
			) {
				goto Failed;
			}
		}
		if ( xrtPathExists(pState->Path) ) {
			cstr sTarget = __xrtLogFileBackupPath(
				pState,
				pState->BackupTarget,
				1u
			);

			if (
				(sTarget == NULL) ||
				!xrtPathRename(pState->Path, sTarget, true)
			) {
				goto Failed;
			}
		}
	}
	pState->File = __xrtLogFileOpenPath(pState->Path, true, &iSize);
	if ( pState->File == NULL ) {
		goto Failed;
	}
	pState->CurrentBytes = iSize;
	pState->Rotations++;
	return true;

Failed:
	pCause = xrtTakeError();
	__xrtLogFileRecover(pState);
	__xrtLogFileErrorCause(
		XERR_IO,
		XLOG_ERROR_FILE_ROTATE,
		"file-rotate",
		"failed to rotate the log file",
		pCause
	);
	return false;
}



/* 在锁内重新打开当前路径，并在成功切换后关闭旧句柄。 */
static bool __xrtLogFileReopenLocked(xlogfilestate* pState)
{
	xfile pOldFile;
	xfile pNewFile;
	uint64 iSize = 0;

	pNewFile = __xrtLogFileOpenPath(pState->Path, false, &iSize);
	if ( pNewFile == NULL ) {
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_OPEN,
			"file-reopen",
			"failed to reopen the log file"
		);
		return false;
	}
	pOldFile = pState->File;
	pState->File = pNewFile;
	pState->CurrentBytes = iSize;
	pState->Reopens++;
	if ( (pOldFile != NULL) && !xrtClose(pOldFile) ) {
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_CLOSE,
			"file-reopen-close",
			"the new log file is active but the old handle failed to close"
		);
		return false;
	}
	return true;
}



/* 在锁内把当前文件提交到稳定存储并累计次数。 */
static bool __xrtLogFileSyncLocked(xlogfilestate* pState)
{
	if ( (pState->File == NULL) && !__xrtLogFileReopenLocked(pState) ) {
		return false;
	}
	if ( !xrtFlush(pState->File) ) {
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_SYNC,
			"file-sync",
			"failed to synchronize the log file"
		);
		return false;
	}
	pState->Syncs++;
	return true;
}



/* 按逐条或单调时间间隔策略决定是否持久化。 */
static bool __xrtLogFileSyncRecord(xlogfilestate* pState)
{
	uint64 iNow;

	if ( pState->Options.Sync == XLOG_FILE_SYNC_MANUAL ) {
		return true;
	}
	if ( pState->Options.Sync == XLOG_FILE_SYNC_RECORD ) {
		return __xrtLogFileSyncLocked(pState);
	}
	iNow = xrtClock();
	if ( (iNow == 0u) && (xrtGetError() != NULL) ) {
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_SYNC,
			"file-sync-clock",
			"failed to read the monotonic clock for log synchronization"
		);
		return false;
	}
	if (
		(iNow >= pState->LastSync) &&
		((iNow - pState->LastSync) < pState->Options.SyncInterval)
	) {
		return true;
	}
	if ( !__xrtLogFileSyncLocked(pState) ) {
		return false;
	}
	pState->LastSync = iNow;
	return true;
}



/* 串行格式化、精确滚动并用一次完整文件写提交记录。 */
static xlogresult __xrtLogFileWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	xlogfilestate* pState = (xlogfilestate*)pUserData;
	xlogfilewriter Writer;
	size_t iWritten = 0;
	uint64 iThread = __xrtCurrentThreadId();
	bool bResult = false;
	bool bFormatted;

	if ( xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) == iThread ) {
		return XLOG_RESULT_DROPPED;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return XLOG_RESULT_ERROR;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	memset(&Writer, 0, sizeof(Writer));
	Writer.State = pState;
	xrtBufferClear(&pState->Record);
	bFormatted = pState->Format(
			pRecord,
			__xrtLogFileBufferWrite,
			&Writer,
			pState->UserData
		);
	if ( !bFormatted || Writer.Failed ) {
		if ( !Writer.Limited ) {
			__xrtLogFileWrap(
				XERR_IO,
				XLOG_ERROR_FILE_FORMAT,
				"file-format",
				"failed to format the log record"
			);
		}
		goto Finish;
	}
	if (
		__xrtLogFileShouldRotate(pState, pState->Record.Size) &&
		!__xrtLogFileRotateLocked(pState)
	) {
		goto Finish;
	}
	if ( (pState->File == NULL) && !__xrtLogFileReopenLocked(pState) ) {
		goto Finish;
	}
	if (
		!xrtWriteFull(
			pState->File,
			pState->Record.Data,
			pState->Record.Size,
			&iWritten
		)
	) {
		pState->CurrentBytes += (uint64)iWritten;
		pState->WrittenBytes += (uint64)iWritten;
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_WRITE,
			"file-write",
			"failed to write the complete log record"
		);
		goto Finish;
	}
	pState->CurrentBytes += (uint64)iWritten;
	pState->WrittenBytes += (uint64)iWritten;
	pState->Records++;
	if ( !__xrtLogFileSyncRecord(pState) ) {
		goto Finish;
	}
	bResult = true;

Finish:
	__xrtLogFileRecordClear(pState);
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return XLOG_RESULT_ERROR;
	}
	return bResult ? XLOG_RESULT_WRITTEN : XLOG_RESULT_ERROR;
}



/* 显式 Flush 始终执行稳定存储提交，不受自动策略影响。 */
static bool __xrtLogFileFlush(ptr pUserData)
{
	xlogfilestate* pState = (xlogfilestate*)pUserData;
	uint64 iThread = __xrtCurrentThreadId();
	bool bResult;

	if ( xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) == iThread ) {
		return true;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	bResult = __xrtLogFileSyncLocked(pState);
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return false;
	}
	return bResult;
}



/* 释放文件状态、格式器数据和动态记录缓冲。 */
static void __xrtLogFileDrop(ptr pUserData)
{
	xlogfilestate* pState = (xlogfilestate*)pUserData;

	if ( pState->File != NULL ) {
		(void)xrtClose(pState->File);
	}
	xrtBufferUnit(&pState->Record);
	(void)xrtMutexUnit(&pState->Lock);
	if ( pState->Drop != NULL ) {
		pState->Drop(pState->UserData);
	}
	xrtFree(pState);
}



/* 释放尚未交给 Sink 的文件状态，不接管格式器数据。 */
static void __xrtLogFileAbort(xlogfilestate* pState)
{
	if ( pState->File != NULL ) {
		(void)xrtClose(pState->File);
	}
	xrtBufferUnit(&pState->Record);
	(void)xrtMutexUnit(&pState->Lock);
	xrtFree(pState);
}



/* 验证公共 Sink 身份并取得文件状态。 */
static xlogfilestate* __xrtLogFileState(const xlogsink* pSink)
{
	ptr pUserData = NULL;

	if ( !__xrtLogSinkData(pSink, __xrtLogFileWrite, &pUserData) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	return (xlogfilestate*)pUserData;
}



/* 初始化常用文件选项。 */
XRT_API bool xrtLogFileOptionsInit(
	xlogfileoptions* pOptions,
	cstr sPath
)
{
	if ( (pOptions == NULL) || (sPath == NULL) || (sPath[0] == 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	memset(pOptions, 0, sizeof(xlogfileoptions));
	pOptions->Path = sPath;
	pOptions->Level = XLOG_INFO;
	pOptions->Mode = XLOG_FILE_APPEND;
	pOptions->Sync = XLOG_FILE_SYNC_MANUAL;
	pOptions->RecordLimit = XLOG_FILE_RECORD_LIMIT_DEFAULT;
	pOptions->BufferLimit = XLOG_FILE_BUFFER_LIMIT_DEFAULT;
	pOptions->SyncInterval = XRT_TIME_SECOND;
	return true;
}



/* 创建通用文件 Sink，并只在成功后接管格式器数据。 */
XRT_API xlogsink* xrtLogFile(const xlogfileconfig* pConfig)
{
	xlogfilestate* pState;
	xlogsinkconfig SinkConfig;
	xlogsink* pSink;
	size_t iPathSize;
	size_t iSlotSize;
	size_t iStateSize;
	uint64 iFileSize = 0;

	if (
		(pConfig == NULL) ||
		(pConfig->Format == NULL) ||
		!__xrtLogFileOptionsValid(&pConfig->Options)
	) {
		__xrtLogFileErrorCause(
			XERR_ARGUMENT,
			XLOG_ERROR_FILE_CONFIG,
			"file-create",
			"invalid log file configuration",
			NULL
		);
		return NULL;
	}
	iPathSize = strlen(pConfig->Options.Path);
	if (
		(iPathSize > (SIZE_MAX - 12u)) ||
		((iPathSize + 12u) > ((SIZE_MAX - sizeof(xlogfilestate)) / 3u))
	) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iSlotSize = iPathSize + 12u;
	iStateSize = sizeof(xlogfilestate) + (iSlotSize * 3u);
	pState = (xlogfilestate*)xrtCalloc(1u, iStateSize);
	if ( pState == NULL ) {
		return NULL;
	}
	if ( !xrtMutexInit(&pState->Lock) ) {
		xrtFree(pState);
		return NULL;
	}
	xrtAtomic64Init(&pState->Owner, 0);
	(void)xrtBufferInit(&pState->Record);
	pState->Options = pConfig->Options;
	pState->Format = pConfig->Format;
	pState->Drop = pConfig->Drop;
	pState->UserData = pConfig->UserData;
	pState->PathSize = iPathSize;
	pState->Path = pState->Text;
	pState->BackupSource = pState->Text + iSlotSize;
	pState->BackupTarget = pState->Text + (iSlotSize * 2u);
	memcpy((str)pState->Path, pConfig->Options.Path, iPathSize + 1u);
	memcpy(pState->BackupSource, pConfig->Options.Path, iPathSize + 1u);
	memcpy(pState->BackupTarget, pConfig->Options.Path, iPathSize + 1u);
	pState->Options.Path = pState->Path;
	pState->File = __xrtLogFileOpenPath(
		pState->Path,
		pState->Options.Mode == XLOG_FILE_TRUNCATE,
		&iFileSize
	);
	if ( pState->File == NULL ) {
		__xrtLogFileWrap(
			XERR_IO,
			XLOG_ERROR_FILE_OPEN,
			"file-create",
			"failed to open the log file"
		);
		__xrtLogFileAbort(pState);
		return NULL;
	}
	pState->CurrentBytes = iFileSize;
	memset(&SinkConfig, 0, sizeof(SinkConfig));
	SinkConfig.Name = XRT_STR_LITERAL("file");
	SinkConfig.Level = pState->Options.Level;
	SinkConfig.Write = __xrtLogFileWrite;
	SinkConfig.Flush = __xrtLogFileFlush;
	SinkConfig.Drop = __xrtLogFileDrop;
	SinkConfig.UserData = pState;
	pSink = xrtLogSinkCreate(&SinkConfig);
	if ( pSink == NULL ) {
		__xrtLogFileAbort(pState);
		return NULL;
	}
	return pSink;
}



/* 返回文件 Sink 的稳定路径。 */
XRT_API cstr xrtLogFilePath(const xlogsink* pSink)
{
	xlogfilestate* pState = __xrtLogFileState(pSink);

	return pState != NULL ? pState->Path : NULL;
}



/* 在文件锁下读取一致统计。 */
XRT_API bool xrtLogFileStats(
	const xlogsink* pSink,
	xlogfilestats* pStats
)
{
	xlogfilestate* pState = __xrtLogFileState(pSink);
	uint64 iThread = __xrtCurrentThreadId();
	bool bUnlocked;

	if ( (pState == NULL) || (pStats == NULL) ) {
		if ( pStats == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}
	if ( xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) == iThread ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}
	pStats->CurrentBytes = pState->CurrentBytes;
	pStats->WrittenBytes = pState->WrittenBytes;
	pStats->Records = pState->Records;
	pStats->Rotations = pState->Rotations;
	pStats->Reopens = pState->Reopens;
	pStats->Syncs = pState->Syncs;
	bUnlocked = xrtMutexUnlock(&pState->Lock);
	return bUnlocked;
}



/* 立即执行一次手动滚动。 */
XRT_API bool xrtLogFileRotate(xlogsink* pSink)
{
	xlogfilestate* pState = __xrtLogFileState(pSink);
	uint64 iThread = __xrtCurrentThreadId();
	bool bResult;

	if ( pState == NULL ) {
		return false;
	}
	if ( xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) == iThread ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	bResult = __xrtLogFileRotateLocked(pState);
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return false;
	}
	return bResult;
}



/* 立即切换到当前路径的新追加句柄。 */
XRT_API bool xrtLogFileReopen(xlogsink* pSink)
{
	xlogfilestate* pState = __xrtLogFileState(pSink);
	uint64 iThread = __xrtCurrentThreadId();
	bool bResult;

	if ( pState == NULL ) {
		return false;
	}
	if ( xrtAtomic64Load(&pState->Owner, XMEMORY_ACQUIRE) == iThread ) {
		__xrtErrorSetInvalidState();
		return false;
	}
	if ( !xrtMutexLock(&pState->Lock) ) {
		return false;
	}
	xrtAtomic64Store(&pState->Owner, iThread, XMEMORY_RELEASE);
	bResult = __xrtLogFileReopenLocked(pState);
	xrtAtomic64Store(&pState->Owner, 0, XMEMORY_RELEASE);
	if ( !xrtMutexUnlock(&pState->Lock) ) {
		return false;
	}
	return bResult;
}

#endif
