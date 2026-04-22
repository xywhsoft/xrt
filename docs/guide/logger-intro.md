# Logger 入门

Logger 适合替代临时 `printf` 调试输出。它可以同时写控制台和文件，并且可以为不同输出器设置不同的最低级别。

## 基础用法

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main()
{
	xrtInit();

	xlogger* pLogger = xlogCreate((str)"demo");
	xlogSetLevel(pLogger, XLOG_TRACE);

	xlogAddConsole(pLogger, XLOG_INFO, TRUE);
	xlogAddFile(pLogger, (str)"demo.log", XLOG_TRACE);

	xloggerDebug(pLogger, "debug value=%d", 123);
	xloggerInfo(pLogger, "program started");
	xloggerWarn(pLogger, "config missing, use default");
	xloggerError(pLogger, "operation failed");

	xlogDestroy(pLogger);
	xrtUnit();
	return 0;
}
```

控制台只会输出 `INFO` 及以上级别，文件会保存 `TRACE` 及以上级别。

## 滚动文件

```c
xlogAddRollingFile(pLogger, (str)"app.log", 10 * 1024 * 1024, 5, XLOG_INFO);
```

当 `app.log` 接近 10MB 时，日志文件会滚动为 `app.log.1`，旧文件继续后移，最多保留 5 个备份。

## JSON 日志

```c
xlogappender* pAppender = xlogAddFile(pLogger, (str)"app.jsonl", XLOG_INFO);
xlogAppenderSetFormat(pAppender, XLOG_FORMAT_JSON);
```

JSON 格式按每行一个对象写入，便于后续被日志采集程序读取。
