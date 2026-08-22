#ifndef XRT_INTERNAL_TASK_H
#define XRT_INTERNAL_TASK_H

#include "xrt_temp.h"



#if defined(XRT_FEATURE_TASK)

/* 任务作业集中保存执行、结果端点、取消令牌和受理后的数据所有权。 */
typedef struct xrt_task_job {
	struct xrt_task_job* Next;
	struct xrt_task_job* RunningPrevious;
	struct xrt_task_job* RunningNext;
	xtaskproc Proc;
	ptr Data;
	xfuturefreeproc Destroy;
	ptr DestroyData;
	xfuture* Future;
	xpromise* Promise;
	xcancel* Cancel;
	bool CancelPosted;
} xrt_task_job;



/* 创建尚未被执行器受理的任务作业和消费端 Future。 */
xrt_task_job* __xrtTaskCreate(
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xfuture** ppFuture
);



/* 执行任务或在预先取消时跳过用户过程，并完成 Future。 */
void __xrtTaskRun(xrt_task_job* pJob);



/* 不执行用户过程，直接把任务完成为取消终态。 */
void __xrtTaskCancel(xrt_task_job* pJob);



/* 不执行用户过程，以指定错误或 INTERNAL 完成失败终态。 */
void __xrtTaskFail(xrt_task_job* pJob, const xerror* pError);



/* 返回任务 Future 当前终态快照。 */
xfuturestate __xrtTaskState(const xrt_task_job* pJob);



/* 释放任务作业；受理成功时同时释放任务数据。 */
void __xrtTaskDestroy(xrt_task_job* pJob, bool bDestroyData);

#endif



#if defined(XRT_FEATURE_TASK_POOL)

/* 资源回收节点嵌入资源对象，投递过程不分配内存，也不占用普通任务队列。 */
typedef void (*xrt_task_finalizer_proc)(ptr pData);



typedef struct xrt_task_finalizer {
	struct xrt_task_finalizer* Next;
	xrt_task_finalizer_proc Proc;
	ptr Data;
} xrt_task_finalizer;



/*
	投递已经受理资源的最终回收过程。
	节点只能投递一次，任务池必须存活到回收过程执行完毕。
*/
void __xrtTaskPoolFinalize(
	xtaskpool* pPool,
	xrt_task_finalizer* pFinalizer,
	xrt_task_finalizer_proc pProc,
	ptr pData
);

#endif

#endif
