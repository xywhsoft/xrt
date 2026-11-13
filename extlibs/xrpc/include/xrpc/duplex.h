#ifndef XRPC_DUPLEX_H
#define XRPC_DUPLEX_H
#include <xrpc/call.h>
#include <xrpc/pending.h>
#include <xrpc/server.h>
#include <xrpc/encode.h>
#if defined(XRPC_FEATURE_XRPC_DUPLEX) && (!defined(XRPC_FEATURE_XRPC_CALL) || \
 !defined(XRPC_FEATURE_XRPC_PENDING) || !defined(XRPC_FEATURE_XRPC_SERVER) || \
 !defined(XRPC_FEATURE_XRPC_ENCODE) || !defined(XRPC_FEATURE_XRPC_RESULT_QUOTA))
#error "xrpc duplex requires call, pending, server, encode and result quota"
#endif
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
typedef struct xrpcduplex xrpcduplex;
typedef struct xrpcduplexoutput xrpcduplexoutput;
typedef struct xrpcduplexitem {bool Notify;xstrview Method,Params;} xrpcduplexitem;
typedef struct xrpcduplexconfig {
    size_t Size;
    xrpcserver* Server; /* Borrowed shared executor; alive through Destroy. */
    xrpcencodeconfig Encode;
    xrpcresponseconfig Response;
    size_t MaxOperations,MaxPendingCalls,MaxIncoming,MaxControlBytes,MaxFixedBytes;
    size_t MaxOutputs,MaxOutputBytes,MaxResults,MaxResultBytes,MaxTemporaryBytes;
    size_t MaxWork;
    int64 IncomingTimeout;
    /* Signal/post ONLY, no waits, business or endpoint reentry. May run on any
     * submitting/cancelling/executor thread. Context/code live through Destroy;
     * an asynchronous posted action needs its own lifetime discipline. */
    void (*Wake)(ptr User);
    ptr User;
} xrpcduplexconfig;
typedef enum xrpcduplexadmission {
    XRPC_DUPLEX_ERROR=-1,XRPC_DUPLEX_ACCEPTED=0,XRPC_DUPLEX_OVERLOADED,
    XRPC_DUPLEX_CLOSED,XRPC_DUPLEX_PROTOCOL
} xrpcduplexadmission;
typedef enum xrpcduplexerror {
    XRPC_DUPLEX_ARGUMENT=18001,XRPC_DUPLEX_STATE,XRPC_DUPLEX_LIMIT,XRPC_DUPLEX_MEMORY
} xrpcduplexerror;
typedef struct xrpcduplexinfo {
    size_t Size;
    bool Closing,Drained,Selected,Started;
    xrpccallstatus Reason;
    size_t Operations,PendingCalls,Incoming,ControlBlocks,ControlBytes;
    /* Outgoing constructors are included in Operations and PendingCalls. */
    size_t Constructing,TemporaryBytes,PeakTemporaryBytes;
    size_t Outputs,OutputBytes,Results,ResultBytes,OwnedBytes;
    uint64 Accepted,Rejected,Completed,Received,UnknownResponses,ProtocolFailures;
    uint64 ControlRefusals,OutputRefusals,ResultRefusals;
    /* Absolute monotonic seconds. INFINITY means wait for Wake/input. Step
     * performs at most MaxWork owner units; More requests another turn now. */
    double NextDeadline;
    bool More;
} xrpcduplexinfo;
XRT_EXTERN_C_BEGIN
/* 128 operations/1024 request IDs/64 incoming jobs; 2 MiB control metadata,
 * 256 outputs/8 MiB wire, 1024 results/8 MiB, 1 MiB aggregate concurrent encoding workspace,
 * 16 MiB endpoint/pending fixed storage, 64 owner units/Step, 5000 ms incoming document deadline. No new thread,
 * engine, timer, listener or shared runtime. NULL Server is client-only. */
XRPC_API bool xrpcDuplexConfigInit(xrpcduplexconfig* Config);
XRPC_API bool xrpcDuplexCreate(const xrpcduplexconfig* Config,xrpcduplex** Result);
/* Concurrent bounded caller-thread admission. Inputs encoded/copied before
 * return; only ACCEPTED writes Future. Timeout starts at API entry, including
 * lock/validation/encoding. Only counter/range reservation and publication hold
 * the endpoint gate; encoding and native construction run outside it. Temporary
 * descriptor storage is an aggregate reservation across all constructors.
 * A reservation before a racing Close can still return ACCEPTED afterward;
 * owner then completes without sending. Construction failure rolls back all
 * credits but consumes its never-reused ID range. Cross-thread wire order is
 * construction publication order; same-thread sequential order is preserved.
 * Fresh string IDs never repeat in one endpoint.
 * Dropping a Future does not cancel. Use native FutureCancel from any thread.
 * Batch has 1..Encode.MaxMessages items; notification indices are SIZE_MAX.
 * Success Future holds native call/batch result, independent of endpoint/server.
 * No replay. No remote cancellation/deadline extension is silently emitted. */
XRPC_API xrpcduplexadmission xrpcDuplexCall(xrpcduplex* Duplex,xstrview Method,
    xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcDuplexNotify(xrpcduplex* Duplex,xstrview Method,
    xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcDuplexBatch(xrpcduplex* Duplex,const xrpcduplexitem* Items,
    size_t Count,int64 Timeout,xfuture** Future);
/* Receive/Step/Next/Started/Sent/Info/Destroy require ONE serialized owner;
 * reject mutating reentry from synchronous completion Watches. Receive takes
 * exactly one whole transport message. It validates/classifies the complete
 * document before executing business/correlating any ID. Mixed request/response
 * documents, ambiguous or invalid responses close with PROTOCOL, without
 * partial matching. Malformed JSON/invalid requests go to the shared Server
 * for standard RPC errors. Input/context copied on accepted request admission;
 * response snapshots copy once and retain independently. OVERLOADED has no
 * effects and retains no input: adapter applies bounded retry/backpressure.
 * Ordinary call response shape is object; batch response is one complete array
 * in arbitrary order, omitting notifications. Unknown-only valid responses are
 * ignored/counted; duplicates/extra/missing IDs for a live group are protocol
 * failures. Replies cannot complete unsent or partially sent requests. */
XRPC_API xrpcduplexadmission xrpcDuplexReceive(xrpcduplex* Duplex,xstrview Input,xbytesview Context);
/* Drive submitted work, due/cancelled calls, executor completions and closing.
 * Call at NextDeadline and after Wake/input. A partial selected send stopped by
 * deadline/cancel closes the channel: adapter MUST abort the transport. Incoming
 * handlers are cooperatively cancelled; close waits for their Future/Watch tails.
 * Shared Server itself is never closed/cancelled/destroyed by this endpoint. */
XRPC_API bool xrpcDuplexStep(xrpcduplex* Duplex);
/* 1 selects an immutable output and transfers one independent reference;
 * 0 empty (outputs unchanged); -1 API/local failure. One selection at a time.
 * Keep Output alive through the physical send borrow. Never send before Started
 * succeeds; token may become stale after Step. Started is the first send-offer
 * boundary, including framing headers, conservatively recording uncertainty
 * even if that offer is refused. Sent means COMPLETE message accepted.
 * Sent releases the endpoint's wire reference. Notification completion confirms
 * transport acceptance only. Selection tokens never wrap or repeat. */
XRPC_API int xrpcDuplexNext(xrpcduplex* Duplex,xrpcduplexoutput** Output,uint64* Token);
XRPC_API bool xrpcDuplexStarted(xrpcduplex* Duplex,uint64 Token);
XRPC_API bool xrpcDuplexSent(xrpcduplex* Duplex,uint64 Token);
/* Concurrent immutable output refs; may outlive endpoint/server/transport.
 * Wire is borrowed through a holding reference. Physical retained wires continue
 * charging their independent quota. Final release never touches the endpoint. */
XRPC_API xstrview xrpcDuplexOutputWire(const xrpcduplexoutput* Output);
XRPC_API bool xrpcDuplexOutputRetain(xrpcduplexoutput* Output);
XRPC_API void xrpcDuplexOutputRelease(xrpcduplexoutput* Output);
/* Concurrent nonblocking close. Reason CLOSED/TRANSPORT/PROTOCOL/LIMIT/MEMORY.
 * Owner Step drives typed completion; no forced interruption/rollback. */
XRPC_API bool xrpcDuplexClose(xrpcduplex* Duplex,xrpccallstatus Reason);
/* Owner-only snapshot; ledger/admission counters sampled under their own locks,
 * not a global atomic snapshot. Closing with only physical pins remaining has
 * a bounded retry deadline so a pre-retirement Wake cannot strand drain.
 * Drained proves no endpoint callback/context
 * duties; retained independent output/results do not delay it. */
XRPC_API bool xrpcDuplexInfo(xrpcduplex* Duplex,xrpcduplexinfo* Info);
/* NULL succeeds. Stop concurrent API entries first. Requires Close + Drained;
 * false retains ownership. Does not wait or drive Step. Result/wire refs survive. */
XRPC_API bool xrpcDuplexDestroy(xrpcduplex* Duplex);
XRT_EXTERN_C_END
#endif
#endif
