#ifndef XMQTT_SERVER_H
#define XMQTT_SERVER_H
#include <xmqtt/codec5.h>
#include <xrt/tcp.h>

#if defined(XMQTT_FEATURE_XMQTT_SERVER) && (!defined(XMQTT_FEATURE_XMQTT_CODEC5) || !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_COND) || !defined(XRT_FEATURE_MAP) || !defined(XRT_FEATURE_AVL) || !defined(XRT_FEATURE_RWLOCK) || !defined(XRT_FEATURE_HASH_KEYED) || !defined(XRT_FEATURE_RANDOM_SECURE))
#error "MQTT server requires codec5, native TCP, condition variables, map, AVL, read-write locks, keyed hash and secure random"
#endif

#if defined(XMQTT_FEATURE_XMQTT_SERVER)
XRT_EXTERN_C_BEGIN
#define XMQTT_SERVER_MAX_PROPERTIES 64u
#define XMQTT_SERVER_MAX_FILTERS 128u
#define XMQTT_SERVER_MAX_WILDCARD_LEVELS 128u

typedef struct xmqttserver xmqttserver;
typedef enum xmqttserverpermission {
	XMQTT_SERVER_ALLOW = 0,
	XMQTT_SERVER_DENY,
	XMQTT_SERVER_BUSY
} xmqttserverpermission;

/* Runs on the connection's XRT worker outside library locks. Borrow CONNECT
 * only through this call. Keep the callback and User alive through Destroy.
 * Return promptly. Call Close to seal admission from a callback if needed. */
typedef xmqttserverpermission (*xmqttserverauthorize)(xmqttversion Version,
	const xmqttconnectpacket* Connect, ptr User);
/* Worker-local policy check with borrowed ClientId and topic/filter. The same
 * callback and User lifetime rules as CONNECT authorization apply. */
typedef xmqttserverpermission (*xmqttserverauthorizetopic)(xmqttversion Version,
	xstrview ClientId, xstrview Topic, bool Publishing, ptr User);

typedef struct xmqttserverconfig {
	size_t Size, MaxConnections, MaxClientIdBytes;
	size_t MaxPersistentSessions; /* Zero disables session retention across disconnect. */
	size_t MaxSubscriptions, MaxSubscriptionsPerConnection, MaxSubscribersPerTopic, MaxTopicBytes;
	size_t MaxWildcardNodes; /* Zero disables wildcard subscriptions. */
	size_t MaxRetainedTopics, MaxRetainedBytes; /* Either zero disables retention. */
	/* QoS 1 and 2 share these quotas. The QoS 1 field names are compatible aliases. */
	union { size_t MaxOutgoingPublishes, MaxOutgoingQos1; };
	union { size_t MaxOutgoingPublishesPerConnection, MaxOutgoingQos1PerConnection; };
	union { size_t MaxOutgoingPublishBytes, MaxOutgoingQos1Bytes; };
	union { size_t MaxIncomingPublishes, MaxIncomingQos1; };
	uint16 ReceiveMaximum; /* Per connection, advertised in MQTT 5 CONNACK. */
	int64 ConnectTimeout;
	xmqttcodecconfig Codec;
	bool Allow311;
	xmqttserverauthorize Authorize;
	xmqttserverauthorizetopic AuthorizeTopic;
	ptr User;
} xmqttserverconfig;

typedef struct xmqttserverinfo {
	size_t Connections, Established, Subscriptions, WildcardNodes;
	size_t PersistentSessions, OfflineSessions;
	union { size_t IncomingPublishes, IncomingQos1; };
	union { size_t OutgoingPublishes, OutgoingQos1; };
	union { size_t OutgoingPublishBytes, OutgoingQos1Bytes; };
	size_t RetainedTopics, RetainedBytes; /* Resident records and bytes for wire data and publisher IDs. */
	uint64 Accepted, Refused, TimedOut;
	/* Forwarded counts native queue acceptance. DroppedDeliveries counts
	 * matching deliveries refused before acceptance. */
	uint64 Forwarded, DroppedDeliveries, RejectedPublications, DroppedRetained;
	union { uint64 OutgoingPublishOverloads, OutgoingQos1Overloads; };
	bool Closing;
} xmqttserverinfo;

/* Defaults: 1024 connections, 256-byte IDs, 5 s CONNECT deadline, 16 KiB
 * wire cap, max 64 properties and 128 filters per request. MQTT 5.0 only.
 * Up to 1024 persistent sessions, held in memory across TCP connections.
 * Up to 65536 subscriptions total, 64 per connection, 1024 subscribers
 * per filter, 1024 bytes per topic/filter, and 65536 wildcard index nodes.
 * Wildcard filters have at most 128 levels. Retention defaults to 4096
 * topics and 16 MiB for wire data and publisher IDs. Inbound QoS 1/2 defaults
 * to 64 outstanding publications per connection and 65536 per server.
 * Outbound QoS 1/2 defaults to 256 deliveries per connection, 16384 per server
 * and 64 MiB of charged wire bytes. Authorizers default to anonymous access.
 * Install application policy before listening externally.
 * Compile xmqtt_codec311 as well to enable Allow311. */
XMQTT_API bool xmqttServerConfigInit(xmqttserverconfig* Config);
XMQTT_API bool xmqttServerCreate(xnetengine* Engine, const xmqttserverconfig* Config,
	xmqttserver** Server);

/* Call on the native Worker from an XRT TCP Accept callback with a new
 * Stream. Return this result from that callback. True transfers the accepted
 * Stream reference. The listener belongs to the application and its stream
 * ReadLimit must accommodate Codec.MaxPacketBytes. Server borrows and pins
 * Engine until Destroy. */
XMQTT_API bool xmqttServerAttach(xmqttserver* Server, xnetstream* Stream);
XMQTT_API bool xmqttServerInfo(xmqttserver* Server, xmqttserverinfo* Info);
/* Close seals admission and asynchronously aborts active connections. Wait
 * joins owned streams and timers, outside Engine workers. Stop/detach all
 * producers and join listener callbacks before the unique owner calls
 * Destroy. Destroy requires a closed, drained Server. */
XMQTT_API bool xmqttServerClose(xmqttserver* Server);
XMQTT_API xwaitresult xmqttServerWaitFor(xmqttserver* Server, int64 Timeout);
XMQTT_API bool xmqttServerDestroy(xmqttserver* Server);
XRT_EXTERN_C_END
#endif
#endif
