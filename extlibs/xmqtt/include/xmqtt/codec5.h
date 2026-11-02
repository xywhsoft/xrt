#ifndef XMQTT_CODEC5_H
#define XMQTT_CODEC5_H
#include <xmqtt/features.h>
#include <xmqtt/export.h>
#include <xmqtt/packet.h>
#include <xmqtt/properties.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC5) && \
	(!defined(XMQTT_FEATURE_XMQTT_PACKET) || !defined(XMQTT_FEATURE_XMQTT_PROPERTIES))
#error "XMQTT_FEATURE_XMQTT_CODEC5 requires packet and properties features"
#endif

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)
XRT_EXTERN_C_BEGIN

/* Decode one complete MQTT 5.0 packet of any of the 15 control types. Explicit
 * sender direction, no network/session side effects or retained callbacks.
 * Header cap checked before buffering; MORE is stateless, resupply full prefix.
 * Packet and optional Issue remain unchanged on MORE/invalid API use. Packet
 * publishes only on DONE; property/topic scratch is provisional on other
 * results and must not be executed/exposed. On confirmed wire/resource errors,
 * optional Issue publishes typed context independently of diagnostic OOM.
 * DONE clears Issue (Error=OK, Offset=NPOS) and preserves ambient error.
 * Returned Wire/text/binary/reasons borrow Input; lists borrow caller scratch.
 * Keep both alive/unchanged while using Packet. NULL Buffers supplies zero
 * scratch. All outputs/arrays must be disjoint from Input/Config/Buffers and
 * each other. Config and Buffers metadata are copied for the call.
 * Covers packet bodies, ids, flags, standard properties, topic/filter/shared
 * syntax, subscription options, reason domains, omitted forms and PFI UTF-8
 * payloads (NUL is legal character data). No successful/MORE allocation or
 * payload copy. Negative statuses distinguish local limits/capacity/API,
 * malformed wire, protocol violations, unsupported CONNECT protocol, and
 * invalid indicated payload format; cold diagnostics can allocate.
 * Negotiated auth/aliases/capabilities, session order, requested ACK counts and
 * QoS/durability obligations remain runtime checks. Short AUTH success syntax
 * is accepted; authentication negotiation requirements are not inferred. */
XMQTT_API xmqttcodecstatus xmqttPacketRead5(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue);

/* Independently preflight the complete immutable descriptor, every active view,
 * properties and aggregate resource limits. No Size result is trusted by Write.
 * All failures preserve outputs; Required/Written/wire cannot overlap active
 * source data/vectors/Packet/config or each other. Capacity is a nonwrapping
 * buffer range. No terminating NUL. Config NULL uses defaults, copied per call.
 * Emit canonical shortest fixed integers and abbreviated zero-reason/no-property
 * ACK/DISCONNECT/AUTH forms. Preserve property/topic/reason order and data bytes.
 * Text/property validation occurs once per call, not again during emission;
 * Size followed by Write performs two independent preflights. Each payload/
 * string/binary field copies once. No allocation/ownership transfer on success. */
XMQTT_API bool xmqttPacketSize5(const xmqttpacket* Packet, xmqttdirection Direction,
	const xmqttcodecconfig* Config, size_t* Required);
XMQTT_API bool xmqttPacketWrite5(ptr Output, size_t Capacity,
	const xmqttpacket* Packet, xmqttdirection Direction,
	const xmqttcodecconfig* Config, size_t* Written);

XRT_EXTERN_C_END
#endif
#endif
