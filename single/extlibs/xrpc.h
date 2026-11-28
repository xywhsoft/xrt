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
#ifndef XRPC_MODULE_XRPC_METHODS
#define XRPC_MODULE_XRPC_METHODS
#endif
#ifndef XRPC_MODULE_XRPC_ID
#define XRPC_MODULE_XRPC_ID
#endif
#ifndef XRPC_MODULE_XRPC_PENDING
#define XRPC_MODULE_XRPC_PENDING
#endif
#ifndef XRPC_MODULE_XRPC_RESPONSE
#define XRPC_MODULE_XRPC_RESPONSE
#endif
#ifndef XRPC_MODULE_XRPC_RECEIVE
#define XRPC_MODULE_XRPC_RECEIVE
#endif
#ifndef XRPC_MODULE_XRPC_CALL
#define XRPC_MODULE_XRPC_CALL
#endif
#ifndef XRPC_MODULE_XRPC_DUPLEX
#define XRPC_MODULE_XRPC_DUPLEX
#endif
#endif

/* xrpc_duplex 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_DUPLEX)
#ifndef XRPC_FEATURE_XRPC_DUPLEX
#define XRPC_FEATURE_XRPC_DUPLEX
#endif
#ifndef XRPC_MODULE_XRPC_CALL
#define XRPC_MODULE_XRPC_CALL
#endif
#ifndef XRPC_MODULE_XRPC_PENDING
#define XRPC_MODULE_XRPC_PENDING
#endif
#ifndef XRPC_MODULE_XRPC_ENCODE
#define XRPC_MODULE_XRPC_ENCODE
#endif
#ifndef XRPC_MODULE_XRPC_RESULT_QUOTA
#define XRPC_MODULE_XRPC_RESULT_QUOTA
#endif
#endif

/* xrpc_call 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_CALL)
#ifndef XRPC_FEATURE_XRPC_CALL
#define XRPC_FEATURE_XRPC_CALL
#endif
#ifndef XRPC_MODULE_XRPC_RESPONSE
#define XRPC_MODULE_XRPC_RESPONSE
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#ifndef XRT_MODULE_TIME
#define XRT_MODULE_TIME
#endif
#endif

/* xrpc_receive 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_RECEIVE)
#ifndef XRPC_FEATURE_XRPC_RECEIVE
#define XRPC_FEATURE_XRPC_RECEIVE
#endif
#ifndef XRPC_MODULE_XRPC_RESPONSE
#define XRPC_MODULE_XRPC_RESPONSE
#endif
#ifndef XRPC_MODULE_XRPC_PENDING
#define XRPC_MODULE_XRPC_PENDING
#endif
#endif

/* xrpc_pending 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_PENDING)
#ifndef XRPC_FEATURE_XRPC_PENDING
#define XRPC_FEATURE_XRPC_PENDING
#endif
#ifndef XRPC_MODULE_XRPC_ID
#define XRPC_MODULE_XRPC_ID
#endif
#endif

/* xrpc_id 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_ID)
#ifndef XRPC_FEATURE_XRPC_ID
#define XRPC_FEATURE_XRPC_ID
#endif
#ifndef XRT_MODULE_JSON_READ
#define XRT_MODULE_JSON_READ
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* xrpc_methods 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_METHODS)
#ifndef XRPC_FEATURE_XRPC_METHODS
#define XRPC_FEATURE_XRPC_METHODS
#endif
#ifndef XRPC_MODULE_XRPC_DISPATCH
#define XRPC_MODULE_XRPC_DISPATCH
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
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

/* xrpc_result_quota 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_RESULT_QUOTA)
#ifndef XRPC_FEATURE_XRPC_RESULT_QUOTA
#define XRPC_FEATURE_XRPC_RESULT_QUOTA
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRPC_MODULE_XRPC_RESPONSE
#define XRPC_MODULE_XRPC_RESPONSE
#endif
#endif

/* xrpc_response 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_RESPONSE)
#ifndef XRPC_FEATURE_XRPC_RESPONSE
#define XRPC_FEATURE_XRPC_RESPONSE
#endif
#ifndef XRPC_MODULE_XRPC_DECODE
#define XRPC_MODULE_XRPC_DECODE
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
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
/* public: extlibs/xrpc/include/xrpc/response.h */
/* ========================================================================== */

#ifndef XRPC_RESPONSE_H
#define XRPC_RESPONSE_H
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA) && \
	(!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRT_FEATURE_ATOMIC) || !defined(XRT_FEATURE_MUTEX))
#error "XRPC result quota requires response, atomic and mutex"
#endif
#if defined(XRPC_FEATURE_XRPC_RESPONSE) && \
	(!defined(XRPC_FEATURE_XRPC_DECODE) || !defined(XRT_FEATURE_ATOMIC))
#error "XRPC_FEATURE_XRPC_RESPONSE requires decode and atomic"
#endif
#if defined(XRPC_FEATURE_XRPC_RESPONSE)
typedef struct xrpcresponses xrpcresponses;
typedef struct xrpcresponseconfig {
	size_t Size, MaxOwnedBytes;
	xrpcdecodeconfig Decode;
} xrpcresponseconfig;
typedef struct xrpcresponseinfo {
	size_t Size, Count, OwnedBytes;
	bool Batch;
	xstrview Wire;
	const xrpcmessage* Messages;
	const xstrview* Ids;
} xrpcresponseinfo;
typedef enum xrpcresponsestatus {
	XRPC_RESPONSE_LOCAL_ERROR = -1, XRPC_RESPONSE_PARSE_ERROR = 0,
	XRPC_RESPONSE_INVALID, XRPC_RESPONSE_DONE
} xrpcresponsestatus;
typedef enum xrpcresponseerror {
	XRPC_RESPONSE_ARGUMENT = 6001, XRPC_RESPONSE_LIMIT,
	XRPC_RESPONSE_MEMORY, XRPC_RESPONSE_ENVELOPE
} xrpcresponseerror;
XRT_EXTERN_C_BEGIN
/* Defaults: response-mode decoder limits, 2 MiB requested owned-byte ceiling. */
XRPC_API bool xrpcResponseConfigInit(xrpcresponseconfig* Config);
/* Validate the COMPLETE document and require every item to be a result/error,
 * then allocate one immutable snapshot. No partial valid subset is published.
 * PARSE_ERROR is malformed JSON, INVALID is a valid JSON invalid response
 * envelope (including requests/notifications/scalars/empty batch). Diagnostic
 * OOM/resource/API/allocation failure is LOCAL_ERROR. No wire error generated.
 * Result stays unchanged on all failures; decoder Scratch is unspecified until
 * DONE. Config is copied; Decode.Mode must be RESPONSES. Scratch capacity >0.
 * Input/config/scratch/result must have valid nonoverlapping spans as in Decode.
 * One allocation holds header, actual-count descriptors, contiguous raw-ID views
 * and one complete input copy; all views rebase to that copy without reparsing.
 * MaxOwnedBytes counts requested bytes, excluding allocator headers/caches.
 * Input/scratch/config stay alive throughout; the caller must not change them
 * concurrently (the decoder itself writes scratch).
 * No input/user pointer, callbacks, locks, queues or threads are retained/added.
 * Success preserves ambient error; cold diagnostics may allocate. */
XRPC_API xrpcresponsestatus xrpcResponsesRead(xstrview Input, const xrpcresponseconfig* Config,
	xrpcmessage* Scratch, size_t Capacity, xrpcresponses** Result);
/* Strong refs permit concurrent immutable readers; one owner must safely
 * publish the initial reference. Retain needs an already-live strong ref and
 * fails on overflow. Final release follows physical reader completion. NULL
 * release OK. Metadata/views remain borrowed until the holding ref is released;
 * never mutate them, including by casting away const. No DOM/string unescaping,
 * narrowing or per-item allocation. Ids duplicates existing descriptor views
 * for bounded batch correlation without strided aliasing or another ID scan. */
XRPC_API bool xrpcResponsesRetain(xrpcresponses* Responses);
XRPC_API void xrpcResponsesRelease(xrpcresponses* Responses);
/* Info.Size must match; output cannot overlap owned storage. */
XRPC_API bool xrpcResponsesInfo(const xrpcresponses* Responses, xrpcresponseinfo* Info);
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
	XRPC_DISPATCH_LIMIT, XRPC_DISPATCH_SINK, XRPC_DISPATCH_ABORTED
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
 * ambient error even if a handler changed it, without notifying it again.
 * Existing failure diagnostics are also re-exposed quietly. A checked error-
 * slot entry failure invokes no callbacks and leaves Info unchanged. A slot
 * failure after admission returns false with actual partial effects in Info;
 * it cannot roll back business/output effects and may leave no diagnostic.
 * Dispatch never retries a known failed slot. Local failure retains its first
 * diagnostic despite callbacks clearing/replacing ambient error, provided the
 * error slot remains writable. User/diagnostic-creation setters retain their
 * own Core contracts; these checks cover dispatch's owning error exchanges. */
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

/* Stop this dispatch locally from an active Call/Emit callback, including a
 * notification or after an already accepted reply. No wire error is generated;
 * accepted output/business effects remain in Info. Returns false to propagate
 * failure conveniently. Error is a borrowed immutable XRT diagnostic, retained
 * before return; NULL creates a fresh XERR_STATE/XRPC_DISPATCH_ABORTED diagnostic
 * rather than using ambient error. A preexisting sticky failure wins. The
 * currently published Error transfers into the context without another Ref;
 * another borrowed Error needs a checked Ref. Diagnostic/reference failure
 * still aborts locally. The reply handle must not outlive its Call callback. */
XRPC_API bool xrpcReplyAbort(xrpcreply* Reply, const xerror* Error);

XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/methods.h */
/* ========================================================================== */

#ifndef XRPC_METHODS_H
#define XRPC_METHODS_H

#if defined(XRPC_FEATURE_XRPC_METHODS) && \
	(!defined(XRPC_FEATURE_XRPC_DISPATCH) || !defined(XRT_FEATURE_ATOMIC) || !defined(XRT_FEATURE_UNICODE))
#error "XRPC_FEATURE_XRPC_METHODS requires dispatch, atomic and unicode"
#endif

#if defined(XRPC_FEATURE_XRPC_METHODS)
typedef struct xrpcmethods xrpcmethods;
typedef struct xrpcmethod {
	/* Decoded, strict UTF-8; empty and embedded NUL names are supported. Names
	 * are case sensitive and are not normalized. rpc. is reserved/rejected. */
	xstrview Name;
	xrpcdispatchcall Call;
	/* Borrowed, not destroyed by the index. Keep callback code/context alive
	 * through all uses of this binding and all index references. */
	ptr User;
} xrpcmethod;

typedef struct xrpcmethodsconfig {
	size_t Size, MaxMethods, MaxNameBytes, MaxRawNameBytes, MaxTotalNameBytes, MaxOwnedBytes;
} xrpcmethodsconfig;
typedef struct xrpcmethodsinfo { size_t Size, Methods, NameBytes, OwnedBytes; } xrpcmethodsinfo;
typedef enum xrpcmethodserror {
	XRPC_METHODS_ARGUMENT = 3001, XRPC_METHODS_LIMIT, XRPC_METHODS_VALUE,
	XRPC_METHODS_DUPLICATE, XRPC_METHODS_MEMORY
} xrpcmethodserror;
typedef enum xrpcmethodsstatus {
	XRPC_METHODS_ERROR = -1, XRPC_METHODS_NOT_FOUND = 0, XRPC_METHODS_FOUND = 1
} xrpcmethodsstatus;

XRT_EXTERN_C_BEGIN
/* Defaults: 4096 methods, 4096 decoded bytes/name, 24578 raw JSON bytes/query,
 * 1 MiB copied names, 2 MiB requested index storage. All bounds positive. */
XRPC_API bool xrpcMethodsConfigInit(xrpcmethodsconfig* Config);

/* Validate then copy a vector into one immutable XRT allocation. Unsorted
 * source is accepted; duplicate decoded names and NULL callbacks are rejected.
 * NULL/zero vector creates an empty index. Config is copied. Names are copied
 * once with exact sizes and no NUL terminators. Callback/User values are copied
 * but code/context ownership remains with the caller. No user callback runs.
 * In-place heapsort has O(N log N) comparisons and no hidden scratch allocation.
 * Requested owned bytes include the header, entries and name payload, excluding
 * allocator headers/rounding and diagnostics. No growth allocation is used.
 * Result is unchanged on failure and cannot overlap any input/vector/config.
 * Success transfers one reference; caller inputs may then be changed/freed.
 * Cold diagnostic allocation can fall back to XERR_MEMORY. */
XRPC_API bool xrpcMethodsCreate(const xrpcmethod* Methods, size_t Count,
	const xrpcmethodsconfig* Config, xrpcmethods** Result);
XRPC_API bool xrpcMethodsRetain(xrpcmethods* Methods);
/* NULL is a no-op. Last release frees the single block without user code. */
XRPC_API void xrpcMethodsRelease(xrpcmethods* Methods);
XRPC_API bool xrpcMethodsInfo(const xrpcmethods* Methods, xrpcmethodsinfo* Info);

/* RawMethod is one complete JSON string, optionally surrounded by JSON
 * whitespace. It is independently validated even for a miss/empty index;
 * malformed JSON, wrong type, limits and diagnostic OOM are LOCAL errors, never
 * NOT_FOUND. Decoded byte equality handles all escapes and surrogate pairs;
 * no numeric/native-string conversion, case folding or normalization occurs.
 * A sorted (32-bit fingerprint, complete name) index keeps worst-case binary
 * search even if fingerprints collide; equality always checks the full name.
 * No successful lookup/miss allocation or decoded-name buffer is added. Raw
 * validation and semantic fingerprinting scan input; escaped collisions can
 * decode again during name comparisons. Successful paths preserve ambient error.
 * Binding changes only on FOUND; its Name borrows the immutable index, and
 * callback/User remain caller owned. Output cannot overlap input/index storage.
 * A live strong reference must cover this entire call and every borrowed view.
 * Concurrent Find/Info are safe; they add no index reference operation. Retain
 * needs an already-live reference/protected handoff, not a load/retain race. */
XRPC_API xrpcmethodsstatus xrpcMethodsFind(const xrpcmethods* Methods,
	xstrview RawMethod, xrpcmethod* Binding);

/* Direct xrpcDispatch Call adapter: pass Methods as CallUser. Hold a strong
 * index reference for the WHOLE dispatch, including all callbacks. This adapter
 * does not add per-item Retain/Release. It validates lookup, forwards a copied
 * callback/context on FOUND, returns false on a normal miss, and uses
 * xrpcReplyAbort for LOCAL lookup failure. Request/Reply obey the dispatch
 * callback scope; the index/code/context must not be destroyed from a callback
 * while that dispatch's owning reference is still required. */
XRPC_API bool xrpcMethodsCall(const xrpcmessage* Request, xrpcreply* Reply, ptr Methods);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/id.h */
/* ========================================================================== */

#ifndef XRPC_ID_H
#define XRPC_ID_H

#if defined(XRPC_FEATURE_XRPC_ID) && \
	(!defined(XRT_FEATURE_JSON_READ) || !defined(XRT_FEATURE_UNICODE))
#error "XRPC_FEATURE_XRPC_ID requires json_read and unicode"
#endif
#if defined(XRPC_FEATURE_XRPC_ID)
typedef struct xrpcidconfig {
	size_t Size, MaxInputBytes, MaxStringBytes, MaxKeyBytes;
} xrpcidconfig;
typedef enum xrpciderror {
	XRPC_ID_ARGUMENT = 4001, XRPC_ID_LIMIT, XRPC_ID_CAPACITY, XRPC_ID_VALUE
} xrpciderror;
XRT_EXTERN_C_BEGIN
/* Defaults: 4096 input/decoded string bytes and 8192 key bytes. All positive. */
XRPC_API bool xrpcIdConfigInit(xrpcidconfig* Config);

/* Independently validate one complete JSON string/number/null ID, optionally
 * surrounded by JSON whitespace, then measure its opaque binary comparison key.
 * Config is copied; NULL selects defaults. Missing ID, bool/array/object, malformed
 * JSON/UTF-8/surrogates, limits and diagnostic OOM are local failures. Required
 * changes only on success and cannot overlap Input/config.
 * Strings compare by decoded UTF-8 bytes, including NUL, without normalization;
 * numbers compare by EXACT decimal value, not native float/int conversion.
 * Decimal/exponent spellings 1/1.0/1e0 compare equal, as do all signed zeros.
 * Exponent arithmetic is decimal and bounded by token bytes, even for huge
 * positive/negative exponents. Number/string/null domains remain distinct.
 * Keys are opaque, non-JSON, not ordered numerically and not a persistence/wire
 * format. Within this implementation, compare complete byte lengths and bytes.
 * Keep the original raw ID separately for sending/echoing. This leaf owns no
 * pending calls and does not decide response eligibility/correlation outcomes.
 * Synchronous caller thread, no retained input/callbacks or ordinary valid-input
 * allocation, locks or threads. Cold diagnostics can allocate. Success preserves
 * ambient error; Input/config must remain alive and unchanged throughout. */
XRPC_API bool xrpcIdKeySize(xstrview Input, const xrpcidconfig* Config, size_t* Required);

/* Preflight independently; no trust in a prior Size. Failure leaves Output and
 * optional Written unchanged. Success writes exactly the key bytes, without a
 * NUL terminator. Capacity must describe a nonwrapping buffer; only the measured
 * written range is modified. Output/Written cannot overlap Input/config or each
 * other. The emission pass has no new failure point or user callback for valid,
 * unchanged inputs. Numeric exponent measurement and emission make bounded
 * linear passes; decoded string measurement/emission each traverse the token. */
XRPC_API bool xrpcIdKeyWrite(ptr Output, size_t Capacity, xstrview Input,
	const xrpcidconfig* Config, size_t* Written);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/pending.h */
/* ========================================================================== */

#ifndef XRPC_PENDING_H
#define XRPC_PENDING_H

#if defined(XRPC_FEATURE_XRPC_PENDING) && !defined(XRPC_FEATURE_XRPC_ID)
#error "XRPC_FEATURE_XRPC_PENDING requires id"
#endif
#if defined(XRPC_FEATURE_XRPC_PENDING)
typedef struct xrpcpending xrpcpending;
typedef struct xrpcpendingconfig {
	size_t Size, MaxCalls, MaxOwnedBytes;
	xrpcidconfig Id;
} xrpcpendingconfig;
typedef struct xrpcpendingtoken { uint64 Serial; size_t Slot; } xrpcpendingtoken;
typedef struct xrpcpendingcall {
	xrpcpendingtoken Token;
	uint64 Deadline;
	ptr User;
} xrpcpendingcall;
typedef enum xrpcpendingresolutionstatus {
	XRPC_PENDING_UNKNOWN = 0, XRPC_PENDING_MATCHED, XRPC_PENDING_REPEATED
} xrpcpendingresolutionstatus;
typedef struct xrpcpendingresolution {
	xrpcpendingresolutionstatus Status;
	xrpcpendingcall Call;
} xrpcpendingresolution;
typedef struct xrpcpendinginfo {
	size_t Size, Capacity, Count, OwnedBytes;
	uint64 NextDeadline;
} xrpcpendinginfo;
typedef enum xrpcpendingstatus {
	XRPC_PENDING_ERROR = -1, XRPC_PENDING_MISSING = 0, XRPC_PENDING_OK = 1,
	XRPC_PENDING_DUPLICATE, XRPC_PENDING_FULL
} xrpcpendingstatus;
typedef enum xrpcpendingerror {
	XRPC_PENDING_ARGUMENT = 5001, XRPC_PENDING_LIMIT, XRPC_PENDING_MEMORY
} xrpcpendingerror;
XRT_EXTERN_C_BEGIN
/* Defaults: 1024 calls, 2 MiB requested owned bytes; ID input 4096, decoded
 * string 256, key 512 bytes. All limits positive. No engine/transport dependency. */
XRPC_API bool xrpcPendingConfigInit(xrpcpendingconfig* Config);
/* One allocation for entries, fixed key slots, deadline heap and shared key
 * scratch; no per-operation ordinary allocation. MaxOwnedBytes includes these
 * requested bytes, not allocator headers/caches. Failure leaves Result unchanged.
 * Config is copied. Every operation, inspection and destruction on one table
 * MUST be serialized by its owner; no internal locking, callbacks or threads.
 * The runtime can confine this table to its connection/worker owner. */
XRPC_API bool xrpcPendingCreate(const xrpcpendingconfig* Config, xrpcpending** Result);
/* Does not destroy User pointers or complete calls. Drain with TakeFirst before
 * destroying when the owner needs to complete every outstanding call. NULL OK. */
XRPC_API void xrpcPendingDestroy(xrpcpending* Pending);
XRPC_API bool xrpcPendingInfo(const xrpcpending* Pending, xrpcpendinginfo* Info);
/* Independently validate/canonicalize Id, copy its comparison key and borrow
 * User (NULL allowed). No raw ID retained: the owner keeps original wire IDs.
 * Deadlines use one caller-chosen monotonic tick domain; UINT64_MAX means never.
 * Full/duplicate are ordinary outcomes, preserving outputs and ambient error;
 * duplicate wins over full. Invalid IDs/config/spans are local errors even full.
 * Success assigns a never-reused nonzero Serial and zero-based Slot. Tokens are
 * scoped to this table lifetime; serial exhaustion fails rather than wrapping.
 * Live IDs use exact xrpcIdKey equality, including null and signed numeric zero.
 * Input/output must not overlap each other or table-owned storage. */
XRPC_API xrpcpendingstatus xrpcPendingInsert(xrpcpending* Pending, xstrview Id,
	uint64 Deadline, ptr User, xrpcpendingtoken* Token);
/* All Take/Expire operations remove before publishing one record. Only OK
 * writes Call; missing/error leave it unchanged. They never run business code.
 * After removal, duplicate replies and stale local tokens are missing, including
 * a stale token after slot reuse. Concurrent response/cancel/expiry arbitration
 * belongs on the serialized owner. Call output cannot overlap table/input.
 * Reusing a removed wire ID CAN match an old response to a new call: tokens only
 * prevent local ABA. The runtime must use fresh wire IDs/connection epochs and
 * reject ineligible envelopes before calling TakeId. This leaf takes an ID,
 * not a response document; it cannot validate envelope role or batch atomicity. */
XRPC_API xrpcpendingstatus xrpcPendingTakeId(xrpcpending* Pending, xstrview Id,
	xrpcpendingcall* Call);
XRPC_API xrpcpendingstatus xrpcPendingTakeToken(xrpcpending* Pending,
	xrpcpendingtoken Token, xrpcpendingcall* Call);
/* Remove the earliest finite deadline <= Now; never-deadline entries stay live
 * even for Now=UINT64_MAX. Equal deadlines use admission serial order. */
XRPC_API xrpcpendingstatus xrpcPendingExpire(xrpcpending* Pending, uint64 Now,
	xrpcpendingcall* Call);
/* Remove earliest deadline, including never, for bounded shutdown/draining. */
XRPC_API xrpcpendingstatus xrpcPendingTakeFirst(xrpcpending* Pending, xrpcpendingcall* Call);
/* Preflight every ID/key limit and lookup before ANY removal. Then remove all
 * matches without another parser, allocation, diagnostic or callback. Count=0
 * permits NULL arrays. Resolutions is caller scratch for Count entries: contents
 * unspecified on false, so publish nothing before true. Pending calls remain
 * unchanged on false (private key scratch can change). IDs/array and output must
 * not overlap table storage or each other. Inputs remain unchanged throughout.
 * A live token's first occurrence is MATCHED; further equivalent IDs in this
 * batch are REPEATED. IDs absent at preflight are UNKNOWN, including previously
 * completed calls. Only MATCHED has a Call; other records are zeroed. User is
 * borrowed, never retained/freed. Every match is removed before return, so an
 * owner may safely complete them afterward. This does not validate response
 * envelopes, own payloads, read deadlines or make concurrent access safe. */
XRPC_API bool xrpcPendingTakeBatch(xrpcpending* Pending, const xstrview* Ids,
	size_t Count, xrpcpendingresolution* Resolutions);
/* Lookup uses a balanced comparison tree, not an attacker-controlled hash
 * probe chain: O(MaxKeyBytes * log(MaxCalls)); deadline heap O(log(MaxCalls)),
 * token slot check O(1) before removal. Mutations may recurse on AVL tree height.
 * Valid operations have no table allocation; JSON/error reference bookkeeping
 * can synchronize and cold diagnostics can allocate. Caller views remain alive
 * and unchanged throughout calls; no view/callback is retained. */
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/receive.h */
/* ========================================================================== */

#ifndef XRPC_RECEIVE_H
#define XRPC_RECEIVE_H
#if defined(XRPC_FEATURE_XRPC_RECEIVE) && \
	(!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRPC_FEATURE_XRPC_PENDING))
#error "XRPC_FEATURE_XRPC_RECEIVE requires response and pending"
#endif
#if defined(XRPC_FEATURE_XRPC_RECEIVE)
typedef struct xrpcreceiveinfo {
	size_t Size;
	bool Batch;
	size_t Count, Matched, Unknown, Repeated;
} xrpcreceiveinfo;
typedef enum xrpcreceiveerror { XRPC_RECEIVE_ARGUMENT = 7001, XRPC_RECEIVE_CAPACITY } xrpcreceiveerror;
XRT_EXTERN_C_BEGIN
/* Synchronous serialized-owner response admission. Validate the whole response
 * document, copy one immutable snapshot, then preflight every pending-table ID
 * and key limit before removing ANY call. No callback/queue/future completion.
 * On DONE, all matched calls have been removed before publishing Responses and
 * Info; resolutions are in response-item order. MATCHED wins once, subsequent
 * equivalent live IDs are REPEATED, preflight-missing IDs UNKNOWN. Only MATCHED
 * carries a borrowed Call/User. Snapshot retention does NOT retain User objects.
 * The owner completes matched calls after return, retaining the snapshot/index
 * for each asynchronous result as needed; copy/retain no borrowed input view.
 * Response status distinguishes local failure, malformed JSON and invalid
 * envelopes. Every failure leaves Pending calls, Responses and Info unchanged;
 * Scratch and Resolutions contents are unspecified and must be discarded.
 * All limits/allocations/ID parsing precede removal; commit does no failable work.
 * Capacity/ResolutionCapacity positive. Scratch/Resolutions/input/config/output/
 * Pending storage must be valid and mutually nonoverlapping. Info.Size must match.
 * Config is copied by the snapshot reader; NULL defaults. Keep all storage alive
 * and input unchanged; the caller must not mutate storage concurrently while the
 * decoder writes scratch and this function removes admitted calls. No reentrant
 * table access. Unknown-only documents still produce an owned snapshot.
 * Response eligibility, fresh wire IDs/epochs, expiry-event ordering, executor
 * completion and backpressure remain runtime policy. An explicitly pending null
 * ID matches null here; this does not distinguish an unsolicited null-ID error.
 * One snapshot allocation/copy; decoder and per-ID canonicalization are separate
 * scans, followed by bounded tree lookups/removals. Cold diagnostics allocate;
 * XRT reference/JSON bookkeeping can synchronize. Ordinary success preserves
 * ambient error. This is not a socket reader or a complete async client. */
XRPC_API xrpcresponsestatus xrpcReceive(xrpcpending* Pending, xstrview Input,
	const xrpcresponseconfig* Config, xrpcmessage* Scratch, size_t Capacity,
	xrpcpendingresolution* Resolutions, size_t ResolutionCapacity,
	xrpcresponses** Responses, xrpcreceiveinfo* Info);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/call.h */
/* ========================================================================== */

#ifndef XRPC_CALL_H
#define XRPC_CALL_H
#if defined(XRPC_FEATURE_XRPC_CALL) && \
	(!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRT_FEATURE_FUTURE))
#error "XRPC_FEATURE_XRPC_CALL requires response and future"
#endif
#if defined(XRPC_FEATURE_XRPC_CALL)
typedef struct xrpccall xrpccall;
typedef enum xrpccallstatus {
	XRPC_CALL_OK = 0, XRPC_CALL_REMOTE_ERROR, XRPC_CALL_TIMEOUT,
	XRPC_CALL_CANCELLED, XRPC_CALL_CLOSED, XRPC_CALL_TRANSPORT,
	XRPC_CALL_PROTOCOL, XRPC_CALL_LIMIT, XRPC_CALL_MEMORY,
	XRPC_CALL_NOTIFY_DONE, XRPC_CALL_BATCH_DONE, XRPC_CALL_HTTP_ERROR
} xrpccallstatus;
typedef enum xrpcdelivery {XRPC_NOT_SENT=0, XRPC_MAY_HAVE_EXECUTED, XRPC_REPLIED} xrpcdelivery;
typedef enum xrpccallfinish {
	XRPC_CALL_FINISH_ERROR=-1, XRPC_CALL_PENDING=0, XRPC_CALL_FINISHED, XRPC_CALL_ALREADY_DONE
} xrpccallfinish;
typedef enum xrpccallerror {XRPC_CALL_ARGUMENT=8001, XRPC_CALL_STATE, XRPC_CALL_ALLOCATION} xrpccallerror;
typedef struct xrpccallresult {
	xrpccallstatus Status;
	xrpcdelivery Delivery;
	xrpcresponses* Responses;
	size_t Index;
	const xerror* Cause;
} xrpccallresult;
/* A batch Future always owns this extended value, including failure/stop.
 * Call is the unchanged ordinary result prefix. Count is the input item count;
 * Indices maps input items to Responses.Messages, SIZE_MAX for notifications
 * or any non-BATCH_DONE outcome. BATCH_DONE can contain remote error items.
 * The immutable map is preallocated with this result, not a producer reference. */
typedef struct xrpcbatchcallresult {
	xrpccallresult Call;
	size_t Count;
	const size_t* Indices;
} xrpcbatchcallresult;
typedef struct xrpccallconfig {
	size_t Size;
	int64 Timeout; /* ms from Create entry, including construction/queue wait. */
	/* Optional nonblocking wake only; invoked on the cancelling thread at most
	 * once through xrtFutureCancel. It can arrive after logical completion while
	 * its captured producer reference remains live. Queued posts need their OWN
	 * captured references beyond Wake return. Raw token requests do not wake; the
	 * owner must explicitly signal/poll those. It must
	 * only post/signal owner work, never finish/destroy this call, wait for its
	 * owner, or execute business/language code inline. */
	void (*Wake)(ptr User);
	/* Mechanical context-reference release at FINAL producer-reference retirement,
	 * after captured cancellation tails. Destroy can return earlier. NULL means
	 * borrowed context; otherwise successful
	 * Create takes exactly one User reference, failure takes nothing. No wait or
	 * new semantic finalizer in Drop. Callback code lives through destruction. */
	void (*Drop)(ptr User);
	ptr User;
} xrpccallconfig;
XRT_EXTERN_C_BEGIN
/* Default 5000 ms, no context callbacks. Zero is immediately due; FOREVER OK. */
XRPC_API bool xrpcCallConfigInit(xrpccallconfig* Config);
/* A unique serialized-owner producer and an independent Future observer.
 * Result storage is preallocated; native producer binding adds no allocation.
 * No thread,
 * timer, queue, transport or additional shared runtime is created. All producer
 * access (including Destroy) must be serialized; observers use public XRT Future
 * APIs concurrently. Wake must obey its nonblocking/reentry contract. Config is
 * copied and outputs remain unchanged on failure; outputs/config cannot overlap.
 * Accepted ordinary terminal outcomes resolve with an immutable xrpccallresult,
 * including timeout/cancel/close/failure. XFUTURE_RESOLVED alone is not success.
 * Dropping an observer does not cancel accepted work. The owner must finish/poll
 * or destroy the producer; Destroy closes unfinished work and detaches its
 * Future producer binding without waiting for captured cancellation callbacks.
 * Future results own response/cause refs, NOT producer/context/engine refs, and
 * survive producer destruction. Borrow Value/Responses/Cause only while holding
 * the result Future; retain Responses for a longer-lived payload view.
 * This native producer and opaque context are not collector admission. */
XRPC_API bool xrpcCallCreate(const xrpccallconfig* Config, xrpccall** Call, xfuture** Future);
/* Same lifetime contract; 1..65536 items, result header plus Count size_t indices.
 * Ordinary call result/producer storage is not enlarged for batch-only data. */
XRPC_API bool xrpcBatchCallCreate(const xrpccallconfig* Config, size_t Count,
	xrpccall** Call, xfuture** Future);
/* NULL succeeds. Success consumes the unique owner right; a captured cancel
 * callback may still pin physical producer/context storage. False retains the
 * owner right for retry after a checked core publication refusal. */
XRPC_API bool xrpcCallDestroy(xrpccall* Call);
/* Owner records the FIRST acceptance of any request bytes by transport. This
 * is uncertainty, not proof of remote execution. False if already terminal. */
XRPC_API bool xrpcCallMarkSent(xrpccall* Call);
/* Borrowed producer-only access; deadline uses xrtTimer's monotonic seconds.
 * No timer is armed: a runtime schedules owner Poll at this absolute deadline. */
XRPC_API double xrpcCallDeadline(const xrpccall* Call);
XRPC_API bool xrpcCallCancellationRequested(const xrpccall* Call);
/* OK/REMOTE_ERROR require an already-correlated live snapshot/index of matching
 * RESULT/ERROR kind; all other statuses forbid a snapshot/nonzero index. Cause
 * is an explicit borrowed diagnostic, never inferred from ambient error. All
 * needed refs are checked BEFORE publication; ERROR leaves the call pending.
 * CANCEL/TIMEOUT conditions are sampled before publication; elapsed deadline
 * wins if both visible. A concurrent later request
 * may lose to completion (cancellation is cooperative). A validated response
 * records REPLIED knowledge even when timeout/cancel wins; stopped outcomes do
 * not retain its payload. No operation is replayed. Other outcomes retain prior
 * NOT_SENT/MAY_HAVE_EXECUTED knowledge from MarkSent.
 * All results are preallocated; ordinary completion with NULL Cause needs no
 * new heap allocation. Snapshot/error refs and diagnostics still have checked
 * failure paths. ALREADY_DONE does not inspect payloads. Future Watches execute
 * synchronously on publication: they must not block an I/O owner. Reentrant
 * terminal queries/Destroy from such a Watch are supported; never do so in Wake.
 * Success/ordinary ALREADY/PENDING preserve ambient error despite completion
 * Watches changing it. Producer storage remains internally pinned through
 * notification/release tails. Actual transport duties remain the runtime's. */
XRPC_API xrpccallfinish xrpcCallFinish(xrpccall* Call, xrpccallstatus Status,
	xrpcresponses* Responses, size_t Index, const xerror* Cause);
/* Complete an already-correlated full response batch. Count equals creation's
 * input count. Non-SIZE_MAX indices must cover every response exactly once;
 * the runtime verifies IDs and notification membership. Failure stays pending.
 * The index vector cannot overlap producer/result storage. BATCH_DONE publishes
 * the map; stop/failure results keep SIZE_MAX. This alone records REPLIED even
 * if a sampled stop wins. Ordinary Finish rejects BATCH_DONE/OK/REMOTE_ERROR on
 * batch producers. NOTIFY_DONE forbids a payload and requires MarkSent; it means
 * transport completion, never confirmation of notification business effects.
 * HTTP_ERROR is a local HTTP binding outcome with optional explicit Cause. */
XRPC_API xrpccallfinish xrpcBatchCallFinish(xrpccall* Call, xrpcresponses* Responses,
	const size_t* Indices, size_t Count, const xerror* Cause);
/* Publish due TIMEOUT or requested CANCELLED; otherwise ordinary PENDING. */
XRPC_API xrpccallfinish xrpcCallPoll(xrpccall* Call);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/duplex.h */
/* ========================================================================== */

#ifndef XRPC_DUPLEX_H
#define XRPC_DUPLEX_H
#if defined(XRPC_FEATURE_XRPC_DUPLEX) && (!defined(XRPC_FEATURE_XRPC_CALL) || \
 !defined(XRPC_FEATURE_XRPC_PENDING) || \
 !defined(XRPC_FEATURE_XRPC_ENCODE) || !defined(XRPC_FEATURE_XRPC_RESULT_QUOTA))
#error "xrpc duplex requires call, pending, encode and result quota"
#endif
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
typedef struct xrpcduplex xrpcduplex;
typedef struct xrpcduplexoutput xrpcduplexoutput;
typedef struct xrpcduplexitem {bool Notify;xstrview Method,Params;} xrpcduplexitem;
/* Called on the owner after request validation and reservation. Input and
 * Context are borrowed for this call; copy them before deferring. Document
 * and Messages are NULL for malformed JSON, which expects a Parse error.
 * Return false to reject without work; true transfers the job token to the
 * APP. Treat the token as opaque and keep it with its endpoint. Reply is
 * driven later by the owner, outside this callback. */
typedef bool (*xrpcduplexrequesthandler)(uint64 Token, xstrview Input,
    xbytesview Context, const xrpcdocument* Document, const xrpcmessage* Messages, ptr User);

typedef struct xrpcduplexconfig {
    size_t Size;
    xrpcencodeconfig Encode;
    xrpcresponseconfig Response;
    size_t MaxOperations,MaxPendingCalls,MaxIncoming,MaxControlBytes,MaxFixedBytes;
    size_t MaxOutputs,MaxOutputBytes,MaxResults,MaxResultBytes,MaxTemporaryBytes;
    size_t MaxWork;
    /* Signal/post ONLY, no waits, business or endpoint reentry. May run on any
     * submitting or cancelling thread. Context/code live through Destroy;
     * an asynchronous posted action needs its own lifetime discipline. */
    void (*Wake)(ptr User);
    ptr User;
    xrpcduplexrequesthandler OnRequest;
} xrpcduplexconfig;
typedef enum xrpcduplexadmission {
    XRPC_DUPLEX_ERROR=-1,XRPC_DUPLEX_ACCEPTED=0,XRPC_DUPLEX_OVERLOADED,
    XRPC_DUPLEX_CLOSED,XRPC_DUPLEX_PROTOCOL
} xrpcduplexadmission;
typedef enum xrpcduplexerror {
    XRPC_DUPLEX_ARGUMENT=18001,XRPC_DUPLEX_STATE,XRPC_DUPLEX_LIMIT,XRPC_DUPLEX_MEMORY
} xrpcduplexerror;
typedef struct xrpcduplexinfo {
    size_t Size;
    bool Closing,Drained,Selected,Started;
    xrpccallstatus Reason;
    size_t Operations,PendingCalls,Incoming,ControlBlocks,ControlBytes;
    /* Outgoing constructors are included in Operations and PendingCalls. */
    size_t Constructing,TemporaryBytes,PeakTemporaryBytes;
    size_t Outputs,OutputBytes,Results,ResultBytes,OwnedBytes;
    uint64 Accepted,Rejected,Completed,Received,UnknownResponses,ProtocolFailures;
    uint64 ControlRefusals,OutputRefusals,ResultRefusals;
    /* Absolute monotonic seconds. INFINITY means wait for Wake/input. The
     * earliest attached finite deadline is indexed. Step performs at most
     * MaxWork owner units; More requests another turn now. */
    double NextDeadline;
    bool More;
} xrpcduplexinfo;
XRT_EXTERN_C_BEGIN
/* 128 operations/1024 request IDs/64 incoming jobs; 2 MiB control metadata,
 * 256 outputs/8 MiB wire, 1024 results/8 MiB, 1 MiB aggregate concurrent encoding workspace,
 * 16 MiB endpoint/pending fixed storage and 64 owner units per Step. The APP
 * drives all progress. MaxIncoming slots count toward fixed storage
 * when OnRequest is set. NULL OnRequest selects a client-only endpoint. */
XRPC_API bool xrpcDuplexConfigInit(xrpcduplexconfig* Config);
XRPC_API bool xrpcDuplexCreate(const xrpcduplexconfig* Config,xrpcduplex** Result);
/* Concurrent bounded caller-thread admission. Inputs encoded/copied before
 * return; only ACCEPTED writes Future. Timeout starts at API entry, including
 * lock/validation/encoding. Only counter/range reservation and publication hold
 * the endpoint gate; encoding and native construction run outside it. Temporary
 * descriptor storage is an aggregate reservation across all constructors.
 * A reservation before a racing Close can still return ACCEPTED afterward;
 * owner then completes without sending. Construction failure rolls back all
 * credits but consumes its never-reused ID range. Cross-thread wire order is
 * construction publication order; same-thread sequential order is preserved.
 * Fresh string IDs never repeat in one endpoint.
 * Dropping a Future does not cancel. Use native FutureCancel from any thread.
 * Batch has 1..Encode.MaxMessages items; notification indices are SIZE_MAX.
 * Success Future holds native call/batch result, independent of the endpoint.
 * Cancellation and deadlines are local. A sent call can still execute remotely. */
XRPC_API xrpcduplexadmission xrpcDuplexCall(xrpcduplex* Duplex,xstrview Method,
    xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcDuplexNotify(xrpcduplex* Duplex,xstrview Method,
    xstrview Params,int64 Timeout,xfuture** Future);
XRPC_API xrpcduplexadmission xrpcDuplexBatch(xrpcduplex* Duplex,const xrpcduplexitem* Items,
    size_t Count,int64 Timeout,xfuture** Future);
/* Receive/Step/Next/Started/Sent/Info/Destroy require ONE serialized owner;
 * reject mutating reentry from synchronous completion Watches. Receive takes
 * exactly one whole transport message. It validates/classifies the complete
 * document before executing business/correlating any ID. Mixed request/response
 * documents, ambiguous or invalid responses close with PROTOCOL, without
 * partial matching. Malformed JSON/invalid requests go to the APP handler
 * for standard RPC errors. Request input/context are borrowed during the APP
 * handler. Outbound result snapshots copy once and retain independently.
 * OVERLOADED retains no input; the adapter applies bounded backpressure.
 * Ordinary call response shape is object; batch response is one complete array
 * in arbitrary order, omitting notifications. Unknown-only valid responses are
 * ignored/counted; duplicates/extra/missing IDs for a live group are protocol
 * failures. Replies cannot complete unsent or partially sent requests. */
XRPC_API xrpcduplexadmission xrpcDuplexReceive(xrpcduplex* Duplex,xstrview Input,xbytesview Context);
/* Complete an APP request token on the owner. Use the original response shape
 * and the exact raw IDs in request order, omitting notifications. A job with
 * no replies completes with an empty span. An accepted nonempty reply is
 * copied into the ordinary output queue. Error/overload retains the token and
 * publishes no bytes. Closing invalidates all outstanding APP tokens. */
XRPC_API xrpcduplexadmission xrpcDuplexReply(xrpcduplex* Duplex, uint64 Token, xstrview Reply);
/* Drive submitted work, due/cancelled calls and close. Call at NextDeadline
 * and after Wake or input. A deadline or cancellation during a partial send
 * closes the endpoint. The APP must then stop the transport. */
XRPC_API bool xrpcDuplexStep(xrpcduplex* Duplex);
/* 1 selects an immutable output and transfers one independent reference;
 * 0 empty (outputs unchanged); -1 API/local failure. One selection at a time.
 * Keep Output alive through the physical send borrow. Never send before Started
 * succeeds; token may become stale after Step. Started is the first send-offer
 * boundary, including framing headers, conservatively recording uncertainty
 * even if that offer is refused. Sent means COMPLETE message accepted.
 * Sent releases the endpoint's wire reference. Notification completion confirms
 * transport acceptance only. Selection tokens never wrap or repeat. */
XRPC_API int xrpcDuplexNext(xrpcduplex* Duplex,xrpcduplexoutput** Output,uint64* Token);
XRPC_API bool xrpcDuplexStarted(xrpcduplex* Duplex,uint64 Token);
XRPC_API bool xrpcDuplexSent(xrpcduplex* Duplex,uint64 Token);
/* Concurrent immutable output refs; may outlive the endpoint and transport.
 * Wire is borrowed through a holding reference. Physical retained wires continue
 * charging their independent quota. Final release never touches the endpoint. */
XRPC_API xstrview xrpcDuplexOutputWire(const xrpcduplexoutput* Output);
XRPC_API bool xrpcDuplexOutputRetain(xrpcduplexoutput* Output);
XRPC_API void xrpcDuplexOutputRelease(xrpcduplexoutput* Output);
/* Concurrent nonblocking close. Reason CLOSED/TRANSPORT/PROTOCOL/LIMIT/MEMORY/
 * TIMEOUT/HTTP_ERROR. TIMEOUT also represents an adapter's earlier whole-setup deadline;
 * individual call clocks do not get refreshed. Existing NOT_SENT/uncertain
 * delivery knowledge is preserved. Owner Step drives typed completion and
 * retires outstanding APP reply tokens. The APP manages its own work. */
XRPC_API bool xrpcDuplexClose(xrpcduplex* Duplex,xrpccallstatus Reason);
/* Owner-only snapshot; ledger/admission counters sampled under their own locks,
 * not a global atomic snapshot. Closing with only physical pins remaining has
 * a bounded retry deadline so a pre-retirement Wake cannot strand drain.
 * Drained proves no endpoint callback/context
 * duties; retained independent output/results do not delay it. */
XRPC_API bool xrpcDuplexInfo(xrpcduplex* Duplex,xrpcduplexinfo* Info);
/* NULL succeeds. Stop concurrent API entries first. Requires Close + Drained;
 * false retains ownership. Does not wait or drive Step. Result/wire refs survive. */
XRPC_API bool xrpcDuplexDestroy(xrpcduplex* Duplex);
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

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_METHODS)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_METHODS"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_ID)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_ID"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_PENDING)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_PENDING"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_RESPONSE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_RESPONSE"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_RECEIVE)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_RECEIVE"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_CALL)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_CALL"
#endif

#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_DUPLEX)
#error "XRPC_FEATURE_XRPC requires XRPC_FEATURE_XRPC_DUPLEX"
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
/* internal: extlibs/xrpc/src/internal/xrpc_result_quota.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_RESPONSE) || \
	defined(XRPC_FEATURE_XRPC_DUPLEX)
#ifndef XRPC_RESULT_QUOTA_H
#define XRPC_RESULT_QUOTA_H
typedef struct xrpcresultquota xrpcresultquota;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
/* Per-client storage ledger, not a runtime/engine owner. A charged allocation
 * owns one real ledger reference and refunds only after its physical free. */
bool xrpcResultQuotaCreate(size_t Blocks,size_t Bytes,xrpcresultquota** Result);
void xrpcResultQuotaRelease(xrpcresultquota* Quota);
void xrpcResultQuotaInfo(xrpcresultquota* Quota,size_t* Blocks,size_t* Bytes,uint64* Rejected);
ptr xrpcResultQuotaAlloc(xrpcresultquota* Quota,size_t Bytes,bool* Limited);
/* Charge retained external bytes as well as actual metadata allocation. */
bool xrpcResultQuotaFits(xrpcresultquota* Quota,size_t Charge);
ptr xrpcResultQuotaAllocCharged(xrpcresultquota* Quota,size_t Allocation,size_t Charge,bool* Limited);
void xrpcResultQuotaFree(ptr Value);
/* Decoded, when supplied, describes validated responses in Scratch from Input. */
xrpcresponsestatus xrpcResponsesReadQuota(xstrview Input,const xrpcresponseconfig* Config,
    xrpcmessage* Scratch,size_t Capacity,const xrpcdocument* Decoded,
	xrpcresponses** Result,xrpcresultquota* Quota);
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xrpc/src/internal/xrpc_string.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_METHODS) || \
	defined(XRPC_FEATURE_XRPC_ID) || \
	defined(XRPC_FEATURE_XRPC_DUPLEX)
#ifndef XRPC_INTERNAL_STRING_H
#define XRPC_INTERNAL_STRING_H

#if defined(XRPC_FEATURE_XRPC_METHODS) || defined(XRPC_FEATURE_XRPC_ID)
/* Interpret ONLY an already fully validated strict JSON string token. The
 * public raw scanner must reach DONE before any use. No grammar validation,
 * allocation or retained view is added here; scalar escapes use public Unicode. */
typedef struct xrpcstringiterator {
	xstrview Raw;
	size_t Position;
	char Pending[4];
	unsigned Read, Count;
} xrpcstringiterator;
static uint32 xrpcStringHex(xrpcstringiterator* It)
{
	uint32 Value = 0;
	for (unsigned Index = 0; Index < 4; ++Index) {
		unsigned Byte = (unsigned char)It->Raw.Data[It->Position++];
		Value = Value * 16u + (Byte <= '9' ? Byte-'0' : Byte <= 'F' ? Byte-'A'+10u : Byte-'a'+10u);
	}
	return Value;
}
static bool xrpcStringNext(xrpcstringiterator* It, unsigned char* Byte)
{
	if (It->Read < It->Count) { *Byte = (unsigned char)It->Pending[It->Read++]; return true; }
	if (It->Position == It->Raw.Size-1u) return false;
	unsigned char Value = (unsigned char)It->Raw.Data[It->Position++];
	if (Value != '\\') { *Byte = Value; return true; }
	Value = (unsigned char)It->Raw.Data[It->Position++];
	if (Value == 'u') {
		uint32 Scalar = xrpcStringHex(It);
		if (Scalar >= 0xd800u && Scalar <= 0xdbffu) {
			It->Position += 2; uint32 Low = xrpcStringHex(It);
			Scalar = 0x10000u + ((Scalar-0xd800u) << 10u) + Low-0xdc00u;
		}
		It->Count = (unsigned)xrtUtf8Encode(Scalar, It->Pending); It->Read = 1;
		*Byte = (unsigned char)It->Pending[0]; return true;
	}
	if (Value == 'b') Value = '\b'; else if (Value == 'f') Value = '\f';
	else if (Value == 'n') Value = '\n'; else if (Value == 'r') Value = '\r'; else if (Value == 't') Value = '\t';
	*Byte = Value; return true;
}
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xrpc/src/internal/xrpc_call_internal.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_CALL) || \
	defined(XRPC_FEATURE_XRPC_DUPLEX)
#ifndef XRPC_CALL_INTERNAL_H
#define XRPC_CALL_INTERNAL_H
#if defined(XRPC_FEATURE_XRPC_CALL)
/* Product-internal constructor shares an API-entry monotonic origin without
 * quantizing away pre-admission/encoding time. No new public ABI. */
bool xrpcCallCreateEntered(const xrpccallconfig* Config, xrpccall** Call,
    xfuture** Future, double Entered);
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config, size_t Count,
    xrpccall** Call, xfuture** Future, double Entered);
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
bool xrpcCallCreateQuota(const xrpccallconfig* Config,size_t Count,
    xrpccall** Call,xfuture** Future,double Entered,xrpcresultquota* Quota,bool* Limited);
#endif
#endif
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xrpc/src/internal/xrpc_pending_internal.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_DUPLEX)
#ifndef XRPC_PENDING_INTERNAL_H
#define XRPC_PENDING_INTERNAL_H
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
/* No removal. Duplicated live IDs remain MATCHED in this preflight. */
bool xrpcPendingProbeBatch(xrpcpending*,const xstrview*,size_t,xrpcpendingresolution*);
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
	const char* Text = Name.Data + 1;
	size_t Size = Name.Size - 2u;
	switch (Size) {
		case 2: return memcmp(Text, "id", 2) == 0 ? XRPC_F_ID : 0;
		case 5: return memcmp(Text, "error", 5) == 0 ? XRPC_F_ERROR : 0;
		case 6:
			if (memcmp(Text, "method", 6) == 0) return XRPC_F_METHOD;
			if (memcmp(Text, "params", 6) == 0) return XRPC_F_PARAMS;
			return memcmp(Text, "result", 6) == 0 ? XRPC_F_RESULT : 0;
		case 7:
			if (memcmp(Text, "jsonrpc", 7) == 0) return XRPC_F_VERSION;
			break;
	}
	/* Only \uXXXX can escape a letter. Each escape adds five bytes. */
	if (Size < 7 || Size > 42) return 0;
	switch (Size % 5) {
		case 0: return xrpcDecodeAscii(Name, "error", false) ? XRPC_F_ERROR : 0;
		case 1:
			if (xrpcDecodeAscii(Name, "method", false)) return XRPC_F_METHOD;
			if (xrpcDecodeAscii(Name, "params", false)) return XRPC_F_PARAMS;
			return xrpcDecodeAscii(Name, "result", false) ? XRPC_F_RESULT : 0;
		case 2:
			if (xrpcDecodeAscii(Name, "id", false)) return XRPC_F_ID;
			return xrpcDecodeAscii(Name, "jsonrpc", false) ? XRPC_F_VERSION : 0;
	}
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
/* source: extlibs/xrpc/src/response/response.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_RESPONSE)
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_RESPONSE)
struct xrpcresponses {
	xatomic32 References;
	bool Batch,QuotaOwned;
	size_t Count, OwnedBytes;
	xstrview Wire;
	xrpcmessage Messages[];
};
static bool xrpcResponseReport(xerrkind Kind,xrpcresponseerror Code,cstr Message)
{ xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false; }
XRPC_API bool xrpcResponseConfigInit(xrpcresponseconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))
		return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response config output");
	xrpcresponseconfig C={0};C.Size=sizeof(C);C.MaxOwnedBytes=2u*1024u*1024u;
	xrpcDecodeConfigInit(&C.Decode);C.Decode.Mode=XRPC_DECODE_RESPONSES;*Config=C;return true;
}
static xstrview xrpcResponseRebase(xstrview View,xstrview Input,cstr Copy)
{ if(View.Data)View.Data=Copy+(size_t)(View.Data-Input.Data);return View; }
static xrpcresponsestatus xrpcResponsesReadImpl(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,const xrpcdocument* Decoded,
	xrpcresponses** Result,xrpcresultquota* Quota)
{
	(void)Quota;
	if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || !Capacity || !Scratch || Capacity>SIZE_MAX/sizeof(*Scratch) ||
		!xrtMemRangeValid(Scratch,Capacity*sizeof(*Scratch)) || !xrtMemRangeValid(Input.Data,Input.Size) ||
		(Config && !xrtMemRangeValid(Config,sizeof(*Config))))goto Argument;
	if(xrtMemRangesOverlap(Result,sizeof(*Result),Input.Data,Input.Size) ||
		xrtMemRangesOverlap(Result,sizeof(*Result),Scratch,Capacity*sizeof(*Scratch)) || (Config &&
		(xrtMemRangesOverlap(Result,sizeof(*Result),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Scratch,Capacity*sizeof(*Scratch),Config,sizeof(*Config)))))goto Argument;
	xrpcresponseconfig C;if(Config)C=*Config;else xrpcResponseConfigInit(&C);
	if(C.Size!=sizeof(C) || !C.MaxOwnedBytes || C.Decode.Mode!=XRPC_DECODE_RESPONSES)goto Argument;
	xrpcdocument Document;
	if(Decoded)Document=*Decoded;
	else {
		xrpcdecodestatus Status=xrpcDecode(Input,&C.Decode,Scratch,Capacity,&Document);
		if(Status==XRPC_DECODE_LOCAL_ERROR)return XRPC_RESPONSE_LOCAL_ERROR;
		if(Status==XRPC_DECODE_PARSE_ERROR)return XRPC_RESPONSE_PARSE_ERROR;
	}
	if(Document.InvalidCount){xrpcResponseReport(XERR_VALUE,XRPC_RESPONSE_ENVELOPE,"invalid RPC response envelope/batch");
		return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_RESPONSE_LOCAL_ERROR:XRPC_RESPONSE_INVALID;}
	size_t Bytes=sizeof(xrpcresponses);
	if(Document.Count>(SIZE_MAX-Bytes)/(sizeof(xrpcmessage)+sizeof(xstrview)))goto Limit;
	Bytes+=Document.Count*(sizeof(xrpcmessage)+sizeof(xstrview));
	if(Input.Size>SIZE_MAX-Bytes || Bytes+Input.Size>C.MaxOwnedBytes)goto Limit;
	Bytes+=Input.Size;xrpcresponses* R=NULL;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(Quota){bool Limited=false;R=xrpcResultQuotaAlloc(Quota,Bytes,&Limited);if(Limited)goto Limit;}
	else
#endif
	R=xrtMalloc(Bytes);
	if(!R){xrpcResponseReport(XERR_MEMORY,XRPC_RESPONSE_MEMORY,"RPC response snapshot allocation failed");return XRPC_RESPONSE_LOCAL_ERROR;}
	xrtAtomic32Init(&R->References,1);R->Batch=Document.Batch;R->QuotaOwned=Quota!=NULL;R->Count=Document.Count;R->OwnedBytes=Bytes;
	xstrview* Ids=(xstrview*)(R->Messages+Document.Count);char* Wire=(char*)(Ids+Document.Count);
	memcpy(Wire,Input.Data,Input.Size);R->Wire=(xstrview){Wire,Input.Size};
	for(size_t Index=0;Index<Document.Count;++Index){xrpcmessage M=Scratch[Index];
		M.Wire=xrpcResponseRebase(M.Wire,Input,Wire);
		M.Id.Raw=xrpcResponseRebase(M.Id.Raw,Input,Wire);M.Method.Raw=xrpcResponseRebase(M.Method.Raw,Input,Wire);
		M.Params.Raw=xrpcResponseRebase(M.Params.Raw,Input,Wire);M.Result.Raw=xrpcResponseRebase(M.Result.Raw,Input,Wire);
		M.Error.Raw=xrpcResponseRebase(M.Error.Raw,Input,Wire);M.ErrorCode.Raw=xrpcResponseRebase(M.ErrorCode.Raw,Input,Wire);
		M.ErrorMessage.Raw=xrpcResponseRebase(M.ErrorMessage.Raw,Input,Wire);M.ErrorData.Raw=xrpcResponseRebase(M.ErrorData.Raw,Input,Wire);
		R->Messages[Index]=M;Ids[Index]=M.Id.Raw;
	}
	*Result=R;return XRPC_RESPONSE_DONE;
Argument:
	xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response config/output span/overlap");return XRPC_RESPONSE_LOCAL_ERROR;
Limit:
	xrpcResponseReport(XERR_RANGE,XRPC_RESPONSE_LIMIT,"RPC response owned-byte limit/size overflow");return XRPC_RESPONSE_LOCAL_ERROR;
}
XRPC_API xrpcresponsestatus xrpcResponsesRead(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result)
{return xrpcResponsesReadImpl(Input,Config,Scratch,Capacity,NULL,Result,NULL);}
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
xrpcresponsestatus xrpcResponsesReadQuota(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,const xrpcdocument* Decoded,
	xrpcresponses** Result,xrpcresultquota* Quota)
{return xrpcResponsesReadImpl(Input,Config,Scratch,Capacity,Decoded,Result,Quota);}
#endif
XRPC_API bool xrpcResponsesRetain(xrpcresponses* Responses)
{
	if(!Responses)return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"RPC responses are NULL");
	uint32 Count=xrtAtomic32Load(&Responses->References,XMEMORY_RELAXED);
	for(;;){if(Count==UINT32_MAX)return xrpcResponseReport(XERR_RANGE,XRPC_RESPONSE_LIMIT,"RPC response references overflow");
		if(xrtAtomic32CompareExchange(&Responses->References,&Count,Count+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XRPC_API void xrpcResponsesRelease(xrpcresponses* Responses)
{
	if(!Responses || xrtAtomic32FetchSub(&Responses->References,1,XMEMORY_ACQ_REL)!=1)return;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(Responses->QuotaOwned){xrpcResultQuotaFree(Responses);return;}
#endif
	xrtFree(Responses);
}
XRPC_API bool xrpcResponsesInfo(const xrpcresponses* Responses,xrpcresponseinfo* Info)
{
	if(!Responses || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) ||
		xrtMemRangesOverlap(Responses,Responses->OwnedBytes,Info,sizeof(*Info)) || Info->Size!=sizeof(*Info))
		return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"invalid RPC response info output");
	*Info=(xrpcresponseinfo){sizeof(*Info),Responses->Count,Responses->OwnedBytes,Responses->Batch,Responses->Wire,
		Responses->Messages,(const xstrview*)(Responses->Messages+Responses->Count)};return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/result_quota/result_quota.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
#include <stddef.h>
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
struct xrpcresultquota {
    xatomic32 References;
    xmutex Gate;
    size_t MaxBlocks,MaxBytes,Blocks,Bytes;
    uint64 Rejected;
};
typedef union xrpcresultallocation {
    struct {xrpcresultquota* Quota;size_t Bytes;} Meta;
    max_align_t Alignment;
} xrpcresultallocation;
bool xrpcResultQuotaCreate(size_t Blocks,size_t Bytes,xrpcresultquota** Result)
{
    xrpcresultquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q)return false;
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);return false;}
    xrtAtomic32Init(&Q->References,1);Q->MaxBlocks=Blocks;Q->MaxBytes=Bytes;*Result=Q;return true;
}
void xrpcResultQuotaRelease(xrpcresultquota* Q)
{if(Q && xrtAtomic32FetchSub(&Q->References,1,XMEMORY_ACQ_REL)==1){xrtMutexUnit(&Q->Gate);xrtFree(Q);}}
void xrpcResultQuotaInfo(xrpcresultquota* Q,size_t* Blocks,size_t* Bytes,uint64* Rejected)
{xrtMutexLock(&Q->Gate);*Blocks=Q->Blocks;*Bytes=Q->Bytes;*Rejected=Q->Rejected;xrtMutexUnlock(&Q->Gate);}
bool xrpcResultQuotaFits(xrpcresultquota* Q,size_t Charge)
{return Charge<=SIZE_MAX-sizeof(xrpcresultallocation) && Q->MaxBlocks && Charge+sizeof(xrpcresultallocation)<=Q->MaxBytes;}
ptr xrpcResultQuotaAllocCharged(xrpcresultquota* Q,size_t Allocation,size_t Bytes,bool* Limited)
{
    *Limited=false;
    if(Allocation>Bytes || Bytes>SIZE_MAX-sizeof(xrpcresultallocation)){xrtMutexLock(&Q->Gate);++Q->Rejected;xrtMutexUnlock(&Q->Gate);*Limited=true;return NULL;}
    Bytes+=sizeof(xrpcresultallocation);xrtMutexLock(&Q->Gate);
    bool OK=Q->Blocks<Q->MaxBlocks && Bytes<=Q->MaxBytes-Q->Bytes;
    if(OK){++Q->Blocks;Q->Bytes+=Bytes;xrtAtomic32FetchAdd(&Q->References,1,XMEMORY_RELAXED);}
    else ++Q->Rejected;
    xrtMutexUnlock(&Q->Gate);if(!OK){*Limited=true;return NULL;}
    xrpcresultallocation* A=xrtMalloc(Allocation+sizeof(xrpcresultallocation));
    if(!A){xrtMutexLock(&Q->Gate);--Q->Blocks;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcResultQuotaRelease(Q);return NULL;}
    A->Meta.Quota=Q;A->Meta.Bytes=Bytes;return A+1;
}
ptr xrpcResultQuotaAlloc(xrpcresultquota* Q,size_t Bytes,bool* Limited)
{return xrpcResultQuotaAllocCharged(Q,Bytes,Bytes,Limited);}
void xrpcResultQuotaFree(ptr Value)
{
    if(!Value)return;
    xrpcresultallocation* A=(xrpcresultallocation*)Value-1;xrpcresultquota* Q=A->Meta.Quota;size_t Bytes=A->Meta.Bytes;
    xrtFree(A);xrtMutexLock(&Q->Gate);--Q->Blocks;Q->Bytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcResultQuotaRelease(Q);
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
	bool Failed, ErrorSlotFailed;
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

/* Re-exposing an already reported diagnostic must neither notify again nor
 * lose the context's owning edge when a retain or native slot write fails. */
static bool xrpcDispatchExpose(xrpcdispatchcontext* Context)
{
	if (!Context->ErrorSlotFailed && Context->Failure != NULL) {
		xerror* Owned = xrtErrorRef(Context->Failure);
		if (Owned != NULL) {
			xerror* Previous = NULL;
			if (!xrtTrySwapErrorTake(Owned, &Previous)) {
				Context->ErrorSlotFailed = true; xrtErrorFree(Owned);
			} else xrtErrorFree(Previous);
		}
	}
	return false;
}

static bool xrpcDispatchRemember(xrpcdispatchcontext* Context)
{
	if (!Context->Failed) {
		Context->Failed = true;
		if (!xrtTrySwapErrorTake(NULL, &Context->Failure)) {
			Context->ErrorSlotFailed = true; return false;
		}
		if (Context->Failure == NULL) {
			xrpcDispatchReport(XERR_INTERNAL, XRPC_DISPATCH_STATE, "RPC operation failed without a diagnostic");
			if (!xrtTrySwapErrorTake(NULL, &Context->Failure)) {
				Context->ErrorSlotFailed = true; return false;
			}
		}
	}
	/* Transfer the first owning edge into the stack context rather than making
	 * a successful error Ref the failure sentinel. Diagnostic publication can
	 * need another reference; failure state and final transfer do not depend on it. */
	return xrpcDispatchExpose(Context);
}

static bool xrpcDispatchFail(xrpcdispatchcontext* Context, xerrkind Kind,
	xrpcdispatcherror Code, cstr Message)
{
	if (!Context->Failed) {
		xrpcDispatchReport(Kind, Code, Message); xrpcDispatchRemember(Context);
	} else xrpcDispatchExpose(Context);
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
	if (Context->Failed) return xrpcDispatchExpose(Context);
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
	if (Context->Failed) return xrpcDispatchExpose(Context);
	return true;
}

static bool xrpcDispatchReply(xrpcreply* Reply, xrpcoutgoing* Response)
{
	if (Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC reply handle");
	xrpcdispatchcontext* Context = Reply->Context;
	if (Context->Failed) return xrpcDispatchExpose(Context);
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

XRPC_API bool xrpcReplyAbort(xrpcreply* Reply, const xerror* Error)
{
	if (Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC abort handle");
	xrpcdispatchcontext* Context = Reply->Context;
	if (Context->Failed) return xrpcDispatchExpose(Context);
	if (!Reply->Active)
		return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_STATE, "RPC abort handle is inactive");
	if (Error == NULL)
		return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_ABORTED, "RPC handler aborted dispatch locally");
	if (Error != xrtGetError()) {
		xerror* Owned = xrtErrorRef(Error);
		if (Owned == NULL)
			return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_ABORTED, "RPC abort diagnostic cannot be retained");
		/* An explicit borrowed error can be retained directly in the context;
		 * installing it only to take it back would add an unnecessary slot write. */
		Context->Failed = true; Context->Failure = Owned;
		return xrpcDispatchExpose(Context);
	}
	return xrpcDispatchRemember(Context);
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
	xerror* Ambient = NULL;
	if (!xrtTrySwapErrorTake(NULL, &Ambient)) return false;
	xrpcdocument Document = {0};
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
		xerror* Previous = NULL;
		if (Context.ErrorSlotFailed || !xrtTrySwapErrorTake(Context.Failure, &Previous))
			xrtErrorFree(Context.Failure);
		xrtErrorFree(Previous); xrtErrorFree(Ambient); return false;
	}
	xerror* Previous = NULL;
	bool Restored = xrtTrySwapErrorTake(Ambient, &Previous);
	if (!Restored) xrtErrorFree(Ambient);
	xrtErrorFree(Previous); return Restored;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/methods/methods.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_METHODS)
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_METHODS)
typedef struct xrpcmethodsentry { uint32 Hash; xrpcmethod Binding; } xrpcmethodsentry;
struct xrpcmethods {
	xatomic32 References;
	xrpcmethodsconfig Config;
	size_t Count, NameBytes, OwnedBytes;
	xrpcmethodsentry Entries[];
};

static bool xrpcMethodsReport(xerrkind Kind, xrpcmethodserror Code, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false; }

XRPC_API bool xrpcMethodsConfigInit(xrpcmethodsconfig* Config)
{
	if (Config == NULL || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method config output");
	*Config = (xrpcmethodsconfig){sizeof(*Config),4096,4096,24578,1024u*1024u,2u*1024u*1024u}; return true;
}

static uint32 xrpcMethodsHash(xstrview Name)
{
	uint32 Hash = 2166136261u;
	for (size_t Index = 0; Index < Name.Size; ++Index) Hash = (Hash ^ (unsigned char)Name.Data[Index]) * 16777619u;
	return Hash;
}
static int xrpcMethodsCompareNames(xstrview A, xstrview B)
{
	size_t Common = A.Size < B.Size ? A.Size : B.Size;
	int Order = Common ? memcmp(A.Data, B.Data, Common) : 0;
	return Order ? Order : (A.Size > B.Size) - (A.Size < B.Size);
}
static int xrpcMethodsCompareEntries(const xrpcmethodsentry* A, const xrpcmethodsentry* B)
{
	return A->Hash != B->Hash ? (A->Hash > B->Hash ? 1 : -1) : xrpcMethodsCompareNames(A->Binding.Name, B->Binding.Name);
}
static void xrpcMethodsSift(xrpcmethodsentry* Entries, size_t Count, size_t Root)
{
	while (Root < Count / 2u) {
		size_t Child = Root * 2u + 1u;
		if (Child + 1u < Count && xrpcMethodsCompareEntries(&Entries[Child], &Entries[Child+1u]) < 0) ++Child;
		if (xrpcMethodsCompareEntries(&Entries[Root], &Entries[Child]) >= 0) break;
		xrpcmethodsentry Temp = Entries[Root]; Entries[Root] = Entries[Child]; Entries[Child] = Temp; Root = Child;
	}
}
static void xrpcMethodsSort(xrpcmethodsentry* Entries, size_t Count)
{
	for (size_t Root = Count / 2u; Root; --Root) xrpcMethodsSift(Entries, Count, Root-1u);
	for (size_t End = Count; End > 1; --End) {
		xrpcmethodsentry Temp = Entries[0]; Entries[0] = Entries[End-1u]; Entries[End-1u] = Temp;
		xrpcMethodsSift(Entries, End-1u, 0);
	}
}

XRPC_API bool xrpcMethodsCreate(const xrpcmethod* Source, size_t Count,
	const xrpcmethodsconfig* Config, xrpcmethods** Result)
{
	if (Result == NULL || !xrtMemRangeValid(Result, sizeof(*Result)) ||
		(Count && Source == NULL) || Count > SIZE_MAX / sizeof(*Source) ||
		!xrtMemRangeValid(Source, Count * sizeof(*Source)) ||
		(Config && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method constructor span");
	if (xrtMemRangesOverlap(Result, sizeof(*Result), Source, Count * sizeof(*Source)) ||
		(Config && xrtMemRangesOverlap(Result, sizeof(*Result), Config, sizeof(*Config))))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method output overlaps input");
	xrpcmethodsconfig Limits;
	if (Config) Limits = *Config; else xrpcMethodsConfigInit(&Limits);
	if (Limits.Size != sizeof(Limits) || !Limits.MaxMethods || !Limits.MaxNameBytes || !Limits.MaxRawNameBytes ||
		!Limits.MaxTotalNameBytes || !Limits.MaxOwnedBytes)
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method configuration");
	if (Count > Limits.MaxMethods || Count > (SIZE_MAX-sizeof(xrpcmethods))/sizeof(xrpcmethodsentry))
		return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method count limit exceeded");
	size_t Names = 0, Bytes = sizeof(xrpcmethods) + Count * sizeof(xrpcmethodsentry);
	for (size_t Index = 0; Index < Count; ++Index) {
		xstrview Name = Source[Index].Name;
		if (!Source[Index].Call || (!Name.Data && Name.Size) || !xrtMemRangeValid(Name.Data, Name.Size) ||
			xrtMemRangesOverlap(Result, sizeof(*Result), Name.Data, Name.Size))
			return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method binding/name/output overlap");
		if (Name.Size > Limits.MaxNameBytes || Name.Size > Limits.MaxTotalNameBytes-Names || Name.Size > SIZE_MAX-Bytes)
			return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method name/storage limit exceeded");
		if (!xrtUtf8Valid(Name, NULL)) return false;
		if (Name.Size >= 4 && memcmp(Name.Data, "rpc.", 4) == 0)
			return xrpcMethodsReport(XERR_VALUE, XRPC_METHODS_VALUE, "reserved RPC method cannot be registered");
		Names += Name.Size; Bytes += Name.Size;
	}
	if (Bytes > Limits.MaxOwnedBytes)
		return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method owned-byte limit exceeded");
	xrpcmethods* Methods = xrtMalloc(Bytes);
	if (!Methods) return xrpcMethodsReport(XERR_MEMORY, XRPC_METHODS_MEMORY, "RPC method allocation failed");
	xrtAtomic32Init(&Methods->References, 1); Methods->Config = Limits;
	Methods->Count = Count; Methods->NameBytes = Names; Methods->OwnedBytes = Bytes;
	char* Next = (char*)(Methods->Entries + Count);
	for (size_t Index = 0; Index < Count; ++Index) {
		xrpcmethod Binding = Source[Index];
		if (Binding.Name.Size) memcpy(Next, Binding.Name.Data, Binding.Name.Size);
		Binding.Name.Data = Next; Next += Binding.Name.Size;
		Methods->Entries[Index].Hash = xrpcMethodsHash(Binding.Name); Methods->Entries[Index].Binding = Binding;
	}
	xrpcMethodsSort(Methods->Entries, Count);
	for (size_t Index = 1; Index < Count; ++Index) if (!xrpcMethodsCompareEntries(&Methods->Entries[Index-1u], &Methods->Entries[Index])) {
		xrtFree(Methods); return xrpcMethodsReport(XERR_EXISTS, XRPC_METHODS_DUPLICATE, "duplicate RPC method name");
	}
	*Result = Methods; return true;
}

XRPC_API bool xrpcMethodsRetain(xrpcmethods* Methods)
{
	if (!Methods) return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method index is NULL");
	uint32 Count = xrtAtomic32Load(&Methods->References, XMEMORY_RELAXED);
	for (;;) {
		if (Count == UINT32_MAX) return xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC method references overflow");
		if (xrtAtomic32CompareExchange(&Methods->References, &Count, Count+1u, XMEMORY_RELAXED, XMEMORY_RELAXED)) return true;
	}
}
XRPC_API void xrpcMethodsRelease(xrpcmethods* Methods)
{
	if (Methods && xrtAtomic32FetchSub(&Methods->References, 1, XMEMORY_ACQ_REL) == 1) xrtFree(Methods);
}
XRPC_API bool xrpcMethodsInfo(const xrpcmethods* Methods, xrpcmethodsinfo* Info)
{
	if (!Methods || !Info || !xrtMemRangeValid(Info, sizeof(*Info)) || Info->Size != sizeof(*Info) ||
		xrtMemRangesOverlap(Methods, Methods->OwnedBytes, Info, sizeof(*Info)))
		return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method info output");
	*Info = (xrpcmethodsinfo){sizeof(*Info),Methods->Count,Methods->NameBytes,Methods->OwnedBytes}; return true;
}

static xjsonvisitaction xrpcMethodsToken(const xjsonscanevent* Event, ptr User)
{
	if (Event->Type != XJSON_SCAN_STRING) {
		xrpcMethodsReport(XERR_VALUE, XRPC_METHODS_VALUE, "RPC method lookup requires one JSON string"); return XJSON_VISIT_FAIL;
	}
	*(xstrview*)User = Event->Raw; return XJSON_VISIT_NEXT;
}
static int xrpcMethodsCompareRaw(xstrview Raw, xstrview Name)
{
	xrpcstringiterator It = {Raw,1,{0},0,0}; unsigned char Byte; size_t Index = 0;
	while (xrpcStringNext(&It, &Byte)) {
		if (Index == Name.Size) return 1;
		unsigned char Other = (unsigned char)Name.Data[Index++];
		if (Byte != Other) return Byte > Other ? 1 : -1;
	}
	return Index == Name.Size ? 0 : -1;
}
XRPC_API xrpcmethodsstatus xrpcMethodsFind(const xrpcmethods* Methods, xstrview Input, xrpcmethod* Binding)
{
	if (!Methods || !Binding || !xrtMemRangeValid(Binding, sizeof(*Binding)) ||
		(!Input.Data && Input.Size) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Binding, sizeof(*Binding), Input.Data, Input.Size) ||
		xrtMemRangesOverlap(Binding, sizeof(*Binding), Methods, Methods->OwnedBytes)) {
		xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "invalid RPC method lookup input/output"); return XRPC_METHODS_ERROR;
	}
	if (Input.Size > Methods->Config.MaxRawNameBytes) {
		xrpcMethodsReport(XERR_RANGE, XRPC_METHODS_LIMIT, "RPC raw method byte limit exceeded"); return XRPC_METHODS_ERROR;
	}
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json); Json.MaxInputBytes = Methods->Config.MaxRawNameBytes;
	Json.MaxStringBytes = Methods->Config.MaxNameBytes; Json.MaxDepth = 1; Json.MaxValues = Json.MaxContainerItems = 1;
	xstrview Raw = {0};
	if (xrtJsonScan(Input, &Json, xrpcMethodsToken, &Raw) != XJSON_VISIT_DONE) return XRPC_METHODS_ERROR;
	if (!Methods->Count) return XRPC_METHODS_NOT_FOUND;
	xstrview Plain = {Raw.Data+1,Raw.Size-2u}; bool Escaped = memchr(Plain.Data, '\\', Plain.Size) != NULL;
	uint32 Hash;
	if (!Escaped) Hash = xrpcMethodsHash(Plain);
	else {
		xrpcstringiterator It = {Raw,1,{0},0,0}; unsigned char Byte; Hash = 2166136261u;
		while (xrpcStringNext(&It, &Byte)) Hash = (Hash ^ Byte) * 16777619u;
	}
	size_t Begin = 0, End = Methods->Count;
	while (Begin < End) {
		size_t Middle = Begin + (End-Begin)/2u; const xrpcmethodsentry* Entry = &Methods->Entries[Middle];
		int Order = Hash != Entry->Hash ? (Hash > Entry->Hash ? 1 : -1) :
			Escaped ? xrpcMethodsCompareRaw(Raw, Entry->Binding.Name) : xrpcMethodsCompareNames(Plain, Entry->Binding.Name);
		if (!Order) { *Binding = Entry->Binding; return XRPC_METHODS_FOUND; }
		if (Order < 0) End = Middle; else Begin = Middle+1u;
	}
	return XRPC_METHODS_NOT_FOUND;
}
XRPC_API bool xrpcMethodsCall(const xrpcmessage* Request, xrpcreply* Reply, ptr User)
{
	if (!Reply) return xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method adapter reply is NULL");
	if (!Request || !xrtMemRangeValid(Request, sizeof(*Request)) ||
		(Request->Kind != XRPC_MESSAGE_CALL && Request->Kind != XRPC_MESSAGE_NOTIFICATION) || Request->Method.Kind != XRPC_VALUE_STRING) {
		xrpcMethodsReport(XERR_ARGUMENT, XRPC_METHODS_ARGUMENT, "RPC method adapter requires a valid request");
		return xrpcReplyAbort(Reply, xrtGetError());
	}
	xrpcmethod Binding; xrpcmethodsstatus Status = xrpcMethodsFind((const xrpcmethods*)User, Request->Method.Raw, &Binding);
	if (Status == XRPC_METHODS_ERROR) return xrpcReplyAbort(Reply, xrtGetError());
	return Status == XRPC_METHODS_FOUND ? Binding.Call(Request, Reply, Binding.User) : false;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/id/id.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_ID)

#if defined(XRPC_FEATURE_XRPC_ID)
typedef struct xrpcidexponent {
	xstrview Digits;
	/* The bounded native offset is stored as little-endian decimal digits.
	 * sizeof(size_t)*3 is larger than its decimal representation on all targets. */
	unsigned char Small[sizeof(size_t)*3u];
	size_t SmallCount, Length;
	bool Subtract, Swap, Negative;
} xrpcidexponent;
typedef struct xrpcidplan {
	xrpcidconfig Config;
	xjsonscantype Type;
	xstrview Raw;
	size_t Bytes, First, Last, Digits;
	bool Negative;
	xrpcidexponent Exponent;
} xrpcidplan;

static bool xrpcIdReport(xerrkind Kind, xrpciderror Code, cstr Message)
{ xrtSetErrorInfo(Kind, "xrt.rpc", (int32)Code, Message); return false; }
XRPC_API bool xrpcIdConfigInit(xrpcidconfig* Config)
{
	if (!Config || !xrtMemRangeValid(Config, sizeof(*Config)))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID config output");
	*Config = (xrpcidconfig){sizeof(*Config),4096,4096,8192}; return true;
}
static xjsonvisitaction xrpcIdToken(const xjsonscanevent* Event, ptr User)
{
	xrpcidplan* Plan = User;
	if (Event->Type != XJSON_SCAN_STRING && Event->Type != XJSON_SCAN_NUMBER && Event->Type != XJSON_SCAN_NULL) {
		xrpcIdReport(XERR_VALUE, XRPC_ID_VALUE, "RPC ID requires a string, number or null"); return XJSON_VISIT_FAIL;
	}
	Plan->Type = Event->Type; Plan->Raw = Event->Raw; return XJSON_VISIT_NEXT;
}

/* Read one digit from the right. No exponent is converted to a native value. */
static unsigned xrpcIdBigDigit(const xrpcidexponent* Exponent, size_t Position)
{ return Position < Exponent->Digits.Size ? (unsigned)(Exponent->Digits.Data[Exponent->Digits.Size-1u-Position]-'0') : 0; }
static unsigned xrpcIdSmallDigit(const xrpcidexponent* Exponent, size_t Position)
{ return Position < Exponent->SmallCount ? Exponent->Small[Position] : 0; }
static size_t xrpcIdExponentRun(const xrpcidexponent* Exponent, unsigned char* Output)
{
	size_t Count = Exponent->Digits.Size > Exponent->SmallCount ? Exponent->Digits.Size : Exponent->SmallCount;
	size_t Length = 1; unsigned Carry = 0;
	for (size_t Index = 0; Index < Count; ++Index) {
		unsigned A = xrpcIdBigDigit(Exponent, Index), B = xrpcIdSmallDigit(Exponent, Index), Digit;
		if (Exponent->Swap) { unsigned Temp = A; A = B; B = Temp; }
		if (Exponent->Subtract) {
			int Difference = (int)A - (int)B - (int)Carry;
			Carry = Difference < 0; Digit = (unsigned)(Difference + (Carry ? 10 : 0));
		} else { unsigned Sum = A + B + Carry; Digit = Sum % 10u; Carry = Sum / 10u; }
		if (Digit) Length = Index+1u;
		if (Output && Index < Exponent->Length) Output[Exponent->Length-1u-Index] = (unsigned char)('0'+Digit);
	}
	if (!Exponent->Subtract && Carry) {
		Length = Count+1u;
		if (Output) Output[0] = (unsigned char)('0'+Carry);
	}
	return Length;
}
static void xrpcIdExponentInit(xrpcidexponent* Exponent, xstrview Digits, bool Negative,
	size_t Offset, bool OffsetNegative)
{
	while (Digits.Size > 1 && Digits.Data[0] == '0') { ++Digits.Data; --Digits.Size; }
	if (Digits.Size == 1 && Digits.Data[0] == '0') Negative = false;
	Exponent->Digits = Digits;
	do { Exponent->Small[Exponent->SmallCount++] = (unsigned char)(Offset % 10u); Offset /= 10u; } while (Offset);
	if (Exponent->SmallCount == 1 && !Exponent->Small[0]) OffsetNegative = false;
	int Order = Digits.Size > Exponent->SmallCount ? 1 : Digits.Size < Exponent->SmallCount ? -1 : 0;
	if (!Order) for (size_t Index = 0; Index < Digits.Size; ++Index) {
		unsigned A = (unsigned)(Digits.Data[Index]-'0'), B = Exponent->Small[Digits.Size-1u-Index];
		if (A != B) { Order = A > B ? 1 : -1; break; }
	}
	Exponent->Subtract = Negative != OffsetNegative;
	Exponent->Swap = Exponent->Subtract && Order < 0;
	Exponent->Negative = Exponent->Subtract ? (Order < 0 ? OffsetNegative : Negative) : Negative;
	if (Exponent->Subtract && !Order) Exponent->Negative = false;
	Exponent->Length = xrpcIdExponentRun(Exponent, NULL);
}
static bool xrpcIdNumber(xrpcidplan* Plan)
{
	xstrview Raw = Plan->Raw; size_t Fraction = 0, Zeros = 0, Position = 0;
	bool Dot = false, Nonzero = false; Plan->Negative = Raw.Data[0] == '-';
	if (Plan->Negative) ++Position;
	for (; Position < Raw.Size && Raw.Data[Position] != 'e' && Raw.Data[Position] != 'E'; ++Position) {
		char Byte = Raw.Data[Position];
		if (Byte == '.') { Dot = true; continue; }
		if (Dot) ++Fraction;
		if (Byte != '0') {
			if (!Nonzero) Plan->First = Position;
			Nonzero = true; Plan->Last = Position; Zeros = 0;
		} else ++Zeros;
	}
	if (!Nonzero) { Plan->Bytes = 2; return true; }
	for (size_t Index = Plan->First; Index <= Plan->Last; ++Index) if (Raw.Data[Index] != '.') ++Plan->Digits;
	xstrview Digits = XRT_STR_LITERAL("0"); bool Negative = false;
	if (Position < Raw.Size) {
		++Position;
		if (Raw.Data[Position] == '+' || Raw.Data[Position] == '-') Negative = Raw.Data[Position++] == '-';
		Digits = (xstrview){Raw.Data+Position,Raw.Size-Position};
	}
	xrpcIdExponentInit(&Plan->Exponent, Digits, Negative, Zeros > Fraction ? Zeros-Fraction : Fraction-Zeros, Zeros < Fraction);
	/* Domain/sign, significant digits, delimiter and signed decimal exponent. */
	size_t Extra = 3u + (Plan->Exponent.Negative ? 1u : 0u);
	if (Plan->Digits > Plan->Config.MaxKeyBytes || Extra > Plan->Config.MaxKeyBytes-Plan->Digits ||
		Plan->Exponent.Length > Plan->Config.MaxKeyBytes-Plan->Digits-Extra)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC numeric ID key limit exceeded");
	Plan->Bytes = Plan->Digits + Extra + Plan->Exponent.Length; return true;
}
static bool xrpcIdPrepare(xrpcidplan* Plan, xstrview Input, const xrpcidconfig* Config)
{
	if ((!Input.Data && Input.Size) || !xrtMemRangeValid(Input.Data, Input.Size) ||
		(Config && !xrtMemRangeValid(Config, sizeof(*Config))))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID input/config span");
	if (Config) Plan->Config = *Config; else xrpcIdConfigInit(&Plan->Config);
	const xrpcidconfig* Limits = &Plan->Config;
	if (Limits->Size != sizeof(*Limits) || !Limits->MaxInputBytes || !Limits->MaxStringBytes || !Limits->MaxKeyBytes)
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID configuration");
	if (Input.Size > Limits->MaxInputBytes)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC ID input byte limit exceeded");
	xjsonreadconfig Json; xrtJsonReadConfigInit(&Json);
	Json.MaxInputBytes = Limits->MaxInputBytes; Json.MaxStringBytes = Limits->MaxStringBytes;
	Json.MaxDepth = 1; Json.MaxValues = Json.MaxContainerItems = 1;
	if (xrtJsonScan(Input, &Json, xrpcIdToken, Plan) != XJSON_VISIT_DONE) return false;
	if (Plan->Type == XJSON_SCAN_NUMBER) {
		if (!xrpcIdNumber(Plan)) return false;
	} else if (Plan->Type == XJSON_SCAN_NULL) Plan->Bytes = 1;
	else {
		Plan->Bytes = 1; xrpcstringiterator It = {Plan->Raw,1,{0},0,0}; unsigned char Byte;
		while (xrpcStringNext(&It, &Byte)) {
			if (Plan->Bytes == Limits->MaxKeyBytes)
				return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC string ID key limit exceeded");
			++Plan->Bytes;
		}
	}
	if (Plan->Bytes > Limits->MaxKeyBytes)
		return xrpcIdReport(XERR_RANGE, XRPC_ID_LIMIT, "RPC ID key limit exceeded");
	return true;
}
static bool xrpcIdOutputSafe(const void* Output, size_t Size, xstrview Input, const xrpcidconfig* Config)
{
	if (!xrtMemRangeValid(Output, Size) || xrtMemRangesOverlap(Output, Size, Input.Data, Input.Size) ||
		(Config && xrtMemRangesOverlap(Output, Size, Config, sizeof(*Config))))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "RPC ID output overlaps input/config or wraps");
	return true;
}
XRPC_API bool xrpcIdKeySize(xstrview Input, const xrpcidconfig* Config, size_t* Required)
{
	xrpcidplan Plan = {0};
	if (!Required) return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "RPC ID key size requires output");
	if (!xrpcIdPrepare(&Plan, Input, Config) || !xrpcIdOutputSafe(Required, sizeof(*Required), Input, Config)) return false;
	*Required = Plan.Bytes; return true;
}
static void xrpcIdEmit(const xrpcidplan* Plan, unsigned char* Output)
{
	if (Plan->Type == XJSON_SCAN_NULL) { Output[0] = 0x10; return; }
	if (Plan->Type == XJSON_SCAN_STRING) {
		*Output++ = 0x11; xrpcstringiterator It = {Plan->Raw,1,{0},0,0}; unsigned char Byte;
		while (xrpcStringNext(&It, &Byte)) *Output++ = Byte;
		return;
	}
	*Output++ = 0x12;
	if (!Plan->Digits) { *Output = '0'; return; }
	*Output++ = Plan->Negative ? '-' : '+';
	for (size_t Index = Plan->First; Index <= Plan->Last; ++Index)
		if (Plan->Raw.Data[Index] != '.') *Output++ = (unsigned char)Plan->Raw.Data[Index];
	*Output++ = 0;
	if (Plan->Exponent.Negative) *Output++ = '-';
	(void)xrpcIdExponentRun(&Plan->Exponent, Output);
}
XRPC_API bool xrpcIdKeyWrite(ptr Output, size_t Capacity, xstrview Input,
	const xrpcidconfig* Config, size_t* Written)
{
	xrpcidplan Plan = {0};
	if (!Output || !xrtMemRangeValid(Output, Capacity))
		return xrpcIdReport(XERR_ARGUMENT, XRPC_ID_ARGUMENT, "invalid RPC ID key buffer");
	if (!xrpcIdPrepare(&Plan, Input, Config)) return false;
	if (Capacity < Plan.Bytes) return xrpcIdReport(XERR_RANGE, XRPC_ID_CAPACITY, "RPC ID key capacity is too small");
	if (!xrpcIdOutputSafe(Output, Plan.Bytes, Input, Config) || (Written &&
		(!xrpcIdOutputSafe(Written, sizeof(*Written), Input, Config) || xrtMemRangesOverlap(Output, Plan.Bytes, Written, sizeof(*Written))))) return false;
	xrpcIdEmit(&Plan, Output);
	if (Written) *Written = Plan.Bytes;
	return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/pending/pending.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_PENDING)
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_PENDING)
typedef struct xrpcpendingentry {
	size_t KeyBytes;
	uint64 Serial, Deadline;
	ptr User;
	uint32 Left, Right, Height, HeapIndex, NextFree;
} xrpcpendingentry;
struct xrpcpending {
	xrpcpendingconfig Config;
	size_t Count, OwnedBytes;
	uint64 Serial;
	uint32 Root, Free;
	xrpcpendingentry Entries[];
};
static xrpcpendingstatus xrpcPendingReport(xerrkind Kind, xrpcpendingerror Code, cstr Message)
{ xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message); return XRPC_PENDING_ERROR; }
static xrpcpendingentry* xrpcPendingEntry(xrpcpending* P,uint32 Slot) { return &P->Entries[Slot-1u]; }
static uint32* xrpcPendingHeap(xrpcpending* P) { return (uint32*)(P->Entries+P->Config.MaxCalls); }
static unsigned char* xrpcPendingScratch(xrpcpending* P) { return (unsigned char*)(xrpcPendingHeap(P)+P->Config.MaxCalls); }
static unsigned char* xrpcPendingKey(xrpcpending* P,uint32 Slot) { return xrpcPendingScratch(P)+(size_t)Slot*P->Config.Id.MaxKeyBytes; }
static int xrpcPendingCompare(xrpcpending* P,const unsigned char* Key,size_t Bytes,uint32 Slot)
{
	size_t Other=xrpcPendingEntry(P,Slot)->KeyBytes,Common=Bytes<Other?Bytes:Other;
	int Order=memcmp(Key,xrpcPendingKey(P,Slot),Common);
	return Order?Order:(Bytes>Other)-(Bytes<Other);
}
static uint32 xrpcPendingHeight(xrpcpending* P,uint32 Slot) { return Slot?xrpcPendingEntry(P,Slot)->Height:0; }
static void xrpcPendingHeightSet(xrpcpending* P,uint32 Slot)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Slot);uint32 L=xrpcPendingHeight(P,E->Left),R=xrpcPendingHeight(P,E->Right);
	E->Height=1u+(L>R?L:R);
}
static uint32 xrpcPendingRotate(xrpcpending* P,uint32 Root,bool Left)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);uint32 Next=Left?E->Right:E->Left;
	xrpcpendingentry* N=xrpcPendingEntry(P,Next);
	if(Left){E->Right=N->Left;N->Left=Root;}else{E->Left=N->Right;N->Right=Root;}
	xrpcPendingHeightSet(P,Root);xrpcPendingHeightSet(P,Next);return Next;
}
static uint32 xrpcPendingBalance(xrpcpending* P,uint32 Root)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);xrpcPendingHeightSet(P,Root);
	int Difference=(int)xrpcPendingHeight(P,E->Left)-(int)xrpcPendingHeight(P,E->Right);
	if(Difference>1){
		xrpcpendingentry* L=xrpcPendingEntry(P,E->Left);
		if(xrpcPendingHeight(P,L->Right)>xrpcPendingHeight(P,L->Left))E->Left=xrpcPendingRotate(P,E->Left,true);
		return xrpcPendingRotate(P,Root,false);
	}
	if(Difference < -1){
		xrpcpendingentry* R=xrpcPendingEntry(P,E->Right);
		if(xrpcPendingHeight(P,R->Left)>xrpcPendingHeight(P,R->Right))E->Right=xrpcPendingRotate(P,E->Right,false);
		return xrpcPendingRotate(P,Root,true);
	}
	return Root;
}
static uint32 xrpcPendingTreeInsert(xrpcpending* P,uint32 Root,uint32 Slot)
{
	if(!Root)return Slot;
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(xrpcPendingCompare(P,xrpcPendingKey(P,Slot),xrpcPendingEntry(P,Slot)->KeyBytes,Root)<0)
		E->Left=xrpcPendingTreeInsert(P,E->Left,Slot);
	else E->Right=xrpcPendingTreeInsert(P,E->Right,Slot);
	return xrpcPendingBalance(P,Root);
}
static uint32 xrpcPendingExtractMin(xrpcpending* P,uint32 Root,uint32* Min)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(!E->Left){*Min=Root;return E->Right;}
	E->Left=xrpcPendingExtractMin(P,E->Left,Min);return xrpcPendingBalance(P,Root);
}
/* Structural transplantation keeps each live token bound to its original slot. */
static uint32 xrpcPendingTreeRemove(xrpcpending* P,uint32 Root,uint32 Slot)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Root);
	if(Root==Slot){
		if(!E->Left)return E->Right;
		if(!E->Right)return E->Left;
		uint32 Next=0,Right=xrpcPendingExtractMin(P,E->Right,&Next);
		xrpcpendingentry* N=xrpcPendingEntry(P,Next);N->Left=E->Left;N->Right=Right;
		return xrpcPendingBalance(P,Next);
	}
	if(xrpcPendingCompare(P,xrpcPendingKey(P,Slot),xrpcPendingEntry(P,Slot)->KeyBytes,Root)<0)
		E->Left=xrpcPendingTreeRemove(P,E->Left,Slot);
	else E->Right=xrpcPendingTreeRemove(P,E->Right,Slot);
	return xrpcPendingBalance(P,Root);
}
static uint32 xrpcPendingFind(xrpcpending* P,size_t Bytes)
{
	uint32 Slot=P->Root;
	while(Slot){int Order=xrpcPendingCompare(P,xrpcPendingScratch(P),Bytes,Slot);
		if(!Order)return Slot;
		Slot=Order<0?xrpcPendingEntry(P,Slot)->Left:xrpcPendingEntry(P,Slot)->Right;}
	return 0;
}
static bool xrpcPendingBefore(xrpcpending* P,uint32 A,uint32 B)
{
	xrpcpendingentry* X=xrpcPendingEntry(P,A);xrpcpendingentry* Y=xrpcPendingEntry(P,B);
	return X->Deadline!=Y->Deadline?X->Deadline<Y->Deadline:X->Serial<Y->Serial;
}
static void xrpcPendingHeapSet(xrpcpending* P,size_t Index,uint32 Slot)
{ xrpcPendingHeap(P)[Index]=Slot;xrpcPendingEntry(P,Slot)->HeapIndex=(uint32)Index; }
static void xrpcPendingHeapUp(xrpcpending* P,size_t Index,uint32 Slot)
{
	uint32* Heap=xrpcPendingHeap(P);
	while(Index){size_t Parent=(Index-1u)/2u;if(!xrpcPendingBefore(P,Slot,Heap[Parent]))break;
		xrpcPendingHeapSet(P,Index,Heap[Parent]);Index=Parent;}
	xrpcPendingHeapSet(P,Index,Slot);
}
static void xrpcPendingHeapDown(xrpcpending* P,size_t Index,uint32 Slot)
{
	uint32* Heap=xrpcPendingHeap(P);
	while(Index<P->Count/2u){size_t Child=Index*2u+1u;
		if(Child+1u<P->Count && xrpcPendingBefore(P,Heap[Child+1u],Heap[Child]))++Child;
		if(!xrpcPendingBefore(P,Heap[Child],Slot))break;
		xrpcPendingHeapSet(P,Index,Heap[Child]);Index=Child;}
	/* Odd counts have one final parent at (Count-2)/2, covered by Count/2. */
	xrpcPendingHeapSet(P,Index,Slot);
}
static bool xrpcPendingOutput(xrpcpending* P,ptr Output,size_t Bytes)
{
	if(!P || !Output || !xrtMemRangeValid(Output,Bytes) || xrtMemRangesOverlap(P,P->OwnedBytes,Output,Bytes)){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending output/table overlap");return false;}
	return true;
}
static bool xrpcPendingInput(xrpcpending* P,xstrview Id,ptr Output,size_t OutputBytes,size_t* Bytes)
{
	if(!xrpcPendingOutput(P,Output,OutputBytes))return false;
	if(!xrtMemRangeValid(Id.Data,Id.Size) || xrtMemRangesOverlap(P,P->OwnedBytes,Id.Data,Id.Size) ||
		xrtMemRangesOverlap(Output,OutputBytes,Id.Data,Id.Size)){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending ID/output/table overlap");return false;}
	return xrpcIdKeyWrite(xrpcPendingScratch(P),P->Config.Id.MaxKeyBytes,Id,&P->Config.Id,Bytes);
}
XRPC_API bool xrpcPendingConfigInit(xrpcpendingconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config))){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending config output");return false;}
	xrpcpendingconfig C={sizeof(C),1024,2u*1024u*1024u,{0}};
	xrpcIdConfigInit(&C.Id);C.Id.MaxStringBytes=256;C.Id.MaxKeyBytes=512;*Config=C;return true;
}
XRPC_API bool xrpcPendingCreate(const xrpcpendingconfig* Config,xrpcpending** Result)
{
	if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config &&
		(!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result))))){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending constructor span");return false;}
	xrpcpendingconfig C;if(Config)C=*Config;else xrpcPendingConfigInit(&C);
	if(C.Size!=sizeof(C) || !C.MaxCalls || C.MaxCalls>UINT32_MAX || !C.MaxOwnedBytes ||
		C.Id.Size!=sizeof(C.Id) || !C.Id.MaxInputBytes || !C.Id.MaxStringBytes || !C.Id.MaxKeyBytes){
		xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending configuration");return false;}
	size_t Bytes=sizeof(xrpcpending);
	if(C.MaxCalls>(SIZE_MAX-Bytes)/(sizeof(xrpcpendingentry)+sizeof(uint32)))goto Limit;
	Bytes+=C.MaxCalls*(sizeof(xrpcpendingentry)+sizeof(uint32));
	if(C.MaxCalls==SIZE_MAX || C.Id.MaxKeyBytes>(SIZE_MAX-Bytes)/(C.MaxCalls+1u))goto Limit;
	Bytes+=(C.MaxCalls+1u)*C.Id.MaxKeyBytes;
	if(Bytes>C.MaxOwnedBytes)goto Limit;
	xrpcpending* P=xrtMalloc(Bytes);
	if(!P){xrpcPendingReport(XERR_MEMORY,XRPC_PENDING_MEMORY,"RPC pending allocation failed");return false;}
	P->Config=C;P->Count=0;P->OwnedBytes=Bytes;P->Serial=0;P->Root=0;P->Free=1;
	for(size_t Index=0;Index<C.MaxCalls;++Index){P->Entries[Index]=(xrpcpendingentry){0};
		P->Entries[Index].NextFree=Index+1u<C.MaxCalls?(uint32)(Index+2u):0;}
	*Result=P;return true;
Limit:
	xrpcPendingReport(XERR_RANGE,XRPC_PENDING_LIMIT,"RPC pending owned-byte limit/size overflow");return false;
}
XRPC_API void xrpcPendingDestroy(xrpcpending* Pending) { if(Pending)xrtFree(Pending); }
XRPC_API bool xrpcPendingInfo(const xrpcpending* Pending,xrpcpendinginfo* Info)
{
	xrpcpending* P=(xrpcpending*)Pending;
	if(!xrpcPendingOutput(P,Info,sizeof(*Info)))return false;
	if(Info->Size!=sizeof(*Info)){xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending info size");return false;}
	*Info=(xrpcpendinginfo){sizeof(*Info),P->Config.MaxCalls,P->Count,P->OwnedBytes,
		P->Count?xrpcPendingEntry(P,xrpcPendingHeap(P)[0])->Deadline:UINT64_MAX};return true;
}
XRPC_API xrpcpendingstatus xrpcPendingInsert(xrpcpending* P,xstrview Id,uint64 Deadline,ptr User,xrpcpendingtoken* Token)
{
	size_t Bytes=0;if(!xrpcPendingInput(P,Id,Token,sizeof(*Token),&Bytes))return XRPC_PENDING_ERROR;
	if(xrpcPendingFind(P,Bytes))return XRPC_PENDING_DUPLICATE;
	if(!P->Free)return XRPC_PENDING_FULL;
	if(P->Serial==UINT64_MAX)return xrpcPendingReport(XERR_RANGE,XRPC_PENDING_LIMIT,"RPC pending serial exhausted");
	uint32 Slot=P->Free;xrpcpendingentry* E=xrpcPendingEntry(P,Slot);P->Free=E->NextFree;
	*E=(xrpcpendingentry){Bytes,++P->Serial,Deadline,User,0,0,1,0,0};
	memcpy(xrpcPendingKey(P,Slot),xrpcPendingScratch(P),Bytes);
	P->Root=xrpcPendingTreeInsert(P,P->Root,Slot);++P->Count;xrpcPendingHeapUp(P,P->Count-1u,Slot);
	*Token=(xrpcpendingtoken){E->Serial,(size_t)Slot-1u};return XRPC_PENDING_OK;
}
static xrpcpendingstatus xrpcPendingTake(xrpcpending* P,uint32 Slot,xrpcpendingcall* Call)
{
	xrpcpendingentry* E=xrpcPendingEntry(P,Slot);
	xrpcpendingcall Value={{E->Serial,(size_t)Slot-1u},E->Deadline,E->User};
	P->Root=xrpcPendingTreeRemove(P,P->Root,Slot);
	size_t Index=E->HeapIndex;uint32 Last=xrpcPendingHeap(P)[--P->Count];
	if(Index<P->Count){
		if(Index && xrpcPendingBefore(P,Last,xrpcPendingHeap(P)[(Index-1u)/2u]))xrpcPendingHeapUp(P,Index,Last);
		else xrpcPendingHeapDown(P,Index,Last);
	}
	*E=(xrpcpendingentry){0};E->NextFree=P->Free;P->Free=Slot;*Call=Value;return XRPC_PENDING_OK;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeId(xrpcpending* P,xstrview Id,xrpcpendingcall* Call)
{
	size_t Bytes=0;if(!xrpcPendingInput(P,Id,Call,sizeof(*Call),&Bytes))return XRPC_PENDING_ERROR;
	uint32 Slot=xrpcPendingFind(P,Bytes);return Slot?xrpcPendingTake(P,Slot,Call):XRPC_PENDING_MISSING;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeToken(xrpcpending* P,xrpcpendingtoken Token,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	if(!Token.Serial || Token.Slot>=P->Config.MaxCalls || P->Entries[Token.Slot].Serial!=Token.Serial)return XRPC_PENDING_MISSING;
	return xrpcPendingTake(P,(uint32)(Token.Slot+1u),Call);
}
XRPC_API xrpcpendingstatus xrpcPendingExpire(xrpcpending* P,uint64 Now,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	if(!P->Count)return XRPC_PENDING_MISSING;
	uint32 Slot=xrpcPendingHeap(P)[0];uint64 Deadline=xrpcPendingEntry(P,Slot)->Deadline;
	return Deadline!=UINT64_MAX && Deadline<=Now?xrpcPendingTake(P,Slot,Call):XRPC_PENDING_MISSING;
}
XRPC_API xrpcpendingstatus xrpcPendingTakeFirst(xrpcpending* P,xrpcpendingcall* Call)
{
	if(!xrpcPendingOutput(P,Call,sizeof(*Call)))return XRPC_PENDING_ERROR;
	return P->Count?xrpcPendingTake(P,xrpcPendingHeap(P)[0],Call):XRPC_PENDING_MISSING;
}
static bool xrpcPendingProbe(xrpcpending* P,const xstrview* Ids,size_t Count,xrpcpendingresolution* Resolutions)
{
	if(!P || Count>SIZE_MAX/sizeof(*Ids) || Count>SIZE_MAX/sizeof(*Resolutions))goto Argument;
	size_t InputBytes=Count*sizeof(*Ids),OutputBytes=Count*sizeof(*Resolutions);
	if(!xrtMemRangeValid(Ids,InputBytes) || !xrtMemRangeValid(Resolutions,OutputBytes) ||
		xrtMemRangesOverlap(P,P->OwnedBytes,Ids,InputBytes) || xrtMemRangesOverlap(P,P->OwnedBytes,Resolutions,OutputBytes) ||
		xrtMemRangesOverlap(Ids,InputBytes,Resolutions,OutputBytes))goto Argument;
	for(size_t Index=0;Index<Count;++Index){xstrview Id=Ids[Index];
		if(!xrtMemRangeValid(Id.Data,Id.Size) || xrtMemRangesOverlap(P,P->OwnedBytes,Id.Data,Id.Size) ||
			xrtMemRangesOverlap(Resolutions,OutputBytes,Id.Data,Id.Size))goto Argument;}
	for(size_t Index=0;Index<Count;++Index){size_t Bytes=0;
		if(!xrpcIdKeyWrite(xrpcPendingScratch(P),P->Config.Id.MaxKeyBytes,Ids[Index],&P->Config.Id,&Bytes))return false;
		uint32 Slot=xrpcPendingFind(P,Bytes);Resolutions[Index]=(xrpcpendingresolution){0};
		if(Slot){xrpcpendingentry* E=xrpcPendingEntry(P,Slot);Resolutions[Index].Status=XRPC_PENDING_MATCHED;
			Resolutions[Index].Call=(xrpcpendingcall){{E->Serial,(size_t)Slot-1u},E->Deadline,E->User};}
	}

	return true;
Argument:
	xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending batch spans/overlap");return false;
}
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
bool xrpcPendingProbeBatch(xrpcpending* P,const xstrview* Ids,size_t Count,xrpcpendingresolution* Resolutions)
{return xrpcPendingProbe(P,Ids,Count,Resolutions);}
#endif
XRPC_API bool xrpcPendingTakeBatch(xrpcpending* P,const xstrview* Ids,size_t Count,xrpcpendingresolution* Resolutions)
{
    if(!xrpcPendingProbe(P,Ids,Count,Resolutions))return false;
	/* All failable work is complete. No insert/callback can reuse a slot here. */
	for(size_t Index=0;Index<Count;++Index)if(Resolutions[Index].Status==XRPC_PENDING_MATCHED){
		xrpcpendingtoken Token=Resolutions[Index].Call.Token;uint32 Slot=(uint32)(Token.Slot+1u);
		if(xrpcPendingEntry(P,Slot)->Serial==Token.Serial)xrpcPendingTake(P,Slot,&Resolutions[Index].Call);
		else Resolutions[Index]=(xrpcpendingresolution){XRPC_PENDING_REPEATED,{{0,0},0,NULL}};
	}
    return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/receive/receive.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_RECEIVE)

#if defined(XRPC_FEATURE_XRPC_RECEIVE)
typedef struct xrpcreceiverange {const void* Data;size_t Bytes;} xrpcreceiverange;
static xrpcresponsestatus xrpcReceiveReport(xerrkind Kind,xrpcreceiveerror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return XRPC_RESPONSE_LOCAL_ERROR;}
XRPC_API xrpcresponsestatus xrpcReceive(xrpcpending* Pending,xstrview Input,
	const xrpcresponseconfig* Config,xrpcmessage* Scratch,size_t Capacity,
	xrpcpendingresolution* Resolutions,size_t ResolutionCapacity,xrpcresponses** Responses,xrpcreceiveinfo* Info)
{
	if(!Capacity || Capacity>SIZE_MAX/sizeof(*Scratch) || !ResolutionCapacity ||
		ResolutionCapacity>SIZE_MAX/sizeof(*Resolutions))goto Argument;
	xrpcpendinginfo PendingInfo={0};PendingInfo.Size=sizeof(PendingInfo);
	if(!xrpcPendingInfo(Pending,&PendingInfo))return XRPC_RESPONSE_LOCAL_ERROR;
	xrpcreceiverange Ranges[]={
		{Input.Data,Input.Size},{Config,Config?sizeof(*Config):0},{Scratch,Capacity*sizeof(*Scratch)},
		{Resolutions,ResolutionCapacity*sizeof(*Resolutions)},{Responses,sizeof(*Responses)},
		{Info,sizeof(*Info)},{Pending,PendingInfo.OwnedBytes}};
	for(size_t A=0;A<sizeof(Ranges)/sizeof(Ranges[0]);++A){
		if(!xrtMemRangeValid(Ranges[A].Data,Ranges[A].Bytes))goto Argument;
		for(size_t B=0;B<A;++B)if(xrtMemRangesOverlap(Ranges[A].Data,Ranges[A].Bytes,Ranges[B].Data,Ranges[B].Bytes))goto Argument;
	}
	if(Info->Size!=sizeof(*Info))goto Argument;
	xrpcresponses* Owned=NULL;xrpcresponsestatus Status=xrpcResponsesRead(Input,Config,Scratch,Capacity,&Owned);
	if(Status!=XRPC_RESPONSE_DONE)return Status;
	xrpcresponseinfo Snapshot={0};Snapshot.Size=sizeof(Snapshot);
	if(!xrpcResponsesInfo(Owned,&Snapshot)){xrpcResponsesRelease(Owned);return XRPC_RESPONSE_LOCAL_ERROR;}
	if(Snapshot.Count>ResolutionCapacity){xrpcResponsesRelease(Owned);
		return xrpcReceiveReport(XERR_RANGE,XRPC_RECEIVE_CAPACITY,"RPC response resolution capacity exceeded");}
	if(!xrpcPendingTakeBatch(Pending,Snapshot.Ids,Snapshot.Count,Resolutions)){xrpcResponsesRelease(Owned);return XRPC_RESPONSE_LOCAL_ERROR;}
	/* No failable work/callback after successful batch removal. */
	xrpcreceiveinfo Complete={0};Complete.Size=sizeof(Complete);Complete.Batch=Snapshot.Batch;Complete.Count=Snapshot.Count;
	for(size_t Index=0;Index<Snapshot.Count;++Index){
		if(Resolutions[Index].Status==XRPC_PENDING_MATCHED)++Complete.Matched;
		else if(Resolutions[Index].Status==XRPC_PENDING_REPEATED)++Complete.Repeated;
		else ++Complete.Unknown;
	}
	*Responses=Owned;*Info=Complete;return XRPC_RESPONSE_DONE;
Argument:
	return xrpcReceiveReport(XERR_ARGUMENT,XRPC_RECEIVE_ARGUMENT,"invalid RPC receive spans/overlap/info size");
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/call/call.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_CALL)
#include <math.h>
#include <string.h>

#if defined(XRPC_FEATURE_XRPC_CALL)
struct xrpccall {
	xatomic32 References;
	xrpccallconfig Config;
	double Deadline;
	xpromise* Promise;
	xcancel* Cancel;
	xrpccallresult* Result;
	xrpcdelivery Delivery;
	bool ContextOwned, Destroying, Batch, QuotaOwned;
};
static bool xrpcCallReport(xerrkind Kind,xrpccallerror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static void xrpcCallResultDrop(ptr Value,ptr User)
{
	(void)User;xrpccallresult* R=Value;
	xrpcResponsesRelease(R->Responses);xrtErrorFree((xerror*)R->Cause);
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(User){xrpcResultQuotaFree(R);return;}
#endif
	xrtFree(R);
}
static void xrpcCallUnref(xrpccall* C)
{
	if(xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
	xrtCancelDestroy(C->Cancel);
	if(C->ContextOwned && C->Config.Drop)C->Config.Drop(C->Config.User);
	xrtFree(C);
}
static void xrpcCallWake(const void* User)
{const xrpccall* C=User;if(C->Config.Wake)C->Config.Wake(C->Config.User);}
static bool xrpcCallRefTry(const void* Value)
{
	xrpccall* C=(xrpccall*)Value;uint32 Count=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
	while(Count!=UINT32_MAX){if(xrtAtomic32CompareExchange(&C->References,&Count,Count+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
	return false;
}
static void xrpcCallProducerDrop(const void* Value){xrpcCallUnref((xrpccall*)Value);}
static bool xrpcCallCount(const void* Value,size_t* Count)
{if(!Value || !Count)return false;*Count=xrtAtomic32Load(&((const xrpccall*)Value)->References,XMEMORY_RELAXED);return true;}
static bool xrpcCallOpaque(const void* Value,xrtownershipvisitor Visit,ptr Context)
{
	/* This is a real native RC node, not a synthetic leaf. Its activation and
	 * arbitrary native context have no collector admission/closed trace policy.
	 * Refuse traversal rather than certify an incomplete ownership graph. */
	(void)Value;(void)Visit;(void)Context;return false;
}
static const xrtownershipops xrpcCallOps={xrpcCallCount,xrpcCallOpaque};
static const xfutureproducerownershipv1 xrpcCallProducer={sizeof(xfutureproducerownershipv1),xrpcCallProducerDrop};
static const xfutureproducercancellationv1 xrpcCallCancellation={sizeof(xfutureproducercancellationv1),xrpcCallRefTry,xrpcCallWake};
XRPC_API bool xrpcCallConfigInit(xrpccallconfig* Config)
{
	if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call config output");
	*Config=(xrpccallconfig){sizeof(*Config),5000,NULL,NULL,NULL};return true;
}
static bool xrpcCallConstruct(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future,double Began,size_t Count,xrpcresultquota* Quota,bool* Limited)
{
	(void)Quota;(void)Limited;
	if(!Call || !Future || !xrtMemRangeValid(Call,sizeof(*Call)) || !xrtMemRangeValid(Future,sizeof(*Future)) ||
		xrtMemRangesOverlap(Call,sizeof(*Call),Future,sizeof(*Future)) || (Config &&
		(!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Call,sizeof(*Call),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Future,sizeof(*Future),Config,sizeof(*Config)))))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call constructor spans/overlap");
	xrpccallconfig Limits;if(Config)Limits=*Config;else xrpcCallConfigInit(&Limits);
	if(Limits.Size!=sizeof(Limits) || (Limits.Timeout<0 && Limits.Timeout!=XRT_WAIT_FOREVER))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call configuration");
	size_t ResultBytes=Count?sizeof(xrpcbatchcallresult)+Count*sizeof(size_t):sizeof(xrpccallresult);ptr Reserved=NULL;
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
	if(Quota){Reserved=xrpcResultQuotaAlloc(Quota,ResultBytes,Limited);if(!Reserved)return false;}
#endif
	xrpccall* C=xrtMalloc(sizeof(*C));if(!C){
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
		if(Reserved)xrpcResultQuotaFree(Reserved);
#endif
		return xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call allocation failed");}
	*C=(xrpccall){0};xrtAtomic32Init(&C->References,2);C->Config=Limits;
	C->Deadline=Limits.Timeout==XRT_WAIT_FOREVER?INFINITY:Limits.Timeout==0?Began:
		nextafter(Began+(double)(Limits.Timeout/1000)+(double)(Limits.Timeout%1000)*.001,INFINITY);
	C->Batch=Count!=0;C->QuotaOwned=Quota!=NULL;
	xfuture* Observer=NULL;C->Result=Reserved?Reserved:xrtMalloc(ResultBytes);
	if(!C->Result)xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call result allocation failed");
	if(C->Result){*C->Result=(xrpccallresult){0};
		if(Count){xrpcbatchcallresult* B=(xrpcbatchcallresult*)C->Result;B->Count=Count;B->Indices=(size_t*)(B+1);
			for(size_t K=0;K<Count;++K)((size_t*)B->Indices)[K]=SIZE_MAX;}
		C->Promise=xrtPromiseCreate(&Observer,NULL);}
	if(C->Promise)C->Cancel=xrtPromiseCancelToken(C->Promise);
	bool Bound=C->Cancel && xrtPromiseProducerBindTakeV2(C->Promise,(xrtownershipref){C,&xrpcCallOps},&xrpcCallProducer,&xrpcCallCancellation);
	if(!Bound){
		xerror* Error=xrtTakeError();xrtPromiseDestroy(C->Promise);xrtFutureDestroy(Observer);
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
		if(C->QuotaOwned)xrpcResultQuotaFree(C->Result);else
#endif
		xrtFree(C->Result);
		xrtCancelDestroy(C->Cancel);xrtFree(C);
		xrtClearError();xrtSetErrorTake(Error);return false;
	}
	C->ContextOwned=true;*Call=C;*Future=Observer;return true;
}
bool xrpcCallCreateEntered(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future,double Began)
{return xrpcCallConstruct(Config,Call,Future,Began,0,NULL,NULL);}
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config,size_t Count,xrpccall** Call,xfuture** Future,double Began)
{
	if(!Count || Count>65536u || Count>(SIZE_MAX-sizeof(xrpcbatchcallresult))/sizeof(size_t))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC batch item count");
	return xrpcCallConstruct(Config,Call,Future,Began,Count,NULL,NULL);
}
#if defined(XRPC_FEATURE_XRPC_RESULT_QUOTA)
bool xrpcCallCreateQuota(const xrpccallconfig* Config,size_t Count,xrpccall** Call,xfuture** Future,double Began,xrpcresultquota* Quota,bool* Limited)
{
	*Limited=false;
	if(!Quota || Count>65536u || Count>(SIZE_MAX-sizeof(xrpcbatchcallresult))/sizeof(size_t))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid quota RPC constructor");
	return xrpcCallConstruct(Config,Call,Future,Began,Count,Quota,Limited);
}
#endif
XRPC_API bool xrpcCallCreate(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future)
{return xrpcCallCreateEntered(Config,Call,Future,xrtTimer());}
XRPC_API bool xrpcBatchCallCreate(const xrpccallconfig* Config,size_t Count,xrpccall** Call,xfuture** Future)
{return xrpcBatchCallCreateEntered(Config,Count,Call,Future,xrtTimer());}
XRPC_API bool xrpcCallMarkSent(xrpccall* Call)
{
	if(!Call)return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");
	if(!Call->Promise)return false;
	Call->Delivery=XRPC_MAY_HAVE_EXECUTED;return true;
}
XRPC_API double xrpcCallDeadline(const xrpccall* Call)
{if(!Call){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return NAN;}return Call->Deadline;}
XRPC_API bool xrpcCallCancellationRequested(const xrpccall* Call)
{if(!Call)return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return xrtCancelRequested(Call->Cancel);}
static xrpccallfinish xrpcCallPublish(xrpccall* C,xrpccallstatus Status,xrpcresponses* Responses,size_t Index,const xerror* Cause,bool Reply,const size_t* Indices)
{
	if(Responses && !xrpcResponsesRetain(Responses))return XRPC_CALL_FINISH_ERROR;
	xerror* Held=Cause?xrtErrorRef(Cause):NULL;
	if(Cause && !Held){xrpcResponsesRelease(Responses);return XRPC_CALL_FINISH_ERROR;}
	if(!xrpcCallRefTry(C)){
		xrpcResponsesRelease(Responses);xrtErrorFree(Held);xrpcCallReport(XERR_RANGE,XRPC_CALL_STATE,"RPC call references overflow");return XRPC_CALL_FINISH_ERROR;}
	if(xrtTimer()>=C->Deadline)Status=XRPC_CALL_TIMEOUT;
	else if(xrtCancelRequested(C->Cancel))Status=XRPC_CALL_CANCELLED;
	if(Status!=XRPC_CALL_OK && Status!=XRPC_CALL_REMOTE_ERROR && Status!=XRPC_CALL_BATCH_DONE){xrpcResponsesRelease(Responses);Responses=NULL;Index=0;}
	xpromise* Promise=C->Promise;xrpccallresult* Result=C->Result;
	if(C->Batch){xrpcbatchcallresult* B=(xrpcbatchcallresult*)Result;Index=SIZE_MAX;
		if(Status==XRPC_CALL_BATCH_DONE)memcpy((size_t*)B->Indices,Indices,B->Count*sizeof(size_t));
		else for(size_t K=0;K<B->Count;++K)((size_t*)B->Indices)[K]=SIZE_MAX;}
	*Result=(xrpccallresult){Status,Reply?XRPC_REPLIED:C->Delivery,Responses,Index,Held};
	C->Promise=NULL;C->Result=NULL;
	xerror* Previous=xrtTakeError();bool Published=xrtPromiseResolveOwned(Promise,Result,xrpcCallResultDrop,C->QuotaOwned?Result:NULL);
	if(Published)xrtPromiseDestroy(Promise);
	else {C->Promise=Promise;C->Result=Result;xrpcResponsesRelease(Result->Responses);xrtErrorFree((xerror*)Result->Cause);*Result=(xrpccallresult){0};
		if(C->Batch){xrpcbatchcallresult* B=(xrpcbatchcallresult*)Result;for(size_t K=0;K<B->Count;++K)((size_t*)B->Indices)[K]=SIZE_MAX;}}
	if(Published){xrpcCallUnref(C);xrtClearError();xrtSetErrorTake(Previous);return XRPC_CALL_FINISHED;}
	xerror* Error=xrtTakeError();xrpcCallUnref(C);xrtErrorFree(Previous);xrtClearError();xrtSetErrorTake(Error);return XRPC_CALL_FINISH_ERROR;
}
XRPC_API xrpccallfinish xrpcCallFinish(xrpccall* C,xrpccallstatus Status,xrpcresponses* Responses,size_t Index,const xerror* Cause)
{
	if(!C){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return XRPC_CALL_FINISH_ERROR;}
	if(!C->Promise)return XRPC_CALL_ALREADY_DONE;
	bool Reply=Status==XRPC_CALL_OK || Status==XRPC_CALL_REMOTE_ERROR;
	if(Status<XRPC_CALL_OK || Status>XRPC_CALL_HTTP_ERROR || Status==XRPC_CALL_BATCH_DONE || (C->Batch && Reply) ||
		(Status==XRPC_CALL_NOTIFY_DONE && C->Delivery==XRPC_NOT_SENT) || (Reply?!Responses:(Responses!=NULL || Index!=0)))goto Argument;
	if(Reply){xrpcresponseinfo Info={0};Info.Size=sizeof(Info);
		if(!xrpcResponsesInfo(Responses,&Info))return XRPC_CALL_FINISH_ERROR;
		if(Index>=Info.Count || Info.Messages[Index].Kind!=(Status==XRPC_CALL_OK?XRPC_MESSAGE_RESULT:XRPC_MESSAGE_ERROR))goto Argument;}
	return xrpcCallPublish(C,Status,Responses,Index,Cause,Reply,NULL);
Argument:
	xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call terminal status/response/index");return XRPC_CALL_FINISH_ERROR;
}
XRPC_API xrpccallfinish xrpcBatchCallFinish(xrpccall* C,xrpcresponses* Responses,const size_t* Indices,size_t Count,const xerror* Cause)
{
	if(!C){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return XRPC_CALL_FINISH_ERROR;}
	if(!C->Promise)return XRPC_CALL_ALREADY_DONE;
	if(!C->Batch || !Responses || !Indices)goto Argument;
	xrpcbatchcallresult* B=(xrpcbatchcallresult*)C->Result;
	if(Count!=B->Count || !xrtMemRangeValid(Indices,Count*sizeof(*Indices)) ||
		xrtMemRangesOverlap(Indices,Count*sizeof(*Indices),C,sizeof(*C)) ||
		xrtMemRangesOverlap(Indices,Count*sizeof(*Indices),B,sizeof(*B)+Count*sizeof(size_t)))goto Argument;
	xrpcresponseinfo I={0};I.Size=sizeof(I);if(!xrpcResponsesInfo(Responses,&I))return XRPC_CALL_FINISH_ERROR;
	if(!I.Batch || I.Count>Count)goto Argument;
	unsigned char* Seen=(unsigned char*)B->Indices;memset(Seen,0,(I.Count+7u)/8u);size_t Matched=0;
	for(size_t K=0;K<Count;++K){size_t V=Indices[K];if(V==SIZE_MAX)continue;
		if(V>=I.Count || (Seen[V/8u]&(1u<<(V%8u))))goto Argument;
		Seen[V/8u]|=(unsigned char)(1u<<(V%8u));++Matched;}
	if(Matched!=I.Count)goto Argument;
	return xrpcCallPublish(C,XRPC_CALL_BATCH_DONE,Responses,SIZE_MAX,Cause,true,Indices);
Argument:
	xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC batch completion/map");return XRPC_CALL_FINISH_ERROR;
}
XRPC_API xrpccallfinish xrpcCallPoll(xrpccall* C)
{
	if(!C){xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"RPC call is NULL");return XRPC_CALL_FINISH_ERROR;}
	if(!C->Promise)return XRPC_CALL_ALREADY_DONE;
	if(xrtTimer()>=C->Deadline)return xrpcCallFinish(C,XRPC_CALL_TIMEOUT,NULL,0,NULL);
	if(xrtCancelRequested(C->Cancel))return xrpcCallFinish(C,XRPC_CALL_CANCELLED,NULL,0,NULL);
	return XRPC_CALL_PENDING;
}
XRPC_API bool xrpcCallDestroy(xrpccall* C)
{
	if(!C || C->Destroying)return true;
	xerror* Previous=xrtTakeError();
	C->Destroying=true;
	if(C->Promise && xrpcCallFinish(C,XRPC_CALL_CLOSED,NULL,0,NULL)==XRPC_CALL_FINISH_ERROR){
		C->Destroying=false;xerror* Error=xrtTakeError();xrtErrorFree(Previous);xrtClearError();xrtSetErrorTake(Error);return false;}
	xrpcCallUnref(C);xrtClearError();xrtSetErrorTake(Previous);return true;
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/duplex/duplex.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_DUPLEX)
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_DUPLEX)
typedef struct xrpcduplexrequest xrpcduplexrequest;
typedef struct xrpcduplexentry {
    xrpcduplexrequest* Request;
    xrpcpendingtoken Token;
    uint64 Id;
    size_t Index;
} xrpcduplexentry;
typedef struct xrpcduplexhostjob {
    uint64 Token;
    size_t Count;
    bool Batch;
    xstrview Ids[];
} xrpcduplexhostjob;

/* Owner-only slots use one-based links. An empty slot reuses Next for
 * the free list; live slots let Close retire one job per work unit. */
typedef struct xrpcduplexhostslot {
    xrpcduplexhostjob* Job;
    uint32 Next, Previous;
} xrpcduplexhostslot;

/* The lower bits locate the slot; the serial prevents stale matches. */
enum { XRPC_DUPLEX_HOST_SLOT_BITS = 16,
       XRPC_DUPLEX_HOST_SLOT_COUNT = 1u << XRPC_DUPLEX_HOST_SLOT_BITS };

struct xrpcduplexoutput {
    xatomic32 References;
    struct xrpcduplexoutput *Next,*Prev;
    xrpcduplexrequest* Request; /* Owner-only; cleared before request retirement. */
    xstrview Wire;
    bool Linked,Started;
};
struct xrpcduplexrequest {
    xatomic32 References;
    xrpcduplex* Duplex;
    xrpcduplexrequest *Next,*Prev,*SubmitNext;
    xrpccall* Call;
    xrpcduplexoutput* Output;
    xrpcresponses* Responses;
    size_t Count,Replies;
    double Deadline;
    bool Batch,Started,Sent,Finishing,Finished;
    xrpccallstatus Status;
    uint32 DeadlineSlot;
    xrpcduplexentry Entries[];
};
struct xrpcduplex {
    xrpcduplexconfig Config;
    xmutex Gate;
    xatomic32 References;
    xrpcresultquota *Control,*Wire,*Results;
    xrpcpending* Pending;
    xrpcduplexrequest *SubmitHead,*SubmitTail,*Head,*Tail,*Scan,*ScanEnd;
    xrpcduplexrequest** Deadlines;
    xrpcduplexoutput *OutputHead,*OutputTail,*Selection;
    xrpcduplexhostslot* HostSlots;
    uint32 HostHead, HostFree;
    uint64 HostSerial;
    size_t Operations,PendingCalls,Incoming,DeadlineCount,Finishing,OwnedBytes,FixedBytes;
    size_t Constructing,TemporaryBytes,PeakTemporaryBytes;
    uint64 Serial,SelectionSerial,SelectedToken;
    uint64 Accepted,Rejected,Completed,Received,UnknownResponses,ProtocolFailures;
    bool Closing,Busy,More,PreferScan;
    unsigned Turn;
    xrpccallstatus Reason;
    xrpcmessage* Scratch;
    xrpcpendingresolution* Resolutions;
    xstrview* CorrelationIds;
    char* CorrelationText;
};

static bool xrpcDuplexReport(xerrkind Kind,xrpcduplexerror Code,cstr Text)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Text);return false;}
static bool xrpcDuplexSpan(xrpcduplex* D,const void* P,size_t N)
{return xrtMemRangeValid(P,N) && !xrtMemRangesOverlap(D,D->OwnedBytes,P,N);}
static bool xrpcDuplexEnter(xrpcduplex* D)
{
    if(!D || D->Busy)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex missing owner or mutating reentry");
    D->Busy=true;return true;
}
static void xrpcDuplexWake(xrpcduplex* D)
{if(D->Config.Wake)D->Config.Wake(D->Config.User);}
static void xrpcDuplexDrop(ptr Data)
{xrpcduplexrequest* R=Data;if(xrtAtomic32FetchSub(&R->References,1,XMEMORY_ACQ_REL)!=1)return;
 xrpcduplex* D=R->Duplex;xrpcResultQuotaFree(R);xrtAtomic32FetchSub(&D->References,1,XMEMORY_RELEASE);}
static void xrpcDuplexCallWake(ptr Data)
{xrpcduplexrequest* R=Data;xrpcDuplexWake(R->Duplex);}
static size_t* xrpcDuplexMap(xrpcduplexrequest* R)
{return (size_t*)(R->Entries+R->Count);}
static xstrview xrpcDuplexId(uint64 Id,char* Buffer)
{int N=snprintf(Buffer,24,"\"%llu\"",(unsigned long long)Id);return (xstrview){Buffer,(size_t)N};}
XRPC_API bool xrpcDuplexConfigInit(xrpcduplexconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex config output");
    *C=(xrpcduplexconfig){0};C->Size=sizeof(*C);xrpcEncodeConfigInit(&C->Encode);xrpcResponseConfigInit(&C->Response);
    C->MaxOperations=128;C->MaxPendingCalls=1024;C->MaxIncoming=64;C->MaxControlBytes=2u*1024u*1024u;
    C->MaxOutputs=256;C->MaxOutputBytes=8u*1024u*1024u;C->MaxResults=1024;C->MaxResultBytes=8u*1024u*1024u;
    C->MaxFixedBytes=16u*1024u*1024u;C->MaxTemporaryBytes=1024u*1024u;C->MaxWork=64;return true;
}
XRPC_API bool xrpcDuplexCreate(const xrpcduplexconfig* Config,xrpcduplex** Result)
{
    if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config &&
       (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))))
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex constructor spans");
    xrpcduplexconfig C;if(Config)C=*Config;else xrpcDuplexConfigInit(&C);
    if(C.Size!=sizeof(C) || !C.MaxOperations || C.MaxOperations>XRPC_DUPLEX_HOST_SLOT_COUNT || !C.MaxIncoming || C.MaxIncoming>C.MaxOperations ||
       !C.MaxPendingCalls || C.MaxPendingCalls>65536 || !C.MaxControlBytes || !C.MaxFixedBytes || !C.MaxOutputs || C.MaxOutputs>65536 ||
       !C.MaxOutputBytes || !C.MaxResults || C.MaxResults>65536 || !C.MaxResultBytes || !C.MaxTemporaryBytes ||
       !C.MaxWork || C.MaxWork>65536 || C.Encode.Size!=sizeof(C.Encode) ||
       C.Response.Size!=sizeof(C.Response) || C.Response.Decode.Size!=sizeof(C.Response.Decode) ||
       C.Response.Decode.Mode!=XRPC_DECODE_RESPONSES || !C.Response.MaxOwnedBytes ||
       !C.Response.Decode.MaxMessages || C.Response.Decode.MaxMessages>65536 ||
       !C.Encode.MaxMessages || C.Encode.MaxMessages>65536)
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex configuration");
    /* Exercise complete nested configs with allocation-free public validators. */
    xrpcoutgoing Check={0};Check.Kind=XRPC_MESSAGE_NOTIFICATION;Check.Method=XRT_STR_LITERAL("x");size_t N;
    xrpcmessage M;xrpcdocument Doc;
    if(!xrpcEncodeSize(&Check,1,false,&C.Encode,&N) ||
       xrpcDecode(XRT_STR_LITERAL("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}"),&C.Response.Decode,&M,1,&Doc)!=XRPC_DECODE_DONE)return false;
    size_t Count=C.Response.Decode.MaxMessages;
    size_t Fixed=sizeof(xrpcduplex)+C.MaxOperations*sizeof(xrpcduplexrequest*);
    if (C.OnRequest) Fixed += C.MaxIncoming * sizeof(xrpcduplexhostslot);
    size_t PerMessage=sizeof(xrpcmessage)+sizeof(xrpcpendingresolution)+sizeof(xstrview)+24u;
    if(Count>(SIZE_MAX-Fixed)/PerMessage)
        return xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex storage overflow");
    size_t Bytes=Fixed+Count*PerMessage;
    if(Bytes>=C.MaxFixedBytes)
        return xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex fixed-storage limit");
    xrpcduplex* D=xrtCalloc(1,Bytes);if(!D)goto Memory;
    D->Config=C;D->OwnedBytes=Bytes;D->Scratch=(xrpcmessage*)(D+1);D->Resolutions=(xrpcpendingresolution*)(D->Scratch+Count);
    D->CorrelationIds=(xstrview*)(D->Resolutions+Count);D->CorrelationText=(char*)(D->CorrelationIds+Count);
    D->Deadlines=(xrpcduplexrequest**)(D->CorrelationText+Count*24u);
    if (C.OnRequest) {
        D->HostSlots = (xrpcduplexhostslot*)(D->Deadlines + C.MaxOperations);
        D->HostFree = 1;
        for (uint32 I = 1; I < C.MaxIncoming; ++I) D->HostSlots[I - 1].Next = I + 1;
    }
    if(!xrtMutexInit(&D->Gate)){xrtFree(D);return false;}
    xrtAtomic32Init(&D->References,1);
    xrpcpendingconfig P;xrpcPendingConfigInit(&P);P.MaxCalls=C.MaxPendingCalls;
    /* Only our canonical nonzero uint64 decimal string IDs enter this table.
     * Peer IDs are fully validated by the response codec, then filtered below.
     * The comparison key is one type byte plus at most twenty digits. */
    P.Id.MaxInputBytes=22;P.Id.MaxStringBytes=20;P.Id.MaxKeyBytes=21;
    P.MaxOwnedBytes=C.MaxFixedBytes-Bytes;
    if(!xrpcPendingCreate(&P,&D->Pending))goto Failed;
    if(!xrpcResultQuotaCreate(C.MaxOperations,C.MaxControlBytes,&D->Control) ||
       !xrpcResultQuotaCreate(C.MaxOutputs,C.MaxOutputBytes,&D->Wire) ||
       !xrpcResultQuotaCreate(C.MaxResults,C.MaxResultBytes,&D->Results)){
        xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex ledger construction failed");goto Failed;}
    xrpcpendinginfo PI={0};PI.Size=sizeof(PI);xrpcPendingInfo(D->Pending,&PI);D->FixedBytes=Bytes+PI.OwnedBytes;
    *Result=D;return true;
Failed:
    xrpcPendingDestroy(D->Pending);xrpcResultQuotaRelease(D->Control);xrpcResultQuotaRelease(D->Wire);
    xrpcResultQuotaRelease(D->Results);xrtMutexUnit(&D->Gate);xrtFree(D);return false;
Memory:
    return xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex construction failed");
}
XRPC_API bool xrpcDuplexOutputRetain(xrpcduplexoutput* O)
{
    if(!O)return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"RPC duplex output is NULL");
    uint32 N=xrtAtomic32Load(&O->References,XMEMORY_RELAXED);
    for(;;){if(N==UINT32_MAX)return xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex output reference overflow");
        if(xrtAtomic32CompareExchange(&O->References,&N,N+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XRPC_API void xrpcDuplexOutputRelease(xrpcduplexoutput* O)
{if(O && xrtAtomic32FetchSub(&O->References,1,XMEMORY_ACQ_REL)==1){xrpcResultQuotaFree(O);}}
XRPC_API xstrview xrpcDuplexOutputWire(const xrpcduplexoutput* O)
{return O?O->Wire:(xstrview){0};}
static xrpcduplexoutput* xrpcDuplexOutputCreate(xrpcduplex* D,size_t Allocation,size_t Charge,bool* Limited)
{
    if(Allocation>SIZE_MAX-sizeof(xrpcduplexoutput) || Charge>SIZE_MAX-sizeof(xrpcduplexoutput)){*Limited=true;return NULL;}
    xrpcduplexoutput* O=xrpcResultQuotaAllocCharged(D->Wire,sizeof(*O)+Allocation,sizeof(*O)+Charge,Limited);
    if(O){memset(O,0,sizeof(*O));xrtAtomic32Init(&O->References,1);}return O;
}
static void xrpcDuplexConstructionReturn(xrpcduplex* D)
{
    /* Wake can race the owner seeing the final constructor pin. Closing Info
     * supplies a retry hint until the last physical reference has retired. */
    xerror* Error=xrtTakeError();xrpcDuplexWake(D);
    xrtAtomic32FetchSub(&D->References,1,XMEMORY_RELEASE);
    xrtClearError();xrtSetErrorTake(Error);
}
static xrpcduplexadmission xrpcDuplexSubmit(xrpcduplex* D,const xrpcduplexitem* Items,
    size_t Count,bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!D || !Count || Count>D->Config.Encode.MaxMessages || (!Batch && Count!=1) || Timeout<XRT_WAIT_FOREVER ||
       Count>SIZE_MAX/sizeof(*Items) || !xrpcDuplexSpan(D,Items,Count*sizeof(*Items)) ||
       !Future || !xrpcDuplexSpan(D,Future,sizeof(*Future)) || xrtMemRangesOverlap(Items,Count*sizeof(*Items),Future,sizeof(*Future)))goto Argument;
    size_t Replies=0;
    for(size_t I=0;I<Count;++I){const xrpcduplexitem* V=Items+I;
        if(!xrpcDuplexSpan(D,V->Method.Data,V->Method.Size) || !xrpcDuplexSpan(D,V->Params.Data,V->Params.Size) ||
           xrtMemRangesOverlap(Future,sizeof(*Future),V->Method.Data,V->Method.Size) ||
           xrtMemRangesOverlap(Future,sizeof(*Future),V->Params.Data,V->Params.Size))goto Argument;
        if(!V->Notify)++Replies;
    }
    if(Count>SIZE_MAX/(sizeof(xrpcoutgoing)+24u) || Count>(SIZE_MAX-sizeof(xrpcduplexrequest))/(sizeof(xrpcduplexentry)+sizeof(size_t)))goto Argument;
    size_t TempBytes=Count*(sizeof(xrpcoutgoing)+24u),Bytes=sizeof(xrpcduplexrequest)+Count*(sizeof(xrpcduplexentry)+sizeof(size_t));
    /* Only reservation/publication are serialized. JSON scans, quoting, copies,
     * allocator/quota locks and native Future construction do not hold Gate. */
    xrtMutexLock(&D->Gate);
    if(D->Closing){xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_CLOSED;}
    if(D->Operations==D->Config.MaxOperations || Replies>D->Config.MaxPendingCalls-D->PendingCalls ||
       Replies>UINT64_MAX-D->Serial || TempBytes>D->Config.MaxTemporaryBytes-D->TemporaryBytes){
        ++D->Rejected;xrtMutexUnlock(&D->Gate);return XRPC_DUPLEX_OVERLOADED;}
    uint64 Serial=D->Serial;D->Serial+=Replies; /* Gaps on rollback; never reuse. */
    ++D->Operations;D->PendingCalls+=Replies;++D->Constructing;D->TemporaryBytes+=TempBytes;
    if(D->TemporaryBytes>D->PeakTemporaryBytes)D->PeakTemporaryBytes=D->TemporaryBytes;
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED); /* Constructor pin. */
    xrtMutexUnlock(&D->Gate);
    bool Limited=false;xrpcduplexadmission Result=XRPC_DUPLEX_ERROR;
    xrpcduplexrequest* R=xrpcResultQuotaAlloc(D->Control,Bytes,&Limited);
    xrpcduplexoutput* O=NULL;xrpcoutgoing* V=NULL;
    if(!R){if(Limited)Result=XRPC_DUPLEX_OVERLOADED;else goto Memory;goto Rollback;}
    memset(R,0,Bytes);R->Duplex=D;R->Count=Count;R->Replies=Replies;R->Batch=Batch;
    xrtAtomic32Init(&R->References,1);
    xrtAtomic32FetchAdd(&D->References,1,XMEMORY_RELAXED); /* Request/context pin. */
    V=xrtMalloc(TempBytes);if(!V)goto Memory;
    memset(V,0,Count*sizeof(*V));char* Ids=(char*)(V+Count);
    for(size_t I=0;I<Count;++I){V[I].Kind=Items[I].Notify?XRPC_MESSAGE_NOTIFICATION:XRPC_MESSAGE_CALL;
        V[I].Method=Items[I].Method;V[I].Params=Items[I].Params;
        R->Entries[I].Request=R;R->Entries[I].Index=I;
        if(!Items[I].Notify){R->Entries[I].Id=++Serial;V[I].Id=xrpcDuplexId(Serial,Ids+I*24u);}
        xrpcDuplexMap(R)[I]=SIZE_MAX;
    }
    size_t WireBytes=0;
    if(!xrpcEncodeSize(V,Count,Batch,&D->Config.Encode,&WireBytes))goto Rollback;
    O=xrpcDuplexOutputCreate(D,WireBytes,WireBytes,&Limited);
    if(!O){if(Limited){Result=XRPC_DUPLEX_OVERLOADED;goto Rollback;}goto Memory;}
    char* Data=(char*)(O+1);
    if(!xrpcEncodeWrite(Data,WireBytes,V,Count,Batch,&D->Config.Encode,NULL))goto Rollback;
    xrtFree(V);V=NULL;O->Wire=(xstrview){Data,WireBytes};O->Request=R;R->Output=O;
    xrpccallconfig C;xrpcCallConfigInit(&C);C.Timeout=Timeout;C.Wake=xrpcDuplexCallWake;C.Drop=xrpcDuplexDrop;C.User=R;
    xfuture* F=NULL;
    if(!xrpcCallCreateQuota(&C,Batch?Count:0,&R->Call,&F,Entered,D->Results,&Limited)){
        if(Limited)Result=XRPC_DUPLEX_OVERLOADED;
        goto Rollback;}
    xrtAtomic32FetchAdd(&R->References,1,XMEMORY_RELAXED); /* Native context owns one R reference. */
    xrtMutexLock(&D->Gate);
    --D->Constructing;D->TemporaryBytes-=TempBytes;++D->Accepted;
    /* A successful reservation precedes a racing Close. Publish even if it has
     * closed meanwhile: owner completes CLOSED without sending/replaying it. */
    if(D->SubmitTail)D->SubmitTail->SubmitNext=R;else D->SubmitHead=R;D->SubmitTail=R;
    *Future=F;xrtMutexUnlock(&D->Gate);xrpcDuplexConstructionReturn(D);return XRPC_DUPLEX_ACCEPTED;
Memory:
    xrpcDuplexReport(XERR_MEMORY,XRPC_DUPLEX_MEMORY,"RPC duplex admission allocation failed");
Rollback:
    xrtFree(V);xrpcDuplexOutputRelease(O);if(R)xrpcDuplexDrop(R);
    xrtMutexLock(&D->Gate);--D->Operations;D->PendingCalls-=Replies;--D->Constructing;D->TemporaryBytes-=TempBytes;
    if(Result==XRPC_DUPLEX_OVERLOADED)++D->Rejected;
    xrtMutexUnlock(&D->Gate);xrpcDuplexConstructionReturn(D);return Result;
Argument:
    xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex admission spans/count/timeout");return XRPC_DUPLEX_ERROR;
}
XRPC_API xrpcduplexadmission xrpcDuplexCall(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={false,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered);}
XRPC_API xrpcduplexadmission xrpcDuplexNotify(xrpcduplex* D,xstrview Method,xstrview Params,int64 Timeout,xfuture** F)
{double Entered=xrtTimer();xrpcduplexitem Item={true,Method,Params};return xrpcDuplexSubmit(D,&Item,1,false,Timeout,F,Entered);}
XRPC_API xrpcduplexadmission xrpcDuplexBatch(xrpcduplex* D,const xrpcduplexitem* Items,size_t Count,int64 Timeout,xfuture** F)
{return xrpcDuplexSubmit(D,Items,Count,true,Timeout,F,xrtTimer());}
static bool xrpcDuplexIsClosing(xrpcduplex* D)
{xrtMutexLock(&D->Gate);bool B=D->Closing;xrtMutexUnlock(&D->Gate);return B;}
static void xrpcDuplexSeal(xrpcduplex* D,xrpccallstatus Reason)
{xrtMutexLock(&D->Gate);if(!D->Closing){D->Closing=true;D->Reason=Reason;}xrtMutexUnlock(&D->Gate);}
XRPC_API bool xrpcDuplexClose(xrpcduplex* D,xrpccallstatus Reason)
{
    if(!D || (Reason!=XRPC_CALL_CLOSED && Reason!=XRPC_CALL_TRANSPORT && Reason!=XRPC_CALL_PROTOCOL && Reason!=XRPC_CALL_LIMIT && Reason!=XRPC_CALL_MEMORY && Reason!=XRPC_CALL_TIMEOUT && Reason!=XRPC_CALL_HTTP_ERROR))
        return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex close reason");
    xrpcDuplexSeal(D,Reason);xrpcDuplexWake(D);return true;
}
static void xrpcDuplexQueue(xrpcduplex* D,xrpcduplexoutput* O)
{O->Linked=true;O->Prev=D->OutputTail;if(O->Prev)O->Prev->Next=O;else D->OutputHead=O;D->OutputTail=O;}
/* The two reserved v1 methods describe a connection-local abandonment
 * extension. Legacy sessions never spend allocation or wire credit on it. */

static void xrpcDuplexUnqueue(xrpcduplex* D,xrpcduplexoutput* O)
{
    if(D->Selection==O){D->Selection=NULL;D->SelectedToken=0;}
    if(O->Linked){if(O->Prev)O->Prev->Next=O->Next;else D->OutputHead=O->Next;
        if(O->Next)O->Next->Prev=O->Prev;else D->OutputTail=O->Prev;O->Linked=false;}
    O->Next=O->Prev=NULL;O->Request=NULL;xrpcDuplexOutputRelease(O);
}
/* Owner-only heap with one slot per finite-deadline operation. The request
 * keeps its index so reply/cancel/close can remove it without leaving tombstones. */
static void xrpcDuplexDeadlineSet(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    D->Deadlines[I]=R;
    R->DeadlineSlot=(uint32)(I+1u);
}
static void xrpcDuplexDeadlineUp(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    while(I){
        size_t Parent=(I-1u)/2u;
        if(D->Deadlines[Parent]->Deadline<=R->Deadline)break;
        xrpcDuplexDeadlineSet(D,I,D->Deadlines[Parent]);
        I=Parent;
    }
    xrpcDuplexDeadlineSet(D,I,R);
}
static void xrpcDuplexDeadlineDown(xrpcduplex* D,size_t I,xrpcduplexrequest* R)
{
    while(I<D->DeadlineCount/2u){
        size_t Child=I*2u+1u;
        if(Child+1u<D->DeadlineCount && D->Deadlines[Child+1u]->Deadline<D->Deadlines[Child]->Deadline)++Child;
        if(R->Deadline<=D->Deadlines[Child]->Deadline)break;
        xrpcDuplexDeadlineSet(D,I,D->Deadlines[Child]);
        I=Child;
    }
    xrpcDuplexDeadlineSet(D,I,R);
}
static void xrpcDuplexDeadlineRemove(xrpcduplex* D,xrpcduplexrequest* R)
{
    if(!R->DeadlineSlot)return;
    size_t I=R->DeadlineSlot-1u;
    R->DeadlineSlot=0;
    xrpcduplexrequest* Last=D->Deadlines[--D->DeadlineCount];
    if(I==D->DeadlineCount)return;
    if(I && Last->Deadline<D->Deadlines[(I-1u)/2u]->Deadline)
        xrpcDuplexDeadlineUp(D,I,Last);
    else xrpcDuplexDeadlineDown(D,I,Last);
}
static void xrpcDuplexAdvanceScan(xrpcduplex* D)
{
    if(D->Scan==D->ScanEnd)D->Scan=D->ScanEnd=NULL;
    else D->Scan=D->Scan->Next;
}
static void xrpcDuplexPrepareFinish(xrpcduplex* D,xrpcduplexrequest* R,xrpccallstatus Status)
{
    if(R->Finishing)return;
    R->Finishing=true;R->Status=Status;++D->Finishing;
    for(size_t I=0;I<R->Count;++I)if(R->Entries[I].Token.Serial){xrpcpendingcall Taken;
        xrpcPendingTakeToken(D->Pending,R->Entries[I].Token,&Taken);R->Entries[I].Token.Serial=0;}
    if(R->Output){if(R->Started && !R->Sent)xrpcDuplexSeal(D,XRPC_CALL_TRANSPORT);
        xrpcDuplexUnqueue(D,R->Output);R->Output=NULL;}
}
static bool xrpcDuplexFinish(xrpcduplex* D,xrpcduplexrequest* R)
{
    if(!R->Finished){xrpccallfinish F=R->Responses && R->Batch?
        xrpcBatchCallFinish(R->Call,R->Responses,xrpcDuplexMap(R),R->Count,NULL):
        xrpcCallFinish(R->Call,R->Status,R->Responses,R->Responses?xrpcDuplexMap(R)[0]:0,NULL);
        if(F==XRPC_CALL_FINISH_ERROR)return false;
        R->Finished=true;
    }
    if(!xrpcCallDestroy(R->Call))return false;
    R->Call=NULL;
    xrpcDuplexDeadlineRemove(D,R);
    --D->Finishing;
    if(D->Scan==R)xrpcDuplexAdvanceScan(D);
    else if(D->ScanEnd==R)D->ScanEnd=R->Prev;
    if(R->Prev)R->Prev->Next=R->Next;else D->Head=R->Next;
    if(R->Next)R->Next->Prev=R->Prev;else D->Tail=R->Prev;
    xrtMutexLock(&D->Gate);
    --D->Operations;D->PendingCalls-=R->Replies;++D->Completed;
    xrtMutexUnlock(&D->Gate);
    xrpcResponsesRelease(R->Responses);xrpcDuplexDrop(R);return true;
}
static xrpcduplexadmission xrpcDuplexProtocol(xrpcduplex* D)
{++D->ProtocolFailures;xrpcDuplexSeal(D,XRPC_CALL_PROTOCOL);return XRPC_DUPLEX_PROTOCOL;}
/* Validated JSON string tokens only. Zero means outside our generated ID domain. */
static uint64 xrpcDuplexNumericId(xstrview Raw,size_t* Count)
{
    xrpcstringiterator It={Raw,1,{0},0,0};unsigned char Byte;
    uint64 Value=0;size_t Digits=0;
    while(xrpcStringNext(&It,&Byte)){
        if(Byte<'0' || Byte>'9' || (!Digits && Byte=='0') || Digits==20 || Value>(UINT64_MAX-(Byte-'0'))/10u)
            return 0;
        Value=Value*10u+(Byte-'0');++Digits;
    }
    if(Count)*Count=Digits;
    return Value;
}

static xstrview xrpcDuplexCorrelationId(const xrpcmessage* M,char* Text)
{
    if(M->Id.Kind!=XRPC_VALUE_STRING)return XRT_STR_LITERAL("null");
    xstrview Raw=M->Id.Raw;size_t Digits=0;
    uint64 Value=xrpcDuplexNumericId(Raw,&Digits);
    if(!Value)return XRT_STR_LITERAL("null");
    return Raw.Size==Digits+2u?Raw:xrpcDuplexId(Value,Text);
}

typedef enum xrpcduplexcontrol {
    XRPC_CONTROL_NONE=0,XRPC_CONTROL_CANCEL_OFFER,XRPC_CONTROL_ABANDON,
    XRPC_CONTROL_DEADLINE_OFFER,XRPC_CONTROL_DEADLINE
} xrpcduplexcontrol;

static xrpcduplexadmission xrpcDuplexResponse(xrpcduplex* D,xstrview Input,const xrpcdocument* Decoded)
{
    const xrpcdecodeconfig* Decode=&D->Config.Response.Decode;
    xrpcdocument Document;
    if(!Decoded){
        xrpcdecodestatus S=xrpcDecode(Input,Decode,D->Scratch,Decode->MaxMessages,&Document);
        if(S==XRPC_DECODE_LOCAL_ERROR)return XRPC_DUPLEX_ERROR;
        if(S==XRPC_DECODE_PARSE_ERROR || Document.InvalidCount)return xrpcDuplexProtocol(D);
        Decoded=&Document;
    }

    size_t Count=Decoded->Count;
    for(size_t N=0;N<Count;++N)
        D->CorrelationIds[N]=xrpcDuplexCorrelationId(D->Scratch+N,D->CorrelationText+N*24u);
    if(!xrpcPendingProbeBatch(D->Pending,D->CorrelationIds,Count,D->Resolutions))return XRPC_DUPLEX_ERROR;
    xrpcduplexrequest* R=NULL;size_t Matches=0;
    for(size_t N=0;N<Count;++N)if(D->Resolutions[N].Status==XRPC_PENDING_MATCHED){
        xrpcduplexentry* E=D->Resolutions[N].Call.User;
        if(!R){R=E->Request;for(size_t K=0;K<R->Count;++K)xrpcDuplexMap(R)[K]=SIZE_MAX;}
        if(E->Request!=R || !R->Sent || R->Finishing || Decoded->Batch!=R->Batch ||
            xrpcDuplexMap(R)[E->Index]!=SIZE_MAX)return xrpcDuplexProtocol(D);
        xrpcDuplexMap(R)[E->Index]=N;++Matches;
    }
    if(!R){D->UnknownResponses+=Count;return XRPC_DUPLEX_ACCEPTED;}
    if(Matches!=Count || Matches!=R->Replies)return xrpcDuplexProtocol(D);

    xrpcresponses* Responses=NULL;
    xrpcresponsestatus S=xrpcResponsesReadQuota(Input,&D->Config.Response,D->Scratch,
        Decode->MaxMessages,Decoded,&Responses,D->Results);
    if(S!=XRPC_RESPONSE_DONE)return S==XRPC_RESPONSE_LOCAL_ERROR?XRPC_DUPLEX_ERROR:xrpcDuplexProtocol(D);
    if(!xrpcPendingTakeBatch(D->Pending,D->CorrelationIds,Count,D->Resolutions)){xrpcResponsesRelease(Responses);return XRPC_DUPLEX_ERROR;}
    for(size_t N=0;N<Count;++N){xrpcduplexentry* E=D->Resolutions[N].Call.User;E->Token.Serial=0;}
    R->Responses=Responses;
    size_t At=xrpcDuplexMap(R)[0];xrpccallstatus Status=R->Batch?XRPC_CALL_BATCH_DONE:
        D->Scratch[At].Kind==XRPC_MESSAGE_ERROR?XRPC_CALL_REMOTE_ERROR:XRPC_CALL_OK;
    xrpcDuplexPrepareFinish(D,R,Status);if(!xrpcDuplexFinish(D,R))return XRPC_DUPLEX_ERROR;
    return XRPC_DUPLEX_ACCEPTED;
}
/* The fixed outer grammar lets us validate only the metadata here and run
 * the untouched inner document through the ordinary bounded duplex decoder. */
static xstrview xrpcDuplexHostId(const xrpcmessage* Message)
{
    if (!Message) return XRT_STR_LITERAL("null");
    if (Message->Kind == XRPC_MESSAGE_NOTIFICATION) return (xstrview){0};
    if (Message->Kind == XRPC_MESSAGE_CALL) return Message->Id.Raw;
    return XRT_STR_LITERAL("null");
}

static void xrpcDuplexHostRetire(xrpcduplex* D, uint32 Slot, bool Completed)
{
    xrpcduplexhostslot* Entry = D->HostSlots + Slot - 1;
    if (Entry->Previous) D->HostSlots[Entry->Previous - 1].Next = Entry->Next;
    else D->HostHead = Entry->Next;
    if (Entry->Next) D->HostSlots[Entry->Next - 1].Previous = Entry->Previous;
    xrpcResultQuotaFree(Entry->Job);
    *Entry = (xrpcduplexhostslot){.Next = D->HostFree};
    D->HostFree = Slot;
    xrtMutexLock(&D->Gate);
    --D->Operations;
    --D->Incoming;
    if (Completed) ++D->Completed;
    xrtMutexUnlock(&D->Gate);
}

static xrpcduplexadmission xrpcDuplexHostReceive(xrpcduplex* D, xstrview Input,
    xbytesview Context, const xrpcdocument* Document)
{
    size_t Messages = Document ? Document->Count : 1;
    size_t Count = 0;
    size_t Bytes = sizeof(xrpcduplexhostjob);
    for (size_t I = 0; I < Messages; ++I) {
        xstrview Id = xrpcDuplexHostId(Document ? D->Scratch + I : NULL);
        if (!Id.Size) continue;
        if (Bytes > SIZE_MAX - sizeof(xstrview) || Id.Size > SIZE_MAX - Bytes - sizeof(xstrview))
            return XRPC_DUPLEX_OVERLOADED;
        Bytes += sizeof(xstrview) + Id.Size;
        ++Count;
    }
    xrtMutexLock(&D->Gate);
    if (D->Closing) {
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_CLOSED;
    }
    if (D->Operations == D->Config.MaxOperations || !D->HostFree ||
        D->HostSerial == (UINT64_MAX >> XRPC_DUPLEX_HOST_SLOT_BITS)) {
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_OVERLOADED;
    }
    bool Limited = false;
    xrpcduplexhostjob* Job = xrpcResultQuotaAlloc(D->Control, Bytes, &Limited);
    if (!Job) {
        if (Limited) ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        if (Limited) return XRPC_DUPLEX_OVERLOADED;
        xrpcDuplexReport(XERR_MEMORY, XRPC_DUPLEX_MEMORY, "RPC APP job allocation failed");
        return XRPC_DUPLEX_ERROR;
    }
    uint32 Slot = D->HostFree;
    xrpcduplexhostslot* Entry = D->HostSlots + Slot - 1;
    D->HostFree = Entry->Next;
    *Entry = (xrpcduplexhostslot){.Job = Job, .Next = D->HostHead};
    if (D->HostHead) D->HostSlots[D->HostHead - 1].Previous = Slot;
    D->HostHead = Slot;
    Job->Token = (++D->HostSerial << XRPC_DUPLEX_HOST_SLOT_BITS) | (Slot - 1u);
    Job->Count = Count;
    Job->Batch = Document && Document->Batch;
    ++D->Operations;
    ++D->Incoming;
    xrtMutexUnlock(&D->Gate);

    char* Copy = (char*)(Job->Ids + Count);
    size_t Index = 0;
    for (size_t I = 0; I < Messages; ++I) {
        xstrview Id = xrpcDuplexHostId(Document ? D->Scratch + I : NULL);
        if (!Id.Size) continue;
        memcpy(Copy, Id.Data, Id.Size);
        Job->Ids[Index++] = (xstrview){Copy, Id.Size};
        Copy += Id.Size;
    }
    if (!D->Config.OnRequest(Job->Token, Input, Context, Document,
        Document ? D->Scratch : NULL, D->Config.User)) {
        xrpcDuplexHostRetire(D, Slot, false);
        xrtMutexLock(&D->Gate);
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        return XRPC_DUPLEX_OVERLOADED;
    }
    xrtMutexLock(&D->Gate);
    ++D->Accepted;
    ++D->Received;
    xrtMutexUnlock(&D->Gate);
    return XRPC_DUPLEX_ACCEPTED;
}

XRPC_API xrpcduplexadmission xrpcDuplexReply(xrpcduplex* D, uint64 Token, xstrview Reply)
{
    if (!D || !Token || !xrpcDuplexSpan(D, Reply.Data, Reply.Size)) {
        xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "invalid RPC APP reply");
        return XRPC_DUPLEX_ERROR;
    }
    if (!xrpcDuplexEnter(D)) return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Status = XRPC_DUPLEX_ERROR;
    if (xrpcDuplexIsClosing(D)) {
        Status = XRPC_DUPLEX_CLOSED;
        goto Done;
    }
    uint32 Slot = (uint32)(Token & (XRPC_DUPLEX_HOST_SLOT_COUNT - 1u)) + 1u;
    if (!D->HostSlots || Slot > D->Config.MaxIncoming) goto Invalid;
    xrpcduplexhostjob* Job = D->HostSlots[Slot - 1].Job;
    if (!Job || Job->Token != Token || ((Job->Count == 0) != (Reply.Size == 0))) goto Invalid;
    if (Reply.Size) {
        xrpcdocument Document;
        if (xrpcDecode(Reply, &D->Config.Response.Decode, D->Scratch,
            D->Config.Response.Decode.MaxMessages, &Document) != XRPC_DECODE_DONE ||
            Document.InvalidCount || Document.Batch != Job->Batch || Document.Count != Job->Count)
            goto Invalid;
        for (size_t I = 0; I < Job->Count; ++I) {
            xstrview Id = D->Scratch[I].Id.Raw;
            if (Id.Size != Job->Ids[I].Size || memcmp(Id.Data, Job->Ids[I].Data, Id.Size))
                goto Invalid;
        }
        if (Reply.Size > SIZE_MAX - sizeof(xrpcduplexoutput) ||
            !xrpcResultQuotaFits(D->Wire, sizeof(xrpcduplexoutput) + Reply.Size)) {
            xrpcDuplexReport(XERR_RANGE, XRPC_DUPLEX_LIMIT, "RPC APP reply exceeds wire quota");
            goto Done;
        }
        bool Limited = false;
        xrpcduplexoutput* Output = xrpcDuplexOutputCreate(D, Reply.Size, Reply.Size, &Limited);
        if (!Output) {
            if (Limited) Status = XRPC_DUPLEX_OVERLOADED;
            else xrpcDuplexReport(XERR_MEMORY, XRPC_DUPLEX_MEMORY, "RPC APP reply allocation failed");
            goto Done;
        }
        memcpy(Output + 1, Reply.Data, Reply.Size);
        Output->Wire = (xstrview){(cstr)(Output + 1), Reply.Size};
        xrpcDuplexQueue(D, Output);
    }
    xrpcDuplexHostRetire(D, Slot, true);
    Status = XRPC_DUPLEX_ACCEPTED;
    goto Done;
Invalid:
    xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "RPC APP reply token, IDs or shape do not match");
Done:
    D->Busy = false;
    return Status;
}

XRPC_API xrpcduplexadmission xrpcDuplexReceive(xrpcduplex* D, xstrview Input, xbytesview Context)
{
    if (!D || !xrpcDuplexSpan(D, Input.Data, Input.Size) ||
        !xrpcDuplexSpan(D, Context.Data, Context.Size)) {
        xrpcDuplexReport(XERR_ARGUMENT, XRPC_DUPLEX_ARGUMENT, "invalid RPC duplex receive spans");
        return XRPC_DUPLEX_ERROR;
    }
    if (!xrpcDuplexEnter(D)) return XRPC_DUPLEX_ERROR;
    xrpcduplexadmission Status = XRPC_DUPLEX_CLOSED;
    if (xrpcDuplexIsClosing(D)) goto Done;

    xrpcdecodeconfig Decode = D->Config.Response.Decode;
    Decode.Mode = XRPC_DECODE_DUPLEX;
    xrpcdocument Document;
    xrpcdecodestatus Decoded = xrpcDecode(Input, &Decode, D->Scratch,
        Decode.MaxMessages, &Document);
    Status = XRPC_DUPLEX_ERROR;
    if (Decoded == XRPC_DECODE_LOCAL_ERROR) goto Done;

    bool Requests = false, Responses = false;
    if (Decoded == XRPC_DECODE_DONE) {
        for (size_t I = 0; I < Document.Count; ++I) {
            const xrpcmessage* M = D->Scratch + I;
            bool Reply = M->Kind == XRPC_MESSAGE_RESULT || M->Kind == XRPC_MESSAGE_ERROR ||
                M->Result.Kind != XRPC_VALUE_MISSING || M->Error.Kind != XRPC_VALUE_MISSING;
            Requests |= M->Method.Kind != XRPC_VALUE_MISSING || !Reply;
            Responses |= Reply;
        }
    }
    if (Requests && Responses) {
        Status = xrpcDuplexProtocol(D);
    } else if (Responses) {
        /* Response mode still handles envelopes that duplex mode rejects. */
        Status = xrpcDuplexResponse(D, Input, Document.InvalidCount ? NULL : &Document);
        if (Status == XRPC_DUPLEX_ACCEPTED) ++D->Received;
    } else if (D->Config.OnRequest) {
        Status = xrpcDuplexHostReceive(D, Input, Context,
            Decoded == XRPC_DECODE_DONE ? &Document : NULL);
    } else {
        xrtMutexLock(&D->Gate);
        ++D->Rejected;
        xrtMutexUnlock(&D->Gate);
        Status = XRPC_DUPLEX_OVERLOADED;
    }
Done:
    D->Busy = false;
    return Status;
}

static bool xrpcDuplexWorkSubmit(xrpcduplex* D,bool* OK)
{
    xrtMutexLock(&D->Gate);xrpcduplexrequest* R=D->SubmitHead;
    if(R){D->SubmitHead=R->SubmitNext;if(!D->SubmitHead)D->SubmitTail=NULL;R->SubmitNext=NULL;}
    xrtMutexUnlock(&D->Gate);if(!R)return false;
    R->Prev=D->Tail;if(R->Prev)R->Prev->Next=R;else D->Head=R;D->Tail=R;
    R->Deadline=xrpcCallDeadline(R->Call);
    if(isfinite(R->Deadline)){
        size_t Slot=D->DeadlineCount++;
        xrpcDuplexDeadlineUp(D,Slot,R);
    }
    if(xrpcDuplexIsClosing(D))xrpcDuplexPrepareFinish(D,R,D->Reason);
    else {xrpccallfinish F=xrpcCallPoll(R->Call);
        if(F==XRPC_CALL_FINISH_ERROR){*OK=false;return true;}
        if(F!=XRPC_CALL_PENDING)xrpcDuplexPrepareFinish(D,R,XRPC_CALL_CLOSED);
        else {
            for(size_t I=0;I<R->Count;++I)if(R->Entries[I].Id){char Id[24];
                xrpcpendingstatus P=xrpcPendingInsert(D->Pending,xrpcDuplexId(R->Entries[I].Id,Id),UINT64_MAX,R->Entries+I,&R->Entries[I].Token);
                if(P!=XRPC_PENDING_OK){xrpcDuplexPrepareFinish(D,R,P==XRPC_PENDING_ERROR?XRPC_CALL_PROTOCOL:XRPC_CALL_LIMIT);break;}}
            if(!R->Finishing)xrpcDuplexQueue(D,R->Output);
        }
    }
    if(R->Finishing)*OK=xrpcDuplexFinish(D,R);
    else D->More=true; /* Added outside this turn's bounded scan cohort. */
    return true;
}
static bool xrpcDuplexWorkCall(xrpcduplex* D,bool* OK,double Now)
{
    xrpcduplexrequest* Due=D->DeadlineCount?D->Deadlines[0]:NULL;
    if(Due && Due->Deadline>Now)Due=NULL;
    /* Give an expired call a turn without waiting for its position in the
     * cancellation cohort. Raw token cancellation still needs that cohort. */
    xrpcduplexrequest* R=Due && (!D->PreferScan || !D->Scan)?Due:D->Scan;
    if(!R)return false;
    D->PreferScan=R!=D->Scan && D->Scan!=NULL;
    if(R==D->Scan)xrpcDuplexAdvanceScan(D);
    if(!R->Finishing){if(xrpcDuplexIsClosing(D))xrpcDuplexPrepareFinish(D,R,D->Reason);
        else {xrpccallfinish P=xrpcCallPoll(R->Call);if(P==XRPC_CALL_FINISH_ERROR){*OK=false;return true;}
            if(P!=XRPC_CALL_PENDING)xrpcDuplexPrepareFinish(D,R,XRPC_CALL_CLOSED);}}
    if(R->Finishing)*OK=xrpcDuplexFinish(D,R);
    return true;
}
static bool xrpcDuplexWorkClose(xrpcduplex* D)
{
    if(!xrpcDuplexIsClosing(D))return false;
    if (D->HostHead) {
        xrpcDuplexHostRetire(D, D->HostHead, true);
        return true;
    }
    if(D->OutputHead){if(D->OutputHead->Request)D->OutputHead->Request->Output=NULL;
        xrpcDuplexUnqueue(D,D->OutputHead);return true;}
    return false;
}
XRPC_API bool xrpcDuplexStep(xrpcduplex* D)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=true;D->More=false;
    if(!D->Scan){D->Scan=D->Head;D->ScanEnd=D->Tail;}
    double Now=D->DeadlineCount?xrtTimer():-INFINITY;
    /* Rotate between admission, deadlines and close, even with MaxWork=1. */
    for(size_t Work=0;Work<D->Config.MaxWork && OK;++Work){bool Worked=false;
        for(unsigned Attempt=0;Attempt<3 && !Worked;++Attempt){unsigned Turn=D->Turn++%3;
            if(Turn==0)Worked=xrpcDuplexWorkSubmit(D,&OK);
            else if(Turn==1)Worked=xrpcDuplexWorkCall(D,&OK,Now);
            else Worked=xrpcDuplexWorkClose(D);
        }
        if(!Worked)break;
    }
    D->More|=D->Scan!=NULL || (D->DeadlineCount && D->Deadlines[0]->Deadline<=Now);
    xrtMutexLock(&D->Gate);
    D->More|=D->SubmitHead!=NULL || (D->Closing && (D->OutputHead || D->HostHead));
    xrtMutexUnlock(&D->Gate);
    D->Busy=false;return OK;
}

XRPC_API int xrpcDuplexNext(xrpcduplex* D,xrpcduplexoutput** Output,uint64* Token)
{
    if(!D || !Output || !Token || !xrpcDuplexSpan(D,Output,sizeof(*Output)) || !xrpcDuplexSpan(D,Token,sizeof(*Token)) ||
       xrtMemRangesOverlap(Output,sizeof(*Output),Token,sizeof(*Token))){xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex output selection spans");return -1;}
    if(!xrpcDuplexEnter(D))return -1;
    int Result=0;
    if(D->Selection){xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex already has a selected output");Result=-1;}
    else if(!xrpcDuplexIsClosing(D) && D->OutputHead){
        if(D->SelectionSerial==UINT64_MAX){xrpcDuplexSeal(D,XRPC_CALL_LIMIT);xrpcDuplexReport(XERR_RANGE,XRPC_DUPLEX_LIMIT,"RPC duplex selection serial exhausted");Result=-1;}
        else if(!xrpcDuplexOutputRetain(D->OutputHead))Result=-1;
        else {
            D->Selection=D->OutputHead;
            D->SelectedToken=++D->SelectionSerial;*Output=D->Selection;*Token=D->SelectedToken;Result=1;
        }
    }
    D->Busy=false;return Result;
}
XRPC_API bool xrpcDuplexStarted(xrpcduplex* D,uint64 Token)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=false;
    if(Token && Token==D->SelectedToken && D->Selection){
        xrtMutexLock(&D->Gate);
        bool Closing=D->Closing;
        xrtMutexUnlock(&D->Gate);
        xrpcduplexrequest* R=D->Selection->Request;
        if(!Closing){
            if(!R)OK=true;
            else if(!R->Started && xrpcCallPoll(R->Call)==XRPC_CALL_PENDING){
                OK=xrpcCallMarkSent(R->Call);R->Started=OK;
            }
            else OK=R->Started;
        }
    }
    if(OK)D->Selection->Started=true;
    D->Busy=false;return OK;
}
XRPC_API bool xrpcDuplexSent(xrpcduplex* D,uint64 Token)
{
    if(!xrpcDuplexEnter(D))return false;
    bool OK=false;xrpcduplexoutput* O=D->Selection;
    if(Token && Token==D->SelectedToken && O && !xrpcDuplexIsClosing(D)){
        xrpcduplexrequest* R=O->Request;
        if(O->Started && (!R || R->Started)){if(R){R->Sent=true;R->Output=NULL;}
            xrpcDuplexUnqueue(D,O);OK=true;
            if(R && !R->Replies){
                xrpcDuplexPrepareFinish(D,R,XRPC_CALL_NOTIFY_DONE);
                OK=xrpcDuplexFinish(D,R);
            }}
    }
    D->Busy=false;return OK;
}
XRPC_API bool xrpcDuplexInfo(xrpcduplex* D,xrpcduplexinfo* I)
{
    if(!D || !I || !xrpcDuplexSpan(D,I,sizeof(*I)) || I->Size!=sizeof(*I))return xrpcDuplexReport(XERR_ARGUMENT,XRPC_DUPLEX_ARGUMENT,"invalid RPC duplex info output");
    xrpcduplexinfo V={0};V.Size=sizeof(V);V.NextDeadline=INFINITY;
    xrtMutexLock(&D->Gate);V.OwnedBytes=D->FixedBytes;V.Closing=D->Closing;V.Reason=D->Reason;V.Operations=D->Operations;V.PendingCalls=D->PendingCalls;
    V.Incoming=D->Incoming;V.Constructing=D->Constructing;V.TemporaryBytes=D->TemporaryBytes;V.PeakTemporaryBytes=D->PeakTemporaryBytes;V.Accepted=D->Accepted;V.Rejected=D->Rejected;V.Completed=D->Completed;
    bool Submit=D->SubmitHead!=NULL;
    xrtMutexUnlock(&D->Gate);V.Received=D->Received;V.UnknownResponses=D->UnknownResponses;V.ProtocolFailures=D->ProtocolFailures;
    xrpcResultQuotaInfo(D->Control,&V.ControlBlocks,&V.ControlBytes,&V.ControlRefusals);
    xrpcResultQuotaInfo(D->Wire,&V.Outputs,&V.OutputBytes,&V.OutputRefusals);xrpcResultQuotaInfo(D->Results,&V.Results,&V.ResultBytes,&V.ResultRefusals);
    V.Selected=D->Selection!=NULL;V.Started=D->Selection && D->Selection->Started;
    V.Drained=V.Closing && !V.Operations && !D->OutputHead && !D->Selection && xrtAtomic32Load(&D->References,XMEMORY_ACQUIRE)==1;
    V.More=D->More || Submit;
    if(D->DeadlineCount)V.NextDeadline=D->Deadlines[0]->Deadline;
    if(V.More || D->Finishing)V.NextDeadline=xrtTimer();
    else if(V.Closing && !V.Drained && !V.Operations){
        double Retry=xrtTimer()+.001;
        if(Retry<V.NextDeadline)V.NextDeadline=Retry; /* recheck a final constructor pin */
    }
    *I=V;return true;
}
XRPC_API bool xrpcDuplexDestroy(xrpcduplex* D)
{
    if(!D)return true;
    if(D->Busy)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex destruction during owner callback");
    xrpcduplexinfo I={0};I.Size=sizeof(I);if(!xrpcDuplexInfo(D,&I))return false;
    if(!I.Drained)return xrpcDuplexReport(XERR_STATE,XRPC_DUPLEX_STATE,"RPC duplex close duties still live");
    xrpcPendingDestroy(D->Pending);xrpcResultQuotaRelease(D->Control);xrpcResultQuotaRelease(D->Wire);xrpcResultQuotaRelease(D->Results);
    xrtMutexUnit(&D->Gate);xrtFree(D);return true;
}
#endif
#endif

#endif
