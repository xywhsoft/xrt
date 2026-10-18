#include <xrudp/datagram.h>
#include <xrt/memory.h>
#include <string.h>

#define XRUDP__DATAGRAM_NONE UINT32_MAX
typedef struct xrudp__datagramchannel {
	xrudpdatagramchannel Config;
	uint64_t SendSequence, ReceiveSequence;
	uint32_t Head, Tail, Latest, SendQueued, ReceiveQueued, Deficit;
} xrudp__datagramchannel;
typedef struct xrudp__datagramslot {
	xrudpdatagraminfo Info;
	uint32_t Channel, Next;
	bool Used;
} xrudp__datagramslot;
struct xrudpdatagrams {
	xrudpdatagramstate State;
	size_t Allocation;
	uint64_t SendTicket, ReceiveTicket;
	uint32_t Cursor, ReceiveHead, ReceiveTail;
	xrudp__datagramchannel* Channel;
	xrudp__datagramslot* Slot;
	unsigned char* Data;
};
static void xrudp__dg_count(uint64_t* value) { if (*value != UINT64_MAX) ++*value; }
static uint64_t xrudp__dg_add(uint64_t a, uint64_t b) { return a > UINT64_MAX - b ? UINT64_MAX : a + b; }
static uint32_t xrudp__dg_find(const xrudpdatagrams* d, uint32_t id)
{
	for (uint32_t i = 0; i < d->State.Channels; ++i) if (d->Channel[i].Config.Id == id) return i;
	return XRUDP__DATAGRAM_NONE;
}
static unsigned char* xrudp__dg_data(const xrudpdatagrams* d, uint32_t slot) { return d->Data + (size_t)slot * d->State.Config.PayloadCapacity; }
static uint32_t xrudp__dg_free(const xrudpdatagrams* d, bool receive)
{
	uint32_t start = receive ? d->State.Config.SendCapacity : 0;
	uint32_t end = receive ? start + d->State.Config.ReceiveCapacity : d->State.Config.SendCapacity;
	for (uint32_t i = start; i < end; ++i) if (!d->Slot[i].Used) return i;
	return XRUDP__DATAGRAM_NONE;
}
static void xrudp__dg_clear(xrudpdatagrams* d, uint32_t slot, bool receive)
{
	xrudp__datagramslot* p = &d->Slot[slot]; xrudp__datagramchannel* c = &d->Channel[p->Channel];
	if (receive) { --d->State.ReceiveQueued; d->State.ReceiveBytes -= p->Info.Size; --c->ReceiveQueued; if (c->Latest == slot) c->Latest = XRUDP__DATAGRAM_NONE; }
	else { --d->State.SendQueued; d->State.SendBytes -= p->Info.Size; if (--c->SendQueued == 0) c->Deficit = 0; }
	xrtSecureZero(xrudp__dg_data(d, slot), p->Info.Size); memset(p, 0, sizeof(*p));
}
void xrtRudpDatagramConfigInit(xrudpdatagramconfig* c)
{
	if (c != NULL) { c->Channels = 16; c->SendCapacity = c->ReceiveCapacity = 32; c->PayloadCapacity = XRUDP_DATAGRAM_PAYLOAD_MAX; }
}
void xrtRudpDatagramChannelInit(xrudpdatagramchannel* c)
{
	if (c != NULL) { memset(c, 0, sizeof(*c)); c->Priority = 1; c->Weight = 1; }
}
xrudpresult xrtRudpDatagramAllocationSize(const xrudpdatagramconfig* c, size_t* output)
{
	if (c == NULL || output == NULL || c->Channels == 0 || c->Channels > XRUDP_DATAGRAM_CHANNELS_MAX ||
		c->SendCapacity == 0 || c->SendCapacity > XRUDP_DATAGRAM_QUEUE_MAX || c->ReceiveCapacity == 0 ||
		c->ReceiveCapacity > XRUDP_DATAGRAM_QUEUE_MAX || c->PayloadCapacity == 0 || c->PayloadCapacity > XRUDP_DATAGRAM_PAYLOAD_MAX) return XRUDP_ARGUMENT;
	*output = sizeof(xrudpdatagrams) + c->Channels * sizeof(xrudp__datagramchannel) +
		(c->SendCapacity + c->ReceiveCapacity) * (sizeof(xrudp__datagramslot) + c->PayloadCapacity);
	return XRUDP_OK;
}
xrudpresult xrtRudpDatagramCreate(const xrudpdatagramconfig* c, xrudpdatagrams** output)
{
	size_t size; if (output == NULL || xrtRudpDatagramAllocationSize(c, &size) != XRUDP_OK) return XRUDP_ARGUMENT;
	xrudpdatagrams* d = (xrudpdatagrams*)xrtMalloc(size); if (d == NULL) return XRUDP_NO_MEMORY;
	memset(d, 0, size); d->State.Config = *c; d->State.NextDeadlineUs = UINT64_MAX; d->Allocation = size;
	d->Channel = (xrudp__datagramchannel*)(d + 1); d->Slot = (xrudp__datagramslot*)(d->Channel + c->Channels);
	d->Data = (unsigned char*)(d->Slot + c->SendCapacity + c->ReceiveCapacity); d->ReceiveHead = d->ReceiveTail = XRUDP__DATAGRAM_NONE;
	*output = d; return XRUDP_OK;
}
void xrtRudpDatagramDestroy(xrudpdatagrams* d)
{
	if (d != NULL) { size_t size = d->Allocation; xrtSecureZero(d, size); xrtFree(d); }
}
xrudpresult xrtRudpDatagramAbort(xrudpdatagrams* d)
{
	if (d == NULL) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_DONE;
	for (uint32_t i = 0; i < d->State.Config.SendCapacity + d->State.Config.ReceiveCapacity; ++i) if (d->Slot[i].Used) {
		bool receive = i >= d->State.Config.SendCapacity;
		xrudp__dg_clear(d, i, receive);
		xrudp__dg_count(receive ? &d->State.ReceiveCancelled : &d->State.SendCancelled);
	}
	for (uint32_t i = 0; i < d->State.Channels; ++i) d->Channel[i].Head = d->Channel[i].Tail = d->Channel[i].Latest = XRUDP__DATAGRAM_NONE;
	d->ReceiveHead = d->ReceiveTail = XRUDP__DATAGRAM_NONE; d->State.Stopped = true; return XRUDP_OK;
}
xrudpresult xrtRudpDatagramConfigure(xrudpdatagrams* d, const xrudpdatagramchannel* c)
{
	if (d == NULL || c == NULL || c->Id == 0 || c->Mode > XRUDP_DATAGRAM_LATEST || c->Priority > 3 || c->Weight == 0 || c->Weight > 16 ||
		c->ReceiveLifetimeUs > XRUDP_DATAGRAM_LIFETIME_MAX_US) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	uint32_t index = xrudp__dg_find(d, c->Id);
	if (index != XRUDP__DATAGRAM_NONE) {
		const xrudpdatagramchannel* old = &d->Channel[index].Config;
		return old->Mode == c->Mode && old->Priority == c->Priority && old->Weight == c->Weight &&
			old->ReplacePending == c->ReplacePending && old->ReceiveLifetimeUs == c->ReceiveLifetimeUs ? XRUDP_DUPLICATE : XRUDP_ARGUMENT;
	}
	if (d->State.Channels == d->State.Config.Channels) return XRUDP_LIMIT;
	xrudp__datagramchannel* p = &d->Channel[d->State.Channels++]; p->Config = *c;
	p->Head = p->Tail = p->Latest = XRUDP__DATAGRAM_NONE; return XRUDP_OK;
}
xrudpresult xrtRudpDatagramChannelState(const xrudpdatagrams* d, uint32_t id, xrudpdatagramchannelstate* output)
{
	if (d == NULL || output == NULL) return XRUDP_ARGUMENT;
	uint32_t index = xrudp__dg_find(d, id); if (index == XRUDP__DATAGRAM_NONE) return XRUDP_ARGUMENT;
	const xrudp__datagramchannel* c = &d->Channel[index]; xrudpdatagramchannelstate s; memset(&s, 0, sizeof(s));
	s.Config = c->Config; s.SendSequence = c->SendSequence; s.ReceiveSequence = c->ReceiveSequence;
	s.SendQueued = c->SendQueued; s.ReceiveQueued = c->ReceiveQueued; s.Deficit = c->Deficit; *output = s; return XRUDP_OK;
}
xrudpresult xrtRudpDatagramState(const xrudpdatagrams* d, xrudpdatagramstate* output)
{
	if (d == NULL || output == NULL) return XRUDP_ARGUMENT;
	xrudpdatagramstate s; memcpy(&s, &d->State, sizeof(s)); s.NextDeadlineUs = UINT64_MAX;
	for (uint32_t i = 0; i < s.Config.SendCapacity + s.Config.ReceiveCapacity; ++i) if (d->Slot[i].Used &&
		d->Slot[i].Info.DeadlineUs != 0 && d->Slot[i].Info.DeadlineUs < s.NextDeadlineUs) s.NextDeadlineUs = d->Slot[i].Info.DeadlineUs;
	memcpy(output, &s, sizeof(s)); return XRUDP_OK;
}
xrudpresult xrtRudpDatagramMaintain(xrudpdatagrams* d, uint64_t now)
{
	if (d == NULL || now < d->State.LastNowUs || now == UINT64_MAX) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	if (now == d->State.LastNowUs) return XRUDP_OK;
	d->State.LastNowUs = now;
	for (uint32_t i = 0; i < d->State.Channels; ++i) {
		xrudp__datagramchannel* c = &d->Channel[i]; uint32_t prev = XRUDP__DATAGRAM_NONE, at = c->Head;
		while (at != XRUDP__DATAGRAM_NONE) {
			xrudp__datagramslot* p = &d->Slot[at]; uint32_t next = p->Next;
			if (p->Info.DeadlineUs != 0 && now >= p->Info.DeadlineUs) {
				if (prev == XRUDP__DATAGRAM_NONE) c->Head = next; else d->Slot[prev].Next = next;
				if (c->Tail == at) c->Tail = prev;
				xrudp__dg_clear(d, at, false); xrudp__dg_count(&d->State.SendExpired);
			} else prev = at;
			at = next;
		}
	}
	uint32_t prev = XRUDP__DATAGRAM_NONE, at = d->ReceiveHead;
	while (at != XRUDP__DATAGRAM_NONE) {
		xrudp__datagramslot* p = &d->Slot[at]; uint32_t next = p->Next;
		if (p->Info.DeadlineUs != 0 && now >= p->Info.DeadlineUs) {
			if (prev == XRUDP__DATAGRAM_NONE) d->ReceiveHead = next; else d->Slot[prev].Next = next;
			if (d->ReceiveTail == at) d->ReceiveTail = prev;
			xrudp__dg_clear(d, at, true); xrudp__dg_count(&d->State.ReceiveExpired);
		} else prev = at;
		at = next;
	}
	return XRUDP_OK;
}
xrudpresult xrtRudpDatagramWrite(xrudpdatagrams* d, uint32_t id, const void* data, size_t size, uint64_t now, uint64_t deadline, uint64_t* sequence)
{
	if (d == NULL || sequence == NULL || (data == NULL && size != 0) || now < d->State.LastNowUs || now == UINT64_MAX ||
		(deadline > now && deadline - now > XRUDP_DATAGRAM_LIFETIME_MAX_US)) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	uint32_t index = xrudp__dg_find(d, id); if (index == XRUDP__DATAGRAM_NONE) return XRUDP_ARGUMENT;
	xrudp__datagramchannel* c = &d->Channel[index];
	if (size > d->State.Config.PayloadCapacity) return XRUDP_LIMIT;
	if (deadline != 0 && deadline <= now) return XRUDP_TIMEOUT;
	if (d->SendTicket == UINT64_MAX || (c->Config.Mode != XRUDP_DATAGRAM_INDEPENDENT && c->SendSequence == UINT64_MAX)) return XRUDP_LIMIT;
	(void)xrtRudpDatagramMaintain(d, now);
	bool replace = c->Config.ReplacePending && c->Head != XRUDP__DATAGRAM_NONE;
	uint32_t slot = replace ? c->Head : xrudp__dg_free(d, false);
	if (slot == XRUDP__DATAGRAM_NONE) return XRUDP_BACKPRESSURE;
	xrudp__datagramslot* p = &d->Slot[slot];
	if (replace) {
		d->State.SendBytes -= p->Info.Size; xrtSecureZero(xrudp__dg_data(d, slot), p->Info.Size); xrudp__dg_count(&d->State.SendReplaced);
	} else {
		p->Used = true; p->Channel = index; p->Next = XRUDP__DATAGRAM_NONE;
		if (c->Tail != XRUDP__DATAGRAM_NONE) d->Slot[c->Tail].Next = slot; else c->Head = slot;
		c->Tail = slot; ++c->SendQueued; ++d->State.SendQueued;
	}
	memset(&p->Info, 0, sizeof(p->Info)); p->Info.Channel = id; p->Info.Size = (uint32_t)size; p->Info.Ticket = ++d->SendTicket;
	p->Info.AtUs = now; p->Info.DeadlineUs = deadline; p->Info.Sequenced = c->Config.Mode != XRUDP_DATAGRAM_INDEPENDENT;
	if (p->Info.Sequenced) p->Info.Sequence = ++c->SendSequence;
	if (size) memcpy(xrudp__dg_data(d, slot), data, size);
	d->State.SendBytes += (uint32_t)size; *sequence = p->Info.Sequence; return XRUDP_OK;
}
xrudpresult xrtRudpDatagramPeekSend(const xrudpdatagrams* d, xrudpdatagramview* output)
{
	if (d == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	for (uint32_t n = 0; n < d->State.Channels; ++n) {
		uint32_t i = (d->Cursor + n) % d->State.Channels, slot = d->Channel[i].Head;
		if (slot != XRUDP__DATAGRAM_NONE) { output->Info = d->Slot[slot].Info; output->Data = xrudp__dg_data(d, slot); return XRUDP_OK; }
	}
	return XRUDP_BACKPRESSURE;
}
xrudpresult xrtRudpDatagramSent(xrudpdatagrams* d, uint64_t ticket, uint32_t wire)
{
	if (d == NULL || ticket == 0 || wire > XRUDP_PACKET_MAX) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	xrudpdatagramview view; if (xrtRudpDatagramPeekSend(d, &view) != XRUDP_OK || view.Info.Ticket != ticket || wire < view.Info.Size + XRUDP_DATAGRAM_OVERHEAD) return XRUDP_ARGUMENT;
	uint32_t index = xrudp__dg_find(d, view.Info.Channel); xrudp__datagramchannel* c = &d->Channel[index]; uint32_t slot = c->Head;
	uint32_t deficit = c->Deficit ? c->Deficit : XRUDP_PACKET_MAX * c->Config.Weight * (4u - c->Config.Priority);
	c->Head = d->Slot[slot].Next; if (c->Head == XRUDP__DATAGRAM_NONE) c->Tail = XRUDP__DATAGRAM_NONE;
	xrudp__dg_clear(d, slot, false); c->Deficit = c->SendQueued && deficit > wire ? deficit - wire : 0;
	d->Cursor = c->Deficit ? index : (index + 1) % d->State.Channels;
	xrudp__dg_count(&d->State.Sent); return XRUDP_OK;
}
xrudpresult xrtRudpDatagramTrimSend(xrudpdatagrams* d, uint32_t maximum, uint32_t* count)
{
	if (count != NULL) *count = 0;
	if (d == NULL || count == NULL || maximum > XRUDP_DATAGRAM_PAYLOAD_MAX) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	for (uint32_t i = 0; i < d->State.Channels; ++i) {
		xrudp__datagramchannel* c = &d->Channel[i]; uint32_t prev = XRUDP__DATAGRAM_NONE, at = c->Head;
		while (at != XRUDP__DATAGRAM_NONE) {
			xrudp__datagramslot* p = &d->Slot[at]; uint32_t next = p->Next;
			if (p->Info.Size > maximum) {
				if (prev == XRUDP__DATAGRAM_NONE) c->Head = next; else d->Slot[prev].Next = next;
				if (c->Tail == at) c->Tail = prev;
				xrudp__dg_clear(d, at, false); xrudp__dg_count(&d->State.SendSizeDropped); ++*count;
			} else prev = at;
			at = next;
		}
	}
	return XRUDP_OK;
}

xrudpresult xrtRudpDatagramInput(xrudpdatagrams* d, uint32_t id, bool sequenced, uint64_t sequence, const void* data, size_t size, uint64_t now)
{
	if (d == NULL || id == 0 || (data == NULL && size != 0) || size > XRUDP_STREAM_CHUNK_MAX ||
		(sequenced ? sequence == 0 : sequence != 0) || now < d->State.LastNowUs || now == UINT64_MAX) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	(void)xrtRudpDatagramMaintain(d, now); xrudp__dg_count(&d->State.Received);
	uint32_t index = xrudp__dg_find(d, id);
	if (index == XRUDP__DATAGRAM_NONE) { xrudp__dg_count(&d->State.DroppedUnknown); return XRUDP_OK; }
	xrudp__datagramchannel* c = &d->Channel[index];
	if ((c->Config.Mode != XRUDP_DATAGRAM_INDEPENDENT) != sequenced) { xrudp__dg_count(&d->State.DroppedMode); return XRUDP_OK; }
	if (sequenced) {
		if (sequence <= c->ReceiveSequence) { xrudp__dg_count(&d->State.DroppedStale); return XRUDP_OK; }
		c->ReceiveSequence = sequence;
	}
	if (size > d->State.Config.PayloadCapacity || d->ReceiveTicket == UINT64_MAX) { xrudp__dg_count(&d->State.DroppedCapacity); return XRUDP_OK; }
	bool replace = c->Config.Mode == XRUDP_DATAGRAM_LATEST && c->Latest != XRUDP__DATAGRAM_NONE;
	uint32_t slot = replace ? c->Latest : xrudp__dg_free(d, true);
	if (slot == XRUDP__DATAGRAM_NONE) { xrudp__dg_count(&d->State.DroppedCapacity); return XRUDP_OK; }
	xrudp__datagramslot* p = &d->Slot[slot];
	if (replace) {
		d->State.ReceiveBytes -= p->Info.Size; xrtSecureZero(xrudp__dg_data(d, slot), p->Info.Size); xrudp__dg_count(&d->State.ReceiveReplaced);
	} else {
		p->Used = true; p->Channel = index; p->Next = XRUDP__DATAGRAM_NONE;
		if (d->ReceiveTail != XRUDP__DATAGRAM_NONE) d->Slot[d->ReceiveTail].Next = slot; else d->ReceiveHead = slot;
		d->ReceiveTail = slot; ++c->ReceiveQueued; ++d->State.ReceiveQueued;
		if (c->Config.Mode == XRUDP_DATAGRAM_LATEST) c->Latest = slot;
	}
	memset(&p->Info, 0, sizeof(p->Info)); p->Info.Channel = id; p->Info.Size = (uint32_t)size;
	p->Info.Sequenced = sequenced; p->Info.Sequence = sequence; p->Info.Ticket = ++d->ReceiveTicket; p->Info.AtUs = now;
	if (c->Config.ReceiveLifetimeUs) p->Info.DeadlineUs = xrudp__dg_add(now, c->Config.ReceiveLifetimeUs);
	if (size) memcpy(xrudp__dg_data(d, slot), data, size);
	d->State.ReceiveBytes += (uint32_t)size; xrudp__dg_count(&d->State.Accepted); return XRUDP_OK;
}
xrudpresult xrtRudpDatagramPeek(const xrudpdatagrams* d, xrudpdatagraminfo* output)
{
	if (output != NULL) memset(output, 0, sizeof(*output));
	if (d == NULL || output == NULL) return XRUDP_ARGUMENT;
	if (d->State.Stopped) return XRUDP_CLOSED;
	if (d->ReceiveHead == XRUDP__DATAGRAM_NONE) return XRUDP_BACKPRESSURE;
	*output = d->Slot[d->ReceiveHead].Info; return XRUDP_OK;
}
xrudpresult xrtRudpDatagramRead(xrudpdatagrams* d, void* output, size_t capacity, uint64_t now, xrudpdatagraminfo* info)
{
	if (info != NULL) memset(info, 0, sizeof(*info));
	if (info == NULL || (output == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	xrudpresult r = xrtRudpDatagramMaintain(d, now); if (r != XRUDP_OK) return r;
	r = xrtRudpDatagramPeek(d, info); if (r != XRUDP_OK) return r;
	if (info->Size > capacity) return XRUDP_TRUNCATED;
	uint32_t slot = d->ReceiveHead; if (info->Size) memcpy(output, xrudp__dg_data(d, slot), info->Size);
	d->ReceiveHead = d->Slot[slot].Next; if (d->ReceiveHead == XRUDP__DATAGRAM_NONE) d->ReceiveTail = XRUDP__DATAGRAM_NONE;
	xrudp__dg_clear(d, slot, true); xrudp__dg_count(&d->State.Read); return XRUDP_OK;
}
