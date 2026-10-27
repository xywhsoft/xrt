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

#endif
