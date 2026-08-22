#ifndef XRT_INTERNAL_PROCESS_RUN_H
#define XRT_INTERNAL_PROCESS_RUN_H

#include "xrt_process.h"

#include <xrt/buffer.h>
#include <xrt/cancel.h>



#if defined(XRT_FEATURE_PROCESS_RUN)

/* 按统一策略把一块输出追加到有界捕获窗口。 */
bool __xrtProcessCaptureAppend(
	xbuffer* pBuffer,
	size_t iLimit,
	xprocessoverflow Overflow,
	bool* pTruncated,
	xbytesview Data
);



/* 对单个仍在运行的进程执行 Interrupt、Terminate、KillTree 分级停止。 */
bool __xrtProcessRunStop(
	xprocess* pProcess,
	uint64 iGrace
);

#endif

#endif
