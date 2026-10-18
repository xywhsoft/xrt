#include <xrudp/transmit.h>
#include <xrt/memory.h>
#include <string.h>

#define XRUDP__TRANSMIT_LEVELS 8

struct xrudptransmit {
	uint64_t Base, End, Highest, Limit, Pending;
	size_t Allocation;
	uint32_t Capacity, Levels;
	bool FinQueued, FinSent, FinAcked, FinReady, Cancelled;
	unsigned char* Acked;
	unsigned char* Sent;
	unsigned char* Ready[XRUDP__TRANSMIT_LEVELS];
	size_t LevelBytes[XRUDP__TRANSMIT_LEVELS];
	unsigned char Data[];
};

static uint32_t xrudp__transmit_capacity(uint32_t capacity)
{
	if (capacity == 0) capacity = XRUDP_TRANSMIT_CAPACITY_DEFAULT;
	return capacity >= XRUDP_TRANSMIT_CAPACITY_MIN && capacity <= XRUDP_TRANSMIT_CAPACITY_MAX &&
		(capacity & (capacity - 1)) == 0 ? capacity : 0;
}

static size_t xrudp__transmit_slot(const xrudptransmit* t, uint64_t offset)
{
	return (size_t)(offset & (t->Capacity - 1));
}

static bool xrudp__transmit_bit(const unsigned char* bits, size_t slot)
{
	return (bits[slot / 8] & (1u << (slot % 8))) != 0;
}

static void xrudp__transmit_set(unsigned char* bits, size_t slot, bool set)
{
	unsigned char mask = (unsigned char)(1u << (slot % 8));
	if (set) bits[slot / 8] |= mask;
	else bits[slot / 8] &= (unsigned char)~mask;
}

static void xrudp__transmit_ready(xrudptransmit* t, size_t slot, bool ready)
{
	for (size_t level = 0; level < t->Levels; ++level) {
		size_t byte = slot / 8;
		bool before = t->Ready[level][byte] != 0;
		xrudp__transmit_set(t->Ready[level], slot, ready);
		ready = t->Ready[level][byte] != 0;
		if (ready == before) break;
		slot = byte;
	}
}

/* Hierarchical readiness avoids scanning a MiB of unrelated in-flight bytes
 * for every one-byte retransmission. At most eight levels with eight bits. */
static bool xrudp__transmit_find(const xrudptransmit* t, size_t level,
	size_t start, size_t* output)
{
	size_t byte = start / 8, bit = 0;
	unsigned int available;
	if (byte >= t->LevelBytes[level]) return false;
	available = t->Ready[level][byte] & (0xffu << (start % 8));
	if (available == 0) {
		if (level + 1 >= t->Levels || !xrudp__transmit_find(t, level + 1, byte + 1, &byte)) return false;
		available = t->Ready[level][byte];
	}
	while ((available & (1u << bit)) == 0) ++bit;
	*output = byte * 8 + bit;
	return true;
}

xrudpresult xrtRudpTransmitAllocationSize(uint32_t capacity, size_t* output)
{
	size_t size, level;
	capacity = xrudp__transmit_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	size = sizeof(xrudptransmit) + capacity + capacity / 4;
	level = capacity / 8;
	for (;;) { size += level; if (level == 1) break; level = (level + 7) / 8; }
	*output = size;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitCreate(uint32_t capacity, uint64_t limit, xrudptransmit** output)
{
	xrudptransmit* t;
	size_t allocation, level;
	unsigned char* next;
	capacity = xrudp__transmit_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	(void)xrtRudpTransmitAllocationSize(capacity, &allocation);
	t = (xrudptransmit*)xrtMalloc(allocation);
	if (t == NULL) return XRUDP_NO_MEMORY;
	memset(t, 0, allocation);
	t->Allocation = allocation; t->Capacity = capacity; t->Limit = limit;
	t->Acked = t->Data + capacity; t->Sent = t->Acked + capacity / 8;
	next = t->Sent + capacity / 8; level = capacity / 8;
	for (;;) {
		t->Ready[t->Levels] = next; t->LevelBytes[t->Levels++] = level; next += level;
		if (level == 1) break;
		level = (level + 7) / 8;
	}
	*output = t;
	return XRUDP_OK;
}

void xrtRudpTransmitDestroy(xrudptransmit* t)
{
	if (t != NULL) { xrtSecureZero(t, t->Allocation); xrtFree(t); }
}

xrudpresult xrtRudpTransmitState(const xrudptransmit* t, xrudptransmitstate* output)
{
	xrudptransmitstate s;
	if (t == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&s, 0, sizeof(s));
	s.BaseOffset = t->Base; s.QueuedEnd = t->End; s.HighestSent = t->Highest;
	s.PeerLimit = t->Limit; s.PendingBytes = t->Pending; s.Capacity = t->Capacity;
	s.FinQueued = t->FinQueued ? 1u : 0u; s.FinSent = t->FinSent ? 1u : 0u;
	s.FinAcknowledged = t->FinAcked ? 1u : 0u; s.Cancelled = t->Cancelled ? 1u : 0u;
	*output = s;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitWrite(xrudptransmit* t, const void* data, size_t size, size_t* written)
{
	size_t count;
	if (written != NULL) *written = 0;
	if (t == NULL || written == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	if (t->Cancelled || t->FinQueued) return XRUDP_CLOSED;
	count = t->Capacity - (size_t)(t->End - t->Base);
	if (count > size) count = size;
	if (size != 0 && count == 0) return XRUDP_BACKPRESSURE;
	if (count > UINT64_MAX - t->End) return XRUDP_LIMIT;
	for (size_t i = 0; i < count; ++i) {
		size_t slot = xrudp__transmit_slot(t, t->End + i);
		t->Data[slot] = ((const unsigned char*)data)[i];
		xrudp__transmit_ready(t, slot, true);
	}
	t->End += count; t->Pending += count; *written = count;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitMessageWrite(xrudptransmit* t, const void* data, size_t size)
{
	if (t == NULL || (data == NULL && size != 0)) return XRUDP_ARGUMENT;
	if (t->Cancelled || t->FinQueued) return XRUDP_CLOSED;
	if (size > t->Capacity - 4u) return XRUDP_LIMIT;
	size_t total = size + 4;
	if (total > UINT64_MAX - t->End) return XRUDP_LIMIT;
	if (total > t->Capacity - (size_t)(t->End - t->Base)) return XRUDP_BACKPRESSURE;
	unsigned char header[4];
	for (size_t i = 0; i < 4; ++i) header[3 - i] = (unsigned char)((uint32_t)size >> (i * 8));
	/* All bounds and terminal state checked before either copy. Serialized
	 * ownership makes both ordinary writes infallible and unobservable apart. */
	size_t written;
	(void)xrtRudpTransmitWrite(t, header, sizeof(header), &written);
	(void)xrtRudpTransmitWrite(t, data, size, &written);
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitRestart(xrudptransmit* t, uint64_t limit)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (!t->Cancelled && !(t->FinQueued && t->FinAcked && t->Base == t->End) &&
		(t->Base != 0 || t->End != 0 || t->Highest != 0 || t->FinQueued)) return XRUDP_BACKPRESSURE;
	xrtSecureZero(t->Data, t->Allocation - sizeof(*t));
	t->Base = t->End = t->Highest = t->Pending = 0; t->Limit = limit;
	t->FinQueued = t->FinSent = t->FinAcked = t->FinReady = t->Cancelled = false;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitFinish(xrudptransmit* t)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	if (t->FinQueued) return XRUDP_DUPLICATE;
	t->FinQueued = t->FinReady = true;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitSetLimit(xrudptransmit* t, uint64_t limit)
{
	if (t == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	if (limit <= t->Limit) return XRUDP_DUPLICATE;
	t->Limit = limit;
	return XRUDP_OK;
}

xrudpresult xrtRudpTransmitPeek(const xrudptransmit* t, bool probe,
	void* output, size_t capacity, xrudptransmitframe* frame)
{
	xrudptransmitframe next;
	size_t slot = 0;
	bool found = false;
	uint64_t end;
	if (t == NULL || output == NULL || capacity == 0 || frame == NULL) return XRUDP_ARGUMENT;
	if (t->Cancelled) return XRUDP_CLOSED;
	memset(&next, 0, sizeof(next));
	if (probe && t->Base < t->End) { next.Offset = t->Base; found = true; }
	else if (t->Base < t->End) {
		size_t baseSlot = xrudp__transmit_slot(t, t->Base);
		found = xrudp__transmit_find(t, 0, baseSlot, &slot) || xrudp__transmit_find(t, 0, 0, &slot);
		if (found) next.Offset = t->Base + ((slot - baseSlot) & (t->Capacity - 1));
	}
	if (capacity > XRUDP_STREAM_CHUNK_MAX) capacity = XRUDP_STREAM_CHUNK_MAX;
	if (found && next.Offset < t->Limit) {
		while (next.Size < capacity && next.Offset + next.Size < t->End && next.Offset + next.Size < t->Limit) {
			slot = xrudp__transmit_slot(t, next.Offset + next.Size);
			if (xrudp__transmit_bit(t->Acked, slot) || (!probe && !xrudp__transmit_bit(t->Ready[0], slot))) break;
			++next.Size;
		}
	}
	end = next.Offset + next.Size;
	if (t->FinQueued && !t->FinAcked && (probe || t->FinReady) && t->End <= t->Limit) {
		if (next.Size == 0) { next.Offset = end = t->End; }
		if (end == t->End) next.Fin = 1;
	}
	if (next.Size == 0 && !next.Fin) return XRUDP_BACKPRESSURE;
	next.HighestDelta = end > t->Highest ? end - t->Highest : 0;
	for (size_t i = 0; i < next.Size; ++i) ((unsigned char*)output)[i] = t->Data[xrudp__transmit_slot(t, next.Offset + i)];
	*frame = next;
	return XRUDP_OK;
}

static xrudpresult xrudp__transmit_range(const xrudptransmit* t,
	uint64_t offset, size_t size, bool fin)
{
	if ((!fin && size == 0) || size > XRUDP_STREAM_CHUNK_MAX || size > UINT64_MAX - offset || offset + size > t->End ||
		(fin && (!t->FinQueued || offset + size != t->End))) return XRUDP_ARGUMENT;
	return t->Cancelled ? XRUDP_CLOSED : XRUDP_OK;
}

xrudpresult xrtRudpTransmitMarkSent(xrudptransmit* t, uint64_t offset,
	size_t size, bool fin, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	xrudpresult result;
	bool pending = false;
	uint64_t end;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	result = xrudp__transmit_range(t, offset, size, fin);
	if (result != XRUDP_OK) return result;
	end = offset + size;
	if (end > t->Limit) return XRUDP_BACKPRESSURE;
	memset(&next, 0, sizeof(next));
	next.HighestDelta = end > t->Highest ? end - t->Highest : 0;
	for (uint64_t i = offset > t->Base ? offset : t->Base; i < end; ++i) {
		size_t slot = xrudp__transmit_slot(t, i);
		if (xrudp__transmit_bit(t->Acked, slot)) continue;
		xrudp__transmit_set(t->Sent, slot, true); xrudp__transmit_ready(t, slot, false); pending = true;
	}
	if (fin && !t->FinAcked) { t->FinSent = true; t->FinReady = false; pending = true; }
	t->Highest += next.HighestDelta; *change = next;
	return pending || next.HighestDelta != 0 ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpTransmitResolve(xrudptransmit* t, uint64_t offset,
	size_t size, bool fin, bool ack, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	xrudpresult result;
	bool progress = false;
	uint64_t end, start;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	result = xrudp__transmit_range(t, offset, size, fin);
	if (result != XRUDP_OK) return result;
	end = offset + size; start = offset > t->Base ? offset : t->Base;
	if (end > t->Highest || (fin && !t->FinSent)) return XRUDP_ARGUMENT;
	for (uint64_t i = start; i < end; ++i)
		if (!xrudp__transmit_bit(t->Sent, xrudp__transmit_slot(t, i))) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	for (uint64_t i = start; i < end; ++i) {
		size_t slot = xrudp__transmit_slot(t, i);
		if (xrudp__transmit_bit(t->Acked, slot)) continue;
		if (ack) {
			xrudp__transmit_set(t->Acked, slot, true); xrudp__transmit_ready(t, slot, false);
			++next.NewlyAckedBytes; --t->Pending; progress = true;
		} else {
			if (!xrudp__transmit_bit(t->Ready[0], slot)) progress = true;
			xrudp__transmit_ready(t, slot, true);
		}
	}
	if (fin && !t->FinAcked) {
		if (ack) { t->FinAcked = true; t->FinReady = false; next.NewFinAcknowledged = 1; progress = true; }
		else { if (!t->FinReady) progress = true; t->FinReady = true; }
	}
	while (t->Base < t->End) {
		size_t slot = xrudp__transmit_slot(t, t->Base);
		if (!xrudp__transmit_bit(t->Acked, slot)) break;
		t->Data[slot] = 0; xrudp__transmit_set(t->Acked, slot, false); xrudp__transmit_set(t->Sent, slot, false);
		++t->Base; ++next.ReleasedBytes;
	}
	*change = next;
	return progress ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpTransmitCancel(xrudptransmit* t, xrudptransmitchange* change)
{
	xrudptransmitchange next;
	if (t == NULL || change == NULL) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	if (t->Cancelled) { *change = next; return XRUDP_DUPLICATE; }
	next.ReleasedBytes = t->End - t->Base;
	t->Base = t->End; t->Pending = 0; t->Cancelled = true; t->FinReady = false;
	xrtSecureZero(t->Data, t->Allocation - sizeof(*t));
	*change = next;
	return XRUDP_OK;
}
