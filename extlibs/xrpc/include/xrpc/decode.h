#ifndef XRPC_DECODE_H
#define XRPC_DECODE_H
#include <xrpc/features.h>
#include <xrpc/export.h>
#include <xrt/features.h>
#include <xrt/json.h>
#include <xrpc/types.h>

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
