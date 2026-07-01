


// Logger 库测试

typedef struct {
	int iCount;
	xloglevel iLastLevel;
	char sLastMessage[128];
} xrt_logger_test_custom;


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
	FILE* pFile;
	long iSize;
	str sText;

	pFile = fopen((const char*)sPath, "rb");
	if ( !pFile ) {
		return xCore.sNull;
	}

	fseek(pFile, 0, SEEK_END);
	iSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	if ( iSize <= 0 ) {
		fclose(pFile);
		return xCore.sNull;
	}

	sText = (str)xrtMalloc((size_t)iSize + 1u);
	if ( !sText ) {
		fclose(pFile);
		return xCore.sNull;
	}

	if ( fread(sText, 1, (size_t)iSize, pFile) != (size_t)iSize ) {
		fclose(pFile);
		xrtFree(sText);
		return xCore.sNull;
	}
	fclose(pFile);
	sText[iSize] = 0;
	return sText;
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
	int iRet;

	sDir = NULL;
	sBasicPath = NULL;
	sJsonPath = NULL;
	sRollPath = NULL;
	sRollPath1 = NULL;
	sRollPath2 = NULL;
	iRet = 0;

	sDir = xrtPathJoin(2, xCore.AppPath, "test");
	sBasicPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_basic.log");
	sJsonPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_json.log");
	sRollPath = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log");
	sRollPath1 = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log.1");
	sRollPath2 = xrtPathJoin(3, xCore.AppPath, "test", "logger_roll.log.2");

	if ( !sDir || !sBasicPath || !sJsonPath || !sRollPath || !sRollPath1 || !sRollPath2 ) {
		printf("Logger Test : path build failed\n");
		iRet = 1;
		goto cleanup;
	}
	if ( !xrtDirCreateAll(sDir) ) {
		printf("Logger Test : create output directory failed\n");
		iRet = 1;
		goto cleanup;
	}

	remove((const char*)sBasicPath);
	remove((const char*)sJsonPath);
	remove((const char*)sRollPath);
	remove((const char*)sRollPath1);
	remove((const char*)sRollPath2);

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

	sText = __xrtLoggerTestReadFile(sBasicPath);
	if ( !__xrtLoggerTestContains(sText, "debug value=7") || !__xrtLoggerTestContains(sText, "hello logger") || !__xrtLoggerTestContains(sText, "warn message") ) {
		printf("Logger Test : text file content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		xlogDestroy(pLogger);
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
		xlogDestroy(pLogger);
		iRet = 6;
		goto cleanup;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}

	xlogDestroy(pLogger);

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
	return iRet;
}
