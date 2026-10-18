#include <xrudp/receive.h>
#include <xrt/memory.h>
#include <string.h>

struct xrudpreceive {
	uint64_t ReadOffset, HighestOffset, FinalOffset, Limit, Buffered;
	uint32_t Capacity, ResetReason, StopReason;
	bool HasFinal, Reset, Stopped;
	unsigned char Data[]; /* Capacity plaintext bytes, then Capacity/8 bitmap. */
};

static uint32_t xrudp__receive_capacity(uint32_t capacity)
{
	if (capacity == 0) capacity = XRUDP_RECEIVE_CAPACITY_DEFAULT;
	return capacity >= XRUDP_RECEIVE_CAPACITY_MIN && capacity <= XRUDP_RECEIVE_CAPACITY_MAX &&
		(capacity & (capacity - 1)) == 0 ? capacity : 0;
}

static size_t xrudp__receive_slot(const xrudpreceive* r, uint64_t offset)
{
	return (size_t)(offset & (r->Capacity - 1));
}

static bool xrudp__receive_has(const xrudpreceive* r, size_t slot)
{
	return (r->Data[r->Capacity + slot / 8] & (1u << (slot % 8))) != 0;
}

xrudpresult xrtRudpReceiveAllocationSize(uint32_t capacity, size_t* output)
{
	capacity = xrudp__receive_capacity(capacity);
	if (capacity == 0 || output == NULL) return XRUDP_ARGUMENT;
	*output = sizeof(xrudpreceive) + capacity + capacity / 8;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveCreate(uint32_t capacity, uint64_t limit, xrudpreceive** output)
{
	xrudpreceive* r;
	size_t size;
	capacity = xrudp__receive_capacity(capacity);
	if (capacity == 0 || output == NULL || limit > capacity) return XRUDP_ARGUMENT;
	(void)xrtRudpReceiveAllocationSize(capacity, &size);
	r = (xrudpreceive*)xrtMalloc(size);
	if (r == NULL) return XRUDP_NO_MEMORY;
	memset(r, 0, size); r->Capacity = capacity; r->Limit = limit;
	*output = r;
	return XRUDP_OK;
}

void xrtRudpReceiveDestroy(xrudpreceive* r)
{
	if (r != NULL) {
		size_t size = sizeof(*r) + r->Capacity + r->Capacity / 8;
		xrtSecureZero(r, size); xrtFree(r);
	}
}

xrudpresult xrtRudpReceiveState(const xrudpreceive* r, xrudpreceivestate* output)
{
	xrudpreceivestate s;
	if (r == NULL || output == NULL) return XRUDP_ARGUMENT;
	memset(&s, 0, sizeof(s));
	s.ReadOffset = r->ReadOffset; s.HighestOffset = r->HighestOffset;
	s.FinalOffset = r->FinalOffset; s.Limit = r->Limit; s.BufferedBytes = r->Buffered;
	s.Capacity = r->Capacity; s.HasFinal = r->HasFinal ? 1u : 0u;
	s.Reset = r->Reset ? 1u : 0u; s.ResetReason = r->ResetReason;
	s.Stopped = r->Stopped ? 1u : 0u; s.StopReason = r->StopReason;
	*output = s;
	return XRUDP_OK;
}

bool xrtRudpReceiveReady(const xrudpreceive* r)
{
	return r != NULL && (r->Reset || r->Stopped || (r->HasFinal && r->ReadOffset == r->FinalOffset) ||
		(r->Buffered != 0 && xrudp__receive_has(r, xrudp__receive_slot(r, r->ReadOffset))));
}

xrudpresult xrtRudpReceiveRestart(xrudpreceive* r, uint64_t limit)
{
	if (r == NULL || limit > r->Capacity) return XRUDP_ARGUMENT;
	if (!r->Reset && !(r->HasFinal && r->ReadOffset == r->FinalOffset) &&
		(r->Stopped || r->ReadOffset != 0 || r->HighestOffset != 0 || r->HasFinal || r->Buffered != 0)) return XRUDP_BACKPRESSURE;
	uint32_t capacity = r->Capacity;
	xrtSecureZero(r, sizeof(*r) + capacity + capacity / 8); r->Capacity = capacity; r->Limit = limit;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveCheck(const xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin, xrudpreceivechange* change)
{
	const unsigned char* bytes = (const unsigned char*)data;
	xrudpreceivechange next;
	uint64_t end;
	if (r == NULL || change == NULL || (data == NULL && size != 0) || size > XRUDP_STREAM_CHUNK_MAX)
		return XRUDP_ARGUMENT;
	if ((uint64_t)size > UINT64_MAX - offset) return XRUDP_MALFORMED;
	end = offset + size;
	if (end > r->Limit || (r->HasFinal && (end > r->FinalOffset || (fin && end != r->FinalOffset))) ||
		(fin && end < r->HighestOffset)) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next));
	if (r->Reset) { *change = next; return XRUDP_DUPLICATE; }
	next.HighestDelta = end > r->HighestOffset ? end - r->HighestOffset : 0;
	next.NewFinal = fin && !r->HasFinal ? 1u : 0u;
	if (r->Stopped) {
		next.ConsumedDelta = next.HighestDelta; *change = next;
		return next.HighestDelta || next.NewFinal ? XRUDP_OK : XRUDP_DUPLICATE;
	}
	for (size_t i = 0; i < size; ++i) {
		size_t slot;
		if (offset + i < r->ReadOffset) continue;
		slot = xrudp__receive_slot(r, offset + i);
		if (xrudp__receive_has(r, slot)) {
			if (r->Data[slot] != bytes[i]) return XRUDP_MALFORMED;
		} else ++next.NewBytes;
	}
	*change = next;
	return next.HighestDelta != 0 || next.NewBytes != 0 || next.NewFinal != 0 ? XRUDP_OK : XRUDP_DUPLICATE;
}

static void xrudp__receive_commit(xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin)
{
	if (r->Reset) return;
	if (r->Stopped) {
		if (offset + size > r->HighestOffset) r->ReadOffset = r->HighestOffset = offset + size;
		if (fin) { r->HasFinal = true; r->FinalOffset = offset + size; }
		return;
	}
	for (size_t i = 0; i < size; ++i) {
		size_t slot;
		if (offset + i < r->ReadOffset) continue;
		slot = xrudp__receive_slot(r, offset + i);
		if (!xrudp__receive_has(r, slot)) ++r->Buffered;
		r->Data[slot] = ((const unsigned char*)data)[i];
		r->Data[r->Capacity + slot / 8] |= (unsigned char)(1u << (slot % 8));
	}
	if (offset + size > r->HighestOffset) r->HighestOffset = offset + size;
	if (fin) { r->HasFinal = true; r->FinalOffset = offset + size; }
}

xrudpresult xrtRudpReceivePush(xrudpreceive* r, uint64_t offset,
	const void* data, size_t size, bool fin, xrudpreceivechange* change)
{
	xrudpreceivechange next;
	xrudpresult result;
	if (change == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpReceiveCheck(r, offset, data, size, fin, &next);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	if (result == XRUDP_OK) xrudp__receive_commit(r, offset, data, size, fin);
	*change = next;
	return result;
}

xrudpresult xrtRudpReceiveCheckBatch(const xrudpreceiveframe* frames,
	size_t count, uint64_t credit, xrudpreceivebatchchange* change)
{
	struct plan { const xrudpreceive* Receive; uint64_t Highest, Final; bool HasFinal; } plans[XRUDP_FRAMES_MAX];
	xrudpreceivebatchchange next;
	size_t streams = 0, wireSize = 0;
	if (frames == NULL || change == NULL || count == 0 || count > XRUDP_FRAMES_MAX) return XRUDP_ARGUMENT;
	memset(&next, 0, sizeof(next));
	for (size_t i = 0; i < count; ++i) {
		const xrudpreceiveframe* f = &frames[i];
		xrudpreceivechange single;
		xrudpresult result = xrtRudpReceiveCheck(f->Receive, f->Offset, f->Data, f->Size, f->Fin, &single);
		if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
		if (f->Size + 16 > XRUDP_PACKET_MAX - XRUDP_HEADER_SIZE - XRUDP_TAG_SIZE - wireSize) return XRUDP_LIMIT;
		wireSize += f->Size + 16;
		const xrudpreceive* r = f->Receive;
		size_t p;
		for (p = 0; p < streams && plans[p].Receive != r; ++p) {}
		if (p == streams) {
			plans[p].Receive = r; plans[p].Highest = r->HighestOffset;
			plans[p].Final = r->FinalOffset; plans[p].HasFinal = r->HasFinal; ++streams;
		}
		uint64_t end = f->Offset + f->Size;
		if ((plans[p].HasFinal && (end > plans[p].Final || (f->Fin && end != plans[p].Final))) ||
			(f->Fin && end < plans[p].Highest)) return XRUDP_MALFORMED;
		if (f->Fin && !plans[p].HasFinal) {
			plans[p].HasFinal = true; plans[p].Final = end; ++next.NewFinals;
		}
		if (end > plans[p].Highest) plans[p].Highest = end;
		if (r->Reset || r->Stopped) continue;
		for (size_t j = 0; j < i; ++j) {
			const xrudpreceiveframe* old = &frames[j];
			if (old->Receive != r) continue;
			uint64_t first = f->Offset > old->Offset ? f->Offset : old->Offset;
			uint64_t last = end < old->Offset + old->Size ? end : old->Offset + old->Size;
			if (first < r->ReadOffset) first = r->ReadOffset;
			if (first < last && memcmp((const unsigned char*)f->Data + (size_t)(first - f->Offset),
				(const unsigned char*)old->Data + (size_t)(first - old->Offset), (size_t)(last - first)) != 0)
				return XRUDP_MALFORMED;
		}
		for (size_t k = 0; k < f->Size; ++k) {
			uint64_t position = f->Offset + k;
			bool covered = false;
			if (position < r->ReadOffset || xrudp__receive_has(r, xrudp__receive_slot(r, position))) continue;
			for (size_t j = 0; j < i; ++j)
				if (frames[j].Receive == r && position >= frames[j].Offset && position - frames[j].Offset < frames[j].Size) {
					covered = true; break;
				}
			if (!covered) ++next.NewBytes;
		}
	}
	for (size_t i = 0; i < streams; ++i) {
		uint64_t delta = plans[i].Highest - plans[i].Receive->HighestOffset;
		if (delta > credit - next.HighestDelta) return XRUDP_MALFORMED;
		next.HighestDelta += delta;
		if (plans[i].Receive->Stopped) next.ConsumedDelta += delta;
	}
	next.Streams = (uint32_t)streams; *change = next;
	return next.HighestDelta || next.NewBytes || next.NewFinals ? XRUDP_OK : XRUDP_DUPLICATE;
}

xrudpresult xrtRudpReceivePushBatch(const xrudpreceiveframe* frames,
	size_t count, uint64_t credit, xrudpreceivebatchchange* change)
{
	xrudpreceivebatchchange next;
	xrudpresult result;
	if (change == NULL) return XRUDP_ARGUMENT;
	result = xrtRudpReceiveCheckBatch(frames, count, credit, &next);
	if (result != XRUDP_OK && result != XRUDP_DUPLICATE) return result;
	if (result == XRUDP_OK)
		for (size_t i = 0; i < count; ++i)
			xrudp__receive_commit(frames[i].Receive, frames[i].Offset, frames[i].Data, frames[i].Size, frames[i].Fin);
	*change = next;
	return result;
}

xrudpresult xrtRudpReceivePeek(const xrudpreceive* r, uint64_t offset, void* output, size_t capacity, size_t* read)
{
	if (read != NULL) *read = 0;
	if (r == NULL || output == NULL || capacity == 0 || read == NULL || offset < r->ReadOffset || offset > r->Limit ||
		(r->HasFinal && offset > r->FinalOffset)) return XRUDP_ARGUMENT;
	if (r->Reset || r->Stopped) return XRUDP_CANCELLED;
	if (r->HasFinal && offset == r->FinalOffset) return XRUDP_DONE;
	size_t count = 0;
	while (count < capacity && offset < r->Limit) {
		size_t slot = xrudp__receive_slot(r, offset);
		if (!xrudp__receive_has(r, slot)) break;
		((unsigned char*)output)[count++] = r->Data[slot]; ++offset;
	}
	*read = count; return count != 0 ? XRUDP_OK : XRUDP_BACKPRESSURE;
}

xrudpresult xrtRudpReceiveRead(xrudpreceive* r, void* output, size_t capacity, size_t* read)
{
	unsigned char* bytes = (unsigned char*)output;
	size_t count = 0;
	if (read != NULL) *read = 0;
	if (r == NULL || output == NULL || capacity == 0 || read == NULL) return XRUDP_ARGUMENT;
	if (r->Reset || r->Stopped) return XRUDP_CANCELLED;
	if (r->HasFinal && r->ReadOffset == r->FinalOffset) return XRUDP_DONE;
	while (count < capacity && r->Buffered != 0) {
		size_t slot = xrudp__receive_slot(r, r->ReadOffset);
		if (!xrudp__receive_has(r, slot)) break;
		bytes[count++] = r->Data[slot]; r->Data[slot] = 0;
		r->Data[r->Capacity + slot / 8] &= (unsigned char)~(1u << (slot % 8));
		++r->ReadOffset; --r->Buffered;
	}
	*read = count;
	return count != 0 ? XRUDP_OK : XRUDP_BACKPRESSURE;
}

xrudpresult xrtRudpReceiveExtend(xrudpreceive* r, uint64_t limit)
{
	uint64_t ceiling;
	if (r == NULL) return XRUDP_ARGUMENT;
	if (r->Reset) return XRUDP_CLOSED;
	if (r->Stopped) return XRUDP_CANCELLED;
	ceiling = r->ReadOffset > UINT64_MAX - r->Capacity ? UINT64_MAX : r->ReadOffset + r->Capacity;
	if (limit < r->Limit || limit > ceiling) return XRUDP_ARGUMENT;
	if (limit == r->Limit) return XRUDP_DUPLICATE;
	r->Limit = limit;
	return XRUDP_OK;
}

xrudpresult xrtRudpReceiveStop(xrudpreceive* r, uint32_t reason, xrudpreceivechange* change)
{
	if (r == NULL || change == NULL) return XRUDP_ARGUMENT;
	if (r->Stopped && reason != r->StopReason) return XRUDP_ARGUMENT;
	xrudpreceivechange next; memset(&next, 0, sizeof(next));
	if (r->Stopped || r->Reset || (r->HasFinal && r->ReadOffset == r->FinalOffset)) {
		*change = next; return XRUDP_DUPLICATE;
	}
	next.ConsumedDelta = r->HighestOffset - r->ReadOffset;
	r->ReadOffset = r->HighestOffset; r->Buffered = 0; r->Stopped = true; r->StopReason = reason;
	xrtSecureZero(r->Data, r->Capacity + r->Capacity / 8);
	*change = next; return XRUDP_OK;
}

xrudpresult xrtRudpReceiveReset(xrudpreceive* r,
	uint64_t final, uint32_t reason, xrudpreceivechange* change)
{
	xrudpreceivechange next;
	if (r == NULL || change == NULL) return XRUDP_ARGUMENT;
	if (final > r->Limit || final < r->HighestOffset || (r->HasFinal && final != r->FinalOffset) ||
		(r->Reset && reason != r->ResetReason)) return XRUDP_MALFORMED;
	memset(&next, 0, sizeof(next));
	if (r->Reset) { *change = next; return XRUDP_DUPLICATE; }
	/* A clean terminal result already delivered cannot become a second reset. */
	if (r->HasFinal && r->ReadOffset == r->FinalOffset) { *change = next; return XRUDP_DUPLICATE; }
	next.HighestDelta = final - r->HighestOffset;
	next.ConsumedDelta = final - r->ReadOffset; next.NewFinal = r->HasFinal ? 0u : 1u;
	r->HighestOffset = r->FinalOffset = r->ReadOffset = final;
	r->Reset = r->HasFinal = true; r->ResetReason = reason; r->Buffered = 0;
	xrtSecureZero(r->Data, r->Capacity + r->Capacity / 8);
	*change = next;
	return XRUDP_OK;
}
