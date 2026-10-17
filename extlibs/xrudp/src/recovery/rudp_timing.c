#include <xrudp/timing.h>
#include <string.h>

static uint64_t xrudp__timing_add(uint64_t a, uint64_t b)
{
	return b > UINT64_MAX - a ? UINT64_MAX : a + b;
}

/* Exact floor(((divisor-1)*old + sample)/divisor), without overflowing
 * intermediate products, even at the full uint64_t input range. */
static uint64_t xrudp__timing_average(uint64_t old, uint64_t sample, uint64_t divisor)
{
	return (old / divisor) * (divisor - 1) + sample / divisor +
		((old % divisor) * (divisor - 1) + sample % divisor) / divisor;
}

static bool xrudp__timing_valid(const xrudprtt* rtt)
{
	return rtt != NULL && rtt->SmoothedUs != 0 &&
		rtt->MaxAckDelayUs <= XRUDP_ACK_DELAY_MAX_US;
}

xrudpresult xrtRudpRttInit(xrudprtt* pRtt, uint64_t initial, uint32_t maxDelay)
{
	xrudprtt rtt;
	if (pRtt == NULL || maxDelay > XRUDP_ACK_DELAY_MAX_US) return XRUDP_ARGUMENT;
	memset(&rtt, 0, sizeof(rtt));
	rtt.SmoothedUs = initial == 0 ? XRUDP_INITIAL_RTT_US : initial;
	rtt.VariationUs = rtt.SmoothedUs / 2;
	rtt.MaxAckDelayUs = maxDelay;
	*pRtt = rtt;
	return XRUDP_OK;
}

xrudpresult xrtRudpRttSample(xrudprtt* pRtt, uint64_t measured, uint32_t peerDelay)
{
	uint64_t adjusted, delay, difference;
	if (!xrudp__timing_valid(pRtt)) return XRUDP_ARGUMENT;
	if (measured == 0) measured = 1;
	pRtt->LatestUs = measured;
	if (!pRtt->HasSample) {
		pRtt->MinimumUs = measured;
		pRtt->SmoothedUs = measured;
		pRtt->VariationUs = measured / 2;
		pRtt->HasSample = true;
		return XRUDP_OK;
	}
	if (measured < pRtt->MinimumUs) pRtt->MinimumUs = measured;
	delay = peerDelay < pRtt->MaxAckDelayUs ? peerDelay : pRtt->MaxAckDelayUs;
	adjusted = measured;
	if (measured - pRtt->MinimumUs >= delay) adjusted -= delay;
	difference = adjusted > pRtt->SmoothedUs ? adjusted - pRtt->SmoothedUs :
		pRtt->SmoothedUs - adjusted;
	pRtt->VariationUs = xrudp__timing_average(pRtt->VariationUs, difference, 4);
	pRtt->SmoothedUs = xrudp__timing_average(pRtt->SmoothedUs, adjusted, 8);
	return XRUDP_OK;
}

xrudpresult xrtRudpRttPto(const xrudprtt* pRtt, unsigned int backoff, uint64_t* interval)
{
	uint64_t variance, base;
	if (!xrudp__timing_valid(pRtt) || interval == NULL) return XRUDP_ARGUMENT;
	variance = pRtt->VariationUs > UINT64_MAX / 4 ? UINT64_MAX : pRtt->VariationUs * 4;
	if (variance < XRUDP_TIMER_GRANULARITY_US) variance = XRUDP_TIMER_GRANULARITY_US;
	base = xrudp__timing_add(pRtt->SmoothedUs, variance);
	base = xrudp__timing_add(base, pRtt->MaxAckDelayUs);
	*interval = backoff >= 64 || base > (UINT64_MAX >> backoff) ? UINT64_MAX : base << backoff;
	return XRUDP_OK;
}

xrudpresult xrtRudpRttLossDelay(const xrudprtt* pRtt, uint64_t* interval)
{
	uint64_t largest, fraction, value;
	if (!xrudp__timing_valid(pRtt) || interval == NULL) return XRUDP_ARGUMENT;
	largest = pRtt->LatestUs > pRtt->SmoothedUs ? pRtt->LatestUs : pRtt->SmoothedUs;
	fraction = largest / 8 + (largest % 8 != 0 ? 1u : 0u);
	value = xrudp__timing_add(largest, fraction);
	*interval = value < XRUDP_TIMER_GRANULARITY_US ? XRUDP_TIMER_GRANULARITY_US : value;
	return XRUDP_OK;
}
