#ifndef XMQTT_SESSION_H
#define XMQTT_SESSION_H
#include <xmqtt/negotiation.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION) && !defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
#error "MQTT session requires negotiation and both codecs"
#endif
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
typedef struct xmqttsession xmqttsession;
typedef struct xmqttsessionpacket xmqttsessionpacket;
typedef enum xmqttsessionstatus {
    XMQTT_SESSION_RESTRICTED=-4,XMQTT_SESSION_ERROR=-3,
    XMQTT_SESSION_STATE_ERROR=-2,XMQTT_SESSION_PROTOCOL_ERROR=-1,
    XMQTT_SESSION_EMPTY=0,XMQTT_SESSION_DONE=1,XMQTT_SESSION_LIMIT,
    XMQTT_SESSION_DUPLICATE,XMQTT_SESSION_REJECTED,XMQTT_SESSION_RESET
} xmqttsessionstatus;
typedef enum xmqttsessionstage {
    XMQTT_SESSION_PUBLISH=0,XMQTT_SESSION_PUBREL,
    XMQTT_SESSION_COMPLETE,XMQTT_SESSION_DISCARDED
} xmqttsessionstage;
typedef struct xmqttsessionconfig {
    size_t Size;xmqttversion Version;xstrview ClientId;xmqttcodecconfig Codec;
    size_t MaxOutgoing,MaxIncoming,MaxBytes;
} xmqttsessionconfig;
typedef struct xmqttsessioninfo {
    size_t Size;xmqttversion Version;bool Connected,HasState;
    size_t ActiveOutgoing,Objects,LiveBytes,BaseBytes,Incoming,Prepared;
    uint16 SendQuota,InitialSendQuota;uint64 Generation;double ExpiresAt; /* Nominal display deadline; expiry uses elapsed seconds. */
} xmqttsessioninfo;
typedef struct xmqttsessionpacketinfo {
    size_t Size;uint16 Identifier;uint8 Qos,Reason,PubrecReason;
    bool Active,Sent,PubrecReceived;xmqttsessionstage Stage;size_t WireBytes;
} xmqttsessionpacketinfo;
/* One retained Packet reference is transferred by Next. Prefix/Tail are
 * immutable slices of that object, even after End, ACK, Reset or Destroy.
 * Do not interleave frames. Sent finishes this exact generation's selection;
 * Generation/Selection identify this exact selection, including repeated
 * PUBREL selections in one connection. Do not reuse an earlier token.
 * End abandons a partial selection. Release Packet once its actual native
 * writes no longer borrow these slices. Releasing early cannot free the
 * session's outstanding selection, but leaves Next blocked until Sent/End. */
typedef struct xmqttsessionframe {
    size_t Size;const xmqttsessionpacket* Packet;uint64 Generation;
    xmqttpackettype Type;uint16 Identifier;xbytesview Prefix,Tail;uint64 Selection;
} xmqttsessionframe;
XRT_EXTERN_C_BEGIN
/* Caller-driven in-memory protocol state, not a transport, retry timer, Future
 * facade or durable store. The optional Client Session factory attaches it;
 * ordinary Client factories retain their connection-local state contract.
 * Mutating operations require one serialized protocol owner. Info, packet
 * getters and Retain/Release may run on other threads with stable references.
 * No callback, thread, Engine pin or credential is owned. The application must
 * bind this object to one broker/security principal and its copied ClientId. */
XMQTT_API bool xmqttSessionConfigInit(xmqttsessionconfig* Config);
/* Defaults: MQTT5,128 outgoing,64 incoming QoS2,16 MiB including all live owned
 * allocations and metadata. Set a nonempty valid ClientId before Create.
 * Fixed id/hash/receive maps are allocated at Create and included in BaseBytes;
 * zero limits disable their maps. Retained completed packets keep byte/object/
 * identifier reservations until their last physical reference is released.
 * Config/descriptors/views borrow only during the call. Outputs are caller
 * storage, disjoint from all inputs and library-owned views. */
XMQTT_API bool xmqttSessionCreate(const xmqttsessionconfig* Config,xmqttsession** Session);
/* Call only after authenticating the accepted connection. Uses independent
 * CONNECT/CONNACK negotiation; identity/version must match. DONE/RESET opens
 * a new generation, resets connection-only send quota and schedules only the
 * required old PUBLISH/PUBREL retries. No live-connection timer retransmission.
 * MQTT5 SessionPresent=0 discards old state (RESET). Missing local state with
 * SessionPresent=1 fails closed. A 311 missing-server-state mismatch also fails
 * closed; use an explicit offline Reset/clean connection to discard it.
 * Rejection/errors preserve the session and Negotiated follows negotiation's
 * output contract. Now is finite nonnegative monotonic seconds, never backwards.
 * Expiry is owner-driven by Open/Expire; it is never applied while connected. */
XMQTT_API xmqttsessionstatus xmqttSessionOpen(xmqttsession* Session,
    const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Negotiated);
/* Call at actual network termination. Disconnect is NULL for an abort; pass
 * a descriptor only if that complete valid DISCONNECT was sent. Original-zero
 * expiry restrictions apply. No native references are released on your behalf.
 * Finite MQTT5 expiry starts here; UINT32_MAX/311 persistent state has no timer.
 * Prepared, uncommitted incoming deliveries are rolled back. */
XMQTT_API xmqttsessionstatus xmqttSessionEnd(xmqttsession* Session,
    const xmqttpacket* Disconnect,double Now);
XMQTT_API xmqttsessionstatus xmqttSessionExpire(xmqttsession* Session,double Now);
/* Explicit administrative termination, offline only. Held immutable packets
 * remain readable and charged until released. Never silently reset a live session. */
XMQTT_API xmqttsessionstatus xmqttSessionReset(xmqttsession* Session);
/* Connected-only new QoS1/2, DUP=0, owned immutable canonical packet snapshot.
 * Identifier0 assigns an unused id; explicit collision is normal LIMIT.
 * Full logical topic required. MQTT5 Topic Alias is validated then removed:
 * connection-only mappings are never persisted or retried. One payload copy.
 * ACCEPT(DONE) transfers a packet reference; LIMIT/other failures leave output
 * unchanged. A held packet does not pin an Engine or connection.
 * Message Expiry remains the publisher-supplied interval, not an offline queue
 * TTL; this is client-originated retransmission, not server onward forwarding. */
XMQTT_API xmqttsessionstatus xmqttSessionPublish(xmqttsession* Session,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
/* One outstanding frame selection. Empty/quota exhaustion preserves output.
 * PUBREL progress bypasses exhausted PUBLISH quota. New/retried PUBLISH use
 * DUP0/DUP1 respectively; PUBREL always uses fixed flags2 and original id.
 * A resumed peer's packet/QoS/retain restrictions fail without dropping state.
 * Selection numbers never wrap within a generation; exhaustion is RESTRICTED
 * without consuming the ready frame or changing output. End/Open starts a new
 * generation. Sent rejects stale selection tokens without changing state. */
XMQTT_API xmqttsessionstatus xmqttSessionNext(xmqttsession* Session,xmqttsessionframe* Frame);
XMQTT_API xmqttsessionstatus xmqttSessionSent(xmqttsession* Session,const xmqttsessionframe* Frame);
/* After full frame validation, correlate PUBACK/PUBREC/PUBCOMP. Negative ACKs
 * retire the flow; successful PUBREC schedules PUBREL, never PUBLISH again.
 * Duplicate successful PUBREC coalesces another PUBREL. Matching PUBCOMP after
 * reconnection can replenish send quota up to its new initial maximum even
 * when that generation sent no corresponding PUBLISH.
 * PUBCOMP is ignored without credit until a matching PUBREL was emitted;
 * other unknown/wrong-stage ACKs are protocol errors. No ACK metadata/payload
 * is copied here. */
XMQTT_API xmqttsessionstatus xmqttSessionAcknowledge(xmqttsession* Session,const xmqttpacket* Ack);
/* Incoming QoS2 identity storage, independent of outgoing ids. The transport
 * validates packet/alias/ReceiveMaximum and reserves local delivery resources
 * before Prepare. DONE reserves a new id, DUPLICATE suppresses another delivery.
 * Commit is allocation-free and must precede making the already prepared local
 * delivery visible and emitting PUBREC; Abort rolls back only a prepared id.
 * These functions do not store application payload or prove durable acceptance. */
XMQTT_API xmqttsessionstatus xmqttSessionReceivePrepare(xmqttsession* Session,uint16 Identifier);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveCommit(xmqttsession* Session,uint16 Identifier);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveAbort(xmqttsession* Session,uint16 Identifier);
/* Received validated PUBREL: DONE sets PUBCOMP reason0; unknown id sets reason
 * 0x92 in v5 (0 in311), EMPTY. Output changes only for those two outcomes.
 * PubcompSent removes known state only after complete PUBCOMP emission. */
XMQTT_API xmqttsessionstatus xmqttSessionReceiveRelease(xmqttsession* Session,uint16 Identifier,uint8* Reason);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveComplete(xmqttsession* Session,uint16 Identifier);
XMQTT_API bool xmqttSessionInfo(xmqttsession* Session,xmqttsessioninfo* Info);
XMQTT_API const xmqttpacket* xmqttSessionPacket(const xmqttsessionpacket* Packet);
XMQTT_API bool xmqttSessionPacketInfo(const xmqttsessionpacket* Packet,xmqttsessionpacketinfo* Info);
/* Successfully held packet references share a UINT32_MAX/2 cap, including
 * active/selected state and caller frames. Next speculatively adds two internal
 * references, checks and refunds them on refusal before changing ready/quota/
 * selection/output state. Its protected active reference and sole protocol
 * owner bound this internal count to cap+2; no overflowing frame is published.
 * Capacity may be briefly reserved by such a pending Next. Insufficient room
 * is RESTRICTED; releases allow retry. Retain uses CAS at the same public cap.
 * A stable live reference is required on entry. */
XMQTT_API bool xmqttSessionPacketRetain(const xmqttsessionpacket* Packet);
XMQTT_API void xmqttSessionPacketRelease(const xmqttsessionpacket* Packet);
/* Offline unique-owner destruction; stop concurrent Session entries first.
 * Retained packets may outlive it; they keep only bounded state/allocator code.
 * A connected session must first End, so destruction cannot hide live state loss. */
XMQTT_API bool xmqttSessionDestroy(xmqttsession* Session);
XRT_EXTERN_C_END
#endif
#endif
