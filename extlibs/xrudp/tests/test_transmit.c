#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char source[XRUDP_TRANSMIT_CAPACITY_MAX];
static uint32_t seed = 0x769e14a3;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}

static int boundaries(void)
{
	xrudptransmit* t = NULL;
	xrudptransmitstate state, before;
	xrudptransmitframe frame, savedFrame;
	xrudptransmitchange change, saved;
	unsigned char output[512], untouched[512];
	size_t written = 9, allocation = 987;
	CHECK(xrtRudpTransmitAllocationSize(257, &allocation) == XRUDP_ARGUMENT && allocation == 987);
	CHECK(xrtRudpTransmitAllocationSize(0, &allocation) == XRUDP_OK && allocation > 65536);
	CHECK(xrtRudpTransmitCreate(255, 0, &t) == XRUDP_ARGUMENT && t == NULL);
	CHECK(xrtRudpTransmitCreate(256, 0, &t) == XRUDP_OK);
	CHECK(xrtRudpTransmitWrite(t, source, 512, &written) == XRUDP_OK && written == 256);
	CHECK(xrtRudpTransmitWrite(t, source, 1, &written) == XRUDP_BACKPRESSURE && written == 0);
	CHECK(xrtRudpTransmitWrite(t, NULL, 0, &written) == XRUDP_OK && written == 0);
	memset(output, 0xa5, sizeof(output)); memcpy(untouched, output, sizeof(output));
	memset(&frame, 0xa5, sizeof(frame)); savedFrame = frame;
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_BACKPRESSURE);
	CHECK(memcmp(output, untouched, sizeof(output)) == 0 && memcmp(&frame, &savedFrame, sizeof(frame)) == 0);
	CHECK(xrtRudpTransmitState(t, &before) == XRUDP_OK);
	memset(&change, 0xa5, sizeof(change)); saved = change;
	CHECK(xrtRudpTransmitResolve(t, 0, 256, false, true, &change) == XRUDP_ARGUMENT);
	CHECK(memcmp(&change, &saved, sizeof(change)) == 0);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 256, false, &change) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpTransmitState(t, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpTransmitSetLimit(t, 256) == XRUDP_OK);
	CHECK(xrtRudpTransmitSetLimit(t, 0) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitMarkSent(t, 128, 128, false, &change) == XRUDP_OK && change.HighestDelta == 256);
	/* A high sent offset is not proof that the lower hole was sent. */
	CHECK(xrtRudpTransmitResolve(t, 0, 128, false, true, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
	CHECK(frame.Offset == 0 && frame.Size == 128 && frame.HighestDelta == 0 && !frame.Fin);
	CHECK(memcmp(output, source, 128) == 0);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 128, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpTransmitResolve(t, 128, 128, false, true, &change) == XRUDP_OK);
	CHECK(change.NewlyAckedBytes == 128 && change.ReleasedBytes == 0);
	CHECK(xrtRudpTransmitResolve(t, 0, 128, false, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK && frame.Size == 128);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 128, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 0, 128, false, true, &change) == XRUDP_OK);
	CHECK(change.NewlyAckedBytes == 128 && change.ReleasedBytes == 256);
	CHECK(xrtRudpTransmitResolve(t, 0, 128, false, false, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitResolve(t, 128, 128, false, true, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitWrite(t, source + 256, 200, &written) == XRUDP_OK && written == 200);
	CHECK(xrtRudpTransmitFinish(t) == XRUDP_OK);
	CHECK(xrtRudpTransmitFinish(t) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitWrite(t, source, 1, &written) == XRUDP_CLOSED && written == 0);
	CHECK(xrtRudpTransmitSetLimit(t, 456) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
	CHECK(frame.Offset == 256 && frame.Size == 200 && frame.Fin && frame.HighestDelta == 200);
	CHECK(memcmp(output, source + 256, 200) == 0);
	CHECK(xrtRudpTransmitResolve(t, 456, 0, true, true, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpTransmitMarkSent(t, frame.Offset, frame.Size, true, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, true, output, sizeof(output), &frame) == XRUDP_OK && frame.Offset == 256 && frame.Size == 200 && frame.Fin);
	CHECK(xrtRudpTransmitMarkSent(t, 456, 0, true, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 456, 0, true, true, &change) == XRUDP_OK && change.NewFinAcknowledged);
	CHECK(xrtRudpTransmitState(t, &state) == XRUDP_OK && state.FinAcknowledged && state.PendingBytes == 200);
	CHECK(xrtRudpTransmitResolve(t, 256, 200, true, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK && !frame.Fin);
	CHECK(xrtRudpTransmitResolve(t, 256, 200, true, true, &change) == XRUDP_OK && !change.NewFinAcknowledged);
	CHECK(change.ReleasedBytes == 200 && change.NewlyAckedBytes == 200);
	CHECK(xrtRudpTransmitResolve(t, 256, 200, true, false, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitState(t, &state) == XRUDP_OK && state.PendingBytes == 0 && state.BaseOffset == 456);
	CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_OK && change.ReleasedBytes == 0);
	CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_CLOSED);
	xrtRudpTransmitDestroy(t);
	CHECK(xrtRudpTransmitCreate(256, 0, &t) == XRUDP_OK);
	CHECK(xrtRudpTransmitFinish(t) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_OK && frame.Size == 0 && frame.Fin);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 0, true, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitResolve(t, 0, 0, true, false, &change) == XRUDP_OK);
	CHECK(xrtRudpTransmitPeek(t, false, output, 512, &frame) == XRUDP_OK && frame.Fin);
	CHECK(xrtRudpTransmitResolve(t, 0, 0, true, true, &change) == XRUDP_OK && change.NewFinAcknowledged);
	CHECK(xrtRudpTransmitPeek(t, true, output, 512, &frame) == XRUDP_BACKPRESSURE);
	CHECK(xrtRudpTransmitMarkSent(t, 0, 0, false, &change) == XRUDP_ARGUMENT);
	xrtRudpTransmitDestroy(t);
	return 0;
}

static int sparse_ready(void)
{
	xrudptransmit* t = NULL;
	xrudptransmitframe frame;
	xrudptransmitchange change;
	unsigned char output[XRUDP_STREAM_CHUNK_MAX];
	size_t written;
	CHECK(xrtRudpTransmitCreate(XRUDP_TRANSMIT_CAPACITY_MAX, UINT64_MAX, &t) == XRUDP_OK);
	CHECK(xrtRudpTransmitWrite(t, source, sizeof(source), &written) == XRUDP_OK && written == sizeof(source));
	for (size_t offset = 0; offset < sizeof(source);) {
		CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
		CHECK(frame.Offset == offset && frame.Size != 0 && memcmp(output, source + offset, frame.Size) == 0);
		CHECK(xrtRudpTransmitMarkSent(t, frame.Offset, frame.Size, false, &change) == XRUDP_OK);
		offset += frame.Size;
	}
	for (size_t i = 0; i < 4000; ++i) {
		uint64_t offset = random_word() % sizeof(source);
		CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_BACKPRESSURE);
		CHECK(xrtRudpTransmitResolve(t, offset, 1, false, false, &change) == XRUDP_OK);
		CHECK(xrtRudpTransmitPeek(t, false, output, sizeof(output), &frame) == XRUDP_OK);
		CHECK(frame.Offset == offset && frame.Size == 1 && output[0] == source[offset]);
		CHECK(xrtRudpTransmitMarkSent(t, offset, 1, false, &change) == XRUDP_OK);
	}
	CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_OK && change.ReleasedBytes == sizeof(source));
	xrtRudpTransmitDestroy(t);
	return 0;
}

static int model(void)
{
	/* Independent flat arrays retain history for every byte, with delayed and
 * duplicate ACK/loss feedback. No ring indexing or readiness tree in oracle. */
	for (size_t round = 0; round < 20; ++round) {
		struct reference { unsigned char Sent, Acked, Ready; } history[8192] = {{0}};
		xrudptransmit* t = NULL;
		xrudptransmitstate state;
		xrudptransmitframe frame;
		xrudptransmitchange change;
		unsigned char output[256];
		size_t base = 0, end = 0, pending = 0, highest = 0, written;
		CHECK(xrtRudpTransmitCreate(256, UINT64_MAX, &t) == XRUDP_OK);
		for (size_t step = 0; step < 5000; ++step) {
			uint32_t word = random_word();
			if (word % 5 == 0 && end < 8192) {
				size_t count = 1 + random_word() % 256, expected = 256 - (end - base);
				if (count > 8192 - end) count = 8192 - end;
				if (expected > count) expected = count;
				CHECK(xrtRudpTransmitWrite(t, source + end, count, &written) == (expected ? XRUDP_OK : XRUDP_BACKPRESSURE));
				CHECK(written == expected);
				for (size_t i = end; i < end + written; ++i) history[i].Ready = 1;
				end += written; pending += written;
			} else if (word % 5 <= 2) {
				bool probe = word % 5 == 2;
				size_t offset = base, count = 0;
				if (!probe) while (offset < end && !history[offset].Ready) ++offset;
				while (offset + count < end && count < 256 && !history[offset + count].Acked &&
					(probe || history[offset + count].Ready)) ++count;
				CHECK(xrtRudpTransmitPeek(t, probe, output, 256, &frame) == (count ? XRUDP_OK : XRUDP_BACKPRESSURE));
				if (count) {
					CHECK(frame.Offset == offset && frame.Size == count && !frame.Fin);
					CHECK(frame.HighestDelta == (offset + count > highest ? offset + count - highest : 0));
					CHECK(memcmp(output, source + offset, count) == 0);
					CHECK(xrtRudpTransmitMarkSent(t, offset, count, false, &change) == XRUDP_OK);
					for (size_t i = offset; i < offset + count; ++i) { history[i].Sent = 1; history[i].Ready = 0; }
					if (offset + count > highest) highest = offset + count;
				}
			} else if (highest) {
				size_t offset = random_word() % highest, count = 1;
				bool ack = (word & 1) != 0, progress = false;
				if (!history[offset].Sent) continue;
				while (count < 128 && offset + count < highest && history[offset + count].Sent) ++count;
				size_t expectedAck = 0, oldBase = base;
				for (size_t i = offset; i < offset + count; ++i) {
					if (history[i].Acked) continue;
					if (ack) { history[i].Acked = 1; history[i].Ready = 0; ++expectedAck; --pending; progress = true; }
					else { if (!history[i].Ready) progress = true; history[i].Ready = 1; }
				}
				while (base < end && history[base].Acked) ++base;
				CHECK(xrtRudpTransmitResolve(t, offset, count, false, ack, &change) == (progress ? XRUDP_OK : XRUDP_DUPLICATE));
				CHECK(change.NewlyAckedBytes == expectedAck && change.ReleasedBytes == base - oldBase);
			}
			CHECK(xrtRudpTransmitState(t, &state) == XRUDP_OK);
			CHECK(state.BaseOffset == base && state.QueuedEnd == end && state.HighestSent == highest && state.PendingBytes == pending);
		}
		CHECK(xrtRudpTransmitCancel(t, &change) == XRUDP_OK && change.ReleasedBytes == end - base);
		xrtRudpTransmitDestroy(t);
	}
	return 0;
}

int main(void)
{
	for (size_t i = 0; i < sizeof(source); ++i) source[i] = (unsigned char)((i * 37) ^ (i >> 7));
	CHECK(boundaries() == 0);
	CHECK(sparse_ready() == 0);
	CHECK(model() == 0);
	puts("xrudp transmit: selective ACK/retry/FIN, sparse bounded readiness and 100,000-step byte-history model passed");
	return 0;
}
