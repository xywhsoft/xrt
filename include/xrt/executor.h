#ifndef XRT_EXECUTOR_H
#define XRT_EXECUTOR_H

#include <xrt/wait.h>



#if defined(XRT_FEATURE_EXECUTOR) && !defined(XRT_FEATURE_ATOMIC)
	#error "XRT_FEATURE_EXECUTOR requires XRT_FEATURE_ATOMIC"
#endif

#if defined(XRT_FEATURE_EXECUTOR) && !defined(XRT_FEATURE_TEMP_MEMORY)
	#error "XRT_FEATURE_EXECUTOR requires XRT_FEATURE_TEMP_MEMORY"
#endif

#if defined(XRT_FEATURE_EXECUTOR) && !defined(XRT_FEATURE_THREAD)
	#error "XRT_FEATURE_EXECUTOR requires XRT_FEATURE_THREAD"
#endif

#if defined(XRT_FEATURE_EXECUTOR) && !defined(XRT_FEATURE_COND)
	#error "XRT_FEATURE_EXECUTOR requires XRT_FEATURE_COND"
#endif



#if defined(XRT_FEATURE_EXECUTOR)

#define XRT_EXECUTOR_QUEUE_LIMIT_DEFAULT ((size_t)1024u)
#define XRT_EXECUTOR_THREAD_LIMIT 256u



/* Executor 是无 Future 的有界高吞吐执行器，与传播结果和取消的 TaskPool 分工。 */
typedef struct xexecutor xexecutor;



/* 工作过程不返回结果；错误上下文只在当前工作内有效，返回后由执行器清理。 */
typedef void (*xexecutorproc)(ptr pData);



/* 工作数据析构在过程返回或排队工作被取消后执行一次。 */
typedef void (*xexecutorfreeproc)(ptr pData, ptr pContext);



/* 批量描述符只在提交调用期间借用，成功后各项数据责任转移给执行器。 */
typedef struct xexecutoritem {
	xexecutorproc Proc;
	ptr Data;
	xexecutorfreeproc Destroy;
	ptr DestroyContext;
} xexecutoritem;



/* QueueLimit 是每个 Worker 的硬队列上限；总排队容量等于 Threads 乘 QueueLimit。 */
typedef struct xexecutorconfig {
	uint32 Threads;
	size_t QueueLimit;
	size_t StackSize;
} xexecutorconfig;



/* 统计区分瞬时负载、累计受理、执行、窃取、取消和拒绝数量。 */
typedef struct xexecutorstats {
	uint32 Threads;
	size_t QueueLimit;
	size_t Queued;
	size_t Running;
	uint64 Submitted;
	uint64 Completed;
	uint64 Executed;
	uint64 Stolen;
	uint64 Cancelled;
	uint64 Rejected;
	bool Closed;
	bool Cancelling;
} xexecutorstats;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_EXECUTOR)

/* 创建预分配作业槽、Worker 本地队列和工作窃取线程。 */
XRT_API xexecutor* xrtExecutorCreate(const xexecutorconfig* pConfig);



/* 提交一个 detached 工作；成功后接管数据析构，失败时所有权仍属于调用方。 */
XRT_API bool xrtExecutorSubmit(
	xexecutor* pExecutor,
	xexecutorproc pProc,
	ptr pData,
	xexecutorfreeproc pDestroy,
	ptr pDestroyContext
);



/* 原子提交一组 detached 工作；整组成功或整组失败，不保证执行顺序。 */
XRT_API bool xrtExecutorSubmitBatch(
	xexecutor* pExecutor,
	const xexecutoritem* pItems,
	size_t iCount
);



/* 停止受理新工作，并让已经受理的工作自然排空。 */
XRT_API bool xrtExecutorClose(xexecutor* pExecutor);



/* 停止受理，丢弃尚未开始的工作并执行其数据析构；运行中工作不会被强停。 */
XRT_API bool xrtExecutorCancel(xexecutor* pExecutor);



/* 永久等待已经关闭的执行器排空；Executor Worker 不能等待自身。 */
XRT_API xwaitresult xrtExecutorWait(xexecutor* pExecutor);



/* 在相对微秒数内等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWaitFor(xexecutor* pExecutor, uint64 iTimeout);



/* 等待到指定单调时钟截止时间；已排空优先于超时。 */
XRT_API xwaitresult xrtExecutorWaitUntil(
	xexecutor* pExecutor,
	xdeadline iDeadline
);



/* 复制当前负载和累计统计快照。 */
XRT_API bool xrtExecutorGet(
	const xexecutor* pExecutor,
	xexecutorstats* pStats
);



/* 关闭、排空、停止 Worker 并释放执行器；Worker 不能销毁自身执行器。 */
XRT_API bool xrtExecutorDestroy(xexecutor* pExecutor);

#endif



XRT_EXTERN_C_END

#endif
