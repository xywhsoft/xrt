#ifndef XRUDP_INTERNAL_LOSS_H
#define XRUDP_INTERNAL_LOSS_H

#include <xrudp/sent.h>
#include <string.h>

/* Separate bounded ownership history: stream references may be reused before
 * older packets finish. Retire only a complete prefix, in send order. */
typedef struct xrudp__lossrecord {
	uint64_t Number, At;
	uint32_t Flags;
	uint8_t Status;
	bool Sampled, NetworkLoss;
} xrudp__lossrecord;
typedef struct xrudp__lossproof {
	xrudp__lossrecord* Records;
	uint32_t Capacity, Head, Count;
	uint64_t RetiredThrough, FirstNumber, FirstAt, LastAt;
	bool Collapsed;
} xrudp__lossproof;

static xrudp__lossrecord* xrudp__loss_at(const xrudp__lossproof* p, uint32_t index)
{ return &p->Records[(p->Head + index) % p->Capacity]; }
static bool xrudp__loss_endpoint(uint32_t flags)
{ return (flags & XRUDP_SENT_ACK_ELICITING) && !(flags & XRUDP_SENT_MTU); }
static bool xrudp__loss_record(xrudp__lossproof* p, uint64_t number, uint64_t at, uint32_t flags, bool sampled)
{
	if (p->Count == p->Capacity) return false;
	xrudp__lossrecord* r = xrudp__loss_at(p, p->Count++);
	memset(r, 0, sizeof(*r)); r->Number = number; r->At = at; r->Flags = flags; r->Sampled = sampled;
	return true;
}
static void xrudp__loss_restart(xrudp__lossproof* p)
{ p->FirstNumber = p->FirstAt = p->LastAt = 0; p->Collapsed = false; }
static uint32_t xrudp__loss_lower(const xrudp__lossproof* p, uint64_t number)
{
	uint32_t first = 0, last = p->Count;
	while (first < last) {
		uint32_t middle = first + (last - first) / 2;
		if (xrudp__loss_at(p, middle)->Number < number) first = middle + 1; else last = middle;
	}
	return first;
}
static void xrudp__loss_ack(xrudp__lossproof* p, const xrudpack* ack)
{
	/* Late ACKs have no second ownership event. An ACK for a retired lost or
 * expired ACK-only packet still invalidates the summarized loss prefix.
 * Forget the whole prefix if its intermediate times were already retired. */
	if (p->FirstNumber) for (size_t i = 0; i < ack->Count; ++i)
		if (ack->Ranges[i].Last >= p->FirstNumber && ack->Ranges[i].First <= p->RetiredThrough) {
			xrudp__loss_restart(p); break;
		}
	/* Finished packets behind an older pending packet have not retired yet.
 * A late ACK must be a barrier even though it cannot emit ownership twice. */
	for (size_t i = 0; i < ack->Count; ++i)
		for (uint32_t j = xrudp__loss_lower(p, ack->Ranges[i].First); j < p->Count; ++j) {
			xrudp__lossrecord* r = xrudp__loss_at(p, j);
			if (r->Number > ack->Ranges[i].Last) break;
			if (r->Status) { r->Status = XRUDP_SENT_ACKED; r->NetworkLoss = false; }
		}
}
static bool xrudp__loss_finish(xrudp__lossproof* p, const xrudpsentevent* event, bool network)
{
	uint32_t first = xrudp__loss_lower(p, event->PacketNumber);
	/* Pure ACK/MTU metadata may leave this proof ring before its ownership
 * ledger completes. Its eventual ACK is a barrier, never a second loss. */
	if (event->PacketNumber <= p->RetiredThrough && !xrudp__loss_endpoint(event->Flags)) {
		if (event->Kind == XRUDP_SENT_ACKED && p->FirstNumber && event->PacketNumber >= p->FirstNumber) xrudp__loss_restart(p);
		return true;
	}
	if (first == p->Count) return false;
	xrudp__lossrecord* r = xrudp__loss_at(p, first);
	if (r->Number != event->PacketNumber || r->Status || r->At != event->SentAtUs || r->Flags != event->Flags) return false;
	r->Status = (uint8_t)event->Kind; r->NetworkLoss = network && event->Kind == XRUDP_SENT_LOST;
	return true;
}
static uint32_t xrudp__loss_period(xrudp__lossproof* p, uint64_t duration)
{
	if (!p->FirstNumber || p->Collapsed || p->LastAt - p->FirstAt <= duration) return 0;
	p->Collapsed = true; return 1;
}
static uint32_t xrudp__loss_retire(xrudp__lossproof* p, uint64_t duration)
{
	/* A new RTT sample may shorten the threshold even when the lost prefix
 * was already retired. Check it before a later ACK closes that period. */
	uint32_t periods = xrudp__loss_period(p, duration);
	while (p->Count && (xrudp__loss_at(p, 0)->Status || !xrudp__loss_endpoint(xrudp__loss_at(p, 0)->Flags))) {
		xrudp__lossrecord* r = xrudp__loss_at(p, 0);
		if (r->Status == XRUDP_SENT_ACKED || r->Status == XRUDP_SENT_CANCELLED) xrudp__loss_restart(p);
		else if (xrudp__loss_endpoint(r->Flags)) {
			if (r->NetworkLoss && r->Sampled) {
				if (!p->FirstNumber) { p->FirstNumber = r->Number; p->FirstAt = r->At; }
				p->LastAt = r->At; periods += xrudp__loss_period(p, duration);
			} else xrudp__loss_restart(p); /* Local discard/no RTT is not loss evidence. */
		}
		/* Unacknowledged MTU/ACK-only metadata cannot start/end a period. Its
 * later real ACK is still a barrier via the retained prefix range above. */
		p->RetiredThrough = r->Number;
		memset(r, 0, sizeof(*r)); p->Head = (p->Head + 1) % p->Capacity; --p->Count;
	}
	return periods;
}
#endif
