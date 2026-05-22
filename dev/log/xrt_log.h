

/*
	XRT 日志系统 API 头文件
*/

#ifndef XRT_LOG_H
#define XRT_LOG_H

#include <stdio.h>
#include "xrt.h"

// 日志级别
typedef enum {
	XLOG_LEVEL_TRACE   = 0,
	XLOG_LEVEL_DEBUG    = 1,
	XLOG_LEVEL_SUCCESS  = 2,
	XLOG_LEVEL_INFO     = 3,
	XLOG_LEVEL_SECTION  = 4,
	XLOG_LEVEL_TITLE    = 5,
	XLOG_LEVEL_WARN     = 6,
	XLOG_LEVEL_ERROR    = 7,
	XLOG_LEVEL_FATAL    = 8
} xlog_level;

// 日志格式
typedef enum {
	XLOG_FMT_TEXT,
	XLOG_FMT_SIMPLE,
} xlog_format;

// 日志事件
typedef struct {
	xtime       timestamp;
	int         level;
	str         file;
	int         line;
	str         func;
	uint32      thread_id;
	str         message;
} xlog_event;

// 日志输出器
typedef struct xlog_appender {
	str                     name;
	int                     min_level;
	int                     format;
	bool                    enable_color;
	void*                   user_data;
	FILE*                   file;       // 文件句柄
	void    (*write)(struct xlog_appender* appender, xlog_event* event);
	void    (*flush)(struct xlog_appender* appender);
	void    (*destroy)(struct xlog_appender* appender);
} xlog_appender, *xlog_appender_ptr;

// 日志记录器
typedef struct xlog_logger {
	str                 name;
	int                 level;
	xparray             appenders;
} xlog_logger, *xlog_logger_ptr;

// 全局默认日志记录器
xlog_logger_ptr xrtGetDefaultLogger();
void xrtSetDefaultLogger(xlog_logger_ptr logger);

// 创建/销毁日志记录器
xlog_logger_ptr xrtLogCreate(const char* name);
void xrtLogDestroy(xlog_logger_ptr logger);

// 配置日志记录器
void xrtLogSetLevel(xlog_logger_ptr logger, int level);

// 输出器管理
xlog_appender_ptr xrtLogAddConsoleAppender(xlog_logger_ptr logger);
xlog_appender_ptr xrtLogAddFileAppender(xlog_logger_ptr logger, const char* file_path, size_t max_file_size, int max_backup_count);
void xrtLogAddAppender(xlog_logger_ptr logger, xlog_appender_ptr appender);
void xrtLogSetAppenderLevel(xlog_appender_ptr appender, int level);
void xrtLogSetColor(xlog_appender_ptr appender, bool enable);

// 日志输出
void xrtLogLog(xlog_logger_ptr logger, int level, const char* file, int line, const char* func, const char* fmt, ...);

// 刷新日志
void xrtLogFlush(xlog_logger_ptr logger);

// 获取级别名称和颜色
str xrtLogGetLevelName(int level);
str xrtLogGetLevelColor(int level);

// 日志宏
#define xrtLogTrace(logger, ...)   xrtLogLog(logger, XLOG_LEVEL_TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogDebug(logger, ...)   xrtLogLog(logger, XLOG_LEVEL_DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogSuccess(logger, ...) xrtLogLog(logger, XLOG_LEVEL_SUCCESS, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogInfo(logger, ...)    xrtLogLog(logger, XLOG_LEVEL_INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogSection(logger, ...) xrtLogLog(logger, XLOG_LEVEL_SECTION, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogTitle(logger, ...)   xrtLogLog(logger, XLOG_LEVEL_TITLE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogWarn(logger, ...)    xrtLogLog(logger, XLOG_LEVEL_WARN, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogError(logger, ...)   xrtLogLog(logger, XLOG_LEVEL_ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define xrtLogFatal(logger, ...)   xrtLogLog(logger, XLOG_LEVEL_FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

// 默认记录器快捷宏
#define xlogTrace(...)   xrtLogTrace(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogDebug(...)   xrtLogDebug(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogSuccess(...) xrtLogSuccess(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogInfo(...)    xrtLogInfo(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogSection(...) xrtLogSection(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogTitle(...)   xrtLogTitle(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogWarn(...)    xrtLogWarn(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogError(...)   xrtLogError(xrtGetDefaultLogger(), __VA_ARGS__)
#define xlogFatal(...)   xrtLogFatal(xrtGetDefaultLogger(), __VA_ARGS__)

#endif
