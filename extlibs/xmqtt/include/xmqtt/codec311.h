#ifndef XMQTT_CODEC311_H
#define XMQTT_CODEC311_H
#include <xmqtt/features.h>
#include <xmqtt/export.h>
#include <xmqtt/packet.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC311) && !defined(XMQTT_FEATURE_XMQTT_PACKET)
#error "XMQTT_FEATURE_XMQTT_CODEC311 requires packet feature"
#endif

#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
XRT_EXTERN_C_BEGIN

/* Decode one MQTT 3.1.1 control packet (all 14 types). Same publication and
 * borrowed lifetime model as Read5: Packet only on DONE; MORE changes no Packet,
 * Issue or scratch; API misuse changes neither Packet nor Issue. Negative wire/
 * resource results publish independent Issue context even on diagnostic OOM.
 * Topic scratch is provisional on errors. Explicit sender direction, stateless
 * full-prefix resupply, no network/session actions or retained callbacks.
 * Config and Buffers metadata are copied. Only Buffers.Topics/TopicCapacity are
 * used/validated; property scratch fields are ignored without dereferencing.
 * Topic scratch, Packet and optional Issue must be mutually disjoint and cannot
 * overlap Input/Config/Buffers metadata. Data/reason views borrow Input; topic
 * lists borrow scratch. Wire.Size consumes one actual packet, leaving suffix.
 * Accept the wire leaf's 1..4-byte nonminimal Remaining Length compatibility;
 * enforce fixed body lengths, roles (DISCONNECT client-only), flag/id/return-
 * code/topic/filter/UTF-8 rules. CleanStart means Clean Session. Empty ClientId
 * requires Clean Session; nonempty IDs may exceed 23 bytes and ASCII. Password
 * requires username. '$share/' is ordinary filter text, without 5.0 semantics.
 * Payload/Will/password are opaque binary; no PFI/property/alias/auth packet.
 * MaxPropertyBytes/MaxProperties do not consume a budget in 3.1.1 (config still
 * must be well-formed). No success/MORE heap allocation or data copy.
 * Negotiated session state, requested SUBACK count/granted QoS, identifier use,
 * ordering, delivery/durability/keepalive/reconnect remain runtime obligations. */
XMQTT_API xmqttcodecstatus xmqttPacketRead311(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue);

/* Independent, atomic descriptor preflight and canonical minimal encoding.
 * All failures preserve Required/Written/wire. Each active string is validated
 * once per call; emission copies data once with no text rescan. Size then Write
 * repeats preflight. Output/size objects cannot overlap Packet/config/active
 * vectors/data or each other. Capacity is a nonwrapping range. Wire metadata is
 * ignored. No ownership transfer, retention, success allocation or callbacks.
 * 5.0 properties, subscription options, nonzero ACK/DISCONNECT reasons and
 * UNSUBACK reason lists are rejected, preventing silent feature loss. AUTH and
 * server DISCONNECT are unavailable. PUBLISH data bytes are always opaque;
 * 3.1.1 cannot detect a peer's wire version by inspecting arbitrary payloads. */
XMQTT_API bool xmqttPacketSize311(const xmqttpacket* Packet, xmqttdirection Direction,
	const xmqttcodecconfig* Config, size_t* Required);
XMQTT_API bool xmqttPacketWrite311(ptr Output, size_t Capacity,
	const xmqttpacket* Packet, xmqttdirection Direction,
	const xmqttcodecconfig* Config, size_t* Written);

XRT_EXTERN_C_END
#endif
#endif
