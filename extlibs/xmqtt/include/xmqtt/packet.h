#ifndef XMQTT_PACKET_H
#define XMQTT_PACKET_H
#include <xmqtt/features.h>
#include <xmqtt/export.h>
#include <xmqtt/wire.h>

#if defined(XMQTT_FEATURE_XMQTT_PACKET) && !defined(XMQTT_FEATURE_XMQTT_WIRE)
#error "XMQTT_FEATURE_XMQTT_PACKET requires XMQTT_FEATURE_XMQTT_WIRE"
#endif

#if defined(XMQTT_FEATURE_XMQTT_PACKET)

#define XMQTT_SUBSCRIBE_QOS_MASK 0x03u
#define XMQTT_SUBSCRIBE_NO_LOCAL 0x04u
#define XMQTT_SUBSCRIBE_RETAIN_AS_PUBLISHED 0x08u
#define XMQTT_SUBSCRIBE_RETAIN_HANDLING_MASK 0x30u

struct xmqttproperty;
typedef struct xmqttpropertylist { const struct xmqttproperty* Data; size_t Count; } xmqttpropertylist;
typedef struct xmqttsubscription { xstrview Filter; uint8 Options; } xmqttsubscription;
typedef struct xmqttsubscriptionlist { const xmqttsubscription* Data; size_t Count; } xmqttsubscriptionlist;

typedef struct xmqttconnectpacket {
	bool CleanStart; /* Clean Session when used with the 3.1.1 codec. */
	uint16 KeepAlive;
	xstrview ClientId;
	bool HasWill, WillRetain;
	uint8 WillQos;
	xmqttpropertylist WillProperties;
	xstrview WillTopic;
	xbytesview WillPayload;
	bool HasUsername, HasPassword;
	xstrview Username;
	xbytesview Password;
} xmqttconnectpacket;

typedef struct xmqttpublishpacket { xstrview Topic; xbytesview Payload; } xmqttpublishpacket;
typedef union xmqttpacketbody {
	xmqttconnectpacket Connect;
	xmqttpublishpacket Publish;
	xmqttsubscriptionlist Topics; /* SUBSCRIBE and UNSUBSCRIBE (Options=0). */
	xbytesview Reasons; /* SUBACK/5.0 UNSUBACK: ordered bytes; 3.1.1 UNSUBACK empty. */
} xmqttpacketbody;

/* Only the union member selected by Type is meaningful. Flags is the actual
 * fixed-header low nibble, including mandatory 2 for PUBREL/SUBSCRIBE/UNSUBSCRIBE.
 * Identifier is required/nonzero for the corresponding packet types and QoS>0.
 * Reason is scalar for CONNACK/PUBACK/PUBREC/PUBREL/PUBCOMP/DISCONNECT/AUTH;
 * SessionPresent is CONNACK-only. Other unused common fields must be zero on
 * encoding. Wire is decoded borrowed metadata (Size is consumed bytes), ignored
 * by encoders. No data/list is owned; all type layouts are feature-independent. */
typedef struct xmqttpacket {
	xmqttpackettype Type;
	uint8 Flags;
	uint16 Identifier;
	uint8 Reason;
	bool SessionPresent;
	xmqttpropertylist Properties;
	xmqttpacketbody Body;
	xbytesview Wire;
} xmqttpacket;

typedef struct xmqttcodecconfig {
	size_t Size;
	size_t MaxPacketBytes, MaxPayloadBytes;
	/* Aggregate across CONNECT and Will sections, including length prefixes. */
	size_t MaxPropertyBytes, MaxProperties;
	/* Topic entries or SUBACK/UNSUBACK reason count. Zero is a real limit. */
	size_t MaxTopics;
	uint32 Utf8Flags;
} xmqttcodecconfig;

typedef struct xmqttcodecbuffers {
	struct xmqttproperty* Properties;
	size_t PropertyCapacity;
	xmqttsubscription* Topics;
	size_t TopicCapacity;
} xmqttcodecbuffers;

typedef enum xmqttcodecstatus {
	XMQTT_CODEC_UNSUPPORTED = -5, XMQTT_CODEC_PAYLOAD_ERROR = -4,
	XMQTT_CODEC_LOCAL_ERROR = -3, XMQTT_CODEC_PROTOCOL_ERROR = -2,
	XMQTT_CODEC_MALFORMED = -1, XMQTT_CODEC_MORE = 0, XMQTT_CODEC_DONE = 1
} xmqttcodecstatus;

typedef enum xmqttcodecerror {
	XMQTT_CODEC_OK = 0, XMQTT_CODEC_ARGUMENT = 3001, XMQTT_CODEC_LIMIT,
	XMQTT_CODEC_CAPACITY, XMQTT_CODEC_FORMAT, XMQTT_CODEC_PROTOCOL,
	XMQTT_CODEC_VERSION, XMQTT_CODEC_PAYLOAD_FORMAT, XMQTT_CODEC_TOPIC,
	XMQTT_CODEC_CLIENT_ID, XMQTT_CODEC_TOPIC_ALIAS, XMQTT_CODEC_TOPIC_ALIAS_REPEAT
} xmqttcodecerror;

/* Machine-readable failure context survives diagnostic OOM. Offset is a
 * zero-based field/section start (0 for the fixed header), or exact payload
 * UTF-8 error byte. Type/Flags/Identifier are only confirmed metadata, never a
 * partially executable packet; Identifier remains zero until validated. */
typedef struct xmqttcodecissue {
	xmqttcodecstatus Status;
	xmqttcodecerror Error;
	size_t Offset;
	xmqttpackettype Type;
	uint8 Flags;
	uint16 Identifier;
} xmqttcodecissue;

XRT_EXTERN_C_BEGIN

/* Zero the packet and set Type/mandatory fixed flags. Caller explicitly fills
 * required identifiers, payloads and connect flags. No allocation/retention;
 * preserves ambient error on success, leaves output unchanged on failure. */
XMQTT_API bool xmqttPacketInit(xmqttpackettype Type, xmqttpacket* Packet);
/* Defaults: packet/payload/property bytes 1 MiB, total properties 256, topics
 * 1024, mandatory string UTF-8 policy. Packet cap 2..PACKET_MAX_BYTES; other
 * caps may be zero. Config layout never changes with selected protocol leaves. */
XMQTT_API bool xmqttCodecConfigInit(xmqttcodecconfig* Config);

XRT_EXTERN_C_END
#endif
#endif
