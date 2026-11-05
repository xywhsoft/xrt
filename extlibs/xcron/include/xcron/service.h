#ifndef XCRON_SERVICE_H
#define XCRON_SERVICE_H
#include <xcron/features.h>
#include <xcron/executor.h>
#include <xrt/memory.h>
#include <xrt/event_waiter.h>
#if defined(XCRON_FEATURE_XCRON_SERVICE) && (!defined(XCRON_FEATURE_XCRON_EXECUTOR) || !defined(XRT_FEATURE_EVENT_WAITER) || !defined(XRT_FEATURE_COND) || !defined(XRT_FEATURE_THREAD))
#error "cron service requires execution adapter and public XRT thread/condition/event deadline waiter"
#endif
#if defined(XCRON_FEATURE_XCRON_SERVICE)
typedef struct xcronservice xcronservice;
typedef uint64 xcronrequestid;
typedef bool (*xcronclockproc)(xcronclock* Clock,ptr Context);
typedef enum xcronservicestate {XCRON_SERVICE_STARTING=0,XCRON_SERVICE_RUNNING,XCRON_SERVICE_STOPPING,XCRON_SERVICE_STOPPED,XCRON_SERVICE_FAILED} xcronservicestate;
typedef enum xcronrequestkind {XCRON_REQUEST_ADD=0,XCRON_REQUEST_CANCEL,XCRON_REQUEST_INFO,XCRON_REQUEST_REAP} xcronrequestkind;
typedef enum xcronserviceerror {
    XCRON_SERVICE_OK=0,XCRON_SERVICE_ARGUMENT=6001,XCRON_SERVICE_LIMIT,
    XCRON_SERVICE_MEMORY,XCRON_SERVICE_CLOSED,XCRON_SERVICE_STALE,
    XCRON_SERVICE_BUSY,XCRON_SERVICE_THREAD,XCRON_SERVICE_CLOCK,
    XCRON_SERVICE_SYSTEM,XCRON_SERVICE_KERNEL,XCRON_SERVICE_EXECUTOR
} xcronserviceerror;
typedef enum xcronservicecompletioncode {XCRON_COMPLETION_NORMAL=0,XCRON_COMPLETION_REJECTED,XCRON_COMPLETION_EXPIRED,XCRON_COMPLETION_CANCELLED,XCRON_COMPLETION_INTERNAL} xcronservicecompletioncode;
typedef struct xcronserviceissue {xcronserviceerror Code;xerrkind Cause;} xcronserviceissue;
typedef struct xcronserviceconfig {
    xcronschedulerconfig Scheduler;
    xexecutor* Executor;
    xcronexecuteproc Proc;
    ptr Context;
    xcronclockproc Clock;
    ptr ClockContext;
    uint32 CommandLimit,CompletionLimit,BatchLimit,CommandWorkLimit;
    uint32 ResampleMilliseconds,RetryIntervalMilliseconds,RetryAttempts,Reserved;
    uint64 AdmissionMilliseconds;
    size_t BytesLimit,AdapterBytesLimit,StackSize;
    xallocationbudget* InitializationBudget;
} xcronserviceconfig;
typedef struct xcronreply {
    xcronrequestid Request;
    xcronrequestkind Kind;
    xcronserviceerror Code;
    xcronjobid Job;
    ptr Data;
    bool Found;
    xcronjobinfo Info;
    xcronretired Retired;
    xcronschedulerissue Scheduler;
    xcronexecutorissue Executor;
    xerror* Error;
} xcronreply;
typedef struct xcronservicecompletion {
    xcronrunid Id;
    xcronjobid Job;
    xtime ScheduledAt,Through;
    xcronclock Observed;
    uint32 Flags;
    xcronrunresult Result;
    xcronservicecompletioncode Code;
    xcronexecutorerror Executor;
    uint32 Attempts;
    xerror* Error;
} xcronservicecompletion;
typedef struct xcronservicestats {
    size_t Bytes,ArenaBytes,KernelBytes,AdapterBytes;
    xcronservicestate State;
    uint32 PendingRequests,ReadyReplies,ReservedCompletions,ReadyCompletions,Unsent;
    xcronserviceissue Failure;
    xcronclock LastClock;
    xcronschedulerstats Kernel;
    xcronexecutorstats Executor;
    uint64 SubmitAttempts,RejectedSubmissions,Retries,AdmissionExpired,Published,Collected,WakeFailures;
    /* Selected backend/capacity persists after stop; live timers becomes zero on
     * successful owner cleanup. Failed cleanup stays visible and Destroy retries.
     * Waiter storage is already included in ArenaBytes, not added to Bytes again. */
    xeventwaiterinfo Waiter;
    uint32 ActiveNativeTimers;
} xcronservicestats;
XRT_EXTERN_C_BEGIN
/* Dedicated public-XRT owner thread, bounded command tickets, kernel, borrowed
 * executor adapter and reserved completion output. No general/shared runtime.
 * Executor, Proc/Context and Clock/ClockContext are borrowed through successful
 * Destroy; Executor stays live until all accepted work/drop returns. Clock=NULL
 * samples xrtNow/xrtTimer; a custom clock runs only on the owner and can run before
 * Create returns, after *Service is published. It must return promptly with a
 * valid pair, retain no output and finish all context-using tails. Monotonic
 * regression/read failure causes observable controlled shutdown. Wall jumps are
 * governed by each job's existing misfire policy. Full recurrence searches can
 * still delay this owner; no hard CPU/P99 bound is claimed.
 * All limits positive, Reserved zero; BatchLimit <=RunLimit and CompletionLimit.
 * BytesLimit reserves arena + Scheduler.BytesLimit + AdapterBytesLimit without
 * overflow. Stats reports actual requested component bytes. Borrowed data/zones,
 * diagnostics, callbacks and native/runtime bootstrap storage are outside this
 * cap. One owner-bound reusable event waiter prefers precise native deadlines,
 * with observable platform fallback in Stats.Waiter and no global resolution
 * change. Its storage is inside ArenaBytes; native timer handles are outside the
 * byte cap and counted separately. Normal stop releases it before STOPPED; a
 * release failure publishes FAILED and Destroy retries after physical join.
 * Optional InitializationBudget charges direct creator and kernel/adapter
 * startup allocations; borrowed only during Create, not business work/bootstrap.
 * Create failure consumes no Data/context/Executor and leaves Service unchanged.
 * Invalid API spans/output aliases preserve outputs; value failures publish typed
 * Issue despite diagnostic OOM. Success/normal timeout preserves ambient error.
 * Output buffers must be disjoint from service arena/inputs/each other and, by
 * caller contract, every borrowed context/zone/data and private component storage.
 * Error domain xrt.cron.service. No API grants safe concurrent Destroy. */
XCRON_API bool xcronServiceCreate(const xcronserviceconfig* Config,xcronservice** Service,xcronserviceissue* Issue);
/* Thread-safe bounded admission. ADD copies its config and success-only retains
 * Zone for the command. Queue acceptance is distinct from job admission. Data is
 * borrowed until a failed ADD reply is collected, or a successful job is reaped.
 * ADD success reply has Job and Data=NULL; failure returns original Data. Other
 * commands carry no Data duty. Close rejects new commands; queued ADDs not yet
 * processed complete CLOSED, while an already executing Add can succeed then be
 * cancelled. Generation tickets remain occupied until exactly one Collect. */
XCRON_API bool xcronServiceAddAsync(xcronservice* Service,const xcronjobconfig* Config,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceCancelAsync(xcronservice* Service,xcronjobid Job,bool CancelRuns,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceInfoAsync(xcronservice* Service,xcronjobid Job,xcronrequestid* Request,xcronserviceissue* Issue);
XCRON_API bool xcronServiceReapAsync(xcronservice* Service,xcronrequestid* Request,xcronserviceissue* Issue);
/* Timeout >=0, one total monotonic budget including contention/spurious wakeups.
 * Ready wins over timeout. XWAIT_OK transfers exactly one reply and its owned
 * optional Error; its Code is the asynchronous operation's result. Timeout leaves
 * output and accepted command/Data duty intact. Stale/already collected tickets
 * fail. Pending blocking waits reject owner and ANY borrowed-executor callback,
 * not only service bodies. Timeout=0 permits nonblocking callback collection. */
XCRON_API xwaitresult xcronServiceCollect(xcronservice* Service,xcronrequestid Request,int64 Timeout,xcronreply* Reply,xcronserviceissue* Issue);
/* Thread-safe nonblocking metadata drain, Capacity positive. Every pulled kernel
 * lease reserved its output slot first, including unsent/native-running work.
 * Full output applies backpressure; it never drops a result. Error ownership is
 * transferred per record and must be xrtErrorFree'd. Records contain NO job Data
 * pointer, so later consumption is safe after a separate retirement receipt.
 * No order between concurrent workers is promised. Retry only no-effect AGAIN;
 * RetryAttempts includes the initial attempt. AdmissionMilliseconds=0 disables
 * retries; elapsed/attempt bounds produce EXPIRED, never silent replay. */
XCRON_API bool xcronServiceDrain(xcronservice* Service,xcronservicecompletion* Completed,size_t Capacity,size_t* Count,xcronserviceissue* Issue);
/* Asynchronously stops future dispatch/admission. Always cancels unsent leases;
 * CancelRuns=true also requests accepted tokens, and can upgrade an earlier close.
 * No forced termination and no close of a borrowed shared Executor. A current
 * owner turn may finish before observing Close; it is not a no-new-body barrier. */
XCRON_API bool xcronServiceClose(xcronservice* Service,bool CancelRuns,xcronserviceissue* Issue);
/* Finite Timeout>=0; rejects owner/borrowed-executor callbacks before a blocking
 * join. Includes native thread/XRT context cleanup. Close/Wait can finish before
 * clients consume outputs/reap jobs, because all final outputs are pre-reserved. */
XCRON_API xwaitresult xcronServiceWaitFor(xcronservice* Service,int64 Timeout,xcronserviceissue* Issue);
/* Only after owner stop; safe serialized kernel handoff. Running callers use
 * ReapAsync. Successful empty reap preserves Retired, sets Found=false. Original
 * Data is returned only after all bodies/data-using tails have ended. */
XCRON_API bool xcronServiceReapStopped(xcronservice* Service,xcronretired* Retired,bool* Found,xcronserviceissue* Issue);
XCRON_API bool xcronServiceInspect(xcronservice* Service,xcronservicestats* Stats,xcronserviceissue* Issue);
/* Returns an owned reference to the first fatal owner error, or NULL. Typed
 * Failure in Stats survives diagnostic OOM and is authoritative even if NULL. */
XCRON_API bool xcronServiceFailure(xcronservice* Service,xerror** Error,xcronserviceissue* Issue);
XCRON_API bool xcronServiceWake(xcronservice* Service,xcronserviceissue* Issue);
/* Caller-exclusive. Requires stopped/joined owner, zero outstanding tickets,
 * completion reservations and jobs. BUSY preserves all duties. No implicit reaping
 * or Data destruction. Borrowed callback/module code remains resident through the
 * host Executor's physical drain/join before unloading its extension library. */
XCRON_API bool xcronServiceDestroy(xcronservice* Service,xcronserviceissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
