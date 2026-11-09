#ifndef XMQTT_CLIENT_INBOX_H
#define XMQTT_CLIENT_INBOX_H
#include <xmqtt/client.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX) && !defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "MQTT Client Inbox requires Client"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
typedef struct xmqttclientinbox xmqttclientinbox;
typedef struct xmqttclientinboxconfig {
    size_t Size;xmqttversion Version;
    size_t MaxObjects,MaxObjectBytes,MaxMessages,MaxMessageBytes;
} xmqttclientinboxconfig;
typedef struct xmqttclientinboxinfo {
    size_t Size;bool Attached;
    size_t Messages,MessageBytes,Objects,ObjectBytes;
} xmqttclientinboxinfo;
XRT_EXTERN_C_BEGIN
/* Passive, Engine-independent receive queue and shared result/message quota.
 * Defaults match Client: MQTT5, 512 objects/16 MiB, 64 queued/8 MiB.
 * Config is copied; fixed object/mutex/quota metadata is outside these budgets. */
XMQTT_API bool xmqttClientInboxConfigInit(xmqttclientinboxconfig* Config);
XMQTT_API bool xmqttClientInboxCreate(const xmqttclientinboxconfig* Config,xmqttclientinbox** Inbox);
/* One numeric TCP Client at a time. Version and the four Client object/message
 * caps must exactly match Inbox. The exclusive attachment lasts through actual
 * Client Destroy, including failed/closed connections; failure changes no output.
 * Only accepted PUBLISH messages enter Inbox. Ready/ACK/result objects share its
 * quota, including retained objects from earlier Clients and Engines.
 * This preserves application messages, not MQTT session protocol state: CONNACK
 * SessionPresent without local state still fails. No automatic reconnect/replay.
 * TLS/Dial/Auth composition and session attachment remain separate work. */
XMQTT_API bool xmqttClientConnectInbox(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
/* Thread-safe with stable owner. Transfer one reference, false/unchanged when
 * empty. ClientTakeMessage consumes this same queue, including earlier messages.
 * No callback runs under Inbox locks. Take works while offline; neither Take
 * returns MQTT protocol credit. Message bytes are the existing owned allocation. */
XMQTT_API bool xmqttClientInboxTake(xmqttclientinbox* Inbox,xmqttmessage** Message);
/* Queue and object gauges are separately sampled; objects include retained
 * taken messages/results, while Messages counts only queued PUBLISH messages.
 * ClientInfo reports these shared gauges and per-connection Received/counters. */
XMQTT_API bool xmqttClientInboxInfo(xmqttclientinbox* Inbox,xmqttclientinboxinfo* Info);
/* Reject while attached, preserving Inbox. Stop all concurrent Inbox API entries
 * before Destroy. Releases untaken messages; taken messages/results may outlive
 * Inbox and every Client/Engine. NULL is accepted. */
XMQTT_API bool xmqttClientInboxDestroy(xmqttclientinbox* Inbox);
XRT_EXTERN_C_END
#endif
#endif
