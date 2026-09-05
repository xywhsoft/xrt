/*
 * 范例：string/glob —— 严格 UTF-8 通配匹配：? * 与字符组
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtStrGlob          通配匹配（? 单标量、* 任意串、[组]）
 *   XSTR_GLOB_CASE_ASCII  仅 ASCII 字母忽略大小写
 * 模块宏：XRT_MODULE_STRING
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/string/glob/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   matched
 *
 * 匹配的三个要点（本例全覆盖）：
 *   report vs Report —— CASE_ASCII 标志下 ASCII 不区分大小写；
 *   ?              —— 恰好一个 Unicode 标量（"你"，非一个字节！）；
 *   [tT][xX][tT]    —— 字符组兜底 TXT 的大小写组合。
 * "严格 UTF-8"：非法序列直接失败而非误匹配；
 *   不需要正则的场合（文件名过滤、配置项选择）别上正则
 *   （第 82 章的正则模块才是完整方案）。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	xstrview Name = XRT_STR_LITERAL("Report-你.TXT");
	xstrview Pattern = XRT_STR_LITERAL("report-?.[tT][xX][tT]");

	printf("%s\n", xrtStrGlob(Name, Pattern, XSTR_GLOB_CASE_ASCII) ?
		"matched" : "not matched");
	return 0;
}
