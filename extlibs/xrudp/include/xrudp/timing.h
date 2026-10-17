#ifndef XRUDP_TIMING_H
#define XRUDP_TIMING_H

#include <xrudp/features.h>
#include <xrudp/wire.h>

#if defined(XRUDP_FEATURE_RUDP_TIMING)

#define XRUDP_INITIAL_RTT_US UINT64_C(333000)
#define XRUDP_TIMER_GRANULARITY_US UINT64_C(1000)
#define XRUDP_ACK_DELAY_MAX_US 25000u

typedef struct xrudprtt {
	uint64_t LatestUs;
	uint64_t MinimumUs;
	uint64_t SmoothedUs;
	uint64_t VariationUs;
	uint32_t MaxAckDelayUs;
	bool HasSample;
} xrudprtt;

XRT_EXTERN_C_BEGIN

/* All units are monotonic microseconds. InitialRtt=0 selects 333ms.
 * MaxAckDelay may be zero (handshake/low-latency profile) and must not exceed
 * the fixed 25ms protocol ceiling. No global clock, allocation or I/O.
 * Owner serializes calls; failure preserves output/state. */
XRUDP_API xrudpresult xrtRudpRttInit(xrudprtt* pRtt,
	uint64_t iInitialRttUs, uint32_t iMaxAckDelayUs);
/* Sample is the measured send-to-ACK interval, never a peer-supplied RTT.
 * A zero measured interval is rounded to 1us. Peer ACK delay is capped by
 * local policy and cannot reduce the sample below observed minimum RTT. */
XRUDP_API xrudpresult xrtRudpRttSample(xrudprtt* pRtt,
	uint64_t iMeasuredUs, uint32_t iPeerAckDelayUs);
/* PTO = smoothed + max(4*variation, granularity) + bounded max ACK delay,
 * with exponential backoff. Integer overflow saturates at UINT64_MAX.
 * Reset backoff only on real authenticated acknowledgment progress. */
XRUDP_API xrudpresult xrtRudpRttPto(const xrudprtt* pRtt,
	unsigned int iBackoff, uint64_t* pIntervalUs);
/* ceil(9/8 * max(latest,smoothed)), no less than timer granularity. */
XRUDP_API xrudpresult xrtRudpRttLossDelay(const xrudprtt* pRtt,
	uint64_t* pIntervalUs);

XRT_EXTERN_C_END

#endif
#endif
