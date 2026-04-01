


#if !defined(_WIN32) && !defined(_WIN64)
	#include <errno.h>
	#include <fcntl.h>
	#include <signal.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <termios.h>
#endif

#define __XPROC_READ_CHUNK_DEFAULT	4096u
#define __XPROC_TIMEOUT_GRACE_MS	250u
#define __XPROC_EVENT_COUNT_DEFAULT	1024u
#define __XPROC_TERMINAL_COLS_DEFAULT	120u
#define __XPROC_TERMINAL_ROWS_DEFAULT	30u

#define __XPROC_STREAM_STDOUT		1
#define __XPROC_STREAM_STDERR		2
#define __XPROC_STREAM_TERMINAL		3

typedef struct {
	char* sData;
	size_t iSize;
	size_t iCap;
} __xproc_strbuf;

typedef struct {
	wchar_t* sData;
	size_t iSize;
	size_t iCap;
} __xproc_wbuf;

typedef struct {
	char* pData;
	size_t iSize;
	size_t iCap;
	uint64 iBaseOffset;
	uint64 iNextOffset;
	bool bTruncated;
	bool bDone;
} __xproc_streambuf;

typedef struct {
	xprocessevent* arrItems;
	uint32 iCount;
	uint32 iCap;
	uint64 iBaseSeq;
	uint64 iNextSeq;
	bool bDone;
} __xproc_eventbuf;

typedef struct {
	int iTargetKind;
	str sProgram;
	str* arrArgs;
	uint32 iArgCount;
	str sCommand;
	str sWorkDir;
	str* arrEnv;
	uint32 iEnvCount;
	bool bInheritEnv;
	bool bUseTerminal;
	bool bMergeStderr;
	bool bCreateProcessGroup;
	bool bHideWindow;
	uint32 iTerminalCols;
	uint32 iTerminalRows;
	uint32 iReadChunkSize;
	size_t iMaxCaptureBytes;
	uint32 iMaxEventCount;
	int iStdinMode;
	int iStdoutMode;
	int iStderrMode;
	bool bStdoutCapture;
	bool bStderrCapture;
	const xprocessevents* pEvents;
	ptr pUserData;
} __xproc_plan;

typedef struct {
	const char** arrArgv;
	uint32 iArgCount;
} __xproc_resolved_cmd;

typedef struct {
	xprocess* pProcess;
	int iStream;
} __xproc_pump_ctx;

typedef struct {
	uint32 iCols;
	uint32 iRows;
} __xproc_terminal_size;

#if defined(_WIN32) || defined(_WIN64)
	static void __xprocCloseTerminalHandlePlatform(HANDLE hTerminal);
#endif

#if !defined(_WIN32) && !defined(_WIN64)
	typedef struct {
		int iStage;
		int iOsError;
	} __xproc_child_error;
#endif


// 内部函数：初始化退出信息
static void __xprocExitInfoInit(xprocessexitinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iKind = XPROC_EXIT_NONE;
	pInfo->iExitCode = -1;
}


// 内部函数：初始化读取信息
static void __xprocReadInfoInit(xprocessreadinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
}


// 内部函数：初始化事件读取信息
static void __xprocEventReadInfoInit(xprocesseventreadinfo* pInfo)
{
	if ( pInfo == NULL ) {
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
}


// 内部函数：设置进程结果
static void __xprocResultInit(xprocessresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	memset(pResult, 0, sizeof(*pResult));
	__xprocExitInfoInit(&pResult->ExitInfo);
	pResult->iExitCode = -1;
}


// 内部函数：预留字符串缓冲区
static bool __xprocStrBufReserve(__xproc_strbuf* pBuf, size_t iNeed)
{
	size_t iCapNew;
	char* sNew;

	if ( pBuf == NULL ) {
		return false;
	}
	if ( iNeed <= pBuf->iCap ) {
		return true;
	}

	iCapNew = pBuf->iCap ? pBuf->iCap : 64u;
	while ( iCapNew < iNeed ) {
		size_t iNext = (iCapNew < 1024u) ? (iCapNew * 2u) : (iCapNew + (iCapNew / 2u));
		if ( iNext <= iCapNew ) {
			iCapNew = iNeed;
			break;
		}
		iCapNew = iNext;
	}

	sNew = (char*)xrtRealloc(pBuf->sData, iCapNew);
	if ( sNew == NULL ) {
		return false;
	}

	pBuf->sData = sNew;
	pBuf->iCap = iCapNew;
	return true;
}


// 内部函数：追加字符串缓冲区原始数据
static bool __xprocStrBufAppendRaw(__xproc_strbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL ) {
		return false;
	}
	if ( iLen == 0u ) {
		return true;
	}
	if ( !__xprocStrBufReserve(pBuf, pBuf->iSize + iLen + 1u) ) {
		return false;
	}

	memcpy(pBuf->sData + pBuf->iSize, sText, iLen);
	pBuf->iSize += iLen;
	pBuf->sData[pBuf->iSize] = '\0';
	return true;
}


// 内部函数：追加字符串缓冲区文本
static bool __xprocStrBufAppend(__xproc_strbuf* pBuf, const char* sText)
{
	if ( sText == NULL ) {
		return true;
	}
	return __xprocStrBufAppendRaw(pBuf, sText, strlen(sText));
}


// 内部函数：追加字符串缓冲区字符
static bool __xprocStrBufAppendChar(__xproc_strbuf* pBuf, char ch)
{
	return __xprocStrBufAppendRaw(pBuf, &ch, 1u);
}


// 内部函数：释放字符串缓冲区
static void __xprocStrBufUnit(__xproc_strbuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->sData ) {
		xrtFree(pBuf->sData);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}


// 内部函数：预留宽字符缓冲区
static bool __xprocWBufReserve(__xproc_wbuf* pBuf, size_t iNeed)
{
	size_t iCapNew;
	wchar_t* sNew;

	if ( pBuf == NULL ) {
		return false;
	}
	if ( iNeed <= pBuf->iCap ) {
		return true;
	}

	iCapNew = pBuf->iCap ? pBuf->iCap : 64u;
	while ( iCapNew < iNeed ) {
		size_t iNext = (iCapNew < 1024u) ? (iCapNew * 2u) : (iCapNew + (iCapNew / 2u));
		if ( iNext <= iCapNew ) {
			iCapNew = iNeed;
			break;
		}
		iCapNew = iNext;
	}

	sNew = (wchar_t*)xrtRealloc(pBuf->sData, iCapNew * sizeof(wchar_t));
	if ( sNew == NULL ) {
		return false;
	}

	pBuf->sData = sNew;
	pBuf->iCap = iCapNew;
	return true;
}


// 内部函数：追加宽字符缓冲区原始数据
static bool __xprocWBufAppendRaw(__xproc_wbuf* pBuf, const wchar_t* sText, size_t iLen)
{
	if ( pBuf == NULL ) {
		return false;
	}
	if ( iLen == 0u ) {
		return true;
	}
	if ( !__xprocWBufReserve(pBuf, pBuf->iSize + iLen) ) {
		return false;
	}

	memcpy(pBuf->sData + pBuf->iSize, sText, iLen * sizeof(wchar_t));
	pBuf->iSize += iLen;
	return true;
}


// 内部函数：释放宽字符缓冲区
static void __xprocWBufUnit(__xproc_wbuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->sData ) {
		xrtFree(pBuf->sData);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}


// 内部函数：预留输出缓冲区
static bool __xprocStreamReserve(__xproc_streambuf* pBuf, size_t iNeed)
{
	size_t iCapNew;
	char* pNew;

	if ( pBuf == NULL ) {
		return false;
	}
	if ( iNeed <= pBuf->iCap ) {
		return true;
	}

	iCapNew = pBuf->iCap ? pBuf->iCap : 128u;
	while ( iCapNew < iNeed ) {
		size_t iNext = (iCapNew < 4096u) ? (iCapNew * 2u) : (iCapNew + (iCapNew / 2u));
		if ( iNext <= iCapNew ) {
			iCapNew = iNeed;
			break;
		}
		iCapNew = iNext;
	}

	pNew = (char*)xrtRealloc(pBuf->pData, iCapNew);
	if ( pNew == NULL ) {
		return false;
	}

	pBuf->pData = pNew;
	pBuf->iCap = iCapNew;
	return true;
}


// 内部函数：丢弃输出缓冲区前缀
static void __xprocStreamDropPrefix(__xproc_streambuf* pBuf, size_t iDrop)
{
	if ( pBuf == NULL || iDrop == 0u ) {
		return;
	}
	if ( iDrop >= pBuf->iSize ) {
		pBuf->iBaseOffset += (uint64)pBuf->iSize;
		pBuf->iSize = 0u;
		if ( pBuf->pData ) {
			pBuf->pData[0] = '\0';
		}
		pBuf->bTruncated = true;
		return;
	}

	memmove(pBuf->pData, pBuf->pData + iDrop, pBuf->iSize - iDrop);
	pBuf->iSize -= iDrop;
	pBuf->iBaseOffset += (uint64)iDrop;
	pBuf->pData[pBuf->iSize] = '\0';
	pBuf->bTruncated = true;
}


// 内部函数：在保留失败时丢弃所有捕获
static void __xprocStreamDropAll(__xproc_streambuf* pBuf, size_t iIncoming)
{
	if ( pBuf == NULL ) {
		return;
	}
	pBuf->iNextOffset += (uint64)iIncoming;
	pBuf->iBaseOffset = pBuf->iNextOffset;
	pBuf->iSize = 0u;
	pBuf->bTruncated = true;
	if ( pBuf->pData ) {
		pBuf->pData[0] = '\0';
	}
}


// 内部函数：追加输出缓冲区
static void __xprocStreamAppend(__xproc_streambuf* pBuf, const void* pData, size_t iSize, size_t iLimit)
{
	const char* sData = (const char*)pData;

	if ( pBuf == NULL || pData == NULL || iSize == 0u ) {
		return;
	}

	if ( iLimit != 0u ) {
		if ( iSize >= iLimit ) {
			size_t iKeepOffset = iSize - iLimit;
			if ( !__xprocStreamReserve(pBuf, iLimit + 1u) ) {
				__xprocStreamDropAll(pBuf, iSize);
				return;
			}
			memcpy(pBuf->pData, sData + iKeepOffset, iLimit);
			pBuf->iBaseOffset = pBuf->iNextOffset + (uint64)iKeepOffset;
			pBuf->iNextOffset += (uint64)iSize;
			pBuf->iSize = iLimit;
			pBuf->pData[pBuf->iSize] = '\0';
			pBuf->bTruncated = true;
			return;
		}
		if ( pBuf->iSize + iSize > iLimit ) {
			__xprocStreamDropPrefix(pBuf, (pBuf->iSize + iSize) - iLimit);
		}
	}

	if ( !__xprocStreamReserve(pBuf, pBuf->iSize + iSize + 1u) ) {
		__xprocStreamDropAll(pBuf, iSize);
		return;
	}

	memcpy(pBuf->pData + pBuf->iSize, sData, iSize);
	pBuf->iSize += iSize;
	pBuf->iNextOffset += (uint64)iSize;
	pBuf->pData[pBuf->iSize] = '\0';
}


// 内部函数：释放输出缓冲区
static void __xprocStreamUnit(__xproc_streambuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->pData ) {
		xrtFree(pBuf->pData);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}


// 内部函数：输出快照复制
static ptr __xprocStreamSnapshot(const __xproc_streambuf* pBuf, size_t* piSize)
{
	char* sCopy;

	if ( piSize ) {
		*piSize = 0u;
	}
	if ( pBuf == NULL || pBuf->iSize == 0u ) {
		return NULL;
	}

	sCopy = (char*)xrtMalloc(pBuf->iSize + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, pBuf->pData, pBuf->iSize);
	sCopy[pBuf->iSize] = '\0';
	if ( piSize ) {
		*piSize = pBuf->iSize;
	}
	return sCopy;
}


// 内部函数：按偏移复制输出
static ptr __xprocStreamReadSince(const __xproc_streambuf* pBuf, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo)
{
	uint64 iStartOffset;
	uint64 iAvailable;
	size_t iCopySize;
	char* sCopy;

	if ( piSize ) {
		*piSize = 0u;
	}
	__xprocReadInfoInit(pInfo);
	if ( pBuf == NULL ) {
		return NULL;
	}

	if ( pInfo ) {
		pInfo->iBaseOffset = pBuf->iBaseOffset;
		pInfo->iStreamEndOffset = pBuf->iNextOffset;
		pInfo->iNextOffset = (iOffset > pBuf->iNextOffset) ? pBuf->iNextOffset : iOffset;
		pInfo->bDone = pBuf->bDone;
	}

	iStartOffset = (iOffset < pBuf->iBaseOffset) ? pBuf->iBaseOffset : iOffset;
	if ( iStartOffset >= pBuf->iNextOffset ) {
		if ( pInfo ) {
			pInfo->iNextOffset = iStartOffset;
		}
		return NULL;
	}

	iAvailable = pBuf->iNextOffset - iStartOffset;
	if ( iMaxBytes != 0u && iAvailable > (uint64)iMaxBytes ) {
		iCopySize = iMaxBytes;
	} else {
		iCopySize = (size_t)iAvailable;
	}

	if ( iCopySize == 0u ) {
		if ( pInfo ) {
			pInfo->iNextOffset = iStartOffset;
		}
		return NULL;
	}

	sCopy = (char*)xrtMalloc(iCopySize + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, pBuf->pData + (size_t)(iStartOffset - pBuf->iBaseOffset), iCopySize);
	sCopy[iCopySize] = '\0';

	if ( piSize ) {
		*piSize = iCopySize;
	}
	if ( pInfo ) {
		pInfo->iBaseOffset = pBuf->iBaseOffset;
		pInfo->iStreamEndOffset = pBuf->iNextOffset;
		pInfo->iNextOffset = iStartOffset + (uint64)iCopySize;
		pInfo->bDone = pBuf->bDone;
	}
	return sCopy;
}


// 内部函数：移动输出缓冲区
static void __xprocStreamMoveOut(__xproc_streambuf* pBuf, ptr* ppData, size_t* piSize, uint64* piBaseOffset, bool* pbTruncated)
{
	if ( ppData ) {
		*ppData = pBuf ? pBuf->pData : NULL;
	}
	if ( piSize ) {
		*piSize = pBuf ? pBuf->iSize : 0u;
	}
	if ( piBaseOffset ) {
		*piBaseOffset = pBuf ? pBuf->iBaseOffset : 0u;
	}
	if ( pbTruncated ) {
		*pbTruncated = pBuf ? pBuf->bTruncated : false;
	}
	if ( pBuf ) {
		pBuf->pData = NULL;
		pBuf->iSize = 0u;
		pBuf->iCap = 0u;
		pBuf->iBaseOffset = 0u;
		pBuf->iNextOffset = 0u;
		pBuf->bTruncated = false;
		pBuf->bDone = false;
	}
}


// 内部函数：释放事件缓冲区
static void __xprocEventBufUnit(__xproc_eventbuf* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->arrItems ) {
		xrtFree(pBuf->arrItems);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}


// 内部函数：预留事件缓冲区
static bool __xprocEventBufReserve(__xproc_eventbuf* pBuf, uint32 iNeed)
{
	uint32 iCapNew;
	xprocessevent* arrNew;

	if ( pBuf == NULL ) {
		return false;
	}
	if ( iNeed <= pBuf->iCap ) {
		return true;
	}

	iCapNew = pBuf->iCap ? pBuf->iCap : 16u;
	while ( iCapNew < iNeed ) {
		if ( iCapNew > UINT32_MAX / 2u ) {
			iCapNew = iNeed;
			break;
		}
		iCapNew *= 2u;
	}

	arrNew = (xprocessevent*)xrtRealloc(pBuf->arrItems, (size_t)iCapNew * sizeof(xprocessevent));
	if ( arrNew == NULL ) {
		return false;
	}

	pBuf->arrItems = arrNew;
	pBuf->iCap = iCapNew;
	return true;
}


// 内部函数：追加事件
static void __xprocEventBufAppend(__xproc_eventbuf* pBuf, uint32 iLimit, const xprocessevent* pEvent)
{
	if ( pBuf == NULL || pEvent == NULL ) {
		return;
	}

	if ( iLimit == 0u ) {
		iLimit = __XPROC_EVENT_COUNT_DEFAULT;
	}

	if ( pBuf->iBaseSeq == 0u && pBuf->iNextSeq == 0u ) {
		pBuf->iBaseSeq = 1u;
		pBuf->iNextSeq = 1u;
	}

	if ( pBuf->iCount >= iLimit ) {
		if ( pBuf->iCount > 1u ) {
			memmove(pBuf->arrItems, pBuf->arrItems + 1, (size_t)(pBuf->iCount - 1u) * sizeof(xprocessevent));
		}
		pBuf->iCount--;
		pBuf->iBaseSeq++;
	}

	if ( !__xprocEventBufReserve(pBuf, pBuf->iCount + 1u) ) {
		if ( pBuf->iCount > 0u ) {
			if ( pBuf->iCount > 1u ) {
				memmove(pBuf->arrItems, pBuf->arrItems + 1, (size_t)(pBuf->iCount - 1u) * sizeof(xprocessevent));
			}
			pBuf->iCount--;
			pBuf->iBaseSeq++;
			if ( !__xprocEventBufReserve(pBuf, pBuf->iCount + 1u) ) {
				pBuf->iNextSeq++;
				return;
			}
		} else {
			pBuf->iNextSeq++;
			return;
		}
	}

	pBuf->arrItems[pBuf->iCount] = *pEvent;
	pBuf->arrItems[pBuf->iCount].iSeq = pBuf->iNextSeq++;
	pBuf->iCount++;
}


// 内部函数：按序号复制事件
static xprocessevent* __xprocEventBufReadSince(const __xproc_eventbuf* pBuf, uint64 iSeq, uint32 iMaxCount, uint32* piCount, xprocesseventreadinfo* pInfo)
{
	uint64 iStartSeq;
	uint32 iStartIndex;
	uint32 iCopyCount;
	xprocessevent* arrCopy;

	if ( piCount ) {
		*piCount = 0u;
	}
	__xprocEventReadInfoInit(pInfo);
	if ( pBuf == NULL ) {
		return NULL;
	}

	if ( pInfo ) {
		pInfo->iBaseSeq = pBuf->iBaseSeq;
		pInfo->iEventEndSeq = pBuf->iNextSeq;
		pInfo->iNextSeq = (iSeq > pBuf->iNextSeq) ? pBuf->iNextSeq : iSeq;
		pInfo->bDone = pBuf->bDone;
	}

	iStartSeq = (iSeq < pBuf->iBaseSeq) ? pBuf->iBaseSeq : iSeq;
	if ( iStartSeq >= pBuf->iNextSeq || pBuf->iCount == 0u ) {
		if ( pInfo ) {
			pInfo->iNextSeq = iStartSeq;
		}
		return NULL;
	}

	iStartIndex = (uint32)(iStartSeq - pBuf->iBaseSeq);
	iCopyCount = pBuf->iCount - iStartIndex;
	if ( iMaxCount != 0u && iCopyCount > iMaxCount ) {
		iCopyCount = iMaxCount;
	}
	if ( iCopyCount == 0u ) {
		if ( pInfo ) {
			pInfo->iNextSeq = iStartSeq;
		}
		return NULL;
	}

	arrCopy = (xprocessevent*)xrtMalloc((size_t)iCopyCount * sizeof(xprocessevent));
	if ( arrCopy == NULL ) {
		return NULL;
	}

	memcpy(arrCopy, pBuf->arrItems + iStartIndex, (size_t)iCopyCount * sizeof(xprocessevent));
	if ( piCount ) {
		*piCount = iCopyCount;
	}
	if ( pInfo ) {
		pInfo->iBaseSeq = pBuf->iBaseSeq;
		pInfo->iEventEndSeq = pBuf->iNextSeq;
		pInfo->iNextSeq = iStartSeq + (uint64)iCopyCount;
		pInfo->bDone = pBuf->bDone;
	}
	return arrCopy;
}


// 初始化进程配置
XXAPI void xrtProcessConfigInit(xprocessconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iTargetKind = XPROC_TARGET_EXEC;
	pConfig->bInheritEnv = true;
	pConfig->bCreateProcessGroup = true;
	pConfig->iTerminalCols = __XPROC_TERMINAL_COLS_DEFAULT;
	pConfig->iTerminalRows = __XPROC_TERMINAL_ROWS_DEFAULT;
	pConfig->iReadChunkSize = __XPROC_READ_CHUNK_DEFAULT;
	pConfig->iMaxEventCount = __XPROC_EVENT_COUNT_DEFAULT;
	pConfig->Stdin.iMode = XPROC_STDIO_INHERIT;
	pConfig->Stdout.iMode = XPROC_STDIO_INHERIT;
	pConfig->Stderr.iMode = XPROC_STDIO_INHERIT;
}


// 释放进程结果
XXAPI void xrtProcessResultUnit(xprocessresult* pResult)
{
	if ( pResult == NULL ) {
		return;
	}
	if ( pResult->pStdout ) {
		xrtFree(pResult->pStdout);
	}
	if ( pResult->pStderr ) {
		xrtFree(pResult->pStderr);
	}
	__xprocResultInit(pResult);
}


// 内部函数：判断 stdio 模式是否合法
static bool __xprocIsValidStdioMode(int iMode)
{
	return iMode == XPROC_STDIO_INHERIT || iMode == XPROC_STDIO_PIPE || iMode == XPROC_STDIO_NULL;
}


// 内部函数：校验环境变量名
static bool __xprocValidateEnvEntry(str sEntry)
{
	const char* sEquals;
	size_t i;

	if ( sEntry == NULL || sEntry[0] == '\0' ) {
		return false;
	}

	sEquals = strchr((const char*)sEntry, '=');
	if ( sEquals == NULL || sEquals == (const char*)sEntry ) {
		return false;
	}

	if ( !isalpha((unsigned char)sEntry[0]) && sEntry[0] != '_' ) {
		return false;
	}
	for ( i = 1u; ((const char*)sEntry + i) < sEquals; i++ ) {
		unsigned char ch = (unsigned char)sEntry[i];
		if ( !isalnum(ch) && ch != '_' ) {
			return false;
		}
	}
	return true;
}


// 内部函数：写入结构化启动失败信息
static void __xprocSetSpawnFailureInfo(xprocessexitinfo* pInfo, int iStage, int iOsError)
{
	if ( pInfo == NULL ) {
		return;
	}
	__xprocExitInfoInit(pInfo);
	pInfo->iKind = XPROC_EXIT_SPAWN_FAILED;
	pInfo->iStage = iStage;
	pInfo->iOsError = iOsError;
}


// 内部函数：从配置生成执行计划
static bool __xprocPlanFromConfig(const xprocessconfig* pConfig, __xproc_plan* pPlan)
{
	uint32 i;
	int iTargetKind;

	if ( pPlan ) {
		memset(pPlan, 0, sizeof(*pPlan));
	}
	if ( pConfig == NULL || pPlan == NULL ) {
		xrtSetError("invalid subprocess config.", FALSE);
		return false;
	}

	iTargetKind = pConfig->iTargetKind ? pConfig->iTargetKind : XPROC_TARGET_EXEC;
	if ( iTargetKind != XPROC_TARGET_EXEC && iTargetKind != XPROC_TARGET_SHELL ) {
		xrtSetError("invalid subprocess target kind.", FALSE);
		return false;
	}
	if ( iTargetKind == XPROC_TARGET_EXEC ) {
		if ( pConfig->sProgram == NULL || pConfig->sProgram[0] == '\0' ) {
			xrtSetError("subprocess requires program path.", FALSE);
			return false;
		}
	}
	if ( pConfig->iArgCount > 0u && pConfig->arrArgs == NULL ) {
		xrtSetError("subprocess args are missing.", FALSE);
		return false;
	}
	if ( pConfig->iEnvCount > 0u && pConfig->arrEnv == NULL ) {
		xrtSetError("subprocess env list is missing.", FALSE);
		return false;
	}
	for ( i = 0u; i < pConfig->iEnvCount; i++ ) {
		if ( !__xprocValidateEnvEntry(pConfig->arrEnv[i]) ) {
			xrtSetError("invalid subprocess env entry.", FALSE);
			return false;
		}
	}

	pPlan->iTargetKind = iTargetKind;
	pPlan->sProgram = pConfig->sProgram;
	pPlan->arrArgs = pConfig->arrArgs;
	pPlan->iArgCount = pConfig->iArgCount;
	pPlan->sCommand = (pConfig->sCommand && pConfig->sCommand[0]) ? pConfig->sCommand : NULL;
	pPlan->sWorkDir = (pConfig->sWorkDir && pConfig->sWorkDir[0]) ? pConfig->sWorkDir : NULL;
	pPlan->arrEnv = pConfig->arrEnv;
	pPlan->iEnvCount = pConfig->iEnvCount;
	pPlan->bInheritEnv = pConfig->bInheritEnv ? true : false;
	pPlan->bUseTerminal = pConfig->bUseTerminal ? true : false;
	pPlan->bMergeStderr = pConfig->bMergeStderr ? true : false;
	pPlan->bCreateProcessGroup = pConfig->bCreateProcessGroup ? true : false;
	pPlan->bHideWindow = pConfig->bHideWindow ? true : false;
	pPlan->iTerminalCols = pConfig->iTerminalCols ? pConfig->iTerminalCols : __XPROC_TERMINAL_COLS_DEFAULT;
	pPlan->iTerminalRows = pConfig->iTerminalRows ? pConfig->iTerminalRows : __XPROC_TERMINAL_ROWS_DEFAULT;
	pPlan->iReadChunkSize = pConfig->iReadChunkSize ? pConfig->iReadChunkSize : __XPROC_READ_CHUNK_DEFAULT;
	pPlan->iMaxCaptureBytes = pConfig->iMaxCaptureBytes;
	pPlan->iMaxEventCount = pConfig->iMaxEventCount ? pConfig->iMaxEventCount : __XPROC_EVENT_COUNT_DEFAULT;
	pPlan->iStdinMode = pConfig->Stdin.iMode;
	pPlan->iStdoutMode = pConfig->Stdout.iMode;
	pPlan->iStderrMode = pConfig->Stderr.iMode;
	pPlan->pEvents = pConfig->pEvents;
	pPlan->pUserData = pConfig->pUserData;

	if ( !__xprocIsValidStdioMode(pPlan->iStdinMode) || !__xprocIsValidStdioMode(pPlan->iStdoutMode) || !__xprocIsValidStdioMode(pPlan->iStderrMode) ) {
		xrtSetError("invalid subprocess stdio mode.", FALSE);
		return false;
	}

	pPlan->bStdoutCapture = pConfig->Stdout.bCapture ? true : false;
	pPlan->bStderrCapture = pConfig->Stderr.bCapture ? true : false;

	if ( pPlan->bUseTerminal ) {
		pPlan->bMergeStderr = true;
		pPlan->iStdinMode = XPROC_STDIO_PIPE;
		pPlan->iStdoutMode = XPROC_STDIO_PIPE;
		pPlan->iStderrMode = XPROC_STDIO_NULL;
		pPlan->bStdoutCapture = true;
		pPlan->bStderrCapture = false;
	}

	if ( pPlan->iStdoutMode == XPROC_STDIO_PIPE ) {
		pPlan->bStdoutCapture = true;
	}
	if ( pPlan->iStderrMode == XPROC_STDIO_PIPE && !pPlan->bMergeStderr ) {
		pPlan->bStderrCapture = true;
	}
	if ( pPlan->pEvents && pPlan->pEvents->OnStdout ) {
		pPlan->bStdoutCapture = true;
	}
	if ( pPlan->pEvents && pPlan->pEvents->OnStderr && !pPlan->bMergeStderr ) {
		pPlan->bStderrCapture = true;
	}
	if ( pPlan->bMergeStderr && pPlan->pEvents && pPlan->pEvents->OnStderr ) {
		pPlan->bStdoutCapture = true;
	}
	if ( pPlan->bMergeStderr && pConfig->Stderr.bCapture ) {
		pPlan->bStdoutCapture = true;
	}

	if ( pPlan->bStdoutCapture ) {
		if ( pPlan->iStdoutMode == XPROC_STDIO_NULL ) {
			xrtSetError("subprocess stdout cannot capture from null.", FALSE);
			return false;
		}
		pPlan->iStdoutMode = XPROC_STDIO_PIPE;
	}
	if ( pPlan->bStderrCapture ) {
		if ( pPlan->iStderrMode == XPROC_STDIO_NULL ) {
			xrtSetError("subprocess stderr cannot capture from null.", FALSE);
			return false;
		}
		pPlan->iStderrMode = XPROC_STDIO_PIPE;
	}

	return true;
}


// 内部函数：判断参数是否需要引用
static bool __xprocCmdNeedsQuote(str sArg)
{
	size_t i;

	if ( sArg == NULL || sArg[0] == '\0' ) {
		return true;
	}
	for ( i = 0u; sArg[i] != '\0'; i++ ) {
		switch ( sArg[i] ) {
			case ' ':
			case '\t':
			case '\n':
			case '\v':
			case '"':
				return true;
			default:
				break;
		}
	}
	return false;
}


// 内部函数：追加 Windows 引用参数
static bool __xprocCmdAppendQuoted(__xproc_strbuf* pBuf, str sArg)
{
	size_t i = 0u;
	size_t iBackslash = 0u;

	if ( sArg == NULL ) {
		sArg = (str)"";
	}
	if ( !__xprocCmdNeedsQuote(sArg) ) {
		return __xprocStrBufAppend(pBuf, (const char*)sArg);
	}
	if ( !__xprocStrBufAppendChar(pBuf, '"') ) {
		return false;
	}
	for ( ;; ) {
		char ch = ((const char*)sArg)[i];
		if ( ch == '\\' ) {
			iBackslash++;
			i++;
			continue;
		}
		if ( ch == '"' ) {
			while ( iBackslash > 0u ) {
				if ( !__xprocStrBufAppendChar(pBuf, '\\') || !__xprocStrBufAppendChar(pBuf, '\\') ) {
					return false;
				}
				iBackslash--;
			}
			if ( !__xprocStrBufAppendChar(pBuf, '\\') || !__xprocStrBufAppendChar(pBuf, '"') ) {
				return false;
			}
			i++;
			continue;
		}
		while ( iBackslash > 0u ) {
			if ( !__xprocStrBufAppendChar(pBuf, '\\') ) {
				return false;
			}
			iBackslash--;
		}
		if ( ch == '\0' ) {
			break;
		}
		if ( !__xprocStrBufAppendChar(pBuf, ch) ) {
			return false;
		}
		i++;
	}
	while ( iBackslash > 0u ) {
		if ( !__xprocStrBufAppendChar(pBuf, '\\') || !__xprocStrBufAppendChar(pBuf, '\\') ) {
			return false;
		}
		iBackslash--;
	}
	return __xprocStrBufAppendChar(pBuf, '"');
}


// 内部函数：判断是否为 powershell 风格
static bool __xprocIsPowerShellProgram(str sProgram)
{
	const char* sName;

	if ( sProgram == NULL || sProgram[0] == '\0' ) {
		return false;
	}
	sName = strrchr((const char*)sProgram, '\\');
	if ( sName == NULL ) {
		sName = strrchr((const char*)sProgram, '/');
	}
	sName = sName ? (sName + 1) : (const char*)sProgram;
	return strcasecmp(sName, "powershell") == 0
		|| strcasecmp(sName, "powershell.exe") == 0
		|| strcasecmp(sName, "pwsh") == 0
		|| strcasecmp(sName, "pwsh.exe") == 0;
}


// 内部函数：释放解析后的命令
static void __xprocResolvedCmdUnit(__xproc_resolved_cmd* pResolved)
{
	if ( pResolved == NULL ) {
		return;
	}
	if ( pResolved->arrArgv ) {
		xrtFree((ptr)pResolved->arrArgv);
	}
	memset(pResolved, 0, sizeof(*pResolved));
}


// 内部函数：解析执行命令
static bool __xprocResolveCommand(const __xproc_plan* pPlan, __xproc_resolved_cmd* pResolved)
{
	const char** arrArgv;
	uint32 iWrite = 0u;
	uint32 iExtra = 0u;
	const char* sProgram;

	if ( pPlan == NULL || pResolved == NULL ) {
		return false;
	}
	memset(pResolved, 0, sizeof(*pResolved));

	if ( pPlan->iTargetKind == XPROC_TARGET_EXEC ) {
		sProgram = (const char*)pPlan->sProgram;
	} else {
		#if defined(_WIN32) || defined(_WIN64)
			sProgram = (pPlan->sProgram && pPlan->sProgram[0]) ? (const char*)pPlan->sProgram : "cmd.exe";
		#else
			sProgram = (pPlan->sProgram && pPlan->sProgram[0]) ? (const char*)pPlan->sProgram : "/bin/sh";
		#endif
	}

	if ( pPlan->iTargetKind == XPROC_TARGET_SHELL && pPlan->sCommand ) {
		#if defined(_WIN32) || defined(_WIN64)
			iExtra = __xprocIsPowerShellProgram((str)sProgram) ? 3u : 3u;
		#else
			iExtra = 1u;
		#endif
	}

	arrArgv = (const char**)xrtCalloc((size_t)(1u + pPlan->iArgCount + iExtra + (pPlan->sCommand ? 1u : 0u) + 1u), sizeof(char*));
	if ( arrArgv == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return false;
	}

	arrArgv[iWrite++] = sProgram;
	for ( uint32 i = 0u; i < pPlan->iArgCount; i++ ) {
		arrArgv[iWrite++] = (const char*)pPlan->arrArgs[i];
	}

	if ( pPlan->iTargetKind == XPROC_TARGET_SHELL && pPlan->sCommand ) {
		#if defined(_WIN32) || defined(_WIN64)
			if ( __xprocIsPowerShellProgram((str)sProgram) ) {
				arrArgv[iWrite++] = "-NoLogo";
				arrArgv[iWrite++] = "-NoProfile";
				arrArgv[iWrite++] = "-Command";
			} else {
				arrArgv[iWrite++] = "/D";
				arrArgv[iWrite++] = "/S";
				arrArgv[iWrite++] = "/C";
			}
		#else
			arrArgv[iWrite++] = "-c";
		#endif
			arrArgv[iWrite++] = (const char*)pPlan->sCommand;
	}

	arrArgv[iWrite] = NULL;
	pResolved->arrArgv = arrArgv;
	pResolved->iArgCount = iWrite;
	return true;
}


#if defined(XRT_NO_THREAD)

// 内部函数：设置线程 required 错误
static void __xprocSetThreadRequiredError(void)
{
	xrtSetError("subprocess requires thread support.", FALSE);
}


// 当前平台是否支持 terminal 模式
XXAPI bool xrtProcessTerminalSupported(void)
{
	return false;
}


// xrtProcessSpawn 相关处理
XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
{
	(void)pConfig;
	__xprocSetThreadRequiredError();
	return NULL;
}


// 销毁进程
XXAPI void xrtProcessDestroy(xprocess* pProcess)
{
	(void)pProcess;
}


// 获取进程状态
XXAPI int xrtProcessState(xprocess* pProcess)
{
	(void)pProcess;
	return XPROC_STATE_FAILED;
}


// 判断是否为进程运行中
XXAPI bool xrtProcessIsRunning(xprocess* pProcess)
{
	(void)pProcess;
	return false;
}


// 获取进程退出码
XXAPI int xrtProcessExitCode(xprocess* pProcess)
{
	(void)pProcess;
	return -1;
}


// 获取结构化退出信息
XXAPI bool xrtProcessGetExitInfo(xprocess* pProcess, xprocessexitinfo* pInfo)
{
	(void)pProcess;
	if ( pInfo ) {
		__xprocExitInfoInit(pInfo);
	}
	__xprocSetThreadRequiredError();
	return false;
}


// 写入进程
XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize)
{
	(void)pProcess;
	(void)pData;
	(void)iSize;
	__xprocSetThreadRequiredError();
	return -1;
}


// 写入进程文本
XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize)
{
	(void)pProcess;
	(void)sText;
	(void)iSize;
	__xprocSetThreadRequiredError();
	return -1;
}


// 关闭进程标准输入
XXAPI bool xrtProcessCloseStdin(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 等待进程
XXAPI bool xrtProcessWait(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 等待进程超时
XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs)
{
	(void)pProcess;
	(void)iTimeoutMs;
	__xprocSetThreadRequiredError();
	return XRT_WAIT_ERROR;
}


// 中断进程
XXAPI bool xrtProcessInterrupt(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 温和终止进程
XXAPI bool xrtProcessTerminate(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 强制终止进程
XXAPI bool xrtProcessKill(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 终止进程树
XXAPI bool xrtProcessKillTree(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}


// 调整 terminal 尺寸
XXAPI bool xrtProcessResizeTerminal(xprocess* pProcess, uint32 iCols, uint32 iRows)
{
	(void)pProcess;
	(void)iCols;
	(void)iRows;
	__xprocSetThreadRequiredError();
	return false;
}


// 获取进程标准输出
XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize)
{
	(void)pProcess;
	if ( piSize ) {
		*piSize = 0u;
	}
	return NULL;
}


// 获取进程标准错误
XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize)
{
	(void)pProcess;
	if ( piSize ) {
		*piSize = 0u;
	}
	return NULL;
}


// 读取标准输出增量
XXAPI ptr xrtProcessReadStdoutSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo)
{
	(void)pProcess;
	(void)iOffset;
	(void)iMaxBytes;
	if ( piSize ) {
		*piSize = 0u;
	}
	__xprocReadInfoInit(pInfo);
	return NULL;
}


// 读取标准错误增量
XXAPI ptr xrtProcessReadStderrSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo)
{
	(void)pProcess;
	(void)iOffset;
	(void)iMaxBytes;
	if ( piSize ) {
		*piSize = 0u;
	}
	__xprocReadInfoInit(pInfo);
	return NULL;
}


// 读取事件增量
XXAPI xprocessevent* xrtProcessReadEventsSince(xprocess* pProcess, uint64 iSeq, uint32 iMaxCount, uint32* piCount, xprocesseventreadinfo* pInfo)
{
	(void)pProcess;
	(void)iSeq;
	(void)iMaxCount;
	if ( piCount ) {
		*piCount = 0u;
	}
	__xprocEventReadInfoInit(pInfo);
	return NULL;
}


// xrtExecCapture 相关处理
XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs)
{
	(void)pConfig;
	(void)pResult;
	(void)iTimeoutMs;
	__xprocSetThreadRequiredError();
	return false;
}

#if !defined(XRT_NO_NETWORK)
// 等待进程 Future
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return NULL;
}
#endif

#else

struct xprocess_struct {
	volatile long iRefCount;
	volatile int iState;
	volatile int bExitReady;
	volatile int bStdoutDone;
	volatile int bStderrDone;
	uint32 iReadChunkSize;
	size_t iMaxCaptureBytes;
	uint32 iMaxEventCount;
	int iStdinMode;
	int iStdoutMode;
	int iStderrMode;
	volatile int iRequestedStop;
	volatile int bStopTimedOut;
	volatile int bStopCancelled;
	bool bUseTerminal;
	bool bStdoutCapture;
	bool bStderrCapture;
	bool bMergeStderr;
	bool bCreateProcessGroup;
	bool bHideWindow;
	uint32 iTerminalCols;
	uint32 iTerminalRows;
	xprocessevents Events;
	ptr pUserData;
	xprocessexitinfo ExitInfo;
	xmutex_struct Lock;
	xcond_struct Cond;
	__xproc_streambuf StdoutBuf;
	__xproc_streambuf StderrBuf;
	__xproc_eventbuf EventBuf;
	xthread hWaitThread;
	xthread hStdoutThread;
	xthread hStderrThread;
	__xproc_pump_ctx StdoutPump;
	__xproc_pump_ctx StderrPump;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hProcess;
		DWORD iProcessId;
		HANDLE hStdinWrite;
		HANDLE hStdoutRead;
		HANDLE hStderrRead;
		HANDLE hJob;
		HANDLE hTerminal;
	#else
		pid_t iPid;
		int fdStdinWrite;
		int fdStdoutRead;
		int fdStderrRead;
	#endif
	#if !defined(XRT_NO_NETWORK)
		xfuture* pWaitFuture;
		xpromise* pWaitPromise;
	#endif
};

static void __xprocFreeProcess(xprocess* pProcess);


// 内部函数：增加引用
static xprocess* __xprocAddRef(xprocess* pProcess)
{
	if ( pProcess ) {
		(void)__xrtAtomicAddFetch32(&pProcess->iRefCount, 1);
	}
	return pProcess;
}


// 内部函数：释放引用
static void __xprocReleaseProcess(xprocess* pProcess)
{
	if ( pProcess && __xrtAtomicAddFetch32(&pProcess->iRefCount, -1) == 0 ) {
		__xprocFreeProcess(pProcess);
	}
}

#if !defined(XRT_NO_NETWORK)
// 内部函数：wait future 清理
static void __xprocWaitFutureCleanup(xfuture* pFuture)
{
	xprocess* pProcess;

	if ( pFuture == NULL ) {
		return;
	}

	pProcess = (xprocess*)pFuture->pPendingCtx;
	pFuture->pPendingCtx = NULL;
	pFuture->pfnPendingCleanup = NULL;
	if ( pProcess == NULL ) {
		return;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( pProcess->pWaitFuture == pFuture ) {
		pProcess->pWaitFuture = NULL;
	}
	pProcess->pWaitPromise = NULL;
	xrtMutexUnlock(&pProcess->Lock);
	__xprocReleaseProcess(pProcess);
}


// 内部函数：分离 wait future
static void __xprocDetachWaitFuture(xprocess* pProcess)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;

	if ( pProcess == NULL ) {
		return;
	}

	xrtMutexLock(&pProcess->Lock);
	pFuture = pProcess->pWaitFuture;
	pPromise = pProcess->pWaitPromise;
	pProcess->pWaitFuture = NULL;
	pProcess->pWaitPromise = NULL;
	xrtMutexUnlock(&pProcess->Lock);

	if ( pPromise ) {
		xPromiseDestroy(pPromise);
	}
	if ( pFuture ) {
		xFutureRelease(pFuture);
	}
}
#endif


// 内部函数：当前时间毫秒
static uint64 __xprocNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64)GetTickCount64();
	#else
		struct timespec tNow;
		#if defined(CLOCK_MONOTONIC)
			if ( clock_gettime(CLOCK_MONOTONIC, &tNow) == 0 ) {
				return ((uint64)tNow.tv_sec * 1000ULL) + ((uint64)tNow.tv_nsec / 1000000ULL);
			}
		#endif
		if ( clock_gettime(CLOCK_REALTIME, &tNow) == 0 ) {
			return ((uint64)tNow.tv_sec * 1000ULL) + ((uint64)tNow.tv_nsec / 1000000ULL);
		}
		return 0u;
	#endif
}


// 内部函数：分配进程对象
static xprocess* __xprocAllocProcess(const __xproc_plan* pPlan)
{
	xprocess* pProcess = (xprocess*)xrtCalloc(1, sizeof(xprocess));

	if ( pProcess == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}

	pProcess->iRefCount = 1;
	pProcess->iState = XPROC_STATE_INIT;
	pProcess->iReadChunkSize = pPlan->iReadChunkSize;
	pProcess->iMaxCaptureBytes = pPlan->iMaxCaptureBytes;
	pProcess->iMaxEventCount = pPlan->iMaxEventCount;
	pProcess->iStdinMode = pPlan->iStdinMode;
	pProcess->iStdoutMode = pPlan->iStdoutMode;
	pProcess->iStderrMode = pPlan->iStderrMode;
	pProcess->bUseTerminal = pPlan->bUseTerminal;
	pProcess->bStdoutCapture = pPlan->bStdoutCapture;
	pProcess->bStderrCapture = pPlan->bStderrCapture;
	pProcess->bMergeStderr = pPlan->bMergeStderr;
	pProcess->bCreateProcessGroup = pPlan->bCreateProcessGroup;
	pProcess->bHideWindow = pPlan->bHideWindow;
	pProcess->iTerminalCols = pPlan->iTerminalCols;
	pProcess->iTerminalRows = pPlan->iTerminalRows;
	pProcess->pUserData = pPlan->pUserData;
	if ( pPlan->pEvents ) {
		pProcess->Events = *pPlan->pEvents;
	}
	__xprocExitInfoInit(&pProcess->ExitInfo);
	xrtMutexInit(&pProcess->Lock);
	xrtCondInit(&pProcess->Cond);

	#if defined(_WIN32) || defined(_WIN64)
		pProcess->hProcess = NULL;
		pProcess->iProcessId = 0u;
		pProcess->hStdinWrite = NULL;
		pProcess->hStdoutRead = NULL;
		pProcess->hStderrRead = NULL;
		pProcess->hJob = NULL;
		pProcess->hTerminal = NULL;
	#else
		pProcess->iPid = -1;
		pProcess->fdStdinWrite = -1;
		pProcess->fdStdoutRead = -1;
		pProcess->fdStderrRead = -1;
	#endif

	return pProcess;
}


// 内部函数：释放进程对象
static void __xprocFreeProcess(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}

	#if !defined(XRT_NO_NETWORK)
		if ( pProcess->pWaitPromise ) {
			xPromiseDestroy(pProcess->pWaitPromise);
			pProcess->pWaitPromise = NULL;
		}
		pProcess->pWaitFuture = NULL;
	#endif

	__xprocStreamUnit(&pProcess->StdoutBuf);
	__xprocStreamUnit(&pProcess->StderrBuf);
	__xprocEventBufUnit(&pProcess->EventBuf);
	xrtCondUnit(&pProcess->Cond);
	xrtMutexUnit(&pProcess->Lock);
	xrtFree(pProcess);
}


// 内部函数：销毁线程句柄
static void __xprocDestroyThreads(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}
	if ( pProcess->hWaitThread ) {
		xrtThreadDestroy(pProcess->hWaitThread);
		pProcess->hWaitThread = NULL;
	}
	if ( pProcess->hStdoutThread ) {
		xrtThreadDestroy(pProcess->hStdoutThread);
		pProcess->hStdoutThread = NULL;
	}
	if ( pProcess->hStderrThread ) {
		xrtThreadDestroy(pProcess->hStderrThread);
		pProcess->hStderrThread = NULL;
	}
}


// 内部函数：关闭 stdin 句柄
static void __xprocCloseStdinHandle(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hStdinWrite ) {
			CloseHandle(pProcess->hStdinWrite);
			pProcess->hStdinWrite = NULL;
		}
	#else
		if ( pProcess->fdStdinWrite >= 0 ) {
			close(pProcess->fdStdinWrite);
			pProcess->fdStdinWrite = -1;
		}
	#endif
}


// 内部函数：关闭 stdout 读句柄
static void __xprocCloseStdoutReadHandle(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hStdoutRead ) {
			CloseHandle(pProcess->hStdoutRead);
			pProcess->hStdoutRead = NULL;
		}
	#else
		if ( pProcess->fdStdoutRead >= 0 ) {
			close(pProcess->fdStdoutRead);
			pProcess->fdStdoutRead = -1;
		}
	#endif
}


// 内部函数：关闭 stderr 读句柄
static void __xprocCloseStderrReadHandle(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hStderrRead ) {
			CloseHandle(pProcess->hStderrRead);
			pProcess->hStderrRead = NULL;
		}
	#else
		if ( pProcess->fdStderrRead >= 0 ) {
			close(pProcess->fdStderrRead);
			pProcess->fdStderrRead = -1;
		}
	#endif
}


// 内部函数：关闭平台句柄
static void __xprocClosePlatformHandles(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}

	__xprocCloseStdinHandle(pProcess);
	__xprocCloseStdoutReadHandle(pProcess);
	__xprocCloseStderrReadHandle(pProcess);

	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess ) {
			CloseHandle(pProcess->hProcess);
			pProcess->hProcess = NULL;
		}
		if ( pProcess->hJob ) {
			CloseHandle(pProcess->hJob);
			pProcess->hJob = NULL;
		}
		if ( pProcess->hTerminal ) {
			__xprocCloseTerminalHandlePlatform(pProcess->hTerminal);
			pProcess->hTerminal = NULL;
		}
		pProcess->iProcessId = 0u;
	#else
		pProcess->iPid = -1;
	#endif
}


// 内部函数：初始化事件
static void __xprocEventInit(xprocessevent* pEvent)
{
	if ( pEvent == NULL ) {
		return;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	__xprocExitInfoInit(&pEvent->ExitInfo);
}


// 内部函数：在锁保护下压入事件
static void __xprocPushEventLocked(xprocess* pProcess, int iKind, int iStream, uint64 iOffset, uint64 iSize, const xprocessexitinfo* pExitInfo)
{
	xprocessevent tEvent;

	if ( pProcess == NULL ) {
		return;
	}

	__xprocEventInit(&tEvent);
	tEvent.iKind = iKind;
	tEvent.iStream = iStream;
	tEvent.iOffset = iOffset;
	tEvent.iSize = iSize;
	tEvent.tTimeMs = __xprocNowMs();
	if ( pExitInfo ) {
		tEvent.ExitInfo = *pExitInfo;
	}
	__xprocEventBufAppend(&pProcess->EventBuf, pProcess->iMaxEventCount, &tEvent);
}


#if defined(_WIN32) || defined(_WIN64)
	#if !defined(PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE)
		#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
	#endif
	#if !defined(EXTENDED_STARTUPINFO_PRESENT)
		#define EXTENDED_STARTUPINFO_PRESENT 0x00080000
	#endif

	typedef HRESULT (WINAPI *procCreatePseudoConsole)(COORD size, HANDLE hInput, HANDLE hOutput, DWORD iFlags, HANDLE* phConsole);
	typedef void (WINAPI *procClosePseudoConsole)(HANDLE hConsole);
	typedef HRESULT (WINAPI *procResizePseudoConsole)(HANDLE hConsole, COORD size);
	typedef BOOL (WINAPI *procInitializeProcThreadAttributeList)(void* pAttributeList, DWORD iAttributeCount, DWORD iFlags, SIZE_T* piSize);
	typedef BOOL (WINAPI *procUpdateProcThreadAttribute)(void* pAttributeList, DWORD iFlags, DWORD_PTR iAttribute, PVOID pValue, SIZE_T iSize, PVOID pPrevValue, SIZE_T* piReturnSize);
	typedef VOID (WINAPI *procDeleteProcThreadAttributeList)(void* pAttributeList);

	typedef struct {
		STARTUPINFOW StartupInfo;
		void* lpAttributeList;
	} __xproc_startupinfoexw;

	typedef struct {
		bool bLoaded;
		bool bSupported;
		procCreatePseudoConsole CreatePseudoConsole;
		procClosePseudoConsole ClosePseudoConsole;
		procResizePseudoConsole ResizePseudoConsole;
		procInitializeProcThreadAttributeList InitializeProcThreadAttributeList;
		procUpdateProcThreadAttribute UpdateProcThreadAttribute;
		procDeleteProcThreadAttributeList DeleteProcThreadAttributeList;
	} __xproc_conpty_api;

	static __xproc_conpty_api __gxprocConPtyApi;

	static void __xprocLoadConPtyApi(void)
	{
		HMODULE hKernel;

		if ( __gxprocConPtyApi.bLoaded ) {
			return;
		}

		memset(&__gxprocConPtyApi, 0, sizeof(__gxprocConPtyApi));
		__gxprocConPtyApi.bLoaded = true;

		hKernel = GetModuleHandleW(L"kernel32.dll");
		if ( hKernel == NULL ) {
			return;
		}

		__gxprocConPtyApi.CreatePseudoConsole = (procCreatePseudoConsole)GetProcAddress(hKernel, "CreatePseudoConsole");
		__gxprocConPtyApi.ClosePseudoConsole = (procClosePseudoConsole)GetProcAddress(hKernel, "ClosePseudoConsole");
		__gxprocConPtyApi.ResizePseudoConsole = (procResizePseudoConsole)GetProcAddress(hKernel, "ResizePseudoConsole");
		__gxprocConPtyApi.InitializeProcThreadAttributeList = (procInitializeProcThreadAttributeList)GetProcAddress(hKernel, "InitializeProcThreadAttributeList");
		__gxprocConPtyApi.UpdateProcThreadAttribute = (procUpdateProcThreadAttribute)GetProcAddress(hKernel, "UpdateProcThreadAttribute");
		__gxprocConPtyApi.DeleteProcThreadAttributeList = (procDeleteProcThreadAttributeList)GetProcAddress(hKernel, "DeleteProcThreadAttributeList");
		__gxprocConPtyApi.bSupported = __gxprocConPtyApi.CreatePseudoConsole != NULL
			&& __gxprocConPtyApi.ClosePseudoConsole != NULL
			&& __gxprocConPtyApi.ResizePseudoConsole != NULL
			&& __gxprocConPtyApi.InitializeProcThreadAttributeList != NULL
			&& __gxprocConPtyApi.UpdateProcThreadAttribute != NULL
			&& __gxprocConPtyApi.DeleteProcThreadAttributeList != NULL;
	}


	// 内部函数：判断 terminal 是否可用
	static bool __xprocTerminalSupportedPlatform(void)
	{
		__xprocLoadConPtyApi();
		return __gxprocConPtyApi.bSupported;
	}


	// 内部函数：关闭 Windows terminal 句柄
	static void __xprocCloseTerminalHandlePlatform(HANDLE hTerminal)
	{
		__xprocLoadConPtyApi();
		if ( hTerminal != NULL && __gxprocConPtyApi.ClosePseudoConsole != NULL ) {
			__gxprocConPtyApi.ClosePseudoConsole(hTerminal);
		}
	}


	// 内部函数：调整 Windows terminal 尺寸
	static bool __xprocResizeTerminalPlatformHandle(HANDLE hTerminal, uint32 iCols, uint32 iRows)
	{
		COORD tSize;

		__xprocLoadConPtyApi();
		if ( hTerminal == NULL || __gxprocConPtyApi.ResizePseudoConsole == NULL ) {
			SetLastError(ERROR_NOT_SUPPORTED);
			return false;
		}

		tSize.X = (SHORT)(iCols ? iCols : __XPROC_TERMINAL_COLS_DEFAULT);
		tSize.Y = (SHORT)(iRows ? iRows : __XPROC_TERMINAL_ROWS_DEFAULT);
		return SUCCEEDED(__gxprocConPtyApi.ResizePseudoConsole(hTerminal, tSize));
	}
#else
	static bool __xprocTerminalSupportedPlatform(void)
	{
		return true;
	}
#endif


// 内部函数：标记输出流结束
static void __xprocMarkStreamDone(xprocess* pProcess, int iStream)
{
	if ( pProcess == NULL ) {
		return;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( iStream == __XPROC_STREAM_STDOUT || iStream == __XPROC_STREAM_TERMINAL ) {
		pProcess->bStdoutDone = true;
		pProcess->StdoutBuf.bDone = true;
	} else {
		pProcess->bStderrDone = true;
		pProcess->StderrBuf.bDone = true;
	}
	xrtCondBroadcast(&pProcess->Cond);
	xrtMutexUnlock(&pProcess->Lock);
}


// 内部函数：处理输出
static void __xprocHandleOutput(xprocess* pProcess, int iStream, const void* pData, size_t iSize)
{
	__xproc_streambuf* pBuf = NULL;
	bool bCapture = false;
	void (*procOutput)(xprocess*, const void*, size_t, ptr) = NULL;
	uint64 iEventOffset = 0u;
	int iEventStream = XPROC_STREAM_NONE;

	if ( pProcess == NULL || pData == NULL || iSize == 0u ) {
		return;
	}

	if ( iStream == __XPROC_STREAM_TERMINAL ) {
		pBuf = &pProcess->StdoutBuf;
		bCapture = pProcess->bStdoutCapture;
		procOutput = pProcess->Events.OnStdout;
		iEventStream = XPROC_STREAM_TERMINAL;
	} else if ( iStream == __XPROC_STREAM_STDERR && pProcess->bMergeStderr ) {
		pBuf = &pProcess->StdoutBuf;
		bCapture = pProcess->bStdoutCapture;
		procOutput = pProcess->Events.OnStdout ? pProcess->Events.OnStdout : pProcess->Events.OnStderr;
		iEventStream = XPROC_STREAM_STDOUT;
	} else if ( iStream == __XPROC_STREAM_STDERR ) {
		pBuf = &pProcess->StderrBuf;
		bCapture = pProcess->bStderrCapture;
		procOutput = pProcess->Events.OnStderr;
		iEventStream = XPROC_STREAM_STDERR;
	} else {
		pBuf = &pProcess->StdoutBuf;
		bCapture = pProcess->bStdoutCapture;
		procOutput = pProcess->Events.OnStdout;
		iEventStream = XPROC_STREAM_STDOUT;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( pBuf ) {
		iEventOffset = pBuf->iNextOffset;
	}
	if ( bCapture ) {
		__xprocStreamAppend(pBuf, pData, iSize, pProcess->iMaxCaptureBytes);
	} else if ( pBuf ) {
		__xprocStreamDropAll(pBuf, iSize);
	}
	__xprocPushEventLocked(pProcess, XPROC_EVENT_OUTPUT, iEventStream, iEventOffset, (uint64)iSize, NULL);
	xrtMutexUnlock(&pProcess->Lock);

	if ( procOutput ) {
		procOutput(pProcess, pData, iSize, pProcess->pUserData);
	}
}


// 内部函数：读取线程
static uint32 __xprocPumpThread(ptr pArg)
{
	__xproc_pump_ctx* pCtx = (__xproc_pump_ctx*)pArg;
	xprocess* pProcess = pCtx ? pCtx->pProcess : NULL;
	uint32 iChunk = (pProcess && pProcess->iReadChunkSize) ? pProcess->iReadChunkSize : __XPROC_READ_CHUNK_DEFAULT;
	char* sBuf = NULL;

	if ( pProcess == NULL || (pCtx->iStream != __XPROC_STREAM_STDOUT && pCtx->iStream != __XPROC_STREAM_STDERR && pCtx->iStream != __XPROC_STREAM_TERMINAL) ) {
		return 1u;
	}

	sBuf = (char*)xrtMalloc(iChunk);
	if ( sBuf == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		__xprocMarkStreamDone(pProcess, pCtx->iStream);
		return 2u;
	}

	for ( ;; ) {
		#if defined(_WIN32) || defined(_WIN64)
			HANDLE hRead = (pCtx->iStream == __XPROC_STREAM_STDOUT || pCtx->iStream == __XPROC_STREAM_TERMINAL) ? pProcess->hStdoutRead : pProcess->hStderrRead;
			DWORD iRead = 0u;

			if ( hRead == NULL ) {
				break;
			}
			if ( !ReadFile(hRead, sBuf, iChunk, &iRead, NULL) ) {
				DWORD iErr = GetLastError();
				if ( iErr == ERROR_BROKEN_PIPE || iErr == ERROR_HANDLE_EOF ) {
					break;
				}
				break;
			}
			if ( iRead == 0u ) {
				break;
			}
			__xprocHandleOutput(pProcess, pCtx->iStream, sBuf, (size_t)iRead);
		#else
			int iFd = (pCtx->iStream == __XPROC_STREAM_STDOUT || pCtx->iStream == __XPROC_STREAM_TERMINAL) ? pProcess->fdStdoutRead : pProcess->fdStderrRead;
			ssize_t iRead;

			if ( iFd < 0 ) {
				break;
			}
			do {
				iRead = read(iFd, sBuf, iChunk);
			} while ( iRead < 0 && errno == EINTR );
			if ( iRead <= 0 ) {
				break;
			}
			__xprocHandleOutput(pProcess, pCtx->iStream, sBuf, (size_t)iRead);
		#endif
	}

	xrtFree(sBuf);
	if ( pCtx->iStream == __XPROC_STREAM_STDOUT ) {
		__xprocCloseStdoutReadHandle(pProcess);
	} else {
		__xprocCloseStderrReadHandle(pProcess);
	}
	__xprocMarkStreamDone(pProcess, pCtx->iStream);
	return 0u;
}


#if defined(_WIN32) || defined(_WIN64)

// 内部函数：打开 NUL 句柄
static HANDLE __xprocOpenWindowsNullHandle(DWORD iAccess)
{
	SECURITY_ATTRIBUTES tSa;

	memset(&tSa, 0, sizeof(tSa));
	tSa.nLength = sizeof(tSa);
	tSa.bInheritHandle = TRUE;
	return CreateFileW(L"NUL", iAccess, FILE_SHARE_READ | FILE_SHARE_WRITE, &tSa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}


// 内部函数：判断宽字符环境项是否被覆盖
static bool __xprocWindowsEnvEntryOverridden(const wchar_t* sEntry, str* arrEnv, uint32 iEnvCount)
{
	uint32 i;

	if ( sEntry == NULL ) {
		return false;
	}
	for ( i = 0u; i < iEnvCount; i++ ) {
		const char* sOverride = (const char*)arrEnv[i];
		const char* sEquals = strchr(sOverride, '=');
		size_t iNameLen;
		size_t j;

		if ( sEquals == NULL ) {
			continue;
		}
		iNameLen = (size_t)(sEquals - sOverride);
		if ( sEntry[0] == L'=' ) {
			continue;
		}
		for ( j = 0u; j < iNameLen; j++ ) {
			if ( sEntry[j] == L'\0' || sEntry[j] == L'=' ) {
				break;
			}
			if ( towlower(sEntry[j]) != towlower((wchar_t)(unsigned char)sOverride[j]) ) {
				break;
			}
		}
		if ( j == iNameLen && sEntry[j] == L'=' ) {
			return true;
		}
	}
	return false;
}


// 内部函数：构建 Windows 环境块
static wchar_t* __xprocBuildWindowsEnvBlock(const __xproc_plan* pPlan)
{
	__xproc_wbuf tBuf;
	wchar_t chZero = L'\0';
	bool bHasEntry = false;
	LPWCH sBaseEnv = NULL;

	memset(&tBuf, 0, sizeof(tBuf));

	if ( pPlan == NULL ) {
		return NULL;
	}
	if ( pPlan->bInheritEnv && pPlan->iEnvCount == 0u ) {
		return NULL;
	}

	if ( pPlan->bInheritEnv ) {
		const wchar_t* sItem;

		sBaseEnv = GetEnvironmentStringsW();
		if ( sBaseEnv == NULL ) {
			xrtSetError("failed to read subprocess environment.", FALSE);
			return NULL;
		}
		for ( sItem = sBaseEnv; *sItem != L'\0'; sItem += wcslen(sItem) + 1u ) {
			size_t iLen = wcslen(sItem);
			if ( __xprocWindowsEnvEntryOverridden(sItem, pPlan->arrEnv, pPlan->iEnvCount) ) {
				continue;
			}
			if ( !__xprocWBufAppendRaw(&tBuf, sItem, iLen + 1u) ) {
				xrtSetError("memory allocate failed.", FALSE);
				__xprocWBufUnit(&tBuf);
				FreeEnvironmentStringsW(sBaseEnv);
				return NULL;
			}
			bHasEntry = true;
		}
		FreeEnvironmentStringsW(sBaseEnv);
	}

	for ( uint32 i = 0u; i < pPlan->iEnvCount; i++ ) {
		u16str sEntryW = xrtUTF8to16((u8str)pPlan->arrEnv[i], 0u, NULL);
		size_t iLen;

		if ( sEntryW == NULL ) {
			xrtSetError("failed to convert subprocess env entry.", FALSE);
			__xprocWBufUnit(&tBuf);
			return NULL;
		}
		iLen = u16len(sEntryW);
		if ( !__xprocWBufAppendRaw(&tBuf, sEntryW, iLen + 1u) ) {
			xrtFree(sEntryW);
			xrtSetError("memory allocate failed.", FALSE);
			__xprocWBufUnit(&tBuf);
			return NULL;
		}
		xrtFree(sEntryW);
		bHasEntry = true;
	}

	if ( !__xprocWBufAppendRaw(&tBuf, &chZero, 1u) ) {
		xrtSetError("memory allocate failed.", FALSE);
		__xprocWBufUnit(&tBuf);
		return NULL;
	}
	if ( !bHasEntry ) {
		if ( !__xprocWBufAppendRaw(&tBuf, &chZero, 1u) ) {
			xrtSetError("memory allocate failed.", FALSE);
			__xprocWBufUnit(&tBuf);
			return NULL;
		}
	}

	return tBuf.sData;
}


// 内部函数：构建 Windows 命令行
static char* __xprocBuildWindowsCommandLine(const __xproc_resolved_cmd* pCmd)
{
	__xproc_strbuf tBuf;
	bool bOk = true;

	if ( pCmd == NULL || pCmd->arrArgv == NULL || pCmd->iArgCount == 0u ) {
		xrtSetError("invalid subprocess command line.", FALSE);
		return NULL;
	}

	memset(&tBuf, 0, sizeof(tBuf));
	for ( uint32 i = 0u; i < pCmd->iArgCount; i++ ) {
		if ( i > 0u ) {
			if ( !__xprocStrBufAppendChar(&tBuf, ' ') ) {
				bOk = false;
				break;
			}
		}
		if ( !__xprocCmdAppendQuoted(&tBuf, (str)pCmd->arrArgv[i]) ) {
			bOk = false;
			break;
		}
	}

	if ( !bOk ) {
		__xprocStrBufUnit(&tBuf);
		xrtSetError("failed to build subprocess command line.", FALSE);
		return NULL;
	}

	return tBuf.sData;
}


// 内部函数：确保 job object
static bool __xprocEnsureJobObject(xprocess* pProcess)
{
	HANDLE hJob;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION tInfo;

	if ( pProcess == NULL || pProcess->hProcess == NULL ) {
		return false;
	}
	if ( pProcess->hJob ) {
		return true;
	}

	hJob = CreateJobObjectW(NULL, NULL);
	if ( hJob == NULL ) {
		return false;
	}

	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if ( !SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &tInfo, sizeof(tInfo)) ) {
		CloseHandle(hJob);
		return false;
	}
	if ( !AssignProcessToJobObject(hJob, pProcess->hProcess) ) {
		CloseHandle(hJob);
		return false;
	}

	pProcess->hJob = hJob;
	return true;
}


// 内部函数：Windows terminal 启动
static bool __xprocSpawnWindowsTerminal(xprocess* pProcess, const __xproc_plan* pPlan, const __xproc_resolved_cmd* pCmd, xprocessexitinfo* pSpawnInfo)
{
	SECURITY_ATTRIBUTES tSa;
	__xproc_startupinfoexw tSi;
	PROCESS_INFORMATION tPi;
	SIZE_T iAttrListSize = 0u;
	void* pAttrList = NULL;
	HANDLE hTerminalInputRead = NULL;
	HANDLE hTerminalOutputWrite = NULL;
	char* sCmdUtf8 = NULL;
	u16str sCmdW = NULL;
	u16str sWorkDirW = NULL;
	wchar_t* sEnvBlockW = NULL;
	DWORD iCreateFlags = EXTENDED_STARTUPINFO_PRESENT;
	BOOL bOk = FALSE;
	COORD tSize;
	int iWorkDirError = 0;

	if ( !__xprocTerminalSupportedPlatform() ) {
		xrtSetError("subprocess terminal is not supported on this platform.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, ERROR_NOT_SUPPORTED);
		goto fail;
	}

	memset(&tSa, 0, sizeof(tSa));
	memset(&tSi, 0, sizeof(tSi));
	memset(&tPi, 0, sizeof(tPi));
	tSa.nLength = sizeof(tSa);
	tSa.bInheritHandle = TRUE;
	tSi.StartupInfo.cb = sizeof(tSi);
	tSi.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
	tSi.StartupInfo.hStdInput = NULL;
	tSi.StartupInfo.hStdOutput = NULL;
	tSi.StartupInfo.hStdError = NULL;

	if ( !CreatePipe(&hTerminalInputRead, &pProcess->hStdinWrite, &tSa, 0u) ) {
		xrtSetError("failed to create subprocess terminal input pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, (int)GetLastError());
		goto fail;
	}
	if ( !CreatePipe(&pProcess->hStdoutRead, &hTerminalOutputWrite, &tSa, 0u) ) {
		xrtSetError("failed to create subprocess terminal output pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, (int)GetLastError());
		goto fail;
	}
	(void)SetHandleInformation(pProcess->hStdinWrite, HANDLE_FLAG_INHERIT, 0u);
	(void)SetHandleInformation(pProcess->hStdoutRead, HANDLE_FLAG_INHERIT, 0u);

	tSize.X = (SHORT)(pPlan->iTerminalCols ? pPlan->iTerminalCols : __XPROC_TERMINAL_COLS_DEFAULT);
	tSize.Y = (SHORT)(pPlan->iTerminalRows ? pPlan->iTerminalRows : __XPROC_TERMINAL_ROWS_DEFAULT);
	if ( !SUCCEEDED(__gxprocConPtyApi.CreatePseudoConsole(tSize, hTerminalInputRead, hTerminalOutputWrite, 0u, &pProcess->hTerminal)) ) {
		xrtSetError("failed to create subprocess terminal.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, (int)GetLastError());
		goto fail;
	}

	(void)__gxprocConPtyApi.InitializeProcThreadAttributeList(NULL, 1u, 0u, &iAttrListSize);
	pAttrList = xrtMalloc(iAttrListSize);
	if ( pAttrList == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, ERROR_OUTOFMEMORY);
		goto fail;
	}
	if ( !__gxprocConPtyApi.InitializeProcThreadAttributeList(pAttrList, 1u, 0u, &iAttrListSize) ) {
		xrtSetError("failed to initialize subprocess terminal attribute list.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, (int)GetLastError());
		goto fail;
	}
	if ( !__gxprocConPtyApi.UpdateProcThreadAttribute(pAttrList, 0u, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pProcess->hTerminal, sizeof(pProcess->hTerminal), NULL, NULL) ) {
		xrtSetError("failed to configure subprocess terminal attribute.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, (int)GetLastError());
		goto fail;
	}
	tSi.lpAttributeList = pAttrList;

	sCmdUtf8 = __xprocBuildWindowsCommandLine(pCmd);
	if ( sCmdUtf8 == NULL ) {
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, (int)GetLastError());
		goto fail;
	}
	sCmdW = xrtUTF8to16((u8str)sCmdUtf8, 0u, NULL);
	if ( sCmdW == NULL ) {
		xrtSetError("failed to convert subprocess command line.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, 0);
		goto fail;
	}
	if ( pPlan->sWorkDir ) {
		DWORD iAttr;

		sWorkDirW = xrtUTF8to16((u8str)pPlan->sWorkDir, 0u, NULL);
		if ( sWorkDirW == NULL ) {
			xrtSetError("failed to convert subprocess working directory.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_WORKDIR, 0);
			goto fail;
		}
		iAttr = GetFileAttributesW(sWorkDirW);
		if ( iAttr == INVALID_FILE_ATTRIBUTES ) {
			iWorkDirError = (int)GetLastError();
		} else if ( !(iAttr & FILE_ATTRIBUTE_DIRECTORY) ) {
			iWorkDirError = ERROR_DIRECTORY;
		}
		if ( iWorkDirError != 0 ) {
			xrtSetError("invalid subprocess working directory.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_WORKDIR, iWorkDirError);
			goto fail;
		}
	}
	sEnvBlockW = __xprocBuildWindowsEnvBlock(pPlan);
	if ( sEnvBlockW != NULL ) {
		iCreateFlags |= CREATE_UNICODE_ENVIRONMENT;
	}
	if ( pPlan->bHideWindow ) {
		iCreateFlags |= CREATE_NO_WINDOW;
	}
	if ( pPlan->bCreateProcessGroup ) {
		iCreateFlags |= CREATE_NEW_PROCESS_GROUP;
	}

	bOk = CreateProcessW(NULL, sCmdW, NULL, NULL, FALSE, iCreateFlags, sEnvBlockW, sWorkDirW, &tSi.StartupInfo, &tPi);
	if ( !bOk ) {
		xrtSetError("failed to start subprocess terminal.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, (int)GetLastError());
		goto fail;
	}

	pProcess->hProcess = tPi.hProcess;
	pProcess->iProcessId = tPi.dwProcessId;
	if ( tPi.hThread ) {
		CloseHandle(tPi.hThread);
	}
	if ( hTerminalInputRead ) {
		CloseHandle(hTerminalInputRead);
		hTerminalInputRead = NULL;
	}
	if ( hTerminalOutputWrite ) {
		CloseHandle(hTerminalOutputWrite);
		hTerminalOutputWrite = NULL;
	}
	if ( pAttrList ) {
		__gxprocConPtyApi.DeleteProcThreadAttributeList(pAttrList);
		xrtFree(pAttrList);
		pAttrList = NULL;
	}
	if ( sCmdUtf8 ) {
		xrtFree(sCmdUtf8);
	}
	if ( sCmdW ) {
		xrtFree(sCmdW);
	}
	if ( sWorkDirW ) {
		xrtFree(sWorkDirW);
	}
	if ( sEnvBlockW ) {
		xrtFree(sEnvBlockW);
	}
	return true;

fail:
	if ( pAttrList ) {
		__gxprocConPtyApi.DeleteProcThreadAttributeList(pAttrList);
		xrtFree(pAttrList);
	}
	if ( hTerminalInputRead ) CloseHandle(hTerminalInputRead);
	if ( hTerminalOutputWrite ) CloseHandle(hTerminalOutputWrite);
	if ( tPi.hThread ) CloseHandle(tPi.hThread);
	if ( tPi.hProcess ) CloseHandle(tPi.hProcess);
	if ( sCmdUtf8 ) xrtFree(sCmdUtf8);
	if ( sCmdW ) xrtFree(sCmdW);
	if ( sWorkDirW ) xrtFree(sWorkDirW);
	if ( sEnvBlockW ) xrtFree(sEnvBlockW);
	__xprocClosePlatformHandles(pProcess);
	return false;
}


// 内部函数：平台启动
static bool __xprocSpawnPlatform(xprocess* pProcess, const __xproc_plan* pPlan, const __xproc_resolved_cmd* pCmd, xprocessexitinfo* pSpawnInfo)
{
	SECURITY_ATTRIBUTES tSa;
	STARTUPINFOW tSi;
	PROCESS_INFORMATION tPi;
	HANDLE hChildStdinRead = NULL;
	HANDLE hChildStdoutWrite = NULL;
	HANDLE hChildStderrWrite = NULL;
	HANDLE hNullStdin = NULL;
	HANDLE hNullStdout = NULL;
	HANDLE hNullStderr = NULL;
	HANDLE hStdInput = NULL;
	HANDLE hStdOutput = NULL;
	HANDLE hStdError = NULL;
	char* sCmdUtf8 = NULL;
	u16str sCmdW = NULL;
	u16str sWorkDirW = NULL;
	wchar_t* sEnvBlockW = NULL;
	BOOL bUseStdHandles = FALSE;
	BOOL bOk;
	DWORD iCreateFlags = 0u;

	if ( pPlan->bUseTerminal ) {
		return __xprocSpawnWindowsTerminal(pProcess, pPlan, pCmd, pSpawnInfo);
	}

	memset(&tSa, 0, sizeof(tSa));
	memset(&tSi, 0, sizeof(tSi));
	memset(&tPi, 0, sizeof(tPi));
	tSa.nLength = sizeof(tSa);
	tSa.bInheritHandle = TRUE;
	tSi.cb = sizeof(tSi);

	if ( pPlan->iStdinMode == XPROC_STDIO_PIPE ) {
		if ( !CreatePipe(&hChildStdinRead, &pProcess->hStdinWrite, &tSa, 0u) ) {
			xrtSetError("failed to create subprocess stdin pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, (int)GetLastError());
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStdinWrite, HANDLE_FLAG_INHERIT, 0u);
		hStdInput = hChildStdinRead;
		bUseStdHandles = TRUE;
	} else if ( pPlan->iStdinMode == XPROC_STDIO_NULL ) {
		hNullStdin = __xprocOpenWindowsNullHandle(GENERIC_READ);
		if ( hNullStdin == INVALID_HANDLE_VALUE || hNullStdin == NULL ) {
			xrtSetError("failed to open subprocess stdin null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, (int)GetLastError());
			goto fail;
		}
		hStdInput = hNullStdin;
		bUseStdHandles = TRUE;
	} else {
		hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	}

	if ( pPlan->iStdoutMode == XPROC_STDIO_PIPE ) {
		if ( !CreatePipe(&pProcess->hStdoutRead, &hChildStdoutWrite, &tSa, 0u) ) {
			xrtSetError("failed to create subprocess stdout pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, (int)GetLastError());
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStdoutRead, HANDLE_FLAG_INHERIT, 0u);
		hStdOutput = hChildStdoutWrite;
		bUseStdHandles = TRUE;
	} else if ( pPlan->iStdoutMode == XPROC_STDIO_NULL ) {
		hNullStdout = __xprocOpenWindowsNullHandle(GENERIC_WRITE);
		if ( hNullStdout == INVALID_HANDLE_VALUE || hNullStdout == NULL ) {
			xrtSetError("failed to open subprocess stdout null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, (int)GetLastError());
			goto fail;
		}
		hStdOutput = hNullStdout;
		bUseStdHandles = TRUE;
	} else {
		hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	if ( pPlan->bMergeStderr ) {
		hStdError = hStdOutput;
		bUseStdHandles = TRUE;
	} else if ( pPlan->iStderrMode == XPROC_STDIO_PIPE ) {
		if ( !CreatePipe(&pProcess->hStderrRead, &hChildStderrWrite, &tSa, 0u) ) {
			xrtSetError("failed to create subprocess stderr pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDERR, (int)GetLastError());
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStderrRead, HANDLE_FLAG_INHERIT, 0u);
		hStdError = hChildStderrWrite;
		bUseStdHandles = TRUE;
	} else if ( pPlan->iStderrMode == XPROC_STDIO_NULL ) {
		hNullStderr = __xprocOpenWindowsNullHandle(GENERIC_WRITE);
		if ( hNullStderr == INVALID_HANDLE_VALUE || hNullStderr == NULL ) {
			xrtSetError("failed to open subprocess stderr null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDERR, (int)GetLastError());
			goto fail;
		}
		hStdError = hNullStderr;
		bUseStdHandles = TRUE;
	} else {
		hStdError = GetStdHandle(STD_ERROR_HANDLE);
	}

	if ( bUseStdHandles ) {
		tSi.dwFlags |= STARTF_USESTDHANDLES;
		tSi.hStdInput = hStdInput;
		tSi.hStdOutput = hStdOutput;
		tSi.hStdError = hStdError;
	}
	if ( pPlan->bHideWindow ) {
		tSi.dwFlags |= STARTF_USESHOWWINDOW;
		tSi.wShowWindow = SW_HIDE;
		iCreateFlags |= CREATE_NO_WINDOW;
	}
	if ( pPlan->bCreateProcessGroup ) {
		iCreateFlags |= CREATE_NEW_PROCESS_GROUP;
	}

	sCmdUtf8 = __xprocBuildWindowsCommandLine(pCmd);
	if ( sCmdUtf8 == NULL ) {
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, (int)GetLastError());
		goto fail;
	}
	sCmdW = xrtUTF8to16((u8str)sCmdUtf8, 0u, NULL);
	if ( sCmdW == NULL ) {
		xrtSetError("failed to convert subprocess command line.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, 0);
		goto fail;
	}
	if ( pPlan->sWorkDir ) {
		DWORD iAttr;
		int iWorkDirError = 0;

		sWorkDirW = xrtUTF8to16((u8str)pPlan->sWorkDir, 0u, NULL);
		if ( sWorkDirW == NULL ) {
			xrtSetError("failed to convert subprocess working directory.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_WORKDIR, 0);
			goto fail;
		}
		iAttr = GetFileAttributesW(sWorkDirW);
		if ( iAttr == INVALID_FILE_ATTRIBUTES ) {
			iWorkDirError = (int)GetLastError();
		} else if ( !(iAttr & FILE_ATTRIBUTE_DIRECTORY) ) {
			iWorkDirError = ERROR_DIRECTORY;
		}
		if ( iWorkDirError != 0 ) {
			xrtSetError("invalid subprocess working directory.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_WORKDIR, iWorkDirError);
			goto fail;
		}
	}
	sEnvBlockW = __xprocBuildWindowsEnvBlock(pPlan);
	if ( sEnvBlockW != NULL ) {
		iCreateFlags |= CREATE_UNICODE_ENVIRONMENT;
	}

	bOk = CreateProcessW(NULL, sCmdW, NULL, NULL, bUseStdHandles, iCreateFlags, sEnvBlockW, sWorkDirW, &tSi, &tPi);
	if ( !bOk ) {
		xrtSetError("failed to start subprocess.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, (int)GetLastError());
		goto fail;
	}

	pProcess->hProcess = tPi.hProcess;
	pProcess->iProcessId = tPi.dwProcessId;
	if ( tPi.hThread ) {
		CloseHandle(tPi.hThread);
	}
	if ( hChildStdinRead ) {
		CloseHandle(hChildStdinRead);
		hChildStdinRead = NULL;
	}
	if ( hChildStdoutWrite ) {
		CloseHandle(hChildStdoutWrite);
		hChildStdoutWrite = NULL;
	}
	if ( hChildStderrWrite && hChildStderrWrite != hChildStdoutWrite ) {
		CloseHandle(hChildStderrWrite);
		hChildStderrWrite = NULL;
	}
	if ( hNullStdin ) {
		CloseHandle(hNullStdin);
		hNullStdin = NULL;
	}
	if ( hNullStdout ) {
		CloseHandle(hNullStdout);
		hNullStdout = NULL;
	}
	if ( hNullStderr ) {
		CloseHandle(hNullStderr);
		hNullStderr = NULL;
	}
	if ( sCmdUtf8 ) {
		xrtFree(sCmdUtf8);
	}
	if ( sCmdW ) {
		xrtFree(sCmdW);
	}
	if ( sWorkDirW ) {
		xrtFree(sWorkDirW);
	}
	if ( sEnvBlockW ) {
		xrtFree(sEnvBlockW);
	}
	return true;

fail:
	if ( hChildStdinRead ) CloseHandle(hChildStdinRead);
	if ( hChildStdoutWrite ) CloseHandle(hChildStdoutWrite);
	if ( hChildStderrWrite && hChildStderrWrite != hChildStdoutWrite ) CloseHandle(hChildStderrWrite);
	if ( hNullStdin ) CloseHandle(hNullStdin);
	if ( hNullStdout ) CloseHandle(hNullStdout);
	if ( hNullStderr ) CloseHandle(hNullStderr);
	if ( tPi.hThread ) CloseHandle(tPi.hThread);
	if ( tPi.hProcess ) CloseHandle(tPi.hProcess);
	if ( sCmdUtf8 ) xrtFree(sCmdUtf8);
	if ( sCmdW ) xrtFree(sCmdW);
	if ( sWorkDirW ) xrtFree(sWorkDirW);
	if ( sEnvBlockW ) xrtFree(sEnvBlockW);
	__xprocClosePlatformHandles(pProcess);
	return false;
}

#else

// 内部函数：设置 CLOEXEC
static bool __xprocSetCloseOnExec(int iFd)
{
	int iFlags;

	if ( iFd < 0 ) {
		return false;
	}
	iFlags = fcntl(iFd, F_GETFD);
	if ( iFlags < 0 ) {
		return false;
	}
	return fcntl(iFd, F_SETFD, iFlags | FD_CLOEXEC) == 0;
}


// 内部函数：写入子进程错误
static void __xprocWriteChildError(int iFd, int iStage, int iOsError)
{
	__xproc_child_error tErr;
	ssize_t iRet;

	tErr.iStage = iStage;
	tErr.iOsError = iOsError;
	do {
		iRet = write(iFd, &tErr, sizeof(tErr));
	} while ( iRet < 0 && errno == EINTR );
}


// 内部函数：POSIX terminal 启动
static bool __xprocSpawnPosixTerminal(xprocess* pProcess, const __xproc_plan* pPlan, const __xproc_resolved_cmd* pCmd, xprocessexitinfo* pSpawnInfo)
{
	int fdMaster = -1;
	int fdParentWrite = -1;
	int fdError[2] = { -1, -1 };
	pid_t iPid;
	__xproc_child_error tChildError;
	ssize_t iReadError;
	struct winsize tWinSize;
	char* sSlaveName;

	if ( pipe(fdError) != 0 ) {
		xrtSetError("failed to create subprocess error pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}
	if ( !__xprocSetCloseOnExec(fdError[1]) ) {
		xrtSetError("failed to configure subprocess error pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	fdMaster = posix_openpt(O_RDWR | O_NOCTTY);
	if ( fdMaster < 0 ) {
		xrtSetError("failed to create subprocess terminal.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}
	if ( grantpt(fdMaster) != 0 || unlockpt(fdMaster) != 0 ) {
		xrtSetError("failed to initialize subprocess terminal.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	sSlaveName = ptsname(fdMaster);
	if ( sSlaveName == NULL ) {
		xrtSetError("failed to resolve subprocess terminal path.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	fdParentWrite = dup(fdMaster);
	if ( fdParentWrite < 0 ) {
		xrtSetError("failed to duplicate subprocess terminal handle.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, errno);
		goto fail;
	}

	memset(&tWinSize, 0, sizeof(tWinSize));
	tWinSize.ws_col = (unsigned short)(pPlan->iTerminalCols ? pPlan->iTerminalCols : __XPROC_TERMINAL_COLS_DEFAULT);
	tWinSize.ws_row = (unsigned short)(pPlan->iTerminalRows ? pPlan->iTerminalRows : __XPROC_TERMINAL_ROWS_DEFAULT);
	(void)ioctl(fdMaster, TIOCSWINSZ, &tWinSize);

	iPid = fork();
	if ( iPid < 0 ) {
		xrtSetError("failed to fork subprocess.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	if ( iPid == 0 ) {
		int fdSlave = -1;

		close(fdError[0]);

		if ( setsid() < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_SPAWN, errno);
			_exit(126);
		}

		fdSlave = open(sSlaveName, O_RDWR);
		if ( fdSlave < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_SPAWN, errno);
			_exit(126);
		}
		#if defined(TIOCSCTTY)
			if ( ioctl(fdSlave, TIOCSCTTY, 0) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_SPAWN, errno);
				_exit(126);
			}
		#endif
		if ( ioctl(fdSlave, TIOCSWINSZ, &tWinSize) < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_SPAWN, errno);
			_exit(126);
		}

		if ( dup2(fdSlave, STDIN_FILENO) < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_STDIN, errno);
			_exit(126);
		}
		if ( dup2(fdSlave, STDOUT_FILENO) < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_STDOUT, errno);
			_exit(126);
		}
		if ( dup2(fdSlave, STDERR_FILENO) < 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_STDERR, errno);
			_exit(126);
		}

		if ( fdSlave > STDERR_FILENO ) {
			close(fdSlave);
		}
		close(fdMaster);
		close(fdParentWrite);

		if ( pPlan->sWorkDir && chdir((const char*)pPlan->sWorkDir) != 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_WORKDIR, errno);
			_exit(126);
		}
		if ( !pPlan->bInheritEnv && clearenv() != 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_ENV, errno);
			_exit(126);
		}
		for ( uint32 i = 0u; i < pPlan->iEnvCount; i++ ) {
			if ( putenv((char*)pPlan->arrEnv[i]) != 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_ENV, errno);
				_exit(126);
			}
		}

		execvp(pCmd->arrArgv[0], (char* const*)pCmd->arrArgv);
		__xprocWriteChildError(fdError[1], XPROC_STAGE_EXEC, errno);
		_exit(127);
	}

	close(fdError[1]);
	fdError[1] = -1;

	pProcess->iPid = iPid;
	pProcess->fdStdoutRead = fdMaster;
	fdMaster = -1;
	pProcess->fdStdinWrite = fdParentWrite;
	fdParentWrite = -1;

	memset(&tChildError, 0, sizeof(tChildError));
	do {
		iReadError = read(fdError[0], &tChildError, sizeof(tChildError));
	} while ( iReadError < 0 && errno == EINTR );
	close(fdError[0]);
	fdError[0] = -1;

	if ( iReadError > 0 ) {
		(void)waitpid(iPid, NULL, 0);
		xrtSetError("failed to start subprocess.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, tChildError.iStage, tChildError.iOsError);
		__xprocClosePlatformHandles(pProcess);
		return false;
	}

	return true;

fail:
	if ( fdError[0] >= 0 ) close(fdError[0]);
	if ( fdError[1] >= 0 ) close(fdError[1]);
	if ( fdMaster >= 0 ) close(fdMaster);
	if ( fdParentWrite >= 0 ) close(fdParentWrite);
	__xprocClosePlatformHandles(pProcess);
	return false;
}


// 内部函数：平台启动
static bool __xprocSpawnPlatform(xprocess* pProcess, const __xproc_plan* pPlan, const __xproc_resolved_cmd* pCmd, xprocessexitinfo* pSpawnInfo)
{
	int fdStdin[2] = { -1, -1 };
	int fdStdout[2] = { -1, -1 };
	int fdStderr[2] = { -1, -1 };
	int fdError[2] = { -1, -1 };
	int fdNullStdin = -1;
	int fdNullStdout = -1;
	int fdNullStderr = -1;
	pid_t iPid;
	__xproc_child_error tChildError;
	ssize_t iReadError;

	if ( pPlan->bUseTerminal ) {
		return __xprocSpawnPosixTerminal(pProcess, pPlan, pCmd, pSpawnInfo);
	}

	if ( pipe(fdError) != 0 ) {
		xrtSetError("failed to create subprocess error pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}
	if ( !__xprocSetCloseOnExec(fdError[1]) ) {
		xrtSetError("failed to configure subprocess error pipe.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	if ( pPlan->iStdinMode == XPROC_STDIO_PIPE ) {
		int iOne = 1;
		if ( socketpair(AF_UNIX, SOCK_STREAM, 0, fdStdin) != 0 ) {
			xrtSetError("failed to create subprocess stdin pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, errno);
			goto fail;
		}
		#if defined(SO_NOSIGPIPE)
			(void)setsockopt(fdStdin[1], SOL_SOCKET, SO_NOSIGPIPE, &iOne, sizeof(iOne));
		#endif
	} else if ( pPlan->iStdinMode == XPROC_STDIO_NULL ) {
		fdNullStdin = open("/dev/null", O_RDONLY);
		if ( fdNullStdin < 0 ) {
			xrtSetError("failed to open subprocess stdin null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDIN, errno);
			goto fail;
		}
	}

	if ( pPlan->iStdoutMode == XPROC_STDIO_PIPE ) {
		if ( pipe(fdStdout) != 0 ) {
			xrtSetError("failed to create subprocess stdout pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, errno);
			goto fail;
		}
	} else if ( pPlan->iStdoutMode == XPROC_STDIO_NULL ) {
		fdNullStdout = open("/dev/null", O_WRONLY);
		if ( fdNullStdout < 0 ) {
			xrtSetError("failed to open subprocess stdout null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, errno);
			goto fail;
		}
	}

	if ( !pPlan->bMergeStderr && pPlan->iStderrMode == XPROC_STDIO_PIPE ) {
		if ( pipe(fdStderr) != 0 ) {
			xrtSetError("failed to create subprocess stderr pipe.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDERR, errno);
			goto fail;
		}
	} else if ( !pPlan->bMergeStderr && pPlan->iStderrMode == XPROC_STDIO_NULL ) {
		fdNullStderr = open("/dev/null", O_WRONLY);
		if ( fdNullStderr < 0 ) {
			xrtSetError("failed to open subprocess stderr null handle.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDERR, errno);
			goto fail;
		}
	}

	iPid = fork();
	if ( iPid < 0 ) {
		xrtSetError("failed to fork subprocess.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, errno);
		goto fail;
	}

	if ( iPid == 0 ) {
		close(fdError[0]);

		if ( pPlan->bCreateProcessGroup && setpgid(0, 0) != 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_SPAWN, errno);
			_exit(126);
		}

		if ( pPlan->iStdinMode == XPROC_STDIO_PIPE ) {
			if ( dup2(fdStdin[0], STDIN_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDIN, errno);
				_exit(126);
			}
		} else if ( pPlan->iStdinMode == XPROC_STDIO_NULL ) {
			if ( dup2(fdNullStdin, STDIN_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDIN, errno);
				_exit(126);
			}
		}

		if ( pPlan->iStdoutMode == XPROC_STDIO_PIPE ) {
			if ( dup2(fdStdout[1], STDOUT_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDOUT, errno);
				_exit(126);
			}
		} else if ( pPlan->iStdoutMode == XPROC_STDIO_NULL ) {
			if ( dup2(fdNullStdout, STDOUT_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDOUT, errno);
				_exit(126);
			}
		}

		if ( pPlan->bMergeStderr ) {
			if ( dup2(STDOUT_FILENO, STDERR_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDERR, errno);
				_exit(126);
			}
		} else if ( pPlan->iStderrMode == XPROC_STDIO_PIPE ) {
			if ( dup2(fdStderr[1], STDERR_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDERR, errno);
				_exit(126);
			}
		} else if ( pPlan->iStderrMode == XPROC_STDIO_NULL ) {
			if ( dup2(fdNullStderr, STDERR_FILENO) < 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_STDERR, errno);
				_exit(126);
			}
		}

		if ( fdStdin[0] >= 0 ) close(fdStdin[0]);
		if ( fdStdin[1] >= 0 ) close(fdStdin[1]);
		if ( fdStdout[0] >= 0 ) close(fdStdout[0]);
		if ( fdStdout[1] >= 0 ) close(fdStdout[1]);
		if ( fdStderr[0] >= 0 ) close(fdStderr[0]);
		if ( fdStderr[1] >= 0 ) close(fdStderr[1]);
		if ( fdNullStdin >= 0 ) close(fdNullStdin);
		if ( fdNullStdout >= 0 ) close(fdNullStdout);
		if ( fdNullStderr >= 0 ) close(fdNullStderr);

		if ( pPlan->sWorkDir && chdir((const char*)pPlan->sWorkDir) != 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_WORKDIR, errno);
			_exit(126);
		}
		if ( !pPlan->bInheritEnv && clearenv() != 0 ) {
			__xprocWriteChildError(fdError[1], XPROC_STAGE_ENV, errno);
			_exit(126);
		}
		for ( uint32 i = 0u; i < pPlan->iEnvCount; i++ ) {
			if ( putenv((char*)pPlan->arrEnv[i]) != 0 ) {
				__xprocWriteChildError(fdError[1], XPROC_STAGE_ENV, errno);
				_exit(126);
			}
		}

		execvp(pCmd->arrArgv[0], (char* const*)pCmd->arrArgv);
		__xprocWriteChildError(fdError[1], XPROC_STAGE_EXEC, errno);
		_exit(127);
	}

	close(fdError[1]);
	fdError[1] = -1;

	if ( fdStdin[0] >= 0 ) {
		close(fdStdin[0]);
		fdStdin[0] = -1;
		pProcess->fdStdinWrite = fdStdin[1];
		fdStdin[1] = -1;
	}
	if ( fdStdout[1] >= 0 ) {
		close(fdStdout[1]);
		fdStdout[1] = -1;
		pProcess->fdStdoutRead = fdStdout[0];
		fdStdout[0] = -1;
	}
	if ( fdStderr[1] >= 0 ) {
		close(fdStderr[1]);
		fdStderr[1] = -1;
		pProcess->fdStderrRead = fdStderr[0];
		fdStderr[0] = -1;
	}
	if ( fdNullStdin >= 0 ) {
		close(fdNullStdin);
		fdNullStdin = -1;
	}
	if ( fdNullStdout >= 0 ) {
		close(fdNullStdout);
		fdNullStdout = -1;
	}
	if ( fdNullStderr >= 0 ) {
		close(fdNullStderr);
		fdNullStderr = -1;
	}

	memset(&tChildError, 0, sizeof(tChildError));
	do {
		iReadError = read(fdError[0], &tChildError, sizeof(tChildError));
	} while ( iReadError < 0 && errno == EINTR );
	close(fdError[0]);
	fdError[0] = -1;

	if ( iReadError > 0 ) {
		(void)waitpid(iPid, NULL, 0);
		xrtSetError("failed to start subprocess.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, tChildError.iStage, tChildError.iOsError);
		__xprocClosePlatformHandles(pProcess);
		return false;
	}

	pProcess->iPid = iPid;
	return true;

fail:
	if ( fdError[0] >= 0 ) close(fdError[0]);
	if ( fdError[1] >= 0 ) close(fdError[1]);
	if ( fdStdin[0] >= 0 ) close(fdStdin[0]);
	if ( fdStdin[1] >= 0 ) close(fdStdin[1]);
	if ( fdStdout[0] >= 0 ) close(fdStdout[0]);
	if ( fdStdout[1] >= 0 ) close(fdStdout[1]);
	if ( fdStderr[0] >= 0 ) close(fdStderr[0]);
	if ( fdStderr[1] >= 0 ) close(fdStderr[1]);
	if ( fdNullStdin >= 0 ) close(fdNullStdin);
	if ( fdNullStdout >= 0 ) close(fdNullStdout);
	if ( fdNullStderr >= 0 ) close(fdNullStderr);
	__xprocClosePlatformHandles(pProcess);
	return false;
}

#endif


// 内部函数：记录停止请求
static void __xprocRecordStopRequest(xprocess* pProcess, int iStopReason, bool bTimedOut, bool bCancelled)
{
	if ( pProcess == NULL ) {
		return;
	}
	xrtMutexLock(&pProcess->Lock);
	if ( iStopReason != XPROC_STOP_NONE ) {
		pProcess->iRequestedStop = iStopReason;
	}
	if ( bTimedOut ) {
		pProcess->bStopTimedOut = true;
	}
	if ( bCancelled ) {
		pProcess->bStopCancelled = true;
	}
	xrtMutexUnlock(&pProcess->Lock);
}


// 内部函数：平台中断
static bool __xprocInterruptPlatform(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess == NULL || pProcess->iProcessId == 0u ) {
			return true;
		}
		if ( !pProcess->bCreateProcessGroup ) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return false;
		}
		(void)SetConsoleCtrlHandler(NULL, TRUE);
		if ( GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pProcess->iProcessId) ) {
			(void)SetConsoleCtrlHandler(NULL, FALSE);
			return true;
		}
		(void)SetConsoleCtrlHandler(NULL, FALSE);
		return false;
	#else
		if ( pProcess->iPid <= 0 ) {
			return true;
		}
		if ( pProcess->bCreateProcessGroup ) {
			return kill(-pProcess->iPid, SIGINT) == 0 || errno == ESRCH;
		}
		return kill(pProcess->iPid, SIGINT) == 0 || errno == ESRCH;
	#endif
}


// 内部函数：平台温和终止
static bool __xprocTerminatePlatform(xprocess* pProcess)
{
	bool bRequested = false;

	if ( pProcess == NULL ) {
		return false;
	}
	__xprocCloseStdinHandle(pProcess);
	bRequested = true;

	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess == NULL ) {
			return true;
		}
		if ( pProcess->bCreateProcessGroup && pProcess->iProcessId != 0u ) {
			(void)SetConsoleCtrlHandler(NULL, TRUE);
			if ( GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pProcess->iProcessId) ) {
				(void)SetConsoleCtrlHandler(NULL, FALSE);
				return true;
			}
			(void)SetConsoleCtrlHandler(NULL, FALSE);
		}
		return bRequested;
	#else
		if ( pProcess->iPid <= 0 ) {
			return true;
		}
		if ( kill(pProcess->iPid, SIGTERM) == 0 || errno == ESRCH ) {
			return true;
		}
		return bRequested;
	#endif
}


// 内部函数：平台强制终止
static bool __xprocKillPlatform(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess == NULL ) {
			return true;
		}
		if ( TerminateProcess(pProcess->hProcess, 1u) ) {
			return true;
		}
		return GetLastError() == ERROR_ACCESS_DENIED && pProcess->bExitReady;
	#else
		if ( pProcess->iPid <= 0 ) {
			return true;
		}
		return kill(pProcess->iPid, SIGKILL) == 0 || errno == ESRCH;
	#endif
}


// 内部函数：平台强制终止树
static bool __xprocKillTreePlatform(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess == NULL ) {
			return true;
		}
		if ( __xprocEnsureJobObject(pProcess) && TerminateJobObject(pProcess->hJob, 1u) ) {
			return true;
		}
		return __xprocKillPlatform(pProcess);
	#else
		if ( pProcess->iPid <= 0 ) {
			return true;
		}
		if ( pProcess->bCreateProcessGroup ) {
			return kill(-pProcess->iPid, SIGKILL) == 0 || errno == ESRCH;
		}
		return kill(pProcess->iPid, SIGKILL) == 0 || errno == ESRCH;
	#endif
}


// 内部函数：调整 terminal 尺寸
static bool __xprocResizeTerminalPlatform(xprocess* pProcess, uint32 iCols, uint32 iRows)
{
	if ( pProcess == NULL || !pProcess->bUseTerminal ) {
		return false;
	}

	#if defined(_WIN32) || defined(_WIN64)
		return __xprocResizeTerminalPlatformHandle(pProcess->hTerminal, iCols, iRows);
	#else
		{
			struct winsize tWinSize;
			int iFd = pProcess->fdStdoutRead >= 0 ? pProcess->fdStdoutRead : pProcess->fdStdinWrite;

			if ( iFd < 0 ) {
				errno = ENODEV;
				return false;
			}

			memset(&tWinSize, 0, sizeof(tWinSize));
			tWinSize.ws_col = (unsigned short)(iCols ? iCols : __XPROC_TERMINAL_COLS_DEFAULT);
			tWinSize.ws_row = (unsigned short)(iRows ? iRows : __XPROC_TERMINAL_ROWS_DEFAULT);
			if ( ioctl(iFd, TIOCSWINSZ, &tWinSize) != 0 ) {
				return false;
			}
			if ( pProcess->iPid > 0 ) {
				if ( pProcess->bCreateProcessGroup ) {
					(void)kill(-pProcess->iPid, SIGWINCH);
				} else {
					(void)kill(pProcess->iPid, SIGWINCH);
				}
			}
			return true;
		}
	#endif
}


// 内部函数：统一停止请求
static bool __xprocRequestStop(xprocess* pProcess, int iStopReason, bool bTimedOut, bool bCancelled)
{
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return false;
	}
	if ( pProcess->bExitReady ) {
		return true;
	}

	__xprocRecordStopRequest(pProcess, iStopReason, bTimedOut, bCancelled);

	switch ( iStopReason ) {
		case XPROC_STOP_INTERRUPT:
			return __xprocInterruptPlatform(pProcess);
		case XPROC_STOP_TERMINATE:
			return __xprocTerminatePlatform(pProcess);
		case XPROC_STOP_KILL:
			return __xprocKillPlatform(pProcess);
		case XPROC_STOP_KILL_TREE:
			return __xprocKillTreePlatform(pProcess);
		default:
			return false;
	}
}


// 内部函数：等待线程
static uint32 __xprocWaitThread(ptr pArg)
{
	xprocess* pProcess = (xprocess*)pArg;
	xprocessexitinfo tExitInfo;
	int iState = XPROC_STATE_EXITED;
	#if !defined(XRT_NO_NETWORK)
		xpromise* pPromise = NULL;
	#endif

	if ( pProcess == NULL ) {
		return 1u;
	}

	__xprocExitInfoInit(&tExitInfo);

	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iWaitRet = WaitForSingleObject(pProcess->hProcess, INFINITE);
			if ( iWaitRet == WAIT_OBJECT_0 ) {
				DWORD iWinExit = 0u;
				if ( GetExitCodeProcess(pProcess->hProcess, &iWinExit) ) {
					tExitInfo.iKind = XPROC_EXIT_NORMAL;
					tExitInfo.iExitCode = (int)iWinExit;
				} else {
					tExitInfo.iKind = XPROC_EXIT_WAIT_FAILED;
					tExitInfo.iStage = XPROC_STAGE_WAIT;
					tExitInfo.iOsError = (int)GetLastError();
					iState = XPROC_STATE_FAILED;
				}
			} else {
				tExitInfo.iKind = XPROC_EXIT_WAIT_FAILED;
				tExitInfo.iStage = XPROC_STAGE_WAIT;
				tExitInfo.iOsError = (int)GetLastError();
				iState = XPROC_STATE_FAILED;
			}
		}
	#else
		{
			int iStatus = 0;
			pid_t iWaitRet;
			do {
				iWaitRet = waitpid(pProcess->iPid, &iStatus, 0);
			} while ( iWaitRet < 0 && errno == EINTR );
			if ( iWaitRet < 0 ) {
				tExitInfo.iKind = XPROC_EXIT_WAIT_FAILED;
				tExitInfo.iStage = XPROC_STAGE_WAIT;
				tExitInfo.iOsError = errno;
				iState = XPROC_STATE_FAILED;
			} else if ( WIFEXITED(iStatus) ) {
				tExitInfo.iKind = XPROC_EXIT_NORMAL;
				tExitInfo.iExitCode = WEXITSTATUS(iStatus);
			} else if ( WIFSIGNALED(iStatus) ) {
				tExitInfo.iKind = XPROC_EXIT_SIGNAL;
				tExitInfo.iSignal = WTERMSIG(iStatus);
				tExitInfo.iExitCode = 128 + tExitInfo.iSignal;
			} else {
				tExitInfo.iKind = XPROC_EXIT_WAIT_FAILED;
				tExitInfo.iStage = XPROC_STAGE_WAIT;
				iState = XPROC_STATE_FAILED;
			}
		}
	#endif

	__xprocCloseStdinHandle(pProcess);
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hTerminal ) {
			__xprocCloseTerminalHandlePlatform(pProcess->hTerminal);
			pProcess->hTerminal = NULL;
		}
	#endif
	if ( pProcess->hStdoutThread ) {
		xrtThreadWait(pProcess->hStdoutThread);
	}
	if ( pProcess->hStderrThread ) {
		xrtThreadWait(pProcess->hStderrThread);
	}

	xrtMutexLock(&pProcess->Lock);
	tExitInfo.iStopReason = pProcess->iRequestedStop;
	tExitInfo.bTimedOut = pProcess->bStopTimedOut ? true : false;
	tExitInfo.bCancelled = pProcess->bStopCancelled ? true : false;
	pProcess->ExitInfo = tExitInfo;
	pProcess->EventBuf.bDone = true;
	__xprocPushEventLocked(pProcess, XPROC_EVENT_EXIT, XPROC_STREAM_NONE, 0u, 0u, &tExitInfo);
	pProcess->iState = iState;
	pProcess->bExitReady = true;
	xrtCondBroadcast(&pProcess->Cond);
	#if !defined(XRT_NO_NETWORK)
		pPromise = pProcess->pWaitPromise;
		pProcess->pWaitPromise = NULL;
	#endif
	xrtMutexUnlock(&pProcess->Lock);

	#if !defined(XRT_NO_NETWORK)
		if ( pPromise ) {
			(void)xPromiseResolve(pPromise, pProcess);
			xPromiseDestroy(pPromise);
		}
	#endif
	if ( pProcess->Events.OnExit ) {
		pProcess->Events.OnExit(pProcess, &pProcess->ExitInfo, pProcess->pUserData);
	}
	return 0u;
}


// 内部函数：启动失败清理
static void __xprocCleanupSpawnFailure(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}

	(void)__xprocKillTreePlatform(pProcess);
	(void)__xprocKillPlatform(pProcess);

	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess ) {
			(void)WaitForSingleObject(pProcess->hProcess, 1000u);
		}
	#else
		if ( pProcess->iPid > 0 ) {
			(void)waitpid(pProcess->iPid, NULL, 0);
		}
	#endif

	if ( pProcess->hStdoutThread ) {
		xrtThreadWait(pProcess->hStdoutThread);
	}
	if ( pProcess->hStderrThread ) {
		xrtThreadWait(pProcess->hStderrThread);
	}
	if ( pProcess->hWaitThread ) {
		xrtThreadWait(pProcess->hWaitThread);
	}
	__xprocDestroyThreads(pProcess);
	__xprocClosePlatformHandles(pProcess);
	#if !defined(XRT_NO_NETWORK)
		__xprocDetachWaitFuture(pProcess);
	#endif
	__xprocReleaseProcess(pProcess);
}


// 内部函数：内部启动
static xprocess* __xprocSpawnInternal(const xprocessconfig* pConfig, xprocessexitinfo* pSpawnInfo)
{
	__xproc_plan tPlan;
	__xproc_resolved_cmd tCmd;
	xprocess* pProcess = NULL;

	__xprocExitInfoInit(pSpawnInfo);
	if ( !__xprocPlanFromConfig(pConfig, &tPlan) ) {
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, 0);
		return NULL;
	}
	if ( !__xprocResolveCommand(&tPlan, &tCmd) ) {
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_EXEC, 0);
		return NULL;
	}

	pProcess = __xprocAllocProcess(&tPlan);
	if ( pProcess == NULL ) {
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_SPAWN, 0);
		__xprocResolvedCmdUnit(&tCmd);
		return NULL;
	}
	if ( !__xprocSpawnPlatform(pProcess, &tPlan, &tCmd, pSpawnInfo) ) {
		__xprocResolvedCmdUnit(&tCmd);
		__xprocReleaseProcess(pProcess);
		return NULL;
	}
	__xprocResolvedCmdUnit(&tCmd);

	if ( pProcess->iStdoutMode != XPROC_STDIO_PIPE ) {
		pProcess->bStdoutDone = true;
		pProcess->StdoutBuf.bDone = true;
	}
	if ( pProcess->iStderrMode != XPROC_STDIO_PIPE || pProcess->bMergeStderr ) {
		pProcess->bStderrDone = true;
		pProcess->StderrBuf.bDone = true;
	}

	pProcess->StdoutPump.pProcess = pProcess;
	pProcess->StdoutPump.iStream = pProcess->bUseTerminal ? __XPROC_STREAM_TERMINAL : __XPROC_STREAM_STDOUT;
	pProcess->StderrPump.pProcess = pProcess;
	pProcess->StderrPump.iStream = __XPROC_STREAM_STDERR;

	if ( pProcess->iStdoutMode == XPROC_STDIO_PIPE ) {
		pProcess->hStdoutThread = xrtThreadCreate(__xprocPumpThread, &pProcess->StdoutPump, 0u);
		if ( pProcess->hStdoutThread == NULL ) {
			xrtSetError("failed to create subprocess stdout thread.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDOUT, 0);
			__xprocCleanupSpawnFailure(pProcess);
			return NULL;
		}
	}
	if ( pProcess->iStderrMode == XPROC_STDIO_PIPE && !pProcess->bMergeStderr ) {
		pProcess->hStderrThread = xrtThreadCreate(__xprocPumpThread, &pProcess->StderrPump, 0u);
		if ( pProcess->hStderrThread == NULL ) {
			xrtSetError("failed to create subprocess stderr thread.", FALSE);
			__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_STDERR, 0);
			__xprocCleanupSpawnFailure(pProcess);
			return NULL;
		}
	}

	pProcess->iState = XPROC_STATE_RUNNING;
	pProcess->hWaitThread = xrtThreadCreate(__xprocWaitThread, pProcess, 0u);
	if ( pProcess->hWaitThread == NULL ) {
		xrtSetError("failed to create subprocess wait thread.", FALSE);
		__xprocSetSpawnFailureInfo(pSpawnInfo, XPROC_STAGE_WAIT, 0);
		__xprocCleanupSpawnFailure(pProcess);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	__xprocPushEventLocked(pProcess, XPROC_EVENT_START, XPROC_STREAM_NONE, 0u, 0u, NULL);
	xrtMutexUnlock(&pProcess->Lock);
	if ( pProcess->Events.OnStart ) {
		pProcess->Events.OnStart(pProcess, pProcess->pUserData);
	}
	return pProcess;
}


// xrtProcessSpawn 相关处理
XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
{
	return __xprocSpawnInternal(pConfig, NULL);
}


// 销毁进程
XXAPI void xrtProcessDestroy(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return;
	}
	if ( pProcess->iState == XPROC_STATE_RUNNING && !pProcess->bExitReady ) {
		xrtSetError("subprocess is still running.", FALSE);
		return;
	}
	if ( pProcess->hWaitThread ) {
		xrtThreadWait(pProcess->hWaitThread);
	}
	if ( pProcess->hStdoutThread ) {
		xrtThreadWait(pProcess->hStdoutThread);
	}
	if ( pProcess->hStderrThread ) {
		xrtThreadWait(pProcess->hStderrThread);
	}
	pProcess->iState = XPROC_STATE_CLOSED;
	__xprocDestroyThreads(pProcess);
	__xprocClosePlatformHandles(pProcess);
	#if !defined(XRT_NO_NETWORK)
		__xprocDetachWaitFuture(pProcess);
	#endif
	__xprocReleaseProcess(pProcess);
}


// 获取进程状态
XXAPI int xrtProcessState(xprocess* pProcess)
{
	return pProcess ? pProcess->iState : XPROC_STATE_FAILED;
}


// 判断是否仍在运行
XXAPI bool xrtProcessIsRunning(xprocess* pProcess)
{
	return pProcess ? (pProcess->iState == XPROC_STATE_RUNNING && !pProcess->bExitReady) : false;
}


// 获取退出码
XXAPI int xrtProcessExitCode(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		return -1;
	}
	return pProcess->ExitInfo.iExitCode;
}


// 获取结构化退出信息
XXAPI bool xrtProcessGetExitInfo(xprocess* pProcess, xprocessexitinfo* pInfo)
{
	if ( pInfo ) {
		__xprocExitInfoInit(pInfo);
	}
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return false;
	}
	if ( pInfo == NULL ) {
		xrtSetError("invalid subprocess exit info output.", FALSE);
		return false;
	}

	xrtMutexLock(&pProcess->Lock);
	*pInfo = pProcess->ExitInfo;
	if ( pProcess->bExitReady ) {
		pInfo->iStopReason = pProcess->iRequestedStop;
		pInfo->bTimedOut = pProcess->bStopTimedOut ? true : false;
		pInfo->bCancelled = pProcess->bStopCancelled ? true : false;
	}
	xrtMutexUnlock(&pProcess->Lock);
	return true;
}


// 当前平台是否支持 terminal 模式
XXAPI bool xrtProcessTerminalSupported(void)
{
	return __xprocTerminalSupportedPlatform();
}


// 写入进程
XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize)
{
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return -1;
	}
	if ( pData == NULL || iSize == 0u ) {
		return 0;
	}

	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iWritten = 0u;

			if ( pProcess->hStdinWrite == NULL ) {
				xrtSetError("subprocess stdin pipe is not available.", FALSE);
				return -1;
			}
			if ( !WriteFile(pProcess->hStdinWrite, pData, (DWORD)iSize, &iWritten, NULL) ) {
				xrtSetError("failed to write subprocess stdin.", FALSE);
				return -1;
			}
			return (int64)iWritten;
		}
	#else
		{
			const char* sCursor = (const char*)pData;
			size_t iLeft = iSize;

			if ( pProcess->fdStdinWrite < 0 ) {
				xrtSetError("subprocess stdin pipe is not available.", FALSE);
				return -1;
			}
			while ( iLeft > 0u ) {
				ssize_t iWritten;
				#if defined(MSG_NOSIGNAL)
					iWritten = send(pProcess->fdStdinWrite, sCursor, iLeft, MSG_NOSIGNAL);
				#else
					iWritten = send(pProcess->fdStdinWrite, sCursor, iLeft, 0);
				#endif
				if ( iWritten < 0 && errno == EINTR ) {
					continue;
				}
				if ( iWritten <= 0 ) {
					xrtSetError("failed to write subprocess stdin.", FALSE);
					return -1;
				}
				sCursor += iWritten;
				iLeft -= (size_t)iWritten;
			}
			return (int64)iSize;
		}
	#endif
}


// 写入进程文本
XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize)
{
	if ( sText == NULL ) {
		return 0;
	}
	if ( iSize == 0u ) {
		iSize = strlen((const char*)sText);
	}
	return xrtProcessWrite(pProcess, sText, iSize);
}


// 关闭 stdin
XXAPI bool xrtProcessCloseStdin(xprocess* pProcess)
{
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return false;
	}
	__xprocCloseStdinHandle(pProcess);
	return true;
}


// 等待进程
XXAPI bool xrtProcessWait(xprocess* pProcess)
{
	return xrtProcessWaitTimeout(pProcess, UINT32_MAX) == XRT_WAIT_OK;
}


// 等待进程超时
XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs)
{
	uint64 iDeadline = 0u;

	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return XRT_WAIT_ERROR;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( iTimeoutMs != UINT32_MAX ) {
		iDeadline = __xprocNowMs() + iTimeoutMs;
	}
	while ( !pProcess->bExitReady ) {
		if ( iTimeoutMs == UINT32_MAX ) {
			xrtCondWait(&pProcess->Cond, &pProcess->Lock);
		} else {
			uint64 iNow = __xprocNowMs();
			uint64 iRemain;
			int iRet;

			if ( iNow >= iDeadline ) {
				xrtMutexUnlock(&pProcess->Lock);
				return XRT_WAIT_TIMEOUT;
			}
			iRemain = iDeadline - iNow;
			iRet = xrtCondWaitTimeout(&pProcess->Cond, &pProcess->Lock, (iRemain > UINT32_MAX) ? UINT32_MAX : (uint32)iRemain);
			if ( iRet == XRT_WAIT_TIMEOUT && !pProcess->bExitReady ) {
				xrtMutexUnlock(&pProcess->Lock);
				return XRT_WAIT_TIMEOUT;
			}
			if ( iRet == XRT_WAIT_ERROR ) {
				xrtMutexUnlock(&pProcess->Lock);
				return XRT_WAIT_ERROR;
			}
		}
	}
	xrtMutexUnlock(&pProcess->Lock);
	if ( pProcess->hWaitThread ) {
		xrtThreadWait(pProcess->hWaitThread);
	}
	return XRT_WAIT_OK;
}


// 中断进程
XXAPI bool xrtProcessInterrupt(xprocess* pProcess)
{
	if ( !__xprocRequestStop(pProcess, XPROC_STOP_INTERRUPT, false, true) ) {
		xrtSetError("failed to interrupt subprocess.", FALSE);
		return false;
	}
	return true;
}


// 温和终止进程
XXAPI bool xrtProcessTerminate(xprocess* pProcess)
{
	if ( !__xprocRequestStop(pProcess, XPROC_STOP_TERMINATE, false, true) ) {
		xrtSetError("failed to terminate subprocess.", FALSE);
		return false;
	}
	return true;
}


// 强制终止进程
XXAPI bool xrtProcessKill(xprocess* pProcess)
{
	if ( !__xprocRequestStop(pProcess, XPROC_STOP_KILL, false, true) ) {
		xrtSetError("failed to kill subprocess.", FALSE);
		return false;
	}
	return true;
}


// 强制终止进程树
XXAPI bool xrtProcessKillTree(xprocess* pProcess)
{
	if ( !__xprocRequestStop(pProcess, XPROC_STOP_KILL_TREE, false, true) ) {
		xrtSetError("failed to kill subprocess tree.", FALSE);
		return false;
	}
	return true;
}


// 调整 terminal 尺寸
XXAPI bool xrtProcessResizeTerminal(xprocess* pProcess, uint32 iCols, uint32 iRows)
{
	bool bOk;

	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return false;
	}
	if ( !pProcess->bUseTerminal ) {
		xrtSetError("subprocess terminal is not enabled.", FALSE);
		return false;
	}

	bOk = __xprocResizeTerminalPlatform(pProcess, iCols, iRows);
	if ( !bOk ) {
		xrtSetError("failed to resize subprocess terminal.", FALSE);
		return false;
	}

	xrtMutexLock(&pProcess->Lock);
	pProcess->iTerminalCols = iCols ? iCols : __XPROC_TERMINAL_COLS_DEFAULT;
	pProcess->iTerminalRows = iRows ? iRows : __XPROC_TERMINAL_ROWS_DEFAULT;
	xrtMutexUnlock(&pProcess->Lock);
	return true;
}


// 获取标准输出快照
XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize)
{
	ptr pData;

	if ( piSize ) {
		*piSize = 0u;
	}
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	pData = __xprocStreamSnapshot(&pProcess->StdoutBuf, piSize);
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}


// 获取标准错误快照
XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize)
{
	ptr pData;

	if ( piSize ) {
		*piSize = 0u;
	}
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	pData = __xprocStreamSnapshot(&pProcess->StderrBuf, piSize);
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}


// 按偏移读取 stdout
XXAPI ptr xrtProcessReadStdoutSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo)
{
	ptr pData;

	if ( piSize ) {
		*piSize = 0u;
	}
	__xprocReadInfoInit(pInfo);
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	pData = __xprocStreamReadSince(&pProcess->StdoutBuf, iOffset, iMaxBytes, piSize, pInfo);
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}


// 按偏移读取 stderr
XXAPI ptr xrtProcessReadStderrSince(xprocess* pProcess, uint64 iOffset, size_t iMaxBytes, size_t* piSize, xprocessreadinfo* pInfo)
{
	ptr pData;

	if ( piSize ) {
		*piSize = 0u;
	}
	__xprocReadInfoInit(pInfo);
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	pData = __xprocStreamReadSince(&pProcess->StderrBuf, iOffset, iMaxBytes, piSize, pInfo);
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}


// 按序号读取事件
XXAPI xprocessevent* xrtProcessReadEventsSince(xprocess* pProcess, uint64 iSeq, uint32 iMaxCount, uint32* piCount, xprocesseventreadinfo* pInfo)
{
	xprocessevent* arrEvents;

	if ( piCount ) {
		*piCount = 0u;
	}
	__xprocEventReadInfoInit(pInfo);
	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	arrEvents = __xprocEventBufReadSince(&pProcess->EventBuf, iSeq, iMaxCount, piCount, pInfo);
	xrtMutexUnlock(&pProcess->Lock);
	return arrEvents;
}


// 内部函数：timeout 收口策略
static void __xprocStopForTimeout(xprocess* pProcess)
{
	(void)__xprocRequestStop(pProcess, XPROC_STOP_INTERRUPT, true, false);
	if ( xrtProcessWaitTimeout(pProcess, __XPROC_TIMEOUT_GRACE_MS) == XRT_WAIT_OK ) {
		return;
	}
	(void)__xprocRequestStop(pProcess, XPROC_STOP_TERMINATE, true, false);
	if ( xrtProcessWaitTimeout(pProcess, __XPROC_TIMEOUT_GRACE_MS) == XRT_WAIT_OK ) {
		return;
	}
	(void)__xprocRequestStop(pProcess, XPROC_STOP_KILL_TREE, true, false);
	(void)__xprocRequestStop(pProcess, XPROC_STOP_KILL, true, false);
	(void)xrtProcessWait(pProcess);
}


// xrtExecCapture 相关处理
XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs)
{
	xprocessconfig tConfig;
	xprocess* pProcess;
	xprocessexitinfo tSpawnInfo;
	int iWaitRet;

	if ( pConfig == NULL || pResult == NULL ) {
		xrtSetError("invalid subprocess capture arguments.", FALSE);
		return false;
	}

	__xprocResultInit(pResult);
	tConfig = *pConfig;
	tConfig.Stdout.iMode = XPROC_STDIO_PIPE;
	tConfig.Stdout.bCapture = true;
	if ( tConfig.bMergeStderr ) {
		tConfig.Stderr.bCapture = false;
	} else {
		tConfig.Stderr.iMode = XPROC_STDIO_PIPE;
		tConfig.Stderr.bCapture = true;
	}

	pProcess = __xprocSpawnInternal(&tConfig, &tSpawnInfo);
	if ( pProcess == NULL ) {
		pResult->ExitInfo = tSpawnInfo;
		pResult->iExitCode = tSpawnInfo.iExitCode;
		return false;
	}

	iWaitRet = xrtProcessWaitTimeout(pProcess, iTimeoutMs == 0u ? UINT32_MAX : iTimeoutMs);
	if ( iWaitRet == XRT_WAIT_TIMEOUT ) {
		__xprocStopForTimeout(pProcess);
	} else if ( iWaitRet == XRT_WAIT_ERROR ) {
		(void)xrtProcessWait(pProcess);
	}

	(void)xrtProcessGetExitInfo(pProcess, &pResult->ExitInfo);
	pResult->iExitCode = pResult->ExitInfo.iExitCode;

	xrtMutexLock(&pProcess->Lock);
	__xprocStreamMoveOut(&pProcess->StdoutBuf, &pResult->pStdout, &pResult->iStdoutSize, &pResult->iStdoutBaseOffset, &pResult->bStdoutTruncated);
	__xprocStreamMoveOut(&pProcess->StderrBuf, &pResult->pStderr, &pResult->iStderrSize, &pResult->iStderrBaseOffset, &pResult->bStderrTruncated);
	xrtMutexUnlock(&pProcess->Lock);

	xrtProcessDestroy(pProcess);
	return true;
}


#if !defined(XRT_NO_NETWORK)
// 等待进程 Future
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	bool bResolveNow = false;

	if ( pProcess == NULL ) {
		xrtSetError("invalid subprocess handle.", FALSE);
		return NULL;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( pProcess->pWaitFuture == NULL ) {
		pFuture = xFutureCreate();
		if ( pFuture == NULL ) {
			xrtMutexUnlock(&pProcess->Lock);
			xrtSetError("failed to create subprocess wait future.", FALSE);
			return NULL;
		}
		pPromise = xPromiseCreate(pFuture);
		if ( pPromise == NULL ) {
			xrtMutexUnlock(&pProcess->Lock);
			xFutureRelease(pFuture);
			xrtSetError("failed to create subprocess wait promise.", FALSE);
			return NULL;
		}
		pFuture->pPendingCtx = __xprocAddRef(pProcess);
		pFuture->pfnPendingCleanup = __xprocWaitFutureCleanup;
		pProcess->pWaitFuture = pFuture;
		if ( pProcess->bExitReady ) {
			bResolveNow = true;
		} else {
			pProcess->pWaitPromise = pPromise;
			pPromise = NULL;
		}
	}
	pFuture = xFutureAddRef(pProcess->pWaitFuture);
	xrtMutexUnlock(&pProcess->Lock);

	if ( bResolveNow && pPromise ) {
		(void)xPromiseResolve(pPromise, pProcess);
		xPromiseDestroy(pPromise);
	}
	return pFuture;
}
#endif

#endif
