#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK_GROUP_COROUTINE)

/* 协程启动参数只在同步预留调用期间借用。 */
typedef struct xrt_task_group_co_start {
	struct xcosched* Sched;
	xtaskproc Proc;
	ptr Data;
	const xtaskargs* Args;
	size_t StackSize;
} xrt_task_group_co_start;



/* 在任务组预留槽位保护下提交协程任务。 */
static xfuture* __xrtTaskGroupCoStart(ptr pData)
{
	xrt_task_group_co_start* pStart =
		(xrt_task_group_co_start*)pData;

	return xrtTaskCo(
		pStart->Sched,
		pStart->Proc,
		pStart->Data,
		pStart->Args,
		pStart->StackSize
	);
}



/* 向协程调度器提交并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupCo(
	xtaskgroup* pGroup,
	struct xcosched* pSched,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	size_t iStackSize
)
{
	xrt_task_group_co_start tStart;

	if ( (pGroup == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&tStart, 0, sizeof(tStart));
	tStart.Sched = pSched;
	tStart.Proc = pProc;
	tStart.Data = pData;
	tStart.Args = pArgs;
	tStart.StackSize = iStackSize;
	return xrtTaskGroupStart(
		pGroup,
		__xrtTaskGroupCoStart,
		&tStart
	);
}

#endif
