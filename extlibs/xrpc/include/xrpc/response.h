#ifndef XRPC_RESPONSE_H
#define XRPC_RESPONSE_H
#include <xrpc/decode.h>
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
