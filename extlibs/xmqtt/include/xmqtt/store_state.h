#ifndef XMQTT_STORE_STATE_H
#define XMQTT_STORE_STATE_H
#include <xmqtt/store_delta.h>
#include <xmqtt/journal.h>
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE) && (!defined(XMQTT_FEATURE_XMQTT_STORE_DELTA) || !defined(XMQTT_FEATURE_XMQTT_JOURNAL))
#error "MQTT store recovery requires typed deltas and journal"
#endif
#if defined(XMQTT_FEATURE_XMQTT_STORE_STATE)
typedef struct xmqttstorestate xmqttstorestate;
typedef enum xmqttstorestatus {
    XMQTT_STORE_ERROR=-4,XMQTT_STORE_CORRUPT=-3,XMQTT_STORE_STATE_ERROR=-2,
    XMQTT_STORE_IO=-1,XMQTT_STORE_EMPTY=0,XMQTT_STORE_DONE=1,XMQTT_STORE_LIMIT=2
} xmqttstorestatus;
typedef enum xmqttstoreviewkind {
    XMQTT_STORE_OUTGOING=0,XMQTT_STORE_INCOMING,XMQTT_STORE_INBOX
} xmqttstoreviewkind;
typedef struct xmqttstorestateconfig {
    size_t Size;xmqttstoredeltaconfig Delta;xbytesview Binding;uint64 Generation;
    xstrview ClientId;uint64 WallMilliseconds,MaxTransactions,MaxOperations,MaxReplayBytes;
    size_t MaxOutgoing,MaxIncoming,MaxInbox,MaxMemoryBytes;
} xmqttstorestateconfig;
typedef struct xmqttstorestateinfo {
    xmqttversion Version;xstrview ClientId;uint64 Epoch,LastKey,Transactions,Operations,ReplayBytes;
    uint64 LastWallMilliseconds,RemainingMilliseconds;bool HasSession,LiveAtCut;
    size_t Outgoing,Incoming,Inbox,MemoryBytes,PeakMemoryBytes,BaseBytes;
} xmqttstorestateinfo;
typedef struct xmqttstoreview {
    uint64 Epoch,Key;uint16 Identifier;uint8 Qos,PubrecReason;
    bool Attempted,Sent,Pubrel,InboxPending;xbytesview Wire;
} xmqttstoreview;
XRT_EXTERN_C_BEGIN
/* Defaults: MQTT5,128 outgoing,64 QoS2 receive identities,1024 pending Inbox
 * deliveries,16MiB recovery memory (including scratch),1M transactions,8M
 * operations and1GiB payload replay. Zero limits are real caps. Set nonempty
 * exact Binding, nonzero Generation and trusted wall ms<=INT64_MAX. ClientId
 * must match BIND; empty permits explicit MQTT5 assigned-id recovery only. */
XMQTT_API bool xmqttStoreStateConfigInit(xmqttstorestateconfig* Config);
/* Blocking storage-owner recovery, never on a network callback. Journal must
 * be at its original replay cursor (or READY with sequence0). Exact MQTT level,
 * typed schema, Binding and storage Generation are checked against its actual
 * descriptor before consuming it. Neither file nor Journal ownership transfers.
 * Replay validates every committed batch and legal epoch/key/id/QoS/clock order.
 * A compacted generation can start with a complete typed checkpoint followed
 * by ordinary deltas. Otherwise first BIND precedes all records and first OPEN
 * uses that epoch. Session-present
 * OPEN keeps it; subsequent new-session OPEN advances exactly once, no wrap.
 * New outgoing/incoming keys strictly increase globally, including after reset.
 * No state is exposed until Journal EMPTY proves the complete committed prefix.
 * Any failure frees the partial candidate and leaves State unchanged. A failed
 * attempt may consume Journal; destroy/reopen it before retry. Journal suffix
 * retirement/sync can already have effects. This creates an immutable offline
 * recovery view, not a connected Session, live commit driver, delivery lease or
 * network ACK. A future adapter must authenticate/renegotiate before replay.
 * Closed expiry consumes downtime. Unknown disconnect time at a live cut never
 * starts a guessed timer; zero-expiry state is discarded. Accepted Inbox data
 * survives protocol reset/expiry/completion until an explicit INBOX_ACK record.
 * Codec/property scratch and all candidate allocations count against MaxMemory;
 * caller-owned Journal/file/allocator overhead remain additional. Entry/exit
 * error exchanges are checked and quiet; final slot failure publishes no view.
 * All outputs must be disjoint from Config, its views and Journal storage. */
XMQTT_API xmqttstorestatus xmqttStoreStateRecover(xmqttjournal* Journal,
    const xmqttstorestateconfig* Config,xmqttstorestate** State);
/* Immutable thread-safe borrowed snapshots with a stable State lifetime. View
 * arrays are indexed in increasing acceptance key order. EMPTY leaves View
 * unchanged. Key lookup is not exposed as a capability to acknowledge delivery.
 * Wire/ClientId remain readable until Destroy, independent of Journal lifetime.
 * Getters allocate nothing, preserve ambient error on DONE/EMPTY and require
 * outputs disjoint from every State-owned allocation. INCOMING includes only
 * active QoS2 identities; INBOX includes only pending application deliveries. */
XMQTT_API bool xmqttStoreStateInfo(const xmqttstorestate* State,xmqttstorestateinfo* Info);
XMQTT_API xmqttstorestatus xmqttStoreStateView(const xmqttstorestate* State,
    xmqttstoreviewkind Kind,size_t Index,xmqttstoreview* View);
/* Unique owner: stop concurrent readers before Destroy. NULL is allowed. */
XMQTT_API void xmqttStoreStateDestroy(xmqttstorestate* State);
XRT_EXTERN_C_END
#endif
#endif
