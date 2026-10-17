#ifndef XRUDP_CONGESTION_H
#define XRUDP_CONGESTION_H

#include <xrudp/features.h>
#include <xrudp/sent.h>

#if defined(XRUDP_FEATURE_RUDP_CONGESTION)

#define XRUDP_CC_INITIAL_WINDOW (10u * XRUDP_PACKET_MAX)
#define XRUDP_CC_MINIMUM_WINDOW (2u * XRUDP_PACKET_MAX)
#define XRUDP_CC_DEFAULT_MAXIMUM (4u * 1024u * 1024u)
#define XRUDP_CC_MAXIMUM_LIMIT (64u * 1024u * 1024u)
#define XRUDP_CC_BURST_BYTES (2u * XRUDP_PACKET_MAX)

/* One owner; initialize before use. Read-only snapshots are ordinary copies.
 * This is local sender policy, never populated from peer-provided state. */
typedef struct xrudpcongestion {
	uint64_t Window;
	uint64_t SlowStartThreshold;
	uint64_t BytesInFlight;
	uint64_t MaximumWindow;
	uint64_t RecoveryThrough;
	uint64_t LargestSent;
	uint64_t AvoidanceCredit;
	uint64_t PacerNextUs;
	uint64_t LastNowUs;
} xrudpcongestion;

XRT_EXTERN_C_BEGIN

/* Maximum=0 defaults to 4MiB; allowed range is initial window..64MiB.
 * Fixed datagram ceiling 1200, initial window=10 datagrams, minimum=2. */
XRUDP_API xrudpresult xrtRudpCongestionInit(xrudpcongestion* pState, uint64_t iMaximumWindow);
/* A pure admission query. BACKPRESSURE: cwnd/hard flight budget; TIMEOUT: wait
 * until *pDueUs for pacing; OK: admitted now. Due is unchanged on other errors.
 * ACK-only bypasses cwnd/pacing; endpoint still must bound control traffic.
 * PROBE may exceed cwnd but not the hard budget and never bypasses pacing.
 * Only the recovery driver may grant up to two PROBE packets per actual PTO. */
XRUDP_API xrudpresult xrtRudpCongestionCanSend(const xrudpcongestion* pState,
	uint64_t iNowUs, uint64_t iSmoothedRttUs, uint32_t iBytes,
	uint32_t iFlags, uint64_t* pDueUs);
/* Commit after UDP admission under the same serialized owner as CanSend and
 * the sent ledger. Records all DATA packet numbers, including ACK-only. */
XRUDP_API xrudpresult xrtRudpCongestionOnSent(xrudpcongestion* pState,
	uint64_t iPacketNumber, uint64_t iNowUs, uint64_t iSmoothedRttUs,
	uint32_t iBytes, uint32_t iFlags);
/* Consume unique final events from Sent exactly once, before any new send.
 * Caller must combine all loss and ACK events from the same ACK processing in
 * one batch (losses are applied before growth regardless of array order).
 * Application-limited or flow-control-limited ACKs do not increase cwnd.
 * No allocations/callbacks. Errors leave the entire state unchanged. */
XRUDP_API xrudpresult xrtRudpCongestionApply(xrudpcongestion* pState,
	const xrudpsentevent* pEvents, size_t iCount, uint64_t iNowUs,
	bool bApplicationLimited);
/* Call only after the recovery driver proves a persistent congestion period,
 * not on a single PTO. The driver owns the consecutive-loss/RTT evidence. */
XRUDP_API xrudpresult xrtRudpCongestionCollapse(xrudpcongestion* pState, uint64_t iNowUs);

XRT_EXTERN_C_END
#endif
#endif
