/*
 * 范例：logging/logger_tour —— Logger 层管理：默认/引用/级别/命名/来源
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogDefault / xrtLogSetDefault / xrtLogRef
 *   xrtLogName / xrtLogLevel / xrtLogSetLevel
 *   xrtLogSubmit / xrtLogSource / xrtLogConsole / xrtLogConsoleConfigInit
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头格式，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/logging/logger_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   default-logger=ok name=demo-tour level=0
 *   set-level=1 console-config=ok
 *   submit=1
 *   ref-release=ok
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xlogger* pLogger = xrtLogCreate(SV("demo-tour"), XLOG_TRACE);
	xlogconsoleconfig ConsoleConfig;
	xlogrecord Record;

	if ( pLogger == NULL ) {
		return 1;
	}

	/* SetDefault：迁移所有权为全局默认——后续任意处 Default() 取用。 */
	printf("default-logger=%s",
		xrtLogSetDefault(pLogger) ? "ok" : "fail");
	{
		xlogger* pDefault = xrtLogDefault();

		printf(" name=%.*s",
			(int)xrtLogName(pDefault).Size, xrtLogName(pDefault).Data);
		printf(" level=%d", (int)xrtLogLevel(pDefault));
	}
	printf("\n");

	/* SetLevel：运行时动态调整门槛。 */
	printf("set-level=%d", xrtLogSetLevel(pLogger, XLOG_WARN) ? 1 : 0);

	/* Console 独立入口（Config 版）——创建并挂到默认 Logger。 */
	(void)xrtLogConsoleConfigInit(&ConsoleConfig);
	{
		xlogsink* pConsoleSink = xrtLogConsole(&ConsoleConfig);

		printf(" console-config=%s", pConsoleSink != NULL ? "ok" : "fail");
		if ( pConsoleSink != NULL ) {
			(void)xrtLogAttach(xrtLogDefault(), pConsoleSink);
			xrtLogSinkFree(pConsoleSink);
		}
		printf("\n");
	}

	/* Submit：最底层提交（自构 record）。 */
	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_ERROR;
	Record.Message = SV("direct submit");
	printf("submit=%d\n",
		xrtLogSubmit(xrtLogDefault(), &Record) == XLOG_RESULT_WRITTEN ? 1 : 0);

	/* Ref：增引用（跨模块共享 Logger），双 Release 收尾。 */
	{
		xlogger* pRef = xrtLogRef(pLogger);

		printf("ref-release=%s\n", pRef != NULL ? "ok" : "fail");
		xrtLogFree(pRef);
	}
	return 0;
}
