#ifndef XRT_CONSOLE_H
#define XRT_CONSOLE_H

#include <xrt/core.h>
#include <xrt/error.h>



#if defined(XRT_FEATURE_CONSOLE)

/* 标准输出流名称跨平台稳定，不直接暴露 FILE 或原生句柄。 */
typedef enum xconsolestream {
	XCONSOLE_STDOUT = 1,
	XCONSOLE_STDERR
} xconsolestream;



/* Console 错误代码在 xrt.console 域内稳定。 */
typedef enum xconsoleerror {
	XCONSOLE_ERROR_STREAM = 1,
	XCONSOLE_ERROR_UTF8,
	XCONSOLE_ERROR_WRITE,
	XCONSOLE_ERROR_FLUSH
} xconsoleerror;



XRT_EXTERN_C_BEGIN



/* 写入 UTF-8 文本；真实 Windows 控制台转换为 UTF-16，重定向时保留原始 UTF-8 字节。 */
XRT_API bool xrtConsoleWrite(xconsolestream Stream, xstrview Text);



/* 原子写入 UTF-8 文本和一个换行符。 */
XRT_API bool xrtConsoleWriteLine(xconsolestream Stream, xstrview Text);



/* 刷新指定标准输出流。 */
XRT_API bool xrtConsoleFlush(xconsolestream Stream);



/* 判断指定标准输出流当前是否连接交互终端；非终端是正常结果，不设置错误。 */
XRT_API bool xrtConsoleIsTerminal(xconsolestream Stream);



XRT_EXTERN_C_END

#endif

#endif
