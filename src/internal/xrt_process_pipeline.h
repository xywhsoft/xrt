#ifndef XRT_INTERNAL_PROCESS_PIPELINE_H
#define XRT_INTERNAL_PROCESS_PIPELINE_H

#include "xrt_process_run.h"



#if defined(XRT_FEATURE_PROCESS_PIPELINE)

/* 平台连接管道只由 Pipeline 编排层持有，并在全部 Spawn 后关闭。 */
typedef struct xprocesspipe {
	intptr_t Read;
	intptr_t Write;
} xprocesspipe;



/* 创建一条不可被无关子进程继承的真实 OS pipe。 */
bool __xrtProcessPipeCreate(xprocesspipe* pPipe);



/* 幂等关闭管道两端。 */
void __xrtProcessPipeClose(xprocesspipe* pPipe);

#endif

#endif
