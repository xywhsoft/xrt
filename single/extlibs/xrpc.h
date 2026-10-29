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
#error "xrpc requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XRPC_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XRPC_IMPLEMENTATION) && \
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
#ifndef XRPC_SINGLE_HEADER_H
#define XRPC_SINGLE_HEADER_H
#define XRPC_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xrpc/include/xrpc/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XRPC_FEATURES_H
#define XRPC_FEATURES_H

/* xrpc 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC)
#ifndef XRPC_FEATURE_XRPC
#define XRPC_FEATURE_XRPC
#endif
#ifndef XRPC_MODULE_XRPC_DECODE
#define XRPC_MODULE_XRPC_DECODE
#endif
#ifndef XRPC_MODULE_XRPC_ENCODE
#define XRPC_MODULE_XRPC_ENCODE
#endif
#ifndef XRPC_MODULE_XRPC_DISPATCH
#define XRPC_MODULE_XRPC_DISPATCH
#endif
#endif

/* xrpc_dispatch 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_DISPATCH)
#ifndef XRPC_FEATURE_XRPC_DISPATCH
#define XRPC_FEATURE_XRPC_DISPATCH
#endif
#ifndef XRPC_MODULE_XRPC_DECODE
#define XRPC_MODULE_XRPC_DECODE
#endif
#ifndef XRPC_MODULE_XRPC_ENCODE
#define XRPC_MODULE_XRPC_ENCODE
#endif
#endif

/* xrpc_encode 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_ENCODE)
#ifndef XRPC_FEATURE_XRPC_ENCODE
#define XRPC_FEATURE_XRPC_ENCODE
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_JSON_ESCAPE
#define XRT_MODULE_JSON_ESCAPE
#endif
#endif

/* xrpc_decode 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_DECODE)
#ifndef XRPC_FEATURE_XRPC_DECODE
#define XRPC_FEATURE_XRPC_DECODE
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#endif

#endif /* XRPC_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/export.h */
/* ========================================================================== */

#ifndef XRPC_EXPORT_H
#define XRPC_EXPORT_H
#if defined(_WIN32) || defined(_WIN64)
 #if defined(XRPC_BUILD_SHARED)
  #define XRPC_API __declspec(dllexport)
 #elif defined(XRPC_USE_SHARED)
  #define XRPC_API __declspec(dllimport)
 #else
  #define XRPC_API
 #endif
#elif defined(__GNUC__) || defined(__clang__)
 #if defined(XRPC_BUILD_SHARED)
  #define XRPC_API __attribute__((visibility("default")))
 #else
  #define XRPC_API
 #endif
#else
 #define XRPC_API
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/types.h */
/* ========================================================================== */

#ifndef XRPC_TYPES_H
#define XRPC_TYPES_H

#if defined(XRPC_FEATURE_XRPC_DECODE) || defined(XRPC_FEATURE_XRPC_ENCODE)

#define XRPC_ERROR_PARSE (-32700)
#define XRPC_ERROR_INVALID_REQUEST (-32600)
#define XRPC_ERROR_METHOD_NOT_FOUND (-32601)
#define XRPC_ERROR_INVALID_PARAMS (-32602)
#define XRPC_ERROR_INTERNAL (-32603)
#define XRPC_METHOD_RESERVED 0x01u

typedef enum xrpcvaluekind {
	XRPC_VALUE_MISSING = 0, XRPC_VALUE_NULL, XRPC_VALUE_BOOLEAN,
	XRPC_VALUE_NUMBER, XRPC_VALUE_STRING, XRPC_VALUE_ARRAY, XRPC_VALUE_OBJECT
} xrpcvaluekind;

/* Raw is the exact JSON token, including string quotes/escapes. Numbers are
 * never converted, so large integers/exponents/fractions and -0 remain exact.
 * MISSING is distinct from explicit null. All views borrow the input. */
typedef struct xrpcvalue { xrpcvaluekind Kind; xstrview Raw; } xrpcvalue;

typedef enum xrpcmessagekind {
	XRPC_MESSAGE_INVALID = 0, XRPC_MESSAGE_CALL, XRPC_MESSAGE_NOTIFICATION,
	XRPC_MESSAGE_RESULT, XRPC_MESSAGE_ERROR
} xrpcmessagekind;

typedef enum xrpccodecerror {
	XRPC_CODEC_ARGUMENT = 1001, XRPC_CODEC_LIMIT, XRPC_CODEC_CAPACITY,
	XRPC_CODEC_VALUE
} xrpccodecerror;

#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/decode.h */
/* ========================================================================== */

#ifndef XRPC_DECODE_H
#define XRPC_DECODE_H

#if defined(XRPC_FEATURE_XRPC_DECODE) && !defined(XRT_FEATURE_JSON_READ)
#error "XRPC_FEATURE_XRPC_DECODE requires XRT_FEATURE_JSON_READ"
#endif

#if defined(XRPC_FEATURE_XRPC_DECODE)

typedef enum xrpcinvalidreason {
	XRPC_INVALID_NONE = 0, XRPC_INVALID_OBJECT, XRPC_INVALID_EMPTY_BATCH,
	XRPC_INVALID_DUPLICATE, XRPC_INVALID_VERSION, XRPC_INVALID_METHOD,
	XRPC_INVALID_ID, XRPC_INVALID_PARAMS, XRPC_INVALID_RESPONSE,
	XRPC_INVALID_ERROR, XRPC_INVALID_AMBIGUOUS
} xrpcinvalidreason;

typedef struct xrpcmessage {
	xrpcmessagekind Kind;
	xrpcinvalidreason Reason;
	uint32 Flags;
	xstrview Wire;
	xrpcvalue Id, Method, Params, Result, Error;
	xrpcvalue ErrorCode, ErrorMessage, ErrorData;
} xrpcmessage;

typedef enum xrpcdecodemode {
	XRPC_DECODE_REQUESTS = 1,
	XRPC_DECODE_RESPONSES,
	/* Classify by method presence; reject method plus result/error ambiguity. */
	XRPC_DECODE_DUPLEX
} xrpcdecodemode;

typedef struct xrpcdecodeconfig {
	size_t Size;
	xrpcdecodemode Mode;
	size_t MaxInputBytes;
	uint32 MaxDepth;
	size_t MaxStringBytes, MaxValues, MaxContainerItems, MaxMessages;
	/* Exact token bytes, including quotes; separate from decoded string cap. */
	size_t MaxIdBytes, MaxMethodBytes;
} xrpcdecodeconfig;

typedef struct xrpcdocument {
	/* True only for a nonempty input batch. [] produces one invalid message
	 * with Batch=false: its error response must be a single object. */
	bool Batch;
	size_t Count;
	size_t InvalidCount;
} xrpcdocument;

typedef enum xrpcdecodestatus {
	XRPC_DECODE_LOCAL_ERROR = -1,
	XRPC_DECODE_PARSE_ERROR = 0,
	XRPC_DECODE_DONE = 1
} xrpcdecodestatus;

XRT_EXTERN_C_BEGIN

/* Defaults: duplex, input/string 1 MiB, depth 64, values/items 65536,
 * messages 256, id/method tokens 4096 bytes. All limits are finite/positive. */
XRPC_API bool xrpcDecodeConfigInit(xrpcdecodeconfig* Config);

/* Decode exactly one complete JSON document using the public XRT raw scanner.
 * Config is copied; NULL selects defaults. Synchronous/caller-thread, stateless,
 * no retained callbacks or input. Successful DONE performs no allocation or
 * payload copy and preserves ambient error. Views remain valid while Input
 * stays alive/unchanged. Method/ErrorMessage are JSON strings, not decoded UTF-8.
 * Messages is caller scratch (Capacity>0). Its contents are unspecified on
 * LOCAL_ERROR/PARSE_ERROR; do not publish or execute anything until DONE.
 * Document is unchanged on LOCAL_ERROR and zeroed on PARSE_ERROR. DONE publishes
 * Count, which can include INVALID items. Scalar roots, [] and invalid batch
 * elements are protocol errors, not malformed JSON. INVALID is not a valid
 * notification: server errors use null id. Valid notifications never get replies.
 * Request/response modes ignore opposite-role extra fields. Duplex rejects
 * ambiguous envelopes. Relevant envelope/error members reject semantic duplicate
 * names, including escaped aliases; unrelated payload keys are left to the app.
 * IDs accept string/number/null; params accept object/array; error.code accepts
 * any mathematically integral numeric lexeme without native narrowing.
 * PARSE_ERROR means malformed JSON (XRT diagnostic preserved). Resource/capacity,
 * API and allocation failures are LOCAL_ERROR, never wire Invalid Request.
 * Output spans cannot overlap each other or Input/config; checks require actual objects.
 * Cold diagnostics can allocate and fall back to XERR_MEMORY. Caller scratch
 * may be smaller than MaxMessages; an actual larger batch fails locally. */
XRPC_API xrpcdecodestatus xrpcDecode(xstrview Input, const xrpcdecodeconfig* Config,
	xrpcmessage* Messages, size_t Capacity, xrpcdocument* Document);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/encode.h */
/* ========================================================================== */

#ifndef XRPC_ENCODE_H
#define XRPC_ENCODE_H

#if defined(XRPC_FEATURE_XRPC_ENCODE) && \
	(!defined(XRT_FEATURE_JSON_READ) || !defined(XRT_FEATURE_JSON_ESCAPE))
#error "XRPC_FEATURE_XRPC_ENCODE requires JSON read and escape features"
#endif

#if defined(XRPC_FEATURE_XRPC_ENCODE)

typedef struct xrpcoutgoing {
	xrpcmessagekind Kind;
	/* Id/Params/Result/ErrorCode/ErrorData are raw JSON, validated and copied
	 * unchanged, including optional surrounding JSON whitespace. Size=0 means
	 * absent. A present null must be the token null. Id: string/number/null;
	 * Params: array/object; Result/Data: any JSON; ErrorCode: integral number. */
	xstrview Id, Method, Params, Result, ErrorCode, ErrorMessage, ErrorData;
	/* Method and ErrorMessage above are decoded UTF-8, quoted by the encoder;
	 * empty strings and embedded NUL are supported. Fields unused by Kind must
	 * have Size=0. CALL needs Id; NOTIFICATION forbids Id; RESULT/ERROR need Id.
	 * ERROR requires ErrorCode, RESULT requires Result. No implicit null/id. */
} xrpcoutgoing;

typedef struct xrpcencodeconfig {
	size_t Size;
	size_t MaxOutputBytes;
	uint32 MaxDepth;
	size_t MaxStringBytes, MaxValues, MaxContainerItems, MaxMessages;
	/* Id includes supplied whitespace; Method is the encoded quoted token. */
	size_t MaxIdBytes, MaxMethodBytes;
} xrpcencodeconfig;

XRT_EXTERN_C_BEGIN

/* Defaults: output/string 1 MiB, depth 64, values/items 65536, messages 256,
 * id/method tokens 4096 bytes. All limits finite/positive, depth<=256. Limits
 * apply to the complete output, including envelope keys/values and batch. */
XRPC_API bool xrpcEncodeConfigInit(xrpcencodeconfig* Config);

/* Nonempty vector, Count=1 for a non-batch object. No empty response batch:
 * callers with no responses send nothing. Batch=true retains array shape even
 * for one message. CALL/NOTIFICATION can coexist in request batches; response
 * batches contain RESULT/ERROR only. Mixing requests/responses is rejected.
 * Config is copied, NULL selects defaults. Inputs borrow caller memory; keep
 * all inputs alive and unchanged throughout the synchronous caller-thread call.
 * No callbacks retained, no success allocation or numeric narrowing. Raw JSON
 * is scanned once; decoded text uses public UTF-8 validation/quoting. Cold
 * diagnostics can allocate. Errors are local, never generated wire responses.
 * Success preserves ambient error. All failures leave outputs unchanged.
 * Size preflights all inputs; Required cannot overlap inputs/config/vector. */
XRPC_API bool xrpcEncodeSize(const xrpcoutgoing* Messages, size_t Count,
	bool Batch, const xrpcencodeconfig* Config, size_t* Required);

/* Preflight independently; no trust in a prior Size call. No terminating NUL.
 * Only measured wire bytes are modified. Their range and optional Written
 * cannot overlap each other or any input span/config/vector. Capacity must
 * describe a nonwrapping buffer range. Raw tokens copy once; quoted text is
 * validated/escaped once while sizing and again while writing. No raw rescan
 * during emission. Concurrent input mutation violates the input contract. */
XRPC_API bool xrpcEncodeWrite(ptr Output, size_t Capacity,
	const xrpcoutgoing* Messages, size_t Count, bool Batch,
	const xrpcencodeconfig* Config, size_t* Written);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/dispatch.h */
/* ========================================================================== */

#ifndef XRPC_DISPATCH_H
#define XRPC_DISPATCH_H

#if defined(XRPC_FEATURE_XRPC_DISPATCH) && \
	(!defined(XRPC_FEATURE_XRPC_DECODE) || !defined(XRPC_FEATURE_XRPC_ENCODE))
#error "XRPC_FEATURE_XRPC_DISPATCH requires decode and encode features"
#endif

#if defined(XRPC_FEATURE_XRPC_DISPATCH)

typedef struct xrpcreply xrpcreply;
typedef enum xrpcdispatcherror {
	XRPC_DISPATCH_ARGUMENT = 2001, XRPC_DISPATCH_STATE,
	XRPC_DISPATCH_LIMIT, XRPC_DISPATCH_SINK
} xrpcdispatcherror;

/* All callbacks execute synchronously on the xrpcDispatch caller thread.
 * Call returns false for an unknown method, true when handled. A handled call
 * without a reply produces Internal error. Notifications may omit a reply.
 * Request and Reply are borrowed only until Call returns. Reply functions
 * consume local/stack payloads before returning and must not be deferred.
 * Call can implement any method index; Method.Raw is a quoted JSON token.
 * Reserved rpc. methods never reach Call; negotiation is a later runtime API. */
typedef bool (*xrpcdispatchcall)(const xrpcmessage* Request, xrpcreply* Reply, ptr User);

/* Emit borrows a validated response and every view only until it returns.
 * EncodedBytes is the single-object xrpcEncodeWrite length. Batch describes
 * the final response document, even when only one reply survives. The sink
 * must copy/encode synchronously and enforce its own storage/backpressure.
 * Return false to stop dispatch; this is a local sink error, never a wire
 * Invalid Request. No further business handler runs after a sticky failure.
 * For batches, accumulate objects with commas; append the closing bracket only
 * after successful dispatch. Zero replies means send nothing, including no []. */
typedef bool (*xrpcdispatchemit)(const xrpcoutgoing* Response, bool Batch,
	size_t EncodedBytes, ptr User);

typedef struct xrpcdispatchconfig {
	size_t Size;
	xrpcdecodeconfig Decode; /* Mode must be REQUESTS. Whole-document limits. */
	xrpcencodeconfig Reply; /* Per response OBJECT limits; no enclosing batch. */
	size_t MaxReplies;
	/* Whole response document, including brackets and commas. Unknown handler
	 * output sizes cannot be admitted in advance; see partial-effects contract. */
	size_t MaxOutputBytes;
} xrpcdispatchconfig;

typedef struct xrpcdispatchinfo {
	size_t Size;
	bool Batch, ParseError;
	size_t Messages, Visited, Invoked, Notifications, Replies, OutputBytes;
} xrpcdispatchinfo;

XRT_EXTERN_C_BEGIN

/* Default request decoder and per-object encoder limits, 256 replies and
 * 1 MiB complete output. NULL config selects defaults. */
XRPC_API bool xrpcDispatchConfigInit(xrpcdispatchconfig* Config);

/* Full JSON validation precedes ALL user callbacks. Scratch is caller-owned
 * decode storage; Capacity>0. Local input/capacity/resource/diagnostic-OOM
 * failure invokes no callback and leaves Info unchanged (scratch unspecified).
 * Malformed JSON instead emits one Parse error/null id. Invalid items emit
 * Invalid Request/null id. Valid calls preserve Id.Raw exactly; valid
 * notifications never emit, including unknown methods/handler errors.
 * The actual reply count is admitted before callbacks. Input/config/scratch
 * and Info cannot overlap; keep input/scratch unchanged throughout callbacks.
 * Config is copied. No input/callback/user data is retained after return.
 * No thread, queue or connection is added. Ordinary valid-input dispatch does
 * not allocate. Protocol/error diagnostics and user callbacks can allocate.
 * Call may be NULL (all methods unknown); Emit must be non-NULL.
 * Info.Size must match. Once decoded/admitted, Info is published even on a
 * callback/reply/output-limit failure and counts only accepted Emit results.
 * A failed sink may already have side effects. Discard partial response bytes;
 * previously invoked business effects cannot be undone or blindly replayed.
 * Independent dispatches can reenter/run concurrently with independent scratch
 * and contexts. Callbacks must not block an I/O worker; this leaf does no
 * executor/async/deadline/transport work. Ordinary success restores the caller's
 * ambient error even if a handler changed it. Local failure retains its first
 * diagnostic despite callbacks clearing/replacing ambient error. */
XRPC_API bool xrpcDispatch(xstrview Input, const xrpcdispatchconfig* Config,
	xrpcmessage* Scratch, size_t Capacity, xrpcdispatchcall Call, ptr CallUser,
	xrpcdispatchemit Emit, ptr EmitUser, xrpcdispatchinfo* Info);

/* Exactly one reply attempt per handler. Duplicate/reentrant attempts and a
 * false Call return after a reply are sticky local state errors. A failed
 * attempt cannot be replaced/retried. On a notification these functions record
 * the attempt and succeed without inspecting payloads or invoking Emit.
 * RawResult is one JSON value. Code is an integral raw JSON number; Message is
 * decoded UTF-8, Data is an optional raw value. Non-notification output is
 * independently preflighted by the public encoder before Emit is invoked. */
XRPC_API bool xrpcReplyResult(xrpcreply* Reply, xstrview RawResult);
XRPC_API bool xrpcReplyError(xrpcreply* Reply, xstrview Code,
	xstrview Message, xstrview Data);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc.h */
/* ========================================================================== */

#ifndef XRPC_H
#define XRPC_H
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DECODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DECODE"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ENCODE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ENCODE"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DISPATCH)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DISPATCH"
#endif
#endif

#endif

#if defined(XRPC_IMPLEMENTATION) && !defined(XRPC_IMPLEMENTATION_ONCE)
#define XRPC_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xrpc/src/internal/xrpc_json.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_DECODE) || \
	defined(XRPC_FEATURE_XRPC_ENCODE)
#ifndef XRPC_INTERNAL_JSON_H
#define XRPC_INTERNAL_JSON_H

#if defined(XRPC_FEATURE_XRPC_DECODE) || defined(XRPC_FEATURE_XRPC_ENCODE)

/* The caller supplies a validated JSON number token, without whitespace.
 * Exact decimal integrality does not need native numeric conversion. Saturate
 * the exponent at token length: it already exceeds every possible mantissa. */
static bool xrpcJsonIntegral(xstrview Raw)
{
	size_t Position = 0, Fraction = 0, Zeros = 0, Exponent = 0;
	bool Dot = false, Nonzero = false, NegativeExponent = false;
	while (Position < Raw.Size && Raw.Data[Position] != 'e' && Raw.Data[Position] != 'E') {
		char Byte = Raw.Data[Position++];
		if (Byte == '.') Dot = true;
		else if (Byte != '-') {
			if (Dot) ++Fraction;
			if (Byte == '0') ++Zeros; else { Zeros = 0; Nonzero = true; }
		}
	}
	if (!Nonzero) return true;
	if (Position < Raw.Size) {
		++Position;
		if (Raw.Data[Position] == '+' || Raw.Data[Position] == '-') NegativeExponent = Raw.Data[Position++] == '-';
		while (Position < Raw.Size) {
			size_t Digit = (size_t)(Raw.Data[Position++] - '0');
			if (Digit > Raw.Size || Exponent > (Raw.Size - Digit) / 10u) Exponent = Raw.Size;
			else Exponent = Exponent * 10u + Digit;
		}
	}
	if (NegativeExponent) return Zeros >= Fraction && Zeros - Fraction >= Exponent;
	return Fraction <= Zeros || Exponent >= Fraction - Zeros;
}

#endif
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/decode/decode.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_DECODE)
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_DECODE)

enum { XRPC_F_VERSION = 1u, XRPC_F_ID = 2u, XRPC_F_METHOD = 4u,
	XRPC_F_PARAMS = 8u, XRPC_F_RESULT = 16u, XRPC_F_ERROR = 32u };

typedef struct xrpcdecodecontext {
	xrpcdecodeconfig Config;
	xrpcmessage* Messages;
	size_t Capacity, Count, InvalidCount, BaseDepth;
	bool RootSeen, Batch, CurrentObject, ErrorActive;
	uint32 Seen, Duplicate, ErrorSeen, ErrorDuplicate;
	bool VersionValid;
	xrpccodecerror Failure;
} xrpcdecodecontext;

static bool xrpcDecodeReport(xerrkind Kind, xrpccodecerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false;
}

XRPC_API bool xrpcDecodeConfigInit(xrpcdecodeconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode config output");
	*Config = (xrpcdecodeconfig){sizeof(*Config), XRPC_DECODE_DUPLEX, 1024u*1024u, 64,
		1024u*1024u, 65536, 65536, 256, 4096, 4096};
	return true;
}

/* Interpret an already-validated JSON string only to compare ASCII protocol
 * names/version/reserved prefix. This is not a second JSON grammar validator. */
static bool xrpcDecodeAscii(xstrview Raw, cstr Text, bool Prefix)
{
	size_t Position = 1, Index = 0;
	while (Text[Index] != 0) {
		if (Position >= Raw.Size - 1u) return false;
		unsigned Value = (unsigned char)Raw.Data[Position++];
		if (Value == '\\') {
			Value = (unsigned char)Raw.Data[Position++];
			if (Value == 'u') {
				Value = 0;
				for (unsigned Digit = 0; Digit < 4; ++Digit) {
					unsigned Byte = (unsigned char)Raw.Data[Position++];
					Value = Value * 16u + (Byte <= '9' ? Byte - '0' : (Byte <= 'F' ? Byte - 'A' + 10u : Byte - 'a' + 10u));
				}
			} else if (Value == 'b') Value = '\b';
			else if (Value == 'f') Value = '\f';
			else if (Value == 'n') Value = '\n';
			else if (Value == 'r') Value = '\r';
			else if (Value == 't') Value = '\t';
		}
		if (Value != (unsigned char)Text[Index++]) return false;
	}
	return Prefix || Position == Raw.Size - 1u;
}

static xrpcvaluekind xrpcDecodeValueKind(xjsonscantype Type)
{
	switch (Type) {
		case XJSON_SCAN_NULL: return XRPC_VALUE_NULL;
		case XJSON_SCAN_BOOL: return XRPC_VALUE_BOOLEAN;
		case XJSON_SCAN_NUMBER: return XRPC_VALUE_NUMBER;
		case XJSON_SCAN_STRING: return XRPC_VALUE_STRING;
		case XJSON_SCAN_ARRAY_BEGIN: case XJSON_SCAN_ARRAY_END: return XRPC_VALUE_ARRAY;
		case XJSON_SCAN_OBJECT_BEGIN: case XJSON_SCAN_OBJECT_END: return XRPC_VALUE_OBJECT;
		default: return XRPC_VALUE_MISSING;
	}
}

static bool xrpcDecodeEnd(xjsonscantype Type)
{
	return Type == XJSON_SCAN_ARRAY_END || Type == XJSON_SCAN_OBJECT_END;
}

static bool xrpcDecodeBegin(xjsonscantype Type)
{
	return Type == XJSON_SCAN_ARRAY_BEGIN || Type == XJSON_SCAN_OBJECT_BEGIN;
}

static uint32 xrpcDecodeField(xstrview Name)
{
	if (xrpcDecodeAscii(Name, "jsonrpc", false)) return XRPC_F_VERSION;
	if (xrpcDecodeAscii(Name, "id", false)) return XRPC_F_ID;
	if (xrpcDecodeAscii(Name, "method", false)) return XRPC_F_METHOD;
	if (xrpcDecodeAscii(Name, "params", false)) return XRPC_F_PARAMS;
	if (xrpcDecodeAscii(Name, "result", false)) return XRPC_F_RESULT;
	if (xrpcDecodeAscii(Name, "error", false)) return XRPC_F_ERROR;
	return 0;
}

static bool xrpcDecodeId(xrpcvaluekind Kind)
{
	return Kind == XRPC_VALUE_STRING || Kind == XRPC_VALUE_NUMBER || Kind == XRPC_VALUE_NULL;
}

static void xrpcDecodeClassify(xrpcdecodecontext* Context, xrpcmessage* Message)
{
	uint32 Relevant = XRPC_F_VERSION | XRPC_F_ID;
	bool Request = Context->Config.Mode == XRPC_DECODE_REQUESTS ||
		(Context->Config.Mode == XRPC_DECODE_DUPLEX && (Context->Seen & XRPC_F_METHOD));
	if (!Context->CurrentObject) { Message->Reason = XRPC_INVALID_OBJECT; return; }
	Relevant |= Request ? (XRPC_F_METHOD | XRPC_F_PARAMS) : (XRPC_F_RESULT | XRPC_F_ERROR);
	if (Context->Config.Mode == XRPC_DECODE_DUPLEX) Relevant = 63u;
	if (Context->Duplicate & Relevant) Message->Reason = XRPC_INVALID_DUPLICATE;
	else if (!Context->VersionValid) Message->Reason = XRPC_INVALID_VERSION;
	else if (Context->Config.Mode == XRPC_DECODE_DUPLEX && (Context->Seen & XRPC_F_METHOD) &&
		(Context->Seen & (XRPC_F_RESULT | XRPC_F_ERROR))) Message->Reason = XRPC_INVALID_AMBIGUOUS;
	else if (Message->Id.Kind != XRPC_VALUE_MISSING && !xrpcDecodeId(Message->Id.Kind)) Message->Reason = XRPC_INVALID_ID;
	else if (Request) {
		if (Message->Method.Kind != XRPC_VALUE_STRING) Message->Reason = XRPC_INVALID_METHOD;
		else if (Message->Params.Kind != XRPC_VALUE_MISSING && Message->Params.Kind != XRPC_VALUE_OBJECT &&
			Message->Params.Kind != XRPC_VALUE_ARRAY) Message->Reason = XRPC_INVALID_PARAMS;
		else {
			Message->Kind = Message->Id.Kind == XRPC_VALUE_MISSING ? XRPC_MESSAGE_NOTIFICATION : XRPC_MESSAGE_CALL;
			if (xrpcDecodeAscii(Message->Method.Raw, "rpc.", true)) Message->Flags |= XRPC_METHOD_RESERVED;
		}
	} else if (Message->Id.Kind == XRPC_VALUE_MISSING ||
		((Context->Seen & XRPC_F_RESULT) != 0) == ((Context->Seen & XRPC_F_ERROR) != 0)) Message->Reason = XRPC_INVALID_RESPONSE;
	else if (Context->Seen & XRPC_F_RESULT) Message->Kind = XRPC_MESSAGE_RESULT;
	else if (Message->Error.Kind != XRPC_VALUE_OBJECT || Context->ErrorDuplicate ||
		Message->ErrorCode.Kind != XRPC_VALUE_NUMBER || Message->ErrorMessage.Kind != XRPC_VALUE_STRING ||
		!xrpcJsonIntegral(Message->ErrorCode.Raw)) Message->Reason = XRPC_INVALID_ERROR;
	else Message->Kind = XRPC_MESSAGE_ERROR;
}

static xjsonvisitaction xrpcDecodeFail(xrpcdecodecontext* Context, xrpccodecerror Code, cstr Message)
{
	Context->Failure = Code;
	xrpcDecodeReport(XERR_RANGE, Code, Message);
	return XJSON_VISIT_FAIL;
}

static xjsonvisitaction xrpcDecodeEvent(const xjsonscanevent* Event, ptr User)
{
	xrpcdecodecontext* Context = (xrpcdecodecontext*)User;
	bool End = xrpcDecodeEnd(Event->Type), Begin = xrpcDecodeBegin(Event->Type);
	if (!Context->RootSeen) {
		Context->RootSeen = true; Context->Batch = Event->Type == XJSON_SCAN_ARRAY_BEGIN;
		Context->BaseDepth = Context->Batch ? 1u : 0u;
	}
	if (Context->Batch && Event->Depth == 0) return XJSON_VISIT_NEXT;
	if (Event->Depth == Context->BaseDepth && !End) {
		if (Context->Count >= Context->Config.MaxMessages) return xrpcDecodeFail(Context, XRPC_CODEC_LIMIT, "RPC batch message limit exceeded");
		if (Context->Count >= Context->Capacity) return xrpcDecodeFail(Context, XRPC_CODEC_CAPACITY, "RPC message scratch capacity exceeded");
		Context->Messages[Context->Count] = (xrpcmessage){0};
		Context->CurrentObject = Event->Type == XJSON_SCAN_OBJECT_BEGIN;
		Context->Seen = Context->Duplicate = Context->ErrorSeen = Context->ErrorDuplicate = 0;
		Context->VersionValid = Context->ErrorActive = false;
	}
	xrpcmessage* Message = &Context->Messages[Context->Count];
	if (Event->Depth == Context->BaseDepth) {
		if (Begin) return XJSON_VISIT_NEXT;
		Message->Wire = Event->Raw; xrpcDecodeClassify(Context, Message);
		if (Message->Kind == XRPC_MESSAGE_INVALID) ++Context->InvalidCount;
		++Context->Count; return XJSON_VISIT_NEXT;
	}
	if (!Context->CurrentObject || !Event->HasName) return XJSON_VISIT_NEXT;
	if (Event->Depth == Context->BaseDepth + 1u) {
		uint32 Field = xrpcDecodeField(Event->Name);
		if (Field == 0) return XJSON_VISIT_NEXT;
		if (!End) { Context->Duplicate |= Context->Seen & Field; Context->Seen |= Field; }
		if (Field == XRPC_F_VERSION) {
			if (!End) Context->VersionValid = Event->Type == XJSON_SCAN_STRING && xrpcDecodeAscii(Event->Raw, "2.0", false);
			return XJSON_VISIT_NEXT;
		}
		xrpcvalue* Value = Field == XRPC_F_ID ? &Message->Id : Field == XRPC_F_METHOD ? &Message->Method :
			Field == XRPC_F_PARAMS ? &Message->Params : Field == XRPC_F_RESULT ? &Message->Result : &Message->Error;
		*Value = (xrpcvalue){xrpcDecodeValueKind(Event->Type), Event->Raw};
		if (Field == XRPC_F_ERROR) {
			Context->ErrorActive = Event->Type == XJSON_SCAN_OBJECT_BEGIN;
			if (!End) { Context->ErrorSeen = Context->ErrorDuplicate = 0; Message->ErrorCode = Message->ErrorMessage = Message->ErrorData = (xrpcvalue){0}; }
		}
		if (!Begin && ((Field == XRPC_F_ID && Value->Raw.Size > Context->Config.MaxIdBytes) ||
			(Field == XRPC_F_METHOD && Context->Config.Mode != XRPC_DECODE_RESPONSES && Value->Raw.Size > Context->Config.MaxMethodBytes)))
			return xrpcDecodeFail(Context, XRPC_CODEC_LIMIT, "RPC id or method token limit exceeded");
	} else if (Context->ErrorActive && Event->Depth == Context->BaseDepth + 2u) {
		uint32 Field = xrpcDecodeAscii(Event->Name, "code", false) ? 1u : xrpcDecodeAscii(Event->Name, "message", false) ? 2u :
			xrpcDecodeAscii(Event->Name, "data", false) ? 4u : 0u;
		if (!Field) return XJSON_VISIT_NEXT;
		if (!End) { Context->ErrorDuplicate |= Context->ErrorSeen & Field; Context->ErrorSeen |= Field; }
		xrpcvalue* Value = Field == 1u ? &Message->ErrorCode : Field == 2u ? &Message->ErrorMessage : &Message->ErrorData;
		*Value = (xrpcvalue){xrpcDecodeValueKind(Event->Type), Event->Raw};
	}
	return XJSON_VISIT_NEXT;
}

static bool xrpcDecodeConfigValid(const xrpcdecodeconfig* Config)
{
	return Config->Size == sizeof(*Config) && Config->Mode >= XRPC_DECODE_REQUESTS && Config->Mode <= XRPC_DECODE_DUPLEX &&
		Config->MaxInputBytes && Config->MaxDepth && Config->MaxDepth <= XJSON_DEPTH_DEFAULT && Config->MaxStringBytes &&
		Config->MaxValues && Config->MaxContainerItems && Config->MaxMessages && Config->MaxIdBytes && Config->MaxMethodBytes;
}

XRPC_API xrpcdecodestatus xrpcDecode(xstrview Input, const xrpcdecodeconfig* Config,
	xrpcmessage* Messages, size_t Capacity, xrpcdocument* Document)
{
	xrpcdecodecontext Context = {0};
	size_t Storage;
	if (Messages == NULL || Document == NULL || Capacity == 0 || Capacity > SIZE_MAX / sizeof(*Messages) ||
		!xrtMemRangeValid(Messages, Capacity * sizeof(*Messages)) || !xrtMemRangeValid(Document, sizeof(*Document)) ||
		(Input.Data == NULL && Input.Size != 0) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		(Config != NULL && !xrtMemRangeValid(Config, sizeof(*Config)))) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode input or output span"); return XRPC_DECODE_LOCAL_ERROR;
	}
	Storage = Capacity * sizeof(*Messages);
	if (xrtMemRangesOverlap(Messages, Storage, Document, sizeof(*Document)) || xrtMemRangesOverlap(Messages, Storage, Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Document, sizeof(*Document), Input.Data, Input.Size) ||
		(Config != NULL && (xrtMemRangesOverlap(Config, sizeof(*Config), Messages, Storage) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Document, sizeof(*Document))))) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC decode inputs and outputs overlap"); return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Config == NULL) xrpcDecodeConfigInit(&Context.Config); else Context.Config = *Config;
	if (!xrpcDecodeConfigValid(&Context.Config)) {
		xrpcDecodeReport(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC decode configuration"); return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Input.Size > Context.Config.MaxInputBytes) {
		xrpcDecodeReport(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC input exceeds configured limit"); return XRPC_DECODE_LOCAL_ERROR;
	}
	Context.Messages = Messages; Context.Capacity = Capacity;
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
	Json.MaxInputBytes = Context.Config.MaxInputBytes; Json.MaxDepth = Context.Config.MaxDepth;
	Json.MaxStringBytes = Context.Config.MaxStringBytes; Json.MaxValues = Context.Config.MaxValues;
	Json.MaxContainerItems = Context.Config.MaxContainerItems;
	if (xrtJsonScan(Input, &Json, xrpcDecodeEvent, &Context) != XJSON_VISIT_DONE) {
		if (Context.Failure) return XRPC_DECODE_LOCAL_ERROR;
		const xerror* Error = xrtGetError();
		if (xrtErrorKind(Error) == XERR_MEMORY) return XRPC_DECODE_LOCAL_ERROR;
		if (xrtErrorDomain(Error) != NULL && strcmp(xrtErrorDomain(Error), "xrt.json") == 0 && xrtErrorCode(Error) == XJSON_ERROR_LIMIT) {
			xrpcDecodeReport(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC JSON resource limit exceeded"); return XRPC_DECODE_LOCAL_ERROR;
		}
		if (xrtErrorDomain(Error) != NULL && strcmp(xrtErrorDomain(Error), "xrt.json") == 0 && xrtErrorCode(Error) == XJSON_ERROR_SYNTAX) {
			*Document = (xrpcdocument){0}; return XRPC_DECODE_PARSE_ERROR;
		}
		return XRPC_DECODE_LOCAL_ERROR;
	}
	if (Context.Batch && Context.Count == 0) {
		Messages[0] = (xrpcmessage){0}; Messages[0].Reason = XRPC_INVALID_EMPTY_BATCH;
		/* Raw [] may contain whitespace. No business data is copied. */
		Messages[0].Wire = Input; Context.Count = Context.InvalidCount = 1; Context.Batch = false;
	}
	*Document = (xrpcdocument){Context.Batch, Context.Count, Context.InvalidCount};
	return XRPC_DECODE_DONE;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/encode/encode.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_ENCODE)
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_ENCODE)

typedef struct xrpcencodecontext {
	xrpcencodeconfig Config;
	unsigned char* Output;
	size_t Bytes, Values, EmbedDepth;
	unsigned Allowed;
	bool RootSeen, Integral;
} xrpcencodecontext;

enum { XRPC_ENCODE_ID = (1u << XRPC_VALUE_NULL) | (1u << XRPC_VALUE_NUMBER) | (1u << XRPC_VALUE_STRING),
	XRPC_ENCODE_PARAMS = (1u << XRPC_VALUE_ARRAY) | (1u << XRPC_VALUE_OBJECT), XRPC_ENCODE_ANY = 126u };

static bool xrpcEncodeFail(xerrkind Kind, xrpccodecerror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false;
}

XRPC_API bool xrpcEncodeConfigInit(xrpcencodeconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC encode config output");
	*Config = (xrpcencodeconfig){sizeof(*Config), 1024u*1024u, 64, 1024u*1024u,
		65536, 65536, 256, 4096, 4096};
	return true;
}

static bool xrpcEncodeValue(xrpcencodecontext* Context)
{
	if (Context->Values == Context->Config.MaxValues)
		return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC output value limit exceeded");
	++Context->Values; return true;
}

static bool xrpcEncodeContainer(xrpcencodecontext* Context, size_t Depth, size_t Items)
{
	if (Depth >= Context->Config.MaxDepth || Items > Context->Config.MaxContainerItems)
		return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC output container limit exceeded");
	return xrpcEncodeValue(Context);
}

static bool xrpcEncodeEmit(xbytesview Data, ptr User)
{
	xrpcencodecontext* Context = (xrpcencodecontext*)User;
	if (Data.Size > Context->Config.MaxOutputBytes - Context->Bytes)
		return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC output byte limit exceeded");
	if (Context->Output != NULL && Data.Size != 0) memcpy(Context->Output + Context->Bytes, Data.Data, Data.Size);
	Context->Bytes += Data.Size; return true;
}

static bool xrpcEncodeBytes(xrpcencodecontext* Context, xstrview Text)
{
	return xrpcEncodeEmit((xbytesview){(cbytes)Text.Data, Text.Size}, Context);
}

static xjsonvisitaction xrpcEncodeEvent(const xjsonscanevent* Event, ptr User)
{
	xrpcencodecontext* Context = (xrpcencodecontext*)User;
	bool End = Event->Type == XJSON_SCAN_ARRAY_END || Event->Type == XJSON_SCAN_OBJECT_END;
	bool Begin = Event->Type == XJSON_SCAN_ARRAY_BEGIN || Event->Type == XJSON_SCAN_OBJECT_BEGIN;
	if (End) return XJSON_VISIT_NEXT;
	if (!Context->RootSeen) {
		xrpcvaluekind Kind = Event->Type == XJSON_SCAN_NULL ? XRPC_VALUE_NULL : Event->Type == XJSON_SCAN_BOOL ? XRPC_VALUE_BOOLEAN :
			Event->Type == XJSON_SCAN_NUMBER ? XRPC_VALUE_NUMBER : Event->Type == XJSON_SCAN_STRING ? XRPC_VALUE_STRING :
			Event->Type == XJSON_SCAN_ARRAY_BEGIN ? XRPC_VALUE_ARRAY : XRPC_VALUE_OBJECT;
		Context->RootSeen = true;
		if (!(Context->Allowed & (1u << Kind)) || (Context->Integral && !xrpcJsonIntegral(Event->Raw))) {
			xrpcEncodeFail(XERR_VALUE, XRPC_CODEC_VALUE, "invalid RPC JSON field type or nonintegral error code"); return XJSON_VISIT_FAIL;
		}
	}
	if (Begin && (Context->EmbedDepth >= Context->Config.MaxDepth || Event->Depth >= Context->Config.MaxDepth - Context->EmbedDepth)) {
		xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC embedded JSON depth limit exceeded"); return XJSON_VISIT_FAIL;
	}
	return xrpcEncodeValue(Context) ? XJSON_VISIT_NEXT : XJSON_VISIT_FAIL;
}

static bool xrpcEncodeRaw(xrpcencodecontext* Context, xstrview Raw, unsigned Allowed, size_t Depth, bool Integral)
{
	if (Context->Output == NULL) {
		if (Raw.Size == 0) return xrpcEncodeFail(XERR_VALUE, XRPC_CODEC_VALUE, "required RPC JSON field is absent");
		if (Raw.Size > Context->Config.MaxOutputBytes - Context->Bytes)
			return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC JSON field exceeds remaining output bytes");
		xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
		Json.MaxInputBytes = Context->Config.MaxOutputBytes; Json.MaxDepth = Context->Config.MaxDepth;
		Json.MaxStringBytes = Context->Config.MaxStringBytes; Json.MaxValues = Context->Config.MaxValues;
		Json.MaxContainerItems = Context->Config.MaxContainerItems;
		Context->Allowed = Allowed; Context->Integral = Integral; Context->EmbedDepth = Depth; Context->RootSeen = false;
		if (xrtJsonScan(Raw, &Json, xrpcEncodeEvent, Context) != XJSON_VISIT_DONE) {
			const xerror* Error = xrtGetError(); cstr Domain = xrtErrorDomain(Error);
			if (Domain != NULL && strcmp(Domain, "xrt.json") == 0 && xrtErrorCode(Error) == XJSON_ERROR_LIMIT)
				return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC JSON resource limit exceeded");
			return false;
		}
	}
	return xrpcEncodeBytes(Context, Raw);
}

static bool xrpcEncodeText(xrpcencodecontext* Context, xstrview Text, bool Method)
{
	size_t Before = Context->Bytes;
	if (Context->Output == NULL) {
		if (Text.Size > Context->Config.MaxStringBytes || Text.Size > Context->Config.MaxOutputBytes - Context->Bytes)
			return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC string exceeds configured limit");
		if (!xrpcEncodeValue(Context)) return false;
	}
	if (!xrtJsonQuoteWrite(Text, 0, xrpcEncodeEmit, Context, NULL)) return false;
	if (Method && Context->Bytes - Before > Context->Config.MaxMethodBytes)
		return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC method token limit exceeded");
	return true;
}

/* Measurement and emission share the same token order/lengths. Only the first
 * pass scans raw values and counts resources. The second pass cannot fail for
 * unchanged validated inputs: quoting has no external callback or allocation. */
static bool xrpcEncodeRun(xrpcencodecontext* Context, const xrpcoutgoing* Messages, size_t Count, bool Batch)
{
	#define XRPC_ENCODE_TOKEN(Token) do { if (!xrpcEncodeBytes(Context, XRT_STR_LITERAL(Token))) return false; } while (0)
	bool Measure = Context->Output == NULL;
	size_t Depth = Batch ? 1u : 0u;
	if (Batch) {
		if (Measure && !xrpcEncodeContainer(Context, 0, Count)) return false;
		XRPC_ENCODE_TOKEN("[");
	}
	for (size_t Index = 0; Index < Count; ++Index) {
		const xrpcoutgoing* Message = &Messages[Index];
		bool Request = Message->Kind == XRPC_MESSAGE_CALL || Message->Kind == XRPC_MESSAGE_NOTIFICATION;
		if (Measure) {
			if (Message->Kind < XRPC_MESSAGE_CALL || Message->Kind > XRPC_MESSAGE_ERROR ||
				(Request != (Messages[0].Kind == XRPC_MESSAGE_CALL || Messages[0].Kind == XRPC_MESSAGE_NOTIFICATION)) ||
				(Request && (Message->Result.Size || Message->ErrorCode.Size || Message->ErrorMessage.Size || Message->ErrorData.Size)) ||
				(!Request && (Message->Method.Size || Message->Params.Size)) ||
				(Message->Kind == XRPC_MESSAGE_NOTIFICATION && Message->Id.Size) ||
				(Message->Kind == XRPC_MESSAGE_RESULT && (Message->ErrorCode.Size || Message->ErrorMessage.Size || Message->ErrorData.Size)) ||
				(Message->Kind == XRPC_MESSAGE_ERROR && Message->Result.Size))
				return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC outgoing kind, batch role or unused fields");
			size_t Items = Request ? (Message->Kind == XRPC_MESSAGE_CALL ? 3u : 2u) + (Message->Params.Size != 0) : 3u;
			if (!xrpcEncodeContainer(Context, Depth, Items) || !xrpcEncodeValue(Context)) return false; /* object + version */
			if (Message->Id.Size > Context->Config.MaxIdBytes)
				return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC id token limit exceeded");
		}
		if (Index) XRPC_ENCODE_TOKEN(",");
		XRPC_ENCODE_TOKEN("{\"jsonrpc\":\"2.0\"");
		if (Request) {
			XRPC_ENCODE_TOKEN(",\"method\":");
			if (!xrpcEncodeText(Context, Message->Method, true)) return false;
			if (Message->Params.Size) {
				XRPC_ENCODE_TOKEN(",\"params\":");
				if (!xrpcEncodeRaw(Context, Message->Params, XRPC_ENCODE_PARAMS, Depth+1u, false)) return false;
			}
		} else if (Message->Kind == XRPC_MESSAGE_RESULT) {
			XRPC_ENCODE_TOKEN(",\"result\":");
			if (!xrpcEncodeRaw(Context, Message->Result, XRPC_ENCODE_ANY, Depth+1u, false)) return false;
		} else {
			if (Measure && !xrpcEncodeContainer(Context, Depth+1u, 2u + (Message->ErrorData.Size != 0))) return false;
			XRPC_ENCODE_TOKEN(",\"error\":{\"code\":");
			if (!xrpcEncodeRaw(Context, Message->ErrorCode, 1u << XRPC_VALUE_NUMBER, Depth+2u, true)) return false;
			XRPC_ENCODE_TOKEN(",\"message\":");
			if (!xrpcEncodeText(Context, Message->ErrorMessage, false)) return false;
			if (Message->ErrorData.Size) {
				XRPC_ENCODE_TOKEN(",\"data\":");
				if (!xrpcEncodeRaw(Context, Message->ErrorData, XRPC_ENCODE_ANY, Depth+2u, false)) return false;
			}
			XRPC_ENCODE_TOKEN("}");
		}
		if (Message->Kind != XRPC_MESSAGE_NOTIFICATION) {
			XRPC_ENCODE_TOKEN(",\"id\":");
			if (!xrpcEncodeRaw(Context, Message->Id, XRPC_ENCODE_ID, Depth+1u, false)) return false;
		}
		XRPC_ENCODE_TOKEN("}");
	}
	if (Batch) XRPC_ENCODE_TOKEN("]");
	#undef XRPC_ENCODE_TOKEN
	return true;
}

static bool xrpcEncodeInputs(xrpcencodecontext* Context, const xrpcoutgoing* Messages, size_t Count, bool Batch, const xrpcencodeconfig* Config)
{
	if (Messages == NULL || Count == 0 || (!Batch && Count != 1) || Count > SIZE_MAX / sizeof(*Messages) ||
		!xrtMemRangeValid(Messages, Count * sizeof(*Messages)) || (Config != NULL && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC encode vector or config span");
	if (Config == NULL) xrpcEncodeConfigInit(&Context->Config); else Context->Config = *Config;
	const xrpcencodeconfig* Limits = &Context->Config;
	if (Limits->Size != sizeof(*Limits) || !Limits->MaxOutputBytes || !Limits->MaxDepth || Limits->MaxDepth > XJSON_DEPTH_DEFAULT ||
		!Limits->MaxStringBytes || !Limits->MaxValues || !Limits->MaxContainerItems || !Limits->MaxMessages || !Limits->MaxIdBytes || !Limits->MaxMethodBytes)
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC encode configuration");
	if (Count > Limits->MaxMessages || Limits->MaxStringBytes < 7u)
		return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_LIMIT, "RPC message or envelope-key string limit exceeded");
	for (size_t Index = 0; Index < Count; ++Index) {
		const xrpcoutgoing* Message = &Messages[Index];
		const xstrview Views[] = {Message->Id, Message->Method, Message->Params, Message->Result, Message->ErrorCode, Message->ErrorMessage, Message->ErrorData};
		for (size_t Item = 0; Item < sizeof(Views)/sizeof(Views[0]); ++Item)
			if ((Views[Item].Data == NULL && Views[Item].Size != 0) || !xrtMemRangeValid(Views[Item].Data, Views[Item].Size))
				return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC encode input span");
	}
	return true;
}

static bool xrpcEncodeOutputSafe(const void* Output, size_t Size, const xrpcoutgoing* Messages, size_t Count, const xrpcencodeconfig* Config)
{
	if (!xrtMemRangeValid(Output, Size) || xrtMemRangesOverlap(Output, Size, Messages, Count * sizeof(*Messages)) ||
		(Config != NULL && xrtMemRangesOverlap(Output, Size, Config, sizeof(*Config))))
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC output overlaps vector/config or has an invalid span");
	for (size_t Index = 0; Index < Count; ++Index) {
		const xrpcoutgoing* Message = &Messages[Index];
		const xstrview Views[] = {Message->Id, Message->Method, Message->Params, Message->Result, Message->ErrorCode, Message->ErrorMessage, Message->ErrorData};
		for (size_t Item = 0; Item < sizeof(Views)/sizeof(Views[0]); ++Item)
			if (xrtMemRangesOverlap(Output, Size, Views[Item].Data, Views[Item].Size))
				return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC output overlaps input data");
	}
	return true;
}

XRPC_API bool xrpcEncodeSize(const xrpcoutgoing* Messages, size_t Count, bool Batch, const xrpcencodeconfig* Config, size_t* Required)
{
	xrpcencodecontext Context = {0};
	if (Required == NULL) return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC encode size requires an output");
	if (!xrpcEncodeInputs(&Context, Messages, Count, Batch, Config) ||
		!xrpcEncodeOutputSafe(Required, sizeof(*Required), Messages, Count, Config) || !xrpcEncodeRun(&Context, Messages, Count, Batch)) return false;
	*Required = Context.Bytes; return true;
}

XRPC_API bool xrpcEncodeWrite(ptr Output, size_t Capacity, const xrpcoutgoing* Messages, size_t Count, bool Batch,
	const xrpcencodeconfig* Config, size_t* Written)
{
	xrpcencodecontext Context = {0};
	if (Output == NULL || !xrtMemRangeValid(Output, Capacity))
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "invalid RPC encode output span");
	if (!xrpcEncodeInputs(&Context, Messages, Count, Batch, Config) || !xrpcEncodeRun(&Context, Messages, Count, Batch)) return false;
	size_t Bytes = Context.Bytes;
	if (Capacity < Bytes) return xrpcEncodeFail(XERR_RANGE, XRPC_CODEC_CAPACITY, "RPC output capacity is too small");
	if (!xrpcEncodeOutputSafe(Output, Bytes, Messages, Count, Config)) return false;
	if (Written != NULL && (!xrpcEncodeOutputSafe(Written, sizeof(*Written), Messages, Count, Config) ||
		xrtMemRangesOverlap(Output, Bytes, Written, sizeof(*Written))))
		return xrpcEncodeFail(XERR_ARGUMENT, XRPC_CODEC_ARGUMENT, "RPC written-size output overlaps wire or inputs");
	Context.Output = (unsigned char*)Output; Context.Bytes = 0;
	/* All failure conditions were checked; immutable/nonoverlapping inputs make
	 * the identical emission pass infallible. No user callbacks run here. */
	(void)xrpcEncodeRun(&Context, Messages, Count, Batch);
	if (Written != NULL) *Written = Bytes;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/dispatch/dispatch.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_DISPATCH)

#if defined(XRPC_FEATURE_XRPC_DISPATCH)

typedef struct xrpcdispatchcontext {
	xrpcdispatchconfig Config;
	xrpcdispatchinfo Info;
	xrpcdispatchemit Emit;
	ptr EmitUser;
	xerror* Failure;
	bool Failed;
} xrpcdispatchcontext;

struct xrpcreply {
	xrpcdispatchcontext* Context;
	xstrview Id;
	bool Notification, Attempted, Active;
};

static bool xrpcDispatchReport(xerrkind Kind, xrpcdispatcherror Code, cstr Message)
{
	xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false;
}

static bool xrpcDispatchRemember(xrpcdispatchcontext* Context)
{
	if (!Context->Failed) {
		Context->Failed = true; Context->Failure = xrtTakeError();
		if (Context->Failure == NULL) {
			xrpcDispatchReport(XERR_INTERNAL, XRPC_DISPATCH_STATE, "RPC operation failed without a diagnostic");
			Context->Failure = xrtTakeError();
		}
	}
	/* Transfer the first owning edge into the stack context rather than making
	 * a successful error Ref the failure sentinel. Diagnostic publication can
	 * need another reference; failure state and final transfer do not depend on it. */
	xrtSetError(Context->Failure);
	return false;
}

static bool xrpcDispatchFail(xrpcdispatchcontext* Context, xerrkind Kind,
	xrpcdispatcherror Code, cstr Message)
{
	if (!Context->Failed) {
		xrpcDispatchReport(Kind, Code, Message); xrpcDispatchRemember(Context);
	} else xrtSetError(Context->Failure);
	return false;
}

XRPC_API bool xrpcDispatchConfigInit(xrpcdispatchconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC dispatch config output");
	xrpcdispatchconfig Value = {0}; Value.Size = sizeof(Value);
	xrpcDecodeConfigInit(&Value.Decode); Value.Decode.Mode = XRPC_DECODE_REQUESTS;
	xrpcEncodeConfigInit(&Value.Reply); Value.MaxReplies = 256; Value.MaxOutputBytes = 1024u*1024u;
	*Config = Value; return true;
}

static bool xrpcDispatchEmit(xrpcdispatchcontext* Context, const xrpcoutgoing* Response)
{
	if (Context->Failed) { xrtSetError(Context->Failure); return false; }
	size_t Bytes = 0;
	if (!xrpcEncodeSize(Response, 1, false, &Context->Config.Reply, &Bytes)) return xrpcDispatchRemember(Context);
	size_t Overhead = Context->Info.Batch ? (Context->Info.Replies ? 1u : 2u) : 0u;
	size_t Remaining = Context->Config.MaxOutputBytes - Context->Info.OutputBytes;
	if (Context->Info.Replies == Context->Config.MaxReplies || Overhead > Remaining || Bytes > Remaining - Overhead)
		return xrpcDispatchFail(Context, XERR_RANGE, XRPC_DISPATCH_LIMIT, "RPC aggregate response limit exceeded");
	if (!Context->Emit(Response, Context->Info.Batch, Bytes, Context->EmitUser))
		return xrpcDispatchFail(Context, XERR_IO, XRPC_DISPATCH_SINK, "RPC response sink rejected output");
	/* A sink can reenter the current Reply and trigger a sticky failure. Its
	 * outer successful emission still counts as accepted physical output. */
	++Context->Info.Replies; Context->Info.OutputBytes += Bytes + Overhead;
	if (Context->Failed) { xrtSetError(Context->Failure); return false; }
	return true;
}

static bool xrpcDispatchReply(xrpcreply* Reply, xrpcoutgoing* Response)
{
	if (Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC reply handle");
	xrpcdispatchcontext* Context = Reply->Context;
	if (Context->Failed) { xrtSetError(Context->Failure); return false; }
	if (!Reply->Active || Reply->Attempted)
		return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_STATE, "RPC reply is inactive or already attempted");
	Reply->Attempted = true;
	if (Reply->Notification) return true;
	Response->Id = Reply->Id;
	return xrpcDispatchEmit(Context, Response);
}

XRPC_API bool xrpcReplyResult(xrpcreply* Reply, xstrview RawResult)
{
	xrpcoutgoing Response = {0}; Response.Kind = XRPC_MESSAGE_RESULT; Response.Result = RawResult;
	return xrpcDispatchReply(Reply, &Response);
}

XRPC_API bool xrpcReplyError(xrpcreply* Reply, xstrview Code, xstrview Message, xstrview Data)
{
	xrpcoutgoing Response = {0}; Response.Kind = XRPC_MESSAGE_ERROR;
	Response.ErrorCode = Code; Response.ErrorMessage = Message; Response.ErrorData = Data;
	return xrpcDispatchReply(Reply, &Response);
}

static bool xrpcDispatchStandard(xrpcdispatchcontext* Context, xstrview Id, int32 Code)
{
	xrpcoutgoing Response = {0}; Response.Kind = XRPC_MESSAGE_ERROR; Response.Id = Id;
	if (Code == XRPC_ERROR_PARSE) {
		Response.ErrorCode = XRT_STR_LITERAL("-32700"); Response.ErrorMessage = XRT_STR_LITERAL("Parse error");
	} else if (Code == XRPC_ERROR_INVALID_REQUEST) {
		Response.ErrorCode = XRT_STR_LITERAL("-32600"); Response.ErrorMessage = XRT_STR_LITERAL("Invalid Request");
	} else if (Code == XRPC_ERROR_METHOD_NOT_FOUND) {
		Response.ErrorCode = XRT_STR_LITERAL("-32601"); Response.ErrorMessage = XRT_STR_LITERAL("Method not found");
	} else {
		Response.ErrorCode = XRT_STR_LITERAL("-32603"); Response.ErrorMessage = XRT_STR_LITERAL("Internal error");
	}
	return xrpcDispatchEmit(Context, &Response);
}

static bool xrpcDispatchInputs(xrpcdispatchcontext* Context, xstrview Input,
	const xrpcdispatchconfig* Config, xrpcmessage* Scratch, size_t Capacity,
	xrpcdispatchemit Emit, xrpcdispatchinfo* Info)
{
	if (Emit == NULL || Info == NULL || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
		Scratch == NULL || !Capacity || Capacity > SIZE_MAX / sizeof(*Scratch) ||
		!xrtMemRangeValid(Scratch, Capacity * sizeof(*Scratch)) ||
		(Input.Data == NULL && Input.Size) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		(Config != NULL && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC dispatch input/output span");
	size_t Storage = Capacity * sizeof(*Scratch);
	if (xrtMemRangesOverlap(Info, sizeof(*Info), Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Info, sizeof(*Info), Scratch, Storage) ||
		xrtMemRangesOverlap(Scratch, Storage, Input.Data, Input.Size) ||
		(Config != NULL && (xrtMemRangesOverlap(Config, sizeof(*Config), Info, sizeof(*Info)) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Scratch, Storage) ||
		 xrtMemRangesOverlap(Config, sizeof(*Config), Input.Data, Input.Size))))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "RPC dispatch inputs/outputs overlap");
	if (Config == NULL) xrpcDispatchConfigInit(&Context->Config); else Context->Config = *Config;
	const xrpcdispatchconfig* Limits = &Context->Config;
	const xrpcencodeconfig* Reply = &Limits->Reply;
	if (Limits->Size != sizeof(*Limits) || !Limits->MaxReplies || !Limits->MaxOutputBytes ||
		Limits->Decode.Mode != XRPC_DECODE_REQUESTS || Reply->Size != sizeof(*Reply) ||
		!Reply->MaxOutputBytes || !Reply->MaxDepth || Reply->MaxDepth > XJSON_DEPTH_DEFAULT ||
		!Reply->MaxStringBytes || !Reply->MaxValues || !Reply->MaxContainerItems ||
		!Reply->MaxMessages || !Reply->MaxIdBytes || !Reply->MaxMethodBytes)
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC dispatch configuration");
	Context->Info.Size = sizeof(Context->Info); return true;
}

XRPC_API bool xrpcDispatch(xstrview Input, const xrpcdispatchconfig* Config,
	xrpcmessage* Scratch, size_t Capacity, xrpcdispatchcall Call, ptr CallUser,
	xrpcdispatchemit Emit, ptr EmitUser, xrpcdispatchinfo* Info)
{
	xrpcdispatchcontext Context = {0};
	if (!xrpcDispatchInputs(&Context, Input, Config, Scratch, Capacity, Emit, Info)) return false;
	Context.Emit = Emit; Context.EmitUser = EmitUser;
	xerror* Ambient = xrtTakeError(); xrpcdocument Document = {0};
	xrpcdecodestatus Decoded = xrpcDecode(Input, &Context.Config.Decode, Scratch, Capacity, &Document);
	if (Decoded == XRPC_DECODE_LOCAL_ERROR) { xrtErrorFree(Ambient); return false; }
	if (Decoded == XRPC_DECODE_PARSE_ERROR) {
		Context.Info.ParseError = true;
		xrpcDispatchStandard(&Context, XRT_STR_LITERAL("null"), XRPC_ERROR_PARSE);
	} else {
		size_t Responses = 0;
		for (size_t Index = 0; Index < Document.Count; ++Index)
			if (Scratch[Index].Kind != XRPC_MESSAGE_NOTIFICATION) ++Responses;
		if (Responses > Context.Config.MaxReplies) {
			xrtErrorFree(Ambient);
			return xrpcDispatchReport(XERR_RANGE, XRPC_DISPATCH_LIMIT, "RPC reply count exceeds admission limit");
		}
		Context.Info.Batch = Document.Batch; Context.Info.Messages = Document.Count;
		for (size_t Index = 0; Index < Document.Count && !Context.Failed; ++Index) {
			const xrpcmessage* Request = &Scratch[Index];
			if (Request->Kind == XRPC_MESSAGE_INVALID) {
				xrpcDispatchStandard(&Context, XRT_STR_LITERAL("null"), XRPC_ERROR_INVALID_REQUEST);
			} else {
				bool Notification = Request->Kind == XRPC_MESSAGE_NOTIFICATION;
				if (Notification) ++Context.Info.Notifications;
				xrpcreply Reply = {&Context, Request->Id.Raw, Notification, false, true};
				bool Handled = false;
				if (Call != NULL && !(Request->Flags & XRPC_METHOD_RESERVED)) {
					++Context.Info.Invoked; Handled = Call(Request, &Reply, CallUser);
				}
				Reply.Active = false;
				if (!Context.Failed) {
					if (!Handled && Reply.Attempted)
						xrpcDispatchFail(&Context, XERR_STATE, XRPC_DISPATCH_STATE, "RPC method rejected after attempting a reply");
					else if (!Notification && !Reply.Attempted)
						xrpcDispatchStandard(&Context, Request->Id.Raw, Handled ? XRPC_ERROR_INTERNAL : XRPC_ERROR_METHOD_NOT_FOUND);
				}
			}
			if (!Context.Failed) ++Context.Info.Visited;
		}
	}
	*Info = Context.Info;
	if (Context.Failed) {
		xrtSetErrorTake(Context.Failure); xrtErrorFree(Ambient); return false;
	}
	xrtSetErrorTake(Ambient); return true;
}
#endif
#endif

#endif
