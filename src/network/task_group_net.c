#include "../internal/xrt_task.h"



#if defined(XRT_FEATURE_TASK_GROUP_NET)

/* 网络任务启动参数只在 TaskGroup 的同步预留调用期间借用。 */
typedef struct xrt_task_group_net_start {
	xnetengine* Engine;
	uint64 Affinity;
	xtasknetproc Proc;
	ptr Data;
	const xtaskargs* Args;
	xdeadline Deadline;
	bool Delayed;
} xrt_task_group_net_start;



/* 在任务组预留槽位保护下提交立即或延迟网络任务。 */
static xfuture* __xrtTaskGroupNetStart(ptr pData)
{
	xrt_task_group_net_start* pStart =
		(xrt_task_group_net_start*)pData;

	if ( pStart->Delayed ) {
		return xrtTaskNetUntil(
			pStart->Engine,
			pStart->Affinity,
			pStart->Proc,
			pStart->Data,
			pStart->Args,
			pStart->Deadline
		);
	}
	return xrtTaskNet(
		pStart->Engine,
		pStart->Affinity,
		pStart->Proc,
		pStart->Data,
		pStart->Args
	);
}



/* 统一构造 TaskGroup 的网络任务预留参数。 */
static xfuture* __xrtTaskGroupNet(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline,
	bool bDelayed
)
{
	xrt_task_group_net_start tStart;

	if ( (pGroup == NULL) || (pEngine == NULL) || (pProc == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	memset(&tStart, 0, sizeof(tStart));
	tStart.Engine = pEngine;
	tStart.Affinity = iAffinity;
	tStart.Proc = pProc;
	tStart.Data = pData;
	tStart.Args = pArgs;
	tStart.Deadline = iDeadline;
	tStart.Delayed = bDelayed;
	return xrtTaskGroupStart(
		pGroup,
		__xrtTaskGroupNetStart,
		&tStart
	);
}



/* 提交立即网络任务并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNet(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs
)
{
	return __xrtTaskGroupNet(
		pGroup,
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		0,
		false
	);
}



/* 延迟提交网络任务并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNetAfter(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
)
{
	return __xrtTaskGroupNet(
		pGroup,
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		xrtDeadlineAfter(iTimeout),
		true
	);
}



/* 按单调截止时间提交网络任务并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNetUntil(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
)
{
	return __xrtTaskGroupNet(
		pGroup,
		pEngine,
		iAffinity,
		pProc,
		pData,
		pArgs,
		iDeadline,
		true
	);
}

#endif
