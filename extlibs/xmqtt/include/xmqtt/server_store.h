#ifndef XMQTT_SERVER_STORE_H
#define XMQTT_SERVER_STORE_H
#include <xmqtt/server.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
#if !defined(XMQTT_FEATURE_XMQTT_SERVER)
#error "MQTT broker state storage requires the broker"
#endif
XRT_EXTERN_C_BEGIN

typedef struct xmqttserverretainedrecord {
	xmqttversion Version;
	xstrview PublisherId;
	xbytesview Publish;
	uint64 RemainingMilliseconds; /* Zero means the publication has no expiry. */
} xmqttserverretainedrecord;

/* The record and both views borrow broker storage for this call only. Return
 * false to stop the visit. The application chooses how to store the records. */
typedef bool (*xmqttserverretainedvisitor)(const xmqttserverretainedrecord* Record, ptr User);

/* Visit the retained state of a closed, drained broker before Destroy, on
 * its application owner thread. One monotonic cut for all records. Expired
 * publications are omitted and finite lifetimes rounded down to milliseconds.
 * Each record includes a complete source-version PUBLISH and original
 * publisher ID. Callbacks run outside broker locks and must leave the broker
 * alive. False means the caller should discard an incomplete checkpoint. */
XMQTT_API bool xmqttServerVisitRetained(xmqttserver* Server, xmqttserverretainedvisitor Visit, ptr User);

/* Restore a complete retained set into an empty broker on the application
 * owner thread, before sharing the broker with other threads or attaching
 * connections. ElapsedMilliseconds is time
 * since capture, including downtime. The caller validates its storage clock
 * and supplies trusted records. All wire packets, publisher IDs and expiry
 * bounds are validated, expired records are omitted and duplicate live topics
 * are rejected. On success the broker owns its copies. Failure leaves the
 * retained set empty. Application policy applies to later MQTT operations.
 * Restore MQTT 3.1.1 records only on a broker configured for MQTT 3.1.1. */
XMQTT_API bool xmqttServerRestoreRetained(xmqttserver* Server,
	const xmqttserverretainedrecord* Records, size_t Count, uint64 ElapsedMilliseconds);

/* An opaque session identity. Compare both words and the Client ID. */
typedef struct xmqttserversessionkey {
	uint64 High, Low;
} xmqttserversessionkey;

typedef enum xmqttserverrecordkind {
	XMQTT_SERVER_RECORD_SESSION = 1,
	XMQTT_SERVER_RECORD_SUBSCRIPTION,
	XMQTT_SERVER_RECORD_INCOMING_QOS2,
	XMQTT_SERVER_RECORD_QUEUED,
	XMQTT_SERVER_RECORD_SENT,
	XMQTT_SERVER_RECORD_WILL
} xmqttserverrecordkind;

/* Records follow their SESSION record. Unused fields contain zero.
 * SESSION: Data is Client ID, Code is MQTT version, Milliseconds is lifetime
 * and Identifier is the last outbound Packet Identifier allocated. Key
 * identifies this session across resumes and checkpoints. All-zero Key
 * denotes an older unkeyed checkpoint, accepted with the older callbacks.
 * SUBSCRIPTION: Data is the filter, Group is the optional shared group and
 * Code is SUBSCRIBE options. INCOMING_QOS2: Identifier and PUBREC reason Code.
 * QUEUED: Data is a PUBLISH template with Packet Identifier 1, Milliseconds
 * is its lifetime. SENT: same template with original Identifier, Code is the
 * expected ACK packet type and Value is the last wire Message Expiry Interval.
 * DeliveryKey identifies a QUEUED or SENT record created by an online commit.
 * An all-zero DeliveryKey uses only explicit checkpoint persistence.
 * WILL: Data is a delayed Will PUBLISH, Milliseconds is time until publication.
 * UINT64_MAX represents an unlimited SESSION or QUEUED lifetime. */
typedef struct xmqttserversessionrecord {
	xmqttserverrecordkind Kind;
	xbytesview Data, Group;
	uint64 Milliseconds;
	uint32 Value;
	uint16 Identifier;
	uint8 Code;
	xmqttserversessionkey Key, DeliveryKey;
} xmqttserversessionrecord;

typedef bool (*xmqttserversessionvisitor)(const xmqttserversessionrecord* Record, ptr User);

/* Seal admission and retire all transports, keeping persistent sessions and
 * previously armed delayed Wills for an application-owned checkpoint. The
 * ordinary shutdown policy for armed Wills on live connections applies.
 * This owner-thread operation is irreversible and repeatable after success. */
XMQTT_API bool xmqttServerCloseForCheckpoint(xmqttserver* Server);

/* After CloseForCheckpoint and WaitFor, visit session state outside locks.
 * The owner keeps the broker alive. Views are borrowed only for this call.
 * Keep the emitted order, including sent packet order and unsent FIFO order.
 * A visitor returning false leaves the checkpoint incomplete. */
XMQTT_API bool xmqttServerVisitSessions(xmqttserver* Server, xmqttserversessionvisitor Visit, ptr User);

/* On a new unattached broker owned by the caller, restore checkpoint records
 * with downtime deducted. Existing resource limits apply. Records are borrowed
 * only through this call. On failure no restored session is left installed.
 * A due delayed Will is routed by the broker's existing expiry worker. */
XMQTT_API bool xmqttServerRestoreSessions(xmqttserver* Server,
	const xmqttserversessionrecord* Records, size_t Count, uint64 ElapsedMilliseconds);

typedef struct xmqttserversessionresetticket xmqttserversessionresetticket;
typedef enum xmqttserversessionresetresult {
	XMQTT_SERVER_SESSION_RESET_COMMITTED = 0,
	XMQTT_SERVER_SESSION_RESET_REJECTED,
	XMQTT_SERVER_SESSION_RESET_UNKNOWN,
	XMQTT_SERVER_SESSION_COMMITTED = XMQTT_SERVER_SESSION_RESET_COMMITTED,
	XMQTT_SERVER_SESSION_REJECTED = XMQTT_SERVER_SESSION_RESET_REJECTED,
	XMQTT_SERVER_SESSION_UNKNOWN = XMQTT_SERVER_SESSION_RESET_UNKNOWN
} xmqttserversessionresetresult;

/* For a fresh session, durably discard all old session records for ClientId.
 * This also applies when Clean Start is false and the broker finds no
 * resumable session. The callback runs outside broker locks on the connection
 * worker. Return promptly. Different Client IDs can be submitted concurrently.
 * False refuses the connection before any durable effect. True transfers one
 * completion duty and ClientId remains borrowed until successful completion.
 * Complete accepted tickets even if the connection times out or closes. */
typedef bool (*xmqttserversessionresetsubmit)(xmqttserversessionresetticket* Ticket,
	xstrview ClientId, ptr User);

/* Install once before session restore or transport attachment. With the hook
 * installed, a new session requires COMMITTED before the broker sends success.
 * The connection limit bounds outstanding resets. Until the reset retires,
 * other CONNECTs for the same Client ID receive Server Busy (3.1.1 code 3).
 * Keep Submit and User alive until the broker and APP workers drain. */
XMQTT_API bool xmqttServerSetSessionReset(xmqttserver* Server,
	xmqttserversessionresetsubmit Submit, ptr User);

/* Thread safe for the unique completion owner. True consumes Ticket and ends
 * access to ClientId. False leaves it owned by the caller. REJECTED certifies
 * no durable effect. UNKNOWN closes broker admission and requires a restart
 * from the authoritative store. WaitFor includes pending reset retirement.
 * Join APP completion callers before destroying the broker and engine. */
XMQTT_API bool xmqttServerCompleteSessionReset(xmqttserversessionresetticket* Ticket,
	xmqttserversessionresetresult Result);

typedef xmqttserversessionresetticket xmqttserversessionticket;
typedef xmqttserversessionresetresult xmqttserversessionresult;
typedef enum xmqttserversessionoperation {
	XMQTT_SERVER_SESSION_DISCARD = 1,
	XMQTT_SERVER_SESSION_FINISH_INCOMING_QOS2,
	XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS2,
	XMQTT_SERVER_SESSION_SEND_OUTGOING,
	XMQTT_SERVER_SESSION_RELEASE_OUTGOING,
	XMQTT_SERVER_SESSION_FINISH_OUTGOING,
	XMQTT_SERVER_SESSION_ACCEPT_INCOMING_QOS1,
	XMQTT_SERVER_SESSION_UPSERT_SUBSCRIPTIONS,
	XMQTT_SERVER_SESSION_REMOVE_SUBSCRIPTIONS
} xmqttserversessionoperation;

/* DISCARD removes the entire ClientId group before fresh CONNECT succeeds.
 * Identifier is zero. FINISH_INCOMING_QOS2 removes a restored inbound QoS 2
 * record with this Identifier before PUBCOMP.
 * The operations are idempotent. A missing group or record is already removed.
 * Incoming acceptance and recipient delivery are not persisted by this hook.
 *
 * Submit runs on the connection worker outside broker locks. Return promptly.
 * Operations for one ClientId are serialized. Different workers may submit
 * concurrently. False guarantees no durable effect. True transfers exactly
 * one completion duty and ClientId stays borrowed through successful completion. */
typedef bool (*xmqttserversessionsubmit)(xmqttserversessionticket* Ticket,
	xmqttserversessionoperation Operation, xstrview ClientId, uint16 Identifier, ptr User);

/* Install once on a fresh broker before restoring or attaching connections.
 * Select this hook or the reset-only hook. A definitive PUBREL failure
 * closes that transport and keeps the identifier for resume. An unknown
 * outcome closes broker admission and invalidates session checkpoint export.
 * The connection limit bounds pending work. Keep callbacks and User alive
 * through broker retirement. */
XMQTT_API bool xmqttServerSetSessionCommit(xmqttserver* Server,
	xmqttserversessionsubmit Submit, ptr User);

/* The unique completion owner may call from any thread. True consumes the
 * ticket and invalidates ClientId; false leaves the duty with the caller.
 * Complete even after Close or timeout. WaitFor includes broker-side retirement.
 * Join APP completion callers before destroying the broker and engine. */
XMQTT_API bool xmqttServerCompleteSessionCommit(xmqttserversessionticket* Ticket,
	xmqttserversessionresult Result);

typedef struct xmqttservertransaction {
	xmqttserversessionoperation Operation;
	xstrview ClientId;
	xmqttserversessionkey Key;
	xmqttversion Version;
	uint32 ExpiryInterval; /* Seconds negotiated at CONNECT. */
	uint16 Identifier;
	uint8 Reason;
	const xmqttserverretainedrecord* Retained; /* Optional mutation in the same commit. */
	xstrview RetainedTopic;
	double RetainedReceived; /* XRT monotonic time at admission. */
	bool RemoveRetained;
	xmqttserversessionkey DeliveryKey;
	uint32 OutgoingExpiry; /* Message Expiry Interval written on first PUBLISH. */
	uint8 OutgoingPhase; /* Expected ACK after SEND or RELEASE; before FINISH. */
} xmqttservertransaction;

typedef struct xmqttservertransactionrecipient {
	xstrview ClientId;
	xmqttserversessionkey Key, DeliveryKey;
	xbytesview Publish; /* Recipient-version template with Packet Identifier 1. */
	uint64 RemainingMilliseconds; /* UINT64_MAX for no message expiry. */
} xmqttservertransactionrecipient;

typedef bool (*xmqttservertransactionvisitor)(const xmqttservertransactionrecipient* Recipient, ptr User);

/* DISCARD replaces the old ClientId group before a successful fresh CONNECT.
 * Atomically install a new session header when Key is nonzero; a zero Key
 * discards the group. FINISH_INCOMING_QOS2 removes the matching inbound ID
 * for this ClientId and Key before PUBCOMP. Treat a missing record as done.
 * ACCEPT_INCOMING_QOS2 commits one PUBLISH. In one transaction
 * add its inbound ID (if Key is nonzero), the optional Retained mutation
 * and all durable recipients enumerated
 * by xmqttServerVisitTransactionRecipients. Compare the current key for each
 * group. Skip replaced or expired recipient groups. A mismatched source key
 * makes the publication transaction fail. Volatile peers are handled by the broker.
 * ACCEPT_INCOMING_QOS1 commits recipients and the optional retained mutation
 * before PUBACK. Use the source key for fencing. Store no inbound identifier
 * for QoS 1. A retransmitted QoS 1 PUBLISH may produce duplicate deliveries.
 * If Retained is supplied, update RetainedTopic or remove it when RemoveRetained
 * is set. Its lifetime is measured at RetainedReceived. Remove an expired value.
 * Enable retained transaction composition with xmqttServerEnableRetainedTransactions
 * after configuring both runtime commit hooks against the same store.
 *
 * With outbound transactions enabled, persist each nonzero DeliveryKey
 * on its queued record. SEND_OUTGOING moves that record to SENT and updates
 * the SESSION's last Packet Identifier in the same commit. Preserve order
 * of first sends on replay. RELEASE_OUTGOING changes the matching SENT
 * record from PUBREC to PUBCOMP. FINISH_OUTGOING removes the record after
 * a terminal ACK or a broker discard. Identifier and OutgoingPhase are
 * both zero for a queued discard. Match the ClientId, Key, DeliveryKey
 * and the previous phase where applicable. A missing record can only be
 * treated as done for FINISH_OUTGOING. The broker sends PUBLISH or PUBREL
 * only after its phase is committed. A rejection closes that transport
 * and leaves the previous phase in memory for resume.
 *
 * UPSERT_SUBSCRIPTIONS and REMOVE_SUBSCRIPTIONS edit one keyed session
 * before the broker sends SUBACK or UNSUBACK. In one transaction verify
 * ClientId and Key, then upsert or remove the filters given by
 * xmqttServerVisitTransactionSubscriptions. Match both filter and shared
 * group. A missing removal is already done. Preserve unrelated records.
 *
 * The callback runs on a worker outside broker locks. Return promptly.
 * True transfers the completion duty and borrows Change until completion.
 * False certifies no durable effect. Use xmqttServerCompleteSessionCommit to
 * report a result. Accepted requests must complete even after broker Close. */
typedef bool (*xmqttservertransactionsubmit)(xmqttserversessionticket* Ticket,
	const xmqttservertransaction* Change, ptr User);

/* Configure a fresh broker before restore or attach. Select one of the
 * session storage callbacks. Restore must supply a nonzero key for every
 * SESSION record. The APP supplies the backend and its time/expiry policy.
 * A positive PUBREC follows confirmation of the acceptance transaction.
 * Enable outbound commits separately when supported by the backend. */
XMQTT_API bool xmqttServerSetSessionTransactions(xmqttserver* Server,
	xmqttservertransactionsubmit Submit, ptr User);

/* Opt in before restore or transport attachment, after configuring
 * SetSessionTransactions. Only deliveries carrying a DeliveryKey use online
 * outbound commits. The key must survive checkpoint export and restore. */
XMQTT_API bool xmqttServerEnableOutboundTransactions(xmqttserver* Server);

/* Opt in after enabling outbound transactions, before restore or attach.
 * The backend handles ACCEPT_INCOMING_QOS1. Retained QoS 1 uses that
 * transaction when retained composition is enabled. On definite refusal,
 * MQTT 5 returns a negative PUBACK and MQTT 3.1.1 closes without PUBACK.
 * A broker without this option keeps its existing QoS 1 path. */
XMQTT_API bool xmqttServerEnableQos1Transactions(xmqttserver* Server);

/* Opt in after configuring SetSessionTransactions and before restoring or
 * attaching. Each packet with successful edits to a persistent session
 * submits one operation. Existing routing stays active pending completion.
 * New filters start matching after COMMITTED. On REJECTED the previous
 * state is preserved. MQTT 3.1.1 closes if removal cannot be acknowledged. */
XMQTT_API bool xmqttServerEnableSubscriptionTransactions(xmqttserver* Server);

/* Visit final per-filter edits for one subscription transaction before its
 * completion. Each record has Kind SUBSCRIPTION. Data is the filter, Group
 * is the share name or empty, and Code is the options for an upsert.
 * Repeated filters expose the final successful edit only. Views are
 * borrowed for the visit. False from Visit ends the visit. */
XMQTT_API bool xmqttServerVisitTransactionSubscriptions(const xmqttserversessionticket* Ticket,
	xmqttserversessionvisitor Visit, ptr User);

/* The ticket owner may visit durable recipients before completing an
 * acceptance transaction. The callback borrows each record and all views
 * for its duration. Remaining time is sampled once per visit; expired
 * legacy deliveries are omitted. Keep keyed records with zero remaining
 * time until the broker retires them or their session expires.
 * False from the visitor ends the visit. */
XMQTT_API bool xmqttServerVisitTransactionRecipients(const xmqttserversessionticket* Ticket,
	xmqttservertransactionvisitor Visit, ptr User);

XRT_EXTERN_C_END
#endif
#endif
