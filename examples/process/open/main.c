#include <xrt.h>

#include <stdio.h>



/*
 * 范例：process/open —— 系统默认程序打开文件或 URI
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtProcessOpen          跨平台"用默认程序打开"
 *   xrtErrorSystemCode      读取错误的系统码（打开失败诊断）
 * 模块宏：XRT_MODULE_PROCESS
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c ${BS}
 *       examples/process/open/main.c -lws2_32 -liphlpapi
 * 用法：
 *   process-open <file-or-uri>
 * 预期输出：
 *   usage: process-open <file-or-uri>（无参数时）
 *
 * 底层分派：Windows ShellExecute / macOS open / Linux xdg-open。
 * 典型用途："报告已生成"后直接帮用户拉起浏览器/PDF 阅读器。
 * 失败信息带系统码——文件不存在/无关联程序一眼可辨。
 */


/* 使用系统默认关联程序打开命令行给出的文件或 URI。 */
int main(int argc, char** argv)
{
	if ( argc != 2 ) {
		printf("usage: process-open <file-or-uri>\n");
		return 0;
	}
	if ( !xrtProcessOpen(argv[1]) ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"open failed: %s (system=%d)\n",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error",
			pError != NULL ? xrtErrorSystemCode(pError) : 0
		);
		return 1;
	}
	return 0;
}
