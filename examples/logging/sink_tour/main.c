/*
 * 范例：logging/sink_tour —— Sink 管理族 + 字段构造全接口
 * ----------------------------------------------------------------
 * 演示 API：
 *   【Sink 管理】  xrtLogConsoleConfigInit / xrtLogConsole（自定义控制台）
 *                  xrtLogSinkRef / xrtLogSinkName / xrtLogSinkLevel /
 *                  xrtLogSinkSetLevel / xrtLogSinkCount / xrtLogSinkStats
 *                  xrtLogDetach / xrtLogDetachAll
 *   【字段构造】  xrtLogFieldNull / Float / String / Time / Error
 *   【格式化】    xrtLogTextWrite / xrtLogFieldsPrintf / xrtLogFieldsPrintfV /
 *                  xrtLogSourcePrintf / xrtLogSourcePrintfV
 *   【校验】      xrtLogTextConfigValidate / xrtLogJsonConfigValidate /
 *                  xrtLogRecordValidate
 *   【配置器】    xrtLogTextFile / xrtLogJsonFile（带路径回调）
 *                  xrtLogFilePath / xrtLogFileReopen / xrtLogFileRotate /
 *                  xrtLogFileStats
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/logging/sink_tour/main.c -lws2_32 -liphlpapi
 * 预期输出（printf 行如下；控制台 Sink 的日志行含
 *   时间戳并与 printf 交错，此处不列）：
 *   sink: name=console count=1 level=3 stats-ok
 *   fields: null+float+string+time+error = 5
 *   text-write: TRACE -
 *   ok(9)
 *   validate: text=1 json=1 record=1
 *   file-path=xrt-sink-tour.log
 *   log-stats=1
 *   detach-single=1 detach-all=0
 *
 * 文件专用族（FilePath/Reopen/Rotate/Stats）面向运行时运维：
 *   Rotate 触发立即轮转、Reopen 支持日志被外部截断后重开。
 */

#include <stdio.h>
#include <stdarg.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

/* TextWrite 输出回调（与 json 范例同型）。 */
static bool writeSink(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1u, Data.Size, pFile) == Data.Size;
}

/* 包装 PrintfV（va_list 版）。 */
static xlogresult logFieldsV(xlogger* pLogger, cstr sFmt, ...)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFmt);
	Result = xrtLogFieldsPrintfV(pLogger, XLOG_INFO, NULL, 0u, sFmt, Args);
	va_end(Args);
	return Result;
}

/* 包装 PrintfV（最底层 va_list printf）。 */
static xlogresult logPrintfV(xlogger* pLogger, cstr sFmt, ...)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFmt);
	Result = xrtLogPrintfV(pLogger, XLOG_INFO, sFmt, Args);
	va_end(Args);
	return Result;
}

/* 包装 SourcePrintfV（带来源的 va_list 版）。 */
static xlogresult logSourceV(xlogger* pLogger, xloglevel Level, cstr sFmt, ...)
{
	va_list Args;
	xlogresult Result;

	va_start(Args, sFmt);
	Result = xrtLogSourcePrintfV(pLogger, Level, NULL, 0u, SV("demo.c"), SV("main"), 1u, 0u, sFmt, Args);
	va_end(Args);
	return Result;
}

int main(void)
{
	xlogconsoleconfig ConsoleConfig;
	xlogsink* pConsole;
	xlogger* pLogger;
	xlogstats Stats;

	/* 控制台 Sink（ConfigInit 后保持默认名与流）。 */
	(void)xrtLogConsoleConfigInit(&ConsoleConfig);
	pConsole = xrtLogConsole(&ConsoleConfig);
	if ( pConsole == NULL ) {
		return 1;
	}
	pLogger = xrtLogCreate(SV("sink-tour"), XLOG_TRACE);
	if ( (pLogger == NULL) || !xrtLogAttach(pLogger, pConsole) ) {
		return 2;
	}

	/* Sink 管理族：Name / Level / SetLevel / Count / Stats。 */
	printf("sink: name=%.*s",
		(int)xrtLogSinkName(pConsole).Size, xrtLogSinkName(pConsole).Data);
	printf(" count=%zu", xrtLogSinkCount(pLogger));
	(void)xrtLogSinkSetLevel(pConsole, XLOG_WARN);
	printf(" level=%d", (int)xrtLogSinkLevel(pConsole));
	if ( xrtLogSinkStats(pConsole, &Stats) ) {
		printf(" stats-ok");
	}
	printf("\n");

	/* 字段构造族五种（Null/Float/String/Time/Error）。 */
	{
		xlogfield Fields[5];
		size_t i = 0;

		Fields[i++] = xrtLogFieldNull(SV("n"));
		Fields[i++] = xrtLogFieldFloat(SV("f"), 1.5);
		Fields[i++] = xrtLogFieldString(SV("s"), SV("val"));
		Fields[i++] = xrtLogFieldTime(SV("t"), xrtNow());
		Fields[i++] = xrtLogFieldError(SV("e"), NULL);
		printf("fields: null+float+string+time+error = %zu\n", i);
		/* 全字段提交 + FieldsPrintf（带字段数组的 printf）。 */
		(void)xrtLogFields(pLogger, XLOG_ERROR, SV("fields demo"),
			Fields, 1u);
		(void)xrtLogFieldsPrintf(pLogger, XLOG_ERROR, Fields, 1u,
			"count=%d", 1);
	}

	/* SourcePrintf（带来源标签的 printf 风格）。 */
	(void)xrtLogSourcePrintf(pLogger, XLOG_WARN, NULL, 0u,
		SV("demo.c"), SV("main"), 1u, 0u, "src=%s", "printf");
	(void)logSourceV(pLogger, XLOG_WARN, "srcv=%d", 42);

	/* 文本格式化缓冲版 + 三种校验器。 */
	{
		char Text[128];
		size_t iSize = 0;
		xlogtextconfig TextConfig;
		xlogjsonconfig JsonConfig;

		(void)xrtLogTextConfigInit(&TextConfig, XLOG_TEXT_SIMPLE);
		printf("text-write: ");
		if ( xrtLogTextWrite(&(xlogrecord){ 0 }, &TextConfig,
			writeSink, stdout, &iSize) ) {
			printf("ok(%zu)", iSize);
		}
		printf("\n");
		printf("validate: text=%d",
			xrtLogTextConfigValidate(&TextConfig) ? 1 : 0);
		(void)xrtLogJsonConfigInit(&JsonConfig);
		printf(" json=%d",
			xrtLogJsonConfigValidate(&JsonConfig) ? 1 : 0);
		printf(" record=%d\n",
			xrtLogRecordValidate(&(xlogrecord){ 0 }) ? 1 : 0);
	}

	/* 文件专用族：TextFile / JsonFile / FilePath / Rotate / Reopen / Stats。 */
	{
		xlogfileoptions Options;
		xlogger* pFileLogger = xrtLogCreate(SV("file-tour"),
			XLOG_INFO);

		(void)xrtLogFileOptionsInit(&Options, "xrt-sink-tour.log");
		Options.MaxBytes = 1024u * 1024u;
		Options.BackupCount = 1u;
		{
			xlogsink* pTextFileSink = xrtLogTextFile(&Options, NULL);

			if ( pTextFileSink != NULL ) {
				/* 文件族全收 Sink：Path / Rotate / Reopen / Stats。 */
				cstr sPath = xrtLogFilePath(pTextFileSink);

				printf("file-path=%s\n", sPath ? sPath : "?");
				(void)xrtLogFileRotate(pTextFileSink);
				(void)xrtLogFileReopen(pTextFileSink);
				{
					xlogfilestats FileStats;

					(void)xrtLogFileStats(pTextFileSink, &FileStats);
				}
				xrtLogSinkFree(pTextFileSink);
			}
			xrtLogFree(pFileLogger);
		}
		/* JsonFile：同样独立创建 Sink。 */
		{
			xlogsink* pJsonSink = xrtLogJsonFile(&Options, NULL);

			if ( pJsonSink != NULL ) {
				xrtLogSinkFree(pJsonSink);
			}
		}
	}

	/* Printf 族（FieldsPrintf 由 V 版包装；PrintfV 直接调用）。 */
	(void)xrtLogPrintf(pLogger, XLOG_INFO, "plain=%d", 1);
	(void)logFieldsV(pLogger, "fsv=%s", "v");
	(void)logPrintfV(pLogger, "pv=%d", 7);

	/* LogStats：Logger 级统计快照（区别于 Sink 级 Stats）。 */
	{
		xlogstats LogStats;

		printf("log-stats=%d\n", xrtLogStats(pLogger, &LogStats) ? 1 : 0);
	}

	/* Source：非 printf 版直接提交（消息为字符串视图）。 */
	(void)xrtLogSource(pLogger, XLOG_WARN, SV("direct source"), NULL, 0u,
		SV("demo.c"), SV("main"), 1u, 0u);

	/* Detach（单个）/ DetachAll（剩余全部）/ SinkRef 收尾。 */
	printf("detach-single=%d", xrtLogDetach(pLogger, pConsole) ? 1 : 0);
	printf(" detach-all=%zu\n", xrtLogDetachAll(pLogger));
	(void)xrtLogSinkRef(pConsole);
	xrtLogSinkFree(pConsole);
	xrtLogFree(pLogger);
	(void)xrtFileDelete("xrt-sink-tour.log");
	(void)xrtFileDelete("xrt-sink-tour.log.1");
	return 0;
}
