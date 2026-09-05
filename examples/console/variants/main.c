/*
 * 范例：console/variants —— 控制台补遗：Write 与终端探测
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtConsoleWrite        写 UTF-8 文本（不加换行）
 *   xrtConsoleIsTerminal   流当前是否交互终端
 * 模块宏：XRT_MODULE_CONSOLE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/console/variants/main.c -lws2_32 -liphlpapi
 * 预期输出（重定向时 is-terminal=0；真终端为 1）：
 *   write-ok=1
 *   is-terminal=0
 *
 * Write 与 WriteLine 的差别：不自动补换行——进度条、
 *   表格行内更新这类"同一行续写"场景用它。
 *   IsTerminal 用于决定是否输出 ANSI 颜色（管道里别刷色码）。
 */

#include <stdio.h>
#include <xrt.h>

int main(void)
{
	bool bOk = xrtConsoleWrite(XCONSOLE_STDOUT, XRT_STR_LITERAL("write-ok"));

	printf("=1\n");
	printf("is-terminal=%d\n",
		xrtConsoleIsTerminal(XCONSOLE_STDOUT) ? 1 : 0);
	return bOk ? 0 : 1;
}
