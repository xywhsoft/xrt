/*
 * 范例：logging/format_text_buffer —— 记录 → 单行文本（字段自选）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtLogTextConfigInit       文本格式配置（预设可再叠加开关）
 *   XLOG_TEXT_LEVEL / _MESSAGE  只输出级别与消息
 *   xrtLogText                 格式化为拥有式字符串
 * 模块宏：XRT_MODULE_LOGGER
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/logging/format_text_buffer/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   service ready
 *
 * 与 format_json_buffer 对称：同一条记录、自选字段、
 *   产出单行文本（LEVEL 开关会带 "INFO " 前缀；这里
 *   输出恰以消息开头，故结果就是消息本身）。
 * 时间/名字/字段各有开关——细到"只要消息"的极简模式。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>



int main(void)
{
	xlogtextconfig Config;
	xlogrecord Record;
	str sText;
	size_t iSize;

	memset(&Record, 0, sizeof(Record));
	Record.Level = XLOG_INFO;
	Record.Message = XRT_STR_LITERAL("service ready");

	/* 级别 + 消息：预设参数本可传 XLOG_TEXT_SIMPLE，此处演示开关组合。 */
	if ( !xrtLogTextConfigInit(&Config, XLOG_TEXT_LEVEL |
		XLOG_TEXT_MESSAGE) ) {
		return 1;
	}
	sText = xrtLogText(&Record, &Config, &iSize);
	if ( sText == NULL ) {
		return 2;
	}
	printf("%.*s", (int)iSize, sText);
	xrtFree(sText);
	return 0;
}
