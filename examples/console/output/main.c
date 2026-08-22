#include <xrt.h>



/* 分别向标准输出和标准错误写入 UTF-8 文本。 */
int main(void)
{
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
	return xrtConsoleFlush(XCONSOLE_STDERR) ? 0 : 2;
}
