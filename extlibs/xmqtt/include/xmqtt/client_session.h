#ifndef XMQTT_CLIENT_SESSION_H
#define XMQTT_CLIENT_SESSION_H
#include <xmqtt/client_inbox.h>
#include <xmqtt/session.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX) || !defined(XMQTT_FEATURE_XMQTT_SESSION))
#error "MQTT Client Session requires Client Inbox and Session"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XRT_EXTERN_C_BEGIN
/* Explicit numeric TCP connection using caller-owned protocol Session and
 * application Inbox, both claimed exclusively through actual Client Destroy.
 * Version/ClientId must match Session; Inbox has the same four Client caps.
 * Non-packet codec limits must fit Session, with the same UTF-8 policy. The
 * connection packet cap may grow to render an alias-expanded canonical replay;
 * Session borrows that codec only while claimed, never after Client destruction.
 * Bind Session/Inbox to one logical broker/security principal. Neither captures
 * credentials or owns an Engine. Constructor failure changes no Client output.
 * Old obligations reserve request slots/outgoing wire bytes during setup. A
 * successful accepted CONNACK restores only actual retained state; missing state
 * follows Session's version-specific reset/fail-closed policy. Replay borrows
 * immutable canonical packets, clears connection aliases and preserves QoS2
 * PUBREL progress. Queued pre-CONNACK requests keep normal Client semantics.
 * Every Future remains scoped to its original connection: interruption settles
 * it with the observed prefix/ACK, while unfinished sent obligations can outlive
 * that Client and resume on a later explicit connection. Zero-output cancelled
 * new requests are withdrawn; accepted prefixes retain protocol obligations.
 * A prior-generation ACK arriving during a partial PUBLISH replay commits its
 * protocol progress and retires that transport with SESSION status. This avoids
 * continuing an abandoned frame or introducing an unnecessary second delivery.
 * Accepted incoming QoS2 deliveries enter Inbox once before PUBREC and survive
 * reconnect; duplicate identities do not deliver another application message.
 * Destroy/Close never implies peer receipt. Actual network termination starts
 * Session expiry; complete DISCONNECT expiry overrides are respected.
 * QoS1/2 admission takes one immutable Session snapshot shared by the
 * request and protocol state. It consumes Session object/id/byte capacity before
 * readiness, but enters recoverable state only when selected for output. Limit
 * returns OVERLOADED without publishing a Future; unsent/failed setup frees the
 * snapshot after its actual last request reference. Encoding runs outside both
 * main gates. Alias transactions use a separate mutex from capture through
 * successful queue publication, including QoS0 bindings; encoding does not hold
 * the Client/Session main gates. Initial non-alias writes use one span; alias
 * writes gather up to four spans (two for compact aliases), with scalar fallback
 * under host queue pressure. Native loans retain both request and packet data.
 * Replay adds no payload copy. Independent request/packet headers remain separate
 * allocations; alias locking, small header copies and native leases have costs.
 * Optional client_session_tls composes TLS/Dial/Auth. Plain Dial/Auth composition,
 * assigned-id adoption, automatic reconnect and durable recovery remain separate
 * work. No production/performance acceptance implied. */
XMQTT_API bool xmqttClientConnectSession(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XRT_EXTERN_C_END
#endif
#endif
