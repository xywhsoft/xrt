


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
};

struct xlogger {
	str sName;
	xloglevel iLevel;
	xmutex pLock;
	xlogappender** arrAppender;
	uint32 iAppenderCount;
	uint32 iAppenderCapacity;
};

static xlogger* __g_pXlogDefault = NULL;
static bool __g_bXlogDefaultOwner = FALSE;


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


// 内部函数：格式化当前本地时间
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
	snprintf(sBuff, iSize, "%04d-%02d-%02d %02d:%02d:%02d",
		tLocal.tm_year + 1900,
		tLocal.tm_mon + 1,
		tLocal.tm_mday,
		tLocal.tm_hour,
		tLocal.tm_min,
		tLocal.tm_sec);
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

	sLevel = xlogLevelName(pEvent->iLevel);
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
		remove((const char*)sDst);
		xrtFree(sDst);
	}

	for ( uint32 i = pAppender->iMaxBackup; i > 1; i-- ) {
		sSrc = __xlogMakeBackupPath(pAppender->sPath, i - 1);
		sDst = __xlogMakeBackupPath(pAppender->sPath, i);
		if ( __xlogOwnStr(sSrc) && __xlogOwnStr(sDst) ) {
			remove((const char*)sDst);
			rename((const char*)sSrc, (const char*)sDst);
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
		remove((const char*)sDst);
		rename((const char*)pAppender->sPath, (const char*)sDst);
		xrtFree(sDst);
	}

	pAppender->pFile = fopen((const char*)pAppender->sPath, "ab");
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

	if ( pLogger->iAppenderCount >= pLogger->iAppenderCapacity ) {
		iCapacity = pLogger->iAppenderCapacity == 0 ? 4 : pLogger->iAppenderCapacity * 2;
		arrNew = (xlogappender**)xrtRealloc(pLogger->arrAppender, sizeof(xlogappender*) * iCapacity);
		if ( !arrNew ) {
			return FALSE;
		}
		pLogger->arrAppender = arrNew;
		pLogger->iAppenderCapacity = iCapacity;
	}

	pLogger->arrAppender[pLogger->iAppenderCount++] = pAppender;
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


// 销毁日志器
XXAPI void xlogDestroy(xlogger* pLogger)
{
	if ( !pLogger ) {
		return;
	}

	xlogFlush(pLogger);

	if ( pLogger == __g_pXlogDefault ) {
		__g_pXlogDefault = NULL;
		__g_bXlogDefaultOwner = FALSE;
	}

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


// 获取默认日志器
XXAPI xlogger* xlogDefault()
{
	if ( __g_pXlogDefault == NULL ) {
		__g_pXlogDefault = xlogCreate((str)"default");
		if ( __g_pXlogDefault ) {
			__g_bXlogDefaultOwner = TRUE;
			xlogAddConsole(__g_pXlogDefault, XLOG_TRACE, TRUE);
		}
	}

	return __g_pXlogDefault;
}


// 设置默认日志器
XXAPI void xlogSetDefault(xlogger* pLogger)
{
	if ( __g_bXlogDefaultOwner && __g_pXlogDefault && __g_pXlogDefault != pLogger ) {
		xlogDestroy(__g_pXlogDefault);
	}
	__g_pXlogDefault = pLogger;
	__g_bXlogDefaultOwner = FALSE;
}


// 内部函数：释放运行时持有的默认日志器
static void __xlogRuntimeUnit()
{
	if ( __g_bXlogDefaultOwner && __g_pXlogDefault ) {
		xlogDestroy(__g_pXlogDefault);
	}
	__g_pXlogDefault = NULL;
	__g_bXlogDefaultOwner = FALSE;
}


// 设置日志器级别
XXAPI void xlogSetLevel(xlogger* pLogger, xloglevel iLevel)
{
	if ( pLogger ) {
		pLogger->iLevel = iLevel;
	}
}


// 获取日志器级别
XXAPI xloglevel xlogGetLevel(xlogger* pLogger)
{
	return pLogger ? pLogger->iLevel : XLOG_OFF;
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
	pAppender->pFile = fopen((const char*)sPath, "ab");
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

	pAppender->iMaxSize = iMaxSize;
	pAppender->iMaxBackup = iMaxBackup;
	return pAppender;
}


// 添加自定义输出器
XXAPI xlogappender* xlogAddCustom(xlogger* pLogger, str sName, xloglevel iMinLevel, xlogcustomproc Proc, ptr pUserData)
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
	if ( !__xlogAddAppender(pLogger, pAppender) ) {
		__xlogAppenderDestroy(pAppender);
		return NULL;
	}
	return pAppender;
}


// 设置输出器级别
XXAPI void xlogAppenderSetLevel(xlogappender* pAppender, xloglevel iMinLevel)
{
	if ( pAppender ) {
		pAppender->iMinLevel = iMinLevel;
	}
}


// 设置输出器格式
XXAPI void xlogAppenderSetFormat(xlogappender* pAppender, xlogformat iFormat)
{
	if ( pAppender ) {
		pAppender->iFormat = iFormat;
	}
}


// 设置输出器颜色
XXAPI void xlogAppenderSetColor(xlogappender* pAppender, bool bColor)
{
	if ( pAppender ) {
		pAppender->bColor = bColor;
	}
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

	if ( !pLogger || !sFmt || iLevel < pLogger->iLevel || iLevel >= XLOG_OFF ) {
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
	tEvent.sLogger = pLogger->sName;
	tEvent.sFile = sFile;
	tEvent.iLine = iLine;
	tEvent.sFunc = sFunc;
	tEvent.iThreadId = xrtThreadGetCurrentId();
	tEvent.sMessage = (const char*)sMessage;

	xrtMutexLock(pLogger->pLock);
	for ( uint32 i = 0; i < pLogger->iAppenderCount; i++ ) {
		xlogappender* pAppender = pLogger->arrAppender[i];
		if ( !pAppender || iLevel < pAppender->iMinLevel ) {
			continue;
		}

		if ( pAppender->iType == XLOG_APPENDER_CUSTOM ) {
			pAppender->Proc(&tEvent, pAppender->pUserData);
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
