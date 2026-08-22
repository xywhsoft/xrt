#ifndef XRT_TASK_H
#define XRT_TASK_H

#include <xrt/future.h>



#if defined(XRT_FEATURE_TASK) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_TASK requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_TASK) && !defined(XRT_FEATURE_TEMP_MEMORY)
	#error "XRT_FEATURE_TASK requires XRT_FEATURE_TEMP_MEMORY"
#endif

#if defined(XRT_FEATURE_TASK_POOL) && !defined(XRT_FEATURE_TASK)
	#error "XRT_FEATURE_TASK_POOL requires XRT_FEATURE_TASK"
#endif

#if defined(XRT_FEATURE_TASK_POOL) && !defined(XRT_FEATURE_THREAD)
	#error "XRT_FEATURE_TASK_POOL requires XRT_FEATURE_THREAD"
#endif

#if defined(XRT_FEATURE_TASK_GROUP) && !defined(XRT_FEATURE_FUTURE)
	#error "XRT_FEATURE_TASK_GROUP requires XRT_FEATURE_FUTURE"
#endif

#if defined(XRT_FEATURE_TASK_GROUP_POOL) && !defined(XRT_FEATURE_TASK_GROUP)
	#error "XRT_FEATURE_TASK_GROUP_POOL requires XRT_FEATURE_TASK_GROUP"
#endif

#if defined(XRT_FEATURE_TASK_GROUP_POOL) && !defined(XRT_FEATURE_TASK_POOL)
	#error "XRT_FEATURE_TASK_GROUP_POOL requires XRT_FEATURE_TASK_POOL"
#endif

#if defined(XRT_FEATURE_TASK_COROUTINE) && !defined(XRT_FEATURE_TASK)
	#error "XRT_FEATURE_TASK_COROUTINE requires XRT_FEATURE_TASK"
#endif

#if \
	defined(XRT_FEATURE_TASK_COROUTINE) && \
	!defined(XRT_FEATURE_COROUTINE_SCHEDULER)
	#error "XRT_FEATURE_TASK_COROUTINE requires XRT_FEATURE_COROUTINE_SCHEDULER"
#endif

#if \
	defined(XRT_FEATURE_TASK_GROUP_COROUTINE) && \
	!defined(XRT_FEATURE_TASK_GROUP)
	#error "XRT_FEATURE_TASK_GROUP_COROUTINE requires XRT_FEATURE_TASK_GROUP"
#endif

#if \
	defined(XRT_FEATURE_TASK_GROUP_COROUTINE) && \
	!defined(XRT_FEATURE_TASK_COROUTINE)
	#error "XRT_FEATURE_TASK_GROUP_COROUTINE requires XRT_FEATURE_TASK_COROUTINE"
#endif



#if defined(XRT_FEATURE_TASK)

/* 任务过程必须显式说明成功、失败或协作取消，避免依赖残留错误状态。 */
typedef enum xtaskoutcome {
	XTASK_SUCCESS = 0,
	XTASK_FAILED = 1,
	XTASK_CANCELLED = 2
} xtaskoutcome;



/* 成功结果可以借用值，也可以把值及其析构过程转移给 Future。 */
typedef struct xtaskvalue {
	ptr Value;
	xfuturefreeproc Destroy;
	ptr DestroyData;
} xtaskvalue;



/* 任务过程借用取消令牌，并把成功值写入预先清零的结果结构。 */
typedef xtaskoutcome (*xtaskproc)(
	xcancel* pCancel,
	ptr pData,
	xtaskvalue* pResult
);



/* 提交参数控制父取消关系及任务数据在受理后的释放方式。 */
typedef struct xtaskargs {
	xcancel* Cancel;
	xfuturefreeproc Destroy;
	ptr DestroyData;
} xtaskargs;

#endif



#if defined(XRT_FEATURE_TASK_GROUP)

#define XRT_TASK_GROUP_CANCEL_ON_FAILED UINT32_C(0x00000001)
#define XRT_TASK_GROUP_CANCEL_ON_CANCELLED UINT32_C(0x00000002)
#define XRT_TASK_GROUP_CANCEL_ON_CLOSED UINT32_C(0x00000004)
#define XRT_TASK_GROUP_CANCEL_ON_STOPPED UINT32_C(0x00000007)



/* 任务组跟踪一组 Future，并在关闭且全部完成后发布唯一 Done Future。 */
typedef struct xtaskgroup xtaskgroup;



/* Future 启动器同步返回一个新引用，返回空时保留自己的结构化错误。 */
typedef xfuture* (*xtaskgroupstartproc)(ptr pData);



/* 全零配置表示不限活动项数量、不自动取消兄弟项且使用独立取消源。 */
typedef struct xtaskgroupconfig {
	xcancel* Cancel;
	size_t Limit;
	uint32 CancelOn;
} xtaskgroupconfig;



/* 任务组统计保留全部历史终态计数，但只为当前活动项占用节点内存。 */
typedef struct xtaskgroupstats {
	size_t Active;
	uint64 Added;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 Closed;
	uint64 Rejected;
	size_t FirstIndex;
	xfuturestate FirstState;
	bool Accepting;
	bool Cancelling;
} xtaskgroupstats;



XRT_EXTERN_C_BEGIN



/* 创建结构化任务组；配置为空时使用全零默认值。 */
XRT_API xtaskgroup* xrtTaskGroupCreate(const xtaskgroupconfig* pConfig);



/* 创建由父组跟踪的子组；父关闭时关闭子组，父取消时取消子组。 */
XRT_API xtaskgroup* xrtTaskGroupChild(
	xtaskgroup* pParent,
	const xtaskgroupconfig* pConfig
);



/* 跟踪一个 Future；成功时组保留到该 Future 终态为止的引用。 */
XRT_API bool xrtTaskGroupAdd(xtaskgroup* pGroup, xfuture* pFuture);



/* 先预留组槽位，再同步启动并跟踪 Future；失败时不会留下未登记操作。 */
XRT_API xfuture* xrtTaskGroupStart(
	xtaskgroup* pGroup,
	xtaskgroupstartproc pProc,
	ptr pData
);



/* 停止接纳新项，并让当前项及子组自然结束。 */
XRT_API bool xrtTaskGroupClose(xtaskgroup* pGroup);



/* 停止接纳新项，并向当前项及子组发出协作取消请求。 */
XRT_API bool xrtTaskGroupCancel(xtaskgroup* pGroup);



/* 返回增加引用后的 Done Future；它在组关闭且活动项归零时成功完成。 */
XRT_API xfuture* xrtTaskGroupFuture(const xtaskgroup* pGroup);



/* 关闭任务组并等待全部当前项进入终态。 */
XRT_API xwaitresult xrtTaskGroupWait(xtaskgroup* pGroup);



/* 关闭任务组并在相对微秒数内等待全部当前项。 */
XRT_API xwaitresult xrtTaskGroupWaitFor(xtaskgroup* pGroup, uint64 iTimeout);



/* 关闭任务组并等待到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtTaskGroupWaitUntil(
	xtaskgroup* pGroup,
	xdeadline iDeadline
);



/* 关闭任务组，并等待组完成、截止时间或调用方取消中的首个事件。 */
XRT_API xwaitresult xrtTaskGroupWaitUntilCancel(
	xtaskgroup* pGroup,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 复制当前负载、累计结果、首个异常槽位和生命周期状态。 */
XRT_API bool xrtTaskGroupGet(
	const xtaskgroup* pGroup,
	xtaskgroupstats* pStats
);



/* 返回首个失败项的借用结构化错误；任务组存活期间保持有效。 */
XRT_API const xerror* xrtTaskGroupError(const xtaskgroup* pGroup);



/* 返回增加引用后的组取消令牌。 */
XRT_API xcancel* xrtTaskGroupCancelToken(const xtaskgroup* pGroup);



/* 关闭并取消仍活动的项，随后以延迟回收方式释放任务组。 */
XRT_API void xrtTaskGroupDestroy(xtaskgroup* pGroup);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_TASK_POOL)

#define XRT_TASK_POOL_QUEUE_LIMIT_DEFAULT 1024u
#define XRT_TASK_POOL_THREAD_LIMIT 256u



/* 任务池对外保持不透明；销毁期间调用方必须停止其他并发访问。 */
typedef struct xtaskpool xtaskpool;



/* 全零配置使用逻辑处理器数量、默认队列上限和平台默认线程栈。 */
typedef struct xtaskpoolconfig {
	uint32 Threads;
	size_t QueueLimit;
	size_t StackSize;
} xtaskpoolconfig;



/* 统计快照区分瞬时负载、终态分布、拒绝量和生命周期状态。 */
typedef struct xtaskpoolstats {
	uint32 Threads;
	size_t QueueLimit;
	size_t Queued;
	size_t Running;
	uint64 Submitted;
	uint64 Completed;
	uint64 Succeeded;
	uint64 Failed;
	uint64 Cancelled;
	uint64 Rejected;
	bool Closed;
	bool Cancelling;
} xtaskpoolstats;



XRT_EXTERN_C_BEGIN



/* 创建有界工作线程池；配置为空或字段为零时使用对应默认值。 */
XRT_API xtaskpool* xrtTaskPoolCreate(const xtaskpoolconfig* pConfig);



/* 提交任务并返回其 Future；失败时任务数据所有权仍属于调用方。 */
XRT_API xfuture* xrtTaskSubmit(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 等待任务池出现队列槽位后提交；任务池工作线程不得阻塞等待所属池。 */
XRT_API xfuture* xrtTaskSubmitWait(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 在相对微秒数内等待任务池出现队列槽位并提交。 */
XRT_API xfuture* xrtTaskSubmitFor(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
);



/* 等待到指定单调时钟截止时间；槽位已经可用时成功优先于超时。 */
XRT_API xfuture* xrtTaskSubmitUntil(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
);



/* 等待槽位、截止时间或调用方取消；等待取消不取消已经受理的任务。 */
XRT_API xfuture* xrtTaskSubmitUntilCancel(
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 停止接收普通任务，并让已经受理的任务与内部资源回收过程自然排空。 */
XRT_API bool xrtTaskPoolClose(xtaskpool* pPool);



/* 停止接收新任务，取消排队任务并请求运行任务协作取消。 */
XRT_API bool xrtTaskPoolCancel(xtaskpool* pPool);



/* 等待已关闭任务池中的全部受理任务进入终态。 */
XRT_API xwaitresult xrtTaskPoolWait(xtaskpool* pPool);



/* 在相对微秒数内等待已关闭任务池排空。 */
XRT_API xwaitresult xrtTaskPoolWaitFor(xtaskpool* pPool, uint64 iTimeout);



/* 等待已关闭任务池排空到指定单调时钟截止时间。 */
XRT_API xwaitresult xrtTaskPoolWaitUntil(xtaskpool* pPool, xdeadline iDeadline);



/* 等待池排空、截止时间或调用方取消中的首个事件。 */
XRT_API xwaitresult xrtTaskPoolWaitUntilCancel(
	xtaskpool* pPool,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 复制任务池统计快照。 */
XRT_API bool xrtTaskPoolGet(const xtaskpool* pPool, xtaskpoolstats* pStats);



/* 关闭、排空、终止工作线程并释放任务池；工作线程不能销毁自身所属的池。 */
XRT_API bool xrtTaskPoolDestroy(xtaskpool* pPool);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_TASK_GROUP_POOL)

XRT_EXTERN_C_BEGIN



/* 立即向任务池提交并原子纳入组；队列已满时完整回滚组预留。 */
XRT_API xfuture* xrtTaskGroupSubmit(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 等待任务池槽位后提交；组取消会中止尚未受理的容量等待。 */
XRT_API xfuture* xrtTaskGroupSubmitWait(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs
);



/* 在相对微秒数内等待任务池槽位并原子纳入组。 */
XRT_API xfuture* xrtTaskGroupSubmitFor(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	uint64 iTimeout
);



/* 等待任务池槽位到指定单调时钟截止时间并原子纳入组。 */
XRT_API xfuture* xrtTaskGroupSubmitUntil(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline
);



/* 同时受截止时间、调用方取消和任务组取消约束地等待提交。 */
XRT_API xfuture* xrtTaskGroupSubmitUntilCancel(
	xtaskgroup* pGroup,
	xtaskpool* pPool,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_TASK_COROUTINE)

struct xcosched;



XRT_EXTERN_C_BEGIN



/* 从任意线程向指定或当前协程调度器提交任务，并返回独立生命周期的 Future。 */
XRT_API xfuture* xrtTaskCo(
	struct xcosched* pSched,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	size_t iStackSize
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_TASK_GROUP_COROUTINE)

XRT_EXTERN_C_BEGIN



/* 向协程调度器提交任务，并在同一预留窗口内原子纳入任务组。 */
XRT_API xfuture* xrtTaskGroupCo(
	xtaskgroup* pGroup,
	struct xcosched* pSched,
	xtaskproc pProc,
	ptr pData,
	const xtaskargs* pArgs,
	size_t iStackSize
);



XRT_EXTERN_C_END

#endif

#endif
