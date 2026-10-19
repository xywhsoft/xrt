#ifndef XRUDP_FUTURE_H
#define XRUDP_FUTURE_H

#include <xrudp/endpoint.h>

#if defined(XRUDP_FEATURE_RUDP_ENDPOINT_FUTURE)
#include <xrt/future.h>

#define XRUDP_ASYNC_OPERATIONS_MAX 64u
#define XRUDP_ASYNC_BYTES_MAX (16u * 1024u * 1024u)
#define XRUDP_ASYNC_COMPLETION_BURST 8u

typedef enum xrudpoperationkind {
    XRUDP_OPERATION_READ = 1,
    XRUDP_OPERATION_WRITE,
    XRUDP_OPERATION_MESSAGE_READ,
    XRUDP_OPERATION_MESSAGE_WRITE,
    XRUDP_OPERATION_DATAGRAM_READ,
    XRUDP_OPERATION_DATAGRAM_WRITE,
    XRUDP_OPERATION_STREAM_OPEN,
    XRUDP_OPERATION_STREAM_FINISH,
    XRUDP_OPERATION_STREAM_STOP,
    XRUDP_OPERATION_STREAM_RESET,
    XRUDP_OPERATION_STREAM_RETIRE,
    XRUDP_OPERATION_DATAGRAM_CONFIGURE,
    XRUDP_OPERATION_SHUTDOWN,
    XRUDP_OPERATION_ABORT,
    XRUDP_OPERATION_WAIT_WRITE_DONE
} xrudpoperationkind;

typedef struct xrudpoperation {
    xrudpoperationkind Kind;
    uint32_t Stream, Channel, Reason;
    const void* Data;
    size_t Size, Capacity;
    uint32_t TimeoutMs, DrainTimeoutMs;
    uint64_t DatagramLifetimeUs; /* From submission, 0 disables; max 10 minutes. */
    xrudpstreamconfig StreamConfig;
    xrudpdatagramchannel ChannelConfig;
} xrudpoperation;

typedef struct xrudpoperationresult {
    xrudpresult Result;
    uint32_t Stream;
    size_t Size;
    const void* Data;
    const xerror* Error; /* Retained transport cause when available; borrowed. */
    xrudpdatagraminfo Datagram;
    uint64_t Sequence;
} xrudpoperationresult;

typedef struct xrudpasyncstate {
    uint32_t Limit, Retained, Pending;
    size_t ByteLimit, RetainedBytes;
    uint64_t Submitted, Completed, Cancelled, Rejected;
    bool Stopping;
} xrudpasyncstate;

XRT_EXTERN_C_BEGIN

/* Default READ on stream 1, no extra operation deadline. Configure endpoint
 * AsyncOperations/AsyncBytes before Bind; both default zero (disabled).
 * The byte quota includes request/result nodes and copied payload buffers.
 * Completed results retain quota until their Future is destroyed. XRT's
 * Future/cancellation bookkeeping retains its own allocation budget. */
XRUDP_API void xrtRudpOperationInit(xrudpoperation* pOperation);
/* Any thread holding an owned peer reference may submit. Input and request
 * are copied before return. NULL means no accepted operation and sets the
 * thread error (argument/capacity/memory/closed). A returned Future settles
 * once; its borrowed value is xrudpoperationresult until Future destruction.
 * Protocol EOF/RESET/errors are Result codes. Cancellation observed before the
 * owner claims a queued request publishes a cancelled Future without data I/O.
 * Cancellation racing that claim leaves the actual outcome authoritative;
 * successful native admission is never replaced by cancellation.
 * WRITE returns one buffered prefix, not delivery; messages are atomic.
 * TRUNCATED reports required Size without consuming a message/datagram.
 * Read results own their data; write input is erased on completion.
 * Destroying an observer Future does not cancel its accepted operation.
 * TimeoutMs is 0 or 1..600000, measured from submission by a monotonic clock;
 * timing out an operation does not close the peer. Negotiating peers wait for
 * confirmation. DATAGRAM lifetime also starts at submission and never renews
 * on native queue admission; expiry before admission returns TIMEOUT/zero Size.
 * Native idle/progress/handshake deadlines remain independent.
 * FIFO applies to each peer/stream/direction and the shared datagram writer;
 * SHUTDOWN waits for earlier reliable writes/open/FIN and fences later writes.
 * abortive STOP/RESET/ABORT may overtake blocked data. Do not concurrently mix
 * raw owner reads/writes with async operations for the same data direction.
 * All mutation stays on the owner. Pending operations settle before endpoint
 * Close; result handles may outlive the stopped/destroyed Engine. ABORT also
 * runs before handshake confirmation. Never block-wait on the bound Worker. */
XRUDP_API xfuture* xrtRudpPeerSubmitAsync(xrudppeer* pPeer, const xrudpoperation* pOperation);
/* Any thread; caller owns an endpoint reference. Safe after Engine destruction.
 * A disabled endpoint returns an all-zero snapshot. */
XRUDP_API xrudpresult xrtRudpEndpointAsyncState(const xrudpendpoint* pEndpoint, xrudpasyncstate* pState);

XRT_EXTERN_C_END
#endif
#endif
