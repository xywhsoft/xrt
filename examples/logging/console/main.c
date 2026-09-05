/*
 * 范例：logging/console —— 三行代码启动控制台日志（最短可用路径）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogCreate     创建 Logger（名字 + 最低级别门槛）
 *   xrtLogAddConsole 附加默认控制台 Sink（时间/级别/名字/消息）
 *   xrtLog           提交一条日志（级别 + 消息视图）
 *   xrtLogFlush      冲刷全部 Sink（进程退出前必调）
 *   xrtLogFree       释放 Logger
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/console/main.c -lws2_32 -liphlpapi
 * 预期输出（UTC 时间戳随运行变化）：
 *   2026-09-05T02:26:05.994045Z INFO example - service started
 *   2026-09-05T02:26:05.994091Z WARN example - using fallback config
 *   2026-09-05T02:26:05.994100Z ERROR example - request failed
 *
 * 分层模型：Logger（过滤/分发）⇄ Sink（输出目的地）。
 *   一个 Logger 可挂多个 Sink（控制台 + 文件 + 网络），
 *   级别门槛在 Logger 与 Sink 两级各自生效。
 */

#include <xrt.h>



int main(void)
{
	/* TRACE 门槛 = 全部放行；生产常用 INFO 压掉调试输出。 */
	xlogger* pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_TRACE);

	if ( (pLogger == NULL) || !xrtLogAddConsole(pLogger, NULL) ) {
		xrtLogFree(pLogger);
		return 1;
	}

	/* 三条不同级别：颜色按级别自动区分（终端支持时）。 */
	(void)xrtLog(pLogger, XLOG_INFO, XRT_STR_LITERAL("service started"));
	(void)xrtLog(pLogger, XLOG_WARN, XRT_STR_LITERAL("using fallback config"));
	(void)xrtLog(pLogger, XLOG_ERROR, XRT_STR_LITERAL("request failed"));

	/* 退出前冲刷，保证缓冲里的最后几条一定落地。 */
	(void)xrtLogFlush(pLogger);
	xrtLogFree(pLogger);
	return 0;
}
