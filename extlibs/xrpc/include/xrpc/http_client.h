#ifndef XRPC_HTTP_CLIENT_H
#define XRPC_HTTP_CLIENT_H
#include <xrpc/call.h>
#include <xrpc/encode.h>
#include <xrt/tcp.h>
#include <xrt/http1.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT) && \
    (!defined(XRPC_FEATURE_XRPC_CALL) || !defined(XRPC_FEATURE_XRPC_ENCODE) || !defined(XRPC_FEATURE_XRPC_ID) || \
     !defined(XRPC_FEATURE_XRPC_RESULT_QUOTA) || !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_HTTP1_BODY) || !defined(XRT_FEATURE_HTTP_HOST))
#error "xrpc HTTP client requires call, encode, TCP and HTTP/1 body"
#endif
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
typedef struct xrpchttpclient xrpchttpclient;
typedef enum xrpchttpstate {
    XRPC_HTTP_CONNECTING=0, XRPC_HTTP_OPEN, XRPC_HTTP_CLOSING, XRPC_HTTP_CLOSED
} xrpchttpstate;
typedef enum xrpcadmission {
    XRPC_ADMISSION_ERROR=-1, XRPC_ADMISSION_ACCEPTED=0,
    XRPC_ADMISSION_OVERLOADED, XRPC_ADMISSION_CLOSED
} xrpcadmission;
typedef enum xrpchttperror {
    XRPC_HTTP_ARGUMENT=9001, XRPC_HTTP_STATE, XRPC_HTTP_ALLOCATION,
    XRPC_HTTP_RESPONSE, XRPC_HTTP_ID_EXHAUSTED
} xrpchttperror;
typedef struct xrpchttpconfig {
    size_t Size;
    xnetstreamconfig Stream;
    xhttp1limits Head;
    xhttp1bodylimits Body;
    xrpcresponseconfig Responses;
    size_t MaxRequests, MaxRequestBytes, MaxRequestBytesTotal;
    size_t WorkRequests, ReadBytes, MaxResponseWireBytes;
    size_t MaxBatchItems, MaxBatchBytesTotal;
    size_t MaxResultBlocks, MaxResultBytes;
    uint32 MaxInformational;
} xrpchttpconfig;
typedef struct xrpchttpinfo {
    size_t Size;
    xrpchttpstate State;
    size_t Requests, RequestBytes, BatchBytes;
    size_t ResultBlocks, ResultBytes;
    uint64 Accepted, Rejected, Completed;
    uint64 ResultRejected; /* Refused header/snapshot credit reservations. */
    xrpccallstatus CloseStatus;
} xrpchttpinfo;
typedef struct xrpchttpitem {
    xrpcmessagekind Kind; /* CALL or NOTIFICATION only. */
    xstrview Method, Params;
} xrpchttpitem;
XRT_EXTERN_C_BEGIN
XRPC_API bool xrpcHttpConfigInit(xrpchttpconfig* Config);
/* Numeric, plain TCP endpoint; existing running engine, one fixed worker.
 * Host is an explicit HTTP authority; Target is origin-form. They are copied.
 * One active HTTP/1 exchange, bounded concurrent submissions, persistent TCP.
 * No implicit DNS, TLS, redirects, authentication or replay. Close then Release.
 * The engine remains pinned through physical posts/timers/cancellation tails.
 * Ordinary calls, notifications and array-shaped batches share this transport;
 * DNS, TLS and full transport policy remain product work. */
XRPC_API bool xrpcHttpConnect(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, xstrview Host, xstrview Target,
    const xrpchttpconfig* Config, xrpchttpclient** Client);
XRPC_API bool xrpcHttpRetain(xrpchttpclient* Client);
XRPC_API void xrpcHttpRelease(xrpchttpclient* Client);
/* Caller-thread input validation/encoding and copy; inputs may be reused on
 * return. Params is absent or a raw JSON array/object; Method is decoded UTF-8.
 * Each accepted call gets a fresh nonreused decimal-string ID and an independent
 * xrpccallresult Future. Deadline begins at API entry, including construction,
 * queue and connection setup. Failure/admission refusal leaves Future unchanged.
 * Future cancellation posts owner work. Raw token requests alone do not wake.
 * Sent timeout/cancel retires the connection; queued unsent calls are closed,
 * never replayed. A validated matched reply is REPLIED; non-2xx HTTP status is
 * not itself an RPC error when its complete body is a valid matching response.
 * No business callbacks are installed; Future Watches execute on the worker
 * and must not block. Results do not retain this client/engine.
 * MaxResultBlocks/Bytes include pending/retained typed result headers/maps and
 * response snapshots, each allocation charged once including its private prefix.
 * Header credit is required at admission; later snapshot exhaustion yields
 * LIMIT and retires the connection without replay. Independent ResponsesRetain
 * still holds snapshot credit after the Future is destroyed. */
XRPC_API xrpcadmission xrpcHttpCallAsync(xrpchttpclient* Client,
    xstrview Method, xstrview Params, int64 Timeout, xfuture** Future);
/* No RPC id or reply. Empty 2xx HTTP completion yields NOTIFY_DONE with
 * MAY_HAVE_EXECUTED, never business confirmation. A nonempty successful HTTP
 * body violates this local binding. Non-2xx yields HTTP_ERROR; optional Cause
 * domain xrt.rpc.http carries its status when a diagnostic can be allocated. */
XRPC_API xrpcadmission xrpcHttpNotifyAsync(xrpchttpclient* Client,
    xstrview Method, xstrview Params, int64 Timeout, xfuture** Future);
/* One array-shaped HTTP request, including a one-item batch. One atomic
 * admission and one deadline/cancellation unit cover every item. The Future
 * always contains xrpcbatchcallresult. BATCH_DONE means all call replies were
 * validated/correlated; inspect each mapped RESULT/ERROR for business outcome.
 * Notifications map to SIZE_MAX. All-notification batches use NOTIFY_DONE.
 * Bad/missing/duplicate/notification/unknown IDs publish no partial replies.
 * MaxBatchItems and aggregate in-flight BatchBytes bound metadata, separately
 * from encoded-wire limits. Retained maps/snapshots are charged to result quotas
 * and do not keep client/engine refs. Outputs/inputs cannot overlap. */
XRPC_API xrpcadmission xrpcHttpBatchAsync(xrpchttpclient* Client,
    const xrpchttpitem* Items, size_t Count, int64 Timeout, xfuture** Future);
/* Request and result-ledger counters are separate instants, not one joint atomic
 * snapshot. Result counters include reserved blocks during allocation. Native
 * Future/context/error metadata, allocator overhead and receive staging are
 * outside result bytes; request counts and per-document limits bound these. */
XRPC_API bool xrpcHttpInfo(xrpchttpclient* Client, xrpchttpinfo* Info);
XRPC_API bool xrpcHttpClose(xrpchttpclient* Client);
/* Independent observer; resolves after request duties, optional connection
 * source callback ownership and the actual stream retire. The
 * publishing stack's mechanical client-reference tail can follow; joining the
 * engine retires that stack. Dropping this observer does not close the client.
 * No blocking wait facade here. */
XRPC_API xfuture* xrpcHttpClosed(xrpchttpclient* Client);
XRT_EXTERN_C_END
#endif
#endif
