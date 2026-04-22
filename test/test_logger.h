


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

	remove("logger_basic.log");
	remove("logger_json.log");
	remove("logger_roll.log");
	remove("logger_roll.log.1");
	remove("logger_roll.log.2");

	memset(&tCustom, 0, sizeof(tCustom));

	pLogger = xlogCreate((str)"logger_test");
	if ( !pLogger ) {
		printf("Logger Test : create failed\n");
		return 1;
	}

	xlogSetLevel(pLogger, XLOG_TRACE);

	pFileAppender = xlogAddFile(pLogger, (str)"logger_basic.log", XLOG_TRACE);
	if ( !pFileAppender ) {
		printf("Logger Test : add file appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		return 2;
	}

	pJsonAppender = xlogAddFile(pLogger, (str)"logger_json.log", XLOG_INFO);
	if ( !pJsonAppender ) {
		printf("Logger Test : add json appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		return 3;
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
		return 4;
	}

	sText = __xrtLoggerTestReadFile((str)"logger_basic.log");
	if ( !__xrtLoggerTestContains(sText, "debug value=7") || !__xrtLoggerTestContains(sText, "hello logger") || !__xrtLoggerTestContains(sText, "warn message") ) {
		printf("Logger Test : text file content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		xlogDestroy(pLogger);
		return 5;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}

	sText = __xrtLoggerTestReadFile((str)"logger_json.log");
	if ( __xrtLoggerTestContains(sText, "debug value=7") || !__xrtLoggerTestContains(sText, "\"level\":\"INFO\"") || !__xrtLoggerTestContains(sText, "\"msg\":\"hello logger\"") ) {
		printf("Logger Test : json file content failed\n");
		if ( __xrtLoggerTestOwnStr(sText) ) {
			xrtFree(sText);
		}
		xlogDestroy(pLogger);
		return 6;
	}
	if ( __xrtLoggerTestOwnStr(sText) ) {
		xrtFree(sText);
	}

	xlogDestroy(pLogger);

	pLogger = xlogCreate((str)"rolling_test");
	if ( !pLogger ) {
		return 7;
	}
	xlogSetLevel(pLogger, XLOG_TRACE);
	pRollingAppender = xlogAddRollingFile(pLogger, (str)"logger_roll.log", 240, 2, XLOG_TRACE);
	if ( !pRollingAppender ) {
		printf("Logger Test : add rolling file appender failed: %s\n", xrtGetError());
		xlogDestroy(pLogger);
		return 8;
	}
	xlogAppenderSetFormat(pRollingAppender, XLOG_FORMAT_SIMPLE);

	for ( int i = 0; i < 20; i++ ) {
		xloggerInfo(pLogger, "rolling line %02d abcdefghijklmnopqrstuvwxyz", i);
	}
	xlogFlush(pLogger);
	xlogDestroy(pLogger);

	if ( !xrtFileExists((str)"logger_roll.log") || !xrtFileExists((str)"logger_roll.log.1") ) {
		printf("Logger Test : rolling file missing\n");
		return 9;
	}

	printf("Logger Test : PASS\n");
	return 0;
}
