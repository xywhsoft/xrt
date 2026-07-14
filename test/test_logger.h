


// Logger 库测试

typedef struct {
	int iCount;
	xloglevel iLastLevel;
	char sLastMessage[128];
} xrt_logger_test_custom;

typedef struct {
	xlogger* pLogger;
	int iCount;
	bool bNested;
} xrt_logger_test_recursive;

static int __g_xrtLoggerTestFreeCount = 0;


// 内部函数：__xrtLoggerTestCustomProc
static void __xrtLoggerTestCustomProc(const xlogevent* pEvent, ptr pUserData)
{
	xrt_logger_test_custom* pState = (xrt_logger_test_custom*)pUserData;

	if ( !pEvent || !pState ) {
		return;
	}

	pState->iCount++;
	pState->iLastLevel = pEvent->iLevel;
	snprintf(pState->sLastMessage, sizeof(pState->sLastMessage), "%s", pEvent->sMessage ? pEvent->sMessage : "");
}


// 内部函数：__xrtLoggerTestReadFile
static str __xrtLoggerTestReadFile(str sPath)
{
	str sText;

	sText = xrtFileReadAll(sPath, XRT_CP_UTF8, NULL);
	return sText ? sText : xCore.sNull;
}


// 验证自定义输出器可以安全地递归写日志
static void __xrtLoggerTestRecursiveProc(const xlogevent* pEvent, ptr pUserData)
{
	xrt_logger_test_recursive* pState = (xrt_logger_test_recursive*)pUserData;

	if ( !pEvent || !pState ) {
		return;
	}
	pState->iCount++;
	if ( !pState->bNested ) {
		pState->bNested = TRUE;
		xloggerInfo(pState->pLogger, "nested log");
	}
}


static void __xrtLoggerTestFreeCustom(ptr pUserData)
{
	__g_xrtLoggerTestFreeCount++;
	xrtFree(pUserData);
}


// 内部函数：__xrtLoggerTestContains
static bool __xrtLoggerTestContains(str sText, const char* sNeedle)
{
	if ( !sText || sText == xCore.sNull || !sNeedle ) {
		return FALSE;
	}
	return strstr((const char*)sText, sNeedle) != NULL;
}


// 内部函数：__xrtLoggerTestOwnStr
static bool __xrtLoggerTestOwnStr(str sText)
{
	return sText && sText != xCore.sNull;
}


// Logger 核心测试
int Test_Logger()
{
	xlogger* pLogger;
	xlogappender* pFileAppender;
	xlogappender* pJsonAppender;
	xlogappender* pRollingAppender;
	xrt_logger_test_custom tCustom;
	str sText;
	str sDir;
	str sBasicPath;
	str sJsonPath;
	str sRollPath;
	str sRollPath1;
	str sRollPath2;
	str sUtf8Path;
	int iRet;

	sDir = NULL;
	sBasicPath = NULL;
	sJsonPath = NULL;
	sRollPath = NULL;
	sRollPath1 = NULL;
	sRollPath2 = NULL;
	sUtf8Path = NULL;
	iRet = 0;

	sDir = xrtPathJoin(2, xCore.AppPath, "test");
	sBasicPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_basic.log");
	sJsonPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_json.log");
	sRollPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log");
	sRollPath1 = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log.1");
	sRollPath2 = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log.2");
	sUtf8Path = xrtPathJoin(3, xCore.AppPath, "test", "日志_utf8.log");

	if ( !sDir || !sBasicPath || !sJsonPath || !sRollPath || !sRollPath1 || !sRollPath2 || !sUtf8Path ) {
		printf("Logger Test : path build failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtDirCreateAll(sDir) ) {
		printf("Logger Test : create output directory failed\n");
		iRet = 1;
		goto cleanup;
	}

	xrtFileDelete(sBasicPath);
	xrtFileDelete(sJsonPath);
	xrtFileDelete(sRollPath);
	xrtFileDelete(sRollPath1);
	xrtFileDelete(sRollPath2);
	xrtFileDelete(sUtf8Path);

	memset(&tCustom, 0, sizeof(tCustom));

	pLogger = xlogCreate((str)"logger_test");
	if ( !pLogger ) {
		printf("Logger Test : create failed\n");
		iRet = 1;
		goto cleanup;
	}

	xlogSetLevel(pLogger, XLOG_TRACE);

	pFileAppender = xlogAddFile(pLogger, sBasicPath, XLOG_TRACE);
	if ( !pFileAppender ) {
		printf("Logger Test : add file appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		iRet = 2;
		goto cleanup;
	}

	pJsonAppender = xlogAddFile(pLogger, sJsonPath, XLOG_INFO);
	if ( !pJsonAppender ) {
		printf("Logger Test : add json appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		iRet = 3;
		goto cleanup;
	}
	xlogAppenderSetFormat(pJsonAppender, XLOG_FORMAT_JSON);
	if ( strcmp((const char*)xlogAppenderName(pJsonAppender), "file") != 0
		|| xlogAppenderGetLevel(pJsonAppender) != XLOG_INFO
		|| xlogAppenderGetFormat(pJsonAppender) != XLOG_FORMAT_JSON
		|| xlogAppenderGetColor(pJsonAppender) ) {
		printf("Logger Test : appender getters failed\n");
		xlogDestroy(pLogger);
		iRet = 4;
		goto cleanup;
	}

	xlogAddCustom(pLogger, (str)"custom", XLOG_WARN, __xrtLoggerTestCustomProc, &tCustom);

	xloggerDebug(pLogger, "debug value=%d", 7);
	xloggerInfo(pLogger, "hello %s", "logger");
	xloggerWarn(pLogger, "warn message");
	xlogFlush(pLogger);

	if ( tCustom.iCount != 1 || tCustom.iLastLevel != XLOG_WARN || strcmp(tCustom.sLastMessage, "warn message") != 0 ) {
		printf("Logger Test : custom appender failed\n");
		xlogDestroy(pLogger);
		iRet = 4;
		goto cleanup;
	}
	xlogDestroy(pLogger);
	pLogger = NULL;

	sText = __xrtLoggerTestReadFile(sBasicPath);
	if ( !__xrtLoggerTestContains(sText, "debug value=7") || !__xrtLoggerTestContains(sText, "hello logger") || !__xrtLoggerTestContains(sText, "warn message") ) {
		printf("Logger Test : text file content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		iRet = 5;
		goto cleanup;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}

	sText = __xrtLoggerTestReadFile(sJsonPath);
	if ( __xrtLoggerTestContains(sText, "debug value=7") || !__xrtLoggerTestContains(sText, "\"level\":\"INFO\"") || !__xrtLoggerTestContains(sText, "\"msg\":\"hello logger\"") ) {
		printf("Logger Test : json file content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		iRet = 6;
		goto cleanup;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}
	pLogger = xlogCreate((str)"rolling_test");
	if ( !pLogger ) {
		iRet = 7;
		goto cleanup;
	}
	xlogSetLevel(pLogger, XLOG_TRACE);
	pRollingAppender = xlogAddRollingFile(pLogger, sRollPath, 240, 2, XLOG_TRACE);
	if ( !pRollingAppender ) {
		printf("Logger Test : add rolling file appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		iRet = 8;
		goto cleanup;
	}
	xlogAppenderSetFormat(pRollingAppender, XLOG_FORMAT_SIMPLE);

	for ( int i = 0; i < 20; i++ ) {
		xloggerInfo(pLogger, "rolling line %02d abcdefghijklmnopqrstuvwxyz", i);
	}
	xlogFlush(pLogger);
	xlogDestroy(pLogger);

	if ( !xrtFileExists(sRollPath) || !xrtFileExists(sRollPath1) ) {
		printf("Logger Test : rolling file missing\n");
		iRet = 9;
		goto cleanup;
	}

	pLogger = xlogCreate((str)"utf8_path_test");
	if ( !pLogger ) {
		iRet = 10;
		goto cleanup;
	}
	pFileAppender = xlogAddFile(pLogger, sUtf8Path, XLOG_TRACE);
	if ( !pFileAppender ) {
		printf("Logger Test : add utf8 file appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		iRet = 11;
		goto cleanup;
	}
	xloggerInfo(pLogger, "utf8 path ok");
	xlogFlush(pLogger);
	xlogDestroy(pLogger);

	sText = __xrtLoggerTestReadFile(sUtf8Path);
	if ( !__xrtLoggerTestContains(sText, "utf8 path ok") ) {
		printf("Logger Test : utf8 path content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		iRet = 12;
		goto cleanup;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}

	/* 默认日志器和自定义输出器都必须拥有明确、可组合的生命周期。 */
	{
		xrt_logger_test_custom* pOwnedCustom = (xrt_logger_test_custom*)xrtCalloc(1, sizeof(*pOwnedCustom));
		xlogger* pDefault = xlogCreate((str)"logger_default_ref");
		__g_xrtLoggerTestFreeCount = 0;
		if ( !pOwnedCustom || !pDefault ||
		     !xlogAddCustomEx(pDefault, (str)"owned", XLOG_TRACE, __xrtLoggerTestCustomProc, pOwnedCustom, __xrtLoggerTestFreeCustom) ) {
			xrtFree(pOwnedCustom);
			xlogDestroy(pDefault);
			iRet = 13;
			goto cleanup;
		}
		xlogSetDefault(pDefault);
		xlogDestroy(pDefault);
		xlogInfo("default retained");
		xlogSetDefault(NULL);
		if ( __g_xrtLoggerTestFreeCount != 1 ) {
			printf("Logger Test : owned custom/default lifetime failed\n");
			iRet = 14;
			goto cleanup;
		}
	}

	/* 自定义回调在日志器锁外执行，递归记录日志不得死锁。 */
	{
		xrt_logger_test_recursive tRecursive;
		xlogappender* pRecursiveAppender;
		memset(&tRecursive, 0, sizeof(tRecursive));
		pLogger = xlogCreate((str)"logger_recursive");
		if ( !pLogger ) {
			iRet = 15;
			goto cleanup;
		}
		tRecursive.pLogger = pLogger;
		pRecursiveAppender = xlogAddCustom(pLogger, (str)"recursive", XLOG_TRACE,
			__xrtLoggerTestRecursiveProc, &tRecursive);
		if ( !pRecursiveAppender ) {
			xlogDestroy(pLogger);
			iRet = 16;
			goto cleanup;
		}
		xloggerInfo(pLogger, "outer log");
		xlogDestroy(pLogger);
		pLogger = NULL;
		if ( tRecursive.iCount != 2 ) {
			printf("Logger Test : recursive custom appender failed\n");
			iRet = 17;
			goto cleanup;
		}
	}

	/* 注销后不得继续回调，并且 appender 用户数据必须立即释放。 */
	{
		xrt_logger_test_custom* pRemoved = (xrt_logger_test_custom*)xrtCalloc(1, sizeof(*pRemoved));
		xlogappender* pRemovedAppender;
		pLogger = xlogCreate((str)"logger_remove");
		__g_xrtLoggerTestFreeCount = 0;
		if ( !pLogger || !pRemoved ) {
			xrtFree(pRemoved);
			xlogDestroy(pLogger);
			iRet = 18;
			goto cleanup;
		}
		pRemovedAppender = xlogAddCustomEx(
			pLogger,
			(str)"removed",
			XLOG_TRACE,
			__xrtLoggerTestCustomProc,
			pRemoved,
			__xrtLoggerTestFreeCustom);
		if ( !pRemovedAppender ) {
			xrtFree(pRemoved);
			xlogDestroy(pLogger);
			iRet = 19;
			goto cleanup;
		}
		xloggerInfo(pLogger, "before remove");
		if ( pRemoved->iCount != 1 || !xlogRemoveAppender(pLogger, pRemovedAppender) ) {
			xlogDestroy(pLogger);
			iRet = 20;
			goto cleanup;
		}
		xloggerInfo(pLogger, "after remove");
		xlogDestroy(pLogger);
		pLogger = NULL;
		if ( __g_xrtLoggerTestFreeCount != 1 ) {
			printf("Logger Test : remove appender lifetime failed\n");
			iRet = 21;
			goto cleanup;
		}
	}

	printf("Logger Test : PASS\n");

cleanup:
	if ( sDir && sDir != xCore.sNull ) {
		xrtFree(sDir);
	}
	if ( sBasicPath && sBasicPath != xCore.sNull ) {
		xrtFree(sBasicPath);
	}
	if ( sJsonPath && sJsonPath != xCore.sNull ) {
		xrtFree(sJsonPath);
	}
	if ( sRollPath && sRollPath != xCore.sNull ) {
		xrtFree(sRollPath);
	}
	if ( sRollPath1 && sRollPath1 != xCore.sNull ) {
		xrtFree(sRollPath1);
	}
	if ( sRollPath2 && sRollPath2 != xCore.sNull ) {
		xrtFree(sRollPath2);
	}
	if ( sUtf8Path && sUtf8Path != xCore.sNull ) {
		xrtFree(sUtf8Path);
	}
	return iRet;
}
