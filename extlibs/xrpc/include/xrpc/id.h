#ifndef XRPC_ID_H
#define XRPC_ID_H
#include <xrpc/features.h>
#include <xrpc/export.h>
#include <xrt/features.h>
#include <xrt/json.h>

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
