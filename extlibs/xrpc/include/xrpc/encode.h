#ifndef XRPC_ENCODE_H
#define XRPC_ENCODE_H
#include <xrpc/features.h>
#include <xrpc/export.h>
#include <xrt/features.h>
#include <xrt/json.h>
#include <xrpc/types.h>

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
