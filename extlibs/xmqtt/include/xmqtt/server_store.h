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

XRT_EXTERN_C_END
#endif
#endif
