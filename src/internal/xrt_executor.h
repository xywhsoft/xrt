#ifndef XRT_INTERNAL_EXECUTOR_H
#define XRT_INTERNAL_EXECUTOR_H

#include "xrt_temp.h"

#if defined(XRT_FEATURE_EXECUTOR)

typedef struct xrt_executor_worker xrt_executor_worker;

/* Slots stay inside the containing allocation. The queue/worker transfers ONE
 * reference; collector pins delay reuse. Each live slot owns one real shell
 * reference, so that its identity and spinlock survive thread retirement. */
typedef struct xrt_executor_job {
	struct xrt_executor_job* Previous;
	struct xrt_executor_job* Next;
	xrt_executor_worker* Owner;
	xexecutorproc Proc;
	ptr Data;
	xexecutorfreeproc Destroy;
	ptr DestroyContext;
	const xexecutordataownershipv1* DataPolicy;
	volatile int32 RefCount;
	const void* OwnershipClaim;
	bool Active;
	bool Finished;
	bool OwnershipCleared;
} xrt_executor_job;

struct xrt_executor_worker {
	xexecutor* Executor;
	xthread* Thread;
	xrt_spinlock Lock;
	xrt_executor_job* Free;
	xrt_executor_job* Head;
	xrt_executor_job* Tail;
	size_t FreeCount;
	size_t Queued;
	uint32 Index;
	bool Parked;
	bool Exited;
};

/* A stack-only record for cancellation cleanup on a non-worker thread. Native
 * callback reentry can submit but must not wait for or destroy its own tail. */
typedef struct xrt_executor_callback {
	struct xrt_executor_callback* Next;
	uint64 Thread;
} xrt_executor_callback;

struct xexecutor {
	xmutex SleepLock;
	xmutex OwnershipLock;
	xcond Work;
	xcond Idle;
	xrt_executor_worker* Workers;
	xrt_executor_job* Jobs;
	uint32 ThreadCount;
	uint32 StartedThreads;
	size_t QueueLimit;
	size_t StackSize;
	size_t SlotCount;
	xatomic32 Closed;
	xatomic32 Cancelling;
	xatomic32 Shutdown;
	xatomic32 Submitters;
	xatomic32 Sleepers;
	xatomic32 Queued;
	xatomic32 Running;
	/* One accepted-work credit, unchanged by queue/worker/cancel-list transfer.
	 * Released only AFTER Proc, Drop and completion statistics are finished. */
	xatomic32 Outstanding;
	xatomic64 NextWorker;
	xatomic64 Submitted;
	xatomic64 Completed;
	xatomic64 Executed;
	xatomic64 Stolen;
	xatomic64 Cancelled;
	xatomic64 Rejected;
	size_t References;
	size_t Entries;
	size_t LiveJobs;
	xrt_executor_callback* Callbacks;
	const void* OwnershipClaim;
	bool Initialized;
	bool OwnerHeld;
	bool Destroying;
	bool Joining;
	bool Joined;
	bool Retiring;
	bool Retired;
	bool OwnershipCleared;
};

void __xrtExecutorMutationBegin(xrtownershipscope* pScope);
void __xrtExecutorMutationEnd(xrtownershipscope* pScope);
void __xrtExecutorOwnershipBegin(xexecutor* pExecutor, xrtownershipscope* pScope);
void __xrtExecutorOwnershipEnd(xexecutor* pExecutor, xrtownershipscope* pScope);
bool __xrtExecutorEnter(xexecutor* pExecutor, bool bTerminal);
void __xrtExecutorLeave(xexecutor* pExecutor);
void __xrtExecutorRelease(const void* pData);
bool __xrtExecutorReserveJobs(xexecutor* pExecutor, size_t iCount);
void __xrtExecutorUnreserveJobs(xexecutor* pExecutor, size_t iCount);
void __xrtExecutorJobRelease(const void* pData);
xrtownershipref __xrtExecutorJobOwnership(const xrt_executor_job* pJob);
bool __xrtExecutorIdle(const xexecutor* pExecutor);
void __xrtExecutorWake(xexecutor* pExecutor, bool bBatch);
bool __xrtExecutorRetire(xexecutor* pExecutor, size_t iEntries);

#endif
#endif
