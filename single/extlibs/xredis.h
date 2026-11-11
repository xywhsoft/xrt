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
#ifndef XREDIS_MODULE_XREDIS_DIAL
#define XREDIS_MODULE_XREDIS_DIAL
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS
#define XREDIS_MODULE_XREDIS_TLS
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS_AUTH
#define XREDIS_MODULE_XREDIS_TLS_AUTH
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_WATCH
#define XREDIS_MODULE_XREDIS_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_TRANSACTION
#define XREDIS_MODULE_XREDIS_POOL_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_WATCH
#define XREDIS_MODULE_XREDIS_POOL_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL_CONNECT
#define XREDIS_MODULE_XREDIS_POOL_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_TRACKING
#define XREDIS_MODULE_XREDIS_TRACKING
#endif
#endif

/* xredis_tracking 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TRACKING)
#ifndef XREDIS_FEATURE_XREDIS_TRACKING
#define XREDIS_FEATURE_XREDIS_TRACKING
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_tls_auth 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TLS_AUTH)
#ifndef XREDIS_FEATURE_XREDIS_TLS_AUTH
#define XREDIS_FEATURE_XREDIS_TLS_AUTH
#endif
#ifndef XREDIS_MODULE_XREDIS_TLS
#define XREDIS_MODULE_XREDIS_TLS
#endif
#ifndef XRT_MODULE_TLS_CLIENT_AUTH
#define XRT_MODULE_TLS_CLIENT_AUTH
#endif
#endif

/* xredis_tls 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TLS)
#ifndef XREDIS_FEATURE_XREDIS_TLS
#define XREDIS_FEATURE_XREDIS_TLS
#endif
#ifndef XREDIS_MODULE_XREDIS_DIAL
#define XREDIS_MODULE_XREDIS_DIAL
#endif
#ifndef XRT_MODULE_TLS_STREAM
#define XRT_MODULE_TLS_STREAM
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

/* xredis_dial 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_DIAL)
#ifndef XREDIS_FEATURE_XREDIS_DIAL
#define XREDIS_FEATURE_XREDIS_DIAL
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL
#define XRT_MODULE_NET_TCP_DIAL
#endif
#endif

/* xredis_pool_transaction 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_TRANSACTION)
#ifndef XREDIS_FEATURE_XREDIS_POOL_TRANSACTION
#define XREDIS_FEATURE_XREDIS_POOL_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#endif

/* xredis_pool_watch 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_WATCH)
#ifndef XREDIS_FEATURE_XREDIS_POOL_WATCH
#define XREDIS_FEATURE_XREDIS_POOL_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#ifndef XREDIS_MODULE_XREDIS_WATCH
#define XREDIS_MODULE_XREDIS_WATCH
#endif
#endif

/* xredis_watch 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_WATCH)
#ifndef XREDIS_FEATURE_XREDIS_WATCH
#define XREDIS_FEATURE_XREDIS_WATCH
#endif
#ifndef XREDIS_MODULE_XREDIS_TRANSACTION
#define XREDIS_MODULE_XREDIS_TRANSACTION
#endif
#endif

/* xredis_transaction 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_TRANSACTION)
#ifndef XREDIS_FEATURE_XREDIS_TRANSACTION
#define XREDIS_FEATURE_XREDIS_TRANSACTION
#endif
#ifndef XREDIS_MODULE_XREDIS_CLIENT
#define XREDIS_MODULE_XREDIS_CLIENT
#endif
#endif

/* xredis_pool_connect 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL_CONNECT)
#ifndef XREDIS_FEATURE_XREDIS_POOL_CONNECT
#define XREDIS_FEATURE_XREDIS_POOL_CONNECT
#endif
#ifndef XREDIS_MODULE_XREDIS_POOL
#define XREDIS_MODULE_XREDIS_POOL
#endif
#endif

/* xredis_pool 及其直接依赖。 */
#if defined(XREDIS_MODULE_ALL) || defined(XREDIS_MODULE_XREDIS_POOL)
#ifndef XREDIS_FEATURE_XREDIS_POOL
#define XREDIS_FEATURE_XREDIS_POOL
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
/* public: extlibs/xredis/include/xredis/pool.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_H
#define XREDIS_POOL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_POOL requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispool xredispool;
typedef struct xredislease xredislease;
typedef enum xredispoolstate { XREDIS_POOL_OPEN, XREDIS_POOL_CLOSING, XREDIS_POOL_CLOSED } xredispoolstate;
typedef struct xredispoolconfig {
    size_t Size;
    /* Includes concurrent acquire constructors and queued acquisitions. */
    size_t MaxPending;
    size_t MaxRequests;
    size_t MaxCommandBytes;
    /* Finite maximum lease hold, including calls and application think time.
     * Default 30 s, hard maximum 24 hours. No renewal on calls. */
    int64 LeaseTimeout;
    /* Graceful Return seals admission and drains, default 5 s, max 60 s.
     * Expiry retires a still-busy connection. */
    int64 ReturnTimeout;
    size_t WorkItems;
} xredispoolconfig;
typedef struct xredispoolinfo {
    size_t Size;
    xredispoolstate State;
    size_t Members, Live, Idle, Leased, Pending;
    uint64 Requests, CommandBytes, Acquired, Rejected, Returned, Retired;
} xredispoolinfo;
/* Borrowed from the acquisition Future. Only OK has a Lease. Retain it before
 * dropping that Future. Failed acquisition performs no Redis command. */
typedef struct xredisleaseresult {
    xredisrequeststatus Status;
    xredislease* Lease;
} xredisleaseresult;
typedef struct xredisleaseclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xredisleaseclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config);
/* Fixed membership, 1..65536 distinct OPEN/ready/physically idle managed
 * clients with the same negotiated RESP version on this running Engine.
 * The caller supplies a common service/credentials/database/session profile;
 * the pool cannot infer erased credentials or server-side profile changes.
 * Existing numeric/DNS/TLS factories
 * supply members. Success holds refs and exclusively claims them; caller refs
 * are borrowed, never consumed. Failure rolls back every partial claim and
 * leaves output unchanged. Busy members return OVERLOADED. No connection is
 * created/reconnected/replayed by this core. PoolClose closes its members.
 * Do not use member clients directly while pooled; Close observers and an
 * external close remain valid. Count/argument storage is borrowed for Create.
 * Limits are hard across all members, including request constructor/wire/
 * timer/cancel tails. Result/reply storage retained by callers remains theirs.
 * Each native parser/push/work limit also remains in force. */
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Pool);
XREDIS_API bool xredisPoolRetain(xredispool* Pool);
/* Caller must Close then Release; Release alone does not close a live pool. */
XREDIS_API void xredisPoolRelease(xredispool* Pool);
/* Bounded FIFO admission. Timeout (-1 forever, 0 due) includes allocation,
 * queue wait and grant. A separate finite lease hold starts at grant. Cancel
 * requests stop; every accepted acquisition resolves a typed immutable result.
 * No live member yields DISCONNECTED for fixed membership; optional automatic
 * maintenance instead waits within the original deadline through outages.
 * Output changes only on ACCEPTED.
 * Sync rejects every worker of the same engine. */
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future);
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info);
XREDIS_API bool xredisPoolClose(xredispool* Pool);
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool);
XREDIS_API bool xredisLeaseRetain(xredislease* Lease);
XREDIS_API void xredisLeaseRelease(xredislease* Lease);
/* Direct native request/Future, with no second per-call Future or wire clone.
 * Ordinary one-reply and blocking commands use the same FIFO and physical
 * obligations as the client. Effective deadline is min(call deadline, lease
 * hold deadline). Connection/mode/transaction controls require their own APIs.
 * Local cancel/timeout does not release a sent request's FIFO obligation. */
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent, nonblocking seal. Return drains within ReturnTimeout and the
 * original hold deadline; Abort retires the connection. Releasing a handle
 * alone does not return it; finite hold expiry recovers dropped observers.
 * Closed resolves only after API constructors and native request storage
 * retire. Reusable certifies the slot-release boundary; it does not promise
 * another caller has not acquired it since. Completed retained metadata pins
 * no client/pool/engine. Join the engine before unloading callback code. */
XREDIS_API bool xredisLeaseReturn(xredislease* Lease);
XREDIS_API bool xredisLeaseAbort(xredislease* Lease);
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_connect.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_CONNECT_H
#define XREDIS_POOL_CONNECT_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS_POOL_CONNECT requires XREDIS_FEATURE_XREDIS_POOL"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
/* Called serially on the pool worker, without a pool/client gate. Must not
 * block. Start an existing managed numeric/DNS/TLS/auth factory with Timeout
 * as its total setup limit. Success transfers ONE private client reference;
 * failure transfers nothing and leaves Client unchanged. No business commands
 * or sharing/claims are permitted before transfer. Slot is stable, Attempt is
 * a saturating per-slot counter. Choose the same service/session/RESP profile
 * on every attempt. No command from an old connection is ever replayed. */
typedef bool (*xredispoolconnectproc)(xnetengine* Engine, size_t Slot,
    uint64 Attempt, int64 Timeout, ptr Data, xredisclient** Client);
typedef void (*xredispoolconnectdrop)(ptr Data);
typedef struct xredispoolconnectconfig {
    size_t Size;
    xredispoolconnectproc Connect;
    xredispoolconnectdrop Drop;
    ptr Data;
    xredisprotocol Protocol;
    size_t MaxConnecting;
    int64 AttemptTimeout;
    int64 RetryMin, RetryMax;
} xredispoolconnectconfig;
typedef struct xredispoolconnectinfo {
    size_t Size;
    size_t Connecting, Waiting;
    uint64 Attempts, Failures, Connected;
    xredisrequeststatus LastFailure;
} xredispoolconnectinfo;
XRT_EXTERN_C_BEGIN
/* Defaults: RESP3, 4 simultaneous attempts, 5 s setup, 100 ms..10 s retry.
 * Connect is mandatory. Bounds: concurrency 1..65536 (capped by Count),
 * setup 1..60000 ms, retry 1..60000 ms, min <= max. */
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config);
/* Start/refill Count slots. No I/O/factory callback runs inline, although a
 * worker may run before this API returns. ACCEPTED consumes the Data/Drop
 * obligation; every failure preserves it and Result. Config values are copied.
 * Attempts, their physical failure tails and retry queues are bounded. Every
 * failed generation physically closes before its slot can reconnect. Backoff
 * is exponential with bounded non-cryptographic jitter, reset on readiness.
 * Acquisition deadlines include outage/retry wait; zero Live does not cause
 * DISCONNECTED in this mode. Each acquisition still has its own finite/forever
 * timeout and cancellation; it does not cancel shared maintenance.
 * PoolClose stops new attempts, closes candidates/members and retires timers/
 * callbacks before Drop and PoolClosed. Drop runs once on the pool worker,
 * outside gates, must not block and must outlive the pool. Context, resolver,
 * verifier/signer code and referenced material remain valid until Drop.
 * Maintenance timer OOM fails the pool closed; it never busy-retries OOM.
 * PoolInfo.Retired counts actual retired client generations (can exceed Count).
 * Retained closed pool metadata holds no context/client/engine reference. */
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity,
    size_t Count, const xredispoolconfig* Config,
    const xredispoolconnectconfig* Connect, xredispool** Result);
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/transaction.h */
/* ========================================================================== */

#ifndef XREDIS_TRANSACTION_H
#define XREDIS_TRANSACTION_H

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "XREDIS_FEATURE_XREDIS_TRANSACTION requires XREDIS_FEATURE_XREDIS_CLIENT"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#define XREDIS_TRANSACTION_COMMANDS_DEFAULT 256u
#define XREDIS_TRANSACTION_COMMANDS_MAX 65536u

typedef struct xredistransactioncommand {
    const xbytesview* Arguments;
    size_t Count;
} xredistransactioncommand;

typedef struct xredistransactionconfig {
    size_t Size;
    size_t MaxCommands;
} xredistransactionconfig;

/* Execution certainty is independent from local timeout/cancellation and from
 * error elements inside a valid EXEC array. Redis does not roll those back. */
typedef enum xredistransactionexecution {
    XREDIS_TRANSACTION_NOT_EXECUTED = 0,
    XREDIS_TRANSACTION_MAY_HAVE_EXECUTED,
    XREDIS_TRANSACTION_EXECUTED,
    XREDIS_TRANSACTION_ABORTED
} xredistransactionexecution;

/* Owned by the returned Future; Request is the first member. Request.Reply is
 * the final EXEC array/error or a rejected MULTI reply. QueueError retains the
 * first queue error; other queue errors are counted and promptly released.
 * Queue rejection uses SERVER_ERROR, no EXEC reply, and NOT_EXECUTED after a
 * validated DISCARD. Counts describe knowledge at logical completion and do
 * not change while an abandoned operation drains. Retain replies explicitly
 * to use them after destroying the Future. Results do not retain the client. */
typedef struct xredistransactionresult {
    xredisrequestresult Request;
    xredistransactionexecution Execution;
    size_t Commands;
    size_t Queued;
    size_t QueueErrors;
    xredisreply* QueueError;
} xredistransactionresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config);

/* One exclusive MULTI / queue / EXEC (or DISCARD) operation on a managed
 * connection. Raw Connect is refused. MULTI is acknowledged before business
 * commands are sent; preceding requests drain, and later requests cannot enter
 * the transaction. Input is borrowed only during admission and encoded once
 * into owned storage. Count=0 is an empty transaction. Config=NULL uses bounded
 * defaults. Client request/byte limits include transaction wire and metadata.
 * Timeout includes admission/encoding/worker/setup/transaction/cleanup time.
 * Before EXEC is accepted, cancellation/timeout suppresses EXEC and drains
 * DISCARD; afterwards execution may be uncertain. DrainTimeout still bounds
 * physical cleanup. Cleanup rejection/malformed or unknown EXEC replies retire
 * the connection; a recognized EXECABORT confirms abort and permits reuse.
 * No automatic replay, rollback, WATCH/read/decision session or retry is added.
 * Connection/mode controls are not transaction payload commands. Unknown module
 * commands must obey Redis transactional queue/reply semantics. Output changes
 * only on ACCEPTED. Future cancellation returns a typed immutable result. */
XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);

/* Same deadline and outcome, then waits. Rejects every worker of this engine. */
XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/watch.h */
/* ========================================================================== */

#ifndef XREDIS_WATCH_H
#define XREDIS_WATCH_H

#if defined(XREDIS_FEATURE_XREDIS_WATCH) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS_WATCH requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
typedef struct xrediswatch xrediswatch;
typedef struct xrediswatchconfig {
    size_t Size;
    /* Positive total lifetime, including admission, application think time,
     * reads and commit. No renewal; at most 24 hours. Default 5000 ms. */
    int64 Timeout;
    /* Separate bounded UNWATCH request deadline, default 5000 ms (max 60 s).
     * Physical retirement also includes the client's existing drain grace. */
    int64 CleanupTimeout;
    size_t MaxKeys;
    size_t MaxWireBytes;
    xredistransactionconfig Transaction;
} xrediswatchconfig;

/* Owned by the start Future. Watch is borrowed on EVERY accepted outcome;
 * only OK permits commands. On failure it still exposes the physical Closed
 * barrier. Retain it before destroying the Future. A retained completed result
 * does not pin the engine. Request.Reply is the WATCH response, not EXEC. */
typedef struct xrediswatchresult {
    xredisrequestresult Request;
    xrediswatch* Watch;
} xrediswatchresult;
/* Owned by Closed's Future. Reason is the scope's first terminal reason;
 * Reusable certifies physical idle plus a validated clearing boundary. */
typedef struct xrediswatchclosedresult {
    xredisrequeststatus Reason;
    bool Reusable;
} xrediswatchclosedresult;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config);
/* Claims an OPEN, ready, idle managed client. A busy/not-ready/already claimed
 * client returns OVERLOADED. Ordinary ClientCall/Transaction admissions are
 * refused while claimed. Keys are binary and copied only during admission.
 * Count must be 1..MaxKeys (default 256, hard limit 65536); default wire 64 KiB.
 * Config is copied; output changes only on ACCEPTED. No implicit retry.
 * Cancelling a pending start Future abandons the scope. */
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API bool xredisWatchRetain(xrediswatch* Watch);
XREDIS_API void xredisWatchRelease(xrediswatch* Watch);
/* Ordinary one-reply reads/commands within this connection's WATCH scope.
 * Connection/mode/transaction controls are reserved. The effective deadline
 * is min(call entry + Timeout, scope deadline). Release alone does not abort.
 * Application writes have their ordinary Redis effects and are not rolled back. */
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future);
/* Exactly one accepted commit seals the scope against new calls/commits.
 * Earlier reads drain before MULTI. Result is xredistransactionresult; a null
 * EXEC is ABORTED, never an automatic retry. Logical completion precedes
 * physical Closed. Cancelling an accepted commit may retire the connection. */
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch,
    const xredistransactioncommand* Commands, size_t Count, int64 Timeout, xfuture** Future);
/* Idempotent nonblocking abandonment. Idle scopes send UNWATCH; abandoning
 * while physical requests remain may retire the connection. No dirty reuse.
 * Closed is a retained observer, not a request to abort. It resolves only
 * after scoped request storage, timers, notifications and the claim retire.
 * Sync facades reject every worker of the client's engine. Stop/join the
 * engine before unloading code, including the final notification return tail. */
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch);
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_watch.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_WATCH_H
#define XREDIS_POOL_WATCH_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_WATCH))
#error "XREDIS_FEATURE_XREDIS_POOL_WATCH requires pool and watch"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XRT_EXTERN_C_BEGIN
/* Nested WATCH scope on a physically idle lease. Busy returns OVERLOADED;
 * admission failure restores the unused lease. Acceptance consumes/seals this
 * lease for the scope; ordinary/transaction/second-WATCH admissions cannot
 * enter it. Result is the native xrediswatchresult on every accepted outcome,
 * including a failed start's physical Closed observer. All scope requests and
 * transaction/control metadata retain the pool's hard aggregate quota. Both
 * total scope/call deadlines are capped at the original lease hold.
 * Return/Abort/hold expiry/PoolClose cannot release a live nested owner. Return
 * requests bounded scope abandonment (UNWATCH when clean); grace expiry or
 * uncertain/busy cleanup retires the client. Explicitly Return the lease after
 * scope Closed. Retained completed scope/lease metadata pins no pool/engine.
 * Same input/output/ownership and sync-worker rules as standalone WATCH. */
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/pool_transaction.h */
/* ========================================================================== */

#ifndef XREDIS_POOL_TRANSACTION_H
#define XREDIS_POOL_TRANSACTION_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION) && (!defined(XREDIS_FEATURE_XREDIS_POOL) || !defined(XREDIS_FEATURE_XREDIS_TRANSACTION))
#error "XREDIS_FEATURE_XREDIS_POOL_TRANSACTION requires pool and transaction"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XRT_EXTERN_C_BEGIN
/* Direct staged transaction on this lease. Accepted enqueue seals the lease
 * against further calls/transactions; earlier leased calls drain before MULTI.
 * Config/commands are borrowed only during admission. Output changes only on
 * ACCEPTED. Native xredistransactionresult preserves execution uncertainty and
 * queue/runtime errors. Group request/byte quotas include wire/control/state.
 * Deadline is capped at the original lease hold. Return the lease explicitly;
 * its physical Closed barrier waits for actual DISCARD/EXEC/drain/retirement.
 * Return never authorizes replay. Sync rejects all workers of this engine. */
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, xfuture** Future);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/dial.h */
/* ========================================================================== */

#ifndef XREDIS_DIAL_H
#define XREDIS_DIAL_H

#if defined(XREDIS_FEATURE_XREDIS_DIAL) && (!defined(XREDIS_FEATURE_XREDIS_CLIENT) || !defined(XRT_FEATURE_NET_TCP_DIAL))
#error "XREDIS_FEATURE_XREDIS_DIAL requires client and net_tcp_dial"
#endif

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#define XREDIS_DIAL_ADDRESSES_MAX 64u
#define XREDIS_DIAL_HOST_MAX 65535u

/* Network policy only; the client Config.Stream remains the sole TCP limits.
 * Timeout=0 disables the additional Dial timer, -1 is infinite. Managed Init's
 * original deadline still spans host copying, DNS, candidate race and HELLO.
 * Default dual stack, 30000 ms, 250 ms fallback, eight candidates, 1024 host
 * bytes. Maximum candidate/address count 64; maximum borrowed host span 65535.
 * No IDNA conversion, TLS, reconnect or application-command replay is implied. */
typedef struct xredisclientdialconfig {
	size_t Size;
	xnetfamily Family;
	int64 Timeout;
	int64 FallbackDelay;
	uint32 MaxAttempts;
	size_t MaxHostBytes;
} xredisclientdialconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config);

/* Async hostname setup on the existing Engine/Resolver. Host is a bounded,
 * nonempty readable view without embedded NUL; copied before return. Resolver
 * is application-owned and must outlive Closed (then retire/join it separately).
 * It is shared; closing a client does not close the service or stop an OS
 * lookup already running there. All client callback/data ownership retires
 * through the client Close barrier. NULL Init selects strict RESP3 defaults.
 * Calls can queue, expire/cancel through DNS and TCP setup. Result unchanged on
 * failure. Successful submission owns one Client reference: Close then Release.
 * Core XRT supplies bounded family interleaving, fallback and losing-socket
 * retirement before transfer. No business operation is retried or replayed. */
XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);

/* Already resolved immutable endpoints; no Resolver is consulted. Each port
 * is preserved. At most 64 entries; Family filters candidates and MaxAttempts
 * bounds selected attempts. Retains the list only until native Dial admission;
 * caller may release its reference immediately. Output storage must be disjoint
 * from descriptors and borrowed host/credential/name/endpoint storage. */
XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine,
	xnetaddrlist* Addresses, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tls.h */
/* ========================================================================== */

#ifndef XREDIS_TLS_H
#define XREDIS_TLS_H
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XREDIS_FEATURE_XREDIS_DIAL) || !defined(XRT_FEATURE_TLS_STREAM) || !defined(XRT_FEATURE_TLS_CLIENT_RESUME))
#error "XREDIS_FEATURE_XREDIS_TLS requires dial, TLS stream and verified/resumable TLS client"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS) && (!defined(XRT_FEATURE_TLS_RECORD_AES) || !defined(XRT_FEATURE_TLS_RECORD_CHACHA) || \
	!defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	!defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P384))
#error "XREDIS_FEATURE_XREDIS_TLS requires its declared AES/ChaCha, SHA256/SHA384 and X25519/P256/P384 backends"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* All views are borrowed only during the factory. Native TLS owns a snapshot
 * of names/ALPN and shared Context/Verifier/Resume references before return.
 * Supply a Verifier (or explicit ResumeOnly with a valid Resume); no implicit
 * insecure verification mode. Verifier callback code/context follow XRT's
 * shared-verifier lifetime, including Engine join after client Closed.
 * Defaults: native TLS policy/stream limits, normal Dial defaults, 64 KiB
 * metadata, 64 KiB plaintext write work per client turn, infer DNS SNI/name.
 * Numeric hosts infer VerifyName only; prepared endpoints need an explicit
 * ServerName or VerifyName. Init's original deadline spans TLS too. */
typedef struct xredisclienttlsconfig {
	size_t Size;
	xtlsclientconfig Session;
	xtlsstreamconfig Stream;
	xredisclientdialconfig Dial;
	size_t MaxMetadataBytes;
	size_t WriteBytes;
	bool ServerNameFromHost;
} xredisclienttlsconfig;

XRT_EXTERN_C_BEGIN
XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config);
/* TLS is required. Result unchanged on rejection; no cleartext fallback or
 * operation replay. Typed readiness is published only after TLS + Redis setup.
 * Resolver is application owned through Closed, and joined separately. */
XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result);
XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
/* Numeric endpoint convenience; TLS identity must be explicit in TLS. */
XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclienttlsconfig* TLS, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tls_auth.h */
/* ========================================================================== */

#ifndef XREDIS_TLS_AUTH_H
#define XREDIS_TLS_AUTH_H
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH) && (!defined(XREDIS_FEATURE_XREDIS_TLS) || !defined(XRT_FEATURE_TLS_CLIENT_AUTH))
#error "XREDIS_FEATURE_XREDIS_TLS_AUTH requires Redis TLS and native TLS client authentication"
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
XRT_EXTERN_C_BEGIN
/* Configs/views are borrowed only during admission. The native session retains
 * the immutable identity before DNS or any worker callback; no caller identity
 * or auth-config borrow escapes. Signers run on the connection worker and must
 * not block it or reenter/destroy the native session. The identity's final
 * Release follows its existing last-reference thread contract. Resolver/code/
 * verifier-context lifetime and total deadlines follow the TLS Dial contract.
 * Auth is mandatory; defaults come from xrtTlsClientAuthConfigInitV1. With
 * RequireCertificate=true, no compatible server request fails readiness.
 * With false the caller explicitly permits a connection without local proof.
 * Full certificate handshakes only: explicit resumes are rejected and tickets
 * are not retained until identity-bound resumption is implemented. A signing
 * backend is selected separately; this module does not add private-key code.
 * Readiness includes the configured Redis initialization, so TLS 1.3 local
 * proof queueing alone cannot publish successful managed readiness. */
XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xredis/include/xredis/tracking.h */
/* ========================================================================== */

#ifndef XREDIS_TRACKING_H
#define XREDIS_TRACKING_H
#if defined(XREDIS_FEATURE_XREDIS_TRACKING) && !defined(XREDIS_FEATURE_XREDIS_CLIENT)
#error "Redis Tracking requires the existing Client"
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
#define XREDIS_TRACKING_PREFIXES_MAX 64u
#define XREDIS_TRACKING_BCAST 0x01u
#define XREDIS_TRACKING_OPTIN 0x02u
#define XREDIS_TRACKING_OPTOUT 0x04u
#define XREDIS_TRACKING_NOLOOP 0x08u
#define XREDIS_TRACKING_REDIRECT 0x10u
#define XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT ((size_t)65536u)
typedef struct xredistrackingconfig {
    size_t Size;bool Enabled;uint32 Flags;int64 Redirect;
    const xbytesview* Prefixes;size_t PrefixCount;
} xredistrackingconfig;
/* Request prefix and ownership match ordinary Client results. Sequence is
 * per Client, in actual received-frame order, not completion/collection order.
 * Zero means no reply was published. Counter exhaustion closes without wrap.
 * No Client/Engine reference survives in an observed result. */
typedef struct xredistrackingresult {xredisrequestresult Request;uint64 Sequence;} xredistrackingresult;
/* Query is the ordinary result prefix. Control independently preserves the
 * CLIENT CACHING outcome known at logical completion. Both replies/causes
 * belong to the Future; neither result retains a Client/Engine. */
typedef struct xrediscachingresult {
    xredistrackingresult Query,Control;
    bool ControlReceived;
} xrediscachingresult;
typedef struct xredistrackingpush {xredisreply* Reply;uint64 Sequence;} xredistrackingpush;
typedef struct xredistrackinginfo {size_t Size,Bytes,Reserved;uint64 Sequence,Rejected;bool Enabled,Preparing;} xredistrackinginfo;
typedef enum xredistrackingeventkind {XREDIS_TRACKING_KEYS=0,XREDIS_TRACKING_FLUSH,XREDIS_TRACKING_REDIRECT_BROKEN} xredistrackingeventkind;
typedef enum xredistrackingstatus {XREDIS_TRACKING_ERROR=-2,XREDIS_TRACKING_MALFORMED=-1,XREDIS_TRACKING_OTHER=0,XREDIS_TRACKING_EVENT=1} xredistrackingstatus;
/* Borrows Reply. Walk exactly Keys nodes using xredisReplyGet/Next, starting
 * at FirstKey: linear iteration, binary views, no allocation/copy. Retain the
 * original Reply explicitly to keep these views after its owning result. */
typedef struct xredistrackingevent {
    const xredisreply* Reply;xredistrackingeventkind Kind;size_t FirstKey,Keys;uint64 Redirect;
} xredistrackingevent;
XRT_EXTERN_C_BEGIN
/* Thread-safe one-time opt-in. MaxBytes=0 uses 64 KiB. One bounded allocation
 * (header + MaxPushes uint64 slots), retained until final Client release.
 * ACCEPTED is idempotent; concurrent installation/insufficient budget is normal
 * OVERLOADED. No command or tracking mode is sent here. Existing queued pushes
 * have sequence0 and precede all newly admitted observed calls. This activation
 * starts an observation epoch with an empty application cache. */
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* Client,size_t MaxBytes);
XREDIS_API bool xredisClientTrackingInfo(xredisclient* Client,xredistrackinginfo* Info);
/* Requires Observe. Same command/deadline/admission/cancel/drain semantics as
 * ordinary calls. One enlarged result allocation, no extra Future or payload
 * copy. Other callers may use ordinary requests; those results carry no order.
 * Sync form rejects every worker of this Engine before submitting. */
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* Client,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Requires Observe and a managed connection. Copies CLIENT CACHING YES/NO
 * and one ordinary query into ONE adjacent wire group/request/Future. Other
 * producers cannot interleave commands; this is not a rollback transaction.
 * Query may execute even if Control is SERVER_ERROR. Only two successful
 * results authorize caching; invalidations still require receive-order checks.
 * Known connection/transaction/multiple-reply controls are refused as queries.
 * MaxCommandBytes/Total include both frames and aligned private group state.
 * A sent cancel/timeout is MAY_HAVE_EXECUTED even if only the prefix was sent:
 * the original group remains a bounded FIFO drain obligation, with no replay.
 * ControlReceived and both results are immutable snapshots, not later updates.
 * Timeout covers construction and both replies. Sync rejects all Engine workers. */
XREDIS_API xredisadmission xredisClientCachingCallAsync(xredisclient* Client,bool Caching,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
XREDIS_API xredisadmission xredisClientCachingCall(xredisclient* Client,bool Caching,
    const xbytesview* Arguments,size_t Count,int64 Timeout,xfuture** Future);
/* Transfers one reply reference. Same push queue/credits as ordinary TakePush;
 * do not mix consumers when ordered invalidation handling is required. Empty
 * preserves output/error. Values survive Client/Engine destruction. */
XREDIS_API bool xredisClientTrackingTake(xredisclient* Client,xredistrackingpush* Push);
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* Config);
/* Encodes CLIENT TRACKING, including binary PREFIX, REDIRECT, BCAST, OPTIN,
 * OPTOUT and NOLOOP. Requires Observe; uses the original entry deadline.
 * Input is copied before return. Direct invalidations require managed RESP3
 * (AUTO must have selected3); RESP2 senders need positive REDIRECT. Success
 * validates a simple OK; unexpected non-error replies close as PROTOCOL.
 * The server reply/status remains explicit:
 * a sent timeout can leave mode uncertain. No replay or hidden local cache.
 * Use CachingCall for adjacent OPTIN/OPTOUT queries shared by producers;
 * two separately submitted commands do not form an atomic group. */
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* Client,
    const xredistrackingconfig* Config,int64 Timeout,xfuture** Future);
/* Fully validate recognized RESP3 invalidations/redirection failure, or the
 * RESP2 __redis__:invalidate Pub/Sub envelope. Null invalidation means flush.
 * OTHER/malformed preserve output and ambient error. Invalid API use reports
 * ERROR; output cannot overlap any Reply allocation. Runtime receiving of RESP2
 * Pub/Sub still requires a dedicated subscribed connection implementation. */
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* Reply,xredistrackingevent* Event);
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

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_DIAL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_DIAL"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TLS)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TLS"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TLS_AUTH"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRANSACTION"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_WATCH"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_TRANSACTION"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_WATCH"
#endif
#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_POOL_CONNECT"
#endif

#if defined(XREDIS_FEATURE_XREDIS) && !defined(XREDIS_FEATURE_XREDIS_TRACKING)
#error "XREDIS_FEATURE_XREDIS requires XREDIS_FEATURE_XREDIS_TRACKING"
#endif

#endif

#endif

#if defined(XREDIS_IMPLEMENTATION) && !defined(XREDIS_IMPLEMENTATION_ONCE)
#define XREDIS_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/client_connect.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
#ifndef XREDIS_CLIENT_CONNECT_INTERNAL_H
#define XREDIS_CLIENT_CONNECT_INTERNAL_H
typedef struct xredisexclusiveowner xredisexclusiveowner;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
enum { XREDIS_OWNER_STARTING, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED,
    XREDIS_OWNER_CLOSING, XREDIS_OWNER_CLOSED };
#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredisownerquota {
    xatomic64 Requests, Bytes;
    xatomic32 Closing;
    uint64 MaxRequests, MaxBytes;
} xredisownerquota;
#endif
/* Embedded in a session or pool slot. Retain/Release are atomic-only; Wake is called
 * outside the client Gate. The owner holds a client ref and an engine Pin
 * until Unclaim, including the physical retirement of request constructors.
 * Deadline is published before ACTIVE, and changed only while sealed, idle
 * and without API constructors from the previous generation. */
struct xredisexclusiveowner {
    ptr Data;
    xatomic32 State;
    double Deadline;
    void (*Retain)(ptr Data);
    void (*Wake)(ptr Data);
    void (*Release)(ptr Data);
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisownerquota* Quota;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent;
    void (*Abandon)(ptr Data);
    xatomic32 Nested;
#endif
};
xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine);
#endif
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientIsWorker(xredisclient* Client);
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered,
    bool Control, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output);
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner);
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon);
void __xredisClientChildDone(xredisexclusiveowner* Owner);
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered);
#endif
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
xredisadmission __xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future);
#endif
#endif
typedef struct xredisclienttlsconfig xredisclienttlsconfig;
typedef struct xtlsclientauthconfigv1 xtlsclientauthconfigv1;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
typedef struct xredistlsplan {
	xtlssession* Session;
	xtlsstreamconfig Config;
	size_t WriteBytes;
} xredistlsplan;
bool __xredisClientDialTls(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, double Entered, xredisclient** Result);
bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan);
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
bool __xredisClientDialTlsAuthV1(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
	const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result);
#endif
/* Consumes TCP always, and Session only on true. Owning Worker only. */
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan);
const xnetstreamevents* __xredisClientTlsPendingEvents(void);
#endif

/* Private cold connection-start handoff. No per-request dispatch, public
 * transport plugin or shared runtime. The descriptor lives in Data's owner.
 * Factory failure consumes nothing; success drops it at physical Close. */
typedef struct xredisconnectplan {
	ptr Data;
	bool (*Start)(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
		const xnetstreamconfig* Config, const xnetstreamevents* Events);
	void (*Cancel)(ptr Data);
	void (*Drop)(ptr Data);
} xredisconnectplan;

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result);
/* Owning Worker only. Done consumes any winning Stream reference, then the
 * source calls Retired as its final action before returning from its callback.
 * The queued client work cannot publish Close until that callback returns. */
void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause);
void __xredisClientConnectRetired(xredisclient* Client);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/pool_admit.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL)
#ifndef XREDIS_POOL_ADMIT_INTERNAL_H
#define XREDIS_POOL_ADMIT_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync);
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xredis/src/internal/pool_connect_internal.h */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL)
#ifndef XREDIS_POOL_CONNECT_INTERNAL_H
#define XREDIS_POOL_CONNECT_INTERNAL_H
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result);
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info);
#endif
#endif
#endif


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

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingWord(const xredisreplynode* N,const xredisreply* R,cstr Word,size_t Length)
{
    return (N->Type==XREDIS_REPLY_BULK_STRING || N->Type==XREDIS_REPLY_SIMPLE_STRING) && !(N->Flags&XREDIS_REPLY_NULL_VALUE) &&
        N->Bytes==Length && !memcmp(R->Data+N->Offset,Word,Length);
}
XREDIS_API xredistrackingstatus xredisTrackingEventRead(const xredisreply* R,xredistrackingevent* Output)
{
    if(!R || !Output || !xrtMemRangeValid(Output,sizeof(*Output)) || xredisReplyOverlap(R,Output,sizeof(*Output))){
        xredisReplyReport(XERR_ARGUMENT,XREDIS_REPLY_ARGUMENT,"invalid tracking event output");return XREDIS_TRACKING_ERROR;
    }
    const xredisreplynode* Root=R->Nodes+R->Root;
    if((Root->Type!=XREDIS_REPLY_PUSH && Root->Type!=XREDIS_REPLY_ARRAY) || !Root->ChildCount)return XREDIS_TRACKING_OTHER;
    size_t First=Root->FirstChild;const xredisreplynode* Tag=R->Nodes+First;
    bool Keys=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"invalidate",10);
    bool Broken=Root->Type==XREDIS_REPLY_PUSH && xredisTrackingWord(Tag,R,"tracking-redir-broken",21);
    size_t Payload=Tag->Next;
    if(Root->Type==XREDIS_REPLY_ARRAY && xredisTrackingWord(Tag,R,"message",7) && Root->ChildCount>=2){
        const xredisreplynode* Channel=R->Nodes+Payload;
        if(xredisTrackingWord(Channel,R,"__redis__:invalidate",20)){
            if(Root->ChildCount!=3)return XREDIS_TRACKING_MALFORMED;
            Keys=true;Payload=Channel->Next;
        }
    }
    if(!Keys && !Broken)return XREDIS_TRACKING_OTHER;
    if(Root->Type==XREDIS_REPLY_PUSH && Root->ChildCount!=2)return XREDIS_TRACKING_MALFORMED;
    const xredisreplynode* Value=R->Nodes+Payload;
    xredistrackingevent E={R,XREDIS_TRACKING_KEYS,XREDIS_REPLY_NONE,0,0};
    if(Broken){
        if(Value->Type!=XREDIS_REPLY_INTEGER || Value->Integer<0)return XREDIS_TRACKING_MALFORMED;
        E.Kind=XREDIS_TRACKING_REDIRECT_BROKEN;E.Redirect=(uint64)Value->Integer;
    }else if(Value->Type==XREDIS_REPLY_NULL ||
        ((Value->Type==XREDIS_REPLY_ARRAY || Value->Type==XREDIS_REPLY_BULK_STRING) && (Value->Flags&XREDIS_REPLY_NULL_VALUE)))E.Kind=XREDIS_TRACKING_FLUSH;
    else {
        if(Value->Type!=XREDIS_REPLY_ARRAY)return XREDIS_TRACKING_MALFORMED;
        E.FirstKey=Value->FirstChild;E.Keys=Value->ChildCount;
        for(size_t N=E.FirstKey;N!=XREDIS_REPLY_NONE;N=R->Nodes[N].Next)
            if(R->Nodes[N].Type!=XREDIS_REPLY_BULK_STRING || (R->Nodes[N].Flags&XREDIS_REPLY_NULL_VALUE))return XREDIS_TRACKING_MALFORMED;
    }
    *Output=E;return XREDIS_TRACKING_EVENT;
}
#endif

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
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
typedef struct xredisobservation {size_t Bytes;xatomic64 Sequence;uint64 Pushes[];} xredisobservation;
/* Observe tags 3/4 select a pair without enlarging ordinary requests. */
typedef struct xrediscachingstate {
    size_t MetadataBytes;
    xredisreply *Control,*Query;
    uint64 ControlSequence,QuerySequence;
    xredisrequeststatus ControlStatus,QueryStatus;
    unsigned Seen;
    bool Written;
} xrediscachingstate;
static const uint8 xredisCachingYes[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$3\r\nYES\r\n";
static const uint8 xredisCachingNo[]="*3\r\n$6\r\nCLIENT\r\n$7\r\nCACHING\r\n$2\r\nNO\r\n";
#define XREDIS_OBSERVE_PARAMETER ,unsigned Observe
#define XREDIS_OBSERVE_ORDINARY ,false
#else
#define XREDIS_OBSERVE_PARAMETER
#define XREDIS_OBSERVE_ORDINARY
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
enum { XREDIS_TX_MULTI, XREDIS_TX_COMMANDS, XREDIS_TX_EXEC, XREDIS_TX_DISCARD };
typedef struct xredistransactionstate {
	size_t Count, Seen, Errors, CommandsBytes, Written, MetadataBytes;
	xredisreply* Error;
	xredistransactionexecution Execution;
	unsigned Phase;
	bool Await;
} xredistransactionstate;
static const uint8 xredisTransactionMulti[] = "*1\r\n$5\r\nMULTI\r\n";
static const uint8 xredisTransactionExec[] = "*1\r\n$4\r\nEXEC\r\n";
static const uint8 xredisTransactionDiscard[] = "*1\r\n$7\r\nDISCARD\r\n";
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    uint8 Observe;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	bool Transaction;
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
	xredisexclusiveowner* Owner;
#endif
	xredisrequest *SubmitHead, *SubmitTail, *CancelHead, *CancelTail;
	xredisrequest *PendingHead, *PendingTail, *SentHead, *SentTail;
	xredisclientinfo Info;
	xerror* CloseCause;
	size_t StorageBytes, SentCount, PushHead;
	bool WakeQueued, CloseDone, EnginePinned;
	/* Only the owning worker accesses these fields. */
	bool Started, TransportClosed, ReadEnded, Reading, ReadYielded, ReadPaused;
	bool Managed, TransportOpen;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	const xredisconnectplan* Plan;
	bool PlanOwned, PlanPending, PlanCancelled;
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xtlsstream* TLS;
	xredisrequest* TlsWrite;
	size_t TlsWritten, TlsWriteBytes, TlsTurnBytes, TlsTurnCalls, InitWritten;
	bool InitSending;
#endif
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xatomicptr Observation;uint64 ObservationRejected;bool ObservationPreparing;
#endif
    xredisreply* PushQueue[];
};

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
/* The tag occupies existing tail padding. Only transactions compute their
 * aligned state address; ordinary request allocation size stays unchanged. */
static xredistransactionstate* xredisTransactionState(xredisrequest* Request)
{
    size_t Alignment = _Alignof(xredistransactionstate);
    size_t Padding = (Alignment - (offsetof(xredisrequest, Wire) + Request->WireBytes) % Alignment) % Alignment;
    return (xredistransactionstate*)(Request->Wire + Request->WireBytes + Padding);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisCachingPair(const xredisrequest* R) { return R->Observe>=3; }
static xrediscachingstate* xredisCachingState(xredisrequest* R)
{
    size_t A=_Alignof(xrediscachingstate);
    size_t Padding=(A-(offsetof(xredisrequest,Wire)+R->WireBytes)%A)%A;
    return (xrediscachingstate*)(R->Wire+R->WireBytes+Padding);
}
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes);
static void xredisCachingWritten(xredisrequest* R);
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence);
#endif
static void xredisClientWork(xnetworker* Worker, ptr Data);
static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data);
static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer);
static void xredisClientTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data);
static void xredisClientRef(xredisclient* Client);
static bool xredisClientWakeLocked(xredisclient* Client);
static void xredisClientWake(xredisclient* Client);
static void xredisClientFail(xredisclient* Client, xredisrequeststatus Status, const xerror* Cause);
static void xredisClientReadyFinish(xredisclient* Client, xredisreadystatus Status, const xerror* Cause);
static void xredisClientInitPump(xredisclient* Client);
static void xredisClientInitFrame(xredisclient* Client, xredisreply* Reply, const xredisreplyvalue* Root);
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionPump(xredisrequest* Request);
static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root);
#endif

static void xredisClientTransportAbort(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { (void)xrtTlsStreamAbort(Client->TLS); return; }
#endif
	if (Client->Stream != NULL) xrtNetStreamAbort(Client->Stream); else Client->TransportClosed = true;
}

static bool xredisClientTransportOpen(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return xrtTlsStreamState(Client->TLS) == XTLS_STREAM_OPEN;
#endif
	return Client->Stream != NULL && xrtNetStreamState(Client->Stream) == XNET_STREAM_OPEN;
}

static const xnetbuf* xredisClientTransportBuffer(xredisclient* Client)
{
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) return Client->TransportOpen ? xrtTlsStreamBuffer(Client->TLS) : NULL;
#endif
	return Client->Stream == NULL ? NULL : xrtNetStreamBuffer(Client->Stream);
}

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
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	if (Client->PlanOwned) Client->Plan->Drop(Client->Plan->Data);
#endif
	if (Client->EnginePinned) xrtNetEngineUnpin(Client->Engine);
	#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtFree(xrtAtomicPtrLoad(&Client->Observation,XMEMORY_RELAXED));
#endif
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

#if defined(XREDIS_FEATURE_XREDIS_POOL)
static bool xredisClientQuotaReserve(xredisexclusiveowner* Owner, size_t Bytes)
{
    xredisownerquota* Quota = Owner == NULL ? NULL : Owner->Quota;
    if (Quota == NULL) return true;
    uint64 Requests = xrtAtomic64Load(&Quota->Requests, XMEMORY_RELAXED);
    for (;;) {
        if (Requests >= Quota->MaxRequests) return false;
        if (xrtAtomic64CompareExchange(&Quota->Requests, &Requests, Requests + 1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) break;
    }
    uint64 Current = xrtAtomic64Load(&Quota->Bytes, XMEMORY_RELAXED);
    for (;;) {
        if ((uint64)Bytes > Quota->MaxBytes - Current) {
            xrtAtomic64FetchSub(&Quota->Requests, 1, XMEMORY_RELAXED); return false;
        }
        if (xrtAtomic64CompareExchange(&Quota->Bytes, &Current, Current + (uint64)Bytes, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
    }
}
#endif

/* Drop the real reservation only after its storage/constructor has retired.
 * An idle transition wakes the exclusive session without calling it under Gate. */
static void xredisClientReservationDrop(xredisclient* Client, size_t Bytes)
{
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    xredisexclusiveowner* Owner = NULL;
#endif
    xrtMutexLock(&Client->Gate);
    --Client->Info.Requests; Client->Info.CommandBytes -= Bytes;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != NULL && Client->Owner->Quota != NULL) {
        xrtAtomic64FetchSub(&Client->Owner->Quota->Requests, 1, XMEMORY_RELAXED);
        xrtAtomic64FetchSub(&Client->Owner->Quota->Bytes, (uint64)Bytes, XMEMORY_RELAXED);
    }
#endif
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) (void)xredisClientWakeLocked(Client);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Info.Requests == 0 && Client->Owner != NULL) {
        Owner = Client->Owner; Owner->Retain(Owner->Data);
    }
#endif
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL) { Owner->Wake(Owner->Data); Owner->Release(Owner->Data); }
#endif
    xredisClientRelease(Client);
}

static void xredisRequestRelease(xredisrequest* Request)
{
	if (xrtAtomic32FetchSub(&Request->References, 1, XMEMORY_ACQ_REL) != 1) return;
	xredisclient* Client = Request->Client;
	size_t WireBytes = Request->WireBytes;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);WireBytes+=S->MetadataBytes;
        xredisReplyRelease(S->Control);xredisReplyRelease(S->Query);
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		WireBytes += xredisTransactionState(Request)->MetadataBytes;
		xredisReplyRelease(xredisTransactionState(Request)->Error);
	}
#endif
	/* The reservation protects actual storage, including the free tail. Keep
	 * the independent Client reference while dropping it AFTER the buffer. */
	if (Request->Sensitive) xrtSecureZero(Request->Wire, Request->WireBytes);
	xrtFree(Request);
	xredisClientReservationDrop(Client, WireBytes);
}

static void xredisClientWireDrop(ptr Data, cbytes Bytes, size_t Size)
{ (void)Bytes; (void)Size; xredisRequestRelease((xredisrequest*)Data); }

static void xredisClientResultDrop(ptr Value, ptr Data)
{
	(void)Data; xredisrequestresult* Result = (xredisrequestresult*)Value;
	xredisReplyRelease(Result->Reply); xrtErrorFree((xerror*)Result->Cause); xrtFree(Result);
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingResultDrop(ptr Value,ptr Data)
{
    xrediscachingresult* R=Value;
    xredisReplyRelease(R->Control.Request.Reply);xrtErrorFree((xerror*)R->Control.Request.Cause);
    xredisClientResultDrop(&R->Query.Request,Data);
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionResultDrop(ptr Value, ptr Data)
{
	xredistransactionresult* Result = (xredistransactionresult*)Value;
	xredisReplyRelease(Result->QueueError); xredisClientResultDrop(&Result->Request, Data);
}
#endif

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
	xfuturefreeproc Drop = xredisClientResultDrop;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){
        xrediscachingstate* S=xredisCachingState(Request);xrediscachingresult* R=(xrediscachingresult*)Result;
        R->ControlReceived=S->Seen!=0;
        R->Control=(xredistrackingresult){
            {S->Seen?S->ControlStatus:Status,S->Seen?XREDIS_REPLIED:Delivery,S->Control,
             S->Seen?NULL:xrtErrorRef(Cause)},S->Seen?S->ControlSequence:0};
        S->Control=NULL;R->Query.Sequence=Reply?S->QuerySequence:0;Drop=xredisCachingResultDrop;
        if(S->Seen==2)Delivery=XREDIS_REPLIED;
        if(S->Query && !Reply){
            xredisReplyRelease(S->Query);S->Query=NULL;
            xrtMutexLock(&Request->Client->Gate);++Request->Client->Info.DiscardedReplies;xrtMutexUnlock(&Request->Client->Gate);
        }
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) {
		xredistransactionstate* State = xredisTransactionState(Request);
		xredistransactionresult* Transaction = (xredistransactionresult*)Result;
		Transaction->Execution = State->Execution; Transaction->Commands = State->Count;
		Transaction->Queued = State->Seen - State->Errors; Transaction->QueueErrors = State->Errors;
		Transaction->QueueError = State->Error; State->Error = NULL; Drop = xredisTransactionResultDrop;
		if (Delivery != XREDIS_REPLIED) Delivery = State->Execution == XREDIS_TRANSACTION_NOT_EXECUTED ? XREDIS_NOT_SENT : XREDIS_MAY_HAVE_EXECUTED;
	}
#endif
	*Result = (xredisrequestresult){Status, Delivery, Reply, xrtErrorRef(Cause)};
	xrtMutexLock(&Request->Client->Gate); ++Request->Client->Info.Completed; xrtMutexUnlock(&Request->Client->Gate);
	if (!xrtPromiseResolveOwned(Promise, Result, Drop, NULL)) Drop(Result, NULL);
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
	xredisClientTransportAbort(Client);
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

#if defined(XREDIS_FEATURE_XREDIS_TLS)
/* TLS copies accepted plaintext into authenticated record storage. Nothing
 * borrows input after Send returns. A short prefix must never be replayed. */
static xtlsresult xredisClientTlsSend(xredisclient* Client, cbytes Data, size_t Size, size_t* Written)
{
	*Written = 0;
	if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return XTLS_AGAIN; }
	if (Size > Client->TlsTurnBytes) Size = Client->TlsTurnBytes;
	--Client->TlsTurnCalls; xtlsresult Result = xrtTlsStreamSend(Client->TLS, Data, Size, Written);
	Client->TlsTurnBytes -= *Written; return Result;
}

static void xredisClientTlsInitPump(xredisclient* Client)
{
	if (Client->InitAwait != XREDIS_I_END && !Client->InitSending) return;
	while (Client->InitNext < XREDIS_I_END && Client->InitLength[Client->InitNext] == 0) ++Client->InitNext;
	if (Client->InitNext == XREDIS_I_END) { xredisClientReadyFinish(Client, XREDIS_READY_OK, NULL); return; }
	size_t Index = Client->InitNext; const xredisinitstep Steps[] = {XREDIS_INIT_HELLO, XREDIS_INIT_AUTHENTICATE, XREDIS_INIT_NAME, XREDIS_INIT_DATABASE};
	xrtMutexLock(&Client->Gate); Client->Info.InitStep = Steps[Index]; xrtMutexUnlock(&Client->Gate);
	Client->InitAwait = Index; Client->InitSending = true;
	while (Client->InitWritten < Client->InitLength[Index] && !xredisClientClosing(Client)) {
		xredisinitwire* Wire = Client->InitWire; (void)xrtAtomic32FetchAdd(&Wire->References, 1, XMEMORY_RELAXED);
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Wire->Data + Client->InitOffset[Index] + Client->InitWritten,
			Client->InitLength[Index] - Client->InitWritten, &Written);
		Client->InitWritten += Written; xredisInitWireRelease(Wire);
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Written == 0) return;
	}
	if (Client->InitWritten == Client->InitLength[Index]) { Client->InitWritten = 0; Client->InitSending = false; }
}
#endif

static void xredisClientInitPump(xredisclient* Client)
{
	if (Client->ReadyPromise == NULL || !Client->TransportOpen) return;
	if (xrtTimer() >= Client->InitDeadline) { xredisClientInitReject(Client, XREDIS_READY_TIMEOUT); return; }
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsInitPump(Client); return; }
#endif
	if (Client->InitAwait != XREDIS_I_END) return;
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
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->InitSending) { xredisReplyRelease(Reply); xredisClientInitReject(Client, XREDIS_READY_PROTOCOL); return; }
#endif
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

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static void xredisCachingFinish(xredisrequest* R)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(!S->Written || S->Seen!=2)return;
    if(R->Promise){
        xredisrequeststatus Status=S->QueryStatus;
        if(xrtAtomic32Load(&R->Cancelled,XMEMORY_ACQUIRE))Status=XREDIS_REQUEST_CANCELLED;
        else if(xrtTimer()>=R->Deadline)Status=XREDIS_REQUEST_TIMEOUT;
        xredisreply* Reply=S->Query;S->Query=NULL;
        if(Status==XREDIS_REQUEST_CANCELLED || Status==XREDIS_REQUEST_TIMEOUT){
            xredisReplyRelease(Reply);Reply=NULL;
            xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);
        }
        xredisClientComplete(R,Status,XREDIS_REPLIED,Reply,NULL);
    }
    xredisClientUnlink(R);R->Phase=XREDIS_R_DONE;
    xredisClientCancelTimer(R);xredisRequestRelease(R);xredisClientWake(C);
}
static void xredisCachingWritten(xredisrequest* R)
{
    if(!xredisCachingPair(R))return;
    xredisCachingState(R)->Written=true;xredisCachingFinish(R);
}
static void xredisCachingFrame(xredisrequest* R,xredisreply* Reply,const xredisreplyvalue* Root,uint64 Sequence)
{
    xrediscachingstate* S=xredisCachingState(R);xredisclient* C=R->Client;
    if(S->Seen>=2){xredisReplyRelease(Reply);xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;}
    bool Error=Root->Type==XREDIS_REPLY_SIMPLE_ERROR || Root->Type==XREDIS_REPLY_BULK_ERROR;
    if(S->Seen++==0){
        S->ControlStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(!Error && !(Root->Type==XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data,"OK"))){
            S->ControlStatus=XREDIS_REQUEST_PROTOCOL;xredisReplyRelease(Reply);
            xredisClientFail(C,XREDIS_REQUEST_PROTOCOL,NULL);return;
        }
        if(R->Promise){S->Control=Reply;S->ControlSequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
    }else{
        S->QueryStatus=Error?XREDIS_REQUEST_SERVER_ERROR:XREDIS_REQUEST_OK;
        if(R->Promise){S->Query=Reply;S->QuerySequence=Sequence;}
        else {xredisReplyRelease(Reply);xrtMutexLock(&C->Gate);++C->Info.DiscardedReplies;xrtMutexUnlock(&C->Gate);}
        xredisCachingFinish(R);
    }
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void xredisTransactionObserve(xredisrequest* Request)
{
	if (Request->Promise != NULL) {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) xredisClientAbandon(Request, XREDIS_REQUEST_CANCELLED);
		else if (xrtTimer() >= Request->Deadline) xredisClientAbandon(Request, XREDIS_REQUEST_TIMEOUT);
	}
}

static void xredisTransactionNext(xredisrequest* Request, unsigned Phase)
{
	xredistransactionstate* State = xredisTransactionState(Request);
	State->Phase = Phase; State->Written = 0; State->Await = false; xredisClientWake(Request->Client);
}

static void xredisTransactionFinish(xredisrequest* Request, xredisrequeststatus Status, xredisreply* Reply)
{
	xredisclient* Client = Request->Client;
	if (Request->Promise == NULL) {
		xredisReplyRelease(Reply);
		xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	} else {
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) { xredisReplyRelease(Reply); Reply = NULL; }
		xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisTransactionPump(xredisrequest* Request)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	xredisTransactionObserve(Request);
	if (xredisClientClosing(Client)) return;
	if (State->Await) return;
	if (Request->Promise == NULL && State->Written == 0) {
		if (State->Phase == XREDIS_TX_MULTI) {
			xredisTransactionFinish(Request, XREDIS_REQUEST_CANCELLED, NULL); return;
		}
		if (State->Phase == XREDIS_TX_COMMANDS || State->Phase == XREDIS_TX_EXEC)
			State->Phase = XREDIS_TX_DISCARD;
	}
	size_t Offset = 0, Length = sizeof(xredisTransactionMulti) - 1u;
	if (State->Phase == XREDIS_TX_COMMANDS) { Offset = Length; Length = State->CommandsBytes; }
	else if (State->Phase == XREDIS_TX_EXEC) { Offset = Length + State->CommandsBytes; Length = sizeof(xredisTransactionExec) - 1u; }
	else if (State->Phase == XREDIS_TX_DISCARD) {
		Offset = Length + State->CommandsBytes + sizeof(xredisTransactionExec) - 1u; Length = sizeof(xredisTransactionDiscard) - 1u;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) {
		while (State->Written < Length && !xredisClientClosing(Client)) {
			if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) { xredisClientWake(Client); return; }
			if (Client->TlsWrite == NULL) { Client->TlsWrite = Request; xredisRequestRef(Request); }
			size_t Written = 0;
			xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Offset + State->Written, Length - State->Written, &Written);
			State->Written += Written;
			if (State->Phase == XREDIS_TX_EXEC && Written != 0) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
			if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
				xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
					XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, Result == XTLS_ERROR ? xrtGetError() : NULL); return;
			}
			if (Written == 0) return;
		}
		if (State->Written != Length) return; /* Concurrent Close owns the remaining write reference. */
		Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
	} else
#endif
	{
		xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire + Offset, Length, xredisClientWireDrop, Request);
		if (Result != XNET_RESULT_OK) {
			xredisRequestRelease(Request);
			if (Result != XNET_RESULT_AGAIN) xredisClientFail(Client,
				xrtErrorKind(xrtGetError()) == XERR_MEMORY && Result == XNET_RESULT_ERROR ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XNET_RESULT_ERROR ? xrtGetError() : NULL);
			return;
		}
		State->Written = Length;
		if (State->Phase == XREDIS_TX_EXEC) State->Execution = XREDIS_TRANSACTION_MAY_HAVE_EXECUTED;
	}
	State->Await = true;
	/* QUEUED replies may arrive while a fragmented TLS command batch is still
	 * being sent. Only cross this boundary after BOTH the full send and N replies. */
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen == State->Count) {
		xredisTransactionObserve(Request);
		xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
	}
}

static void xredisTransactionFrame(xredisrequest* Request, xredisreply* Reply, const xredisreplyvalue* Root)
{
	xredisclient* Client = Request->Client; xredistransactionstate* State = xredisTransactionState(Request);
	bool Error = Root->Type == XREDIS_REPLY_SIMPLE_ERROR || Root->Type == XREDIS_REPLY_BULK_ERROR;
	if (State->Phase == XREDIS_TX_COMMANDS && State->Seen < State->Count) {
		xredisTransactionObserve(Request);
		if (!Error && !(Root->Type == XREDIS_REPLY_SIMPLE_STRING && xredisClientInitLiteral(Root->Data, "QUEUED"))) goto protocol;
		++State->Seen;
		if (Error) {
			++State->Errors;
			if (Request->Promise != NULL && State->Error == NULL) { State->Error = Reply; Reply = NULL; }
		}
		xredisReplyRelease(Reply);
		if (State->Await && State->Seen == State->Count)
			xredisTransactionNext(Request, State->Errors != 0 || Request->Promise == NULL ? XREDIS_TX_DISCARD : XREDIS_TX_EXEC);
		return;
	}
	if (!State->Await) goto protocol;
	if (State->Phase == XREDIS_TX_MULTI) {
		if (Error) { xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return; }
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisTransactionObserve(Request);
		xredisReplyRelease(Reply);
		xredisTransactionNext(Request, Request->Promise == NULL ? XREDIS_TX_DISCARD : State->Count ? XREDIS_TX_COMMANDS : XREDIS_TX_EXEC); return;
	}
	if (State->Phase == XREDIS_TX_EXEC) {
		if (Root->Type == XREDIS_REPLY_ARRAY && !(Root->Flags & XREDIS_REPLY_NULL_VALUE) && Root->ChildCount == State->Count)
			State->Execution = XREDIS_TRANSACTION_EXECUTED;
		else if (Root->Type == XREDIS_REPLY_NULL || (Root->Type == XREDIS_REPLY_ARRAY && (Root->Flags & XREDIS_REPLY_NULL_VALUE)))
			State->Execution = XREDIS_TRANSACTION_ABORTED;
		else if (Error) {
			if (xredisClientInitCode(Root->Data, "EXECABORT")) State->Execution = XREDIS_TRANSACTION_ABORTED;
			else {
				/* An unknown EXEC error does not certify that the connection left
				 * MULTI. Do not expose possibly dirty state to following calls. */
				xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			}
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		} else goto protocol;
		xredisTransactionFinish(Request, XREDIS_REQUEST_OK, Reply); return;
	}
	if (State->Phase == XREDIS_TX_DISCARD) {
		if (Error) {
			xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
			xredisTransactionFinish(Request, XREDIS_REQUEST_SERVER_ERROR, Reply); return;
		}
		if (Root->Type != XREDIS_REPLY_SIMPLE_STRING || !xredisClientInitLiteral(Root->Data, "OK")) goto protocol;
		xredisReplyRelease(Reply); xredisTransactionFinish(Request, State->Errors ? XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_CANCELLED, NULL); return;
	}
protocol:
	xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL);
}
#endif

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

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
static xredisrequeststatus xredisClientConnectFailure(const xerror* Cause)
{
	xerrkind Kind = xrtErrorKind(Cause);
	return Kind == XERR_MEMORY ? XREDIS_REQUEST_MEMORY :
		(Kind == XERR_AGAIN || Kind == XERR_RANGE) ? XREDIS_REQUEST_LIMIT : XREDIS_REQUEST_DISCONNECTED;
}

void __xredisClientConnectDone(xredisclient* Client, xnetresult Result,
	xnetstream* Stream, const xerror* Cause)
{
	if (Stream != NULL) {
		Client->Stream = Stream;
		/* A close requested during DNS may have observed no transport. A late
		 * winning reference still has to reach its ACTUAL TCP close barrier. */
		Client->TransportClosed = xrtNetStreamState(Stream) == XNET_STREAM_CLOSED;
		if (xredisClientClosing(Client)) xrtNetStreamAbort(Stream);
	} else {
		/* A cancelled pending Dial never created a stream. Its terminal callback
		 * is the physical transport barrier even when public Close won first. */
		Client->TransportClosed = true;
		if (!xredisClientClosing(Client)) {
		xredisrequeststatus Status = Result == XNET_RESULT_TIMEOUT || Result == XNET_RESULT_CANCELLED ?
			XREDIS_REQUEST_DISCONNECTED : xredisClientConnectFailure(Cause);
		xredisClientFail(Client, Status, Cause);
		xredisreadystatus Ready = Result == XNET_RESULT_TIMEOUT ? XREDIS_READY_TIMEOUT :
			Result == XNET_RESULT_CANCELLED ? XREDIS_READY_CLOSED : xredisClientReadyFailure(Status);
		xredisClientReadyFinish(Client, Ready, Cause);
		}
	}
	xredisClientWake(Client);
}

void __xredisClientConnectRetired(xredisclient* Client)
{ Client->PlanPending = false; xredisClientWake(Client); }
#endif

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsOpen(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnOpen(NULL, Data); }
static void xredisClientTlsRead(xtlsstream* Stream, const xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }
static void xredisClientTlsEnd(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientOnEnd(NULL, Data); }
static void xredisClientTlsWritable(xtlsstream* Stream, ptr Data)
{ (void)Stream; xredisClientWake((xredisclient*)Data); }
static void xredisClientTlsClose(xtlsstream* Stream, xnetresult Result, const xerror* Error, ptr Data)
{
	(void)Stream; xredisclient* Client = (xredisclient*)Data; xredisClientOnClose(NULL, Result, Error, Data);
	if (Client->ReadyPromise != NULL && xrtErrorKind(Error) == XERR_TIMEOUT) xredisClientReadyFinish(Client, XREDIS_READY_TIMEOUT, Error);
}
static const xtlsstreamevents xredisClientTlsEvents = {
	xredisClientTlsOpen, xredisClientTlsRead, xredisClientTlsEnd, xredisClientTlsWritable, xredisClientTlsWritable, xredisClientTlsClose, NULL
};
const xnetstreamevents* __xredisClientTlsPendingEvents(void)
{
	static const xnetstreamevents Events = {NULL, NULL, NULL, NULL, NULL, NULL, xredisClientOnClose}; return &Events;
}
bool __xredisClientConnectTls(xredisclient* Client, xnetstream* Transport, const xredistlsplan* Plan)
{
	if (xredisClientClosing(Client) || xrtNetStreamState(Transport) != XNET_STREAM_OPEN) {
		__xredisClientConnectDone(Client, XNET_RESULT_OK, Transport, NULL);
		if (!xredisClientClosing(Client)) xredisClientFail(Client, XREDIS_REQUEST_DISCONNECTED, NULL);
		return false;
	}
	Client->TransportClosed = false; Client->TlsWriteBytes = Plan->WriteBytes;
	if (xrtTlsStreamAttach(Transport, Plan->Session, &Plan->Config, &xredisClientTlsEvents, Client, &Client->TLS)) return true;
	xerror* Cause = xrtErrorRef(xrtGetError()); __xredisClientConnectDone(Client, XNET_RESULT_ERROR, Transport, Cause);
	xredisClientFail(Client, xredisClientConnectFailure(Cause), Cause); xrtErrorFree(Cause); return false;
}
#endif

static void xredisClientFrame(xredisclient* Client, xredisreply* Reply)
{
	xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisReplyInfo(Reply, &Info);
	xredisreplyvalue Root; xredisReplyGet(Reply, Info.Root, &Root);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xredisobservation* Observation=xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE);uint64 Sequence=0;
    if(Observation){
        /* One IO worker writes; external Info readers only load this atomic. */
        Sequence=xrtAtomic64Load(&Observation->Sequence,XMEMORY_RELAXED);
        if(Sequence==UINT64_MAX){xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_LIMIT,NULL);return;}
        ++Sequence;xrtAtomic64Store(&Observation->Sequence,Sequence,XMEMORY_RELEASE);
    }
#endif
    if (Root.Type == XREDIS_REPLY_PUSH) {
		xrtMutexLock(&Client->Gate);
		bool Fits = Client->Info.Pushes < Client->Config.MaxPushes &&
			Info.ReservedBytes <= Client->Config.MaxPushBytes - Client->Info.PushBytes;
		if (Fits) {
			size_t Index = (Client->PushHead + Client->Info.Pushes) % Client->Config.MaxPushes;
			Client->PushQueue[Index] = Reply; ++Client->Info.Pushes; Client->Info.PushBytes += Info.ReservedBytes;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            if(Observation)Observation->Pushes[Index]=Sequence;
#endif
		}
		xrtMutexUnlock(&Client->Gate);
		if (!Fits) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_LIMIT, NULL); }
		return;
	}
	if (Client->ReadyPromise != NULL) { xredisClientInitFrame(Client, Reply, &Root); return; }
	xredisrequest* Request = Client->SentHead;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Request != NULL && Request == Client->TlsWrite
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        && !xredisCachingPair(Request)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		&& !Request->Transaction
#endif
	) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#endif
	if (Request == NULL) { xredisReplyRelease(Reply); xredisClientFail(Client, XREDIS_REQUEST_PROTOCOL, NULL); return; }
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Request->Transaction) { xredisTransactionFrame(Request, Reply, &Root); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(xredisCachingPair(Request)){xredisCachingFrame(Request,Reply,&Root,Sequence);return;}
    if(Request->Observe==2 && Root.Type!=XREDIS_REPLY_SIMPLE_ERROR && Root.Type!=XREDIS_REPLY_BULK_ERROR &&
        !(Root.Type==XREDIS_REPLY_SIMPLE_STRING && Root.Data.Size==2 && !memcmp(Root.Data.Data,"OK",2))){
        xredisReplyRelease(Reply);xredisClientFail(Client,XREDIS_REQUEST_PROTOCOL,NULL);return;
    }
#endif
	if (Request->Promise != NULL) {
		xredisrequeststatus Status = Root.Type == XREDIS_REPLY_SIMPLE_ERROR || Root.Type == XREDIS_REPLY_BULK_ERROR ?
			XREDIS_REQUEST_SERVER_ERROR : XREDIS_REQUEST_OK;
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE)) Status = XREDIS_REQUEST_CANCELLED;
		else if (xrtTimer() >= Request->Deadline) Status = XREDIS_REQUEST_TIMEOUT;
		if (Status == XREDIS_REQUEST_CANCELLED || Status == XREDIS_REQUEST_TIMEOUT) {
			xredisReplyRelease(Reply); Reply = NULL;
			xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
		}
		#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(Request->Observe)((xredistrackingresult*)Request->Result)->Sequence=Reply?Sequence:0;
#endif
        xredisClientComplete(Request, Status, XREDIS_REPLIED, Reply, NULL);
	} else {
		xredisReplyRelease(Reply); xrtMutexLock(&Client->Gate); ++Client->Info.DiscardedReplies; xrtMutexUnlock(&Client->Gate);
	}
	xredisClientUnlink(Request); Request->Phase = XREDIS_R_DONE;
	xredisClientCancelTimer(Request); xredisRequestRelease(Request); xredisClientWake(Client);
}

static void xredisClientReadBuffer(xredisclient* Client, const xnetbuf* Buffer)
{
	if (Client->Reading || xredisClientClosing(Client)) return;
	Client->Reading = true; size_t Bytes = 0, Frames = 0;
	while (Bytes < Client->Config.ReadBytes && Frames < Client->Config.ReadFrames && !xredisClientClosing(Client)) {
		xnetspan Span; if (!xrtNetBufFront(Buffer, &Span)) break;
		size_t Available = Client->Config.ReadBytes - Bytes;
		if (Span.Size > Available) Span.Size = Available;
		xredisreply* Reply = NULL; size_t Consumed = 0;
		xredisreplystatus Status = xredisReplyReaderFeed(Client->Reader, (xbytesview){Span.Data, Span.Size}, &Consumed, &Reply);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS != NULL) (void)xrtTlsStreamConsume(Client->TLS, Consumed); else
#endif
		(void)xrtNetBufConsume((xnetbuf*)Buffer, Consumed);
		Bytes += Consumed;
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
		Client->ReadYielded = true; Client->ReadPaused = true;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TLS == NULL)
#endif
		xrtNetStreamPause(Client->Stream);
		xredisClientWake(Client);
	}
}

static void xredisClientRead(xnetstream* Stream, xnetbuf* Buffer, ptr Data)
{ (void)Stream; xredisClientReadBuffer((xredisclient*)Data, Buffer); }

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

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static void xredisClientTlsPump(xredisclient* Client)
{
	while (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
		if (Client->TlsTurnBytes == 0 || Client->TlsTurnCalls == 0) {
			if (Client->TlsWrite != NULL || (Client->PendingHead != NULL && Client->SentCount < Client->Config.MaxInFlight)) xredisClientWake(Client);
			return;
		}
		xredisrequest* Request = Client->TlsWrite;
		if (Request == NULL) {
			if (Client->PendingHead == NULL || Client->SentCount >= Client->Config.MaxInFlight) return;
			Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
			if (Request->Transaction) {
				if (Client->SentCount != 0) return;
				xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
			}
#endif
			if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
				--Client->TlsTurnCalls;
				xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
			}
			xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
			Client->TlsWrite = Request; Client->TlsWritten = 0;
		}
		size_t Written = 0; xtlsresult Result = xredisClientTlsSend(Client, Request->Wire + Client->TlsWritten, Request->WireBytes - Client->TlsWritten, &Written);
		Client->TlsWritten += Written;
		if (Result == XTLS_ERROR || Result == XTLS_CLOSED) {
			/* Zero accepted bytes still means NOT_SENT. A partial command is a
			 * sent FIFO obligation even after the caller abandons its Future. */
			if (Client->TlsWritten == 0) { xredisClientUnlink(Request); xredisClientAppend(Client, Request, false); }
			xredisClientFail(Client, Result == XTLS_ERROR && xrtErrorKind(xrtGetError()) == XERR_MEMORY ? XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED,
				Result == XTLS_ERROR ? xrtGetError() : NULL); return;
		}
		if (Client->TlsWritten == Request->WireBytes) {
			Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Request);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
			continue;
		}
		if (Written == 0) {
			if (Client->TlsWritten == 0 && Client->TlsWrite != NULL) {
				/* No byte was accepted: keep original FIFO position and report
				 * NOT_SENT if the caller now expires/cancels while blocked. */
				xredisClientUnlink(Request); Request->Phase = XREDIS_R_PENDING;
				Request->Next = Client->PendingHead;
				if (Client->PendingHead != NULL) Client->PendingHead->Previous = Request; else Client->PendingTail = Request;
				Client->PendingHead = Request; Client->TlsWrite = NULL; xredisRequestRelease(Request);
			}
			return;
		}
	}
}
#endif

static void xredisClientPump(xredisclient* Client)
{
	if (Client->ReadyPromise != NULL) return;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
	if (Client->SentHead != NULL && Client->SentHead->Transaction) { xredisTransactionPump(Client->SentHead); return; }
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Client->TLS != NULL) { xredisClientTlsPump(Client); return; }
#endif
	for (size_t Work = 0; Work < Client->Config.WorkRequests && Client->PendingHead != NULL &&
		Client->SentCount < Client->Config.MaxInFlight && !xredisClientClosing(Client) &&
		xredisClientTransportOpen(Client); ++Work) {
		xredisrequest* Request = Client->PendingHead;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
		if (Request->Transaction) {
			if (Client->SentCount != 0) return;
			xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisTransactionPump(Request); return;
		}
#endif
		if (xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) || xrtTimer() >= Request->Deadline) {
			xredisClientAbandon(Request, xrtAtomic32Load(&Request->Cancelled, XMEMORY_ACQUIRE) ?
				XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT); continue;
		}
		xredisClientUnlink(Request); xredisClientAppend(Client, Request, true); xredisRequestRef(Request);
		xnetresult Result = xrtNetStreamSendRef(Client->Stream, Request->Wire, Request->WireBytes, xredisClientWireDrop, Request);
		if (Result == XNET_RESULT_OK) {
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
            xredisCachingWritten(Request);
#endif
            continue;
        }
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
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	Client->TlsTurnBytes = Client->TlsWriteBytes; Client->TlsTurnCalls = Client->Config.WorkRequests;
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
			if (Client->Plan != NULL) {
				Client->PlanPending = true;
				if (!Client->Plan->Start(Client->Plan->Data, Client, Client->Engine,
					xrtNetWorkerIndex(Client->Worker), &Client->Config.Stream, &xredisClientEvents)) {
					Client->PlanPending = false;
					xredisClientFail(Client, xredisClientConnectFailure(xrtGetError()), xrtGetError());
				}
			} else
#endif
			{
			Client->Stream = xrtNetStreamConnect(Client->Engine, &Client->Remote, xrtNetWorkerIndex(Client->Worker),
				&Client->Config.Stream, &xredisClientEvents, Client);
			if (Client->Stream == NULL) xredisClientFail(Client, xrtErrorKind(xrtGetError()) == XERR_MEMORY ?
				XREDIS_REQUEST_MEMORY : XREDIS_REQUEST_DISCONNECTED, xrtGetError());
			}
		} else Client->TransportClosed = true;
	}
	xredisClientIngest(Client);
	if (!xredisClientClosing(Client)) {
		const xnetbuf* Buffer = xredisClientTransportBuffer(Client);
		if (Buffer != NULL && xrtNetBufSize(Buffer) != 0) xredisClientReadBuffer(Client, Buffer);
		if (!xredisClientClosing(Client) && !Client->ReadEnded && !Client->ReadYielded && Client->ReadPaused) {
			Client->ReadPaused = false;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
			if (Client->TLS == NULL && Client->Stream != NULL)
#endif
			xrtNetStreamResume(Client->Stream);
		}
		if (Client->ReadEnded && !Client->ReadYielded && !xredisClientClosing(Client)) {
			bool Complete = xredisReplyReaderFinish(Client->Reader);
			xredisClientFail(Client, Complete ? XREDIS_REQUEST_DISCONNECTED : XREDIS_REQUEST_PROTOCOL,
				Complete ? NULL : xrtGetError());
		}
		if (!xredisClientClosing(Client) && xredisClientTransportOpen(Client)) {
			if (Client->Managed && Client->ReadyPromise != NULL) xredisClientInitPump(Client);
			else xredisClientPump(Client);
		}
	}
	if (xredisClientClosing(Client)) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanPending && !Client->PlanCancelled) {
			Client->PlanCancelled = true; Client->Plan->Cancel(Client->Plan->Data);
		}
#endif
		if (Client->ReadyPromise != NULL)
			xredisClientReadyFinish(Client, xredisClientReadyFailure(Client->Info.CloseStatus), Client->CloseCause);
		if (!Client->TransportClosed) xredisClientTransportAbort(Client);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		if (Client->TlsWrite != NULL) {
			xredisrequest* Write = Client->TlsWrite; Client->TlsWrite = NULL; Client->TlsWritten = 0; xredisRequestRelease(Write);
		}
#endif
		xredisClientRetire(Client);
	}
	xrtMutexLock(&Client->Gate);
	bool Again = Client->SubmitHead != NULL || Client->CancelHead != NULL ||
		(Client->Info.State >= XREDIS_CLIENT_CLOSING && (Client->PendingHead != NULL || Client->SentHead != NULL));
	if (Again) (void)xredisClientWakeLocked(Client);
	bool Done = Client->TransportClosed && Client->Info.Requests == 0 && Client->Info.InitBytes == 0 &&
		Client->InitTimer == 0 && !Client->WakeQueued && !Client->CloseDone;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    Done=Done && !Client->ObservationPreparing;
#endif
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Done = Done && !Client->PlanPending;
#endif
	if (Done) { Client->CloseDone = true; Client->Info.State = XREDIS_CLIENT_CLOSED; Client->EnginePinned = false; }
	xrtMutexUnlock(&Client->Gate);
	if (Done) {
		xpromise* Promise = Client->ClosePromise; Client->ClosePromise = NULL;
		xrtNetStreamDestroy(Client->Stream); Client->Stream = NULL;
		xredisReplyReaderDestroy(Client->Reader); Client->Reader = NULL;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		if (Client->PlanOwned) {
			const xredisconnectplan* Plan = Client->Plan; Client->Plan = NULL; Client->PlanOwned = false;
			Plan->Drop(Plan->Data);
		}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS)
		xrtTlsStreamDestroy(Client->TLS); Client->TLS = NULL;
#endif
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
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{
	xredisclientconfig Copy;
	if (Engine == NULL || (Remote == NULL && Plan == NULL) || Result == NULL ||
		(Remote != NULL && (!xrtMemRangeValid(Remote, sizeof(*Remote)) || xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))) ||
		!xrtMemRangeValid(Result, sizeof(*Result)) ||
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
		(Remote != NULL && ((Remote->Family != XNET_FAMILY_IPV4 && Remote->Family != XNET_FAMILY_IPV6) || Remote->Port == 0)) ||
		!xrtNetStreamConfigValid(&Copy.Stream))
		return xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client config or endpoint");
	if (!xrtNetEnginePin(Engine)) return false;
	size_t Bytes = sizeof(xredisclient) + Copy.MaxPushes * sizeof(xredisreply*);
	xredisclient* Client = (xredisclient*)xrtCalloc(1, Bytes);
	if (Client == NULL) { xrtNetEngineUnpin(Engine); return false; }
	Client->Engine = Engine; Client->EnginePinned = true; Client->Config = Copy;
	if (Remote != NULL) Client->Remote = *Remote;
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->Plan = Plan;
#endif
	Client->StorageBytes = Bytes; Client->Info.Size = sizeof(Client->Info); Client->Info.CloseStatus = XREDIS_REQUEST_CLOSED;
	Client->Managed = Init != NULL; Client->InitAwait = XREDIS_I_END;
	if (Client->Managed) {
		Client->InitMode = InitCopy.Mode; Client->InitFlags = InitCopy.Flags;
		Client->InitDeadline = xredisClientDeadline(Entered, InitCopy.Timeout);
	}
	Client->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
	xrtAtomic32Init(&Client->References, 1);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    xrtAtomicPtrInit(&Client->Observation,NULL);
#endif
	if (!xrtMutexInit(&Client->Gate)) { xrtNetEngineUnpin(Engine); xrtFree(Client); return false; }
	if (!xrtNetPostInit(&Client->Wake) || !xredisReplyReaderCreate(&Copy.Replies, &Client->Reader) ||
		(Client->ClosePromise = xrtPromiseCreate(&Client->CloseFuture, NULL)) == NULL ||
		(Client->ReadyPromise = xrtPromiseCreate(&Client->ReadyFuture, NULL)) == NULL ||
		(Client->ReadyResult = (xredisreadyresult*)xrtMalloc(sizeof(*Client->ReadyResult))) == NULL ||
		(Client->Managed && !xredisClientInitBuild(Client, &InitCopy))) { xredisClientRelease(Client); return false; }
	xredisClientRef(Client); /* Active connection, even before Stream is created. */
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
	Client->PlanOwned = Plan != NULL;
#endif
	xrtMutexLock(&Client->Gate); bool Posted = xredisClientWakeLocked(Client); xrtMutexUnlock(&Client->Gate);
	if (!Posted) {
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
		Client->PlanOwned = false;
#endif
		xredisinitwire* Wire = Client->InitWire; Client->InitWire = NULL; xredisInitWireRelease(Wire);
		xredisClientRelease(Client); xredisClientRelease(Client); return false;
	}
	*Result = Client; return true;
}

XREDIS_API bool xredisClientConnect(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, xredisclient** Result)
{ return xredisClientCreate(Engine, Remote, Affinity, Config, NULL, xrtTimer(), NULL, Result); }

XREDIS_API bool xredisClientConnectInit(xnetengine* Engine, const xnetaddr* Remote,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init, xredisclient** Result)
{
	double Entered = xrtTimer(); xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	return xredisClientCreate(Engine, Remote, Affinity, Config, Init, Entered, NULL, Result);
}

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
bool __xredisClientCreatePlanned(xnetengine* Engine, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	double Entered, const xredisconnectplan* Plan, xredisclient** Result)
{ return xredisClientCreate(Engine, NULL, Affinity, Config, Init, Entered, Plan, Result); }
#endif

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

static bool xredisClientManagedControl(const xbytesview* Arguments, size_t Count)
{
	return xredisClientWord(Arguments[0], "HELLO") || xredisClientWord(Arguments[0], "AUTH") ||
		xredisClientWord(Arguments[0], "SELECT") || xredisClientWord(Arguments[0], "RESET") ||
		xredisClientWord(Arguments[0], "MULTI") || xredisClientWord(Arguments[0], "EXEC") ||
		xredisClientWord(Arguments[0], "DISCARD") || xredisClientWord(Arguments[0], "WATCH") ||
		xredisClientWord(Arguments[0], "UNWATCH") ||
		(Count > 1 && xredisClientWord(Arguments[0], "CLIENT") && xredisClientWord(Arguments[1], "SETNAME"));
}

/* Gate held. Ordinary calls cannot enter a claimed connection. Owned calls
 * must recheck at publication, since a concurrent commit/abort may seal it
 * while the caller is allocating/encoding outside Gate. */
static xredisadmission xredisClientOwnerAdmission(xredisclient* Client, xredisexclusiveowner* Owner, bool Control)
{
    if (Client->Info.State >= XREDIS_CLIENT_CLOSING) return XREDIS_ADMISSION_CLOSED;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Client->Owner != Owner) return Owner == NULL ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_CLOSED;
    if (Owner != NULL) {
#if defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Owner->Quota != NULL && xrtAtomic32Load(&Owner->Quota->Closing, XMEMORY_ACQUIRE)) return XREDIS_ADMISSION_CLOSED;
#endif
        uint32 State = xrtAtomic32Load(&Owner->State, XMEMORY_ACQUIRE);
        if ((!Control && State != XREDIS_OWNER_ACTIVE) || State == XREDIS_OWNER_CLOSED) return XREDIS_ADMISSION_CLOSED;
    }
#else
    (void)Owner; (void)Control;
#endif
    return XREDIS_ADMISSION_ACCEPTED;
}

#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static bool xredisClientOwnerSwap(xredisexclusiveowner* Owner, uint32 From, uint32 To)
{ return xrtAtomic32CompareExchange(&Owner->State, &From, To, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE); }
#endif

xredisadmission __xredisClientClaim(xredisclient* Client, xredisexclusiveowner* Owner,
    xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (Client == NULL || Owner == NULL || Engine == NULL || Worker == NULL ||
        xrtMemRangesOverlap(Client, Client ? Client->StorageBytes : 0, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid exclusive owner"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed ? XREDIS_ADMISSION_ERROR :
        Client->Info.State != XREDIS_CLIENT_OPEN || !Client->Info.Ready || Client->Info.Requests || Client->Owner ?
        XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Client->Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker; }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate);
    if (Admission == XREDIS_ADMISSION_ERROR) xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "exclusive sessions require a live managed connection");
    return Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
xredisadmission __xredisClientClaimStarting(xredisclient* Client, xredisexclusiveowner* Owner, xnetengine* Engine)
{
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING ? XREDIS_ADMISSION_CLOSED :
        !Client->Managed || Client->Engine != Engine ? XREDIS_ADMISSION_ERROR :
        Client->Info.Requests || Client->Owner ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xrtNetEnginePin(Engine)) Admission = XREDIS_ADMISSION_ERROR;
        else { Client->Owner = Owner; xredisClientRef(Client); }
    }
    xrtMutexUnlock(&Client->Gate); return Admission;
}
#endif
static bool xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner, bool Unused)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xredisexclusiveowner* Parent = NULL;
#else
    (void)Unused;
#endif
    xrtMutexLock(&Client->Gate);
    bool Idle = Client->Owner == Owner && Client->Info.Requests == 0;
    if (Idle) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        Parent = Owner->Parent;
        if (Parent != NULL) {
            if (Unused) {
                Parent->Retain(Parent->Data);
                xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
                if (!xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE))
                    (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
            }
        }
        Client->Owner = Parent;
#else
        Client->Owner = NULL;
#endif
    }
    xrtMutexUnlock(&Client->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (Parent != NULL && Unused) { Parent->Wake(Parent->Data); Parent->Release(Parent->Data); }
#endif
    return Idle;
}
bool __xredisClientUnclaim(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, false); }

#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
bool __xredisClientUnclaimUnused(xredisclient* Client, xredisexclusiveowner* Owner)
{ return xredisClientUnclaim(Client, Owner, true); }

xredisadmission __xredisClientClaimNested(xredisclient* Client, xredisexclusiveowner* Parent,
    xredisexclusiveowner* Owner, xnetengine** Engine, xnetworker** Worker, ptr Output)
{
    if (xrtMemRangesOverlap(Client, Client->StorageBytes, Output, sizeof(xfuture*))) {
        xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "nested output overlaps client"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Client->Gate);
    xredisadmission Admission = Client->Info.State >= XREDIS_CLIENT_CLOSING || Client->Owner != Parent ||
        xrtAtomic32Load(&Parent->Quota->Closing, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Parent->State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ? XREDIS_ADMISSION_CLOSED :
        Client->Info.Requests ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        if (!xredisClientOwnerSwap(Parent, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
        else if (!xrtNetEnginePin(Client->Engine)) {
            (void)xredisClientOwnerSwap(Parent, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE); Admission = XREDIS_ADMISSION_ERROR;
        } else {
            Owner->Parent = Parent; Owner->Quota = Parent->Quota;
            xrtAtomic32Store(&Parent->Nested, 1, XMEMORY_RELEASE);
            if (Owner->Deadline > Parent->Deadline) Owner->Deadline = Parent->Deadline;
            Client->Owner = Owner; xredisClientRef(Client); *Engine = Client->Engine; *Worker = Client->Worker;
        }
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) ++Client->Info.Rejected;
    xrtMutexUnlock(&Client->Gate); return Admission;
}

/* Caller owns the parent/client lifetime. Retain the child under the native
 * Gate, then request abandonment outside it. A temporary zero-request WATCH
 * interval is not permission to return the parent slot. */
bool __xredisClientOwnerReady(xredisclient* Client, xredisexclusiveowner* Parent, bool Abandon)
{
    xredisexclusiveowner* Child = NULL;
    xrtMutexLock(&Client->Gate); bool Ready = Client->Owner == Parent && !xrtAtomic32Load(&Parent->Nested, XMEMORY_ACQUIRE);
    if (!Ready && Abandon && Client->Owner != NULL && Client->Owner->Parent == Parent) {
        Child = Client->Owner; Child->Retain(Child->Data);
    }
    xrtMutexUnlock(&Client->Gate);
    if (Child != NULL) { Child->Abandon(Child->Data); Child->Release(Child->Data); }
    return Ready;
}
void __xredisClientChildDone(xredisexclusiveowner* Owner)
{
    xredisexclusiveowner* Parent = Owner->Parent;
    if (Parent == NULL) return;
    Parent->Retain(Parent->Data);
    xrtAtomic32Store(&Parent->Nested, 0, XMEMORY_RELEASE);
    Parent->Wake(Parent->Data); Parent->Release(Parent->Data);
}
#endif

bool __xredisClientIsWorker(xredisclient* Client)
{
    xrtMutexLock(&Client->Gate);
    bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL;
    xrtMutexUnlock(&Client->Gate); return Worker;
}
#endif

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static xredisadmission xredisClientTransactionAdmit(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future, xredisexclusiveowner* Owner)
{
	xredistransactionconfig Options;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future)) ||
		(Config != NULL && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid transaction arguments/output"); return XREDIS_ADMISSION_ERROR;
	}
	if (Config == NULL) xredisClientTransactionConfigInit(&Options); else Options = *Config;
	if (Options.Size != sizeof(Options) || Options.MaxCommands == 0 || Options.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX ||
		Count > Options.MaxCommands || (Count != 0 && Commands == NULL) || !xrtMemRangeValid(Commands, Count * sizeof(*Commands)) ||
		xrtMemRangesOverlap(Commands, Count * sizeof(*Commands), Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid or excessive transaction descriptors"); return XREDIS_ADMISSION_ERROR;
	}
	if (!Client->Managed) {
		xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "transactions require a managed connection"); return XREDIS_ADMISSION_ERROR;
	}
	size_t CommandsBytes = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		const xredistransactioncommand* Command = &Commands[Index]; size_t Bytes = 0;
		if (!xredisCommandSize(Command->Arguments, Command->Count, &Bytes)) return XREDIS_ADMISSION_ERROR;
		if (xrtMemRangesOverlap(Command->Arguments, Command->Count * sizeof(*Command->Arguments), Future, sizeof(*Future))) {
			xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps arguments"); return XREDIS_ADMISSION_ERROR;
		}
		for (size_t Argument = 0; Argument < Command->Count; ++Argument)
			if (xrtMemRangesOverlap(Command->Arguments[Argument].Data, Command->Arguments[Argument].Size, Future, sizeof(*Future))) {
				xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "transaction output overlaps payload"); return XREDIS_ADMISSION_ERROR;
			}
		if (!xredisClientCommandMode(Command->Arguments, Command->Count) || xredisClientManagedControl(Command->Arguments, Command->Count) ||
			xredisClientWord(Command->Arguments[0], "QUIT") || xredisClientWord(Command->Arguments[0], "READONLY") ||
			xredisClientWord(Command->Arguments[0], "READWRITE") || xredisClientWord(Command->Arguments[0], "ASKING") ||
			(Command->Count > 1 && xredisClientWord(Command->Arguments[0], "CLIENT") &&
			 (xredisClientWord(Command->Arguments[1], "TRACKING") || xredisClientWord(Command->Arguments[1], "CACHING")))) {
			xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "transaction payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
		}
		if (Bytes > SIZE_MAX - CommandsBytes) {
			xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction wire size overflow"); return XREDIS_ADMISSION_ERROR;
		}
		CommandsBytes += Bytes;
	}
	size_t ControlBytes = sizeof(xredisTransactionMulti) + sizeof(xredisTransactionExec) + sizeof(xredisTransactionDiscard) - 3u;
	if (CommandsBytes > SIZE_MAX - ControlBytes - sizeof(xredisrequest) - sizeof(xredistransactionstate) - _Alignof(xredistransactionstate)) {
		xredisClientReport(XERR_RANGE, XREDIS_CLIENT_ARGUMENT, "transaction allocation size overflow"); return XREDIS_ADMISSION_ERROR;
	}
	size_t Bytes = CommandsBytes + ControlBytes;
	size_t Padding = (_Alignof(xredistransactionstate) - (offsetof(xredisrequest, Wire) + Bytes) % _Alignof(xredistransactionstate)) % _Alignof(xredistransactionstate);
	size_t MetadataBytes = Padding + sizeof(xredistransactionstate), Charged = Bytes + MetadataBytes;
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = xredisClientOwnerAdmission(Client, Owner, false);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
	Request->Transaction = true;
	xredistransactionstate* State = xredisTransactionState(Request);
	*State = (xredistransactionstate){0}; State->Count = Count;
	State->CommandsBytes = CommandsBytes; State->MetadataBytes = MetadataBytes;
	Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(xredistransactionresult));
	xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token);
	}
	bool Encoded = Request->Watch != NULL; size_t Offset = sizeof(xredisTransactionMulti) - 1u;
	if (Encoded) memcpy(Request->Wire, xredisTransactionMulti, Offset);
	for (size_t Index = 0; Encoded && Index < Count; ++Index) {
		size_t Written = 0;
		Encoded = xredisCommandWrite(Request->Wire + Offset, CommandsBytes - (Offset - (sizeof(xredisTransactionMulti) - 1u)),
			Commands[Index].Arguments, Commands[Index].Count, &Written);
		Offset += Written;
	}
	if (Encoded) {
		memcpy(Request->Wire + Offset, xredisTransactionExec, sizeof(xredisTransactionExec) - 1u); Offset += sizeof(xredisTransactionExec) - 1u;
		memcpy(Request->Wire + Offset, xredisTransactionDiscard, sizeof(xredisTransactionDiscard) - 1u);
		xrtMutexLock(&Client->Gate);
        Admission = xredisClientOwnerAdmission(Client, Owner, false);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
        if (Admission == XREDIS_ADMISSION_ACCEPTED && Owner != NULL &&
            !xredisClientOwnerSwap(Owner, XREDIS_OWNER_ACTIVE, XREDIS_OWNER_SEALED)) Admission = XREDIS_ADMISSION_CLOSED;
#endif
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
			if (Client->SubmitTail != NULL) Client->SubmitTail->SubmitNext = Request; else Client->SubmitHead = Request;
			Client->SubmitTail = Request;
			if (xredisClientWakeLocked(Client)) {
				++Client->Info.Accepted; *Future = Observer; xrtMutexUnlock(&Client->Gate); return XREDIS_ADMISSION_ACCEPTED;
			}
			xredisrequest* Previous = NULL;
			for (xredisrequest* Item = Client->SubmitHead; Item != Request; Item = Item->SubmitNext) Previous = Item;
			if (Previous != NULL) Previous->SubmitNext = NULL; else Client->SubmitHead = NULL;
			Client->SubmitTail = Previous;
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
            if (Owner != NULL) (void)xredisClientOwnerSwap(Owner, XREDIS_OWNER_SEALED, XREDIS_OWNER_ACTIVE);
#endif
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

xredisadmission __xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, NULL); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedTransaction(xredisclient* Client, xredisexclusiveowner* Owner,
    const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
    int64 Timeout, double Entered, xfuture** Future)
{ return xredisClientTransactionAdmit(Client, Commands, Count, Config, Timeout, Entered, Future, Owner); }
#endif

xredisadmission __xredisClientTransaction(xredisclient* Client,
	const xredistransactioncommand* Commands, size_t Count, const xredistransactionconfig* Config,
	int64 Timeout, double Entered, xfuture** Future)
{
	if (Client == NULL) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "client is NULL"); return XREDIS_ADMISSION_ERROR;
	}
	xrtMutexLock(&Client->Gate); bool Worker = !Client->CloseDone && xrtNetEngineCurrent(Client->Engine) != NULL; xrtMutexUnlock(&Client->Gate);
	if (Worker) { xredisClientReport(XERR_STATE, XREDIS_CLIENT_STATE, "sync transaction cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
	xredisadmission Admission = __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, Entered, Future);
	if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
	return Admission;
}
#endif

static xredisadmission xredisClientCallAdmit(xredisclient* Client,
	const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future,
    double Entered, xredisexclusiveowner* Owner, bool Control XREDIS_OBSERVE_PARAMETER)
{
	size_t Bytes = 0;
	if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
		xrtMemRangesOverlap(Client, Client == NULL ? 0 : Client->StorageBytes, Future, sizeof(*Future))) {
		xredisClientReport(XERR_ARGUMENT, XREDIS_CLIENT_ARGUMENT, "invalid client call arguments"); return XREDIS_ADMISSION_ERROR;
	}
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && !xredisTrackingOutput(Client,Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"tracking output overlaps owned storage");return XREDIS_ADMISSION_ERROR;
    }
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && !Control && (xredisClientWord(Arguments[0], "QUIT") ||
        xredisClientWord(Arguments[0], "READONLY") || xredisClientWord(Arguments[0], "READWRITE") ||
        xredisClientWord(Arguments[0], "ASKING") ||
        (Count > 1 && xredisClientWord(Arguments[0], "CLIENT") &&
         (xredisClientWord(Arguments[1], "TRACKING") || xredisClientWord(Arguments[1], "CACHING"))))) {
        xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "exclusive payload changes connection/control mode"); return XREDIS_ADMISSION_ERROR;
    }
#endif
	if (!Control && Client->Managed && xredisClientManagedControl(Arguments, Count)) {
		xredisClientReport(XERR_UNSUPPORTED, XREDIS_CLIENT_COMMAND_MODE, "managed initialization/transaction controls require their dedicated API");
		return XREDIS_ADMISSION_ERROR;
	}
	size_t Charged=Bytes;
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    size_t PrefixBytes=0,MetadataBytes=0,QueryBytes=Bytes;
    if(Observe>=3){
        if(xredisClientWord(Arguments[0],"CLIENT") || xredisClientWord(Arguments[0],"QUIT") ||
           xredisClientWord(Arguments[0],"READONLY") || xredisClientWord(Arguments[0],"READWRITE") ||
           xredisClientWord(Arguments[0],"ASKING")){
            xredisClientReport(XERR_UNSUPPORTED,XREDIS_CLIENT_COMMAND_MODE,"caching query changes connection/control mode");return XREDIS_ADMISSION_ERROR;
        }
        PrefixBytes=Observe==3?sizeof(xredisCachingYes)-1:sizeof(xredisCachingNo)-1;
        if(Bytes>SIZE_MAX-sizeof(xredisrequest)-sizeof(xrediscachingstate)-_Alignof(xrediscachingstate)-PrefixBytes){
            xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"caching group size overflow");return XREDIS_ADMISSION_ERROR;
        }
        Bytes+=PrefixBytes;
        size_t A=_Alignof(xrediscachingstate),Padding=(A-(offsetof(xredisrequest,Wire)+Bytes)%A)%A;
        MetadataBytes=Padding+sizeof(xrediscachingstate);Charged=Bytes+MetadataBytes;
    }
#endif
	xrtMutexLock(&Client->Gate);
	xredisadmission Admission = xredisClientOwnerAdmission(Client, Owner, Control);
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    if(Observe && Admission==XREDIS_ADMISSION_ACCEPTED && !xrtAtomicPtrLoad(&Client->Observation,XMEMORY_ACQUIRE)){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");return XREDIS_ADMISSION_ERROR;
    }
    if(Observe>=3 && Admission==XREDIS_ADMISSION_ACCEPTED && !Client->Managed){
        xrtMutexUnlock(&Client->Gate);xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"caching groups require a managed client");return XREDIS_ADMISSION_ERROR;
    }
#endif
	if (Admission == XREDIS_ADMISSION_ACCEPTED) Admission = Client->Info.Requests >= Client->Config.MaxRequests || Charged > Client->Config.MaxCommandBytes ||
		Charged > Client->Config.MaxCommandBytesTotal - Client->Info.CommandBytes ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Admission == XREDIS_ADMISSION_ACCEPTED && !xredisClientQuotaReserve(Owner, Charged)) Admission = XREDIS_ADMISSION_OVERLOADED;
#endif
	if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Client->Info.Rejected; xrtMutexUnlock(&Client->Gate); return Admission; }
	++Client->Info.Requests; Client->Info.CommandBytes += Charged; xredisClientRef(Client); xrtMutexUnlock(&Client->Gate);
	xredisrequest* Request = (xredisrequest*)xrtMalloc(sizeof(*Request) + Charged);
	if (Request == NULL) {
		xredisClientReservationDrop(Client, Charged); return XREDIS_ADMISSION_ERROR;
	}
	/* Initialize metadata only: opaque wire bytes are written by the encoder,
	 * without a redundant full-payload zero pass. */
	memset(Request, 0, offsetof(xredisrequest, Wire));
	xrtAtomic32Init(&Request->References, 1); xrtAtomic32Init(&Request->Cancelled, 0);
	Request->Client = Client; Request->WireBytes = Bytes; Request->Deadline = xredisClientDeadline(Entered, Timeout);
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
    if (Owner != NULL && (!Control || xredisClientWord(Arguments[0], "WATCH")) && Request->Deadline > Owner->Deadline) Request->Deadline = Owner->Deadline;
#endif
	Request->Sensitive = xredisClientWord(Arguments[0], "AUTH") || xredisClientWord(Arguments[0], "HELLO");
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    Request->Observe=Observe;
    if(Observe>=3){xrediscachingstate* S=xredisCachingState(Request);memset(S,0,sizeof(*S));S->MetadataBytes=MetadataBytes;}
    Request->Result=xrtMalloc(Observe>=3?sizeof(xrediscachingresult):Observe?sizeof(xredistrackingresult):sizeof(*Request->Result));
    if(Observe && Request->Result)((xredistrackingresult*)Request->Result)->Sequence=0;
    if(Observe>=3 && Request->Result)memset(Request->Result,0,sizeof(xrediscachingresult));
#else
    Request->Result = (xredisrequestresult*)xrtMalloc(sizeof(*Request->Result));
#endif
    xfuture* Observer = NULL;
	if (Request->Result != NULL) Request->Promise = xrtPromiseCreate(&Observer, NULL);
	if (Request->Promise != NULL) {
		xcancel* Token = xrtFutureCancelToken(Observer);
		if (Token != NULL) Request->Watch = xrtCancelWatch(Token, xredisClientCancel, Request);
		xrtCancelDestroy(Token); /* FutureCancelToken returns a retained reference. */
	}
    bool Encoded=false;
    if(Request->Watch!=NULL){
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
        if(PrefixBytes){
            memcpy(Request->Wire,Observe==3?xredisCachingYes:xredisCachingNo,PrefixBytes);
            Encoded=xredisCommandWrite(Request->Wire+PrefixBytes,QueryBytes,Arguments,Count,NULL);
        }else
#endif
        Encoded=xredisCommandWrite(Request->Wire,Bytes,Arguments,Count,NULL);
    }
	if (Encoded) {
		xrtMutexLock(&Client->Gate);
        Admission = xredisClientOwnerAdmission(Client, Owner, Control);
		if (Admission == XREDIS_ADMISSION_ACCEPTED) {
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
		} else ++Client->Info.Rejected;
		xrtMutexUnlock(&Client->Gate);
	}
	xrtCancelUnwatch(Request->Watch); xrtPromiseDestroy(Request->Promise); xrtFutureDestroy(Observer);
	xrtFree(Request->Result); xredisRequestRelease(Request);
	return Admission == XREDIS_ADMISSION_ACCEPTED ? XREDIS_ADMISSION_ERROR : Admission;
}

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
static bool xredisTrackingOutput(xredisclient* C,const void* Output,size_t Bytes)
{
    if(!C || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(C,C->StorageBytes,Output,Bytes))return false;
    xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    return !S || !xrtMemRangesOverlap(S,S->Bytes,Output,Bytes);
}
XREDIS_API xredisadmission xredisClientTrackingObserve(xredisclient* C,size_t MaxBytes)
{
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    if(!MaxBytes)MaxBytes=XREDIS_TRACKING_OBSERVATION_BYTES_DEFAULT;
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    size_t Bytes=sizeof(xredisobservation);
    xredisadmission A=C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
    if(A==XREDIS_ADMISSION_ACCEPTED && (C->Config.MaxPushes>(SIZE_MAX-Bytes)/sizeof(uint64) ||
        (Bytes+=C->Config.MaxPushes*sizeof(uint64))>MaxBytes || C->ObservationPreparing))A=XREDIS_ADMISSION_OVERLOADED;
    if(A!=XREDIS_ADMISSION_ACCEPTED || S){
        if(A==XREDIS_ADMISSION_OVERLOADED && C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;
        xrtMutexUnlock(&C->Gate);return A;
    }
    C->ObservationPreparing=true;xrtMutexUnlock(&C->Gate);
    S=xrtCalloc(1,Bytes);
    if(S){S->Bytes=Bytes;xrtAtomic64Init(&S->Sequence,0);}
    xrtMutexLock(&C->Gate);
    A=!S?XREDIS_ADMISSION_ERROR:C->Info.State>=XREDIS_CLIENT_CLOSING?XREDIS_ADMISSION_CLOSED:XREDIS_ADMISSION_ACCEPTED;
    if(A==XREDIS_ADMISSION_ACCEPTED)xrtAtomicPtrStore(&C->Observation,S,XMEMORY_RELEASE);
    else {xrtFree(S);if(C->ObservationRejected!=UINT64_MAX)++C->ObservationRejected;}
    /* Refund unpublished storage before physical close can observe quiescence. */
    C->ObservationPreparing=false;
    if(C->Info.State>=XREDIS_CLIENT_CLOSING)(void)xredisClientWakeLocked(C);
    xrtMutexUnlock(&C->Gate);return A;
}
XREDIS_API bool xredisClientTrackingInfo(xredisclient* C,xredistrackinginfo* Info)
{
    if(!xredisTrackingOutput(C,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking info output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    *Info=(xredistrackinginfo){sizeof(*Info),S?S->Bytes:0,C->ObservationPreparing?sizeof(xredisobservation)+C->Config.MaxPushes*sizeof(uint64):0,S?xrtAtomic64Load(&S->Sequence,XMEMORY_ACQUIRE):0,C->ObservationRejected,S!=NULL,C->ObservationPreparing};
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingCallAsync(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,true);}
XREDIS_API xredisadmission xredisClientCachingCallAsync(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{return xredisClientCallAdmit(C,Args,Count,Timeout,Future,xrtTimer(),NULL,false,Caching?3:4);}
XREDIS_API xredisadmission xredisClientCachingCall(xredisclient* C,bool Caching,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid caching client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync caching call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,Caching?3:4);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API xredisadmission xredisClientTrackingCall(xredisclient* C,const xbytesview* Args,size_t Count,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!C){xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking client");return XREDIS_ADMISSION_ERROR;}
    xrtMutexLock(&C->Gate);bool Worker=!C->CloseDone && xrtNetEngineCurrent(C->Engine)!=NULL;xrtMutexUnlock(&C->Gate);
    if(Worker){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"sync tracking call cannot block an engine worker");return XREDIS_ADMISSION_ERROR;}
    xredisadmission A=xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,true);
    if(A==XREDIS_ADMISSION_ACCEPTED)(void)xrtFutureWait(*Future);
    return A;
}
XREDIS_API bool xredisClientTrackingTake(xredisclient* C,xredistrackingpush* Push)
{
    if(!xredisTrackingOutput(C,Push,sizeof(*Push)))return xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking push output");
    xrtMutexLock(&C->Gate);xredisobservation* S=xrtAtomicPtrLoad(&C->Observation,XMEMORY_ACQUIRE);
    if(!S){xrtMutexUnlock(&C->Gate);return xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"tracking observation is not enabled");}
    if(!C->Info.Pushes){xrtMutexUnlock(&C->Gate);return false;}
    size_t Index=C->PushHead;xredisreply* Value=C->PushQueue[Index];C->PushQueue[Index]=NULL;
    *Push=(xredistrackingpush){Value,S->Pushes[Index]};S->Pushes[Index]=0;
    C->PushHead=(Index+1)%C->Config.MaxPushes;--C->Info.Pushes;
    xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(Value,&I);C->Info.PushBytes-=I.ReservedBytes;
    xrtMutexUnlock(&C->Gate);return true;
}
XREDIS_API xredisadmission xredisClientTrackingConfigureAsync(xredisclient* C,const xredistrackingconfig* K,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!K || !xrtMemRangeValid(K,sizeof(*K)) || K->Size!=sizeof(*K) || K->Flags&~31u || K->Redirect<0 ||
        K->PrefixCount>XREDIS_TRACKING_PREFIXES_MAX || !xrtMemRangeValid(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes)) ||
        ((K->Flags&XREDIS_TRACKING_OPTIN) && (K->Flags&XREDIS_TRACKING_OPTOUT)) ||
        ((K->Flags&XREDIS_TRACKING_BCAST) && (K->Flags&(XREDIS_TRACKING_OPTIN|XREDIS_TRACKING_OPTOUT))) ||
        (K->PrefixCount && !(K->Flags&XREDIS_TRACKING_BCAST)) || (K->Redirect && !(K->Flags&XREDIS_TRACKING_REDIRECT)) ||
        (!K->Enabled && (K->Flags || K->Redirect || K->PrefixCount)) || !Future || !xrtMemRangeValid(Future,sizeof(*Future)) ||
        xrtMemRangesOverlap(K,sizeof(*K),Future,sizeof(*Future)) || xrtMemRangesOverlap(K->Prefixes,K->PrefixCount*sizeof(*K->Prefixes),Future,sizeof(*Future))){
        xredisClientReport(XERR_ARGUMENT,XREDIS_CLIENT_ARGUMENT,"invalid tracking configuration");return XREDIS_ADMISSION_ERROR;
    }
    if(C && K->Enabled && (!(K->Flags&XREDIS_TRACKING_REDIRECT) || !K->Redirect)){
        xrtMutexLock(&C->Gate);
        bool Unknown=C->Info.State<XREDIS_CLIENT_CLOSING && !(C->Managed &&
            (C->InitMode==XREDIS_INIT_RESP3 || (C->Info.Ready && C->Info.Protocol==XREDIS_RESP3)));
        xrtMutexUnlock(&C->Gate);
        if(Unknown){xredisClientReport(XERR_STATE,XREDIS_CLIENT_STATE,"direct invalidations require managed RESP3 selection");return XREDIS_ADMISSION_ERROR;}
    }
    xbytesview Args[3+2+2*XREDIS_TRACKING_PREFIXES_MAX+4];size_t Count=0;
    Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("CLIENT");Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("TRACKING");
    Args[Count++]=K->Enabled?(xbytesview)XRT_BYTES_LITERAL("ON"):(xbytesview)XRT_BYTES_LITERAL("OFF");
    unsigned char Number[20];size_t Start=sizeof(Number);uint64 Id=(uint64)K->Redirect;
    if(K->Flags&XREDIS_TRACKING_REDIRECT){do {Number[--Start]=(unsigned char)('0'+Id%10u);Id/=10u;}while(Id);
        Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("REDIRECT");Args[Count++]=(xbytesview){Number+Start,sizeof(Number)-Start};}
    for(size_t I=0;I<K->PrefixCount;++I){Args[Count++]=(xbytesview)XRT_BYTES_LITERAL("PREFIX");Args[Count++]=K->Prefixes[I];}
    const xbytesview Flags[]={XRT_BYTES_LITERAL("BCAST"),XRT_BYTES_LITERAL("OPTIN"),XRT_BYTES_LITERAL("OPTOUT"),XRT_BYTES_LITERAL("NOLOOP")};
    for(unsigned I=0;I<4;++I)if(K->Flags&(1u<<I))Args[Count++]=Flags[I];
    return xredisClientCallAdmit(C,Args,Count,Timeout,Future,Entered,NULL,false,2);
}
#endif

XREDIS_API xredisadmission xredisClientCallAsync(xredisclient* Client,
    const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, xrtTimer(), NULL, false XREDIS_OBSERVE_ORDINARY); }
#if defined(XREDIS_FEATURE_XREDIS_WATCH) || defined(XREDIS_FEATURE_XREDIS_POOL)
xredisadmission __xredisClientOwnedCall(xredisclient* Client, xredisexclusiveowner* Owner,
    const xbytesview* Arguments, size_t Count, int64 Timeout, double Entered, bool Control, xfuture** Future)
{ return xredisClientCallAdmit(Client, Arguments, Count, Timeout, Future, Entered, Owner, Control XREDIS_OBSERVE_ORDINARY); }
#endif

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

#undef XREDIS_OBSERVE_PARAMETER
#undef XREDIS_OBSERVE_ORDINARY
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool/pool.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL)
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_POOL)
typedef struct xredispoolslot xredispoolslot;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
typedef struct xredispoolconnectstate xredispoolconnectstate;
#endif
enum { XREDIS_L_WAITING, XREDIS_L_GRANTED, XREDIS_L_DONE };
struct xredislease {
    xatomic32 References, Cancelled, Reason;
    xmutex Gate;
    xredispool* Pool;
    xredispoolslot* Slot;
    xredislease *Next, *Previous, *EventNext;
    xpromise *Promise, *ClosePromise;
    xfuture *Future, *CloseFuture;
    xredisleaseresult* Result;
    xredisleaseclosedresult* CloseResult;
    xcancelwatch* Cancel;
    double Deadline, HoldDeadline, ReturnBy, TimerDeadline;
    int64 ReturnTimeout;
    uint64 Timer;
    size_t APIs;
    unsigned Phase;
    bool EventQueued, Returning, Abort;
};
struct xredispoolslot {
    xredispool* Pool;
    xredisclient* Client;
    xredislease* Lease;
    xredisexclusiveowner Owner;
    xfuture* Closed;
    xfuturewatch Watch;
    xredispoolslot *NextIdle, *PreviousIdle, *EventNext;
    bool Claimed, Registered, Idle, EventQueued, Dead, Retired;
};
struct xredispool {
    xatomic32 References;
    xmutex Gate;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xredispoolconfig Config;
    xredispoolinfo Info;
    xredisownerquota Quota;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xredislease *WaitHead, *WaitTail, *LeaseEvents, *LeaseEventTail;
    xredispoolslot *IdleHead, *IdleTail, *SlotEvents, *SlotEventTail;
    size_t Count, StorageBytes, InitializeNext, CloseNext, Retired;
    bool Initialized, Queued, Finished;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xredispoolconnectstate* Connector;
#endif
    xredispoolslot Slots[];
};

static void xredisPoolWork(xnetworker* Worker, ptr Data);
static void xredisPoolLeaseNotify(xredislease* Lease);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot);
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status);
static void xredisPoolConnectDrive(xredispool* Pool);
static bool xredisPoolConnectMore(xredispool* Pool);
static void xredisPoolConnectSchedule(xredispool* Pool);
static bool xredisPoolConnectDispose(xredispool* Pool);
#endif
static bool xredisPoolCanWait(xredispool* Pool)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) return true;
#endif
    return Pool->Info.Live != 0;
}
static void xredisPoolRef(xredispool* Pool)
{ xrtAtomic32FetchAdd(&Pool->References, 1, XMEMORY_RELAXED); }
static bool xredisPoolReport(xerrkind Kind, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Message); return false; }
static double xredisPoolDeadline(double Entered, int64 Timeout)
{ return Timeout == XRT_WAIT_FOREVER ? INFINITY : Timeout == 0 ? Entered : nextafter(Entered + (double)Timeout * 0.001, INFINITY); }
static void xredisLeaseRef(xredislease* Lease)
{ xrtAtomic32FetchAdd(&Lease->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisPoolRetain(xredispool* Pool)
{ if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); xredisPoolRef(Pool); return true; }
XREDIS_API void xredisPoolRelease(xredispool* Pool)
{
    if (!Pool || xrtAtomic32FetchSub(&Pool->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtPromiseDestroy(Pool->ClosePromise); xrtFutureDestroy(Pool->CloseFuture);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    xrtFree(Pool->Connector);
#endif
    xrtMutexUnit(&Pool->Gate); xrtFree(Pool);
}
XREDIS_API bool xredisLeaseRetain(xredislease* Lease)
{ if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); xredisLeaseRef(Lease); return true; }
XREDIS_API void xredisLeaseRelease(xredislease* Lease)
{
    if (!Lease || xrtAtomic32FetchSub(&Lease->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Lease->Cancel);
    xrtPromiseDestroy(Lease->Promise); xrtFutureDestroy(Lease->Future);
    xrtPromiseDestroy(Lease->ClosePromise); xrtFutureDestroy(Lease->CloseFuture);
    xrtFree(Lease->Result); xrtFree(Lease->CloseResult);
    xrtMutexUnit(&Lease->Gate); xrtFree(Lease);
}

/* Pool Gate held. The runtime Pin and one reference per post protect Worker. */
static bool xredisPoolWakeLocked(xredispool* Pool)
{
    if (!Pool->Initialized || Pool->Finished || Pool->Queued) return true;
    Pool->Queued = true; xredisPoolRef(Pool);
    if (xrtNetPost(Pool->Worker, &Pool->Work, xredisPoolWork, Pool)) return true;
    Pool->Queued = false; xredisPoolRelease(Pool); return false;
}
static void xredisPoolSlotRef(ptr Data) { xredisPoolRef(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotDrop(ptr Data) { xredisPoolRelease(((xredispoolslot*)Data)->Pool); }
static void xredisPoolSlotNotify(ptr Data)
{
    xredispoolslot* Slot = Data; xredispool* Pool = Slot->Pool;
    xrtMutexLock(&Pool->Gate);
    if (Pool->Initialized && !Pool->Finished && !Slot->EventQueued) {
        Slot->EventQueued = true; Slot->EventNext = NULL;
        if (Pool->SlotEventTail) Pool->SlotEventTail->EventNext = Slot; else Pool->SlotEvents = Slot;
        Pool->SlotEventTail = Slot; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolQueueLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Pool->Gate);
    if (!Pool->Finished && !Lease->EventQueued) {
        Lease->EventQueued = true; Lease->EventNext = NULL; xredisLeaseRef(Lease);
        if (Pool->LeaseEventTail) Pool->LeaseEventTail->EventNext = Lease; else Pool->LeaseEvents = Lease;
        Pool->LeaseEventTail = Lease; xredisPoolWakeLocked(Pool);
    }
    xrtMutexUnlock(&Pool->Gate);
}
static void xredisPoolLeaseNotify(xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate); xredispool* Pool = Lease->Pool;
    if (Pool) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Pool) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static void xredisLeaseReason(xredislease* Lease, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    xrtAtomic32CompareExchange(&Lease->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisLeaseCancel(ptr Data)
{ xredislease* Lease = Data; xrtAtomic32Store(&Lease->Cancelled, 1, XMEMORY_RELEASE); xredisPoolLeaseNotify(Lease); }
static void xredisLeaseTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredislease* Lease = Data;
    if (Lease->Timer == Id) Lease->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    else if (Result != XNET_RESULT_CANCELLED) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    xredisPoolLeaseNotify(Lease); xredisLeaseRelease(Lease);
}
/* Owning pool worker only; cancellation callbacks only queue events. */
static bool xredisLeaseArm(xredispool* Pool, xredislease* Lease, double Deadline)
{
    if (Lease->Timer && Lease->TimerDeadline == Deadline) return true;
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    if (Lease->Timer) return false;
    if (Deadline == INFINITY) return true;
    Lease->TimerDeadline = Deadline; xredisLeaseRef(Lease);
    Lease->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisLeaseTimer, Lease);
    if (Lease->Timer) return true;
    xredisLeaseRelease(Lease); xredisLeaseReason(Lease, XREDIS_REQUEST_MEMORY); return false;
}
static bool xredisLeaseDisarm(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, Lease->Timer);
    return Lease->Timer == 0;
}

static void xredisPoolIdleRemove(xredispool* Pool, xredispoolslot* Slot)
{
    /* Owning worker, with Pool Gate held for public counters. */
    if (!Slot->Idle) return;
    if (Slot->PreviousIdle) Slot->PreviousIdle->NextIdle = Slot->NextIdle; else Pool->IdleHead = Slot->NextIdle;
    if (Slot->NextIdle) Slot->NextIdle->PreviousIdle = Slot->PreviousIdle; else Pool->IdleTail = Slot->PreviousIdle;
    Slot->NextIdle = Slot->PreviousIdle = NULL; Slot->Idle = false; --Pool->Info.Idle;
}
static void xredisPoolIdleAppend(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Idle || Slot->Dead || Slot->Lease || Pool->Info.State != XREDIS_POOL_OPEN) return;
    Slot->Idle = true; Slot->PreviousIdle = Pool->IdleTail; Slot->NextIdle = NULL;
    if (Pool->IdleTail) Pool->IdleTail->NextIdle = Slot; else Pool->IdleHead = Slot;
    Pool->IdleTail = Slot; ++Pool->Info.Idle;
}
static void xredisPoolWaitRemove(xredispool* Pool, xredislease* Lease)
{
    if (Lease->Previous) Lease->Previous->Next = Lease->Next; else Pool->WaitHead = Lease->Next;
    if (Lease->Next) Lease->Next->Previous = Lease->Previous; else Pool->WaitTail = Lease->Previous;
    Lease->Next = Lease->Previous = NULL; --Pool->Info.Pending;
}
static void xredisPoolAcquireResultDrop(ptr Value, ptr Data)
{ (void)Data; xredisleaseresult* Result = Value; xredisLeaseRelease(Result->Lease); xrtFree(Result); }
static void xredisPoolClosedResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }
static void xredisPoolPublishAcquire(xredislease* Lease, xredisrequeststatus Status)
{
    xpromise* Promise = Lease->Promise; Lease->Promise = NULL;
    xfuture* Future = Lease->Future; Lease->Future = NULL;
    xredisleaseresult* Result = Lease->Result; Lease->Result = NULL;
    *Result = (xredisleaseresult){Status, NULL};
    if (Status == XREDIS_REQUEST_OK) { xredisLeaseRef(Lease); Result->Lease = Lease; }
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolAcquireResultDrop, NULL)) xredisPoolAcquireResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}
static void xredisPoolPublishReturn(xredislease* Lease, xredisrequeststatus Status, bool Reusable)
{
    xpromise* Promise = Lease->ClosePromise; Lease->ClosePromise = NULL;
    xredisleaseclosedresult* Result = Lease->CloseResult; Lease->CloseResult = NULL;
    *Result = (xredisleaseclosedresult){Status, Reusable};
    if (!xrtPromiseResolveOwned(Promise, Result, xredisPoolClosedResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
}
static void xredisPoolFailAcquire(xredispool* Pool, xredislease* Lease, xredisrequeststatus Status)
{
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Pool->Gate); xredisPoolWaitRemove(Pool, Lease); xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; xrtMutexUnlock(&Lease->Gate);
    xredisPoolRelease(Pool); /* Acquisition's runtime reference. */
    xredisPoolPublishAcquire(Lease, Status); xredisPoolPublishReturn(Lease, Status, false); xredisLeaseRelease(Lease);
}

static void xredisPoolProcessLease(xredispool* Pool, xredislease* Lease)
{
    xrtMutexLock(&Lease->Gate);
    unsigned Phase = Lease->Phase; xredispoolslot* Slot = Lease->Slot;
    bool Returning = Lease->Returning, Abort = Lease->Abort; size_t APIs = Lease->APIs;
    double ReturnBy = Lease->ReturnBy;
    xrtMutexUnlock(&Lease->Gate);
    if (Phase == XREDIS_L_DONE) return;
    xrtMutexLock(&Pool->Gate); bool Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool Live = xredisPoolCanWait(Pool), Immediate = Pool->WaitHead == Lease && Pool->IdleHead != NULL;
    xrtMutexUnlock(&Pool->Gate);
    if (Closing) xredisLeaseReason(Lease, XREDIS_REQUEST_CLOSED);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Phase == XREDIS_L_WAITING) {
        if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
        if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
        if (!Live) xredisLeaseReason(Lease, XREDIS_REQUEST_DISCONNECTED);
        Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
        if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
        if (!Immediate && !xredisLeaseArm(Pool, Lease, Lease->Deadline) && !Lease->Timer) xredisPoolLeaseNotify(Lease);
        return;
    }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) xredisLeaseReason(Lease, Info.CloseStatus);
    if (xrtTimer() >= Lease->HoldDeadline || (Returning && xrtTimer() >= ReturnBy)) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (!Returning && !Reason) return;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    if (Abort || Closing || (Reason && Info.Requests)) xredisClientClose(Slot->Client);
    if (APIs) return; /* Leave always queues another event. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (!__xredisClientOwnerReady(Slot->Client, &Slot->Owner, true)) {
        if (Reason || Abort || Closing) xredisClientClose(Slot->Client);
        else {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) xredisClientClose(Slot->Client);
        }
        return;
    }
#endif
    xredisClientInfo(Slot->Client, &Info);
    if (Info.Requests) {
        if (!Reason && !Abort && !Closing) {
            double Deadline = ReturnBy < Lease->HoldDeadline ? ReturnBy : Lease->HoldDeadline;
            if (!xredisLeaseArm(Pool, Lease, Deadline) && !Lease->Timer) { xredisClientClose(Slot->Client); xredisPoolLeaseNotify(Lease); }
        }
        return;
    }
    bool Reusable = Info.State == XREDIS_CLIENT_OPEN && !Abort && !Closing;
    if (!Reusable && xrtFutureState(Slot->Closed) == XFUTURE_PENDING) { xredisClientClose(Slot->Client); return; }
    if (!xredisLeaseDisarm(Pool, Lease)) return;
    xrtMutexLock(&Lease->Gate);
    if (Lease->APIs) { xrtMutexUnlock(&Lease->Gate); return; }
    if (Lease->Abort && Reusable) { xrtMutexUnlock(&Lease->Gate); xredisClientClose(Slot->Client); return; }
    Lease->Phase = XREDIS_L_DONE; Lease->Pool = NULL; Lease->Slot = NULL;
    xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = NULL;
    xrtMutexLock(&Pool->Gate); --Pool->Info.Leased; ++Pool->Info.Returned;
    if (Reusable) xredisPoolIdleAppend(Pool, Slot);
    xrtMutexUnlock(&Pool->Gate);
    xredisPoolPublishReturn(Lease, Reason ? (xredisrequeststatus)(Reason - 1u) : XREDIS_REQUEST_OK, Reusable);
    xredisPoolRelease(Pool); xredisLeaseRelease(Lease);
    if (!Reusable) xredisPoolSlotNotify(Slot);
}

static bool xredisPoolDropMember(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Registered) { xrtFutureWatchRemove(Slot->Closed, &Slot->Watch); Slot->Registered = false; }
    if (Slot->Claimed) {
        if (!__xredisClientUnclaim(Slot->Client, &Slot->Owner)) return false;
        Slot->Claimed = false; xrtNetEngineUnpin(Pool->Engine);
    }
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisClientRelease(Slot->Client); Slot->Client = NULL;
    xrtFutureDestroy(Slot->Closed); Slot->Closed = NULL;
    xrtMutexLock(&Pool->Gate); ++Pool->Info.Retired; xrtMutexUnlock(&Pool->Gate); return true;
}
static void xredisPoolProcessSlot(xredispool* Pool, xredispoolslot* Slot)
{
    if (Slot->Retired) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && xredisPoolConnectProcess(Pool, Slot)) return;
#endif
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        xrtMutexLock(&Pool->Gate);
        if (!Slot->Dead) { Slot->Dead = true; --Pool->Info.Live; xredisPoolIdleRemove(Pool, Slot); }
        xrtMutexUnlock(&Pool->Gate);
    }
    if (Slot->Lease) { xredisPoolQueueLease(Pool, Slot->Lease); return; }
    if (Slot->Dead) {
        if (Info.Requests || xrtFutureState(Slot->Closed) == XFUTURE_PENDING) return;
        if (!xredisPoolDropMember(Pool, Slot)) return;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
        if (Pool->Connector) { xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus); return; }
#endif
        Slot->Retired = true;
        xrtMutexLock(&Pool->Gate); ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
    } else if (!Info.Requests) {
        xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate);
    }
}

static void xredisPoolGrant(xredispool* Pool, xredislease* Lease, xredispoolslot* Slot)
{
    /* No application can use this lease yet. Retire the acquisition cancel
     * callback before the final grant check; future cancellation afterwards
     * cannot revoke an already granted lease. */
    if (!xredisLeaseDisarm(Pool, Lease)) { xredisPoolLeaseNotify(Lease); return; }
    xrtCancelUnwatch(Lease->Cancel); Lease->Cancel = NULL;
    if (xrtAtomic32Load(&Lease->Cancelled, XMEMORY_ACQUIRE)) xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED);
    if (xrtTimer() >= Lease->Deadline) xredisLeaseReason(Lease, XREDIS_REQUEST_TIMEOUT);
    uint32 Reason = xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE);
    if (Reason) { xredisPoolFailAcquire(Pool, Lease, (xredisrequeststatus)(Reason - 1u)); return; }
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Info.State != XREDIS_CLIENT_OPEN || Info.Requests) { xredisPoolSlotNotify(Slot); return; }
    Lease->HoldDeadline = xredisPoolDeadline(xrtTimer(), Pool->Config.LeaseTimeout); Lease->ReturnTimeout = Pool->Config.ReturnTimeout;
    if (!xredisLeaseArm(Pool, Lease, Lease->HoldDeadline)) {
        xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_MEMORY); return;
    }
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolFailAcquire(Pool, Lease, XREDIS_REQUEST_CLOSED); return;
    }
    xredisPoolWaitRemove(Pool, Lease); ++Pool->Info.Leased; ++Pool->Info.Acquired;
    xrtMutexUnlock(&Pool->Gate);
    xrtMutexLock(&Lease->Gate); Lease->Phase = XREDIS_L_GRANTED; Lease->Slot = Slot; xrtMutexUnlock(&Lease->Gate);
    Slot->Lease = Lease; Slot->Owner.Deadline = Lease->HoldDeadline;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_ACTIVE, XMEMORY_RELEASE);
    xredisPoolPublishAcquire(Lease, XREDIS_REQUEST_OK);
}

static void xredisPoolFinish(xredispool* Pool)
{
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector && !xredisPoolConnectDispose(Pool)) return;
#endif
    xrtMutexLock(&Pool->Gate);
    if (Pool->Queued || Pool->Info.Pending || Pool->Info.Leased || Pool->Retired != Pool->Count || Pool->LeaseEvents || Pool->SlotEvents) {
        xrtMutexUnlock(&Pool->Gate); return;
    }
    Pool->Finished = true; Pool->Info.State = XREDIS_POOL_CLOSED;
    xnetengine* Engine = Pool->Engine; Pool->Engine = NULL; Pool->Worker = NULL;
    xrtMutexUnlock(&Pool->Gate);
    xrtNetEngineUnpin(Engine);
    xpromise* Promise = Pool->ClosePromise; Pool->ClosePromise = NULL;
    xrtPromiseResolve(Promise, NULL); xrtPromiseDestroy(Promise); xredisPoolRelease(Pool); /* Runtime. */
}
static void xredisPoolWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xredispool* Pool = Data;
    xrtMutexLock(&Pool->Gate); Pool->Queued = false; bool Closing = Pool->Info.State != XREDIS_POOL_OPEN; xrtMutexUnlock(&Pool->Gate);
    size_t Budget = Pool->Config.WorkItems;
    if (Closing) for (size_t Index = 0; Index < Budget && Pool->CloseNext < Pool->Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Pool->CloseNext++];
        if (!Slot->Retired) { if (Slot->Client) xredisClientClose(Slot->Client); xredisPoolProcessSlot(Pool, Slot); }
    }
    else for (size_t Index = 0; Index < Budget && Pool->InitializeNext < Pool->Count; ++Index)
        xredisPoolProcessSlot(Pool, &Pool->Slots[Pool->InitializeNext++]);
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredispoolslot* Slot = Pool->SlotEvents;
        if (Slot) { Pool->SlotEvents = Slot->EventNext; if (!Pool->SlotEvents) Pool->SlotEventTail = NULL; Slot->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Slot) break; xredisPoolProcessSlot(Pool, Slot);
    }
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->LeaseEvents;
        if (Lease) { Pool->LeaseEvents = Lease->EventNext; if (!Pool->LeaseEvents) Pool->LeaseEventTail = NULL; Lease->EventQueued = false; }
        xrtMutexUnlock(&Pool->Gate); if (!Lease) break; xredisPoolProcessLease(Pool, Lease); xredisLeaseRelease(Lease);
    }
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectDrive(Pool);
#endif
    for (size_t Index = 0; Index < Budget; ++Index) {
        xrtMutexLock(&Pool->Gate); xredislease* Lease = Pool->WaitHead;
        xredispoolslot* Slot = Pool->Info.State == XREDIS_POOL_OPEN ? Pool->IdleHead : NULL;
        bool NoLive = !xredisPoolCanWait(Pool);
        if (Slot && Lease) xredisPoolIdleRemove(Pool, Slot);
        xrtMutexUnlock(&Pool->Gate);
        if (!Lease) break;
        if (Closing || NoLive) { xredisLeaseReason(Lease, Closing ? XREDIS_REQUEST_CLOSED : XREDIS_REQUEST_DISCONNECTED); xredisPoolProcessLease(Pool, Lease); }
        else if (Slot) {
            xredisPoolGrant(Pool, Lease, Slot);
            if (!Slot->Lease && !Slot->Dead) { xrtMutexLock(&Pool->Gate); xredisPoolIdleAppend(Pool, Slot); xrtMutexUnlock(&Pool->Gate); }
        } else {
            if (!Lease->Timer && !xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) xredisPoolLeaseNotify(Lease);
            break;
        }
    }
    xrtMutexLock(&Pool->Gate);
    Closing = Pool->Info.State != XREDIS_POOL_OPEN;
    bool More = Pool->SlotEvents || Pool->LeaseEvents || (Pool->WaitHead && (Pool->IdleHead || Closing || !xredisPoolCanWait(Pool))) ||
        (Closing ? Pool->CloseNext < Pool->Count : Pool->InitializeNext < Pool->Count);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) More = More || xredisPoolConnectMore(Pool);
#endif
    if (More) xredisPoolWakeLocked(Pool);
    xrtMutexUnlock(&Pool->Gate);
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    if (Pool->Connector) xredisPoolConnectSchedule(Pool);
#endif
    if (Closing) xredisPoolFinish(Pool);
    xredisPoolRelease(Pool);
}

XREDIS_API bool xredisPoolConfigInit(xredispoolconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool config output");
    *Config = (xredispoolconfig){sizeof(*Config), 256, 4096, 64u * 1024u * 1024u, 30000, 5000, 64}; return true;
}
static int xredisPoolCompareClient(const void* Left, const void* Right)
{
    uintptr_t A = (uintptr_t)*(xredisclient* const*)Left, B = (uintptr_t)*(xredisclient* const*)Right;
    return (A > B) - (A < B);
}
static bool xredisPoolBounds(const xredispoolconfig* Options)
{
    return Options->Size == sizeof(*Options) && Options->MaxPending && Options->MaxPending <= 65536 && Options->MaxRequests && Options->MaxCommandBytes &&
        Options->LeaseTimeout > 0 && Options->LeaseTimeout <= 86400000 && Options->ReturnTimeout > 0 && Options->ReturnTimeout <= 60000 &&
        Options->WorkItems && Options->WorkItems <= 65536;
}
static xredispool* xredisPoolAllocate(xnetengine* Engine, uint64 Affinity, size_t Count, const xredispoolconfig* Options)
{
    if (!xrtNetEnginePin(Engine)) return NULL;
    size_t Bytes = sizeof(xredispool) + Count * sizeof(xredispoolslot);
    xredispool* Pool = xrtCalloc(1, Bytes);
    if (!Pool || !xrtMutexInit(&Pool->Gate)) { xrtFree(Pool); xrtNetEngineUnpin(Engine); return NULL; }
    xrtAtomic32Init(&Pool->References, 1); Pool->Engine = Engine; Pool->Count = Count; Pool->StorageBytes = Bytes; Pool->Config = *Options;
    Pool->Worker = xrtNetEngineWorker(Engine, (uint32)(Affinity % xrtNetEngineWorkerCount(Engine)));
    Pool->Info.Size = sizeof(Pool->Info); Pool->Info.Members = Count;
    xrtAtomic64Init(&Pool->Quota.Requests, 0); xrtAtomic64Init(&Pool->Quota.Bytes, 0); xrtAtomic32Init(&Pool->Quota.Closing, 0);
    Pool->Quota.MaxRequests = (uint64)Options->MaxRequests; Pool->Quota.MaxBytes = (uint64)Options->MaxCommandBytes;
    if (!xrtNetPostInit(&Pool->Work) || !(Pool->ClosePromise = xrtPromiseCreate(&Pool->CloseFuture, NULL))) {
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return NULL;
    }
    return Pool;
}
static void xredisPoolSlotInit(xredispool* Pool, xredispoolslot* Slot)
{
    Slot->Pool = Pool; Slot->Owner.Data = Slot;
    Slot->Owner.Retain = xredisPoolSlotRef; Slot->Owner.Release = xredisPoolSlotDrop; Slot->Owner.Wake = xredisPoolSlotNotify;
    Slot->Owner.Quota = &Pool->Quota; Slot->Owner.Deadline = INFINITY; xrtAtomic32Init(&Slot->Owner.State, XREDIS_OWNER_CLOSING);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    xrtAtomic32Init(&Slot->Owner.Nested, 0);
#endif
}
XREDIS_API xredisadmission xredisPoolCreate(xnetengine* Engine, uint64 Affinity,
    xredisclient* const* Clients, size_t Count, const xredispoolconfig* Config, xredispool** Result)
{
    xredispoolconfig Options;
    if (!Engine || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) || !Count || Count > 65536 || !Clients ||
        !xrtMemRangeValid(Clients, Count * sizeof(*Clients)) || xrtMemRangesOverlap(Clients, Count * sizeof(*Clients), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool members/config/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    if (!xredisPoolBounds(&Options)) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool bounds/deadlines"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient** Sorted = xrtMalloc(Count * sizeof(*Sorted)); if (!Sorted) return XREDIS_ADMISSION_ERROR;
    memcpy(Sorted, Clients, Count * sizeof(*Sorted)); qsort(Sorted, Count, sizeof(*Sorted), xredisPoolCompareClient);
    bool Valid = Sorted[0] != NULL;
    for (size_t Index = 1; Valid && Index < Count; ++Index) Valid = Sorted[Index] != Sorted[Index - 1];
    xrtFree(Sorted);
    if (!Valid) { xredisPoolReport(XERR_ARGUMENT, "NULL or duplicate pool member"); return XREDIS_ADMISSION_ERROR; }
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options);
    if (!Pool) return XREDIS_ADMISSION_ERROR;
    Pool->Info.Live = Count;
    xredisadmission Admission = XREDIS_ADMISSION_ACCEPTED;
    xredisprotocol Protocol = XREDIS_PROTOCOL_UNKNOWN;
    for (size_t Index = 0; Admission == XREDIS_ADMISSION_ACCEPTED && Index < Count; ++Index) {
        xredispoolslot* Slot = &Pool->Slots[Index]; xredisPoolSlotInit(Pool, Slot);
        xnetengine* MemberEngine = NULL; xnetworker* MemberWorker = NULL;
        Admission = __xredisClientClaim(Clients[Index], &Slot->Owner, &MemberEngine, &MemberWorker, Result);
        (void)MemberWorker;
        if (Admission != XREDIS_ADMISSION_ACCEPTED) break;
        Slot->Claimed = true; Slot->Client = Clients[Index];
        if (MemberEngine != Engine) {
            __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); Slot->Client = NULL; Slot->Claimed = false;
            xrtNetEngineUnpin(MemberEngine); xredisPoolReport(XERR_ARGUMENT, "pool members belong to different engines"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        xredisclientinfo MemberInfo = {0}; MemberInfo.Size = sizeof(MemberInfo); xredisClientInfo(Slot->Client, &MemberInfo);
        if (Index == 0) Protocol = MemberInfo.Protocol;
        else if (Protocol != MemberInfo.Protocol) {
            xredisPoolReport(XERR_ARGUMENT, "pool members use different RESP protocols"); Admission = XREDIS_ADMISSION_ERROR; break;
        }
        Slot->Closed = xredisClientClosed(Slot->Client);
        if (!xrtFutureWatchInit(&Slot->Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) { Admission = XREDIS_ADMISSION_ERROR; break; }
        xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Slot->Closed, &Slot->Watch);
        Slot->Registered = Watched == XFUTURE_WATCH_PENDING;
        if (!Slot->Registered) xredisPoolRelease(Pool);
        if (Watched == XFUTURE_WATCH_ERROR) Admission = XREDIS_ADMISSION_ERROR;
    }
    if (Admission != XREDIS_ADMISSION_ACCEPTED) {
        for (size_t Index = 0; Index < Count; ++Index) {
            xredispoolslot* Slot = &Pool->Slots[Index];
            if (Slot->Registered) xrtFutureWatchRemove(Slot->Closed, &Slot->Watch);
            if (Slot->Claimed) { __xredisClientUnclaim(Slot->Client, &Slot->Owner); xredisClientRelease(Slot->Client); xrtNetEngineUnpin(Engine); }
            xrtFutureDestroy(Slot->Closed);
        }
        xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return Admission;
    }
    xredisPoolRef(Pool); /* Active lifetime, independent of caller. */
    xrtMutexLock(&Pool->Gate); Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}

static xredisadmission xredisPoolAcquireAdmit(xredispool* Pool, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    if (!Pool || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || Timeout < XRT_WAIT_FOREVER ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid pool acquisition/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Pool->Gate);
    if (Sync && !Pool->Finished && xrtNetEngineCurrent(Pool->Engine)) {
        xrtMutexUnlock(&Pool->Gate); xredisPoolReport(XERR_STATE, "sync acquire cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    xredisadmission Admission = Pool->Info.State != XREDIS_POOL_OPEN ? XREDIS_ADMISSION_CLOSED :
        Pool->Info.Pending >= Pool->Config.MaxPending ? XREDIS_ADMISSION_OVERLOADED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { ++Pool->Info.Rejected; xrtMutexUnlock(&Pool->Gate); return Admission; }
    ++Pool->Info.Pending; xredisPoolRef(Pool); xrtMutexUnlock(&Pool->Gate);
    xredislease* Lease = xrtCalloc(1, sizeof(*Lease));
    if (!Lease || !xrtMutexInit(&Lease->Gate)) {
        xrtFree(Lease); xrtMutexLock(&Pool->Gate); --Pool->Info.Pending; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
        xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR;
    }
    xrtAtomic32Init(&Lease->References, 1); xrtAtomic32Init(&Lease->Cancelled, 0); xrtAtomic32Init(&Lease->Reason, 0);
    Lease->Pool = Pool; Lease->Deadline = xredisPoolDeadline(Entered, Timeout);
    Lease->Result = xrtMalloc(sizeof(*Lease->Result)); Lease->CloseResult = xrtMalloc(sizeof(*Lease->CloseResult));
    bool Ready = Lease->Result && Lease->CloseResult && (Lease->Promise = xrtPromiseCreate(&Lease->Future, NULL)) &&
        (Lease->ClosePromise = xrtPromiseCreate(&Lease->CloseFuture, NULL));
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Lease->Future); if (Token) Lease->Cancel = xrtCancelWatch(Token, xredisLeaseCancel, Lease);
        xrtCancelDestroy(Token); Ready = Lease->Cancel != NULL;
    }
    xrtMutexLock(&Pool->Gate);
    if (Ready && Pool->Info.State == XREDIS_POOL_OPEN) {
        Lease->Previous = Pool->WaitTail; if (Pool->WaitTail) Pool->WaitTail->Next = Lease; else Pool->WaitHead = Lease; Pool->WaitTail = Lease;
        xfuture* Observer = xrtFutureRef(Lease->Future); xredisLeaseRef(Lease); xredisPoolRef(Pool); /* Admission tail. */
        *Future = Observer; xrtMutexUnlock(&Pool->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisLeaseRelease(Lease); xredisPoolRelease(Pool);
        if (Sync) xrtFutureWait(Observer);
        return XREDIS_ADMISSION_ACCEPTED;
    }
    Admission = Pool->Info.State == XREDIS_POOL_OPEN ? XREDIS_ADMISSION_ERROR : XREDIS_ADMISSION_CLOSED;
    --Pool->Info.Pending; if (Admission == XREDIS_ADMISSION_CLOSED) ++Pool->Info.Rejected;
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    xredisLeaseRelease(Lease); xredisPoolRelease(Pool); return Admission;
}
XREDIS_API xredisadmission xredisPoolAcquireAsync(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisPoolAcquire(xredispool* Pool, int64 Timeout, xfuture** Future)
{ return xredisPoolAcquireAdmit(Pool, Timeout, Future, xrtTimer(), true); }
XREDIS_API bool xredisPoolInfo(xredispool* Pool, xredispoolinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool snapshot output");
    xrtMutexLock(&Pool->Gate); *Info = Pool->Info;
    Info->Requests = xrtAtomic64Load(&Pool->Quota.Requests, XMEMORY_RELAXED); Info->CommandBytes = xrtAtomic64Load(&Pool->Quota.Bytes, XMEMORY_RELAXED);
    xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API bool xredisPoolClose(xredispool* Pool)
{
    if (!Pool) return xredisPoolReport(XERR_ARGUMENT, "pool is NULL");
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State == XREDIS_POOL_OPEN) Pool->Info.State = XREDIS_POOL_CLOSING;
    xrtAtomic32Store(&Pool->Quota.Closing, 1, XMEMORY_RELEASE);
    xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); return true;
}
XREDIS_API xfuture* xredisPoolClosed(xredispool* Pool)
{ if (!Pool) { xredisPoolReport(XERR_ARGUMENT, "pool is NULL"); return NULL; } return xrtFutureRef(Pool->CloseFuture); }

static xredisadmission xredisLeaseEnter(xredislease* Lease, bool Sync, xfuture** Future,
    xredispool** ResultPool, xredispoolslot** ResultSlot)
{
    if (!Lease || !Future || !xrtMemRangeValid(Future, sizeof(*Future)) || xrtMemRangesOverlap(Lease, Lease ? sizeof(*Lease) : 0, Future, sizeof(*Future))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid leased call/output"); return XREDIS_ADMISSION_ERROR;
    }
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase != XREDIS_L_GRANTED || Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE)) {
        xrtMutexUnlock(&Lease->Gate); return XREDIS_ADMISSION_CLOSED;
    }
    xredispoolslot* Slot = Lease->Slot; xredispool* Pool = Lease->Pool; ++Lease->APIs;
    xrtMutexUnlock(&Lease->Gate);
    if (Sync && __xredisClientIsWorker(Slot->Client)) {
        xrtMutexLock(&Lease->Gate); --Lease->APIs; xredisPoolRef(Pool); xrtMutexUnlock(&Lease->Gate);
        xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool);
        xredisPoolReport(XERR_STATE, "sync leased operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    *ResultPool = Pool; *ResultSlot = Slot; return XREDIS_ADMISSION_ACCEPTED;
}
static void xredisLeaseLeave(xredislease* Lease, xredispool* Pool)
{
    xrtMutexLock(&Lease->Gate); --Lease->APIs;
    bool Wake = Lease->Returning || xrtAtomic32Load(&Lease->Reason, XMEMORY_ACQUIRE) ||
        xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE);
    if (Wake) xredisPoolRef(Pool);
    xrtMutexUnlock(&Lease->Gate);
    if (Wake) { xredisPoolQueueLease(Pool, Lease); xredisPoolRelease(Pool); }
}
static xredisadmission xredisLeaseCallAdmit(xredislease* Lease, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Slot->Client, &Slot->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisLeaseCallAsync(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseCall(xredislease* Lease, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisLeaseCallAdmit(Lease, Arguments, Count, Timeout, Future, xrtTimer(), true); }
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
xredisadmission __xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Slot->Client, &Slot->Owner, Commands, Count, Config, Timeout, Entered, Future);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, bool Sync)
{
    xredispool* Pool = NULL; xredispoolslot* Slot = NULL;
    xredisadmission Admission = xredisLeaseEnter(Lease, Sync, Future, &Pool, &Slot);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisWatchBeginOwned(Slot->Client, &Slot->Owner, Keys, Count, Config, Future, Entered);
    xredisLeaseLeave(Lease, Pool);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) xrtFutureWait(*Future);
    return Admission;
}
#endif
static bool xredisLeaseSeal(xredislease* Lease, bool Abort)
{
    if (!Lease) return xredisPoolReport(XERR_ARGUMENT, "lease is NULL");
    xrtMutexLock(&Lease->Gate);
    if (Lease->Phase == XREDIS_L_GRANTED) {
        if (!Lease->Returning) { Lease->Returning = true; Lease->ReturnBy = xredisPoolDeadline(xrtTimer(), Lease->ReturnTimeout); }
        if (Abort) { Lease->Abort = true; xredisLeaseReason(Lease, XREDIS_REQUEST_CANCELLED); }
        xrtAtomic32Store(&Lease->Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Lease->Gate); xredisPoolLeaseNotify(Lease); return true;
}
XREDIS_API bool xredisLeaseReturn(xredislease* Lease) { return xredisLeaseSeal(Lease, false); }
XREDIS_API bool xredisLeaseAbort(xredislease* Lease) { return xredisLeaseSeal(Lease, true); }
XREDIS_API xfuture* xredisLeaseClosed(xredislease* Lease)
{ if (!Lease) { xredisPoolReport(XERR_ARGUMENT, "lease is NULL"); return NULL; } return xrtFutureRef(Lease->CloseFuture); }
/* Optional cold maintenance: no transport dispatch, adapter Future or command
 * replay on hot calls. All mutable maintenance state belongs to this worker. */
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
enum { XREDIS_PC_WAIT, XREDIS_PC_CONNECTING, XREDIS_PC_DRAIN, XREDIS_PC_ONLINE, XREDIS_PC_DONE };
typedef struct xredispoolconnectentry {
    xfuture* Ready;
    xfuturewatch Watch;
    size_t Heap, Previous, Next;
    double Due;
    uint64 Attempt;
    unsigned Phase, Failures;
    bool Registered, Queued, InFlight;
} xredispoolconnectentry;
struct xredispoolconnectstate {
    xredispoolconnectconfig Config;
    xredispoolconnectinfo Info;
    xredispoolconnectentry* Entries;
    size_t* Heap;
    size_t HeapCount, Head, Tail;
    uint64 Random, Timer;
    double TimerDeadline;
    bool Dropped;
};
static void xredisPoolConnectIncrement(uint64* Value) { if (*Value != UINT64_MAX) ++*Value; }
static bool xredisPoolConnectOpen(xredispool* Pool)
{ return !xrtAtomic32Load(&Pool->Quota.Closing, XMEMORY_ACQUIRE); }
static void xredisPoolConnectHeapSwap(xredispoolconnectstate* State, size_t A, size_t B)
{
    size_t Item = State->Heap[A]; State->Heap[A] = State->Heap[B]; State->Heap[B] = Item;
    State->Entries[State->Heap[A]].Heap = A; State->Entries[State->Heap[B]].Heap = B;
}
static bool xredisPoolConnectBefore(xredispoolconnectstate* State, size_t A, size_t B)
{
    double Left = State->Entries[State->Heap[A]].Due, Right = State->Entries[State->Heap[B]].Due;
    return Left < Right || (Left == Right && State->Heap[A] < State->Heap[B]);
}
static void xredisPoolConnectHeapFix(xredispoolconnectstate* State, size_t Index)
{
    while (Index && xredisPoolConnectBefore(State, Index, (Index - 1u) / 2u)) {
        size_t Parent = (Index - 1u) / 2u; xredisPoolConnectHeapSwap(State, Index, Parent); Index = Parent;
    }
    for (;;) {
        size_t Left = Index * 2u + 1u; if (Left >= State->HeapCount) break;
        size_t Best = Left;
        if (Left + 1u < State->HeapCount && xredisPoolConnectBefore(State, Left + 1u, Left)) Best = Left + 1u;
        if (!xredisPoolConnectBefore(State, Best, Index)) break;
        xredisPoolConnectHeapSwap(State, Index, Best); Index = Best;
    }
}
static void xredisPoolConnectHeapRemove(xredispoolconnectstate* State, size_t Item)
{
    size_t Index = State->Entries[Item].Heap; if (Index == SIZE_MAX) return;
    size_t Last = --State->HeapCount; State->Entries[Item].Heap = SIZE_MAX;
    if (Index == Last) return;
    State->Heap[Index] = State->Heap[Last]; State->Entries[State->Heap[Index]].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectHeapPut(xredispoolconnectstate* State, size_t Item, double Due)
{
    xredisPoolConnectHeapRemove(State, Item); State->Entries[Item].Due = Due;
    size_t Index = State->HeapCount++; State->Heap[Index] = Item; State->Entries[Item].Heap = Index;
    xredisPoolConnectHeapFix(State, Index);
}
static void xredisPoolConnectQueueRemove(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (!Entry->Queued) return;
    if (Entry->Previous != SIZE_MAX) State->Entries[Entry->Previous].Next = Entry->Next; else State->Head = Entry->Next;
    if (Entry->Next != SIZE_MAX) State->Entries[Entry->Next].Previous = Entry->Previous; else State->Tail = Entry->Previous;
    Entry->Previous = Entry->Next = SIZE_MAX; Entry->Queued = false;
}
static void xredisPoolConnectQueue(xredispoolconnectstate* State, size_t Item)
{
    xredispoolconnectentry* Entry = &State->Entries[Item]; if (Entry->Queued) return;
    Entry->Queued = true; Entry->Next = SIZE_MAX; Entry->Previous = State->Tail;
    if (State->Tail != SIZE_MAX) State->Entries[State->Tail].Next = Item; else State->Head = Item;
    State->Tail = Item;
}
static void xredisPoolConnectReadyDrop(xredispool* Pool, size_t Item)
{
    xredispoolconnectentry* Entry = &Pool->Connector->Entries[Item];
    if (Entry->Registered) { xrtFutureWatchRemove(Entry->Ready, &Entry->Watch); Entry->Registered = false; }
    xrtFutureDestroy(Entry->Ready); Entry->Ready = NULL;
}
static xredisrequeststatus xredisPoolConnectReadyStatus(xredisreadystatus Status)
{
    switch (Status) {
    case XREDIS_READY_TIMEOUT: return XREDIS_REQUEST_TIMEOUT;
    case XREDIS_READY_CLOSED: return XREDIS_REQUEST_CLOSED;
    case XREDIS_READY_DISCONNECTED: return XREDIS_REQUEST_DISCONNECTED;
    case XREDIS_READY_PROTOCOL: return XREDIS_REQUEST_PROTOCOL;
    case XREDIS_READY_LIMIT: return XREDIS_REQUEST_LIMIT;
    case XREDIS_READY_MEMORY: return XREDIS_REQUEST_MEMORY;
    default: return XREDIS_REQUEST_INITIALIZATION;
    }
}
static void xredisPoolConnectFailed(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Failures < 31) ++Entry->Failures;
    xrtMutexLock(&Pool->Gate); xredisPoolConnectIncrement(&State->Info.Failures); State->Info.LastFailure = Status; xrtMutexUnlock(&Pool->Gate);
}
static double xredisPoolConnectRetry(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    uint64 Cap = (uint64)State->Config.RetryMin << Entry->Failures;
    if (Cap > (uint64)State->Config.RetryMax) Cap = (uint64)State->Config.RetryMax;
    /* Jitter is for retry dephasing, never for cryptographic purposes. */
    uint64 Value = State->Random; Value ^= Value << 13; Value ^= Value >> 7; Value ^= Value << 17; State->Random = Value;
    uint64 Delay = (uint64)State->Config.RetryMin + Value % (Cap - (uint64)State->Config.RetryMin + 1u);
    return xredisPoolDeadline(xrtTimer(), (int64)Delay);
}
static void xredisPoolConnectRetired(xredispool* Pool, xredispoolslot* Slot, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
    xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
    xredisPoolConnectReadyDrop(Pool, Item);
    if (Entry->Phase == XREDIS_PC_ONLINE && xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
    xrtMutexLock(&Pool->Gate);
    if (Entry->InFlight) { Entry->InFlight = false; --State->Info.Connecting; }
    bool Open = Pool->Info.State == XREDIS_POOL_OPEN;
    if (Open) { Entry->Phase = XREDIS_PC_WAIT; ++State->Info.Waiting; }
    else { Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true; ++Pool->Retired; }
    xrtMutexUnlock(&Pool->Gate);
    if (Open) xredisPoolConnectHeapPut(State, Item, xredisPoolConnectRetry(Pool, Item));
}
static void xredisPoolConnectDrain(xredispool* Pool, size_t Item, xredisrequeststatus Status)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Phase != XREDIS_PC_DRAIN) {
        if (xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, Status);
        Entry->Phase = XREDIS_PC_DRAIN; xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectReadyDrop(Pool, Item);
    }
    xredisClientClose(Pool->Slots[Item].Client); xredisPoolSlotNotify(&Pool->Slots[Item]);
}
static bool xredisPoolConnectProcess(xredispool* Pool, xredispoolslot* Slot)
{
    xredispoolconnectstate* State = Pool->Connector; size_t Item = (size_t)(Slot - Pool->Slots);
    xredispoolconnectentry* Entry = &State->Entries[Item];
    if (Entry->Phase == XREDIS_PC_ONLINE) return false;
    if (Entry->Phase == XREDIS_PC_WAIT) {
        if (!xredisPoolConnectOpen(Pool)) {
            xredisPoolConnectHeapRemove(State, Item); xredisPoolConnectQueueRemove(State, Item);
            Entry->Phase = XREDIS_PC_DONE; Slot->Retired = true;
            xrtMutexLock(&Pool->Gate); --State->Info.Waiting; ++Pool->Retired; xrtMutexUnlock(&Pool->Gate);
        }
        return true;
    }
    if (Entry->Phase == XREDIS_PC_DONE) return true;
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Slot->Client, &Info);
    if (Entry->Phase == XREDIS_PC_CONNECTING) {
        if (!xredisPoolConnectOpen(Pool)) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_CLOSED);
        else if (xrtTimer() >= Entry->Due) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_TIMEOUT);
        else if (xrtFutureState(Entry->Ready) != XFUTURE_PENDING) {
            const xredisreadyresult* Ready = xrtFutureValue(Entry->Ready);
            if (Ready && Ready->Status == XREDIS_READY_OK && Ready->Protocol == State->Config.Protocol &&
                Slot->Claimed && Info.State == XREDIS_CLIENT_OPEN && !Info.Requests) {
                xredisPoolConnectReadyDrop(Pool, Item); xredisPoolConnectHeapRemove(State, Item);
                Entry->Phase = XREDIS_PC_ONLINE; Entry->Failures = 0; Entry->InFlight = false; Slot->Dead = false;
                xrtMutexLock(&Pool->Gate); --State->Info.Connecting; ++Pool->Info.Live; xredisPoolConnectIncrement(&State->Info.Connected); xrtMutexUnlock(&Pool->Gate);
                return false;
            }
            xredisPoolConnectDrain(Pool, Item, Ready ? xredisPoolConnectReadyStatus(Ready->Status) : XREDIS_REQUEST_INITIALIZATION);
        } else if (Info.State >= XREDIS_CLIENT_CLOSING) xredisPoolConnectDrain(Pool, Item, Info.CloseStatus);
    }
    if (Entry->Phase == XREDIS_PC_DRAIN) {
        if (!Info.Requests && xrtFutureState(Slot->Closed) != XFUTURE_PENDING && xredisPoolDropMember(Pool, Slot))
            xredisPoolConnectRetired(Pool, Slot, Info.CloseStatus);
        else if (!Slot->Registered) xredisPoolConnectHeapPut(State, Item, xredisPoolDeadline(xrtTimer(), 10));
    }
    return true;
}
static bool xredisPoolConnectWatch(xredispool* Pool, xredispoolslot* Slot, xfuture* Future, xfuturewatch* Watch, bool* Registered)
{
    if (!xrtFutureWatchInit(Watch, xredisPoolSlotNotify, xredisPoolSlotDrop, Slot)) return false;
    xredisPoolRef(Pool); xfuturewatchresult Watched = xrtFutureWatchAdd(Future, Watch);
    *Registered = Watched == XFUTURE_WATCH_PENDING;
    if (!*Registered) xredisPoolRelease(Pool);
    return Watched != XFUTURE_WATCH_ERROR;
}
static void xredisPoolConnectStart(xredispool* Pool, size_t Item)
{
    xredispoolconnectstate* State = Pool->Connector; xredispoolconnectentry* Entry = &State->Entries[Item]; xredispoolslot* Slot = &Pool->Slots[Item];
    xrtMutexLock(&Pool->Gate);
    if (Pool->Info.State != XREDIS_POOL_OPEN) { xrtMutexUnlock(&Pool->Gate); return; }
    --State->Info.Waiting; ++State->Info.Connecting; xredisPoolConnectIncrement(&State->Info.Attempts);
    Entry->InFlight = true; Entry->Phase = XREDIS_PC_CONNECTING; xredisPoolConnectIncrement(&Entry->Attempt);
    xrtMutexUnlock(&Pool->Gate);
    Entry->Due = xredisPoolDeadline(xrtTimer(), State->Config.AttemptTimeout);
    xredisclient* Client = NULL;
    bool Started = State->Config.Connect(Pool->Engine, Item, Entry->Attempt, State->Config.AttemptTimeout, State->Config.Data, &Client);
    if (!Started || !Client) {
        if (xredisPoolConnectOpen(Pool)) xredisPoolConnectFailed(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
        xredisPoolConnectRetired(Pool, Slot, XREDIS_REQUEST_INITIALIZATION); return;
    }
    Slot->Client = Client; Slot->Dead = true;
    xrtAtomic32Store(&Slot->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE); Slot->Owner.Deadline = INFINITY;
    if (__xredisClientClaimStarting(Client, &Slot->Owner, Pool->Engine) == XREDIS_ADMISSION_ACCEPTED) {
        Slot->Claimed = true; xredisClientRelease(Client); /* Keep the native claim reference. */
    }
    Slot->Closed = xredisClientClosed(Client); Entry->Ready = xredisClientReady(Client);
    bool ClosedWatch = xredisPoolConnectWatch(Pool, Slot, Slot->Closed, &Slot->Watch, &Slot->Registered);
    bool ReadyWatch = xredisPoolConnectWatch(Pool, Slot, Entry->Ready, &Entry->Watch, &Entry->Registered);
    if (!Slot->Claimed || !ClosedWatch || !ReadyWatch) xredisPoolConnectDrain(Pool, Item, XREDIS_REQUEST_INITIALIZATION);
    else xredisPoolConnectHeapPut(State, Item, Entry->Due);
    xredisPoolSlotNotify(Slot);
}
static void xredisPoolConnectDrive(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    /* Move due retries out of the heap even when concurrency is full. They
     * must never hide a later connecting attempt's hard timeout at the root. */
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->HeapCount; ++Count) {
        size_t Item = State->Heap[0]; xredispoolconnectentry* Entry = &State->Entries[Item];
        if (Entry->Due > xrtTimer()) break;
        xredisPoolConnectHeapRemove(State, Item);
        if (Entry->Phase == XREDIS_PC_WAIT && xredisPoolConnectOpen(Pool)) xredisPoolConnectQueue(State, Item);
        else xredisPoolSlotNotify(&Pool->Slots[Item]);
    }
    for (size_t Count = 0; Count < Pool->Config.WorkItems && State->Head != SIZE_MAX; ++Count) {
        if (State->Info.Connecting >= State->Config.MaxConnecting || !xredisPoolConnectOpen(Pool)) break;
        size_t Item = State->Head; xredisPoolConnectQueueRemove(State, Item); xredisPoolConnectStart(Pool, Item);
    }
}
static bool xredisPoolConnectMore(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    return (xredisPoolConnectOpen(Pool) && State->Head != SIZE_MAX && State->Info.Connecting < State->Config.MaxConnecting) ||
        (State->HeapCount && State->Entries[State->Heap[0]].Due <= xrtTimer());
}
static void xredisPoolConnectTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xredispool* Pool = Data; xredispoolconnectstate* State = Pool->Connector;
    if (State->Timer == Id) State->Timer = 0;
    if (Result != XNET_RESULT_OK && Result != XNET_RESULT_CANCELLED) xredisPoolClose(Pool);
    xrtMutexLock(&Pool->Gate); xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate); xredisPoolRelease(Pool);
}
static void xredisPoolConnectSchedule(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector;
    double Deadline = State->HeapCount ? State->Entries[State->Heap[0]].Due : INFINITY;
    if (Deadline <= xrtTimer()) Deadline = INFINITY; /* Bounded post continuation handles due work. */
    if (State->Timer && State->TimerDeadline == Deadline) return;
    if (State->Timer) xrtNetEngineTimerCancelCurrent(Pool->Engine, State->Timer);
    if (State->Timer || Deadline == INFINITY) return;
    State->TimerDeadline = Deadline; xredisPoolRef(Pool);
    State->Timer = __xrtNetEngineSchedule(Pool->Engine, xrtNetWorkerIndex(Pool->Worker), Deadline, xredisPoolConnectTimer, Pool);
    if (!State->Timer) {
        xrtMutexLock(&Pool->Gate); State->Info.LastFailure = XREDIS_REQUEST_MEMORY; xrtMutexUnlock(&Pool->Gate);
        xredisPoolClose(Pool); xredisPoolRelease(Pool);
    }
}
static bool xredisPoolConnectDispose(xredispool* Pool)
{
    xredispoolconnectstate* State = Pool->Connector; if (State->Timer) return false;
    xrtMutexLock(&Pool->Gate);
    bool Ready = !Pool->Queued && !Pool->Info.Pending && !Pool->Info.Leased && Pool->Retired == Pool->Count && !Pool->LeaseEvents && !Pool->SlotEvents;
    xrtMutexUnlock(&Pool->Gate);
    if (!Ready) return false;
    if (!State->Dropped) {
        xredispoolconnectdrop Drop = State->Config.Drop; ptr Data = State->Config.Data;
        State->Dropped = true; State->Config.Connect = NULL; State->Config.Drop = NULL; State->Config.Data = NULL;
        if (Drop) Drop(Data);
    }
    return true;
}
xredisadmission __xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{
    if (!Engine || !Count || Count > 65536 || !Result || !xrtMemRangeValid(Result, sizeof(*Result)) ||
        !Connect || !xrtMemRangeValid(Connect, sizeof(*Connect)) || xrtMemRangesOverlap(Connect, sizeof(*Connect), Result, sizeof(*Result)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Result, sizeof(*Result))))) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool config/output"); return XREDIS_ADMISSION_ERROR;
    }
    xredispoolconfig Options; if (Config) Options = *Config; else xredisPoolConfigInit(&Options);
    xredispoolconnectconfig Factory = *Connect;
    if (!xredisPoolBounds(&Options) || Factory.Size != sizeof(Factory) || !Factory.Connect ||
        (Factory.Protocol != XREDIS_RESP2 && Factory.Protocol != XREDIS_RESP3) || !Factory.MaxConnecting || Factory.MaxConnecting > 65536 ||
        Factory.AttemptTimeout <= 0 || Factory.AttemptTimeout > 60000 || Factory.RetryMin <= 0 || Factory.RetryMax < Factory.RetryMin || Factory.RetryMax > 60000) {
        xredisPoolReport(XERR_ARGUMENT, "invalid automatic pool bounds/factory/profile"); return XREDIS_ADMISSION_ERROR;
    }
    xredispool* Pool = xredisPoolAllocate(Engine, Affinity, Count, &Options); if (!Pool) return XREDIS_ADMISSION_ERROR;
    size_t Bytes = sizeof(xredispoolconnectstate) + Count * (sizeof(xredispoolconnectentry) + sizeof(size_t));
    xredispoolconnectstate* State = Pool->Connector = xrtCalloc(1, Bytes);
    if (!State) { xrtNetEngineUnpin(Engine); xredisPoolRelease(Pool); return XREDIS_ADMISSION_ERROR; }
    State->Config = Factory; if (State->Config.MaxConnecting > Count) State->Config.MaxConnecting = Count;
    State->Entries = (xredispoolconnectentry*)(State + 1); State->Heap = (size_t*)(State->Entries + Count);
    State->Head = State->Tail = SIZE_MAX; State->HeapCount = Count;
    State->Random = ((uint64)(xrtTimer() * 1000000.0) ^ (uint64)(uintptr_t)Pool) | 1u;
    State->Info.Size = sizeof(State->Info); State->Info.Waiting = Count;
    double Now = xrtTimer();
    for (size_t Item = 0; Item < Count; ++Item) {
        xredisPoolSlotInit(Pool, &Pool->Slots[Item]); Pool->Slots[Item].Dead = true;
        State->Entries[Item].Heap = State->Heap[Item] = Item; State->Entries[Item].Due = Now;
        State->Entries[Item].Previous = State->Entries[Item].Next = SIZE_MAX;
    }
    Pool->InitializeNext = Count; xredisPoolRef(Pool);
    xrtMutexLock(&Pool->Gate); Pool->Initialized = true; *Result = Pool; xredisPoolWakeLocked(Pool); xrtMutexUnlock(&Pool->Gate);
    return XREDIS_ADMISSION_ACCEPTED;
}
bool __xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{
    if (!Pool || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
        xrtMemRangesOverlap(Pool, Pool ? Pool->StorageBytes : 0, Info, sizeof(*Info))) return xredisPoolReport(XERR_ARGUMENT, "invalid pool connector snapshot output");
    xrtMutexLock(&Pool->Gate); bool Valid = Pool->Connector != NULL; if (Valid) *Info = Pool->Connector->Info; xrtMutexUnlock(&Pool->Gate);
    return Valid ? true : xredisPoolReport(XERR_STATE, "pool has no automatic connector");
}
#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_connect/pool_connect.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
XREDIS_API bool xredisPoolConnectConfigInit(xredispoolconnectconfig* Config)
{
    if (!Config || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid pool connector config output"); return false;
    }
    *Config = (xredispoolconnectconfig){sizeof(*Config), NULL, NULL, NULL,
        XREDIS_RESP3, 4, 5000, 100, 10000}; return true;
}
XREDIS_API xredisadmission xredisPoolConnect(xnetengine* Engine, uint64 Affinity, size_t Count,
    const xredispoolconfig* Config, const xredispoolconnectconfig* Connect, xredispool** Result)
{ return __xredisPoolConnect(Engine, Affinity, Count, Config, Connect, Result); }
XREDIS_API bool xredisPoolConnectInfo(xredispool* Pool, xredispoolconnectinfo* Info)
{ return __xredisPoolConnectInfo(Pool, Info); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/transaction/transaction.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)

#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
XREDIS_API bool xredisClientTransactionConfigInit(xredistransactionconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, "invalid transaction config output"); return false;
    }
    *Config = (xredistransactionconfig){sizeof(*Config), XREDIS_TRANSACTION_COMMANDS_DEFAULT}; return true;
}

XREDIS_API xredisadmission xredisClientTransactionAsync(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransactionAsync(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}

XREDIS_API xredisadmission xredisClientTransaction(xredisclient* Client,
    const xredistransactioncommand* Commands, size_t Count,
    const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{
    return __xredisClientTransaction(Client, Commands, Count, Config, Timeout, xrtTimer(), Future);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/watch/watch.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_WATCH)
#include <math.h>
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_WATCH)
enum { XREDIS_W_WATCH, XREDIS_W_COMMIT, XREDIS_W_UNWATCH, XREDIS_W_CLIENT_CLOSED, XREDIS_W_SLOTS };
struct xrediswatch {
    xatomic32 References, Reason, Abandoned;
    xmutex Gate;
    xredisexclusiveowner Owner;
    xrediswatchconfig Config;
    xredisclient* Client;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetpost Work;
    xfuturewatch Watches[XREDIS_W_SLOTS];
    xfuture* Futures[XREDIS_W_SLOTS];
    bool Registered[XREDIS_W_SLOTS];
    xpromise *StartPromise, *ClosePromise;
    xfuture *StartFuture, *CloseFuture;
    xrediswatchresult* StartResult;
    xrediswatchclosedresult* CloseResult;
    xcancelwatch* Cancel;
    uint64 Timer;
    size_t APIs;
    bool Initialized, Queued, Finished, Armed, WatchSeen, CommitSeen, Cleared, UnwatchSeen;
};

static void xredisWatchWork(xnetworker* Worker, ptr Data);
static void xredisWatchWake(ptr Data);
static bool xredisWatchReport(xerrkind Kind, cstr Text)
{ xrtSetErrorInfo(Kind, "xrt.redis", Kind == XERR_STATE ? XREDIS_CLIENT_STATE : XREDIS_CLIENT_ARGUMENT, Text); return false; }
static void xredisWatchRef(ptr Data)
{ (void)xrtAtomic32FetchAdd(&((xrediswatch*)Data)->References, 1, XMEMORY_RELAXED); }

XREDIS_API bool xredisWatchRetain(xrediswatch* Watch)
{
    if (Watch == NULL) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchRef(Watch); return true;
}
XREDIS_API void xredisWatchRelease(xrediswatch* Watch)
{
    if (Watch == NULL || xrtAtomic32FetchSub(&Watch->References, 1, XMEMORY_ACQ_REL) != 1) return;
    xrtCancelUnwatch(Watch->Cancel);
    xrtPromiseDestroy(Watch->StartPromise); xrtFutureDestroy(Watch->StartFuture);
    xrtPromiseDestroy(Watch->ClosePromise); xrtFutureDestroy(Watch->CloseFuture);
    xrtFree(Watch->StartResult); xrtFree(Watch->CloseResult);
    xrtMutexUnit(&Watch->Gate); xrtFree(Watch);
}
static void xredisWatchDrop(ptr Data) { xredisWatchRelease(Data); }

/* Notifications never call the session while holding the client Gate. The
 * separate engine Pin protects posts even after an external ClientClose. */
static void xredisWatchWake(ptr Data)
{
    xrediswatch* Watch = Data; xrtMutexLock(&Watch->Gate);
    if (Watch->Initialized && !Watch->Finished && !Watch->Queued) {
        Watch->Queued = true; xredisWatchRef(Watch);
        if (!xrtNetPost(Watch->Worker, &Watch->Work, xredisWatchWork, Watch)) {
            Watch->Queued = false; xredisWatchRelease(Watch);
        }
    }
    xrtMutexUnlock(&Watch->Gate);
}
static void xredisWatchReason(xrediswatch* Watch, xredisrequeststatus Status)
{
    uint32 Expected = 0;
    (void)xrtAtomic32CompareExchange(&Watch->Reason, &Expected, (uint32)Status + 1u, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE);
}
static void xredisWatchStop(xrediswatch* Watch, xredisrequeststatus Status)
{
    xrtMutexLock(&Watch->Gate);
    if (!Watch->Finished) {
        xredisWatchReason(Watch, Status);
        xrtAtomic32Store(&Watch->Abandoned, 1, XMEMORY_RELEASE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
}
static void xredisWatchCancel(ptr Data) { xredisWatchStop(Data, XREDIS_REQUEST_CANCELLED); }
static void xredisWatchTimer(xnetworker* Worker, uint64 Id, xnetresult Result, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    if (Watch->Timer == Id) Watch->Timer = 0;
    if (Result == XNET_RESULT_OK) xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    xredisWatchWake(Watch);
    xredisWatchRelease(Watch);
}
static bool xredisWatchObserve(xrediswatch* Watch, unsigned Slot)
{
    xredisWatchRef(Watch);
    xfuturewatchresult Result = xrtFutureWatchAdd(Watch->Futures[Slot], &Watch->Watches[Slot]);
    Watch->Registered[Slot] = Result == XFUTURE_WATCH_PENDING;
    if (Result != XFUTURE_WATCH_PENDING) xredisWatchRelease(Watch);
    return Result != XFUTURE_WATCH_ERROR;
}
static bool xredisWatchOK(const xredisrequestresult* Result)
{
    xredisreplyvalue Root;
    return Result && Result->Status == XREDIS_REQUEST_OK && Result->Reply &&
        xredisReplyGet(Result->Reply, 0, &Root) && Root.Type == XREDIS_REPLY_SIMPLE_STRING &&
        Root.Data.Size == 2 && memcmp(Root.Data.Data, "OK", 2) == 0;
}
static const xredisrequestresult* xredisWatchOutcome(xfuture* Future)
{
    return Future && xrtFutureState(Future) != XFUTURE_PENDING ? xrtFutureValue(Future) : NULL;
}
static void xredisWatchResultDrop(ptr Value, ptr Data)
{
    (void)Data; xrediswatchresult* Result = Value;
    xredisReplyRelease(Result->Request.Reply); xrtErrorFree((xerror*)Result->Request.Cause);
    xredisWatchRelease(Result->Watch); xrtFree(Result);
}
static void xredisWatchCloseResultDrop(ptr Value, ptr Data) { (void)Data; xrtFree(Value); }

static void xredisWatchPublish(xrediswatch* Watch, xredisrequeststatus Status, const xredisrequestresult* Source)
{
    if (Watch->StartPromise == NULL) return;
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    xpromise* Promise = Watch->StartPromise; Watch->StartPromise = NULL;
    xrediswatchresult* Result = Watch->StartResult; Watch->StartResult = NULL;
    Result->Request = (xredisrequestresult){Status, Source ? Source->Delivery : XREDIS_MAY_HAVE_EXECUTED,
        Source ? Source->Reply : NULL, Source ? xrtErrorRef(Source->Cause) : NULL};
    if (Result->Request.Reply) xredisReplyRetain(Result->Request.Reply);
    /* Even a failed/cancelled start must expose its physical cleanup barrier.
     * The inert handle retains metadata, not the client/engine after Closed. */
    xredisWatchRef(Watch); Result->Watch = Watch;
    xfuture* Future = Watch->StartFuture; Watch->StartFuture = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchResultDrop, NULL)) xredisWatchResultDrop(Result, NULL);
    xrtPromiseDestroy(Promise); xrtFutureDestroy(Future);
}

/* Called only by the owning worker, with all native reservations physically
 * idle. Remove notifications before releasing the independent engine Pin.
 * A post queued by any racing notification/API must run first. */
static void xredisWatchFinish(xrediswatch* Watch, bool Reusable)
{
    /* Sealing prevents new APIs; an already admitted commit may still be
     * attaching its observer. Do not inspect/detach that storage until its
     * API tail has published and left. No wait occurs under either Gate. */
    xrtMutexLock(&Watch->Gate);
    bool Busy = Watch->APIs != 0;
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) return;
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xrtCancelUnwatch(Watch->Cancel); Watch->Cancel = NULL;
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) if (Watch->Registered[Slot]) {
        xrtFutureWatchRemove(Watch->Futures[Slot], &Watch->Watches[Slot]); Watch->Registered[Slot] = false;
    }
    xrtMutexLock(&Watch->Gate);
    if (Watch->APIs || Watch->Queued || Watch->Timer != 0) { xrtMutexUnlock(&Watch->Gate); return; }
    if (!__xredisClientUnclaim(Watch->Client, &Watch->Owner)) { xrtMutexUnlock(&Watch->Gate); return; }
    Watch->Finished = true; xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSED, XMEMORY_RELEASE);
    xredisclient* Client = Watch->Client; Watch->Client = NULL;
    xnetengine* Engine = Watch->Engine; Watch->Engine = NULL; Watch->Worker = NULL;
    xrtMutexUnlock(&Watch->Gate);
    for (unsigned Slot = 0; Slot < XREDIS_W_SLOTS; ++Slot) {
        xrtFutureDestroy(Watch->Futures[Slot]); Watch->Futures[Slot] = NULL;
    }
    xredisClientRelease(Client); xrtNetEngineUnpin(Engine);
    xrediswatchclosedresult* Result = Watch->CloseResult; Watch->CloseResult = NULL;
    *Result = (xrediswatchclosedresult){(xredisrequeststatus)(xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) - 1u), Reusable};
    xpromise* Promise = Watch->ClosePromise; Watch->ClosePromise = NULL;
    if (!xrtPromiseResolveOwned(Promise, Result, xredisWatchCloseResultDrop, NULL)) xrtFree(Result);
    xrtPromiseDestroy(Promise);
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    /* The parent's pool/lease barrier must include this scope's own Pin,
     * observers and physical Closed publication, not just native request=0. */
    __xredisClientChildDone(&Watch->Owner);
#endif
    xredisWatchRelease(Watch); /* Active lifetime. */
}

static void xredisWatchWork(xnetworker* Worker, ptr Data)
{
    (void)Worker; xrediswatch* Watch = Data;
    xrtMutexLock(&Watch->Gate); Watch->Queued = false;
    bool Busy = Watch->APIs != 0; xfuture* Commit = Watch->Futures[XREDIS_W_COMMIT];
    xrtMutexUnlock(&Watch->Gate);
    if (Busy) { xredisWatchRelease(Watch); return; }
    if (!Watch->Armed) {
        Watch->Armed = true; xredisWatchRef(Watch);
        Watch->Timer = __xrtNetEngineSchedule(Watch->Engine, xrtNetWorkerIndex(Watch->Worker), Watch->Owner.Deadline, xredisWatchTimer, Watch);
        if (!Watch->Timer) { xredisWatchRelease(Watch); xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY); }
    }
    if (!xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) && xrtTimer() >= Watch->Owner.Deadline)
        xredisWatchStop(Watch, XREDIS_REQUEST_TIMEOUT);
    const xredisrequestresult* Start = xredisWatchOutcome(Watch->Futures[XREDIS_W_WATCH]);
    if (Start && !Watch->WatchSeen) {
        Watch->WatchSeen = true;
        uint32 Expected = XREDIS_OWNER_STARTING;
        if (xredisWatchOK(Start) && !xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE) &&
            xrtAtomic32CompareExchange(&Watch->Owner.State, &Expected, XREDIS_OWNER_ACTIVE, XMEMORY_ACQ_REL, XMEMORY_ACQUIRE)) {
            xredisWatchPublish(Watch, XREDIS_REQUEST_OK, Start);
        } else {
            uint32 Stopped = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
            xredisrequeststatus Status = Stopped ? (xredisrequeststatus)(Stopped - 1u) :
                Start->Status == XREDIS_REQUEST_OK ? XREDIS_REQUEST_PROTOCOL : Start->Status;
            xredisWatchReason(Watch, Status);
            xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
            if (!Stopped && Status == XREDIS_REQUEST_PROTOCOL) xredisClientClose(Watch->Client);
        }
    }
    const xredistransactionresult* Transaction = (const xredistransactionresult*)xredisWatchOutcome(Commit);
    if (Transaction && !Watch->CommitSeen) {
        Watch->CommitSeen = true; xredisWatchReason(Watch, Transaction->Request.Status);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
        Watch->Cleared = Transaction->Execution == XREDIS_TRANSACTION_EXECUTED || Transaction->Execution == XREDIS_TRANSACTION_ABORTED ||
            (Transaction->Request.Status == XREDIS_REQUEST_SERVER_ERROR && Transaction->QueueErrors != 0 &&
             Transaction->Execution == XREDIS_TRANSACTION_NOT_EXECUTED);
        if (Transaction->Request.Status == XREDIS_REQUEST_TIMEOUT || Transaction->Request.Status == XREDIS_REQUEST_CANCELLED)
            xredisWatchStop(Watch, Transaction->Request.Status);
    }
    uint32 Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
    xredisclientinfo Info = {0}; Info.Size = sizeof(Info); xredisClientInfo(Watch->Client, &Info);
    if (Info.State >= XREDIS_CLIENT_CLOSING && !Reason) {
        xredisWatchReason(Watch, Info.CloseStatus); Reason = xrtAtomic32Load(&Watch->Reason, XMEMORY_ACQUIRE);
        xrtAtomic32Store(&Watch->Owner.State, XREDIS_OWNER_CLOSING, XMEMORY_RELEASE);
    }
    if (!Reason) { xredisWatchRelease(Watch); return; }
    if (Watch->Timer != 0) (void)xrtNetEngineTimerCancelCurrent(Watch->Engine, Watch->Timer);
    xredisWatchPublish(Watch, (xredisrequeststatus)(Reason - 1u), Start);
    if (xrtAtomic32Load(&Watch->Abandoned, XMEMORY_ACQUIRE)) {
        if (Watch->Futures[XREDIS_W_WATCH]) xrtFutureCancel(Watch->Futures[XREDIS_W_WATCH]);
        if (Commit) xrtFutureCancel(Commit);
        /* Once cleanup itself owns the last native reservation, allow its
         * bounded deadline/drain to finish. Repeated Abort is idempotent. */
        if (Info.Requests && Watch->Futures[XREDIS_W_UNWATCH] == NULL) xredisClientClose(Watch->Client);
        xredisClientInfo(Watch->Client, &Info);
    }
    if (Info.State >= XREDIS_CLIENT_CLOSING) {
        if (!Info.Requests && xrtFutureState(Watch->Futures[XREDIS_W_CLIENT_CLOSED]) != XFUTURE_PENDING) xredisWatchFinish(Watch, false);
    } else if (!Info.Requests) {
        if (Watch->Cleared) xredisWatchFinish(Watch, true);
        else if (Watch->Futures[XREDIS_W_UNWATCH] == NULL) {
            xbytesview Word = XRT_BYTES_LITERAL("UNWATCH");
            xredisadmission Admission = __xredisClientOwnedCall(Watch->Client, &Watch->Owner, &Word, 1,
                Watch->Config.CleanupTimeout, xrtTimer(), true, &Watch->Futures[XREDIS_W_UNWATCH]);
            if (Admission != XREDIS_ADMISSION_ACCEPTED || !xredisWatchObserve(Watch, XREDIS_W_UNWATCH)) xredisClientClose(Watch->Client);
        } else if (!Watch->UnwatchSeen) {
            const xredisrequestresult* Result = xredisWatchOutcome(Watch->Futures[XREDIS_W_UNWATCH]);
            if (Result) {
                Watch->UnwatchSeen = true;
                if (xredisWatchOK(Result)) { Watch->Cleared = true; xredisWatchFinish(Watch, true); }
                else xredisClientClose(Watch->Client);
            }
        }
    }
    xredisWatchRelease(Watch);
}

XREDIS_API bool xredisWatchConfigInit(xrediswatchconfig* Config)
{
    if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisWatchReport(XERR_ARGUMENT, "invalid watch config");
    *Config = (xrediswatchconfig){sizeof(*Config), 5000, 5000, 256, 65536, {sizeof(xredistransactionconfig), XREDIS_TRANSACTION_COMMANDS_DEFAULT}};
    return true;
}

static xredisadmission xredisWatchBegin(xredisclient* Client, const xbytesview* Keys, size_t Count,
    const xrediswatchconfig* Config, xfuture** Future, double Entered, xredisexclusiveowner* Parent)
{
    xrediswatchconfig Options;
    if (Client == NULL || Future == NULL || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        (Config && (!xrtMemRangeValid(Config, sizeof(*Config)) || xrtMemRangesOverlap(Config, sizeof(*Config), Future, sizeof(*Future))))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch arguments/output"); return XREDIS_ADMISSION_ERROR;
    }
    if (Config == NULL) xredisWatchConfigInit(&Options); else Options = *Config;
    if (Options.Size != sizeof(Options) || Options.Timeout <= 0 || Options.Timeout > 86400000 ||
        Options.CleanupTimeout <= 0 || Options.CleanupTimeout > 60000 || !Options.MaxKeys || Options.MaxKeys > 65536 ||
        !Options.MaxWireBytes || !Count || Count > Options.MaxKeys || !Keys || !xrtMemRangeValid(Keys, Count * sizeof(*Keys)) ||
        xrtMemRangesOverlap(Keys, Count * sizeof(*Keys), Future, sizeof(*Future)) ||
        Options.Transaction.Size != sizeof(Options.Transaction) || !Options.Transaction.MaxCommands || Options.Transaction.MaxCommands > XREDIS_TRANSACTION_COMMANDS_MAX) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch config/keys"); return XREDIS_ADMISSION_ERROR;
    }
    for (size_t Index = 0; Index < Count; ++Index) if (!xrtMemRangeValid(Keys[Index].Data, Keys[Index].Size) ||
        xrtMemRangesOverlap(Keys[Index].Data, Keys[Index].Size, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch key/output alias"); return XREDIS_ADMISSION_ERROR;
    }
    xbytesview* Arguments = xrtMalloc((Count + 1u) * sizeof(*Arguments));
    if (!Arguments) return XREDIS_ADMISSION_ERROR;
    Arguments[0] = (xbytesview)XRT_BYTES_LITERAL("WATCH"); memcpy(Arguments + 1, Keys, Count * sizeof(*Keys));
    size_t Bytes = 0;
    if (!xredisCommandSize(Arguments, Count + 1u, &Bytes) || Bytes > Options.MaxWireBytes) {
        xrtFree(Arguments); xredisWatchReport(XERR_RANGE, "WATCH exceeds wire limit"); return XREDIS_ADMISSION_ERROR;
    }
    xrediswatch* Watch = xrtMalloc(sizeof(*Watch));
    if (!Watch) { xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    memset(Watch, 0, sizeof(*Watch));
    if (!xrtMutexInit(&Watch->Gate)) { xrtFree(Watch); xrtFree(Arguments); return XREDIS_ADMISSION_ERROR; }
    xrtAtomic32Init(&Watch->References, 1); xrtAtomic32Init(&Watch->Reason, 0); xrtAtomic32Init(&Watch->Abandoned, 0);
    Watch->Config = Options; Watch->Owner.Data = Watch;
    xrtAtomic32Init(&Watch->Owner.State, XREDIS_OWNER_STARTING);
    Watch->Owner.Deadline = nextafter(Entered + (double)Options.Timeout * 0.001, INFINITY);
    Watch->Owner.Retain = xredisWatchRef; Watch->Owner.Release = xredisWatchDrop; Watch->Owner.Wake = xredisWatchWake;
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    Watch->Owner.Abandon = xredisWatchCancel;
    xrtAtomic32Init(&Watch->Owner.Nested, 0);
#else
    (void)Parent;
#endif
    Watch->StartResult = xrtMalloc(sizeof(*Watch->StartResult)); Watch->CloseResult = xrtMalloc(sizeof(*Watch->CloseResult));
    if (Watch->StartResult) memset(Watch->StartResult, 0, sizeof(*Watch->StartResult));
    bool Ready = Watch->StartResult && Watch->CloseResult && xrtNetPostInit(&Watch->Work) &&
        (Watch->StartPromise = xrtPromiseCreate(&Watch->StartFuture, NULL)) &&
        (Watch->ClosePromise = xrtPromiseCreate(&Watch->CloseFuture, NULL));
    for (unsigned Slot = 0; Ready && Slot < XREDIS_W_SLOTS; ++Slot)
        Ready = xrtFutureWatchInit(&Watch->Watches[Slot], xredisWatchWake, xredisWatchDrop, Watch);
    if (Ready) {
        xcancel* Token = xrtFutureCancelToken(Watch->StartFuture);
        if (Token) Watch->Cancel = xrtCancelWatch(Token, xredisWatchCancel, Watch);
        xrtCancelDestroy(Token); Ready = Watch->Cancel != NULL;
    }
    xredisadmission Admission = XREDIS_ADMISSION_ERROR;
    if (Ready) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
        if (Parent != NULL) Admission = __xredisClientClaimNested(Client, Parent, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
        else
#endif
        Admission = __xredisClientClaim(Client, &Watch->Owner, &Watch->Engine, &Watch->Worker, Future);
    }
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        Watch->Client = Client; Watch->Futures[XREDIS_W_CLIENT_CLOSED] = xredisClientClosed(Client);
        Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count + 1u, Options.Timeout, Entered, true, &Watch->Futures[XREDIS_W_WATCH]);
        if (Admission != XREDIS_ADMISSION_ACCEPTED) {
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
            (void)__xredisClientUnclaimUnused(Client, &Watch->Owner);
#else
            (void)__xredisClientUnclaim(Client, &Watch->Owner);
#endif
            xredisClientRelease(Client); xrtNetEngineUnpin(Watch->Engine);
            xrtFutureDestroy(Watch->Futures[XREDIS_W_CLIENT_CLOSED]); Watch->Client = NULL; Watch->Engine = NULL;
        }
    }
    xrtFree(Arguments);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) { xredisWatchRelease(Watch); return Admission; }
    /* Native acceptance is the effects boundary. Every subsequent failure is
     * a typed asynchronous outcome with physical cleanup, never ERROR. */
    bool Observed = xredisWatchObserve(Watch, XREDIS_W_WATCH) && xredisWatchObserve(Watch, XREDIS_W_CLIENT_CLOSED);
    xfuture* Observer = xrtFutureRef(Watch->StartFuture);
    xredisWatchRef(Watch); /* Constructor tail may race an immediate failure. */
    xrtMutexLock(&Watch->Gate); Watch->Initialized = true; xrtMutexUnlock(&Watch->Gate);
    if (!Observed) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    *Future = Observer; xredisWatchWake(Watch); xredisWatchRelease(Watch); return XREDIS_ADMISSION_ACCEPTED;
}
XREDIS_API xredisadmission xredisClientWatchAsync(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, xrtTimer(), NULL); }
XREDIS_API xredisadmission xredisClientWatch(xredisclient* Client, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{
    double Entered = xrtTimer();
    if (Client && __xredisClientIsWorker(Client)) { xredisWatchReport(XERR_STATE, "sync WATCH cannot block an engine worker"); return XREDIS_ADMISSION_ERROR; }
    xredisadmission Admission = xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, NULL);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
xredisadmission __xredisWatchBeginOwned(xredisclient* Client, xredisexclusiveowner* Parent,
    const xbytesview* Keys, size_t Count, const xrediswatchconfig* Config, xfuture** Future, double Entered)
{ return xredisWatchBegin(Client, Keys, Count, Config, Future, Entered, Parent); }
#endif

/* APIs protect the engine/client borrow through encoding and final admission.
 * Commit seals under the native client Gate, not at the start of encoding. */
static xredisadmission xredisWatchEnter(xrediswatch* Watch, bool Sync, xredisclient** Client)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return XREDIS_ADMISSION_ERROR; }
    xrtMutexLock(&Watch->Gate);
    xredisadmission Admission = Watch->Finished || xrtAtomic32Load(&Watch->Owner.State, XMEMORY_ACQUIRE) != XREDIS_OWNER_ACTIVE ?
        XREDIS_ADMISSION_CLOSED : XREDIS_ADMISSION_ACCEPTED;
    if (Admission == XREDIS_ADMISSION_ACCEPTED) { ++Watch->APIs; *Client = Watch->Client; }
    xrtMutexUnlock(&Watch->Gate);
    if (Admission == XREDIS_ADMISSION_ACCEPTED && Sync && __xredisClientIsWorker(*Client)) {
        xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch);
        xredisWatchReport(XERR_STATE, "sync WATCH operation cannot block an engine worker"); return XREDIS_ADMISSION_ERROR;
    }
    return Admission;
}
static void xredisWatchLeave(xrediswatch* Watch)
{ xrtMutexLock(&Watch->Gate); --Watch->APIs; xrtMutexUnlock(&Watch->Gate); xredisWatchWake(Watch); }

static xredisadmission xredisWatchCallAdmit(xrediswatch* Watch, const xbytesview* Arguments, size_t Count,
    int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedCall(Client, &Watch->Owner, Arguments, Count, Timeout, Entered, false, Future);
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCallAsync(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCall(xrediswatch* Watch, const xbytesview* Arguments, size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCallAdmit(Watch, Arguments, Count, Timeout, Future, true, xrtTimer()); }

static xredisadmission xredisWatchCommitAdmit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future, bool Sync, double Entered)
{
    if (!Future || !xrtMemRangeValid(Future, sizeof(*Future)) ||
        xrtMemRangesOverlap(Watch, Watch ? sizeof(*Watch) : 0, Future, sizeof(*Future))) {
        xredisWatchReport(XERR_ARGUMENT, "invalid watch operation output"); return XREDIS_ADMISSION_ERROR;
    }
    xredisclient* Client = NULL; xredisadmission Admission = xredisWatchEnter(Watch, Sync, &Client);
    if (Admission != XREDIS_ADMISSION_ACCEPTED) return Admission;
    Admission = __xredisClientOwnedTransaction(Client, &Watch->Owner, Commands, Count, &Watch->Config.Transaction, Timeout, Entered, Future);
    if (Admission == XREDIS_ADMISSION_ACCEPTED) {
        xrtMutexLock(&Watch->Gate); Watch->Futures[XREDIS_W_COMMIT] = xrtFutureRef(*Future); xrtMutexUnlock(&Watch->Gate);
        if (!xredisWatchObserve(Watch, XREDIS_W_COMMIT)) xredisWatchStop(Watch, XREDIS_REQUEST_MEMORY);
    }
    xredisWatchLeave(Watch);
    if (Sync && Admission == XREDIS_ADMISSION_ACCEPTED) (void)xrtFutureWait(*Future);
    return Admission;
}
XREDIS_API xredisadmission xredisWatchCommitAsync(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, false, xrtTimer()); }
XREDIS_API xredisadmission xredisWatchCommit(xrediswatch* Watch, const xredistransactioncommand* Commands,
    size_t Count, int64 Timeout, xfuture** Future)
{ return xredisWatchCommitAdmit(Watch, Commands, Count, Timeout, Future, true, xrtTimer()); }
XREDIS_API bool xredisWatchAbort(xrediswatch* Watch)
{
    if (!Watch) return xredisWatchReport(XERR_ARGUMENT, "watch is NULL");
    xredisWatchStop(Watch, XREDIS_REQUEST_CANCELLED); return true;
}
XREDIS_API xfuture* xredisWatchClosed(xrediswatch* Watch)
{
    if (!Watch) { xredisWatchReport(XERR_ARGUMENT, "watch is NULL"); return NULL; }
    return xrtFutureRef(Watch->CloseFuture);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_watch/pool_watch.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
XREDIS_API xredisadmission xredisLeaseWatchAsync(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseWatch(xredislease* Lease, const xbytesview* Keys,
    size_t Count, const xrediswatchconfig* Config, xfuture** Future)
{ return __xredisLeaseWatch(Lease, Keys, Count, Config, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/pool_transaction/pool_transaction.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
XREDIS_API xredisadmission xredisLeaseTransactionAsync(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), false); }
XREDIS_API xredisadmission xredisLeaseTransaction(xredislease* Lease, const xredistransactioncommand* Commands,
    size_t Count, const xredistransactionconfig* Config, int64 Timeout, xfuture** Future)
{ return __xredisLeaseTransaction(Lease, Commands, Count, Config, Timeout, Future, xrtTimer(), true); }
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/dial/dial.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
#if defined(XREDIS_FEATURE_XREDIS_TLS)
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_DIAL)
typedef struct xredisconnectdata {
	xredisconnectplan Plan;
	xredisclient* Client; /* Borrowed through this source's retirement callback. */
	xnetresolver* Resolver;
	xnetaddrlist* Addresses;
	xnetdial* Dial;
	xnetdialconfig Config;
	uint16 Port;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xredistlsplan TLS;
#endif
	char Host[];
} xredisconnectdata;

static bool xredisDialArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientDialConfigInit(xredisclientdialconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisDialArgument("invalid Dial config output");
	*Config = (xredisclientdialconfig){sizeof(*Config), XNET_FAMILY_UNSPEC, 30000, 250, 8, 1024}; return true;
}

static void xredisDialDone(xnetdial* Dial, xnetresult Result, xnetstream* Stream, const xerror* Error, ptr Data)
{
	(void)Dial; xredisconnectdata* Context = (xredisconnectdata*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Context->TLS.Session != NULL && Stream != NULL) {
		if (__xredisClientConnectTls(Context->Client, Stream, &Context->TLS)) Context->TLS.Session = NULL;
	} else
#endif
	__xredisClientConnectDone(Context->Client, Result, Stream, Error);
	/* Last access. This same Worker's queued client turn executes after the
	 * native callback's return; no fabricated owned/collector policy is used. */
	__xredisClientConnectRetired(Context->Client);
}

static bool xredisDialStart(ptr Data, xredisclient* Client, xnetengine* Engine, uint64 Affinity,
	const xnetstreamconfig* Config, const xnetstreamevents* Events)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data; Context->Client = Client;
	Context->Config.Stream = *Config; Context->Config.Affinity = Affinity;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (Context->TLS.Session != NULL) Events = __xredisClientTlsPendingEvents();
#endif
	if (Context->Addresses != NULL) Context->Dial = xrtNetDialAddresses(Engine, Context->Addresses, &Context->Config, Events, Client, xredisDialDone, Context);
	else Context->Dial = xrtNetDial(Engine, Context->Resolver, Context->Host, Context->Port, &Context->Config, Events, Client, xredisDialDone, Context);
	if (Context->Dial == NULL) return false;
	xrtNetAddrListDestroy(Context->Addresses); Context->Addresses = NULL; return true;
}

static void xredisDialCancel(ptr Data)
{ xredisconnectdata* Context = (xredisconnectdata*)Data; (void)xrtNetDialCancel(Context->Dial); }

static void xredisDialDrop(ptr Data)
{
	xredisconnectdata* Context = (xredisconnectdata*)Data;
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	xrtTlsSessionDestroy(Context->TLS.Session);
#endif
	xrtNetDialDestroy(Context->Dial); xrtNetAddrListDestroy(Context->Addresses); xrtFree(Context);
}

static bool xredisDialCreate(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial,
	const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result)
{
	if (Engine == NULL || Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Dial != NULL && (!xrtMemRangeValid(Dial, sizeof(*Dial)) || xrtMemRangesOverlap(Dial, sizeof(*Dial), Result, sizeof(*Result)))))
		return xredisDialArgument("invalid Dial arguments or config/output overlap");
	xredisclientdialconfig Policy;
	if (Dial == NULL) xredisClientDialConfigInit(&Policy); else Policy = *Dial;
	if (Policy.Size != sizeof(Policy) || (Policy.Family != XNET_FAMILY_UNSPEC && Policy.Family != XNET_FAMILY_IPV4 && Policy.Family != XNET_FAMILY_IPV6) ||
		Policy.Timeout < XRT_WAIT_FOREVER || Policy.FallbackDelay < 0 || Policy.MaxAttempts == 0 || Policy.MaxAttempts > XREDIS_DIAL_ADDRESSES_MAX ||
		Policy.MaxHostBytes == 0 || Policy.MaxHostBytes > XREDIS_DIAL_HOST_MAX) return xredisDialArgument("invalid Dial family, deadline or bounds");
	if (Addresses == NULL) {
		if (Resolver == NULL || Port == 0 || Host.Size == 0 || Host.Size > Policy.MaxHostBytes ||
			Host.Size > SIZE_MAX - sizeof(xredisconnectdata) - 1u || !xrtMemRangeValid(Host.Data, Host.Size) ||
			xrtMemRangesOverlap(Host.Data, Host.Size, Result, sizeof(*Result)) || memchr(Host.Data, 0, Host.Size) != NULL)
			return xredisDialArgument("invalid host, port, Resolver or input/output overlap");
	} else {
		size_t Count = xrtNetAddrListCount(Addresses), Eligible = 0;
		if (Count == 0 || Count > XREDIS_DIAL_ADDRESSES_MAX) return xredisDialArgument("resolved endpoint list exceeds bounds");
		for (size_t Index = 0; Index < Count; ++Index) {
			const xnetaddr* Address = xrtNetAddrListGet(Addresses, Index); if (Address == NULL) return false;
			if (xrtMemRangesOverlap(Address, sizeof(*Address), Result, sizeof(*Result))) return xredisDialArgument("endpoint output overlaps address storage");
			if (Policy.Family != XNET_FAMILY_UNSPEC && Address->Family != Policy.Family) continue;
			if (Address->Port == 0) return xredisDialArgument("resolved endpoint port is zero");
			++Eligible;
		}
		if (Eligible == 0) return xredisDialArgument("no endpoint matches the selected family");
	}
	xredisclientinitconfig Defaults;
	if (Init == NULL) { xredisClientInitConfigInit(&Defaults); Init = &Defaults; }
	xredisconnectdata* Context = (xredisconnectdata*)xrtCalloc(1, sizeof(*Context) + Host.Size + 1u);
	if (Context == NULL) return false;
	Context->Plan = (xredisconnectplan){Context, xredisDialStart, xredisDialCancel, xredisDialDrop};
	xrtNetDialConfigInit(&Context->Config); Context->Config.Family = Policy.Family; Context->Config.Timeout = Policy.Timeout;
	Context->Config.FallbackDelay = Policy.FallbackDelay; Context->Config.MaxAttempts = Policy.MaxAttempts;
	Context->Resolver = Resolver; Context->Port = Port;
	if (Addresses != NULL) {
		Context->Addresses = xrtNetAddrListRef(Addresses);
		if (Context->Addresses == NULL) { xredisDialDrop(Context); return false; }
	} else memcpy(Context->Host, Host.Data, Host.Size);
#if defined(XREDIS_FEATURE_XREDIS_TLS)
	if (TLS != NULL && !__xredisTlsPlanCreate(TLS, Auth, (xstrview){Context->Host, Host.Size}, Result, &Context->TLS)) { xredisDialDrop(Context); return false; }
#else
	(void)TLS; (void)Auth;
#endif
	if (__xredisClientCreatePlanned(Engine, Affinity, Config, Init, Entered, &Context->Plan, Result)) return true;
	xredisDialDrop(Context); return false;
}

XREDIS_API bool xredisClientDial(xnetengine* Engine, xnetresolver* Resolver,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclientdialconfig* Dial, xredisclient** Result)
{ return xredisDialCreate(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, Dial, NULL, NULL, xrtTimer(), Result); }

XREDIS_API bool xredisClientDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses,
	uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
	const xredisclientdialconfig* Dial, xredisclient** Result)
{
	if (Addresses == NULL) return xredisDialArgument("resolved endpoint list is NULL");
	return xredisDialCreate(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, Dial, NULL, NULL, xrtTimer(), Result);
}
#if defined(XREDIS_FEATURE_XREDIS_TLS)
bool __xredisClientDialTls(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, double Entered, xredisclient** Result)
{
	/* Read the public TLS descriptor only after its full span/output validation. */
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || Result == NULL ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Result, sizeof(*Result))) return xredisDialArgument("invalid TLS config/output");
	return xredisDialCreate(Engine, Resolver, Addresses, Host, Port, Affinity, Config, Init, &TLS->Dial, TLS, NULL, Entered, Result);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
bool __xredisClientDialTlsAuthV1(xnetengine* Engine, xnetresolver* Resolver, xnetaddrlist* Addresses,
	xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
	const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
	const xtlsclientauthconfigv1* Auth, double Entered, xredisclient** Result)
{
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || Auth == NULL ||
		!xrtMemRangeValid(Auth, sizeof(*Auth)) || Auth->Size != sizeof(*Auth) || Result == NULL ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Result, sizeof(*Result)) ||
		xrtMemRangesOverlap(Auth, sizeof(*Auth), Result, sizeof(*Result)))
		return xredisDialArgument("invalid TLS authentication config/output");
	return xredisDialCreate(Engine, Resolver, Addresses, Host, Port, Affinity, Config, Init, &TLS->Dial, TLS, Auth, Entered, Result);
}
#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tls/tls.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TLS)
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
#endif
#include <string.h>

#if defined(XREDIS_FEATURE_XREDIS_TLS)
static bool xredisTlsArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsConfigInit(xredisclienttlsconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config))) return xredisTlsArgument("invalid TLS config output");
	memset(Config, 0, sizeof(*Config)); Config->Size = sizeof(*Config);
	xrtTlsClientConfigInit(&Config->Session); xrtTlsStreamConfigInit(&Config->Stream); xredisClientDialConfigInit(&Config->Dial);
	Config->MaxMetadataBytes = 64u * 1024u; Config->WriteBytes = 64u * 1024u; Config->ServerNameFromHost = true; return true;
}

bool __xredisTlsPlanCreate(const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth,
	xstrview Host, xredisclient** Output, xredistlsplan* Plan)
{
	if (TLS == NULL || !xrtMemRangeValid(TLS, sizeof(*TLS)) || TLS->Size != sizeof(*TLS) ||
		xrtMemRangesOverlap(TLS, sizeof(*TLS), Output, sizeof(*Output)) || TLS->MaxMetadataBytes == 0 || TLS->MaxMetadataBytes > 1024u * 1024u ||
		TLS->WriteBytes == 0 || TLS->WriteBytes > 1024u * 1024u || TLS->Stream.HandshakeTimeout < 0 || TLS->Stream.CloseTimeout < 0 ||
		TLS->Stream.AsyncBytesLimit == 0 || TLS->Stream.AsyncCountLimit == 0 || TLS->Stream.AsyncBatch == 0 || TLS->Session.ProtocolCount > 256u)
		return xredisTlsArgument("invalid TLS descriptor, bounds or output overlap");
	xtlsclientconfig Session = TLS->Session;
	if (TLS->ServerNameFromHost && Session.ServerName.Size == 0 && Host.Size != 0) {
		xerror* Saved = xrtTakeError(); xnetaddr Numeric;
		bool Literal = xrtNetAddrParse(&Numeric, Host.Data, 1); xrtSetErrorTake(Saved);
		if (!Literal) Session.ServerName = Host;
		else if (Session.VerifyName.Size == 0) Session.VerifyName = Host;
	}
	if (Session.ServerName.Size == 0 && Session.VerifyName.Size == 0) return xredisTlsArgument("TLS peer identity is required");
	size_t Bytes = Session.ProtocolCount * sizeof(xstrview);
	if (!xrtMemRangeValid(Session.Protocols, Bytes) || xrtMemRangesOverlap(Session.Protocols, Bytes, Output, sizeof(*Output)) || Bytes > TLS->MaxMetadataBytes)
		return xredisTlsArgument("invalid TLS ALPN descriptors");
	for (size_t Index = 0; Index < Session.ProtocolCount + 2u; ++Index) {
		xstrview View = Index == 0 ? Session.ServerName : Index == 1 ? Session.VerifyName : Session.Protocols[Index - 2u];
		if (View.Size > TLS->MaxMetadataBytes - Bytes || !xrtMemRangeValid(View.Data, View.Size) ||
			xrtMemRangesOverlap(View.Data, View.Size, Output, sizeof(*Output)) || (Index < 2u && View.Size != 0 && memchr(View.Data, 0, View.Size) != NULL))
			return xredisTlsArgument("invalid or excessive TLS name/ALPN bytes");
		Bytes += View.Size;
	}
#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
	Plan->Session = Auth != NULL ? xrtTlsClientCreateAuthV1(&Session, Auth, NULL) : xrtTlsClientCreate(&Session, NULL);
#else
	(void)Auth; Plan->Session = xrtTlsClientCreate(&Session, NULL);
#endif
	if (Plan->Session == NULL) return false;
	Plan->Config = TLS->Stream; Plan->WriteBytes = TLS->WriteBytes; return true;
}

XREDIS_API bool xredisClientTlsDial(xnetengine* Engine, xnetresolver* Resolver, xstrview Host, uint16 Port, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{ return __xredisClientDialTls(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, xrtTimer(), Result); }

XREDIS_API bool xredisClientTlsDialAddresses(xnetengine* Engine, xnetaddrlist* Addresses, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	if (Addresses == NULL) return xredisTlsArgument("TLS endpoint list is NULL");
	return __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsConnect(xnetengine* Engine, const xnetaddr* Remote, uint64 Affinity,
	const xredisclientconfig* Config, const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS, xredisclient** Result)
{
	double Entered = xrtTimer();
	if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
		xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result))) return xredisTlsArgument("invalid TLS numeric endpoint/output");
	xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
	bool Success = __xredisClientDialTls(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Entered, Result);
	xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tls_auth/tls_auth.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)

#if defined(XREDIS_FEATURE_XREDIS_TLS_AUTH)
static bool xredisTlsAuthArgument(cstr Message)
{ xrtSetErrorInfo(XERR_ARGUMENT, "xrt.redis", XREDIS_CLIENT_ARGUMENT, Message); return false; }

XREDIS_API bool xredisClientTlsAuthDialV1(xnetengine* Engine, xnetresolver* Resolver,
    xstrview Host, uint16 Port, uint64 Affinity, const xredisclientconfig* Config,
    const xredisclientinitconfig* Init, const xredisclienttlsconfig* TLS,
    const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    return __xredisClientDialTlsAuthV1(Engine, Resolver, NULL, Host, Port, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthDialAddressesV1(xnetengine* Engine, xnetaddrlist* Addresses,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    if (Addresses == NULL) return xredisTlsAuthArgument("TLS authentication endpoint list is NULL");
    return __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, xrtTimer(), Result);
}

XREDIS_API bool xredisClientTlsAuthConnectV1(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, const xredisclientconfig* Config, const xredisclientinitconfig* Init,
    const xredisclienttlsconfig* TLS, const xtlsclientauthconfigv1* Auth, xredisclient** Result)
{
    double Entered = xrtTimer();
    if (Remote == NULL || Result == NULL || !xrtMemRangeValid(Remote, sizeof(*Remote)) ||
        xrtMemRangesOverlap(Remote, sizeof(*Remote), Result, sizeof(*Result)))
        return xredisTlsAuthArgument("invalid TLS authentication numeric endpoint/output");
    xnetaddrlist* Addresses = xrtNetAddrListCreate(Remote, 1); if (Addresses == NULL) return false;
    bool Success = __xredisClientDialTlsAuthV1(Engine, NULL, Addresses, (xstrview){NULL, 0}, 0, Affinity, Config, Init, TLS, Auth, Entered, Result);
    xrtNetAddrListDestroy(Addresses); return Success;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xredis/src/tracking/tracking.c */
/* ========================================================================== */

#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
XREDIS_API bool xredisTrackingConfigInit(xredistrackingconfig* K)
{
    if(!K || !xrtMemRangeValid(K,sizeof(*K))){
        xrtSetErrorInfo(XERR_ARGUMENT,"xrt.redis",XREDIS_CLIENT_ARGUMENT,"invalid tracking config output");return false;
    }
    *K=(xredistrackingconfig){sizeof(*K),true,0,0,NULL,0};return true;
}
#endif
#endif

#endif
