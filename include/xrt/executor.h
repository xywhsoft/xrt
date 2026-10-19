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

/* Resident contract for the ONE Data reference transferred on acceptance.
 * Proc borrows Data; Drop(Data,NULL) consumes it exactly once outside mutation
 * and all native locks. Ops identifies that same physical Data node. Descriptor,
 * Ops and callbacks outlive every accepted slot, including collector pins after
 * work completes. A failed submission does not consume Data. */
typedef struct xexecutordataownershipv1 {
	size_t size;
	xexecutorproc Proc;
	xexecutorfreeproc Drop;
	const xrtownershipops* Ops;
} xexecutordataownershipv1;

typedef struct xexecutorowneditemv1 {
	ptr Data;
	const xexecutordataownershipv1* Policy;
} xexecutorowneditemv1;

/* One actual executor-owned context reference, shared by its workers. Exit
 * borrows it on each REAL worker exactly once, after all accepted work/drop
 * tails, outside native locks/mutation and before publishing worker exit.
 * Drop(Data,NULL) consumes it once AFTER every worker has joined. Callbacks,
 * Ops and policy are immutable resident code; no generated descriptor/code
 * lifetime is inferred. Context nodes require independent graph admission. */
typedef struct xexecutorworkerexitownershipv1 {
	size_t size;
	xexecutorproc Exit;
	xexecutorfreeproc Drop;
	const xrtownershipops* Ops;
} xexecutorworkerexitownershipv1;



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

/* Success consumes ONE Data reference. Failure consumes none and invokes no
 * Exit/Drop, including partial native-thread creation rollback. Unlike plain
 * Create, Close+drain stops these workers; Wait includes every Exit tail.
 * Native queue statistics still count jobs only. Exit records semantic errors
 * in its owned context; it cannot change physical completion into a lost
 * native creator. NULL Data/policy are invalid; use plain Create without it. */
XRT_API xexecutor* xrtExecutorCreateOwnedExitV1(const xexecutorconfig* pConfig,
	ptr pData, const xexecutorworkerexitownershipv1* pPolicy);



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

/* Same bounded, allocation-free admission as Submit/SubmitBatch. Batch is
 * all-or-nothing; each item transfers a separate actual Data reference. */
XRT_API bool xrtExecutorSubmitOwnedV1(xexecutor* pExecutor, ptr pData,
	const xexecutordataownershipv1* pPolicy);
XRT_API bool xrtExecutorSubmitOwnedBatchV1(xexecutor* pExecutor,
	const xexecutorowneditemv1* pItems, size_t iCount);

/* Read under the entire graph's exclusive ownership freeze. Queued slots own
 * their actual Data and one containing Executor reference. Active callbacks,
 * cleanup tails and native entries refuse Count. Legacy opaque queued items
 * refuse admission; worker/control allocations are uniquely contained, not
 * fabricated reference-counted nodes. Preparation closes admission and drains
 * original work, then joins at zero timeout outside freeze; it never cancels
 * work to manufacture readiness. Clear requires physical join. */
XRT_API xrtownershipref xrtExecutorOwnership(const xexecutor* pExecutor);
XRT_API const xrtownershipadapterv1* xrtExecutorOwnershipAdapterV1(
	xrtownershipref Reference, const xrtownershippreparationv1** ppPreparation);
/* V1 refuses a live owned exit context. V2 matches its exact registered policy
 * identity BEFORE Count/Trace. Equal descriptor copies are not authority.
 * Unknown/BUSY leaves output unchanged; plain executors are still admitted. */
XRT_API const xrtownershipadapterv1* xrtExecutorOwnershipAdapterV2(
	xrtownershipref Reference, const xexecutorworkerexitownershipv1* const* pPolicies,
	size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
/* Match an admitted resident Data policy identity BEFORE following its fields.
 * A pinned completed slot is not reused and keeps its containing allocation
 * alive, even after Executor destruction. Outputs change only on success. */
XRT_API const xrtownershipadapterv1* xrtExecutorJobOwnershipAdapterV1(
	xrtownershipref Reference, const xexecutordataownershipv1* const* pPolicies,
	size_t iPolicyCount, const xrtownershippreparationv1** ppPreparation);
XRT_API const xrtownershipquiescencev1* xrtExecutorOwnershipQuiescenceV1Get(void);
XRT_API const xrtownershipquiescencev1* xrtExecutorJobOwnershipQuiescenceV1Get(void);



/* 停止受理，丢弃尚未开始的工作并执行其数据析构；运行中工作不会被强停。 */
XRT_API bool xrtExecutorCancel(xexecutor* pExecutor);



/* Wait includes execution AND cancelled-item cleanup tails. A worker or a
 * cancellation cleanup callback cannot wait for its own executor. */
XRT_API xwaitresult xrtExecutorWait(xexecutor* pExecutor);



/* 在相对毫秒数内等待已经关闭的执行器排空。 */
XRT_API xwaitresult xrtExecutorWaitFor(xexecutor* pExecutor, int64 iTimeout);



/* 等待到指定单调时钟截止时间；已排空优先于超时。 */




/* 复制当前负载和累计统计快照。 */
XRT_API bool xrtExecutorGet(
	const xexecutor* pExecutor,
	xexecutorstats* pStats
);



/* Close, drain and join. Caller must stop concurrent access; reentrant or
 * overlapping native entries refuse before releasing ownership. Worker/cleanup
 * callbacks cannot destroy their own executor. Only the original owner is
 * consumed; explicit pins may keep the retired shell/slot allocation alive. */
XRT_API bool xrtExecutorDestroy(xexecutor* pExecutor);

#endif



XRT_EXTERN_C_END

#endif
