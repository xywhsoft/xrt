#ifndef XMQTT_INTERNAL_H
#define XMQTT_INTERNAL_H
#include <xmqtt/wire.h>

#if (defined(__GNUC__) || defined(__clang__)) && !defined(_WIN32)
#define XMQTT_INTERNAL __attribute__((visibility("hidden")))
#else
#define XMQTT_INTERNAL
#endif

#if defined(XMQTT_FEATURE_XMQTT_WIRE)
/* Library-only helpers; caller has validated spans/config/output aliases.
 * Failure is independent of diagnostic allocation and meaningful on ERROR. */
XMQTT_INTERNAL bool xmqttInternalFlags(xmqttpackettype Type, uint8 Flags, xmqttversion Version);
XMQTT_INTERNAL xmqttwirestatus xmqttInternalHeader(xbytesview Input,
	const xmqttwireconfig* Config, xmqttfixedheader* Header, xmqttwireerror* Failure);
#endif

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
#include <xmqtt/properties.h>
/* A packet encoder prepares each property section exactly once, checks global
 * aliases, then emits the prepared immutable vector without another text scan. */
XMQTT_INTERNAL bool xmqttInternalPropertiesPrepare(const xmqttproperty* Properties,
	size_t Count, xmqttpropertyscope Scope, xmqttdirection Direction,
	const xmqttpropertyconfig* Config, xmqttpropertyconfig* Limits,
	size_t* Size, uint32* BodySize);
XMQTT_INTERNAL bool xmqttInternalPropertiesOutput(const void* Output, size_t Size,
	const xmqttproperty* Properties, size_t Count, const xmqttpropertyconfig* Config);
XMQTT_INTERNAL size_t xmqttInternalPropertiesEmit(ptr Output,
	const xmqttproperty* Properties, size_t Count, uint32 Body, size_t Size);
#endif

#if defined(XMQTT_FEATURE_XMQTT_PACKET)
#include <xmqtt/packet.h>
XMQTT_INTERNAL bool xmqttInternalCodecSettings(xmqttdirection Direction,
	const xmqttcodecconfig* Config, xmqttcodecconfig* Copy);
XMQTT_INTERNAL bool xmqttInternalRole(xmqttpackettype Type, xmqttdirection Direction);
XMQTT_INTERNAL bool xmqttInternalReason(xmqttversion Version, xmqttpackettype Type,
	uint8 Reason, xmqttdirection Direction);
/* Already validated MQTT UTF-8 strings. Shared prefix is interpreted only when
 * Shared=true. Wildcards/levels are checked without Unicode normalization. */
XMQTT_INTERNAL bool xmqttInternalTopic(xstrview Text, bool Filter, bool Shared,
	bool* IsShared);
#endif
#endif
