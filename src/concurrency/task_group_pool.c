#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK_GROUP_POOL)

/* 同步启动器参数只在 xrtTaskGroupStart 调用期间借用。 */
typedef struct xrt_task_group_pool_start {
	xtaskpool* Pool;
	xtaskproc Proc;
	ptr Data;
	const xtaskargs* Args;
	xdeadline Deadline;
	xcancel* Cancel;
	bool Wait;
} xrt_task_group_pool_start;



/* 把组取消转发到一次性的组合容量等待令牌。 */
static void __xrtTaskGroupPoolCancel(ptr pData)
{
	(void)xrtCancelRequest((xcancel*)pData);
}



/* 在预留槽位保护下调用立即或可等待的任务池提交路径。 */
static xfuture* __xrtTaskGroupPoolStart(ptr pData)
{
	xrt_task_group_pool_start* pStart =
		(xrt_task_group_pool_start*)pData;

	if ( !pStart->Wait ) {
		return xrtTaskSubmit(
			pStart->Pool,
			pStart->Proc,
			pStart->Data,
			pStart->Args
		);
	}
	return xrtTaskSubmitUntilCancel(
		pStart->Pool,
		pStart->Proc,
		pStart->Data,
		pStart->Args,
		pStart->Deadline,
		pStart->Cancel
	);
}



/* 保存失败错误，再释放组合取消对象，避免清理覆盖原始原因。 */
static xfuture* __xrtTaskGroupPoolFinish(
	xfuture* pFuture,
	xcancelwatch* pWatch,
	xcancel* pCombined,
	xcancel* pGroupCancel
)
{
	xerror* pError = pFuture == NULL ? xrtTakeError() : NULL;

	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pCombined);
	xrtCancelDestroy(pGroupCancel);
	if ( pFuture == NULL ) {
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtErrorSetInternal();
		}
	}
	return pFuture;
}



/* 统一实现立即提交和带组合取消的容量等待提交。 */
static xfuture* __xrtTaskGroupPoolSubmit(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	bool bWait,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xrt_task_group_pool_start tStart;
	xcancel* pGroupCancel = NULL;
	xcancel* pCombined = NULL;
	xcancelwatch* pWatch = NULL;
	xfuture* pFuture;
	xerror* pError;

	if ( (pGroup == NULL) || (pPool == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&tStart, 0, sizeof(tStart));
	tStart.Pool = pPool;
	tStart.Proc = pProc;
	tStart.Data = pData;
	tStart.Args = pArgs;
	tStart.Deadline = iDeadline;
	tStart.Wait = bWait;
	if ( !bWait ) {
		return xrtTaskGroupStart(
			pGroup,
			__xrtTaskGroupPoolStart,
			&tStart
		);
	}

	/* 子令牌继承调用方取消，监听再把组取消并入同一个等待源。 */
	pGroupCancel = xrtTaskGroupCancelToken(pGroup);
	if ( pGroupCancel == NULL ) {
		return NULL;
	}
	pCombined = xrtCancelChild(pCancel);
	if ( pCombined == NULL ) {
		xrtCancelDestroy(pGroupCancel);
		return NULL;
	}
	pWatch = xrtCancelWatch(
		pGroupCancel,
		__xrtTaskGroupPoolCancel,
		pCombined
	);
	if ( pWatch == NULL ) {
		pError = xrtTakeError();
		xrtCancelDestroy(pCombined);
		xrtCancelDestroy(pGroupCancel);
		if ( pError != NULL ) {
			xrtSetError(pError);
			xrtErrorFree(pError);
		} else {
			__xrtErrorSetInternal();
		}
		return NULL;
	}
	tStart.Cancel = pCombined;
	pFuture = xrtTaskGroupStart(
		pGroup,
		__xrtTaskGroupPoolStart,
		&tStart
	);
	return __xrtTaskGroupPoolFinish(
		pFuture,
		pWatch,
		pCombined,
		pGroupCancel
	);
}



/* 立即提交并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupSubmit(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	return __xrtTaskGroupPoolSubmit(
		pGroup,
		pPool,
		pProc,
		pData,
		pArgs,
		false,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 永久等待任务池槽位并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupSubmitWait(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	return xrtTaskGroupSubmitUntilCancel(
		pGroup,
		pPool,
		pProc,
		pData,
		pArgs,
		XRT_DEADLINE_NEVER,
		NULL
	);
}



/* 在相对微秒数内等待任务池槽位并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupSubmitFor(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
)
{
	return xrtTaskGroupSubmitUntilCancel(
		pGroup,
		pPool,
		pProc,
		pData,
		pArgs,
		xrtDeadlineAfter(iTimeout),
		NULL
	);
}



/* 等待任务池槽位到指定单调时钟截止时间并原子纳入组。 */
XRT_API xfuture* xrtTaskGroupSubmitUntil(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
)
{
	return xrtTaskGroupSubmitUntilCancel(
		pGroup,
		pPool,
		pProc,
		pData,
		pArgs,
		iDeadline,
		NULL
	);
}



/* 同时受截止时间、调用方取消和组取消约束地等待提交。 */
XRT_API xfuture* xrtTaskGroupSubmitUntilCancel(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtTaskGroupPoolSubmit(
		pGroup,
		pPool,
		pProc,
		pData,
		pArgs,
		true,
		iDeadline,
		pCancel
	);
}

#endif
