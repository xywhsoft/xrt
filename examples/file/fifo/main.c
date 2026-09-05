#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/fifo —— POSIX 命名管道（可裁剪能力探测）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFifoCreate    创建命名管道（mkfifo）
 *   XERR_UNSUPPORTED 平台不支持时的结构化错误类别
 * 模块宏：XRT_MODULE_FILE（FIFO 特性按平台裁剪）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/fifo/main.c -lws2_32 -liphlpapi
 * 预期输出（Windows）：
 *   FIFO is not supported on Windows
 * 预期输出（POSIX）：
 *   FIFO created
 *
 * 能力探测范式：可选功能失败后读错误类别——
 *   UNSUPPORTED 走降级路径（优雅退出），其他错误照常报。
 *   XRT 可裁剪特性在运行期都这样探测，不必编译期 ifdefs。
 */


/* 展示可裁剪的 POSIX FIFO 创建能力。 */
int main(void)
{
	static const char sPath[] = "xrt-fifo-example";

	#if defined(_WIN32) || defined(_WIN64)
		if ( !xrtFifoCreate(sPath, 0600u) &&
			(xrtGetError() != NULL) &&
			(xrtErrorKind(xrtGetError()) == XERR_UNSUPPORTED) ) {
			puts("FIFO is not supported on Windows");
			return 0;
		}
		return 1;
	#else
		(void)xrtFileDelete(sPath);
		xrtClearError();
		if ( !xrtFifoCreate(sPath, 0600u) ) {
			return 1;
		}
		puts("FIFO created");
		return xrtFileDelete(sPath) ? 0 : 1;
	#endif
}
