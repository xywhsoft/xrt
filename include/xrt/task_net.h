#ifndef XRT_TASK_NET_H
#define XRT_TASK_NET_H

#include <xrt/net.h>
#include <xrt/task.h>



#if defined(XRT_FEATURE_TASK_NET) && !defined(XRT_FEATURE_TASK)
	#error "XRT_FEATURE_TASK_NET requires XRT_FEATURE_TASK"
#endif

#if defined(XRT_FEATURE_TASK_NET) && !defined(XRT_FEATURE_NET_ENGINE)
	#error "XRT_FEATURE_TASK_NET requires XRT_FEATURE_NET_ENGINE"
#endif

#if defined(XRT_FEATURE_TASK_NET) && !defined(XRT_FEATURE_FUTURE_BRIDGE)
	#error "XRT_FEATURE_TASK_NET requires XRT_FEATURE_FUTURE_BRIDGE"
#endif

#if \
	defined(XRT_FEATURE_TASK_GROUP_NET) && \
	!defined(XRT_FEATURE_TASK_GROUP)
	#error "XRT_FEATURE_TASK_GROUP_NET requires XRT_FEATURE_TASK_GROUP"
#endif

#if \
	defined(XRT_FEATURE_TASK_GROUP_NET) && \
	!defined(XRT_FEATURE_TASK_NET)
	#error "XRT_FEATURE_TASK_GROUP_NET requires XRT_FEATURE_TASK_NET"
#endif



#if defined(XRT_FEATURE_TASK_NET)

/* 网络任务过程额外借用亲和 Worker，并复用任务核心的取消和结果合同。 */
typedef xtaskoutcome (*xtasknetproc)(
	xnetworker* pWorker,
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
);



XRT_EXTERN_C_BEGIN



/* 向指定亲和 Worker 提交任务；任务过程不得阻塞网络事件循环。 */
XRT_API xfuture* xrtTaskNet(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 在相对微秒数到期后向指定亲和 Worker 提交任务。 */
XRT_API xfuture* xrtTaskNetAfter(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
);



/* 在指定单调时钟截止时间到期后向亲和 Worker 提交任务。 */
XRT_API xfuture* xrtTaskNetUntil(
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_TASK_GROUP_NET)

XRT_EXTERN_C_BEGIN



/* 向亲和 Worker 提交任务，并在同一预留窗口内原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNet(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 延迟提交网络任务，并在同一预留窗口内原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNetAfter(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
);



/* 按单调截止时间提交网络任务，并原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupNetUntil(
	xtaskgroup* pGroup,
	xnetengine* pEngine,
	uint64 iAffinity,
	xtasknetproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
);



XRT_EXTERN_C_END

#endif

#endif
