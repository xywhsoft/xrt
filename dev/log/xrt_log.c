#include <stdarg.h>
#include <string.h>
#include "xrt_log.h"

// ANSI 颜色代码
#define XLOG_COLOR_FG(code)   "\033[" code "m"
#define XLOG_COLOR_RESET      "\033[0m"

// 默认记录器
static xlog_logger_ptr g_default_logger = NULL;

// 获取级别颜色代码
str xrtLogGetLevelColor(int level)
{
	switch (level) {
		case XLOG_LEVEL_TRACE:   return XLOG_COLOR_FG("90");
		case XLOG_LEVEL_DEBUG:   return XLOG_COLOR_FG("37;1");
		case XLOG_LEVEL_SUCCESS: return XLOG_COLOR_FG("32");
		case XLOG_LEVEL_INFO:    return XLOG_COLOR_FG("39");
		case XLOG_LEVEL_SECTION: return XLOG_COLOR_FG("39;1");
		case XLOG_LEVEL_TITLE:   return XLOG_COLOR_FG("30;1");
		case XLOG_LEVEL_WARN:    return XLOG_COLOR_FG("33");
		case XLOG_LEVEL_ERROR:   return XLOG_COLOR_FG("31");
		case XLOG_LEVEL_FATAL:   return XLOG_COLOR_FG("35");
		default:                 return XLOG_COLOR_RESET;
	}
}

// 获取级别名称
str xrtLogGetLevelName(int level)
{
	switch (level) {
		case XLOG_LEVEL_TRACE:   return "TRACE";
		case XLOG_LEVEL_DEBUG:   return "DEBUG";
		case XLOG_LEVEL_SUCCESS: return "SUCCESS";
		case XLOG_LEVEL_INFO:    return "INFO   ";
		case XLOG_LEVEL_SECTION: return "";
		case XLOG_LEVEL_TITLE:   return "";
		case XLOG_LEVEL_WARN:    return "WARN   ";
		case XLOG_LEVEL_ERROR:   return "ERROR  ";
		case XLOG_LEVEL_FATAL:   return "FATAL  ";
		default:                 return "UNKN   ";
	}
}

// 格式化日志事件（TEXT 格式）
static str xrtLogFormatEvent_Text(xlog_event* event, bool with_color)
{
	str color_start = "";
	str color_end = "";
	
	if (with_color) {
		color_start = xrtLogGetLevelColor(event->level);
		color_end = XLOG_COLOR_RESET;
	}
	
	// Title: 独占一行
	if (event->level == XLOG_LEVEL_TITLE) {
		return xrtFormat("\n%s%s%s\n",
			color_start, event->message, color_end);
	}
	
	// Section: 使用粗体
	if (event->level == XLOG_LEVEL_SECTION) {
		return xrtFormat("%s=== %s ===%s\n",
			color_start, event->message, color_end);
	}
	
	// 普通日志格式
	str time_str = xrtNowStr();
	str level_name = xrtLogGetLevelName(event->level);
	str result;
	
	if (event->file && event->line > 0) {
		result = xrtFormat("%s [%s] [%s:%d:%s] %s%s%s\n",
			time_str, level_name, event->file, event->line, event->func,
			color_start, event->message, color_end);
	} else {
		result = xrtFormat("%s [%s] %s%s%s\n",
			time_str, level_name, color_start, event->message, color_end);
	}
	
	return result;
}

// 控制台输出器写入
static void xrtLogConsole_Write(xlog_appender* appender, xlog_event* event)
{
	FILE* fp = stdout;
	
	str formatted = xrtLogFormatEvent_Text(event, appender->enable_color);
	fprintf(fp, "%s", formatted);
	fflush(fp);
	
	xrtFree(formatted);
}

// 控制台输出器刷新
static void xrtLogConsole_Flush(xlog_appender* appender)
{
	fflush(stdout);
}

// 控制台输出器销毁
static void xrtLogConsole_Destroy(xlog_appender* appender)
{
	if (appender->name) xrtFree(appender->name);
	xrtFree(appender);
}

// 文件输出器写入
static void xrtLogFile_Write(xlog_appender* appender, xlog_event* event)
{
	FILE* fp = (FILE*)appender->file;
	if (!fp) return;
	
	// 文件输出不包含颜色
	str formatted = xrtLogFormatEvent_Text(event, FALSE);
	fprintf(fp, "%s", formatted);
	fflush(fp);
	
	xrtFree(formatted);
}

// 文件输出器刷新
static void xrtLogFile_Flush(xlog_appender* appender)
{
	FILE* fp = (FILE*)appender->file;
	if (fp) fflush(fp);
}

// 文件输出器销毁
static void xrtLogFile_Destroy(xlog_appender* appender)
{
	FILE* fp = (FILE*)appender->file;
	if (fp) fclose(fp);
	
	if (appender->name) xrtFree(appender->name);
	if (appender->user_data) xrtFree(appender->user_data);
	xrtFree(appender);
}

// 创建日志记录器
xlog_logger_ptr xrtLogCreate(const char* name)
{
	xlog_logger_ptr logger = xrtMalloc(sizeof(xlog_logger));
	if (!logger) return NULL;
	
	memset(logger, 0, sizeof(xlog_logger));
	
	logger->name = xrtCopyStr((str)name, 0);
	logger->level = XLOG_LEVEL_INFO;
	logger->appenders = xrtPtrArrayCreate(10, 10);
	
	return logger;
}

// 销毁日志记录器
void xrtLogDestroy(xlog_logger_ptr logger)
{
	if (!logger) return;
	
	xrtLogFlush(logger);
	
	for (int i = 0; i < logger->appenders->Count; i++) {
		xlog_appender_ptr appender = xrtPtrArrayGet(logger->appenders, i + 1);
		if (appender && appender->destroy) {
			appender->destroy(appender);
		}
	}
	
	xrtPtrArrayDestroy(logger->appenders);
	
	if (logger->name) xrtFree(logger->name);
	
	xrtFree(logger);
}

// 设置日志级别
void xrtLogSetLevel(xlog_logger_ptr logger, int level)
{
	if (logger) {
		logger->level = level;
	}
}

// 添加控制台输出器
xlog_appender_ptr xrtLogAddConsoleAppender(xlog_logger_ptr logger)
{
	if (!logger) return NULL;
	
	xlog_appender_ptr console = xrtMalloc(sizeof(xlog_appender));
	if (!console) return NULL;
	
	memset(console, 0, sizeof(xlog_appender));
	
	console->name = xrtCopyStr("console", 0);
	console->min_level = XLOG_LEVEL_TRACE;
	console->format = XLOG_FMT_TEXT;
	console->enable_color = TRUE;
	console->file = stdout;
	console->write = xrtLogConsole_Write;
	console->flush = xrtLogConsole_Flush;
	console->destroy = xrtLogConsole_Destroy;
	
	xrtPtrArrayAppend(logger->appenders, console);
	
	return console;
}

// 添加文件输出器
xlog_appender_ptr xrtLogAddFileAppender(xlog_logger_ptr logger, const char* file_path, size_t max_file_size, int max_backup_count)
{
	if (!logger || !file_path) return NULL;
	
	xlog_appender_ptr file = xrtMalloc(sizeof(xlog_appender));
	if (!file) return NULL;
	
	memset(file, 0, sizeof(xlog_appender));
	
	file->name = xrtCopyStr("file", 0);
	file->min_level = XLOG_LEVEL_TRACE;
	file->format = XLOG_FMT_TEXT;
	file->enable_color = FALSE;
	file->file = fopen(file_path, "a");
	file->write = xrtLogFile_Write;
	file->flush = xrtLogFile_Flush;
	file->destroy = xrtLogFile_Destroy;
	
	file->user_data = xrtCopyStr((str)file_path, 0);
	
	xrtPtrArrayAppend(logger->appenders, file);
	
	return file;
}

// 设置输出器最低级别
void xrtLogSetAppenderLevel(xlog_appender_ptr appender, int level)
{
	if (appender) {
		appender->min_level = level;
	}
}

// 启用/禁用彩色输出
void xrtLogSetColor(xlog_appender_ptr appender, bool enable)
{
	if (appender) {
		appender->enable_color = enable;
	}
}

// 日志输出核心函数
void xrtLogLog(xlog_logger_ptr logger, int level, const char* file, int line, const char* func, const char* fmt, ...)
{
	if (!logger) return;
	
	// 级别过滤
	if (level < logger->level) return;
	
	// 格式化消息
	va_list args;
	va_start(args, fmt);
	
	// 使用 vsnprintf 计算大小
	int iSize = vsnprintf(NULL, 0, fmt, args);
	va_end(args);
	
	str message = NULL;
	if (iSize > 0) {
		message = xrtMalloc(iSize + 1);
		if (message) {
			va_start(args, fmt);
			iSize = vsnprintf(message, iSize + 1, fmt, args);
			va_end(args);
			message[iSize] = 0;
		}
	}
	
	if (!message) return;
	
	// 创建日志事件
	xlog_event event;
	event.timestamp = xrtNow();
	event.level = level;
	event.file = (str)file;
	event.line = line;
	event.func = (str)func;
	event.message = message;
	
	// 写入所有输出器
	for (int i = 0; i < logger->appenders->Count; i++) {
		xlog_appender_ptr appender = xrtPtrArrayGet(logger->appenders, i + 1);
		
		if (appender && level >= appender->min_level && appender->write) {
			appender->write(appender, &event);
		}
	}
	
	xrtFree(message);
}

// 刷新日志
void xrtLogFlush(xlog_logger_ptr logger)
{
	if (!logger) return;
	
	for (int i = 0; i < logger->appenders->Count; i++) {
		xlog_appender_ptr appender = xrtPtrArrayGet(logger->appenders, i + 1);
		
		if (appender && appender->flush) {
			appender->flush(appender);
		}
	}
}

// 获取默认记录器
xlog_logger_ptr xrtGetDefaultLogger()
{
	return g_default_logger;
}

// 设置默认记录器
void xrtSetDefaultLogger(xlog_logger_ptr logger)
{
	g_default_logger = logger;
}
