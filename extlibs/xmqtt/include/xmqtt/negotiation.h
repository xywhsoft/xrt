#ifndef XMQTT_NEGOTIATION_H
#define XMQTT_NEGOTIATION_H
#include <xmqtt/codec5.h>
#include <xmqtt/codec311.h>
#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION) && \
    (!defined(XMQTT_FEATURE_XMQTT_CODEC5) || !defined(XMQTT_FEATURE_XMQTT_CODEC311))
#error "MQTT negotiation requires both packet codecs"
#endif
#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)

typedef enum xmqttnegotiationstatus {
    XMQTT_NEGOTIATION_STATE_ERROR=-4, XMQTT_NEGOTIATION_RESTRICTED=-3,
    XMQTT_NEGOTIATION_PROTOCOL_ERROR=-2, XMQTT_NEGOTIATION_LOCAL_ERROR=-1,
    XMQTT_NEGOTIATION_REJECTED=0, XMQTT_NEGOTIATION_DONE=1
} xmqttnegotiationstatus;
typedef enum xmqttnegotiationerror {
    XMQTT_NEGOTIATION_OK=0, XMQTT_NEGOTIATION_ARGUMENT=4001,
    XMQTT_NEGOTIATION_SESSION, XMQTT_NEGOTIATION_CLIENT_ID,
    XMQTT_NEGOTIATION_AUTH_METHOD, XMQTT_NEGOTIATION_AUTH_DATA,
    XMQTT_NEGOTIATION_RESPONSE_INFO, XMQTT_NEGOTIATION_PACKET_SIZE,
    XMQTT_NEGOTIATION_QOS, XMQTT_NEGOTIATION_RETAIN,
    XMQTT_NEGOTIATION_TOPIC_ALIAS, XMQTT_NEGOTIATION_WILDCARD,
    XMQTT_NEGOTIATION_SHARED, XMQTT_NEGOTIATION_SUBSCRIPTION_ID,
    XMQTT_NEGOTIATION_SESSION_EXPIRY
} xmqttnegotiationerror;
typedef struct xmqttnegotiationissue {
    xmqttnegotiationstatus Status;
    xmqttnegotiationerror Error;
    xmqttpropertyid Property; /* Zero when no single property identifies it. */
} xmqttnegotiationissue;
typedef struct xmqttnegotiationconfig {
    size_t Size;
    xmqttversion Version;
    bool HasSessionState; /* Local durable/volatile state for this ClientId. */
    xmqttcodecconfig Codec;
} xmqttnegotiationconfig;
/* Caller-owned value, never a connection/session owner. ClientId/AuthMethod
 * borrow CONNECT or CONNACK text; ServerReference/ResponseInfo borrow CONNACK.
 * Preserve those byte views, not property scratch, while using the result.
 * Rejected results populate only Size/Version/Reason/ServerReference.
 * Packet/receive/alias limits are protocol values, not allocated local buffers
 * or available QoS credit. KeepAlive is seconds; expiry fields are MQTT 5 only. */
typedef struct xmqttnegotiated {
    size_t Size;
    xmqttversion Version;
    bool Accepted,SessionPresent,ServerAssignedId,HasAuthMethod;
    uint8 Reason,MaximumQos;
    bool RetainAvailable,WildcardAvailable,SharedAvailable,SubscriptionIdsAvailable;
    uint16 KeepAlive,SendReceiveMaximum,ReceiveMaximum,SendAliasMaximum,ReceiveAliasMaximum;
    uint32 SendPacketBytes,ReceivePacketBytes,RequestedSessionExpiry,SessionExpiry;
    xstrview ClientId,AuthMethod,ServerReference,ResponseInfo;
} xmqttnegotiated;
XRT_EXTERN_C_BEGIN
/* Defaults: MQTT 5, no local session, standard finite codec limits. */
XMQTT_API bool xmqttNegotiationConfigInit(xmqttnegotiationconfig* Config);
/* Full independent descriptor preflights, then CONNECT/CONNACK cross checks.
 * Explicit version; no fallback, network action, credential verification or
 * AUTH exchange state. Zero success/rejected heap allocations or text copies.
 * Publish Result on DONE/REJECTED only; all negative statuses preserve it.
 * Valid semantic decisions publish/clear Issue independently of diagnostic OOM.
 * Invalid API/descriptor/alias failures leave Issue unchanged (LOCAL_ERROR).
 * All outputs must be disjoint from active inputs/vectors/config/each other.
 * DONE/REJECTED preserve ambient error; diagnostics on other paths may allocate.
 * MQTT 5 SessionPresent with missing local state is STATE_ERROR: close; this is
 * not necessarily a malformed broker. MQTT 3.1.1 leaves mismatch policy to its
 * caller. A client continuing with MQTT 5 SessionPresent=0 must discard old
 * local state. This function never mutates that state or proves its durability. */
XMQTT_API xmqttnegotiationstatus xmqttNegotiate(const xmqttpacket* Connect,
    const xmqttpacket* Connack,const xmqttnegotiationconfig* Config,
    xmqttnegotiated* Result,xmqttnegotiationissue* Issue);
/* Independently validate and size a client packet against negotiated peer
 * capabilities. Required is optional and changes on DONE only. No encoding or
 * state mutation; no flow-credit/id ownership, alias-map, ACK correlation or
 * authentication sequence check. Cannot send a second CONNECT. Checks AUTH
 * method and original CONNECT expiry restrictions, including after a server
 * expiry override. SUBSCRIBE requested QoS may exceed MaximumQos; UNSUBSCRIBE
 * is not restricted by disabled wildcard/shared subscription availability.
 * Codec NULL uses finite defaults independently of the negotiation config.
 * Same atomicity, alias, error and allocation rules as xmqttNegotiate. */
XMQTT_API xmqttnegotiationstatus xmqttNegotiatedPacketCheck(const xmqttnegotiated* Peer,
    const xmqttpacket* Packet,const xmqttcodecconfig* Codec,
    size_t* Required,xmqttnegotiationissue* Issue);
XRT_EXTERN_C_END
#endif
#endif
