/*
 * 范例：logging/core —— 自定义 Sink + 结构化字段：日志系统的地基
 * ----------------------------------------------------------------
 * 演示 API：
 *   xlogsinkconfig / xrtLogSinkCreate   描述并创建输出目的地
 *   xrtLogCreate / xrtLogAttach         Logger 与 Sink 组合
 *   xrtLogFieldInt                      构造整数字段
 *   xrtLogFields                        提交带结构化字段的记录
 *   xlogrecord / xrtLogLevelName        借用记录结构与级别名
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/core/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   [INFO] example: request complete
 *
 * Sink 契约要点（exampleWrite 体现）：
 *   - 记录按借用传入（pRecord），回调内不得保留引用；
 *   - 返回 XLOG_RESULT_WRITTEN / _ERROR，由 Logger 汇总上报；
 *   - UserData 携带输出目标（本例 FILE*）——
 *     同一个 Write 函数可服务任意目的地。
 * 结构化字段（request_id=42）由 Sink 自行决定消费方式：
 *   文本 Sink 可打印，JSON Sink 进 fields 对象（见 json 范例）。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 示例 Sink 写入函数：格式 [级别] 名字: 消息，零中间分配。 */
static xlogresult exampleWrite(
	const xlogrecord* pRecord,
	ptr pUserData
)
{
	FILE* pFile = (FILE*)pUserData;

	fprintf(
		pFile,
		"[%s] %.*s: %.*s\n",
		xrtLogLevelName(pRecord->Level),
		(int)pRecord->Logger.Size,
		pRecord->Logger.Data,
		(int)pRecord->Message.Size,
		pRecord->Message.Data
	);
	return ferror(pFile) == 0
		? XLOG_RESULT_WRITTEN
		: XLOG_RESULT_ERROR;
}



int main(void)
{
	xlogsinkconfig Config;
	xlogfield Field;
	xlogger* pLogger;
	xlogsink* pSink;

	/* Sink 配置四要素：名字 / 级别门槛 / 写入函数 / 用户数据。 */
	memset(&Config, 0, sizeof(Config));
	Config.Name = XRT_STR_LITERAL("stdout");
	Config.Level = XLOG_INFO;
	Config.Write = exampleWrite;
	Config.UserData = stdout;

	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_DEBUG);
	pSink = xrtLogSinkCreate(&Config);
	if (
		(pLogger == NULL) || (pSink == NULL) ||
		!xrtLogAttach(pLogger, pSink)
	) {
		xrtLogSinkFree(pSink);
		xrtLogFree(pLogger);
		return 1;
	}

	/* 结构化提交：一个整数字段随记录直达 Sink。 */
	Field = xrtLogFieldInt(XRT_STR_LITERAL("request_id"), 42);
	if (
		xrtLogFields(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("request complete"),
			&Field,
			1u
		) != XLOG_RESULT_WRITTEN
	) {
		xrtLogSinkFree(pSink);
		xrtLogFree(pLogger);
		return 2;
	}
	xrtLogSinkFree(pSink);
	xrtLogFree(pLogger);
	return 0;
}
