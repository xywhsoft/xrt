#ifndef XRT_INTERNAL_CONSOLE_H
#define XRT_INTERNAL_CONSOLE_H

#include "xrt_internal.h"
#include <xrt/console.h>

#include <stdio.h>



#if defined(XRT_FEATURE_CONSOLE)

/* 内部 Writer 在一次打开期间独占一个标准流，供多段输出保持原子边界。 */
typedef struct xconsolewriter {
	FILE* File;
	ptr Native;
	ptr Console;
} xconsolewriter;



/* 打开并锁定标准流；成功后必须调用 Close。 */
bool __xrtConsoleWriterOpen(
	xconsolestream Stream,
	xconsolewriter* pWriter
);



/* 向已锁定 Writer 写入 UTF-8 文本。 */
bool __xrtConsoleWriterWrite(
	xconsolewriter* pWriter,
	const void* pData,
	size_t iSize
);



/* 刷新已锁定 Writer。 */
bool __xrtConsoleWriterFlush(xconsolewriter* pWriter);



/* 解锁 Writer，不关闭进程标准流。 */
void __xrtConsoleWriterClose(xconsolewriter* pWriter);

#endif

#endif
