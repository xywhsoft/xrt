#ifndef XRPC_CALL_H
#define XRPC_CALL_H
#include <xrpc/response.h>
#include <xrt/wait.h>
#include <xrt/future.h>
#include <xrt/time.h>
#if defined(XRPC_FEATURE_XRPC_CALL) && \
	(!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRT_FEATURE_FUTURE))
#error "XRPC_FEATURE_XRPC_CALL requires response and future"
#endif
#if defined(XRPC_FEATURE_XRPC_CALL)
typedef struct xrpccall xrpccall;
typedef enum xrpccallstatus {
	XRPC_CALL_OK = 0, XRPC_CALL_REMOTE_ERROR, XRPC_CALL_TIMEOUT,
	XRPC_CALL_CANCELLED, XRPC_CALL_CLOSED, XRPC_CALL_TRANSPORT,
	XRPC_CALL_PROTOCOL, XRPC_CALL_LIMIT, XRPC_CALL_MEMORY,
	XRPC_CALL_NOTIFY_DONE, XRPC_CALL_BATCH_DONE, XRPC_CALL_HTTP_ERROR
} xrpccallstatus;
typedef enum xrpcdelivery {XRPC_NOT_SENT=0, XRPC_MAY_HAVE_EXECUTED, XRPC_REPLIED} xrpcdelivery;
typedef enum xrpccallfinish {
	XRPC_CALL_FINISH_ERROR=-1, XRPC_CALL_PENDING=0, XRPC_CALL_FINISHED, XRPC_CALL_ALREADY_DONE
} xrpccallfinish;
typedef enum xrpccallerror {XRPC_CALL_ARGUMENT=8001, XRPC_CALL_STATE, XRPC_CALL_ALLOCATION} xrpccallerror;
typedef struct xrpccallresult {
	xrpccallstatus Status;
	xrpcdelivery Delivery;
	xrpcresponses* Responses;
	size_t Index;
	const xerror* Cause;
} xrpccallresult;
/* A batch Future always owns this extended value, including failure/stop.
 * Call is the unchanged ordinary result prefix. Count is the input item count;
 * Indices maps input items to Responses.Messages, SIZE_MAX for notifications
 * or any non-BATCH_DONE outcome. BATCH_DONE can contain remote error items.
 * The immutable map is preallocated with this result, not a producer reference. */
typedef struct xrpcbatchcallresult {
	xrpccallresult Call;
	size_t Count;
	const size_t* Indices;
} xrpcbatchcallresult;
typedef struct xrpccallconfig {
	size_t Size;
	int64 Timeout; /* ms from Create entry, including construction/queue wait. */
	/* Optional nonblocking wake only; invoked on the cancelling thread at most
	 * once through xrtFutureCancel. It can arrive after logical completion while
	 * its captured producer reference remains live. Queued posts need their OWN
	 * captured references beyond Wake return. Raw token requests do not wake; the
	 * owner must explicitly signal/poll those. It must
	 * only post/signal owner work, never finish/destroy this call, wait for its
	 * owner, or execute business/language code inline. */
	void (*Wake)(ptr User);
	/* Mechanical context-reference release at FINAL producer-reference retirement,
	 * after captured cancellation tails. Destroy can return earlier. NULL means
	 * borrowed context; otherwise successful
	 * Create takes exactly one User reference, failure takes nothing. No wait or
	 * new semantic finalizer in Drop. Callback code lives through destruction. */
	void (*Drop)(ptr User);
	ptr User;
} xrpccallconfig;
XRT_EXTERN_C_BEGIN
/* Default 5000 ms, no context callbacks. Zero is immediately due; FOREVER OK. */
XRPC_API bool xrpcCallConfigInit(xrpccallconfig* Config);
/* A unique serialized-owner producer and an independent Future observer.
 * Result storage is preallocated; native producer binding adds no allocation.
 * No thread,
 * timer, queue, transport or additional shared runtime is created. All producer
 * access (including Destroy) must be serialized; observers use public XRT Future
 * APIs concurrently. Wake must obey its nonblocking/reentry contract. Config is
 * copied and outputs remain unchanged on failure; outputs/config cannot overlap.
 * Accepted ordinary terminal outcomes resolve with an immutable xrpccallresult,
 * including timeout/cancel/close/failure. XFUTURE_RESOLVED alone is not success.
 * Dropping an observer does not cancel accepted work. The owner must finish/poll
 * or destroy the producer; Destroy closes unfinished work and detaches its
 * Future producer binding without waiting for captured cancellation callbacks.
 * Future results own response/cause refs, NOT producer/context/engine refs, and
 * survive producer destruction. Borrow Value/Responses/Cause only while holding
 * the result Future; retain Responses for a longer-lived payload view.
 * This native producer and opaque context are not collector admission. */
XRPC_API bool xrpcCallCreate(const xrpccallconfig* Config, xrpccall** Call, xfuture** Future);
/* Same lifetime contract; 1..65536 items, result header plus Count size_t indices.
 * Ordinary call result/producer storage is not enlarged for batch-only data. */
XRPC_API bool xrpcBatchCallCreate(const xrpccallconfig* Config, size_t Count,
	xrpccall** Call, xfuture** Future);
/* NULL succeeds. Success consumes the unique owner right; a captured cancel
 * callback may still pin physical producer/context storage. False retains the
 * owner right for retry after a checked core publication refusal. */
XRPC_API bool xrpcCallDestroy(xrpccall* Call);
/* Owner records the FIRST acceptance of any request bytes by transport. This
 * is uncertainty, not proof of remote execution. False if already terminal. */
XRPC_API bool xrpcCallMarkSent(xrpccall* Call);
/* Borrowed producer-only access; deadline uses xrtTimer's monotonic seconds.
 * No timer is armed: a runtime schedules owner Poll at this absolute deadline. */
XRPC_API double xrpcCallDeadline(const xrpccall* Call);
XRPC_API bool xrpcCallCancellationRequested(const xrpccall* Call);
/* OK/REMOTE_ERROR require an already-correlated live snapshot/index of matching
 * RESULT/ERROR kind; all other statuses forbid a snapshot/nonzero index. Cause
 * is an explicit borrowed diagnostic, never inferred from ambient error. All
 * needed refs are checked BEFORE publication; ERROR leaves the call pending.
 * CANCEL/TIMEOUT conditions are sampled before publication; elapsed deadline
 * wins if both visible. A concurrent later request
 * may lose to completion (cancellation is cooperative). A validated response
 * records REPLIED knowledge even when timeout/cancel wins; stopped outcomes do
 * not retain its payload. No operation is replayed. Other outcomes retain prior
 * NOT_SENT/MAY_HAVE_EXECUTED knowledge from MarkSent.
 * All results are preallocated; ordinary completion with NULL Cause needs no
 * new heap allocation. Snapshot/error refs and diagnostics still have checked
 * failure paths. ALREADY_DONE does not inspect payloads. Future Watches execute
 * synchronously on publication: they must not block an I/O owner. Reentrant
 * terminal queries/Destroy from such a Watch are supported; never do so in Wake.
 * Success/ordinary ALREADY/PENDING preserve ambient error despite completion
 * Watches changing it. Producer storage remains internally pinned through
 * notification/release tails. Actual transport duties remain the runtime's. */
XRPC_API xrpccallfinish xrpcCallFinish(xrpccall* Call, xrpccallstatus Status,
	xrpcresponses* Responses, size_t Index, const xerror* Cause);
/* Complete an already-correlated full response batch. Count equals creation's
 * input count. Non-SIZE_MAX indices must cover every response exactly once;
 * the runtime verifies IDs and notification membership. Failure stays pending.
 * The index vector cannot overlap producer/result storage. BATCH_DONE publishes
 * the map; stop/failure results keep SIZE_MAX. This alone records REPLIED even
 * if a sampled stop wins. Ordinary Finish rejects BATCH_DONE/OK/REMOTE_ERROR on
 * batch producers. NOTIFY_DONE forbids a payload and requires MarkSent; it means
 * transport completion, never confirmation of notification business effects.
 * HTTP_ERROR is a local HTTP binding outcome with optional explicit Cause. */
XRPC_API xrpccallfinish xrpcBatchCallFinish(xrpccall* Call, xrpcresponses* Responses,
	const size_t* Indices, size_t Count, const xerror* Cause);
/* Publish due TIMEOUT or requested CANCELLED; otherwise ordinary PENDING. */
XRPC_API xrpccallfinish xrpcCallPoll(xrpccall* Call);
XRT_EXTERN_C_END
#endif
#endif
