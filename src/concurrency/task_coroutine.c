#include "../internal/xrt_task.h"
#include "../internal/xrt_coroutine.h"



#if defined(XRT_FEATURE_TASK_COROUTINE)

/* 跨线程提交先进入调度器投递队列，再由所属线程创建真正的协程。 */
typedef struct xrt_task_co_post {
	xrt_task_job* Job;
	size_t StackSize;
} xrt_task_co_post;



/* 协程用户过程复用任务核心的显式结果和隔离执行上下文。 */
static ptr __xrtTaskCoRun(ptr pData)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;

	__xrtTaskRun(pJob);
	return pJob;
}



/* 无论用户过程是否进入，都完成遗留 Future 并释放受理后的任务资源。 */
static void __xrtTaskCoFinalize(
	xcoroterm Term,
	ptr pResult,
	const xerror* pError,
	ptr pData
)
{
	xrt_task_job* pJob = (xrt_task_job*)pData;

	(void)pResult;
	if ( __xrtTaskState(pJob) == XFUTURE_PENDING ) {
		if ( Term == XCORO_TERM_CANCELLED ) {
			__xrtTaskCancel(pJob);
		} else if ( Term == XCORO_TERM_ERROR ) {
			__xrtTaskFail(pJob, pError);
		} else {
			__xrtTaskFail(pJob, NULL);
		}
	}
	__xrtTaskDestroy(pJob, true);
}



/* 在所属线程直接创建任务协程，避免常用路径产生额外投递分配。 */
static bool __xrtTaskCoSpawn(
	xcosched* pSched,
	xrt_task_job* pJob,
	size_t iStackSize
)
{
	xcoroargs tCoArgs;

	memset(&tCoArgs, 0, sizeof(tCoArgs));
	tCoArgs.StackSize = iStackSize;
	tCoArgs.Cancel = pJob->Cancel;
	tCoArgs.Finalize = __xrtTaskCoFinalize;
	tCoArgs.FinalizeData = pJob;
	return xrtCoGo(pSched, __xrtTaskCoRun, pJob, &tCoArgs);
}



/* 在调度器所属线程创建任务协程，失败时把结构化错误写入任务 Future。 */
static void __xrtTaskCoPost(xcosched* pSched, ptr pData)
{
	xrt_task_co_post* pPost = (xrt_task_co_post*)pData;
	xrt_task_job* pJob = pPost->Job;
	xerror* pError;

	if ( __xrtTaskCoSpawn(pSched, pJob, pPost->StackSize) ) {
		pPost->Job = NULL;
		return;
	}
	pError = xrtTakeError();
	__xrtTaskFail(pJob, pError);
	xrtErrorFree(pError);
}



/* 释放调度器已经受理但未交给协程终结器的任务。 */
static void __xrtTaskCoPostDestroy(ptr pData)
{
	xrt_task_co_post* pPost = (xrt_task_co_post*)pData;

	if ( pPost->Job != NULL ) {
		if ( __xrtTaskState(pPost->Job) == XFUTURE_PENDING ) {
			__xrtTaskFail(pPost->Job, NULL);
		}
		__xrtTaskDestroy(pPost->Job, true);
	}
	xrtFree(pPost);
}



/* 把任务提交到指定或当前协程调度器。 */
XRT_API xfuture* xrtTaskCo(
	xcosched* pSched,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	size_t iStackSize
)
{
	xrt_task_job* pJob;
	xrt_task_co_post* pPost;
	xfuture* pFuture;

	if ( pProc == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( pSched == NULL ) {
		pSched = xrtCoSchedCurrent();
	}
	if ( pSched == NULL ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pJob = __xrtTaskCreate(pProc, pData, pArgs, &pFuture);
	if ( pJob == NULL ) {
		return NULL;
	}
	if ( __xrtCoSchedIsOwner(pSched) ) {
		if ( !__xrtTaskCoSpawn(pSched, pJob, iStackSize) ) {
			__xrtTaskDestroy(pJob, false);
			return NULL;
		}
		return pFuture;
	}
	pPost = (xrt_task_co_post*)xrtMalloc(sizeof(xrt_task_co_post));
	if ( pPost == NULL ) {
		__xrtTaskDestroy(pJob, false);
		return NULL;
	}
	pPost->Job = pJob;
	pPost->StackSize = iStackSize;
	if ( !xrtCoSchedPostOwned(
		pSched,
		__xrtTaskCoPost,
		pPost,
		__xrtTaskCoPostDestroy
	) ) {
		xrtFree(pPost);
		__xrtTaskDestroy(pJob, false);
		return NULL;
	}
	return pFuture;
}

#endif
