#ifndef XMQTT_CLIENT_H
#define XMQTT_CLIENT_H
#include <xmqtt/negotiation.h>
#include <xrt/tcp.h>
#include <xrt/future.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT) && (!defined(XMQTT_FEATURE_XMQTT_NEGOTIATION) || !defined(XRT_FEATURE_NET_TCP_RETIRE) || !defined(XRT_FEATURE_FUTURE))
#error "MQTT client requires negotiation, TCP retirement waiting and Future"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
typedef struct xmqttclient xmqttclient;
typedef struct xmqttmessage xmqttmessage;
typedef enum xmqttclientstate {XMQTT_CLIENT_CONNECTING=0,XMQTT_CLIENT_OPEN,XMQTT_CLIENT_CLOSING,XMQTT_CLIENT_CLOSED} xmqttclientstate;
typedef enum xmqttclientstatus {
    XMQTT_STATUS_OK=0,XMQTT_STATUS_REJECTED,XMQTT_STATUS_TIMEOUT,XMQTT_STATUS_CANCELLED,
    XMQTT_STATUS_CLOSED,XMQTT_STATUS_TRANSPORT,XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_LIMIT,
    XMQTT_STATUS_MEMORY,XMQTT_STATUS_UNSUPPORTED,XMQTT_STATUS_SESSION,
    XMQTT_STATUS_INITIALIZATION,XMQTT_STATUS_SERVER_DISCONNECT,XMQTT_STATUS_RESTRICTED
} xmqttclientstatus;
typedef enum xmqttdelivery {XMQTT_NOT_SENT=0,XMQTT_MAY_HAVE_REACHED_PEER,XMQTT_PEER_ACCEPTED,XMQTT_PEER_REJECTED} xmqttdelivery;
typedef enum xmqttclientadmission {XMQTT_ADMISSION_ERROR=-1,XMQTT_ADMISSION_ACCEPTED=0,XMQTT_ADMISSION_OVERLOADED,XMQTT_ADMISSION_CLOSED} xmqttclientadmission;
/* A resolved Future is a typed outcome, not necessarily operation success.
 * QoS-0 OK observes complete native write-buffer retirement while open; it
 * does not acknowledge broker receipt, routing, storage or subscriber delivery.
 * Accepting ANY prefix into TCP makes delivery uncertain. Never auto-replayed.
 * QoS-1 completes after a corresponding PUBACK; PEER_ACCEPTED/REJECTED denotes
 * its decision, not subscriber delivery or durable storage. Puback (if present)
 * owns its reason/properties and may be separately retained. Local metadata
 * exhaustion can return LIMIT/MEMORY after an ACK, retaining its scalar decision.
 * Cause is borrowed from the result; retained results do not retain Client.
 * The original four-field prefix is unchanged; new fields are appended. */
typedef struct xmqttwriteresult {
    xmqttclientstatus Status;xmqttdelivery Delivery;size_t AcceptedBytes;const xerror* Cause;
    uint16 Identifier;uint8 Reason;const xmqttmessage* Puback;
} xmqttwriteresult;
/* Connack and Negotiated views belong to this Future. The optional message
 * may be separately retained. No connection/engine reference survives in it. */
typedef struct xmqttreadyresult {
    xmqttclientstatus Status;uint8 Reason;xmqttnegotiated Negotiated;
    const xmqttmessage* Connack;const xerror* Cause;
} xmqttreadyresult;
/* Nonblocking IO-worker notification. May take messages, submit or Close, but
 * must not block or Wait/Destroy. Code/User stay alive through Destroy. */
typedef void (*xmqttclientnotify)(xmqttclient* Client,ptr User);
typedef struct xmqttclientconfig {
    size_t Size;xmqttversion Version;xnetstreamconfig Stream;xmqttcodecconfig Codec;
    size_t MaxRequests,MaxOutgoingBytes,MaxObjects,MaxObjectBytes,MaxMessages,MaxMessageBytes;
    size_t MaxPrivateBytes,MaxSetupBytes,InitialInputBytes,ReadBytes,WriteBytes,WriteChunkBytes,WorkPackets;
    int64 SetupTimeout,PingTimeout;
    xmqttclientnotify Notify;ptr User;
    int64 CloseTimeout; /* Positive diagnostic drain deadline, default 5000 ms. */
} xmqttclientconfig;
typedef struct xmqttclientinfo {
    size_t Size;xmqttclientstate State;xmqttclientstatus CloseStatus;uint8 ServerReason;
    bool Ready;size_t Requests,OutgoingBytes,Messages,MessageBytes,Objects,ObjectBytes,PrivateBytes,StartupBytes;
    uint64 Accepted,Rejected,Completed,Received,ProtocolErrors,Pings,Pongs;
    /* Outgoing reason and bytes accepted into native output (0..3 for diagnostics).
     * Neither proves peer receipt. Diagnostic timeout preserves the original error. */
    uint8 LocalReason;bool DrainTimedOut,DrainStarted;size_t DisconnectBytes;
} xmqttclientinfo;
XRT_EXTERN_C_BEGIN
/* MQTT 5, finite codec defaults; 128 requests/4 MiB outgoing; 512 objects/16 MiB,
 * 64 queued messages/8 MiB; 4 MiB private, 64 KiB setup, 1 KiB initial input;
 * 64 KiB read/write work, 16 KiB chunks, 64 packets/turn; 5s setup/ping/diagnostic
 * close timeout. CloseTimeout must be positive; all durations use milliseconds. */
XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* Config);
/* One numeric TCP connection on a running host Engine, no TLS/DNS/fallback or
 * implicit replay. CONNECT is fully encoded/copied before return; callbacks may
 * run before return. Setup timeout starts at factory entry. Caller owns one
 * unique Client; Close/Wait/Destroy it before destroying Engine. Output changes
 * only on success. Startup wire is wiped after handshake/stops AND its final
 * native reference. Caller/native/system copies are outside that contract.
 * This vertical slice supports basic CONNECT auth, CONNACK, keep-alive, QoS-0/1
 * receives/sends and server DISCONNECT. Incoming QoS-1 is acknowledged only after
 * an owned message enters the bounded local queue, independent of application
 * Take/Release. PUBACKs are ordered, bounded by min(MaxMessages, ReceiveMaximum),
 * lazily charged to private bytes, and never interleave a partial outgoing frame.
 * Enhanced auth and positive receive alias capacity are explicitly unsupported;
 * QoS 2, subscriptions, reconnect,
 * durable sessions remain separate pending work.
 * No state is restored: MQTT 5 SessionPresent without local state closes.
 * Explicit Maximum Packet Size must not exceed the local codec packet cap.
 * After accepted MQTT5 negotiation, protocol failures attempt a final minimal
 * diagnostic DISCONNECT at a frame boundary, bounded by CloseTimeout including
 * native queue drain. Partial frames, peer caps, EOF, resource failure or explicit
 * Close force abort instead. The original typed error survives every fallback. */
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Client);
/* Shared readiness Future, always typed. Explicit Future cancellation requests
 * Close; raw cancel-token requests are sampled on IO/deadline progress. */
XMQTT_API xfuture* xmqttClientReady(xmqttclient* Client);
/* Thread-safe finite admission, immutable preencoded copy. QoS-0/1 new PUBLISH
 * (DUP=0), with a nonempty topic and no topic alias. QoS-1 Identifier=0 assigns
 * an unused id; nonzero reserves the explicit id, with normal OVERLOADED on
 * conflict. Its physical reservation lasts until real request retirement.
 * A bounded bitmap/hash table is allocated lazily on the first QoS-1 attempt,
 * charged to MaxPrivateBytes and retained through Destroy, even on later
 * admission failure. Peer Receive Maximum limits QoS-1 sends; QoS-0/control
 * traffic may continue while it is exhausted. May queue while connecting, but no
 * publish is sent before accepted CONNACK. Timeout includes encoding/queueing.
 * Future output changes on ACCEPTED only; normal overload/closed is counted.
 * Explicit Future cancellation wakes the owner; raw token sampled on progress.
 * Cancelling/timing out an already accepted prefix retires the connection. */
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Same operation/deadline; rejects ANY worker of the same Engine. */
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Stop new admissions after accepted CONNACK, drain all accepted publishes and
 * their QoS1 PUBACKs plus partial control frames, then send this final DISCONNECT
 * and gracefully close TCP. NULL Packet selects normal reason 0. Version 311
 * forbids properties/nonzero reason. Negotiated packet/expiry limits apply.
 * Snapshot/encode before return; one request and its wire use the existing
 * request/outgoing/object quotas. OVERLOADED/CLOSED leave Future/error unchanged.
 * Timeout is finite >=0 ms from entry, covering admission, drain and native close;
 * 0 expires immediately. Cancellation/Close/errors abort without reopening.
 * ACCEPTED returns a typed xmqttwriteresult Future: OK means the native queue
 * drained and TCP closed normally, NOT a broker ACK/Will-suppression guarantee.
 * AcceptedBytes and MAY_HAVE_REACHED_PEER are uncertain even on OK; Reason is
 * the requested reason, Identifier=0 and Puback=NULL. Failure preserves first
 * error and any accepted prefix. The result may outlive Client/Engine. WaitFor
 * additionally observes all actual references. Counters include this request. */
XMQTT_API xmqttclientadmission xmqttClientDisconnectAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Same operation/deadline; blocking wrapper rejects every same-Engine worker. */
XMQTT_API xmqttclientadmission xmqttClientDisconnect(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Transfer one queued message reference, false/unchanged on empty. Overfull
 * receive/object budgets close rather than silently dropping a packet. Already
 * queued messages remain available after Close; Destroy releases untaken ones.
 * Server DISCONNECT's reason is in Info, not added to the PUBLISH queue. */
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* Client,xmqttmessage** Message);
XMQTT_API const xmqttpacket* xmqttMessagePacket(const xmqttmessage* Message);
XMQTT_API bool xmqttMessageRetain(const xmqttmessage* Message);
XMQTT_API void xmqttMessageRelease(const xmqttmessage* Message);
XMQTT_API bool xmqttClientInfo(xmqttclient* Client,xmqttclientinfo* Info);
/* Abortive/nonblocking; application Close starts no MQTT DISCONNECT, so a broker
 * may publish Will. It can interrupt a previously started automatic error drain;
 * already accepted diagnostic bytes cannot be retracted. First error wins.
 * CLOSED is logical; Wait additionally observes real producer/post/timer/send/
 * event-owner and native stream reference retirement. Timeout preserves owner.
 * Wait uses completion signals without polling or wait-node allocation, with one
 * remaining timeout budget (native phase rounds to milliseconds). Concurrent
 * waiters borrow the same stable caller owner; stop all API entries before Destroy.
 * Wait/Destroy reject Engine workers. Repeated Close on CLOSED posts no new work. */
XMQTT_API bool xmqttClientClose(xmqttclient* Client);
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* Client,int64 Timeout);
XMQTT_API bool xmqttClientDestroy(xmqttclient* Client);
XRT_EXTERN_C_END
#endif
#endif
