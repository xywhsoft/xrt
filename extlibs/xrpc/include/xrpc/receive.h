#ifndef XRPC_RECEIVE_H
#define XRPC_RECEIVE_H
#include <xrpc/response.h>
#include <xrpc/pending.h>
#if defined(XRPC_FEATURE_XRPC_RECEIVE) && \
	(!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRPC_FEATURE_XRPC_PENDING))
#error "XRPC_FEATURE_XRPC_RECEIVE requires response and pending"
#endif
#if defined(XRPC_FEATURE_XRPC_RECEIVE)
typedef struct xrpcreceiveinfo {
	size_t Size;
	bool Batch;
	size_t Count, Matched, Unknown, Repeated;
} xrpcreceiveinfo;
typedef enum xrpcreceiveerror { XRPC_RECEIVE_ARGUMENT = 7001, XRPC_RECEIVE_CAPACITY } xrpcreceiveerror;
XRT_EXTERN_C_BEGIN
/* Synchronous serialized-owner response admission. Validate the whole response
 * document, copy one immutable snapshot, then preflight every pending-table ID
 * and key limit before removing ANY call. No callback/queue/future completion.
 * On DONE, all matched calls have been removed before publishing Responses and
 * Info; resolutions are in response-item order. MATCHED wins once, subsequent
 * equivalent live IDs are REPEATED, preflight-missing IDs UNKNOWN. Only MATCHED
 * carries a borrowed Call/User. Snapshot retention does NOT retain User objects.
 * The owner completes matched calls after return, retaining the snapshot/index
 * for each asynchronous result as needed; copy/retain no borrowed input view.
 * Response status distinguishes local failure, malformed JSON and invalid
 * envelopes. Every failure leaves Pending calls, Responses and Info unchanged;
 * Scratch and Resolutions contents are unspecified and must be discarded.
 * All limits/allocations/ID parsing precede removal; commit does no failable work.
 * Capacity/ResolutionCapacity positive. Scratch/Resolutions/input/config/output/
 * Pending storage must be valid and mutually nonoverlapping. Info.Size must match.
 * Config is copied by the snapshot reader; NULL defaults. Keep all storage alive
 * and input unchanged; the caller must not mutate storage concurrently while the
 * decoder writes scratch and this function removes admitted calls. No reentrant
 * table access. Unknown-only documents still produce an owned snapshot.
 * Response eligibility, fresh wire IDs/epochs, expiry-event ordering, executor
 * completion and backpressure remain runtime policy. An explicitly pending null
 * ID matches null here; this does not distinguish an unsolicited null-ID error.
 * One snapshot allocation/copy; decoder and per-ID canonicalization are separate
 * scans, followed by bounded tree lookups/removals. Cold diagnostics allocate;
 * XRT reference/JSON bookkeeping can synchronize. Ordinary success preserves
 * ambient error. This is not a socket reader or a complete async client. */
XRPC_API xrpcresponsestatus xrpcReceive(xrpcpending* Pending, xstrview Input,
	const xrpcresponseconfig* Config, xrpcmessage* Scratch, size_t Capacity,
	xrpcpendingresolution* Resolutions, size_t ResolutionCapacity,
	xrpcresponses** Responses, xrpcreceiveinfo* Info);
XRT_EXTERN_C_END
#endif
#endif
