#include <xrudp/congestion.h>
#include <string.h>

static bool xrudp__cc_valid(const xrudpcongestion* s)
{
	return s != NULL && s->MaximumWindow >= XRUDP_CC_INITIAL_WINDOW &&
		s->MaximumWindow <= XRUDP_CC_MAXIMUM_LIMIT && s->Window >= XRUDP_CC_MINIMUM_WINDOW &&
		s->Window <= s->MaximumWindow && s->SlowStartThreshold >= XRUDP_CC_MINIMUM_WINDOW &&
		s->RecoveryThrough <= s->LargestSent && s->AvoidanceCredit < s->Window &&
		s->BytesInFlight <= s->MaximumWindow + XRUDP_SENT_PROBE_RESERVE * XRUDP_PACKET_MAX;
}

static bool xrudp__cc_packet(uint32_t bytes, uint32_t flags)
{
	return bytes >= XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + XRUDP_FRAME_HEADER_SIZE &&
		bytes <= XRUDP_PACKET_MAX && (flags & ~31u) == 0 &&
		(!(flags & XRUDP_SENT_ACK_ONLY) || flags == XRUDP_SENT_ACK_ONLY) &&
		(!(flags & XRUDP_SENT_MTU) || ((flags & 7u) == (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING) && bytes >= 512)) &&
		((flags & XRUDP_SENT_PROBE) == 0 || (flags & XRUDP_SENT_ACK_ELICITING) != 0);
}

static bool xrudp__cc_flight(uint32_t flags)
{
	return (flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0;
}

static uint64_t xrudp__cc_add(uint64_t a, uint64_t b)
{
	return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

/* ceil(rtt * bytes / window), without a wide-integer compiler dependency.
 * window<=64MiB and bytes<=2400 keep the remainder product bounded. */
static uint64_t xrudp__cc_interval(uint64_t rtt, uint32_t bytes, uint64_t window)
{
	uint64_t quotient = rtt / window, remainder = (rtt % window) * bytes;
	uint64_t tail = remainder / window + (remainder % window != 0 ? 1u : 0u);
	if (bytes != 0 && quotient > UINT64_MAX / bytes) return UINT64_MAX;
	return xrudp__cc_add(quotient * bytes, tail);
}

xrudpresult xrtRudpCongestionInit(xrudpcongestion* s, uint64_t maximum)
{
	xrudpcongestion next;
	if (maximum == 0) maximum = XRUDP_CC_DEFAULT_MAXIMUM;
	if (s == NULL || maximum < XRUDP_CC_INITIAL_WINDOW || maximum > XRUDP_CC_MAXIMUM_LIMIT)
		return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	next.Window = XRUDP_CC_INITIAL_WINDOW; next.MaximumWindow = maximum;
	next.SlowStartThreshold = UINT64_MAX;
	*s = next;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionCanSend(const xrudpcongestion* s,
	uint64_t now, uint64_t rtt, uint32_t bytes, uint32_t flags, uint64_t* due)
{
	uint64_t limit, tolerance, earliest;
	if (!xrudp__cc_valid(s) || due == NULL || !xrudp__cc_packet(bytes, flags) ||
		rtt == 0 || now < s->LastNowUs) return XRUDP_ARGUMENT;
	if (!xrudp__cc_flight(flags)) { *due = now; return XRUDP_OK; }
	limit = (flags & XRUDP_SENT_PROBE) != 0 ?
		s->MaximumWindow + XRUDP_SENT_PROBE_RESERVE * XRUDP_PACKET_MAX : s->Window;
	if (s->BytesInFlight > limit || bytes > limit - s->BytesInFlight) return XRUDP_BACKPRESSURE;
	/* A two-datagram GCRA burst bound; idle time cannot accumulate more credit. */
	tolerance = xrudp__cc_interval(rtt, XRUDP_CC_BURST_BYTES - bytes, s->Window);
	earliest = s->PacerNextUs > tolerance ? s->PacerNextUs - tolerance : 0;
	*due = earliest > now ? earliest : now;
	return earliest > now ? XRUDP_TIMEOUT : XRUDP_OK;
}

xrudpresult xrtRudpCongestionOnSent(xrudpcongestion* s,
	uint64_t number, uint64_t now, uint64_t rtt, uint32_t bytes, uint32_t flags)
{
	uint64_t due, interval;
	xrudpresult result;
	if (s == NULL || number == 0 || number == UINT64_MAX || number <= s->LargestSent)
		return XRUDP_ARGUMENT;
	result = xrtRudpCongestionCanSend(s, now, rtt, bytes, flags, &due);
	if (result != XRUDP_OK) return result;
	if (xrudp__cc_flight(flags)) {
		interval = xrudp__cc_interval(rtt, bytes, s->Window);
		if (interval == 0) interval = 1;
		s->PacerNextUs = xrudp__cc_add(s->PacerNextUs > now ? s->PacerNextUs : now, interval);
		s->BytesInFlight += bytes;
	}
	s->LargestSent = number; s->LastNowUs = now;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionApply(xrudpcongestion* s, const xrudpsentevent* events,
	size_t count, uint64_t now, bool limited)
{
	xrudpcongestion next;
	uint64_t removed = 0, newestLoss = 0;
	if (!xrudp__cc_valid(s) || (events == NULL && count != 0) ||
		count > XRUDP_SENT_CAPACITY_MAX + XRUDP_SENT_PROBE_RESERVE || now < s->LastNowUs)
		return XRUDP_ARGUMENT;
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* p = &events[i];
		if (!xrudp__cc_packet(p->Bytes, p->Flags) || p->PacketNumber == 0 ||
			p->PacketNumber > s->LargestSent || p->SentAtUs > now || p->Reserved != 0 ||
			p->Kind < XRUDP_SENT_ACKED || p->Kind > XRUDP_SENT_CANCELLED ||
			(p->Kind == XRUDP_SENT_EXPIRED && xrudp__cc_flight(p->Flags))) return XRUDP_ARGUMENT;
		if (xrudp__cc_flight(p->Flags)) {
			removed += p->Bytes;
			if (p->Kind == XRUDP_SENT_LOST && !(p->Flags & XRUDP_SENT_MTU) && p->PacketNumber > newestLoss) newestLoss = p->PacketNumber;
		}
	}
	if (removed > s->BytesInFlight) return XRUDP_ARGUMENT;
	next = *s; next.BytesInFlight -= removed; next.LastNowUs = now;
	if (newestLoss > next.RecoveryThrough) {
		next.RecoveryThrough = next.LargestSent;
		next.Window /= 2;
		if (next.Window < XRUDP_CC_MINIMUM_WINDOW) next.Window = XRUDP_CC_MINIMUM_WINDOW;
		next.SlowStartThreshold = next.Window; next.AvoidanceCredit = 0;
	}
	for (size_t i = 0; i < count; ++i) {
		const xrudpsentevent* p = &events[i];
		if (limited || p->Kind != XRUDP_SENT_ACKED || (p->Flags & XRUDP_SENT_MTU) || !xrudp__cc_flight(p->Flags) ||
			p->PacketNumber <= next.RecoveryThrough || next.Window == next.MaximumWindow) continue;
		if (next.Window < next.SlowStartThreshold) {
			next.Window += p->Bytes;
		} else {
			next.AvoidanceCredit += p->Bytes;
			if (next.AvoidanceCredit >= next.Window) {
				next.AvoidanceCredit -= next.Window;
				next.Window += XRUDP_PACKET_MAX;
			}
		}
		if (next.Window >= next.MaximumWindow) {
			next.Window = next.MaximumWindow; next.AvoidanceCredit = 0;
		}
	}
	*s = next;
	return XRUDP_OK;
}

xrudpresult xrtRudpCongestionCollapse(xrudpcongestion* s, uint64_t now)
{
	if (!xrudp__cc_valid(s) || s->LargestSent == 0 || now < s->LastNowUs) return XRUDP_ARGUMENT;
	s->SlowStartThreshold = s->Window / 2;
	if (s->SlowStartThreshold < XRUDP_CC_MINIMUM_WINDOW) s->SlowStartThreshold = XRUDP_CC_MINIMUM_WINDOW;
	s->Window = XRUDP_CC_MINIMUM_WINDOW; s->AvoidanceCredit = 0;
	s->RecoveryThrough = s->LargestSent; s->LastNowUs = now;
	return XRUDP_OK;
}
