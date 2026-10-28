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
#error "xredis requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XREDIS_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XREDIS_IMPLEMENTATION) && \
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
#ifndef XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER_H
#define XREDIS_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xredis/include/xredis/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XREDIS_FEATURES_H
#define XREDIS_FEATURES_H

/* xredis 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS)
#ifndef XREDIS_FEATURE_XREDIS
#define XREDIS_FEATURE_XREDIS
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_READER
#define XREDIS_MODULE_XREDIS_READER
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_client 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_CLIENT)
#ifndef XREDIS_FEATURE_XREDIS_CLIENT
#define XREDIS_FEATURE_XREDIS_CLIENT
#endif
#ifndef XREDIS_MODULE_XREDIS_COMMAND
#define XREDIS_MODULE_XREDIS_COMMAND
#endif
#ifndef XREDIS_MODULE_XREDIS_REPLY
#define XREDIS_MODULE_XREDIS_REPLY
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_NUMBER_INTEGER
#endif
#endif

/* xredis_reply 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_REPLY)
#ifndef XREDIS_FEATURE_XREDIS_REPLY
#define XREDIS_FEATURE_XREDIS_REPLY
#endif
#ifndef XREDIS_MODULE_XREDIS_READER
#define XREDIS_MODULE_XREDIS_READER
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_NUMBER_FLOAT
#endif
#endif

/* xredis_reader 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_READER)
#ifndef XREDIS_FEATURE_XREDIS_READER
#define XREDIS_FEATURE_XREDIS_READER
#endif
#endif

/* xredis_command 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_COMMAND)
#ifndef XREDIS_FEATURE_XREDIS_COMMAND
#define XREDIS_FEATURE_XREDIS_COMMAND
#endif
#endif

#endif /* XREDIS_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/export.h */
/* ========================================================================== */

#ifndef XREDIS_EXPORT_H
#define XREDIS_EXPORT_H


#if defined(_WIN32) || defined(_WIN64)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __declspec(dllexport)
 #elif defined(XREDIS_USE_SHARED)
  #define XREDIS_API __declspec(dllimport)
 #else
  #define XREDIS_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XREDIS_BUILD_SHARED)
  #define XREDIS_API __attribute__((visibility("default")))
 #else
  #define XREDIS_API
 #endif
#else
 #define XREDIS_API
#endif

#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/command.h */
/* ========================================================================== */

#ifndef XREDIS_COMMAND_H
#define XREDIS_COMMAND_H


#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

/* These error codes belong to the xrt.redis domain, not server error replies. */
typedef enum xrediscommanderror {
	XREDIS_COMMAND_ARGUMENT = 1,
	XREDIS_COMMAND_OVERFLOW,
	XREDIS_COMMAND_CAPACITY,
	XREDIS_COMMAND_OVERLAP
} xrediscommanderror;

XRT_EXTERN_C_BEGIN

/* Measure one RESP array of bulk strings, shared by RESP2 and RESP3 commands.
 * Count must be positive; argument zero is the command name. Arguments borrow
 * stable, readable memory for this call. NULL/0 is a valid empty argument;
 * NULL/nonzero is invalid. Embedded NUL and CRLF are ordinary payload bytes.
 * A length/count greater than INT64_MAX or size/address arithmetic overflow is
 * rejected. Required is unchanged on failure and must not alias an input.
 * Success allocates nothing and preserves the ambient XRT error. */
XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required);

/* Write the exact measured wire bytes into caller-owned Output; no trailing NUL.
 * Written is optional. Output and Written must not overlap input descriptors,
 * input payloads, or each other. Only the actual wire span is an output span.
 * All validation precedes the first write: Output and Written are unchanged on
 * failure. Success copies each payload exactly once, allocates nothing, retains
 * nothing and preserves the ambient error. The caller supplies actual readable/
 * writable objects; address-wrap checks do not validate arbitrary addresses.
 * This contiguous convenience encoder is not a zero-copy send API. */
XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/reader.h */
/* ========================================================================== */

#ifndef XREDIS_READER_H
#define XREDIS_READER_H


#if defined(XREDIS_FEATURE_XREDIS_READER)

typedef struct xredisreader xredisreader;

typedef enum xredisprotocol {
	XREDIS_RESP2 = 2,
	XREDIS_RESP3 = 3
} xredisprotocol;

typedef enum xredisreplytype {
	XREDIS_REPLY_SIMPLE_STRING = 1,
	XREDIS_REPLY_SIMPLE_ERROR,
	XREDIS_REPLY_INTEGER,
	XREDIS_REPLY_BULK_STRING,
	XREDIS_REPLY_ARRAY,
	XREDIS_REPLY_NULL,
	XREDIS_REPLY_BOOLEAN,
	XREDIS_REPLY_DOUBLE,
	XREDIS_REPLY_BIG_NUMBER,
	XREDIS_REPLY_BULK_ERROR,
	XREDIS_REPLY_VERBATIM,
	XREDIS_REPLY_MAP,
	XREDIS_REPLY_ATTRIBUTE,
	XREDIS_REPLY_SET,
	XREDIS_REPLY_PUSH
} xredisreplytype;

#define XREDIS_REPLY_NULL_VALUE 0x01u
#define XREDIS_REPLY_STREAMED 0x02u
#define XREDIS_LENGTH_UNKNOWN UINT64_MAX

/* Optional compatibility with pre-7.2 libc NaN forms: case insensitive,
 * signed nan and nan([A-Za-z0-9_]*). The default follows normalized RESP3. */
#define XREDIS_READER_LEGACY_NAN 0x01u

typedef enum xredisreadkind {
	XREDIS_READ_BEGIN = 1,
	XREDIS_READ_DATA,
	XREDIS_READ_END
} xredisreadkind;

/* Depth is zero for the root. Length is payload bytes for scalars and logical
 * elements/pairs for aggregates (maps/attributes count pairs). Scalar BEGIN
 * lengths are unknown, except null; bulk/aggregate BEGIN has the declared size
 * or UNKNOWN for streaming. END always has the actual size, null has size 0.
 * DATA contains nonempty raw wire payload, including numeric text and the four
 * verbatim format bytes; it excludes framing/chunk headers. Integer/Boolean
 * are meaningful only on END for the corresponding types. Data and the event
 * itself are borrowed ONLY until this callback returns. No input is retained. */
typedef struct xredisreadevent {
	xredisreadkind Kind;
	xredisreplytype Type;
	uint32 Flags;
	size_t Depth;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
} xredisreadevent;

typedef enum xredisreadaction {
	XREDIS_READ_NEXT = 0,
	XREDIS_READ_PAUSE,
	XREDIS_READ_FAIL
} xredisreadaction;

typedef xredisreadaction (*xredisreadproc)(const xredisreadevent* Event, ptr User);

typedef enum xredisreadererror {
	XREDIS_READER_OK = 0,
	XREDIS_READER_ARGUMENT = 1001,
	XREDIS_READER_STATE,
	XREDIS_READER_PROTOCOL,
	XREDIS_READER_LIMIT,
	XREDIS_READER_CALLBACK,
	XREDIS_READER_TRUNCATED
} xredisreadererror;

typedef enum xredisreadstatus {
	XREDIS_READER_ERROR = -1,
	XREDIS_READER_MORE = 0,
	XREDIS_READER_READY,
	XREDIS_READER_PAUSED,
	XREDIS_READER_YIELD
} xredisreadstatus;

/* Initialize with ConfigInit, then change explicit limits. Zero is a real
 * limit, not unlimited. MaxFrameBytes, MaxValues and MaxFeedBytes must be >0;
 * uint64 limits must be <=INT64_MAX. MaxDepth counts open aggregates, not
 * scalar nesting. MaxLineBytes bounds BOTH scalar lines and numeric headers.
 * MaxContainerItems counts direct elements/pairs, excluding attributes.
 * MaxValues includes every value and attribute in the semantic frame. */
typedef struct xredisreaderconfig {
	size_t Size;
	xredisprotocol Protocol;
	uint32 Flags;
	size_t MaxDepth;
	uint64 MaxFrameBytes;
	uint64 MaxBulkBytes;
	uint64 MaxLineBytes;
	uint64 MaxContainerItems;
	uint64 MaxValues;
	size_t MaxFeedBytes;
} xredisreaderconfig;

typedef struct xredisreaderinfo {
	size_t Size;
	uint64 StreamBytes;
	uint64 FrameBytes;
	uint64 FrameValues;
	uint64 LastFrameBytes;
	uint64 LastFrameValues;
	uint64 CompletedFrames;
	size_t OpenContainers;
	bool Paused;
	xredisreadererror Error;
	/* Absolute stream position at detection (next unconsumed byte), not always
	 * the start of an invalid token. Reset restarts all counters at zero. */
	uint64 ErrorOffset;
} xredisreaderinfo;

XRT_EXTERN_C_BEGIN

/* Defaults: RESP3, normalized NaN, depth 64, frame 64 MiB, bulk 16 MiB,
 * line 64 KiB, container/value counts 1 million, Feed budget 64 KiB. */
XREDIS_API bool xredisReaderConfigInit(xredisreaderconfig* Config);

/* One bounded XRT allocation including the depth stack. NULL Config selects
 * defaults. Callback is required; User is borrowed until Destroy. Config is
 * copied. Result is unchanged on failure and must not overlap Config. */
XREDIS_API bool xredisReaderCreate(const xredisreaderconfig* Config,
	xredisreadproc Callback, ptr User, xredisreader** Result);

/* Single-owner, thread-confined reader; callers serialize all access. Feed,
 * Reset, Finish and Destroy cannot reenter a callback; Info can. Callback
 * reentry failure does not poison the outer parse or free the reader.
 * Destroy(NULL) succeeds; valid Destroy releases its one allocation. */
XREDIS_API bool xredisReaderDestroy(xredisreader* Reader);
XREDIS_API bool xredisReaderReset(xredisreader* Reader);
XREDIS_API bool xredisReaderInfo(const xredisreader* Reader, xredisreaderinfo* Info);

/* Consume at most MaxFeedBytes and stop after ONE complete semantic frame.
 * READY includes any preceding attributes and exactly one root value/push.
 * Attributes do not consume parent items; pushes must be top-level, nonempty
 * and start with a non-null simple/bulk string. RESP2 rejects RESP3 types.
 * MORE needs more bytes; YIELD leaves bytes in Input after the work budget;
 * PAUSED means the callback requested pause AFTER the event was delivered.
 * Resume with the unconsumed suffix, possibly empty: pending events and READY
 * may occur with Consumed==0. Events are never repeated after pause.
 * DATA may precede a later malformed terminator: discard partial frame state
 * on ERROR, and publish an assembled result only after READY. Protocol, limit,
 * callback and truncated-EOF failures are terminal until Reset. Invalid API
 * arguments leave reader state and Consumed unchanged. Other results set
 * Consumed, including ERROR. Consumed must not overlap Input or reader storage.
 * No successful Feed/Reset/Info/Finish allocates, copies payload or clears the
 * ambient error (callbacks can change it). Error diagnostics are cold-path XRT
 * allocations with XERR_MEMORY fallback. Actual readable objects are required;
 * address-span checks cannot validate arbitrary addresses. */
XREDIS_API xredisreadstatus xredisReaderFeed(xredisreader* Reader,
	xbytesview Input, size_t* Consumed);

/* End-of-stream check. Succeeds only at a frame boundary. Partial input and
 * orphan attributes fail terminally. A paused reader must first be resumed. */
XREDIS_API bool xredisReaderFinish(xredisreader* Reader);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/reply.h */
/* ========================================================================== */

#ifndef XREDIS_REPLY_H
#define XREDIS_REPLY_H


#if defined(XREDIS_FEATURE_XREDIS_REPLY) && \
	(!defined(XREDIS_FEATURE_XREDIS_READER) || !defined(XRT_FEATURE_ATOMIC) || !defined(XRT_FEATURE_NUMBER_FLOAT))
#error "XREDIS_FEATURE_XREDIS_REPLY requires reader, atomic and number_float features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreply xredisreply;
typedef struct xredisreplyreader xredisreplyreader;

#define XREDIS_REPLY_NONE SIZE_MAX

typedef enum xredisreplyerror {
	XREDIS_REPLY_OK = 0,
	XREDIS_REPLY_ARGUMENT = 2001,
	XREDIS_REPLY_STATE,
	XREDIS_REPLY_PROTOCOL,
	XREDIS_REPLY_LIMIT,
	XREDIS_REPLY_MEMORY,
	XREDIS_REPLY_TRUNCATED
} xredisreplyerror;

typedef enum xredisreplystatus {
	XREDIS_REPLY_ERROR = -1,
	XREDIS_REPLY_MORE = 0,
	XREDIS_REPLY_READY,
	XREDIS_REPLY_YIELD
} xredisreplystatus;

/* Indices belong to one immutable reply. NONE ends every chain. ChildCount
 * counts actual children (twice Length for maps/attributes); metadata maps do
 * not consume children. Attributes is an ordered chain of attribute nodes,
 * using Next just like the ordinary child chain. Preserve duplicate map keys,
 * wire order, null kinds and streamed flags. Data contains owned raw scalar
 * payload, including numeric lexemes and verbatim fmt:, without framing or
 * a trailing NUL. Integer/Boolean are native values of those types only. */
typedef struct xredisreplyvalue {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	xbytesview Data;
	size_t ChildCount;
	size_t FirstChild;
	size_t Next;
	size_t Attributes;
} xredisreplyvalue;

typedef struct xredisreplyinfo {
	size_t Size;
	size_t Root;
	size_t Nodes;
	size_t PayloadBytes;
	/* Requested XRT bytes, including header/inline storage and unused vector
	 * capacity. Peak also includes old+new buffers during geometric growth.
	 * Allocator headers, size-class rounding and diagnostics are excluded. */
	size_t ReservedBytes;
	size_t PeakBytes;
} xredisreplyinfo;

typedef struct xredisreplyreaderconfig {
	size_t Size;
	xredisreaderconfig Reader;
	/* Per-frame peak requested owned bytes, including temporary growth copies.
	 * Does not include reader/control stacks or already transferred replies.
	 * Zero is invalid. Small limits can reject even an empty scalar. */
	size_t MaxOwnedBytes;
} xredisreplyreaderconfig;

typedef struct xredisreplyreaderinfo {
	size_t Size;
	xredisreaderinfo Reader;
	xredisreplyerror Error;
	size_t PartialNodes;
	size_t PartialPayloadBytes;
	size_t PartialReservedBytes;
	/* Current/most recently completed or failed frame; Reset clears it. */
	size_t PeakOwnedBytes;
} xredisreplyreaderinfo;

XRT_EXTERN_C_BEGIN

/* Default event-reader limits, plus 64 MiB peak owned bytes per frame. */
XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config);

/* Config is copied; NULL selects defaults. Two XRT allocations hold control
 * and bounded depth stacks. Result is unchanged on failure. Creation may
 * report the nested Reader's validation errors. All reader access is confined
 * to one thread. No user callback or input pointer is retained. */
XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config,
	xredisreplyreader** Result);
XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader);
XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info);

/* Consume at most Reader.MaxFeedBytes and publish ONE fully validated semantic
 * frame on READY, transferring one strong reference. Prior attributes belong
 * to that root, including pushes. Result is unchanged on all other outcomes.
 * Resume YIELD with the unconsumed suffix; MORE needs more input. No retained
 * source bytes, per-node allocation, second parse or final flatten/copy.
 * Scalar bytes are copied into owned storage; vector growth can copy them
 * again. One small scalar needs one allocation. All resource/protocol/OOM/EOF
 * errors are terminal until Reset, release partial ownership immediately and
 * keep their typed cause in Info even if diagnostics cannot allocate.
 * Invalid API calls leave state/Consumed/Result unchanged. Otherwise Consumed
 * is always written, including ERROR. All outputs must be disjoint from each
 * other, Input and reader/partial-frame storage. Successful calls preserve
 * the ambient error. Actual readable objects are required, not just spans. */
XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result);
XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader);

/* Immutable replies may be shared across threads after a synchronized handoff.
 * Each user must already own a live reference when retaining/accessing it.
 * Retain uses a bounded 32-bit atomic count; overflow leaves it unchanged.
 * Release(NULL) is allowed. Final release frees all owned storage on its thread.
 * Refcounting does not publish a pointer or make a concurrent final release
 * safe without a reference. Views remain valid until that last release. */
XREDIS_API bool xredisReplyRetain(xredisreply* Reply);
XREDIS_API void xredisReplyRelease(xredisreply* Reply);
XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info);
XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value);

/* Lazy, locale-independent correctly rounded binary64 conversion of a DOUBLE
 * node. Raw text stays lossless; legacy NaN payloads normalize to signed NaN.
 * Finite overflow reports the public XRT number error and preserves Value;
 * it does not retroactively invalidate a syntactically valid wire reply.
 * Successful conversion allocates nothing and preserves the ambient error. */
XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value);

XRT_EXTERN_C_END

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/client.h */
/* ========================================================================== */

#ifndef XREDIS_CLIENT_H
#define XREDIS_CLIENT_H

#if defined(XREDIS_FEATURE_XREDIS_CLIENT) && \
	(!defined(XREDIS_FEATURE_XREDIS_COMMAND) || !defined(XREDIS_FEATURE_XREDIS_REPLY) || \
	 !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_FUTURE) || !defined(XRT_FEATURE_NUMBER_INTEGER))
#error "XREDIS_FEATURE_XREDIS_CLIENT requires command, reply, net_tcp, future and number_integer features"
#endif

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
typedef struct xredisclient xredisclient;

typedef enum xredisclientstate {
	XREDIS_CLIENT_CONNECTING = 0, XREDIS_CLIENT_OPEN, XREDIS_CLIENT_CLOSING, XREDIS_CLIENT_CLOSED
} xredisclientstate;

typedef enum xredisrequeststatus {
	XREDIS_REQUEST_OK = 0, XREDIS_REQUEST_SERVER_ERROR, XREDIS_REQUEST_TIMEOUT,
	XREDIS_REQUEST_CANCELLED, XREDIS_REQUEST_CLOSED, XREDIS_REQUEST_DISCONNECTED,
	XREDIS_REQUEST_PROTOCOL, XREDIS_REQUEST_LIMIT, XREDIS_REQUEST_MEMORY,
	XREDIS_REQUEST_INITIALIZATION
} xredisrequeststatus;

typedef enum xredisdelivery {
	XREDIS_NOT_SENT = 0, XREDIS_MAY_HAVE_EXECUTED, XREDIS_REPLIED
} xredisdelivery;

typedef enum xredisadmission {
	XREDIS_ADMISSION_ERROR = -1, XREDIS_ADMISSION_ACCEPTED = 0,
	XREDIS_ADMISSION_OVERLOADED, XREDIS_ADMISSION_CLOSED
} xredisadmission;

typedef enum xredisclienterror {
	XREDIS_CLIENT_ARGUMENT = 3001, XREDIS_CLIENT_STATE, XREDIS_CLIENT_COMMAND_MODE,
	XREDIS_CLIENT_INITIALIZATION_FAILED
} xredisclienterror;

/* Every accepted call resolves its Future with this immutable typed outcome,
 * including cancellation/timeout/transport failure. Future cancellation asks
 * the producer to stop; XFUTURE_RESOLVED itself does not imply request success.
 * Reply (OK/SERVER_ERROR) and optional explicit Cause are borrowed from that
 * Future. Retain a reply before dropping its Future. Status/Delivery remain
 * exact even if error diagnostics cannot allocate. Delivery states knowledge
 * at logical completion: accepting bytes into TCP is MAY_HAVE_EXECUTED, not
 * proof that the server received/executed them. No operation is auto-replayed. */
typedef struct xredisrequestresult {
	xredisrequeststatus Status;
	xredisdelivery Delivery;
	xredisreply* Reply;
	const xerror* Cause;
} xredisrequestresult;

typedef struct xredisclientconfig {
	size_t Size;
	xnetstreamconfig Stream;
	xredisreplyreaderconfig Replies;
	size_t MaxRequests;
	size_t MaxCommandBytes;
	size_t MaxCommandBytesTotal;
	size_t MaxInFlight;
	size_t MaxPushes;
	size_t MaxPushBytes;
	size_t WorkRequests;
	size_t ReadBytes;
	size_t ReadFrames;
	/* Positive grace after a sent request is locally cancelled/timed out.
	 * Keep its FIFO slot until drained; retire the connection if grace expires. */
	int64 DrainTimeout;
} xredisclientconfig;

typedef enum xredisinitmode {
	XREDIS_INIT_AUTO = 0, XREDIS_INIT_RESP2 = 2, XREDIS_INIT_RESP3 = 3
} xredisinitmode;

#define XREDIS_INIT_AUTH 0x01u
#define XREDIS_INIT_USERNAME 0x02u
#define XREDIS_INIT_SETNAME 0x04u
#define XREDIS_INIT_SELECT 0x08u
#define XREDIS_PROTOCOL_UNKNOWN ((xredisprotocol)0)

typedef enum xredisinitstep {
	XREDIS_INIT_NONE = 0, XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE,
	XREDIS_INIT_NAME, XREDIS_INIT_DATABASE, XREDIS_INIT_DONE
} xredisinitstep;

typedef enum xredisreadystatus {
	XREDIS_READY_OK = 0, XREDIS_READY_AUTH_FAILED, XREDIS_READY_UNSUPPORTED,
	XREDIS_READY_SERVER_ERROR, XREDIS_READY_PROTOCOL, XREDIS_READY_TIMEOUT,
	XREDIS_READY_CLOSED, XREDIS_READY_DISCONNECTED, XREDIS_READY_LIMIT,
	XREDIS_READY_MEMORY
} xredisreadystatus;

/* Initialize before use. Views borrow stable readable input only for ConnectInit.
 * Flags make empty passwords/names and an explicit SELECT 0 unambiguous. With
 * AUTH but no USERNAME use the default user (legacy AUTH password on RESP2).
 * Unselected nonempty fields, USERNAME without AUTH, and a nonzero Database
 * without SELECT are invalid. Server validates client-name/database semantics.
 * Timeout includes factory encoding, worker wait, TCP setup and all steps.
 * MaxWireBytes bounds ALL preencoded setup commands, including AUTO fallback.
 * Default strict RESP3, no optional steps, 5000 ms, 64 KiB. */
typedef struct xredisclientinitconfig {
	size_t Size;
	xredisinitmode Mode;
	uint32 Flags;
	xbytesview Username;
	xbytesview Password;
	xbytesview Name;
	uint32 Database;
	int64 Timeout;
	size_t MaxWireBytes;
} xredisclientinitconfig;

/* Borrowed from the readiness Future. Protocol can be UNKNOWN before selection.
 * Negotiated means a validated HELLO response; legacy RESP2 uses a new server
 * connection's default. Hello preserves the successful RESP3 map/attributes on
 * OK only. Error responses are not retained or copied into Cause, since a peer
 * may echo authentication arguments. Typed status/step survive diagnostic OOM.
 * Retained readiness outcomes do not keep the client/engine alive. */
typedef struct xredisreadyresult {
	xredisreadystatus Status;
	xredisinitstep Step;
	xredisprotocol Protocol;
	bool Negotiated;
	bool Downgraded;
	xredisreply* Hello;
	const xerror* Cause;
} xredisreadyresult;

typedef struct xredisclientinfo {
	size_t Size;
	xredisclientstate State;
	/* Includes concurrent construction and wire/timer/cancel-release tails.
	 * Completed results retained by callers do not retain request/client/engine. */
	size_t Requests;
	size_t CommandBytes;
	size_t Pushes;
	size_t PushBytes;
	uint64 Accepted;
	uint64 Rejected;
	uint64 Completed;
	uint64 DiscardedReplies;
	xredisrequeststatus CloseStatus;
	bool Ready;
	xredisprotocol Protocol;
	bool Negotiated;
	bool Downgraded;
	xredisinitstep InitStep;
	/* Encoded setup storage still owned, including accepted TCP reference tails;
	 * zero is published only after the owned wire is wiped and freed. */
	size_t InitBytes;
} xredisclientinfo;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config);
XREDIS_API bool xredisClientInitConfigInit(xredisclientinitconfig* Init);

/* One TCP connection on an existing running XRT engine, fixed worker affinity.
 * Numeric address only. Config is copied; NULL supplies finite defaults.
 * Asynchronous start permits calls while connecting. Result unchanged on
 * failure. No AUTH/HELLO negotiation, DNS, TLS or reconnect is implicit.
 * Connect/Call may finish before their creation function returns. The caller
 * owns one reference and must Close then Release. Engine is pinned until all
 * physical requests/posts retire; the engine outlives these APIs/callbacks.
 * This module handles ordinary one-reply commands. Known PubSub/replication/
 * MONITOR/CLIENT REPLY mode switches are rejected before admission; dedicated
 * modes and unknown module-command cardinality need their own contract. */
XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result);

/* Managed setup barrier. NULL Init selects strict RESP3 defaults. HELLO 3 can
 * combine AUTH/SETNAME; optional SELECT follows its validated reply. AUTO falls
 * back ONLY on explicit NOPROTO or unknown HELLO errors, then AUTH/name/SELECT.
 * RESP2 uses legacy AUTH/name/SELECT directly, without requiring HELLO support.
 * RESP3/AUTO require a RESP3 reader. Business requests may queue with their own
 * original deadlines, but none is sent until setup succeeds. Setup failure
 * retires the connection; surviving calls report INITIALIZATION/NOT_SENT.
 * Options are copied as encoded wire; no credential view is retained. AUTO
 * preencoding copies auth/name twice; strict modes once. Owned setup bytes are
 * securely wiped after final ownership release, including error/close paths.
 * This does not erase caller, peer, OS or lower transport buffers. No implicit
 * TLS/DNS/reconnect; the application chooses its transport/security policy. */
XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	xredisclient** Result);

/* Shared readiness observer, resolved with xredisreadyresult on every terminal
 * outcome. It does not cancel setup: use Close for connection cancellation.
 * Raw Connect reports transport readiness with UNKNOWN protocol/no negotiation.
 * CONNECTING remains until this barrier succeeds; OPEN means Ready. */
XREDIS_API xfuture* xredisClientReady(xredisclient* Client);
XREDIS_API bool xredisClientRetain(xredisclient* Client);
XREDIS_API void xredisClientRelease(xredisclient* Client);

/* Thread-safe bounded admission; copies/encodes arguments during this call.
 * Timeout is signed milliseconds (-1 forever, 0 already due), measured from
 * API entry through encoding, submission, setup and processing. Future output
 * changes only on ACCEPTED. OVERLOADED/CLOSED are normal counted admission
 * results, not request outcomes. All outputs are disjoint from input/client.
 * Successful submission does not retain any source argument/storage.
 * Limits include abandoned sent requests and actual retained command buffers.
 * xrtFutureCancel wakes the worker without running request completion inline.
 * Cancellation wins when observed before completion; deadline equality expires.
 * Local completion does not remove a sent request's FIFO/wire obligation.
 * Future watch/continuation notifications execute on XRT completion paths:
 * application callbacks must not block a network worker. Managed connections
 * reserve HELLO/AUTH/SELECT/CLIENT SETNAME for setup. Raw Connect still permits
 * explicit controls. Raw AUTH/HELLO command storage is wiped after its final
 * send/FIFO/cancel owner releases it. */
XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Same operation/deadline, then waits for the typed outcome. Rejects calls
 * from ANY worker of the same engine (including a different affinity), since
 * blocking an engine worker is not a supported sync facade. */
XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);

/* Nonblocking ownership transfer of one RESP3 PUSH. Empty returns false and
 * preserves Reply/ambient error. A full count/byte queue retires the connection
 * rather than silently losing pushes or shifting response correlation.
 * Ordinary replies without a sent FIFO slot are protocol errors. */
XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply);
XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info);

/* Idempotent, allocation-free close request. Accepted requests are completed
 * with explicit statuses; physical duties remain alive until TCP closure and
 * data-bearing request/control ownership retires. Closed returns a retained
 * observer of that barrier, resolved with NULL. The current notification tail
 * may still be returning; stop/join the engine before unloading callback code.
 * Closed does not itself request close. */
XREDIS_API bool xredisClientClose(xredisclient* Client);
XREDIS_API xfuture* xredisClientClosed(xredisclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis.h */
/* ========================================================================== */

#ifndef XREDIS_H
#define XREDIS_H


#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_COMMAND)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_COMMAND"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_READER)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_READER"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_REPLY)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_REPLY"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif


#endif

#endif

#if defined(XREDIS_IMPLEMENTATION) && !defined(XREDIS_IMPLEMENTATION_ONCE)
#define XREDIS_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* source: extlibs/xredis/src/command/command.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_COMMAND)

static bool xredisCommandFail(xerrkind Kind, xrediscommanderror Code,
	cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static size_t xredisCommandDigits(size_t Value)
{
	size_t Count = 1;
	while (Value >= 10u) { Value /= 10u; ++Count; }
	return Count;
}

static bool xredisCommandMeasure(const xbytesview* Arguments, size_t Count,
	size_t* Size)
{
	size_t Total;
	if (Count == 0 || Arguments == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"a command requires a nonempty argument vector");
	if (Count > (uint64)INT64_MAX || Count > SIZE_MAX / sizeof(*Arguments) ||
		!xrtMemRangeValid(Arguments, Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
			"command argument count or descriptor span overflows");
	Total = 3u + xredisCommandDigits(Count); /* *Count CRLF */
	for (size_t Index = 0; Index < Count; ++Index) {
		size_t Bytes = Arguments[Index].Size;
		size_t Overhead;
		if (Arguments[Index].Data == NULL && Bytes != 0)
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
				"a nonempty command argument requires data");
		if (Bytes > (uint64)INT64_MAX ||
			!xrtMemRangeValid(Arguments[Index].Data, Bytes))
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"command argument length or address span overflows");
		Overhead = 5u + xredisCommandDigits(Bytes); /* $Bytes CRLF data CRLF */
		if (Bytes > SIZE_MAX - Overhead || Total > SIZE_MAX - Bytes - Overhead)
			return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_OVERFLOW,
				"encoded command size overflows");
		Total += Bytes + Overhead;
	}
	*Size = Total;
	return true;
}

static bool xredisCommandOutputSafe(const void* Output, size_t Size,
	const xbytesview* Arguments, size_t Count)
{
	if (!xrtMemRangeValid(Output, Size))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command output address span is invalid");
	if (xrtMemRangesOverlap(Output, Size, Arguments,
		Count * sizeof(*Arguments)))
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
			"command output overlaps argument descriptors");
	for (size_t Index = 0; Index < Count; ++Index) {
		if (xrtMemRangesOverlap(Output, Size,
			Arguments[Index].Data, Arguments[Index].Size))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command output overlaps argument data");
	}
	return true;
}

static unsigned char* xredisCommandNumber(unsigned char* Output, size_t Value)
{
	unsigned char Digits[3u * sizeof(size_t)];
	size_t Count = 0;
	do { Digits[Count++] = (unsigned char)('0' + Value % 10u); Value /= 10u; }
	while (Value != 0);
	while (Count != 0) *Output++ = Digits[--Count];
	*Output++ = '\r'; *Output++ = '\n';
	return Output;
}

XREDIS_API bool xredisCommandSize(const xbytesview* Arguments, size_t Count,
	size_t* Required)
{
	size_t Size;
	if (Required == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command size requires an output");
	if (!xredisCommandMeasure(Arguments, Count, &Size) ||
		!xredisCommandOutputSafe(Required, sizeof(*Required), Arguments, Count))
		return false;
	*Required = Size;
	return true;
}

XREDIS_API bool xredisCommandWrite(ptr Output, size_t Capacity,
	const xbytesview* Arguments, size_t Count, size_t* Written)
{
	size_t Size;
	unsigned char* Cursor;
	if (!xredisCommandMeasure(Arguments, Count, &Size)) return false;
	if (Output == NULL)
		return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_ARGUMENT,
			"command write requires an output buffer");
	if (Capacity < Size)
		return xredisCommandFail(XERR_RANGE, XREDIS_COMMAND_CAPACITY,
			"command output capacity is too small");
	if (!xredisCommandOutputSafe(Output, Size, Arguments, Count)) return false;
	if (Written != NULL) {
		if (!xredisCommandOutputSafe(Written, sizeof(*Written), Arguments, Count))
			return false;
		if (xrtMemRangesOverlap(Output, Size, Written, sizeof(*Written)))
			return xredisCommandFail(XERR_ARGUMENT, XREDIS_COMMAND_OVERLAP,
				"command size output overlaps wire bytes");
	}
	Cursor = Output;
	*Cursor++ = '*'; Cursor = xredisCommandNumber(Cursor, Count);
	for (size_t Index = 0; Index < Count; ++Index) {
		*Cursor++ = '$'; Cursor = xredisCommandNumber(Cursor, Arguments[Index].Size);
		if (Arguments[Index].Size != 0) {
			memcpy(Cursor, Arguments[Index].Data, Arguments[Index].Size);
			Cursor += Arguments[Index].Size;
		}
		*Cursor++ = '\r'; *Cursor++ = '\n';
	}
	if (Written != NULL) *Written = Size;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/reader/reader.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_READER)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_READER)

typedef enum xredisreaderstate {
	XREDIS_RS_TYPE, XREDIS_RS_BEGIN_LINE, XREDIS_RS_LINE, XREDIS_RS_LINE_LF,
	XREDIS_RS_HEADER, XREDIS_RS_HEADER_LF, XREDIS_RS_BEGIN_VALUE,
	XREDIS_RS_BULK, XREDIS_RS_BULK_CR, XREDIS_RS_BULK_LF, XREDIS_RS_CHUNK,
	XREDIS_RS_END, XREDIS_RS_FINISH_VALUE, XREDIS_RS_END_CONTAINER,
	XREDIS_RS_TERM_CR, XREDIS_RS_TERM_LF, XREDIS_RS_COMPLETE, XREDIS_RS_FAILED
} xredisreaderstate;

typedef enum xredisreaderfloatstate {
	XREDIS_RF_START, XREDIS_RF_SIGN, XREDIS_RF_INTEGER, XREDIS_RF_DOT,
	XREDIS_RF_FRACTION, XREDIS_RF_EXP, XREDIS_RF_EXP_SIGN, XREDIS_RF_EXP_DIGITS,
	XREDIS_RF_I, XREDIS_RF_IN, XREDIS_RF_INF,
	XREDIS_RF_N, XREDIS_RF_NA, XREDIS_RF_NAN, XREDIS_RF_NAN_PAYLOAD,
	XREDIS_RF_NAN_DONE
} xredisreaderfloatstate;

typedef struct xredisreaderlevel {
	uint64 Remaining;
	uint64 Seen;
	xredisreplytype Type;
	uint32 Flags;
	bool Attributes;
} xredisreaderlevel;

typedef struct xredisreaderparse {
	xredisreaderstate State;
	xredisreadevent Current;
	size_t Depth;
	uint64 StreamBytes, FrameBytes, FrameValues;
	uint64 LastFrameBytes, LastFrameValues, CompletedFrames;
	uint64 HeaderValue, HeaderBytes, HeaderDigits;
	uint64 LineBytes, Digits, Number, Remaining, PayloadBytes;
	xredisreaderfloatstate FloatState;
	bool Negative, Plus, HeaderNegative, HeaderUnknown, ChunkHeader;
	bool FrameActive, RootAttributes, Paused;
	xredisreadererror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	uint64 ErrorOffset;
} xredisreaderparse;

struct xredisreader {
	xredisreaderconfig Config;
	xredisreadproc Callback;
	ptr User;
	size_t StorageBytes;
	bool Active;
	xredisreaderparse Parse;
	xredisreaderlevel Stack[];
};

static bool xredisReaderReport(xerrkind Kind, xredisreadererror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReaderFail(xredisreader* Reader, xerrkind Kind,
	xredisreadererror Code, cstr Message)
{
	xredisreaderparse* P = &Reader->Parse;
	P->State = XREDIS_RS_FAILED; P->Error = Code; P->ErrorKind = Kind;
	P->ErrorMessage = Message; P->ErrorOffset = P->StreamBytes;
	return xredisReaderReport(Kind, Code, Message);
}

static bool xredisReaderIdle(xredisreader* Reader)
{
	if (Reader == NULL)
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "reader is NULL");
	if (Reader->Active)
		return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "reader callback cannot reenter mutation");
	return true;
}

XREDIS_API bool xredisReaderConfigInit(xredisreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config output");
	*Config = (xredisreaderconfig){sizeof(*Config), XREDIS_RESP3, 0, 64,
		64u * 1024u * 1024u, 16u * 1024u * 1024u, 64u * 1024u,
		1000000u, 1000000u, 64u * 1024u};
	return true;
}

XREDIS_API bool xredisReaderCreate(const xredisreaderconfig* Config,
	xredisreadproc Callback, ptr User, xredisreader** Result)
{
	xredisreaderconfig Copy;
	xredisreader* Reader;
	size_t StorageBytes;
	if (Callback == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader creation arguments");
	if (Config == NULL) xredisReaderConfigInit(&Copy);
	else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || (Copy.Protocol != XREDIS_RESP2 && Copy.Protocol != XREDIS_RESP3) ||
		(Copy.Flags & ~XREDIS_READER_LEGACY_NAN) != 0 || Copy.MaxFrameBytes == 0 ||
		Copy.MaxValues == 0 || Copy.MaxFeedBytes == 0 || Copy.MaxFrameBytes > (uint64)INT64_MAX ||
		Copy.MaxBulkBytes > (uint64)INT64_MAX || Copy.MaxLineBytes > (uint64)INT64_MAX ||
		Copy.MaxContainerItems > (uint64)INT64_MAX || Copy.MaxValues > (uint64)INT64_MAX ||
		Copy.MaxDepth > (SIZE_MAX - sizeof(*Reader)) / sizeof(xredisreaderlevel))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader config size, protocol or limits");
	StorageBytes = sizeof(*Reader) + Copy.MaxDepth * sizeof(xredisreaderlevel);
	Reader = (xredisreader*)xrtCalloc(1, StorageBytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->Callback = Callback; Reader->User = User;
	Reader->StorageBytes = StorageBytes; Reader->Parse.State = XREDIS_RS_TYPE;
	*Result = Reader;
	return true;
}

XREDIS_API bool xredisReaderDestroy(xredisreader* Reader)
{
	if (Reader == NULL) return true;
	if (!xredisReaderIdle(Reader)) return false;
	xrtFree(Reader);
	return true;
}

XREDIS_API bool xredisReaderReset(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	memset(&Reader->Parse, 0, sizeof(Reader->Parse));
	Reader->Parse.State = XREDIS_RS_TYPE;
	return true;
}

XREDIS_API bool xredisReaderInfo(const xredisreader* Reader, xredisreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader info output or size");
	const xredisreaderparse* P = &Reader->Parse;
	*Info = (xredisreaderinfo){sizeof(*Info), P->StreamBytes, P->FrameBytes, P->FrameValues,
		P->LastFrameBytes, P->LastFrameValues, P->CompletedFrames, P->Depth,
		P->Paused, P->Error, P->ErrorOffset};
	return true;
}

static bool xredisReaderAggregate(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_ARRAY || Type == XREDIS_REPLY_MAP ||
		Type == XREDIS_REPLY_ATTRIBUTE || Type == XREDIS_REPLY_SET || Type == XREDIS_REPLY_PUSH;
}

static bool xredisReaderPairs(xredisreplytype Type)
{
	return Type == XREDIS_REPLY_MAP || Type == XREDIS_REPLY_ATTRIBUTE;
}

static bool xredisReaderRoom(xredisreader* Reader)
{
	if (Reader->Parse.FrameBytes >= Reader->Config.MaxFrameBytes || Reader->Parse.StreamBytes == UINT64_MAX)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "reader wire byte limit exceeded");
	return true;
}

static void xredisReaderAdvance(xredisreader* Reader, size_t* Index, size_t Count)
{
	Reader->Parse.StreamBytes += Count; Reader->Parse.FrameBytes += Count;
	*Index += Count;
}

static bool xredisReaderEmit(xredisreader* Reader, xredisreadkind Kind, xbytesview Data)
{
	xredisreadevent Event = Reader->Parse.Current;
	Event.Kind = Kind; Event.Data = Data;
	xredisreadaction Action = Reader->Callback(&Event, Reader->User);
	if (Action == XREDIS_READ_NEXT) return true;
	if (Action == XREDIS_READ_PAUSE) { Reader->Parse.Paused = true; return false; }
	return xredisReaderFail(Reader, XERR_CANCELLED, XREDIS_READER_CALLBACK,
		Action == XREDIS_READ_FAIL ? "reader callback rejected the frame" : "invalid reader callback action");
}

static void xredisReaderHeaderStart(xredisreaderparse* P, bool Chunk)
{
	P->HeaderValue = 0; P->HeaderBytes = 0; P->HeaderDigits = 0;
	P->HeaderNegative = false; P->HeaderUnknown = false;
	P->ChunkHeader = Chunk; P->State = XREDIS_RS_HEADER;
}

static bool xredisReaderType(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type;
	switch (Byte) {
		case '+': Type = XREDIS_REPLY_SIMPLE_STRING; break;
		case '-': Type = XREDIS_REPLY_SIMPLE_ERROR; break;
		case ':': Type = XREDIS_REPLY_INTEGER; break;
		case '$': Type = XREDIS_REPLY_BULK_STRING; break;
		case '*': Type = XREDIS_REPLY_ARRAY; break;
		case '_': Type = XREDIS_REPLY_NULL; break;
		case '#': Type = XREDIS_REPLY_BOOLEAN; break;
		case ',': Type = XREDIS_REPLY_DOUBLE; break;
		case '(': Type = XREDIS_REPLY_BIG_NUMBER; break;
		case '!': Type = XREDIS_REPLY_BULK_ERROR; break;
		case '=': Type = XREDIS_REPLY_VERBATIM; break;
		case '%': Type = XREDIS_REPLY_MAP; break;
		case '|': Type = XREDIS_REPLY_ATTRIBUTE; break;
		case '~': Type = XREDIS_REPLY_SET; break;
		case '>': Type = XREDIS_REPLY_PUSH; break;
		default: return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "unknown RESP type prefix");
	}
	if (Reader->Config.Protocol == XREDIS_RESP2 && Type > XREDIS_REPLY_ARRAY)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP3 type in RESP2 mode");
	if (Type == XREDIS_REPLY_PUSH && P->Depth != 0)
		return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push must be top-level");
	if (P->Depth != 0 && Type != XREDIS_REPLY_ATTRIBUTE) {
		xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
		if (Parent->Type == XREDIS_REPLY_PUSH && Parent->Seen == 0 &&
			Type != XREDIS_REPLY_SIMPLE_STRING && Type != XREDIS_REPLY_BULK_STRING)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind must be a string");
		if ((Parent->Flags & XREDIS_REPLY_STREAMED) &&
			(xredisReaderPairs(Parent->Type) ? Parent->Seen / 2u : Parent->Seen) >= Reader->Config.MaxContainerItems)
			return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "streamed aggregate item limit exceeded");
	}
	if (P->FrameValues >= Reader->Config.MaxValues)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "frame value limit exceeded");
	++P->FrameValues; P->FrameActive = true;
	P->Current = (xredisreadevent){0}; P->Current.Type = Type;
	P->Current.Depth = P->Depth; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	P->LineBytes = 0; P->Digits = 0; P->Number = 0; P->PayloadBytes = 0;
	P->Negative = false; P->Plus = false; P->FloatState = XREDIS_RF_START;
	if (Type == XREDIS_REPLY_NULL) { P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0; }
	if (Type == XREDIS_REPLY_BULK_STRING || Type == XREDIS_REPLY_BULK_ERROR ||
		Type == XREDIS_REPLY_VERBATIM || xredisReaderAggregate(Type)) xredisReaderHeaderStart(P, false);
	else P->State = XREDIS_RS_BEGIN_LINE;
	return true;
}

static bool xredisReaderHeaderByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	if (P->HeaderBytes >= Reader->Config.MaxLineBytes)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP header line limit exceeded");
	if (P->HeaderBytes == 0 && Byte == '-') P->HeaderNegative = true;
	else if (P->HeaderBytes == 0 && Byte == '?') P->HeaderUnknown = true;
	else if (Byte >= '0' && Byte <= '9' && !P->HeaderUnknown) {
		uint64 Digit = (uint64)(Byte - '0');
		if (P->HeaderValue > ((uint64)INT64_MAX - Digit) / 10u)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "RESP length exceeds int64");
		P->HeaderValue = P->HeaderValue * 10u + Digit; ++P->HeaderDigits;
	} else return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP length token");
	++P->HeaderBytes;
	return true;
}

static bool xredisReaderBulkRoom(xredisreader* Reader, uint64 Bytes)
{
	xredisreaderparse* P = &Reader->Parse;
	uint64 Room = Reader->Config.MaxFrameBytes - P->FrameBytes;
	if (Bytes > Reader->Config.MaxBulkBytes - P->PayloadBytes || Room < 2u || Bytes > Room - 2u)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "bulk payload or frame limit exceeded");
	return true;
}

static bool xredisReaderHeaderEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	uint64 Length = P->HeaderValue;
	if (P->HeaderUnknown) {
		if (P->HeaderBytes != 1u || P->ChunkHeader || Reader->Config.Protocol != XREDIS_RESP3 ||
			(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY &&
			 Type != XREDIS_REPLY_MAP && Type != XREDIS_REPLY_SET))
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "streaming is invalid for this RESP type");
		P->Current.Flags = XREDIS_REPLY_STREAMED; P->Current.Length = XREDIS_LENGTH_UNKNOWN;
	} else {
		if (P->HeaderDigits == 0)
			return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP length digits");
		if (P->HeaderNegative) {
			if (P->HeaderBytes != 2u || Length != 1u || P->ChunkHeader ||
				(Type != XREDIS_REPLY_BULK_STRING && Type != XREDIS_REPLY_ARRAY))
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid negative RESP length");
			if (P->Depth != 0 && Reader->Stack[P->Depth - 1u].Type == XREDIS_REPLY_PUSH &&
				Reader->Stack[P->Depth - 1u].Seen == 0)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push kind cannot be null");
			P->Current.Flags = XREDIS_REPLY_NULL_VALUE; P->Current.Length = 0;
		} else if (P->ChunkHeader) {
			if (Length != 0 && !xredisReaderBulkRoom(Reader, Length)) return false;
			P->Remaining = Length; P->State = Length == 0 ? XREDIS_RS_END : XREDIS_RS_BULK;
			return true;
		} else {
			P->Current.Length = Length;
			if (xredisReaderAggregate(Type)) {
				uint64 Slots = xredisReaderPairs(Type) ? Length * 2u : Length;
				if (Length > Reader->Config.MaxContainerItems || Slots > Reader->Config.MaxValues - P->FrameValues)
					return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "declared aggregate exceeds item or value limit");
				if (Type == XREDIS_REPLY_PUSH && Length == 0)
					return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "push requires a kind string");
			} else if (!xredisReaderBulkRoom(Reader, Length)) return false;
			if (Type == XREDIS_REPLY_VERBATIM && Length < 4u)
				return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim payload requires a format prefix");
		}
	}
	if (xredisReaderAggregate(Type) && !(P->Current.Flags & XREDIS_REPLY_NULL_VALUE) && P->Depth >= Reader->Config.MaxDepth)
		return xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "aggregate nesting limit exceeded");
	P->State = XREDIS_RS_BEGIN_VALUE;
	return true;
}

/* An incremental grammar, independent of locale/libc strtod and without a
 * token buffer. Conversion/rounding is intentionally left to a reply builder. */
static bool xredisReaderFloatByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	bool Digit = Byte >= '0' && Byte <= '9';
	bool Legacy = (Reader->Config.Flags & XREDIS_READER_LEGACY_NAN) != 0;
	unsigned char Special = Legacy && Byte >= 'A' && Byte <= 'Z' ? (unsigned char)(Byte + ('a' - 'A')) : Byte;
	switch (P->FloatState) {
		case XREDIS_RF_START:
			if (Byte == '+' || Byte == '-') {
				P->Negative = Byte == '-'; P->Plus = Byte == '+'; P->FloatState = XREDIS_RF_SIGN; return true;
			}
			/* fall through */
		case XREDIS_RF_SIGN:
			if (Digit) P->FloatState = XREDIS_RF_INTEGER;
			else if (Byte == 'i' && !P->Plus) P->FloatState = XREDIS_RF_I;
			else if (Special == 'n' && (Legacy || (!P->Negative && !P->Plus))) P->FloatState = XREDIS_RF_N;
			else return false;
			return true;
		case XREDIS_RF_INTEGER:
			if (Digit) return true;
			if (Byte == '.') { P->FloatState = XREDIS_RF_DOT; return true; }
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_DOT:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_FRACTION; return true;
		case XREDIS_RF_FRACTION:
			if (Digit) return true;
			if (Byte == 'e' || Byte == 'E') { P->FloatState = XREDIS_RF_EXP; return true; }
			return false;
		case XREDIS_RF_EXP:
			if (Byte == '+' || Byte == '-') { P->FloatState = XREDIS_RF_EXP_SIGN; return true; }
			/* fall through */
		case XREDIS_RF_EXP_SIGN:
			if (!Digit) return false;
			P->FloatState = XREDIS_RF_EXP_DIGITS; return true;
		case XREDIS_RF_EXP_DIGITS: return Digit;
		case XREDIS_RF_I: if (Byte != 'n') return false; P->FloatState = XREDIS_RF_IN; return true;
		case XREDIS_RF_IN: if (Byte != 'f') return false; P->FloatState = XREDIS_RF_INF; return true;
		case XREDIS_RF_N: if (Special != 'a') return false; P->FloatState = XREDIS_RF_NA; return true;
		case XREDIS_RF_NA: if (Special != 'n') return false; P->FloatState = XREDIS_RF_NAN; return true;
		case XREDIS_RF_NAN:
			if (Byte != '(' || !Legacy) return false;
			P->FloatState = XREDIS_RF_NAN_PAYLOAD; return true;
		case XREDIS_RF_NAN_PAYLOAD:
			if (Byte == ')') { P->FloatState = XREDIS_RF_NAN_DONE; return true; }
			return Digit || (Special >= 'a' && Special <= 'z') || Byte == '_';
		default: return false;
	}
}

static bool xredisReaderScalarByte(xredisreader* Reader, unsigned char Byte)
{
	xredisreaderparse* P = &Reader->Parse;
	switch (P->Current.Type) {
		case XREDIS_REPLY_NULL: return false;
		case XREDIS_REPLY_BOOLEAN:
			if (P->LineBytes != 0 || (Byte != 't' && Byte != 'f')) return false;
			P->Current.Boolean = Byte == 't'; break;
		case XREDIS_REPLY_INTEGER:
		case XREDIS_REPLY_BIG_NUMBER:
			if (P->LineBytes == 0 && (Byte == '+' || Byte == '-')) P->Negative = Byte == '-';
			else {
				if (Byte < '0' || Byte > '9') return false;
				if (P->Current.Type == XREDIS_REPLY_INTEGER) {
					uint64 Limit = (uint64)INT64_MAX + (P->Negative ? 1u : 0u);
					uint64 Digit = (uint64)(Byte - '0');
					if (P->Number > (Limit - Digit) / 10u) return false;
					P->Number = P->Number * 10u + Digit;
				}
				++P->Digits;
			} break;
		case XREDIS_REPLY_DOUBLE: if (!xredisReaderFloatByte(Reader, Byte)) return false; break;
		default: break;
	}
	++P->LineBytes;
	return true;
}

static bool xredisReaderScalarEnd(xredisreader* Reader)
{
	xredisreaderparse* P = &Reader->Parse;
	xredisreplytype Type = P->Current.Type;
	bool Valid = true;
	if (Type == XREDIS_REPLY_INTEGER || Type == XREDIS_REPLY_BIG_NUMBER) Valid = P->Digits != 0;
	else if (Type == XREDIS_REPLY_BOOLEAN) Valid = P->LineBytes == 1u;
	else if (Type == XREDIS_REPLY_DOUBLE) Valid = P->FloatState == XREDIS_RF_INTEGER ||
		P->FloatState == XREDIS_RF_FRACTION || P->FloatState == XREDIS_RF_EXP_DIGITS ||
		P->FloatState == XREDIS_RF_INF || P->FloatState == XREDIS_RF_NAN || P->FloatState == XREDIS_RF_NAN_DONE;
	if (!Valid) return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "incomplete RESP scalar token");
	if (Type == XREDIS_REPLY_INTEGER)
		P->Current.Integer = P->Negative ? (P->Number == (uint64)INT64_MAX + 1u ? INT64_MIN : -(int64)P->Number) : (int64)P->Number;
	P->Current.Length = P->LineBytes; P->State = XREDIS_RS_END;
	return true;
}

static size_t xredisReaderAvailable(const xredisreader* Reader, size_t Available)
{
	uint64 Room = Reader->Config.MaxFrameBytes - Reader->Parse.FrameBytes;
	uint64 StreamRoom = UINT64_MAX - Reader->Parse.StreamBytes;
	if ((uint64)Available > Room) Available = (size_t)Room;
	if ((uint64)Available > StreamRoom) Available = (size_t)StreamRoom;
	return Available;
}

XREDIS_API xredisreadstatus xredisReaderFeed(xredisreader* Reader,
	xbytesview Input, size_t* Consumed)
{
	xredisreadstatus Result = XREDIS_READER_MORE;
	size_t Index = 0, Budget;
	xredisreaderparse* P;
	const xbytesview Empty = {NULL, 0};
	if (!xredisReaderIdle(Reader)) return XREDIS_READER_ERROR;
	if (Consumed == NULL || !xrtMemRangeValid(Consumed, sizeof(*Consumed)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Reader, Reader->StorageBytes, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed))) {
		xredisReaderReport(XERR_ARGUMENT, XREDIS_READER_ARGUMENT, "invalid reader input, consumed output or overlap");
		return XREDIS_READER_ERROR;
	}
	P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) {
		*Consumed = 0; xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage); return XREDIS_READER_ERROR;
	}
	Budget = Input.Size < Reader->Config.MaxFeedBytes ? Input.Size : Reader->Config.MaxFeedBytes;
	Reader->Active = true; P->Paused = false;
	for (;;) {
		/* Pending events/transitions are committed before callbacks. Draining
		 * them costs O(depth), even for empty input or a one-byte work budget. */
		if (P->State == XREDIS_RS_BEGIN_LINE) {
			P->State = XREDIS_RS_LINE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_BEGIN_VALUE) {
			if (P->Current.Flags & XREDIS_REPLY_NULL_VALUE) P->State = XREDIS_RS_END;
			else if (xredisReaderAggregate(P->Current.Type)) {
				xredisreaderlevel* Level = &Reader->Stack[P->Depth++];
				*Level = (xredisreaderlevel){0}; Level->Type = P->Current.Type; Level->Flags = P->Current.Flags;
				Level->Remaining = (Level->Flags & XREDIS_REPLY_STREAMED) ? 0 :
					(xredisReaderPairs(Level->Type) ? P->Current.Length * 2u : P->Current.Length);
				P->State = !(Level->Flags & XREDIS_REPLY_STREAMED) && Level->Remaining == 0 ? XREDIS_RS_END_CONTAINER : XREDIS_RS_TYPE;
			} else {
				P->Remaining = P->Current.Length;
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK :
					(P->Remaining == 0 ? XREDIS_RS_BULK_CR : XREDIS_RS_BULK);
			}
			if (!xredisReaderEmit(Reader, XREDIS_READ_BEGIN, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_END_CONTAINER) {
			xredisreaderlevel* Level = &Reader->Stack[--P->Depth];
			P->Current = (xredisreadevent){0}; P->Current.Type = Level->Type;
			P->Current.Flags = Level->Flags; P->Current.Depth = P->Depth;
			P->Current.Length = xredisReaderPairs(Level->Type) ? Level->Seen / 2u : Level->Seen;
			P->State = XREDIS_RS_END; continue;
		}
		if (P->State == XREDIS_RS_END) {
			if (P->Current.Type == XREDIS_REPLY_BULK_STRING || P->Current.Type == XREDIS_REPLY_BULK_ERROR ||
				P->Current.Type == XREDIS_REPLY_VERBATIM) P->Current.Length = P->PayloadBytes;
			P->State = XREDIS_RS_FINISH_VALUE;
			if (!xredisReaderEmit(Reader, XREDIS_READ_END, Empty)) goto callback_stop;
			continue;
		}
		if (P->State == XREDIS_RS_FINISH_VALUE) {
			if (P->Current.Type == XREDIS_REPLY_ATTRIBUTE) {
				if (P->Depth == 0) P->RootAttributes = true;
				else Reader->Stack[P->Depth - 1u].Attributes = true;
				P->State = XREDIS_RS_TYPE;
			} else if (P->Depth == 0) P->State = XREDIS_RS_COMPLETE;
			else {
				xredisreaderlevel* Parent = &Reader->Stack[P->Depth - 1u];
				++Parent->Seen; Parent->Attributes = false;
				if (!(Parent->Flags & XREDIS_REPLY_STREAMED) && --Parent->Remaining == 0) P->State = XREDIS_RS_END_CONTAINER;
				else P->State = XREDIS_RS_TYPE;
			}
			continue;
		}
		if (P->State == XREDIS_RS_COMPLETE) {
			if (P->CompletedFrames == UINT64_MAX) {
				xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "completed frame counter overflows"); goto failed;
			}
			++P->CompletedFrames; P->LastFrameBytes = P->FrameBytes; P->LastFrameValues = P->FrameValues;
			P->FrameBytes = 0; P->FrameValues = 0; P->FrameActive = false; P->RootAttributes = false;
			P->State = XREDIS_RS_TYPE; Result = XREDIS_READER_READY; break;
		}
		if (Index == Budget) { Result = Index < Input.Size ? XREDIS_READER_YIELD : XREDIS_READER_MORE; break; }
		if (!xredisReaderRoom(Reader)) goto failed;
		unsigned char Byte = Input.Data[Index];
		switch (P->State) {
			case XREDIS_RS_TYPE:
				if (Byte == '.') {
					if (P->Depth == 0 || !(Reader->Stack[P->Depth - 1u].Flags & XREDIS_REPLY_STREAMED) ||
						Reader->Stack[P->Depth - 1u].Attributes ||
						(xredisReaderPairs(Reader->Stack[P->Depth - 1u].Type) && (Reader->Stack[P->Depth - 1u].Seen & 1u))) {
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid streamed aggregate terminator or orphan attributes"); goto failed;
					}
					P->State = XREDIS_RS_TERM_CR;
				} else if (!xredisReaderType(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER:
				if (Byte == '\r') P->State = XREDIS_RS_HEADER_LF;
				else if (!xredisReaderHeaderByte(Reader, Byte)) goto failed;
				xredisReaderAdvance(Reader, &Index, 1); break;
			case XREDIS_RS_HEADER_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderHeaderEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_LINE: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				const unsigned char* CR = (const unsigned char*)memchr(Input.Data + Index, '\r', Count);
				if (CR != NULL) Count = (size_t)(CR - Input.Data - Index);
				const unsigned char* LF = (const unsigned char*)memchr(Input.Data + Index, '\n', Count);
				if (LF != NULL) Count = (size_t)(LF - Input.Data - Index);
				if (Count == 0) {
					if (Byte != '\r') goto bad_terminator;
					xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_LINE_LF; break;
				}
				uint64 LineRoom = Reader->Config.MaxLineBytes - P->LineBytes;
				if (LineRoom == 0) { xredisReaderFail(Reader, XERR_RANGE, XREDIS_READER_LIMIT, "RESP scalar line limit exceeded"); goto failed; }
				if ((uint64)Count > LineRoom) Count = (size_t)LineRoom;
				if (P->Current.Type == XREDIS_REPLY_SIMPLE_STRING || P->Current.Type == XREDIS_REPLY_SIMPLE_ERROR) P->LineBytes += Count;
				else for (size_t Part = 0; Part < Count; ++Part) {
					if (!xredisReaderScalarByte(Reader, Input.Data[Index + Part])) {
						xredisReaderAdvance(Reader, &Index, Part);
						xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "invalid RESP scalar or integer overflow"); goto failed;
					}
				}
				xbytesview Data = {Input.Data + Index, Count}; xredisReaderAdvance(Reader, &Index, Count);
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_LINE_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				if (!xredisReaderScalarEnd(Reader)) goto failed;
				break;
			case XREDIS_RS_CHUNK:
				if (Byte != ';') { xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing streamed bulk chunk prefix"); goto failed; }
				xredisReaderAdvance(Reader, &Index, 1); xredisReaderHeaderStart(P, true); break;
			case XREDIS_RS_BULK: {
				size_t Count = xredisReaderAvailable(Reader, Budget - Index);
				if ((uint64)Count > P->Remaining) Count = (size_t)P->Remaining;
				if (P->Current.Type == XREDIS_REPLY_VERBATIM && P->PayloadBytes <= 3u &&
					(uint64)Count > 3u - P->PayloadBytes && Input.Data[Index + (size_t)(3u - P->PayloadBytes)] != ':') {
					xredisReaderAdvance(Reader, &Index, (size_t)(3u - P->PayloadBytes));
					xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "verbatim format requires a colon"); goto failed;
				}
				xbytesview Data = {Input.Data + Index, Count};
				P->Remaining -= Count; P->PayloadBytes += Count;
				xredisReaderAdvance(Reader, &Index, Count);
				if (P->Remaining == 0) P->State = XREDIS_RS_BULK_CR;
				if (!xredisReaderEmit(Reader, XREDIS_READ_DATA, Data)) goto callback_stop;
				break;
			}
			case XREDIS_RS_BULK_CR:
			case XREDIS_RS_TERM_CR:
				if (Byte != '\r') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = P->State == XREDIS_RS_BULK_CR ? XREDIS_RS_BULK_LF : XREDIS_RS_TERM_LF; break;
			case XREDIS_RS_BULK_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1);
				P->State = (P->Current.Flags & XREDIS_REPLY_STREAMED) ? XREDIS_RS_CHUNK : XREDIS_RS_END; break;
			case XREDIS_RS_TERM_LF:
				if (Byte != '\n') goto bad_terminator;
				xredisReaderAdvance(Reader, &Index, 1); P->State = XREDIS_RS_END_CONTAINER; break;
			default:
				xredisReaderFail(Reader, XERR_INTERNAL, XREDIS_READER_STATE, "invalid reader state"); goto failed;
		}
	}
	goto done;
bad_terminator:
	xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_PROTOCOL, "missing RESP CRLF terminator");
failed:
	Result = XREDIS_READER_ERROR; goto done;
callback_stop:
	Result = P->Error != XREDIS_READER_OK ? XREDIS_READER_ERROR : XREDIS_READER_PAUSED;
done:
	Reader->Active = false; *Consumed = Index;
	return Result;
}

XREDIS_API bool xredisReaderFinish(xredisreader* Reader)
{
	if (!xredisReaderIdle(Reader)) return false;
	xredisreaderparse* P = &Reader->Parse;
	if (P->Error != XREDIS_READER_OK) return xredisReaderReport(P->ErrorKind, P->Error, P->ErrorMessage);
	if (P->Paused) return xredisReaderReport(XERR_STATE, XREDIS_READER_STATE, "resume paused reader before end-of-stream");
	if (P->State == XREDIS_RS_TYPE && !P->FrameActive && !P->RootAttributes && P->Depth == 0) return true;
	return xredisReaderFail(Reader, XERR_PROTOCOL, XREDIS_READER_TRUNCATED, "truncated RESP frame or orphan attributes at end-of-stream");
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/reply/reply.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_REPLY)
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_REPLY)

typedef struct xredisreplynode {
	xredisreplytype Type;
	uint32 Flags;
	uint64 Length;
	int64 Integer;
	bool Boolean;
	size_t Offset, Bytes, ChildCount, FirstChild, Next, Attributes;
} xredisreplynode;

struct xredisreply {
	xatomic32 References;
	xredisreplynode* Nodes;
	unsigned char* Data;
	size_t Count, NodeCapacity, DataBytes, DataCapacity, Root, Reserved, Peak;
	xredisreplynode InlineNode[1];
	unsigned char InlineData[64];
};

typedef struct xredisreplylevel {
	size_t Open, LastChild, PendingFirst, PendingLast;
} xredisreplylevel;

struct xredisreplyreader {
	xredisreplyreaderconfig Config;
	xredisreader* Parser;
	xredisreply* Partial;
	size_t StorageBytes, Peak;
	xredisreplyerror Error;
	xerrkind ErrorKind;
	cstr ErrorMessage;
	xredisreplylevel Levels[];
};

static bool xredisReplyReport(xerrkind Kind, xredisreplyerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message);
	return false;
}

static bool xredisReplyFail(xredisreplyreader* Reader, xerrkind Kind,
	xredisreplyerror Code, cstr Message)
{
	Reader->Error = Code; Reader->ErrorKind = Kind; Reader->ErrorMessage = Message;
	return false;
}

static bool xredisReplyOverlap(const xredisreply* Reply, const void* Data, size_t Bytes)
{
	return Reply != NULL && (xrtMemRangesOverlap(Reply, sizeof(*Reply), Data, Bytes) ||
		(Reply->Nodes != Reply->InlineNode && xrtMemRangesOverlap(Reply->Nodes,
			Reply->NodeCapacity * sizeof(*Reply->Nodes), Data, Bytes)) ||
		(Reply->Data != Reply->InlineData && xrtMemRangesOverlap(Reply->Data, Reply->DataCapacity, Data, Bytes)));
}

static bool xredisReplyReaderOverlap(const xredisreplyreader* Reader, const void* Data, size_t Bytes)
{
	return xrtMemRangesOverlap(Reader, Reader->StorageBytes, Data, Bytes) ||
		xredisReplyOverlap(Reader->Partial, Data, Bytes);
}

XREDIS_API bool xredisReplyRetain(xredisreply* Reply)
{
	if (Reply == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply is NULL");
	uint32 Count = xrtAtomic32Load(&Reply->References, XMEMORY_RELAXED);
	for (;;) {
		if (Count == UINT32_MAX)
			return xredisReplyReport(XERR_RANGE, XREDIS_REPLY_LIMIT, "reply reference count overflows");
		if (xrtAtomic32CompareExchange(&Reply->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisReplyRelease(xredisreply* Reply)
{
	if (Reply == NULL) return;
	if (xrtAtomic32FetchSub(&Reply->References, 1, XMEMORY_ACQ_REL) != 1) return;
	if (Reply->Nodes != Reply->InlineNode) xrtFree(Reply->Nodes);
	if (Reply->Data != Reply->InlineData) xrtFree(Reply->Data);
	xrtFree(Reply);
}

XREDIS_API bool xredisReplyInfo(const xredisreply* Reply, xredisreplyinfo* Info)
{
	if (Reply == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyOverlap(Reply, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply info output or size");
	*Info = (xredisreplyinfo){sizeof(*Info), Reply->Root, Reply->Count, Reply->DataBytes, Reply->Reserved, Reply->Peak};
	return true;
}

XREDIS_API bool xredisReplyGet(const xredisreply* Reply, size_t Index, xredisreplyvalue* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) ||
		xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply node index or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	*Value = (xredisreplyvalue){Node->Type, Node->Flags, Node->Length, Node->Integer, Node->Boolean,
		{Node->Bytes == 0 ? NULL : Reply->Data + Node->Offset, Node->Bytes},
		Node->ChildCount, Node->FirstChild, Node->Next, Node->Attributes};
	return true;
}

XREDIS_API bool xredisReplyDouble(const xredisreply* Reply, size_t Index, double* Value)
{
	if (Reply == NULL || Index >= Reply->Count || Reply->Nodes[Index].Type != XREDIS_REPLY_DOUBLE ||
		Value == NULL || !xrtMemRangeValid(Value, sizeof(*Value)) || xredisReplyOverlap(Reply, Value, sizeof(*Value)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid double node or output");
	const xredisreplynode* Node = &Reply->Nodes[Index];
	xstrview Text = {(cstr)Reply->Data + Node->Offset, Node->Bytes};
	size_t Start = Text.Data[0] == '-' || Text.Data[0] == '+' ? 1u : 0;
	if (Text.Data[Start] == 'n' || Text.Data[Start] == 'N')
		Text = Text.Data[0] == '-' ? (xstrview){"-nan", 4} : (xstrview){"nan", 3};
	return xrtNumParse(Text, XNUMBER_PARSE_SPECIAL, Value);
}

XREDIS_API bool xredisReplyReaderConfigInit(xredisreplyreaderconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader config output");
	*Config = (xredisreplyreaderconfig){0}; Config->Size = sizeof(*Config);
	xredisReaderConfigInit(&Config->Reader); Config->MaxOwnedBytes = 64u * 1024u * 1024u;
	return true;
}

/* Budget includes the complete destination before the old buffer is freed.
 * Explicit copy/free counts only used bytes and keeps failure ownership clear. */
static size_t xredisReplyGrowth(xredisreplyreader* Reader, size_t Old, size_t Need, size_t Unit)
{
	xredisreply* Reply = Reader->Partial;
	size_t Available = (Reader->Config.MaxOwnedBytes - Reply->Reserved) / Unit;
	if (Need > Available) {
		xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply peak byte limit exceeded"); return 0;
	}
	size_t Capacity = Old <= SIZE_MAX / 2u ? Old * 2u : SIZE_MAX;
	if (Capacity < Need) Capacity = Need;
	if (Capacity > Available) Capacity = Available;
	return Capacity;
}

static void xredisReplyPeak(xredisreplyreader* Reader, size_t Extra)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Reserved + Extra > Reply->Peak) Reply->Peak = Reply->Reserved + Extra;
	Reader->Peak = Reply->Peak;
}

static bool xredisReplyNodes(xredisreplyreader* Reader)
{
	xredisreply* Reply = Reader->Partial;
	if (Reply->Count < Reply->NodeCapacity) return true;
	if (Reply->Count == SIZE_MAX)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned node count overflows");
	size_t Capacity = xredisReplyGrowth(Reader, Reply->NodeCapacity, Reply->Count + 1u, sizeof(*Reply->Nodes));
	if (Capacity == 0) return false;
	size_t Bytes = Capacity * sizeof(*Reply->Nodes);
	xredisreplynode* Nodes = (xredisreplynode*)xrtMalloc(Bytes);
	if (Nodes == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned node allocation failed");
	xredisReplyPeak(Reader, Bytes);
	memcpy(Nodes, Reply->Nodes, Reply->Count * sizeof(*Nodes));
	if (Reply->Nodes != Reply->InlineNode) {
		xrtFree(Reply->Nodes); Reply->Reserved -= Reply->NodeCapacity * sizeof(*Nodes);
	}
	Reply->Nodes = Nodes; Reply->NodeCapacity = Capacity; Reply->Reserved += Bytes;
	return true;
}

static bool xredisReplyPayload(xredisreplyreader* Reader, size_t Additional)
{
	xredisreply* Reply = Reader->Partial;
	if (Additional > SIZE_MAX - Reply->DataBytes)
		return xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned payload length overflows");
	size_t Need = Reply->DataBytes + Additional;
	if (Need <= Reply->DataCapacity) return true;
	size_t Capacity = xredisReplyGrowth(Reader, Reply->DataCapacity, Need, 1);
	if (Capacity == 0) return false;
	unsigned char* Data = (unsigned char*)xrtMalloc(Capacity);
	if (Data == NULL) return xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned payload allocation failed");
	xredisReplyPeak(Reader, Capacity);
	memcpy(Data, Reply->Data, Reply->DataBytes);
	if (Reply->Data != Reply->InlineData) { xrtFree(Reply->Data); Reply->Reserved -= Reply->DataCapacity; }
	Reply->Data = Data; Reply->DataCapacity = Capacity; Reply->Reserved += Capacity;
	return true;
}

static void xredisReplyLevels(xredisreplyreader* Reader)
{
	Reader->Levels[0] = (xredisreplylevel){XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
}

static xredisreadaction xredisReplyCollect(const xredisreadevent* Event, ptr User)
{
	xredisreplyreader* Reader = (xredisreplyreader*)User;
	xredisreplylevel* Level = &Reader->Levels[Event->Depth];
	if (Reader->Partial == NULL) {
		Reader->Peak = 0;
		if (sizeof(xredisreply) > Reader->Config.MaxOwnedBytes) {
			xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned reply header exceeds byte limit"); return XREDIS_READ_FAIL;
		}
		xredisreply* Reply = (xredisreply*)xrtCalloc(1, sizeof(*Reply));
		if (Reply == NULL) {
			xredisReplyFail(Reader, XERR_MEMORY, XREDIS_REPLY_MEMORY, "owned reply allocation failed"); return XREDIS_READ_FAIL;
		}
		xrtAtomic32Init(&Reply->References, 1); Reply->Nodes = Reply->InlineNode; Reply->NodeCapacity = 1;
		Reply->Data = Reply->InlineData; Reply->DataCapacity = sizeof(Reply->InlineData); Reply->Root = XREDIS_REPLY_NONE;
		Reply->Reserved = sizeof(*Reply); Reply->Peak = sizeof(*Reply); Reader->Peak = Reply->Peak;
		Reader->Partial = Reply;
	}
	xredisreply* Reply = Reader->Partial;
	if (Event->Kind == XREDIS_READ_BEGIN) {
		/* A new parent's child level has no pending metadata. Initialize lazily
		 * so a scalar frame never clears the entire configured depth stack. */
		if (Event->Depth < Reader->Config.Reader.MaxDepth)
			Reader->Levels[Event->Depth + 1u] = (xredisreplylevel){XREDIS_REPLY_NONE,
				XREDIS_REPLY_NONE, XREDIS_REPLY_NONE, XREDIS_REPLY_NONE};
		if (!xredisReplyNodes(Reader)) return XREDIS_READ_FAIL;
		size_t Index = Reply->Count++;
		xredisreplynode* Node = &Reply->Nodes[Index];
		*Node = (xredisreplynode){0}; Node->Type = Event->Type; Node->Flags = Event->Flags;
		Node->Offset = Reply->DataBytes; Node->FirstChild = XREDIS_REPLY_NONE;
		Node->Next = XREDIS_REPLY_NONE; Node->Attributes = XREDIS_REPLY_NONE;
		Level->Open = Index; Level->LastChild = XREDIS_REPLY_NONE;
		if (Event->Type != XREDIS_REPLY_ATTRIBUTE) {
			Node->Attributes = Level->PendingFirst;
			Level->PendingFirst = XREDIS_REPLY_NONE; Level->PendingLast = XREDIS_REPLY_NONE;
			if (Event->Depth == 0) Reply->Root = Index;
			else {
				xredisreplylevel* ParentLevel = &Reader->Levels[Event->Depth - 1u];
				xredisreplynode* Parent = &Reply->Nodes[ParentLevel->Open];
				if (ParentLevel->LastChild == XREDIS_REPLY_NONE) Parent->FirstChild = Index;
				else Reply->Nodes[ParentLevel->LastChild].Next = Index;
				ParentLevel->LastChild = Index; ++Parent->ChildCount;
			}
		}
		if (!(Event->Flags & XREDIS_REPLY_NULL_VALUE) && Event->Length != XREDIS_LENGTH_UNKNOWN &&
			(Event->Type == XREDIS_REPLY_BULK_STRING || Event->Type == XREDIS_REPLY_BULK_ERROR || Event->Type == XREDIS_REPLY_VERBATIM)) {
			if (Event->Length > (uint64)SIZE_MAX) {
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "owned bulk length overflows size_t"); return XREDIS_READ_FAIL;
			}
			if (!xredisReplyPayload(Reader, (size_t)Event->Length)) return XREDIS_READ_FAIL;
		}
	} else if (Event->Kind == XREDIS_READ_DATA) {
		if (!xredisReplyPayload(Reader, Event->Data.Size)) return XREDIS_READ_FAIL;
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		memcpy(Reply->Data + Reply->DataBytes, Event->Data.Data, Event->Data.Size);
		Reply->DataBytes += Event->Data.Size; Node->Bytes += Event->Data.Size;
	} else {
		xredisreplynode* Node = &Reply->Nodes[Level->Open];
		Node->Length = Event->Length; Node->Integer = Event->Integer; Node->Boolean = Event->Boolean;
		if (Event->Type == XREDIS_REPLY_ATTRIBUTE) {
			if (Level->PendingLast == XREDIS_REPLY_NONE) Level->PendingFirst = Level->Open;
			else Reply->Nodes[Level->PendingLast].Next = Level->Open;
			Level->PendingLast = Level->Open;
		}
		Level->Open = XREDIS_REPLY_NONE;
	}
	return XREDIS_READ_NEXT;
}

XREDIS_API bool xredisReplyReaderCreate(const xredisreplyreaderconfig* Config, xredisreplyreader** Result)
{
	xredisreplyreaderconfig Copy;
	if (Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader creation arguments");
	if (Config == NULL) xredisReplyReaderConfigInit(&Copy); else Copy = *Config;
	if (Copy.Size != sizeof(Copy) || Copy.MaxOwnedBytes == 0 ||
		Copy.Reader.MaxDepth >= (SIZE_MAX - sizeof(xredisreplyreader)) / sizeof(xredisreplylevel))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader size or limits");
	size_t Bytes = sizeof(xredisreplyreader) + (Copy.Reader.MaxDepth + 1u) * sizeof(xredisreplylevel);
	xredisreplyreader* Reader = (xredisreplyreader*)xrtCalloc(1, Bytes);
	if (Reader == NULL) return false;
	Reader->Config = Copy; Reader->StorageBytes = Bytes;
	if (!xredisReaderCreate(&Copy.Reader, xredisReplyCollect, Reader, &Reader->Parser)) { xrtFree(Reader); return false; }
	xredisReplyLevels(Reader); *Result = Reader;
	return true;
}

XREDIS_API void xredisReplyReaderDestroy(xredisreplyreader* Reader)
{
	if (Reader == NULL) return;
	xredisReplyRelease(Reader->Partial); xredisReaderDestroy(Reader->Parser); xrtFree(Reader);
}

XREDIS_API bool xredisReplyReaderReset(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	Reader->Error = XREDIS_REPLY_OK; Reader->ErrorKind = XERR_NONE; Reader->ErrorMessage = NULL; Reader->Peak = 0;
	xredisReplyLevels(Reader);
	return xredisReaderReset(Reader->Parser);
}

XREDIS_API bool xredisReplyReaderInfo(const xredisreplyreader* Reader, xredisreplyreaderinfo* Info)
{
	if (Reader == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xredisReplyReaderOverlap(Reader, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply reader info output or size");
	xredisreplyreaderinfo Copy = {0}; Copy.Size = sizeof(Copy); Copy.Reader.Size = sizeof(Copy.Reader);
	if (!xredisReaderInfo(Reader->Parser, &Copy.Reader)) return false;
	Copy.Error = Reader->Error; Copy.PeakOwnedBytes = Reader->Peak;
	if (Reader->Partial != NULL) {
		Copy.PartialNodes = Reader->Partial->Count; Copy.PartialPayloadBytes = Reader->Partial->DataBytes;
		Copy.PartialReservedBytes = Reader->Partial->Reserved;
	}
	*Info = Copy; return true;
}

static bool xredisReplyParserFailure(xredisreplyreader* Reader)
{
	if (Reader->Error == XREDIS_REPLY_OK) {
		xredisreaderinfo Info = {0}; Info.Size = sizeof(Info); xredisReaderInfo(Reader->Parser, &Info);
		switch (Info.Error) {
			case XREDIS_READER_LIMIT:
				xredisReplyFail(Reader, XERR_RANGE, XREDIS_REPLY_LIMIT, "RESP reader limit exceeded"); break;
			case XREDIS_READER_TRUNCATED:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_TRUNCATED, "incomplete RESP reply at EOF"); break;
			case XREDIS_READER_PROTOCOL:
				xredisReplyFail(Reader, XERR_PROTOCOL, XREDIS_REPLY_PROTOCOL, "invalid RESP reply"); break;
			default:
				xredisReplyFail(Reader, XERR_STATE, XREDIS_REPLY_STATE, "unexpected RESP reader state"); break;
		}
	}
	xredisReplyRelease(Reader->Partial); Reader->Partial = NULL;
	return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
}

XREDIS_API xredisreplystatus xredisReplyReaderFeed(xredisreplyreader* Reader,
	xbytesview Input, size_t* Consumed, xredisreply** Result)
{
	if (Reader == NULL || Consumed == NULL || Result == NULL ||
		!xrtMemRangeValid(Consumed, sizeof(*Consumed)) || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Input.Data, Input.Size) ||
		xredisReplyReaderOverlap(Reader, Consumed, sizeof(*Consumed)) ||
		xredisReplyReaderOverlap(Reader, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Consumed, sizeof(*Consumed)) ||
		xrtMemRangesOverlap(Input.Data, Input.Size, Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Consumed, sizeof(*Consumed), Result, sizeof(*Result))) {
		xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "invalid reply input, outputs or overlap"); return XREDIS_REPLY_ERROR;
	}
	if (Reader->Error != XREDIS_REPLY_OK) {
		*Consumed = 0; xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage); return XREDIS_REPLY_ERROR;
	}
	xredisreadstatus Status = xredisReaderFeed(Reader->Parser, Input, Consumed);
	if (Status == XREDIS_READER_ERROR) { xredisReplyParserFailure(Reader); return XREDIS_REPLY_ERROR; }
	if (Status == XREDIS_READER_READY) {
		*Result = Reader->Partial; Reader->Partial = NULL; xredisReplyLevels(Reader); return XREDIS_REPLY_READY;
	}
	return Status == XREDIS_READER_YIELD ? XREDIS_REPLY_YIELD : XREDIS_REPLY_MORE;
}

XREDIS_API bool xredisReplyReaderFinish(xredisreplyreader* Reader)
{
	if (Reader == NULL) return xredisReplyReport(XERR_ARGUMENT, XREDIS_REPLY_ARGUMENT, "reply reader is NULL");
	if (Reader->Error != XREDIS_REPLY_OK)
		return xredisReplyReport(Reader->ErrorKind, Reader->Error, Reader->ErrorMessage);
	if (!xredisReaderFinish(Reader->Parser)) return xredisReplyParserFailure(Reader);
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/client/client.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)

typedef struct xredisrequest xredisrequest;
typedef struct xredisinitwire xredisinitwire;
enum { XREDIS_R_SUBMITTED, XREDIS_R_PENDING, XREDIS_R_SENT, XREDIS_R_DONE };
enum { XREDIS_I_HELLO, XREDIS_I_AUTH, XREDIS_I_NAME, XREDIS_I_SELECT, XREDIS_I_END };

struct xredisinitwire {
	xatomic32 References;
	xredisclient* Client;
	size_t Size;
	unsigned char Data[];
};

struct xredisrequest {
	xatomic32 References, Cancelled;
	xredisclient* Client;
	xpromise* Promise;
	xcancelwatch* Watch;
	xredisrequestresult* Result;
	xredisrequest *Next, *Previous, *SubmitNext, *CancelNext;
	size_t WireBytes;
	double Deadline;
	uint64 Timer;
	unsigned Phase;
	bool CancelQueued, Sensitive;
	unsigned char Wire[];
};

struct xredisclient {
	xatomic32 References;
	xmutex Gate;
	xnetengine* Engine;
	xnetworker* Worker;
	xnetstream* Stream;
	xnetaddr Remote;
	xredisclientconfig Config;
	xredisreplyreader* Reader;
	xpromise* ClosePromise;
	xfuture* CloseFuture;
	xpromise* ReadyPromise;
	xfuture* ReadyFuture;
	xredisreadyresult* ReadyResult;
	xredisreply* Hello;
	xredisinitwire* InitWire;
	xredisinitmode InitMode;
	uint32 InitFlags;
	size_t InitOffset[XREDIS_I_END], InitLength[XREDIS_I_END], InitNext, InitAwait;
	double InitDeadline;
	uint64 InitTimer;
	xnetpost Wake;
	xredisrequest *SubmitHead, *SubmitTail, *CancelHead, *CancelTail;
	xredisrequest *PendingHead, *PendingTail, *SentHead, *SentTail;
	xredisclientinfo Info;
	xerror* CloseCause;
	size_t StorageBytes, SentCount, PushHead;
	bool WakeQueued, CloseDone, EnginePinned;
	/* Only the owning worker accesses these fields. */
	bool Started, TransportClosed, ReadEnded, Reading, ReadYielded, ReadPaused;
	bool Managed, TransportOpen;
	xredisreply* PushQueue[];
};

static void xredisClientWork(xnetworker* Worker, ptr Data);
static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data);
static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);
static void xredisClientRef(xredisclient* Client);
static bool xredisClientWakeLocked(xredisclient* Client);
static void xredisClientWake(xredisclient* Client);
static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause);
static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause);
static void xredisClientInitPump(xredisclient* Client);
static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root);

static bool xredisClientReport(xerrkind Kind, xredisclienterror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.redis", (int32)Code, Message); return false;
}

static double xredisClientDeadline(double Now, int64 Timeout)
{
	if (Timeout == XRT_WAIT_FOREVER) return INFINITY;
	if (Timeout == 0) return Now;
	return nextafter(Now + (double)(Timeout / 1000) + (double)(Timeout % 1000) * 0.001, INFINITY);
}

static void xredisInitWireRelease(xredisinitwire* Wire)
{
	if (Wire == NULL || xrtAtomic32FetchSub(&Wire->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Wire->Client;
	xrtSecureZero(Wire->Data, Wire->Size); xrtFree(Wire);
	if (Client != NULL) {
		xrtMutexLock(&Client->Gate); Client->Info.InitBytes = 0;
		if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
		xrtMutexUnlock(&Client->Gate); xredisClientRelease(Client);
	}
}

static void xredisInitWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisInitWireRelease((xredisinitwire*)Data); }

static void xredisClientReadyDrop(ptr Data, ptr Context)
{
	(void)Context; xredisreadyresult* Result = (xredisreadyresult*)Data;
	xredisReplyRelease(Result->Hello); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

XREDIS_API bool xredisClientInitConfigInit(xredisclientinitconfig* Init)
{
	if (Init == NULL || !xrtMemRangeValid(Init, sizeof(*Init)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization config output");
	*Init = (xredisclientinitconfig){0}; Init->Size = sizeof(*Init); Init->Mode = XREDIS_INIT_RESP3;
	Init->Timeout = 5000; Init->MaxWireBytes = 64u * 1024u; return true;
}

static bool xredisClientInitValid(const xredisclientinitconfig* Init, const xredisclientconfig* Config)
{
	const uint32 Known = XREDIS_INIT_AUTH | XREDIS_INIT_USERNAME | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
	if (Init->Size != sizeof(*Init) || (Init->Mode != XREDIS_INIT_AUTO && Init->Mode != XREDIS_INIT_RESP2 && Init->Mode != XREDIS_INIT_RESP3) ||
		(Init->Flags & ~Known) != 0 || Init->Timeout < XRT_WAIT_FOREVER || Init->MaxWireBytes == 0 ||
		((Init->Flags & XREDIS_INIT_USERNAME) && !(Init->Flags & XREDIS_INIT_AUTH)) ||
		(!(Init->Flags & XREDIS_INIT_AUTH) && Init->Password.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_USERNAME) && Init->Username.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SETNAME) && Init->Name.Size != 0) ||
		(!(Init->Flags & XREDIS_INIT_SELECT) && Init->Database != 0) ||
		(Init->Mode != XREDIS_INIT_RESP2 && Config->Replies.Reader.Protocol != XREDIS_RESP3))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization mode, flags, limits or reader");
	const xbytesview Views[] = {Init->Username, Init->Password, Init->Name};
	for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
		if ((Views[Index].Data == NULL && Views[Index].Size != 0) || !xrtMemRangeValid(Views[Index].Data, Views[Index].Size))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization input span");
	return true;
}

static bool xredisClientInitBuild(xredisclient* Client, const xredisclientinitconfig* Init)
{
	xbytesview Args[XREDIS_I_END][7]; size_t Counts[XREDIS_I_END] = {0};
	char Database[32]; size_t DatabaseBytes = 0;
	if (Init->Flags & XREDIS_INIT_SELECT) {
		if (!xrtUIntWrite(Init->Database, 10, Database, sizeof(Database), &DatabaseBytes, 0)) return false;
	}
	if (Init->Mode != XREDIS_INIT_RESP2) {
		size_t N = 0; Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("HELLO");
		Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("3");
		if (Init->Flags & XREDIS_INIT_AUTH) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			Args[XREDIS_I_HELLO][N++] = (Init->Flags & XREDIS_INIT_USERNAME) ? Init->Username : (xbytesview)XRT_BYTES_LITERAL("default");
			Args[XREDIS_I_HELLO][N++] = Init->Password;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_HELLO][N++] = (xbytesview)XRT_BYTES_LITERAL("SETNAME"); Args[XREDIS_I_HELLO][N++] = Init->Name;
		}
		Counts[XREDIS_I_HELLO] = N;
	}
	if (Init->Mode != XREDIS_INIT_RESP3) {
		if (Init->Flags & XREDIS_INIT_AUTH) {
			size_t N = 0; Args[XREDIS_I_AUTH][N++] = (xbytesview)XRT_BYTES_LITERAL("AUTH");
			if (Init->Flags & XREDIS_INIT_USERNAME) Args[XREDIS_I_AUTH][N++] = Init->Username;
			Args[XREDIS_I_AUTH][N++] = Init->Password; Counts[XREDIS_I_AUTH] = N;
		}
		if (Init->Flags & XREDIS_INIT_SETNAME) {
			Args[XREDIS_I_NAME][0] = (xbytesview)XRT_BYTES_LITERAL("CLIENT"); Args[XREDIS_I_NAME][1] = (xbytesview)XRT_BYTES_LITERAL("SETNAME");
			Args[XREDIS_I_NAME][2] = Init->Name; Counts[XREDIS_I_NAME] = 3;
		}
	}
	if (Init->Flags & XREDIS_INIT_SELECT) {
		Args[XREDIS_I_SELECT][0] = (xbytesview)XRT_BYTES_LITERAL("SELECT");
		Args[XREDIS_I_SELECT][1] = (xbytesview){(cbytes)Database, DatabaseBytes}; Counts[XREDIS_I_SELECT] = 2;
	}
	size_t Total = 0;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0) {
		size_t Bytes = 0;
		if (!xredisCommandSize(Args[Index], Counts[Index], &Bytes)) return false;
		if (Bytes > Client->Config.Stream.WriteLimit || Bytes > Init->MaxWireBytes - Total ||
			Bytes > SIZE_MAX - sizeof(xredisinitwire) - Total)
			return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "initialization encoded wire exceeds configured limits");
		Client->InitOffset[Index] = Total; Client->InitLength[Index] = Bytes; Total += Bytes;
	}
	Client->InitNext = Init->Mode == XREDIS_INIT_RESP2 ? XREDIS_I_AUTH : XREDIS_I_HELLO;
	Client->InitAwait = XREDIS_I_END;
	if (Total == 0) return true;
	xredisinitwire* Wire = (xredisinitwire*)xrtMalloc(sizeof(*Wire) + Total);
	if (Wire == NULL) return false;
	xrtAtomic32Init(&Wire->References, 1); Wire->Client = NULL; Wire->Size = Total;
	for (size_t Index = 0; Index < XREDIS_I_END; ++Index) if (Counts[Index] != 0 &&
		!xredisCommandWrite(Wire->Data + Client->InitOffset[Index], Client->InitLength[Index], Args[Index], Counts[Index], NULL)) {
		xredisInitWireRelease(Wire); return false;
	}
	Wire->Client = Client; xredisClientRef(Client); Client->InitWire = Wire; Client->Info.InitBytes = Total;
	return true;
}

static void xredisClientRef(xredisclient* Client)
{ (void)xrtAtomic32FetchAdd(&Client->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisClientRetain(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	uint32 Count = xrtAtomic32Load(&Client->References, XMEMORY_RELAXED);
	for (;;) {
		/* Reserve the upper half for bounded private request/control references. */
		if (Count >= UINT32_MAX / 2u) return xredisClientReport(XERR_RANGE, XREDIS_CLIENT_STATE, "client reference count limit");
		if (xrtAtomic32CompareExchange(&Client->References, &Count, Count + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}

XREDIS_API void xredisClientRelease(xredisclient* Client)
{
	if (Client == NULL || xrtAtomic32FetchSub(&Client->References, 1, XMEMORY_ACQ_REL) != 1) return;
	for (size_t Index = 0; Index < Client->Config.MaxPushes; ++Index) xredisReplyRelease(Client->PushQueue[Index]);
	xredisReplyReaderDestroy(Client->Reader); xrtNetStreamDestroy(Client->Stream);
	xrtPromiseDestroy(Client->ClosePromise); xrtFutureDestroy(Client->CloseFuture); xrtErrorFree(Client->CloseCause);
	xrtPromiseDestroy(Client->ReadyPromise); xrtFutureDestroy(Client->ReadyFuture);
	xrtFree(Client->ReadyResult); xredisReplyRelease(Client->Hello);
	if (Client->EnginePinned) xrtNetEngineUnpin(Client->Engine);
	xrtMutexUnit(&Client->Gate); xrtFree(Client);
}

/* Gate is held. One reference protects each accepted embedded-post callback. */
static bool xredisClientWakeLocked(xredisclient* Client)
{
	if (Client->CloseDone || Client->WakeQueued) return true;
	Client->WakeQueued = true; xredisClientRef(Client);
	if (xrtNetPost(Client->Worker, &Client->Wake, xredisClientWork, Client)) return true;
	Client->WakeQueued = false; xredisClientRelease(Client); return false;
}

static void xredisClientWake(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); (void)xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
}

static bool xredisClientClosing(xredisclient* Client)
{
	xrtMutexLock(&Client->Gate); bool Closing = Client->Info.State >= XREDIS_CLIENT_CLOSING;
	xrtMutexUnlock(&Client->Gate); return Closing;
}

/* A connection retired by ANOTHER request's drain grace must not report that
 * this request's own deadline expired. CloseStatus keeps the connection cause. */
static xredisrequeststatus xredisClientFailureStatus(xredisclient* Client)
{ return Client->Info.CloseStatus == XREDIS_REQUEST_TIMEOUT ? XREDIS_REQUEST_DISCONNECTED : Client->Info.CloseStatus; }

static void xredisRequestRef(xredisrequest* Request)
{ (void)xrtAtomic32FetchAdd(&Request->References, 1, XMEMORY_RELAXED); }

static void xredisRequestRelease(xredisrequest* Request)
{
	if (xrtAtomic32FetchSub(&Request->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Request->Client;
	size_t WireBytes = Request->WireBytes;
	/* The reservation protects actual storage, including the free tail. Keep
	 * the independent Client reference while dropping it AFTER the buffer. */
	if (Request->Sensitive) xrtSecureZero(Request->Wire, WireBytes);
	xrtFree(Request);
	xrtMutexLock(&Client->Gate);
	--Client->Info.Requests; Client->Info.CommandBytes -= WireBytes;
	if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
	xrtMutexUnlock(&Client->Gate);
	xredisClientRelease(Client);
}

static void xredisClientWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisRequestRelease((xredisrequest*)Data); }

static void xredisClientResultDrop(ptr Value, ptr Data)
{
	(void)Data; xredisrequestresult* Result = (xredisrequestresult*)Value;
	xredisReplyRelease(Result->Reply); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

static void xredisClientCancel(ptr Data)
{
	xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	xrtAtomic32Store(&Request->Cancelled, 1, XMEMORY_RELEASE);
	xrtMutexLock(&Client->Gate);
	if (!Request->CancelQueued && !Client->CloseDone) {
		Request->CancelQueued = true; Request->CancelNext = NULL; xredisRequestRef(Request);
		if (Client->CancelTail != NULL) Client->CancelTail->CancelNext = Request; else Client->CancelHead = Request;
		Client->CancelTail = Request; (void)xredisClientWakeLocked(Client);
	}
	xrtMutexUnlock(&Client->Gate);
}

static void xredisClientComplete(xredisrequest* Request, xredisrequeststatus Status,
	xredisdelivery Delivery, xredisreply* Reply, const xerror* Cause)
{
	if (Request->Promise == NULL) { xredisReplyRelease(Reply); return; }
	/* No Gate is held. Our cancellation callback only queues/wakes and never
	 * waits for this worker, so physical Unwatch retirement cannot self-deadlock. */
	xrtCancelUnwatch(Request->Watch); Request->Watch = NULL;
	xpromise* Promise = Request->Promise; Request->Promise = NULL;
	xredisrequestresult* Result = Request->Result; Request->Result = NULL;
	*Result = (xredisrequestresult){Status, Delivery, Reply, xrtErrorRef(Cause)};
	xrtMutexLock(&Request->Client->Gate); ++Request->Client->Info.Completed; xrtMutexUnlock(&Request->Client->Gate);
	if (!xrtPromiseResolveOwned(Promise, Result, xredisClientResultDrop, NULL)) xredisClientResultDrop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientCancelTimer(xredisrequest* Request)
{
	uint64 Timer = Request->Timer;
	if (Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Request->Client->Engine, Timer);
}

static bool xredisClientArm(xredisrequest* Request, double Deadline)
{
	if (Deadline == INFINITY) return true;
	double Milliseconds = ceil((Deadline - xrtTimer()) * 1000.0);
	int64 Timeout = Milliseconds <= 0 ? 0 : Milliseconds >= 0x1p63 ? INT64_MAX : (int64)Milliseconds;
	xredisRequestRef(Request);
	Request->Timer = xrtNetEngineAfter(Request->Client->Engine, xrtNetWorkerIndex(Request->Client->Worker),
		Timeout, xredisClientTimer, Request);
	if (Request->Timer != 0) return true;
	xredisRequestRelease(Request); return false;
}

static void xredisClientUnlink(xredisrequest* Request)
{
	xredisclient* Client = Request->Client;
	xredisrequest** Head = Request->Phase == XREDIS_R_SENT ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Request->Phase == XREDIS_R_SENT ? &Client->SentTail : &Client->PendingTail;
	if (Request->Previous != NULL) Request->Previous->Next = Request->Next; else *Head = Request->Next;
	if (Request->Next != NULL) Request->Next->Previous = Request->Previous; else *Tail = Request->Previous;
	if (Request->Phase == XREDIS_R_SENT) --Client->SentCount;
	Request->Previous = NULL; Request->Next = NULL;
}

static void xredisClientAppend(xredisclient* Client, xredisrequest* Request, bool Sent)
{
	xredisrequest** Head = Sent ? &Client->SentHead : &Client->PendingHead;
	xredisrequest** Tail = Sent ? &Client->SentTail : &Client->PendingTail;
	Request->Previous = *Tail; Request->Next = NULL;
	if (*Tail != NULL) (*Tail)->Next = Request; else *Head = Request;
	*Tail = Request; Request->Phase = Sent ? XREDIS_R_SENT : XREDIS_R_PENDING;
	if (Sent) ++Client->SentCount;
}

static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause)
{
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = Status;
		Client->CloseCause = xrtErrorRef(Cause);
	}
	xrtMutexUnlock(&Client->Gate);
	if (Client->Stream != NULL) xrtNetStreamAbort(Client->Stream); else Client->TransportClosed = true;
	xredisClientWake(Client);
}

static xredisreadystatus xredisClientReadyFailure(xredisrequeststatus Status)
{
	switch (Status) {
		case XREDIS_REQUEST_CLOSED: return XREDIS_READY_CLOSED;
		case XREDIS_REQUEST_TIMEOUT: return XREDIS_READY_TIMEOUT;
		case XREDIS_REQUEST_PROTOCOL: return XREDIS_READY_PROTOCOL;
		case XREDIS_REQUEST_LIMIT: return XREDIS_READY_LIMIT;
		case XREDIS_REQUEST_MEMORY: return XREDIS_READY_MEMORY;
		case XREDIS_REQUEST_INITIALIZATION: return XREDIS_READY_SERVER_ERROR;
		default: return XREDIS_READY_DISCONNECTED;
	}
}

static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause)
{
	if (Client->ReadyPromise == NULL) return;
	xpromise* Promise = Client->ReadyPromise; Client->ReadyPromise = NULL;
	xredisreadyresult* Result = Client->ReadyResult; Client->ReadyResult = NULL;
	xrtMutexLock(&Client->Gate);
	if (Status == XREDIS_READY_OK && Client->Info.State >= XREDIS_CLIENT_CLOSING)
		Status = xredisClientReadyFailure(Client->Info.CloseStatus);
	if (Status == XREDIS_READY_OK) {
		Client->Info.Ready = true; Client->Info.State = XREDIS_CLIENT_OPEN; Client->Info.InitStep = XREDIS_INIT_DONE;
	}
	*Result = (xredisreadyresult){Status, Client->Info.InitStep, Client->Info.Protocol,
		Client->Info.Negotiated, Client->Info.Downgraded, NULL, xrtErrorRef(Cause)};
	xrtMutexUnlock(&Client->Gate);
	if (Status == XREDIS_READY_OK) Result->Hello = Client->Hello;
	else xredisReplyRelease(Client->Hello);
	Client->Hello = NULL;
	if (Client->InitTimer != 0) (void)xrtNetEngineTimerCancelCurrent(Client->Engine, Client->InitTimer);
	xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
	if (!xrtPromiseResolveOwned(Promise, Result, xredisClientReadyDrop, NULL)) xredisClientReadyDrop(Result, NULL);
	xrtPromiseDestroy(Promise);
}

static void xredisClientInitReject(xredisclient* Client, xredisreadystatus Status)
{
	xerrkind Kind = Status == XREDIS_READY_AUTH_FAILED ? XERR_PERMISSION :
		Status == XREDIS_READY_UNSUPPORTED ? XERR_UNSUPPORTED : Status == XREDIS_READY_TIMEOUT ? XERR_TIMEOUT :
		Status == XREDIS_READY_PROTOCOL ? XERR_PROTOCOL : XERR_VALUE;
	/* Deliberately resident/redacted diagnostics: an unknown-command response
	 * can echo HELLO AUTH arguments. Never wrap/copy that server text. */
	xerror* Cause = xrtErrorCreate(Kind, "xrt.redis", XREDIS_CLIENT_INITIALIZATION_FAILED, "connection initialization failed");
	xredisClientFail(Client, XREDIS_REQUEST_INITIALIZATION, Cause);
	xredisClientReadyFinish(Client, Status, Cause); xrtErrorFree(Cause);
}

static void xredisClientInitTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
	if (Client->InitTimer == Id) Client->InitTimer = 0;
	if (Client->ReadyPromise != NULL) {
		if (Result == XNET_RESULT_OK) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
		else if (Result != XNET_RESULT_CANCELLED) xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	}
	xredisClientWake(Client); xredisClientRelease(Client);
}

static bool xredisClientInitArm(xredisclient* Client)
{
	if (!Client->Managed || Client->InitDeadline == INFINITY) return true;
	double Remaining = ceil((Client->InitDeadline - xrtTimer()) * 1000.0);
	int64 Timeout = Remaining <= 0 ? 0 : Remaining >= 0x1p63 ? INT64_MAX : (int64)Remaining;
	xredisClientRef(Client);
	Client->InitTimer = xrtNetEngineAfter(Client->Engine, xrtNetWorkerIndex(Client->Worker), Timeout, xredisClientInitTimer, Client);
	if (Client->InitTimer != 0) return true;
	xredisClientRelease(Client); return false;
}

static void xredisClientInitPump(xredisclient* Client)
{
	if (Client->ReadyPromise == NULL || !Client->TransportOpen || Client->InitAwait != XREDIS_I_END) return;
	if (xrtTimer() >= Client->InitDeadline) { xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
	while (Client->InitNext < XREDIS_I_END && Client->InitLength[Client->InitNext] == 0) ++Client->InitNext;
	if (Client->InitNext == XREDIS_I_END) { xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL); return; }
	size_t Index = Client->InitNext;
	const xredisinitstep Steps[] = {XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE, XREDIS_INIT_NAME, XREDIS_INIT_DATABASE};
	xrtMutexLock(&Client->Gate); Client->Info.InitStep = Steps[Index]; xrtMutexUnlock(&Client->Gate);
	xredisinitwire* Wire = Client->InitWire;
	Client->InitAwait = Index; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
	xnetresult Result = xrtNetStreamSendRef(Client->Stream, Wire->Data + Client->InitOffset[Index], Client->InitLength[Index], xredisInitWireDrop, Wire);
	if (Result == XNET_RESULT_OK) return;
	Client->InitAwait = XREDIS_I_END; xredisInitWireRelease(Wire);
	if (Result == XNET_RESULT_AGAIN) return;
	xredisClientFail(Client, Result == XNET_RESULT_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
		XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
}

static bool xredisClientInitLiteral(xbytesview Bytes, cstr Text)
{ size_t Length = strlen(Text); return Bytes.Size == Length && (Length == 0 || memcmp(Bytes.Data, Text, Length) == 0); }

static bool xredisClientInitCode(xbytesview Bytes, cstr Code)
{
	size_t Length = strlen(Code);
	return Bytes.Size >= Length && memcmp(Bytes.Data, Code, Length) == 0 && (Bytes.Size == Length || Bytes.Data[Length] == ' ');
}

static bool xredisClientInitUnsupported(xbytesview Bytes)
{
	if (xredisClientInitCode(Bytes, "NOPROTO")) return true;
	const cstr Prefix = "ERR unknown command 'HELLO'"; size_t Length = strlen(Prefix);
	if (Bytes.Size < Length || (Bytes.Size > Length && Bytes.Data[Length] != ',' && Bytes.Data[Length] != ' ')) return false;
	for (size_t Index = 0; Index < Length; ++Index) {
		unsigned char A = Bytes.Data[Index], B = (unsigned char)Prefix[Index];
		if (A >= 'a' && A <= 'z') A -= 'a' - 'A';
		if (B >= 'a' && B <= 'z') B -= 'a' - 'A';
		if (A != B) return false;
	}
	return true;
}

static bool xredisClientInitHello(xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (Root->Type != XREDIS_REPLY_MAP || (Root->Flags & XREDIS_REPLY_NULL_VALUE)) return false;
	uint32 Seen = 0;
	for (size_t Index = Root->FirstChild; Index != XREDIS_REPLY_NONE;) {
		xredisreplyvalue Key, Value;
		if (!xredisReplyGet(Reply, Index, &Key) || Key.Next == XREDIS_REPLY_NONE || !xredisReplyGet(Reply, Key.Next, &Value)) return false;
		if ((Key.Type == XREDIS_REPLY_SIMPLE_STRING || Key.Type == XREDIS_REPLY_BULK_STRING) && !(Key.Flags & XREDIS_REPLY_NULL_VALUE)) {
			uint32 Bit = xredisClientInitLiteral(Key.Data, "proto") ? 1u : xredisClientInitLiteral(Key.Data, "server") ? 2u :
				xredisClientInitLiteral(Key.Data, "version") ? 4u : 0;
			if (Bit != 0) {
				if ((Seen & Bit) || (Bit == 1u ? Value.Type != XREDIS_REPLY_INTEGER || Value.Integer != 3 :
					(Value.Type != XREDIS_REPLY_SIMPLE_STRING && Value.Type != XREDIS_REPLY_BULK_STRING) || (Value.Flags & XREDIS_REPLY_NULL_VALUE))) return false;
				Seen |= Bit;
			}
		}
		Index = Value.Next;
	}
	return Seen == 7u;
}

static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root)
{
	if (xrtTimer() >= Client->InitDeadline) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
	size_t Index = Client->InitAwait;
	if (Index >= XREDIS_I_END) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR) {
		bool Unsupported = Index == XREDIS_I_HELLO && xredisClientInitUnsupported(Root->Data);
		bool Authentication = xredisClientInitCode(Root->Data, "WRONGPASS") ||
			xredisClientInitCode(Root->Data, "NOAUTH") ||
			((Index == XREDIS_I_HELLO || Index == XREDIS_I_AUTH) && (Client->InitFlags & XREDIS_INIT_AUTH) &&
			 xredisClientInitLiteral(Root->Data, "ERR invalid password"));
		xredisReplyRelease(Reply);
		if (Unsupported && Client->InitMode == XREDIS_INIT_AUTO) {
			xrtMutexLock(&Client->Gate); Client->Info.Downgraded = true; Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
			Client->InitNext = XREDIS_I_AUTH; Client->InitAwait = XREDIS_I_END; xredisClientWake(Client); return;
		}
		xredisClientInitReject(Client, Unsupported ? XREDIS_READY_UNSUPPORTED : Authentication ? XREDIS_READY_AUTH_FAILED : XREDIS_READY_SERVER_ERROR);
		return;
	}
	bool Valid = Index == XREDIS_I_HELLO ? xredisClientInitHello(Reply, Root) :
		Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "OK");
	if (!Valid) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
	if (Index == XREDIS_I_HELLO) {
		Client->Hello = Reply;
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP3; Client->Info.Negotiated = true; xrtMutexUnlock(&Client->Gate);
		Client->InitNext = XREDIS_I_SELECT;
	} else { xredisReplyRelease(Reply); Client->InitNext = Index + 1u; }
	Client->InitAwait = XREDIS_I_END; xredisClientWake(Client);
}

static void xredisClientAbandon(xredisrequest* Request, xredisrequeststatus Status)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) return;
	xredisClientComplete(Request, Status, Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT, NULL, NULL);
	xredisClientCancelTimer(Request);
	if (Request->Phase == XREDIS_R_SENT) {
		if (!xredisClientArm(Request, xredisClientDeadline(xrtTimer(), Client->Config.DrainTimeout)))
			xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
	} else if (Request->Phase == XREDIS_R_PENDING) {
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request);
	}
}

static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
	(void)Worker; xredisrequest* Request = (xredisrequest*)Data; xredisclient* Client = Request->Client;
	if (Request->Timer == Id) Request->Timer = 0;
	if (Result == XNET_RESULT_OK) {
		if (Request->Promise != NULL) xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
			XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT);
		else if (Request->Phase == XREDIS_R_SENT) xredisClientFail(Client, XREDIS_REQUEST_TIMEOUT, NULL);
	} else if (Result != XNET_RESULT_CANCELLED && Request->Phase != XREDIS_R_DONE)
		xredisClientFail(Client, XREDIS_REQUEST_CLOSED, NULL);
	xredisClientWake(Client); xredisRequestRelease(Request);
}

static void xredisClientOnOpen(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data;
	Client->TransportOpen = true;
	if (!Client->Managed) xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL);
	else if (Client->InitMode == XREDIS_INIT_RESP2) {
		xrtMutexLock(&Client->Gate); Client->Info.Protocol = XREDIS_RESP2; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientWake(Client);
}

static void xredisClientOnEnd(xnetstream* Stream, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; Client->ReadEnded = true; xredisClientWake(Client);
}

static void xredisClientOnWritable(xnetstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }

static void xredisClientOnLowWater(xnetstream* Stream, size_t Queued, ptr Data)
{ (void)Queued; xredisClientOnWritable(Stream, Data); }

static void xredisClientOnClose(xnetstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; (void)Result; xredisclient* Client = (xredisclient*)Data;
	Client->TransportClosed = true;
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
		Client->Info.State = XREDIS_CLIENT_CLOSING;
		Client->Info.CloseStatus = xrtErrorKind(Error) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED;
		Client->CloseCause = xrtErrorRef(Error);
	}
	xrtMutexUnlock(&Client->Gate); xredisClientWake(Client);
}

static const xnetstreamevents xredisClientEvents = {
	xredisClientOnOpen, xredisClientRead, xredisClientOnEnd, NULL,
	xredisClientOnLowWater, xredisClientOnWritable, xredisClientOnClose
};

static void xredisClientFrame(xredisclient* Client, xredisreply* Reply)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Reply, &Info);
	xredisreplyvalue Root; xredisReplyGet(Reply, Info.Root, &Root);
	if (Root.Type == XREDIS_REPLY_PUSH) {
		xrtMutexLock(&Client->Gate);
		bool Fits = Client->Info.Pushes < Client->Config.MaxPushes &&
			Info.ReservedBytes <= Client->Config.MaxPushBytes - Client->Info.PushBytes;
		if (Fits) {
			size_t Index = (Client->PushHead + Client->Info.Pushes) % Client->Config.MaxPushes;
			Client->PushQueue[Index] = Reply; ++Client->Info.Pushes; Client->Info.PushBytes += Info.ReservedBytes;
		}
		xrtMutexUnlock(&Client->Gate);
		if (!Fits) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_LIMIT, NULL); }
		return;
	}
	if (Client->ReadyPromise != NULL) { xredisClientInitFrame(Client, Reply, &Root); return; }
	xredisrequest* Request = Client->SentHead;
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
	if (Request->Promise != NULL) {
		xredisrequeststatus Status = Root.Type == XREDIS_REPLY_SIMPLE_ERROR || Root.Type == XREDIS_REPLY_BULK_ERROR ?
			XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_OK;
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) {
			xredisReplyRelease(Reply); Reply = NULL;
			xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
		}
		xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	} else {
		xredisReplyRelease(Reply); xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{
	xredisclient* Client = (xredisclient*)Data;
	if (Client->Reading || xredisClientClosing(Client)) return;
	Client->Reading = true; size_t Bytes = 0, Frames = 0;
	while (Bytes < Client->Config.ReadBytes && Frames < Client->Config.ReadFrames && !xredisClientClosing(Client)) {
		xnetspan Span; if (!xrtNetBufFront(Buffer, &Span)) break;
		size_t Available = Client->Config.ReadBytes - Bytes;
		if (Span.Size > Available) Span.Size = Available;
		xredisreply* Reply = NULL; size_t Consumed = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Client->Reader, (xbytesview){Span.Data, Span.Size}, &Consumed, &Reply);
		xrtNetBufConsume(Buffer, Consumed); Bytes += Consumed;
		if (Status == XREDIS_REPLY_ERROR) {
			xredisreplyreaderinfo State = {0}; State.Size = sizeof(State); xredisReplyReaderInfo(Client->Reader, &State);
			xredisrequeststatus Error = State.Error == XREDIS_REPLY_MEMORY ? XREDIS_REQUEST_MEMORY :
				State.Error == XREDIS_REPLY_LIMIT ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_PROTOCOL;
			xredisClientFail(Client, Error, xrtGetError()); break;
		}
		if (Status == XREDIS_REPLY_READY) { ++Frames; xredisClientFrame(Client, Reply); }
		else if (Consumed == 0) break;
	}
	Client->Reading = false;
	if (!xredisClientClosing(Client) && (Bytes == Client->Config.ReadBytes || Frames == Client->Config.ReadFrames)) {
		Client->ReadYielded = true; Client->ReadPaused = true; xrtNetStreamPause(Stream); xredisClientWake(Client);
	}
}

static void xredisClientIngest(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests; ++Work) {
		xrtMutexLock(&Client->Gate); xredisrequest* Request = Client->SubmitHead;
		if (Request != NULL) {
			Client->SubmitHead = Request->SubmitNext;
			if (Client->SubmitHead == NULL) Client->SubmitTail = NULL;
		}
		xrtMutexUnlock(&Client->Gate);
		if (Request == NULL) break;
		if (Request->Promise == NULL) { Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request); continue; }
		if (xredisClientClosing(Client)) {
			xredisClientComplete(Request, xredisClientFailureStatus(Client), XREDIS_NOT_SENT, NULL, Client->CloseCause);
			Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request); continue;
		}
		xredisClientAppend(Client, Request, false);
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
		else if (!xredisClientArm(Request, Request->Deadline)) {
			xredisClientComplete(Request, XREDIS_REQUEST_MEMORY, XREDIS_NOT_SENT, NULL, xrtGetError());
			xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE; xredisRequestRelease(Request);
		}
	}
}

static void xredisClientPump(xredisclient* Client)
{
	if (Client->ReadyPromise != NULL) return;
	for (size_t Work = 0; Work < Client->Config.WorkRequests && Client->PendingHead != NULL &&
		Client->SentCount < Client->Config.MaxInFlight && !xredisClientClosing(Client) &&
		xrtNetStreamState(Client->Stream) == XNET_STREAM_OPEN; ++Work) {
		xredisrequest* Request = Client->PendingHead;
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
			xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
				XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
		}
		xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire, Request->WireBytes, xredisClientWireDrop, Request);
		if (Result == XNET_RESULT_OK) continue;
		xredisRequestRelease(Request); xredisClientUnlink(Request);
		Request->Phase = XREDIS_R_PENDING;
		if (Result == XNET_RESULT_AGAIN) {
			Request->Next = Client->PendingHead;
			if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
			Client->PendingHead = Request; break;
		}
		xredisClientComplete(Request, Result == XNET_RESULT_CLOSED ? XREDIS_REQUEST_DISCONNECTED :
			xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
			XREDIS_NOT_SENT, NULL, Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
		Request->Phase = XREDIS_R_DONE; xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientRetire(xredisclient* Client)
{
	for (size_t Work = 0; Work < Client->Config.WorkRequests &&
		(Client->PendingHead != NULL || Client->SentHead != NULL); ++Work) {
		xredisrequest* Request = Client->PendingHead != NULL ? Client->PendingHead : Client->SentHead;
		xredisdelivery Delivery = Request->Phase == XREDIS_R_SENT ? XREDIS_MAY_HAVE_EXECUTED : XREDIS_NOT_SENT;
		xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
		xredisClientComplete(Request, xredisClientFailureStatus(Client), Delivery, NULL, Client->CloseCause);
		xredisClientCancelTimer(Request); xredisRequestRelease(Request);
	}
}

static void xredisClientWork(xnetworker* Worker, ptr Data)
{
	(void)Worker; xredisclient* Client = (xredisclient*)Data;
	xrtMutexLock(&Client->Gate); Client->WakeQueued = false;
	xredisrequest* Cancel = Client->CancelHead;
	xredisrequest* Last = Cancel;
	for (size_t Count = 1; Last != NULL && Count < Client->Config.WorkRequests && Last->CancelNext != NULL; ++Count)
		Last = Last->CancelNext;
	if (Last != NULL) { Client->CancelHead = Last->CancelNext; Last->CancelNext = NULL; }
	if (Client->CancelHead == NULL) Client->CancelTail = NULL;
	xrtMutexUnlock(&Client->Gate);
	Client->ReadYielded = false;
	while (Cancel != NULL) {
		xredisrequest* Next = Cancel->CancelNext;
		if (Cancel->Phase != XREDIS_R_DONE) xredisClientAbandon(Cancel, XREDIS_REQUEST_CANCELLED);
		xredisRequestRelease(Cancel); Cancel = Next;
	}
	if (!Client->Started) {
		Client->Started = true;
		if (!xredisClientClosing(Client) && Client->Managed) {
			if (xrtTimer() >= Client->InitDeadline) xredisClientInitReject(Client, XREDIS_READY_TIMEOUT);
			else if (!xredisClientInitArm(Client)) xredisClientFail(Client, XREDIS_REQUEST_MEMORY, xrtGetError());
		}
		if (!xredisClientClosing(Client)) {
			Client->Stream = xrtNetStreamConnect(Client->Engine, &Client->Remote, xrtNetWorkerIndex(Client->Worker),
				&Client->Config.Stream, &xredisClientEvents, Client);
			if (Client->Stream == NULL) xredisClientFail(Client, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
				XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, xrtGetError());
		} else Client->TransportClosed = true;
	}
	xredisClientIngest(Client);
	if (!xredisClientClosing(Client) && Client->Stream != NULL) {
		const xnetbuf* Buffer = xrtNetStreamBuffer(Client->Stream);
		if (Buffer != NULL && xrtNetBufSize(Buffer) != 0) xredisClientRead(Client->Stream, (xnetbuf*)Buffer, Client);
		if (!xredisClientClosing(Client) && !Client->ReadEnded && !Client->ReadYielded && Client->ReadPaused) {
			Client->ReadPaused = false; xrtNetStreamResume(Client->Stream);
		}
		if (Client->ReadEnded && !Client->ReadYielded && !xredisClientClosing(Client)) {
			bool Complete = xredisReplyReaderFinish(Client->Reader);
			xredisClientFail(Client, Complete ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_PROTOCOL,
				Complete ? NULL : xrtGetError());
		}
		if (!xredisClientClosing(Client)) {
			if (Client->Managed && Client->ReadyPromise != NULL) xredisClientInitPump(Client);
			else xredisClientPump(Client);
		}
	}
	if (xredisClientClosing(Client)) {
		if (Client->ReadyPromise != NULL)
			xredisClientReadyFinish(Client, xredisClientReadyFailure(Client->Info.CloseStatus), Client->CloseCause);
		if (Client->Stream != NULL && !Client->TransportClosed) xrtNetStreamAbort(Client->Stream);
		xredisClientRetire(Client);
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && Client->Info.InitBytes == 0 &&
		Client->InitTimer == 0 && !Client->WakeQueued && !Client->CloseDone;
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
	if (Done) {
		xpromise* Promise = Client->ClosePromise; Client->ClosePromise = NULL;
		xrtNetStreamDestroy(Client->Stream); Client->Stream = NULL;
		xredisReplyReaderDestroy(Client->Reader); Client->Reader = NULL;
		xrtNetEngineUnpin(Client->Engine); xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise);
		xredisClientRelease(Client); /* Runtime activation. */
	}
	xredisClientRelease(Client); /* This embedded post; last access. */
}

XREDIS_API bool xredisClientConfigInit(xredisclientconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config output");
	*Config = (xredisclientconfig){0}; Config->Size = sizeof(*Config);
	xrtNetStreamConfigInit(&Config->Stream); xredisReplyReaderConfigInit(&Config->Replies);
	Config->MaxRequests = 1024; Config->MaxCommandBytes = 1024u * 1024u;
	if (Config->MaxCommandBytes > Config->Stream.WriteLimit) Config->MaxCommandBytes = Config->Stream.WriteLimit;
	Config->MaxCommandBytesTotal = 8u * 1024u * 1024u; Config->MaxInFlight = 128;
	Config->MaxPushes = 128; Config->MaxPushBytes = 16u * 1024u * 1024u;
	Config->WorkRequests = 128; Config->ReadBytes = 64u * 1024u; Config->ReadFrames = 128; Config->DrainTimeout = 5000;
	return true;
}

static bool xredisClientCreate(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, xredisclient** Result)
{
	xredisclientconfig Copy;
	if (Engine == NULL || Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
		!xrtMemRangeValid(Result, sizeof(*Result)) || xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result)))))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client creation arguments");
	if (Config == NULL) xredisClientConfigInit(&Copy); else Copy = *Config;
	xredisclientinitconfig InitCopy = {0};
	if (Init != NULL) {
		if (!xrtMemRangeValid(Init, sizeof(*Init)) || xrtMemRangesOverlap(Init, sizeof(*Init), Result, sizeof(*Result)))
			return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid initialization descriptor or output overlap");
		InitCopy = *Init;
		if (!xredisClientInitValid(&InitCopy, &Copy)) return false;
		const xbytesview Views[] = {InitCopy.Username, InitCopy.Password, InitCopy.Name};
		for (size_t Index = 0; Index < sizeof(Views) / sizeof(Views[0]); ++Index)
			if (xrtMemRangesOverlap(Views[Index].Data, Views[Index].Size, Result, sizeof(*Result)))
				return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "initialization output overlaps input payload");
	}
	if (Copy.Size != sizeof(Copy) || Copy.MaxRequests == 0 || Copy.MaxRequests > 1000000u ||
		Copy.MaxInFlight == 0 || Copy.MaxInFlight > Copy.MaxRequests || Copy.MaxCommandBytes == 0 ||
		Copy.MaxCommandBytes > Copy.MaxCommandBytesTotal || Copy.MaxCommandBytes > Copy.Stream.WriteLimit ||
		Copy.MaxCommandBytes > SIZE_MAX - sizeof(xredisrequest) || Copy.WorkRequests == 0 ||
		Copy.ReadBytes == 0 || Copy.ReadFrames == 0 || Copy.DrainTimeout <= 0 ||
		Copy.MaxPushes > (SIZE_MAX - sizeof(xredisclient)) / sizeof(xredisreply*) ||
		(Remote->Family != XNET_FAMILY_IPV4 && Remote->Family != XNET_FAMILY_IPV6) || Remote->Port == 0 ||
		!xrtNetStreamConfigValid(&Copy.Stream))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config or endpoint");
	if (!xrtNetEnginePin(Engine)) return false;
	size_t Bytes = sizeof(xredisclient) + Copy.MaxPushes * sizeof(xredisreply*);
	xredisclient* Client = (xredisclient*)xrtCalloc(1, Bytes);
	if (Client == NULL) { xrtNetEngineUnpin(Engine); return false; }
	Client->Engine = Engine; Client->EnginePinned = true; Client->Config = Copy; Client->Remote = *Remote;
	Client->StorageBytes = Bytes; Client->Info.Size = sizeof(Client->Info); Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED;
	Client->Managed = Init != NULL; Client->InitAwait = XREDIS_I_END;
	if (Client->Managed) {
		Client->InitMode = InitCopy.Mode; Client->InitFlags = InitCopy.Flags;
		Client->InitDeadline = xredisClientDeadline(Entered, InitCopy.Timeout);
	}
	Client->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
	xrtAtomic32Init(&Client->References, 1);
	if (!xrtMutexInit(&Client->Gate)) { xrtNetEngineUnpin(Engine); xrtFree(Client); return false; }
	if (!xrtNetPostInit(&Client->Wake) || !xredisReplyReaderCreate(&Copy.Replies, &Client->Reader) ||
		(Client->ClosePromise = xrtPromiseCreate(&Client->CloseFuture, NULL)) == NULL ||
		(Client->ReadyPromise = xrtPromiseCreate(&Client->ReadyFuture, NULL)) == NULL ||
		(Client->ReadyResult = (xredisreadyresult*)xrtMalloc(sizeof(*Client->ReadyResult))) == NULL ||
		(Client->Managed && !xredisClientInitBuild(Client, &InitCopy))) { xredisClientRelease(Client); return false; }
	xredisClientRef(Client); /* Active connection, even before Stream is created. */
	xrtMutexLock(&Client->Gate); bool Posted = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
	if (!Posted) {
		xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
		xredisClientRelease(Client); xredisClientRelease(Client); return false;
	}
	*Result = Client; return true;
}

XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result)
{ return xredisClientCreate(Engine, Remote, Affinity, Config, NULL, xrtTimer(), Result); }

XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init, xredisclient** Result)
{
	double Entered = xrtTimer(); xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	return xredisClientCreate(Engine, Remote, Affinity, Config, Init, Entered, Result);
}

XREDIS_API xfuture* xredisClientReady(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->ReadyFuture);
}

static bool xredisClientWord(xbytesview Word, cstr Text)
{
	size_t Size = strlen(Text); if (Word.Size != Size) return false;
	for (size_t Index = 0; Index < Size; ++Index) {
		unsigned char Byte = Word.Data[Index]; if (Byte >= 'a' && Byte <= 'z') Byte -= 'a' - 'A';
		if (Byte != (unsigned char)Text[Index]) return false;
	}
	return true;
}

static bool xredisClientCommandMode(const xbytesview* Args, size_t Count)
{
	const cstr Modes[] = {"SUBSCRIBE", "PSUBSCRIBE", "SSUBSCRIBE", "UNSUBSCRIBE", "PUNSUBSCRIBE",
		"SUNSUBSCRIBE", "MONITOR", "SYNC", "PSYNC"};
	for (size_t Index = 0; Index < sizeof(Modes) / sizeof(Modes[0]); ++Index)
		if (xredisClientWord(Args[0], Modes[Index])) return false;
	return !(Count > 1 && xredisClientWord(Args[0], "CLIENT") && xredisClientWord(Args[1], "REPLY"));
}

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{
	double Entered = xrtTimer(); size_t Bytes = 0;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client call arguments"); return XREDIS_ADMISSION_ERROR;
	}
	if (!xredisCommandSize(Arguments, Count, &Bytes)) return XREDIS_ADMISSION_ERROR;
	if (xrtMemRangesOverlap(Arguments, Count * sizeof(*Arguments), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
	}
	for (size_t Index = 0; Index < Count; ++Index) if (xrtMemRangesOverlap(Arguments[Index].Data, Arguments[Index].Size, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "call output overlaps argument payload"); return XREDIS_ADMISSION_ERROR;
	}
	if (!xredisClientCommandMode(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "command changes response cardinality/mode"); return XREDIS_ADMISSION_ERROR;
	}
	if (Client->Managed && (xredisClientWord(Arguments[0], "HELLO") || xredisClientWord(Arguments[0], "AUTH") ||
		xredisClientWord(Arguments[0], "SELECT") || (Count > 1 && xredisClientWord(Arguments[0], "CLIENT") && xredisClientWord(Arguments[1], "SETNAME")))) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "managed initialization controls cannot be changed by an ordinary call");
		return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
		Client->Info.Requests >= Client->Config.MaxRequests || Bytes > Client->Config.MaxCommandBytes ||
		Bytes > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Bytes; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Bytes);
	if (Request == NULL) {
		xrtMutexLock(&Client->Gate); --Client->Info.Requests; Client->Info.CommandBytes -= Bytes;
		if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
		xrtMutexUnlock(&Client->Gate); xredisClientRelease(Client); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
	Request->Sensitive = xredisClientWord(Arguments[0], "AUTH") || xredisClientWord(Arguments[0], "HELLO");
	Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(*Request->Result));
	xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token); /* FutureCancelToken returns a retained reference. */
	}
	if (Request->Watch != NULL && xredisCommandWrite(Request->Wire, Bytes, Arguments, Count, NULL)) {
		xrtMutexLock(&Client->Gate);
		if (Client->Info.State < XREDIS_CLIENT_CLOSING) {
			Request->SubmitNext = NULL;
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			/* A live pinned worker accepts this embedded post without allocation. */
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			/* The failed wake accepted no callback. Remove this exact tail. */
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
		} else { Admission = XREDIS_ADMISSION_CLOSED; ++Client->Info.Rejected; }
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_CLOSED ? XREDIS_ADMISSION_CLOSED : XREDIS_ADMISSION_ERROR;
}

XREDIS_API xredisadmission xredisClientCall(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate);
	bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
	xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync client call cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = xredisClientCallAsync(Client, Arguments, Count, Timeout, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}

XREDIS_API bool xredisClientTakePush(xredisclient* Client, xredisreply** Reply)
{
	if (Client == NULL || Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Reply, sizeof(*Reply)))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid push output");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.Pushes == 0) { xrtMutexUnlock(&Client->Gate); return false; }
	xredisreply* Value = Client->PushQueue[Client->PushHead]; Client->PushQueue[Client->PushHead] = NULL;
	Client->PushHead = (Client->PushHead + 1u) % Client->Config.MaxPushes; --Client->Info.Pushes;
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Value, &Info); Client->Info.PushBytes -= Info.ReservedBytes;
	*Reply = Value; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientInfo(xredisclient* Client, xredisclientinfo* Info)
{
	if (Client == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Info, sizeof(*Info)) || Info->Size != sizeof(*Info))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client info output");
	xrtMutexLock(&Client->Gate); *Info = Client->Info; xrtMutexUnlock(&Client->Gate); return true;
}

XREDIS_API bool xredisClientClose(xredisclient* Client)
{
	if (Client == NULL) return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL");
	xrtMutexLock(&Client->Gate);
	if (Client->Info.State < XREDIS_CLIENT_CLOSING) { Client->Info.State = XREDIS_CLIENT_CLOSING; Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED; }
	bool Result = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate); return Result;
}

XREDIS_API xfuture* xredisClientClosed(xredisclient* Client)
{
	if (Client == NULL) { xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return NULL; }
	return xrtFutureRef(Client->CloseFuture);
}

#endif
#endif

#endif
