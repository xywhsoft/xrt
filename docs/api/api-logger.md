# Logger 日志系统

Logger 模块提供同步日志输出能力，支持控制台、文件、按大小滚动文件和自定义输出器。

## 级别

```c
XLOG_TRACE
XLOG_DEBUG
XLOG_INFO
XLOG_WARN
XLOG_ERROR
XLOG_FATAL
XLOG_OFF
```

日志事件只有在 `event level >= logger level` 且 `event level >= appender min level` 时才会写入。

## 创建日志器

```c
xlogger* pLogger = xlogCreate((str)"app");
xlogSetLevel(pLogger, XLOG_TRACE);
```

销毁：

```c
xlogFlush(pLogger);
xlogDestroy(pLogger);
```

## 输出器

控制台：

```c
xlogAddConsole(pLogger, XLOG_INFO, TRUE);
```

普通文件：

```c
xlogAddFile(pLogger, (str)"app.log", XLOG_TRACE);
```

按大小滚动文件：

```c
xlogAddRollingFile(pLogger, (str)"app.log", 10 * 1024 * 1024, 5, XLOG_INFO);
```

滚动文件命名为：

```text
app.log
app.log.1
app.log.2
```

## 格式

```c
xlogAppenderSetFormat(pAppender, XLOG_FORMAT_TEXT);
xlogAppenderSetFormat(pAppender, XLOG_FORMAT_SIMPLE);
xlogAppenderSetFormat(pAppender, XLOG_FORMAT_JSON);
```

默认格式是 `XLOG_FORMAT_TEXT`。

## 写日志

```c
xloggerInfo(pLogger, "server started: port=%d", 8080);
xloggerWarn(pLogger, "config missing: %s", "app.ini");
xloggerError(pLogger, "request failed: code=%d", 500);
```

默认日志器快捷宏：

```c
xlogInfo("server started");
xlogError("fatal error: %s", sError);
```

首次调用 `xlogDefault()` 会创建一个带控制台输出器的默认日志器。
