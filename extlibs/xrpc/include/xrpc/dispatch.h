#ifndef XRPC_DISPATCH_H
#define XRPC_DISPATCH_H
#include <xrpc/decode.h>
#include <xrpc/encode.h>

#if defined(XRPC_FEATURE_XRPC_DISPATCH) && \
	(!defined(XRPC_FEATURE_XRPC_DECODE) || !defined(XRPC_FEATURE_XRPC_ENCODE))
#error "XRPC_FEATURE_XRPC_DISPATCH requires decode and encode features"
#endif

#if defined(XRPC_FEATURE_XRPC_DISPATCH)

typedef struct xrpcreply xrpcreply;
typedef enum xrpcdispatcherror {
	XRPC_DISPATCH_ARGUMENT = 2001, XRPC_DISPATCH_STATE,
	XRPC_DISPATCH_LIMIT, XRPC_DISPATCH_SINK
} xrpcdispatcherror;

/* All callbacks execute synchronously on the xrpcDispatch caller thread.
 * Call returns false for an unknown method, true when handled. A handled call
 * without a reply produces Internal error. Notifications may omit a reply.
 * Request and Reply are borrowed only until Call returns. Reply functions
 * consume local/stack payloads before returning and must not be deferred.
 * Call can implement any method index; Method.Raw is a quoted JSON token.
 * Reserved rpc. methods never reach Call; negotiation is a later runtime API. */
typedef bool (*xrpcdispatchcall)(const xrpcmessage* Request, xrpcreply* Reply, ptr User);

/* Emit borrows a validated response and every view only until it returns.
 * EncodedBytes is the single-object xrpcEncodeWrite length. Batch describes
 * the final response document, even when only one reply survives. The sink
 * must copy/encode synchronously and enforce its own storage/backpressure.
 * Return false to stop dispatch; this is a local sink error, never a wire
 * Invalid Request. No further business handler runs after a sticky failure.
 * For batches, accumulate objects with commas; append the closing bracket only
 * after successful dispatch. Zero replies means send nothing, including no []. */
typedef bool (*xrpcdispatchemit)(const xrpcoutgoing* Response, bool Batch,
	size_t EncodedBytes, ptr User);

typedef struct xrpcdispatchconfig {
	size_t Size;
	xrpcdecodeconfig Decode; /* Mode must be REQUESTS. Whole-document limits. */
	xrpcencodeconfig Reply; /* Per response OBJECT limits; no enclosing batch. */
	size_t MaxReplies;
	/* Whole response document, including brackets and commas. Unknown handler
	 * output sizes cannot be admitted in advance; see partial-effects contract. */
	size_t MaxOutputBytes;
} xrpcdispatchconfig;

typedef struct xrpcdispatchinfo {
	size_t Size;
	bool Batch, ParseError;
	size_t Messages, Visited, Invoked, Notifications, Replies, OutputBytes;
} xrpcdispatchinfo;

XRT_EXTERN_C_BEGIN

/* Default request decoder and per-object encoder limits, 256 replies and
 * 1 MiB complete output. NULL config selects defaults. */
XRPC_API bool xrpcDispatchConfigInit(xrpcdispatchconfig* Config);

/* Full JSON validation precedes ALL user callbacks. Scratch is caller-owned
 * decode storage; Capacity>0. Local input/capacity/resource/diagnostic-OOM
 * failure invokes no callback and leaves Info unchanged (scratch unspecified).
 * Malformed JSON instead emits one Parse error/null id. Invalid items emit
 * Invalid Request/null id. Valid calls preserve Id.Raw exactly; valid
 * notifications never emit, including unknown methods/handler errors.
 * The actual reply count is admitted before callbacks. Input/config/scratch
 * and Info cannot overlap; keep input/scratch unchanged throughout callbacks.
 * Config is copied. No input/callback/user data is retained after return.
 * No thread, queue or connection is added. Ordinary valid-input dispatch does
 * not allocate. Protocol/error diagnostics and user callbacks can allocate.
 * Call may be NULL (all methods unknown); Emit must be non-NULL.
 * Info.Size must match. Once decoded/admitted, Info is published even on a
 * callback/reply/output-limit failure and counts only accepted Emit results.
 * A failed sink may already have side effects. Discard partial response bytes;
 * previously invoked business effects cannot be undone or blindly replayed.
 * Independent dispatches can reenter/run concurrently with independent scratch
 * and contexts. Callbacks must not block an I/O worker; this leaf does no
 * executor/async/deadline/transport work. Ordinary success restores the caller's
 * ambient error even if a handler changed it. Local failure retains its first
 * diagnostic despite callbacks clearing/replacing ambient error. */
XRPC_API bool xrpcDispatch(xstrview Input, const xrpcdispatchconfig* Config,
	xrpcmessage* Scratch, size_t Capacity, xrpcdispatchcall Call, ptr CallUser,
	xrpcdispatchemit Emit, ptr EmitUser, xrpcdispatchinfo* Info);

/* Exactly one reply attempt per handler. Duplicate/reentrant attempts and a
 * false Call return after a reply are sticky local state errors. A failed
 * attempt cannot be replaced/retried. On a notification these functions record
 * the attempt and succeed without inspecting payloads or invoking Emit.
 * RawResult is one JSON value. Code is an integral raw JSON number; Message is
 * decoded UTF-8, Data is an optional raw value. Non-notification output is
 * independently preflighted by the public encoder before Emit is invoked. */
XRPC_API bool xrpcReplyResult(xrpcreply* Reply, xstrview RawResult);
XRPC_API bool xrpcReplyError(xrpcreply* Reply, xstrview Code,
	xstrview Message, xstrview Data);

XRT_EXTERN_C_END
#endif
#endif
