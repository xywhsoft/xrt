#ifndef XMQTT_PROPERTIES_H
#define XMQTT_PROPERTIES_H
#include <xmqtt/features.h>
#include <xmqtt/export.h>
#include <xmqtt/wire.h>

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
