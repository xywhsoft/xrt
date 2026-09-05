/*
 * 范例：string/format_tour —— Format 双入口：直接版与 va_list 版
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFormat      printf 风格格式化 → 拥有式字符串
 *   xrtFormatV     va_list 版（包装可变参数转发的基础入口）
 * 模块宏：XRT_MODULE_STRING（FORMAT 特性）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/string/format_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   direct=id=7 ok=true
 *   wrapped=x=1.50 tag=core
 *
 * xrtFormat 是日志/模板/HTTP 层的公共地基；FormatV 是
 *   "自己写可变参数包装函数"时的转发目标（本例 appendTag
 *   演示这个姿势——与 StrBufAppendFormatV 同理）。
 */

#include <stdio.h>
#include <stdarg.h>
#include <xrt.h>

/* 业务包装：固定前缀 + 任意格式化参数 → 拥有式结果。 */
static str makeReport(cstr sTag, ...)
{
	va_list Args;
	str sBody;

	va_start(Args, sTag);
	sBody = xrtFormatV("x=%s", Args);
	va_end(Args);
	return sBody;
}

int main(void)
{
	str sDirect = xrtFormat("id=%d ok=%s", 7, "true");
	printf("direct=%s\n", sDirect ? sDirect : "(null)");
	xrtFree(sDirect);

	/* va_list 版：注意浮点经可变参数提升为 double。 */
	str sWrapped = makeReport("1.50", "core");
	printf("wrapped=%s\n", sWrapped ? sWrapped : "(null)");
	xrtFree(sWrapped);
	return 0;
}
