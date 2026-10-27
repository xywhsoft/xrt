#ifndef XREDIS_READER_H
#define XREDIS_READER_H

#include <xredis/features.h>
#include <xredis/export.h>
#include <xrt/error.h>

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
