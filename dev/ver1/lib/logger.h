


/* ================================ 日志系统 ================================ */

#define XLOG_APPENDER_CONSOLE	1
#define XLOG_APPENDER_FILE		2
#define XLOG_APPENDER_CUSTOM	3

#define XLOG_COLOR_TRACE		"\033[90m"
#define XLOG_COLOR_DEBUG		"\033[37;1m"
#define XLOG_COLOR_INFO			"\033[39m"
#define XLOG_COLOR_WARN			"\033[33m"
#define XLOG_COLOR_ERROR		"\033[31m"
#define XLOG_COLOR_FATAL		"\033[35m"
#define XLOG_COLOR_RESET		"\033[0m"

struct xlogappender {
	xlogger* pOwner;
	str sName;
	xloglevel iMinLevel;
	xlogformat iFormat;
	int iType;
	bool bColor;
	bool bOwnFile;
	FILE* pFile;
	str sPath;
	uint64 iMaxSize;
	uint32 iMaxBackup;
	xlogcustomproc Proc;
	ptr pUserData;
	xlogfreeproc FreeUserData;
	volatile int32 iActiveCallbacks;
};

struct xlogger {
	volatile int32 iRefCount;
	str sName;
	xloglevel iLevel;
	xmutex pLock;
	xlogappender** arrAppender;
	uint32 iAppenderCount;
	uint32 iAppenderCapacity;
};

static xlogger* __g_pXlogDefault = NULL;


// 内部函数：获取级别颜色
static const char* __xlogLevelColor(xloglevel iLevel)
{
	switch ( iLevel ) {
		case XLOG_TRACE: return XLOG_COLOR_TRACE;
		case XLOG_DEBUG: return XLOG_COLOR_DEBUG;
		case XLOG_INFO: return XLOG_COLOR_INFO;
		case XLOG_WARN: return XLOG_COLOR_WARN;
		case XLOG_ERROR: return XLOG_COLOR_ERROR;
		case XLOG_FATAL: return XLOG_COLOR_FATAL;
		default: return "";
	}
}


// 内部函数：判断是否需要释放字符串
static bool __xlogOwnStr(str sText)
{
	return sText && sText != xCore.sNull;
}


// 内部函数：追加打开日志文件，Windows 下保持 UTF-8 路径语义
static FILE* __xlogOpenAppendFile(str sPath)
{
	if ( !sPath || sPath == xCore.sNull ) {
		return NULL;
	}

	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16((u8str)sPath, 0, NULL);
		FILE* pFile = NULL;
		if ( !sPathW ) {
			return NULL;
		}
		pFile = _wfsopen((const wchar_t*)sPathW, L"ab", _SH_DENYNO);
		xrtFree(sPathW);
		return pFile;
	#else
		return fopen((const char*)sPath, "ab");
	#endif
}


// 内部函数：格式化当前本地时间
// 内部函数：删除日志文件，避免 logger 在 XRT_NO_FILE 裁剪时依赖 file 模块
static bool __xlogDeleteFile(str sPath)
{
	if ( !sPath || sPath == xCore.sNull ) {
		return FALSE;
	}

	#if defined(_WIN32) || defined(_WIN64)
		u16str sPathW = xrtUTF8to16((u8str)sPath, 0, NULL);
		int iRet;
		if ( !sPathW ) {
			return FALSE;
		}
		iRet = _wremove((const wchar_t*)sPathW);
		xrtFree(sPathW);
		return iRet == 0;
	#else
		return remove((const char*)sPath) == 0;
	#endif
}


// 内部函数：重命名日志文件，避免 logger 在 XRT_NO_FILE 裁剪时依赖 file 模块
static bool __xlogRenameFile(str sSrc, str sDst)
{
	if ( !sSrc || sSrc == xCore.sNull || !sDst || sDst == xCore.sNull ) {
		return FALSE;
	}

	#if defined(_WIN32) || defined(_WIN64)
		u16str sSrcW = xrtUTF8to16((u8str)sSrc, 0, NULL);
		u16str sDstW = xrtUTF8to16((u8str)sDst, 0, NULL);
		int iRet;
		if ( !sSrcW || !sDstW ) {
			xrtFree(sSrcW);
			xrtFree(sDstW);
			return FALSE;
		}
		iRet = _wrename((const wchar_t*)sSrcW, (const wchar_t*)sDstW);
		xrtFree(sSrcW);
		xrtFree(sDstW);
		return iRet == 0;
	#else
		return rename((const char*)sSrc, (const char*)sDst) == 0;
	#endif
}


static void __xlogFormatNow(char* sBuff, size_t iSize)
{
	time_t tRaw;
	struct tm tLocal;

	if ( !sBuff || iSize == 0 ) {
		return;
	}

	tRaw = time(NULL);
	memset(&tLocal, 0, sizeof(tLocal));
	#if defined(_WIN32) || defined(_WIN64)
		localtime_s(&tLocal, &tRaw);
	#else
		localtime_r(&tRaw, &tLocal);
	#endif
	if ( strftime(sBuff, iSize, "%Y-%m-%d %H:%M:%S", &tLocal) == 0 ) {
		sBuff[0] = '\0';
	}
}


// 内部函数：JSON 字符串转义写入
static void __xlogJsonWriteEscaped(FILE* pFile, const char* sText)
{
	const unsigned char* p;

	if ( !pFile ) {
		return;
	}

	fputc('"', pFile);
	if ( sText ) {
		for ( p = (const unsigned char*)sText; *p; p++ ) {
			switch ( *p ) {
				case '\\': fputs("\\\\", pFile); break;
				case '"': fputs("\\\"", pFile); break;
				case '\n': fputs("\\n", pFile); break;
				case '\r': fputs("\\r", pFile); break;
				case '\t': fputs("\\t", pFile); break;
				default:
					if ( *p < 0x20 ) {
						fprintf(pFile, "\\u%04x", (unsigned int)*p);
					} else {
						fputc(*p, pFile);
					}
					break;
			}
		}
	}
	fputc('"', pFile);
}


// 内部函数：写入格式化事件
static void __xlogWriteFormatted(FILE* pFile, xlogformat iFormat, bool bColor, const xlogevent* pEvent)
{
	char sTime[32];
	const char* sLevel;
	const char* sColor;

	if ( !pFile || !pEvent ) {
		return;
	}

	sLevel = __xrt_cstr(xlogLevelName(pEvent->iLevel));
	sColor = bColor ? __xlogLevelColor(pEvent->iLevel) : "";
	__xlogFormatNow(sTime, sizeof(sTime));

	if ( iFormat == XLOG_FORMAT_SIMPLE ) {
		if ( bColor ) {
			fprintf(pFile, "%s[%s]%s %s\n", sColor, sLevel, XLOG_COLOR_RESET, pEvent->sMessage ? pEvent->sMessage : "");
		} else {
			fprintf(pFile, "[%s] %s\n", sLevel, pEvent->sMessage ? pEvent->sMessage : "");
		}
		return;
	}

	if ( iFormat == XLOG_FORMAT_JSON ) {
		fprintf(pFile, "{\"time\":");
		__xlogJsonWriteEscaped(pFile, sTime);
		fprintf(pFile, ",\"level\":");
		__xlogJsonWriteEscaped(pFile, sLevel);
		fprintf(pFile, ",\"logger\":");
		__xlogJsonWriteEscaped(pFile, pEvent->sLogger);
		fprintf(pFile, ",\"thread\":%llu,\"file\":", (unsigned long long)pEvent->iThreadId);
		__xlogJsonWriteEscaped(pFile, pEvent->sFile);
		fprintf(pFile, ",\"line\":%u,\"func\":", pEvent->iLine);
		__xlogJsonWriteEscaped(pFile, pEvent->sFunc);
		fprintf(pFile, ",\"msg\":");
		__xlogJsonWriteEscaped(pFile, pEvent->sMessage);
		fprintf(pFile, "}\n");
		return;
	}

	if ( bColor ) {
		fprintf(pFile, "%s [%s%-5s%s] [tid=%llu] [%s:%u %s] %s\n",
			sTime,
			sColor,
			sLevel,
			XLOG_COLOR_RESET,
			(unsigned long long)pEvent->iThreadId,
			pEvent->sFile ? pEvent->sFile : "",
			pEvent->iLine,
			pEvent->sFunc ? pEvent->sFunc : "",
			pEvent->sMessage ? pEvent->sMessage : "");
	} else {
		fprintf(pFile, "%s [%-5s] [tid=%llu] [%s:%u %s] %s\n",
			sTime,
			sLevel,
			(unsigned long long)pEvent->iThreadId,
			pEvent->sFile ? pEvent->sFile : "",
			pEvent->iLine,
			pEvent->sFunc ? pEvent->sFunc : "",
			pEvent->sMessage ? pEvent->sMessage : "");
	}
}


// 内部函数：创建备份路径
static str __xlogMakeBackupPath(str sPath, uint32 iIndex)
{
	if ( !sPath || iIndex == 0 ) {
		return xCore.sNull;
	}

	return xrtFormat("%s.%u", sPath, iIndex);
}


// 内部函数：执行日志滚动
static bool __xlogRotateFile(xlogappender* pAppender)
{
	str sSrc;
	str sDst;

	if ( !pAppender || !pAppender->sPath || pAppender->iMaxSize == 0 || pAppender->iMaxBackup == 0 ) {
		return TRUE;
	}

	if ( pAppender->pFile ) {
		fclose(pAppender->pFile);
		pAppender->pFile = NULL;
	}

	sDst = __xlogMakeBackupPath(pAppender->sPath, pAppender->iMaxBackup);
	if ( __xlogOwnStr(sDst) ) {
		__xlogDeleteFile(sDst);
		xrtFree(sDst);
	}

	for ( uint32 i = pAppender->iMaxBackup; i > 1; i-- ) {
		sSrc = __xlogMakeBackupPath(pAppender->sPath, i - 1);
		sDst = __xlogMakeBackupPath(pAppender->sPath, i);
		if ( __xlogOwnStr(sSrc) && __xlogOwnStr(sDst) ) {
			__xlogDeleteFile(sDst);
			__xlogRenameFile(sSrc, sDst);
		}
		if ( __xlogOwnStr(sSrc) ) {
			xrtFree(sSrc);
		}
		if ( __xlogOwnStr(sDst) ) {
			xrtFree(sDst);
		}
	}

	sDst = __xlogMakeBackupPath(pAppender->sPath, 1);
	if ( __xlogOwnStr(sDst) ) {
		__xlogDeleteFile(sDst);
		__xlogRenameFile(pAppender->sPath, sDst);
		xrtFree(sDst);
	}

	pAppender->pFile = __xlogOpenAppendFile(pAppender->sPath);
	if ( !pAppender->pFile ) {
		xrtSetError("logger reopen rolling file failed.", FALSE);
		return FALSE;
	}
	return TRUE;
}


// 内部函数：按需滚动
static bool __xlogRotateIfNeeded(xlogappender* pAppender, size_t iMessageSize)
{
	long iPos;

	if ( !pAppender || !pAppender->pFile || pAppender->iMaxSize == 0 || pAppender->iMaxBackup == 0 ) {
		return TRUE;
	}

	iPos = ftell(pAppender->pFile);
	if ( iPos < 0 ) {
		return TRUE;
	}

	if ( ((uint64)iPos + (uint64)iMessageSize) < pAppender->iMaxSize ) {
		return TRUE;
	}

	return __xlogRotateFile(pAppender);
}


// 内部函数：创建输出器
static xlogappender* __xlogAppenderCreate(str sName, int iType, xloglevel iMinLevel)
{
	xlogappender* pAppender;

	pAppender = (xlogappender*)xrtCalloc(1, sizeof(xlogappender));
	if ( !pAppender ) {
		return NULL;
	}

	pAppender->sName = xrtCopyStr(sName ? sName : (str)"appender", 0);
	pAppender->iMinLevel = iMinLevel;
	pAppender->iFormat = XLOG_FORMAT_TEXT;
	pAppender->iType = iType;
	pAppender->bColor = FALSE;
	return pAppender;
}


// 内部函数：释放输出器
static void __xlogAppenderDestroy(xlogappender* pAppender)
{
	if ( !pAppender ) {
		return;
	}

	if ( pAppender->bOwnFile && pAppender->pFile ) {
		fclose(pAppender->pFile);
		pAppender->pFile = NULL;
	}
	if ( __xlogOwnStr(pAppender->sName) ) {
		xrtFree(pAppender->sName);
	}
	if ( __xlogOwnStr(pAppender->sPath) ) {
		xrtFree(pAppender->sPath);
	}
	if ( pAppender->FreeUserData && pAppender->pUserData ) {
		pAppender->FreeUserData(pAppender->pUserData);
		pAppender->pUserData = NULL;
	}
	xrtFree(pAppender);
}


// 内部函数：追加输出器
static bool __xlogAddAppender(xlogger* pLogger, xlogappender* pAppender)
{
	xlogappender** arrNew;
	uint32 iCapacity;

	if ( !pLogger || !pAppender ) {
		return FALSE;
	}

	xrtMutexLock(pLogger->pLock);
	if ( pLogger->iAppenderCount >= pLogger->iAppenderCapacity ) {
		iCapacity = pLogger->iAppenderCapacity == 0 ? 4 : pLogger->iAppenderCapacity * 2;
		arrNew = (xlogappender**)xrtRealloc(pLogger->arrAppender, sizeof(xlogappender*) * iCapacity);
		if ( !arrNew ) {
			xrtMutexUnlock(pLogger->pLock);
			return FALSE;
		}
		pLogger->arrAppender = arrNew;
		pLogger->iAppenderCapacity = iCapacity;
	}

	pAppender->pOwner = pLogger;
	pLogger->arrAppender[pLogger->iAppenderCount++] = pAppender;
	xrtMutexUnlock(pLogger->pLock);
	return TRUE;
}


// 创建日志器
XXAPI xlogger* xlogCreate(str sName)
{
	xlogger* pLogger;

	pLogger = (xlogger*)xrtCalloc(1, sizeof(xlogger));
	if ( !pLogger ) {
		return NULL;
	}
	pLogger->iRefCount = 1;

	pLogger->sName = xrtCopyStr(sName ? sName : (str)"default", 0);
	pLogger->iLevel = XLOG_INFO;
	pLogger->pLock = xrtMutexCreate();
	if ( !pLogger->pLock ) {
		if ( __xlogOwnStr(pLogger->sName) ) {
			xrtFree(pLogger->sName);
		}
		xrtFree(pLogger);
		return NULL;
	}

	return pLogger;
}


// 保留日志器引用
XXAPI xlogger* xlogAddRef(xlogger* pLogger)
{
	if ( pLogger ) {
		xrtAtomicRefRetain(&pLogger->iRefCount);
	}
	return pLogger;
}


// 释放日志器实际资源
static void __xlogDestroy(xlogger* pLogger)
{
	if ( !pLogger ) {
		return;
	}

	xlogFlush(pLogger);

	for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
		__xlogAppenderDestroy(pLogger->arrAppender[i]);
	}

	if ( pLogger->arrAppender ) {
		xrtFree(pLogger->arrAppender);
	}
	if ( __xlogOwnStr(pLogger->sName) ) {
		xrtFree(pLogger->sName);
	}
	if ( pLogger->pLock ) {
		xrtMutexDestroy(pLogger->pLock);
	}
	xrtFree(pLogger);
}


// 释放日志器引用
XXAPI void xlogRelease(xlogger* pLogger)
{
	if ( !pLogger ) {
		return;
	}

	/* 默认日志器始终持有一个引用，普通调用者不能误释放这个保底引用。 */
	if ( pLogger == __g_pXlogDefault && pLogger->iRefCount <= 1 ) {
		return;
	}
	if ( xrtAtomicRefRelease(&pLogger->iRefCount) == 0 ) {
		__xlogDestroy(pLogger);
	}
}


// 销毁日志器，等价于释放创建者持有的引用
XXAPI void xlogDestroy(xlogger* pLogger)
{
	xlogRelease(pLogger);
}


// 获取默认日志器
XXAPI xlogger* xlogDefault()
{
	if ( __g_pXlogDefault == NULL ) {
		__g_pXlogDefault = xlogCreate((str)"default");
		if ( __g_pXlogDefault ) {
			xlogAddConsole(__g_pXlogDefault, XLOG_TRACE, TRUE);
		}
	}

	return __g_pXlogDefault;
}


// 设置默认日志器
XXAPI void xlogSetDefault(xlogger* pLogger)
{
	xlogger* pOld;

	if ( __g_pXlogDefault == pLogger ) {
		return;
	}
	if ( pLogger ) {
		xlogAddRef(pLogger);
	}
	pOld = __g_pXlogDefault;
	__g_pXlogDefault = pLogger;
	if ( pOld ) {
		xlogRelease(pOld);
	}
}


// 内部函数：释放运行时持有的默认日志器
static void __xlogRuntimeUnit()
{
	xlogger* pLogger = __g_pXlogDefault;
	__g_pXlogDefault = NULL;
	if ( pLogger ) {
		xlogRelease(pLogger);
	}
}


// 设置日志器级别
XXAPI void xlogSetLevel(xlogger* pLogger, xloglevel iLevel)
{
	if ( pLogger ) {
		xrtMutexLock(pLogger->pLock);
		pLogger->iLevel = iLevel;
		xrtMutexUnlock(pLogger->pLock);
	}
}


// 获取日志器级别
XXAPI xloglevel xlogGetLevel(xlogger* pLogger)
{
	xloglevel iLevel;
	if ( !pLogger ) {
		return XLOG_OFF;
	}
	xrtMutexLock(pLogger->pLock);
	iLevel = pLogger->iLevel;
	xrtMutexUnlock(pLogger->pLock);
	return iLevel;
}


// 添加控制台输出器
XXAPI xlogappender* xlogAddConsole(xlogger* pLogger, xloglevel iMinLevel, bool bColor)
{
	xlogappender* pAppender;

	if ( !pLogger ) {
		return NULL;
	}

	pAppender = __xlogAppenderCreate((str)"console", XLOG_APPENDER_CONSOLE, iMinLevel);
	if ( !pAppender ) {
		return NULL;
	}

	pAppender->bColor = bColor;
	pAppender->pFile = stdout;
	if ( !__xlogAddAppender(pLogger, pAppender) ) {
		__xlogAppenderDestroy(pAppender);
		return NULL;
	}
	return pAppender;
}


// 添加文件输出器
XXAPI xlogappender* xlogAddFile(xlogger* pLogger, str sPath, xloglevel iMinLevel)
{
	xlogappender* pAppender;

	if ( !pLogger || !sPath || sPath[0] == '\0' ) {
		return NULL;
	}

	pAppender = __xlogAppenderCreate((str)"file", XLOG_APPENDER_FILE, iMinLevel);
	if ( !pAppender ) {
		return NULL;
	}

	pAppender->sPath = xrtCopyStr(sPath, 0);
	pAppender->pFile = __xlogOpenAppendFile(sPath);
	pAppender->bOwnFile = TRUE;
	if ( !pAppender->pFile ) {
		__xlogAppenderDestroy(pAppender);
		xrtSetError("logger open file failed.", FALSE);
		return NULL;
	}

	if ( !__xlogAddAppender(pLogger, pAppender) ) {
		__xlogAppenderDestroy(pAppender);
		return NULL;
	}
	return pAppender;
}


// 添加滚动文件输出器
XXAPI xlogappender* xlogAddRollingFile(xlogger* pLogger, str sPath, uint64 iMaxSize, uint32 iMaxBackup, xloglevel iMinLevel)
{
	xlogappender* pAppender;

	pAppender = xlogAddFile(pLogger, sPath, iMinLevel);
	if ( !pAppender ) {
		return NULL;
	}

	xrtMutexLock(pLogger->pLock);
	pAppender->iMaxSize = iMaxSize;
	pAppender->iMaxBackup = iMaxBackup;
	xrtMutexUnlock(pLogger->pLock);
	return pAppender;
}


// 添加自定义输出器
XXAPI xlogappender* xlogAddCustom(xlogger* pLogger, str sName, xloglevel iMinLevel, xlogcustomproc Proc, ptr pUserData)
{
	return xlogAddCustomEx(pLogger, sName, iMinLevel, Proc, pUserData, NULL);
}


// 添加带用户数据析构回调的自定义输出器
XXAPI xlogappender* xlogAddCustomEx(xlogger* pLogger, str sName, xloglevel iMinLevel, xlogcustomproc Proc, ptr pUserData, xlogfreeproc FreeUserData)
{
	xlogappender* pAppender;

	if ( !pLogger || !Proc ) {
		return NULL;
	}

	pAppender = __xlogAppenderCreate(sName ? sName : (str)"custom", XLOG_APPENDER_CUSTOM, iMinLevel);
	if ( !pAppender ) {
		return NULL;
	}

	pAppender->Proc = Proc;
	pAppender->pUserData = pUserData;
	pAppender->FreeUserData = FreeUserData;
	if ( !__xlogAddAppender(pLogger, pAppender) ) {
		/* 注册失败时所有权仍属于调用者。 */
		pAppender->FreeUserData = NULL;
		__xlogAppenderDestroy(pAppender);
		return NULL;
	}
	return pAppender;
}


// 注销输出器，并等待已经进入的自定义回调结束
XXAPI bool xlogRemoveAppender(xlogger* pLogger, xlogappender* pAppender)
{
	bool bFound = FALSE;

	if ( !pLogger || !pAppender ) {
		return FALSE;
	}

	xrtMutexLock(pLogger->pLock);
	for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
		if ( pLogger->arrAppender[i] != pAppender ) {
			continue;
		}
		if ( i + 1 < pLogger->iAppenderCount ) {
			memmove(
				pLogger->arrAppender + i,
				pLogger->arrAppender + i + 1,
				sizeof(xlogappender*) * (pLogger->iAppenderCount - i - 1));
		}
		pLogger->iAppenderCount--;
		pLogger->arrAppender[pLogger->iAppenderCount] = NULL;
		pAppender->pOwner = NULL;
		bFound = TRUE;
		break;
	}
	xrtMutexUnlock(pLogger->pLock);

	if ( !bFound ) {
		return FALSE;
	}

	/*
		回调在 logger 锁外执行。注销先阻止新回调进入，再等待活动回调退出，
		保证函数返回后用户数据和回调地址都不再被 XRT 使用。
		回调本身不得同步注销正在执行的同一个 appender。
	*/
	while ( pAppender->iActiveCallbacks > 0 ) {
		xrtThreadYield();
	}
	__xlogAppenderDestroy(pAppender);
	return TRUE;
}


// 设置输出器级别
XXAPI void xlogAppenderSetLevel(xlogappender* pAppender, xloglevel iMinLevel)
{
	if ( pAppender ) {
		if ( pAppender->pOwner ) {
			xrtMutexLock(pAppender->pOwner->pLock);
		}
		pAppender->iMinLevel = iMinLevel;
		if ( pAppender->pOwner ) {
			xrtMutexUnlock(pAppender->pOwner->pLock);
		}
	}
}


// 设置输出器格式
XXAPI void xlogAppenderSetFormat(xlogappender* pAppender, xlogformat iFormat)
{
	if ( pAppender ) {
		if ( pAppender->pOwner ) {
			xrtMutexLock(pAppender->pOwner->pLock);
		}
		pAppender->iFormat = iFormat;
		if ( pAppender->pOwner ) {
			xrtMutexUnlock(pAppender->pOwner->pLock);
		}
	}
}


// 设置输出器颜色
XXAPI void xlogAppenderSetColor(xlogappender* pAppender, bool bColor)
{
	if ( pAppender ) {
		if ( pAppender->pOwner ) {
			xrtMutexLock(pAppender->pOwner->pLock);
		}
		pAppender->bColor = bColor;
		if ( pAppender->pOwner ) {
			xrtMutexUnlock(pAppender->pOwner->pLock);
		}
	}
}


// 获取输出器名称
XXAPI str xlogAppenderName(const xlogappender* pAppender)
{
	return pAppender && pAppender->sName ? pAppender->sName : xCore.sNull;
}


// 获取输出器最低级别
XXAPI xloglevel xlogAppenderGetLevel(const xlogappender* pAppender)
{
	xloglevel iLevel;
	if ( !pAppender ) {
		return XLOG_OFF;
	}
	if ( pAppender->pOwner ) {
		xrtMutexLock(pAppender->pOwner->pLock);
	}
	iLevel = pAppender->iMinLevel;
	if ( pAppender->pOwner ) {
		xrtMutexUnlock(pAppender->pOwner->pLock);
	}
	return iLevel;
}


// 获取输出器格式
XXAPI xlogformat xlogAppenderGetFormat(const xlogappender* pAppender)
{
	xlogformat iFormat;
	if ( !pAppender ) {
		return XLOG_FORMAT_TEXT;
	}
	if ( pAppender->pOwner ) {
		xrtMutexLock(pAppender->pOwner->pLock);
	}
	iFormat = pAppender->iFormat;
	if ( pAppender->pOwner ) {
		xrtMutexUnlock(pAppender->pOwner->pLock);
	}
	return iFormat;
}


// 获取输出器颜色开关
XXAPI bool xlogAppenderGetColor(const xlogappender* pAppender)
{
	bool bColor;
	if ( !pAppender ) {
		return FALSE;
	}
	if ( pAppender->pOwner ) {
		xrtMutexLock(pAppender->pOwner->pLock);
	}
	bColor = pAppender->bColor;
	if ( pAppender->pOwner ) {
		xrtMutexUnlock(pAppender->pOwner->pLock);
	}
	return bColor;
}


// 获取日志级别名称
XXAPI str xlogLevelName(xloglevel iLevel)
{
	switch ( iLevel ) {
		case XLOG_TRACE: return (str)"TRACE";
		case XLOG_DEBUG: return (str)"DEBUG";
		case XLOG_INFO: return (str)"INFO";
		case XLOG_WARN: return (str)"WARN";
		case XLOG_ERROR: return (str)"ERROR";
		case XLOG_FATAL: return (str)"FATAL";
		case XLOG_OFF: return (str)"OFF";
		default: return (str)"UNKN";
	}
}


// 核心日志输出
XXAPI void xlogWrite(xlogger* pLogger, xloglevel iLevel, const char* sFile, uint32 iLine, const char* sFunc, const char* sFmt, ...)
{
	va_list args;

	va_start(args, sFmt);
	xlogWriteV(pLogger, iLevel, sFile, iLine, sFunc, sFmt, args);
	va_end(args);
}


// 核心日志输出 va_list 版本
XXAPI void xlogWriteV(xlogger* pLogger, xloglevel iLevel, const char* sFile, uint32 iLine, const char* sFunc, const char* sFmt, va_list args)
{
	va_list argsCopy;
	int iSize;
	str sMessage;
	xlogevent tEvent;

	if ( !pLogger || !sFmt || iLevel < xlogGetLevel(pLogger) || iLevel >= XLOG_OFF ) {
		return;
	}

	va_copy(argsCopy, args);
	iSize = vsnprintf(NULL, 0, sFmt, argsCopy);
	va_end(argsCopy);
	if ( iSize < 0 ) {
		return;
	}

	sMessage = (str)xrtMalloc((size_t)iSize + 1u);
	if ( !sMessage ) {
		return;
	}

	vsnprintf((char*)sMessage, (size_t)iSize + 1u, sFmt, args);

	memset(&tEvent, 0, sizeof(tEvent));
	tEvent.iTime = xrtNow();
	tEvent.iLevel = iLevel;
	tEvent.sLogger = __xrt_cstr(pLogger->sName);
	tEvent.sFile = sFile;
	tEvent.iLine = iLine;
	tEvent.sFunc = sFunc;
	tEvent.iThreadId = xrtThreadGetCurrentId();
	tEvent.sMessage = (const char*)sMessage;

	xrtMutexLock(pLogger->pLock);
	for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
		xlogappender* pAppender = pLogger->arrAppender[i];
		if ( !pAppender || pAppender->iType == XLOG_APPENDER_CUSTOM || iLevel < pAppender->iMinLevel ) {
			continue;
		}

		if ( pAppender->iType == XLOG_APPENDER_CONSOLE ) {
			FILE* pFile = (iLevel >= XLOG_ERROR) ? stderr : stdout;
			__xlogWriteFormatted(pFile, pAppender->iFormat, pAppender->bColor, &tEvent);
			fflush(pFile);
			continue;
		}

		if ( pAppender->iType == XLOG_APPENDER_FILE && pAppender->pFile ) {
			if ( __xlogRotateIfNeeded(pAppender, (size_t)iSize + 160u) ) {
				__xlogWriteFormatted(pAppender->pFile, pAppender->iFormat, FALSE, &tEvent);
				fflush(pAppender->pFile);
			}
		}
	}
	xrtMutexUnlock(pLogger->pLock);

	/*
		用户回调必须在锁外执行。先在锁内建立活动快照，既允许递归记录，
		也让 xlogRemoveAppender 能可靠等待已经进入的回调完成。
	*/
	{
		xlogappender** arrCustom = NULL;
		uint32 iCustomCount = 0;
		uint32 iCustomIndex = 0;

		xrtMutexLock(pLogger->pLock);
		for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
			xlogappender* pAppender = pLogger->arrAppender[i];
			if ( pAppender
				&& pAppender->iType == XLOG_APPENDER_CUSTOM
				&& pAppender->Proc
				&& iLevel >= pAppender->iMinLevel ) {
				iCustomCount++;
			}
		}
		if ( iCustomCount > 0 ) {
			arrCustom = (xlogappender**)xrtCalloc(iCustomCount, sizeof(xlogappender*));
		}
		if ( arrCustom ) {
			for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
				xlogappender* pAppender = pLogger->arrAppender[i];
				if ( pAppender
					&& pAppender->iType == XLOG_APPENDER_CUSTOM
					&& pAppender->Proc
					&& iLevel >= pAppender->iMinLevel ) {
					xrtAtomicRefRetain(&pAppender->iActiveCallbacks);
					arrCustom[iCustomIndex++] = pAppender;
				}
			}
		}
		xrtMutexUnlock(pLogger->pLock);

		for ( uint32 i = 0; i < iCustomIndex; i++ ) {
			xlogappender* pAppender = arrCustom[i];
			pAppender->Proc(&tEvent, pAppender->pUserData);
			xrtAtomicRefRelease(&pAppender->iActiveCallbacks);
		}
		xrtFree(arrCustom);
	}

	xrtFree(sMessage);
}


// 刷新日志器
XXAPI void xlogFlush(xlogger* pLogger)
{
	if ( !pLogger ) {
		return;
	}

	xrtMutexLock(pLogger->pLock);
	for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
		xlogappender* pAppender = pLogger->arrAppender[i];
		if ( pAppender && pAppender->pFile ) {
			fflush(pAppender->pFile);
		}
	}
	xrtMutexUnlock(pLogger->pLock);
}
