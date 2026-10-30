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
#ifndef XRPC_MODULE_XRPC_HTTP_CLIENT
#define XRPC_MODULE_XRPC_HTTP_CLIENT
#endif
#ifndef XRPC_MODULE_XRPC_SERVER
#define XRPC_MODULE_XRPC_SERVER
#endif
#endif

/* xrpc_server 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_SERVER)
#ifndef XRPC_FEATURE_XRPC_SERVER
#define XRPC_FEATURE_XRPC_SERVER
#endif
#ifndef XRPC_MODULE_XRPC_DISPATCH
#define XRPC_MODULE_XRPC_DISPATCH
#endif
#ifndef XRT_MODULE_EXECUTOR
#define XRT_MODULE_EXECUTOR
#endif
#ifndef XRT_MODULE_FUTURE
#define XRT_MODULE_FUTURE
#endif
#endif

/* xrpc_http_client 及其直接依赖。 */
#if defined(XRPC_MODULE_ALL) || defined(XRPC_MODULE_XRPC_HTTP_CLIENT)
#ifndef XRPC_FEATURE_XRPC_HTTP_CLIENT
#define XRPC_FEATURE_XRPC_HTTP_CLIENT
#endif
#ifndef XRPC_MODULE_XRPC_CALL
#define XRPC_MODULE_XRPC_CALL
#endif
#ifndef XRPC_MODULE_XRPC_ENCODE
#define XRPC_MODULE_XRPC_ENCODE
#endif
#ifndef XRPC_MODULE_XRPC_ID
#define XRPC_MODULE_XRPC_ID
#endif
#ifndef XRT_MODULE_NET_TCP
#define XRT_MODULE_NET_TCP
#endif
#ifndef XRT_MODULE_HTTP1_BODY
#define XRT_MODULE_HTTP1_BODY
#endif
#ifndef XRT_MODULE_HTTP_HOST
#define XRT_MODULE_HTTP_HOST
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
/* public: extlibs/xrpc/include/xrpc/response.h */
/* ========================================================================== */

#ifndef XRPC_RESPONSE_H
#define XRPC_RESPONSE_H
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
/* public: extlibs/xrpc/include/xrpc/http_client.h */
/* ========================================================================== */

#ifndef XRPC_HTTP_CLIENT_H
#define XRPC_HTTP_CLIENT_H
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT) && \
    (!defined(XRPC_FEATURE_XRPC_CALL) || !defined(XRPC_FEATURE_XRPC_ENCODE) || !defined(XRPC_FEATURE_XRPC_ID) || \
     !defined(XRT_FEATURE_NET_TCP) || !defined(XRT_FEATURE_HTTP1_BODY) || !defined(XRT_FEATURE_HTTP_HOST))
#error "xrpc HTTP client requires call, encode, TCP and HTTP/1 body"
#endif
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
typedef struct xrpchttpclient xrpchttpclient;
typedef enum xrpchttpstate {
    XRPC_HTTP_CONNECTING=0, XRPC_HTTP_OPEN, XRPC_HTTP_CLOSING, XRPC_HTTP_CLOSED
} xrpchttpstate;
typedef enum xrpcadmission {
    XRPC_ADMISSION_ERROR=-1, XRPC_ADMISSION_ACCEPTED=0,
    XRPC_ADMISSION_OVERLOADED, XRPC_ADMISSION_CLOSED
} xrpcadmission;
typedef enum xrpchttperror {
    XRPC_HTTP_ARGUMENT=9001, XRPC_HTTP_STATE, XRPC_HTTP_ALLOCATION,
    XRPC_HTTP_RESPONSE, XRPC_HTTP_ID_EXHAUSTED
} xrpchttperror;
typedef struct xrpchttpconfig {
    size_t Size;
    xnetstreamconfig Stream;
    xhttp1limits Head;
    xhttp1bodylimits Body;
    xrpcresponseconfig Responses;
    size_t MaxRequests, MaxRequestBytes, MaxRequestBytesTotal;
    size_t WorkRequests, ReadBytes, MaxResponseWireBytes;
    size_t MaxBatchItems, MaxBatchBytesTotal;
    uint32 MaxInformational;
} xrpchttpconfig;
typedef struct xrpchttpinfo {
    size_t Size;
    xrpchttpstate State;
    size_t Requests, RequestBytes, BatchBytes;
    uint64 Accepted, Rejected, Completed;
    xrpccallstatus CloseStatus;
} xrpchttpinfo;
typedef struct xrpchttpitem {
    xrpcmessagekind Kind; /* CALL or NOTIFICATION only. */
    xstrview Method, Params;
} xrpchttpitem;
XRT_EXTERN_C_BEGIN
XRPC_API bool xrpcHttpConfigInit(xrpchttpconfig* Config);
/* Numeric, plain TCP endpoint; existing running engine, one fixed worker.
 * Host is an explicit HTTP authority; Target is origin-form. They are copied.
 * One active HTTP/1 exchange, bounded concurrent submissions, persistent TCP.
 * No implicit DNS, TLS, redirects, authentication or replay. Close then Release.
 * The engine remains pinned through physical posts/timers/cancellation tails.
 * Ordinary calls, notifications and array-shaped batches share this transport;
 * DNS, TLS and full transport policy remain product work. */
XRPC_API bool xrpcHttpConnect(xnetengine* Engine, const xnetaddr* Remote,
    uint64 Affinity, xstrview Host, xstrview Target,
    const xrpchttpconfig* Config, xrpchttpclient** Client);
XRPC_API bool xrpcHttpRetain(xrpchttpclient* Client);
XRPC_API void xrpcHttpRelease(xrpchttpclient* Client);
/* Caller-thread input validation/encoding and copy; inputs may be reused on
 * return. Params is absent or a raw JSON array/object; Method is decoded UTF-8.
 * Each accepted call gets a fresh nonreused decimal-string ID and an independent
 * xrpccallresult Future. Deadline begins at API entry, including construction,
 * queue and connection setup. Failure/admission refusal leaves Future unchanged.
 * Future cancellation posts owner work. Raw token requests alone do not wake.
 * Sent timeout/cancel retires the connection; queued unsent calls are closed,
 * never replayed. A validated matched reply is REPLIED; non-2xx HTTP status is
 * not itself an RPC error when its complete body is a valid matching response.
 * No business callbacks are installed; Future Watches execute on the worker
 * and must not block. Results do not retain this client/engine. */
XRPC_API xrpcadmission xrpcHttpCallAsync(xrpchttpclient* Client,
    xstrview Method, xstrview Params, int64 Timeout, xfuture** Future);
/* No RPC id or reply. Empty 2xx HTTP completion yields NOTIFY_DONE with
 * MAY_HAVE_EXECUTED, never business confirmation. A nonempty successful HTTP
 * body violates this local binding. Non-2xx yields HTTP_ERROR; optional Cause
 * domain xrt.rpc.http carries its status when a diagnostic can be allocated. */
XRPC_API xrpcadmission xrpcHttpNotifyAsync(xrpchttpclient* Client,
    xstrview Method, xstrview Params, int64 Timeout, xfuture** Future);
/* One array-shaped HTTP request, including a one-item batch. One atomic
 * admission and one deadline/cancellation unit cover every item. The Future
 * always contains xrpcbatchcallresult. BATCH_DONE means all call replies were
 * validated/correlated; inspect each mapped RESULT/ERROR for business outcome.
 * Notifications map to SIZE_MAX. All-notification batches use NOTIFY_DONE.
 * Bad/missing/duplicate/notification/unknown IDs publish no partial replies.
 * MaxBatchItems and aggregate in-flight BatchBytes bound metadata, separately
 * from encoded-wire limits. Retained completed result maps/snapshots remain
 * caller-owned and do not keep client/engine refs. Outputs/inputs cannot overlap. */
XRPC_API xrpcadmission xrpcHttpBatchAsync(xrpchttpclient* Client,
    const xrpchttpitem* Items, size_t Count, int64 Timeout, xfuture** Future);
XRPC_API bool xrpcHttpInfo(xrpchttpclient* Client, xrpchttpinfo* Info);
XRPC_API bool xrpcHttpClose(xrpchttpclient* Client);
/* Independent observer; resolves after request duties and stream retire. The
 * publishing stack's mechanical client-reference tail can follow; joining the
 * engine retires that stack. Dropping this observer does not close the client.
 * No blocking wait facade here. */
XRPC_API xfuture* xrpcHttpClosed(xrpchttpclient* Client);
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* public: extlibs/xrpc/include/xrpc/server.h */
/* ========================================================================== */

#ifndef XRPC_SERVER_H
#define XRPC_SERVER_H
#if defined(XRPC_FEATURE_XRPC_SERVER) && \
    (!defined(XRPC_FEATURE_XRPC_DISPATCH) || !defined(XRT_FEATURE_EXECUTOR) || !defined(XRT_FEATURE_FUTURE))
#error "xrpc server requires dispatch, executor and future"
#endif
#if defined(XRPC_FEATURE_XRPC_SERVER)
typedef struct xrpcserver xrpcserver;
typedef enum xrpcserverstatus {
    XRPC_SERVER_DONE=0, XRPC_SERVER_CANCELLED, XRPC_SERVER_TIMEOUT,
    XRPC_SERVER_MEMORY, XRPC_SERVER_LIMIT, XRPC_SERVER_LOCAL_ERROR
} xrpcserverstatus;
typedef enum xrpcserveradmission {
    XRPC_SERVER_ADMISSION_ERROR=-1, XRPC_SERVER_ACCEPTED=0,
    XRPC_SERVER_OVERLOADED, XRPC_SERVER_CLOSED
} xrpcserveradmission;
typedef enum xrpcservererror {
    XRPC_SERVER_ARGUMENT=10001, XRPC_SERVER_STATE, XRPC_SERVER_ALLOCATION,
    XRPC_SERVER_RESOURCE
} xrpcservererror;
typedef struct xrpcservercontext {
    xcancel* Cancel; /* Borrowed. Poll for request or server cancellation. */
    double Deadline; /* Absolute xrtTimer seconds; may be INFINITY. */
} xrpcservercontext;
/* Runs on a Core executor worker, never the submitting/I/O thread. Request,
 * Reply and Context are borrowed through this call only. Reply is synchronous;
 * do not defer it. Return/Reply rules match xrpcdispatchcall. Concurrent jobs
 * may enter Call/User concurrently. Poll cancellation/deadline during long work;
 * cancellation cannot undo business effects or forcibly interrupt a handler. */
typedef bool (*xrpcservercall)(const xrpcmessage* Request,xrpcreply* Reply,
    const xrpcservercontext* Context,ptr User);
typedef struct xrpcserverconfig {
    size_t Size;
    xexecutorconfig Executor;
    xrpcdispatchconfig Dispatch;
    size_t MaxOperations,MaxRequestBytesTotal,MaxResults,MaxResultBytes,MaxWorkspaceBytes;
    size_t InitialOutputBytes;
    xrpcservercall Call;
    ptr User; /* Borrowed code/context; alive through successful Destroy. */
} xrpcserverconfig;
typedef struct xrpcserverinfo {
    size_t Size;
    bool Accepting,Cancelling;
    size_t Operations,RequestBytes,Results,ResultBytes,WorkspaceBytes,Running;
    uint64 Accepted,Rejected,Completed,PublicationFailures;
} xrpcserverinfo;
typedef struct xrpcserverresult {
    xrpcserverstatus Status;
    bool MayHaveExecuted;
    xrpcdispatchinfo Dispatch;
    xstrview Wire;
    const xerror* Cause;
} xrpcserverresult;
XRT_EXTERN_C_BEGIN
/* Two workers, 64 queue slots each; 128 live operations, 8 MiB owned request
 * storage, 1024 pending/retained results, 8 MiB result storage, 4 MiB workspace.
 * Output starts at 256 bytes and grows within Dispatch/aggregate bounds. */
XRPC_API bool xrpcServerConfigInit(xrpcserverconfig* Config);
/* Owns a bounded Core executor and preallocated per-worker dispatch scratch.
 * Config is copied; no transport/listener or shared runtime is created. */
XRPC_API bool xrpcServerCreate(const xrpcserverconfig* Config,xrpcserver** Server);
/* Concurrent bounded admission. Input is copied before return and fully decoded
 * on a worker before any handler. One document (including a mixed batch) is one
 * job, deadline and cooperative cancel unit. Timeout starts at entry. Raw token
 * or Future cancellation is sampled before/after handlers and output emission;
 * a queued stop resolves when its worker starts or server cancellation drops it.
 * Refusal/error leaves Future unchanged. No replay or business rollback.
 * Normal terminal outcomes RESOLVE with xrpcserverresult. DONE includes valid
 * RPC errors/parse errors, so inspect Wire/Dispatch. Notification-only documents
 * produce empty Wire. On local failure/stop, partial output is discarded and
 * MayHaveExecuted records actual handler entry, not successful business effects.
 * Result storage includes pending AND caller-retained result headers/capacities.
 * Growth can hit an aggregate limit/OOM after business effects; it is a typed
 * local outcome. Retained Futures keep a quota ledger, not server/worker refs.
 * A checked Core publication refusal increments PublicationFailures and closes
 * the Future without a typed value; callers must also inspect Future state.
 * Completion Watches run on worker/cancel-drop threads and must not block them. */
XRPC_API xrpcserveradmission xrpcServerSubmit(xrpcserver* Server,xstrview Input,
    int64 Timeout,xfuture** Future);
XRPC_API bool xrpcServerInfo(xrpcserver* Server,xrpcserverinfo* Info);
/* Concurrent nonblocking admission close/drain, or cooperative cancellation plus
 * queued-job drop. User cancel watches can run on the Cancel caller, outside
 * native locks. Running handlers remain responsible for returning. */
XRPC_API bool xrpcServerClose(xrpcserver* Server);
XRPC_API bool xrpcServerCancel(xrpcserver* Server);
/* Closes admission and waits for construction/work/publication/drop tails using
 * one deadline. Self-worker/cleanup waits fail. Retained results do not delay it.
 * This drains duties; successful Destroy additionally joins physical workers. */
XRPC_API xwaitresult xrpcServerWaitFor(xrpcserver* Server,int64 Timeout);
/* NULL succeeds. Stop concurrent API entries first. Success joins and consumes
 * the unique owner; failure leaves it for retry. Cannot destroy on own worker or
 * completion/cancel-drop callback. Result Futures survive successful Destroy.
 * Opaque handlers/jobs/results are not collector admission. */
XRPC_API bool xrpcServerDestroy(xrpcserver* Server);
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
#if defined(XRPC_FEATURE_XRPC) && (!defined(XRPC_FEATURE_XRPC_RESPONSE) || !defined(XRPC_FEATURE_XRPC_RECEIVE))
#error "XRPC_FEATURE_XRPC requires response and receive"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_CALL)
#error "XRPC_FEATURE_XRPC requires call"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
#error "XRPC_FEATURE_XRPC requires http_client"
#endif
#if defined(XRPC_FEATURE_XRPC) && !defined(XRPC_FEATURE_XRPC_SERVER)
#error "XRPC_FEATURE_XRPC requires server"
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
/* internal: extlibs/xrpc/src/internal/xrpc_string.h */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_METHODS) || \
	defined(XRPC_FEATURE_XRPC_ID)
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

#if defined(XRPC_FEATURE_XRPC_CALL)
#ifndef XRPC_CALL_INTERNAL_H
#define XRPC_CALL_INTERNAL_H
#if defined(XRPC_FEATURE_XRPC_CALL)
/* Product-internal constructor shares an API-entry monotonic origin without
 * quantizing away pre-admission/encoding time. No new public ABI. */
bool xrpcCallCreateEntered(const xrpccallconfig* Config, xrpccall** Call,
    xfuture** Future, double Entered);
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config, size_t Count,
    xrpccall** Call, xfuture** Future, double Entered);
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

XRPC_API bool xrpcReplyAbort(xrpcreply* Reply, const xerror* Error)
{
	if (Reply == NULL || !xrtMemRangeValid(Reply, sizeof(*Reply)))
		return xrpcDispatchReport(XERR_ARGUMENT, XRPC_DISPATCH_ARGUMENT, "invalid RPC abort handle");
	xrpcdispatchcontext* Context = Reply->Context;
	if (Context->Failed) { xrtSetError(Context->Failure); return false; }
	if (!Reply->Active)
		return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_STATE, "RPC abort handle is inactive");
	if (Error == NULL)
		return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_ABORTED, "RPC handler aborted dispatch locally");
	if (Error != xrtGetError()) {
		xerror* Owned = xrtErrorRef(Error);
		if (Owned == NULL)
			return xrpcDispatchFail(Context, XERR_STATE, XRPC_DISPATCH_ABORTED, "RPC abort diagnostic cannot be retained");
		xrtSetErrorTake(Owned);
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
XRPC_API bool xrpcPendingTakeBatch(xrpcpending* P,const xstrview* Ids,size_t Count,xrpcpendingresolution* Resolutions)
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
	/* All failable work is complete. No insert/callback can reuse a slot here. */
	for(size_t Index=0;Index<Count;++Index)if(Resolutions[Index].Status==XRPC_PENDING_MATCHED){
		xrpcpendingtoken Token=Resolutions[Index].Call.Token;uint32 Slot=(uint32)(Token.Slot+1u);
		if(xrpcPendingEntry(P,Slot)->Serial==Token.Serial)xrpcPendingTake(P,Slot,&Resolutions[Index].Call);
		else Resolutions[Index]=(xrpcpendingresolution){XRPC_PENDING_REPEATED,{{0,0},0,NULL}};
	}
	return true;
Argument:
	xrpcPendingReport(XERR_ARGUMENT,XRPC_PENDING_ARGUMENT,"invalid RPC pending batch spans/overlap");return false;
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
	bool Batch;
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
XRPC_API xrpcresponsestatus xrpcResponsesRead(xstrview Input,const xrpcresponseconfig* Config,
	xrpcmessage* Scratch,size_t Capacity,xrpcresponses** Result)
{
	if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || !Capacity || !Scratch || Capacity>SIZE_MAX/sizeof(*Scratch) ||
		!xrtMemRangeValid(Scratch,Capacity*sizeof(*Scratch)) || !xrtMemRangeValid(Input.Data,Input.Size) ||
		(Config && !xrtMemRangeValid(Config,sizeof(*Config))))goto Argument;
	if(xrtMemRangesOverlap(Result,sizeof(*Result),Input.Data,Input.Size) ||
		xrtMemRangesOverlap(Result,sizeof(*Result),Scratch,Capacity*sizeof(*Scratch)) || (Config &&
		(xrtMemRangesOverlap(Result,sizeof(*Result),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Scratch,Capacity*sizeof(*Scratch),Config,sizeof(*Config)))))goto Argument;
	xrpcresponseconfig C;if(Config)C=*Config;else xrpcResponseConfigInit(&C);
	if(C.Size!=sizeof(C) || !C.MaxOwnedBytes || C.Decode.Mode!=XRPC_DECODE_RESPONSES)goto Argument;
	xrpcdocument Document;xrpcdecodestatus Status=xrpcDecode(Input,&C.Decode,Scratch,Capacity,&Document);
	if(Status==XRPC_DECODE_LOCAL_ERROR)return XRPC_RESPONSE_LOCAL_ERROR;
	if(Status==XRPC_DECODE_PARSE_ERROR)return XRPC_RESPONSE_PARSE_ERROR;
	if(Document.InvalidCount){xrpcResponseReport(XERR_VALUE,XRPC_RESPONSE_ENVELOPE,"invalid RPC response envelope/batch");
		return xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_RESPONSE_LOCAL_ERROR:XRPC_RESPONSE_INVALID;}
	size_t Bytes=sizeof(xrpcresponses);
	if(Document.Count>(SIZE_MAX-Bytes)/(sizeof(xrpcmessage)+sizeof(xstrview)))goto Limit;
	Bytes+=Document.Count*(sizeof(xrpcmessage)+sizeof(xstrview));
	if(Input.Size>SIZE_MAX-Bytes || Bytes+Input.Size>C.MaxOwnedBytes)goto Limit;
	Bytes+=Input.Size;xrpcresponses* R=xrtMalloc(Bytes);
	if(!R){xrpcResponseReport(XERR_MEMORY,XRPC_RESPONSE_MEMORY,"RPC response snapshot allocation failed");return XRPC_RESPONSE_LOCAL_ERROR;}
	xrtAtomic32Init(&R->References,1);R->Batch=Document.Batch;R->Count=Document.Count;R->OwnedBytes=Bytes;
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
XRPC_API bool xrpcResponsesRetain(xrpcresponses* Responses)
{
	if(!Responses)return xrpcResponseReport(XERR_ARGUMENT,XRPC_RESPONSE_ARGUMENT,"RPC responses are NULL");
	uint32 Count=xrtAtomic32Load(&Responses->References,XMEMORY_RELAXED);
	for(;;){if(Count==UINT32_MAX)return xrpcResponseReport(XERR_RANGE,XRPC_RESPONSE_LIMIT,"RPC response references overflow");
		if(xrtAtomic32CompareExchange(&Responses->References,&Count,Count+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
}
XRPC_API void xrpcResponsesRelease(xrpcresponses* Responses)
{ if(Responses && xrtAtomic32FetchSub(&Responses->References,1,XMEMORY_ACQ_REL)==1)xrtFree(Responses); }
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
	bool ContextOwned, Destroying, Batch;
};
static bool xrpcCallReport(xerrkind Kind,xrpccallerror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static void xrpcCallResultDrop(ptr Value,ptr User)
{
	(void)User;xrpccallresult* R=Value;
	xrpcResponsesRelease(R->Responses);xrtErrorFree((xerror*)R->Cause);xrtFree(R);
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
static bool xrpcCallConstruct(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future,double Began,size_t Count)
{
	if(!Call || !Future || !xrtMemRangeValid(Call,sizeof(*Call)) || !xrtMemRangeValid(Future,sizeof(*Future)) ||
		xrtMemRangesOverlap(Call,sizeof(*Call),Future,sizeof(*Future)) || (Config &&
		(!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Call,sizeof(*Call),Config,sizeof(*Config)) ||
		 xrtMemRangesOverlap(Future,sizeof(*Future),Config,sizeof(*Config)))))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call constructor spans/overlap");
	xrpccallconfig Limits;if(Config)Limits=*Config;else xrpcCallConfigInit(&Limits);
	if(Limits.Size!=sizeof(Limits) || (Limits.Timeout<0 && Limits.Timeout!=XRT_WAIT_FOREVER))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC call configuration");
	xrpccall* C=xrtMalloc(sizeof(*C));if(!C)return xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call allocation failed");
	*C=(xrpccall){0};xrtAtomic32Init(&C->References,2);C->Config=Limits;
	C->Deadline=Limits.Timeout==XRT_WAIT_FOREVER?INFINITY:Limits.Timeout==0?Began:
		nextafter(Began+(double)(Limits.Timeout/1000)+(double)(Limits.Timeout%1000)*.001,INFINITY);
	C->Batch=Count!=0;size_t ResultBytes=Count?sizeof(xrpcbatchcallresult)+Count*sizeof(size_t):sizeof(*C->Result);
	xfuture* Observer=NULL;C->Result=xrtMalloc(ResultBytes);
	if(!C->Result)xrpcCallReport(XERR_MEMORY,XRPC_CALL_ALLOCATION,"RPC call result allocation failed");
	if(C->Result){*C->Result=(xrpccallresult){0};
		if(Count){xrpcbatchcallresult* B=(xrpcbatchcallresult*)C->Result;B->Count=Count;B->Indices=(size_t*)(B+1);
			for(size_t K=0;K<Count;++K)((size_t*)B->Indices)[K]=SIZE_MAX;}
		C->Promise=xrtPromiseCreate(&Observer,NULL);}
	if(C->Promise)C->Cancel=xrtPromiseCancelToken(C->Promise);
	bool Bound=C->Cancel && xrtPromiseProducerBindTakeV2(C->Promise,(xrtownershipref){C,&xrpcCallOps},&xrpcCallProducer,&xrpcCallCancellation);
	if(!Bound){
		xerror* Error=xrtTakeError();xrtPromiseDestroy(C->Promise);xrtFutureDestroy(Observer);xrtFree(C->Result);xrtCancelDestroy(C->Cancel);xrtFree(C);
		xrtClearError();xrtSetErrorTake(Error);return false;
	}
	C->ContextOwned=true;*Call=C;*Future=Observer;return true;
}
bool xrpcCallCreateEntered(const xrpccallconfig* Config,xrpccall** Call,xfuture** Future,double Began)
{return xrpcCallConstruct(Config,Call,Future,Began,0);}
bool xrpcBatchCallCreateEntered(const xrpccallconfig* Config,size_t Count,xrpccall** Call,xfuture** Future,double Began)
{
	if(!Count || Count>65536u || Count>(SIZE_MAX-sizeof(xrpcbatchcallresult))/sizeof(size_t))
		return xrpcCallReport(XERR_ARGUMENT,XRPC_CALL_ARGUMENT,"invalid RPC batch item count");
	return xrpcCallConstruct(Config,Call,Future,Began,Count);
}
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
	xerror* Previous=xrtTakeError();bool Published=xrtPromiseResolveOwned(Promise,Result,xrpcCallResultDrop,NULL);
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
/* source: extlibs/xrpc/src/http_client/http_client.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_HTTP_CLIENT)
typedef struct xrpchttprequest xrpchttprequest;
typedef struct xrpchttpbatch {
    uint64 First;
    size_t Count, Calls;
    size_t* Indices;
    unsigned char* Notifications;
    void* Workspace;
} xrpchttpbatch;
enum {XRPC_HTTP_CALL, XRPC_HTTP_NOTIFICATION, XRPC_HTTP_BATCH};
struct xrpchttprequest {
    xatomic32 References;
    xrpchttpclient* Client;
    xrpccall* Call;
    xnetpost CancelPost;
    xrpchttprequest *Next, *Previous;
    unsigned char* Wire;
    size_t WireBytes, IdBytes;
    union {
        char Id[24];
        struct {xrpchttpbatch* Batch;size_t Bytes;} Group;
    };
    uint64 Timer;
    bool Linked, Sent, Completing;
    uint8 Operation;
    xrpccallstatus Status;
    xrpcresponses* Response;
};
struct xrpchttpclient {
    xatomic32 References;
    xmutex Gate;
    xnetengine* Engine;
    xnetworker* Worker;
    xnetstream* Stream;
    xnetaddr Remote;
    xrpchttpconfig Config;
    xrpchttpinfo Info;
    xnetpost Wake;
    xpromise* ClosePromise;
    xfuture* CloseFuture;
    xrpchttprequest *SubmitHead, *SubmitTail, *Head, *Tail, *Active;
    xerror* Cause;
    uint64 Serial;
    bool WakeQueued, Started, TransportClosed, EnginePinned, CloseDone;
    bool ReadEnded, BodyStarted, ConnectionClose;
    size_t InputUsed, InputScan, InputCapacity, BodyUsed, BodyCapacity, ResponseWireBytes;
    uint32 Informational;
    uint16 HttpStatus;
    unsigned char *Input, *Payload;
    xhttpfield *Fields, *Trailers;
    xhttp1body Body;
    xstrview Host, Target;
    unsigned char Text[];
};
static void xrpcHttpWork(xnetworker* Worker,ptr Data);
static void xrpcHttpTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data);
static bool xrpcHttpWakeLocked(xrpchttpclient* C);
static bool xrpcHttpReport(xerrkind Kind,xrpchttperror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static void xrpcHttpRef(xrpchttpclient* C)
{(void)xrtAtomic32FetchAdd(&C->References,1,XMEMORY_RELAXED);}
XRPC_API bool xrpcHttpRetain(xrpchttpclient* C)
{
    if(!C)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");
    uint32 N=xrtAtomic32Load(&C->References,XMEMORY_RELAXED);
    while(N<UINT32_MAX/2u){if(xrtAtomic32CompareExchange(&C->References,&N,N+1u,XMEMORY_RELAXED,XMEMORY_RELAXED))return true;}
    return xrpcHttpReport(XERR_RANGE,XRPC_HTTP_STATE,"HTTP client reference limit");
}
XRPC_API void xrpcHttpRelease(xrpchttpclient* C)
{
    if(!C || xrtAtomic32FetchSub(&C->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrtNetStreamDestroy(C->Stream);xrtPromiseDestroy(C->ClosePromise);xrtFutureDestroy(C->CloseFuture);
    xrtErrorFree(C->Cause);xrtFree(C->Input);xrtFree(C->Payload);xrtFree(C->Fields);xrtFree(C->Trailers);
    if(C->EnginePinned)xrtNetEngineUnpin(C->Engine);
    xrtMutexUnit(&C->Gate);xrtFree(C);
}
static bool xrpcHttpWakeLocked(xrpchttpclient* C)
{
    if(C->WakeQueued || C->CloseDone)return true;
    C->WakeQueued=true;xrpcHttpRef(C);
    if(xrtNetPost(C->Worker,&C->Wake,xrpcHttpWork,C))return true;
    C->WakeQueued=false;xrpcHttpRelease(C);return false;
}
static void xrpcHttpWake(xrpchttpclient* C)
{xrtMutexLock(&C->Gate);(void)xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);}
static bool xrpcHttpClosing(xrpchttpclient* C)
{xrtMutexLock(&C->Gate);bool R=C->Info.State>=XRPC_HTTP_CLOSING;xrtMutexUnlock(&C->Gate);return R;}
static void xrpcHttpFail(xrpchttpclient* C,xrpccallstatus Status,const xerror* Cause)
{
    xrtMutexLock(&C->Gate);
    if(C->Info.State<XRPC_HTTP_CLOSING){C->Info.State=XRPC_HTTP_CLOSING;C->Info.CloseStatus=Status;C->Cause=Cause?xrtErrorRef(Cause):NULL;}
    (void)xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);
}
static xrpccallstatus xrpcHttpParseFailure(const xhttp1errorinfo* E)
{
    if(xrtErrorKind(xrtGetError())==XERR_MEMORY)return XRPC_CALL_MEMORY;
    switch(E->Code){
        case XHTTP1_ERROR_HEAD_TOO_LARGE:case XHTTP1_ERROR_START_LINE_TOO_LARGE:
        case XHTTP1_ERROR_FIELD_LINE_TOO_LARGE:case XHTTP1_ERROR_TOO_MANY_FIELDS:
        case XHTTP1_ERROR_BODY_TOO_LARGE:case XHTTP1_ERROR_CHUNK_LINE_TOO_LARGE:
        case XHTTP1_ERROR_TRAILER_TOO_LARGE:case XHTTP1_ERROR_TRAILER_LINE_TOO_LARGE:
        case XHTTP1_ERROR_TOO_MANY_TRAILERS:return XRPC_CALL_LIMIT;
        default:return XRPC_CALL_PROTOCOL;
    }
}
static void xrpcHttpReservationDrop(xrpchttpclient* C,size_t Bytes,size_t BatchBytes)
{
    xrtMutexLock(&C->Gate);--C->Info.Requests;C->Info.RequestBytes-=Bytes;C->Info.BatchBytes-=BatchBytes;
    if(C->Info.State>=XRPC_HTTP_CLOSING)(void)xrpcHttpWakeLocked(C);
    xrtMutexUnlock(&C->Gate);xrpcHttpRelease(C);
}
static void xrpcHttpRequestRef(xrpchttprequest* R)
{(void)xrtAtomic32FetchAdd(&R->References,1,XMEMORY_RELAXED);}
static void xrpcHttpRequestDrop(ptr Value)
{
    xrpchttprequest* R=Value;if(xrtAtomic32FetchSub(&R->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpchttpclient* C=R->Client;size_t Bytes=R->WireBytes;
    size_t BatchBytes=R->Operation==XRPC_HTTP_BATCH?R->Group.Bytes:0;
    if(R->Operation==XRPC_HTTP_BATCH)xrtFree(R->Group.Batch);
    xrpcResponsesRelease(R->Response);xrtFree(R->Wire);xrtFree(R);xrpcHttpReservationDrop(C,Bytes,BatchBytes);
}
static void xrpcHttpReset(xrpchttpclient* C)
{C->Active=NULL;C->InputUsed=0;C->InputScan=0;C->BodyUsed=0;C->ResponseWireBytes=0;C->Informational=0;C->BodyStarted=false;C->ConnectionClose=false;}
static bool xrpcHttpFinish(xrpchttprequest* R,xrpccallstatus Status,const xerror* Cause)
{
    xrpchttpclient* C=R->Client;
    if(R->Completing){Status=R->Status;Cause=Status==XRPC_CALL_HTTP_ERROR?C->Cause:NULL;}
    xrpccallfinish F=R->Completing && Status==XRPC_CALL_BATCH_DONE?
        xrpcBatchCallFinish(R->Call,R->Response,R->Group.Batch->Indices,R->Group.Batch->Count,Cause):
        xrpcCallFinish(R->Call,Status,R->Completing?R->Response:NULL,0,Cause);
    if(F==XRPC_CALL_FINISH_ERROR)return false;
    if(R->Previous)R->Previous->Next=R->Next;else C->Head=R->Next;
    if(R->Next)R->Next->Previous=R->Previous;else C->Tail=R->Previous;
    R->Linked=false;R->Next=R->Previous=NULL;
    if(C->Active==R)xrpcHttpReset(C);
    if(R->Timer)(void)xrtNetEngineTimerCancelCurrent(C->Engine,R->Timer);
    xrtMutexLock(&C->Gate);++C->Info.Completed;xrtMutexUnlock(&C->Gate);
    (void)xrpcCallDestroy(R->Call);R->Call=NULL;xrpcHttpRequestDrop(R);return true;
}
static void xrpcHttpStop(xrpchttprequest* R)
{
    if(!R->Call || !R->Linked)return;
    if(R->Completing){(void)xrpcHttpFinish(R,R->Status,NULL);return;}
    if(xrpcCallPoll(R->Call)==XRPC_CALL_FINISHED){
        xrpchttpclient* C=R->Client;bool Sent=R->Sent;
        if(Sent)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);
        (void)xrpcHttpFinish(R,XRPC_CALL_CLOSED,NULL);
    }
}
static void xrpcHttpCancelWork(xnetworker* Worker,ptr Data)
{(void)Worker;xrpchttprequest* R=Data;xrpcHttpStop(R);xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);}
static void xrpcHttpCancelWake(ptr Data)
{
    xrpchttprequest* R=Data;xrpcHttpRequestRef(R);
    if(!xrtNetPost(R->Client->Worker,&R->CancelPost,xrpcHttpCancelWork,R)){
        xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);
    }
}
static void xrpcHttpTimer(xnetworker* Worker,uint64 Id,xnetresult Result,ptr Data)
{
    (void)Worker;xrpchttprequest* R=Data;
    if(R->Timer==Id)R->Timer=0;
    if(Result==XNET_RESULT_OK)xrpcHttpStop(R);
    else if(Result!=XNET_RESULT_CANCELLED && R->Call)xrpcHttpFail(R->Client,XRPC_CALL_CLOSED,NULL);
    xrpcHttpWake(R->Client);xrpcHttpRequestDrop(R);
}
static bool xrpcHttpArm(xrpchttprequest* R)
{
    double Deadline=xrpcCallDeadline(R->Call);if(Deadline==INFINITY)return true;
    xrpcHttpRequestRef(R);
    R->Timer=__xrtNetEngineSchedule(R->Client->Engine,xrtNetWorkerIndex(R->Client->Worker),Deadline,xrpcHttpTimer,R);
    if(R->Timer)return true;
    xrpcHttpRequestDrop(R);return false;
}
static void xrpcHttpIngest(xrpchttpclient* C)
{
    for(size_t N=0;N<C->Config.WorkRequests;++N){
        xrtMutexLock(&C->Gate);xrpchttprequest* R=C->SubmitHead;
        if(R){C->SubmitHead=R->Next;if(!C->SubmitHead)C->SubmitTail=NULL;}
        xrtMutexUnlock(&C->Gate);if(!R)break;
        R->Next=NULL;R->Previous=C->Tail;R->Linked=true;
        if(C->Tail)C->Tail->Next=R;else C->Head=R;C->Tail=R;
        if(!xrpcHttpClosing(C) && !xrpcHttpArm(R))xrpcHttpFail(C,XRPC_CALL_MEMORY,xrtGetError());
        xrpcHttpStop(R);
    }
}
static xrpccallstatus xrpcHttpAppend(xrpchttpclient* C,xbytesview Data)
{
    size_t Limit=(size_t)C->Config.Body.MaxBody;
    if(Data.Size>Limit-C->BodyUsed)return XRPC_CALL_LIMIT;
    size_t Need=C->BodyUsed+Data.Size;
    if(Need>C->BodyCapacity){
        size_t Capacity=C->BodyCapacity?C->BodyCapacity:256u;if(Capacity>Limit)Capacity=Limit;
        while(Capacity<Need){if(Capacity>Limit/2u){Capacity=Limit;break;}Capacity*=2u;}
        ptr P=xrtRealloc(C->Payload,Capacity);if(!P)return XRPC_CALL_MEMORY;C->Payload=P;C->BodyCapacity=Capacity;
    }
    if(Data.Size)memcpy(C->Payload+C->BodyUsed,Data.Data,Data.Size);
    C->BodyUsed=Need;return XRPC_CALL_OK;
}
static bool xrpcHttpBatchId(xrpcvalue Id,uint64* Value)
{
    if(Id.Kind!=XRPC_VALUE_STRING)return false;
    xrpcstringiterator It={Id.Raw,1,{0},0,0};unsigned char Byte;uint64 N=0;size_t Digits=0;
    while(xrpcStringNext(&It,&Byte)){
        if(Byte<'0' || Byte>'9' || (!Digits && Byte=='0') || Digits==20 || N>(UINT64_MAX-(uint64)(Byte-'0'))/10u)return false;
        N=N*10u+(uint64)(Byte-'0');++Digits;
    }
    if(!Digits)return false;
    *Value=N;return true;
}
static bool xrpcHttpReply(xrpchttpclient* C)
{
    xrpchttprequest* R=C->Active;xrpcmessage Scratch;
    xrpchttpbatch* B=R->Operation==XRPC_HTTP_BATCH?R->Group.Batch:NULL;
    bool Notification=R->Operation==XRPC_HTTP_NOTIFICATION || (B && !B->Calls);
    if(Notification){
        if(C->HttpStatus<200 || C->HttpStatus>=300){
            xrtSetErrorInfo(XERR_IO,"xrt.rpc.http",C->HttpStatus,"HTTP notification rejected");
            const xerror* E=xrtErrorKind(xrtGetError())==XERR_MEMORY?NULL:xrtGetError();
            R->Completing=true;R->Status=XRPC_CALL_HTTP_ERROR;xrpcHttpFail(C,XRPC_CALL_TRANSPORT,E);
        }else if(C->BodyUsed){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);return false;}
        else{R->Completing=true;R->Status=XRPC_CALL_NOTIFY_DONE;if(C->ConnectionClose)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);}
        return xrpcHttpFinish(R,R->Status,C->Cause);
    }
    xrpcresponsestatus S=xrpcResponsesRead((xstrview){(cstr)C->Payload,C->BodyUsed},&C->Config.Responses,
        B?(xrpcmessage*)B->Workspace:&Scratch,B?B->Count:1,&R->Response);
    if(S!=XRPC_RESPONSE_DONE){
        xrpccallstatus Status=XRPC_CALL_PROTOCOL;
        if(xrtErrorKind(xrtGetError())==XERR_MEMORY)Status=XRPC_CALL_MEMORY;
        else if(S==XRPC_RESPONSE_LOCAL_ERROR && (xrtErrorCode(xrtGetError())==XRPC_CODEC_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_CAPACITY || xrtErrorCode(xrtGetError())==XRPC_RESPONSE_LIMIT))Status=XRPC_CALL_LIMIT;
        xrpcHttpFail(C,Status,xrtGetError());return false;
    }
    xrpcresponseinfo I={0};I.Size=sizeof(I);if(!xrpcResponsesInfo(R->Response,&I))return false;
    if(B){
        if(!I.Batch || I.Count!=B->Calls)goto Invalid;
        for(size_t K=0;K<B->Count;++K)B->Indices[K]=SIZE_MAX;
        for(size_t K=0;K<I.Count;++K){uint64 Id;
            if(!xrpcHttpBatchId(I.Messages[K].Id,&Id) || Id<B->First || Id-B->First>=B->Count)goto Invalid;
            size_t Item=(size_t)(Id-B->First);
            if(B->Notifications[Item] || B->Indices[Item]!=SIZE_MAX)goto Invalid;
            B->Indices[Item]=K;
        }
        R->Completing=true;R->Status=XRPC_CALL_BATCH_DONE;
        if(C->ConnectionClose)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);
        return xrpcHttpFinish(R,R->Status,NULL);
    }
    unsigned char Expected[64],Actual[64];size_t A=0,ActualBytes=0;xrpcidconfig K;xrpcIdConfigInit(&K);K.MaxKeyBytes=sizeof(Expected);K.MaxStringBytes=sizeof(Expected);
    K.MaxInputBytes=C->Config.Responses.Decode.MaxIdBytes;
    if(I.Batch || I.Count!=1 || !xrpcIdKeyWrite(Expected,sizeof(Expected),(xstrview){R->Id,R->IdBytes},&K,&A) ||
        !xrpcIdKeyWrite(Actual,sizeof(Actual),I.Ids[0],&K,&ActualBytes) || A!=ActualBytes || memcmp(Expected,Actual,A)){
        xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);return false;
    }
    R->Completing=true;R->Status=I.Messages[0].Kind==XRPC_MESSAGE_RESULT?XRPC_CALL_OK:XRPC_CALL_REMOTE_ERROR;
    bool Close=C->ConnectionClose;if(Close)xrpcHttpFail(C,XRPC_CALL_CLOSED,NULL);
    return xrpcHttpFinish(R,R->Status,NULL);
Invalid:
    xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);return false;
}
/* Incremental head delimiter scan prevents reparsing growing fragmented heads.
 * Body state belongs to the public HTTP/1 reader; only an incomplete trailer
 * prefix is retained/compacted and may be rescanned by that reader. */
static bool xrpcHttpParse(xrpchttpclient* C,bool End)
{
    size_t Offset=0;
    while(C->Active && !C->Active->Completing){
        if(!C->BodyStarted){
            size_t HeadEnd=0;
            while(C->InputScan<C->InputUsed){size_t N=++C->InputScan;
                if(N>=4 && !memcmp(C->Input+N-4,"\r\n\r\n",4)){HeadEnd=N;break;}
                if(N>=C->Config.Head.MaxHead)break;
            }
            if(!HeadEnd){
                if(End || C->InputUsed>=C->Config.Head.MaxHead)xrpcHttpFail(C,End?XRPC_CALL_PROTOCOL:XRPC_CALL_LIMIT,NULL);
                break;
            }
            xhttp1head Head;xrtHttp1HeadInit(&Head,C->Fields,C->Config.Head.MaxFields);
            xhttp1errorinfo Error={0};
            if(xrtHttp1ResponseParse((xbytesview){C->Input,HeadEnd},&Head,&C->Config.Head,&Error)!=XHTTP1_READY){xrpcHttpFail(C,xrpcHttpParseFailure(&Error),xrtGetError());break;}
            if(Head.Status<200){
                if(Head.Status==101 || (Head.Flags&(XHTTP1_CONTENT_LENGTH|XHTTP1_TRANSFER_ENCODING))){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
                if(++C->Informational>C->Config.MaxInformational){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
                memmove(C->Input,C->Input+HeadEnd,C->InputUsed-HeadEnd);C->InputUsed-=HeadEnd;C->InputScan=0;continue;
            }
            xhttp1bodyplan Plan;
            bool EncodingOK=true;
            for(size_t I=0;I<Head.FieldCount;++I)if(xrtHttpFieldNameEqual(Head.Fields[I].Name,XRT_STR_LITERAL("Content-Encoding")) &&
                !xrtHttpTokenEqual(Head.Fields[I].Value,XRT_STR_LITERAL("identity")))EncodingOK=false;
            if(!EncodingOK || (Head.Flags&XHTTP1_TRANSFER_OTHER) || !xrtHttp1ResponseBodyPlan(&Head,XRT_STR_LITERAL("POST"),&Plan) || Plan.Mode==XHTTP1_BODY_TUNNEL){
                xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;
            }
            if(Plan.Mode==XHTTP1_BODY_FIXED && Plan.Length>C->Config.Body.MaxBody){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
            if(!xrtHttp1BodyInit(&C->Body,&Plan,C->Trailers,C->Config.Body.MaxTrailers,&C->Config.Body)){
                xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;
            }
            C->ConnectionClose=(Head.Flags&XHTTP1_CONNECTION_CLOSE)!=0 ||
                (Head.Version==XHTTP_VERSION_1_0 && !(Head.Flags&XHTTP1_KEEP_ALIVE)) || Plan.Mode==XHTTP1_BODY_CLOSE;
            C->HttpStatus=Head.Status;C->BodyStarted=true;Offset=HeadEnd;
        }
        xbytesview Data={0};size_t Used=0;
        xhttp1errorinfo Error={0};
        xhttp1bodystatus S=xrtHttp1BodyRead(&C->Body,(xbytesview){C->Input+Offset,C->InputUsed-Offset},End,&Used,&Data,&Error);
        Offset+=Used;
        if(S==XHTTP1_BODY_ERROR || S==XHTTP1_BODY_FIELDS){xrpcHttpFail(C,S==XHTTP1_BODY_FIELDS?XRPC_CALL_LIMIT:xrpcHttpParseFailure(&Error),xrtGetError());break;}
        if(S==XHTTP1_BODY_DATA){xrpccallstatus Appended=xrpcHttpAppend(C,Data);
            if(Appended!=XRPC_CALL_OK){xrpcHttpFail(C,Appended,Appended==XRPC_CALL_MEMORY?xrtGetError():NULL);break;}}
        if(S==XHTTP1_BODY_DONE){
            if(Offset!=C->InputUsed){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
            C->InputUsed=0;C->InputScan=0;return xrpcHttpReply(C);
        }
        if(!Used || (S==XHTTP1_BODY_MORE && Offset==C->InputUsed))break;
    }
    if(Offset){memmove(C->Input,C->Input+Offset,C->InputUsed-Offset);C->InputUsed-=Offset;}
    return !xrpcHttpClosing(C);
}
static void xrpcHttpRead(xrpchttpclient* C)
{
    size_t Budget=C->Config.ReadBytes;
    while(Budget && C->Stream && !C->TransportClosed && !xrpcHttpClosing(C)){
        size_t Available=xrtNetStreamAvailable(C->Stream);if(!Available)break;
        if(!C->Active || !C->Active->Sent){xrpcHttpFail(C,XRPC_CALL_PROTOCOL,NULL);break;}
        size_t N=C->InputCapacity-C->InputUsed;if(N>Budget)N=Budget;if(N>Available)N=Available;
        size_t Remaining=C->Config.MaxResponseWireBytes-C->ResponseWireBytes;if(N>Remaining)N=Remaining;
        if(!N){xrpcHttpFail(C,XRPC_CALL_LIMIT,NULL);break;}
        N=xrtNetStreamRead(C->Stream,C->Input+C->InputUsed,N);if(!N)break;
        Budget-=N;C->InputUsed+=N;C->ResponseWireBytes+=N;if(!xrpcHttpParse(C,false))break;
    }
    if(C->ReadEnded && !xrpcHttpClosing(C) && (!C->Stream || xrtNetStreamAvailable(C->Stream)==0)){
        if(C->Active)(void)xrpcHttpParse(C,true);
        if(!xrpcHttpClosing(C))xrpcHttpFail(C,XRPC_CALL_TRANSPORT,NULL);
    }
    if(C->Stream && !C->TransportClosed && !xrpcHttpClosing(C)){
        if(xrtNetStreamAvailable(C->Stream))xrpcHttpWake(C);
        else (void)xrtNetStreamResume(C->Stream);
    }
}
static void xrpcHttpOnOpen(xnetstream* Stream,ptr Data)
{(void)Stream;xrpchttpclient* C=Data;xrtMutexLock(&C->Gate);if(C->Info.State<XRPC_HTTP_CLOSING)C->Info.State=XRPC_HTTP_OPEN;xrtMutexUnlock(&C->Gate);xrpcHttpWake(C);}
static void xrpcHttpOnRead(xnetstream* Stream,xnetbuf* Buffer,ptr Data)
{(void)Buffer;xrtNetStreamPause(Stream);xrpcHttpWake(Data);}
static void xrpcHttpOnEnd(xnetstream* Stream,ptr Data)
{(void)Stream;xrpchttpclient* C=Data;C->ReadEnded=true;xrpcHttpWake(C);}
static void xrpcHttpOnDrain(xnetstream* Stream,ptr Data)
{(void)Stream;xrpcHttpWake(Data);}
static void xrpcHttpOnLow(xnetstream* Stream,size_t Queued,ptr Data)
{(void)Queued;xrpcHttpOnDrain(Stream,Data);}
static void xrpcHttpOnClose(xnetstream* Stream,xnetresult Result,const xerror* Error,ptr Data)
{
    (void)Stream;(void)Result;xrpchttpclient* C=Data;C->TransportClosed=true;
    xrpcHttpFail(C,xrtErrorKind(Error)==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,Error);
}
static const xnetstreamevents xrpcHttpEvents={xrpcHttpOnOpen,xrpcHttpOnRead,xrpcHttpOnEnd,NULL,xrpcHttpOnLow,xrpcHttpOnDrain,xrpcHttpOnClose};
static void xrpcHttpWork(xnetworker* Worker,ptr Data)
{
    (void)Worker;xrpchttpclient* C=Data;
    xrtMutexLock(&C->Gate);C->WakeQueued=false;xrtMutexUnlock(&C->Gate);
    if(!C->Started){
        C->Started=true;
        if(!xrpcHttpClosing(C)){
            C->Stream=xrtNetStreamConnect(C->Engine,&C->Remote,xrtNetWorkerIndex(C->Worker),&C->Config.Stream,&xrpcHttpEvents,C);
            if(!C->Stream){C->TransportClosed=true;xrpcHttpFail(C,xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,xrtGetError());}
        }else C->TransportClosed=true;
    }
    xrpcHttpIngest(C);
    if(!xrpcHttpClosing(C)){
        xrpcHttpRead(C);
        if(C->Active && C->Active->Completing)(void)xrpcHttpFinish(C->Active,C->Active->Status,NULL);
        if(!xrpcHttpClosing(C) && !C->Active && C->Head && C->Stream && xrtNetStreamState(C->Stream)==XNET_STREAM_OPEN){
            xrpchttprequest* R=C->Head;C->Active=R;
        }
        if(!xrpcHttpClosing(C) && C->Active && !C->Active->Sent){
            xrpchttprequest* R=C->Active;xrpcHttpStop(R);
            if(C->Active==R){
                xnetresult S=xrtNetStreamSend(C->Stream,R->Wire,R->WireBytes);
                if(S==XNET_RESULT_OK){R->Sent=true;(void)xrpcCallMarkSent(R->Call);}
                else if(S!=XNET_RESULT_AGAIN)xrpcHttpFail(C,S==XNET_RESULT_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY?XRPC_CALL_MEMORY:XRPC_CALL_TRANSPORT,S==XNET_RESULT_ERROR?xrtGetError():NULL);
            }
        }
    }
    if(xrpcHttpClosing(C)){
        if(C->Stream && !C->TransportClosed)(void)xrtNetStreamAbort(C->Stream);
        for(size_t N=0;C->Head && N<C->Config.WorkRequests;++N)if(!xrpcHttpFinish(C->Head,C->Info.CloseStatus,C->Cause))break;
    }
    xrtMutexLock(&C->Gate);
    if(C->SubmitHead || (C->Info.State>=XRPC_HTTP_CLOSING && C->Head) || (!C->Active && C->Head && C->Info.State==XRPC_HTTP_OPEN))
        (void)xrpcHttpWakeLocked(C);
    bool Done=C->TransportClosed && C->Info.Requests==0 && !C->WakeQueued && !C->CloseDone;
    if(Done){C->CloseDone=true;C->Info.State=XRPC_HTTP_CLOSED;C->EnginePinned=false;}
    xrtMutexUnlock(&C->Gate);
    if(Done){
        xrtNetStreamDestroy(C->Stream);C->Stream=NULL;
        xpromise* P=C->ClosePromise;C->ClosePromise=NULL;
        (void)xrtNetEngineUnpin(C->Engine);(void)xrtPromiseResolve(P,NULL);xrtPromiseDestroy(P);
        xrpcHttpRelease(C); /* Runtime activation. */
    }
    xrpcHttpRelease(C); /* Current embedded post. */
}
XRPC_API bool xrpcHttpConfigInit(xrpchttpconfig* Config)
{
    if(!Config || !xrtMemRangeValid(Config,sizeof(*Config)))return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP config output");
    *Config=(xrpchttpconfig){0};Config->Size=sizeof(*Config);xrtNetStreamConfigInit(&Config->Stream);
    xrtHttp1LimitsInit(&Config->Head);xrtHttp1BodyLimitsInit(&Config->Body);xrpcResponseConfigInit(&Config->Responses);
    Config->Body.MaxBody=1024u*1024u;Config->MaxRequests=1024;Config->MaxRequestBytes=256u*1024u;
    if(Config->MaxRequestBytes>Config->Stream.WriteLimit)Config->MaxRequestBytes=Config->Stream.WriteLimit;
    Config->MaxRequestBytesTotal=8u*1024u*1024u;Config->WorkRequests=64;Config->ReadBytes=64u*1024u;Config->MaxResponseWireBytes=2u*1024u*1024u;Config->MaxInformational=8;
    Config->MaxBatchItems=256;Config->MaxBatchBytesTotal=8u*1024u*1024u;return true;
}
XRPC_API bool xrpcHttpConnect(xnetengine* Engine,const xnetaddr* Remote,uint64 Affinity,
    xstrview Host,xstrview Target,const xrpchttpconfig* Config,xrpchttpclient** Client)
{
    if(!Engine || !Remote || !Client || !xrtMemRangeValid(Remote,sizeof(*Remote)) || !xrtMemRangeValid(Client,sizeof(*Client)) ||
        xrtMemRangesOverlap(Remote,sizeof(*Remote),Client,sizeof(*Client)) || !xrtMemRangeValid(Host.Data,Host.Size) || !xrtMemRangeValid(Target.Data,Target.Size) ||
        (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Client,sizeof(*Client)))) ||
        xrtMemRangesOverlap(Host.Data,Host.Size,Client,sizeof(*Client)) || xrtMemRangesOverlap(Target.Data,Target.Size,Client,sizeof(*Client)))
        return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP connect inputs");
    xrpchttpconfig K;if(Config)K=*Config;else xrpcHttpConfigInit(&K);
    const xrpcdecodeconfig* D=&K.Responses.Decode;
    if(K.Size!=sizeof(K) || !K.MaxRequests || K.MaxRequests>1000000u || !K.MaxRequestBytes || K.MaxRequestBytes>K.MaxRequestBytesTotal ||
        K.MaxRequestBytes>K.Stream.WriteLimit || !K.WorkRequests || !K.ReadBytes || !K.MaxResponseWireBytes || !K.Head.MaxHead || !K.Head.MaxFields || !K.Head.MaxStartLine || !K.Head.MaxFieldLine ||
        !K.MaxBatchItems || K.MaxBatchItems>65536u || K.MaxBatchItems>D->MaxMessages || !K.MaxBatchBytesTotal ||
        !K.Body.MaxBody || K.Body.MaxBody>SIZE_MAX || !K.Body.MaxTrailer || !K.Body.MaxTrailers || !K.Body.MaxChunkLine || !K.Body.MaxTrailerLine || !K.MaxInformational ||
        K.Responses.Size!=sizeof(K.Responses) || !K.Responses.MaxOwnedBytes || D->Size!=sizeof(*D) || D->Mode!=XRPC_DECODE_RESPONSES ||
        !D->MaxInputBytes || !D->MaxDepth || D->MaxDepth>XJSON_DEPTH_DEFAULT || !D->MaxStringBytes || !D->MaxValues || !D->MaxContainerItems || !D->MaxMessages || D->MaxIdBytes<22u || !D->MaxMethodBytes ||
        K.Head.MaxFields>SIZE_MAX/sizeof(xhttpfield) || K.Body.MaxTrailers>SIZE_MAX/sizeof(xhttpfield) ||
        Host.Size>K.MaxRequestBytes || Target.Size>K.MaxRequestBytes-Host.Size ||
        Host.Size>SIZE_MAX-sizeof(xrpchttpclient) || Target.Size>SIZE_MAX-sizeof(xrpchttpclient)-Host.Size ||
        (Remote->Family!=XNET_FAMILY_IPV4 && Remote->Family!=XNET_FAMILY_IPV6) || !Remote->Port || !Host.Size || !Target.Size || Target.Data[0]!='/' ||
        !xrtHttpHostValid(Host) || !xrtHttp1TargetValid(Target) || !xrtNetStreamConfigValid(&K.Stream))
        return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP endpoint/configuration");
    size_t Capacity=K.Head.MaxHead>K.Body.MaxTrailer?K.Head.MaxHead:K.Body.MaxTrailer;
    if(Capacity>K.Stream.ReadLimit || Capacity>SIZE_MAX-K.ReadBytes)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP input bounds exceed stream/size limit");
    if(!xrtNetEnginePin(Engine))return false;
    xrpchttpclient* C=xrtCalloc(1,sizeof(*C)+Host.Size+Target.Size);
    if(!C){xrtNetEngineUnpin(Engine);return false;}
    xrtAtomic32Init(&C->References,1);C->Engine=Engine;C->EnginePinned=true;C->Remote=*Remote;C->Config=K;
    C->Worker=xrtNetEngineWorker(Engine,(uint32)(Affinity%xrtNetEngineWorkerCount(Engine)));
    C->Info.Size=sizeof(C->Info);C->Info.CloseStatus=XRPC_CALL_CLOSED;C->InputCapacity=Capacity+K.ReadBytes;
    C->Host=(xstrview){(cstr)C->Text,Host.Size};C->Target=(xstrview){(cstr)C->Text+Host.Size,Target.Size};
    memcpy((ptr)C->Host.Data,Host.Data,Host.Size);memcpy((ptr)C->Target.Data,Target.Data,Target.Size);
    if(!xrtMutexInit(&C->Gate)){xrtNetEngineUnpin(Engine);xrtFree(C);return false;}
    C->Input=xrtMalloc(C->InputCapacity);C->Fields=xrtMalloc(K.Head.MaxFields*sizeof(xhttpfield));C->Trailers=xrtMalloc(K.Body.MaxTrailers*sizeof(xhttpfield));
    if(!C->Input || !C->Fields || !C->Trailers || !xrtNetPostInit(&C->Wake) || !(C->ClosePromise=xrtPromiseCreate(&C->CloseFuture,NULL))){xrpcHttpRelease(C);return false;}
    xrpcHttpRef(C);xrtMutexLock(&C->Gate);bool Posted=xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);
    if(!Posted){xrpcHttpRelease(C);xrpcHttpRelease(C);return false;}
    *Client=C;return true;
}
static xrpcadmission xrpcHttpSubmit(xrpchttpclient* C,const xrpchttpitem* Items,size_t Count,
    bool Batch,int64 Timeout,xfuture** Future,double Entered)
{
    if(!C || !Future || !Items || !Count || Count>SIZE_MAX/sizeof(*Items) || !xrtMemRangeValid(Future,sizeof(*Future)) ||
        !xrtMemRangeValid(Items,Count*sizeof(*Items)) || xrtMemRangesOverlap(Items,Count*sizeof(*Items),Future,sizeof(*Future)) ||
        Timeout<XRT_WAIT_FOREVER || xrtMemRangesOverlap(C,sizeof(*C)+C->Host.Size+C->Target.Size,Future,sizeof(*Future))){
        xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP operation inputs");return XRPC_ADMISSION_ERROR;
    }
    if(Batch && Count>C->Config.MaxBatchItems){xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return XRPC_ADMISSION_OVERLOADED;}
    size_t Calls=0;
    for(size_t K=0;K<Count;++K){const xrpchttpitem* I=&Items[K];
        if((I->Kind!=XRPC_MESSAGE_CALL && I->Kind!=XRPC_MESSAGE_NOTIFICATION) || !xrtMemRangeValid(I->Method.Data,I->Method.Size) || !xrtMemRangeValid(I->Params.Data,I->Params.Size) ||
            xrtMemRangesOverlap(Future,sizeof(*Future),I->Method.Data,I->Method.Size) || xrtMemRangesOverlap(Future,sizeof(*Future),I->Params.Data,I->Params.Size)){
            xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP item/kind/view/output overlap");return XRPC_ADMISSION_ERROR;}
        if(I->Kind==XRPC_MESSAGE_CALL)++Calls;
    }
    xrtMutexLock(&C->Gate);xrpcadmission Admission=XRPC_ADMISSION_ACCEPTED;
    if(C->Info.State>=XRPC_HTTP_CLOSING)Admission=XRPC_ADMISSION_CLOSED;
    else if(C->Info.Requests>=C->Config.MaxRequests)Admission=XRPC_ADMISSION_OVERLOADED;
    if(Admission!=XRPC_ADMISSION_ACCEPTED){++C->Info.Rejected;xrtMutexUnlock(&C->Gate);return Admission;}
    size_t Serials=Calls?(Batch?Count:1):0;
    if(Serials>UINT64_MAX-C->Serial){xrtMutexUnlock(&C->Gate);xrpcHttpReport(XERR_RANGE,XRPC_HTTP_ID_EXHAUSTED,"RPC wire ID space exhausted");return XRPC_ADMISSION_ERROR;}
    uint64 Serial=Serials?C->Serial+1u:0;C->Serial+=(uint64)Serials;++C->Info.Requests;xrpcHttpRef(C);xrtMutexUnlock(&C->Gate);
    xrpchttprequest* R=xrtCalloc(1,sizeof(*R));if(!R){xrpcHttpReservationDrop(C,0,0);return XRPC_ADMISSION_ERROR;}
    R->Client=C;R->Operation=Batch?XRPC_HTTP_BATCH:Calls?XRPC_HTTP_CALL:XRPC_HTTP_NOTIFICATION;xrtAtomic32Init(&R->References,1);xfuture* F=NULL;
    xrpccallconfig CallConfig;xrpcCallConfigInit(&CallConfig);CallConfig.Timeout=Timeout;CallConfig.User=R;CallConfig.Wake=xrpcHttpCancelWake;CallConfig.Drop=xrpcHttpRequestDrop;
    xrpcHttpRequestRef(R);
    bool Created=Batch?xrpcBatchCallCreateEntered(&CallConfig,Count,&R->Call,&F,Entered):xrpcCallCreateEntered(&CallConfig,&R->Call,&F,Entered);
    if(!Created){xrpcHttpRequestDrop(R);goto Error;}
    if(!xrtNetPostInit(&R->CancelPost))goto Error;
    xrpcoutgoing Single={0};xrpcoutgoing* Messages=&Single;
    if(Batch){
        size_t Prefix=sizeof(xrpchttpbatch)+Count*(sizeof(size_t)+1u);
        size_t Alignment=_Alignof(xrpcmessage)>_Alignof(xrpcoutgoing)?_Alignof(xrpcmessage):_Alignof(xrpcoutgoing);
        size_t Padding=(Alignment-Prefix%Alignment)%Alignment;
        size_t Unit=sizeof(xrpcmessage)>sizeof(xrpcoutgoing)+24u?sizeof(xrpcmessage):sizeof(xrpcoutgoing)+24u;
        if(Count>(SIZE_MAX-Prefix-Padding)/Unit){xrpcHttpReport(XERR_RANGE,XRPC_HTTP_ALLOCATION,"RPC batch storage overflows");goto Error;}
        size_t Bytes=Prefix+Padding+Count*Unit;
        xrtMutexLock(&C->Gate);
        if(C->Info.State>=XRPC_HTTP_CLOSING)Admission=XRPC_ADMISSION_CLOSED;
        else if(Bytes>C->Config.MaxBatchBytesTotal-C->Info.BatchBytes)Admission=XRPC_ADMISSION_OVERLOADED;
        if(Admission==XRPC_ADMISSION_ACCEPTED){C->Info.BatchBytes+=Bytes;R->Group.Bytes=Bytes;}
        xrtMutexUnlock(&C->Gate);if(Admission!=XRPC_ADMISSION_ACCEPTED)goto Refused;
        xrpchttpbatch* B=xrtMalloc(Bytes);if(!B)goto Error;R->Group.Batch=B;
        *B=(xrpchttpbatch){0};B->Count=Count;B->Calls=Calls;B->First=Serial;B->Indices=(size_t*)(B+1);
        B->Notifications=(unsigned char*)(B->Indices+Count);B->Workspace=(unsigned char*)B+Prefix+Padding;
        Messages=B->Workspace;char* Ids=(char*)(Messages+Count);
        for(size_t K=0;K<Count;++K){B->Indices[K]=SIZE_MAX;B->Notifications[K]=(unsigned char)(Items[K].Kind==XRPC_MESSAGE_NOTIFICATION);
            Messages[K]=(xrpcoutgoing){0};Messages[K].Kind=Items[K].Kind;Messages[K].Method=Items[K].Method;Messages[K].Params=Items[K].Params;
            if(!B->Notifications[K]){int N=snprintf(Ids+K*24u,24,"\"%"PRIu64"\"",Serial+(uint64)K);
                if(N<=0 || N>=24)goto Error;
                Messages[K].Id=(xstrview){Ids+K*24u,(size_t)N};}
        }
    }else{
        Single.Kind=Items[0].Kind;Single.Method=Items[0].Method;Single.Params=Items[0].Params;
        if(Calls){int N=snprintf(R->Id,sizeof(R->Id),"\"%"PRIu64"\"",Serial);if(N<=0 || (size_t)N>=sizeof(R->Id))goto Error;
            R->IdBytes=(size_t)N;Single.Id=(xstrview){R->Id,R->IdBytes};}
    }
    xrpcencodeconfig Encode;xrpcEncodeConfigInit(&Encode);Encode.MaxOutputBytes=C->Config.MaxRequestBytes;Encode.MaxMessages=C->Config.MaxBatchItems;
    size_t BodyBytes=0,HeadBytes=0;if(!xrpcEncodeSize(Messages,Count,Batch,&Encode,&BodyBytes))goto Error;
    char Length[24];int N=snprintf(Length,sizeof(Length),"%zu",BodyBytes);if(N<=0 || (size_t)N>=sizeof(Length))goto Error;
    xhttpfield Fields[]={{XRT_STR_LITERAL("Host"),C->Host},{XRT_STR_LITERAL("Content-Type"),XRT_STR_LITERAL("application/json")},
        {XRT_STR_LITERAL("Content-Length"),(xstrview){Length,(size_t)N}},
        {XRT_STR_LITERAL("Accept-Encoding"),XRT_STR_LITERAL("identity")}};
    if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),C->Target,XHTTP_VERSION_1_1,Fields,4,NULL,0,&HeadBytes))goto Error;
    if(BodyBytes>C->Config.MaxRequestBytes || HeadBytes>C->Config.MaxRequestBytes-BodyBytes){Admission=XRPC_ADMISSION_OVERLOADED;goto Refused;}
    size_t Bytes=HeadBytes+BodyBytes;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XRPC_HTTP_CLOSING)Admission=XRPC_ADMISSION_CLOSED;
    else if(Bytes>C->Config.MaxRequestBytesTotal-C->Info.RequestBytes)Admission=XRPC_ADMISSION_OVERLOADED;
    if(Admission==XRPC_ADMISSION_ACCEPTED){C->Info.RequestBytes+=Bytes;R->WireBytes=Bytes;}
    xrtMutexUnlock(&C->Gate);if(Admission!=XRPC_ADMISSION_ACCEPTED)goto Refused;
    R->Wire=xrtMalloc(Bytes);size_t Written=0;
    if(!R->Wire || !xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),C->Target,XHTTP_VERSION_1_1,Fields,4,R->Wire,HeadBytes,&Written) || Written!=HeadBytes ||
        !xrpcEncodeWrite(R->Wire+HeadBytes,BodyBytes,Messages,Count,Batch,&Encode,NULL))goto Error;
    xrtMutexLock(&C->Gate);
    if(C->Info.State>=XRPC_HTTP_CLOSING){xrtMutexUnlock(&C->Gate);Admission=XRPC_ADMISSION_CLOSED;goto Refused;}
    if(!xrpcHttpWakeLocked(C)){xrtMutexUnlock(&C->Gate);goto Error;}
    xrpcHttpRequestRef(R);if(C->SubmitTail)C->SubmitTail->Next=R;else C->SubmitHead=R;C->SubmitTail=R;++C->Info.Accepted;
    xrtMutexUnlock(&C->Gate);*Future=F;xrpcHttpRequestDrop(R);return XRPC_ADMISSION_ACCEPTED;
Refused:
    xrtMutexLock(&C->Gate);++C->Info.Rejected;xrtMutexUnlock(&C->Gate);
    {xerror* E=xrtTakeError();if(R->Call)(void)xrpcCallDestroy(R->Call);xrtFutureDestroy(F);xrpcHttpRequestDrop(R);xrtClearError();xrtSetErrorTake(E);}return Admission;
Error:
    {xerror* E=xrtTakeError();if(R->Call)(void)xrpcCallDestroy(R->Call);xrtFutureDestroy(F);xrpcHttpRequestDrop(R);xrtClearError();xrtSetErrorTake(E);}return XRPC_ADMISSION_ERROR;
}
XRPC_API xrpcadmission xrpcHttpCallAsync(xrpchttpclient* C,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double Entered=xrtTimer();xrpchttpitem I={XRPC_MESSAGE_CALL,Method,Params};return xrpcHttpSubmit(C,&I,1,false,Timeout,Future,Entered);}
XRPC_API xrpcadmission xrpcHttpNotifyAsync(xrpchttpclient* C,xstrview Method,xstrview Params,int64 Timeout,xfuture** Future)
{double Entered=xrtTimer();xrpchttpitem I={XRPC_MESSAGE_NOTIFICATION,Method,Params};return xrpcHttpSubmit(C,&I,1,false,Timeout,Future,Entered);}
XRPC_API xrpcadmission xrpcHttpBatchAsync(xrpchttpclient* C,const xrpchttpitem* Items,size_t Count,int64 Timeout,xfuture** Future)
{return xrpcHttpSubmit(C,Items,Count,true,Timeout,Future,xrtTimer());}
XRPC_API bool xrpcHttpInfo(xrpchttpclient* C,xrpchttpinfo* Info)
{
    if(!C || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) ||
        xrtMemRangesOverlap(C,sizeof(*C)+C->Host.Size+C->Target.Size,Info,sizeof(*Info)))return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"invalid HTTP info output");
    xrtMutexLock(&C->Gate);*Info=C->Info;xrtMutexUnlock(&C->Gate);return true;
}
XRPC_API bool xrpcHttpClose(xrpchttpclient* C)
{
    if(!C)return xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");
    xrtMutexLock(&C->Gate);if(C->Info.State<XRPC_HTTP_CLOSING){C->Info.State=XRPC_HTTP_CLOSING;C->Info.CloseStatus=XRPC_CALL_CLOSED;}
    bool R=xrpcHttpWakeLocked(C);xrtMutexUnlock(&C->Gate);return R;
}
XRPC_API xfuture* xrpcHttpClosed(xrpchttpclient* C)
{if(!C){xrpcHttpReport(XERR_ARGUMENT,XRPC_HTTP_ARGUMENT,"HTTP client is NULL");return NULL;}return xrtFutureRef(C->CloseFuture);}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/xrpc/src/server/server.c */
/* ========================================================================== */

#if defined(XRPC_FEATURE_XRPC_SERVER)
#include <math.h>
#include <string.h>
#if defined(XRPC_FEATURE_XRPC_SERVER)
typedef struct xrpcserverquota {
    xatomic32 References;
    xmutex Gate;
    xcond Idle;
    xrpcserverinfo Info;
} xrpcserverquota;
typedef struct xrpcserverslot {size_t Next;xrpcmessage* Scratch;} xrpcserverslot;
typedef struct xrpcserverbox {
    xrpcserverresult Value; /* Public prefix, one allocation with output. */
    xrpcserverquota* Quota;
    size_t Capacity;
    unsigned char Data[];
} xrpcserverbox;
typedef struct xrpcserverjob {
    xatomic32 References;
    xrpcserver* Server;
    xpromise* Promise;
    xcancel* Cancel;
    xrpcserverbox* Box;
    size_t InputBytes,OwnedBytes,OutputBytes,InitialBoxBytes;
    double Deadline;
    bool Finished,ResultOwned,Entered;
    xrpcserverstatus OutputFailure;
    xrpcdispatchinfo Dispatch;
    unsigned char Input[];
} xrpcserverjob;
struct xrpcserver {
    xrpcserverconfig Config;
    xrpcserverquota* Quota;
    xexecutor* Executor;
    xcancel* Cancel;
    size_t OwnedBytes,FreeSlot;
    xrpcserverslot Slots[];
};
static bool xrpcServerReport(xerrkind Kind,xrpcservererror Code,cstr Message)
{xrtSetErrorInfo(Kind,"xrt.rpc",(int32)Code,Message);return false;}
static double xrpcServerDeadline(double Entered,int64 Timeout)
{return Timeout==XRT_WAIT_FOREVER?INFINITY:Timeout==0?Entered:nextafter(Entered+(double)(Timeout/1000)+(double)(Timeout%1000)*.001,INFINITY);}
static int64 xrpcServerRemaining(double Deadline)
{if(Deadline==INFINITY)return XRT_WAIT_FOREVER;double N=ceil((Deadline-xrtTimer())*1000.);return N<=0?0:N>=0x1p63?INT64_MAX:(int64)N;}
static void xrpcServerQuotaDrop(xrpcserverquota* Q)
{if(xrtAtomic32FetchSub(&Q->References,1,XMEMORY_ACQ_REL)!=1)return;xrtCondUnit(&Q->Idle);xrtMutexUnit(&Q->Gate);xrtFree(Q);}
static void xrpcServerResultRefund(xrpcserverquota* Q,size_t Bytes)
{xrtMutexLock(&Q->Gate);--Q->Info.Results;Q->Info.ResultBytes-=Bytes;xrtMutexUnlock(&Q->Gate);xrpcServerQuotaDrop(Q);}
static void xrpcServerResultDrop(ptr Value,ptr User)
{(void)User;xrpcserverbox* B=Value;xrpcserverquota* Q=B->Quota;size_t Bytes=sizeof(*B)+B->Capacity;xrtErrorFree((xerror*)B->Value.Cause);xrtFree(B);xrpcServerResultRefund(Q,Bytes);}
static void xrpcServerOperationDrop(xrpcserver* S,size_t Bytes)
{xrpcserverquota* Q=S->Quota;xrtMutexLock(&Q->Gate);--Q->Info.Operations;Q->Info.RequestBytes-=Bytes;xrtCondBroadcast(&Q->Idle);xrtMutexUnlock(&Q->Gate);}
static void xrpcServerJobDropRef(xrpcserverjob* J)
{
    if(xrtAtomic32FetchSub(&J->References,1,XMEMORY_ACQ_REL)!=1)return;
    xrpcserver* S=J->Server;size_t Bytes=J->OwnedBytes;
    if(J->ResultOwned){if(J->Box)xrpcServerResultDrop(J->Box,NULL);else xrpcServerResultRefund(S->Quota,J->InitialBoxBytes);}
    xrtCancelDestroy(J->Cancel);xrtFree(J);xrpcServerOperationDrop(S,Bytes);
}
static void xrpcServerProducerDrop(const void* Data){xrpcServerJobDropRef((xrpcserverjob*)Data);}
static bool xrpcServerJobCount(const void* Data,size_t* Count)
{if(!Data || !Count)return false;*Count=xrtAtomic32Load(&((const xrpcserverjob*)Data)->References,XMEMORY_RELAXED);return true;}
static bool xrpcServerJobTrace(const void* Data,xrtownershipvisitor Visit,ptr Context)
{(void)Data;(void)Visit;(void)Context;return false;}
static const xrtownershipops xrpcServerJobOps={xrpcServerJobCount,xrpcServerJobTrace};
static const xfutureproducerownershipv1 xrpcServerProducer={sizeof(xfutureproducerownershipv1),xrpcServerProducerDrop};
static xrpcserverstatus xrpcServerStop(xrpcserverjob* J)
{if(xrtTimer()>=J->Deadline)return XRPC_SERVER_TIMEOUT;if(xrtCancelRequested(J->Cancel))return XRPC_SERVER_CANCELLED;return XRPC_SERVER_DONE;}
static bool xrpcServerGrow(xrpcserverjob* J,size_t Need)
{
    xrpcserverbox* B=J->Box;if(Need<=B->Capacity)return true;
    size_t Limit=J->Server->Config.Dispatch.MaxOutputBytes,Capacity=B->Capacity;
    while(Capacity<Need){if(Capacity>Limit/2u){Capacity=Limit;break;}Capacity*=2u;}
    /* Core realloc may allocate/copy before freeing the old block. Reserve
     * the entire new block alongside the old one, including its header. */
    size_t OldBytes=sizeof(*B)+B->Capacity,NewBytes=sizeof(*B)+Capacity;xrpcserverquota* Q=B->Quota;
    xrtMutexLock(&Q->Gate);bool Fits=NewBytes<=J->Server->Config.MaxResultBytes-Q->Info.ResultBytes;
    if(Fits)Q->Info.ResultBytes+=NewBytes;
    xrtMutexUnlock(&Q->Gate);
    if(!Fits){J->OutputFailure=XRPC_SERVER_LIMIT;return xrpcServerReport(XERR_RANGE,XRPC_SERVER_RESOURCE,"RPC retained result budget exhausted");}
    xrpcserverbox* New=xrtRealloc(B,NewBytes);
    if(!New){xrtMutexLock(&Q->Gate);Q->Info.ResultBytes-=NewBytes;xrtMutexUnlock(&Q->Gate);J->OutputFailure=XRPC_SERVER_MEMORY;return false;}
    New->Capacity=Capacity;J->Box=New;xrtMutexLock(&Q->Gate);Q->Info.ResultBytes-=OldBytes;xrtMutexUnlock(&Q->Gate);return true;
}
static bool xrpcServerEmit(const xrpcoutgoing* Response,bool Batch,size_t Bytes,ptr Data)
{
    xrpcserverjob* J=Data;if(xrpcServerStop(J)!=XRPC_SERVER_DONE)return false;
    size_t Extra=Batch?(J->OutputBytes?1u:2u):0u,Limit=J->Server->Config.Dispatch.MaxOutputBytes;
    if(Extra>Limit-J->OutputBytes || Bytes>Limit-J->OutputBytes-Extra){J->OutputFailure=XRPC_SERVER_LIMIT;return false;}
    size_t Need=J->OutputBytes+Extra+Bytes;if(!xrpcServerGrow(J,Need))return false;
    unsigned char* P=J->Box->Data;size_t At=J->OutputBytes;
    if(Batch){if(At)P[At-1]=',';else P[At++]='[';if(J->OutputBytes)At=J->OutputBytes;}
    size_t Written=0;if(!xrpcEncodeWrite(P+At,Bytes,Response,1,false,&J->Server->Config.Dispatch.Reply,&Written) || Written!=Bytes)return false;
    At+=Bytes;if(Batch)P[At++]=']';J->OutputBytes=At;return true;
}
static bool xrpcServerInvoke(const xrpcmessage* Request,xrpcreply* Reply,ptr Data)
{
    xrpcserverjob* J=Data;if(xrpcServerStop(J)!=XRPC_SERVER_DONE){(void)xrpcReplyAbort(Reply,NULL);return true;}
    xrpcservercontext Context={J->Cancel,J->Deadline};J->Entered=true;
    bool Handled=J->Server->Config.Call(Request,Reply,&Context,J->Server->Config.User);
    if(xrpcServerStop(J)!=XRPC_SERVER_DONE)(void)xrpcReplyAbort(Reply,NULL);
    return Handled;
}
static void xrpcServerFinish(xrpcserverjob* J,xrpcserverstatus Status)
{
    J->Finished=true;xrpcserverbox* B=J->Box;
    B->Value.Status=Status;B->Value.MayHaveExecuted=J->Entered;B->Value.Dispatch=J->Dispatch;
    if(Status==XRPC_SERVER_DONE)B->Value.Wire=(xstrview){(cstr)B->Data,J->OutputBytes};
    else{B->Value.Wire=(xstrview){0};if(Status!=XRPC_SERVER_TIMEOUT && Status!=XRPC_SERVER_CANCELLED)B->Value.Cause=xrtTakeError();}
    xrpcserverquota* Q=J->Server->Quota;xrtMutexLock(&Q->Gate);++Q->Info.Completed;xrtMutexUnlock(&Q->Gate);
    J->Box=NULL;J->ResultOwned=false;
    if(!xrtPromiseResolveOwned(J->Promise,B,xrpcServerResultDrop,NULL)){
        J->Box=B;J->ResultOwned=true;xrtMutexLock(&Q->Gate);++Q->Info.PublicationFailures;xrtMutexUnlock(&Q->Gate);
    }
}
static void xrpcServerRun(ptr Data)
{
    xrpcserverjob* J=Data;xrpcserver* S=J->Server;xrpcserverquota* Q=S->Quota;
    xrtMutexLock(&Q->Gate);size_t Slot=S->FreeSlot;S->FreeSlot=S->Slots[Slot].Next;++Q->Info.Running;xrtMutexUnlock(&Q->Gate);
    xrpcserverstatus Status=xrpcServerStop(J);
    if(Status==XRPC_SERVER_DONE){
        xrtClearError();bool OK=xrpcDispatch((xstrview){(cstr)J->Input,J->InputBytes},&S->Config.Dispatch,S->Slots[Slot].Scratch,S->Config.Dispatch.Decode.MaxMessages,
            S->Config.Call?xrpcServerInvoke:NULL,J,xrpcServerEmit,J,&J->Dispatch);
        Status=xrpcServerStop(J);
        if(Status==XRPC_SERVER_DONE && !OK){
            if(J->OutputFailure!=XRPC_SERVER_DONE)Status=J->OutputFailure;
            else if(xrtErrorKind(xrtGetError())==XERR_MEMORY)Status=XRPC_SERVER_MEMORY;
            else if(xrtErrorCode(xrtGetError())==XRPC_DISPATCH_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_LIMIT || xrtErrorCode(xrtGetError())==XRPC_CODEC_CAPACITY)Status=XRPC_SERVER_LIMIT;
            else Status=XRPC_SERVER_LOCAL_ERROR;
        }
    }
    xrtMutexLock(&Q->Gate);S->Slots[Slot].Next=S->FreeSlot;S->FreeSlot=Slot;--Q->Info.Running;xrtMutexUnlock(&Q->Gate);
    xrpcServerFinish(J,Status);
}
static void xrpcServerJobDrop(ptr Data,ptr Context)
{
    (void)Context;xrpcserverjob* J=Data;
    if(!J->Finished)xrpcServerFinish(J,xrpcServerStop(J)==XRPC_SERVER_TIMEOUT?XRPC_SERVER_TIMEOUT:XRPC_SERVER_CANCELLED);
    xpromise* P=J->Promise;J->Promise=NULL;xrtPromiseDestroy(P);xrpcServerJobDropRef(J);
}
XRPC_API bool xrpcServerConfigInit(xrpcserverconfig* C)
{
    if(!C || !xrtMemRangeValid(C,sizeof(*C)))return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server config output");
    *C=(xrpcserverconfig){0};C->Size=sizeof(*C);C->Executor=(xexecutorconfig){2,64,0};xrpcDispatchConfigInit(&C->Dispatch);
    C->MaxOperations=128;C->MaxRequestBytesTotal=8u*1024u*1024u;C->MaxResults=1024;C->MaxResultBytes=8u*1024u*1024u;
    C->MaxWorkspaceBytes=4u*1024u*1024u;C->InitialOutputBytes=256;return true;
}
XRPC_API bool xrpcServerCreate(const xrpcserverconfig* Config,xrpcserver** Result)
{
    if(!Result || !xrtMemRangeValid(Result,sizeof(*Result)) || (Config && (!xrtMemRangeValid(Config,sizeof(*Config)) || xrtMemRangesOverlap(Config,sizeof(*Config),Result,sizeof(*Result)))))goto Argument;
    xrpcserverconfig C;if(Config)C=*Config;else xrpcServerConfigInit(&C);
    const xrpcdecodeconfig* D=&C.Dispatch.Decode;const xrpcencodeconfig* E=&C.Dispatch.Reply;
    if(C.Size!=sizeof(C) || !C.Executor.Threads || C.Executor.Threads>XRT_EXECUTOR_THREAD_LIMIT || !C.Executor.QueueLimit || !C.MaxOperations ||
        !C.MaxRequestBytesTotal || !C.MaxResults || !C.MaxResultBytes || !C.MaxWorkspaceBytes || !C.InitialOutputBytes ||
        C.Dispatch.Size!=sizeof(C.Dispatch) || !C.Dispatch.MaxReplies || !C.Dispatch.MaxOutputBytes || C.Dispatch.MaxOutputBytes>SIZE_MAX-sizeof(xrpcserverbox) ||
        D->Size!=sizeof(*D) || D->Mode!=XRPC_DECODE_REQUESTS || !D->MaxInputBytes || !D->MaxDepth || D->MaxDepth>XJSON_DEPTH_DEFAULT ||
        !D->MaxStringBytes || !D->MaxValues || !D->MaxContainerItems || !D->MaxMessages || !D->MaxIdBytes || !D->MaxMethodBytes ||
        E->Size!=sizeof(*E) || !E->MaxOutputBytes || !E->MaxDepth || E->MaxDepth>XJSON_DEPTH_DEFAULT || !E->MaxStringBytes ||
        !E->MaxValues || !E->MaxContainerItems || !E->MaxMessages || !E->MaxIdBytes || !E->MaxMethodBytes)goto Argument;
    size_t Base=sizeof(xrpcserver)+(size_t)C.Executor.Threads*sizeof(xrpcserverslot);
    if(D->MaxMessages>(SIZE_MAX-Base)/(sizeof(xrpcmessage)*(size_t)C.Executor.Threads))goto Argument;
    size_t Bytes=Base+D->MaxMessages*sizeof(xrpcmessage)*(size_t)C.Executor.Threads;
    if(Bytes>C.MaxWorkspaceBytes || sizeof(xrpcserverquota)>C.MaxWorkspaceBytes-Bytes)goto Argument;
    xrpcserver* S=xrtCalloc(1,Bytes);if(!S)return false;S->Config=C;S->OwnedBytes=Bytes;S->FreeSlot=0;
    xrpcserverquota* Q=xrtCalloc(1,sizeof(*Q));if(!Q){xrtFree(S);return false;}S->Quota=Q;xrtAtomic32Init(&Q->References,1);
    if(!xrtMutexInit(&Q->Gate)){xrtFree(Q);xrtFree(S);return false;}
    if(!xrtCondInit(&Q->Idle)){xrtMutexUnit(&Q->Gate);xrtFree(Q);xrtFree(S);return false;}
    Q->Info.Size=sizeof(Q->Info);Q->Info.Accepting=true;Q->Info.WorkspaceBytes=Bytes+sizeof(*Q);
    for(size_t K=0;K<C.Executor.Threads;++K){S->Slots[K].Next=K+1;S->Slots[K].Scratch=(xrpcmessage*)((unsigned char*)S+Base)+K*D->MaxMessages;}
    S->Cancel=xrtCancelCreate();S->Executor=S->Cancel?xrtExecutorCreate(&C.Executor):NULL;
    if(!S->Executor){xerror* X=xrtTakeError();xrtCancelDestroy(S->Cancel);xrpcServerQuotaDrop(Q);xrtFree(S);xrtClearError();xrtSetErrorTake(X);return false;}
    *Result=S;return true;
Argument:
    return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server configuration/output span");
}
XRPC_API xrpcserveradmission xrpcServerSubmit(xrpcserver* S,xstrview Input,int64 Timeout,xfuture** Future)
{
    double Entered=xrtTimer();
    if(!S || !Future || Timeout<XRT_WAIT_FOREVER || !xrtMemRangeValid(Future,sizeof(*Future)) || !xrtMemRangeValid(Input.Data,Input.Size) ||
        xrtMemRangesOverlap(Future,sizeof(*Future),Input.Data,Input.Size) || xrtMemRangesOverlap(Future,sizeof(*Future),S,S->OwnedBytes)){
        xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server submission span/timeout");return XRPC_SERVER_ADMISSION_ERROR;}
    if(Input.Size>SIZE_MAX-sizeof(xrpcserverjob)){xrpcServerReport(XERR_RANGE,XRPC_SERVER_RESOURCE,"server request size overflows");return XRPC_SERVER_ADMISSION_ERROR;}
    size_t JobBytes=sizeof(xrpcserverjob)+Input.Size,Capacity=S->Config.InitialOutputBytes;
    if(Capacity>S->Config.Dispatch.MaxOutputBytes)Capacity=S->Config.Dispatch.MaxOutputBytes;
    size_t BoxBytes=sizeof(xrpcserverbox)+Capacity;xrpcserverquota* Q=S->Quota;xerror* Ambient=xrtTakeError();
    xrtMutexLock(&Q->Gate);xrpcserveradmission A=XRPC_SERVER_ACCEPTED;
    if(!Q->Info.Accepting)A=XRPC_SERVER_CLOSED;
    else if(Q->Info.Operations>=S->Config.MaxOperations || Q->Info.Results>=S->Config.MaxResults || Input.Size>S->Config.Dispatch.Decode.MaxInputBytes ||
        JobBytes>S->Config.MaxRequestBytesTotal-Q->Info.RequestBytes || BoxBytes>S->Config.MaxResultBytes-Q->Info.ResultBytes ||
        xrtAtomic32Load(&Q->References,XMEMORY_RELAXED)>=UINT32_MAX/2u)A=XRPC_SERVER_OVERLOADED;
    if(A!=XRPC_SERVER_ACCEPTED){++Q->Info.Rejected;xrtMutexUnlock(&Q->Gate);xrtSetErrorTake(Ambient);return A;}
    ++Q->Info.Operations;Q->Info.RequestBytes+=JobBytes;++Q->Info.Results;Q->Info.ResultBytes+=BoxBytes;
    xrtAtomic32FetchAdd(&Q->References,1,XMEMORY_RELAXED);xrtMutexUnlock(&Q->Gate);
    xrpcserverjob* J=xrtMalloc(JobBytes);xfuture* F=NULL;
    if(!J){xrpcServerResultRefund(Q,BoxBytes);xrpcServerOperationDrop(S,JobBytes);goto NoJob;}
    *J=(xrpcserverjob){0};J->Server=S;J->OwnedBytes=JobBytes;J->InputBytes=Input.Size;J->InitialBoxBytes=BoxBytes;J->ResultOwned=true;
    J->Deadline=xrpcServerDeadline(Entered,Timeout);J->Dispatch.Size=sizeof(J->Dispatch);xrtAtomic32Init(&J->References,1);
    if(Input.Size)memcpy(J->Input,Input.Data,Input.Size);
    J->Box=xrtCalloc(1,BoxBytes);if(!J->Box)goto Error;J->Box->Quota=Q;J->Box->Capacity=Capacity;
    J->Promise=xrtPromiseCreate(&F,S->Cancel);if(!J->Promise)goto Error;J->Cancel=xrtPromiseCancelToken(J->Promise);if(!J->Cancel)goto Error;
    xrtAtomic32FetchAdd(&J->References,1,XMEMORY_RELAXED);
    if(!xrtPromiseProducerBindTakeV1(J->Promise,(xrtownershipref){J,&xrpcServerJobOps},&xrpcServerProducer)){xrpcServerJobDropRef(J);goto Error;}
    xrtMutexLock(&Q->Gate);
    if(!Q->Info.Accepting)A=XRPC_SERVER_CLOSED;
    else if(!xrtExecutorSubmit(S->Executor,xrpcServerRun,J,xrpcServerJobDrop,NULL)){
        xerrkind Kind=xrtErrorKind(xrtGetError());A=Kind==XERR_AGAIN?XRPC_SERVER_OVERLOADED:Kind==XERR_CLOSED?XRPC_SERVER_CLOSED:XRPC_SERVER_ADMISSION_ERROR;}
    if(A==XRPC_SERVER_ACCEPTED)++Q->Info.Accepted;else if(A!=XRPC_SERVER_ADMISSION_ERROR)++Q->Info.Rejected;
    xrtMutexUnlock(&Q->Gate);
    if(A==XRPC_SERVER_ACCEPTED){*Future=F;xrtClearError();xrtSetErrorTake(Ambient);return A;}
Error:
    {xerror* X=xrtTakeError();xpromise* P=J->Promise;J->Promise=NULL;xrtPromiseDestroy(P);xrtFutureDestroy(F);xrpcServerJobDropRef(J);xrtClearError();xrtSetErrorTake(X);}
NoJob:
    if(A==XRPC_SERVER_OVERLOADED || A==XRPC_SERVER_CLOSED){xrtClearError();xrtSetErrorTake(Ambient);return A;}
    xrtErrorFree(Ambient);return XRPC_SERVER_ADMISSION_ERROR;
}
XRPC_API bool xrpcServerInfo(xrpcserver* S,xrpcserverinfo* Info)
{
    if(!S || !Info || !xrtMemRangeValid(Info,sizeof(*Info)) || Info->Size!=sizeof(*Info) || xrtMemRangesOverlap(Info,sizeof(*Info),S,S->OwnedBytes))
        return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server info output");
    xrtMutexLock(&S->Quota->Gate);*Info=S->Quota->Info;xrtMutexUnlock(&S->Quota->Gate);return true;
}
XRPC_API bool xrpcServerClose(xrpcserver* S)
{if(!S)return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"server is NULL");xrtMutexLock(&S->Quota->Gate);S->Quota->Info.Accepting=false;xrtMutexUnlock(&S->Quota->Gate);return xrtExecutorClose(S->Executor);}
XRPC_API bool xrpcServerCancel(xrpcserver* S)
{
    if(!S)return xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"server is NULL");
    xerror* Ambient=xrtTakeError();
    xrtMutexLock(&S->Quota->Gate);S->Quota->Info.Accepting=false;S->Quota->Info.Cancelling=true;xrtMutexUnlock(&S->Quota->Gate);
    bool OK=(xrtCancelRequest(S->Cancel) || xrtCancelRequested(S->Cancel)) && xrtExecutorCancel(S->Executor);
    if(OK){xrtClearError();xrtSetErrorTake(Ambient);}else xrtErrorFree(Ambient);
    return OK;
}
XRPC_API xwaitresult xrpcServerWaitFor(xrpcserver* S,int64 Timeout)
{
    double Entered=xrtTimer();if(!S || Timeout<XRT_WAIT_FOREVER){xrpcServerReport(XERR_ARGUMENT,XRPC_SERVER_ARGUMENT,"invalid server wait");return XWAIT_ERROR;}
    double Deadline=xrpcServerDeadline(Entered,Timeout);if(!xrpcServerClose(S))return XWAIT_ERROR;
    xwaitresult R=xrtExecutorWaitFor(S->Executor,xrpcServerRemaining(Deadline));if(R!=XWAIT_OK)return R;
    xrpcserverquota* Q=S->Quota;xrtMutexLock(&Q->Gate);
    while(Q->Info.Operations){int64 Left=xrpcServerRemaining(Deadline);if(!Left){R=XWAIT_TIMEOUT;break;}
        R=xrtCondWaitFor(&Q->Idle,&Q->Gate,Left);if(R==XWAIT_ERROR)break;if(!Q->Info.Operations){R=XWAIT_OK;break;}if(R==XWAIT_TIMEOUT)break;}
    xrtMutexUnlock(&Q->Gate);return R;
}
XRPC_API bool xrpcServerDestroy(xrpcserver* S)
{
    if(!S)return true;
    if(xrpcServerWaitFor(S,XRT_WAIT_FOREVER)!=XWAIT_OK || !xrtExecutorDestroy(S->Executor))return false;
    xrtCancelDestroy(S->Cancel);xrpcServerQuotaDrop(S->Quota);xrtFree(S);return true;
}
#endif
#endif

#endif
