/*
 * 范例：logging/file_json —— 一行挂载 JSON Lines 滚动文件输出
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogAddJsonFile   便捷层：JSON 格式器 + 滚动文件一步挂到 Logger
 *   xrtLogFieldUInt     构造无符号整数字段
 *   xrtLogFields        提交结构化记录（字段进 JSON 的 fields 对象）
 * 模块宏：XRT_MODULE_LOGGER（依赖 FILE/JSON 格式器）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/file_json/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   wrote example_logger_json.log
 * （文件内容为 JSON Lines：每行一个完整 JSON 对象，
 *   字段形如 {"time":...,"level":"INFO","message":"...","fields":{...}}）
 *
 * 便捷层 vs 手工组装（file 范例）：一行等价于
 *   "创建 JSON 格式器 + 创建文件 Sink + Attach" 三步；
 *   生产日志接入绝大多数场景用这组 Add* Helper 即可
 *   （同族：AddConsole / AddTextFile / AddRing）。
 */

#include <xrt.h>

#include <stdio.h>



int main(void)
{
	xlogfileoptions Options;
	xlogger* pLogger;
	xlogfield Field;

	if ( !xrtLogFileOptionsInit(&Options, "example_logger_json.log") ) {
		return 1;
	}
	Options.MaxBytes = 1024u * 1024u;   /* 1MB 轮转 */
	Options.BackupCount = 3u;

	pLogger = xrtLogCreate(XRT_STR_LITERAL("example"), XLOG_INFO);
	Field = xrtLogFieldUInt(XRT_STR_LITERAL("request_id"), 42u);
	if (
		(pLogger == NULL) ||
		!xrtLogAddJsonFile(pLogger, &Options, NULL) ||   /* 一行挂载 */
		(xrtLogFields(
			pLogger,
			XLOG_INFO,
			XRT_STR_LITERAL("JSON file sink ready"),
			&Field,
			1u
		) != XLOG_RESULT_WRITTEN)
	) {
		xrtLogFree(pLogger);
		return 2;
	}
	xrtLogFree(pLogger);
	printf("wrote example_logger_json.log\n");
	return 0;
}
