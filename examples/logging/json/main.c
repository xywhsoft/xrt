/*
 * 范例：logging/json —— 流式 JSON 日志：全字段结构化、零 DOM
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogJsonWrite   流式格式化：分片推给回调，不建中间字符串
 *   xlogrecord 全字段：Time / Level / Logger / Message / Fields[]
 *   xrtLogFieldUInt / FieldBool   数值与布尔字段构造
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/json/main.c -lws2_32 -liphlpapi
 * 预期输出（time 为 Unix 微秒，随运行变化）：
 *   {"time":1788575322193322,"level":"INFO","logger":"http",
 *    "message":"request completed","fields":{"request_id":42,"cached":false}}
 *
 * 流式 vs 缓冲（format_json_buffer）：
 *   大对象（超长消息、几十个字段）时分片直写目标，
 *   不需要先拼一整行再拷走——JSON Lines 高吞吐路径。
 * 失败报告示范：出错时用 xrtErrorMessage(xrtGetError())
 *   直接取结构化错误的描述（错误模型贯穿到日志模块）。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 输出回调：每个 JSON 分片一段（本例直写 stdout）。 */
static bool exampleLogWrite(xbytesview Data, ptr pUserData)
{
	FILE* pFile = (FILE*)pUserData;

	return fwrite(Data.Data, 1u, Data.Size, pFile) == Data.Size;
}



int main(void)
{
	xlogjsonconfig Config;
	xlogfield Fields[2];
	xlogrecord Record;

	/* 手工组装完整记录：时间、级别、来源、消息、两个结构化字段。 */
	memset(&Record, 0, sizeof(Record));
	Fields[0] = xrtLogFieldUInt(XRT_STR_LITERAL("request_id"), 42u);
	Fields[1] = xrtLogFieldBool(XRT_STR_LITERAL("cached"), false);
	Record.Time = xrtNow();
	Record.Level = XLOG_INFO;
	Record.Logger = XRT_STR_LITERAL("http");
	Record.Message = XRT_STR_LITERAL("request completed");
	Record.Fields = Fields;
	Record.FieldCount = 2u;

	/* 默认配置全字段输出；分片经回调直达目标。 */
	if (
		!xrtLogJsonConfigInit(&Config) ||
		!xrtLogJsonWrite(
			&Record,
			&Config,
			exampleLogWrite,
			stdout,
			NULL
		)
	) {
		fprintf(stderr, "log JSON failed: %s\n", xrtErrorMessage(xrtGetError()));
		return 1;
	}
	return 0;
}
