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

typedef enum xmqttdirection { XMQTT_CLIENT_TO_SERVER = 1, XMQTT_SERVER_TO_CLIENT } xmqttdirection;

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
#endif

#endif

#if defined(XMQTT_IMPLEMENTATION) && !defined(XMQTT_IMPLEMENTATION_ONCE)
#define XMQTT_IMPLEMENTATION_ONCE 1


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

static bool xmqttWireFlags(xmqttpackettype Type, uint8 Flags, xmqttversion Version)
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

static xmqttwirestatus xmqttWireHeader(xbytesview Input, const xmqttwireconfig* Config, xmqttfixedheader* Header)
{
	if (Input.Size == 0) return XMQTT_WIRE_MORE;
	xmqttpackettype Type = (xmqttpackettype)(Input.Data[0] >> 4); uint8 Flags = Input.Data[0] & 15u;
	if (!xmqttWireFlags(Type, Flags, Config->Version)) {
		xmqttWireFail(XERR_PROTOCOL, XMQTT_WIRE_MALFORMED, "invalid MQTT packet type or fixed-header flags"); return XMQTT_WIRE_ERROR;
	}
	uint32 Remaining; size_t Bytes;
	xmqttwirestatus Status = xmqttWireVbi((xbytesview){Input.Data+1u, Input.Size-1u}, Config->Version, &Remaining, &Bytes);
	if (Status != XMQTT_WIRE_DONE) return Status;
	size_t PacketBytes = 1u + Bytes + Remaining;
	if (PacketBytes > Config->MaxPacketBytes) {
		xmqttWireFail(XERR_RANGE, XMQTT_WIRE_LIMIT, "MQTT total packet size exceeds configured limit"); return XMQTT_WIRE_ERROR;
	}
	*Header = (xmqttfixedheader){Type, Flags, Remaining, Bytes+1u, PacketBytes}; return XMQTT_WIRE_DONE;
}

XMQTT_API xmqttwirestatus xmqttHeaderRead(xbytesview Input, const xmqttwireconfig* Config, xmqttfixedheader* Header)
{
	xmqttwireconfig Copy;
	if (!xmqttWireHeaderArgs(Input, Config, Header, sizeof(*Header), &Copy)) return XMQTT_WIRE_ERROR;
	return xmqttWireHeader(Input, &Copy, Header);
}

XMQTT_API bool xmqttHeaderWrite(ptr Output, size_t Capacity, xmqttpackettype Type, uint8 Flags,
	uint32 RemainingLength, const xmqttwireconfig* Config, size_t* Written)
{
	xmqttwireconfig Copy;
	if (!xmqttWireConfig(Config, &Copy)) return false;
	if (!xmqttWireFlags(Type, Flags, Copy.Version)) return xmqttWireFail(XERR_ARGUMENT, XMQTT_WIRE_ARGUMENT, "invalid outgoing MQTT type or flags");
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
	xmqttwirestatus Status = xmqttWireHeader(Input, &Copy, &Header);
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

static bool xmqttPropertiesMeasure(const xmqttproperty* Properties, size_t Count, xmqttpropertyscope Scope,
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

static bool xmqttPropertiesOutput(const void* Output, size_t Size, const xmqttproperty* Properties,
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
	if (!xmqttPropertiesMeasure(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body) || !xmqttPropertiesOutput(Required,sizeof(*Required),Properties,Count,Config)) return false;
	*Required = Size; return true;
}

XMQTT_API bool xmqttPropertiesWrite(ptr Output, size_t Capacity, const xmqttproperty* Properties, size_t Count,
	xmqttpropertyscope Scope, xmqttdirection Direction, const xmqttpropertyconfig* Config, size_t* Written)
{
	xmqttpropertyconfig Limits; size_t Size; uint32 Body;
	if (Output == NULL || !xrtMemRangeValid(Output,Capacity)) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"invalid MQTT property wire buffer"); return false;
	}
	if (!xmqttPropertiesMeasure(Properties,Count,Scope,Direction,Config,&Limits,&Size,&Body)) return false;
	if (Capacity < Size) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_CAPACITY,"MQTT property wire capacity is too small"); return false;
	}
	if (!xmqttPropertiesOutput(Output,Size,Properties,Count,Config)) return false;
	if (Written != NULL && (!xmqttPropertiesOutput(Written,sizeof(*Written),Properties,Count,Config) || xrtMemRangesOverlap(Written,sizeof(*Written),Output,Size))) {
		xmqttPropertiesFail(XMQTT_PROPERTIES_LOCAL_ERROR,XMQTT_PROPERTY_ARGUMENT,"MQTT property Written overlaps wire or inputs"); return false;
	}
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
	if (Written != NULL) *Written = Size;
	return true;
}

#endif
#endif

#endif
