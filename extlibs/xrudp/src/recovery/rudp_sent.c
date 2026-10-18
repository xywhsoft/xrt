#include <xrudp/sent.h>
#include <xrt/memory.h>
#include <string.h>

typedef struct xrudp__sentpacket {
	uint64_t Number, AtUs, Tag;
	uint32_t Bytes, Flags;
	uint32_t Status;
} xrudp__sentpacket;

struct xrudpsentwindow {
	uint64_t LargestSent, LargestAcked, InFlight, LastNow;
	uint32_t NormalCapacity, Capacity, Head, Count, Pending;
	bool Stopped;
	xrudp__sentpacket Packets[];
};

static xrudp__sentpacket* xrudp__sent_at(const xrudpsentwindow* w, size_t index)
{
	return (xrudp__sentpacket*)&w->Packets[(w->Head + index) % w->Capacity];
}

static size_t xrudp__sent_lower(const xrudpsentwindow* w, uint64_t number)
{
	size_t first = 0, last = w->Count;
	while (first < last) {
		size_t middle = first + (last - first) / 2;
		if (xrudp__sent_at(w, middle)->Number < number) first = middle + 1;
		else last = middle;
	}
	return first;
}

static void xrudp__sent_trim(xrudpsentwindow* w)
{
	while (w->Count != 0 && xrudp__sent_at(w, 0)->Status != 0) {
		memset(xrudp__sent_at(w, 0), 0, sizeof(xrudp__sentpacket));
		w->Head = (w->Head + 1) % w->Capacity;
		--w->Count;
	}
}

static void xrudp__sent_finish(xrudpsentwindow* w, xrudp__sentpacket* p,
	uint32_t kind, xrudpsentevent* event)
{
	memset(event, 0, sizeof(*event));
	event->PacketNumber = p->Number; event->UserTag = p->Tag;
	event->SentAtUs = p->AtUs; event->Bytes = p->Bytes;
	event->Flags = p->Flags; event->Kind = kind;
	if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) w->InFlight -= p->Bytes;
	p->Status = kind;
	p->Tag = 0;
	--w->Pending;
}

xrudpresult xrtRudpSentCreate(uint32_t capacity, xrudpsentwindow** output)
{
	return xrtRudpSentCreateAfter(capacity, 0, output);
}

xrudpresult xrtRudpSentAllocationSize(uint32_t capacity, size_t* output)
{
	if (output == NULL || capacity > XRUDP_SENT_CAPACITY_MAX) return XRUDP_ARGUMENT;
	if (capacity == 0) capacity = XRUDP_SENT_CAPACITY_DEFAULT;
	*output = sizeof(xrudpsentwindow) + (capacity + XRUDP_SENT_PROBE_RESERVE) * sizeof(xrudp__sentpacket);
	return XRUDP_OK;
}

xrudpresult xrtRudpSentCreateAfter(uint32_t capacity, uint64_t retired, xrudpsentwindow** output)
{
	xrudpsentwindow* w;
	size_t size;
	if (output == NULL || capacity > XRUDP_SENT_CAPACITY_MAX || retired == UINT64_MAX) return XRUDP_ARGUMENT;
	if (capacity == 0) capacity = XRUDP_SENT_CAPACITY_DEFAULT;
	(void)xrtRudpSentAllocationSize(capacity, &size);
	w = (xrudpsentwindow*)xrtMalloc(size);
	if (w == NULL) return XRUDP_NO_MEMORY;
	memset(w, 0, size);
	w->NormalCapacity = capacity; w->Capacity = capacity + XRUDP_SENT_PROBE_RESERVE;
	w->LargestSent = retired;
	*output = w;
	return XRUDP_OK;
}

void xrtRudpSentDestroy(xrudpsentwindow* w)
{
	if (w != NULL) {
		size_t size = sizeof(*w) + w->Capacity * sizeof(xrudp__sentpacket);
		xrtSecureZero(w, size);
		xrtFree(w);
	}
}

xrudpresult xrtRudpSentState(const xrudpsentwindow* w, xrudpsentstate* output)
{
	xrudpsentstate state;
	if (w == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&state, 0, sizeof(state));
	state.LargestSent = w->LargestSent; state.LargestAcked = w->LargestAcked;
	state.BytesInFlight = w->InFlight; state.LastNowUs = w->LastNow;
	state.NormalCapacity = w->NormalCapacity; state.TotalCapacity = w->Capacity;
	state.StoredPackets = w->Count; state.PendingPackets = w->Pending;
	state.Stopped = w->Stopped ? 1u : 0u;
	*output = state;
	return XRUDP_OK;
}

bool xrtRudpSentCanRecord(const xrudpsentwindow* w, bool probe)
{
	return w != NULL && !w->Stopped && w->Count < (probe ? w->Capacity : w->NormalCapacity);
}
bool xrtRudpSentCanRecordAck(const xrudpsentwindow* w)
{
	/* Keep two slots exclusively available to an actual PTO burst. ACK-only
	 * metadata never spends the last recovery opportunity. */
	return w != NULL && !w->Stopped && w->Count < w->Capacity - 2;
}

xrudpresult xrtRudpSentRecord(xrudpsentwindow* w, uint64_t number,
	uint64_t now, uint32_t bytes, uint32_t flags, uint64_t tag)
{
	xrudp__sentpacket* p;
	bool probe = (flags & XRUDP_SENT_PROBE) != 0;
	if (w == NULL || number == 0 || number == UINT64_MAX || number <= w->LargestSent ||
		now < w->LastNow || bytes < XRUDP_HEADER_SIZE + XRUDP_TAG_SIZE + XRUDP_FRAME_HEADER_SIZE ||
		bytes > XRUDP_PACKET_MAX || (flags & ~(XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PROBE | XRUDP_SENT_PADDING | XRUDP_SENT_MTU | XRUDP_SENT_ACK_ONLY)) != 0 ||
		((flags & XRUDP_SENT_ACK_ONLY) && flags != XRUDP_SENT_ACK_ONLY) ||
		((flags & XRUDP_SENT_MTU) && ((flags & 7u) != (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING) || bytes < 512)) ||
		(probe && (flags & XRUDP_SENT_ACK_ELICITING) == 0)) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	if (!(flags == XRUDP_SENT_ACK_ONLY ? xrtRudpSentCanRecordAck(w) : xrtRudpSentCanRecord(w, probe))) return XRUDP_BACKPRESSURE;
	p = xrudp__sent_at(w, w->Count++);
	memset(p, 0, sizeof(*p));
	p->Number = number; p->AtUs = now; p->Tag = tag; p->Bytes = bytes; p->Flags = flags;
	w->LargestSent = number; w->LastNow = now; ++w->Pending;
	if ((flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) w->InFlight += bytes;
	return XRUDP_OK;
}

xrudpresult xrtRudpSentDiscardPacket(xrudpsentwindow* w, uint64_t number,
	uint64_t now, xrudpsentevent* event)
{
	if (w == NULL || event == NULL || number == 0 || number > w->LargestSent || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	if (w->Count == 0 || number < xrudp__sent_at(w, 0)->Number) return XRUDP_DONE;
	size_t at = xrudp__sent_lower(w, number);
	if (at == w->Count || xrudp__sent_at(w, at)->Number != number) return XRUDP_ARGUMENT;
	xrudp__sentpacket* p = xrudp__sent_at(w, at);
	if (p->Status != 0) return XRUDP_DONE;
	uint32_t kind = (p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) ? XRUDP_SENT_LOST : XRUDP_SENT_EXPIRED;
	xrudp__sent_finish(w, p, kind, event); w->LastNow = now; xrudp__sent_trim(w);
	return XRUDP_OK;
}

static xrudpresult xrudp__sent_ack_check(const xrudpsentwindow* w, const xrudpack* ack)
{
	uint64_t previous = 0;
	if (ack->Count == 0 || ack->Count > XRUDP_ACK_RANGES_MAX) return XRUDP_MALFORMED;
	for (size_t i = 0; i < ack->Count; ++i) {
		uint64_t first = ack->Ranges[i].First, last = ack->Ranges[i].Last;
		size_t lower, upper;
		if (first == 0 || first > last || last > w->LargestSent ||
			(i != 0 && (last >= previous || previous - last <= 1))) return XRUDP_MALFORMED;
		previous = first;
		if (w->Count == 0 || last < xrudp__sent_at(w, 0)->Number) continue;
		if (first < xrudp__sent_at(w, 0)->Number) first = xrudp__sent_at(w, 0)->Number;
		lower = xrudp__sent_lower(w, first);
		upper = xrudp__sent_lower(w, last + 1);
		if ((uint64_t)(upper - lower) != last - first + 1) return XRUDP_MALFORMED;
	}
	return XRUDP_OK;
}

xrudpresult xrtRudpSentCheckAcknowledge(const xrudpsentwindow* w, const xrudpack* ack,
	uint64_t now, size_t* count, xrudpackchange* change)
{
	xrudpackchange next;
	xrudpresult result;
	size_t needed = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || ack == NULL || count == NULL || change == NULL || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	result = xrudp__sent_ack_check(w, ack);
	if (result != XRUDP_OK) return result;
	memset(&next, 0, sizeof(next));
	next.LargestAcked = ack->Ranges[0].Last > w->LargestAcked ? ack->Ranges[0].Last : w->LargestAcked;
	next.PeerAckDelayUs = ack->DelayUs;
	for (size_t i = 0; i < ack->Count; ++i) {
		size_t lower = xrudp__sent_lower(w, ack->Ranges[i].First);
		size_t upper = xrudp__sent_lower(w, ack->Ranges[i].Last + 1);
		for (size_t j = lower; j < upper; ++j) {
			const xrudp__sentpacket* p = xrudp__sent_at(w, j);
			if (p->Status != 0) continue;
			++needed; ++next.NewlyAckedPackets;
			if ((p->Flags & XRUDP_SENT_ACK_ELICITING) != 0) ++next.NewlyAckedEliciting;
			if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) != 0) {
				next.NewlyAckedBytes += p->Bytes;
			}
		}
	}
	if (next.NewlyAckedEliciting != 0) {
		size_t index = xrudp__sent_lower(w, ack->Ranges[0].Last);
		if (index < w->Count) {
			const xrudp__sentpacket* p = xrudp__sent_at(w, index);
			if (p->Number == ack->Ranges[0].Last && p->Status == 0) {
				next.RttSampleUs = now - p->AtUs;
				next.RttSampleValid = 1;
			}
		}
	}
	*change = next; *count = needed;
	return XRUDP_OK;
}

xrudpresult xrtRudpSentAcknowledge(xrudpsentwindow* w, const xrudpack* ack,
	uint64_t now, xrudpsentevent* events, size_t capacity, size_t* count,
	xrudpackchange* change)
{
	xrudpackchange next;
	size_t needed, written = 0;
	xrudpresult result;
	if (count != NULL) *count = 0;
	if (count == NULL || change == NULL || (events == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	result = xrtRudpSentCheckAcknowledge(w, ack, now, &needed, &next);
	if (result != XRUDP_OK) return result;
	if (needed > capacity) return XRUDP_BACKPRESSURE;
	/* Validation, event capacity and timing checks are complete. */
	for (size_t i = 0; i < ack->Count; ++i) {
		size_t lower = xrudp__sent_lower(w, ack->Ranges[i].First);
		size_t upper = xrudp__sent_lower(w, ack->Ranges[i].Last + 1);
		for (size_t j = lower; j < upper; ++j) {
			xrudp__sentpacket* p = xrudp__sent_at(w, j);
			if (p->Status == 0) xrudp__sent_finish(w, p, XRUDP_SENT_ACKED, &events[written++]);
		}
	}
	w->LargestAcked = next.LargestAcked; w->LastNow = now;
	xrudp__sent_trim(w);
	*change = next; *count = written;
	return XRUDP_OK;
}

static uint32_t xrudp__sent_loss_kind(const xrudpsentwindow* w,
	const xrudp__sentpacket* p, uint64_t lossDelay, uint64_t ackOnlyAge, uint64_t now)
{
	if (p->Status != 0) return 0;
	if ((p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) == 0)
		return now - p->AtUs >= ackOnlyAge ? XRUDP_SENT_EXPIRED : 0;
	if (p->Number >= w->LargestAcked) return 0;
	return w->LargestAcked - p->Number >= 3 || now - p->AtUs >= lossDelay ? XRUDP_SENT_LOST : 0;
}

xrudpresult xrtRudpSentDetectLoss(xrudpsentwindow* w,
	uint64_t lossDelay, uint64_t ackOnlyAge, uint64_t now,
	xrudpsentevent* events, size_t capacity, size_t* count)
{
	size_t needed = 0, written = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || count == NULL || (events == NULL && capacity != 0) ||
		lossDelay < 1000 || ackOnlyAge < 1000 || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	for (size_t i = 0; i < w->Count; ++i)
		if (xrudp__sent_loss_kind(w, xrudp__sent_at(w, i), lossDelay, ackOnlyAge, now) != 0) ++needed;
	if (needed > capacity) return XRUDP_BACKPRESSURE;
	if (needed == 0) return XRUDP_DONE;
	for (size_t i = 0; i < w->Count; ++i) {
		xrudp__sentpacket* p = xrudp__sent_at(w, i);
		uint32_t kind = xrudp__sent_loss_kind(w, p, lossDelay, ackOnlyAge, now);
		if (kind != 0) xrudp__sent_finish(w, p, kind, &events[written++]);
	}
	w->LastNow = now;
	xrudp__sent_trim(w);
	*count = written;
	return XRUDP_OK;
}

xrudpresult xrtRudpSentDiscardPath(xrudpsentwindow* w, uint64_t now,
	xrudpsentevent* events, size_t capacity, size_t* count)
{
	if (count != NULL) *count = 0;
	if (w == NULL || count == NULL || (events == NULL && capacity != 0) || now < w->LastNow) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_CLOSED;
	if (w->Pending > capacity) return XRUDP_BACKPRESSURE;
	if (w->Pending == 0) return XRUDP_DONE;
	size_t written = 0;
	for (size_t i = 0; i < w->Count; ++i) {
		xrudp__sentpacket* p = xrudp__sent_at(w, i);
		if (p->Status == 0) xrudp__sent_finish(w, p,
			(p->Flags & (XRUDP_SENT_ACK_ELICITING | XRUDP_SENT_PADDING)) ? XRUDP_SENT_LOST : XRUDP_SENT_EXPIRED, &events[written++]);
	}
	w->LastNow = now; xrudp__sent_trim(w); *count = written; return XRUDP_OK;
}
xrudpresult xrtRudpSentAbort(xrudpsentwindow* w,
	xrudpsentevent* events, size_t capacity, size_t* count)
{
	size_t written = 0;
	if (count != NULL) *count = 0;
	if (w == NULL || count == NULL || (events == NULL && capacity != 0)) return XRUDP_ARGUMENT;
	if (w->Stopped) return XRUDP_DONE;
	if (w->Pending > capacity) return XRUDP_BACKPRESSURE;
	for (size_t i = 0; i < w->Count; ++i) {
		xrudp__sentpacket* p = xrudp__sent_at(w, i);
		if (p->Status == 0) xrudp__sent_finish(w, p, XRUDP_SENT_CANCELLED, &events[written++]);
	}
	w->Stopped = true;
	xrudp__sent_trim(w);
	*count = written;
	return XRUDP_OK;
}
