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
#ifndef XMQTT_MODULE_XMQTT_CLIENT_AUTH
#define XMQTT_MODULE_XMQTT_CLIENT_AUTH
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_DIAL
#define XMQTT_MODULE_XMQTT_CLIENT_DIAL
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_TLS
#define XMQTT_MODULE_XMQTT_CLIENT_TLS
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_TLS_RESUME
#define XMQTT_MODULE_XMQTT_CLIENT_TLS_RESUME
#endif
#ifndef XMQTT_MODULE_XMQTT_SESSION
#define XMQTT_MODULE_XMQTT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_INBOX
#define XMQTT_MODULE_XMQTT_CLIENT_INBOX
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION_DIAL
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION_DIAL
#endif
#endif

/* xmqtt_client_session_dial 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_SESSION_DIAL)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL
#define XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_DIAL
#define XMQTT_MODULE_XMQTT_CLIENT_DIAL
#endif
#endif

/* xmqtt_client_session_tls 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_SESSION_TLS)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS
#define XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_SESSION
#define XMQTT_MODULE_XMQTT_CLIENT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_TLS
#define XMQTT_MODULE_XMQTT_CLIENT_TLS
#endif
#endif

/* xmqtt_client_session 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_SESSION)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_SESSION
#define XMQTT_FEATURE_XMQTT_CLIENT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_INBOX
#define XMQTT_MODULE_XMQTT_CLIENT_INBOX
#endif
#ifndef XMQTT_MODULE_XMQTT_SESSION
#define XMQTT_MODULE_XMQTT_SESSION
#endif
#endif

/* xmqtt_client_inbox 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_INBOX)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_INBOX
#define XMQTT_FEATURE_XMQTT_CLIENT_INBOX
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT
#define XMQTT_MODULE_XMQTT_CLIENT
#endif
#endif

/* xmqtt_session 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_SESSION)
#ifndef XMQTT_FEATURE_XMQTT_SESSION
#define XMQTT_FEATURE_XMQTT_SESSION
#endif
#ifndef XMQTT_MODULE_XMQTT_NEGOTIATION
#define XMQTT_MODULE_XMQTT_NEGOTIATION
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#endif

/* xmqtt_client_tls_resume 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_TLS_RESUME)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME
#define XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_TLS
#define XMQTT_MODULE_XMQTT_CLIENT_TLS
#endif
#endif

/* xmqtt_client_tls 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_TLS)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_TLS
#define XMQTT_FEATURE_XMQTT_CLIENT_TLS
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT_DIAL
#define XMQTT_MODULE_XMQTT_CLIENT_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM_WRITE_PROGRESS
#define XRT_MODULE_TLS_STREAM_WRITE_PROGRESS
#endif
#ifndef XRT_MODULE_TLS_CLIENT_AUTH
#define XRT_MODULE_TLS_CLIENT_AUTH
#endif
#ifndef XRT_MODULE_TLS_CLIENT_RESUME
#define XRT_MODULE_TLS_CLIENT_RESUME
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_TLS_RECORD_CHACHA
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P384
#define XRT_MODULE_TLS_KEY_EXCHANGE_P384
#endif
#endif

/* xmqtt_client_dial 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_DIAL)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_DIAL
#define XMQTT_FEATURE_XMQTT_CLIENT_DIAL
#endif
#ifndef XMQTT_MODULE_XMQTT_CLIENT
#define XMQTT_MODULE_XMQTT_CLIENT
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* xmqtt_client_auth 及其直接依赖。 */
#if defined(XMQTT_MODULE_ALL) || defined(XMQTT_MODULE_XMQTT_CLIENT_AUTH)
#ifndef XMQTT_FEATURE_XMQTT_CLIENT_AUTH
#define XMQTT_FEATURE_XMQTT_CLIENT_AUTH
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
#ifndef XRT_MODULE_NET_TCP_RETIRE
#define XRT_MODULE_NET_TCP_RETIRE
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
#if defined(XMQTT_FEATURE_XMQTT_CLIENT) && (!defined(XMQTT_FEATURE_XMQTT_NEGOTIATION) || !defined(XRT_FEATURE_NET_TCP_RETIRE) || !defined(XRT_FEATURE_FUTURE))
#error "MQTT client requires negotiation, TCP retirement waiting and Future"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)
typedef struct xmqttclient xmqttclient;
typedef struct xmqttmessage xmqttmessage;
typedef enum xmqttclientstate {XMQTT_CLIENT_CONNECTING=0,XMQTT_CLIENT_OPEN,XMQTT_CLIENT_CLOSING,XMQTT_CLIENT_CLOSED} xmqttclientstate;
typedef enum xmqttclientstatus {
    XMQTT_STATUS_OK=0,XMQTT_STATUS_REJECTED,XMQTT_STATUS_TIMEOUT,XMQTT_STATUS_CANCELLED,
    XMQTT_STATUS_CLOSED,XMQTT_STATUS_TRANSPORT,XMQTT_STATUS_PROTOCOL,XMQTT_STATUS_LIMIT,
    XMQTT_STATUS_MEMORY,XMQTT_STATUS_UNSUPPORTED,XMQTT_STATUS_SESSION,
    XMQTT_STATUS_INITIALIZATION,XMQTT_STATUS_SERVER_DISCONNECT,XMQTT_STATUS_RESTRICTED,
    XMQTT_STATUS_AUTHENTICATION
} xmqttclientstatus;
typedef enum xmqttdelivery {XMQTT_NOT_SENT=0,XMQTT_MAY_HAVE_REACHED_PEER,XMQTT_PEER_ACCEPTED,XMQTT_PEER_REJECTED} xmqttdelivery;
typedef enum xmqttclientadmission {XMQTT_ADMISSION_ERROR=-1,XMQTT_ADMISSION_ACCEPTED=0,XMQTT_ADMISSION_OVERLOADED,XMQTT_ADMISSION_CLOSED} xmqttclientadmission;
/* A resolved Future is a typed outcome, not necessarily operation success.
 * QoS-0 OK observes complete native write-buffer retirement while open; it
 * does not acknowledge broker receipt, routing, storage or subscriber delivery.
 * Accepting ANY plaintext prefix into the selected TCP/TLS transport makes
 * delivery uncertain. AcceptedBytes counts that plaintext. Never auto-replayed.
 * QoS-1 completes after a corresponding PUBACK; PEER_ACCEPTED/REJECTED denotes
 * its decision, not subscriber delivery or durable storage. Puback (if present)
 * owns its reason/properties and may be separately retained. Local metadata
 * exhaustion can return LIMIT/MEMORY after an ACK, retaining its scalar decision.
 * Cause is borrowed from the result; retained results do not retain Client.
 * QoS2 keeps the first successful PUBREC (or terminating negative PUBREC) and
 * final PUBCOMP independently. Flags/reasons survive metadata allocation failure.
 * PEER_ACCEPTED denotes final protocol success, not subscriber/durable delivery.
 * A negative PUBCOMP (including id-not-found) does not prove non-delivery.
 * Negative PUBREC ends with REJECTED; otherwise completion requires PUBCOMP and
 * any partial repeated PUBREL frame. No timer-based PUBLISH/PUBREL retransmission,
 * reconnect or persistence. AcceptedBytes counts only the original PUBLISH.
 * The original four-field/Puback prefixes stay fixed; callers must rebuild for
 * the appended QoS2 metadata (write result 40 -> 64 bytes on supported x64). */
typedef struct xmqttwriteresult {
    xmqttclientstatus Status;xmqttdelivery Delivery;size_t AcceptedBytes;const xerror* Cause;
    uint16 Identifier;uint8 Reason;const xmqttmessage* Puback;
    const xmqttmessage *Pubrec,*Pubcomp;uint8 PubrecReason;bool PubrecReceived,PubcompReceived;
} xmqttwriteresult;
/* SUBACK/UNSUBACK is per filter, not an atomic operation. REJECTED can include
 * successful filters. Ack owns the ordered reason bytes and properties; it can
 * outlive Client/Engine and be separately retained. Without Acknowledged, any
 * AcceptedBytes means effects at the broker are uncertain. Scalar counts and
 * Acknowledged survive metadata LIMIT/MEMORY; Ack may then be NULL. MQTT 3.1.1
 * UNSUBACK has no per-filter reasons: all requested removals are acknowledged. */
typedef struct xmqttsubscriptionresult {
    xmqttclientstatus Status;size_t AcceptedBytes;const xerror* Cause;
    uint16 Identifier;bool Acknowledged;size_t Requested,Granted,Rejected;
    const xmqttmessage* Ack;
} xmqttsubscriptionresult;
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
    int64 CloseTimeout; /* Positive diagnostic drain deadline, default 5000 ms. */
} xmqttclientconfig;
typedef struct xmqttclientinfo {
    size_t Size;xmqttclientstate State;xmqttclientstatus CloseStatus;uint8 ServerReason;
    bool Ready;size_t Requests,OutgoingBytes,Messages,MessageBytes,Objects,ObjectBytes,PrivateBytes,StartupBytes;
    uint64 Accepted,Rejected,Completed,Received,ProtocolErrors,Pings,Pongs;
    /* Outgoing reason and bytes accepted into native output (0..3 for diagnostics).
     * Neither proves peer receipt. Diagnostic timeout preserves the original error. */
    uint8 LocalReason;bool DrainTimedOut,DrainStarted;size_t DisconnectBytes;
} xmqttclientinfo;
XRT_EXTERN_C_BEGIN
/* MQTT 5, finite codec defaults; 128 requests/4 MiB outgoing; 512 objects/16 MiB,
 * 64 queued messages/8 MiB; 4 MiB private, 64 KiB setup, 1 KiB initial input;
 * 64 KiB read/write work, 16 KiB chunks, 64 packets/turn; 5s setup/ping/diagnostic
 * close timeout. CloseTimeout must be positive; all durations use milliseconds. */
XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* Config);
/* One numeric TCP connection on a running host Engine, no TLS/DNS/fallback or
 * implicit replay. Optional client_dial supplies hostname/prepared-address setup.
 * CONNECT is fully encoded/copied before return; callbacks may
 * run before return. Setup timeout starts at factory entry. Caller owns one
 * unique Client; Close/Wait/Destroy it before destroying Engine. Output changes
 * only on success. Startup wire is wiped after handshake/stops AND its final
 * native reference. Caller/native/system copies are outside that contract.
 * This vertical slice supports basic CONNECT auth, CONNACK, keep-alive, QoS-0/1/2
 * receives/sends and server DISCONNECT. Incoming QoS-1 is acknowledged only after
 * an owned message enters the bounded local queue, independent of application
 * Take/Release. PUBACKs are ordered, bounded by min(MaxMessages, ReceiveMaximum),
 * lazily charged to private bytes, and never interleave a partial outgoing frame.
 * QoS2 enqueues once before PUBREC, retaining incoming id/credit through PUBCOMP
 * output. Active duplicate PUBLISH repeats PUBREC without delivery; repeated or
 * unknown PUBREL receives PUBCOMP (v5 reason 0x92 when unknown). Application Take
 * does not return protocol credit. A separate lazy 64 KiB id-state table and
 * 2*min(MaxMessages,ReceiveMaximum) four-byte ACK slots use MaxPrivateBytes;
 * duplicate ACKs coalesce while queued. This is volatile current-connection
 * delivery, not durable/business exactly-once. Graceful shutdown also drains
 * accepted incoming QoS2 flows; its finite deadline still applies.
 * MQTT5 CONNECT Topic Alias Maximum opts into incoming aliases (default 0).
 * A lazy direct-index table plus topic caches use MaxPrivateBytes through Destroy;
 * shorter/same-length rebindings reuse cache capacity, larger ones temporarily
 * charge both old/new entries. Empty-topic messages append an owned resolved
 * topic to their existing allocation, charged to object/message bytes. Packet
 * Topic is logical/resolved; Wire preserves the original alias-bearing frame.
 * Rebinding/Destroy never changes retained messages. Aliases are connection-only,
 * including when QoS2 duplicate delivery is suppressed. Enhanced authentication uses the optional
 * client_auth factory and ticket API;
 * Subscriptions operate on this connection; automatic resubscription, reconnect
 * and durable sessions remain separate pending work.
 * No state is restored: MQTT 5 SessionPresent without local state closes.
 * Explicit Maximum Packet Size must not exceed the local codec packet cap.
 * After accepted MQTT5 negotiation, protocol failures attempt a final minimal
 * diagnostic DISCONNECT at a frame boundary, bounded by CloseTimeout including
 * native queue drain. Partial frames, peer caps, EOF, resource failure or explicit
 * Close force abort instead. The original typed error survives every fallback. */
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Client);
/* Shared readiness Future, always typed. Explicit Future cancellation requests
 * Close; raw cancel-token requests are sampled on IO/deadline progress. */
XMQTT_API xfuture* xmqttClientReady(xmqttclient* Client);
/* Thread-safe finite admission, immutable preencoded copy. QoS-0/1/2 new PUBLISH
 * (DUP=0). MQTT5 Topic Alias opts into a logical-topic snapshot: a full topic
 * updates the latest admitted intent; an empty topic captures that intent or
 * fails admission if none exists. Cancellation/timeout of the earlier bind does
 * not retarget a later snapshot. The owner emits a compact frame only when the
 * actual wire mapping matches; otherwise it includes the captured full topic,
 * including when flow credit reorders publications. No original message is
 * replayed. Aliases must fit the server's negotiated maximum before sending.
 * Local/peer packet caps apply to the selected actual frame; retained snapshot
 * bytes use OutgoingBytes. Aligned alias-only request metadata, lazy 256-page
 * mapping tables and immutable retained topics use MaxPrivateBytes through
 * their actual lifetimes. Ordinary requests have no such tail/table allocation.
 * Native gathers borrow existing owned segments without another payload copy;
 * AcceptedBytes counts actual selected PUBLISH bytes (which may grow or shrink
 * versus the caller's encoded snapshot). Full-topic repeated calls can compact;
 * explicit empty references retain a single raw-buffer send when already bound.
 * Mappings are connection-only and independent from receive aliases.
 * QoS-1/2 Identifier=0 assigns
 * an unused id; nonzero reserves the explicit id, with normal OVERLOADED on
 * conflict. Its physical reservation lasts until real request retirement.
 * A bounded bitmap/hash table is allocated lazily on the first QoS-1/2 attempt,
 * charged to MaxPrivateBytes and retained through Destroy, even on later
 * admission failure. Peer Receive Maximum limits QoS-1/2 sends; QoS-0/control
 * traffic may continue while it is exhausted. QoS2 holds credit through PUBCOMP
 * or negative PUBREC, and repeats PUBREL only for duplicate successful PUBREC. May queue while connecting, but no
 * publish is sent before accepted CONNACK. Timeout includes encoding/queueing.
 * Future output changes on ACCEPTED only; normal overload/closed is counted.
 * Explicit Future cancellation wakes the owner; raw token sampled on progress.
 * Cancelling/timing out an already accepted prefix retires the connection. */
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Same operation/deadline; rejects ANY worker of the same Engine. */
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Immutable encoded SUBSCRIBE/UNSUBSCRIBE snapshot, with the same deadline,
 * cancellation, admission and physical id-reservation rules as PublishAsync.
 * Identifier=0 assigns a shared unused outgoing id. May queue before CONNACK;
 * negotiated packet/wildcard/shared/subscription-id limits apply before send.
 * Requested QoS may exceed MaximumQos. Controls do not consume publish credit.
 * Packet/Codec.MaxTopics bound the vector; no subscription registry is kept.
 * ACCEPTED returns an xmqttsubscriptionresult Future. A corresponding full ACK
 * is required; malformed/mismatched counts or excessive granted QoS close the
 * connection. Incoming publications can arrive before SUBACK or after UNSUBACK.
 * Caller inputs are borrowed only during this function; Future changes only on
 * ACCEPTED. No automatic replay or rollback of partially successful filters. */
XMQTT_API xmqttclientadmission xmqttClientSubscribeAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
XMQTT_API xmqttclientadmission xmqttClientUnsubscribeAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Blocking wrappers reject every worker of the same Engine. */
XMQTT_API xmqttclientadmission xmqttClientSubscribe(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
XMQTT_API xmqttclientadmission xmqttClientUnsubscribe(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Stop new admissions after accepted CONNACK, drain all accepted publishes and
 * subscription requests with their acknowledgements plus partial control frames,
 * then send this final DISCONNECT
 * and gracefully close TCP. NULL Packet selects normal reason 0. Version 311
 * forbids properties/nonzero reason. Negotiated packet/expiry limits apply.
 * Snapshot/encode before return; one request and its wire use the existing
 * request/outgoing/object quotas. OVERLOADED/CLOSED leave Future/error unchanged.
 * Timeout is finite >=0 ms from entry, covering admission, drain and native close;
 * 0 expires immediately. Cancellation/Close/errors abort without reopening.
 * ACCEPTED returns a typed xmqttwriteresult Future: OK means the native queue
 * drained and the selected transport closed normally (authenticated close for
 * TLS), NOT a broker ACK/Will-suppression guarantee.
 * AcceptedBytes and MAY_HAVE_REACHED_PEER are uncertain even on OK; Reason is
 * the requested reason, Identifier=0 and Puback=NULL. Failure preserves first
 * error and any accepted prefix. The result may outlive Client/Engine. WaitFor
 * additionally observes all actual references. Counters include this request. */
XMQTT_API xmqttclientadmission xmqttClientDisconnectAsync(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Same operation/deadline; blocking wrapper rejects every same-Engine worker. */
XMQTT_API xmqttclientadmission xmqttClientDisconnect(xmqttclient* Client,
    const xmqttpacket* Packet,int64 Timeout,xfuture** Future);
/* Transfer one queued message reference, false/unchanged on empty. Overfull
 * receive/object budgets close rather than silently dropping a packet. Already
 * queued messages remain available after Close; Destroy releases untaken ones.
 * Optional Client Inbox factories instead preserve their shared queue through
 * Client/Engine destruction; Take then consumes that same cross-Client queue.
 * Server DISCONNECT's reason is in Info, not added to the PUBLISH queue. */
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* Client,xmqttmessage** Message);
XMQTT_API const xmqttpacket* xmqttMessagePacket(const xmqttmessage* Message);
XMQTT_API bool xmqttMessageRetain(const xmqttmessage* Message);
XMQTT_API void xmqttMessageRelease(const xmqttmessage* Message);
XMQTT_API bool xmqttClientInfo(xmqttclient* Client,xmqttclientinfo* Info);
/* Abortive/nonblocking; application Close starts no MQTT DISCONNECT, so a broker
 * may publish Will. It can interrupt a previously started automatic error drain;
 * already accepted diagnostic bytes cannot be retracted. First error wins.
 * CLOSED is logical; Wait additionally observes real producer/post/timer/send/
 * event-owner and native stream reference retirement. Timeout preserves owner.
 * Wait uses completion signals without polling or wait-node allocation, with one
 * remaining timeout budget (native phase rounds to milliseconds). Concurrent
 * waiters borrow the same stable caller owner; stop all API entries before Destroy.
 * Wait/Destroy reject Engine workers. Repeated Close on CLOSED posts no new work. */
XMQTT_API bool xmqttClientClose(xmqttclient* Client);
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* Client,int64 Timeout);
XMQTT_API bool xmqttClientDestroy(xmqttclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_auth.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_AUTH_H
#define XMQTT_CLIENT_AUTH_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH) && !defined(XMQTT_FEATURE_XMQTT_CLIENT)
#error "MQTT enhanced authentication requires the client"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
typedef struct xmqttauthticket xmqttauthticket;
typedef enum xmqttauthphase {
    XMQTT_AUTH_CHALLENGE=0,XMQTT_AUTH_VERIFY_CONNECT,XMQTT_AUTH_VERIFY_REAUTH
} xmqttauthphase;
typedef struct xmqttauthconfig {
    size_t Size,MaxRounds,MaxExchangeBytes;
    int64 ResponseTimeout;
} xmqttauthconfig;
typedef struct xmqttauthresult {
    xmqttclientstatus Status;size_t AcceptedBytes;
    const xmqttmessage* Final;const xerror* Cause;
} xmqttauthresult;
XRT_EXTERN_C_BEGIN
/* Defaults: 32 server challenges, 1 MiB aggregate AUTH/final-CONNACK wire bytes,
 * 5000 ms for each application decision. SetupTimeout/reauth Timeout is the
 * original whole-exchange deadline and never restarts. CONNECT uses MaxSetupBytes.
 * All limits are positive, MaxRounds <= 65536; ResponseTimeout is finite. */
XMQTT_API bool xmqttAuthConfigInit(xmqttauthconfig* Config);
/* MQTT5 only, explicit CONNECT Authentication Method required (including empty).
 * Copies config/method/CONNECT. Algorithms and credential generation belong to
 * the application. Ordinary xmqttClientConnect continues to reject enhanced auth.
 * A successful CONNACK is presented for local verification before Ready resolves.
 * Application sends and TakeMessage stay gated until accepted. Legitimate early
 * publications remain in the bounded queue; control ACKs/keepalive still progress.
 * No reconnect, implicit replay, TLS or authentication algorithm is implied. */
XMQTT_API bool xmqttClientConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
/* Nonblocking mailbox using the existing Notify callback. Transfer one unique
 * ticket reference; false/unchanged when none. Move it to an application thread
 * without blocking the IO worker. One decision/response per ticket; duplicates
 * are rejected. Ticket pins Client: release every taken ticket before WaitFor/
 * Destroy. Message may be separately retained and then outlive Client/Engine.
 * Releasing without an answer leaves the finite decision timeout in effect. */
XMQTT_API bool xmqttClientTakeAuth(xmqttclient* Client,xmqttauthticket** Ticket);
XMQTT_API xmqttauthphase xmqttAuthPhase(const xmqttauthticket* Ticket);
XMQTT_API const xmqttmessage* xmqttAuthMessage(const xmqttauthticket* Ticket);
XMQTT_API void xmqttAuthRelease(xmqttauthticket* Ticket);
/* CHALLENGE only: AUTH reason 0x18 with exactly the original method. Immutable
 * encoded snapshot; ACCEPTED means queued, not transmitted. Control wire uses
 * MaxPrivateBytes independently from saturated ordinary request quota. Tickets,
 * messages/results use MaxObjects/MaxObjectBytes. OVERLOADED/CLOSED preserve
 * caller error. Resource/encoding refusals leave ticket state unchanged. An
 * expired decision returns ERROR and closes with typed TIMEOUT. */
XMQTT_API xmqttclientadmission xmqttAuthRespond(xmqttauthticket* Ticket,const xmqttpacket* Response);
/* Accept only VERIFY_* final proof; reject any phase. Schedules a local decision,
 * sends no client AUTH-success packet. Rejection closes with AUTHENTICATION.
 * A completed/stale ticket changes no state. May be called from Notify. */
XMQTT_API xmqttclientadmission xmqttAuthComplete(xmqttauthticket* Ticket,bool Accept);
/* One exchange on an already verified connection. AUTH reason 0x19, same method;
 * finite Timeout >= 0 ms covers queueing, all rounds and final application proof.
 * Other traffic continues with prior authentication; graceful Disconnect is
 * OVERLOADED while authentication is active. Cancellation/timeout/rejection closes
 * the connection; no replay. Success-only output is typed xmqttauthresult and
 * independent from Client/Engine after resolution. AcceptedBytes is the sum of
 * this exchange's AUTH prefixes accepted by TCP, not authentication success.
 * Retaining old tickets/results remains charged through actual retirement. */
XMQTT_API xmqttclientadmission xmqttClientReauthenticateAsync(xmqttclient* Client,
    const xmqttpacket* Start,int64 Timeout,xfuture** Future);
/* Same operation, rejects every worker of the host Engine before waiting. */
XMQTT_API xmqttclientadmission xmqttClientReauthenticate(xmqttclient* Client,
    const xmqttpacket* Start,int64 Timeout,xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_dial.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_DIAL_H
#define XMQTT_CLIENT_DIAL_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "MQTT Dial requires client and net_tcp_dial"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
#define XMQTT_DIAL_ADDRESSES_MAX 64u
#define XMQTT_DIAL_HOST_MAX 65535u
/* Network policy only. Config.Stream remains the only TCP buffer/timeout policy.
 * Defaults: dual stack, 30000ms additional Dial timeout, 250ms fallback, eight
 * candidates, 1024 host bytes. Timeout=0 disables the additional Dial timer;
 * -1 is infinite. The finite original SetupTimeout still spans all setup. */
typedef struct xmqttclientdialconfig {
    size_t Size;xnetfamily Family;int64 Timeout,FallbackDelay;
    uint32 MaxAttempts;size_t MaxHostBytes;
} xmqttclientdialconfig;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientDialConfigInit(xmqttclientdialconfig* Config);
/* Bounded copied nonempty host view, no embedded NUL, nonzero port. Resolver is
 * borrowed through physical Client retirement. Engine/Resolver remain alive;
 * Close/Wait/Destroy the unique Client before their destruction. Output changes
 * only on success and must not overlap Dial or borrowed host/endpoint storage.
 * CONNECT/config are copied before return. Ready reports typed DNS/connect/setup
 * failures. Cancel/Close wakes the owner; physical Wait includes all client
 * callbacks and losing candidates. A detached shared DNS job may continue under
 * Resolver ownership; Resolver Destroy joins it. No request/session replay or reconnect.
 * Initial host scratch uses MaxPrivateBytes and retires before return; native
 * Dial/Resolver/Stream storage uses their own bounded Core configurations. */
XMQTT_API bool xmqttClientDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,
    uint16 Port,uint64 Affinity,const xmqttclientconfig* Config,
    const xmqttclientdialconfig* Dial,const xmqttpacket* Connect,xmqttclient** Client);
/* Immutable prepared endpoints, no Resolver/DNS. At most64, nonzero eligible
 * ports; Family filters and MaxAttempts bounds selected attempts. The list is
 * retained by Core during admission; caller may release it immediately. */
XMQTT_API bool xmqttClientDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* Same Dial/setup deadline with the enhanced-auth ticket and final-proof gates. */
XMQTT_API bool xmqttClientDialAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,
    uint16 Port,uint64 Affinity,const xmqttclientconfig* Config,
    const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesAuth(xnetengine* Engine,xnetaddrlist* Addresses,
    uint64 Affinity,const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,
    const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_tls.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_TLS_H
#define XMQTT_CLIENT_TLS_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL) || !defined(XRT_FEATURE_TLS_STREAM_WRITE_PROGRESS) || !defined(XRT_FEATURE_TLS_CLIENT_AUTH) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME))
#error "MQTT TLS requires Dial, native TLS write progress, verified/resumable client and client authentication"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS) && (!defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384))
#error "MQTT TLS requires its declared native cipher, schedule and key-exchange backends"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
/* Borrowed only during the factory. Native Session snapshots names/ALPN and
 * retains Context/Verifier/Resume and optional ClientAuth.Identity before DNS.
 * Their callback/code/context lifetime follows the native shared-object contract
 * through physical Client retirement. Library code stays resident through Engine
 * join before unloading. Signers run on the connection Worker.
 * Defaults: native TLS policy/limits, Dial defaults,64 KiB metadata, infer SNI
 * and VerifyName from host; numeric literals infer VerifyName without IP SNI.
 * Prepared/numeric endpoints require an explicit Session peer identity.
 * MaxMetadataBytes bounds names, ALPN view descriptors and ALPN bytes together;
 * native Session/ciphertext storage uses its separate bounded Core limits.
 * ClientAuth selects native full-handshake mutual authentication; its current
 * native contract rejects explicit resumption. No private-key backend is forced.
 * Layout is independent of other compile-time selections. */
typedef struct xmqttclienttlsconfig {
    size_t Size;xtlsclientconfig Session;xtlsstreamconfig Stream;
    xmqttclientdialconfig Dial;size_t MaxMetadataBytes;bool ServerNameFromHost;
    const xtlsclientauthconfigv1* ClientAuth;
} xmqttclienttlsconfig;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientTlsConfigInit(xmqttclienttlsconfig* Config);
/* TLS required; no cleartext fallback or request replay. Config/CONNECT/views
 * snapshot before return; output changes only on success. Original SetupTimeout
 * spans admission, DNS, TCP, TLS and MQTT authentication/CONNACK. Ready succeeds
 * only after all configured gates. Resolver is borrowed through Client retirement.
 * AcceptedBytes counts MQTT plaintext accepted by TLS, conservatively uncertain
 * even before ciphertext reaches TCP. QoS0 OK observes its saved native ciphertext
 * prefix retirement while open. ACK results may precede that physical duty;
 * request quota/id reservations remain held until its real prefix/close outcome.
 * TLS requests add one aligned fence tail (24 bytes plus padding on x64) under MaxPrivateBytes, with no
 * per-chunk watch allocation. Native ciphertext is not a second plaintext copy.
 * Each TLS owner turn shares WriteBytes plaintext and WorkPackets send-call
 * budgets across startup, control and application output.
 * Graceful Disconnect includes authenticated TLS close; it is bounded by its
 * original deadline. Close/Wait/Destroy retain all actual callback/session/native
 * tails, including final verifier/identity releases. Public Client layouts stay fixed. */
XMQTT_API bool xmqttClientTlsConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDial(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddresses(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* Same TLS factories with the MQTT5 challenge/final-proof/reauthentication gates.
 * TLS mutual authentication and MQTT enhanced authentication are independent. */
XMQTT_API bool xmqttClientTlsConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_tls_resume.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_TLS_RESUME_H
#define XMQTT_CLIENT_TLS_RESUME_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME) && !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#error "MQTT TLS resume handoff requires the TLS client"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
/* Thread-safe observation, independent of MQTT CONNACK/authentication readiness.
 * Established/Resumed describe the authenticated TLS handshake and remain valid
 * after close. Enabled requires native ResumeLimit>0 and no ClientAuth (the
 * native mutual-authentication client currently disables ticket publication).
 * One latest TLS1.3 credential is cached. Captured counts objects taken from
 * the native queue; Evicted counts replaced cache entries and Rejected counts
 * local byte-budget refusals. Native queue drops/OOM are outside these counters.
 * CachedBytes counts ticket/secret/name/ALPN/identity bytes plus peer-chain DER;
 * it is charged to MaxPrivateBytes while cached. Native opaque metadata and its
 * bounded queue retain their separate Core limits. Counters saturate.
 * No public layout depends on optional module selections. Size is output. */
typedef struct xmqttclienttlsresumeinfo {
    size_t Size,CachedBytes;uint64 Captured,Evicted,Rejected;
    bool Enabled,Established,Resumed;
} xmqttclienttlsresumeinfo;
XRT_EXTERN_C_BEGIN
XMQTT_API bool xmqttClientTlsResumeInfo(xmqttclient* Client,xmqttclienttlsresumeinfo* Info);
/* Transfers the cached native reference to the caller without allocation/copy.
 * Any thread may call with a stable borrowed Client, including Config.Notify
 * on its Worker or after ClientWait. False/empty preserves Resume and ambient
 * error; invalid arguments/non-TLS clients report an error without output change.
 * Notify coalesces ticket changes in existing Client work, outside native TLS
 * callbacks. Take refunds the local cached-byte charge; the caller now owns
 * credential storage/budget and must xrtTlsResumeRelease it. It may outlive
 * Client/Engine and be supplied as another factory's Session.Resume. That factory
 * retains it before DNS. Keep Core code resident through its final release.
 * This is a TLS credential, not MQTT session state or permission to replay any
 * request. Each connection still performs CONNECT/CONNACK/authentication. There
 * is no automatic reconnect, persistence or 0-RTT. Native ResumeOnly controls
 * whether TLS may fall back to a full handshake; identity/ALPN/expiry validation
 * and certificate policy remain native. Credentials/secrets must not be logged. */
XMQTT_API bool xmqttClientTlsTakeResume(xmqttclient* Client,xtlsresume** Resume);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/session.h */
/* ========================================================================== */

#ifndef XMQTT_SESSION_H
#define XMQTT_SESSION_H
#if defined(XMQTT_FEATURE_XMQTT_SESSION) && !defined(XMQTT_FEATURE_XMQTT_NEGOTIATION)
#error "MQTT session requires negotiation and both codecs"
#endif
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
typedef struct xmqttsession xmqttsession;
typedef struct xmqttsessionpacket xmqttsessionpacket;
typedef enum xmqttsessionstatus {
    XMQTT_SESSION_RESTRICTED=-4,XMQTT_SESSION_ERROR=-3,
    XMQTT_SESSION_STATE_ERROR=-2,XMQTT_SESSION_PROTOCOL_ERROR=-1,
    XMQTT_SESSION_EMPTY=0,XMQTT_SESSION_DONE=1,XMQTT_SESSION_LIMIT,
    XMQTT_SESSION_DUPLICATE,XMQTT_SESSION_REJECTED,XMQTT_SESSION_RESET
} xmqttsessionstatus;
typedef enum xmqttsessionstage {
    XMQTT_SESSION_PUBLISH=0,XMQTT_SESSION_PUBREL,
    XMQTT_SESSION_COMPLETE,XMQTT_SESSION_DISCARDED
} xmqttsessionstage;
typedef struct xmqttsessionconfig {
    size_t Size;xmqttversion Version;xstrview ClientId;xmqttcodecconfig Codec;
    size_t MaxOutgoing,MaxIncoming,MaxBytes;
} xmqttsessionconfig;
typedef struct xmqttsessioninfo {
    size_t Size;xmqttversion Version;bool Connected,HasState;
    size_t ActiveOutgoing,Objects,LiveBytes,BaseBytes,Incoming,Prepared;
    uint16 SendQuota,InitialSendQuota;uint64 Generation;double ExpiresAt; /* Nominal display deadline; expiry uses elapsed seconds. */
} xmqttsessioninfo;
typedef struct xmqttsessionpacketinfo {
    size_t Size;uint16 Identifier;uint8 Qos,Reason,PubrecReason;
    bool Active,Sent,PubrecReceived;xmqttsessionstage Stage;size_t WireBytes;
} xmqttsessionpacketinfo;
/* One retained Packet reference is transferred by Next. Prefix/Tail are
 * immutable slices of that object, even after End, ACK, Reset or Destroy.
 * Do not interleave frames. Sent finishes this exact generation's selection;
 * Generation/Selection identify this exact selection, including repeated
 * PUBREL selections in one connection. Do not reuse an earlier token.
 * End abandons a partial selection. Release Packet once its actual native
 * writes no longer borrow these slices. Releasing early cannot free the
 * session's outstanding selection, but leaves Next blocked until Sent/End. */
typedef struct xmqttsessionframe {
    size_t Size;const xmqttsessionpacket* Packet;uint64 Generation;
    xmqttpackettype Type;uint16 Identifier;xbytesview Prefix,Tail;uint64 Selection;
} xmqttsessionframe;
XRT_EXTERN_C_BEGIN
/* Caller-driven in-memory protocol state, not a transport, retry timer, Future
 * facade or durable store. The optional Client Session factory attaches it;
 * ordinary Client factories retain their connection-local state contract.
 * Mutating operations require one serialized protocol owner. Info, packet
 * getters and Retain/Release may run on other threads with stable references.
 * No callback, thread, Engine pin or credential is owned. The application must
 * bind this object to one broker/security principal and its copied ClientId. */
XMQTT_API bool xmqttSessionConfigInit(xmqttsessionconfig* Config);
/* Defaults: MQTT5,128 outgoing,64 incoming QoS2,16 MiB including all live owned
 * allocations and metadata. Set a nonempty valid ClientId before Create.
 * Fixed id/hash/receive maps are allocated at Create and included in BaseBytes;
 * zero limits disable their maps. Retained completed packets keep byte/object/
 * identifier reservations until their last physical reference is released.
 * Config/descriptors/views borrow only during the call. Outputs are caller
 * storage, disjoint from all inputs and library-owned views. */
XMQTT_API bool xmqttSessionCreate(const xmqttsessionconfig* Config,xmqttsession** Session);
/* Call only after authenticating the accepted connection. Uses independent
 * CONNECT/CONNACK negotiation; identity/version must match. DONE/RESET opens
 * a new generation, resets connection-only send quota and schedules only the
 * required old PUBLISH/PUBREL retries. No live-connection timer retransmission.
 * MQTT5 SessionPresent=0 discards old state (RESET). Missing local state with
 * SessionPresent=1 fails closed. A 311 missing-server-state mismatch also fails
 * closed; use an explicit offline Reset/clean connection to discard it.
 * Rejection/errors preserve the session and Negotiated follows negotiation's
 * output contract. Now is finite nonnegative monotonic seconds, never backwards.
 * Expiry is owner-driven by Open/Expire; it is never applied while connected. */
XMQTT_API xmqttsessionstatus xmqttSessionOpen(xmqttsession* Session,
    const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Negotiated);
/* Call at actual network termination. Disconnect is NULL for an abort; pass
 * a descriptor only if that complete valid DISCONNECT was sent. Original-zero
 * expiry restrictions apply. No native references are released on your behalf.
 * Finite MQTT5 expiry starts here; UINT32_MAX/311 persistent state has no timer.
 * Prepared, uncommitted incoming deliveries are rolled back. */
XMQTT_API xmqttsessionstatus xmqttSessionEnd(xmqttsession* Session,
    const xmqttpacket* Disconnect,double Now);
XMQTT_API xmqttsessionstatus xmqttSessionExpire(xmqttsession* Session,double Now);
/* Explicit administrative termination, offline only. Held immutable packets
 * remain readable and charged until released. Never silently reset a live session. */
XMQTT_API xmqttsessionstatus xmqttSessionReset(xmqttsession* Session);
/* Connected-only new QoS1/2, DUP=0, owned immutable canonical packet snapshot.
 * Identifier0 assigns an unused id; explicit collision is normal LIMIT.
 * Full logical topic required. MQTT5 Topic Alias is validated then removed:
 * connection-only mappings are never persisted or retried. One payload copy.
 * ACCEPT(DONE) transfers a packet reference; LIMIT/other failures leave output
 * unchanged. A held packet does not pin an Engine or connection.
 * Message Expiry remains the publisher-supplied interval, not an offline queue
 * TTL; this is client-originated retransmission, not server onward forwarding. */
XMQTT_API xmqttsessionstatus xmqttSessionPublish(xmqttsession* Session,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
/* One outstanding frame selection. Empty/quota exhaustion preserves output.
 * PUBREL progress bypasses exhausted PUBLISH quota. New/retried PUBLISH use
 * DUP0/DUP1 respectively; PUBREL always uses fixed flags2 and original id.
 * A resumed peer's packet/QoS/retain restrictions fail without dropping state.
 * Selection numbers never wrap within a generation; exhaustion is RESTRICTED
 * without consuming the ready frame or changing output. End/Open starts a new
 * generation. Sent rejects stale selection tokens without changing state. */
XMQTT_API xmqttsessionstatus xmqttSessionNext(xmqttsession* Session,xmqttsessionframe* Frame);
XMQTT_API xmqttsessionstatus xmqttSessionSent(xmqttsession* Session,const xmqttsessionframe* Frame);
/* After full frame validation, correlate PUBACK/PUBREC/PUBCOMP. Negative ACKs
 * retire the flow; successful PUBREC schedules PUBREL, never PUBLISH again.
 * Duplicate successful PUBREC coalesces another PUBREL. Matching PUBCOMP after
 * reconnection can replenish send quota up to its new initial maximum even
 * when that generation sent no corresponding PUBLISH.
 * PUBCOMP is ignored without credit until a matching PUBREL was emitted;
 * other unknown/wrong-stage ACKs are protocol errors. No ACK metadata/payload
 * is copied here. */
XMQTT_API xmqttsessionstatus xmqttSessionAcknowledge(xmqttsession* Session,const xmqttpacket* Ack);
/* Incoming QoS2 identity storage, independent of outgoing ids. The transport
 * validates packet/alias/ReceiveMaximum and reserves local delivery resources
 * before Prepare. DONE reserves a new id, DUPLICATE suppresses another delivery.
 * Commit is allocation-free and must precede making the already prepared local
 * delivery visible and emitting PUBREC; Abort rolls back only a prepared id.
 * These functions do not store application payload or prove durable acceptance. */
XMQTT_API xmqttsessionstatus xmqttSessionReceivePrepare(xmqttsession* Session,uint16 Identifier);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveCommit(xmqttsession* Session,uint16 Identifier);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveAbort(xmqttsession* Session,uint16 Identifier);
/* Received validated PUBREL: DONE sets PUBCOMP reason0; unknown id sets reason
 * 0x92 in v5 (0 in311), EMPTY. Output changes only for those two outcomes.
 * PubcompSent removes known state only after complete PUBCOMP emission. */
XMQTT_API xmqttsessionstatus xmqttSessionReceiveRelease(xmqttsession* Session,uint16 Identifier,uint8* Reason);
XMQTT_API xmqttsessionstatus xmqttSessionReceiveComplete(xmqttsession* Session,uint16 Identifier);
XMQTT_API bool xmqttSessionInfo(xmqttsession* Session,xmqttsessioninfo* Info);
XMQTT_API const xmqttpacket* xmqttSessionPacket(const xmqttsessionpacket* Packet);
XMQTT_API bool xmqttSessionPacketInfo(const xmqttsessionpacket* Packet,xmqttsessionpacketinfo* Info);
/* Successfully held packet references share a UINT32_MAX/2 cap, including
 * active/selected state and caller frames. Next speculatively adds two internal
 * references, checks and refunds them on refusal before changing ready/quota/
 * selection/output state. Its protected active reference and sole protocol
 * owner bound this internal count to cap+2; no overflowing frame is published.
 * Capacity may be briefly reserved by such a pending Next. Insufficient room
 * is RESTRICTED; releases allow retry. Retain uses CAS at the same public cap.
 * A stable live reference is required on entry. */
XMQTT_API bool xmqttSessionPacketRetain(const xmqttsessionpacket* Packet);
XMQTT_API void xmqttSessionPacketRelease(const xmqttsessionpacket* Packet);
/* Offline unique-owner destruction; stop concurrent Session entries first.
 * Retained packets may outlive it; they keep only bounded state/allocator code.
 * A connected session must first End, so destruction cannot hide live state loss. */
XMQTT_API bool xmqttSessionDestroy(xmqttsession* Session);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_inbox.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_INBOX_H
#define XMQTT_CLIENT_INBOX_H
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
 * Optional client_session and client_session_tls factories add protocol recovery
 * and TLS/Dial/Auth composition while retaining these passive Inbox bounds. */
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


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_session.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_SESSION_H
#define XMQTT_CLIENT_SESSION_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
#endif
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
 * Optional client_session_dial and client_session_tls compose Dial/TLS. Enhanced
 * authentication is selected independently. Assigned-id adoption, automatic
 * reconnect and durable recovery remain separate
 * work. No production/performance acceptance implied. */
XMQTT_API bool xmqttClientConnectSession(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT5 enhanced authentication on numeric TCP. Auth tickets, challenge rounds,
 * finite decision/setup deadlines and final proof follow Client Auth. Session
 * opening/reset/replay waits for local initial proof acceptance. Pre-acceptance
 * failure preserves offline state and expiry; after acceptance a local replay
 * failure can follow an authenticated reset/resume before Ready succeeds.
 * Auth/CONNECT are copied; credentials are not stored in the passive Session.
 * Reauthentication uses the existing connection and does not reopen Session. */
XMQTT_API bool xmqttClientConnectSessionAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttauthconfig* Auth,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_session_tls.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_SESSION_TLS_H
#define XMQTT_CLIENT_SESSION_TLS_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS))
#error "MQTT TLS Session requires Client Session and TLS"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS)
XRT_EXTERN_C_BEGIN
/* Explicit persistent protocol Session/Inbox with mandatory authenticated TLS.
 * Identity/codec/quota/exclusive claims follow Client Session; metadata snapshots,
 * verifier/identity retention, one entry setup deadline and actual ciphertext
 * retirement follow Client TLS. Caller binds passive state to one logical
 * broker/security principal. TLS credentials and native sessions are not stored
 * in the MQTT Session. Pre-acceptance failures preserve old state and expiry.
 * After MQTT/proof acceptance, local replay failure may follow a reset/resume.
 * Constructor output changes only on success; Open/replay waits for all gates.
 * Each reconnection creates a new TLS connection, explicitly after old Client
 * Destroy; optional TLS resumption is independent from MQTT SessionPresent.
 * QoS1/2 retain one canonical plaintext snapshot. TLS record encryption/copying,
 * request fence tail, ciphertext buffers and native retirement still have costs.
 * No cleartext fallback, automatic reconnect or durable store is implied. */
XMQTT_API bool xmqttClientTlsConnectSession(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialSession(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesSession(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT5 challenge/final-proof/reauthentication is independent of TLS mutual
 * authentication. Session opening and replay wait for initial proof acceptance. */
XMQTT_API bool xmqttClientTlsConnectSessionAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialSessionAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientTlsDialAddressesSessionAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclienttlsconfig* TLS,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xmqtt/include/xmqtt/client_session_dial.h */
/* ========================================================================== */

#ifndef XMQTT_CLIENT_SESSION_DIAL_H
#define XMQTT_CLIENT_SESSION_DIAL_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL) && (!defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION) || !defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL))
#error "MQTT Dial Session requires Client Session and Dial"
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
XRT_EXTERN_C_BEGIN
/* Explicit TCP Session/Inbox with hostname or prepared endpoints. All passive
 * ownership, identity, replay and physical-retirement rules follow Client Session.
 * Dial copies host/config/CONNECT and retains prepared endpoints before return.
 * Resolver stays alive through physical Client retirement; detached shared DNS
 * jobs remain Resolver-owned. One entry setup deadline spans DNS, TCP and MQTT,
 * including application proof when Auth is selected. Failed setup/denied proof
 * preserves offline state/expiry. Caller binds all endpoints to the same broker/
 * security principal; fallback is connection setup, not automatic reconnect.
 * Native Dial/Resolver bounds remain separate from MQTT private/request bounds.
 * Output changes only on success. Neither passive owner stores credentials.
 * TCP has no transport encryption; select client_session_tls for TLS. */
XMQTT_API bool xmqttClientDialSession(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesSession(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,xmqttsession* Session,xmqttclientinbox* Inbox,
    const xmqttpacket* Connect,xmqttclient** Client);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* MQTT5 challenge/final-proof/reauthentication follow Client Auth. Application
 * final proof gates Session opening and replay; all deadlines share factory entry. */
XMQTT_API bool xmqttClientDialSessionAuth(xnetengine* Engine,xnetresolver* Resolver,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
XMQTT_API bool xmqttClientDialAddressesSessionAuth(xnetengine* Engine,xnetaddrlist* Addresses,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const xmqttauthconfig* Auth,
    xmqttsession* Session,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Client);
#endif
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
/* Optional allocation-independent reason for alias zero/repetition. Other
 * errors leave it zero. Public property read statuses/outputs stay unchanged. */
XMQTT_INTERNAL xmqttpropertystatus xmqttInternalPropertiesRead(xbytesview Input,
    xmqttpropertyscope Scope,xmqttdirection Direction,const xmqttpropertyconfig* Config,
    xmqttproperty* Properties,size_t Capacity,xmqttpropertydocument* Document,uint8* AliasReason);
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
/* internal: extlibs/xmqtt/src/internal/xmqtt_session_client.h */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_SESSION)
#ifndef XMQTT_SESSION_CLIENT_INTERNAL_H
#define XMQTT_SESSION_CLIENT_INTERNAL_H
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
/* Library-only exclusive driver. No callback, runtime or public operation-code
 * interface. All mutable entry points share the Session gate; public mutations
 * refuse while claimed. Immutable getters/ref operations remain available. */
XRT_EXTERN_C_BEGIN
XMQTT_INTERNAL bool xmqttInternalSessionClaim(xmqttsession* S,const void* Owner,xmqttversion Version,
    const xmqttpacket* Connect,const xmqttcodecconfig* Codec,double Now,size_t* Count,size_t* Bytes);
XMQTT_INTERNAL void xmqttInternalSessionUnclaim(xmqttsession* S,const void* Owner);
XMQTT_INTERNAL uint16 xmqttInternalSessionIdentifier(xmqttsession* S,const void* Owner,
    const unsigned char* Used,uint16 Requested,uint16 Previous);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionVisit(xmqttsession* S,const void* Owner,
    const xmqttsessionpacket* Previous,const xmqttsessionpacket** Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionOpen(xmqttsession* S,const void* Owner,
    const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Negotiated);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionEnd(xmqttsession* S,const void* Owner,const xmqttpacket* Disconnect,double Now);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionPublish(xmqttsession* S,const void* Owner,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
/* One immutable copy, physically reserved but not linked to protocol state.
 * The exclusive driver activates it only when selecting its first output. */
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSnapshot(xmqttsession* S,const void* Owner,
    const xmqttpacket* Publish,const xmqttsessionpacket** Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSelect(xmqttsession* S,const void* Owner,
    const xmqttsessionpacket* Packet,xmqttsessionframe* Frame);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSent(xmqttsession* S,const void* Owner,const xmqttsessionframe* Frame);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionAcknowledge(xmqttsession* S,const void* Owner,const xmqttpacket* Ack);
/* Withdraw only a driver-owned, new-on-this-connection PUBLISH for which the
 * Client has accepted zero native bytes. Imported obligations cannot use this. */
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionWithdraw(xmqttsession* S,const void* Owner,const xmqttsessionpacket* Packet);
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionReceive(xmqttsession* S,const void* Owner,uint16 Id,unsigned Operation,uint8* Reason);
XMQTT_INTERNAL unsigned xmqttInternalSessionReceiveState(xmqttsession* S,const void* Owner,uint16 Id);
XRT_EXTERN_C_END
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

XMQTT_INTERNAL xmqttpropertystatus xmqttInternalPropertiesRead(xbytesview Input, xmqttpropertyscope Scope,
	xmqttdirection Direction, const xmqttpropertyconfig* Config, xmqttproperty* Properties, size_t Capacity, xmqttpropertydocument* Document,uint8* AliasReason)
{
	if(AliasReason)*AliasReason=0;
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
		if (Status != XMQTT_PROPERTIES_DONE) {if(AliasReason && Property.Id==XMQTT_PROPERTY_TOPIC_ALIAS && Scope==XMQTT_PROPERTIES_PUBLISH)*AliasReason=0x82;return Status;}
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
		Status = xmqttPropertiesValue(&Property); if (Status != XMQTT_PROPERTIES_DONE) {if(AliasReason && Property.Id==XMQTT_PROPERTY_TOPIC_ALIAS)*AliasReason=0x94;return Status;}
		Properties[Count++] = Property;
	}
	xmqttpropertystatus Status = xmqttPropertiesFinal(Scope,Seen); if (Status != XMQTT_PROPERTIES_DONE) return Status;
	*Document = (xmqttpropertydocument){Count,Total,{Input.Data,Total}}; return XMQTT_PROPERTIES_DONE;
}

XMQTT_API xmqttpropertystatus xmqttPropertiesRead(xbytesview Input,xmqttpropertyscope Scope,
    xmqttdirection Direction,const xmqttpropertyconfig* Config,xmqttproperty* Properties,size_t Capacity,xmqttpropertydocument* Document)
{return xmqttInternalPropertiesRead(Input,Scope,Direction,Config,Properties,Capacity,Document,NULL);}

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
	xmqttpropertydocument Doc;uint8 AliasReason=0;
	xmqttpropertystatus Status=xmqttInternalPropertiesRead((xbytesview){Reader->Input.Data+Reader->Position,Reader->End-Reader->Position},Scope,Reader->Direction,
		&Config,Output,Reader->Buffers.PropertyCapacity-Reader->PropertiesUsed,&Doc,&AliasReason);
	if (Status!=XMQTT_PROPERTIES_DONE) {
		if (Status==XMQTT_PROPERTIES_MORE) return xmqtt5ReadFail(Reader,XMQTT_CODEC_MALFORMED,XMQTT_CODEC_FORMAT,Reader->Position,"MQTT property section crosses packet boundary");
		return xmqtt5ReadMark(Reader,Status==XMQTT_PROPERTIES_LOCAL_ERROR ? XMQTT_CODEC_LOCAL_ERROR :
			Status==XMQTT_PROPERTIES_PROTOCOL_ERROR ? XMQTT_CODEC_PROTOCOL_ERROR : XMQTT_CODEC_MALFORMED,
			AliasReason==0x94 ? XMQTT_CODEC_TOPIC_ALIAS : AliasReason==0x82 ? XMQTT_CODEC_TOPIC_ALIAS_REPEAT :
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
#include <stdlib.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_CLIENT)

/* The host worker is the sole protocol owner. Gate protects admission, public
 * snapshots and the receive queue; no user callback runs while it is held. */
typedef struct xmqttquota {xatomic32 Refs;xmutex Gate;size_t Count,Bytes,MaxCount,MaxBytes;} xmqttquota;
typedef struct xmqttbox {xmqttquota* Quota;size_t Bytes;} xmqttbox;
typedef struct xmqttreadybox {xmqttreadyresult Value;xmqttbox Box;char Id[];} xmqttreadybox;
typedef struct xmqttwritebox {xmqttwriteresult Value;xmqttbox Box;} xmqttwritebox;
typedef struct xmqttsubscriptionbox {xmqttsubscriptionresult Value;xmqttbox Box;} xmqttsubscriptionbox;
typedef struct xmqttrequest xmqttrequest;
typedef struct xmqttsendflow {
    size_t Capacity,Reserved;uint16 Next;
    unsigned char Identifiers[8192];
    xmqttrequest* Buckets[];
} xmqttsendflow;
typedef struct xmqttreceiveflow {
    size_t Capacity,Head,Count;unsigned Offset;
    uint16 Identifiers[];
} xmqttreceiveflow;
enum {XMQTT_RX_ACTIVE=1,XMQTT_RX_REC_QUEUED=4,
    XMQTT_RX_COMP_QUEUED=8,XMQTT_RX_RELEASED=16,XMQTT_RX_UNKNOWN_QUEUED=32,XMQTT_RX_OLD=64};
typedef struct xmqttreceiveack {uint16 Identifier;uint8 Type;bool Unknown;} xmqttreceiveack;
/* Constant-time incoming identifier lookup, independent of outgoing ids. Only
 * the first QoS2/unknown PUBREL allocates it; duplicates allocate no objects. */
typedef struct xmqttreceivetwo {
    size_t Capacity,Head,Count,Inflight;unsigned Offset;bool TwoNext;
    unsigned char State[65536];xmqttreceiveack Acks[];
} xmqttreceivetwo;
typedef struct xmqttstartup {xatomic32 Refs;xmqttclient* Client;size_t Bytes;unsigned char Data[];} xmqttstartup;
typedef struct xmqttalias {size_t Size,Capacity;char Topic[];} xmqttalias;
enum {XMQTT_TX_ALIAS=8,XMQTT_TX_TLS=16,XMQTT_TX_SESSION=32,XMQTT_TX_BORROW=64,XMQTT_TX_SHARED=128};
typedef struct xmqttsendtopic {uint32 Refs;size_t Size;char Data[];} xmqttsendtopic;
typedef struct xmqttsendaliasslot {xmqttsendtopic *Planned,*Actual;} xmqttsendaliasslot;
typedef struct xmqttsendaliases {xmqttsendaliasslot* Pages[256];} xmqttsendaliases;
/* Optional aligned request tail: no larger allocation for ordinary publishes. */
typedef struct xmqttsendaliasrequest {
    xmqttsendtopic* Topic;xmqttsendaliasslot* Slot;size_t FrameBytes,TailOffset;
    uint16 Alias;uint8 PrefixBytes,Mode,RawHeader;unsigned char Prefix[7];
} xmqttsendaliasrequest;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
typedef struct xmqtttlsfence {xmqttrequest *Previous,*Next;uint64 Cut;} xmqtttlsfence;
typedef struct xmqtttlsplan {xtlssession* Session;xtlsstreamconfig Config;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool Capture;
#endif
} xmqtttlsplan;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
typedef struct xmqtttlsresumestate {xmqttclienttlsresumeinfo Info;xtlsresume* Value;bool Notify;} xmqtttlsresumestate;
#endif
typedef struct xmqtttlsstate {
    xmqtttlsplan Plan;xtlsstream* Stream;xmqttrequest *Head,*Tail;size_t Bytes,Calls;bool Open;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    xmqtttlsresumestate Resume;
#endif
} xmqtttlsstate;
#endif
struct xmqttmessage {xatomic32 Refs;xmqttbox Box;xmqttpacket Packet;xmqttmessage* Next;xmqttproperty Props[];};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
struct xmqttclientinbox {
    xmutex Gate;xmqttclientinboxconfig Config;xmqttquota* Quota;
    xmqttclient* Client;xmqttmessage *Head,*Tail;size_t Count,Bytes;
};
#endif
struct xmqttrequest {
    xatomic32 Refs,NativeRefs;
    xmqttclient* Client;xmqttrequest *Next,*Previous,*HashNext;
    xpromise* Promise;xcancel* Cancel;union {xmqttwritebox* Write;xmqttsubscriptionbox* Subscription;} Result;
    size_t Bytes,Offset;double Deadline;bool Retain,Credit,Acknowledged;
    uint8 Qos;uint16 Identifier;uint8 Type,Capabilities;
    union {struct {uint8 Offset;bool Pending,Sent;unsigned char Wire[4];} Release;size_t TopicCount;} State;
    unsigned char Wire[];
};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
typedef struct xmqttsessionrequest {const xmqttsessionpacket* Packet;} xmqttsessionrequest;
typedef struct xmqttsharedalias {size_t PropertiesAt;uint8 PrefixBytes;unsigned char Prefix[16];} xmqttsharedalias;
typedef struct xmqttsessiondriver {
    xmqttsession* State;xmqttrequest* Selected;xmqttsessionframe Frame;xmutex AliasGate;
    size_t ReservedCount,ReservedBytes;
    bool Opened,EndExpiry;uint32 Expiry;double EndAt;
} xmqttsessiondriver;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
typedef struct xmqttauthbox {xmqttauthresult Value;xmqttbox Box;} xmqttauthbox;
typedef struct xmqttauthwire {xatomic32 Refs;xmqttclient* Client;size_t Bytes,Offset;unsigned char Wire[];} xmqttauthwire;
struct xmqttauthticket {xatomic32 Refs;xmqttbox Box;xmqttclient* Client;xmqttmessage* Message;xmqttauthphase Phase;bool Taken,Answered;};
typedef struct xmqttauthstate {
    xmqttauthconfig Config;xstrview Method;size_t Bytes,Rounds,Sent;
    bool ProtocolReady,Verified,Active,Reauth,AwaitServer;int Decision;
    xmqttauthticket* Ticket;xmqttauthwire* Out;xpromise* Promise;xcancel* Cancel;xmqttauthbox* Result;
    double Deadline,StepDeadline;char MethodBytes[];
} xmqttauthstate;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
typedef struct xmqttdialplan {
    xnetresolver* Resolver;xnetaddrlist* Addresses;xstrview Host;uint16 Port;
    xnetdialconfig Config;
} xmqttdialplan;
#endif
struct xmqttclient {
    xatomic32 Refs;xmutex Gate;xnetengine* Engine;xnetworker* Worker;xnetstream* Stream;
    xmqttclientconfig Config;xmqttclientinfo Info;xmqttquota* Quota;
    xnetpost Post;bool Posted,Started,Stopped,ReadEnded,NativeClosed,ProblemInfo,ReceivePaused,AbortRequested,Draining,DrainClosing;
    xpromise* ReadyPromise;xfuture* ReadyFuture;xcancel* ReadyCancel;xmqttreadybox* ReadyBox;
    xmqttstartup* Startup;size_t SetupOffset;xmqttpacket SetupPacket;
    xmqttproperty *SetupProps,*Props;
    unsigned char* Input;size_t InputBytes,InputCapacity;
    xmqttrequest *SubmitHead,*SubmitTail,*Requests,*RequestTail,*Partial;
    xatomicptr SendFlow;uint32 SendInflight;
    xmqttreceiveflow* ReceiveFlow;xmqttreceivetwo* ReceiveTwo;xmqttalias** ReceiveAliases;xmqttsendaliases* SendAliases;
    xmqttmessage *MessageHead,*MessageTail;
    xmqttnegotiated Peer;xerror* Cause;
    /* Before CONNACK PingDeadline records last accepted control output time;
     * afterwards it is the outstanding PING response deadline. */
    double SetupDeadline,NextPing,PingDeadline,TimerAt,DrainDeadline;
    uint64 Timer;unsigned PingOffset;bool PingOutstanding;
    xmqttrequest* Disconnect;bool DisconnectReserved,ApplicationClose,ApplicationFinal,ApplicationNativeClosing;
    xmutex RetirementLock;xcond RetirementReady;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    xmqttclientinbox* Inbox;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    xmqttsessiondriver* Session;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttauthstate* Auth;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xnetdial* Dial;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqtttlsstate* TLS;
#endif
    xmqttproperty Scratch[];
};
static void xmqttClientWork(xnetworker* Worker,ptr Data);
static bool xmqttClientWakeLocked(xmqttclient* C);
static bool xmqttClientProtocolReady(const xmqttclient* C);
static void xmqttClientWake(xmqttclient* C);
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static bool xmqttClientSessionRestore(xmqttclient* C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static bool xmqttClientSessionAuthenticate(xmqttclient* C);
#endif
static void xmqttClientSessionFinish(xmqttclient* C,xmqttrequest* R);
static bool xmqttClientSessionSent(xmqttclient* C,xmqttrequest* R);
static void xmqttClientSessionEnd(xmqttclient* C);
static void xmqttClientSessionUnreserve(xmqttclient* C);
static xmqttclientstatus xmqttClientSessionSelect(xmqttclient* C,xmqttrequest* R,bool* Empty);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static void xmqttTlsFail(xmqttclient* C,xtlsresult Result)
{
    const xerror* E=C->TLS->Stream?xrtTlsStreamError(C->TLS->Stream):NULL;
    if(!E && Result==XTLS_ERROR)E=xrtGetError();
    xerrkind K=E?xrtErrorKind(E):XERR_NONE;
    xmqttClientFail(C,K==XERR_MEMORY?XMQTT_STATUS_MEMORY:K==XERR_RANGE || K==XERR_AGAIN?XMQTT_STATUS_LIMIT:K==XERR_TIMEOUT?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_TRANSPORT,E);
}
#endif
static bool xmqttTransportOpen(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return C->TLS->Stream && C->TLS->Open && xrtTlsStreamState(C->TLS->Stream)==XTLS_STREAM_OPEN;
#endif
    return C->Stream && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN;
}
static size_t xmqttTransportAvailable(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return C->TLS->Stream?xrtTlsStreamAvailable(C->TLS->Stream):0;
#endif
    return xrtNetStreamAvailable(C->Stream);
}
static size_t xmqttTransportRead(xmqttclient* C,void* Data,size_t Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){size_t N=0;xtlsresult S=xrtTlsStreamRead(C->TLS->Stream,Data,Size,&N);
        if(S!=XTLS_OK && S!=XTLS_AGAIN && S!=XTLS_CLOSED)xmqttTlsFail(C,S);
        return N;}
#endif
    return xrtNetStreamRead(C->Stream,Data,Size);
}
static size_t xmqttTransportWritable(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)return xmqttTransportOpen(C)?SIZE_MAX:0;
#endif
    return xrtNetStreamWritable(C->Stream);
}
static bool xmqttTransportClose(xmqttclient* C,bool Abort)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS && C->TLS->Stream)return Abort?xrtTlsStreamAbort(C->TLS->Stream):xrtTlsStreamClose(C->TLS->Stream);
#endif
    return !C->Stream || (Abort?xrtNetStreamAbort(C->Stream):xrtNetStreamClose(C->Stream));
}
static xnetresult xmqttTransportSend(xmqttclient* C,cbytes Data,size_t* Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(!C->TLS->Bytes || !C->TLS->Calls){*Size=0;xmqttClientWake(C);return XNET_RESULT_AGAIN;}
        size_t N=0,Capacity=*Size;if(Capacity>C->TLS->Bytes)Capacity=C->TLS->Bytes;--C->TLS->Calls;
        xtlsresult S=xrtTlsStreamSend(C->TLS->Stream,Data,Capacity,&N);*Size=N;C->TLS->Bytes-=N;
        if(S!=XTLS_OK && S!=XTLS_AGAIN){xmqttTlsFail(C,S);return N?XNET_RESULT_OK:XNET_RESULT_ERROR;}
        return N?XNET_RESULT_OK:XNET_RESULT_AGAIN;}
#endif
    return xrtNetStreamSend(C->Stream,Data,*Size);
}
static xnetresult xmqttTransportSendRef(xmqttclient* C,cbytes Data,size_t* Size,xnetreleaseproc Release,ptr User)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){xnetresult S=xmqttTransportSend(C,Data,Size);if(S==XNET_RESULT_OK)Release(User,Data,*Size);return S;}
#endif
    return xrtNetStreamSendRef(C->Stream,Data,*Size,Release,User);
}
static bool xmqttClientError(xerrkind Kind,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.mqtt.client",1,Message);return false;}
static void xmqttClientRef(xmqttclient* C)
{(void)xrtAtomic32FetchAdd(&C->Refs,1,XMEMORY_RELAXED);}
static void xmqttClientDrop(const void* Data)
{
    xmqttclient* C=(xmqttclient*)Data;uint32 Count=xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE);
    /* Above two, a successful CAS cannot publish the caller-only state. There
     * is no Client access after it. Terminal publication shares the wait lock;
     * Gate may already be held by a failed post, so it cannot be the wait lock. */
    while(Count>2)if(xrtAtomic32CompareExchange(&C->Refs,&Count,Count-1,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return;
    if(!xrtMutexLock(&C->RetirementLock))abort();
    uint32 Before=xrtAtomic32FetchSub(&C->Refs,1,XMEMORY_ACQ_REL);
    if(Before<2 || (Before==2 && !xrtCondBroadcast(&C->RetirementReady)))abort();
    if(!xrtMutexUnlock(&C->RetirementLock))abort();
}
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
{xrtMutexLock(&C->Gate);bool B=C->Info.State>=XMQTT_CLIENT_CLOSING && !(C->ApplicationClose && !C->ApplicationFinal && !C->AbortRequested && C->Info.CloseStatus==XMQTT_STATUS_OK);xrtMutexUnlock(&C->Gate);return B;}
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
static xmqttquota* xmqttQuotaCreate(size_t Count,size_t Bytes)
{
    xmqttquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q)return NULL;
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);return NULL;}
    xrtAtomic32Init(&Q->Refs,1);Q->MaxCount=Count;Q->MaxBytes=Bytes;return Q;
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
static xmqttmessage* xmqttMessageCopy(xmqttclient* C,const xmqttpacket* P,xmqttclientstatus* Failure,const xstrview* Resolved)
{
    size_t Count=P->Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttmessage))/sizeof(xmqttproperty) ||
       P->Wire.Size>SIZE_MAX-sizeof(xmqttmessage)-Count*sizeof(xmqttproperty)){*Failure=XMQTT_STATUS_LIMIT;return NULL;}
    size_t Bytes=sizeof(xmqttmessage)+Count*sizeof(xmqttproperty)+P->Wire.Size;
    if(Resolved){if(Resolved->Size>SIZE_MAX-Bytes){*Failure=XMQTT_STATUS_LIMIT;return NULL;}Bytes+=Resolved->Size;}
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
    if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK)M->Packet.Body.Reasons=xmqttRebaseBytes(P->Body.Reasons,P->Wire.Data,Wire);
    if(P->Type==XMQTT_PACKET_PUBLISH){
        if(Resolved){char* Topic=(char*)(Wire+P->Wire.Size);memcpy(Topic,Resolved->Data,Resolved->Size);M->Packet.Body.Publish.Topic=(xstrview){Topic,Resolved->Size};}
        else M->Packet.Body.Publish.Topic=xmqttRebaseText(P->Body.Publish.Topic,P->Wire.Data,Wire);
        M->Packet.Body.Publish.Payload=xmqttRebaseBytes(P->Body.Publish.Payload,P->Wire.Data,Wire);
    }
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
{xmqttmessage* M=(xmqttmessage*)Message;if(M && xrtAtomic32FetchSub(&M->Refs,1,XMEMORY_ACQ_REL)==1){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(M->Packet.Type==XMQTT_PACKET_AUTH || M->Packet.Type==XMQTT_PACKET_CONNACK)xrtSecureZero((ptr)M->Packet.Wire.Data,M->Packet.Wire.Size);
#endif
    xmqttBoxFree(M,M->Box);}}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
XMQTT_API bool xmqttClientInboxConfigInit(xmqttclientinboxconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox config output");
    *K=(xmqttclientinboxconfig){sizeof(*K),XMQTT_VERSION_5,512,16u*1024u*1024u,64,8u*1024u*1024u};return true;
}
XMQTT_API bool xmqttClientInboxCreate(const xmqttclientinboxconfig* Config,xmqttclientinbox** Out)
{
    xmqttclientinboxconfig K;
    if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox config");K=*Config;}else xmqttClientInboxConfigInit(&K);
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || K.Size!=sizeof(K) ||
       (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u ||
       !K.MaxObjectBytes || !K.MaxMessages || !K.MaxMessageBytes)return xmqttClientError(XERR_ARGUMENT,"invalid Inbox limits");
    xmqttclientinbox* I=xrtCalloc(1,sizeof(*I));if(!I)return false;
    if(!xrtMutexInit(&I->Gate)){xrtFree(I);return false;}
    I->Config=K;I->Quota=xmqttQuotaCreate(K.MaxObjects,K.MaxObjectBytes);
    if(!I->Quota){xrtMutexUnit(&I->Gate);xrtFree(I);return false;}*Out=I;return true;
}
static bool xmqttInboxPut(xmqttclient* C,xmqttclientinbox* I,xmqttmessage* M)
{
    xrtMutexLock(&I->Gate);bool Full=I->Count==I->Config.MaxMessages || M->Box.Bytes>I->Config.MaxMessageBytes-I->Bytes;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(!Full && C->Session && ((M->Packet.Flags>>1)&3u)==2 && xmqttInternalSessionReceive(C->Session->State,C,M->Packet.Identifier,1,NULL)!=XMQTT_SESSION_DONE)abort();
#else
    (void)C;
#endif
    if(!Full){if(I->Tail)I->Tail->Next=M;else I->Head=M;I->Tail=M;++I->Count;I->Bytes+=M->Box.Bytes;}
    xrtMutexUnlock(&I->Gate);return !Full;
}
XMQTT_API bool xmqttClientInboxTake(xmqttclientinbox* I,xmqttmessage** Out)
{
    if(!I || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox message output");
    xrtMutexLock(&I->Gate);xmqttmessage* M=I->Head;
    if(M){I->Head=M->Next;if(!I->Head)I->Tail=NULL;--I->Count;I->Bytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&I->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInboxInfo(xmqttclientinbox* I,xmqttclientinboxinfo* Out)
{
    if(!I || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid Inbox info output");
    xrtMutexLock(&I->Gate);xmqttclientinboxinfo V={sizeof(V),I->Client!=NULL,I->Count,I->Bytes,0,0};xrtMutexUnlock(&I->Gate);
    xrtMutexLock(&I->Quota->Gate);V.Objects=I->Quota->Count;V.ObjectBytes=I->Quota->Bytes;xrtMutexUnlock(&I->Quota->Gate);*Out=V;return true;
}
XMQTT_API bool xmqttClientInboxDestroy(xmqttclientinbox* I)
{
    if(!I)return true;
    xrtMutexLock(&I->Gate);
    if(I->Client){xrtMutexUnlock(&I->Gate);return xmqttClientError(XERR_STATE,"Inbox still attached to Client");}
    xmqttmessage* M=I->Head;xrtMutexUnlock(&I->Gate);
    while(M){xmqttmessage* Next=M->Next;xmqttMessageRelease(M);M=Next;}
    xmqttQuotaDrop(I->Quota);xrtMutexUnit(&I->Gate);xrtFree(I);return true;
}
#endif
static void xmqttReadyFree(ptr Value,ptr Context)
{(void)Context;xmqttreadybox* B=Value;xmqttMessageRelease(B->Value.Connack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttWriteFree(ptr Value,ptr Context)
{(void)Context;xmqttwritebox* B=Value;xmqttMessageRelease(B->Value.Puback);xmqttMessageRelease(B->Value.Pubrec);xmqttMessageRelease(B->Value.Pubcomp);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static void xmqttSubscriptionFree(ptr Value,ptr Context)
{(void)Context;xmqttsubscriptionbox* B=Value;xmqttMessageRelease(B->Value.Ack);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);}
static bool xmqttRequestSubscription(const xmqttrequest* R)
{return R->Type==XMQTT_PACKET_SUBSCRIBE || R->Type==XMQTT_PACKET_UNSUBSCRIBE;}
static void xmqttRequestResultFree(xmqttrequest* R)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(R->Capabilities&XMQTT_TX_BORROW)return;
#endif
    if(xmqttRequestSubscription(R)){if(R->Result.Subscription)xmqttSubscriptionFree(R->Result.Subscription,NULL);}
    else if(R->Result.Write)xmqttWriteFree(R->Result.Write,NULL);
}
/* Gate serializes cold allocation and all identifier reservations. Only the
 * worker mutates hash buckets; the immutable table address publishes atomically. */
static xmqttclientadmission xmqttIdentifierReserveImpl(xmqttclient* C,uint16 Requested,uint16* Identifier,bool Restore)
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
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session && !Restore){Id=xmqttInternalSessionIdentifier(C->Session->State,C,F->Identifiers,Requested,F->Next);
        if(!Id)return XMQTT_ADMISSION_OVERLOADED;
        if(!Requested)F->Next=Id;
    }else
#else
    (void)Restore;
#endif
    {
    if(Id){if(F->Identifiers[Id>>3]&(1u<<(Id&7u)))return XMQTT_ADMISSION_OVERLOADED;}
    else {do{Id=++F->Next;if(!Id)Id=++F->Next;}while(F->Identifiers[Id>>3]&(1u<<(Id&7u)));}
    }
    F->Identifiers[Id>>3]|=(unsigned char)(1u<<(Id&7u));++F->Reserved;*Identifier=Id;return XMQTT_ADMISSION_ACCEPTED;
}
static xmqttclientadmission xmqttIdentifierReserveLocked(xmqttclient* C,uint16 Requested,uint16* Identifier)
{return xmqttIdentifierReserveImpl(C,Requested,Identifier,false);}
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
static size_t xmqttSendAliasPadding(size_t Bytes)
{return (0u-Bytes)&(_Alignof(xmqttsendaliasrequest)-1u);}
static size_t xmqttRequestStorageBytes(const xmqttrequest* R)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(R->Capabilities&(XMQTT_TX_BORROW|XMQTT_TX_SHARED))return 0;
#endif
    return R->Bytes;
}
static xmqttsendaliasrequest* xmqttSendAliasRequest(const xmqttrequest* R)
{size_t Bytes=xmqttRequestStorageBytes(R);return (xmqttsendaliasrequest*)(R->Wire+Bytes+xmqttSendAliasPadding(Bytes));}
static size_t xmqttRequestExtra(size_t Bytes,bool Alias,bool TLS)
{
    size_t N=Alias?xmqttSendAliasPadding(Bytes)+sizeof(xmqttsendaliasrequest):0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(TLS)N+=((0u-Bytes-N)&(_Alignof(xmqtttlsfence)-1u))+sizeof(xmqtttlsfence);
#else
    (void)TLS;
#endif
    return N;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static size_t xmqttSessionRequestExtra(size_t Bytes,bool Alias,bool TLS,bool Borrow)
{
    size_t Before=(Borrow?0:Bytes)+xmqttRequestExtra(Borrow?0:Bytes,Alias,TLS);
    return ((0u-Before)&(_Alignof(xmqttsessionrequest)-1u))+sizeof(xmqttsessionrequest)+(Borrow?sizeof(xmqttwritebox):Alias?sizeof(xmqttsharedalias):0);
}
static xmqttsessionrequest* xmqttSessionRequest(const xmqttrequest* R)
{
    size_t Bytes=xmqttRequestStorageBytes(R);
    size_t Before=Bytes+xmqttRequestExtra(Bytes,(R->Capabilities&XMQTT_TX_ALIAS)!=0,(R->Capabilities&XMQTT_TX_TLS)!=0);
    return (xmqttsessionrequest*)(R->Wire+Before+((0u-Before)&(_Alignof(xmqttsessionrequest)-1u)));
}
static xmqttsharedalias* xmqttSharedAlias(const xmqttrequest* R)
{return (xmqttsharedalias*)(xmqttSessionRequest(R)+1);}
#endif
static bool xmqttClientUsesTls(const xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    return C->TLS!=NULL;
#else
    (void)C;return false;
#endif
}
static size_t xmqttRequestWireBytes(const xmqttrequest* R)
{if(R->Capabilities&XMQTT_TX_ALIAS){size_t Bytes=xmqttSendAliasRequest(R)->FrameBytes;if(Bytes)return Bytes;}return R->Bytes;}
static void xmqttSendTopicDropLocked(xmqttclient* C,xmqttsendtopic* T)
{if(T && !--T->Refs){C->Info.PrivateBytes-=sizeof(*T)+T->Size;xrtFree(T);}}
static void* xmqttSendAliasAllocateLocked(xmqttclient* C,size_t Bytes,xmqttclientadmission* Admission)
{
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){*Admission=XMQTT_ADMISSION_OVERLOADED;return NULL;}
    void* P=xrtCalloc(1,Bytes);if(!P){*Admission=XMQTT_ADMISSION_ERROR;return NULL;}C->Info.PrivateBytes+=Bytes;return P;
}
/* Planned state linearizes only with successful public admission. Actual state
 * is worker-owned and describes accepted TCP packet order, not broker ACKs. */
static xmqttclientadmission xmqttSendAliasSnapshotLocked(xmqttclient* C,xmqttrequest* R,const xstrview* Logical)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttclientadmission Admission=XMQTT_ADMISSION_ACCEPTED;
    size_t Topic;const char* Data;
    if(Logical){Topic=Logical->Size;Data=Logical->Data;}
    else {size_t H=1;while(R->Wire[H++]&128u){}A->RawHeader=(uint8)H;
        Topic=((size_t)R->Wire[H]<<8)|R->Wire[H+1];A->TailOffset=H+2+Topic;Data=(const char*)R->Wire+H+2;}
    size_t Page=A->Alias>>8,Index=A->Alias&255u;
    if(!Topic && (!C->SendAliases || !C->SendAliases->Pages[Page] || !C->SendAliases->Pages[Page][Index].Planned)){
        xmqttClientError(XERR_STATE,"empty outgoing alias has no admitted topic");return XMQTT_ADMISSION_ERROR;}
    if(!C->SendAliases){C->SendAliases=xmqttSendAliasAllocateLocked(C,sizeof(*C->SendAliases),&Admission);if(!C->SendAliases)return Admission;}
    if(!C->SendAliases->Pages[Page]){C->SendAliases->Pages[Page]=xmqttSendAliasAllocateLocked(C,256*sizeof(xmqttsendaliasslot),&Admission);if(!C->SendAliases->Pages[Page])return Admission;}
    A->Slot=C->SendAliases->Pages[Page]+Index;xmqttsendtopic* T=A->Slot->Planned;
    if(Topic && (!T || Topic!=T->Size || memcmp(T->Data,Data,Topic))){
        T=xmqttSendAliasAllocateLocked(C,sizeof(*T)+Topic,&Admission);if(!T)return Admission;T->Refs=1;T->Size=Topic;memcpy(T->Data,Data,Topic);
    }else ++T->Refs;
    A->Topic=T;return XMQTT_ADMISSION_ACCEPTED;
}
static void xmqttSendAliasCommitLocked(xmqttclient* C,xmqttrequest* R)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsendtopic* Old=A->Slot->Planned;
    ++A->Topic->Refs;A->Slot->Planned=A->Topic;xmqttSendTopicDropLocked(C,Old);
}
static void xmqttRequestRef(xmqttrequest* R)
{(void)xrtAtomic32FetchAdd(&R->Refs,1,XMEMORY_RELAXED);}
static void xmqttRequestDrop(const void* Data)
{
    xmqttrequest* R=(xmqttrequest*)Data;if(xrtAtomic32FetchSub(&R->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=R->Client;size_t Bytes=R->Bytes;uint16 Id=R->Identifier;uint8 Capabilities=R->Capabilities;
    bool Alias=(Capabilities&XMQTT_TX_ALIAS)!=0,TLS=(Capabilities&XMQTT_TX_TLS)!=0;
    xmqttsendtopic* Topic=Alias?xmqttSendAliasRequest(R)->Topic:NULL;size_t Storage=xmqttRequestStorageBytes(R),Extra=xmqttRequestExtra(Storage,Alias,TLS);xrtCancelDestroy(R->Cancel);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    const xmqttsessionpacket* Packet=(Capabilities&XMQTT_TX_SESSION)?xmqttSessionRequest(R)->Packet:NULL;
    if(Capabilities&XMQTT_TX_SESSION)Extra+=xmqttSessionRequestExtra(Storage,Alias,TLS,(Capabilities&XMQTT_TX_BORROW)!=0);
#endif
    /* The final reference is gone; capture immutable bookkeeping first, free
     * real storage, then publish capacity. C's request reference remains held
     * on this stack until all accounting and packet retirement have finished. */
    xrtFree(R);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Packet)xmqttSessionPacketRelease(Packet);
#endif
    xrtMutexLock(&C->Gate);xmqttSendTopicDropLocked(C,Topic);C->Info.PrivateBytes-=Extra;
    xmqttIdentifierReleaseLocked(C,Id);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;xrtMutexUnlock(&C->Gate);
    xmqttClientDrop(C);
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
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static xmqtttlsfence* xmqttTlsFence(const xmqttrequest* R)
{
    size_t Bytes=xmqttRequestStorageBytes(R),At=Bytes+xmqttRequestExtra(Bytes,(R->Capabilities&XMQTT_TX_ALIAS)!=0,true)-sizeof(xmqtttlsfence);
    return (xmqtttlsfence*)(R->Wire+At);
}
static void xmqttTlsUnlink(xmqttclient* C,xmqttrequest* R)
{
    xmqtttlsfence* F=xmqttTlsFence(R);xmqtttlsstate* T=C->TLS;
    if(F->Previous)xmqttTlsFence(F->Previous)->Next=F->Next;else T->Head=F->Next;
    if(F->Next)xmqttTlsFence(F->Next)->Previous=F->Previous;else T->Tail=F->Previous;
    F->Previous=F->Next=NULL;
}
static void xmqttTlsAppend(xmqttclient* C,xmqttrequest* R,uint64 Cut)
{
    xmqtttlsstate* T=C->TLS;xmqtttlsfence* F=xmqttTlsFence(R);F->Cut=Cut;F->Previous=T->Tail;F->Next=NULL;
    if(T->Tail)xmqttTlsFence(T->Tail)->Next=R;else T->Head=R;T->Tail=R;
}
static void xmqttTlsReleaseFence(xmqttclient* C,xmqttrequest* R)
{
    xmqttTlsUnlink(C,R);(void)xrtAtomic32FetchSub(&R->NativeRefs,1,XMEMORY_RELEASE);xmqttRequestDrop(R);
}
static void xmqttTlsProgress(xmqttclient* C)
{
    xmqtttlsstate* T=C->TLS;
    if(!T || !T->Head || !T->Stream || !T->Open || C->ReadEnded || C->NativeClosed || C->ApplicationNativeClosing || C->DrainClosing || xmqttClientClosing(C))return;
    xtlswriteprogress W={sizeof(W),0,0};
    if(!xrtTlsStreamWriteProgress(T->Stream,&W)){xmqttTlsFail(C,XTLS_ERROR);return;}
    while(T->Head && xmqttTlsFence(T->Head)->Cut<=W.Completed)xmqttTlsReleaseFence(C,T->Head);
}
#endif
/* Raw TCP keeps its existing per-segment leases. TLS copies plaintext into its
 * authenticated record buffers and holds one actual request credit until the
 * saved ciphertext prefix retires, independently of early protocol ACKs. */
static xnetresult xmqttRequestSend(xmqttclient* C,xmqttrequest* R,const xnetref* Refs,size_t Count,size_t* Size)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(!C->TLS->Bytes || !C->TLS->Calls){*Size=0;xmqttClientWake(C);return XNET_RESULT_AGAIN;}
        bool Added=!xrtAtomic32Load(&R->NativeRefs,XMEMORY_RELAXED);
        if(Added){xmqttRequestRef(R);xrtAtomic32Store(&R->NativeRefs,1,XMEMORY_RELAXED);xmqttTlsAppend(C,R,UINT64_MAX);}
        xnetspan Spans[4];size_t Left=C->TLS->Bytes,SpanCount=0;
        for(size_t I=0;I<Count && Left;++I){size_t N=Refs[I].Size;if(N>Left)N=Left;Spans[SpanCount++]=(xnetspan){Refs[I].Data,N};Left-=N;}
        size_t N=0;--C->TLS->Calls;xtlsresult S=xrtTlsStreamSendVec(C->TLS->Stream,Spans,SpanCount,&N);*Size=N;C->TLS->Bytes-=N;
        if(N && C->TLS->Stream){xtlswriteprogress W={sizeof(W),0,0};
            if(xrtTlsStreamWriteProgress(C->TLS->Stream,&W)){xmqttTlsUnlink(C,R);xmqttTlsAppend(C,R,W.Produced);}
            else xmqttTlsFail(C,XTLS_ERROR);
        }else if(!N && Added && xrtAtomic32Load(&R->NativeRefs,XMEMORY_RELAXED))xmqttTlsReleaseFence(C,R);
        if(S!=XTLS_OK && S!=XTLS_AGAIN){xmqttTlsFail(C,S);return N?XNET_RESULT_OK:XNET_RESULT_ERROR;}
        return N?XNET_RESULT_OK:XNET_RESULT_AGAIN;
    }
#endif
    for(size_t I=0;I<Count;++I)xmqttRequestRef(R);
    (void)xrtAtomic32FetchAdd(&R->NativeRefs,(uint32)Count,XMEMORY_RELAXED);
    xnetresult S=Count==1?xrtNetStreamSendRef(C->Stream,Refs[0].Data,Refs[0].Size,xmqttRequestNativeDrop,R):xrtNetStreamSendRefs(C->Stream,Refs,Count);
    if(S!=XNET_RESULT_OK){(void)xrtAtomic32FetchSub(&R->NativeRefs,(uint32)Count,XMEMORY_RELAXED);for(size_t I=0;I<Count;++I)xmqttRequestDrop(R);}
    (void)Size;return S;
}
static void xmqttRequestFinish(xmqttclient* C,xmqttrequest** Link,xmqttclientstatus Status)
{
    xmqttrequest* R=*Link;*Link=R->Next;xpromise* P=R->Promise;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(R->Capabilities&XMQTT_TX_SESSION)xmqttClientSessionFinish(C,R);
#endif
    if(C->Partial==R)C->Partial=NULL;
    if(R->Next)R->Next->Previous=R->Previous;else C->RequestTail=R->Previous;
    xmqttRequestMapRemove(C,R);if(R->Credit)--C->SendInflight;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(R->Capabilities&XMQTT_TX_BORROW){xmqttRequestDrop(R);if(C->Started)xmqttClientNotify(C);return;}
#endif
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    if(xmqttRequestSubscription(R)){
        xmqttsubscriptionbox* B=R->Result.Subscription;
        B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Identifier=R->Identifier;
        if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
        if(!xrtPromiseResolveOwned(P,B,xmqttSubscriptionFree,NULL))xmqttSubscriptionFree(B,NULL);
    }else {
        xmqttwritebox* B=R->Result.Write;
        B->Value.Status=Status;B->Value.AcceptedBytes=R->Offset;B->Value.Delivery=R->Offset?XMQTT_MAY_HAVE_REACHED_PEER:XMQTT_NOT_SENT;
        B->Value.Identifier=R->Identifier;
        if(R->Acknowledged)B->Value.Delivery=B->Value.Reason<0x80?XMQTT_PEER_ACCEPTED:XMQTT_PEER_REJECTED;
        if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);
        if(!xrtPromiseResolveOwned(P,B,xmqttWriteFree,NULL))xmqttWriteFree(B,NULL);
    }
    xrtPromiseDestroy(P);xmqttRequestDrop(R);xmqttClientNotify(C);
}
static void xmqttClientFailReason(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause,uint8 Reason)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XMQTT_CLIENT_CLOSING || (C->ApplicationClose && C->Info.State==XMQTT_CLIENT_CLOSING && C->Info.CloseStatus==XMQTT_STATUS_OK)){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=Status;
        if(Status==XMQTT_STATUS_PROTOCOL)++C->Info.ProtocolErrors;
        if(Cause)C->Cause=xrtErrorRef(Cause);
        if(!C->ApplicationClose && C->Config.Version==XMQTT_VERSION_5 && C->Info.Ready)C->Info.LocalReason=Reason;
    }
    xrtMutexUnlock(&C->Gate);
}
static void xmqttClientFail(xmqttclient* C,xmqttclientstatus Status,const xerror* Cause)
{xmqttClientFailReason(C,Status,Cause,Status==XMQTT_STATUS_PROTOCOL?0x82:0);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
/* A retained cancellation dispatch may outlive its exchange. Only wake the
 * owner; it samples the CURRENT exchange's token, never a stale generation. */
static void xmqttAuthCancel(const void* Data)
{xmqttClientWake((xmqttclient*)Data);}
static const xfutureproducercancellationv1 xmqttAuthCancellation={sizeof(xfutureproducercancellationv1),xmqttClientRetainProducer,xmqttAuthCancel};
static xmqttclientadmission xmqttAuthDecisionTimeout(xmqttclient* C)
{
    xmqttClientError(XERR_TIMEOUT,"authentication decision deadline expired");
    xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,xrtGetError());xmqttClientWake(C);return XMQTT_ADMISSION_ERROR;
}
static bool xmqttAuthMethod(const xmqttauthstate* A,const xmqttpacket* P)
{
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_METHOD){
        xstrview V=P->Properties.Data[I].Value.Text;
        return V.Size==A->Method.Size && (!V.Size || !memcmp(V.Data,A->Method.Data,V.Size));
    }
    return false;
}
XMQTT_API bool xmqttAuthConfigInit(xmqttauthconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication config output");
    *K=(xmqttauthconfig){sizeof(*K),32,1024u*1024u,5000};return true;
}
static void xmqttAuthResultFree(ptr Value,ptr Context)
{
    (void)Context;xmqttauthbox* B=Value;xmqttMessageRelease(B->Value.Final);xrtErrorFree((xerror*)B->Value.Cause);xmqttBoxFree(B,B->Box);
}
static void xmqttAuthWireDrop(xmqttauthwire* W)
{
    if(!W || xrtAtomic32FetchSub(&W->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttclient* C=W->Client;size_t Bytes=sizeof(*W)+W->Bytes;xrtSecureZero(W->Wire,W->Bytes);xrtFree(W);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);
}
static void xmqttAuthWireRelease(ptr Data,cbytes Bytes,size_t Size)
{(void)Bytes;(void)Size;xmqttauthwire* W=Data;xmqttClientWake(W->Client);xmqttAuthWireDrop(W);}
static xmqttauthwire* xmqttAuthWireAllocate(xmqttclient* C,const xmqttpacket* P,size_t Bytes,xmqttclientadmission* Admission)
{
    xrtMutexLock(&C->Gate);
    if(Bytes>SIZE_MAX-sizeof(xmqttauthwire) || sizeof(xmqttauthwire)+Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        xrtMutexUnlock(&C->Gate);*Admission=XMQTT_ADMISSION_OVERLOADED;return NULL;}
    C->Info.PrivateBytes+=sizeof(xmqttauthwire)+Bytes;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttauthwire* W=xrtMalloc(sizeof(*W)+Bytes);
    if(!W){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=sizeof(xmqttauthwire)+Bytes;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return NULL;}
    memset(W,0,sizeof(*W));W->Client=C;W->Bytes=Bytes;xrtAtomic32Init(&W->Refs,1);
    if(!xmqttPacketWrite5(W->Wire,Bytes,P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)){xmqttAuthWireDrop(W);return NULL;}
    return W;
}
XMQTT_API void xmqttAuthRelease(xmqttauthticket* T)
{
    if(T && xrtAtomic32FetchSub(&T->Refs,1,XMEMORY_ACQ_REL)==1){xmqttclient* C=T->Client;xmqttMessageRelease(T->Message);xmqttBoxFree(T,T->Box);xmqttClientDrop(C);}
}
XMQTT_API xmqttauthphase xmqttAuthPhase(const xmqttauthticket* T)
{return T?T->Phase:XMQTT_AUTH_CHALLENGE;}
XMQTT_API const xmqttmessage* xmqttAuthMessage(const xmqttauthticket* T)
{return T?T->Message:NULL;}
XMQTT_API bool xmqttClientTakeAuth(xmqttclient* C,xmqttauthticket** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication ticket output");
    xrtMutexLock(&C->Gate);xmqttauthticket* T=C->Auth?C->Auth->Ticket:NULL;
    if(T && !T->Taken && C->Info.State<XMQTT_CLIENT_CLOSING){T->Taken=true;(void)xrtAtomic32FetchAdd(&T->Refs,1,XMEMORY_RELAXED);}else T=NULL;
    xrtMutexUnlock(&C->Gate);if(!T)return false;*Out=T;return true;
}
/* Worker-owned receipt; the mailbox and decision publication share Gate. */
static bool xmqttAuthOffer(xmqttclient* C,xmqttmessage* M,xmqttauthphase Phase)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttauthticket* T=xmqttBoxAllocate(C->Quota,sizeof(*T),&Failure);
    if(!T){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    T->Box=(xmqttbox){C->Quota,sizeof(*T)};T->Client=C;T->Message=M;T->Phase=Phase;xrtAtomic32Init(&T->Refs,1);
    xmqttClientRef(C);(void)xmqttMessageRetain(M);
    xrtMutexLock(&C->Gate);C->Auth->Ticket=T;C->Auth->AwaitServer=false;C->Auth->Decision=0;
    C->Auth->StepDeadline=xmqttClientDeadline(xrtTimer(),C->Auth->Config.ResponseTimeout);xrtMutexUnlock(&C->Gate);
    xmqttClientNotify(C);return true;
}
static bool xmqttAuthCharge(xmqttclient* C,size_t Bytes,bool Challenge)
{
    xrtMutexLock(&C->Gate);xmqttauthstate* A=C->Auth;
    bool Fits=Bytes<=A->Config.MaxExchangeBytes-A->Bytes && (!Challenge || A->Rounds<A->Config.MaxRounds);
    if(Fits){A->Bytes+=Bytes;if(Challenge)++A->Rounds;}xrtMutexUnlock(&C->Gate);
    if(!Fits)xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);
    return Fits;
}
static bool xmqttAuthFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);bool Expected=A && A->Active && A->AwaitServer && !A->Ticket && (!A->Out || A->Out->Offset==A->Out->Bytes) && (A->ProtocolReady || (C->Startup && C->SetupOffset==C->Startup->Bytes));
    bool Reauth=A && A->Reauth;xrtMutexUnlock(&C->Gate);
    if(!Expected || !xmqttAuthMethod(A,P) || (P->Reason!=0x18 && !(Reauth && P->Reason==0))){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!C->ProblemInfo)for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_REASON_STRING || P->Properties.Data[I].Id==XMQTT_PROPERTY_USER){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!xmqttAuthCharge(C,P->Wire.Size,P->Reason==0x18))return false;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,NULL);
    if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
    if(!P->Reason){A->Result->Value.Final=M;return xmqttAuthOffer(C,M,XMQTT_AUTH_VERIFY_REAUTH);}
    bool OK=xmqttAuthOffer(C,M,XMQTT_AUTH_CHALLENGE);xmqttMessageRelease(M);return OK;
}
static bool xmqttAuthConnack(xmqttclient* C,xmqttmessage* M)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);bool Expected=A->Active && !A->Reauth && A->AwaitServer && !A->Ticket;xrtMutexUnlock(&C->Gate);
    if(!Expected || !xmqttAuthMethod(A,&M->Packet)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    if(!xmqttAuthCharge(C,M->Packet.Wire.Size,false))return false;
    xrtMutexLock(&C->Gate);A->ProtocolReady=true;xrtMutexUnlock(&C->Gate);
    /* Session opening after proof still needs the owned CONNECT snapshot.
     * Its existing setup/private budgets remain charged until Ready settles. */
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(!C->Session){xmqttStartupRetire(C);}
#else
    xmqttStartupRetire(C);
#endif
    return xmqttAuthOffer(C,M,XMQTT_AUTH_VERIFY_CONNECT);
}
static void xmqttAuthFinish(xmqttclient* C,xmqttclientstatus Status)
{
    xmqttauthstate* A=C->Auth;
    xrtMutexLock(&C->Gate);xpromise* P=A->Promise;xmqttauthbox* B=A->Result;xcancel* Cancel=A->Cancel;
    A->Promise=NULL;A->Result=NULL;A->Cancel=NULL;A->Active=false;A->Reauth=false;A->StepDeadline=INFINITY;
    if(B){B->Value.Status=Status;B->Value.AcceptedBytes=A->Sent;if(C->Cause)B->Value.Cause=xrtErrorRef(C->Cause);}
    xrtMutexUnlock(&C->Gate);
    xrtCancelDestroy(Cancel);if(P){if(!xrtPromiseResolveOwned(P,B,xmqttAuthResultFree,NULL))xmqttAuthResultFree(B,NULL);xrtPromiseDestroy(P);xmqttClientNotify(C);}
}
static void xmqttAuthWork(xmqttclient* C)
{
    xmqttauthstate* A=C->Auth;if(!A)return;
    xrtMutexLock(&C->Gate);xmqttauthticket* T=A->Ticket;int Decision=A->Decision;bool Closing=C->Info.State>=XMQTT_CLIENT_CLOSING;
    if(T && T->Answered){A->Ticket=NULL;A->StepDeadline=INFINITY;}else T=NULL;
    bool Cancel=A->Cancel && xrtCancelRequested(A->Cancel);double At=fmin(A->Deadline,A->StepDeadline);bool Active=A->Active;bool Expired=Active && xrtTimer()>=At;
    /* Reauth stays reserved until Finish atomically detaches its producer/result.
     * Otherwise a new admission can overwrite the still-pending old exchange. */
    xrtMutexUnlock(&C->Gate);
    if(!Closing && Cancel)xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    if(!Closing && Expired)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(T){if(Decision<0)xmqttClientFailReason(C,XMQTT_STATUS_AUTHENTICATION,NULL,0x80);
        else if(Decision>0 && !Closing && !Cancel && !Expired){if(T->Phase==XMQTT_AUTH_VERIFY_CONNECT){bool Accepted=true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
                if(C->Session)Accepted=xmqttClientSessionAuthenticate(C);
#endif
                xrtMutexLock(&C->Gate);bool Publish=Accepted && C->Info.State<XMQTT_CLIENT_CLOSING;
                if(Publish){A->Active=false;A->Verified=true;C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;}xrtMutexUnlock(&C->Gate);
                if(Publish)xmqttReadyFinish(C,XMQTT_STATUS_OK);
            }else xmqttAuthFinish(C,XMQTT_STATUS_OK);}
        xmqttAuthRelease(T);}
}
static bool xmqttAuthSend(xmqttclient* C,size_t* Budget)
{
    xmqttauthstate* A=C->Auth;if(!A)return true;
    xrtMutexLock(&C->Gate);xmqttauthwire* W=A->Out;xrtMutexUnlock(&C->Gate);
    while(W && W->Offset<W->Bytes && *Budget){size_t N=W->Bytes-W->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xmqttTransportSendRef(C,W->Wire+W->Offset,&N,xmqttAuthWireRelease,W);
        if(S!=XNET_RESULT_OK){xmqttAuthWireDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        W->Offset+=N;*Budget-=N;xrtMutexLock(&C->Gate);A->Sent+=N;xrtMutexUnlock(&C->Gate);
        double SentAt=xrtTimer();if(!A->ProtocolReady)C->PingDeadline=SentAt;
        if(C->Peer.KeepAlive)C->NextPing=SentAt+C->Peer.KeepAlive;
    }
    if(W && W->Offset<W->Bytes){xmqttClientWake(C);return false;}
    if(W){xrtMutexLock(&C->Gate);A->Out=NULL;A->AwaitServer=true;xrtMutexUnlock(&C->Gate);xmqttAuthWireDrop(W);}return true;
}
/* Stop releases only the client's mailbox/output owners. Caller/native duties
 * still pin Client and remain visible to physical WaitFor. */
static bool xmqttAuthPartial(xmqttclient* C)
{
    if(!C->Auth)return false;
    xrtMutexLock(&C->Gate);xmqttauthwire* W=C->Auth?C->Auth->Out:NULL;bool Partial=W && W->Offset && W->Offset<W->Bytes;xrtMutexUnlock(&C->Gate);return Partial;
}
static double xmqttAuthDeadline(xmqttclient* C)
{
    if(!C->Auth)return INFINITY;
    xrtMutexLock(&C->Gate);xmqttauthstate* A=C->Auth;double At=A && A->Active?fmin(A->Deadline,A->StepDeadline):INFINITY;xrtMutexUnlock(&C->Gate);return At;
}
static bool xmqttAuthStop(xmqttclient* C)
{
    xmqttauthstate* A=C->Auth;if(!A)return false;
    xrtMutexLock(&C->Gate);xmqttauthticket* T=A->Ticket;xmqttauthwire* W=A->Out;
    A->Ticket=NULL;A->Out=NULL;bool Partial=W && W->Offset && W->Offset<W->Bytes;xrtMutexUnlock(&C->Gate);
    xmqttAuthFinish(C,C->Info.CloseStatus);xmqttAuthRelease(T);xmqttAuthWireDrop(W);return Partial;
}
XMQTT_API xmqttclientadmission xmqttAuthComplete(xmqttauthticket* T,bool Accept)
{
    if(!T){xmqttClientError(XERR_ARGUMENT,"authentication ticket is NULL");return XMQTT_ADMISSION_ERROR;}
    xmqttclient* C=T->Client;xmqttauthstate* A=C->Auth;xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(A->Ticket!=T || !T->Taken || T->Answered || (Accept && T->Phase==XMQTT_AUTH_CHALLENGE)){
        xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"authentication ticket cannot accept this decision");return XMQTT_ADMISSION_ERROR;}
    if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline)){xrtMutexUnlock(&C->Gate);return xmqttAuthDecisionTimeout(C);}
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_ERROR;}
    T->Answered=true;A->Decision=Accept?1:-1;A->StepDeadline=INFINITY;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_ACCEPTED;
}
XMQTT_API xmqttclientadmission xmqttAuthRespond(xmqttauthticket* T,const xmqttpacket* P)
{
    if(!T || !P || !xrtMemRangeValid(P,sizeof(*P)) || P->Type!=XMQTT_PACKET_AUTH || P->Reason!=0x18){xmqttClientError(XERR_ARGUMENT,"expected authentication continuation");return XMQTT_ADMISSION_ERROR;}
    xmqttclient* C=T->Client;xmqttauthstate* A=C->Auth;size_t Bytes=0;
    if(!xmqttPacketSize5(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes))return XMQTT_ADMISSION_ERROR;
    if(!xmqttAuthMethod(A,P)){xmqttClientError(XERR_ARGUMENT,"authentication method changed");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(A->Ticket!=T || !T->Taken || T->Answered || T->Phase!=XMQTT_AUTH_CHALLENGE){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"authentication ticket is stale");return XMQTT_ADMISSION_ERROR;}
    if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline)){xrtMutexUnlock(&C->Gate);return xmqttAuthDecisionTimeout(C);}
    if(A->Out || Bytes>A->Config.MaxExchangeBytes-A->Bytes){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    xrtMutexUnlock(&C->Gate);
    xmqttclientadmission S=XMQTT_ADMISSION_ERROR;xmqttauthwire* W=xmqttAuthWireAllocate(C,P,Bytes,&S);if(!W)return S;
    bool Late=false;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING)S=XMQTT_ADMISSION_CLOSED;
    else if(A->Ticket!=T || !T->Taken || T->Answered || T->Phase!=XMQTT_AUTH_CHALLENGE){xmqttClientError(XERR_STATE,"authentication ticket is stale");}
    else if(xrtTimer()>=fmin(A->Deadline,A->StepDeadline))Late=true;
    else if(Bytes>A->Config.MaxExchangeBytes-A->Bytes || A->Out)S=XMQTT_ADMISSION_OVERLOADED;
    else if(A->ProtocolReady && Bytes>C->Peer.SendPacketBytes){xmqttClientError(XERR_RANGE,"AUTH exceeds peer packet limit");}
    else if(xmqttClientWakeLocked(C)){A->Out=W;A->Bytes+=Bytes;T->Answered=true;A->Decision=0;A->StepDeadline=INFINITY;S=XMQTT_ADMISSION_ACCEPTED;}
    xrtMutexUnlock(&C->Gate);if(S!=XMQTT_ADMISSION_ACCEPTED)xmqttAuthWireDrop(W);
    return Late?xmqttAuthDecisionTimeout(C):S;
}
XMQTT_API xmqttclientadmission xmqttClientReauthenticateAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    double Deadline=Timeout?xmqttClientDeadline(xrtTimer(),Timeout):-INFINITY;
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<0 || P->Type!=XMQTT_PACKET_AUTH || P->Reason!=0x19){xmqttClientError(XERR_ARGUMENT,"invalid reauthentication request");return XMQTT_ADMISSION_ERROR;}
    xmqttauthstate* A=C->Auth;if(!A){xmqttClientError(XERR_STATE,"connection has no enhanced authentication");return XMQTT_ADMISSION_ERROR;}
    size_t Bytes=0;if(!xmqttPacketSize5(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Bytes))return XMQTT_ADMISSION_ERROR;
    if(!xmqttAuthMethod(A,P)){xmqttClientError(XERR_ARGUMENT,"reauthentication method changed");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(!A->Verified){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"initial authentication is not verified");return XMQTT_ADMISSION_ERROR;}
    if(A->Active || A->Out || C->DisconnectReserved || Bytes>A->Config.MaxExchangeBytes){xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    xrtMutexUnlock(&C->Gate);
    xmqttclientadmission S=XMQTT_ADMISSION_ERROR;xmqttauthwire* W=xmqttAuthWireAllocate(C,P,Bytes,&S);if(!W)return S;
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttauthbox* B=xmqttBoxAllocate(C->Quota,sizeof(*B),&Failure);xfuture* F=NULL;xpromise* Promise=NULL;xcancel* Cancel=NULL;
    if(!B){if(Failure==XMQTT_STATUS_LIMIT)S=XMQTT_ADMISSION_OVERLOADED;goto Done;}
    B->Box=(xmqttbox){C->Quota,sizeof(*B)};Promise=xrtPromiseCreate(&F,NULL);if(!Promise)goto Done;
    xmqttClientRef(C);if(!xrtPromiseProducerBindTakeV2(Promise,(xrtownershipref){C,&xmqttPrivateOps},&xmqttReadyProducer,&xmqttAuthCancellation)){xmqttClientDrop(C);goto Done;}
    Cancel=xrtPromiseCancelToken(Promise);if(!Cancel)goto Done;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING)S=XMQTT_ADMISSION_CLOSED;
    else if(!A->Verified){xmqttClientError(XERR_STATE,"initial authentication is not verified");}
    else if(A->Active || A->Out || C->DisconnectReserved || Bytes>A->Config.MaxExchangeBytes)S=XMQTT_ADMISSION_OVERLOADED;
    else if(Bytes>C->Peer.SendPacketBytes){xmqttClientError(XERR_RANGE,"AUTH exceeds peer packet limit");}
    else if(xmqttClientWakeLocked(C)){A->Active=A->Reauth=true;A->AwaitServer=false;A->Out=W;A->Promise=Promise;A->Cancel=Cancel;A->Result=B;
        A->Rounds=A->Sent=0;A->Bytes=Bytes;A->Deadline=Deadline;A->StepDeadline=INFINITY;S=XMQTT_ADMISSION_ACCEPTED;}
    xrtMutexUnlock(&C->Gate);
Done:
    if(S==XMQTT_ADMISSION_ACCEPTED){*Out=F;return S;}
    xrtCancelDestroy(Cancel);xrtPromiseDestroy(Promise);xrtFutureDestroy(F);if(B)xmqttAuthResultFree(B,NULL);xmqttAuthWireDrop(W);return S;
}
XMQTT_API xmqttclientadmission xmqttClientReauthenticate(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously authenticate from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientReauthenticateAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
#endif

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
{(void)S;xmqttclient* C=Data;C->NativeClosed=true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session)C->Session->EndAt=xrtTimer();
#endif
    xrtMutexLock(&C->Gate);bool Expected=C->ApplicationClose && C->ApplicationNativeClosing && Result==XNET_RESULT_OK;xrtMutexUnlock(&C->Gate);
    if(!Expected)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,Cause);
    xmqttClientWake(C);
}
static const xnetstreameventsownershipv1 xmqttClientEvents={sizeof(xnetstreameventsownershipv1),
    {xmqttClientOpen,xmqttClientRead,xmqttClientEnd,NULL,xmqttClientLow,xmqttClientDrain,xmqttClientClosed},xmqttClientDrop,&xmqttPrivateOps};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
static xmqttclientstatus xmqttDialFailure(xnetresult Result,const xerror* Cause)
{
    if(Result==XNET_RESULT_TIMEOUT)return XMQTT_STATUS_TIMEOUT;
    if(Result==XNET_RESULT_CANCELLED)return XMQTT_STATUS_CANCELLED;
    xerrkind Kind=Cause?xrtErrorKind(Cause):XERR_NONE;
    return Kind==XERR_MEMORY?XMQTT_STATUS_MEMORY:(Kind==XERR_RANGE || Kind==XERR_AGAIN)?XMQTT_STATUS_LIMIT:XMQTT_STATUS_TRANSPORT;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
static void xmqttTlsResumeIncrement(uint64* Value)
{if(*Value!=UINT64_MAX)++*Value;}
static bool xmqttTlsResumeBytes(xtlsresume* R,size_t* Bytes)
{
    xtlsresumeinfo I;if(!xrtTlsResumeInfo(R,&I))return false;
    const size_t Sizes[]={I.Ticket.Size,I.Secret.Size,I.ServerName.Size,I.Protocol.Size,I.PeerIdentity.Size};size_t N=0;
    for(size_t At=0;At<sizeof(Sizes)/sizeof(*Sizes);++At){if(Sizes[At]>SIZE_MAX-N)return false;N+=Sizes[At];}
    size_t Count=xrtTlsResumePeerCertificateCountV1(R);
    for(size_t At=0;At<Count;++At){const xx509cert* Cert=xrtTlsResumePeerCertificateV1(R,At);if(!Cert || Cert->Raw.Size>SIZE_MAX-N)return false;N+=Cert->Raw.Size;}
    *Bytes=N;return true;
}
/* Native already owns bounded immutable credentials. Move one reference; no
 * new node, credential copy, Work item or cross-thread native Session call. */
static void xmqttTlsTicket(xtlsstream* S,ptr Data)
{
    xmqttclient* C=Data;xmqtttlsresumestate* R=&C->TLS->Resume;xtlssession* Session=xrtTlsStreamSession(S);
    for(size_t At=0;At<XTLS_CLIENT_RESUME_LIMIT_MAX;++At){xtlsresume* Next=xrtTlsClientTakeResume(Session);if(!Next)break;
        size_t Bytes=0;bool Valid=xmqttTlsResumeBytes(Next,&Bytes);xtlsresume* Drop=Next;
        xrtMutexLock(&C->Gate);xmqttTlsResumeIncrement(&R->Info.Captured);
        if(Valid && R->Info.Enabled && Bytes<=C->Config.MaxPrivateBytes-C->Info.PrivateBytes+R->Info.CachedBytes){
            Drop=R->Value;if(Drop)xmqttTlsResumeIncrement(&R->Info.Evicted);
            C->Info.PrivateBytes-=R->Info.CachedBytes;C->Info.PrivateBytes+=Bytes;R->Value=Next;R->Info.CachedBytes=Bytes;
        }else xmqttTlsResumeIncrement(&R->Info.Rejected);
        R->Notify=true;xrtMutexUnlock(&C->Gate);xrtTlsResumeRelease(Drop);
    }
    xmqttClientWake(C);
}
static bool xmqttTlsResumeOutput(xmqttclient* C,ptr Out,size_t Bytes)
{
    if(!C || !Out || !xrtMemRangeValid(Out,Bytes) || !C->TLS ||
       xrtMemRangesOverlap(Out,Bytes,C,sizeof(*C)) || xrtMemRangesOverlap(Out,Bytes,C->TLS,sizeof(*C->TLS)))
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS resume client/output or owner overlap");
    size_t Scratch=(size_t)(C->Props-C->Scratch)+C->Config.Codec.MaxProperties;
    if(xrtMemRangesOverlap(Out,Bytes,C->Scratch,Scratch*sizeof(*C->Scratch)))return xmqttClientError(XERR_ARGUMENT,"TLS resume output overlaps client scratch");
    return true;
}
XMQTT_API bool xmqttClientTlsResumeInfo(xmqttclient* C,xmqttclienttlsresumeinfo* Out)
{
    if(!xmqttTlsResumeOutput(C,Out,sizeof(*Out)))return false;
    xrtMutexLock(&C->Gate);xmqttclienttlsresumeinfo I=C->TLS->Resume.Info;xrtMutexUnlock(&C->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientTlsTakeResume(xmqttclient* C,xtlsresume** Out)
{
    if(!xmqttTlsResumeOutput(C,Out,sizeof(*Out)))return false;
    xrtMutexLock(&C->Gate);xmqtttlsresumestate* R=&C->TLS->Resume;xtlsresume* Value=R->Value;
    if(Value){R->Value=NULL;C->Info.PrivateBytes-=R->Info.CachedBytes;R->Info.CachedBytes=0;}
    xrtMutexUnlock(&C->Gate);if(!Value)return false;*Out=Value;return true;
}
#endif
static void xmqttTlsWritten(xtlsstream* S,ptr Data)
{(void)S;xmqttClientWake(Data);}
static void xmqttTlsOpen(xtlsstream* S,ptr Data)
{
    xmqttclient* C=Data;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool Resumed=xrtTlsClientResumed(xrtTlsStreamSession(S));
    xrtMutexLock(&C->Gate);C->TLS->Resume.Info.Established=true;C->TLS->Resume.Info.Resumed=Resumed;xrtMutexUnlock(&C->Gate);
#endif
    if(!xrtTlsStreamSetWriteProgress(S,xmqttTlsWritten))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());
    else C->TLS->Open=true;
    xmqttClientWake(C);
}
static void xmqttTlsRead(xtlsstream* S,const xnetbuf* B,ptr Data)
{(void)S;(void)B;xmqttClientWake(Data);}
static void xmqttTlsEnd(xtlsstream* S,ptr Data)
{(void)S;xmqttclient* C=Data;C->ReadEnded=true;xmqttClientWake(C);}
static void xmqttTlsClosed(xtlsstream* S,xnetresult Result,const xerror* Cause,ptr Data)
{
    xmqttclient* C=Data;C->NativeClosed=true;C->TLS->Open=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session)C->Session->EndAt=xrtTimer();
#endif
    while(C->TLS->Head)xmqttTlsReleaseFence(C,C->TLS->Head);
    xrtMutexLock(&C->Gate);bool Expected=C->ApplicationClose && C->ApplicationNativeClosing && Result==XNET_RESULT_OK;xrtMutexUnlock(&C->Gate);
    if(!Expected)xmqttClientFail(C,xmqttDialFailure(Result,Cause),Cause);
    C->TLS->Stream=NULL;xmqttClientWake(C);
    /* Break the actual Client -> TLS -> Client cycle. Native/runtime references
     * retain TLS and its owned Client credit through final Session destruction.
     * The separate real TCP caller reference remains for WaitRetired. */
    xrtTlsStreamDestroy(S);
}
static const xtlsstreamownershipv1 xmqttTlsEvents={sizeof(xtlsstreamownershipv1),
    {xmqttTlsOpen,xmqttTlsRead,xmqttTlsEnd,xmqttTlsWritten,xmqttTlsWritten,xmqttTlsClosed,
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    xmqttTlsTicket
#else
    NULL
#endif
    },xmqttClientDrop,&xmqttPrivateOps};
static bool xmqttTlsAttach(xmqttclient* C,xnetstream* S)
{
    if(!xrtNetStreamRef(S))return false; /* Client's independent retirement handle. */
    xmqttClientRef(C);
    if(xrtTlsStreamAttachOwnedV1(S,C->TLS->Plan.Session,&C->TLS->Plan.Config,C,&xmqttTlsEvents,&C->TLS->Stream)){
        C->TLS->Plan.Session=NULL;return true;
    }
    xmqttClientDrop(C);xrtNetStreamDestroy(S);return false;
}
#endif
static void xmqttClientDialDone(xnetdial* D,xnetresult Result,xnetstream* Stream,const xerror* Cause,ptr Data)
{
    xmqttclient* C=Data;
    /* Pair with factory publication before accessing its immutable setup. The
     * owning Worker serializes all subsequent transport/state operations. */
    xrtMutexLock(&C->Gate);C->Dial=NULL;C->Stream=Stream;xrtMutexUnlock(&C->Gate);
    if(Stream){
        if(xmqttClientClosing(C))(void)xrtNetStreamAbort(Stream);
        else {
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
            if(C->TLS){if(!xmqttTlsAttach(C,Stream)){xmqttClientFail(C,xmqttDialFailure(XNET_RESULT_ERROR,xrtGetError()),xrtGetError());(void)xrtNetStreamAbort(Stream);}}
            else
#endif
            {xmqttClientRef(C);
            if(!xrtNetStreamSetEventsOwnedV1(Stream,C,&xmqttClientEvents)){
                xmqttClientDrop(C);const xerror* Error=xrtNetStreamError(Stream);if(!Error)Error=xrtGetError();
                xmqttClientFail(C,xmqttDialFailure(XNET_RESULT_ERROR,Error),Error);(void)xrtNetStreamAbort(Stream);}
            else {xnetstreamstats I;
                if(!xrtNetStreamStats(Stream,&I))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());
                else if(I.ReadEnded){C->ReadEnded=true;xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);}}
            }
        }
    }else {C->NativeClosed=true;xmqttClientFail(C,xmqttDialFailure(Result,Cause),Cause);}
    /* A queued client turn also holds C through the physical Done/Drop tail.
     * Initially callback-less winners retain buffered input; that turn reads it. */
    xmqttClientWake(C);
    /* Consume the Client's caller reference after detaching its field. Core's
     * active Done invocation owns D through its callback tail. Successful
     * connections need no completed controller, candidates or old failures. */
    xrtNetDialDestroy(D);
}
static const xnetdialownershipv1 xmqttClientDialEvents={sizeof(xnetdialownershipv1),xmqttClientDialDone,xmqttClientDrop,&xmqttPrivateOps};
#endif
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
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth)xrtSecureZero(C->Input,C->InputCapacity);
#endif
    xrtFree(C->Input);
    xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=C->InputCapacity;xrtMutexUnlock(&C->Gate);
    C->Input=P;C->InputCapacity=Capacity;return true;
}
static void* xmqttAliasAllocate(xmqttclient* C,size_t Bytes,bool Zero)
{
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return NULL;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    void* Data=Zero?xrtCalloc(1,Bytes):xrtMalloc(Bytes);
    if(!Data){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());}
    return Data;
}
/* Called once per accepted PUBLISH, including active QoS2 duplicates. Only the
 * owner accesses this table. Messages never retain an entry or its Client. */
static bool xmqttReceiveAlias(xmqttclient* C,const xmqttpacket* P,xstrview* Resolved)
{
    uint16 Id=0;
    for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){Id=(uint16)P->Properties.Data[I].Value.Number;break;}
    if(!Id)return true; /* The codec rejects zero/duplicate alias properties. */
    if(Id>C->Peer.ReceiveAliasMaximum){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x94);return false;}
    xstrview Topic=P->Body.Publish.Topic;
    if(!Topic.Size){
        xmqttalias* A=C->ReceiveAliases?C->ReceiveAliases[Id-1]:NULL;
        if(!A){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x82);return false;}
        *Resolved=(xstrview){A->Topic,A->Size};return true;
    }
    if(!C->ReceiveAliases){C->ReceiveAliases=xmqttAliasAllocate(C,(size_t)C->Peer.ReceiveAliasMaximum*sizeof(*C->ReceiveAliases),true);if(!C->ReceiveAliases)return false;}
    xmqttalias* Old=C->ReceiveAliases[Id-1];
    if(Old && Topic.Size<=Old->Capacity){memcpy(Old->Topic,Topic.Data,Topic.Size);Old->Size=Topic.Size;return true;}
    xmqttalias* A=xmqttAliasAllocate(C,sizeof(*A)+Topic.Size,false);if(!A)return false;
    A->Size=A->Capacity=Topic.Size;memcpy(A->Topic,Topic.Data,Topic.Size);C->ReceiveAliases[Id-1]=A;
    if(Old){size_t Bytes=sizeof(*Old)+Old->Capacity;xrtFree(Old);xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);}
    return true;
}
static bool xmqttReceivePrepare(xmqttclient* C)
{
    if(C->ReceiveFlow)return true;
    size_t Capacity=C->Config.MaxMessages;
    if(Capacity>C->Peer.ReceiveMaximum)Capacity=C->Peer.ReceiveMaximum;
    size_t Bytes=sizeof(xmqttreceiveflow)+Capacity*sizeof(uint16);
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    xmqttreceiveflow* F=xrtCalloc(1,Bytes);
    if(!F){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    F->Capacity=Capacity;C->ReceiveFlow=F;return true;
}
static bool xmqttReceiveTwoPrepare(xmqttclient* C)
{
    if(C->ReceiveTwo)return true;
    size_t Capacity=C->Config.MaxMessages;
    if(Capacity>C->Peer.ReceiveMaximum)Capacity=C->Peer.ReceiveMaximum;
    Capacity*=2;size_t Bytes=sizeof(xmqttreceivetwo)+Capacity*sizeof(xmqttreceiveack);
    xrtMutexLock(&C->Gate);
    if(Bytes>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    C->Info.PrivateBytes+=Bytes;xrtMutexUnlock(&C->Gate);
    xmqttreceivetwo* F=xrtCalloc(1,Bytes);
    if(!F){xrtMutexLock(&C->Gate);C->Info.PrivateBytes-=Bytes;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
    F->Capacity=Capacity;C->ReceiveTwo=F;return true;
}
static void xmqttReceiveTwoQueue(xmqttclient* C,uint16 Id,uint8 Type,bool Unknown)
{
    xmqttreceivetwo* F=C->ReceiveTwo;
    unsigned Flag=Type==0x50?XMQTT_RX_REC_QUEUED:Unknown?XMQTT_RX_UNKNOWN_QUEUED:XMQTT_RX_COMP_QUEUED;
    if(F->State[Id]&Flag)return;
    F->Acks[(F->Head+F->Count)%F->Capacity]=(xmqttreceiveack){Id,Type,Unknown};++F->Count;
    F->State[Id]|=(unsigned char)Flag;
}
/* Preflight before consuming the buffered frame. Queue exhaustion can always
 * be relieved by writing ACKs, whereas active ids retain receive credit. */
static bool xmqttReceiveRoom(xmqttclient* C,const xmqttpacket* P)
{
    if(!xmqttClientProtocolReady(C))return true;
    unsigned Qos=P->Type==XMQTT_PACKET_PUBLISH?(P->Flags>>1)&3u:0;
    if(!Qos && P->Type!=XMQTT_PACKET_PUBREL)return true;
    unsigned State=C->ReceiveTwo?C->ReceiveTwo->State[P->Identifier]:0;
    if(Qos){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session && Qos==1 && xmqttInternalSessionReceiveState(C->Session->State,C,P->Identifier)>=2){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
#endif
        if((State&XMQTT_RX_ACTIVE) && (Qos!=2 || (State&XMQTT_RX_RELEASED))){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        bool Duplicate=Qos==2 && (State&XMQTT_RX_ACTIVE);
        if(!Duplicate){
            if(Qos==2 && C->ReceiveFlow){xmqttreceiveflow* F=C->ReceiveFlow;
                for(size_t I=0;I<F->Count;++I)if(F->Identifiers[(F->Head+I)%F->Capacity]==P->Identifier){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}}
            size_t Inflight=(C->ReceiveFlow?C->ReceiveFlow->Count:0)+(C->ReceiveTwo?C->ReceiveTwo->Inflight:0);
            if(Inflight>=C->Peer.ReceiveMaximum){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x93);return false;}
        }
        if(Qos==1){
            if(C->ReceiveFlow && C->ReceiveFlow->Count==C->ReceiveFlow->Capacity){C->ReceivePaused=true;return false;}
            return true;
        }
    }
    if(!xmqttReceiveTwoPrepare(C))return false;
    unsigned Flag=Qos==2?XMQTT_RX_REC_QUEUED:State&XMQTT_RX_ACTIVE?XMQTT_RX_COMP_QUEUED:XMQTT_RX_UNKNOWN_QUEUED;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session && P->Type==XMQTT_PACKET_PUBREL && xmqttInternalSessionReceiveState(C->Session->State,C,P->Identifier)>=2)Flag=XMQTT_RX_COMP_QUEUED;
#endif
    if(!(State&Flag) && C->ReceiveTwo->Count==C->ReceiveTwo->Capacity){C->ReceivePaused=true;return false;}
    return true;
}
static bool xmqttClientProtocolReady(const xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && C->Auth->ProtocolReady)return true;
#endif
    return C->Info.Ready;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static void xmqttClientSessionUnreserve(xmqttclient* C)
{
    xmqttsessiondriver* D=C->Session;if(!D || !D->ReservedCount)return;
    xrtMutexLock(&C->Gate);C->Info.Requests-=D->ReservedCount;C->Info.OutgoingBytes-=D->ReservedBytes;D->ReservedCount=D->ReservedBytes=0;xrtMutexUnlock(&C->Gate);
}
static void xmqttClientSessionEnd(xmqttclient* C)
{
    xmqttsessiondriver* D=C->Session;if(!D || !D->Opened || !C->NativeClosed)return;
    xmqttpacket Q;xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Q);xmqttproperty P={0};
    if(D->EndExpiry){P.Id=XMQTT_PROPERTY_SESSION_EXPIRY;P.Type=XMQTT_PROPERTY_UINT32;P.Value.Number=D->Expiry;Q.Properties=(xmqttpropertylist){&P,1};}
    if(xmqttInternalSessionEnd(D->State,C,D->EndExpiry?&Q:NULL,D->EndAt)!=XMQTT_SESSION_DONE)abort();
    D->Opened=false;
}
static void xmqttClientSessionFinish(xmqttclient* C,xmqttrequest* R)
{
    xmqttsessiondriver* D=C->Session;xmqttsessionrequest* B=xmqttSessionRequest(R);if(!B->Packet)return;
    if(!(R->Capabilities&XMQTT_TX_BORROW) && !R->Offset){xmqttsessionpacketinfo I;
        if(!xmqttSessionPacketInfo(B->Packet,&I))abort();
        if(I.Active && !I.Sent && xmqttInternalSessionWithdraw(D->State,C,B->Packet)!=XMQTT_SESSION_DONE)abort();
    }
    if(D->Selected==R){const xmqttsessionpacket* P=D->Frame.Packet;D->Selected=NULL;memset(&D->Frame,0,sizeof(D->Frame));xmqttSessionPacketRelease(P);}
}
static bool xmqttClientSessionSent(xmqttclient* C,xmqttrequest* R)
{
    xmqttsessiondriver* D=C->Session;if(!D || D->Selected!=R)return true;
    xmqttsessionstatus S=xmqttInternalSessionSent(D->State,C,&D->Frame);
    if(S!=XMQTT_SESSION_DONE){xmqttClientFail(C,XMQTT_STATUS_SESSION,xrtGetError());return false;}
    const xmqttsessionpacket* P=D->Frame.Packet;D->Selected=NULL;memset(&D->Frame,0,sizeof(D->Frame));xmqttSessionPacketRelease(P);return true;
}
static xmqttclientstatus xmqttClientSessionSelect(xmqttclient* C,xmqttrequest* R,bool* Empty)
{
    *Empty=false;xmqttsessiondriver* D=C->Session;xmqttsessionrequest* B=xmqttSessionRequest(R);
    if(D->Selected){if(D->Selected!=R)*Empty=true;return XMQTT_STATUS_OK;}
    if(!B->Packet){
        xmqttpacket Q;xmqttcodecconfig K=C->Config.Codec;if(R->Capabilities&XMQTT_TX_ALIAS)K.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES;
        xmqttcodecbuffers Buffers={C->Props,C->Config.Codec.MaxProperties,NULL,0};
        xmqttcodecstatus Read=C->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){R->Wire,R->Bytes},XMQTT_CLIENT_TO_SERVER,&K,&Buffers,&Q,NULL):xmqttPacketRead311((xbytesview){R->Wire,R->Bytes},XMQTT_CLIENT_TO_SERVER,&K,&Buffers,&Q,NULL);
        if(Read!=XMQTT_CODEC_DONE)return XMQTT_STATUS_INITIALIZATION;
        if(R->Capabilities&XMQTT_TX_ALIAS){xmqttsendtopic* T=xmqttSendAliasRequest(R)->Topic;Q.Body.Publish.Topic=(xstrview){T->Data,T->Size};}
        xmqttsessionstatus S=xmqttInternalSessionPublish(D->State,C,&Q,&B->Packet);
        if(S!=XMQTT_SESSION_DONE)return S==XMQTT_SESSION_LIMIT?XMQTT_STATUS_LIMIT:S==XMQTT_SESSION_RESTRICTED?XMQTT_STATUS_RESTRICTED:xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_INITIALIZATION;
    }
    xmqttsessionstatus S=xmqttInternalSessionSelect(D->State,C,B->Packet,&D->Frame);
    if(S==XMQTT_SESSION_EMPTY){*Empty=true;return XMQTT_STATUS_OK;}
    if(S!=XMQTT_SESSION_DONE)return S==XMQTT_SESSION_RESTRICTED?XMQTT_STATUS_RESTRICTED:XMQTT_STATUS_SESSION;
    D->Selected=R;C->Partial=R;return XMQTT_STATUS_OK;
}
static bool xmqttClientSessionEarlierSent(xmqttrequest* R)
{
    if(!(R->Capabilities&XMQTT_TX_BORROW))return false;
    xmqttsessionpacketinfo I;return xmqttSessionPacketInfo(xmqttSessionRequest(R)->Packet,&I) && I.Sent;
}
static bool xmqttClientSessionAck(xmqttclient* C,const xmqttpacket* P)
{
    xmqttsessionstatus S=xmqttInternalSessionAcknowledge(C->Session->State,C,P);
    if(S==XMQTT_SESSION_DONE || S==XMQTT_SESSION_DUPLICATE || S==XMQTT_SESSION_REJECTED)return true;
    xmqttClientFail(C,S==XMQTT_SESSION_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY?XMQTT_STATUS_MEMORY:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;
}
static bool xmqttClientSessionRestore(xmqttclient* C)
{
    xmqttsessiondriver* D=C->Session;const xmqttsessionpacket* Previous=NULL;xmqttrequest* Last=NULL;
    for(;;){if(xmqttClientClosing(C))return false;const xmqttsessionpacket* Packet=NULL;xmqttsessionstatus S=xmqttInternalSessionVisit(D->State,C,Previous,&Packet);
        if(S==XMQTT_SESSION_EMPTY)break;
        if(S!=XMQTT_SESSION_DONE){xmqttClientFail(C,XMQTT_STATUS_LIMIT,xrtGetError());return false;}
        xmqttsessionpacketinfo I;xmqttSessionPacketInfo(Packet,&I);size_t Bytes=I.Stage==XMQTT_SESSION_PUBREL?0:I.WireBytes;
        bool TLS=xmqttClientUsesTls(C);
        size_t Extra=xmqttRequestExtra(0,false,TLS)+xmqttSessionRequestExtra(Bytes,false,TLS,true);uint16 Id=0;
        xrtMutexLock(&C->Gate);xmqttclientadmission A=Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes?XMQTT_ADMISSION_OVERLOADED:xmqttIdentifierReserveImpl(C,I.Identifier,&Id,true);
        if(A!=XMQTT_ADMISSION_ACCEPTED){xrtMutexUnlock(&C->Gate);xmqttSessionPacketRelease(Packet);xmqttClientFail(C,A==XMQTT_ADMISSION_OVERLOADED?XMQTT_STATUS_LIMIT:XMQTT_STATUS_MEMORY,A==XMQTT_ADMISSION_ERROR?xrtGetError():NULL);return false;}
        if(!D->ReservedCount || Bytes>D->ReservedBytes)abort();
        --D->ReservedCount;D->ReservedBytes-=Bytes;C->Info.PrivateBytes+=Extra;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
        xmqttrequest* R=xrtCalloc(1,sizeof(*R)+Extra);
        if(!R){xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Id);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;C->Info.PrivateBytes-=Extra;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);xmqttSessionPacketRelease(Packet);xmqttClientFail(C,XMQTT_STATUS_MEMORY,xrtGetError());return false;}
        R->Client=C;R->Bytes=Bytes;R->Identifier=Id;R->Qos=I.Qos;R->Retain=(xmqttSessionPacket(Packet)->Flags&1u)!=0;R->Type=XMQTT_PACKET_PUBLISH;R->Capabilities=XMQTT_TX_SESSION|XMQTT_TX_BORROW|(TLS?XMQTT_TX_TLS:0);R->Deadline=INFINITY;
        xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);xmqttsessionrequest* B=xmqttSessionRequest(R);B->Packet=Packet;R->Result.Write=(xmqttwritebox*)(B+1);
        R->Result.Write->Value.PubrecReceived=I.PubrecReceived;R->Result.Write->Value.PubrecReason=I.PubrecReason;
        if(I.Qos==2){R->State.Release.Pending=I.Stage==XMQTT_SESSION_PUBREL;R->State.Release.Wire[0]=0x62;R->State.Release.Wire[1]=2;R->State.Release.Wire[2]=(unsigned char)(Id>>8);R->State.Release.Wire[3]=(unsigned char)Id;}
        R->Next=Last?Last->Next:C->Requests;R->Previous=Last;
        if(R->Next)R->Next->Previous=R;else C->RequestTail=R;
        if(Last)Last->Next=R;else C->Requests=R;Last=R;xmqttRequestMapInsert(C,R);Previous=Packet;
    }
    if(D->ReservedCount || D->ReservedBytes)abort();
    return true;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static bool xmqttClientSessionAuthenticate(xmqttclient* C)
{
    if(xmqttClientClosing(C))return false;
    xmqttnegotiated N;xmqttsessionstatus S=xmqttInternalSessionOpen(C->Session->State,C,&C->SetupPacket,
        xmqttMessagePacket(C->ReadyBox->Value.Connack),xrtTimer(),&N);
    if(S!=XMQTT_SESSION_DONE && S!=XMQTT_SESSION_RESET){xmqttClientFail(C,S==XMQTT_SESSION_STATE_ERROR?XMQTT_STATUS_SESSION:S==XMQTT_SESSION_ERROR?XMQTT_STATUS_INITIALIZATION:S==XMQTT_SESSION_REJECTED?XMQTT_STATUS_REJECTED:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;}
    C->Session->Opened=true;if(S==XMQTT_SESSION_RESET)xmqttClientSessionUnreserve(C);
    if(!xmqttClientSessionRestore(C) || xmqttClientClosing(C))return false;
    if(xrtTimer()>=C->SetupDeadline){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);return false;}
    return true;
}
#endif
static size_t xmqttClientSessionRefs(xmqttclient* C,size_t At,size_t Bytes,xmqttrequest* R,xnetref Refs[2])
{
    xmqttsessionframe* F=&C->Session->Frame;xbytesview Spans[]={F->Prefix,F->Tail};size_t Count=0;
    for(unsigned I=0;I<2 && Bytes;++I){if(At>=Spans[I].Size){At-=Spans[I].Size;continue;}size_t N=Spans[I].Size-At;if(N>Bytes)N=Bytes;
        Refs[Count++]=(xnetref){Spans[I].Data+At,N,xmqttRequestNativeDrop,R};Bytes-=N;At=0;}
    return Count;
}
static void xmqttClientSessionDisconnect(xmqttclient* C,xmqttrequest* R)
{
    if(!C->Session || R->Type!=XMQTT_PACKET_DISCONNECT || C->Config.Version!=XMQTT_VERSION_5)return;
    xmqttpacket Q;xmqttcodecbuffers B={C->Props,C->Config.Codec.MaxProperties,NULL,0};
    if(xmqttPacketRead5((xbytesview){R->Wire,R->Bytes},XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&B,&Q,NULL)!=XMQTT_CODEC_DONE)abort();
    for(size_t I=0;I<Q.Properties.Count;++I)if(Q.Properties.Data[I].Id==XMQTT_PROPERTY_SESSION_EXPIRY){C->Session->EndExpiry=true;C->Session->Expiry=Q.Properties.Data[I].Value.Number;}
}
#endif
static bool xmqttClientFrame(xmqttclient* C,const xmqttpacket* P)
{
    xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(P->Type==XMQTT_PACKET_AUTH)return xmqttAuthFrame(C,P);
#endif
    if(!xmqttClientProtocolReady(C)){
        if(P->Type!=XMQTT_PACKET_CONNACK || !C->Startup || C->SetupOffset!=C->Startup->Bytes){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,NULL);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttnegotiationconfig K;xmqttNegotiationConfigInit(&K);K.Version=C->Config.Version;K.Codec=C->Config.Codec;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        /* Retained-state validity is checked by Session Open. Authenticated
         * composition defers that mutation/check until local proof acceptance. */
        K.HasSessionState=C->Session!=NULL;
#endif
        xmqttnegotiated N;xmqttnegotiationissue Issue;
        xmqttnegotiationstatus S;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
           && !C->Auth
#endif
        ){xmqttsessionstatus J=xmqttInternalSessionOpen(C->Session->State,C,&C->SetupPacket,&M->Packet,xrtTimer(),&N);
            S=J==XMQTT_SESSION_DONE || J==XMQTT_SESSION_RESET?XMQTT_NEGOTIATION_DONE:J==XMQTT_SESSION_REJECTED?XMQTT_NEGOTIATION_REJECTED:J==XMQTT_SESSION_STATE_ERROR?XMQTT_NEGOTIATION_STATE_ERROR:J==XMQTT_SESSION_ERROR?XMQTT_NEGOTIATION_LOCAL_ERROR:XMQTT_NEGOTIATION_PROTOCOL_ERROR;
            if(J==XMQTT_SESSION_DONE || J==XMQTT_SESSION_RESET){C->Session->Opened=true;if(J==XMQTT_SESSION_RESET)xmqttClientSessionUnreserve(C);}
        }else
#endif
        S=xmqttNegotiate(&C->SetupPacket,&M->Packet,&K,&N,&Issue);
        if(S<0){xmqttMessageRelease(M);xmqttClientFail(C,S==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_STATUS_SESSION:S==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_STATUS_INITIALIZATION:XMQTT_STATUS_PROTOCOL,xrtGetError());return false;}
        if(N.Accepted && N.SessionPresent
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
           && !C->Session
#endif
        ){xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}
        xmqttreadybox* B=C->ReadyBox;B->Value.Connack=M;B->Value.Reason=P->Reason;
        if(N.ClientId.Size && !N.ServerAssignedId)N.ClientId=(xstrview){B->Id,N.ClientId.Size};
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(C->Auth && N.HasAuthMethod)for(size_t I=0;I<M->Packet.Properties.Count;++I)
            if(M->Packet.Properties.Data[I].Id==XMQTT_PROPERTY_AUTH_METHOD)N.AuthMethod=M->Packet.Properties.Data[I].Value.Text;
#endif
        B->Value.Negotiated=N;C->Peer=N;
        if(S==XMQTT_NEGOTIATION_REJECTED){xmqttClientFail(C,XMQTT_STATUS_REJECTED,NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
           && !C->Auth
#endif
           && (!xmqttClientSessionRestore(C) || xmqttClientClosing(C)))return false;
        if(C->Session && xrtTimer()>=C->SetupDeadline){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);return false;}
#endif
        /* Keep Alive is based on control output, not CONNACK arrival. A server
         * cannot send PINGRESP before its successful CONNACK, so a pre-CONNACK
         * ping starts its response budget only now. */
        C->NextPing=N.KeepAlive?C->PingDeadline+N.KeepAlive:INFINITY;
        C->PingDeadline=C->PingOutstanding?xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout):INFINITY;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        if(C->Auth)return xmqttAuthConnack(C,M);
#endif
        xrtMutexLock(&C->Gate);bool PublishReady=C->Info.State<XMQTT_CLIENT_CLOSING;
        if(PublishReady){C->Info.Ready=true;C->Info.State=XMQTT_CLIENT_OPEN;}xrtMutexUnlock(&C->Gate);
        if(!PublishReady)return false;
        xmqttReadyFinish(C,XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PINGRESP){C->PingOutstanding=false;C->PingDeadline=INFINITY;xrtMutexLock(&C->Gate);++C->Info.Pongs;xrtMutexUnlock(&C->Gate);return true;}
    if(P->Type==XMQTT_PACKET_SUBACK || P->Type==XMQTT_PACKET_UNSUBACK){
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        uint8 Expected=P->Type==XMQTT_PACKET_SUBACK?XMQTT_PACKET_SUBSCRIBE:XMQTT_PACKET_UNSUBSCRIBE;
        if(!R || R->Type!=Expected || R->Offset!=xmqttRequestWireBytes(R) || (!C->ProblemInfo && P->Properties.Count)){
            xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        size_t Count=R->State.TopicCount,Rejected=0;
        if(!(C->Config.Version==XMQTT_VERSION_311 && Expected==XMQTT_PACKET_UNSUBSCRIBE) && P->Body.Reasons.Size!=Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        /* Traverse our already validated immutable encoding, without another
         * allocation or retained caller descriptors. Skip filter bytes, compare
         * each successful grant with its original requested QoS. */
        size_t At=1;while(R->Wire[At++]&128u){}At+=2;
        if(C->Config.Version==XMQTT_VERSION_5){size_t Properties=0,Shift=0;unsigned char Byte;
            do{Byte=R->Wire[At++];Properties|=(size_t)(Byte&127u)<<Shift;Shift+=7;}while(Byte&128u);At+=Properties;}
        for(size_t I=0;I<P->Body.Reasons.Size;++I){unsigned char Reason=((const unsigned char*)P->Body.Reasons.Data)[I];
            if(Reason>=0x80)++Rejected;
            if(Expected==XMQTT_PACKET_SUBSCRIBE){size_t N=((size_t)R->Wire[At]<<8)|R->Wire[At+1];At+=N+2;
                if(Reason<0x80 && Reason>(R->Wire[At]&3u)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}++At;}
        }
        xmqttsubscriptionresult* V=&R->Result.Subscription->Value;
        V->Acknowledged=true;V->Granted=Count-Rejected;V->Rejected=Rejected;
        V->Ack=xmqttMessageCopy(C,P,&Failure,NULL);
        if(!V->Ack){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,Rejected?XMQTT_STATUS_REJECTED:XMQTT_STATUS_OK);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBACK){
        /* The codec already limits PUBACK properties to Reason String/User. */
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        bool Earlier=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        Earlier=R && xmqttClientSessionEarlierSent(R);
#endif
        if(!R || R->Type!=XMQTT_PACKET_PUBLISH || R->Qos!=1 || ((!R->Credit || R->Offset!=xmqttRequestWireBytes(R)) && !Earlier)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session && (R->Capabilities&XMQTT_TX_SESSION)){if(R->Acknowledged)return true;if(!xmqttClientSessionAck(C,P))return false;
            /* The ACK can belong to a fully sent prior-generation packet. A
             * partial replay cannot be retracted or followed by another frame;
             * commit the observed ACK, then retire this transport. */
            if(C->Session->Selected==R && R->Offset && R->Offset<xmqttRequestWireBytes(R)){xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}}
#endif
        R->Acknowledged=true;R->Result.Write->Value.Reason=P->Reason;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(!(R->Capabilities&XMQTT_TX_BORROW)){
#endif
        R->Result.Write->Value.Puback=xmqttMessageCopy(C,P,&Failure,NULL);
        if(!R->Result.Write->Value.Puback){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        }
        if(C->Session && C->Session->Selected==R && R->Offset && R->Offset<xmqttRequestWireBytes(R))return true;
#endif
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,P->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBREC || P->Type==XMQTT_PACKET_PUBCOMP){
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttrequest* R=xmqttRequestFind(C,P->Identifier);
        /* A late/repeated PUBCOMP must not complete a new flow or mint quota.
         * Its old generation is not encoded on the wire. Ignore until an actual
         * QoS2 flow has sent a complete PUBREL on this connection. */
        if(P->Type==XMQTT_PACKET_PUBCOMP && (!R || (R->Qos==2 && !R->State.Release.Sent)))return true;
        bool Earlier=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        Earlier=R && xmqttClientSessionEarlierSent(R);
#endif
        if(!R || R->Type!=XMQTT_PACKET_PUBLISH || R->Qos!=2 || ((!R->Credit || R->Offset!=xmqttRequestWireBytes(R)) && !Earlier)){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session && (R->Capabilities&XMQTT_TX_SESSION)){if(R->Acknowledged)return true;if(!xmqttClientSessionAck(C,P))return false;
            if(C->Session->Selected==R && C->Session->Frame.Type==XMQTT_PACKET_PUBLISH && R->Offset && R->Offset<xmqttRequestWireBytes(R)){xmqttClientFail(C,XMQTT_STATUS_SESSION,NULL);return false;}
            if(Earlier && C->Session->Selected!=R)R->Offset=R->Bytes;
        }
#endif
        xmqttwriteresult* V=&R->Result.Write->Value;
        if(P->Type==XMQTT_PACKET_PUBREC){
            if(R->Acknowledged)return true;
            if(!V->PubrecReceived || P->Reason>=0x80){
                V->PubrecReceived=true;V->PubrecReason=P->Reason;V->Reason=P->Reason;
                if(P->Reason>=0x80)R->Acknowledged=true;
                xmqttMessageRelease(V->Pubrec);V->Pubrec=NULL;
                xmqttmessage* M=NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
                if(!(R->Capabilities&XMQTT_TX_BORROW))
#endif
                M=xmqttMessageCopy(C,P,&Failure,NULL);
                if(!M
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
                   && !(R->Capabilities&XMQTT_TX_BORROW)
#endif
                ){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
                V->Pubrec=M;
            }
            if(P->Reason<0x80){
                /* Coalesce duplicates while one immutable PUBREL is unsent or
                 * partial; once a complete one was sent, schedule it again. */
                if(R->State.Release.Offset==4)R->State.Release.Offset=0;
                R->State.Release.Pending=true;return true;
            }
        }else {
            if(V->PubcompReceived)return true;
            V->PubcompReceived=true;V->Reason=P->Reason;R->Acknowledged=true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
            if(!(R->Capabilities&XMQTT_TX_BORROW)){
#endif
            V->Pubcomp=xmqttMessageCopy(C,P,&Failure,NULL);
            if(!V->Pubcomp){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
            }
#endif
        }
        if(R->Qos==2 && R->State.Release.Offset && R->State.Release.Offset<4)return true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session && C->Session->Selected==R && R->Offset && R->Offset<xmqttRequestWireBytes(R))return true;
#endif
        xmqttrequest** Link=R->Previous?&R->Previous->Next:&C->Requests;
        xmqttRequestFinish(C,Link,V->Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);return true;
    }
    if(P->Type==XMQTT_PACKET_PUBREL){
        if(!C->ProblemInfo && P->Properties.Count){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
        xmqttreceivetwo* F=C->ReceiveTwo;unsigned State=F->State[P->Identifier];
        bool Unknown=!(State&XMQTT_RX_ACTIVE);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session){uint8 Reason=0;xmqttsessionstatus S=xmqttInternalSessionReceive(C->Session->State,C,P->Identifier,3,&Reason);
            if(S!=XMQTT_SESSION_DONE && S!=XMQTT_SESSION_EMPTY){xmqttClientFail(C,XMQTT_STATUS_SESSION,xrtGetError());return false;}
            Unknown=S==XMQTT_SESSION_EMPTY;
            if(!Unknown && !(State&XMQTT_RX_ACTIVE))F->State[P->Identifier]|=XMQTT_RX_ACTIVE|XMQTT_RX_OLD;
        }
#endif
        if(!Unknown)F->State[P->Identifier]|=XMQTT_RX_RELEASED;
        xmqttReceiveTwoQueue(C,P->Identifier,0x70,Unknown);return true;
    }
    if(P->Type==XMQTT_PACKET_DISCONNECT){xrtMutexLock(&C->Gate);C->Info.ServerReason=P->Reason;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_SERVER_DISCONNECT,NULL);return false;}
    if(P->Type!=XMQTT_PACKET_PUBLISH){xmqttClientFail(C,XMQTT_STATUS_PROTOCOL,NULL);return false;}
    xstrview Resolved={0};if(!xmqttReceiveAlias(C,P,&Resolved))return false;
    unsigned Qos=(P->Flags>>1)&3u;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session && Qos==2 && xmqttInternalSessionReceiveState(C->Session->State,C,P->Identifier)>=2){
        if(!(C->ReceiveTwo->State[P->Identifier]&XMQTT_RX_ACTIVE)){C->ReceiveTwo->State[P->Identifier]|=XMQTT_RX_ACTIVE;++C->ReceiveTwo->Inflight;}
        xmqttReceiveTwoQueue(C,P->Identifier,0x50,false);return true;
    }
#endif
    if(Qos==2 && (C->ReceiveTwo->State[P->Identifier]&XMQTT_RX_ACTIVE)){xmqttReceiveTwoQueue(C,P->Identifier,0x50,false);return true;}
    if(Qos==1 && !xmqttReceivePrepare(C))return false;
    xmqttmessage* M=xmqttMessageCopy(C,P,&Failure,Resolved.Size?&Resolved:NULL);if(!M){xmqttClientFail(C,Failure,Failure==XMQTT_STATUS_MEMORY?xrtGetError():NULL);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    bool Prepared=false;
    if(C->Session && Qos==2){xmqttsessionstatus S=xmqttInternalSessionReceive(C->Session->State,C,P->Identifier,0,NULL);
        if(S!=XMQTT_SESSION_DONE){xmqttMessageRelease(M);xmqttClientFail(C,S==XMQTT_SESSION_LIMIT?XMQTT_STATUS_LIMIT:XMQTT_STATUS_SESSION,S==XMQTT_SESSION_LIMIT?NULL:xrtGetError());return false;}Prepared=true;}
#endif
    xrtMutexLock(&C->Gate);
    bool Full;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){Full=!xmqttInboxPut(C,C->Inbox,M);if(!Full)++C->Info.Received;}else
#endif
    {Full=C->Info.Messages==C->Config.MaxMessages || M->Box.Bytes>C->Config.MaxMessageBytes-C->Info.MessageBytes;
    if(!Full){if(C->MessageTail)C->MessageTail->Next=M;else C->MessageHead=M;C->MessageTail=M;++C->Info.Messages;C->Info.MessageBytes+=M->Box.Bytes;++C->Info.Received;}}
    xrtMutexUnlock(&C->Gate);
    if(Full){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(Prepared && xmqttInternalSessionReceive(C->Session->State,C,P->Identifier,2,NULL)!=XMQTT_SESSION_DONE)abort();
#endif
        xmqttMessageRelease(M);xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return false;}
    if(Qos==1){xmqttreceiveflow* F=C->ReceiveFlow;F->Identifiers[(F->Head+F->Count)%F->Capacity]=P->Identifier;++F->Count;}
    else if(Qos==2){C->ReceiveTwo->State[P->Identifier]|=XMQTT_RX_ACTIVE;++C->ReceiveTwo->Inflight;xmqttReceiveTwoQueue(C,P->Identifier,0x50,false);}
    xmqttClientNotify(C);return true;
}
static void xmqttClientReadPump(xmqttclient* C)
{
    size_t Read=0,Packets=0;
    while(!xmqttClientClosing(C) && Packets<C->Config.WorkPackets){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH) && defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        /* A post-CONNACK frame may already be buffered. Backpressure it until
         * local proof accepts opening/resetting the attached protocol Session.
         * Ordinary Auth keeps its existing bounded, hidden receive quarantine. */
        if(C->Session && !C->Info.Ready && C->Auth && C->Auth->ProtocolReady)return;
#endif
        size_t Needed=5;xmqttfixedheader H;xmqttwireconfig K={sizeof(K),C->Config.Version,XMQTT_PACKET_MAX_BYTES};
        xmqttwirestatus Header=xmqttHeaderRead((xbytesview){C->Input,C->InputBytes},&K,&H);
        if(Header==XMQTT_WIRE_ERROR){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,xrtGetError(),0x81);return;}
        if(Header==XMQTT_WIRE_DONE){
            if(H.PacketBytes>C->Peer.ReceivePacketBytes){xmqttClientFailReason(C,XMQTT_STATUS_PROTOCOL,NULL,0x95);return;}
            if(H.PacketBytes>C->Config.Codec.MaxPacketBytes){xmqttClientFail(C,XMQTT_STATUS_LIMIT,NULL);return;}
            Needed=H.PacketBytes;
            if(C->InputBytes>=Needed){xmqttpacket P;xmqttcodecissue Issue;xmqttcodecbuffers B={C->Props,C->Config.Codec.MaxProperties,NULL,0};
                xmqttcodecstatus S=C->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue):xmqttPacketRead311((xbytesview){C->Input,Needed},XMQTT_SERVER_TO_CLIENT,&C->Config.Codec,&B,&P,&Issue);
                if(S!=XMQTT_CODEC_DONE){xmqttClientFailReason(C,S==XMQTT_CODEC_LOCAL_ERROR?XMQTT_STATUS_LIMIT:XMQTT_STATUS_PROTOCOL,xrtGetError(),S==XMQTT_CODEC_LOCAL_ERROR?0:Issue.Error==XMQTT_CODEC_TOPIC_ALIAS?0x94:Issue.Error==XMQTT_CODEC_TOPIC_ALIAS_REPEAT?0x82:0x81);return;}
                if(!xmqttReceiveRoom(C,&P))return;
                if(!xmqttClientFrame(C,&P))return;
                C->InputBytes-=Needed;if(C->InputBytes)memmove(C->Input,C->Input+Needed,C->InputBytes);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
                if(C->Auth && (P.Type==XMQTT_PACKET_AUTH || P.Type==XMQTT_PACKET_CONNACK))xrtSecureZero(C->Input+C->InputBytes,Needed);
#endif
                ++Packets;continue;
            }
        }
        if(Needed>C->InputCapacity && !xmqttClientGrow(C,Needed))return;
        if(Read==C->Config.ReadBytes || !xmqttTransportAvailable(C))break;
        size_t Count=C->InputCapacity-C->InputBytes;
        if(Count>C->Config.ReadBytes-Read)Count=C->Config.ReadBytes-Read;
        size_t N=xmqttTransportRead(C,C->Input+C->InputBytes,Count);if(!N)break;
        Read+=N;C->InputBytes+=N;
    }
    if(Packets==C->Config.WorkPackets || (Read==C->Config.ReadBytes && xmqttTransportAvailable(C)))xmqttClientWake(C);
}
static bool xmqttClientSendStartup(xmqttclient* C,size_t* Budget)
{
    xmqttstartup* W=C->Startup;
    while(W && C->SetupOffset<W->Bytes && *Budget){
        size_t N=W->Bytes-C->SetupOffset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        (void)xrtAtomic32FetchAdd(&W->Refs,1,XMEMORY_RELAXED);
        xnetresult S=xmqttTransportSendRef(C,W->Data+C->SetupOffset,&N,xmqttStartupRelease,W);
        if(S!=XNET_RESULT_OK){xmqttStartupDrop(W);if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        C->SetupOffset+=N;*Budget-=N;
        if(C->SetupOffset==W->Bytes){C->PingDeadline=xrtTimer();C->NextPing=C->Peer.KeepAlive?C->PingDeadline+C->Peer.KeepAlive:INFINITY;}
    }
    if(W && C->SetupOffset<W->Bytes && !*Budget)xmqttClientWake(C);
    return W && C->SetupOffset==W->Bytes;
}
static bool xmqttClientSendPing(xmqttclient* C,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<2){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    static const unsigned char Ping[]={0xc0,0};
    while(C->PingOffset<2 && *Budget){
        size_t N=2-C->PingOffset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ping+C->PingOffset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(!C->PingOutstanding){C->PingOutstanding=true;
            if(xmqttClientProtocolReady(C))C->PingDeadline=xmqttClientDeadline(xrtTimer(),C->Config.PingTimeout);}
        C->PingOffset+=(unsigned)N;*Budget-=N;
    }
    if(C->PingOffset!=2){xmqttClientWake(C);return false;}
    C->PingOffset=0;double SentAt=xrtTimer();C->NextPing=C->Peer.KeepAlive?SentAt+C->Peer.KeepAlive:INFINITY;
    if(!xmqttClientProtocolReady(C))C->PingDeadline=SentAt;
    xrtMutexLock(&C->Gate);++C->Info.Pings;xrtMutexUnlock(&C->Gate);return true;
}
static bool xmqttClientSendAcks(xmqttclient* C,size_t* Budget,bool One)
{
    xmqttreceiveflow* F=C->ReceiveFlow;
    while(F && F->Count && *Budget){uint16 Id=F->Identifiers[F->Head];
        const unsigned char Ack[]={0x40,2,(unsigned char)(Id>>8),(unsigned char)Id};
        if(C->Peer.SendPacketBytes<sizeof(Ack)){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
        size_t N=sizeof(Ack)-F->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ack+F->Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        F->Offset+=(unsigned)N;*Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        if(F->Offset==sizeof(Ack)){F->Offset=0;F->Head=(F->Head+1)%F->Capacity;--F->Count;
            if(C->ReceivePaused){C->ReceivePaused=false;xmqttClientWake(C);}if(One)return true;}
    }
    if(F && F->Count){xmqttClientWake(C);return false;}return true;
}
static bool xmqttClientSendTwoAcks(xmqttclient* C,size_t* Budget,bool PartialOnly)
{
    xmqttreceivetwo* F=C->ReceiveTwo;size_t Packets=0;
    while(F && F->Count && *Budget && Packets<C->Config.WorkPackets){xmqttreceiveack A=F->Acks[F->Head];
        unsigned char Ack[]={A.Type,2,(unsigned char)(A.Identifier>>8),(unsigned char)A.Identifier,0x92};
        size_t Bytes=A.Unknown && C->Config.Version==XMQTT_VERSION_5?5:4;Ack[1]=(unsigned char)(Bytes-2);
        if(C->Peer.SendPacketBytes<Bytes){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
        size_t N=Bytes-F->Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetresult S=xmqttTransportSend(C,Ack+F->Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        F->Offset+=(unsigned)N;*Budget-=N;if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
        if(F->Offset==Bytes){unsigned char* State=F->State+A.Identifier;
            if(A.Type==0x50)*State&=(unsigned char)~XMQTT_RX_REC_QUEUED;
            else if(A.Unknown)*State&=(unsigned char)~XMQTT_RX_UNKNOWN_QUEUED;
            else {
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
                if(C->Session && xmqttInternalSessionReceive(C->Session->State,C,A.Identifier,4,NULL)!=XMQTT_SESSION_DONE)abort();
#endif
                if(!(*State&XMQTT_RX_OLD))--F->Inflight;
                *State&=(unsigned char)~(XMQTT_RX_ACTIVE|XMQTT_RX_RELEASED|XMQTT_RX_COMP_QUEUED|XMQTT_RX_OLD);}
            F->Offset=0;F->Head=(F->Head+1)%F->Capacity;--F->Count;++Packets;
            if(C->ReceivePaused){C->ReceivePaused=false;xmqttClientWake(C);}
            if(PartialOnly)return true;
        }
    }
    if(F && F->Count){xmqttClientWake(C);return false;}return true;
}
/* A partial frame always wins. At boundaries, alternate ready ACK classes so
 * a replenished QoS1 ring cannot starve QoS2 under small write budgets. */
static bool xmqttClientSendReceiveAcks(xmqttclient* C,size_t* Budget)
{
    xmqttreceivetwo* F=C->ReceiveTwo;xmqttreceiveflow* One=C->ReceiveFlow;size_t Packets=0;
    while((F->Count || (One && One->Count)) && *Budget && Packets<C->Config.WorkPackets){
        bool Two=F->Offset || (!(One && One->Offset) && F->Count && (!(One && One->Count) || F->TwoNext));
        if(!(Two?xmqttClientSendTwoAcks(C,Budget,true):xmqttClientSendAcks(C,Budget,true)))return false;
        F->TwoNext=!Two;++Packets;
    }
    if(F->Count || (One && One->Count)){xmqttClientWake(C);return false;}return true;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
static xmqttclientstatus xmqttSharedAliasPrepare(xmqttclient* C,xmqttrequest* R,bool Compact)
{
    const xmqttpacket* P=xmqttSessionPacket(xmqttSessionRequest(R)->Packet);xbytesview W=P->Wire;
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsharedalias* B=xmqttSharedAlias(R);
    size_t At=1;while(W.Data[At++]&128u){}At+=2+P->Body.Publish.Topic.Size+2;
    uint32 Properties=0;unsigned Shift=0;unsigned char Byte;
    do{Byte=W.Data[At++];Properties|=(uint32)(Byte&127u)<<Shift;Shift+=7;}while(Byte&128u);
    B->PropertiesAt=At;uint32 N=Properties+3u;size_t H=0;
    do{Byte=(unsigned char)(N&127u);N>>=7;B->Prefix[H++]=(unsigned char)(Byte|(N?128u:0u));}while(N);
    B->Prefix[H++]=0x23;B->Prefix[H++]=(unsigned char)(A->Alias>>8);B->Prefix[H++]=(unsigned char)A->Alias;B->PrefixBytes=(uint8)H;
    size_t Topic=Compact?0:P->Body.Publish.Topic.Size,Tail=W.Size-At,Body=2+Topic+2+H+Tail;
    if(Body>XMQTT_VARIABLE_MAX)return XMQTT_STATUS_RESTRICTED;
    A->Prefix[0]=W.Data[0];H=1;N=(uint32)Body;
    do{Byte=(unsigned char)(N&127u);N>>=7;A->Prefix[H++]=(unsigned char)(Byte|(N?128u:0u));}while(N);
    A->Prefix[H++]=(unsigned char)(Topic>>8);A->Prefix[H++]=(unsigned char)Topic;A->PrefixBytes=(uint8)H;A->FrameBytes=H+Topic+2+B->PrefixBytes+Tail;A->Mode=Compact?2:3;
    if(Compact){size_t Prefix=B->PrefixBytes;memmove(B->Prefix+H+2,B->Prefix,Prefix);memcpy(B->Prefix,A->Prefix,H);
        memcpy(B->Prefix+H,(const unsigned char*)P->Body.Publish.Topic.Data+P->Body.Publish.Topic.Size,2);B->PrefixBytes=(uint8)(H+2+Prefix);}
    if(A->FrameBytes>C->Peer.SendPacketBytes)return XMQTT_STATUS_RESTRICTED;
    return A->FrameBytes>C->Config.Codec.MaxPacketBytes?XMQTT_STATUS_LIMIT:XMQTT_STATUS_OK;
}
/* Four immutable spans: header/topic length, topic+id (or just id), updated
 * property length+alias, then the original canonical properties+payload.
 * User Property order is unchanged; no property value or payload is copied. */
static size_t xmqttSharedAliasRefs(xmqttrequest* R,size_t Bytes,xnetref Refs[4])
{
    const xmqttpacket* P=xmqttSessionPacket(xmqttSessionRequest(R)->Packet);xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsharedalias* B=xmqttSharedAlias(R);
    if(A->Mode==2){size_t At=R->Offset,Count=0;xbytesview Spans[]={{B->Prefix,B->PrefixBytes},{P->Wire.Data+B->PropertiesAt,P->Wire.Size-B->PropertiesAt}};
        for(unsigned I=0;I<2 && Bytes;++I){if(At>=Spans[I].Size){At-=Spans[I].Size;continue;}size_t N=Spans[I].Size-At;if(N>Bytes)N=Bytes;
            Refs[Count++]=(xnetref){Spans[I].Data+At,N,xmqttRequestNativeDrop,R};Bytes-=N;At=0;}return Count;}
    size_t Topic=A->Mode==3?P->Body.Publish.Topic.Size:0,Skip=P->Body.Publish.Topic.Size-Topic,At=R->Offset,Count=0;
    const unsigned char* Data[4]={A->Prefix,(const unsigned char*)P->Body.Publish.Topic.Data+Skip,B->Prefix,P->Wire.Data+B->PropertiesAt};
    size_t Sizes[4]={A->PrefixBytes,Topic+2,B->PrefixBytes,P->Wire.Size-B->PropertiesAt};
    for(unsigned I=0;I<4 && Bytes;++I){if(At>=Sizes[I]){At-=Sizes[I];continue;}size_t N=Sizes[I]-At;if(N>Bytes)N=Bytes;
        Refs[Count++]=(xnetref){Data[I]+At,N,xmqttRequestNativeDrop,R};Bytes-=N;At=0;}
    return Count;
}
#endif
static xmqttclientstatus xmqttSendAliasPrepare(xmqttclient* C,xmqttrequest* R)
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);xmqttsendtopic* Actual=A->Slot->Actual;
    if(A->Alias>C->Peer.SendAliasMaximum)return XMQTT_STATUS_RESTRICTED;
    bool Compact=Actual && (Actual==A->Topic || (Actual->Size==A->Topic->Size && !memcmp(Actual->Data,A->Topic->Data,Actual->Size)));
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(R->Capabilities&XMQTT_TX_SHARED)return xmqttSharedAliasPrepare(C,R,Compact);
#endif
    size_t Topic=Compact?0:A->Topic->Size,Tail=R->Bytes-A->TailOffset,Body=2+Topic+Tail;
    if(Body>XMQTT_VARIABLE_MAX)return XMQTT_STATUS_RESTRICTED;
    A->Prefix[0]=R->Wire[0];size_t H=1;uint32 N=(uint32)Body;
    do{unsigned char Byte=(unsigned char)(N&127u);N>>=7;A->Prefix[H++]=(unsigned char)(Byte|(N?128u:0u));}while(N);
    A->Prefix[H++]=(unsigned char)(Topic>>8);A->Prefix[H++]=(unsigned char)Topic;A->PrefixBytes=(uint8)H;A->FrameBytes=H+Topic+Tail;
    if(A->FrameBytes>C->Peer.SendPacketBytes)return XMQTT_STATUS_RESTRICTED;
    if(A->FrameBytes>C->Config.Codec.MaxPacketBytes)return XMQTT_STATUS_LIMIT;
    bool RawEmpty=!R->Wire[A->RawHeader] && !R->Wire[A->RawHeader+1];
    A->Mode=Compact?(RawEmpty?1:2):(RawEmpty?3:1);return XMQTT_STATUS_OK;
}
static size_t xmqttSendAliasRefs(xmqttrequest* R,size_t Bytes,xnetref Refs[3])
{
    xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);size_t At=R->Offset,Count=0;
    const unsigned char* Data[3]={A->Prefix,(const unsigned char*)A->Topic->Data,R->Wire+A->TailOffset};
    size_t Sizes[3]={A->PrefixBytes,A->Mode==3?A->Topic->Size:0,R->Bytes-A->TailOffset};
    for(unsigned I=0;I<3 && Bytes;++I){if(At>=Sizes[I]){At-=Sizes[I];continue;}size_t N=Sizes[I]-At;if(N>Bytes)N=Bytes;
        Refs[Count++]=(xnetref){Data[I]+At,N,xmqttRequestNativeDrop,R};Bytes-=N;At=0;}
    return Count;
}
static bool xmqttClientSendAliasRequest(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    size_t Bytes=xmqttRequestWireBytes(R);
    while(R->Offset<Bytes && *Budget){size_t N=Bytes-R->Offset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        xnetref Refs[4];size_t Count=1;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(R->Capabilities&XMQTT_TX_SHARED)Count=xmqttSharedAliasRefs(R,N,Refs);
        else
#endif
        if(xmqttSendAliasRequest(R)->Mode!=1)Count=xmqttSendAliasRefs(R,N,Refs);
        else Refs[0]=(xnetref){R->Wire+R->Offset,N,xmqttRequestNativeDrop,R};
        xnetresult S=xmqttRequestSend(C,R,Refs,Count,&N);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        /* A host queue may admit one lease but not the whole gather. Make
         * bounded progress with the first span, preserving frame ordering. */
        if(S==XNET_RESULT_AGAIN && Count>1 && (R->Capabilities&XMQTT_TX_SHARED)){N=Refs[0].Size;S=xmqttRequestSend(C,R,Refs,1,&N);}
#endif
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
        R->Offset+=N;*Budget-=N;C->Partial=R->Offset==Bytes?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->Offset!=Bytes){xmqttClientWake(C);return false;}
    {xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);if(A->Slot->Actual!=A->Topic){
        xrtMutexLock(&C->Gate);xmqttsendtopic* Old=A->Slot->Actual;++A->Topic->Refs;A->Slot->Actual=A->Topic;xmqttSendTopicDropLocked(C,Old);xrtMutexUnlock(&C->Gate);}}
    return true;
}
static bool xmqttClientSendRequest(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    if(R->Capabilities&XMQTT_TX_ALIAS){bool Done=xmqttClientSendAliasRequest(C,R,Budget);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(Done && (R->Capabilities&XMQTT_TX_SESSION))Done=xmqttClientSessionSent(C,R);
#endif
        return Done;}
    while(R->Offset<R->Bytes && *Budget){size_t N=R->Bytes-R->Offset,Available=xmqttTransportWritable(C);
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(N>*Budget)N=*Budget;
        if(N>Available)N=Available;
        if(!N)return false;
        xnetref Refs[2];size_t Count;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(R->Capabilities&XMQTT_TX_BORROW)Count=xmqttClientSessionRefs(C,R->Offset,N,R,Refs);
        else if(R->Capabilities&XMQTT_TX_SHARED){const xmqttpacket* P=xmqttSessionPacket(xmqttSessionRequest(R)->Packet);Refs[0]=(xnetref){P->Wire.Data+R->Offset,N,xmqttRequestNativeDrop,R};Count=1;}
        else
#endif
        {Refs[0]=(xnetref){R->Wire+R->Offset,N,xmqttRequestNativeDrop,R};Count=1;}
        xnetresult S=xmqttRequestSend(C,R,Refs,Count,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        if(R->Qos && !R->Credit){R->Credit=true;++C->SendInflight;}
        R->Offset+=N;*Budget-=N;C->Partial=R->Offset==R->Bytes?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->Offset!=R->Bytes){xmqttClientWake(C);return false;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if((R->Capabilities&XMQTT_TX_SESSION) && !xmqttClientSessionSent(C,R))return false;
    xmqttClientSessionDisconnect(C,R);
#endif
    return true;
}
/* The inline four-byte frame is immutable while every native lease owns R.
 * Identifier retirement therefore includes repeated PUBREL write tails. */
static bool xmqttClientSendPubrel(xmqttclient* C,xmqttrequest* R,size_t* Budget)
{
    if(C->Peer.SendPacketBytes<4){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return false;}
    while(R->State.Release.Offset<4 && *Budget){size_t N=4-R->State.Release.Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>*Budget)N=*Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return false;
        xnetref Ref={R->State.Release.Wire+R->State.Release.Offset,N,xmqttRequestNativeDrop,R};xnetresult S=xmqttRequestSend(C,R,&Ref,1,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());return false;}
        R->State.Release.Offset+=(uint8)N;*Budget-=N;C->Partial=R->State.Release.Offset==4?NULL:R;
        if(C->Peer.KeepAlive)C->NextPing=xrtTimer()+C->Peer.KeepAlive;
    }
    if(R->State.Release.Offset!=4){xmqttClientWake(C);return false;}
    R->State.Release.Pending=false;R->State.Release.Sent=true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if((R->Capabilities&XMQTT_TX_SESSION) && !xmqttClientSessionSent(C,R))return false;
#endif
    return true;
}
static void xmqttClientSendPump(xmqttclient* C)
{
    size_t Budget=C->Config.WriteBytes;
    if(!xmqttClientProtocolReady(C)){
        if(C->Startup && C->SetupOffset==C->Startup->Bytes){
            if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
            if(!xmqttAuthSend(C,&Budget))return;
#endif
            if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
        }
        return;
    }
    /* Finish the one partial packet before inserting any other packet, even
     * when a formerly blocked earlier request has just gained send credit. */
    if(C->Partial){xmqttrequest* R=C->Partial;
        bool Release=R->Qos==2 && R->Result.Write->Value.PubrecReceived;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(C->Session && C->Session->Selected==R)Release=C->Session->Frame.Type==XMQTT_PACKET_PUBREL;
#endif
        if(!(Release?xmqttClientSendPubrel(C,R,&Budget):xmqttClientSendRequest(C,R,&Budget)))return;
    }
    if(C->PingOffset && !xmqttClientSendPing(C,&Budget))return;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    /* An AUTH can be partial too. Complete an existing receive ACK before
     * starting AUTH; finish AUTH before starting any new receive ACK. */
    if(C->Auth){
        if(C->ReceiveTwo && C->ReceiveTwo->Offset && !xmqttClientSendTwoAcks(C,&Budget,true))return;
        if(C->ReceiveFlow && C->ReceiveFlow->Offset && !xmqttClientSendAcks(C,&Budget,true))return;
        if(!xmqttAuthSend(C,&Budget))return;
    }
#endif
    if(!(C->ReceiveTwo?xmqttClientSendReceiveAcks(C,&Budget):xmqttClientSendAcks(C,&Budget,false)))return;
    if(!C->Info.Ready){if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);return;}
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){
        xmqttrequest* R=*Link;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if((R->Capabilities&XMQTT_TX_SESSION) && R->Acknowledged){xmqttRequestFinish(C,Link,R->Result.Write->Value.Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);continue;}
#endif
        if(R->Qos==2 && R->Acknowledged){xmqttRequestFinish(C,Link,R->Result.Write->Value.Reason<0x80?XMQTT_STATUS_OK:XMQTT_STATUS_REJECTED);continue;}
        if(R->Qos==2 && R->State.Release.Pending){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
            if(R->Capabilities&XMQTT_TX_SESSION){bool Empty=false;xmqttclientstatus S=xmqttClientSessionSelect(C,R,&Empty);
                if(S!=XMQTT_STATUS_OK){xmqttClientFail(C,S,xrtGetError());return;}if(Empty){Link=&R->Next;continue;}}
#endif
            if(!xmqttClientSendPubrel(C,R,&Budget))return;
            if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;continue;}
        if(R->Offset==xmqttRequestWireBytes(R)){Link=&R->Next;continue;}
        if((!(R->Capabilities&XMQTT_TX_ALIAS) && R->Bytes>C->Peer.SendPacketBytes) || R->Qos>C->Peer.MaximumQos || (R->Retain && !C->Peer.RetainAvailable) ||
           ((R->Capabilities&1u) && !C->Peer.WildcardAvailable) || ((R->Capabilities&2u) && !C->Peer.SharedAvailable) ||
           ((R->Capabilities&4u) && !C->Peer.SubscriptionIdsAvailable)){
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
            if(R->Capabilities&XMQTT_TX_BORROW){xmqttClientFail(C,XMQTT_STATUS_RESTRICTED,NULL);return;}
#endif
            xmqttRequestFinish(C,Link,XMQTT_STATUS_RESTRICTED);continue;}
        /* The Session quota is authoritative for replay. This connection also
         * keeps the existing conservative in-flight window; an old PUBCOMP
         * cannot enlarge the current PUBLISH window beyond Receive Maximum. */
        if(R->Qos && !R->Credit && C->SendInflight>=C->Peer.SendReceiveMaximum){Link=&R->Next;continue;}
        if((R->Capabilities&XMQTT_TX_ALIAS) && !R->Offset){xmqttclientstatus S=xmqttSendAliasPrepare(C,R);
            if(S!=XMQTT_STATUS_OK){xmqttRequestFinish(C,Link,S);continue;}}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
        if(R->Capabilities&XMQTT_TX_BORROW){size_t Size=0;const xmqttpacket* P=xmqttSessionPacket(xmqttSessionRequest(R)->Packet);
            bool Valid=C->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Size):xmqttPacketSize311(P,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,&Size);
            if(!Valid){xmqttClientFail(C,XMQTT_STATUS_LIMIT,xrtGetError());return;}}
        if(R->Capabilities&XMQTT_TX_SESSION){bool Empty=false;xmqttclientstatus S=xmqttClientSessionSelect(C,R,&Empty);
            if(S!=XMQTT_STATUS_OK){if(R->Capabilities&XMQTT_TX_BORROW){xmqttClientFail(C,S,xrtGetError());return;}xmqttRequestFinish(C,Link,S);continue;}
            if(Empty){Link=&R->Next;continue;}}
#endif
        if(!xmqttClientSendRequest(C,R,&Budget))return;
        if(!Budget){xmqttClientWake(C);return;}Link=&R->Next;
    }
    if(xrtTimer()>=C->NextPing)(void)xmqttClientSendPing(C,&Budget);
}
static void xmqttClientCollect(xmqttclient* C)
{
    xrtMutexLock(&C->Gate);xmqttrequest* Head=C->SubmitHead;C->SubmitHead=C->SubmitTail=NULL;xrtMutexUnlock(&C->Gate);
    if(Head){if(C->RequestTail)C->RequestTail->Next=Head;else C->Requests=Head;
        while(Head){Head->Previous=C->RequestTail;C->RequestTail=Head;xmqttRequestMapInsert(C,Head);Head=Head->Next;}}
}
static void xmqttClientDrainStop(xmqttclient* C,bool Abort)
{
    C->Draining=false;
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;C->TimerAt=INFINITY;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    if(Abort)(void)xmqttTransportClose(C,true);
}
static void xmqttClientDrainError(xmqttclient* C)
{
    xrtMutexLock(&C->Gate);bool Abort=C->AbortRequested;size_t Offset=C->Info.DisconnectBytes;uint8 Reason=C->Info.LocalReason;xrtMutexUnlock(&C->Gate);
    if(C->NativeClosed){xmqttClientDrainStop(C,false);return;}
    if(Abort || C->ReadEnded || xrtTimer()>=C->DrainDeadline){
        if(!Abort && !C->ReadEnded){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);}
        xmqttClientDrainStop(C,true);return;
    }
    if(C->DrainClosing)return;
    const unsigned char Wire[]={0xe0,1,Reason};size_t Budget=C->Config.WriteBytes;
    while(Offset<sizeof(Wire) && Budget){size_t N=sizeof(Wire)-Offset,Available=xmqttTransportWritable(C);
        if(N>Available)N=Available;
        if(N>Budget)N=Budget;
        if(N>C->Config.WriteChunkBytes)N=C->Config.WriteChunkBytes;
        if(!N)return;
        xnetresult S=xmqttTransportSend(C,Wire+Offset,&N);
        if(S!=XNET_RESULT_OK){if(S!=XNET_RESULT_AGAIN)xmqttClientDrainStop(C,true);return;}
        Offset+=N;Budget-=N;xrtMutexLock(&C->Gate);C->Info.DisconnectBytes=Offset;xrtMutexUnlock(&C->Gate);
    }
    if(Offset==sizeof(Wire)){C->DrainClosing=true;if(!xmqttTransportClose(C,false))xmqttClientDrainStop(C,true);}
    else xmqttClientWake(C);
}
/* The accepted application request owns its wire and producer lease. Drain
 * prior protocol obligations before making DISCONNECT the final frame. */
static void xmqttClientStopApplication(xmqttclient* C)
{
    xmqttrequest* R=C->Disconnect;if(!R)return;
    if(!C->Stopped){C->Stopped=true;xmqttClientNotify(C);}
    if(xrtCancelRequested(R->Cancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    if(xrtTimer()>=R->Deadline){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);}
    xrtMutexLock(&C->Gate);xmqttclientstatus Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    if(Status!=XMQTT_STATUS_OK)goto Failed;
    xmqttClientSchedule(C,R->Deadline);
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    if(Status!=XMQTT_STATUS_OK)goto Failed;
    if(!C->ApplicationNativeClosing){
        xrtMutexLock(&C->Gate);C->Info.DrainStarted=true;xrtMutexUnlock(&C->Gate);
        size_t Budget=C->Config.WriteBytes;bool Sent=xmqttClientSendRequest(C,R,&Budget);
        xrtMutexLock(&C->Gate);C->Info.DisconnectBytes=R->Offset;Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
        if(Status!=XMQTT_STATUS_OK)goto Failed;
        if(!Sent)return;
        C->ApplicationNativeClosing=true;
        if(!xmqttTransportClose(C,false)){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,xrtGetError());goto Failed;}
    }
    if(!C->NativeClosed || xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE))return;
    Status=XMQTT_STATUS_OK;
    goto Finish;
Failed:
    /* Disconnect/Close may publish after this worker's initial collection but
     * before its stop decision. All earlier accepted submissions must settle
     * before the disconnect request and the application-close state retire. */
    xmqttClientCollect(C);
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;xrtMutexUnlock(&C->Gate);
    (void)xmqttTransportClose(C,true);
    {size_t Count=0;while(C->Requests && Count++<C->Config.WorkPackets)xmqttRequestFinish(C,&C->Requests,Status);}
    if(C->Requests){xmqttClientWake(C);return;}
Finish:
    if(C->Timer){uint64 Id=C->Timer;C->Timer=0;C->TimerAt=INFINITY;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
    xrtMutexLock(&C->Gate);Status=C->Info.CloseStatus;C->Info.State=XMQTT_CLIENT_CLOSED;xrtMutexUnlock(&C->Gate);
    xmqttRequestFinish(C,&C->Disconnect,Status);xmqttClientNotify(C);
}
static void xmqttClientStop(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    xmqttClientSessionUnreserve(C);
#endif
    if(C->ApplicationClose){xmqttClientStopApplication(C);return;}
    if(!C->Stopped){C->Stopped=true;
        bool AuthPartial=false;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        AuthPartial=xmqttAuthStop(C);
#endif
        if(C->Timer){uint64 Id=C->Timer;C->Timer=0;(void)xrtNetEngineTimerCancelCurrent(C->Engine,Id);}
        xmqttReadyFinish(C,C->Info.CloseStatus);xmqttStartupRetire(C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
        if(C->TLS && C->TLS->Plan.Session){xtlssession* S=C->TLS->Plan.Session;C->TLS->Plan.Session=NULL;xrtTlsSessionDestroy(S);}
#endif
        xrtMutexLock(&C->Gate);bool Diagnostic=C->Info.LocalReason && !C->AbortRequested;xrtMutexUnlock(&C->Gate);
        /* Never append a diagnostic inside a partial MQTT packet. Already
         * fully accepted output frames may drain ahead of the final packet. */
        C->Draining=Diagnostic && !AuthPartial && !C->Partial && !C->PingOffset && !(C->ReceiveFlow && C->ReceiveFlow->Offset) && !(C->ReceiveTwo && C->ReceiveTwo->Offset) &&
            !C->ReadEnded && !C->NativeClosed &&
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
            C->Stream &&
#endif
            C->Peer.SendPacketBytes>=3 && xmqttTransportOpen(C);
        if(C->Draining){C->DrainDeadline=xmqttClientDeadline(xrtTimer(),C->Config.CloseTimeout);xmqttClientSchedule(C,C->DrainDeadline);
            if(!C->Timer)C->Draining=false;else {xrtMutexLock(&C->Gate);C->Info.DrainStarted=true;xrtMutexUnlock(&C->Gate);}}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
        if(C->Dial && !C->Stream)(void)xrtNetDialCancel(C->Dial);
#endif
        if(!C->Draining
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
            && C->Stream
#endif
        )(void)xmqttTransportClose(C,true);
        xmqttClientNotify(C); /* CLOSING is observable; reentrant Close may abort. */
    }
    if(C->Draining)xmqttClientDrainError(C);
    size_t Count=0;
    while(C->Requests && Count++<C->Config.WorkPackets)xmqttRequestFinish(C,&C->Requests,C->Info.CloseStatus);
    if(C->Requests){xmqttClientWake(C);return;}
    if(C->Draining)return;
    xrtMutexLock(&C->Gate);bool Changed=C->Info.State!=XMQTT_CLIENT_CLOSED;C->Info.State=XMQTT_CLIENT_CLOSED;xrtMutexUnlock(&C->Gate);
    if(Changed)xmqttClientNotify(C);
}
static void xmqttClientWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xmqttclient* C=Data;
    xrtMutexLock(&C->Gate);C->Posted=false;bool Started=C->Started;xmqttrequest* Disconnect=C->Disconnect;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    bool ResumeNotify=Started && C->TLS && C->TLS->Resume.Notify;if(ResumeNotify)C->TLS->Resume.Notify=false;
#endif
    xrtMutexUnlock(&C->Gate);
    if(!Started){xmqttClientDrop(C);return;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    xmqttClientSessionEnd(C);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){C->TLS->Bytes=C->Config.WriteBytes;C->TLS->Calls=C->Config.WorkPackets;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    if(ResumeNotify)xmqttClientNotify(C);
#endif
    xmqttClientCollect(C);
    if(Disconnect){
        if(xrtCancelRequested(Disconnect->Cancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
        if(xrtTimer()>=Disconnect->Deadline){xrtMutexLock(&C->Gate);C->Info.DrainTimedOut=true;xrtMutexUnlock(&C->Gate);xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);}
        if(!C->Stopped){C->Stopped=true;xmqttClientNotify(C);}
    }
    if(C->ReadyPromise && xrtCancelRequested(C->ReadyCancel))xmqttClientFail(C,XMQTT_STATUS_CANCELLED,NULL);
    double Now=xrtTimer();
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqttTlsProgress(C);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    xmqttAuthWork(C);
#endif
    if(!C->Info.Ready && Now>=C->SetupDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    if(!C->ApplicationFinal && C->PingOutstanding && xmqttClientProtocolReady(C) && Now>=C->PingDeadline)xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
    for(xmqttrequest** Link=&C->Requests;*Link && !xmqttClientClosing(C);){xmqttrequest* R=*Link;
        xmqttclientstatus S=xrtCancelRequested(R->Cancel)?XMQTT_STATUS_CANCELLED:Now>=R->Deadline?XMQTT_STATUS_TIMEOUT:XMQTT_STATUS_OK;
        if(S!=XMQTT_STATUS_OK){bool Partial=R->Offset!=0;xmqttRequestFinish(C,Link,S);if(Partial)xmqttClientFail(C,S,NULL);continue;}
        if(R->Type==XMQTT_PACKET_PUBLISH && !R->Qos && R->Offset==xmqttRequestWireBytes(R) && !xrtAtomic32Load(&R->NativeRefs,XMEMORY_ACQUIRE)){
            if(!xmqttTransportOpen(C) || C->ReadEnded){xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);break;}
            xmqttRequestFinish(C,Link,XMQTT_STATUS_OK);continue;
        }
        Link=&R->Next;
    }
    if(!xmqttClientClosing(C)
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
       && C->Stream
#endif
       && xmqttTransportOpen(C)){
        /* Startup must be queued before an immediate/coalesced CONNACK. */
        if(!C->Info.Ready){size_t Budget=C->Config.WriteBytes;(void)xmqttClientSendStartup(C,&Budget);}
        if(!xmqttClientClosing(C))xmqttClientReadPump(C);
        if(!xmqttClientClosing(C) && !C->ReadEnded)xmqttClientSendPump(C);
        if(xmqttClientProtocolReady(C) && !C->PingOutstanding && xrtTimer()>=C->NextPing){
            if((C->ReceiveFlow && C->ReceiveFlow->Offset) || (C->ReceiveTwo && C->ReceiveTwo->Offset))xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
            if(xmqttAuthPartial(C))xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);
#endif
            for(xmqttrequest* R=C->Requests;R;R=R->Next)if((R->Offset && R->Offset<xmqttRequestWireBytes(R)) || (R->Qos==2 && R->State.Release.Offset && R->State.Release.Offset<4)){xmqttClientFail(C,XMQTT_STATUS_TIMEOUT,NULL);break;}}
    }
    if((C->ReadEnded || C->NativeClosed) && !(Disconnect && C->ApplicationNativeClosing))xmqttClientFail(C,XMQTT_STATUS_TRANSPORT,NULL);
    bool SessionReceiveIdle=true;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Disconnect && C->Session){xmqttsessioninfo I;xmqttSessionInfo(C->Session->State,&I);SessionReceiveIdle=!I.Incoming && !I.Prepared;}
#endif
    if(Disconnect && SessionReceiveIdle && !xmqttClientClosing(C) && !C->Requests && !C->Partial && !C->PingOffset && !(C->ReceiveFlow && C->ReceiveFlow->Count) && !(C->ReceiveTwo && (C->ReceiveTwo->Count || C->ReceiveTwo->Inflight))){
        xrtMutexLock(&C->Gate);if(!C->SubmitHead)C->ApplicationFinal=true;xrtMutexUnlock(&C->Gate);
    }
    if(xmqttClientClosing(C))xmqttClientStop(C);
    else {double At=C->Info.Ready?C->NextPing:C->SetupDeadline;
        if(!C->Info.Ready && C->NextPing<At)At=C->NextPing;
        if(xmqttClientProtocolReady(C) && C->PingOutstanding && C->PingDeadline<At)At=C->PingDeadline;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
        {double AuthAt=xmqttAuthDeadline(C);if(AuthAt<At)At=AuthAt;}
#endif
        for(xmqttrequest* R=C->Requests;R;R=R->Next)if(R->Deadline<At)At=R->Deadline;
        if(Disconnect && Disconnect->Deadline<At)At=Disconnect->Deadline;
        xmqttClientSchedule(C,At);
    }
    xmqttClientDrop(C);
}

XMQTT_API bool xmqttClientConfigInit(xmqttclientconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid client config output");
    *K=(xmqttclientconfig){0};K->Size=sizeof(*K);K->Version=XMQTT_VERSION_5;xrtNetStreamConfigInit(&K->Stream);xmqttCodecConfigInit(&K->Codec);
    K->MaxRequests=128;K->MaxOutgoingBytes=4u*1024u*1024u;K->MaxObjects=512;K->MaxObjectBytes=16u*1024u*1024u;K->MaxMessages=64;K->MaxMessageBytes=8u*1024u*1024u;
    K->MaxPrivateBytes=4u*1024u*1024u;K->MaxSetupBytes=64u*1024u;K->InitialInputBytes=1024;K->ReadBytes=K->WriteBytes=64u*1024u;K->WriteChunkBytes=16u*1024u;K->WorkPackets=64;K->SetupTimeout=K->PingTimeout=K->CloseTimeout=5000;return true;
}
static void xmqttClientFree(xmqttclient* C)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(C->Session){xmqttClientSessionUnreserve(C);xmqttClientSessionEnd(C);
        if(C->Session->State)xmqttInternalSessionUnclaim(C->Session->State,C);
        if(C->Session->Selected)abort();
        xrtMutexUnit(&C->Session->AliasGate);xrtFree(C->Session);}
#endif
    xmqttmessage* M=C->MessageHead;while(M){xmqttmessage* Next=M->Next;xmqttMessageRelease(M);M=Next;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xrtNetDialDestroy(C->Dial);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS){if(C->TLS->Stream || C->TLS->Head)abort();xrtTlsSessionDestroy(C->TLS->Plan.Session);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
        xrtTlsResumeRelease(C->TLS->Resume.Value);
#endif
        xrtFree(C->TLS);}
#endif
    xrtNetStreamDestroy(C->Stream);xrtFutureDestroy(C->ReadyFuture);xrtCancelDestroy(C->ReadyCancel);xrtErrorFree(C->Cause);
    if(C->SendAliases){for(size_t Page=0;Page<256;++Page){xmqttsendaliasslot* Slots=C->SendAliases->Pages[Page];if(!Slots)continue;
        for(size_t I=0;I<256;++I){xmqttSendTopicDropLocked(C,Slots[I].Planned);xmqttSendTopicDropLocked(C,Slots[I].Actual);}xrtFree(Slots);}xrtFree(C->SendAliases);}
    if(C->ReceiveAliases){for(size_t I=0;I<C->Peer.ReceiveAliasMaximum;++I)xrtFree(C->ReceiveAliases[I]);xrtFree(C->ReceiveAliases);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth){xrtSecureZero(C->Input,C->InputCapacity);xrtSecureZero(C->Auth->MethodBytes,C->Auth->Method.Size);xrtFree(C->Auth);}
#endif
    xrtFree(C->Input);xrtFree(C->ReceiveFlow);xrtFree(C->ReceiveTwo);xrtFree(xrtAtomicPtrLoad(&C->SendFlow,XMEMORY_RELAXED));xmqttQuotaDrop(C->Quota);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){xmqttclientinbox* I=C->Inbox;xrtMutexLock(&I->Gate);if(I->Client!=C)abort();I->Client=NULL;xrtMutexUnlock(&I->Gate);}
#endif
    (void)xrtNetEngineUnpin(C->Engine);xrtCondUnit(&C->RetirementReady);xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);
}
static bool xmqttClientConnectImpl(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out,const void* AuthConfig,double Now,const void* DialPlan,const void* TlsPlan,void* InboxPlan,void* SessionPlan)
{
    bool Planned=false;size_t HostBytes=0,TlsBytes=0,SessionBytes=0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(SessionPlan)SessionBytes=sizeof(xmqttsessiondriver);
#else
    (void)SessionPlan;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    const xmqtttlsplan* TLS=TlsPlan;if(TLS)TlsBytes=sizeof(xmqtttlsstate);
#else
    (void)TlsPlan;
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    const xmqttdialplan* D=DialPlan;char* HostCopy=NULL;Planned=D!=NULL;
    if(D && !D->Addresses)HostBytes=D->Host.Size+1;
#else
    (void)DialPlan;
#endif
    xmqttclientconfig K;if(Config){if(!xrtMemRangeValid(Config,sizeof(*Config)))return xmqttClientError(XERR_ARGUMENT,"invalid client config");K=*Config;}else xmqttClientConfigInit(&K);
    if(!Engine || (!Remote && !Planned) || !Out || !Connect || !xrtMemRangeValid(Out,sizeof(*Out)) || !xrtMemRangeValid(Connect,sizeof(*Connect)) ||
       K.Size!=sizeof(K) || (K.Version!=XMQTT_VERSION_5 && K.Version!=XMQTT_VERSION_311) || Connect->Type!=XMQTT_PACKET_CONNECT ||
       !K.MaxRequests || K.MaxRequests>65536 || !K.MaxOutgoingBytes || K.MaxObjects<2 || K.MaxObjects>UINT32_MAX/8u || !K.MaxObjectBytes ||
       !K.MaxMessages || !K.MaxMessageBytes || !K.MaxSetupBytes || K.InitialInputBytes<5 || K.InitialInputBytes>K.Codec.MaxPacketBytes ||
       !K.ReadBytes || !K.WriteBytes || !K.WriteChunkBytes || !K.WorkPackets || K.SetupTimeout<0 || K.PingTimeout<=0 || K.CloseTimeout<=0)
        return xmqttClientError(XERR_ARGUMENT,"invalid MQTT client configuration");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    xmqttclientinbox* Inbox=InboxPlan;
    if(Inbox && (K.Version!=Inbox->Config.Version || K.MaxObjects!=Inbox->Config.MaxObjects || K.MaxObjectBytes!=Inbox->Config.MaxObjectBytes ||
       K.MaxMessages!=Inbox->Config.MaxMessages || K.MaxMessageBytes!=Inbox->Config.MaxMessageBytes))return xmqttClientError(XERR_ARGUMENT,"Client and Inbox limits differ");
#else
    (void)InboxPlan;
#endif
    if(!xrtNetStreamConfigValid(&K.Stream))return false;
    size_t WireBytes=0;bool Valid=K.Version==XMQTT_VERSION_5?xmqttPacketSize5(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes):xmqttPacketSize311(Connect,XMQTT_CLIENT_TO_SERVER,&K.Codec,&WireBytes);
    if(!Valid)return false;
    uint32 ReceiveBytes=XMQTT_PACKET_MAX_BYTES;bool ProblemInfo=true,HasMethod=false;xstrview Method={0};
    for(size_t I=0;I<Connect->Properties.Count;++I){const xmqttproperty* P=Connect->Properties.Data+I;
        if(P->Id==XMQTT_PROPERTY_AUTH_METHOD){HasMethod=true;Method=P->Value.Text;if(!AuthConfig)return xmqttClientError(XERR_STATE,"enhanced authentication requires xmqttClientConnectAuth");}
        if(P->Id==XMQTT_PROPERTY_REQUEST_PROBLEM_INFO)ProblemInfo=P->Value.Number!=0;
        if(P->Id==XMQTT_PROPERTY_MAXIMUM_PACKET_SIZE){ReceiveBytes=P->Value.Number;if(ReceiveBytes>K.Codec.MaxPacketBytes)return xmqttClientError(XERR_ARGUMENT,"advertised receive packet size exceeds local codec limit");}
    }
    size_t AuthBytes=0;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(AuthConfig){if(K.Version!=XMQTT_VERSION_5 || !HasMethod)return xmqttClientError(XERR_ARGUMENT,"enhanced authentication requires MQTT5 CONNECT method");AuthBytes=sizeof(xmqttauthstate)+Method.Size;}
#else
    (void)HasMethod;(void)Method;
#endif
    size_t SetupCount=Connect->Properties.Count+Connect->Body.Connect.WillProperties.Count;
    if(SetupCount>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty) || K.Codec.MaxProperties>(SIZE_MAX-sizeof(xmqttclient))/sizeof(xmqttproperty)-SetupCount || WireBytes>K.MaxSetupBytes || WireBytes>SIZE_MAX-sizeof(xmqttstartup))return xmqttClientError(XERR_RANGE,"MQTT client setup capacity");
    size_t ClientBytes=sizeof(xmqttclient)+(SetupCount+K.Codec.MaxProperties)*sizeof(xmqttproperty),StartupBytes=sizeof(xmqttstartup)+WireBytes;
    if(ClientBytes>K.MaxPrivateBytes || K.InitialInputBytes>K.MaxPrivateBytes-ClientBytes || StartupBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes || AuthBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes || HostBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes-AuthBytes || TlsBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes-AuthBytes-HostBytes || SessionBytes>K.MaxPrivateBytes-ClientBytes-K.InitialInputBytes-StartupBytes-AuthBytes-HostBytes-TlsBytes)return xmqttClientError(XERR_RANGE,"MQTT private setup budget");
    if(!xrtNetEnginePin(Engine))return false;
    xmqttclient* C=xrtMalloc(ClientBytes);if(!C){xrtNetEngineUnpin(Engine);return false;}memset(C,0,ClientBytes);
    if(!xrtMutexInit(&C->Gate)){xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtMutexInit(&C->RetirementLock)){xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    if(!xrtCondInit(&C->RetirementReady)){xrtMutexUnit(&C->RetirementLock);xrtMutexUnit(&C->Gate);xrtFree(C);xrtNetEngineUnpin(Engine);return false;}
    C->Engine=Engine;C->Config=K;C->ProblemInfo=ProblemInfo;C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));xrtAtomic32Init(&C->Refs,1);
    xrtAtomicPtrInit(&C->SendFlow,NULL);
    C->Info.Size=sizeof(C->Info);C->Info.State=XMQTT_CLIENT_CONNECTING;C->Info.PrivateBytes=ClientBytes+K.InitialInputBytes+StartupBytes+AuthBytes+HostBytes+TlsBytes+SessionBytes;C->Info.StartupBytes=WireBytes;
    C->SetupProps=C->Scratch;C->Props=C->Scratch+SetupCount;C->Peer.ReceivePacketBytes=ReceiveBytes;
    C->Peer.SendPacketBytes=XMQTT_PACKET_MAX_BYTES;C->Peer.KeepAlive=Connect->Body.Connect.KeepAlive;
    C->SetupDeadline=xmqttClientDeadline(Now,K.SetupTimeout);C->NextPing=C->PingDeadline=C->TimerAt=INFINITY;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(SessionPlan){xmqttsessiondriver* Driver=xrtCalloc(1,sizeof(*Driver));if(!Driver)goto Fail;
        if(!xrtMutexInit(&Driver->AliasGate)){xrtFree(Driver);goto Fail;}C->Session=Driver;
        size_t Count=0,Bytes=0;
        if(!xmqttInternalSessionClaim(SessionPlan,C,K.Version,Connect,&C->Config.Codec,Now,&Count,&Bytes))goto Fail;
        C->Session->State=SessionPlan;
        if(Count>K.MaxRequests || Bytes>K.MaxOutgoingBytes){xmqttClientError(XERR_RANGE,"session replay exceeds Client admission limits");goto Fail;}
        C->Session->ReservedCount=C->Info.Requests=Count;C->Session->ReservedBytes=C->Info.OutgoingBytes=Bytes;
    }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(TLS){C->TLS=xrtCalloc(1,sizeof(*C->TLS));if(!C->TLS)goto Fail;C->TLS->Plan=*TLS;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
        C->TLS->Resume.Info.Size=sizeof(C->TLS->Resume.Info);C->TLS->Resume.Info.Enabled=TLS->Capture;
#endif
    }
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(AuthConfig){C->Auth=xrtCalloc(1,AuthBytes);if(!C->Auth)goto Fail;C->Auth->Config=*(const xmqttauthconfig*)AuthConfig;
        C->Auth->Method=(xstrview){C->Auth->MethodBytes,Method.Size};if(Method.Size)memcpy(C->Auth->MethodBytes,Method.Data,Method.Size);
        C->Auth->Active=C->Auth->AwaitServer=true;C->Auth->Deadline=C->SetupDeadline;C->Auth->StepDeadline=INFINITY;}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(Inbox){xrtMutexLock(&Inbox->Gate);
        if(Inbox->Client){xrtMutexUnlock(&Inbox->Gate);xmqttClientError(XERR_STATE,"Inbox already attached");goto Fail;}
        Inbox->Client=C;C->Inbox=Inbox;C->Quota=Inbox->Quota;(void)xrtAtomic32FetchAdd(&C->Quota->Refs,1,XMEMORY_RELAXED);xrtMutexUnlock(&Inbox->Gate);
    }else
#endif
    {C->Quota=xmqttQuotaCreate(K.MaxObjects,K.MaxObjectBytes);if(!C->Quota)goto Fail;}
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
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(HostBytes){HostCopy=xrtMalloc(HostBytes);if(!HostCopy)goto Fail;memcpy(HostCopy,D->Host.Data,D->Host.Size);HostCopy[D->Host.Size]=0;}
#endif
    xrtMutexLock(&C->Gate);xmqttClientRef(C);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(D){xnetdialconfig Policy=D->Config;Policy.Stream=K.Stream;Policy.Affinity=Affinity;
        C->Dial=D->Addresses?xrtNetDialAddressesOwnedV1(Engine,D->Addresses,&Policy,NULL,NULL,C,&xmqttClientDialEvents):
            xrtNetDialOwnedV1(Engine,D->Resolver,HostCopy,D->Port,&Policy,NULL,NULL,C,&xmqttClientDialEvents);
        if(!C->Dial){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}
        xrtFree(HostCopy);HostCopy=NULL;C->Info.PrivateBytes-=HostBytes;
    }else
#endif
    {C->Stream=xrtNetStreamConnectOwnedV1(Engine,Remote,Affinity,&K.Stream,C,&xmqttClientEvents);
        if(!C->Stream){xmqttClientDrop(C);xrtMutexUnlock(&C->Gate);goto Fail;}}
    C->Started=true;(void)xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);*Out=C;return true;
Fail:
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    xrtFree(HostCopy);
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    if(C->TLS)C->TLS->Plan.Session=NULL; /* failure transfers no plan ownership */
#endif
    xrtPromiseDestroy(C->ReadyPromise);C->ReadyPromise=NULL;if(C->ReadyBox)xmqttReadyFree(C->ReadyBox,NULL);xmqttStartupRetire(C);xmqttClientFree(C);return false;
}
XMQTT_API bool xmqttClientConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttpacket* Connect,xmqttclient** Out)
{double Now=xrtTimer();return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,NULL,Now,NULL,NULL,NULL,NULL);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
XMQTT_API bool xmqttClientConnectInbox(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,xmqttclientinbox* Inbox,const xmqttpacket* Connect,xmqttclient** Out)
{double Now=xrtTimer();if(!Inbox)return xmqttClientError(XERR_ARGUMENT,"Inbox is NULL");return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,NULL,Now,NULL,NULL,Inbox,NULL);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_API bool xmqttClientConnectSession(xnetengine* E,const xnetaddr* A,uint64 Affinity,const xmqttclientconfig* K,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** Out)
{double Now=xrtTimer();if(!S || !I)return xmqttClientError(XERR_ARGUMENT,"Session and Inbox are required");return xmqttClientConnectImpl(E,A,Affinity,K,Q,Out,NULL,Now,NULL,NULL,I,S);}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
static bool xmqttAuthConfigCopy(const xmqttauthconfig* Auth,xmqttauthconfig* K)
{
    if(Auth){if(!xrtMemRangeValid(Auth,sizeof(*Auth)))return xmqttClientError(XERR_ARGUMENT,"invalid authentication config");*K=*Auth;}else xmqttAuthConfigInit(K);
    if(K->Size!=sizeof(*K) || !K->MaxRounds || K->MaxRounds>65536 || !K->MaxExchangeBytes || K->ResponseTimeout<=0)return xmqttClientError(XERR_ARGUMENT,"invalid authentication limits");
    return true;
}
XMQTT_API bool xmqttClientConnectAuth(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,const xmqttclientconfig* Config,const xmqttauthconfig* Auth,const xmqttpacket* Connect,xmqttclient** Out)
{
    double Now=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientConnectImpl(Engine,Remote,Affinity,Config,Connect,Out,&K,Now,NULL,NULL,NULL,NULL);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_API bool xmqttClientConnectSessionAuth(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttauthconfig* Auth,
    xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!S || !I)return xmqttClientError(XERR_ARGUMENT,"Session and Inbox are required");
    if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientConnectImpl(E,R,A,C,Q,O,&K,At,NULL,NULL,I,S);
}
#endif
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
XMQTT_API bool xmqttClientDialConfigInit(xmqttclientdialconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid Dial config output");
    *K=(xmqttclientdialconfig){sizeof(*K),XNET_FAMILY_UNSPEC,30000,250,8,1024};return true;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
XMQTT_API bool xmqttClientTlsConfigInit(xmqttclienttlsconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS config output");
    memset(K,0,sizeof(*K));K->Size=sizeof(*K);xrtTlsClientConfigInit(&K->Session);xrtTlsStreamConfigInit(&K->Stream);
    xmqttClientDialConfigInit(&K->Dial);K->MaxMetadataBytes=65536;K->ServerNameFromHost=true;return true;
}
static bool xmqttTlsPlanCreate(const xmqttclienttlsconfig* K,xstrview Host,xmqttclient** Out,xmqtttlsplan* Plan)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || xrtMemRangesOverlap(K,sizeof(*K),Out,sizeof(*Out)) ||
       !K->MaxMetadataBytes || K->MaxMetadataBytes>1024u*1024u || K->Stream.HandshakeTimeout<0 || K->Stream.CloseTimeout<0 ||
       !K->Stream.AsyncBytesLimit || !K->Stream.AsyncCountLimit || !K->Stream.AsyncBatch || K->Session.ProtocolCount>256)
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS config, metadata bounds or output overlap");
    xtlsclientconfig S=K->Session;
    if(K->ServerNameFromHost && !S.ServerName.Size && Host.Size){
        bool Numeric=memchr(Host.Data,':',Host.Size)!=NULL;
        if(!Numeric && Host.Size<32){char Text[32];memcpy(Text,Host.Data,Host.Size);Text[Host.Size]=0;xnetaddr A;
            xerror* Saved=xrtTakeError();Numeric=xrtNetAddrParse(&A,Text,1);xrtSetErrorTake(Saved);}
        if(Numeric){if(!S.VerifyName.Size)S.VerifyName=Host;}else S.ServerName=Host;
    }
    if(!S.ServerName.Size && !S.VerifyName.Size)return xmqttClientError(XERR_ARGUMENT,"TLS peer identity is required");
    size_t Bytes=S.ProtocolCount*sizeof(xstrview);
    if(!xrtMemRangeValid(S.Protocols,Bytes) || xrtMemRangesOverlap(S.Protocols,Bytes,Out,sizeof(*Out)) || Bytes>K->MaxMetadataBytes)
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS ALPN descriptors");
    for(size_t I=0;I<S.ProtocolCount+2;++I){xstrview V=I==0?S.ServerName:I==1?S.VerifyName:S.Protocols[I-2];
        if(V.Size>K->MaxMetadataBytes-Bytes || !xrtMemRangeValid(V.Data,V.Size) || xrtMemRangesOverlap(V.Data,V.Size,Out,sizeof(*Out)) ||
           (I<2 && V.Size && memchr(V.Data,0,V.Size)))return xmqttClientError(XERR_ARGUMENT,"invalid or excessive TLS metadata");
        Bytes+=V.Size;
    }
    if(K->ClientAuth && (!xrtMemRangeValid(K->ClientAuth,sizeof(*K->ClientAuth)) || xrtMemRangesOverlap(K->ClientAuth,sizeof(*K->ClientAuth),Out,sizeof(*Out))))
        return xmqttClientError(XERR_ARGUMENT,"invalid TLS client authentication descriptor");
    Plan->Session=K->ClientAuth?xrtTlsClientCreateAuthV1(&S,K->ClientAuth,NULL):xrtTlsClientCreate(&S,NULL);
    if(!Plan->Session)return false;
    Plan->Config=K->Stream;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS_RESUME)
    Plan->Capture=K->Session.ResumeLimit!=0 && !K->ClientAuth;
#endif
    return true;
}
#endif
static bool xmqttClientDialCreate(xnetengine* Engine,xnetresolver* Resolver,xnetaddrlist* Addresses,xstrview Host,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* Config,const xmqttclientdialconfig* Dial,const void* Auth,const xmqttpacket* Connect,xmqttclient** Out,double Entered,const void* TLS,void* Inbox,void* Session)
{
    if(!Out || !xrtMemRangeValid(Out,sizeof(*Out)) || (Dial && (!xrtMemRangeValid(Dial,sizeof(*Dial)) || xrtMemRangesOverlap(Dial,sizeof(*Dial),Out,sizeof(*Out)))))
        return xmqttClientError(XERR_ARGUMENT,"invalid Dial config/output");
    xmqttclientdialconfig K;if(Dial)K=*Dial;else xmqttClientDialConfigInit(&K);
    if(K.Size!=sizeof(K) || (K.Family!=XNET_FAMILY_UNSPEC && K.Family!=XNET_FAMILY_IPV4 && K.Family!=XNET_FAMILY_IPV6) ||
       K.Timeout<XRT_WAIT_FOREVER || K.FallbackDelay<0 || !K.MaxAttempts || K.MaxAttempts>XMQTT_DIAL_ADDRESSES_MAX || !K.MaxHostBytes || K.MaxHostBytes>XMQTT_DIAL_HOST_MAX)
        return xmqttClientError(XERR_ARGUMENT,"invalid Dial family, timeout or limits");
    if(Addresses){size_t Count=xrtNetAddrListCount(Addresses),Eligible=0;
        if(!Count || Count>XMQTT_DIAL_ADDRESSES_MAX)return xmqttClientError(XERR_ARGUMENT,"invalid Dial endpoint count");
        for(size_t I=0;I<Count;++I){const xnetaddr* A=xrtNetAddrListGet(Addresses,I);if(!A)return false;
            if(xrtMemRangesOverlap(A,sizeof(*A),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"Dial output overlaps endpoint storage");
            if(K.Family!=XNET_FAMILY_UNSPEC && A->Family!=K.Family)continue;
            if(!A->Port)return xmqttClientError(XERR_ARGUMENT,"zero Dial endpoint port");
            ++Eligible;}
        if(!Eligible)return xmqttClientError(XERR_ARGUMENT,"no eligible Dial endpoint");
    }else if(!Resolver || !Port || !Host.Size || Host.Size>K.MaxHostBytes || !xrtMemRangeValid(Host.Data,Host.Size) ||
        xrtMemRangesOverlap(Host.Data,Host.Size,Out,sizeof(*Out)) || memchr(Host.Data,0,Host.Size))return xmqttClientError(XERR_ARGUMENT,"invalid Dial host, Resolver or port");
    xmqttdialplan P={0};P.Resolver=Resolver;P.Addresses=Addresses;P.Host=Host;P.Port=Port;xrtNetDialConfigInit(&P.Config);
    P.Config.Family=K.Family;P.Config.Timeout=K.Timeout;P.Config.FallbackDelay=K.FallbackDelay;P.Config.MaxAttempts=K.MaxAttempts;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
    xmqtttlsplan T={0};if(TLS && !xmqttTlsPlanCreate(TLS,Host,Out,&T))return false;
    bool OK=xmqttClientConnectImpl(Engine,NULL,Affinity,Config,Connect,Out,Auth,Entered,&P,TLS?&T:NULL,Inbox,Session);
    if(!OK)xrtTlsSessionDestroy(T.Session);
    return OK;
#else
    return xmqttClientConnectImpl(Engine,NULL,Affinity,Config,Connect,Out,Auth,Entered,&P,TLS,Inbox,Session);
#endif
}
XMQTT_API bool xmqttClientDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttpacket* Q,xmqttclient** O)
{return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,NULL,Q,O,xrtTimer(),NULL,NULL,NULL);}
XMQTT_API bool xmqttClientDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();if(!L)return xmqttClientError(XERR_ARGUMENT,"Dial addresses are NULL");
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,NULL,Q,O,Entered,NULL,NULL,NULL);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientDialAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,&K,Q,O,Entered,NULL,NULL,NULL);
}
XMQTT_API bool xmqttClientDialAddressesAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double Entered=xrtTimer();xmqttauthconfig K;if(!L)return xmqttClientError(XERR_ARGUMENT,"Dial addresses are NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,&K,Q,O,Entered,NULL,NULL,NULL);
}
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_DIAL)
XMQTT_API bool xmqttClientDialSession(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!S || !I)return xmqttClientError(XERR_ARGUMENT,"Session and Inbox are required");
    return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,NULL,Q,O,At,NULL,I,S);
}
XMQTT_API bool xmqttClientDialAddressesSession(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!S || !I || !L)return xmqttClientError(XERR_ARGUMENT,"Session, Inbox and endpoints are required");
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,NULL,Q,O,At,NULL,I,S);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientDialSessionAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!S || !I)return xmqttClientError(XERR_ARGUMENT,"Session and Inbox are required");
    if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,R,NULL,H,P,A,C,D,&K,Q,O,At,NULL,I,S);
}
XMQTT_API bool xmqttClientDialAddressesSessionAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,
    const xmqttclientdialconfig* D,const xmqttauthconfig* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!S || !I || !L)return xmqttClientError(XERR_ARGUMENT,"Session, Inbox and endpoints are required");
    if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientDialCreate(E,NULL,L,(xstrview){0},0,A,C,D,&K,Q,O,At,NULL,I,S);
}
#endif
#endif
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_TLS)
static bool xmqttClientTlsFactory(xnetengine* E,xnetresolver* Resolver,xnetaddrlist* L,const xnetaddr* Address,xstrview H,uint16 Port,uint64 Affinity,
    const xmqttclientconfig* K,const xmqttclienttlsconfig* TLS,const void* Auth,const xmqttpacket* Q,xmqttclient** Out,double Entered,void* Inbox,void* Session)
{
    if(!TLS || !xrtMemRangeValid(TLS,sizeof(*TLS)) || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) ||
       xrtMemRangesOverlap(TLS,sizeof(*TLS),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS config/output");
    xnetaddrlist* Temporary=NULL;
    if(Address){if(!xrtMemRangeValid(Address,sizeof(*Address)) || xrtMemRangesOverlap(Address,sizeof(*Address),Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid TLS numeric endpoint/output");
        Temporary=xrtNetAddrListCreate(Address,1);if(!Temporary)return false;L=Temporary;}
    bool OK=xmqttClientDialCreate(E,Resolver,L,H,Port,Affinity,K,&TLS->Dial,Auth,Q,Out,Entered,TLS,Inbox,Session);
    xrtNetAddrListDestroy(Temporary);return OK;
}
XMQTT_API bool xmqttClientTlsConnect(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");
    return xmqttClientTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,NULL,Q,O,At,NULL,NULL);
}
XMQTT_API bool xmqttClientTlsDial(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{return xmqttClientTlsFactory(E,R,NULL,NULL,H,P,A,C,T,NULL,Q,O,xrtTimer(),NULL,NULL);}
XMQTT_API bool xmqttClientTlsDialAddresses(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");
    return xmqttClientTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,NULL,Q,O,At,NULL,NULL);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientTlsConnectAuth(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,&K,Q,O,At,NULL,NULL);
}
XMQTT_API bool xmqttClientTlsDialAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,R,NULL,NULL,H,P,A,C,T,&K,Q,O,At,NULL,NULL);
}
XMQTT_API bool xmqttClientTlsDialAddressesAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,&K,Q,O,At,NULL,NULL);
}
#endif
#endif
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION_TLS)
static bool xmqttClientSessionTlsFactory(xnetengine* E,xnetresolver* R,xnetaddrlist* L,const xnetaddr* Address,xstrview H,uint16 Port,uint64 A,
    const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const void* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O,double At)
{
    if(!S || !I)return xmqttClientError(XERR_ARGUMENT,"Session and Inbox are required");
    return xmqttClientTlsFactory(E,R,L,Address,H,Port,A,C,T,Auth,Q,O,At,I,S);
}
XMQTT_API bool xmqttClientTlsConnectSession(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");
    return xmqttClientSessionTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,NULL,S,I,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDialSession(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{return xmqttClientSessionTlsFactory(E,R,NULL,NULL,H,P,A,C,T,NULL,S,I,Q,O,xrtTimer());}
XMQTT_API bool xmqttClientTlsDialAddressesSession(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");
    return xmqttClientSessionTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,NULL,S,I,Q,O,At);
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
XMQTT_API bool xmqttClientTlsConnectSessionAuth(xnetengine* E,const xnetaddr* R,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!R)return xmqttClientError(XERR_ARGUMENT,"TLS numeric endpoint is NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientSessionTlsFactory(E,NULL,NULL,R,(xstrview){0},0,A,C,T,&K,S,I,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDialSessionAuth(xnetengine* E,xnetresolver* R,xstrview H,uint16 P,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientSessionTlsFactory(E,R,NULL,NULL,H,P,A,C,T,&K,S,I,Q,O,At);
}
XMQTT_API bool xmqttClientTlsDialAddressesSessionAuth(xnetengine* E,xnetaddrlist* L,uint64 A,const xmqttclientconfig* C,const xmqttclienttlsconfig* T,const xmqttauthconfig* Auth,xmqttsession* S,xmqttclientinbox* I,const xmqttpacket* Q,xmqttclient** O)
{
    double At=xrtTimer();xmqttauthconfig K;if(!L)return xmqttClientError(XERR_ARGUMENT,"TLS endpoints are NULL");if(!xmqttAuthConfigCopy(Auth,&K))return false;
    return xmqttClientSessionTlsFactory(E,NULL,L,NULL,(xstrview){0},0,A,C,T,&K,S,I,Q,O,At);
}
#endif
#endif
XMQTT_API xfuture* xmqttClientReady(xmqttclient* C)
{if(!C){xmqttClientError(XERR_ARGUMENT,"client is NULL");return NULL;}return xrtFutureRef(C->ReadyFuture);}
static xmqttclientadmission xmqttClientSubmitAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out,uint8 Type)
{
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!C || !P || !Out || !xrtMemRangeValid(P,sizeof(*P)) || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<XRT_WAIT_FOREVER ||
       P->Type!=Type || (Type==XMQTT_PACKET_PUBLISH && (((P->Flags>>1)&3u)>2 || (P->Flags&8u)))){xmqttClientError(XERR_ARGUMENT,"invalid MQTT request arguments");return XMQTT_ADMISSION_ERROR;}
    bool Subscription=Type!=XMQTT_PACKET_PUBLISH;uint8 Qos=Subscription?0:(uint8)((P->Flags>>1)&3u);bool NeedId=Subscription || Qos;uint16 Identifier=0;xmqttpacket Adapted;const xmqttpacket* Packet=P;
    if(NeedId){Adapted=*P;if(!Adapted.Identifier)Adapted.Identifier=1;Packet=&Adapted;} /* sizing only */
    bool Alias=false;uint16 AliasId=0;
    if(!Subscription && C->Config.Version==XMQTT_VERSION_5 && P->Properties.Count<=C->Config.Codec.MaxProperties && P->Properties.Count<=SIZE_MAX/sizeof(xmqttproperty) &&
       (!P->Properties.Count || P->Properties.Data) && xrtMemRangeValid(P->Properties.Data,P->Properties.Count*sizeof(xmqttproperty))){
        for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_TOPIC_ALIAS){Alias=true;AliasId=(uint16)P->Properties.Data[I].Value.Number;}}
    xmqttcodecconfig Encoding;const xmqttcodecconfig* Codec=&C->Config.Codec;
    if(Alias){Encoding=*Codec;Encoding.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES;Codec=&Encoding;}
    size_t Bytes;bool Valid=C->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(Packet,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes):xmqttPacketSize311(Packet,XMQTT_CLIENT_TO_SERVER,Codec,&Bytes);
    if(!Valid){if(!Subscription && !P->Body.Publish.Topic.Size)xmqttClientError(XERR_ARGUMENT,"empty publish topic requires a valid outgoing alias");return XMQTT_ADMISSION_ERROR;}
    size_t Storage=Bytes;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    bool Session=C->Session && Qos,Shared=Session,AliasLocked=false;if(Shared)Storage=0;
#endif
    size_t Extra=xmqttRequestExtra(Storage,Alias,xmqttClientUsesTls(C));
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Session)Extra+=xmqttSessionRequestExtra(Storage,Alias,xmqttClientUsesTls(C),false);
#endif
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest) || Extra>SIZE_MAX-sizeof(xmqttrequest)-Bytes || Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    if(NeedId){xmqttclientadmission Admission=xmqttIdentifierReserveLocked(C,P->Identifier,&Identifier);
        if(Admission!=XMQTT_ADMISSION_ACCEPTED){if(Admission==XMQTT_ADMISSION_OVERLOADED)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
        Adapted.Identifier=Identifier;
    }
    if(Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){xmqttIdentifierReleaseLocked(C,Identifier);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    ++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;C->Info.PrivateBytes+=Extra;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Storage+Extra);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Type=Type;R->Retain=!Subscription && (P->Flags&1u)!=0;R->Qos=Qos;R->Identifier=Identifier;R->State.Release.Wire[0]=0x62;R->State.Release.Wire[1]=2;R->State.Release.Wire[2]=(unsigned char)(Identifier>>8);R->State.Release.Wire[3]=(unsigned char)Identifier;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    if(xmqttClientUsesTls(C))R->Capabilities|=XMQTT_TX_TLS;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Session)R->Capabilities|=XMQTT_TX_SESSION;
    if(Shared)R->Capabilities|=XMQTT_TX_SHARED;
#endif
    if(Alias){R->Capabilities|=XMQTT_TX_ALIAS;xmqttsendaliasrequest* A=xmqttSendAliasRequest(R);memset(A,0,sizeof(*A));A->Alias=AliasId;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Session)xmqttSessionRequest(R)->Packet=NULL;
#endif
    if(Subscription){
        R->Result.Subscription=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Subscription),&Failure);
        if(!R->Result.Subscription){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
        R->Result.Subscription->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Subscription)};
        R->State.TopicCount=P->Body.Topics.Count;R->Result.Subscription->Value.Requested=R->State.TopicCount;
        if(Type==XMQTT_PACKET_SUBSCRIBE){
            for(size_t I=0;I<P->Properties.Count;++I)if(P->Properties.Data[I].Id==XMQTT_PROPERTY_SUBSCRIPTION_ID)R->Capabilities|=4u;
            for(size_t I=0;I<P->Body.Topics.Count;++I){xstrview Filter=P->Body.Topics.Data[I].Filter;
                if(Filter.Size>=7 && !memcmp(Filter.Data,"$share/",7))R->Capabilities|=2u;
                for(size_t J=0;J<Filter.Size;++J)if(Filter.Data[J]=='+' || Filter.Data[J]=='#'){R->Capabilities|=1u;break;}
            }
        }
    }else {
        R->Result.Write=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Write),&Failure);if(!R->Result.Write){if(Failure==XMQTT_STATUS_LIMIT)goto Overload;goto Failed;}
        R->Result.Write->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Write)};
    }
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    /* Serialize alias transactions through successful queue publication,
     * including QoS0 bindings. Neither C->Gate nor the Session gate protects
     * payload encoding; close/IO and ordinary publishers remain independent. */
    if(Alias && C->Session){xrtMutexLock(&C->Session->AliasGate);AliasLocked=true;
        xrtMutexLock(&C->Gate);if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);goto Closed;}
        if(Shared){xmqttclientadmission A=xmqttSendAliasSnapshotLocked(C,R,&Packet->Body.Publish.Topic);
            if(A!=XMQTT_ADMISSION_ACCEPTED){xrtMutexUnlock(&C->Gate);if(A==XMQTT_ADMISSION_OVERLOADED)goto Overload;goto Failed;}}
        xrtMutexUnlock(&C->Gate);}
    if(Shared){xmqttpacket Logical=*Packet;if(Alias){xmqttsendtopic* T=xmqttSendAliasRequest(R)->Topic;Logical.Body.Publish.Topic=(xstrview){T->Data,T->Size};}
        xmqttsessionstatus S=xmqttInternalSessionSnapshot(C->Session->State,C,&Logical,&xmqttSessionRequest(R)->Packet);
        if(S!=XMQTT_SESSION_DONE){if(S==XMQTT_SESSION_LIMIT)goto Overload;goto Failed;}}
    else
#endif
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);goto Closed;}
    if(Alias
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
       && !Shared
#endif
    ){xmqttclientadmission A=xmqttSendAliasSnapshotLocked(C,R,NULL);if(A!=XMQTT_ADMISSION_ACCEPTED){xrtMutexUnlock(&C->Gate);if(A==XMQTT_ADMISSION_OVERLOADED)goto Overload;goto Failed;}}
    /* Publish no accepted request unless its worker wake is committed. The
     * post cannot run inline and cannot observe the queue until Gate unlocks. */
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    if(Alias)xmqttSendAliasCommitLocked(C,R);
    if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(AliasLocked)xrtMutexUnlock(&C->Session->AliasGate);
#endif
    *Out=F;return XMQTT_ADMISSION_ACCEPTED;
Closed:
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(AliasLocked)xrtMutexUnlock(&C->Session->AliasGate);
#endif
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_CLOSED;
Overload:
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(AliasLocked)xrtMutexUnlock(&C->Session->AliasGate);
#endif
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_OVERLOADED;
Failed:
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(AliasLocked)xrtMutexUnlock(&C->Session->AliasGate);
#endif
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);xmqttRequestResultFree(R);xmqttRequestDrop(R);return XMQTT_ADMISSION_ERROR;
UndoReservation:
    xrtMutexLock(&C->Gate);xmqttIdentifierReleaseLocked(C,Identifier);--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;C->Info.PrivateBytes-=Extra;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientPublishAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_PUBLISH);}
XMQTT_API xmqttclientadmission xmqttClientSubscribeAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_SUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientUnsubscribeAsync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubmitAsync(C,P,Timeout,Out,XMQTT_PACKET_UNSUBSCRIBE);}
static xmqttclientadmission xmqttClientSubscriptionSync(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out,uint8 Type)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously change subscriptions from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientSubmitAsync(C,P,Timeout,Out,Type);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API xmqttclientadmission xmqttClientSubscribe(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubscriptionSync(C,P,Timeout,Out,XMQTT_PACKET_SUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientUnsubscribe(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{return xmqttClientSubscriptionSync(C,P,Timeout,Out,XMQTT_PACKET_UNSUBSCRIBE);}
XMQTT_API xmqttclientadmission xmqttClientPublish(xmqttclient* C,const xmqttpacket* P,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously publish from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientPublishAsync(C,P,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API xmqttclientadmission xmqttClientDisconnectAsync(xmqttclient* C,const xmqttpacket* Packet,int64 Timeout,xfuture** Out)
{
    double Deadline=Timeout?xmqttClientDeadline(xrtTimer(),Timeout):-INFINITY;xmqttpacket Default;
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)) || Timeout<0 || (Packet && !xrtMemRangeValid(Packet,sizeof(*Packet)))){xmqttClientError(XERR_ARGUMENT,"invalid graceful disconnect arguments");return XMQTT_ADMISSION_ERROR;}
    if(!Packet){xmqttPacketInit(XMQTT_PACKET_DISCONNECT,&Default);Packet=&Default;}
    if(Packet->Type!=XMQTT_PACKET_DISCONNECT){xmqttClientError(XERR_ARGUMENT,"expected DISCONNECT packet");return XMQTT_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
    if(!C->Info.Ready){xrtMutexUnlock(&C->Gate);xmqttClientError(XERR_STATE,"graceful disconnect requires accepted CONNACK");return XMQTT_ADMISSION_ERROR;}
    xmqttnegotiated Peer=C->Peer;xrtMutexUnlock(&C->Gate);size_t Bytes=0;xmqttnegotiationissue Issue;
    if(xmqttNegotiatedPacketCheck(&Peer,Packet,&C->Config.Codec,&Bytes,&Issue)!=XMQTT_NEGOTIATION_DONE){xmqttClientError(XERR_ARGUMENT,"DISCONNECT violates packet or negotiated limits");return XMQTT_ADMISSION_ERROR;}
    size_t Extra=xmqttRequestExtra(Bytes,false,xmqttClientUsesTls(C));
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_CLOSED;}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && C->Auth->Active){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
#endif
    if(C->DisconnectReserved || C->Info.Requests==C->Config.MaxRequests || Bytes>C->Config.MaxOutgoingBytes-C->Info.OutgoingBytes || Bytes>SIZE_MAX-sizeof(xmqttrequest) || Extra>SIZE_MAX-sizeof(xmqttrequest)-Bytes || Extra>C->Config.MaxPrivateBytes-C->Info.PrivateBytes){
        ++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XMQTT_ADMISSION_OVERLOADED;}
    C->DisconnectReserved=true;++C->Info.Requests;C->Info.OutgoingBytes+=Bytes;C->Info.PrivateBytes+=Extra;xmqttClientRef(C);xrtMutexUnlock(&C->Gate);
    xmqttrequest* R=xrtMalloc(sizeof(*R)+Bytes+Extra);xfuture* F=NULL;xmqttclientstatus Failure=XMQTT_STATUS_LIMIT;xmqttclientadmission Admission=XMQTT_ADMISSION_ERROR;
    if(!R)goto UndoReservation;
    memset(R,0,sizeof(*R));R->Client=C;R->Bytes=Bytes;R->Deadline=Deadline;R->Type=XMQTT_PACKET_DISCONNECT;xrtAtomic32Init(&R->Refs,1);xrtAtomic32Init(&R->NativeRefs,0);
    if(xmqttClientUsesTls(C))R->Capabilities|=XMQTT_TX_TLS;
    R->Result.Write=xmqttBoxAllocate(C->Quota,sizeof(*R->Result.Write),&Failure);
    if(!R->Result.Write){if(Failure==XMQTT_STATUS_LIMIT)Admission=XMQTT_ADMISSION_OVERLOADED;goto Failed;}
    R->Result.Write->Box=(xmqttbox){C->Quota,sizeof(*R->Result.Write)};R->Result.Write->Value.Reason=Packet->Reason;
    if(!(C->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL):xmqttPacketWrite311(R->Wire,Bytes,Packet,XMQTT_CLIENT_TO_SERVER,&C->Config.Codec,NULL)))goto Failed;
    R->Promise=xrtPromiseCreate(&F,NULL);if(!R->Promise)goto Failed;
    xmqttRequestRef(R);if(!xrtPromiseProducerBindTakeV2(R->Promise,(xrtownershipref){R,&xmqttPrivateOps},&xmqttRequestProducer,&xmqttRequestCancellation)){xmqttRequestDrop(R);goto Failed;}
    R->Cancel=xrtPromiseCancelToken(R->Promise);if(!R->Cancel)goto Failed;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XMQTT_CLIENT_CLOSING){xrtMutexUnlock(&C->Gate);Admission=XMQTT_ADMISSION_CLOSED;goto Failed;}
    if(!xmqttClientWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Failed;}
    C->DisconnectReserved=false;C->Disconnect=R;C->ApplicationClose=true;C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=XMQTT_STATUS_OK;C->Info.LocalReason=Packet->Reason;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Out=F;return XMQTT_ADMISSION_ACCEPTED;
Failed:
    xrtMutexLock(&C->Gate);C->DisconnectReserved=false;if(Admission!=XMQTT_ADMISSION_ERROR)++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    xrtPromiseDestroy(R->Promise);xrtFutureDestroy(F);if(R->Result.Write)xmqttWriteFree(R->Result.Write,NULL);xmqttRequestDrop(R);return Admission;
UndoReservation:
    xrtMutexLock(&C->Gate);C->DisconnectReserved=false;--C->Info.Requests;C->Info.OutgoingBytes-=Bytes;C->Info.PrivateBytes-=Extra;xrtMutexUnlock(&C->Gate);xmqttClientDrop(C);return XMQTT_ADMISSION_ERROR;
}
XMQTT_API xmqttclientadmission xmqttClientDisconnect(xmqttclient* C,const xmqttpacket* Packet,int64 Timeout,xfuture** Out)
{
    if(C && xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot synchronously disconnect from an Engine worker");return XMQTT_ADMISSION_ERROR;}
    xmqttclientadmission S=xmqttClientDisconnectAsync(C,Packet,Timeout,Out);if(S==XMQTT_ADMISSION_ACCEPTED)(void)xrtFutureWaitFor(*Out,XRT_WAIT_FOREVER);return S;
}
XMQTT_API bool xmqttClientTakeMessage(xmqttclient* C,xmqttmessage** Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid message output");
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox)return xmqttClientInboxTake(C->Inbox,Out);
#endif
    xrtMutexLock(&C->Gate);xmqttmessage* M=C->MessageHead;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_AUTH)
    if(C->Auth && !C->Auth->Verified)M=NULL;
#endif
    if(M){C->MessageHead=M->Next;if(!C->MessageHead)C->MessageTail=NULL;--C->Info.Messages;C->Info.MessageBytes-=M->Box.Bytes;M->Next=NULL;}
    xrtMutexUnlock(&C->Gate);if(!M)return false;*Out=M;return true;
}
XMQTT_API bool xmqttClientInfo(xmqttclient* C,xmqttclientinfo* Out)
{
    if(!C || !Out || !xrtMemRangeValid(Out,sizeof(*Out)))return xmqttClientError(XERR_ARGUMENT,"invalid client info output");
    xrtMutexLock(&C->Gate);xmqttclientinfo I=C->Info;xrtMutexUnlock(&C->Gate);
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_INBOX)
    if(C->Inbox){xrtMutexLock(&C->Inbox->Gate);I.Messages=C->Inbox->Count;I.MessageBytes=C->Inbox->Bytes;xrtMutexUnlock(&C->Inbox->Gate);}
#endif
    xrtMutexLock(&C->Quota->Gate);I.Objects=C->Quota->Count;I.ObjectBytes=C->Quota->Bytes;xrtMutexUnlock(&C->Quota->Gate);*Out=I;return true;
}
XMQTT_API bool xmqttClientClose(xmqttclient* C)
{
    if(!C)return xmqttClientError(XERR_ARGUMENT,"client is NULL");
    xrtMutexLock(&C->Gate);
    if(C->Info.State==XMQTT_CLIENT_CLOSED){xrtMutexUnlock(&C->Gate);return true;}
    if(C->ApplicationClose && C->Info.CloseStatus==XMQTT_STATUS_OK)C->Info.CloseStatus=XMQTT_STATUS_CLOSED;
    C->AbortRequested=true;
    if(C->Info.State<XMQTT_CLIENT_CLOSING){C->Info.State=XMQTT_CLIENT_CLOSING;C->Info.CloseStatus=XMQTT_STATUS_CLOSED;}
    bool OK=xmqttClientWakeLocked(C);xrtMutexUnlock(&C->Gate);return OK;
}
XMQTT_API xwaitresult xmqttClientWaitFor(xmqttclient* C,int64 Timeout)
{
    if(!C || Timeout<XRT_WAIT_FOREVER){xmqttClientError(XERR_ARGUMENT,"invalid client wait");return XWAIT_ERROR;}
    if(xrtNetEngineCurrent(C->Engine)){xmqttClientError(XERR_STATE,"cannot wait from an Engine worker");return XWAIT_ERROR;}
    double Deadline=xmqttClientDeadline(xrtTimer(),Timeout);
    if(!xrtMutexLock(&C->RetirementLock))return XWAIT_ERROR;
    xwaitresult Result=XWAIT_OK;
    while(xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE)!=1){
        if(!Timeout || xrtTimer()>=Deadline){Result=XWAIT_TIMEOUT;break;}
        Result=xrtCondWaitFor(&C->RetirementReady,&C->RetirementLock,xmqttClientRemaining(Deadline));
        if(Result!=XWAIT_OK && Result!=XWAIT_TIMEOUT)break;
    }
    bool Retired=xrtAtomic32Load(&C->Refs,XMEMORY_ACQUIRE)==1;
    if(!xrtMutexUnlock(&C->RetirementLock))return XWAIT_ERROR;
    if(Result==XWAIT_ERROR)return Result;
    if(!Retired)return XWAIT_TIMEOUT;
    /* Never acquire Gate under RetirementLock: failed admission may drop its
     * last asynchronous reference while Gate is held. Caller owner stays live. */
    xrtMutexLock(&C->Gate);bool Closed=C->Info.State==XMQTT_CLIENT_CLOSED && !C->Info.Requests && !C->Info.StartupBytes;xrtMutexUnlock(&C->Gate);
    if(!Closed){xmqttClientError(XERR_STATE,"client retired before logical close");return XWAIT_ERROR;}
    /* Logical Client publication can precede the actual native callback tail.
     * Carry only the original budget, rounded to the public millisecond unit. */
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_DIAL)
    if(!C->Stream)return XWAIT_OK;
#endif
    return xrtNetStreamWaitRetired(C->Stream,(!Timeout || xrtTimer()>=Deadline)?0:xmqttClientRemaining(Deadline));
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


/* ========================================================================== */
/* source: extlibs/xmqtt/src/session/session.c */
/* ========================================================================== */

#if defined(XMQTT_FEATURE_XMQTT_SESSION)
#include <stdlib.h>
#include <math.h>
#include <string.h>
#if defined(XMQTT_FEATURE_XMQTT_SESSION)
enum {XMQTT_SESSION_ID_WORDS=2048,XMQTT_SESSION_ID_BYTES=8192,XMQTT_SESSION_RX_BYTES=16384};
struct xmqttsessionpacket {
    xatomic32 Refs;xmqttsession* Owner;
    xmqttsessionpacket *Next,*Previous,*HashNext,*ReadyNext,*ReadyPrevious;
    size_t Allocation,Bytes;uint16 Identifier;uint8 Qos,Reason,PubrecReason,ReadyKind,SelectedType;
    bool Active,Sent,Attempted,ReleaseSent,PubrecReceived,Need,Retry;
    xmqttsessionstage Stage;unsigned char Duplicate,Release[4];
    xmqttpacket Packet;xmqttproperty Properties[];
};
struct xmqttsession {
    xatomic32 Refs;xmutex Gate;xmqttsessionconfig Config;xmqttnegotiated Peer;
    size_t BaseBytes,LiveBytes,Objects,Active,RxCount,Prepared,Buckets;
    xmqttsessionpacket **Hash,*Head,*Tail,*ReadyHead[2],*ReadyTail[2],*Selected;
    unsigned char *Ids,*Receive;uint16 Next,Quota,InitialQuota;uint64 Generation,Selection;
    bool Connected,HasState,Persistent,InfiniteExpiry;uint32 Expiry;double LastNow,ExpiresAt,ClosedAt;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    const void* Driver;const xmqttcodecconfig* DriverCodec;
#endif
    unsigned char Data[];
};
static xmqttsessionstatus xmqttSessionFailure(xmqttsessionstatus Status,cstr Text)
{
    xerrkind K=Status==XMQTT_SESSION_STATE_ERROR?XERR_STATE:Status==XMQTT_SESSION_PROTOCOL_ERROR?XERR_PROTOCOL:Status==XMQTT_SESSION_RESTRICTED?XERR_RANGE:XERR_ARGUMENT;
    xrtSetErrorInfo(K,"xrt.mqtt",6001-(int32)Status,Text);return Status;
}
static bool xmqttSessionOwner(xmqttsession* S,const void* Owner)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(S->Driver!=Owner){xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session is claimed by another protocol owner");return false;}
#else
    (void)S;(void)Owner;
#endif
    return true;
}
static bool xmqttSessionSpan(const void* P,size_t N)
{return (P || !N) && xrtMemRangeValid(P,N);}
static bool xmqttSessionSeparate(const void* A,size_t N,const void* B,size_t M)
{return !N || !M || !xrtMemRangesOverlap(A,N,B,M);}
static bool xmqttSessionOutput(xmqttsession* S,const void* P,size_t N)
{return xmqttSessionSpan(P,N) && xmqttSessionSeparate(P,N,S,S->BaseBytes);}
/* Descriptor validation precedes this traversal. */
static bool xmqttSessionPropertiesOutput(const void* O,size_t N,xmqttpropertylist L)
{
    if(!xmqttSessionSeparate(O,N,L.Data,L.Count*sizeof(*L.Data)))return false;
    for(size_t I=0;I<L.Count;++I){const xmqttproperty* P=L.Data+I;
        if(P->Type==XMQTT_PROPERTY_STRING && !xmqttSessionSeparate(O,N,P->Value.Text.Data,P->Value.Text.Size))return false;
        if(P->Type==XMQTT_PROPERTY_BINARY && !xmqttSessionSeparate(O,N,P->Value.Binary.Data,P->Value.Binary.Size))return false;
        if(P->Type==XMQTT_PROPERTY_STRING_PAIR && (!xmqttSessionSeparate(O,N,P->Value.Pair.Name.Data,P->Value.Pair.Name.Size) || !xmqttSessionSeparate(O,N,P->Value.Pair.Value.Data,P->Value.Pair.Value.Size)))return false;
    }return true;
}
static bool xmqttSessionPacketOutput(const void* O,size_t N,const xmqttpacket* P)
{
    if(!xmqttSessionSeparate(O,N,P,sizeof(*P)) || !xmqttSessionPropertiesOutput(O,N,P->Properties))return false;
    if(P->Type==XMQTT_PACKET_PUBLISH)return xmqttSessionSeparate(O,N,P->Body.Publish.Topic.Data,P->Body.Publish.Topic.Size) && xmqttSessionSeparate(O,N,P->Body.Publish.Payload.Data,P->Body.Publish.Payload.Size);
    if(P->Type==XMQTT_PACKET_CONNECT){const xmqttconnectpacket* C=&P->Body.Connect;
        if(!xmqttSessionSeparate(O,N,C->ClientId.Data,C->ClientId.Size))return false;
        if(C->HasUsername && !xmqttSessionSeparate(O,N,C->Username.Data,C->Username.Size))return false;
        if(C->HasPassword && !xmqttSessionSeparate(O,N,C->Password.Data,C->Password.Size))return false;
        if(C->HasWill && (!xmqttSessionSeparate(O,N,C->WillTopic.Data,C->WillTopic.Size) || !xmqttSessionSeparate(O,N,C->WillPayload.Data,C->WillPayload.Size) || !xmqttSessionPropertiesOutput(O,N,C->WillProperties)))return false;
    }return true;
}
static const xmqttcodecconfig* xmqttSessionCodec(xmqttsession* S,const void* Owner)
{
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Owner)return S->DriverCodec;
#else
    (void)Owner;
#endif
    return &S->Config.Codec;
}
static bool xmqttSessionSizePacket(xmqttsession* S,const void* Owner,const xmqttpacket* P,xmqttdirection D,size_t* N)
{const xmqttcodecconfig* K=xmqttSessionCodec(S,Owner);return S->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(P,D,K,N):xmqttPacketSize311(P,D,K,N);}
static void xmqttSessionDrop(xmqttsession* S)
{if(xrtAtomic32FetchSub(&S->Refs,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&S->Gate);xrtFree(S);}}
/* Next holds Gate and the ready list's active reference. There is one
 * speculative two-reference reservation per packet at most. Public Retain
 * cannot increment at/above its cap, so a supported count is at most cap+2,
 * far below UINT32_MAX. Refund is a raw decrement, never a physical release;
 * the active reference keeps it nonzero while concurrent callers release. */
static bool xmqttSessionPacketRef(xmqttsessionpacket* P)
{
    uint32 N=xrtAtomic32FetchAdd(&P->Refs,2,XMEMORY_RELAXED);
    if(N && N<=UINT32_MAX/2u-2u)return true;
    (void)xrtAtomic32FetchSub(&P->Refs,2,XMEMORY_RELAXED);return false;
}
XMQTT_API bool xmqttSessionPacketRetain(const xmqttsessionpacket* Packet)
{
    if(!Packet){xmqttSessionFailure(XMQTT_SESSION_ERROR,"session packet is NULL");return false;}
    xmqttsessionpacket* P=(xmqttsessionpacket*)Packet;uint32 N=xrtAtomic32Load(&P->Refs,XMEMORY_RELAXED);
    while(N && N<UINT32_MAX/2u)if(xrtAtomic32CompareExchange(&P->Refs,&N,N+1,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;
    xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session packet reference limit");return false;
}
static uint16 xmqttSessionIdFind(const xmqttsession* S,const unsigned char* Used,uint16 First)
{
    /* Scan the first partial group, wrap, then visit its remaining low bits.
     * Both byte maps are protected by the caller's existing gates. */
    unsigned Group=First>>5,Start=First&31u;
    for(unsigned Step=0;Step<=XMQTT_SESSION_ID_WORDS;++Step){
        if(Step==XMQTT_SESSION_ID_WORDS && !Start)break;
        unsigned Index=(Group+Step)&(XMQTT_SESSION_ID_WORDS-1u),Bit=Step?0:Start;
        size_t Offset=Index*4u;uint32 Busy=S->Ids?(uint32)S->Ids[Offset]|((uint32)S->Ids[Offset+1]<<8)|((uint32)S->Ids[Offset+2]<<16)|((uint32)S->Ids[Offset+3]<<24):0;
        if(Used){size_t At=Index*4u;Busy|=(uint32)Used[At]|((uint32)Used[At+1]<<8)|((uint32)Used[At+2]<<16)|((uint32)Used[At+3]<<24);}
        if(!Index)Busy|=1u; /* Identifier zero is never assignable. */
        uint32 Free=~Busy;
        if(!Step)Free&=UINT32_MAX<<Start;
        if(Step==XMQTT_SESSION_ID_WORDS)Free&=(1u<<Start)-1u;
        if(Free){Free>>=Bit;while(!(Free&1u)){Free>>=1;++Bit;}return (uint16)(Index*32u+Bit);}
    }
    return 0;
}
static void xmqttSessionReservationDrop(xmqttsession* S,uint16 Id,size_t Allocation)
{xrtMutexLock(&S->Gate);S->LiveBytes-=Allocation;--S->Objects;S->Ids[Id>>3]&=(unsigned char)~(1u<<(Id&7u));xrtMutexUnlock(&S->Gate);xmqttSessionDrop(S);}
XMQTT_API void xmqttSessionPacketRelease(const xmqttsessionpacket* Packet)
{
    xmqttsessionpacket* P=(xmqttsessionpacket*)Packet;if(!P || xrtAtomic32FetchSub(&P->Refs,1,XMEMORY_ACQ_REL)!=1)return;
    xmqttsession* S=P->Owner;size_t Allocation=P->Allocation;uint16 Id=P->Identifier;
    /* Keep the physical reservation until the allocator has actually retired
     * the packet. The packet's state reference still protects S throughout. */
    xrtFree(P);xmqttSessionReservationDrop(S,Id,Allocation);
}
static void xmqttSessionListRelease(xmqttsessionpacket* P)
{while(P){xmqttsessionpacket* Next=P->Next;xmqttSessionPacketRelease(P);P=Next;}}
static unsigned xmqttSessionReceiveState(const xmqttsession* S,uint16 Id)
{return S->Receive?(S->Receive[Id>>2]>>((Id&3u)*2u))&3u:0;}
static void xmqttSessionReceiveSet(xmqttsession* S,uint16 Id,unsigned State)
{unsigned Shift=(Id&3u)*2u;S->Receive[Id>>2]=(unsigned char)((S->Receive[Id>>2]&~(3u<<Shift))|(State<<Shift));}
static size_t xmqttSessionHash(const xmqttsession* S,uint16 Id)
{return ((uint32)Id*2654435761u)&(S->Buckets-1u);}
static xmqttsessionpacket* xmqttSessionFind(xmqttsession* S,uint16 Id)
{
    if(!S->Hash)return NULL;
    for(xmqttsessionpacket* P=S->Hash[xmqttSessionHash(S,Id)];P;P=P->HashNext)if(P->Identifier==Id)return P;
    return NULL;
}
static void xmqttSessionReadyRemove(xmqttsession* S,xmqttsessionpacket* P)
{
    if(!P->ReadyKind)return;
    unsigned K=P->ReadyKind-1u;
    if(P->ReadyPrevious)P->ReadyPrevious->ReadyNext=P->ReadyNext;else S->ReadyHead[K]=P->ReadyNext;
    if(P->ReadyNext)P->ReadyNext->ReadyPrevious=P->ReadyPrevious;else S->ReadyTail[K]=P->ReadyPrevious;
    P->ReadyKind=0;P->ReadyNext=P->ReadyPrevious=NULL;
}
static void xmqttSessionReadyAdd(xmqttsession* S,xmqttsessionpacket* P)
{
    P->Need=true;if(P->ReadyKind || S->Selected==P)return;
    unsigned K=P->Stage==XMQTT_SESSION_PUBREL?1u:0u;P->ReadyKind=(uint8)(K+1u);P->ReadyPrevious=S->ReadyTail[K];P->ReadyNext=NULL;
    if(S->ReadyTail[K])S->ReadyTail[K]->ReadyNext=P;else S->ReadyHead[K]=P;S->ReadyTail[K]=P;
}
static void xmqttSessionUnlink(xmqttsession* S,xmqttsessionpacket* P)
{
    xmqttSessionReadyRemove(S,P);xmqttsessionpacket** Link=S->Hash+xmqttSessionHash(S,P->Identifier);while(*Link!=P)Link=&(*Link)->HashNext;*Link=P->HashNext;
    if(P->Previous)P->Previous->Next=P->Next;else S->Head=P->Next;
    if(P->Next)P->Next->Previous=P->Previous;else S->Tail=P->Previous;
    P->Next=P->Previous=P->HashNext=NULL;P->Active=false;P->Need=false;--S->Active;
}
static xmqttsessionpacket* xmqttSessionClearLocked(xmqttsession* S)
{
    xmqttsessionpacket* P=S->Head;S->Head=S->Tail=NULL;S->Active=0;
    for(xmqttsessionpacket* Q=P;Q;Q=Q->Next){Q->Active=Q->Need=false;Q->ReadyKind=0;Q->Stage=XMQTT_SESSION_DISCARDED;}
    if(S->Hash)memset(S->Hash,0,S->Buckets*sizeof(*S->Hash));
    if(S->Receive)memset(S->Receive,0,XMQTT_SESSION_RX_BYTES);
    S->ReadyHead[0]=S->ReadyHead[1]=S->ReadyTail[0]=S->ReadyTail[1]=NULL;
    S->RxCount=S->Prepared=0;S->HasState=false;S->ExpiresAt=INFINITY;return P;
}
static bool xmqttSessionTime(xmqttsession* S,double Now)
{return isfinite(Now) && Now>=0 && Now>=S->LastNow;}
static bool xmqttSessionAvailableAt(const xmqttsession* S,double Now)
{return S->HasState && (S->InfiniteExpiry || Now-S->ClosedAt<(double)S->Expiry);}
XMQTT_API bool xmqttSessionConfigInit(xmqttsessionconfig* C)
{
    if(!xmqttSessionSpan(C,sizeof(*C))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session config output");return false;}
    *C=(xmqttsessionconfig){0};C->Size=sizeof(*C);C->Version=XMQTT_VERSION_5;xmqttCodecConfigInit(&C->Codec);C->MaxOutgoing=128;C->MaxIncoming=64;C->MaxBytes=16u*1024u*1024u;return true;
}
XMQTT_API bool xmqttSessionCreate(const xmqttsessionconfig* Config,xmqttsession** Out)
{
    if(!xmqttSessionSpan(Config,sizeof(*Config)) || !xmqttSessionSpan(Out,sizeof(*Out)) || !xmqttSessionSeparate(Config,sizeof(*Config),Out,sizeof(*Out))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session config/output");return false;}
    xmqttsessionconfig C=*Config;xmqttcodecconfig Codec;
    if(C.Size!=sizeof(C) || (C.Version!=XMQTT_VERSION_5 && C.Version!=XMQTT_VERSION_311) || C.MaxOutgoing>65535 || C.MaxIncoming>65535 || !C.ClientId.Size || C.ClientId.Size>65535 ||
       !xmqttSessionSpan(C.ClientId.Data,C.ClientId.Size) || !xmqttSessionSeparate(Out,sizeof(*Out),C.ClientId.Data,C.ClientId.Size) ||
       !xmqttInternalCodecSettings(XMQTT_CLIENT_TO_SERVER,&C.Codec,&Codec) || !xmqttUtf8StringCheck(C.ClientId,C.Codec.Utf8Flags,NULL)){
        xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session settings/identity");return false;}
    size_t Buckets=0;if(C.MaxOutgoing){Buckets=2;while(Buckets<C.MaxOutgoing*2u)Buckets*=2u;}
    size_t Base=sizeof(xmqttsession)+Buckets*sizeof(xmqttsessionpacket*)+(C.MaxOutgoing?XMQTT_SESSION_ID_BYTES:0u)+(C.MaxIncoming?XMQTT_SESSION_RX_BYTES:0u)+C.ClientId.Size;
    if(Base>C.MaxBytes){xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session fixed maps exceed byte budget");return false;}
    xmqttsession* S=xrtCalloc(1,Base);if(!S)return false;
    if(!xrtMutexInit(&S->Gate)){xrtFree(S);return false;}
    xrtAtomic32Init(&S->Refs,1);S->Config=C;S->Config.Codec=Codec;S->BaseBytes=S->LiveBytes=Base;S->Buckets=Buckets;S->Next=1;S->ExpiresAt=INFINITY;
    unsigned char* At=S->Data;
    if(Buckets){S->Hash=(xmqttsessionpacket**)At;At+=Buckets*sizeof(*S->Hash);S->Ids=At;At+=XMQTT_SESSION_ID_BYTES;}
    if(C.MaxIncoming){S->Receive=At;At+=XMQTT_SESSION_RX_BYTES;}
    memcpy(At,C.ClientId.Data,C.ClientId.Size);S->Config.ClientId=(xstrview){(const char*)At,C.ClientId.Size};*Out=S;return true;
}
static xmqttsessionstatus xmqttSessionOpenOwned(xmqttsession* S,const void* Owner,const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out)))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session negotiation output");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}bool Available=xmqttSessionAvailableAt(S,Now);
    if(S->Connected || !xmqttSessionTime(S,Now) || S->Generation==UINT64_MAX){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"invalid session generation/time");}
    xmqttnegotiationconfig C;xmqttNegotiationConfigInit(&C);C.Version=S->Config.Version;C.Codec=*xmqttSessionCodec(S,Owner);C.HasSessionState=Available;
    xmqttnegotiated N;xmqttnegotiationstatus Status=xmqttNegotiate(Connect,Connack,&C,&N,NULL);
    if(Status<0){xrtMutexUnlock(&S->Gate);return Status==XMQTT_NEGOTIATION_STATE_ERROR?XMQTT_SESSION_STATE_ERROR:Status==XMQTT_NEGOTIATION_LOCAL_ERROR?XMQTT_SESSION_ERROR:XMQTT_SESSION_PROTOCOL_ERROR;}
    if(!xmqttSessionPacketOutput(Out,sizeof(*Out),Connect) || !xmqttSessionPacketOutput(Out,sizeof(*Out),Connack)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"overlapping negotiation output");}
    if(Connect->Body.Connect.ClientId.Size!=S->Config.ClientId.Size || memcmp(Connect->Body.Connect.ClientId.Data,S->Config.ClientId.Data,S->Config.ClientId.Size)){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session ClientId mismatch");}
    if(Status==XMQTT_NEGOTIATION_REJECTED){*Out=N;xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_REJECTED;}
    if(N.SessionPresent && !Available){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"server session without local state");}
    if(S->Config.Version==XMQTT_VERSION_311 && Available && !Connect->Body.Connect.CleanStart && !N.SessionPresent){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"311 server lost session; explicit reset required");}
    bool Reset=S->HasState && (!Available || Connect->Body.Connect.CleanStart || !N.SessionPresent);
    xmqttsessionpacket* Drop=Reset?xmqttSessionClearLocked(S):NULL;
    S->Connected=S->HasState=true;S->Persistent=!Connect->Body.Connect.CleanStart;S->Peer=N;
    S->Peer.ClientId=S->Config.ClientId;S->Peer.AuthMethod=S->Peer.ServerReference=S->Peer.ResponseInfo=(xstrview){0};S->Peer.HasAuthMethod=false;
    S->LastNow=Now;S->ExpiresAt=INFINITY;S->InfiniteExpiry=true;++S->Generation;S->Selection=0;S->Quota=S->InitialQuota=N.SendReceiveMaximum;
    for(xmqttsessionpacket* P=S->Head;P;P=P->Next){P->ReleaseSent=false;xmqttSessionReadyAdd(S,P);}
    *Out=N;xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return Reset?XMQTT_SESSION_RESET:XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionEndOwned(xmqttsession* S,const void* Owner,const xmqttpacket* Disconnect,double Now)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}
    if(!S->Connected || !xmqttSessionTime(S,Now)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session is not connected or time went backwards");}
    uint32 Expiry=S->Peer.SessionExpiry;
    if(Disconnect){
        if(!xmqttSessionSpan(Disconnect,sizeof(*Disconnect)) || Disconnect->Type!=XMQTT_PACKET_DISCONNECT){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid ended DISCONNECT");}
        if(xmqttNegotiatedPacketCheck(&S->Peer,Disconnect,xmqttSessionCodec(S,Owner),NULL,NULL)!=XMQTT_NEGOTIATION_DONE){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
        for(size_t I=0;I<Disconnect->Properties.Count;++I)if(Disconnect->Properties.Data[I].Id==XMQTT_PROPERTY_SESSION_EXPIRY)Expiry=Disconnect->Properties.Data[I].Value.Number;
    }
    S->Connected=false;S->LastNow=Now;S->Quota=S->InitialQuota=0;xmqttsessionpacket* Selection=S->Selected;S->Selected=NULL;
    S->ReadyHead[0]=S->ReadyHead[1]=S->ReadyTail[0]=S->ReadyTail[1]=NULL;
    for(xmqttsessionpacket* P=S->Head;P;P=P->Next)P->ReadyKind=0;
    bool Retain=S->Config.Version==XMQTT_VERSION_5?Expiry!=0:S->Persistent;
    xmqttsessionpacket* Drop=NULL;
    if(!Retain)Drop=xmqttSessionClearLocked(S);
    else {S->ClosedAt=Now;S->Expiry=Expiry;S->InfiniteExpiry=S->Config.Version==XMQTT_VERSION_311 || Expiry==UINT32_MAX;S->ExpiresAt=S->InfiniteExpiry?INFINITY:Now+(double)Expiry;
        if(S->Prepared){for(uint32 Id=1;Id<=65535;++Id)if(xmqttSessionReceiveState(S,(uint16)Id)==1){xmqttSessionReceiveSet(S,(uint16)Id,0);--S->RxCount;}S->Prepared=0;}}
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(Selection);xmqttSessionListRelease(Drop);return XMQTT_SESSION_DONE;
}
XMQTT_API xmqttsessionstatus xmqttSessionExpire(xmqttsession* S,double Now)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,NULL)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}
    if(S->Connected || !xmqttSessionTime(S,Now)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot expire live session or reverse time");}
    S->LastNow=Now;bool Expired=S->HasState && !xmqttSessionAvailableAt(S,Now);xmqttsessionpacket* Drop=Expired?xmqttSessionClearLocked(S):NULL;
    xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return Expired?XMQTT_SESSION_DONE:XMQTT_SESSION_EMPTY;
}
XMQTT_API xmqttsessionstatus xmqttSessionReset(xmqttsession* S)
{
    if(!S)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,NULL)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}if(S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot reset live session");}
    xmqttsessionpacket* Drop=xmqttSessionClearLocked(S);xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);return XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionPublishOwned(xmqttsession* S,const void* Owner,const xmqttpacket* Publish,const xmqttsessionpacket** Out,bool Pending)
{
    if(!S || !xmqttSessionSpan(Publish,sizeof(*Publish)) || !xmqttSessionOutput(S,Out,sizeof(*Out)) || Publish->Type!=XMQTT_PACKET_PUBLISH || (Publish->Flags&8u) || ((Publish->Flags>>1)&3u)==0 || ((Publish->Flags>>1)&3u)>2 || !Publish->Body.Publish.Topic.Size)
        return xmqttSessionFailure(XMQTT_SESSION_ERROR,"session publish requires new QoS1/2 and full logical topic");
    xmqttcodecconfig Codec=*xmqttSessionCodec(S,Owner);if(Owner)Codec.MaxPacketBytes=XMQTT_PACKET_MAX_BYTES;
    xmqttpacket P=*Publish;if(!P.Identifier)P.Identifier=1;size_t Capacity;
    if(!(S->Config.Version==XMQTT_VERSION_5?xmqttPacketSize5(&P,XMQTT_CLIENT_TO_SERVER,&Codec,&Capacity):xmqttPacketSize311(&P,XMQTT_CLIENT_TO_SERVER,&Codec,&Capacity)))return XMQTT_SESSION_ERROR;
    if(!xmqttSessionPacketOutput(Out,sizeof(*Out),Publish))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"overlapping packet output");
    size_t Count=P.Properties.Count;
    if(Count>(SIZE_MAX-sizeof(xmqttsessionpacket))/sizeof(xmqttproperty) || Capacity>SIZE_MAX-sizeof(xmqttsessionpacket)-Count*sizeof(xmqttproperty))return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session packet size overflow");
    size_t Allocation=sizeof(xmqttsessionpacket)+Count*sizeof(xmqttproperty)+Capacity;
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}
    if(!S->Connected && !Pending){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot admit packet offline");}
    if(S->Objects>=S->Config.MaxOutgoing || Allocation>S->Config.MaxBytes-S->LiveBytes){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}
    uint16 Id=Publish->Identifier;
    if(Id && (S->Ids[Id>>3]&(1u<<(Id&7u)))){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}
    if(!Id){Id=S->Next;if(S->Ids[Id>>3]&(1u<<(Id&7u)))Id=xmqttSessionIdFind(S,NULL,Id);
        if(!Id){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_LIMIT;}}
    if(Pending){
        /* Reserve bounded storage/id ownership, then encode outside both the
         * Session gate and the Client admission gate. No protocol link exists. */
        S->Ids[Id>>3]|=(unsigned char)(1u<<(Id&7u));S->Next=Id==65535?1:(uint16)(Id+1u);++S->Objects;S->LiveBytes+=Allocation;
        (void)xrtAtomic32FetchAdd(&S->Refs,1,XMEMORY_RELAXED);xrtMutexUnlock(&S->Gate);
    }
    xmqttsessionpacket* R=xrtMalloc(Allocation);if(!R){if(Pending)xmqttSessionReservationDrop(S,Id,Allocation);else xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
    memset(R,0,sizeof(*R));R->Owner=S;R->Identifier=P.Identifier=Id;R->Qos=(P.Flags>>1)&3u;R->Allocation=Allocation;R->Active=!Pending;R->Stage=XMQTT_SESSION_PUBLISH;
    size_t Properties=0;for(size_t I=0;I<Count;++I)if(P.Properties.Data[I].Id!=XMQTT_PROPERTY_TOPIC_ALIAS)R->Properties[Properties++]=P.Properties.Data[I];
    P.Properties=(xmqttpropertylist){R->Properties,Properties};unsigned char* Wire=(unsigned char*)(R->Properties+Count);size_t Bytes=0;
    bool Written=S->Config.Version==XMQTT_VERSION_5?xmqttPacketWrite5(Wire,Capacity,&P,XMQTT_CLIENT_TO_SERVER,&Codec,&Bytes):xmqttPacketWrite311(Wire,Capacity,&P,XMQTT_CLIENT_TO_SERVER,&Codec,&Bytes);
    xmqttcodecbuffers Buffers={R->Properties,Count,NULL,0};
    xmqttcodecstatus Read=Written?(S->Config.Version==XMQTT_VERSION_5?xmqttPacketRead5((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,&Codec,&Buffers,&R->Packet,NULL):xmqttPacketRead311((xbytesview){Wire,Bytes},XMQTT_CLIENT_TO_SERVER,&Codec,&Buffers,&R->Packet,NULL)):XMQTT_CODEC_LOCAL_ERROR;
    if(Read!=XMQTT_CODEC_DONE){xrtFree(R);if(Pending)xmqttSessionReservationDrop(S,Id,Allocation);else xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_ERROR;}
    if(!Pending && ((!Owner && Bytes>S->Peer.SendPacketBytes) || R->Qos>S->Peer.MaximumQos || ((P.Flags&1u) && !S->Peer.RetainAvailable))){
        xrtFree(R);xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"publish exceeds negotiated peer capability");}
    R->Bytes=Bytes;R->Duplicate=(unsigned char)(Wire[0]|8u);R->Release[0]=0x62;R->Release[1]=2;R->Release[2]=(unsigned char)(Id>>8);R->Release[3]=(unsigned char)Id;
    if(!Pending){R->Previous=S->Tail;if(S->Tail)S->Tail->Next=R;else S->Head=R;S->Tail=R;
        size_t Bucket=xmqttSessionHash(S,Id);R->HashNext=S->Hash[Bucket];S->Hash[Bucket]=R;++S->Active;xmqttSessionReadyAdd(S,R);}
    if(!Pending){S->Ids[Id>>3]|=(unsigned char)(1u<<(Id&7u));S->Next=Id==65535?1:(uint16)(Id+1u);++S->Objects;S->LiveBytes+=Allocation;
        (void)xrtAtomic32FetchAdd(&S->Refs,1,XMEMORY_RELAXED);}
    xrtAtomic32Init(&R->Refs,Pending?1:2);
    if(!Pending)xrtMutexUnlock(&S->Gate);
    *Out=R;return XMQTT_SESSION_DONE;
}
static xmqttsessionframe xmqttSessionFrame(xmqttsession* S,xmqttsessionpacket* P)
{
    xmqttsessionframe F={0};F.Size=sizeof(F);F.Packet=P;F.Generation=S->Generation;F.Selection=S->Selection;F.Type=(xmqttpackettype)P->SelectedType;F.Identifier=P->Identifier;
    if(F.Type==XMQTT_PACKET_PUBREL)F.Prefix=(xbytesview){P->Release,4};
    else {F.Prefix=(xbytesview){P->Retry?&P->Duplicate:P->Packet.Wire.Data,1};F.Tail=(xbytesview){P->Packet.Wire.Data+1,P->Bytes-1};}
    return F;
}
static xmqttsessionstatus xmqttSessionNextOwned(xmqttsession* S,const void* Owner,const xmqttsessionpacket* Wanted,xmqttsessionframe* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out)))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session frame output");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot select frame offline");}
    xmqttsessionpacket* P=Wanted?(xmqttsessionpacket*)Wanted:S->ReadyHead[1]?S->ReadyHead[1]:S->Quota?S->ReadyHead[0]:NULL;
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
    if(Owner && Wanted && P->Owner==S && !P->Active && P->Stage==XMQTT_SESSION_PUBLISH && !P->Attempted){
        if(!S->Connected || S->Selected || !S->Quota){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_EMPTY;}
        /* An unlinked snapshot is private to exactly one request, has never
         * been selected and cannot have a public/native packet loan. */
        if(xrtAtomic32FetchAdd(&P->Refs,1,XMEMORY_RELAXED)!=1)abort();
        P->Active=true;P->Previous=S->Tail;if(S->Tail)S->Tail->Next=P;else S->Head=P;S->Tail=P;
        size_t Bucket=xmqttSessionHash(S,P->Identifier);P->HashNext=S->Hash[Bucket];S->Hash[Bucket]=P;++S->Active;xmqttSessionReadyAdd(S,P);
    }
#endif
    if(Wanted && (P->Owner!=S || !P->Active || !P->ReadyKind || (P->Stage!=XMQTT_SESSION_PUBREL && !S->Quota)))P=NULL;
    if(S->Selected || !P){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_EMPTY;}
    if(S->Selection==UINT64_MAX){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session selection sequence exhausted");}
    if(!xmqttSessionSeparate(Out,sizeof(*Out),P,P->Allocation)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"frame output overlaps owned packet");}
    bool Release=P->Stage==XMQTT_SESSION_PUBREL;
    if((!Owner && (Release?4:P->Bytes)>S->Peer.SendPacketBytes) || (!Release && (P->Qos>S->Peer.MaximumQos || ((P->Packet.Flags&1u) && !S->Peer.RetainAvailable)))){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"resumed packet exceeds new peer capability");}
    if(!xmqttSessionPacketRef(P)){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_RESTRICTED,"session packet reference limit");}
    xmqttSessionReadyRemove(S,P);P->Need=false;P->SelectedType=Release?XMQTT_PACKET_PUBREL:XMQTT_PACKET_PUBLISH;P->Retry=P->Attempted;S->Selected=P;++S->Selection;
    if(!Release){--S->Quota;P->Attempted=true;}
    *Out=xmqttSessionFrame(S,P);
    xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionSentOwned(xmqttsession* S,const void* Owner,const xmqttsessionframe* F)
{
    if(!S || !xmqttSessionSpan(F,sizeof(*F)) || F->Size!=sizeof(*F))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session frame token");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}xmqttsessionpacket* P=S->Selected;
    if(!S->Connected || !P || F->Packet!=P || F->Generation!=S->Generation || F->Selection!=S->Selection){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"stale session frame selection");}
    xmqttsessionframe Expected=xmqttSessionFrame(S,P);
    if(F->Type!=Expected.Type || F->Identifier!=Expected.Identifier || F->Prefix.Data!=Expected.Prefix.Data || F->Prefix.Size!=Expected.Prefix.Size || F->Tail.Data!=Expected.Tail.Data || F->Tail.Size!=Expected.Tail.Size){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_ERROR,"altered session frame token");}
    S->Selected=NULL;if(F->Type==XMQTT_PACKET_PUBLISH)P->Sent=true;else P->ReleaseSent=true;
    if(P->Active && P->Need)xmqttSessionReadyAdd(S,P);
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(P);return XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionAcknowledgeOwned(xmqttsession* S,const void* Owner,const xmqttpacket* Ack)
{
    if(!S || !xmqttSessionSpan(Ack,sizeof(*Ack)) || (Ack->Type!=XMQTT_PACKET_PUBACK && Ack->Type!=XMQTT_PACKET_PUBREC && Ack->Type!=XMQTT_PACKET_PUBCOMP))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session acknowledgement");
    size_t Bytes;if(!xmqttSessionSizePacket(S,Owner,Ack,XMQTT_SERVER_TO_CLIENT,&Bytes))return XMQTT_SESSION_ERROR;
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"acknowledgement while offline");}
    xmqttsessionpacket* P=xmqttSessionFind(S,Ack->Identifier);
    if(Ack->Type==XMQTT_PACKET_PUBCOMP && (!P || P->Qos!=2 || P->Stage!=XMQTT_SESSION_PUBREL || !P->ReleaseSent)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DUPLICATE;}
    if(!P || !P->Sent || (Ack->Type==XMQTT_PACKET_PUBACK && P->Qos!=1) || (Ack->Type==XMQTT_PACKET_PUBREC && P->Qos!=2) || (Ack->Type==XMQTT_PACKET_PUBCOMP && !P->ReleaseSent)){
        xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_PROTOCOL_ERROR,"acknowledgement does not match a sent flow");}
    if(Ack->Type==XMQTT_PACKET_PUBREC && P->Stage==XMQTT_SESSION_PUBREL){
        if(Ack->Reason>=0x80){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_PROTOCOL_ERROR,"negative PUBREC after accepted PUBREC");}
        if(!Owner || S->Selected!=P || P->SelectedType!=XMQTT_PACKET_PUBREL)xmqttSessionReadyAdd(S,P);
        xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DUPLICATE;
    }
    if(Ack->Type==XMQTT_PACKET_PUBREC){P->PubrecReceived=true;P->PubrecReason=Ack->Reason;
        if(Ack->Reason<0x80){xmqttSessionReadyRemove(S,P);P->Stage=XMQTT_SESSION_PUBREL;P->ReleaseSent=false;xmqttSessionReadyAdd(S,P);xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_DONE;}}
    P->Reason=Ack->Reason;P->Stage=XMQTT_SESSION_COMPLETE;xmqttSessionUnlink(S,P);
    if(S->Quota<S->InitialQuota)++S->Quota;
    xrtMutexUnlock(&S->Gate);xmqttSessionPacketRelease(P);
    return Ack->Reason>=0x80?XMQTT_SESSION_REJECTED:XMQTT_SESSION_DONE;
}
static xmqttsessionstatus xmqttSessionReceiveChange(xmqttsession* S,const void* Owner,uint16 Id,unsigned Operation,uint8* Reason)
{
    if(!S || !Id || (Reason && !xmqttSessionOutput(S,Reason,sizeof(*Reason))))return xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid incoming session identifier/output");
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,Owner)){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_STATE_ERROR;}if(!S->Connected){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"incoming state while offline");}
    unsigned State=xmqttSessionReceiveState(S,Id);xmqttsessionstatus Result=XMQTT_SESSION_DONE;
    if(!Operation){
        if(State==1)Result=XMQTT_SESSION_STATE_ERROR;
        else if(State)Result=XMQTT_SESSION_DUPLICATE;
        else if(S->RxCount==S->Config.MaxIncoming)Result=XMQTT_SESSION_LIMIT;
        else {xmqttSessionReceiveSet(S,Id,1);++S->RxCount;++S->Prepared;}
    }else if(Operation==1){if(State!=1)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,2);--S->Prepared;}}
    else if(Operation==2){if(State!=1)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,0);--S->RxCount;--S->Prepared;}}
    else if(Operation==3){
        if(State==1)Result=XMQTT_SESSION_STATE_ERROR;
        else if(!State){*Reason=S->Config.Version==XMQTT_VERSION_5?0x92:0;Result=XMQTT_SESSION_EMPTY;}
        else {xmqttSessionReceiveSet(S,Id,3);*Reason=0;}
    }else {if(State!=3)Result=XMQTT_SESSION_STATE_ERROR;else {xmqttSessionReceiveSet(S,Id,0);--S->RxCount;}}
    xrtMutexUnlock(&S->Gate);return Result==XMQTT_SESSION_STATE_ERROR?xmqttSessionFailure(Result,"invalid incoming session transition"):Result;
}
XMQTT_API xmqttsessionstatus xmqttSessionReceivePrepare(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,NULL,Id,0,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveCommit(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,NULL,Id,1,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveAbort(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,NULL,Id,2,NULL);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveRelease(xmqttsession* S,uint16 Id,uint8* Reason)
{if(!Reason)return xmqttSessionFailure(XMQTT_SESSION_ERROR,"PUBCOMP reason output is NULL");return xmqttSessionReceiveChange(S,NULL,Id,3,Reason);}
XMQTT_API xmqttsessionstatus xmqttSessionReceiveComplete(xmqttsession* S,uint16 Id)
{return xmqttSessionReceiveChange(S,NULL,Id,4,NULL);}
XMQTT_API bool xmqttSessionInfo(xmqttsession* S,xmqttsessioninfo* Out)
{
    if(!S || !xmqttSessionOutput(S,Out,sizeof(*Out))){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session info output");return false;}
    xrtMutexLock(&S->Gate);xmqttsessioninfo I={0};I.Size=sizeof(I);I.Version=S->Config.Version;I.Connected=S->Connected;I.HasState=S->HasState;I.ActiveOutgoing=S->Active;I.Objects=S->Objects;I.LiveBytes=S->LiveBytes;I.BaseBytes=S->BaseBytes;
    I.Incoming=S->RxCount-S->Prepared;I.Prepared=S->Prepared;I.SendQuota=S->Quota;I.InitialSendQuota=S->InitialQuota;I.Generation=S->Generation;I.ExpiresAt=S->ExpiresAt;*Out=I;xrtMutexUnlock(&S->Gate);return true;
}
XMQTT_API const xmqttpacket* xmqttSessionPacket(const xmqttsessionpacket* P)
{return P?&P->Packet:NULL;}
XMQTT_API bool xmqttSessionPacketInfo(const xmqttsessionpacket* P,xmqttsessionpacketinfo* Out)
{
    if(!P || !xmqttSessionOutput(P->Owner,Out,sizeof(*Out)) || !xmqttSessionSeparate(Out,sizeof(*Out),P,P->Allocation)){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session packet info output");return false;}
    xmqttsession* S=P->Owner;xrtMutexLock(&S->Gate);xmqttsessionpacketinfo I={0};I.Size=sizeof(I);I.Identifier=P->Identifier;I.Qos=P->Qos;I.Reason=P->Reason;I.PubrecReason=P->PubrecReason;I.PubrecReceived=P->PubrecReceived;I.Active=P->Active;I.Sent=P->Sent;I.Stage=P->Stage;I.WireBytes=P->Bytes;*Out=I;xrtMutexUnlock(&S->Gate);return true;
}
XMQTT_API bool xmqttSessionDestroy(xmqttsession* S)
{
    if(!S){xmqttSessionFailure(XMQTT_SESSION_ERROR,"session is NULL");return false;}
    xrtMutexLock(&S->Gate);if(!xmqttSessionOwner(S,NULL)){xrtMutexUnlock(&S->Gate);return false;}if(S->Connected){xrtMutexUnlock(&S->Gate);xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot destroy connected session");return false;}
    xmqttsessionpacket* Drop=xmqttSessionClearLocked(S);xrtMutexUnlock(&S->Gate);xmqttSessionListRelease(Drop);xmqttSessionDrop(S);return true;
}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL bool xmqttInternalSessionClaim(xmqttsession* S,const void* Owner,xmqttversion Version,
    const xmqttpacket* Connect,const xmqttcodecconfig* Codec,double Now,size_t* Count,size_t* Bytes)
{
    if(!S || !Owner || !Connect || !Codec || !Count || !Bytes){xmqttSessionFailure(XMQTT_SESSION_ERROR,"invalid session driver claim");return false;}
    xrtMutexLock(&S->Gate);
    if(S->Driver || S->Connected || !xmqttSessionTime(S,Now)){xrtMutexUnlock(&S->Gate);xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session driver is unavailable or clock differs");return false;}
    const xmqttcodecconfig* K=&S->Config.Codec;
    if(Version!=S->Config.Version || Connect->Body.Connect.ClientId.Size!=S->Config.ClientId.Size ||
       memcmp(Connect->Body.Connect.ClientId.Data,S->Config.ClientId.Data,S->Config.ClientId.Size) ||
       Codec->MaxPayloadBytes>K->MaxPayloadBytes ||
       Codec->MaxProperties>K->MaxProperties || Codec->MaxPropertyBytes>K->MaxPropertyBytes || Codec->MaxTopics>K->MaxTopics || Codec->Utf8Flags!=K->Utf8Flags){
        xrtMutexUnlock(&S->Gate);xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"session identity, version or codec capacity differs");return false;}
    size_t N=0,B=0;
    if(!Connect->Body.Connect.CleanStart && xmqttSessionAvailableAt(S,Now))for(xmqttsessionpacket* P=S->Head;P;P=P->Next){++N;if(P->Stage==XMQTT_SESSION_PUBLISH)B+=P->Bytes;}
    S->Driver=Owner;S->DriverCodec=Codec;*Count=N;*Bytes=B;xrtMutexUnlock(&S->Gate);return true;
}
XMQTT_INTERNAL void xmqttInternalSessionUnclaim(xmqttsession* S,const void* Owner)
{
    xrtMutexLock(&S->Gate);if(S->Driver!=Owner || S->Connected)abort();S->Driver=NULL;S->DriverCodec=NULL;xrtMutexUnlock(&S->Gate);
}
XMQTT_INTERNAL uint16 xmqttInternalSessionIdentifier(xmqttsession* S,const void* Owner,const unsigned char* Used,uint16 Requested,uint16 Previous)
{
    xrtMutexLock(&S->Gate);if(S->Driver!=Owner)abort();uint16 Id=Requested;
    if(Id){if((Used[Id>>3]&(1u<<(Id&7u))) || (S->Ids && (S->Ids[Id>>3]&(1u<<(Id&7u)))))Id=0;}
    else {Id=Previous==65535?1:(uint16)(Previous+1u);
        if((Used[Id>>3]&(1u<<(Id&7u))) || (S->Ids && (S->Ids[Id>>3]&(1u<<(Id&7u)))))Id=xmqttSessionIdFind(S,Used,Id);}
    xrtMutexUnlock(&S->Gate);return Id;
}
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionVisit(xmqttsession* S,const void* Owner,const xmqttsessionpacket* Previous,const xmqttsessionpacket** Out)
{
    xrtMutexLock(&S->Gate);if(S->Driver!=Owner)abort();
    xmqttsessionpacket* P=Previous?Previous->Next:S->Head;
    if(!P){xrtMutexUnlock(&S->Gate);return XMQTT_SESSION_EMPTY;}
    /* The sole driver keeps the active list unchanged across this call. Public
     * mutators are refused, and an active reference protects P from readers. */
    xrtMutexUnlock(&S->Gate);if(!xmqttSessionPacketRetain(P))return XMQTT_SESSION_RESTRICTED;
    *Out=P;return XMQTT_SESSION_DONE;
}
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionWithdraw(xmqttsession* S,const void* Owner,const xmqttsessionpacket* Packet)
{
    xmqttsessionpacket* P=(xmqttsessionpacket*)Packet;
    xrtMutexLock(&S->Gate);
    if(S->Driver!=Owner || !P || P->Owner!=S || !P->Active || P->Sent || P->Stage!=XMQTT_SESSION_PUBLISH){xrtMutexUnlock(&S->Gate);return xmqttSessionFailure(XMQTT_SESSION_STATE_ERROR,"cannot withdraw an observed protocol obligation");}
    bool Selected=S->Selected==P;
    if(Selected){S->Selected=NULL;if(S->Quota<S->InitialQuota)++S->Quota;}
    P->Stage=XMQTT_SESSION_DISCARDED;xmqttSessionUnlink(S,P);xrtMutexUnlock(&S->Gate);
    if(Selected)xmqttSessionPacketRelease(P);
    xmqttSessionPacketRelease(P);return XMQTT_SESSION_DONE;
}
XMQTT_INTERNAL unsigned xmqttInternalSessionReceiveState(xmqttsession* S,const void* Owner,uint16 Id)
{
    xrtMutexLock(&S->Gate);if(S->Driver!=Owner)abort();unsigned State=xmqttSessionReceiveState(S,Id);xrtMutexUnlock(&S->Gate);return State;
}
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionReceive(xmqttsession* S,const void* Owner,uint16 Id,unsigned Operation,uint8* Reason)
{return xmqttSessionReceiveChange(S,Owner,Id,Operation,Reason);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionOpen(xmqttsession* S,const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Out)
{return xmqttSessionOpenOwned(S,NULL,Connect,Connack,Now,Out);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionOpen(xmqttsession* S,const void* Owner,const xmqttpacket* Connect,const xmqttpacket* Connack,double Now,xmqttnegotiated* Out)
{return xmqttSessionOpenOwned(S,Owner,Connect,Connack,Now,Out);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionEnd(xmqttsession* S,const xmqttpacket* Disconnect,double Now)
{return xmqttSessionEndOwned(S,NULL,Disconnect,Now);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionEnd(xmqttsession* S,const void* Owner,const xmqttpacket* Disconnect,double Now)
{return xmqttSessionEndOwned(S,Owner,Disconnect,Now);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionPublish(xmqttsession* S,const xmqttpacket* Publish,const xmqttsessionpacket** Out)
{return xmqttSessionPublishOwned(S,NULL,Publish,Out,false);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionPublish(xmqttsession* S,const void* Owner,const xmqttpacket* Publish,const xmqttsessionpacket** Out)
{return xmqttSessionPublishOwned(S,Owner,Publish,Out,false);}
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSnapshot(xmqttsession* S,const void* Owner,const xmqttpacket* Publish,const xmqttsessionpacket** Out)
{return xmqttSessionPublishOwned(S,Owner,Publish,Out,true);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionSent(xmqttsession* S,const xmqttsessionframe* F)
{return xmqttSessionSentOwned(S,NULL,F);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSent(xmqttsession* S,const void* Owner,const xmqttsessionframe* F)
{return xmqttSessionSentOwned(S,Owner,F);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionAcknowledge(xmqttsession* S,const xmqttpacket* Ack)
{return xmqttSessionAcknowledgeOwned(S,NULL,Ack);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionAcknowledge(xmqttsession* S,const void* Owner,const xmqttpacket* Ack)
{return xmqttSessionAcknowledgeOwned(S,Owner,Ack);}
#endif
XMQTT_API xmqttsessionstatus xmqttSessionNext(xmqttsession* S,xmqttsessionframe* Out)
{return xmqttSessionNextOwned(S,NULL,NULL,Out);}
#if defined(XMQTT_FEATURE_XMQTT_CLIENT_SESSION)
XMQTT_INTERNAL xmqttsessionstatus xmqttInternalSessionSelect(xmqttsession* S,const void* Owner,const xmqttsessionpacket* P,xmqttsessionframe* Out)
{return xmqttSessionNextOwned(S,Owner,P,Out);}
#endif
#endif
#endif

#endif
