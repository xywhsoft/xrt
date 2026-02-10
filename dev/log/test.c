#define XRT_IMPLEMENTATION
#include "xrt_log.h"

int main()
{
	#if defined(_WIN32) || defined(_WIN64)
		SetConsoleOutputCP(65001);
	#endif
	
	xrtGlobalData* core = xrtInit();
	
	printf("=== XRT 日志系统测试 ===\n\n");
	
	xlog_logger_ptr logger = xrtLogCreate("myapp");
	xrtLogSetLevel(logger, XLOG_LEVEL_TRACE);
	
	xlog_appender_ptr console = xrtLogAddConsoleAppender(logger);
	xrtLogSetColor(console, true);
	
	xlog_appender_ptr file = xrtLogAddFileAppender(logger, "app.log", 1024 * 1024, 5);
	
	xrtSetDefaultLogger(logger);
	
	xlogTitle("XRT 日志系统测试");
	
	xlogSection("基础功能");
	xlogTrace("应用程序启动");
	xlogDebug("调试信息: 配置已加载");
	xlogSuccess("初始化成功！");
	xlogInfo("版本: 1.0.0");
	xlogWarn("警告: 配置文件未找到，使用默认配置");
	
	xlogSection("级别过滤");
	xrtLogSetLevel(logger, XLOG_LEVEL_WARN);
	xlogTrace("TRACE - 不会输出");
	xlogDebug("DEBUG - 不会输出");
	xlogSuccess("SUCCESS - 不会输出");
	xlogInfo("INFO - 不会输出");
	xlogWarn("WARN - 会输出");
	xlogError("ERROR - 会输出");
	
	xrtLogDestroy(logger);
	
	printf("\n测试程序执行完成！\n");
	printf("日志已写入 app.log 文件\n");
	
	xrtUnit();
	
	#if defined(_WIN32) || defined(_WIN64)
		printf("\n按任意键退出...");
		system("pause");
	#endif
	
	return 0;
}
