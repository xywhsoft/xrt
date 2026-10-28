#ifndef XRPC_TYPES_H
#define XRPC_TYPES_H
#include <xrpc/features.h>
#include <xrt/core.h>

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
