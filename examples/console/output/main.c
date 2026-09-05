/*
 * 范例：console/output —— 向标准输出/错误流写入 UTF-8 文本
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtConsoleWriteLine  把整行 UTF-8 文本写入指定控制流（自动补换行）
 *   xrtConsoleFlush      冲刷指定流的缓冲（服务日志收尾必备）
 *   XCONSOLE_STDOUT / XCONSOLE_STDERR  两个目标流
 * 模块宏：XRT_MODULE_CONSOLE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/console/output/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   （stdout）service started
 *   （stderr）example diagnostic
 *
 * 为什么不用 printf：控制台 API 处理的是 UTF-8 视图，
 * 在 Windows 上正确走宽字符控制台路径（避免代码页乱码），
 * 并且不引入 printf 的格式化开销——格式化交给 xrtFormat。
 */

#include <xrt.h>



int main(void)
{
	/* 正常日志走 stdout；诊断信息走 stderr，两者不会被混流重排。 */
	if (
		!xrtConsoleWriteLine(
			XCONSOLE_STDOUT,
			XRT_STR_LITERAL("service started")
		) ||
		!xrtConsoleWriteLine(
			XCONSOLE_STDERR,
			XRT_STR_LITERAL("example diagnostic")
		)
	) {
		return 1;
	}

	/* 程序退出前显式冲刷，保证缓冲中的诊断信息一定落地。 */
	return xrtConsoleFlush(XCONSOLE_STDERR) ? 0 : 2;
}
