#ifndef XCRON_EXECUTOR_H
#define XCRON_EXECUTOR_H
#include <xcron/features.h>
#include <xcron/scheduler.h>
#include <xrt/executor.h>
#if defined(XCRON_FEATURE_XCRON_EXECUTOR) && (!defined(XCRON_FEATURE_XCRON_SCHEDULER) || !defined(XRT_FEATURE_EXECUTOR))
#error "cron executor adapter requires scheduler and public XRT executor"
#endif
#if defined(XCRON_FEATURE_XCRON_EXECUTOR)
#define XCRON_EXECUTOR_BATCH_MAX 64u
typedef struct xcronexecutor xcronexecutor;
typedef struct xcronexecution xcronexecution;
typedef xcronrunresult (*xcronexecuteproc)(const xcronrun* Run,const xcronexecution* Execution,ptr Context);
typedef void (*xcronexecutornotify)(ptr Context);
typedef enum xcronexecutorerror {
    XCRON_EXECUTOR_OK=0,XCRON_EXECUTOR_ARGUMENT=5001,XCRON_EXECUTOR_LIMIT,
    XCRON_EXECUTOR_MEMORY,XCRON_EXECUTOR_CLOSED,XCRON_EXECUTOR_STALE,
    XCRON_EXECUTOR_BUSY,XCRON_EXECUTOR_THREAD,XCRON_EXECUTOR_REENTRY,
    XCRON_EXECUTOR_DUPLICATE,XCRON_EXECUTOR_REJECTED,XCRON_EXECUTOR_RESULT,
    XCRON_EXECUTOR_OWNER_CONFLICT
} xcronexecutorerror;
typedef struct xcronexecutorissue {xcronexecutorerror Code;xerrkind Cause;} xcronexecutorissue;
typedef struct xcronexecutorconfig {
    xexecutor* Executor;
    xcronexecuteproc Proc;
    ptr Context;
    xcronexecutornotify Notify;
    ptr NotifyContext;
    uint32 RunLimit,Reserved;
    size_t BytesLimit;
} xcronexecutorconfig;
typedef struct xcroncompletion {
    xcronrun Run;
    xcronrunresult Result;
    xcronexecutorerror Code;
    xerror* Error;
} xcroncompletion;
typedef struct xcronexecutorstats {
    size_t Bytes;
    uint32 Limit,Active,Peak,Producers;
    bool Closed;
    uint64 Submitted,Rejected,Drained,CancelRequests;
} xcronexecutorstats;
XRT_EXTERN_C_BEGIN
/* A preallocated execution adapter for leases from ONE originating scheduler.
 * This thread owns all adapter operations except ExecutionCancelled. Executor,
 * callback code and both contexts are borrowed through successful Destroy.
 * Executor must remain live and eventually run/drop every accepted submission;
 * adapter Close does not close a shared executor. One allocation bounds slots,
 * the lease-ID hash and completion queues within BytesLimit (not borrowed data,
 * executor storage or callback/diagnostic allocations). Limits positive,
 * Reserved zero. No normal steady allocation. No timer, thread or runtime is
 * created. Invalid arguments/spans/output aliases preserve all outputs. Other
 * failures publish typed Issue despite diagnostic OOM. Success preserves error.
 * Outputs must be disjoint from owned adapter storage, inputs and each other;
 * caller also keeps them disjoint from borrowed contexts/job data/scheduler.
 * Error domain xrt.cron.executor; rejected XRT submissions preserve their cause. */
XCRON_API bool xcronExecutorCreate(const xcronexecutorconfig* Config,xcronexecutor** Adapter,xcronexecutorissue* Issue);
/* Success transfers the duty to return this exact lease in Drain; caller must
 * not Complete it or release its job Data earlier. Failure transfers nothing,
 * invokes no Proc and leaves the caller responsible for Complete/retry. Active
 * duplicate IDs fail; IDs are scoped to the originating scheduler, not global.
 * Proc borrows copied Run/Execution only during its call, on an XRT worker.
 * Callback must finish ALL its data-using tails before returning. FAILED takes
 * that worker's ambient error into the receipt; no owner-thread ambient error
 * supplies another run's cause. Invalid return enums become FAILED/RESULT.
 * A callback selected on the adapter's owner thread is refused with a typed
 * OWNER_CONFLICT completion, preventing owner-state reentry. Callback code stays
 * resident through executor drain/join before unloading the extension library. */
XCRON_API bool xcronExecutorSubmit(xcronexecutor* Adapter,const xcronrun* Run,xcronexecutorissue* Issue);
/* Atomic submission of1..BATCH_MAX already-ready leases. Does not wait to fill
 * a batch. Success transfers every lease's physical completion duty; failure
 * transfers none and invokes no Proc. Each lease still has its own token,
 * result, Error and Drain receipt. IDs must be distinct and inactive. The full
 * input array must be disjoint from owned adapter storage. Descriptors use at
 * most BATCH_MAX*sizeof(xexecutoritem) stack bytes for descriptors;
 * no new retained storage/allocation. The borrowed XRT executor must accept the
 * entire batch in one worker queue; its atomic batch rejection is preserved,
 * even when individual submissions could fit across multiple worker queues.
 * Submitted/Rejected count leases, not API calls; invalid arguments count none.
 * Native rejection retains all duties, so the owner may retry/fall back to scalar
 * submission or Complete every rejected lease. All other Submit rules apply. */
XCRON_API bool xcronExecutorSubmitBatch(xcronexecutor* Adapter,const xcronrun* Runs,size_t Count,xcronexecutorissue* Issue);
/* Cooperative request. A queued callback observes it before Proc; running Proc
 * decides when to stop and which result to return. A late request never changes
 * an already completed outcome. Repeated requests while the ID exists succeed.
 * These do not cancel the scheduler job; coordinate SchedulerCancel separately. */
XCRON_API bool xcronExecutorCancel(xcronexecutor* Adapter,xcronrunid Id,xcronexecutorissue* Issue);
XCRON_API bool xcronExecutorCancelJob(xcronexecutor* Adapter,xcronjobid Id,uint32* Requested,xcronexecutorissue* Issue);
/* Capacity positive. Nonblocking bounded pull of at most Capacity completions.
 * Each receipt transfers Error ownership (possibly NULL); caller frees it with
 * xrtErrorFree. Call SchedulerComplete exactly once after receiving the receipt,
 * then a terminal job can be reaped. No order between concurrent completions is
 * promised. Detached finite batches drain before newer batches; no starvation.
 * This releases adapter capacity, not the scheduler's lease. Data is borrowed
 * through SchedulerReap, never owned by this copied receipt. */
XCRON_API bool xcronExecutorDrain(xcronexecutor* Adapter,xcroncompletion* Completed,size_t Capacity,size_t* Count,xcronexecutorissue* Issue);
/* Idempotent, stops Submit. CancelRuns=true also requests all existing tokens.
 * Notify is optional, runs after publication on the executor's completion/drop
 * thread (possibly this owner when that thread cancels queued XRT work). It must
 * not block or enter adapter operations. Owner reentry and other-thread access
 * are rejected. It may signal a caller-owned event/post a wake; that resource
 * lives through Destroy. Notifications may coalesce/be stale: Drain is authority.
 * The owner must resample with a bounded wait to handle wake-delivery failures. */
XCRON_API bool xcronExecutorClose(xcronexecutor* Adapter,bool CancelRuns,xcronexecutorissue* Issue);
XCRON_API bool xcronExecutorInspect(const xcronexecutor* Adapter,xcronexecutorstats* Stats,xcronexecutorissue* Issue);
/* BUSY until every accepted receipt is drained AND all publication/Notify tails
 * have left adapter memory. Does not join/destroy the borrowed executor. */
XCRON_API bool xcronExecutorDestroy(xcronexecutor* Adapter,xcronexecutorissue* Issue);
/* Any callback thread; NULL means false. Execution cannot escape Proc's scope. */
XCRON_API bool xcronExecutionCancelled(const xcronexecution* Execution);
XRT_EXTERN_C_END
#endif
#endif
