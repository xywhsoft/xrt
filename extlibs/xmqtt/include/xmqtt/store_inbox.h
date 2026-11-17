#ifndef XMQTT_STORE_INBOX_H
#define XMQTT_STORE_INBOX_H
#include <xmqtt/store_write.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX) && (!defined(XMQTT_FEATURE_XMQTT_STORE_WRITE) || !defined(XRT_FEATURE_RANDOM_SECURE))
#error "MQTT durable Inbox requires the semantic writer and secure randomness"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_INBOX)
/* Copyable opaque reservation. Initialize with Take and pass unchanged.
 * Contains no resource pointer. Its authority ends with this Writer instance
 * or on accepted Ack/Release, whichever comes first. */
typedef struct xmqttstorelease {uint64 Opaque[4];} xmqttstorelease;
XRT_EXTERN_C_BEGIN
/* Same transfer and native lifetime as StoreWriteCreate, with durable Inbox
 * leases enabled. A new incarnation fences tokens from earlier Writers.
 * All raw INBOX_ACK deltas submitted to this Writer are refused before IO.
 * Existing pending deliveries enter the ready queue in persistent key order. */
XMQTT_API bool xmqttStoreInboxCreate(xtaskpool* Pool,xmqttjournal* Journal,xmqttstorestate* State,
    const xmqttstorewriteconfig* Config,xmqttstorewrite** Writer);
/* Exclusive FIFO ready claim. Copy canonical bytes into caller Buffer; View
 * then borrows Buffer. Lease, View and Required publish only on DONE, except
 * LIMIT for short Buffer writes only Required. EMPTY includes the case where
 * all deliveries are already leased/acknowledging. Closed/failed or legacy
 * Writer returns STATE_ERROR. Outputs must be writable, mutually disjoint
 * and caller-owned. No allocation, decode, sync or wait for IO. Claims and
 * retries share MaxInbox credit with pending durable deliveries. */
XMQTT_API xmqttstorestatus xmqttStoreInboxTake(xmqttstorewrite* Writer,ptr Buffer,size_t Capacity,
    xmqttstorelease* Lease,xmqttstoreview* View,size_t* Required);
/* Release the exact live claim and append its delivery to the ready tail.
 * No IO. Stale, foreign or acknowledging leases are refused. */
XMQTT_API bool xmqttStoreInboxRelease(xmqttstorewrite* Writer,xmqttstorelease Lease);
/* Call after application effects. ACCEPTED consumes the live claim; Future
 * resolves to xmqttcommitresult even after cancellation, Close or timeout.
 * COMMITTED removes delivery; definite NOT_COMMITTED requeues it with a
 * fresh claim on next Take if the Writer is usable. Uncertain/fatal results
 * suspend further claims until recovery. A refusal leaves Lease live and
 * output untouched. Releasing/destroying the Future does not cancel the ACK.
 * Take/Release/Ack are thread-safe and may coexist with a claimed Client.
 * Stop API entrants before destroying Writer, as for StoreWriteDestroy. */
XMQTT_API xmqttjournaladmission xmqttStoreInboxAckAsync(xmqttstorewrite* Writer,xmqttstorelease Lease,
    int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif
