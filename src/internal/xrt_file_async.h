#ifndef XRT_INTERNAL_FILE_ASYNC_H
#define XRT_INTERNAL_FILE_ASYNC_H

#include "xrt_file.h"
#include "xrt_task.h"
#include <xrt/file_async.h>



#if defined(XRT_FEATURE_FILE_ASYNC_COMMON)

/* 统一包装异步文件操作的同步底座错误。 */
void __xrtFileAsyncError(
	xfileasyncerror Code,
	cstr sOperation,
	cstr sMessage
);



/* 提交拥有型任务；拒绝时同步析构数据并保留外层错误。 */
xfuture* __xrtFileAsyncSubmit(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	xfuturefreeproc pDestroy
);



/* 释放单次分配的 Future 值。 */
void __xrtFileAsyncValueFree(ptr pValue, ptr pData);



/* 连续分配任务头和一至两条路径，并返回任务拥有的路径快照。 */
ptr __xrtFileAsyncPathTaskCreate(
	size_t iTaskSize,
	cstr sSource,
	cstr sTarget,
	cstr* pSource,
	cstr* pTarget
);

#endif

#endif
