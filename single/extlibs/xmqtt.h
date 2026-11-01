/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xmqtt requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XMQTT_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XMQTT_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XMQTT_SINGLE_HEADER_H
#define XMQTT_SINGLE_HEADER_H
#define XMQTT_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xmqtt/include/xmqtt/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XMQTT_FEATURES_H
#define XMQTT_FEATURES_H

/* xmqtt 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT)
#ifndef XMQTT_FEATURE_XMQTT
#define XMQTT_FEATURE_XMQTT
#endif
#ifndef XMQTT_MODULE_XMQTT_WIRE
#define XMQTT_MODULE_XMQTT_WIRE
#endif
#ifndef XMQTT_MODULE_XMQTT_PROPERTIES
#define XMQTT_MODULE_XMQTT_PROPERTIES
#endif
#ifndef XMQTT_MODULE_XMQTT_CODEC5
#define XMQTT_MODULE_XMQTT_CODEC5
#endif
#ifndef XMQTT_MODULE_XMQTT_CODEC311
#define XMQTT_MODULE_XMQTT_CODEC311
#endif
#ifndef XMQTT_MODULE_XMQTT_NEGOTIATION
#define XMQTT_MODULE_XMQTT_NEGOTIATION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT
#define XMQTT_MODULE_XMQTT_CLIENT
#endif
#endif

/* xmqtt_client 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT
#define XMQTT_FEATURE_XMQTT_CLIENT
#endif
#ifndef XMQTT_MODULE_XMQTT_NEGOTIATION
#define XMQTT_MODULE_XMQTT_NEGOTIATION
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* xmqtt_negotiation 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_NEGOTIATION)
#ifndef XMQTT_FEATURE_XMQTT_NEGOTIATION
#define XMQTT_FEATURE_XMQTT_NEGOTIATION
#endif
#ifndef XMQTT_MODULE_XMQTT_CODEC5
#define XMQTT_MODULE_XMQTT_CODEC5
#endif
#ifndef XMQTT_MODULE_XMQTT_CODEC311
#define XMQTT_MODULE_XMQTT_CODEC311
#endif
#endif

/* xmqtt_codec311 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CODEC311)
#ifndef XMQTT_FEATURE_XMQTT_CODEC311
#define XMQTT_FEATURE_XMQTT_CODEC311
#endif
#ifndef XMQTT_MODULE_XMQTT_PACKET
#define XMQTT_MODULE_XMQTT_PACKET
#endif
#endif

/* xmqtt_codec5 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CODEC5)
#ifndef XMQTT_FEATURE_XMQTT_CODEC5
#define XMQTT_FEATURE_XMQTT_CODEC5
#endif
#ifndef XMQTT_MODULE_XMQTT_PACKET
#define XMQTT_MODULE_XMQTT_PACKET
#endif
#ifndef XMQTT_MODULE_XMQTT_PROPERTIES
#define XMQTT_MODULE_XMQTT_PROPERTIES
#endif
#endif

/* xmqtt_packet 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_PACKET)
#ifndef XMQTT_FEATURE_XMQTT_PACKET
#define XMQTT_FEATURE_XMQTT_PACKET
#endif
#ifndef XMQTT_MODULE_XMQTT_WIRE
#define XMQTT_MODULE_XMQTT_WIRE
#endif
#endif

/* xmqtt_properties 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_PROPERTIES)
#ifndef XMQTT_FEATURE_XMQTT_PROPERTIES
#define XMQTT_FEATURE_XMQTT_PROPERTIES
#endif
#ifndef XMQTT_MODULE_XMQTT_WIRE
#define XMQTT_MODULE_XMQTT_WIRE
#endif
#endif

/* xmqtt_wire 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_WIRE)
#ifndef XMQTT_FEATURE_XMQTT_WIRE
#define XMQTT_FEATURE_XMQTT_WIRE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

#endif /* XMQTT_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/export.h */
/* ========================================================================== */

#ifndef XMQTT_EXPORT_H
#define XMQTT_EXPORT_H
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XMQTT_BUILD_SHARED)
  #define XMQTT_API __declspec(dllexport)
 #elif defined(XMQTT_USE_SHARED)
  #define XMQTT_API __declspec(dllimport)
 #else
  #define XMQTT_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XMQTT_BUILD_SHARED)
  #define XMQTT_API __attribute__((visibility("default")))
 #else
  #define XMQTT_API
 #endif
#else
 #define XMQTT_API
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/wire.h */
/* ========================================================================== */

#ifndef XMQTT_WIRE_H
#define XMQTT_WIRE_H

#if defined(XMQTT_FEATURE_XMQTT_WIRE) && !defined(XRT_FEATURE_UNICODE)
#error "XMQTT_FEATURE_XMQTT_WIRE requires XRT_FEATURE_UNICODE"
#endif

#if defined(XMQTT_FEATURE_XMQTT_WIRE)

#define XMQTT_VARIABLE_MAX UINT32_C(268435455)
#define XMQTT_PACKET_MAX_BYTES UINT32_C(268435460)
#define XMQTT_STRING_MAX_BYTES UINT16_MAX
#define XMQTT_UTF8_REJECT_DISALLOWED 0x01u
#define XMQTT_PUBLISH_RETAIN 0x01u
#define XMQTT_PUBLISH_QOS_MASK 0x06u
#define XMQTT_PUBLISH_DUP 0x08u

/* Values equal the protocol-level byte, not the spelling of a version. */
typedef enum xmqttversion { XMQTT_VERSION_311 = 4, XMQTT_VERSION_5 = 5 } xmqttversion;

typedef enum xmqttdirection { XMQTT_CLIENT_TO_SERVER = 1, XMQTT_SERVER_TO_CLIENT } xmqttdirection;

typedef enum xmqttpackettype {
	XMQTT_PACKET_CONNECT = 1, XMQTT_PACKET_CONNACK, XMQTT_PACKET_PUBLISH,
	XMQTT_PACKET_PUBACK, XMQTT_PACKET_PUBREC, XMQTT_PACKET_PUBREL,
	XMQTT_PACKET_PUBCOMP, XMQTT_PACKET_SUBSCRIBE, XMQTT_PACKET_SUBACK,
	XMQTT_PACKET_UNSUBSCRIBE, XMQTT_PACKET_UNSUBACK, XMQTT_PACKET_PINGREQ,
	XMQTT_PACKET_PINGRESP, XMQTT_PACKET_DISCONNECT, XMQTT_PACKET_AUTH
} xmqttpackettype;

typedef enum xmqttwirestatus {
	XMQTT_WIRE_ERROR = -1, XMQTT_WIRE_MORE = 0, XMQTT_WIRE_DONE = 1
} xmqttwirestatus;

typedef enum xmqttwireerror {
	XMQTT_WIRE_ARGUMENT = 1001, XMQTT_WIRE_MALFORMED, XMQTT_WIRE_LIMIT,
	XMQTT_WIRE_CAPACITY, XMQTT_WIRE_UTF8
} xmqttwireerror;

typedef struct xmqttwireconfig {
	size_t Size;
	xmqttversion Version;
	/* Includes the first byte and actual encoded Remaining Length bytes.
	 * Range 2..XMQTT_PACKET_MAX_BYTES. Not a receive-buffer allocation. */
	size_t MaxPacketBytes;
} xmqttwireconfig;

typedef struct xmqttfixedheader {
	xmqttpackettype Type;
	uint8 Flags;
	uint32 RemainingLength;
	size_t HeaderBytes, PacketBytes;
} xmqttfixedheader;

/* Wire and Body borrow Input; Body includes variable header plus payload.
 * DONE means complete framing only, not a valid packet body/session action. */
typedef struct xmqttframe {
	xmqttfixedheader Header;
	xbytesview Wire, Body;
} xmqttframe;

XRT_EXTERN_C_BEGIN

/* Defaults: MQTT 5.0, total packet cap 1 MiB. Config is copied in each call;
 * NULL uses defaults. Caller-thread, stateless; no input/callback retained.
 * All successful and MORE paths allocate nothing and preserve ambient error.
 * Failure diagnostics can allocate/fall back to XERR_MEMORY. All read outputs
 * stay unchanged on MORE/ERROR. Writes and Written stay unchanged on failure.
 * Actual readable objects are required; range checks only detect arithmetic
 * wrap/null misuse. Output objects must not overlap Input/config/each other.
 * Write capacity describes a nonwrapping buffer; only wire bytes are modified.
 * No output NUL. Written is optional for writes; Consumed is required for reads.
 * These primitives do not check packet bodies, directions, ids or session order. */
XMQTT_API bool xmqttWireConfigInit(xmqttwireconfig* Config);

/* Read one 1..4-byte integer, ignoring any following bytes. MQTT 5.0 enforces
 * shortest encoding; 3.1.1 compatibility accepts nonminimal encodings of up to
 * four bytes. A fourth continuation bit is immediately malformed, not MORE. */
XMQTT_API xmqttwirestatus xmqttVbiRead(xbytesview Input, xmqttversion Version,
	uint32* Value, size_t* Consumed);
/* Always emits the shortest encoding, 1..4 bytes, in either protocol. */
XMQTT_API bool xmqttVbiWrite(ptr Output, size_t Capacity, uint32 Value, size_t* Written);

/* HeaderRead needs only the fixed header. Validate type/version, reserved
 * flags, PUBLISH QoS!=3, DUP=0 for QoS0, integer form and TOTAL packet cap
 * before body buffering. At most five input bytes are examined. AUTH is
 * reserved/rejected in 3.1.1. MORE retains no prefix: supply the full prefix
 * again on the next call. This permits bounded caller-owned receive storage. */
XMQTT_API xmqttwirestatus xmqttHeaderRead(xbytesview Input,
	const xmqttwireconfig* Config, xmqttfixedheader* Header);
/* Writes only the fixed header (2..5 bytes), applying the same checks/cap.
 * A packet codec must validate/build the actual variable header/payload. */
XMQTT_API bool xmqttHeaderWrite(ptr Output, size_t Capacity, xmqttpackettype Type,
	uint8 Flags, uint32 RemainingLength, const xmqttwireconfig* Config, size_t* Written);
/* One complete packet; trailing coalesced data is left after Header.PacketBytes.
 * Header/size failures are detected without inspecting/copying the body. */
XMQTT_API xmqttwirestatus xmqttFrameRead(xbytesview Input,
	const xmqttwireconfig* Config, xmqttframe* Frame);

/* MQTT length-prefixed string data (<=65535 bytes), excluding its two-byte
 * prefix. Always require well-formed Unicode scalar UTF-8 and reject U+0000.
 * REJECT_DISALLOWED additionally rejects controls U+0001..001F/007F..009F and
 * Unicode noncharacters. Flags=0 accepts these optional code points. BOM is
 * never stripped; empty string is valid here (field rules belong to codecs).
 * ErrorOffset is optional, XRT_NPOS on success, offending byte on text/length
 * failure (65535 for excessive length); unchanged on invalid API use. */
XMQTT_API bool xmqttUtf8StringCheck(xstrview Text, uint32 Flags, size_t* ErrorOffset);
XMQTT_API xmqttwirestatus xmqttUtf8StringRead(xbytesview Input, uint32 Flags,
	xstrview* Value, size_t* Consumed);
XMQTT_API bool xmqttUtf8StringWrite(ptr Output, size_t Capacity, xstrview Text,
	uint32 Flags, size_t* Written);
/* Binary has the same uint16 big-endian length prefix, with arbitrary bytes
 * including NUL; size<=65535. Reads borrow without copying; writes copy once.
 * Field writes and Written cannot overlap any source bytes. */
XMQTT_API xmqttwirestatus xmqttBinaryRead(xbytesview Input,
	xbytesview* Value, size_t* Consumed);
XMQTT_API bool xmqttBinaryWrite(ptr Output, size_t Capacity,
	xbytesview Data, size_t* Written);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/properties.h */
/* ========================================================================== */

#ifndef XMQTT_PROPERTIES_H
#define XMQTT_PROPERTIES_H

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES) && !defined(XMQTT_FEATURE_XMQTT_WIRE)
#error "XMQTT_FEATURE_XMQTT_PROPERTIES requires XMQTT_FEATURE_XMQTT_WIRE"
#endif

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)

#define XMQTT_PROPERTY_SECTION_MAX (XMQTT_VARIABLE_MAX + 4u)

typedef enum xmqttpropertyid {
	XMQTT_PROPERTY_PAYLOAD_FORMAT = 1, XMQTT_PROPERTY_MESSAGE_EXPIRY = 2,
	XMQTT_PROPERTY_CONTENT_TYPE = 3, XMQTT_PROPERTY_RESPONSE_TOPIC = 8,
	XMQTT_PROPERTY_CORRELATION_DATA = 9, XMQTT_PROPERTY_SUBSCRIPTION_ID = 11,
	XMQTT_PROPERTY_SESSION_EXPIRY = 17, XMQTT_PROPERTY_ASSIGNED_CLIENT_ID = 18,
	XMQTT_PROPERTY_SERVER_KEEP_ALIVE = 19, XMQTT_PROPERTY_AUTH_METHOD = 21,
	XMQTT_PROPERTY_AUTH_DATA = 22, XMQTT_PROPERTY_REQUEST_PROBLEM_INFO = 23,
	XMQTT_PROPERTY_WILL_DELAY = 24, XMQTT_PROPERTY_REQUEST_RESPONSE_INFO = 25,
	XMQTT_PROPERTY_RESPONSE_INFO = 26, XMQTT_PROPERTY_SERVER_REFERENCE = 28,
	XMQTT_PROPERTY_REASON_STRING = 31, XMQTT_PROPERTY_RECEIVE_MAXIMUM = 33,
	XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM = 34, XMQTT_PROPERTY_TOPIC_ALIAS = 35,
	XMQTT_PROPERTY_MAXIMUM_QOS = 36, XMQTT_PROPERTY_RETAIN_AVAILABLE = 37,
	XMQTT_PROPERTY_USER = 38, XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE = 39,
	XMQTT_PROPERTY_WILDCARD_AVAILABLE = 40, XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE = 41,
	XMQTT_PROPERTY_SHARED_AVAILABLE = 42
} xmqttpropertyid;

typedef enum xmqttpropertytype {
	XMQTT_PROPERTY_BYTE = 1, XMQTT_PROPERTY_UINT16, XMQTT_PROPERTY_UINT32,
	XMQTT_PROPERTY_VARIABLE, XMQTT_PROPERTY_STRING, XMQTT_PROPERTY_BINARY,
	XMQTT_PROPERTY_STRING_PAIR
} xmqttpropertytype;

typedef struct xmqttstringpair { xstrview Name, Value; } xmqttstringpair;
typedef union xmqttpropertyvalue {
	uint32 Number;
	xstrview Text;
	xbytesview Binary;
	xmqttstringpair Pair;
} xmqttpropertyvalue;

/* Type must match the standard type of Id; only its active union member is
 * meaningful. Number represents BYTE/UINT16/UINT32/VARIABLE without narrowing.
 * Text/binary/pair views borrow source bytes, excluding their length prefix. */
typedef struct xmqttproperty {
	xmqttpropertyid Id;
	xmqttpropertytype Type;
	xmqttpropertyvalue Value;
} xmqttproperty;

typedef enum xmqttpropertyscope {
	XMQTT_PROPERTIES_CONNECT = 1, XMQTT_PROPERTIES_CONNACK, XMQTT_PROPERTIES_PUBLISH,
	XMQTT_PROPERTIES_PUBACK, XMQTT_PROPERTIES_PUBREC, XMQTT_PROPERTIES_PUBREL,
	XMQTT_PROPERTIES_PUBCOMP, XMQTT_PROPERTIES_SUBSCRIBE, XMQTT_PROPERTIES_SUBACK,
	XMQTT_PROPERTIES_UNSUBSCRIBE, XMQTT_PROPERTIES_UNSUBACK,
	XMQTT_PROPERTIES_DISCONNECT = 14, XMQTT_PROPERTIES_AUTH, XMQTT_PROPERTIES_WILL
} xmqttpropertyscope;

typedef struct xmqttpropertyconfig {
	size_t Size;
	/* Includes the Property Length integer itself; positive and <=SECTION_MAX.
	 * MaxProperties may be zero to allow only empty sections. */
	size_t MaxBytes, MaxProperties;
	uint32 Utf8Flags;
} xmqttpropertyconfig;

typedef struct xmqttpropertydocument {
	size_t Count, Consumed;
	/* Exact complete section including its length, borrowed from Input. */
	xbytesview Raw;
} xmqttpropertydocument;

/* Status does not depend on diagnostic allocation success. In particular,
 * diagnostic OOM cannot turn confirmed malformed/protocol wire into local
 * overload, or local scratch/admission failure into a peer protocol error. */
typedef enum xmqttpropertystatus {
	XMQTT_PROPERTIES_LOCAL_ERROR = -3, XMQTT_PROPERTIES_PROTOCOL_ERROR = -2,
	XMQTT_PROPERTIES_MALFORMED = -1, XMQTT_PROPERTIES_MORE = 0, XMQTT_PROPERTIES_DONE = 1
} xmqttpropertystatus;

typedef enum xmqttpropertyerror {
	XMQTT_PROPERTY_ARGUMENT = 2001, XMQTT_PROPERTY_LIMIT, XMQTT_PROPERTY_CAPACITY,
	XMQTT_PROPERTY_MALFORMED, XMQTT_PROPERTY_PROTOCOL
} xmqttpropertyerror;

XRT_EXTERN_C_BEGIN

/* Defaults: complete section 1 MiB, 256 properties, mandatory UTF-8 rules.
 * Config is copied; NULL uses defaults. Pure caller-thread synchronous calls,
 * with no retained inputs/callbacks or successful/MORE heap allocations. */
XMQTT_API bool xmqttPropertiesConfigInit(xmqttpropertyconfig* Config);

/* MQTT 5.0 only: one explicit Property Length plus its properties. Does not
 * invent omitted sections for abbreviated packets, or apply session defaults.
 * Packet/WILL scope and sender direction are explicit and checked. Covers all
 * 27 standard ids/types, allowed scopes, singleton/repeated rules, numeric
 * constraints, response-topic syntax and CONNECT authentication dependency.
 * Explicit AUTH sections require Authentication Method; packet-level omitted
 * forms and negotiated authentication state belong to the packet/runtime layer.
 * Count/Consumed/Raw publish only on DONE; Document is unchanged otherwise.
 * Properties is caller scratch, unspecified on errors. Do not use it until DONE.
 * MORE changes no scratch and retains no prefix: resupply the complete prefix.
 * Capacity may be below the configured limit; NULL/0 scratch allows empty only.
 * Views borrow Input and stay valid only while its bytes stay alive/unchanged.
 * Stop at one section; trailing payload/coalesced bytes remain unconsumed.
 * Outputs must not overlap each other, Input or Config. Arithmetic range checks
 * require actual objects. Diagnostics may allocate/fall back to XERR_MEMORY;
 * underlying wire diagnostics are retained where appropriate. DONE/MORE preserve
 * ambient error. Unknown ids/types/scopes are malformed, not silently skipped. */
XMQTT_API xmqttpropertystatus xmqttPropertiesRead(xbytesview Input,
	xmqttpropertyscope Scope, xmqttdirection Direction, const xmqttpropertyconfig* Config,
	xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document);

/* Ordered input vector, including repeated User Properties and repeated
 * server PUBLISH Subscription Identifiers. Other duplicates are rejected.
 * NULL/0 means empty. Required/unused union members are determined by Type.
 * Preflight all descriptors/values/limits; Required unchanged on every failure.
 * Required cannot overlap the vector, active views or Config. */
XMQTT_API bool xmqttPropertiesSize(const xmqttproperty* Properties, size_t Count,
	xmqttpropertyscope Scope, xmqttdirection Direction, const xmqttpropertyconfig* Config,
	size_t* Required);
/* Independent preflight, shortest integer forms, original property order and
 * exact UTF-8/binary bytes, no terminating NUL. No success heap allocation.
 * All failures preserve output and optional Written. Wire/Written cannot overlap
 * each other, vector, active views or Config. Input bytes/config/vector must stay
 * alive/unchanged throughout each call. Text is checked once per preflight;
 * emission does not revalidate UTF-8. Data is copied once during emission. */
XMQTT_API bool xmqttPropertiesWrite(ptr Output, size_t Capacity,
	const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Written);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/packet.h */
/* ========================================================================== */

#ifndef XMQTT_PACKET_H
#define XMQTT_PACKET_H

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
	XMQTT_CODEC_CLIENT_ID
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


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/codec5.h */
/* ========================================================================== */

#ifndef XMQTT_CODEC5_H
#define XMQTT_CODEC5_H

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


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/codec311.h */
/* ========================================================================== */

#ifndef XMQTT_CODEC311_H
#define XMQTT_CODEC311_H

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


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/negotiation.h */
/* ========================================================================== */

#ifndef XMQTT_NEGOTIATION_H
#define XMQTT_NEGOTIATION_H
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


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_H
#define XMQTT_CLIENT_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT) && (!defined(XMQTT_FEATURE_XMQTT_NEGOTIATION) || !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_FUTURE))
#error "MQTT client requires negotiation, TCP and Future"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
typedef struct xmqttclient xmqttclient;
typedef struct xmqttmessage xmqttmessage;
typedef enum xmqttclientstate {XMQTT_CLIENT_CONNECTING=0,XMQTT_CLIENT_OPEN,XMQTT_CLIENT_CLOSING,XMQTT_CLIENT_CLOSED} xmqttclientstate;
typedef enum xmqttclientstatus {
    XMQTT_STATUS_OK=0,XMQTT_STATUS_REJECTED,XMQTT_STATUS_TIMEOUT,XMQTT_STATUS_CANCELLED,
    XMQTT_STATUS_CLOSED,XMQTT_STATUS_TRANSPORT,XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_LIMIT,
    XMQTT_STATUS_MEMORY,XMQTT_STATUS_UNSUPPORTED,XMQTT_STATUS_SESSION,
    XMQTT_STATUS_INITIALIZATION,XMQTT_STATUS_SERVER_DISCONNECT,XMQTT_STATUS_RESTRICTED
} xmqttclientstatus;
typedef enum xmqttdelivery {XMQTT_NOT_SENT=0,XMQTT_MAY_HAVE_REACHED_PEER,XMQTT_PEER_ACCEPTED,XMQTT_PEER_REJECTED} xmqttdelivery;
typedef enum xmqttclientadmission {XMQTT_ADMISSION_ERROR=-1,XMQTT_ADMISSION_ACCEPTED=0,XMQTT_ADMISSION_OVERLOADED,XMQTT_ADMISSION_CLOSED} xmqttclientadmission;
/* A resolved Future is a typed outcome, not necessarily operation success.
 * QoS-0 OK observes complete native write-buffer retirement while open; it
 * does not acknowledge broker receipt, routing, storage or subscriber delivery.
 * Accepting ANY prefix into TCP makes delivery uncertain. Never auto-replayed.
 * QoS-1 completes after a corresponding PUBACK; PEER_ACCEPTED/REJECTED denotes
 * its decision, not subscriber delivery or durable storage. Puback (if present)
 * owns its reason/properties and may be separately retained. Local metadata
 * exhaustion can return LIMIT/MEMORY after an ACK, retaining its scalar decision.
 * Cause is borrowed from the result; retained results do not retain Client.
 * The original four-field prefix is unchanged; new fields are appended. */
typedef struct xmqttwriteresult {
    xmqttclientstatus Status;xmqttdelivery Delivery;size_t AcceptedBytes;const xerror* Cause;
    uint16 Identifier;uint8 Reason;const xmqttmessage* Puback;
} xmqttwriteresult;
/* Connack and Negotiated views belong to this Future. The optional message
 * may be separately retained. No connection/engine reference survives in it. */
typedef struct xmqttreadyresult {
    xmqttclientstatus Status;uint8 Reason;xmqttnegotiated Negotiated;
    const xmqttmessage* Connack;const xerror* Cause;
} xmqttreadyresult;
/* Nonblocking IO-worker notification. May take messages, submit or Close, but
 * must not block or Wait/Destroy. Code/User stay alive through Destroy. */
typedef void (*xmqttclientnotify)(xmqttclient* Client,ptr User);
typedef struct xmqttclientconfig {
    size_t Size;xmqttversion Version;xnetstreamconfig Stream;xmqttcodecconfig Codec;
    size_t MaxRequests,MaxOutgoingBytes,MaxObjects,MaxObjectBytes,MaxMessages,MaxMessageBytes;
    size_t MaxPrivateBytes,MaxSetupBytes,InitialInputBytes,ReadBytes,WriteBytes,WriteChunkBytes,WorkPackets;
    int64 SetupTimeout,PingTimeout;
    xmqttclientnotify Notify;ptr User;
} xmqttclientconfig;
typedef struct xmqttclientinfo {
    size_t Size;xmqttclientstate State;xmqttclientstatus CloseStatus;uint8 ServerReason;
    bool Ready;size_t Requests,OutgoingBytes,Messages,MessageBytes,Objects,ObjectBytes,PrivateBytes,StartupBytes;
    uint64 Accepted,Rejected,Completed,Received,ProtocolErrors,Pings,Pongs;
} xmqttclientinfo;
XRT_EXTERN_C_BEGIN
/* MQTT 5, finite codec defaults; 128 requests/4 MiB outgoing; 512 objects/16 MiB,
 * 64 queued messages/8 MiB; 4 MiB private, 64 KiB setup, 1 KiB initial input;
 * 64 KiB read/write work, 16 KiB chunks, 64 packets/turn; 5s setup/ping timeout. */
XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* Config);
/* One numeric TCP connection on a running host Engine, no TLS/DNS/fallback or
 * implicit replay. CONNECT is fully encoded/copied before return; callbacks may
 * run before return. Setup timeout starts at factory entry. Caller owns one
 * unique Client; Close/Wait/Destroy it before destroying Engine. Output changes
 * only on success. Startup wire is wiped after handshake/stops AND its final
 * native reference. Caller/native/system copies are outside that contract.
 * This vertical slice supports basic CONNECT auth, CONNACK, keep-alive, QoS-0
 * receives, QoS-0/1 sends and server DISCONNECT. Enhanced auth and positive receive
 * alias capacity are explicitly unsupported; QoS-1 receive, QoS 2, subscriptions, reconnect,
 * durable sessions and graceful DISCONNECT remain separate pending work.
 * No state is restored: MQTT 5 SessionPresent without local state closes.
 * Explicit Maximum Packet Size must not exceed the local codec packet cap. */
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Client);
/* Shared readiness Future, always typed. Explicit Future cancellation requests
 * Close; raw cancel-token requests are sampled on IO/deadline progress. */
XMQTT_API xfuture* xmqttClientReady(xmqttclient* Client);
/* Thread-safe finite admission, immutable preencoded copy. QoS-0/1 new PUBLISH
 * (DUP=0), with a nonempty topic and no topic alias. QoS-1 Identifier=0 assigns
 * an unused id; nonzero reserves the explicit id, with normal OVERLOADED on
 * conflict. Its physical reservation lasts until real request retirement.
 * A bounded bitmap/hash table is allocated lazily on the first QoS-1 attempt,
 * charged to MaxPrivateBytes and retained through Destroy, even on later
 * admission failure. Peer Receive Maximum limits QoS-1 sends; QoS-0/control
 * traffic may continue while it is exhausted. May queue while connecting, but no
 * publish is sent before accepted CONNACK. Timeout includes encoding/queueing.
 * Future output changes on ACCEPTED only; normal overload/closed is counted.
 * Explicit Future cancellation wakes the owner; raw token sampled on progress.
 * Cancelling/timing out an already accepted prefix retires the connection. */
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Same operation/deadline; rejects ANY worker of the same Engine. */
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Transfer one queued message reference, false/unchanged on empty. Overfull
 * receive/object budgets close rather than silently dropping a packet. Already
 * queued messages remain available after Close; Destroy releases untaken ones.
 * Server DISCONNECT's reason is in Info, not added to the PUBLISH queue. */
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* Client,xmqttmessage** Message);
XMQTT_API const xmqttpacket* xmqttMessagePacket(const xmqttmessage* Message);
XMQTT_API bool xmqttMessageRetain(const xmqttmessage* Message);
XMQTT_API void xmqttMessageRelease(const xmqttmessage* Message);
XMQTT_API bool xmqttClientInfo(xmqttclient* Client,xmqttclientinfo* Info);
/* Abortive/nonblocking; sends no MQTT DISCONNECT, so a broker may publish Will.
 * CLOSED is logical; Wait additionally observes real producer/post/timer/send/
 * event-owner and native stream reference retirement. Timeout preserves owner.
 * Stop concurrent API entries before Destroy. Wait/Destroy reject Engine workers. */
XMQTT_API bool xmqttClientClose(xmqttclient* Client);
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* Client,int64 Timeout);
XMQTT_API bool xmqttClientDestroy(xmqttclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt.h */
/* ========================================================================== */

#ifndef XMQTT_H
#define XMQTT_H
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_WIRE)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_WIRE"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_PROPERTIES"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CODEC5)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CODEC5"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CODEC311)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CODEC311"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_NEGOTIATION"
#endif
#if defined(XMQTT_FEATURE_XMQTT) && !defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "XMQTT_FEATURE_XMQTT requires XMQTT_FEATURE_XMQTT_CLIENT"
#endif
#endif

#endif

#if defined(XMQTT_IMPLEMENTATION) && !defined(XMQTT_IMPLEMENTATION_ONCE)
#define XMQTT_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xmqtt/src/internal/xmqtt_internal.h */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_WIRE) || \
	defined(XMQTT_FEATURE_XMQTT_PROPERTIES) || \
	defined(XMQTT_FEATURE_XMQTT_PACKET) || \
	defined(XMQTT_FEATURE_XMQTT_CODEC5) || \
	defined(XMQTT_FEATURE_XMQTT_CODEC311)
#ifndef XMQTT_INTERNAL_H
#define XMQTT_INTERNAL_H

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
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/wire/wire.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_WIRE)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_WIRE)

static bool xmqttWireFail(xerrkind Kind, xmqttwireerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.mqtt", (int32)Code, Message); return false;
}

static bool xmqttWireSpan(xbytesview Input)
{
	if ((Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT input span");
	return true;
}

static bool xmqttWireVersion(xmqttversion Version)
{
	return Version == XMQTT_VERSION_311 || Version == XMQTT_VERSION_5;
}

static bool xmqttWireReadOutput(ptr Output, size_t Size, xbytesview Input)
{
	if (Output == NULL || !xrtMemRangeValid(Output, Size) || xrtMemRangesOverlap(Output, Size, Input.Data, Input.Size))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT read output span or input overlap");
	return true;
}

static bool xmqttWireReadPair(xbytesview Input, ptr Value, size_t Size, size_t* Consumed)
{
	if (!xmqttWireSpan(Input) || !xmqttWireReadOutput(Value, Size, Input) || !xmqttWireReadOutput(Consumed, sizeof(*Consumed), Input)) return false;
	if (xrtMemRangesOverlap(Value, Size, Consumed, sizeof(*Consumed)))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "MQTT read outputs overlap");
	return true;
}

static bool xmqttWireConfig(const xmqttwireconfig* Config, xmqttwireconfig* Copy)
{
	if (Config != NULL && !xrtMemRangeValid(Config, sizeof(*Config)))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT config span");
	if (Config == NULL) xmqttWireConfigInit(Copy); else *Copy = *Config;
	if (Copy->Size != sizeof(*Copy) || !xmqttWireVersion(Copy->Version) || Copy->MaxPacketBytes < 2u || Copy->MaxPacketBytes > XMQTT_PACKET_MAX_BYTES)
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT wire configuration");
	return true;
}

XMQTT_API bool xmqttWireConfigInit(xmqttwireconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT config output");
	*Config = (xmqttwireconfig){sizeof(*Config), XMQTT_VERSION_5, 1024u*1024u}; return true;
}

static xmqttwirestatus xmqttWireVbi(xbytesview Input, xmqttversion Version, uint32* Value, size_t* Consumed)
{
	uint32 Number = 0;
	for (unsigned Index = 0; Index < 4u; ++Index) {
		if (Index == Input.Size) return XMQTT_WIRE_MORE;
		unsigned Byte = Input.Data[Index]; Number |= (uint32)(Byte & 127u) << (7u * Index);
		if (!(Byte & 128u)) {
			if (Version == XMQTT_VERSION_5 && Index != 0 && Byte == 0) {
				xmqttWireFail(XERR_PROTOCOL, XMQTT_WIRE_MALFORMED, "MQTT 5 variable integer is not minimally encoded"); return XMQTT_WIRE_ERROR;
			}
			*Value = Number; *Consumed = Index+1u; return XMQTT_WIRE_DONE;
		}
	}
	xmqttWireFail(XERR_PROTOCOL, XMQTT_WIRE_MALFORMED, "MQTT variable integer exceeds four bytes"); return XMQTT_WIRE_ERROR;
}

XMQTT_API xmqttwirestatus xmqttVbiRead(xbytesview Input, xmqttversion Version, uint32* Value, size_t* Consumed)
{
	if (!xmqttWireReadPair(Input, Value, sizeof(*Value), Consumed)) return XMQTT_WIRE_ERROR;
	if (!xmqttWireVersion(Version)) {
		xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT version"); return XMQTT_WIRE_ERROR;
	}
	return xmqttWireVbi(Input, Version, Value, Consumed);
}

static size_t xmqttWirePutVbi(unsigned char* Output, uint32 Value)
{
	size_t Bytes = 0;
	do {
		unsigned char Byte = (unsigned char)(Value & 127u); Value >>= 7;
		Output[Bytes++] = (unsigned char)(Byte | (Value ? 128u : 0u));
	} while (Value);
	return Bytes;
}

static bool xmqttWireWriteOutput(ptr Output, size_t Capacity, size_t Bytes, xbytesview Input,
	const xmqttwireconfig* Config, size_t* Written)
{
	if (Output == NULL || !xrtMemRangeValid(Output, Capacity) || xrtMemRangesOverlap(Output, Bytes, Input.Data, Input.Size) ||
		(Config != NULL && xrtMemRangesOverlap(Output, Bytes, Config, sizeof(*Config))))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid MQTT write output or source overlap");
	if (Written != NULL && (!xmqttWireReadOutput(Written, sizeof(*Written), Input) ||
		xrtMemRangesOverlap(Written, sizeof(*Written), Output, Bytes) ||
		(Config != NULL && xrtMemRangesOverlap(Written, sizeof(*Written), Config, sizeof(*Config)))))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "MQTT written-size output overlaps wire/config/input");
	if (Capacity < Bytes) return xmqttWireFail(XERR_RANGE, XMQTT_WIRE_CAPACITY, "MQTT output capacity is too small");
	return true;
}

XMQTT_API bool xmqttVbiWrite(ptr Output, size_t Capacity, uint32 Value, size_t* Written)
{
	if (Value > XMQTT_VARIABLE_MAX) return xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "MQTT variable integer exceeds protocol maximum");
	unsigned char Encoded[4]; size_t Bytes = xmqttWirePutVbi(Encoded, Value);
	if (!xmqttWireWriteOutput(Output, Capacity, Bytes, (xbytesview){0}, NULL, Written)) return false;
	memcpy(Output, Encoded, Bytes); if (Written != NULL) *Written = Bytes; return true;
}

XMQTT_INTERNAL bool xmqttInternalFlags(xmqttpackettype Type, uint8 Flags, xmqttversion Version)
{
	if (Type < XMQTT_PACKET_CONNECT || Type > XMQTT_PACKET_AUTH || (Type == XMQTT_PACKET_AUTH && Version == XMQTT_VERSION_311) || Flags > 15u) return false;
	if (Type == XMQTT_PACKET_PUBLISH) {
		unsigned Qos = (Flags & XMQTT_PUBLISH_QOS_MASK) >> 1;
		return Qos != 3u && (Qos != 0 || !(Flags & XMQTT_PUBLISH_DUP));
	}
	return Flags == (Type == XMQTT_PACKET_PUBREL || Type == XMQTT_PACKET_SUBSCRIBE || Type == XMQTT_PACKET_UNSUBSCRIBE ? 2u : 0u);
}

static bool xmqttWireHeaderArgs(xbytesview Input, const xmqttwireconfig* Config,
	ptr Output, size_t Size, xmqttwireconfig* Copy)
{
	if (!xmqttWireSpan(Input) || !xmqttWireReadOutput(Output, Size, Input) || !xmqttWireConfig(Config, Copy)) return false;
	if (Config != NULL && xrtMemRangesOverlap(Output, Size, Config, sizeof(*Config)))
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "MQTT header/frame output overlaps config");
	return true;
}

XMQTT_INTERNAL xmqttwirestatus xmqttInternalHeader(xbytesview Input, const xmqttwireconfig* Config, xmqttfixedheader* Header, xmqttwireerror* Failure)
{
	if (Input.Size == 0) return XMQTT_WIRE_MORE;
	xmqttpackettype Type = (xmqttpackettype)(Input.Data[0] >> 4); uint8 Flags = Input.Data[0] & 15u;
	if (!xmqttInternalFlags(Type, Flags, Config->Version)) {
		if (Failure != NULL) *Failure = XMQTT_WIRE_MALFORMED;
		xmqttWireFail(XERR_PROTOCOL, XMQTT_WIRE_MALFORMED, "invalid MQTT packet type or fixed-header flags"); return XMQTT_WIRE_ERROR;
	}
	uint32 Remaining; size_t Bytes;
	xmqttwirestatus Status = xmqttWireVbi((xbytesview){Input.Data+1u, Input.Size-1u}, Config->Version, &Remaining, &Bytes);
	if (Status != XMQTT_WIRE_DONE) {
		if (Status == XMQTT_WIRE_ERROR && Failure != NULL) *Failure = XMQTT_WIRE_MALFORMED;
		return Status;
	}
	size_t PacketBytes = 1u + Bytes + Remaining;
	if (PacketBytes > Config->MaxPacketBytes) {
		if (Failure != NULL) *Failure = XMQTT_WIRE_LIMIT;
		xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "MQTT total packet size exceeds configured limit"); return XMQTT_WIRE_ERROR;
	}
	*Header = (xmqttfixedheader){Type, Flags, Remaining, Bytes+1u, PacketBytes}; return XMQTT_WIRE_DONE;
}

XMQTT_API xmqttwirestatus xmqttHeaderRead(xbytesview Input, const xmqttwireconfig* Config, xmqttfixedheader* Header)
{
	xmqttwireconfig Copy;
	if (!xmqttWireHeaderArgs(Input, Config, Header, sizeof(*Header), &Copy)) return XMQTT_WIRE_ERROR;
	return xmqttInternalHeader(Input, &Copy, Header, NULL);
}

XMQTT_API bool xmqttHeaderWrite(ptr Output, size_t Capacity, xmqttpackettype Type, uint8 Flags,
	uint32 RemainingLength, const xmqttwireconfig* Config, size_t* Written)
{
	xmqttwireconfig Copy;
	if (!xmqttWireConfig(Config, &Copy)) return false;
	if (!xmqttInternalFlags(Type, Flags, Copy.Version)) return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid outgoing MQTT type or flags");
	if (RemainingLength > XMQTT_VARIABLE_MAX) return xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "MQTT remaining length exceeds protocol maximum");
	unsigned char Encoded[5]; Encoded[0] = (unsigned char)(((unsigned)Type << 4) | Flags);
	size_t Bytes = 1u + xmqttWirePutVbi(Encoded+1u, RemainingLength);
	if (Bytes + RemainingLength > Copy.MaxPacketBytes) return xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "outgoing MQTT total packet size exceeds limit");
	if (!xmqttWireWriteOutput(Output, Capacity, Bytes, (xbytesview){0}, Config, Written)) return false;
	memcpy(Output, Encoded, Bytes); if (Written != NULL) *Written = Bytes; return true;
}

XMQTT_API xmqttwirestatus xmqttFrameRead(xbytesview Input, const xmqttwireconfig* Config, xmqttframe* Frame)
{
	xmqttwireconfig Copy; xmqttfixedheader Header;
	if (!xmqttWireHeaderArgs(Input, Config, Frame, sizeof(*Frame), &Copy)) return XMQTT_WIRE_ERROR;
	xmqttwirestatus Status = xmqttInternalHeader(Input, &Copy, &Header, NULL);
	if (Status != XMQTT_WIRE_DONE) return Status;
	if (Input.Size < Header.PacketBytes) return XMQTT_WIRE_MORE;
	*Frame = (xmqttframe){Header, {Input.Data, Header.PacketBytes}, {Input.Data+Header.HeaderBytes, Header.RemainingLength}};
	return XMQTT_WIRE_DONE;
}

static bool xmqttWireUtf8Flags(uint32 Flags)
{
	if (Flags & ~XMQTT_UTF8_REJECT_DISALLOWED)
		return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "unknown MQTT UTF-8 string policy");
	return true;
}

static bool xmqttWireTextFail(size_t Position, size_t* ErrorOffset, xmqttwireerror Code)
{
	if (ErrorOffset != NULL) *ErrorOffset = Position;
	return xmqttWireFail(Code == XMQTT_WIRE_LIMIT ? XERR_RANGE : XERR_PROTOCOL, Code, "invalid MQTT UTF-8 encoded string");
}

XMQTT_API bool xmqttUtf8StringCheck(xstrview Text, uint32 Flags, size_t* ErrorOffset)
{
	xbytesview Bytes = {(cbytes)Text.Data, Text.Size};
	if (!xmqttWireSpan(Bytes) || !xmqttWireUtf8Flags(Flags) ||
		(ErrorOffset != NULL && !xmqttWireReadOutput(ErrorOffset, sizeof(*ErrorOffset), Bytes))) return false;
	if (Text.Size > XMQTT_STRING_MAX_BYTES) return xmqttWireTextFail(XMQTT_STRING_MAX_BYTES, ErrorOffset, XMQTT_WIRE_LIMIT);
	for (size_t Position = 0; Position < Text.Size;) {
		uint32 Scalar = (unsigned char)Text.Data[Position]; size_t Read = 1;
		if (Scalar >= 128u && xrtUtf8Decode((xstrview){Text.Data+Position, Text.Size-Position}, &Scalar, &Read) != XUTF_OK)
			return xmqttWireTextFail(Position, ErrorOffset, XMQTT_WIRE_UTF8);
		if (Scalar == 0 || ((Flags & XMQTT_UTF8_REJECT_DISALLOWED) &&
			(Scalar <= 31u || (Scalar >= 127u && Scalar <= 159u) || (Scalar >= 0xfdd0u && Scalar <= 0xfdefu) || (Scalar & 0xffffu) >= 0xfffeu)))
			return xmqttWireTextFail(Position, ErrorOffset, XMQTT_WIRE_UTF8);
		Position += Read;
	}
	if (ErrorOffset != NULL) *ErrorOffset = XRT_NPOS;
	return true;
}

static xmqttwirestatus xmqttWireField(xbytesview Input, xbytesview* Value, size_t* Consumed)
{
	if (Input.Size < 2u) return XMQTT_WIRE_MORE;
	size_t Length = ((size_t)Input.Data[0] << 8) | Input.Data[1];
	if (Input.Size - 2u < Length) return XMQTT_WIRE_MORE;
	*Value = (xbytesview){Input.Data+2u, Length}; *Consumed = Length+2u; return XMQTT_WIRE_DONE;
}

XMQTT_API xmqttwirestatus xmqttBinaryRead(xbytesview Input, xbytesview* Value, size_t* Consumed)
{
	if (!xmqttWireReadPair(Input, Value, sizeof(*Value), Consumed)) return XMQTT_WIRE_ERROR;
	return xmqttWireField(Input, Value, Consumed);
}

XMQTT_API xmqttwirestatus xmqttUtf8StringRead(xbytesview Input, uint32 Flags, xstrview* Value, size_t* Consumed)
{
	if (!xmqttWireReadPair(Input, Value, sizeof(*Value), Consumed) || !xmqttWireUtf8Flags(Flags)) return XMQTT_WIRE_ERROR;
	xbytesview Bytes; size_t Read;
	xmqttwirestatus Status = xmqttWireField(Input, &Bytes, &Read);
	if (Status != XMQTT_WIRE_DONE) return Status;
	xstrview Text = {(cstr)Bytes.Data, Bytes.Size};
	if (!xmqttUtf8StringCheck(Text, Flags, NULL)) return XMQTT_WIRE_ERROR;
	*Value = Text; *Consumed = Read; return XMQTT_WIRE_DONE;
}

static bool xmqttWireFieldWrite(ptr Output, size_t Capacity, xbytesview Data, bool Text, uint32 Flags, size_t* Written)
{
	if (!xmqttWireSpan(Data)) return false;
	if (Data.Size > XMQTT_STRING_MAX_BYTES) return xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "MQTT string/binary field exceeds 65535 bytes");
	size_t Bytes = Data.Size+2u;
	if (!xmqttWireWriteOutput(Output, Capacity, Bytes, Data, NULL, Written)) return false;
	if (Text && !xmqttUtf8StringCheck((xstrview){(cstr)Data.Data, Data.Size}, Flags, NULL)) return false;
	unsigned char* Cursor = (unsigned char*)Output;
	Cursor[0] = (unsigned char)(Data.Size >> 8); Cursor[1] = (unsigned char)Data.Size;
	if (Data.Size) memcpy(Cursor+2u, Data.Data, Data.Size);
	if (Written != NULL) *Written = Bytes;
	return true;
}

XMQTT_API bool xmqttUtf8StringWrite(ptr Output, size_t Capacity, xstrview Text, uint32 Flags, size_t* Written)
{
	return xmqttWireFieldWrite(Output, Capacity, (xbytesview){(cbytes)Text.Data, Text.Size}, true, Flags, Written);
}

XMQTT_API bool xmqttBinaryWrite(ptr Output, size_t Capacity, xbytesview Data, size_t* Written)
{
	return xmqttWireFieldWrite(Output, Capacity, Data, false, 0, Written);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/properties/properties.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_PROPERTIES)

typedef struct xmqttpropertyspec { xmqttpropertytype Type; uint32 Scopes; } xmqttpropertyspec;
enum { MP_CONNECT=1u<<1, MP_CONNACK=1u<<2, MP_PUBLISH=1u<<3, MP_PUBACKS=0xfu<<4,
	MP_SUBSCRIBE=1u<<8, MP_SUBACK=1u<<9, MP_UNSUBSCRIBE=1u<<10, MP_UNSUBACK=1u<<11,
	MP_DISCONNECT=1u<<14, MP_AUTH=1u<<15, MP_WILL=1u<<16,
	MP_USER=MP_CONNECT|MP_CONNACK|MP_PUBLISH|MP_PUBACKS|MP_SUBSCRIBE|MP_SUBACK|MP_UNSUBSCRIBE|MP_UNSUBACK|MP_DISCONNECT|MP_AUTH|MP_WILL,
	MP_REASON=MP_CONNACK|MP_PUBACKS|MP_SUBACK|MP_UNSUBACK|MP_DISCONNECT|MP_AUTH };

/* Direct indexed standard registry; zero entries are unassigned ids. */
static const xmqttpropertyspec xmqttPropertySpecs[43] = {
	[1]={XMQTT_PROPERTY_BYTE,MP_PUBLISH|MP_WILL}, [2]={XMQTT_PROPERTY_UINT32,MP_PUBLISH|MP_WILL},
	[3]={XMQTT_PROPERTY_STRING,MP_PUBLISH|MP_WILL}, [8]={XMQTT_PROPERTY_STRING,MP_PUBLISH|MP_WILL},
	[9]={XMQTT_PROPERTY_BINARY,MP_PUBLISH|MP_WILL}, [11]={XMQTT_PROPERTY_VARIABLE,MP_PUBLISH|MP_SUBSCRIBE},
	[17]={XMQTT_PROPERTY_UINT32,MP_CONNECT|MP_CONNACK|MP_DISCONNECT}, [18]={XMQTT_PROPERTY_STRING,MP_CONNACK},
	[19]={XMQTT_PROPERTY_UINT16,MP_CONNACK}, [21]={XMQTT_PROPERTY_STRING,MP_CONNECT|MP_CONNACK|MP_AUTH},
	[22]={XMQTT_PROPERTY_BINARY,MP_CONNECT|MP_CONNACK|MP_AUTH}, [23]={XMQTT_PROPERTY_BYTE,MP_CONNECT},
	[24]={XMQTT_PROPERTY_UINT32,MP_WILL}, [25]={XMQTT_PROPERTY_BYTE,MP_CONNECT}, [26]={XMQTT_PROPERTY_STRING,MP_CONNACK},
	[28]={XMQTT_PROPERTY_STRING,MP_CONNACK|MP_DISCONNECT}, [31]={XMQTT_PROPERTY_STRING,MP_REASON},
	[33]={XMQTT_PROPERTY_UINT16,MP_CONNECT|MP_CONNACK}, [34]={XMQTT_PROPERTY_UINT16,MP_CONNECT|MP_CONNACK},
	[35]={XMQTT_PROPERTY_UINT16,MP_PUBLISH}, [36]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [37]={XMQTT_PROPERTY_BYTE,MP_CONNACK},
	[38]={XMQTT_PROPERTY_STRING_PAIR,MP_USER}, [39]={XMQTT_PROPERTY_UINT32,MP_CONNECT|MP_CONNACK},
	[40]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [41]={XMQTT_PROPERTY_BYTE,MP_CONNACK}, [42]={XMQTT_PROPERTY_BYTE,MP_CONNACK}
};

static xmqttpropertystatus xmqttPropertiesFail(xmqttpropertystatus Status, xmqttpropertyerror Code, cstr Message)
{
	xerrkind Kind = Code == XMQTT_PROPERTY_ARGUMENT ? XERR_ARGUMENT :
		(Code == XMQTT_PROPERTY_LIMIT || Code == XMQTT_PROPERTY_CAPACITY) ? XERR_RANGE : XERR_PROTOCOL;
	xrtSetErrorInfo(Kind,"xrt.mqtt",(int32)Code,Message); return Status;
}

static bool xmqttPropertiesSpan(const void* Data, size_t Size)
{
	return (Data != NULL || Size == 0) && xrtMemRangeValid(Data,Size);
}

static bool xmqttPropertiesSettings(xmqttpropertyscope Scope, xmqttdirection Direction,
	const xmqttpropertyconfig* Config, xmqttpropertyconfig* Copy)
{
	if (!((Scope >= XMQTT_PROPERTIES_CONNECT && Scope <= XMQTT_PROPERTIES_UNSUBACK) ||
		(Scope >= XMQTT_PROPERTIES_DISCONNECT && Scope <= XMQTT_PROPERTIES_WILL)) ||
		(Direction != XMQTT_CLIENT_TO_SERVER && Direction != XMQTT_SERVER_TO_CLIENT) ||
		((Scope == XMQTT_PROPERTIES_CONNECT || Scope == XMQTT_PROPERTIES_WILL || Scope == XMQTT_PROPERTIES_SUBSCRIBE || Scope == XMQTT_PROPERTIES_UNSUBSCRIBE) && Direction != XMQTT_CLIENT_TO_SERVER) ||
		((Scope == XMQTT_PROPERTIES_CONNACK || Scope == XMQTT_PROPERTIES_SUBACK || Scope == XMQTT_PROPERTIES_UNSUBACK) && Direction != XMQTT_SERVER_TO_CLIENT) ||
		(Config != NULL && !xrtMemRangeValid(Config,sizeof(*Config)))) goto Invalid;
	if (Config == NULL) xmqttPropertiesConfigInit(Copy); else *Copy = *Config;
	if (Copy->Size != sizeof(*Copy) || !Copy->MaxBytes || Copy->MaxBytes > XMQTT_PROPERTY_SECTION_MAX || (Copy->Utf8Flags & ~XMQTT_UTF8_REJECT_DISALLOWED)) goto Invalid;
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property scope, direction or configuration"); return false;
}

XMQTT_API bool xmqttPropertiesConfigInit(xmqttpropertyconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config,sizeof(*Config))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property config output"); return false;
	}
	*Config = (xmqttpropertyconfig){sizeof(*Config),1024u*1024u,256,0}; return true;
}

static const xmqttpropertyspec* xmqttPropertiesSpec(xmqttpropertyid Id)
{
	return Id > 0 && Id < 43 && xmqttPropertySpecs[Id].Type != 0 ? &xmqttPropertySpecs[Id] : NULL;
}

static xmqttpropertystatus xmqttPropertiesRule(const xmqttproperty* Property, const xmqttpropertyspec* Spec,
	xmqttpropertyscope Scope, xmqttdirection Direction, uint64* Seen)
{
	if (!(Spec->Scopes & (UINT32_C(1) << Scope)))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"MQTT property is not valid for its packet or Will scope");
	if ((Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID && Scope == XMQTT_PROPERTIES_PUBLISH && Direction == XMQTT_CLIENT_TO_SERVER) ||
		(Property->Id == XMQTT_PROPERTY_SESSION_EXPIRY && Scope == XMQTT_PROPERTIES_DISCONNECT && Direction == XMQTT_SERVER_TO_CLIENT))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT property is forbidden in this direction");
	uint64 Bit = UINT64_C(1) << Property->Id;
	bool Repeated = Property->Id == XMQTT_PROPERTY_USER || (Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID && Scope == XMQTT_PROPERTIES_PUBLISH);
	if ((*Seen & Bit) && !Repeated)
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"duplicate singleton MQTT property");
	*Seen |= Bit; return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesValue(const xmqttproperty* Property)
{
	uint32 Number = Property->Type <= XMQTT_PROPERTY_VARIABLE ? Property->Value.Number : 0;
	if ((Property->Type == XMQTT_PROPERTY_BYTE && Number > 255u) ||
		(Property->Type == XMQTT_PROPERTY_UINT16 && Number > 65535u) ||
		(Property->Type == XMQTT_PROPERTY_VARIABLE && Number > XMQTT_VARIABLE_MAX) ||
		((Property->Id == XMQTT_PROPERTY_PAYLOAD_FORMAT || Property->Id == XMQTT_PROPERTY_REQUEST_PROBLEM_INFO ||
		Property->Id == XMQTT_PROPERTY_REQUEST_RESPONSE_INFO || Property->Id == XMQTT_PROPERTY_MAXIMUM_QOS ||
		Property->Id == XMQTT_PROPERTY_RETAIN_AVAILABLE || Property->Id == XMQTT_PROPERTY_WILDCARD_AVAILABLE ||
		Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE || Property->Id == XMQTT_PROPERTY_SHARED_AVAILABLE) && Number > 1u) ||
		((Property->Id == XMQTT_PROPERTY_SUBSCRIPTION_ID || Property->Id == XMQTT_PROPERTY_RECEIVE_MAXIMUM ||
		Property->Id == XMQTT_PROPERTY_TOPIC_ALIAS || Property->Id == XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE) && Number == 0))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT property numeric value is out of range");
	if (Property->Id == XMQTT_PROPERTY_RESPONSE_TOPIC) {
		xstrview Text = Property->Value.Text;
		if (!Text.Size) return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT response topic is empty");
		for (size_t Index = 0; Index < Text.Size; ++Index)
			if (Text.Data[Index] == '+' || Text.Data[Index] == '#')
				return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT response topic contains a wildcard");
	}
	return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesFinal(xmqttpropertyscope Scope, uint64 Seen)
{
	uint64 Method = UINT64_C(1) << XMQTT_PROPERTY_AUTH_METHOD, Data = UINT64_C(1) << XMQTT_PROPERTY_AUTH_DATA;
	if ((Scope == XMQTT_PROPERTIES_CONNECT && (Seen & Data) && !(Seen & Method)) || (Scope == XMQTT_PROPERTIES_AUTH && !(Seen & Method)))
		return xmqttPropertiesFail(XMQTT_PROPERTIES_PROTOCOL_ERROR,XMQTT_PROPERTY_PROTOCOL,"MQTT authentication method is required in this property section");
	return XMQTT_PROPERTIES_DONE;
}

static xmqttpropertystatus xmqttPropertiesField(xbytesview Input, uint32 Flags, xmqttproperty* Property, size_t* Consumed)
{
	xmqttwirestatus Status; size_t First;
	if (Property->Type == XMQTT_PROPERTY_STRING) Status = xmqttUtf8StringRead(Input,Flags,&Property->Value.Text,Consumed);
	else if (Property->Type == XMQTT_PROPERTY_BINARY) Status = xmqttBinaryRead(Input,&Property->Value.Binary,Consumed);
	else {
		Status = xmqttUtf8StringRead(Input,Flags,&Property->Value.Pair.Name,&First);
		if (Status == XMQTT_WIRE_DONE) {
			Status = xmqttUtf8StringRead((xbytesview){Input.Data+First,Input.Size-First},Flags,&Property->Value.Pair.Value,Consumed);
			if (Status == XMQTT_WIRE_DONE) *Consumed += First;
		}
	}
	if (Status == XMQTT_WIRE_DONE) return XMQTT_PROPERTIES_DONE;
	if (Status == XMQTT_WIRE_ERROR) return XMQTT_PROPERTIES_MALFORMED; /* Confirmed UTF-8 error, even if diagnostic OOM. */
	return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"MQTT property field crosses its declared section boundary");
}

static bool xmqttPropertiesReadArgs(xbytesview Input, const xmqttpropertyconfig* Config,
	xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document)
{
	if (!xmqttPropertiesSpan(Input.Data,Input.Size) || Capacity > SIZE_MAX/sizeof(*Properties) ||
		!xmqttPropertiesSpan(Properties,Capacity*sizeof(*Properties)) || Document == NULL || !xrtMemRangeValid(Document,sizeof(*Document))) goto Invalid;
	size_t Storage = Capacity*sizeof(*Properties);
	if (xrtMemRangesOverlap(Properties,Storage,Input.Data,Input.Size) || xrtMemRangesOverlap(Document,sizeof(*Document),Input.Data,Input.Size) ||
		xrtMemRangesOverlap(Properties,Storage,Document,sizeof(*Document)) ||
		(Config != NULL && (xrtMemRangesOverlap(Properties,Storage,Config,sizeof(*Config)) || xrtMemRangesOverlap(Document,sizeof(*Document),Config,sizeof(*Config))))) goto Invalid;
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property input/output spans or overlap"); return false;
}

XMQTT_API xmqttpropertystatus xmqttPropertiesRead(xbytesview Input, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document)
{
	xmqttpropertyconfig Limits;
	if (!xmqttPropertiesSettings(Scope,Direction,Config,&Limits) || !xmqttPropertiesReadArgs(Input,Config,Properties,Capacity,Document)) return XMQTT_PROPERTIES_LOCAL_ERROR;
	uint32 Length; size_t Prefix;
	xmqttwirestatus WireStatus = xmqttVbiRead(Input,XMQTT_VERSION_5,&Length,&Prefix);
	if (WireStatus != XMQTT_WIRE_DONE) return WireStatus == XMQTT_WIRE_MORE ? XMQTT_PROPERTIES_MORE : XMQTT_PROPERTIES_MALFORMED;
	size_t Total = Prefix+Length;
	if (Total > Limits.MaxBytes) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property section exceeds configured bytes");
	if (Input.Size < Total) return XMQTT_PROPERTIES_MORE;
	size_t Position = Prefix, Count = 0; uint64 Seen = 0;
	while (Position < Total) {
		if (Count >= Limits.MaxProperties) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property count exceeds configured limit");
		if (Count >= Capacity) return xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_CAPACITY,"MQTT property scratch capacity exhausted");
		uint32 Id; size_t Read;
		WireStatus = xmqttVbiRead((xbytesview){Input.Data+Position,Total-Position},XMQTT_VERSION_5,&Id,&Read);
		if (WireStatus != XMQTT_WIRE_DONE) {
			if (WireStatus == XMQTT_WIRE_MORE) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT property identifier");
			return XMQTT_PROPERTIES_MALFORMED;
		}
		Position += Read;
		xmqttproperty Property = {0}; Property.Id = (xmqttpropertyid)Id;
		const xmqttpropertyspec* Spec = xmqttPropertiesSpec(Property.Id);
		if (Spec == NULL) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"unknown MQTT property identifier");
		Property.Type = Spec->Type;
		xmqttpropertystatus Status = xmqttPropertiesRule(&Property,Spec,Scope,Direction,&Seen);
		if (Status != XMQTT_PROPERTIES_DONE) return Status;
		if (Property.Type >= XMQTT_PROPERTY_STRING) {
			Status = xmqttPropertiesField((xbytesview){Input.Data+Position,Total-Position},Limits.Utf8Flags,&Property,&Read);
			if (Status != XMQTT_PROPERTIES_DONE) return Status;
		} else if (Property.Type == XMQTT_PROPERTY_VARIABLE) {
			WireStatus = xmqttVbiRead((xbytesview){Input.Data+Position,Total-Position},XMQTT_VERSION_5,&Property.Value.Number,&Read);
			if (WireStatus == XMQTT_WIRE_MORE) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT variable property value");
			if (WireStatus != XMQTT_WIRE_DONE) return XMQTT_PROPERTIES_MALFORMED;
		} else {
			Read = Property.Type == XMQTT_PROPERTY_BYTE ? 1u : Property.Type == XMQTT_PROPERTY_UINT16 ? 2u : 4u;
			if (Total-Position < Read) return xmqttPropertiesFail(XMQTT_PROPERTIES_MALFORMED,XMQTT_PROPERTY_MALFORMED,"truncated MQTT numeric property value");
			for (size_t Index = 0; Index < Read; ++Index) Property.Value.Number = (Property.Value.Number << 8) | Input.Data[Position+Index];
		}
		Position += Read;
		Status = xmqttPropertiesValue(&Property); if (Status != XMQTT_PROPERTIES_DONE) return Status;
		Properties[Count++] = Property;
	}
	xmqttpropertystatus Status = xmqttPropertiesFinal(Scope,Seen); if (Status != XMQTT_PROPERTIES_DONE) return Status;
	*Document = (xmqttpropertydocument){Count,Total,{Input.Data,Total}}; return XMQTT_PROPERTIES_DONE;
}

static size_t xmqttPropertiesVbiSize(uint32 Number)
{
	return 1u + (Number >= 128u) + (Number >= 16384u) + (Number >= 2097152u);
}

static size_t xmqttPropertiesViews(const xmqttproperty* Property, xbytesview* Views)
{
	if (Property->Type == XMQTT_PROPERTY_STRING) { Views[0] = (xbytesview){(cbytes)Property->Value.Text.Data,Property->Value.Text.Size}; return 1; }
	if (Property->Type == XMQTT_PROPERTY_BINARY) { Views[0] = Property->Value.Binary; return 1; }
	if (Property->Type == XMQTT_PROPERTY_STRING_PAIR) {
		Views[0] = (xbytesview){(cbytes)Property->Value.Pair.Name.Data,Property->Value.Pair.Name.Size};
		Views[1] = (xbytesview){(cbytes)Property->Value.Pair.Value.Data,Property->Value.Pair.Value.Size}; return 2;
	}
	return 0;
}

XMQTT_INTERNAL bool xmqttInternalPropertiesPrepare(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, xmqttpropertyconfig* Limits, size_t* Size, uint32* BodySize)
{
	if (!xmqttPropertiesSettings(Scope,Direction,Config,Limits)) return false;
	if (Count > SIZE_MAX/sizeof(*Properties) || !xmqttPropertiesSpan(Properties,Count*sizeof(*Properties))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property vector span"); return false;
	}
	if (Count > Limits->MaxProperties) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property count exceeds configured limit"); return false;
	}
	size_t Body = 0; uint64 Seen = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xmqttproperty* Property = &Properties[Index]; const xmqttpropertyspec* Spec = xmqttPropertiesSpec(Property->Id);
		if (Spec == NULL || Property->Type != Spec->Type) {
			xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"unknown MQTT property id or mismatched descriptor type"); return false;
		}
		if (xmqttPropertiesRule(Property,Spec,Scope,Direction,&Seen) != XMQTT_PROPERTIES_DONE) return false;
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(Property,Views), Bytes = 1;
		for (size_t View = 0; View < ViewCount; ++View) {
			if (!xmqttPropertiesSpan(Views[View].Data,Views[View].Size)) {
				xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property data span"); return false;
			}
			if (Views[View].Size > XMQTT_STRING_MAX_BYTES) {
				xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property field exceeds uint16 length"); return false;
			}
			Bytes += 2u+Views[View].Size;
		}
		if (!ViewCount && xmqttPropertiesValue(Property) != XMQTT_PROPERTIES_DONE) return false;
		if (!ViewCount) Bytes += Property->Type == XMQTT_PROPERTY_BYTE ? 1u : Property->Type == XMQTT_PROPERTY_UINT16 ? 2u :
			Property->Type == XMQTT_PROPERTY_UINT32 ? 4u : xmqttPropertiesVbiSize(Property->Value.Number);
		/* Keep at least one byte for Property Length. Reject a known byte-budget
		 * failure before walking UTF-8 or scanning a response topic. */
		if (Bytes > XMQTT_VARIABLE_MAX-Body || Bytes >= Limits->MaxBytes || Body >= Limits->MaxBytes-Bytes) {
			xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property section exceeds configured or protocol size"); return false;
		}
		for (size_t View = 0; View < ViewCount; ++View)
			if (Property->Type != XMQTT_PROPERTY_BINARY && !xmqttUtf8StringCheck((xstrview){(cstr)Views[View].Data,Views[View].Size},Limits->Utf8Flags,NULL)) return false;
		if (ViewCount && xmqttPropertiesValue(Property) != XMQTT_PROPERTIES_DONE) return false;
		Body += Bytes;
	}
	if (xmqttPropertiesFinal(Scope,Seen) != XMQTT_PROPERTIES_DONE) return false;
	size_t Total = Body+xmqttPropertiesVbiSize((uint32)Body);
	if (Total > Limits->MaxBytes) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_LIMIT,"MQTT property length prefix exceeds configured section bytes"); return false;
	}
	*Size = Total; *BodySize = (uint32)Body; return true;
}

XMQTT_INTERNAL bool xmqttInternalPropertiesOutput(const void* Output, size_t Size, const xmqttproperty* Properties,
	size_t Count, const xmqttpropertyconfig* Config)
{
	if (Output == NULL || !xrtMemRangeValid(Output,Size) || xrtMemRangesOverlap(Output,Size,Properties,Count*sizeof(*Properties)) ||
		(Config != NULL && xrtMemRangesOverlap(Output,Size,Config,sizeof(*Config)))) goto Invalid;
	for (size_t Index = 0; Index < Count; ++Index) {
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(&Properties[Index],Views);
		for (size_t View = 0; View < ViewCount; ++View)
			if (xrtMemRangesOverlap(Output,Size,Views[View].Data,Views[View].Size)) goto Invalid;
	}
	return true;
Invalid:
	xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"MQTT property output overlaps inputs/config or has invalid span"); return false;
}

XMQTT_API bool xmqttPropertiesSize(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Required)
{
	xmqttpropertyconfig Limits; size_t Size; uint32 Body;
	if (!xmqttInternalPropertiesPrepare(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body) || !xmqttInternalPropertiesOutput(Required,sizeof(*Required),Properties,Count,Config)) return false;
	*Required = Size; return true;
}

XMQTT_API bool xmqttPropertiesWrite(ptr Output, size_t Capacity, const xmqttproperty* Properties, size_t Count,
	xmqttpropertyscope Scope, xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Written)
{
	xmqttpropertyconfig Limits; size_t Size; uint32 Body;
	if (Output == NULL || !xrtMemRangeValid(Output,Capacity)) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property wire buffer"); return false;
	}
	if (!xmqttInternalPropertiesPrepare(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body)) return false;
	if (Capacity < Size) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_CAPACITY,"MQTT property wire capacity is too small"); return false;
	}
	if (!xmqttInternalPropertiesOutput(Output,Size,Properties,Count,Config)) return false;
	if (Written != NULL && (!xmqttInternalPropertiesOutput(Written,sizeof(*Written),Properties,Count,Config) || xrtMemRangesOverlap(Written,sizeof(*Written),Output,Size))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"MQTT property Written overlaps wire or inputs"); return false;
	}
	(void)xmqttInternalPropertiesEmit(Output,Properties,Count,Body,Size);
	if (Written != NULL) *Written = Size;
	return true;
}

XMQTT_INTERNAL size_t xmqttInternalPropertiesEmit(ptr Output, const xmqttproperty* Properties, size_t Count, uint32 Body, size_t Size)
{
	unsigned char* Cursor = (unsigned char*)Output; size_t Bytes;
	/* Infallible emission for preflighted immutable, nonoverlapping inputs.
	 * Text is not rescanned; only the active views' bytes are copied once. */
	(void)xmqttVbiWrite(Cursor,Size,Body,&Bytes); Cursor += Bytes;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xmqttproperty* Property = &Properties[Index]; *Cursor++ = (unsigned char)Property->Id;
		xbytesview Views[2]; size_t ViewCount = xmqttPropertiesViews(Property,Views);
		for (size_t View = 0; View < ViewCount; ++View) {
			*Cursor++ = (unsigned char)(Views[View].Size >> 8); *Cursor++ = (unsigned char)Views[View].Size;
			if (Views[View].Size) { memcpy(Cursor,Views[View].Data,Views[View].Size); Cursor += Views[View].Size; }
		}
		if (!ViewCount) {
			if (Property->Type == XMQTT_PROPERTY_VARIABLE) {
				(void)xmqttVbiWrite(Cursor,Size-(size_t)(Cursor-(unsigned char*)Output),Property->Value.Number,&Bytes); Cursor += Bytes;
			}
			else {
				unsigned Width = Property->Type == XMQTT_PROPERTY_BYTE ? 1u : Property->Type == XMQTT_PROPERTY_UINT16 ? 2u : 4u;
				for (unsigned Byte = 0; Byte < Width; ++Byte) *Cursor++ = (unsigned char)(Property->Value.Number >> (8u*(Width-Byte-1u)));
			}
		}
	}
	return (size_t)(Cursor-(unsigned char*)Output);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/packet/packet.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_PACKET)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_PACKET)

static bool xmqttPacketArgument(cstr Message)
{
	xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_CODEC_ARGUMENT,Message); return false;
}

XMQTT_API bool xmqttPacketInit(xmqttpackettype Type, xmqttpacket* Packet)
{
	if (Type<XMQTT_PACKET_CONNECT || Type>XMQTT_PACKET_AUTH || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)))
		return xmqttPacketArgument("invalid MQTT packet initializer");
	*Packet=(xmqttpacket){0}; Packet->Type=Type;
	if (Type==XMQTT_PACKET_PUBREL || Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) Packet->Flags=2;
	return true;
}

XMQTT_API bool xmqttCodecConfigInit(xmqttcodecconfig* Config)
{
	if (Config==NULL || !xrtMemRangeValid(Config,sizeof(*Config))) return xmqttPacketArgument("invalid MQTT codec config output");
	*Config=(xmqttcodecconfig){sizeof(*Config),1024u*1024u,1024u*1024u,1024u*1024u,256,1024,0}; return true;
}

XMQTT_INTERNAL bool xmqttInternalCodecSettings(xmqttdirection Direction, const xmqttcodecconfig* Config, xmqttcodecconfig* Copy)
{
	if ((Direction!=XMQTT_CLIENT_TO_SERVER && Direction!=XMQTT_SERVER_TO_CLIENT) || (Config!=NULL && !xrtMemRangeValid(Config,sizeof(*Config))))
		return xmqttPacketArgument("invalid MQTT codec direction/config span");
	if (Config==NULL) xmqttCodecConfigInit(Copy); else *Copy=*Config;
	if (Copy->Size!=sizeof(*Copy) || Copy->MaxPacketBytes<2 || Copy->MaxPacketBytes>XMQTT_PACKET_MAX_BYTES ||
		Copy->MaxPayloadBytes>XMQTT_VARIABLE_MAX || Copy->MaxPropertyBytes>XMQTT_VARIABLE_MAX+4u || (Copy->Utf8Flags&~XMQTT_UTF8_REJECT_DISALLOWED))
		return xmqttPacketArgument("invalid MQTT codec resource configuration");
	return true;
}

XMQTT_INTERNAL bool xmqttInternalRole(xmqttpackettype Type, xmqttdirection Direction)
{
	if (Type<XMQTT_PACKET_CONNECT || Type>XMQTT_PACKET_AUTH) return false;
	if (Type==XMQTT_PACKET_CONNECT || Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE || Type==XMQTT_PACKET_PINGREQ)
		return Direction==XMQTT_CLIENT_TO_SERVER;
	if (Type==XMQTT_PACKET_CONNACK || Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK || Type==XMQTT_PACKET_PINGRESP)
		return Direction==XMQTT_SERVER_TO_CLIENT;
	return true;
}

XMQTT_INTERNAL bool xmqttInternalReason(xmqttversion Version, xmqttpackettype Type, uint8 Reason, xmqttdirection Direction)
{
	if (Version==XMQTT_VERSION_311) {
		if (Type==XMQTT_PACKET_CONNACK) return Reason<=5;
		if (Type==XMQTT_PACKET_SUBACK) return Reason<=2 || Reason==0x80;
		return Reason==0;
	}
	static const uint8 Connack[]={0,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8c,0x90,0x95,0x97,0x99,0x9a,0x9b,0x9c,0x9d,0x9f};
	static const uint8 Puback[]={0,0x10,0x80,0x83,0x87,0x90,0x91,0x97,0x99};
	static const uint8 Pubrel[]={0,0x92};
	static const uint8 Suback[]={0,1,2,0x80,0x83,0x87,0x8f,0x91,0x97,0x9e,0xa1,0xa2};
	static const uint8 Unsuback[]={0,0x11,0x80,0x83,0x87,0x8f,0x91};
	static const uint8 DisconnectBoth[]={0,0x80,0x81,0x82,0x83,0x90,0x93,0x94,0x95,0x96,0x97,0x98,0x99};
	static const uint8 DisconnectServer[]={0x87,0x89,0x8b,0x8d,0x8e,0x8f,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0xa0,0xa1,0xa2};
	const uint8* Values=NULL; size_t Count=0;
	if (Type==XMQTT_PACKET_CONNACK) { Values=Connack; Count=sizeof(Connack); }
	else if (Type==XMQTT_PACKET_PUBACK || Type==XMQTT_PACKET_PUBREC) { Values=Puback; Count=sizeof(Puback); }
	else if (Type==XMQTT_PACKET_PUBREL || Type==XMQTT_PACKET_PUBCOMP) { Values=Pubrel; Count=sizeof(Pubrel); }
	else if (Type==XMQTT_PACKET_SUBACK) { Values=Suback; Count=sizeof(Suback); }
	else if (Type==XMQTT_PACKET_UNSUBACK) { Values=Unsuback; Count=sizeof(Unsuback); }
	else if (Type==XMQTT_PACKET_AUTH) return Reason==0x18 || (Reason==0 && Direction==XMQTT_SERVER_TO_CLIENT) || (Reason==0x19 && Direction==XMQTT_CLIENT_TO_SERVER);
	else if (Type==XMQTT_PACKET_DISCONNECT) {
		if (Reason==4 && Direction==XMQTT_CLIENT_TO_SERVER) return true;
		for (size_t Index=0; Index<sizeof(DisconnectBoth); ++Index) if (Reason==DisconnectBoth[Index]) return true;
		if (Direction==XMQTT_SERVER_TO_CLIENT)
			for (size_t Index=0; Index<sizeof(DisconnectServer); ++Index) if (Reason==DisconnectServer[Index]) return true;
		return false;
	} else return Reason==0;
	for (size_t Index=0; Index<Count; ++Index) if (Reason==Values[Index]) return true;
	return false;
}

XMQTT_INTERNAL bool xmqttInternalTopic(xstrview Text, bool Filter, bool Shared, bool* IsShared)
{
	size_t Begin=0;
	if (IsShared!=NULL) *IsShared=false;
	if (!Text.Size) return false;
	if (Filter && Shared && Text.Size>=7 && memcmp(Text.Data,"$share/",7)==0) {
		size_t Slash=7;
		while (Slash<Text.Size && Text.Data[Slash]!='/') {
			if (Text.Data[Slash]=='+' || Text.Data[Slash]=='#') return false;
			++Slash;
		}
		if (Slash==7 || Slash==Text.Size || Slash+1u==Text.Size) return false;
		Begin=Slash+1u; if (IsShared!=NULL) *IsShared=true;
	}
	for (size_t Index=Begin; Index<Text.Size; ++Index) {
		if (Text.Data[Index]=='+') {
			if (!Filter || (Index!=Begin && Text.Data[Index-1u]!='/') || (Index+1u<Text.Size && Text.Data[Index+1u]!='/')) return false;
		} else if (Text.Data[Index]=='#') {
			if (!Filter || Index+1u!=Text.Size || (Index!=Begin && Text.Data[Index-1u]!='/')) return false;
		}
	}
	return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/codec5/decode.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)

typedef struct xmqtt5reader {
	xbytesview Input;
	size_t Position, End, PropertiesUsed, PropertyBytes;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	xmqttcodecbuffers Buffers;
	xmqttpacket Value;
	xmqttcodecissue Failure;
} xmqtt5reader;

static bool xmqtt5ReadMark(xmqtt5reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset)
{
	Reader->Failure=(xmqttcodecissue){Status,Error,Offset,Reader->Value.Type,Reader->Value.Flags,Reader->Value.Identifier}; return false;
}

static bool xmqtt5ReadFail(xmqtt5reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset, cstr Message)
{
	xerrkind Kind=Status==XMQTT_CODEC_LOCAL_ERROR ? (Error==XMQTT_CODEC_ARGUMENT ? XERR_ARGUMENT : XERR_RANGE) : XERR_PROTOCOL;
	xmqtt5ReadMark(Reader,Status,Error,Offset); xrtSetErrorInfo(Kind,"xrt.mqtt",Error,Message); return false;
}

static bool xmqtt5ReadArgs(xbytesview Input, const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue, xmqttcodecbuffers* Copy)
{
	if ((Input.Data==NULL && Input.Size) || !xrtMemRangeValid(Input.Data,Input.Size) || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Buffers!=NULL && !xrtMemRangeValid(Buffers,sizeof(*Buffers)))) goto Invalid;
	*Copy=Buffers==NULL ? (xmqttcodecbuffers){0} : *Buffers;
	if (Copy->PropertyCapacity>SIZE_MAX/sizeof(xmqttproperty) || Copy->TopicCapacity>SIZE_MAX/sizeof(xmqttsubscription) ||
		(Copy->Properties==NULL && Copy->PropertyCapacity) || (Copy->Topics==NULL && Copy->TopicCapacity) ||
		!xrtMemRangeValid(Copy->Properties,Copy->PropertyCapacity*sizeof(xmqttproperty)) || !xrtMemRangeValid(Copy->Topics,Copy->TopicCapacity*sizeof(xmqttsubscription))) goto Invalid;
	const void* Outputs[]={Packet,Issue,Copy->Properties,Copy->Topics};
	size_t Sizes[]={sizeof(*Packet),Issue==NULL ? 0 : sizeof(*Issue),Copy->PropertyCapacity*sizeof(xmqttproperty),Copy->TopicCapacity*sizeof(xmqttsubscription)};
	for (size_t Index=0; Index<4; ++Index) {
		if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Input.Data,Input.Size) ||
			(Config!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Config,sizeof(*Config))) ||
			(Buffers!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Buffers,sizeof(*Buffers)))) goto Invalid;
		for (size_t Other=0; Other<Index; ++Other)
			if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Outputs[Other],Sizes[Other])) goto Invalid;
	}
	return true;
Invalid:
	xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_CODEC_ARGUMENT,"invalid MQTT packet read spans or output overlap"); return false;
}

static bool xmqtt5ReadByte(xmqtt5reader* Reader, uint8* Value)
{
	if (Reader->Position==Reader->End) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT byte field");
	*Value=Reader->Input.Data[Reader->Position++]; return true;
}

static bool xmqtt5ReadWord(xmqtt5reader* Reader, uint16* Value)
{
	if (Reader->End-Reader->Position<2) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT two-byte integer");
	*Value=(uint16)(((uint16)Reader->Input.Data[Reader->Position]<<8)|Reader->Input.Data[Reader->Position+1u]); Reader->Position+=2; return true;
}

static bool xmqtt5ReadIdentifier(xmqtt5reader* Reader)
{
	uint16 Identifier; size_t Start=Reader->Position;
	if (!xmqtt5ReadWord(Reader,&Identifier)) return false;
	if (!Identifier) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"MQTT packet identifier is zero");
	Reader->Value.Identifier=Identifier; return true;
}

static bool xmqtt5ReadField(xmqtt5reader* Reader, bool Text, ptr Value)
{
	xbytesview Input={Reader->Input.Data+Reader->Position,Reader->End-Reader->Position}; size_t Read;
	xmqttwirestatus Status=Text ? xmqttUtf8StringRead(Input,Reader->Config.Utf8Flags,(xstrview*)Value,&Read) : xmqttBinaryRead(Input,(xbytesview*)Value,&Read);
	if (Status==XMQTT_WIRE_MORE) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT field crosses packet boundary");
	if (Status==XMQTT_WIRE_ERROR) return xmqtt5ReadMark(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position);
	Reader->Position+=Read; return true;
}

static bool xmqtt5ReadProperties(xmqtt5reader* Reader, xmqttpropertyscope Scope, xmqttpropertylist* Values)
{
	if (Reader->PropertyBytes==Reader->Config.MaxPropertyBytes)
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT aggregate property byte limit exhausted");
	xmqttpropertyconfig Config={sizeof(Config),Reader->Config.MaxPropertyBytes-Reader->PropertyBytes,
		Reader->Config.MaxProperties-Reader->PropertiesUsed,Reader->Config.Utf8Flags};
	xmqttproperty* Output=Reader->Buffers.Properties==NULL ? NULL : Reader->Buffers.Properties+Reader->PropertiesUsed;
	xmqttpropertydocument Doc;
	xmqttpropertystatus Status=xmqttPropertiesRead((xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position},Scope,Reader->Direction,
		&Config,Output,Reader->Buffers.PropertyCapacity-Reader->PropertiesUsed,&Doc);
	if (Status!=XMQTT_PROPERTIES_DONE) {
		if (Status==XMQTT_PROPERTIES_MORE) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT property section crosses packet boundary");
		return xmqtt5ReadMark(Reader,Status==XMQTT_PROPERTIES_LOCAL_ERROR ? XMQTT_CODEC_LOCAL_ERROR :
			Status==XMQTT_PROPERTIES_PROTOCOL_ERROR ? XMQTT_CODEC_PROTOCOL_ERROR : XMQTT_CODEC_MALFORMED,
			Status==XMQTT_PROPERTIES_LOCAL_ERROR ? XMQTT_CODEC_LIMIT : Status==XMQTT_PROPERTIES_PROTOCOL_ERROR ? XMQTT_CODEC_PROTOCOL : XMQTT_CODEC_FORMAT,Reader->Position);
	}
	*Values=(xmqttpropertylist){Doc.Count ? Output : NULL,Doc.Count};
	Reader->PropertiesUsed+=Doc.Count; Reader->PropertyBytes+=Doc.Consumed; Reader->Position+=Doc.Consumed; return true;
}

static bool xmqtt5ReadPayload(xmqtt5reader* Reader, xbytesview Data, xmqttpropertylist Properties, size_t Offset)
{
	if (Data.Size>Reader->Config.MaxPayloadBytes) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Offset,"MQTT payload exceeds configured limit");
	for (size_t Index=0; Index<Properties.Count; ++Index) {
		if (Properties.Data[Index].Id==XMQTT_PROPERTY_PAYLOAD_FORMAT && Properties.Data[Index].Value.Number==1) {
			size_t Error;
			if (!xrtUtf8Valid((xstrview){(cstr)Data.Data,Data.Size},&Error))
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_PAYLOAD_ERROR,XMQTT_CODEC_PAYLOAD_FORMAT,Offset+Error,"MQTT indicated payload format is not well-formed UTF-8");
			break;
		}
	}
	return true;
}

static bool xmqtt5ReadConnect(xmqtt5reader* Reader)
{
	xmqttconnectpacket* Value=&Reader->Value.Body.Connect; xstrview Name; uint8 Version, Flags; size_t Start=Reader->Position;
	if (!xmqtt5ReadField(Reader,true,&Name)) return false;
	if (Name.Size!=4 || memcmp(Name.Data,"MQTT",4)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"unsupported MQTT protocol name");
	Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Version)) return false;
	if (Version!=5) return xmqtt5ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"CONNECT is not MQTT version 5");
	Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Flags)) return false;
	if ((Flags&1u) || ((Flags&0x18u)==0x18u) || (!(Flags&4u) && (Flags&0x38u)))
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT CONNECT flags");
	Value->CleanStart=(Flags&2u)!=0; Value->HasWill=(Flags&4u)!=0; Value->WillQos=(Flags>>3)&3u; Value->WillRetain=(Flags&0x20u)!=0;
	Value->HasUsername=(Flags&0x80u)!=0; Value->HasPassword=(Flags&0x40u)!=0;
	if (!xmqtt5ReadWord(Reader,&Value->KeepAlive) || !xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_CONNECT,&Reader->Value.Properties) ||
		!xmqtt5ReadField(Reader,true,&Value->ClientId)) return false;
	if (Value->HasWill) {
		if (!xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_WILL,&Value->WillProperties)) return false;
		Start=Reader->Position; if (!xmqtt5ReadField(Reader,true,&Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Will Topic");
		Start=Reader->Position;
		if (!xmqtt5ReadField(Reader,false,&Value->WillPayload) || !xmqtt5ReadPayload(Reader,Value->WillPayload,Value->WillProperties,Start+2u)) return false;
	}
	if (Value->HasUsername && !xmqtt5ReadField(Reader,true,&Value->Username)) return false;
	if (Value->HasPassword && !xmqtt5ReadField(Reader,false,&Value->Password)) return false;
	return true;
}

static bool xmqtt5ReadPublish(xmqtt5reader* Reader)
{
	xmqttpublishpacket* Value=&Reader->Value.Body.Publish; size_t Start=Reader->Position;
	if (!xmqtt5ReadField(Reader,true,&Value->Topic)) return false;
	if (Value->Topic.Size && !xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"PUBLISH Topic Name contains wildcards");
	if ((Reader->Value.Flags&XMQTT_PUBLISH_QOS_MASK) && !xmqtt5ReadIdentifier(Reader)) return false;
	if (!xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_PUBLISH,&Reader->Value.Properties)) return false;
	if (!Value->Topic.Size) {
		bool Alias=false;
		for (size_t Index=0; Index<Reader->Value.Properties.Count; ++Index) if (Reader->Value.Properties.Data[Index].Id==XMQTT_PROPERTY_TOPIC_ALIAS) Alias=true;
		if (!Alias) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"empty PUBLISH topic has no Topic Alias");
	}
	Value->Payload=(xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position};
	if (!xmqtt5ReadPayload(Reader,Value->Payload,Reader->Value.Properties,Reader->Position)) return false;
	Reader->Position=Reader->End; return true;
}

static bool xmqtt5ReadTopics(xmqtt5reader* Reader)
{
	if (!xmqtt5ReadIdentifier(Reader) || !xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Reader->Value.Type,&Reader->Value.Properties)) return false;
	size_t Count=0;
	while (Reader->Position<Reader->End) {
		if (Count>=Reader->Config.MaxTopics) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT topic count exceeds configured limit");
		if (Count>=Reader->Buffers.TopicCapacity) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_CAPACITY,Reader->Position,"MQTT topic scratch capacity exhausted");
		xmqttsubscription Value={0}; bool Shared; size_t Start=Reader->Position;
		if (!xmqtt5ReadField(Reader,true,&Value.Filter)) return false;
		if (!xmqttInternalTopic(Value.Filter,true,true,&Shared)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Topic Filter");
		if (Reader->Value.Type==XMQTT_PACKET_SUBSCRIBE) {
			Start=Reader->Position; if (!xmqtt5ReadByte(Reader,&Value.Options)) return false;
			if ((Value.Options&0xc0u) || (Value.Options&3u)==3u || (Value.Options&0x30u)==0x30u)
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT subscription option bits");
			if (Shared && (Value.Options&XMQTT_SUBSCRIBE_NO_LOCAL)) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"No Local is forbidden on shared subscriptions");
		}
		Reader->Buffers.Topics[Count++]=Value;
	}
	if (!Count) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT topic request has an empty payload");
	Reader->Value.Body.Topics=(xmqttsubscriptionlist){Reader->Buffers.Topics,Count}; return true;
}

static bool xmqtt5ReadReason(xmqtt5reader* Reader)
{
	size_t Start=Reader->Position;
	if (!xmqtt5ReadByte(Reader,&Reader->Value.Reason)) return false;
	if (!xmqttInternalReason(XMQTT_VERSION_5,Reader->Value.Type,Reader->Value.Reason,Reader->Direction))
		return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"reason code is not valid for MQTT packet/direction");
	return true;
}

static bool xmqtt5ReadBody(xmqtt5reader* Reader)
{
	xmqttpackettype Type=Reader->Value.Type;
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt5ReadConnect(Reader);
	if (Type==XMQTT_PACKET_PUBLISH) return xmqtt5ReadPublish(Reader);
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) return xmqtt5ReadTopics(Reader);
	if (Type==XMQTT_PACKET_CONNACK) {
		uint8 Flags; size_t Start=Reader->Position;
		if (!xmqtt5ReadByte(Reader,&Flags)) return false;
		if (Flags&0xfeu) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"reserved CONNACK bits are nonzero");
		Reader->Value.SessionPresent=Flags!=0;
		if (!xmqtt5ReadReason(Reader)) return false;
		if (Reader->Value.Reason && Flags) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"failed CONNACK has Session Present");
		return xmqtt5ReadProperties(Reader,XMQTT_PROPERTIES_CONNACK,&Reader->Value.Properties);
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP) return true;
	if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		if (!xmqtt5ReadIdentifier(Reader) || !xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties)) return false;
		size_t Count=Reader->End-Reader->Position;
		if (!Count) return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT acknowledgement has no reason list");
		if (Count>Reader->Config.MaxTopics) return xmqtt5ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT acknowledgement reason count exceeds limit");
		for (size_t Index=0; Index<Count; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_5,Type,Reader->Input.Data[Reader->Position+Index],Reader->Direction))
				return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position+Index,"invalid MQTT acknowledgement reason");
		Reader->Value.Body.Reasons=(xbytesview){Reader->Input.Data+Reader->Position,Count}; Reader->Position=Reader->End; return true;
	}
	if (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) {
		if (!xmqtt5ReadIdentifier(Reader)) return false;
		if (Reader->Position<Reader->End && !xmqtt5ReadReason(Reader)) return false;
		return Reader->Position==Reader->End || xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties);
	}
	/* DISCONNECT and AUTH have no identifier. AUTH reason-only is not a defined
	 * abbreviation; success with no body is explicitly permitted by the spec. */
	if (Reader->Position==Reader->End) {
		if (!xmqttInternalReason(XMQTT_VERSION_5,Type,0,Reader->Direction))
			return xmqtt5ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"implicit MQTT reason is invalid for sender");
		return true;
	}
	if (!xmqtt5ReadReason(Reader)) return false;
	if (Type==XMQTT_PACKET_DISCONNECT && Reader->Position==Reader->End) return true;
	return xmqtt5ReadProperties(Reader,(xmqttpropertyscope)Type,&Reader->Value.Properties);
}

XMQTT_API xmqttcodecstatus xmqttPacketRead5(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers, xmqttpacket* Packet, xmqttcodecissue* Issue)
{
	xmqtt5reader Reader={0}; Reader.Input=Input; Reader.Direction=Direction;
	if (!xmqttInternalCodecSettings(Direction,Config,&Reader.Config) || !xmqtt5ReadArgs(Input,Config,Buffers,Packet,Issue,&Reader.Buffers)) return XMQTT_CODEC_LOCAL_ERROR;
	xmqttwireconfig Wire={sizeof(Wire),XMQTT_VERSION_5,Reader.Config.MaxPacketBytes}; xmqttfixedheader Header; xmqttwireerror Error=0;
	xmqttwirestatus Status=xmqttInternalHeader(Input,&Wire,&Header,&Error);
	if (Status==XMQTT_WIRE_MORE) return XMQTT_CODEC_MORE;
	if (Status==XMQTT_WIRE_ERROR) { xmqtt5ReadMark(&Reader,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LOCAL_ERROR : XMQTT_CODEC_MALFORMED,
		Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LIMIT : XMQTT_CODEC_FORMAT,0); goto Failed; }
	Reader.Value.Type=Header.Type; Reader.Value.Flags=Header.Flags;
	if (!xmqttInternalRole(Header.Type,Direction)) { xmqtt5ReadFail(&Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,0,"MQTT packet travels in forbidden direction"); goto Failed; }
	if ((Header.Type==XMQTT_PACKET_PINGREQ || Header.Type==XMQTT_PACKET_PINGRESP) && Header.RemainingLength) {
		xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"MQTT PING packet has a body"); goto Failed;
	}
	if (Header.Type==XMQTT_PACKET_AUTH && Header.RemainingLength==1) {
		xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"AUTH lacks its property section"); goto Failed;
	}
	if (Input.Size<Header.PacketBytes) return XMQTT_CODEC_MORE;
	Reader.Position=Header.HeaderBytes; Reader.End=Header.PacketBytes;
	if (!xmqtt5ReadBody(&Reader)) goto Failed;
	if (Reader.Position!=Reader.End) { xmqtt5ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader.Position,"unexpected trailing MQTT packet body bytes"); goto Failed; }
	Reader.Value.Wire=(xbytesview){Input.Data,Header.PacketBytes}; *Packet=Reader.Value;
	if (Issue!=NULL) *Issue=(xmqttcodecissue){XMQTT_CODEC_DONE,XMQTT_CODEC_OK,XRT_NPOS,0,0,0};
	return XMQTT_CODEC_DONE;
Failed:
	if (Issue!=NULL) *Issue=Reader.Failure;
	return Reader.Failure.Status;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/codec5/encode.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC5)

typedef struct xmqtt5plan {
	const xmqttpacket* Packet;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	size_t Body, Total, PropertyBytes, PropertyCount;
	size_t RootSize, WillSize;
	uint32 RootBody, WillBody;
	uint8 ConnectFlags;
} xmqtt5plan;

static bool xmqtt5WriteFail(xerrkind Kind, xmqttcodecerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind,"xrt.mqtt",Code,Message); return false;
}

static bool xmqtt5WriteSpan(const void* Data, size_t Size)
{
	return ((Data!=NULL || !Size) && xrtMemRangeValid(Data,Size)) ||
		xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT descriptor data span");
}

static size_t xmqtt5WriteVbiSize(size_t Value)
{
	return 1u+(Value>=128u)+(Value>=16384u)+(Value>=2097152u);
}

static bool xmqtt5WriteAdd(xmqtt5plan* Plan, size_t Size)
{
	if (Size>XMQTT_VARIABLE_MAX-Plan->Body) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT body exceeds protocol length");
	Plan->Body+=Size;
	if (Plan->Body+1u+xmqtt5WriteVbiSize(Plan->Body)>Plan->Config.MaxPacketBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT packet exceeds configured limit");
	return true;
}

static bool xmqtt5WriteField(xmqtt5plan* Plan, xbytesview Value, bool Text)
{
	if (!xmqtt5WriteSpan(Value.Data,Value.Size)) return false;
	if (Value.Size>XMQTT_STRING_MAX_BYTES) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT field exceeds two-byte length");
	if (!xmqtt5WriteAdd(Plan,2u+Value.Size)) return false;
	return !Text || xmqttUtf8StringCheck((xstrview){(cstr)Value.Data,Value.Size},Plan->Config.Utf8Flags,NULL);
}

static bool xmqtt5WriteText(xmqtt5plan* Plan, xstrview Value)
{
	return xmqtt5WriteField(Plan,(xbytesview){(cbytes)Value.Data,Value.Size},true);
}

static bool xmqtt5WriteProperties(xmqtt5plan* Plan, xmqttpropertylist Values, xmqttpropertyscope Scope, size_t* Size, uint32* Body)
{
	if (Plan->PropertyBytes==Plan->Config.MaxPropertyBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT aggregate property byte limit exhausted");
	xmqttpropertyconfig Config={sizeof(Config),Plan->Config.MaxPropertyBytes-Plan->PropertyBytes,
		Plan->Config.MaxProperties-Plan->PropertyCount,Plan->Config.Utf8Flags}, Limits;
	/* At least two fixed-header bytes are needed. Bound the property preflight
	 * by the remaining packet budget before it scans any property text. The
	 * final Add also accounts for a longer Remaining Length integer. */
	size_t Available=Plan->Config.MaxPacketBytes-2u-Plan->Body;
	if (Available>XMQTT_VARIABLE_MAX-Plan->Body) Available=XMQTT_VARIABLE_MAX-Plan->Body;
	if (Config.MaxBytes>Available) Config.MaxBytes=Available;
	if (!Config.MaxBytes) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"no MQTT packet budget remains for a property section");
	if (!xmqttInternalPropertiesPrepare(Values.Data,Values.Count,Scope,Plan->Direction,&Config,&Limits,Size,Body)) return false;
	Plan->PropertyBytes+=*Size; Plan->PropertyCount+=Values.Count; return xmqtt5WriteAdd(Plan,*Size);
}

static bool xmqtt5WritePayload(xbytesview Value, xmqttpropertylist Properties)
{
	for (size_t Index=0; Index<Properties.Count; ++Index)
		if (Properties.Data[Index].Id==XMQTT_PROPERTY_PAYLOAD_FORMAT && Properties.Data[Index].Value.Number==1) {
			if (!xrtUtf8Valid((xstrview){(cstr)Value.Data,Value.Size},NULL))
				return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PAYLOAD_FORMAT,"MQTT indicated payload format is not well-formed UTF-8");
			break;
		}
	return true;
}

static bool xmqtt5WriteConnect(xmqtt5plan* Plan)
{
	const xmqttconnectpacket* Value=&Plan->Packet->Body.Connect;
	if (Value->WillQos>2 || (!Value->HasWill && (Value->WillRetain || Value->WillQos || Value->WillProperties.Count || Value->WillTopic.Size || Value->WillPayload.Size)) ||
		(!Value->HasUsername && Value->Username.Size) || (!Value->HasPassword && Value->Password.Size))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"inconsistent MQTT CONNECT descriptor flags/fields");
	Plan->ConnectFlags=(uint8)((Value->CleanStart ? 2u : 0u)|(Value->HasWill ? 4u : 0u)|(Value->WillQos<<3)|
		(Value->WillRetain ? 32u : 0u)|(Value->HasUsername ? 128u : 0u)|(Value->HasPassword ? 64u : 0u));
	if (!xmqtt5WriteAdd(Plan,10) || !xmqtt5WriteProperties(Plan,Plan->Packet->Properties,XMQTT_PROPERTIES_CONNECT,&Plan->RootSize,&Plan->RootBody) ||
		!xmqtt5WriteText(Plan,Value->ClientId)) return false;
	if (Value->HasWill) {
		if (Value->WillPayload.Size>Plan->Config.MaxPayloadBytes)
			return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT Will payload exceeds configured limit");
		if (!xmqtt5WriteProperties(Plan,Value->WillProperties,XMQTT_PROPERTIES_WILL,&Plan->WillSize,&Plan->WillBody) ||
			!xmqtt5WriteText(Plan,Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Will Topic");
		if (!xmqtt5WriteField(Plan,Value->WillPayload,false)) return false;
	}
	if (Value->HasUsername && !xmqtt5WriteText(Plan,Value->Username)) return false;
	if (Value->HasPassword && !xmqtt5WriteField(Plan,Value->Password,false)) return false;
	return !Value->HasWill || xmqtt5WritePayload(Value->WillPayload,Value->WillProperties);
}

static bool xmqtt5WritePublish(xmqtt5plan* Plan)
{
	const xmqttpublishpacket* Value=&Plan->Packet->Body.Publish;
	if (!xmqtt5WriteSpan(Value->Payload.Data,Value->Payload.Size)) return false;
	if (Value->Payload.Size>Plan->Config.MaxPayloadBytes)
		return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT PUBLISH payload exceeds configured limit");
	if (!xmqtt5WriteText(Plan,Value->Topic)) return false;
	if (Value->Topic.Size && !xmqttInternalTopic(Value->Topic,false,false,NULL))
		return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"PUBLISH Topic Name contains wildcards");
	if (Plan->Packet->Identifier && !xmqtt5WriteAdd(Plan,2)) return false;
	if (!xmqtt5WriteProperties(Plan,Plan->Packet->Properties,XMQTT_PROPERTIES_PUBLISH,&Plan->RootSize,&Plan->RootBody)) return false;
	if (!Value->Topic.Size) {
		bool Alias=false;
		for (size_t Index=0; Index<Plan->Packet->Properties.Count; ++Index) if (Plan->Packet->Properties.Data[Index].Id==XMQTT_PROPERTY_TOPIC_ALIAS) Alias=true;
		if (!Alias) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"empty PUBLISH topic has no Topic Alias");
	}
	return xmqtt5WriteAdd(Plan,Value->Payload.Size) && xmqtt5WritePayload(Value->Payload,Plan->Packet->Properties);
}

static bool xmqtt5WriteTopics(xmqtt5plan* Plan)
{
	xmqttsubscriptionlist Values=Plan->Packet->Body.Topics;
	if (!Values.Count) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT topic request has an empty payload");
	if (Values.Count>Plan->Config.MaxTopics) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT topic count exceeds configured limit");
	if (Values.Count>SIZE_MAX/sizeof(*Values.Data)) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT topic vector size overflows");
	if (!xmqtt5WriteSpan(Values.Data,Values.Count*sizeof(*Values.Data))) return false;
	if (!xmqtt5WriteAdd(Plan,2) || !xmqtt5WriteProperties(Plan,Plan->Packet->Properties,(xmqttpropertyscope)Plan->Packet->Type,&Plan->RootSize,&Plan->RootBody)) return false;
	for (size_t Index=0; Index<Values.Count; ++Index) {
		const xmqttsubscription* Value=&Values.Data[Index]; bool Shared;
		if (!xmqtt5WriteText(Plan,Value->Filter)) return false;
		if (!xmqttInternalTopic(Value->Filter,true,true,&Shared)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Topic Filter");
		if (Plan->Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
			if (Value->Options) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"UNSUBSCRIBE descriptor has subscription options");
		} else {
			if ((Value->Options&0xc0u) || (Value->Options&3u)==3u || (Value->Options&0x30u)==0x30u || (Shared && (Value->Options&XMQTT_SUBSCRIBE_NO_LOCAL)))
				return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT subscription options");
			if (!xmqtt5WriteAdd(Plan,1)) return false;
		}
	}
	return true;
}

static bool xmqtt5WritePrepare(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqtt5plan* Plan)
{
	if (Packet==NULL || !xmqtt5WriteSpan(Packet,sizeof(*Packet)) || !xmqttInternalCodecSettings(Direction,Config,&Plan->Config))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT packet/config descriptor");
	Plan->Packet=Packet; Plan->Direction=Direction;
	xmqttpackettype Type=Packet->Type;
	if (!xmqttInternalRole(Type,Direction) || !xmqttInternalFlags(Type,Packet->Flags,XMQTT_VERSION_5))
		return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT descriptor type/direction/fixed flags");
	bool Identifier=(Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_UNSUBACK) || (Type==XMQTT_PACKET_PUBLISH && (Packet->Flags&XMQTT_PUBLISH_QOS_MASK));
	bool Reason=Type==XMQTT_PACKET_CONNACK || (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) || Type>=XMQTT_PACKET_DISCONNECT;
	if (Identifier!=(Packet->Identifier!=0) || (Reason ? !xmqttInternalReason(XMQTT_VERSION_5,Type,Packet->Reason,Direction) : Packet->Reason!=0) ||
		(Packet->SessionPresent && (Type!=XMQTT_PACKET_CONNACK || Packet->Reason)))
		return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"inconsistent MQTT identifier/reason/session descriptor");
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt5WriteConnect(Plan);
	if (Type==XMQTT_PACKET_PUBLISH) return xmqtt5WritePublish(Plan);
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) return xmqtt5WriteTopics(Plan);
	if (Type==XMQTT_PACKET_CONNACK) return xmqtt5WriteAdd(Plan,2) &&
		xmqtt5WriteProperties(Plan,Packet->Properties,XMQTT_PROPERTIES_CONNACK,&Plan->RootSize,&Plan->RootBody);
	if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		xbytesview Reasons=Packet->Body.Reasons;
		if (!Reasons.Size) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT acknowledgement reason list is empty");
		if (Reasons.Size>Plan->Config.MaxTopics) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT acknowledgement reason count exceeds limit");
		if (!xmqtt5WriteSpan(Reasons.Data,Reasons.Size) || !xmqtt5WriteAdd(Plan,2) ||
			!xmqtt5WriteProperties(Plan,Packet->Properties,(xmqttpropertyscope)Type,&Plan->RootSize,&Plan->RootBody) || !xmqtt5WriteAdd(Plan,Reasons.Size)) return false;
		for (size_t Index=0; Index<Reasons.Size; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_5,Type,Reasons.Data[Index],Direction)) return xmqtt5WriteFail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT acknowledgement reason");
		return true;
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP) {
		if (Packet->Properties.Count) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"PING descriptor has properties");
		return true;
	}
	if (Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_PUBCOMP) {
		if (!xmqtt5WriteAdd(Plan,2)) return false;
		if (!Packet->Reason && !Packet->Properties.Count) return true;
	} else if (!Packet->Reason && !Packet->Properties.Count) return true;
	if (!xmqtt5WriteAdd(Plan,1)) return false;
	if (!Packet->Properties.Count && Type!=XMQTT_PACKET_AUTH) return true;
	return xmqtt5WriteProperties(Plan,Packet->Properties,(xmqttpropertyscope)Type,&Plan->RootSize,&Plan->RootBody);
}

static bool xmqtt5WriteOverlap(const void* Output, size_t Size, const void* Data, size_t Bytes)
{
	return !xrtMemRangesOverlap(Output,Size,Data,Bytes) || xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT output overlaps descriptor inputs");
}

static bool xmqtt5WriteOutput(const xmqtt5plan* Plan, const void* Output, size_t Size, const xmqttcodecconfig* Config)
{
	const xmqttpacket* Packet=Plan->Packet;
	if (Output==NULL) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT codec output");
	if (!xmqtt5WriteSpan(Output,Size) || !xmqtt5WriteOverlap(Output,Size,Packet,sizeof(*Packet)) ||
		(Config!=NULL && !xmqtt5WriteOverlap(Output,Size,Config,sizeof(*Config)))) return false;
	if (Plan->RootSize && !xmqttInternalPropertiesOutput(Output,Size,Packet->Properties.Data,Packet->Properties.Count,NULL)) return false;
	if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (!xmqtt5WriteOverlap(Output,Size,Value->ClientId.Data,Value->ClientId.Size)) return false;
		if (Value->HasWill && (!xmqttInternalPropertiesOutput(Output,Size,Value->WillProperties.Data,Value->WillProperties.Count,NULL) ||
			!xmqtt5WriteOverlap(Output,Size,Value->WillTopic.Data,Value->WillTopic.Size) || !xmqtt5WriteOverlap(Output,Size,Value->WillPayload.Data,Value->WillPayload.Size))) return false;
		if (Value->HasUsername && !xmqtt5WriteOverlap(Output,Size,Value->Username.Data,Value->Username.Size)) return false;
		if (Value->HasPassword && !xmqtt5WriteOverlap(Output,Size,Value->Password.Data,Value->Password.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		if (!xmqtt5WriteOverlap(Output,Size,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size) ||
			!xmqtt5WriteOverlap(Output,Size,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!xmqtt5WriteOverlap(Output,Size,Values.Data,Values.Count*sizeof(*Values.Data))) return false;
		for (size_t Index=0; Index<Values.Count; ++Index)
			if (!xmqtt5WriteOverlap(Output,Size,Values.Data[Index].Filter.Data,Values.Data[Index].Filter.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBACK || Packet->Type==XMQTT_PACKET_UNSUBACK) {
		if (!xmqtt5WriteOverlap(Output,Size,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size)) return false;
	}
	return true;
}

static unsigned char* xmqtt5EmitWord(unsigned char* Output, uint16 Value)
{
	*Output++=(unsigned char)(Value>>8); *Output++=(unsigned char)Value; return Output;
}

static unsigned char* xmqtt5EmitData(unsigned char* Output, const void* Data, size_t Size)
{
	if (Size) memcpy(Output,Data,Size);
	return Output+Size;
}

static unsigned char* xmqtt5EmitField(unsigned char* Output, const void* Data, size_t Size)
{
	return xmqtt5EmitData(xmqtt5EmitWord(Output,(uint16)Size),Data,Size);
}

static unsigned char* xmqtt5EmitProperties(unsigned char* Output, xmqttpropertylist Values, uint32 Body, size_t Size)
{
	return Output+xmqttInternalPropertiesEmit(Output,Values.Data,Values.Count,Body,Size);
}

static void xmqtt5Emit(const xmqtt5plan* Plan, ptr Output)
{
	const xmqttpacket* Packet=Plan->Packet; xmqttpackettype Type=Packet->Type; unsigned char* Cursor=(unsigned char*)Output; size_t Bytes;
	*Cursor++=(uint8)((Type<<4)|Packet->Flags); (void)xmqttVbiWrite(Cursor,Plan->Total-1u,(uint32)Plan->Body,&Bytes); Cursor+=Bytes;
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		Cursor=xmqtt5EmitData(Cursor,"\x00\x04MQTT\x05",7); *Cursor++=Plan->ConnectFlags; Cursor=xmqtt5EmitWord(Cursor,Value->KeepAlive);
		Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		Cursor=xmqtt5EmitField(Cursor,Value->ClientId.Data,Value->ClientId.Size);
		if (Value->HasWill) {
			Cursor=xmqtt5EmitProperties(Cursor,Value->WillProperties,Plan->WillBody,Plan->WillSize);
			Cursor=xmqtt5EmitField(Cursor,Value->WillTopic.Data,Value->WillTopic.Size); Cursor=xmqtt5EmitField(Cursor,Value->WillPayload.Data,Value->WillPayload.Size);
		}
		if (Value->HasUsername) Cursor=xmqtt5EmitField(Cursor,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) Cursor=xmqtt5EmitField(Cursor,Value->Password.Data,Value->Password.Size);
	} else if (Type==XMQTT_PACKET_PUBLISH) {
		Cursor=xmqtt5EmitField(Cursor,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size);
		if (Packet->Identifier) Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier);
		Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		(void)xmqtt5EmitData(Cursor,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier); Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) {
			const xmqttsubscription* Value=&Packet->Body.Topics.Data[Index]; Cursor=xmqtt5EmitField(Cursor,Value->Filter.Data,Value->Filter.Size);
			if (Type==XMQTT_PACKET_SUBSCRIBE) *Cursor++=Value->Options;
		}
	} else if (Type==XMQTT_PACKET_CONNACK) {
		*Cursor++=Packet->SessionPresent ? 1u : 0u; *Cursor++=Packet->Reason;
		(void)xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
	} else if (Type==XMQTT_PACKET_SUBACK || Type==XMQTT_PACKET_UNSUBACK) {
		Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier); Cursor=xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
		(void)xmqtt5EmitData(Cursor,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
	} else if (Type!=XMQTT_PACKET_PINGREQ && Type!=XMQTT_PACKET_PINGRESP) {
		if (Packet->Identifier) Cursor=xmqtt5EmitWord(Cursor,Packet->Identifier);
		if (Plan->RootSize || Packet->Reason) *Cursor++=Packet->Reason;
		if (Plan->RootSize) (void)xmqtt5EmitProperties(Cursor,Packet->Properties,Plan->RootBody,Plan->RootSize);
	}
}

XMQTT_API bool xmqttPacketSize5(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Required)
{
	xmqtt5plan Plan={0};
	if (!xmqtt5WritePrepare(Packet,Direction,Config,&Plan) || !xmqtt5WriteOutput(&Plan,Required,sizeof(*Required),Config)) return false;
	*Required=Plan.Body+1u+xmqtt5WriteVbiSize(Plan.Body); return true;
}

XMQTT_API bool xmqttPacketWrite5(ptr Output, size_t Capacity, const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Written)
{
	xmqtt5plan Plan={0};
	if (Output==NULL || !xmqtt5WriteSpan(Output,Capacity)) return xmqtt5WriteFail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT packet wire buffer");
	if (!xmqtt5WritePrepare(Packet,Direction,Config,&Plan)) return false;
	Plan.Total=Plan.Body+1u+xmqtt5WriteVbiSize(Plan.Body);
	if (Capacity<Plan.Total) return xmqtt5WriteFail(XERR_RANGE,XMQTT_CODEC_CAPACITY,"MQTT packet wire capacity too small");
	if (!xmqtt5WriteOutput(&Plan,Output,Plan.Total,Config) || (Written!=NULL &&
		(!xmqtt5WriteOutput(&Plan,Written,sizeof(*Written),Config) || !xmqtt5WriteOverlap(Written,sizeof(*Written),Output,Plan.Total)))) return false;
	xmqtt5Emit(&Plan,Output); if (Written!=NULL) *Written=Plan.Total; return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/codec311/codec311.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
#include <string.h>

#if defined(XMQTT_FEATURE_XMQTT_CODEC311)

/* Separate static protocol paths keep the 3.1.1 leaf independent of properties
 * and avoid a version switch in each field. Wire/Unicode and packet grammar
 * come from the same shared leaves as the 5.0 codec. */
static bool xmqtt311Fail(xerrkind Kind, xmqttcodecerror Error, cstr Message)
{
	xrtSetErrorInfo(Kind,"xrt.mqtt",Error,Message); return false;
}

static bool xmqtt311Span(const void* Data, size_t Size)
{
	return ((Data!=NULL || !Size) && xrtMemRangeValid(Data,Size)) || xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 data span");
}

static bool xmqtt311Role(xmqttpackettype Type, xmqttdirection Direction)
{
	return Type!=XMQTT_PACKET_AUTH && xmqttInternalRole(Type,Direction) && (Type!=XMQTT_PACKET_DISCONNECT || Direction==XMQTT_CLIENT_TO_SERVER);
}

typedef struct xmqtt311reader {
	xbytesview Input;
	size_t Position, End;
	xmqttdirection Direction;
	xmqttcodecconfig Config;
	xmqttcodecbuffers Buffers;
	xmqttpacket Value;
	xmqttcodecissue Failure;
} xmqtt311reader;

static bool xmqtt311Mark(xmqtt311reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset)
{
	Reader->Failure=(xmqttcodecissue){Status,Error,Offset,Reader->Value.Type,Reader->Value.Flags,Reader->Value.Identifier}; return false;
}

static bool xmqtt311ReadFail(xmqtt311reader* Reader, xmqttcodecstatus Status, xmqttcodecerror Error, size_t Offset, cstr Message)
{
	xmqtt311Mark(Reader,Status,Error,Offset); return xmqtt311Fail(Status==XMQTT_CODEC_LOCAL_ERROR ? XERR_RANGE : XERR_PROTOCOL,Error,Message);
}

static bool xmqtt311ReadArgs(xbytesview Input, const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers,
	xmqttpacket* Packet, xmqttcodecissue* Issue, xmqttcodecbuffers* Copy)
{
	if (!xmqtt311Span(Input.Data,Input.Size) || Packet==NULL || !xrtMemRangeValid(Packet,sizeof(*Packet)) ||
		(Issue!=NULL && !xrtMemRangeValid(Issue,sizeof(*Issue))) || (Buffers!=NULL && !xrtMemRangeValid(Buffers,sizeof(*Buffers)))) goto Invalid;
	*Copy=Buffers==NULL ? (xmqttcodecbuffers){0} : *Buffers;
	if (Copy->TopicCapacity>SIZE_MAX/sizeof(xmqttsubscription) || (Copy->Topics==NULL && Copy->TopicCapacity) ||
		!xrtMemRangeValid(Copy->Topics,Copy->TopicCapacity*sizeof(xmqttsubscription))) goto Invalid;
	const void* Outputs[]={Packet,Issue,Copy->Topics};
	size_t Sizes[]={sizeof(*Packet),Issue==NULL ? 0 : sizeof(*Issue),Copy->TopicCapacity*sizeof(xmqttsubscription)};
	for (size_t Index=0; Index<3; ++Index) {
		if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Input.Data,Input.Size) ||
			(Config!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Config,sizeof(*Config))) ||
			(Buffers!=NULL && xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Buffers,sizeof(*Buffers)))) goto Invalid;
		for (size_t Other=0; Other<Index; ++Other) if (xrtMemRangesOverlap(Outputs[Index],Sizes[Index],Outputs[Other],Sizes[Other])) goto Invalid;
	}
	return true;
Invalid:
	return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 read outputs or aliases");
}

static bool xmqtt311ReadByte(xmqtt311reader* Reader, uint8* Value)
{
	if (Reader->Position==Reader->End) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT byte");
	*Value=Reader->Input.Data[Reader->Position++]; return true;
}

static bool xmqtt311ReadWord(xmqtt311reader* Reader, uint16* Value)
{
	if (Reader->End-Reader->Position<2) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"truncated MQTT two-byte integer");
	*Value=(uint16)(((uint16)Reader->Input.Data[Reader->Position]<<8)|Reader->Input.Data[Reader->Position+1u]); Reader->Position+=2; return true;
}

static bool xmqtt311ReadIdentifier(xmqtt311reader* Reader)
{
	uint16 Value; size_t Start=Reader->Position;
	if (!xmqtt311ReadWord(Reader,&Value)) return false;
	if (!Value) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"MQTT packet identifier is zero");
	Reader->Value.Identifier=Value; return true;
}

static bool xmqtt311ReadField(xmqtt311reader* Reader, bool Text, ptr Value)
{
	xbytesview Input={Reader->Input.Data+Reader->Position,Reader->End-Reader->Position}; size_t Bytes;
	xmqttwirestatus Status=Text ? xmqttUtf8StringRead(Input,Reader->Config.Utf8Flags,(xstrview*)Value,&Bytes) : xmqttBinaryRead(Input,(xbytesview*)Value,&Bytes);
	if (Status==XMQTT_WIRE_MORE) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT field crosses packet boundary");
	if (Status==XMQTT_WIRE_ERROR) return xmqtt311Mark(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position);
	Reader->Position+=Bytes; return true;
}

static bool xmqtt311ReadConnect(xmqtt311reader* Reader)
{
	xmqttconnectpacket* Value=&Reader->Value.Body.Connect; xstrview Name; uint8 Version, Flags; size_t Start=Reader->Position;
	if (!xmqtt311ReadField(Reader,true,&Name)) return false;
	if (Name.Size!=4 || memcmp(Name.Data,"MQTT",4)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"unsupported MQTT protocol name");
	Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Version)) return false;
	if (Version!=4) return xmqtt311ReadFail(Reader,XMQTT_CODEC_UNSUPPORTED,XMQTT_CODEC_VERSION,Start,"CONNECT is not MQTT 3.1.1");
	Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Flags)) return false;
	if ((Flags&1u) || (Flags&0x18u)==0x18u || (!(Flags&4u) && (Flags&0x38u)) || ((Flags&0xc0u)==0x40u))
		return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT 3.1.1 CONNECT flags");
	Value->CleanStart=(Flags&2u)!=0; Value->HasWill=(Flags&4u)!=0; Value->WillQos=(Flags>>3)&3u; Value->WillRetain=(Flags&32u)!=0;
	Value->HasUsername=(Flags&128u)!=0; Value->HasPassword=(Flags&64u)!=0;
	if (!xmqtt311ReadWord(Reader,&Value->KeepAlive)) return false;
	Start=Reader->Position; if (!xmqtt311ReadField(Reader,true,&Value->ClientId)) return false;
	if (!Value->ClientId.Size && !Value->CleanStart) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_CLIENT_ID,Start,"empty MQTT 3.1.1 ClientId requires Clean Session");
	if (Value->HasWill) {
		Start=Reader->Position; if (!xmqtt311ReadField(Reader,true,&Value->WillTopic)) return false;
		if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Will Topic");
		Start=Reader->Position; if (!xmqtt311ReadField(Reader,false,&Value->WillPayload)) return false;
		if (Value->WillPayload.Size>Reader->Config.MaxPayloadBytes) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Start,"MQTT Will payload exceeds limit");
	}
	if (Value->HasUsername && !xmqtt311ReadField(Reader,true,&Value->Username)) return false;
	if (Value->HasPassword && !xmqtt311ReadField(Reader,false,&Value->Password)) return false;
	return true;
}

static bool xmqtt311ReadBody(xmqtt311reader* Reader)
{
	xmqttpackettype Type=Reader->Value.Type;
	if (Type==XMQTT_PACKET_CONNECT) return xmqtt311ReadConnect(Reader);
	if (Type==XMQTT_PACKET_PUBLISH) {
		xmqttpublishpacket* Value=&Reader->Value.Body.Publish; size_t Start=Reader->Position;
		if (!xmqtt311ReadField(Reader,true,&Value->Topic)) return false;
		if (!xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT PUBLISH Topic Name");
		if ((Reader->Value.Flags&XMQTT_PUBLISH_QOS_MASK) && !xmqtt311ReadIdentifier(Reader)) return false;
		Value->Payload=(xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position};
		if (Value->Payload.Size>Reader->Config.MaxPayloadBytes) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT PUBLISH payload exceeds limit");
		Reader->Position=Reader->End; return true;
	}
	if (Type==XMQTT_PACKET_CONNACK) {
		uint8 Flags; size_t Start=Reader->Position;
		if (!xmqtt311ReadByte(Reader,&Flags) || !xmqtt311ReadByte(Reader,&Reader->Value.Reason)) return false;
		if (Flags&0xfeu) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"reserved CONNACK flags are nonzero");
		if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Reader->Value.Reason,Reader->Direction) || (Flags && Reader->Value.Reason))
			return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Start,"invalid MQTT 3.1.1 CONNACK code/session flag");
		Reader->Value.SessionPresent=Flags!=0; return true;
	}
	if (Type==XMQTT_PACKET_PINGREQ || Type==XMQTT_PACKET_PINGRESP || Type==XMQTT_PACKET_DISCONNECT) return true;
	if (!xmqtt311ReadIdentifier(Reader)) return false;
	if (Type==XMQTT_PACKET_SUBACK) {
		size_t Count=Reader->End-Reader->Position;
		if (!Count) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"SUBACK return-code list is empty");
		if (Count>Reader->Config.MaxTopics) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"SUBACK return-code count exceeds limit");
		for (size_t Index=0; Index<Count; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Reader->Input.Data[Reader->Position+Index],Reader->Direction))
				return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position+Index,"invalid SUBACK return code");
		Reader->Value.Body.Reasons=(xbytesview){Reader->Input.Data+Reader->Position,Count}; Reader->Position=Reader->End; return true;
	}
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		size_t Count=0;
		while (Reader->Position<Reader->End) {
			if (Count>=Reader->Config.MaxTopics) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_LIMIT,Reader->Position,"MQTT topic count exceeds limit");
			if (Count>=Reader->Buffers.TopicCapacity) return xmqtt311ReadFail(Reader,XMQTT_CODEC_LOCAL_ERROR,XMQTT_CODEC_CAPACITY,Reader->Position,"MQTT topic scratch exhausted");
			xmqttsubscription Value={0}; size_t Start=Reader->Position;
			if (!xmqtt311ReadField(Reader,true,&Value.Filter)) return false;
			if (!xmqttInternalTopic(Value.Filter,true,false,NULL)) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_TOPIC,Start,"invalid MQTT Topic Filter");
			if (Type==XMQTT_PACKET_SUBSCRIBE) {
				Start=Reader->Position; if (!xmqtt311ReadByte(Reader,&Value.Options)) return false;
				if (Value.Options>2) return xmqtt311ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Start,"invalid MQTT 3.1.1 subscription QoS byte");
			}
			Reader->Buffers.Topics[Count++]=Value;
		}
		if (!Count) return xmqtt311ReadFail(Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,Reader->Position,"MQTT topic request is empty");
		Reader->Value.Body.Topics=(xmqttsubscriptionlist){Reader->Buffers.Topics,Count};
	}
	return true;
}

XMQTT_API xmqttcodecstatus xmqttPacketRead311(xbytesview Input, xmqttdirection Direction,
	const xmqttcodecconfig* Config, const xmqttcodecbuffers* Buffers, xmqttpacket* Packet, xmqttcodecissue* Issue)
{
	xmqtt311reader Reader={0}; Reader.Input=Input; Reader.Direction=Direction;
	if (!xmqttInternalCodecSettings(Direction,Config,&Reader.Config) || !xmqtt311ReadArgs(Input,Config,Buffers,Packet,Issue,&Reader.Buffers)) return XMQTT_CODEC_LOCAL_ERROR;
	xmqttwireconfig Wire={sizeof(Wire),XMQTT_VERSION_311,Reader.Config.MaxPacketBytes}; xmqttfixedheader Header; xmqttwireerror Error=0;
	xmqttwirestatus Status=xmqttInternalHeader(Input,&Wire,&Header,&Error);
	if (Status==XMQTT_WIRE_MORE) return XMQTT_CODEC_MORE;
	if (Status==XMQTT_WIRE_ERROR) {
		xmqtt311Mark(&Reader,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LOCAL_ERROR : XMQTT_CODEC_MALFORMED,Error==XMQTT_WIRE_LIMIT ? XMQTT_CODEC_LIMIT : XMQTT_CODEC_FORMAT,0); goto Failed;
	}
	Reader.Value.Type=Header.Type; Reader.Value.Flags=Header.Flags;
	if (!xmqtt311Role(Header.Type,Direction)) { xmqtt311ReadFail(&Reader,XMQTT_CODEC_PROTOCOL_ERROR,XMQTT_CODEC_PROTOCOL,0,"forbidden MQTT 3.1.1 sender direction"); goto Failed; }
	bool TwoBytes=Header.Type==XMQTT_PACKET_CONNACK || (Header.Type>=XMQTT_PACKET_PUBACK && Header.Type<=XMQTT_PACKET_PUBCOMP) || Header.Type==XMQTT_PACKET_UNSUBACK;
	bool Empty=Header.Type>=XMQTT_PACKET_PINGREQ;
	if ((TwoBytes && Header.RemainingLength!=2) || (Empty && Header.RemainingLength)) {
		xmqtt311ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,0,"incorrect fixed MQTT 3.1.1 body length"); goto Failed;
	}
	if (Input.Size<Header.PacketBytes) return XMQTT_CODEC_MORE;
	Reader.Position=Header.HeaderBytes; Reader.End=Header.PacketBytes;
	if (!xmqtt311ReadBody(&Reader)) goto Failed;
	if (Reader.Position!=Reader.End) { xmqtt311ReadFail(&Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader.Position,"unexpected trailing MQTT body bytes"); goto Failed; }
	Reader.Value.Wire=(xbytesview){Input.Data,Header.PacketBytes}; *Packet=Reader.Value;
	if (Issue!=NULL) *Issue=(xmqttcodecissue){XMQTT_CODEC_DONE,XMQTT_CODEC_OK,XRT_NPOS,0,0,0};
	return XMQTT_CODEC_DONE;
Failed:
	if (Issue!=NULL) *Issue=Reader.Failure;
	return Reader.Failure.Status;
}

typedef struct xmqtt311plan {
	const xmqttpacket* Packet;
	xmqttcodecconfig Config;
	size_t Body, Total;
	uint8 ConnectFlags;
} xmqtt311plan;

static size_t xmqtt311VbiSize(size_t Value) { return 1u+(Value>=128u)+(Value>=16384u)+(Value>=2097152u); }

static bool xmqtt311Add(xmqtt311plan* Plan, size_t Size)
{
	if (Size>XMQTT_VARIABLE_MAX-Plan->Body) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT body exceeds protocol length");
	Plan->Body+=Size;
	return Plan->Body+1u+xmqtt311VbiSize(Plan->Body)<=Plan->Config.MaxPacketBytes || xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT packet exceeds configured limit");
}

static bool xmqtt311Field(xmqtt311plan* Plan, const void* Data, size_t Size, bool Text)
{
	if (!xmqtt311Span(Data,Size)) return false;
	if (Size>XMQTT_STRING_MAX_BYTES) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT field exceeds two-byte length");
	return xmqtt311Add(Plan,Size+2u) && (!Text || xmqttUtf8StringCheck((xstrview){(cstr)Data,Size},Plan->Config.Utf8Flags,NULL));
}

static bool xmqtt311Prepare(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, xmqtt311plan* Plan)
{
	if (Packet==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT packet descriptor");
	if (!xmqtt311Span(Packet,sizeof(*Packet)) || !xmqttInternalCodecSettings(Direction,Config,&Plan->Config)) return false;
	Plan->Packet=Packet; xmqttpackettype Type=Packet->Type;
	if (!xmqtt311Role(Type,Direction) || !xmqttInternalFlags(Type,Packet->Flags,XMQTT_VERSION_311) || Packet->Properties.Count)
		return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"invalid MQTT 3.1.1 type/direction/flags/properties");
	bool Identifier=(Type>=XMQTT_PACKET_PUBACK && Type<=XMQTT_PACKET_UNSUBACK) || (Type==XMQTT_PACKET_PUBLISH && (Packet->Flags&XMQTT_PUBLISH_QOS_MASK));
	if (Identifier!=(Packet->Identifier!=0) || !xmqttInternalReason(XMQTT_VERSION_311,Type,Packet->Reason,Direction) ||
		(Type==XMQTT_PACKET_SUBACK && Packet->Reason) || (Packet->SessionPresent && (Type!=XMQTT_PACKET_CONNACK || Packet->Reason)))
		return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 identifier/return code/session flag");
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (Value->WillQos>2 || Value->WillProperties.Count || (!Value->HasWill && (Value->WillQos || Value->WillRetain || Value->WillTopic.Size || Value->WillPayload.Size)) ||
			(Value->HasPassword && !Value->HasUsername) || (!Value->HasUsername && Value->Username.Size) || (!Value->HasPassword && Value->Password.Size))
			return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"inconsistent MQTT 3.1.1 CONNECT flags/fields");
		if (!Value->ClientId.Size && !Value->CleanStart) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_CLIENT_ID,"empty ClientId requires Clean Session");
		Plan->ConnectFlags=(uint8)((Value->CleanStart ? 2u : 0u)|(Value->HasWill ? 4u : 0u)|(Value->WillQos<<3)|
			(Value->WillRetain ? 32u : 0u)|(Value->HasUsername ? 128u : 0u)|(Value->HasPassword ? 64u : 0u));
		if (!xmqtt311Add(Plan,10) || !xmqtt311Field(Plan,Value->ClientId.Data,Value->ClientId.Size,true)) return false;
		if (Value->HasWill) {
			if (Value->WillPayload.Size>Plan->Config.MaxPayloadBytes) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"Will payload exceeds configured limit");
			if (!xmqtt311Field(Plan,Value->WillTopic.Data,Value->WillTopic.Size,true)) return false;
			if (!xmqttInternalTopic(Value->WillTopic,false,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Will Topic");
			if (!xmqtt311Field(Plan,Value->WillPayload.Data,Value->WillPayload.Size,false)) return false;
		}
		if (Value->HasUsername && !xmqtt311Field(Plan,Value->Username.Data,Value->Username.Size,true)) return false;
		return !Value->HasPassword || xmqtt311Field(Plan,Value->Password.Data,Value->Password.Size,false);
	}
	if (Type==XMQTT_PACKET_PUBLISH) {
		const xmqttpublishpacket* Value=&Packet->Body.Publish;
		if (!xmqtt311Span(Value->Payload.Data,Value->Payload.Size)) return false;
		if (Value->Payload.Size>Plan->Config.MaxPayloadBytes) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"PUBLISH payload exceeds configured limit");
		if (!xmqtt311Field(Plan,Value->Topic.Data,Value->Topic.Size,true)) return false;
		if (!xmqttInternalTopic(Value->Topic,false,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT PUBLISH Topic Name");
		return (!Packet->Identifier || xmqtt311Add(Plan,2)) && xmqtt311Add(Plan,Value->Payload.Size);
	}
	if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!Values.Count) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"MQTT topic request is empty");
		if (Values.Count>Plan->Config.MaxTopics) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"MQTT topic count exceeds limit");
		if (Values.Count>SIZE_MAX/sizeof(*Values.Data)) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT topic vector size overflows");
		if (!xmqtt311Span(Values.Data,Values.Count*sizeof(*Values.Data)) || !xmqtt311Add(Plan,2)) return false;
		for (size_t Index=0; Index<Values.Count; ++Index) {
			const xmqttsubscription* Value=&Values.Data[Index];
			if (!xmqtt311Field(Plan,Value->Filter.Data,Value->Filter.Size,true)) return false;
			if (!xmqttInternalTopic(Value->Filter,true,false,NULL)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_TOPIC,"invalid MQTT Topic Filter");
			if (Value->Options>(Type==XMQTT_PACKET_SUBSCRIBE ? 2u : 0u)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 subscription options");
			if (Type==XMQTT_PACKET_SUBSCRIBE && !xmqtt311Add(Plan,1)) return false;
		}
		return true;
	}
	if (Type==XMQTT_PACKET_SUBACK) {
		xbytesview Values=Packet->Body.Reasons;
		if (!Values.Size) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"SUBACK return-code list is empty");
		if (Values.Size>Plan->Config.MaxTopics) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_LIMIT,"SUBACK return-code count exceeds limit");
		if (!xmqtt311Span(Values.Data,Values.Size) || !xmqtt311Add(Plan,2) || !xmqtt311Add(Plan,Values.Size)) return false;
		for (size_t Index=0; Index<Values.Size; ++Index)
			if (!xmqttInternalReason(XMQTT_VERSION_311,Type,Values.Data[Index],Direction)) return xmqtt311Fail(XERR_PROTOCOL,XMQTT_CODEC_PROTOCOL,"invalid MQTT 3.1.1 SUBACK return code");
		return true;
	}
	if (Type==XMQTT_PACKET_UNSUBACK && Packet->Body.Reasons.Size) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT 3.1.1 UNSUBACK has no reason list");
	return Type>=XMQTT_PACKET_PINGREQ || xmqtt311Add(Plan,2);
}

static bool xmqtt311Disjoint(const void* Output, size_t Size, const void* Data, size_t Bytes)
{
	return !xrtMemRangesOverlap(Output,Size,Data,Bytes) || xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"MQTT output overlaps active source data");
}

static bool xmqtt311Output(const xmqtt311plan* Plan, const void* Output, size_t Size, const xmqttcodecconfig* Config)
{
	if (Output==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT output");
	const xmqttpacket* Packet=Plan->Packet;
	if (!xmqtt311Span(Output,Size) || !xmqtt311Disjoint(Output,Size,Packet,sizeof(*Packet)) || (Config!=NULL && !xmqtt311Disjoint(Output,Size,Config,sizeof(*Config)))) return false;
	if (Packet->Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		if (!xmqtt311Disjoint(Output,Size,Value->ClientId.Data,Value->ClientId.Size) || (Value->HasWill &&
			(!xmqtt311Disjoint(Output,Size,Value->WillTopic.Data,Value->WillTopic.Size) || !xmqtt311Disjoint(Output,Size,Value->WillPayload.Data,Value->WillPayload.Size))) ||
			(Value->HasUsername && !xmqtt311Disjoint(Output,Size,Value->Username.Data,Value->Username.Size)) ||
			(Value->HasPassword && !xmqtt311Disjoint(Output,Size,Value->Password.Data,Value->Password.Size))) return false;
	} else if (Packet->Type==XMQTT_PACKET_PUBLISH) {
		if (!xmqtt311Disjoint(Output,Size,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size) ||
			!xmqtt311Disjoint(Output,Size,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBSCRIBE || Packet->Type==XMQTT_PACKET_UNSUBSCRIBE) {
		xmqttsubscriptionlist Values=Packet->Body.Topics;
		if (!xmqtt311Disjoint(Output,Size,Values.Data,Values.Count*sizeof(*Values.Data))) return false;
		for (size_t Index=0; Index<Values.Count; ++Index) if (!xmqtt311Disjoint(Output,Size,Values.Data[Index].Filter.Data,Values.Data[Index].Filter.Size)) return false;
	} else if (Packet->Type==XMQTT_PACKET_SUBACK && !xmqtt311Disjoint(Output,Size,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size)) return false;
	return true;
}

static unsigned char* xmqtt311EmitWord(unsigned char* Output, uint16 Value)
{
	*Output++=(unsigned char)(Value>>8); *Output++=(unsigned char)Value; return Output;
}

static unsigned char* xmqtt311EmitData(unsigned char* Output, const void* Data, size_t Size)
{
	if (Size) memcpy(Output,Data,Size);
	return Output+Size;
}

static unsigned char* xmqtt311EmitField(unsigned char* Output, const void* Data, size_t Size)
{
	return xmqtt311EmitData(xmqtt311EmitWord(Output,(uint16)Size),Data,Size);
}

static void xmqtt311Emit(const xmqtt311plan* Plan, ptr Output)
{
	const xmqttpacket* Packet=Plan->Packet; xmqttpackettype Type=Packet->Type; unsigned char* Cursor=(unsigned char*)Output; size_t Bytes;
	*Cursor++=(uint8)((Type<<4)|Packet->Flags); (void)xmqttVbiWrite(Cursor,Plan->Total-1u,(uint32)Plan->Body,&Bytes); Cursor+=Bytes;
	if (Type==XMQTT_PACKET_CONNECT) {
		const xmqttconnectpacket* Value=&Packet->Body.Connect;
		Cursor=xmqtt311EmitData(Cursor,"\x00\x04MQTT\x04",7); *Cursor++=Plan->ConnectFlags; Cursor=xmqtt311EmitWord(Cursor,Value->KeepAlive);
		Cursor=xmqtt311EmitField(Cursor,Value->ClientId.Data,Value->ClientId.Size);
		if (Value->HasWill) { Cursor=xmqtt311EmitField(Cursor,Value->WillTopic.Data,Value->WillTopic.Size); Cursor=xmqtt311EmitField(Cursor,Value->WillPayload.Data,Value->WillPayload.Size); }
		if (Value->HasUsername) Cursor=xmqtt311EmitField(Cursor,Value->Username.Data,Value->Username.Size);
		if (Value->HasPassword) Cursor=xmqtt311EmitField(Cursor,Value->Password.Data,Value->Password.Size);
	} else if (Type==XMQTT_PACKET_PUBLISH) {
		Cursor=xmqtt311EmitField(Cursor,Packet->Body.Publish.Topic.Data,Packet->Body.Publish.Topic.Size);
		if (Packet->Identifier) Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		(void)xmqtt311EmitData(Cursor,Packet->Body.Publish.Payload.Data,Packet->Body.Publish.Payload.Size);
	} else if (Type==XMQTT_PACKET_SUBSCRIBE || Type==XMQTT_PACKET_UNSUBSCRIBE) {
		Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		for (size_t Index=0; Index<Packet->Body.Topics.Count; ++Index) {
			const xmqttsubscription* Value=&Packet->Body.Topics.Data[Index]; Cursor=xmqtt311EmitField(Cursor,Value->Filter.Data,Value->Filter.Size);
			if (Type==XMQTT_PACKET_SUBSCRIBE) *Cursor++=Value->Options;
		}
	} else if (Type==XMQTT_PACKET_CONNACK) { *Cursor++=Packet->SessionPresent ? 1u : 0u; *Cursor=Packet->Reason; }
	else if (Type<XMQTT_PACKET_PINGREQ) {
		Cursor=xmqtt311EmitWord(Cursor,Packet->Identifier);
		if (Type==XMQTT_PACKET_SUBACK) (void)xmqtt311EmitData(Cursor,Packet->Body.Reasons.Data,Packet->Body.Reasons.Size);
	}
}

XMQTT_API bool xmqttPacketSize311(const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Required)
{
	xmqtt311plan Plan={0};
	if (!xmqtt311Prepare(Packet,Direction,Config,&Plan) || !xmqtt311Output(&Plan,Required,sizeof(*Required),Config)) return false;
	*Required=Plan.Body+1u+xmqtt311VbiSize(Plan.Body); return true;
}

XMQTT_API bool xmqttPacketWrite311(ptr Output, size_t Capacity, const xmqttpacket* Packet, xmqttdirection Direction, const xmqttcodecconfig* Config, size_t* Written)
{
	xmqtt311plan Plan={0};
	if (Output==NULL) return xmqtt311Fail(XERR_ARGUMENT,XMQTT_CODEC_ARGUMENT,"NULL MQTT wire output");
	if (!xmqtt311Span(Output,Capacity) || !xmqtt311Prepare(Packet,Direction,Config,&Plan)) return false;
	Plan.Total=Plan.Body+1u+xmqtt311VbiSize(Plan.Body);
	if (Capacity<Plan.Total) return xmqtt311Fail(XERR_RANGE,XMQTT_CODEC_CAPACITY,"MQTT wire capacity is too small");
	if (!xmqtt311Output(&Plan,Output,Plan.Total,Config) || (Written!=NULL &&
		(!xmqtt311Output(&Plan,Written,sizeof(*Written),Config) || !xmqtt311Disjoint(Written,sizeof(*Written),Output,Plan.Total)))) return false;
	xmqtt311Emit(&Plan,Output); if (Written!=NULL) *Written=Plan.Total; return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/negotiation/negotiation.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
static bool xmqttNegSpan(const void* P,size_t N){return (P || !N) && xrtMemRangeValid(P,N);}
static bool xmqttNegDisjoint(const void* O,size_t N,const void* P,size_t B)
{return !N || !B || !xrtMemRangesOverlap(O,N,P,B);}
static bool xmqttNegView(const void* O,size_t N,xstrview V)
{return xmqttNegDisjoint(O,N,V.Data,V.Size);}
static bool xmqttNegPropertiesOutput(const void* O,size_t N,xmqttpropertylist L)
{
    if(!xmqttNegDisjoint(O,N,L.Data,L.Count*sizeof(*L.Data)))return false;
    for(size_t K=0;K<L.Count;++K){const xmqttproperty* P=&L.Data[K];
        if(P->Type==XMQTT_PROPERTY_STRING && !xmqttNegView(O,N,P->Value.Text))return false;
        if(P->Type==XMQTT_PROPERTY_BINARY && !xmqttNegDisjoint(O,N,P->Value.Binary.Data,P->Value.Binary.Size))return false;
        if(P->Type==XMQTT_PROPERTY_STRING_PAIR && (!xmqttNegView(O,N,P->Value.Pair.Name) || !xmqttNegView(O,N,P->Value.Pair.Value)))return false;
    }return true;
}
/* Called only after the public codec has validated every descriptor/span. */
static bool xmqttNegPacketOutput(const void* O,size_t N,const xmqttpacket* P)
{
    if(!N)return true;
    if(!xmqttNegDisjoint(O,N,P,sizeof(*P)) || !xmqttNegPropertiesOutput(O,N,P->Properties))return false;
    if(P->Type==XMQTT_PACKET_CONNECT){const xmqttconnectpacket* C=&P->Body.Connect;
        if(!xmqttNegView(O,N,C->ClientId))return false;
        if(C->HasWill && (!xmqttNegPropertiesOutput(O,N,C->WillProperties) || !xmqttNegView(O,N,C->WillTopic) || !xmqttNegDisjoint(O,N,C->WillPayload.Data,C->WillPayload.Size)))return false;
        if(C->HasUsername && !xmqttNegView(O,N,C->Username))return false;
        if(C->HasPassword && !xmqttNegDisjoint(O,N,C->Password.Data,C->Password.Size))return false;
    }else if(P->Type==XMQTT_PACKET_PUBLISH){if(!xmqttNegView(O,N,P->Body.Publish.Topic) || !xmqttNegDisjoint(O,N,P->Body.Publish.Payload.Data,P->Body.Publish.Payload.Size))return false;
    }else if(P->Type==XMQTT_PACKET_SUBSCRIBE || P->Type==XMQTT_PACKET_UNSUBSCRIBE){
        if(!xmqttNegDisjoint(O,N,P->Body.Topics.Data,P->Body.Topics.Count*sizeof(*P->Body.Topics.Data)))return false;
        for(size_t K=0;K<P->Body.Topics.Count;++K)if(!xmqttNegView(O,N,P->Body.Topics.Data[K].Filter))return false;
    }else if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK){if(!xmqttNegDisjoint(O,N,P->Body.Reasons.Data,P->Body.Reasons.Size))return false;}
    return true;
}
static xmqttnegotiationstatus xmqttNegArgument(void)
{xrtSetErrorInfo(XERR_ARGUMENT,"xrt.mqtt",XMQTT_NEGOTIATION_ARGUMENT,"invalid MQTT negotiation input/output");return XMQTT_NEGOTIATION_LOCAL_ERROR;}
static xmqttnegotiationstatus xmqttNegFail(xmqttnegotiationstatus S,xmqttnegotiationerror E,xmqttpropertyid P,xmqttnegotiationissue* I)
{
    if(I)*I=(xmqttnegotiationissue){S,E,P};
    xerrkind Kind=S==XMQTT_NEGOTIATION_RESTRICTED?XERR_RANGE:S==XMQTT_NEGOTIATION_PROTOCOL_ERROR?XERR_PROTOCOL:XERR_STATE;
    xrtSetErrorInfo(Kind,"xrt.mqtt",(int32)E,"MQTT negotiation constraint failed");return S;
}
static bool xmqttNegSize(const xmqttpacket* P,xmqttversion V,xmqttdirection D,const xmqttcodecconfig* C,size_t* N)
{return V==XMQTT_VERSION_5?xmqttPacketSize5(P,D,C,N):xmqttPacketSize311(P,D,C,N);}
static bool xmqttNegEqual(xstrview A,xstrview B)
{return A.Size==B.Size && (!A.Size || !memcmp(A.Data,B.Data,A.Size));}
static uint32 xmqttNegPacketLimit(uint32 V){return V>XMQTT_PACKET_MAX_BYTES?XMQTT_PACKET_MAX_BYTES:V;}
XMQTT_API bool xmqttNegotiationConfigInit(xmqttnegotiationconfig* C)
{
    if(!C || !xmqttNegSpan(C,sizeof(*C))){xmqttNegArgument();return false;}
    *C=(xmqttnegotiationconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;xmqttCodecConfigInit(&C->Codec);return true;
}
XMQTT_API xmqttnegotiationstatus xmqttNegotiate(const xmqttpacket* Connect,const xmqttpacket* Connack,
    const xmqttnegotiationconfig* Config,xmqttnegotiated* Result,xmqttnegotiationissue* Issue)
{
    if(!Connect || !Connack || !Result || !xmqttNegSpan(Connect,sizeof(*Connect)) || !xmqttNegSpan(Connack,sizeof(*Connack)) ||
        !xmqttNegSpan(Result,sizeof(*Result)) || (Issue && !xmqttNegSpan(Issue,sizeof(*Issue))) || (Config && !xmqttNegSpan(Config,sizeof(*Config))) ||
        !xmqttNegDisjoint(Result,sizeof(*Result),Connect,sizeof(*Connect)) || !xmqttNegDisjoint(Result,sizeof(*Result),Connack,sizeof(*Connack)) ||
        !xmqttNegDisjoint(Result,sizeof(*Result),Config,Config?sizeof(*Config):0) || !xmqttNegDisjoint(Result,sizeof(*Result),Issue,Issue?sizeof(*Issue):0) ||
        !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Connect,sizeof(*Connect)) || !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Connack,sizeof(*Connack)) ||
        !xmqttNegDisjoint(Issue,Issue?sizeof(*Issue):0,Config,Config?sizeof(*Config):0))return xmqttNegArgument();
    xmqttnegotiationconfig C;if(Config)C=*Config;else xmqttNegotiationConfigInit(&C);
    if(C.Size!=sizeof(C) || (C.Version!=XMQTT_VERSION_5 && C.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT || Connack->Type!=XMQTT_PACKET_CONNACK)return xmqttNegArgument();
    size_t ConnectBytes=0,ConnackBytes=0;
    if(!xmqttNegSize(Connect,C.Version,XMQTT_CLIENT_TO_SERVER,&C.Codec,&ConnectBytes) || !xmqttNegSize(Connack,C.Version,XMQTT_SERVER_TO_CLIENT,&C.Codec,&ConnackBytes))return XMQTT_NEGOTIATION_LOCAL_ERROR;
    if(!xmqttNegPacketOutput(Result,sizeof(*Result),Connect) || !xmqttNegPacketOutput(Result,sizeof(*Result),Connack) ||
        !xmqttNegPacketOutput(Issue,Issue?sizeof(*Issue):0,Connect) || !xmqttNegPacketOutput(Issue,Issue?sizeof(*Issue):0,Connack))return xmqttNegArgument();
    (void)ConnectBytes;xmqttnegotiated N={0};N.Size=sizeof(N);N.Version=C.Version;N.Reason=Connack->Reason;
    N.ClientId=Connect->Body.Connect.ClientId;N.KeepAlive=Connect->Body.Connect.KeepAlive;
    N.MaximumQos=2;N.RetainAvailable=N.WildcardAvailable=N.SharedAvailable=N.SubscriptionIdsAvailable=true;
    N.SendReceiveMaximum=N.ReceiveMaximum=UINT16_MAX;N.SendPacketBytes=N.ReceivePacketBytes=XMQTT_PACKET_MAX_BYTES;
    bool RequestedResponse=false;const xmqttproperty *Method=NULL,*AckMethod=NULL,*Assigned=NULL,*AuthData=NULL,*Response=NULL;
    if(C.Version==XMQTT_VERSION_5){
        for(size_t K=0;K<Connect->Properties.Count;++K){const xmqttproperty* P=&Connect->Properties.Data[K];switch(P->Id){
            case XMQTT_PROPERTY_SESSION_EXPIRY:N.RequestedSessionExpiry=N.SessionExpiry=P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE:N.ReceivePacketBytes=xmqttNegPacketLimit(P->Value.Number);break;
            case XMQTT_PROPERTY_RECEIVE_MAXIMUM:N.ReceiveMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM:N.ReceiveAliasMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_REQUEST_RESPONSE_INFO:RequestedResponse=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_AUTH_METHOD:Method=P;N.HasAuthMethod=true;N.AuthMethod=P->Value.Text;break;
            default:break;}}
        for(size_t K=0;K<Connack->Properties.Count;++K){const xmqttproperty* P=&Connack->Properties.Data[K];switch(P->Id){
            case XMQTT_PROPERTY_SESSION_EXPIRY:N.SessionExpiry=P->Value.Number;break;
            case XMQTT_PROPERTY_SERVER_KEEP_ALIVE:N.KeepAlive=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE:N.SendPacketBytes=xmqttNegPacketLimit(P->Value.Number);break;
            case XMQTT_PROPERTY_RECEIVE_MAXIMUM:N.SendReceiveMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM:N.SendAliasMaximum=(uint16)P->Value.Number;break;
            case XMQTT_PROPERTY_MAXIMUM_QOS:N.MaximumQos=(uint8)P->Value.Number;break;
            case XMQTT_PROPERTY_RETAIN_AVAILABLE:N.RetainAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_WILDCARD_AVAILABLE:N.WildcardAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_SHARED_AVAILABLE:N.SharedAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_SUBSCRIPTION_ID_AVAILABLE:N.SubscriptionIdsAvailable=P->Value.Number!=0;break;
            case XMQTT_PROPERTY_ASSIGNED_CLIENT_ID:Assigned=P;break;
            case XMQTT_PROPERTY_AUTH_METHOD:AckMethod=P;break;
            case XMQTT_PROPERTY_AUTH_DATA:AuthData=P;break;
            case XMQTT_PROPERTY_RESPONSE_INFO:Response=P;N.ResponseInfo=P->Value.Text;break;
            case XMQTT_PROPERTY_SERVER_REFERENCE:N.ServerReference=P->Value.Text;break;
            default:break;}}
        if(ConnackBytes>N.ReceivePacketBytes)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_PACKET_SIZE,XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Issue);
        if(!Method && AckMethod)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(!Method && AuthData)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_DATA,XMQTT_PROPERTY_AUTH_DATA,Issue);
        if(!RequestedResponse && Response)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RESPONSE_INFO,XMQTT_PROPERTY_RESPONSE_INFO,Issue);
    }
    if(Connack->Reason){xmqttnegotiated R={0};R.Size=sizeof(R);R.Version=C.Version;R.Reason=Connack->Reason;R.ServerReference=N.ServerReference;*Result=R;if(Issue)*Issue=(xmqttnegotiationissue){0};return XMQTT_NEGOTIATION_REJECTED;}
    if(Connack->SessionPresent && Connect->Body.Connect.CleanStart)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
    if(C.Version==XMQTT_VERSION_5){
        if(Method && (!AckMethod || !xmqttNegEqual(Method->Value.Text,AckMethod->Value.Text)))return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(!N.ClientId.Size){
            if(!Assigned || !Assigned->Value.Text.Size)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID,XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,Issue);
            if(Connack->SessionPresent)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
            N.ClientId=Assigned->Value.Text;N.ServerAssignedId=true;
        }else if(Assigned && !xmqttNegEqual(N.ClientId,Assigned->Value.Text))return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_CLIENT_ID,XMQTT_PROPERTY_ASSIGNED_CLIENT_ID,Issue);
        if(Connack->SessionPresent && !C.HasSessionState)return xmqttNegFail(XMQTT_NEGOTIATION_STATE_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
        if(Connect->Body.Connect.HasWill){
            if(Connect->Body.Connect.WillQos>N.MaximumQos)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_QOS,XMQTT_PROPERTY_MAXIMUM_QOS,Issue);
            if(Connect->Body.Connect.WillRetain && !N.RetainAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_PROTOCOL_ERROR,XMQTT_NEGOTIATION_RETAIN,XMQTT_PROPERTY_RETAIN_AVAILABLE,Issue);
        }
    }
    N.Accepted=true;N.SessionPresent=Connack->SessionPresent;*Result=N;if(Issue)*Issue=(xmqttnegotiationissue){0};return XMQTT_NEGOTIATION_DONE;
}
XMQTT_API xmqttnegotiationstatus xmqttNegotiatedPacketCheck(const xmqttnegotiated* Peer,const xmqttpacket* Packet,
    const xmqttcodecconfig* Codec,size_t* Required,xmqttnegotiationissue* Issue)
{
    size_t RequiredBytes=Required?sizeof(*Required):0,IssueBytes=Issue?sizeof(*Issue):0;
    if(!Peer || !Packet || !xmqttNegSpan(Peer,sizeof(*Peer)) || !xmqttNegSpan(Packet,sizeof(*Packet)) || (Required && !xmqttNegSpan(Required,RequiredBytes)) ||
        (Issue && !xmqttNegSpan(Issue,IssueBytes)) || (Codec && !xmqttNegSpan(Codec,sizeof(*Codec))) || !xmqttNegDisjoint(Required,RequiredBytes,Issue,IssueBytes) ||
        !xmqttNegDisjoint(Required,RequiredBytes,Peer,sizeof(*Peer)) || !xmqttNegDisjoint(Issue,IssueBytes,Peer,sizeof(*Peer)) ||
        !xmqttNegDisjoint(Required,RequiredBytes,Codec,Codec?sizeof(*Codec):0) || !xmqttNegDisjoint(Issue,IssueBytes,Codec,Codec?sizeof(*Codec):0))return xmqttNegArgument();
    if(Peer->Size!=sizeof(*Peer) || !Peer->Accepted || (Peer->Version!=XMQTT_VERSION_5 && Peer->Version!=XMQTT_VERSION_311) ||
        !Peer->SendPacketBytes || Peer->SendPacketBytes>XMQTT_PACKET_MAX_BYTES || Peer->MaximumQos>2 || !Peer->SendReceiveMaximum || !Peer->ReceiveMaximum ||
        !xmqttNegSpan(Peer->ClientId.Data,Peer->ClientId.Size) || !xmqttNegSpan(Peer->AuthMethod.Data,Peer->AuthMethod.Size) ||
        !xmqttNegSpan(Peer->ServerReference.Data,Peer->ServerReference.Size) || !xmqttNegSpan(Peer->ResponseInfo.Data,Peer->ResponseInfo.Size))return xmqttNegArgument();
    const xstrview Views[]={Peer->ClientId,Peer->AuthMethod,Peer->ServerReference,Peer->ResponseInfo};
    for(size_t K=0;K<4;++K)if(!xmqttNegView(Required,RequiredBytes,Views[K]) || !xmqttNegView(Issue,IssueBytes,Views[K]))return xmqttNegArgument();
    size_t Bytes=0;if(!xmqttNegSize(Packet,Peer->Version,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes))return XMQTT_NEGOTIATION_LOCAL_ERROR;
    if(!xmqttNegPacketOutput(Required,RequiredBytes,Packet) || !xmqttNegPacketOutput(Issue,IssueBytes,Packet))return xmqttNegArgument();
    if(Packet->Type==XMQTT_PACKET_CONNECT)return xmqttNegFail(XMQTT_NEGOTIATION_STATE_ERROR,XMQTT_NEGOTIATION_SESSION,0,Issue);
    if(Bytes>Peer->SendPacketBytes)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_PACKET_SIZE,XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE,Issue);
    if(Peer->Version==XMQTT_VERSION_5){
        if(Packet->Type==XMQTT_PACKET_PUBLISH){
            if(((Packet->Flags&XMQTT_PUBLISH_QOS_MASK)>>1)>Peer->MaximumQos)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_QOS,XMQTT_PROPERTY_MAXIMUM_QOS,Issue);
            if((Packet->Flags&XMQTT_PUBLISH_RETAIN) && !Peer->RetainAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_RETAIN,XMQTT_PROPERTY_RETAIN_AVAILABLE,Issue);
        }
        const xmqttproperty* Method=NULL;
        for(size_t K=0;K<Packet->Properties.Count;++K){const xmqttproperty* P=&Packet->Properties.Data[K];
            if(P->Id==XMQTT_PROPERTY_TOPIC_ALIAS && P->Value.Number>Peer->SendAliasMaximum)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_TOPIC_ALIAS,P->Id,Issue);
            if(Packet->Type==XMQTT_PACKET_SUBSCRIBE && P->Id==XMQTT_PROPERTY_SUBSCRIPTION_ID && !Peer->SubscriptionIdsAvailable)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SUBSCRIPTION_ID,P->Id,Issue);
            if(Packet->Type==XMQTT_PACKET_DISCONNECT && P->Id==XMQTT_PROPERTY_SESSION_EXPIRY && !Peer->RequestedSessionExpiry && P->Value.Number)return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SESSION_EXPIRY,P->Id,Issue);
            if(P->Id==XMQTT_PROPERTY_AUTH_METHOD)Method=P;
        }
        if(Packet->Type==XMQTT_PACKET_AUTH && (!Peer->HasAuthMethod || !Method || !xmqttNegEqual(Peer->AuthMethod,Method->Value.Text)))return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_AUTH_METHOD,XMQTT_PROPERTY_AUTH_METHOD,Issue);
        if(Packet->Type==XMQTT_PACKET_SUBSCRIBE){
            for(size_t K=0;K<Packet->Body.Topics.Count;++K){xstrview F=Packet->Body.Topics.Data[K].Filter;
                if(!Peer->SharedAvailable && F.Size>=7 && !memcmp(F.Data,"$share/",7))return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_SHARED,XMQTT_PROPERTY_SHARED_AVAILABLE,Issue);
                if(!Peer->WildcardAvailable)for(size_t I=0;I<F.Size;++I)if(F.Data[I]=='+' || F.Data[I]=='#')return xmqttNegFail(XMQTT_NEGOTIATION_RESTRICTED,XMQTT_NEGOTIATION_WILDCARD,XMQTT_PROPERTY_WILDCARD_AVAILABLE,Issue);
            }
        }
    }
    if(Required)*Required=Bytes;
    if(Issue)*Issue=(xmqttnegotiationissue){0};
    return XMQTT_NEGOTIATION_DONE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmqtt/src/client/client.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
#include <math.h>
#include <stddef.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)

/* The host worker is the sole protocol owner. Gate protects admission, public
 * snapshots and the receive queue; no user callback runs while it is held. */
typedef struct xmqttquota {xatomic32 Refs;xmutex Gate;size_t Count,Bytes,MaxCount,MaxBytes;} xmqttquota;
typedef struct xmqttbox {xmqttquota* Quota;size_t Bytes;} xmqttbox;
typedef struct xmqttreadybox {xmqttreadyresult Value;xmqttbox Box;char Id[];} xmqttreadybox;
typedef struct xmqttwritebox {xmqttwriteresult Value;xmqttbox Box;} xmqttwritebox;
typedef struct xmqttrequest xmqttrequest;
typedef struct xmqttsendflow {
    size_t Capacity,Reserved;uint16 Next;
    unsigned char Identifiers[8192];
    xmqttrequest* Buckets[];
} xmqttsendflow;
typedef struct xmqttstartup {xatomic32 Refs;xmqttclient* Client;size_t Bytes;unsigned char Data[];} xmqttstartup;
struct xmqttmessage {xatomic32 Refs;xmqttbox Box;xmqttpacket Packet;xmqttmessage* Next;xmqttproperty Props[];};
struct xmqttrequest {
    xatomic32 Refs,NativeRefs;
    xmqttclient* Client;xmqttrequest *Next,*Previous,*HashNext;
    xpromise* Promise;xcancel* Cancel;xmqttwritebox* Result;
    size_t Bytes,Offset;double Deadline;bool Retain,Credit,Acknowledged;
    uint8 Qos;uint16 Identifier;
    unsigned char Wire[];
};
struct xmqttclient {
    xatomic32 Refs;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetstream* Stream;
    xmqttclientconfig Config;xmqttclientinfo Info;xmqttquota* Quota;
    xnetpost Post;bool Posted,Started,Stopped,ReadEnded,NativeClosed,ProblemInfo;
    xpromise* ReadyPromise;xfuture* ReadyFuture;xcancel* ReadyCancel;xmqttreadybox* ReadyBox;
    xmqttstartup* Startup;size_t SetupOffset;xmqttpacket SetupPacket;
    xmqttproperty *SetupProps,*Props;
    unsigned char* Input;size_t InputBytes,InputCapacity;
    xmqttrequest *SubmitHead,*SubmitTail,*Requests,*RequestTail;
    xatomicptr SendFlow;uint32 SendInflight;
    xmqttmessage *MessageHead,*MessageTail;
    xmqttnegotiated Peer;xerror* Cause;
    double SetupDeadline,NextPing,PingDeadline,TimerAt;
    uint64 Timer;unsigned PingOffset;bool PingOutstanding;
    xmqttproperty Scratch[];
};
static void xmqttClientWork(xnetworker* Worker,ptr Data);
static bool xmqttClientWakeLocked(xmqttclient* C);
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause);
static bool xmqttClientError(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.mqtt.client",1,Message);return false;}
static void xmqttClientRef(xmqttclient* C)
{(void)xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);}
static void xmqttClientDrop(const void* Data)
{xmqttclient* C=(xmqttclient*)Data;(void)xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_ACQ_REL);}
/* RefCount is the actual first member of both private producer objects.
 * Their children are deliberately opaque to a foreign ownership collector. */
static bool xmqttPrivateCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load((const xatomic32*)Data,XMEMORY_ACQUIRE);return true;}
static bool xmqttPrivateTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xmqttPrivateOps={xmqttPrivateCount,xmqttPrivateTrace};
static bool xmqttClientRetainProducer(const void* Data)
{xmqttClientRef((xmqttclient*)Data);return true;}
static void xmqttClientWake(xmqttclient* C)
{xrtMutexLock(&C->Gate);(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static bool xmqttClientWakeLocked(xmqttclient* C)
{
    if(C->Posted)return true;
    C->Posted=true;xmqttClientRef(C);
    if(xrtNetPost(C->Worker,&C->Post,xmqttClientWork,C))return true;
    C->Posted=false;xmqttClientDrop(C);return false;
}
static bool xmqttClientClosing(xmqttclient* C)
{xrtMutexLock(&C->Gate);bool B=C->Info.State>=XMQTT_CLIENT_CLOSING;xrtMutexUnlock(&C->Gate);return B;}
static void xmqttClientNotify(xmqttclient* C)
{if(C->Config.Notify)C->Config.Notify(C,C->Config.User);}
static double xmqttClientDeadline(double Now,int64 Milliseconds)
{return Milliseconds==XRT_WAIT_FOREVER?INFINITY:nextafter(Now+(double)(Milliseconds/1000)+(double)(Milliseconds%1000)*0.001,INFINITY);}
static int64 xmqttClientRemaining(double At)
{
    double Delta=ceil((At-xrtTimer())*1000.0);
    if(Delta<=1.0)return 1;
    return Delta>=(double)INT64_MAX?INT64_MAX:(int64)Delta;
}
static void xmqttQuotaDrop(xmqttquota* Q)
{
    if(Q && xrtAtomic32FetchSub(&Q->Refs,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&Q->Gate);xrtFree(Q);}
}
static void* xmqttBoxAllocate(xmqttquota* Q,size_t Bytes,xmqttclientstatus* Failure)
{
    xrtMutexLock(&Q->Gate);
    if(Q->Count==Q->MaxCount || Bytes>Q->MaxBytes-Q->Bytes){xrtMutexUnlock(&Q->Gate);*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    ++Q->Count;Q->Bytes+=Bytes;(void)xrtAtomic32FetchAdd(&Q->Refs,1,XMEMORY_RELAXED);xrtMutexUnlock(&Q->Gate);
    void* P=xrtMalloc(Bytes);
    if(P){memset(P,0,Bytes);return P;}
    xrtMutexLock(&Q->Gate);--Q->Count;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xmqttQuotaDrop(Q);*Failure=XMQTT_STATUS_MEMORY;return NULL;
}
static void xmqttBoxFree(void* P,xmqttbox B)
{xrtFree(P);xrtMutexLock(&B.Quota->Gate);--B.Quota->Count;B.Quota->Bytes-=B.Bytes;xrtMutexUnlock(&B.Quota->Gate);xmqttQuotaDrop(B.Quota);}
static xstrview xmqttRebaseText(xstrview V,const unsigned char* Old,const unsigned char* New)
{if(V.Data)V.Data=(const char*)(New+((const unsigned char*)V.Data-Old));return V;}
static xbytesview xmqttRebaseBytes(xbytesview V,const unsigned char* Old,const unsigned char* New)
{if(V.Data)V.Data=New+((const unsigned char*)V.Data-Old);return V;}
static xmqttmessage* xmqttMessageCopy(xmqttclient* C,const xmqttpacket* P,xmqttclientstatus* Failure)
{
    size_t Count=P->Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttmessage))/sizeof(xmqttproperty) ||
       P->Wire.Size>SIZE_MAX-sizeof(xmqttmessage)-Count*sizeof(xmqttproperty)){*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    size_t Bytes=sizeof(xmqttmessage)+Count*sizeof(xmqttproperty)+P->Wire.Size;
    xmqttmessage* M=xmqttBoxAllocate(C->Quota,Bytes,Failure);if(!M)return NULL;
    M->Box=(xmqttbox){C->Quota,Bytes};xrtAtomic32Init(&M->Refs,1);M->Packet=*P;
    unsigned char* Wire=(unsigned char*)(M->Props+Count);memcpy(Wire,P->Wire.Data,P->Wire.Size);
    if(Count)memcpy(M->Props,P->Properties.Data,Count*sizeof(*M->Props));
    M->Packet.Wire=(xbytesview){Wire,P->Wire.Size};M->Packet.Properties.Data=Count?M->Props:NULL;
    for(size_t I=0;I<Count;++I){xmqttproperty* V=M->Props+I;
        if(V->Type==XMQTT_PROPERTY_STRING)V->Value.Text=xmqttRebaseText(V->Value.Text,P->Wire.Data,Wire);
        else if(V->Type==XMQTT_PROPERTY_BINARY)V->Value.Binary=xmqttRebaseBytes(V->Value.Binary,P->Wire.Data,Wire);
        else if(V->Type==XMQTT_PROPERTY_STRING_PAIR){V->Value.Pair.Name=xmqttRebaseText(V->Value.Pair.Name,P->Wire.Data,Wire);V->Value.Pair.Value=xmqttRebaseText(V->Value.Pair.Value,P->Wire.Data,Wire);}
    }
    if(P->Type==XMQTT_PACKET_PUBLISH){M->Packet.Body.Publish.Topic=xmqttRebaseText(P->Body.Publish.Topic,P->Wire.Data,Wire);M->Packet.Body.Publish.Payload=xmqttRebaseBytes(P->Body.Publish.Payload,P->Wire.Data,Wire);}
    return M;
}
XMQTT_API const xmqttpacket* xmqttMessagePacket(const xmqttmessage* M)
{return M?&M->Packet:NULL;}
XMQTT_API bool xmqttMessageRetain(const xmqttmessage* Message)
{
    if(!Message)return xmqttClientError(XERR_ARGUMENT,"message is NULL");
    xmqttmessage* M=(xmqttmessage*)Message;uint32 N=xrtAtomic32Load(&M->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&M->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    return xmqttClientError(XERR_RANGE,"message reference limit");
}
XMQTT_API void xmqttMessageRelease(const xmqttmessage* Message)
{xmqttmessage* M=(xmqttmessage*)Message;if(M && xrtAtomic32FetchSub(&M->Refs,1,XMEMORY_ACQ_REL)==1)xmqttBoxFree(M,M->Box);}
static void xmqttReadyFree(ptr Value,ptr Context)
{(void)Context;xmqttreadybox* B=Value;xmqttMessageRelease(B->Value.Connack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttWriteFree(ptr Value,ptr Context)
{(void)Context;xmqttwritebox* B=Value;xmqttMessageRelease(B->Value.Puback);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
/* Gate serializes cold allocation and all identifier reservations. Only the
 * worker mutates hash buckets; the immutable table address publishes atomically. */
static xmqttclientadmission xmqttIdentifierReserveLocked(xmqttclient* C,uint16 Requested,uint16* Identifier)
{
    xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
    if(!F){size_t Capacity=2;while(Capacity<C->Config.MaxRequests*2u)Capacity*=2u;
        size_t Bytes=sizeof(*F)+Capacity*sizeof(xmqttrequest*);
        if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes)return XMQTT_ADMISSION_OVERLOADED;
        F=xrtCalloc(1,Bytes);if(!F)return XMQTT_ADMISSION_ERROR;
        F->Capacity=Capacity;C->Info.PrivateBytes+=Bytes;xrtAtomicPtrStore(&C->SendFlow,F,XMEMORY_RELEASE);
    }
    if(F->Reserved==65535u)return XMQTT_ADMISSION_OVERLOADED;
    uint16 Id=Requested;
    if(Id){if(F->Identifiers[Id>>3]&(1u<<(Id&7u)))return XMQTT_ADMISSION_OVERLOADED;}
    else {do{Id=++F->Next;if(!Id)Id=++F->Next;}while(F->Identifiers[Id>>3]&(1u<<(Id&7u)));}
    F->Identifiers[Id>>3]|=(unsigned char)(1u<<(Id&7u));++F->Reserved;*Identifier=Id;return XMQTT_ADMISSION_ACCEPTED;
}
static void xmqttIdentifierReleaseLocked(xmqttclient* C,uint16 Id)
{
    if(Id){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
        F->Identifiers[Id>>3]&=(unsigned char)~(1u<<(Id&7u));--F->Reserved;}
}
static void xmqttRequestMapInsert(xmqttclient* C,xmqttrequest* R)
{
    if(R->Identifier){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);size_t Index=R->Identifier&(F->Capacity-1u);
        R->HashNext=F->Buckets[Index];F->Buckets[Index]=R;}
}
static void xmqttRequestMapRemove(xmqttclient* C,xmqttrequest* R)
{
    if(R->Identifier){xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);
        xmqttrequest** Link=F->Buckets+(R->Identifier&(F->Capacity-1u));while(*Link && *Link!=R)Link=&(*Link)->HashNext;
        if(*Link)*Link=R->HashNext;}
}
static xmqttrequest* xmqttRequestFind(xmqttclient* C,uint16 Id)
{
    xmqttsendflow* F=xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_ACQUIRE);if(!F)return NULL;
    xmqttrequest* R=F->Buckets[Id&(F->Capacity-1u)];while(R && R->Identifier!=Id)R=R->HashNext;return R;
}
static void xmqttStartupDrop(xmqttstartup* W)
{
    if(!W || xrtAtomic32FetchSub(&W->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=W->Client;size_t Bytes=sizeof(*W)+W->Bytes;xrtSecureZero(W->Data,W->Bytes);xrtFree(W);
    xrtMutexLock(&C->Gate);C->Info.StartupBytes=0;C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
}
static void xmqttStartupRelease(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;(void)Size;xmqttStartupDrop(Data);}
static void xmqttStartupRetire(xmqttclient* C)
{
    xmqttstartup* W=C->Startup;C->Startup=NULL;memset(&C->SetupPacket,0,sizeof(C->SetupPacket));
    if(C->SetupProps)memset(C->SetupProps,0,(size_t)(C->Props-C->SetupProps)*sizeof(*C->SetupProps));
    xmqttStartupDrop(W);
}
static void xmqttReadyFinish(xmqttclient* C,xmqttclientstatus Status)
{
    if(!C->ReadyPromise)return;
    xmqttreadybox* B=C->ReadyBox;xpromise* P=C->ReadyPromise;C->ReadyBox=NULL;C->ReadyPromise=NULL;
    B->Value.Status=Status;if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
    if(!xrtPromiseResolveOwned(P,B,xmqttReadyFree,NULL))xmqttReadyFree(B,NULL);
    xrtPromiseDestroy(P);xmqttStartupRetire(C);xmqttClientNotify(C);
}
static void xmqttRequestRef(xmqttrequest* R)
{(void)xrtAtomic32FetchAdd(&R->Refs,1,XMEMORY_RELAXED);}
static void xmqttRequestDrop(const void* Data)
{
    xmqttrequest* R=(xmqttrequest*)Data;if(xrtAtomic32FetchSub(&R->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=R->Client;size_t Bytes=R->Bytes;uint16 Id=R->Identifier;xrtCancelDestroy(R->Cancel);xrtFree(R);
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Id);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
}
static bool xmqttRequestRetainProducer(const void* Data)
{xmqttRequestRef((xmqttrequest*)Data);return true;}
static void xmqttRequestCancel(const void* Data)
{xmqttClientWake(((xmqttrequest*)Data)->Client);}
static void xmqttReadyCancel(const void* Data)
{xmqttclient* C=(xmqttclient*)Data;xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);xmqttClientWake(C);}
static const xfutureproducerownershipv1 xmqttRequestProducer={sizeof(xfutureproducerownershipv1),xmqttRequestDrop};
static const xfutureproducercancellationv1 xmqttRequestCancellation={sizeof(xfutureproducercancellationv1),xmqttRequestRetainProducer,xmqttRequestCancel};
static const xfutureproducerownershipv1 xmqttReadyProducer={sizeof(xfutureproducerownershipv1),xmqttClientDrop};
static const xfutureproducercancellationv1 xmqttReadyCancellation={sizeof(xfutureproducercancellationv1),xmqttClientRetainProducer,xmqttReadyCancel};
static void xmqttRequestNativeDrop(ptr Data,cbytes Bytes,size_t Size)
{
    (void)Bytes;(void)Size;xmqttrequest* R=Data;
    (void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_ACQ_REL);xmqttClientWake(R->Client);xmqttRequestDrop(R);
}
static void xmqttRequestFinish(xmqttclient* C,xmqttrequest** Link,xmqttclientstatus Status)
{
    xmqttrequest* R=*Link;*Link=R->Next;xmqttwritebox* B=R->Result;xpromise* P=R->Promise;
    if(R->Next)R->Next->Previous=R->Previous;else C->RequestTail=R->Previous;
    xmqttRequestMapRemove(C,R);if(R->Credit)--C->SendInflight;
    B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Delivery=R->Offset?XMQTT_MAY_HAVE_REACHED_PEER:XMQTT_NOT_SENT;
    B->Value.Identifier=R->Identifier;
    if(R->Acknowledged)B->Value.Delivery=B->Value.Reason<0x80?XMQTT_PEER_ACCEPTED:XMQTT_PEER_REJECTED;
    if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    if(!xrtPromiseResolveOwned(P,B,xmqttWriteFree,NULL))xmqttWriteFree(B,NULL);
    xrtPromiseDestroy(P);xmqttRequestDrop(R);xmqttClientNotify(C);
}
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XMQTT_CLIENT_CLOSING){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=Status;
        if(Status==XMQTT_STATUS_PROTOCOL)++C->Info.ProtocolErrors;
        if(Cause)C->Cause=xrtErrorRef(Cause);
    }
    xrtMutexUnlock(&C->Gate);
}
static void xmqttClientOpen(xnetstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttClientRead(xnetstream* S,xnetbuf* B,ptr Data)
{(void)S;(void)B;xmqttClientWake(Data);}
static void xmqttClientEnd(xnetstream* S,ptr Data)
{(void)S;xmqttclient* C=Data;C->ReadEnded=true;xmqttClientWake(C);}
static void xmqttClientLow(xnetstream* S,size_t Bytes,ptr Data)
{(void)S;(void)Bytes;xmqttClientWake(Data);}
static void xmqttClientDrain(xnetstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttClientClosed(xnetstream* S,xnetresult Result,const xerror* Cause,ptr Data)
{(void)S;(void)Result;xmqttclient* C=Data;C->NativeClosed=true;xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,Cause);xmqttClientWake(C);}
static const xnetstreameventsownershipv1 xmqttClientEvents={sizeof(xnetstreameventsownershipv1),
    {xmqttClientOpen,xmqttClientRead,xmqttClientEnd,NULL,xmqttClientLow,xmqttClientDrain,xmqttClientClosed},xmqttClientDrop,&xmqttPrivateOps};
static void xmqttClientTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xmqttclient* C=Data;
    if(C->Timer==Id){C->Timer=0;C->TimerAt=INFINITY;if(Result==XNET_RESULT_OK)xmqttClientWake(C);}
    xmqttClientDrop(C);
}
static void xmqttClientSchedule(xmqttclient* C,double At)
{
    if(C->Timer && C->TimerAt<=At)return;
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    C->TimerAt=At;if(!isfinite(At))return;
    xmqttClientRef(C);C->Timer=xrtNetEngineAfter(C->Engine,xrtNetWorkerIndex(C->Worker),xmqttClientRemaining(At),xmqttClientTimer,C);
    if(!C->Timer){xmqttClientDrop(C);xmqttClientFail(C,XMQTT_STATUS_LIMIT,xrtGetError());xmqttClientWake(C);}
}
static bool xmqttClientGrow(xmqttclient* C,size_t Capacity)
{
    if(Capacity<=C->InputCapacity)return true;
    xrtMutexLock(&C->Gate);
    if(Capacity>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Capacity;xrtMutexUnlock(&C->Gate);
    unsigned char* P=xrtMalloc(Capacity);
    if(!P){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Capacity;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    if(C->InputBytes)memcpy(P,C->Input,C->InputBytes);
    xrtFree(C->Input);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=C->InputCapacity;xrtMutexUnlock(&C->Gate);
    C->Input=P;C->InputCapacity=Capacity;return true;
}
static bool xmqttClientFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!C->Info.Ready){
        if(P->Type!=XMQTT_PACKET_CONNACK || !C->Startup || C->SetupOffset!=C->Startup->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttmessage* M=xmqttMessageCopy(C,P,&Failure);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=C->Config.Version;K.Codec=C->Config.Codec;
        xmqttnegotiated N;xmqttnegotiationissue Issue;
        xmqttnegotiationstatus S=xmqttNegotiate(&C->SetupPacket,&M->Packet,&K,&N,&Issue);
        if(S<0){xmqttMessageRelease(M);xmqttClientFail(C,S==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_STATUS_SESSION:S==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;}
        if(N.Accepted && N.SessionPresent){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}
        xmqttreadybox* B=C->ReadyBox;B->Value.Connack=M;B->Value.Reason=P->Reason;
        if(N.ClientId.Size && !N.ServerAssignedId)N.ClientId=(xstrview){B->Id,N.ClientId.Size};
        B->Value.Negotiated=N;C->Peer=N;
        if(S==XMQTT_NEGOTIATION_REJECTED){xmqttClientFail(C,XMQTT_STATUS_REJECTED,NULL);return false;}
        xrtMutexLock(&C->Gate);C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;xrtMutexUnlock(&C->Gate);
        C->NextPing=N.KeepAlive?xrtTimer()+N.KeepAlive:INFINITY;xmqttReadyFinish(C,XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PINGRESP){C->PingOutstanding=false;C->PingDeadline=INFINITY;xrtMutexLock(&C->Gate);++C->Info.Pongs;xrtMutexUnlock(&C->Gate);return true;}
    if(P->Type==XMQTT_PACKET_PUBACK){
        /* The codec already limits PUBACK properties to Reason String/User. */
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        if(!R || R->Qos!=1 || !R->Credit || R->Offset!=R->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        R->Acknowledged=true;R->Result->Value.Reason=P->Reason;
        R->Result->Value.Puback=xmqttMessageCopy(C,P,&Failure);
        if(!R->Result->Value.Puback){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,P->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_DISCONNECT){xrtMutexLock(&C->Gate);C->Info.ServerReason=P->Reason;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_SERVER_DISCONNECT,NULL);return false;}
    if(P->Type!=XMQTT_PACKET_PUBLISH){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if((P->Flags>>1)&3u){xmqttClientFail(C,XMQTT_STATUS_UNSUPPORTED,NULL);return false;}
    xmqttmessage* M=xmqttMessageCopy(C,P,&Failure);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    xrtMutexLock(&C->Gate);
    bool Full=C->Info.Messages==C->Config.MaxMessages || M->Box.Bytes>C->Config.MaxMessageBytes-C->Info.MessageBytes;
    if(!Full){if(C->MessageTail)C->MessageTail->Next=M;else C->MessageHead=M;C->MessageTail=M;++C->Info.Messages;C->Info.MessageBytes+=M->Box.Bytes;++C->Info.Received;}
    xrtMutexUnlock(&C->Gate);
    if(Full){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    xmqttClientNotify(C);return true;
}
static void xmqttClientReadPump(xmqttclient* C)
{
    size_t Read=0,Packets=0;
    while(!xmqttClientClosing(C) && Packets<C->Config.WorkPackets){
        size_t Needed=5;xmqttfixedheader H;xmqttwireconfig K={sizeof(K),C->Config.Version,XMQTT_PACKET_MAX_BYTES};
        xmqttwirestatus Header=xmqttHeaderRead((xbytesview){C->Input,C->InputBytes},&K,&H);
        if(Header==XMQTT_WIRE_ERROR){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,xrtGetError());return;}
        if(Header==XMQTT_WIRE_DONE){
            if(H.PacketBytes>C->Peer.ReceivePacketBytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return;}
            if(H.PacketBytes>C->Config.Codec.MaxPacketBytes){xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return;}
            Needed=H.PacketBytes;
            if(C->InputBytes>=Needed){xmqttpacket P;xmqttcodecissue Issue;xmqttcodecbuffers B={C->Props,C->Config.Codec.MaxProperties,NULL,0};
                xmqttcodecstatus S=C->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue):xmqttPacketRead311((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue);
                if(S!=XMQTT_CODEC_DONE){xmqttClientFail(C,S==XMQTT_CODEC_LOCAL_ERROR?XMQTT_STATUS_LIMIT:XMQTT_STATUS_PROTOCOL,xrtGetError());return;}
                if(!xmqttClientFrame(C,&P))return;
                C->InputBytes-=Needed;if(C->InputBytes)memmove(C->Input,C->Input+Needed,C->InputBytes);++Packets;continue;
            }
        }
        if(Needed>C->InputCapacity && !xmqttClientGrow(C,Needed))return;
        if(Read==C->Config.ReadBytes || !xrtNetStreamAvailable(C->Stream))break;
        size_t Count=C->InputCapacity-C->InputBytes;
        if(Count>C->Config.ReadBytes-Read)Count=C->Config.ReadBytes-Read;
        size_t N=xrtNetStreamRead(C->Stream,C->Input+C->InputBytes,Count);if(!N)break;
        Read+=N;C->InputBytes+=N;
    }
    if(Packets==C->Config.WorkPackets || (Read==C->Config.ReadBytes && xrtNetStreamAvailable(C->Stream)))xmqttClientWake(C);
}
static bool xmqttClientSendStartup(xmqttclient* C,size_t* Budget)
{
    xmqttstartup* W=C->Startup;
    while(W && C->SetupOffset<W->Bytes && *Budget){
        size_t N=W->Bytes-C->SetupOffset,Available=xrtNetStreamWritable(C->Stream);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xrtNetStreamSendRef(C->Stream,W->Data+C->SetupOffset,N,xmqttStartupRelease,W);
        if(S!=XNET_RESULT_OK){xmqttStartupDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        C->SetupOffset+=N;*Budget-=N;
    }
    if(W && C->SetupOffset<W->Bytes && !*Budget)xmqttClientWake(C);
    return W && C->SetupOffset==W->Bytes;
}
static bool xmqttClientSendPing(xmqttclient* C,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<2){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    static const unsigned char Ping[]={0xc0,0};
    while(C->PingOffset<2 && *Budget){
        size_t N=2-C->PingOffset,Available=xrtNetStreamWritable(C->Stream);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(!N)return false;
        xnetresult S=xrtNetStreamSend(C->Stream,Ping+C->PingOffset,N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(!C->PingOffset){C->PingOutstanding=true;C->PingDeadline=xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout);}
        C->PingOffset+=(unsigned)N;*Budget-=N;
    }
    if(C->PingOffset!=2){xmqttClientWake(C);return false;}
    C->PingOffset=0;C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    xrtMutexLock(&C->Gate);++C->Info.Pings;xrtMutexUnlock(&C->Gate);return true;
}
static void xmqttClientSendPump(xmqttclient* C)
{
    size_t Budget=C->Config.WriteBytes;
    if(!C->Info.Ready)return;
    /* Never insert a PUBLISH between the two bytes of a backpressured PING. */
    if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){
        xmqttrequest* R=*Link;
        if(R->Offset==R->Bytes){Link=&R->Next;continue;}
        if(R->Bytes>C->Peer.SendPacketBytes || R->Qos>C->Peer.MaximumQos || (R->Retain && !C->Peer.RetainAvailable)){xmqttRequestFinish(C,Link,XMQTT_STATUS_RESTRICTED);continue;}
        if(R->Qos && !R->Credit && C->SendInflight>=C->Peer.SendReceiveMaximum){Link=&R->Next;continue;}
        while(R->Offset<R->Bytes && Budget){size_t N=R->Bytes-R->Offset,Available=xrtNetStreamWritable(C->Stream);
            if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
            if(N>Budget)N=Budget;
            if(N>Available)N=Available;
            if(!N)return;
            xmqttRequestRef(R);(void)xrtAtomic32FetchAdd(&R->NativeRefs,1,XMEMORY_RELAXED);
            xnetresult S=xrtNetStreamSendRef(C->Stream,R->Wire+R->Offset,N,xmqttRequestNativeDrop,R);
            if(S!=XNET_RESULT_OK){(void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_RELAXED);xmqttRequestDrop(R);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return;}
            if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
            R->Offset+=N;Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        }
        if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;
    }
    if(!C->PingOutstanding && xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
}
static void xmqttClientCollect(xmqttclient* C)
{
    xrtMutexLock(&C->Gate);xmqttrequest* Head=C->SubmitHead;C->SubmitHead=C->SubmitTail=NULL;xrtMutexUnlock(&C->Gate);
    if(Head){if(C->RequestTail)C->RequestTail->Next=Head;else C->Requests=Head;
        while(Head){Head->Previous=C->RequestTail;C->RequestTail=Head;xmqttRequestMapInsert(C,Head);Head=Head->Next;}}
}
static void xmqttClientStop(xmqttclient* C)
{
    if(!C->Stopped){C->Stopped=true;
        if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
        (void)xrtNetStreamAbort(C->Stream);xmqttReadyFinish(C,C->Info.CloseStatus);xmqttStartupRetire(C);
    }
    size_t Count=0;
    while(C->Requests && Count++<C->Config.WorkPackets)xmqttRequestFinish(C,&C->Requests,C->Info.CloseStatus);
    if(C->Requests){xmqttClientWake(C);return;}
    xrtMutexLock(&C->Gate);bool Changed=C->Info.State!=XMQTT_CLIENT_CLOSED;C->Info.State=XMQTT_CLIENT_CLOSED;xrtMutexUnlock(&C->Gate);
    if(Changed)xmqttClientNotify(C);
}
static void xmqttClientWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xmqttclient* C=Data;
    xrtMutexLock(&C->Gate);C->Posted=false;bool Started=C->Started;xrtMutexUnlock(&C->Gate);
    if(!Started){xmqttClientDrop(C);return;}
    xmqttClientCollect(C);
    if(C->ReadyPromise && xrtCancelRequested(C->ReadyCancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    double Now=xrtTimer();
    if(!C->Info.Ready && Now>=C->SetupDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(C->PingOutstanding && Now>=C->PingDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){xmqttrequest* R=*Link;
        xmqttclientstatus S=xrtCancelRequested(R->Cancel)?XMQTT_STATUS_CANCELLED:Now>=R->Deadline?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK;
        if(S!=XMQTT_STATUS_OK){bool Partial=R->Offset!=0;xmqttRequestFinish(C,Link,S);if(Partial)xmqttClientFail(C,S,NULL);continue;}
        if(!R->Qos && R->Offset==R->Bytes && !xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE)){
            if(xrtNetStreamState(C->Stream)!=XNET_STREAM_OPEN || C->ReadEnded){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);break;}
            xmqttRequestFinish(C,Link,XMQTT_STATUS_OK);continue;
        }
        Link=&R->Next;
    }
    if(!xmqttClientClosing(C) && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN){
        /* Startup must be queued before an immediate/coalesced CONNACK. */
        if(!C->Info.Ready){size_t Budget=C->Config.WriteBytes;(void)xmqttClientSendStartup(C,&Budget);}
        if(!xmqttClientClosing(C))xmqttClientReadPump(C);
        if(!xmqttClientClosing(C) && !C->ReadEnded)xmqttClientSendPump(C);
        if(C->Info.Ready && !C->PingOutstanding && xrtTimer()>=C->NextPing)
            for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Offset && R->Offset<R->Bytes){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);break;}
    }
    if(C->ReadEnded || C->NativeClosed)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);
    if(xmqttClientClosing(C))xmqttClientStop(C);
    else {double At=C->Info.Ready?(C->PingOutstanding?C->PingDeadline:C->NextPing):C->SetupDeadline;
        for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Deadline<At)At=R->Deadline;
        xmqttClientSchedule(C,At);
    }
    xmqttClientDrop(C);
}

XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid client config output");
    *K=(xmqttclientconfig){0};K->Size=sizeof(*K);K->Version=XMQTT_VERSION_5;xrtNetStreamConfigInit(&K->Stream);xmqttCodecConfigInit(&K->Codec);
    K->MaxRequests=128;K->MaxOutgoingBytes=4u*1024u*1024u;K->MaxObjects=512;K->MaxObjectBytes=16u*1024u*1024u;K->MaxMessages=64;K->MaxMessageBytes=8u*1024u*1024u;
    K->MaxPrivateBytes=4u*1024u*1024u;K->MaxSetupBytes=64u*1024u;K->InitialInputBytes=1024;K->ReadBytes=K->WriteBytes=64u*1024u;K->WriteChunkBytes=16u*1024u;K->WorkPackets=64;K->SetupTimeout=K->PingTimeout=5000;return true;
}
static void xmqttClientFree(xmqttclient* C)
{
    xmqttmessage* M=C->MessageHead;while(M){xmqttmessage* Next=M->Next;xmqttMessageRelease(M);M=Next;}
    xrtNetStreamDestroy(C->Stream);xrtFutureDestroy(C->ReadyFuture);xrtCancelDestroy(C->ReadyCancel);xrtErrorFree(C->Cause);
    xrtFree(C->Input);xrtFree(xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_RELAXED));xmqttQuotaDrop(C->Quota);(void)xrtNetEngineUnpin(C->Engine);xrtMutexUnit(&C->Gate);xrtFree(C);
}
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out)
{
    double Now=xrtTimer();xmqttclientconfig K;if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid client config");K=*Config;}else xmqttClientConfigInit(&K);
    if(!Engine || !Remote || !Out || !Connect || !xrtMemRangeValid(Out,sizeof(*Out)) || !xrtMemRangeValid(Connect,sizeof(*Connect)) ||
       K.Size!=sizeof(K) || (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT ||
       !K.MaxRequests || K.MaxRequests>65536 || !K.MaxOutgoingBytes || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u || !K.MaxObjectBytes ||
       !K.MaxMessages || !K.MaxMessageBytes || !K.MaxSetupBytes || K.InitialInputBytes<5 || K.InitialInputBytes>K.Codec.MaxPacketBytes ||
       !K.ReadBytes || !K.WriteBytes || !K.WriteChunkBytes || !K.WorkPackets || K.SetupTimeout<0 || K.PingTimeout<=0)
        return xmqttClientError(XERR_ARGUMENT,"invalid MQTT client configuration");
    if(!xrtNetStreamConfigValid(&K.Stream))return false;
    size_t WireBytes=0;bool Valid=K.Version==XMQTT_VERSION_5?xmqttPacketSize5(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes):xmqttPacketSize311(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes);
    if(!Valid)return false;
    uint32 ReceiveBytes=XMQTT_PACKET_MAX_BYTES;bool ProblemInfo=true;
    for(size_t I=0;I<Connect->Properties.Count;++I){const xmqttproperty* P=Connect->Properties.Data+I;
        if(P->Id==XMQTT_PROPERTY_AUTH_METHOD || (P->Id==XMQTT_PROPERTY_TOPIC_ALIAS_MAXIMUM && P->Value.Number))return xmqttClientError(XERR_STATE,"enhanced authentication/receive aliases not implemented");
        if(P->Id==XMQTT_PROPERTY_REQUEST_PROBLEM_INFO)ProblemInfo=P->Value.Number!=0;
        if(P->Id==XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE){ReceiveBytes=P->Value.Number;if(ReceiveBytes>K.Codec.MaxPacketBytes)return xmqttClientError(XERR_ARGUMENT,"advertised receive packet size exceeds local codec limit");}
    }
    size_t SetupCount=Connect->Properties.Count+Connect->Body.Connect.WillProperties.Count;
    if(SetupCount>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty) || K.Codec.MaxProperties>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty)-SetupCount || WireBytes>K.MaxSetupBytes || WireBytes>SIZE_MAX-sizeof(xmqttstartup))return xmqttClientError(XERR_RANGE,"MQTT client setup capacity");
    size_t ClientBytes=sizeof(xmqttclient)+(SetupCount+K.Codec.MaxProperties)*sizeof(xmqttproperty),StartupBytes=sizeof(xmqttstartup)+WireBytes;
    if(ClientBytes>K.MaxPrivateBytes || K.InitialInputBytes>K.MaxPrivateBytes-ClientBytes || StartupBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes)return xmqttClientError(XERR_RANGE,"MQTT private setup budget");
    if(!xrtNetEnginePin(Engine))return false;
    xmqttclient* C=xrtMalloc(ClientBytes);if(!C){xrtNetEngineUnpin(Engine);return false;}memset(C,0,ClientBytes);
    if(!xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    C->Engine=Engine;C->Config=K;C->ProblemInfo=ProblemInfo;C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));xrtAtomic32Init(&C->Refs,1);
    xrtAtomicPtrInit(&C->SendFlow,NULL);
    C->Info.Size=sizeof(C->Info);C->Info.State=XMQTT_CLIENT_CONNECTING;C->Info.PrivateBytes=ClientBytes+K.InitialInputBytes+StartupBytes;C->Info.StartupBytes=WireBytes;
    C->SetupProps=C->Scratch;C->Props=C->Scratch+SetupCount;C->Peer.ReceivePacketBytes=ReceiveBytes;
    C->SetupDeadline=xmqttClientDeadline(Now,K.SetupTimeout);C->NextPing=C->PingDeadline=C->TimerAt=INFINITY;
    C->Quota=xrtMalloc(sizeof(*C->Quota));if(!C->Quota)goto Fail;memset(C->Quota,0,sizeof(*C->Quota));
    if(!xrtMutexInit(&C->Quota->Gate)){xrtFree(C->Quota);C->Quota=NULL;goto Fail;}
    xrtAtomic32Init(&C->Quota->Refs,1);C->Quota->MaxCount=K.MaxObjects;C->Quota->MaxBytes=K.MaxObjectBytes;
    C->Input=xrtMalloc(K.InitialInputBytes);if(!C->Input)goto Fail;C->InputCapacity=K.InitialInputBytes;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    C->ReadyBox=xmqttBoxAllocate(C->Quota,sizeof(xmqttreadybox)+Connect->Body.Connect.ClientId.Size,&Failure);if(!C->ReadyBox){if(Failure==XMQTT_STATUS_LIMIT)xmqttClientError(XERR_RANGE,"ready result budget");goto Fail;}
    C->ReadyBox->Box=(xmqttbox){C->Quota,sizeof(xmqttreadybox)+Connect->Body.Connect.ClientId.Size};
    if(Connect->Body.Connect.ClientId.Size)memcpy(C->ReadyBox->Id,Connect->Body.Connect.ClientId.Data,Connect->Body.Connect.ClientId.Size);
    C->Startup=xrtMalloc(StartupBytes);if(!C->Startup)goto Fail;C->Startup->Client=C;C->Startup->Bytes=WireBytes;xrtAtomic32Init(&C->Startup->Refs,1);xmqttClientRef(C);
    if(!(K.Version==XMQTT_VERSION_5?xmqttPacketWrite5(C->Startup->Data,WireBytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,NULL):xmqttPacketWrite311(C->Startup->Data,WireBytes,Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,NULL)))goto Fail;
    xmqttcodecbuffers B={C->SetupProps,SetupCount,NULL,0};
    if((K.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){C->Startup->Data,WireBytes},XMQTT_CLIENT_TO_SERVER,&K.Codec,&B,&C->SetupPacket,NULL):xmqttPacketRead311((xbytesview){C->Startup->Data,WireBytes},XMQTT_CLIENT_TO_SERVER,&K.Codec,&B,&C->SetupPacket,NULL))!=XMQTT_CODEC_DONE)goto Fail;
    C->ReadyPromise=xrtPromiseCreate(&C->ReadyFuture,NULL);if(!C->ReadyPromise)goto Fail;
    xmqttClientRef(C);if(!xrtPromiseProducerBindTakeV2(C->ReadyPromise,(xrtownershipref){C,&xmqttPrivateOps},&xmqttReadyProducer,&xmqttReadyCancellation)){xmqttClientDrop(C);goto Fail;}
    C->ReadyCancel=xrtPromiseCancelToken(C->ReadyPromise);if(!C->ReadyCancel || !xrtNetPostInit(&C->Post))goto Fail;
    /* Serialize startup publication against immediate native callbacks. */
    xrtMutexLock(&C->Gate);xmqttClientRef(C);C->Stream=xrtNetStreamConnectOwnedV1(Engine,Remote,Affinity,&K.Stream,C,&xmqttClientEvents);
    if(!C->Stream){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}
    C->Started=true;(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);*Out=C;return true;
Fail:
    xrtPromiseDestroy(C->ReadyPromise);C->ReadyPromise=NULL;if(C->ReadyBox)xmqttReadyFree(C->ReadyBox,NULL);xmqttStartupRetire(C);xmqttClientFree(C);return false;
}
XMQTT_API xfuture* xmqttClientReady(xmqttclient* C)
{if(!C){xmqttClientError(XERR_ARGUMENT,"client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<XRT_WAIT_FOREVER ||
       P->Type!=XMQTT_PACKET_PUBLISH || ((P->Flags>>1)&3u)>1 || (P->Flags&8u) || !P->Body.Publish.Topic.Size){xmqttClientError(XERR_ARGUMENT,"new QoS-0/1 publish arguments");return XMQTT_ADMISSION_ERROR;}
    uint8 Qos=(uint8)((P->Flags>>1)&3u);uint16 Identifier=0;xmqttpacket Adapted;const xmqttpacket* Packet=P;
    if(Qos){Adapted=*P;if(!Adapted.Identifier)Adapted.Identifier=1;Packet=&Adapted;} /* sizing only */
    size_t Bytes;bool Valid=C->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes):xmqttPacketSize311(Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes);
    if(!Valid)return XMQTT_ADMISSION_ERROR;
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){xmqttClientError(XERR_ARGUMENT,"outgoing alias not implemented");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest)){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    if(Qos){xmqttclientadmission Admission=xmqttIdentifierReserveLocked(C,P->Identifier,&Identifier);
        if(Admission!=XMQTT_ADMISSION_ACCEPTED){if(Admission==XMQTT_ADMISSION_OVERLOADED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
        Adapted.Identifier=Identifier;
    }
    ++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Bytes);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Retain=(P->Flags&1u)!=0;R->Qos=Qos;R->Identifier=Identifier;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    R->Result=xmqttBoxAllocate(C->Quota,sizeof(*R->Result),&Failure);if(!R->Result){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
    R->Result->Box=(xmqttbox){C->Quota,sizeof(*R->Result)};
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);goto Closed;}
    /* Publish no accepted request unless its worker wake is committed. The
     * post cannot run inline and cannot observe the queue until Gate unlocks. */
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Out=F;return XMQTT_ADMISSION_ACCEPTED;
Closed:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result)xmqttWriteFree(R->Result,NULL);xmqttRequestDrop(R);return XMQTT_ADMISSION_CLOSED;
Overload:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);xmqttRequestDrop(R);return XMQTT_ADMISSION_OVERLOADED;
Failed:
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result)xmqttWriteFree(R->Result,NULL);xmqttRequestDrop(R);return XMQTT_ADMISSION_ERROR;
UndoReservation:
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Identifier);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously publish from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientPublishAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* C,xmqttmessage** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid message output");
    xrtMutexLock(&C->Gate);xmqttmessage* M=C->MessageHead;
    if(M){C->MessageHead=M->Next;if(!C->MessageHead)C->MessageTail=NULL;--C->Info.Messages;C->Info.MessageBytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&C->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInfo(xmqttclient* C,xmqttclientinfo* Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid client info output");
    xrtMutexLock(&C->Gate);xmqttclientinfo I=C->Info;xrtMutexUnlock(&C->Gate);
    xrtMutexLock(&C->Quota->Gate);I.Objects=C->Quota->Count;I.ObjectBytes=C->Quota->Bytes;xrtMutexUnlock(&C->Quota->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientClose(xmqttclient* C)
{
    if(!C)return xmqttClientError(XERR_ARGUMENT,"client is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XMQTT_CLIENT_CLOSING){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=XMQTT_STATUS_CLOSED;}
    bool OK=xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);return OK;
}
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* C,int64 Timeout)
{
    if(!C || Timeout<XRT_WAIT_FOREVER){xmqttClientError(XERR_ARGUMENT,"invalid client wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot wait from an Engine worker");return XWAIT_ERROR;}
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    for(;;){xrtMutexLock(&C->Gate);bool Closed=C->Info.State==XMQTT_CLIENT_CLOSED && !C->Info.Requests && !C->Info.StartupBytes;xrtMutexUnlock(&C->Gate);
        if(Closed && xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE)==1){xrtownershipref S=xrtNetStreamOwnership(C->Stream);size_t Count=0;if(S.Ops && S.Ops->Count(S.Data,&Count) && Count==1)return XWAIT_OK;}
        if(xrtTimer()>=Deadline)return XWAIT_TIMEOUT;
        xrtSleep(1);
    }
}
XMQTT_API bool xmqttClientDestroy(xmqttclient* C)
{
    if(!C)return true;
    if(xrtNetEngineCurrent(C->Engine))return xmqttClientError(XERR_STATE,"cannot destroy from an Engine worker");
    if(!xmqttClientClose(C) || xmqttClientWaitFor(C,XRT_WAIT_FOREVER)!=XWAIT_OK)return false;
    xmqttClientFree(C);return true;
}
#endif
#endif

#endif
