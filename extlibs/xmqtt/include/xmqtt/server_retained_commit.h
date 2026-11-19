#ifndef XMQTT_SERVER_RETAINED_COMMIT_H
#define XMQTT_SERVER_RETAINED_COMMIT_H
#include <xmqtt/server_store.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER_RETAINED_COMMIT)
#if !defined(XMQTT_FEATURE_XMQTT_SERVER_STORE)
#error "MQTT retained commits require broker state storage"
#endif
XRT_EXTERN_C_BEGIN

typedef struct xmqttserverretainedticket xmqttserverretainedticket;
typedef enum xmqttserverretainedresult {
	XMQTT_SERVER_RETAINED_COMMITTED = 0,
	XMQTT_SERVER_RETAINED_REJECTED,
	XMQTT_SERVER_RETAINED_UNKNOWN
} xmqttserverretainedresult;

/* Submit runs on the publishing worker outside broker locks. Different
 * publishing workers can call Submit concurrently. Return promptly.
 * True transfers one completion duty to the APP. False declines the request
 * before any durable effect. The APP may use the immutable record through
 * successful completion. Record.RemainingMilliseconds is measured at Received,
 * an XRT monotonic timestamp. Remove marks a tombstone for the PUBLISH topic.
 * Keep Submit and User alive through broker retirement. */
typedef bool (*xmqttserverretainedsubmit)(xmqttserverretainedticket* Ticket,
	xstrview Topic, const xmqttserverretainedrecord* Record, bool Remove, double Received, ptr User);

/* Configure a fresh broker before restoring sessions or attaching connections.
 * PendingBytes bounds aggregate ticket memory. One affected shard and one publishing peer
 * per ticket allow unrelated shards/peers to progress concurrently.
 * This mode handles retained client PUBLISH packets with QoS 0, 1 and 2.
 * Retained Wills are refused in CONNECT while the backend is enabled.
 * Restore the backend's retained checkpoint before attachment. */
XMQTT_API bool xmqttServerSetRetainedCommit(xmqttserver* Server, size_t PendingBytes,
	xmqttserverretainedsubmit Submit, ptr User);

/* Thread safe for the unique completion owner. COMMITTED confirms durability.
 * REJECTED certifies no durable change. UNKNOWN seals broker admission and
 * requires recovery from the authoritative backend before reopening. True
 * consumes Ticket and ends all access to its record. False leaves it owned
 * by the caller. Complete accepted tickets even after broker Close. WaitFor
 * includes their broker-side retirement. Join APP completion callers before
 * destroying the broker and its engine. */
XMQTT_API bool xmqttServerCompleteRetained(xmqttserverretainedticket* Ticket,
	xmqttserverretainedresult Result);

XRT_EXTERN_C_END
#endif
#endif
