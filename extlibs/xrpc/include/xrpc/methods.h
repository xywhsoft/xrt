#ifndef XRPC_METHODS_H
#define XRPC_METHODS_H
#include <xrpc/dispatch.h>

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
