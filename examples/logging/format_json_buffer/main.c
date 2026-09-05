/*
 * 范例：logging/format_json_buffer —— 记录 → JSON 字符串（分配版）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogJsonConfigInit  JSON 格式配置（字段开关）
 *   XLOG_JSON_LEVEL / XLOG_JSON_MESSAGE  只输出级别与消息
 *   xrtLogJson            格式化为拥有式字符串（xrtFree 释放）
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/format_json_buffer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   {"level":"INFO","message":"service ready"}
 *
 * 独立格式化入口的用途：不走 Sink，直接把日志行交给
 *   自己的传输层（HTTP 上报、消息队列）；
 *   Flags 控制字段取舍——time/logger/fields 各自开关
 *   （全量输出见 json 范例的流式版本）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



int main(void)
{
	xlogjsonconfig Config;
	xlogrecord Record;
	str sJson;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("service ready");

	/* 只开 level + message 两个键：最小 JSON 载荷。 */
	if ( !xrtLogJsonConfigInit(&Config) ) {
		return 1;
	}
	Config.Flags = XLOG_JSON_LEVEL | XLOG_JSON_MESSAGE;

	/* 一步格式化：产物拥有式，长度出参避免 strlen。 */
	sJson = xrtLogJson(&Record, &Config, &iSize);
	if ( sJson == NULL ) {
		return 2;
	}
	printf("%.*s\n", (int)iSize, sJson);
	xrtFree(sJson);
	return 0;
}
