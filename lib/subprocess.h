#if !defined(_WIN32) && !defined(_WIN64)
	#include <errno.h>
	#include <signal.h>
	#include <sys/socket.h>
#endif

#define __XPROC_READ_CHUNK_DEFAULT	4096u

#define __XPROC_STREAM_STDOUT		1
#define __XPROC_STREAM_STDERR		2

typedef struct {
	char* pData;
	size_t iSize;
	size_t iCap;
	bool bTruncated;
} __xproc_buffer;

typedef struct {
	char* sData;
	size_t iSize;
	size_t iCap;
} __xproc_strbuf;

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

static bool __xprocStrBufAppendRaw(__xproc_strbuf* pBuf, const char* sText, size_t iLen)
{
	if ( pBuf == NULL ) {
		return false;
	}
	if ( iLen == 0 ) {
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

static bool __xprocStrBufAppend(__xproc_strbuf* pBuf, const char* sText)
{
	if ( sText == NULL ) {
		return true;
	}
	return __xprocStrBufAppendRaw(pBuf, sText, strlen(sText));
}

static bool __xprocStrBufAppendChar(__xproc_strbuf* pBuf, char ch)
{
	return __xprocStrBufAppendRaw(pBuf, &ch, 1u);
}

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

static bool __xprocCmdNeedsQuote(str sArg)
{
	size_t i;

	if ( sArg == NULL || sArg[0] == '\0' ) {
		return true;
	}

	for ( i = 0; sArg[i] != '\0'; i++ ) {
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

static bool __xprocCmdAppendQuoted(__xproc_strbuf* pBuf, str sArg)
{
	size_t i = 0;
	size_t iBackslash = 0;

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
			while ( iBackslash > 0 ) {
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

		while ( iBackslash > 0 ) {
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

	while ( iBackslash > 0 ) {
		if ( !__xprocStrBufAppendChar(pBuf, '\\') || !__xprocStrBufAppendChar(pBuf, '\\') ) {
			return false;
		}
		iBackslash--;
	}

	return __xprocStrBufAppendChar(pBuf, '"');
}

static bool __xprocBufferReserve(__xproc_buffer* pBuf, size_t iNeed)
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

static void __xprocBufferAppend(__xproc_buffer* pBuf, const void* pData, size_t iSize, size_t iLimit)
{
	size_t iCopy = iSize;

	if ( pBuf == NULL || pData == NULL || iSize == 0 ) {
		return;
	}

	if ( iLimit != 0u ) {
		if ( pBuf->iSize >= iLimit ) {
			pBuf->bTruncated = true;
			return;
		}
		if ( iCopy > (iLimit - pBuf->iSize) ) {
			iCopy = iLimit - pBuf->iSize;
			pBuf->bTruncated = true;
		}
	}

	if ( iCopy == 0 ) {
		return;
	}
	if ( !__xprocBufferReserve(pBuf, pBuf->iSize + iCopy + 1u) ) {
		pBuf->bTruncated = true;
		return;
	}

	memcpy(pBuf->pData + pBuf->iSize, pData, iCopy);
	pBuf->iSize += iCopy;
	pBuf->pData[pBuf->iSize] = '\0';
	if ( iCopy != iSize ) {
		pBuf->bTruncated = true;
	}
}

static void __xprocBufferUnit(__xproc_buffer* pBuf)
{
	if ( pBuf == NULL ) {
		return;
	}
	if ( pBuf->pData ) {
		xrtFree(pBuf->pData);
	}
	memset(pBuf, 0, sizeof(*pBuf));
}

static void __xprocBufferMoveOut(__xproc_buffer* pBuf, ptr* ppData, size_t* piSize, bool* pbTruncated)
{
	if ( ppData ) {
		*ppData = pBuf ? pBuf->pData : NULL;
	}
	if ( piSize ) {
		*piSize = pBuf ? pBuf->iSize : 0u;
	}
	if ( pbTruncated ) {
		*pbTruncated = pBuf ? pBuf->bTruncated : false;
	}
	if ( pBuf ) {
		pBuf->pData = NULL;
		pBuf->iSize = 0u;
		pBuf->iCap = 0u;
		pBuf->bTruncated = false;
	}
}

XXAPI void xrtProcessConfigInit(xprocessconfig* pConfig)
{
	if ( pConfig == NULL ) {
		return;
	}

	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->iReadChunkSize = __XPROC_READ_CHUNK_DEFAULT;
}

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
	memset(pResult, 0, sizeof(*pResult));
}

#if defined(XRT_NO_THREAD)

static void __xprocSetThreadRequiredError(void)
{
	xrtSetError("subprocess requires thread support.", FALSE);
}

XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
{
	(void)pConfig;
	__xprocSetThreadRequiredError();
	return NULL;
}

XXAPI void xrtProcessDestroy(xprocess* pProcess)
{
	(void)pProcess;
}

XXAPI int xrtProcessState(xprocess* pProcess)
{
	(void)pProcess;
	return XPROC_STATE_FAILED;
}

XXAPI bool xrtProcessIsRunning(xprocess* pProcess)
{
	(void)pProcess;
	return false;
}

XXAPI int xrtProcessExitCode(xprocess* pProcess)
{
	(void)pProcess;
	return -1;
}

XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize)
{
	(void)pProcess;
	(void)pData;
	(void)iSize;
	__xprocSetThreadRequiredError();
	return -1;
}

XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize)
{
	(void)pProcess;
	(void)sText;
	(void)iSize;
	__xprocSetThreadRequiredError();
	return -1;
}

XXAPI bool xrtProcessCloseStdin(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}

XXAPI bool xrtProcessWait(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}

XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs)
{
	(void)pProcess;
	(void)iTimeoutMs;
	__xprocSetThreadRequiredError();
	return XRT_WAIT_ERROR;
}

XXAPI bool xrtProcessTerminate(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}

XXAPI bool xrtProcessKillTree(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return false;
}

XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize)
{
	(void)pProcess;
	if ( piSize ) {
		*piSize = 0u;
	}
	return NULL;
}

XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize)
{
	(void)pProcess;
	if ( piSize ) {
		*piSize = 0u;
	}
	return NULL;
}

XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs)
{
	(void)pConfig;
	(void)pResult;
	(void)iTimeoutMs;
	__xprocSetThreadRequiredError();
	return false;
}

#if !defined(XRT_NO_NETWORK)
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess)
{
	(void)pProcess;
	__xprocSetThreadRequiredError();
	return NULL;
}
#endif

#else

typedef struct {
	xprocess* pProcess;
	int iStream;
} __xproc_pump_ctx;

struct xprocess_struct {
	volatile long iRefCount;
	volatile int iState;
	volatile int bExitReady;
	volatile int bStdoutDone;
	volatile int bStderrDone;
	int iExitCode;
	uint32 iFlags;
	uint32 iReadChunkSize;
	size_t iMaxCaptureBytes;
	xprocessevents Events;
	ptr pUserData;
	xmutex_struct Lock;
	xcond_struct Cond;
	__xproc_buffer StdoutBuf;
	__xproc_buffer StderrBuf;
	xthread hWaitThread;
	xthread hStdoutThread;
	xthread hStderrThread;
	__xproc_pump_ctx StdoutPump;
	__xproc_pump_ctx StderrPump;
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE hProcess;
		HANDLE hStdinWrite;
		HANDLE hStdoutRead;
		HANDLE hStderrRead;
		HANDLE hJob;
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

static xprocess* __xprocAddRef(xprocess* pProcess)
{
	if ( pProcess ) {
		(void)__xrtAtomicAddFetch32(&pProcess->iRefCount, 1);
	}
	return pProcess;
}

static void __xprocReleaseProcess(xprocess* pProcess)
{
	if ( pProcess && __xrtAtomicAddFetch32(&pProcess->iRefCount, -1) == 0 ) {
		__xprocFreeProcess(pProcess);
	}
}

#if !defined(XRT_NO_NETWORK)
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

static uint32 __xprocNormalizeFlags(uint32 iFlags)
{
	if ( (iFlags & XPROC_F_MERGE_STDERR) != 0u ) {
		iFlags |= XPROC_F_PIPE_STDOUT;
		iFlags &= ~XPROC_F_PIPE_STDERR;
	}
	return iFlags;
}

static bool __xprocValidateConfig(const xprocessconfig* pConfig, uint32 iFlags)
{
	if ( pConfig == NULL ) {
		xrtSetError("invalid subprocess config.", FALSE);
		return false;
	}

	if ( (iFlags & XPROC_F_USE_SHELL) != 0u ) {
		if ( pConfig->sCommandLine == NULL || pConfig->sCommandLine[0] == '\0' ) {
			xrtSetError("shell mode requires command line.", FALSE);
			return false;
		}
	} else {
		if ( pConfig->sProgram == NULL || pConfig->sProgram[0] == '\0' ) {
			xrtSetError("subprocess requires program path.", FALSE);
			return false;
		}
	}

	return true;
}

static xprocess* __xprocAllocProcess(const xprocessconfig* pConfig, uint32 iFlags)
{
	xprocess* pProcess = (xprocess*)xrtCalloc(1, sizeof(xprocess));

	if ( pProcess == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		return NULL;
	}

	pProcess->iRefCount = 1;
	pProcess->iState = XPROC_STATE_INIT;
	pProcess->iExitCode = -1;
	pProcess->iFlags = iFlags;
	pProcess->iReadChunkSize = pConfig->iReadChunkSize ? pConfig->iReadChunkSize : __XPROC_READ_CHUNK_DEFAULT;
	pProcess->iMaxCaptureBytes = pConfig->iMaxCaptureBytes;
	pProcess->pUserData = pConfig->pUserData;
	if ( pConfig->pEvents ) {
		pProcess->Events = *pConfig->pEvents;
	}

	xrtMutexInit(&pProcess->Lock);
	xrtCondInit(&pProcess->Cond);

	#if defined(_WIN32) || defined(_WIN64)
		pProcess->hProcess = NULL;
		pProcess->hStdinWrite = NULL;
		pProcess->hStdoutRead = NULL;
		pProcess->hStderrRead = NULL;
		pProcess->hJob = NULL;
	#else
		pProcess->iPid = -1;
		pProcess->fdStdinWrite = -1;
		pProcess->fdStdoutRead = -1;
		pProcess->fdStderrRead = -1;
	#endif

	return pProcess;
}

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

	__xprocBufferUnit(&pProcess->StdoutBuf);
	__xprocBufferUnit(&pProcess->StderrBuf);
	xrtCondUnit(&pProcess->Cond);
	xrtMutexUnit(&pProcess->Lock);
	xrtFree(pProcess);
}

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
	#else
		pProcess->iPid = -1;
	#endif
}

static void __xprocMarkStreamDone(xprocess* pProcess, int iStream)
{
	if ( pProcess == NULL ) {
		return;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( iStream == __XPROC_STREAM_STDOUT ) {
		pProcess->bStdoutDone = true;
	} else {
		pProcess->bStderrDone = true;
	}
	xrtCondBroadcast(&pProcess->Cond);
	xrtMutexUnlock(&pProcess->Lock);
}

static void __xprocHandleOutput(xprocess* pProcess, int iStream, const void* pData, size_t iSize)
{
	__xproc_buffer* pBuf = NULL;
	void (*OnOutput)(xprocess*, const void*, size_t, ptr) = NULL;

	if ( pProcess == NULL || pData == NULL || iSize == 0 ) {
		return;
	}

	if ( iStream == __XPROC_STREAM_STDERR && (pProcess->iFlags & XPROC_F_MERGE_STDERR) != 0u ) {
		pBuf = &pProcess->StdoutBuf;
		OnOutput = pProcess->Events.OnStdout;
	} else if ( iStream == __XPROC_STREAM_STDERR ) {
		pBuf = &pProcess->StderrBuf;
		OnOutput = pProcess->Events.OnStderr;
	} else {
		pBuf = &pProcess->StdoutBuf;
		OnOutput = pProcess->Events.OnStdout;
	}

	xrtMutexLock(&pProcess->Lock);
	if ( (pProcess->iFlags & XPROC_F_NO_CAPTURE) == 0u ) {
		__xprocBufferAppend(pBuf, pData, iSize, pProcess->iMaxCaptureBytes);
	}
	xrtMutexUnlock(&pProcess->Lock);

	if ( OnOutput ) {
		OnOutput(pProcess, pData, iSize, pProcess->pUserData);
	}
}

static bool __xprocTerminatePlatform(xprocess* pProcess, bool bKillTree);

static uint32 __xprocPumpThread(ptr pArg)
{
	__xproc_pump_ctx* pCtx = (__xproc_pump_ctx*)pArg;
	xprocess* pProcess = pCtx ? pCtx->pProcess : NULL;
	uint32 iChunk = (pProcess && pProcess->iReadChunkSize) ? pProcess->iReadChunkSize : __XPROC_READ_CHUNK_DEFAULT;
	char* pBuf = NULL;

	if ( pProcess == NULL || (pCtx->iStream != __XPROC_STREAM_STDOUT && pCtx->iStream != __XPROC_STREAM_STDERR) ) {
		return 1u;
	}

	pBuf = (char*)xrtMalloc(iChunk);
	if ( pBuf == NULL ) {
		xrtSetError("memory allocate failed.", FALSE);
		__xprocMarkStreamDone(pProcess, pCtx->iStream);
		return 2u;
	}

	for ( ;; ) {
		#if defined(_WIN32) || defined(_WIN64)
			HANDLE hRead = (pCtx->iStream == __XPROC_STREAM_STDOUT) ? pProcess->hStdoutRead : pProcess->hStderrRead;
			DWORD iRead = 0u;
			if ( hRead == NULL ) {
				break;
			}
			if ( !ReadFile(hRead, pBuf, iChunk, &iRead, NULL) ) {
				DWORD iErr = GetLastError();
				if ( iErr == ERROR_BROKEN_PIPE || iErr == ERROR_HANDLE_EOF ) {
					break;
				}
				break;
			}
			if ( iRead == 0u ) {
				break;
			}
			__xprocHandleOutput(pProcess, pCtx->iStream, pBuf, (size_t)iRead);
		#else
			int iFd = (pCtx->iStream == __XPROC_STREAM_STDOUT) ? pProcess->fdStdoutRead : pProcess->fdStderrRead;
			ssize_t iRead;
			if ( iFd < 0 ) {
				break;
			}
			do {
				iRead = read(iFd, pBuf, iChunk);
			} while ( iRead < 0 && errno == EINTR );
			if ( iRead <= 0 ) {
				break;
			}
			__xprocHandleOutput(pProcess, pCtx->iStream, pBuf, (size_t)iRead);
		#endif
	}

	xrtFree(pBuf);
	if ( pCtx->iStream == __XPROC_STREAM_STDOUT ) {
		__xprocCloseStdoutReadHandle(pProcess);
	} else {
		__xprocCloseStderrReadHandle(pProcess);
	}
	__xprocMarkStreamDone(pProcess, pCtx->iStream);
	return 0u;
}

#if defined(_WIN32) || defined(_WIN64)

static char* __xprocBuildWindowsCommandLine(const xprocessconfig* pConfig)
{
	__xproc_strbuf tBuf;
	bool bOk = false;
	uint32 i;

	memset(&tBuf, 0, sizeof(tBuf));

	if ( (pConfig->iFlags & XPROC_F_USE_SHELL) != 0u ) {
		bOk = __xprocStrBufAppend(&tBuf, "cmd.exe /C ");
		if ( bOk ) {
			bOk = __xprocStrBufAppend(&tBuf, (const char*)pConfig->sCommandLine);
		}
	} else {
		bOk = __xprocCmdAppendQuoted(&tBuf, pConfig->sProgram);
		for ( i = 0; bOk && i < pConfig->iArgCount; i++ ) {
			if ( !__xprocStrBufAppendChar(&tBuf, ' ') ) {
				bOk = false;
				break;
			}
			if ( !__xprocCmdAppendQuoted(&tBuf, pConfig->arrArgs ? pConfig->arrArgs[i] : NULL) ) {
				bOk = false;
				break;
			}
		}
	}

	if ( !bOk ) {
		__xprocStrBufUnit(&tBuf);
		xrtSetError("failed to build subprocess command line.", FALSE);
		return NULL;
	}

	return tBuf.sData;
}

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

static bool __xprocSpawnPlatform(xprocess* pProcess, const xprocessconfig* pConfig)
{
	SECURITY_ATTRIBUTES tSa;
	STARTUPINFOW tSi;
	PROCESS_INFORMATION tPi;
	HANDLE hChildStdinRead = NULL;
	HANDLE hChildStdoutWrite = NULL;
	HANDLE hChildStderrWrite = NULL;
	char* sCmdUtf8 = NULL;
	u16str sCmdW = NULL;
	u16str sWorkDirW = NULL;
	bool bUseStdHandles = false;
	BOOL bOk;
	DWORD iCreateFlags = 0u;

	memset(&tSa, 0, sizeof(tSa));
	memset(&tSi, 0, sizeof(tSi));
	memset(&tPi, 0, sizeof(tPi));
	tSa.nLength = sizeof(tSa);
	tSa.bInheritHandle = TRUE;
	tSi.cb = sizeof(tSi);

	if ( (pProcess->iFlags & XPROC_F_PIPE_STDIN) != 0u ) {
		if ( !CreatePipe(&hChildStdinRead, &pProcess->hStdinWrite, &tSa, 0) ) {
			xrtSetError("failed to create subprocess stdin pipe.", FALSE);
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStdinWrite, HANDLE_FLAG_INHERIT, 0);
		bUseStdHandles = true;
	}
	if ( (pProcess->iFlags & XPROC_F_PIPE_STDOUT) != 0u ) {
		if ( !CreatePipe(&pProcess->hStdoutRead, &hChildStdoutWrite, &tSa, 0) ) {
			xrtSetError("failed to create subprocess stdout pipe.", FALSE);
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStdoutRead, HANDLE_FLAG_INHERIT, 0);
		bUseStdHandles = true;
	}
	if ( (pProcess->iFlags & XPROC_F_MERGE_STDERR) != 0u ) {
		hChildStderrWrite = hChildStdoutWrite;
	} else if ( (pProcess->iFlags & XPROC_F_PIPE_STDERR) != 0u ) {
		if ( !CreatePipe(&pProcess->hStderrRead, &hChildStderrWrite, &tSa, 0) ) {
			xrtSetError("failed to create subprocess stderr pipe.", FALSE);
			goto fail;
		}
		(void)SetHandleInformation(pProcess->hStderrRead, HANDLE_FLAG_INHERIT, 0);
		bUseStdHandles = true;
	}

	if ( bUseStdHandles ) {
		tSi.dwFlags |= STARTF_USESTDHANDLES;
		tSi.hStdInput = hChildStdinRead ? hChildStdinRead : GetStdHandle(STD_INPUT_HANDLE);
		tSi.hStdOutput = hChildStdoutWrite ? hChildStdoutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
		tSi.hStdError = hChildStderrWrite ? hChildStderrWrite : GetStdHandle(STD_ERROR_HANDLE);
	}
	if ( (pProcess->iFlags & XPROC_F_HIDE_WINDOW) != 0u ) {
		tSi.dwFlags |= STARTF_USESHOWWINDOW;
		tSi.wShowWindow = SW_HIDE;
		iCreateFlags |= CREATE_NO_WINDOW;
	}

	sCmdUtf8 = __xprocBuildWindowsCommandLine(pConfig);
	if ( sCmdUtf8 == NULL ) {
		goto fail;
	}
	sCmdW = xrtUTF8to16((u8str)sCmdUtf8, 0, NULL);
	if ( sCmdW == NULL ) {
		xrtSetError("failed to convert subprocess command line.", FALSE);
		goto fail;
	}
	if ( pConfig->sWorkDir && pConfig->sWorkDir[0] ) {
		sWorkDirW = xrtUTF8to16((u8str)pConfig->sWorkDir, 0, NULL);
		if ( sWorkDirW == NULL ) {
			xrtSetError("failed to convert subprocess working directory.", FALSE);
			goto fail;
		}
	}

	bOk = CreateProcessW(NULL, sCmdW, NULL, NULL, bUseStdHandles ? TRUE : FALSE, iCreateFlags, NULL, sWorkDirW, &tSi, &tPi);
	if ( !bOk ) {
		xrtSetError("failed to start subprocess.", FALSE);
		goto fail;
	}

	pProcess->hProcess = tPi.hProcess;
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
	if ( (pProcess->iFlags & XPROC_F_KILL_TREE) != 0u ) {
		(void)__xprocEnsureJobObject(pProcess);
	}

	xrtFree(sCmdUtf8);
	xrtFree(sCmdW);
	if ( sWorkDirW ) {
		xrtFree(sWorkDirW);
	}
	return true;

fail:
	if ( hChildStdinRead ) CloseHandle(hChildStdinRead);
	if ( hChildStdoutWrite ) CloseHandle(hChildStdoutWrite);
	if ( hChildStderrWrite && hChildStderrWrite != hChildStdoutWrite ) CloseHandle(hChildStderrWrite);
	if ( tPi.hThread ) CloseHandle(tPi.hThread);
	if ( tPi.hProcess ) CloseHandle(tPi.hProcess);
	if ( sCmdUtf8 ) xrtFree(sCmdUtf8);
	if ( sCmdW ) xrtFree(sCmdW);
	if ( sWorkDirW ) xrtFree(sWorkDirW);
	__xprocClosePlatformHandles(pProcess);
	return false;
}

#else

static bool __xprocSpawnPlatform(xprocess* pProcess, const xprocessconfig* pConfig)
{
	int fdStdin[2] = { -1, -1 };
	int fdStdout[2] = { -1, -1 };
	int fdStderr[2] = { -1, -1 };
	char** arrExec = NULL;
	pid_t iPid;
	uint32 i;

	if ( (pProcess->iFlags & XPROC_F_PIPE_STDIN) != 0u ) {
		int iOne = 1;
		if ( socketpair(AF_UNIX, SOCK_STREAM, 0, fdStdin) != 0 ) {
			xrtSetError("failed to create subprocess stdin pipe.", FALSE);
			goto fail;
		}
		#if defined(SO_NOSIGPIPE)
			(void)setsockopt(fdStdin[1], SOL_SOCKET, SO_NOSIGPIPE, &iOne, sizeof(iOne));
		#endif
	}
	if ( (pProcess->iFlags & XPROC_F_PIPE_STDOUT) != 0u ) {
		if ( pipe(fdStdout) != 0 ) {
			xrtSetError("failed to create subprocess stdout pipe.", FALSE);
			goto fail;
		}
	}
	if ( (pProcess->iFlags & XPROC_F_MERGE_STDERR) == 0u && (pProcess->iFlags & XPROC_F_PIPE_STDERR) != 0u ) {
		if ( pipe(fdStderr) != 0 ) {
			xrtSetError("failed to create subprocess stderr pipe.", FALSE);
			goto fail;
		}
	}

	if ( (pProcess->iFlags & XPROC_F_USE_SHELL) == 0u ) {
		arrExec = (char**)xrtCalloc((size_t)pConfig->iArgCount + 2u, sizeof(char*));
		if ( arrExec == NULL ) {
			xrtSetError("memory allocate failed.", FALSE);
			goto fail;
		}
		arrExec[0] = (char*)pConfig->sProgram;
		for ( i = 0; i < pConfig->iArgCount; i++ ) {
			arrExec[i + 1u] = (char*)(pConfig->arrArgs ? pConfig->arrArgs[i] : NULL);
		}
		arrExec[pConfig->iArgCount + 1u] = NULL;
	}

	iPid = fork();
	if ( iPid < 0 ) {
		xrtSetError("failed to fork subprocess.", FALSE);
		goto fail;
	}
	if ( iPid == 0 ) {
		if ( pConfig->sWorkDir && pConfig->sWorkDir[0] ) (void)chdir(pConfig->sWorkDir);
		(void)setpgid(0, 0);
		if ( fdStdin[0] >= 0 ) dup2(fdStdin[0], STDIN_FILENO);
		if ( fdStdout[1] >= 0 ) dup2(fdStdout[1], STDOUT_FILENO);
		if ( (pProcess->iFlags & XPROC_F_MERGE_STDERR) != 0u && fdStdout[1] >= 0 ) dup2(fdStdout[1], STDERR_FILENO);
		else if ( fdStderr[1] >= 0 ) dup2(fdStderr[1], STDERR_FILENO);
		if ( fdStdin[0] >= 0 ) close(fdStdin[0]);
		if ( fdStdin[1] >= 0 ) close(fdStdin[1]);
		if ( fdStdout[0] >= 0 ) close(fdStdout[0]);
		if ( fdStdout[1] >= 0 ) close(fdStdout[1]);
		if ( fdStderr[0] >= 0 ) close(fdStderr[0]);
		if ( fdStderr[1] >= 0 ) close(fdStderr[1]);
		if ( (pProcess->iFlags & XPROC_F_USE_SHELL) != 0u ) execl("/bin/sh", "sh", "-c", pConfig->sCommandLine, (char*)NULL);
		else execvp(pConfig->sProgram, arrExec);
		_exit(127);
	}

	pProcess->iPid = iPid;
	if ( fdStdin[0] >= 0 ) { close(fdStdin[0]); pProcess->fdStdinWrite = fdStdin[1]; fdStdin[0] = -1; fdStdin[1] = -1; }
	if ( fdStdout[1] >= 0 ) { close(fdStdout[1]); pProcess->fdStdoutRead = fdStdout[0]; fdStdout[0] = -1; fdStdout[1] = -1; }
	if ( fdStderr[1] >= 0 ) { close(fdStderr[1]); pProcess->fdStderrRead = fdStderr[0]; fdStderr[0] = -1; fdStderr[1] = -1; }
	if ( arrExec ) xrtFree(arrExec);
	return true;

fail:
	if ( arrExec ) xrtFree(arrExec);
	if ( fdStdin[0] >= 0 ) close(fdStdin[0]);
	if ( fdStdin[1] >= 0 ) close(fdStdin[1]);
	if ( fdStdout[0] >= 0 ) close(fdStdout[0]);
	if ( fdStdout[1] >= 0 ) close(fdStdout[1]);
	if ( fdStderr[0] >= 0 ) close(fdStderr[0]);
	if ( fdStderr[1] >= 0 ) close(fdStderr[1]);
	return false;
}

#endif

static bool __xprocTerminatePlatform(xprocess* pProcess, bool bKillTree)
{
	if ( pProcess == NULL ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		if ( pProcess->hProcess == NULL ) return true;
		if ( bKillTree && __xprocEnsureJobObject(pProcess) && TerminateJobObject(pProcess->hJob, 1u) ) return true;
		if ( TerminateProcess(pProcess->hProcess, 1u) ) return true;
		return GetLastError() == ERROR_ACCESS_DENIED && pProcess->bExitReady;
	#else
		int iRet;
		if ( pProcess->iPid <= 0 ) return true;
		iRet = kill(bKillTree ? -pProcess->iPid : pProcess->iPid, bKillTree ? SIGKILL : SIGTERM);
		return (iRet == 0) || (errno == ESRCH);
	#endif
}

static uint32 __xprocWaitThread(ptr pArg)
{
	xprocess* pProcess = (xprocess*)pArg;
	int iExitCode = -1;
	int iState = XPROC_STATE_EXITED;
	#if !defined(XRT_NO_NETWORK)
		xpromise* pPromise = NULL;
	#endif

	if ( pProcess == NULL ) return 1u;
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iWaitRet = WaitForSingleObject(pProcess->hProcess, INFINITE);
			if ( iWaitRet == WAIT_OBJECT_0 ) {
				DWORD iWinExit = 0u;
				if ( GetExitCodeProcess(pProcess->hProcess, &iWinExit) ) iExitCode = (int)iWinExit;
			} else {
				iState = XPROC_STATE_FAILED;
			}
		}
	#else
		{
			int iStatus = 0;
			pid_t iWaitRet;
			do { iWaitRet = waitpid(pProcess->iPid, &iStatus, 0); } while ( iWaitRet < 0 && errno == EINTR );
			if ( iWaitRet < 0 ) iState = XPROC_STATE_FAILED;
			else if ( WIFEXITED(iStatus) ) iExitCode = WEXITSTATUS(iStatus);
			else if ( WIFSIGNALED(iStatus) ) iExitCode = 128 + WTERMSIG(iStatus);
		}
	#endif

	__xprocCloseStdinHandle(pProcess);
	if ( pProcess->hStdoutThread ) xrtThreadWait(pProcess->hStdoutThread);
	if ( pProcess->hStderrThread ) xrtThreadWait(pProcess->hStderrThread);
	xrtMutexLock(&pProcess->Lock);
	pProcess->iExitCode = iExitCode;
	pProcess->iState = iState;
	pProcess->bExitReady = true;
	xrtCondBroadcast(&pProcess->Cond);
	#if !defined(XRT_NO_NETWORK)
		pPromise = pProcess->pWaitPromise;
		pProcess->pWaitPromise = NULL;
	#endif
	xrtMutexUnlock(&pProcess->Lock);
	#if !defined(XRT_NO_NETWORK)
		if ( pPromise ) { (void)xPromiseResolve(pPromise, pProcess); xPromiseDestroy(pPromise); }
	#endif
	if ( pProcess->Events.OnExit ) pProcess->Events.OnExit(pProcess, pProcess->iExitCode, pProcess->pUserData);
	return 0u;
}

static void __xprocCleanupSpawnFailure(xprocess* pProcess)
{
	if ( pProcess == NULL ) return;
	(void)__xprocTerminatePlatform(pProcess, true);
	if ( pProcess->hWaitThread ) xrtThreadWait(pProcess->hWaitThread);
	if ( pProcess->hStdoutThread ) xrtThreadWait(pProcess->hStdoutThread);
	if ( pProcess->hStderrThread ) xrtThreadWait(pProcess->hStderrThread);
	__xprocDestroyThreads(pProcess);
	__xprocClosePlatformHandles(pProcess);
	__xprocFreeProcess(pProcess);
}

XXAPI xprocess* xrtProcessSpawn(const xprocessconfig* pConfig)
{
	xprocess* pProcess;
	uint32 iFlags = __xprocNormalizeFlags(pConfig ? pConfig->iFlags : 0u);
	if ( !__xprocValidateConfig(pConfig, iFlags) ) return NULL;
	pProcess = __xprocAllocProcess(pConfig, iFlags);
	if ( pProcess == NULL ) return NULL;
	if ( !__xprocSpawnPlatform(pProcess, pConfig) ) { __xprocFreeProcess(pProcess); return NULL; }
	if ( (iFlags & XPROC_F_PIPE_STDOUT) == 0u ) pProcess->bStdoutDone = true;
	if ( (iFlags & XPROC_F_PIPE_STDERR) == 0u || (iFlags & XPROC_F_MERGE_STDERR) != 0u ) pProcess->bStderrDone = true;
	pProcess->StdoutPump.pProcess = pProcess;
	pProcess->StdoutPump.iStream = __XPROC_STREAM_STDOUT;
	pProcess->StderrPump.pProcess = pProcess;
	pProcess->StderrPump.iStream = __XPROC_STREAM_STDERR;
	if ( (iFlags & XPROC_F_PIPE_STDOUT) != 0u ) {
		pProcess->hStdoutThread = xrtThreadCreate(__xprocPumpThread, &pProcess->StdoutPump, 0);
		if ( pProcess->hStdoutThread == NULL ) { xrtSetError("failed to create subprocess stdout thread.", FALSE); __xprocCleanupSpawnFailure(pProcess); return NULL; }
	}
	if ( (iFlags & XPROC_F_PIPE_STDERR) != 0u && (iFlags & XPROC_F_MERGE_STDERR) == 0u ) {
		pProcess->hStderrThread = xrtThreadCreate(__xprocPumpThread, &pProcess->StderrPump, 0);
		if ( pProcess->hStderrThread == NULL ) { xrtSetError("failed to create subprocess stderr thread.", FALSE); __xprocCleanupSpawnFailure(pProcess); return NULL; }
	}
	pProcess->iState = XPROC_STATE_RUNNING;
	pProcess->hWaitThread = xrtThreadCreate(__xprocWaitThread, pProcess, 0);
	if ( pProcess->hWaitThread == NULL ) { xrtSetError("failed to create subprocess wait thread.", FALSE); __xprocCleanupSpawnFailure(pProcess); return NULL; }
	if ( pProcess->Events.OnStart ) pProcess->Events.OnStart(pProcess, pProcess->pUserData);
	return pProcess;
}

XXAPI void xrtProcessDestroy(xprocess* pProcess)
{
	if ( pProcess == NULL ) return;
	if ( pProcess->iState == XPROC_STATE_RUNNING && !pProcess->bExitReady ) { xrtSetError("subprocess is still running.", FALSE); return; }
	if ( pProcess->hWaitThread ) xrtThreadWait(pProcess->hWaitThread);
	if ( pProcess->hStdoutThread ) xrtThreadWait(pProcess->hStdoutThread);
	if ( pProcess->hStderrThread ) xrtThreadWait(pProcess->hStderrThread);
	pProcess->iState = XPROC_STATE_CLOSED;
	__xprocDestroyThreads(pProcess);
	__xprocClosePlatformHandles(pProcess);
	#if !defined(XRT_NO_NETWORK)
		__xprocDetachWaitFuture(pProcess);
	#endif
	__xprocReleaseProcess(pProcess);
}

XXAPI int xrtProcessState(xprocess* pProcess) { return pProcess ? pProcess->iState : XPROC_STATE_FAILED; }
XXAPI bool xrtProcessIsRunning(xprocess* pProcess) { return pProcess ? (pProcess->iState == XPROC_STATE_RUNNING && !pProcess->bExitReady) : false; }
XXAPI int xrtProcessExitCode(xprocess* pProcess) { return pProcess ? pProcess->iExitCode : -1; }

XXAPI int64 xrtProcessWrite(xprocess* pProcess, const void* pData, size_t iSize)
{
	if ( pProcess == NULL || pData == NULL || iSize == 0 ) return 0;
	#if defined(_WIN32) || defined(_WIN64)
		{
			DWORD iWritten = 0u;
			if ( pProcess->hStdinWrite == NULL ) { xrtSetError("subprocess stdin pipe is not available.", FALSE); return -1; }
			if ( !WriteFile(pProcess->hStdinWrite, pData, (DWORD)iSize, &iWritten, NULL) ) { xrtSetError("failed to write subprocess stdin.", FALSE); return -1; }
			return (int64)iWritten;
		}
	#else
		{
			const char* pCursor = (const char*)pData;
			size_t iLeft = iSize;
			if ( pProcess->fdStdinWrite < 0 ) { xrtSetError("subprocess stdin pipe is not available.", FALSE); return -1; }
			while ( iLeft > 0 ) {
				ssize_t iWritten;
				#if defined(MSG_NOSIGNAL)
					iWritten = send(pProcess->fdStdinWrite, pCursor, iLeft, MSG_NOSIGNAL);
				#else
					iWritten = send(pProcess->fdStdinWrite, pCursor, iLeft, 0);
				#endif
				if ( iWritten < 0 && errno == EINTR ) continue;
				if ( iWritten <= 0 ) { xrtSetError("failed to write subprocess stdin.", FALSE); return -1; }
				pCursor += iWritten;
				iLeft -= (size_t)iWritten;
			}
			return (int64)iSize;
		}
	#endif
}

XXAPI int64 xrtProcessWriteText(xprocess* pProcess, str sText, size_t iSize)
{
	if ( sText == NULL ) return 0;
	if ( iSize == 0 ) iSize = strlen((const char*)sText);
	return xrtProcessWrite(pProcess, sText, iSize);
}

XXAPI bool xrtProcessCloseStdin(xprocess* pProcess)
{
	if ( pProcess == NULL ) { xrtSetError("invalid subprocess handle.", FALSE); return false; }
	__xprocCloseStdinHandle(pProcess);
	return true;
}

XXAPI bool xrtProcessWait(xprocess* pProcess) { return xrtProcessWaitTimeout(pProcess, UINT32_MAX) == XRT_WAIT_OK; }

XXAPI int xrtProcessWaitTimeout(xprocess* pProcess, uint32 iTimeoutMs)
{
	uint64 iDeadline = 0u;
	if ( pProcess == NULL ) { xrtSetError("invalid subprocess handle.", FALSE); return XRT_WAIT_ERROR; }
	xrtMutexLock(&pProcess->Lock);
	if ( iTimeoutMs != UINT32_MAX ) iDeadline = __xprocNowMs() + iTimeoutMs;
	while ( !pProcess->bExitReady ) {
		if ( iTimeoutMs == UINT32_MAX ) xrtCondWait(&pProcess->Cond, &pProcess->Lock);
		else {
			uint64 iNow = __xprocNowMs();
			if ( iNow >= iDeadline ) { xrtMutexUnlock(&pProcess->Lock); return XRT_WAIT_TIMEOUT; }
			{
				uint64 iRemain = iDeadline - iNow;
				int iRet = xrtCondWaitTimeout(&pProcess->Cond, &pProcess->Lock, (iRemain > UINT32_MAX) ? UINT32_MAX : (uint32)iRemain);
				if ( iRet == XRT_WAIT_TIMEOUT && !pProcess->bExitReady ) { xrtMutexUnlock(&pProcess->Lock); return XRT_WAIT_TIMEOUT; }
				if ( iRet == XRT_WAIT_ERROR ) { xrtMutexUnlock(&pProcess->Lock); return XRT_WAIT_ERROR; }
			}
		}
	}
	xrtMutexUnlock(&pProcess->Lock);
	if ( pProcess->hWaitThread ) xrtThreadWait(pProcess->hWaitThread);
	return XRT_WAIT_OK;
}

XXAPI bool xrtProcessTerminate(xprocess* pProcess)
{
	if ( pProcess == NULL ) { xrtSetError("invalid subprocess handle.", FALSE); return false; }
	if ( pProcess->bExitReady ) return true;
	if ( !__xprocTerminatePlatform(pProcess, false) ) { xrtSetError("failed to terminate subprocess.", FALSE); return false; }
	return true;
}

XXAPI bool xrtProcessKillTree(xprocess* pProcess)
{
	if ( pProcess == NULL ) { xrtSetError("invalid subprocess handle.", FALSE); return false; }
	if ( pProcess->bExitReady ) return true;
	if ( !__xprocTerminatePlatform(pProcess, true) ) { xrtSetError("failed to kill subprocess tree.", FALSE); return false; }
	return true;
}

XXAPI ptr xrtProcessGetStdout(xprocess* pProcess, size_t* piSize)
{
	ptr pData = NULL;
	if ( piSize ) *piSize = 0u;
	if ( pProcess == NULL ) return NULL;
	xrtMutexLock(&pProcess->Lock);
	pData = pProcess->StdoutBuf.pData;
	if ( piSize ) *piSize = pProcess->StdoutBuf.iSize;
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}

XXAPI ptr xrtProcessGetStderr(xprocess* pProcess, size_t* piSize)
{
	ptr pData = NULL;
	if ( piSize ) *piSize = 0u;
	if ( pProcess == NULL ) return NULL;
	xrtMutexLock(&pProcess->Lock);
	pData = pProcess->StderrBuf.pData;
	if ( piSize ) *piSize = pProcess->StderrBuf.iSize;
	xrtMutexUnlock(&pProcess->Lock);
	return pData;
}

XXAPI bool xrtExecCapture(const xprocessconfig* pConfig, xprocessresult* pResult, uint32 iTimeoutMs)
{
	xprocessconfig tConfig;
	xprocess* pProcess;
	int iWaitRet;
	if ( pConfig == NULL || pResult == NULL ) { xrtSetError("invalid subprocess capture arguments.", FALSE); return false; }
	tConfig = *pConfig;
	tConfig.iFlags = __xprocNormalizeFlags(tConfig.iFlags);
	tConfig.iFlags &= ~XPROC_F_NO_CAPTURE;
	tConfig.iFlags |= XPROC_F_PIPE_STDOUT;
	if ( (tConfig.iFlags & XPROC_F_MERGE_STDERR) == 0u ) tConfig.iFlags |= XPROC_F_PIPE_STDERR;
	pProcess = xrtProcessSpawn(&tConfig);
	if ( pProcess == NULL ) return false;
	iWaitRet = xrtProcessWaitTimeout(pProcess, iTimeoutMs == 0u ? UINT32_MAX : iTimeoutMs);
	if ( iWaitRet != XRT_WAIT_OK ) {
		(void)xrtProcessKillTree(pProcess);
		(void)xrtProcessTerminate(pProcess);
		(void)xrtProcessWait(pProcess);
		xrtProcessDestroy(pProcess);
		if ( iWaitRet == XRT_WAIT_TIMEOUT ) xrtSetError("subprocess capture wait timeout.", FALSE);
		return false;
	}
	memset(pResult, 0, sizeof(*pResult));
	pResult->iExitCode = xrtProcessExitCode(pProcess);
	__xprocBufferMoveOut(&pProcess->StdoutBuf, &pResult->pStdout, &pResult->iStdoutSize, &pResult->bStdoutTruncated);
	__xprocBufferMoveOut(&pProcess->StderrBuf, &pResult->pStderr, &pResult->iStderrSize, &pResult->bStderrTruncated);
	xrtProcessDestroy(pProcess);
	return true;
}

#if !defined(XRT_NO_NETWORK)
XXAPI xfuture* xrtProcessWaitFuture(xprocess* pProcess)
{
	xfuture* pFuture = NULL;
	xpromise* pPromise = NULL;
	bool bResolveNow = false;
	if ( pProcess == NULL ) { xrtSetError("invalid subprocess handle.", FALSE); return NULL; }
	xrtMutexLock(&pProcess->Lock);
	if ( pProcess->pWaitFuture == NULL ) {
		pFuture = xFutureCreate();
		if ( pFuture == NULL ) { xrtMutexUnlock(&pProcess->Lock); xrtSetError("failed to create subprocess wait future.", FALSE); return NULL; }
		pPromise = xPromiseCreate(pFuture);
		if ( pPromise == NULL ) { xrtMutexUnlock(&pProcess->Lock); xFutureRelease(pFuture); xrtSetError("failed to create subprocess wait promise.", FALSE); return NULL; }
		pFuture->pPendingCtx = __xprocAddRef(pProcess);
		pFuture->pfnPendingCleanup = __xprocWaitFutureCleanup;
		pProcess->pWaitFuture = pFuture;
		if ( pProcess->bExitReady ) bResolveNow = true;
		else { pProcess->pWaitPromise = pPromise; pPromise = NULL; }
	}
	pFuture = xFutureAddRef(pProcess->pWaitFuture);
	xrtMutexUnlock(&pProcess->Lock);
	if ( bResolveNow && pPromise ) { (void)xPromiseResolve(pPromise, pProcess); xPromiseDestroy(pPromise); }
	return pFuture;
}
#endif
#endif
