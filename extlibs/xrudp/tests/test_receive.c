#include <xrudp.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static unsigned char source[8192];

static int boundaries(void)
{
	xrudpreceive* r = NULL;
	xrudpreceivestate state, before;
	xrudpreceivechange change, saved;
	unsigned char output[512], bad[40];
	size_t count = 9, allocation = 987;
	CHECK(xrtRudpReceiveAllocationSize(255, &allocation) == XRUDP_ARGUMENT && allocation == 987);
	CHECK(xrtRudpReceiveAllocationSize(0, &allocation) == XRUDP_OK && allocation > 65536 + 8192);
	CHECK(xrtRudpReceiveCreate(257, 0, &r) == XRUDP_ARGUMENT && r == NULL);
	CHECK(xrtRudpReceiveCreate(XRUDP_RECEIVE_CAPACITY_MAX * 2, 0, &r) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceiveCreate(256, 257, &r) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceiveCreate(256, 0, &r) == XRUDP_OK);
	memset(&change, 0xa5, sizeof(change)); saved = change;
	CHECK(xrtRudpReceivePush(r, 0, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(memcmp(&change, &saved, sizeof(change)) == 0);
	CHECK(xrtRudpReceiveExtend(r, 256) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 1, source + 1, 255, false, &change) == XRUDP_OK);
	CHECK(change.NewBytes == 255 && change.HighestDelta == 256);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0);
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	CHECK(xrtRudpReceiveCheck(r, 0, source, 1, false, &change) == XRUDP_OK && change.NewBytes == 1);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 1, false, &change) == XRUDP_OK && change.HighestDelta == 0);
	CHECK(xrtRudpReceiveRead(r, output, 17, &count) == XRUDP_OK && count == 17 && memcmp(output, source, 17) == 0);
	CHECK(xrtRudpReceivePush(r, 256, source + 256, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveExtend(r, 274) == XRUDP_ARGUMENT); /* Physical window is only 273. */
	CHECK(xrtRudpReceiveExtend(r, 273) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 256, source + 256, 17, false, &change) == XRUDP_OK && change.NewBytes == 17);
	CHECK(xrtRudpReceivePush(r, 0, source, 17, false, &change) == XRUDP_DUPLICATE);
	CHECK(change.NewBytes == 0 && change.HighestDelta == 0);
	memcpy(bad, source + 10, sizeof(bad)); bad[20] ^= 1;
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	memset(&change, 0xa5, sizeof(change)); saved = change;
	CHECK(xrtRudpReceivePush(r, 10, bad, sizeof(bad), false, &change) == XRUDP_MALFORMED);
	CHECK(memcmp(&change, &saved, sizeof(change)) == 0);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceivePush(r, UINT64_MAX, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 0, NULL, 1, false, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceivePush(r, 0, source, XRUDP_STREAM_CHUNK_MAX + 1, false, &change) == XRUDP_ARGUMENT);
	CHECK(xrtRudpReceivePush(r, 272, NULL, 0, true, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, NULL, 0, true, &change) == XRUDP_OK && change.NewFinal == 1);
	CHECK(xrtRudpReceivePush(r, 272, NULL, 0, true, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceivePush(r, 273, NULL, 0, true, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_OK && count == 256);
	CHECK(memcmp(output, source + 17, count) == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE && count == 0);
	CHECK(xrtRudpReceiveReset(r, 273, 9, &change) == XRUDP_DUPLICATE && change.ConsumedDelta == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 10, source + 10, 10, false, &change) == XRUDP_OK);
	memcpy(bad, source, 20); bad[15] ^= 1;
	CHECK(xrtRudpReceiveState(r, &before) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 0, bad, 20, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && memcmp(&state, &before, sizeof(state)) == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_BACKPRESSURE && count == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 20, false, &change) == XRUDP_OK && change.NewBytes == 10);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_OK && count == 20);
	CHECK(memcmp(output, source, count) == 0);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 100, source + 100, 10, false, &change) == XRUDP_OK);
	CHECK(xrtRudpReceiveReset(r, 109, 7, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 7, &change) == XRUDP_OK);
	CHECK(change.HighestDelta == 90 && change.ConsumedDelta == 200 && change.NewFinal == 1);
	CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK && state.Reset && state.ResetReason == 7);
	CHECK(state.ReadOffset == 200 && state.HighestOffset == 200 && state.BufferedBytes == 0);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_CANCELLED && count == 0);
	CHECK(xrtRudpReceivePush(r, 0, source, 200, false, &change) == XRUDP_DUPLICATE);
	CHECK(xrtRudpReceivePush(r, 200, source, 1, false, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 8, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 199, 7, &change) == XRUDP_MALFORMED);
	CHECK(xrtRudpReceiveReset(r, 200, 7, &change) == XRUDP_DUPLICATE && change.ConsumedDelta == 0);
	CHECK(xrtRudpReceiveExtend(r, 300) == XRUDP_CLOSED);
	xrtRudpReceiveDestroy(r);
	CHECK(xrtRudpReceiveCreate(256, 0, &r) == XRUDP_OK);
	CHECK(xrtRudpReceivePush(r, 0, NULL, 0, true, &change) == XRUDP_OK);
	CHECK(xrtRudpReceiveRead(r, output, sizeof(output), &count) == XRUDP_DONE);
	xrtRudpReceiveDestroy(r);
	return 0;
}

static uint32_t seed = 0x4f8e9a31;
static uint32_t random_word(void)
{
	seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed;
}

static int model(void)
{
	/* Full-history reference bitmap uses no modulo/ring indexing. */
	for (size_t round = 0; round < 25; ++round) {
		xrudpreceive* r = NULL;
		xrudpreceivestate state;
		xrudpreceivechange change;
		unsigned char seen[8192] = {0}, output[256];
		size_t consumed = 0, highest = 0, buffered = 0, limit = 256, count;
		bool final = false;
		CHECK(xrtRudpReceiveCreate(256, 256, &r) == XRUDP_OK);
		for (size_t step = 0; step < 4000; ++step) {
			uint32_t word = random_word();
			if (word % 3 != 0) {
				size_t offset = word % (limit + 1), size = random_word() % 81;
				size_t newBytes = 0, end;
				if (size > limit - offset) size = limit - offset;
				end = offset + size;
				bool fin = end == sizeof(source);
				for (size_t i = offset; i < end; ++i) if (!seen[i]) ++newBytes;
				xrudpresult expected = newBytes || end > highest || (fin && !final) ? XRUDP_OK : XRUDP_DUPLICATE;
				CHECK(xrtRudpReceiveCheck(r, offset, source + offset, size, fin, &change) == expected);
				CHECK(change.HighestDelta == (end > highest ? end - highest : 0) && change.NewBytes == newBytes);
				CHECK(xrtRudpReceivePush(r, offset, source + offset, size, fin, &change) == expected);
				for (size_t i = offset; i < end; ++i) seen[i] = 1;
				if (end > highest) highest = end;
				buffered += newBytes; final = final || fin;
			} else {
				size_t capacity = 1 + random_word() % 256, wanted = 0;
				while (wanted < capacity && consumed + wanted < sizeof(source) && seen[consumed + wanted]) ++wanted;
				xrudpresult expected = wanted ? XRUDP_OK : (final && consumed == sizeof(source) ? XRUDP_DONE : XRUDP_BACKPRESSURE);
				CHECK(xrtRudpReceiveRead(r, output, capacity, &count) == expected && count == wanted);
				CHECK(memcmp(output, source + consumed, count) == 0);
				consumed += count; buffered -= count;
				limit = consumed + 256 < sizeof(source) ? consumed + 256 : sizeof(source);
				xrudpresult extended = xrtRudpReceiveExtend(r, limit);
				CHECK(extended == XRUDP_OK || extended == XRUDP_DUPLICATE);
			}
			CHECK(xrtRudpReceiveState(r, &state) == XRUDP_OK);
			CHECK(state.ReadOffset == consumed && state.HighestOffset == highest && state.Limit == limit);
			CHECK(state.BufferedBytes == buffered && state.HasFinal == (final ? 1u : 0u));
		}
		/* Force remaining holes to arrive: exact delivery and ring reuse across 8KiB. */
		while (consumed < sizeof(source)) {
			size_t amount = limit - consumed;
			CHECK(amount != 0 && amount <= 256);
			xrudpresult result = xrtRudpReceivePush(r, consumed, source + consumed, amount,
				consumed + amount == sizeof(source), &change);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE);
			CHECK(xrtRudpReceiveRead(r, output, 256, &count) == XRUDP_OK && count == amount);
			CHECK(memcmp(output, source + consumed, count) == 0); consumed += count;
			limit = consumed + 256 < sizeof(source) ? consumed + 256 : sizeof(source);
			result = xrtRudpReceiveExtend(r, limit);
			CHECK(result == XRUDP_OK || result == XRUDP_DUPLICATE);
		}
		CHECK(xrtRudpReceiveRead(r, output, 256, &count) == XRUDP_DONE);
		xrtRudpReceiveDestroy(r);
	}
	return 0;
}

int main(void)
{
	for (size_t i = 0; i < sizeof(source); ++i) source[i] = (unsigned char)((i * 37) ^ (i >> 5));
	CHECK(boundaries() == 0);
	CHECK(model() == 0);
	puts("xrudp receive: bounded ring/bitmap, flow/final/overlap errors and 100,000-step reassembly model passed");
	return 0;
}
