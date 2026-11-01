#ifndef XRPC_SERVER_H
#define XRPC_SERVER_H
#include <xrpc/dispatch.h>
#include <xrt/executor.h>
#include <xrt/future.h>
#if defined(XRPC_FEATURE_XRPC_SERVER) && \
    (!defined(XRPC_FEATURE_XRPC_DISPATCH) || !defined(XRT_FEATURE_EXECUTOR) || !defined(XRT_FEATURE_FUTURE))
#error "xrpc server requires dispatch, executor and future"
#endif
#if defined(XRPC_FEATURE_XRPC_SERVER)
typedef struct xrpcserver xrpcserver;
#define XRPC_SERVER_CONTEXT_BYTES_DEFAULT 16384u
#define XRPC_SERVER_CONTEXT_BYTES_MAX 1048576u
typedef enum xrpcserverstatus {
    XRPC_SERVER_DONE=0, XRPC_SERVER_CANCELLED, XRPC_SERVER_TIMEOUT,
    XRPC_SERVER_MEMORY, XRPC_SERVER_LIMIT, XRPC_SERVER_LOCAL_ERROR
} xrpcserverstatus;
typedef enum xrpcserveradmission {
    XRPC_SERVER_ADMISSION_ERROR=-1, XRPC_SERVER_ACCEPTED=0,
    XRPC_SERVER_OVERLOADED, XRPC_SERVER_CLOSED
} xrpcserveradmission;
typedef enum xrpcservererror {
    XRPC_SERVER_ARGUMENT=10001, XRPC_SERVER_STATE, XRPC_SERVER_ALLOCATION,
    XRPC_SERVER_RESOURCE
} xrpcservererror;
typedef struct xrpcservercontext {
    xcancel* Cancel; /* Borrowed. Poll for request or server cancellation. */
    double Deadline; /* Absolute xrtTimer seconds; may be INFINITY. */
    xbytesview Data; /* Immutable copied request context, borrowed through Call. */
} xrpcservercontext;
/* Runs on a Core executor worker, never the submitting/I/O thread. Request,
 * Reply and Context are borrowed through this call only. Reply is synchronous;
 * do not defer it. Return/Reply rules match xrpcdispatchcall. Concurrent jobs
 * may enter Call/User concurrently. Poll cancellation/deadline during long work;
 * cancellation cannot undo business effects or forcibly interrupt a handler. */
typedef bool (*xrpcservercall)(const xrpcmessage* Request,xrpcreply* Reply,
    const xrpcservercontext* Context,ptr User);
typedef struct xrpcserverconfig {
    size_t Size;
    xexecutorconfig Executor;
    xrpcdispatchconfig Dispatch;
    size_t MaxOperations,MaxRequestBytesTotal,MaxResults,MaxResultBytes,MaxWorkspaceBytes;
    size_t InitialOutputBytes;
    xrpcservercall Call;
    ptr User; /* Borrowed code/context; alive through successful Destroy. */
    size_t MaxContextBytes;
} xrpcserverconfig;
typedef struct xrpcserverinfo {
    size_t Size;
    bool Accepting,Cancelling;
    size_t Operations,RequestBytes,Results,ResultBytes,WorkspaceBytes,Running;
    size_t ContextBytes; /* Included in RequestBytes; excludes alignment padding. */
    uint64 Accepted,Rejected,Completed,PublicationFailures;
} xrpcserverinfo;
typedef struct xrpcserverresult {
    xrpcserverstatus Status;
    bool MayHaveExecuted;
    xrpcdispatchinfo Dispatch;
    xstrview Wire;
    const xerror* Cause;
} xrpcserverresult;
XRT_EXTERN_C_BEGIN
/* Two workers, 64 queue slots each; 128 live operations, 8 MiB owned request
 * storage, 1024 pending/retained results, 8 MiB result storage, 4 MiB workspace.
 * Output starts at 256 bytes and grows within Dispatch/aggregate bounds. */
XRPC_API bool xrpcServerConfigInit(xrpcserverconfig* Config);
/* Owns a bounded Core executor and preallocated per-worker dispatch scratch.
 * Config is copied; no transport/listener or shared runtime is created. */
XRPC_API bool xrpcServerCreate(const xrpcserverconfig* Config,xrpcserver** Server);
/* Concurrent bounded admission. Input is copied before return and fully decoded
 * on a worker before any handler. One document (including a mixed batch) is one
 * job, deadline and cooperative cancel unit. Timeout starts at entry. Raw token
 * or Future cancellation is sampled before/after handlers and output emission;
 * a queued stop resolves when its worker starts or server cancellation drops it.
 * Refusal/error leaves Future unchanged. No replay or business rollback.
 * Normal terminal outcomes RESOLVE with xrpcserverresult. DONE includes valid
 * RPC errors/parse errors, so inspect Wire/Dispatch. Notification-only documents
 * produce empty Wire. On local failure/stop, partial output is discarded and
 * MayHaveExecuted records actual handler entry, not successful business effects.
 * Result storage includes pending AND caller-retained result headers/capacities.
 * Growth can hit an aggregate limit/OOM after business effects; it is a typed
 * local outcome. Retained Futures keep a quota ledger, not server/worker refs.
 * A checked Core publication refusal increments PublicationFailures and closes
 * the Future without a typed value; callers must also inspect Future state.
 * Completion Watches run on worker/cancel-drop threads and must not block them. */
XRPC_API xrpcserveradmission xrpcServerSubmit(xrpcserver* Server,xstrview Input,
    int64 Timeout,xfuture** Future);
/* Same request semantics, with an independent byte snapshot for every handler
 * in this document. Copies Context before return into the existing job block;
 * it may overlap Input, but neither may overlap Future. At most MaxContextBytes
 * (default 16 KiB, hard 1 MiB; zero disables nonempty context). Job header,
 * alignment padding, Input and Context share MaxRequestBytesTotal. Nonempty
 * Data is aligned for max_align_t; embedded pointers are not deep-copied.
 * Do not mutate or retain the borrowed handler view. Context storage is wiped
 * at the final job reference, independent of retained result Futures. No user
 * cleanup callback or extra context allocation is installed. Plain Submit uses
 * an empty view. Recompile consumers for config/context layout changes. */
XRPC_API xrpcserveradmission xrpcServerSubmitWithContext(xrpcserver* Server,
    xstrview Input,xbytesview Context,int64 Timeout,xfuture** Future);
XRPC_API bool xrpcServerInfo(xrpcserver* Server,xrpcserverinfo* Info);
/* Concurrent nonblocking admission close/drain, or cooperative cancellation plus
 * queued-job drop. User cancel watches can run on the Cancel caller, outside
 * native locks. Running handlers remain responsible for returning. */
XRPC_API bool xrpcServerClose(xrpcserver* Server);
XRPC_API bool xrpcServerCancel(xrpcserver* Server);
/* Closes admission and waits for construction/work/publication/drop tails using
 * one deadline. Self-worker/cleanup waits fail. Retained results do not delay it.
 * This drains duties; successful Destroy additionally joins physical workers. */
XRPC_API xwaitresult xrpcServerWaitFor(xrpcserver* Server,int64 Timeout);
/* NULL succeeds. Stop concurrent API entries first. Success joins and consumes
 * the unique owner; failure leaves it for retry. Cannot destroy on own worker or
 * completion/cancel-drop callback. Result Futures survive successful Destroy.
 * Opaque handlers/jobs/results are not collector admission. */
XRPC_API bool xrpcServerDestroy(xrpcserver* Server);
XRT_EXTERN_C_END
#endif
#endif
