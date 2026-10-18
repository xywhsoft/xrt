#ifndef XRUDP_DISCOVERY_H
#define XRUDP_DISCOVERY_H
#include <xrudp/features.h>
#include <xrudp/stun.h>
#if defined(XRUDP_FEATURE_RUDP_DISCOVERY)

#define XRUDP_DISCOVERY_ATTEMPTS_MAX 7u
#define XRUDP_DISCOVERY_DEFAULT_TIMEOUT_MS 39500u

/* Safe to publish: mappings are hints, never peer pins or identity evidence.
 * Reason OK=pending, DONE=mapping learned, other values terminal failure.
 * NextUs is the next useful clock, including the absolute nonrenewable limit. */
typedef struct xrudpdiscoverystate {
	xrudpaddress Server, Mapped;
	xrudpresult Reason;
	uint64_t StartedUs, DeadlineUs, NextUs, Discarded;
	uint32_t Attempts;
	uint16_t RemoteError;
	bool Fingerprint;
} xrudpdiscoverystate;

/* Caller-owned fixed storage. Initialize before use; single serialized owner.
 * Underscore members are private, not a serialization or logging interface.
 * No I/O, allocation or global clock. Randomness is needed only at Init.
 * The object, I/O buffers and output counters must not overlap. */
typedef struct xrudpdiscovery {
	xrudpdiscoverystate State;
	uint64_t _Now, _SendAt, _FinishAt;
	unsigned char _Transaction[XRUDP_STUN_TRANSACTION_SIZE], _Request[28];
	uint8_t _Size;
	bool _Offered;
} xrudpdiscovery;

XRT_EXTERN_C_BEGIN
/* A trusted application chooses Server, which must be a valid unicast address.
 * Timeout=0 uses 39.5s; explicit 1000..60000ms is an absolute upper bound, not
 * renewed by backpressure/noise/retransmission. Fingerprint mode requires it
 * in responses; false permits absent but verifies any present fingerprint.
 * Failure preserves the object. No credentials, redirect or DNS mechanism. */
XRUDP_API xrudpresult xrtRudpDiscoveryInit(xrudpdiscovery* pDiscovery, const xrudpaddress* pServer,
	uint64_t iNowUs, uint32_t iTimeoutMs, bool bFingerprint);
/* Offers the same request until Sent confirms actual UDP admission. OK returns
 * its bytes; TIMEOUT means pending and *pNextUs is a future useful deadline,
 * or State.Reason=TIMEOUT for terminal expiry. Never consume attempts for
 * local backpressure. Output counters zeroed; invalid calls preserve state. */
XRUDP_API xrudpresult xrtRudpDiscoveryNext(xrudpdiscovery* pDiscovery, uint64_t iNowUs,
	void* pOutput, size_t iCapacity, size_t* pWritten, uint64_t* pNextUs);
XRUDP_API xrudpresult xrtRudpDiscoverySent(xrudpdiscovery* pDiscovery, uint64_t iNowUs);
/* Only the exact configured source and random transaction may complete it,
 * after at least one admitted request. Noise returns BACKPRESSURE; matched
 * unsupported/remote-error responses terminate with PROTOCOL_MISMATCH. Bad
 * framing, tags/fingerprint or missing required mapping cannot renew limits.
 * Success gives DONE; no device authentication or reliable-session admission. */
XRUDP_API xrudpresult xrtRudpDiscoveryInput(xrudpdiscovery* pDiscovery, const xrudpaddress* pSource,
	const void* pData, size_t iSize, uint64_t iNowUs);
XRUDP_API xrudpresult xrtRudpDiscoveryCancel(xrudpdiscovery* pDiscovery);
XRUDP_API xrudpresult xrtRudpDiscoveryState(const xrudpdiscovery* pDiscovery, xrudpdiscoverystate* pState);
XRT_EXTERN_C_END
#endif
#endif
